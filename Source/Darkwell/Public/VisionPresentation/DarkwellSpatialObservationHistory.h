#pragma once

#include "CoreMinimal.h"
#include "VisionPresentation/DarkwellSpatialPropMemory.h"
#include "VisionPresentation/DarkwellHistoryGridV2.h"

/**
 * One player-observed world-space occupancy for a stable prop identity.
 * StableID identifies the real prop. Epoch identifies independently verifiable
 * spatial knowledge and is never exposed as a second actor identity.
 */
struct DARKWELL_API FDarkwellSpatialObservationRecord
{
	uint32 Epoch = 0;
	FTransform SnapshotTransform = FTransform::Identity;
	FDarkwellSpatialPropMemory SpatialMemory;
	FDarkwellHistoryGridV2 FineHistory;
	bool bCurrentObservedLocation = false;
	uint64 PoseUpdates = 0;
};

/**
 * Lab validation model for SpatialEvidenceOnly moving-prop memory.
 * It owns no actors, meshes, materials or SightWeave authority.
 */
struct DARKWELL_API FDarkwellSpatialObservationHistory
{
	static constexpr int32 MaxResidentRecords = 64;

	void Initialize(FName InStableId);
	int32 BeginObservedLocation(
		const FTransform& SnapshotTransform,
		const FBox2D& WorldBounds,
		float CellSize = 2.5f);
	bool RebaseCurrentObservedLocation(
		const FTransform& SnapshotTransform,
		const FBox2D& WorldBounds,
		float CellSize = 2.5f);
	bool UpdateCurrentObservedPosePreservingEvidence(const FTransform& Pose);
	bool FreezeCurrentForHiddenMovement();
	/** Drops only the unsealed live observation. Does not manufacture empty evidence. */
	bool AbandonCurrentObservationWithoutHistory();
	bool AdvanceCurrent(float DeltaSeconds, TConstArrayView<float> Coverage);
	bool AdvanceHistorical(
		uint32 Epoch,
		float DeltaSeconds,
		TConstArrayView<float> Coverage);
	int32 ReleaseFullyErasedRecords();

	FName GetStableId() const { return StableId; }
	uint32 GetNextEpoch() const { return NextEpoch; }
	int32 GetCurrentIndex() const { return CurrentIndex; }
	uint64 GetOverflowRejectCount() const { return OverflowRejectCount; }
	TConstArrayView<FDarkwellSpatialObservationRecord> GetRecords() const
	{
		return Records;
	}
	TArrayView<FDarkwellSpatialObservationRecord> GetMutableRecords()
	{
		return Records;
	}
	FDarkwellSpatialObservationRecord* FindRecord(uint32 Epoch);
	const FDarkwellSpatialObservationRecord* FindRecord(uint32 Epoch) const;

private:
	static bool IsFullyErased(const FDarkwellSpatialObservationRecord& Record);

	FName StableId;
	uint32 NextEpoch = 1;
	int32 CurrentIndex = INDEX_NONE;
	uint64 OverflowRejectCount = 0;
	TArray<FDarkwellSpatialObservationRecord> Records;
};
