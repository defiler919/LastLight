// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/DarkwellGameState.h"

#include "Game/DarkwellMissionRules.h"
#include "Gameplay/DarkwellGameplayTags.h"

void ADarkwellGameState::BeginPlay()
{
	Super::BeginPlay();
	MissionState = DarkwellGameplayTags::State_Mission_FindFuse;
	OnMissionStateChanged.Broadcast(MissionState);
}

bool ADarkwellGameState::CollectGeneratorFuse()
{
	return ApplyMissionEvent(EDarkwellMissionEvent::CollectFuse);
}

bool ADarkwellGameState::CompleteEscape()
{
	return ApplyMissionEvent(EDarkwellMissionEvent::UseExit);
}

bool ADarkwellGameState::RestoreMissionState(const FGameplayTag SavedMissionState)
{
	if (SavedMissionState != DarkwellGameplayTags::State_Mission_FindFuse
		&& SavedMissionState != DarkwellGameplayTags::State_Mission_ReachExit
		&& SavedMissionState != DarkwellGameplayTags::State_Mission_Escaped)
	{
		return false;
	}

	MissionState = SavedMissionState;
	OnMissionStateChanged.Broadcast(MissionState);
	return true;
}

bool ADarkwellGameState::IsFuseCollected() const
{
	return MissionState == DarkwellGameplayTags::State_Mission_ReachExit
		|| MissionState == DarkwellGameplayTags::State_Mission_Escaped;
}

bool ADarkwellGameState::IsEscapeComplete() const
{
	return MissionState == DarkwellGameplayTags::State_Mission_Escaped;
}

FText ADarkwellGameState::GetObjectiveText() const
{
	if (MissionState == DarkwellGameplayTags::State_Mission_ReachExit)
	{
		return NSLOCTEXT("Darkwell", "ObjectiveReachExit", "REACH THE EMERGENCY EXIT");
	}

	if (MissionState == DarkwellGameplayTags::State_Mission_Escaped)
	{
		return NSLOCTEXT("Darkwell", "ObjectiveEscaped", "ESCAPE COMPLETE");
	}

	return NSLOCTEXT("Darkwell", "ObjectiveFindFuse", "FIND THE GENERATOR FUSE");
}

bool ADarkwellGameState::ApplyMissionEvent(const EDarkwellMissionEvent Event)
{
	const FGameplayTag NewState = Darkwell::MissionRules::ResolveMissionState(MissionState, Event);
	if (!NewState.IsValid() || NewState == MissionState)
	{
		return false;
	}

	MissionState = NewState;
	OnMissionStateChanged.Broadcast(MissionState);
	return true;
}
