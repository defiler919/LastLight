#include "VisionPresentation/DarkwellApartmentBenchmark.h"
#include "VisionPresentation/DarkwellStaticEnvironmentSubsystem.h"
#include "VisionPresentation/DarkwellObjectMemoryScene.h"
#include "VisionPresentation/DarkwellGrayPolicyLab.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "Player/DarkwellCharacter.h"
#include "Player/DarkwellPlayerController.h"
#include "World/DarkwellDoor.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "UnrealClient.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "ProfilingDebugging/MiscTrace.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
namespace
{
// Exact pose-paired diagnostic, separate from the real-input 30 second run.
// Both processes build the same history before choosing the measured heading.
bool TickPaired(UWorld* W,ADarkwellObjectMemoryScene* S,bool Gray)
{
 auto* PC=Cast<ADarkwellPlayerController>(UGameplayStatics::GetPlayerController(W,0));
 auto* P=PC?Cast<ADarkwellCharacter>(PC->GetPawn()):nullptr;
 if(!P || !GEngine->GameViewport || !GEngine->GameViewport->Viewport)return true;
 static int Step=-1,ExitFrames=0;static FString Root,Lines;static double Prev=0;
 if(Step<0)
 {
  if(!FPlatformApplicationMisc::IsThisApplicationForeground())return true;
  FParse::Value(FCommandLine::Get(),TEXT("ApartmentBenchOutput="),Root);
  PC->SetActorTickEnabled(false);
  for(TActorIterator<ADarkwellDoor> It(W);It;++It)It->RestoreDoorState(DarkwellGameplayTags::State_World_Door_Open);
  FFileHelper::SaveStringToFile(S->GetStorageTelemetry(),*(Root/TEXT("initial-storage.txt")));
  Step=0;Prev=FPlatformTime::Seconds();Lines.Reserve(4*1024*1024);
 }
 if(Step>=600)
 {
  if(++ExitFrames==1)
  {
   FFileHelper::SaveStringToFile(Lines,*(Root/TEXT("frames.jsonl")));
   FFileHelper::SaveStringToFile(S->GetStorageTelemetry(),*(Root/TEXT("end-storage.txt")));
   FFileHelper::SaveStringToFile(TEXT("240 matched pose samples; scripted replay, not native WASD"),*(Root/TEXT("complete.txt")));
   FScreenshotRequest::RequestScreenshot(Root/TEXT("viewport.png"),false,false);
  }
  if(ExitFrames>=12)FPlatformMisc::RequestExit(false);
  return false;
 }
 if(!FPlatformApplicationMisc::IsThisApplicationForeground() || GEngine->GameViewport->Viewport->GetSizeXY()!=FIntPoint(1280,720))
 {FFileHelper::SaveStringToFile(TEXT("Invalid foreground/viewport"),*(Root/TEXT("invalid.txt")));FPlatformMisc::RequestExit(false);return false;}
 const double Now=FPlatformTime::Seconds();
 if(Step==360)FFileHelper::SaveStringToFile(S->GetStorageTelemetry(),*(Root/TEXT("start-storage.txt")));
 if(Step>=360)
 {
  const auto* Fog=W->GetSubsystem<UDarkwellFogVisualSubsystem>();const auto& Src=Fog->GetPublishedSource();
  const auto* D=GetDefault<ADarkwellSightWeaveGrayPolicyLabDirector>();
  const auto Pos=P->GetActorLocation();
  Lines+=FString::Printf(TEXT("{\"step\":%d,\"wall_ms\":%.4f,\"x\":%.6f,\"y\":%.6f,\"yaw\":%.4f,\"source_x\":%.6f,\"source_y\":%.6f,\"engine\":%s,\"static\":%s,\"memory\":%s}\n"),Step-360,(Now-Prev)*1000,Pos.X,Pos.Y,P->GetActorRotation().Yaw,Src.BodyCenter.X,Src.BodyCenter.Y,*D->GetFrameEnvironmentForTesting(),*W->GetSubsystem<UDarkwellStaticEnvironmentSubsystem>()->GetTelemetry(),*S->GetHistoryRuntimeTelemetry());
 }
 Prev=Now;
 FVector Pos;float Yaw;
 if(Step<240)
 {
  const FVector Poses[]={{-350,180,92},{0,180,92},{350,180,92},{350,300,92},{0,300,92},{-350,300,92}};
  Pos=Poses[Step/40];Yaw=Step<120?90:270;
 }
 else if(Step<300){Pos={-330,150,92};Yaw=200;}
 else
 {
  Pos={-330+25*FMath::Sin((Step-360)*PI/60.0),150,92};Yaw=Gray?20:200;
 }
 const FRotator R(0,Yaw,0);P->SetActorLocationAndRotation(Pos,R,false,nullptr,ETeleportType::TeleportPhysics);P->AimAtWorldPoint(Pos+R.Vector()*1000);
 ++Step;return true;
}
}
#endif

bool Darkwell::ApartmentBenchmark::Tick(UWorld* W,ADarkwellObjectMemoryScene* S)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
 static FString Mode; static const bool Enabled=FParse::Value(FCommandLine::Get(),TEXT("ApartmentBench="),Mode);
 if(!Enabled) return true;
 if(Mode==TEXT("PairGray") || Mode==TEXT("PairUnknown"))return TickPaired(W,S,Mode==TEXT("PairGray"));
 auto* PC=Cast<ADarkwellPlayerController>(UGameplayStatics::GetPlayerController(W,0));
 auto* P=PC?Cast<ADarkwellCharacter>(PC->GetPawn()):nullptr;
 if(!P || !GEngine->GameViewport || !GEngine->GameViewport->Viewport) return true;
 static double Start=0,Prev=0;static bool Measuring=false;static int32 Waypoint=0,Frames=0,MovingFrames=0;static FKey Held;
 static FString Lines,Root;static FVector Last;static double Distance=0,YawTravel=0;static float LastYaw=0;static bool Finished=false;
 if(Finished){static int ExitFrames=0;if(++ExitFrames>=12)FPlatformMisc::RequestExit(false);return false;}
 static const bool Black=FParse::Param(FCommandLine::Get(),TEXT("BenchBlack"));
 const double Now=FPlatformTime::Seconds();
 const FKey Keys[]{EKeys::W,EKeys::A,EKeys::S,EKeys::D};
 auto Key=[&](FKey K,EInputEvent Event){GEngine->GameViewport->InputKey(FInputKeyEventArgs(GEngine->GameViewport->Viewport,FInputDeviceId::CreateFromInternalId(0),K,Event,FPlatformTime::Cycles64()));};
 if(!Start)
 {
  if(!FPlatformApplicationMisc::IsThisApplicationForeground()) return true;
  FParse::Value(FCommandLine::Get(),TEXT("ApartmentBenchOutput="),Root);check(!Root.IsEmpty());IFileManager::Get().MakeDirectory(*Root,true);
  for(TActorIterator<ADarkwellDoor> It(W);It;++It) It->RestoreDoorState(DarkwellGameplayTags::State_World_Door_Open);
  P->SetActorLocation(FVector(Black?-180:0,Black?-100:30,92));
  Start=Prev=Now;Last=P->GetActorLocation();LastYaw=P->GetActorRotation().Yaw;
  Lines.Reserve(8*1024*1024);
  FFileHelper::SaveStringToFile(S->GetStorageTelemetry(),*(Root/TEXT("initial-storage.txt")));
 }
 const double T=Now-Start;
 static bool MidCapture=false;
 if(FParse::Param(FCommandLine::Get(),TEXT("ApartmentBenchMidGpuCapture")) && !MidCapture && T>=45){MidCapture=true;GEngine->Exec(W,TEXT("r.ProfileGPU.ShowUI 0"));GEngine->Exec(W,TEXT("profilegpu"));FScreenshotRequest::RequestScreenshot(Root/TEXT("during-measurement.png"),false,false);}
 const bool Stop=Mode==TEXT("Still") && T>=25;
 static double NavigationCheck=0; static FVector NavigationPosition;
 if(!Stop && Now-NavigationCheck>1.0){if(NavigationCheck>0 && FVector::Dist2D(NavigationPosition,P->GetActorLocation())<10)++Waypoint;NavigationPosition=P->GetActorLocation();NavigationCheck=Now;}
 const FVector2D ApartmentRoute[]{ {0,100},{-350,100},{-350,200},{-350,100},{0,100},{350,100},{350,180},{350,100},{0,100},{0,-300},{0,30} };
 const FVector2D BlackRoute[]{ {-250,-155},{-200,-155},{-200,-110},{-250,-110} };
 const FVector2D Goal=Black?BlackRoute[Waypoint%4]:ApartmentRoute[Waypoint%11];
 const FVector2D Delta=Goal-FVector2D(P->GetActorLocation());
 if(Delta.Size()<25) ++Waypoint;
 FKey Desired;
 if(!Stop) Desired=FMath::Abs(Delta.Y)>FMath::Abs(Delta.X)?(Delta.Y>0?EKeys::W:EKeys::S):(Delta.X>0?EKeys::A:EKeys::D);
 // Test-input navigation only: avoid pinning the real capsule against a wall.
 // No teleport, speed change, collision disabling or authority alteration.
 if(!Stop)
 {
  auto Clear=[&](FKey K){const FVector Direction=K==EKeys::W?FVector(0,1,0):K==EKeys::S?FVector(0,-1,0):K==EKeys::A?FVector(1,0,0):FVector(-1,0,0);FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(ApartmentBenchmarkInput),false,P);return !W->SweepSingleByChannel(Hit,P->GetActorLocation(),P->GetActorLocation()+Direction*80,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(P->GetCapsuleComponent()->GetScaledCapsuleRadius()),Q);};
  if(!Clear(Desired))
  {
   const FKey Other=(Desired==EKeys::W || Desired==EKeys::S)?(Delta.X>0?EKeys::A:EKeys::D):(Delta.Y>0?EKeys::W:EKeys::S);
   if(Clear(Other)) Desired=Other;
   else for(int I=0;I<4;++I) if(Clear(Keys[(I+int(T/2))%4])){Desired=Keys[(I+int(T/2))%4];break;}
  }
 }
 if(Desired!=Held){if(Held.IsValid())Key(Held,IE_Released);Held=Desired;if(Held.IsValid())Key(Held,IE_Pressed);}
 const double AimT=Stop?25:T;
 PC->SetMouseLocation(640+int32(240*FMath::Cos(AimT*1.1)),360+int32(180*FMath::Sin(AimT*1.1)));
 const double Wall=(Now-Prev)*1000;Prev=Now;
 if(T>=30 && !Measuring){Measuring=true;S->ResetHistoryRuntimeTelemetryForTesting();TRACE_BEGIN_REGION(TEXT("ApartmentMeasured"));FFileHelper::SaveStringToFile(S->GetStorageTelemetry(),*(Root/TEXT("start-storage.txt")));Last=P->GetActorLocation();LastYaw=P->GetActorRotation().Yaw;}
 if(Measuring)
 {
  const FIntPoint Size=GEngine->GameViewport->Viewport->GetSizeXY();
  if(!FPlatformApplicationMisc::IsThisApplicationForeground() || Size!=FIntPoint(1280,720))
  {FFileHelper::SaveStringToFile(TEXT("Invalid foreground or viewport"),*(Root/TEXT("invalid.txt")));FPlatformMisc::RequestExit(false);return false;}
  const double Step=FVector::Dist2D(Last,P->GetActorLocation());Distance+=Step;MovingFrames+=Step>0.1;Last=P->GetActorLocation();YawTravel+=FMath::Abs(FMath::FindDeltaAngleDegrees(LastYaw,P->GetActorRotation().Yaw));LastYaw=P->GetActorRotation().Yaw;
  const auto* D=GetDefault<ADarkwellSightWeaveGrayPolicyLabDirector>();
  const auto* Fog=W->GetSubsystem<UDarkwellFogVisualSubsystem>();const auto& FD=Fog->GetDiagnostics();
  static uint64 PreviousComputations=0,PreviousHits=0;
  const FString StaticJson=TEXT("\"static\":")+W->GetSubsystem<UDarkwellStaticEnvironmentSubsystem>()->GetTelemetry()+TEXT(",");
  const FString FogJson=FString::Printf(TEXT("\"fog\":{\"authority\":%llu,\"draws\":%llu,\"segments\":%d,\"computations_delta\":%llu,\"cache_hits_delta\":%llu},"),FD.LastAuthorityRevision,FD.CoverageDrawCount,FD.CachedOccluderSegmentCount,Fog->GetCoverageComputationsForTesting()-PreviousComputations,Fog->GetCoverageCacheHitsForTesting()-PreviousHits);
  PreviousComputations=Fog->GetCoverageComputationsForTesting();PreviousHits=Fog->GetCoverageCacheHitsForTesting();
  Lines+=FString::Printf(TEXT("{%s\"t\":%.4f,\"wall_ms\":%.4f,\"x\":%.3f,\"y\":%.3f,\"yaw\":%.3f,\"key\":\"%s\",\"engine\":%s,\"memory\":%s}\n"),*(StaticJson+FogJson),T,Wall,Last.X,Last.Y,P->GetActorRotation().Yaw,*Held.ToString(),*D->GetFrameEnvironmentForTesting(),*S->GetHistoryRuntimeTelemetry());++Frames;
 }
 if(T>=60)
 {
  TRACE_END_REGION(TEXT("ApartmentMeasured"));if(Held.IsValid())Key(Held,IE_Released);
  FFileHelper::SaveStringToFile(Lines,*(Root/TEXT("frames.jsonl")));
  FFileHelper::SaveStringToFile(S->GetStorageTelemetry(),*(Root/TEXT("end-storage.txt")));

  FFileHelper::SaveStringToFile(FString::Printf(TEXT("frames=%d moving_frames=%d distance_cm=%.2f yaw_travel_deg=%.2f mode=%s"),Frames,MovingFrames,Distance,YawTravel,*Mode),*(Root/TEXT("complete.txt")));
  if(Mode!=TEXT("Still") && (Distance<300 || MovingFrames*2<Frames))FFileHelper::SaveStringToFile(TEXT("Insufficient actual movement"),*(Root/TEXT("invalid.txt")));
  GEngine->Exec(W,TEXT("r.ProfileGPU.ShowUI 0"));GEngine->Exec(W,TEXT("profilegpu"));
  FScreenshotRequest::RequestScreenshot(Root/TEXT("viewport.png"),false,false);
  // Exit after diagnostics have rendered, outside the measurement interval.
  Finished=true;Measuring=false;
 }
 return true;
#else
 return true;
#endif
}
