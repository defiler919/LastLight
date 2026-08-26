#pragma once

#include "RenderGraphResources.h"
#include "SightWeavePresentation.h"
#include "SightWeaveRenderWorldSubsystem.h"
#include "SightWeaveSparseAtlas.h"

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
	bool ProcessPending_RenderThread(FRDGBuilder& GraphBuilder);
	void PreparePresentationResources_RenderThread(FRDGBuilder& GraphBuilder);
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
	uint64 GetResidencyGeneration_RenderThread() const { return ResidencyGeneration; }
	uint64 GetPageTableUploadCount_RenderThread() const { return PageTableUploadCount; }
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
	FRDGTextureRef RegisterResidentPageForReadback_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSightWeaveSparseTileIdentity& Identity,
		FIntRect& OutSlotRect,
		FSightWeaveSparsePhysicalAddress& OutAddress);
	bool AddReadback_RenderThread(const FSightWeaveSparseTileIdentity& Identity);
	bool RemoveReadback_RenderThread(
		const FSightWeaveSparseTileIdentity& Identity,
		const FSightWeaveSparsePhysicalAddress& Address);
#endif

private:
	struct FScopeState;

	bool CheckCapabilities_RenderThread();
	bool EnsureScratchTextures_RenderThread();
	bool EnsurePage_RenderThread(
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

	FSightWeaveRenderWorldIdentity WorldIdentity;
	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> PendingPacket;
	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> AppliedPacket;
	FSightWeaveViewPresentationSelection PresentationSelection;
	TSharedPtr<const FSightWeaveViewPresentationBinding, ESPMode::ThreadSafe> PresentationBinding;
	ESightWeavePresentationBindingFailure PresentationBindingFailure =
		ESightWeavePresentationBindingFailure::Disabled;
	TArray<TUniquePtr<FScopeState>> Scopes;
	TRefCountPtr<IPooledRenderTarget> VisionScratch;
	TRefCountPtr<IPooledRenderTarget> IlluminationScratch;
	TRefCountPtr<IPooledRenderTarget> SuppressionScratch;
	uint64 DesiredRevision = 0;
	uint64 DesiredHash = 0;
	uint64 AppliedRevision = 0;
	uint64 DirtyTileDispatchCount = 0;
	uint64 ResourceGeneration = 0;
	uint64 ResidencyGeneration = 1;
	uint64 PageTableUploadCount = 0;
	uint64 PageAllocationCount = 0;
	uint64 ScratchAllocationCount = 0;
	uint64 DuplicatePacketCount = 0;
	uint64 StalePacketCount = 0;
	uint64 RejectedPacketCount = 0;
	ESightWeaveRenderAvailability Availability = ESightWeaveRenderAvailability::Unknown;
	bool bPendingForceBlack = false;
	bool bReleased = false;
#if WITH_DEV_AUTOMATION_TESTS
	FSightWeaveSparseRenderTimings LastTimings;
#endif
};
