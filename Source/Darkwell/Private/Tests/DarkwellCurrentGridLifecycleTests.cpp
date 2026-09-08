#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "VisionPresentation/DarkwellCurrentLiveGrid.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellCurrentGridLifecycle,
 "Darkwell.CurrentGrid.Lifecycle", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellCurrentGridLifecycle::RunTest(const FString&)
{
 FDarkwellCurrentLiveGrid Grid;
 const FBox2D Bounds(FVector2D(-20),FVector2D(20));
 TArray<FDarkwellCurrentLiveGrid::FDescriptor> Geometry;
 Geometry.Add({1,1,FBox(FVector(-20,-20,0),FVector(20,20,80)),FTransform::Identity});
 Grid.ResetGeometry(TEXT("Lifecycle"),Geometry,FTransform::Identity);
 Grid.Advance(.2f,FTransform::Identity,[](FVector2D){return 1.f;});
 FDarkwellSpatialPropMemory Snapshot;
 Snapshot.Initialize(TEXT("Lifecycle"),Bounds); Snapshot.BeginPresent();
 TArray<float> Coverage; Coverage.Init(1.f,16*16);
 Grid.AdvanceConfirmedWhole(.2f,FTransform::Identity,Snapshot,Bounds,Coverage);
 const auto& Part=Grid.Parts[0];
 AddInfo(FString::Printf(TEXT("GRID_REPRO confirmed=1 cells=%d size=%dx%d capture=%d observation=%d"),
  Part.Local.GetCells().Num(),Part.Local.GetSize().X,Part.Local.GetSize().Y,
  Part.LastLegalCaptureMask.Num(),Part.CurrentLegalObservationMask.Num()));
 TestTrue(TEXT("Confirmed Whole uses the compact representation"),Grid.IsUniformWholePresentation());
 TestEqual(TEXT("Confirmed Whole releases dense observation work"),Part.CurrentLegalObservationMask.Num(),0);
 // Reentry explicitly transitions to dense state; geometry IDs and dimensions
 // survive the compact interval, and masks are rebuilt before any dense read.
 Grid.Advance(.2f,FTransform::Identity,[](FVector2D){return 0.f;});
 Grid.WriteWorldSnapshot(Snapshot,Bounds);
 TestEqual(TEXT("Capture mask shares local geometry lifetime"),Part.LastLegalCaptureMask.Num(),Part.Local.GetCells().Num());
 TestEqual(TEXT("Observation mask shares local geometry lifetime"),Part.CurrentLegalObservationMask.Num(),Part.Local.GetCells().Num());
 Grid.Advance(.2f,FTransform::Identity,[](FVector2D){return 1.f;});
 TestTrue(TEXT("Fully observed precondition"),Grid.bFullyObservedAtPose);
 Grid.ResetGeometry(TEXT("Lifecycle"),Geometry,FTransform::Identity);
 TestFalse(TEXT("Geometry replacement does not inherit full observation"),Grid.bFullyObservedAtPose);
 Grid.WriteWorldSnapshot(Snapshot,Bounds);
 for(const auto& C:Snapshot.GetCells()) TestEqual(TEXT("Reset geometry is Unknown"),C.DiscoveredPresent,0.f);
 return true;
}
#endif
