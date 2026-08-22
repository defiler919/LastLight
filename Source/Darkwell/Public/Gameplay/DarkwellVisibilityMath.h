// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DarkwellVisibilityMath.generated.h"

/** StarCraft-style knowledge state for one world-space fog cell. */
UENUM(BlueprintType)
enum class EDarkwellFogCellState : uint8
{
	Unexplored,
	Explored,
	Visible
};

namespace Darkwell::VisibilityMath
{
	DARKWELL_API FIntPoint WorldToCell(const FVector& WorldLocation, float CellSize);
	DARKWELL_API FVector CellToWorldCenter(const FIntPoint& Cell, float CellSize, float Height);
	DARKWELL_API bool IsInsideVisionCone(
		const FVector& Origin,
		const FVector& FacingDirection,
		const FVector& Target,
		float Range,
		float HalfAngleDegrees);
	DARKWELL_API EDarkwellFogCellState ResolveFogCellState(bool bCurrentlyVisible, bool bPreviouslyExplored);
}
