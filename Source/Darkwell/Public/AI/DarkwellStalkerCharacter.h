// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Gameplay/DarkwellFogSubject.h"
#include "GameplayTagContainer.h"
#include "DarkwellStalkerCharacter.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/** Greybox enemy body and durable state; decision-making lives in its native AI controller. */
UCLASS()
class DARKWELL_API ADarkwellStalkerCharacter : public ACharacter, public IDarkwellFogSubject
{
	GENERATED_BODY()

public:
	ADarkwellStalkerCharacter();

	virtual void BeginPlay() override;
	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator,
		AActor* DamageCauser) override;
	virtual void SetPlayerFogState(EDarkwellFogCellState NewState) override;

	void SetBehaviorState(FGameplayTag NewState);
	void ConfigurePersistentId(FName InPersistentId) { PersistentId = InPersistentId; }
	void RestorePersistentState(const FTransform& SavedTransform, float SavedHealth, FGameplayTag SavedBehaviorState, bool bSavedAlive);
	FName GetPersistentId() const { return PersistentId; }
	FGameplayTag GetBehaviorState() const { return BehaviorState; }
	FGameplayTag GetEnemyArchetype() const { return EnemyArchetype; }
	const FText& GetThreatName() const { return ThreatName; }
	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	float GetTorchDeterrenceRangeScale() const { return TorchDeterrenceRangeScale; }
	float GetTorchBoundarySafetyMargin() const { return TorchBoundarySafetyMargin; }
	float GetTorchBoundaryBuffer() const { return TorchBoundaryBuffer; }
	float GetLanternFocusSecondsToStun() const { return LanternFocusSecondsToStun; }
	float GetLanternFocusDecayPerSecond() const { return LanternFocusDecayPerSecond; }
	float GetLanternFocusStunDuration() const { return LanternFocusStunDuration; }
	float GetAttackRange() const { return AttackRange; }
	float GetAttackDamage() const { return AttackDamage; }
	float GetAttackInterval() const { return AttackInterval; }
	bool IsAlive() const;
	void ApplySightWeaveVisibility(bool bVisible, uint64 AuthorityRevision);
	uint64 GetAppliedVisibilityAuthorityRevision() const
	{
		return AppliedVisibilityAuthorityRevision;
	}
	bool IsVisibleBySightWeaveAuthority() const { return bVisibleBySightWeaveAuthority; }

private:
	void RefreshPresentation();
	void ApplyDeadState();

	UPROPERTY(EditInstanceOnly, Category = "Persistence")
	FName PersistentId;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Archetype")
	FGameplayTag EnemyArchetype;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Archetype")
	FText ThreatName;

	UPROPERTY(VisibleAnywhere, Category = "Enemy")
	TObjectPtr<UStaticMeshComponent> GreyboxBody;

	UPROPERTY(VisibleAnywhere, Category = "Enemy")
	TObjectPtr<UStaticMeshComponent> FacingMarker;

	UPROPERTY(VisibleAnywhere, Category = "Enemy")
	TObjectPtr<UPointLightComponent> StateLight;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy", meta = (ClampMin = "1.0"))
	float MaxHealth = 120.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Torch", meta = (ClampMin = "0.0"))
	float TorchDeterrenceRangeScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Torch", meta = (ClampMin = "0.0"))
	float TorchBoundarySafetyMargin = 55.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Torch", meta = (ClampMin = "0.0"))
	float TorchBoundaryBuffer = 130.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Lantern", meta = (ClampMin = "0.01"))
	float LanternFocusSecondsToStun = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Lantern", meta = (ClampMin = "0.0"))
	float LanternFocusDecayPerSecond = 0.18f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Lantern", meta = (ClampMin = "0.01"))
	float LanternFocusStunDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 125.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.0"))
	float AttackDamage = 16.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.01"))
	float AttackInterval = 1.15f;

	FLinearColor IdleStateColor = FLinearColor(0.18f, 0.04f, 0.02f);
	FLinearColor HuntingStateColor = FLinearColor(0.95f, 0.015f, 0.005f);
	FLinearColor InvestigatingStateColor = FLinearColor(0.95f, 0.28f, 0.02f);
	FLinearColor RepelledStateColor = FLinearColor(0.2f, 0.55f, 1.0f);
	FLinearColor StunnedStateColor = FLinearColor(0.72f, 0.92f, 1.0f);


private:
	UPROPERTY(VisibleInstanceOnly, Category = "Enemy")
	float Health = 120.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Enemy")
	FGameplayTag BehaviorState;
	uint64 AppliedVisibilityAuthorityRevision = 0;
	bool bVisibleBySightWeaveAuthority = true;
};
