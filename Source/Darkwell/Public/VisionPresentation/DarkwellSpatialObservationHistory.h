#pragma once

#include "CoreMinimal.h"
#include "VisionPresentation/DarkwellSpatialPropMemory.h"
#include "VisionPresentation/DarkwellHistoryGridV2.h"

class UStaticMesh;

/** Observed content, sufficient to rebuild a proxy after the source is gone. */
struct DARKWELL_API FDarkwellObservedPrimitive
{
	TSoftObjectPtr<UStaticMesh> Mesh;
	FBox LocalBounds = FBox(ForceInit);
	FTransform RelativeTransform = FTransform::Identity;
	uint64 PrimitiveKey = 0;
};

/**
 * One player-observed world-space occupancy for a stable prop identity.
 * StableID identifies the real prop. Epoch identifies independently verifiable
 * spatial knowledge and is never exposed as a second actor identity.
 */
struct DARKWELL_API FDarkwellSpatialObservationRecord
{
	uint32 Epoch = 0;
	FTransform SnapshotTransform = FTransform::Identity;
	uint64 ContentRevision = 0;
	TArray<FDarkwellObservedPrimitive> Primitives;
	FLinearColor Tint = FLinearColor::Gray;
	float UVScale = 5;
	FDarkwellSpatialPropMemory SpatialMemory;
	FDarkwellHistoryGridV2 FineHistory;
	/** Immutable binary capture at fine-grid resolution, independent of alpha/AA. */
	TBitArray<> LastLegalCaptureMask;
	/** Pose/content geometry cache; not an observation or empty-evidence mask. */
	TBitArray<> GeometryFootprint;
	/** Whole captures are immutable geometry knowledge and never own a cut cap. */
	bool bConfirmedWholeCapture = false;
	bool bCaptureRevisionValid = false;
	uint64 CaptureAuthorityRevision = 0;
	uint64 CaptureCoverageRevision = 0;
	uint64 CapturePoseRevision = 0;
	uint64 CapturePolicyRevision = 0;
	uint64 CaptureGeometryRevision = 0;
	bool bCurrentObservedLocation = false;
	uint64 PoseUpdates = 0;
};

/**
 * Captured object states and their effective historical knowledge.
 * It owns no actors, meshes, materials or SightWeave authority.
 */
struct DARKWELL_API FDarkwellSpatialObservationHistory
{
	/** Legacy observation API budget; production Current admission is independent. */
	static constexpr int32 MaxResidentRecords = 64;

	void Initialize(FName InStableId);
	int32 BeginObservedLocation(
		const FTransform& SnapshotTransform,
		const FBox2D& WorldBounds,
		float CellSize = 2.5f);
	/** Preserve actual new knowledge beyond the legacy 64-record admission limit. */
	int32 BeginCurrentObservation(const FTransform& SnapshotTransform,
		const FBox2D& WorldBounds, float CellSize = 2.5f);
	bool CanSealCurrentObservation() const { return CurrentIndex != INDEX_NONE && (bIndependentCurrentAdmission || Records.Num() <= MaxResidentRecords); }
	bool RebaseCurrentObservedLocation(
		const FTransform& SnapshotTransform,
		const FBox2D& WorldBounds,
		float CellSize = 2.5f);
	bool UpdateCurrentObservedPosePreservingEvidence(const FTransform& Pose);
	/** Reenter unchanged knowledge; caller must validate captured content and pose. */
	bool ResumeUncontradictedObservation(uint32 Epoch);
	bool CanResumeUncontradictedObservation(uint32 Epoch) const;
	bool FreezeCurrentForHiddenMovement();
	/** Confirmed Whole entry point. The exact fine geometry mask is the capture authority. */
	bool FreezeCurrentFromGeometryMask(const TBitArray<>& FullGeometryMask);
	/** Drops only the unsealed live observation. Does not manufacture empty evidence. */
	bool AbandonCurrentObservationWithoutHistory();
	bool AdvanceCurrent(float DeltaSeconds, TConstArrayView<float> Coverage);
	bool AdvanceHistorical(
		uint32 Epoch,
		float DeltaSeconds,
		TConstArrayView<float> Coverage);
	int32 ReleaseFullyErasedRecords();
	/** Host must first prove no residual 3D cap. Refuses any remaining surface. */
	bool ReleaseTerminalRecord(uint32 Epoch);
	uint64 GetCompactedRecordCount() const { return CompactedRecordCount; }

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
	int32 BeginObservation(const FTransform& SnapshotTransform,
		const FBox2D& WorldBounds, float CellSize, int32 ResidentLimit);
	static bool IsFullyErased(const FDarkwellSpatialObservationRecord& Record);

	FName StableId;
	uint32 NextEpoch = 1;
	int32 CurrentIndex = INDEX_NONE;
	uint64 OverflowRejectCount = 0;
	uint64 CompactedRecordCount = 0;
	bool bIndependentCurrentAdmission = false;
	TArray<FDarkwellSpatialObservationRecord> Records;
};
