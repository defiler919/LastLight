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
	FRenderQueryRHIRef GPUStartQuery;
	FRenderQueryRHIRef GPUEndQuery;
	double StartSeconds = FPlatformTime::Seconds();
	double GameThreadSubmitMicroseconds = 0.0;
	double RenderThreadConsumeMicroseconds = 0.0;
	double RenderThreadRDGSetupMicroseconds = 0.0;
	bool bNoChangeProducedMaskWork = false;
	bool bGPUTimestampsIssued = false;
};

namespace
{
	BEGIN_SHADER_PARAMETER_STRUCT(FSightWeaveGPUTimestampPassParameters, )
		RDG_TEXTURE_ACCESS(EffectiveLive, ERHIAccess::CopySrc)
	END_SHADER_PARAMETER_STRUCT()

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

	const double SubmitStartSeconds = FPlatformTime::Seconds();
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
			const double ConsumeStartSeconds = FPlatformTime::Seconds();
			for (const TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe>& Submitted
				: NewState->Packets)
			{
				NewState->RenderState->SubmitPacket_RenderThread(Submitted);
			}
			NewState->RenderThreadConsumeMicroseconds =
				(FPlatformTime::Seconds() - ConsumeStartSeconds) * 1000000.0;
			if (GSupportsTimestampRenderQueries)
			{
				NewState->GPUStartQuery = RHICreateRenderQuery(RQT_AbsoluteTime);
				NewState->GPUEndQuery = RHICreateRenderQuery(RQT_AbsoluteTime);
				if (NewState->GPUStartQuery.IsValid() && NewState->GPUEndQuery.IsValid())
				{
					RHICmdList.EndRenderQuery(NewState->GPUStartQuery.GetReference());
					NewState->bGPUTimestampsIssued = true;
				}
			}
			FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("SightWeave.TestReadback"));
			const double RDGSetupStartSeconds = FPlatformTime::Seconds();
			FRDGTextureRef EffectiveLive =
				NewState->RenderState->ProcessPending_RenderThread(GraphBuilder);
			NewState->RenderThreadRDGSetupMicroseconds =
				(FPlatformTime::Seconds() - RDGSetupStartSeconds) * 1000000.0;
			NewState->bNoChangeProducedMaskWork =
				NewState->RenderState->ProcessPending_RenderThread(GraphBuilder) != nullptr;
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
			if (NewState->bGPUTimestampsIssued)
			{
				FSightWeaveGPUTimestampPassParameters* TimestampParameters =
					GraphBuilder.AllocParameters<FSightWeaveGPUTimestampPassParameters>();
				TimestampParameters->EffectiveLive = EffectiveLive;
				FRHIRenderQuery* EndQuery = NewState->GPUEndQuery.GetReference();
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("SightWeave.TimestampMaskComplete"),
					TimestampParameters,
					ERDGPassFlags::Copy | ERDGPassFlags::NeverCull,
					[EndQuery](FRDGAsyncTask, FRHICommandList& CommandList)
					{
						CommandList.EndRenderQuery(EndQuery);
					});
			}
			AddEnqueueCopyPass(GraphBuilder, NewState->Readback.Get(), EffectiveLive);
			GraphBuilder.Execute();
		});
	NewState->GameThreadSubmitMicroseconds =
		(FPlatformTime::Seconds() - SubmitStartSeconds) * 1000000.0;
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
			Result.DuplicatePacketCount =
				PollState->RenderState->GetDuplicatePacketCount_RenderThread();
			Result.StalePacketCount =
				PollState->RenderState->GetStalePacketCount_RenderThread();
			Result.RejectedPacketCount =
				PollState->RenderState->GetRejectedPacketCount_RenderThread();
			Result.bNoChangeProducedMaskWork = PollState->bNoChangeProducedMaskWork;
			Result.GameThreadSubmitMicroseconds = PollState->GameThreadSubmitMicroseconds;
			Result.RenderThreadConsumeMicroseconds = PollState->RenderThreadConsumeMicroseconds;
			Result.RenderThreadRDGSetupMicroseconds = PollState->RenderThreadRDGSetupMicroseconds;
			const FSightWeaveRenderPassSetupTimings& PassTimings =
				PollState->RenderState->GetLastPassSetupTimings_RenderThread();
			Result.ClearPassSetupMicroseconds = PassTimings.ClearMicroseconds;
			Result.RasterVisionSetupMicroseconds = PassTimings.RasterVisionMicroseconds;
			Result.RasterIlluminationSetupMicroseconds = PassTimings.RasterIlluminationMicroseconds;
			Result.RasterBypassSetupMicroseconds = PassTimings.RasterBypassMicroseconds;
			Result.RasterSuppressionSetupMicroseconds = PassTimings.RasterSuppressionMicroseconds;
			Result.CombinePassSetupMicroseconds = PassTimings.CombineMicroseconds;
			Result.PersistentMaskBytes = 256ull * 256ull;
			Result.ScratchMaskBytes = Packet.GetIndices().IsEmpty() ? 0ull : 4ull * 256ull * 256ull;
			Result.PacketBufferBytes =
				static_cast<uint64>(Packet.GetVertices().Num()) * sizeof(FVector2f)
				+ static_cast<uint64>(Packet.GetIndices().Num()) * sizeof(uint32);
			if (PollState->bGPUTimestampsIssued)
			{
				uint64 GPUStartMicroseconds = 0;
				uint64 GPUEndMicroseconds = 0;
				if (RHIGetRenderQueryResult(
						PollState->GPUStartQuery.GetReference(),
						GPUStartMicroseconds,
						false)
					&& RHIGetRenderQueryResult(
						PollState->GPUEndQuery.GetReference(),
						GPUEndMicroseconds,
						false)
					&& GPUEndMicroseconds >= GPUStartMicroseconds)
				{
					Result.bGPUTimestampAvailable = true;
					Result.GPUWorkMicroseconds =
						static_cast<double>(GPUEndMicroseconds - GPUStartMicroseconds);
				}
			}
			Result.ReadbackEndToEndMicroseconds =
				(FPlatformTime::Seconds() - PollState->StartSeconds) * 1000000.0;
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
