// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visibility/SightWeave/DarkwellSightWeaveWorldSubsystem.h"

#include "AI/DarkwellStalkerCharacter.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/DarkwellLoadoutComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Controller.h"
#include "HAL/IConsoleManager.h"
#include "Gameplay/DarkwellVisibilityComponent.h"
#include "Player/DarkwellCharacter.h"
#include "SightWeaveQueries.h"
#include "SightWeaveWorldSubsystem.h"
#include "UI/DarkwellHUD.h"
#include "Visibility/DarkwellVisionIntegrationFixture.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#if !UE_SERVER
#include "Engine/TextureRenderTarget2D.h"
#include "UnrealClient.h"
#include "SightWeavePresentation.h"
#include "SightWeaveRenderWorldSubsystem.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogDarkwellSightWeave, Log, All);

namespace Darkwell::SightWeaveAdapter
{
	uint64 NextWorldGeneration = 1;
	constexpr double ActivationTimeoutSeconds = 5.0;
	const FName OwnerName(TEXT("Local"));
	const FName FloorName(TEXT("Darkwell.Integration.Ground"));
	const FName TorchCapability(TEXT("Darkwell.Visible.Torch"));
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	TAutoConsoleVariable<int32> CVarDiagnosticLogGameTransforms(
		TEXT("r.SightWeave.Diagnostic.LogGameTransforms"),
		0,
		TEXT("Emit one player/camera transform record per game frame for visual rescue."));
	TAutoConsoleVariable<int32> CVarDiagnosticSurfaceFogOff(
		TEXT("r.Darkwell.SightWeave.Diagnostic.SurfaceFogOff"),
		0,
		TEXT("Development-only native-material control for SurfaceMaterial visual evidence."));
	TAutoConsoleVariable<int32> CVarDiagnosticSurfaceTrajectory(
		TEXT("r.Darkwell.SightWeave.Diagnostic.SurfaceTrajectory"),
		0,
		TEXT("Development-only deterministic motion: 0=off, 1=world +X, 2=world -Y, "
			"3=doorway lane +X, 4=world +Y, 5=rotate yaw."));
	TAutoConsoleVariable<float> CVarDiagnosticSurfaceTrajectorySpeed(
		TEXT("r.Darkwell.SightWeave.Diagnostic.SurfaceTrajectorySpeed"),
		15.0f,
		TEXT("Centimeters per second for the deterministic SurfaceMaterial trajectory."));
	TAutoConsoleVariable<int32> CVarDiagnosticSurfaceProofMode(
		TEXT("r.Darkwell.SightWeave.Diagnostic.SurfaceProofMode"),
		0,
		TEXT("Development-only architecture isolation: 0=Gameplay, 1=AuthorityFrozen, "
			"2=CameraFrozen while visibility authority continues."));
	TAutoConsoleVariable<int32> CVarDiagnosticSurfaceCoverageReadback(
		TEXT("r.Darkwell.SightWeave.Diagnostic.SurfaceCoverageReadback"),
		0,
		TEXT("Development-only one-shot GPU readback of the SurfaceMaterial state texture."));
	TAutoConsoleVariable<int32> CVarDiagnosticFogTrajectory(
		TEXT("r.Darkwell.FogVisual.Diagnostic.Trajectory"),
		0,
		TEXT("P1 deterministic source motion: 0=off, 1=+X, 2=-Y, 3=diagonal +X+Y, "
			"4=+Y, 5=rotate yaw."));
	TAutoConsoleVariable<float> CVarDiagnosticFogTrajectorySpeed(
		TEXT("r.Darkwell.FogVisual.Diagnostic.TrajectorySpeed"),
		15.0f,
		TEXT("Centimeters per second for the P1 continuous-coverage trajectory."));
	TAutoConsoleVariable<float> CVarDiagnosticFogSourceOffsetTexelsX(
		TEXT("r.Darkwell.FogVisual.Diagnostic.SourceOffsetTexelsX"),
		0.0f,
		TEXT("P1 raw-coverage proof offset in 2.5 cm presentation texels."));
	TAutoConsoleVariable<float> CVarDiagnosticFogSourceOffsetTexelsY(
		TEXT("r.Darkwell.FogVisual.Diagnostic.SourceOffsetTexelsY"),
		0.0f,
		TEXT("P1 raw-coverage proof offset in 2.5 cm presentation texels."));
#endif

	FTransform BuildSourceTransform(const ADarkwellCharacter& Character)
	{
		return FTransform(Character.GetActorRotation(),
			Character.GetActorLocation() + FVector(0.0, 0.0, 52.0));
	}
}

void UDarkwellSightWeaveWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Diagnostics = FDarkwellVisibilityAuthorityDiagnostics();
	Diagnostics.WorldGeneration = Darkwell::SightWeaveAdapter::NextWorldGeneration++;
	if (const UWorld* World = GetWorld())
	{
		Diagnostics.WorldName = World->GetFName();
	}
	RuntimeSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<USightWeaveWorldSubsystem>() : nullptr;
	Diagnostics.bRuntimeServiceAvailable = RuntimeSubsystem
		&& RuntimeSubsystem->IsSightWeaveInitialized();
#if !UE_SERVER
	RenderSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<USightWeaveRenderWorldSubsystem>() : nullptr;
	FogVisualSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>() : nullptr;
	Diagnostics.bRenderServiceAvailable = RenderSubsystem != nullptr;
#else
	Diagnostics.bRenderServiceAvailable = false;
#endif
	ResetToLegacy();
	UE_LOG(LogDarkwellSightWeave, Log,
		TEXT("World=%s authority=Legacy generation=%llu runtime=%d render=%d"),
		*Diagnostics.WorldName.ToString(), Diagnostics.WorldGeneration,
		Diagnostics.bRuntimeServiceAvailable ? 1 : 0,
		Diagnostics.bRenderServiceAvailable ? 1 : 0);
}

void UDarkwellSightWeaveWorldSubsystem::Deinitialize()
{
	UE_LOG(LogDarkwellSightWeave, Log,
		TEXT("World=%s authority=%s teardown generation=%llu"),
		*Diagnostics.WorldName.ToString(),
		IsSightWeaveAuthorityActive() ? TEXT("SightWeave") : TEXT("Legacy"),
		Diagnostics.WorldGeneration);
	RollbackToLegacy(TEXT("World teardown"), false);
#if !UE_SERVER
	RenderSubsystem = nullptr;
#endif
	FogVisualSubsystem = nullptr;
	RuntimeSubsystem = nullptr;
	Super::Deinitialize();
}

void UDarkwellSightWeaveWorldSubsystem::Tick(const float DeltaTime)
{
	if (Diagnostics.State == EDarkwellVisibilityAuthorityState::SightWeaveRequested)
	{
		RequestAgeSeconds += FMath::Max(0.0f, DeltaTime);
		if (!TryActivate()
			&& RequestAgeSeconds >= Darkwell::SightWeaveAdapter::ActivationTimeoutSeconds)
		{
			RollbackToLegacy(TEXT("Timed out waiting for the integration actors"), true);
		}
		return;
	}
	if (!IsSightWeaveAuthorityActive())
	{
		return;
	}
	if (!Player.IsValid() || !Stalker.IsValid() || !RequestedFixture.IsValid())
	{
		RollbackToLegacy(TEXT("An authoritative integration actor was destroyed"), true);
		return;
	}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const int32 ProjectTrajectory =
		Darkwell::SightWeaveAdapter::CVarDiagnosticFogTrajectory.GetValueOnGameThread();
	const int32 DiagnosticTrajectory = ProjectTrajectory != 0
		? ProjectTrajectory
		: Darkwell::SightWeaveAdapter::CVarDiagnosticSurfaceTrajectory.GetValueOnGameThread();
	if (DiagnosticTrajectory >= 1 && DiagnosticTrajectory <= 5)
	{
		const float Speed = FMath::Clamp(
			ProjectTrajectory != 0
				? Darkwell::SightWeaveAdapter::CVarDiagnosticFogTrajectorySpeed.GetValueOnGameThread()
				: Darkwell::SightWeaveAdapter::CVarDiagnosticSurfaceTrajectorySpeed.GetValueOnGameThread(),
			0.0f,
			100.0f);
		if (DiagnosticTrajectory == 5)
		{
			FRotator NextRotation = Player->GetActorRotation();
			NextRotation.Yaw += Speed * FMath::Max(DeltaTime, 0.0f);
			Player->SetActorRotation(NextRotation, ETeleportType::TeleportPhysics);
		}
		else
		{
			const FVector Direction = DiagnosticTrajectory == 2
				? FVector(0.0, -1.0, 0.0)
				: DiagnosticTrajectory == 3
					? FVector(1.0, 1.0, 0.0).GetSafeNormal()
				: DiagnosticTrajectory == 4
					? FVector(0.0, 1.0, 0.0)
					: FVector(1.0, 0.0, 0.0);
			FVector NextLocation = Player->GetActorLocation()
				+ Direction * Speed * FMath::Max(DeltaTime, 0.0f);
			if (ProjectTrajectory == 0 && DiagnosticTrajectory == 3)
			{
				NextLocation.Y = 0.0;
			}
			Player->SetActorLocation(
				NextLocation,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
		}
	}
	const int32 SurfaceProofMode = FMath::Clamp(
		Darkwell::SightWeaveAdapter::CVarDiagnosticSurfaceProofMode.GetValueOnGameThread(),
		0,
		2);
	if (APlayerController* Controller = GetWorld()
		? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (SurfaceProofMode == 2 && !bDiagnosticProofCameraActive)
		{
			Controller->SetViewTarget(RequestedFixture.Get());
			bDiagnosticProofCameraActive = true;
		}
		else if (SurfaceProofMode != 2 && bDiagnosticProofCameraActive)
		{
			Controller->SetViewTarget(Player.Get());
			bDiagnosticProofCameraActive = false;
		}
	}
#else
	constexpr int32 SurfaceProofMode = 0;
#endif
	if (SurfaceProofMode != 1)
	{
		UpdateDynamicAuthority();
		UpdateSubjectAuthority();
	}
	if (ADarkwellStalkerCharacter* P1Subject = Stalker.Get())
	{
		P1Subject->SetActorHiddenInGame(true);
	}
	if (FogVisualSubsystem
		&& !FogVisualSubsystem->UpdateSource(BuildFogVisualSourceSnapshot()))
	{
		RollbackToLegacy(TEXT("DARKWELL continuous fog source update failed"), true);
		return;
	}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (Darkwell::SightWeaveAdapter::CVarDiagnosticLogGameTransforms.GetValueOnGameThread() != 0)
	{
		const APlayerController* Controller = GetWorld()
			? GetWorld()->GetFirstPlayerController() : nullptr;
		const APlayerCameraManager* Camera = Controller ? Controller->PlayerCameraManager : nullptr;
		const FVector PlayerLocation = Player->GetActorLocation();
		const FRotator PlayerRotation = Player->GetActorRotation();
		const FVector CameraLocation = Camera ? Camera->GetCameraLocation() : FVector::ZeroVector;
		const FRotator CameraRotation = Camera ? Camera->GetCameraRotation() : FRotator::ZeroRotator;
		UE_LOG(
			LogDarkwellSightWeave,
			Display,
			TEXT("VisualRescueTransform frame=%llu proofMode=%d player=(%.3f,%.3f,%.3f %.3f,%.3f,%.3f) camera=(%.3f,%.3f,%.3f %.3f,%.3f,%.3f)"),
			GFrameCounter,
			SurfaceProofMode,
			PlayerLocation.X, PlayerLocation.Y, PlayerLocation.Z,
			PlayerRotation.Pitch, PlayerRotation.Yaw, PlayerRotation.Roll,
			CameraLocation.X, CameraLocation.Y, CameraLocation.Z,
			CameraRotation.Pitch, CameraRotation.Yaw, CameraRotation.Roll);
	}
#if !UE_SERVER
	if (Darkwell::SightWeaveAdapter::CVarDiagnosticSurfaceCoverageReadback.GetValueOnGameThread() != 0)
	{
		++DiagnosticCoverageReadbackFrameCount;
		if (DiagnosticCoverageReadbackFrameCount == 1)
		{
			UE_LOG(LogDarkwellSightWeave, Display,
				TEXT("SurfaceCoverageReadback armed frame=%llu"), GFrameCounter);
		}
		if (!bDiagnosticCoverageReadbackComplete && DiagnosticCoverageReadbackFrameCount >= 60
			&& RenderSubsystem && RenderSubsystem->GetSurfaceStateTexture())
		{
			UTextureRenderTarget2D* SurfaceStateTexture = RenderSubsystem->GetSurfaceStateTexture();
			FTextureRenderTargetResource* Resource =
				SurfaceStateTexture->GameThread_GetRenderTargetResource();
			TArray<FFloat16Color> Pixels;
			const bool bReadSucceeded = Resource
				&& Resource->ReadFloat16Pixels(Pixels, FReadSurfaceDataFlags(RCM_MinMax));
			const int32 ExpectedPixelCount = SurfaceStateTexture->SizeX * SurfaceStateTexture->SizeY;
			if (bReadSucceeded && Pixels.Num() == ExpectedPixelCount && Pixels.Num() > 0)
			{
				FLinearColor Min(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);
				FLinearColor Max(-FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX);
				FLinearColor Sum = FLinearColor::Transparent;
				int64 ValidCount = 0;
				int64 UnknownCount = 0;
				int64 RememberedCount = 0;
				int64 LiveCount = 0;
				for (const FFloat16Color& Pixel : Pixels)
				{
					const FLinearColor Value = Pixel.GetFloats();
					Min.R = FMath::Min(Min.R, Value.R);
					Min.G = FMath::Min(Min.G, Value.G);
					Min.B = FMath::Min(Min.B, Value.B);
					Min.A = FMath::Min(Min.A, Value.A);
					Max.R = FMath::Max(Max.R, Value.R);
					Max.G = FMath::Max(Max.G, Value.G);
					Max.B = FMath::Max(Max.B, Value.B);
					Max.A = FMath::Max(Max.A, Value.A);
					Sum += Value;
					ValidCount += Value.A > 0.5f ? 1 : 0;
					UnknownCount += Value.R < 0.25f ? 1 : 0;
					RememberedCount += Value.R >= 0.25f && Value.R < 0.75f ? 1 : 0;
					LiveCount += Value.R >= 0.75f ? 1 : 0;
				}
				const FLinearColor Mean = Sum / static_cast<float>(Pixels.Num());
				const auto SampleAt = [&Pixels, SurfaceStateTexture](const int32 X, const int32 Y)
				{
					return Pixels[Y * SurfaceStateTexture->SizeX + X].GetFloats();
				};
				const FLinearColor Center = SampleAt(
					SurfaceStateTexture->SizeX / 2, SurfaceStateTexture->SizeY / 2);
				const FLinearColor Quarter = SampleAt(
					SurfaceStateTexture->SizeX / 4, SurfaceStateTexture->SizeY / 4);
				const FSightWeaveSurfaceTextureMapping& Mapping =
					RenderSubsystem->GetSurfaceTextureMapping();
				const FVector PlayerWorld = Player->GetActorLocation();
				const FVector2D PlayerUV = Mapping.WorldToUV(FVector2D(PlayerWorld.X, PlayerWorld.Y));
				const int32 PlayerX = FMath::Clamp(
					FMath::FloorToInt(PlayerUV.X * SurfaceStateTexture->SizeX),
					0,
					SurfaceStateTexture->SizeX - 1);
				const int32 PlayerY = FMath::Clamp(
					FMath::FloorToInt(PlayerUV.Y * SurfaceStateTexture->SizeY),
					0,
					SurfaceStateTexture->SizeY - 1);
				const int32 PlayerYFlipped = SurfaceStateTexture->SizeY - 1 - PlayerY;
				const FLinearColor PlayerSample = SampleAt(PlayerX, PlayerY);
				const FLinearColor PlayerSampleFlipped = SampleAt(PlayerX, PlayerYFlipped);
				int32 PlayerNeighborhoodLive = 0;
				int32 PlayerNeighborhoodLiveFlipped = 0;
				constexpr int32 NeighborhoodRadius = 32;
				for (int32 OffsetY = -NeighborhoodRadius; OffsetY <= NeighborhoodRadius; ++OffsetY)
				{
					for (int32 OffsetX = -NeighborhoodRadius; OffsetX <= NeighborhoodRadius; ++OffsetX)
					{
						const int32 X = FMath::Clamp(PlayerX + OffsetX, 0, SurfaceStateTexture->SizeX - 1);
						const int32 Y = FMath::Clamp(PlayerY + OffsetY, 0, SurfaceStateTexture->SizeY - 1);
						const int32 YFlipped = FMath::Clamp(
							PlayerYFlipped + OffsetY, 0, SurfaceStateTexture->SizeY - 1);
						PlayerNeighborhoodLive += SampleAt(X, Y).R >= 0.75f ? 1 : 0;
						PlayerNeighborhoodLiveFlipped += SampleAt(X, YFlipped).R >= 0.75f ? 1 : 0;
					}
				}
				UE_LOG(LogDarkwellSightWeave, Display,
					TEXT("SurfaceCoverageReadback extent=%dx%d pixels=%d "
						"min=(%.4f,%.4f,%.4f,%.4f) max=(%.4f,%.4f,%.4f,%.4f) "
						"mean=(%.4f,%.4f,%.4f,%.4f) center=(%.4f,%.4f,%.4f,%.4f) "
						"quarter=(%.4f,%.4f,%.4f,%.4f) "
						"playerWorld=(%.3f,%.3f) playerUV=(%.6f,%.6f) playerTexel=(%d,%d) "
						"player=(%.4f,%.4f,%.4f,%.4f) flippedY=%d flipped=(%.4f,%.4f,%.4f,%.4f) "
						"neighborhoodLive=%d neighborhoodLiveFlipped=%d "
						"valid=%lld unknown=%lld remembered=%lld live=%lld"),
					SurfaceStateTexture->SizeX, SurfaceStateTexture->SizeY, Pixels.Num(),
					Min.R, Min.G, Min.B, Min.A, Max.R, Max.G, Max.B, Max.A,
					Mean.R, Mean.G, Mean.B, Mean.A,
					Center.R, Center.G, Center.B, Center.A,
					Quarter.R, Quarter.G, Quarter.B, Quarter.A,
					PlayerWorld.X, PlayerWorld.Y, PlayerUV.X, PlayerUV.Y, PlayerX, PlayerY,
					PlayerSample.R, PlayerSample.G, PlayerSample.B, PlayerSample.A,
					PlayerYFlipped,
					PlayerSampleFlipped.R, PlayerSampleFlipped.G,
					PlayerSampleFlipped.B, PlayerSampleFlipped.A,
					PlayerNeighborhoodLive, PlayerNeighborhoodLiveFlipped,
					ValidCount, UnknownCount, RememberedCount, LiveCount);
			}
			else
			{
				UE_LOG(LogDarkwellSightWeave, Error,
					TEXT("SurfaceCoverageReadback failed success=%d pixels=%d expected=%d format=%d"),
					bReadSucceeded ? 1 : 0,
					Pixels.Num(), ExpectedPixelCount,
					static_cast<int32>(SurfaceStateTexture->GetFormat()));
			}
			bDiagnosticCoverageReadbackComplete = true;
		}
	}
	else
	{
		DiagnosticCoverageReadbackFrameCount = 0;
		bDiagnosticCoverageReadbackComplete = false;
	}
#endif
#endif
}

TStatId UDarkwellSightWeaveWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(
		UDarkwellSightWeaveWorldSubsystem, STATGROUP_Tickables);
}

bool UDarkwellSightWeaveWorldSubsystem::RequestSightWeaveAuthority(
	ADarkwellVisionIntegrationFixture* Fixture)
{
	if (!Fixture || Fixture->GetWorld() != GetWorld())
	{
		return false;
	}
	if (RequestedFixture.IsValid() && RequestedFixture.Get() != Fixture)
	{
		RollbackToLegacy(TEXT("A second integration fixture requested authority"), true);
		return false;
	}
	if (IsSightWeaveAuthorityActive())
	{
		return RequestedFixture.Get() == Fixture;
	}
	RequestedFixture = Fixture;
	RequestAgeSeconds = 0.0;
	Diagnostics.RequestedMode = EDarkwellVisibilityAuthorityMode::SightWeave;
	Diagnostics.State = EDarkwellVisibilityAuthorityState::SightWeaveRequested;
	Diagnostics.FailureReason.Reset();
	TryActivate();
	return Diagnostics.State == EDarkwellVisibilityAuthorityState::SightWeaveRequested
		|| IsSightWeaveAuthorityActive();
}

bool UDarkwellSightWeaveWorldSubsystem::TryGetSubjectSnapshot(
	const FName StableSubjectId,
	FDarkwellVisibilitySubjectSnapshot& OutSnapshot) const
{
	OutSnapshot = FDarkwellVisibilitySubjectSnapshot();
	const FDarkwellVisibilitySubjectSnapshot* Snapshot =
		SubjectSnapshots.Find(StableSubjectId);
	if (!IsSightWeaveAuthorityActive() || !Snapshot
		|| !Snapshot->IsUsableFor(StableSubjectId))
	{
		return false;
	}
	OutSnapshot = *Snapshot;
	return true;
}

bool UDarkwellSightWeaveWorldSubsystem::TryActivate()
{
	if (Diagnostics.State != EDarkwellVisibilityAuthorityState::SightWeaveRequested
		|| !RequestedFixture.IsValid())
	{
		return false;
	}
	ADarkwellCharacter* FoundPlayer = nullptr;
	ADarkwellStalkerCharacter* FoundStalker = nullptr;
	FSightWeaveFloorDefinition Floor;
	FSightWeaveVisionSourceDescription Body;
	FSightWeaveVisionSourceDescription Cone;
	FSightWeaveIlluminationSourceDescription Torch;
	TArray<FSightWeaveSegment2D> Segments;
	TArray<FSightWeaveStaticEnvironmentDescription> StaticDescriptions;
	FString Failure;
	if (!ValidateAndBuildDescriptions(FoundPlayer, FoundStalker, Floor, Body,
		Cone, Torch, Segments, StaticDescriptions, Failure))
	{
		Diagnostics.FailureReason = Failure;
		return false;
	}

	Player = FoundPlayer;
	Stalker = FoundStalker;
	// P1 is intentionally a no-dynamic-object visual proof. Keep the subject
	// registered for authority checks but prevent its controller from attacking
	// or moving the player during the long coverage captures.
	if (AController* StalkerController = FoundStalker->GetController())
	{
		StalkerController->SetActorTickEnabled(false);
	}
	FloorId = Floor.FloorId;
	KnowledgeOwnerId = Body.KnowledgeOwnerId;
	BodyDescription = Body;
	ConeDescription = Cone;
	TorchDescription = Torch;
	SetLegacyConsumersEnabled(false);
#if !UE_SERVER
	// Suppress before the first floor publication so the SightWeave default-scope
	// path never registers its old composite or tile presentation for this candidate.
	RenderSubsystem->SetPresentationSuppressed(true);
#endif
	if (!RuntimeSubsystem->RegisterFloor(Floor, RequestedFixture.Get()))
	{
		RollbackToLegacy(TEXT("Floor registration failed"), true);
		return false;
	}
	BodyVisionHandle = RuntimeSubsystem->RegisterVisionSource(Body, FoundPlayer);
	ConeVisionHandle = RuntimeSubsystem->RegisterVisionSource(Cone, FoundPlayer);
	TorchIlluminationHandle = RuntimeSubsystem->RegisterIlluminationSource(Torch, FoundPlayer);
	OccluderHandle = RuntimeSubsystem->RegisterOccluder(
		Segments, false, true, RequestedFixture.Get());
	if (!BodyVisionHandle.IsValid() || !ConeVisionHandle.IsValid()
		|| !TorchIlluminationHandle.IsValid() || !OccluderHandle.IsValid())
	{
		RollbackToLegacy(TEXT("Dynamic authority registration failed"), true);
		return false;
	}
	if (!RuntimeSubsystem->ConfigureExplorationMemory(
		KnowledgeOwnerId, FloorId, ESightWeaveRenderPrecisionTier::Ultra))
	{
		RollbackToLegacy(TEXT("Ultra exploration memory configuration failed"), true);
		return false;
	}
	for (const FSightWeaveStaticEnvironmentDescription& Description : StaticDescriptions)
	{
		const FSightWeaveStaticEnvironmentHandle Handle =
			RuntimeSubsystem->RegisterStaticEnvironment(Description, RequestedFixture.Get());
		if (!Handle.IsValid())
		{
			RollbackToLegacy(TEXT("Static-environment registration failed"), true);
			return false;
		}
		StaticEnvironmentHandles.Add(Handle);
	}

	FSightWeaveMemoryScopeKey Scope;
	if (!RuntimeSubsystem->GetExplorationMemoryScope(Scope))
	{
		RollbackToLegacy(TEXT("The exact exploration-memory scope was unavailable"), true);
		return false;
	}
	StalkerSubjectRegistration.Identity.StableId = FoundStalker->GetPersistentId();
	StalkerSubjectRegistration.Identity.InstanceGeneration =
		static_cast<int64>(Diagnostics.WorldGeneration);
	StalkerSubjectRegistration.Scope = Scope;
	StalkerSubjectRegistration.Policy = ESightWeaveSubjectMemoryPolicy::NeverRemember;
	StalkerSubjectHandle = SubjectAuthority.Register(StalkerSubjectRegistration);
	if (!StalkerSubjectHandle.IsValid())
	{
		RollbackToLegacy(TEXT("Stalker NeverRemember registration failed"), true);
		return false;
	}
#if !UE_SERVER
	TArray<FDarkwellFogVisualSegment> FogVisualSegments;
	FogVisualSegments.Reserve(Segments.Num());
	for (const FSightWeaveSegment2D& Segment : Segments)
	{
		FDarkwellFogVisualSegment& FogSegment = FogVisualSegments.AddDefaulted_GetRef();
		FogSegment.A = Segment.A;
		FogSegment.B = Segment.B;
	}
	if (!FogVisualSubsystem
		|| !FogVisualSubsystem->ActivateP2(
			RequestedFixture.Get(),
			BuildFogVisualSourceSnapshot(),
			FogVisualSegments))
	{
		RollbackToLegacy(TEXT("DARKWELL P2 continuous occlusion activation failed"), true);
		return false;
	}
#endif
	Diagnostics.ActiveMode = EDarkwellVisibilityAuthorityMode::SightWeave;
	Diagnostics.State = EDarkwellVisibilityAuthorityState::ActiveSightWeave;
	Diagnostics.FailureReason.Reset();
	Diagnostics.FloorCount = 1;
	Diagnostics.VisionSourceCount = 2;
	Diagnostics.IlluminationSourceCount = 1;
	Diagnostics.OccluderCount = 1;
	Diagnostics.StaticEnvironmentCount = StaticEnvironmentHandles.Num();
	Diagnostics.SubjectCount = 1;
	Diagnostics.bLegacyWritesEnabled = false;
	Diagnostics.bLegacyPresentationEnabled = false;
	Diagnostics.bSightWeavePresentationEnabled = false;
	Diagnostics.bProjectFogPresentationEnabled = true;
	UpdateSubjectAuthority();
	FoundStalker->SetActorHiddenInGame(true);
	UE_LOG(LogDarkwellSightWeave, Log,
		TEXT("World=%s authority=SightWeave active visual=DarkwellContinuousP2 segments=%d oldSightWeaveVisual=Suppressed floor=%s vision=2 light=1 occluder=1 static=%d subject=%s"),
		*Diagnostics.WorldName.ToString(),
		FogVisualSegments.Num(),
		*FloorId.GetValue().ToString(),
		StaticEnvironmentHandles.Num(), *FoundStalker->GetPersistentId().ToString());
	return true;
}

bool UDarkwellSightWeaveWorldSubsystem::ValidateAndBuildDescriptions(
	ADarkwellCharacter*& OutPlayer,
	ADarkwellStalkerCharacter*& OutStalker,
	FSightWeaveFloorDefinition& OutFloor,
	FSightWeaveVisionSourceDescription& OutBody,
	FSightWeaveVisionSourceDescription& OutCone,
	FSightWeaveIlluminationSourceDescription& OutTorch,
	TArray<FSightWeaveSegment2D>& OutSegments,
	TArray<FSightWeaveStaticEnvironmentDescription>& OutStatic,
	FString& OutFailure) const
{
	OutPlayer = nullptr;
	OutStalker = nullptr;
	if (!HasRequiredSightWeaveServices())
	{
		OutFailure = TEXT("Required Runtime/Render world services are unavailable");
		return false;
	}
	if (RuntimeSubsystem->GetFloorCount() != 0
		|| RuntimeSubsystem->GetVisionSourceCount() != 0
		|| RuntimeSubsystem->GetIlluminationSourceCount() != 0
		|| RuntimeSubsystem->GetOccluderCount() != 0)
	{
		OutFailure = TEXT("The dedicated integration world already contains SightWeave registrations");
		return false;
	}

	UWorld* World = GetWorld();
	int32 PlayerCount = 0;
	for (TActorIterator<ADarkwellCharacter> It(World); It; ++It)
	{
		OutPlayer = *It;
		++PlayerCount;
	}
	int32 StalkerCount = 0;
	for (TActorIterator<ADarkwellStalkerCharacter> It(World); It; ++It)
	{
		if (It->GetClass() == ADarkwellStalkerCharacter::StaticClass())
		{
			OutStalker = *It;
			++StalkerCount;
		}
	}
	if (PlayerCount != 1 || StalkerCount != 1 || !OutStalker
		|| OutStalker->GetPersistentId().IsNone())
	{
		OutFailure = FString::Printf(
			TEXT("Expected one player and one base Stalker with a stable ID (players=%d stalkers=%d)"),
			PlayerCount, StalkerCount);
		return false;
	}

	const ADarkwellVisionIntegrationFixture* Fixture = RequestedFixture.Get();
	const FBox2D Bounds = Fixture->GetSightWeaveFloorBounds();
	const float FloorZ = Fixture->GetActorLocation().Z;
	FSightWeaveHeightRange HeightRange;
	HeightRange.ZMin = FloorZ - 100.0f;
	HeightRange.ZMax = FloorZ + 300.0f;
	OutFloor.FloorId = FSightWeaveFloorId(Darkwell::SightWeaveAdapter::FloorName);
	OutFloor.BoundsMin = Bounds.Min;
	OutFloor.BoundsMax = Bounds.Max;
	OutFloor.HeightRange = HeightRange;

	const FSightWeaveKnowledgeOwnerId Owner(Darkwell::SightWeaveAdapter::OwnerName);
	const FTransform SourceTransform =
		Darkwell::SightWeaveAdapter::BuildSourceTransform(*OutPlayer);
	OutBody.Transform = SourceTransform;
	OutBody.KnowledgeOwnerId = Owner;
	OutBody.FloorId = OutFloor.FloorId;
	OutBody.HeightRange = HeightRange;
	OutBody.Shape = ESightWeaveSourceShape::Radial;
	OutBody.Range = 120.0f;
	OutBody.HalfAngleDegrees = 180.0f;
	OutBody.IlluminationPolicy = ESightWeaveIlluminationPolicy::BypassLegalIllumination;

	OutCone.Transform = SourceTransform;
	OutCone.KnowledgeOwnerId = Owner;
	OutCone.FloorId = OutFloor.FloorId;
	OutCone.HeightRange = HeightRange;
	OutCone.Shape = ESightWeaveSourceShape::DirectionalCone;
	OutCone.Range = 2200.0f;
	OutCone.HalfAngleDegrees = FMath::Lerp(
		52.0f, 35.0f, OutPlayer->GetShotgunAimProgress());
	OutCone.IlluminationPolicy = ESightWeaveIlluminationPolicy::RequiresLegalIllumination;
	OutCone.Compatibility.AcceptedCapabilities.Add(
		Darkwell::SightWeaveAdapter::TorchCapability);
	OutCone.Compatibility.Normalize();

	const UDarkwellLoadoutComponent* Loadout = OutPlayer->GetLoadoutComponent();
	OutTorch.Transform = SourceTransform;
	OutTorch.KnowledgeOwnerId = Owner;
	OutTorch.FloorId = OutFloor.FloorId;
	OutTorch.HeightRange = HeightRange;
	OutTorch.Shape = ESightWeaveSourceShape::Radial;
	OutTorch.Range = 1250.0f;
	OutTorch.HalfAngleDegrees = 180.0f;
	OutTorch.bActive = Loadout && OutPlayer->IsAlive()
		&& Loadout->IsTorchOn() && Loadout->GetTorchCharge() > 0.0f;
	OutTorch.EmittedCapabilities.Add(Darkwell::SightWeaveAdapter::TorchCapability);
	OutTorch.NormalizeCapabilities();

	TArray<FDarkwellVisionIntegrationSegment> FixtureSegments;
	Fixture->BuildSightWeaveOccluderSegments(FixtureSegments);
	OutSegments.Reset(FixtureSegments.Num());
	for (const FDarkwellVisionIntegrationSegment& Segment : FixtureSegments)
	{
		FSightWeaveSegment2D& Converted = OutSegments.AddDefaulted_GetRef();
		Converted.A = Segment.A;
		Converted.B = Segment.B;
		Converted.FloorId = OutFloor.FloorId;
		Converted.HeightRange.ZMin = Segment.ZMin;
		Converted.HeightRange.ZMax = Segment.ZMax;
	}

	TArray<FDarkwellVisionIntegrationSurface> FixtureSurfaces;
	Fixture->BuildSightWeaveStaticSurfaces(FixtureSurfaces);
	OutStatic.Reset(FixtureSurfaces.Num());
	for (const FDarkwellVisionIntegrationSurface& Surface : FixtureSurfaces)
	{
		FSightWeaveStaticEnvironmentDescription& Converted =
			OutStatic.AddDefaulted_GetRef();
		Converted.KnowledgeOwnerId = Owner;
		Converted.FloorId = OutFloor.FloorId;
		Converted.HeightRange = HeightRange;
		Converted.WorldFootprint = Surface.WorldFootprint;
		Converted.NeutralIntensity = Surface.NeutralIntensity;
		Converted.bExplicitlyImmutable = true;
	}

	if (!OutFloor.IsValid() || !OutBody.IsValid() || !OutCone.IsValid()
		|| !OutTorch.IsValid() || OutSegments.Num() != 11 || OutStatic.Num() != 4)
	{
		OutFailure = FString::Printf(
			TEXT("One or more project-fog declarations are invalid (segments=%d static=%d)"),
			OutSegments.Num(),
			OutStatic.Num());
		return false;
	}
	for (const FSightWeaveSegment2D& Segment : OutSegments)
	{
		if (!Segment.IsFinite())
		{
			OutFailure = TEXT("An integration occluder segment is invalid");
			return false;
		}
	}
	for (const FSightWeaveStaticEnvironmentDescription& Description : OutStatic)
	{
		if (!Description.IsValid())
		{
			OutFailure = TEXT("An integration static-environment surface is invalid");
			return false;
		}
	}
	return true;
}

void UDarkwellSightWeaveWorldSubsystem::UpdateDynamicAuthority()
{
	ADarkwellCharacter* Character = Player.Get();
	if (!Character || !RuntimeSubsystem)
	{
		return;
	}
	const FTransform Transform = Darkwell::SightWeaveAdapter::BuildSourceTransform(*Character);
	if (!BodyDescription.Transform.Equals(Transform))
	{
		BodyDescription.Transform = Transform;
		ConeDescription.Transform = Transform;
		TorchDescription.Transform = Transform;
		RuntimeSubsystem->UpdateVisionSourceTransform(BodyVisionHandle, Transform);
		RuntimeSubsystem->UpdateVisionSourceTransform(ConeVisionHandle, Transform);
		RuntimeSubsystem->UpdateIlluminationSourceTransform(TorchIlluminationHandle, Transform);
	}
	const float HalfAngle = FMath::Lerp(
		52.0f, 35.0f, Character->GetShotgunAimProgress());
	if (!FMath::IsNearlyEqual(ConeDescription.HalfAngleDegrees, HalfAngle))
	{
		ConeDescription.HalfAngleDegrees = HalfAngle;
		RuntimeSubsystem->UpdateVisionSource(ConeVisionHandle, ConeDescription);
	}
	const UDarkwellLoadoutComponent* Loadout = Character->GetLoadoutComponent();
	const bool bTorchActive = Loadout && Character->IsAlive()
		&& Loadout->IsTorchOn() && Loadout->GetTorchCharge() > 0.0f;
	if (TorchDescription.bActive != bTorchActive)
	{
		TorchDescription.bActive = bTorchActive;
		RuntimeSubsystem->UpdateIlluminationSource(TorchIlluminationHandle, TorchDescription);
	}
}

void UDarkwellSightWeaveWorldSubsystem::UpdateSubjectAuthority()
{
	ADarkwellStalkerCharacter* Subject = Stalker.Get();
	if (!Subject || !RuntimeSubsystem || !StalkerSubjectHandle.IsValid())
	{
		return;
	}
	FSightWeaveQuerySampleSet SampleSet;
	const FVector Location = Subject->GetActorLocation();
	SampleSet.Samples = {
		Location + FVector(0.0, 0.0, 40.0),
		Location + FVector(0.0, 0.0, 100.0)};
	SampleSet.Rule = ESightWeaveSampleRule::AnySample;
	SampleSet.RequiredCount = 1;
	const FSightWeaveVisibilityQueryResult Query = RuntimeSubsystem->QuerySamples(
		KnowledgeOwnerId, FloorId, SampleSet);

	FSightWeaveSubjectObservation Observation;
	Observation.Identity = StalkerSubjectRegistration.Identity;
	Observation.Scope = StalkerSubjectRegistration.Scope;
	Observation.ObservationRevision = NextObservationRevision++;
	Observation.EligibilityRevision =
		static_cast<uint64>(FMath::Max<int64>(0, Query.SnapshotRevision.GetValue()));
	Observation.SourceLiveRevision = Observation.EligibilityRevision;
	Observation.TransitionIdentity = Observation.ObservationRevision;
	Observation.bHardLive = Query.bAuthoritative && Query.bVisible;
	Observation.bEligibleForMemoryWrite = false;
	const FSightWeaveSubjectTransitionResult Transition =
		SubjectAuthority.SubmitObservation(StalkerSubjectHandle, Observation);

	FSightWeaveSubjectPresentationContext Context;
	Context.Identity = Observation.Identity;
	Context.Scope = Observation.Scope;
	Context.EligibilityRevision = Observation.EligibilityRevision;
	Context.SourceLiveRevision = Observation.SourceLiveRevision;
	Context.bHardLive = Observation.bHardLive;
	Context.bBlockMemoryWrites = true;
	Context.bSuppressMemoryPresentation = true;
	const FSightWeaveSubjectPresentationResult Presentation =
		SubjectAuthority.EvaluatePresentation(StalkerSubjectHandle, Context);
	const bool bAuthoritative = Query.bAuthoritative && Transition.Succeeded()
		&& Presentation.Failure == ESightWeaveSubjectPresentationFailure::None;
	const bool bHardLive = bAuthoritative
		&& Presentation.State == ESightWeaveSubjectPresentationState::Live;

	FDarkwellVisibilitySubjectSnapshot Snapshot;
	Snapshot.StableSubjectId = Subject->GetPersistentId();
	Snapshot.AuthorityMode = EDarkwellVisibilityAuthorityMode::SightWeave;
	Snapshot.AuthorityRevision = NextAuthorityRevision++;
	Snapshot.SourceSnapshotRevision = Query.SnapshotRevision.GetValue();
	Snapshot.bAuthoritative = bAuthoritative;
	Snapshot.bHardLive = bHardLive;
	SubjectSnapshots.Add(Snapshot.StableSubjectId, Snapshot);
	Subject->ApplySightWeaveVisibility(bHardLive, Snapshot.AuthorityRevision);
	Diagnostics.AuthorityRevision = Snapshot.AuthorityRevision;
	Diagnostics.RuntimeSnapshotRevision = Snapshot.SourceSnapshotRevision;
}

FDarkwellFogVisualSourceSnapshot
UDarkwellSightWeaveWorldSubsystem::BuildFogVisualSourceSnapshot() const
{
	FDarkwellFogVisualSourceSnapshot Result;
	const FVector BodyLocation = BodyDescription.Transform.GetLocation();
	const FVector ConeLocation = ConeDescription.Transform.GetLocation();
	const FVector Forward3D = ConeDescription.Transform.GetUnitAxis(EAxis::X);
	Result.BodyCenter = FVector2D(BodyLocation.X, BodyLocation.Y);
	Result.ConeOrigin = FVector2D(ConeLocation.X, ConeLocation.Y);
	Result.ConeForward = FVector2D(Forward3D.X, Forward3D.Y).GetSafeNormal();
	Result.BodyRadiusCentimeters = BodyDescription.Range;
	Result.ConeRangeCentimeters = FMath::Min(ConeDescription.Range, TorchDescription.Range);
	Result.ConeHalfAngleDegrees = ConeDescription.HalfAngleDegrees;
	Result.bConeLegallyLive = ConeDescription.bActive && TorchDescription.bActive;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const FVector2D DiagnosticOffset = FVector2D(
		Darkwell::SightWeaveAdapter::CVarDiagnosticFogSourceOffsetTexelsX.GetValueOnGameThread(),
		Darkwell::SightWeaveAdapter::CVarDiagnosticFogSourceOffsetTexelsY.GetValueOnGameThread())
		* 2.5;
	Result.BodyCenter += DiagnosticOffset;
	Result.ConeOrigin += DiagnosticOffset;
#endif
	Result.AuthorityRevision = RuntimeSubsystem
		? static_cast<uint64>(FMath::Max<int64>(0, RuntimeSubsystem->GetRevision().GetValue()))
		: 0;
	return Result;
}

void UDarkwellSightWeaveWorldSubsystem::SetLegacyConsumersEnabled(const bool bEnabled)
{
	if (ADarkwellCharacter* Character = Player.Get())
	{
		if (UDarkwellVisibilityComponent* Visibility = Character->GetVisibilityComponent())
		{
			Visibility->SetVisibilityAuthorityEnabled(bEnabled);
		}
	}
	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* Controller = World->GetFirstPlayerController())
		{
			if (ADarkwellHUD* HUD = Cast<ADarkwellHUD>(Controller->GetHUD()))
			{
				HUD->SetLegacyFogAuthorityEnabled(bEnabled);
			}
		}
	}
}

void UDarkwellSightWeaveWorldSubsystem::RollbackToLegacy(
	const FString& FailureReason,
	const bool bRestoreConsumers)
{
	if (FogVisualSubsystem)
	{
		FogVisualSubsystem->Deactivate();
	}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bDiagnosticProofCameraActive)
	{
		if (APlayerController* Controller = GetWorld()
			? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			Controller->SetViewTarget(Player.Get());
		}
		bDiagnosticProofCameraActive = false;
	}
#endif
	if (ADarkwellVisionIntegrationFixture* Fixture = RequestedFixture.Get())
	{
		Fixture->DisableSightWeaveSurfaceMaterial();
	}
	if (RuntimeSubsystem)
	{
		for (int32 Index = StaticEnvironmentHandles.Num() - 1; Index >= 0; --Index)
		{
			RuntimeSubsystem->UnregisterStaticEnvironment(StaticEnvironmentHandles[Index]);
		}
		StaticEnvironmentHandles.Reset();
		SubjectAuthority.Reset();
		StalkerSubjectHandle = FSightWeaveSubjectHandle();
		RuntimeSubsystem->DisableExplorationMemory();
		if (OccluderHandle.IsValid())
		{
			RuntimeSubsystem->UnregisterOccluder(OccluderHandle);
		}
		if (TorchIlluminationHandle.IsValid())
		{
			RuntimeSubsystem->UnregisterIlluminationSource(TorchIlluminationHandle);
		}
		if (ConeVisionHandle.IsValid())
		{
			RuntimeSubsystem->UnregisterVisionSource(ConeVisionHandle);
		}
		if (BodyVisionHandle.IsValid())
		{
			RuntimeSubsystem->UnregisterVisionSource(BodyVisionHandle);
		}
		if (FloorId.IsValid())
		{
			RuntimeSubsystem->UnregisterFloor(FloorId);
		}
	}
#if !UE_SERVER
	if (RenderSubsystem)
	{
		RenderSubsystem->DisableSurfaceMaterialPresentation();
		RenderSubsystem->ClearPresentationScope();
		RenderSubsystem->SetPresentationSuppressed(false);
	}
#endif
	if (bRestoreConsumers)
	{
		if (ADarkwellStalkerCharacter* Subject = Stalker.Get())
		{
			if (AController* StalkerController = Subject->GetController())
			{
				StalkerController->SetActorTickEnabled(true);
			}
			Subject->ApplySightWeaveVisibility(true, 0);
		}
		SetLegacyConsumersEnabled(true);
	}
	const FName WorldName = Diagnostics.WorldName;
	const uint64 WorldGeneration = Diagnostics.WorldGeneration;
	const bool bRuntimeAvailable = Diagnostics.bRuntimeServiceAvailable;
	const bool bRenderAvailable = Diagnostics.bRenderServiceAvailable;
	ResetToLegacy();
	Diagnostics.WorldName = WorldName;
	Diagnostics.WorldGeneration = WorldGeneration;
	Diagnostics.bRuntimeServiceAvailable = bRuntimeAvailable;
	Diagnostics.bRenderServiceAvailable = bRenderAvailable;
	if (!FailureReason.IsEmpty() && FailureReason != TEXT("World teardown"))
	{
		Diagnostics.State = EDarkwellVisibilityAuthorityState::SightWeaveFailed;
		Diagnostics.RequestedMode = EDarkwellVisibilityAuthorityMode::SightWeave;
		Diagnostics.FailureReason = FailureReason;
		UE_LOG(LogDarkwellSightWeave, Error,
			TEXT("World=%s authority=Legacy SightWeave activation failed: %s"),
			*Diagnostics.WorldName.ToString(), *FailureReason);
	}
	BodyVisionHandle = FSightWeaveVisionSourceHandle();
	ConeVisionHandle = FSightWeaveVisionSourceHandle();
	TorchIlluminationHandle = FSightWeaveIlluminationSourceHandle();
	OccluderHandle = FSightWeaveOccluderHandle();
	FloorId = FSightWeaveFloorId();
	KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId();
	SubjectSnapshots.Reset();
	Player.Reset();
	Stalker.Reset();
	RequestedFixture.Reset();
}

bool UDarkwellSightWeaveWorldSubsystem::HasRequiredSightWeaveServices() const
{
	if (!Diagnostics.bRuntimeServiceAvailable || !RuntimeSubsystem)
	{
		return false;
	}
#if UE_SERVER
	return true;
#else
	return Diagnostics.bRenderServiceAvailable
		&& RenderSubsystem
		&& FogVisualSubsystem;
#endif
}

void UDarkwellSightWeaveWorldSubsystem::ResetToLegacy()
{
	Diagnostics.RequestedMode = EDarkwellVisibilityAuthorityMode::Legacy;
	Diagnostics.ActiveMode = EDarkwellVisibilityAuthorityMode::Legacy;
	Diagnostics.State = EDarkwellVisibilityAuthorityState::Legacy;
	Diagnostics.FailureReason.Reset();
	Diagnostics.AuthorityRevision = 0;
	Diagnostics.RuntimeSnapshotRevision = 0;
	Diagnostics.FloorCount = 0;
	Diagnostics.VisionSourceCount = 0;
	Diagnostics.IlluminationSourceCount = 0;
	Diagnostics.OccluderCount = 0;
	Diagnostics.StaticEnvironmentCount = 0;
	Diagnostics.SubjectCount = 0;
	Diagnostics.bLegacyWritesEnabled = true;
	Diagnostics.bLegacyPresentationEnabled = true;
	Diagnostics.bSightWeavePresentationEnabled = false;
	Diagnostics.bProjectFogPresentationEnabled = false;
}
