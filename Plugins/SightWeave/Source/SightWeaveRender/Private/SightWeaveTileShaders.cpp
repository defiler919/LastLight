#include "SightWeaveTileShaders.h"

IMPLEMENT_GLOBAL_SHADER(
	FSightWeaveTileVertexShader,
	"/Plugin/SightWeave/Private/SightWeaveSingleTile.usf",
	"SightWeaveSmokeVS",
	SF_Vertex);

IMPLEMENT_GLOBAL_SHADER(
	FSightWeaveTilePixelShader,
	"/Plugin/SightWeave/Private/SightWeaveSingleTile.usf",
	"SightWeaveSmokePS",
	SF_Pixel);

IMPLEMENT_GLOBAL_SHADER(
	FSightWeaveFullscreenVertexShader,
	"/Plugin/SightWeave/Private/SightWeaveSingleTile.usf",
	"SightWeaveFullscreenVS",
	SF_Vertex);

IMPLEMENT_GLOBAL_SHADER(
	FSightWeaveCombinePixelShader,
	"/Plugin/SightWeave/Private/SightWeaveSingleTile.usf",
	"SightWeaveCombinePS",
	SF_Pixel);

IMPLEMENT_GLOBAL_SHADER(
	FSightWeaveAtlasProfileCombinePixelShader,
	"/Plugin/SightWeave/Private/SightWeaveSingleTile.usf",
	"SightWeaveAtlasProfileCombinePS",
	SF_Pixel);

IMPLEMENT_GLOBAL_SHADER(
	FSightWeaveAtlasSuppressionPixelShader,
	"/Plugin/SightWeave/Private/SightWeaveSingleTile.usf",
	"SightWeaveAtlasSuppressionPS",
	SF_Pixel);

IMPLEMENT_GLOBAL_SHADER(
	FSightWeaveHardMaskCompositePixelShader,
	"/Plugin/SightWeave/Private/SightWeaveSingleTile.usf",
	"SightWeaveHardMaskCompositePS",
	SF_Pixel);
