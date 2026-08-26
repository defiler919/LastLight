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
