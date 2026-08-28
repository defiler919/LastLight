#include "SightWeaveM4P1LabFixture.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "SightWeaveComponents.h"
#include "SightWeaveLastSeenProxyComponent.h"
#include "SightWeaveSettings.h"
#include "SightWeaveWorldSubsystem.h"

namespace
{
	constexpr int32 MinimumState = 0;
	constexpr int32 MaximumState = 5;
	const FSoftObjectPath CubePath(TEXT("/Engine/BasicShapes/Cube.Cube"));
	const FSoftObjectPath MaterialPath(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	class FM4P1AcceptedCustomProvider final : public ISightWeaveSubjectSnapshotProvider
	{
	public:
		explicit FM4P1AcceptedCustomProvider(FSightWeaveBasicStaticMeshSnapshotCandidate InCandidate)
			: Candidate(MoveTemp(InCandidate))
		{
		}

		virtual FName GetSightWeaveProviderName() const override
		{
			return FName(TEXT("M4P1LabCustom"));
		}

		virtual uint32 GetSightWeaveProviderVersion() const override
		{
			return 1;
		}

		virtual bool BuildSightWeaveSnapshotCandidate(
			const FSightWeaveSubjectRegistration&,
			const FSightWeaveSubjectObservation&,
			FSightWeaveBasicStaticMeshSnapshotCandidate& OutCandidate) const override
		{
			OutCandidate = Candidate;
			return true;
		}

	private:
		FSightWeaveBasicStaticMeshSnapshotCandidate Candidate;
	};

	AActor* SpawnRootedActor(UWorld* World, const TCHAR* Label, const FVector& Location)
	{
		if (!IsValid(World))
		{
			return nullptr;
		}
		FActorSpawnParameters Parameters;
		Parameters.ObjectFlags |= RF_Transient;
		Parameters.OverrideLevel = World->PersistentLevel;
		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), Location,
			FRotator::ZeroRotator, Parameters);
		if (!IsValid(Actor))
		{
			return nullptr;
		}
		Actor->SetActorLabel(Label);
		Actor->Tags.AddUnique(FName(TEXT("SightWeaveM4P1Lab")));
		USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("M4P1Root"));
		Actor->AddInstanceComponent(Root);
		Actor->SetRootComponent(Root);
		Root->RegisterComponent();
		Actor->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		return Actor;
	}

	ACameraActor* SpawnCamera(
		UWorld* World,
		const TCHAR* Label,
		const FVector& Location,
		const float Yaw,
		const float OrthoWidth)
	{
		FActorSpawnParameters Parameters;
		Parameters.ObjectFlags |= RF_Transient;
		Parameters.OverrideLevel = World ? World->PersistentLevel : nullptr;
		ACameraActor* Camera = IsValid(World)
			? World->SpawnActor<ACameraActor>(Location, FRotator(-90.0f, Yaw, 0.0f), Parameters)
			: nullptr;
		if (!IsValid(Camera))
		{
			return nullptr;
		}
		Camera->SetActorLabel(Label);
		Camera->Tags.AddUnique(FName(TEXT("SightWeaveM4P1Lab")));
		Camera->SetActorHiddenInGame(true);
		Camera->GetCameraComponent()->ProjectionMode = ECameraProjectionMode::Orthographic;
		Camera->GetCameraComponent()->OrthoWidth = OrthoWidth;
		return Camera;
	}

	USightWeaveVisionSourceComponent* AddVisionSource(
		AActor* Actor,
		const float Range,
		const bool bActive)
	{
		if (!IsValid(Actor))
		{
			return nullptr;
		}
		USightWeaveVisionSourceComponent* Vision =
			NewObject<USightWeaveVisionSourceComponent>(Actor, TEXT("M4P1Vision"));
		Actor->AddInstanceComponent(Vision);
		Vision->SetupAttachment(Actor->GetRootComponent());
		Vision->Description.KnowledgeOwnerId =
			FSightWeaveKnowledgeOwnerId(FName(TEXT("Local")));
		Vision->Description.FloorId = FSightWeaveFloorId(FName(TEXT("Ground")));
		Vision->Description.HeightRange = { 0.0f, 300.0f };
		Vision->Description.Shape = ESightWeaveSourceShape::Radial;
		Vision->Description.Range = Range;
		Vision->Description.HalfAngleDegrees = 180.0f;
		Vision->Description.bActive = bActive;
		Vision->Description.IlluminationPolicy =
			ESightWeaveIlluminationPolicy::BypassLegalIllumination;
		Vision->RegisterComponent();
		return Vision->GetVisionSourceHandle().IsValid() ? Vision : nullptr;
	}

	FSightWeaveSubjectObservation MakeObservation(
		const FSightWeaveSubjectRegistration& Registration,
		const FSightWeaveBasicStaticMeshSnapshotCandidate& Candidate,
		const uint64 Revision,
		const bool bHardLive,
		const uint64 TransitionIdentity = 0)
	{
		FSightWeaveSubjectObservation Observation;
		Observation.Identity = Registration.Identity;
		Observation.Scope = Registration.Scope;
		Observation.ObservationRevision = Revision;
		Observation.EligibilityRevision = 100;
		Observation.SourceLiveRevision = 200;
		Observation.TransitionIdentity = TransitionIdentity;
		Observation.bHardLive = bHardLive;
		Observation.bEligibleForMemoryWrite = true;
		Observation.BasicSnapshot = Candidate;
		return Observation;
	}

	FSightWeaveSubjectPresentationContext MakeContext(
		const FSightWeaveSubjectRegistration& Registration,
		const FSightWeaveLastSeenSnapshotDescriptor* Snapshot,
		const bool bHardLive)
	{
		FSightWeaveSubjectPresentationContext Context;
		Context.Identity = Registration.Identity;
		Context.Scope = Registration.Scope;
		Context.bHardLive = bHardLive;
		Context.bHardMemoryAtSnapshot = true;
		if (Snapshot)
		{
			Context.SnapshotRevision = Snapshot->SnapshotRevision;
			Context.EligibilityRevision = Snapshot->EligibilityRevision;
			Context.SourceLiveRevision = Snapshot->SourceLiveRevision;
		}
		return Context;
	}
}

FSightWeaveM4P1LabFixture::~FSightWeaveM4P1LabFixture()
{
	Shutdown();
}

bool FSightWeaveM4P1LabFixture::Initialize(UWorld* InWorld)
{
	Shutdown();
	if (!IsValid(InWorld) || !IsValid(InWorld->PersistentLevel))
	{
		return false;
	}
	World = InWorld;
	USightWeaveWorldSubsystem* Subsystem = InWorld->GetSubsystem<USightWeaveWorldSubsystem>();
	if (!Subsystem)
	{
		return false;
	}
	const ESightWeaveRenderPrecisionTier Precision =
		GetDefault<USightWeaveSettings>()->ExplorationMemoryPrecisionTier;
	if ((!Subsystem->IsExplorationMemoryConfigured()
		&& !Subsystem->ConfigureExplorationMemory(
			FSightWeaveKnowledgeOwnerId(FName(TEXT("Local"))),
			FSightWeaveFloorId(FName(TEXT("Ground"))),
			Precision))
		|| !Subsystem->GetExplorationMemoryScope(Scope)
		|| !BuildCameras())
	{
		Shutdown();
		return false;
	}
	bReady = RebuildSubjects(MinimumState);
	return bReady;
}

bool FSightWeaveM4P1LabFixture::ApplyState(const int32 InState)
{
	const int32 ClampedState = FMath::Clamp(InState, MinimumState, MaximumState);
	return bReady && (AppliedState == ClampedState || RebuildSubjects(ClampedState));
}

void FSightWeaveM4P1LabFixture::Tick()
{
	if (!StaticEnvironmentCaptureSource.IsValid())
	{
		return;
	}
	USightWeaveWorldSubsystem* Subsystem = World.IsValid()
		? World->GetSubsystem<USightWeaveWorldSubsystem>()
		: nullptr;
	if (Subsystem && Subsystem->QueryHardMemoryAtLocation(StaticEnvironmentSampleLocation))
	{
		StaticEnvironmentCaptureSource->SetVisionSourceEnabled(false);
		StaticEnvironmentCaptureSource.Reset();
	}
}

void FSightWeaveM4P1LabFixture::Shutdown()
{
	Authority.Reset();
	DestroyActors(SubjectActors);
	DestroyActors(CameraActors);
	Scope = FSightWeaveMemoryScopeKey();
	StaticEnvironmentCaptureSource.Reset();
	StaticEnvironmentSampleLocation = FVector::ZeroVector;
	World.Reset();
	AppliedState = INDEX_NONE;
	VisibleProxyCount = 0;
	VisibleLiveCount = 0;
	bReady = false;
}

bool FSightWeaveM4P1LabFixture::BuildCameras()
{
	UWorld* FixtureWorld = World.Get();
	const struct
	{
		const TCHAR* Label;
		FVector Location;
		float Yaw;
		float Width;
	} Definitions[] = {
		{ TEXT("SW_M4P1_Camera0_Overview"), FVector(77500.0, 7500.0, 12000.0), 90.0f, 19000.0f },
		{ TEXT("SW_M4P1_Camera1_Transition"), FVector(72000.0, 7500.0, 6500.0), 90.0f, 5600.0f },
		{ TEXT("SW_M4P1_Camera2_PolicyMatrix"), FVector(77700.0, 5900.0, 6500.0), 90.0f, 3500.0f },
		{ TEXT("SW_M4P1_Camera3_PageBoundary"), FVector(150220.0, 9800.0, 6500.0), 90.0f, 7200.0f },
		{ TEXT("SW_M4P1_Camera4_Rotated45"), FVector(83000.0, 8500.0, 6500.0), 45.0f, 7600.0f }
	};
	for (const auto& Definition : Definitions)
	{
		ACameraActor* Camera = SpawnCamera(
			FixtureWorld,
			Definition.Label,
			Definition.Location,
			Definition.Yaw,
			Definition.Width);
		if (!Camera)
		{
			return false;
		}
		CameraActors.Add(Camera);
	}
	return true;
}

bool FSightWeaveM4P1LabFixture::RebuildSubjects(const int32 InState)
{
	UWorld* FixtureWorld = World.Get();
	if (!IsValid(FixtureWorld))
	{
		return false;
	}
	Authority.Reset();
	DestroyActors(SubjectActors);
	StaticEnvironmentCaptureSource.Reset();
	VisibleProxyCount = 0;
	VisibleLiveCount = 0;

	UStaticMesh* Cube = Cast<UStaticMesh>(CubePath.TryLoad());
	UMaterialInterface* Material = Cast<UMaterialInterface>(MaterialPath.TryLoad());
	if (!Cube || !Material)
	{
		return false;
	}

	// A small off-camera source keeps the exact Local/Ground presentation scope resident.
	AActor* Anchor = SpawnRootedActor(
		FixtureWorld,
		TEXT("SW_M4P1_PresentationScopeAnchor"),
		FVector(-8100.0, -6100.0, 100.0));
	if (!Anchor || !AddVisionSource(Anchor, 300.0f, true))
	{
		return false;
	}
	SubjectActors.Add(Anchor);

	auto AddSubject = [this, FixtureWorld, Cube, Material, InState](
		const TCHAR* Label,
		const FVector& Location,
		const float Yaw,
		const FVector& Scale,
		const ESightWeaveSubjectMemoryPolicy Policy,
		const int64 Generation,
		const bool bSubmitLive,
		const bool bSubmitNonLive,
		const bool bContextLive,
		const bool bSuppress,
		const bool bBlock,
		const bool bUnknown,
		const bool bClear,
		const bool bStaleRevision,
		const bool bCustomValid,
		const bool bCustomInvalid) -> bool
	{
		AActor* Actor = SpawnRootedActor(FixtureWorld, Label, FVector::ZeroVector);
		if (!Actor)
		{
			return false;
		}
		SubjectActors.Add(Actor);
		const FTransform Transform(FRotator(0.0f, Yaw, 0.0f), Location, Scale);

		UStaticMeshComponent* Live = NewObject<UStaticMeshComponent>(Actor, TEXT("M4P1Live"));
		Actor->AddInstanceComponent(Live);
		Live->SetupAttachment(Actor->GetRootComponent());
		Live->SetStaticMesh(Cube);
		Live->SetMaterial(0, Material);
		Live->SetWorldTransform(Transform);
		Live->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Live->SetCastShadow(false);
		Live->RegisterComponent();
		if (Policy == ESightWeaveSubjectMemoryPolicy::StaticEnvironment)
		{
			USightWeaveStaticEnvironmentComponent* StaticEnvironment =
				NewObject<USightWeaveStaticEnvironmentComponent>(Actor, TEXT("M4P1StaticEnvironment"));
			Actor->AddInstanceComponent(StaticEnvironment);
			StaticEnvironment->SetupAttachment(Actor->GetRootComponent());
			StaticEnvironment->KnowledgeOwnerId =
				FSightWeaveKnowledgeOwnerId(FName(TEXT("Local")));
			StaticEnvironment->FloorId = FSightWeaveFloorId(FName(TEXT("Ground")));
			StaticEnvironment->LocalHeightRange = { 0.0f, 300.0f };
			StaticEnvironment->LocalFootprint = {
				FVector2D(-180.0, -180.0), FVector2D(180.0, -180.0),
				FVector2D(180.0, 180.0), FVector2D(-180.0, 180.0)
			};
			StaticEnvironment->NeutralIntensity = 112;
			StaticEnvironment->bExplicitlyImmutable = true;
			StaticEnvironment->bEnabled = true;
			StaticEnvironment->SetWorldLocation(Location);
			StaticEnvironment->RegisterComponent();
			if (!StaticEnvironment->GetStaticEnvironmentHandle().IsValid())
			{
				return false;
			}
		}

		USightWeaveLastSeenProxyComponent* Proxy =
			NewObject<USightWeaveLastSeenProxyComponent>(Actor, TEXT("M4P1Proxy"));
		Actor->AddInstanceComponent(Proxy);
		Proxy->SetupAttachment(Actor->GetRootComponent());
		Proxy->RegisterComponent();

		FSightWeaveSubjectRegistration Registration;
		Registration.Identity.StableId = FName(Label);
		const bool bIdentityReuse = FStringView(Label) == TEXTVIEW("SW_M4P1_PrimaryTransition")
			&& InState == 5;
		Registration.Identity.InstanceGeneration = bIdentityReuse ? 1 : Generation;
		Registration.Scope = Scope;
		Registration.Policy = Policy;
		if (Policy == ESightWeaveSubjectMemoryPolicy::Custom)
		{
			Registration.CustomProviderName = FName(TEXT("M4P1LabCustom"));
			Registration.CustomProviderVersion = 1;
		}
		const FSightWeaveSubjectHandle Handle = Authority.Register(Registration);
		if (!Handle.IsValid())
		{
			return false;
		}

		FSightWeaveBasicStaticMeshSnapshotCandidate Candidate;
		Candidate.WorldTransform = Transform;
		Candidate.WorldBounds = Cube->GetBounds().GetBox().TransformBy(Transform);
		Candidate.StaticMeshAsset = CubePath;
		Candidate.MaterialOverrides = { MaterialPath };
		Candidate.VisualVariantId = FName(TEXT("M4P1Neutral"));
		Candidate.bOpaqueStaticMesh = true;

		if (bSubmitLive)
		{
			Authority.SubmitObservation(Handle, MakeObservation(Registration, Candidate, 1, true));
		}
		if (bSubmitNonLive)
		{
			const FSightWeaveSubjectObservation Falling =
				MakeObservation(Registration, Candidate, 2, false, 1000 + Generation);
			if (bCustomValid)
			{
				const FM4P1AcceptedCustomProvider Provider(Candidate);
				Authority.SubmitObservation(Handle, Falling, &Provider);
			}
			else if (bCustomInvalid)
			{
				Authority.SubmitObservation(Handle, Falling, nullptr);
			}
			else
			{
				Authority.SubmitObservation(Handle, Falling);
			}
		}
		if (FStringView(Label) == TEXTVIEW("SW_M4P1_PrimaryTransition") && InState == 2)
		{
			Authority.SubmitObservation(Handle, MakeObservation(Registration, Candidate, 3, true));
		}
		if (bIdentityReuse)
		{
			FSightWeaveSubjectRegistration ReusedRegistration = Registration;
			ReusedRegistration.Identity.InstanceGeneration = 2;
			if (!Authority.Update(Handle, ReusedRegistration))
			{
				return false;
			}
			Registration = MoveTemp(ReusedRegistration);
		}

		const FSightWeaveLastSeenSnapshotDescriptor* Snapshot = Authority.FindSnapshot(Handle);
		if (bClear && Snapshot)
		{
			FSightWeaveMemoryRegion Region;
			Region.Scope = Registration.Scope;
			Region.HeightRange = { 0.0f, 300.0f };
			Region.Shape = ESightWeaveMemoryRegionShape::Circle;
			Region.Center = FVector2D(Location.X, Location.Y);
			Region.Radius = 700.0f;
			Authority.ClearSnapshots(Region);
			Snapshot = Authority.FindSnapshot(Handle);
		}
		FSightWeaveSubjectPresentationContext Context =
			MakeContext(Registration, Snapshot, bContextLive);
		Context.bSuppressMemoryPresentation = bSuppress;
		Context.bBlockMemoryWrites = bBlock;
		Context.bHardMemoryAtSnapshot = !bUnknown;
		if (bStaleRevision && Snapshot
			&& FStringView(Label) == TEXTVIEW("SW_M4P1_ScopeMismatch"))
		{
			Context.Scope.FloorOrigin.X += 25.0;
		}
		else if (bStaleRevision && Snapshot)
		{
			++Context.SnapshotRevision;
		}
		const FSightWeaveSubjectPresentationResult Presentation =
			Authority.EvaluatePresentation(Handle, Context);
		FSightWeaveSubjectProxyPresentationBridge::Apply(
			Presentation, Snapshot, Live, Proxy);
		VisibleLiveCount += Live->IsVisible() ? 1 : 0;
		VisibleProxyCount += Proxy->IsVisible() ? 1 : 0;
		return Proxy->HasRenderOnlyConfiguration();
	};

	bool bSuccess = true;
	// State: 0 Live, 1 Remembered, 2 Reacquired, 3 Suppressed+blocked,
	// 4 Cleared, 5 stable-id generation reuse (no stale proxy).
	const bool bPrimaryLive = InState == 0 || InState == 2;
	const bool bPrimarySuppressed = InState == 3;
	const bool bPrimaryCleared = InState == 4;
	const bool bPrimaryReuse = InState == 5;
	bSuccess &= AddSubject(
		TEXT("SW_M4P1_PrimaryTransition"), FVector(72000.0, 7500.0, 120.0), 0.0f,
		FVector(4.0, 4.0, 2.4), ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot,
		bPrimaryReuse ? 2 : 1, true, !bPrimaryLive || InState == 2, bPrimaryLive,
		bPrimarySuppressed, bPrimarySuppressed, false, bPrimaryCleared,
		bPrimaryReuse, false, false);
	if (bPrimaryLive)
	{
		AActor* Source = SpawnRootedActor(
			FixtureWorld,
			TEXT("SW_M4P1_PrimaryLiveSource"),
			FVector(72000.0, 7500.0, 100.0));
		bSuccess &= Source && AddVisionSource(Source, 650.0f, true);
		if (Source)
		{
			SubjectActors.Add(Source);
		}
	}

	// Always-on comparison matrix.
	bSuccess &= AddSubject(TEXT("SW_M4P1_LastSeenRemembered"), FVector(75500.0, 5900.0, 100.0),
		0.0f, FVector(2.6, 2.6, 2.0), ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot,
		11, true, true, false, false, false, false, false, false, false, false);
	bSuccess &= AddSubject(TEXT("SW_M4P1_NeverRemember"), FVector(77000.0, 5900.0, 100.0),
		0.0f, FVector(2.6, 2.6, 2.0), ESightWeaveSubjectMemoryPolicy::NeverRemember,
		12, true, true, false, false, false, false, false, false, false, false);
	bSuccess &= AddSubject(TEXT("SW_M4P1_VisibleOnly"), FVector(78500.0, 5900.0, 100.0),
		0.0f, FVector(2.6, 2.6, 2.0), ESightWeaveSubjectMemoryPolicy::VisibleOnly,
		13, true, true, false, false, false, false, false, false, false, false);
	bSuccess &= AddSubject(TEXT("SW_M4P1_StaticEnvironmentDelegated"), FVector(80000.0, 5900.0, 100.0),
		0.0f, FVector(2.6, 2.6, 2.0), ESightWeaveSubjectMemoryPolicy::StaticEnvironment,
		14, false, false, false, false, false, false, false, false, false, false);
	AActor* StaticCapture = SpawnRootedActor(
		FixtureWorld,
		TEXT("SW_M4P1_StaticEnvironmentCaptureOnce"),
		FVector(80000.0, 5900.0, 100.0));
	if (StaticCapture)
	{
		SubjectActors.Add(StaticCapture);
		StaticEnvironmentCaptureSource = AddVisionSource(StaticCapture, 420.0f, true);
		StaticEnvironmentSampleLocation = FVector(80000.0, 5900.0, 100.0);
	}
	bSuccess &= StaticEnvironmentCaptureSource.IsValid();
	bSuccess &= AddSubject(TEXT("SW_M4P1_CustomValid"), FVector(75500.0, 8200.0, 100.0),
		0.0f, FVector(2.6, 2.6, 2.0), ESightWeaveSubjectMemoryPolicy::Custom,
		15, true, true, false, false, false, false, false, false, true, false);
	bSuccess &= AddSubject(TEXT("SW_M4P1_CustomInvalid"), FVector(81500.0, 10000.0, 100.0),
		0.0f, FVector(2.6, 2.6, 2.0), ESightWeaveSubjectMemoryPolicy::Custom,
		16, true, true, false, false, false, false, false, false, false, true);
	bSuccess &= AddSubject(TEXT("SW_M4P1_Cleared"), FVector(78500.0, 8200.0, 100.0),
		0.0f, FVector(2.6, 2.6, 2.0), ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot,
		17, true, true, false, false, false, false, true, false, false, false);
	bSuccess &= AddSubject(TEXT("SW_M4P1_Suppressed"), FVector(80000.0, 8200.0, 100.0),
		0.0f, FVector(2.6, 2.6, 2.0), ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot,
		18, true, true, false, true, false, false, false, false, false, false);
	bSuccess &= AddSubject(TEXT("SW_M4P1_Blocked"), FVector(81500.0, 5900.0, 100.0),
		0.0f, FVector(2.6, 2.6, 2.0), ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot,
		19, true, true, false, false, true, false, false, false, false, false);
	bSuccess &= AddSubject(TEXT("SW_M4P1_Unknown"), FVector(81500.0, 8200.0, 100.0),
		0.0f, FVector(2.6, 2.6, 2.0), ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot,
		20, true, true, false, false, false, true, false, false, false, false);
	bSuccess &= AddSubject(TEXT("SW_M4P1_StaleRevision"), FVector(83000.0, 5900.0, 100.0),
		0.0f, FVector(2.6, 2.6, 2.0), ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot,
		21, true, true, false, false, false, false, false, true, false, false);
	bSuccess &= AddSubject(TEXT("SW_M4P1_ScopeMismatch"), FVector(84500.0, 5900.0, 100.0),
		0.0f, FVector(2.6, 2.6, 2.0), ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot,
		24, true, true, false, false, false, false, false, true, false, false);
	bSuccess &= AddSubject(TEXT("SW_M4P1_IdentityGenerationReuse"), FVector(84500.0, 8500.0, 100.0),
		0.0f, FVector(2.6, 2.6, 2.0), ESightWeaveSubjectMemoryPolicy::NeverRemember,
		25, true, true, false, false, false, false, false, false, false, false);
	bSuccess &= AddSubject(TEXT("SW_M4P1_Rotated45"), FVector(83000.0, 8800.0, 100.0),
		45.0f, FVector(4.2, 1.8, 2.0), ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot,
		22, true, true, false, false, false, false, false, false, false, false);
	bSuccess &= AddSubject(TEXT("SW_M4P1_PageBoundary"), FVector(150220.0, 9800.0, 100.0),
		0.0f, FVector(9.0, 2.4, 2.0), ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot,
		23, true, true, false, false, false, false, false, false, false, false);
	bSuccess &= AddSubject(TEXT("SW_M4P1_PageBoundarySuppressed"), FVector(148700.0, 8400.0, 100.0),
		0.0f, FVector(3.0, 3.0, 2.0), ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot,
		26, true, true, false, true, false, false, false, false, false, false);
	bSuccess &= AddSubject(TEXT("SW_M4P1_PageBoundaryCleared"), FVector(151700.0, 11200.0, 100.0),
		0.0f, FVector(3.0, 3.0, 2.0), ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot,
		27, true, true, false, false, false, false, true, false, false, false);

	auto AddLeakSentinel = [this, FixtureWorld, Cube, Material](
		const TCHAR* Label, const FVector& Location, const bool bAddLight)
	{
		AActor* Actor = SpawnRootedActor(FixtureWorld, Label, FVector::ZeroVector);
		if (!Actor)
		{
			return false;
		}
		SubjectActors.Add(Actor);
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Actor, TEXT("DynamicLeakMesh"));
		Actor->AddInstanceComponent(Mesh);
		Mesh->SetupAttachment(Actor->GetRootComponent());
		Mesh->SetStaticMesh(Cube);
		Mesh->SetMaterial(0, Material);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetWorldTransform(FTransform(FRotator::ZeroRotator, Location, FVector(2.4)));
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->RegisterComponent();
		if (bAddLight)
		{
			UPointLightComponent* Light =
				NewObject<UPointLightComponent>(Actor, TEXT("CurrentLightLeak"));
			Actor->AddInstanceComponent(Light);
			Light->SetupAttachment(Actor->GetRootComponent());
			Light->SetWorldLocation(Location + FVector(0.0, 0.0, 350.0));
			Light->SetIntensity(12000.0f);
			Light->SetLightColor(FLinearColor::Red);
			Light->RegisterComponent();
		}
		return true;
	};
	bSuccess &= AddLeakSentinel(
		TEXT("SW_M4P1_MovingMeshLeakSentinel"), FVector(77500.0, 6800.0, 120.0), false);
	bSuccess &= AddLeakSentinel(
		TEXT("SW_M4P1_CurrentLightLeakSentinel"), FVector(79000.0, 6800.0, 120.0), true);

	AppliedState = bSuccess ? InState : INDEX_NONE;
	return bSuccess;
}

void FSightWeaveM4P1LabFixture::DestroyActors(TArray<TWeakObjectPtr<AActor>>& Actors)
{
	for (const TWeakObjectPtr<AActor>& Actor : Actors)
	{
		if (Actor.IsValid())
		{
			Actor->Destroy();
		}
	}
	Actors.Reset();
}
