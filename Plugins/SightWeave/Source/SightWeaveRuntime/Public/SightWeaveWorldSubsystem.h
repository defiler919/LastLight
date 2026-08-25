#pragma once

#include "CoreMinimal.h"
#include "SightWeaveDebug.h"
#include "SightWeaveGeometry.h"
#include "SightWeaveQueries.h"
#include "SightWeaveSpatialIndex.h"
#include "SightWeavePreparedEventIndexStats.h"
#include "Subsystems/WorldSubsystem.h"
#include "SightWeaveTypes.h"

#include "SightWeaveWorldSubsystem.generated.h"

#if WITH_DEV_AUTOMATION_TESTS
struct FSightWeaveDynamicUpdateStageMetrics
{
	double PrepareAndCompareMicroseconds = 0.0;
	double SpatialIndexMicroseconds = 0.0;
	double DirtyDiscoveryMicroseconds = 0.0;
	double PublicationMicroseconds = 0.0;
	double VisionRebuildMicroseconds = 0.0;
	double IlluminationRebuildMicroseconds = 0.0;
	double SnapshotMaterializationMicroseconds = 0.0;
};
#endif

class FSightWeavePreparedEventIndex;

UCLASS()
class SIGHTWEAVERUNTIME_API USightWeaveWorldSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "SightWeave")
	bool IsSightWeaveInitialized() const { return bSightWeaveInitialized; }

	UFUNCTION(BlueprintPure, Category = "SightWeave")
	FSightWeaveRevision GetRevision() const { return Revision; }

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Floor")
	bool RegisterFloor(const FSightWeaveFloorDefinition& Definition, UObject* Owner);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Floor")
	bool UpdateFloor(FSightWeaveFloorId ExistingFloorId, const FSightWeaveFloorDefinition& Definition);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Floor")
	bool UnregisterFloor(FSightWeaveFloorId FloorId);

	UFUNCTION(BlueprintPure, Category = "SightWeave|Floor")
	bool IsFloorRegistered(FSightWeaveFloorId FloorId) const;

	UFUNCTION(BlueprintPure, Category = "SightWeave|Floor")
	FSightWeaveFloorId GetActiveFloorId() const;

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Vision")
	FSightWeaveVisionSourceHandle RegisterVisionSource(const FSightWeaveVisionSourceDescription& Description, UObject* Owner);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Vision")
	bool UpdateVisionSource(FSightWeaveVisionSourceHandle Handle, const FSightWeaveVisionSourceDescription& Description);

	/** Allocation-free warmed motion path; preserves all non-transform source metadata. */
	UFUNCTION(BlueprintCallable, Category = "SightWeave|Vision")
	bool UpdateVisionSourceTransform(FSightWeaveVisionSourceHandle Handle, const FTransform& Transform);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Vision")
	bool UnregisterVisionSource(FSightWeaveVisionSourceHandle Handle);

	UFUNCTION(BlueprintPure, Category = "SightWeave|Vision")
	bool IsVisionSourceHandleValid(FSightWeaveVisionSourceHandle Handle) const;

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Illumination")
	FSightWeaveIlluminationSourceHandle RegisterIlluminationSource(const FSightWeaveIlluminationSourceDescription& Description, UObject* Owner);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Illumination")
	bool UpdateIlluminationSource(FSightWeaveIlluminationSourceHandle Handle, const FSightWeaveIlluminationSourceDescription& Description);

	/** Allocation-free warmed motion path; preserves all non-transform source metadata. */
	UFUNCTION(BlueprintCallable, Category = "SightWeave|Illumination")
	bool UpdateIlluminationSourceTransform(FSightWeaveIlluminationSourceHandle Handle, const FTransform& Transform);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Illumination")
	bool UnregisterIlluminationSource(FSightWeaveIlluminationSourceHandle Handle);

	UFUNCTION(BlueprintPure, Category = "SightWeave|Illumination")
	bool IsIlluminationSourceHandleValid(FSightWeaveIlluminationSourceHandle Handle) const;

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Occluder")
	FSightWeaveOccluderHandle RegisterOccluder(
		const TArray<FSightWeaveSegment2D>& Segments,
		bool bDynamic,
		bool bEnabled,
		UObject* Owner);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Occluder")
	bool UpdateOccluder(
		FSightWeaveOccluderHandle Handle,
		const TArray<FSightWeaveSegment2D>& Segments,
		bool bDynamic,
		bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Occluder")
	bool UnregisterOccluder(FSightWeaveOccluderHandle Handle);

	UFUNCTION(BlueprintPure, Category = "SightWeave|Occluder")
	bool IsOccluderHandleValid(FSightWeaveOccluderHandle Handle) const;

	UFUNCTION(BlueprintPure, Category = "SightWeave|Occluder")
	FSightWeaveRevision GetOccluderGeometryRevision(FSightWeaveOccluderHandle Handle) const;

	UFUNCTION(BlueprintCallable, Category = "SightWeave|M2 Hard Live Suppression")
	FSightWeaveHardSuppressionHandle RegisterHardLiveSuppression(
		const FSightWeaveHardSuppressionDescription& Description,
		UObject* Owner);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|M2 Hard Live Suppression")
	bool UpdateHardLiveSuppression(
		FSightWeaveHardSuppressionHandle Handle,
		const FSightWeaveHardSuppressionDescription& Description);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|M2 Hard Live Suppression")
	bool UnregisterHardLiveSuppression(FSightWeaveHardSuppressionHandle Handle);

	UFUNCTION(BlueprintPure, Category = "SightWeave|M2 Hard Live Suppression")
	bool IsHardLiveSuppressionHandleValid(FSightWeaveHardSuppressionHandle Handle) const;

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Subject Reveal")
	FSightWeaveSubjectRevealHandle ApplySubjectRevealOverride(const FSightWeaveSubjectRevealSpecification& Specification, UObject* Owner);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Subject Reveal")
	bool UpdateSubjectRevealOverride(FSightWeaveSubjectRevealHandle Handle, const FSightWeaveSubjectRevealSpecification& Specification);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Subject Reveal")
	bool RemoveSubjectRevealOverride(FSightWeaveSubjectRevealHandle Handle);

	UFUNCTION(BlueprintPure, Category = "SightWeave|Subject Reveal")
	bool IsSubjectRevealHandleValid(FSightWeaveSubjectRevealHandle Handle) const;

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Lifecycle")
	int32 UnregisterAllForOwner(UObject* Owner);

	UFUNCTION(BlueprintPure, Category = "SightWeave|Query")
	FSightWeaveVisibilityQueryResult QueryVisibilityAtLocation(FSightWeaveFloorId FloorId, FVector WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "SightWeave|Query")
	FSightWeaveVisibilityQueryResult QueryVisionSourceAtLocation(FSightWeaveVisionSourceHandle Handle, FSightWeaveFloorId FloorId, FVector WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "SightWeave|Query")
	FSightWeaveVisibilityQueryResult QueryPureVisionAtLocation(
		FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
		FSightWeaveFloorId FloorId,
		FVector WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "SightWeave|Query")
	FSightWeaveIlluminationQueryResult QueryLegalIlluminationAtLocation(
		FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
		FSightWeaveFloorId FloorId,
		FVector WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "SightWeave|Query")
	FSightWeaveVisibilityQueryResult QueryEffectiveLiveAtLocation(
		FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
		FSightWeaveFloorId FloorId,
		FVector WorldLocation) const;

	/** Reuses result attribution storage for allocation-stable authority queries. */
	void QueryEffectiveLiveAtLocationInto(
		FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
		FSightWeaveFloorId FloorId,
		FVector WorldLocation,
		FSightWeaveVisibilityQueryResult& OutResult) const;

	UFUNCTION(BlueprintPure, Category = "SightWeave|Query")
	FSightWeaveVisibilityQueryResult QueryVisionSourceHardLiveAtLocation(
		FSightWeaveVisionSourceHandle Handle,
		FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
		FSightWeaveFloorId FloorId,
		FVector WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "SightWeave|Query")
	FSightWeaveVisibilityQueryResult QuerySamples(
		FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
		FSightWeaveFloorId FloorId,
		const FSightWeaveQuerySampleSet& SampleSet) const;

	UFUNCTION(BlueprintPure, Category = "SightWeave|Query")
	FSightWeaveVisibilityQueryResult QueryBounds(
		FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
		FSightWeaveFloorId FloorId,
		FBox WorldBounds,
		ESightWeaveSampleRule Rule = ESightWeaveSampleRule::AnySample,
		int32 RequiredCount = 1) const;

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Query")
	void QueryBatch(
		const TArray<FSightWeaveQueryRequest>& Requests,
		TArray<FSightWeaveVisibilityQueryResult>& OutResults) const;

	/** Publishes registry changes and dirty polygons into an immutable ordinary-data snapshot. */
	UFUNCTION(BlueprintCallable, Category = "SightWeave|Snapshot")
	FSightWeaveRevision PublishSnapshot();

	UFUNCTION(BlueprintPure, Category = "SightWeave|Snapshot")
	FSightWeaveFrameSnapshot GetPublishedSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "SightWeave|Debug")
	FSightWeaveDebugData BuildDebugData() const;

	/** Draws explanatory data only in non-Shipping builds; it never contributes to authority. */
	UFUNCTION(BlueprintCallable, Category = "SightWeave|Debug")
	bool DrawDebugSnapshot(
		const FSightWeaveDebugDrawOptions& Options,
		const TArray<FSightWeaveDebugQueryMarker>& QueryMarkers) const;

	UFUNCTION(BlueprintPure, Category = "SightWeave|Diagnostics")
	int32 GetVisionSourceCount() const { return VisionSources.Num(); }

	UFUNCTION(BlueprintPure, Category = "SightWeave|Diagnostics")
	int32 GetIlluminationSourceCount() const { return IlluminationSources.Num(); }

	UFUNCTION(BlueprintPure, Category = "SightWeave|Diagnostics")
	int32 GetSubjectRevealCount() const { return SubjectReveals.Num(); }

	UFUNCTION(BlueprintPure, Category = "SightWeave|Diagnostics")
	int32 GetFloorCount() const { return Floors.Num(); }

	UFUNCTION(BlueprintPure, Category = "SightWeave|Diagnostics")
	int32 GetOccluderCount() const { return Occluders.Num(); }

	UFUNCTION(BlueprintPure, Category = "SightWeave|Diagnostics")
	int32 GetHardLiveSuppressionCount() const { return HardSuppressions.Num(); }

	UFUNCTION(BlueprintPure, Category = "SightWeave|Diagnostics")
	int32 GetDirtyVisionSourceCount() const { return DirtyVisionSources.Num(); }

	UFUNCTION(BlueprintPure, Category = "SightWeave|Diagnostics")
	int32 GetDirtyIlluminationSourceCount() const { return DirtyIlluminationSources.Num(); }

	UFUNCTION(BlueprintPure, Category = "SightWeave|Diagnostics")
	FSightWeaveSpatialIndexStats GetSpatialIndexStats() const { return SpatialIndex.GetStats(); }

	UFUNCTION(BlueprintPure, Category = "SightWeave|Diagnostics")
	FSightWeavePreparedEventIndexStats GetPreparedEventIndexStats() const;

#if WITH_DEV_AUTOMATION_TESTS
	const FSightWeaveDynamicUpdateStageMetrics& GetLastDynamicUpdateStageMetrics() const
	{
		return LastDynamicUpdateStageMetrics;
	}

	TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe>
	AcquirePublishedSnapshotForTesting() const
	{
		return PublishedSnapshot;
	}

	bool ConfigurePreparedEventIndexForTesting(int32 MaximumEntries, int64 MaximumBytes);
	static bool ExercisePreparedEventIndexConcurrentIsolationForTesting(
		const FSightWeaveReferenceSolveInput& Input,
		int32 WorkerCount,
		int32 RepeatsPerWorker);
#endif

	void QueryOccluderSegments(
		FSightWeaveFloorId FloorId,
		const FBox2D& Bounds,
		const FSightWeaveHeightRange& HeightRange,
		TArray<FSightWeaveSegment2D>& OutSegments) const;

	/** Clears diagnostic dirty sets after a caller has consumed them. */
	void ClearDirtySourceFlags()
	{
		DirtyVisionSources.Reset();
		DirtyIlluminationSources.Reset();
	}

private:
	void AdvanceRevision();
	void ResetState();
	FSightWeaveVisibilityQueryResult MakeQueryResult(
		ESightWeaveQueryStatus Status,
		FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
		FSightWeaveFloorId FloorId) const;
	FSightWeaveIlluminationQueryResult MakeIlluminationQueryResult(
		ESightWeaveQueryStatus Status,
		FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
		FSightWeaveFloorId FloorId) const;
	FSightWeaveVisibilityQueryResult QueryEffectiveLiveInternal(
		FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
		FSightWeaveFloorId FloorId,
		FVector WorldLocation,
		const FSightWeaveVisionSourceHandle* RestrictToSource,
		bool bPureVision) const;
	void QueryEffectiveLiveInternalInto(
		FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
		FSightWeaveFloorId FloorId,
		FVector WorldLocation,
		const FSightWeaveVisionSourceHandle* RestrictToSource,
		bool bPureVision,
		FSightWeaveVisibilityQueryResult& OutResult) const;
	void QueryEffectiveLiveValidated(
		FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
		FSightWeaveFloorId FloorId,
		FVector WorldLocation,
		const FSightWeaveVisionSourceHandle* RestrictToSource,
		bool bPureVision,
		const FSightWeaveFrameSnapshot& Snapshot,
		const FSightWeaveFloorDefinition& Floor,
		const FSightWeaveGeometryTolerances& Tolerances,
		FSightWeaveVisibilityQueryResult& OutResult,
		const FSightWeaveVisionSnapshotEntry* const* PrefilteredVisionEntries = nullptr,
		int32 PrefilteredVisionEntryCount = 0,
		uint64 PrefilteredIlluminationEligibilityMask = 0,
		bool bPrefilteredHeightMismatch = false,
		bool bUsePrefilteredBatchState = false) const;
	void InitializeQueryResult(
		FSightWeaveVisibilityQueryResult& Result,
		ESightWeaveQueryStatus Status,
		FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
		FSightWeaveFloorId FloorId,
		const FSightWeaveRevision* SnapshotRevision = nullptr) const;
	bool IsPointSuppressed(
		const FSightWeaveFrameSnapshot& Snapshot,
		FSightWeaveFloorId FloorId,
		FVector WorldLocation,
		TArray<FSightWeaveHardSuppressionHandle>& OutHandles) const;
	void RebuildVisionSnapshotEntry(int64 SourceId);
	void RebuildIlluminationSnapshotEntry(int64 SourceId);
	void ResolveSnapshotCompatibility(FSightWeaveFrameSnapshot& Snapshot) const;
	bool IsFloorDefinitionAllowed(const FSightWeaveFloorDefinition& Definition, const FSightWeaveFloorId* IgnoreFloor) const;
	void MarkSourcesAffectedByOccluderChange(
		FSightWeaveFloorId OldFloor,
		const FBox2D& OldBounds,
		FSightWeaveFloorId NewFloor,
		const FBox2D& NewBounds);
	TArray<FSightWeaveSegment2D> PrepareOccluderSegments(
		FSightWeaveOccluderHandle Handle,
		TConstArrayView<FSightWeaveSegment2D> Segments,
		bool bDynamic);
	bool PrepareDynamicOccluderSegmentsInto(
		FSightWeaveOccluderHandle Handle,
		TConstArrayView<FSightWeaveSegment2D> Segments,
		bool bDynamic,
		TArray<FSightWeaveSegment2D>& OutPrepared);

	struct FOccluderRecord
	{
		TArray<FSightWeaveSegment2D> Segments;
		FBox2D Bounds = FBox2D(ForceInit);
		FSightWeaveRevision GeometryRevision;
		bool bDynamic = false;
		bool bEnabled = true;
	};

	struct FSourceCandidateQueryKey
	{
		FSightWeaveFloorId FloorId;
		FSightWeaveHeightRange HeightRange;
		FBox2D Bounds = FBox2D(ForceInit);

		bool Matches(
			const FSightWeaveFloorId InFloorId,
			const FSightWeaveHeightRange& InHeightRange,
			const FBox2D& InBounds) const
		{
			return FloorId == InFloorId
				&& HeightRange.ZMin == InHeightRange.ZMin
				&& HeightRange.ZMax == InHeightRange.ZMax
				&& Bounds.bIsValid == InBounds.bIsValid
				&& (!Bounds.bIsValid
					|| (Bounds.Min.X == InBounds.Min.X
						&& Bounds.Min.Y == InBounds.Min.Y
						&& Bounds.Max.X == InBounds.Max.X
						&& Bounds.Max.Y == InBounds.Max.Y));
		}
	};

	bool bSightWeaveInitialized = false;
	int64 NextVisionSourceId = 1;
	int64 NextIlluminationSourceId = 1;
	int64 NextSubjectRevealId = 1;
	int64 NextOccluderId = 1;
	int64 NextHardSuppressionId = 1;
	int64 NextSegmentId = 1;
	FSightWeaveRevision Revision;
	FSightWeaveRevision LastOccluderRevision;

	TMap<FSightWeaveFloorId, FSightWeaveFloorDefinition> Floors;
	TMap<int64, FSightWeaveVisionSourceDescription> VisionSources;
	TMap<int64, FSightWeaveIlluminationSourceDescription> IlluminationSources;
	TMap<int64, FSightWeaveSubjectRevealSpecification> SubjectReveals;
	TMap<int64, FOccluderRecord> Occluders;
	TMap<int64, FSightWeaveHardSuppressionDescription> HardSuppressions;
	TMap<int64, FSightWeaveRevision> HardSuppressionRevisions;
	FSightWeaveFloorSpatialIndex SpatialIndex;
	TSet<int64> DirtyVisionSources;
	TSet<int64> DirtyIlluminationSources;
	TSet<int64> PendingVisionSnapshotRebuilds;
	TSet<int64> PendingIlluminationSnapshotRebuilds;
	TMap<int64, FSightWeaveRevision> VisionSourceRevisions;
	TMap<int64, FSightWeaveRevision> IlluminationSourceRevisions;
	TMap<int64, FSightWeaveVisionSnapshotEntry> CachedVisionSnapshotEntries;
	TMap<int64, FSightWeaveIlluminationSnapshotEntry> CachedIlluminationSnapshotEntries;
	TMap<int64, TArray<FSightWeaveSegment2D>> CachedVisionSolveSegments;
	TMap<int64, TArray<FSightWeaveSegment2D>> CachedIlluminationSolveSegments;
	TMap<int64, FSourceCandidateQueryKey> CachedVisionCandidateQueryKeys;
	TMap<int64, FSourceCandidateQueryKey> CachedIlluminationCandidateQueryKeys;
	TMap<int64, TSharedPtr<FSightWeaveOptimizedSolveCache>> CachedVisionPreparedSolves;
	TMap<int64, TSharedPtr<FSightWeaveOptimizedSolveCache>> CachedIlluminationPreparedSolves;
	TSharedPtr<FSightWeavePreparedEventIndex> PreparedEventIndex;
	TArray<FSightWeaveSegment2D> DynamicPreparedSegmentsScratch;
	TArray<int64> PublicationDirtyVisionIds;
	TArray<int64> PublicationDirtyIlluminationIds;
	TArray<int64> PublicationVisionIds;
	TArray<int64> PublicationIlluminationIds;
	TArray<int64> PublicationSuppressionIds;
	TSharedPtr<FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> PublishedSnapshot;
	TSharedPtr<FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> StandbySnapshot;

#if WITH_DEV_AUTOMATION_TESTS
	FSightWeaveDynamicUpdateStageMetrics LastDynamicUpdateStageMetrics;
#endif

	TMap<FSightWeaveFloorId, TWeakObjectPtr<UObject>> FloorOwners;
	TMap<int64, TWeakObjectPtr<UObject>> VisionOwners;
	TMap<int64, TWeakObjectPtr<UObject>> IlluminationOwners;
	TMap<int64, TWeakObjectPtr<UObject>> SubjectRevealOwners;
	TMap<int64, TWeakObjectPtr<UObject>> OccluderOwners;
	TMap<int64, TWeakObjectPtr<UObject>> HardSuppressionOwners;
};
