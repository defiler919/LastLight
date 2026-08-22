// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EDarkwellEnemyIntent : uint8
{
	Idle,
	Investigate,
	Hunt,
	Repel,
	HoldAtBay
};

namespace Darkwell::EnemyMath
{
	/** Chooses the first stalker behavior from durable sensory facts. */
	DARKWELL_API EDarkwellEnemyIntent ChooseIntent(
		bool bHasVisualContact,
		bool bIsAlerted,
		bool bTorchDeterrentActive,
		float DistanceToPlayer,
		float PlayerFacingDot,
		float TorchDeterrentRange,
		float TorchFacingThreshold,
		float TorchBoundaryBuffer);

	/** Returns the nearest planar point on a minimum-distance boundary around the player. */
	DARKWELL_API FVector MakeBoundaryDestination(
		const FVector& EnemyLocation,
		const FVector& PlayerLocation,
		float MinimumDistance);

	/** Advances or decays normalized lantern focus stun buildup. */
	DARKWELL_API float UpdateLanternStunBuildup(
		float CurrentBuildup,
		bool bFocusHitsEnemy,
		float DeltaSeconds,
		float SecondsToFill,
		float DecayPerSecond);
}
