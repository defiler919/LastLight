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
#include "TextureResource.h"
#include "ConvexVolume.h"
#include "RHICommandList.h"
#include "RenderingThread.h"
#include "Math/Float16Color.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeExit.h"
#include "VisionPresentation/DarkwellMemoryRegionSubsystem.h"
#include "VisionPresentation/DarkwellMemoryRegionSamples.h"
#include "SightWeaveWorldSubsystem.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
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
      Mix(V->Render.Cap.IsValid() && V->Render.Cap->IsVisible());
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
    auto* V=Prop.Visuals.Find(R.Epoch); if(!V || !V->Render.Cap.IsValid()) continue;
    const auto FineBefore=R.FineHistory.GetSamples();
    TArray<FDarkwellHistoryGridV2::FSample> SavedSamples; SavedSamples.Append(FineBefore.GetData(),FineBefore.Num());
    const auto Suppression=V->SuppressedByCurrentEvidence;
    const auto Capture=R.LastLegalCaptureMask;
    auto Hash=[&]()
    {
     uint64 H=1469598103934665603ull; auto Mix=[&](uint64 X){H=(H^X)*1099511628211ull;};
     Mix(V->CapSignature); Mix(V->CapTriangles); Mix(V->CapExpected); Mix(V->CapGenerated);
     Mix(V->CapClipped); Mix(V->MissingHistoricalCuts); Mix(V->Render.Cap->IsVisible());
     for(const auto& Q:V->CapQuads) { Mix(Q.PrimitiveIndex); for(const FVector P:{Q.A,Q.B,Q.C,Q.D}) Mix(GetTypeHash(P)); }
     for(const auto P:V->CapSamplePoints) Mix(GetTypeHash(P));
     V->Render.Cap->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Mesh)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellA1Residency,
 "Darkwell.ObjectMemory.A1ConservativeDemand",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellA1Residency::RunTest(const FString&)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 TArray<FString> Oracle;
 TArray<TWeakObjectPtr<UObject>> Objects;
 for(bool Enabled:{false,true})
 {
  {
   FRoom F;F.World->AddToRoot();ON_SCOPE_EXIT{F.World->RemoveFromRoot();};
   F.Face(-90);F.Step(2);
   auto& S=*F.Room;
   if(!TestTrue(TEXT("Named old-history fixture"),S.ConfigureOldHistoryDemandForTesting()))return false;
   int32 Whole=0,Partial=0;
   for(const auto& Pair:S.Tracked)if(Pair.Key.ToString().StartsWith(TEXT("Lab.A1.Old.")))
    for(const auto& R:Pair.Value.History.GetRecords()) R.bConfirmedWholeCapture?++Whole:++Partial;
   TestEqual(TEXT("Actual confirmed Whole fixtures"),Whole,32);TestEqual(TEXT("Partial fixtures"),Partial,32);
   auto Apply=[&](double Now,float Yaw,FVector Camera,bool Valid=true)
   {
    FConvexVolume Volume;
    const FQuat Rotation=FRotator(0,Yaw,0).Quaternion();
    for(const FVector N:{FVector(-1,1,0),FVector(-1,-1,0),FVector(-1,0,1),FVector(-1,0,-1)})
     Volume.Planes.Add(FPlane(Camera,Rotation.RotateVector(N.GetSafeNormal())));
    Volume.Init();
    const auto Before=S.GetOldHistoryEvidenceHashForTesting();
    S.ApplyPresentationDemand(Valid?&Volume:nullptr,Camera,Now,Enabled);
    TestEqual(TEXT("Demand changes no authority/occupancy/ownership"),S.GetOldHistoryEvidenceHashForTesting(),Before);
    TestEqual(TEXT("Every needed record is materialized in this call"),S.Residency.Missing,uint64(0));
    // B0 must flush registrations before returning, never on a later frame.
    for(const auto& Pair:S.Tracked) for(const auto& Entry:Pair.Value.Visuals)
    {
     const auto& V=Entry.Value;
     if(auto* Proxy=V.Render.Proxy.Get())
     {
      TInlineComponentArray<UStaticMeshComponent*> Meshes(Proxy);
      for(auto* Mesh:Meshes) TestTrue(TEXT("Same-call mesh registration"),Mesh->IsRegistered());
     }
     if(auto* Cap=V.Render.Cap.Get()) TestTrue(TEXT("Same-call cap registration"),Cap->IsRegistered());
    }
   };
   Apply(1,0,FVector::ZeroVector);
   auto Count=[&](){int32 K=0;for(const auto& Pair:S.Tracked)if(Pair.Key.ToString().StartsWith(TEXT("Lab.A1.Old.")))
    for(const auto& V:Pair.Value.Visuals)K+=V.Value.Render.Proxy.IsValid();return K;};
   TestEqual(TEXT("Recent captures pinned regardless of camera"),Count(),64);
   auto& Offline=S.Tracked.FindChecked(TEXT("Lab.A1.Old.2.0"));
   const auto EvictedTexture=Offline.Visuals.FindChecked(1).Render.Texture;
   Apply(6,0,FVector::ZeroVector);
   if(Enabled)
   {
    CollectGarbage(RF_NoFlags,true);
    TestFalse(TEXT("Automatic eviction texture is actually reclaimed by GC"),EvictedTexture.IsValid());
    TestNotNull(TEXT("CPU record survives automatic eviction and GC"),Offline.History.FindRecord(1));
   }
   TestEqual(TEXT("Old demand K is small, N retained"),Count(),Enabled?16:64);
   const uint64 BeforeBoundary=S.Residency.Evictions;
   for(int32 I=0;I<12;++I)Apply(6.1+I*.02,I%2?47.f:43.f,FVector(0,I*5,0));
   TestEqual(TEXT("Expanded retain boundary prevents thrash"),S.Residency.Evictions,BeforeBoundary);
   // Slow translation, 180 turn, teleport and a whole group simultaneously reenter.
   for(int32 Phase=0;Phase<6;++Phase)
   {
    F.Step();
    Apply(8+Phase*2,Phase%2?180:0,Phase==3?FVector(0,11600,100):FVector(Phase*50,0,100));
    const auto Hash=S.GetOldHistoryEvidenceHashForTesting();
    if(!Enabled)Oracle.Add(Hash);else TestEqual(TEXT("Same input CPU oracle"),Hash,Oracle[Phase]);
   }
   if(Enabled)TestTrue(TEXT("Multiple reentries really rebuilt resources"),S.Residency.Rebuilds>=16);
   Apply(30,0,FVector::ZeroVector,false);
   TestEqual(TEXT("Invalid camera is fail-open all resident"),Count(),64);
   Apply(32,0,FVector::ZeroVector);
   if(Enabled)
   {
    const auto Capture=*Offline.History.FindRecord(1);
    Offline.Actual->Destroy();
    auto* Replacement=F.World->SpawnActor<AActor>();
    auto* Mesh=NewObject<UStaticMeshComponent>(Replacement);Replacement->SetRootComponent(Mesh);Replacement->AddInstanceComponent(Mesh);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
    Mesh->SetMobility(EComponentMobility::Movable);Mesh->RegisterComponent();Replacement->SetActorLocation(FVector(30000,30000,0));
    auto* Memory=NewObject<UDarkwellRememberablePropComponent>(Replacement);Memory->bUseSpatialMemory=true;Memory->ConfigureStableId(Offline.StableId);Memory->AddMemoryPrimitive(Mesh);
    Replacement->AddInstanceComponent(Memory);Memory->RegisterComponent();
    auto* Policy=NewObject<USightWeaveObjectPolicyComponent>(Replacement);Replacement->AddInstanceComponent(Policy);Policy->RegisterComponent();Replacement->DispatchBeginPlay();
    TestTrue(TEXT("Source replacement while automatically nonresident"),S.RegisterRememberable(Memory,Policy));
    CollectGarbage(RF_NoFlags,true);
    Apply(33,90,FVector::ZeroVector);
    TestTrue(TEXT("Automatic rebuild after replacement preserves captured pose"),Offline.Visuals.FindChecked(1).Render.Proxy.IsValid()
     && Offline.Visuals.FindChecked(1).Render.Proxy->GetActorTransform().Equals(Capture.SnapshotTransform));
   }
   Objects.Append(S.GetOwnedPresentationObjectsForTesting());
   // Returning mode 0 restores managed resources; A0 explicit releases are independent.
   S.ApplyPresentationDemand(nullptr,FVector::ZeroVector,33,false);
   TestEqual(TEXT("Oracle mode restores all histories synchronously"),Count(),64);
   Objects.Append(S.GetOwnedPresentationObjectsForTesting());
   S.ResetMemory();
   S.ApplyPresentationDemand(nullptr,FVector::ZeroVector,34,true);
   TestEqual(TEXT("Reset never reconstructs old records"),S.GetTotalSpatialRecordCount(),0);
  }
  CollectGarbage(RF_NoFlags,true);
  for(const auto& Object:Objects)TestFalse(TEXT("World teardown/GC releases managed resources"),Object.IsValid());
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellPresentationResidency,
 "Darkwell.ObjectMemory.PresentationResidency",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellPresentationResidency::RunTest(const FString&)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 using FTicket=ADarkwellObjectMemoryScene::FPresentationTicket;
 TArray<TWeakObjectPtr<UObject>> Released;
 FTicket PreviousWorldTicket;
 int32 OfflineFrames=0, EvidenceChanges=0, CapChecks=0, GpuReadbacks=0;
 for(const Reveal Mode:{Reveal::SpatialPartial,Reveal::WholeObjectAfterSpan})
 {
  TArray<uint64> Reference;
  for(const bool Evict:{false,true})
  {
   {
    FRoom F;
    F.World->AddToRoot();
    ON_SCOPE_EXIT { F.World->RemoveFromRoot(); };
    auto& Scene=*F.Room;
    TestFalse(TEXT("A previous world's ticket cannot address a recreated Scene"),Scene.RebuildHistoricalPresentationForTesting(PreviousWorldTicket));
    Scene.ResetTrackedRevealPolicyForLab(Id,Mode,100,History::StationaryOnly);
    F.Face(Mode==Reveal::SpatialPartial?146:90); F.Step(25);
    auto& Prop=Scene.Tracked.FindChecked(Id);
    FTicket Ticket, SupersededTicket;
    const uint32 Epoch=Prop.History.GetRecords()[0].Epoch;
    TestFalse(TEXT("Current cannot be explicitly evicted"),Scene.ReleaseHistoricalPresentationForTesting(Id,Epoch,Ticket));
    F.Face(-90); F.Step();
    auto* R=Prop.History.FindRecord(Epoch);
    if(!TestNotNull(TEXT("Legal first exit creates existing history"),R)) return false;
    TestTrue(TEXT("First exit publishes in this update, zero extra frames"),!R->bCurrentObservedLocation
     && Prop.Visuals.FindChecked(Epoch).Render.Proxy.IsValid() && !Prop.Visuals.FindChecked(Epoch).Render.Proxy->IsHidden());
    F.Step(2);
    auto Hash=[&]()
    {
     uint64 H=1469598103934665603ull;
     auto Mix=[&](uint64 V){H=(H^V)*1099511628211ull;};
     auto Bits=[&](const TBitArray<>& B){Mix(B.Num());for(int32 I=0;I<B.Num();++I)Mix(B[I]);};
     auto Floats=[&](const TArray<float>& A){Mix(A.Num());for(float V:A)Mix(GetTypeHash(V));};
     Mix(Prop.History.GetRecords().Num());
     for(const auto& Record:Prop.History.GetRecords())
     {
      Mix(Record.Epoch); Mix(Record.bCurrentObservedLocation); Mix(Record.FineHistory.EvidenceHash());
      Mix(Record.bConfirmedWholeCapture); Mix(Record.bCaptureRevisionValid);
      Bits(Record.LastLegalCaptureMask); Bits(Record.GeometryFootprint);
      Mix(GetTypeHash(Record.SnapshotTransform.ToString()));
      Mix(GetTypeHash(Record.Tint)); Mix(GetTypeHash(Record.UVScale));
      for(const auto& P:Record.Primitives) {Mix(GetTypeHash(P.Mesh.ToSoftObjectPath()));Mix(GetTypeHash(P.RelativeTransform.ToString()));}
      for(const auto& C:Record.SpatialMemory.GetCells())
       for(float V:{C.CurrentLegalCoverage,C.DiscoveredPresent,C.VerifiedEmpty,C.InitialRemembered,C.RemainingStale,
        C.AppearanceBlend,C.LiveBlend,C.StaleOpacity,C.ExitAge,C.EmptyDwell}) Mix(GetTypeHash(V));
      const auto* V=Prop.Visuals.Find(Record.Epoch); Mix(V!=nullptr); if(!V)continue;
      Bits(V->SuppressedByCurrentEvidence); Bits(V->TransientCurrentSuppression);
      Bits(V->CachedFineOccupied); Bits(V->CachedCoarseOccupied);
      Floats(V->CachedCoarseCoverage); Floats(V->CachedCoarseEvidence); Floats(V->CachedFineCoverage);
      Mix(V->ProcessedGeometryRevision); Mix(V->ProcessedOwnershipRevision); Mix(V->ProcessedOwnershipMaximumEpoch);
      Mix(V->bPresentationRetired); Mix(V->CapTriangles); Mix(V->CapExpected); Mix(V->CapGenerated); Mix(V->CapClipped); Mix(V->MissingHistoricalCuts);
      Mix(V->SubmittedPresentation.Num()); for(const auto& P:V->SubmittedPresentation)Mix(GetTypeHash(P));
      for(const auto& Q:V->CapQuads) for(const auto& P:{Q.A,Q.B,Q.C,Q.D})Mix(GetTypeHash(P));
      for(const auto& G:V->PartGeometry) {Mix(GetTypeHash(G.WorldTransform.ToString()));Mix(GetTypeHash(G.LocalBounds.Min));Mix(GetTypeHash(G.LocalBounds.Max));}
     }
     return H;
    };
    const uint64 BeforeRelease=Hash();
    if(Evict)
    {
     auto& V=Prop.Visuals.FindChecked(Epoch);
     Released.Add(V.Render.Proxy); Released.Add(V.Render.Texture); if(V.Render.Cap.IsValid())Released.Add(V.Render.Cap);
     for(const auto& M:V.Render.Materials)Released.Add(M);
     TestTrue(TEXT("Explicit single sealed-record release"),Scene.ReleaseHistoricalPresentationForTesting(Id,Epoch,SupersededTicket));
     TestEqual(TEXT("Release changes no CPU state"),Hash(),BeforeRelease);
     TestTrue(TEXT("Repeated release is safe"),Scene.ReleaseHistoricalPresentationForTesting(Id,Epoch,Ticket));
     TestFalse(TEXT("An older request cannot publish"),Scene.RebuildHistoricalPresentationForTesting(SupersededTicket));
     CollectGarbage(RF_NoFlags,true);
     for(const auto& Object:Released)TestFalse(TEXT("Released render objects reclaimed while CPU history lives"),Object.IsValid());
    }
    uint64 InitialEvidence=Prop.History.FindRecord(Epoch)->FineHistory.EvidenceHash();
    // Hidden motion changes occupancy; renewed legal viewing supplies real empty
    // evidence and new observed ownership. No synthetic FineHistory writes.
    auto Pose=Scene.GetTrackedTransform(Id); Pose.AddToTranslation(FVector(65,0,0));
    Pose.SetRotation(FRotator(0,23,0).Quaternion()); Scene.SetTrackedTransformForTesting(Id,Pose);
    for(int32 Frame=0;Frame<12;++Frame)
    {
     F.Face(Frame<2?-90:146); F.Step();
     const auto* Record=Prop.History.FindRecord(Epoch);
     auto* V=Prop.Visuals.Find(Epoch);
     if(Evict && Frame<6 && Record && V && !V->bPresentationRetired)
     {
      TestFalse(TEXT("UpdateTracked cannot automatically rematerialize an evicted old record"),V->Render.Proxy.IsValid()||V->Render.Texture.IsValid()||V->Render.Cap.IsValid()||!V->Render.Materials.IsEmpty());
      ++OfflineFrames;
      if(Record->FineHistory.EvidenceHash()!=InitialEvidence)++EvidenceChanges;
     }
     const uint64 H=Hash();
     if(!Evict)Reference.Add(H);else if(!TestEqual(*FString::Printf(TEXT("CPU/ownership/occupancy/pixels/cap oracle frame %d"),Frame),H,Reference[Frame]))return false;
     if(Frame==5 && Evict)
     {
      if(!TestTrue(TEXT("Rebuild surviving old history from latest CPU state"),Scene.RebuildHistoricalPresentationForTesting(Ticket)))return false;
      TestEqual(TEXT("Rebuild does not change CPU/knowledge result"),Hash(),H);
      V=Prop.Visuals.Find(Epoch); Record=Prop.History.FindRecord(Epoch);
      TestTrue(TEXT("Rebuild publishes captured pose immediately"),V->Render.Proxy.IsValid() && !V->Render.Proxy->IsHidden() && V->Render.Proxy->GetActorTransform().Equals(Record->SnapshotTransform));
      TestEqual(TEXT("New texture receives current pixels"),V->Render.UploadedTextureSignature,V->TextureSignature);
      if(V->Render.Texture->GetResource())
      {
       TArray<FFloat16Color> Readback;
       const FTextureRHIRef Texture=V->Render.Texture->GetResource()->TextureRHI;
       const FIntPoint Size=Record->FineHistory.GetSize();
       ENQUEUE_RENDER_COMMAND(A0ReadCurrentTexture)([Texture,Size,&Readback](FRHICommandListImmediate& RHICmdList)
       { RHICmdList.ReadSurfaceFloatData(Texture,FIntRect(0,0,Size.X,Size.Y),Readback,ECubeFace::CubeFace_PosX,0,0); });
       FlushRenderingCommands();
       TestEqual(TEXT("D3D12 readback dimensions retained"),Readback.Num(),V->SubmittedPresentation.Num());
       int32 Mismatches=0;
       for(int32 I=0;I<Readback.Num() && I<V->SubmittedPresentation.Num();++I)
       {
        const FFloat16Color Expected(V->SubmittedPresentation[I]);
        if(FMemory::Memcmp(&Readback[I],&Expected,sizeof(Expected))!=0)++Mismatches;
       }
       TestEqual(TEXT("D3D12 texture contains exact latest Float16 pixels"),Mismatches,0);
       ++GpuReadbacks;
      }
      for(const auto& M:V->Render.Materials)
       TestTrue(TEXT("New MID binds latest texture and is ready"),M.IsValid() && M->K2_GetTextureParameterValue(TEXT("SpatialStateTexture"))==V->Render.Texture.Get() && M->K2_GetScalarParameterValue(TEXT("SpatialReady"))==1.f);
      if(V->Render.Cap.IsValid())V->Render.Cap->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Mesh)
      {
       TestEqual(TEXT("Rebuilt cap contains current CPU topology"),Mesh.TriangleCount(),V->CapTriangles);
       int32 Vertex=0;for(const auto& Q:V->CapQuads)for(const auto& P:{Q.A,Q.B,Q.C,Q.D})TestTrue(TEXT("Rebuilt cap vertices equal current CPU quads"),Mesh.GetVertex(Vertex++).Equals(P-Scene.GetActorLocation()));
       ++CapChecks;
      });
      const int32 Resources=Scene.GetHistoricalPresentationResourceCountForTesting(Id);
      TestTrue(TEXT("Successful rebuild is idempotent"),Scene.RebuildHistoricalPresentationForTesting(Ticket));
      TestEqual(TEXT("No duplicate registration/resources"),Scene.GetHistoricalPresentationResourceCountForTesting(Id),Resources);
     }
    }
    // Source replacement revokes a request, but must retain prior captured content.
    if(Evict)
    {
     if(!TestTrue(TEXT("Old record survives for replacement contract"),Scene.ReleaseHistoricalPresentationForTesting(Id,Epoch,Ticket)))return false;
     const auto OldCapture=*Prop.History.FindRecord(Epoch);
     Prop.Actual->Destroy();
     auto* Replacement=F.World->SpawnActor<AActor>();
     auto* Mesh=NewObject<UStaticMeshComponent>(Replacement); Replacement->SetRootComponent(Mesh); Replacement->AddInstanceComponent(Mesh);
     Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
     Mesh->SetMobility(EComponentMobility::Movable); Mesh->RegisterComponent(); Replacement->SetActorLocation(FVector(5000,5000,0));
     auto* Memory=NewObject<UDarkwellRememberablePropComponent>(Replacement); Memory->bUseSpatialMemory=true; Memory->ConfigureStableId(Id); Memory->AddMemoryPrimitive(Mesh);
     Replacement->AddInstanceComponent(Memory); Memory->RegisterComponent();
     auto* Policy=NewObject<USightWeaveObjectPolicyComponent>(Replacement); Replacement->AddInstanceComponent(Policy); Policy->RegisterComponent(); Replacement->DispatchBeginPlay();
     TestTrue(TEXT("SourceReplace uses production registration"),Scene.RegisterRememberable(Memory,Policy));
     TestFalse(TEXT("Pre-replacement request is stale"),Scene.RebuildHistoricalPresentationForTesting(Ticket));
     CollectGarbage(RF_NoFlags,true);
     TestTrue(TEXT("Fresh request can address preserved old knowledge"),Scene.ReleaseHistoricalPresentationForTesting(Id,Epoch,Ticket));
     auto& MissingMesh=Prop.History.FindRecord(Epoch)->Primitives[0].Mesh;
     const auto SavedMesh=MissingMesh; MissingMesh.Reset();
     AddExpectedError(TEXT("A0 missing captured mesh"),EAutomationExpectedErrorFlags::Contains,1);
     TestFalse(TEXT("Missing captured asset fails explicitly"),Scene.RebuildHistoricalPresentationForTesting(Ticket));
     TestFalse(TEXT("Failed preparation publishes no partial proxy"),Prop.Visuals.FindChecked(Epoch).Render.Proxy.IsValid());
     MissingMesh=SavedMesh;
     TestTrue(TEXT("Rebuild after source GC uses captured asset paths"),Scene.RebuildHistoricalPresentationForTesting(Ticket));
     const auto* Record=Prop.History.FindRecord(Epoch);
     TestTrue(TEXT("Source replacement retains original capture and pose"),Record && Record->ContentRevision==OldCapture.ContentRevision && Record->SnapshotTransform.Equals(OldCapture.SnapshotTransform));
     TestTrue(TEXT("Release before legal terminal evidence"),Scene.ReleaseHistoricalPresentationForTesting(Id,Epoch,Ticket));
     F.Face(90); F.Step(35);
     TestFalse(TEXT("Legal empty evidence retires/removes the offline old record"),Prop.History.FindRecord(Epoch)!=nullptr);
     TestFalse(TEXT("Terminal record cannot be resurrected by old request"),Scene.RebuildHistoricalPresentationForTesting(Ticket));
     // Reset and History.Initialize may reuse epoch numbers, never tickets.
     Scene.ResetRoom(F.Player);
     TestFalse(TEXT("Reset rejects old ticket"),Scene.RebuildHistoricalPresentationForTesting(Ticket));
     Scene.ResetTrackedRevealPolicyForLab(Id,Mode,100,History::StationaryOnly);
     F.Face(90);F.Step(25);F.Face(-90);F.Step(3);
     const uint32 ResumeEpoch=Scene.Tracked.FindChecked(Id).History.GetRecords()[0].Epoch;
     FTicket Resume;
     TestTrue(TEXT("Explicitly release uncontradicted stationary history"),Scene.ReleaseHistoricalPresentationForTesting(Id,ResumeEpoch,Resume));
     F.Face(90);F.Step(25);
     TestFalse(TEXT("Legal resume revokes the old resource request"),Scene.RebuildHistoricalPresentationForTesting(Resume));
     F.Face(-90);F.Step();
     TestTrue(TEXT("Resumed capture seals with no extra blank frame"),Scene.GetVisibleHistoricalProxyCountForTesting(Id)>0);
     Scene.ConfigureHistoricalEpochCountForTesting(Id,2);
     FTicket Fresh;
     TestTrue(TEXT("Reused epoch exists after History.Initialize"),Scene.ReleaseHistoricalPresentationForTesting(Id,1,Fresh));
     Scene.ConfigureHistoricalEpochCountForTesting(Id,2);
     TestFalse(TEXT("History lifetime rejects same-epoch old work"),Scene.RebuildHistoricalPresentationForTesting(Fresh));
     PreviousWorldTicket=Fresh;
    }
    Released.Append(Scene.GetOwnedPresentationObjectsForTesting());
   }
   CollectGarbage(RF_NoFlags,true);
   for(const auto& Object:Released)TestFalse(TEXT("World teardown releases concrete resources"),Object.IsValid());
  }
 }
 TestTrue(TEXT("Positive offline update coverage"),OfflineFrames>=6);
 TestTrue(TEXT("Offline authority actually changed, not just sleeping parity"),EvidenceChanges>0);
 TestTrue(TEXT("Partial rebuilt cap mesh checked"),CapChecks>0);
 AddInfo(FString::Printf(TEXT("A0 offline frames=%d changed-evidence frames=%d cap meshes=%d GPU readbacks=%d; first Whole exit extra frames=0"),OfflineFrames,EvidenceChanges,CapChecks,GpuReadbacks));
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
      if(!V || !V->Render.Proxy.IsValid()) continue;
      TInlineComponentArray<UStaticMeshComponent*> Meshes(V->Render.Proxy.Get());
      TestEqual(TEXT("All captured primitive components retained"),Meshes.Num(),R.Primitives.Num());
      TestEqual(TEXT("MID ownership is per record, legacy per part"),V->Render.Materials.Num(),Legacy?Meshes.Num():1);
      for(const auto& M:V->Render.Materials)
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
       TestTrue(TEXT("Binding belongs to the record"),V->Render.Materials.Contains(M));
       TestEqual(TEXT("Prepared Current is transparent, sealed is ready"),M->K2_GetScalarParameterValue(TEXT("SpatialReady")),R.bCurrentObservedLocation?0.f:1.f);
       TestTrue(TEXT("Texture binding is record-local"),M->K2_GetTextureParameterValue(TEXT("SpatialStateTexture"))==V->Render.Texture.Get());
       TestTrue(TEXT("Captured bounds unchanged"),M->K2_GetVectorParameterValue(TEXT("SpatialMinInv"))==FLinearColor(Bounds.Min.X,Bounds.Min.Y,Inv.X,Inv.Y));
       TestTrue(TEXT("Captured tint unchanged"),M->K2_GetVectorParameterValue(TEXT("OriginalBaseColorTint"))==R.Tint);
       TestEqual(TEXT("Captured UV unchanged"),M->K2_GetScalarParameterValue(TEXT("OriginalUVScale")),R.UVScale);
       ++CheckedMeshes;
      }
      if(!R.bCurrentObservedLocation)
      {
       TestFalse(TEXT("First exit publishes the proxy in this call"),V->Render.Proxy->IsHidden());
       TestFalse(TEXT("First exit finishes prepared-resource handoff"),V->Render.bProxyPreparedForCapture);
       TestTrue(TEXT("History uses captured pose"),V->Render.Proxy->GetActorTransform().Equals(R.SnapshotTransform));
       TestTrue(TEXT("First exit submits complete pixels"),!V->SubmittedPresentation.IsEmpty());
      }
      if(R.bConfirmedWholeCapture) TestFalse(TEXT("Confirmed Whole has no cap"),V->Render.Cap.IsValid());
      Mix(V->Render.Texture->GetSizeX()); Mix(V->Render.Texture->GetSizeY());
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
     const FName PreviousCap=V.Render.Cap.IsValid()?V.Render.Cap->GetFName():NAME_None;
     Released.Append(F.Room->GetOwnedPresentationObjectsForTesting());
     const auto PreviousMaterials=V.Render.Materials;
     F.Room->DestroyVisual(V,false);
     for(const auto& M:PreviousMaterials) TestFalse(TEXT("Retired MID leaves owning array"),F.Room->OwnedMaterials.Contains(M.Get()));
     F.Room->EnsureRecordVisual(Prop,R); F.Room->UpdateRecordTexture(Prop,R); F.Room->UpdateRecordCap(Prop,R);
     if(!Legacy && PreviousCap!=NAME_None)
      TestTrue(TEXT("Cap reconstruction cannot overwrite pending destruction"),V.Render.Cap.IsValid() && V.Render.Cap->GetFName()!=PreviousCap);
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
 const auto InitialProxy=F.Room->Tracked.FindChecked(Id).Visuals.CreateConstIterator().Value().Render.Proxy;
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
 TestTrue(TEXT("Unchanged captured state reuses its concrete history proxy"),InitialProxy==F.Room->Tracked.FindChecked(Id).Visuals.CreateConstIterator().Value().Render.Proxy);
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
  TestTrue(TEXT("Memory proxy rebuilds without a live source"),Visual.Render.Proxy.IsValid());
  if(!Visual.Render.Proxy.IsValid()) return false;
  TArray<UStaticMeshComponent*> Parts; Visual.Render.Proxy->GetComponents(Parts);
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
 {
  FRoom F;
  F.Room->ResetTrackedRevealPolicyForLab(Id,Reveal::WholeObjectAfterSpan,100,History::StationaryOnly);
  for (uint64 Frame=1; Frame<=30; ++Frame) { F.Room->WholePreparationFrameForTesting=Frame; F.Step(); }
  F.Room->InvalidateWholePreparation();
  F.Room->WholePreparationFrameForTesting=100;
  F.Room->RuntimeFrame.TextureCreations=1;
  F.Room->AdvanceWholePreparation();
  TestTrue(TEXT("resource frame admits no packet"),F.Room->GetWholePreparationTelemetry().Contains(TEXT("\"pending\":0")));
  F.Room->RuntimeFrame = {};
  F.Room->AdvanceWholePreparation();
  TestTrue(TEXT("same-frame second update cannot bypass suppression"),F.Room->GetWholePreparationTelemetry().Contains(TEXT("\"pending\":0")));
  F.Room->InvalidateWholePreparation();
  F.Room->AdvanceWholePreparation();
  TestTrue(TEXT("invalidation does not clear busy-frame latch"),F.Room->GetWholePreparationTelemetry().Contains(TEXT("\"suppressed\":1")));
  F.Room->WholePreparationFrameForTesting=101;
  F.Room->AdvanceWholePreparation();
  TestTrue(TEXT("next idle frame admits exact current input"),F.Room->GetWholePreparationTelemetry().Contains(TEXT("\"pending\":1")));
  F.Room->WholePreparationFrameForTesting=102;
  F.Room->WholePreparationForegroundUsForTesting=1000;
  F.Room->AdvanceWholePreparation();
  TestTrue(TEXT("native-heavy frame performs zero geometry work"),F.Room->GetWholePreparationTelemetry().Contains(TEXT("\"frame_work\":0,")));
  F.Face(-90); F.Room->WholePreparationFrameForTesting=103; F.Step();
  TBitArray<> A,B;
  TestTrue(TEXT("suppressed preparation never delays legal seal"),F.Room->GetNewestCaptureMasksForTesting(Id,A,B));
  TestTrue(TEXT("suppressed fallback preserves both masks"),A==OracleCapture && B==OracleFrozen);
 }
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
   F.Room->RequestWholePreparation(Clone, true);
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


IMPLEMENT_COMPLEX_AUTOMATION_TEST(FDarkwellUnknownRegionContract,"Darkwell.UnknownRegion",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FDarkwellUnknownRegionContract::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{ for(const TCHAR* N:{TEXT("Whole"),TEXT("SpatialPartial")}) {Names.Add(N);Commands.Add(N);} }
bool FDarkwellUnknownRegionContract::RunTest(const FString& ModeName)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 const bool Partial=ModeName==TEXT("SpatialPartial");
 for(const TCHAR* Sequence:{TEXT("A"),TEXT("B"),TEXT("C")})
 {
  FRoom F; F.World->AddToRoot(); ON_SCOPE_EXIT { F.World->RemoveFromRoot(); };
  auto& Scene=*F.Room;
  Scene.ResetTrackedRevealPolicyForLab(Id,Partial?Reveal::SpatialPartial:Reveal::WholeObjectAfterSpan,100,History::StationaryOnly);
  auto* Fog=F.World->GetSubsystem<UDarkwellFogVisualSubsystem>();
  auto* Region=F.World->GetSubsystem<UDarkwellMemoryRegionSubsystem>();
  auto Observe=[&](){F.Face(Partial?146:90);F.Step(25);};
  auto Leave=[&](){F.Face(-90);F.Step(15);};
  F.Face(146); F.Step(25); // Positive gray control at another, outside identity.
  Observe(); Leave();
  auto& P=Scene.Tracked.FindChecked(Id);
  if(!TestTrue(TEXT("Initial real legal observation produces gray"),!P.History.GetRecords().IsEmpty())) return false;
  const auto Old=P.History.GetRecords()[0];
  const uint64 OldEvidence=Old.FineHistory.EvidenceHash();
  auto* Runtime=F.World->GetSubsystem<USightWeaveWorldSubsystem>();
  TMap<FName,uint64> Outside;
  auto OutsideHash=[&](const auto& Other){uint64 H=0;for(const auto& R:Other.History.GetRecords()) H=HashCombineFast(H,R.FineHistory.EvidenceHash());return H;};
  int32 OutsideKnown=0;
  for(const auto& Pair:Scene.Tracked) if(Pair.Key!=Id)
  {
   Outside.Add(Pair.Key,OutsideHash(Pair.Value));
   for(const auto& R:Pair.Value.History.GetRecords()) OutsideKnown+=R.FineHistory.HasResidualSurface()?1:0;
  }
  TestTrue(TEXT("Outside comparison contains real retained gray, not an empty control"),OutsideKnown>0);
  const FBox2D B=Old.SpatialMemory.GetBounds().ExpandBy(10);
  if(!Partial) TestFalse(TEXT("Straddling Whole object box is refused without side effects"),Region->ConfigureRegion(B.GetCenter(),B.Max));
  if(!TestTrue(TEXT("Configure complete deterministic room region"),Region->ConfigureRegion(B.Min,B.Max))) return false;
  TestTrue(TEXT("Same region is idempotent"),Region->ConfigureRegion(B.Min,B.Max));
  TestFalse(TEXT("Cannot silently replace the authority domain"),Region->ConfigureRegion(B.Min-FVector2D(1),B.Max));
  const FVector2D Center=B.GetCenter();
  TestEqual(TEXT("Initial CPU ground is remembered"),Region->QueryKnowledge(Center),Region->Remembered());
  TestTrue(TEXT("Partial starts with legal cut, Whole starts without cap"),
   Partial?Scene.GetVisibleHistoricalCapCountForTesting(Id)>0:Scene.GetVisibleHistoricalCapCountForTesting(Id)==0);
  // Fixed overhead scene view plus direct CPU-mirror readback, never input to authority.
  AActor* CaptureOwner=nullptr; USceneCaptureComponent2D* Capture=nullptr; UTextureRenderTarget2D* Target=nullptr;
  if(!GUsingNullRHI)
  {
   CaptureOwner=F.World->SpawnActor<AActor>();
   Capture=NewObject<USceneCaptureComponent2D>(CaptureOwner); CaptureOwner->AddInstanceComponent(Capture);
   Capture->RegisterComponent(); Capture->SetWorldLocation(FVector(Center.X,Center.Y,1100));
   Capture->SetWorldRotation(FRotator(-90,0,0)); Capture->ProjectionType=ECameraProjectionMode::Orthographic;
   if(Partial)
   {
    const FVector Eye(Center.X-500,Center.Y-600,750);
    Capture->SetWorldLocation(Eye); Capture->SetWorldRotation((FVector(Center,70)-Eye).Rotation());
   }
   Capture->OrthoWidth=440; Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false;
   Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;
   Capture->ShowFlags.SetTemporalAA(false); Capture->ShowFlags.SetMotionBlur(false);
   Target=NewObject<UTextureRenderTarget2D>(CaptureOwner); Target->InitCustomFormat(384,384,PF_B8G8R8A8,false);
   Target->UpdateResourceImmediate(); Capture->TextureTarget=Target;
  }
  auto Snapshot=[&](const TCHAR* Stage)
  {
   if(GUsingNullRHI) return;
   const auto Size=Region->GetSize(); TArray<FColor> Pixels;
   const FTextureRHIRef Texture=Region->GetPresentationTexture()->GetResource()->TextureRHI;
   ENQUEUE_RENDER_COMMAND(UnknownReadAuthorityMirror)([Texture,Size,&Pixels](FRHICommandListImmediate& RHICmdList)
    {RHICmdList.ReadSurfaceData(Texture,FIntRect(0,0,Size.X,Size.Y),Pixels,FReadSurfaceDataFlags(RCM_UNorm));});
   FlushRenderingCommands();
   int32 Mismatch=0; const FVector2D Step=B.GetSize()/FVector2D(Size);
   for(int32 Y=0;Y<Size.Y;++Y) for(int32 X=0;X<Size.X;++X)
   {
    const bool Known=Region->QueryKnowledge(B.Min+FVector2D(X+.5,Y+.5)*Step)==Region->Remembered();
    if(!Pixels.IsValidIndex(Y*Size.X+X) || (Pixels[Y*Size.X+X].R>127)!=Known) ++Mismatch;
   }
   TestEqual(TEXT("D3D12 texture exactly mirrors CPU knowledge; no renderer grants"),Mismatch,0);
   FString ReportPath;
   FParse::Value(FCommandLine::Get(),TEXT("ReportExportPath="),ReportPath);
   const FString Root=ReportPath.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("UnknownRegion"):FPaths::GetPath(ReportPath)/TEXT("Captures");
   const FString Dir=Root/ModeName/Sequence;
   IFileManager::Get().MakeDirectory(*Dir,true);
   auto Save=[&](const FString& Path,int32 W,int32 H,const TArray<FColor>& Data)
   {TArray<uint8> PNG;FImageUtils::CompressImageArray(W,H,Data,PNG);FFileHelper::SaveArrayToFile(PNG,*Path);};
   Save(Dir/(FString(Stage)+TEXT("_knowledge.png")),Size.X,Size.Y,Pixels);
   FAssetCompilingManager::Get().FinishAllCompilation();
#if WITH_EDITOR
   if(GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
#endif
   F.World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
   // The first capture admits scene-view shader/PSO work in a fresh automation
   // world. Resolve it, then read the same unchanged CPU state, not a fallback.
#if WITH_EDITOR
   if(GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
#endif
   F.World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
   TArray<FColor> View; Target->GameThread_GetRenderTargetResource()->ReadPixels(View);
   if(TestEqual(TEXT("D3D12 scene capture is complete"),View.Num(),384*384)) Save(Dir/(FString(Stage)+TEXT("_scene.png")),384,384,View);
  };
  Snapshot(TEXT("01_gray"));
  const bool Block=FCString::Strcmp(Sequence,TEXT("A"))!=0;
  const bool Clear=FCString::Strcmp(Sequence,TEXT("B"))!=0;
  if(Clear)
  {
   ADarkwellObjectMemoryScene::FPresentationTicket Ticket;
   TestTrue(TEXT("Get genuine old presentation ticket"),Scene.ReleaseHistoricalPresentationForTesting(Id,Old.Epoch,Ticket));
   TestTrue(TEXT("Authoritative clear succeeds"),Region->ClearMemory());
   TestFalse(TEXT("Clear destroys stored CPU ground bits"),Region->HasStoredMemory(Center));
   TestFalse(TEXT("Plugin CPU HardMemory is also destructively cleared"),Runtime->QueryHardMemoryAtLocation(FVector(Center,0)));
   TestNull(TEXT("Clear deletes the old fine/coarse/capture record"),P.History.FindRecord(Old.Epoch));
   TestFalse(TEXT("Old A0 ticket cannot resurrect a cleared record"),Scene.RebuildHistoricalPresentationForTesting(Ticket));
   TestEqual(TEXT("Clear removes gray resources"),Scene.GetVisibleHistoricalProxyCountForTesting(Id),0);
  }
  if(Block) TestTrue(TEXT("Enable write block"),Region->SetBlockMemoryWrites(true));
  for(const auto& Pair:Outside) TestEqual(TEXT("Region transaction leaves outside record evidence intact"),OutsideHash(Scene.Tracked.FindChecked(Pair.Key)),Pair.Value);
  TestEqual(TEXT("Half-open maximum boundary is outside and unchanged"),Region->QueryKnowledge(B.Max),Region->Remembered());
  TestEqual(TEXT("Without Live the region is authoritative Unknown"),Region->QueryKnowledge(Center),Region->Unknown());
  Snapshot(TEXT("02_unknown"));
  Observe();
  TestTrue(TEXT("Block does not disable legal Live"),Scene.IsCurrentSourceVisibleForTesting(Id));
  TestTrue(TEXT("Legal contact remains authoritative"),Scene.GetLastLegalCoverageRatioForTesting(Id)>0);
  TestEqual(TEXT("Capture eligibility follows write gate, not Live"),Scene.IsCaptureEligible(P),!Block);
  if(Block)
  {
   TestEqual(TEXT("Block never edits the stored ground memory"),Region->HasStoredMemory(Center),!Clear);
   if(Clear) TestFalse(TEXT("Blocked legal observation cannot write plugin HardMemory either"),Runtime->QueryHardMemoryAtLocation(FVector(Center,0)));
   int32 NewSealed=0;
   for(const auto& R:P.History.GetRecords()) if(!R.bCurrentObservedLocation && R.Epoch!=Old.Epoch) ++NewSealed;
   TestEqual(TEXT("Blocked observation creates no new sealed gray"),NewSealed,0);
   if(!Clear)
   {
    TestEqual(TEXT("Block preserves old fine evidence"),P.History.FindRecord(Old.Epoch)->FineHistory.EvidenceHash(),OldEvidence);
    ADarkwellObjectMemoryScene::FPresentationTicket T;
    TestTrue(TEXT("A0 release works on blocked resident history"),Scene.ReleaseHistoricalPresentationForTesting(Id,Old.Epoch,T));
    TestTrue(TEXT("A0 rebuild remains a resource operation"),Scene.RebuildHistoricalPresentationForTesting(T));
    TestEqual(TEXT("Rebuilding presentation cannot bypass CPU Block"),Scene.GetVisibleHistoricalProxyCountForTesting(Id),0);
   }
  }
  Snapshot(TEXT("03_live")); Leave();
  if(Block)
  {
   TestEqual(TEXT("After blocked Live exits there is no gray proxy"),Scene.GetVisibleHistoricalProxyCountForTesting(Id),0);
   TestEqual(TEXT("Blocked live slot is discarded"),P.History.GetCurrentIndex(),INDEX_NONE);
   Snapshot(TEXT("04_left_black"));
   TestTrue(TEXT("Unblock succeeds"),Region->SetBlockMemoryWrites(false));
   Snapshot(TEXT("05_unblocked"));
   TestEqual(TEXT("Unblock restores only pre-existing CPU ground knowledge"),Region->HasStoredMemory(Center),!Clear);
   TestEqual(TEXT("Old object record restoration matches destructive clear"),P.History.FindRecord(Old.Epoch)!=nullptr,!Clear);
   TestEqual(TEXT("Old gray visible only for Block-only"),Scene.GetVisibleHistoricalProxyCountForTesting(Id)>0,!Clear);
   if(!Clear) TestEqual(TEXT("Unblock preserves exact old fine evidence"),P.History.FindRecord(Old.Epoch)->FineHistory.EvidenceHash(),OldEvidence);
   else
   {
    F.Step(10);
    TestEqual(TEXT("Idle updates after unblock do not resurrect cleared gray"),P.History.GetRecords().Num(),0);
    TestFalse(TEXT("Idle outside-Live updates cannot grant ground memory"),Region->HasStoredMemory(Center));
    Observe(); Leave();
   }
  }
  TestTrue(TEXT("Fresh unblocked legal observation produces gray"),Scene.GetVisibleHistoricalProxyCountForTesting(Id)>0);
  TestTrue(TEXT("New legal observation rebuilds actual CPU ground samples"),Region->GetStoredSampleCount()>0);
  if(Clear) for(const auto& R:P.History.GetRecords()) TestTrue(TEXT("New knowledge has a new epoch; no old identity reuse"),R.Epoch>Old.Epoch);
  Snapshot(TEXT("06_final_gray"));
  // Commands issued after the normal scene update must still preserve Live.
  Observe();
  const auto RawBefore=Fog->QueryLiveCoverageAtWorldPoint(Center);
  TestTrue(TEXT("Activate Block during Live"),Region->SetBlockMemoryWrites(true));
  TestTrue(TEXT("Block publishes Live in same call"),Scene.IsCurrentSourceVisibleForTesting(Id));
  if(!Partial) TestTrue(TEXT("Block keeps Whole confirmation"),Scene.IsRevealConfirmedForTesting(Id));
  TestTrue(TEXT("Clear during blocked Live"),Region->ClearMemory());
  TestTrue(TEXT("Clear publishes Live in same call"),Scene.IsCurrentSourceVisibleForTesting(Id));
  if(!Partial) TestTrue(TEXT("Clear keeps Whole Live qualification"),Scene.IsRevealConfirmedForTesting(Id));
  const auto RawAfter=Fog->QueryLiveCoverageAtWorldPoint(Center);
  TestEqual(TEXT("Memory operations do not alter Live coverage"),RawAfter.Coverage,RawBefore.Coverage);
  TestEqual(TEXT("Memory operations do not alter Vision revision"),RawAfter.AuthorityRevision,RawBefore.AuthorityRevision);
  Snapshot(TEXT("07_clear_block_still_live"));
  Leave(); TestTrue(TEXT("Unblock after Live clear"),Region->SetBlockMemoryWrites(false)); F.Step(5);
  TestEqual(TEXT("Live clear then departure cannot revive old or blocked captures"),P.History.GetRecords().Num(),0);
  TestEqual(TEXT("Blocked observations never write retained ground"),Region->GetStoredSampleCount(),0);
  Snapshot(TEXT("08_no_resurrection"));
  AddInfo(FString::Printf(TEXT("UNKNOWN_REGION mode=%s sequence=%s passed_old_resurrection=0 revision=%llu"),*ModeName,Sequence,Region->GetAuthorityRevision()));
  if(CaptureOwner) CaptureOwner->Destroy();
 }
 return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellUnknownPartialCut,"Darkwell.UnknownPartial.SampleCut",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellUnknownPartialCut::RunTest(const FString&)
{
 using namespace Darkwell::GrayObjectPolicyTests;
 using namespace Darkwell::MemoryRegionSamples;
 FDarkwellHistoryGridV2 UnblockedReference;
 for(bool Block:{false,true})
 {
  FRoom F;F.World->AddToRoot();ON_SCOPE_EXIT {F.World->RemoveFromRoot();};
  F.Face(-90);F.Step(2);
  auto& Scene=*F.Room;Scene.ResetTrackedRevealPolicyForLab(Id,Reveal::SpatialPartial,100,History::StationaryOnly);
  auto& P=Scene.Tracked.FindChecked(Id);const auto OB=Scene.ActualBounds(*P.Actual);
  const FBox2D B(FVector2D(OB.GetCenter().X-19.83,OB.Min.Y-5),FVector2D(OB.GetCenter().X+20.17,OB.Max.Y+5));
  if(Block)
  {auto* Region=F.World->GetSubsystem<UDarkwellMemoryRegionSubsystem>();TestTrue(TEXT("Fresh straddle configure"),Region->ConfigureRegion(B.Min,B.Max));TestTrue(TEXT("Fresh straddle block"),Region->SetBlockMemoryWrites(true));}
  F.Face(90);F.Step(25);F.Face(-90);F.Step(15);
  if(!TestTrue(TEXT("Fresh observation seals Partial"),!P.History.GetRecords().IsEmpty())) return false;
  const auto& Grid=P.History.GetRecords().Last().FineHistory;
  if(!Block) UnblockedReference=Grid;
  else
  {
   TestEqual(TEXT("Same fine sampling resolution"),Grid.GetSize(),UnblockedReference.GetSize());
   int32 InsideLeak=0,LostFacts=0,LostKnownAA=0,ChangedUnknownAA=0;
   for(int32 I=0;I<Grid.GetSamples().Num();++I)
   {
    const auto& C=Grid.GetSamples()[I];const auto& R=UnblockedReference.GetSamples()[I];
    if(Contains(B,Darkwell::MemoryRegionSamples::Center(Grid.GetBounds(),Grid.GetSize(),I))) InsideLeak+=C.InitialRemembered>0;
    else
    {
     LostFacts+=C.InitialRemembered!=R.InitialRemembered;
     LostKnownAA+=R.InitialRemembered>0 && C.FrozenAAEnvelope!=R.FrozenAAEnvelope;
     ChangedUnknownAA+=R.InitialRemembered==0 && C.FrozenAAEnvelope!=R.FrozenAAEnvelope;
    }
   }
   TestEqual(TEXT("Block only removes inside fine samples, including mixed coarse boundary cells"),InsideLeak,0);
   AddInfo(FString::Printf(TEXT("OUTSIDE_FINE_REFERENCE facts=%d known_aa=%d unknown_unused_aa=%d"),LostFacts,LostKnownAA,ChangedUnknownAA));
   TestEqual(TEXT("Every newly observed outside fine fact matches no-Block reference"),LostFacts,0);
   TestEqual(TEXT("Every known outside AA envelope matches no-Block reference"),LostKnownAA,0);
  }
 }
 for(const TCHAR* Sequence:{TEXT("A"),TEXT("B"),TEXT("C")})
 {
  FRoom F; F.World->AddToRoot(); ON_SCOPE_EXIT {F.World->RemoveFromRoot();};
  auto& Scene=*F.Room;
  Scene.ResetTrackedRevealPolicyForLab(Id,Reveal::SpatialPartial,100,History::StationaryOnly);
  auto Observe=[&](){F.Face(90);F.Step(25);};
  auto Leave=[&](){F.Face(-90);F.Step(15);};
  Observe();Leave();
  auto& P=Scene.Tracked.FindChecked(Id);
  if(!TestTrue(TEXT("Existing Partial history"),!P.History.GetRecords().IsEmpty())) return false;
  const auto Old=P.History.GetRecords()[0]; const auto OB=Old.SpatialMemory.GetBounds();
  const FVector2D Center=OB.GetCenter();
  // Two interior cuts; deliberately not aligned to coarse or fine edges.
  const FBox2D B(FVector2D(Center.X-19.83,OB.Min.Y-5),FVector2D(Center.X+20.17,OB.Max.Y+5));
  auto* Region=F.World->GetSubsystem<UDarkwellMemoryRegionSubsystem>();
  if(!TestTrue(TEXT("Fixed AABB can straddle Partial"),Region->ConfigureRegion(B.Min,B.Max))) return false;
  TestTrue(TEXT("Min is inside"),Contains(B,B.Min)); TestFalse(TEXT("Max is outside"),Contains(B,B.Max));
  const auto S=Old.FineHistory.GetSize();
  auto Inside=[&](int32 I){return Contains(B,Darkwell::MemoryRegionSamples::Center(OB,S,I));};
  const bool Clear=FCString::Strcmp(Sequence,TEXT("B"))!=0;
  const bool Block=FCString::Strcmp(Sequence,TEXT("A"))!=0;
  auto Stored=[&](bool In){int32 N=0;for(const auto& R:P.History.GetRecords()) if(!R.bCurrentObservedLocation)
   for(int32 I=0;I<R.FineHistory.GetSamples().Num();++I)
    if(Contains(B,Darkwell::MemoryRegionSamples::Center(R.FineHistory.GetBounds(),R.FineHistory.GetSize(),I))==In && R.FineHistory.GetSamples()[I].InitialRemembered>0) ++N;return N;};
  TestTrue(TEXT("Positive retained samples on both sides"),Stored(true)>0 && Stored(false)>0);
  // Fixed overhead scene view plus direct CPU-mirror readback, never input to authority.
  AActor* CaptureOwner=nullptr; USceneCaptureComponent2D* Capture=nullptr; UTextureRenderTarget2D* Target=nullptr;
  if(!GUsingNullRHI)
  {
   CaptureOwner=F.World->SpawnActor<AActor>();
   Capture=NewObject<USceneCaptureComponent2D>(CaptureOwner); CaptureOwner->AddInstanceComponent(Capture);
   Capture->RegisterComponent(); Capture->ProjectionType=ECameraProjectionMode::Orthographic;
   const FVector Eye(Center.X-500,Center.Y-600,750);
   Capture->SetWorldLocation(Eye); Capture->SetWorldRotation((FVector(Center,70)-Eye).Rotation());
   Capture->OrthoWidth=440; Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false;
   Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;
   Capture->ShowFlags.SetTemporalAA(false); Capture->ShowFlags.SetMotionBlur(false);
   Target=NewObject<UTextureRenderTarget2D>(CaptureOwner); Target->InitCustomFormat(384,384,PF_B8G8R8A8,false);
   Target->UpdateResourceImmediate(); Capture->TextureTarget=Target;
  }
  auto Snapshot=[&](const TCHAR* Stage)
  {
   TestTrue(*FString::Printf(TEXT("No double gray contributors %s %s"),Sequence,Stage),Scene.GetMaxOverlapContributorsForTesting(Id)<=1);
   TestTrue(*FString::Printf(TEXT("No double cap contributors %s %s"),Sequence,Stage),Scene.GetMaxCapContributorsForTesting(Id)<=1);
   if(GUsingNullRHI) return;
   for(const auto& Pair:P.Visuals)
   {
    const auto& V=Pair.Value;
    if(!V.Render.Texture.IsValid() || V.SubmittedPresentation.IsEmpty()) continue;
    const FIntPoint TS(V.Render.Texture->GetSizeX(),V.Render.Texture->GetSizeY());
    const FTextureRHIRef Tex=V.Render.Texture->GetResource()->TextureRHI;
    TArray<FFloat16Color> Readback;
    ENQUEUE_RENDER_COMMAND(UnknownPartialReadFineMirror)([Tex,TS,&Readback](FRHICommandListImmediate& Cmd)
     {Cmd.ReadSurfaceFloatData(Tex,FIntRect(0,0,TS.X,TS.Y),Readback,ECubeFace::CubeFace_PosX,0,0);});
    FlushRenderingCommands(); int32 Bad=0, BadB=0;
    for(int32 I=0;I<V.SubmittedPresentation.Num();++I)
    {
     if(!Readback.IsValidIndex(I) || float(Readback[I].A)!=V.SubmittedPresentation[I].A) ++Bad;
     if(!Readback.IsValidIndex(I) || float(Readback[I].B)!=float(FFloat16Color(V.SubmittedPresentation[I]).B)) ++BadB;
    }
    TestEqual(TEXT("D3D12 fine hard gate exactly mirrors CPU submission"),Bad,0);
    TestEqual(TEXT("D3D12 bilinear B exactly mirrors half-float submission"),BadB,0);
   }
   const auto Size=Region->GetSize(); TArray<FColor> Pixels;
   const FTextureRHIRef Texture=Region->GetPresentationTexture()->GetResource()->TextureRHI;
   ENQUEUE_RENDER_COMMAND(UnknownReadAuthorityMirror)([Texture,Size,&Pixels](FRHICommandListImmediate& RHICmdList)
    {RHICmdList.ReadSurfaceData(Texture,FIntRect(0,0,Size.X,Size.Y),Pixels,FReadSurfaceDataFlags(RCM_UNorm));});
   FlushRenderingCommands();
   int32 Mismatch=0; const FVector2D Step=B.GetSize()/FVector2D(Size);
   for(int32 Y=0;Y<Size.Y;++Y) for(int32 X=0;X<Size.X;++X)
   {
    const bool Known=Region->QueryKnowledge(B.Min+FVector2D(X+.5,Y+.5)*Step)==Region->Remembered();
    if(!Pixels.IsValidIndex(Y*Size.X+X) || (Pixels[Y*Size.X+X].R>127)!=Known) ++Mismatch;
   }
   TestEqual(TEXT("D3D12 texture exactly mirrors CPU knowledge; no renderer grants"),Mismatch,0);
   FString ReportPath;
   FParse::Value(FCommandLine::Get(),TEXT("ReportExportPath="),ReportPath);
   const FString Root=ReportPath.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("UnknownRegion"):FPaths::GetPath(ReportPath)/TEXT("Captures");
   const FString Dir=Root/TEXT("SpatialPartialCut")/Sequence;
   IFileManager::Get().MakeDirectory(*Dir,true);
   auto Save=[&](const FString& Path,int32 W,int32 H,const TArray<FColor>& Data)
   {TArray<uint8> PNG;FImageUtils::CompressImageArray(W,H,Data,PNG);FFileHelper::SaveArrayToFile(PNG,*Path);};
   Save(Dir/(FString(Stage)+TEXT("_knowledge.png")),Size.X,Size.Y,Pixels);
   if(FString(Stage).StartsWith(TEXT("07")) || FString(Stage).StartsWith(TEXT("09")))
   {
    FString CSV=TEXT("epoch,index,x,y,initial,opacity,aa,b,a,footprint,state\n");
    for(const auto& R:P.History.GetRecords())
    {
     const auto* V=P.Visuals.Find(R.Epoch); if(!V) continue;
     const auto FS=R.FineHistory.GetSize(); TArray<FColor> Gray;
     for(int32 I=0;I<R.FineHistory.GetSamples().Num();++I)
     {
      const auto& Q=R.FineHistory.GetSamples()[I];
      const auto W=Darkwell::MemoryRegionSamples::Center(R.FineHistory.GetBounds(),FS,I);
      const auto T=V->SubmittedPresentation.IsValidIndex(I)?V->SubmittedPresentation[I]:FLinearColor::Transparent;
      CSV+=FString::Printf(TEXT("%u,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.0f,%d,%s\n"),R.Epoch,I,W.X,W.Y,Q.InitialRemembered,Q.Opacity,Q.FrozenAAEnvelope,T.B,T.A,R.GeometryFootprint.IsValidIndex(I)&&R.GeometryFootprint[I],*Q.State.ToString());
      Gray.Add(FLinearColor(T.B,T.B,T.B,1).ToFColor(false));
     }
     if(!Gray.IsEmpty()) Save(Dir/FString::Printf(TEXT("%s_epoch%u_B.png"),Stage,R.Epoch),FS.X,FS.Y,Gray);
    }
    FFileHelper::SaveStringToFile(CSV,*(Dir/(FString(Stage)+TEXT("_samples.csv"))));
    int32 Probes=0,UnpaddedDips=0,SubmittedDips=0;
    for(const auto& R:P.History.GetRecords())
    {
     const auto* V=P.Visuals.Find(R.Epoch); if(!V || V->bPresentationRetired) continue;
     const auto FS=R.FineHistory.GetSize(); const auto Bounds=R.FineHistory.GetBounds();
     if(V->SubmittedPresentation.Num()!=FS.X*FS.Y || R.GeometryFootprint.Num()!=FS.X*FS.Y) continue;
     TArray<FLinearColor> Raw; R.FineHistory.BuildPresentation(Raw);
     for(const auto& Part:V->PartGeometry)
     {
      const auto L=Part.LocalBounds;
      const FVector Corners[]{FVector(L.Min.X,L.Min.Y,L.GetCenter().Z),FVector(L.Max.X,L.Min.Y,L.GetCenter().Z),FVector(L.Max.X,L.Max.Y,L.GetCenter().Z),FVector(L.Min.X,L.Max.Y,L.GetCenter().Z)};
      for(int32 Edge=0;Edge<4;++Edge) for(int32 K=1;K<512;++K)
      {
       const FVector2D W(Part.WorldTransform.TransformPosition(FMath::Lerp(Corners[Edge],Corners[(Edge+1)%4],K/512.0)));
       const auto UV=(W-Bounds.Min)/Bounds.GetSize()*FVector2D(FS)-FVector2D(.5);
       const int32 X=FMath::FloorToInt(UV.X),Y=FMath::FloorToInt(UV.Y);
       if(X<0 || Y<0 || X+1>=FS.X || Y+1>=FS.Y) continue;
       const int32 Indices[]{Y*FS.X+X,Y*FS.X+X+1,(Y+1)*FS.X+X,(Y+1)*FS.X+X+1};
       bool InteriorFull=true,Exterior=false;
       for(int32 I:Indices) if(R.GeometryFootprint[I]) InteriorFull &= Raw[I].B==1 && V->SubmittedPresentation[I].A==1; else Exterior=true;
       if(!InteriorFull || !Exterior) continue; // Exclude genuine observation/cut/fade edges.
       const float FX=UV.X-X,FY=UV.Y-Y;
       auto Filter=[&](const TArray<FLinearColor>& T){return FMath::Lerp(FMath::Lerp(T[Indices[0]].B,T[Indices[1]].B,FX),FMath::Lerp(T[Indices[2]].B,T[Indices[3]].B,FX),FY);};
       ++Probes; UnpaddedDips+=Filter(Raw)<.999f; SubmittedDips+=Filter(V->SubmittedPresentation)<.999f;
      }
     }
    }
    AddInfo(FString::Printf(TEXT("ROTATED_FILTER stage=%s probes=%d unpadded_dips=%d submitted_dips=%d"),Stage,Probes,UnpaddedDips,SubmittedDips));
    TestTrue(TEXT("Rotated silhouette exercises bilinear exterior taps"),Probes>0 && UnpaddedDips>0);
    TestEqual(TEXT("Physical silhouette does not modulate fully retained gray"),SubmittedDips,0);
   }
   FAssetCompilingManager::Get().FinishAllCompilation();
#if WITH_EDITOR
   if(GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
#endif
   F.World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
   // The first capture admits scene-view shader/PSO work in a fresh automation
   // world. Resolve it, then read the same unchanged CPU state, not a fallback.
#if WITH_EDITOR
   if(GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
#endif
   F.World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
   TArray<FColor> View; Target->GameThread_GetRenderTargetResource()->ReadPixels(View);
   if(TestEqual(TEXT("D3D12 scene capture is complete"),View.Num(),384*384)) Save(Dir/(FString(Stage)+TEXT("_scene.png")),384,384,View);

  };

  Snapshot(TEXT("01_gray"));
  ADarkwellObjectMemoryScene::FPresentationTicket Ticket;
  TestTrue(TEXT("Release genuine A0 resources before transaction"),Scene.ReleaseHistoricalPresentationForTesting(Id,Old.Epoch,Ticket));
  if(Clear) TestTrue(TEXT("Sample clear"),Region->ClearMemory());
  if(Block) TestTrue(TEXT("Sample block"),Region->SetBlockMemoryWrites(true));
  TestTrue(TEXT("A0 rebuild reads latest CPU sample state"),Scene.RebuildHistoricalPresentationForTesting(Ticket));
  auto* R=P.History.FindRecord(Old.Epoch);
  if(!TestNotNull(TEXT("Outside knowledge retains record identity"),R)) return false;
  int32 WrongInside=0,WrongOutside=0;
  for(int32 I=0;I<S.X*S.Y;++I)
  {
   const auto& Now=R->FineHistory.GetSamples()[I]; const auto& Before=Old.FineHistory.GetSamples()[I];
   if(Inside(I)) {if(Clear && (Now.InitialRemembered!=0 || Now.Opacity!=0 || Now.FrozenAAEnvelope!=0 || Now.bVerifiedEmpty || Now.State!=FDarkwellHistoryGridV2::NeverObserved())) ++WrongInside;}
   else if(Now.State!=Before.State || Now.InitialRemembered!=Before.InitialRemembered || Now.Opacity!=Before.Opacity || Now.FrozenAAEnvelope!=Before.FrozenAAEnvelope || Now.bVerifiedEmpty!=Before.bVerifiedEmpty) ++WrongOutside;
  }
  TestEqual(TEXT("Inside clear erases all fine facts and AA"),WrongInside,0);
  TestEqual(TEXT("Every outside fine sample is unchanged"),WrongOutside,0);
  const auto CS=Old.SpatialMemory.GetSize(); WrongOutside=0;WrongInside=0;
  for(int32 I=0;I<CS.X*CS.Y;++I)
  {
   const auto& C=R->SpatialMemory.GetCells()[I]; const auto& O=Old.SpatialMemory.GetCells()[I];
   if(Contains(B,Darkwell::MemoryRegionSamples::Center(OB,CS,I))) {if(Clear && (C.InitialRemembered!=0 || C.DiscoveredPresent!=0 || C.RemainingStale!=0 || C.StaleOpacity!=0)) ++WrongInside;}
   else if(FMemory::Memcmp(&C,&O,sizeof(C))!=0) ++WrongOutside;
  }
  TestEqual(TEXT("Inside coarse facts erased"),WrongInside,0);TestEqual(TEXT("Outside coarse exactly preserved"),WrongOutside,0);
  auto CheckPresentation=[&](bool HiddenInside)
  {
   const auto* V=P.Visuals.Find(Old.Epoch); if(!V) {AddError(TEXT("Missing old visual"));return;}
   int32 Leak=0,Lost=0;
   for(int32 I=0;I<V->SubmittedPresentation.Num();++I)
    if(Inside(I)) {if(HiddenInside && V->SubmittedPresentation[I].A>0) ++Leak;}
    else if(Old.FineHistory.GetSamples()[I].InitialRemembered>0 && R->FineHistory.GetSamples()[I].State==FDarkwellHistoryGridV2::Unresolved() && V->SubmittedPresentation[I].A==0) ++Lost;
   TestEqual(TEXT("No hard-gate leak inside"),Leak,0);TestEqual(TEXT("Retained outside surface survives"),Lost,0);
   TestTrue(TEXT("Cut has cap geometry"),V->CapTriangles>0);
   TestEqual(TEXT("Cap stays within recorded geometry"),Scene.GetCapVerticesOutsideSourceForTesting(Id),0);
  };
  CheckPresentation(true);Snapshot(TEXT("02_cut"));
  Observe();
  TestTrue(TEXT("Live remains visible during crossing Block"),Scene.IsCurrentSourceVisibleForTesting(Id));
  TestTrue(TEXT("Partial capture remains enabled outside"),Scene.IsCaptureEligible(P));
  if(Block) for(const auto& Part:P.CurrentLive.Parts)
  {
   const auto LS=Part.Local.GetSize();
   int32 Leaks=0,OutsideWritten=0,LiveInside=0,InsideSamples=0;
   for(int32 I=0;I<Part.Local.GetCells().Num();++I)
   {
    const auto W=FVector2D(Part.Pose.TransformPosition(FVector(Darkwell::MemoryRegionSamples::Center(Part.Local.GetBounds(),LS,I),0)));
    const auto& C=Part.Local.GetCells()[I];
    if(Contains(B,W)) {++InsideSamples;Leaks+=C.DiscoveredPresent>0 || Part.LastLegalCaptureMask[I];LiveInside+=C.CurrentLegalCoverage>=.99f && C.AppearanceBlend>0;}
    else OutsideWritten+=C.DiscoveredPresent>0;
   }
   TestEqual(TEXT("Blocked local knowledge stays empty"),Leaks,0);
   TestTrue(TEXT("Outside local writes continue"),OutsideWritten>0);
   if(InsideSamples>0) TestTrue(TEXT("Inside legal Live has independent appearance"),LiveInside>0);
  }
  Snapshot(TEXT("03_live"));Leave();
  if(Block)
  {
   int32 NewInside=0,NewOutside=0;
   for(const auto& N:P.History.GetRecords()) if(N.Epoch!=Old.Epoch)
    for(int32 I=0;I<N.FineHistory.GetSamples().Num();++I) if(N.FineHistory.GetSamples()[I].InitialRemembered>0)
     {if(Contains(B,Darkwell::MemoryRegionSamples::Center(N.FineHistory.GetBounds(),N.FineHistory.GetSize(),I))) ++NewInside;else ++NewOutside;}
   TestEqual(TEXT("New captures contain no blocked fine knowledge"),NewInside,0);
   TestTrue(TEXT("New captures retain outside fine knowledge"),NewOutside>0);
   Snapshot(TEXT("04_left"));
   TestTrue(TEXT("Unblock"),Region->SetBlockMemoryWrites(false));F.Step(5);
   TestEqual(TEXT("Clear plus Block cannot restore old inside facts"),Stored(true)>0,!Clear);
   if(!Clear)
   {
    const auto* Original=P.History.FindRecord(Old.Epoch);int32 Changed=0;
    if(!Original) ++Changed;
    else for(int32 I=0;I<S.X*S.Y;++I) if(Inside(I))
    {const auto& N=Original->FineHistory.GetSamples()[I];const auto& O=Old.FineHistory.GetSamples()[I];
     if(N.State!=O.State || N.InitialRemembered!=O.InitialRemembered || N.Opacity!=O.Opacity || N.FrozenAAEnvelope!=O.FrozenAAEnvelope || N.bVerifiedEmpty!=O.bVerifiedEmpty) ++Changed;}
    TestEqual(TEXT("Block-only preserves every old inside fine field"),Changed,0);
   }
   Snapshot(TEXT("05_unblock"));
  }
  if(Clear && Block) {F.Step(10);TestEqual(TEXT("Idle does not resurrect gray"),Stored(true),0);}
  Observe();Leave();TestTrue(TEXT("New legal observation rebuilds inside gray"),Stored(true)>0);
  Snapshot(TEXT("06_rebuilt"));
  if(Clear)
  {
   const auto* Previous=P.History.FindRecord(Old.Epoch);int32 Revived=0;
   if(Previous) for(int32 I=0;I<S.X*S.Y;++I) if(Inside(I)) Revived+=Previous->FineHistory.GetSamples()[I].InitialRemembered>0;
   TestEqual(TEXT("Fresh observation cannot reinitialize cleared old epoch"),Revived,0);
  }
  // Hidden rigid pose change, then legal resweep: old world samples must stay erased.
  auto Pose=P.Actual->GetActorTransform();Pose.SetRotation(FRotator(0,37,0).Quaternion());
  TestTrue(TEXT("Rotate actual geometry"),Scene.SetTrackedTransformForTesting(Id,Pose));F.Step(3);
  Observe();F.Face(146);F.Step(15);Leave();Snapshot(TEXT("07_rotated_resweep"));
  TestEqual(TEXT("Rotated caps remain inside source geometry"),Scene.GetCapVerticesOutsideSourceForTesting(Id),0);
  const auto NewestRotated=P.History.GetRecords().Last();int32 FalseEmpty=0;
  for(int32 I=0;I<NewestRotated.FineHistory.GetSamples().Num();++I)
   if(NewestRotated.FineHistory.GetSamples()[I].InitialRemembered>0 && NewestRotated.GeometryFootprint[I]) FalseEmpty+=NewestRotated.FineHistory.GetSamples()[I].bVerifiedEmpty;
  TestEqual(TEXT("Rotated recorded edge cells intersecting unchanged actual geometry are not false-empty"),FalseEmpty,0);
  if(Clear)
  {
   const auto* Previous=P.History.FindRecord(Old.Epoch);int32 Revived=0;
   if(Previous) for(int32 I=0;I<S.X*S.Y;++I) if(Inside(I)) Revived+=Previous->FineHistory.GetSamples()[I].InitialRemembered>0;
   TestEqual(TEXT("Rotation does not resurrect old pose gray"),Revived,0);
  }
  Observe();
  TestTrue(TEXT("Enable cut Block during rotated Live"),Region->SetBlockMemoryWrites(true));
  TestTrue(TEXT("Cut Block does not add a first-display frame"),Scene.IsCurrentSourceVisibleForTesting(Id));
  TestTrue(TEXT("Clear cut during rotated blocked Live"),Region->ClearMemory());
  TestTrue(TEXT("Clear preserves same-call Live"),Scene.IsCurrentSourceVisibleForTesting(Id));
  Snapshot(TEXT("08_live_transaction"));
  Leave();TestTrue(TEXT("Unblock after rotated Live transaction"),Region->SetBlockMemoryWrites(false));F.Step(10);
  TestEqual(TEXT("No retained inside sample after rotated Clear plus Block"),Stored(true),0);
  TestTrue(TEXT("Outside gray survives rotated transaction"),Stored(false)>0);
  auto SubmissionHash=[&](){uint64 H=0;for(const auto& V:P.Visuals) {H=HashCombineFast(H,V.Value.TextureSignature);H=HashCombineFast(H,V.Value.CapSignature);}return H;};
  const auto Stable=SubmissionHash();
  for(int32 Frame=0;Frame<10;++Frame) {F.Step();TestEqual(TEXT("Idle cut has no cap or texture oscillation"),SubmissionHash(),Stable);}
  int32 OutsideRotatedAALoss=0;
  const auto& AfterRotated=P.History.GetRecords().Last();
  for(int32 I=0;I<AfterRotated.FineHistory.GetSamples().Num();++I)
   if(!Contains(B,Darkwell::MemoryRegionSamples::Center(AfterRotated.FineHistory.GetBounds(),AfterRotated.FineHistory.GetSize(),I)))
   {
    const auto& BeforeSample=NewestRotated.FineHistory.GetSamples()[I];
    const auto& AfterSample=AfterRotated.FineHistory.GetSamples()[I];
    if(BeforeSample.InitialRemembered>0 && (AfterSample.InitialRemembered==0 || AfterSample.FrozenAAEnvelope<BeforeSample.FrozenAAEnvelope)) ++OutsideRotatedAALoss;
   }
  TestEqual(TEXT("Rotated Clear Block preserves all outside known AA support"),OutsideRotatedAALoss,0);
  Snapshot(TEXT("09_no_resurrection"));
  // Diagnostic isolation only. Acceptance uses 09 with the real gray and caps
  // both enabled. These images must never substitute for its visual review.
  if(FCString::Strcmp(Sequence,TEXT("A"))==0)
  {
   int32 Quads=0,AxisX=0,AxisY=0;FString CSV=TEXT("epoch,visible,yaw,ax,ay,az,bx,by,bz,cx,cy,cz,dx,dy,dz\n");
   for(auto& V:P.Visuals)
   {
    for(const auto& Q:V.Value.CapQuads) {++Quads;AxisX+=FMath::Abs(Q.A.X-Q.B.X)<.001;AxisY+=FMath::Abs(Q.A.Y-Q.B.Y)<.001;}
    const auto* DiagnosticRecord=P.History.FindRecord(V.Key);
    for(const auto& Q:V.Value.CapQuads) CSV+=FString::Printf(TEXT("%u,%d,%.5f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n"),V.Key,V.Value.Render.Cap.IsValid() && V.Value.Render.Cap->IsVisible(),DiagnosticRecord?DiagnosticRecord->SnapshotTransform.Rotator().Yaw:0,Q.A.X,Q.A.Y,Q.A.Z,Q.B.X,Q.B.Y,Q.B.Z,Q.C.X,Q.C.Y,Q.C.Z,Q.D.X,Q.D.Y,Q.D.Z);
    if(V.Value.Render.Cap.IsValid()) V.Value.Render.Cap->SetVisibility(false);
   }
   FFileHelper::SaveStringToFile(CSV,*(FPaths::ProjectSavedDir()/TEXT("UnknownPartial/rotated_caps.csv")));
   AddInfo(FString::Printf(TEXT("ROTATED_CAP_DIAGNOSTIC quads=%d axis_x=%d axis_y=%d"),Quads,AxisX,AxisY));
   Snapshot(TEXT("10_diagnostic_caps_off"));
   for(auto& V:P.Visuals)
   {
    if(V.Value.Render.Cap.IsValid()) V.Value.Render.Cap->SetVisibility(V.Value.CapTriangles>0);
    if(V.Value.Render.Proxy.IsValid()) V.Value.Render.Proxy->SetActorHiddenInGame(true);
   }
   Snapshot(TEXT("11_diagnostic_caps_only"));
   for(auto& V:P.Visuals) if(V.Value.Render.Proxy.IsValid()) V.Value.Render.Proxy->SetActorHiddenInGame(V.Value.bPresentationRetired);
  }
  Observe();Leave();
  TestTrue(TEXT("Post-rotation fresh observation rebuilds cleared region"),Stored(true)>0);
  TestEqual(TEXT("Post-rotation reobservation caps remain inside geometry"),Scene.GetCapVerticesOutsideSourceForTesting(Id),0);
  Snapshot(TEXT("12_rotated_reobserved"));
  AddInfo(FString::Printf(TEXT("UNKNOWN_PARTIAL_CUT sequence=%s fine=%dx%d no_resurrection=1"),Sequence,S.X,S.Y));
  if(CaptureOwner) CaptureOwner->Destroy();
 }
 return true;
}

// Temporal history must advance on real engine frames, not repeated captures
// inside RunTest's single frame. This is visual validation, never CPU knowledge.
class FUnknownPartialTemporalCommand final : public IAutomationLatentCommand
{
public:
 explicit FUnknownPartialTemporalCommand(FAutomationTestBase* InTest):Test(InTest) {}
 virtual ~FUnknownPartialTemporalCommand()
 {
  if(Room) { if(Owner) Owner->Destroy(); Room->World->RemoveFromRoot(); Room.Reset(); }
 }
 virtual bool Update() override
 {
  using namespace Darkwell::GrayObjectPolicyTests;
  if(!Room)
  {
   Room=MakeUnique<FRoom>(); Room->World->AddToRoot();
   Room->Room->ResetTrackedRevealPolicyForLab(Id,Reveal::SpatialPartial,100,History::StationaryOnly);
   auto& P=Room->Room->Tracked.FindChecked(Id); const auto B=Room->Room->ActualBounds(*P.Actual);
   Center=B.GetCenter();
   Region=Room->World->GetSubsystem<UDarkwellMemoryRegionSubsystem>();
   Owner=Room->World->SpawnActor<AActor>(); Capture=NewObject<USceneCaptureComponent2D>(Owner);
   Owner->AddInstanceComponent(Capture); Capture->RegisterComponent();
   const FVector Eye(Center.X-500,Center.Y-600,750);
   Capture->SetWorldLocation(Eye); Capture->SetWorldRotation((FVector(Center,70)-Eye).Rotation());
   Capture->ProjectionType=ECameraProjectionMode::Orthographic; Capture->OrthoWidth=440;
   Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false; Capture->bAlwaysPersistRenderingState=true;
   Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;
   Capture->ShowFlags.SetTemporalAA(true); Capture->ShowFlags.SetMotionBlur(false);
   // Fixed exposure makes independent before/after sessions comparable.
   Capture->PostProcessSettings.bOverride_AutoExposureMinBrightness=true;
   Capture->PostProcessSettings.bOverride_AutoExposureMaxBrightness=true;
   Capture->PostProcessSettings.AutoExposureMinBrightness=1;
   Capture->PostProcessSettings.AutoExposureMaxBrightness=1;
   Target=NewObject<UTextureRenderTarget2D>(Owner); Target->InitCustomFormat(768,768,PF_B8G8R8A8,false);
   Target->UpdateResourceImmediate(); Capture->TextureTarget=Target;
   FString Report; FParse::Value(FCommandLine::Get(),TEXT("ReportExportPath="),Report);
   Dir=(Report.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("UnknownRegion"):FPaths::GetPath(Report)/TEXT("Captures"))/TEXT("SpatialPartialTemporal");
   IFileManager::Get().MakeDirectory(*Dir,true);
   FAssetCompilingManager::Get().FinishAllCompilation();
   if(GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
  }
  auto& Scene=*Room->Room; auto& P=Scene.Tracked.FindChecked(Id);
  if(Frame==26 || Frame==66 || Frame==121 || Frame==162 || Frame==213) Room->Face(-90);
  if(Frame==41)
  {
   const auto B=P.History.GetRecords()[0].FineHistory.GetBounds();
   CutBounds=FBox2D(FVector2D(Center.X-19.83,B.Min.Y-5),FVector2D(Center.X+20.17,B.Max.Y+5));
   Test->TestTrue(TEXT("Temporal fixed region"),Region->ConfigureRegion(CutBounds.Min,CutBounds.Max));
   Test->TestTrue(TEXT("Temporal initial Clear"),Region->ClearMemory()); Room->Face(90);
  }
  if(Frame==81)
  {
   auto Pose=P.Actual->GetActorTransform(); Pose.SetRotation(FRotator(0,37,0).Quaternion());
   Test->TestTrue(TEXT("Temporal rotate 37 degrees"),Scene.SetTrackedTransformForTesting(Id,Pose)); Room->Face(90);
  }
  if(Frame==106) Room->Face(146);
  if(Frame==136 || Frame==188) Room->Face(90);
  if(Frame==161)
  {
   Test->TestTrue(TEXT("Temporal Block"),Region->SetBlockMemoryWrites(true));
   Test->TestTrue(TEXT("Temporal Clear in Live"),Region->ClearMemory());
   Test->TestTrue(TEXT("Temporal transaction same-call Live"),Scene.IsCurrentSourceVisibleForTesting(Id));
   for(const auto& R:P.History.GetRecords()) ClearedEpoch=FMath::Max(ClearedEpoch,R.Epoch);
  }
  if(Frame==177) Test->TestTrue(TEXT("Temporal unblock"),Region->SetBlockMemoryWrites(false));
  Room->Step();
  Room->World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
  const TCHAR* Stage=Frame==40?TEXT("01_gray"):Frame==135?TEXT("07_rotated_resweep"):
   Frame==161?TEXT("08_live_transaction"):Frame==176?TEXT("08b_left_blocked"):
   Frame==177?TEXT("08c_unblock_first_frame"):Frame==187?TEXT("09_no_resurrection"):
   Frame==227?TEXT("12_rotated_reobserved"):nullptr;
  if(Stage)
  {
   if(Frame>=176)
   {
    int32 OldInside=0,NewInside=0;
    for(const auto& R:P.History.GetRecords()) for(int32 I=0;I<R.FineHistory.GetSamples().Num();++I)
    {
     const auto W=Darkwell::MemoryRegionSamples::Center(R.FineHistory.GetBounds(),R.FineHistory.GetSize(),I);
     if(Darkwell::MemoryRegionSamples::Contains(CutBounds,W) && R.FineHistory.GetSamples()[I].InitialRemembered>0)
      { if(R.Epoch<=ClearedEpoch) ++OldInside; else ++NewInside; }
    }
    Test->TestEqual(TEXT("Temporal old cleared epochs never revive"),OldInside,0);
    if(Frame<188) Test->TestEqual(TEXT("Temporal leave and unblock do not grant new gray"),NewInside,0);
    if(Frame==227) Test->TestTrue(TEXT("Temporal new legal observation rebuilds gray"),NewInside>0);
   }
   TArray<FColor> View; Target->GameThread_GetRenderTargetResource()->ReadPixels(View);
   Test->TestEqual(TEXT("Temporal real D3D12 capture complete"),View.Num(),768*768);
   TArray<uint8> PNG; FImageUtils::CompressImageArray(768,768,View,PNG); FFileHelper::SaveArrayToFile(PNG,*(Dir/(FString(Stage)+TEXT(".png"))));
   Test->TestEqual(TEXT("Temporal cap stays inside source"),Scene.GetCapVerticesOutsideSourceForTesting(Id),0);
   Test->TestTrue(TEXT("Temporal single gray contributor"),Scene.GetMaxOverlapContributorsForTesting(Id)<=1);
   Test->TestTrue(TEXT("Temporal single cap contributor"),Scene.GetMaxCapContributorsForTesting(Id)<=1);
   Test->AddInfo(FString::Printf(TEXT("TEMPORAL_SURFACE stage=%s engine_frame=%llu aa=%d"),Stage,GFrameCounter,IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod"))->GetInt()));
  }
  return ++Frame>227;
 }
private:
 FAutomationTestBase* Test;
 TUniquePtr<Darkwell::GrayObjectPolicyTests::FRoom> Room;
 AActor* Owner=nullptr;
 USceneCaptureComponent2D* Capture=nullptr;
 UTextureRenderTarget2D* Target=nullptr;
 UDarkwellMemoryRegionSubsystem* Region=nullptr;
 FVector2D Center;
 FBox2D CutBounds=FBox2D(ForceInit);
 FString Dir;
 int32 Frame=1;
 uint32 ClearedEpoch=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellUnknownPartialTemporal,"Darkwell.UnknownPartial.TemporalSurface",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellUnknownPartialTemporal::RunTest(const FString&)
{
 if(GUsingNullRHI) { AddError(TEXT("TemporalSurface requires real D3D12 rendering")); return false; }
 ADD_LATENT_AUTOMATION_COMMAND(FUnknownPartialTemporalCommand(this));
 return true;
}

#endif
