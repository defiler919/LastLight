#pragma once

#include "CoreMinimal.h"
#include "SightWeavePresentation.h"
#include "SightWeaveSparseAtlas.h"
#include "Subsystems/WorldSubsystem.h"

#include "SightWeaveRenderWorldSubsystem.generated.h"

class FSightWeaveSceneViewExtension;
class FSightWeaveMemoryPacket;
struct FSightWeaveFrameSnapshot;

enum class ESightWeaveRenderAvailability : uint8
{
	Unknown,
	Available,
	NullRHI,
	UnsupportedRHI,
	UnsupportedPixelFormat,
	ConservativeRasterUnavailable,
	ResourceAllocationFailed,
	InvalidPacket,
	WorldTeardown
};

/** Game-thread diagnostics. GPU/readback timings are test-only and reported separately. */
struct SIGHTWEAVERENDER_API FSightWeaveRenderWorldDiagnostics
{
	uint64 PublishedPacketCount = 0;
	uint64 FailClosedClearCount = 0;
	uint64 LastSubmittedPacketRevision = 0;
	uint64 LastSubmittedSnapshotRevision = 0;
	uint64 SubmittedDirtyTileCount = 0;
	uint64 SubmittedRemovedTileCount = 0;
	int32 LastDesiredTileCount = 0;
	int32 LastMaximumActiveTiles = 0;
	uint64 PresentationSelectionRevision = 0;
	bool bPresentationEnabled = false;
	ESightWeaveSparsePacketFailure LastBuildFailure = ESightWeaveSparsePacketFailure::None;
};

/**
 * Owns exactly one SightWeave render lifetime for one UWorld. It reacts to
 * immutable Runtime publication; it never polls gameplay and never ticks.
 */
UCLASS()
class SIGHTWEAVERENDER_API USightWeaveRenderWorldSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	FSightWeaveRenderWorldIdentity GetWorldIdentity() const { return WorldIdentity; }
	const FSightWeaveRenderWorldDiagnostics& GetDiagnostics() const { return Diagnostics; }
	bool HasSceneViewExtension() const { return SceneViewExtension.IsValid(); }
	const FSightWeaveViewPresentationSelection& GetPresentationSelection() const
	{
		return PresentationSelection;
	}
	bool SetPresentationScope(
		FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
		FSightWeaveFloorId FloorId,
		ESightWeaveRenderPrecisionTier PrecisionTier = ESightWeaveRenderPrecisionTier::Standard);
	void ClearPresentationScope();

private:
	void HandleSnapshotPublished(
		TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> Snapshot);
	void HandleMemoryPacketPublished(
		TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> Packet);
	void BuildAndSubmitPacket(
		const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe>& Snapshot);
	void SubmitFailClosedClear(uint64 SnapshotRevision, ESightWeaveSparsePacketFailure Failure);
	void SubmitPacket(TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet);
	void UpdateDefaultPresentationSelection(const FSightWeaveFrameSnapshot& Snapshot);
	void PublishPresentationSelection();

	FSightWeaveRenderWorldIdentity WorldIdentity;
	uint64 NextPacketRevision = 1;
	uint64 NextPresentationRevision = 1;
	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> LastPacket;
	FSightWeaveViewPresentationSelection PresentationSelection;
	FSightWeaveKnowledgeOwnerId ExplicitPresentationOwner;
	FSightWeaveFloorId ExplicitPresentationFloor;
	ESightWeaveRenderPrecisionTier ExplicitPresentationPrecision =
		ESightWeaveRenderPrecisionTier::Standard;
	bool bHasExplicitPresentationScope = false;
	FDelegateHandle SnapshotPublishedHandle;
	FDelegateHandle MemoryPacketPublishedHandle;
	TSharedPtr<FSightWeaveSceneViewExtension, ESPMode::ThreadSafe> SceneViewExtension;
	FSightWeaveRenderWorldDiagnostics Diagnostics;
};
