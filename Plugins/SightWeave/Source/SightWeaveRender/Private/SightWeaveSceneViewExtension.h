#pragma once

#include "SceneViewExtension.h"
#include "SightWeaveRenderPacket.h"

class FSightWeaveSingleTileRenderState;

class FSightWeaveSceneViewExtension final : public FWorldSceneViewExtension
{
public:
	FSightWeaveSceneViewExtension(
		const FAutoRegister& AutoRegister,
		UWorld* World,
		FSightWeaveRenderWorldIdentity WorldIdentity);

	void SubmitPacket(TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe> Packet);
	void Shutdown(FSightWeaveRenderWorldIdentity ExpectedWorldIdentity);

	virtual void PreRenderViewFamily_RenderThread(
		FRDGBuilder& GraphBuilder,
		FSceneViewFamily& ViewFamily) override;

private:
	FSightWeaveRenderWorldIdentity WorldIdentity;
	TSharedRef<FSightWeaveSingleTileRenderState, ESPMode::ThreadSafe> RenderState;
	bool bShutdown = false;
};
