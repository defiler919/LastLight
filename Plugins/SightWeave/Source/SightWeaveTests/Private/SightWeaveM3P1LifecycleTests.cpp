#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "SightWeaveRenderWorldSubsystem.h"
#include "SightWeaveWorldSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace SightWeave::M3P1::LifecycleTests
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

	FSightWeaveFloorDefinition Floor()
	{
		FSightWeaveFloorDefinition Result;
		Result.FloorId = FSightWeaveFloorId(FName(TEXT("M3Ground")));
		Result.BoundsMin = FVector2D(-3000.0, -3000.0);
		Result.BoundsMax = FVector2D(3000.0, 3000.0);
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		Result.bEnabled = true;
		Result.bActiveForQueries = true;
		return Result;
	}

	FSightWeaveVisionSourceDescription BypassVision()
	{
		FSightWeaveVisionSourceDescription Result;
		Result.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("M3Owner")));
		Result.FloorId = FSightWeaveFloorId(FName(TEXT("M3Ground")));
		Result.Transform.SetLocation(FVector(0.0, 0.0, 100.0));
		Result.Shape = ESightWeaveSourceShape::Radial;
		Result.Range = 800.0f;
		Result.HalfAngleDegrees = 180.0f;
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		Result.IlluminationPolicy = ESightWeaveIlluminationPolicy::BypassLegalIllumination;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P1PublicationAndSVEWorldLifecycleTest,
	"SightWeave.M3P1.Lifecycle.PublicationSVEWorldRestart",
	SightWeave::M3P1::LifecycleTests::TestFlags)

bool FSightWeaveM3P1PublicationAndSVEWorldLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P1::LifecycleTests;
	TestTrue(TEXT("SightWeaveRender module is loaded"),
		FModuleManager::Get().IsModuleLoaded(TEXT("SightWeaveRender")));

	uint64 FirstWorldSerial = 0;
	{
		FTestWorld World(TEXT("SightWeaveM3P1LifecycleA"));
		if (!TestNotNull(TEXT("First test world exists"), World.Get()))
		{
			return false;
		}
		USightWeaveWorldSubsystem* Runtime = World.Get()->GetSubsystem<USightWeaveWorldSubsystem>();
		USightWeaveRenderWorldSubsystem* Render =
			World.Get()->GetSubsystem<USightWeaveRenderWorldSubsystem>();
		if (!TestNotNull(TEXT("Runtime world subsystem exists"), Runtime)
			|| !TestNotNull(TEXT("Render world subsystem exists"), Render))
		{
			return false;
		}
		FirstWorldSerial = Render->GetWorldIdentity().Serial;
		TestTrue(TEXT("Render world identity is non-pointer and valid"),
			Render->GetWorldIdentity().IsValid());
		TestTrue(TEXT("World-scoped SVE is registered"), Render->HasSceneViewExtension());

		int32 PublicationEventCount = 0;
		const FDelegateHandle PublicationHandle = Runtime->OnSnapshotPublished().AddLambda(
			[&PublicationEventCount](FSightWeaveImmutableSnapshotPtr Snapshot)
			{
				if (Snapshot.IsValid() && Snapshot->bPublished)
				{
					++PublicationEventCount;
				}
			});
		UObject* Owner = World.Get();
		TestTrue(TEXT("Floor registers"), Runtime->RegisterFloor(Floor(), Owner));
		const FSightWeaveVisionSourceHandle Vision =
			Runtime->RegisterVisionSource(BypassVision(), Owner);
		TestTrue(TEXT("Bypass vision registers"), Vision.IsValid());
		const FSightWeaveRevision PublishedRevision = Runtime->PublishSnapshot();
		const FSightWeaveImmutableSnapshotPtr Snapshot = Runtime->AcquirePublishedSnapshot();
		TestTrue(TEXT("Immutable snapshot is published"),
			Snapshot.IsValid() && Snapshot->bPublished);
		TestEqual(TEXT("Published snapshot revision is traceable"),
			Snapshot->Revision, PublishedRevision);
		TestEqual(TEXT("Floor and vision mutations each publish exactly once"), PublicationEventCount, 2);
		TestEqual(TEXT("Render receives one immutable packet"),
			Render->GetDiagnostics().PublishedPacketCount,
			uint64(1));

		Runtime->PublishSnapshot();
		TestEqual(TEXT("No-change publication emits no event"), PublicationEventCount, 2);
		TestEqual(TEXT("No-change publication emits no packet"),
			Render->GetDiagnostics().PublishedPacketCount,
			uint64(1));

		TestTrue(TEXT("Vision source deletes"), Runtime->UnregisterVisionSource(Vision));
		Runtime->PublishSnapshot();
		TestEqual(TEXT("Deletion publishes replacement snapshot"), PublicationEventCount, 3);
		TestEqual(TEXT("Deletion publishes explicit black clear"),
			Render->GetDiagnostics().PublishedPacketCount,
			uint64(2));
		Runtime->OnSnapshotPublished().Remove(PublicationHandle);
	}

	{
		FTestWorld RestartedWorld(TEXT("SightWeaveM3P1LifecycleB"));
		USightWeaveRenderWorldSubsystem* RestartedRender = RestartedWorld.Get()
			? RestartedWorld.Get()->GetSubsystem<USightWeaveRenderWorldSubsystem>()
			: nullptr;
		if (!TestNotNull(TEXT("Restarted render subsystem exists"), RestartedRender))
		{
			return false;
		}
		TestNotEqual(TEXT("Restart gets a distinct monotonic world identity"),
			RestartedRender->GetWorldIdentity().Serial,
			FirstWorldSerial);
		TestTrue(TEXT("Restart registers a new SVE"), RestartedRender->HasSceneViewExtension());
		TestEqual(TEXT("Restart cannot inherit prior packet diagnostics"),
			RestartedRender->GetDiagnostics().PublishedPacketCount,
			uint64(0));
	}
	return true;
}

#endif
