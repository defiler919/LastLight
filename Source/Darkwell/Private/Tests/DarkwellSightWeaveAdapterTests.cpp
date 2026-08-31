#if WITH_DEV_AUTOMATION_TESTS

#include "AI/DarkwellStalkerCharacter.h"
#include "Combat/DarkwellLoadoutComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "Gameplay/DarkwellVisibilityComponent.h"
#include "Misc/AutomationTest.h"
#include "Player/DarkwellCharacter.h"
#include "SightWeavePresentation.h"
#include "SightWeaveRenderWorldSubsystem.h"
#include "SightWeaveStaticEnvironment.h"
#include "SightWeaveWorldSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Visibility/DarkwellVisionIntegrationFixture.h"
#include "Visibility/SightWeave/DarkwellSightWeaveWorldSubsystem.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "VisionPresentation/DarkwellRememberablePropComponent.h"
#include "VisionPresentation/DarkwellRememberedPropSubsystem.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "VisionPresentation/DarkwellStalePropLabComponent.h"
#include "HAL/IConsoleManager.h"

namespace Darkwell::SightWeaveAdapterTests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	class FTestWorld final
	{
	public:
		explicit FTestWorld(const TCHAR* BaseName, UPackage* Outer = GetTransientPackage())
		{
			const FName WorldName = MakeUniqueObjectName(
				GetTransientPackage(),
				UWorld::StaticClass(),
				FName(BaseName));
			World = NewObject<UWorld>(Outer, WorldName, RF_Transient);
			if (!World || !GEngine)
			{
				return;
			}
			World->WorldType = EWorldType::Game;
			FWorldContext& Context = GEngine->CreateNewWorldContext(World->WorldType);
			Context.SetCurrentWorld(World);
			World->InitializeNewWorld(UWorld::InitializationValues()
				.InitializeScenes(false)
				.AllowAudioPlayback(false)
				.CreatePhysicsScene(false)
				.RequiresHitProxies(false)
				.CreateNavigation(false)
				.CreateAISystem(false)
				.ShouldSimulatePhysics(false)
				.SetTransactional(false));
		}

		~FTestWorld()
		{
			if (World && GEngine)
			{
				World->DestroyWorld(true);
				GEngine->DestroyWorldContext(World);
			}
		}

		UWorld* Get() const { return World; }

	private:
		UWorld* World = nullptr;
	};

	template <typename T>
	T* Spawn(UWorld& World, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		FActorSpawnParameters Parameters;
		Parameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		T* Actor = World.SpawnActor<T>(T::StaticClass(), Location, Rotation, Parameters);
		if (Actor)
		{
			Actor->DispatchBeginPlay();
		}
		return Actor;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellVisualRescuePresentationStateTest,
	"Darkwell.SightWeave.VisualRescue.PresentationState.TruthTable",
	Darkwell::SightWeaveAdapterTests::TestFlags)

bool FDarkwellVisualRescuePresentationStateTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("No live or memory resolves Unknown"),
		SightWeaveResolvePresentationState(false, false),
		ESightWeavePresentationState::Unknown);
	TestEqual(TEXT("Eligible memory resolves Remembered"),
		SightWeaveResolvePresentationState(false, true),
		ESightWeavePresentationState::Remembered);
	TestEqual(TEXT("Live without memory resolves Live"),
		SightWeaveResolvePresentationState(true, false),
		ESightWeavePresentationState::Live);
	TestEqual(TEXT("Live has strict precedence over Remembered"),
		SightWeaveResolvePresentationState(true, true),
		ESightWeavePresentationState::Live);
	TestNotEqual(TEXT("Static and occluder stencils are distinct"),
		SightWeave::RememberedScene::StaticEnvironmentStencilValue,
		SightWeave::RememberedScene::OccluderSurfaceStencilValue);
	TestNotEqual(TEXT("Static stencil does not collide with LastSeen proxy"),
		SightWeave::RememberedScene::StaticEnvironmentStencilValue, 246);
	TestNotEqual(TEXT("Occluder stencil does not collide with LastSeen proxy"),
		SightWeave::RememberedScene::OccluderSurfaceStencilValue, 246);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellM6P1DefaultAuthorityTest,
	"Darkwell.SightWeave.M6P1.Authority.DefaultLegacy",
	Darkwell::SightWeaveAdapterTests::TestFlags)

bool FDarkwellM6P1DefaultAuthorityTest::RunTest(const FString& Parameters)
{
	Darkwell::SightWeaveAdapterTests::FTestWorld World(TEXT("DarkwellM6P1Authority"));
	UDarkwellSightWeaveWorldSubsystem* Adapter = World.Get()
		? World.Get()->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>()
		: nullptr;
	if (!TestNotNull(TEXT("DARKWELL adapter exists"), Adapter))
	{
		return false;
	}

	const FDarkwellVisibilityAuthorityDiagnostics& Diagnostics = Adapter->GetDiagnostics();
	TestEqual(TEXT("Default authority is Legacy"),
		Adapter->GetAuthorityMode(), EDarkwellVisibilityAuthorityMode::Legacy);
	TestEqual(TEXT("Default state is Legacy"),
		Adapter->GetAuthorityState(), EDarkwellVisibilityAuthorityState::Legacy);
	TestFalse(TEXT("SightWeave is not an active consumer by default"),
		Adapter->IsSightWeaveAuthorityActive());
	TestTrue(TEXT("Legacy writes remain enabled"), Diagnostics.bLegacyWritesEnabled);
	TestTrue(TEXT("Legacy presentation remains enabled"),
		Diagnostics.bLegacyPresentationEnabled);
	TestFalse(TEXT("SightWeave presentation remains disabled"),
		Diagnostics.bSightWeavePresentationEnabled);
	TestTrue(TEXT("Runtime and non-server Render services resolve"),
		Adapter->HasRequiredSightWeaveServices());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellM6P1WorldIsolationTest,
	"Darkwell.SightWeave.M6P1.Lifecycle.MultiWorldIsolation",
	Darkwell::SightWeaveAdapterTests::TestFlags)

bool FDarkwellM6P1WorldIsolationTest::RunTest(const FString& Parameters)
{
	uint64 FirstGeneration = 0;
	{
		Darkwell::SightWeaveAdapterTests::FTestWorld First(TEXT("DarkwellM6P1WorldA"));
		UDarkwellSightWeaveWorldSubsystem* Adapter = First.Get()
			? First.Get()->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>()
			: nullptr;
		if (!TestNotNull(TEXT("First adapter exists"), Adapter))
		{
			return false;
		}
		FirstGeneration = Adapter->GetDiagnostics().WorldGeneration;
		TestTrue(TEXT("First world generation is nonzero"), FirstGeneration > 0);
	}

	Darkwell::SightWeaveAdapterTests::FTestWorld Second(TEXT("DarkwellM6P1WorldB"));
	UDarkwellSightWeaveWorldSubsystem* Restarted = Second.Get()
		? Second.Get()->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>()
		: nullptr;
	if (!TestNotNull(TEXT("Restarted adapter exists"), Restarted))
	{
		return false;
	}
	TestTrue(TEXT("Restart receives a distinct monotonic generation"),
		Restarted->GetDiagnostics().WorldGeneration > FirstGeneration);
	TestEqual(TEXT("Restart cannot inherit prior authority"),
		Restarted->GetAuthorityMode(), EDarkwellVisibilityAuthorityMode::Legacy);
	TestEqual(TEXT("Restart cannot inherit registrations"),
		Restarted->GetDiagnostics().VisionSourceCount
			+ Restarted->GetDiagnostics().IlluminationSourceCount
			+ Restarted->GetDiagnostics().SubjectCount,
		0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellM6P1VerticalSliceAuthorityTest,
	"Darkwell.SightWeave.M6P1.Integration.VerticalSliceAuthority",
	Darkwell::SightWeaveAdapterTests::TestFlags)

bool FDarkwellM6P1VerticalSliceAuthorityTest::RunTest(const FString& Parameters)
{
	using namespace Darkwell::SightWeaveAdapterTests;
	FTestWorld TestWorld(TEXT("DarkwellM6P1VerticalSlice"));
	UWorld* World = TestWorld.Get();
	if (!TestNotNull(TEXT("Test world exists"), World))
	{
		return false;
	}
	ADarkwellVisionIntegrationFixture* Fixture =
		Spawn<ADarkwellVisionIntegrationFixture>(*World, FVector::ZeroVector);
	ADarkwellCharacter* Player = Spawn<ADarkwellCharacter>(
		*World, FVector(-650.0, 0.0, 92.0), FRotator::ZeroRotator);
	ADarkwellStalkerCharacter* Stalker = Spawn<ADarkwellStalkerCharacter>(
		*World, FVector(550.0, 0.0, 92.0), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Fixture exists"), Fixture)
		|| !TestNotNull(TEXT("Player exists"), Player)
		|| !TestNotNull(TEXT("Stalker exists"), Stalker))
	{
		return false;
	}
	TInlineComponentArray<UStaticMeshComponent*> FixtureMeshes;
	Fixture->GetComponents(FixtureMeshes);
	int32 StaticSurfaceCount = 0;
	int32 OccluderSurfaceCount = 0;
	for (const UStaticMeshComponent* Mesh : FixtureMeshes)
	{
		if (!Mesh || !Mesh->bRenderCustomDepth)
		{
			continue;
		}
		StaticSurfaceCount += Mesh->CustomDepthStencilValue
			== SightWeave::RememberedScene::StaticEnvironmentStencilValue ? 1 : 0;
		OccluderSurfaceCount += Mesh->CustomDepthStencilValue
			== SightWeave::RememberedScene::OccluderSurfaceStencilValue ? 1 : 0;
	}
	TestEqual(TEXT("Ground and landmark are immutable Remembered surfaces"),
		StaticSurfaceCount, 2);
	TestEqual(TEXT("P2 proof walls are explicitly classified occluder surfaces"),
		OccluderSurfaceCount, 7);
	Stalker->ConfigurePersistentId(FName(TEXT("Enemy.Stalker.VisionIntegration")));

	UDarkwellSightWeaveWorldSubsystem* Adapter =
		World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
	USightWeaveWorldSubsystem* Runtime =
		World->GetSubsystem<USightWeaveWorldSubsystem>();
	USightWeaveRenderWorldSubsystem* Render =
		World->GetSubsystem<USightWeaveRenderWorldSubsystem>();
	UDarkwellFogVisualSubsystem* ProjectFog =
		World->GetSubsystem<UDarkwellFogVisualSubsystem>();
	if (!TestNotNull(TEXT("Adapter exists"), Adapter)
		|| !TestNotNull(TEXT("Runtime exists"), Runtime)
		|| !TestNotNull(TEXT("Render bridge exists"), Render)
		|| !TestNotNull(TEXT("DARKWELL fog presentation exists"), ProjectFog))
	{
		return false;
	}
	TestTrue(TEXT("Authority request is accepted"),
		Adapter->RequestSightWeaveAuthority(Fixture));
	TestTrue(TEXT("SightWeave becomes the only active authority"),
		Adapter->IsSightWeaveAuthorityActive());
	TestTrue(TEXT("Old SightWeave visual processing is suppressed"),
		Render->IsPresentationSuppressed());
	TestNull(TEXT("Rejected SightWeave SurfaceMaterial target is absent"),
		Render->GetSurfaceStateTexture());
	UTextureRenderTarget2D* LiveCoverage = ProjectFog->GetLiveCoverageTexture();
	TestNotNull(TEXT("Project continuous LiveCoverage target exists"), LiveCoverage);
	if (LiveCoverage)
	{
		TestEqual(TEXT("Continuous presentation target uses single-channel half float"),
			LiveCoverage->RenderTargetFormat, ETextureRenderTargetFormat::RTF_R16f);
		TestTrue(TEXT("Continuous presentation target is bilinear and clamped"),
			LiveCoverage->Filter == TextureFilter::TF_Bilinear
				&& LiveCoverage->AddressX == TextureAddress::TA_Clamp
				&& LiveCoverage->AddressY == TextureAddress::TA_Clamp);
	}
	TestTrue(TEXT("Project fog is active on the P4 fixture"),
		Fixture->IsDarkwellProjectFogEnabled());
	const UCameraComponent* PlayerCamera = Player->GetTopDownCamera();
	TestTrue(TEXT("Project fog locks exposure while active"),
		PlayerCamera
			&& PlayerCamera->PostProcessSettings.bOverride_AutoExposureMethod
			&& PlayerCamera->PostProcessSettings.AutoExposureMethod == AEM_Manual
			&& PlayerCamera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure
			&& !PlayerCamera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure);
	TestFalse(TEXT("Project coverage uses continuous P2 occlusion"),
		ProjectFog->GetDiagnostics().bP1NoOcclusion);
	TestTrue(TEXT("Project presentation enables object-local P3 surface coverage"),
		ProjectFog->GetDiagnostics().bP3SurfaceCoverage);
	TestTrue(TEXT("Project presentation enables P4 dynamic subjects"),
		ProjectFog->GetDiagnostics().bP4DynamicSubjects);
	TestEqual(TEXT("Project presentation caches all fixture occluder segments"),
		ProjectFog->GetDiagnostics().CachedOccluderSegmentCount, 11);
	TestEqual(TEXT("Project coverage remains Ultra-equivalent 2.5 cm per texel"),
		ProjectFog->GetMapping().CentimetersPerTexel, 2.5f);
	TestFalse(TEXT("Legacy visibility writes are disabled"),
		Player->GetVisibilityComponent()->IsVisibilityAuthorityEnabled());
	TestEqual(TEXT("Exactly one floor is registered"), Runtime->GetFloorCount(), 1);
	TestEqual(TEXT("Body and cone sources are registered"),
		Runtime->GetVisionSourceCount(), 2);
	TestEqual(TEXT("Only the legal torch light is registered"),
		Runtime->GetIlluminationSourceCount(), 1);
	FSightWeaveMemoryScopeKey RescueScope;
	TestTrue(TEXT("Visual-rescue memory scope is published"),
		Runtime->GetExplorationMemoryScope(RescueScope));
	TestEqual(TEXT("Live and Remembered use the Ultra 2.5 cm project scope"),
		RescueScope.PrecisionTier, ESightWeaveRenderPrecisionTier::Ultra);
	TestEqual(TEXT("One static occluder owner is registered"),
		Runtime->GetOccluderCount(), 1);
	TestTrue(TEXT("Duplicate request from the same fixture is idempotent"),
		Adapter->RequestSightWeaveAuthority(Fixture));
	TestEqual(TEXT("Idempotence does not duplicate vision handles"),
		Runtime->GetVisionSourceCount(), 2);

	FDarkwellVisibilitySubjectSnapshot Snapshot;
	TestTrue(TEXT("Doorway target has one authoritative subject snapshot"),
		Adapter->TryGetSubjectSnapshot(Stalker->GetPersistentId(), Snapshot));
	TestTrue(TEXT("Torch enables the directional-cone target"), Snapshot.bHardLive);
	TestFalse(TEXT("HardLive NeverRemember Stalker is rendered"), Stalker->IsHidden());
	TestEqual(TEXT("Stalker and HUD-facing snapshot share the same revision"),
		Stalker->GetAppliedVisibilityAuthorityRevision(), Snapshot.AuthorityRevision);

	const uint64 FailClosedBeforeToolCycle = Render->GetDiagnostics().FailClosedClearCount;
	TestTrue(TEXT("Lantern can replace the right-hand torch"),
		Player->GetLoadoutComponent()->EquipRightHandItem(
			DarkwellGameplayTags::Equipment_Right_Lantern));
	Adapter->Tick(0.0f);
	TestTrue(TEXT("Lantern state still produces an authoritative decision"),
		Adapter->TryGetSubjectSnapshot(Stalker->GetPersistentId(), Snapshot));
	TestFalse(TEXT("Rendered/non-authoritative lantern light cannot satisfy the cone"),
		Snapshot.bHardLive);
	TestTrue(TEXT("NeverRemember Stalker hides after losing HardLive"),
		Stalker->IsHidden());
	TestEqual(TEXT("An inactive compatible torch is an empty legal-light set, not an invalid scope"),
		Render->GetDiagnostics().FailClosedClearCount, FailClosedBeforeToolCycle);
	TestEqual(TEXT("Tool cycling preserves a valid render packet"),
		Render->GetDiagnostics().LastBuildFailure, ESightWeaveSparsePacketFailure::None);

	Stalker->SetActorLocation(FVector(-570.0, 0.0, 92.0), false, nullptr,
		ETeleportType::TeleportPhysics);
	Adapter->Tick(0.0f);
	TestTrue(TEXT("Body radius bypass remains visible without legal light"),
		Adapter->TryGetSubjectSnapshot(Stalker->GetPersistentId(), Snapshot)
			&& Snapshot.bHardLive);
	TestFalse(TEXT("Body-live NeverRemember Stalker is rendered"), Stalker->IsHidden());

	TestTrue(TEXT("Torch can be restored"),
		Player->GetLoadoutComponent()->EquipRightHandItem(
			DarkwellGameplayTags::Equipment_Right_Torch));
	Adapter->Tick(0.0f);
	TestEqual(TEXT("Restoring the torch keeps the render packet valid"),
		Render->GetDiagnostics().LastBuildFailure, ESightWeaveSparsePacketFailure::None);
	Player->SetActorLocation(FVector(-650.0, -400.0, 92.0), false, nullptr,
		ETeleportType::TeleportPhysics);
	Stalker->SetActorLocation(FVector(550.0, -400.0, 92.0), false, nullptr,
		ETeleportType::TeleportPhysics);
	Adapter->Tick(0.0f);
	TestTrue(TEXT("Occluded state still produces an authoritative decision"),
		Adapter->TryGetSubjectSnapshot(Stalker->GetPersistentId(), Snapshot));
	TestFalse(TEXT("Frozen wall segment occludes both body samples"), Snapshot.bHardLive);
	TestTrue(TEXT("Occluded NeverRemember Stalker is hidden"), Stalker->IsHidden());
	TestEqual(TEXT("Hidden actor and HUD predicate retain one revision"),
		Stalker->GetAppliedVisibilityAuthorityRevision(), Snapshot.AuthorityRevision);

	const FSightWeaveImmutableStaticEnvironmentPacketPtr StaticPacket =
		Runtime->AcquirePublishedStaticEnvironmentPacket();
	TestTrue(TEXT("Static-environment memory packet is valid"),
		StaticPacket.IsValid() && StaticPacket->IsValid());
	TestTrue(TEXT("Static-environment memory owns rasterized tiles"),
		StaticPacket.IsValid() && !StaticPacket->GetTiles().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellM6P1DuplicateFixtureRollbackTest,
	"Darkwell.SightWeave.M6P1.Lifecycle.DuplicateFixtureRollback",
	Darkwell::SightWeaveAdapterTests::TestFlags)

bool FDarkwellM6P1DuplicateFixtureRollbackTest::RunTest(const FString& Parameters)
{
	using namespace Darkwell::SightWeaveAdapterTests;
	FTestWorld TestWorld(TEXT("DarkwellM6P1DuplicateFixture"));
	UWorld* World = TestWorld.Get();
	ADarkwellVisionIntegrationFixture* First =
		Spawn<ADarkwellVisionIntegrationFixture>(*World, FVector::ZeroVector);
	ADarkwellVisionIntegrationFixture* Second =
		Spawn<ADarkwellVisionIntegrationFixture>(*World, FVector(0.0, 1500.0, 0.0));
	ADarkwellCharacter* Player = Spawn<ADarkwellCharacter>(
		*World, FVector(-650.0, 0.0, 92.0));
	ADarkwellStalkerCharacter* Stalker = Spawn<ADarkwellStalkerCharacter>(
		*World, FVector(550.0, 0.0, 92.0));
	Stalker->ConfigurePersistentId(FName(TEXT("Enemy.Stalker.Rollback")));
	const FPostProcessSettings OriginalCameraSettings =
		Player->GetTopDownCamera()->PostProcessSettings;
	UDarkwellSightWeaveWorldSubsystem* Adapter =
		World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
	USightWeaveWorldSubsystem* Runtime =
		World->GetSubsystem<USightWeaveWorldSubsystem>();
	if (!First || !Second || !Player || !Stalker || !Adapter || !Runtime)
	{
		AddError(TEXT("Duplicate-fixture test setup failed"));
		return false;
	}
	TestTrue(TEXT("First request activates"), Adapter->RequestSightWeaveAuthority(First));
	AddExpectedError(
		TEXT("SightWeave activation failed: A second integration fixture requested authority"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	TestFalse(TEXT("Second fixture is rejected"),
		Adapter->RequestSightWeaveAuthority(Second));
	TestEqual(TEXT("Failure returns active mode to Legacy"),
		Adapter->GetAuthorityMode(), EDarkwellVisibilityAuthorityMode::Legacy);
	TestEqual(TEXT("Duplicate request is diagnosable"),
		Adapter->GetAuthorityState(), EDarkwellVisibilityAuthorityState::SightWeaveFailed);
	TestEqual(TEXT("Rollback removes all floors"), Runtime->GetFloorCount(), 0);
	TestEqual(TEXT("Rollback removes all vision sources"),
		Runtime->GetVisionSourceCount(), 0);
	TestTrue(TEXT("Rollback restores Legacy writes"),
		Player->GetVisibilityComponent()->IsVisibilityAuthorityEnabled());
	TestEqual(TEXT("Rollback restores the player's exposure method"),
		Player->GetTopDownCamera()->PostProcessSettings.AutoExposureMethod,
		OriginalCameraSettings.AutoExposureMethod);
	TestEqual(TEXT("Rollback restores the exposure override flag"),
		Player->GetTopDownCamera()->PostProcessSettings.bOverride_AutoExposureMethod,
		OriginalCameraSettings.bOverride_AutoExposureMethod);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellSurfaceMaterialStateTruthTableTest,
	"Darkwell.SightWeave.VisualRescue.SurfaceMaterial.StateTruthTable",
	Darkwell::SightWeaveAdapterTests::TestFlags)

bool FDarkwellSurfaceMaterialStateTruthTableTest::RunTest(const FString& Parameters)
{
	const FDarkwellSightWeaveSurfaceWeights Unknown =
		FDarkwellSightWeaveSurfaceMath::ResolveWeights(0.0f, 0.0f, 1.0f, 0.0f);
	TestEqual(TEXT("Unknown has no Live weight"), Unknown.Live, 0.0f);
	TestEqual(TEXT("Unknown has no Remembered weight"), Unknown.Remembered, 0.0f);
	TestEqual(TEXT("Unknown is fully black"), Unknown.Unknown, 1.0f);

	const FDarkwellSightWeaveSurfaceWeights Remembered =
		FDarkwellSightWeaveSurfaceMath::ResolveWeights(1.0f, 0.0f, 1.0f, 0.0f);
	TestEqual(TEXT("Remembered is mutually exclusive from Live"), Remembered.Live, 0.0f);
	TestEqual(TEXT("Remembered retains the static surface"), Remembered.Remembered, 1.0f);
	TestEqual(TEXT("Remembered is mutually exclusive from Unknown"), Remembered.Unknown, 0.0f);

	const FDarkwellSightWeaveSurfaceWeights Live =
		FDarkwellSightWeaveSurfaceMath::ResolveWeights(1.0f, 1.0f, 1.0f, 0.0f);
	TestEqual(TEXT("Live wins over Remembered"), Live.Live, 1.0f);
	TestEqual(TEXT("Live suppresses Remembered"), Live.Remembered, 0.0f);
	TestEqual(TEXT("Live suppresses Unknown"), Live.Unknown, 0.0f);

	const FDarkwellSightWeaveSurfaceWeights InvalidScope =
		FDarkwellSightWeaveSurfaceMath::ResolveWeights(1.0f, 1.0f, 0.0f, 0.0f);
	TestEqual(TEXT("Invalid scope fails closed"), InvalidScope.Unknown, 1.0f);
	const FDarkwellSightWeaveSurfaceWeights NeverRemember =
		FDarkwellSightWeaveSurfaceMath::ResolveWeights(1.0f, 0.0f, 1.0f, 3.0f);
	TestEqual(TEXT("Category 3 never enters Remembered"), NeverRemember.Remembered, 0.0f);
	TestEqual(TEXT("Category 3 non-Live fails black"), NeverRemember.Unknown, 1.0f);
	const FDarkwellSightWeaveSurfaceWeights Continuous =
		FDarkwellSightWeaveSurfaceMath::ResolveWeights(0.6f, 0.25f, 1.0f, 0.0f);
	TestEqual(TEXT("Continuous Live coverage remains 0.25"), Continuous.Live, 0.25f);
	TestTrue(TEXT("Remembered is Known minus Live"),
		FMath::IsNearlyEqual(Continuous.Remembered, 0.35f, 1.0e-5f));
	TestTrue(TEXT("Unknown is one minus Known"),
		FMath::IsNearlyEqual(Continuous.Unknown, 0.4f, 1.0e-5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellSurfaceMaterialMappingAndWallTest,
	"Darkwell.SightWeave.VisualRescue.SurfaceMaterial.MappingAndWallSampling",
	Darkwell::SightWeaveAdapterTests::TestFlags)

bool FDarkwellSurfaceMaterialMappingAndWallTest::RunTest(const FString& Parameters)
{
	FSightWeaveSurfaceTextureMapping Mapping;
	Mapping.WorldMin = FVector2D(-1750.0, -1250.0);
	Mapping.WorldExtent = FVector2D(3500.0, 2500.0);
	Mapping.InvWorldExtent = FVector2D(1.0 / 3500.0, 1.0 / 2500.0);
	Mapping.TextureExtent = FIntPoint(1400, 1000);
	Mapping.CentimetersPerTexel = 2.5f;
	TestTrue(TEXT("Ultra mapping is valid"), Mapping.IsValid());
	TestTrue(TEXT("World minimum maps to UV zero"),
		Mapping.WorldToUV(Mapping.WorldMin).Equals(FVector2D::ZeroVector));
	TestTrue(TEXT("World maximum maps to UV one"),
		Mapping.WorldToUV(Mapping.WorldMin + Mapping.WorldExtent).Equals(FVector2D(1.0, 1.0)));
	const FVector2D SurfacePoint(100.0, 200.0);
	TestTrue(TEXT("+X face samples its own outward free space"),
		FDarkwellSightWeaveSurfaceMath::ResolveSurfaceSampleWorldPosition(
			SurfacePoint, FVector(1.0, 0.0, 0.0), 27.5f).Equals(FVector2D(127.5, 200.0)));
	TestTrue(TEXT("-X face samples its own outward free space"),
		FDarkwellSightWeaveSurfaceMath::ResolveSurfaceSampleWorldPosition(
			SurfacePoint, FVector(-1.0, 0.0, 0.0), 27.5f).Equals(FVector2D(72.5, 200.0)));
	TestTrue(TEXT("+Y face samples its own outward free space"),
		FDarkwellSightWeaveSurfaceMath::ResolveSurfaceSampleWorldPosition(
			SurfacePoint, FVector(0.0, 1.0, 0.0), 27.5f).Equals(FVector2D(100.0, 227.5)));
	TestTrue(TEXT("-Y face samples its own outward free space"),
		FDarkwellSightWeaveSurfaceMath::ResolveSurfaceSampleWorldPosition(
			SurfacePoint, FVector(0.0, -1.0, 0.0), 27.5f).Equals(FVector2D(100.0, 172.5)));
	TestTrue(TEXT("Floor and cube top keep the current world position"),
		FDarkwellSightWeaveSurfaceMath::ResolveSurfaceSampleWorldPosition(
			SurfacePoint, FVector(0.0, 0.0, 1.0), 27.5f).Equals(SurfacePoint));
	TestEqual(TEXT("Wall conservative bias remains frozen at 7.5 cm"),
		Darkwell::SightWeaveSurface::WallConservativeSampleBiasCentimeters,
		7.5f);
	TestEqual(TEXT("Fixture wall sample crosses its 20 cm half thickness plus the bias"),
		Darkwell::SightWeaveSurface::FixtureWallSampleDistanceCentimeters,
		27.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellSurfaceMaterialPrimitiveCategoryTest,
	"Darkwell.SightWeave.VisualRescue.SurfaceMaterial.PrimitiveCategories",
	Darkwell::SightWeaveAdapterTests::TestFlags)

bool FDarkwellSurfaceMaterialPrimitiveCategoryTest::RunTest(const FString& Parameters)
{
	using namespace Darkwell::SightWeaveAdapterTests;
	FTestWorld TestWorld(TEXT("DarkwellSurfacePrimitiveCategories"));
	ADarkwellVisionIntegrationFixture* Fixture = Spawn<ADarkwellVisionIntegrationFixture>(
		*TestWorld.Get(), FVector::ZeroVector);
	if (!Fixture)
	{
		AddError(TEXT("Could not create the integration fixture"));
		return false;
	}
	TInlineComponentArray<UStaticMeshComponent*> Meshes;
	Fixture->GetComponents(Meshes);
	int32 GroundCount = 0;
	int32 WallCount = 0;
	int32 StaticCount = 0;
	TArray<TPair<UStaticMeshComponent*, UMaterialInterface*>> InitialMaterials;
	for (const UStaticMeshComponent* Mesh : Meshes)
	{
		InitialMaterials.Emplace(
			const_cast<UStaticMeshComponent*>(Mesh),
			Mesh->GetMaterial(0));
		const TArray<float>& Data = Mesh->GetCustomPrimitiveData().Data;
		if (!Data.IsValidIndex(Darkwell::SightWeaveSurface::SurfaceCategoryCustomPrimitiveDataIndex))
		{
			continue;
		}
		const float Category = Data[Darkwell::SightWeaveSurface::SurfaceCategoryCustomPrimitiveDataIndex];
		GroundCount += Category == Darkwell::SightWeaveSurface::GroundCategory ? 1 : 0;
		WallCount += Category == Darkwell::SightWeaveSurface::WallOrCubeSideCategory ? 1 : 0;
		StaticCount += Category == Darkwell::SightWeaveSurface::RememberableStaticCategory ? 1 : 0;
	}
	TestEqual(TEXT("Exactly one ground primitive uses CPD[0]=0"), GroundCount, 1);
	TestEqual(TEXT("All seven P3 wall proof primitives use CPD[0]=1"), WallCount, 7);
	for (const UStaticMeshComponent* Mesh : Meshes)
	{
		const TArray<float>& Data = Mesh->GetCustomPrimitiveData().Data;
		if (Data.IsValidIndex(Darkwell::SightWeaveSurface::SurfaceCategoryCustomPrimitiveDataIndex)
			&& Data[Darkwell::SightWeaveSurface::SurfaceCategoryCustomPrimitiveDataIndex]
				== Darkwell::SightWeaveSurface::WallOrCubeSideCategory)
		{
			TestEqual(TEXT("Wall CPD[1] is unused; rendered geometry owns X direction"),
				Data[1], 0.0f);
			TestEqual(TEXT("Wall CPD[2] is unused; rendered geometry owns Y direction"),
				Data[2], 0.0f);
			TestEqual(TEXT("Wall CPD[3] crosses the fixture half thickness"),
				Data[Darkwell::SightWeaveSurface::WallSampleDistanceCustomPrimitiveDataIndex], 27.5f);
		}
	}
	TestEqual(TEXT("Landmark and movable proof prop use memory-eligible CPD[0]=2"),
		StaticCount, 2);
	Fixture->DisableSightWeaveSurfaceMaterial();
	for (const TPair<UStaticMeshComponent*, UMaterialInterface*>& Initial : InitialMaterials)
	{
		TestTrue(
			TEXT("Rollback before activation preserves the fixture material"),
			Initial.Key->GetMaterial(0) == Initial.Value);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellProjectFogRememberedPropRuntimeTest,
	"Darkwell.FogVisual.RememberedProp.RuntimeAtoB",
	Darkwell::SightWeaveAdapterTests::TestFlags)

bool FDarkwellProjectFogRememberedPropRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace Darkwell::SightWeaveAdapterTests;
	FTestWorld TestWorld(TEXT("DarkwellRememberedPropRuntime"));
	UWorld* World = TestWorld.Get();
	ADarkwellVisionIntegrationFixture* Fixture =
		Spawn<ADarkwellVisionIntegrationFixture>(*World, FVector::ZeroVector);
	ADarkwellCharacter* Player = Spawn<ADarkwellCharacter>(
		*World, FVector(900.0f, -700.0f, 92.0f));
	ADarkwellStalkerCharacter* Stalker = Spawn<ADarkwellStalkerCharacter>(
		*World, FVector::ZeroVector);
	Stalker->ConfigurePersistentId(FName(TEXT("Enemy.Stalker.MemoryIsolation")));
	UDarkwellSightWeaveWorldSubsystem* Adapter =
		World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
	UDarkwellRememberedPropSubsystem* Memory =
		World->GetSubsystem<UDarkwellRememberedPropSubsystem>();
	if (!Fixture || !Player || !Stalker || !Adapter || !Memory
		|| !Fixture->GetRememberablePropProof())
	{
		AddError(TEXT("RememberedProp runtime setup failed"));
		return false;
	}
	TestTrue(TEXT("Project authority activates"),
		Adapter->RequestSightWeaveAuthority(Fixture));
	Memory->RefreshNowForTesting();

	const FName StableId(TEXT("Fixture.RememberableProp.Cabinet"));
	bool bCurrentLive = false;
	bool bSnapshotValid = false;
	FVector SnapshotLocation = FVector::ZeroVector;
	AActor* Proxy = nullptr;
	TestTrue(TEXT("Stable remembered-prop record exists"),
		Memory->TryGetRecordForTesting(
			StableId, bCurrentLive, bSnapshotValid, SnapshotLocation, Proxy));
	TestTrue(TEXT("Seeing one admitted sample reveals the whole proof prop"),
		bCurrentLive && Fixture->GetRememberablePropProof()->IsVisible());

	if (Proxy)
	{
		TestFalse(TEXT("Memory proxy has no Actor collision"),
			Proxy->GetActorEnableCollision());
		TInlineComponentArray<UStaticMeshComponent*> ProxyMeshes(Proxy);
		for (const UStaticMeshComponent* Mesh : ProxyMeshes)
		{
			TestEqual(TEXT("Memory proxy mesh has no collision"),
				Mesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
			TestFalse(TEXT("Memory proxy mesh casts no shadow"), Mesh->CastShadow);
			TestFalse(TEXT("Memory proxy mesh contributes no dynamic GI"),
				Mesh->bAffectDynamicIndirectLighting);
			TestFalse(TEXT("Memory proxy mesh affects no distance-field lighting"),
				Mesh->bAffectDistanceFieldLighting);
			TestFalse(TEXT("Memory proxy mesh writes no CustomDepth"),
				Mesh->bRenderCustomDepth);
			TestFalse(TEXT("Memory proxy mesh never affects navigation"),
				Mesh->CanEverAffectNavigation());
		}
	}

	const FVector B(-1200.0f, 900.0f, 60.0f);
	Fixture->GetRememberablePropProof()->SetWorldLocation(B);
	Memory->RefreshNowForTesting();
	Memory->TryGetRecordForTesting(
		StableId, bCurrentLive, bSnapshotValid, SnapshotLocation, Proxy);
	TestFalse(TEXT("Out-of-sight B remains hidden"), bCurrentLive);
	TestFalse(TEXT("Observing empty A invalidates the entire old proxy"),
		bSnapshotValid);

	Player->SetActorLocation(FVector(B.X, B.Y, 92.0f), false, nullptr,
		ETeleportType::TeleportPhysics);
	Adapter->Tick(0.0f);
	Memory->RefreshNowForTesting();
	Memory->TryGetRecordForTesting(
		StableId, bCurrentLive, bSnapshotValid, SnapshotLocation, Proxy);
	TestTrue(TEXT("Same stable identity becomes wholly Live at B"), bCurrentLive);
	TestTrue(TEXT("The only snapshot moves to B"),
		bSnapshotValid && SnapshotLocation.Equals(FVector(B.X, B.Y, B.Z), 1.0f));

	Player->SetActorLocation(FVector::ZeroVector, false, nullptr,
		ETeleportType::TeleportPhysics);
	Adapter->Tick(0.0f);
	Memory->RefreshNowForTesting();
	Memory->TryGetRecordForTesting(
		StableId, bCurrentLive, bSnapshotValid, SnapshotLocation, Proxy);
	TestTrue(TEXT("Leaving B hides current and retains exactly one B proxy"),
		!bCurrentLive && bSnapshotValid && Proxy && !Proxy->IsHidden());
	TestEqual(TEXT("Only the fixture prop is registered; Stalker is NeverRemember"),
		Memory->GetDiagnostics().RegisteredCount, 1);
	TestEqual(TEXT("Exactly one remembered proxy is visible"),
		Memory->GetDiagnostics().ProxyCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellPropLabRuntimeMatrixTest,
	"Darkwell.PropLab.Runtime.SixCombinationsAndNeverRemember",
	Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellPropLabRuntimeMatrixTest::RunTest(const FString& Parameters)
{
	using namespace Darkwell::SightWeaveAdapterTests;
	FTestWorld TestWorld(TEXT("PropLabRuntime"), CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")));
	UWorld* World = TestWorld.Get();
	// This lightweight world has no InitializeActorsForPlay pass. Complete the
	// fixture lifecycle so Destroy routes EndPlay exactly as a real PIE world does.
	auto* Fixture = World->SpawnActor<ADarkwellPropGameplayLab>();
	Fixture->PostInitializeComponents();
	Fixture->DispatchBeginPlay();
	auto* Player = Spawn<ADarkwellCharacter>(*World, FVector(0,-650,92),FRotator(0,-90,0));
	auto* Stalker = Spawn<ADarkwellStalkerCharacter>(*World,FVector(0,-500,92));
	Stalker->ConfigurePersistentId(TEXT("Lab.Test.Stalker"));
	auto* Adapter = World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
	auto* Memory = World->GetSubsystem<UDarkwellRememberedPropSubsystem>();
	auto* Mode = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Darkwell.ProjectFogVisual.PropPresentationMode"));
	auto* Policy = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Darkwell.ProjectFogVisual.PropRelocationPolicy"));
	TestTrue(TEXT("Exact lab world activates the existing project adapter"),Adapter->RequestSightWeaveAuthority(Fixture));
	for(int32 M=0;M<3;++M) for(int32 R=0;R<2;++R)
	{
		Mode->Set(M,ECVF_SetByConsole); Policy->Set(R,ECVF_SetByConsole);
		const FName Id(*FString::Printf(TEXT("Lab.Test.M%d.P%d"),M,R));
		FTransform A(FVector(-200,350,0));
		auto* Prop=World->SpawnActorDeferred<ADarkwellPropLabFurniture>(ADarkwellPropLabFurniture::StaticClass(),A);
		Prop->StableId=Id; Prop->Shape=1; Prop->Dimensions=FVector(82,76,190);
		auto* LiveEffect=NewObject<USceneComponent>(Prop);
		LiveEffect->SetupAttachment(Prop->GetRootComponent()); LiveEffect->RegisterComponent();
		Prop->Memory->AddLiveOnlyComponent(LiveEffect);
		Prop->FinishSpawning(A); Prop->DispatchBeginPlay();
		auto Observe=[&](FVector Position,float Yaw)
		{
			Player->SetActorLocationAndRotation(Position,FRotator(0,Yaw,0));
			Adapter->Tick(0); Memory->RefreshNowForTesting(); Fixture->Tick(.016f);
		};
		Observe(FVector(0,-650,92),-90);
		bool KnownLive=false,KnownValid=false; FVector KnownLocation; AActor* KnownProxy=nullptr;
		Memory->TryGetRecordForTesting(Id,KnownLive,KnownValid,KnownLocation,KnownProxy);
		TestTrue(TEXT("Known geometry presentation does not grant identity Live"),!KnownLive && KnownValid);
		TestFalse(TEXT("Surface-only geometry does not grant source Live or reveal LiveOnly effects"),Prop->Memory->IsSourceLive() || LiveEffect->IsVisible());
		for(UStaticMeshComponent* Primitive : Prop->Memory->GetMemoryPrimitives())
			TestEqual(TEXT("Only sweep modes expose unchanged known geometry for per-pixel coverage"),Primitive->IsVisible(),M!=0);
		TestEqual(TEXT("Known complete silhouette has exactly one geometry representation"),KnownProxy && !KnownProxy->IsHidden(),M==0);
		Prop->SetActorLocation(FVector(650,340,0));
		Observe(FVector(0,-650,92),-90);
		for(UStaticMeshComponent* Primitive : Prop->Memory->GetMemoryPrimitives())
			TestFalse(TEXT("Unrecognized new transform never exposes source geometry"),Primitive->IsVisible());
		bool Live=false,Valid=false; FVector Location; AActor* Proxy=nullptr;
		Memory->TryGetRecordForTesting(Id,Live,Valid,Location,Proxy);
		TestTrue(TEXT("Unseen B stays hidden while entire A snapshot remains"),!Live && Valid && Proxy && !Proxy->IsHidden() && Location.X==-200);
		Observe(FVector(650,50,92),90);
		Memory->TryGetRecordForTesting(Id,Live,Valid,Location,Proxy);
		TestTrue(TEXT("Identity recognition shows complete source B"),Live && Valid && Location.X==650);
		TestEqual(TEXT("Only policy 0 retains A on B-first order"),Memory->GetUnverifiedSnapshotCount(Id),R==0 ? 1 : 0);
		for(UStaticMeshComponent* Primitive : Prop->Memory->GetMemoryPrimitives())
		{
			TestTrue(TEXT("Whole source geometry remains visible in every presentation mode"),Primitive->IsVisible());
			TestFalse(TEXT("Lab never writes CustomDepth"),Primitive->bRenderCustomDepth);
		}
		Observe(FVector(-250,50,92),90);
		TestEqual(TEXT("Verifying empty A retires its proxy under either policy"),Memory->GetUnverifiedSnapshotCount(Id),0);
		Prop->Destroy();
		Observe(FVector(650,50,92),90);
		Memory->TryGetRecordForTesting(Id,Live,Valid,Location,Proxy);
		TestFalse(TEXT("Observed empty B removes destroyed furniture memory"),Valid);
		// Presentation never influences enemy or HUD authority, including the legal-light cycle.
		for(const auto Tool : {DarkwellGameplayTags::Equipment_Right_Torch.GetTag(),DarkwellGameplayTags::Equipment_Right_Lantern.GetTag(),DarkwellGameplayTags::Equipment_Right_Torch.GetTag()})
		{
			Player->GetLoadoutComponent()->EquipRightHandItem(Tool);
			Observe(FVector(0,-650,92),90);
			FDarkwellVisibilitySubjectSnapshot Snapshot;
			TestTrue(TEXT("Stalker has one authoritative snapshot"),Adapter->TryGetSubjectSnapshot(Stalker->GetPersistentId(),Snapshot));
			TestEqual(TEXT("Only legal Torch reveals cone Stalker"),Snapshot.bHardLive,Tool==DarkwellGameplayTags::Equipment_Right_Torch);
			TestEqual(TEXT("Enemy presentation matches HardLive"),Stalker->IsVisibleBySightWeaveAuthority(),Snapshot.bHardLive);
			TestEqual(TEXT("HUD uses the same authority revision"),Stalker->GetAppliedVisibilityAuthorityRevision(),Snapshot.AuthorityRevision);
		}
		TestFalse(TEXT("NeverRemember enemy never enters furniture records"),Memory->TryGetRecordForTesting(Stalker->GetPersistentId(),Live,Valid,Location,Proxy));
		Observe(FVector(0,-650,92),-90);
	}
	// Similar appearance must not act as identity recognition across live records.
	Mode->Set(2,ECVF_SetByConsole); Policy->Set(1,ECVF_SetByConsole);
	auto MakeTwin=[&](FName Id,FVector Location)
	{
		const FTransform Transform(Location);
		auto* Twin=World->SpawnActorDeferred<ADarkwellPropLabFurniture>(ADarkwellPropLabFurniture::StaticClass(),Transform);
		Twin->StableId=Id; Twin->Shape=0; Twin->Dimensions=FVector(65,62,110);
		Twin->FinishSpawning(Transform); Twin->DispatchBeginPlay();
		return Twin;
	};
	auto* TwinA=MakeTwin(TEXT("Lab.Test.TwinA"),FVector(-200,350,0));
	auto* TwinB=MakeTwin(TEXT("Lab.Test.TwinB"),FVector(850,540,0));
	TwinA->SetActorLocation(FVector(-850,200,0));
	TwinB->SetActorLocation(FVector(650,340,0));
	Player->SetActorLocationAndRotation(FVector(650,50,92),FRotator(0,90,0));
	Adapter->Tick(0); Memory->RefreshNowForTesting();
	bool TwinLive=false,TwinValid=false; FVector TwinLocation; AActor* TwinProxy=nullptr;
	Memory->TryGetRecordForTesting(TEXT("Lab.Test.TwinB"),TwinLive,TwinValid,TwinLocation,TwinProxy);
	TestTrue(TEXT("Recognized twin B moves only its own StableID"),TwinLive && TwinValid && TwinLocation.X==650);
	Memory->TryGetRecordForTesting(TEXT("Lab.Test.TwinA"),TwinLive,TwinValid,TwinLocation,TwinProxy);
	TestTrue(TEXT("Similar visible twin cannot clear unseen twin A memory"),!TwinLive && TwinValid && TwinLocation.X==-200);
	Fixture->Destroy();
	TestEqual(TEXT("Leaving laboratory restores accepted presentation CVar"),Mode->GetInt(),0);
	TestEqual(TEXT("Leaving laboratory restores relocation CVar"),Policy->GetInt(),0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellPropLabEmptyAndComparisonTest,
 "Darkwell.PropLab.Comparison.EmptyWorldAndActorAuthority", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellPropLabEmptyAndComparisonTest::RunTest(const FString& Parameters)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("PropComparison"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")));
 UWorld* World=TestWorld.Get();
 auto* Fixture=World->SpawnActor<ADarkwellPropGameplayLab>(); Fixture->PostInitializeComponents(); Fixture->DispatchBeginPlay();
 auto* Player=Spawn<ADarkwellCharacter>(*World,Darkwell::PropLab::ComparisonPlayerPosition(),FRotator(0,-30,0));
 auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 auto* Memory=World->GetSubsystem<UDarkwellRememberedPropSubsystem>();
 auto* Fog=World->GetSubsystem<UDarkwellFogVisualSubsystem>();
 TestTrue(TEXT("Lab authority starts without a placed enemy"),Adapter->RequestSightWeaveAuthority(Fixture));
 Adapter->Tick(0);
 TestTrue(TEXT("Zero enemy does not roll back the laboratory"),Adapter->IsSightWeaveAuthorityActive());
 for(int32 Generation=0;Generation<2;++Generation)
 {
  auto* Enemy=Spawn<ADarkwellStalkerCharacter>(*World,FVector(400,-550,92)); Enemy->ConfigurePersistentId(TEXT("Lab.OptionalTest"));
  Adapter->Tick(0);
  FDarkwellVisibilitySubjectSnapshot Snapshot;
  TestTrue(TEXT("Explicit late enemy has NeverRemember authority"),Adapter->TryGetSubjectSnapshot(TEXT("Lab.OptionalTest"),Snapshot));
  bool Live=false,Valid=false; FVector Location; AActor* Proxy=nullptr;
  TestFalse(TEXT("Late enemy never becomes a furniture memory"),Memory->TryGetRecordForTesting(TEXT("Lab.OptionalTest"),Live,Valid,Location,Proxy));
  Enemy->Destroy(); Adapter->Tick(0);
  TestTrue(TEXT("Removing optional enemy preserves floor authority"),Adapter->IsSightWeaveAuthorityActive());
  TestFalse(TEXT("Removing enemy clears its HUD snapshot"),Adapter->TryGetSubjectSnapshot(TEXT("Lab.OptionalTest"),Snapshot));
 }
 const FTransform Transform(FVector(400,-300,0));
 auto* Island=World->SpawnActorDeferred<ADarkwellPropLabFurniture>(ADarkwellPropLabFurniture::StaticClass(),Transform);
 Island->StableId=TEXT("Lab.Island"); Island->Shape=4; Island->Dimensions=FVector(1400,110,90);
 Island->FinishSpawning(Transform); Island->DispatchBeginPlay();
 TestEqual(TEXT("Comparison island has exactly one primitive"),Island->Memory->GetMemoryPrimitives().Num(),1);
 auto* Mode=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Darkwell.ProjectFogVisual.PropPresentationMode"));
 for(int32 M=0;M<3;++M)
 {
  Mode->Set(M,ECVF_SetByConsole);
  int32 Intermediate=0,Gray=0,LiveFrames=0;
  for(int32 Step=0;Step<=300;++Step)
  {
   Player->SetActorRotation(FRotator(0,Darkwell::PropLab::ComparisonYaw(Step*.1f),0));
   Adapter->Tick(0); Memory->RefreshNowForTesting();
   bool Live=false,Valid=false; FVector Location; AActor* Proxy=nullptr;
   Memory->TryGetRecordForTesting(TEXT("Lab.Island"),Live,Valid,Location,Proxy);
   TestTrue(TEXT("Full silhouette remains known through forward/reverse scans"),Valid);
   TestEqual(TEXT("Source visibility is unified by identity, not component thresholds"),Island->Parts[0]->IsVisible(),M==0 ? Live : true);
   TestEqual(TEXT("Exactly one source or gray proxy represents the island"),Proxy && !Proxy->IsHidden(),M==0 && !Live);
   float Mean=0;
   for(int32 X=0;X<=100;++X) Mean+=Fog->EvaluateLiveCoverageAtWorldPoint(FVector2D(-300+14*X,-300));
   Mean/=101;
   Intermediate+=(Mean>.2f && Mean<.8f); Gray+=(Mean<.001f); LiveFrames+=Live;
  }
  TestTrue(TEXT("Route has sustained intermediate spatial coverage"),Intermediate>70);
  TestTrue(TEXT("Route contains fully gray endpoints and live identity samples"),Gray>30 && LiveFrames>50);
 }
 Fixture->Destroy();
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellStaleSnapshotOwnershipTest,
 "Darkwell.PropLab.Stale.RuntimeOldSnapshotOwnership", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellStaleSnapshotOwnershipTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("StaleOwnership"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")));
 UWorld* World=TestWorld.Get();
 auto* Fixture=World->SpawnActor<ADarkwellPropGameplayLab>(); Fixture->PostInitializeComponents(); Fixture->DispatchBeginPlay();
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(400,-700,92),FRotator(0,90,0));
 auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 auto* Memory=World->GetSubsystem<UDarkwellRememberedPropSubsystem>();
 TestTrue(TEXT("Existing Lab authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 for(int32 Event=0;Event<2;++Event)
 {
  IConsoleManager::Get().FindConsoleVariable(TEXT("r.Darkwell.ProjectFogVisual.PropPresentationMode"))->Set(Event+1,ECVF_SetByConsole);
  const FName Id(*FString::Printf(TEXT("Lab.Stale.Ownership%d"),Event));
  TestTrue(TEXT("Explicit lab subject opt-in"),Memory->SetLabVerificationSubject(Id));
  const FTransform A(FVector(400,-300,0));
  auto* Prop=World->SpawnActorDeferred<ADarkwellPropLabFurniture>(ADarkwellPropLabFurniture::StaticClass(),A);
  Prop->StableId=Id; Prop->Shape=4; Prop->Dimensions=FVector(600,100,90); Prop->Memory->bRememberFromStart=false;
  Prop->FinishSpawning(A); Prop->DispatchBeginPlay();
  Player->SetActorRotation(FRotator(0,90,0)); Adapter->Tick(0); Memory->RefreshNowForTesting();
  bool Live=false,Valid=false; FVector At; AActor* Proxy=nullptr;
  Memory->TryGetRecordForTesting(Id,Live,Valid,At,Proxy);
  TestTrue(TEXT("Snapshot requires actual initial observation"),Live && Valid && Proxy);
  const FVector RememberedA=At; // Observation origin is the primitive center, not actor floor Z.
  Player->SetActorRotation(FRotator(0,-90,0)); Adapter->Tick(0); Memory->RefreshNowForTesting();
  TestFalse(TEXT("Stale experiment initial gray uses identical whole-object proxy in every mode"),Prop->Parts[0]->IsVisible());
  TestNotNull(TEXT("Freeze previously observed A before offscreen event"),Memory->FreezeLabVerificationSnapshot());
  if(Event==0) Prop->Destroy(); else Prop->SetActorLocation(FVector(3000,3000,0));
  for(float Yaw:{-90.f,0.f,45.f,90.f,180.f,-90.f})
  {
   Player->SetActorRotation(FRotator(0,Yaw,0)); Adapter->Tick(0); Memory->RefreshNowForTesting();
   Memory->TryGetRecordForTesting(Id,Live,Valid,At,Proxy);
   TestTrue(TEXT(".50/max coverage cannot erase independently owned A"),Valid && Proxy && At.Equals(RememberedA));
   TestFalse(TEXT("Hidden B never becomes live"),Live);
  }
  Memory->FinishLabVerificationSnapshot();
  for(int32 I=0;I<10;++I) Memory->RefreshNowForTesting();
  Memory->TryGetRecordForTesting(Id,Live,Valid,At,Proxy);
  TestTrue(TEXT("Verified erasure cannot resurrect on ordinary refresh"),!Valid && !Proxy && !Live);
  if(Event==1) Prop->Destroy(); Memory->ReleaseLabVerificationSubject();
 }
 auto* Stale=Fixture->FindComponentByClass<UDarkwellStalePropLabComponent>();
 TestTrue(TEXT("Explicit stale route starts"),Stale && Stale->Start(1,2));
 TestEqual(TEXT("Existing PlayerController cursor guard sees a scripted Lab route"),
  IConsoleManager::Get().FindConsoleVariable(TEXT("r.Darkwell.ProjectFogVisual.LabRoute"))->GetInt(),14);
 if(Stale) Stale->Stop();
 TestEqual(TEXT("Stop restores manual Lab input"),
  IConsoleManager::Get().FindConsoleVariable(TEXT("r.Darkwell.ProjectFogVisual.LabRoute"))->GetInt(),0);
 Fixture->Destroy();
 FTestWorld Outside(TEXT("StaleOutside"));
 TestFalse(TEXT("Dedicated ownership cannot activate outside Lab"),Outside.Get()->GetSubsystem<UDarkwellRememberedPropSubsystem>()->SetLabVerificationSubject(TEXT("Forbidden")));
 return true;
}

#endif
