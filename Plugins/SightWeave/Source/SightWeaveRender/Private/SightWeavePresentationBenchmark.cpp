#include "SightWeavePresentationBenchmark.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/CriticalSection.h"
#include "HAL/PlatformTime.h"
#include "RenderGraphBuilder.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "SightWeaveSparseAtlasRenderState.h"

namespace SightWeavePresentationBenchmarkPrivate
{
	BEGIN_SHADER_PARAMETER_STRUCT(FSightWeaveBenchmarkTimestampParameters, )
		RDG_TEXTURE_ACCESS(Output, ERHIAccess::CopySrc)
	END_SHADER_PARAMETER_STRUCT()

	struct FQueryPair
	{
		FRenderQueryRHIRef Start;
		FRenderQueryRHIRef End;
	};

	void AddTimestampStart(FRDGBuilder& GraphBuilder, const FRenderQueryRHIRef& Query)
	{
		FRHIRenderQuery* RawQuery = Query.GetReference();
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("SightWeave.Presentation.BenchmarkTimestampStart"),
			ERDGPassFlags::None | ERDGPassFlags::NeverCull,
			[RawQuery](FRHICommandList& CommandList)
			{
				CommandList.EndRenderQuery(RawQuery);
			});
	}

	void AddTimestampEnd(
		FRDGBuilder& GraphBuilder,
		const FRenderQueryRHIRef& Query,
		FRDGTextureRef Output)
	{
		FSightWeaveBenchmarkTimestampParameters* Parameters =
			GraphBuilder.AllocParameters<FSightWeaveBenchmarkTimestampParameters>();
		Parameters->Output = Output;
		FRHIRenderQuery* RawQuery = Query.GetReference();
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("SightWeave.Presentation.BenchmarkTimestampEnd"),
			Parameters,
			ERDGPassFlags::Copy | ERDGPassFlags::NeverCull,
			[RawQuery](FRDGAsyncTask, FRHICommandList& CommandList)
			{
				CommandList.EndRenderQuery(RawQuery);
			});
	}
}

using namespace SightWeavePresentationBenchmarkPrivate;

struct FSightWeavePresentationBenchmark::FState final
{
	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet;
	FSightWeaveViewPresentationSelection Selection;
	FIntPoint OutputExtent = FIntPoint::ZeroValue;
	FVector2f WorldMin = FVector2f::ZeroVector;
	FVector2f WorldStep = FVector2f::ZeroVector;
	int32 WarmSampleCount = 0;
	TArray<TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe>> WarmPackets;
	FCriticalSection ResultGuard;
	FSightWeavePresentationBenchmarkResult Result;
	TAtomic<bool> bPollQueued{ false };
	TAtomic<bool> bFinished{ false };
	TUniquePtr<FSightWeaveSparseAtlasRenderState> RenderState;
	FQueryPair ColdQueries;
	FQueryPair ColdFeatherQueries;
	FQueryPair ColdCompositeQueries;
	TArray<FQueryPair> WarmQueries;
	TArray<FQueryPair> WarmFeatherQueries;
	TArray<FQueryPair> WarmCompositeQueries;
	double StartSeconds = FPlatformTime::Seconds();

	void Finish(FSightWeavePresentationBenchmarkResult&& InResult)
	{
		FScopeLock Lock(&ResultGuard);
		Result = MoveTemp(InResult);
		bFinished.Store(true);
	}
};

FSightWeavePresentationBenchmark::FSightWeavePresentationBenchmark(
	TSharedRef<FState, ESPMode::ThreadSafe> InState)
	: State(MoveTemp(InState))
{
}

TSharedRef<FSightWeavePresentationBenchmark, ESPMode::ThreadSafe>
FSightWeavePresentationBenchmark::Start(
	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet,
	FSightWeaveViewPresentationSelection Selection,
	const FIntPoint OutputExtent,
	const FVector2f TestWorldMin,
	const FVector2f TestWorldStep,
	const int32 WarmSampleCount,
	TArray<TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe>> WarmPackets)
{
	check(IsInGameThread());
	const double GTStartSeconds = FPlatformTime::Seconds();
	const TSharedRef<FState, ESPMode::ThreadSafe> NewState = MakeShared<FState, ESPMode::ThreadSafe>();
	NewState->Packet = MoveTemp(Packet);
	NewState->Selection = MoveTemp(Selection);
	NewState->OutputExtent = OutputExtent;
	NewState->WorldMin = TestWorldMin;
	NewState->WorldStep = TestWorldStep;
	NewState->WarmSampleCount = FMath::Max(1, WarmSampleCount);
	NewState->WarmPackets = MoveTemp(WarmPackets);
	NewState->Result.OutputExtent = OutputExtent;
	const TSharedRef<FSightWeavePresentationBenchmark, ESPMode::ThreadSafe> Request =
		MakeShareable(new FSightWeavePresentationBenchmark(NewState));
	NewState->Result.GameThreadSubmitMicroseconds =
		(FPlatformTime::Seconds() - GTStartSeconds) * 1000000.0;

	ENQUEUE_RENDER_COMMAND(SightWeaveStartPresentationBenchmark)(
		[NewState](FRHICommandListImmediate& RHICmdList)
		{
			FSightWeavePresentationBenchmarkResult Result = MoveTemp(NewState->Result);
			if (!GSupportsTimestampRenderQueries
				|| !NewState->Packet.IsValid()
				|| !NewState->Packet->IsValid()
				|| !NewState->Selection.IsValid()
				|| NewState->OutputExtent.X <= 0
				|| NewState->OutputExtent.Y <= 0)
			{
				Result.Status = ESightWeavePresentationReadbackStatus::Failed;
				Result.Failure = TEXT("Benchmark requires valid inputs and absolute GPU timestamps");
				NewState->Finish(MoveTemp(Result));
				return;
			}

			const FSightWeaveRenderWorldIdentity WorldIdentity = NewState->Packet->GetWorldIdentity();
			NewState->RenderState = MakeUnique<FSightWeaveSparseAtlasRenderState>(WorldIdentity);
			const double BindingStart = FPlatformTime::Seconds();
			NewState->RenderState->SubmitPresentationSelection_RenderThread(NewState->Selection);
			NewState->RenderState->SubmitPacket_RenderThread(NewState->Packet);
			Result.RenderThreadBindingSubmitMicroseconds =
				(FPlatformTime::Seconds() - BindingStart) * 1000000.0;

			NewState->ColdQueries.Start = RHICreateRenderQuery(RQT_AbsoluteTime);
			NewState->ColdQueries.End = RHICreateRenderQuery(RQT_AbsoluteTime);
			NewState->ColdFeatherQueries.Start = RHICreateRenderQuery(RQT_AbsoluteTime);
			NewState->ColdFeatherQueries.End = RHICreateRenderQuery(RQT_AbsoluteTime);
			NewState->ColdCompositeQueries.Start = RHICreateRenderQuery(RQT_AbsoluteTime);
			NewState->ColdCompositeQueries.End = RHICreateRenderQuery(RQT_AbsoluteTime);
			if (!NewState->ColdQueries.Start.IsValid() || !NewState->ColdQueries.End.IsValid())
			{
				Result.Status = ESightWeavePresentationReadbackStatus::Failed;
				Result.Failure = TEXT("Could not allocate cold benchmark timestamp queries");
				NewState->RenderState->Release_RenderThread(WorldIdentity);
				NewState->RenderState.Reset();
				NewState->Finish(MoveTemp(Result));
				return;
			}

			FRDGBuilder ColdGraph(RHICmdList, RDG_EVENT_NAME("SightWeave.Presentation.BenchmarkCold"));
			AddTimestampStart(ColdGraph, NewState->ColdQueries.Start);
			const double ColdSetupStart = FPlatformTime::Seconds();
			NewState->RenderState->ProcessPending_RenderThread(ColdGraph);
			NewState->RenderState->PreparePresentationResources_RenderThread(ColdGraph);
			AddTimestampStart(ColdGraph, NewState->ColdFeatherQueries.Start);
			NewState->RenderState->ProcessVisualFeather_RenderThread(ColdGraph);
			AddTimestampStart(ColdGraph, NewState->ColdFeatherQueries.End);
			AddTimestampStart(ColdGraph, NewState->ColdCompositeQueries.Start);
			FRDGTextureRef ColdOutput = NewState->RenderState->AddPresentationBenchmarkComposite_RenderThread(
				ColdGraph, NewState->OutputExtent, NewState->WorldMin, NewState->WorldStep);
			Result.ColdRenderThreadSetupMicroseconds =
				(FPlatformTime::Seconds() - ColdSetupStart) * 1000000.0;
			if (!ColdOutput)
			{
				Result.Status = ESightWeavePresentationReadbackStatus::Failed;
				Result.Failure = TEXT("Cold benchmark output creation failed");
				ColdGraph.Execute();
				NewState->RenderState->Release_RenderThread(WorldIdentity);
				NewState->RenderState.Reset();
				NewState->Finish(MoveTemp(Result));
				return;
			}
			AddTimestampEnd(ColdGraph, NewState->ColdCompositeQueries.End, ColdOutput);
			AddTimestampEnd(ColdGraph, NewState->ColdQueries.End, ColdOutput);
			ColdGraph.Execute();

			Result.InitialPageTableUploadCount = NewState->RenderState->GetPageTableUploadCount_RenderThread();
			Result.InitialPageAllocationCount = NewState->RenderState->GetPageAllocationCount_RenderThread();
			Result.InitialScratchAllocationCount = NewState->RenderState->GetScratchAllocationCount_RenderThread();
			Result.InitialResourceGeneration = NewState->RenderState->GetResourceGeneration_RenderThread();
			Result.InitialFeatherTileDispatchCount =
				NewState->RenderState->GetFeatherTileDispatchCount_RenderThread();
			Result.ResidentTileCount = NewState->RenderState->GetResidentTileCount_RenderThread();
			Result.AllocatedPageCount = NewState->RenderState->GetAllocatedPageCount_RenderThread();
			Result.AllocatedFeatherPageCount =
				NewState->RenderState->GetAllocatedFeatherPageCount_RenderThread();

			NewState->WarmQueries.SetNum(NewState->WarmSampleCount);
			NewState->WarmFeatherQueries.SetNum(NewState->WarmSampleCount);
			NewState->WarmCompositeQueries.SetNum(NewState->WarmSampleCount);
			Result.WarmRenderThreadPacketSubmitMicroseconds.Reserve(NewState->WarmSampleCount);
			Result.WarmRenderThreadMaskSetupMicroseconds.Reserve(NewState->WarmSampleCount);
			Result.WarmRenderThreadFeatherSetupMicroseconds.Reserve(NewState->WarmSampleCount);
			Result.WarmRenderThreadViewSetupMicroseconds.Reserve(NewState->WarmSampleCount);
			Result.WarmRenderThreadCompositeSetupMicroseconds.Reserve(NewState->WarmSampleCount);
			Result.WarmRequestedDirtyTileCounts.Reserve(NewState->WarmSampleCount);
			Result.WarmFeatherTileDispatchCounts.Reserve(NewState->WarmSampleCount);
			for (int32 Index = 0; Index < NewState->WarmSampleCount; ++Index)
			{
				FQueryPair& Queries = NewState->WarmQueries[Index];
				FQueryPair& FeatherQueries = NewState->WarmFeatherQueries[Index];
				FQueryPair& CompositeQueries = NewState->WarmCompositeQueries[Index];
				Queries.Start = RHICreateRenderQuery(RQT_AbsoluteTime);
				Queries.End = RHICreateRenderQuery(RQT_AbsoluteTime);
				FeatherQueries.Start = RHICreateRenderQuery(RQT_AbsoluteTime);
				FeatherQueries.End = RHICreateRenderQuery(RQT_AbsoluteTime);
				CompositeQueries.Start = RHICreateRenderQuery(RQT_AbsoluteTime);
				CompositeQueries.End = RHICreateRenderQuery(RQT_AbsoluteTime);
				if (!Queries.Start.IsValid() || !Queries.End.IsValid())
				{
					Result.Status = ESightWeavePresentationReadbackStatus::Failed;
					Result.Failure = TEXT("Could not allocate warmed benchmark timestamp queries");
					NewState->RenderState->Release_RenderThread(WorldIdentity);
					NewState->RenderState.Reset();
					NewState->Finish(MoveTemp(Result));
					return;
				}
				const TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> WarmPacket =
					NewState->WarmPackets.IsEmpty()
						? nullptr
						: NewState->WarmPackets[Index % NewState->WarmPackets.Num()];
				const double PacketSubmitStart = FPlatformTime::Seconds();
				if (WarmPacket.IsValid())
				{
					NewState->RenderState->SubmitPacket_RenderThread(WarmPacket);
				}
				Result.WarmRenderThreadPacketSubmitMicroseconds.Add(
					(FPlatformTime::Seconds() - PacketSubmitStart) * 1000000.0);
				Result.WarmRequestedDirtyTileCounts.Add(
					WarmPacket.IsValid() ? WarmPacket->GetDirtyTileIndices().Num() : 0);
				FRDGBuilder WarmGraph(RHICmdList, RDG_EVENT_NAME("SightWeave.Presentation.BenchmarkWarm"));
				AddTimestampStart(WarmGraph, Queries.Start);
				const double MaskSetupStart = FPlatformTime::Seconds();
				NewState->RenderState->ProcessPending_RenderThread(WarmGraph);
				Result.WarmRenderThreadMaskSetupMicroseconds.Add(
					(FPlatformTime::Seconds() - MaskSetupStart) * 1000000.0);
				const double ViewSetupStart = FPlatformTime::Seconds();
				NewState->RenderState->PreparePresentationResources_RenderThread(WarmGraph);
				Result.WarmRenderThreadViewSetupMicroseconds.Add(
					(FPlatformTime::Seconds() - ViewSetupStart) * 1000000.0);
				const uint64 FeatherDispatchStart =
					NewState->RenderState->GetFeatherTileDispatchCount_RenderThread();
				AddTimestampStart(WarmGraph, FeatherQueries.Start);
				const double FeatherSetupStart = FPlatformTime::Seconds();
				NewState->RenderState->ProcessVisualFeather_RenderThread(WarmGraph);
				Result.WarmRenderThreadFeatherSetupMicroseconds.Add(
					(FPlatformTime::Seconds() - FeatherSetupStart) * 1000000.0);
				AddTimestampStart(WarmGraph, FeatherQueries.End);
				Result.WarmFeatherTileDispatchCounts.Add(
					NewState->RenderState->GetFeatherTileDispatchCount_RenderThread()
						- FeatherDispatchStart);
				AddTimestampStart(WarmGraph, CompositeQueries.Start);
				const double CompositeSetupStart = FPlatformTime::Seconds();
				FRDGTextureRef Output = NewState->RenderState->AddPresentationBenchmarkComposite_RenderThread(
					WarmGraph, NewState->OutputExtent, NewState->WorldMin, NewState->WorldStep);
				Result.WarmRenderThreadCompositeSetupMicroseconds.Add(
					(FPlatformTime::Seconds() - CompositeSetupStart) * 1000000.0);
				AddTimestampEnd(WarmGraph, CompositeQueries.End, Output);
				AddTimestampEnd(WarmGraph, Queries.End, Output);
				WarmGraph.Execute();
			}

			Result.FinalPageTableUploadCount = NewState->RenderState->GetPageTableUploadCount_RenderThread();
			Result.FinalPageAllocationCount = NewState->RenderState->GetPageAllocationCount_RenderThread();
			Result.FinalScratchAllocationCount = NewState->RenderState->GetScratchAllocationCount_RenderThread();
			Result.FinalResourceGeneration = NewState->RenderState->GetResourceGeneration_RenderThread();
			Result.FinalFeatherTileDispatchCount =
				NewState->RenderState->GetFeatherTileDispatchCount_RenderThread();
			Result.FeatherPageAllocationCount =
				NewState->RenderState->GetFeatherPageAllocationCount_RenderThread();
			Result.FeatherScratchAllocationCount =
				NewState->RenderState->GetFeatherScratchAllocationCount_RenderThread();
			Result.FeatherResourceGeneration =
				NewState->RenderState->GetFeatherResourceGeneration_RenderThread();
			Result.PersistentGPUBytes = static_cast<uint64>(Result.AllocatedPageCount) * 2048ull * 2048ull
				+ static_cast<uint64>(Result.AllocatedFeatherPageCount) * 2048ull * 2048ull
				+ 3ull * 256ull * 256ull
				+ (Result.FeatherScratchAllocationCount > 0
					? 2ull * SightWeave::VisualFeather::TransformWorkSize
						* SightWeave::VisualFeather::TransformWorkSize * 8ull
					: 0ull)
				+ static_cast<uint64>(Result.ResidentTileCount) * sizeof(FIntVector4);
			Result.TransientOutputBytes = static_cast<uint64>(NewState->OutputExtent.X)
				* static_cast<uint64>(NewState->OutputExtent.Y) * 4ull;
			NewState->Result = MoveTemp(Result);
		});
	return Request;
}

void FSightWeavePresentationBenchmark::Poll()
{
	check(IsInGameThread());
	if (State->bFinished.Load() || State->bPollQueued.Exchange(true))
	{
		return;
	}
	const TSharedRef<FState, ESPMode::ThreadSafe> PollState = State;
	ENQUEUE_RENDER_COMMAND(SightWeavePollPresentationBenchmark)(
		[PollState](FRHICommandListImmediate& RHICmdList)
		{
			if (PollState->bFinished.Load())
			{
				PollState->bPollQueued.Store(false);
				return;
			}
			uint64 ColdStart = 0;
			uint64 ColdEnd = 0;
			uint64 ColdFeatherStart = 0;
			uint64 ColdFeatherEnd = 0;
			uint64 ColdCompositeStart = 0;
			uint64 ColdCompositeEnd = 0;
			if (!RHIGetRenderQueryResult(PollState->ColdQueries.Start.GetReference(), ColdStart, false)
				|| !RHIGetRenderQueryResult(PollState->ColdQueries.End.GetReference(), ColdEnd, false)
				|| !RHIGetRenderQueryResult(PollState->ColdFeatherQueries.Start.GetReference(), ColdFeatherStart, false)
				|| !RHIGetRenderQueryResult(PollState->ColdFeatherQueries.End.GetReference(), ColdFeatherEnd, false)
				|| !RHIGetRenderQueryResult(PollState->ColdCompositeQueries.Start.GetReference(), ColdCompositeStart, false)
				|| !RHIGetRenderQueryResult(PollState->ColdCompositeQueries.End.GetReference(), ColdCompositeEnd, false))
			{
				PollState->bPollQueued.Store(false);
				return;
			}
			TArray<double> WarmGPU;
			TArray<double> WarmGPUFeather;
			TArray<double> WarmGPUComposite;
			WarmGPU.Reserve(PollState->WarmQueries.Num());
			WarmGPUFeather.Reserve(PollState->WarmQueries.Num());
			WarmGPUComposite.Reserve(PollState->WarmQueries.Num());
			for (int32 Index = 0; Index < PollState->WarmQueries.Num(); ++Index)
			{
				const FQueryPair& Queries = PollState->WarmQueries[Index];
				const FQueryPair& FeatherQueries = PollState->WarmFeatherQueries[Index];
				const FQueryPair& CompositeQueries = PollState->WarmCompositeQueries[Index];
				uint64 Start = 0;
				uint64 End = 0;
				uint64 FeatherStart = 0;
				uint64 FeatherEnd = 0;
				uint64 CompositeStart = 0;
				uint64 CompositeEnd = 0;
				if (!RHIGetRenderQueryResult(Queries.Start.GetReference(), Start, false)
					|| !RHIGetRenderQueryResult(Queries.End.GetReference(), End, false)
					|| !RHIGetRenderQueryResult(FeatherQueries.Start.GetReference(), FeatherStart, false)
					|| !RHIGetRenderQueryResult(FeatherQueries.End.GetReference(), FeatherEnd, false)
					|| !RHIGetRenderQueryResult(CompositeQueries.Start.GetReference(), CompositeStart, false)
					|| !RHIGetRenderQueryResult(CompositeQueries.End.GetReference(), CompositeEnd, false))
				{
					PollState->bPollQueued.Store(false);
					return;
				}
				WarmGPU.Add(End >= Start ? static_cast<double>(End - Start) : 0.0);
				WarmGPUFeather.Add(FeatherEnd >= FeatherStart
					? static_cast<double>(FeatherEnd - FeatherStart) : 0.0);
				WarmGPUComposite.Add(CompositeEnd >= CompositeStart
					? static_cast<double>(CompositeEnd - CompositeStart) : 0.0);
			}

			FSightWeavePresentationBenchmarkResult Result = MoveTemp(PollState->Result);
			Result.Status = ESightWeavePresentationReadbackStatus::Complete;
			Result.ColdGPUTotalMicroseconds =
				ColdEnd >= ColdStart ? static_cast<double>(ColdEnd - ColdStart) : 0.0;
			Result.ColdGPUFeatherMicroseconds = ColdFeatherEnd >= ColdFeatherStart
				? static_cast<double>(ColdFeatherEnd - ColdFeatherStart) : 0.0;
			Result.ColdGPUCompositeMicroseconds = ColdCompositeEnd >= ColdCompositeStart
				? static_cast<double>(ColdCompositeEnd - ColdCompositeStart) : 0.0;
			Result.WarmGPUFeatherMicroseconds = MoveTemp(WarmGPUFeather);
			Result.WarmGPUCompositeMicroseconds = MoveTemp(WarmGPUComposite);
			Result.WarmGPUTotalMicroseconds = MoveTemp(WarmGPU);
			PollState->RenderState->Release_RenderThread(PollState->Packet->GetWorldIdentity());
			PollState->RenderState.Reset();
			PollState->bPollQueued.Store(false);
			PollState->Finish(MoveTemp(Result));
		});
}

bool FSightWeavePresentationBenchmark::TryTakeResult(
	FSightWeavePresentationBenchmarkResult& OutResult)
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

bool FSightWeavePresentationBenchmark::IsFinished() const
{
	return State->bFinished.Load();
}

#endif
