#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Player/DarkwellCharacter.h"
#include "VisionPresentation/DarkwellCurrentLiveGrid.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "VisionPresentation/DarkwellMovingPropLabRoom.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "Visibility/SightWeave/DarkwellSightWeaveWorldSubsystem.h"

namespace Darkwell::ConfirmedWholeViewEdgeTests
{
 FDarkwellCurrentLiveGrid MakeGrid(FDarkwellSpatialPropMemory& Snapshot)
 {
  FDarkwellCurrentLiveGrid Grid;
  FDarkwellCurrentLiveGrid::FDescriptor Part;
  Part.PrimitiveKey=1;
  Part.MeshKey=2;
  Part.LocalBounds=FBox(FVector(-50,-25,0),FVector(50,25,100));
  Grid.ResetGeometry(TEXT("Test.ConfirmedWhole.ViewEdge"),TArray{Part},FTransform::Identity);
  Snapshot.Initialize(TEXT("Test.ConfirmedWhole.ViewEdge"),FBox2D(FVector2D(-50,-25),FVector2D(50,25)),2.5f);
  Snapshot.BeginPresent();
  return Grid;
 }

 bool IsUniform(const FDarkwellCurrentLiveGrid::FDividerDiagnostics& Diagnostic)
 {
  return FMath::IsNearlyEqual(Diagnostic.MinimumAppearance,Diagnostic.MaximumAppearance,UE_SMALL_NUMBER);
 }

 const FName LabId(TEXT("Lab.InWorld.Rotate.Cabinet"));
 struct FRoom
 {
  UWorld* World=nullptr;
  ADarkwellCharacter* Player=nullptr;
  ADarkwellPropGameplayLab* Fixture=nullptr;
  ADarkwellMovingPropLabRoom* Room=nullptr;
  UDarkwellSightWeaveWorldSubsystem* Adapter=nullptr;
  FRoom()
  {
   UPackage* Package=CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab"));
   World=NewObject<UWorld>(Package,MakeUniqueObjectName(Package,UWorld::StaticClass(),TEXT("ConfirmedWholeViewEdge")),RF_Transient);
   World->WorldType=EWorldType::Game;
   GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
   World->InitializeNewWorld(UWorld::InitializationValues().InitializeScenes(true).AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false));
   World->URL.AddOption(TEXT("PropLabOriginal"));
   World->URL.AddOption(TEXT("InWorldControls"));
   FActorSpawnParameters Parameters;
   Parameters.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   Player=World->SpawnActor<ADarkwellCharacter>(ADarkwellCharacter::StaticClass(),FVector(-300,100,92),FRotator(0,90,0),Parameters);
   Player->DispatchBeginPlay();
   Fixture=World->SpawnActor<ADarkwellPropGameplayLab>();
   Fixture->PostInitializeComponents();
   Fixture->DispatchBeginPlay();
   Room=ADarkwellMovingPropLabRoom::FindActive(World);
   Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
   Adapter->RequestSightWeaveAuthority(Fixture);
   Room->ResetRoom(Player);
   Face(90);
  }
  ~FRoom()
  {
   Fixture->Destroy();
   World->DestroyWorld(true);
   GEngine->DestroyWorldContext(World);
  }
  void Face(const float Yaw)
  {
   Player->SetActorLocation(FVector(-300,100,92));
   Player->SetActorRotation(FRotator(0,Yaw,0));
  }
  void Step(const int32 Frames=1,const float Dt=1.f/60.f)
  {
   for(int32 Frame=0;Frame<Frames;++Frame)
   {
    Adapter->Tick(Dt);
    Room->UpdateRoom(Dt,Player);
    Fixture->Tick(Dt);
   }
  }
 };

 uint64 PresentationHash(const FDarkwellCurrentLiveGrid& Grid,const TBitArray<>& Mask)
 {
  uint64 Hash=1469598103934665603ull;
  auto Mix=[&](const uint64 Value){Hash=(Hash^Value)*1099511628211ull;};
  for(const auto& Part:Grid.Parts) for(const auto& Cell:Part.Raster.GetCells())
  {
   Mix(FMath::RoundToInt(Cell.AppearanceBlend*1000000));
   Mix(FMath::RoundToInt(Cell.LiveBlend*1000000));
   Mix(Cell.DiscoveredPresent>0);
  }
  for(TConstSetBitIterator<> It(Mask);It;++It) Mix(It.GetIndex()+1);
  return Hash;
 }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellConfirmedWholeFastViewEdgeLeavesNoDivider,
 "Darkwell.PropLab.ConfirmedWholeViewEdge.ConfirmedWholeFastViewEdgeLeavesNoDivider",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellConfirmedWholeFastViewEdgeLeavesNoDivider::RunTest(const FString&)
{
 using namespace Darkwell::ConfirmedWholeViewEdgeTests;
 for(const float SweepDegrees:{90.f,160.f,180.f})
 {
  for(const int32 SweepFrames:{1,8})
  {
   FDarkwellSpatialPropMemory Snapshot;
   FDarkwellCurrentLiveGrid Grid=MakeGrid(Snapshot);
   const FVector2D EdgeNormal=FVector2D(FMath::Cos(FMath::DegreesToRadians(SweepDegrees)),FMath::Sin(FMath::DegreesToRadians(SweepDegrees)));
   auto RawLiveCoverage=[&](const FVector2D Point)
   {
    return FVector2D::DotProduct(Point,EdgeNormal)>=0 ? 1.f : 0.f;
   };
   for(int32 Frame=0;Frame<SweepFrames;++Frame)
   {
    Grid.Advance(FDarkwellSpatialPropMemory::EnterSeconds/SweepFrames,FTransform::Identity,RawLiveCoverage);
   }
   Grid.WriteWorldSnapshot(Snapshot,FBox2D(FVector2D(-50,-25),FVector2D(50,25)));
   Grid.WritePartRasters(RawLiveCoverage,false);
   Grid.ApplyWholeObjectPresentation(1.f/60.f,Snapshot,[](FVector2D){return 1.f;});

   FDarkwellCurrentLiveGrid::FDividerDiagnostics Diagnostic;
   TestTrue(TEXT("Whole part exposes deterministic divider diagnostics"),Grid.GetDividerDiagnostics(0,Diagnostic));
   AddInfo(FString::Printf(TEXT("DIVIDER SOURCE=%s angle=%.0f frames=%d raw=%d/%d occlusion=%d/%d whole=%d/%d appearance_min=%.6f appearance_max=%.6f"),
    FDarkwellCurrentLiveGrid::DividerSourceName(Diagnostic.Source),SweepDegrees,SweepFrames,
    Diagnostic.RawLiveCoverage.CountSetBits(),Diagnostic.RawLiveCoverage.Num(),
    Diagnostic.PhysicalOcclusionGate.CountSetBits(),Diagnostic.PhysicalOcclusionGate.Num(),
    Diagnostic.WholePresentationMask.CountSetBits(),Diagnostic.WholePresentationMask.Num(),
    Diagnostic.MinimumAppearance,Diagnostic.MaximumAppearance));
   TestNotEqual(TEXT("No-wall split no longer contributes a VIEW_EDGE divider"),Diagnostic.Source,FDarkwellCurrentLiveGrid::EDividerSource::ViewEdge);
   TestTrue(TEXT("Raw live coverage contains the swept cone edge"),Diagnostic.RawLiveCoverage.CountSetBits()>0 && Diagnostic.RawLiveCoverage.CountSetBits()<Diagnostic.RawLiveCoverage.Num());
   TestEqual(TEXT("Physical occlusion gate is fully open"),Diagnostic.PhysicalOcclusionGate.CountSetBits(),Diagnostic.PhysicalOcclusionGate.Num());
   TestTrue(TEXT("Whole presentation owns the full primitive"),Diagnostic.WholePresentationMask==Diagnostic.FullGeometryMask);
   TestTrue(*FString::Printf(TEXT("Confirmed Whole appearance is uniform after %.0f degree %s sweep"),SweepDegrees,SweepFrames==1?TEXT("single-frame"):TEXT("multi-frame")),IsUniform(Diagnostic));
  }
 }
 {
  FRoom Room;
  Room.Room->ResetTrackedRevealPolicyForLab(LabId,ESightWeaveRevealMode::WholeObjectAfterSpan,100,ESightWeaveHistoryMode::StationaryOnly);
  Room.Face(90); Room.Step(30);
  TestTrue(TEXT("Room Whole confirms before fast edge sequence"),Room.Room->IsRevealConfirmedForTesting(LabId));
  for(const float Yaw:{0.f,90.f,146.f,-14.f,166.f,-90.f,90.f})
  {
   Room.Face(Yaw); Room.Step();
   if(Room.Room->IsCurrentSourceVisibleForTesting(LabId))
    TestTrue(TEXT("Every contacted confirmed Whole frame uses one object scalar"),Room.Room->IsWholePresentationUniformForTesting(LabId) || FMath::IsNearlyEqual(Room.Room->GetCurrentPresentationMinimumForTesting(LabId),1.f));
  }
  Room.Face(-90); Room.Step(30);
  TBitArray<> Capture,Frozen;
  TestTrue(TEXT("Fast complete exit freezes Whole"),Room.Room->GetNewestCaptureMasksForTesting(LabId,Capture,Frozen));
  ADarkwellMovingPropLabRoom::FDividerMaskDiagnostics Historical;
  TestTrue(TEXT("Fast exit exposes full diagnostic masks"),Room.Room->GetDividerMaskDiagnosticsForTesting(LabId,Historical));
  TestTrue(TEXT("View edge is absent from frozen Whole history"),Capture==Frozen && Frozen==Historical.FullGeometryMask);
  TestEqual(TEXT("Fast exit produces no Whole cap"),Room.Room->GetVisibleHistoricalCapCountForTesting(LabId),0);
  Room.Face(90); Room.Step(30);
  TestTrue(TEXT("Reobserved Whole is again uniformly current"),Room.Room->IsCurrentSourceVisibleForTesting(LabId) && Room.Room->IsWholePresentationUniformForTesting(LabId));
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellConfirmedWholeRepeatedFastSweep,
 "Darkwell.PropLab.ConfirmedWholeViewEdge.ConfirmedWholeRepeatedFastSweep",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellConfirmedWholeRepeatedFastSweep::RunTest(const FString&)
{
 using namespace Darkwell::ConfirmedWholeViewEdgeTests;
 uint64 Reference=0;
 for(const float Hz:{30.f,60.f,120.f,144.f})
 {
  FDarkwellSpatialPropMemory Snapshot;
  FDarkwellCurrentLiveGrid Grid=MakeGrid(Snapshot);
  const FBox2D Bounds(FVector2D(-50,-25),FVector2D(50,25));
  for(int32 Sweep=0;Sweep<1000;++Sweep)
  {
   TArray<float> Raw;
   Raw.SetNumUninitialized(Snapshot.GetSize().X*Snapshot.GetSize().Y);
   for(int32 Y=0;Y<Snapshot.GetSize().Y;++Y) for(int32 X=0;X<Snapshot.GetSize().X;++X)
    Raw[Y*Snapshot.GetSize().X+X]=((X<Snapshot.GetSize().X/2)==((Sweep&1)==0))?1.f:0.f;
   Grid.AdvanceWholeWithOcclusion(1.f/Hz,FTransform::Identity,Snapshot,Bounds,Raw,[](FVector2D){return 1.f;});
  }
  TBitArray<> FullGeometry;
  const FIntPoint FineSize=Snapshot.GetSize()*FDarkwellHistoryGridV2::SamplesPerCell;
  TestTrue(TEXT("Repeated sweep retains a valid FullGeometryMask"),Grid.BuildFullGeometryMask(Bounds,FineSize,FullGeometry));
  FDarkwellSpatialObservationHistory History;
  History.Initialize(TEXT("Test.ConfirmedWhole.Repeated"));
  const int32 Current=History.BeginCurrentObservation(FTransform::Identity,Bounds,2.5f);
  TestTrue(TEXT("Repeated Whole history accepts only the geometry mask"),Current!=INDEX_NONE && History.FreezeCurrentFromGeometryMask(FullGeometry));
  auto& Record=History.GetMutableRecords()[0];
  Record.FineHistory.Initialize(Record.SpatialMemory,Record.LastLegalCaptureMask);
  TBitArray<> Frozen(false,Record.FineHistory.GetSamples().Num());
  for(int32 Index=0;Index<Frozen.Num();++Index) Frozen[Index]=Record.FineHistory.GetSamples()[Index].InitialRemembered>0;
  TestTrue(TEXT("1000 sweeps cannot create a partial Whole history"),Frozen==FullGeometry && Record.LastLegalCaptureMask==FullGeometry);
  for(const auto& Part:Grid.Parts)
  {
   float Minimum=1,Maximum=0;
   for(const auto& Cell:Part.Raster.GetCells()) { Minimum=FMath::Min(Minimum,Cell.AppearanceBlend); Maximum=FMath::Max(Maximum,Cell.AppearanceBlend); }
   TestTrue(TEXT("1000 sweeps leave no permanent object-level divider"),FMath::IsNearlyEqual(Minimum,Maximum,UE_SMALL_NUMBER));
  }
  const uint64 Hash=PresentationHash(Grid,FullGeometry);
  if(Reference==0) Reference=Hash;
  else TestEqual(TEXT("30/60/120/144 Hz converge to the identical state hash"),Hash,Reference);
  AddInfo(FString::Printf(TEXT("CONFIRMED_WHOLE_1000_SWEEPS hz=%.0f hash=%llu capture=%d/%d"),Hz,Hash,FullGeometry.CountSetBits(),FullGeometry.Num()));
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellConfirmedWholeHitchAtomicity,
 "Darkwell.PropLab.ConfirmedWholeViewEdge.ConfirmedWholeHitchAtomicity",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellConfirmedWholeHitchAtomicity::RunTest(const FString&)
{
 using namespace Darkwell::ConfirmedWholeViewEdgeTests;
 for(const float Dt:{.100f,.200f,.333f})
 {
  FDarkwellSpatialPropMemory Snapshot;
  FDarkwellCurrentLiveGrid Grid=MakeGrid(Snapshot);
  TArray<float> Raw;
  Raw.Init(0,Snapshot.GetCells().Num());
  for(int32 Index=0;Index<Raw.Num()/2;++Index) Raw[Index]=1;
  const FBox2D Bounds(FVector2D(-50,-25),FVector2D(50,25));
  Grid.AdvanceWholeWithOcclusion(Dt,FTransform::Identity,Snapshot,Bounds,Raw,[](FVector2D){return 1.f;});
  TBitArray<> FullGeometry;
  Grid.BuildFullGeometryMask(Bounds,Snapshot.GetSize()*FDarkwellHistoryGridV2::SamplesPerCell,FullGeometry);
  FDarkwellSpatialObservationHistory History;
  History.Initialize(TEXT("Test.ConfirmedWhole.Hitch"));
  History.BeginCurrentObservation(FTransform::Identity,Bounds,2.5f);
  TestTrue(TEXT("Hitch capture commits the complete immutable geometry mask"),History.FreezeCurrentFromGeometryMask(FullGeometry));
  TestTrue(TEXT("Hitch capture is never a half Whole"),History.GetRecords()[0].LastLegalCaptureMask==FullGeometry);
  AddInfo(FString::Printf(TEXT("CONFIRMED_WHOLE_HITCH dt_ms=%.0f complete=%d/%d"),Dt*1000,FullGeometry.CountSetBits(),FullGeometry.Num()));
 }
 FDarkwellSpatialObservationHistory Invalid;
 Invalid.Initialize(TEXT("Test.ConfirmedWhole.InvalidRevision"));
 Invalid.BeginCurrentObservation(FTransform::Identity,FBox2D(FVector2D(-50,-25),FVector2D(50,25)),2.5f);
 TBitArray<> Empty;
 TestFalse(TEXT("Incomplete revision result cannot freeze"),Invalid.FreezeCurrentFromGeometryMask(Empty));
 TestTrue(TEXT("Rejected atomic commit leaves current intact for the next coherent snapshot"),Invalid.GetCurrentIndex()!=INDEX_NONE && Invalid.GetRecords().Num()==1);
 {
  FRoom Room;
  Room.Room->ResetTrackedRevealPolicyForLab(LabId,ESightWeaveRevealMode::WholeObjectAfterSpan,100,ESightWeaveHistoryMode::StationaryOnly);
  Room.Face(90); Room.Step(30); TestTrue(TEXT("Whole is confirmed before invalid hitch"),Room.Room->IsRevealConfirmedForTesting(LabId));
  Room.Room->InjectInvalidCoverageOnceForTesting(LabId); Room.Face(-90); Room.Step(1,.333f);
  TestEqual(TEXT("Invalid 333ms publication cannot freeze"),Room.Room->GetStaleEpochCountForTesting(LabId),0);
  TestEqual(TEXT("Invalid 333ms publication retains exactly one current candidate"),Room.Room->GetCurrentEpochCountForTesting(LabId),1);
  Room.Step(1,.333f);
  TBitArray<> Capture,Frozen;
  TestTrue(TEXT("Next coherent publication freezes"),Room.Room->GetNewestCaptureMasksForTesting(LabId,Capture,Frozen));
  ADarkwellMovingPropLabRoom::FDividerMaskDiagnostics Diagnostic;
  TestTrue(TEXT("Frozen hitch diagnostic is available"),Room.Room->GetDividerMaskDiagnosticsForTesting(LabId,Diagnostic));
  TestTrue(TEXT("Coherent retry freezes exactly FullGeometryMask"),Capture==Frozen && Frozen==Diagnostic.FullGeometryMask);
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellConfirmedWholeWallOcclusionIsTransient,
 "Darkwell.PropLab.ConfirmedWholeViewEdge.ConfirmedWholeWallOcclusionIsTransient",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellConfirmedWholeWallOcclusionIsTransient::RunTest(const FString&)
{
 using namespace Darkwell::ConfirmedWholeViewEdgeTests;
 FRoom F;
 F.Room->ResetTrackedRevealPolicyForLab(LabId,ESightWeaveRevealMode::WholeObjectAfterSpan,100,ESightWeaveHistoryMode::StationaryOnly);
 F.Face(90); F.Step(30);
 auto Pose=F.Room->GetTrackedTransform(LabId); Pose.SetLocation(FVector(500,0,0)); F.Room->SetTrackedTransformForTesting(LabId,Pose);
 F.Player->SetActorLocation(FVector(500,-600,92)); F.Player->SetActorRotation(FRotator(0,90,0)); F.Step(30);
 ADarkwellMovingPropLabRoom::FDividerMaskDiagnostics South;
 TestTrue(TEXT("South wall diagnostic"),F.Room->GetDividerMaskDiagnosticsForTesting(LabId,South));
 TBitArray<> SouthVisible=South.FullGeometryMask; SouthVisible.CombineWithBitwiseAND(South.PhysicalOcclusionGate,EBitwiseOperatorFlags::MinSize);
 TestTrue(TEXT("Physical wall transiently cuts current Whole"),SouthVisible.CountSetBits()>0 && SouthVisible.CountSetBits()<South.FullGeometryMask.CountSetBits());
 TestEqual(TEXT("Current divider is attributed to wall occlusion"),South.Source,FDarkwellCurrentLiveGrid::EDividerSource::WallOcclusion);
 TestEqual(TEXT("Confirmed Whole wall current owns no cap"),F.Room->GetVisibleHistoricalCapCountForTesting(LabId),0);
 F.Player->SetActorLocation(FVector(500,600,92)); F.Player->SetActorRotation(FRotator(0,-90,0)); F.Step(30);
 ADarkwellMovingPropLabRoom::FDividerMaskDiagnostics North;
 TestTrue(TEXT("North wall diagnostic"),F.Room->GetDividerMaskDiagnosticsForTesting(LabId,North));
 TestTrue(TEXT("Occlusion boundary follows the other side of the wall"),North.PhysicalOcclusionGate!=South.PhysicalOcclusionGate);
 F.Player->SetActorRotation(FRotator(0,90,0)); F.Step(30);
 TBitArray<> Capture,Frozen;
 TestTrue(TEXT("Wall case seals Whole history"),F.Room->GetNewestCaptureMasksForTesting(LabId,Capture,Frozen));
 ADarkwellMovingPropLabRoom::FDividerMaskDiagnostics Historical;
 TestTrue(TEXT("Wall history diagnostic"),F.Room->GetDividerMaskDiagnosticsForTesting(LabId,Historical));
 TestTrue(TEXT("Wall outline never persists in Whole capture"),Capture==Frozen && Frozen==Historical.FullGeometryMask);
 TestEqual(TEXT("Wall outline never generates a Whole cap"),F.Room->GetVisibleHistoricalCapCountForTesting(LabId),0);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellSpatialPartialKeepsViewEdgeCut,
 "Darkwell.PropLab.ConfirmedWholeViewEdge.SpatialPartialKeepsViewEdgeCut",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellSpatialPartialKeepsViewEdgeCut::RunTest(const FString&)
{
 using namespace Darkwell::ConfirmedWholeViewEdgeTests;
 FRoom F;
 F.Room->ResetTrackedRevealPolicyForLab(LabId,ESightWeaveRevealMode::SpatialPartial,100,ESightWeaveHistoryMode::StationaryOnly);
 F.Face(146); F.Step(); F.Face(-90); F.Step(30);
 TBitArray<> Capture,Frozen;
 TestTrue(TEXT("SpatialPartial freezes a legal observed mask"),F.Room->GetNewestCaptureMasksForTesting(LabId,Capture,Frozen));
 TestTrue(TEXT("SpatialPartial FrozenHistoryMask remains LastLegalCaptureMask"),Capture==Frozen);
 TestTrue(TEXT("SpatialPartial keeps unseen samples empty"),Capture.CountSetBits()>0 && Capture.CountSetBits()<Capture.Num());
 TestTrue(TEXT("SpatialPartial legal cut keeps its cap"),F.Room->GetVisibleHistoricalCapCountForTesting(LabId)>0);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellUnconfirmedWholeStillUsesLocalObservation,
 "Darkwell.PropLab.ConfirmedWholeViewEdge.UnconfirmedWholeStillUsesLocalObservation",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellUnconfirmedWholeStillUsesLocalObservation::RunTest(const FString&)
{
 using namespace Darkwell::ConfirmedWholeViewEdgeTests;
 FRoom F;
 F.Room->ResetTrackedRevealPolicyForLab(LabId,ESightWeaveRevealMode::WholeObjectAfterSpan,100,ESightWeaveHistoryMode::StationaryOnly);
 F.Face(146); F.Step();
 TestFalse(TEXT("Short local contact remains unconfirmed"),F.Room->IsRevealConfirmedForTesting(LabId));
 TestTrue(TEXT("Unconfirmed Whole uses partial legal current"),F.Room->GetLastLegalCoverageRatioForTesting(LabId)>0 && F.Room->GetLastLegalCoverageRatioForTesting(LabId)<1);
 TestTrue(TEXT("Unconfirmed Whole is not promoted to uniform Whole presentation"),!F.Room->IsWholePresentationUniformForTesting(LabId));
 F.Face(-90); F.Step(30);
 TestEqual(TEXT("Unconfirmed Whole creates no history"),F.Room->GetStaleEpochCountForTesting(LabId),0);
 TestEqual(TEXT("Unconfirmed Whole creates no proxy or cap resources"),F.Room->GetHistoricalPresentationResourceCountForTesting(LabId),0);
 TestEqual(TEXT("Unconfirmed Whole cap count remains zero"),F.Room->GetVisibleHistoricalCapCountForTesting(LabId),0);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellMultiPrimitiveWholeMask,
 "Darkwell.PropLab.ConfirmedWholeViewEdge.MultiPrimitiveWholeMask",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellMultiPrimitiveWholeMask::RunTest(const FString&)
{
 FDarkwellCurrentLiveGrid Grid;
 auto Descriptor=[](const uint64 Key,const FVector2D HalfExtent,const float X)
 {
  FDarkwellCurrentLiveGrid::FDescriptor Result;
  Result.PrimitiveKey=Key; Result.MeshKey=100+Key;
  Result.LocalBounds=FBox(FVector(-HalfExtent.X,-HalfExtent.Y,0),FVector(HalfExtent.X,HalfExtent.Y,100));
  Result.RelativeTransform=FTransform(FVector(X,0,0));
  return Result;
 };
 const TArray Descriptors{Descriptor(1,FVector2D(20,20),-40),Descriptor(2,FVector2D(10,15),15),Descriptor(3,FVector2D(3,3),35)};
 Grid.ResetGeometry(TEXT("Test.ConfirmedWhole.MultiPrimitive"),Descriptors,FTransform::Identity);
 const FBox2D Bounds(FVector2D(-70,-30),FVector2D(50,30));
 const FIntPoint Size(120,60);
 TBitArray<> Full;
 TestTrue(TEXT("Multi-primitive FullGeometryMask builds"),Grid.BuildFullGeometryMask(Bounds,Size,Full));
 auto IsSet=[&](const FVector2D Point)
 {
  const FVector2D UV=(Point-Bounds.Min)/Bounds.GetSize();
  const int32 X=FMath::Clamp(FMath::FloorToInt(UV.X*Size.X),0,Size.X-1);
  const int32 Y=FMath::Clamp(FMath::FloorToInt(UV.Y*Size.Y),0,Size.Y-1);
  return Full[Y*Size.X+X];
 };
 TestTrue(TEXT("Body primitive belongs to FullGeometryMask"),IsSet(FVector2D(-40,0)));
 TestTrue(TEXT("Door primitive belongs to FullGeometryMask"),IsSet(FVector2D(15,0)));
 TestTrue(TEXT("Handle primitive belongs to FullGeometryMask"),IsSet(FVector2D(35,0)));
 TestFalse(TEXT("Real gap between body and door is not filled"),IsSet(FVector2D(-5,0)));
 TestFalse(TEXT("Actor AABB corner is not filled"),IsSet(FVector2D(-69,29)));
 TestTrue(TEXT("FullGeometryMask is not Actor AABB fill"),Full.CountSetBits()>0 && Full.CountSetBits()<Full.Num());
 FDarkwellSpatialPropMemory Snapshot;
 Snapshot.Initialize(TEXT("Test.ConfirmedWhole.MultiPrimitive"),Bounds,2.5f); Snapshot.BeginPresent();
 TArray<float> Raw; Raw.Init(0,Snapshot.GetCells().Num()); for(int32 Index=0;Index<Raw.Num();Index+=2) Raw[Index]=1;
 Grid.AdvanceWholeWithOcclusion(.2f,FTransform::Identity,Snapshot,Bounds,Raw,[](const FVector2D Point){return Point.X<15?1.f:0.f;});
 bool bSawVisible=false,bSawBlocked=false;
 for(const auto& Part:Grid.Parts) for(const auto& Cell:Part.Raster.GetCells()) { bSawVisible|=Cell.AppearanceBlend>0; bSawBlocked|=Cell.AppearanceBlend==0; }
 TestTrue(TEXT("Physical wall gate independently blocks only affected primitive regions"),bSawVisible && bSawBlocked);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellCurrentHistoricalContributorExclusivity,
 "Darkwell.PropLab.ConfirmedWholeViewEdge.CurrentHistoricalContributorExclusivity",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellCurrentHistoricalContributorExclusivity::RunTest(const FString&)
{
 using namespace Darkwell::ConfirmedWholeViewEdgeTests;
 FRoom F;
 F.Room->ResetTrackedRevealPolicyForLab(LabId,ESightWeaveRevealMode::WholeObjectAfterSpan,100,ESightWeaveHistoryMode::StationaryOnly);
 F.Face(90); F.Step(30); F.Face(-90); F.Step(30); F.Face(90); F.Step(30);
 F.Room->ForceContributionRefreshForTesting(LabId);
 TestEqual(TEXT("Current and stale surface never overlap"),F.Room->GetCurrent3DOverlapStaleSurfaceForTesting(LabId),0);
 TestEqual(TEXT("Current and stale cap never overlap"),F.Room->GetCurrent3DOverlapStaleCapForTesting(LabId),0);
 TestTrue(TEXT("Current + stale surface + stale cap is at most one"),F.Room->GetMax3DRenderOwnershipContributorsForTesting(LabId)<=1);
 TestEqual(TEXT("Confirmed Whole owns no stale cap contributor"),F.Room->GetVisibleHistoricalCapCountForTesting(LabId),0);
 ADarkwellMovingPropLabRoom::FDividerMaskDiagnostics Diagnostic;
 TestTrue(TEXT("Contributor diagnostic snapshot"),F.Room->GetDividerMaskDiagnosticsForTesting(LabId,Diagnostic));
 for(int32 Index=0;Index<Diagnostic.FinalCurrentContribution.Num();++Index)
  TestTrue(TEXT("Per-sample current/history contributor exclusivity"),int32(Diagnostic.FinalCurrentContribution[Index])+int32(Diagnostic.FinalHistoricalContribution[Index])<=1);
 return true;
}
#endif
