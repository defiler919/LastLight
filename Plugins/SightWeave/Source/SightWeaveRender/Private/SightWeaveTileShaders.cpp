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
