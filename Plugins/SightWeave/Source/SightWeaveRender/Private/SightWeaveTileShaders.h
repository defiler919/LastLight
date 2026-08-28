#pragma once

#include "GlobalShader.h"
#include "SceneView.h"
#include "ShaderParameterStruct.h"

/** M3.1 D3D12 SM6 shader registration smoke; parameters are extended by the raster checkpoint. */
class FSightWeaveTileVertexShader final : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSightWeaveTileVertexShader);
	SHADER_USE_PARAMETER_STRUCT(FSightWeaveTileVertexShader, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float2>, TriangleVertices)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TriangleIndices)
		SHADER_PARAMETER(uint32, FirstIndex)
		SHADER_PARAMETER(float, InvPhysicalWorldSpan)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == SP_PCD3D_SM6;
	}
};

class FSightWeaveFullscreenVertexShader final : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSightWeaveFullscreenVertexShader);
	SHADER_USE_PARAMETER_STRUCT(FSightWeaveFullscreenVertexShader, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == SP_PCD3D_SM6;
	}
};

class FSightWeaveCombinePixelShader final : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSightWeaveCombinePixelShader);
	SHADER_USE_PARAMETER_STRUCT(FSightWeaveCombinePixelShader, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, VisionTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, IlluminationTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BypassTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SuppressionTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == SP_PCD3D_SM6;
	}
};

class FSightWeaveAtlasProfileCombinePixelShader final : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSightWeaveAtlasProfileCombinePixelShader);
	SHADER_USE_PARAMETER_STRUCT(FSightWeaveAtlasProfileCombinePixelShader, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, VisionTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, IlluminationTexture)
		SHADER_PARAMETER(uint32, DestinationOriginX)
		SHADER_PARAMETER(uint32, DestinationOriginY)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == SP_PCD3D_SM6;
	}
};

class FSightWeaveAtlasSuppressionPixelShader final : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSightWeaveAtlasSuppressionPixelShader);
	SHADER_USE_PARAMETER_STRUCT(FSightWeaveAtlasSuppressionPixelShader, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SuppressionTexture)
		SHADER_PARAMETER(uint32, DestinationOriginX)
		SHADER_PARAMETER(uint32, DestinationOriginY)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == SP_PCD3D_SM6;
	}
};

class FSightWeaveTilePixelShader final : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSightWeaveTilePixelShader);
	SHADER_USE_PARAMETER_STRUCT(FSightWeaveTilePixelShader, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(float, RasterCentimetersPerTexel)
		SHADER_PARAMETER(uint32, RasterTargetOriginX)
		SHADER_PARAMETER(uint32, RasterTargetOriginY)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == SP_PCD3D_SM6;
	}
};

/** After-tonemap hard mask composite. The sparse atlas is sampled with integer texel loads. */
class FSightWeaveHardMaskCompositePixelShader final : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSightWeaveHardMaskCompositePixelShader);
	SHADER_USE_PARAMETER_STRUCT(FSightWeaveHardMaskCompositePixelShader, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SubjectProxyDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<uint2>, SubjectProxyStencilTexture)
		SHADER_PARAMETER(uint32, SubjectProxyStencilValue)
		SHADER_PARAMETER(float, SubjectProxyNeutralIntensity)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<int4>, PageTable)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage3)
		SHADER_PARAMETER(FIntPoint, OutputRectMin)
		SHADER_PARAMETER(FIntPoint, OutputRectSize)
		SHADER_PARAMETER(FIntPoint, SceneColorRectMin)
		SHADER_PARAMETER(FIntPoint, SceneColorRectSize)
		SHADER_PARAMETER(FVector2f, TranslatedFloorOrigin)
		SHADER_PARAMETER(float, CentimetersPerTexel)
		SHADER_PARAMETER(uint32, PageTableCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<int4>, MemoryPageTable)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MemoryPage0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MemoryPage1)
		SHADER_PARAMETER(uint32, MemoryPageTableCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<int4>, StaticAttributePageTable)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, StaticAttributePage0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, StaticAttributePage1)
		SHADER_PARAMETER(uint32, StaticAttributePageTableCount)
		SHADER_PARAMETER(FVector2f, MemoryTranslatedFloorOrigin)
		SHADER_PARAMETER(float, MemoryTranslatedFloorPlaneZ)
		SHADER_PARAMETER(float, MemoryCentimetersPerTexel)
		SHADER_PARAMETER(uint32, MemoryPresentationAvailable)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == SP_PCD3D_SM6;
	}
};

class FSightWeaveFeatherSeedPixelShader final : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSightWeaveFeatherSeedPixelShader);
	SHADER_USE_PARAMETER_STRUCT(FSightWeaveFeatherSeedPixelShader, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<int4>, PageTable)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage3)
		SHADER_PARAMETER(FIntPoint, FeatherWorkOrigin)
		SHADER_PARAMETER(uint32, FeatherWorkSize)
		SHADER_PARAMETER(uint32, PageTableCount)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == SP_PCD3D_SM6;
	}
};

class FSightWeaveFeatherJumpPixelShader final : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSightWeaveFeatherJumpPixelShader);
	SHADER_USE_PARAMETER_STRUCT(FSightWeaveFeatherJumpPixelShader, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FeatherSeedTexture)
		SHADER_PARAMETER(FIntPoint, FeatherWorkOrigin)
		SHADER_PARAMETER(int32, FeatherJumpStep)
		SHADER_PARAMETER(uint32, FeatherWorkSize)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == SP_PCD3D_SM6;
	}
};

class FSightWeaveFeatherFinalizePixelShader final : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSightWeaveFeatherFinalizePixelShader);
	SHADER_USE_PARAMETER_STRUCT(FSightWeaveFeatherFinalizePixelShader, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<int4>, PageTable)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FeatherSeedTexture)
		SHADER_PARAMETER(FIntPoint, FeatherLogicalTile)
		SHADER_PARAMETER(float, FeatherWidthCentimeters)
		SHADER_PARAMETER(float, CentimetersPerTexel)
		SHADER_PARAMETER(uint32, PageTableCount)
		SHADER_PARAMETER(uint32, DestinationOriginX)
		SHADER_PARAMETER(uint32, DestinationOriginY)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == SP_PCD3D_SM6;
	}
};

class FSightWeaveInwardFeatherCompositePixelShader final : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSightWeaveInwardFeatherCompositePixelShader);
	SHADER_USE_PARAMETER_STRUCT(FSightWeaveInwardFeatherCompositePixelShader, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SubjectProxyDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<uint2>, SubjectProxyStencilTexture)
		SHADER_PARAMETER(uint32, SubjectProxyStencilValue)
		SHADER_PARAMETER(float, SubjectProxyNeutralIntensity)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<int4>, PageTable)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FeatherPage0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FeatherPage1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FeatherPage2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FeatherPage3)
		SHADER_PARAMETER(FIntPoint, OutputRectMin)
		SHADER_PARAMETER(FIntPoint, OutputRectSize)
		SHADER_PARAMETER(FIntPoint, SceneColorRectMin)
		SHADER_PARAMETER(FIntPoint, SceneColorRectSize)
		SHADER_PARAMETER(FVector2f, TranslatedFloorOrigin)
		SHADER_PARAMETER(float, CentimetersPerTexel)
		SHADER_PARAMETER(float, FeatherWidthCentimeters)
		SHADER_PARAMETER(uint32, PageTableCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<int4>, MemoryPageTable)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MemoryPage0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MemoryPage1)
		SHADER_PARAMETER(uint32, MemoryPageTableCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<int4>, StaticAttributePageTable)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, StaticAttributePage0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, StaticAttributePage1)
		SHADER_PARAMETER(uint32, StaticAttributePageTableCount)
		SHADER_PARAMETER(FVector2f, MemoryTranslatedFloorOrigin)
		SHADER_PARAMETER(float, MemoryTranslatedFloorPlaneZ)
		SHADER_PARAMETER(float, MemoryCentimetersPerTexel)
		SHADER_PARAMETER(uint32, MemoryPresentationAvailable)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == SP_PCD3D_SM6;
	}
};

#if WITH_DEV_AUTOMATION_TESTS
class FSightWeaveMemoryPresentationTestPixelShader final : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSightWeaveMemoryPresentationTestPixelShader);
	SHADER_USE_PARAMETER_STRUCT(FSightWeaveMemoryPresentationTestPixelShader, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float2>, TestTranslatedWorldPositions)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, TestSceneColors)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<int4>, PageTable)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage3)
		SHADER_PARAMETER(FVector2f, TranslatedFloorOrigin)
		SHADER_PARAMETER(float, CentimetersPerTexel)
		SHADER_PARAMETER(uint32, PageTableCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<int4>, MemoryPageTable)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MemoryPage0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MemoryPage1)
		SHADER_PARAMETER(uint32, MemoryPageTableCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<int4>, StaticAttributePageTable)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, StaticAttributePage0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, StaticAttributePage1)
		SHADER_PARAMETER(uint32, StaticAttributePageTableCount)
		SHADER_PARAMETER(FVector2f, MemoryTranslatedFloorOrigin)
		SHADER_PARAMETER(float, MemoryTranslatedFloorPlaneZ)
		SHADER_PARAMETER(float, MemoryCentimetersPerTexel)
		SHADER_PARAMETER(uint32, MemoryPresentationAvailable)
		SHADER_PARAMETER(uint32, TestSampleCount)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == SP_PCD3D_SM6;
	}
};

class FSightWeavePresentationTestPixelShader final : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSightWeavePresentationTestPixelShader);
	SHADER_USE_PARAMETER_STRUCT(FSightWeavePresentationTestPixelShader, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float2>, TestTranslatedWorldPositions)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, TestSceneColors)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<int4>, PageTable)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage3)
		SHADER_PARAMETER(FVector2f, TranslatedFloorOrigin)
		SHADER_PARAMETER(float, CentimetersPerTexel)
		SHADER_PARAMETER(uint32, PageTableCount)
		SHADER_PARAMETER(uint32, TestSampleCount)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == SP_PCD3D_SM6;
	}
};

class FSightWeaveFeatherPresentationTestPixelShader final : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSightWeaveFeatherPresentationTestPixelShader);
	SHADER_USE_PARAMETER_STRUCT(FSightWeaveFeatherPresentationTestPixelShader, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float2>, TestTranslatedWorldPositions)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, TestSceneColors)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<int4>, PageTable)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FeatherPage0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FeatherPage1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FeatherPage2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FeatherPage3)
		SHADER_PARAMETER(FVector2f, TranslatedFloorOrigin)
		SHADER_PARAMETER(float, CentimetersPerTexel)
		SHADER_PARAMETER(float, FeatherWidthCentimeters)
		SHADER_PARAMETER(uint32, PageTableCount)
		SHADER_PARAMETER(uint32, TestSampleCount)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == SP_PCD3D_SM6;
	}
};

class FSightWeavePresentationBenchmarkPixelShader final : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSightWeavePresentationBenchmarkPixelShader);
	SHADER_USE_PARAMETER_STRUCT(FSightWeavePresentationBenchmarkPixelShader, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<int4>, PageTable)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage3)
		SHADER_PARAMETER(FVector2f, TranslatedFloorOrigin)
		SHADER_PARAMETER(float, CentimetersPerTexel)
		SHADER_PARAMETER(uint32, PageTableCount)
		SHADER_PARAMETER(FVector2f, TestWorldMin)
		SHADER_PARAMETER(FVector2f, TestWorldStep)
		SHADER_PARAMETER(FIntPoint, TestOutputExtent)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == SP_PCD3D_SM6;
	}
};

class FSightWeaveFeatherPresentationBenchmarkPixelShader final : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSightWeaveFeatherPresentationBenchmarkPixelShader);
	SHADER_USE_PARAMETER_STRUCT(FSightWeaveFeatherPresentationBenchmarkPixelShader, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<int4>, PageTable)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AtlasPage3)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FeatherPage0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FeatherPage1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FeatherPage2)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FeatherPage3)
		SHADER_PARAMETER(FVector2f, TranslatedFloorOrigin)
		SHADER_PARAMETER(float, CentimetersPerTexel)
		SHADER_PARAMETER(float, FeatherWidthCentimeters)
		SHADER_PARAMETER(uint32, PageTableCount)
		SHADER_PARAMETER(FVector2f, TestWorldMin)
		SHADER_PARAMETER(FVector2f, TestWorldStep)
		SHADER_PARAMETER(FIntPoint, TestOutputExtent)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == SP_PCD3D_SM6;
	}
};
#endif
