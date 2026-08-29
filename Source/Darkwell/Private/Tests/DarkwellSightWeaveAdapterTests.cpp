#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
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

#endif
