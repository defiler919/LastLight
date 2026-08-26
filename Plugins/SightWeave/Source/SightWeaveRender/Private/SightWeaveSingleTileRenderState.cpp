#include "SightWeaveSingleTileRenderState.h"

#include "CommonRenderResources.h"
#include "GlobalShader.h"
#include "Misc/App.h"
#include "PipelineStateCache.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHI.h"
#include "RHIGlobals.h"
#include "RHIStaticStates.h"
#include "SightWeaveTileShaders.h"

namespace
{
	BEGIN_SHADER_PARAMETER_STRUCT(FSightWeaveRasterPassParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveTileVertexShader::FParameters, VertexShader)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FSightWeaveCombinePassParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveFullscreenVertexShader::FParameters, VertexShader)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveCombinePixelShader::FParameters, PixelShader)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	FRDGTextureDesc MakeMaskTextureDesc()
	{
		return FRDGTextureDesc::Create2D(
			FIntPoint(
				SightWeave::RenderPacket::PhysicalTileSize,
				SightWeave::RenderPacket::PhysicalTileSize),
			PF_G8,
			FClearValueBinding::Black,
			TexCreate_RenderTargetable | TexCreate_ShaderResource);
	}

	void AddRasterPass(
		FRDGBuilder& GraphBuilder,
		const TCHAR* EventName,
		FRDGTextureRef Target,
		FRDGBufferSRVRef Vertices,
		FRDGBufferSRVRef Indices,
		const FSightWeaveRenderTriangleRange& Range)
	{
		if (Range.IsEmpty())
		{
			return;
		}
		TShaderMapRef<FSightWeaveTileVertexShader> VertexShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		TShaderMapRef<FSightWeaveTilePixelShader> PixelShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FSightWeaveRasterPassParameters* PassParameters =
			GraphBuilder.AllocParameters<FSightWeaveRasterPassParameters>();
		PassParameters->VertexShader.TriangleVertices = Vertices;
		PassParameters->VertexShader.TriangleIndices = Indices;
		PassParameters->VertexShader.FirstIndex = Range.FirstIndex;
		PassParameters->VertexShader.InvPhysicalWorldSpan =
			1.0f / (SightWeave::RenderPacket::PhysicalTileSize
				* SightWeave::RenderPacket::StandardCentimetersPerTexel);
		PassParameters->RenderTargets[0] = FRenderTargetBinding(Target, ERenderTargetLoadAction::ELoad);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("%s", EventName),
			PassParameters,
			ERDGPassFlags::Raster,
			[PassParameters, VertexShader, PixelShader, IndexCount = Range.IndexCount](
				FRDGAsyncTask,
				FRHICommandList& RHICmdList)
			{
				RHICmdList.SetViewport(
					0.0f,
					0.0f,
					0.0f,
					SightWeave::RenderPacket::PhysicalTileSize,
					SightWeave::RenderPacket::PhysicalTileSize,
					1.0f);
				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RED>::GetRHI();
				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;
				GraphicsPSOInit.ConservativeRasterization = EConservativeRasterization::Overestimated;
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI =
					GEmptyVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
				SetShaderParameters(
					RHICmdList,
					VertexShader,
					VertexShader.GetVertexShader(),
					PassParameters->VertexShader);
				RHICmdList.SetStreamSource(0, nullptr, 0);
				RHICmdList.DrawPrimitive(0, IndexCount / 3, 1);
			});
	}
}

FSightWeaveSingleTileRenderState::FSightWeaveSingleTileRenderState(
	const FSightWeaveRenderWorldIdentity InWorldIdentity)
	: WorldIdentity(InWorldIdentity)
{
}

void FSightWeaveSingleTileRenderState::SubmitPacket_RenderThread(
	const TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe>& Packet)
{
	check(IsInRenderingThread());
	if (bReleased || !Packet.IsValid() || Packet->GetWorldIdentity() != WorldIdentity)
	{
		++RejectedPacketCount;
		return;
	}
	if (Packet->GetPacketRevision() < DesiredRevision)
	{
		++StalePacketCount;
		return;
	}
	if (Packet->GetPacketRevision() == DesiredRevision)
	{
		if (Packet->GetContentHash() == DesiredHash)
		{
			++DuplicatePacketCount;
		}
		else
		{
			++RejectedPacketCount;
			PendingPacket = Packet;
			bPendingForceBlack = true;
			DesiredHash = 0;
			Availability = ESightWeaveRenderAvailability::InvalidPacket;
		}
		return;
	}
	DesiredRevision = Packet->GetPacketRevision();
	DesiredHash = Packet->GetContentHash();
	PendingPacket = Packet;
	bPendingForceBlack = false;
}

void FSightWeaveSingleTileRenderState::ProcessPending_RenderThread(FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());
	if (bReleased || !PendingPacket.IsValid())
	{
		return;
	}
	const TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe> Packet = MoveTemp(PendingPacket);
	const bool bForceBlack = bPendingForceBlack;
	bPendingForceBlack = false;
	if (bForceBlack
		|| !Packet->IsValid()
		|| FSightWeaveRenderPacketBuilder::Validate(*Packet) != ESightWeaveRenderPacketFailure::None)
	{
		Availability = ESightWeaveRenderAvailability::InvalidPacket;
		if (CheckCapabilities_RenderThread() && EnsurePersistentTexture_RenderThread())
		{
			AddBlackClearPass_RenderThread(GraphBuilder);
			AppliedRevision = Packet->GetPacketRevision();
		}
		else
		{
			EffectiveLiveTexture.SafeRelease();
			AppliedRevision = 0;
		}
		return;
	}
	if (!CheckCapabilities_RenderThread() || !EnsurePersistentTexture_RenderThread())
	{
		EffectiveLiveTexture.SafeRelease();
		AppliedRevision = 0;
		return;
	}
	AddRasterPasses_RenderThread(GraphBuilder, *Packet);
	AppliedRevision = Packet->GetPacketRevision();
	Availability = ESightWeaveRenderAvailability::Available;
}

void FSightWeaveSingleTileRenderState::Release_RenderThread(
	const FSightWeaveRenderWorldIdentity ExpectedWorldIdentity)
{
	check(IsInRenderingThread());
	if (ExpectedWorldIdentity != WorldIdentity)
	{
		return;
	}
	bReleased = true;
	PendingPacket.Reset();
	bPendingForceBlack = false;
	EffectiveLiveTexture.SafeRelease();
	DesiredRevision = 0;
	DesiredHash = 0;
	AppliedRevision = 0;
	++ResourceGeneration;
	Availability = ESightWeaveRenderAvailability::WorldTeardown;
}

bool FSightWeaveSingleTileRenderState::CheckCapabilities_RenderThread()
{
	check(IsInRenderingThread());
	if (GUsingNullRHI || !FApp::CanEverRender())
	{
		Availability = ESightWeaveRenderAvailability::NullRHI;
		return false;
	}
	if (GMaxRHIShaderPlatform != SP_PCD3D_SM6)
	{
		Availability = ESightWeaveRenderAvailability::UnsupportedRHI;
		return false;
	}
	constexpr EPixelFormatCapabilities RequiredCapabilities =
		EPixelFormatCapabilities::Texture2D
		| EPixelFormatCapabilities::RenderTarget
		| EPixelFormatCapabilities::TextureLoad
		| EPixelFormatCapabilities::TextureSample;
	if (!GPixelFormats[PF_G8].Supported
		|| !RHIPixelFormatHasCapabilities(PF_G8, RequiredCapabilities))
	{
		Availability = ESightWeaveRenderAvailability::UnsupportedPixelFormat;
		return false;
	}
	if (!GRHISupportsConservativeRasterization)
	{
		Availability = ESightWeaveRenderAvailability::ConservativeRasterUnavailable;
		return false;
	}
	return true;
}

bool FSightWeaveSingleTileRenderState::EnsurePersistentTexture_RenderThread()
{
	check(IsInRenderingThread());
	if (EffectiveLiveTexture.IsValid())
	{
		return true;
	}
	if (!AllocatePooledTexture(
		MakeMaskTextureDesc(),
		EffectiveLiveTexture,
		TEXT("SightWeave.EffectiveLive.SingleTile")))
	{
		Availability = ESightWeaveRenderAvailability::ResourceAllocationFailed;
		return false;
	}
	++ResourceGeneration;
	return true;
}

void FSightWeaveSingleTileRenderState::AddBlackClearPass_RenderThread(FRDGBuilder& GraphBuilder)
{
	FRDGTextureRef EffectiveLive = GraphBuilder.RegisterExternalTexture(
		EffectiveLiveTexture,
		TEXT("SightWeave.EffectiveLive.SingleTile"));
	RDG_EVENT_SCOPE(GraphBuilder, "SightWeave.ClearTile");
	AddClearRenderTargetPass(GraphBuilder, EffectiveLive, FLinearColor::Black);
}

void FSightWeaveSingleTileRenderState::AddRasterPasses_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FSightWeaveRenderPacket& Packet)
{
	RDG_EVENT_SCOPE(GraphBuilder, "SightWeave.SingleTile.Revision_%llu", Packet.GetPacketRevision());
	const FRDGTextureDesc ScratchDesc = MakeMaskTextureDesc();
	FRDGTextureRef Vision = GraphBuilder.CreateTexture(ScratchDesc, TEXT("SightWeave.VisionScratch"));
	FRDGTextureRef Illumination = GraphBuilder.CreateTexture(ScratchDesc, TEXT("SightWeave.IlluminationScratch"));
	FRDGTextureRef Bypass = GraphBuilder.CreateTexture(ScratchDesc, TEXT("SightWeave.BypassScratch"));
	FRDGTextureRef Suppression = GraphBuilder.CreateTexture(ScratchDesc, TEXT("SightWeave.SuppressionScratch"));
	FRDGTextureRef EffectiveLive = GraphBuilder.RegisterExternalTexture(
		EffectiveLiveTexture,
		TEXT("SightWeave.EffectiveLive.SingleTile"));
	{
		RDG_EVENT_SCOPE(GraphBuilder, "SightWeave.ClearTile");
		AddClearRenderTargetPass(GraphBuilder, Vision, FLinearColor::Black);
		AddClearRenderTargetPass(GraphBuilder, Illumination, FLinearColor::Black);
		AddClearRenderTargetPass(GraphBuilder, Bypass, FLinearColor::Black);
		AddClearRenderTargetPass(GraphBuilder, Suppression, FLinearColor::Black);
	}

	FRDGBufferRef VertexBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("SightWeave.PacketVertices"),
		Packet.GetVertices());
	FRDGBufferRef IndexBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("SightWeave.PacketIndices"),
		Packet.GetIndices());
	FRDGBufferSRVRef VertexSRV = GraphBuilder.CreateSRV(VertexBuffer);
	FRDGBufferSRVRef IndexSRV = GraphBuilder.CreateSRV(IndexBuffer);
	AddRasterPass(GraphBuilder, TEXT("SightWeave.RasterVision"), Vision, VertexSRV, IndexSRV,
		Packet.GetRange(ESightWeaveRenderMaskLayer::Vision));
	AddRasterPass(GraphBuilder, TEXT("SightWeave.RasterIllumination"), Illumination, VertexSRV, IndexSRV,
		Packet.GetRange(ESightWeaveRenderMaskLayer::Illumination));
	AddRasterPass(GraphBuilder, TEXT("SightWeave.RasterBypass"), Bypass, VertexSRV, IndexSRV,
		Packet.GetRange(ESightWeaveRenderMaskLayer::Bypass));
	AddRasterPass(GraphBuilder, TEXT("SightWeave.RasterSuppression"), Suppression, VertexSRV, IndexSRV,
		Packet.GetRange(ESightWeaveRenderMaskLayer::Suppression));

	TShaderMapRef<FSightWeaveFullscreenVertexShader> VertexShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	TShaderMapRef<FSightWeaveCombinePixelShader> PixelShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FSightWeaveCombinePassParameters* PassParameters =
		GraphBuilder.AllocParameters<FSightWeaveCombinePassParameters>();
	PassParameters->PixelShader.VisionTexture = Vision;
	PassParameters->PixelShader.IlluminationTexture = Illumination;
	PassParameters->PixelShader.BypassTexture = Bypass;
	PassParameters->PixelShader.SuppressionTexture = Suppression;
	PassParameters->RenderTargets[0] = FRenderTargetBinding(EffectiveLive, ERenderTargetLoadAction::EClear);
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("SightWeave.CombineEffectiveLive"),
		PassParameters,
		ERDGPassFlags::Raster,
		[PassParameters, VertexShader, PixelShader](FRDGAsyncTask, FRHICommandList& RHICmdList)
		{
			RHICmdList.SetViewport(
				0.0f,
				0.0f,
				0.0f,
				SightWeave::RenderPacket::PhysicalTileSize,
				SightWeave::RenderPacket::PhysicalTileSize,
				1.0f);
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RED>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI =
				GEmptyVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
			SetShaderParameters(
				RHICmdList,
				VertexShader,
				VertexShader.GetVertexShader(),
				PassParameters->VertexShader);
			SetShaderParameters(
				RHICmdList,
				PixelShader,
				PixelShader.GetPixelShader(),
				PassParameters->PixelShader);
			RHICmdList.SetStreamSource(0, nullptr, 0);
			RHICmdList.DrawPrimitive(0, 1, 1);
		});
	++RasterDispatchCount;
}
