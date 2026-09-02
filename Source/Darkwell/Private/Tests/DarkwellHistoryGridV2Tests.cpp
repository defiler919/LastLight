#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "VisionPresentation/DarkwellHistoryGridV2.h"

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
#endif
