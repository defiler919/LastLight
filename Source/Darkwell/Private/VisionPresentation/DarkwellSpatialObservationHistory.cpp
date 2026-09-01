#include "VisionPresentation/DarkwellSpatialObservationHistory.h"

DEFINE_LOG_CATEGORY_STATIC(LogDarkwellSpatialObservationHistory, Log, All);

void FDarkwellSpatialObservationHistory::Initialize(const FName InStableId)
{
	StableId = InStableId;
	NextEpoch = 1;
	CurrentIndex = INDEX_NONE;
	OverflowRejectCount = 0;
	Records.Reset();
}

int32 FDarkwellSpatialObservationHistory::BeginObservedLocation(
	const FTransform& SnapshotTransform,
	const FBox2D& WorldBounds,
	const float CellSize)
{
	if (StableId.IsNone() || !WorldBounds.bIsValid || CellSize <= 0.0f)
	{
		return INDEX_NONE;
	}
	if (CurrentIndex != INDEX_NONE)
	{
		return CurrentIndex;
	}
	if (Records.Num() >= MaxResidentRecords)
	{
		++OverflowRejectCount;
		UE_LOG(
			LogDarkwellSpatialObservationHistory,
			Warning,
			TEXT("SpatialEvidenceOnly history capacity reached; new location remains undisclosed id=%s records=%d"),
			*StableId.ToString(),
			Records.Num());
		return INDEX_NONE;
	}

	FDarkwellSpatialObservationRecord& Record = Records.AddDefaulted_GetRef();
	Record.Epoch = NextEpoch++;
	Record.SnapshotTransform = SnapshotTransform;
	Record.SpatialMemory.Initialize(StableId, WorldBounds, CellSize);
	Record.SpatialMemory.BeginPresent();
	Record.bCurrentObservedLocation = true;
	CurrentIndex = Records.Num() - 1;
	return CurrentIndex;
}

bool FDarkwellSpatialObservationHistory::RebaseCurrentObservedLocation(
	const FTransform& SnapshotTransform,
	const FBox2D& WorldBounds,
	const float CellSize)
{
	if (!Records.IsValidIndex(CurrentIndex) || !WorldBounds.bIsValid || CellSize <= 0.0f)
	{
		return false;
	}
	FDarkwellSpatialObservationRecord& Record = Records[CurrentIndex];
	Record.SnapshotTransform = SnapshotTransform;
	Record.SpatialMemory.Initialize(StableId, WorldBounds, CellSize);
	Record.SpatialMemory.BeginPresent();
	Record.bCurrentObservedLocation = true;
	return true;
}

bool FDarkwellSpatialObservationHistory::FreezeCurrentForHiddenMovement()
{
	if (!Records.IsValidIndex(CurrentIndex))
	{
		return false;
	}
	FDarkwellSpatialObservationRecord& Record = Records[CurrentIndex];
	Record.SpatialMemory.BeginAbsent();
	Record.bCurrentObservedLocation = false;
	CurrentIndex = INDEX_NONE;
	return true;
}

bool FDarkwellSpatialObservationHistory::AdvanceCurrent(
	const float DeltaSeconds,
	const TConstArrayView<float> Coverage)
{
	return Records.IsValidIndex(CurrentIndex)
		&& Records[CurrentIndex].SpatialMemory.Advance(DeltaSeconds, Coverage);
}

bool FDarkwellSpatialObservationHistory::AdvanceHistorical(
	const uint32 Epoch,
	const float DeltaSeconds,
	const TConstArrayView<float> Coverage)
{
	FDarkwellSpatialObservationRecord* Record = FindRecord(Epoch);
	return Record && !Record->bCurrentObservedLocation
		&& Record->SpatialMemory.Advance(DeltaSeconds, Coverage);
}

int32 FDarkwellSpatialObservationHistory::ReleaseFullyErasedRecords()
{
	int32 Released = 0;
	for (int32 Index = Records.Num() - 1; Index >= 0; --Index)
	{
		if (Index == CurrentIndex || !IsFullyErased(Records[Index]))
		{
			continue;
		}
		Records.RemoveAt(Index);
		if (CurrentIndex > Index)
		{
			--CurrentIndex;
		}
		++Released;
	}
	return Released;
}

FDarkwellSpatialObservationRecord* FDarkwellSpatialObservationHistory::FindRecord(
	const uint32 Epoch)
{
	return Records.FindByPredicate(
		[Epoch](const FDarkwellSpatialObservationRecord& Record)
		{
			return Record.Epoch == Epoch;
		});
}

const FDarkwellSpatialObservationRecord* FDarkwellSpatialObservationHistory::FindRecord(
	const uint32 Epoch) const
{
	return Records.FindByPredicate(
		[Epoch](const FDarkwellSpatialObservationRecord& Record)
		{
			return Record.Epoch == Epoch;
		});
}

bool FDarkwellSpatialObservationHistory::IsFullyErased(
	const FDarkwellSpatialObservationRecord& Record)
{
	if (!Record.SpatialMemory.IsAbsent())
	{
		return false;
	}
	for (const FDarkwellSpatialPropMemory::FCell& Cell : Record.SpatialMemory.GetCells())
	{
		if (Cell.RemainingStale > 0.0f || Cell.StaleOpacity > 0.0f)
		{
			return false;
		}
	}
	return true;
}
