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

	/** Turns toward DesiredYaw by at most TurnRate * DeltaTime using the shortest angular path. */
	DARKWELL_API float TurnYawToward(
		float CurrentYaw,
		float DesiredYaw,
		float TurnRateDegreesPerSecond,
		float DeltaTime);

	/** Sprinting requires an active request, gameplay permission, and a movement direction. */
	DARKWELL_API bool ShouldSprint(
		bool bSprintRequested,
		bool bCanMove,
		const FVector& MovementDirection);

	/**
	 * Scales locomotion by the angle between body facing and requested movement.
	 * Forward movement reaches full speed, strafing uses StrafeScale, and
	 * backpedalling uses BackpedalScale with continuous interpolation between.
	 */
	DARKWELL_API float ComputeDirectionalSpeedScale(
		const FVector& FacingDirection,
		const FVector& MovementDirection,
		float StrafeScale,
		float BackpedalScale);

	/** A held primary-fire gesture enters aimed mode at the configured threshold. */
	DARKWELL_API bool IsPrimaryFireAimActive(float HeldSeconds, float HoldThresholdSeconds);

	/** Returns linear aim progress from the instant primary fire is pressed until the configured tighten duration. */
	DARKWELL_API float ComputePrimaryFireAimProgress(float HeldSeconds, float TightenDurationSeconds);

	/** Tests a planar candidate against a maximum range and a forgiving facing cone. */
	DARKWELL_API bool IsFacingProximityCandidate(
		const FVector& FacingDirection,
		const FVector& CandidateOffset,
		float MaximumDistance,
		float HalfAngleDegrees);

	/** Chooses the candidate nearest the facing centerline, using distance only to break an angular tie. */
	DARKWELL_API bool IsFacingInteractionCandidatePreferred(
		float CandidateAlignment,
		float CandidateDistanceSquared,
		float BestAlignment,
		float BestDistanceSquared);
}
