// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "Player/DarkwellWeaponWheelRules.h"
#include "DarkwellCharacter.generated.h"

class UCameraComponent;
class UDarkwellInteractionComponent;
class UDarkwellInventoryComponent;
class UDarkwellLoadoutComponent;
class UDarkwellVisibilityComponent;
class UInputAction;
class UInputMappingContext;
class UPointLightComponent;
class USpotLightComponent;
class USpringArmComponent;
class UStaticMeshComponent;
struct FInputActionValue;

/** Native player pawn for the DARKWELL greybox prototype. */
UCLASS()
class DARKWELL_API ADarkwellCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADarkwellCharacter();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator,
		AActor* DamageCauser) override;

	/** Turns the character toward a world-space point while keeping the body upright. */
	void AimAtWorldPoint(const FVector& WorldPoint);
	void UpdateInteractionFocus(AActor* Candidate);
	void UpdateInteractionFocusAtPoint(AActor* Candidate, const FVector& FocusWorldPoint);
	void UpdateWeaponWheelInput(bool bLeftDown, bool bRightDown);

	UDarkwellLoadoutComponent* GetLoadoutComponent() const { return LoadoutComponent; }
	UDarkwellInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
	UDarkwellInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }
	UDarkwellVisibilityComponent* GetVisibilityComponent() const { return VisibilityComponent; }
	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	float GetDamageFeedbackAlpha() const;
	bool IsAlive() const;
	bool HasEscaped() const;
	bool CanAcceptGameplayInput() const;
	bool IsInventoryOpen() const;
	bool IsSprinting() const;
	FGameplayTag GetMovementState() const { return MovementState; }
	void CompleteEscape();
	void RestorePersistentState(
		const FTransform& SavedTransform,
		float SavedHealth,
		FGameplayTag SavedLifeState,
		FGameplayTag SavedCompletionState);
	void GrantLoadProtection(float DurationSeconds);
	FGameplayTag GetLifeState() const { return LifeState; }
	FGameplayTag GetCompletionState() const { return CompletionState; }
	EDarkwellWeaponWheelSide GetActiveWeaponWheel() const { return ActiveWeaponWheel; }
	bool IsWeaponWheelOpen() const { return ActiveWeaponWheel != EDarkwellWeaponWheelSide::None; }

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void AddDefaultInputMapping();
	void MoveForward(const FInputActionValue& Value);
	void MoveBackward(const FInputActionValue& Value);
	void MoveLeft(const FInputActionValue& Value);
	void MoveRight(const FInputActionValue& Value);
	void MoveAlongCameraAxes(float ForwardAmount, float RightAmount);
	FVector GetRequestedMovementDirection() const;
	void BeginSprint(const FInputActionValue& Value);
	void EndSprint(const FInputActionValue& Value);
	void UpdateFacingAndMovement(float DeltaTime);
	void Interact(const FInputActionValue& Value);
	void BeginUseRightHand(const FInputActionValue& Value);
	void EndUseRightHand(const FInputActionValue& Value);
	void FireShotgun(const FInputActionValue& Value);
	void ReloadShotgun(const FInputActionValue& Value);
	void ToggleBackpack(const FInputActionValue& Value);
	void TakeAllInventory(const FInputActionValue& Value);
	void HandleDeath();

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> TopDownCamera;

	UPROPERTY(VisibleAnywhere, Category = "Greybox")
	TObjectPtr<UStaticMeshComponent> GreyboxBody;

	UPROPERTY(VisibleAnywhere, Category = "Greybox")
	TObjectPtr<UStaticMeshComponent> AimMarker;

	UPROPERTY(VisibleAnywhere, Category = "Greybox")
	TObjectPtr<UStaticMeshComponent> ShotgunMesh;

	UPROPERTY(VisibleAnywhere, Category = "Greybox")
	TObjectPtr<UStaticMeshComponent> TorchMesh;

	UPROPERTY(VisibleAnywhere, Category = "Equipment")
	TObjectPtr<USpotLightComponent> TorchLight;

	UPROPERTY(VisibleAnywhere, Category = "Greybox")
	TObjectPtr<UStaticMeshComponent> LanternMesh;

	UPROPERTY(VisibleAnywhere, Category = "Equipment")
	TObjectPtr<UPointLightComponent> LanternBaseLight;

	UPROPERTY(VisibleAnywhere, Category = "Equipment")
	TObjectPtr<USpotLightComponent> LanternFocusLight;

	UPROPERTY(VisibleAnywhere, Category = "Gameplay")
	TObjectPtr<UDarkwellInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, Category = "Gameplay")
	TObjectPtr<UDarkwellLoadoutComponent> LoadoutComponent;

	UPROPERTY(VisibleAnywhere, Category = "Gameplay")
	TObjectPtr<UDarkwellInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, Category = "Gameplay")
	TObjectPtr<UDarkwellVisibilityComponent> VisibilityComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveForwardAction;

	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveBackwardAction;

	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveLeftAction;

	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveRightAction;

	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> UseRightHandAction;

	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LeftWeaponWheelAction;

	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> RightWeaponWheelAction;

	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ReloadAction;

	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> BackpackAction;

	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> TakeAllAction;

	UPROPERTY(VisibleDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(VisibleInstanceOnly, Category = "Aim")
	FVector CurrentAimPoint = FVector::ZeroVector;
	bool bHasAimPoint = false;
	bool bLeftWeaponWheelWasDown = false;
	bool bRightWeaponWheelWasDown = false;
	bool bSprintRequested = false;
	bool bMoveForwardRequested = false;
	bool bMoveBackwardRequested = false;
	bool bMoveLeftRequested = false;
	bool bMoveRightRequested = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Movement")
	FGameplayTag MovementState;

	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "1.0"))
	float WalkSpeed = 430.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "1.0"))
	float SprintSpeed = 650.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "1.0"))
	float WalkTurnRateDegreesPerSecond = 240.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "1.0"))
	float SprintTurnRateDegreesPerSecond = 165.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Equipment")
	EDarkwellWeaponWheelSide ActiveWeaponWheel = EDarkwellWeaponWheelSide::None;

	UPROPERTY(EditDefaultsOnly, Category = "Survival", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Survival")
	float Health = 100.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Survival")
	FGameplayTag LifeState;

	UPROPERTY(VisibleInstanceOnly, Category = "Mission")
	FGameplayTag CompletionState;

	UPROPERTY(EditDefaultsOnly, Category = "Survival", meta = (ClampMin = "0.0"))
	float DamageInvulnerabilitySeconds = 0.65f;

	UPROPERTY(EditDefaultsOnly, Category = "Survival", meta = (ClampMin = "0.0"))
	float DamageFeedbackDurationSeconds = 0.45f;

	double InvulnerableUntilTimeSeconds = 0.0;
	double LastDamageTimeSeconds = -1.0;
};
