#pragma once

#include "SceneViewExtension.h"
#include "SightWeaveSparseAtlas.h"

class FSightWeaveSparseAtlasRenderState;

class FSightWeaveSceneViewExtension final : public FWorldSceneViewExtension
{
public:
	FSightWeaveSceneViewExtension(
		const FAutoRegister& AutoRegister,
		UWorld* World,
		FSightWeaveRenderWorldIdentity WorldIdentity);

	void SubmitPacket(TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet);
	void Shutdown(FSightWeaveRenderWorldIdentity ExpectedWorldIdentity);

	virtual void PreRenderViewFamily_RenderThread(
		FRDGBuilder& GraphBuilder,
		FSceneViewFamily& ViewFamily) override;

private:
	FSightWeaveRenderWorldIdentity WorldIdentity;
	TSharedRef<FSightWeaveSparseAtlasRenderState, ESPMode::ThreadSafe> RenderState;
	bool bShutdown = false;
};
