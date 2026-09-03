#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UObject/UObjectIterator.h"
#include "UObject/GarbageCollection.h"
#include "SightWeaveObjectPolicy.h"
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
  FRoom(bool GrayFixture=false)
  {
   UPackage* Package=CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab"));
   World=NewObject<UWorld>(Package,MakeUniqueObjectName(Package,UWorld::StaticClass(),TEXT("GrayObjectPolicy")),RF_Transient);
   World->WorldType=EWorldType::Game; GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
   World->InitializeNewWorld(UWorld::InitializationValues().InitializeScenes(true).AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false));
   World->URL.AddOption(TEXT("PropLabOriginal")); World->URL.AddOption(TEXT("InWorldControls"));
   if(GrayFixture) World->URL.AddOption(TEXT("GrayObjectPolicies"));
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
  TEXT("ConfirmedPersistsThroughRigidMotion"),TEXT("InvalidCoverageDoesNotResetTentativeSession"),
  TEXT("SpatialPartialStaticKeepsObservedGrayRegion"),TEXT("SpatialPartialStaticKeepsLegalCap"),
  TEXT("SpatialPartialDoesNotRequireConfirmation"),TEXT("SpatialPartialStationaryOnlyNoMovingHistory"),
  TEXT("SpatialPartialNeverNoHistory"),TEXT("SpatialPartialFrozenMaskMatchesKnowledgeMask"),
  TEXT("SpatialPartialAppearanceBlendDoesNotControlKnowledge"),TEXT("StaticWholeStationaryOnlyRetainsGray"),
  TEXT("StaticPartialStationaryOnlyRetainsGray"),TEXT("StaticNeverDoesNotRetainGray"),
  TEXT("CoverageEdgeNeverIsExpectedNegativeControl"),TEXT("SixPolicyCombinationsCoexist"),
  TEXT("MotionStateAndRevealPolicyIsolation"),TEXT("ExistingHistoryNotIdentityCleared"),
  TEXT("ResetClearsOnlyTarget"),TEXT("PlayStopResourceLifetime"),TEXT("CanonicalRasterMatchesOriginalSamples"),TEXT("ConfirmedWholeStopsSpanAndDenseObservationWork"),TEXT("CachedDiagnosticsMatchForcedDiagnostics"),TEXT("RepeatedPoseDiagnosticsMatchFullScan"),TEXT("FramePhysicalCacheMatchesGeometryOracle"),TEXT("WholeObjectConfirmedRespectsPartialWall")})
 { Names.Add(N); Commands.Add(N); }
}
bool FDarkwellGrayObjectPolicyTest::RunTest(const FString& Case)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 if(Case==TEXT("PlayStopResourceLifetime"))
 {
  TArray<TWeakObjectPtr<UObject>> Owned;
  for(int32 Cycle=0;Cycle<3;++Cycle)
  {
   { FRoom Life;
     if(Cycle==1) Life.Room->ResetTrackedRevealPolicyForLab(Id,Reveal::SpatialPartial,100,History::StationaryOnly);
     Life.Face(Cycle==1?146:90); Life.Step(15); Life.Face(-90); Life.Step(15);
     if(Cycle==1) TestTrue(TEXT("Positive partial cap allocation before teardown"),Life.Room->GetVisibleHistoricalCapCountForTesting(Id)>0);
     TestTrue(TEXT("Positive historical resource allocation"),Life.Room->GetHistoricalPresentationResourceCountForTesting(Id)>0);
     auto Resources=Life.Room->GetOwnedPresentationObjectsForTesting();
     TestTrue(TEXT("Track concrete source, texture, material, proxy and cap allocations"),Resources.Num()>Life.Room->GetTrackedIdentityCount()*2);
     Owned.Append(Resources);
     Owned.Add(Life.World); Owned.Add(Life.Room); Owned.Add(Life.Room->GetObjectPolicyForTesting(Id)); }
   CollectGarbage(RF_NoFlags);
   for(auto P:Owned) TestFalse(TEXT("Destroyed world, room, policies and concrete presentation objects are reclaimed"),P.IsValid());
  }
  return true;
 }
 const bool Matrix=Case.Contains(TEXT("Coexist")) || Case.Contains(TEXT("Isolation")) || Case.Contains(TEXT("ResetClears")) || Case.StartsWith(TEXT("Static")) || Case.StartsWith(TEXT("CoverageEdge"));
 FRoom F(Matrix);
 if(Case==TEXT("FramePhysicalCacheMatchesGeometryOracle"))
 {
  F.Face(90); F.Step(30); F.Face(-90); F.Step(30);
  const auto Initial=F.Room->GetTrackedTransform(Id);
  for(float Angle:{0.f,7.f,45.f,89.f,135.f,180.f})
  {
   auto Pose=Initial; Pose.SetRotation(FQuat(FRotator(0,Angle,0))); F.Room->SetTrackedTransformForTesting(Id,Pose); F.Step();
   TestTrue(TEXT("Cached physical primitives match direct live geometry at rotated historical samples"),F.Room->DoesFrameOccupancyMatchOracleForTesting(Id));
  }
  return true;
 }
 if(Case==TEXT("CachedDiagnosticsMatchForcedDiagnostics") || Case==TEXT("RepeatedPoseDiagnosticsMatchFullScan"))
 {
  if(Case==TEXT("RepeatedPoseDiagnosticsMatchFullScan"))
  {
   F.Room->ResetTrackedRevealPolicyForLab(Id,Reveal::SpatialPartial,100,History::StationaryOnly);
   for(int32 Cycle=0;Cycle<8;++Cycle) { F.Face(146); F.Step(12); F.Face(-90); F.Step(12); }
   TestTrue(TEXT("Repeated real observation retains several epochs at the same pose"),F.Room->GetSpatialRecordCount(Id)>=4);
   TestTrue(TEXT("Partial history exercises cap contributor diagnostics"),F.Room->GetVisibleHistoricalCapCountForTesting(Id)>0);
  }
  else
  {
   F.Room->ResetTrackedRevealPolicyForLab(Id,Reveal::SpatialPartial,100,History::Always);
   F.Face(90); F.Step(30);
   TestTrue(TEXT("Multiple independent history records"),F.Room->ConfigureHistoricalEpochCountForTesting(Id,2));
  }
  for(float Yaw:{-90.f,90.f,135.f,146.f,150.f,270.f,270.f})
  {
   F.Face(Yaw); F.Step(3);
   const auto Surface=F.Room->GetMaxSurfaceContributorsForTesting(Id),Cap=F.Room->GetMaxCapContributorsForTesting(Id);
   const auto Diagnosis=F.Room->Get3DOwnershipTelemetryForTesting(Id);
   F.Room->ForceContributionRefreshForTesting(Id);
   TestEqual(TEXT("Cached surface contributors match forced full recalculation"),F.Room->GetMaxSurfaceContributorsForTesting(Id),Surface);
   TestEqual(TEXT("Cached cap index preserves contributor count"),F.Room->GetMaxCapContributorsForTesting(Id),Cap);
   TestEqual(TEXT("Cached 3D diagnostics match forced recalculation"),F.Room->Get3DOwnershipTelemetryForTesting(Id),Diagnosis);
  }
  return true;
 }
 if(Case==TEXT("CanonicalRasterMatchesOriginalSamples"))
 {
  auto* Fog=F.World->GetSubsystem<UDarkwellFogVisualSubsystem>();
  const FBox2D B(FVector2D(-375,612.5),FVector2D(-225,692.5)); const FIntPoint Size(61,33);
  const auto Step=B.GetSize()/FVector2D(Size);
  for(float Yaw:{-90.f,0.f,90.f,135.f,146.f,150.f,270.f})
  {
   F.Face(Yaw); F.Step(); TArray<float> Values,Again; uint64 Requests=0;
   const auto Q=Fog->QueryCanonicalCoverageRaster(B,Size,Values,Requests);
   TestTrue(TEXT("Valid canonical publication"),Q.bValid);
   for(int32 Y=0;Y<Size.Y;++Y) for(int32 X=0;X<Size.X;++X)
   {
    float Oracle=1;
    for(auto O:{FVector2D(0),FVector2D(1,0),FVector2D(0,1),FVector2D(1),FVector2D(.5)})
     Oracle=FMath::Min(Oracle,Fog->QueryLiveCoverageAtWorldPoint(B.Min+Step*(FVector2D(X,Y)+O)).Coverage);
    if(!TestEqual(TEXT("Canonical raster equals original five exact samples"),Values[Y*Size.X+X],Oracle)) return false;
   }
   const auto Computations=Fog->GetCoverageComputationsForTesting(); const uint64 BeforeRequests=Requests;
   Fog->QueryCanonicalCoverageRaster(B,Size,Again,Requests);
   TestTrue(TEXT("Raster shared without repeat point computations"),Again==Values && Requests==BeforeRequests && Fog->GetCoverageComputationsForTesting()==Computations);
  }
  return true;
 }
 if(Matrix)
 {
  TestEqual(TEXT("Six explicit static controls coexist with original nine"),F.Room->GetTrackedIdentityCount(),15);
  for(int32 I=0;I<6;++I)
  {
   const FName S(*FString::Printf(TEXT("Lab.Gray.Static.%d"),I)); auto* P=F.Room->GetObjectPolicyForTesting(S);
   TestTrue(TEXT("Per-object reveal"),P->GetResolvedRevealMode()==(I<3?Reveal::WholeObjectAfterSpan:Reveal::SpatialPartial));
   TestTrue(TEXT("Per-object history"),P->GetResolvedHistoryMode()==(I%3==0?History::Always:I%3==1?History::StationaryOnly:History::Never));
  }
  if(Case==TEXT("SixPolicyCombinationsCoexist"))
  {
   const FName Edge(TEXT("Lab.InWorld.Edge.Cabinet"));
   TestTrue(TEXT("Explicit reset overrides launch fixture"),F.Room->ResetTrackedPolicyForLab(Edge,History::Always));
   TestTrue(TEXT("Launch defaults cannot override explicit object policy"),F.Room->GetObjectPolicyForTesting(Edge)->GetResolvedHistoryMode()==History::Always); return true;
  }
  if(Case.Contains(TEXT("Isolation")) || Case.Contains(TEXT("ResetClears")))
  {
   F.Face(90); F.Step(30); TestTrue(TEXT("Target confirms"),F.Room->IsRevealConfirmedForTesting(Id));
   const FName Other(TEXT("Lab.Gray.Static.1")); auto* P=F.Room->GetObjectPolicyForTesting(Other);
   TestFalse(TEXT("Unseen object's confirmation independent"),F.Room->IsRevealConfirmedForTesting(Other));
   F.Room->GetObjectPolicyForTesting(Id)->SetSightWeaveMoving(true);
   TestFalse(TEXT("Motion remains object-local"),P->IsSightWeaveMoving());
   const FString Before=F.Room->GetRevealPolicyTelemetry(Other);
   F.Room->ResetTrackedRevealPolicyForLab(Id,Reveal::SpatialPartial,1,History::Never);
   TestEqual(TEXT("Neighbor registration state untouched"),F.Room->GetRevealPolicyTelemetry(Other),Before);
   TestFalse(TEXT("Reset clears target confirmation"),F.Room->IsRevealConfirmedForTesting(Id)); return true;
  }
  const FName Target=Case.StartsWith(TEXT("CoverageEdge"))?FName(TEXT("Lab.InWorld.Edge.Cabinet")):
   FName(Case.Contains(TEXT("Whole"))?TEXT("Lab.Gray.Static.1"):Case.Contains(TEXT("Never"))?TEXT("Lab.Gray.Static.5"):TEXT("Lab.Gray.Static.4"));
  // Reposition only this control into the established legal-contact fixture.
  F.Room->SetTrackedTransformForTesting(Target,F.Room->GetTrackedTransform(Id));
  F.Face(Case.Contains(TEXT("Whole"))?90:146); F.Step(30); F.Face(-90); F.Step(30);
  const bool Negative=Case.Contains(TEXT("Never"));
  TestEqual(TEXT("Static positive/negative lifecycle control"),F.Room->GetStaleEpochCountForTesting(Target),Negative?0:1);
  if(Negative) TestEqual(TEXT("Never resources remain zero"),F.Room->GetHistoricalPresentationResourceCountForTesting(Target),0);
  else TestTrue(TEXT("Static gray remains after view loss"),F.Room->GetNewestHistoricalDiscoveredCellCountForTesting(Target)>0);
  return true;
 }
 if(Case.StartsWith(TEXT("SpatialPartial")))
 {
  const bool Never=Case.Contains(TEXT("Never"));
  TBitArray<> FirstCapture;
  for(int32 Pass=0;Pass<(Case.Contains(TEXT("AppearanceBlend"))?2:1);++Pass)
  {
   F.Room->ResetTrackedRevealPolicyForLab(Id,Reveal::SpatialPartial,10000,Never?History::Never:History::StationaryOnly);
   F.Face(146); F.Step(Pass==0?1:30);
   TestFalse(TEXT("SpatialPartial never requires span confirmation"),F.Room->IsRevealConfirmedForTesting(Id));
   TestTrue(TEXT("True partial legal contact"),F.Room->GetLastLegalCoverageRatioForTesting(Id)>0 && F.Room->GetLastLegalCoverageRatioForTesting(Id)<1);
   if(Case.Contains(TEXT("Moving")))
   {
    F.Room->StartTrackedRotationForTesting(Id,180,4); F.Step(30); F.Face(-90); F.Step(240);
    TestEqual(TEXT("Partial StationaryOnly motion creates no history"),F.Room->GetHistoricalPresentationResourceCountForTesting(Id),0); return true;
   }
   F.Face(-90); F.Step(30);
   if(Never) { TestEqual(TEXT("Partial Never leaves no gray resources"),F.Room->GetHistoricalPresentationResourceCountForTesting(Id),0); return true; }
   TestEqual(TEXT("Static partial seals one epoch"),F.Room->GetStaleEpochCountForTesting(Id),1);
   TBitArray<> Capture,Frozen; TestTrue(TEXT("Explicit sealed masks"),F.Room->GetNewestCaptureMasksForTesting(Id,Capture,Frozen));
   TestTrue(TEXT("FrozenHistoryMask equals LastLegalCaptureMask at every sample"),Capture==Frozen);
   TestTrue(TEXT("Observed region retained and unseen region stays absent"),Capture.CountSetBits()>0 && Capture.CountSetBits()<Capture.Num());
   TestTrue(TEXT("Legal partial cut has historical cap"),F.Room->GetVisibleHistoricalCapCountForTesting(Id)>0);
   if(Pass==0) FirstCapture=Capture; else TestTrue(TEXT("One-frame and settled appearance capture identical binary knowledge"),FirstCapture==Capture);
  }
  return true;
 }
 if(Case==TEXT("ExistingHistoryNotIdentityCleared"))
 {
  F.Face(90); F.Step(30); F.Face(-90); F.Step(30);
  TBitArray<> Capture,Frozen; F.Room->GetNewestCaptureMasksForTesting(Id,Capture,Frozen);
  const uint64 Signature=F.Room->GetHistoricalVisualSignatureForTesting(Id);
  F.Room->StartTrackedRotationForTesting(Id,180,4); F.Step(250);
  TBitArray<> After,AfterFrozen; F.Room->GetNewestCaptureMasksForTesting(Id,After,AfterFrozen);
  TestTrue(TEXT("Pre-motion capture retained by spatial evidence"),Capture==After && Frozen==AfterFrozen && Capture.CountSetBits()>0);
  TestEqual(TEXT("No StableID deletion of old presentation"),F.Room->GetHistoricalVisualSignatureForTesting(Id),Signature); return true;
 }
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
 if(Case==TEXT("WholeObjectConfirmedRespectsPartialWall"))
 {
  auto Pose=F.Room->GetTrackedTransform(Id); Pose.SetLocation(FVector(500,0,0)); F.Room->SetTrackedTransformForTesting(Id,Pose);
  F.Player->SetActorLocation(FVector(500,-600,92)); F.Player->SetActorRotation(FRotator(0,90,0)); F.Step(30);
  TestTrue(TEXT("Confirmed state persists at partial opaque wall"),F.Room->IsRevealConfirmedForTesting(Id));
  TestTrue(TEXT("Actual legal portion stays visible"),F.Room->IsCurrentSourceVisibleForTesting(Id));
  TestTrue(TEXT("Real wall cuts legal coverage"),F.Room->GetLastLegalCoverageRatioForTesting(Id)>0 && F.Room->GetLastLegalCoverageRatioForTesting(Id)<1);
  TestEqual(TEXT("Confirmed presentation cannot reveal wall-hidden pixels"),F.Room->GetCurrentPresentationMinimumForTesting(Id),0.f);
  auto* Fog=F.World->GetSubsystem<UDarkwellFogVisualSubsystem>();
  for(auto Point:{FVector2D(500,-20),FVector2D(500,20)})
  {
   const auto Q=Fog->QueryObjectOcclusionAtWorldPoint(Point); const auto Count=Fog->GetCoverageComputationsForTesting();
   const auto Again=Fog->QueryObjectOcclusionAtWorldPoint(Point);
   TestEqual(TEXT("Occlusion cache never recomputes same point/revision"),Fog->GetCoverageComputationsForTesting(),Count);
   TestTrue(TEXT("Object-only occlusion remains exact on both wall sides"),Q.bValid && Again.Coverage==Q.Coverage && Q.Coverage==(Point.Y<0?1.f:0.f));
  }
  return true;
 }
 if(Case==TEXT("ConfirmedWholeStopsSpanAndDenseObservationWork"))
 {
  const uint64 Evaluations=F.Room->GetRevealSpanEvaluationsForTesting(Id);
  F.Room->GetObjectPolicyForTesting(Id)->SetSightWeaveMoving(true);
  const FTransform Initial=F.Room->GetTrackedTransform(Id);
  for(int32 I=0;I<60;++I)
  {
   auto Pose=Initial; Pose.SetRotation(FQuat(FRotator(0,I*.7f,0))); F.Room->SetTrackedTransformForTesting(Id,Pose); F.Step();
   TestTrue(TEXT("Confirmed unoccluded source uses uniform full-resolution atlas and no observation masks"),F.Room->IsWholePresentationUniformForTesting(Id));
   TestEqual(TEXT("Confirmed rigid motion never recalculates span"),F.Room->GetRevealSpanEvaluationsForTesting(Id),Evaluations);
   TestEqual(TEXT("Every actual source pixel remains full"),F.Room->GetCurrentPresentationMinimumForTesting(Id),1.f);
  }
  F.Face(-90); F.Step(60); F.Room->ResetHistoryRuntimeTelemetryForTesting(); F.Step(30);
  TestEqual(TEXT("Settled loss performs no dense current work"),F.Room->GetHistoryRuntimeTotalTelemetryForTesting().CurrentSamplesTouched,uint64(0));
  return true;
 }
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
  TBitArray<> Reference;
  for(float Dt:{1.f/30,1.f/60,1.f/120,1.f/144}) for(bool Fast:{false,true})
  {
   F.Room->ResetTrackedRevealPolicyForLab(Id,Reveal::WholeObjectAfterSpan,100,History::StationaryOnly);
   F.Face(0); F.Step(1,Dt); TestFalse(TEXT("Starting endpoint never sees target"),F.Room->IsRevealConfirmedForTesting(Id));
   if(Fast) { F.Face(160); F.Step(1,Dt); }
   else for(int32 Angle=5;Angle<=160;Angle+=5) { F.Face(Angle); F.Step(1,Dt); }
   TestTrue(TEXT("Skipped positive interval and slow observation both confirm"),F.Room->IsRevealConfirmedForTesting(Id));
   TestEqual(TEXT("Current endpoint remains outside legal field"),F.Room->GetLastLegalCoverageRatioForTesting(Id),0.f);
   TestFalse(TEXT("Swept observation cannot show illegal current endpoint"),F.Room->IsCurrentSourceVisibleForTesting(Id));
   TBitArray<> Capture,Frozen; TestTrue(TEXT("Stationary swept observation seals gray"),F.Room->GetNewestCaptureMasksForTesting(Id,Capture,Frozen));
   TestTrue(TEXT("Swept gray uses binary knowledge"),Capture==Frozen && Capture.CountSetBits()>0);
   if(Reference.Num()==0) Reference=Capture; else TestTrue(TEXT("Fast/slow capture masks identical at all four rates"),Capture==Reference);
  }
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellIncrementalHistoryEvidenceParity,
 "Darkwell.PropLab.GrayObjectPolicy.IncrementalHistoryEvidenceMatchesFullUpdate",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellIncrementalHistoryEvidenceParity::RunTest(const FString&)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 TArray<uint64> Reference;
 for(bool Full:{true,false})
 {
  FRoom F; F.Room->bForceFullHistoryEvidenceForTesting=Full;
  F.Room->ResetTrackedRevealPolicyForLab(Id,Reveal::SpatialPartial,100,History::StationaryOnly);
  for(int32 Frame=0;Frame<240;++Frame)
  {
   if(Frame==0) F.Face(146);
   if(Frame==15 || Frame==110) F.Face(-90);
   if(Frame==45) TestTrue(TEXT("Real motion in replay"),F.Room->StartTrackedRotationForTesting(Id,180,1));
   if(Frame==75) F.Face(90);
   if(Frame==140) F.Face(0);
   if(Frame==141) F.Face(160);
   if(Frame==170 || Frame==215) F.Face(80);
   if(Frame==195) F.Face(150);
   F.Step();
   TArray<ADarkwellMovingPropLabRoom::FFineEvidenceDiagnostic> Samples;
   F.Room->GetFineEvidenceDiagnosticsForTesting(Id,Samples);
   uint64 Hash=1469598103934665603ull;
   auto Mix=[&](uint64 V){Hash=(Hash^V)*1099511628211ull;};
   Mix(Samples.Num());
   for(const auto& D:Samples)
   {
    Mix(D.Epoch); Mix(D.Index); Mix(GetTypeHash(D.Sample.State));
    Mix(D.Sample.bVerifiedEmpty); Mix(FMath::RoundToInt(D.Sample.Opacity*1000000));
    Mix(FMath::RoundToInt(D.Sample.InitialRemembered*1000000));
    Mix(FMath::RoundToInt(D.Sample.FrozenAAEnvelope*1000000));
    Mix(FMath::RoundToInt(D.Sample.EmptyDwell*1000000));
    Mix(D.bOccupied); Mix(D.bOwned); Mix(D.bValid); Mix(D.bSubmitted);
    Mix(FMath::RoundToInt(D.Coverage*1000000));
   }
   if(Full) Reference.Add(Hash);
   else if(!TestEqual(*FString::Printf(TEXT("Full-update reference matches every sample at frame %d"),Frame),Hash,Reference[Frame])) return false;
  }
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellGrayHistoryCapacityCurrentTest,
 "Darkwell.PropLab.GrayObjectPolicy.CurrentLiveAtHistoryCapacity",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellGrayHistoryCapacityCurrentTest::RunTest(const FString&)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 for(Reveal Mode:{Reveal::WholeObjectAfterSpan,Reveal::SpatialPartial})
 {
  FRoom F; F.Room->ResetTrackedRevealPolicyForLab(Id,Mode,100,History::StationaryOnly);
  F.Face(90); F.Step(15);
  auto& Prop=F.Room->Tracked.FindChecked(Id);
  F.Room->AbandonCurrentObservationWithoutHistory(Prop);
  Prop.History.Initialize(Id);
  // Synthetic small, distant observation records isolate admission capacity.
  // Real live coverage, motion, capture and presentation still run normally.
  for(int32 I=0;I<Prop.History.MaxResidentRecords;++I)
  {
   const FVector2D P(10000+I*25,10000);
   const int32 Index=Prop.History.BeginObservedLocation(FTransform(FVector(P,0)),FBox2D(P,P+FVector2D(2.5)),2.5f);
   Prop.History.AdvanceCurrent(.2f,TArray<float>{1}); Prop.History.FreezeCurrentForHiddenMovement();
   auto& R=Prop.History.GetMutableRecords()[Index]; R.FineHistory.Initialize(R.SpatialMemory,R.LastLegalCaptureMask);
  }
  TArray<uint64> Before;
  for(const auto& R:Prop.History.GetRecords()) Before.Add(R.FineHistory.EvidenceHash());
  for(int32 Cycle=0;Cycle<3;++Cycle)
  {
   F.Face(90); F.Step(15);
   TestTrue(TEXT("Current source is visible with all 64 historical slots occupied"),F.Room->IsCurrentSourceVisibleForTesting(Id));
   TestEqual(TEXT("One live record coexists with full history"),F.Room->GetCurrentEpochCountForTesting(Id),1);
   if(Cycle==1)
   {
    Prop.bInjectInvalidCoverageOnce=true; const int32 BeforeIndex=Prop.History.GetCurrentIndex(); F.Step();
    TestEqual(TEXT("Invalid revision preserves independent current"),Prop.History.GetCurrentIndex(),BeforeIndex);
    F.Step(); TestTrue(TEXT("Legal current recovers after invalid publication"),F.Room->IsCurrentSourceVisibleForTesting(Id));
   }
   F.Face(-90); F.Step(15);
   TestEqual(TEXT("Rejected capture does not grow or replace history"),F.Room->GetStaleEpochCountForTesting(Id),64);
   TestEqual(TEXT("View loss releases only the transient current"),F.Room->GetCurrentEpochCountForTesting(Id),0);
  }
  F.Face(90); F.Step(15); auto* Policy=F.Room->GetObjectPolicyForTesting(Id);
  Policy->SetSightWeaveMoving(true);
  auto Pose=F.Room->GetTrackedTransform(Id); Pose.SetRotation(FQuat(FRotator(0,30,0)));
  F.Room->SetTrackedTransformForTesting(Id,Pose); F.Step(15);
  TestTrue(TEXT("Moving live remains visible at historical capacity"),F.Room->IsCurrentSourceVisibleForTesting(Id));
  F.Face(-90); F.Step(15);
  TestEqual(TEXT("Moving view loss adds no historical record"),F.Room->GetStaleEpochCountForTesting(Id),64);
  for(int32 I=0;I<64;++I) TestEqual(TEXT("All stored history evidence remains intact"),Prop.History.GetRecords()[I].FineHistory.EvidenceHash(),Before[I]);
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellRepeatedHistoryEvidenceParity,
 "Darkwell.PropLab.GrayObjectPolicy.RepeatedHistoryEvidenceMatchesFullUpdate",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellRepeatedHistoryEvidenceParity::RunTest(const FString&)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 TArray<uint64> Reference; uint64 Reuses=0, OwnershipReuses=0, CoverageReuses=0, OccupancySamplesReused=0;
 for(bool Full:{true,false})
 {
  FRoom F; F.Room->bForceFullHistoryEvidenceForTesting=Full;
  for(int32 Frame=0;Frame<150;++Frame)
  {
   F.Face(Frame%20<10?90:-90);
   if(Frame==120) { auto P=F.Room->GetTrackedTransform(Id); P.SetRotation(FQuat(FRotator(0,15,0))); F.Room->SetTrackedTransformForTesting(Id,P); }
   F.Step();
   if(!Full) { const auto T=F.Room->GetHistoryRuntimeFrameTelemetryForTesting(); Reuses+=T.HistoryGeometryReuseHits; OwnershipReuses+=T.HistoryOwnershipReuseHits; CoverageReuses+=T.HistoryCoverageReuseHits; OccupancySamplesReused+=T.HistoryOccupancySamplesReused; }
   uint64 Hash=1469598103934665603ull;
   auto Mix=[&](uint64 V){Hash=(Hash^V)*1099511628211ull;};
   for(FName Target:{Id,FName(TEXT("Lab.Moving.Cabinet"))})
   {
    TArray<ADarkwellMovingPropLabRoom::FFineEvidenceDiagnostic> Samples;
    F.Room->GetFineEvidenceDiagnosticsForTesting(Target,Samples); Mix(Samples.Num());
    // Coarse diagnostic fields and their distinct center occupancy must also
    // stay identical when physical and ownership dirty regions are separated.
    const auto& Prop=F.Room->Tracked.FindChecked(Target);
    for(const auto& Record:Prop.History.GetRecords()) if(!Record.bCurrentObservedLocation)
    {
     const auto* Visual=Prop.Visuals.Find(Record.Epoch);
     for(const auto& Cell:Record.SpatialMemory.GetCells())
      for(float V:{Cell.InitialRemembered,Cell.RemainingStale,Cell.StaleOpacity,Cell.VerifiedEmpty,Cell.EmptyDwell,Cell.CurrentLegalCoverage}) Mix(GetTypeHash(V));
     if(Visual) for(TConstSetBitIterator<> It(Visual->CachedCoarseOccupied);It;++It) Mix(It.GetIndex()+1);
    }
    for(const auto& D:Samples)
    {
     Mix(D.Epoch); Mix(D.Index); Mix(GetTypeHash(D.Sample.State)); Mix(D.Sample.bVerifiedEmpty);
     for(float V:{D.Sample.Opacity,D.Sample.InitialRemembered,D.Sample.FrozenAAEnvelope,D.Sample.EmptyDwell,D.Coverage}) Mix(FMath::RoundToInt(V*1000000));
     Mix(D.bOccupied); Mix(D.bOwned); Mix(D.bValid); Mix(D.bSubmitted);
    }
   }
   if(Full) Reference.Add(Hash);
   else if(!TestEqual(*FString::Printf(TEXT("Repeated history equals full original evidence at frame %d"),Frame),Hash,Reference[Frame])) return false;
  }
  TestTrue(TEXT("Reference and optimized replay both retain repeated history"),F.Room->GetStaleEpochCountForTesting(Id)>=4);
 }
 TestTrue(TEXT("Replay actually exercises exact geometry reuse"),Reuses>0);
 TestTrue(TEXT("Replay actually exercises exact ownership reuse"),OwnershipReuses>0);
 TestTrue(TEXT("Replay actually exercises canonical coverage-delta reuse"),CoverageReuses>0);
 TestTrue(TEXT("Ownership-only updates reuse physical occupancy"),OccupancySamplesReused>0);
 AddInfo(FString::Printf(TEXT("Repeated history geometry reuse hits=%llu ownership reuse hits=%llu coverage reuse hits=%llu occupancy samples reused=%llu"),Reuses,OwnershipReuses,CoverageReuses,OccupancySamplesReused));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellPlanarProjectionParity,
 "Darkwell.PropLab.GrayObjectPolicy.PlanarProjectionMatchesOriginalSlab",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellPlanarProjectionParity::RunTest(const FString&)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 FRoom F; int32 Planar=0,Fallback=0,Queries=0;
 for(double Yaw:{0.,7.,45.,90.,179.,270.}) for(double Pitch:{0.,17.,90.})
 for(FVector Scale:{FVector(1),FVector(-1,2,.5),FVector(1,2,-.5),FVector(0,1,1),FVector(1,1,0)})
 {
  ADarkwellMovingPropLabRoom::FPrimitiveGeometrySnapshot G;
  G.LocalBounds=FBox(FVector(-75,-37.5,0),FVector(75,37.5,145));
  G.WorldTransform=FTransform(FRotator(Pitch,Yaw,0),FVector(-300,650,13),Scale); G.CachePlanarProjection();
  G.bCachedPlanarProjection?++Planar:++Fallback;
  for(double Tolerance:{0.,.02,.25})
  for(double X:{-76.,-75.0001,-75.,-74.9999,0.,74.9999,75.,75.0001,76.})
  for(double Y:{-38.,-37.5001,-37.5,-37.4999,0.,37.4999,37.5,37.5001,38.})
  {
   const FVector2D P(G.WorldTransform.TransformPosition(FVector(X,Y,30)));
   double Min=0,Max=0,ExpectedMin=0,ExpectedMax=0;
   F.Room->bForceFullHistoryEvidenceForTesting=true;
   const bool Expected=F.Room->QueryVerticalInterval(G,P,ExpectedMin,ExpectedMax,Tolerance);
   F.Room->bForceFullHistoryEvidenceForTesting=false;
   const bool Actual=F.Room->QueryVerticalInterval(G,P,Min,Max,Tolerance);
   if(!TestEqual(TEXT("Cached planar coverage equals original slab predicate"),Actual,Expected)) return false;
   if(Expected && (!TestTrue(TEXT("Exact lower interval"),Min==ExpectedMin) || !TestTrue(TEXT("Exact upper interval"),Max==ExpectedMax))) return false;
   ++Queries;
  }
 }
 TestTrue(TEXT("Positive planar cache and tilted/singular fallbacks exercised"),Planar>0 && Fallback>0);
 AddInfo(FString::Printf(TEXT("Planar slab parity: %d queries, %d cached transforms, %d fallbacks"),Queries,Planar,Fallback));
 return true;
}

#endif
