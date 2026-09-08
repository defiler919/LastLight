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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellCurrentGridDisplayBoundary,
 "Darkwell.CurrentGrid.DisplayBoundary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellCurrentGridDisplayBoundary::RunTest(const FString&)
{
 FDarkwellCurrentLiveGrid Grid;
 const FBox Box(FVector(-70,-30,0),FVector(70,30,110));
 const FTransform Pose(FRotator(0,37,0));
 TArray<FDarkwellCurrentLiveGrid::FDescriptor> Geometry{{1,1,Box,FTransform::Identity}};
 Grid.ResetGeometry(TEXT("DisplayBoundary"),Geometry,Pose);
 Grid.Advance(.2f,Pose,[](FVector2D P){return P.Y<0?1.f:0.f;});
 const auto World=Box.TransformBy(Pose); const FBox2D Bounds(FVector2D(World.Min),FVector2D(World.Max));
 FDarkwellSpatialPropMemory Knowledge,Display;
 Knowledge.Initialize(TEXT("DisplayBoundary"),Bounds); Knowledge.BeginPresent();
 Display.Initialize(TEXT("DisplayBoundary"),Bounds); Display.BeginPresent();
 Grid.WriteWorldSnapshot(Knowledge,Bounds);
 const auto Hash=Grid.StateHash();
 Grid.WritePresentationSnapshot(Display,Bounds);
 TestEqual(TEXT("Display extraction never mutates local knowledge"),Grid.StateHash(),Hash);
 const auto S=Knowledge.GetSize(); const auto Step=Bounds.GetSize()/FVector2D(S);
 int32 Extended=0;
 for(int32 I=0;I<Knowledge.GetCells().Num();++I)
 {
  const auto Point=Bounds.Min+Step*FVector2D(I%S.X+.5,I/S.X+.5);
  const auto Local=Pose.InverseTransformPosition(FVector(Point,55));
  const auto& A=Knowledge.GetCells()[I]; const auto& B=Display.GetCells()[I];
  if(Box.IsInside(Local)) TestEqual(TEXT("Real observation cuts retain exact discovery"),B.DiscoveredPresent,A.DiscoveredPresent);
  else Extended+=B.DiscoveredPresent>A.DiscoveredPresent;
 }
 TestTrue(TEXT("Only physical silhouette padding is extended"),Extended>0);
 TBitArray<> Capture(false,S.X*S.Y*16);
 for(int32 Y=0;Y<S.Y*4;++Y) for(int32 X=0;X<S.X*4;++X)
  Capture[Y*S.X*4+X]=Knowledge.GetCells()[(Y/4)*S.X+X/4].DiscoveredPresent>0;
 const auto Original=Capture;
 Grid.ExtendCaptureAtPhysicalEdges(Bounds,S,4,Capture);
 TestEqual(TEXT("Capture repair preserves local authority"),Grid.StateHash(),Hash);
 TestTrue(TEXT("Proven rotated edge samples survive capture"),Capture.CountSetBits()>Original.CountSetBits());
 for(int32 I=0;I<Capture.Num();++I)
 {
  const int32 X=(I%(S.X*4))/4,Y=(I/(S.X*4))/4;
  const auto Center=Bounds.Min+Step*FVector2D(X+.5,Y+.5);
  if(Box.IsInside(Pose.InverseTransformPosition(FVector(Center,55))))
   TestEqual(TEXT("Interior capture bits, including true cuts, are unchanged"),Capture[I],Original[I]);
 }
 // A rotated thin part's AABB overlaps another, physically separate unknown
 // part. Padding must never become that other part's observation evidence.
 FDarkwellCurrentLiveGrid Multi;
 TArray<FDarkwellCurrentLiveGrid::FDescriptor> MultiGeometry{
  {1,1,FBox(FVector(-70,-5,0),FVector(70,5,50)),Pose},
  {2,1,FBox(FVector(-2,-2,0),FVector(2,2,50)),FTransform(FVector(-40,30,0))}};
 Multi.ResetGeometry(TEXT("SeparateParts"),MultiGeometry,FTransform::Identity);
 Multi.Advance(.2f,FTransform::Identity,[](FVector2D P){return P.Y<0?1.f:0.f;});
 const FBox2D MultiBounds(FVector2D(-80,-60),FVector2D(80,60));
 const FIntPoint MultiSize(64,48); TBitArray<> EdgeOnly(false,64*48*16);
 Multi.ExtendCaptureAtPhysicalEdges(MultiBounds,MultiSize,4,EdgeOnly);
 const FVector2D UV=(FVector2D(-40,30)-MultiBounds.Min)/MultiBounds.GetSize();
 TestFalse(TEXT("AABB overlap cannot reveal a separate unknown primitive"),EdgeOnly[FMath::FloorToInt(UV.Y*192)*256+FMath::FloorToInt(UV.X*256)]);
 return true;
}
#endif
