#if WITH_DEV_AUTOMATION_TESTS

#include "AI/DarkwellStalkerCharacter.h"
#include "Combat/DarkwellLoadoutComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "Gameplay/DarkwellVisibilityComponent.h"
#include "Interaction/DarkwellInteractionComponent.h"
#include "Misc/AutomationTest.h"
#include "Player/DarkwellCharacter.h"
#include "SightWeavePresentation.h"
#include "SightWeaveRenderWorldSubsystem.h"
#include "SightWeaveStaticEnvironment.h"
#include "SightWeaveWorldSubsystem.h"
#include "UObject/Package.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"
#include "Visibility/DarkwellVisionIntegrationFixture.h"
#include "Visibility/SightWeave/DarkwellSightWeaveWorldSubsystem.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "VisionPresentation/DarkwellRememberablePropComponent.h"
#include "VisionPresentation/DarkwellRememberedPropSubsystem.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "VisionPresentation/DarkwellStalePropLabComponent.h"
#include "VisionPresentation/DarkwellManualStaleRoom.h"
#include "VisionPresentation/DarkwellMovingPropLabRoom.h"
#include "HAL/IConsoleManager.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#if WITH_EDITOR
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionShadowReplace.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#endif

namespace Darkwell::SightWeaveAdapterTests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	class FTestWorld final
	{
	public:
		explicit FTestWorld(
			const TCHAR* BaseName,
			UPackage* Outer = GetTransientPackage(),
			const bool bCreatePhysicsScene = false)
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
				.InitializeScenes(bCreatePhysicsScene)
				.AllowAudioPlayback(false)
				.CreatePhysicsScene(bCreatePhysicsScene)
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
	"Darkwell.PropLab.Runtime.ThreeModesSpatialEvidenceAndNeverRemember",
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
	TestTrue(TEXT("Exact lab world activates the existing project adapter"),Adapter->RequestSightWeaveAuthority(Fixture));
	for(int32 M=0;M<3;++M)
	{
		Mode->Set(M,ECVF_SetByConsole);
		const FName Id(*FString::Printf(TEXT("Lab.Test.M%d"),M));
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
		TestEqual(TEXT("B-first observation retains A for spatial verification"),Memory->GetUnverifiedSnapshotCount(Id),1);
		for(UStaticMeshComponent* Primitive : Prop->Memory->GetMemoryPrimitives())
		{
			TestTrue(TEXT("Whole source geometry remains visible in every presentation mode"),Primitive->IsVisible());
			TestFalse(TEXT("Lab never writes CustomDepth"),Primitive->bRenderCustomDepth);
		}
		Observe(FVector(-250,50,92),90);
		TestEqual(TEXT("Only legal evidence at A retires its proxy"),Memory->GetUnverifiedSnapshotCount(Id),0);
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
	Mode->Set(2,ECVF_SetByConsole);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellManualSwitchCyclesTest,
 "Darkwell.PropLab.ManualSwitch.IsolationAndTenCycles", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellManualSwitchCyclesTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("ManualCycles"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")));
 UWorld* World=TestWorld.Get();
 auto* Room=Spawn<ADarkwellManualStaleRoom>(*World,FVector(4000,0,0));
 auto* Fixture=Spawn<ADarkwellPropGameplayLab>(*World,FVector::ZeroVector);
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(4500,150,92),FRotator(0,90,0));
 auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 auto* Memory=World->GetSubsystem<UDarkwellRememberedPropSubsystem>();
 TestTrue(TEXT("Manual room activates existing authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 TestTrue(TEXT("Explicit reset starts room"),Room->ResetRoom(Player));
 auto Step=[&](FVector P,float Yaw,int32 Count=6)
 {
  Player->SetActorLocation(P); Player->SetActorRotation(FRotator(0,Yaw,0));
  for(int32 I=0;I<Count;++I) { Adapter->Tick(1.f/30); Memory->RefreshNowForTesting(); Room->UpdateObservation(1.f/30,Player); }
 };
 bool Live=false,Valid=false; FVector At; AActor* Proxy=nullptr;
 Step(FVector(4500,150,92),90);
 Memory->TryGetRecordForTesting(Room->CabinetId(),Live,Valid,At,Proxy);
 TestTrue(TEXT("Reset does not invent memory: actual observation creates it"),Live && Valid && Proxy);
 const TWeakObjectPtr<AActor> OriginalSnapshot=Proxy;
 const FVector Press=Room->SwitchPosition()+FVector(0,0,92),Outside=Press+FVector(220,0,0);
 for(int32 Cycle=0;Cycle<10;++Cycle)
 {
  Step(Press,90);
  TestFalse(TEXT("One entry removes real cabinet"),Room->HasActualCabinet());
  TestEqual(TEXT("One trigger per entry"),Room->GetToggleCount(),Cycle*2+1);
  for(int32 Angle=0;Angle<360;Angle+=30)
  {
   Step(Press,float(Angle),3);
   TestEqual(TEXT("All sampled headings at switch have zero legal cabinet coverage"),Room->GetCabinetCoverage(),0.f);
  }
  TestEqual(TEXT("Dwelling cannot retrigger"),Room->GetToggleCount(),Cycle*2+1);
  for(int32 Mode:{2,0,1})
  {
   Room->Command({TEXT("stalemanual"),TEXT("mode"),FString::FromInt(Mode)});
   Memory->TryGetRecordForTesting(Room->CabinetId(),Live,Valid,At,Proxy);
   TestTrue(TEXT("Mode switch preserves absent actual and original snapshot"),!Room->HasActualCabinet() && Valid && Proxy==OriginalSnapshot.Get());
  }
  Step(Outside,90); TestTrue(TEXT("Exit rearms"),Room->IsSwitchArmed());
  Step(Press,90);
  TestTrue(TEXT("Next entry respawns actual at same stable identity"),Room->HasActualCabinet());
  Memory->TryGetRecordForTesting(Room->CabinetId(),Live,Valid,At,Proxy);
  TestTrue(TEXT("Unseen respawn preserves snapshot and never leaks source"),!Live && Valid && Proxy==OriginalSnapshot.Get());
  TestEqual(TEXT("No empty evidence behind partition"),Room->GetVerifiedFraction(),0.f);
  Step(Outside,90);
 }
 TestEqual(TEXT("Ten complete absence/presence cycles"),Room->GetToggleCount(),20);
 World->URL.AddOption(TEXT("PropLabOriginal"));
 TestNull(TEXT("Original layout opt-out preserves legacy fixtures/routes"),ADarkwellManualStaleRoom::FindActive(World));
 TestTrue(TEXT("Original bounds stay byte-for-byte values"),Fixture->GetSightWeaveFloorBounds()==FBox2D(FVector2D(-1250,-950),FVector2D(1250,950)));
 Fixture->Destroy();
 FTestWorld OutsideWorld(TEXT("ManualOutside"));
 TestNull(TEXT("No manual room outside Lab"),ADarkwellManualStaleRoom::FindActive(OutsideWorld.Get()));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellManualSwitchErasureTest,
 "Darkwell.PropLab.ManualSwitch.ErasureAndModeChanges", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellManualSwitchErasureTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("ManualErasure"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")));
 UWorld* World=TestWorld.Get();
 auto* Room=Spawn<ADarkwellManualStaleRoom>(*World,FVector(4000,0,0));
 auto* Fixture=Spawn<ADarkwellPropGameplayLab>(*World,FVector::ZeroVector);
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(4500,150,92),FRotator(0,90,0));
 auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 auto* Memory=World->GetSubsystem<UDarkwellRememberedPropSubsystem>();
 TestTrue(TEXT("Manual authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 auto Step=[&](FVector P,float Yaw,int32 Count=6)
 {
  Player->SetActorLocation(P); Player->SetActorRotation(FRotator(0,Yaw,0));
  for(int32 I=0;I<Count;++I) { Adapter->Tick(1.f/30); Memory->RefreshNowForTesting(); Room->UpdateObservation(1.f/30,Player); }
 };
 for(int32 InitialMode=0;InitialMode<3;++InitialMode)
 {
  Room->ResetRoom(Player); Room->Command({TEXT("stalemanual"),TEXT("mode"),FString::FromInt(InitialMode)});
  Step(FVector(4500,150,92),90);
  Step(Room->SwitchPosition()+FVector(0,0,92),90);
  bool Live=false,Valid=false; FVector At; AActor* Proxy=nullptr;
  Memory->TryGetRecordForTesting(Room->CabinetId(),Live,Valid,At,Proxy);
  TestTrue(TEXT("Unseen removal keeps full old snapshot"),Valid && Proxy && !Live);
  TestEqual(TEXT("No switch memory shortcut"),Room->GetRemainingOpacity(),1.f);
  bool bPartial=false;
  for(int32 Yaw=0;Yaw<=180;Yaw+=3)
  {
   Step(FVector(4500,150,92),float(Yaw));
   const float Fraction=Room->GetVerifiedFraction();
   if(Fraction>0 && Fraction<1)
   {
    bPartial=true;
    if(InitialMode==0) TestEqual(TEXT("Whole mode stays whole with partial evidence"),Room->GetRemainingOpacity(),1.f);
   }
  }
  TestTrue(TEXT("Free scan has a partial evidence interval"),bPartial);
  TestEqual(TEXT("Legal unoccluded scan verifies all occupancy"),Room->GetVerifiedFraction(),1.f);
  Memory->TryGetRecordForTesting(Room->CabinetId(),Live,Valid,At,Proxy);
  TestTrue(TEXT("All modes finish erasure by legal evidence"),!Valid && !Proxy);
  for(int32 Mode:{0,1,2,0})
  {
   Room->Command({TEXT("stalemanual"),TEXT("mode"),FString::FromInt(Mode)});
   Step(Room->SwitchPosition()+FVector(220,0,92),-90);
   Memory->TryGetRecordForTesting(Room->CabinetId(),Live,Valid,At,Proxy);
   TestTrue(TEXT("Cleared memory cannot resurrect on mode switch or leaving"),!Valid && !Proxy && !Room->HasActualCabinet());
  }
  Step(Room->SwitchPosition()+FVector(0,0,92),90);
  Memory->TryGetRecordForTesting(Room->CabinetId(),Live,Valid,At,Proxy);
  TestTrue(TEXT("Unobserved return after erasure creates no new snapshot"),Room->HasActualCabinet() && !Valid && !Live && !Proxy);
  Step(FVector(4500,150,92),90);
  Memory->TryGetRecordForTesting(Room->CabinetId(),Live,Valid,At,Proxy);
  TestTrue(TEXT("Legally observing present cabinet creates fresh memory"),Live && Valid && Proxy);
 }
 Fixture->Destroy(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellManualHiddenShadowTest,
 "Darkwell.PropLab.ManualSwitch.HiddenShadowSameGeometry", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellManualHiddenShadowTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("ManualHiddenShadow"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")));
 UWorld* World=TestWorld.Get();
 auto* Room=Spawn<ADarkwellManualStaleRoom>(*World,FVector(4000,0,0));
 auto* Fixture=Spawn<ADarkwellPropGameplayLab>(*World,FVector::ZeroVector);
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(4500,150,92),FRotator(0,90,0));
 auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 auto* Memory=World->GetSubsystem<UDarkwellRememberedPropSubsystem>();
 TestTrue(TEXT("Existing manual authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 auto Actual=[&]() -> ADarkwellPropLabFurniture*
 {
  for(TActorIterator<ADarkwellPropLabFurniture> It(World);It;++It)
   if(It->StableId==Room->CabinetId()) return *It;
  return nullptr;
 };
 auto Step=[&](FVector P,float Yaw)
 {
  Player->SetActorLocation(P); Player->SetActorRotation(FRotator(0,Yaw,0));
  for(int32 I=0;I<8;++I) { Adapter->Tick(1.f/30); Memory->RefreshNowForTesting(); Room->UpdateObservation(1.f/30,Player); }
 };
 struct FGeometry
 {
  UStaticMeshComponent* Component; UStaticMesh* Mesh;
  FTransform Transform; FBoxSphereBounds Bounds;
 };
 const FVector Top(4500,150,92),Press=Room->SwitchPosition()+FVector(0,0,92),Outside=Press+FVector(220,0,0);
 for(int32 Mode:{0,1,2})
 {
  Room->ResetRoom(Player); Room->Command({TEXT("stalemanual"),TEXT("mode"),FString::FromInt(Mode)});
  Step(Top,90);
  ADarkwellPropLabFurniture* First=Actual();
  if(!TestNotNull(TEXT("Initial actual"),First)) return false;
  TArray<FGeometry> Geometry;
  for(UStaticMeshComponent* Part:First->Memory->GetMemoryPrimitives())
   Geometry.Add({Part,Part->GetStaticMesh(),Part->GetComponentTransform(),Part->Bounds});
  const FTransform ActorTransform=First->GetActorTransform();
  const FVector Dimensions=First->Dimensions;
  for(int32 Cycle=0;Cycle<2;++Cycle)
  {
   bool Live=false,Valid=false; FVector At; AActor* Proxy=nullptr;
   Memory->TryGetRecordForTesting(Room->CabinetId(),Live,Valid,At,Proxy);
   TestTrue(TEXT("Real observation creates ordinary snapshot"),Live && Valid && Proxy);
   const TWeakObjectPtr<AActor> Snapshot=Proxy;
   const uint64 Revision=Memory->GetDiagnostics().SnapshotRevision;
   Step(Press,90);
   TestNull(TEXT("ABSENT has no actual actor or shadow source"),Actual());
   Memory->TryGetRecordForTesting(Room->CabinetId(),Live,Valid,At,Proxy);
   TestTrue(TEXT("ABSENT retains unverified memory"),!Live && Valid && Proxy==Snapshot.Get());
   if(Proxy)
   {
    TInlineComponentArray<UStaticMeshComponent*> Parts(Proxy);
    TestEqual(TEXT("Only the original three memory parts"),Parts.Num(),3);
    for(auto* Part:Parts) TestFalse(TEXT("Gray memory never casts a current real shadow"),Part->CastShadow);
   }
   Step(Outside,90); Step(Press,90);
   ADarkwellPropLabFurniture* Current=Actual();
   if(!TestNotNull(TEXT("PRESENT respawns original class"),Current)) return false;
   TestEqual(TEXT("StableID unchanged"),Current->Memory->GetStableId(),Room->CabinetId());
   TestTrue(TEXT("Actor transform and dimensions unchanged"),Current->GetActorTransform().Equals(ActorTransform,0) && Current->Dimensions==Dimensions);
   TInlineComponentArray<UStaticMeshComponent*> AllParts(Current);
   TestEqual(TEXT("No second geometry: same 12 native slots, 3 used"),AllParts.Num(),12);
   TestEqual(TEXT("Original three actual parts only"),Current->Memory->GetMemoryPrimitives().Num(),3);
   const uint64 Appearance=Current->Memory->ComputeAppearanceRevision();
   auto CheckParts=[&](bool bVisible)
   {
    int32 Casters=0,Index=0;
    for(UStaticMeshComponent* Part:Current->Memory->GetMemoryPrimitives())
    {
     const FGeometry& G=Geometry[Index++];
     TestEqual(TEXT("Legacy visibility or Mode 2 material submission (pixels tested separately)"),Part->IsVisible(),bVisible || Mode==2);
     TestTrue(TEXT("Native hidden shadow enabled on same actual component"),Part->CastShadow && Part->bCastHiddenShadow);
     TestTrue(TEXT("Mesh asset and world transform unchanged"),Part->GetStaticMesh()==G.Mesh && Part->GetComponentTransform().Equals(G.Transform,0));
     TestTrue(TEXT("Bounds origin, extent and radius unchanged"),Part->Bounds.Origin==G.Bounds.Origin && Part->Bounds.BoxExtent==G.Bounds.BoxExtent && Part->Bounds.SphereRadius==G.Bounds.SphereRadius);
     Casters+=Part->CastShadow && (Part->IsVisible() || Part->bCastHiddenShadow);
    }
    TestEqual(TEXT("Exactly three current shadow sources in both states"),Casters,3);
    for(auto* Part:AllParts)
     if(!Current->Memory->GetMemoryPrimitives().Contains(Part)) TestFalse(TEXT("Unused native slots cannot cast hidden shadows"),Part->bCastHiddenShadow);
   };
   CheckParts(false);
   Memory->TryGetRecordForTesting(Room->CabinetId(),Live,Valid,At,Proxy);
   TestTrue(TEXT("Hidden respawn does not reveal or replace snapshot"),!Live && Valid && Proxy==Snapshot.Get());
   TestEqual(TEXT("Hidden shadow causes no snapshot revision"),Memory->GetDiagnostics().SnapshotRevision,Revision);
   TestEqual(TEXT("Hidden shadow creates no empty evidence"),Room->GetVerifiedFraction(),0.f);
   TArray<UStaticMeshComponent*> HiddenParts; for(UStaticMeshComponent* Part:Current->Memory->GetMemoryPrimitives()) HiddenParts.Add(Part);
   Step(Top,90); CheckParts(true);
   int32 Index=0; for(UStaticMeshComponent* Part:Current->Memory->GetMemoryPrimitives()) TestEqual(TEXT("Visible transition reuses exact hidden component pointer"),Part,HiddenParts[Index++]);
   TestEqual(TEXT("Visibility/shadow transition leaves appearance revision unchanged"),Current->Memory->ComputeAppearanceRevision(),Appearance);
   Step(Outside,90); CheckParts(false); Step(Top,90);
  }
 }
 Fixture->Destroy(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellManualFixedRevealGeometryTest,
 "Darkwell.PropLab.ManualSwitch.FixedRevealGeometry", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellManualFixedRevealGeometryTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("ManualFixedReveal"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")));
 UWorld* World=TestWorld.Get();
 auto* Room=Spawn<ADarkwellManualStaleRoom>(*World,FVector(4000,0,0));
 auto* Fixture=Spawn<ADarkwellPropGameplayLab>(*World,FVector::ZeroVector);
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(4500,150,92),FRotator(0,90,0));
 auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 auto* Memory=World->GetSubsystem<UDarkwellRememberedPropSubsystem>();
 TestTrue(TEXT("Uses existing legal authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 Room->ResetRoom(Player); Room->Command({TEXT("stalemanual"),TEXT("mode"),TEXT("2")});
 ADarkwellPropLabFurniture* Cabinet=nullptr;
 for(TActorIterator<ADarkwellPropLabFurniture> It(World);It;++It) if(It->StableId==Room->CabinetId()) Cabinet=*It;
 if(!TestNotNull(TEXT("Original actual cabinet"),Cabinet)) return false;
 struct FPart
 {
  UStaticMeshComponent* Component; UStaticMesh* Mesh; UMaterialInterface* Material;
  FTransform Relative,World; FVector LocalMin,LocalMax; FBoxSphereBounds Bounds;
  TArray<FVector> Corners,Vertices;
 };
 auto Capture=[&]()
 {
  TArray<FPart> Result;
  for(UStaticMeshComponent* Part:Cabinet->Memory->GetMemoryPrimitives())
  {
   FPart P; P.Component=Part; P.Mesh=Part->GetStaticMesh(); P.Material=Part->GetMaterial(0);
   P.Relative=Part->GetRelativeTransform(); P.World=Part->GetComponentTransform(); P.Bounds=Part->Bounds;
   Part->GetLocalBounds(P.LocalMin,P.LocalMax);
   for(int32 I=0;I<8;++I) P.Corners.Add(P.World.TransformPosition(FVector(I&1?P.LocalMax.X:P.LocalMin.X,I&2?P.LocalMax.Y:P.LocalMin.Y,I&4?P.LocalMax.Z:P.LocalMin.Z)));
   const auto& VB=P.Mesh->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;
   for(uint32 I=0;I<VB.GetNumVertices();++I) P.Vertices.Add(P.World.TransformPosition(FVector(VB.VertexPosition(I))));
   Result.Add(MoveTemp(P));
  }
  return Result;
 };
 auto Matches=[](const TArray<FPart>& A,const TArray<FPart>& B)
 {
  if(A.Num()!=B.Num()) return false;
  for(int32 I=0;I<A.Num();++I)
  {
   const auto& X=A[I]; const auto& Y=B[I];
   if(X.Component!=Y.Component || X.Mesh!=Y.Mesh || X.Material!=Y.Material || !X.Relative.Equals(Y.Relative,0) || !X.World.Equals(Y.World,0)
    || X.LocalMin!=Y.LocalMin || X.LocalMax!=Y.LocalMax || X.Bounds.Origin!=Y.Bounds.Origin || X.Bounds.BoxExtent!=Y.Bounds.BoxExtent
    || X.Bounds.SphereRadius!=Y.Bounds.SphereRadius || X.Corners!=Y.Corners || X.Vertices!=Y.Vertices) return false;
  }
  return true;
 };
 const auto Baseline=Capture(); const FTransform Actor=Cabinet->GetActorTransform();
 const uint64 Appearance=Cabinet->Memory->ComputeAppearanceRevision();
 TestEqual(TEXT("Three original real parts"),Baseline.Num(),3);
 TInlineComponentArray<UPrimitiveComponent*> OriginalPrimitives(Cabinet);
 TestEqual(TEXT("Twelve existing native slots, no additional primitive type"),OriginalPrimitives.Num(),12);
 // Negative controls mutate captured DATA only, never any actor or geometry.
 auto Bad=Baseline; Bad[0].World.SetScale3D(FVector(.01,1,1));
 TestFalse(TEXT("Guard rejects thin plate / scale reconstruction"),Matches(Baseline,Bad));
 Bad=Baseline; Bad[0].Bounds.BoxExtent.X*=.25;
 TestFalse(TEXT("Guard rejects rebuilt smaller bounds"),Matches(Baseline,Bad));
 Bad=Baseline; Bad[0].Vertices[0].X+=1;
 TestFalse(TEXT("Guard rejects moved surface vertex"),Matches(Baseline,Bad));
 Bad=Baseline; Bad.Add(Baseline[0]);
 TestFalse(TEXT("Guard rejects auxiliary cabinet part"),Matches(Baseline,Bad));
 int32 Partial=0,Empty=0,Full=0; bool Bins[4]={false,false,false,false};
 TArray<FDarkwellSpatialPropMemory::FCell> PreviousCells;
 bool bSawPartialKnowledge=false,bSawRetainedGray=false;
 for(float Yaw=-30;Yaw<=210;Yaw+=.5f)
 {
  Player->SetActorRotation(FRotator(0,Yaw,0));
  Adapter->Tick(1.f/60); Memory->RefreshNowForTesting(); Fixture->Tick(1.f/60);
  // This isolated world has no PlayerController; Fixture cannot find a pawn.
  // Explicitly advance the SAME runtime observation entry with our test actor.
  Room->UpdateObservation(1.f/60,Player);
  const auto& Spatial=Room->GetSpatialStateForTesting();
  const auto Cells=Spatial.GetCells(); int32 Known=0;
  bool bMonotonic=true,bUnknownHidden=true,bNoHole=true,bDisjoint=true;
  for(int32 I=0;I<Cells.Num();++I)
  {
   const auto& C=Cells[I]; const auto P=Spatial.Presentation(I); Known+=C.DiscoveredPresent>0;
   if(PreviousCells.IsValidIndex(I))
   {
    bMonotonic&=C.DiscoveredPresent>=PreviousCells[I].DiscoveredPresent;
    bNoHole&=P.R>=PreviousCells[I].AppearanceBlend;
   }
   if(C.DiscoveredPresent==0) bUnknownHidden&=P.R==0 && P.B==0;
   bDisjoint&=P.R==0 || P.B==0;
   bSawRetainedGray|=C.DiscoveredPresent>0 && C.CurrentLegalCoverage==0 && P.R==1 && P.G==0;
  }
  bSawPartialKnowledge|=Known>0 && Known<Cells.Num()/4;
  TestTrue(TEXT("Real legal-field integration: per-cell D never shrinks on turn-away"),bMonotonic);
  TestTrue(TEXT("No whole gray snapshot leak into unknown cells"),bUnknownHidden);
  TestTrue(TEXT("Known source cannot become a floor hole on coverage exit"),bNoHole);
  TestTrue(TEXT("Source and existing proxy have disjoint per-cell ownership"),bDisjoint);
  PreviousCells=TArray<FDarkwellSpatialPropMemory::FCell>(Cells);
  auto* Fog=World->GetSubsystem<UDarkwellFogVisualSubsystem>();
  int32 Covered=0;
  for(int32 X=0;X<100;++X) Covered+=Fog->EvaluateLiveCoverageAtWorldPoint(FVector2D(4282+4.4*X,420))>=.99f;
  Partial+=Covered>0 && Covered<100; Empty+=Covered==0; Full+=Covered==100;
  Bins[0]|=Covered>0 && Covered<=5; Bins[1]|=Covered>=23 && Covered<=27;
  Bins[2]|=Covered>=48 && Covered<=52; Bins[3]|=Covered>=73 && Covered<=77;
  TestTrue(TEXT("Every scan frame: identity, local/world transforms, bounds, eight corners and ALL original vertices fixed"),Matches(Baseline,Capture()));
  TestTrue(TEXT("Actor transform exactly unchanged"),Cabinet->GetActorTransform().Equals(Actor,0));
  TestEqual(TEXT("Mask parameters never revise appearance/snapshot identity"),Cabinet->Memory->ComputeAppearanceRevision(),Appearance);
  TInlineComponentArray<UPrimitiveComponent*> Now(Cabinet);
  TestTrue(TEXT("No auxiliary renderable geometry, same component pointers"),TArray<UPrimitiveComponent*>(Now)==TArray<UPrimitiveComponent*>(OriginalPrimitives));
  int32 Casters=0;
  for(UStaticMeshComponent* Part:Cabinet->Memory->GetMemoryPrimitives())
  {
   TestTrue(TEXT("Original geometry submitted even before whole-object threshold; material clips pixels"),Part->IsVisible());
   Casters+=Part->CastShadow && Part->bCastHiddenShadow;
   auto* MID=Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0));
   TestTrue(TEXT("Dedicated spatial mask active"),MID && MID->K2_GetScalarParameterValue(TEXT("FixedRevealEnabled"))==1);
  }
  TestEqual(TEXT("Same three original shadow sources at every scan angle"),Casters,3);
 }
 TestTrue(TEXT("Scan exercises zero, partial and full LEGAL coverage"),Empty>0 && Partial>10 && Full>0);
 for(bool B:Bins) TestTrue(TEXT("Fixed corners/vertices checked at narrow, 25%, 50%, 75% legal surface coverage (sampling +/-2%)"),B);
 TestTrue(TEXT("First partial discovery does not mark the entire cabinet known"),bSawPartialKnowledge);
 TestTrue(TEXT("Exit ends on persistent gray original geometry, not an empty floor"),bSawRetainedGray);
 const FIntPoint AuthoritySize=Room->GetSpatialStateForTesting().GetSize();
 TestEqual(TEXT("AA uses four display samples per unchanged authority cell"),Room->GetSpatialPresentationSize(),AuthoritySize*4);
 TestTrue(TEXT("Dense conservative presentation field uses bilinear sampling"),Room->GetSpatialPresentationTextureForTesting()
  && Room->GetSpatialPresentationTextureForTesting()->Filter==TF_Bilinear);
 Fixture->Destroy(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellManualStaleCutCapTest,
 "Darkwell.PropLab.ManualSwitch.Mode2SymmetricDarkGrayCutCap", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellManualStaleCutCapTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 using namespace UE::Geometry;
 FTestWorld TestWorld(TEXT("ManualStaleCutCap"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")));
 UWorld* World=TestWorld.Get();
 auto* Room=Spawn<ADarkwellManualStaleRoom>(*World,FVector(4000,0,0));
 auto* Fixture=Spawn<ADarkwellPropGameplayLab>(*World,FVector::ZeroVector);
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(4500,150,92),FRotator(0,90,0));
 auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 auto* Memory=World->GetSubsystem<UDarkwellRememberedPropSubsystem>();
 TestTrue(TEXT("Uses existing legal authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 Room->ResetRoom(Player); Room->Command({TEXT("stalemanual"),TEXT("mode"),TEXT("2")});
 auto Step=[&](FVector P,float Yaw,int32 Frames=1)
 {
  Player->SetActorLocation(P); Player->SetActorRotation(FRotator(0,Yaw,0));
  for(int32 I=0;I<Frames;++I)
  { Adapter->Tick(1.f/30); Memory->RefreshNowForTesting(); Fixture->Tick(1.f/30); Room->UpdateObservation(1.f/30,Player); }
 };
 ADarkwellPropLabFurniture* Cabinet=nullptr;
 for(TActorIterator<ADarkwellPropLabFurniture> It(World);It;++It) if(It->StableId==Room->CabinetId()) Cabinet=*It;
 if(!TestNotNull(TEXT("Original actual cabinet"),Cabinet)) return false;
 TArray<FBox> PartBounds;
 for(UStaticMeshComponent* Part:Cabinet->Memory->GetMemoryPrimitives()) PartBounds.Add(Part->Bounds.GetBox());
 TestEqual(TEXT("Original three cabinet solids define cap intersections"),PartBounds.Num(),3);
 Step(FVector(4500,150,92),-30,2);
 TestEqual(TEXT("Zero present coverage has no premature cap"),Room->GetStaleCapTriangleCount(),0);
 bool bSawPresentPartial=false,bSawPresentCap=false;
 for(float Yaw=-30;Yaw<=210;Yaw+=.5f)
 {
  Step(FVector(4500,150,92),Yaw,1);
  const auto Cells=Room->GetSpatialStateForTesting().GetCells();
  int32 Known=0; for(const auto& C:Cells) Known+=C.DiscoveredPresent>0;
  if(Known<=0 || Known>=Cells.Num()) continue;
  bSawPresentPartial=true;
  if(Room->GetStaleCapTriangleCount()>0) { bSawPresentCap=true; break; }
 }
 TestTrue(TEXT("Partial legal present discovery occurs"),bSawPresentPartial);
 TestTrue(TEXT("Touching discovered/undiscovered present boundary produces cap triangles"),bSawPresentCap);
 TestTrue(TEXT("Present cap is the same non-authoritative component"),Room->GetStaleCapComponentForTesting()->IsVisible());
 TArray<FDarkwellSpatialPropMemory::FCell> BeforeCapModeSwitch;
 BeforeCapModeSwitch.Append(Room->GetSpatialStateForTesting().GetCells().GetData(),Room->GetSpatialStateForTesting().GetCells().Num());
 Room->Command({TEXT("stalemanual"),TEXT("mode"),TEXT("1")}); Room->UpdateObservation(0,Player);
 TestTrue(TEXT("Mode 1 clears present cap"),!Room->GetStaleCapComponentForTesting()->IsVisible() && Room->GetStaleCapTriangleCount()==0);
 const auto AfterCapModeSwitch=Room->GetSpatialStateForTesting().GetCells();
 bool bAuthorityUnchanged=BeforeCapModeSwitch.Num()==AfterCapModeSwitch.Num();
 for(int32 I=0;bAuthorityUnchanged && I<BeforeCapModeSwitch.Num();++I)
 {
  const auto& A=BeforeCapModeSwitch[I]; const auto& B=AfterCapModeSwitch[I];
  bAuthorityUnchanged=A.DiscoveredPresent==B.DiscoveredPresent && A.VerifiedEmpty==B.VerifiedEmpty
   && A.InitialRemembered==B.InitialRemembered && A.RemainingStale==B.RemainingStale;
 }
 TestTrue(TEXT("Cap and mode presentation never mutate D/V/R authority"),bAuthorityUnchanged);
 Room->Command({TEXT("stalemanual"),TEXT("mode"),TEXT("2")}); Room->UpdateObservation(0,Player);
 TestTrue(TEXT("Mode 2 rebuilds identical present authority boundary"),Room->GetStaleCapTriangleCount()>0);
 for(float Yaw=-30;Yaw<=210;Yaw+=1) Step(FVector(4500,150,92),Yaw,2);
 float Discovered=0; for(const auto& C:Room->GetSpatialStateForTesting().GetCells()) Discovered+=C.DiscoveredPresent;
 TestTrue(TEXT("Setup remembers nearly the full original surface"),Discovered>=Room->GetSpatialStateForTesting().GetCells().Num()*.95f);
 TestEqual(TEXT("Complete present discovery removes cap"),Room->GetStaleCapTriangleCount(),0);
 Step(Room->SwitchPosition()+FVector(0,0,92),90,4);
 TestFalse(TEXT("Pressure mutation makes actual cabinet absent"),Room->HasActualCabinet());

 bool bSawPartial=false,bSawCap=false,bCapVisibleValid=true,bNoCollisionValid=true,bNoOverlapValid=true,bNoShadowValid=true;
 bool bTriangleCountValid=true,bGridValid=true,bInsideOriginalValid=true;
 for(float Yaw=-30;Yaw<=210;Yaw+=.5f)
 {
  Step(FVector(4500,150,92),Yaw,1);
  const auto Cells=Room->GetSpatialStateForTesting().GetCells();
  int32 Verified=0,Remembered=0;
  for(const auto& C:Cells) { Verified+=C.InitialRemembered>0 && C.VerifiedEmpty>0; Remembered+=C.InitialRemembered>0; }
  const bool bPartial=Verified>0 && Verified<Remembered;
  if(!bPartial) continue;
  bSawPartial=true;
  const UDynamicMeshComponent* Cap=Room->GetStaleCapComponentForTesting();
  if(Room->GetStaleCapTriangleCount()==0) continue; // Disjoint remembered islands need no exposed section yet.
  bSawCap=true;
  bCapVisibleValid&=Cap && Cap->IsVisible();
  bNoCollisionValid&=Cap && Cap->GetCollisionEnabled()==ECollisionEnabled::NoCollision;
  bNoOverlapValid&=Cap && !Cap->GetGenerateOverlapEvents();
  bNoShadowValid&=Cap && !Cap->CastShadow;
  if(Cap) Cap->ProcessMesh([&](const FDynamicMesh3& Mesh)
  {
   bTriangleCountValid&=Mesh.TriangleCount()==Room->GetStaleCapTriangleCount();
   const FBox2D& Grid=Room->GetSpatialStateForTesting().GetBounds();
   const FVector2D Cell=Grid.GetSize()/FVector2D(Room->GetSpatialStateForTesting().GetSize());
   for(int32 VertexId:Mesh.VertexIndicesItr())
   {
    const FVector WorldVertex=FVector(Mesh.GetVertex(VertexId))+Room->GetActorLocation();
    const double GX=(WorldVertex.X-Grid.Min.X)/Cell.X,GY=(WorldVertex.Y-Grid.Min.Y)/Cell.Y;
    const bool bOnGrid=FMath::IsNearlyEqual(GX,FMath::RoundToDouble(GX),1e-3)
                      || FMath::IsNearlyEqual(GY,FMath::RoundToDouble(GY),1e-3);
    bool bInsideOriginal=false;
    for(const FBox& Box:PartBounds) bInsideOriginal|=Box.ExpandBy(.01).IsInsideOrOn(WorldVertex);
    bGridValid&=bOnGrid;
    bInsideOriginalValid&=bInsideOriginal;
   }
  });
  if(Room->GetStaleCapTriangleCount()>0) break;
 }
 TestTrue(TEXT("Partial legal empty verification produces a cut boundary"),bSawPartial);
 TestTrue(TEXT("A touching verified/retained boundary produces cap triangles"),bSawCap);
 TestTrue(TEXT("Cap component is visible only at a partial cut"),bCapVisibleValid);
 TestTrue(TEXT("Cap has no collision"),bNoCollisionValid);
 TestTrue(TEXT("Cap generates no overlap events"),bNoOverlapValid);
 TestTrue(TEXT("Cap casts no shadow"),bNoShadowValid);
 TestTrue(TEXT("Cap telemetry exactly matches its generated triangle mesh"),bTriangleCountValid);
 TestTrue(TEXT("Every cap vertex lies on the fixed authority grid"),bGridValid);
 TestTrue(TEXT("Every cap vertex is clipped to one of the original three solid bounds"),bInsideOriginalValid);
 Room->Command({TEXT("stalemanual"),TEXT("mode"),TEXT("1")}); Step(FVector(4500,150,92),90);
 TestTrue(TEXT("Mode 1 cannot retain the Mode 2 cap"),!Room->GetStaleCapComponentForTesting()->IsVisible() && Room->GetStaleCapTriangleCount()==0);
 Room->Command({TEXT("stalemanual"),TEXT("mode"),TEXT("2")}); Step(FVector(4500,150,92),90);
 TestTrue(TEXT("Returning Mode 2 rebuilds the same authority boundary without memory reset"),Room->GetStaleCapTriangleCount()>0);
 for(int32 Sweep=0;Sweep<3;++Sweep) for(float Yaw=-30;Yaw<=210;Yaw+=1) Step(FVector(4500,150,92),Yaw,4);
 TestEqual(TEXT("Complete erase removes every meaningless dark-gray cap"),Room->GetStaleCapTriangleCount(),0);
 TestFalse(TEXT("Complete erase hides cap component"),Room->GetStaleCapComponentForTesting()->IsVisible());
 Fixture->Destroy(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellMovingPropRoomRuntimeTest,
 "Darkwell.PropLab.MovingRules.Runtime.AtoBtoCAndMultiCounts", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellMovingPropRoomRuntimeTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("MovingPropRoom"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")));
 UWorld* World=TestWorld.Get();
 World->URL.AddOption(TEXT("PropLabOriginal"));
 World->URL.AddOption(TEXT("MoveRules"));
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(-1100,300,92),FRotator(0,90,0));
 auto* Fixture=World->SpawnActor<ADarkwellPropGameplayLab>(); Fixture->PostInitializeComponents(); Fixture->DispatchBeginPlay();
 auto* Room=ADarkwellMovingPropLabRoom::FindActive(World);
 auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 if(!TestNotNull(TEXT("MoveRules URL spawns the isolated native room"),Room) || !Player || !Adapter) return false;
 TestTrue(TEXT("Moving room activates ordinary SightWeave authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 TestTrue(TEXT("Reset establishes deterministic basic-geometry identities"),Room->ResetRoom(Player));
 auto Step=[&](int32 Frames=30)
 {
  for(int32 Frame=0;Frame<Frames;++Frame)
  { Adapter->Tick(1.f/30); Room->UpdateRoom(1.f/30,Player); Fixture->Tick(1.f/30); }
 };
 Step();
 int32 EnemyCount=0; for(TActorIterator<ADarkwellStalkerCharacter> It(World);It;++It) ++EnemyCount;
 TestEqual(TEXT("No placed or spawned enemy in moving room"),EnemyCount,0);
 TestEqual(TEXT("Cabinet, bed, table, lamp, thin panel and two twins are present"),Room->GetTrackedIdentityCount(),7);
 TMap<FName,int32> PrimitiveCounts;
 for(TActorIterator<ADarkwellPropLabFurniture> It(World);It;++It) if(It->bSpatialHistoryManaged)
 {
  TestTrue(TEXT("Spatial history proxy authority never registers a second StableID"),It->Memory->GetStableId().IsNone());
  TestFalse(TEXT("Duplicate actual StableID fails closed"),PrimitiveCounts.Contains(It->StableId));
  PrimitiveCounts.Add(It->StableId,It->Memory->GetMemoryPrimitives().Num());
 }
 TestEqual(TEXT("Cabinet is a three-part basic fixture"),PrimitiveCounts.FindRef(TEXT("Lab.Moving.Cabinet")),3);
 TestEqual(TEXT("Bed is a three-part basic fixture"),PrimitiveCounts.FindRef(TEXT("Lab.Moving.Bed")),3);
 TestEqual(TEXT("Table has top and four legs"),PrimitiveCounts.FindRef(TEXT("Lab.Moving.Table")),5);
 TestEqual(TEXT("Lamp uses base, thin stem and shade"),PrimitiveCounts.FindRef(TEXT("Lab.Moving.Lamp")),3);
 TestEqual(TEXT("Thin panel stays one thin primitive"),PrimitiveCounts.FindRef(TEXT("Lab.Moving.ThinPanel")),1);
 ADarkwellPropLabFurniture* Table=nullptr; ADarkwellPropLabFurniture* Lamp=nullptr;
 for(TActorIterator<ADarkwellPropLabFurniture> It(World);It;++It)
 {
  if(It->StableId==TEXT("Lab.Moving.Table")) Table=*It;
  if(It->StableId==TEXT("Lab.Moving.Lamp")) Lamp=*It;
 }
 FBox TableBounds(ForceInit),LampBounds(ForceInit); bool bPrimitiveIntersection=false;
 if(Table && Lamp)
 {
  for(const TObjectPtr<UStaticMeshComponent>& Part:Table->Memory->GetMemoryPrimitives()) TableBounds+=Part->Bounds.GetBox();
  for(const TObjectPtr<UStaticMeshComponent>& Part:Lamp->Memory->GetMemoryPrimitives()) LampBounds+=Part->Bounds.GetBox();
  for(const TObjectPtr<UStaticMeshComponent>& A:Table->Memory->GetMemoryPrimitives())
   for(const TObjectPtr<UStaticMeshComponent>& B:Lamp->Memory->GetMemoryPrimitives())
    bPrimitiveIntersection|=A->Bounds.GetBox().Intersect(B->Bounds.GetBox());
 }
 TestTrue(TEXT("Table and lamp intentionally exercise overlapping aggregate bounds"),TableBounds.Intersect(LampBounds));
 TestFalse(TEXT("Overlapping aggregate bounds use separated, non-coplanar primitives"),bPrimitiveIntersection);
 TestFalse(TEXT("Duplicate StableID is rejected without creating a second actor identity"),Room->TryDuplicateStableIdForTesting(TEXT("Lab.Moving.Cabinet")));

 TestTrue(TEXT("Select visible translation"),Room->SelectScenario(1,Player));
 bool bSawPresentCap=false;
 for(int32 Yaw=0;Yaw<=180 && !bSawPresentCap;Yaw+=2)
 {
  Player->SetActorRotation(FRotator(0,float(Yaw),0)); Step(1);
  bSawPresentCap=Room->GetTotalCapTriangles()>0;
 }
 TestTrue(TEXT("Partial PRESENT discovery uses the frozen symmetric dark-gray cap path"),bSawPresentCap);
 Player->SetActorRotation(FRotator(0,90,0)); Step();
 TestTrue(TEXT("Start deterministic in-view translation"),Room->AdvanceScenario(Player)); Step(130);
 TestEqual(TEXT("Observed translation rebases one epoch and leaves no proxy chain"),Room->GetSpatialRecordCount(TEXT("Lab.Moving.Cabinet")),1);
 TestEqual(TEXT("Observed translation leaves no historical proxy"),Room->GetTotalProxyCount(),0);

 TestTrue(TEXT("Select visible rotation"),Room->SelectScenario(2,Player)); Step();
 TestTrue(TEXT("Start observed 90 degree rotation"),Room->AdvanceScenario(Player)); Step(100);
 TestTrue(TEXT("Start observed 180 degree rotation"),Room->AdvanceScenario(Player)); Step(100);
 TestEqual(TEXT("Observed rotations keep one final observed pose"),Room->GetSpatialRecordCount(TEXT("Lab.Moving.Cabinet")),1);

 TestTrue(TEXT("Select coverage-boundary movement"),Room->SelectScenario(6,Player)); Step();
 TestTrue(TEXT("Start movement out of legal coverage"),Room->AdvanceScenario(Player)); Step(250);
 TestEqual(TEXT("Last legal pose freezes once hidden movement begins"),Room->GetSpatialRecordCount(TEXT("Lab.Moving.Cabinet")),1);
 TestTrue(TEXT("Explicitly observe the final real position"),Room->AdvanceScenario(Player)); Step();
 TestEqual(TEXT("Re-entry creates a new spatial epoch without hidden interpolation"),Room->GetSpatialRecordCount(TEXT("Lab.Moving.Cabinet")),2);

 TestTrue(TEXT("Select deterministic A-B-C scenario"),Room->SelectScenario(7,Player));
 Step();
 TestEqual(TEXT("A observation creates one record"),Room->GetSpatialRecordCount(TEXT("Lab.Moving.Cabinet")),1);
 TestTrue(TEXT("Hidden A-B command accepted"),Room->AdvanceScenario(Player)); Step(4);
 TestEqual(TEXT("Hidden move keeps A and reveals no B record"),Room->GetSpatialRecordCount(TEXT("Lab.Moving.Cabinet")),1);
 TestTrue(TEXT("Explicit B observation phase accepted"),Room->AdvanceScenario(Player)); Step();
 TestEqual(TEXT("Seeing B adds a second record without clearing A"),Room->GetSpatialRecordCount(TEXT("Lab.Moving.Cabinet")),2);
 TestTrue(TEXT("Hidden B-C command accepted"),Room->AdvanceScenario(Player)); Step(4);
 TestTrue(TEXT("Explicit C observation phase accepted"),Room->AdvanceScenario(Player)); Step();
 TestEqual(TEXT("Seeing C preserves independent A and B history"),Room->GetSpatialRecordCount(TEXT("Lab.Moving.Cabinet")),3);
 TestTrue(TEXT("A and B are rendered as independent no-authority proxies"),Room->GetTotalProxyCount()>=2);
 for(TActorIterator<AActor> It(World);It;++It) if(It->GetName().StartsWith(TEXT("SpatialMemory_")))
 {
  TestFalse(TEXT("Spatial memory proxy has no actor collision"),It->GetActorEnableCollision());
  TInlineComponentArray<UStaticMeshComponent*> Meshes(*It);
  for(const UStaticMeshComponent* Mesh:Meshes)
  { TestFalse(TEXT("Spatial memory proxy casts no duplicate shadow"),Mesh->CastShadow); TestEqual(TEXT("Spatial memory proxy mesh has no collision"),Mesh->GetCollisionEnabled(),ECollisionEnabled::NoCollision); }
 }
 TestTrue(TEXT("Explicit A verification phase accepted"),Room->AdvanceScenario(Player)); Step(40);
 TestEqual(TEXT("Legal evidence at A clears only A"),Room->GetSpatialRecordCount(TEXT("Lab.Moving.Cabinet")),2);
 TestTrue(TEXT("Explicit B verification phase accepted"),Room->AdvanceScenario(Player)); Step(40);
 TestEqual(TEXT("Legal evidence at B clears only B; current C remains"),Room->GetSpatialRecordCount(TEXT("Lab.Moving.Cabinet")),1);

 for(const int32 Count:{2,8,32})
 {
  TestTrue(TEXT("Supported deterministic multi count"),Room->SetMultiCount(Count,Player));
  TArray<ADarkwellPropLabFurniture*> MultiActors;
  for(TActorIterator<ADarkwellPropLabFurniture> It(World);It;++It) if(It->bSpatialHistoryManaged) MultiActors.Add(*It);
  MultiActors.Sort([](const ADarkwellPropLabFurniture& A,const ADarkwellPropLabFurniture& B){return A.StableId.LexicalLess(B.StableId);});
  for(ADarkwellPropLabFurniture* Actor:MultiActors)
  { const FVector P=Actor->GetActorLocation(); Player->SetActorLocationAndRotation(FVector(P.X,P.Y-300,92),FRotator(0,90,0)); Step(8); }
  TestEqual(TEXT("Every multi fixture has one unique real identity"),Room->GetTrackedIdentityCount(),Count);
  TSet<FName> StableIds;
  int32 Managed=0;
  for(TActorIterator<ADarkwellPropLabFurniture> It(World);It;++It) if(It->bSpatialHistoryManaged)
  { ++Managed; StableIds.Add(It->StableId); TestTrue(TEXT("Multi actual is excluded from ordinary single-snapshot registration"),It->Memory->GetStableId().IsNone()); }
  TestEqual(TEXT("No duplicate managed actors"),Managed,Count);
  TestEqual(TEXT("No duplicate StableID in 2/8/32 fixture"),StableIds.Num(),Count);
  for(int32 Index=0;Index<Count;++Index)
   TestEqual(TEXT("Each observed identical mesh owns an isolated spatial record"),Room->GetSpatialRecordCount(*FString::Printf(TEXT("Lab.Multi.%02d"),Index)),1);
  TestTrue(TEXT("Explicit multi mutation hides movement and removes one actual"),Room->AdvanceScenario(Player)); Step(4);
  TestTrue(TEXT("Moving item keeps its old record"),Room->GetSpatialRecordCount(TEXT("Lab.Multi.00"))==1);
  TestFalse(TEXT("ABSENT item has no rendering/collision occupancy"),Room->IsActualPresent(TEXT("Lab.Multi.01")));
  TestEqual(TEXT("ABSENT transition retains its own independent record"),Room->GetSpatialRecordCount(TEXT("Lab.Multi.01")),1);
  for(int32 Index=2;Index<Count;++Index)
   TestEqual(TEXT("Mutating two identities does not alter neighbors"),Room->GetSpatialRecordCount(*FString::Printf(TEXT("Lab.Multi.%02d"),Index)),1);
 }
 TestTrue(TEXT("HUD telemetry names the fixed rule"),Room->GetTelemetry().Contains(TEXT("SpatialEvidenceOnly")));
 Fixture->Destroy(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellMovingPropInWorldControlsTest,
 "Darkwell.PropLab.MovingRules.InWorldControls.NoConsoleContinuousMotion", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellMovingPropInWorldControlsTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("MovingPropInWorldControls"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")),true);
 UWorld* World=TestWorld.Get(); World->URL.AddOption(TEXT("PropLabOriginal")); World->URL.AddOption(TEXT("InWorldControls"));
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(-1100,80,92),FRotator(0,90,0));
 auto* Fixture=World->SpawnActor<ADarkwellPropGameplayLab>(); Fixture->PostInitializeComponents(); Fixture->DispatchBeginPlay();
 auto* Room=ADarkwellMovingPropLabRoom::FindActive(World);
 auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 if(!TestNotNull(TEXT("InWorldControls URL spawns moving room"),Room)||!Player||!Adapter)return false;
 TestTrue(TEXT("In-world room activates ordinary SightWeave authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 TestTrue(TEXT("In-world reset establishes controls and props"),Room->ResetRoom(Player));
 auto Step=[&](int32 Frames=30)
 {
  for(int32 Frame=0;Frame<Frames;++Frame)
  {Adapter->Tick(1.f/30);Room->UpdateRoom(1.f/30,Player);Fixture->Tick(1.f/30);}
 };
 auto Use=[&](EDarkwellMovingPropLabControlKind Kind)
 {
  auto* Control=Room->GetControlForTesting(Kind); if(!Control)return false;
  const FVector Center=Control->GetActorLocation();
	const float ApproachSign=Center.Y < -800.f ? 1.f : -1.f;
	const FVector Approach(Center.X,Center.Y+ApproachSign*190.f,92);
  FVector Facing=(Center-Approach).GetSafeNormal2D();
  Player->SetActorLocation(Approach);
  Player->SetActorRotation(Facing.Rotation()); Step(2);
  World->UpdateWorldComponents(true,false);
  Player->GetInteractionComponent()->UpdateFocusedActorFromWorld();
  if(Player->GetInteractionComponent()->GetFocusedActor()!=Control)return false;
  if(!Player->GetInteractionComponent()->GetFocusedPrompt().ToString().Contains(TEXT("F")))return false;
  return Player->GetInteractionComponent()->TryInteract();
 };
 auto UseReset=[&](){return Use(EDarkwellMovingPropLabControlKind::ResetCurrent);};
 Step();
 int32 EnemyCount=0;for(TActorIterator<ADarkwellStalkerCharacter> It(World);It;++It)++EnemyCount;
 TestEqual(TEXT("In-world room has no enemy"),EnemyCount,0);
 int32 ControlCount=0;for(TActorIterator<ADarkwellMovingPropLabControl> It(World);It;++It)++ControlCount;
 TestEqual(TEXT("Seven labeled F-key mechanisms exist"),ControlCount,7);

 auto* InitialTranslateControl=Room->GetControlForTesting(EDarkwellMovingPropLabControlKind::VisibleTranslate);
 if(TestNotNull(TEXT("Initial visible-translation mechanism exists"),InitialTranslateControl))
 {
  const FVector Center=InitialTranslateControl->GetActorLocation();
  Player->SetActorLocation(FVector(Center.X,Center.Y-190,92));
  Player->SetActorRotation(FRotator(0,90,0));Step(2);
  World->UpdateWorldComponents(true,false);
  Player->GetInteractionComponent()->UpdateFocusedActorFromWorld();
  TestEqual(TEXT("World proximity/visibility trace finds visible translation before F"),
   Player->GetInteractionComponent()->GetFocusedActor(),static_cast<AActor*>(InitialTranslateControl));
 }

 TestTrue(TEXT("In-world interactable starts visible translation without console"),Use(EDarkwellMovingPropLabControlKind::VisibleTranslate));
 TestEqual(TEXT("Translation selection starts at scenario 1 phase 0"),Room->GetScenario(),1);
 TestEqual(TEXT("One-second preparation is stopped"),Room->GetMotionState(),FString(TEXT("STOPPED")));
 const int32 BeforeFocus=Room->GetSpatialRecordCount(TEXT("Lab.Moving.Cabinet"));
 auto* TranslateControl=Room->GetControlForTesting(EDarkwellMovingPropLabControlKind::VisibleTranslate);
 if(TestNotNull(TEXT("Visible translation control remains available during focus change"),TranslateControl))
 {TranslateControl->OnInteractionFocusChanged(false);TranslateControl->OnInteractionFocusChanged(true);Step(2);}
 TestEqual(TEXT("Focus changes do not reset spatial history"),Room->GetSpatialRecordCount(TEXT("Lab.Moving.Cabinet")),BeforeFocus);
 TSet<int32> TranslateSamples;
 for(int32 Frame=0;Frame<165;++Frame)
 {Step(1);TranslateSamples.Add(FMath::RoundToInt(Room->GetTrackedTransform(TEXT("Lab.Moving.Cabinet")).GetLocation().X*10));}
 TestTrue(TEXT("Visible translation contains many intermediate transforms"),TranslateSamples.Num()>=80);
 TestEqual(TEXT("Visible translation finishes and holds B"),Room->GetMotionState(),FString(TEXT("FINISHED")));
 TestEqual(TEXT("Visible translation labels B"),Room->GetObjectPositionLabel(),FString(TEXT("B")));
 TestEqual(TEXT("Visible translation creates no residual chain"),Room->GetSpatialRecordCount(TEXT("Lab.Moving.Cabinet")),1);
 TestTrue(TEXT("Second F-key mechanism remains usable after translation completes"),Use(EDarkwellMovingPropLabControlKind::VisibleRotate));
 TSet<int32> RotationSamples;
	bool bRotationTexturesMatched = true;
	bool bRotationStayedSingleLive = true;
	bool bRotationContributorsExclusive = true;
 for(int32 Frame=0;Frame<165;++Frame)
 {Step(1);RotationSamples.Add(FMath::RoundToInt(Room->GetTrackedTransform(TEXT("Lab.InWorld.Rotate.Cabinet")).Rotator().Yaw*10));
  bRotationTexturesMatched &= Room->DoSpatialRecordTexturesMatchForTesting(TEXT("Lab.InWorld.Rotate.Cabinet"));
  bRotationStayedSingleLive &= Room->GetCurrentEpochCountForTesting(TEXT("Lab.InWorld.Rotate.Cabinet"))==1
   && Room->GetStaleEpochCountForTesting(TEXT("Lab.InWorld.Rotate.Cabinet"))==0
   && Room->GetVisibleHistoricalProxyCountForTesting(TEXT("Lab.InWorld.Rotate.Cabinet"))==0;
  bRotationContributorsExclusive &= Room->GetMaxOverlapContributorsForTesting(TEXT("Lab.InWorld.Rotate.Cabinet"))<=1;}
 TestTrue(TEXT("Visible rotation contains many intermediate angles"),RotationSamples.Num()>=80);
	TestTrue(TEXT("Rotating bounds always use a matching presentation texture extent"),bRotationTexturesMatched);
 TestTrue(TEXT("Continuously visible rotation remains one live epoch with no stale proxy"),bRotationStayedSingleLive);
 TestTrue(TEXT("Continuously visible rotation never has two contributors per world sample"),bRotationContributorsExclusive);
 TestEqual(TEXT("Continuously visible rotation has exactly one render contributor"),
  Room->GetMaxOverlapContributorsForTesting(TEXT("Lab.InWorld.Rotate.Cabinet")),1);
 TestEqual(TEXT("Visible rotation finishes without pose chain"),Room->GetSpatialRecordCount(TEXT("Lab.InWorld.Rotate.Cabinet")),1);
 TestTrue(TEXT("Third F-key mechanism starts after rotation without global reset"),Use(EDarkwellMovingPropLabControlKind::CoverageBoundary));
 TSet<int32> BoundarySamples;
 for(int32 Frame=0;Frame<285;++Frame)
 {Step(1);BoundarySamples.Add(FMath::RoundToInt(Room->GetTrackedTransform(TEXT("Lab.InWorld.Edge.Cabinet")).GetLocation().Y*10));}
 TestTrue(TEXT("Coverage-boundary motion contains many intermediate transforms"),BoundarySamples.Num()>=160);
 TestEqual(TEXT("Coverage-boundary motion finishes and holds B"),Room->GetMotionState(),FString(TEXT("FINISHED")));
 TestTrue(TEXT("Coverage-boundary movement creates no continuous residual chain"),
  Room->GetSpatialRecordCount(TEXT("Lab.InWorld.Edge.Cabinet"))<=1);
 TestTrue(TEXT("Fourth F-key mechanism arms offscreen A-to-B without global reset"),Use(EDarkwellMovingPropLabControlKind::HiddenAtoB));Step();
 const int32 HiddenARecords=Room->GetSpatialRecordCount(TEXT("Lab.InWorld.Hidden.Cabinet"));
 TestEqual(TEXT("A was observed before pressure trigger"),HiddenARecords,1);
 Player->SetActorLocation(Room->GetPressurePlatePosition()+FVector(0,0,92));Player->SetActorRotation(FRotator(0,-90,0));Step(3);
 TestEqual(TEXT("Pressure starts only after cabinet coverage is illegal"),Room->GetMotionState(),FString(TEXT("RUNNING")));
 TestEqual(TEXT("A epoch seals before the first hidden movement frame"),Room->GetHiddenFreezeCountForTesting(TEXT("Lab.InWorld.Hidden.Cabinet")),1);
 TSet<int32> HiddenMotionSamples;
 for(int32 Frame=0;Frame<130;++Frame)
 {Step(1);HiddenMotionSamples.Add(FMath::RoundToInt(Room->GetTrackedTransform(TEXT("Lab.InWorld.Hidden.Cabinet")).GetLocation().X*10));}
 TestTrue(TEXT("Offscreen A-to-B is continuous rather than a button teleport"),HiddenMotionSamples.Num()>=80);
 TestEqual(TEXT("Hidden motion retains A only before B is seen"),Room->GetSpatialRecordCount(TEXT("Lab.InWorld.Hidden.Cabinet")),1);
 Player->SetActorLocation(FVector(900,300,92));Player->SetActorRotation(FRotator(0,90,0));Step();
 TestEqual(TEXT("Seeing B adds B without identity-clearing A"),Room->GetSpatialRecordCount(TEXT("Lab.InWorld.Hidden.Cabinet")),2);
 Player->SetActorLocation(FVector(500,300,92));Player->SetActorRotation(FRotator(0,90,0));Step();
 TestEqual(TEXT("Only legal observation of A erases A"),Room->GetSpatialRecordCount(TEXT("Lab.InWorld.Hidden.Cabinet")),1);
 TestTrue(TEXT("Fifth F-key mechanism arms A-to-B-to-C without global reset"),Use(EDarkwellMovingPropLabControlKind::AtoBtoC));Step();
 Player->SetActorLocation(Room->GetPressurePlatePosition()+FVector(0,0,92));Player->SetActorRotation(FRotator(0,-90,0));Step(130);
 Player->SetActorLocation(FVector(-1050,700,92));Player->SetActorRotation(FRotator(0,90,0));Step();
 TestEqual(TEXT("ABC B observation preserves A"),Room->GetSpatialRecordCount(TEXT("Lab.InWorld.ABC.Cabinet")),2);
 Player->SetActorLocation(Room->GetPressurePlatePosition()+FVector(0,0,92));Player->SetActorRotation(FRotator(0,-90,0));Step(130);
 Player->SetActorLocation(FVector(-600,700,92));Player->SetActorRotation(FRotator(0,90,0));Step();
 TestEqual(TEXT("ABC C observation preserves independent A and B"),Room->GetSpatialRecordCount(TEXT("Lab.InWorld.ABC.Cabinet")),3);
 Player->SetActorLocation(FVector(-1500,700,92));Player->SetActorRotation(FRotator(0,90,0));Step();
 TestEqual(TEXT("ABC observing A erases A only"),Room->GetSpatialRecordCount(TEXT("Lab.InWorld.ABC.Cabinet")),2);
 Player->SetActorLocation(FVector(-1050,700,92));Player->SetActorRotation(FRotator(0,90,0));Step();
 TestEqual(TEXT("ABC observing B erases B only"),Room->GetSpatialRecordCount(TEXT("Lab.InWorld.ABC.Cabinet")),1);
 Player->SetActorLocation(FVector(-1200,-900,92));Player->SetActorRotation(FRotator(0,90,0));Step();
 const FTransform StaticTable=Room->GetTrackedTransform(TEXT("Lab.InWorld.Multi.LongTable"));
 TestTrue(TEXT("Sixth F-key mechanism starts multi-prop without global reset"),Use(EDarkwellMovingPropLabControlKind::MultiProp));
 TSet<int32> HighSamples,LowSamples;
 for(int32 Frame=0;Frame<165;++Frame)
 {Step(1);HighSamples.Add(FMath::RoundToInt(Room->GetTrackedTransform(TEXT("Lab.InWorld.Multi.HighCabinet")).GetLocation().X*10));LowSamples.Add(FMath::RoundToInt(Room->GetTrackedTransform(TEXT("Lab.InWorld.Multi.LowCabinet")).GetLocation().Y*10));}
 TestTrue(TEXT("High and low props move independently through intermediate transforms"),HighSamples.Num()>=80&&LowSamples.Num()>=80);
 TestFalse(TEXT("Small box becomes absent"),Room->IsActualPresent(TEXT("Lab.InWorld.Multi.SmallBox")));
 TestTrue(TEXT("Long table remains unchanged"),Room->GetTrackedTransform(TEXT("Lab.InWorld.Multi.LongTable")).Equals(StaticTable,0.01f));
 TestEqual(TEXT("High moving prop has no residual chain"),Room->GetSpatialRecordCount(TEXT("Lab.InWorld.Multi.HighCabinet")),1);
 TestTrue(TEXT("Low moving prop has no residual chain even when it began undiscovered"),
  Room->GetSpatialRecordCount(TEXT("Lab.InWorld.Multi.LowCabinet"))<=1);
 Player->SetActorLocation(FVector(-300,300,92));Player->SetActorRotation(FRotator(0,90,0));Step();
 const int32 UnrelatedRotationHistory=Room->GetSpatialRecordCount(TEXT("Lab.InWorld.Rotate.Cabinet"));
 TestTrue(TEXT("Unrelated rotation zone has retained evidence"),UnrelatedRotationHistory>0);
 TestTrue(TEXT("Seventh F-key mechanism is a real traced local reset"),UseReset());Step();
 TestEqual(TEXT("Reset returns scenario selection to zero"),Room->GetScenario(),0);
 TestEqual(TEXT("Reset current multi zone leaves unrelated evidence intact"),Room->GetSpatialRecordCount(TEXT("Lab.InWorld.Rotate.Cabinet")),UnrelatedRotationHistory);
 TestTrue(TEXT("Multi box is restored only by explicit zone reset"),Room->IsActualPresent(TEXT("Lab.InWorld.Multi.SmallBox")));

 Fixture->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellMovingPropVisibleRotationExclusionTest,
 "Darkwell.PropLab.MovingRules.InWorldControls.VisibleRotationExclusion", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellMovingPropVisibleRotationExclusionTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("MovingPropVisibleRotationExclusion"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")),true);
 UWorld* World=TestWorld.Get();World->URL.AddOption(TEXT("PropLabOriginal"));World->URL.AddOption(TEXT("InWorldControls"));
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(-300,70,92),FRotator(0,90,0));
 auto* Fixture=World->SpawnActor<ADarkwellPropGameplayLab>();Fixture->PostInitializeComponents();Fixture->DispatchBeginPlay();
 auto* Room=ADarkwellMovingPropLabRoom::FindActive(World);auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 if(!TestNotNull(TEXT("Visible-rotation Lab exists"),Room)||!Player||!Adapter)return false;
 TestTrue(TEXT("Visible-rotation Lab authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 TestTrue(TEXT("Visible-rotation Lab reset"),Room->ResetRoom(Player));
 auto Step=[&](int32 Frames=1){for(int32 Frame=0;Frame<Frames;++Frame)
  {Adapter->Tick(1.f/60);Room->UpdateRoom(1.f/60,Player);Fixture->Tick(1.f/60);}};
 auto UseRotate=[&]()
 {
  auto* Control=Room->GetControlForTesting(EDarkwellMovingPropLabControlKind::VisibleRotate);if(!Control)return false;
  const FVector Center=Control->GetActorLocation();Player->SetActorLocation(FVector(Center.X,Center.Y-190,92));
  Player->SetActorRotation(FRotator(0,90,0));Step(2);World->UpdateWorldComponents(true,false);
  Player->GetInteractionComponent()->UpdateFocusedActorFromWorld();
  return Player->GetInteractionComponent()->GetFocusedActor()==Control
   && Player->GetInteractionComponent()->TryInteract();
 };
 const FName RotateId(TEXT("Lab.InWorld.Rotate.Cabinet"));
 Step(60);TestTrue(TEXT("Traced F starts visible rotation"),UseRotate());
 TSet<int32> Samples;bool bSingleLive=true,bExclusive=true;
 for(int32 Frame=0;Frame<300;++Frame)
 {
  Step();Samples.Add(FMath::RoundToInt(Room->GetTrackedTransform(RotateId).Rotator().Yaw*10));
  bSingleLive &= Room->GetCurrentEpochCountForTesting(RotateId)==1
   && Room->GetStaleEpochCountForTesting(RotateId)==0
   && Room->GetVisibleHistoricalProxyCountForTesting(RotateId)==0;
  bExclusive &= Room->GetMaxOverlapContributorsForTesting(RotateId)<=1;
 }
 TestTrue(TEXT("0-to-180 has at least 80 distinct intermediate angles"),Samples.Num()>=80);
 TestTrue(TEXT("0-to-180 stays live epoch one with stale epoch zero"),bSingleLive);
 TestTrue(TEXT("0-to-180 contributor count never exceeds one"),bExclusive);
 TestEqual(TEXT("0-to-180 finishes with exactly one render contributor"),
  Room->GetMaxOverlapContributorsForTesting(RotateId),1);
 TestEqual(TEXT("0-to-180 finishes at 180"),
  static_cast<int32>(FMath::RoundToInt(FMath::Abs(Room->GetTrackedTransform(RotateId).Rotator().Yaw))),180);
 for(int32 Frame=0;Frame<600;++Frame){Step();bExclusive&=Room->GetMaxOverlapContributorsForTesting(RotateId)<=1;}
 TestEqual(TEXT("Ten-second fixed view creates zero stale proxies"),Room->GetVisibleHistoricalProxyCountForTesting(RotateId),0);
 TestTrue(TEXT("Ten-second fixed view remains overlap-free"),bExclusive);

 TestTrue(TEXT("Round two starts 180-to-0"),Room->StartTrackedRotationForTesting(RotateId,0,4));
 TSet<int32> ReverseSamples;
 for(int32 Frame=0;Frame<245;++Frame){Step();ReverseSamples.Add(FMath::RoundToInt(Room->GetTrackedTransform(RotateId).Rotator().Yaw*10));}
 TestTrue(TEXT("180-to-0 has at least 80 intermediate angles"),ReverseSamples.Num()>=80);
 TestEqual(TEXT("180-to-0 has no stale epoch"),Room->GetStaleEpochCountForTesting(RotateId),0);
 TestTrue(TEXT("Round three starts 0-to-90"),Room->StartTrackedRotationForTesting(RotateId,90,2));
 TSet<int32> FirstHalf;for(int32 Frame=0;Frame<125;++Frame){Step();FirstHalf.Add(FMath::RoundToInt(Room->GetTrackedTransform(RotateId).Rotator().Yaw*10));}
 TestTrue(TEXT("0-to-90 is continuous"),FirstHalf.Num()>=80);
 TestTrue(TEXT("Round three continues 90-to-180"),Room->StartTrackedRotationForTesting(RotateId,180,2));
 TSet<int32> SecondHalf;for(int32 Frame=0;Frame<125;++Frame){Step();SecondHalf.Add(FMath::RoundToInt(Room->GetTrackedTransform(RotateId).Rotator().Yaw*10));}
 TestTrue(TEXT("90-to-180 is continuous"),SecondHalf.Num()>=80);
 TestEqual(TEXT("Three visible rounds retain exactly one current epoch"),Room->GetCurrentEpochCountForTesting(RotateId),1);
 TestEqual(TEXT("Three visible rounds retain zero stale epochs"),Room->GetStaleEpochCountForTesting(RotateId),0);
 TestEqual(TEXT("Three visible rounds create zero stale proxies"),Room->GetHistoricalProxyCreationCountForTesting(RotateId),0);
 TestTrue(TEXT("Three visible rounds preserve one contributor maximum"),Room->GetMaxOverlapContributorsForTesting(RotateId)<=1);
 Fixture->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellMovingPropInvalidCoverageTest,
 "Darkwell.PropLab.MovingRules.InWorldControls.InvalidCoverageDoesNotSeal", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellMovingPropInvalidCoverageTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("MovingPropInvalidCoverage"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")),true);
 UWorld* World=TestWorld.Get();World->URL.AddOption(TEXT("PropLabOriginal"));World->URL.AddOption(TEXT("InWorldControls"));
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(-300,70,92),FRotator(0,90,0));
 auto* Fixture=World->SpawnActor<ADarkwellPropGameplayLab>();Fixture->PostInitializeComponents();Fixture->DispatchBeginPlay();
 auto* Room=ADarkwellMovingPropLabRoom::FindActive(World);auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 if(!TestNotNull(TEXT("Invalid-coverage Lab exists"),Room)||!Player||!Adapter)return false;
 TestTrue(TEXT("Invalid-coverage Lab authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 TestTrue(TEXT("Invalid-coverage Lab reset"),Room->ResetRoom(Player));
 auto Step=[&](int32 Frames=1){for(int32 Frame=0;Frame<Frames;++Frame)
  {Adapter->Tick(1.f/60);Room->UpdateRoom(1.f/60,Player);Fixture->Tick(1.f/60);}};
 auto* Control=Room->GetControlForTesting(EDarkwellMovingPropLabControlKind::VisibleRotate);
 if(!TestNotNull(TEXT("Invalid-coverage rotation control"),Control))return false;
 const FVector Center=Control->GetActorLocation();Player->SetActorLocation(FVector(Center.X,Center.Y-190,92));
 Player->SetActorRotation(FRotator(0,90,0));Step(62);World->UpdateWorldComponents(true,false);
 Player->GetInteractionComponent()->UpdateFocusedActorFromWorld();
 TestEqual(TEXT("Invalid-coverage route focuses the actual F control"),
  Player->GetInteractionComponent()->GetFocusedActor(),static_cast<AActor*>(Control));
 TestTrue(TEXT("F starts the continuously visible rotation"),Player->GetInteractionComponent()->TryInteract());
 Step(90);
 const FName RotateId(TEXT("Lab.InWorld.Rotate.Cabinet"));
 const int32 EpisodeBefore=Room->GetObservationEpisodeForTesting(RotateId);
 const int32 SealsBefore=Room->GetSealCountForTesting(RotateId);
 const int32 RecordsBefore=Room->GetSpatialRecordCount(RotateId);
 const FTransform TransformBefore=Room->GetTrackedTransform(RotateId);
 TestTrue(TEXT("Test hook marks exactly the next coverage sample invalid"),
  Room->InjectInvalidCoverageOnceForTesting(RotateId));
 Step();
 TestFalse(TEXT("Injected frame is reported invalid rather than legal zero"),
  Room->IsLastCoverageValidForTesting(RotateId));
 TestEqual(TEXT("Invalid reason remains explicit"),
  Room->GetLastCoverageZeroReasonForTesting(RotateId),FString(TEXT("TEST_INJECTED_INVALID")));
 TestEqual(TEXT("Invalid frame cannot seal an epoch"),Room->GetSealCountForTesting(RotateId),SealsBefore);
 TestEqual(TEXT("Invalid frame cannot change observation episode"),
  Room->GetObservationEpisodeForTesting(RotateId),EpisodeBefore);
 TestEqual(TEXT("Invalid frame cannot create a stale record"),Room->GetSpatialRecordCount(RotateId),RecordsBefore);
 TestEqual(TEXT("Invalid frame keeps one current epoch"),Room->GetCurrentEpochCountForTesting(RotateId),1);
 Step();
 TestTrue(TEXT("Next authoritative frame becomes valid again"),Room->IsLastCoverageValidForTesting(RotateId));
 TestEqual(TEXT("Recovery continues the same observation episode"),
  Room->GetObservationEpisodeForTesting(RotateId),EpisodeBefore);
 TestEqual(TEXT("Recovery still has no stale proxy"),Room->GetVisibleHistoricalProxyCountForTesting(RotateId),0);
 TestTrue(TEXT("Physical rotation continues across the invalid presentation frame"),
  !Room->GetTrackedTransform(RotateId).Equals(TransformBefore,0.001f));
 Fixture->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellMovingPropRotationOcclusionTest,
 "Darkwell.PropLab.MovingRules.InWorldControls.RotationOcclusionLastSeenPose", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellMovingPropRotationOcclusionTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("MovingPropRotationOcclusion"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")),true);
 UWorld* World=TestWorld.Get();World->URL.AddOption(TEXT("PropLabOriginal"));World->URL.AddOption(TEXT("InWorldControls"));
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(-300,70,92),FRotator(0,90,0));
 auto* Fixture=World->SpawnActor<ADarkwellPropGameplayLab>();Fixture->PostInitializeComponents();Fixture->DispatchBeginPlay();
 auto* Room=ADarkwellMovingPropLabRoom::FindActive(World);auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 if(!TestNotNull(TEXT("Occluded-rotation Lab exists"),Room)||!Player||!Adapter)return false;
 TestTrue(TEXT("Occluded-rotation Lab authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 TestTrue(TEXT("Occluded-rotation Lab reset"),Room->ResetRoom(Player));
 auto Step=[&](int32 Frames=1){for(int32 Frame=0;Frame<Frames;++Frame)
  {Adapter->Tick(1.f/60);Room->UpdateRoom(1.f/60,Player);Fixture->Tick(1.f/60);}};
 auto* Control=Room->GetControlForTesting(EDarkwellMovingPropLabControlKind::VisibleRotate);
 if(!TestNotNull(TEXT("Rotation control exists"),Control))return false;
 const FVector Center=Control->GetActorLocation();Player->SetActorLocation(FVector(Center.X,Center.Y-190,92));
 Player->SetActorRotation(FRotator(0,90,0));Step(62);World->UpdateWorldComponents(true,false);
 Player->GetInteractionComponent()->UpdateFocusedActorFromWorld();
 TestEqual(TEXT("Rotation trace focuses the real control"),Player->GetInteractionComponent()->GetFocusedActor(),static_cast<AActor*>(Control));
 TestTrue(TEXT("F starts rotation before occlusion"),Player->GetInteractionComponent()->TryInteract());
 Step(120);const FName RotateId(TEXT("Lab.InWorld.Rotate.Cabinet"));
 const float LastVisibleYaw=Room->GetTrackedTransform(RotateId).Rotator().Yaw;
 TestTrue(TEXT("Rotation visibly advances before coverage loss"),FMath::Abs(LastVisibleYaw)>5&&FMath::Abs(LastVisibleYaw)<120);
 Player->SetActorLocation(FVector(900,-850,92));Player->SetActorRotation(FRotator(0,-90,0));Step(3);
 TestEqual(TEXT("Coverage loss seals one last-seen stale epoch"),Room->GetHiddenFreezeCountForTesting(RotateId),1);
 const float FrozenYaw=Room->GetNewestHistoricalYawForTesting(RotateId);
 TestTrue(TEXT("Frozen pose is the last observed intermediate pose, not zero-degree start"),FMath::Abs(FrozenYaw)>5);
 TestTrue(TEXT("Frozen pose stays near the last legally observed frame"),FMath::Abs(FMath::FindDeltaAngleDegrees(FrozenYaw,LastVisibleYaw))<8);
 Step(240);TestEqual(TEXT("Hidden continuation creates one stale epoch"),Room->GetStaleEpochCountForTesting(RotateId),1);
 Player->SetActorLocation(FVector(-300,300,92));Player->SetActorRotation(FRotator(0,90,0));Step(120);
 TestEqual(TEXT("Returning legal view creates one current epoch"),Room->GetCurrentEpochCountForTesting(RotateId),1);
 TestEqual(TEXT("Old last-seen pose remains one independent stale epoch"),Room->GetStaleEpochCountForTesting(RotateId),1);
 bool bExclusive=true;for(int32 Frame=0;Frame<600;++Frame){Step();bExclusive&=Room->GetMaxOverlapContributorsForTesting(RotateId)<=1;}
 TestTrue(TEXT("Current and stale poses are spatially contribution-exclusive for ten seconds"),bExclusive);
 TestEqual(TEXT("Old proxy never whole-object flickers during fixed view"),Room->GetHistoricalProxyVisibilityTransitionsForTesting(RotateId),0);
 TestTrue(TEXT("Legally observed current pose can return to the old spatial pose"),
  Room->StartTrackedRotationForTesting(RotateId,FrozenYaw,2));
 Step(150);
 TestEqual(TEXT("A fully superseded stale surface has no visible proxy"),
  Room->GetVisibleHistoricalProxyCountForTesting(RotateId),0);
 TestEqual(TEXT("Superseded-by-current ownership never leaves a stale cap"),
  Room->GetVisibleHistoricalCapCountForTesting(RotateId),0);
 TestEqual(TEXT("A fully resolved epoch releases proxy, cap, texture and materials"),
  Room->GetHistoricalPresentationResourceCountForTesting(RotateId),0);
 Player->SetActorLocation(FVector(900,-850,92));Player->SetActorRotation(FRotator(0,-90,0));Step(60);
 TestEqual(TEXT("Retired historical presentation cannot reappear after view loss"),
  Room->GetHistoricalPresentationResourceCountForTesting(RotateId),0);
 AddInfo(Room->GetHistoricalVisualTelemetryForTesting(RotateId));
	Fixture->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellPartialRotatedStaleCapReproductionTest,
 "Darkwell.PropLab.MovingRules.InWorldControls.PartialRotatedStaleCapReproduction", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellPartialRotatedStaleCapReproductionTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("PartialRotatedStaleCapReproduction"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")),true);
 UWorld* World=TestWorld.Get();World->URL.AddOption(TEXT("PropLabOriginal"));World->URL.AddOption(TEXT("InWorldControls"));
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(-300,70,92),FRotator(0,90,0));
 auto* Fixture=World->SpawnActor<ADarkwellPropGameplayLab>();Fixture->PostInitializeComponents();Fixture->DispatchBeginPlay();
 auto* Room=ADarkwellMovingPropLabRoom::FindActive(World);auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 if(!TestNotNull(TEXT("Partial-rotation Lab exists"),Room)||!Player||!Adapter)return false;
 TestTrue(TEXT("Partial-rotation Lab authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 TestTrue(TEXT("Partial-rotation Lab reset"),Room->ResetRoom(Player));
 auto Step=[&](int32 Frames=1){for(int32 Frame=0;Frame<Frames;++Frame)
  {Adapter->Tick(1.f/60);Room->UpdateRoom(1.f/60,Player);Fixture->Tick(1.f/60);}};
 const FName RotateId(TEXT("Lab.InWorld.Rotate.Cabinet"));
 auto* Control=Room->GetControlForTesting(EDarkwellMovingPropLabControlKind::VisibleRotate);
 if(!TestNotNull(TEXT("Partial-rotation F control exists"),Control))return false;

 // A. Fully observe the zero-degree cabinet through ordinary project coverage.
 Player->SetActorLocation(FVector(-300,70,92));Player->SetActorRotation(FRotator(0,90,0));Step(90);
 TestEqual(TEXT("Initial pose is completely inside legal coverage"),Room->GetLastLegalCoverageRatioForTesting(RotateId),1.0f);
 World->UpdateWorldComponents(true,false);Player->GetInteractionComponent()->UpdateFocusedActorFromWorld();
 TestEqual(TEXT("Real F trace focuses VISIBLE ROTATE"),Player->GetInteractionComponent()->GetFocusedActor(),static_cast<AActor*>(Control));
 TestTrue(TEXT("Real F interaction arms VISIBLE ROTATE"),Player->GetInteractionComponent()->TryInteract());

 // B/C. Turn away during the one-second preparation and let rotation begin hidden.
 Player->SetActorRotation(FRotator(0,-90,0));Step(130);
 TestEqual(TEXT("Initial full observation seals exactly once when hidden motion begins"),Room->GetSealCountForTesting(RotateId),1);
 const float HiddenYaw=FMath::Abs(Room->GetTrackedTransform(RotateId).Rotator().Yaw);
 TestTrue(TEXT("Hidden rotation reaches a real intermediate angle"),HiddenYaw>=40.0f&&HiddenYaw<=120.0f);

 // D. Sweep only the cone edge across the rotating cabinet. No coverage value or
 // memory cell is injected; the ordinary SightWeave/project-fog path must find a
 // legal but deliberately partial observation.
 bool bFoundPartial=false;
 float PartialRatio=0.0f;
 for(int32 Yaw=150;Yaw>=135&&!bFoundPartial;--Yaw)
 {
  Player->SetActorRotation(FRotator(0,float(Yaw),0));Step(2);
  PartialRatio=Room->GetLastLegalCoverageRatioForTesting(RotateId);
  bFoundPartial=PartialRatio>=0.08f&&PartialRatio<0.35f;
 }
 TestTrue(TEXT("Cone-edge sweep produces genuine partial legal coverage"),bFoundPartial);
 TestTrue(TEXT("Partial reacquire occurs while the cabinet is still rotating"),Room->GetMotionState()==TEXT("RUNNING"));
 Step(4);
 Player->SetActorRotation(FRotator(0,-90,0));Step(3);
 TestEqual(TEXT("Turning away after partial reacquire seals one intermediate epoch"),Room->GetSealCountForTesting(RotateId),2);
 const int32 PartialDiscovered=Room->GetNewestHistoricalDiscoveredCellCountForTesting(RotateId);
 const int32 PartialCells=Room->GetNewestHistoricalCellCountForTesting(RotateId);
 TestTrue(TEXT("Intermediate epoch contains real discovered-present samples"),PartialDiscovered>0);
 TestTrue(TEXT("Intermediate epoch remains a local fragment, never a whole cabinet"),PartialDiscovered<PartialCells);

 // E/F. Finish at 180 degrees while hidden, then slowly reacquire through the
 // same ordinary cone edge before allowing the whole final cabinet to become Live.
 Step(300);
 TestEqual(TEXT("Hidden motion finishes at 180 degrees"),
  static_cast<int32>(FMath::RoundToInt(FMath::Abs(Room->GetTrackedTransform(RotateId).Rotator().Yaw))),180);
 int32 MaxSurface3DOverlap=0,MaxCap3DOverlap=0,Max3DOwnership=0;
 for(int32 Yaw=155;Yaw>=90;--Yaw)
 {
  Player->SetActorRotation(FRotator(0,float(Yaw),0));Step(2);
  MaxSurface3DOverlap=FMath::Max(MaxSurface3DOverlap,Room->GetCurrent3DOverlapStaleSurfaceForTesting(RotateId));
  MaxCap3DOverlap=FMath::Max(MaxCap3DOverlap,Room->GetCurrent3DOverlapStaleCapForTesting(RotateId));
  Max3DOwnership=FMath::Max(Max3DOwnership,Room->GetMax3DRenderOwnershipContributorsForTesting(RotateId));
 }
 Player->SetActorRotation(FRotator(0,90,0));Step(120);
 MaxSurface3DOverlap=FMath::Max(MaxSurface3DOverlap,Room->GetCurrent3DOverlapStaleSurfaceForTesting(RotateId));
 MaxCap3DOverlap=FMath::Max(MaxCap3DOverlap,Room->GetCurrent3DOverlapStaleCapForTesting(RotateId));
 Max3DOwnership=FMath::Max(Max3DOwnership,Room->GetMax3DRenderOwnershipContributorsForTesting(RotateId));
 TestEqual(TEXT("Final cabinet becomes completely legal Live geometry"),Room->GetLastLegalCoverageRatioForTesting(RotateId),1.0f);
 TestTrue(TEXT("SpatialEvidenceOnly legitimately retains at least one stale epoch"),Room->GetStaleEpochCountForTesting(RotateId)>0);
 TestEqual(TEXT("Current-owned 3D space contains no stale surface contribution"),MaxSurface3DOverlap,0);
 TestEqual(TEXT("Current-owned 3D space contains no stale cap contribution"),MaxCap3DOverlap,0);
 TestTrue(TEXT("True 3D render ownership never exceeds one contributor"),Max3DOwnership<=1);
 AddInfo(FString::Printf(TEXT("partial_ratio=%.6f partial_discovered=%d/%d max_surface_3d_overlap=%d max_cap_3d_overlap=%d max_3d_ownership=%d %s"),
  PartialRatio,PartialDiscovered,PartialCells,MaxSurface3DOverlap,MaxCap3DOverlap,Max3DOwnership,
  *Room->Get3DOwnershipTelemetryForTesting(RotateId)));
 Fixture->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellPartialCurrentMultiEpoch3DOwnershipTest,
 "Darkwell.PropLab.MovingRules.InWorldControls.PartialCurrentMultiEpoch3DOwnership", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellPartialCurrentMultiEpoch3DOwnershipTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("PartialCurrentMultiEpoch3DOwnership"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")),true);
 UWorld* World=TestWorld.Get();World->URL.AddOption(TEXT("PropLabOriginal"));World->URL.AddOption(TEXT("InWorldControls"));
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(-300,70,92),FRotator(0,90,0));
 auto* Fixture=World->SpawnActor<ADarkwellPropGameplayLab>();Fixture->PostInitializeComponents();Fixture->DispatchBeginPlay();
 auto* Room=ADarkwellMovingPropLabRoom::FindActive(World);auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 if(!TestNotNull(TEXT("Multi-epoch 3D Lab exists"),Room)||!Player||!Adapter)return false;
 TestTrue(TEXT("Multi-epoch 3D Lab authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 TestTrue(TEXT("Multi-epoch 3D Lab reset"),Room->ResetRoom(Player));
 auto Step=[&](int32 Frames=1){for(int32 Frame=0;Frame<Frames;++Frame)
  {Adapter->Tick(1.f/60);Room->UpdateRoom(1.f/60,Player);Fixture->Tick(1.f/60);}};
 const FName RotateId(TEXT("Lab.InWorld.Rotate.Cabinet"));
 auto* Control=Room->GetControlForTesting(EDarkwellMovingPropLabControlKind::VisibleRotate);
 if(!TestNotNull(TEXT("Multi-epoch rotation control exists"),Control))return false;
 Player->SetActorLocation(FVector(-300,70,92));Player->SetActorRotation(FRotator(0,90,0));Step(90);World->UpdateWorldComponents(true,false);
 Player->GetInteractionComponent()->UpdateFocusedActorFromWorld();
 TestEqual(TEXT("Multi-epoch route focuses real F control"),Player->GetInteractionComponent()->GetFocusedActor(),static_cast<AActor*>(Control));
 TestTrue(TEXT("Multi-epoch route starts through real F"),Player->GetInteractionComponent()->TryInteract());
 Player->SetActorRotation(FRotator(0,-90,0));Step(130);

 auto PartialReacquire=[&](const float Minimum,const float Maximum)
 {
  for(int32 Yaw=150;Yaw>=130;--Yaw)
  {
   Player->SetActorRotation(FRotator(0,float(Yaw),0));Step(2);
   const float Ratio=Room->GetLastLegalCoverageRatioForTesting(RotateId);
   if(Ratio>=Minimum&&Ratio<=Maximum)return Ratio;
  }
  return 0.0f;
 };
 const float FirstPartial=PartialReacquire(.06f,.30f);TestTrue(TEXT("First intermediate epoch is partially observed"),FirstPartial>0);
 Step(4);Player->SetActorRotation(FRotator(0,-90,0));Step(3);
 const int32 FirstPartialCells=Room->GetNewestHistoricalDiscoveredCellCountForTesting(RotateId);
 TestTrue(TEXT("First partial epoch owns a strict cell subset"),FirstPartialCells>0
  && FirstPartialCells<Room->GetNewestHistoricalCellCountForTesting(RotateId));

 Step(65);
 const float SecondPartial=PartialReacquire(.04f,.32f);TestTrue(TEXT("Second intermediate angle is partially observed"),SecondPartial>0);
 Step(4);Player->SetActorRotation(FRotator(0,-90,0));Step(3);
 TestTrue(TEXT("0, first partial and second partial create multiple legitimate stale epochs"),Room->GetStaleEpochCountForTesting(RotateId)>=3);
 Step(300);

 float FinalPartial=0.0f;
 for(int32 Yaw=155;Yaw>=90&&FinalPartial==0.0f;--Yaw)
 {
  Player->SetActorRotation(FRotator(0,float(Yaw),0));Step(2);
  const float Ratio=Room->GetLastLegalCoverageRatioForTesting(RotateId);
  if(Ratio>=.40f&&Ratio<=.70f)FinalPartial=Ratio;
 }
 Step(8);
 TestTrue(TEXT("Final cabinet is only partly legally reacquired"),FinalPartial>0.0f
  && Room->GetLastLegalCoverageRatioForTesting(RotateId)<.75f);
 TestEqual(TEXT("Partial current-owned region has no stale surface intrusion"),Room->GetCurrent3DOverlapStaleSurfaceForTesting(RotateId),0);
 TestEqual(TEXT("Partial current-owned region clips only overlapping stale cap segments"),Room->GetCurrent3DOverlapStaleCapForTesting(RotateId),0);
 TestTrue(TEXT("Unreacquired spatial history remains resident without StableID clearing"),Room->GetStaleEpochCountForTesting(RotateId)>0
  && Room->GetHistoricalPresentationResourceCountForTesting(RotateId)>0);
 TestTrue(TEXT("Partial current reacquire keeps true 3D ownership exclusive"),Room->GetMaxTotalContributorsForTesting(RotateId)<=1);

 Player->SetActorRotation(FRotator(0,90,0));Step(120);
 TestEqual(TEXT("Full final reacquire keeps stale surface out of current OBB"),Room->GetCurrent3DOverlapStaleSurfaceForTesting(RotateId),0);
 TestEqual(TEXT("Full final reacquire keeps stale cap out of current OBB"),Room->GetCurrent3DOverlapStaleCapForTesting(RotateId),0);
 TestTrue(TEXT("Full final Live does not identity-clear all non-overlapping history"),Room->GetStaleEpochCountForTesting(RotateId)>0);
 TestTrue(TEXT("Multi-epoch final ownership remains one"),Room->GetMaxTotalContributorsForTesting(RotateId)<=1);
	 const FString LiveDiagnosis=Room->GetMultiEpochCompositeDiagnosis(RotateId);
	 Player->SetActorRotation(FRotator(0,-90,0));Step(30);
	 const FString MemoryDiagnosis=Room->GetMultiEpochCompositeDiagnosis(RotateId);
 AddInfo(FString::Printf(TEXT("first_partial=%.6f second_partial=%.6f final_partial=%.6f stale=%d %s"),
  FirstPartial,SecondPartial,FinalPartial,Room->GetStaleEpochCountForTesting(RotateId),*Room->Get3DOwnershipTelemetryForTesting(RotateId)));
	 AddInfo(TEXT("MULTI_EPOCH_LIVE_DIAGNOSIS\n")+LiveDiagnosis);
	 AddInfo(TEXT("MULTI_EPOCH_MEMORY_DIAGNOSIS\n")+MemoryDiagnosis);
 Fixture->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellEdgeContactSliverTest,
 "Darkwell.PropLab.MovingRules.InWorldControls.EdgeContactSliver", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellEdgeContactSliverTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("EdgeContactSliver"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")),true);
 UWorld* World=TestWorld.Get();World->URL.AddOption(TEXT("PropLabOriginal"));World->URL.AddOption(TEXT("InWorldControls"));
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(-300,70,92),FRotator(0,90,0));
 auto* Fixture=World->SpawnActor<ADarkwellPropGameplayLab>();Fixture->PostInitializeComponents();Fixture->DispatchBeginPlay();
 auto* Room=ADarkwellMovingPropLabRoom::FindActive(World);auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 if(!TestNotNull(TEXT("Edge-contact Lab exists"),Room)||!Player||!Adapter)return false;
 TestTrue(TEXT("Edge-contact authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 TestTrue(TEXT("Edge-contact reset"),Room->ResetRoom(Player));
 auto Step=[&](int32 Frames=1){for(int32 Frame=0;Frame<Frames;++Frame)
  {Adapter->Tick(1.f/60);Room->UpdateRoom(1.f/60,Player);Fixture->Tick(1.f/60);}};
 const FName RotateId(TEXT("Lab.InWorld.Rotate.Cabinet"));
 auto* Control=Room->GetControlForTesting(EDarkwellMovingPropLabControlKind::VisibleRotate);
 if(!TestNotNull(TEXT("Edge-contact real F control exists"),Control))return false;

 Player->SetActorLocation(FVector(-300,70,92));Player->SetActorRotation(FRotator(0,90,0));Step(90);World->UpdateWorldComponents(true,false);
 Player->GetInteractionComponent()->UpdateFocusedActorFromWorld();
 TestEqual(TEXT("Edge-contact route focuses VISIBLE ROTATE"),Player->GetInteractionComponent()->GetFocusedActor(),static_cast<AActor*>(Control));
 TestTrue(TEXT("Edge-contact route starts through real F"),Player->GetInteractionComponent()->TryInteract());
 Player->SetActorRotation(FRotator(0,-90,0));Step(130);
 bool bFoundPartial=false;
 for(int32 Yaw=150;Yaw>=130&&!bFoundPartial;--Yaw)
 {
  Player->SetActorRotation(FRotator(0,float(Yaw),0));Step(2);
  const float Ratio=Room->GetLastLegalCoverageRatioForTesting(RotateId);
  bFoundPartial=Ratio>=.06f&&Ratio<=.30f;
 }
 TestTrue(TEXT("Edge-contact route creates the intermediate partial epoch"),bFoundPartial);
 Step(4);Player->SetActorRotation(FRotator(0,-90,0));Step(303);

 int32 MaxSurfaceContact=0,MaxCapContact=0,MaxFilterLeak=0;
 FString ResidualTelemetry;
 for(int32 Yaw=155;Yaw>=90;--Yaw)
 {
  Player->SetActorRotation(FRotator(0,float(Yaw),0));Step(2);
  MaxSurfaceContact=FMath::Max(MaxSurfaceContact,Room->GetCurrentRenderContactStaleSurfaceForTesting(RotateId));
  MaxCapContact=FMath::Max(MaxCapContact,Room->GetCurrentRenderContactStaleCapForTesting(RotateId));
  MaxFilterLeak=FMath::Max(MaxFilterLeak,Room->GetHardOwnershipFilterLeakForTesting(RotateId));
  if(ResidualTelemetry.IsEmpty())ResidualTelemetry=Room->GetResidualFragmentTelemetryForTesting(RotateId);
 }
 TestEqual(TEXT("Closed-set ownership removes submitted stale surface contact"),MaxSurfaceContact,0);
 TestEqual(TEXT("Exact segment clipping removes submitted stale cap contact"),MaxCapContact,0);
 TestEqual(TEXT("Hard ownership remains zero after bilinear filtering"),MaxFilterLeak,0);
 TestEqual(TEXT("Legacy open-volume surface overlap misses the residual"),Room->GetCurrent3DOverlapStaleSurfaceForTesting(RotateId),0);
 TestEqual(TEXT("Legacy open-volume cap overlap misses the residual"),Room->GetCurrent3DOverlapStaleCapForTesting(RotateId),0);
 AddInfo(FString::Printf(TEXT("surface_contact=%d cap_contact=%d filter_leak=%d fragments=[%s]"),
  MaxSurfaceContact,MaxCapContact,MaxFilterLeak,*ResidualTelemetry));
 Fixture->Destroy();return true;
}

namespace
{
 struct FDarkwellResidualOwnershipRouteResult
 {
  int32 MaxSurfaceContact=0;
  int32 MaxCapContact=0;
  int32 MaxFilterLeak=0;
  int32 ViewsChecked=0;
  int32 MissingCuts=0;
  int32 OutsideSource=0;
  int32 VisibleCaps=0;
  int32 Records=0;
  int32 FalseOccupied=0;
  int32 FinalProxies=0;
  int32 FinalCaps=0;
  FString FineHistory;
	 FString CompositeDiagnosis;
	 FString SealedFineHistory;
  float SealedYaw=0;
 };

 FDarkwellResidualOwnershipRouteResult RunResidualOwnershipRoute(
  FAutomationTestBase& Test,const bool bCheckFourViews,const int32 Mode=2,const bool bLateObservation=false,
  const int32 ExtraHiddenFrames=0,const float PlayerY=70,const bool bFastReacquire=false,const float TargetLateYaw=0)
 {
  using namespace Darkwell::SightWeaveAdapterTests;
  FDarkwellResidualOwnershipRouteResult Result;
  FTestWorld TestWorld(TEXT("ResidualOwnershipRoute"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")),true);
  UWorld* World=TestWorld.Get();World->URL.AddOption(TEXT("PropLabOriginal"));World->URL.AddOption(TEXT("InWorldControls"));
  auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(-300,70,92),FRotator(0,90,0));
  auto* Fixture=World->SpawnActor<ADarkwellPropGameplayLab>();Fixture->PostInitializeComponents();Fixture->DispatchBeginPlay();
  auto* Room=ADarkwellMovingPropLabRoom::FindActive(World);auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
  if(!Test.TestNotNull(TEXT("Residual route Lab exists"),Room)||!Player||!Adapter)return Result;
  Test.TestTrue(TEXT("Residual route authority"),Adapter->RequestSightWeaveAuthority(Fixture));
  Test.TestTrue(TEXT("Residual route reset"),Room->ResetRoom(Player));
  IConsoleVariable* PresentationMode=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Darkwell.ProjectFogVisual.PropPresentationMode"));
  const int32 PreviousMode=PresentationMode->GetInt();
  PresentationMode->Set(Mode,ECVF_SetByConsole);
  Test.TestEqual(TEXT("Mode selector really accepted requested value"),PresentationMode->GetInt(),Mode);
  auto Step=[&](int32 Frames=1){for(int32 Frame=0;Frame<Frames;++Frame)
   {Adapter->Tick(1.f/60);Room->UpdateRoom(1.f/60,Player);Fixture->Tick(1.f/60);}};
  const FName RotateId(TEXT("Lab.InWorld.Rotate.Cabinet"));
  auto* Control=Room->GetControlForTesting(EDarkwellMovingPropLabControlKind::VisibleRotate);
  if(!Test.TestNotNull(TEXT("Residual route F control exists"),Control))return Result;
  Player->SetActorLocation(FVector(-300,PlayerY,92));Player->SetActorRotation(FRotator(0,90,0));Step(90);
  World->UpdateWorldComponents(true,false);Player->GetInteractionComponent()->UpdateFocusedActorFromWorld();
  Test.TestEqual(TEXT("Residual route focuses VISIBLE ROTATE"),Player->GetInteractionComponent()->GetFocusedActor(),static_cast<AActor*>(Control));
  Test.TestTrue(TEXT("Residual route starts through F"),Player->GetInteractionComponent()->TryInteract());
  Player->SetActorRotation(FRotator(0,-90,0));
  bool bPartial=false;
  if(TargetLateYaw>0)
  {
   for(int32 I=0;I<420 && FMath::Abs(Room->GetTrackedTransform(RotateId).Rotator().Yaw)<TargetLateYaw-5;++I) Step();
   Player->SetActorRotation(FRotator(0,146,0));Step(2);
   const float Ratio=Room->GetLastLegalCoverageRatioForTesting(RotateId);
   bPartial=Ratio>0 && Ratio<.50f;
  }
  else
  {
  Step((bLateObservation ? 190 : 130)+ExtraHiddenFrames);
  for(int32 Yaw=150;Yaw>=130&&!bPartial;--Yaw)
  {
   Player->SetActorRotation(FRotator(0,float(Yaw),0));Step(2);
   const float Ratio=Room->GetLastLegalCoverageRatioForTesting(RotateId);
   bPartial=Ratio>=.06f&&Ratio<=.30f;
  }
  }
  Test.TestTrue(TEXT("Residual route creates a partial intermediate epoch"),bPartial);
  Step(4);Player->SetActorRotation(FRotator(0,-90,0));Step(303);
  Result.SealedYaw=Room->GetNewestHistoricalYawForTesting(RotateId);
	 Result.SealedFineHistory=Room->GetFineHistoryTelemetry(RotateId);
	 Test.AddInfo(TEXT("SEALED_HISTORY_GRID_V2 ")+Result.SealedFineHistory);
  Result.MissingCuts=Room->GetMissingHistoricalCutCountForTesting(RotateId);
  Result.OutsideSource=Room->GetCapVerticesOutsideSourceForTesting(RotateId);
  Result.VisibleCaps=Room->GetVisibleHistoricalCapCountForTesting(RotateId);
  Result.Records=Room->GetSpatialRecordCount(RotateId);
  Test.AddInfo(FString::Printf(TEXT("MODE=%d %s"),Mode,*Room->GetCapLifecycleTelemetryForTesting(RotateId)));
  auto Accumulate=[&]()
  {
   Result.MaxSurfaceContact=FMath::Max(Result.MaxSurfaceContact,Room->GetCurrentRenderContactStaleSurfaceForTesting(RotateId));
   Result.MaxCapContact=FMath::Max(Result.MaxCapContact,Room->GetCurrentRenderContactStaleCapForTesting(RotateId));
   Result.MaxFilterLeak=FMath::Max(Result.MaxFilterLeak,Room->GetHardOwnershipFilterLeakForTesting(RotateId));
   Result.OutsideSource=FMath::Max(Result.OutsideSource,Room->GetCapVerticesOutsideSourceForTesting(RotateId));
  };
  for(int32 Yaw=(bFastReacquire ? 90 : 155);Yaw>=90;--Yaw)
  {
   Player->SetActorRotation(FRotator(0,float(Yaw),0));Step(2);Accumulate();
  }
  Step(30);
  Result.FineHistory=Room->GetFineHistoryTelemetry(RotateId);
  Test.AddInfo(TEXT("HISTORY_GRID_V2 ")+Result.FineHistory);
  Result.FalseOccupied=Room->GetFalseOccupiedHistoryCountForTesting(RotateId);
  Result.FinalProxies=Room->GetVisibleHistoricalProxyCountForTesting(RotateId);
  Result.FinalCaps=Room->GetVisibleHistoricalCapCountForTesting(RotateId);
	 Result.CompositeDiagnosis=Room->GetMultiEpochCompositeDiagnosis(RotateId);
  Test.AddInfo(FString::Printf(TEXT("RESIDUAL_RECHECK hidden_extra=%d player_y=%.1f sealed_yaw=%.4f proxies=%d caps=%d %s"),
   ExtraHiddenFrames,PlayerY,Room->GetNewestHistoricalYawForTesting(RotateId),Result.FinalProxies,Result.FinalCaps,
   *Room->GetResidualFragmentTelemetryForTesting(RotateId)));
  Test.AddInfo(Room->GetFalseOccupiedHistoryTelemetryForTesting(RotateId));
  if(bCheckFourViews)
  {
   struct FView { FVector Location; float Yaw; };
   const FView Views[]={{FVector(-300,70,92),90},{FVector(300,650,92),180},
    {FVector(-300,1230,92),-90},{FVector(-900,650,92),0}};
   for(const FView& View:Views)
   {
    Player->SetActorLocation(View.Location);Player->SetActorRotation(FRotator(0,View.Yaw,0));Step(30);Accumulate();++Result.ViewsChecked;
   }
  }
  Test.AddInfo(FString::Printf(TEXT("views=%d surface_contact=%d cap_contact=%d filter_leak=%d telemetry=%s"),
   Result.ViewsChecked,Result.MaxSurfaceContact,Result.MaxCapContact,Result.MaxFilterLeak,
   *Room->GetResidualFragmentTelemetryForTesting(RotateId)));
  PresentationMode->Set(PreviousMode,ECVF_SetByConsole);
  Fixture->Destroy();return Result;
 }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellFineHistoryParallelTest,
 "Darkwell.PropLab.MovingRules.HistoryGridV2.ParallelLateReacquire", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellFineHistoryParallelTest::RunTest(const FString&)
{
 for (const bool Fast : {true,false})
 {
  const auto Result=RunResidualOwnershipRoute(*this,false,2,true,53,100,Fast);
  TestFalse(TEXT("Real partial rotation produced per-epoch fine diagnostics"),Result.FineHistory.IsEmpty());
  AddInfo(FString::Printf(TEXT("V2_REACQUIRE fast=%d proxy=%d cap=%d %s"),
   Fast,Result.FinalProxies,Result.FinalCaps,*Result.FineHistory));
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellFineLateAnglesTest,
 "Darkwell.PropLab.MovingRules.HistoryGridV2.LateRotationAnglesFastFinalReacquire", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellFineLateAnglesTest::RunTest(const FString&)
{
 for(const float Target : {120.f,128.f,145.f,157.f})
 {
  const auto R=RunResidualOwnershipRoute(*this,false,2,true,0,100,true,Target);
  TestTrue(TEXT("Actual sealed late pose near requested test angle"),FMath::Abs(R.SealedYaw-Target)<10);
  TestEqual(TEXT("Fast final pose contains no stale proxy"),R.FinalProxies,0);
  TestEqual(TEXT("Fast final pose contains no stale cap"),R.FinalCaps,0);
  TestEqual(TEXT("No surface render contact"),R.MaxSurfaceContact,0);
  TestEqual(TEXT("No cap render contact"),R.MaxCapContact,0);
  AddInfo(FString::Printf(TEXT("LATE_ANGLE target=%.1f actual=%.3f %s"),Target,R.SealedYaw,*R.FineHistory));
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellFineSlowReacquireTest,
 "Darkwell.PropLab.MovingRules.HistoryGridV2.SlowFinalReacquireMatchesFast", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellFineSlowReacquireTest::RunTest(const FString&)
{
 const auto Fast=RunResidualOwnershipRoute(*this,false,2,true,0,100,true,157);
 const auto Slow=RunResidualOwnershipRoute(*this,false,2,true,0,100,false,157);
	 AddInfo(TEXT("FAST_SEALED ")+Fast.SealedFineHistory);
	 AddInfo(TEXT("SLOW_SEALED ")+Slow.SealedFineHistory);
 TestEqual(TEXT("Same frozen pose"),Slow.SealedYaw,Fast.SealedYaw);
 TestEqual(TEXT("Same terminal fine state counts"),Slow.FineHistory,Fast.FineHistory);
 TestEqual(TEXT("Slow final has no stale proxy"),Slow.FinalProxies,0);
 TestEqual(TEXT("Slow final has no stale cap"),Slow.FinalCaps,0);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellCapPartialClipTest,
 "Darkwell.PropLab.MovingRules.InWorldControls.CapPartialClipNotWholeReject", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellCapPartialClipTest::RunTest(const FString&)
{
 const TArray<FVector2D> Owned{FVector2D(0,50)};
 const auto Residual=ADarkwellMovingPropLabRoom::SubtractOwnedCapIntervals(FVector2D(0,100),Owned);
 TestEqual(TEXT("Half-owned candidate preserves one legal interval"),Residual.Num(),1);
 if(Residual.Num()==1)
 {
  TestTrue(TEXT("Only contact clearance is removed from legal half"),FMath::IsNearlyEqual(Residual[0].X,50.051,1.e-6));
  TestEqual(TEXT("Far endpoint is preserved"),Residual[0].Y,100.0);
 }
 const TArray<FVector2D> Middle{FVector2D(40,60)};
 TestEqual(TEXT("Interior clipping retains both disjoint legal pieces"),
  ADarkwellMovingPropLabRoom::SubtractOwnedCapIntervals(FVector2D(0,100),Middle).Num(),2);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellCapCoplanarContactTest,
 "Darkwell.PropLab.MovingRules.InWorldControls.CoplanarContact", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellCapCoplanarContactTest::RunTest(const FString&)
{
 using Room=ADarkwellMovingPropLabRoom;
 for(const double Gap:{0.0,.025,.05,.1,1.0})
 {
  const TArray<FVector2D> Owned{FVector2D(100+Gap,120)};
  const auto R=Room::SubtractOwnedCapIntervals(FVector2D(0,100),Owned);
  TestEqual(TEXT("Touching cannot reject whole remote cap"),R.Num(),1);
  if(R.Num()==1)
  {
   TestEqual(TEXT("Nonconflicting endpoint retained"),R[0].X,0.0);
   TestTrue(TEXT("Closed contact clipped without tolerance inflation"),
    FMath::IsNearlyEqual(R[0].Y,FMath::Min(100.0,100+Gap-.051),1.e-6));
  }
 }
 Room::FPrimitiveGeometrySnapshot G;
 G.LocalBounds=FBox(FVector(-50,-25,0),FVector(50,25,100));
 for(const double Yaw:{0.0,.01,5.0,45.0,89.99})
 {
  G.WorldTransform=FTransform(FRotator(0,Yaw,0),FVector::ZeroVector);
  double Entry,Exit;
  TestTrue(TEXT("Actual transformed source accepts interior segment"),
   Room::ClipSegmentToGeometryProjection(G,FVector2D(-100,0),FVector2D(100,0),0.0,Entry,Exit));
  for(const double T:{Entry,Exit})
  {
   const FVector P(FMath::Lerp(-100.0,100.0,T),0,50);
   TestTrue(TEXT("Both cut endpoints stay inside original source OBB"),
    G.LocalBounds.ExpandBy(.0001).IsInsideOrOn(G.WorldTransform.InverseTransformPosition(P)));
  }
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellHistoricalCapLifecycleTest,
 "Darkwell.PropLab.MovingRules.InWorldControls.ValidHistoricalCapSurvivesOwnershipClipping", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellHistoricalCapLifecycleTest::RunTest(const FString&)
{
 const auto Result=RunResidualOwnershipRoute(*this,false);
 TestEqual(TEXT("Sealed real partial discovery must not lose its cut candidate"),Result.MissingCuts,0);
 TestTrue(TEXT("Hidden partial history retains a rendered cap"),Result.VisibleCaps>0);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellCurrentOwnedResidualZeroTest,
 "Darkwell.PropLab.MovingRules.InWorldControls.CurrentOwnedResidualZero", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellCurrentOwnedResidualZeroTest::RunTest(const FString&)
{
 const auto Result=RunResidualOwnershipRoute(*this,true,2,true);
 TestEqual(TEXT("Cap fragments must never protrude outside their original source primitive"),Result.OutsideSource,0);
 TestEqual(TEXT("Current-owned stale surface is zero"),Result.MaxSurfaceContact,0);
 TestEqual(TEXT("Current-owned stale cap is zero"),Result.MaxCapContact,0);
 TestEqual(TEXT("Legally observed empty holes inside aggregate bounds must not retain stale surfaces"),Result.FalseOccupied,0);
 TestEqual(TEXT("Full legal recheck leaves no rendered stale proxy"),Result.FinalProxies,0);
 TestEqual(TEXT("Full legal recheck leaves no rendered stale cap"),Result.FinalCaps,0);
 const auto GpuPose=RunResidualOwnershipRoute(*this,true,2,true,15,100);
 TestEqual(TEXT("Later GPU-like pose has zero owned surface contact"),GpuPose.MaxSurfaceContact,0);
 TestEqual(TEXT("Later GPU-like pose has zero owned cap contact"),GpuPose.MaxCapContact,0);
 TestEqual(TEXT("Later GPU-like pose has zero filtered ownership leakage"),GpuPose.MaxFilterLeak,0);
 TestEqual(TEXT("Later GPU-like full recheck has no remaining proxy"),GpuPose.FinalProxies,0);
 TestEqual(TEXT("Later GPU-like full recheck has no remaining cap"),GpuPose.FinalCaps,0);
 const auto BoundaryPose=RunResidualOwnershipRoute(*this,true,2,true,20,100);
 TestEqual(TEXT("GPU boundary pose has zero owned surface contact"),BoundaryPose.MaxSurfaceContact,0);
 TestEqual(TEXT("GPU boundary pose has zero owned cap contact"),BoundaryPose.MaxCapContact,0);
 TestEqual(TEXT("GPU boundary pose has zero filtered ownership leakage"),BoundaryPose.MaxFilterLeak,0);
 TestEqual(TEXT("GPU boundary pose full recheck has no remaining proxy"),BoundaryPose.FinalProxies,0);
 TestEqual(TEXT("GPU boundary pose full recheck has no remaining cap"),BoundaryPose.FinalCaps,0);
 const auto LastEdgePose=RunResidualOwnershipRoute(*this,true,2,true,23,100);
 TestEqual(TEXT("Final edge-pose has zero owned surface contact"),LastEdgePose.MaxSurfaceContact,0);
 TestEqual(TEXT("Final edge-pose has zero owned cap contact"),LastEdgePose.MaxCapContact,0);
 TestEqual(TEXT("Final edge-pose has zero filtered ownership leakage"),LastEdgePose.MaxFilterLeak,0);
 TestEqual(TEXT("Final edge-pose full recheck has no remaining proxy"),LastEdgePose.FinalProxies,0);
 TestEqual(TEXT("Final edge-pose full recheck has no remaining cap"),LastEdgePose.FinalCaps,0);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellMode1Mode2ComparisonTest,
 "Darkwell.PropLab.MovingRules.InWorldControls.Mode1Mode2Comparison", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellMode1Mode2ComparisonTest::RunTest(const FString&)
{
 const auto Mode1=RunResidualOwnershipRoute(*this,false,1,true);
 const auto Mode2=RunResidualOwnershipRoute(*this,false,2,true);
 TestEqual(TEXT("Moving Lab routes both CVar values through shared spatial history"),Mode1.Records,Mode2.Records);
 TestEqual(TEXT("Moving Lab cap path is shared, unlike manual-room mode 1"),Mode1.VisibleCaps,Mode2.VisibleCaps);
 TestEqual(TEXT("Same cap loss is reproduced by the mode 1 and 2 selectors"),Mode1.MissingCuts,Mode2.MissingCuts);
 TestEqual(TEXT("Same geometry residual diagnostic in both selectors"),Mode1.OutsideSource,Mode2.OutsideSource);
 TestEqual(TEXT("Same false occupancy residual diagnostic in both selectors"),Mode1.FalseOccupied,Mode2.FalseOccupied);
 AddInfo(FString::Printf(TEXT("MOVING_LAB_SHARED_PATH Mode1 missing=%d outside=%d caps=%d; Mode2 missing=%d outside=%d caps=%d"),
  Mode1.MissingCuts,Mode1.OutsideSource,Mode1.VisibleCaps,Mode2.MissingCuts,Mode2.OutsideSource,Mode2.VisibleCaps));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellHardOwnershipFilteringTest,
 "Darkwell.PropLab.MovingRules.InWorldControls.HardOwnershipFiltering", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellHardOwnershipFilteringTest::RunTest(const FString&)
{
 const FDarkwellResidualOwnershipRouteResult Result=RunResidualOwnershipRoute(*this,false);
 TestEqual(TEXT("Smooth history cannot filter back into hard current ownership"),Result.MaxFilterLeak,0);
 TestEqual(TEXT("Filtered boundary submits no stale surface"),Result.MaxSurfaceContact,0);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellFourViewStableResidualTest,
 "Darkwell.PropLab.MovingRules.InWorldControls.FourViewStableResidual", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellFourViewStableResidualTest::RunTest(const FString&)
{
 const FDarkwellResidualOwnershipRouteResult Result=RunResidualOwnershipRoute(*this,true);
 TestEqual(TEXT("Four distinct final viewpoints were checked"),Result.ViewsChecked,4);
 TestEqual(TEXT("No viewpoint restores stale surface contact"),Result.MaxSurfaceContact,0);
 TestEqual(TEXT("No viewpoint restores stale cap contact"),Result.MaxCapContact,0);
 TestEqual(TEXT("No viewpoint restores filtered stale contribution"),Result.MaxFilterLeak,0);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellVerifiedEmptyCapPositiveControlTest,
 "Darkwell.PropLab.MovingRules.InWorldControls.VerifiedEmptyCapPositiveControl", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellVerifiedEmptyCapPositiveControlTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("VerifiedEmptyCapPositiveControl"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")));
 UWorld* World=TestWorld.Get();
 auto* Room=Spawn<ADarkwellManualStaleRoom>(*World,FVector(4000,0,0));
 auto* Fixture=Spawn<ADarkwellPropGameplayLab>(*World,FVector::ZeroVector);
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(4500,150,92),FRotator(0,90,0));
 auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 auto* Memory=World->GetSubsystem<UDarkwellRememberedPropSubsystem>();
 TestTrue(TEXT("Positive control uses legal authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 Room->ResetRoom(Player);Room->Command({TEXT("stalemanual"),TEXT("mode"),TEXT("2")});
 auto Step=[&](float Yaw,int32 Frames=1)
 {Player->SetActorLocation(FVector(4500,150,92));Player->SetActorRotation(FRotator(0,Yaw,0));
  for(int32 I=0;I<Frames;++I){Adapter->Tick(1.f/30);Memory->RefreshNowForTesting();Fixture->Tick(1.f/30);Room->UpdateObservation(1.f/30,Player);}};
 for(float Yaw=-30;Yaw<=210;Yaw+=1)Step(Yaw,2);
 Step(90,30);
 Player->SetActorLocation(Room->SwitchPosition()+FVector(0,0,92));Player->SetActorRotation(FRotator(0,90,0));
 for(int32 I=0;I<4;++I){Adapter->Tick(1.f/30);Memory->RefreshNowForTesting();Fixture->Tick(1.f/30);Room->UpdateObservation(1.f/30,Player);}
 TestFalse(TEXT("Positive control actual cabinet is absent"),Room->HasActualCabinet());
 bool bSawPartialVerified=false,bSawLegalCap=false;
 for(float Yaw=-30;Yaw<=210&&!bSawLegalCap;Yaw+=.5f)
 {
  Step(Yaw,1);int32 Verified=0,Remembered=0;
  for(const auto& Cell:Room->GetSpatialStateForTesting().GetCells())
  {Verified+=Cell.InitialRemembered>0&&Cell.VerifiedEmpty>0;Remembered+=Cell.InitialRemembered>0;}
  bSawPartialVerified|=Verified>0&&Verified<Remembered;
  bSawLegalCap=bSawPartialVerified&&Room->GetStaleCapTriangleCount()>0
   && Room->GetStaleCapComponentForTesting()->IsVisible();
 }
 TestTrue(TEXT("Real partial VerifiedEmpty evidence is reached"),bSawPartialVerified);
 TestTrue(TEXT("Non-overlapping #343A40 VerifiedEmpty cap remains visible"),bSawLegalCap);
 Fixture->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellMovingPropStaleFlickerStabilityTest,
 "Darkwell.PropLab.MovingRules.InWorldControls.StaleEpochFlickerStability", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellMovingPropStaleFlickerStabilityTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("MovingPropStaleFlicker"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")),true);
 UWorld* World=TestWorld.Get(); World->URL.AddOption(TEXT("PropLabOriginal")); World->URL.AddOption(TEXT("InWorldControls"));
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(-1100,80,92),FRotator(0,90,0));
 auto* Fixture=World->SpawnActor<ADarkwellPropGameplayLab>(); Fixture->PostInitializeComponents(); Fixture->DispatchBeginPlay();
 auto* Room=ADarkwellMovingPropLabRoom::FindActive(World);
 auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 if(!TestNotNull(TEXT("Flicker lab room exists"),Room)||!Player||!Adapter)return false;
 TestTrue(TEXT("Flicker lab requests SightWeave authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 TestTrue(TEXT("Flicker lab reset"),Room->ResetRoom(Player));
 auto Step=[&](int32 Frames=1)
 {
  for(int32 Frame=0;Frame<Frames;++Frame)
  {Adapter->Tick(1.f/60);Room->UpdateRoom(1.f/60,Player);Fixture->Tick(1.f/60);}
 };
 auto Use=[&](EDarkwellMovingPropLabControlKind Kind)
 {
  auto* Control=Room->GetControlForTesting(Kind); if(!Control)return false;
  const FVector Center=Control->GetActorLocation();
	const float ApproachSign=Center.Y < -800.f ? 1.f : -1.f;
  Player->SetActorLocation(FVector(Center.X,Center.Y+ApproachSign*190.f,92));
  Player->SetActorRotation((Center-Player->GetActorLocation()).GetSafeNormal2D().Rotation());
  Step(2);World->UpdateWorldComponents(true,false);
  Player->GetInteractionComponent()->UpdateFocusedActorFromWorld();
  return Player->GetInteractionComponent()->GetFocusedActor()==Control
   && Player->GetInteractionComponent()->TryInteract();
 };
 const FName HiddenId(TEXT("Lab.InWorld.Hidden.Cabinet"));
 for(int32 Round=0;Round<3;++Round)
 {
  TestTrue(*FString::Printf(TEXT("Round %d traced F arms hidden control"),Round+1),
   Use(EDarkwellMovingPropLabControlKind::HiddenAtoB));
  Step(30);
  Player->SetActorLocation(Room->GetPressurePlatePosition()+FVector(0,0,92));
  Player->SetActorRotation(FRotator(0,-90,0));Step(1);
  TestEqual(*FString::Printf(TEXT("Round %d seals exactly one A epoch before movement"),Round+1),
   Room->GetHiddenFreezeCountForTesting(HiddenId),1);
  TestEqual(*FString::Printf(TEXT("Round %d creates exactly one stale proxy"),Round+1),
   Room->GetHistoricalProxyCreationCountForTesting(HiddenId),1);
  const uint64 StartSignature=Room->GetHistoricalVisualSignatureForTesting(HiddenId);
  const int32 StartUploads=Room->GetHistoricalTextureUploadCountForTesting(HiddenId);
  TSet<int32> TransitSamples;
  for(int32 Frame=0;Frame<240;++Frame)
  {
   Step();
   TransitSamples.Add(FMath::RoundToInt(Room->GetTrackedTransform(HiddenId).GetLocation().X*10));
   TestEqual(*FString::Printf(TEXT("Round %d frame %d never reseals A"),Round+1,Frame),
    Room->GetHiddenFreezeCountForTesting(HiddenId),1);
  }
  TestTrue(*FString::Printf(TEXT("Round %d hidden motion has many intermediate transforms"),Round+1),
   TransitSamples.Num()>=160);
  TestEqual(*FString::Printf(TEXT("Round %d fixed A signature survives transit"),Round+1),
   Room->GetHistoricalVisualSignatureForTesting(HiddenId),StartSignature);
  TestEqual(*FString::Printf(TEXT("Round %d proxy never toggles visibility"),Round+1),
   Room->GetHistoricalProxyVisibilityTransitionsForTesting(HiddenId),0);
  TestEqual(*FString::Printf(TEXT("Round %d unchanged stale texture is not re-uploaded"),Round+1),
   Room->GetHistoricalTextureUploadCountForTesting(HiddenId),StartUploads);

  const int32 FixedFrames=Round==0?600:60;
  for(int32 Frame=0;Frame<FixedFrames;++Frame)Step();
  TestEqual(*FString::Printf(TEXT("Round %d fixed camera stale signature remains stable"),Round+1),
   Room->GetHistoricalVisualSignatureForTesting(HiddenId),StartSignature);
  TestEqual(*FString::Printf(TEXT("Round %d fixed camera has zero whole-proxy visibility transitions"),Round+1),
   Room->GetHistoricalProxyVisibilityTransitionsForTesting(HiddenId),0);
  if(Round==0)
  {
   for(int32 Frame=0;Frame<120;++Frame)
   {
    Player->SetActorRotation(FRotator(0,-110.f+40.f*Frame/119.f,0));Step();
   }
   TestEqual(TEXT("Slow behind-wall sweep cannot change the fixed A stale signature"),
    Room->GetHistoricalVisualSignatureForTesting(HiddenId),StartSignature);
   TestEqual(TEXT("Slow behind-wall sweep cannot flash the whole stale proxy"),
    Room->GetHistoricalProxyVisibilityTransitionsForTesting(HiddenId),0);
  }
  AddInfo(Room->GetHistoricalVisualTelemetryForTesting(HiddenId));
  TestTrue(*FString::Printf(TEXT("Round %d traced F reset is available"),Round+1),
   Use(EDarkwellMovingPropLabControlKind::ResetCurrent));Step(2);
 }
 Fixture->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellHistoryGridV2EpochScalingTelemetryTest,
 "Darkwell.PropLab.MovingRules.HistoryRuntime.EpochScalingTelemetry", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellHistoryGridV2EpochScalingTelemetryTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("HistoryRuntimeEpochScaling"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")),true);
 UWorld* World=TestWorld.Get();World->URL.AddOption(TEXT("PropLabOriginal"));World->URL.AddOption(TEXT("InWorldControls"));
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(-300,70,92),FRotator(0,-90,0));
 auto* Fixture=World->SpawnActor<ADarkwellPropGameplayLab>();Fixture->PostInitializeComponents();Fixture->DispatchBeginPlay();
 auto* Room=ADarkwellMovingPropLabRoom::FindActive(World);auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 if(!TestNotNull(TEXT("History runtime Lab exists"),Room)||!Player||!Adapter)return false;
 TestTrue(TEXT("History runtime authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 TestTrue(TEXT("History runtime reset"),Room->ResetRoom(Player));
 auto StepAndCollect=[&](int32 Frames,TArray<double>* GtSamples){for(int32 Frame=0;Frame<Frames;++Frame)
  {Adapter->Tick(1.f/60);Room->UpdateRoom(1.f/60,Player);Fixture->Tick(1.f/60);
   if(GtSamples)GtSamples->Add(Room->GetHistoryRuntimeFrameTelemetryForTesting().MovingPropLabGameThreadUs);}};
 auto Step=[&](int32 Frames){StepAndCollect(Frames,nullptr);};
 auto Percentile=[](const TArray<double>& Values,const double Fraction)
 {
  if(Values.IsEmpty())return 0.0;
  TArray<double> Sorted=Values;Sorted.Sort();
  return Sorted[FMath::Clamp(FMath::CeilToInt(Fraction*Sorted.Num())-1,0,Sorted.Num()-1)];
 };
 const FName RotateId(TEXT("Lab.InWorld.Rotate.Cabinet"));
 for(const int32 Epochs:{0,1,2,4,8})
 {
  TestTrue(FString::Printf(TEXT("Configure %d historical epochs"),Epochs),
   Room->ConfigureHistoricalEpochCountForTesting(RotateId,Epochs));
  TArray<double> ChangedGt;StepAndCollect(120,&ChangedGt);
  const auto T=Room->GetHistoryRuntimeTotalTelemetryForTesting();
  TestEqual(TEXT("Telemetry window frame count"),T.FramesAccumulated,uint64(120));
  TestEqual(TEXT("Resident epoch gauge"),T.ActiveHistoricalEpochs,Epochs);
  if(Epochs>0)TestTrue(TEXT("Fine samples are resident"),T.FineSamplesResident>0);
  const double Frames=FMath::Max<uint64>(1,T.FramesAccumulated);
  AddInfo(FString::Printf(TEXT("HISTORY_RUNTIME_POST_REFACTOR epochs=%d resident_samples=%d fine_bytes=%llu samples_scanned_per_frame=%.3f coverage_queries_per_frame=%.3f occupancy_tests_per_frame=%.3f geometry_tests_per_frame=%.3f ownership_tests_per_frame=%.3f texture_calls_per_frame=%.3f texture_uploads_per_sec=%.3f cap_calls_per_frame=%.3f cap_rebuilds_per_sec=%.3f refresh_us_per_frame=%.3f rotation_log_us_per_frame=%.3f report_us_per_frame=%.3f advance_fine_us_per_frame=%.3f tracked_us_per_frame=%.3f gt_us_per_frame=%.3f gt_p95_us=%.3f gt_p99_us=%.3f working_set=%llu uobjects=%d resources=proxy:%d/cap:%d/texture:%d/mid:%d"),
   Epochs,T.FineSamplesResident,T.FineHistoryResidentBytes,T.FineSamplesScanned/Frames,
   T.CoverageQueries/Frames,T.OccupancyTests/Frames,T.PrimitiveGeometryTests/Frames,
   T.OwnershipTests/Frames,T.UpdateRecordTextureCalls/Frames,T.TextureUploads/Frames*60.0,
   T.UpdateRecordCapCalls/Frames,T.CapMeshRebuilds/Frames*60.0,
   T.RefreshContributionDiagnosticsUs/Frames,T.LogRotationFrameUs/Frames,T.ReportHudUs/Frames,
   T.AdvanceFineHistoryUs/Frames,T.UpdateTrackedUs/Frames,T.MovingPropLabGameThreadUs/Frames,
   Percentile(ChangedGt,.95),Percentile(ChangedGt,.99),
   T.ProcessWorkingSetBytes,T.UObjectCount,T.ProxyCount,T.CapComponentCount,T.TextureCount,T.MidCount));
  Room->ResetHistoryRuntimeTelemetryForTesting();TArray<double> SteadyGt;StepAndCollect(300,&SteadyGt);
  const auto Steady=Room->GetHistoryRuntimeTotalTelemetryForTesting();
  TestEqual(TEXT("Steady epoch window scans no fine samples"),Steady.FineSamplesScanned,uint64(0));
  TestEqual(TEXT("Steady epoch window makes no coverage queries"),Steady.CoverageQueries,uint64(0));
  TestEqual(TEXT("Steady epoch window makes no occupancy tests"),Steady.OccupancyTests,uint64(0));
  TestEqual(TEXT("Steady epoch window makes no ownership tests"),Steady.OwnershipTests,uint64(0));
  TestEqual(TEXT("Steady epoch window makes no texture updates"),Steady.UpdateRecordTextureCalls,uint64(0));
  TestEqual(TEXT("Steady epoch window makes no cap updates"),Steady.UpdateRecordCapCalls,uint64(0));
  const double SteadyFrames=FMath::Max<uint64>(1,Steady.FramesAccumulated);
  AddInfo(FString::Printf(TEXT("HISTORY_RUNTIME_STEADY epochs=%d resident_samples=%d fine_bytes=%llu samples_scanned_per_frame=%.3f coverage_queries_per_frame=%.3f occupancy_tests_per_frame=%.3f geometry_tests_per_frame=%.3f ownership_tests_per_frame=%.3f texture_calls_per_frame=%.3f texture_uploads_per_sec=%.3f cap_calls_per_frame=%.3f cap_rebuilds_per_sec=%.3f refresh_us_per_frame=%.3f rotation_log_us_per_frame=%.3f report_us_per_frame=%.3f advance_fine_us_per_frame=%.3f tracked_us_per_frame=%.3f gt_us_per_frame=%.3f gt_p95_us=%.3f gt_p99_us=%.3f working_set=%llu uobjects=%d resources=proxy:%d/cap:%d/texture:%d/mid:%d"),
   Epochs,Steady.FineSamplesResident,Steady.FineHistoryResidentBytes,Steady.FineSamplesScanned/SteadyFrames,
   Steady.CoverageQueries/SteadyFrames,Steady.OccupancyTests/SteadyFrames,
   Steady.PrimitiveGeometryTests/SteadyFrames,Steady.OwnershipTests/SteadyFrames,
   Steady.UpdateRecordTextureCalls/SteadyFrames,Steady.TextureUploads/SteadyFrames*60.0,
   Steady.UpdateRecordCapCalls/SteadyFrames,Steady.CapMeshRebuilds/SteadyFrames*60.0,
   Steady.RefreshContributionDiagnosticsUs/SteadyFrames,Steady.LogRotationFrameUs/SteadyFrames,
   Steady.ReportHudUs/SteadyFrames,Steady.AdvanceFineHistoryUs/SteadyFrames,
   Steady.UpdateTrackedUs/SteadyFrames,Steady.MovingPropLabGameThreadUs/SteadyFrames,
   Percentile(SteadyGt,.95),Percentile(SteadyGt,.99),
   Steady.ProcessWorkingSetBytes,Steady.UObjectCount,Steady.ProxyCount,Steady.CapComponentCount,
   Steady.TextureCount,Steady.MidCount));
 }
 Fixture->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellMultiEpochCompositeDiagnosisTest,
 "Darkwell.PropLab.MovingRules.HistoryRuntime.MultiEpochCompositeDiagnosis", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellMultiEpochCompositeDiagnosisTest::RunTest(const FString&)
{
 const auto Result=RunResidualOwnershipRoute(*this,false,2,true,0,100,true,157);
 TestFalse(TEXT("Multi-epoch diagnosis is emitted"),Result.CompositeDiagnosis.IsEmpty());
 TestTrue(TEXT("Multi-epoch diagnosis includes A/B/C/D counts"),
  Result.CompositeDiagnosis.Contains(TEXT("A="))&&Result.CompositeDiagnosis.Contains(TEXT("B="))
  &&Result.CompositeDiagnosis.Contains(TEXT("C="))&&Result.CompositeDiagnosis.Contains(TEXT("D=")));
 AddInfo(TEXT("MULTI_EPOCH_COMPOSITE_DIAGNOSIS\n")+Result.CompositeDiagnosis);
 return true;
}

namespace
{
 struct FFastSweepObservation
 {
  int32 LegalFrames=0, InvalidFrames=0, DwellResets=0;
  float Consecutive=0, Total=0, Maximum=0, PreviousDwell=0;
 };
 struct FFastSweepRouteResult
 {
  TMap<uint64,FFastSweepObservation> Observations;
  TArray<ADarkwellMovingPropLabRoom::FFineEvidenceDiagnostic> Final;
  FString Hash;
  int32 SurvivingSeenEmpty=0, Proxies=0, Records=0;
 };
 FFastSweepRouteResult RunFastSweepEvidenceRoute(FAutomationTestBase& Test, int32 SweepFrames, float Fps=60)
 {
  using namespace Darkwell::SightWeaveAdapterTests;
  FFastSweepRouteResult Result;
  FTestWorld TestWorld(TEXT("FastSweepEvidence"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")),true);
  UWorld* World=TestWorld.Get();World->URL.AddOption(TEXT("PropLabOriginal"));World->URL.AddOption(TEXT("InWorldControls"));
  auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(-300,100,92),FRotator(0,90,0));
  auto* Fixture=World->SpawnActor<ADarkwellPropGameplayLab>();Fixture->PostInitializeComponents();Fixture->DispatchBeginPlay();
  auto* Room=ADarkwellMovingPropLabRoom::FindActive(World);auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
  if(!Test.TestNotNull(TEXT("Fast sweep Lab exists"),Room)||!Player||!Adapter)return Result;
  Test.TestTrue(TEXT("Fast sweep legal authority"),Adapter->RequestSightWeaveAuthority(Fixture));
  Room->ResetRoom(Player);
  const FName Id(TEXT("Lab.InWorld.Rotate.Cabinet"));
  auto Step=[&](int32 Frames=1,float Dt=1.f/60){for(int32 F=0;F<Frames;++F){Adapter->Tick(Dt);Room->UpdateRoom(Dt,Player);Fixture->Tick(Dt);}};
  Player->SetActorLocation(FVector(-300,100,92));Player->SetActorRotation(FRotator(0,90,0));Step(90);
  World->UpdateWorldComponents(true,false);Player->GetInteractionComponent()->UpdateFocusedActorFromWorld();
  Test.TestTrue(TEXT("Fast sweep route starts through F"),Player->GetInteractionComponent()->TryInteract());
  Player->SetActorRotation(FRotator(0,-90,0));Step(190);
  Player->SetActorRotation(FRotator(0,146,0));Step(6);
  Test.TestTrue(TEXT("Brief intermediate observation is partial"),Room->GetLastLegalCoverageRatioForTesting(Id)>0 && Room->GetLastLegalCoverageRatioForTesting(Id)<.5f);
  Player->SetActorRotation(FRotator(0,-90,0));Step(303);
  Test.TestTrue(TEXT("Normal middle gray history exists before sweep"),Room->GetVisibleHistoricalProxyCountForTesting(Id)>0);
  Player->SetActorRotation(FRotator(0,160,0));Step(1);
  auto Observe=[&](float Dt)
  {
   TArray<ADarkwellMovingPropLabRoom::FFineEvidenceDiagnostic> Samples;
   Room->GetFineEvidenceDiagnosticsForTesting(Id,Samples);
   for(const auto& D:Samples)
   {
    auto& O=Result.Observations.FindOrAdd((uint64(D.Epoch)<<32)|uint32(D.Index));
    const bool Legal=D.bValid && D.Coverage>=FDarkwellSpatialPropMemory::LegalCoverage && !D.bOccupied;
    if(Legal){++O.LegalFrames;O.Total+=Dt;O.Consecutive+=Dt;O.Maximum=FMath::Max(O.Maximum,O.Consecutive);}
    else O.Consecutive=0;
    O.InvalidFrames+=!D.bValid;
    O.DwellResets+=O.PreviousDwell>0 && D.Sample.EmptyDwell==0 && !D.Sample.bVerifiedEmpty && !D.bOwned;
    O.PreviousDwell=D.Sample.EmptyDwell;
   }
  };
  for(int32 F=1;F<=SweepFrames;++F)
  {
   Player->SetActorRotation(FRotator(0,FMath::Lerp(160.f,20.f,float(F)/SweepFrames),0));
   Step(1,1.f/Fps);Observe(1.f/Fps);
  }
  Step(90);Observe(0);
  Room->GetFineEvidenceDiagnosticsForTesting(Id,Result.Final);
  Result.Hash=Room->GetFineHistoryTelemetry(Id);
  Result.Proxies=Room->GetVisibleHistoricalProxyCountForTesting(Id);Result.Records=Room->GetSpatialRecordCount(Id);
  int32 Rows=0, Resets=0;
  for(const auto& D:Result.Final)
  {
   const auto* O=Result.Observations.Find((uint64(D.Epoch)<<32)|uint32(D.Index));
   if(!O)continue;Resets+=O->DwellResets;
   if(!D.bSubmitted || O->LegalFrames==0 || D.bOccupied)continue;
   ++Result.SurvivingSeenEmpty;
   if(Rows++<12)Test.AddInfo(FString::Printf(TEXT("FAST_SWEEP_SAMPLE epoch=%u index=%d xy=(%.3f,%.3f) state=%s opacity=%.3f coverage=%.3f valid=%d occupied=%d ownership=%d dwell=%.5f verified=%d legal_frames=%d total=%.5f max_consecutive=%.5f resets=%d invalid=%d"),
    D.Epoch,D.Index,D.Position.X,D.Position.Y,*D.Sample.State.ToString(),D.Sample.Opacity,D.Coverage,D.bValid,D.bOccupied,D.bOwned,D.Sample.EmptyDwell,D.Sample.bVerifiedEmpty,O->LegalFrames,O->Total,O->Maximum,O->DwellResets,O->InvalidFrames));
  }
  Test.AddInfo(FString::Printf(TEXT("FAST_SWEEP_ROUTE frames=%d fps=%.0f seen_empty_survivors=%d proxies=%d records=%d dwell_resets=%d %s"),SweepFrames,Fps,Result.SurvivingSeenEmpty,Result.Proxies,Result.Records,Resets,*Result.Hash));
  Fixture->Destroy();return Result;
 }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellFastSweepReproductionTest,
 "Darkwell.PropLab.MovingRules.FastSweep.ReproduceEvidenceTunneling", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellFastSweepReproductionTest::RunTest(const FString&)
{
 const auto Slow=RunFastSweepEvidenceRoute(*this,280);
 const auto Fast=RunFastSweepEvidenceRoute(*this,2);
 const auto Extreme=RunFastSweepEvidenceRoute(*this,1);
 TestEqual(TEXT("Slow sweep resolves legally seen empty samples"),Slow.SurvivingSeenEmpty,0);
 TestTrue(TEXT("Baseline reproduces short-legal-dwell survivors"),Fast.SurvivingSeenEmpty>0);
 TestTrue(TEXT("Old gray survives alongside observed final 180 pose"),Fast.Proxies>0 && Fast.Records>=3);
 int32 SpatialMiss=0;
 for(const auto& D:Extreme.Final)
 {
  const uint64 Key=(uint64(D.Epoch)<<32)|uint32(D.Index);
  const auto* S=Slow.Observations.Find(Key);const auto* F=Extreme.Observations.Find(Key);
  SpatialMiss+=D.bSubmitted && S && S->LegalFrames>0 && (!F || F->LegalFrames==0);
 }
 TestTrue(TEXT("Extreme endpoints skip space seen by identical slow arc"),SpatialMiss>0);
 AddInfo(FString::Printf(TEXT("FAST_SWEEP_ROOT_CAUSE=BOTH dwell_survivors=%d spatial_missed=%d"),Fast.SurvivingSeenEmpty,SpatialMiss));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellHistoryRuntimeIdleCostTest,
 "Darkwell.PropLab.MovingRules.HistoryRuntime.HistoryGridV2IdleCost", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellHistoryRuntimeIdleCostTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("HistoryRuntimeIdleCost"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")),true);
 UWorld* World=TestWorld.Get();World->URL.AddOption(TEXT("PropLabOriginal"));World->URL.AddOption(TEXT("InWorldControls"));
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(-300,70,92),FRotator(0,-90,0));
 auto* Fixture=World->SpawnActor<ADarkwellPropGameplayLab>();Fixture->PostInitializeComponents();Fixture->DispatchBeginPlay();
 auto* Room=ADarkwellMovingPropLabRoom::FindActive(World);auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 if(!TestNotNull(TEXT("Idle-cost Lab exists"),Room)||!Player||!Adapter)return false;
 TestTrue(TEXT("Idle-cost authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 TestTrue(TEXT("Idle-cost reset"),Room->ResetRoom(Player));
 auto Step=[&](int32 Frames){for(int32 Frame=0;Frame<Frames;++Frame)
  {Adapter->Tick(1.f/60);Room->UpdateRoom(1.f/60,Player);Fixture->Tick(1.f/60);}};
 const FName RotateId(TEXT("Lab.InWorld.Rotate.Cabinet"));
 TestTrue(TEXT("Idle-cost configures eight historical epochs"),Room->ConfigureHistoricalEpochCountForTesting(RotateId,8));
 Step(180);Room->ResetHistoryRuntimeTelemetryForTesting();Step(600);
 const auto T=Room->GetHistoryRuntimeTotalTelemetryForTesting();
 TestEqual(TEXT("Idle window retains eight epochs"),T.ActiveHistoricalEpochs,8);
 TestEqual(TEXT("Idle window scans no fine samples"),T.FineSamplesScanned,uint64(0));
 TestEqual(TEXT("Idle window performs no coverage scan"),T.CoverageFullScans,uint64(0));
 TestEqual(TEXT("Idle window performs no coverage query"),T.CoverageQueries,uint64(0));
 TestEqual(TEXT("Idle window performs no occupancy test"),T.OccupancyTests,uint64(0));
 TestEqual(TEXT("Idle window performs no geometry test"),T.PrimitiveGeometryTests,uint64(0));
 TestEqual(TEXT("Idle window performs no ownership test"),T.OwnershipTests,uint64(0));
 TestEqual(TEXT("Idle window performs no texture update"),T.UpdateRecordTextureCalls,uint64(0));
 TestEqual(TEXT("Idle window performs no texture upload"),T.TextureUploads,uint64(0));
 TestEqual(TEXT("Idle window performs no cap update"),T.UpdateRecordCapCalls,uint64(0));
 TestEqual(TEXT("Idle window performs no cap rebuild"),T.CapMeshRebuilds,uint64(0));
 TestEqual(TEXT("Idle window performs no contribution refresh"),T.RefreshContributionDiagnosticsUs,0.0);
 TestEqual(TEXT("Idle window performs no rotation diagnostic traversal"),T.LogRotationFrameUs,0.0);
 const double MeanGt=T.MovingPropLabGameThreadUs/FMath::Max<uint64>(1,T.FramesAccumulated);
 TestTrue(TEXT("Eight-epoch idle room stays below five milliseconds"),MeanGt<5000.0);
 AddInfo(FString::Printf(TEXT("HISTORY_RUNTIME_IDLE frames=%llu epochs=%d resident=%d fine_bytes=%llu gt_us_per_frame=%.3f report_us_per_frame=%.3f working_set=%llu uobjects=%d resources=proxy:%d/cap:%d/texture:%d/mid:%d"),
  T.FramesAccumulated,T.ActiveHistoricalEpochs,T.FineSamplesResident,T.FineHistoryResidentBytes,MeanGt,
  T.ReportHudUs/FMath::Max<uint64>(1,T.FramesAccumulated),T.ProcessWorkingSetBytes,T.UObjectCount,
  T.ProxyCount,T.CapComponentCount,T.TextureCount,T.MidCount));
 Fixture->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellHistoryRuntimeDirtyRegionTest,
 "Darkwell.PropLab.MovingRules.HistoryRuntime.HistoryGridV2DirtyRegion", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellHistoryRuntimeDirtyRegionTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("HistoryRuntimeDirtyRegion"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")),true);
 UWorld* World=TestWorld.Get();World->URL.AddOption(TEXT("PropLabOriginal"));World->URL.AddOption(TEXT("InWorldControls"));
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(-300,70,92),FRotator(0,-90,0));
 auto* Fixture=World->SpawnActor<ADarkwellPropGameplayLab>();Fixture->PostInitializeComponents();Fixture->DispatchBeginPlay();
 auto* Room=ADarkwellMovingPropLabRoom::FindActive(World);auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 if(!TestNotNull(TEXT("Dirty-region Lab exists"),Room)||!Player||!Adapter)return false;
 TestTrue(TEXT("Dirty-region authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 TestTrue(TEXT("Dirty-region reset"),Room->ResetRoom(Player));
 auto Step=[&](int32 Frames){for(int32 Frame=0;Frame<Frames;++Frame)
  {Adapter->Tick(1.f/60);Room->UpdateRoom(1.f/60,Player);Fixture->Tick(1.f/60);}};
 const FName RotateId(TEXT("Lab.InWorld.Rotate.Cabinet"));
 const FName EventId(TEXT("Lab.InWorld.Multi.SmallBox"));
 TestTrue(TEXT("Dirty-region configures one historical epoch"),Room->ConfigureHistoricalEpochCountForTesting(RotateId,1));
	 Step(180);
	 FTransform LocalChange=Room->GetTrackedTransform(RotateId);
	 LocalChange.AddToTranslation(FVector(45,0,0));
	 TestTrue(TEXT("Dirty-region moves one unrelated actual into a local overlap"),
	  Room->SetTrackedTransformForTesting(EventId,LocalChange));
 Room->ResetHistoryRuntimeTelemetryForTesting();Step(2);
 const auto Changed=Room->GetHistoryRuntimeTotalTelemetryForTesting();
 TestTrue(TEXT("Local geometry event scans at least one fine sample"),Changed.FineSamplesScanned>0);
 TestTrue(TEXT("Local geometry event scans less than the resident grid"),
  Changed.FineSamplesScanned<static_cast<uint64>(Changed.FineSamplesResident));
	 TestTrue(TEXT("Local geometry event needs at most one current coverage scan"),Changed.CoverageFullScans<=1);
 AddInfo(FString::Printf(TEXT("HISTORY_RUNTIME_DIRTY resident=%d scanned=%llu occupancy=%llu geometry=%llu ownership=%llu"),
  Changed.FineSamplesResident,Changed.FineSamplesScanned,Changed.OccupancyTests,
  Changed.PrimitiveGeometryTests,Changed.OwnershipTests));
	 Step(60);Room->ResetHistoryRuntimeTelemetryForTesting();Step(120);
 const auto Stable=Room->GetHistoryRuntimeTotalTelemetryForTesting();
 TestEqual(TEXT("After local event settles, fine scans return to zero"),Stable.FineSamplesScanned,uint64(0));
 TestEqual(TEXT("After local event settles, occupancy tests return to zero"),Stable.OccupancyTests,uint64(0));
 TestEqual(TEXT("After local event settles, ownership tests return to zero"),Stable.OwnershipTests,uint64(0));
 Fixture->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellHistoryRuntimeResetLifetimeTest,
 "Darkwell.PropLab.MovingRules.HistoryRuntime.RepeatedResetLifetime50", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellHistoryRuntimeResetLifetimeTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("HistoryRuntimeResetLifetime"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")),true);
 UWorld* World=TestWorld.Get();World->URL.AddOption(TEXT("PropLabOriginal"));World->URL.AddOption(TEXT("InWorldControls"));
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(-300,70,92),FRotator(0,-90,0));
 auto* Fixture=World->SpawnActor<ADarkwellPropGameplayLab>();Fixture->PostInitializeComponents();Fixture->DispatchBeginPlay();
 auto* Room=ADarkwellMovingPropLabRoom::FindActive(World);auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 if(!TestNotNull(TEXT("Reset-lifetime Lab exists"),Room)||!Player||!Adapter)return false;
 TestTrue(TEXT("Reset-lifetime authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 TestTrue(TEXT("Reset-lifetime reset"),Room->ResetRoom(Player));
 auto Step=[&](int32 Frames){for(int32 Frame=0;Frame<Frames;++Frame)
  {Adapter->Tick(1.f/60);Room->UpdateRoom(1.f/60,Player);Fixture->Tick(1.f/60);}};
 const FName RotateId(TEXT("Lab.InWorld.Rotate.Cabinet"));
 TestTrue(TEXT("Reset-lifetime baseline epochs"),Room->ConfigureHistoricalEpochCountForTesting(RotateId,4));Step(2);
 const auto Baseline=Room->GetHistoryRuntimeFrameTelemetryForTesting();
 for(int32 Cycle=0;Cycle<50;++Cycle)
 {
  TestTrue(FString::Printf(TEXT("Reset cycle %d rebuilds the same four epochs"),Cycle+1),
   Room->ConfigureHistoricalEpochCountForTesting(RotateId,4));
  Step(2);
  const auto Current=Room->GetHistoryRuntimeFrameTelemetryForTesting();
  TestEqual(TEXT("Proxy resources remain bounded"),Current.ProxyCount,Baseline.ProxyCount);
  TestEqual(TEXT("Cap resources remain bounded"),Current.CapComponentCount,Baseline.CapComponentCount);
  TestEqual(TEXT("Texture resources remain bounded"),Current.TextureCount,Baseline.TextureCount);
	  TestEqual(TEXT("MID resources remain bounded"),Current.MidCount,Baseline.MidCount);
	  if((Cycle+1)%5==0)CollectGarbage(RF_NoFlags,true);
	 }
 CollectGarbage(RF_NoFlags,true);Step(1);
 const auto Final=Room->GetHistoryRuntimeFrameTelemetryForTesting();
 TestTrue(TEXT("UObject count remains bounded after garbage collection"),Final.UObjectCount<=Baseline.UObjectCount+1024);
 AddInfo(FString::Printf(TEXT("HISTORY_RUNTIME_RESET_50 baseline_uobjects=%d final_uobjects=%d working_set=%llu resources=proxy:%d/cap:%d/texture:%d/mid:%d"),
  Baseline.UObjectCount,Final.UObjectCount,Final.ProcessWorkingSetBytes,Final.ProxyCount,
  Final.CapComponentCount,Final.TextureCount,Final.MidCount));
 TestTrue(TEXT("Reset-lifetime clears all synthetic epochs"),Room->ConfigureHistoricalEpochCountForTesting(RotateId,0));Step(2);
 TestEqual(TEXT("No historical presentation resource survives clear"),
  Room->GetHistoricalPresentationResourceCountForTesting(RotateId),0);
 Fixture->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellHistoryRuntimeLongSoakTest,
 "Darkwell.PropLab.MovingRules.HistoryRuntime.LongSoakFiveMinutes", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellHistoryRuntimeLongSoakTest::RunTest(const FString&)
{
 using namespace Darkwell::SightWeaveAdapterTests;
 FTestWorld TestWorld(TEXT("HistoryRuntimeLongSoak"),CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")),true);
 UWorld* World=TestWorld.Get();World->URL.AddOption(TEXT("PropLabOriginal"));World->URL.AddOption(TEXT("InWorldControls"));
 auto* Player=Spawn<ADarkwellCharacter>(*World,FVector(-300,70,92),FRotator(0,-90,0));
 auto* Fixture=World->SpawnActor<ADarkwellPropGameplayLab>();Fixture->PostInitializeComponents();Fixture->DispatchBeginPlay();
 auto* Room=ADarkwellMovingPropLabRoom::FindActive(World);auto* Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 if(!TestNotNull(TEXT("Soak Lab exists"),Room)||!Player||!Adapter)return false;
 TestTrue(TEXT("Soak authority"),Adapter->RequestSightWeaveAuthority(Fixture));
 TestTrue(TEXT("Soak reset"),Room->ResetRoom(Player));
 auto Step=[&](int32 Frames){for(int32 Frame=0;Frame<Frames;++Frame)
  {Adapter->Tick(1.f/60);Room->UpdateRoom(1.f/60,Player);Fixture->Tick(1.f/60);}};
 const FName RotateId(TEXT("Lab.InWorld.Rotate.Cabinet"));
 TestTrue(TEXT("Soak configures eight historical epochs"),Room->ConfigureHistoricalEpochCountForTesting(RotateId,8));
 Step(180);const auto Start=Room->GetHistoryRuntimeFrameTelemetryForTesting();
 Room->ResetHistoryRuntimeTelemetryForTesting();Step(18000);
 const auto T=Room->GetHistoryRuntimeTotalTelemetryForTesting();
 TestEqual(TEXT("Soak simulates five minutes at sixty Hz"),T.FramesAccumulated,uint64(18000));
 TestEqual(TEXT("Soak scans no idle fine samples"),T.FineSamplesScanned,uint64(0));
 TestEqual(TEXT("Soak performs no idle coverage query"),T.CoverageQueries,uint64(0));
 TestEqual(TEXT("Soak performs no idle occupancy test"),T.OccupancyTests,uint64(0));
 TestEqual(TEXT("Soak performs no idle ownership test"),T.OwnershipTests,uint64(0));
 TestEqual(TEXT("Soak performs no texture upload"),T.TextureUploads,uint64(0));
 TestEqual(TEXT("Soak performs no cap rebuild"),T.CapMeshRebuilds,uint64(0));
 TestEqual(TEXT("Soak proxy count remains stable"),T.ProxyCount,Start.ProxyCount);
 TestEqual(TEXT("Soak cap count remains stable"),T.CapComponentCount,Start.CapComponentCount);
 TestEqual(TEXT("Soak texture count remains stable"),T.TextureCount,Start.TextureCount);
 TestEqual(TEXT("Soak MID count remains stable"),T.MidCount,Start.MidCount);
 TestTrue(TEXT("Soak UObject count remains stable"),T.UObjectCount<=Start.UObjectCount+64);
 TestTrue(TEXT("Soak working set grows by less than 64 MiB"),
  T.ProcessWorkingSetBytes<=Start.ProcessWorkingSetBytes+64ull*1024ull*1024ull);
 const double MeanGt=T.MovingPropLabGameThreadUs/FMath::Max<uint64>(1,T.FramesAccumulated);
 TestTrue(TEXT("Five-minute eight-epoch soak stays below five milliseconds"),MeanGt<5000.0);
 AddInfo(FString::Printf(TEXT("HISTORY_RUNTIME_SOAK_5M frames=%llu gt_us_per_frame=%.3f start_working_set=%llu final_working_set=%llu start_uobjects=%d final_uobjects=%d resources=proxy:%d/cap:%d/texture:%d/mid:%d"),
  T.FramesAccumulated,MeanGt,Start.ProcessWorkingSetBytes,T.ProcessWorkingSetBytes,
  Start.UObjectCount,T.UObjectCount,T.ProxyCount,T.CapComponentCount,T.TextureCount,T.MidCount));
 Fixture->Destroy();return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellManualFixedRevealMaterialTest,
 "Darkwell.PropLab.ManualSwitch.FixedRevealMaterialContract", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellManualFixedRevealMaterialTest::RunTest(const FString&)
{
 auto* M=LoadObject<UMaterial>(nullptr,TEXT("/Game/Darkwell/Vision/PropLab/M_ManualFixedReveal.M_ManualFixedReveal"));
 if(!TestNotNull(TEXT("Fixed mesh masked material asset"),M)) return false;
 TestEqual(TEXT("Pixel mask, never translucent whole-body fade"),M->BlendMode,BLEND_Masked);
 TestNull(TEXT("No WPO: cannot thin, scale, move or rebuild vertices"),M->GetEditorOnlyData()->WorldPositionOffset.Expression);
 auto* Shadow=Cast<UMaterialExpressionShadowReplace>(M->GetEditorOnlyData()->OpacityMask.Expression);
 if(!TestNotNull(TEXT("Same mesh separates main mask from full real shadow"),Shadow)) return false;
 auto* One=Cast<UMaterialExpressionConstant>(Shadow->Shadow.Expression);
 TestTrue(TEXT("All original shadow pixels always enabled"),One && One->R==1.f);
 auto* Dither=Cast<UMaterialExpressionMaterialFunctionCall>(Shadow->Default.Expression);
 if(!TestNotNull(TEXT("Local alpha through native temporal opacity dithering"),Dither)) return false;
 UMaterialExpressionCustom* Alpha=nullptr;
 for(const auto& Input:Dither->FunctionInputs)
  if(auto* C=Cast<UMaterialExpressionCustom>(Input.Input.Expression)) Alpha=C;
 if(!TestNotNull(TEXT("Dither consumes local alpha, not an object-wide scalar"),Alpha)) return false;
 TestTrue(TEXT("Persistent per-position opacity: rejects current-Raw reset and whole-body fade"),
  Alpha->Code==TEXT("return Enabled > 0.5 ? (Ready > 0.5 ? saturate(State.r) : 0.0) : 1.0;"));
 if(!TestEqual(TEXT("Separate texture, mode and fail-closed readiness inputs"),Alpha->Inputs.Num(),3)) return false;
 auto* State=Cast<UMaterialExpressionTextureSampleParameter2D>(Alpha->Inputs[0].Input.Expression);
 if(!TestNotNull(TEXT("Discovery is a LOCAL texture, never uniform fade"),State)) return false;
 TestEqual(TEXT("Cumulative state texture"),State->ParameterName,FName(TEXT("SpatialStateTexture")));
 TArray<UMaterialExpression*> Pending{State->Coordinates.Expression}; TSet<UMaterialExpression*> Seen; int32 WorldNodes=0;
 while(!Pending.IsEmpty())
 {
  auto* Node=Pending.Pop(); if(!Node || Seen.Contains(Node)) continue; Seen.Add(Node);
  if(auto* W=Cast<UMaterialExpressionWorldPosition>(Node))
  {
   ++WorldNodes;
   TestEqual(TEXT("Fixed absolute world position excludes shader offsets"),W->WorldPositionShaderOffset,WPT_ExcludeAllShaderOffsets);
  }
  if(auto* Mask=Cast<UMaterialExpressionComponentMask>(Node); Mask && Mask->A)
   TestTrue(TEXT("Spatial inverse Y uses an actual RGBA connection; default RGB fails SM6"),Mask->Input.MaskA!=0);
  for(int32 I=0;;++I) { auto* Input=Node->GetInput(I); if(!Input) break; Pending.Add(Input->Expression); }
 }
 TestEqual(TEXT("Coverage coordinates actually depend on original world-space surface"),WorldNodes,1);
 Pending={M->GetEditorOnlyData()->BaseColor.Expression}; Seen.Reset(); bool bLocalColor=false;
 while(!Pending.IsEmpty())
 {
  auto* Node=Pending.Pop(); if(!Node || Seen.Contains(Node)) continue; Seen.Add(Node);
  if(auto* C=Cast<UMaterialExpressionCustom>(Node))
   bLocalColor|=C->Code==TEXT("return Enabled > 0.5 ? (Ready > 0.5 ? saturate(State.g) : 0.0) : lerp(lerp(Raw, min(Raw, Soft), UseSoft), 1.0, Whole);");
  for(int32 I=0;;++I) { auto* Input=Node->GetInput(I); if(!Input) break; Pending.Add(Input->Expression); }
 }
 TestTrue(TEXT("Original surface uses local live/gray blend, preserves exact Mode 0/1 branch"),bLocalColor);
 auto* Proxy=LoadObject<UMaterial>(nullptr,TEXT("/Game/Darkwell/Vision/PropLab/M_ManualAccumulatedMemory.M_ManualAccumulatedMemory"));
 if(!TestNotNull(TEXT("Existing proxy's accumulated mask material"),Proxy)) return false;
 TestNull(TEXT("Proxy also has no WPO"),Proxy->GetEditorOnlyData()->WorldPositionOffset.Expression);
 auto* ProxyAlpha=Cast<UMaterialExpressionCustom>(Proxy->GetEditorOnlyData()->Opacity.Expression);
 TestTrue(TEXT("Proxy can only show unresolved previously discovered cells, never snapshotValid whole-body opacity"),
  ProxyAlpha && ProxyAlpha->Code==TEXT("return Ready > 0.5 ? saturate(State.b) : 0.0;"));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellManualStaleCutCapMaterialTest,
 "Darkwell.PropLab.ManualSwitch.Mode2DarkGrayCutCapMaterial", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellManualStaleCutCapMaterialTest::RunTest(const FString&)
{
 auto* M=LoadObject<UMaterial>(nullptr,TEXT("/Game/Darkwell/Vision/PropLab/M_ManualStaleCutCap.M_ManualStaleCutCap"));
 if(!TestNotNull(TEXT("Dedicated cut-only material"),M)) return false;
 TestEqual(TEXT("Opaque solid section, not a black unknown overlay"),M->BlendMode,BLEND_Opaque);
 TestTrue(TEXT("Unlit color cannot drift with highlights or shadow"),M->GetShadingModels().HasShadingModel(MSM_Unlit));
 auto* Color=Cast<UMaterialExpressionConstant3Vector>(M->GetEditorOnlyData()->EmissiveColor.Expression);
 if(!TestNotNull(TEXT("Cap uses one fixed emissive color"),Color)) return false;
 const FLinearColor Expected=FLinearColor::FromSRGBColor(FColor(0x34,0x3A,0x40));
 TestTrue(TEXT("Cap color is exactly neutral dark gray sRGB #343A40, not black"),Color->Constant.Equals(Expected,1e-6f));
 TestNull(TEXT("Unlit cap does not use lit base color"),M->GetEditorOnlyData()->BaseColor.Expression);
 TestTrue(TEXT("Both scan directions see the same cap"),M->TwoSided);
 TestNull(TEXT("Cap material cannot move or scale geometry"),M->GetEditorOnlyData()->WorldPositionOffset.Expression);
 TestNull(TEXT("Cap has no opacity-driven body replacement"),M->GetEditorOnlyData()->Opacity.Expression);
 return true;
}
#endif

#endif
