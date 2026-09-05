#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "VisionPresentation/DarkwellSpatialObservationHistory.h"

namespace Darkwell::SpatialObservationHistoryTests
{
	constexpr EAutomationTestFlags Flags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FBox2D Bounds(const float X)
	{
		return FBox2D(FVector2D(X, 0.0f), FVector2D(X + 20.0f, 10.0f));
	}

	void Advance(
		FDarkwellSpatialObservationHistory& History,
		const uint32 Epoch,
		const TArray<float>& Coverage,
		const int32 Frames = 30)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			History.AdvanceHistorical(Epoch, 1.0f / 60.0f, Coverage);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellSpatialHistoryAtoBtoCTest,
	"Darkwell.PropLab.MovingRules.SpatialHistory.AtoBtoC",
	Darkwell::SpatialObservationHistoryTests::Flags)

bool FDarkwellSpatialHistoryAtoBtoCTest::RunTest(const FString&)
{
	using namespace Darkwell::SpatialObservationHistoryTests;
	FDarkwellSpatialObservationHistory History;
	History.Initialize(TEXT("Lab.Moving.Cabinet"));
	TArray<float> Full{1.0f, 1.0f};

	TestEqual(TEXT("A starts epoch one"), History.BeginObservedLocation(FTransform(FVector(0, 0, 0)), Bounds(0), 10.0f), 0);
	for (int32 Frame = 0; Frame < 15; ++Frame) History.AdvanceCurrent(1.0f / 60.0f, Full);
	const uint32 EpochA = History.GetRecords()[0].Epoch;
	TestTrue(TEXT("Hidden A to B freezes A"), History.FreezeCurrentForHiddenMovement());
	TestEqual(TEXT("Seeing B creates a second record for the same StableID"), History.BeginObservedLocation(FTransform(FVector(100, 0, 0)), Bounds(100), 10.0f), 1);
	for (int32 Frame = 0; Frame < 15; ++Frame) History.AdvanceCurrent(1.0f / 60.0f, Full);
	const uint32 EpochB = History.GetRecords()[1].Epoch;
	TestTrue(TEXT("Hidden B to C freezes B"), History.FreezeCurrentForHiddenMovement());
	TestEqual(TEXT("Seeing C creates a third independent record"), History.BeginObservedLocation(FTransform(FVector(200, 0, 0)), Bounds(200), 10.0f), 2);

	TestEqual(TEXT("One internal identity owns three spatial records"), History.GetStableId(), FName(TEXT("Lab.Moving.Cabinet")));
	TestEqual(TEXT("A, B and C coexist"), History.GetRecords().Num(), 3);
	TestTrue(TEXT("A remains stale after seeing C"), History.FindRecord(EpochA)->SpatialMemory.IsAbsent());
	TestTrue(TEXT("B remains stale after seeing C"), History.FindRecord(EpochB)->SpatialMemory.IsAbsent());
	TestTrue(TEXT("C is current Live knowledge"), History.GetRecords()[2].bCurrentObservedLocation);

	Advance(History, EpochA, Full);
	TestTrue(TEXT("Only A reaches erased state"), History.FindRecord(EpochA)->SpatialMemory.GetCells()[0].RemainingStale == 0.0f);
	TestTrue(TEXT("B is unaffected by A evidence"), History.FindRecord(EpochB)->SpatialMemory.GetCells()[0].RemainingStale == 1.0f);
	TestEqual(TEXT("Fully faded A releases independently"), History.ReleaseFullyErasedRecords(), 1);
	TestTrue(TEXT("B survives A release"), History.FindRecord(EpochB) != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellSpatialHistoryVisibleMovementTest,
	"Darkwell.PropLab.MovingRules.SpatialHistory.VisibleMovementNoTrail",
	Darkwell::SpatialObservationHistoryTests::Flags)

bool FDarkwellSpatialHistoryVisibleMovementTest::RunTest(const FString&)
{
	using namespace Darkwell::SpatialObservationHistoryTests;
	FDarkwellSpatialObservationHistory History;
	History.Initialize(TEXT("Lab.Moving.Visible"));
	History.BeginObservedLocation(FTransform(FVector(0, 0, 0)), Bounds(0), 10.0f);
	const uint32 Epoch = History.GetRecords()[0].Epoch;
	for (int32 Step = 1; Step <= 20; ++Step)
	{
		TestTrue(TEXT("Observed rebase succeeds"), History.RebaseCurrentObservedLocation(FTransform(FVector(Step * 5.0f, 0, 0)), Bounds(Step * 5.0f), 10.0f));
	}
	TestEqual(TEXT("Continuous observed movement creates no residual chain"), History.GetRecords().Num(), 1);
	TestEqual(TEXT("Continuous observation retains one epoch"), History.GetRecords()[0].Epoch, Epoch);
	TestTrue(TEXT("Final observed transform is authoritative player knowledge"), History.GetRecords()[0].SnapshotTransform.GetLocation().Equals(FVector(100, 0, 0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellSpatialHistoryMultiIdentityTest,
	"Darkwell.PropLab.MovingRules.SpatialHistory.MultiIdentityIsolation",
	Darkwell::SpatialObservationHistoryTests::Flags)

bool FDarkwellSpatialHistoryMultiIdentityTest::RunTest(const FString&)
{
	using namespace Darkwell::SpatialObservationHistoryTests;
	TArray<FDarkwellSpatialObservationHistory> Histories;
	Histories.SetNum(32);
	for (int32 Index = 0; Index < Histories.Num(); ++Index)
	{
		Histories[Index].Initialize(*FString::Printf(TEXT("Lab.Multi.%02d"), Index));
		Histories[Index].BeginObservedLocation(FTransform(FVector(Index * 25.0f, 0, 0)), Bounds(Index * 25.0f), 10.0f);
		TArray<float> Full{1.0f, 1.0f};
		for (int32 Frame = 0; Frame < 15; ++Frame) Histories[Index].AdvanceCurrent(1.0f / 60.0f, Full);
		Histories[Index].FreezeCurrentForHiddenMovement();
	}
	TArray<float> Full{1.0f, 1.0f};
	const uint32 TargetEpoch = Histories[7].GetRecords()[0].Epoch;
	Advance(Histories[7], TargetEpoch, Full);
	for (int32 Index = 0; Index < Histories.Num(); ++Index)
	{
		const float Remaining = Histories[Index].GetRecords()[0].SpatialMemory.GetCells()[0].RemainingStale;
		TestEqual(TEXT("Evidence affects exactly one identity"), Remaining, Index == 7 ? 0.0f : 1.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellSpatialHistoryCapacityTest,
	"Darkwell.PropLab.MovingRules.SpatialHistory.CapacityFailsClosed",
	Darkwell::SpatialObservationHistoryTests::Flags)

bool FDarkwellSpatialHistoryCapacityTest::RunTest(const FString&)
{
	using namespace Darkwell::SpatialObservationHistoryTests;
	FDarkwellSpatialObservationHistory History;
	History.Initialize(TEXT("Lab.Moving.Capacity"));
	for (int32 Index = 0; Index < FDarkwellSpatialObservationHistory::MaxResidentRecords; ++Index)
	{
		TestTrue(TEXT("Resident epoch accepted below cap"), History.BeginObservedLocation(FTransform(FVector(Index * 25.0f, 0, 0)), Bounds(Index * 25.0f), 10.0f) != INDEX_NONE);
		TestTrue(TEXT("Epoch freezes without identity rewrite"), History.FreezeCurrentForHiddenMovement());
	}
	TestEqual(TEXT("History has explicit bounded resident count"), History.GetRecords().Num(), FDarkwellSpatialObservationHistory::MaxResidentRecords);
	TestEqual(TEXT("Overflow refuses a disclosed new record"), History.BeginObservedLocation(FTransform(FVector(9999, 0, 0)), Bounds(9999), 10.0f), INDEX_NONE);
	TestEqual(TEXT("Overflow is diagnosed once"), History.GetOverflowRejectCount(), uint64(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellHistoryIndependentCurrentTest,
 "Darkwell.PropLab.MovingRules.SpatialHistory.CurrentAdmissionIndependentOfHistoryCapacity",
 Darkwell::SpatialObservationHistoryTests::Flags)
bool FDarkwellHistoryIndependentCurrentTest::RunTest(const FString&)
{
 using namespace Darkwell::SpatialObservationHistoryTests;
 FDarkwellSpatialObservationHistory H; H.Initialize(TEXT("Capacity.Live"));
 const TArray<float> Full{1,1};
 for(int32 I=0;I<H.MaxResidentRecords;++I)
 {
  TestTrue(TEXT("Independent current accepted"),H.BeginCurrentObservation(FTransform(FVector(I*25,0,0)),Bounds(I*25),10)!=INDEX_NONE);
  H.AdvanceCurrent(.2f,Full); TestTrue(TEXT("Capture below history capacity"),H.FreezeCurrentForHiddenMovement());
 }
 for(int32 Cycle=0;Cycle<3;++Cycle)
 {
  const int32 Live=H.BeginCurrentObservation(FTransform(FVector(9999,0,0)),Bounds(9999),10);
  if(!TestEqual(TEXT("New current remains independent of retained history"),Live,H.MaxResidentRecords+Cycle)) return false;
  H.AdvanceCurrent(.2f,Full);
  TestEqual(TEXT("Legal current evidence can advance at capacity"),H.GetRecords()[Live].SpatialMemory.GetCells()[0].DiscoveredPresent,1.f);
  TestTrue(TEXT("Legacy capacity cannot discard newly observed knowledge"),H.FreezeCurrentForHiddenMovement());
  TestEqual(TEXT("Successful capture seals current"),H.GetCurrentIndex(),INDEX_NONE);
  TestEqual(TEXT("Actual knowledge growth remains representable"),H.GetRecords().Num(),H.MaxResidentRecords+Cycle+1);
  for(int32 I=0;I<H.MaxResidentRecords;++I)
  {
   const auto& R=H.GetRecords()[I];
   TestEqual(TEXT("Stored epochs remain unchanged"),R.Epoch,uint32(I+1));
   TestEqual(TEXT("Existing history cannot be cleared on capacity"),R.SpatialMemory.GetCells()[0].RemainingStale,1.f);
  }
 }
 TestEqual(TEXT("Production admission rejects no acquired knowledge"),H.GetOverflowRejectCount(),uint64(0));
 Advance(H,H.GetRecords()[0].Epoch,Full);
 TestEqual(TEXT("Legal empty evidence releases its own historical record"),H.ReleaseFullyErasedRecords(),1);
 const int32 Live=H.BeginCurrentObservation(FTransform(FVector(9000,0,0)),Bounds(9000),10);
 TestTrue(TEXT("Epoch ordering survives growth and release"),H.GetRecords()[Live].Epoch>67);
 TestTrue(TEXT("Further capture remains available"),H.FreezeCurrentForHiddenMovement());
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellTerminalRecordReleaseTest,
 "Darkwell.PropLab.MovingRules.SpatialHistory.TerminalReleaseDoesNotInventEmpty",
 Darkwell::SpatialObservationHistoryTests::Flags)
bool FDarkwellTerminalRecordReleaseTest::RunTest(const FString&)
{
 FDarkwellSpatialObservationHistory H; H.Initialize(TEXT("Terminal"));
 H.BeginCurrentObservation(FTransform::Identity,FBox2D(FVector2D(0),FVector2D(2.5)),2.5);
 H.AdvanceCurrent(.2f,TArray<float>{1}); H.FreezeCurrentForHiddenMovement();
 auto& R=H.GetMutableRecords()[0]; const uint32 Epoch=R.Epoch;
 R.FineHistory.Initialize(R.SpatialMemory,R.LastLegalCaptureMask);
 TArray<float> Dark; Dark.Init(0,16); TBitArray<> Occupied(true,16),Owned(false,16);
 Owned[0]=true; R.FineHistory.Advance(.2f,Dark,Occupied,Owned);
 TestFalse(TEXT("Any unresolved surface forbids release"),H.ReleaseTerminalRecord(Epoch));
 Owned.Init(true,16); R.FineHistory.Advance(.2f,Dark,Occupied,Owned);
 TestFalse(TEXT("Replaced history is not verified empty"),R.FineHistory.IsFullyVerifiedEmpty());
 TestEqual(TEXT("No replacement sample pretends to prove emptiness"),R.FineHistory.Count(FDarkwellHistoryGridV2::VerifiedEmpty()),0);
 TestTrue(TEXT("Fully replaced record can release after host proves no cap"),H.ReleaseTerminalRecord(Epoch));
 TestEqual(TEXT("Terminal dense state is gone"),H.GetRecords().Num(),0);
 TestEqual(TEXT("Compaction is distinct from empty erasure"),H.GetCompactedRecordCount(),uint64(1));
 TestFalse(TEXT("A released epoch cannot reenter raw capture"),H.ResumeUncontradictedObservation(Epoch));
 TestEqual(TEXT("Epoch sequence cannot resurrect a released capture"),H.GetNextEpoch(),Epoch+1);
 return true;
}

#endif

