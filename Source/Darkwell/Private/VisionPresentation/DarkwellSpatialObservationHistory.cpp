#include "VisionPresentation/DarkwellSpatialObservationHistory.h"

DEFINE_LOG_CATEGORY_STATIC(LogDarkwellSpatialObservationHistory, Log, All);

void FDarkwellSpatialObservationHistory::Initialize(const FName InStableId)
{
	StableId = InStableId;
	NextEpoch = 1;
	CurrentIndex = INDEX_NONE;
	OverflowRejectCount = 0;
	CompactedRecordCount = 0;
	bIndependentCurrentAdmission = false;
	Records.Reset();
}

int32 FDarkwellSpatialObservationHistory::BeginObservedLocation(
	const FTransform& SnapshotTransform,
	const FBox2D& WorldBounds,
	const float CellSize)
{
	return BeginObservation(SnapshotTransform, WorldBounds, CellSize, MaxResidentRecords);
}

int32 FDarkwellSpatialObservationHistory::BeginCurrentObservation(
	const FTransform& SnapshotTransform, const FBox2D& WorldBounds, const float CellSize)
{
	// Unresolved knowledge may grow; repeating an unchanged state does not.
	// Residency/compaction must never reject the player's newly acquired facts.
	return BeginObservation(SnapshotTransform, WorldBounds, CellSize, MAX_int32);
}

int32 FDarkwellSpatialObservationHistory::BeginObservation(
	const FTransform& SnapshotTransform, const FBox2D& WorldBounds,
	const float CellSize, const int32 ResidentLimit)
{
	if (StableId.IsNone() || !WorldBounds.bIsValid || CellSize <= 0.0f)
	{
		return INDEX_NONE;
	}
	if (CurrentIndex != INDEX_NONE)
	{
		return CurrentIndex;
	}
	if (Records.Num() >= ResidentLimit)
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
	bIndependentCurrentAdmission = ResidentLimit > MaxResidentRecords;
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
	const FIntPoint Size(FMath::CeilToInt(WorldBounds.GetSize().X/CellSize),FMath::CeilToInt(WorldBounds.GetSize().Y/CellSize));
	// Compatibility for fixed-footprint callers. Shape changes require the host's
	// explicit geometry reset; never silently destroy observation evidence here.
	if(Size!=Record.SpatialMemory.GetSize()) return false;
	++Record.PoseUpdates;
	Record.SnapshotTransform = SnapshotTransform;
	Record.SpatialMemory.PrepareCurrentRaster(WorldBounds,Size);
	Record.bCurrentObservedLocation = true;
	return true;
}

bool FDarkwellSpatialObservationHistory::UpdateCurrentObservedPosePreservingEvidence(const FTransform& Pose)
{
	if(!Records.IsValidIndex(CurrentIndex)) return false;
	auto& Record=Records[CurrentIndex]; Record.SnapshotTransform=Pose; ++Record.PoseUpdates; return true;
}

bool FDarkwellSpatialObservationHistory::FreezeCurrentForHiddenMovement()
{
	if (!Records.IsValidIndex(CurrentIndex))
	{
		return false;
	}
	if (!CanSealCurrentObservation())
	{
		++OverflowRejectCount;
		UE_LOG(LogDarkwellSpatialObservationHistory, Warning,
			TEXT("History capture capacity reached; current observation remains independent id=%s histories=%d"),
			*StableId.ToString(), Records.Num() - 1);
		return false;
	}
	FDarkwellSpatialObservationRecord& Record = Records[CurrentIndex];
	const auto Size=Record.SpatialMemory.GetSize();
 const int32 K=FDarkwellHistoryGridV2::SamplesPerCell;
 Record.LastLegalCaptureMask.Init(false,Size.X*Size.Y*K*K);
 for(int32 Y=0;Y<Size.Y*K;++Y) for(int32 X=0;X<Size.X*K;++X)
  Record.LastLegalCaptureMask[Y*Size.X*K+X]=Record.SpatialMemory.GetCells()[(Y/K)*Size.X+X/K].DiscoveredPresent>0;
 Record.SpatialMemory.BeginAbsent();
	Record.bCurrentObservedLocation = false;
	CurrentIndex = INDEX_NONE;
	return true;
}

bool FDarkwellSpatialObservationHistory::ResumeUncontradictedObservation(uint32 Epoch)
{
	if (CurrentIndex != INDEX_NONE) return false;
	const int32 Index = Records.IndexOfByPredicate([Epoch](const auto& R) { return R.Epoch == Epoch; });
	if (Index == INDEX_NONE) return false;
	auto& Record = Records[Index];
	// This narrow reuse path cannot reinterpret counterevidence or replacement.
	// A later canonical rebuild must start from effective facts, never raw capture.
	if (!Record.FineHistory.IsInitialized() || Record.FineHistory.GetSamples().ContainsByPredicate([](const auto& S)
		{ return S.InitialRemembered > 0 && (S.bVerifiedEmpty || S.State == FDarkwellHistoryGridV2::Superseded()); })) return false;
	Record.SpatialMemory.BeginPresent();
	// Keep the last valid capture if its resumed source becomes ineligible.
	Record.bCurrentObservedLocation = true;
	CurrentIndex = Index;
	bIndependentCurrentAdmission = true;
	return true;
}

bool FDarkwellSpatialObservationHistory::FreezeCurrentFromGeometryMask(const TBitArray<>& FullGeometryMask)
{
 if(!Records.IsValidIndex(CurrentIndex)) return false;
 if(!CanSealCurrentObservation())
 {
  ++OverflowRejectCount;
  UE_LOG(LogDarkwellSpatialObservationHistory,Warning,
   TEXT("History capture capacity reached; confirmed Whole current remains independent id=%s histories=%d"),
   *StableId.ToString(),Records.Num()-1);
  return false;
 }
 FDarkwellSpatialObservationRecord& Record=Records[CurrentIndex];
 const FIntPoint Size=Record.SpatialMemory.GetSize()*FDarkwellHistoryGridV2::SamplesPerCell;
 if(FullGeometryMask.Num()!=Size.X*Size.Y || FullGeometryMask.CountSetBits()==0) return false;
 Record.LastLegalCaptureMask=FullGeometryMask;
 Record.bConfirmedWholeCapture=true;
 Record.SpatialMemory.BeginAbsent();
 Record.bCurrentObservedLocation=false;
 CurrentIndex=INDEX_NONE;
 return true;
}

bool FDarkwellSpatialObservationHistory::AdvanceCurrent(
	const float DeltaSeconds,
	const TConstArrayView<float> Coverage)
{
	return Records.IsValidIndex(CurrentIndex)
		&& Records[CurrentIndex].SpatialMemory.Advance(DeltaSeconds, Coverage);
}

bool FDarkwellSpatialObservationHistory::AbandonCurrentObservationWithoutHistory()
{
	if (!Records.IsValidIndex(CurrentIndex)) return false;
	check(Records[CurrentIndex].bCurrentObservedLocation);
	Records.RemoveAt(CurrentIndex);
	CurrentIndex = INDEX_NONE;
	// NextEpoch remains monotonic; sealed records and their evidence are untouched.
	return true;
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

bool FDarkwellSpatialObservationHistory::ReleaseTerminalRecord(uint32 Epoch)
{
	const int32 Index=Records.IndexOfByPredicate([Epoch](const auto& R){return R.Epoch==Epoch;});
	if(Index==INDEX_NONE || Index==CurrentIndex || !Records[Index].FineHistory.IsInitialized()
		|| Records[Index].FineHistory.HasResidualSurface()) return false;
	Records.RemoveAt(Index);
	if(CurrentIndex>Index) --CurrentIndex;
	++CompactedRecordCount;
	return true;
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
	if (Record.FineHistory.IsInitialized()) return Record.FineHistory.IsFullyVerifiedEmpty();
	for (const FDarkwellSpatialPropMemory::FCell& Cell : Record.SpatialMemory.GetCells())
	{
		if (Cell.RemainingStale > 0.0f || Cell.StaleOpacity > 0.0f)
		{
			return false;
		}
	}
	return true;
}
