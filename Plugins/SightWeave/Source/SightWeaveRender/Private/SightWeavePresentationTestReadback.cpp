#include "SightWeavePresentationTestReadback.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/CriticalSection.h"
#include "HAL/PlatformTime.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "RHIGPUReadback.h"
#include "SightWeaveSparseAtlasRenderState.h"

namespace SightWeavePresentationTestReadbackPrivate
{
	BEGIN_SHADER_PARAMETER_STRUCT(FSightWeavePresentationTimestampParameters, )
		RDG_TEXTURE_ACCESS(Output, ERHIAccess::CopySrc)
	END_SHADER_PARAMETER_STRUCT()
}

using namespace SightWeavePresentationTestReadbackPrivate;

struct FSightWeavePresentationTestReadback::FState final
{
	FState(
		TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> InPacket,
		FSightWeaveViewPresentationSelection InSelection,
		TArray<FVector2f> InPositions,
		TArray<FVector4f> InColors,
		const int32 InRepeatedViewSetups)
		: Packet(MoveTemp(InPacket))
		, Selection(MoveTemp(InSelection))
		, Positions(MoveTemp(InPositions))
		, Colors(MoveTemp(InColors))
		, RepeatedViewSetups(InRepeatedViewSetups)
	{
	}

	void Finish(FSightWeavePresentationReadbackResult&& InResult)
	{
		FScopeLock Lock(&ResultGuard);
		Result = MoveTemp(InResult);
		bFinished.Store(true);
	}

	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet;
	FSightWeaveViewPresentationSelection Selection;
	TArray<FVector2f> Positions;
	TArray<FVector4f> Colors;
	int32 RepeatedViewSetups = 0;
	FCriticalSection ResultGuard;
	FSightWeavePresentationReadbackResult Result;
	TAtomic<bool> bPollQueued{ false };
	TAtomic<bool> bFinished{ false };
	TUniquePtr<FSightWeaveSparseAtlasRenderState> RenderState;
	TUniquePtr<FRHIGPUTextureReadback> Readback;
	FRenderQueryRHIRef GPUStartQuery;
	FRenderQueryRHIRef GPUEndQuery;
	double StartSeconds = FPlatformTime::Seconds();
	bool bGPUTimestampsIssued = false;
};

FSightWeavePresentationTestReadback::FSightWeavePresentationTestReadback(
	TSharedRef<FState, ESPMode::ThreadSafe> InState)
	: State(MoveTemp(InState))
{
}

TSharedRef<FSightWeavePresentationTestReadback, ESPMode::ThreadSafe>
FSightWeavePresentationTestReadback::Start(
	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet,
	FSightWeaveViewPresentationSelection Selection,
	TArray<FVector2f> TranslatedWorldPositions,
	TArray<FVector4f> SceneColors,
	const int32 RepeatedCameraOnlyViewSetups)
{
	check(IsInGameThread());
	const TSharedRef<FState, ESPMode::ThreadSafe> NewState = MakeShared<FState, ESPMode::ThreadSafe>(
		MoveTemp(Packet),
		MoveTemp(Selection),
		MoveTemp(TranslatedWorldPositions),
		MoveTemp(SceneColors),
		FMath::Max(0, RepeatedCameraOnlyViewSetups));
	const TSharedRef<FSightWeavePresentationTestReadback, ESPMode::ThreadSafe> Request =
		MakeShareable(new FSightWeavePresentationTestReadback(NewState));
	ENQUEUE_RENDER_COMMAND(SightWeaveStartPresentationReadback)(
		[NewState](FRHICommandListImmediate& RHICmdList)
		{
			FSightWeavePresentationReadbackResult Failure;
			if (!NewState->Packet.IsValid()
				|| !NewState->Packet->IsValid()
				|| !NewState->Selection.IsValid()
				|| NewState->Positions.IsEmpty()
				|| NewState->Positions.Num() != NewState->Colors.Num())
			{
				Failure.Status = ESightWeavePresentationReadbackStatus::Failed;
				Failure.Failure = TEXT("Invalid packet, selection, or synthetic presentation inputs");
				NewState->Finish(MoveTemp(Failure));
				return;
			}

			const FSightWeaveRenderWorldIdentity WorldIdentity =
				NewState->Packet->GetWorldIdentity();
			NewState->RenderState = MakeUnique<FSightWeaveSparseAtlasRenderState>(WorldIdentity);
			NewState->RenderState->SubmitPresentationSelection_RenderThread(NewState->Selection);
			NewState->RenderState->SubmitPacket_RenderThread(NewState->Packet);
			FRDGBuilder GraphBuilder(
				RHICmdList,
				RDG_EVENT_NAME("SightWeave.Presentation.TestReadback"));
			NewState->RenderState->ProcessPending_RenderThread(GraphBuilder);
			if (NewState->RenderState->GetAvailability_RenderThread()
				!= ESightWeaveRenderAvailability::Available)
			{
				Failure.Status = ESightWeavePresentationReadbackStatus::Failed;
				Failure.Failure = TEXT("Presentation render state is unavailable");
				Failure.InitialPageTableUploadCount =
					NewState->RenderState->GetPageTableUploadCount_RenderThread();
				Failure.FinalPageTableUploadCount = Failure.InitialPageTableUploadCount;
				GraphBuilder.Execute();
				NewState->RenderState->Release_RenderThread(WorldIdentity);
				NewState->RenderState.Reset();
				NewState->Finish(MoveTemp(Failure));
				return;
			}
			NewState->RenderState->PreparePresentationResources_RenderThread(GraphBuilder);
			Failure.InitialPageTableUploadCount =
				NewState->RenderState->GetPageTableUploadCount_RenderThread();

			if (GSupportsTimestampRenderQueries)
			{
				NewState->GPUStartQuery = RHICreateRenderQuery(RQT_AbsoluteTime);
				NewState->GPUEndQuery = RHICreateRenderQuery(RQT_AbsoluteTime);
				if (NewState->GPUStartQuery.IsValid() && NewState->GPUEndQuery.IsValid())
				{
					FRHIRenderQuery* StartQuery = NewState->GPUStartQuery.GetReference();
					GraphBuilder.AddPass(
						RDG_EVENT_NAME("SightWeave.Presentation.TimestampStart"),
						ERDGPassFlags::None | ERDGPassFlags::NeverCull,
						[StartQuery](FRHICommandList& CommandList)
						{
							CommandList.EndRenderQuery(StartQuery);
						});
					NewState->bGPUTimestampsIssued = true;
				}
			}
			const double SetupStartSeconds = FPlatformTime::Seconds();
			FRDGTextureRef Output = NewState->RenderState->AddPresentationTestComposite_RenderThread(
				GraphBuilder,
				NewState->Positions,
				NewState->Colors);
			Failure.RenderThreadCompositeSetupMicroseconds =
				(FPlatformTime::Seconds() - SetupStartSeconds) * 1000000.0;
			if (!Output)
			{
				Failure.Status = ESightWeavePresentationReadbackStatus::Failed;
				Failure.Failure = TEXT("Presentation test target could not be created");
				GraphBuilder.Execute();
				NewState->RenderState->Release_RenderThread(WorldIdentity);
				NewState->RenderState.Reset();
				NewState->Finish(MoveTemp(Failure));
				return;
			}
			if (NewState->bGPUTimestampsIssued)
			{
				FSightWeavePresentationTimestampParameters* TimestampParameters =
					GraphBuilder.AllocParameters<FSightWeavePresentationTimestampParameters>();
				TimestampParameters->Output = Output;
				FRHIRenderQuery* EndQuery = NewState->GPUEndQuery.GetReference();
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("SightWeave.Presentation.TimestampEnd"),
					TimestampParameters,
					ERDGPassFlags::Copy | ERDGPassFlags::NeverCull,
					[EndQuery](FRDGAsyncTask, FRHICommandList& CommandList)
					{
						CommandList.EndRenderQuery(EndQuery);
					});
			}
			NewState->Readback = MakeUnique<FRHIGPUTextureReadback>(
				TEXT("SightWeave.M3P3.PresentationReadback"));
			AddEnqueueCopyPass(GraphBuilder, NewState->Readback.Get(), Output);
			GraphBuilder.Execute();

			for (int32 Index = 0; Index < NewState->RepeatedViewSetups; ++Index)
			{
				FRDGBuilder CameraGraphBuilder(
					RHICmdList,
					RDG_EVENT_NAME("SightWeave.Presentation.CameraOnlyViewSetup"));
				NewState->RenderState->PreparePresentationResources_RenderThread(CameraGraphBuilder);
				CameraGraphBuilder.Execute();
			}
			Failure.FinalPageTableUploadCount =
				NewState->RenderState->GetPageTableUploadCount_RenderThread();
			NewState->Result = MoveTemp(Failure);
		});
	return Request;
}

void FSightWeavePresentationTestReadback::Poll()
{
	check(IsInGameThread());
	if (State->bFinished.Load() || State->bPollQueued.Exchange(true))
	{
		return;
	}
	const TSharedRef<FState, ESPMode::ThreadSafe> PollState = State;
	ENQUEUE_RENDER_COMMAND(SightWeavePollPresentationReadback)(
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

			FSightWeavePresentationReadbackResult Result = MoveTemp(PollState->Result);
			Result.Status = ESightWeavePresentationReadbackStatus::Complete;
			if (PollState->bGPUTimestampsIssued)
			{
				uint64 StartMicroseconds = 0;
				uint64 EndMicroseconds = 0;
				if (RHIGetRenderQueryResult(
						PollState->GPUStartQuery.GetReference(),
						StartMicroseconds,
						false)
					&& RHIGetRenderQueryResult(
						PollState->GPUEndQuery.GetReference(),
						EndMicroseconds,
						false)
					&& EndMicroseconds >= StartMicroseconds)
				{
					Result.bGPUTimestampAvailable = true;
					Result.GPUCompositeMicroseconds =
						static_cast<double>(EndMicroseconds - StartMicroseconds);
				}
			}

			int32 RowPitchInPixels = 0;
			int32 BufferHeight = 0;
			void* Source = PollState->Readback->Lock(RowPitchInPixels, &BufferHeight);
			if (!Source
				|| RowPitchInPixels < PollState->Positions.Num()
				|| BufferHeight < 1)
			{
				Result.Status = ESightWeavePresentationReadbackStatus::Failed;
				Result.Failure = TEXT("Presentation readback returned invalid dimensions");
			}
			else
			{
				Result.Pixels.SetNumUninitialized(PollState->Positions.Num());
				FMemory::Memcpy(
					Result.Pixels.GetData(),
					Source,
					Result.Pixels.Num() * sizeof(FColor));
			}
			PollState->Readback->Unlock();
			PollState->Readback.Reset();
			PollState->RenderState->Release_RenderThread(
				PollState->Packet->GetWorldIdentity());
			PollState->RenderState.Reset();
			Result.ReadbackEndToEndMicroseconds =
				(FPlatformTime::Seconds() - PollState->StartSeconds) * 1000000.0;
			PollState->bPollQueued.Store(false);
			PollState->Finish(MoveTemp(Result));
		});
}

bool FSightWeavePresentationTestReadback::TryTakeResult(
	FSightWeavePresentationReadbackResult& OutResult)
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

bool FSightWeavePresentationTestReadback::IsFinished() const
{
	return State->bFinished.Load();
}

#endif
