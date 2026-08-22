// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Darkwell::PlayerMath
{
	/** Returns a normalized XY direction from Origin to Target. Height is intentionally ignored. */
	DARKWELL_API bool TryGetPlanarDirection(const FVector& Origin, const FVector& Target, FVector& OutDirection);

	/** Intersects a forward ray with a horizontal plane. Parallel rays and intersections behind the ray are rejected. */
	DARKWELL_API bool TryIntersectHorizontalPlane(
		const FVector& RayOrigin,
		const FVector& RayDirection,
		float PlaneHeight,
		FVector& OutIntersection);
}
