// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Darkwell::SurvivalRules
{
	/** Returns whether an alive character is currently allowed to receive damage. */
	DARKWELL_API bool CanApplyDamage(float CurrentHealth, double CurrentTimeSeconds, double InvulnerableUntilSeconds);

	/** Clamps requested damage so health cannot be reduced below zero. */
	DARKWELL_API float ComputeAppliedDamage(float CurrentHealth, float RequestedDamage);

	/** Returns a normalized, linearly fading damage-feedback strength. */
	DARKWELL_API float ComputeDamageFeedbackAlpha(
		double CurrentTimeSeconds,
		double LastDamageTimeSeconds,
		float FeedbackDurationSeconds);
}
