// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visibility/DarkwellVisionIntegrationFixture.h"

#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "SightWeavePresentation.h"
#include "UObject/ConstructorHelpers.h"

namespace Darkwell::VisionIntegrationFixture
{
	constexpr double FloorHalfX = 1500.0;
	constexpr double FloorHalfY = 1000.0;
	constexpr double WallX = -20.0;
	constexpr double WallSouthMinY = -650.0;
	constexpr double WallSouthMaxY = -150.0;
	constexpr double WallNorthMinY = 150.0;
	constexpr double WallNorthMaxY = 650.0;

	TArray<FVector2D> Rectangle(
		const double MinX,
		const double MinY,
		const double MaxX,
		const double MaxY)
	{
		return {
			FVector2D(MinX, MinY),
			FVector2D(MaxX, MinY),
			FVector2D(MaxX, MaxY),
			FVector2D(MinX, MaxY)
		};
	}

	void ConfigureRememberedSceneSurface(
		UStaticMeshComponent& Component,
		const int32 StencilValue)
	{
		Component.SetRenderCustomDepth(true);
		Component.SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
		Component.SetCustomDepthStencilValue(StencilValue);
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
namespace
{
	TAutoConsoleVariable<int32> CVarDiagnosticSurfaceCoverageView(
		TEXT("r.Darkwell.SightWeave.Diagnostic.SurfaceCoverageView"),
		0,
		TEXT("Show material-facing surface authority from the runtime state texture."));
	TAutoConsoleVariable<int32> CVarDarkwellFogDiagnosticRawCoverageView(
		TEXT("r.Darkwell.FogVisual.Diagnostic.RawCoverageView"),
		0,
		TEXT("Development-only view of the project-owned raw LiveCoverage field."));
}
#endif

FVector2D FDarkwellSightWeaveSurfaceMath::ResolveSurfaceSampleWorldPosition(
	const FVector2D WorldPosition,
	const FVector GeometricNormalWS,
	const float SampleDistanceCentimeters)
{
	if (FMath::Abs(GeometricNormalWS.Z)
		>= Darkwell::SightWeaveSurface::VerticalSurfaceNormalZThreshold)
	{
		return WorldPosition;
	}
	const FVector2D OutwardXY(GeometricNormalWS.X, GeometricNormalWS.Y);
	return WorldPosition + OutwardXY.GetSafeNormal()
		* FMath::Max(0.0f, SampleDistanceCentimeters);
}

FDarkwellSightWeaveSurfaceWeights FDarkwellSightWeaveSurfaceMath::ResolveWeights(
	const float KnownCoverage,
	const float LiveCoverage,
	const float ScopeValidity,
	const float SurfaceCategory)
{
	FDarkwellSightWeaveSurfaceWeights Result;
	const float Valid = FMath::Clamp(ScopeValidity, 0.0f, 1.0f);
	const float FilteredLive = FMath::Clamp(LiveCoverage, 0.0f, 1.0f);
	Result.Live = FilteredLive * Valid;
	const float Known = FMath::Max(
		FMath::Clamp(KnownCoverage, 0.0f, 1.0f),
		FilteredLive);
	const float MemoryEligible = SurfaceCategory < 2.5f ? 1.0f : 0.0f;
	Result.Remembered = FMath::Clamp(Known - FilteredLive, 0.0f, 1.0f)
		* MemoryEligible * Valid;
	Result.Unknown = FMath::Clamp(1.0f - Result.Live - Result.Remembered, 0.0f, 1.0f);
	return Result;
}

ADarkwellVisionIntegrationFixture::ADarkwellVisionIntegrationFixture()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	SceneRoot->SetMobility(EComponentMobility::Static);

	Ground = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ground"));
	Ground->SetupAttachment(SceneRoot);
	Ground->SetRelativeLocation(FVector(0.0f, 0.0f, -50.0f));
	Ground->SetRelativeScale3D(FVector(30.0f, 20.0f, 1.0f));
	Ground->SetMobility(EComponentMobility::Static);
	Ground->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Darkwell::VisionIntegrationFixture::ConfigureRememberedSceneSurface(
		*Ground,
		SightWeave::RememberedScene::StaticEnvironmentStencilValue);
	Ground->SetCustomPrimitiveDataFloat(
		Darkwell::SightWeaveSurface::SurfaceCategoryCustomPrimitiveDataIndex,
		Darkwell::SightWeaveSurface::GroundCategory);

	WallSouth = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallSouth"));
	WallSouth->SetupAttachment(SceneRoot);
	WallSouth->SetRelativeLocation(FVector(0.0f, -400.0f, 100.0f));
	WallSouth->SetRelativeScale3D(FVector(0.4f, 5.0f, 2.0f));
	WallSouth->SetMobility(EComponentMobility::Static);
	WallSouth->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Darkwell::VisionIntegrationFixture::ConfigureRememberedSceneSurface(
		*WallSouth,
		SightWeave::RememberedScene::OccluderSurfaceStencilValue);
	WallSouth->SetCustomPrimitiveDataFloat(
		Darkwell::SightWeaveSurface::SurfaceCategoryCustomPrimitiveDataIndex,
		Darkwell::SightWeaveSurface::WallOrCubeSideCategory);
	WallSouth->SetCustomPrimitiveDataFloat(
		Darkwell::SightWeaveSurface::WallSampleDistanceCustomPrimitiveDataIndex,
		Darkwell::SightWeaveSurface::FixtureWallSampleDistanceCentimeters);

	WallNorth = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallNorth"));
	WallNorth->SetupAttachment(SceneRoot);
	WallNorth->SetRelativeLocation(FVector(0.0f, 400.0f, 100.0f));
	WallNorth->SetRelativeScale3D(FVector(0.4f, 5.0f, 2.0f));
	WallNorth->SetMobility(EComponentMobility::Static);
	WallNorth->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Darkwell::VisionIntegrationFixture::ConfigureRememberedSceneSurface(
		*WallNorth,
		SightWeave::RememberedScene::OccluderSurfaceStencilValue);
	WallNorth->SetCustomPrimitiveDataFloat(
		Darkwell::SightWeaveSurface::SurfaceCategoryCustomPrimitiveDataIndex,
		Darkwell::SightWeaveSurface::WallOrCubeSideCategory);
	WallNorth->SetCustomPrimitiveDataFloat(
		Darkwell::SightWeaveSurface::WallSampleDistanceCustomPrimitiveDataIndex,
		Darkwell::SightWeaveSurface::FixtureWallSampleDistanceCentimeters);

	MemoryLandmark = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MemoryLandmark"));
	MemoryLandmark->SetupAttachment(SceneRoot);
	MemoryLandmark->SetRelativeLocation(FVector(650.0f, 500.0f, 75.0f));
	MemoryLandmark->SetRelativeScale3D(FVector(1.5f));
	MemoryLandmark->SetMobility(EComponentMobility::Static);
	MemoryLandmark->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Darkwell::VisionIntegrationFixture::ConfigureRememberedSceneSurface(
		*MemoryLandmark,
		SightWeave::RememberedScene::StaticEnvironmentStencilValue);
	MemoryLandmark->SetCustomPrimitiveDataFloat(
		Darkwell::SightWeaveSurface::SurfaceCategoryCustomPrimitiveDataIndex,
		Darkwell::SightWeaveSurface::RememberableStaticCategory);
	MemoryLandmark->SetCustomPrimitiveDataFloat(
		Darkwell::SightWeaveSurface::WallSampleDistanceCustomPrimitiveDataIndex,
		Darkwell::SightWeaveSurface::WallConservativeSampleBiasCentimeters);

	GreyboxKeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("GreyboxKeyLight"));
	GreyboxKeyLight->SetupAttachment(SceneRoot);
	GreyboxKeyLight->SetRelativeRotation(FRotator(-55.0f, -35.0f, 0.0f));
	GreyboxKeyLight->SetIntensity(4.0f);
	GreyboxKeyLight->SetLightColor(FLinearColor(0.78f, 0.84f, 1.0f));
	GreyboxKeyLight->SetMobility(EComponentMobility::Stationary);

	ProofCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ProofCamera"));
	ProofCamera->SetupAttachment(SceneRoot);
	ProofCamera->SetRelativeLocation(FVector(-1600.0f, -1600.0f, 1800.0f));
	ProofCamera->SetRelativeRotation(FRotator(-38.5f, 45.0f, 0.0f));
	ProofCamera->SetFieldOfView(70.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Ground->SetStaticMesh(CubeMesh.Object);
		WallSouth->SetStaticMesh(CubeMesh.Object);
		WallNorth->SetStaticMesh(CubeMesh.Object);
		MemoryLandmark->SetStaticMesh(CubeMesh.Object);
	}
}

bool ADarkwellVisionIntegrationFixture::EnableSightWeaveSurfaceMaterial(
	UTexture* StateTexture,
	const FVector2D WorldMin,
	const FVector2D InvWorldExtent,
	const bool bDiagnosticFogOff)
{
	if (!StateTexture || !FMath::IsFinite(WorldMin.X) || !FMath::IsFinite(WorldMin.Y)
		|| !FMath::IsFinite(InvWorldExtent.X) || !FMath::IsFinite(InvWorldExtent.Y)
		|| InvWorldExtent.X <= 0.0 || InvWorldExtent.Y <= 0.0)
	{
		return false;
	}
	UMaterialInterface* SurfaceParent = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Darkwell/Vision/Materials/M_DarkwellSightWeaveSurface.M_DarkwellSightWeaveSurface"));
	if (!SurfaceParent)
	{
		return false;
	}
	TArray<FMaterialParameterInfo> TextureParameterInfos;
	TArray<FGuid> TextureParameterGuids;
	SurfaceParent->GetAllTextureParameterInfo(TextureParameterInfos, TextureParameterGuids);
	const FMaterialParameterInfo* StateTextureParameter = TextureParameterInfos.FindByPredicate(
		[](const FMaterialParameterInfo& Info)
		{
			return Info.Name == TEXT("SightWeaveStateTexture");
		});
	if (!StateTextureParameter)
	{
		UE_LOG(LogTemp, Error,
			TEXT("SightWeave surface master has no compiled SightWeaveStateTexture parameter (textureParameters=%d)"),
			TextureParameterInfos.Num());
		return false;
	}

	if (!IsSightWeaveSurfaceMaterialEnabled())
	{
		OriginalGroundMaterial = Ground->GetMaterial(0);
		OriginalWallSouthMaterial = WallSouth->GetMaterial(0);
		OriginalWallNorthMaterial = WallNorth->GetMaterial(0);
		OriginalStaticMaterial = MemoryLandmark->GetMaterial(0);
		bSurfaceMaterialOverrideCaptured = true;
		SurfaceGroundMaterial = UMaterialInstanceDynamic::Create(SurfaceParent, this);
		SurfaceWallMaterial = UMaterialInstanceDynamic::Create(SurfaceParent, this);
		SurfaceStaticMaterial = UMaterialInstanceDynamic::Create(SurfaceParent, this);
	}
	if (!SurfaceGroundMaterial || !SurfaceWallMaterial || !SurfaceStaticMaterial)
	{
		DisableSightWeaveSurfaceMaterial();
		return false;
	}
	// Attach the MIDs before updating dynamic parameters so every setter
	// invalidates an active primitive's uniform-expression cache.
	Ground->SetMaterial(0, SurfaceGroundMaterial);
	WallSouth->SetMaterial(0, SurfaceWallMaterial);
	WallNorth->SetMaterial(0, SurfaceWallMaterial);
	MemoryLandmark->SetMaterial(0, SurfaceStaticMaterial);
	SurfaceGroundMaterial->SetScalarParameterValue(TEXT("OriginalUVScale"), 18.0f);
	SurfaceGroundMaterial->SetVectorParameterValue(
		TEXT("OriginalBaseColorTint"), FLinearColor(0.62f, 0.72f, 0.78f, 1.0f));
	SurfaceWallMaterial->SetScalarParameterValue(TEXT("OriginalUVScale"), 6.0f);
	SurfaceWallMaterial->SetVectorParameterValue(
		TEXT("OriginalBaseColorTint"), FLinearColor(0.55f, 0.58f, 0.62f, 1.0f));
	SurfaceStaticMaterial->SetScalarParameterValue(TEXT("OriginalUVScale"), 3.0f);
	SurfaceStaticMaterial->SetVectorParameterValue(
		TEXT("OriginalBaseColorTint"), FLinearColor(0.78f, 0.46f, 0.24f, 1.0f));

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const int32 CoverageDiagnosticMode =
		CVarDiagnosticSurfaceCoverageView.GetValueOnGameThread();
#else
	constexpr int32 CoverageDiagnosticMode = 0;
#endif
	const FLinearColor WorldMinParameter(
		static_cast<float>(WorldMin.X), static_cast<float>(WorldMin.Y), 0.0f, 0.0f);
	const FLinearColor WorldInvExtentParameter(
		static_cast<float>(InvWorldExtent.X), static_cast<float>(InvWorldExtent.Y), 0.0f, 0.0f);
	for (UMaterialInstanceDynamic* Material : {
		SurfaceGroundMaterial.Get(), SurfaceWallMaterial.Get(), SurfaceStaticMaterial.Get() })
	{
		Material->SetTextureParameterValueByInfo(*StateTextureParameter, StateTexture);
		if (Material->K2_GetTextureParameterValueByInfo(*StateTextureParameter) != StateTexture)
		{
			UE_LOG(LogTemp, Error,
				TEXT("SightWeave surface material rejected its state-texture binding: %s"),
				*GetNameSafe(Material));
			DisableSightWeaveSurfaceMaterial();
			return false;
		}
		Material->SetVectorParameterValue(TEXT("SightWeaveWorldMin"), WorldMinParameter);
		Material->SetVectorParameterValue(TEXT("SightWeaveWorldInvExtent"), WorldInvExtentParameter);
		Material->SetScalarParameterValue(
			TEXT("SightWeaveDiagnosticFogOff"),
			bDiagnosticFogOff ? 1.0f : 0.0f);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		Material->SetScalarParameterValue(
			TEXT("SightWeaveDiagnosticCoverageView"),
			CoverageDiagnosticMode != 0 ? 1.0f : 0.0f);
		if (CoverageDiagnosticMode != 0)
		{
			UE_LOG(LogTemp, Display,
				TEXT("SightWeaveCoverageBinding material=%s texture=%s coverage=%.3f worldMin=(%.6f,%.6f) invExtent=(%.9f,%.9f)"),
				*GetNameSafe(Material),
				*GetNameSafe(Material->K2_GetTextureParameterValue(TEXT("SightWeaveStateTexture"))),
				Material->K2_GetScalarParameterValue(TEXT("SightWeaveDiagnosticCoverageView")),
				WorldMinParameter.R, WorldMinParameter.G,
				WorldInvExtentParameter.R, WorldInvExtentParameter.G);
		}
#endif
	}
	return true;
}

void ADarkwellVisionIntegrationFixture::DisableSightWeaveSurfaceMaterial()
{
	if (bSurfaceMaterialOverrideCaptured && Ground)
	{
		Ground->SetMaterial(0, OriginalGroundMaterial);
	}
	if (bSurfaceMaterialOverrideCaptured && WallSouth)
	{
		WallSouth->SetMaterial(0, OriginalWallSouthMaterial);
	}
	if (bSurfaceMaterialOverrideCaptured && WallNorth)
	{
		WallNorth->SetMaterial(0, OriginalWallNorthMaterial);
	}
	if (bSurfaceMaterialOverrideCaptured && MemoryLandmark)
	{
		MemoryLandmark->SetMaterial(0, OriginalStaticMaterial);
	}
	SurfaceGroundMaterial = nullptr;
	SurfaceWallMaterial = nullptr;
	SurfaceStaticMaterial = nullptr;
	OriginalGroundMaterial = nullptr;
	OriginalWallSouthMaterial = nullptr;
	OriginalWallNorthMaterial = nullptr;
	OriginalStaticMaterial = nullptr;
	bSurfaceMaterialOverrideCaptured = false;
}

bool ADarkwellVisionIntegrationFixture::EnableDarkwellProjectFogP1(
	UTexture* LiveCoverageTexture,
	const FVector2D WorldMin,
	const FVector2D InvWorldExtent)
{
	if (!LiveCoverageTexture
		|| !FMath::IsFinite(WorldMin.X)
		|| !FMath::IsFinite(WorldMin.Y)
		|| !FMath::IsFinite(InvWorldExtent.X)
		|| !FMath::IsFinite(InvWorldExtent.Y)
		|| InvWorldExtent.X <= 0.0
		|| InvWorldExtent.Y <= 0.0)
	{
		return false;
	}
	UMaterialInterface* SurfaceParent = LoadObject<UMaterialInterface>(
		nullptr,
		TEXT("/Game/Darkwell/Vision/ProjectFog/M_DarkwellFogSurface.M_DarkwellFogSurface"));
	if (!SurfaceParent)
	{
		UE_LOG(LogTemp, Error, TEXT("DARKWELL project fog surface material is missing"));
		return false;
	}
	if (!bProjectFogOverrideCaptured)
	{
		ProjectFogOriginalGroundMaterial = Ground->GetMaterial(0);
		bProjectFogOriginalWallSouthHidden = WallSouth->bHiddenInGame;
		bProjectFogOriginalWallNorthHidden = WallNorth->bHiddenInGame;
		bProjectFogOriginalLandmarkHidden = MemoryLandmark->bHiddenInGame;
		ProjectFogOriginalWallSouthCollision = WallSouth->GetCollisionEnabled();
		ProjectFogOriginalWallNorthCollision = WallNorth->GetCollisionEnabled();
		ProjectFogOriginalLandmarkCollision = MemoryLandmark->GetCollisionEnabled();
		bProjectFogOverrideCaptured = true;
	}
	ProjectFogGroundMaterial = UMaterialInstanceDynamic::Create(SurfaceParent, this);
	if (!ProjectFogGroundMaterial)
	{
		DisableDarkwellProjectFog();
		return false;
	}
	Ground->SetMaterial(0, ProjectFogGroundMaterial);
	ProjectFogGroundMaterial->SetTextureParameterValue(
		TEXT("DarkwellLiveCoverageTexture"),
		LiveCoverageTexture);
	ProjectFogGroundMaterial->SetVectorParameterValue(
		TEXT("FogWorldMin"),
		FLinearColor(
			static_cast<float>(WorldMin.X),
			static_cast<float>(WorldMin.Y), 0.0f, 0.0f));
	ProjectFogGroundMaterial->SetVectorParameterValue(
		TEXT("FogWorldInvExtent"),
		FLinearColor(
			static_cast<float>(InvWorldExtent.X),
			static_cast<float>(InvWorldExtent.Y), 0.0f, 0.0f));
	ProjectFogGroundMaterial->SetScalarParameterValue(TEXT("OriginalUVScale"), 18.0f);
	ProjectFogGroundMaterial->SetVectorParameterValue(
		TEXT("OriginalBaseColorTint"),
		FLinearColor(0.62f, 0.72f, 0.78f, 1.0f));
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	ProjectFogGroundMaterial->SetScalarParameterValue(
		TEXT("DiagnosticRawCoverageView"),
		CVarDarkwellFogDiagnosticRawCoverageView.GetValueOnGameThread() != 0
			? 1.0f
			: 0.0f);
#endif
	if (ProjectFogGroundMaterial->K2_GetTextureParameterValue(
		TEXT("DarkwellLiveCoverageTexture")) != LiveCoverageTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("DARKWELL project fog rejected coverage texture binding"));
		DisableDarkwellProjectFog();
		return false;
	}

	// P1 is a strict no-wall proof. These primitives and their collision are restored
	// when the project presentation deactivates; P2 owns their explicit return.
	WallSouth->SetHiddenInGame(true);
	WallNorth->SetHiddenInGame(true);
	MemoryLandmark->SetHiddenInGame(true);
	WallSouth->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WallNorth->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MemoryLandmark->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	return true;
}

void ADarkwellVisionIntegrationFixture::DisableDarkwellProjectFog()
{
	if (bProjectFogOverrideCaptured)
	{
		Ground->SetMaterial(0, ProjectFogOriginalGroundMaterial);
		WallSouth->SetHiddenInGame(bProjectFogOriginalWallSouthHidden);
		WallNorth->SetHiddenInGame(bProjectFogOriginalWallNorthHidden);
		MemoryLandmark->SetHiddenInGame(bProjectFogOriginalLandmarkHidden);
		WallSouth->SetCollisionEnabled(ProjectFogOriginalWallSouthCollision);
		WallNorth->SetCollisionEnabled(ProjectFogOriginalWallNorthCollision);
		MemoryLandmark->SetCollisionEnabled(ProjectFogOriginalLandmarkCollision);
	}
	ProjectFogGroundMaterial = nullptr;
	ProjectFogOriginalGroundMaterial = nullptr;
	bProjectFogOverrideCaptured = false;
}

FBox2D ADarkwellVisionIntegrationFixture::GetSightWeaveFloorBounds() const
{
	using namespace Darkwell::VisionIntegrationFixture;
	const FVector Origin = GetActorLocation();
	return FBox2D(
		FVector2D(Origin.X - FloorHalfX - 250.0, Origin.Y - FloorHalfY - 250.0),
		FVector2D(Origin.X + FloorHalfX + 250.0, Origin.Y + FloorHalfY + 250.0));
}

void ADarkwellVisionIntegrationFixture::BuildSightWeaveOccluderSegments(
	TArray<FDarkwellVisionIntegrationSegment>& OutSegments) const
{
	using namespace Darkwell::VisionIntegrationFixture;
	const FVector Origin = GetActorLocation();
	OutSegments.Reset(2);
	FDarkwellVisionIntegrationSegment& South = OutSegments.AddDefaulted_GetRef();
	South.A = FVector2D(Origin.X + WallX, Origin.Y + WallSouthMinY);
	South.B = FVector2D(Origin.X + WallX, Origin.Y + WallSouthMaxY);
	South.ZMin = Origin.Z - 10.0f;
	South.ZMax = Origin.Z + 250.0f;
	FDarkwellVisionIntegrationSegment& North = OutSegments.AddDefaulted_GetRef();
	North.A = FVector2D(Origin.X + WallX, Origin.Y + WallNorthMinY);
	North.B = FVector2D(Origin.X + WallX, Origin.Y + WallNorthMaxY);
	North.ZMin = South.ZMin;
	North.ZMax = South.ZMax;
}

void ADarkwellVisionIntegrationFixture::BuildSightWeaveStaticSurfaces(
	TArray<FDarkwellVisionIntegrationSurface>& OutSurfaces) const
{
	using namespace Darkwell::VisionIntegrationFixture;
	const FVector Origin = GetActorLocation();
	OutSurfaces.Reset(4);
	FDarkwellVisionIntegrationSurface& Floor = OutSurfaces.AddDefaulted_GetRef();
	Floor.WorldFootprint = Rectangle(
		Origin.X - FloorHalfX,
		Origin.Y - FloorHalfY,
		Origin.X + FloorHalfX,
		Origin.Y + FloorHalfY);
	Floor.NeutralIntensity = 74;
	FDarkwellVisionIntegrationSurface& South = OutSurfaces.AddDefaulted_GetRef();
	South.WorldFootprint = Rectangle(
		Origin.X - 25.0,
		Origin.Y + WallSouthMinY,
		Origin.X + 25.0,
		Origin.Y + WallSouthMaxY);
	South.NeutralIntensity = 150;
	FDarkwellVisionIntegrationSurface& North = OutSurfaces.AddDefaulted_GetRef();
	North.WorldFootprint = Rectangle(
		Origin.X - 25.0,
		Origin.Y + WallNorthMinY,
		Origin.X + 25.0,
		Origin.Y + WallNorthMaxY);
	North.NeutralIntensity = 150;
	FDarkwellVisionIntegrationSurface& Landmark = OutSurfaces.AddDefaulted_GetRef();
	Landmark.WorldFootprint = Rectangle(
		Origin.X + 575.0,
		Origin.Y + 425.0,
		Origin.X + 725.0,
		Origin.Y + 575.0);
	Landmark.NeutralIntensity = 196;
}
