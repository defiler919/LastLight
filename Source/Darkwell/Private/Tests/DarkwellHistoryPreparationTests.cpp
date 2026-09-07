#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "VisionPresentation/DarkwellHistoryPreparation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellWholePreparationProtocol,
	"Darkwell.ObjectMemory.Preparation.Protocol", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDarkwellWholePreparationProtocol::RunTest(const FString&)
{
	using namespace Darkwell::HistoryPreparation;
	const FTicket Ticket{1, 2, 3, 4, 5};
	for (const int32 Cells : {1, 127, 128, 129, 257})
	for (const int32 Quota : {1, 127, 128, 129})
	{
		FMaskJob Job;
		TestTrue(TEXT("bounded admission"), Job.Initialize(Ticket, Cells));
		int32 Calls = 0;
		auto Predicate = [&](bool Whole, int32 I) { ++Calls; return Whole ? (I % 3 == 0) : (I % 5 == 0); };
		while (!Job.IsReady()) TestTrue(TEXT("bounded chunk"), Job.Step(1, Quota, Predicate) <= FMath::Min(Quota, 128));
		TestEqual(TEXT("each independent output evaluated exactly once"), Calls, Cells * 2);
		TBitArray<> Whole, Footprint;
		TestTrue(TEXT("Ready consumed"), Job.Take(Ticket, Whole, Footprint));
		for (int32 I = 0; I < Cells; ++I)
		{
			TestEqual(TEXT("whole bits"), bool(Whole[I]), I % 3 == 0);
			TestEqual(TEXT("footprint bits"), bool(Footprint[I]), I % 5 == 0);
		}
		TestFalse(TEXT("no double consume"), Job.Take(Ticket, Whole, Footprint));
		TestEqual(TEXT("revoked cannot finish"), Job.Step(2, 128, Predicate), 0);
	}
	for (int32 Field = 0; Field < 5; ++Field)
	{
		FMaskJob Job; Job.Initialize(Ticket, 1); Job.Step(1, 2, [](bool, int32) { return true; });
		FTicket Stale = Ticket;
		switch (Field) { case 0: ++Stale.Owner; break; case 1: ++Stale.History; break; case 2: ++Stale.Record; break;
			case 3: ++Stale.Source; break; default: ++Stale.Request; }
		TBitArray<> A, B;
		TestFalse(TEXT("lifetime/revision/request mismatch"), Job.Take(Stale, A, B));
		TestFalse(TEXT("failed fallback revokes original ticket"), Job.Take(Ticket, A, B));
	}
	FMaskJob Age; Age.Initialize(Ticket, 129);
	for (uint64 Frame = 1; Frame <= 8; ++Frame) Age.Step(Frame, 1, [](bool, int32) { return true; });
	TestEqual(TEXT("age stops on ninth advancing frame"), Age.Step(9, 1, [](bool, int32) { return true; }), 0);
	TestTrue(TEXT("aged job releases masks"), Age.bRevoked && Age.Whole.IsEmpty() && Age.Footprint.IsEmpty());
	TestFalse(TEXT("oversized packet refused"), Age.Initialize(Ticket, 65537));
	FFrameBudget Budget; Budget.Begin(10); Budget.Charge(.001, 128); Budget.Begin(10);
	TestFalse(TEXT("same engine frame cannot replenish budget"), Budget.CanAdvance(.001, 1024));
	Age.Revoke(); Budget.Begin(10);
	TestEqual(TEXT("cancel does not replenish units"), Budget.Work, 128);
	Budget.Begin(11); TestTrue(TEXT("next engine frame replenishes"), Budget.CanAdvance(.001, 1024));
	Budget.Charge(.0008, 0);
	TestFalse(TEXT("snapshot in flight prevents another chunk or admission"), Budget.CanAdvance(.001, 1024, .0003));
	TestTrue(TEXT("small snapshot leaves time for work"), Budget.CanAdvance(.001, 1024, .0001));
	return true;
}
#endif
