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

IMPLEMENT_GLOBAL_SHADER(
	FSightWeaveSurfaceStatePixelShader,
	"/Plugin/SightWeave/Private/SightWeaveSingleTile.usf",
	"SightWeaveSurfaceStatePS",
	SF_Pixel);

IMPLEMENT_GLOBAL_SHADER(
	FSightWeaveFeatherSeedPixelShader,
	"/Plugin/SightWeave/Private/SightWeaveSingleTile.usf",
	"SightWeaveFeatherSeedPS",
	SF_Pixel);

IMPLEMENT_GLOBAL_SHADER(
	FSightWeaveFeatherJumpPixelShader,
	"/Plugin/SightWeave/Private/SightWeaveSingleTile.usf",
	"SightWeaveFeatherJumpPS",
	SF_Pixel);

IMPLEMENT_GLOBAL_SHADER(
	FSightWeaveFeatherFinalizePixelShader,
	"/Plugin/SightWeave/Private/SightWeaveSingleTile.usf",
	"SightWeaveFeatherFinalizePS",
	SF_Pixel);

IMPLEMENT_GLOBAL_SHADER(
	FSightWeaveInwardFeatherCompositePixelShader,
	"/Plugin/SightWeave/Private/SightWeaveSingleTile.usf",
	"SightWeaveInwardFeatherCompositePS",
	SF_Pixel);

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_GLOBAL_SHADER(
	FSightWeaveMemoryPresentationTestPixelShader,
	"/Plugin/SightWeave/Private/SightWeaveSingleTile.usf",
	"SightWeaveMemoryPresentationTestPS",
	SF_Pixel);

IMPLEMENT_GLOBAL_SHADER(
	FSightWeavePresentationTestPixelShader,
	"/Plugin/SightWeave/Private/SightWeaveSingleTile.usf",
	"SightWeavePresentationTestPS",
	SF_Pixel);

IMPLEMENT_GLOBAL_SHADER(
	FSightWeaveFeatherPresentationTestPixelShader,
	"/Plugin/SightWeave/Private/SightWeaveSingleTile.usf",
	"SightWeaveFeatherPresentationTestPS",
	SF_Pixel);

IMPLEMENT_GLOBAL_SHADER(
	FSightWeavePresentationBenchmarkPixelShader,
	"/Plugin/SightWeave/Private/SightWeaveSingleTile.usf",
	"SightWeavePresentationBenchmarkPS",
	SF_Pixel);

IMPLEMENT_GLOBAL_SHADER(
	FSightWeaveFeatherPresentationBenchmarkPixelShader,
	"/Plugin/SightWeave/Private/SightWeaveSingleTile.usf",
	"SightWeaveFeatherPresentationBenchmarkPS",
	SF_Pixel);
#endif
