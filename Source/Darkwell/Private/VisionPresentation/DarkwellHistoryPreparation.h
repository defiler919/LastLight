#pragma once

#include "CoreMinimal.h"

namespace Darkwell::HistoryPreparation
{
/** A private continuation, never a license to create or restore a history record.
 * The owner validates its exact immutable input before Take. Frame ids are engine
 * frame ids in production and explicit integers in deterministic tests. */
struct FTicket
{
	uint64 Owner = 0, History = 0, Record = 0, Source = 0, Request = 0;
	bool operator==(const FTicket& Other) const = default;
};

struct FMaskJob
{
	FTicket Ticket;
	TBitArray<> Whole, Footprint;
	int32 Cursor = 0;
	uint64 LastFrame = MAX_uint64;
	uint32 AdvancingFrames = 0;
	bool bRevoked = false;

	bool Initialize(FTicket InTicket, int32 Cells);
	/** At most Quota cell predicates; no retained callbacks or borrowed inputs. */
	int32 Step(uint64 Frame, int32 Quota, TFunctionRef<bool(bool, int32)> Predicate);
	bool IsReady() const;
	bool Take(FTicket Expected, TBitArray<>& OutWhole, TBitArray<>& OutFootprint);
	void Revoke();
};

/** Shared by every update of one owner; cancellation cannot replenish this. */
struct FFrameBudget
{
	uint64 Frame = MAX_uint64;
	double SpentSeconds = 0;
	int32 Work = 0;
	void Begin(uint64 InFrame);
	void Charge(double Seconds, int32 Units);
	bool CanAdvance(double SecondsLimit, int32 WorkLimit, double InFlightSeconds = 0) const;
};
}
