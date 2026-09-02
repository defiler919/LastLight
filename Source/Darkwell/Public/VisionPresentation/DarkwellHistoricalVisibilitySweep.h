#pragma once

#include "CoreMinimal.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"

/** Continuous fixed-origin rotation proof. Never publishes or unions a visibility sector. */
struct DARKWELL_API FDarkwellHistoricalVisibilitySweep
{
	static bool IsSupported(const FDarkwellFogVisualSourceSnapshot& Previous,
		const FDarkwellFogVisualSourceSnapshot& Current);
	static bool MayAffectBounds(const FDarkwellFogVisualSourceSnapshot& Previous,
		const FDarkwellFogVisualSourceSnapshot& Current, const FBox2D& Bounds);
	/** Finds ONE common intermediate pose for all corners and center, then tests real occlusion. */
	static bool ProveEmptyFootprintCoverage(const FDarkwellFogVisualSourceSnapshot& Previous,
		const FDarkwellFogVisualSourceSnapshot& Current,
		TConstArrayView<FDarkwellFogVisualSegment> Occluders,
		const FBox2D& Footprint, uint64& OutCoverageQueries);
};
