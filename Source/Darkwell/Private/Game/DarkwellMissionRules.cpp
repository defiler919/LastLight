// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/DarkwellMissionRules.h"

#include "Gameplay/DarkwellGameplayTags.h"

FGameplayTag Darkwell::MissionRules::ResolveMissionState(
	const FGameplayTag CurrentState,
	const EDarkwellMissionEvent Event)
{
	if (CurrentState == DarkwellGameplayTags::State_Mission_FindFuse
		&& Event == EDarkwellMissionEvent::CollectFuse)
	{
		return DarkwellGameplayTags::State_Mission_ReachExit;
	}

	if (CurrentState == DarkwellGameplayTags::State_Mission_ReachExit
		&& Event == EDarkwellMissionEvent::UseExit)
	{
		return DarkwellGameplayTags::State_Mission_Escaped;
	}

	return CurrentState;
}
