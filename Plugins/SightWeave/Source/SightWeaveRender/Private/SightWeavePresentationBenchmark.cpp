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
	FCriticalSection ResultGuard;
	FSightWeavePresentationBenchmarkResult Result;
	TAtomic<bool> bPollQueued{ false };
	TAtomic<bool> bFinished{ false };
	TUniquePtr<FSightWeaveSparseAtlasRenderState> RenderState;
	FQueryPair ColdQueries;
	TArray<FQueryPair> WarmQueries;
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
	const int32 WarmSampleCount)
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
			AddTimestampEnd(ColdGraph, NewState->ColdQueries.End, ColdOutput);
			ColdGraph.Execute();

			Result.InitialPageTableUploadCount = NewState->RenderState->GetPageTableUploadCount_RenderThread();
			Result.InitialPageAllocationCount = NewState->RenderState->GetPageAllocationCount_RenderThread();
			Result.InitialScratchAllocationCount = NewState->RenderState->GetScratchAllocationCount_RenderThread();
			Result.InitialResourceGeneration = NewState->RenderState->GetResourceGeneration_RenderThread();
			Result.ResidentTileCount = NewState->RenderState->GetResidentTileCount_RenderThread();
			Result.AllocatedPageCount = NewState->RenderState->GetAllocatedPageCount_RenderThread();

			NewState->WarmQueries.SetNum(NewState->WarmSampleCount);
			Result.WarmRenderThreadViewSetupMicroseconds.Reserve(NewState->WarmSampleCount);
			Result.WarmRenderThreadCompositeSetupMicroseconds.Reserve(NewState->WarmSampleCount);
			for (int32 Index = 0; Index < NewState->WarmSampleCount; ++Index)
			{
				FQueryPair& Queries = NewState->WarmQueries[Index];
				Queries.Start = RHICreateRenderQuery(RQT_AbsoluteTime);
				Queries.End = RHICreateRenderQuery(RQT_AbsoluteTime);
				if (!Queries.Start.IsValid() || !Queries.End.IsValid())
				{
					Result.Status = ESightWeavePresentationReadbackStatus::Failed;
					Result.Failure = TEXT("Could not allocate warmed benchmark timestamp queries");
					NewState->RenderState->Release_RenderThread(WorldIdentity);
					NewState->RenderState.Reset();
					NewState->Finish(MoveTemp(Result));
					return;
				}
				FRDGBuilder WarmGraph(RHICmdList, RDG_EVENT_NAME("SightWeave.Presentation.BenchmarkWarm"));
				AddTimestampStart(WarmGraph, Queries.Start);
				const double ViewSetupStart = FPlatformTime::Seconds();
				NewState->RenderState->PreparePresentationResources_RenderThread(WarmGraph);
				Result.WarmRenderThreadViewSetupMicroseconds.Add(
					(FPlatformTime::Seconds() - ViewSetupStart) * 1000000.0);
				const double CompositeSetupStart = FPlatformTime::Seconds();
				FRDGTextureRef Output = NewState->RenderState->AddPresentationBenchmarkComposite_RenderThread(
					WarmGraph, NewState->OutputExtent, NewState->WorldMin, NewState->WorldStep);
				Result.WarmRenderThreadCompositeSetupMicroseconds.Add(
					(FPlatformTime::Seconds() - CompositeSetupStart) * 1000000.0);
				AddTimestampEnd(WarmGraph, Queries.End, Output);
				WarmGraph.Execute();
			}

			Result.FinalPageTableUploadCount = NewState->RenderState->GetPageTableUploadCount_RenderThread();
			Result.FinalPageAllocationCount = NewState->RenderState->GetPageAllocationCount_RenderThread();
			Result.FinalScratchAllocationCount = NewState->RenderState->GetScratchAllocationCount_RenderThread();
			Result.FinalResourceGeneration = NewState->RenderState->GetResourceGeneration_RenderThread();
			Result.PersistentGPUBytes = static_cast<uint64>(Result.AllocatedPageCount) * 2048ull * 2048ull
				+ 3ull * 256ull * 256ull
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
			if (!RHIGetRenderQueryResult(PollState->ColdQueries.Start.GetReference(), ColdStart, false)
				|| !RHIGetRenderQueryResult(PollState->ColdQueries.End.GetReference(), ColdEnd, false))
			{
				PollState->bPollQueued.Store(false);
				return;
			}
			TArray<double> WarmGPU;
			WarmGPU.Reserve(PollState->WarmQueries.Num());
			for (const FQueryPair& Queries : PollState->WarmQueries)
			{
				uint64 Start = 0;
				uint64 End = 0;
				if (!RHIGetRenderQueryResult(Queries.Start.GetReference(), Start, false)
					|| !RHIGetRenderQueryResult(Queries.End.GetReference(), End, false))
				{
					PollState->bPollQueued.Store(false);
					return;
				}
				WarmGPU.Add(End >= Start ? static_cast<double>(End - Start) : 0.0);
			}

			FSightWeavePresentationBenchmarkResult Result = MoveTemp(PollState->Result);
			Result.Status = ESightWeavePresentationReadbackStatus::Complete;
			Result.ColdGPUTotalMicroseconds =
				ColdEnd >= ColdStart ? static_cast<double>(ColdEnd - ColdStart) : 0.0;
			Result.WarmGPUCompositeMicroseconds = MoveTemp(WarmGPU);
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
