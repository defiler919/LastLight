#pragma once

#include "CoreMinimal.h"
#include "SightWeaveRenderPacket.h"
#include "Subsystems/WorldSubsystem.h"

#include "SightWeaveRenderWorldSubsystem.generated.h"

class FSightWeaveSceneViewExtension;
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
	ESightWeaveRenderPacketFailure LastBuildFailure = ESightWeaveRenderPacketFailure::None;
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

private:
	void HandleSnapshotPublished(
		TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> Snapshot);
	void BuildAndSubmitPacket(
		const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe>& Snapshot);
	void SubmitFailClosedClear(uint64 SnapshotRevision, ESightWeaveRenderPacketFailure Failure);
	void SubmitPacket(TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe> Packet);

	FSightWeaveRenderWorldIdentity WorldIdentity;
	uint64 NextPacketRevision = 1;
	FSightWeaveKnowledgeOwnerId LastKnowledgeOwnerId;
	FSightWeaveFloorId LastFloorId;
	FSightWeaveRenderProfileIdentity LastProfile;
	FIntPoint LastTileCoordinate = FIntPoint::ZeroValue;
	FBox2D LastPhysicalWorldBounds = FBox2D(ForceInit);
	FDelegateHandle SnapshotPublishedHandle;
	TSharedPtr<FSightWeaveSceneViewExtension, ESPMode::ThreadSafe> SceneViewExtension;
	FSightWeaveRenderWorldDiagnostics Diagnostics;
};
