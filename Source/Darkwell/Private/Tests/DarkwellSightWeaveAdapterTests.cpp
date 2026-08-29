#if WITH_DEV_AUTOMATION_TESTS

#include "AI/DarkwellStalkerCharacter.h"
#include "Combat/DarkwellLoadoutComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "Gameplay/DarkwellVisibilityComponent.h"
#include "Misc/AutomationTest.h"
#include "Player/DarkwellCharacter.h"
#include "SightWeaveStaticEnvironment.h"
#include "SightWeaveWorldSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Visibility/DarkwellVisionIntegrationFixture.h"
#include "Visibility/SightWeave/DarkwellSightWeaveWorldSubsystem.h"

namespace Darkwell::SightWeaveAdapterTests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	class FTestWorld final
	{
	public:
		explicit FTestWorld(const TCHAR* BaseName)
		{
			const FName WorldName = MakeUniqueObjectName(
				GetTransientPackage(),
				UWorld::StaticClass(),
				FName(BaseName));
			World = NewObject<UWorld>(GetTransientPackage(), WorldName, RF_Transient);
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
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(true);
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
	Stalker->ConfigurePersistentId(FName(TEXT("Enemy.Stalker.VisionIntegration")));

	UDarkwellSightWeaveWorldSubsystem* Adapter =
		World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
	USightWeaveWorldSubsystem* Runtime =
		World->GetSubsystem<USightWeaveWorldSubsystem>();
	if (!TestNotNull(TEXT("Adapter exists"), Adapter)
		|| !TestNotNull(TEXT("Runtime exists"), Runtime))
	{
		return false;
	}
	TestTrue(TEXT("Authority request is accepted"),
		Adapter->RequestSightWeaveAuthority(Fixture));
	TestTrue(TEXT("SightWeave becomes the only active authority"),
		Adapter->IsSightWeaveAuthorityActive());
	TestFalse(TEXT("Legacy visibility writes are disabled"),
		Player->GetVisibilityComponent()->IsVisibilityAuthorityEnabled());
	TestEqual(TEXT("Exactly one floor is registered"), Runtime->GetFloorCount(), 1);
	TestEqual(TEXT("Body and cone sources are registered"),
		Runtime->GetVisionSourceCount(), 2);
	TestEqual(TEXT("Only the legal torch light is registered"),
		Runtime->GetIlluminationSourceCount(), 1);
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
	TestEqual(TEXT("Stalker and HUD-facing snapshot share the same revision"),
		Stalker->GetAppliedVisibilityAuthorityRevision(), Snapshot.AuthorityRevision);

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

	Stalker->SetActorLocation(FVector(-570.0, 0.0, 92.0), false, nullptr,
		ETeleportType::TeleportPhysics);
	Adapter->Tick(0.0f);
	TestTrue(TEXT("Body radius bypass remains visible without legal light"),
		Adapter->TryGetSubjectSnapshot(Stalker->GetPersistentId(), Snapshot)
			&& Snapshot.bHardLive);

	TestTrue(TEXT("Torch can be restored"),
		Player->GetLoadoutComponent()->EquipRightHandItem(
			DarkwellGameplayTags::Equipment_Right_Torch));
	Player->SetActorLocation(FVector(-650.0, -400.0, 92.0), false, nullptr,
		ETeleportType::TeleportPhysics);
	Stalker->SetActorLocation(FVector(550.0, -400.0, 92.0), false, nullptr,
		ETeleportType::TeleportPhysics);
	Adapter->Tick(0.0f);
	TestTrue(TEXT("Occluded state still produces an authoritative decision"),
		Adapter->TryGetSubjectSnapshot(Stalker->GetPersistentId(), Snapshot));
	TestFalse(TEXT("Frozen wall segment occludes both body samples"), Snapshot.bHardLive);

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
	return true;
}

#endif
