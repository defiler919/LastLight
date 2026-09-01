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
#include "VisionPresentation/DarkwellManualStaleRoom.h"
#include "HAL/IConsoleManager.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#if WITH_EDITOR
#include "Materials/MaterialExpressionConstant.h"
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
 "Darkwell.PropLab.ManualSwitch.Mode2BlackCutCap", Darkwell::SightWeaveAdapterTests::TestFlags)
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
 for(float Yaw=-30;Yaw<=210;Yaw+=1) Step(FVector(4500,150,92),Yaw,2);
 float Discovered=0; for(const auto& C:Room->GetSpatialStateForTesting().GetCells()) Discovered+=C.DiscoveredPresent;
 TestTrue(TEXT("Setup remembers nearly the full original surface"),Discovered>=Room->GetSpatialStateForTesting().GetCells().Num()*.95f);
 TestEqual(TEXT("No cap while actual cabinet is present"),Room->GetStaleCapTriangleCount(),0);
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
 TestEqual(TEXT("Complete erase removes every meaningless black cap"),Room->GetStaleCapTriangleCount(),0);
 TestFalse(TEXT("Complete erase hides cap component"),Room->GetStaleCapComponentForTesting()->IsVisible());
 Fixture->Destroy(); return true;
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
 "Darkwell.PropLab.ManualSwitch.Mode2BlackCutCapMaterial", Darkwell::SightWeaveAdapterTests::TestFlags)
bool FDarkwellManualStaleCutCapMaterialTest::RunTest(const FString&)
{
 auto* M=LoadObject<UMaterial>(nullptr,TEXT("/Game/Darkwell/Vision/PropLab/M_ManualStaleCutCap.M_ManualStaleCutCap"));
 if(!TestNotNull(TEXT("Dedicated cut-only material"),M)) return false;
 TestEqual(TEXT("Opaque solid section, not a black unknown overlay"),M->BlendMode,BLEND_Opaque);
 TestTrue(TEXT("Unlit stable black interior"),M->GetShadingModels().HasShadingModel(MSM_Unlit));
 TestTrue(TEXT("Both scan directions see the same cap"),M->TwoSided);
 TestNull(TEXT("Cap material cannot move or scale geometry"),M->GetEditorOnlyData()->WorldPositionOffset.Expression);
 TestNull(TEXT("Cap has no opacity-driven body replacement"),M->GetEditorOnlyData()->Opacity.Expression);
 return true;
}
#endif

#endif
