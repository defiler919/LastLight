// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/DarkwellEnemyMath.h"

EDarkwellEnemyIntent Darkwell::EnemyMath::ChooseIntent(
	const bool bHasVisualContact,
	const bool bIsAlerted,
	const bool bTorchDeterrentActive,
	const float DistanceToPlayer,
	const float PlayerFacingDot,
	const float TorchDeterrentRange,
	const float TorchFacingThreshold,
	const float TorchBoundaryBuffer)
{
	if (bHasVisualContact)
	{
		const float SafeRange = FMath::Max(0.0f, TorchDeterrentRange);
		const bool bInsideTorchCone = bTorchDeterrentActive
			&& PlayerFacingDot >= TorchFacingThreshold;
		if (bInsideTorchCone && DistanceToPlayer <= SafeRange)
		{
			return EDarkwellEnemyIntent::Repel;
		}
		if (bInsideTorchCone && DistanceToPlayer <= SafeRange + FMath::Max(0.0f, TorchBoundaryBuffer))
		{
			return EDarkwellEnemyIntent::HoldAtBay;
		}
		return EDarkwellEnemyIntent::Hunt;
	}

	return bIsAlerted ? EDarkwellEnemyIntent::Investigate : EDarkwellEnemyIntent::Idle;
}

FVector Darkwell::EnemyMath::MakeBoundaryDestination(
	const FVector& EnemyLocation,
	const FVector& PlayerLocation,
	const float MinimumDistance)
{
	FVector Away = EnemyLocation - PlayerLocation;
	Away.Z = 0.0f;
	Away = Away.GetSafeNormal(UE_SMALL_NUMBER, FVector::XAxisVector);
	FVector Destination = PlayerLocation + Away * FMath::Max(0.0f, MinimumDistance);
	Destination.Z = EnemyLocation.Z;
	return Destination;
}

float Darkwell::EnemyMath::UpdateLanternStunBuildup(
	const float CurrentBuildup,
	const bool bFocusHitsEnemy,
	const float DeltaSeconds,
	const float SecondsToFill,
	const float DecayPerSecond)
{
	const float SafeDelta = FMath::Max(0.0f, DeltaSeconds);
	const float ClampedCurrent = FMath::Clamp(CurrentBuildup, 0.0f, 1.0f);
	if (bFocusHitsEnemy)
	{
		return FMath::Clamp(
			ClampedCurrent + SafeDelta / FMath::Max(UE_SMALL_NUMBER, SecondsToFill),
			0.0f,
			1.0f);
	}

	return FMath::Clamp(
		ClampedCurrent - FMath::Max(0.0f, DecayPerSecond) * SafeDelta,
		0.0f,
		1.0f);
}
