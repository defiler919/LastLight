#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "RenderingThread.h"
#include "SightWeaveRenderWorldSubsystem.h"
#include "SightWeaveWorldSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace SightWeave::M3P3::LifecycleTests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	class FPresentationWorld final
	{
	public:
		explicit FPresentationWorld(const TCHAR* BaseName)
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
			World->WorldType = EWorldType::PIE;
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

		~FPresentationWorld()
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
		Result.FloorId = FSightWeaveFloorId(FName(TEXT("M3P3Floor")));
		Result.BoundsMin = FVector2D(-5000.0, -4000.0);
		Result.BoundsMax = FVector2D(5000.0, 4000.0);
		Result.HeightRange.ZMin = -100.0f;
		Result.HeightRange.ZMax = 500.0f;
		Result.bEnabled = true;
		Result.bActiveForQueries = true;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P3PresentationWorldLifecycleTest,
	"SightWeave.M3P3.Lifecycle.PIEPresentationWorldRestart",
	SightWeave::M3P3::LifecycleTests::TestFlags)

bool FSightWeaveM3P3PresentationWorldLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P3::LifecycleTests;
	uint64 FirstWorldSerial = 0;
	{
		FPresentationWorld World(TEXT("SightWeaveM3P3PIEA"));
		if (!TestNotNull(TEXT("M3.3 PIE world exists"), World.Get()))
		{
			return false;
		}
		USightWeaveWorldSubsystem* Runtime =
			World.Get()->GetSubsystem<USightWeaveWorldSubsystem>();
		USightWeaveRenderWorldSubsystem* Render =
			World.Get()->GetSubsystem<USightWeaveRenderWorldSubsystem>();
		if (!TestNotNull(TEXT("M3.3 runtime subsystem exists"), Runtime)
			|| !TestNotNull(TEXT("M3.3 render subsystem exists"), Render))
		{
			return false;
		}
		FirstWorldSerial = Render->GetWorldIdentity().Serial;
		TestTrue(TEXT("World-scoped SVE registers"), Render->HasSceneViewExtension());
		TestTrue(TEXT("Initial presentation selection is valid"),
			Render->GetPresentationSelection().IsValid());
		TestFalse(TEXT("Plugin with no active floor is explicitly disabled"),
			Render->GetPresentationSelection().IsEnabled());

		TestTrue(TEXT("Active presentation floor registers"),
			Runtime->RegisterFloor(Floor(), World.Get()));
		TestTrue(TEXT("Single active floor enables default presentation"),
			Render->GetPresentationSelection().IsEnabled());
		TestEqual(TEXT("Default presentation uses the active floor"),
			Render->GetPresentationSelection().GetFloorId(),
			Floor().FloorId);
		const uint64 DefaultRevision =
			Render->GetPresentationSelection().GetPresentationRevision();

		TestTrue(TEXT("Explicit owner/floor scope is accepted"),
			Render->SetPresentationScope(
				FSightWeaveKnowledgeOwnerId(FName(TEXT("ExplicitOwner"))),
				Floor().FloorId,
				ESightWeaveRenderPrecisionTier::Fine));
		TestEqual(TEXT("Explicit owner is immutable in selection"),
			Render->GetPresentationSelection().GetKnowledgeOwnerId(),
			FSightWeaveKnowledgeOwnerId(FName(TEXT("ExplicitOwner"))));
		TestEqual(TEXT("Explicit precision is immutable in selection"),
			Render->GetPresentationSelection().GetPrecisionTier(),
			ESightWeaveRenderPrecisionTier::Fine);
		TestTrue(TEXT("Explicit presentation increments revision"),
			Render->GetPresentationSelection().GetPresentationRevision() > DefaultRevision);

		Render->ClearPresentationScope();
		TestEqual(TEXT("Clearing explicit scope restores default Local owner"),
			Render->GetPresentationSelection().GetKnowledgeOwnerId(),
			FSightWeaveKnowledgeOwnerId(FName(TEXT("Local"))));
		TestEqual(TEXT("Diagnostics mirror enabled selection"),
			Render->GetDiagnostics().bPresentationEnabled,
			true);
	}
	FlushRenderingCommands();

	{
		FPresentationWorld Restarted(TEXT("SightWeaveM3P3PIEB"));
		USightWeaveRenderWorldSubsystem* Render = Restarted.Get()
			? Restarted.Get()->GetSubsystem<USightWeaveRenderWorldSubsystem>()
			: nullptr;
		if (!TestNotNull(TEXT("Restarted M3.3 render subsystem exists"), Render))
		{
			return false;
		}
		TestNotEqual(TEXT("World restart creates a distinct lifetime identity"),
			Render->GetWorldIdentity().Serial,
			FirstWorldSerial);
		TestFalse(TEXT("Restart cannot inherit enabled presentation scope"),
			Render->GetPresentationSelection().IsEnabled());
		TestEqual(TEXT("Restart cannot inherit packet diagnostics"),
			Render->GetDiagnostics().PublishedPacketCount,
			uint64(0));
	}
	FlushRenderingCommands();
	return true;
}

#endif
