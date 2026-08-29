#include "SightWeaveSparseAtlasRenderState.h"

#include "Algo/Sort.h"
#include "CommonRenderResources.h"
#include "GlobalShader.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHI.h"
#include "RHIGlobals.h"
#include "RHIStaticStates.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "ScreenPass.h"
#include "SightWeaveRenderModule.h"
#include "SightWeaveLastSeenProxyComponent.h"
#include "SightWeavePresentation.h"
#include "SightWeaveTileShaders.h"
#include "SystemTextures.h"

namespace SightWeaveSparseAtlasRenderPrivate
{
	TAutoConsoleVariable<float> CVarRememberedBrightness(
		TEXT("r.SightWeave.RememberedBrightness"),
		0.22f,
		TEXT("Neutral brightness of the DARKWELL filtered static Remembered scene."),
		ECVF_RenderThreadSafe);
	TAutoConsoleVariable<float> CVarRememberedContrast(
		TEXT("r.SightWeave.RememberedContrast"),
		0.42f,
		TEXT("Static class contrast retained by the DARKWELL Remembered scene."),
		ECVF_RenderThreadSafe);
	TAutoConsoleVariable<float> CVarRememberedDetailStrength(
		TEXT("r.SightWeave.RememberedDetailStrength"),
		0.055f,
		TEXT("Stable low-information geometric detail in the DARKWELL Remembered scene."),
		ECVF_RenderThreadSafe);
	TAutoConsoleVariable<float> CVarRememberedDetailWorldScale(
		TEXT("r.SightWeave.RememberedDetailWorldScale"),
		160.0f,
		TEXT("World-space centimeters per Remembered detail cell."),
		ECVF_RenderThreadSafe);
	TAutoConsoleVariable<float> CVarRememberedSurfaceDepthTolerance(
		TEXT("r.SightWeave.RememberedSurfaceDepthToleranceCm"),
		8.0f,
		TEXT("Maximum SceneDepth/CustomDepth separation for immutable surface classification."),
		ECVF_RenderThreadSafe);
	TAutoConsoleVariable<float> CVarOccluderSurfaceBias(
		TEXT("r.SightWeave.OccluderSurfaceBiasCm"),
		7.5f,
		TEXT("Conservative visibility bias applied only to classified occluder surfaces."),
		ECVF_RenderThreadSafe);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	TAutoConsoleVariable<int32> CVarDiagnosticCompositeMode(
		TEXT("r.SightWeave.Diagnostic.CompositeMode"),
		0,
		TEXT("DARKWELL visual-rescue A/B mode: 0 normal, 1 bypass, 2 no Remembered, "
			"3 Remembered without CustomDepth/Stencil, 4 unified state, 5 SceneDepth world, "
			"6 CustomDepth, 7 CustomStencil, 8 no occluder conservative sampling, "
			"9 raw memory atlas, 10 raw static-attribute atlas, 11 Remembered surface "
			"classification, 12 Remembered current SceneColor, 13 fixed-input gray filter, "
			"14 post-TSR stable Remembered shading, 15 pre-TSR B0 fixed surface/fixed gray "
			"without CustomDepth or CustomStencil reads."),
		ECVF_RenderThreadSafe);
	TAutoConsoleVariable<int32> CVarDiagnosticStableDepthCoordinates(
		TEXT("r.SightWeave.Diagnostic.StableDepthCoordinates"),
		1,
		TEXT("Use the formal jitter-compensated SceneDepth coordinates with unjittered "
			"CustomDepth. Set to 0 only for Development/Editor legacy-path A/B diagnosis."),
		ECVF_RenderThreadSafe);
	TAutoConsoleVariable<int32> CVarDiagnosticFreezeMaskUpdates(
		TEXT("r.SightWeave.Diagnostic.FreezeMaskUpdates"),
		0,
		TEXT("Freeze live mask revisions after the first accepted packet."),
		ECVF_RenderThreadSafe);
	TAutoConsoleVariable<int32> CVarDiagnosticForceFullMaskRebuild(
		TEXT("r.SightWeave.Diagnostic.ForceFullMaskRebuild"),
		0,
		TEXT("Force every accepted live mask packet through a stable full rebuild."),
		ECVF_RenderThreadSafe);
	TAutoConsoleVariable<int32> CVarDiagnosticLogFrames(
		TEXT("r.SightWeave.Diagnostic.LogFrames"),
		0,
		TEXT("Emit one concise visual-rescue render diagnostic record per frame."),
		ECVF_RenderThreadSafe);
#endif

	uint32 MemoryScopeMismatchMask(
		const FSightWeaveMemoryScopeKey& MemoryScope,
		const FSightWeaveViewPresentationBinding& Binding)
	{
		const FSightWeaveSparseScopeKey& LiveScope = Binding.GetScopeKey();
		const TConstArrayView<FSightWeaveRenderProfileIdentity> LiveProfiles =
			Binding.GetCanonicalProfiles();
		uint32 MismatchMask = 0;
		MismatchMask |= MemoryScope.WorldIdentity != LiveScope.WorldIdentity ? 1u << 0 : 0;
		MismatchMask |= MemoryScope.KnowledgeOwnerId != LiveScope.KnowledgeOwnerId ? 1u << 1 : 0;
		MismatchMask |= MemoryScope.FloorId != LiveScope.FloorId ? 1u << 2 : 0;
		MismatchMask |= MemoryScope.PrecisionTier != LiveScope.PrecisionTier ? 1u << 3 : 0;
		MismatchMask |= MemoryScope.FloorOrigin != LiveScope.FloorOrigin ? 1u << 4 : 0;
		MismatchMask |= MemoryScope.CanonicalProfiles.Num() != LiveProfiles.Num() ? 1u << 5 : 0;
		if (MemoryScope.CanonicalProfiles.Num() == LiveProfiles.Num())
		{
			for (int32 Index = 0; Index < LiveProfiles.Num(); ++Index)
			{
				if (!MemoryScope.CanonicalProfiles[Index].IsEquivalentTo(LiveProfiles[Index]))
				{
					MismatchMask |= 1u << 6;
					break;
				}
			}
		}
		return MismatchMask;
	}

	bool MemoryScopeMatchesPresentationBinding(
		const FSightWeaveMemoryScopeKey& MemoryScope,
		const FSightWeaveViewPresentationBinding& Binding)
	{
		return MemoryScopeMismatchMask(MemoryScope, Binding) == 0;
	}

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

	BEGIN_SHADER_PARAMETER_STRUCT(FSightWeaveMemoryUploadPassParameters, )
		RDG_TEXTURE_ACCESS(Texture, ERHIAccess::CopyDest)
	END_SHADER_PARAMETER_STRUCT()

#if WITH_DEV_AUTOMATION_TESTS
	BEGIN_SHADER_PARAMETER_STRUCT(FSightWeaveMemoryPresentationTestPassParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveFullscreenVertexShader::FParameters, VertexShader)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveMemoryPresentationTestPixelShader::FParameters, PixelShader)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FSightWeavePresentationTestPassParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveFullscreenVertexShader::FParameters, VertexShader)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeavePresentationTestPixelShader::FParameters, PixelShader)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FSightWeaveFeatherPresentationTestPassParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveFullscreenVertexShader::FParameters, VertexShader)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveFeatherPresentationTestPixelShader::FParameters, PixelShader)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FSightWeavePresentationBenchmarkPassParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveFullscreenVertexShader::FParameters, VertexShader)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeavePresentationBenchmarkPixelShader::FParameters, PixelShader)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FSightWeaveFeatherPresentationBenchmarkPassParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveFullscreenVertexShader::FParameters, VertexShader)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSightWeaveFeatherPresentationBenchmarkPixelShader::FParameters, PixelShader)
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

	FSightWeaveSparseTileIdentity MakeMemoryTileIdentity(
		const FSightWeaveMemoryScopeKey& Scope,
		const FIntPoint LogicalCoordinate)
	{
		FSightWeaveSparseTileIdentity Identity;
		Identity.TileKey.Scope.WorldIdentity = Scope.WorldIdentity;
		Identity.TileKey.Scope.KnowledgeOwnerId = Scope.KnowledgeOwnerId;
		Identity.TileKey.Scope.FloorId = Scope.FloorId;
		Identity.TileKey.Scope.PrecisionTier = Scope.PrecisionTier;
		Identity.TileKey.Scope.FloorOrigin = Scope.FloorOrigin;
		Identity.TileKey.LogicalCoordinate = LogicalCoordinate;
		Identity.CanonicalProfiles = Scope.CanonicalProfiles;
		return Identity;
	}

	bool MemoryTileMatchesScope(
		const FSightWeavePackedMemoryTile& Tile,
		const FSightWeaveMemoryScopeKey& Scope)
	{
		return Tile.IsValid() && Tile.Key.Scope.IsEquivalentTo(Scope);
	}

	int32 FloorDivide(const int32 Value, const int32 Divisor)
	{
		check(Divisor > 0);
		const int32 Quotient = Value / Divisor;
		const int32 Remainder = Value % Divisor;
		return Remainder < 0 ? Quotient - 1 : Quotient;
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

struct FSightWeaveSparseAtlasRenderState::FMemoryMirrorState final
{
	FMemoryMirrorState()
		: Residency(SightWeave::SparseAtlas::StandardActiveTileCapacity)
	{
	}

	FSightWeaveMemoryScopeKey Scope;
	FSightWeaveSparseAtlasResidency Residency;
	TMap<FIntPoint, FSightWeavePackedMemoryTile> CachedTiles;
	TArray<TRefCountPtr<IPooledRenderTarget>> Pages;
	TRefCountPtr<FRDGPooledBuffer> PageTable;
	FRDGBufferRef CurrentPageTable = nullptr;
	TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> PendingPacket;
	TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> AppliedPacket;
	uint64 DesiredPacketRevision = 0;
	uint64 AppliedPacketRevision = 0;
	uint64 AppliedMemoryRevision = 0;
	uint64 AppliedModifierRevision = 0;
	uint64 ResourceGeneration = 1;
	uint64 ResidencyGeneration = 1;
	uint64 PageTableResidencyGeneration = 0;
	uint64 PageTableMemoryRevision = 0;
	uint64 UploadCount = 0;
	uint64 PageTableUploadCount = 0;
	int32 PageTableEntryCount = 0;
	ESightWeaveRenderAvailability Availability = ESightWeaveRenderAvailability::Unknown;
	bool bHasScope = false;
	bool bForceFullRebuild = false;
};

struct FSightWeaveSparseAtlasRenderState::FStaticAttributeMirrorState final
{
	FStaticAttributeMirrorState()
		: Residency(SightWeave::StaticEnvironment::DefaultMaximumTiles)
	{
	}

	FSightWeaveMemoryScopeKey Scope;
	FSightWeaveSparseAtlasResidency Residency;
	TMap<FIntPoint, FSightWeaveStaticEnvironmentTile> CachedTiles;
	TArray<TRefCountPtr<IPooledRenderTarget>> Pages;
	TRefCountPtr<FRDGPooledBuffer> PageTable;
	FRDGBufferRef CurrentPageTable = nullptr;
	TSharedPtr<const FSightWeaveStaticEnvironmentPacket, ESPMode::ThreadSafe> PendingPacket;
	TSharedPtr<const FSightWeaveStaticEnvironmentPacket, ESPMode::ThreadSafe> AppliedPacket;
	uint64 DesiredPacketRevision = 0;
	uint64 AppliedPacketRevision = 0;
	uint64 AppliedEligibilityRevision = 0;
	uint64 ResourceGeneration = 1;
	uint64 ResidencyGeneration = 1;
	uint64 PageTableResidencyGeneration = 0;
	uint64 PageTableEligibilityRevision = 0;
	uint64 UploadCount = 0;
	uint64 PageTableUploadCount = 0;
	int32 PageTableEntryCount = 0;
	ESightWeaveRenderAvailability Availability = ESightWeaveRenderAvailability::Unknown;
	bool bHasScope = false;
};

FSightWeaveSparseAtlasRenderState::FSightWeaveSparseAtlasRenderState(
	const FSightWeaveRenderWorldIdentity InWorldIdentity)
	: WorldIdentity(InWorldIdentity)
	, MemoryMirror(MakeUnique<FMemoryMirrorState>())
	, StaticAttributeMirror(MakeUnique<FStaticAttributeMirrorState>())
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
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CVarDiagnosticFreezeMaskUpdates.GetValueOnRenderThread() != 0
		&& DesiredRevision != 0)
	{
		return;
	}
#endif
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
			bPendingRequiresFullRebuild = false;
			DesiredHash = 0;
			Availability = ESightWeaveRenderAvailability::InvalidPacket;
		}
#if WITH_DEV_AUTOMATION_TESTS
		FinishConsumeTiming();
#endif
		return;
	}
	const bool bDroppedUnappliedPacket = PendingPacket.IsValid();
	DesiredRevision = Packet->GetPacketRevision();
	DesiredHash = Packet->GetContentHash();
	PendingPacket = Packet;
	bPendingForceBlack = false;
	bPendingRequiresFullRebuild = bPendingRequiresFullRebuild
		|| bDroppedUnappliedPacket
		|| !AppliedPacket.IsValid();
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

void FSightWeaveSparseAtlasRenderState::SubmitMemoryPacket_RenderThread(
	const TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe>& Packet)
{
	check(IsInRenderingThread());
	if (bReleased || !MemoryMirror.IsValid())
	{
		return;
	}
	if (!Packet.IsValid())
	{
		MemoryMirror->PendingPacket.Reset();
		MemoryMirror->Availability = ESightWeaveRenderAvailability::Unknown;
		return;
	}
	if (Packet->GetPacketRevision() <= MemoryMirror->DesiredPacketRevision)
	{
		return;
	}
	if (MemoryMirror->bHasScope
		&& !MemoryMirror->Scope.IsEquivalentTo(Packet->GetScope()))
	{
		MemoryMirror->PendingPacket = Packet;
		MemoryMirror->DesiredPacketRevision = Packet->GetPacketRevision();
		MemoryMirror->Availability = ESightWeaveRenderAvailability::InvalidPacket;
		MemoryMirror->bForceFullRebuild = true;
		return;
	}
	const bool bDroppedPending = MemoryMirror->PendingPacket.IsValid();
	MemoryMirror->PendingPacket = Packet;
	MemoryMirror->DesiredPacketRevision = Packet->GetPacketRevision();
	MemoryMirror->bForceFullRebuild = MemoryMirror->bForceFullRebuild
		|| bDroppedPending
		|| !MemoryMirror->AppliedPacket.IsValid();
}

bool FSightWeaveSparseAtlasRenderState::ProcessMemoryPending_RenderThread(
	FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());
	if (bReleased || !MemoryMirror.IsValid() || !MemoryMirror->PendingPacket.IsValid())
	{
		return false;
	}
	const TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> Packet =
		MoveTemp(MemoryMirror->PendingPacket);
	const bool bForceFullRebuild = MemoryMirror->bForceFullRebuild || Packet->IsFullRebuild();
	MemoryMirror->bForceFullRebuild = false;

	if (!Packet->IsValid()
		|| !Packet->GetScope().IsValid()
		|| Packet->GetAuthorityTiles().Num()
			> SightWeave::SparseAtlas::StandardActiveTileCapacity)
	{
		FailMemoryMirror_RenderThread(ESightWeaveRenderAvailability::InvalidPacket);
		return false;
	}
	if (MemoryMirror->bHasScope
		&& !MemoryMirror->Scope.IsEquivalentTo(Packet->GetScope()))
	{
		FailMemoryMirror_RenderThread(ESightWeaveRenderAvailability::InvalidPacket);
		return false;
	}
	if (!CheckMemoryCapabilities_RenderThread())
	{
		FailMemoryMirror_RenderThread(MemoryMirror->Availability);
		return false;
	}
	if (!MemoryMirror->bHasScope)
	{
		MemoryMirror->Scope = Packet->GetScope();
		MemoryMirror->bHasScope = true;
	}

	TArray<FIntPoint> ChangedCoordinates;
	auto AddChanged = [&ChangedCoordinates](const FIntPoint Coordinate)
	{
		ChangedCoordinates.AddUnique(Coordinate);
	};

	if (bForceFullRebuild)
	{
		MemoryMirror->CachedTiles.Reset();
		MemoryMirror->Residency.Reset();
		++MemoryMirror->ResidencyGeneration;
		for (const FSightWeavePackedMemoryTile& Tile : Packet->GetAuthorityTiles())
		{
			if (!MemoryTileMatchesScope(Tile, MemoryMirror->Scope))
			{
				FailMemoryMirror_RenderThread(ESightWeaveRenderAvailability::InvalidPacket);
				return false;
			}
			MemoryMirror->CachedTiles.Add(Tile.Key.LogicalCoordinate, Tile);
			AddChanged(Tile.Key.LogicalCoordinate);
		}
	}
	else
	{
		for (const FSightWeaveMemoryTileKey& Removed : Packet->GetRemovedTiles())
		{
			if (!Removed.IsValid() || !Removed.Scope.IsEquivalentTo(MemoryMirror->Scope))
			{
				FailMemoryMirror_RenderThread(ESightWeaveRenderAvailability::InvalidPacket);
				return false;
			}
			MemoryMirror->CachedTiles.Remove(Removed.LogicalCoordinate);
			const FSightWeaveSparseTileIdentity Identity =
				MakeMemoryTileIdentity(MemoryMirror->Scope, Removed.LogicalCoordinate);
			if (MemoryMirror->Residency.Release(Identity))
			{
				++MemoryMirror->ResidencyGeneration;
			}
			AddChanged(Removed.LogicalCoordinate);
		}
		for (const FSightWeavePackedMemoryTile& Tile : Packet->GetDirtyTiles())
		{
			if (!MemoryTileMatchesScope(Tile, MemoryMirror->Scope))
			{
				FailMemoryMirror_RenderThread(ESightWeaveRenderAvailability::InvalidPacket);
				return false;
			}
			MemoryMirror->CachedTiles.Add(Tile.Key.LogicalCoordinate, Tile);
			AddChanged(Tile.Key.LogicalCoordinate);
		}
	}
	if (MemoryMirror->CachedTiles.Num()
		> SightWeave::SparseAtlas::StandardActiveTileCapacity)
	{
		FailMemoryMirror_RenderThread(ESightWeaveRenderAvailability::ResourceAllocationFailed);
		return false;
	}
	if (Packet->GetModifierRevision() != MemoryMirror->AppliedModifierRevision)
	{
		for (const TPair<FIntPoint, FSightWeavePackedMemoryTile>& Pair :
			MemoryMirror->CachedTiles)
		{
			AddChanged(Pair.Key);
		}
	}

	TArray<FIntPoint> UploadCoordinates;
	for (const FIntPoint Changed : ChangedCoordinates)
	{
		for (int32 Y = -1; Y <= 1; ++Y)
		{
			for (int32 X = -1; X <= 1; ++X)
			{
				const FIntPoint Candidate = Changed + FIntPoint(X, Y);
				if (MemoryMirror->CachedTiles.Contains(Candidate))
				{
					UploadCoordinates.AddUnique(Candidate);
				}
			}
		}
	}

	bool bUploaded = false;
	for (const FIntPoint Coordinate : UploadCoordinates)
	{
		const FSightWeaveSparseTileIdentity Identity =
			MakeMemoryTileIdentity(MemoryMirror->Scope, Coordinate);
		const FSightWeaveSparseResidencyResult ResidencyResult =
			MemoryMirror->Residency.Acquire(Identity, Packet->GetMemoryRevision());
		if (!ResidencyResult.Succeeded())
		{
			FailMemoryMirror_RenderThread(ESightWeaveRenderAvailability::ResourceAllocationFailed);
			return false;
		}
		if (ResidencyResult.Disposition == ESightWeaveSparseResidencyDisposition::Allocated
			|| ResidencyResult.Disposition == ESightWeaveSparseResidencyDisposition::Reused)
		{
			++MemoryMirror->ResidencyGeneration;
		}
		FRDGTextureRef Page = nullptr;
		bool bColdCreated = false;
		if (!EnsureMemoryPage_RenderThread(
				GraphBuilder,
				ResidencyResult.Address.PageIndex,
				Page,
				bColdCreated)
			|| !Page)
		{
			FailMemoryMirror_RenderThread(ESightWeaveRenderAvailability::ResourceAllocationFailed);
			return false;
		}

		TArray<uint8> UploadBytes;
		UploadBytes.SetNumZeroed(
			SightWeave::SparseAtlas::PhysicalTileSize
			* SightWeave::SparseAtlas::PhysicalTileSize);
		if (Packet->GetPresentationSuppressions().IsEmpty())
		{
			// The overwhelmingly common path has no presentation suppression. Expand
			// the authoritative packed interior a byte at a time, then resolve only
			// the four-texel gutter through logical neighbors. This preserves exact
			// point sampling without doing a TMap lookup for every interior texel.
			const FSightWeavePackedMemoryTile* CenterTile =
				MemoryMirror->CachedTiles.Find(Coordinate);
			check(CenterTile);
			for (int32 InteriorY = 0; InteriorY < SightWeave::Memory::InteriorTileSize; ++InteriorY)
			{
				const int32 SourceRow = InteriorY * SightWeave::Memory::RowBytes;
				const int32 DestinationRow =
					(InteriorY + SightWeave::SparseAtlas::GutterTexels)
					* SightWeave::SparseAtlas::PhysicalTileSize
					+ SightWeave::SparseAtlas::GutterTexels;
				for (int32 ByteX = 0; ByteX < SightWeave::Memory::RowBytes; ++ByteX)
				{
					const uint8 Packed = CenterTile->PackedBits[SourceRow + ByteX];
					const int32 Destination = DestinationRow + ByteX * 8;
					if (Packed == 0)
					{
						continue;
					}
					if (Packed == 255)
					{
						FMemory::Memset(UploadBytes.GetData() + Destination, 255, 8);
						continue;
					}
					for (int32 Bit = 0; Bit < 8; ++Bit)
					{
						UploadBytes[Destination + Bit] =
							(Packed & (1u << Bit)) != 0 ? 255 : 0;
					}
				}
			}
			auto SampleGutter = [this, Coordinate](const int32 PhysicalX, const int32 PhysicalY)
			{
				const int32 GlobalX = Coordinate.X * SightWeave::Memory::InteriorTileSize
					+ PhysicalX - SightWeave::SparseAtlas::GutterTexels;
				const int32 GlobalY = Coordinate.Y * SightWeave::Memory::InteriorTileSize
					+ PhysicalY - SightWeave::SparseAtlas::GutterTexels;
				const FIntPoint SourceCoordinate(
					FloorDivide(GlobalX, SightWeave::Memory::InteriorTileSize),
					FloorDivide(GlobalY, SightWeave::Memory::InteriorTileSize));
				const FSightWeavePackedMemoryTile* SourceTile =
					MemoryMirror->CachedTiles.Find(SourceCoordinate);
				return SourceTile && SourceTile->TestBit(FIntPoint(
					GlobalX - SourceCoordinate.X * SightWeave::Memory::InteriorTileSize,
					GlobalY - SourceCoordinate.Y * SightWeave::Memory::InteriorTileSize));
			};
			for (int32 PhysicalY = 0; PhysicalY < SightWeave::SparseAtlas::GutterTexels; ++PhysicalY)
			{
				for (int32 PhysicalX = 0; PhysicalX < SightWeave::SparseAtlas::PhysicalTileSize; ++PhysicalX)
				{
					UploadBytes[PhysicalY * SightWeave::SparseAtlas::PhysicalTileSize + PhysicalX] =
						SampleGutter(PhysicalX, PhysicalY) ? 255 : 0;
					const int32 BottomY = SightWeave::SparseAtlas::PhysicalTileSize
						- SightWeave::SparseAtlas::GutterTexels + PhysicalY;
					UploadBytes[BottomY * SightWeave::SparseAtlas::PhysicalTileSize + PhysicalX] =
						SampleGutter(PhysicalX, BottomY) ? 255 : 0;
				}
			}
			for (int32 PhysicalY = SightWeave::SparseAtlas::GutterTexels;
				PhysicalY < SightWeave::SparseAtlas::PhysicalTileSize - SightWeave::SparseAtlas::GutterTexels;
				++PhysicalY)
			{
				for (int32 PhysicalX = 0; PhysicalX < SightWeave::SparseAtlas::GutterTexels; ++PhysicalX)
				{
					UploadBytes[PhysicalY * SightWeave::SparseAtlas::PhysicalTileSize + PhysicalX] =
						SampleGutter(PhysicalX, PhysicalY) ? 255 : 0;
					const int32 RightX = SightWeave::SparseAtlas::PhysicalTileSize
						- SightWeave::SparseAtlas::GutterTexels + PhysicalX;
					UploadBytes[PhysicalY * SightWeave::SparseAtlas::PhysicalTileSize + RightX] =
						SampleGutter(RightX, PhysicalY) ? 255 : 0;
				}
			}
		}
		else
		{
			for (int32 PhysicalY = 0; PhysicalY < SightWeave::SparseAtlas::PhysicalTileSize; ++PhysicalY)
			{
				for (int32 PhysicalX = 0; PhysicalX < SightWeave::SparseAtlas::PhysicalTileSize; ++PhysicalX)
				{
					const int32 GlobalX = Coordinate.X * SightWeave::Memory::InteriorTileSize
						+ PhysicalX - SightWeave::SparseAtlas::GutterTexels;
					const int32 GlobalY = Coordinate.Y * SightWeave::Memory::InteriorTileSize
						+ PhysicalY - SightWeave::SparseAtlas::GutterTexels;
					const FIntPoint SourceCoordinate(
						FloorDivide(GlobalX, SightWeave::Memory::InteriorTileSize),
						FloorDivide(GlobalY, SightWeave::Memory::InteriorTileSize));
					const FSightWeavePackedMemoryTile* SourceTile =
						MemoryMirror->CachedTiles.Find(SourceCoordinate);
					if (!SourceTile)
					{
						continue;
					}
					const FIntPoint SourceTexel(
						GlobalX - SourceCoordinate.X * SightWeave::Memory::InteriorTileSize,
						GlobalY - SourceCoordinate.Y * SightWeave::Memory::InteriorTileSize);
					bool bPresented = SourceTile->TestBit(SourceTexel);
					if (bPresented)
					{
						const float CentimetersPerTexel =
							SightWeaveCentimetersPerTexel(MemoryMirror->Scope.PrecisionTier);
						const FVector WorldLocation(
							MemoryMirror->Scope.FloorOrigin.X
								+ (static_cast<double>(GlobalX) + 0.5) * CentimetersPerTexel,
							MemoryMirror->Scope.FloorOrigin.Y
								+ (static_cast<double>(GlobalY) + 0.5) * CentimetersPerTexel,
							MemoryMirror->Scope.FloorPlaneZ);
						bPresented = !Packet->GetPresentationSuppressions().ContainsByPredicate(
							[WorldLocation](const FSightWeaveMemoryModifierDescription& Modifier)
							{
								return Modifier.Region.ContainsWorldLocation(WorldLocation);
							});
					}
					UploadBytes[PhysicalY * SightWeave::SparseAtlas::PhysicalTileSize + PhysicalX] =
						bPresented ? 255 : 0;
				}
			}
		}

		const FIntRect SlotRect = ResidencyResult.Address.GetSlotRect();
		FSightWeaveMemoryUploadPassParameters* Parameters =
			GraphBuilder.AllocParameters<FSightWeaveMemoryUploadPassParameters>();
		Parameters->Texture = Page;
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("SightWeave.Memory.Upload"),
			Parameters,
			ERDGPassFlags::Copy,
			[Page, SlotRect, UploadBytes = MoveTemp(UploadBytes)](
				FRDGAsyncTask,
				FRHICommandList& RHICmdList)
			{
				const FUpdateTextureRegion2D Region(
					SlotRect.Min.X,
					SlotRect.Min.Y,
					0,
					0,
					SightWeave::SparseAtlas::PhysicalTileSize,
					SightWeave::SparseAtlas::PhysicalTileSize);
				RHICmdList.UpdateTexture2D(
					Page->GetRHI(),
					0,
					Region,
					SightWeave::SparseAtlas::PhysicalTileSize,
					UploadBytes.GetData());
			});
		MemoryMirror->Residency.MarkApplied(
			ResidencyResult.Address,
			Packet->GetMemoryRevision());
		++MemoryMirror->UploadCount;
		bUploaded = true;
	}

	MemoryMirror->AppliedPacket = Packet;
	MemoryMirror->AppliedPacketRevision = Packet->GetPacketRevision();
	MemoryMirror->AppliedMemoryRevision = Packet->GetMemoryRevision();
	MemoryMirror->AppliedModifierRevision = Packet->GetModifierRevision();
	MemoryMirror->Availability = ESightWeaveRenderAvailability::Available;
	return bUploaded;
}

void FSightWeaveSparseAtlasRenderState::SubmitStaticEnvironmentPacket_RenderThread(
	const TSharedPtr<const FSightWeaveStaticEnvironmentPacket, ESPMode::ThreadSafe>& Packet)
{
	check(IsInRenderingThread());
	if (bReleased || !StaticAttributeMirror.IsValid())
	{
		return;
	}
	if (!Packet.IsValid())
	{
		StaticAttributeMirror->PendingPacket.Reset();
		StaticAttributeMirror->Availability = ESightWeaveRenderAvailability::Unknown;
		return;
	}
	if (Packet->GetPacketRevision() <= StaticAttributeMirror->DesiredPacketRevision)
	{
		return;
	}
	StaticAttributeMirror->DesiredPacketRevision = Packet->GetPacketRevision();
	StaticAttributeMirror->PendingPacket = Packet;
}

bool FSightWeaveSparseAtlasRenderState::ProcessStaticEnvironmentPending_RenderThread(
	FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());
	if (bReleased
		|| !StaticAttributeMirror.IsValid()
		|| !StaticAttributeMirror->PendingPacket.IsValid())
	{
		return false;
	}
	const TSharedPtr<const FSightWeaveStaticEnvironmentPacket, ESPMode::ThreadSafe> Packet =
		MoveTemp(StaticAttributeMirror->PendingPacket);
	if (!Packet->IsValid()
		|| !Packet->GetScope().IsValid()
		|| Packet->GetTiles().Num() > SightWeave::StaticEnvironment::DefaultMaximumTiles)
	{
		FailStaticEnvironmentMirror_RenderThread(ESightWeaveRenderAvailability::InvalidPacket);
		return false;
	}
	if (StaticAttributeMirror->bHasScope
		&& !StaticAttributeMirror->Scope.IsEquivalentTo(Packet->GetScope()))
	{
		FailStaticEnvironmentMirror_RenderThread(ESightWeaveRenderAvailability::InvalidPacket);
		return false;
	}
	if (GUsingNullRHI || !FApp::CanEverRender())
	{
		FailStaticEnvironmentMirror_RenderThread(ESightWeaveRenderAvailability::NullRHI);
		return false;
	}
	if (GMaxRHIShaderPlatform != SP_PCD3D_SM6)
	{
		FailStaticEnvironmentMirror_RenderThread(ESightWeaveRenderAvailability::UnsupportedRHI);
		return false;
	}
	constexpr EPixelFormatCapabilities RequiredCapabilities =
		EPixelFormatCapabilities::Texture2D
		| EPixelFormatCapabilities::TextureLoad
		| EPixelFormatCapabilities::TextureSample;
	if (!GPixelFormats[PF_G8].Supported
		|| !RHIPixelFormatHasCapabilities(PF_G8, RequiredCapabilities))
	{
		FailStaticEnvironmentMirror_RenderThread(
			ESightWeaveRenderAvailability::UnsupportedPixelFormat);
		return false;
	}
	if (StaticAttributeMirror->AppliedPacket.IsValid()
		&& StaticAttributeMirror->AppliedEligibilityRevision
			== Packet->GetEligibilityRevision())
	{
		StaticAttributeMirror->AppliedPacket = Packet;
		StaticAttributeMirror->AppliedPacketRevision = Packet->GetPacketRevision();
		StaticAttributeMirror->Availability = ESightWeaveRenderAvailability::Available;
		return false;
	}

	StaticAttributeMirror->Scope = Packet->GetScope();
	StaticAttributeMirror->bHasScope = true;
	StaticAttributeMirror->CachedTiles.Reset();
	StaticAttributeMirror->Residency.Reset();
	++StaticAttributeMirror->ResidencyGeneration;
	for (const FSightWeaveStaticEnvironmentTile& Tile : Packet->GetTiles())
	{
		if (!Tile.IsValid() || !Tile.Key.Scope.IsEquivalentTo(StaticAttributeMirror->Scope))
		{
			FailStaticEnvironmentMirror_RenderThread(ESightWeaveRenderAvailability::InvalidPacket);
			return false;
		}
		StaticAttributeMirror->CachedTiles.Add(Tile.Key.LogicalCoordinate, Tile);
	}

	bool bUploaded = false;
	for (const TPair<FIntPoint, FSightWeaveStaticEnvironmentTile>& Pair :
		StaticAttributeMirror->CachedTiles)
	{
		const FIntPoint Coordinate = Pair.Key;
		const FSightWeaveSparseTileIdentity Identity =
			MakeMemoryTileIdentity(StaticAttributeMirror->Scope, Coordinate);
		const uint64 ResidencyRevision = Packet->GetEligibilityRevision() + 1;
		const FSightWeaveSparseResidencyResult ResidencyResult =
			StaticAttributeMirror->Residency.Acquire(Identity, ResidencyRevision);
		if (!ResidencyResult.Succeeded())
		{
			FailStaticEnvironmentMirror_RenderThread(
				ESightWeaveRenderAvailability::ResourceAllocationFailed);
			return false;
		}
		if (ResidencyResult.Disposition == ESightWeaveSparseResidencyDisposition::Allocated
			|| ResidencyResult.Disposition == ESightWeaveSparseResidencyDisposition::Reused)
		{
			++StaticAttributeMirror->ResidencyGeneration;
		}
		FRDGTextureRef Page = nullptr;
		bool bColdCreated = false;
		if (!EnsureStaticEnvironmentPage_RenderThread(
				GraphBuilder,
				ResidencyResult.Address.PageIndex,
				Page,
				bColdCreated)
			|| !Page)
		{
			FailStaticEnvironmentMirror_RenderThread(
				ESightWeaveRenderAvailability::ResourceAllocationFailed);
			return false;
		}
		TArray<uint8> UploadBytes;
		UploadBytes.SetNumZeroed(
			SightWeave::SparseAtlas::PhysicalTileSize
			* SightWeave::SparseAtlas::PhysicalTileSize);
		for (int32 PhysicalY = 0; PhysicalY < SightWeave::SparseAtlas::PhysicalTileSize; ++PhysicalY)
		{
			for (int32 PhysicalX = 0; PhysicalX < SightWeave::SparseAtlas::PhysicalTileSize; ++PhysicalX)
			{
				const int32 GlobalX = Coordinate.X * SightWeave::Memory::InteriorTileSize
					+ PhysicalX - SightWeave::SparseAtlas::GutterTexels;
				const int32 GlobalY = Coordinate.Y * SightWeave::Memory::InteriorTileSize
					+ PhysicalY - SightWeave::SparseAtlas::GutterTexels;
				const FIntPoint SourceCoordinate(
					FloorDivide(GlobalX, SightWeave::Memory::InteriorTileSize),
					FloorDivide(GlobalY, SightWeave::Memory::InteriorTileSize));
				const FSightWeaveStaticEnvironmentTile* SourceTile =
					StaticAttributeMirror->CachedTiles.Find(SourceCoordinate);
				if (!SourceTile)
				{
					continue;
				}
				UploadBytes[
					PhysicalY * SightWeave::SparseAtlas::PhysicalTileSize + PhysicalX] =
					SourceTile->Sample(FIntPoint(
						GlobalX - SourceCoordinate.X * SightWeave::Memory::InteriorTileSize,
						GlobalY - SourceCoordinate.Y * SightWeave::Memory::InteriorTileSize));
			}
		}
		const FIntRect SlotRect = ResidencyResult.Address.GetSlotRect();
		FSightWeaveMemoryUploadPassParameters* Parameters =
			GraphBuilder.AllocParameters<FSightWeaveMemoryUploadPassParameters>();
		Parameters->Texture = Page;
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("SightWeave.StaticEnvironment.Upload"),
			Parameters,
			ERDGPassFlags::Copy,
			[Page, SlotRect, UploadBytes = MoveTemp(UploadBytes)](
				FRDGAsyncTask,
				FRHICommandList& RHICmdList)
			{
				const FUpdateTextureRegion2D Region(
					SlotRect.Min.X,
					SlotRect.Min.Y,
					0,
					0,
					SightWeave::SparseAtlas::PhysicalTileSize,
					SightWeave::SparseAtlas::PhysicalTileSize);
				RHICmdList.UpdateTexture2D(
					Page->GetRHI(),
					0,
					Region,
					SightWeave::SparseAtlas::PhysicalTileSize,
					UploadBytes.GetData());
			});
		StaticAttributeMirror->Residency.MarkApplied(
			ResidencyResult.Address,
			ResidencyRevision);
		++StaticAttributeMirror->UploadCount;
		bUploaded = true;
	}
	StaticAttributeMirror->AppliedPacket = Packet;
	StaticAttributeMirror->AppliedPacketRevision = Packet->GetPacketRevision();
	StaticAttributeMirror->AppliedEligibilityRevision = Packet->GetEligibilityRevision();
	StaticAttributeMirror->Availability = ESightWeaveRenderAvailability::Available;
	return bUploaded;
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
	const bool bForceFullRebuild = bPendingRequiresFullRebuild
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		|| CVarDiagnosticForceFullMaskRebuild.GetValueOnRenderThread() != 0
#endif
		;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const uint64 DirtyTileDispatchCountBefore = DirtyTileDispatchCount;
#endif
	bPendingForceBlack = false;
	bPendingRequiresFullRebuild = false;
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
	TBitArray<> DirtyTileMask(false, Packet->GetTiles().Num());
	auto ProcessDirtyTile = [this, &GraphBuilder, &Packet, &bAnyFeatherImpact, &bAnyMaskWork](
		const int32 DirtyTileIndex)
	{
		const FSightWeaveSparseRenderTile& Tile = Packet->GetTiles()[DirtyTileIndex];
		MarkFeatherDirtyAround_RenderThread(Tile.Identity.TileKey);
		bAnyFeatherImpact = true;
		FScopeState* Scope = FindScope_RenderThread(Tile.Identity.TileKey.Scope);
		if (!Scope
			|| Scope->Availability == ESightWeaveRenderAvailability::InvalidPacket
			|| Scope->Availability == ESightWeaveRenderAvailability::ResourceAllocationFailed)
		{
			return;
		}
		const FSightWeaveSparseResidencyResult ResidencyResult =
			Scope->Residency.Acquire(Tile.Identity, Packet->GetPacketRevision());
		if (!ResidencyResult.Succeeded())
		{
			FailScope_RenderThread(*Scope, ESightWeaveRenderAvailability::ResourceAllocationFailed);
			return;
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
			return;
		}
		if (!Page)
		{
			return;
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
	};
	if (bForceFullRebuild)
	{
		for (int32 TileIndex = 0; TileIndex < Packet->GetTiles().Num(); ++TileIndex)
		{
			DirtyTileMask[TileIndex] = true;
			ProcessDirtyTile(TileIndex);
		}
	}
	else
	{
		for (const int32 DirtyTileIndex : Packet->GetDirtyTileIndices())
		{
			if (DirtyTileMask.IsValidIndex(DirtyTileIndex))
			{
				DirtyTileMask[DirtyTileIndex] = true;
			}
			ProcessDirtyTile(DirtyTileIndex);
		}
	}

	for (int32 TileIndex = 0; TileIndex < Packet->GetTiles().Num(); ++TileIndex)
	{
		if (DirtyTileMask[TileIndex])
		{
			continue;
		}
		const FSightWeaveSparseRenderTile& Tile = Packet->GetTiles()[TileIndex];
		FScopeState* Scope = FindScope_RenderThread(Tile.Identity.TileKey.Scope);
		if (!Scope
			|| Scope->Availability == ESightWeaveRenderAvailability::InvalidPacket
			|| Scope->Availability == ESightWeaveRenderAvailability::ResourceAllocationFailed)
		{
			continue;
		}
		const FSightWeaveSparseResidencySlot* Slot = Scope->Residency.Find(Tile.Identity);
		if (!Slot || !Scope->Residency.MarkApplied(Slot->Address, Packet->GetPacketRevision()))
		{
			FailScope_RenderThread(*Scope, ESightWeaveRenderAvailability::InvalidPacket);
		}
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
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	LastMaskUpdateFrame = GFrameNumberRenderThread;
	LastSubmittedTileCount = static_cast<uint32>(FMath::Min<uint64>(
		DirtyTileDispatchCount - DirtyTileDispatchCountBefore,
		MAX_uint32));
	bLastMaskUpdateWasFullRebuild = bForceFullRebuild;
#endif
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

void FSightWeaveSparseAtlasRenderState::PrepareMemoryPresentationResources_RenderThread(
	FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());
	if (!MemoryMirror.IsValid())
	{
		return;
	}
	MemoryMirror->CurrentPageTable = nullptr;
	if (MemoryMirror->Availability == ESightWeaveRenderAvailability::Available)
	{
		PrepareMemoryPageTable_RenderThread(GraphBuilder);
	}
}

void FSightWeaveSparseAtlasRenderState::PrepareStaticEnvironmentPresentationResources_RenderThread(
	FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());
	if (!StaticAttributeMirror.IsValid())
	{
		return;
	}
	StaticAttributeMirror->CurrentPageTable = nullptr;
	if (StaticAttributeMirror->Availability == ESightWeaveRenderAvailability::Available)
	{
		PrepareStaticEnvironmentPageTable_RenderThread(GraphBuilder);
	}
}

bool FSightWeaveSparseAtlasRenderState::PrepareStaticEnvironmentPageTable_RenderThread(
	FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());
	if (!StaticAttributeMirror.IsValid())
	{
		return false;
	}
	if (StaticAttributeMirror->PageTable.IsValid()
		&& StaticAttributeMirror->PageTableResidencyGeneration
			== StaticAttributeMirror->ResidencyGeneration
		&& StaticAttributeMirror->PageTableEligibilityRevision
			== StaticAttributeMirror->AppliedEligibilityRevision)
	{
		StaticAttributeMirror->CurrentPageTable = GraphBuilder.RegisterExternalBuffer(
			StaticAttributeMirror->PageTable,
			TEXT("SightWeave.StaticEnvironment.PageTable"));
		return StaticAttributeMirror->CurrentPageTable != nullptr;
	}
	TArray<FIntVector4> Entries;
	const uint64 ResidencyRevision =
		StaticAttributeMirror->AppliedEligibilityRevision + 1;
	for (const FSightWeaveSparseResidencySlot& Slot :
		StaticAttributeMirror->Residency.GetSlots())
	{
		if (Slot.bOccupied && Slot.AppliedRevision == ResidencyRevision)
		{
			Entries.Emplace(
				Slot.Identity.TileKey.LogicalCoordinate.X,
				Slot.Identity.TileKey.LogicalCoordinate.Y,
				Slot.Address.PageIndex,
				Slot.Address.SlotIndex);
		}
	}
	Algo::Sort(Entries, [](const FIntVector4& A, const FIntVector4& B)
	{
		return A.X < B.X || (A.X == B.X && A.Y < B.Y);
	});
	StaticAttributeMirror->PageTableEntryCount = Entries.Num();
	if (Entries.IsEmpty())
	{
		Entries.Emplace(0, 0, INDEX_NONE, INDEX_NONE);
	}
	StaticAttributeMirror->CurrentPageTable = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("SightWeave.StaticEnvironment.PageTable"),
		MoveTemp(Entries));
	GraphBuilder.QueueBufferExtraction(
		StaticAttributeMirror->CurrentPageTable,
		&StaticAttributeMirror->PageTable);
	StaticAttributeMirror->PageTableResidencyGeneration =
		StaticAttributeMirror->ResidencyGeneration;
	StaticAttributeMirror->PageTableEligibilityRevision =
		StaticAttributeMirror->AppliedEligibilityRevision;
	++StaticAttributeMirror->PageTableUploadCount;
	return true;
}

bool FSightWeaveSparseAtlasRenderState::PrepareMemoryPageTable_RenderThread(
	FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());
	if (!MemoryMirror.IsValid())
	{
		return false;
	}
	if (MemoryMirror->PageTable.IsValid()
		&& MemoryMirror->PageTableResidencyGeneration == MemoryMirror->ResidencyGeneration
		&& MemoryMirror->PageTableMemoryRevision == MemoryMirror->AppliedMemoryRevision)
	{
		MemoryMirror->CurrentPageTable = GraphBuilder.RegisterExternalBuffer(
			MemoryMirror->PageTable,
			TEXT("SightWeave.Memory.PageTable"));
		return MemoryMirror->CurrentPageTable != nullptr;
	}

	TArray<FIntVector4> Entries;
	Entries.Reserve(MemoryMirror->Residency.GetResidentCount());
	for (const FSightWeaveSparseResidencySlot& Slot : MemoryMirror->Residency.GetSlots())
	{
		if (!Slot.bOccupied || Slot.AppliedRevision != MemoryMirror->AppliedMemoryRevision)
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
	MemoryMirror->PageTableEntryCount = Entries.Num();
	if (Entries.IsEmpty())
	{
		Entries.Emplace(0, 0, INDEX_NONE, INDEX_NONE);
	}
	MemoryMirror->CurrentPageTable = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("SightWeave.Memory.PageTable"),
		MoveTemp(Entries));
	GraphBuilder.QueueBufferExtraction(
		MemoryMirror->CurrentPageTable,
		&MemoryMirror->PageTable);
	MemoryMirror->PageTableResidencyGeneration = MemoryMirror->ResidencyGeneration;
	MemoryMirror->PageTableMemoryRevision = MemoryMirror->AppliedMemoryRevision;
	++MemoryMirror->PageTableUploadCount;
	return true;
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
	const FPostProcessMaterialInputs& Inputs,
	const bool bPreTemporalUpscaleProof)
{
	check(IsInRenderingThread());
	const FScreenPassTexture SceneColor = FScreenPassTexture::CopyFromSlice(
		GraphBuilder,
		Inputs.GetInput(EPostProcessMaterialInput::SceneColor));
	check(SceneColor.IsValid());
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const int32 RequestedDiagnosticMode = FMath::Clamp(
		CVarDiagnosticCompositeMode.GetValueOnRenderThread(), 0, 15);
	const int32 DiagnosticMode = bPreTemporalUpscaleProof && RequestedDiagnosticMode == 0
		? 15
		: RequestedDiagnosticMode;
	if (DiagnosticMode == 1)
	{
		return SceneColor;
	}
	const bool bUseStableDepthCoordinates =
		CVarDiagnosticStableDepthCoordinates.GetValueOnRenderThread() != 0;
#else
	check(!bPreTemporalUpscaleProof);
	constexpr int32 DiagnosticMode = 0;
	constexpr bool bUseStableDepthCoordinates = true;
#endif

	FScreenPassRenderTarget Output = Inputs.OverrideOutput;
	if (!Output.IsValid())
	{
		Output = FScreenPassRenderTarget::CreateFromInput(
			GraphBuilder,
			SceneColor,
			View.GetOverwriteLoadAction(),
			TEXT("SightWeave.HardMask.Output"));
	}
	auto FailBlack = [this, &GraphBuilder, &Output](
		const int32 DiagnosticCode,
		const TCHAR* DiagnosticName,
		const FScopeState* Scope = nullptr) -> FScreenPassTexture
	{
		ReportCompositeDiagnostic_RenderThread(DiagnosticCode, DiagnosticName, Scope);
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
		return FailBlack(101, TEXT("fail-binding"));
	}
	FScopeState* Scope = FindScope_RenderThread(Binding->GetScopeKey());
	if (!Scope
		|| Scope->Availability != ESightWeaveRenderAvailability::Available
		|| Scope->AppliedRevision != Binding->GetPacketRevision()
		|| Scope->PageTableResidencyGeneration != Binding->GetResidencyGeneration()
		|| Scope->PageTablePacketRevision != Binding->GetPacketRevision()
		|| !Scope->CurrentPageTable)
	{
		return FailBlack(102, TEXT("fail-scope-page-table"), Scope);
	}
	for (const FSightWeaveSparseResidencySlot& Slot : Scope->Residency.GetSlots())
	{
		if (Slot.bOccupied && Slot.Address.PageIndex >= 4)
		{
			return FailBlack(103, TEXT("fail-page-index"), Scope);
		}
	}
	if (!Inputs.SceneTextures.SceneTextures)
	{
		return FailBlack(104, TEXT("fail-scene-textures"), Scope);
	}
	FRDGTextureRef SceneDepth =
		Inputs.SceneTextures.SceneTextures->GetParameters()->SceneDepthTexture;
	if (!SceneDepth)
	{
		return FailBlack(105, TEXT("fail-scene-depth"), Scope);
	}
	FRDGTextureRef SubjectProxyDepth =
		Inputs.SceneTextures.SceneTextures->GetParameters()->CustomDepthTexture;
	FRDGTextureSRVRef SubjectProxyStencil =
		Inputs.SceneTextures.SceneTextures->GetParameters()->CustomStencilTexture;
	if (!SubjectProxyDepth || !SubjectProxyStencil)
	{
		return FailBlack(108, TEXT("fail-subject-proxy-depth-stencil"), Scope);
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
	const FVector2D TemporalProjectionJitter = View.ViewMatrices.GetTemporalAAJitter();
	const FVector2f TemporalProjectionJitterFloat(TemporalProjectionJitter);
	const FVector2f TemporalJitterPixels(
		static_cast<float>(TemporalProjectionJitter.X * View.UnscaledViewRect.Width() * 0.5),
		static_cast<float>(TemporalProjectionJitter.Y * View.UnscaledViewRect.Height() * -0.5));
	const bool bMemoryReady = MemoryMirror.IsValid()
		&& MemoryMirror->Availability == ESightWeaveRenderAvailability::Available
		&& MemoryMirror->AppliedPacket.IsValid()
		&& MemoryMirror->CurrentPageTable
		&& MemoryScopeMatchesPresentationBinding(MemoryMirror->Scope, *Binding);
	const bool bStaticEnvironmentReady = bMemoryReady
		&& StaticAttributeMirror.IsValid()
		&& StaticAttributeMirror->Availability == ESightWeaveRenderAvailability::Available
		&& StaticAttributeMirror->AppliedPacket.IsValid()
		&& StaticAttributeMirror->CurrentPageTable
		&& StaticAttributeMirror->Scope.IsEquivalentTo(MemoryMirror->Scope);
	ReportMemoryPresentationDiagnostic_RenderThread(
		Binding->GetVisualFeather().IsEnabled() ? 2 : 1,
		bMemoryReady,
		bStaticEnvironmentReady,
		MemoryMirror.IsValid() && MemoryMirror->Scope.IsValid(),
		MemoryMirror.IsValid()
			&& MemoryScopeMatchesPresentationBinding(MemoryMirror->Scope, *Binding),
		MemoryMirror.IsValid()
			&& StaticAttributeMirror.IsValid()
			&& StaticAttributeMirror->Scope.IsEquivalentTo(MemoryMirror->Scope));
	FRDGBufferRef MemoryPageTable = bMemoryReady
		? MemoryMirror->CurrentPageTable
		: Scope->CurrentPageTable;
	FRDGBufferRef StaticAttributePageTable = bStaticEnvironmentReady
		? StaticAttributeMirror->CurrentPageTable
		: Scope->CurrentPageTable;
	FRDGTextureRef MemoryPages[2] = { DummyPage, DummyPage };
	FRDGTextureRef StaticAttributePages[2] = { DummyPage, DummyPage };
	if (bMemoryReady)
	{
		for (int32 PageIndex = 0;
			PageIndex < MemoryMirror->Pages.Num() && PageIndex < 2;
			++PageIndex)
		{
			if (MemoryMirror->Pages[PageIndex].IsValid())
			{
				MemoryPages[PageIndex] = GraphBuilder.RegisterExternalTexture(
					MemoryMirror->Pages[PageIndex],
					TEXT("SightWeave.Memory.CompositePage"));
			}
		}
	}
	if (bStaticEnvironmentReady)
	{
		for (int32 PageIndex = 0;
			PageIndex < StaticAttributeMirror->Pages.Num() && PageIndex < 2;
			++PageIndex)
		{
			if (StaticAttributeMirror->Pages[PageIndex].IsValid())
			{
				StaticAttributePages[PageIndex] = GraphBuilder.RegisterExternalTexture(
					StaticAttributeMirror->Pages[PageIndex],
					TEXT("SightWeave.StaticEnvironment.CompositePage"));
			}
		}
	}
	const FVector2D MemoryTranslatedFloorOrigin = bMemoryReady
		? MemoryMirror->Scope.FloorOrigin
			+ FVector2D(PreViewTranslation.X, PreViewTranslation.Y)
		: FVector2D::ZeroVector;
	const float MemoryTranslatedFloorPlaneZ = bMemoryReady
		? MemoryMirror->Scope.FloorPlaneZ + PreViewTranslation.Z
		: 0.0f;
	const float MemoryCentimetersPerTexel = bMemoryReady
		? SightWeaveCentimetersPerTexel(MemoryMirror->Scope.PrecisionTier)
		: 1.0f;
	auto BindMemoryPresentation = [this,
		&GraphBuilder,
		MemoryPageTable,
		StaticAttributePageTable,
		&MemoryPages,
		&StaticAttributePages,
		MemoryTranslatedFloorOrigin,
		MemoryTranslatedFloorPlaneZ,
		MemoryCentimetersPerTexel,
		bMemoryReady,
		bStaticEnvironmentReady,
		TemporalProjectionJitterFloat,
		DiagnosticMode,
		bUseStableDepthCoordinates,
		bPreTemporalUpscaleProof](auto* Parameters)
	{
		Parameters->TemporalProjectionJitter = TemporalProjectionJitterFloat;
		Parameters->DiagnosticMode = static_cast<uint32>(DiagnosticMode);
		Parameters->UseStableDepthCoordinates = bUseStableDepthCoordinates ? 1u : 0u;
		Parameters->PreTemporalUpscaleProof = bPreTemporalUpscaleProof ? 1u : 0u;
		Parameters->MemoryPageTable = GraphBuilder.CreateSRV(MemoryPageTable);
		Parameters->MemoryPage0 = MemoryPages[0];
		Parameters->MemoryPage1 = MemoryPages[1];
		Parameters->MemoryPageTableCount = bMemoryReady
			? static_cast<uint32>(MemoryMirror->PageTableEntryCount)
			: 0;
		Parameters->StaticAttributePageTable =
			GraphBuilder.CreateSRV(StaticAttributePageTable);
		Parameters->StaticAttributePage0 = StaticAttributePages[0];
		Parameters->StaticAttributePage1 = StaticAttributePages[1];
		Parameters->StaticAttributePageTableCount = bStaticEnvironmentReady
			? static_cast<uint32>(StaticAttributeMirror->PageTableEntryCount)
			: 0;
		Parameters->MemoryTranslatedFloorOrigin = FVector2f(MemoryTranslatedFloorOrigin);
		Parameters->MemoryTranslatedFloorPlaneZ = MemoryTranslatedFloorPlaneZ;
		Parameters->MemoryCentimetersPerTexel = MemoryCentimetersPerTexel;
		Parameters->MemoryPresentationAvailable =
			bMemoryReady && bStaticEnvironmentReady ? 1u : 0u;
		Parameters->StaticEnvironmentStencilValue =
			SightWeave::RememberedScene::StaticEnvironmentStencilValue;
		Parameters->OccluderSurfaceStencilValue =
			SightWeave::RememberedScene::OccluderSurfaceStencilValue;
		Parameters->RememberedBrightness = FMath::Clamp(
			CVarRememberedBrightness.GetValueOnRenderThread(), 0.0f, 1.0f);
		Parameters->RememberedContrast = FMath::Clamp(
			CVarRememberedContrast.GetValueOnRenderThread(), 0.0f, 1.0f);
		Parameters->RememberedDetailStrength = FMath::Clamp(
			CVarRememberedDetailStrength.GetValueOnRenderThread(), 0.0f, 0.25f);
		Parameters->RememberedDetailWorldScale = FMath::Max(
			CVarRememberedDetailWorldScale.GetValueOnRenderThread(), 1.0f);
		Parameters->RememberedSurfaceDepthToleranceCentimeters = FMath::Max(
			CVarRememberedSurfaceDepthTolerance.GetValueOnRenderThread(), 0.1f);
		Parameters->OccluderSurfaceBiasCentimeters = DiagnosticMode == 8
			? 0.0f
			: FMath::Clamp(
				CVarOccluderSurfaceBias.GetValueOnRenderThread(), 0.0f, 25.0f);
	};
	const FVector2D TranslatedFloorOrigin = Binding->GetScopeKey().FloorOrigin
		+ FVector2D(PreViewTranslation.X, PreViewTranslation.Y);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (CVarDiagnosticLogFrames.GetValueOnRenderThread() != 0)
	{
		const IConsoleVariable* CustomDepthTemporalAAJitter =
			IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepthTemporalAAJitter"));
		const IConsoleVariable* CustomDepthMode =
			IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth"));
		const int32 CustomDepthTemporalAAJitterValue = CustomDepthTemporalAAJitter
			? CustomDepthTemporalAAJitter->GetInt()
			: INDEX_NONE;
		const int32 CustomDepthModeValue = CustomDepthMode
			? CustomDepthMode->GetInt()
			: INDEX_NONE;
		const uint32 CustomDepthTemporalAAJitterSetBy = CustomDepthTemporalAAJitter
			? static_cast<uint32>(CustomDepthTemporalAAJitter->GetFlags())
				& static_cast<uint32>(ECVF_SetByMask)
			: 0u;
		const uint32 SubmittedTiles = LastMaskUpdateFrame == GFrameNumberRenderThread
			? LastSubmittedTileCount : 0;
		const TCHAR* UpdateMode = LastMaskUpdateFrame == GFrameNumberRenderThread
			? (bLastMaskUpdateWasFullRebuild ? TEXT("full") : TEXT("incremental"))
			: TEXT("none");
		const FVector ViewOrigin = View.ViewMatrices.GetViewOrigin();
		const FMatrix ViewProjection = View.ViewMatrices.GetWorldToClip();
		UE_LOG(
			LogSightWeaveRender,
			Display,
			TEXT("VisualRescue frame=%llu compositeStage=%s viewRect=(%d,%d %dx%d) sceneDepthExtent=(%d,%d) customDepthExtent=(%d,%d) output=(%d,%d %dx%d) sceneColor=(%d,%d %dx%d) sceneColorExtent=(%d,%d) jitterPixels=(%.4f,%.4f) customDepthTemporalAAJitter=%d customDepthTemporalAAJitterSetBy=0x%08x customDepthMode=%d viewOrigin=(%.3f,%.3f,%.3f) vp=(%.6f,%.6f,%.6f,%.6f,%.6f,%.6f) maskOrigin=(%.3f,%.3f) stateRevision=%llu featherRevision=%llu submittedTiles=%u bindingFailure=%d historyValid=0 update=%s staticClassVersion=%llu diagnosticMode=%d stableDepthCoordinates=%d"),
			GFrameNumberRenderThread,
			bPreTemporalUpscaleProof ? TEXT("pre-tsr-proof") : TEXT("post-tonemap-control"),
			View.UnscaledViewRect.Min.X, View.UnscaledViewRect.Min.Y,
			View.UnscaledViewRect.Width(), View.UnscaledViewRect.Height(),
			SceneDepth->Desc.Extent.X, SceneDepth->Desc.Extent.Y,
			SubjectProxyDepth->Desc.Extent.X, SubjectProxyDepth->Desc.Extent.Y,
			Output.ViewRect.Min.X, Output.ViewRect.Min.Y,
			Output.ViewRect.Width(), Output.ViewRect.Height(),
			SceneColor.ViewRect.Min.X, SceneColor.ViewRect.Min.Y,
			SceneColor.ViewRect.Width(), SceneColor.ViewRect.Height(),
			SceneColor.Texture->Desc.Extent.X, SceneColor.Texture->Desc.Extent.Y,
			TemporalJitterPixels.X, TemporalJitterPixels.Y,
			CustomDepthTemporalAAJitterValue,
			CustomDepthTemporalAAJitterSetBy,
			CustomDepthModeValue,
			ViewOrigin.X, ViewOrigin.Y, ViewOrigin.Z,
			ViewProjection.M[0][0], ViewProjection.M[0][1],
			ViewProjection.M[1][0], ViewProjection.M[1][1],
			ViewProjection.M[2][0], ViewProjection.M[2][1],
			Binding->GetScopeKey().FloorOrigin.X,
			Binding->GetScopeKey().FloorOrigin.Y,
			AppliedRevision,
			Scope->FeatherAppliedRevision,
			SubmittedTiles,
			static_cast<int32>(PresentationBindingFailure),
			UpdateMode,
			StaticAttributeMirror.IsValid()
				? StaticAttributeMirror->AppliedEligibilityRevision : 0,
			DiagnosticMode,
			bUseStableDepthCoordinates ? 1 : 0);
	}
#endif
	if (Binding->GetVisualFeather().IsEnabled())
	{
		if (bFeatherUpdateIncomplete
			|| Binding->GetFeatherResourceGeneration() != Scope->FeatherResourceGeneration
			|| Binding->GetFeatherAppliedRevision() != AppliedRevision
			|| Binding->GetFeatherSettingsRevision() != Binding->GetPresentationRevision())
		{
			return FailBlack(106, TEXT("fail-feather-revision"), Scope);
		}
		FRDGTextureRef FeatherPages[4] = { DummyPage, DummyPage, DummyPage, DummyPage };
		for (int32 PageIndex = 0; PageIndex < Scope->Pages.Num() && PageIndex < 4; ++PageIndex)
		{
			if (Scope->Pages[PageIndex].IsValid())
			{
				if (!Scope->FeatherPages.IsValidIndex(PageIndex)
					|| !Scope->FeatherPages[PageIndex].IsValid())
				{
					return FailBlack(107, TEXT("fail-feather-page"), Scope);
				}
				FeatherPages[PageIndex] = GraphBuilder.RegisterExternalTexture(
					Scope->FeatherPages[PageIndex],
					TEXT("SightWeave.Presentation.FeatherPage"));
			}
		}

		TShaderMapRef<FSightWeaveFullscreenVertexShader> VertexShader(
			GetGlobalShaderMap(View.GetFeatureLevel()));
		TShaderMapRef<FSightWeaveInwardFeatherCompositePixelShader> PixelShader(
			GetGlobalShaderMap(View.GetFeatureLevel()));
		FSightWeaveInwardFeatherCompositePixelShader::FParameters* Parameters =
			GraphBuilder.AllocParameters<FSightWeaveInwardFeatherCompositePixelShader::FParameters>();
		Parameters->View = View.ViewUniformBuffer;
		Parameters->SceneColorTexture = SceneColor.Texture;
		Parameters->SceneDepthTexture = SceneDepth;
		Parameters->SubjectProxyDepthTexture = SubjectProxyDepth;
		Parameters->SubjectProxyStencilTexture = SubjectProxyStencil;
		Parameters->SubjectProxyStencilValue =
			SightWeave::SubjectMemory::LastSeenProxyStencilValue;
		Parameters->SubjectProxyNeutralIntensity =
			SightWeave::SubjectMemory::LastSeenProxyNeutralIntensity;
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
		BindMemoryPresentation(Parameters);
		Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("SightWeave.InwardFeatherComposite"),
			Parameters,
			ERDGPassFlags::Raster,
			[Parameters, VertexShader, PixelShader, OutputRect = Output.ViewRect](
				FRDGAsyncTask,
				FRHICommandList& RHICmdList)
			{
				ConfigureViewport(RHICmdList, OutputRect);
				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
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
					PixelShader,
					PixelShader.GetPixelShader(),
					*Parameters);
				RHICmdList.SetStreamSource(0, nullptr, 0);
				RHICmdList.DrawPrimitive(0, 1, 1);
			});
		ReportCompositeDiagnostic_RenderThread(2, TEXT("submitted-feather"), Scope);
	}
	else
	{
		TShaderMapRef<FSightWeaveFullscreenVertexShader> VertexShader(
			GetGlobalShaderMap(View.GetFeatureLevel()));
		TShaderMapRef<FSightWeaveHardMaskCompositePixelShader> PixelShader(
			GetGlobalShaderMap(View.GetFeatureLevel()));
		FSightWeaveHardMaskCompositePixelShader::FParameters* Parameters =
			GraphBuilder.AllocParameters<FSightWeaveHardMaskCompositePixelShader::FParameters>();
		Parameters->View = View.ViewUniformBuffer;
		Parameters->SceneColorTexture = SceneColor.Texture;
		Parameters->SceneDepthTexture = SceneDepth;
		Parameters->SubjectProxyDepthTexture = SubjectProxyDepth;
		Parameters->SubjectProxyStencilTexture = SubjectProxyStencil;
		Parameters->SubjectProxyStencilValue =
			SightWeave::SubjectMemory::LastSeenProxyStencilValue;
		Parameters->SubjectProxyNeutralIntensity =
			SightWeave::SubjectMemory::LastSeenProxyNeutralIntensity;
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
		BindMemoryPresentation(Parameters);
		Parameters->RenderTargets[0] = Output.GetRenderTargetBinding();
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("SightWeave.HardMaskComposite"),
			Parameters,
			ERDGPassFlags::Raster,
			[Parameters, VertexShader, PixelShader, OutputRect = Output.ViewRect](
				FRDGAsyncTask,
				FRHICommandList& RHICmdList)
			{
				ConfigureViewport(RHICmdList, OutputRect);
				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
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
					PixelShader,
					PixelShader.GetPixelShader(),
					*Parameters);
				RHICmdList.SetStreamSource(0, nullptr, 0);
				RHICmdList.DrawPrimitive(0, 1, 1);
			});
		ReportCompositeDiagnostic_RenderThread(1, TEXT("submitted-hard"), Scope);
	}
	return MoveTemp(Output);
}

void FSightWeaveSparseAtlasRenderState::ReportCompositeDiagnostic_RenderThread(
	const int32 DiagnosticCode,
	const TCHAR* DiagnosticName,
	const FScopeState* Scope)
{
	check(IsInRenderingThread());
	if (LastCompositeDiagnosticCode == DiagnosticCode)
	{
		return;
	}
	LastCompositeDiagnosticCode = DiagnosticCode;

	const uint64 ScopeAppliedRevision = Scope ? Scope->AppliedRevision : 0;
	const int32 PageTableEntryCount = Scope ? Scope->PageTableEntryCount : 0;
	const int32 ResidentTileCount = Scope ? Scope->Residency.GetResidentCount() : 0;
	UE_LOG(
		LogSightWeaveRender,
		Warning,
		TEXT("Presentation composite state=%s bindingFailure=%d featherWidth=%.3f applied=%llu resourceGeneration=%llu residencyGeneration=%llu scopeApplied=%llu pageTableEntries=%d residentTiles=%d"),
		DiagnosticName,
		static_cast<int32>(PresentationBindingFailure),
		PresentationSelection.GetVisualFeather().WidthCentimeters,
		AppliedRevision,
		ResourceGeneration,
		ResidencyGeneration,
		ScopeAppliedRevision,
		PageTableEntryCount,
		ResidentTileCount);
}

bool FSightWeaveSparseAtlasRenderState::FMemoryPresentationDiagnosticSnapshot::IsEquivalentTo(
	const FMemoryPresentationDiagnosticSnapshot& Other) const
{
	return bInitialized == Other.bInitialized
		&& CompositeDiagnosticCode == Other.CompositeDiagnosticCode
		&& MemoryAvailability == Other.MemoryAvailability
		&& StaticEnvironmentAvailability == Other.StaticEnvironmentAvailability
		&& MemoryPacketRevision == Other.MemoryPacketRevision
		&& StaticPacketRevision == Other.StaticPacketRevision
		&& StaticEligibilityRevision == Other.StaticEligibilityRevision
		&& MemoryResourceGeneration == Other.MemoryResourceGeneration
		&& StaticResourceGeneration == Other.StaticResourceGeneration
		&& MemoryResidencyGeneration == Other.MemoryResidencyGeneration
		&& StaticResidencyGeneration == Other.StaticResidencyGeneration
		&& MemoryPageTableEntryCount == Other.MemoryPageTableEntryCount
		&& StaticPageTableEntryCount == Other.StaticPageTableEntryCount
		&& MemoryResidentTileCount == Other.MemoryResidentTileCount
		&& StaticResidentTileCount == Other.StaticResidentTileCount
		&& MemoryScopeMismatchMask == Other.MemoryScopeMismatchMask
		&& MemoryPrecisionTier == Other.MemoryPrecisionTier
		&& LivePrecisionTier == Other.LivePrecisionTier
		&& bMemoryReady == Other.bMemoryReady
		&& bStaticEnvironmentReady == Other.bStaticEnvironmentReady
		&& bMemoryPresentationAvailable == Other.bMemoryPresentationAvailable
		&& bMemoryScopeValid == Other.bMemoryScopeValid
		&& bMemoryScopeMatchesBinding == Other.bMemoryScopeMatchesBinding
		&& bStaticScopeMatchesMemory == Other.bStaticScopeMatchesMemory;
}

void FSightWeaveSparseAtlasRenderState::ReportMemoryPresentationDiagnostic_RenderThread(
	const int32 CompositeDiagnosticCode,
	const bool bMemoryReady,
	const bool bStaticEnvironmentReady,
	const bool bMemoryScopeValid,
	const bool bMemoryScopeMatchesBinding,
	const bool bStaticScopeMatchesMemory)
{
	check(IsInRenderingThread());
	FMemoryPresentationDiagnosticSnapshot Current;
	Current.CompositeDiagnosticCode = CompositeDiagnosticCode;
	Current.MemoryAvailability = MemoryMirror.IsValid()
		? MemoryMirror->Availability
		: ESightWeaveRenderAvailability::Unknown;
	Current.StaticEnvironmentAvailability = StaticAttributeMirror.IsValid()
		? StaticAttributeMirror->Availability
		: ESightWeaveRenderAvailability::Unknown;
	Current.MemoryPacketRevision = MemoryMirror.IsValid()
		? MemoryMirror->AppliedPacketRevision
		: 0;
	Current.StaticPacketRevision = StaticAttributeMirror.IsValid()
		? StaticAttributeMirror->AppliedPacketRevision
		: 0;
	Current.StaticEligibilityRevision = StaticAttributeMirror.IsValid()
		? StaticAttributeMirror->AppliedEligibilityRevision
		: 0;
	Current.MemoryResourceGeneration = MemoryMirror.IsValid()
		? MemoryMirror->ResourceGeneration
		: 0;
	Current.StaticResourceGeneration = StaticAttributeMirror.IsValid()
		? StaticAttributeMirror->ResourceGeneration
		: 0;
	Current.MemoryResidencyGeneration = MemoryMirror.IsValid()
		? MemoryMirror->ResidencyGeneration
		: 0;
	Current.StaticResidencyGeneration = StaticAttributeMirror.IsValid()
		? StaticAttributeMirror->ResidencyGeneration
		: 0;
	Current.MemoryPageTableEntryCount = MemoryMirror.IsValid()
		? MemoryMirror->PageTableEntryCount
		: 0;
	Current.StaticPageTableEntryCount = StaticAttributeMirror.IsValid()
		? StaticAttributeMirror->PageTableEntryCount
		: 0;
	Current.MemoryResidentTileCount = MemoryMirror.IsValid()
		? MemoryMirror->Residency.GetResidentCount()
		: 0;
	Current.StaticResidentTileCount = StaticAttributeMirror.IsValid()
		? StaticAttributeMirror->Residency.GetResidentCount()
		: 0;
	Current.MemoryPrecisionTier = MemoryMirror.IsValid()
		? MemoryMirror->Scope.PrecisionTier
		: ESightWeaveRenderPrecisionTier::Standard;
	Current.LivePrecisionTier = PresentationBinding.IsValid()
		? PresentationBinding->GetScopeKey().PrecisionTier
		: ESightWeaveRenderPrecisionTier::Standard;
	Current.bMemoryReady = bMemoryReady;
	Current.bStaticEnvironmentReady = bStaticEnvironmentReady;
	Current.bMemoryPresentationAvailable = bMemoryReady && bStaticEnvironmentReady;
	Current.bMemoryScopeValid = bMemoryScopeValid;
	Current.bMemoryScopeMatchesBinding = bMemoryScopeMatchesBinding;
	Current.bStaticScopeMatchesMemory = bStaticScopeMatchesMemory;
	Current.MemoryScopeMismatchMask = MemoryMirror.IsValid() && PresentationBinding.IsValid()
		? MemoryScopeMismatchMask(MemoryMirror->Scope, *PresentationBinding)
		: MAX_uint32;
	Current.bInitialized = true;
	if (LastMemoryPresentationDiagnostic.IsEquivalentTo(Current))
	{
		return;
	}
	LastMemoryPresentationDiagnostic = Current;

	UE_LOG(
		LogSightWeaveRender,
		Display,
		TEXT("Presentation memory state composite=%d memoryReady=%d staticEnvironmentReady=%d memoryPresentationAvailable=%d memoryAvailability=%d staticEnvironmentAvailability=%d memoryPacketRevision=%llu staticPacketRevision=%llu staticEligibilityRevision=%llu memoryPageTableEntries=%d staticPageTableEntries=%d memoryResidentTiles=%d staticResidentTiles=%d memoryResourceGeneration=%llu staticResourceGeneration=%llu memoryResidencyGeneration=%llu staticResidencyGeneration=%llu memoryScopeValid=%d memoryScopeMatchesBinding=%d memoryScopeMismatchMask=0x%02x staticScopeMatchesMemory=%d memoryPrecisionTier=%d livePrecisionTier=%d memoryWorld=%llu liveWorld=%llu memoryOwner=%s liveOwner=%s memoryFloor=%s liveFloor=%s memoryFloorOrigin=(%.3f,%.3f) liveFloorOrigin=(%.3f,%.3f) memoryProfiles=%d liveProfiles=%d"),
		Current.CompositeDiagnosticCode,
		Current.bMemoryReady,
		Current.bStaticEnvironmentReady,
		Current.bMemoryPresentationAvailable,
		static_cast<int32>(Current.MemoryAvailability),
		static_cast<int32>(Current.StaticEnvironmentAvailability),
		Current.MemoryPacketRevision,
		Current.StaticPacketRevision,
		Current.StaticEligibilityRevision,
		Current.MemoryPageTableEntryCount,
		Current.StaticPageTableEntryCount,
		Current.MemoryResidentTileCount,
		Current.StaticResidentTileCount,
		Current.MemoryResourceGeneration,
		Current.StaticResourceGeneration,
		Current.MemoryResidencyGeneration,
		Current.StaticResidencyGeneration,
		Current.bMemoryScopeValid,
		Current.bMemoryScopeMatchesBinding,
		Current.MemoryScopeMismatchMask,
		Current.bStaticScopeMatchesMemory,
		static_cast<int32>(Current.MemoryPrecisionTier),
		static_cast<int32>(Current.LivePrecisionTier),
		MemoryMirror.IsValid() ? MemoryMirror->Scope.WorldIdentity.Serial : 0,
		PresentationBinding.IsValid()
			? PresentationBinding->GetScopeKey().WorldIdentity.Serial
			: 0,
		MemoryMirror.IsValid()
			? *MemoryMirror->Scope.KnowledgeOwnerId.GetValue().ToString()
			: TEXT("None"),
		PresentationBinding.IsValid()
			? *PresentationBinding->GetScopeKey().KnowledgeOwnerId.GetValue().ToString()
			: TEXT("None"),
		MemoryMirror.IsValid()
			? *MemoryMirror->Scope.FloorId.GetValue().ToString()
			: TEXT("None"),
		PresentationBinding.IsValid()
			? *PresentationBinding->GetScopeKey().FloorId.GetValue().ToString()
			: TEXT("None"),
		MemoryMirror.IsValid() ? MemoryMirror->Scope.FloorOrigin.X : 0.0,
		MemoryMirror.IsValid() ? MemoryMirror->Scope.FloorOrigin.Y : 0.0,
		PresentationBinding.IsValid() ? PresentationBinding->GetScopeKey().FloorOrigin.X : 0.0,
		PresentationBinding.IsValid() ? PresentationBinding->GetScopeKey().FloorOrigin.Y : 0.0,
		MemoryMirror.IsValid() ? MemoryMirror->Scope.CanonicalProfiles.Num() : 0,
		PresentationBinding.IsValid() ? PresentationBinding->GetCanonicalProfiles().Num() : 0);
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
	bPendingRequiresFullRebuild = false;
	AppliedPacket.Reset();
	PresentationBinding.Reset();
	PresentationSelection = FSightWeaveViewPresentationSelection();
	PresentationBindingFailure = ESightWeavePresentationBindingFailure::WorldTeardown;
	Scopes.Reset();
	if (MemoryMirror.IsValid())
	{
		MemoryMirror->PendingPacket.Reset();
		MemoryMirror->AppliedPacket.Reset();
		MemoryMirror->CachedTiles.Reset();
		MemoryMirror->Residency.Reset();
		MemoryMirror->Pages.Reset();
		MemoryMirror->PageTable.SafeRelease();
		MemoryMirror->CurrentPageTable = nullptr;
		MemoryMirror->Availability = ESightWeaveRenderAvailability::WorldTeardown;
		++MemoryMirror->ResourceGeneration;
		++MemoryMirror->ResidencyGeneration;
	}
	if (StaticAttributeMirror.IsValid())
	{
		StaticAttributeMirror->PendingPacket.Reset();
		StaticAttributeMirror->AppliedPacket.Reset();
		StaticAttributeMirror->CachedTiles.Reset();
		StaticAttributeMirror->Residency.Reset();
		StaticAttributeMirror->Pages.Reset();
		StaticAttributeMirror->PageTable.SafeRelease();
		StaticAttributeMirror->CurrentPageTable = nullptr;
		StaticAttributeMirror->Availability = ESightWeaveRenderAvailability::WorldTeardown;
		++StaticAttributeMirror->ResourceGeneration;
		++StaticAttributeMirror->ResidencyGeneration;
	}
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

bool FSightWeaveSparseAtlasRenderState::CheckMemoryCapabilities_RenderThread()
{
	check(IsInRenderingThread());
	if (!MemoryMirror.IsValid())
	{
		return false;
	}
	if (GUsingNullRHI || !FApp::CanEverRender())
	{
		MemoryMirror->Availability = ESightWeaveRenderAvailability::NullRHI;
		return false;
	}
	if (GMaxRHIShaderPlatform != SP_PCD3D_SM6)
	{
		MemoryMirror->Availability = ESightWeaveRenderAvailability::UnsupportedRHI;
		return false;
	}
	constexpr EPixelFormatCapabilities RequiredCapabilities =
		EPixelFormatCapabilities::Texture2D
		| EPixelFormatCapabilities::TextureLoad
		| EPixelFormatCapabilities::TextureSample;
	if (!GPixelFormats[PF_G8].Supported
		|| !RHIPixelFormatHasCapabilities(PF_G8, RequiredCapabilities))
	{
		MemoryMirror->Availability = ESightWeaveRenderAvailability::UnsupportedPixelFormat;
		return false;
	}
	return true;
}

bool FSightWeaveSparseAtlasRenderState::EnsureMemoryPage_RenderThread(
	FRDGBuilder& GraphBuilder,
	const int32 PageIndex,
	FRDGTextureRef& OutPage,
	bool& bOutColdCreated)
{
	check(IsInRenderingThread());
	OutPage = nullptr;
	bOutColdCreated = false;
	if (!MemoryMirror.IsValid())
	{
		return false;
	}
	const int32 MaximumPages = FMath::DivideAndRoundUp(
		SightWeave::SparseAtlas::StandardActiveTileCapacity,
		SightWeave::SparseAtlas::SlotsPerPage);
	if (PageIndex < 0 || PageIndex >= MaximumPages)
	{
		return false;
	}
	if (MemoryMirror->Pages.Num() <= PageIndex)
	{
		MemoryMirror->Pages.SetNum(PageIndex + 1);
	}
	if (!MemoryMirror->Pages[PageIndex].IsValid())
	{
		AllocatePooledTexture(
			MakeMaskTextureDesc(SightWeave::SparseAtlas::PageSize),
			MemoryMirror->Pages[PageIndex],
			TEXT("SightWeave.Memory.Page"));
		if (!MemoryMirror->Pages[PageIndex].IsValid())
		{
			return false;
		}
		bOutColdCreated = true;
		++MemoryMirror->ResourceGeneration;
	}
	OutPage = GraphBuilder.RegisterExternalTexture(
		MemoryMirror->Pages[PageIndex],
		TEXT("SightWeave.Memory.Page"));
	if (bOutColdCreated)
	{
		AddClearRenderTargetPass(GraphBuilder, OutPage, FLinearColor::Black);
	}
	return OutPage != nullptr;
}

void FSightWeaveSparseAtlasRenderState::FailMemoryMirror_RenderThread(
	const ESightWeaveRenderAvailability Failure)
{
	check(IsInRenderingThread());
	if (!MemoryMirror.IsValid())
	{
		return;
	}
	const bool bReleasedResources = !MemoryMirror->Pages.IsEmpty()
		|| MemoryMirror->PageTable.IsValid();
	MemoryMirror->PendingPacket.Reset();
	MemoryMirror->AppliedPacket.Reset();
	MemoryMirror->CachedTiles.Reset();
	MemoryMirror->Residency.Reset();
	MemoryMirror->Pages.Reset();
	MemoryMirror->PageTable.SafeRelease();
	MemoryMirror->CurrentPageTable = nullptr;
	MemoryMirror->Scope = FSightWeaveMemoryScopeKey();
	MemoryMirror->bHasScope = false;
	MemoryMirror->AppliedPacketRevision = 0;
	MemoryMirror->AppliedMemoryRevision = 0;
	MemoryMirror->AppliedModifierRevision = 0;
	MemoryMirror->PageTableEntryCount = 0;
	MemoryMirror->Availability = Failure;
	++MemoryMirror->ResidencyGeneration;
	if (bReleasedResources)
	{
		++MemoryMirror->ResourceGeneration;
	}
}

bool FSightWeaveSparseAtlasRenderState::EnsureStaticEnvironmentPage_RenderThread(
	FRDGBuilder& GraphBuilder,
	const int32 PageIndex,
	FRDGTextureRef& OutPage,
	bool& bOutColdCreated)
{
	check(IsInRenderingThread());
	OutPage = nullptr;
	bOutColdCreated = false;
	if (!StaticAttributeMirror.IsValid())
	{
		return false;
	}
	const int32 MaximumPages = FMath::DivideAndRoundUp(
		SightWeave::StaticEnvironment::DefaultMaximumTiles,
		SightWeave::SparseAtlas::SlotsPerPage);
	if (PageIndex < 0 || PageIndex >= MaximumPages)
	{
		return false;
	}
	if (StaticAttributeMirror->Pages.Num() <= PageIndex)
	{
		StaticAttributeMirror->Pages.SetNum(PageIndex + 1);
	}
	if (!StaticAttributeMirror->Pages[PageIndex].IsValid())
	{
		AllocatePooledTexture(
			MakeMaskTextureDesc(SightWeave::SparseAtlas::PageSize),
			StaticAttributeMirror->Pages[PageIndex],
			TEXT("SightWeave.StaticEnvironment.Page"));
		if (!StaticAttributeMirror->Pages[PageIndex].IsValid())
		{
			return false;
		}
		bOutColdCreated = true;
		++StaticAttributeMirror->ResourceGeneration;
	}
	OutPage = GraphBuilder.RegisterExternalTexture(
		StaticAttributeMirror->Pages[PageIndex],
		TEXT("SightWeave.StaticEnvironment.Page"));
	if (bOutColdCreated)
	{
		AddClearRenderTargetPass(GraphBuilder, OutPage, FLinearColor::Black);
	}
	return OutPage != nullptr;
}

void FSightWeaveSparseAtlasRenderState::FailStaticEnvironmentMirror_RenderThread(
	const ESightWeaveRenderAvailability Failure)
{
	check(IsInRenderingThread());
	if (!StaticAttributeMirror.IsValid())
	{
		return;
	}
	const bool bReleasedResources = !StaticAttributeMirror->Pages.IsEmpty()
		|| StaticAttributeMirror->PageTable.IsValid();
	StaticAttributeMirror->PendingPacket.Reset();
	StaticAttributeMirror->AppliedPacket.Reset();
	StaticAttributeMirror->CachedTiles.Reset();
	StaticAttributeMirror->Residency.Reset();
	StaticAttributeMirror->Pages.Reset();
	StaticAttributeMirror->PageTable.SafeRelease();
	StaticAttributeMirror->CurrentPageTable = nullptr;
	StaticAttributeMirror->Scope = FSightWeaveMemoryScopeKey();
	StaticAttributeMirror->bHasScope = false;
	StaticAttributeMirror->AppliedPacketRevision = 0;
	StaticAttributeMirror->AppliedEligibilityRevision = 0;
	StaticAttributeMirror->PageTableEntryCount = 0;
	StaticAttributeMirror->Availability = Failure;
	++StaticAttributeMirror->ResidencyGeneration;
	if (bReleasedResources)
	{
		++StaticAttributeMirror->ResourceGeneration;
	}
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
	const int32 FeatherRadiusTexels = FMath::Clamp(
		FMath::CeilToInt(
			PresentationSelection.GetVisualFeather().WidthCentimeters
			/ Tile.CentimetersPerTexel),
		1,
		SightWeave::VisualFeather::MaximumRadiusTexels);
	const int32 FirstJumpStep = FMath::RoundUpToPowerOfTwo(FeatherRadiusTexels);
	for (int32 JumpStep = FirstJumpStep; JumpStep >= 1; JumpStep >>= 1)
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

int32 FSightWeaveSparseAtlasRenderState::GetAllocatedFeatherPageCount_RenderThread() const
{
	int32 Count = 0;
	for (const TUniquePtr<FScopeState>& Scope : Scopes)
	{
		for (const TRefCountPtr<IPooledRenderTarget>& Page : Scope->FeatherPages)
		{
			Count += Page.IsValid() ? 1 : 0;
		}
	}
	return Count;
}

ESightWeaveRenderAvailability
FSightWeaveSparseAtlasRenderState::GetMemoryAvailability_RenderThread() const
{
	return MemoryMirror.IsValid()
		? MemoryMirror->Availability
		: ESightWeaveRenderAvailability::Unknown;
}

uint64 FSightWeaveSparseAtlasRenderState::GetMemoryAppliedRevision_RenderThread() const
{
	return MemoryMirror.IsValid() ? MemoryMirror->AppliedMemoryRevision : 0;
}

uint64 FSightWeaveSparseAtlasRenderState::GetMemoryResourceGeneration_RenderThread() const
{
	return MemoryMirror.IsValid() ? MemoryMirror->ResourceGeneration : 0;
}

uint64 FSightWeaveSparseAtlasRenderState::GetMemoryResidencyGeneration_RenderThread() const
{
	return MemoryMirror.IsValid() ? MemoryMirror->ResidencyGeneration : 0;
}

uint64 FSightWeaveSparseAtlasRenderState::GetMemoryUploadCount_RenderThread() const
{
	return MemoryMirror.IsValid() ? MemoryMirror->UploadCount : 0;
}

uint64 FSightWeaveSparseAtlasRenderState::GetMemoryPageTableUploadCount_RenderThread() const
{
	return MemoryMirror.IsValid() ? MemoryMirror->PageTableUploadCount : 0;
}

int32 FSightWeaveSparseAtlasRenderState::GetMemoryResidentTileCount_RenderThread() const
{
	return MemoryMirror.IsValid() ? MemoryMirror->Residency.GetResidentCount() : 0;
}

int32 FSightWeaveSparseAtlasRenderState::GetAllocatedMemoryPageCount_RenderThread() const
{
	if (!MemoryMirror.IsValid())
	{
		return 0;
	}
	int32 Count = 0;
	for (const TRefCountPtr<IPooledRenderTarget>& Page : MemoryMirror->Pages)
	{
		Count += Page.IsValid() ? 1 : 0;
	}
	return Count;
}

ESightWeaveRenderAvailability
FSightWeaveSparseAtlasRenderState::GetStaticEnvironmentAvailability_RenderThread() const
{
	return StaticAttributeMirror.IsValid()
		? StaticAttributeMirror->Availability
		: ESightWeaveRenderAvailability::Unknown;
}

uint64 FSightWeaveSparseAtlasRenderState::GetStaticEnvironmentAppliedRevision_RenderThread() const
{
	return StaticAttributeMirror.IsValid()
		? StaticAttributeMirror->AppliedEligibilityRevision
		: 0;
}

uint64 FSightWeaveSparseAtlasRenderState::GetStaticEnvironmentUploadCount_RenderThread() const
{
	return StaticAttributeMirror.IsValid() ? StaticAttributeMirror->UploadCount : 0;
}

int32 FSightWeaveSparseAtlasRenderState::GetStaticEnvironmentResidentTileCount_RenderThread() const
{
	return StaticAttributeMirror.IsValid()
		? StaticAttributeMirror->Residency.GetResidentCount()
		: 0;
}

int32 FSightWeaveSparseAtlasRenderState::GetAllocatedStaticEnvironmentPageCount_RenderThread() const
{
	if (!StaticAttributeMirror.IsValid())
	{
		return 0;
	}
	int32 Count = 0;
	for (const TRefCountPtr<IPooledRenderTarget>& Page : StaticAttributeMirror->Pages)
	{
		Count += Page.IsValid() ? 1 : 0;
	}
	return Count;
}

#if WITH_DEV_AUTOMATION_TESTS
FRDGTextureRef FSightWeaveSparseAtlasRenderState::AddMemoryPresentationTestComposite_RenderThread(
	FRDGBuilder& GraphBuilder,
	const TConstArrayView<FVector2f> TranslatedWorldPositions,
	const TConstArrayView<FVector4f> SceneColors,
	const bool bForceMemoryUnavailable)
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
		TEXT("SightWeave.MemoryPresentation.TestOutput"));
	const TSharedPtr<const FSightWeaveViewPresentationBinding, ESPMode::ThreadSafe> Binding =
		PresentationBinding;
	FScopeState* Scope = Binding.IsValid() ? FindScope_RenderThread(Binding->GetScopeKey()) : nullptr;
	if (!Binding.IsValid()
		|| !Binding->IsValid()
		|| !Scope
		|| Scope->Availability != ESightWeaveRenderAvailability::Available
		|| !Scope->CurrentPageTable)
	{
		AddClearRenderTargetPass(GraphBuilder, Output, FLinearColor::Black);
		return Output;
	}
	const bool bMemoryReady = !bForceMemoryUnavailable
		&& MemoryMirror.IsValid()
		&& MemoryMirror->Availability == ESightWeaveRenderAvailability::Available
		&& MemoryMirror->CurrentPageTable
		&& MemoryScopeMatchesPresentationBinding(MemoryMirror->Scope, *Binding);
	const bool bStaticReady = bMemoryReady
		&& StaticAttributeMirror.IsValid()
		&& StaticAttributeMirror->Availability == ESightWeaveRenderAvailability::Available
		&& StaticAttributeMirror->CurrentPageTable
		&& StaticAttributeMirror->Scope.IsEquivalentTo(MemoryMirror->Scope);

	TArray<FVector2f> OwnedPositions(TranslatedWorldPositions);
	TArray<FVector4f> OwnedColors(SceneColors);
	FRDGBufferRef PositionBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("SightWeave.MemoryPresentation.TestPositions"),
		MoveTemp(OwnedPositions));
	FRDGBufferRef ColorBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("SightWeave.MemoryPresentation.TestColors"),
		MoveTemp(OwnedColors));
	FRDGTextureRef DummyPage = GSystemTextures.GetBlackDummy(GraphBuilder);
	FRDGTextureRef LivePages[4] = { DummyPage, DummyPage, DummyPage, DummyPage };
	for (int32 PageIndex = 0; PageIndex < Scope->Pages.Num() && PageIndex < 4; ++PageIndex)
	{
		if (Scope->Pages[PageIndex].IsValid())
		{
			LivePages[PageIndex] = GraphBuilder.RegisterExternalTexture(
				Scope->Pages[PageIndex],
				TEXT("SightWeave.MemoryPresentation.TestLivePage"));
		}
	}
	FRDGTextureRef MemoryPages[2] = { DummyPage, DummyPage };
	FRDGTextureRef StaticPages[2] = { DummyPage, DummyPage };
	if (bMemoryReady)
	{
		for (int32 PageIndex = 0; PageIndex < MemoryMirror->Pages.Num() && PageIndex < 2; ++PageIndex)
		{
			if (MemoryMirror->Pages[PageIndex].IsValid())
			{
				MemoryPages[PageIndex] = GraphBuilder.RegisterExternalTexture(
					MemoryMirror->Pages[PageIndex],
					TEXT("SightWeave.MemoryPresentation.TestMemoryPage"));
			}
		}
	}
	if (bStaticReady)
	{
		for (int32 PageIndex = 0;
			PageIndex < StaticAttributeMirror->Pages.Num() && PageIndex < 2;
			++PageIndex)
		{
			if (StaticAttributeMirror->Pages[PageIndex].IsValid())
			{
				StaticPages[PageIndex] = GraphBuilder.RegisterExternalTexture(
					StaticAttributeMirror->Pages[PageIndex],
					TEXT("SightWeave.MemoryPresentation.TestStaticPage"));
			}
		}
	}

	TShaderMapRef<FSightWeaveFullscreenVertexShader> VertexShader(
		GetGlobalShaderMap(GMaxRHIFeatureLevel));
	TShaderMapRef<FSightWeaveMemoryPresentationTestPixelShader> PixelShader(
		GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FSightWeaveMemoryPresentationTestPassParameters* Parameters =
		GraphBuilder.AllocParameters<FSightWeaveMemoryPresentationTestPassParameters>();
	Parameters->PixelShader.TestTranslatedWorldPositions = GraphBuilder.CreateSRV(PositionBuffer);
	Parameters->PixelShader.TestSceneColors = GraphBuilder.CreateSRV(ColorBuffer);
	Parameters->PixelShader.PageTable = GraphBuilder.CreateSRV(Scope->CurrentPageTable);
	Parameters->PixelShader.AtlasPage0 = LivePages[0];
	Parameters->PixelShader.AtlasPage1 = LivePages[1];
	Parameters->PixelShader.AtlasPage2 = LivePages[2];
	Parameters->PixelShader.AtlasPage3 = LivePages[3];
	Parameters->PixelShader.TranslatedFloorOrigin = FVector2f(Binding->GetScopeKey().FloorOrigin);
	Parameters->PixelShader.CentimetersPerTexel =
		SightWeaveCentimetersPerTexel(Binding->GetScopeKey().PrecisionTier);
	Parameters->PixelShader.PageTableCount = static_cast<uint32>(Scope->PageTableEntryCount);
	Parameters->PixelShader.MemoryPageTable = GraphBuilder.CreateSRV(
		bMemoryReady ? MemoryMirror->CurrentPageTable : Scope->CurrentPageTable);
	Parameters->PixelShader.MemoryPage0 = MemoryPages[0];
	Parameters->PixelShader.MemoryPage1 = MemoryPages[1];
	Parameters->PixelShader.MemoryPageTableCount = bMemoryReady
		? static_cast<uint32>(MemoryMirror->PageTableEntryCount)
		: 0;
	Parameters->PixelShader.StaticAttributePageTable = GraphBuilder.CreateSRV(
		bStaticReady ? StaticAttributeMirror->CurrentPageTable : Scope->CurrentPageTable);
	Parameters->PixelShader.StaticAttributePage0 = StaticPages[0];
	Parameters->PixelShader.StaticAttributePage1 = StaticPages[1];
	Parameters->PixelShader.StaticAttributePageTableCount = bStaticReady
		? static_cast<uint32>(StaticAttributeMirror->PageTableEntryCount)
		: 0;
	Parameters->PixelShader.MemoryTranslatedFloorOrigin = bMemoryReady
		? FVector2f(MemoryMirror->Scope.FloorOrigin)
		: FVector2f::ZeroVector;
	Parameters->PixelShader.MemoryTranslatedFloorPlaneZ = bMemoryReady
		? MemoryMirror->Scope.FloorPlaneZ
		: 0.0f;
	Parameters->PixelShader.MemoryCentimetersPerTexel = bMemoryReady
		? SightWeaveCentimetersPerTexel(MemoryMirror->Scope.PrecisionTier)
		: 1.0f;
	Parameters->PixelShader.MemoryPresentationAvailable = bMemoryReady && bStaticReady ? 1u : 0u;
	Parameters->PixelShader.TestSampleCount = static_cast<uint32>(TranslatedWorldPositions.Num());
	Parameters->RenderTargets[0] = FRenderTargetBinding(Output, ERenderTargetLoadAction::EClear);
	const FIntRect Viewport(FIntPoint::ZeroValue, OutputDesc.Extent);
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("SightWeave.MemoryPresentation.TestComposite"),
		Parameters,
		ERDGPassFlags::Raster,
		[Parameters, VertexShader, PixelShader, Viewport](
			FRDGAsyncTask,
			FRHICommandList& RHICmdList)
		{
			ConfigureViewport(RHICmdList, Viewport);
			FGraphicsPipelineStateInitializer PSO;
			RHICmdList.ApplyCachedRenderTargets(PSO);
			PSO.BlendState = TStaticBlendState<CW_RGBA>::GetRHI();
			PSO.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			PSO.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			PSO.PrimitiveType = PT_TriangleList;
			PSO.BoundShaderState.VertexDeclarationRHI =
				GEmptyVertexDeclaration.VertexDeclarationRHI;
			PSO.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			PSO.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			SetGraphicsPipelineState(RHICmdList, PSO, 0);
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
	if (Binding->GetVisualFeather().IsEnabled())
	{
		if (bFeatherUpdateIncomplete
			|| Binding->GetFeatherResourceGeneration() != Scope->FeatherResourceGeneration
			|| Binding->GetFeatherAppliedRevision() != AppliedRevision)
		{
			AddClearRenderTargetPass(GraphBuilder, Output, FLinearColor::Black);
			return Output;
		}
		FRDGTextureRef Dummy = GSystemTextures.GetBlackDummy(GraphBuilder);
		FRDGTextureRef HardPages[4] = { Dummy, Dummy, Dummy, Dummy };
		FRDGTextureRef FeatherPages[4] = { Dummy, Dummy, Dummy, Dummy };
		for (int32 PageIndex = 0; PageIndex < Scope->Pages.Num() && PageIndex < 4; ++PageIndex)
		{
			if (Scope->Pages[PageIndex].IsValid())
			{
				if (!Scope->FeatherPages.IsValidIndex(PageIndex)
					|| !Scope->FeatherPages[PageIndex].IsValid())
				{
					AddClearRenderTargetPass(GraphBuilder, Output, FLinearColor::Black);
					return Output;
				}
				HardPages[PageIndex] = GraphBuilder.RegisterExternalTexture(
					Scope->Pages[PageIndex], TEXT("SightWeave.FeatherTest.HardPage"));
				FeatherPages[PageIndex] = GraphBuilder.RegisterExternalTexture(
					Scope->FeatherPages[PageIndex], TEXT("SightWeave.FeatherTest.FeatherPage"));
			}
		}
		TShaderMapRef<FSightWeaveFullscreenVertexShader> FeatherVertexShader(
			GetGlobalShaderMap(GMaxRHIFeatureLevel));
		TShaderMapRef<FSightWeaveFeatherPresentationTestPixelShader> FeatherPixelShader(
			GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FSightWeaveFeatherPresentationTestPassParameters* FeatherParameters =
			GraphBuilder.AllocParameters<FSightWeaveFeatherPresentationTestPassParameters>();
		FeatherParameters->PixelShader.TestTranslatedWorldPositions = GraphBuilder.CreateSRV(PositionBuffer);
		FeatherParameters->PixelShader.TestSceneColors = GraphBuilder.CreateSRV(ColorBuffer);
		FeatherParameters->PixelShader.PageTable = GraphBuilder.CreateSRV(Scope->CurrentPageTable);
		FeatherParameters->PixelShader.AtlasPage0 = HardPages[0];
		FeatherParameters->PixelShader.AtlasPage1 = HardPages[1];
		FeatherParameters->PixelShader.AtlasPage2 = HardPages[2];
		FeatherParameters->PixelShader.AtlasPage3 = HardPages[3];
		FeatherParameters->PixelShader.FeatherPage0 = FeatherPages[0];
		FeatherParameters->PixelShader.FeatherPage1 = FeatherPages[1];
		FeatherParameters->PixelShader.FeatherPage2 = FeatherPages[2];
		FeatherParameters->PixelShader.FeatherPage3 = FeatherPages[3];
		FeatherParameters->PixelShader.TranslatedFloorOrigin = FVector2f(Binding->GetScopeKey().FloorOrigin);
		FeatherParameters->PixelShader.CentimetersPerTexel = SightWeaveCentimetersPerTexel(
			Binding->GetScopeKey().PrecisionTier);
		FeatherParameters->PixelShader.FeatherWidthCentimeters =
			Binding->GetVisualFeather().WidthCentimeters;
		FeatherParameters->PixelShader.PageTableCount = static_cast<uint32>(Scope->PageTableEntryCount);
		FeatherParameters->PixelShader.TestSampleCount = static_cast<uint32>(TranslatedWorldPositions.Num());
		FeatherParameters->RenderTargets[0] = FRenderTargetBinding(Output, ERenderTargetLoadAction::EClear);
		const FIntRect FeatherViewport(FIntPoint::ZeroValue, OutputDesc.Extent);
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("SightWeave.Feather.TestComposite"),
			FeatherParameters,
			ERDGPassFlags::Raster,
			[FeatherParameters, FeatherVertexShader, FeatherPixelShader, FeatherViewport](FRDGAsyncTask, FRHICommandList& RHICmdList)
			{
				ConfigureViewport(RHICmdList, FeatherViewport);
				FGraphicsPipelineStateInitializer PSO;
				RHICmdList.ApplyCachedRenderTargets(PSO);
				PSO.BlendState = TStaticBlendState<CW_RGBA>::GetRHI();
				PSO.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
				PSO.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				PSO.PrimitiveType = PT_TriangleList;
				PSO.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
				PSO.BoundShaderState.VertexShaderRHI = FeatherVertexShader.GetVertexShader();
				PSO.BoundShaderState.PixelShaderRHI = FeatherPixelShader.GetPixelShader();
				SetGraphicsPipelineState(RHICmdList, PSO, 0);
				SetShaderParameters(RHICmdList, FeatherVertexShader, FeatherVertexShader.GetVertexShader(), FeatherParameters->VertexShader);
				SetShaderParameters(RHICmdList, FeatherPixelShader, FeatherPixelShader.GetPixelShader(), FeatherParameters->PixelShader);
				RHICmdList.SetStreamSource(0, nullptr, 0);
				RHICmdList.DrawPrimitive(0, 1, 1);
			});
		return Output;
	}
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
	if (Binding->GetVisualFeather().IsEnabled())
	{
		if (bFeatherUpdateIncomplete
			|| Binding->GetFeatherResourceGeneration() != Scope->FeatherResourceGeneration
			|| Binding->GetFeatherAppliedRevision() != AppliedRevision)
		{
			AddClearRenderTargetPass(GraphBuilder, Output, FLinearColor::Black);
			return Output;
		}
		FRDGTextureRef Dummy = GSystemTextures.GetBlackDummy(GraphBuilder);
		FRDGTextureRef HardPages[4] = { Dummy, Dummy, Dummy, Dummy };
		FRDGTextureRef FeatherPages[4] = { Dummy, Dummy, Dummy, Dummy };
		for (int32 PageIndex = 0; PageIndex < Scope->Pages.Num() && PageIndex < 4; ++PageIndex)
		{
			if (Scope->Pages[PageIndex].IsValid())
			{
				if (!Scope->FeatherPages.IsValidIndex(PageIndex)
					|| !Scope->FeatherPages[PageIndex].IsValid())
				{
					AddClearRenderTargetPass(GraphBuilder, Output, FLinearColor::Black);
					return Output;
				}
				HardPages[PageIndex] = GraphBuilder.RegisterExternalTexture(
					Scope->Pages[PageIndex], TEXT("SightWeave.FeatherBenchmark.HardPage"));
				FeatherPages[PageIndex] = GraphBuilder.RegisterExternalTexture(
					Scope->FeatherPages[PageIndex], TEXT("SightWeave.FeatherBenchmark.FeatherPage"));
			}
		}
		TShaderMapRef<FSightWeaveFullscreenVertexShader> FeatherVertexShader(
			GetGlobalShaderMap(GMaxRHIFeatureLevel));
		TShaderMapRef<FSightWeaveFeatherPresentationBenchmarkPixelShader> FeatherPixelShader(
			GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FSightWeaveFeatherPresentationBenchmarkPassParameters* FeatherParameters =
			GraphBuilder.AllocParameters<FSightWeaveFeatherPresentationBenchmarkPassParameters>();
		FeatherParameters->PixelShader.PageTable = GraphBuilder.CreateSRV(Scope->CurrentPageTable);
		FeatherParameters->PixelShader.AtlasPage0 = HardPages[0];
		FeatherParameters->PixelShader.AtlasPage1 = HardPages[1];
		FeatherParameters->PixelShader.AtlasPage2 = HardPages[2];
		FeatherParameters->PixelShader.AtlasPage3 = HardPages[3];
		FeatherParameters->PixelShader.FeatherPage0 = FeatherPages[0];
		FeatherParameters->PixelShader.FeatherPage1 = FeatherPages[1];
		FeatherParameters->PixelShader.FeatherPage2 = FeatherPages[2];
		FeatherParameters->PixelShader.FeatherPage3 = FeatherPages[3];
		FeatherParameters->PixelShader.TranslatedFloorOrigin = FVector2f(Binding->GetScopeKey().FloorOrigin);
		FeatherParameters->PixelShader.CentimetersPerTexel = SightWeaveCentimetersPerTexel(
			Binding->GetScopeKey().PrecisionTier);
		FeatherParameters->PixelShader.FeatherWidthCentimeters =
			Binding->GetVisualFeather().WidthCentimeters;
		FeatherParameters->PixelShader.PageTableCount = static_cast<uint32>(Scope->PageTableEntryCount);
		FeatherParameters->PixelShader.TestWorldMin = TestWorldMin;
		FeatherParameters->PixelShader.TestWorldStep = TestWorldStep;
		FeatherParameters->PixelShader.TestOutputExtent = OutputExtent;
		FeatherParameters->RenderTargets[0] = FRenderTargetBinding(Output, ERenderTargetLoadAction::EClear);
		const FIntRect FeatherViewport(FIntPoint::ZeroValue, OutputExtent);
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("SightWeave.Feather.BenchmarkComposite"),
			FeatherParameters,
			ERDGPassFlags::Raster,
			[FeatherParameters, FeatherVertexShader, FeatherPixelShader, FeatherViewport](FRDGAsyncTask, FRHICommandList& RHICmdList)
			{
				ConfigureViewport(RHICmdList, FeatherViewport);
				FGraphicsPipelineStateInitializer PSO;
				RHICmdList.ApplyCachedRenderTargets(PSO);
				PSO.BlendState = TStaticBlendState<CW_RGBA>::GetRHI();
				PSO.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
				PSO.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				PSO.PrimitiveType = PT_TriangleList;
				PSO.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
				PSO.BoundShaderState.VertexShaderRHI = FeatherVertexShader.GetVertexShader();
				PSO.BoundShaderState.PixelShaderRHI = FeatherPixelShader.GetPixelShader();
				SetGraphicsPipelineState(RHICmdList, PSO, 0);
				SetShaderParameters(RHICmdList, FeatherVertexShader, FeatherVertexShader.GetVertexShader(), FeatherParameters->VertexShader);
				SetShaderParameters(RHICmdList, FeatherPixelShader, FeatherPixelShader.GetPixelShader(), FeatherParameters->PixelShader);
				RHICmdList.SetStreamSource(0, nullptr, 0);
				RHICmdList.DrawPrimitive(0, 1, 1);
			});
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

FRDGTextureRef FSightWeaveSparseAtlasRenderState::RegisterMemoryPageForReadback_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FSightWeaveMemoryTileKey& TileKey,
	FIntRect& OutSlotRect,
	FSightWeaveSparsePhysicalAddress& OutAddress)
{
	if (!MemoryMirror.IsValid() || !MemoryMirror->bHasScope
		|| !TileKey.Scope.IsEquivalentTo(MemoryMirror->Scope))
	{
		return nullptr;
	}
	const FSightWeaveSparseTileIdentity Identity =
		MakeMemoryTileIdentity(MemoryMirror->Scope, TileKey.LogicalCoordinate);
	const FSightWeaveSparseResidencySlot* Slot = MemoryMirror->Residency.Find(Identity);
	if (!Slot
		|| !MemoryMirror->Pages.IsValidIndex(Slot->Address.PageIndex)
		|| !MemoryMirror->Pages[Slot->Address.PageIndex].IsValid())
	{
		return nullptr;
	}
	OutAddress = Slot->Address;
	OutSlotRect = Slot->Address.GetSlotRect();
	return GraphBuilder.RegisterExternalTexture(
		MemoryMirror->Pages[Slot->Address.PageIndex],
		TEXT("SightWeave.Memory.ReadbackPage"));
}

bool FSightWeaveSparseAtlasRenderState::AddMemoryReadback_RenderThread(
	const FSightWeaveMemoryTileKey& TileKey)
{
	if (!MemoryMirror.IsValid() || !MemoryMirror->bHasScope
		|| !TileKey.Scope.IsEquivalentTo(MemoryMirror->Scope))
	{
		return false;
	}
	const FSightWeaveSparseTileIdentity Identity =
		MakeMemoryTileIdentity(MemoryMirror->Scope, TileKey.LogicalCoordinate);
	const FSightWeaveSparseResidencySlot* Slot = MemoryMirror->Residency.Find(Identity);
	return Slot && MemoryMirror->Residency.AddReadback(Slot->Address);
}

bool FSightWeaveSparseAtlasRenderState::RemoveMemoryReadback_RenderThread(
	const FSightWeaveMemoryTileKey& TileKey,
	const FSightWeaveSparsePhysicalAddress& Address)
{
	return MemoryMirror.IsValid()
		&& MemoryMirror->bHasScope
		&& TileKey.Scope.IsEquivalentTo(MemoryMirror->Scope)
		&& MemoryMirror->Residency.RemoveReadback(Address);
}
#endif
