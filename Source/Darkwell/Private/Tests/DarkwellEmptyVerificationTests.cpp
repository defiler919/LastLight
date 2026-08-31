#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "VisionPresentation/DarkwellEmptyVerification.h"
#include "VisionPresentation/DarkwellStalePropLabComponent.h"
#include "VisionPresentation/DarkwellRememberedPropSubsystem.h"
#include "VisionPresentation/DarkwellMode2SolidComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellMode2RevealRampTest,"Darkwell.PropLab.Mode2Solid.SpatialRampAndLegalGate",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellMode2RevealRampTest::RunTest(const FString&)
{
 using Solid=UDarkwellMode2SolidComponent;
 TestEqual(TEXT("No legal view means no geometry alpha"),Solid::AdvanceReveal(0,0,10),0.f);
 TestEqual(TEXT("Object identity .50 cannot open spatial surface"),Solid::AdvanceReveal(0,.5f,10),0.f);
 TestEqual(TEXT("Soft outer fringe is not direct observation"),Solid::AdvanceReveal(0,.98f,10),0.f);
 TestTrue(TEXT("First legal frame begins fade immediately"),Solid::AdvanceReveal(0,1,1.f/60)>0);
 TestTrue(TEXT("Halfway at .10 seconds"),FMath::IsNearlyEqual(Solid::AdvanceReveal(0,1,.1f),.5f));
 TestEqual(TEXT("Complete at .20 seconds"),Solid::AdvanceReveal(0,1,.2f),1.f);
 TestEqual(TEXT("Occlusion immediately removes live pixels, no fade through wall"),Solid::AdvanceReveal(1,0,.001f),0.f);
 float Left=0,Right=0;
 for(int32 I=0;I<30;++I) { Left=Solid::AdvanceReveal(Left,1,1.f/60); Right=Solid::AdvanceReveal(Right,0,1.f/60); }
 TestTrue(TEXT("Scan can reveal one side fully without exposing other side"),Left==1 && Right==0);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellMode2ContourTest,"Darkwell.PropLab.Mode2Solid.ContourDoesNotWriteEvidence",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellMode2ContourTest::RunTest(const FString&)
{
 FDarkwellEmptyVerification E; E.Initialize(FBox2D(FVector2D(0,0),FVector2D(20,20)));
 TArray<float> Opacity={0,1,0,1};
 for(double Y=0;Y<=20;Y+=.25)
 {
  TestTrue(TEXT("Smooth cut sits on the shared boundary, no cap over empty half"),FMath::IsNearlyEqual(UDarkwellMode2SolidComponent::SampleOpacity(E,Opacity,FVector2D(10,Y)),.5f));
  TestTrue(TEXT("Empty side remains outside closed volume"),UDarkwellMode2SolidComponent::SampleOpacity(E,Opacity,FVector2D(9,Y))<.5f);
 }
 TestEqual(TEXT("Visual reconstruction creates no authoritative evidence"),E.VerifiedFraction(),0.f);
 TestFalse(TEXT("Visual reconstruction cannot finish a snapshot"),E.IsObjectEmpty());
 FDarkwellEmptyVerification Surround; Surround.Initialize(FBox2D(FVector2D(0,0),FVector2D(50,50)));
 TArray<float> Ring; Ring.Init(1,25); Ring[12]=0;
 for(double Y=20;Y<=30;Y+=.25) for(double X=20;X<=30;X+=.25)
  TestTrue(TEXT("Even eight retained neighbours cannot place black volume in a fully empty cell"),
   UDarkwellMode2SolidComponent::SampleOpacity(Surround,Ring,FVector2D(X,Y))<.8f);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellEmptyEvidenceTest,"Darkwell.PropLab.Stale.IndependentLegalEvidence",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellEmptyEvidenceTest::RunTest(const FString&)
{
 FDarkwellEmptyVerification Grid; Grid.Initialize(FBox2D(FVector2D(0,0),FVector2D(100,20)));
 auto Empty=[](const FBox2D&){return false;};
 // Existing .50 live-enter threshold cannot verify an empty cell, even indefinitely.
 for(int32 I=0;I<100;++I) Grid.Observe(.03333334f,I/30.f,[](FVector2D){return .5f;},Empty);
 TestEqual(TEXT("Presentation enter is not empty authority"),Grid.VerifiedFraction(),0.f);
 Grid.Observe(4,5,[](FVector2D){return 1.f;},Empty);
 TestEqual(TEXT("One stalled frame is insufficient"),Grid.VerifiedFraction(),0.f);
 for(int32 I=0;I<10;++I) Grid.Observe(1.f/30,6+I/30.f,[](FVector2D P){return P.X<=30?1.f:0.f;},Empty);
 TestTrue(TEXT("Edge evidence does not clear whole object"),Grid.VerifiedFraction()>0 && !Grid.IsObjectEmpty());
 const float Partial=Grid.VerifiedFraction();
 for(int32 I=0;I<30;++I) Grid.Observe(1.f/30,7+I/30.f,[](FVector2D){return 0.f;},Empty);
 TestEqual(TEXT("Looking away never resurrects erased cells"),Grid.VerifiedFraction(),Partial);
 for(int32 I=0;I<10;++I) Grid.Observe(1.f/30,8+I/30.f,[](FVector2D){return 1.f;},[](const FBox2D& Box){return Box.Min.X>=50;});
 TestFalse(TEXT("Legal view of occupied cells is not empty evidence"),Grid.IsObjectEmpty());
 TestEqual(TEXT("Other identity occupying half the footprint blocks confirmation"),Grid.VerifiedFraction(),.5f);
 for(int32 I=0;I<10;++I) Grid.Observe(1.f/30,9+I/30.f,[](FVector2D){return 1.f;},Empty);
 TestTrue(TEXT("All cells independently confirmed allows whole erasure"),Grid.IsObjectEmpty());
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellStaleModesTest,"Darkwell.PropLab.Stale.ModesShareMonotonicAuthority",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellStaleModesTest::RunTest(const FString&)
{
 FDarkwellEmptyVerification Grid; Grid.Initialize(FBox2D(FVector2D(0,0),FVector2D(100,20)));
 for(int32 I=0;I<3;++I) Grid.Observe(1.f/30,(I+1)/30.f,[](FVector2D P){return P.X<=50?1.f:0.f;},[](const FBox2D&){return false;});
 TestEqual(TEXT("Half-footprint evidence"),Grid.VerifiedFraction(),.5f);
 TestEqual(TEXT("Mode0 still shows whole ghost"),Grid.Opacity(0,0,.2f),1.f);
 TestEqual(TEXT("Mode1 erases verified cell immediately"),Grid.Opacity(0,1,.1f),0.f);
 TestEqual(TEXT("Mode1 retains unobserved cell"),Grid.Opacity(9,1,.4f),1.f);
 TestTrue(TEXT("Mode2 takes .20s after SAME evidence"),FMath::IsNearlyEqual(Grid.Opacity(0,2,.2f),.5f));
 TestEqual(TEXT("Mode2 fully erased after .20s"),Grid.Opacity(0,2,.31f),0.f);
 TestEqual(TEXT("Mode2 unobserved portion is untouched"),Grid.Opacity(9,2,99),1.f);
 for(int32 Mode=0;Mode<3;++Mode) for(int32 I=0;I<100;++I) Grid.Opacity(I%Grid.Cells.Num(),Mode,99);
 TestEqual(TEXT("Rendering never changes authority"),Grid.VerifiedFraction(),.5f);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellStaleOcclusionTest,"Darkwell.PropLab.Stale.OcclusionAndBoundaryJitter",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellStaleOcclusionTest::RunTest(const FString&)
{
 FDarkwellEmptyVerification Grid; Grid.Initialize(FBox2D(FVector2D(0,0),FVector2D(10,10)));
 for(int32 I=0;I<30;++I) Grid.Observe(1.f/30,I/30.f,[](FVector2D P){return P.Equals(FVector2D(10,10))?0.f:1.f;},[](const FBox2D&){return false;});
 TestEqual(TEXT("A single occluded support point retains the cell"),Grid.VerifiedFraction(),0.f);
 for(int32 I=0;I<60;++I) Grid.Observe(1.f/30,1+I/30.f,[I](FVector2D){return I%2?1.f:.98f;},[](const FBox2D&){return false;});
 TestEqual(TEXT("Boundary jitter cannot accumulate interrupted dwell"),Grid.VerifiedFraction(),0.f);
 for(int32 I=0;I<3;++I) Grid.Observe(1.f/30,3+I/30.f,[](FVector2D){return 1.f;},[](const FBox2D&){return false;});
 TestTrue(TEXT("Stable evidence latches"),Grid.IsObjectEmpty());
 Grid.Observe(10,20,[](FVector2D){return 0.f;},[](const FBox2D&){return true;});
 TestTrue(TEXT("Later darkness/occupancy does not resurrect old ghost"),Grid.IsObjectEmpty());
 for(float T=10;T<20;T+=.1f)
  TestTrue(TEXT("D reverses C without changing path extent"),FMath::IsNearlyEqual(UDarkwellStalePropLabComponent::ScanYaw(T,2)+UDarkwellStalePropLabComponent::ScanYaw(T,3),180.f));
 TestEqual(TEXT("Pause at middle"),UDarkwellStalePropLabComponent::ScanYaw(21,2),90.f);
 return true;
}
#endif
