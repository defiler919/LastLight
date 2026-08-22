// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gameplay/DarkwellSurvivalRules.h"

bool Darkwell::SurvivalRules::CanApplyDamage(
	const float CurrentHealth,
	const double CurrentTimeSeconds,
	const double InvulnerableUntilSeconds)
{
	return CurrentHealth > 0.0f && CurrentTimeSeconds >= InvulnerableUntilSeconds;
}

float Darkwell::SurvivalRules::ComputeAppliedDamage(
	const float CurrentHealth,
	const float RequestedDamage)
{
	return FMath::Min(FMath::Max(0.0f, CurrentHealth), FMath::Max(0.0f, RequestedDamage));
}

float Darkwell::SurvivalRules::ComputeDamageFeedbackAlpha(
	const double CurrentTimeSeconds,
	const double LastDamageTimeSeconds,
	const float FeedbackDurationSeconds)
{
	if (FeedbackDurationSeconds <= 0.0f || LastDamageTimeSeconds < 0.0)
	{
		return 0.0f;
	}

	const double ElapsedSeconds = CurrentTimeSeconds - LastDamageTimeSeconds;
	if (ElapsedSeconds < 0.0 || ElapsedSeconds >= FeedbackDurationSeconds)
	{
		return 0.0f;
	}

	return 1.0f - static_cast<float>(ElapsedSeconds / FeedbackDurationSeconds);
}
