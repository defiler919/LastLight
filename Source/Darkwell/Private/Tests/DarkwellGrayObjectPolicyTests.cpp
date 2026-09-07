#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "VisionPresentation/DarkwellHistoryPreparation.h"
#include "UObject/UObjectIterator.h"
#include "UObject/GarbageCollection.h"
#include "SightWeaveObjectPolicy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Player/DarkwellCharacter.h"
#include "VisionPresentation/DarkwellMovingPropLabRoom.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "VisionPresentation/DarkwellGrayPolicyLab.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "VisionPresentation/DarkwellRememberablePropComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeExit.h"
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
  FRoom(bool GrayFixture=false, bool StressLab=false)
  {
   UPackage* Package=CreatePackage(StressLab ? Darkwell::GrayPolicyLab::MapPath : TEXT("/Game/Maps/L_ProjectFogPropGameplayLab"));
   World=NewObject<UWorld>(Package,MakeUniqueObjectName(Package,UWorld::StaticClass(),TEXT("GrayObjectPolicy")),RF_Transient);
   World->WorldType=EWorldType::Game; GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
   World->InitializeNewWorld(UWorld::InitializationValues().InitializeScenes(true).AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false));
   World->URL.AddOption(TEXT("PropLabOriginal")); World->URL.AddOption(TEXT("InWorldControls"));
   if(GrayFixture) World->URL.AddOption(TEXT("GrayObjectPolicies"));
   FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   Player=World->SpawnActor<ADarkwellCharacter>(ADarkwellCharacter::StaticClass(),FVector(-300,100,92),FRotator(0,90,0),P); Player->DispatchBeginPlay();
   Fixture=World->SpawnActor<ADarkwellPropGameplayLab>(); Fixture->PostInitializeComponents(); Fixture->DispatchBeginPlay();
   Room=ADarkwellMovingPropLabRoom::FindActive(World); Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
   Adapter->RequestSightWeaveAuthority(Fixture);
   if(StressLab) Room->ConfigureForGrayPolicyLab(Player); else Room->ResetRoom(Player);
   Face(90);
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
  TEXT("ResetClearsOnlyTarget"),TEXT("PlayStopResourceLifetime"),TEXT("CanonicalRasterMatchesOriginalSamples"),TEXT("ConfirmedWholeStopsSpanAndDenseObservationWork"),TEXT("CachedDiagnosticsMatchForcedDiagnostics"),TEXT("RepeatedPoseDiagnosticsMatchFullScan"),TEXT("FramePhysicalCacheMatchesGeometryOracle"),TEXT("WholeObjectConfirmedUsesLegalWallContact")})
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
   TestEqual(TEXT("Repeated same-state observations retain one knowledge state"),F.Room->GetSpatialRecordCount(Id),1);
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
 if(Case==TEXT("WholeObjectConfirmedUsesLegalWallContact"))
 {
  auto Pose=F.Room->GetTrackedTransform(Id); Pose.SetLocation(FVector(500,0,0)); F.Room->SetTrackedTransformForTesting(Id,Pose);
  F.Player->SetActorLocation(FVector(500,-600,92)); F.Player->SetActorRotation(FRotator(0,90,0)); F.Step(30);
  TestTrue(TEXT("Confirmed state persists at partial opaque wall"),F.Room->IsRevealConfirmedForTesting(Id));
  TestTrue(TEXT("Actual legal portion stays visible"),F.Room->IsCurrentSourceVisibleForTesting(Id));
  TestTrue(TEXT("Real wall cuts legal coverage"),F.Room->GetLastLegalCoverageRatioForTesting(Id)>0 && F.Room->GetLastLegalCoverageRatioForTesting(Id)<1);
  TestEqual(TEXT("Confirmed contact presents the full object despite partial wall coverage"),F.Room->GetCurrentPresentationMinimumForTesting(Id),1.f);
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
   TestFalse(TEXT("Both completed sweep sessions retire qualification at the outside endpoint"),F.Room->IsRevealConfirmedForTesting(Id));
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
  TestFalse(TEXT("Hidden rigid motion ends the previous observation qualification"),F.Room->IsRevealConfirmedForTesting(Id));
  F.Face(146); F.Step(30); TestFalse(TEXT("Short reacquisition must satisfy this session's span"),F.Room->IsRevealConfirmedForTesting(Id));
  F.Face(90); F.Step(30); TestTrue(TEXT("Same configured span requalifies the newly observed session"),F.Room->IsRevealConfirmedForTesting(Id));
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
   Mix(Samples.Num()); Mix(F.Room->GetTotalCapTriangles());
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellDistantHistorySleepWakeTest,
 "Darkwell.PropLab.GrayObjectPolicy.DistantHistorySleepsAndWakesWithoutMutation",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellDistantHistorySleepWakeTest::RunTest(const FString&)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 FRoom F;
 F.Room->ResetTrackedRevealPolicyForLab(
  Id,Reveal::SpatialPartial,100,History::StationaryOnly);
 F.Face(146); F.Step(30); F.Face(-90); F.Step(30);
 if(!TestEqual(TEXT("Fixture creates one historical record"),
  F.Room->GetStaleEpochCountForTesting(Id),1)) return false;
 auto EvidenceHash=[&]()
 {
  TArray<ADarkwellMovingPropLabRoom::FFineEvidenceDiagnostic> Samples;
  F.Room->GetFineEvidenceDiagnosticsForTesting(Id,Samples);
  uint64 Hash=1469598103934665603ull;
  for(const auto& D:Samples)
  {
   Hash=(Hash^D.Epoch)*1099511628211ull;
   Hash=(Hash^D.Index)*1099511628211ull;
   Hash=(Hash^GetTypeHash(D.Sample.State))*1099511628211ull;
   Hash=(Hash^GetTypeHash(D.Sample.Opacity))*1099511628211ull;
   Hash=(Hash^static_cast<uint64>(D.Sample.bVerifiedEmpty))*1099511628211ull;
  }
  return Hash;
 };
 F.Player->SetActorLocation(FVector(10000,10000,92));
 F.Player->SetActorRotation(FRotator(0,-90,0));
 // The old observer footprint receives one conservative transition frame;
 // after that, an out-of-range pending evidence timer must not keep the record
 // awake because no current coverage can affect it.
 F.Step(3);
 const auto Sleeping=F.Room->GetHistoryRuntimeFrameTelemetryForTesting();
 TestTrue(TEXT("Distant history is excluded by the spatial candidate index"),
  Sleeping.SleepingHistoricalEpochs>0 && Sleeping.CandidateHistoricalEpochs==0);
 const uint64 Before=EvidenceHash();
 F.Step(30);
 TestEqual(TEXT("Sleeping history evidence is immutable while irrelevant"),
  EvidenceHash(),Before);
 F.Face(-90); F.Step();
 const auto Awake=F.Room->GetHistoryRuntimeFrameTelemetryForTesting();
 TestTrue(TEXT("Returning observer wakes the historical record"),
  Awake.CandidateHistoricalEpochs>0 && Awake.ActiveHistoricalEpochs>0);
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
   TestEqual(TEXT("One genuinely new state is retained beyond legacy capacity; repeats reuse it"),F.Room->GetStaleEpochCountForTesting(Id),65);
   TestEqual(TEXT("View loss releases only the transient current"),F.Room->GetCurrentEpochCountForTesting(Id),0);
  }
  F.Face(90); F.Step(15); auto* Policy=F.Room->GetObjectPolicyForTesting(Id);
  Policy->SetSightWeaveMoving(true);
  auto Pose=F.Room->GetTrackedTransform(Id); Pose.SetRotation(FQuat(FRotator(0,30,0)));
  F.Room->SetTrackedTransformForTesting(Id,Pose); F.Step(15);
  TestTrue(TEXT("Moving live remains visible at historical capacity"),F.Room->IsCurrentSourceVisibleForTesting(Id));
  F.Face(-90); F.Step(15);
  TestTrue(TEXT("Moving view loss adds no historical record; legally replaced state may compact"),F.Room->GetStaleEpochCountForTesting(Id)<=65);
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
  TestTrue(TEXT("Reference and optimized replay retain visible current knowledge without duplicate sessions"),F.Room->IsCurrentSourceVisibleForTesting(Id) && F.Room->GetSpatialRecordCount(Id)>0 && F.Room->GetSpatialRecordCount(Id)<=3);
 }
 // Cross-record cache hits are no longer a product expectation: the identical
 // observation sessions now share one state. Full per-frame evidence parity
 // above remains required; independent solid-interior coverage is tested below.
 AddInfo(FString::Printf(TEXT("Repeated history geometry reuse hits=%llu ownership reuse hits=%llu coverage reuse hits=%llu occupancy samples reused=%llu"),Reuses,OwnershipReuses,CoverageReuses,OccupancySamplesReused));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellBatchOwnershipParity,
 "Darkwell.PropLab.ArchitectureAudit.BatchOwnershipSamplesEquivalent",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellBatchOwnershipParity::RunTest(const FString&)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 TArray<uint64> Reference;
 for(bool Full:{true,false})
 {
  FRoom F; F.Room->bForceFullHistoryEvidenceForTesting=Full;
  F.Face(90); F.Step(20); F.Face(-90); F.Step(20);
  TestTrue(TEXT("Batch fixture supplies distinct captured poses"),F.Room->ConfigureHistoricalEpochCountForTesting(Id,16));
  for(int32 Frame=0;Frame<6;++Frame)
  {
   F.Face(Frame==1||Frame==2?136.f:Frame==4?20.f:-90.f); F.Step();
   TArray<ADarkwellMovingPropLabRoom::FFineEvidenceDiagnostic> Samples;
   F.Room->GetFineEvidenceDiagnosticsForTesting(Id,Samples);
   uint64 H=1469598103934665603ull;
   auto Mix=[&](uint64 V){H=(H^V)*1099511628211ull;};
   Mix(Samples.Num()); Mix(F.Room->GetTotalCapTriangles());
   for(const auto& D:Samples)
   {
    Mix(D.Epoch); Mix(D.Index); Mix(GetTypeHash(D.Sample.State)); Mix(D.Sample.bVerifiedEmpty);
    for(float V:{D.Sample.InitialRemembered,D.Sample.Opacity,D.Sample.FrozenAAEnvelope,D.Sample.EmptyDwell,D.Coverage}) Mix(FMath::RoundToInt(V*1000000));
    Mix(D.bOccupied); Mix(D.bOwned); Mix(D.bValid); Mix(D.bSubmitted);
   }
   if(Full) Reference.Add(H);
   else if(!TestEqual(*FString::Printf(TEXT("Batch ownership matches full sample oracle at frame %d"),Frame),H,Reference[Frame])) return false;
  }
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellOwnershipIndexParity,
 "Darkwell.PropLab.ArchitectureAudit.OwnershipSpatialIndexMatchesScan",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellOwnershipIndexParity::RunTest(const FString&)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 FRoom F;
 if(!TestTrue(TEXT("Index fixture seeds independent histories"),F.Room->ConfigureHistoricalEpochCountForTesting(Id,8))) return false;
 auto& Prop=F.Room->Tracked.FindChecked(Id);
 using FGeometry=ADarkwellObjectMemoryScene::FPrimitiveGeometrySnapshot;
 TMap<uint32,TArray<FGeometry>> Original;
 TArray<const FDarkwellSpatialObservationRecord*> Candidates;
 for(const auto& Record:Prop.History.GetRecords())
 {
  Candidates.Add(&Record);
  Original.Add(Record.Epoch,Prop.Visuals.FindChecked(Record.Epoch).PartGeometry);
 }
 TGuardValue<bool> CandidateScope(F.Room->bUseNewerCandidates,true);
 TGuardValue<FName> IdScope(F.Room->NewerCandidateId,Id);
 TGuardValue<uint32> EpochScope(F.Room->NewerCandidateMaximumEpoch,Candidates.Last()->Epoch);
 TGuardValue<TConstArrayView<const FDarkwellSpatialObservationRecord*>> ViewScope(F.Room->FrameNewerCandidates,MakeArrayView(Candidates));
 int32 Queries=0;
 for(int32 Case=0;Case<4;++Case)
 {
  for(auto& Pair:Prop.Visuals)
  {
   Pair.Value.PartGeometry=Original.FindChecked(Pair.Key);
   for(auto& G:Pair.Value.PartGeometry)
   {
    if(Case==1) G.WorldTransform.SetScale3D(G.WorldTransform.GetScale3D()*FVector(-1,1,1));
    if(Case==2) G.WorldTransform.SetRotation(FRotator(17,31,9).Quaternion());
    if(Case==3) G.WorldTransform.SetScale3D(FVector(0,1,1));
    G.CachePlanarProjection();
   }
  }
  ADarkwellObjectMemoryScene::FNewerOwnershipIndex Index;
  const bool Ready=F.Room->BuildNewerOwnershipIndex(Prop,MakeArrayView(Candidates),Index);
  TestEqual(TEXT("Planar and negative scale index; tilt and singular fallback"),Ready,Case<2);
  for(int32 Older:{0,3,6})
  {
   const auto& Record=*Candidates[Older];
   const auto& Visual=Prop.Visuals.FindChecked(Record.Epoch);
   auto Geometry=F.Room->CollectNewerGeometrySnapshots(Prop,Record.Epoch);
   TGuardValue<bool> GeometryScope(F.Room->bUseOwnershipGeometry,true);
   TGuardValue<TConstArrayView<FGeometry>> GeometryView(F.Room->FrameOwnershipGeometry,MakeArrayView(Geometry));
   FRandomStream Random(100+Older);
   TArray<FVector2D> Points;
   const auto Bounds=Record.SpatialMemory.GetBounds().ExpandBy(40);
   for(int32 I=0;I<300;++I) Points.Add(FVector2D(Random.FRandRange(Bounds.Min.X,Bounds.Max.X),Random.FRandRange(Bounds.Min.Y,Bounds.Max.Y)));
   for(const auto& G:Visual.PartGeometry)
    for(const FVector2D Point:{G.ProjectionBounds.Min,G.ProjectionBounds.Max,G.ProjectionBounds.GetCenter()})
     for(double Offset:{-.051,-.0001,0.,.0001,.051}) Points.Add(Point+FVector2D(Offset));
   for(const auto Point:Points)
   {
    TArray<FVector2D> Scan, Indexed;
    F.Room->ActiveOwnershipIndex=nullptr;
    F.Room->CollectNewerOwnedVerticalIntervals(Prop,Record.Epoch,Point,Scan,.051);
    const FBox2D Footprint(Point-FVector2D(.3125),Point+FVector2D(.3125));
    const bool Overlap=F.Room->HasNewerObservedGeometryOverlapWithinFootprint(Prop,Visual,Record.Epoch,Footprint);
    F.Room->ActiveOwnershipIndex=Ready?&Index:nullptr;
    F.Room->CollectNewerOwnedVerticalIntervals(Prop,Record.Epoch,Point,Indexed,.051);
    const bool IndexedOverlap=F.Room->HasNewerObservedGeometryOverlapWithinFootprint(Prop,Visual,Record.Epoch,Footprint);
    F.Room->ActiveOwnershipIndex=nullptr;
    if(!TestTrue(TEXT("Complete ordered intervals equal unindexed scan"),Scan==Indexed)
     || !TestEqual(TEXT("Conservative footprint equals unindexed scan"),IndexedOverlap,Overlap)) return false;
    ++Queries;
   }
  }
 }
 AddInfo(FString::Printf(TEXT("Ownership spatial index compared %d complete interval and footprint queries"),Queries));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellJoinedOwnershipParity,
 "Darkwell.PropLab.ArchitectureAudit.JoinedSealedOwnershipParityAndLifetime",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellJoinedOwnershipParity::RunTest(const FString&)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 TArray<uint64> Reference;
 uint64 Batches=0, WorkSamples=0;
 for(bool Serial:{true,false})
 {
  FRoom F; F.Room->bForceSerialOwnershipForTesting=Serial;
  F.Face(-90); F.Step(2);
  for(int32 Frame=0;Frame<10;++Frame)
  {
   // Each reseed destroys old visuals and replaces epochs. No task may retain
   // the previous borrowed input after Step returns. Reset then world teardown
   // exercise both cancellation boundaries of this joined (not queued) design.
   if(Frame==0 || Frame==4 || Frame==8)
    if(!TestTrue(TEXT("New independent sealed captures"),F.Room->ConfigureHistoricalEpochCountForTesting(Id,Frame==4?8:16))) return false;
   if(Frame==2) F.Room->InjectInvalidCoverageOnceForTesting(Id);
   if(Frame==6) F.Room->ResetRoom(F.Player);
   F.Face(Frame==1 || Frame==3 || Frame==7 ? 136.f : -90.f);
   F.Step();
   const auto T=F.Room->GetHistoryRuntimeFrameTelemetryForTesting();
   if(Serial) TestEqual(TEXT("Serial reference dispatches no tasks"),T.JoinedOwnershipBatches,uint64(0));
   else { Batches+=T.JoinedOwnershipBatches; WorkSamples+=T.JoinedOwnershipSamples; }
   TArray<ADarkwellMovingPropLabRoom::FFineEvidenceDiagnostic> Samples;
   F.Room->GetFineEvidenceDiagnosticsForTesting(Id,Samples);
   uint64 H=1469598103934665603ull;
   auto Mix=[&](uint64 V){H=(H^V)*1099511628211ull;};
   Mix(Samples.Num()); Mix(F.Room->GetTotalCapTriangles());
   for(const auto& D:Samples)
   {
    Mix(D.Epoch); Mix(D.Index); Mix(GetTypeHash(D.Sample.State)); Mix(D.Sample.bVerifiedEmpty);
    for(float V:{D.Sample.InitialRemembered,D.Sample.Opacity,D.Sample.FrozenAAEnvelope,D.Sample.EmptyDwell,D.Coverage}) Mix(GetTypeHash(V));
    Mix(D.bOccupied); Mix(D.bOwned); Mix(D.bValid); Mix(D.bSubmitted);
   }
   const auto& Prop=F.Room->Tracked.FindChecked(Id);
   for(const auto& R:Prop.History.GetRecords())
   {
    Mix(R.Epoch); Mix(R.bCurrentObservedLocation);
    if(const auto* V=Prop.Visuals.Find(R.Epoch))
    {
     Mix(V->bPresentationRetired); Mix(V->CapTriangles); Mix(V->TextureSignature);
     for(TConstSetBitIterator<> It(V->SuppressedByCurrentEvidence);It;++It) Mix(It.GetIndex()+1);
     for(const auto& Q:V->CapQuads)
      for(const FVector P:{Q.A,Q.B,Q.C,Q.D}) Mix(GetTypeHash(P));
    }
   }
   if(Serial) Reference.Add(H);
   else if(!TestEqual(*FString::Printf(TEXT("Joined publication equals serial, including reset/invalid coverage at frame %d"),Frame),H,Reference[Frame])) return false;
  }
 }
 TestTrue(TEXT("Large sealed histories actually execute joined batches"),Batches>0 && WorkSamples>0);
 AddInfo(FString::Printf(TEXT("Joined ownership parity: batches=%llu input_samples=%llu; all tasks joined before reset/reseed/world destruction"),Batches,WorkSamples));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellCapturePreparationParity,
 "Darkwell.PropLab.ArchitectureAudit.CapturePreparationParityAndLifetime",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellCapturePreparationParity::RunTest(const FString&)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 // Full slab fallback is the geometry oracle, including thin, tilted and
 // reflected parts. Non-word-aligned dimensions catch packed-output races.
 int32 FootprintCases=0;
 {
  FRoom F;
  for(const FRotator Pose:{FRotator(0,17,0),FRotator(31,73,19),FRotator(0,179,0)})
   for(const FVector Scale:{FVector(1,1,1),FVector(-.7,1.8,.4),FVector(1,.01,1)})
    for(const FIntPoint Size:{FIntPoint(17,19),FIntPoint(65,67),FIntPoint(129,127)})
    {
     TArray<ADarkwellObjectMemoryScene::FPrimitiveGeometrySnapshot> Parts;
     for(const FBox Local:{FBox(FVector(-23,-15,-40),FVector(23,15,40)),FBox(FVector(-37,-.2,-2),FVector(37,.2,2))})
     {
      auto& P=Parts.AddDefaulted_GetRef(); P.LocalBounds=Local;
      P.WorldTransform=FTransform(Pose,FVector(7,-9,20),Scale); P.PrimitiveIndex=Parts.Num()-1; P.CachePlanarProjection();
     }
     const FBox2D Bounds(FVector2D(-70,-70),FVector2D(70,70));
     F.Room->bForceFullHistoryEvidenceForTesting=true;
     const auto Original=F.Room->BuildCaptureGeometryFootprint(Bounds,Size,Parts,false);
     F.Room->bForceFullHistoryEvidenceForTesting=false;
     const auto Joined=F.Room->BuildCaptureGeometryFootprint(Bounds,Size,Parts,true);
     if(!TestTrue(TEXT("Joined exact footprint matches full slab oracle"),Original==Joined)) return false;
     ++FootprintCases;
    }
 }
 for(const Reveal Mode:{Reveal::SpatialPartial,Reveal::WholeObjectAfterSpan})
 {
  TArray<uint64> Reference;
  for(const bool Legacy:{true,false})
  {
   FRoom F; F.Room->bForceLegacyCapturePreparationForTesting=Legacy;
   F.Room->ResetTrackedRevealPolicyForLab(Id,Mode,100,History::StationaryOnly);
   F.Face(90); F.Step(25); F.Face(-90); F.Step();
   for(int32 Phase=0;Phase<6;++Phase)
   {
    if(Phase==0 || Phase==5)
     if(!TestTrue(TEXT("Cold capture succeeds"),F.Room->ConfigureHistoricalEpochCountForTesting(Id,Phase==0?8:4))) return false;
    if(Phase==1) { F.Room->InjectInvalidCoverageOnceForTesting(Id); F.Step(); }
    if(Phase==2) F.Room->ResetRoom(F.Player);
    if(Phase==3) { F.Room->ResetTrackedRevealPolicyForLab(Id,Mode,100,History::StationaryOnly); F.Face(90); F.Step(25); }
    if(Phase==4) { F.Face(-90); F.Step(); }
    const auto& Prop=F.Room->Tracked.FindChecked(Id);
    uint64 H=1469598103934665603ull;
    auto Mix=[&](uint64 V){H=(H^V)*1099511628211ull;};
    auto Bits=[&](const TBitArray<>& B){Mix(B.Num()); for(TConstSetBitIterator<> It(B);It;++It) Mix(It.GetIndex()+1);};
    Mix(Prop.History.GetRecords().Num());
    for(const auto& R:Prop.History.GetRecords())
    {
     Mix(R.Epoch); Mix(R.bCurrentObservedLocation); Bits(R.LastLegalCaptureMask); Bits(R.GeometryFootprint);
     for(const auto& S:R.FineHistory.GetSamples())
     {
      Mix(GetTypeHash(S.State)); Mix(S.bVerifiedEmpty);
      for(float V:{S.InitialRemembered,S.Opacity,S.FrozenAAEnvelope,S.EmptyDwell}) Mix(GetTypeHash(V));
     }
     if(const auto* V=Prop.Visuals.Find(R.Epoch))
     {
      Mix(V->TextureSignature); Mix(V->CapTriangles); Bits(V->SuppressedByCurrentEvidence);
      Mix(V->Cap.IsValid() && V->Cap->IsVisible());
      for(const auto& Q:V->CapQuads) for(const FVector P:{Q.A,Q.B,Q.C,Q.D}) Mix(GetTypeHash(P));
     }
    }
    if(Legacy) Reference.Add(H);
    else if(!TestEqual(*FString::Printf(TEXT("Atomic capture parity mode=%d phase=%d"),int32(Mode),Phase),H,Reference[Phase])) return false;
   }
  }
 }
 AddInfo(FString::Printf(TEXT("Capture footprint cases=%d; Partial/Whole capture, invalid coverage, reset, reseed and world teardown parity"),FootprintCases));
 return true;
}

// Compare the submitted mesh itself, ordered cap diagnostics and authority state.
// Forced rebuilds exercise the joined transaction even when its signature is warm.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellJoinedCapParity,
 "Darkwell.PropLab.ArchitectureAudit.JoinedCapMeshParityAndLifetime",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellJoinedCapParity::RunTest(const FString&)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 int32 Compared=0, NonEmpty=0, Joined=0, CurrentFallback=0;
 for(const Reveal Mode:{Reveal::SpatialPartial,Reveal::WholeObjectAfterSpan})
 {
  FRoom F;
  F.Room->DestroyTracked();
  FResolvedSightWeaveObjectPolicy Policy; Policy.RevealMode=Mode; Policy.HistoryMode=History::StationaryOnly;
  F.Room->SpawnTracked(Id,4,FVector(620,75,115),FLinearColor(.62,.42,.18),
   FTransform(FVector(-300,650,0)),ESightWeaveObjectPolicySource::UseProjectDefault,History::StationaryOnly,&Policy);
  for(int32 Phase=0;Phase<10;++Phase)
  {
   if(Phase==0) { F.Face(52); F.Step(20); F.Face(-90); F.Step(); }
   if(Phase==7)
    if(!TestTrue(TEXT("Reseed sealed histories"),F.Room->ConfigureHistoricalEpochCountForTesting(Id,4))) return false;
   if(Phase==8)
   {
    // The scaling fixture intentionally removes its actual source. Restore it
    // explicitly so this phase exercises a live Current, not only sealed data.
    auto& Restored=F.Room->Tracked.FindChecked(Id);
    Restored.bExists=true; Restored.Actual->SetActorHiddenInGame(false);
    Restored.Actual->SetActorEnableCollision(true);
   }
   if(Phase==1 || Phase==8) { F.Face(40); F.Step(12); }
   if(Phase==2 || Phase==9) { F.Face(-90); F.Step(); }
   if(Phase==3) { F.Room->InjectInvalidCoverageOnceForTesting(Id); F.Step(); }
   if(Phase==4) { F.Face(90); F.Step(25); }
   if(Phase==5) { F.Face(-90); F.Step(); }
   if(Phase==6) { F.Room->ResetRoom(F.Player); F.Room->ResetTrackedRevealPolicyForLab(Id,Mode,100,History::StationaryOnly); }
   auto& Prop=F.Room->Tracked.FindChecked(Id);
   for(auto& R:Prop.History.GetMutableRecords())
   {
    auto* V=Prop.Visuals.Find(R.Epoch); if(!V || !V->Cap.IsValid()) continue;
    const auto FineBefore=R.FineHistory.GetSamples();
    TArray<FDarkwellHistoryGridV2::FSample> SavedSamples; SavedSamples.Append(FineBefore.GetData(),FineBefore.Num());
    const auto Suppression=V->SuppressedByCurrentEvidence;
    const auto Capture=R.LastLegalCaptureMask;
    auto Hash=[&]()
    {
     uint64 H=1469598103934665603ull; auto Mix=[&](uint64 X){H=(H^X)*1099511628211ull;};
     Mix(V->CapSignature); Mix(V->CapTriangles); Mix(V->CapExpected); Mix(V->CapGenerated);
     Mix(V->CapClipped); Mix(V->MissingHistoricalCuts); Mix(V->Cap->IsVisible());
     for(const auto& Q:V->CapQuads) { Mix(Q.PrimitiveIndex); for(const FVector P:{Q.A,Q.B,Q.C,Q.D}) Mix(GetTypeHash(P)); }
     for(const auto P:V->CapSamplePoints) Mix(GetTypeHash(P));
     V->Cap->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Mesh)
     {
      Mix(Mesh.VertexCount()); Mix(Mesh.TriangleCount());
      for(int32 I:Mesh.VertexIndicesItr()) { Mix(I); Mix(GetTypeHash(Mesh.GetVertex(I))); }
      for(int32 I:Mesh.TriangleIndicesItr()) { const auto T=Mesh.GetTriangle(I); Mix(I); Mix(T.A); Mix(T.B); Mix(T.C); }
     });
     return H;
    };
    V->CapSignature=0; F.Room->bForceSerialCapBuildForTesting=true;
    const auto Before=F.Room->GetHistoryRuntimeFrameTelemetryForTesting();
    const int32 BeforeJoined=F.Room->JoinedCapBuildsForTesting;
    F.Room->UpdateRecordCap(Prop,R); const uint64 Reference=Hash();
    const auto Middle=F.Room->GetHistoryRuntimeFrameTelemetryForTesting();
    TestEqual(TEXT("Serial control never dispatches rows"),F.Room->JoinedCapBuildsForTesting,BeforeJoined);
    NonEmpty+=V->CapTriangles>0;
    V->CapSignature=0; F.Room->bForceSerialCapBuildForTesting=false;
    F.Room->UpdateRecordCap(Prop,R);
    const auto After=F.Room->GetHistoryRuntimeFrameTelemetryForTesting();
    if(!TestEqual(*FString::Printf(TEXT("Exact mesh and cap diagnostic parity mode=%d phase=%d epoch=%u"),int32(Mode),Phase,R.Epoch),Hash(),Reference)) return false;
    TestEqual(TEXT("Same geometric predicates evaluated"),After.PrimitiveGeometryTests-Middle.PrimitiveGeometryTests,Middle.PrimitiveGeometryTests-Before.PrimitiveGeometryTests);
    TestEqual(TEXT("Same ownership candidates visited"),After.OwnershipRecordVisits-Middle.OwnershipRecordVisits,Middle.OwnershipRecordVisits-Before.OwnershipRecordVisits);
    bool LiveNewer=false;
    for(const auto& C:Prop.History.GetRecords()) LiveNewer|=C.bCurrentObservedLocation && C.Epoch>R.Epoch;
    if(!R.bCurrentObservedLocation && LiveNewer)
    {
     ++CurrentFallback;
     TestEqual(TEXT("Live Current dependency remains on GT"),F.Room->JoinedCapBuildsForTesting,BeforeJoined);
    }
    TestTrue(TEXT("Cap publication never changes authority or suppression masks"),Suppression==V->SuppressedByCurrentEvidence && Capture==R.LastLegalCaptureMask);
    TestEqual(TEXT("Fine history storage retained"),R.FineHistory.GetSamples().Num(),SavedSamples.Num());
    for(int32 I=0;I<SavedSamples.Num();++I)
    {
     const auto& A=SavedSamples[I]; const auto& B=R.FineHistory.GetSamples()[I];
     if(!TestTrue(TEXT("Every fine authority field unchanged"),A.State==B.State && A.InitialRemembered==B.InitialRemembered && A.Opacity==B.Opacity
      && A.FrozenAAEnvelope==B.FrozenAAEnvelope && A.bVerifiedEmpty==B.bVerifiedEmpty && A.EmptyDwell==B.EmptyDwell)) return false;
    }
    ++Compared;
   }
  }
  Joined+=F.Room->JoinedCapBuildsForTesting;
 }
 TestTrue(TEXT("Non-vacuous submitted caps and joined large grids"),Compared>0 && NonEmpty>0 && Joined>0);
 TestTrue(TEXT("Exercised historical cap with live Current fallback"),CurrentFallback>0);
 AddInfo(FString::Printf(TEXT("CAP_PARITY compared=%d nonempty=%d joined=%d live_current_fallback=%d; reset/reseed/invalid coverage/Whole/Partial/world teardown"),Compared,NonEmpty,Joined,CurrentFallback));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellRecordResourcesParity,
 "Darkwell.PropLab.ArchitectureAudit.RecordScopedResourcesParityAndLifetime",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellRecordResourcesParity::RunTest(const FString&)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 auto* CVar=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Darkwell.ObjectMemory.RecordScopedResources"));
 if(!TestNotNull(TEXT("Same-binary resource oracle"),CVar)) return false;
 const int32 Original=CVar->GetInt();
 ON_SCOPE_EXIT { CVar->Set(Original,ECVF_SetByCode); };
 for(const Reveal Mode:{Reveal::SpatialPartial,Reveal::WholeObjectAfterSpan})
 {
  TArray<uint64> Reference;
  for(const bool Legacy:{true,false})
  {
   CVar->Set(Legacy?0:1,ECVF_SetByCode);
   TArray<TWeakObjectPtr<UObject>> Released;
   {
    FRoom F;
    F.Room->ResetTrackedRevealPolicyForLab(Id,Mode,100,History::StationaryOnly);
    F.Face(90); F.Step(25);
    auto Check=[&](int32 Phase)
    {
     auto& Prop=F.Room->Tracked.FindChecked(Id);
     uint64 H=1469598103934665603ull;
     auto Mix=[&](uint64 V){H=(H^V)*1099511628211ull;};
     TSet<UMaterialInstanceDynamic*> RecordMaterials;
     int32 CheckedMeshes=0;
     for(const auto& R:Prop.History.GetRecords())
     {
      Mix(R.Epoch); Mix(R.bCurrentObservedLocation); Mix(R.FineHistory.EvidenceHash());
      const auto* V=Prop.Visuals.Find(R.Epoch);
      if(!V || !V->Proxy.IsValid()) continue;
      TInlineComponentArray<UStaticMeshComponent*> Meshes(V->Proxy.Get());
      TestEqual(TEXT("All captured primitive components retained"),Meshes.Num(),R.Primitives.Num());
      TestEqual(TEXT("MID ownership is per record, legacy per part"),V->Materials.Num(),Legacy?Meshes.Num():1);
      for(const auto& M:V->Materials)
      {
       TestFalse(TEXT("Materials never alias across records"),RecordMaterials.Contains(M.Get()));
       RecordMaterials.Add(M.Get());
      }
      const auto& Bounds=R.SpatialMemory.GetBounds();
      const FVector2D Inv=FVector2D(1,1)/Bounds.GetSize();
      for(auto* Mesh:Meshes)
      {
       auto* M=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
       if(!TestNotNull(TEXT("Every mesh has a dynamic material"),M)) return false;
       TestTrue(TEXT("Each mesh registered"),Mesh->IsRegistered());
       TestTrue(TEXT("Binding belongs to the record"),V->Materials.Contains(M));
       TestEqual(TEXT("Prepared Current is transparent, sealed is ready"),M->K2_GetScalarParameterValue(TEXT("SpatialReady")),R.bCurrentObservedLocation?0.f:1.f);
       TestTrue(TEXT("Texture binding is record-local"),M->K2_GetTextureParameterValue(TEXT("SpatialStateTexture"))==V->Texture.Get());
       TestTrue(TEXT("Captured bounds unchanged"),M->K2_GetVectorParameterValue(TEXT("SpatialMinInv"))==FLinearColor(Bounds.Min.X,Bounds.Min.Y,Inv.X,Inv.Y));
       TestTrue(TEXT("Captured tint unchanged"),M->K2_GetVectorParameterValue(TEXT("OriginalBaseColorTint"))==R.Tint);
       TestEqual(TEXT("Captured UV unchanged"),M->K2_GetScalarParameterValue(TEXT("OriginalUVScale")),R.UVScale);
       ++CheckedMeshes;
      }
      if(!R.bCurrentObservedLocation)
      {
       TestFalse(TEXT("First exit publishes the proxy in this call"),V->Proxy->IsHidden());
       TestFalse(TEXT("First exit finishes prepared-resource handoff"),V->bProxyPreparedForCapture);
       TestTrue(TEXT("History uses captured pose"),V->Proxy->GetActorTransform().Equals(R.SnapshotTransform));
       TestTrue(TEXT("First exit submits complete pixels"),!V->SubmittedPresentation.IsEmpty());
      }
      if(R.bConfirmedWholeCapture) TestFalse(TEXT("Confirmed Whole has no cap"),V->Cap.IsValid());
      Mix(V->Texture->GetSizeX()); Mix(V->Texture->GetSizeY());
      Mix(V->TextureSignature); Mix(V->CapSignature); Mix(V->CapTriangles);
      Mix(V->ProxyCreationCount); Mix(V->TextureCreationCount);
     }
     TestTrue(TEXT("Positive resource assertions"),CheckedMeshes>0);
     if(Legacy) Reference.Add(H); else TestEqual(TEXT("Resource allocation preserves complete presentation/evidence"),H,Reference[Phase]);
     return true;
    };
    if(!Check(0)) return false;
    F.Face(-90); F.Step();
    if(!Check(1)) return false;
    if(!TestTrue(TEXT("Distinct record batch"),F.Room->ConfigureHistoricalEpochCountForTesting(Id,8))) return false;
    if(!Check(2)) return false;
    auto& Prop=F.Room->Tracked.FindChecked(Id);
    for(auto& R:Prop.History.GetMutableRecords())
    {
     auto& V=Prop.Visuals.FindChecked(R.Epoch);
     const FName PreviousCap=V.Cap.IsValid()?V.Cap->GetFName():NAME_None;
     Released.Append(F.Room->GetOwnedPresentationObjectsForTesting());
     const auto PreviousMaterials=V.Materials;
     F.Room->DestroyVisual(V,false);
     for(const auto& M:PreviousMaterials) TestFalse(TEXT("Retired MID leaves owning array"),F.Room->OwnedMaterials.Contains(M.Get()));
     F.Room->EnsureRecordVisual(Prop,R); F.Room->UpdateRecordTexture(Prop,R); F.Room->UpdateRecordCap(Prop,R);
     if(!Legacy && PreviousCap!=NAME_None)
      TestTrue(TEXT("Cap reconstruction cannot overwrite pending destruction"),V.Cap.IsValid() && V.Cap->GetFName()!=PreviousCap);
     const int32 Materials=F.Room->OwnedMaterials.Num();
     F.Room->EnsureRecordVisual(Prop,R);
     TestEqual(TEXT("Ensure is allocation-idempotent"),F.Room->OwnedMaterials.Num(),Materials);
    }
    if(!Check(3)) return false;
    Released.Append(F.Room->GetOwnedPresentationObjectsForTesting());
   }
   CollectGarbage(RF_NoFlags,true);
   for(const auto& Object:Released) TestFalse(TEXT("Reset/world teardown releases all presentation objects"),Object.IsValid());
  }
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellJoinedOccupancyParity,
 "Darkwell.PropLab.GrayObjectPolicy.JoinedOccupancyParityAndInvalidation",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellJoinedOccupancyParity::RunTest(const FString&)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 FRoom F;
 auto& Scene=*F.Room;
 using FGeometry=ADarkwellObjectMemoryScene::FPrimitiveGeometrySnapshot;
 using FSnapshot=ADarkwellObjectMemoryScene::FActualOccupancySnapshot;
 TGuardValue<bool> FrameScope(Scene.bUseFrameOccupancy,true);
 Scene.bCacheFrameOccupancyPoints=true;
 int32 Compared=0;
 auto SameCache=[&](const TMap<FVector2D,bool>& A,const TMap<FVector2D,bool>& B)
 {
  if(A.Num()!=B.Num()) return false;
  for(const auto& Pair:A) { const bool* V=B.Find(Pair.Key); if(!V || *V!=Pair.Value) return false; }
  return true;
 };
 // Analytic cells include a thin strip crossing cell edges but no centers.
 // These tests do not derive expected thin-edge results from the serial path.
 for(int32 Case=0;Case<16;++Case)
 {
  const FIntPoint Size=Case==10?FIntPoint(16,16):FIntPoint(128,128);
  const FBox2D Bounds(FVector2D(0,0),FVector2D(Size));
  Scene.FrameOccupancy.Reset(); Scene.FrameOccupancyPoints.Reset();
  Scene.bUseFrameOccupancy=Case!=11;
  Scene.bFilterFrameOccupancy=Case==3 || Case==13 || Case==15;
  Scene.FrameOccupancyCandidates={};
  Scene.bCacheFrameOccupancyPoints=Case!=7;
  auto AddGeometry=[&](FBox Box,FTransform Pose)
  {
   FGeometry Geometry; Geometry.LocalBounds=Box; Geometry.WorldTransform=Pose;
   Geometry.PrimitiveIndex=0; Geometry.CachePlanarProjection();
   FSnapshot Snapshot; Snapshot.StableId=FName(TEXT("Occupancy.Oracle.Physical"));
   const FBox WorldBox=Box.TransformBy(Pose);
   Snapshot.Bounds=FBox2D(FVector2D(WorldBox.Min),FVector2D(WorldBox.Max));
   Snapshot.Geometry.Add(Geometry); Scene.FrameOccupancy.Add(MoveTemp(Snapshot));
  };
  if(Case!=4)
  {
   AddGeometry(FBox(FVector(0.99,0,-1),FVector(1.01,128,1)),FTransform::Identity);
   FTransform Pose(FRotator(Case==5?23:0,37,0),FVector(30,30,0),Case==6?FVector(-1,2,1):Case==14?FVector(0,1,1):FVector(1));
   AddGeometry(FBox(FVector(-10,-3,-8),FVector(10,3,8)),Pose);
  }
  TArray<const FSnapshot*> Candidates;
  if(Case==13) { for(const auto& Snapshot:Scene.FrameOccupancy) Candidates.Add(&Snapshot); Scene.FrameOccupancyCandidates=MakeArrayView(Candidates); }
  TArray<int32> Indices;
  for(int32 I=0;I<Size.X*Size.Y;++I) Indices.Add(I);
  if(Case==12) { Indices.Add(0); Indices.Add(0); } // overlapping outputs only written on GT
  TBitArray<> Mask(true,Size.X*Size.Y);
  if(Case==2) for(int32 I=0;I<Mask.Num();I+=2) Mask[I]=false;
  const TBitArray<>* Whole=Case==0 || Case==11 || Case==15?nullptr:&Mask;
  if(Case==8) for(int32 I=0;I<131070;++I) Scene.FrameOccupancyPoints.Add(FVector2D(-I-1,-1),false);
  if(Case==9)
  {
   Scene.bForceSerialOccupancyForTesting=true;
   TBitArray<> Warm(false,Mask.Num()); Scene.BuildOccupiedSamples(Bounds,Size,Indices,Whole,Warm);
  }
  const auto CacheBefore=Scene.FrameOccupancyPoints;
  TBitArray<> Serial(true,Mask.Num()), Joined=Serial;
  Scene.bForceSerialOccupancyForTesting=true;
  const auto CountsBefore=Scene.RuntimeFrame;
  Scene.BuildOccupiedSamples(Bounds,Size,Indices,Whole,Serial);
  const auto CountsSerial=Scene.RuntimeFrame; const auto CacheSerial=Scene.FrameOccupancyPoints;
  Scene.FrameOccupancyPoints=CacheBefore; Scene.RuntimeFrame=CountsBefore;
  Scene.bForceSerialOccupancyForTesting=false;
  const int32 JoinedBefore=Scene.JoinedOccupancyBuildsForTesting;
  Scene.BuildOccupiedSamples(Bounds,Size,Indices,Whole,Joined);
  if(!TestTrue(*FString::Printf(TEXT("All occupancy bits/cache values and capacity equal case=%d"),Case),
   Serial==Joined && SameCache(CacheSerial,Scene.FrameOccupancyPoints))) return false;
  TestEqual(TEXT("Same logical sample count"),Scene.RuntimeFrame.OccupancyTests,CountsSerial.OccupancyTests);
  if(Case!=12) TestEqual(TEXT("Same real geometry query count for unique centers"),Scene.RuntimeFrame.PrimitiveGeometryTests,CountsSerial.PrimitiveGeometryTests);
  if(Case==0) TestFalse(TEXT("Thin strip does not contain cell zero center"),Joined[0]);
  if(Case==1 || Case==3) TestTrue(TEXT("Whole retains physical thin strip at cell edge even without center candidates"),Joined[0]);
  if(Case==2) TestFalse(TEXT("Whole mask exclusion is authoritative"),Joined[0]);
  if(Case==4) TestEqual(TEXT("Empty physical frame produces empty occupancy"),Joined.CountSetBits(),0);
  if(Case==15) TestTrue(TEXT("Empty Partial ROI clears bits without populating point cache"),Joined.CountSetBits()==0 && Scene.FrameOccupancyPoints.IsEmpty());
  if(Case==8) TestEqual(TEXT("Point cache retains original cap"),Scene.FrameOccupancyPoints.Num(),131072);
  TestEqual(TEXT("Only nonconstant large snapshot batches join"),Scene.JoinedOccupancyBuildsForTesting-JoinedBefore,Case==10 || Case==11 || Case==15?0:1);
  ++Compared;
 }
 Scene.bFilterFrameOccupancy=false; Scene.FrameOccupancyCandidates={}; Scene.bUseFrameOccupancy=true; Scene.bCacheFrameOccupancyPoints=true;
 // A coarse grid large enough to dispatch, followed by sparse fine-to-coarse
 // invalidation. Assert against independent center queries, including unchanged bits.
 {
  FDarkwellSpatialObservationRecord CoarseRecord;
  CoarseRecord.SpatialMemory.Initialize(Id,FBox2D(FVector2D(0,0),FVector2D(128,128)),1);
  CoarseRecord.SpatialMemory.BeginPresent();
  CoarseRecord.SpatialMemory.BeginAbsent();
  CoarseRecord.FineHistory.Initialize(CoarseRecord.SpatialMemory);
  ADarkwellObjectMemoryScene::FRecordVisual CoarseVisual;
  const int32 BeforeJoined=Scene.JoinedOccupancyBuildsForTesting;
  Scene.FrameOccupancyPoints.Reset();
  Scene.UpdateCoarseOccupancy(CoarseRecord,CoarseVisual,{},true);
  TestEqual(TEXT("Large coarse batch really joins"),Scene.JoinedOccupancyBuildsForTesting,BeforeJoined+1);
  TBitArray<> Expected(false,128*128);
  for(int32 I=0;I<Expected.Num();++I) Expected[I]=Scene.IsOccupiedByActual(FVector2D(I%128+.5,I/128+.5),NAME_None);
  TestTrue(TEXT("Every coarse center matches independent serial point oracle"),CoarseVisual.CachedCoarseOccupied==Expected);
  Scene.FrameOccupancy.Reset(); Scene.FrameOccupancyPoints.Reset();
  TArray<int32> FineDirty{0,4,512*4};
  Expected[0]=false; Expected[1]=false; Expected[128]=false;
  Scene.UpdateCoarseOccupancy(CoarseRecord,CoarseVisual,FineDirty,false);
  TestTrue(TEXT("Sparse coarse mapping preserves all untouched bits"),CoarseVisual.CachedCoarseOccupied==Expected);
  TestEqual(TEXT("Sparse coarse mapping stays serial"),Scene.JoinedOccupancyBuildsForTesting,BeforeJoined+1);
  ++Compared;
 }
 Scene.FrameOccupancy.Reset(); Scene.FrameOccupancyPoints.Reset();
 Scene.ResetTrackedRevealPolicyForLab(Id,Reveal::SpatialPartial,100,History::StationaryOnly);
 if(!TestTrue(TEXT("Seed records for production dirty/coarse path"),Scene.ConfigureHistoricalEpochCountForTesting(Id,2))) return false;
 auto& Prop=Scene.Tracked.FindChecked(Id);
 auto& Record=Prop.History.GetMutableRecords()[0];
 auto& Visual=Prop.Visuals.FindChecked(Record.Epoch);
 const auto CaptureBefore=Record.LastLegalCaptureMask;
 const auto FineBefore=Record.FineHistory.GetSamples();
 TArray<FDarkwellHistoryGridV2::FSample> Samples; Samples.Append(FineBefore.GetData(),FineBefore.Num());
 FSnapshot Physical; Physical.StableId=Id; Physical.Bounds=Record.FineHistory.GetBounds();
 Physical.Geometry=Visual.PartGeometry; Scene.FrameOccupancy.Add(Physical);
 Visual.ProcessedGeometryRevision=0; Visual.ProcessedOwnershipRevision=0;
 Visual.CachedFineOccupied.Reset(); Visual.CachedCoarseOccupied.Reset();
 for(int32 Phase=0;Phase<7;++Phase)
 {
  if(Phase==2) { ++Prop.ObservationOwnershipRevision; Prop.CurrentLive.OwnershipDirtyRegions.Add(Record.FineHistory.GetBounds()); }
  if(Phase==3) { Scene.FrameOccupancy[0].Geometry[0].WorldTransform.AddToTranslation(FVector(1.e-8,0,0)); Scene.FrameOccupancy[0].Geometry[0].CachePlanarProjection(); ++Scene.GeometryRevision; }
  if(Phase==4) { Scene.FrameOccupancy.Reset(); ++Scene.GeometryRevision; }
  if(Phase==5) { Scene.FrameOccupancy.Add(Physical); ++Scene.GeometryRevision; Record.bConfirmedWholeCapture=true; Visual.ProcessedGeometryRevision=0; }
  if(Phase==6) { Visual.CachedFineOccupied.Reset(); Visual.CachedCoarseOccupied.Reset(); ++Scene.GeometryRevision; }
  Scene.FrameHistoryGeometry.Reset(); Scene.FrameOccupancyPoints.Reset();
  const auto Before=Visual;
  const auto Telemetry=Scene.RuntimeFrame;
  TArray<int32> SerialDirty,SerialPhysical,JoinedDirty,JoinedPhysical;
  Scene.bForceSerialOccupancyForTesting=true;
  Scene.BuildGeometryDirtyIndices(Prop,Record,Visual,SerialDirty,SerialPhysical);
  Scene.UpdateCoarseOccupancy(Record,Visual,SerialPhysical,true);
  const auto SerialVisual=Visual; const auto SerialCache=Scene.FrameOccupancyPoints;
  Visual=Before; Scene.FrameHistoryGeometry.Reset(); Scene.FrameOccupancyPoints.Reset(); Scene.RuntimeFrame=Telemetry;
  Scene.bForceSerialOccupancyForTesting=false;
  Scene.BuildGeometryDirtyIndices(Prop,Record,Visual,JoinedDirty,JoinedPhysical);
  Scene.UpdateCoarseOccupancy(Record,Visual,JoinedPhysical,true);
  if(!TestTrue(*FString::Printf(TEXT("Dirty lists, fine/coarse bits, revisions and point cache match phase=%d"),Phase),
   SerialDirty==JoinedDirty && SerialPhysical==JoinedPhysical && SerialVisual.CachedFineOccupied==Visual.CachedFineOccupied
   && SerialVisual.CachedCoarseOccupied==Visual.CachedCoarseOccupied && SerialVisual.ProcessedGeometryRevision==Visual.ProcessedGeometryRevision
   && SerialVisual.ProcessedOwnershipRevision==Visual.ProcessedOwnershipRevision && SameCache(SerialCache,Scene.FrameOccupancyPoints))) return false;
  if(Phase==1) TestTrue(TEXT("Unchanged revisions produce no dirty samples"),JoinedDirty.IsEmpty());
  if(Phase==2) TestTrue(TEXT("Ownership-only dirty preserves exact physical occupancy"),!JoinedDirty.IsEmpty() && JoinedPhysical.IsEmpty());
  if(Phase==3) TestTrue(TEXT("Sub-tolerance physical motion invalidates occupancy"),!JoinedPhysical.IsEmpty());
  if(Phase==4) TestEqual(TEXT("Removed/collision-disabled geometry clears occupancy"),Visual.CachedFineOccupied.CountSetBits(),0);
  // Replay the SAME before-state to exercise the frame geometry reuse path.
  const int32 JoinedCount=Scene.JoinedOccupancyBuildsForTesting;
  Visual=Before; TArray<int32> ReusedDirty,ReusedPhysical;
  Scene.BuildGeometryDirtyIndices(Prop,Record,Visual,ReusedDirty,ReusedPhysical);
  TestTrue(TEXT("Geometry cache replay retains exact fine output and lists"),Visual.CachedFineOccupied==SerialVisual.CachedFineOccupied && ReusedDirty==SerialDirty && ReusedPhysical==SerialPhysical);
  TestEqual(TEXT("Geometry reuse dispatches no duplicate worker batch"),Scene.JoinedOccupancyBuildsForTesting,JoinedCount);
  Visual=SerialVisual; ++Compared;
 }
 TestTrue(TEXT("Occupancy never changes captured knowledge mask"),Record.LastLegalCaptureMask==CaptureBefore);
 for(int32 I=0;I<Samples.Num();++I)
 {
  const auto& A=Samples[I]; const auto& B=Record.FineHistory.GetSamples()[I];
  if(!TestTrue(TEXT("Occupancy does not write fine authority fields"),A.State==B.State && A.Opacity==B.Opacity && A.InitialRemembered==B.InitialRemembered
   && A.FrozenAAEnvelope==B.FrozenAAEnvelope && A.EmptyDwell==B.EmptyDwell && A.bVerifiedEmpty==B.bVerifiedEmpty)) return false;
 }
 TestTrue(TEXT("Non-vacuous joined occupancy coverage"),Scene.JoinedOccupancyBuildsForTesting>=12);
 AddInfo(FString::Printf(TEXT("OCCUPANCY_PARITY compared=%d joined=%d; thin Whole/Partial, rotated/tilted/negative-scale, empty ROI/frame, cache capacity/hits, small/live fallback, duplicate indices, dirty/reuse/revision/coarse/world teardown"),Compared,Scene.JoinedOccupancyBuildsForTesting));
 return true;
}

// Bounded CPU diagnostic of the actual 64+64+56 stress constructor. Kept out
// of the functional manifest and never reported as a real D3D12 frame sample.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellJoinedDistributedBatch,
 "Darkwell.Stabilization.Diagnostics.JoinedDistributedBatch",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellJoinedDistributedBatch::RunTest(const FString&)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 for(int32 Run=0;Run<4;++Run)
 {
  const bool Serial=Run%2==0;
  FRoom F(false,true); F.Room->bForceSerialOwnershipForTesting=Serial;
  F.Player->SetActorLocation(FVector(6000,3600,92));
  F.Player->SetActorRotation(FRotator(0,90,0)); F.Step(2);
  const double Begin=FPlatformTime::Seconds();
  if(!TestTrue(TEXT("Actual distributed stress constructor succeeds"),F.Room->SetGrayPolicyStressMode(6))) return false;
  const double SetupMs=(FPlatformTime::Seconds()-Begin)*1000;
  for(int32 Frame=0;Frame<3;++Frame)
  {
   const double Start=FPlatformTime::Seconds(); F.Step();
   const double StepMs=(FPlatformTime::Seconds()-Start)*1000;
   const auto T=F.Room->GetHistoryRuntimeFrameTelemetryForTesting();
   if(Frame==0 && !Serial) TestTrue(TEXT("184 workload uses joined ownership"),T.JoinedOwnershipBatches>0);
   AddInfo(FString::Printf(TEXT("GRAY_DISTRIBUTED_CPU run=%d serial=%d frame=%d setup_ms=%.3f step_ms=%.3f memory_ms=%.3f ownership_ms=%.3f cap_ms=%.3f occupancy_ms=%.3f fine_ms=%.3f texture_ms=%.3f batches=%llu input_samples=%llu records=%d geometry_tests=%llu visits=%llu"),
    Run,Serial,Frame,SetupMs,StepMs,T.MovingPropLabGameThreadUs/1000,T.OwnershipUs/1000,T.CapPresentationUs/1000,
    T.OccupancyUs/1000,T.AdvanceFineHistoryUs/1000,T.TextureSubmissionUs/1000,T.JoinedOwnershipBatches,T.JoinedOwnershipSamples,T.SpatialRecordCount,T.PrimitiveGeometryTests,T.OwnershipRecordVisits));
  }
 }
 return true;
}

// Separate diagnostic selector: measures cold CPU work without a frame gate.
// Each size starts in a fresh world; no warm-up is inserted after seeding.
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FDarkwellOwnershipScaling,
 "Darkwell.Stabilization.Diagnostics.OwnershipScaling",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FDarkwellOwnershipScaling::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
 for(int32 Count:{8,16,32,64}) { const auto Name=FString::FromInt(Count); Names.Add(Name); Commands.Add(Name); }
}
bool FDarkwellOwnershipScaling::RunTest(const FString& Case)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 const int32 Count=FCString::Atoi(*Case);
 FRoom F; F.Face(-90); F.Step(2);
 const double Start=FPlatformTime::Seconds();
 if(!TestTrue(TEXT("All distinct poses are seeded"),F.Room->ConfigureHistoricalEpochCountForTesting(Id,Count))) return false;
 const double SetupMs=(FPlatformTime::Seconds()-Start)*1000;
 TestEqual(TEXT("Cold records are not hidden in warm-up"),F.Room->GetSpatialRecordCount(Id),Count);
 for(int32 Frame=0;Frame<3;++Frame)
 {
  const double Begin=FPlatformTime::Seconds(); F.Step();
  const double StepMs=(FPlatformTime::Seconds()-Begin)*1000;
  const auto P=F.Room->GetHistoryRuntimeFrameTelemetryForTesting();
  AddInfo(FString::Printf(TEXT("GRAY_ARCH_SCALE count=%d frame=%d setup_ms=%.3f step_ms=%.3f memory_ms=%.3f ownership_ms=%.3f cap_ms=%.3f occupancy_ms=%.3f fine_ms=%.3f texture_ms=%.3f resident_samples=%d scanned=%llu records=%d geometry_tests=%llu ownership_visits=%llu footprint_queries=%llu cap_signature_samples=%llu gpu_uploads=%llu"),
   Count,Frame,SetupMs,StepMs,P.MovingPropLabGameThreadUs/1000,P.OwnershipUs/1000,P.CapPresentationUs/1000,P.OccupancyUs/1000,P.AdvanceFineHistoryUs/1000,P.TextureSubmissionUs/1000,
   P.FineSamplesResident,P.FineSamplesScanned,P.SpatialRecordCount,P.PrimitiveGeometryTests,P.OwnershipRecordVisits,P.OwnershipFootprintQueries,P.CapSignatureSamples,P.GpuTextureUploads));
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellTerminalSceneCompaction,
 "Darkwell.PropLab.ArchitectureAudit.TerminalSceneCompaction",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellTerminalSceneCompaction::RunTest(const FString&)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 FRoom F; F.Face(90); F.Step(20); F.Face(-90); F.Step(20);
 auto& Prop=F.Room->Tracked.FindChecked(Id);
 // Duplicate an actually acquired capture to isolate the residency rule.
 // This does not act as the independent observation/visibility oracle.
 const auto Capture=Prop.History.GetRecords()[0];
 for(int32 I=0;I<3;++I)
 {
  const int32 Index=Prop.History.BeginCurrentObservation(Capture.SnapshotTransform,Capture.SpatialMemory.GetBounds(),2.5);
  auto& Record=Prop.History.GetMutableRecords()[Index]; const uint32 Epoch=Record.Epoch;
  Record=Capture; Record.Epoch=Epoch; Record.bCurrentObservedLocation=true; Record.SpatialMemory.BeginPresent();
  TestTrue(TEXT("Duplicate captured state seals for residency fixture"),Prop.History.FreezeCurrentFromGeometryMask(Capture.LastLegalCaptureMask));
  F.Room->EnsureRecordVisual(Prop,Record); F.Room->UpdateRecordTexture(Prop,Record); F.Room->UpdateRecordCap(Prop,Record);
 }
 ++F.Room->GeometryRevision; ++Prop.ObservationOwnershipRevision;
 F.Room->bHistoricalSpatialIndexDirty=true;
 const uint32 OldEpoch=Prop.History.GetRecords()[0].Epoch;
 TestFalse(TEXT("Old state has no empty proof"),Prop.History.GetRecords()[0].FineHistory.IsFullyVerifiedEmpty());
 F.Step(3);
 if(!TestEqual(TEXT("Fully replaced captured states release dense residency"),Prop.History.GetRecords().Num(),1)) return false;
 TestEqual(TEXT("Three terminal captures compact independently of empty evidence"),Prop.History.GetCompactedRecordCount(),uint64(3));
 TestTrue(TEXT("The latest knowledge remains visible"),F.Room->GetVisibleHistoricalProxyCountForTesting(Id)>0);
 if(!TestFalse(TEXT("Deleted state cannot rebuild from raw capture"),Prop.History.ResumeUncontradictedObservation(OldEpoch))) return false;
 F.Step(30);
 TestEqual(TEXT("Leaving unchanged unverified scene preserves the remaining state"),Prop.History.GetRecords().Num(),1);
 return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FDarkwellMemoryEpisodeContract,
 "Darkwell.PropLab.ArchitectureAudit.StaticPartialEpisodes",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FDarkwellMemoryEpisodeContract::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
 for(const TCHAR* Name:{TEXT("30Hz"),TEXT("60Hz"),TEXT("120Hz"),TEXT("144Hz"),TEXT("ReverseOrder"),TEXT("LargeDelta"),TEXT("ResumedThenHiddenMotion"),TEXT("ExploreForward"),TEXT("ExploreReverse"),TEXT("ErasedDoesNotRebuild")})
 { Names.Add(Name); Commands.Add(Name); }
}
bool FDarkwellMemoryEpisodeContract::RunTest(const FString& Case)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 FRoom F;
 F.Room->DestroyTracked();
 FResolvedSightWeaveObjectPolicy Policy;
 Policy.RevealMode=Reveal::SpatialPartial; Policy.HistoryMode=History::StationaryOnly;
 F.Room->SpawnTracked(Id,4,FVector(620,75,115),FLinearColor(.62,.42,.18),
  FTransform(FVector(-300,650,0)),ESightWeaveObjectPolicySource::UseProjectDefault,History::StationaryOnly,&Policy);
 const int32 Hz=Case.EndsWith(TEXT("Hz"))?FCString::Atoi(*Case):60;
 const float Dt=Case==TEXT("LargeDelta")?.4f:1.f/Hz;
 const int32 Frames=Case==TEXT("LargeDelta")?1:FMath::CeilToInt(Hz/3.f);
 const bool Explore=Case.StartsWith(TEXT("Explore"));
 if(Explore)
 {
  TArray<float> Order=Case==TEXT("ExploreReverse")?TArray<float>{52,40,28,18}:TArray<float>{18,28,40,52};
  for(float Yaw:Order) { F.Face(Yaw); F.Step(Frames,Dt); F.Face(-90); F.Step(Frames,Dt); }
 }
 else { F.Face(90); F.Step(Frames,Dt); F.Face(-90); F.Step(Frames,Dt); }
 const int32 InitialRecords=F.Room->GetSpatialRecordCount(Id);
 const auto InitialTextures=F.Room->Tracked.FindChecked(Id).CurrentPresentation.LiveTextures;
 const auto InitialProxy=F.Room->Tracked.FindChecked(Id).Visuals.CreateConstIterator().Value().Proxy;
 if(Case==TEXT("ErasedDoesNotRebuild"))
 {
  auto& Prop=F.Room->Tracked.FindChecked(Id);
  const auto Pose=Prop.Actual->GetActorTransform();
  Prop.ObjectPolicy->SetSightWeaveMoving(true); Prop.Actual->AddActorWorldOffset(FVector(450,0,0)); F.Step(Frames,Dt);
  Prop.ObjectPolicy->SetSightWeaveMoving(false);
  F.Face(160); F.Step(Frames,Dt); F.Face(-90); F.Step(Frames,Dt);
  const uint32 Epoch=Prop.History.GetRecords()[0].Epoch;
  auto* Record=Prop.History.FindRecord(Epoch);
  TArray<int32> Erased;
  for(int32 I=0; I<Record->FineHistory.GetSamples().Num(); ++I)
   if(Record->FineHistory.GetSamples()[I].bVerifiedEmpty) Erased.Add(I);
  TestTrue(TEXT("Real legal empty view erased a strict subset"),!Erased.IsEmpty() && Erased.Num()<Record->FineHistory.GetSamples().Num());
  TestFalse(TEXT("Counterevidence forbids raw-capture reuse"),Prop.History.ResumeUncontradictedObservation(Epoch));
  // Returning the actual object in darkness grants no new knowledge.
  Prop.ObjectPolicy->SetSightWeaveMoving(true); Prop.Actual->SetActorTransform(Pose); F.Step(Frames,Dt);
  Prop.ObjectPolicy->SetSightWeaveMoving(false); F.Step(Frames,Dt);
  F.Room->DestroyVisual(Prop.Visuals.FindChecked(Epoch)); Prop.Visuals.Remove(Epoch);
  Record=Prop.History.FindRecord(Epoch);
  F.Room->EnsureRecordVisual(Prop,*Record); F.Room->UpdateRecordTexture(Prop,*Record); F.Room->UpdateRecordCap(Prop,*Record);
  const auto& Pixels=Prop.Visuals.FindChecked(Epoch).SubmittedPresentation;
  int32 Resurrected=0;
  for(int32 I:Erased) Resurrected+=!Record->FineHistory.GetSamples()[I].bVerifiedEmpty || (Pixels[I].A*Pixels[I].B)>0;
  TestEqual(TEXT("Resource reconstruction cannot resurrect erased samples"),Resurrected,0);
  return true;
 }
 if(Case==TEXT("ResumedThenHiddenMotion"))
 {
  F.Face(90); F.Step(Frames,Dt);
  auto& Prop=F.Room->Tracked.FindChecked(Id);
  Prop.ObjectPolicy->SetSightWeaveMoving(true);
  F.Face(-90); Prop.Actual->AddActorWorldOffset(FVector(450,0,0)); F.Step(Frames,Dt);
  TestEqual(TEXT("Leaving with motion cannot discard previously sealed knowledge"),F.Room->GetStaleEpochCountForTesting(Id),1);
  TestTrue(TEXT("Unverified old memory remains drawable"),F.Room->GetVisibleHistoricalProxyCountForTesting(Id)>0);
  Prop.ObjectPolicy->SetSightWeaveMoving(false); F.Step(Frames,Dt);
  TestEqual(TEXT("Hidden stop does not add a new endpoint"),F.Room->GetSpatialRecordCount(Id),1);
  return true;
 }
 int32 Missing=0, InternalCaps=0;
 TArray<float> Yaws{52.f,40.f,28.f,52.f,40.f,28.f};
 if(Case==TEXT("ReverseOrder")) Yaws={28.f,40.f,52.f,28.f,40.f,52.f};
 for(float Yaw:Yaws)
 {
  F.Face(Yaw); F.Step(Frames,Dt); F.Face(-90); F.Step(Frames,Dt);
  const auto& Prop=F.Room->Tracked.FindChecked(Id);
  // Independent analytic oracle: a single solid cuboid, fully observed before
  // these repeated subset views. Every interior point remains known and solid.
  for(double X=-580;X<-20;X+=.625)
  {
   const FVector2D Point(X,650);
   double Sum=0;
   for(const auto& R:Prop.History.GetRecords())
   {
    if(R.bCurrentObservedLocation) continue;
    const auto* V=Prop.Visuals.Find(R.Epoch); if(!V || V->bPresentationRetired) continue;
    const auto B=R.FineHistory.GetBounds(); const auto S=R.FineHistory.GetSize();
    const auto UV=(Point-B.Min)/B.GetSize();
    const int32 I=FMath::FloorToInt(UV.Y*S.Y)*S.X+FMath::FloorToInt(UV.X*S.X);
    if(V->SubmittedPresentation.IsValidIndex(I)) Sum+=V->SubmittedPresentation[I].B*V->SubmittedPresentation[I].A;
   }
   if(!Explore || X>-390) Missing+=Sum<.999;
   if(Explore && X<-490) Missing+=Sum>.001;
  }
  for(const auto& Pair:Prop.Visuals) for(const auto& Q:Pair.Value.CapQuads)
   InternalCaps+=Q.A.X>(Explore?-390:-600) && Q.A.X<0 && Q.A.Y>615 && Q.A.Y<685;
 }
 TestEqual(TEXT("Fully known interior has no surface deficit after subset reobservations"),Missing,0);
 TestEqual(TEXT("Fully known cuboid has no artificial internal caps"),InternalCaps,0);
 if(Explore) TestTrue(TEXT("External known/unknown cut still has a cap"),F.Room->GetVisibleHistoricalCapCountForTesting(Id)>0);
 TestEqual(TEXT("Repeated operations without new knowledge do not grow retained records"),F.Room->GetSpatialRecordCount(Id),InitialRecords);
 TestTrue(TEXT("Same source reuses live texture allocations across episodes"),InitialTextures==F.Room->Tracked.FindChecked(Id).CurrentPresentation.LiveTextures);
 TestTrue(TEXT("Unchanged captured state reuses its concrete history proxy"),InitialProxy==F.Room->Tracked.FindChecked(Id).Visuals.CreateConstIterator().Value().Proxy);
 AddInfo(FString::Printf(TEXT("EPISODE_ORACLE missing=%d internal_caps=%d initial_records=%d final_records=%d"),Missing,InternalCaps,InitialRecords,F.Room->GetSpatialRecordCount(Id)));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellObservedContentContract,
 "Darkwell.PropLab.ArchitectureAudit.ObservedContentSurvivesSource",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellObservedContentContract::RunTest(const FString&)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 for(auto Mode:{Reveal::SpatialPartial,Reveal::WholeObjectAfterSpan})
 {
  FRoom F;
  F.Room->ResetTrackedRevealPolicyForLab(Id,Mode,100,History::StationaryOnly);
  F.Face(90); F.Step(20); F.Face(-90); F.Step(20);
  auto& Prop=F.Room->Tracked.FindChecked(Id);
  auto* Source=Prop.Actual.Get(); auto* Memory=Source->FindComponentByClass<UDarkwellRememberablePropComponent>();
  const uint32 Epoch=Prop.History.GetRecords()[0].Epoch;
  const auto Original=Prop.History.GetRecords()[0];
  TestTrue(TEXT("Legal capture retains content"),!Original.Primitives.IsEmpty() && Original.ContentRevision!=0);
  const uint64 BeforePose=Memory->ComputeMemoryContentRevision();
  const auto Pose=Source->GetActorTransform();
  Source->SetActorRotation(FRotator(0,23,0));
  TestEqual(TEXT("Authored content version excludes world pose"),Memory->ComputeMemoryContentRevision(),BeforePose);
  Source->SetActorTransform(Pose);
  Memory->SetMemoryAppearance(FLinearColor::Red,7);
  auto* Part=Memory->GetMemoryPrimitives()[0].Get();
  Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
  Part->SetRelativeScale3D(FVector(3,2,1));
  F.Step(20);
  TestEqual(TEXT("Hidden changes create no observed content"),Prop.History.GetRecords().Num(),1);
  auto* Record=Prop.History.FindRecord(Epoch);
  TestTrue(TEXT("Hidden appearance cannot rewrite remembered tint"),Record->Tint==Original.Tint);
  TestTrue(TEXT("Hidden geometry cannot rewrite observed mesh"),Record->Primitives[0].Mesh==Original.Primitives[0].Mesh);
  F.Room->DestroyVisual(Prop.Visuals.FindChecked(Epoch)); Prop.Visuals.Remove(Epoch);
  Source->Destroy(); Prop.Actual.Reset(); Prop.bExists=false;
  F.Room->EnsureRecordVisual(Prop,*Record);
  auto& Visual=Prop.Visuals.FindChecked(Epoch);
  TestTrue(TEXT("Memory proxy rebuilds without a live source"),Visual.Proxy.IsValid());
  if(!Visual.Proxy.IsValid()) return false;
  TArray<UStaticMeshComponent*> Parts; Visual.Proxy->GetComponents(Parts);
  TestEqual(TEXT("Rebuilt part count comes from capture"),Parts.Num(),Original.Primitives.Num());
  if(Parts.IsEmpty()) return false;
  TestTrue(TEXT("Rebuilt mesh is the observed mesh"),Parts[0]->GetStaticMesh()==Original.Primitives[0].Mesh.Get());
  TestTrue(TEXT("Rebuilt placement is the observed placement"),Parts[0]->GetComponentTransform().Equals(Original.Primitives[0].RelativeTransform*Original.SnapshotTransform));
  auto* Material=Cast<UMaterialInstanceDynamic>(Parts[0]->GetMaterial(0));
  FLinearColor Tint; float UV=0;
  TestTrue(TEXT("Rebuilt tint is observed content"),Material && Material->GetVectorParameterValue(TEXT("OriginalBaseColorTint"),Tint) && Tint==Original.Tint);
  TestTrue(TEXT("Rebuilt UV scale is observed content"),Material && Material->GetScalarParameterValue(TEXT("OriginalUVScale"),UV) && UV==Original.UVScale);
 }
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
 for(const FVector2D WorldOffset : {FVector2D::ZeroVector,FVector2D(-2000,1700),FVector2D(1800,-2200)})
 {
   // Singular inverse transforms can accept points outside their collapsed
   // world AABB. Exercise those points independently of TransformPosition.
   const FVector2D P=FVector2D(G.WorldTransform.TransformPosition(FVector(X,Y,30)))+WorldOffset;
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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellWholePreparationHandoff,"Darkwell.ObjectMemory.Preparation.FirstWholeHandoff",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellWholePreparationHandoff::RunTest(const FString&)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 auto* Mode = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Darkwell.ObjectMemory.WholeGeometryPreparation"));
 const int32 Previous = Mode->GetInt(); ON_SCOPE_EXIT { Mode->Set(Previous, ECVF_SetByCode); };
 TBitArray<> OracleCapture, OracleFrozen;
 for (int32 M = 0; M <= 2; ++M)
 {
  Mode->Set(M, ECVF_SetByCode);
  FRoom F;
  F.Room->ResetTrackedRevealPolicyForLab(Id,Reveal::WholeObjectAfterSpan,100,History::StationaryOnly);
  for (uint64 Frame = 1; Frame <= 30; ++Frame) { F.Room->WholePreparationFrameForTesting=Frame; F.Step(); }
  AddInfo(F.Room->GetWholePreparationTelemetry());
  if (M > 0) TestTrue(TEXT("unchanged coverage does not readmit"), F.Room->GetWholePreparationTelemetry().Contains(TEXT("\"requests\":1,")));
  if (M > 0) TestTrue(TEXT("lead time produces Ready"), F.Room->GetWholePreparationTelemetry().Contains(TEXT("\"ready\":1")));
  const auto Before = F.Room->GetWholePreparationTelemetry();
  F.Room->AdvanceWholePreparation();
  auto WorkField=[](const FString& Value) { const int32 Start=Value.Find(TEXT("\"frame_work\":")); return Value.Mid(Start,Value.Find(TEXT(",\"frame_ms\""))-Start); };
  TestEqual(TEXT("repeat update cannot replenish work"), WorkField(F.Room->GetWholePreparationTelemetry()),WorkField(Before));
  F.Face(-90); F.Room->WholePreparationFrameForTesting=31; F.Step();
  TBitArray<> Capture,Frozen;
  TestTrue(TEXT("first exit synchronously seals"),F.Room->GetNewestCaptureMasksForTesting(Id,Capture,Frozen));
  if (M == 0) { OracleCapture=Capture; OracleFrozen=Frozen; }
  else { TestTrue(TEXT("capture parity"),Capture==OracleCapture); TestTrue(TEXT("fine parity"),Frozen==OracleFrozen); }
  if (M == 1) TestTrue(TEXT("shadow consumed"),F.Room->GetWholePreparationTelemetry().Contains(TEXT("\"shadow\":1")));
  if (M == 2) TestTrue(TEXT("Ready consumed"),F.Room->GetWholePreparationTelemetry().Contains(TEXT("\"hits\":1")));
  AddInfo(F.Room->GetWholePreparationTelemetry());
  F.Room->ResetMemory();
  TestTrue(TEXT("reset releases all staging"), F.Room->GetWholePreparationTelemetry().Contains(TEXT("\"pending\":0")));
 }
 Mode->Set(2, ECVF_SetByCode);
 for (int32 Case = 0; Case < 8; ++Case)
 {
  FRoom F;
  F.Room->ResetTrackedRevealPolicyForLab(Id,Reveal::WholeObjectAfterSpan,100,History::StationaryOnly);
  for (uint64 Frame=1; Frame<=30; ++Frame) { F.Room->WholePreparationFrameForTesting=Frame; F.Step(); }
  auto* Prop = F.Room->Tracked.Find(Id);
  TestNotNull(TEXT("tracked source"), Prop);
  const auto Index = Prop->History.GetCurrentIndex();
  TestTrue(TEXT("has Current"),Index!=INDEX_NONE);
  auto& Current = Prop->History.GetMutableRecords()[Index];
  switch (Case) {
   case 0: F.Room->InvalidateWholePreparation(Id); break;
   case 1: Prop->History.Initialize(Id); break;
   case 2: Current.SnapshotTransform.AddToTranslation(FVector(1.e-7,0,0)); break;
   case 3: ++Current.ContentRevision; break;
   case 4: ++Prop->PolicyRevision; break;
   case 5: Prop->Actual->Destroy(); break;
   case 6: F.Room->InvalidateWholePreparation(); break;
   case 7: Current.bCaptureRevisionValid=false; break;
  }
  TBitArray<> A,B;
  TestFalse(FString::Printf(TEXT("stale case %d cannot be consumed"),Case),F.Room->TakeWholePreparation(*Prop,A,B));
  TestFalse(TEXT("late duplicate cannot revive"),F.Room->TakeWholePreparation(*Prop,A,B));
  TestTrue(TEXT("stale staging released"),F.Room->GetWholePreparationTelemetry().Contains(TEXT("\"pending\":0")));
 }

 {
  FRoom F;
  F.Room->ResetTrackedRevealPolicyForLab(Id,Reveal::WholeObjectAfterSpan,100,History::StationaryOnly);
  for(uint64 Frame=1;Frame<=30;++Frame) { F.Room->WholePreparationFrameForTesting=Frame; F.Step(); }
  const auto* Original=F.Room->Tracked.Find(Id);
  const int32 AuthorityCount=F.Room->Tracked.Num();
  for(int32 I=0;I<12;++I) {
   auto Clone=*Original;
   Clone.StableId=FName(*FString::Printf(TEXT("Preparation.Admission.%d"),I));
   F.Room->RequestWholePreparation(Clone);
  }
  TestTrue(TEXT("ninth packet cannot exceed bounded queue"),F.Room->GetWholePreparationTelemetry().Contains(TEXT("\"pending\":8,")));
  TestEqual(TEXT("private admission grants no identity"),F.Room->Tracked.Num(),AuthorityCount);
  F.Room->ResetMemory();
  TestTrue(TEXT("queue reset releases masks and descriptors"),F.Room->GetWholePreparationTelemetry().Contains(TEXT("\"bytes\":0,")));
 }
 // Each mask has its own original wrapper oracle, including reflected thin geometry.
 {
  FRoom F;
  for(const double Scale : {1.0,-1.0}) for(const int32 Quota : {1,127,128,129}) {
   FDarkwellCurrentLiveGrid Grid;
   auto& Part=Grid.Parts.AddDefaulted_GetRef();
   Part.Geometry.LocalBounds=FBox(FVector(-17,-.01,0),FVector(17,.01,23));
   Part.Pose=FTransform(FRotator(0,37,0),FVector(0,0,0),FVector(Scale,1,1));
   const FBox2D Bounds(FVector2D(-25,-25),FVector2D(25,25));
   const FIntPoint Size(17,9);
   TArray<ADarkwellObjectMemoryScene::FPrimitiveGeometrySnapshot> Geometry;
   auto& CapturePart=Geometry.AddDefaulted_GetRef();
   CapturePart.LocalBounds=Part.Geometry.LocalBounds; CapturePart.WorldTransform=Part.Pose; CapturePart.CachePlanarProjection();
   TBitArray<> ExpectedWhole;
   Grid.BuildFullGeometryMask(Bounds,Size,ExpectedWhole);
   const auto ExpectedFootprint=F.Room->BuildCaptureGeometryFootprint(Bounds,Size,Geometry,false);
   Darkwell::HistoryPreparation::FMaskJob Job;
   const Darkwell::HistoryPreparation::FTicket Ticket{1,1,1,1,1};
   Job.Initialize(Ticket,Size.X*Size.Y);
   while(!Job.IsReady()) Job.Step(1,Quota,[&](bool Whole,int32 Index) {
    if(!Whole) return F.Room->PrepareFootprintCell(Bounds,Size,Index,Geometry);
    const FVector2D Step=Bounds.GetSize()/FVector2D(Size);
    const FVector2D Min=Bounds.Min+Step*FVector2D(Index%Size.X,Index/Size.X);
    return FDarkwellCurrentLiveGrid::IntersectsWholeCell(Part.Geometry.LocalBounds,Part.Pose,FBox2D(Min,Min+Step));
   });
   TBitArray<> A,B; Job.Take(Ticket,A,B);
   TestTrue(TEXT("thin rotated reflected Whole per-bit oracle"),A==ExpectedWhole);
   TestTrue(TEXT("thin rotated reflected capture per-bit oracle"),B==ExpectedFootprint);
  }
 }
 // Incomplete work must finish through the existing seal, without a wait frame.
 {
  FRoom F;
  F.Room->ResetTrackedRevealPolicyForLab(Id,Reveal::WholeObjectAfterSpan,100,History::StationaryOnly);
  F.Room->WholePreparationWorkForTesting=1;
  for(uint64 Frame=1;Frame<=3;++Frame) { F.Room->WholePreparationFrameForTesting=Frame; F.Step(); }
  F.Face(-90); F.Room->WholePreparationFrameForTesting=4; F.Step();
  TBitArray<> A,B;
  TestTrue(TEXT("Preparing falls back in exit call"),F.Room->GetNewestCaptureMasksForTesting(Id,A,B));
  TestTrue(TEXT("Preparing capture parity"),A==OracleCapture && B==OracleFrozen);
  TestTrue(TEXT("fallback clears pending"),F.Room->GetWholePreparationTelemetry().Contains(TEXT("\"pending\":0")));
 }

 return true;
}

#endif
