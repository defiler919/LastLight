#include "VisionPresentation/DarkwellHistoryPreparation.h"

namespace Darkwell::HistoryPreparation
{
bool FMaskJob::Initialize(const FTicket InTicket, const int32 Cells)
{
	Revoke();
	if (!InTicket.Owner || !InTicket.History || !InTicket.Record || !InTicket.Source
		|| !InTicket.Request || Cells <= 0 || Cells > 65536) return false;
	Ticket = InTicket;
	Whole.Init(false, Cells);
	Footprint.Init(false, Cells);
	Cursor = 0;
	LastFrame = MAX_uint64;
	AdvancingFrames = 0;
	bRevoked = false;
	return true;
}

int32 FMaskJob::Step(const uint64 Frame, const int32 Quota,
	TFunctionRef<bool(bool, int32)> Predicate)
{
	check(IsInGameThread());
	if (bRevoked || Quota <= 0 || IsReady()) return 0;
	if (Frame != LastFrame)
	{
		LastFrame = Frame;
		if (++AdvancingFrames > 8) { Revoke(); return 0; }
	}
	const int32 Begin = Cursor;
	const int32 End = FMath::Min(2 * Whole.Num(), Cursor + FMath::Min(Quota, 128));
	for (; Cursor < End; ++Cursor)
	{
		const bool bWhole = Cursor < Whole.Num();
		const int32 Index = bWhole ? Cursor : Cursor - Whole.Num();
		(bWhole ? Whole : Footprint)[Index] = Predicate(bWhole, Index);
	}
	return Cursor - Begin;
}

bool FMaskJob::IsReady() const
{
	return !bRevoked && !Whole.IsEmpty() && Footprint.Num() == Whole.Num()
		&& Cursor == Whole.Num() * 2;
}

bool FMaskJob::Take(const FTicket Expected, TBitArray<>& OutWhole, TBitArray<>& OutFootprint)
{
	const bool bValid = IsReady() && Ticket == Expected;
	if (bValid)
	{
		OutWhole = MoveTemp(Whole);
		OutFootprint = MoveTemp(Footprint);
	}
	// A failed seal also consumes the publication right. A later result must not
	// override the synchronous fallback or be reused by a resumed epoch.
	Revoke();
	return bValid;
}

void FMaskJob::Revoke()
{
	bRevoked = true;
	Whole.Empty();
	Footprint.Empty();
	Cursor = 0;
}

void FFrameBudget::Begin(const uint64 InFrame)
{
	if (Frame == InFrame) return;
	Frame = InFrame;
	SpentSeconds = 0;
	Work = 0;
}

void FFrameBudget::Charge(const double Seconds, const int32 Units)
{
	SpentSeconds += FMath::Max(0.0, Seconds);
	Work += FMath::Max(0, Units);
}

bool FFrameBudget::CanAdvance(const double SecondsLimit, const int32 WorkLimit) const
{
	return SpentSeconds < SecondsLimit && Work < WorkLimit;
}
}
