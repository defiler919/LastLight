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

	float ComputeDirectionalSpeedScale(
		const FVector& FacingDirection,
		const FVector& MovementDirection,
		const float StrafeScale,
		const float BackpedalScale)
	{
		const FVector Facing2D = FacingDirection.GetSafeNormal2D();
		const FVector Movement2D = MovementDirection.GetSafeNormal2D();
		if (Facing2D.IsNearlyZero() || Movement2D.IsNearlyZero())
		{
			return 1.0f;
		}

		const float SafeStrafeScale = FMath::Clamp(StrafeScale, 0.0f, 1.0f);
		const float SafeBackpedalScale = FMath::Clamp(BackpedalScale, 0.0f, SafeStrafeScale);
		const float ForwardAlignment = FMath::Clamp(
			FVector::DotProduct(Facing2D, Movement2D),
			-1.0f,
			1.0f);
		return ForwardAlignment >= 0.0f
			? FMath::Lerp(SafeStrafeScale, 1.0f, ForwardAlignment)
			: FMath::Lerp(SafeStrafeScale, SafeBackpedalScale, -ForwardAlignment);
	}

	bool IsPrimaryFireAimActive(const float HeldSeconds, const float HoldThresholdSeconds)
	{
		return HeldSeconds >= FMath::Max(0.0f, HoldThresholdSeconds);
	}

	float ComputePrimaryFireAimProgress(const float HeldSeconds, const float TightenDurationSeconds)
	{
		const float SafeDuration = FMath::Max(TightenDurationSeconds, UE_SMALL_NUMBER);
		return FMath::Clamp(FMath::Max(HeldSeconds, 0.0f) / SafeDuration, 0.0f, 1.0f);
	}

	bool IsFacingProximityCandidate(
		const FVector& FacingDirection,
		const FVector& CandidateOffset,
		const float MaximumDistance,
		const float HalfAngleDegrees)
	{
		const FVector PlanarOffset(CandidateOffset.X, CandidateOffset.Y, 0.0f);
		const float SafeMaximumDistance = FMath::Max(0.0f, MaximumDistance);
		if (PlanarOffset.SizeSquared() > FMath::Square(SafeMaximumDistance))
		{
			return false;
		}
		if (PlanarOffset.IsNearlyZero())
		{
			return true;
		}

		const FVector PlanarFacing = FacingDirection.GetSafeNormal2D();
		if (PlanarFacing.IsNearlyZero())
		{
			return false;
		}

		const float MinimumAlignment = FMath::Cos(FMath::DegreesToRadians(
			FMath::Clamp(HalfAngleDegrees, 0.0f, 180.0f)));
		return FVector::DotProduct(PlanarFacing, PlanarOffset.GetSafeNormal()) >= MinimumAlignment;
	}

	bool IsFacingInteractionCandidatePreferred(
		const float CandidateAlignment,
		const float CandidateDistanceSquared,
		const float BestAlignment,
		const float BestDistanceSquared)
	{
		if (!FMath::IsNearlyEqual(CandidateAlignment, BestAlignment))
		{
			return CandidateAlignment > BestAlignment;
		}

		return CandidateDistanceSquared < BestDistanceSquared;
	}
}
