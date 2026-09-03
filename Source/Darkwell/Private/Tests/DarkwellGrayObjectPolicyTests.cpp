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
  TEXT("ResetClearsOnlyTarget"),TEXT("PlayStopResourceLifetime"),TEXT("CanonicalRasterMatchesOriginalSamples"),TEXT("ConfirmedWholeStopsSpanAndDenseObservationWork"),TEXT("CachedDiagnosticsMatchForcedDiagnostics"),TEXT("FramePhysicalCacheMatchesGeometryOracle")})
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
   { FRoom Life; Life.Face(90); Life.Step(15); Life.Face(-90); Life.Step(15);
     TestTrue(TEXT("Positive historical resource allocation"),Life.Room->GetHistoricalPresentationResourceCountForTesting(Id)>0);
     Owned.Add(Life.World); Owned.Add(Life.Room); Owned.Add(Life.Room->GetObjectPolicyForTesting(Id)); }
   CollectGarbage(RF_NoFlags);
   for(auto P:Owned) TestFalse(TEXT("Destroyed world, room and policy are reclaimed"),P.IsValid());
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
 if(Case==TEXT("CachedDiagnosticsMatchForcedDiagnostics"))
 {
  F.Room->ResetTrackedRevealPolicyForLab(Id,Reveal::SpatialPartial,100,History::Always);
  F.Face(90); F.Step(30);
  TestTrue(TEXT("Multiple independent history records"),F.Room->ConfigureHistoricalEpochCountForTesting(Id,2));
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
