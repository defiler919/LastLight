#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DarkwellLegacyObjectPolicyFixture.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Player/DarkwellCharacter.h"
#include "Interaction/DarkwellInteractionComponent.h"
#include "VisionPresentation/DarkwellMovingPropLabRoom.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
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

namespace Darkwell::GrayPolicyBaseline
{
 struct FMeasurements
 {
  TArray<double> Times;
  double RefreshUs=0,FineUs=0;
  uint64 Queries=0, Computations=0, CacheHits=0, Scans=0, Submissions=0, Uploads=0, Caps=0, Samples=0, Textures=0, Mids=0;
  void Add(const ADarkwellMovingPropLabRoom::FHistoryRuntimeTelemetry& P)
  {
   Times.Add(P.MovingPropLabGameThreadUs); Queries+=P.CoverageQueries; Computations+=P.CoverageComputations; CacheHits+=P.CoverageCacheHits; Scans+=P.FineSamplesScanned;
   Submissions+=P.TextureUploads; Uploads+=P.GpuTextureUploads; Caps+=P.CapMeshRebuilds;
   RefreshUs+=P.RefreshContributionDiagnosticsUs; FineUs+=P.AdvanceFineHistoryUs; Samples+=P.CurrentSamplesTouched; Textures+=P.TextureCreations; Mids+=P.MidCreations;
  }
  FString Report(const FString& Case,int32 Frame,int32 Tracked,const ADarkwellMovingPropLabRoom::FHistoryRuntimeTelemetry& P)
  {
   double Sum=0; for(double T:Times) Sum+=T; Times.Sort(); const int32 N=Times.Num();
   return FString::Printf(TEXT("GRAY_HOME_PERF case=%s frame=%d tracked=%d mean_us=%.3f p50_us=%.3f p95_us=%.3f p99_us=%.3f peak_us=%.3f queries_per_frame=%.3f computations_per_frame=%.3f cache_hits_per_frame=%.3f current_samples_per_frame=%.3f history_scans=%llu submissions=%llu gpu_uploads=%llu texture_creations=%llu mid_creations=%llu cap_rebuilds=%llu active_history=%d records=%d proxies=%d caps=%d textures=%d mids=%d uobjects=%d live_uobjects=%d working_set=%llu refresh_mean_us=%.3f fine_mean_us=%.3f"),
    *Case,Frame,Tracked,Sum/N,Times[N/2],Times[FMath::Min(N-1,int32(N*.95))],Times[FMath::Min(N-1,int32(N*.99))],Times.Last(),double(Queries)/N,double(Computations)/N,double(CacheHits)/N,double(Samples)/N,Scans,Submissions,Uploads,Textures,Mids,Caps,P.ActiveHistoricalEpochs,P.SpatialRecordCount,P.ProxyCount,P.CapComponentCount,P.TextureCount,P.MidCount+P.SourceMidCount,P.UObjectCount,P.LiveUObjectCount,P.ProcessWorkingSetBytes,RefreshUs/N,FineUs/N);
  }
 };
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FDarkwellGrayHomeBaseline,
 "Darkwell.PropLab.GrayHomeBaseline", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FDarkwellGrayHomeBaseline::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
 for(const TCHAR* N : {TEXT("OneChangedView"),TEXT("EightChangedView"),TEXT("ThirtyTwoChangedView"),TEXT("OneMoving"),TEXT("EightMoving"),TEXT("HistoryGrowth"),TEXT("FifteenMinuteInteraction")})
 { Names.Add(N); Commands.Add(N); }
}
bool FDarkwellGrayHomeBaseline::RunTest(const FString& Case)
{
 using namespace Darkwell::GrayPolicyBaseline;
 FRoom F;
 auto* Fog=F.World->GetSubsystem<UDarkwellFogVisualSubsystem>();
 const bool Soak=Case==TEXT("FifteenMinuteInteraction"), Growth=Case==TEXT("HistoryGrowth");
 const bool Moving=Case.Contains(TEXT("Moving")) || Soak;
 const int32 Count=Case.StartsWith(TEXT("ThirtyTwo"))?32:Case.StartsWith(TEXT("Eight"))?8:1;
 if(!Soak && !Growth) TestTrue(TEXT("Isolated count fixture"),F.Room->SetMultiCount(Count,F.Player));
 if(!Soak && !Growth && (Moving || Count==1))
 {
  for(int32 J=0;J<Count;++J)
  {
   const FName Target(*FString::Printf(TEXT("Lab.Multi.%02d"),J));
   FTransform Pose=F.Room->GetTrackedTransform(Target);
   Pose.SetLocation(FVector(Count==1?0:-300+(J%4)*200,500+(J/4)*160,0));
   TestTrue(TEXT("Visible compact motion fixture"),F.Room->SetTrackedTransformForTesting(Target,Pose));
  }
 }
 F.Step(30);
 if(!Soak && !Growth && (Moving || Count==1))
  for(int32 J=0;J<Count;++J)
   TestTrue(TEXT("Every performance target starts legally visible"),F.Room->IsCurrentSourceVisibleForTesting(FName(*FString::Printf(TEXT("Lab.Multi.%02d"),J))));
 double WallBudget=0;
 FParse::Value(FCommandLine::Get(),TEXT("GrayBaselineWallBudgetSeconds="),WallBudget);
 bool bBudgetExceeded=false;
 auto Measure=[&](const FString& Label,int32 Frames,bool Animate)
 {
  const double Begin=FPlatformTime::Seconds();
  double LastReportTime=Begin;
  FMeasurements M;
  TArray<FTransform> MotionStarts;
  for(int32 I=0;I<Frames;++I)
  {
   if(Animate)
   {
    if(Moving && I%360==0)
    {
     if(Soak) TestTrue(TEXT("Continuous room motion starts"),F.Room->StartTrackedRotationForTesting(Id,((I/360)%2)?0:180,4));
     else
     {
      MotionStarts.Reset();
      for(int32 J=0;J<Count;++J)
      {
       const FName Target(*FString::Printf(TEXT("Lab.Multi.%02d"),J));
       MotionStarts.Add(F.Room->GetTrackedTransform(Target));
       F.Room->GetObjectPolicyForTesting(Target)->SetSightWeaveMoving(true);
      }
     }
    }
    // The single-rotation helper intentionally rejects an already active group.
    // Drive every independent prop through the same explicit motion/pose APIs.
    if(Moving && !Soak && I%360<240)
     for(int32 J=0;J<Count;++J)
     {
      const FName Target(*FString::Printf(TEXT("Lab.Multi.%02d"),J));
      FTransform Pose=MotionStarts[J];
      Pose.SetRotation(FQuat(FRotator(0,Pose.Rotator().Yaw+180.f*(I%360+1)/240.f,0)));
      TestTrue(TEXT("Every moving object receives a real pose update"),F.Room->SetTrackedTransformForTesting(Target,Pose));
      if(I%360==239) F.Room->GetObjectPolicyForTesting(Target)->SetSightWeaveMoving(false);
     }
    F.Player->SetActorRotation(FRotator(0,Soak?90.f+80.f*FMath::Sin(I*.012f):float(I%720)*.5f,0));
   }
   F.Step(); M.Add(F.Room->GetHistoryRuntimeFrameTelemetryForTesting());
   const bool Stop=Soak && WallBudget>0 && FPlatformTime::Seconds()-Begin>=WallBudget;
   if((I+1)%3600==0 || I+1==Frames || Stop || (Soak && FPlatformTime::Seconds()-LastReportTime>=60))
   {
    const FString Row=M.Report(Label,I+1,F.Room->GetTrackedIdentityCount(),F.Room->GetHistoryRuntimeFrameTelemetryForTesting());
    // Persist every simulated minute even if an expensive diagnostic run is interrupted.
    UE_LOG(LogTemp,Display,TEXT("%s wall_seconds=%.3f"),*Row,FPlatformTime::Seconds()-Begin); AddInfo(Row); M=FMeasurements{}; LastReportTime=FPlatformTime::Seconds();
   }
   if(Stop) { bBudgetExceeded=true; AddError(TEXT("Soak deliberately interrupted at diagnostic wall budget; NOT a fifteen-minute pass. Completed samples retained.")); break; }
  }
 };
 if(Growth)
 {
  for(int32 Epochs : {1,2,4,8})
  {
   TestTrue(TEXT("Explicit synthetic history fixture (not natural interaction)"),F.Room->ConfigureHistoricalEpochCountForTesting(Id,Epochs));
   F.Face(-90); F.Step(30);
   Measure(FString::Printf(TEXT("History%dIdle"),Epochs),120,false);
   Measure(FString::Printf(TEXT("History%dChangedView"),Epochs),120,true);
  }
 }
 else
 {
  Measure(Case,Soak?54000:600,true);
  F.Room->StopMotion(); F.Step(60);
  Measure(Case+TEXT("Idle"),600,false);
 }
 // Forensic hashing has a cost, so this frame is deliberately outside all GT timings.
 F.Player->SetActorRotation(FRotator(0,91,0)); F.Adapter->Tick(1.f/60);
 Fog->BeginCoverageAuditForTesting(); F.Room->UpdateRoom(1.f/60,F.Player);
 uint64 Queries=0,Duplicates=0; Fog->EndCoverageAuditForTesting(Queries,Duplicates);
 AddInfo(FString::Printf(TEXT("GRAY_HOME_AUDIT case=%s queries=%llu duplicates=%llu (one separately instrumented frame)"),*Case,Queries,Duplicates));
 return !bBudgetExceeded;
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
 FDarkwellLegacyObjectPolicyFixture LegacyPolicy;
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
