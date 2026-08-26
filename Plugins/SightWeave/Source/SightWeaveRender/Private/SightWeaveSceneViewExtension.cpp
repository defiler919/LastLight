#include "SightWeaveSceneViewExtension.h"

#include "RenderingThread.h"
#include "SightWeaveSingleTileRenderState.h"

FSightWeaveSceneViewExtension::FSightWeaveSceneViewExtension(
	const FAutoRegister& AutoRegister,
	UWorld* World,
	const FSightWeaveRenderWorldIdentity InWorldIdentity)
	: FWorldSceneViewExtension(AutoRegister, World)
	, WorldIdentity(InWorldIdentity)
	, RenderState(MakeShared<FSightWeaveSingleTileRenderState, ESPMode::ThreadSafe>(InWorldIdentity))
{
}

void FSightWeaveSceneViewExtension::SubmitPacket(
	TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe> Packet)
{
	check(IsInGameThread());
	if (bShutdown || !Packet.IsValid() || Packet->GetWorldIdentity() != WorldIdentity)
	{
		return;
	}
	const TSharedRef<FSightWeaveSingleTileRenderState, ESPMode::ThreadSafe> State = RenderState;
	ENQUEUE_RENDER_COMMAND(SightWeaveSubmitPacket)(
		[State, Packet = MoveTemp(Packet)](FRHICommandListImmediate& RHICmdList)
		{
			State->SubmitPacket_RenderThread(Packet);
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
	const TSharedRef<FSightWeaveSingleTileRenderState, ESPMode::ThreadSafe> State = RenderState;
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
}
