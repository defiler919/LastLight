#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "VisionPresentation/DarkwellRememberedPropSubsystem.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "HAL/IConsoleManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellPropComparisonRouteTest,
 "Darkwell.PropLab.Comparison.FixedCameraTrajectory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDarkwellPropComparisonRouteTest::RunTest(const FString& Parameters)
{
 for(int32 Mode=0;Mode<3;++Mode)
 {
  auto* CVar=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Darkwell.ProjectFogVisual.PropPresentationMode"));
  CVar->Set(Mode,ECVF_SetByConsole);
  TestEqual(TEXT("Gray start yaw"),Darkwell::PropLab::ComparisonYaw(0),-30.f);
  TestEqual(TEXT("Initial hold"),Darkwell::PropLab::ComparisonYaw(2),-30.f);
  TestEqual(TEXT("Middle hold starts at 8s"),Darkwell::PropLab::ComparisonYaw(8),40.f);
  TestEqual(TEXT("Middle hold lasts 2s"),Darkwell::PropLab::ComparisonYaw(10),40.f);
  TestEqual(TEXT("Whole forward traversal is 12s plus hold"),Darkwell::PropLab::ComparisonYaw(16),110.f);
  TestEqual(TEXT("Reverse finishes at 28s"),Darkwell::PropLab::ComparisonYaw(28),-30.f);
  TestEqual(TEXT("30s route ends gray"),Darkwell::PropLab::ComparisonYaw(30),-30.f);
  for(float T=2.1f;T<7.8f;T+=.1f)
   TestTrue(TEXT("Forward angular speed constant"),FMath::IsNearlyEqual(Darkwell::PropLab::ComparisonYaw(T+.1f)-Darkwell::PropLab::ComparisonYaw(T),70.f/60,.0001f));
  for(float T=16.1f;T<27.8f;T+=.1f)
   TestTrue(TEXT("Reverse angular speed matches"),FMath::IsNearlyEqual(Darkwell::PropLab::ComparisonYaw(T+.1f)-Darkwell::PropLab::ComparisonYaw(T),-70.f/60,.0001f));
 }
 IConsoleManager::Get().FindConsoleVariable(TEXT("r.Darkwell.ProjectFogVisual.PropPresentationMode"))->Set(0,ECVF_SetByConsole);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellPropLabSpatialEvidenceRuleTest,
 "Darkwell.PropLab.MovingRules.SpatialEvidenceOnlyState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDarkwellPropLabSpatialEvidenceRuleTest::RunTest(const FString& Parameters)
{
 const FTransform A(FVector(0,0,0)), B(FVector(500,0,0));
 for(int32 Mode=0;Mode<3;++Mode)
 {
  IConsoleManager::Get().FindConsoleVariable(TEXT("r.Darkwell.ProjectFogVisual.PropPresentationMode"))->Set(Mode,ECVF_SetByConsole);
  FDarkwellRememberedPropState First; First.Initialize(A,1);
  auto D=First.Observe(true,B,0,0,1,true);
  TestTrue(TEXT("Unseen relocation preserves A and hides B"),D.bShowProxy && !D.bShowCurrent && First.SnapshotTransform.Equals(A));
  D=First.Observe(true,B,0,1,1,true);
  TestFalse(TEXT("Checking empty A invalidates its memory"),D.bSnapshotValid);
  D=First.Observe(true,B,1,0,1,true);
  TestTrue(TEXT("Then seeing B creates current snapshot only"),D.bShowCurrent && !D.bRetainPreviousSnapshot && First.SnapshotTransform.Equals(B));
  FDarkwellRememberedPropState Second; Second.Initialize(A,1);
  D=Second.Observe(true,B,1,0,1,true);
  TestTrue(TEXT("Seeing B first always retains A as separate spatial knowledge"),D.bRetainPreviousSnapshot);
  TestTrue(TEXT("B updates current observation without invalidating A"),Second.SnapshotTransform.Equals(B));
  FDarkwellRememberedPropState Twin; Twin.Initialize(A,1);
  Twin.Observe(true,B,0,0,1,true);
  TestTrue(TEXT("Recognizing another identical-looking ID does not change twin memory"),Twin.SnapshotTransform.Equals(A));
  D=Twin.Observe(false,FTransform::Identity,0,0,1,true);
  TestTrue(TEXT("Unseen destruction retains memory"),D.bShowProxy);
  D=Twin.Observe(false,FTransform::Identity,0,1,1,true);
  TestFalse(TEXT("Observed empty destroyed location removes memory"),D.bSnapshotValid);
 }
 IConsoleManager::Get().FindConsoleVariable(TEXT("r.Darkwell.ProjectFogVisual.PropPresentationMode"))->Set(0,ECVF_SetByConsole);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellPropLabIsolationTest,
 "Darkwell.PropLab.Scope.DefaultAndNoWorld", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDarkwellPropLabIsolationTest::RunTest(const FString& Parameters)
{
 TestFalse(TEXT("No world cannot enable lab"),Darkwell::PropLab::IsLabWorld(nullptr));
 TestEqual(TEXT("No world retains accepted presentation"),Darkwell::PropLab::PresentationMode(nullptr),0);
 FDarkwellRememberedPropState Baseline; Baseline.Initialize(FTransform::Identity,1);
 auto D=Baseline.Observe(true,FTransform(FVector(500,0,0)),1,0,1);
 TestFalse(TEXT("Baseline call still replaces snapshot immediately, without retired proxies"),D.bRetainPreviousSnapshot);
 TestTrue(TEXT("Baseline enter and exit thresholds unchanged"),FDarkwellRememberedPropState::ResolveObjectLive(false,.5f) && FDarkwellRememberedPropState::ResolveObjectLive(true,.25f));
 return true;
}
#endif
