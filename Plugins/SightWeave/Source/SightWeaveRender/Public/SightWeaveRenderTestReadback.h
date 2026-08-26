#pragma once

#include "CoreMinimal.h"
#include "SightWeaveRenderPacket.h"
#include "SightWeaveRenderWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

enum class ESightWeaveRenderReadbackStatus : uint8
{
	Pending,
	Complete,
	Failed,
	DiscardedStale
};

struct SIGHTWEAVERENDER_API FSightWeaveRenderReadbackExpectation
{
	FSightWeaveRenderWorldIdentity WorldIdentity;
	FSightWeaveKnowledgeOwnerId KnowledgeOwnerId;
	FSightWeaveFloorId FloorId;
	FSightWeaveRenderProfileIdentity CompatibilityProfile;
	uint64 PacketRevision = 0;
};

struct SIGHTWEAVERENDER_API FSightWeaveRenderReadbackResult
{
	ESightWeaveRenderReadbackStatus Status = ESightWeaveRenderReadbackStatus::Pending;
	ESightWeaveRenderAvailability Availability = ESightWeaveRenderAvailability::Unknown;
	FSightWeaveRenderReadbackExpectation Identity;
	uint64 RegistryRevision = 0;
	uint64 PublishedSnapshotRevision = 0;
	uint64 PacketContentHash = 0;
	uint64 MaskHash = 0;
	uint64 ResourceGeneration = 0;
	uint64 RasterDispatchCount = 0;
	FIntPoint TileCoordinate = FIntPoint::ZeroValue;
	int32 Width = 0;
	int32 Height = 0;
	int32 RowPitchInPixels = 0;
	int32 BufferHeight = 0;
	int32 ZeroTexelCount = 0;
	int32 WhiteTexelCount = 0;
	int32 NonBinaryTexelCount = 0;
	bool bPF_G8Texture2D = false;
	bool bPF_G8RenderTarget = false;
	bool bPF_G8ShaderResource = false;
	bool bPF_G8UAV = false;
	FString PixelFormatName;
	FString Failure;
	TArray<uint8> Pixels;
};

/**
 * Test-only one-shot asynchronous GPU readback. The API is compiled out when
 * WITH_DEV_AUTOMATION_TESTS is false and never participates in CPU Authority.
 */
class SIGHTWEAVERENDER_API FSightWeaveRenderTestReadback final
	: public TSharedFromThis<FSightWeaveRenderTestReadback, ESPMode::ThreadSafe>
{
public:
	static TSharedRef<FSightWeaveRenderTestReadback, ESPMode::ThreadSafe> Start(
		TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe> Packet);
	/** Test-only ordered submissions into one render state; metadata follows ResultPacketIndex. */
	static TSharedRef<FSightWeaveRenderTestReadback, ESPMode::ThreadSafe> StartSequence(
		TArray<TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe>> Packets,
		int32 ResultPacketIndex);

	/** Queues a non-blocking render-thread readiness check. */
	void Poll();
	/** Returns true only after completion/failure. Mismatched current identity is discarded. */
	bool TryTakeResult(
		const FSightWeaveRenderReadbackExpectation& CurrentExpectation,
		FSightWeaveRenderReadbackResult& OutResult);
	bool IsFinished() const;

private:
	struct FState;
	explicit FSightWeaveRenderTestReadback(TSharedRef<FState, ESPMode::ThreadSafe> InState);

	TSharedRef<FState, ESPMode::ThreadSafe> State;
};

#endif
