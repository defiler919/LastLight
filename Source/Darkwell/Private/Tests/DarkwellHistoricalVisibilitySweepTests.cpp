#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "VisionPresentation/DarkwellHistoricalVisibilitySweep.h"
#include "VisionPresentation/DarkwellHistoryGridV2.h"

namespace
{
	FDarkwellFogVisualSourceSnapshot SweepSource(double Degrees)
	{
		FDarkwellFogVisualSourceSnapshot S;
		S.BodyRadiusCentimeters=10;S.ConeRangeCentimeters=1000;S.ConeHalfAngleDegrees=25;
		S.ConeForward=FVector2D(FMath::Cos(FMath::DegreesToRadians(Degrees)),FMath::Sin(FMath::DegreesToRadians(Degrees)));
		S.bConeLegallyLive=true;S.AuthorityRevision=1;return S;
	}
	uint64 RunSweepGrid(float Fps,float Duration, TConstArrayView<FDarkwellFogVisualSegment> Occluders={},int32* OutEmpty=nullptr)
	{
		FDarkwellSpatialPropMemory M;M.Initialize(TEXT("Lab.Sweep"),FBox2D(FVector2D(498,-2),FVector2D(503,3)));
		M.BeginPresent();TArray<float> C;C.Init(1,4);M.Advance(.2f,C);M.BeginAbsent();
		FDarkwellHistoryGridV2 G;G.Initialize(M);const auto Size=G.GetSize();const auto Bounds=G.GetBounds();
		const FVector2D Step=Bounds.GetSize()/FVector2D(Size.X,Size.Y);
		TBitArray<> Occupied(false,64),Owned(false,64);C.Init(0,64);
		auto Previous=SweepSource(-60);const int32 Frames=FMath::Max(1,FMath::RoundToInt(Fps*Duration));
		for(int32 F=1;F<=Frames;++F)
		{
			auto Current=SweepSource(FMath::Lerp(-60.0,60.0,double(F)/Frames));
			for(int32 I=0;I<64;++I)
			{
				const FVector2D Min=Bounds.Min+Step*FVector2D(I%Size.X,I/Size.X);uint64 Queries=0;
				const FBox2D Cell(Min,Min+Step);
				C[I]=FDarkwellHistoricalVisibilitySweep::ProveEmptyFootprintCoverage(Previous,Current,Occluders,Cell,Queries)?1:0;
			}
			G.Advance(1/Fps,C,Occupied,Owned);Previous=Current;
		}
		C.Init(0,64);for(int32 F=0;F<60;++F)G.Advance(1/Fps,C,Occupied,Owned);
		if(OutEmpty)*OutEmpty=G.Count(G.VerifiedEmpty());return G.EvidenceHash();
	}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellSweepWallNegativeTest,
	"Darkwell.PropLab.MovingRules.FastSweep.FastSweepBehindWallNegative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDarkwellSweepWallNegativeTest::RunTest(const FString&)
{
	const auto A=SweepSource(-60),B=SweepSource(60);const FBox2D Cell(FVector2D(499,-1),FVector2D(501,1));
	uint64 Queries=0;
	TestEqual(TEXT("Both discrete endpoints miss the center"),FDarkwellContinuousVisibilityBuilder::QuerySourceCoverage(A,Cell.GetCenter(),{}).Coverage,0.f);
	TestTrue(TEXT("Continuous lawful rotation proves the common footprint"),FDarkwellHistoricalVisibilitySweep::ProveEmptyFootprintCoverage(A,B,{},Cell,Queries));
	const TArray<FDarkwellFogVisualSegment> Wall{{FVector2D(250,-1000),FVector2D(250,1000)}};
	TestFalse(TEXT("Same swept sector cannot see through actual wall segments"),FDarkwellHistoricalVisibilitySweep::ProveEmptyFootprintCoverage(A,B,Wall,Cell,Queries));
	int32 Empty=0;RunSweepGrid(30,.01f,Wall,&Empty);TestEqual(TEXT("Wall-hidden history never becomes verified empty"),Empty,0);
	auto Invalid=B;Invalid.bConeLegallyLive=false;
	TestFalse(TEXT("Legality transition cannot interpolate knowledge"),FDarkwellHistoricalVisibilitySweep::IsSupported(A,Invalid));
	Invalid=B;Invalid.ConeOrigin.X=100;
	TestFalse(TEXT("Unproven moving-origin path is conservative"),FDarkwellHistoricalVisibilitySweep::IsSupported(A,Invalid));
	TestFalse(TEXT("Ambiguous half-turn fails conservative"),FDarkwellHistoricalVisibilitySweep::IsSupported(SweepSource(0),SweepSource(180)));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellSweepSpeedEquivalentTest,
	"Darkwell.PropLab.MovingRules.FastSweep.SlowFastSweepEquivalent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDarkwellSweepSpeedEquivalentTest::RunTest(const FString&)
{
	const uint64 Expected=RunSweepGrid(60,4);
	for(const float Duration:{4.f,1.f,.1f,.001f})
	{
		int32 Empty=0;const uint64 Hash=RunSweepGrid(60,Duration,{},&Empty);
		TestEqual(TEXT("Every lawful speed has identical ordered evidence hash"),Hash,Expected);
		TestEqual(TEXT("Every cell was swept"),Empty,64);
		AddInfo(FString::Printf(TEXT("SWEEP_SPEED seconds=%.4f fps=60 hash=%llu empty=%d analytic_substeps=0"),Duration,Hash,Empty));
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellSweepFrameInvariantTest,
	"Darkwell.PropLab.MovingRules.FastSweep.FrameRateInvariant30_60_120",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDarkwellSweepFrameInvariantTest::RunTest(const FString&)
{
	const uint64 Expected=RunSweepGrid(60,4);
	for(const float Fps:{30.f,60.f,120.f,144.f})for(const float Duration:{4.f,1.f,.1f,.001f})
	{
		int32 Empty=0;const uint64 Hash=RunSweepGrid(Fps,Duration,{},&Empty);
		TestEqual(TEXT("30/60/120/144 fps x four speeds exact evidence equality"),Hash,Expected);
		TestEqual(TEXT("No spatial tunneling at one-frame extreme"),Empty,64);
		AddInfo(FString::Printf(TEXT("SWEEP_FPS fps=%.0f seconds=%.4f hash=%llu empty=%d"),Fps,Duration,Hash,Empty));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellPositiveSweepEndpointBoundTest,
 "Darkwell.PropLab.MovingRules.FastSweep.PositiveObservationEndpointBound",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellPositiveSweepEndpointBoundTest::RunTest(const FString&)
{
 int32 Rejected=0,Skipped=0;
 for(double Distance:{30.,100.,500.}) for(double Bearing:{-90.,-30.,0.,20.,80.}) for(double Start:{-120.,-50.,0.,70.}) for(double Turn:{.5,10.,30.,80.,160.})
 {
  const auto A=SweepSource(Start),B=SweepSource(Start+Turn);
  const FVector2D Center=FVector2D(FMath::Cos(FMath::DegreesToRadians(Bearing)),FMath::Sin(FMath::DegreesToRadians(Bearing)))*Distance;
  const FBox2D Bounds(Center-FVector2D(5),Center+FVector2D(5));
  const FVector2D Points[]{Center+FVector2D(-1.25,-1.25),Center+FVector2D(1.25,-1.25),Center+FVector2D(1.25,1.25),Center+FVector2D(-1.25,1.25),Center};
  auto Legal=[&](const FDarkwellFogVisualSourceSnapshot& S){ for(auto P:Points) if(FDarkwellContinuousVisibilityBuilder::QuerySourceCoverage(S,P,{}).Coverage<.99f) return false; return true; };
  if(Legal(A) || Legal(B)) continue;
  uint64 Queries=0; const bool Interior=FDarkwellHistoricalVisibilitySweep::ProvePointSetCoverage(A,B,{},Points,Queries);
  if(!FDarkwellHistoricalVisibilitySweep::MayAddIntermediateSamples(A,B,Bounds))
  { ++Rejected; TestFalse(TEXT("Cheap short-interval rejection never loses a real intermediate footprint"),Interior); }
  else if(Interior) ++Skipped;
 }
 TestTrue(TEXT("Short intervals exercise cheap rejection"),Rejected>0);
 TestTrue(TEXT("Large intervals retain actual skipped observations"),Skipped>0);
 return true;
}
#endif
