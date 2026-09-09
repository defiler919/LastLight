#include "Misc/AutomationTest.h"
#include "VisionPresentation/DarkwellStaticKnowledge.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
 FDarkwellFogVisualSourceSnapshot Source(FVector2D Origin,float Angle=0)
 {
  FDarkwellFogVisualSourceSnapshot S;S.BodyCenter=S.ConeOrigin=Origin;S.BodyRadiusCentimeters=8;
  S.ConeForward=FVector2D(FMath::Cos(FMath::DegreesToRadians(Angle)),FMath::Sin(FMath::DegreesToRadians(Angle)));
  S.ConeRangeCentimeters=180;S.ConeHalfAngleDegrees=35;S.bConeLegallyLive=true;S.AuthorityRevision=1;return S;
 }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStaticKnowledgeContract,"Darkwell.SightWeave.StaticKnowledge.UnknownClearBlock",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStaticKnowledgeContract::RunTest(const FString&)
{
 FDarkwellStaticKnowledge K;const FBox2D Area({-80,-80},{160,80});K.Declare(Area);
 TestEqual(TEXT("Unknown allocates no knowledge tiles"),K.GetTiles().Num(),0);
 auto S=Source({0,0});K.Observe(S,{},Area);const FVector2D P(20.3125,.3125);
 TestTrue(TEXT("legal observation writes"),K.HasMemory(P));
 auto Away=Source({-1000,0},180);K.Observe(Away,{},Area);TestTrue(TEXT("leaving retains memory"),K.HasMemory(P));
 const FBox2D R({10,-10},{40,10});K.SetBlock(R,true);K.Clear(R);
 TestFalse(TEXT("clear is immediately unknown"),K.HasMemory(P));K.Observe(S,{},Area);
 TestFalse(TEXT("blocked live cannot write"),K.HasMemory(P));
 TestTrue(TEXT("P4 remains legally live"),FDarkwellContinuousVisibilityBuilder::QuerySourceCoverage(S,P,{}).Coverage>=.99f);
 K.SetBlock(R,false);TestFalse(TEXT("release never restores"),K.HasMemory(P));
 K.Observe(S,{},Area);TestTrue(TEXT("new legal observation rebuilds"),K.HasMemory(P));
 for(int I=0;I<20;++I){K.SetBlock(R,true);K.SetBlock(R,true);K.Clear(R);K.Observe(S,{},Area);TestFalse(TEXT("repeat block"),K.HasMemory(P));K.SetBlock(R,false);K.SetBlock(R,false);TestFalse(TEXT("repeat release"),K.HasMemory(P));K.Observe(S,{},Area);}
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStaticKnowledgeOracle,"Darkwell.SightWeave.StaticKnowledge.RotatedOcclusionOracle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStaticKnowledgeOracle::RunTest(const FString&)
{
 for(float Angle:{0.f,37.f,90.f})
 {
  FDarkwellStaticKnowledge K;const FBox2D Area({-10,-30},{60,30});K.Declare(Area);auto S=Source({-10,0},Angle);
  const FVector2D D(FMath::Cos(FMath::DegreesToRadians(Angle)),FMath::Sin(FMath::DegreesToRadians(Angle)));
  TArray<FDarkwellFogVisualSegment> Walls;Walls.Add({FVector2D(25,0)-D*20,FVector2D(25,0)+D*20});
  K.Observe(S,Walls,Area);
  int Mismatches=0;
  for(int Y=-40;Y<40;++Y)for(int X=-12;X<88;++X)
  {
   const auto Min=FVector2D(X,Y)*K.SampleCm;bool Expected=true;
   for(auto O:{FVector2D(0),FVector2D(1,0),FVector2D(0,1),FVector2D(1),FVector2D(.5)})
    Expected &= FDarkwellContinuousVisibilityBuilder::QuerySourceCoverage(S,Min+O*K.SampleCm,Walls).Coverage>=.99f;
   Mismatches+=Expected!=K.HasMemory(Min+FVector2D(K.SampleCm*.5));
  }
  TestEqual(FString::Printf(TEXT("%.0f degrees: exact corner+center oracle"),Angle),Mismatches,0);
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStaticKnowledgeBoundary,"Darkwell.SightWeave.StaticKnowledge.TileEdgesSparseAndDirty",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStaticKnowledgeBoundary::RunTest(const FString&)
{
 FDarkwellStaticKnowledge K;const FBox2D Area({-160,-160},{160,160});K.Declare(Area);K.Declare(FBox2D({100000,100000},{100080,100080}));
 auto S=Source({0,0});S.BodyRadiusCentimeters=1000;K.Observe(S,{},Area);
 TestFalse(TEXT("far declared geometry is not preknown"),K.HasMemory({100001,100001}));
 const FBox2D Cut({-80,-80},{80,80});K.Clear(Cut);
 for(auto P:{FVector2D(-80.3125,0.3125),FVector2D(80.3125,0.3125),FVector2D(.3125,-80.3125),FVector2D(.3125,80.3125)})TestTrue(TEXT("outside four edges retained"),K.HasMemory(P));
 for(auto P:{FVector2D(-79.6875,-79.6875),FVector2D(79.6875,-79.6875),FVector2D(-79.6875,79.6875),FVector2D(79.6875,79.6875)})TestFalse(TEXT("inside four corners cleared"),K.HasMemory(P));
 K.Observe(S,{},Area);
 for(auto P:{FVector2D(-80.3125,-80.3125),FVector2D(-79.6875,-79.6875),FVector2D(79.6875,79.6875),FVector2D(80.3125,80.3125)})TestTrue(TEXT("reobserve tile corners without seam"),K.HasMemory(P));
 K.Observe(S,{},Area);TestEqual(TEXT("fully known tiles do not resample"),K.Stats.TestedSamples,uint64(0));
 return true;
}
#endif
