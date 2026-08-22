// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DarkwellMissionRules.generated.h"

UENUM()
enum class EDarkwellMissionEvent : uint8
{
	CollectFuse,
	UseExit
};

namespace Darkwell::MissionRules
{
	/** Applies a mission event to the current durable mission state. */
	DARKWELL_API FGameplayTag ResolveMissionState(FGameplayTag CurrentState, EDarkwellMissionEvent Event);
}
