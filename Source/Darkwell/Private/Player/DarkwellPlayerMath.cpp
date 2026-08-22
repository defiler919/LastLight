// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/DarkwellPlayerMath.h"

namespace Darkwell::PlayerMath
{
	bool TryGetPlanarDirection(const FVector& Origin, const FVector& Target, FVector& OutDirection)
	{
		const FVector PlanarDelta(Target.X - Origin.X, Target.Y - Origin.Y, 0.0);
		const double SizeSquared = PlanarDelta.SizeSquared();
		if (SizeSquared <= UE_DOUBLE_KINDA_SMALL_NUMBER)
		{
			OutDirection = FVector::ZeroVector;
			return false;
		}

		OutDirection = PlanarDelta * FMath::InvSqrt(SizeSquared);
		return true;
	}

	bool TryIntersectHorizontalPlane(
		const FVector& RayOrigin,
		const FVector& RayDirection,
		const float PlaneHeight,
		FVector& OutIntersection)
	{
		if (FMath::IsNearlyZero(RayDirection.Z))
		{
			OutIntersection = FVector::ZeroVector;
			return false;
		}

		const double DistanceAlongRay = (PlaneHeight - RayOrigin.Z) / RayDirection.Z;
		if (DistanceAlongRay < 0.0)
		{
			OutIntersection = FVector::ZeroVector;
			return false;
		}

		OutIntersection = RayOrigin + RayDirection * DistanceAlongRay;
		OutIntersection.Z = PlaneHeight;
		return true;
	}

	float TurnYawToward(
		const float CurrentYaw,
		const float DesiredYaw,
		const float TurnRateDegreesPerSecond,
		const float DeltaTime)
	{
		const float MaximumDelta = FMath::Max(0.0f, TurnRateDegreesPerSecond)
			* FMath::Max(0.0f, DeltaTime);
		return FMath::FixedTurn(CurrentYaw, DesiredYaw, MaximumDelta);
	}

	bool ShouldSprint(
		const bool bSprintRequested,
		const bool bCanMove,
		const FVector& MovementDirection)
	{
		return bSprintRequested && bCanMove && !MovementDirection.IsNearlyZero();
	}
}
