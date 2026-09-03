#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/DarkwellInteractable.h"
#include "Visibility/DarkwellVisionIntegrationFixture.h"
#include "VisionPresentation/DarkwellSpatialObservationHistory.h"
#include "VisionPresentation/DarkwellCurrentLiveGrid.h"
#include "SightWeaveObjectPolicy.h"
#include "SightWeaveRevealObservation.h"
#include "DarkwellMovingPropLabRoom.generated.h"

class ADarkwellCharacter;
class ADarkwellPropLabFurniture;
class UDynamicMeshComponent;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UTexture2D;
class ADarkwellMovingPropLabRoom;

UENUM()
enum class EDarkwellMovingPropLabControlKind : uint8
{
	VisibleTranslate,
	VisibleRotate,
	HiddenAtoB,
	CoverageBoundary,
	AtoBtoC,
	MultiProp,
	ResetCurrent
};

/** Lab-only world interaction driven by the existing F-key interaction path. */
UCLASS()
class DARKWELL_API ADarkwellMovingPropLabControl final
	: public AActor
	, public IDarkwellInteractable
{
	GENERATED_BODY()

public:
	ADarkwellMovingPropLabControl();
	void Configure(
		ADarkwellMovingPropLabRoom* InRoom,
		EDarkwellMovingPropLabControlKind InKind,
		const FText& InLabel);
	void RefreshDisplay();

	virtual bool CanInteract(const ADarkwellCharacter& Character) const override;
	virtual void Interact(ADarkwellCharacter& Character) override;
	virtual FText GetInteractionPrompt(const ADarkwellCharacter& Character) const override;
	virtual void OnInteractionFocusChanged(bool bFocused) override;

	UFUNCTION(BlueprintPure, Category="Lab") EDarkwellMovingPropLabControlKind GetControlKind() const { return Kind; }
	UFUNCTION(BlueprintCallable, Category="Lab") bool TriggerForLabEvidence(ADarkwellCharacter* Character);
	EDarkwellMovingPropLabControlKind GetKind() const { return Kind; }

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> ControlRoot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> StatusIndicator;
	TWeakObjectPtr<ADarkwellMovingPropLabRoom> Room;
	FText BaseLabel;
	EDarkwellMovingPropLabControlKind Kind = EDarkwellMovingPropLabControlKind::VisibleTranslate;
	bool bFocused = false;
};

/** Development-only, runtime-spawned basic-geometry room for moving prop rules. */
UCLASS()
class DARKWELL_API ADarkwellMovingPropLabRoom final : public AActor
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Lab") FString GetMovingLiveTelemetry(FName StableId) const;
	struct FHistoryRuntimeTelemetry
	{
		uint64 FrameNumber = 0;
		uint64 FramesAccumulated = 0;
		int32 ActiveHistoricalEpochs = 0;
		int32 FineSamplesResident = 0;
		uint64 FineSamplesScanned = 0;
		uint64 CoverageFullScans = 0;
		uint64 CoverageQueries = 0;
  uint64 CoverageComputations=0, CoverageCacheHits=0;
		uint64 CurrentSamplesTouched = 0;
		uint64 TextureCreations = 0;
		uint64 MidCreations = 0;
		uint64 GpuTextureUploads = 0;
		uint64 OccupancyTests = 0;
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
		double MovingPropLabGameThreadUs = 0.0;
	};

	ADarkwellMovingPropLabRoom();
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

	static ADarkwellMovingPropLabRoom* FindActive(const UWorld* World);
	FBox2D FloorBounds() const;
	void BuildOccluders(TArray<FDarkwellVisionIntegrationSegment>& Out) const;
	void BindRoomPresentation(UTexture* Raw, FVector2D Min, FVector2D Inv);
	bool ResetRoom(ADarkwellCharacter* Player);
	void UpdateRoom(float DeltaSeconds, ADarkwellCharacter* Player);
	void Command(const TArray<FString>& Args);

	UFUNCTION(BlueprintPure, Category="Lab") FString GetStatus() const { return Status; }
	UFUNCTION(BlueprintPure, Category="Lab") int32 GetScenario() const { return Scenario; }
	UFUNCTION(BlueprintPure, Category="Lab") int32 GetScenarioPhase() const { return ScenarioPhase; }
	UFUNCTION(BlueprintPure, Category="Lab") int32 GetTrackedIdentityCount() const { return Tracked.Num(); }
	UFUNCTION(BlueprintPure, Category="Lab") int32 GetTotalSpatialRecordCount() const;
	UFUNCTION(BlueprintPure, Category="Lab") int32 GetSpatialRecordCount(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab") bool IsActualPresent(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab") int32 GetTotalProxyCount() const;
	UFUNCTION(BlueprintPure, Category="Lab") int32 GetTotalCapTriangles() const;
	UFUNCTION(BlueprintPure, Category="Lab") FString GetTelemetry() const;
	UFUNCTION(BlueprintPure, Category="Lab") FString GetMotionState() const;
	UFUNCTION(BlueprintPure, Category="Lab") FString GetCurrentInteraction() const;
	UFUNCTION(BlueprintPure, Category="Lab") FString GetObjectPositionLabel() const;
	UFUNCTION(BlueprintPure, Category="Lab") FVector GetPressurePlatePosition() const;
	UFUNCTION(BlueprintPure, Category="Lab") FTransform GetTrackedTransform(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab") bool IsInWorldControlMode() const { return bInWorldControls; }
	bool TryDuplicateStableIdForTesting(FName StableId);
	bool DoSpatialRecordTexturesMatchForTesting(FName StableId) const;
	bool ActivateInWorldControl(EDarkwellMovingPropLabControlKind Kind, ADarkwellCharacter& Character);
	bool CanActivateInWorldControl(EDarkwellMovingPropLabControlKind Kind) const;
	bool CanFocusInWorldControl() const { return bInWorldControls && bStarted; }
	bool IsInWorldControlCompleted(EDarkwellMovingPropLabControlKind Kind) const;
	FText GetInWorldControlPrompt(EDarkwellMovingPropLabControlKind Kind) const;
	FText GetInWorldControlDisplay(EDarkwellMovingPropLabControlKind Kind) const;
	FColor GetInWorldControlColor(EDarkwellMovingPropLabControlKind Kind) const;
	ADarkwellMovingPropLabControl* GetControlForTesting(EDarkwellMovingPropLabControlKind Kind) const;
	int32 GetHiddenFreezeCountForTesting(FName StableId) const;
	int32 GetHistoricalProxyVisibilityTransitionsForTesting(FName StableId) const;
	int32 GetHistoricalProxyCreationCountForTesting(FName StableId) const;
	int32 GetHistoricalTextureUploadCountForTesting(FName StableId) const;
	uint64 GetHistoricalVisualSignatureForTesting(FName StableId) const;
	FString GetHistoricalVisualTelemetryForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetCurrentEpochCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetStaleEpochCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetVisibleHistoricalProxyCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetVisibleHistoricalCapCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetHistoricalPresentationResourceCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetMaxOverlapContributorsForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetMaxSurfaceContributorsForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetMaxCapContributorsForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetMaxTotalContributorsForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetCurrent3DOverlapStaleSurfaceForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetCurrent3DOverlapStaleCapForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetMax3DRenderOwnershipContributorsForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetCurrentRenderContactStaleSurfaceForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetCurrentRenderContactStaleCapForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetHardOwnershipFilterLeakForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") FString Get3DOwnershipTelemetryForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") FString GetResidualFragmentTelemetryForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") FString GetCapLifecycleTelemetryForTesting(FName StableId) const;
	int32 GetMissingHistoricalCutCountForTesting(FName StableId) const;
	int32 GetCapVerticesOutsideSourceForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetFalseOccupiedHistoryCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") FString GetFalseOccupiedHistoryTelemetryForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetNewestHistoricalDiscoveredCellCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetNewestHistoricalCellCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") float GetLastLegalCoverageRatioForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") bool IsLastCoverageValidForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") FString GetLastCoverageZeroReasonForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int64 GetTransformRevisionForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int64 GetCoverageRevisionForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int64 GetCoverageTransformRevisionForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int64 GetCoverageGridRevisionForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetSealCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetObservationEpisodeForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") FString GetObservationStateForTesting(FName StableId) const;
	float GetNewestHistoricalYawForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") FString GetFineHistoryTelemetry(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") FString GetHistoryRuntimeTelemetry() const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") FString GetMultiEpochCompositeDiagnosis(FName StableId) const;
	FHistoryRuntimeTelemetry GetHistoryRuntimeFrameTelemetryForTesting() const { return RuntimeFrame; }
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
	void GetFineEvidenceDiagnosticsForTesting(FName StableId, TArray<FFineEvidenceDiagnostic>& Out) const;
	FHistoryRuntimeTelemetry GetHistoryRuntimeTotalTelemetryForTesting() const { return RuntimeTotal; }
	void ResetHistoryRuntimeTelemetryForTesting();
	bool ConfigureHistoricalEpochCountForTesting(FName StableId, int32 HistoricalEpochs);
	bool SetTrackedTransformForTesting(FName StableId, const FTransform& Transform);
	bool StartTrackedRotationForTesting(FName StableId, float TargetYaw, float Duration);
	bool InjectInvalidCoverageOnceForTesting(FName StableId);
	/** Explicit per-object reset/re-registration, never an in-place mode change. */
	UFUNCTION(BlueprintCallable, Category="Lab|History Policy")
	bool ResetTrackedPolicyForLab(FName StableId, ESightWeaveHistoryMode Mode);
	UFUNCTION(BlueprintCallable, Category="Lab|Object Policy")
	bool ResetTrackedRevealPolicyForLab(FName StableId, ESightWeaveRevealMode RevealMode, float MinimumSpanCm, ESightWeaveHistoryMode HistoryMode);
	UFUNCTION(BlueprintPure, Category="Lab|Object Policy")
	FString GetRevealPolicyTelemetry(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Object Policy")
	bool IsRevealConfirmedForTesting(FName StableId) const;
	bool GetNewestCaptureMasksForTesting(FName Id,TBitArray<>& Capture,TBitArray<>& Frozen) const;
	uint64 GetRevealSpanEvaluationsForTesting(FName Id) const;
 bool IsWholePresentationUniformForTesting(FName Id) const;
	float GetCurrentPresentationMinimumForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|History Policy")
	FString GetHistoryPolicyTelemetry(FName StableId) const;
	USightWeaveObjectPolicyComponent* GetObjectPolicyForTesting(FName StableId) const;
	bool IsCurrentSourceVisibleForTesting(FName StableId) const;
	bool CurrentHasOnlyLivePresentationForTesting(FName StableId) const;

	bool SelectScenario(int32 InScenario, ADarkwellCharacter* Player);
	bool AdvanceScenario(ADarkwellCharacter* Player);
	bool SetMultiCount(int32 Count, ADarkwellCharacter* Player);
	void StopMotion();

private:
	struct FPrimitiveGeometrySnapshot
	{
		FBox LocalBounds = FBox(ForceInit);
		FTransform WorldTransform = FTransform::Identity;
		int32 PrimitiveIndex = INDEX_NONE;
	};
	friend class FDarkwellCapPartialClipTest;
	friend class FDarkwellCapCoplanarContactTest;
	static TArray<FVector2D> SubtractOwnedCapIntervals(FVector2D Candidate, TConstArrayView<FVector2D> Owned);

	struct FCapQuadSnapshot
	{
		FVector A = FVector::ZeroVector;
		FVector B = FVector::ZeroVector;
		FVector C = FVector::ZeroVector;
		FVector D = FVector::ZeroVector;
		int32 PrimitiveIndex = INDEX_NONE;
	};

	struct FRecordVisual
	{
		uint32 Epoch = 0;
		TWeakObjectPtr<AActor> Proxy;
		TWeakObjectPtr<UTexture2D> Texture;
		TArray<TWeakObjectPtr<UTexture2D>> LiveTextures;
		TArray<TArray<FLinearColor>> LivePixels;
		TArray<uint64> LiveSignatures;
		int32 LiveTextureCreations=0,LiveTextureUploads=0;
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
		TBitArray<> CachedFineOccupied;
		TArray<FBox2D> CachedGeometryRegions;
		uint64 CachedCoverageAuthorityRevision = MAX_uint64;
		uint64 CachedCoverageDrawRevision = MAX_uint64;
		uint64 ProcessedGeometryRevision = 0;
		uint64 ProcessedOwnershipRevision = 0;
		float CoarseEvidenceActiveSeconds = 0.0f;
		bool bPresentationDirty = true;
		bool bCapTopologyDirty = true;
		bool bPresentationRetired = false;
		bool bHasProxyVisibilitySample = false;
		bool bLastProxyVisible = false;
	};

	enum class EObservationState : uint8
	{
		NeverObserved,
		ObservedArmed,
		UnobservedSealed
	};

	struct FTrackedProp
	{
		FName StableId;
		TWeakObjectPtr<ADarkwellPropLabFurniture> Actual;
		TWeakObjectPtr<USightWeaveObjectPolicyComponent> ObjectPolicy;
		uint64 ProcessedMovingRevision = 0;
		FDarkwellSpatialObservationHistory History;
		FDarkwellCurrentLiveGrid CurrentLive;
		FSightWeaveRevealObservation RevealObservation;
		TBitArray<> CurrentLegalObservationMask;
  bool bCachedWholeLegalContact=false;
		uint32 LocalEpoch=0;
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

	struct FActiveMotion
	{
		TWeakObjectPtr<ADarkwellPropLabFurniture> Prop;
		FTransform Start = FTransform::Identity;
		FTransform End = FTransform::Identity;
		float Seconds = 0.0f;
		float Duration = 0.0f;
	};

	ADarkwellPropLabFurniture* SpawnTracked(
		FName StableId,
		int32 Shape,
		FVector Dimensions,
		FLinearColor Tint,
		const FTransform& Transform,
		ESightWeaveObjectPolicySource PolicySource = ESightWeaveObjectPolicySource::UseProjectDefault,
		ESightWeaveHistoryMode HistoryMode = ESightWeaveHistoryMode::Always,
		const FResolvedSightWeaveObjectPolicy* PerFieldPolicy = nullptr);
	bool IsCaptureEligible(const FTrackedProp& Prop) const;
	void DestroyTracked();
	void DestroyTracked(FName StableId);
	void DestroyVisual(FRecordVisual& Visual, bool bDiscardEvidence = true);
	void ConfigureInWorldProps();
	void SpawnInWorldControls();
	void DestroyInWorldControls();
	bool ResetCurrentInWorldZone();
	void ResetInWorldControlState();
	bool IsCurrentInWorldControlBusy() const;
	void MarkActiveInWorldControlCompleted();
	FString GetNextInWorldControlLabel() const;
	bool FreezeCurrentForHiddenMotion(FTrackedProp& Prop, const TCHAR* Reason);
	void AbandonCurrentObservationWithoutHistory(FTrackedProp& Prop);
	void StartMotion(ADarkwellPropLabFurniture* Prop, const FTransform& Target, float Duration);
	void UpdateInWorldAutomation(float DeltaSeconds, ADarkwellCharacter* Player);
	void UpdatePressurePlate(ADarkwellCharacter* Player);
	void CompleteInWorldMotionGroup();
	FName GetInWorldPropId(EDarkwellMovingPropLabControlKind Kind) const;
	FBox2D ActualBounds(const ADarkwellPropLabFurniture& Prop) const;
	TArray<FBox> ActualPartBounds(const ADarkwellPropLabFurniture& Prop) const;
	TArray<FPrimitiveGeometrySnapshot> ActualPartGeometry(
		const ADarkwellPropLabFurniture& Prop) const;
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
		TArray<int32>& OutDirtyIndices);
	bool IsHistoricalPresentationResolved(
		const FDarkwellSpatialObservationRecord& Record,
		const FRecordVisual& Visual) const;
	void RetireHistoricalPresentation(FTrackedProp& Prop, FRecordVisual& Visual);
	void RefreshContributionDiagnostics(FTrackedProp& Prop) const;
	void LogRotationFrame(const FTrackedProp& Prop) const;
	void UpdateTracked(FTrackedProp& Prop, float DeltaSeconds);
	bool SetTrackedExists(FName StableId, bool bExists);
	void EnsureRecordVisual(FTrackedProp& Prop, FDarkwellSpatialObservationRecord& Record);
	void UpdateRecordTexture(FTrackedProp& Prop, FDarkwellSpatialObservationRecord& Record);
	void UpdateCurrentPartTextures(FTrackedProp& Prop, FRecordVisual& Visual);
	void UpdateRecordCap(FTrackedProp& Prop, FDarkwellSpatialObservationRecord& Record);
	AActor* SpawnMemoryProxy(const FTrackedProp& Prop, const FDarkwellSpatialObservationRecord& Record);
	void BindProxyMaterial(FTrackedProp& Prop, FDarkwellSpatialObservationRecord& Record, AActor* Proxy);
	void TeleportPlayer(ADarkwellCharacter* Player, FVector Location, float Yaw) const;
	void ConfigureScenarioProps(int32 InScenario);
	void UpdateDeterministicMotion(float DeltaSeconds);
	void Report();
	void FinalizeHistoryRuntimeTelemetry(uint64 UpdateRoomStartCycles);

	UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> Structure;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PressurePlate;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> PressureLabel;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> OwnedMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> OwnedTextures;
	UPROPERTY(Transient) TArray<TObjectPtr<UDynamicMeshComponent>> OwnedCaps;
	TMap<FName, FTrackedProp> Tracked;
	UPROPERTY(Transient) TArray<TObjectPtr<ADarkwellMovingPropLabControl>> InWorldControls;
	TArray<FActiveMotion> ActiveMotions;
	TSet<EDarkwellMovingPropLabControlKind> CompletedInWorldControls;
	FString Status;
	FString CurrentInteraction = TEXT("NONE");
	EDarkwellMovingPropLabControlKind ActiveControl = EDarkwellMovingPropLabControlKind::VisibleTranslate;
	float AutoDelaySeconds = 0.0f;
	int32 Scenario = 0;
	int32 ScenarioPhase = 0;
	int32 MultiCount = 0;
	int32 HiddenMoveIndex = 0;
	bool bMotionActive = false;
	bool bStarted = false;
	bool bInWorldControls = false;
	bool bInWorldScenarioSelected = false;
	bool bInWorldFinished = false;
	bool bPressureWaitingForExit = false;
	mutable FHistoryRuntimeTelemetry RuntimeFrame;
	mutable FHistoryRuntimeTelemetry RuntimeTotal;
	uint64 RuntimeFrameSequence = 0;
	uint64 GeometryRevision = 1;
	uint64 OwnershipRevision = 1;
};
