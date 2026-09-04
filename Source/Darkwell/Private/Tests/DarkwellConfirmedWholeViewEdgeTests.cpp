#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "VisionPresentation/DarkwellCurrentLiveGrid.h"

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
 return true;
}
#endif
