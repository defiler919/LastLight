#include "SightWeaveSceneViewExtension.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "RenderingThread.h"
#include "ScreenPass.h"
#include "SightWeaveSparseAtlasRenderState.h"

namespace
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	TAutoConsoleVariable<int32> CVarSightWeaveDiagnosticCompositePass(
		TEXT("r.SightWeave.Diagnostic.CompositePass"),
		0,
		TEXT("DARKWELL temporal-space A/B: 0 after Tonemap (rejected control), "
			"1 BeforeDOF/pre-TSR with fixed neutral Remembered proof input. "
			"Development/Editor and L_VisionIntegration only."),
		ECVF_RenderThreadSafe);
#endif
}

FSightWeaveSceneViewExtension::FSightWeaveSceneViewExtension(
	const FAutoRegister& AutoRegister,
	UWorld* World,
	const FSightWeaveRenderWorldIdentity InWorldIdentity)
	: FWorldSceneViewExtension(AutoRegister, World)
	, WorldIdentity(InWorldIdentity)
	, RenderState(MakeShared<FSightWeaveSparseAtlasRenderState, ESPMode::ThreadSafe>(InWorldIdentity))
	, bAllowsPreTemporalUpscaleProof(
		World != nullptr && World->GetMapName().EndsWith(TEXT("L_VisionIntegration")))
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
	EPostProcessingPass SelectedPass = EPostProcessingPass::Tonemap;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bAllowsPreTemporalUpscaleProof
		&& CVarSightWeaveDiagnosticCompositePass.GetValueOnRenderThread() == 1)
	{
		SelectedPass = EPostProcessingPass::BeforeDOF;
	}
#endif
	if (PassId == SelectedPass
		&& RenderState->IsPresentationEnabled_RenderThread())
	{
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(
			this,
			&FSightWeaveSceneViewExtension::PostProcessComposite_RenderThread));
	}
}

FScreenPassTexture FSightWeaveSceneViewExtension::PostProcessComposite_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessMaterialInputs& Inputs)
{
	bool bPreTemporalUpscaleProof = false;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bPreTemporalUpscaleProof = bAllowsPreTemporalUpscaleProof
		&& CVarSightWeaveDiagnosticCompositePass.GetValueOnRenderThread() == 1;
#endif
	return RenderState->AddHardMaskComposite_RenderThread(
		GraphBuilder,
		View,
		Inputs,
		bPreTemporalUpscaleProof);
}
