#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SightWeaveObjectPolicy.h"
#include "SightWeaveSettings.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

namespace SightWeave::ObjectPolicyTests
{
	constexpr auto Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSightWeaveObjectOverrideTest,
	"SightWeave.ObjectPolicy.ObjectOverrideResolution", SightWeave::ObjectPolicyTests::Flags)
bool FSightWeaveObjectOverrideTest::RunTest(const FString&)
{
	using Mode = ESightWeaveHistoryMode;
	using Source = ESightWeaveObjectPolicySource;
	TestTrue(TEXT("Default config preserves capture"), GetDefault<USightWeaveSettings>()->DefaultHistoryMode == Mode::Always);
	for (const Mode Default : {Mode::Always, Mode::StationaryOnly, Mode::Never})
		for (const Mode Override : {Mode::Always, Mode::StationaryOnly, Mode::Never})
		{
			TestTrue(TEXT("Inheritance ignores unused override"),
				FResolvedSightWeaveObjectPolicy::Resolve(Default, Source::UseProjectDefault, Override).HistoryMode == Default);
			TestTrue(TEXT("Object override wins"),
				FResolvedSightWeaveObjectPolicy::Resolve(Default, Source::Override, Override).HistoryMode == Override);
		}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSightWeaveMotionIdempotenceTest,
	"SightWeave.ObjectPolicy.MotionApiIdempotence", SightWeave::ObjectPolicyTests::Flags)
bool FSightWeaveMotionIdempotenceTest::RunTest(const FString&)
{
	FSightWeaveObjectHistoryCapture State;
	FResolvedSightWeaveObjectPolicy Policy; Policy.HistoryMode = ESightWeaveHistoryMode::StationaryOnly;
	State.Initialize(Policy);
	TestFalse(TEXT("Unbalanced stop is harmless"), State.SetMoving(false));
	for (int32 Cycle = 0; Cycle < 100; ++Cycle)
	{
		TestTrue(TEXT("Begin changes state"), State.SetMoving(true));
		TestFalse(TEXT("Duplicate begin is idempotent"), State.SetMoving(true));
		State.ObserveLegally();
		TestFalse(TEXT("Moving observation cannot arm capture"), State.IsHistoryEligible());
		TestTrue(TEXT("End changes state"), State.SetMoving(false));
		TestFalse(TEXT("Duplicate end is idempotent"), State.SetMoving(false));
		TestFalse(TEXT("End alone cannot arm capture"), State.IsHistoryEligible());
		State.ObserveLegally();
		TestTrue(TEXT("Fresh stationary observation arms capture"), State.IsHistoryEligible());
	}
	TestEqual(TEXT("Exactly two revisions per cycle"), State.GetMovingRevision(), uint64(200));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSightWeavePolicyRegistrationTest,
	"SightWeave.ObjectPolicy.RegistrationCachesPolicyAndIsolatesObjects", SightWeave::ObjectPolicyTests::Flags)
bool FSightWeavePolicyRegistrationTest::RunTest(const FString&)
{
	UWorld* World = NewObject<UWorld>(GetTransientPackage(), MakeUniqueObjectName(
		GetTransientPackage(), UWorld::StaticClass(), TEXT("ObjectPolicyWorld")), RF_Transient);
	if (!TestNotNull(TEXT("World"), World)) return false;
	World->WorldType = EWorldType::Game;
	GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
	World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false));
	AActor* Actor = World->SpawnActor<AActor>();
	USightWeaveObjectPolicyComponent* A = NewObject<USightWeaveObjectPolicyComponent>(Actor);
	USightWeaveObjectPolicyComponent* B = NewObject<USightWeaveObjectPolicyComponent>(Actor);
	B->PolicySource = ESightWeaveObjectPolicySource::Override;
	B->HistoryMode = ESightWeaveHistoryMode::Never;
	A->RegisterComponent(); B->RegisterComponent();
	const auto OriginalDefault = GetMutableDefault<USightWeaveSettings>()->DefaultHistoryMode;
	GetMutableDefault<USightWeaveSettings>()->DefaultHistoryMode = ESightWeaveHistoryMode::StationaryOnly;
	A->HistoryMode = ESightWeaveHistoryMode::Never;
	A->SetSightWeaveMoving(true);
	TestTrue(TEXT("Registration is stable despite authoring/default edits"), A->GetResolvedHistoryMode() == OriginalDefault);
	TestTrue(TEXT("Override remains Never"), B->GetResolvedHistoryMode() == ESightWeaveHistoryMode::Never);
	TestFalse(TEXT("Motion is independent"), B->IsSightWeaveMoving());
	B->NotifyLegalObservation();
	TestFalse(TEXT("Never cannot capture"), B->IsHistoryEligible());
	TestFalse(TEXT("Component never ticks"), A->PrimaryComponentTick.bCanEverTick);
	TestNotNull(TEXT("Blueprint motion entry"), A->FindFunction(TEXT("SetSightWeaveMoving")));
	GetMutableDefault<USightWeaveSettings>()->DefaultHistoryMode = OriginalDefault;
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(true);
	return true;
}
#endif
