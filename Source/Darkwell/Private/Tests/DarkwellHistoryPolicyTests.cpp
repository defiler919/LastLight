#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Player/DarkwellCharacter.h"
#include "Interaction/DarkwellInteractionComponent.h"
#include "VisionPresentation/DarkwellMovingPropLabRoom.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "Visibility/SightWeave/DarkwellSightWeaveWorldSubsystem.h"

namespace Darkwell::HistoryPolicyTests
{
	const FName Id(TEXT("Lab.InWorld.Rotate.Cabinet"));
	using Mode = ESightWeaveHistoryMode;
	struct FRoom
	{
		UWorld* World;
		ADarkwellCharacter* Player;
		ADarkwellPropGameplayLab* Fixture;
		ADarkwellMovingPropLabRoom* Room;
		UDarkwellSightWeaveWorldSubsystem* Adapter;
		FRoom()
		{
			UPackage* Package = CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab"));
			World = NewObject<UWorld>(Package, MakeUniqueObjectName(Package, UWorld::StaticClass(), TEXT("HistoryPolicy")), RF_Transient);
			World->WorldType = EWorldType::Game;
			GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
			World->InitializeNewWorld(UWorld::InitializationValues().InitializeScenes(true)
				.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
			World->URL.AddOption(TEXT("PropLabOriginal")); World->URL.AddOption(TEXT("InWorldControls"));
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ADarkwellCharacter>(ADarkwellCharacter::StaticClass(), FVector(-300,100,92), FRotator(0,90,0), P);
			Player->DispatchBeginPlay();
			Fixture = World->SpawnActor<ADarkwellPropGameplayLab>();
			Fixture->PostInitializeComponents(); Fixture->DispatchBeginPlay();
			Room = ADarkwellMovingPropLabRoom::FindActive(World);
			Adapter = World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
			Adapter->RequestSightWeaveAuthority(Fixture); Room->ResetRoom(Player);
			Face(90);
		}
		~FRoom() { Fixture->Destroy(); World->DestroyWorld(true); GEngine->DestroyWorldContext(World); }
		void Face(float Yaw) { Player->SetActorLocation(FVector(-300,100,92)); Player->SetActorRotation(FRotator(0,Yaw,0)); }
		void Step(int32 Frames=1)
		{
			for (int32 I=0; I<Frames; ++I) { Adapter->Tick(1.f/60); Room->UpdateRoom(1.f/60,Player); Fixture->Tick(1.f/60); }
		}
		bool UseRotation()
		{
			auto* C = Room->GetControlForTesting(EDarkwellMovingPropLabControlKind::VisibleRotate);
			Player->SetActorLocation(C->GetActorLocation()+FVector(0,-190,0));
			Player->SetActorRotation(FRotator(0,90,0)); Step(2);
			World->UpdateWorldComponents(true,false);
			auto* Interaction=Player->GetInteractionComponent(); Interaction->UpdateFocusedActorFromWorld();
			return Interaction->GetFocusedActor()==C && Interaction->GetFocusedPrompt().ToString().Contains(TEXT("F")) && Interaction->TryInteract();
		}
	};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FDarkwellHistoryPolicyTest,
	"Darkwell.PropLab.HistoryPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FDarkwellHistoryPolicyTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	for (const TCHAR* Name : {TEXT("ProjectDefaultPreservesCurrentBehavior"), TEXT("AlwaysVisibleRotateParity"),
		TEXT("StationaryOnlyNoMovingHistory"), TEXT("StationaryOnlyRequiresFreshObservation"),
		TEXT("StationaryOnlyContinuousLiveMotion"), TEXT("NeverNoHistoricalResources"),
		TEXT("ExistingHistoryNotIdentityCleared"), TEXT("MultiObjectPolicyIsolation"),
		TEXT("AbandonCurrentObservationWithoutHistory"), TEXT("TransientLivePresentationPreservesAuthorityAndAA")})
	{
		Names.Add(Name); Commands.Add(Name);
	}
}
bool FDarkwellHistoryPolicyTest::RunTest(const FString& Case)
{
	using namespace Darkwell::HistoryPolicyTests;
	if (Case == TEXT("AbandonCurrentObservationWithoutHistory"))
	{
		FDarkwellSpatialObservationHistory H; H.Initialize(TEXT("one-real-identity"));
		H.BeginObservedLocation(FTransform::Identity,FBox2D(FVector2D(0),FVector2D(2.5)));
		H.AdvanceCurrent(.2f,TArray<float>{1}); H.FreezeCurrentForHiddenMovement();
		H.GetMutableRecords()[0].FineHistory.Initialize(H.GetRecords()[0].SpatialMemory);
		const uint64 Hash=H.GetRecords()[0].FineHistory.EvidenceHash();
		H.BeginObservedLocation(FTransform(FVector(20,0,0)),FBox2D(FVector2D(20,0),FVector2D(22.5,2.5)));
		TestTrue(TEXT("Abandon unsealed current"),H.AbandonCurrentObservationWithoutHistory());
		TestFalse(TEXT("Repeated abandon is harmless"),H.AbandonCurrentObservationWithoutHistory());
		TestEqual(TEXT("Only prior historical record retained"),H.GetRecords().Num(),1);
		TestEqual(TEXT("Fine evidence is bit-identical"),H.GetRecords()[0].FineHistory.EvidenceHash(),Hash);
		TestEqual(TEXT("Epoch is not reused"),H.GetNextEpoch(),uint32(3)); return true;
	}
	if (Case == TEXT("TransientLivePresentationPreservesAuthorityAndAA"))
	{
		FDarkwellSpatialPropMemory M;
		M.Initialize(TEXT("transient"),FBox2D(FVector2D(0),FVector2D(5,2.5)));
		M.BeginPresent(); M.Advance(.2f,TArray<float>{1,1}); M.Advance(.2f,TArray<float>{1,0});
		TArray<FLinearColor> Normal, Live, Again;
		M.BuildConservativePresentation(4,Normal); M.BuildConservativePresentation(4,Live,true);
		M.BuildConservativePresentation(4,Again);
		TestTrue(TEXT("Transient presentation cannot mutate memory or normal AA"),Normal==Again);
		TestEqual(TEXT("Original .20 enter"),M.EnterSeconds,.20f); TestEqual(TEXT("Original .18 exit"),M.ExitSeconds,.18f);
		for(int32 Y=0;Y<4;++Y)
		{
			TestEqual(TEXT("Inward AA at live-only boundary remains zero"),Live[Y*8+3].R,0.f);
			for(int32 X=4;X<8;++X) TestEqual(TEXT("Illegal samples strictly zero"),Live[Y*8+X].R,0.f);
		}
		TestEqual(TEXT("No empty fact manufactured"),M.GetCells()[1].VerifiedEmpty,0.f); return true;
	}
	FRoom F;
	if (!TestNotNull(TEXT("Runtime room"),F.Room)) return false;
	const bool Always = Case.StartsWith(TEXT("Always")) || Case.StartsWith(TEXT("ProjectDefault"));
	const bool Never = Case.StartsWith(TEXT("Never"));
	if (!Always) TestTrue(TEXT("Explicit single-object reset/registration"),F.Room->ResetTrackedPolicyForLab(Id,Never?Mode::Never:Mode::StationaryOnly));
	auto* Policy=F.Room->GetObjectPolicyForTesting(Id);
	TestTrue(TEXT("Resolved mode"),Policy->GetResolvedHistoryMode()==(Always?Mode::Always:Never?Mode::Never:Mode::StationaryOnly));
	F.Face(90); F.Step(30);
	TestEqual(TEXT("Real legal initial coverage"),F.Room->GetLastLegalCoverageRatioForTesting(Id),1.f);
	TestTrue(TEXT("Live source is shown"),F.Room->IsCurrentSourceVisibleForTesting(Id));
	if (Case==TEXT("MultiObjectPolicyIsolation"))
	{
		const FName Other(TEXT("Lab.Moving.Cabinet")), Third(TEXT("Lab.InWorld.Edge.Cabinet"));
		F.Room->ResetTrackedPolicyForLab(Third,Mode::Never);
		auto* A=F.Room->GetObjectPolicyForTesting(Other); auto* C=F.Room->GetObjectPolicyForTesting(Third);
		Policy->SetSightWeaveMoving(true);
		TestTrue(TEXT("Unconfigured neighbor is Always"),A->GetResolvedHistoryMode()==Mode::Always);
		TestFalse(TEXT("Motion never reaches neighbor"),A->IsSightWeaveMoving());
		TestFalse(TEXT("Never never arms"),C->IsHistoryEligible());
		const int32 OtherRecords=F.Room->GetSpatialRecordCount(Other);
		F.Room->ResetTrackedPolicyForLab(Third,Mode::Always);
		TestEqual(TEXT("Explicit reset is object-local"),F.Room->GetSpatialRecordCount(Other),OtherRecords);
		return true;
	}
	if (Case==TEXT("ExistingHistoryNotIdentityCleared"))
	{
		F.Face(-90); F.Step(30);
		TestEqual(TEXT("Prior stationary gray epoch sealed"),F.Room->GetStaleEpochCountForTesting(Id),1);
		const uint64 Signature=F.Room->GetHistoricalVisualSignatureForTesting(Id);
		F.Room->StartTrackedRotationForTesting(Id,180,4);
		F.Step(250);
		TestEqual(TEXT("Motion did not delete historical record"),F.Room->GetStaleEpochCountForTesting(Id),1);
		TestEqual(TEXT("Offscreen stationary history unchanged by identity/motion"),F.Room->GetHistoricalVisualSignatureForTesting(Id),Signature);
		return true;
	}
	if (Case==TEXT("StationaryOnlyContinuousLiveMotion") || Case==TEXT("ProjectDefaultPreservesCurrentBehavior"))
	{
		TestTrue(TEXT("Trace, prompt and real F binding starts rotation"),F.UseRotation()); F.Face(90);
		int32 Visible=0; TSet<int32> Angles;
		for(int32 I=0;I<310;++I)
		{
			F.Step(); Visible+=F.Room->IsCurrentSourceVisibleForTesting(Id)?1:0;
			Angles.Add(FMath::RoundToInt(F.Room->GetTrackedTransform(Id).Rotator().Yaw*10));
			TestEqual(TEXT("Continuously observed motion has no stale epoch"),F.Room->GetStaleEpochCountForTesting(Id),0);
		}
		TestEqual(TEXT("Every frame stays live"),Visible,310);
		TestTrue(TEXT("At least 80 intermediate poses"),Angles.Num()>=80);
		TestFalse(TEXT("Motion API ends"),Policy->IsSightWeaveMoving());
		TestTrue(TEXT("Final stationary observation rearmed"),Policy->IsHistoryEligible());
		F.Face(-90); F.Step(30);
		if(!Always) { TestEqual(TEXT("Only final stationary epoch sealed"),F.Room->GetStaleEpochCountForTesting(Id),1);
			TestTrue(TEXT("Final angle remembered, not origin"),FMath::Abs(F.Room->GetNewestHistoricalYawForTesting(Id))>179); }
		return true;
	}
	// Same observed/offscreen/partial-middle/hidden-completion route for all modes.
	TestTrue(TEXT("Start explicit four-second rotation"),F.Room->StartTrackedRotationForTesting(Id,180,4));
	for(int32 I=0;I<240;++I)
	{
		F.Face((I>=60&&I<66)||(I>=150&&I<156)?146:-90); F.Step();
		if(!Always)
		{
			TestEqual(TEXT("No new moving stale epoch"),F.Room->GetStaleEpochCountForTesting(Id),0);
			TestEqual(TEXT("No historical resources"),F.Room->GetHistoricalPresentationResourceCountForTesting(Id),0);
			TestTrue(TEXT("Transient current has no offscreen gray"),F.Room->CurrentHasOnlyLivePresentationForTesting(Id));
		}
	}
	F.Face(-90); F.Step(30);
	if(Always)
	{
		TestTrue(TEXT("Always retains legitimately observed intermediate epoch"),F.Room->GetStaleEpochCountForTesting(Id)>0);
		TestTrue(TEXT("Always still initializes fine history"),F.Room->GetFineHistoryTelemetry(Id).Contains(TEXT("fine=")));
		return true;
	}
	TestEqual(TEXT("Offscreen end cannot invent final observation"),F.Room->GetCurrentEpochCountForTesting(Id),0);
	TestFalse(TEXT("Hidden current source"),F.Room->IsCurrentSourceVisibleForTesting(Id));
	TestFalse(TEXT("No automatic history rearm"),Policy->IsHistoryEligible());
	for(int32 Cycle=0;Cycle<(Never?10:1);++Cycle)
	{
		F.Face(90); F.Step(30); TestTrue(TEXT("Reacquired Live works"),F.Room->IsCurrentSourceVisibleForTesting(Id));
		F.Face(-90); F.Step(30);
		TestEqual(TEXT("Only eligible stationary final can seal"),F.Room->GetStaleEpochCountForTesting(Id),Never?0:1);
		if(Never) { TestEqual(TEXT("Never retains no records"),F.Room->GetSpatialRecordCount(Id),0);
			TestEqual(TEXT("Never retains no history resources"),F.Room->GetHistoricalPresentationResourceCountForTesting(Id),0); }
	}
	return true;
}
#endif
