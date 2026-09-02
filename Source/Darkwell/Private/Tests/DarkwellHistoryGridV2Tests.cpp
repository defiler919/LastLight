#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "VisionPresentation/DarkwellHistoryGridV2.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellMixedCellTest,
	"Darkwell.PropLab.MovingRules.HistoryGridV2.MixedCellEmptyOccupiedUnresolved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDarkwellMixedCellTest::RunTest(const FString&)
{
	FDarkwellSpatialPropMemory Old;
	Old.Initialize(TEXT("Lab.Mixed"), FBox2D(FVector2D(0, 0), FVector2D(2.5, 2.5)));
	Old.BeginPresent();
	const TArray<float> Full{1.f};
	Old.Advance(.20f, Full); Old.BeginAbsent();
	FDarkwellHistoryGridV2 Grid; Grid.Initialize(Old);
	TArray<float> Legal; Legal.Init(0.f, 16);
	TBitArray<> Occupied(false, 16), Owned(false, 16);
	Legal[0] = 1; Occupied[1] = true; Owned[1] = true;
	for (int32 I = 0; I < 8; ++I) Grid.Advance(1.f / 60, Legal, Occupied, Owned);
	TestTrue(TEXT("Legal empty subregion confirmed"), Grid.GetSamples()[0].State == Grid.VerifiedEmpty());
	TestTrue(TEXT("Current-owned subregion superseded"), Grid.GetSamples()[1].State == Grid.Superseded());
	TestFalse(TEXT("Ownership is not empty knowledge"), Grid.GetSamples()[1].bVerifiedEmpty);
	TestTrue(TEXT("No new evidence retains history"), Grid.GetSamples()[2].State == Grid.Unresolved());
	TestEqual(TEXT("All three coexist in one original coarse cell"), Grid.CountMixedCoarseCells(), 1);
	TestEqual(TEXT("Parallel model cannot mutate old V"), Old.GetCells()[0].VerifiedEmpty, 0.f);
	Legal.Init(0, 16); Owned.Init(false, 16);
	for (int32 I = 0; I < 30; ++I) Grid.Advance(1.f / 60, Legal, Occupied, Owned);
	TestEqual(TEXT("Empty never resurrects offscreen"), Grid.GetSamples()[0].Opacity, 0.f);
	TestTrue(TEXT("Ownership never resurrects offscreen"), Grid.GetSamples()[1].State == Grid.Superseded());
	TestEqual(TEXT("Unobserved remembered region remains"), Grid.GetSamples()[2].Opacity, 1.f);
	TestTrue(TEXT("Cannot retire an unresolved record"), Grid.HasResidualSurface());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellFineSurfaceTest,
	"Darkwell.PropLab.MovingRules.HistoryGridV2.SurfaceOwnershipAndFade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDarkwellFineSurfaceTest::RunTest(const FString&)
{
	FDarkwellSpatialPropMemory Old;
	Old.Initialize(TEXT("Lab.Surface"), FBox2D(FVector2D(0), FVector2D(2.5)));
	Old.BeginPresent(); Old.Advance(.20f, TArray<float>{1.f}); Old.BeginAbsent();
	FDarkwellHistoryGridV2 Grid; Grid.Initialize(Old);
	TArray<float> Legal; Legal.Init(0,16); Legal[0]=1;
	TBitArray<> Occupied(false,16), Owned(false,16); Owned[1]=true; Occupied[1]=true;
	TArray<FLinearColor> Pixels;
	Grid.Advance(1.f/60,Legal,Occupied,Owned); Grid.BuildPresentation(Pixels);
	TestEqual(TEXT("Superseded final unfiltered ownership is zero immediately"),Pixels[1].A,0.f);
	TestEqual(TEXT("Smooth field is not zeroed before bilinear"),Pixels[1].B,1.f);
	for(int32 I=0;I<6;++I)Grid.Advance(1.f/60,Legal,Occupied,Owned);
	Grid.BuildPresentation(Pixels);
	TestTrue(TEXT("Empty evidence fades locally with existing contract"),Pixels[0].B>0 && Pixels[0].B<1);
	TestEqual(TEXT("Neighbor remains fully remembered"),Pixels[2].B,1.f);
	for(int32 I=0;I<30;++I)Grid.Advance(1.f/60,Legal,Occupied,Owned);
	Grid.BuildPresentation(Pixels);
	TestEqual(TEXT("Fully erased final gate cannot receive bilinear spill"),Pixels[0].A,0.f);
	TestEqual(TEXT("Fully erased raw opacity is zero"),Pixels[0].B,0.f);
	TestFalse(TEXT("Ownership does not claim whole record verified empty"),Grid.IsFullyVerifiedEmpty());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellFineCapBoundaryTest,
	"Darkwell.PropLab.MovingRules.HistoryGridV2.PositiveHistoricalCapAndNoCapForSupersededBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDarkwellFineCapBoundaryTest::RunTest(const FString&)
{
	FDarkwellSpatialPropMemory Old;
	Old.Initialize(TEXT("Lab.Cap"),FBox2D(FVector2D(0),FVector2D(5,2.5)));
	Old.BeginPresent(); Old.Advance(.2f,TArray<float>{1,0}); Old.BeginAbsent();
	FDarkwellHistoryGridV2 Grid; Grid.Initialize(Old);
	TestTrue(TEXT("Partial discovery has a legal fine boundary cap"),Grid.CanEmitCap(3,4));
	TArray<float> Coverage; Coverage.Init(0,32); Coverage[2]=1;
	TBitArray<> Occupied(false,32),Owned(false,32);Owned[4]=true;Occupied[4]=true;
	for(int32 I=0;I<8;++I) Grid.Advance(1.f/60,Coverage,Occupied,Owned);
	TestFalse(TEXT("Ownership is not a cut and must not emit cap"),Grid.CanEmitCap(3,4));
	TestTrue(TEXT("Verified empty remains a positive cut"),Grid.CanEmitCap(3,2));
	TestFalse(TEXT("Superseding neighbor did not manufacture empty knowledge"),Grid.GetSamples()[4].bVerifiedEmpty);
	TestTrue(TEXT("Unresolved source and fading empty forbid retirement"),Grid.HasResidualSurface());
	Owned.Init(true,32);Grid.Advance(1.f/60,Coverage,Occupied,Owned);
	TestFalse(TEXT("All ownership-resolved surface can retire"),Grid.HasResidualSurface());
	TestFalse(TEXT("Retiring output is not claiming all space empty"),Grid.IsFullyVerifiedEmpty());
	return true;
}
namespace
{
	FDarkwellHistoryGridV2 MakeEmptyEvidenceGrid()
	{
		FDarkwellSpatialPropMemory Memory;
		Memory.Initialize(TEXT("Lab.FastSweep"), FBox2D(FVector2D(0), FVector2D(2.5)));
		Memory.BeginPresent(); Memory.Advance(.2f, TArray<float>{1}); Memory.BeginAbsent();
		FDarkwellHistoryGridV2 Grid; Grid.Initialize(Memory); return Grid;
	}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellFastEmptyEvidenceTest,
	"Darkwell.PropLab.MovingRules.FastSweep.FastSweepHistoricalEmptyEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDarkwellFastEmptyEvidenceTest::RunTest(const FString&)
{
	for (const float Fps : {30.f,60.f,120.f,144.f})
	{
		auto Grid=MakeEmptyEvidenceGrid();TArray<float> Coverage;Coverage.Init(0,16);Coverage[0]=1;
		TBitArray<> Occupied(false,16),Owned(false,16);
		Grid.Advance(1/Fps,Coverage,Occupied,Owned);
		TestTrue(TEXT("Single lawful footprint commits empty fact independent of frame duration"),Grid.GetSamples()[0].bVerifiedEmpty);
		TestTrue(TEXT("Frozen .20 fade is not instant removal"),Grid.GetSamples()[0].Opacity>0);
		Coverage.Init(0,16);
		for(int32 I=0;I<40;++I)Grid.Advance(1/Fps,Coverage,Occupied,Owned);
		TestEqual(TEXT("Fact continues fading after leaving"),Grid.GetSamples()[0].Opacity,0.f);
		TestTrue(TEXT("Fact is monotonic"),Grid.GetSamples()[0].bVerifiedEmpty);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellInvalidFineEvidenceTest,
	"Darkwell.PropLab.MovingRules.FastSweep.InvalidCoverageDoesNotAccumulateEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDarkwellInvalidFineEvidenceTest::RunTest(const FString&)
{
	auto Grid=MakeEmptyEvidenceGrid();TArray<float> C;C.Init(std::numeric_limits<float>::quiet_NaN(),16);
	TBitArray<> O(false,16),N(false,16);for(int32 I=0;I<60;++I)Grid.Advance(1.f/60,C,O,N);
	TestEqual(TEXT("Invalid input contributes no evidence"),Grid.Count(Grid.VerifiedEmpty()),0);
	TestEqual(TEXT("No invalid dwell"),Grid.GetSamples()[0].EmptyDwell,0.f);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellOccupiedFineEvidenceTest,
	"Darkwell.PropLab.MovingRules.FastSweep.OccupiedSampleDoesNotBecomeEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDarkwellOccupiedFineEvidenceTest::RunTest(const FString&)
{
	auto Grid=MakeEmptyEvidenceGrid();TArray<float> C;C.Init(1,16);TBitArray<> O(true,16),N(false,16);
	Grid.Advance(.1f,C,O,N);TestEqual(TEXT("Occupied footprints do not prove empty"),Grid.Count(Grid.VerifiedEmpty()),0);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellSupersededFineEvidenceTest,
	"Darkwell.PropLab.MovingRules.FastSweep.SupersededStillNotVerifiedEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDarkwellSupersededFineEvidenceTest::RunTest(const FString&)
{
	auto Grid=MakeEmptyEvidenceGrid();TArray<float> C;C.Init(1,16);TBitArray<> O(false,16),N(true,16);
	Grid.Advance(.1f,C,O,N);N.Init(false,16);Grid.Advance(.1f,C,O,N);
	TestEqual(TEXT("Terminal ownership remains"),Grid.Count(Grid.Superseded()),16);
	TestFalse(TEXT("Ownership never manufactures an empty fact"),Grid.GetSamples()[0].bVerifiedEmpty);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellUnobservedFineEvidenceTest,
	"Darkwell.PropLab.MovingRules.FastSweep.UnobservedHistoryStillSurvives",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDarkwellUnobservedFineEvidenceTest::RunTest(const FString&)
{
	auto Grid=MakeEmptyEvidenceGrid();TArray<float> C;C.Init(0,16);TBitArray<> O(false,16),N(false,16);
	for(int32 I=0;I<600;++I)Grid.Advance(1.f/60,C,O,N);
	TestEqual(TEXT("No identity or time based clearing"),Grid.Count(Grid.Unresolved()),16);return true;
}
#endif
