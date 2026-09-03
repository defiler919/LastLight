#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Player/DarkwellCharacter.h"
#include "Interaction/DarkwellInteractionComponent.h"
#include "VisionPresentation/DarkwellMovingPropLabRoom.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "Visibility/SightWeave/DarkwellSightWeaveWorldSubsystem.h"

namespace Darkwell::GrayPolicyBaseline
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
			World = NewObject<UWorld>(Package, MakeUniqueObjectName(Package, UWorld::StaticClass(), TEXT("GrayPolicyBaseline")), RF_Transient);
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

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FDarkwellGrayPolicyBaseline,
 "Darkwell.PropLab.GrayPolicyBaseline", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FDarkwellGrayPolicyBaseline::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
 for (const TCHAR* N : {TEXT("OneMoving"),TEXT("EightChangedView"),TEXT("ThirtyTwoChangedView"),TEXT("StaticPartialPositiveAndNeverNegative"),TEXT("FifteenMinuteInteraction")})
 { Names.Add(N); Commands.Add(N); }
}
bool FDarkwellGrayPolicyBaseline::RunTest(const FString& Case)
{
 using namespace Darkwell::GrayPolicyBaseline;
 FRoom F;
 if(Case==TEXT("StaticPartialPositiveAndNeverNegative"))
 {
  for(auto M : {Mode::StationaryOnly,Mode::Never})
  {
   F.Room->ResetTrackedPolicyForLab(Id,M); F.Face(146); F.Step(30);
   const float Ratio=F.Room->GetLastLegalCoverageRatioForTesting(Id);
   TestTrue(TEXT("Real partial legal contact"),Ratio>0 && Ratio<1);
   F.Face(-90); F.Step(30);
   const int32 Stale=F.Room->GetStaleEpochCountForTesting(Id);
   AddInfo(FString::Printf(TEXT("GRAY_BASELINE_STATIC mode=%d coverage=%f stale=%d caps=%d resources=%d"),int32(M),Ratio,Stale,F.Room->GetVisibleHistoricalCapCountForTesting(Id),F.Room->GetHistoricalPresentationResourceCountForTesting(Id)));
   TestEqual(TEXT("Stationary positive versus Never negative"),Stale,M==Mode::Never?0:1);
  }
  return true;
 }
 const bool Soak=Case==TEXT("FifteenMinuteInteraction");
 const int32 Count=Case==TEXT("ThirtyTwoChangedView")?32:Case==TEXT("EightChangedView")?8:1;
 if(Count>1) TestTrue(TEXT("Existing stress fixture"),F.Room->SetMultiCount(Count,F.Player));
 F.Step(30);
 const int32 Frames=Soak?15*60*60:600;
 TArray<double> Times; Times.Reserve(3600);
 uint64 Queries=0,Scans=0,Uploads=0,Caps=0;
 for(int32 I=0;I<Frames;++I)
 {
  if(Count==1 && I%360==0) F.Room->StartTrackedRotationForTesting(Id,((I/360)%2)?0:180,4);
  const float Yaw=Count>1?float(I%720)*.5f:Soak?90.f+80.f*FMath::Sin(I*.012f):90.f;
  F.Player->SetActorRotation(FRotator(0,Yaw,0)); F.Step();
  const auto P=F.Room->GetHistoryRuntimeFrameTelemetryForTesting();
  Times.Add(P.MovingPropLabGameThreadUs); Queries+=P.CoverageQueries; Scans+=P.FineSamplesScanned; Uploads+=P.TextureUploads; Caps+=P.CapMeshRebuilds;
  if((I+1)%3600==0 || I+1==Frames)
  {
   double Sum=0;for(double T:Times)Sum+=T;Times.Sort();const int32 N=Times.Num();
   AddInfo(FString::Printf(TEXT("GRAY_POLICY_PERF case=%s frame=%d tracked=%d mean_us=%.3f p50_us=%.3f p95_us=%.3f p99_us=%.3f peak_us=%.3f queries_per_frame=%.3f history_scans=%llu uploads=%llu cap_rebuilds=%llu uobjects=%d working_set=%llu textures=%d mids=%d proxies=%d records=%d"),
    *Case,I+1,F.Room->GetTrackedIdentityCount(),Sum/N,Times[N/2],Times[FMath::Min(N-1,int32(N*.95))],Times[FMath::Min(N-1,int32(N*.99))],Times.Last(),double(Queries)/N,Scans,Uploads,Caps,P.UObjectCount,P.ProcessWorkingSetBytes,P.TextureCount,P.MidCount,P.ProxyCount,P.SpatialRecordCount));
   Times.Reset();Queries=Scans=Uploads=Caps=0;
  }
 }
 return true;
}
#endif
