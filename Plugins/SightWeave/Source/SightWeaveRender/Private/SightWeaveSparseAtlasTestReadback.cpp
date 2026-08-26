#include "SightWeaveSparseAtlasTestReadback.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/CriticalSection.h"
#include "Hash/xxhash.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "RHIGPUReadback.h"
#include "SightWeaveSparseAtlasRenderState.h"

struct FSightWeaveSparseAtlasTestReadback::FState final
{
	FState(
		TArray<TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe>> InPackets,
		FSightWeaveSparseTileIdentity InSelectedTile)
		: Packets(MoveTemp(InPackets))
		, SelectedTile(MoveTemp(InSelectedTile))
	{
		if (!Packets.IsEmpty())
		{
			Result.Identity.PacketRevision = Packets.Last()->GetPacketRevision();
			Result.Identity.TileIdentity = SelectedTile;
		}
	}

	void Finish(FSightWeaveSparseReadbackResult&& InResult)
	{
		FScopeLock Lock(&ResultGuard);
		Result = MoveTemp(InResult);
		bFinished.Store(true);
	}

	TArray<TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe>> Packets;
	FSightWeaveSparseTileIdentity SelectedTile;
	FSightWeaveSparsePhysicalAddress ReadbackAddress;
	FCriticalSection ResultGuard;
	FSightWeaveSparseReadbackResult Result;
	TAtomic<bool> bPollQueued{ false };
	TAtomic<bool> bFinished{ false };
	TUniquePtr<FSightWeaveSparseAtlasRenderState> RenderState;
	TUniquePtr<FRHIGPUTextureReadback> Readback;
	FRenderQueryRHIRef GPUStartQuery;
	FRenderQueryRHIRef GPUEndQuery;
	double StartSeconds = FPlatformTime::Seconds();
	double GameThreadSubmitMicroseconds = 0.0;
	bool bGPUTimestampsIssued = false;
};

namespace SightWeaveSparseAtlasTestReadbackPrivate
{
	BEGIN_SHADER_PARAMETER_STRUCT(FSightWeaveSparseTimestampPassParameters, )
		RDG_TEXTURE_ACCESS(AtlasPage, ERHIAccess::CopySrc)
	END_SHADER_PARAMETER_STRUCT()

	bool Matches(
		const FSightWeaveSparseReadbackExpectation& A,
		const FSightWeaveSparseReadbackExpectation& B)
	{
		return A.PacketRevision == B.PacketRevision
			&& A.TileIdentity.IsEquivalentTo(B.TileIdentity);
	}
}

using namespace SightWeaveSparseAtlasTestReadbackPrivate;

FSightWeaveSparseAtlasTestReadback::FSightWeaveSparseAtlasTestReadback(
	TSharedRef<FState, ESPMode::ThreadSafe> InState)
	: State(MoveTemp(InState))
{
}

TArray<double> FSightWeaveSparseAtlasTestReadback::BenchmarkGameThreadSubmitMicroseconds(
	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet,
	const int32 WarmupCount,
	const int32 SampleCount)
{
	check(IsInGameThread());
	TArray<double> Samples;
	if (!Packet.IsValid() || WarmupCount < 0 || SampleCount <= 0)
	{
		return Samples;
	}
	auto Enqueue = [&Packet]()
	{
		ENQUEUE_RENDER_COMMAND(SightWeaveBenchmarkSparseSubmit)(
			[OwnedPacket = Packet](FRHICommandListImmediate& RHICmdList)
			{
				if (!OwnedPacket.IsValid())
				{
					return;
				}
			});
	};
	for (int32 Index = 0; Index < WarmupCount; ++Index)
	{
		Enqueue();
	}
	FlushRenderingCommands();
	Samples.Reserve(SampleCount);
	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		const double StartSeconds = FPlatformTime::Seconds();
		Enqueue();
		Samples.Add((FPlatformTime::Seconds() - StartSeconds) * 1000000.0);
		if ((Index + 1) % 256 == 0)
		{
			FlushRenderingCommands();
		}
	}
	FlushRenderingCommands();
	return Samples;
}

TSharedRef<FSightWeaveSparseAtlasTestReadback, ESPMode::ThreadSafe>
FSightWeaveSparseAtlasTestReadback::StartSequence(
	TArray<TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe>> Packets,
	FSightWeaveSparseTileIdentity SelectedTile)
{
	check(IsInGameThread());
	const TSharedRef<FState, ESPMode::ThreadSafe> NewState =
		MakeShared<FState, ESPMode::ThreadSafe>(MoveTemp(Packets), MoveTemp(SelectedTile));
	const TSharedRef<FSightWeaveSparseAtlasTestReadback, ESPMode::ThreadSafe> Request =
		MakeShareable(new FSightWeaveSparseAtlasTestReadback(NewState));
	const double SubmitStartSeconds = FPlatformTime::Seconds();
	ENQUEUE_RENDER_COMMAND(SightWeaveStartSparseAtlasReadback)(
		[NewState](FRHICommandListImmediate& RHICmdList)
		{
			FSightWeaveSparseReadbackResult FailureResult = NewState->Result;
			if (NewState->Packets.IsEmpty()
				|| !NewState->Packets.Last().IsValid()
				|| !NewState->Packets.Last()->IsValid()
				|| !NewState->SelectedTile.IsValid())
			{
				FailureResult.Status = ESightWeaveSparseReadbackStatus::Failed;
				FailureResult.Availability = ESightWeaveRenderAvailability::InvalidPacket;
				FailureResult.Failure = TEXT("Packet sequence or selected tile identity is invalid");
				NewState->Finish(MoveTemp(FailureResult));
				return;
			}

			const FSightWeaveRenderWorldIdentity WorldIdentity =
				NewState->Packets[0]->GetWorldIdentity();
			NewState->RenderState = MakeUnique<FSightWeaveSparseAtlasRenderState>(WorldIdentity);
			for (int32 PacketIndex = 0; PacketIndex < NewState->Packets.Num(); ++PacketIndex)
			{
				const TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe>& Packet =
					NewState->Packets[PacketIndex];
				if (!Packet.IsValid() || Packet->GetWorldIdentity() != WorldIdentity)
				{
					FailureResult.Status = ESightWeaveSparseReadbackStatus::Failed;
					FailureResult.Availability = ESightWeaveRenderAvailability::InvalidPacket;
					FailureResult.Failure = TEXT("Sequence crosses a world lifetime or contains a null packet");
					NewState->RenderState->Release_RenderThread(WorldIdentity);
					NewState->RenderState.Reset();
					NewState->Finish(MoveTemp(FailureResult));
					return;
				}
				const uint64 DispatchBefore =
					NewState->RenderState->GetDirtyTileDispatchCount_RenderThread();
				const double ConsumeStartSeconds = FPlatformTime::Seconds();
				NewState->RenderState->SubmitPacket_RenderThread(Packet);
				const double ConsumeMicroseconds =
					(FPlatformTime::Seconds() - ConsumeStartSeconds) * 1000000.0;
				FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("SightWeave.Sparse.TestUpdate"));
				const bool bMaskWork = NewState->RenderState->ProcessPending_RenderThread(GraphBuilder);

				FSightWeaveSparseUpdateSample& Sample = FailureResult.Updates.AddDefaulted_GetRef();
				Sample.PacketRevision = Packet->GetPacketRevision();
				Sample.RequestedDirtyTileCount = Packet->GetDirtyTileIndices().Num();
				Sample.bProducedMaskWork = bMaskWork;
				Sample.DirtyTileDispatchDelta =
					NewState->RenderState->GetDirtyTileDispatchCount_RenderThread() - DispatchBefore;
				Sample.ResourceGeneration = NewState->RenderState->GetResourceGeneration_RenderThread();
				Sample.PageAllocationCount = NewState->RenderState->GetPageAllocationCount_RenderThread();
				Sample.ScratchAllocationCount = NewState->RenderState->GetScratchAllocationCount_RenderThread();
				Sample.EvictionCount = NewState->RenderState->GetEvictionCount_RenderThread();
				Sample.ResidentTileCount = NewState->RenderState->GetResidentTileCount_RenderThread();
				Sample.AllocatedPageCount = NewState->RenderState->GetAllocatedPageCount_RenderThread();
				Sample.GameThreadSubmitMicroseconds = PacketIndex == 0
					? NewState->GameThreadSubmitMicroseconds
					: 0.0;
				const FSightWeaveSparseRenderTimings& Timings =
					NewState->RenderState->GetLastTimings_RenderThread();
				Sample.RenderThreadPacketConsumeMicroseconds = FMath::Max(
					ConsumeMicroseconds,
					Timings.PacketConsumeMicroseconds);
				Sample.RenderThreadDirtySchedulingMicroseconds = Timings.DirtySchedulingMicroseconds;
				Sample.RenderThreadRDGSetupMicroseconds = Timings.RDGSetupMicroseconds;
				Sample.TileClearSetupMicroseconds = Timings.TileClearSetupMicroseconds;
				Sample.RasterSetupMicroseconds = Timings.RasterSetupMicroseconds;
				Sample.PublicationMicroseconds = Timings.PublicationMicroseconds;

				if (PacketIndex == NewState->Packets.Num() - 1)
				{
					FailureResult.Availability =
						NewState->RenderState->GetAvailability_RenderThread();
					if (!NewState->RenderState->AddReadback_RenderThread(NewState->SelectedTile))
					{
						FailureResult.Status = ESightWeaveSparseReadbackStatus::Failed;
						FailureResult.Failure = TEXT("Selected tile is not resident for readback");
						GraphBuilder.Execute();
						NewState->RenderState->Release_RenderThread(WorldIdentity);
						NewState->RenderState.Reset();
						NewState->Finish(MoveTemp(FailureResult));
						return;
					}
					FIntRect SlotRect;
					FRDGTextureRef Page =
						NewState->RenderState->RegisterResidentPageForReadback_RenderThread(
							GraphBuilder,
							NewState->SelectedTile,
							SlotRect,
							NewState->ReadbackAddress);
					if (!Page)
					{
						FailureResult.Status = ESightWeaveSparseReadbackStatus::Failed;
						FailureResult.Failure = TEXT("Selected tile page could not be registered");
						GraphBuilder.Execute();
						NewState->RenderState->RemoveReadback_RenderThread(
							NewState->SelectedTile,
							NewState->ReadbackAddress);
						NewState->RenderState->Release_RenderThread(WorldIdentity);
						NewState->RenderState.Reset();
						NewState->Finish(MoveTemp(FailureResult));
						return;
					}

					if (GSupportsTimestampRenderQueries)
					{
						NewState->GPUStartQuery = RHICreateRenderQuery(RQT_AbsoluteTime);
						NewState->GPUEndQuery = RHICreateRenderQuery(RQT_AbsoluteTime);
						if (NewState->GPUStartQuery.IsValid() && NewState->GPUEndQuery.IsValid())
						{
							RHICmdList.EndRenderQuery(NewState->GPUStartQuery.GetReference());
							NewState->bGPUTimestampsIssued = true;
							FSightWeaveSparseTimestampPassParameters* TimestampParameters =
								GraphBuilder.AllocParameters<FSightWeaveSparseTimestampPassParameters>();
							TimestampParameters->AtlasPage = Page;
							FRHIRenderQuery* EndQuery = NewState->GPUEndQuery.GetReference();
							GraphBuilder.AddPass(
								RDG_EVENT_NAME("SightWeave.Sparse.TimestampComplete"),
								TimestampParameters,
								ERDGPassFlags::Copy | ERDGPassFlags::NeverCull,
								[EndQuery](FRDGAsyncTask, FRHICommandList& CommandList)
								{
									CommandList.EndRenderQuery(EndQuery);
								});
						}
					}
					NewState->Readback = MakeUnique<FRHIGPUTextureReadback>(
						TEXT("SightWeave.M3P2.SparseTileReadback"));
					AddEnqueueCopyPass(
						GraphBuilder,
						NewState->Readback.Get(),
						Page,
						FResolveRect(SlotRect.Min.X, SlotRect.Min.Y, SlotRect.Max.X, SlotRect.Max.Y));
				}
				GraphBuilder.Execute();
			}
			FailureResult.PhysicalAddress = NewState->ReadbackAddress;
			NewState->Result = MoveTemp(FailureResult);
		});
	NewState->GameThreadSubmitMicroseconds =
		(FPlatformTime::Seconds() - SubmitStartSeconds) * 1000000.0;
	return Request;
}

void FSightWeaveSparseAtlasTestReadback::Poll()
{
	check(IsInGameThread());
	if (State->bFinished.Load() || State->bPollQueued.Exchange(true))
	{
		return;
	}
	const TSharedRef<FState, ESPMode::ThreadSafe> PollState = State;
	ENQUEUE_RENDER_COMMAND(SightWeavePollSparseAtlasReadback)(
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

			FSightWeaveSparseReadbackResult Result = MoveTemp(PollState->Result);
			Result.Status = ESightWeaveSparseReadbackStatus::Complete;
			Result.Availability = PollState->RenderState->GetAvailability_RenderThread();
			Result.DuplicatePacketCount = PollState->RenderState->GetDuplicatePacketCount_RenderThread();
			Result.StalePacketCount = PollState->RenderState->GetStalePacketCount_RenderThread();
			Result.RejectedPacketCount = PollState->RenderState->GetRejectedPacketCount_RenderThread();
			Result.Width = SightWeave::SparseAtlas::PhysicalTileSize;
			Result.Height = SightWeave::SparseAtlas::PhysicalTileSize;
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

			void* Source = PollState->Readback->Lock(Result.RowPitchInPixels, &Result.BufferHeight);
			if (!Source
				|| Result.RowPitchInPixels < Result.Width
				|| Result.BufferHeight < Result.Height)
			{
				Result.Status = ESightWeaveSparseReadbackStatus::Failed;
				Result.Failure = TEXT("Sparse PF_G8 readback returned invalid dimensions");
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
			PollState->RenderState->RemoveReadback_RenderThread(
				PollState->SelectedTile,
				PollState->ReadbackAddress);
			PollState->RenderState->Release_RenderThread(
				PollState->Packets.Last()->GetWorldIdentity());
			PollState->RenderState.Reset();
			Result.ReadbackEndToEndMicroseconds =
				(FPlatformTime::Seconds() - PollState->StartSeconds) * 1000000.0;
			PollState->bPollQueued.Store(false);
			PollState->Finish(MoveTemp(Result));
		});
}

bool FSightWeaveSparseAtlasTestReadback::TryTakeResult(
	const FSightWeaveSparseReadbackExpectation& CurrentExpectation,
	FSightWeaveSparseReadbackResult& OutResult)
{
	check(IsInGameThread());
	if (!State->bFinished.Load())
	{
		return false;
	}
	FScopeLock Lock(&State->ResultGuard);
	OutResult = MoveTemp(State->Result);
	if (OutResult.Status == ESightWeaveSparseReadbackStatus::Complete
		&& !SightWeaveSparseAtlasTestReadbackPrivate::Matches(
			OutResult.Identity,
			CurrentExpectation))
	{
		OutResult.Status = ESightWeaveSparseReadbackStatus::DiscardedStale;
		OutResult.Failure = TEXT("Sparse readback identity/revision no longer matches current scope");
		OutResult.Pixels.Reset();
	}
	return true;
}

bool FSightWeaveSparseAtlasTestReadback::IsFinished() const
{
	return State->bFinished.Load();
}

#endif
