#include "SightWeaveMemoryPresentationTestReadback.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/CriticalSection.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "RHIGPUReadback.h"
#include "SightWeaveSparseAtlasRenderState.h"

struct FSightWeaveMemoryPresentationTestReadback::FState final
{
	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> LivePacket;
	FSightWeaveViewPresentationSelection Selection;
	TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> MemoryPacket;
	TSharedPtr<const FSightWeaveStaticEnvironmentPacket, ESPMode::ThreadSafe> StaticPacket;
	TArray<FVector2f> WorldPositions;
	TArray<FVector4f> SceneColors;
	bool bForceMemoryUnavailable = false;
	FCriticalSection Guard;
	FSightWeaveMemoryPresentationReadbackResult Result;
	TAtomic<bool> bPollQueued{ false };
	TAtomic<bool> bFinished{ false };
	TUniquePtr<FSightWeaveSparseAtlasRenderState> RenderState;
	TUniquePtr<FRHIGPUTextureReadback> Readback;

	void Finish(FSightWeaveMemoryPresentationReadbackResult&& InResult)
	{
		FScopeLock Lock(&Guard);
		Result = MoveTemp(InResult);
		bFinished.Store(true);
	}
};

FSightWeaveMemoryPresentationTestReadback::FSightWeaveMemoryPresentationTestReadback(
	TSharedRef<FState, ESPMode::ThreadSafe> InState)
	: State(MoveTemp(InState))
{
}

TSharedRef<FSightWeaveMemoryPresentationTestReadback, ESPMode::ThreadSafe>
FSightWeaveMemoryPresentationTestReadback::Start(
	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> LivePacket,
	FSightWeaveViewPresentationSelection Selection,
	TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> MemoryPacket,
	TSharedPtr<const FSightWeaveStaticEnvironmentPacket, ESPMode::ThreadSafe> StaticPacket,
	TArray<FVector2f> WorldPositions,
	TArray<FVector4f> SceneColors,
	const bool bForceMemoryUnavailable)
{
	check(IsInGameThread());
	const TSharedRef<FState, ESPMode::ThreadSafe> NewState =
		MakeShared<FState, ESPMode::ThreadSafe>();
	NewState->LivePacket = MoveTemp(LivePacket);
	NewState->Selection = MoveTemp(Selection);
	NewState->MemoryPacket = MoveTemp(MemoryPacket);
	NewState->StaticPacket = MoveTemp(StaticPacket);
	NewState->WorldPositions = MoveTemp(WorldPositions);
	NewState->SceneColors = MoveTemp(SceneColors);
	NewState->bForceMemoryUnavailable = bForceMemoryUnavailable;
	const TSharedRef<FSightWeaveMemoryPresentationTestReadback, ESPMode::ThreadSafe> Request =
		MakeShareable(new FSightWeaveMemoryPresentationTestReadback(NewState));
	ENQUEUE_RENDER_COMMAND(SightWeaveStartMemoryPresentationReadback)(
		[NewState](FRHICommandListImmediate& RHICmdList)
		{
			FSightWeaveMemoryPresentationReadbackResult Result;
			if (!NewState->LivePacket.IsValid()
				|| !NewState->LivePacket->IsValid()
				|| !NewState->MemoryPacket.IsValid()
				|| !NewState->StaticPacket.IsValid()
				|| NewState->WorldPositions.IsEmpty()
				|| NewState->WorldPositions.Num() != NewState->SceneColors.Num())
			{
				Result.Failure = TEXT("Invalid M3.5 presentation readback input");
				NewState->Finish(MoveTemp(Result));
				return;
			}
			const FSightWeaveRenderWorldIdentity WorldIdentity =
				NewState->LivePacket->GetWorldIdentity();
			NewState->RenderState = MakeUnique<FSightWeaveSparseAtlasRenderState>(WorldIdentity);
			NewState->RenderState->SubmitPresentationSelection_RenderThread(NewState->Selection);
			NewState->RenderState->SubmitPacket_RenderThread(NewState->LivePacket);
			NewState->RenderState->SubmitMemoryPacket_RenderThread(NewState->MemoryPacket);
			NewState->RenderState->SubmitStaticEnvironmentPacket_RenderThread(NewState->StaticPacket);
			FRDGBuilder GraphBuilder(
				RHICmdList,
				RDG_EVENT_NAME("SightWeave.MemoryPresentation.Readback"));
			NewState->RenderState->ProcessPending_RenderThread(GraphBuilder);
			NewState->RenderState->ProcessMemoryPending_RenderThread(GraphBuilder);
			NewState->RenderState->ProcessStaticEnvironmentPending_RenderThread(GraphBuilder);
			NewState->RenderState->PreparePresentationResources_RenderThread(GraphBuilder);
			NewState->RenderState->PrepareMemoryPresentationResources_RenderThread(GraphBuilder);
			NewState->RenderState->PrepareStaticEnvironmentPresentationResources_RenderThread(
				GraphBuilder);
			FRDGTextureRef Output =
				NewState->RenderState->AddMemoryPresentationTestComposite_RenderThread(
					GraphBuilder,
					NewState->WorldPositions,
					NewState->SceneColors,
					NewState->bForceMemoryUnavailable);
			Result.LiveAvailability = NewState->RenderState->GetAvailability_RenderThread();
			Result.MemoryAvailability =
				NewState->RenderState->GetMemoryAvailability_RenderThread();
			Result.StaticEnvironmentAvailability =
				NewState->RenderState->GetStaticEnvironmentAvailability_RenderThread();
			if (!Output || GUsingNullRHI)
			{
				Result.Failure = TEXT("M3.5 composite output unavailable");
				GraphBuilder.Execute();
				NewState->RenderState->Release_RenderThread(WorldIdentity);
				NewState->RenderState.Reset();
				NewState->Finish(MoveTemp(Result));
				return;
			}
			NewState->Readback = MakeUnique<FRHIGPUTextureReadback>(
				TEXT("SightWeave.M3P5.MemoryPresentationReadback"));
			AddEnqueueCopyPass(GraphBuilder, NewState->Readback.Get(), Output);
			GraphBuilder.Execute();
			NewState->Result = MoveTemp(Result);
		});
	return Request;
}

void FSightWeaveMemoryPresentationTestReadback::Poll()
{
	check(IsInGameThread());
	if (State->bFinished.Load() || State->bPollQueued.Exchange(true))
	{
		return;
	}
	const TSharedRef<FState, ESPMode::ThreadSafe> PollState = State;
	ENQUEUE_RENDER_COMMAND(SightWeavePollMemoryPresentationReadback)(
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
			FSightWeaveMemoryPresentationReadbackResult Result = MoveTemp(PollState->Result);
			Result.Width = PollState->WorldPositions.Num();
			void* Source = PollState->Readback->Lock(
				Result.RowPitchInPixels,
				&Result.BufferHeight);
			if (!Source || Result.RowPitchInPixels < Result.Width || Result.BufferHeight < 1)
			{
				Result.Failure = TEXT("M3.5 RGBA8 readback dimensions invalid");
			}
			else
			{
				Result.Pixels.SetNumUninitialized(Result.Width);
				const uint8* SourceBytes = static_cast<const uint8*>(Source);
				for (int32 PixelIndex = 0; PixelIndex < Result.Width; ++PixelIndex)
				{
					const uint8* Pixel = SourceBytes + PixelIndex * 4;
					Result.Pixels[PixelIndex] = FColor(Pixel[0], Pixel[1], Pixel[2], Pixel[3]);
				}
				Result.bComplete = true;
			}
			PollState->Readback->Unlock();
			PollState->Readback.Reset();
			PollState->RenderState->Release_RenderThread(
				PollState->LivePacket->GetWorldIdentity());
			PollState->RenderState.Reset();
			PollState->bPollQueued.Store(false);
			PollState->Finish(MoveTemp(Result));
		});
}

bool FSightWeaveMemoryPresentationTestReadback::TryTakeResult(
	FSightWeaveMemoryPresentationReadbackResult& OutResult)
{
	check(IsInGameThread());
	if (!State->bFinished.Load())
	{
		return false;
	}
	FScopeLock Lock(&State->Guard);
	OutResult = MoveTemp(State->Result);
	return true;
}

bool FSightWeaveMemoryPresentationTestReadback::IsFinished() const
{
	return State->bFinished.Load();
}

#endif
