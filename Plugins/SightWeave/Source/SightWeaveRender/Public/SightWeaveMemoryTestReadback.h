#pragma once

#include "CoreMinimal.h"
#include "SightWeaveMemory.h"
#include "SightWeaveRenderWorldSubsystem.h"
#include "SightWeaveSparseAtlas.h"

#if WITH_DEV_AUTOMATION_TESTS

enum class ESightWeaveMemoryReadbackStatus : uint8
{
	Pending,
	Complete,
	Failed
};

struct SIGHTWEAVERENDER_API FSightWeaveMemoryMirrorUpdateSample
{
	uint64 PacketRevision = 0;
	uint64 MemoryRevision = 0;
	uint64 UploadDelta = 0;
	uint64 UploadCount = 0;
	uint64 PageTableUploadCount = 0;
	uint64 ResourceGeneration = 0;
	uint64 ResidencyGeneration = 0;
	int32 ResidentTileCount = 0;
	int32 AllocatedPageCount = 0;
	bool bProducedMirrorWork = false;
};

struct SIGHTWEAVERENDER_API FSightWeaveMemoryReadbackResult
{
	ESightWeaveMemoryReadbackStatus Status = ESightWeaveMemoryReadbackStatus::Pending;
	ESightWeaveRenderAvailability Availability = ESightWeaveRenderAvailability::Unknown;
	FSightWeaveMemoryTileKey TileKey;
	FSightWeaveSparsePhysicalAddress PhysicalAddress;
	int32 Width = 0;
	int32 Height = 0;
	int32 RowPitchInPixels = 0;
	int32 BufferHeight = 0;
	int32 ZeroTexelCount = 0;
	int32 WhiteTexelCount = 0;
	int32 NonBinaryTexelCount = 0;
	FString Failure;
	TArray<FSightWeaveMemoryMirrorUpdateSample> Updates;
	TArray<uint8> Pixels;
};

/** Test-only packet sequence, resource counters, and asynchronous PF_G8 slot readback. */
class SIGHTWEAVERENDER_API FSightWeaveMemoryTestReadback final
	: public TSharedFromThis<FSightWeaveMemoryTestReadback, ESPMode::ThreadSafe>
{
public:
	static TSharedRef<FSightWeaveMemoryTestReadback, ESPMode::ThreadSafe> StartSequence(
		TArray<TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe>> Packets,
		FSightWeaveMemoryTileKey SelectedTile);

	void Poll();
	bool TryTakeResult(FSightWeaveMemoryReadbackResult& OutResult);
	bool IsFinished() const;

private:
	struct FState;
	explicit FSightWeaveMemoryTestReadback(TSharedRef<FState, ESPMode::ThreadSafe> InState);

	TSharedRef<FState, ESPMode::ThreadSafe> State;
};

#endif
