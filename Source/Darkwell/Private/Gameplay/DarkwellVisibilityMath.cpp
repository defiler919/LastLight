// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gameplay/DarkwellVisibilityMath.h"

namespace Darkwell::VisibilityMath
{
	FIntPoint WorldToCell(const FVector& WorldLocation, const float CellSize)
	{
		const float SafeCellSize = FMath::Max(CellSize, UE_KINDA_SMALL_NUMBER);
		return FIntPoint(
			FMath::FloorToInt(WorldLocation.X / SafeCellSize),
			FMath::FloorToInt(WorldLocation.Y / SafeCellSize));
	}

	FVector CellToWorldCenter(const FIntPoint& Cell, const float CellSize, const float Height)
	{
		const float SafeCellSize = FMath::Max(CellSize, UE_KINDA_SMALL_NUMBER);
		return FVector(
			(static_cast<double>(Cell.X) + 0.5) * SafeCellSize,
			(static_cast<double>(Cell.Y) + 0.5) * SafeCellSize,
			Height);
	}

	bool IsInsideVisionCone(
		const FVector& Origin,
		const FVector& FacingDirection,
		const FVector& Target,
		const float Range,
		const float HalfAngleDegrees)
	{
		const FVector ToTarget = (Target - Origin).GetSafeNormal2D();
		const float Distance = FVector::Dist2D(Origin, Target);
		if (Distance > FMath::Max(0.0f, Range) || ToTarget.IsNearlyZero())
		{
			return false;
		}

		const FVector Facing = FacingDirection.GetSafeNormal2D();
		if (Facing.IsNearlyZero())
		{
			return false;
		}

		const float SafeHalfAngle = FMath::Clamp(HalfAngleDegrees, 0.0f, 180.0f);
		const float FacingThreshold = FMath::Cos(FMath::DegreesToRadians(SafeHalfAngle));
		return FVector::DotProduct(Facing, ToTarget) >= FacingThreshold;
	}

	EDarkwellFogCellState ResolveFogCellState(
		const bool bCurrentlyVisible,
		const bool bPreviouslyExplored)
	{
		if (bCurrentlyVisible)
		{
			return EDarkwellFogCellState::Visible;
		}
		return bPreviouslyExplored
			? EDarkwellFogCellState::Explored
			: EDarkwellFogCellState::Unexplored;
	}

	float ConstrainRememberedFogAlpha(
		const float SmoothedAlpha,
		const float BoundaryAlpha)
	{
		return FMath::Clamp(FMath::Max(SmoothedAlpha, BoundaryAlpha), 0.0f, 1.0f);
	}
}
