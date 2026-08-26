#include "SightWeaveSparseAtlasRenderState.h"

#include "Algo/Sort.h"
#include "CommonRenderResources.h"
#include "GlobalShader.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHI.h"
#include "RHIGlobals.h"
#include "RHIStaticStates.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "ScreenPass.h"
#include "SightWeaveTileShaders.h"
#include "SystemTextures.h"

namespace SightWeaveSparseAtlasRenderPrivate
{
	BEGIN_SHADER_PARAMETER_STRUCT(FSightWeaveSparseRasterPassParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveTileVertexShader::FParameters, VertexShader)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveTilePixelShader::FParameters, PixelShader)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FSightWeaveSparseProfileCombinePassParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveFullscreenVertexShader::FParameters, VertexShader)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveAtlasProfileCombinePixelShader::FParameters, PixelShader)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FSightWeaveSparseSuppressionPassParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveFullscreenVertexShader::FParameters, VertexShader)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveAtlasSuppressionPixelShader::FParameters, PixelShader)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FSightWeaveFeatherSeedPassParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveFullscreenVertexShader::FParameters, VertexShader)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveFeatherSeedPixelShader::FParameters, PixelShader)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FSightWeaveFeatherJumpPassParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveFullscreenVertexShader::FParameters, VertexShader)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveFeatherJumpPixelShader::FParameters, PixelShader)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FSightWeaveFeatherFinalizePassParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveFullscreenVertexShader::FParameters, VertexShader)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveFeatherFinalizePixelShader::FParameters, PixelShader)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

#if WITH_DEV_AUTOMATION_TESTS
	BEGIN_SHADER_PARAMETER_STRUCT(FSightWeavePresentationTestPassParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveFullscreenVertexShader::FParameters, VertexShader)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeavePresentationTestPixelShader::FParameters, PixelShader)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FSightWeavePresentationBenchmarkPassParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveFullscreenVertexShader::FParameters, VertexShader)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeavePresentationBenchmarkPixelShader::FParameters, PixelShader)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
#endif

	FRDGTextureDesc MakeMaskTextureDesc(const int32 Size)
	{
		return FRDGTextureDesc::Create2D(
			FIntPoint(Size, Size),
			PF_G8,
			FClearValueBinding::Black,
			TexCreate_RenderTargetable | TexCreate_ShaderResource);
	}

	FRDGTextureDesc MakeFeatherSeedTextureDesc()
	{
		return FRDGTextureDesc::Create2D(
			FIntPoint(
				SightWeave::VisualFeather::TransformWorkSize,
				SightWeave::VisualFeather::TransformWorkSize),
			PF_G32R32F,
			FClearValueBinding::None,
			TexCreate_RenderTargetable | TexCreate_ShaderResource);
	}

	void ConfigureViewport(FRHICommandList& RHICmdList, const FIntRect& Rect)
	{
		RHICmdList.SetViewport(
			static_cast<float>(Rect.Min.X),
			static_cast<float>(Rect.Min.Y),
			0.0f,
			static_cast<float>(Rect.Max.X),
			static_cast<float>(Rect.Max.Y),
			1.0f);
	}

	void AddSparseRasterPass(
		FRDGBuilder& GraphBuilder,
		const TCHAR* EventName,
		FRDGTextureRef Target,
		const FIntRect& Viewport,
		FRDGBufferSRVRef Vertices,
		FRDGBufferSRVRef Indices,
		const FSightWeaveRenderTriangleRange& Range,
		const float CentimetersPerTexel,
		const bool bMaxBlend)
	{
		if (Range.IsEmpty())
		{
			return;
		}
		TShaderMapRef<FSightWeaveTileVertexShader> VertexShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		TShaderMapRef<FSightWeaveTilePixelShader> PixelShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FSightWeaveSparseRasterPassParameters* PassParameters =
			GraphBuilder.AllocParameters<FSightWeaveSparseRasterPassParameters>();
		PassParameters->VertexShader.TriangleVertices = Vertices;
		PassParameters->VertexShader.TriangleIndices = Indices;
		PassParameters->VertexShader.FirstIndex = Range.FirstIndex;
		PassParameters->VertexShader.InvPhysicalWorldSpan =
			1.0f / (SightWeave::SparseAtlas::PhysicalTileSize * CentimetersPerTexel);
		PassParameters->PixelShader.RasterCentimetersPerTexel = CentimetersPerTexel;
		PassParameters->PixelShader.RasterTargetOriginX = static_cast<uint32>(Viewport.Min.X);
		PassParameters->PixelShader.RasterTargetOriginY = static_cast<uint32>(Viewport.Min.Y);
		PassParameters->RenderTargets[0] = FRenderTargetBinding(Target, ERenderTargetLoadAction::ELoad);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("%s", EventName),
			PassParameters,
			ERDGPassFlags::Raster,
			[PassParameters, VertexShader, PixelShader, Viewport, bMaxBlend,
				IndexCount = Range.IndexCount](FRDGAsyncTask, FRHICommandList& RHICmdList)
			{
				ConfigureViewport(RHICmdList, Viewport);
				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.BlendState = bMaxBlend
					? TStaticBlendState<CW_RED, BO_Max, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI()
					: TStaticBlendState<CW_RED>::GetRHI();
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
				RHICmdList.DrawPrimitive(0, (IndexCount / 3) * 2, 1);
			});
	}

	void AddProfileCombinePass(
		FRDGBuilder& GraphBuilder,
		FRDGTextureRef Target,
		const FIntRect& Viewport,
		FRDGTextureRef Vision,
		FRDGTextureRef Illumination)
	{
		TShaderMapRef<FSightWeaveFullscreenVertexShader> VertexShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		TShaderMapRef<FSightWeaveAtlasProfileCombinePixelShader> PixelShader(
			GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FSightWeaveSparseProfileCombinePassParameters* PassParameters =
			GraphBuilder.AllocParameters<FSightWeaveSparseProfileCombinePassParameters>();
		PassParameters->PixelShader.VisionTexture = Vision;
		PassParameters->PixelShader.IlluminationTexture = Illumination;
		PassParameters->PixelShader.DestinationOriginX = static_cast<uint32>(Viewport.Min.X);
		PassParameters->PixelShader.DestinationOriginY = static_cast<uint32>(Viewport.Min.Y);
		PassParameters->RenderTargets[0] = FRenderTargetBinding(Target, ERenderTargetLoadAction::ELoad);
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("SightWeave.Sparse.ProfileUnion"),
			PassParameters,
			ERDGPassFlags::Raster,
			[PassParameters, VertexShader, PixelShader, Viewport](FRDGAsyncTask, FRHICommandList& RHICmdList)
			{
				ConfigureViewport(RHICmdList, Viewport);
				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.BlendState =
					TStaticBlendState<CW_RED, BO_Max, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
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
	}

	void AddSuppressionPass(
		FRDGBuilder& GraphBuilder,
		FRDGTextureRef Target,
		const FIntRect& Viewport,
		FRDGTextureRef Suppression)
	{
		TShaderMapRef<FSightWeaveFullscreenVertexShader> VertexShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		TShaderMapRef<FSightWeaveAtlasSuppressionPixelShader> PixelShader(
			GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FSightWeaveSparseSuppressionPassParameters* PassParameters =
			GraphBuilder.AllocParameters<FSightWeaveSparseSuppressionPassParameters>();
		PassParameters->PixelShader.SuppressionTexture = Suppression;
		PassParameters->PixelShader.DestinationOriginX = static_cast<uint32>(Viewport.Min.X);
		PassParameters->PixelShader.DestinationOriginY = static_cast<uint32>(Viewport.Min.Y);
		PassParameters->RenderTargets[0] = FRenderTargetBinding(Target, ERenderTargetLoadAction::ELoad);
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("SightWeave.Sparse.SuppressionLast"),
			PassParameters,
			ERDGPassFlags::Raster,
			[PassParameters, VertexShader, PixelShader, Viewport](FRDGAsyncTask, FRHICommandList& RHICmdList)
			{
				ConfigureViewport(RHICmdList, Viewport);
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
	}

	bool PacketContainsScope(
		const FSightWeaveSparseRenderPacket& Packet,
		const FSightWeaveSparseScopeKey& ScopeKey)
	{
		return Packet.GetScopes().ContainsByPredicate([&ScopeKey](const FSightWeaveSparseRenderScope& Scope)
		{
			return Scope.ScopeKey.IsEquivalentTo(ScopeKey);
		});
	}

	bool PacketContainsTileInScope(
		const FSightWeaveSparseRenderPacket& Packet,
		const FSightWeaveSparseTileIdentity& Identity)
	{
		return Packet.GetTiles().ContainsByPredicate([&Identity](const FSightWeaveSparseRenderTile& Tile)
		{
			return Tile.Identity.IsEquivalentTo(Identity);
		});
	}
}

using namespace SightWeaveSparseAtlasRenderPrivate;

struct FSightWeaveSparseAtlasRenderState::FScopeState final
{
	FScopeState(const FSightWeaveSparseScopeKey& InScopeKey, const int32 InCapacity)
		: ScopeKey(InScopeKey)
		, Capacity(InCapacity)
		, Residency(InCapacity)
	{
	}

	FSightWeaveSparseScopeKey ScopeKey;
	int32 Capacity = 0;
	FSightWeaveSparseAtlasResidency Residency;
	TArray<TRefCountPtr<IPooledRenderTarget>> Pages;
	TArray<TRefCountPtr<IPooledRenderTarget>> FeatherPages;
	TRefCountPtr<FRDGPooledBuffer> PageTable;
	FRDGBufferRef CurrentPageTable = nullptr;
	uint64 PageTableResidencyGeneration = 0;
	uint64 PageTablePacketRevision = 0;
	int32 PageTableEntryCount = 0;
	uint64 DesiredRevision = 0;
	uint64 AppliedRevision = 0;
	uint64 FeatherAppliedRevision = 0;
	uint64 FeatherSettingsRevision = 0;
	uint64 FeatherResourceGeneration = 0;
	ESightWeaveRenderAvailability Availability = ESightWeaveRenderAvailability::Unknown;
};

FSightWeaveSparseAtlasRenderState::FSightWeaveSparseAtlasRenderState(
	const FSightWeaveRenderWorldIdentity InWorldIdentity)
	: WorldIdentity(InWorldIdentity)
{
}

FSightWeaveSparseAtlasRenderState::~FSightWeaveSparseAtlasRenderState() = default;

void FSightWeaveSparseAtlasRenderState::SubmitPacket_RenderThread(
	const TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe>& Packet)
{
	check(IsInRenderingThread());
#if WITH_DEV_AUTOMATION_TESTS
	const double StartSeconds = FPlatformTime::Seconds();
	LastTimings = FSightWeaveSparseRenderTimings();
	auto FinishConsumeTiming = [this, StartSeconds]()
	{
		LastTimings.PacketConsumeMicroseconds =
			(FPlatformTime::Seconds() - StartSeconds) * 1000000.0;
	};
#endif
	if (bReleased || !Packet.IsValid() || Packet->GetWorldIdentity() != WorldIdentity)
	{
		++RejectedPacketCount;
#if WITH_DEV_AUTOMATION_TESTS
		FinishConsumeTiming();
#endif
		return;
	}
	if (Packet->GetPacketRevision() < DesiredRevision)
	{
		++StalePacketCount;
#if WITH_DEV_AUTOMATION_TESTS
		FinishConsumeTiming();
#endif
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
#if WITH_DEV_AUTOMATION_TESTS
		FinishConsumeTiming();
#endif
		return;
	}
	DesiredRevision = Packet->GetPacketRevision();
	DesiredHash = Packet->GetContentHash();
	PendingPacket = Packet;
	bPendingForceBlack = false;
#if WITH_DEV_AUTOMATION_TESTS
	FinishConsumeTiming();
#endif
}

void FSightWeaveSparseAtlasRenderState::SubmitPresentationSelection_RenderThread(
	const FSightWeaveViewPresentationSelection& Selection)
{
	check(IsInRenderingThread());
	if (bReleased || !Selection.IsValid() || Selection.GetWorldIdentity() != WorldIdentity)
	{
		PresentationSelection = FSightWeaveViewPresentationSelection();
		PresentationBinding.Reset();
		PresentationBindingFailure = bReleased
			? ESightWeavePresentationBindingFailure::WorldTeardown
			: ESightWeavePresentationBindingFailure::InvalidSelection;
		return;
	}
	const bool bSelectionChanged = !PresentationSelection.IsEquivalentTo(Selection);
	PresentationSelection = Selection;
	if (bSelectionChanged)
	{
		if (!Selection.IsEnabled() || !Selection.GetVisualFeather().IsEnabled())
		{
			ReleaseFeatherResources_RenderThread();
			bFeatherFullRebuildPending = false;
			bFeatherUpdateIncomplete = false;
		}
		else
		{
			ReleaseFeatherResources_RenderThread();
			bFeatherFullRebuildPending = true;
			bFeatherUpdateIncomplete = true;
		}
	}
	RefreshPresentationBinding_RenderThread();
}

bool FSightWeaveSparseAtlasRenderState::ProcessPending_RenderThread(FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());
	if (bReleased || !PendingPacket.IsValid())
	{
		return false;
	}
#if WITH_DEV_AUTOMATION_TESTS
	const double RDGStartSeconds = FPlatformTime::Seconds();
	const double SchedulingStartSeconds = RDGStartSeconds;
	const double PriorConsume = LastTimings.PacketConsumeMicroseconds;
	LastTimings = FSightWeaveSparseRenderTimings();
	LastTimings.PacketConsumeMicroseconds = PriorConsume;
#endif
	const TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet = MoveTemp(PendingPacket);
	const bool bForceBlack = bPendingForceBlack;
	bPendingForceBlack = false;
	if (bForceBlack
		|| !Packet->IsValid()
		|| FSightWeaveSparseRenderPacketBuilder::Validate(*Packet) != ESightWeaveSparsePacketFailure::None)
	{
		FailAllScopes_RenderThread(ESightWeaveRenderAvailability::InvalidPacket);
		AppliedPacket.Reset();
		PresentationBinding.Reset();
		PresentationBindingFailure = ESightWeavePresentationBindingFailure::InvalidPacket;
		AppliedRevision = Packet->GetPacketRevision();
		return false;
	}
	if (!CheckCapabilities_RenderThread())
	{
		FailAllScopes_RenderThread(Availability);
		AppliedPacket.Reset();
		PresentationBinding.Reset();
		PresentationBindingFailure = ESightWeavePresentationBindingFailure::ScopeUnavailable;
		AppliedRevision = 0;
		return false;
	}

	RemoveAbsentScopes_RenderThread(*Packet);
	bool bAnyFeatherImpact = false;
	for (const FSightWeaveSparseRenderScope& ScopePacket : Packet->GetScopes())
	{
		if (!ScopePacket.IsValid())
		{
			if (FScopeState* Existing = FindScope_RenderThread(ScopePacket.ScopeKey))
			{
				FailScope_RenderThread(*Existing, ESightWeaveRenderAvailability::InvalidPacket);
				Existing->DesiredRevision = Packet->GetPacketRevision();
			}
			continue;
		}
		FScopeState& Scope = FindOrAddScope_RenderThread(ScopePacket);
		Scope.DesiredRevision = Packet->GetPacketRevision();
	}

	for (TUniquePtr<FScopeState>& Scope : Scopes)
	{
		TArray<FSightWeaveSparseTileIdentity> Releases;
		for (const FSightWeaveSparseResidencySlot& Slot : Scope->Residency.GetSlots())
		{
			if (Slot.bOccupied && !PacketContainsTileInScope(*Packet, Slot.Identity))
			{
				Releases.Add(Slot.Identity);
			}
		}
		for (const FSightWeaveSparseTileIdentity& Identity : Releases)
		{
			MarkFeatherDirtyAround_RenderThread(Identity.TileKey);
			bAnyFeatherImpact = true;
			if (!Scope->Residency.Release(Identity))
			{
				FailScope_RenderThread(*Scope, ESightWeaveRenderAvailability::InvalidPacket);
				break;
			}
			++ResidencyGeneration;
		}
	}
#if WITH_DEV_AUTOMATION_TESTS
	LastTimings.DirtySchedulingMicroseconds =
		(FPlatformTime::Seconds() - SchedulingStartSeconds) * 1000000.0;
#endif

	bool bAnyMaskWork = false;
	for (const int32 DirtyTileIndex : Packet->GetDirtyTileIndices())
	{
		const FSightWeaveSparseRenderTile& Tile = Packet->GetTiles()[DirtyTileIndex];
		MarkFeatherDirtyAround_RenderThread(Tile.Identity.TileKey);
		bAnyFeatherImpact = true;
		FScopeState* Scope = FindScope_RenderThread(Tile.Identity.TileKey.Scope);
		if (!Scope
			|| Scope->Availability == ESightWeaveRenderAvailability::InvalidPacket
			|| Scope->Availability == ESightWeaveRenderAvailability::ResourceAllocationFailed)
		{
			continue;
		}
		const FSightWeaveSparseResidencyResult ResidencyResult =
			Scope->Residency.Acquire(Tile.Identity, Packet->GetPacketRevision());
		if (!ResidencyResult.Succeeded())
		{
			FailScope_RenderThread(*Scope, ESightWeaveRenderAvailability::ResourceAllocationFailed);
			continue;
		}
		if (ResidencyResult.Disposition == ESightWeaveSparseResidencyDisposition::Allocated
			|| ResidencyResult.Disposition == ESightWeaveSparseResidencyDisposition::Reused)
		{
			++ResidencyGeneration;
		}
		FRDGTextureRef Page = nullptr;
		bool bColdCreated = false;
		if (!EnsurePage_RenderThread(
				GraphBuilder,
				*Scope,
				ResidencyResult.Address.PageIndex,
				Page,
				bColdCreated)
			|| !EnsureScratchTextures_RenderThread())
		{
			FailScope_RenderThread(*Scope, ESightWeaveRenderAvailability::ResourceAllocationFailed);
			continue;
		}
		if (!Page)
		{
			continue;
		}
		const FIntRect SlotRect = ResidencyResult.Address.GetSlotRect();
#if WITH_DEV_AUTOMATION_TESTS
		const double ClearStartSeconds = FPlatformTime::Seconds();
#endif
		AddClearRenderTargetPass(GraphBuilder, Page, FLinearColor::Black, SlotRect);
#if WITH_DEV_AUTOMATION_TESTS
		LastTimings.TileClearSetupMicroseconds +=
			(FPlatformTime::Seconds() - ClearStartSeconds) * 1000000.0;
		const double RasterStartSeconds = FPlatformTime::Seconds();
#endif
		AddTilePasses_RenderThread(GraphBuilder, Page, SlotRect, Tile);
#if WITH_DEV_AUTOMATION_TESTS
		LastTimings.RasterSetupMicroseconds +=
			(FPlatformTime::Seconds() - RasterStartSeconds) * 1000000.0;
#endif
		Scope->Residency.MarkApplied(ResidencyResult.Address, Packet->GetPacketRevision());
		++DirtyTileDispatchCount;
		bAnyMaskWork = true;
	}

#if WITH_DEV_AUTOMATION_TESTS
	const double PublicationStartSeconds = FPlatformTime::Seconds();
#endif
	for (TUniquePtr<FScopeState>& Scope : Scopes)
	{
		if (Scope->DesiredRevision == Packet->GetPacketRevision()
			&& Scope->Availability != ESightWeaveRenderAvailability::InvalidPacket
			&& Scope->Availability != ESightWeaveRenderAvailability::ResourceAllocationFailed)
		{
			Scope->AppliedRevision = Packet->GetPacketRevision();
			Scope->Availability = ESightWeaveRenderAvailability::Available;
		}
	}
	AppliedRevision = Packet->GetPacketRevision();
	Availability = ESightWeaveRenderAvailability::Available;
	AppliedPacket = Packet;
	if (PresentationSelection.IsEnabled()
		&& PresentationSelection.GetVisualFeather().IsEnabled()
		&& bAnyFeatherImpact)
	{
		bFeatherUpdateIncomplete = true;
	}
	RefreshPresentationBinding_RenderThread();
#if WITH_DEV_AUTOMATION_TESTS
	LastTimings.PublicationMicroseconds =
		(FPlatformTime::Seconds() - PublicationStartSeconds) * 1000000.0;
	LastTimings.RDGSetupMicroseconds =
		(FPlatformTime::Seconds() - RDGStartSeconds) * 1000000.0;
#endif
	return bAnyMaskWork;
}

void FSightWeaveSparseAtlasRenderState::PreparePresentationResources_RenderThread(
	FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());
	for (TUniquePtr<FScopeState>& Scope : Scopes)
	{
		Scope->CurrentPageTable = nullptr;
		if (Scope->Availability == ESightWeaveRenderAvailability::Available)
		{
			PrepareScopePageTable_RenderThread(GraphBuilder, *Scope);
		}
	}
}

bool FSightWeaveSparseAtlasRenderState::PrepareScopePageTable_RenderThread(
	FRDGBuilder& GraphBuilder,
	FScopeState& Scope)
{
	check(IsInRenderingThread());
	if (Scope.PageTable.IsValid()
		&& Scope.PageTableResidencyGeneration == ResidencyGeneration
		&& Scope.PageTablePacketRevision == Scope.AppliedRevision)
	{
		Scope.CurrentPageTable = GraphBuilder.RegisterExternalBuffer(
			Scope.PageTable,
			TEXT("SightWeave.Presentation.PageTable"));
		return Scope.CurrentPageTable != nullptr;
	}

	TArray<FIntVector4> Entries;
	Entries.Reserve(Scope.Residency.GetResidentCount());
	for (const FSightWeaveSparseResidencySlot& Slot : Scope.Residency.GetSlots())
	{
		if (!Slot.bOccupied || Slot.AppliedRevision != Scope.AppliedRevision)
		{
			continue;
		}
		Entries.Emplace(
			Slot.Identity.TileKey.LogicalCoordinate.X,
			Slot.Identity.TileKey.LogicalCoordinate.Y,
			Slot.Address.PageIndex,
			Slot.Address.SlotIndex);
	}
	Algo::Sort(Entries, [](const FIntVector4& A, const FIntVector4& B)
	{
		return A.X < B.X || (A.X == B.X && A.Y < B.Y);
	});
	Scope.PageTableEntryCount = Entries.Num();
	if (Entries.IsEmpty())
	{
		Entries.Emplace(0, 0, INDEX_NONE, INDEX_NONE);
	}

	Scope.CurrentPageTable = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("SightWeave.Presentation.PageTable"),
		MoveTemp(Entries));
	GraphBuilder.QueueBufferExtraction(Scope.CurrentPageTable, &Scope.PageTable);
	Scope.PageTableResidencyGeneration = ResidencyGeneration;
	Scope.PageTablePacketRevision = Scope.AppliedRevision;
	++PageTableUploadCount;
	return true;
}

FScreenPassTexture FSightWeaveSparseAtlasRenderState::AddHardMaskComposite_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessMaterialInputs& Inputs)
{
	check(IsInRenderingThread());
	const FScreenPassTexture SceneColor = FScreenPassTexture::CopyFromSlice(
		GraphBuilder,
		Inputs.GetInput(EPostProcessMaterialInput::SceneColor));
	check(SceneColor.IsValid());

	FScreenPassRenderTarget Output = Inputs.OverrideOutput;
	if (!Output.IsValid())
	{
		Output = FScreenPassRenderTarget::CreateFromInput(
			GraphBuilder,
			SceneColor,
			View.GetOverwriteLoadAction(),
			TEXT("SightWeave.HardMask.Output"));
	}
	auto FailBlack = [&GraphBuilder, &Output]() -> FScreenPassTexture
	{
		AddClearRenderTargetPass(GraphBuilder, Output.Texture, FLinearColor::Black, Output.ViewRect);
		return MoveTemp(Output);
	};

	const TSharedPtr<const FSightWeaveViewPresentationBinding, ESPMode::ThreadSafe> Binding =
		PresentationBinding;
	if (!Binding.IsValid()
		|| !Binding->IsValid()
		|| Binding->GetResourceGeneration() != ResourceGeneration
		|| Binding->GetResidencyGeneration() != ResidencyGeneration
		|| Binding->GetPacketRevision() != AppliedRevision)
	{
		return FailBlack();
	}
	FScopeState* Scope = FindScope_RenderThread(Binding->GetScopeKey());
	if (!Scope
		|| Scope->Availability != ESightWeaveRenderAvailability::Available
		|| Scope->AppliedRevision != Binding->GetPacketRevision()
		|| Scope->PageTableResidencyGeneration != Binding->GetResidencyGeneration()
		|| Scope->PageTablePacketRevision != Binding->GetPacketRevision()
		|| !Scope->CurrentPageTable)
	{
		return FailBlack();
	}
	for (const FSightWeaveSparseResidencySlot& Slot : Scope->Residency.GetSlots())
	{
		if (Slot.bOccupied && Slot.Address.PageIndex >= 4)
		{
			return FailBlack();
		}
	}
	if (!Inputs.SceneTextures.SceneTextures)
	{
		return FailBlack();
	}
	FRDGTextureRef SceneDepth =
		Inputs.SceneTextures.SceneTextures->GetParameters()->SceneDepthTexture;
	if (!SceneDepth)
	{
		return FailBlack();
	}

	FRDGTextureRef DummyPage = GSystemTextures.GetBlackDummy(GraphBuilder);
	FRDGTextureRef AtlasPages[4] = { DummyPage, DummyPage, DummyPage, DummyPage };
	for (int32 PageIndex = 0; PageIndex < Scope->Pages.Num() && PageIndex < 4; ++PageIndex)
	{
		if (Scope->Pages[PageIndex].IsValid())
		{
			AtlasPages[PageIndex] = GraphBuilder.RegisterExternalTexture(
				Scope->Pages[PageIndex],
				TEXT("SightWeave.Presentation.AtlasPage"));
		}
	}
	const FVector PreViewTranslation = View.ViewMatrices.GetPreViewTranslation();
	const FVector2D TranslatedFloorOrigin = Binding->GetScopeKey().FloorOrigin
		+ FVector2D(PreViewTranslation.X, PreViewTranslation.Y);
	if (Binding->GetVisualFeather().IsEnabled())
	{
		if (bFeatherUpdateIncomplete
			|| Binding->GetFeatherResourceGeneration() != Scope->FeatherResourceGeneration
			|| Binding->GetFeatherAppliedRevision() != AppliedRevision
			|| Binding->GetFeatherSettingsRevision() != Binding->GetPresentationRevision())
		{
			return FailBlack();
		}
		FRDGTextureRef FeatherPages[4] = { DummyPage, DummyPage, DummyPage, DummyPage };
		for (int32 PageIndex = 0; PageIndex < Scope->Pages.Num() && PageIndex < 4; ++PageIndex)
		{
			if (Scope->Pages[PageIndex].IsValid())
			{
				if (!Scope->FeatherPages.IsValidIndex(PageIndex)
					|| !Scope->FeatherPages[PageIndex].IsValid())
				{
					return FailBlack();
				}
				FeatherPages[PageIndex] = GraphBuilder.RegisterExternalTexture(
					Scope->FeatherPages[PageIndex],
					TEXT("SightWeave.Presentation.FeatherPage"));
			}
		}

		TShaderMapRef<FSightWeaveInwardFeatherCompositePixelShader> PixelShader(
			GetGlobalShaderMap(View.GetFeatureLevel()));
		FSightWeaveInwardFeatherCompositePixelShader::FParameters* Parameters =
			GraphBuilder.AllocParameters<FSightWeaveInwardFeatherCompositePixelShader::FParameters>();
		Parameters->View = View.ViewUniformBuffer;
		Parameters->SceneColorTexture = SceneColor.Texture;
		Parameters->SceneDepthTexture = SceneDepth;
		Parameters->PageTable = GraphBuilder.CreateSRV(Scope->CurrentPageTable);
		Parameters->AtlasPage0 = AtlasPages[0];
		Parameters->AtlasPage1 = AtlasPages[1];
		Parameters->AtlasPage2 = AtlasPages[2];
		Parameters->AtlasPage3 = AtlasPages[3];
		Parameters->FeatherPage0 = FeatherPages[0];
		Parameters->FeatherPage1 = FeatherPages[1];
		Parameters->FeatherPage2 = FeatherPages[2];
		Parameters->FeatherPage3 = FeatherPages[3];
		Parameters->OutputRectMin = Output.ViewRect.Min;
		Parameters->OutputRectSize = Output.ViewRect.Size();
		Parameters->SceneColorRectMin = SceneColor.ViewRect.Min;
		Parameters->SceneColorRectSize = SceneColor.ViewRect.Size();
		Parameters->TranslatedFloorOrigin = FVector2f(TranslatedFloorOrigin);
		Parameters->CentimetersPerTexel = SightWeaveCentimetersPerTexel(
			Binding->GetScopeKey().PrecisionTier);
		Parameters->FeatherWidthCentimeters = Binding->GetVisualFeather().WidthCentimeters;
		Parameters->PageTableCount = static_cast<uint32>(Scope->PageTableEntryCount);
		Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();
		AddDrawScreenPass(
			GraphBuilder,
			RDG_EVENT_NAME("SightWeave.InwardFeatherComposite"),
			View,
			FScreenPassTextureViewport(Output),
			FScreenPassTextureViewport(SceneColor),
			PixelShader,
			Parameters);
	}
	else
	{
		TShaderMapRef<FSightWeaveHardMaskCompositePixelShader> PixelShader(
			GetGlobalShaderMap(View.GetFeatureLevel()));
		FSightWeaveHardMaskCompositePixelShader::FParameters* Parameters =
			GraphBuilder.AllocParameters<FSightWeaveHardMaskCompositePixelShader::FParameters>();
		Parameters->View = View.ViewUniformBuffer;
		Parameters->SceneColorTexture = SceneColor.Texture;
		Parameters->SceneDepthTexture = SceneDepth;
		Parameters->PageTable = GraphBuilder.CreateSRV(Scope->CurrentPageTable);
		Parameters->AtlasPage0 = AtlasPages[0];
		Parameters->AtlasPage1 = AtlasPages[1];
		Parameters->AtlasPage2 = AtlasPages[2];
		Parameters->AtlasPage3 = AtlasPages[3];
		Parameters->OutputRectMin = Output.ViewRect.Min;
		Parameters->OutputRectSize = Output.ViewRect.Size();
		Parameters->SceneColorRectMin = SceneColor.ViewRect.Min;
		Parameters->SceneColorRectSize = SceneColor.ViewRect.Size();
		Parameters->TranslatedFloorOrigin = FVector2f(TranslatedFloorOrigin);
		Parameters->CentimetersPerTexel = SightWeaveCentimetersPerTexel(
			Binding->GetScopeKey().PrecisionTier);
		Parameters->PageTableCount = static_cast<uint32>(Scope->PageTableEntryCount);
		Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();
		AddDrawScreenPass(
			GraphBuilder,
			RDG_EVENT_NAME("SightWeave.HardMaskComposite"),
			View,
			FScreenPassTextureViewport(Output),
			FScreenPassTextureViewport(SceneColor),
			PixelShader,
			Parameters);
	}
	return MoveTemp(Output);
}

void FSightWeaveSparseAtlasRenderState::Release_RenderThread(
	const FSightWeaveRenderWorldIdentity ExpectedWorldIdentity)
{
	check(IsInRenderingThread());
	if (ExpectedWorldIdentity != WorldIdentity || bReleased)
	{
		return;
	}
	bReleased = true;
	PendingPacket.Reset();
	AppliedPacket.Reset();
	PresentationBinding.Reset();
	PresentationSelection = FSightWeaveViewPresentationSelection();
	PresentationBindingFailure = ESightWeavePresentationBindingFailure::WorldTeardown;
	Scopes.Reset();
	VisionScratch.SafeRelease();
	IlluminationScratch.SafeRelease();
	SuppressionScratch.SafeRelease();
	FeatherScratchA.SafeRelease();
	FeatherScratchB.SafeRelease();
	FeatherDirtyCenters.Reset();
	DesiredRevision = 0;
	DesiredHash = 0;
	AppliedRevision = 0;
	++ResourceGeneration;
	++ResidencyGeneration;
	++FeatherResourceGeneration;
	Availability = ESightWeaveRenderAvailability::WorldTeardown;
}

bool FSightWeaveSparseAtlasRenderState::CheckCapabilities_RenderThread()
{
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
	if (PresentationSelection.GetVisualFeather().IsEnabled()
		&& (!GPixelFormats[PF_G32R32F].Supported
			|| !RHIPixelFormatHasCapabilities(PF_G32R32F, RequiredCapabilities)))
	{
		Availability = ESightWeaveRenderAvailability::UnsupportedPixelFormat;
		return false;
	}
	return true;
}

bool FSightWeaveSparseAtlasRenderState::EnsureScratchTextures_RenderThread()
{
	if (VisionScratch.IsValid()
		&& IlluminationScratch.IsValid()
		&& SuppressionScratch.IsValid())
	{
		return true;
	}
	VisionScratch.SafeRelease();
	IlluminationScratch.SafeRelease();
	SuppressionScratch.SafeRelease();
	const FRDGTextureDesc Desc = MakeMaskTextureDesc(SightWeave::SparseAtlas::PhysicalTileSize);
	AllocatePooledTexture(Desc, VisionScratch, TEXT("SightWeave.Sparse.VisionScratch"));
	AllocatePooledTexture(Desc, IlluminationScratch, TEXT("SightWeave.Sparse.IlluminationScratch"));
	AllocatePooledTexture(Desc, SuppressionScratch, TEXT("SightWeave.Sparse.SuppressionScratch"));
	if (!VisionScratch.IsValid()
		|| !IlluminationScratch.IsValid()
		|| !SuppressionScratch.IsValid())
	{
		VisionScratch.SafeRelease();
		IlluminationScratch.SafeRelease();
		SuppressionScratch.SafeRelease();
		return false;
	}
	ScratchAllocationCount += 3;
	++ResourceGeneration;
	return true;
}

bool FSightWeaveSparseAtlasRenderState::EnsurePage_RenderThread(
	FRDGBuilder& GraphBuilder,
	FScopeState& Scope,
	const int32 PageIndex,
	FRDGTextureRef& OutPage,
	bool& bOutColdCreated)
{
	OutPage = nullptr;
	bOutColdCreated = false;
	const int32 MaximumPages = FMath::DivideAndRoundUp(
		Scope.Capacity,
		SightWeave::SparseAtlas::SlotsPerPage);
	if (PageIndex < 0 || PageIndex >= MaximumPages)
	{
		return false;
	}
	if (Scope.Pages.Num() <= PageIndex)
	{
		Scope.Pages.SetNum(PageIndex + 1);
	}
	if (!Scope.Pages[PageIndex].IsValid())
	{
		AllocatePooledTexture(
			MakeMaskTextureDesc(SightWeave::SparseAtlas::PageSize),
			Scope.Pages[PageIndex],
			TEXT("SightWeave.Sparse.EffectivePage"));
		if (!Scope.Pages[PageIndex].IsValid())
		{
			return false;
		}
		++PageAllocationCount;
		++ResourceGeneration;
		bOutColdCreated = true;
	}
	OutPage = GraphBuilder.RegisterExternalTexture(
		Scope.Pages[PageIndex],
		TEXT("SightWeave.Sparse.EffectivePage"));
	if (bOutColdCreated)
	{
		AddClearRenderTargetPass(GraphBuilder, OutPage, FLinearColor::Black);
	}
	return true;
}

bool FSightWeaveSparseAtlasRenderState::EnsureFeatherScratchTextures_RenderThread()
{
	if (FeatherScratchA.IsValid() && FeatherScratchB.IsValid())
	{
		return true;
	}
	FeatherScratchA.SafeRelease();
	FeatherScratchB.SafeRelease();
	const FRDGTextureDesc Desc = MakeFeatherSeedTextureDesc();
	AllocatePooledTexture(Desc, FeatherScratchA, TEXT("SightWeave.Feather.SeedA"));
	AllocatePooledTexture(Desc, FeatherScratchB, TEXT("SightWeave.Feather.SeedB"));
	if (!FeatherScratchA.IsValid() || !FeatherScratchB.IsValid())
	{
		FeatherScratchA.SafeRelease();
		FeatherScratchB.SafeRelease();
		return false;
	}
	FeatherScratchAllocationCount += 2;
	++FeatherResourceGeneration;
	return true;
}

bool FSightWeaveSparseAtlasRenderState::EnsureFeatherPage_RenderThread(
	FRDGBuilder& GraphBuilder,
	FScopeState& Scope,
	const int32 PageIndex,
	FRDGTextureRef& OutPage,
	bool& bOutColdCreated)
{
	OutPage = nullptr;
	bOutColdCreated = false;
	if (!Scope.Pages.IsValidIndex(PageIndex) || !Scope.Pages[PageIndex].IsValid())
	{
		return false;
	}
	if (Scope.FeatherPages.Num() <= PageIndex)
	{
		Scope.FeatherPages.SetNum(PageIndex + 1);
	}
	if (!Scope.FeatherPages[PageIndex].IsValid())
	{
		AllocatePooledTexture(
			MakeMaskTextureDesc(SightWeave::SparseAtlas::PageSize),
			Scope.FeatherPages[PageIndex],
			TEXT("SightWeave.Feather.Page"));
		if (!Scope.FeatherPages[PageIndex].IsValid())
		{
			return false;
		}
		++FeatherPageAllocationCount;
		++FeatherResourceGeneration;
		bOutColdCreated = true;
	}
	OutPage = GraphBuilder.RegisterExternalTexture(
		Scope.FeatherPages[PageIndex],
		TEXT("SightWeave.Feather.Page"));
	if (bOutColdCreated)
	{
		AddClearRenderTargetPass(GraphBuilder, OutPage, FLinearColor::Black);
	}
	return true;
}

void FSightWeaveSparseAtlasRenderState::MarkFeatherDirtyAround_RenderThread(
	const FSightWeaveSparseTileKey& TileKey)
{
	if (!TileKey.IsValid())
	{
		return;
	}
	if (!FeatherDirtyCenters.ContainsByPredicate([&TileKey](const FSightWeaveSparseTileKey& Existing)
	{
		return Existing.IsEquivalentTo(TileKey);
	}))
	{
		FeatherDirtyCenters.Add(TileKey);
	}
}

void FSightWeaveSparseAtlasRenderState::ReleaseFeatherResources_RenderThread()
{
	bool bReleasedAny = FeatherScratchA.IsValid() || FeatherScratchB.IsValid();
	FeatherScratchA.SafeRelease();
	FeatherScratchB.SafeRelease();
	for (TUniquePtr<FScopeState>& Scope : Scopes)
	{
		bReleasedAny |= !Scope->FeatherPages.IsEmpty();
		Scope->FeatherPages.Reset();
		Scope->FeatherAppliedRevision = 0;
		Scope->FeatherSettingsRevision = 0;
		Scope->FeatherResourceGeneration = 0;
	}
	FeatherDirtyCenters.Reset();
	if (bReleasedAny)
	{
		++FeatherResourceGeneration;
	}
}

void FSightWeaveSparseAtlasRenderState::InvalidateFeather_RenderThread(
	const ESightWeavePresentationBindingFailure Failure)
{
	for (TUniquePtr<FScopeState>& Scope : Scopes)
	{
		Scope->FeatherAppliedRevision = 0;
		Scope->FeatherSettingsRevision = 0;
		Scope->FeatherResourceGeneration = 0;
	}
	bFeatherUpdateIncomplete = true;
	PresentationBinding.Reset();
	PresentationBindingFailure = Failure;
}

void FSightWeaveSparseAtlasRenderState::AddTilePasses_RenderThread(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef Page,
	const FIntRect& SlotRect,
	const FSightWeaveSparseRenderTile& Tile)
{
	if (Tile.Indices.IsEmpty())
	{
		return;
	}
	FRDGTextureRef Vision = GraphBuilder.RegisterExternalTexture(
		VisionScratch,
		TEXT("SightWeave.Sparse.VisionScratch"));
	FRDGTextureRef Illumination = GraphBuilder.RegisterExternalTexture(
		IlluminationScratch,
		TEXT("SightWeave.Sparse.IlluminationScratch"));
	FRDGTextureRef Suppression = GraphBuilder.RegisterExternalTexture(
		SuppressionScratch,
		TEXT("SightWeave.Sparse.SuppressionScratch"));
	FRDGBufferRef VertexBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("SightWeave.Sparse.PacketVertices"),
		MakeArrayView(Tile.Vertices));
	FRDGBufferRef IndexBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("SightWeave.Sparse.PacketIndices"),
		MakeArrayView(Tile.Indices));
	FRDGBufferSRVRef VertexSRV = GraphBuilder.CreateSRV(VertexBuffer);
	FRDGBufferSRVRef IndexSRV = GraphBuilder.CreateSRV(IndexBuffer);
	const FIntRect ScratchRect(0, 0, SightWeave::SparseAtlas::PhysicalTileSize,
		SightWeave::SparseAtlas::PhysicalTileSize);
	for (const FSightWeaveSparseProfileGeometry& Profile : Tile.Profiles)
	{
		AddClearRenderTargetPass(GraphBuilder, Vision, FLinearColor::Black);
		AddClearRenderTargetPass(GraphBuilder, Illumination, FLinearColor::Black);
		AddSparseRasterPass(
			GraphBuilder,
			TEXT("SightWeave.Sparse.RasterVision"),
			Vision,
			ScratchRect,
			VertexSRV,
			IndexSRV,
			Profile.VisionRange,
			Tile.CentimetersPerTexel,
			false);
		AddSparseRasterPass(
			GraphBuilder,
			TEXT("SightWeave.Sparse.RasterIllumination"),
			Illumination,
			ScratchRect,
			VertexSRV,
			IndexSRV,
			Profile.IlluminationRange,
			Tile.CentimetersPerTexel,
			false);
		AddProfileCombinePass(GraphBuilder, Page, SlotRect, Vision, Illumination);
	}

	if (!Tile.BypassRange.IsEmpty())
	{
		AddSparseRasterPass(
			GraphBuilder,
			TEXT("SightWeave.Sparse.UnionBypass"),
			Page,
			SlotRect,
			VertexSRV,
			IndexSRV,
			Tile.BypassRange,
			Tile.CentimetersPerTexel,
			true);
	}

	if (!Tile.SuppressionRange.IsEmpty())
	{
		AddClearRenderTargetPass(GraphBuilder, Suppression, FLinearColor::Black);
		AddSparseRasterPass(
			GraphBuilder,
			TEXT("SightWeave.Sparse.RasterSuppression"),
			Suppression,
			ScratchRect,
			VertexSRV,
			IndexSRV,
			Tile.SuppressionRange,
			Tile.CentimetersPerTexel,
			false);
		AddSuppressionPass(GraphBuilder, Page, SlotRect, Suppression);
	}
}

bool FSightWeaveSparseAtlasRenderState::AddFeatherTilePasses_RenderThread(
	FRDGBuilder& GraphBuilder,
	FScopeState& Scope,
	const FSightWeaveSparseRenderTile& Tile,
	const FSightWeaveSparsePhysicalAddress& Address)
{
	check(IsInRenderingThread());
	if (!Scope.CurrentPageTable || !Address.IsValid() || !EnsureFeatherScratchTextures_RenderThread())
	{
		return false;
	}
	FRDGTextureRef FeatherPage = nullptr;
	bool bColdCreated = false;
	if (!EnsureFeatherPage_RenderThread(
			GraphBuilder,
			Scope,
			Address.PageIndex,
			FeatherPage,
			bColdCreated)
		|| !FeatherPage)
	{
		return false;
	}

	FRDGTextureRef HardPages[4];
	FRDGTextureRef DummyPage = GSystemTextures.GetBlackDummy(GraphBuilder);
	for (FRDGTextureRef& HardPage : HardPages)
	{
		HardPage = DummyPage;
	}
	for (int32 PageIndex = 0; PageIndex < Scope.Pages.Num() && PageIndex < 4; ++PageIndex)
	{
		if (Scope.Pages[PageIndex].IsValid())
		{
			HardPages[PageIndex] = GraphBuilder.RegisterExternalTexture(
				Scope.Pages[PageIndex],
				TEXT("SightWeave.Feather.HardPage"));
		}
	}

	FRDGTextureRef ScratchA = GraphBuilder.RegisterExternalTexture(
		FeatherScratchA,
		TEXT("SightWeave.Feather.SeedA"));
	FRDGTextureRef ScratchB = GraphBuilder.RegisterExternalTexture(
		FeatherScratchB,
		TEXT("SightWeave.Feather.SeedB"));
	const int32 WorkSize = SightWeave::VisualFeather::TransformWorkSize;
	const FIntRect WorkRect(0, 0, WorkSize, WorkSize);
	const FIntPoint WorkOrigin = Tile.Identity.TileKey.LogicalCoordinate
		* SightWeave::SparseAtlas::InteriorTileSize
		- FIntPoint(
			SightWeave::VisualFeather::MaximumRadiusTexels,
			SightWeave::VisualFeather::MaximumRadiusTexels);

	TShaderMapRef<FSightWeaveFullscreenVertexShader> VertexShader(
		GetGlobalShaderMap(GMaxRHIFeatureLevel));
	TShaderMapRef<FSightWeaveFeatherSeedPixelShader> SeedShader(
		GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FSightWeaveFeatherSeedPassParameters* SeedParameters =
		GraphBuilder.AllocParameters<FSightWeaveFeatherSeedPassParameters>();
	SeedParameters->PixelShader.PageTable = GraphBuilder.CreateSRV(Scope.CurrentPageTable);
	SeedParameters->PixelShader.AtlasPage0 = HardPages[0];
	SeedParameters->PixelShader.AtlasPage1 = HardPages[1];
	SeedParameters->PixelShader.AtlasPage2 = HardPages[2];
	SeedParameters->PixelShader.AtlasPage3 = HardPages[3];
	SeedParameters->PixelShader.FeatherWorkOrigin = WorkOrigin;
	SeedParameters->PixelShader.FeatherWorkSize = static_cast<uint32>(WorkSize);
	SeedParameters->PixelShader.PageTableCount = static_cast<uint32>(Scope.PageTableEntryCount);
	SeedParameters->RenderTargets[0] = FRenderTargetBinding(ScratchA, ERenderTargetLoadAction::ENoAction);
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("SightWeave.Feather.Seed"),
		SeedParameters,
		ERDGPassFlags::Raster,
		[SeedParameters, VertexShader, SeedShader, WorkRect](FRDGAsyncTask, FRHICommandList& RHICmdList)
		{
			ConfigureViewport(RHICmdList, WorkRect);
			FGraphicsPipelineStateInitializer PSO;
			RHICmdList.ApplyCachedRenderTargets(PSO);
			PSO.BlendState = TStaticBlendState<CW_RG>::GetRHI();
			PSO.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			PSO.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			PSO.PrimitiveType = PT_TriangleList;
			PSO.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			PSO.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			PSO.BoundShaderState.PixelShaderRHI = SeedShader.GetPixelShader();
			SetGraphicsPipelineState(RHICmdList, PSO, 0);
			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), SeedParameters->VertexShader);
			SetShaderParameters(RHICmdList, SeedShader, SeedShader.GetPixelShader(), SeedParameters->PixelShader);
			RHICmdList.SetStreamSource(0, nullptr, 0);
			RHICmdList.DrawPrimitive(0, 1, 1);
		});

	FRDGTextureRef CurrentSeeds = ScratchA;
	FRDGTextureRef NextSeeds = ScratchB;
	for (int32 JumpStep = 256; JumpStep >= 1; JumpStep >>= 1)
	{
		TShaderMapRef<FSightWeaveFeatherJumpPixelShader> JumpShader(
			GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FSightWeaveFeatherJumpPassParameters* JumpParameters =
			GraphBuilder.AllocParameters<FSightWeaveFeatherJumpPassParameters>();
		JumpParameters->PixelShader.FeatherSeedTexture = CurrentSeeds;
		JumpParameters->PixelShader.FeatherWorkOrigin = WorkOrigin;
		JumpParameters->PixelShader.FeatherJumpStep = JumpStep;
		JumpParameters->PixelShader.FeatherWorkSize = static_cast<uint32>(WorkSize);
		JumpParameters->RenderTargets[0] = FRenderTargetBinding(NextSeeds, ERenderTargetLoadAction::ENoAction);
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("SightWeave.Feather.Jump(%d)", JumpStep),
			JumpParameters,
			ERDGPassFlags::Raster,
			[JumpParameters, VertexShader, JumpShader, WorkRect](FRDGAsyncTask, FRHICommandList& RHICmdList)
			{
				ConfigureViewport(RHICmdList, WorkRect);
				FGraphicsPipelineStateInitializer PSO;
				RHICmdList.ApplyCachedRenderTargets(PSO);
				PSO.BlendState = TStaticBlendState<CW_RG>::GetRHI();
				PSO.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
				PSO.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				PSO.PrimitiveType = PT_TriangleList;
				PSO.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
				PSO.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				PSO.BoundShaderState.PixelShaderRHI = JumpShader.GetPixelShader();
				SetGraphicsPipelineState(RHICmdList, PSO, 0);
				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), JumpParameters->VertexShader);
				SetShaderParameters(RHICmdList, JumpShader, JumpShader.GetPixelShader(), JumpParameters->PixelShader);
				RHICmdList.SetStreamSource(0, nullptr, 0);
				RHICmdList.DrawPrimitive(0, 1, 1);
			});
		Swap(CurrentSeeds, NextSeeds);
	}

	const FIntRect SlotRect = Address.GetSlotRect();
	AddClearRenderTargetPass(GraphBuilder, FeatherPage, FLinearColor::Black, SlotRect);
	const FIntRect InteriorRect(
		SlotRect.Min + FIntPoint(SightWeave::SparseAtlas::GutterTexels, SightWeave::SparseAtlas::GutterTexels),
		SlotRect.Max - FIntPoint(SightWeave::SparseAtlas::GutterTexels, SightWeave::SparseAtlas::GutterTexels));
	TShaderMapRef<FSightWeaveFeatherFinalizePixelShader> FinalizeShader(
		GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FSightWeaveFeatherFinalizePassParameters* FinalizeParameters =
		GraphBuilder.AllocParameters<FSightWeaveFeatherFinalizePassParameters>();
	FinalizeParameters->PixelShader.PageTable = GraphBuilder.CreateSRV(Scope.CurrentPageTable);
	FinalizeParameters->PixelShader.AtlasPage0 = HardPages[0];
	FinalizeParameters->PixelShader.AtlasPage1 = HardPages[1];
	FinalizeParameters->PixelShader.AtlasPage2 = HardPages[2];
	FinalizeParameters->PixelShader.AtlasPage3 = HardPages[3];
	FinalizeParameters->PixelShader.FeatherSeedTexture = CurrentSeeds;
	FinalizeParameters->PixelShader.FeatherLogicalTile = Tile.Identity.TileKey.LogicalCoordinate;
	FinalizeParameters->PixelShader.FeatherWidthCentimeters =
		PresentationSelection.GetVisualFeather().WidthCentimeters;
	FinalizeParameters->PixelShader.CentimetersPerTexel = Tile.CentimetersPerTexel;
	FinalizeParameters->PixelShader.PageTableCount = static_cast<uint32>(Scope.PageTableEntryCount);
	FinalizeParameters->PixelShader.DestinationOriginX = static_cast<uint32>(SlotRect.Min.X);
	FinalizeParameters->PixelShader.DestinationOriginY = static_cast<uint32>(SlotRect.Min.Y);
	FinalizeParameters->RenderTargets[0] = FRenderTargetBinding(FeatherPage, ERenderTargetLoadAction::ELoad);
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("SightWeave.Feather.Finalize"),
		FinalizeParameters,
		ERDGPassFlags::Raster,
		[FinalizeParameters, VertexShader, FinalizeShader, InteriorRect](FRDGAsyncTask, FRHICommandList& RHICmdList)
		{
			ConfigureViewport(RHICmdList, InteriorRect);
			FGraphicsPipelineStateInitializer PSO;
			RHICmdList.ApplyCachedRenderTargets(PSO);
			PSO.BlendState = TStaticBlendState<CW_RED>::GetRHI();
			PSO.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			PSO.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			PSO.PrimitiveType = PT_TriangleList;
			PSO.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			PSO.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			PSO.BoundShaderState.PixelShaderRHI = FinalizeShader.GetPixelShader();
			SetGraphicsPipelineState(RHICmdList, PSO, 0);
			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), FinalizeParameters->VertexShader);
			SetShaderParameters(RHICmdList, FinalizeShader, FinalizeShader.GetPixelShader(), FinalizeParameters->PixelShader);
			RHICmdList.SetStreamSource(0, nullptr, 0);
			RHICmdList.DrawPrimitive(0, 1, 1);
		});
	++FeatherTileDispatchCount;
	return true;
}

bool FSightWeaveSparseAtlasRenderState::ProcessVisualFeather_RenderThread(
	FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());
	if (bReleased || !PresentationSelection.IsValid() || !PresentationSelection.IsEnabled())
	{
		return false;
	}
	if (!PresentationSelection.GetVisualFeather().IsEnabled())
	{
		ReleaseFeatherResources_RenderThread();
		RefreshPresentationBinding_RenderThread();
		return false;
	}
	if (!AppliedPacket.IsValid() || !CheckCapabilities_RenderThread())
	{
		InvalidateFeather_RenderThread(ESightWeavePresentationBindingFailure::FeatherUnavailable);
		return false;
	}

	FScopeState* Scope = nullptr;
	for (TUniquePtr<FScopeState>& Candidate : Scopes)
	{
		if (Candidate->ScopeKey.WorldIdentity == PresentationSelection.GetWorldIdentity()
			&& Candidate->ScopeKey.KnowledgeOwnerId == PresentationSelection.GetKnowledgeOwnerId()
			&& Candidate->ScopeKey.FloorId == PresentationSelection.GetFloorId()
			&& Candidate->ScopeKey.PrecisionTier == PresentationSelection.GetPrecisionTier())
		{
			Scope = Candidate.Get();
			break;
		}
	}
	if (!Scope
		|| Scope->Availability != ESightWeaveRenderAvailability::Available
		|| Scope->AppliedRevision != AppliedRevision
		|| (!Scope->CurrentPageTable && !PrepareScopePageTable_RenderThread(GraphBuilder, *Scope)))
	{
		InvalidateFeather_RenderThread(ESightWeavePresentationBindingFailure::FeatherUnavailable);
		return false;
	}

	const double InteriorWorldSpan = SightWeave::SparseAtlas::InteriorTileSize
		* SightWeaveCentimetersPerTexel(Scope->ScopeKey.PrecisionTier);
	const int32 Expansion = FMath::Clamp(
		FMath::CeilToInt(PresentationSelection.GetVisualFeather().WidthCentimeters / InteriorWorldSpan),
		0,
		1);
	TArray<const FSightWeaveSparseRenderTile*> TilesToDerive;
	for (const FSightWeaveSparseRenderTile& Tile : AppliedPacket->GetTiles())
	{
		if (!Tile.Identity.TileKey.Scope.IsEquivalentTo(Scope->ScopeKey))
		{
			continue;
		}
		bool bInclude = bFeatherFullRebuildPending
			|| Scope->FeatherAppliedRevision == 0
			|| Scope->FeatherSettingsRevision != PresentationSelection.GetPresentationRevision();
		if (!bInclude)
		{
			for (const FSightWeaveSparseTileKey& DirtyCenter : FeatherDirtyCenters)
			{
				if (DirtyCenter.Scope.IsEquivalentTo(Scope->ScopeKey)
					&& FMath::Abs(DirtyCenter.LogicalCoordinate.X - Tile.Identity.TileKey.LogicalCoordinate.X) <= Expansion
					&& FMath::Abs(DirtyCenter.LogicalCoordinate.Y - Tile.Identity.TileKey.LogicalCoordinate.Y) <= Expansion)
				{
					bInclude = true;
					break;
				}
			}
		}
		if (bInclude)
		{
			TilesToDerive.Add(&Tile);
		}
	}

	for (const FSightWeaveSparseRenderTile* Tile : TilesToDerive)
	{
		const FSightWeaveSparseResidencySlot* Slot = Scope->Residency.Find(Tile->Identity);
		if (!Slot || Slot->AppliedRevision != AppliedRevision
			|| !AddFeatherTilePasses_RenderThread(GraphBuilder, *Scope, *Tile, Slot->Address))
		{
			InvalidateFeather_RenderThread(ESightWeavePresentationBindingFailure::FeatherUnavailable);
			return false;
		}
	}

	Scope->FeatherAppliedRevision = AppliedRevision;
	Scope->FeatherSettingsRevision = PresentationSelection.GetPresentationRevision();
	Scope->FeatherResourceGeneration = FeatherResourceGeneration;
	FeatherDirtyCenters.Reset();
	bFeatherFullRebuildPending = false;
	bFeatherUpdateIncomplete = false;
	RefreshPresentationBinding_RenderThread();
	return !TilesToDerive.IsEmpty();
}

FSightWeaveSparseAtlasRenderState::FScopeState*
FSightWeaveSparseAtlasRenderState::FindScope_RenderThread(const FSightWeaveSparseScopeKey& ScopeKey)
{
	const TUniquePtr<FScopeState>* Found = Scopes.FindByPredicate(
		[&ScopeKey](const TUniquePtr<FScopeState>& Scope)
		{
			return Scope->ScopeKey.IsEquivalentTo(ScopeKey);
		});
	return Found ? Found->Get() : nullptr;
}

const FSightWeaveSparseAtlasRenderState::FScopeState*
FSightWeaveSparseAtlasRenderState::FindScope_RenderThread(const FSightWeaveSparseScopeKey& ScopeKey) const
{
	const TUniquePtr<FScopeState>* Found = Scopes.FindByPredicate(
		[&ScopeKey](const TUniquePtr<FScopeState>& Scope)
		{
			return Scope->ScopeKey.IsEquivalentTo(ScopeKey);
		});
	return Found ? Found->Get() : nullptr;
}

FSightWeaveSparseAtlasRenderState::FScopeState&
FSightWeaveSparseAtlasRenderState::FindOrAddScope_RenderThread(
	const FSightWeaveSparseRenderScope& Scope)
{
	if (FScopeState* Existing = FindScope_RenderThread(Scope.ScopeKey))
	{
		if (Existing->Capacity != Scope.MaximumActiveTiles)
		{
			Existing->Pages.Reset();
			Existing->FeatherPages.Reset();
			Existing->PageTable.SafeRelease();
			Existing->CurrentPageTable = nullptr;
			Existing->PageTableEntryCount = 0;
			Existing->PageTableResidencyGeneration = 0;
			Existing->PageTablePacketRevision = 0;
			Existing->FeatherAppliedRevision = 0;
			Existing->FeatherSettingsRevision = 0;
			Existing->FeatherResourceGeneration = 0;
			Existing->Capacity = Scope.MaximumActiveTiles;
			Existing->Residency = FSightWeaveSparseAtlasResidency(Scope.MaximumActiveTiles);
			++ResourceGeneration;
			++ResidencyGeneration;
			++FeatherResourceGeneration;
			bFeatherFullRebuildPending = true;
			bFeatherUpdateIncomplete = true;
		}
		return *Existing;
	}
	TUniquePtr<FScopeState>& Added = Scopes.Add_GetRef(
		MakeUnique<FScopeState>(Scope.ScopeKey, Scope.MaximumActiveTiles));
	return *Added;
}

void FSightWeaveSparseAtlasRenderState::FailScope_RenderThread(
	FScopeState& Scope,
	const ESightWeaveRenderAvailability Failure)
{
	Scope.Pages.Reset();
	Scope.FeatherPages.Reset();
	Scope.PageTable.SafeRelease();
	Scope.CurrentPageTable = nullptr;
	Scope.PageTableEntryCount = 0;
	Scope.PageTableResidencyGeneration = 0;
	Scope.PageTablePacketRevision = 0;
	Scope.Residency.Reset();
	Scope.AppliedRevision = 0;
	Scope.FeatherAppliedRevision = 0;
	Scope.FeatherSettingsRevision = 0;
	Scope.FeatherResourceGeneration = 0;
	Scope.Availability = Failure;
	++ResourceGeneration;
	++ResidencyGeneration;
	++FeatherResourceGeneration;
	bFeatherUpdateIncomplete = true;
}

void FSightWeaveSparseAtlasRenderState::FailAllScopes_RenderThread(
	const ESightWeaveRenderAvailability Failure)
{
	for (TUniquePtr<FScopeState>& Scope : Scopes)
	{
		FailScope_RenderThread(*Scope, Failure);
	}
	Availability = Failure;
}

void FSightWeaveSparseAtlasRenderState::RemoveAbsentScopes_RenderThread(
	const FSightWeaveSparseRenderPacket& Packet)
{
	const int32 Removed = Scopes.RemoveAll([&Packet](const TUniquePtr<FScopeState>& Scope)
	{
		return !PacketContainsScope(Packet, Scope->ScopeKey);
	});
	ResourceGeneration += static_cast<uint64>(Removed);
	ResidencyGeneration += static_cast<uint64>(Removed);
	FeatherResourceGeneration += static_cast<uint64>(Removed);
}

bool FSightWeaveSparseAtlasRenderState::HasCompletePresentationResidency_RenderThread(
	const FSightWeaveSparseRenderPacket& Packet,
	const FSightWeaveSparseScopeKey& ScopeKey) const
{
	const FScopeState* Scope = FindScope_RenderThread(ScopeKey);
	if (!Scope
		|| Scope->Availability != ESightWeaveRenderAvailability::Available
		|| Scope->AppliedRevision != Packet.GetPacketRevision())
	{
		return false;
	}
	for (const FSightWeaveSparseRenderTile& Tile : Packet.GetTiles())
	{
		if (!Tile.Identity.TileKey.Scope.IsEquivalentTo(ScopeKey))
		{
			continue;
		}
		const FSightWeaveSparseResidencySlot* Slot = Scope->Residency.Find(Tile.Identity);
		if (!Slot
			|| Slot->AppliedRevision != Packet.GetPacketRevision()
			|| !Scope->Pages.IsValidIndex(Slot->Address.PageIndex)
			|| !Scope->Pages[Slot->Address.PageIndex].IsValid())
		{
			return false;
		}
	}
	return true;
}

void FSightWeaveSparseAtlasRenderState::RefreshPresentationBinding_RenderThread()
{
	check(IsInRenderingThread());
	PresentationBinding.Reset();
	if (bReleased)
	{
		PresentationBindingFailure = ESightWeavePresentationBindingFailure::WorldTeardown;
		return;
	}
	if (!PresentationSelection.IsValid())
	{
		PresentationBindingFailure = ESightWeavePresentationBindingFailure::InvalidSelection;
		return;
	}
	if (!PresentationSelection.IsEnabled())
	{
		PresentationBindingFailure = ESightWeavePresentationBindingFailure::Disabled;
		return;
	}
	if (!AppliedPacket.IsValid())
	{
		PresentationBindingFailure = ESightWeavePresentationBindingFailure::InvalidPacket;
		return;
	}
	if (PresentationSelection.GetVisualFeather().IsEnabled() && bFeatherUpdateIncomplete)
	{
		PresentationBindingFailure = ESightWeavePresentationBindingFailure::FeatherUnavailable;
		return;
	}
	FScopeState* SelectedScope = nullptr;
	for (TUniquePtr<FScopeState>& Candidate : Scopes)
	{
		if (Candidate->ScopeKey.WorldIdentity == PresentationSelection.GetWorldIdentity()
			&& Candidate->ScopeKey.KnowledgeOwnerId == PresentationSelection.GetKnowledgeOwnerId()
			&& Candidate->ScopeKey.FloorId == PresentationSelection.GetFloorId()
			&& Candidate->ScopeKey.PrecisionTier == PresentationSelection.GetPrecisionTier())
		{
			SelectedScope = Candidate.Get();
			break;
		}
	}
	const FSightWeavePresentationBindingBuildResult Built =
		FSightWeavePresentationBindingBuilder::Build(
			*AppliedPacket,
			PresentationSelection,
			ResourceGeneration,
			ResidencyGeneration,
			SelectedScope ? SelectedScope->FeatherResourceGeneration : 0,
			SelectedScope ? SelectedScope->FeatherAppliedRevision : 0,
			SelectedScope ? SelectedScope->FeatherSettingsRevision : 0);
	if (!Built.Succeeded())
	{
		PresentationBindingFailure = Built.Failure;
		return;
	}
	if (!HasCompletePresentationResidency_RenderThread(*AppliedPacket, Built.Binding->GetScopeKey()))
	{
		PresentationBindingFailure = ESightWeavePresentationBindingFailure::ResidencyIncomplete;
		return;
	}
	if (Built.Binding->GetVisualFeather().IsEnabled())
	{
		if (!SelectedScope
			|| SelectedScope->FeatherAppliedRevision != AppliedRevision
			|| SelectedScope->FeatherSettingsRevision
				!= PresentationSelection.GetPresentationRevision()
			|| SelectedScope->FeatherResourceGeneration == 0)
		{
			PresentationBindingFailure = ESightWeavePresentationBindingFailure::FeatherRevisionMismatch;
			return;
		}
		for (const FSightWeaveSparseResidencySlot& Slot : SelectedScope->Residency.GetSlots())
		{
			if (Slot.bOccupied
				&& (!SelectedScope->FeatherPages.IsValidIndex(Slot.Address.PageIndex)
					|| !SelectedScope->FeatherPages[Slot.Address.PageIndex].IsValid()))
			{
				PresentationBindingFailure = ESightWeavePresentationBindingFailure::FeatherUnavailable;
				return;
			}
		}
	}
	PresentationBinding = Built.Binding;
	PresentationBindingFailure = ESightWeavePresentationBindingFailure::None;
}

uint64 FSightWeaveSparseAtlasRenderState::GetEvictionCount_RenderThread() const
{
	uint64 Count = 0;
	for (const TUniquePtr<FScopeState>& Scope : Scopes)
	{
		Count += Scope->Residency.GetEvictionCount();
	}
	return Count;
}

int32 FSightWeaveSparseAtlasRenderState::GetResidentTileCount_RenderThread() const
{
	int32 Count = 0;
	for (const TUniquePtr<FScopeState>& Scope : Scopes)
	{
		Count += Scope->Residency.GetResidentCount();
	}
	return Count;
}

int32 FSightWeaveSparseAtlasRenderState::GetAllocatedPageCount_RenderThread() const
{
	int32 Count = 0;
	for (const TUniquePtr<FScopeState>& Scope : Scopes)
	{
		for (const TRefCountPtr<IPooledRenderTarget>& Page : Scope->Pages)
		{
			Count += Page.IsValid() ? 1 : 0;
		}
	}
	return Count;
}

#if WITH_DEV_AUTOMATION_TESTS
FRDGTextureRef FSightWeaveSparseAtlasRenderState::AddPresentationTestComposite_RenderThread(
	FRDGBuilder& GraphBuilder,
	const TConstArrayView<FVector2f> TranslatedWorldPositions,
	const TConstArrayView<FVector4f> SceneColors)
{
	check(IsInRenderingThread());
	if (TranslatedWorldPositions.IsEmpty()
		|| TranslatedWorldPositions.Num() != SceneColors.Num())
	{
		return nullptr;
	}
	const FRDGTextureDesc OutputDesc = FRDGTextureDesc::Create2D(
		FIntPoint(TranslatedWorldPositions.Num(), 1),
		PF_R8G8B8A8,
		FClearValueBinding::Black,
		TexCreate_RenderTargetable | TexCreate_ShaderResource);
	FRDGTextureRef Output = GraphBuilder.CreateTexture(
		OutputDesc,
		TEXT("SightWeave.Presentation.TestOutput"));

	const TSharedPtr<const FSightWeaveViewPresentationBinding, ESPMode::ThreadSafe> Binding =
		PresentationBinding;
	FScopeState* Scope = Binding.IsValid() ? FindScope_RenderThread(Binding->GetScopeKey()) : nullptr;
	if (!Binding.IsValid()
		|| !Binding->IsValid()
		|| Binding->GetResourceGeneration() != ResourceGeneration
		|| Binding->GetResidencyGeneration() != ResidencyGeneration
		|| Binding->GetPacketRevision() != AppliedRevision
		|| !Scope
		|| Scope->Availability != ESightWeaveRenderAvailability::Available
		|| Scope->PageTableResidencyGeneration != ResidencyGeneration
		|| Scope->PageTablePacketRevision != AppliedRevision
		|| !Scope->CurrentPageTable)
	{
		AddClearRenderTargetPass(GraphBuilder, Output, FLinearColor::Black);
		return Output;
	}
	for (const FSightWeaveSparseResidencySlot& Slot : Scope->Residency.GetSlots())
	{
		if (Slot.bOccupied && Slot.Address.PageIndex >= 4)
		{
			AddClearRenderTargetPass(GraphBuilder, Output, FLinearColor::Black);
			return Output;
		}
	}

	TArray<FVector2f> OwnedPositions(TranslatedWorldPositions);
	TArray<FVector4f> OwnedColors(SceneColors);
	FRDGBufferRef PositionBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("SightWeave.Presentation.TestPositions"),
		MoveTemp(OwnedPositions));
	FRDGBufferRef ColorBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("SightWeave.Presentation.TestColors"),
		MoveTemp(OwnedColors));

	TShaderMapRef<FSightWeaveFullscreenVertexShader> VertexShader(
		GetGlobalShaderMap(GMaxRHIFeatureLevel));
	TShaderMapRef<FSightWeavePresentationTestPixelShader> PixelShader(
		GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FSightWeavePresentationTestPassParameters* Parameters =
		GraphBuilder.AllocParameters<FSightWeavePresentationTestPassParameters>();
	Parameters->PixelShader.TestTranslatedWorldPositions = GraphBuilder.CreateSRV(PositionBuffer);
	Parameters->PixelShader.TestSceneColors = GraphBuilder.CreateSRV(ColorBuffer);
	Parameters->PixelShader.PageTable = GraphBuilder.CreateSRV(Scope->CurrentPageTable);
	FRDGTextureRef DummyPage = GSystemTextures.GetBlackDummy(GraphBuilder);
	FRDGTextureRef AtlasPages[4] = { DummyPage, DummyPage, DummyPage, DummyPage };
	for (int32 PageIndex = 0; PageIndex < Scope->Pages.Num() && PageIndex < 4; ++PageIndex)
	{
		if (Scope->Pages[PageIndex].IsValid())
		{
			AtlasPages[PageIndex] = GraphBuilder.RegisterExternalTexture(
				Scope->Pages[PageIndex],
				TEXT("SightWeave.Presentation.TestAtlasPage"));
		}
	}
	Parameters->PixelShader.AtlasPage0 = AtlasPages[0];
	Parameters->PixelShader.AtlasPage1 = AtlasPages[1];
	Parameters->PixelShader.AtlasPage2 = AtlasPages[2];
	Parameters->PixelShader.AtlasPage3 = AtlasPages[3];
	const FVector2D FloorOrigin = Binding->GetScopeKey().FloorOrigin;
	Parameters->PixelShader.TranslatedFloorOrigin = FVector2f(FloorOrigin);
	Parameters->PixelShader.CentimetersPerTexel = SightWeaveCentimetersPerTexel(
		Binding->GetScopeKey().PrecisionTier);
	Parameters->PixelShader.PageTableCount = static_cast<uint32>(Scope->PageTableEntryCount);
	Parameters->PixelShader.TestSampleCount = static_cast<uint32>(TranslatedWorldPositions.Num());
	Parameters->RenderTargets[0] = FRenderTargetBinding(Output, ERenderTargetLoadAction::EClear);
	const FIntRect Viewport(FIntPoint::ZeroValue, OutputDesc.Extent);
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("SightWeave.Presentation.TestComposite"),
		Parameters,
		ERDGPassFlags::Raster,
		[Parameters, VertexShader, PixelShader, Viewport](FRDGAsyncTask, FRHICommandList& RHICmdList)
		{
			ConfigureViewport(RHICmdList, Viewport);
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA>::GetRHI();
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
				Parameters->VertexShader);
			SetShaderParameters(
				RHICmdList,
				PixelShader,
				PixelShader.GetPixelShader(),
				Parameters->PixelShader);
			RHICmdList.SetStreamSource(0, nullptr, 0);
			RHICmdList.DrawPrimitive(0, 1, 1);
		});
	return Output;
}

FRDGTextureRef FSightWeaveSparseAtlasRenderState::AddPresentationBenchmarkComposite_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FIntPoint OutputExtent,
	const FVector2f TestWorldMin,
	const FVector2f TestWorldStep)
{
	check(IsInRenderingThread());
	if (OutputExtent.X <= 0 || OutputExtent.Y <= 0)
	{
		return nullptr;
	}
	const FRDGTextureDesc OutputDesc = FRDGTextureDesc::Create2D(
		OutputExtent,
		PF_R8G8B8A8,
		FClearValueBinding::Black,
		TexCreate_RenderTargetable | TexCreate_ShaderResource);
	FRDGTextureRef Output = GraphBuilder.CreateTexture(
		OutputDesc,
		TEXT("SightWeave.Presentation.BenchmarkOutput"));

	const TSharedPtr<const FSightWeaveViewPresentationBinding, ESPMode::ThreadSafe> Binding =
		PresentationBinding;
	FScopeState* Scope = Binding.IsValid() ? FindScope_RenderThread(Binding->GetScopeKey()) : nullptr;
	if (!Binding.IsValid()
		|| !Binding->IsValid()
		|| Binding->GetResourceGeneration() != ResourceGeneration
		|| Binding->GetResidencyGeneration() != ResidencyGeneration
		|| Binding->GetPacketRevision() != AppliedRevision
		|| !Scope
		|| Scope->Availability != ESightWeaveRenderAvailability::Available
		|| Scope->PageTableResidencyGeneration != ResidencyGeneration
		|| Scope->PageTablePacketRevision != AppliedRevision
		|| !Scope->CurrentPageTable)
	{
		AddClearRenderTargetPass(GraphBuilder, Output, FLinearColor::Black);
		return Output;
	}

	TShaderMapRef<FSightWeaveFullscreenVertexShader> VertexShader(
		GetGlobalShaderMap(GMaxRHIFeatureLevel));
	TShaderMapRef<FSightWeavePresentationBenchmarkPixelShader> PixelShader(
		GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FSightWeavePresentationBenchmarkPassParameters* Parameters =
		GraphBuilder.AllocParameters<FSightWeavePresentationBenchmarkPassParameters>();
	Parameters->PixelShader.PageTable = GraphBuilder.CreateSRV(Scope->CurrentPageTable);
	FRDGTextureRef DummyPage = GSystemTextures.GetBlackDummy(GraphBuilder);
	FRDGTextureRef AtlasPages[4] = { DummyPage, DummyPage, DummyPage, DummyPage };
	for (int32 PageIndex = 0; PageIndex < Scope->Pages.Num() && PageIndex < 4; ++PageIndex)
	{
		if (Scope->Pages[PageIndex].IsValid())
		{
			AtlasPages[PageIndex] = GraphBuilder.RegisterExternalTexture(
				Scope->Pages[PageIndex],
				TEXT("SightWeave.Presentation.BenchmarkAtlasPage"));
		}
	}
	Parameters->PixelShader.AtlasPage0 = AtlasPages[0];
	Parameters->PixelShader.AtlasPage1 = AtlasPages[1];
	Parameters->PixelShader.AtlasPage2 = AtlasPages[2];
	Parameters->PixelShader.AtlasPage3 = AtlasPages[3];
	Parameters->PixelShader.TranslatedFloorOrigin = FVector2f(Binding->GetScopeKey().FloorOrigin);
	Parameters->PixelShader.CentimetersPerTexel = SightWeaveCentimetersPerTexel(
		Binding->GetScopeKey().PrecisionTier);
	Parameters->PixelShader.PageTableCount = static_cast<uint32>(Scope->PageTableEntryCount);
	Parameters->PixelShader.TestWorldMin = TestWorldMin;
	Parameters->PixelShader.TestWorldStep = TestWorldStep;
	Parameters->PixelShader.TestOutputExtent = OutputExtent;
	Parameters->RenderTargets[0] = FRenderTargetBinding(Output, ERenderTargetLoadAction::EClear);
	const FIntRect Viewport(FIntPoint::ZeroValue, OutputExtent);
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("SightWeave.Presentation.BenchmarkComposite"),
		Parameters,
		ERDGPassFlags::Raster,
		[Parameters, VertexShader, PixelShader, Viewport](FRDGAsyncTask, FRHICommandList& RHICmdList)
		{
			ConfigureViewport(RHICmdList, Viewport);
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI =
				GEmptyVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Parameters->VertexShader);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), Parameters->PixelShader);
			RHICmdList.SetStreamSource(0, nullptr, 0);
			RHICmdList.DrawPrimitive(0, 1, 1);
		});
	return Output;
}

FRDGTextureRef FSightWeaveSparseAtlasRenderState::RegisterResidentPageForReadback_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FSightWeaveSparseTileIdentity& Identity,
	FIntRect& OutSlotRect,
	FSightWeaveSparsePhysicalAddress& OutAddress)
{
	FScopeState* Scope = FindScope_RenderThread(Identity.TileKey.Scope);
	const FSightWeaveSparseResidencySlot* Slot = Scope ? Scope->Residency.Find(Identity) : nullptr;
	if (!Scope || !Slot || !Scope->Pages.IsValidIndex(Slot->Address.PageIndex)
		|| !Scope->Pages[Slot->Address.PageIndex].IsValid())
	{
		return nullptr;
	}
	OutAddress = Slot->Address;
	OutSlotRect = Slot->Address.GetSlotRect();
	return GraphBuilder.RegisterExternalTexture(
		Scope->Pages[Slot->Address.PageIndex],
		TEXT("SightWeave.Sparse.ReadbackPage"));
}

bool FSightWeaveSparseAtlasRenderState::AddReadback_RenderThread(
	const FSightWeaveSparseTileIdentity& Identity)
{
	FScopeState* Scope = FindScope_RenderThread(Identity.TileKey.Scope);
	const FSightWeaveSparseResidencySlot* Slot = Scope ? Scope->Residency.Find(Identity) : nullptr;
	return Scope && Slot && Scope->Residency.AddReadback(Slot->Address);
}

bool FSightWeaveSparseAtlasRenderState::RemoveReadback_RenderThread(
	const FSightWeaveSparseTileIdentity& Identity,
	const FSightWeaveSparsePhysicalAddress& Address)
{
	FScopeState* Scope = FindScope_RenderThread(Identity.TileKey.Scope);
	return Scope && Scope->Residency.RemoveReadback(Address);
}
#endif
