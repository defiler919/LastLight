#pragma once

#include "RenderGraphResources.h"
#include "SightWeaveMemory.h"
#include "SightWeavePresentation.h"
#include "SightWeaveRenderWorldSubsystem.h"
#include "SightWeaveSparseAtlas.h"
#include "SightWeaveStaticEnvironment.h"

class FRDGBuilder;
class FSceneView;
struct FPostProcessMaterialInputs;
struct FScreenPassTexture;
class FRDGPooledBuffer;
struct IPooledRenderTarget;

#if WITH_DEV_AUTOMATION_TESTS
struct FSightWeaveSparseRenderTimings
{
	double PacketConsumeMicroseconds = 0.0;
	double DirtySchedulingMicroseconds = 0.0;
	double RDGSetupMicroseconds = 0.0;
	double TileClearSetupMicroseconds = 0.0;
	double RasterSetupMicroseconds = 0.0;
	double PublicationMicroseconds = 0.0;
};
#endif

/** Render-thread owner for one world's persistent, scope-partitioned sparse PF_G8 atlas. */
class FSightWeaveSparseAtlasRenderState final
{
public:
	explicit FSightWeaveSparseAtlasRenderState(FSightWeaveRenderWorldIdentity InWorldIdentity);
	~FSightWeaveSparseAtlasRenderState();

	void SubmitPacket_RenderThread(
		const TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe>& Packet);
	void SubmitPresentationSelection_RenderThread(
		const FSightWeaveViewPresentationSelection& Selection);
	void SubmitMemoryPacket_RenderThread(
		const TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe>& Packet);
	void SubmitStaticEnvironmentPacket_RenderThread(
		const TSharedPtr<const FSightWeaveStaticEnvironmentPacket, ESPMode::ThreadSafe>& Packet);
	bool ProcessPending_RenderThread(FRDGBuilder& GraphBuilder);
	bool ProcessMemoryPending_RenderThread(FRDGBuilder& GraphBuilder);
	bool ProcessStaticEnvironmentPending_RenderThread(FRDGBuilder& GraphBuilder);
	bool ProcessVisualFeather_RenderThread(FRDGBuilder& GraphBuilder);
	void PreparePresentationResources_RenderThread(FRDGBuilder& GraphBuilder);
	void PrepareMemoryPresentationResources_RenderThread(FRDGBuilder& GraphBuilder);
	void PrepareStaticEnvironmentPresentationResources_RenderThread(FRDGBuilder& GraphBuilder);
	FScreenPassTexture AddHardMaskComposite_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FPostProcessMaterialInputs& Inputs);
	void Release_RenderThread(FSightWeaveRenderWorldIdentity ExpectedWorldIdentity);

	ESightWeaveRenderAvailability GetAvailability_RenderThread() const { return Availability; }
	uint64 GetDesiredRevision_RenderThread() const { return DesiredRevision; }
	uint64 GetAppliedRevision_RenderThread() const { return AppliedRevision; }
	uint64 GetDirtyTileDispatchCount_RenderThread() const { return DirtyTileDispatchCount; }
	uint64 GetResourceGeneration_RenderThread() const { return ResourceGeneration; }
	uint64 GetPageAllocationCount_RenderThread() const { return PageAllocationCount; }
	uint64 GetScratchAllocationCount_RenderThread() const { return ScratchAllocationCount; }
	uint64 GetDuplicatePacketCount_RenderThread() const { return DuplicatePacketCount; }
	uint64 GetStalePacketCount_RenderThread() const { return StalePacketCount; }
	uint64 GetRejectedPacketCount_RenderThread() const { return RejectedPacketCount; }
	uint64 GetEvictionCount_RenderThread() const;
	int32 GetResidentTileCount_RenderThread() const;
	int32 GetAllocatedPageCount_RenderThread() const;
	int32 GetAllocatedFeatherPageCount_RenderThread() const;
	uint64 GetResidencyGeneration_RenderThread() const { return ResidencyGeneration; }
	uint64 GetPageTableUploadCount_RenderThread() const { return PageTableUploadCount; }
	uint64 GetFeatherResourceGeneration_RenderThread() const { return FeatherResourceGeneration; }
	uint64 GetFeatherPageAllocationCount_RenderThread() const { return FeatherPageAllocationCount; }
	uint64 GetFeatherScratchAllocationCount_RenderThread() const { return FeatherScratchAllocationCount; }
	uint64 GetFeatherTileDispatchCount_RenderThread() const { return FeatherTileDispatchCount; }
	ESightWeaveRenderAvailability GetMemoryAvailability_RenderThread() const;
	uint64 GetMemoryAppliedRevision_RenderThread() const;
	uint64 GetMemoryResourceGeneration_RenderThread() const;
	uint64 GetMemoryResidencyGeneration_RenderThread() const;
	uint64 GetMemoryUploadCount_RenderThread() const;
	uint64 GetMemoryPageTableUploadCount_RenderThread() const;
	int32 GetMemoryResidentTileCount_RenderThread() const;
	int32 GetAllocatedMemoryPageCount_RenderThread() const;
	ESightWeaveRenderAvailability GetStaticEnvironmentAvailability_RenderThread() const;
	uint64 GetStaticEnvironmentAppliedRevision_RenderThread() const;
	uint64 GetStaticEnvironmentUploadCount_RenderThread() const;
	int32 GetStaticEnvironmentResidentTileCount_RenderThread() const;
	int32 GetAllocatedStaticEnvironmentPageCount_RenderThread() const;
	bool IsPresentationEnabled_RenderThread() const
	{
		return PresentationSelection.IsEnabled() && !bReleased;
	}
	TSharedPtr<const FSightWeaveViewPresentationBinding, ESPMode::ThreadSafe>
		GetPresentationBinding_RenderThread() const { return PresentationBinding; }
	ESightWeavePresentationBindingFailure GetPresentationBindingFailure_RenderThread() const
	{
		return PresentationBindingFailure;
	}

#if WITH_DEV_AUTOMATION_TESTS
	const FSightWeaveSparseRenderTimings& GetLastTimings_RenderThread() const { return LastTimings; }
	FRDGTextureRef AddPresentationTestComposite_RenderThread(
		FRDGBuilder& GraphBuilder,
		TConstArrayView<FVector2f> TranslatedWorldPositions,
		TConstArrayView<FVector4f> SceneColors);
	FRDGTextureRef AddMemoryPresentationTestComposite_RenderThread(
		FRDGBuilder& GraphBuilder,
		TConstArrayView<FVector2f> TranslatedWorldPositions,
		TConstArrayView<FVector4f> SceneColors,
		bool bForceMemoryUnavailable = false);
	FRDGTextureRef AddPresentationBenchmarkComposite_RenderThread(
		FRDGBuilder& GraphBuilder,
		FIntPoint OutputExtent,
		FVector2f TestWorldMin,
		FVector2f TestWorldStep);
	FRDGTextureRef RegisterResidentPageForReadback_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSightWeaveSparseTileIdentity& Identity,
		FIntRect& OutSlotRect,
		FSightWeaveSparsePhysicalAddress& OutAddress);
	bool AddReadback_RenderThread(const FSightWeaveSparseTileIdentity& Identity);
	bool RemoveReadback_RenderThread(
		const FSightWeaveSparseTileIdentity& Identity,
		const FSightWeaveSparsePhysicalAddress& Address);
	FRDGTextureRef RegisterMemoryPageForReadback_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSightWeaveMemoryTileKey& TileKey,
		FIntRect& OutSlotRect,
		FSightWeaveSparsePhysicalAddress& OutAddress);
	bool AddMemoryReadback_RenderThread(const FSightWeaveMemoryTileKey& TileKey);
	bool RemoveMemoryReadback_RenderThread(
		const FSightWeaveMemoryTileKey& TileKey,
		const FSightWeaveSparsePhysicalAddress& Address);
#endif

private:
	struct FScopeState;
	struct FMemoryMirrorState;
	struct FStaticAttributeMirrorState;

	bool CheckCapabilities_RenderThread();
	bool EnsureScratchTextures_RenderThread();
	bool EnsureFeatherScratchTextures_RenderThread();
	bool EnsurePage_RenderThread(
		FRDGBuilder& GraphBuilder,
		FScopeState& Scope,
		int32 PageIndex,
		FRDGTextureRef& OutPage,
		bool& bOutColdCreated);
	bool EnsureFeatherPage_RenderThread(
		FRDGBuilder& GraphBuilder,
		FScopeState& Scope,
		int32 PageIndex,
		FRDGTextureRef& OutPage,
		bool& bOutColdCreated);
	void AddTilePasses_RenderThread(
		FRDGBuilder& GraphBuilder,
		FRDGTextureRef Page,
		const FIntRect& SlotRect,
		const FSightWeaveSparseRenderTile& Tile);
	bool AddFeatherTilePasses_RenderThread(
		FRDGBuilder& GraphBuilder,
		FScopeState& Scope,
		const FSightWeaveSparseRenderTile& Tile,
		const FSightWeaveSparsePhysicalAddress& Address);
	void MarkFeatherDirtyAround_RenderThread(const FSightWeaveSparseTileKey& TileKey);
	void ReleaseFeatherResources_RenderThread();
	void InvalidateFeather_RenderThread(ESightWeavePresentationBindingFailure Failure);
	FScopeState* FindScope_RenderThread(const FSightWeaveSparseScopeKey& ScopeKey);
	const FScopeState* FindScope_RenderThread(const FSightWeaveSparseScopeKey& ScopeKey) const;
	FScopeState& FindOrAddScope_RenderThread(const FSightWeaveSparseRenderScope& Scope);
	void FailScope_RenderThread(FScopeState& Scope, ESightWeaveRenderAvailability Failure);
	void FailAllScopes_RenderThread(ESightWeaveRenderAvailability Failure);
	void RemoveAbsentScopes_RenderThread(const FSightWeaveSparseRenderPacket& Packet);
	void RefreshPresentationBinding_RenderThread();
	bool HasCompletePresentationResidency_RenderThread(
		const FSightWeaveSparseRenderPacket& Packet,
		const FSightWeaveSparseScopeKey& ScopeKey) const;
	bool PrepareScopePageTable_RenderThread(FRDGBuilder& GraphBuilder, FScopeState& Scope);
	void ReportCompositeDiagnostic_RenderThread(
		int32 DiagnosticCode,
		const TCHAR* DiagnosticName,
		const FScopeState* Scope);
	void ReportMemoryPresentationDiagnostic_RenderThread(
		int32 CompositeDiagnosticCode,
		bool bMemoryReady,
		bool bStaticEnvironmentReady,
		bool bMemoryScopeValid,
		bool bMemoryScopeMatchesBinding,
		bool bStaticScopeMatchesMemory);
	bool CheckMemoryCapabilities_RenderThread();
	bool EnsureMemoryPage_RenderThread(
		FRDGBuilder& GraphBuilder,
		int32 PageIndex,
		FRDGTextureRef& OutPage,
		bool& bOutColdCreated);
	bool PrepareMemoryPageTable_RenderThread(FRDGBuilder& GraphBuilder);
	void FailMemoryMirror_RenderThread(ESightWeaveRenderAvailability Failure);
	bool EnsureStaticEnvironmentPage_RenderThread(
		FRDGBuilder& GraphBuilder,
		int32 PageIndex,
		FRDGTextureRef& OutPage,
		bool& bOutColdCreated);
	bool PrepareStaticEnvironmentPageTable_RenderThread(FRDGBuilder& GraphBuilder);
	void FailStaticEnvironmentMirror_RenderThread(ESightWeaveRenderAvailability Failure);

	FSightWeaveRenderWorldIdentity WorldIdentity;
	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> PendingPacket;
	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> AppliedPacket;
	FSightWeaveViewPresentationSelection PresentationSelection;
	TSharedPtr<const FSightWeaveViewPresentationBinding, ESPMode::ThreadSafe> PresentationBinding;
	ESightWeavePresentationBindingFailure PresentationBindingFailure =
		ESightWeavePresentationBindingFailure::Disabled;
	TArray<TUniquePtr<FScopeState>> Scopes;
	TUniquePtr<FMemoryMirrorState> MemoryMirror;
	TUniquePtr<FStaticAttributeMirrorState> StaticAttributeMirror;
	TRefCountPtr<IPooledRenderTarget> VisionScratch;
	TRefCountPtr<IPooledRenderTarget> IlluminationScratch;
	TRefCountPtr<IPooledRenderTarget> SuppressionScratch;
	TRefCountPtr<IPooledRenderTarget> FeatherScratchA;
	TRefCountPtr<IPooledRenderTarget> FeatherScratchB;
	TArray<FSightWeaveSparseTileKey> FeatherDirtyCenters;
	uint64 DesiredRevision = 0;
	uint64 DesiredHash = 0;
	uint64 AppliedRevision = 0;
	uint64 DirtyTileDispatchCount = 0;
	uint64 ResourceGeneration = 0;
	uint64 ResidencyGeneration = 1;
	uint64 PageTableUploadCount = 0;
	uint64 PageAllocationCount = 0;
	uint64 ScratchAllocationCount = 0;
	uint64 FeatherResourceGeneration = 1;
	uint64 FeatherPageAllocationCount = 0;
	uint64 FeatherScratchAllocationCount = 0;
	uint64 FeatherTileDispatchCount = 0;
	uint64 DuplicatePacketCount = 0;
	uint64 StalePacketCount = 0;
	uint64 RejectedPacketCount = 0;
	ESightWeaveRenderAvailability Availability = ESightWeaveRenderAvailability::Unknown;
	bool bPendingForceBlack = false;
	bool bPendingRequiresFullRebuild = false;
	bool bFeatherFullRebuildPending = false;
	bool bFeatherUpdateIncomplete = false;
	bool bReleased = false;
	int32 LastCompositeDiagnosticCode = INDEX_NONE;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	uint64 LastMaskUpdateFrame = MAX_uint64;
	uint32 LastSubmittedTileCount = 0;
	bool bLastMaskUpdateWasFullRebuild = false;
#endif
	struct FMemoryPresentationDiagnosticSnapshot
	{
		int32 CompositeDiagnosticCode = INDEX_NONE;
		ESightWeaveRenderAvailability MemoryAvailability = ESightWeaveRenderAvailability::Unknown;
		ESightWeaveRenderAvailability StaticEnvironmentAvailability =
			ESightWeaveRenderAvailability::Unknown;
		uint64 MemoryPacketRevision = 0;
		uint64 StaticPacketRevision = 0;
		uint64 StaticEligibilityRevision = 0;
		uint64 MemoryResourceGeneration = 0;
		uint64 StaticResourceGeneration = 0;
		uint64 MemoryResidencyGeneration = 0;
		uint64 StaticResidencyGeneration = 0;
		int32 MemoryPageTableEntryCount = 0;
		int32 StaticPageTableEntryCount = 0;
		int32 MemoryResidentTileCount = 0;
		int32 StaticResidentTileCount = 0;
		uint32 MemoryScopeMismatchMask = MAX_uint32;
		ESightWeaveRenderPrecisionTier MemoryPrecisionTier =
			ESightWeaveRenderPrecisionTier::Standard;
		ESightWeaveRenderPrecisionTier LivePrecisionTier =
			ESightWeaveRenderPrecisionTier::Standard;
		bool bMemoryReady = false;
		bool bStaticEnvironmentReady = false;
		bool bMemoryPresentationAvailable = false;
		bool bMemoryScopeValid = false;
		bool bMemoryScopeMatchesBinding = false;
		bool bStaticScopeMatchesMemory = false;
		bool bInitialized = false;

		bool IsEquivalentTo(const FMemoryPresentationDiagnosticSnapshot& Other) const;
	};
	FMemoryPresentationDiagnosticSnapshot LastMemoryPresentationDiagnostic;
#if WITH_DEV_AUTOMATION_TESTS
	FSightWeaveSparseRenderTimings LastTimings;
#endif
};
