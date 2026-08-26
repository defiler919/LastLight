#include "SightWeaveRenderTestReadback.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/CriticalSection.h"
#include "Hash/xxhash.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "RHIGPUReadback.h"
#include "SightWeaveSingleTileRenderState.h"

struct FSightWeaveRenderTestReadback::FState final
{
	explicit FState(
		TArray<TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe>> InPackets,
		const int32 ResultPacketIndex)
		: Packets(MoveTemp(InPackets))
	{
		if (Packets.IsValidIndex(ResultPacketIndex))
		{
			Packet = Packets[ResultPacketIndex];
		}
	}

	void Finish(FSightWeaveRenderReadbackResult&& InResult)
	{
		FScopeLock Lock(&ResultGuard);
		Result = MoveTemp(InResult);
		bFinished.Store(true);
	}

	TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe> Packet;
	TArray<TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe>> Packets;
	FCriticalSection ResultGuard;
	FSightWeaveRenderReadbackResult Result;
	TAtomic<bool> bStarted{ false };
	TAtomic<bool> bPollQueued{ false };
	TAtomic<bool> bFinished{ false };
	TUniquePtr<FSightWeaveSingleTileRenderState> RenderState;
	TUniquePtr<FRHIGPUTextureReadback> Readback;
};

namespace
{
	FSightWeaveRenderReadbackExpectation MakeExpectation(const FSightWeaveRenderPacket& Packet)
	{
		FSightWeaveRenderReadbackExpectation Result;
		Result.WorldIdentity = Packet.GetWorldIdentity();
		Result.KnowledgeOwnerId = Packet.GetKnowledgeOwnerId();
		Result.FloorId = Packet.GetFloorId();
		Result.CompatibilityProfile = Packet.GetCompatibilityProfile();
		Result.PacketRevision = Packet.GetPacketRevision();
		return Result;
	}

	bool Matches(
		const FSightWeaveRenderReadbackExpectation& A,
		const FSightWeaveRenderReadbackExpectation& B)
	{
		return A.WorldIdentity == B.WorldIdentity
			&& A.KnowledgeOwnerId == B.KnowledgeOwnerId
			&& A.FloorId == B.FloorId
			&& A.CompatibilityProfile.IsEquivalentTo(B.CompatibilityProfile)
			&& A.PacketRevision == B.PacketRevision;
	}

	void PopulateCapabilities(FSightWeaveRenderReadbackResult& Result)
	{
		Result.PixelFormatName = TEXT("PF_G8 / DXGI_FORMAT_R8_UNORM");
		Result.bPF_G8Texture2D = RHIPixelFormatHasCapabilities(
			PF_G8,
			EPixelFormatCapabilities::Texture2D);
		Result.bPF_G8RenderTarget = RHIPixelFormatHasCapabilities(
			PF_G8,
			EPixelFormatCapabilities::RenderTarget);
		Result.bPF_G8ShaderResource = RHIPixelFormatHasCapabilities(
			PF_G8,
			EPixelFormatCapabilities::TextureLoad | EPixelFormatCapabilities::TextureSample);
		Result.bPF_G8UAV = RHIPixelFormatHasCapabilities(
			PF_G8,
			EPixelFormatCapabilities::UAV | EPixelFormatCapabilities::TypedUAVStore);
	}
}

FSightWeaveRenderTestReadback::FSightWeaveRenderTestReadback(
	TSharedRef<FState, ESPMode::ThreadSafe> InState)
	: State(MoveTemp(InState))
{
}

TSharedRef<FSightWeaveRenderTestReadback, ESPMode::ThreadSafe>
FSightWeaveRenderTestReadback::Start(
	TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe> Packet)
{
	TArray<TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe>> Packets;
	Packets.Add(MoveTemp(Packet));
	return StartSequence(MoveTemp(Packets), 0);
}

TSharedRef<FSightWeaveRenderTestReadback, ESPMode::ThreadSafe>
FSightWeaveRenderTestReadback::StartSequence(
	TArray<TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe>> Packets,
	const int32 ResultPacketIndex)
{
	check(IsInGameThread());
	const TSharedRef<FState, ESPMode::ThreadSafe> NewState =
		MakeShared<FState, ESPMode::ThreadSafe>(MoveTemp(Packets), ResultPacketIndex);
	const TSharedRef<FSightWeaveRenderTestReadback, ESPMode::ThreadSafe> Request =
		MakeShareable(new FSightWeaveRenderTestReadback(NewState));

	ENQUEUE_RENDER_COMMAND(SightWeaveStartTestReadback)(
		[NewState](FRHICommandListImmediate& RHICmdList)
		{
			NewState->bStarted.Store(true);
			FSightWeaveRenderReadbackResult FailureResult;
			if (!NewState->Packet.IsValid() || !NewState->Packet->IsValid())
			{
				FailureResult.Status = ESightWeaveRenderReadbackStatus::Failed;
				FailureResult.Availability = ESightWeaveRenderAvailability::InvalidPacket;
				FailureResult.Failure = TEXT("Packet is null or invalid");
				NewState->Finish(MoveTemp(FailureResult));
				return;
			}

			const FSightWeaveRenderPacket& Packet = *NewState->Packet;
			FailureResult.Identity = MakeExpectation(Packet);
			FailureResult.RegistryRevision = Packet.GetRegistryRevision();
			FailureResult.PublishedSnapshotRevision = Packet.GetPublishedSnapshotRevision();
			FailureResult.PacketContentHash = Packet.GetContentHash();
			FailureResult.TileCoordinate = Packet.GetTileCoordinate();
			FailureResult.Width = SightWeave::RenderPacket::PhysicalTileSize;
			FailureResult.Height = SightWeave::RenderPacket::PhysicalTileSize;
			PopulateCapabilities(FailureResult);

			NewState->RenderState = MakeUnique<FSightWeaveSingleTileRenderState>(
				Packet.GetWorldIdentity());
			for (const TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe>& Submitted
				: NewState->Packets)
			{
				NewState->RenderState->SubmitPacket_RenderThread(Submitted);
			}
			FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("SightWeave.TestReadback"));
			FRDGTextureRef EffectiveLive =
				NewState->RenderState->ProcessPending_RenderThread(GraphBuilder);
			FailureResult.Availability = NewState->RenderState->GetAvailability_RenderThread();
			FailureResult.ResourceGeneration =
				NewState->RenderState->GetResourceGeneration_RenderThread();
			FailureResult.RasterDispatchCount =
				NewState->RenderState->GetRasterDispatchCount_RenderThread();
			if (!EffectiveLive)
			{
				FailureResult.Status = ESightWeaveRenderReadbackStatus::Failed;
				FailureResult.Failure = FString::Printf(
					TEXT("No readable PF_G8 EffectiveLive texture was produced (availability=%d)"),
					static_cast<int32>(FailureResult.Availability));
				GraphBuilder.Execute();
				NewState->RenderState.Reset();
				NewState->Finish(MoveTemp(FailureResult));
				return;
			}

			NewState->Readback = MakeUnique<FRHIGPUTextureReadback>(
				TEXT("SightWeave.M3P1.EffectiveLiveReadback"));
			AddEnqueueCopyPass(GraphBuilder, NewState->Readback.Get(), EffectiveLive);
			GraphBuilder.Execute();
		});
	return Request;
}

void FSightWeaveRenderTestReadback::Poll()
{
	check(IsInGameThread());
	if (State->bFinished.Load() || State->bPollQueued.Exchange(true))
	{
		return;
	}
	const TSharedRef<FState, ESPMode::ThreadSafe> PollState = State;
	ENQUEUE_RENDER_COMMAND(SightWeavePollTestReadback)(
		[PollState](FRHICommandListImmediate& RHICmdList)
		{
			if (PollState->bFinished.Load())
			{
				PollState->bPollQueued.Store(false);
				return;
			}
			if (!PollState->Readback.IsValid() || !PollState->Readback->IsReady())
			{
				PollState->bPollQueued.Store(false);
				return;
			}

			FSightWeaveRenderReadbackResult Result;
			const FSightWeaveRenderPacket& Packet = *PollState->Packet;
			Result.Status = ESightWeaveRenderReadbackStatus::Complete;
			Result.Availability = PollState->RenderState->GetAvailability_RenderThread();
			Result.Identity = MakeExpectation(Packet);
			Result.RegistryRevision = Packet.GetRegistryRevision();
			Result.PublishedSnapshotRevision = Packet.GetPublishedSnapshotRevision();
			Result.PacketContentHash = Packet.GetContentHash();
			Result.ResourceGeneration =
				PollState->RenderState->GetResourceGeneration_RenderThread();
			Result.RasterDispatchCount =
				PollState->RenderState->GetRasterDispatchCount_RenderThread();
			Result.TileCoordinate = Packet.GetTileCoordinate();
			Result.Width = SightWeave::RenderPacket::PhysicalTileSize;
			Result.Height = SightWeave::RenderPacket::PhysicalTileSize;
			PopulateCapabilities(Result);

			void* Source = PollState->Readback->Lock(
				Result.RowPitchInPixels,
				&Result.BufferHeight);
			if (!Source
				|| Result.RowPitchInPixels < Result.Width
				|| Result.BufferHeight < Result.Height)
			{
				Result.Status = ESightWeaveRenderReadbackStatus::Failed;
				Result.Failure = TEXT("PF_G8 readback returned an invalid pointer, row pitch, or height");
			}
			else
			{
				Result.Pixels.SetNumUninitialized(Result.Width * Result.Height);
				const uint8* SourceBytes = static_cast<const uint8*>(Source);
				for (int32 Y = 0; Y < Result.Height; ++Y)
				{
					FMemory::Memcpy(
						Result.Pixels.GetData() + Y * Result.Width,
						SourceBytes + Y * Result.RowPitchInPixels,
						Result.Width);
				}
				for (const uint8 Value : Result.Pixels)
				{
					Result.ZeroTexelCount += Value == 0;
					Result.WhiteTexelCount += Value == 255;
					Result.NonBinaryTexelCount += Value != 0 && Value != 255;
				}
				Result.MaskHash = FXxHash64::HashBuffer(
					Result.Pixels.GetData(),
					Result.Pixels.Num()).Hash;
			}
			PollState->Readback->Unlock();
			PollState->Readback.Reset();
			PollState->RenderState->Release_RenderThread(Packet.GetWorldIdentity());
			PollState->RenderState.Reset();
			PollState->bPollQueued.Store(false);
			PollState->Finish(MoveTemp(Result));
		});
}

bool FSightWeaveRenderTestReadback::TryTakeResult(
	const FSightWeaveRenderReadbackExpectation& CurrentExpectation,
	FSightWeaveRenderReadbackResult& OutResult)
{
	check(IsInGameThread());
	if (!State->bFinished.Load())
	{
		return false;
	}
	FScopeLock Lock(&State->ResultGuard);
	OutResult = MoveTemp(State->Result);
	if (OutResult.Status == ESightWeaveRenderReadbackStatus::Complete
		&& !Matches(OutResult.Identity, CurrentExpectation))
	{
		OutResult.Status = ESightWeaveRenderReadbackStatus::DiscardedStale;
		OutResult.Failure = TEXT("Readback identity/revision no longer matches current scope");
		OutResult.Pixels.Reset();
	}
	return true;
}

bool FSightWeaveRenderTestReadback::IsFinished() const
{
	return State->bFinished.Load();
}

#endif
