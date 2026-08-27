#include "SightWeaveSceneViewExtension.h"

#include "RenderingThread.h"
#include "ScreenPass.h"
#include "SightWeaveSparseAtlasRenderState.h"

FSightWeaveSceneViewExtension::FSightWeaveSceneViewExtension(
	const FAutoRegister& AutoRegister,
	UWorld* World,
	const FSightWeaveRenderWorldIdentity InWorldIdentity)
	: FWorldSceneViewExtension(AutoRegister, World)
	, WorldIdentity(InWorldIdentity)
	, RenderState(MakeShared<FSightWeaveSparseAtlasRenderState, ESPMode::ThreadSafe>(InWorldIdentity))
{
}

void FSightWeaveSceneViewExtension::SubmitPacket(
	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet)
{
	check(IsInGameThread());
	if (bShutdown || !Packet.IsValid() || Packet->GetWorldIdentity() != WorldIdentity)
	{
		return;
	}
	const TSharedRef<FSightWeaveSparseAtlasRenderState, ESPMode::ThreadSafe> State = RenderState;
	ENQUEUE_RENDER_COMMAND(SightWeaveSubmitPacket)(
		[State, Packet = MoveTemp(Packet)](FRHICommandListImmediate& RHICmdList)
		{
			State->SubmitPacket_RenderThread(Packet);
		});
}

void FSightWeaveSceneViewExtension::SubmitMemoryPacket(
	TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> Packet)
{
	check(IsInGameThread());
	if (bShutdown)
	{
		return;
	}
	const TSharedRef<FSightWeaveSparseAtlasRenderState, ESPMode::ThreadSafe> State = RenderState;
	ENQUEUE_RENDER_COMMAND(SightWeaveSubmitMemoryPacket)(
		[State, Packet = MoveTemp(Packet)](FRHICommandListImmediate& RHICmdList)
		{
			State->SubmitMemoryPacket_RenderThread(Packet);
		});
}

void FSightWeaveSceneViewExtension::SubmitStaticEnvironmentPacket(
	TSharedPtr<const FSightWeaveStaticEnvironmentPacket, ESPMode::ThreadSafe> Packet)
{
	check(IsInGameThread());
	if (bShutdown)
	{
		return;
	}
	const TSharedRef<FSightWeaveSparseAtlasRenderState, ESPMode::ThreadSafe> State = RenderState;
	ENQUEUE_RENDER_COMMAND(SightWeaveSubmitStaticEnvironmentPacket)(
		[State, Packet = MoveTemp(Packet)](FRHICommandListImmediate& RHICmdList)
		{
			State->SubmitStaticEnvironmentPacket_RenderThread(Packet);
		});
}

void FSightWeaveSceneViewExtension::SubmitPresentationSelection(
	const FSightWeaveViewPresentationSelection& Selection)
{
	check(IsInGameThread());
	if (bShutdown || !Selection.IsValid() || Selection.GetWorldIdentity() != WorldIdentity)
	{
		return;
	}
	const TSharedRef<FSightWeaveSparseAtlasRenderState, ESPMode::ThreadSafe> State = RenderState;
	ENQUEUE_RENDER_COMMAND(SightWeaveSubmitPresentationSelection)(
		[State, Selection](FRHICommandListImmediate& RHICmdList)
		{
			State->SubmitPresentationSelection_RenderThread(Selection);
		});
}

void FSightWeaveSceneViewExtension::Shutdown(
	const FSightWeaveRenderWorldIdentity ExpectedWorldIdentity)
{
	check(IsInGameThread());
	if (bShutdown || ExpectedWorldIdentity != WorldIdentity)
	{
		return;
	}
	bShutdown = true;
	const TSharedRef<FSightWeaveSparseAtlasRenderState, ESPMode::ThreadSafe> State = RenderState;
	ENQUEUE_RENDER_COMMAND(SightWeaveReleaseWorld)(
		[State, ExpectedWorldIdentity](FRHICommandListImmediate& RHICmdList)
		{
			State->Release_RenderThread(ExpectedWorldIdentity);
		});
}

void FSightWeaveSceneViewExtension::PreRenderViewFamily_RenderThread(
	FRDGBuilder& GraphBuilder,
	FSceneViewFamily& ViewFamily)
{
	RenderState->ProcessPending_RenderThread(GraphBuilder);
	RenderState->ProcessMemoryPending_RenderThread(GraphBuilder);
	RenderState->ProcessStaticEnvironmentPending_RenderThread(GraphBuilder);
	RenderState->PreparePresentationResources_RenderThread(GraphBuilder);
	RenderState->PrepareMemoryPresentationResources_RenderThread(GraphBuilder);
	RenderState->PrepareStaticEnvironmentPresentationResources_RenderThread(GraphBuilder);
	RenderState->ProcessVisualFeather_RenderThread(GraphBuilder);
}

void FSightWeaveSceneViewExtension::SubscribeToPostProcessingPass(
	const EPostProcessingPass PassId,
	const FSceneView& View,
	FAfterPassCallbackDelegateArray& InOutPassCallbacks,
	const bool bIsPassEnabled)
{
	if (PassId == EPostProcessingPass::Tonemap
		&& RenderState->IsPresentationEnabled_RenderThread())
	{
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(
			this,
			&FSightWeaveSceneViewExtension::PostProcessPassAfterTonemap_RenderThread));
	}
}

FScreenPassTexture FSightWeaveSceneViewExtension::PostProcessPassAfterTonemap_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessMaterialInputs& Inputs)
{
	return RenderState->AddHardMaskComposite_RenderThread(GraphBuilder, View, Inputs);
}
