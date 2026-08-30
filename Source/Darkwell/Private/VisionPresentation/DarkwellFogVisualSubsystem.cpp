// Copyright Epic Games, Inc. All Rights Reserved.

#include "VisionPresentation/DarkwellFogVisualSubsystem.h"

#include "Engine/TextureRenderTarget2D.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Visibility/DarkwellVisionIntegrationFixture.h"

DEFINE_LOG_CATEGORY_STATIC(LogDarkwellFogVisual, Log, All);

namespace Darkwell::FogVisual
{
	constexpr float CentimetersPerTexel = 2.5f;
	constexpr float CoverageTransitionWidthCentimeters = 2.5f;
	constexpr TCHAR CoverageMaterialPath[] =
		TEXT("/Game/Darkwell/Vision/ProjectFog/M_DarkwellFogCoverage.M_DarkwellFogCoverage");

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	TAutoConsoleVariable<int32> CVarDiagnosticRawCoverageReadback(
		TEXT("r.Darkwell.FogVisual.Diagnostic.RawCoverageReadback"),
		0,
		TEXT("Development-only one-shot raw R16F coverage statistics after 60 frames."));
#endif

	float SignedLinearCoverage(const float SignedDistance, const float Width)
	{
		return FMath::Clamp(0.5f + SignedDistance / FMath::Max(Width, 0.001f), 0.0f, 1.0f);
	}
}

bool FDarkwellFogVisualSourceSnapshot::IsValid() const
{
	return FMath::IsFinite(BodyCenter.X)
		&& FMath::IsFinite(BodyCenter.Y)
		&& FMath::IsFinite(ConeOrigin.X)
		&& FMath::IsFinite(ConeOrigin.Y)
		&& FMath::IsFinite(ConeForward.X)
		&& FMath::IsFinite(ConeForward.Y)
		&& FMath::IsFinite(BodyRadiusCentimeters)
		&& FMath::IsFinite(ConeRangeCentimeters)
		&& FMath::IsFinite(ConeHalfAngleDegrees)
		&& !ConeForward.IsNearlyZero()
		&& BodyRadiusCentimeters > 0.0f
		&& ConeRangeCentimeters > 0.0f
		&& ConeHalfAngleDegrees > 0.0f
		&& ConeHalfAngleDegrees < 90.0f;
}

bool FDarkwellFogVisualSourceSnapshot::IsEquivalentTo(
	const FDarkwellFogVisualSourceSnapshot& Other) const
{
	return BodyCenter.Equals(Other.BodyCenter, 1.0e-4)
		&& ConeOrigin.Equals(Other.ConeOrigin, 1.0e-4)
		&& ConeForward.Equals(Other.ConeForward, 1.0e-6)
		&& FMath::IsNearlyEqual(BodyRadiusCentimeters, Other.BodyRadiusCentimeters, 1.0e-4f)
		&& FMath::IsNearlyEqual(ConeRangeCentimeters, Other.ConeRangeCentimeters, 1.0e-4f)
		&& FMath::IsNearlyEqual(ConeHalfAngleDegrees, Other.ConeHalfAngleDegrees, 1.0e-4f)
		&& AuthorityRevision == Other.AuthorityRevision
		&& bConeLegallyLive == Other.bConeLegallyLive;
}

bool FDarkwellFogVisualMapping::IsValid() const
{
	return FMath::IsFinite(WorldMin.X)
		&& FMath::IsFinite(WorldMin.Y)
		&& FMath::IsFinite(WorldExtent.X)
		&& FMath::IsFinite(WorldExtent.Y)
		&& FMath::IsFinite(InvWorldExtent.X)
		&& FMath::IsFinite(InvWorldExtent.Y)
		&& FMath::IsFinite(CentimetersPerTexel)
		&& WorldExtent.X > 0.0
		&& WorldExtent.Y > 0.0
		&& InvWorldExtent.X > 0.0
		&& InvWorldExtent.Y > 0.0
		&& TextureExtent.X > 0
		&& TextureExtent.Y > 0
		&& CentimetersPerTexel > 0.0f;
}

FVector2D FDarkwellFogVisualMapping::WorldToUV(const FVector2D& WorldPosition) const
{
	return IsValid()
		? (WorldPosition - WorldMin) * InvWorldExtent
		: FVector2D::ZeroVector;
}

float FDarkwellContinuousVisibilityBuilder::EvaluateNoOcclusionCoverage(
	const FDarkwellFogVisualSourceSnapshot& Source,
	const FVector2D& WorldPosition,
	const float TransitionWidthCentimeters)
{
	if (!Source.IsValid())
	{
		return 0.0f;
	}
	const FVector2D Forward = Source.ConeForward.GetSafeNormal();
	const float BodySignedDistance = Source.BodyRadiusCentimeters
		- FVector2D::Distance(WorldPosition, Source.BodyCenter);
	const float BodyCoverage = Darkwell::FogVisual::SignedLinearCoverage(
		BodySignedDistance, TransitionWidthCentimeters);
	if (!Source.bConeLegallyLive)
	{
		return BodyCoverage;
	}

	const FVector2D Relative = WorldPosition - Source.ConeOrigin;
	const float Along = FVector2D::DotProduct(Relative, Forward);
	const float Cross = FMath::Abs(Relative.X * Forward.Y - Relative.Y * Forward.X);
	const float HalfAngleRadians = FMath::DegreesToRadians(Source.ConeHalfAngleDegrees);
	const float SideSignedDistance = Along * FMath::Sin(HalfAngleRadians)
		- Cross * FMath::Cos(HalfAngleRadians);
	const float RadialSignedDistance = Source.ConeRangeCentimeters - Relative.Size();
	const float ConeCoverage = Darkwell::FogVisual::SignedLinearCoverage(
		FMath::Min(SideSignedDistance, RadialSignedDistance),
		TransitionWidthCentimeters);
	return FMath::Max(BodyCoverage, ConeCoverage);
}

void UDarkwellFogVisualSubsystem::Deinitialize()
{
	Deactivate();
	Super::Deinitialize();
}

bool UDarkwellFogVisualSubsystem::ActivateP1(
	ADarkwellVisionIntegrationFixture* Fixture,
	const FDarkwellFogVisualSourceSnapshot& Source)
{
	if (!Fixture || Fixture->GetWorld() != GetWorld() || !Source.IsValid())
	{
		return false;
	}
	Deactivate();
	if (!CreateResources(Fixture->GetSightWeaveFloorBounds()))
	{
		return false;
	}
	if (!Fixture->EnableDarkwellProjectFogP1(
		LiveCoverageTexture,
		Mapping.WorldMin,
		Mapping.InvWorldExtent))
	{
		Deactivate();
		return false;
	}
	ActiveFixture = Fixture;
	Diagnostics = FDarkwellFogVisualDiagnostics();
	Diagnostics.bActive = true;
	Diagnostics.bOldSightWeavePresentationSuppressed = true;
	Diagnostics.ActivationRevision = Source.AuthorityRevision;
	if (!DrawCoverage(Source))
	{
		Deactivate();
		return false;
	}
	UE_LOG(LogDarkwellFogVisual, Log,
		TEXT("P1 active policy=RememberedFromStart extent=%dx%d cmPerTexel=%.3f oldSightWeaveVisual=Suppressed"),
		Mapping.TextureExtent.X,
		Mapping.TextureExtent.Y,
		Mapping.CentimetersPerTexel);
	return true;
}

bool UDarkwellFogVisualSubsystem::UpdateSource(
	const FDarkwellFogVisualSourceSnapshot& Source)
{
	if (!Diagnostics.bActive || !Source.IsValid())
	{
		return false;
	}
	const bool bNeedsDraw = !Source.IsEquivalentTo(LastSource);
	if (bNeedsDraw && !DrawCoverage(Source))
	{
		return false;
	}
	TryDiagnosticReadback();
	return true;
}

void UDarkwellFogVisualSubsystem::Deactivate()
{
	if (ADarkwellVisionIntegrationFixture* Fixture = ActiveFixture.Get())
	{
		Fixture->DisableDarkwellProjectFog();
	}
	ActiveFixture.Reset();
	LiveCoverageTexture = nullptr;
	CoverageMaterial = nullptr;
	Mapping = FDarkwellFogVisualMapping();
	LastSource = FDarkwellFogVisualSourceSnapshot();
	Diagnostics = FDarkwellFogVisualDiagnostics();
	DiagnosticReadbackFrameCount = 0;
}

bool UDarkwellFogVisualSubsystem::CreateResources(const FBox2D& WorldBounds)
{
	if (!WorldBounds.bIsValid || !GetWorld())
	{
		return false;
	}
	Mapping.WorldMin = WorldBounds.Min;
	Mapping.CentimetersPerTexel = Darkwell::FogVisual::CentimetersPerTexel;
	Mapping.TextureExtent = FIntPoint(
		FMath::CeilToInt(WorldBounds.GetSize().X / Mapping.CentimetersPerTexel),
		FMath::CeilToInt(WorldBounds.GetSize().Y / Mapping.CentimetersPerTexel));
	Mapping.WorldExtent = FVector2D(
		Mapping.TextureExtent.X * Mapping.CentimetersPerTexel,
		Mapping.TextureExtent.Y * Mapping.CentimetersPerTexel);
	Mapping.InvWorldExtent = FVector2D(
		1.0 / Mapping.WorldExtent.X,
		1.0 / Mapping.WorldExtent.Y);
	if (!Mapping.IsValid())
	{
		return false;
	}

	UMaterialInterface* CoverageParent = LoadObject<UMaterialInterface>(
		nullptr,
		Darkwell::FogVisual::CoverageMaterialPath);
	if (!CoverageParent)
	{
		UE_LOG(LogDarkwellFogVisual, Error,
			TEXT("Missing project coverage material %s"),
			Darkwell::FogVisual::CoverageMaterialPath);
		return false;
	}
	CoverageMaterial = UMaterialInstanceDynamic::Create(CoverageParent, this);
	LiveCoverageTexture = NewObject<UTextureRenderTarget2D>(this);
	if (!CoverageMaterial || !LiveCoverageTexture)
	{
		return false;
	}
	LiveCoverageTexture->RenderTargetFormat = ETextureRenderTargetFormat::RTF_R16f;
	LiveCoverageTexture->SRGB = false;
	LiveCoverageTexture->bForceLinearGamma = true;
	LiveCoverageTexture->ClearColor = FLinearColor::Black;
	LiveCoverageTexture->Filter = TextureFilter::TF_Bilinear;
	LiveCoverageTexture->AddressX = TextureAddress::TA_Clamp;
	LiveCoverageTexture->AddressY = TextureAddress::TA_Clamp;
	LiveCoverageTexture->bAutoGenerateMips = true;
	LiveCoverageTexture->MipsSamplerFilter = TextureFilter::TF_Bilinear;
	LiveCoverageTexture->InitAutoFormat(Mapping.TextureExtent.X, Mapping.TextureExtent.Y);
	LiveCoverageTexture->UpdateResourceImmediate(true);
	return true;
}

void UDarkwellFogVisualSubsystem::UpdateMaterialParameters(
	const FDarkwellFogVisualSourceSnapshot& Source)
{
	const FVector2D Forward = Source.ConeForward.GetSafeNormal();
	const float HalfAngleRadians = FMath::DegreesToRadians(Source.ConeHalfAngleDegrees);
	CoverageMaterial->SetVectorParameterValue(TEXT("FogWorldMin"), FLinearColor(
		static_cast<float>(Mapping.WorldMin.X),
		static_cast<float>(Mapping.WorldMin.Y), 0.0f, 0.0f));
	CoverageMaterial->SetVectorParameterValue(TEXT("FogWorldExtent"), FLinearColor(
		static_cast<float>(Mapping.WorldExtent.X),
		static_cast<float>(Mapping.WorldExtent.Y), 0.0f, 0.0f));
	CoverageMaterial->SetVectorParameterValue(TEXT("BodyCenter"), FLinearColor(
		static_cast<float>(Source.BodyCenter.X),
		static_cast<float>(Source.BodyCenter.Y), 0.0f, 0.0f));
	CoverageMaterial->SetScalarParameterValue(TEXT("BodyRadius"), Source.BodyRadiusCentimeters);
	CoverageMaterial->SetVectorParameterValue(TEXT("ConeOrigin"), FLinearColor(
		static_cast<float>(Source.ConeOrigin.X),
		static_cast<float>(Source.ConeOrigin.Y), 0.0f, 0.0f));
	CoverageMaterial->SetVectorParameterValue(TEXT("ConeForward"), FLinearColor(
		static_cast<float>(Forward.X),
		static_cast<float>(Forward.Y), 0.0f, 0.0f));
	CoverageMaterial->SetScalarParameterValue(TEXT("ConeRange"), Source.ConeRangeCentimeters);
	CoverageMaterial->SetScalarParameterValue(TEXT("ConeSinHalfAngle"), FMath::Sin(HalfAngleRadians));
	CoverageMaterial->SetScalarParameterValue(TEXT("ConeCosHalfAngle"), FMath::Cos(HalfAngleRadians));
	CoverageMaterial->SetScalarParameterValue(TEXT("ConeEnabled"), Source.bConeLegallyLive ? 1.0f : 0.0f);
	CoverageMaterial->SetScalarParameterValue(
		TEXT("CoverageWidth"),
		Darkwell::FogVisual::CoverageTransitionWidthCentimeters);
}

bool UDarkwellFogVisualSubsystem::DrawCoverage(
	const FDarkwellFogVisualSourceSnapshot& Source)
{
	if (!CoverageMaterial || !LiveCoverageTexture || !GetWorld())
	{
		return false;
	}
	UpdateMaterialParameters(Source);
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(
		GetWorld(),
		LiveCoverageTexture,
		CoverageMaterial);
	LastSource = Source;
	Diagnostics.LastAuthorityRevision = Source.AuthorityRevision;
	++Diagnostics.CoverageDrawCount;
	return true;
}

void UDarkwellFogVisualSubsystem::TryDiagnosticReadback()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (Darkwell::FogVisual::CVarDiagnosticRawCoverageReadback.GetValueOnGameThread() == 0)
	{
		DiagnosticReadbackFrameCount = 0;
		return;
	}
	if (++DiagnosticReadbackFrameCount != 60 || !LiveCoverageTexture)
	{
		return;
	}
	FTextureRenderTargetResource* Resource =
		LiveCoverageTexture->GameThread_GetRenderTargetResource();
	TArray<FLinearColor> Pixels;
	const bool bSucceeded = Resource
		&& Resource->ReadLinearColorPixels(Pixels, FReadSurfaceDataFlags(RCM_MinMax));
	if (!bSucceeded || Pixels.IsEmpty())
	{
		UE_LOG(LogDarkwellFogVisual, Error, TEXT("RawCoverageReadback failed"));
		return;
	}
	float Minimum = 1.0f;
	float Maximum = 0.0f;
	int64 Intermediate = 0;
	double CoverageSum = 0.0;
	double MomentX = 0.0;
	double MomentY = 0.0;
	for (int32 Index = 0; Index < Pixels.Num(); ++Index)
	{
		const float Coverage = Pixels[Index].R;
		Minimum = FMath::Min(Minimum, Coverage);
		Maximum = FMath::Max(Maximum, Coverage);
		Intermediate += Coverage > 0.001f && Coverage < 0.999f ? 1 : 0;
		const int32 X = Index % Mapping.TextureExtent.X;
		const int32 Y = Index / Mapping.TextureExtent.X;
		CoverageSum += Coverage;
		MomentX += Coverage * (static_cast<double>(X) + 0.5);
		MomentY += Coverage * (static_cast<double>(Y) + 0.5);
	}
	const double CentroidX = CoverageSum > 0.0 ? MomentX / CoverageSum : 0.0;
	const double CentroidY = CoverageSum > 0.0 ? MomentY / CoverageSum : 0.0;
	Diagnostics.LastMinimumCoverage = Minimum;
	Diagnostics.LastMaximumCoverage = Maximum;
	Diagnostics.LastIntermediatePixelCount = Intermediate;
	UE_LOG(LogDarkwellFogVisual, Display,
		TEXT("RawCoverageReadback extent=%dx%d min=%.6f max=%.6f intermediate=%lld coverageSum=%.6f centroidTexel=(%.6f,%.6f) draws=%llu revision=%llu"),
		Mapping.TextureExtent.X,
		Mapping.TextureExtent.Y,
		Minimum,
		Maximum,
		Intermediate,
		CoverageSum,
		CentroidX,
		CentroidY,
		Diagnostics.CoverageDrawCount,
		Diagnostics.LastAuthorityRevision);
#endif
}
