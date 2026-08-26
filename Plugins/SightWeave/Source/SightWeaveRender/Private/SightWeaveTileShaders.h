#pragma once

#include "GlobalShader.h"
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

class FSightWeaveTilePixelShader final : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSightWeaveTilePixelShader);
	SHADER_USE_PARAMETER_STRUCT(FSightWeaveTilePixelShader, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return Parameters.Platform == SP_PCD3D_SM6;
	}
};
