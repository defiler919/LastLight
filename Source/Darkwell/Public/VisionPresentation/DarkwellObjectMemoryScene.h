#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VisionPresentation/DarkwellSpatialObservationHistory.h"
#include "VisionPresentation/DarkwellCurrentLiveGrid.h"
#include "SightWeaveObjectPolicy.h"
#include "SightWeaveRevealObservation.h"
#include "DarkwellObjectMemoryScene.generated.h"

class UDynamicMeshComponent;
class UMaterialInstanceDynamic;
class UTexture2D;
class UMaterialInterface;
class UStaticMeshComponent;
class UDarkwellRememberablePropComponent;

/** Project gray-memory runtime. Register ordinary actors and update after coverage publication. */
UCLASS()
class DARKWELL_API ADarkwellObjectMemoryScene : public AActor
{
 GENERATED_BODY()
public:
 ADarkwellObjectMemoryScene();
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 bool RegisterRememberable(UDarkwellRememberablePropComponent* Memory, USightWeaveObjectPolicyComponent* Policy);
 void UpdateMemory(float DeltaSeconds, FVector ObserverLocation);
 /** Explicit knowledge reset; never called merely because an actor moved or vanished. */
 void ResetMemory();
	struct FHistoryRuntimeTelemetry
	{
		uint64 FrameNumber = 0;
		uint64 FramesAccumulated = 0;
		int32 ActiveHistoricalEpochs = 0;
		int32 CandidateHistoricalEpochs = 0;
		int32 SleepingHistoricalEpochs = 0;
		int32 DirtyTileCount = 0;
		int32 FineSamplesResident = 0;
		uint64 FineSamplesScanned = 0;
		uint64 CoverageFullScans = 0;
		uint64 CoverageQueries = 0;
		uint64 OcclusionOnlyQueries = 0;
  uint64 CoverageComputations=0, CoverageCacheHits=0;
		uint64 CurrentSamplesTouched = 0;
		uint64 TextureCreations = 0;
		uint64 MidCreations = 0;
		uint64 GpuTextureUploads = 0;
		uint64 OccupancyTests = 0;
  uint64 OccupancyCacheHits = 0;
		uint64 HistoryGeometryReuseHits = 0;
		uint64 HistoryOwnershipReuseHits = 0;
		uint64 HistoryCoverageReuseHits = 0;
		uint64 HistoryOccupancySamplesReused = 0;
		uint64 PrimitiveGeometryTests = 0;
		uint64 OwnershipTests = 0;
		uint64 UpdateRecordTextureCalls = 0;
		uint64 TextureUploads = 0;
		uint64 UpdateRecordCapCalls = 0;
		uint64 CapMeshRebuilds = 0;
		uint64 SweepCandidateSamples = 0;
		uint64 SweepCoverageQueries = 0;
		uint64 SweepAcceptedSamples = 0;
		uint64 SweepBudgetRejects = 0;
		uint64 SweepUnsupportedEvents = 0;
		double SweepProofUs = 0;
		int32 ProxyCount = 0;
		int32 CapComponentCount = 0;
		int32 TextureCount = 0;
		int32 MidCount = 0;
		int32 SourceMidCount = 0;
		int32 SpatialRecordCount = 0;
		uint64 FineHistoryResidentBytes = 0;
		uint64 ProcessWorkingSetBytes = 0;
		int32 UObjectCount = 0;
		int32 LiveUObjectCount = 0;
		double RefreshContributionDiagnosticsUs = 0.0;
		double LogRotationFrameUs = 0.0;
		double ReportHudUs = 0.0;
		double AdvanceFineHistoryUs = 0.0;
		double UpdateTrackedUs = 0.0;
		double CoverageUs = 0.0;
		double OccupancyUs = 0.0;
		double OwnershipUs = 0.0;
		double TextureSubmissionUs = 0.0;
		double CapPresentationUs = 0.0;
		double CurrentRevealUs = 0.0;
		double HistoricalCandidateUs = 0.0;
		double HistoricalEvidenceUs = 0.0;
		double OccupancySnapshotUs = 0.0;
		double MovingPropLabGameThreadUs = 0.0;
	};
	struct FDividerMaskDiagnostics
	{
		FDarkwellCurrentLiveGrid::EDividerSource Source = FDarkwellCurrentLiveGrid::EDividerSource::Unknown;
		TBitArray<> FullGeometryMask;
		TBitArray<> RawLiveCoverage;
		bool bObjectHasLegalContact = false;
		TBitArray<> PhysicalOcclusionGate;
		TBitArray<> WholePresentationMask;
		TBitArray<> CurrentLegalObservationMask;
		TBitArray<> LastLegalCaptureMask;
		TBitArray<> FrozenHistoryMask;
		TBitArray<> CapMask;
		TBitArray<> FinalCurrentContribution;
		TBitArray<> FinalHistoricalContribution;
	};
	struct FFineEvidenceDiagnostic
	{
		uint32 Epoch = 0;
		int32 Index = 0;
		FVector2D Position = FVector2D::ZeroVector;
		FDarkwellHistoryGridV2::FSample Sample;
		float Coverage = 0;
		bool bValid = false;
		bool bOccupied = false;
		bool bOwned = false;
		bool bSubmitted = false;
	};
	UFUNCTION(BlueprintPure, Category="Object Memory") FString GetMovingLiveTelemetry(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory") int32 GetTrackedIdentityCount() const { return Tracked.Num(); }
	UFUNCTION(BlueprintPure, Category="Object Memory") int32 GetTotalSpatialRecordCount() const;
	UFUNCTION(BlueprintPure, Category="Object Memory") int32 GetSpatialRecordCount(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory") bool IsActualPresent(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory") int32 GetTotalProxyCount() const;
	UFUNCTION(BlueprintPure, Category="Object Memory") int32 GetTotalCapTriangles() const;
	UFUNCTION(BlueprintPure, Category="Object Memory") FTransform GetTrackedTransform(FName StableId) const;
	bool DoSpatialRecordTexturesMatchForTesting(FName StableId) const;
	int32 GetHiddenFreezeCountForTesting(FName StableId) const;
	int32 GetHistoricalProxyVisibilityTransitionsForTesting(FName StableId) const;
	int32 GetHistoricalProxyCreationCountForTesting(FName StableId) const;
	int32 GetHistoricalTextureUploadCountForTesting(FName StableId) const;
	uint64 GetHistoricalVisualSignatureForTesting(FName StableId) const;
	FString GetHistoricalVisualTelemetryForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int32 GetCurrentEpochCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int32 GetStaleEpochCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int32 GetVisibleHistoricalProxyCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int32 GetVisibleHistoricalCapCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int32 GetHistoricalPresentationResourceCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int32 GetMaxOverlapContributorsForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int32 GetMaxSurfaceContributorsForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int32 GetMaxCapContributorsForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int32 GetMaxTotalContributorsForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int32 GetCurrent3DOverlapStaleSurfaceForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int32 GetCurrent3DOverlapStaleCapForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int32 GetMax3DRenderOwnershipContributorsForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int32 GetCurrentRenderContactStaleSurfaceForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int32 GetCurrentRenderContactStaleCapForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int32 GetHardOwnershipFilterLeakForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") FString Get3DOwnershipTelemetryForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") FString GetResidualFragmentTelemetryForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") FString GetCapLifecycleTelemetryForTesting(FName StableId) const;
	int32 GetMissingHistoricalCutCountForTesting(FName StableId) const;
	int32 GetCapVerticesOutsideSourceForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int32 GetFalseOccupiedHistoryCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") FString GetFalseOccupiedHistoryTelemetryForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int32 GetNewestHistoricalDiscoveredCellCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int32 GetNewestHistoricalCellCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") float GetLastLegalCoverageRatioForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") bool IsLastCoverageValidForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") FString GetLastCoverageZeroReasonForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int64 GetTransformRevisionForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int64 GetCoverageRevisionForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int64 GetCoverageTransformRevisionForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int64 GetCoverageGridRevisionForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int32 GetSealCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") int32 GetObservationEpisodeForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") FString GetObservationStateForTesting(FName StableId) const;
	float GetNewestHistoricalYawForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") FString GetFineHistoryTelemetry(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") FString GetHistoryRuntimeTelemetry() const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics") FString GetMultiEpochCompositeDiagnosis(FName StableId) const;
	bool GetDividerMaskDiagnosticsForTesting(FName StableId,FDividerMaskDiagnostics& Out) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics")
	FString GetDividerMaskTelemetryForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics")
	FString GetMemorySeamAuditForTesting(FName StableId) const;
#if WITH_DEV_AUTOMATION_TESTS
 TArray<TWeakObjectPtr<UObject>> GetOwnedPresentationObjectsForTesting() const;
#endif
	FHistoryRuntimeTelemetry GetHistoryRuntimeFrameTelemetryForTesting() const { return RuntimeFrame; }
	void GetFineEvidenceDiagnosticsForTesting(FName StableId, TArray<FFineEvidenceDiagnostic>& Out) const;
	FHistoryRuntimeTelemetry GetHistoryRuntimeTotalTelemetryForTesting() const { return RuntimeTotal; }
	UFUNCTION(BlueprintCallable, Category="Object Memory|Diagnostics")
	void ResetHistoryRuntimeTelemetryForTesting();
	bool SetTrackedTransformForTesting(FName StableId, const FTransform& Transform);
	bool InjectInvalidCoverageOnceForTesting(FName StableId);
	UFUNCTION(BlueprintPure, Category="Object Memory|Object Policy")
	FString GetRevealPolicyTelemetry(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Object Policy")
	bool IsRevealConfirmedForTesting(FName StableId) const;
	bool GetNewestCaptureMasksForTesting(FName Id,TBitArray<>& Capture,TBitArray<>& Frozen) const;
	bool DoesFrameOccupancyMatchOracleForTesting(FName Id);
 void ForceContributionRefreshForTesting(FName Id);
 uint64 GetRevealSpanEvaluationsForTesting(FName Id) const;
 bool IsWholePresentationUniformForTesting(FName Id) const;
	float GetCurrentPresentationMinimumForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|History Policy")
	FString GetHistoryPolicyTelemetry(FName StableId) const;
	USightWeaveObjectPolicyComponent* GetObjectPolicyForTesting(FName StableId) const;
	bool IsCurrentSourceVisibleForTesting(FName StableId) const;
	bool CurrentHasOnlyLivePresentationForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Object Memory|Diagnostics")
	int64 GetGeometryRevisionForTesting() const { return static_cast<int64>(GeometryRevision); }
#if WITH_DEV_AUTOMATION_TESTS
 bool bForceFullHistoryEvidenceForTesting=false;
#endif
protected:
	struct FPrimitiveGeometrySnapshot
	{
		FBox LocalBounds = FBox(ForceInit);
		FTransform WorldTransform = FTransform::Identity;
		int32 PrimitiveIndex = INDEX_NONE;
		bool bCachedPlanarProjection = false;
		double PlanarMinZ = 0, PlanarMaxZ = 0, ToleranceScale = 1;
		FBox2D ProjectionBounds = FBox2D(ForceInit);
		double ProjectionToleranceFactor = 1, ProjectionRoundoffMargin = 0;
		void CachePlanarProjection();
	};
 struct FActualOccupancySnapshot
 {
  FName StableId; FBox2D Bounds; TArray<FPrimitiveGeometrySnapshot> Geometry;
 };
	TArray<FActualOccupancySnapshot> FrameOccupancy;
 bool bUseFrameOccupancy=false, bFilterFrameOccupancy=false;
 TConstArrayView<const FActualOccupancySnapshot*> FrameOccupancyCandidates;
 mutable TMap<FVector2D,bool> FrameOccupancyPoints;
 bool bCacheFrameOccupancyPoints=false;
 bool bUseNewerCandidates=false;
 FName NewerCandidateId;
 uint32 NewerCandidateMaximumEpoch=0;
 TConstArrayView<const FDarkwellSpatialObservationRecord*> FrameNewerCandidates;
 TConstArrayView<FPrimitiveGeometrySnapshot> FrameOwnershipGeometry;
 bool bUseOwnershipGeometry=false;
	friend class FDarkwellCapPartialClipTest;
	friend class FDarkwellCapCoplanarContactTest;
	friend class FDarkwellGrayHistoryCapacityCurrentTest;
	friend class FDarkwellPlanarProjectionParity;
	friend class FDarkwellRepeatedHistoryEvidenceParity;
	friend class FDarkwellMemoryEpisodeContract;
	friend class FDarkwellObservedContentContract;
	friend class FDarkwellTerminalSceneCompaction;
	friend class FDarkwellObjectMemoryOrdinaryHost;
	static TArray<FVector2D> SubtractOwnedCapIntervals(FVector2D Candidate, TConstArrayView<FVector2D> Owned);

	struct FCapQuadSnapshot
	{
		FVector A = FVector::ZeroVector;
		FVector B = FVector::ZeroVector;
		FVector C = FVector::ZeroVector;
		FVector D = FVector::ZeroVector;
		int32 PrimitiveIndex = INDEX_NONE;
	};

	/** Per-source presentation allocations survive observation entry/exit. */
	struct FCurrentPresentation
	{
		TArray<TWeakObjectPtr<UTexture2D>> LiveTextures;
		TArray<TArray<FLinearColor>> LivePixels;
		TArray<uint64> LiveSignatures;
		int32 LiveTextureCreations=0,LiveTextureUploads=0;
	};

	struct FRecordVisual
	{
		uint32 Epoch = 0;
		TWeakObjectPtr<AActor> Proxy;
		TWeakObjectPtr<UTexture2D> Texture;
		TWeakObjectPtr<UDynamicMeshComponent> Cap;
		TArray<FBox> PartBounds;
		TArray<FPrimitiveGeometrySnapshot> PartGeometry;
		TArray<FCapQuadSnapshot> CapQuads;
		uint64 CapSignature = 0;
		uint64 TextureSignature = 0;
		int32 CapTriangles = 0;
		int32 CapExpected = 0;
		int32 CapGenerated = 0;
		int32 CapClipped = 0;
		int32 MissingHistoricalCuts = 0;
		int32 ProxyCreationCount = 0;
		int32 TextureCreationCount = 0;
		int32 TextureUploadCount = 0;
		int32 ProxyVisibilityTransitions = 0;
		FIntPoint HistoricalTextureSize = FIntPoint::ZeroValue;
		TBitArray<> SuppressedByCurrentEvidence;
		TArray<FVector2D> CapSamplePoints;
		TArray<FLinearColor> SubmittedPresentation;
		TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> Materials;
		TArray<float> CachedCoarseCoverage;
		TArray<float> CachedCoarseEvidence;
		TArray<float> CachedFineCoverage;
		uint64 CachedFineAuthorityRevision = MAX_uint64, CachedFineDrawRevision = MAX_uint64;
		TBitArray<> CachedFineOccupied, CachedCoarseOccupied;
		TArray<FBox2D> CachedGeometryRegions;
  TArray<FPrimitiveGeometrySnapshot> CachedPhysicalGeometry, CachedNewerGeometry;
		uint64 CachedCoverageAuthorityRevision = MAX_uint64;
		uint64 CachedCoverageDrawRevision = MAX_uint64;
		uint64 ProcessedGeometryRevision = 0;
		uint64 ProcessedOwnershipRevision = 0;
		uint32 ProcessedOwnershipMaximumEpoch = 0;
		uint64 LastCandidateFrame = 0;
		float CoarseEvidenceActiveSeconds = 0.0f;
		bool bPresentationDirty = true;
		bool bCapTopologyDirty = true;
		bool bPresentationRetired = false;
		bool bProxyPreparedForCapture = false;
		bool bHasProxyVisibilitySample = false;
		bool bLastProxyVisible = false;
	};
	struct FHistorySpatialKey
	{
		FName StableId;
		uint32 Epoch = 0;
		bool operator==(const FHistorySpatialKey& Other) const
		{
			return StableId == Other.StableId && Epoch == Other.Epoch;
		}
		friend uint32 GetTypeHash(const FHistorySpatialKey& Key)
		{
			return HashCombineFast(
				Key.StableId.GetComparisonIndex().ToUnstableInt(),
				HashCombineFast(Key.StableId.GetNumber(), Key.Epoch));
		}
	};
	struct FHistoryGeometryReuse
	{
		FName StableId;
		FBox2D Bounds;
		FIntPoint Size;
		uint64 PreviousGeometryRevision = 0, PreviousOwnershipRevision = 0;
		TArray<FPrimitiveGeometrySnapshot> BeforePhysical, BeforeNewer, AfterNewer;
		TBitArray<> Occupied;
		TArray<int32> DirtyIndices, PhysicalDirtyIndices;
	};
	TArray<FHistoryGeometryReuse> FrameHistoryGeometry;
	struct FHistoryOwnershipReuse
	{
		FName StableId;
		FBox2D Bounds;
		FIntPoint Size;
		TArray<FPrimitiveGeometrySnapshot> OldGeometry;
		TArray<uint32> NewerEpochs;
		TBitArray<> Evaluated, Overlap;
	};
	TArray<FHistoryOwnershipReuse> FrameHistoryOwnership;
	struct FHistoryCoverageReuse
	{
		FBox2D Bounds;
		FIntPoint Size;
		bool bPreviousValid = false;
		uint64 PreviousAuthority = MAX_uint64, PreviousDraw = MAX_uint64;
		uint64 Authority = MAX_uint64, Draw = MAX_uint64;
		TArray<float> Values;
		TBitArray<> Crossings;
	};
	TArray<FHistoryCoverageReuse> FrameHistoryCoverage;

	enum class EObservationState : uint8
	{
		NeverObserved,
		ObservedArmed,
		UnobservedSealed
	};

	struct FTrackedProp
	{
		struct FSourceBinding
		{
			TWeakObjectPtr<UStaticMeshComponent> Part;
			TWeakObjectPtr<UMaterialInterface> OriginalMaterial;
			bool bVisible = true;
		};
		TArray<FSourceBinding> SourceBindings;
		FResolvedSightWeaveObjectPolicy RegisteredPolicy;
		bool bLastCaptureEligible = false;
		FName StableId;
		TWeakObjectPtr<AActor> Actual;
		TWeakObjectPtr<USightWeaveObjectPolicyComponent> ObjectPolicy;
		uint64 ProcessedMovingRevision = 0;
		uint64 PolicyRevision = 1;
		FDarkwellSpatialObservationHistory History;
		FDarkwellCurrentLiveGrid CurrentLive;
		FCurrentPresentation CurrentPresentation;
		FSightWeaveRevealObservation RevealObservation;
		TBitArray<> CurrentLegalObservationMask;
  bool bCachedWholeLegalContact=false;
  uint64 ObservationOwnershipRevision=1;
		uint32 LocalEpoch=0;
		uint64 LastCaptureAppearanceRevision=0;
		TMap<uint32, FRecordVisual> Visuals;
		FTransform InitialTransform = FTransform::Identity;
		FTransform LastPhysicalTransform = FTransform::Identity;
		FTransform LastGeometryTransform = FTransform::Identity;
		FVector Dimensions = FVector::ZeroVector;
		FLinearColor Tint = FLinearColor::Gray;
		int32 Shape = 0;
		int32 HiddenFreezeCount = 0;
		int32 ObservationEpisode = 0;
		int32 MaxOverlapContributors = 0;
		int32 MaxSurfaceContributors = 0;
		int32 MaxCapContributors = 0;
		int32 MaxTotalContributors = 0;
		int32 VisibleHistoricalCaps = 0;
		int32 Current3DOverlapStaleSurface = 0;
		int32 Current3DOverlapStaleCap = 0;
		int32 Max3DRenderOwnershipContributors = 0;
		int32 CurrentRenderContactStaleSurface = 0;
		int32 CurrentRenderContactStaleCap = 0;
		int32 HardOwnershipFilterLeak = 0;
		TArray<FString> ResidualFragmentDiagnostics;
		uint32 Offending3DEpoch = 0;
		int32 Offending3DPrimitive = INDEX_NONE;
		FVector Offending3DWorldPosition = FVector::ZeroVector;
		float LastLegalCoverageRatio = 0.0f;
		uint64 TransformRevision = 1;
		uint64 GridRevision = 1;
		uint64 CoverageRevision = 0;
		uint64 CoverageAuthorityRevision = 0;
		uint64 CoverageTransformRevision = 0;
		uint64 CoverageGridRevision = 0;
		TArray<float> CachedCurrentCoverage;
		uint64 CachedCurrentAuthorityRevision = MAX_uint64;
		uint64 CachedCurrentCoverageDrawRevision = MAX_uint64;
		uint64 CachedCurrentTransformRevision = 0;
		uint64 CachedCurrentGridRevision = 0;
		float CurrentPresentationActiveSeconds = 0.0f;
		FBox2D LastCoverageBounds;
		FIntPoint LastCoverageSize = FIntPoint::ZeroValue;
		FString LastCoverageZeroReason = TEXT("NOT_SAMPLED");
		EObservationState ObservationState = EObservationState::NeverObserved;
		bool bLastCoverageValid = false;
		bool bInjectInvalidCoverageOnce = false;
		bool bExists = false;
		bool bDiagnosticsDirty = true;
  uint64 DiagnosticSignature=0;
	};

	struct FCoverageSnapshot
	{
		TArray<float> Values;
		uint64 AuthorityRevision = 0;
		uint64 CoverageRevision = 0;
		uint64 TransformRevision = 0;
		uint64 GridRevision = 0;
		FString ZeroReason = TEXT("NOT_SAMPLED");
		bool bValid = false;
	};

	bool IsCaptureEligible(const FTrackedProp& Prop) const;
	void DestroyVisual(FRecordVisual& Visual, bool bDiscardEvidence = true);
	void ReleaseSourcePresentation(FTrackedProp& Prop);
	bool FreezeCurrentForHiddenMotion(FTrackedProp& Prop, const TCHAR* Reason, bool bSealLastEligibleObservation = false);
	void AbandonCurrentObservationWithoutHistory(FTrackedProp& Prop);
	FBox2D ActualBounds(const AActor& Prop) const;
	TArray<FBox> ActualPartBounds(const AActor& Prop) const;
	TArray<FPrimitiveGeometrySnapshot> ActualPartGeometry(
		const AActor& Prop) const;
	bool QueryVerticalInterval(
		const FPrimitiveGeometrySnapshot& Geometry,
		FVector2D Point,
		double& OutMinZ,
		double& OutMaxZ,
		double ProjectionTolerance = 0.0) const;
	static bool ClipSegmentToGeometryProjection(
		const FPrimitiveGeometrySnapshot& Geometry,
		FVector2D Start,
		FVector2D End,
		double WorldTolerance,
		double& OutStartAlpha,
		double& OutEndAlpha);
	bool CollectCurrentOwnedVerticalIntervals(
		const FTrackedProp& Prop,
		FVector2D Point,
		TArray<FVector2D>& OutIntervals, double ProjectionTolerance = 0.0) const;
	bool CollectNewerOwnedVerticalIntervals(
		const FTrackedProp& Prop,
		uint32 OlderEpoch,
		FVector2D Point,
		TArray<FVector2D>& OutIntervals, double ProjectionTolerance = 0.0) const;
	bool HasNewerObservedGeometryOverlapAt(
		const FTrackedProp& Prop,
		const FRecordVisual& OlderVisual,
		uint32 OlderEpoch,
		FVector2D Point) const;
	bool HasNewerObservedGeometryOverlapWithinFootprint(
		const FTrackedProp& Prop,
		const FRecordVisual& OlderVisual,
		uint32 OlderEpoch,
		const FBox2D& Footprint) const;
	TArray<FPrimitiveGeometrySnapshot> CollectNewerGeometrySnapshots(
		const FTrackedProp& Prop,
		uint32 OlderEpoch) const;
	TArray<float> ConservativeCoverage(const FBox2D& Bounds) const;
	FCoverageSnapshot SampleConservativeCoverage(
		const FBox2D& Bounds,
		uint64 TransformRevision,
		uint64 GridRevision, int32 Subdivision = 1) const;
	bool AdvanceFineHistory(FTrackedProp& Prop, FDarkwellSpatialObservationRecord& Record,
		float DeltaSeconds, bool bCoverageDirty, TConstArrayView<int32> GeometryDirtyIndices,
		uint64 SweepPreviousDrawRevision);
	bool IsOccupiedByActual(FVector2D Point, FName IgnoredStableId) const;
	bool HasCurrentObservedContributionAt(const FTrackedProp& Prop, FVector2D Point) const;
	bool HasNewerObservedContributionAt(
		const FTrackedProp& Prop,
		uint32 OlderEpoch,
		FVector2D Point) const;
	bool UpdateHistoricalContributionExclusion(
		FTrackedProp& Prop,
		FDarkwellSpatialObservationRecord& Record,
		TConstArrayView<int32> DirtyIndices);
	void BuildGeometryDirtyIndices(const FTrackedProp& Prop,
		FDarkwellSpatialObservationRecord& Record, FRecordVisual& Visual,
		TArray<int32>& OutDirtyIndices, TArray<int32>& OutPhysicalDirtyIndices);
	bool IsHistoricalPresentationResolved(
		const FDarkwellSpatialObservationRecord& Record,
		const FRecordVisual& Visual) const;
	void RetireHistoricalPresentation(FTrackedProp& Prop, FRecordVisual& Visual);
	void RefreshContributionDiagnostics(FTrackedProp& Prop) const;
	const FTrackedProp* GetContributionDiagnostics(FName StableId) const;
	void UpdateTracked(FTrackedProp& Prop, float DeltaSeconds);
	void EnsureRecordVisual(FTrackedProp& Prop, FDarkwellSpatialObservationRecord& Record);
	void CaptureObservedContent(const FTrackedProp& Prop, FDarkwellSpatialObservationRecord& Record) const;
	void UpdateRecordTexture(FTrackedProp& Prop, FDarkwellSpatialObservationRecord& Record);
	void UpdateCurrentPartTextures(FTrackedProp& Prop);
	void UpdateRecordCap(FTrackedProp& Prop, FDarkwellSpatialObservationRecord& Record);
	void StampConfirmedWholeCapture(FTrackedProp& Prop, FDarkwellSpatialObservationRecord& Record,
		const FCoverageSnapshot& CoverageSnapshot) const;
	AActor* SpawnMemoryProxy(const FTrackedProp& Prop, const FDarkwellSpatialObservationRecord& Record);
	void BindProxyMaterial(FTrackedProp& Prop, FDarkwellSpatialObservationRecord& Record, AActor* Proxy);
	void RebuildHistoricalSpatialIndex();
	void PrepareHistoricalCandidates(FVector ObserverLocation);
	void QueryHistoricalSpatialIndex(const FBox2D& Bounds, bool bDirtyRegion);
	bool IsHistoricalCandidate(
		const FTrackedProp& Prop,
		const FDarkwellSpatialObservationRecord& Record,
		const FRecordVisual* Visual) const;
	void FinalizeHistoryRuntimeTelemetry(uint64 UpdateRoomStartCycles);

	virtual void LogRotationFrame(const FTrackedProp&) const {}
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> OwnedMaterials;
	/** GC-visible ownership avoids rooting a material/actor/world cycle in plain C++ state. */
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> OriginalSourceMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> OwnedTextures;
	UPROPERTY(Transient) TArray<TObjectPtr<UDynamicMeshComponent>> OwnedCaps;
	TMap<FName, FTrackedProp> Tracked;
	TMap<FIntPoint, TArray<FHistorySpatialKey>> HistoricalSpatialIndex;
	TSet<FHistorySpatialKey> FrameHistoricalCandidates;
	TSet<FIntPoint> FrameHistoryDirtyTiles;
	TArray<FBox2D> PendingHistoryDirtyRegions;
	mutable FHistoryRuntimeTelemetry RuntimeFrame;
	mutable FHistoryRuntimeTelemetry RuntimeTotal;
	uint64 RuntimeFrameSequence = 0;
	uint64 GeometryRevision = 1;
	FVector2D PreviousHistoryObserverLocation = FVector2D::ZeroVector;
	bool bHistoricalSpatialIndexDirty = true;
	bool bHasPreviousHistoryObserver = false;

 bool bPhysicalMotionThisFrame=false;
};
