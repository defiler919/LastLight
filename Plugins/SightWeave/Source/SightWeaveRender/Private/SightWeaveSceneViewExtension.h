#pragma once

#include "SceneViewExtension.h"
#include "SightWeaveMemory.h"
#include "SightWeavePresentation.h"
#include "SightWeaveSparseAtlas.h"
#include "SightWeaveStaticEnvironment.h"

class FSightWeaveSparseAtlasRenderState;

class FSightWeaveSceneViewExtension final : public FWorldSceneViewExtension
{
public:
	FSightWeaveSceneViewExtension(
		const FAutoRegister& AutoRegister,
		UWorld* World,
		FSightWeaveRenderWorldIdentity WorldIdentity);

	void SubmitPacket(TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet);
	void SubmitMemoryPacket(TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> Packet);
	void SubmitStaticEnvironmentPacket(
		TSharedPtr<const FSightWeaveStaticEnvironmentPacket, ESPMode::ThreadSafe> Packet);
	void SubmitPresentationSelection(const FSightWeaveViewPresentationSelection& Selection);
	void Shutdown(FSightWeaveRenderWorldIdentity ExpectedWorldIdentity);

	virtual void PreRenderViewFamily_RenderThread(
		FRDGBuilder& GraphBuilder,
		FSceneViewFamily& ViewFamily) override;
	virtual void SubscribeToPostProcessingPass(
		EPostProcessingPass PassId,
		const FSceneView& View,
		FAfterPassCallbackDelegateArray& InOutPassCallbacks,
		bool bIsPassEnabled) override;

private:
	FScreenPassTexture PostProcessPassAfterTonemap_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FPostProcessMaterialInputs& Inputs);

	FSightWeaveRenderWorldIdentity WorldIdentity;
	TSharedRef<FSightWeaveSparseAtlasRenderState, ESPMode::ThreadSafe> RenderState;
	bool bShutdown = false;
};
