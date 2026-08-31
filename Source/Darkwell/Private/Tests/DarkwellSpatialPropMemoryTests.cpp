#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "VisionPresentation/DarkwellSpatialPropMemory.h"

namespace Darkwell::SpatialMemoryTests
{
 constexpr EAutomationTestFlags Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
 FDarkwellSpatialPropMemory Make()
 {
  FDarkwellSpatialPropMemory S; S.Initialize(TEXT("Lab.ManualStale.Cabinet"),FBox2D(FVector2D(0,0),FVector2D(100,100)),10); S.BeginPresent(); return S;
 }
 void Run(FDarkwellSpatialPropMemory& S,const TArray<float>& C,int32 Frames=30)
 { for(int32 I=0;I<Frames;++I) S.Advance(1.f/60,C); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellSpatialDiscoveryTest,"Darkwell.PropLab.SpatialMemory.PartialDiscoveryPersists",Darkwell::SpatialMemoryTests::Flags)
bool FDarkwellSpatialDiscoveryTest::RunTest(const FString&)
{
 using namespace Darkwell::SpatialMemoryTests;
 auto S=Make(); TArray<float> Coverage; Coverage.Init(0,100); int32 Previous=0;
 for(int32 Discovered:{10,25,50,75})
 {
  for(int32 I=0;I<100;++I) Coverage[I]=I>=Previous && I<Discovered?1:0;
  for(int32 Frame=0;Frame<12;++Frame)
  {
   const TArray<FDarkwellSpatialPropMemory::FCell> Before(S.GetCells());
   TestTrue(TEXT("Valid sample array"),S.Advance(1.f/60,Coverage));
   for(int32 I=0;I<100;++I)
   {
    const auto& C=S.GetCells()[I];
    TestTrue(TEXT("Every position discovered monotonically"),C.DiscoveredPresent>=Before[I].DiscoveredPresent);
    TestEqual(TEXT("No whole gray snapshot outside discovered region"),C.DiscoveredPresent,I<Discovered?1.f:0.f);
    if(I>=Discovered) TestTrue(TEXT("Unknown source AND proxy pixels zero"),S.Presentation(I).R==0 && S.Presentation(I).B==0);
    if(I<Previous) TestEqual(TEXT("Previously known region stays gray while scanning NEW region"),C.LiveBlend,0.f);
   }
  }
  Coverage.Init(0,100); Run(S,Coverage);
  for(int32 I=0;I<100;++I)
  {
   TestEqual(TEXT("Turning away never forgets discovered pixels"),S.GetCells()[I].DiscoveredPresent,I<Discovered?1.f:0.f);
   TestEqual(TEXT("Known pixels become gray, not floor"),S.Presentation(I).R,I<Discovered?1.f:0.f);
   TestEqual(TEXT("Live leaves smoothly to zero"),S.Presentation(I).G,0.f);
  }
  Previous=Discovered;
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellSpatialFlickerTest,"Darkwell.PropLab.SpatialMemory.BoundaryOscillationAndExit",Darkwell::SpatialMemoryTests::Flags)
bool FDarkwellSpatialFlickerTest::RunTest(const FString&)
{
 using namespace Darkwell::SpatialMemoryTests;
 auto S=Make(); TArray<float> C; C.Init(0,100); C[0]=1;
 S.Advance(1.f/60,C);
 TestTrue(TEXT("One sample starts weak local fade immediately, never full green flash"),S.GetCells()[0].LiveBlend>0 && S.GetCells()[0].LiveBlend<.1f);
 C[0]=0; S.Advance(1.f/60,C);
 TestTrue(TEXT("Exit does not reset fade on the next frame"),S.GetCells()[0].LiveBlend>0);
 float Before=S.GetCells()[0].LiveBlend; Run(S,C,20);
 TestTrue(TEXT("Brief glimpse persists as gray after fade"),S.Presentation(0).R==1 && S.Presentation(0).G==0 && Before>0);
 for(int32 I=0;I<240;++I)
 {
  C[0]=I%2==0?1:0;
  const auto Old=S.GetCells()[0]; S.Advance(1.f/60,C); const auto Now=S.GetCells()[0];
  TestTrue(TEXT("Small coverage oscillations never reset local age or knowledge"),Now.AppearanceBlend>=Old.AppearanceBlend && Now.DiscoveredPresent==1);
  TestTrue(TEXT("No alternating green/gray pulses"),Now.LiveBlend+UE_SMALL_NUMBER>=Old.LiveBlend);
  TestEqual(TEXT("Visual hysteresis cannot discover neighboring unknown cell"),S.GetCells()[1].DiscoveredPresent,0.f);
 }
 TestEqual(TEXT("Repeated re-entry converges rather than resetting"),S.GetCells()[0].LiveBlend,1.f);
 C[0]=0;
 for(int32 I=0;I<15;++I)
 {
  Before=S.GetCells()[0].LiveBlend; S.Advance(1.f/60,C);
  // Subtracting two float states can round by one float epsilon; this is not
  // a visual-time tolerance and does not relax coverage or the .18 second rule.
  TestTrue(TEXT("Exit blend monotone with bounded per-frame change"),S.GetCells()[0].LiveBlend<=Before && Before-S.GetCells()[0].LiveBlend<=1.f/60/.18f+FLT_EPSILON);
  TestEqual(TEXT("Geometry opacity never drops on exit"),S.Presentation(0).R,1.f);
 }
 TestEqual(TEXT("Exit reaches gray after .18s + short visual hold"),S.Presentation(0).G,0.f);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellSpatialEmptyTest,"Darkwell.PropLab.SpatialMemory.EmptyVerificationNeverResurrects",Darkwell::SpatialMemoryTests::Flags)
bool FDarkwellSpatialEmptyTest::RunTest(const FString&)
{
 using namespace Darkwell::SpatialMemoryTests;
 auto S=Make(); TArray<float> C; C.Init(1,100); Run(S,C); S.BeginAbsent();
 for(int32 Empty:{25,50,75,100})
 {
  for(int32 I=0;I<100;++I) C[I]=I<Empty?1:0;
  for(int32 Frame=0;Frame<30;++Frame)
  {
   const TArray<FDarkwellSpatialPropMemory::FCell> Before(S.GetCells()); S.Advance(1.f/60,C);
   for(int32 I=0;I<100;++I)
   {
    const auto& N=S.GetCells()[I];
    TestTrue(TEXT("Every empty-evidence pixel monotone"),N.VerifiedEmpty>=Before[I].VerifiedEmpty);
    TestTrue(TEXT("Every stale pixel only decreases"),N.RemainingStale<=Before[I].RemainingStale && N.StaleOpacity<=Before[I].StaleOpacity);
    TestEqual(TEXT("Remaining is initial known minus verified, never full object reset"),N.RemainingStale,N.InitialRemembered*(1-N.VerifiedEmpty));
   }
  }
  C.Init(0,100); Run(S,C);
  for(int32 I=0;I<100;++I)
  {
   TestEqual(TEXT("Turning away cannot restore erased region"),S.Presentation(I).B,I<Empty?0.f:1.f);
   TestEqual(TEXT("ABSENT has no source material opacity"),S.Presentation(I).R,0.f);
  }
 }
 // A partly discovered cabinet may never turn into a full stale cabinet.
 S=Make(); C.Init(0,100); for(int32 I=0;I<10;++I) C[I]=1; Run(S,C); S.BeginAbsent();
 for(int32 I=0;I<100;++I) TestEqual(TEXT("Only ten truly observed cells survive destruction"),S.Presentation(I).B,I<10?1.f:0.f);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellSpatialGenerationTest,"Darkwell.PropLab.SpatialMemory.GenerationIsolation",Darkwell::SpatialMemoryTests::Flags)
bool FDarkwellSpatialGenerationTest::RunTest(const FString&)
{
 using namespace Darkwell::SpatialMemoryTests;
 auto S=Make(); TArray<float> C; C.Init(0,100);
 for(int32 Cycle=0;Cycle<4;++Cycle)
 {
  const uint32 Generation=S.GetGeneration();
  C.Init(0,100); for(int32 I=0;I<25;++I) C[I]=1; Run(S,C); S.BeginAbsent();
  TestEqual(TEXT("Physical transition increments generation exactly once"),S.GetGeneration(),Generation+1);
  for(int32 I=0;I<100;++I) C[I]=I<10?1:0; Run(S,C); S.BeginPresent();
  for(int32 I=0;I<100;++I)
  {
   const auto& Cell=S.GetCells()[I];
   TestTrue(TEXT("New actual remains undiscovered, no old live/empty array reuse"),Cell.DiscoveredPresent==0 && Cell.LiveBlend==0 && Cell.VerifiedEmpty==0);
   TestEqual(TEXT("Only separately retained old unverified knowledge survives"),S.Presentation(I).B,I>=10 && I<25?1.f:0.f);
   TestEqual(TEXT("Hidden new body never leaks old knowledge as current geometry"),S.Presentation(I).R,0.f);
  }
  TestEqual(TEXT("Stable identity unchanged across generations"),S.GetStableId(),FName(TEXT("Lab.ManualStale.Cabinet")));
  S.BeginAbsent(); C.Init(1,100); Run(S,C); S.BeginPresent();
  for(int32 I=0;I<100;++I) TestTrue(TEXT("Fully verified empty history cannot resurrect after another respawn"),S.Presentation(I).R==0 && S.Presentation(I).B==0);
 }
 return true;
}
#endif
