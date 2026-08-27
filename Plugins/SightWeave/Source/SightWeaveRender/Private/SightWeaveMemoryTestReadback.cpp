#include "SightWeaveMemoryTestReadback.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/CriticalSection.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "RHIGPUReadback.h"
#include "SightWeaveSparseAtlasRenderState.h"

struct FSightWeaveMemoryTestReadback::FState final
{
	FState(
		TArray<TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe>> InPackets,
		FSightWeaveMemoryTileKey InSelectedTile)
		: Packets(MoveTemp(InPackets))
		, SelectedTile(MoveTemp(InSelectedTile))
	{
		Result.TileKey = SelectedTile;
	}

	void Finish(FSightWeaveMemoryReadbackResult&& InResult)
	{
		FScopeLock Lock(&ResultGuard);
		Result = MoveTemp(InResult);
		bFinished.Store(true);
	}

	TArray<TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe>> Packets;
	FSightWeaveMemoryTileKey SelectedTile;
	FSightWeaveSparsePhysicalAddress ReadbackAddress;
	FCriticalSection ResultGuard;
	FSightWeaveMemoryReadbackResult Result;
	TAtomic<bool> bPollQueued{ false };
	TAtomic<bool> bFinished{ false };
	TUniquePtr<FSightWeaveSparseAtlasRenderState> RenderState;
	TUniquePtr<FRHIGPUTextureReadback> Readback;
	TArray<FRenderQueryRHIRef> GPUStartQueries;
	TArray<FRenderQueryRHIRef> GPUEndQueries;
};

namespace SightWeaveMemoryTestReadbackPrivate
{
	constexpr uint64 MemoryPageBytes =
		static_cast<uint64>(SightWeave::SparseAtlas::PageSize)
		* SightWeave::SparseAtlas::PageSize;
	constexpr uint64 PageTableEntryBytes = sizeof(FIntVector4);
}

using namespace SightWeaveMemoryTestReadbackPrivate;

FSightWeaveMemoryTestReadback::FSightWeaveMemoryTestReadback(
	TSharedRef<FState, ESPMode::ThreadSafe> InState)
	: State(MoveTemp(InState))
{
}

TSharedRef<FSightWeaveMemoryTestReadback, ESPMode::ThreadSafe>
FSightWeaveMemoryTestReadback::StartSequence(
	TArray<TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe>> Packets,
	FSightWeaveMemoryTileKey SelectedTile)
{
	check(IsInGameThread());
	const TSharedRef<FState, ESPMode::ThreadSafe> NewState =
		MakeShared<FState, ESPMode::ThreadSafe>(MoveTemp(Packets), MoveTemp(SelectedTile));
	const TSharedRef<FSightWeaveMemoryTestReadback, ESPMode::ThreadSafe> Request =
		MakeShareable(new FSightWeaveMemoryTestReadback(NewState));
	ENQUEUE_RENDER_COMMAND(SightWeaveStartMemoryMirrorReadback)(
		[NewState](FRHICommandListImmediate& RHICmdList)
		{
			FSightWeaveMemoryReadbackResult Result = NewState->Result;
			if (NewState->Packets.IsEmpty()
				|| !NewState->Packets[0].IsValid()
				|| !NewState->SelectedTile.IsValid())
			{
				Result.Status = ESightWeaveMemoryReadbackStatus::Failed;
				Result.Availability = ESightWeaveRenderAvailability::InvalidPacket;
				Result.Failure = TEXT("Memory packet sequence or selected tile is invalid");
				NewState->Finish(MoveTemp(Result));
				return;
			}

			const FSightWeaveRenderWorldIdentity WorldIdentity =
				NewState->Packets[0]->GetScope().WorldIdentity;
			NewState->RenderState = MakeUnique<FSightWeaveSparseAtlasRenderState>(WorldIdentity);
			NewState->GPUStartQueries.SetNum(NewState->Packets.Num());
			NewState->GPUEndQueries.SetNum(NewState->Packets.Num());
			for (int32 PacketIndex = 0; PacketIndex < NewState->Packets.Num(); ++PacketIndex)
			{
				const TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe>& Packet =
					NewState->Packets[PacketIndex];
				if (!Packet.IsValid())
				{
					Result.Status = ESightWeaveMemoryReadbackStatus::Failed;
					Result.Availability = ESightWeaveRenderAvailability::InvalidPacket;
					Result.Failure = TEXT("Memory sequence contains a null packet");
					NewState->RenderState->Release_RenderThread(WorldIdentity);
					NewState->RenderState.Reset();
					NewState->Finish(MoveTemp(Result));
					return;
				}
				const uint64 UploadBefore =
					NewState->RenderState->GetMemoryUploadCount_RenderThread();
				const double TotalSetupStartSeconds = FPlatformTime::Seconds();
				const double ConsumeStartSeconds = FPlatformTime::Seconds();
				NewState->RenderState->SubmitMemoryPacket_RenderThread(Packet);
				const double ConsumeMicroseconds =
					(FPlatformTime::Seconds() - ConsumeStartSeconds) * 1000000.0;
				FRDGBuilder GraphBuilder(
					RHICmdList,
					RDG_EVENT_NAME("SightWeave.Memory.TestUpdate"));
				if (GSupportsTimestampRenderQueries)
				{
					NewState->GPUStartQueries[PacketIndex] = RHICreateRenderQuery(RQT_AbsoluteTime);
					NewState->GPUEndQueries[PacketIndex] = RHICreateRenderQuery(RQT_AbsoluteTime);
					if (NewState->GPUStartQueries[PacketIndex].IsValid()
						&& NewState->GPUEndQueries[PacketIndex].IsValid())
					{
						RHICmdList.EndRenderQuery(
							NewState->GPUStartQueries[PacketIndex].GetReference());
					}
				}
				const double ResidencyStartSeconds = FPlatformTime::Seconds();
				const bool bMirrorWork =
					NewState->RenderState->ProcessMemoryPending_RenderThread(GraphBuilder);
				const double ResidencyMicroseconds =
					(FPlatformTime::Seconds() - ResidencyStartSeconds) * 1000000.0;
				const double PageTableStartSeconds = FPlatformTime::Seconds();
				NewState->RenderState->PrepareMemoryPresentationResources_RenderThread(GraphBuilder);
				const double PageTableMicroseconds =
					(FPlatformTime::Seconds() - PageTableStartSeconds) * 1000000.0;
				if (NewState->GPUStartQueries[PacketIndex].IsValid()
					&& NewState->GPUEndQueries[PacketIndex].IsValid())
				{
					FRHIRenderQuery* EndQuery =
						NewState->GPUEndQueries[PacketIndex].GetReference();
					GraphBuilder.AddPass(
						RDG_EVENT_NAME("SightWeave.Memory.TimestampComplete"),
						ERDGPassFlags::None | ERDGPassFlags::NeverCull,
						[EndQuery](FRDGAsyncTask, FRHICommandList& CommandList)
						{
							CommandList.EndRenderQuery(EndQuery);
						});
				}

				FSightWeaveMemoryMirrorUpdateSample& Sample = Result.Updates.AddDefaulted_GetRef();
				Sample.PacketRevision = Packet->GetPacketRevision();
				Sample.MemoryRevision = Packet->GetMemoryRevision();
				Sample.UploadCount = NewState->RenderState->GetMemoryUploadCount_RenderThread();
				Sample.UploadDelta = Sample.UploadCount - UploadBefore;
				Sample.PageTableUploadCount =
					NewState->RenderState->GetMemoryPageTableUploadCount_RenderThread();
				Sample.ResourceGeneration =
					NewState->RenderState->GetMemoryResourceGeneration_RenderThread();
				Sample.ResidencyGeneration =
					NewState->RenderState->GetMemoryResidencyGeneration_RenderThread();
				Sample.ResidentTileCount =
					NewState->RenderState->GetMemoryResidentTileCount_RenderThread();
				Sample.AllocatedPageCount =
					NewState->RenderState->GetAllocatedMemoryPageCount_RenderThread();
				Sample.RequestedDirtyTileCount = Packet->GetDirtyTiles().Num();
				Sample.RequestedRemovedTileCount = Packet->GetRemovedTiles().Num();
				Sample.UploadBytes = Sample.UploadDelta
					* SightWeave::SparseAtlas::PhysicalTileSize
					* SightWeave::SparseAtlas::PhysicalTileSize;
				Sample.PersistentGPUBytes =
					static_cast<uint64>(Sample.AllocatedPageCount) * MemoryPageBytes
					+ FMath::Max(1, Sample.ResidentTileCount) * PageTableEntryBytes;
				Sample.RenderThreadPacketConsumeMicroseconds = ConsumeMicroseconds;
				Sample.RenderThreadResidencyUploadSetupMicroseconds = ResidencyMicroseconds;
				Sample.RenderThreadPageTableSetupMicroseconds = PageTableMicroseconds;
				Sample.RenderThreadTotalSetupMicroseconds =
					(FPlatformTime::Seconds() - TotalSetupStartSeconds) * 1000000.0;
				Sample.bProducedMirrorWork = bMirrorWork;

				if (PacketIndex == NewState->Packets.Num() - 1)
				{
					Result.Availability =
						NewState->RenderState->GetMemoryAvailability_RenderThread();
					if (Result.Availability != ESightWeaveRenderAvailability::Available)
					{
						Result.Status = ESightWeaveMemoryReadbackStatus::Failed;
						Result.Failure = TEXT("Memory mirror is unavailable");
						GraphBuilder.Execute();
						NewState->RenderState->Release_RenderThread(WorldIdentity);
						NewState->RenderState.Reset();
						NewState->Finish(MoveTemp(Result));
						return;
					}
					if (!NewState->RenderState->AddMemoryReadback_RenderThread(
							NewState->SelectedTile))
					{
						Result.Status = ESightWeaveMemoryReadbackStatus::Failed;
						Result.Failure = TEXT("Selected memory tile is not resident");
						GraphBuilder.Execute();
						NewState->RenderState->Release_RenderThread(WorldIdentity);
						NewState->RenderState.Reset();
						NewState->Finish(MoveTemp(Result));
						return;
					}
					FIntRect SlotRect;
					FRDGTextureRef Page =
						NewState->RenderState->RegisterMemoryPageForReadback_RenderThread(
							GraphBuilder,
							NewState->SelectedTile,
							SlotRect,
							NewState->ReadbackAddress);
					if (!Page)
					{
						Result.Status = ESightWeaveMemoryReadbackStatus::Failed;
						Result.Failure = TEXT("Selected memory page could not be registered");
						GraphBuilder.Execute();
						NewState->RenderState->Release_RenderThread(WorldIdentity);
						NewState->RenderState.Reset();
						NewState->Finish(MoveTemp(Result));
						return;
					}
					NewState->Readback = MakeUnique<FRHIGPUTextureReadback>(
						TEXT("SightWeave.M3P5.MemoryTileReadback"));
					AddEnqueueCopyPass(
						GraphBuilder,
						NewState->Readback.Get(),
						Page,
						FResolveRect(
							SlotRect.Min.X,
							SlotRect.Min.Y,
							SlotRect.Max.X,
							SlotRect.Max.Y));
				}
				GraphBuilder.Execute();
			}
			Result.PhysicalAddress = NewState->ReadbackAddress;
			NewState->Result = MoveTemp(Result);
		});
	return Request;
}

void FSightWeaveMemoryTestReadback::Poll()
{
	check(IsInGameThread());
	if (State->bFinished.Load() || State->bPollQueued.Exchange(true))
	{
		return;
	}
	const TSharedRef<FState, ESPMode::ThreadSafe> PollState = State;
	ENQUEUE_RENDER_COMMAND(SightWeavePollMemoryMirrorReadback)(
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
			FSightWeaveMemoryReadbackResult Result = MoveTemp(PollState->Result);
			Result.Status = ESightWeaveMemoryReadbackStatus::Complete;
			Result.Availability = PollState->RenderState->GetMemoryAvailability_RenderThread();
			Result.Width = SightWeave::SparseAtlas::PhysicalTileSize;
			Result.Height = SightWeave::SparseAtlas::PhysicalTileSize;
			for (int32 SampleIndex = 0; SampleIndex < Result.Updates.Num(); ++SampleIndex)
			{
				if (!PollState->GPUStartQueries.IsValidIndex(SampleIndex)
					|| !PollState->GPUEndQueries.IsValidIndex(SampleIndex)
					|| !PollState->GPUStartQueries[SampleIndex].IsValid()
					|| !PollState->GPUEndQueries[SampleIndex].IsValid())
				{
					continue;
				}
				uint64 GPUStartMicroseconds = 0;
				uint64 GPUEndMicroseconds = 0;
				if (RHIGetRenderQueryResult(
						PollState->GPUStartQueries[SampleIndex].GetReference(),
						GPUStartMicroseconds,
						false)
					&& RHIGetRenderQueryResult(
						PollState->GPUEndQueries[SampleIndex].GetReference(),
						GPUEndMicroseconds,
						false)
					&& GPUEndMicroseconds >= GPUStartMicroseconds)
				{
					Result.Updates[SampleIndex].bGPUTimestampAvailable = true;
					Result.Updates[SampleIndex].GPUWorkMicroseconds =
						static_cast<double>(GPUEndMicroseconds - GPUStartMicroseconds);
				}
			}
			void* Source = PollState->Readback->Lock(Result.RowPitchInPixels, &Result.BufferHeight);
			if (!Source
				|| Result.RowPitchInPixels < Result.Width
				|| Result.BufferHeight < Result.Height)
			{
				Result.Status = ESightWeaveMemoryReadbackStatus::Failed;
				Result.Failure = TEXT("Memory PF_G8 readback returned invalid dimensions");
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
			}
			PollState->Readback->Unlock();
			PollState->Readback.Reset();
			PollState->RenderState->RemoveMemoryReadback_RenderThread(
				PollState->SelectedTile,
				PollState->ReadbackAddress);
			PollState->RenderState->Release_RenderThread(
				PollState->Packets[0]->GetScope().WorldIdentity);
			PollState->RenderState.Reset();
			PollState->bPollQueued.Store(false);
			PollState->Finish(MoveTemp(Result));
		});
}

bool FSightWeaveMemoryTestReadback::TryTakeResult(
	FSightWeaveMemoryReadbackResult& OutResult)
{
	check(IsInGameThread());
	if (!State->bFinished.Load())
	{
		return false;
	}
	FScopeLock Lock(&State->ResultGuard);
	OutResult = MoveTemp(State->Result);
	return true;
}

bool FSightWeaveMemoryTestReadback::IsFinished() const
{
	return State->bFinished.Load();
}

#endif
