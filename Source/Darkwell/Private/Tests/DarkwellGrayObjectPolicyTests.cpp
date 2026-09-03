#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Player/DarkwellCharacter.h"
#include "VisionPresentation/DarkwellMovingPropLabRoom.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "Visibility/SightWeave/DarkwellSightWeaveWorldSubsystem.h"

namespace Darkwell::GrayObjectPolicyTests
{
 const FName Id(TEXT("Lab.InWorld.Rotate.Cabinet"));
 using Reveal=ESightWeaveRevealMode;
 using History=ESightWeaveHistoryMode;
 struct FRoom
 {
  UWorld* World;
  ADarkwellCharacter* Player;
  ADarkwellPropGameplayLab* Fixture;
  ADarkwellMovingPropLabRoom* Room;
  UDarkwellSightWeaveWorldSubsystem* Adapter;
  FRoom()
  {
   UPackage* Package=CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab"));
   World=NewObject<UWorld>(Package,MakeUniqueObjectName(Package,UWorld::StaticClass(),TEXT("GrayObjectPolicy")),RF_Transient);
   World->WorldType=EWorldType::Game; GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
   World->InitializeNewWorld(UWorld::InitializationValues().InitializeScenes(true).AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false));
   World->URL.AddOption(TEXT("PropLabOriginal")); World->URL.AddOption(TEXT("InWorldControls"));
   FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   Player=World->SpawnActor<ADarkwellCharacter>(ADarkwellCharacter::StaticClass(),FVector(-300,100,92),FRotator(0,90,0),P); Player->DispatchBeginPlay();
   Fixture=World->SpawnActor<ADarkwellPropGameplayLab>(); Fixture->PostInitializeComponents(); Fixture->DispatchBeginPlay();
   Room=ADarkwellMovingPropLabRoom::FindActive(World); Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
   Adapter->RequestSightWeaveAuthority(Fixture); Room->ResetRoom(Player); Face(90);
  }
  ~FRoom() { Fixture->Destroy(); World->DestroyWorld(true); GEngine->DestroyWorldContext(World); }
  void Face(float Yaw) { Player->SetActorLocation(FVector(-300,100,92)); Player->SetActorRotation(FRotator(0,Yaw,0)); }
  void Step(int32 Frames=1,float Dt=1.f/60) { for(int32 I=0;I<Frames;++I) { Adapter->Tick(Dt); Room->UpdateRoom(Dt,Player); Fixture->Tick(Dt); } }
 };
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FDarkwellGrayObjectPolicyTest,"Darkwell.PropLab.GrayObjectPolicy",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FDarkwellGrayObjectPolicyTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
 for(const TCHAR* N : {TEXT("WholeObjectBelowThresholdNoHistory"),TEXT("WholeObjectThresholdConfirmsFullObject"),
  TEXT("WholeObjectConfirmedStaticHistory"),TEXT("WholeObjectStationaryOnlyNoMovingHistory"),
  TEXT("WholeObjectNeverNoHistoryResources"),TEXT("WholeObjectFastSlowConfirmationEquivalent"),
  TEXT("WholeObjectBehindWallDoesNotConfirm"),TEXT("WholeObjectDoesNotExpandWorldCoverage"),
  TEXT("ConfirmedPersistsThroughRigidMotion"),TEXT("InvalidCoverageDoesNotResetTentativeSession")})
 { Names.Add(N); Commands.Add(N); }
}
bool FDarkwellGrayObjectPolicyTest::RunTest(const FString& Case)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 FRoom F;
 const bool Never=Case.Contains(TEXT("Never"));
 TestTrue(TEXT("Per-object registration"),F.Room->ResetTrackedRevealPolicyForLab(Id,Reveal::WholeObjectAfterSpan,100,Never?History::Never:History::StationaryOnly));
 if(Case==TEXT("WholeObjectBehindWallDoesNotConfirm"))
 {
  F.Face(-90); F.Step(30);
  TestFalse(TEXT("No legal contact cannot confirm"),F.Room->IsRevealConfirmedForTesting(Id));
  const FName Hidden(TEXT("Lab.InWorld.Hidden.Cabinet"));
  AddInfo(F.Room->GetRevealPolicyTelemetry(Hidden));
  // Put the tested real cabinet behind the existing opaque divider, then aim at it.
  const auto Pose=F.Room->GetTrackedTransform(Hidden);
  F.Room->SetTrackedTransformForTesting(Id,Pose);
  F.Player->SetActorLocation(FVector(500,-600,92)); F.Player->SetActorRotation((Pose.GetLocation()-F.Player->GetActorLocation()).Rotation()); F.Step(30);
  TestEqual(TEXT("Existing wall rejects legal contact"),F.Room->GetLastLegalCoverageRatioForTesting(Id),0.f);
  TestFalse(TEXT("Wall-hidden cabinet never confirms"),F.Room->IsRevealConfirmedForTesting(Id)); return true;
 }
 if(Case==TEXT("WholeObjectBelowThresholdNoHistory") || Case==TEXT("InvalidCoverageDoesNotResetTentativeSession"))
 {
  F.Face(146); F.Step(1);
  AddInfo(F.Room->GetRevealPolicyTelemetry(Id));
  TestTrue(TEXT("Real partial contact"),F.Room->GetLastLegalCoverageRatioForTesting(Id)>0);
  TestFalse(TEXT("Partial span below 100 cm"),F.Room->IsRevealConfirmedForTesting(Id));
  const FString Before=F.Room->GetRevealPolicyTelemetry(Id);
  if(Case.StartsWith(TEXT("Invalid")))
  {
   F.Room->InjectInvalidCoverageOnceForTesting(Id); F.Step();
   TestEqual(TEXT("Invalid publication retains tentative progress"),F.Room->GetRevealPolicyTelemetry(Id),Before);
   TestFalse(TEXT("Invalid source is hidden"),F.Room->IsCurrentSourceVisibleForTesting(Id));
  }
  F.Face(-90); F.Step(30);
  TestEqual(TEXT("Unconfirmed Whole has no history resources"),F.Room->GetHistoricalPresentationResourceCountForTesting(Id),0);
  TestEqual(TEXT("Unconfirmed Whole has no stale epochs"),F.Room->GetStaleEpochCountForTesting(Id),0);
  TestTrue(TEXT("Real view loss clears tentative mask"),F.Room->GetRevealPolicyTelemetry(Id).Contains(TEXT("\"tentative_samples\":0")));
  return true;
 }
 F.Face(90); F.Step(30);
 TestTrue(TEXT("Threshold reached"),F.Room->IsRevealConfirmedForTesting(Id));
 TestEqual(TEXT("Whole source presents every pixel after normal entry"),F.Room->GetCurrentPresentationMinimumForTesting(Id),1.f);
 if(Case==TEXT("WholeObjectThresholdConfirmsFullObject") || Case==TEXT("WholeObjectDoesNotExpandWorldCoverage"))
 {
  F.Face(146); F.Step(30);
  TestTrue(TEXT("Legal world field still partial"),F.Room->GetLastLegalCoverageRatioForTesting(Id)>0 && F.Room->GetLastLegalCoverageRatioForTesting(Id)<1);
  TestEqual(TEXT("Object-only confirmed permission remains full"),F.Room->GetCurrentPresentationMinimumForTesting(Id),1.f);
  const auto WorldCoverage=F.World->GetSubsystem<UDarkwellFogVisualSubsystem>()->QueryLiveCoverageAtWorldPoint(FVector2D(-230,650));
  TestTrue(TEXT("Full object presentation does not explore its out-of-cone world position"),WorldCoverage.bValid && WorldCoverage.Coverage<.99f);
  return true;
 }
 if(Case==TEXT("WholeObjectFastSlowConfirmationEquivalent"))
 {
  for(float Dt : {1.f/30,1.f/60,1.f/120,1.f/144})
  {
   F.Room->ResetTrackedRevealPolicyForLab(Id,Reveal::WholeObjectAfterSpan,100,History::StationaryOnly);
   F.Face(146); F.Step(1,Dt); TestFalse(TEXT("Short partial contact below threshold at every rate"),F.Room->IsRevealConfirmedForTesting(Id));
   F.Face(90); F.Step(1,Dt); TestTrue(TEXT("Same legal span confirms at every rate, independent of appearance"),F.Room->IsRevealConfirmedForTesting(Id));
  }
  return true;
 }
 if(Case.Contains(TEXT("Moving")) || Case.Contains(TEXT("RigidMotion")))
 {
  TestTrue(TEXT("Start real rotation"),F.Room->StartTrackedRotationForTesting(Id,180,4));
  F.Step(30); TestTrue(TEXT("Rigid motion retains confirmation"),F.Room->IsRevealConfirmedForTesting(Id));
  F.Face(-90); F.Step(270);
  TestEqual(TEXT("No moving history"),F.Room->GetHistoricalPresentationResourceCountForTesting(Id),0);
  TestTrue(TEXT("Hidden rigid motion retains registration confirmation"),F.Room->IsRevealConfirmedForTesting(Id));
  F.Face(146); F.Step(30); TestTrue(TEXT("Reacquisition needs no second span confirmation"),F.Room->IsRevealConfirmedForTesting(Id));
  return true;
 }
 F.Face(-90); F.Step(30);
 TestEqual(TEXT("Static capture mode enforced"),F.Room->GetStaleEpochCountForTesting(Id),Never?0:1);
 if(Never) TestEqual(TEXT("Never allocates no historical presentation"),F.Room->GetHistoricalPresentationResourceCountForTesting(Id),0);
 else TestTrue(TEXT("Confirmed full static gray history exists"),F.Room->GetNewestHistoricalDiscoveredCellCountForTesting(Id)>0);
 return true;
}
#endif
