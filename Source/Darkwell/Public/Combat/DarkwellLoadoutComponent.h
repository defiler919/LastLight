// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "DarkwellLoadoutComponent.generated.h"

class USpotLightComponent;
class UPointLightComponent;
class UStaticMeshComponent;

enum class EDarkwellLightPressureKind : uint8
{
	None,
	TorchDeterrent,
	LanternFocus
};

/** Native runtime rules for the right-hand torch and left-hand double-barrel shotgun. */
UCLASS(ClassGroup = (Darkwell), meta = (BlueprintSpawnableComponent))
class DARKWELL_API UDarkwellLoadoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDarkwellLoadoutComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetRightHandPresentation(
		UStaticMeshComponent* InTorchMesh,
		UPointLightComponent* InTorchLight,
		UStaticMeshComponent* InLanternMesh,
		UPointLightComponent* InLanternBaseLight,
		USpotLightComponent* InLanternFocusLight);
	bool BeginRightHandUse();
	bool EndRightHandUse();
	void CancelRightHandUse();
	bool CycleRightHandItem();
	bool EquipRightHandItem(FGameplayTag ItemTag);
	bool TryFire(const FVector& AimPoint, float AimProgress = 0.0f);
	bool BeginReload();
	void DeactivateForOwnerIncapacitated();
	int32 AddReserveShells(int32 Amount);
	void RestorePersistentState(
		int32 SavedLoadedShells,
		float SavedTorchCharge,
		float SavedTorchHeat,
		float SavedLanternFuel,
		FGameplayTag SavedLeftHandItem,
		FGameplayTag SavedRightHandItem);

	int32 GetLoadedShells() const { return LoadedShells; }
	int32 GetReserveShells() const;
	float GetTorchCharge() const { return TorchCharge; }
	float GetTorchHeat() const { return TorchHeat; }
	float GetLanternFuel() const { return LanternFuel; }
	bool IsTorchOn() const;
	bool IsTorchDeterrentActive() const;
	bool IsTorchSwinging() const;
	bool IsTorchOverheated() const { return bTorchOverheated; }
	bool IsLanternOn() const;
	bool IsLanternFocused() const;
	bool IsLanternFlashActive() const;
	float GetLanternFlashCooldownRemaining() const { return LanternFlashCooldownRemaining; }
	EDarkwellLightPressureKind GetActiveLightPressure(float& OutRange, float& OutFacingThreshold) const;
	bool IsReloading() const;
	FGameplayTag GetEquippedLeftHandItem() const { return EquippedLeftHandItem; }
	FGameplayTag GetEquippedRightHandItem() const { return EquippedRightHandItem; }
	const FGameplayTagContainer& GetRuntimeStates() const { return RuntimeStates; }

private:
	void CompleteReload();
	void TickRightHandTool(float DeltaTime);
	void TriggerTorchSwing();
	void TriggerLanternFlash();
	void RefreshRightHandPresentation();
	void ClearRightHandActionStates();
	void RefreshWeaponState();

	UPROPERTY(EditDefaultsOnly, Category = "Shotgun", meta = (ClampMin = "1"))
	int32 MaxLoadedShells = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Shotgun", meta = (ClampMin = "0"))
	int32 LoadedShells = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Shotgun", meta = (ClampMin = "0"))
	int32 StartingReserveShells = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Shotgun", meta = (ClampMin = "0.0"))
	float ReloadDuration = 1.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Shotgun", meta = (ClampMin = "1"))
	int32 PelletCount = 8;

	UPROPERTY(EditDefaultsOnly, Category = "Shotgun", meta = (ClampMin = "0.0"))
	float PelletDamage = 18.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Shotgun", meta = (ClampMin = "0.0"))
	float ShotRange = 2600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Shotgun", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float HipFireSpreadHalfAngleDegrees = 9.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Shotgun", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float AimedSpreadHalfAngleDegrees = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Torch", meta = (ClampMin = "0.0"))
	float TorchCharge = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Torch", meta = (ClampMin = "0.0"))
	float TorchDrainPerSecond = 0.45f;

	UPROPERTY(EditDefaultsOnly, Category = "Torch", meta = (ClampMin = "0.0"))
	float TorchDeterrentExtraDrainPerSecond = 1.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Torch", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float TorchHeat = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Torch", meta = (ClampMin = "0.0"))
	float TorchSwingHeat = 22.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Torch", meta = (ClampMin = "0.0"))
	float TorchHoldHeatPerSecond = 27.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Torch", meta = (ClampMin = "0.0"))
	float TorchCoolPerSecond = 16.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Torch", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float TorchOverheatRecoveryThreshold = 45.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Torch|Swing", meta = (ClampMin = "0.01"))
	float TorchSwingDuration = 0.38f;

	UPROPERTY(EditDefaultsOnly, Category = "Torch|Swing", meta = (ClampMin = "0.0"))
	float TorchSwingRange = 245.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Torch|Swing", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float TorchSwingHalfAngleDegrees = 62.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Torch|Swing", meta = (ClampMin = "0.0"))
	float TorchSwingDamage = 12.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Torch|Swing", meta = (ClampMin = "0.0"))
	float TorchSwingControlDuration = 0.55f;

	UPROPERTY(EditDefaultsOnly, Category = "Torch|Deterrent", meta = (ClampMin = "0.0"))
	float TorchDeterrentRange = 820.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Torch|Deterrent", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float TorchDeterrentHalfAngleDegrees = 48.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Torch|Reload", meta = (ClampMin = "0.0"))
	float ReloadTorchIntensity = 2600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Torch|Reload", meta = (ClampMin = "0.0"))
	float ReloadTorchRadius = 320.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Torch|Reload")
	FVector ReloadTorchRelativeLocation = FVector(0.0f, 0.0f, 115.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Lantern", meta = (ClampMin = "0.0"))
	float LanternFuel = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Lantern", meta = (ClampMin = "0.0"))
	float LanternBaseDrainPerSecond = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "Lantern|Focus", meta = (ClampMin = "0.0"))
	float LanternFocusExtraDrainPerSecond = 1.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Lantern|Focus", meta = (ClampMin = "0.0"))
	float LanternFocusRange = 1750.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Lantern|Focus", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float LanternFocusHalfAngleDegrees = 16.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Lantern|Flash", meta = (ClampMin = "0.0"))
	float LanternFlashFuelCost = 9.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Lantern|Flash", meta = (ClampMin = "0.01"))
	float LanternFlashDuration = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Lantern|Flash", meta = (ClampMin = "0.0"))
	float LanternFlashCooldown = 2.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Lantern|Flash", meta = (ClampMin = "0.0"))
	float LanternFlashRange = 1350.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Lantern|Flash", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float LanternFlashHalfAngleDegrees = 52.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Lantern|Flash", meta = (ClampMin = "0.0"))
	float LanternFlashControlDuration = 1.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Right Hand", meta = (ClampMin = "0.01"))
	float RightHandHoldThreshold = 0.28f;

	UPROPERTY(VisibleInstanceOnly, Category = "State")
	FGameplayTagContainer RuntimeStates;

	UPROPERTY(VisibleInstanceOnly, Category = "Equipment")
	FGameplayTag EquippedLeftHandItem;

	UPROPERTY(VisibleInstanceOnly, Category = "Equipment")
	FGameplayTag EquippedRightHandItem;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> TorchLight;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> TorchMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> LanternMesh;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> LanternBaseLight;

	UPROPERTY(Transient)
	TObjectPtr<USpotLightComponent> LanternFocusLight;

	FVector RaisedTorchRelativeLocation = FVector::ZeroVector;
	float RaisedTorchIntensity = 0.0f;
	float RaisedTorchRadius = 0.0f;
	bool bHasRaisedTorchPresentation = false;
	FVector RaisedTorchMeshRelativeLocation = FVector::ZeroVector;
	FRotator RaisedTorchMeshRelativeRotation = FRotator::ZeroRotator;

	float ReloadTimeRemaining = 0.0f;
	float RightHandHeldSeconds = -1.0f;
	float TorchSwingTimeRemaining = 0.0f;
	float LanternFlashTimeRemaining = 0.0f;
	float LanternFlashCooldownRemaining = 0.0f;
	bool bHoldActionStarted = false;
	bool bTorchOverheated = false;
	bool bOwnerIncapacitated = false;
};
