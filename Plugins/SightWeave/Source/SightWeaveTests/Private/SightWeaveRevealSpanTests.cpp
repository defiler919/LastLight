#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SightWeaveRevealObservation.h"

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FSightWeaveRevealSpanTest,"SightWeave.RevealPolicy",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FSightWeaveRevealSpanTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
 for(const TCHAR* N : {TEXT("MinimumSpanWorldUnitSemantics"),TEXT("MinimumSpanSmallObjectClamp"),
  TEXT("MinimumSpanFirstLegalContact"),TEXT("ContiguousSpanDoesNotBridgeGaps"),
  TEXT("TentativeSessionResetsOnRealViewLoss"),TEXT("InvalidCoverageDoesNotResetTentativeSession"),
  TEXT("ConfirmedSessionSurvivesLegalRigidMotion")}) { Names.Add(N); Commands.Add(N); }
}
bool FSightWeaveRevealSpanTest::RunTest(const FString& Case)
{
 FResolvedSightWeaveObjectPolicy Policy; Policy.RevealMode=ESightWeaveRevealMode::WholeObjectAfterSpan;
 FIntPoint Size(8,2); FVector2D Step(25,10); TBitArray<> Shape(true,16), Mask(false,16);
 FSightWeaveRevealObservation S;
 if(Case==TEXT("MinimumSpanSmallObjectClamp"))
 {
  Shape.Init(true,6); Mask.Init(false,6);
  TestTrue(TEXT("Physical footprint initialized"),S.Initialize(Policy,FIntPoint(3,2),FVector2D(10),Shape));
  TestEqual(TEXT("Small object clamps to longest continuous world span"),S.GetEffectiveMinimumSpanCm(),30.f);
  Mask[0]=true; S.Observe(true,Mask); TestFalse(TEXT("One small cell does not meet full clamped span"),S.IsConfirmed());
  Mask[1]=true; Mask[2]=true; S.Observe(true,Mask); TestTrue(TEXT("Small object can confirm"),S.IsConfirmed()); return true;
 }
 if(Case==TEXT("MinimumSpanFirstLegalContact"))
 {
  Policy.MinimumObservedSpanCm=0; Shape[0]=false; S.Initialize(Policy,Size,Step,Shape);
  S.Observe(true,Mask); TestFalse(TEXT("Zero never means unconditional reveal"),S.IsConfirmed());
  Mask[0]=true; S.Observe(true,Mask); TestFalse(TEXT("Outside footprint cannot confirm"),S.IsConfirmed());
  Mask[1]=true; S.Observe(false,Mask); TestFalse(TEXT("Invalid authority cannot confirm"),S.IsConfirmed());
  S.Observe(true,Mask); TestTrue(TEXT("First real legal sample confirms"),S.IsConfirmed()); return true;
 }
 if(Case==TEXT("ContiguousSpanDoesNotBridgeGaps"))
 {
  Size=FIntPoint(10,1); Step=FVector2D(10); Shape.Init(true,10); Mask.Init(false,10);
  Mask[0]=true; Mask[9]=true;
  TestEqual(TEXT("Distant endpoints are two ten-centimeter runs"),S.LongestContinuousSpan(Size,Step,Shape,Mask),10.f);
  Mask.Init(true,10); Shape[4]=false; Shape[5]=false;
  TestEqual(TEXT("Footprint holes break a run even when input bits claim them"),S.LongestContinuousSpan(Size,Step,Shape,Mask),40.f);
  TestEqual(TEXT("Columns also respect physical steps"),S.LongestContinuousSpan(FIntPoint(1,10),FVector2D(5,7),Shape,Mask),28.f);
  return true;
 }
 S.Initialize(Policy,Size,Step,Shape); Mask[0]=true; Mask[1]=true; Mask[2]=true; S.Observe(true,Mask);
 TestEqual(TEXT("Three contiguous 25 cm samples measure 75 cm"),S.GetObservedSpanCm(),75.f);
 TestFalse(TEXT("Below 100 cm"),S.IsConfirmed());
 if(Case==TEXT("MinimumSpanWorldUnitSemantics"))
 {
  FSightWeaveRevealObservation SmallScale; SmallScale.Initialize(Policy,Size,FVector2D(10),Shape); SmallScale.Observe(true,Mask);
  TestEqual(TEXT("Same bits with ten-centimeter cells measure 30 cm"),SmallScale.GetObservedSpanCm(),30.f);
  Mask[3]=true; S.Observe(true,Mask); TestTrue(TEXT("Exactly 100 cm confirms without exposure timer"),S.IsConfirmed()); return true;
 }
 TBitArray<> Empty(false,16), Final(false,16); Final[3]=true;
 if(Case==TEXT("TentativeSessionResetsOnRealViewLoss"))
 {
  S.Observe(true,Empty); TestEqual(TEXT("Valid view loss clears progress"),S.GetObservedSpanCm(),0.f);
  S.Observe(true,Final); TestFalse(TEXT("Fresh observation cannot borrow old session"),S.IsConfirmed());
  TestEqual(TEXT("Fresh session contains one cell"),S.GetTentativeMask().CountSetBits(),1); return true;
 }
 if(Case==TEXT("InvalidCoverageDoesNotResetTentativeSession"))
 {
  S.Observe(false,Empty); TestEqual(TEXT("Invalid revision retains legitimate 75 cm"),S.GetObservedSpanCm(),75.f);
  S.Observe(true,Final); TestTrue(TEXT("Next valid contact completes the same session"),S.IsConfirmed()); return true;
 }
 S.Observe(true,Final); TestTrue(TEXT("Confirmed"),S.IsConfirmed());
 const uint64 Evaluations=S.GetSpanEvaluations();
 for(int32 Pose=0;Pose<240;++Pose) S.Observe(Pose%3!=0,Pose%3==0?Empty:Final);
 TestTrue(TEXT("Legal contact and invalid publication preserve this session"),S.IsConfirmed());
 TestEqual(TEXT("Confirmed path performs no more span scans"),S.GetSpanEvaluations(),Evaluations);
 TestEqual(TEXT("Tentative storage released after confirmation"),S.GetTentativeMask().Num(),0);
 for(int32 Session=0;Session<50;++Session)
 {
  S.EndSession();
  TestFalse(TEXT("Proven loss ends qualification after host handoff"),S.IsConfirmed());
  TestTrue(TEXT("Session end preserves geometry"),S.IsInitialized());
  TestEqual(TEXT("Every session uses the same configured span"),S.GetEffectiveMinimumSpanCm(),100.f);
  S.Observe(true,Mask); TestFalse(TEXT("Prior confirmation cannot promote 75 cm"),S.IsConfirmed());
  S.Observe(false,Empty); TestEqual(TEXT("Invalid publication preserves this session's span"),S.GetObservedSpanCm(),75.f);
  S.Observe(true,Final); TestTrue(TEXT("This session's contiguous 100 cm qualifies again"),S.IsConfirmed());
 }
 S.Initialize(Policy,Size,Step,Shape); TestFalse(TEXT("Explicit re-registration/topology reset clears confirmation"),S.IsConfirmed());
 return true;
}
#endif
