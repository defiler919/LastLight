#pragma once

#include "CoreMinimal.h"
#include "SightWeaveRenderWorldSubsystem.h"
#include "SightWeaveSparseAtlas.h"

#if WITH_DEV_AUTOMATION_TESTS

enum class ESightWeaveSparseReadbackStatus : uint8
{
	Pending,
	Complete,
	Failed,
	DiscardedStale
};

struct SIGHTWEAVERENDER_API FSightWeaveSparseReadbackExpectation
{
	FSightWeaveSparseTileIdentity TileIdentity;
	uint64 PacketRevision = 0;
};

struct SIGHTWEAVERENDER_API FSightWeaveSparseUpdateSample
{
	uint64 PacketRevision = 0;
	int32 RequestedDirtyTileCount = 0;
	bool bProducedMaskWork = false;
	uint64 DirtyTileDispatchDelta = 0;
	uint64 ResourceGeneration = 0;
	uint64 PageAllocationCount = 0;
	uint64 ScratchAllocationCount = 0;
	uint64 EvictionCount = 0;
	int32 ResidentTileCount = 0;
	int32 AllocatedPageCount = 0;
	double GameThreadSubmitMicroseconds = 0.0;
	double RenderThreadPacketConsumeMicroseconds = 0.0;
	double RenderThreadDirtySchedulingMicroseconds = 0.0;
	double RenderThreadRDGSetupMicroseconds = 0.0;
	double TileClearSetupMicroseconds = 0.0;
	double RasterSetupMicroseconds = 0.0;
	double PublicationMicroseconds = 0.0;
};

struct SIGHTWEAVERENDER_API FSightWeaveSparseReadbackResult
{
	ESightWeaveSparseReadbackStatus Status = ESightWeaveSparseReadbackStatus::Pending;
	ESightWeaveRenderAvailability Availability = ESightWeaveRenderAvailability::Unknown;
	FSightWeaveSparseReadbackExpectation Identity;
	FSightWeaveSparsePhysicalAddress PhysicalAddress;
	uint64 MaskHash = 0;
	uint64 DuplicatePacketCount = 0;
	uint64 StalePacketCount = 0;
	uint64 RejectedPacketCount = 0;
	int32 Width = 0;
	int32 Height = 0;
	int32 RowPitchInPixels = 0;
	int32 BufferHeight = 0;
	int32 ZeroTexelCount = 0;
	int32 WhiteTexelCount = 0;
	int32 NonBinaryTexelCount = 0;
	bool bGPUTimestampAvailable = false;
	double GPUWorkMicroseconds = 0.0;
	double ReadbackEndToEndMicroseconds = 0.0;
	FString Failure;
	TArray<FSightWeaveSparseUpdateSample> Updates;
	TArray<uint8> Pixels;
};

/** Test-only persistent-state sequence plus asynchronous selected-slot readback. */
class SIGHTWEAVERENDER_API FSightWeaveSparseAtlasTestReadback final
	: public TSharedFromThis<FSightWeaveSparseAtlasTestReadback, ESPMode::ThreadSafe>
{
public:
	/** Measures GT ownership capture/enqueue only; commands are no-op and flushed outside samples. */
	static TArray<double> BenchmarkGameThreadSubmitMicroseconds(
		TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet,
		int32 WarmupCount,
		int32 SampleCount);
	static TSharedRef<FSightWeaveSparseAtlasTestReadback, ESPMode::ThreadSafe> StartSequence(
		TArray<TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe>> Packets,
		FSightWeaveSparseTileIdentity SelectedTile);

	void Poll();
	bool TryTakeResult(
		const FSightWeaveSparseReadbackExpectation& CurrentExpectation,
		FSightWeaveSparseReadbackResult& OutResult);
	bool IsFinished() const;

private:
	struct FState;
	explicit FSightWeaveSparseAtlasTestReadback(TSharedRef<FState, ESPMode::ThreadSafe> InState);

	TSharedRef<FState, ESPMode::ThreadSafe> State;
};

#endif
