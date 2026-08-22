// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "Perception/AIPerceptionTypes.h"
#include "DarkwellStalkerController.generated.h"

class ADarkwellCharacter;
class UAIPerceptionComponent;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;

/** Native first-enemy brain: perception, pursuit, torch boundary control, and lantern stun buildup. */
UCLASS()
class DARKWELL_API ADarkwellStalkerController : public AAIController
{
	GENERATED_BODY()

public:
	ADarkwellStalkerController();
	virtual void Tick(float DeltaSeconds) override;
	void ApplyLoadSafetyGrace(float DurationSeconds);
	void ApplyLightControl(float DurationSeconds);
	float GetLanternStunBuildup() const { return LanternStunBuildup; }
	float GetLanternStunSecondsRemaining() const { return LightControlRemaining; }

protected:
	virtual void OnPossess(APawn* InPawn) override;

private:
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	void UpdateBehavior();
	void UpdateLanternFocus(float DeltaSeconds);
	bool HasClearLightPathTo(const ADarkwellCharacter& Player) const;
	void SetEnemyState(FGameplayTag State) const;

	UPROPERTY(VisibleAnywhere, Category = "Perception")
	TObjectPtr<UAIPerceptionComponent> EnemyPerception;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	UPROPERTY(Transient)
	TObjectPtr<ADarkwellCharacter> TrackedPlayer;

	UPROPERTY(EditDefaultsOnly, Category = "Behavior", meta = (ClampMin = "0.0"))
	float InvestigationDuration = 5.0f;

	FVector LastKnownPlayerLocation = FVector::ZeroVector;
	float AlertTimeRemaining = 0.0f;
	float AttackCooldownRemaining = 0.0f;
	float BehaviorUpdateRemaining = 0.0f;
	float LoadSafetyGraceRemaining = 0.0f;
	float LightControlRemaining = 0.0f;
	float LanternStunBuildup = 0.0f;
	bool bHasVisualContact = false;
};
