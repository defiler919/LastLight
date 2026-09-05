#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "VisionPresentation/DarkwellCurrentLiveGrid.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellUniformCoverageProofTest,"Darkwell.FogVisual.CanonicalCoverage.UniformProofMatchesOriginalOracle",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellUniformCoverageProofTest::RunTest(const FString&)
{
 FDarkwellFogVisualSourceSnapshot S; S.BodyCenter=FVector2D(0); S.ConeOrigin=FVector2D(3,-5);
 S.BodyRadiusCentimeters=100; S.ConeRangeCentimeters=1200; S.ConeHalfAngleDegrees=40; S.bConeLegallyLive=true; S.AuthorityRevision=1;
 const TArray<FDarkwellFogVisualSegment> Walls{{FVector2D(200,-200),FVector2D(200,30)},{FVector2D(200,45),FVector2D(200,200)},
  {FVector2D(-150,20),FVector2D(-50,120)}};
 int32 Positive=0,Negative=0,Boundary=0;
 for(int32 Angle=0;Angle<360;Angle+=17)
 {
  S.ConeForward=FVector2D(FMath::Cos(FMath::DegreesToRadians(float(Angle))),FMath::Sin(FMath::DegreesToRadians(float(Angle))));
  for(int32 Y=-600;Y<=600;Y+=83) for(int32 X=-600;X<=600;X+=79)
  {
   const FBox2D B(FVector2D(X,Y),FVector2D(X+63.37,Y+47.11)); float V;
   if(!FDarkwellContinuousVisibilityBuilder::TryUniformCoverage(S,B,Walls,V)) { ++Boundary; continue; }
   V>0?++Positive:++Negative;
   for(int32 J=0;J<=6;++J) for(int32 I=0;I<=6;++I)
   {
    const auto P=B.Min+B.GetSize()*FVector2D(I/6.,J/6.);
    const auto Q=FDarkwellContinuousVisibilityBuilder::QuerySourceCoverage(S,P,Walls);
    if(!TestEqual(TEXT("Every claimed uniform tile matches unchanged analytic oracle"),V,Q.Coverage)) return false;
   }
  }
 }
 TestTrue(TEXT("Positive uniform proofs exercised"),Positive>0);
 TestTrue(TEXT("Negative uniform proofs exercised"),Negative>0);
 TestTrue(TEXT("Boundary and wall tiles retain exact sampling"),Boundary>0);
 const FBox2D Behind(FVector2D(300,-60),FVector2D(400,60));
 TestFalse(TEXT("Short wall inside ray fan rejects complete visibility despite clear outer rays"),
  FDarkwellContinuousVisibilityBuilder::IsOcclusionFree(FVector2D(0),Behind,TArray<FDarkwellFogVisualSegment>{{FVector2D(200,-2),FVector2D(200,2)}}));
 AddInfo(FString::Printf(TEXT("positive=%d negative=%d boundary=%d"),Positive,Negative,Boundary)); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellCurrentTileCoverageTest,"Darkwell.FogVisual.CanonicalCoverage.CurrentTilesMatchPointOracle",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellCurrentTileCoverageTest::RunTest(const FString&)
{
 FDarkwellFogVisualSourceSnapshot S;
 S.BodyCenter=S.ConeOrigin=FVector2D(0); S.BodyRadiusCentimeters=70;
 S.ConeRangeCentimeters=1200; S.ConeHalfAngleDegrees=40; S.bConeLegallyLive=true; S.AuthorityRevision=1;
 const TArray<FDarkwellFogVisualSegment> Walls{{FVector2D(-120,310),FVector2D(-50,310)}};
 FDarkwellCurrentLiveGrid::FDescriptor D{1,1,FBox(FVector(-310,-37.5,0),FVector(310,37.5,115)),FTransform::Identity};
 uint64 ReferenceQueries=0, TileQueries=0;
 for(float PoseYaw:{0.f,31.f,113.f})
 {
  const FTransform Pose(FRotator(0,PoseYaw,0),FVector(0,650,0));
  FDarkwellCurrentLiveGrid PointGrid,TileGrid;
  PointGrid.ResetGeometry(TEXT("PlainObject"),MakeArrayView(&D,1),Pose);
  TileGrid.ResetGeometry(TEXT("PlainObject"),MakeArrayView(&D,1),Pose);
  for(int32 Angle=-90;Angle<270;Angle+=19)
  {
   S.ConeForward=FVector2D(FMath::Cos(FMath::DegreesToRadians(float(Angle))),FMath::Sin(FMath::DegreesToRadians(float(Angle))));
   auto Query=[&](FVector2D P){return FDarkwellContinuousVisibilityBuilder::QuerySourceCoverage(S,P,Walls).Coverage;};
   auto Uniform=[&](const FBox2D& B,float& V){return FDarkwellContinuousVisibilityBuilder::TryUniformCoverage(S,B,Walls,V);};
   PointGrid.Advance(1.f/60,Pose,Query); TileGrid.Advance(1.f/60,Pose,Query,Uniform);
   ReferenceQueries+=PointGrid.Queries; TileQueries+=TileGrid.Queries;
   if(!TestTrue(TEXT("Every local cell equals original five-point coverage"),PointGrid.Parts[0].Coverage==TileGrid.Parts[0].Coverage)) return false;
   if(!TestEqual(TEXT("Knowledge and blends remain identical"),PointGrid.StateHash(),TileGrid.StateHash())) return false;
  }
  const auto W=D.LocalBounds.TransformBy(Pose); const FBox2D B(FVector2D(W.Min),FVector2D(W.Max));
  TBitArray<> A,C;
  TileGrid.BuildFullGeometryMask(B,FIntPoint(256,128),A);
  TileGrid.BuildFullGeometryMask(B,FIntPoint(256,128),C);
  TestTrue(TEXT("Repeated geometry mask is exact"),A==C);
 }
 TestTrue(TEXT("Uniform interiors materially reduce point queries"),TileQueries*3<ReferenceQueries);
 AddInfo(FString::Printf(TEXT("current point queries=%llu tiled=%llu"),ReferenceQueries,TileQueries));
 return true;
}
#endif
