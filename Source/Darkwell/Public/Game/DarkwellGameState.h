// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayTagContainer.h"
#include "DarkwellGameState.generated.h"

enum class EDarkwellMissionEvent : uint8;

DECLARE_MULTICAST_DELEGATE_OneParam(FDarkwellMissionStateChanged, FGameplayTag);

/** Authoritative offline mission progress for the current DARKWELL level. */
UCLASS()
class DARKWELL_API ADarkwellGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	bool CollectGeneratorFuse();
	bool CompleteEscape();
	bool RestoreMissionState(FGameplayTag SavedMissionState);

	FGameplayTag GetMissionState() const { return MissionState; }
	bool IsFuseCollected() const;
	bool IsEscapeComplete() const;
	FText GetObjectiveText() const;

	FDarkwellMissionStateChanged OnMissionStateChanged;

private:
	bool ApplyMissionEvent(EDarkwellMissionEvent Event);

	UPROPERTY(VisibleInstanceOnly, Category = "Mission")
	FGameplayTag MissionState;
};
