// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "Inventory/DarkwellInventoryComponent.h"
#include "DarkwellSaveGame.generated.h"

USTRUCT()
struct DARKWELL_API FDarkwellInventorySaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	int32 SlotCapacity = 0;

	UPROPERTY(SaveGame)
	TArray<FDarkwellItemStack> Slots;
};

USTRUCT()
struct DARKWELL_API FDarkwellPlayerSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FTransform Transform = FTransform::Identity;

	UPROPERTY(SaveGame)
	float Health = 100.0f;

	UPROPERTY(SaveGame)
	FGameplayTag LifeState;

	UPROPERTY(SaveGame)
	FGameplayTag CompletionState;

	UPROPERTY(SaveGame)
	FDarkwellInventorySaveData Inventory;

	UPROPERTY(SaveGame)
	int32 LoadedShells = 2;

	UPROPERTY(SaveGame)
	float TorchCharge = 100.0f;

	UPROPERTY(SaveGame)
	float TorchHeat = 0.0f;

	UPROPERTY(SaveGame)
	float LanternFuel = 100.0f;

	UPROPERTY(SaveGame)
	bool bTorchOn = false;

	UPROPERTY(SaveGame)
	FGameplayTag EquippedLeftHandItem;

	UPROPERTY(SaveGame)
	FGameplayTag EquippedRightHandItem;
};

USTRUCT()
struct DARKWELL_API FDarkwellContainerSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FName PersistentId;

	UPROPERTY(SaveGame)
	FDarkwellInventorySaveData Inventory;
};

USTRUCT()
struct DARKWELL_API FDarkwellWorldPickupSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FName PersistentId;

	UPROPERTY(SaveGame)
	int32 Quantity = 0;
};

USTRUCT()
struct DARKWELL_API FDarkwellEnemySaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FName PersistentId;

	UPROPERTY(SaveGame)
	FTransform Transform = FTransform::Identity;

	UPROPERTY(SaveGame)
	float Health = 0.0f;

	UPROPERTY(SaveGame)
	FGameplayTag BehaviorState;

	UPROPERTY(SaveGame)
	bool bAlive = true;
};

/** Versioned, offline continuation data for one DARKWELL save slot. */
UCLASS()
class DARKWELL_API UDarkwellSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 MinimumSupportedVersion = 1;
	static constexpr int32 CurrentVersion = 3;

	UPROPERTY(SaveGame)
	int32 SaveVersion = CurrentVersion;

	UPROPERTY(SaveGame)
	FString MapName;

	UPROPERTY(SaveGame)
	FDateTime SavedAtUtc;

	UPROPERTY(SaveGame)
	FDarkwellPlayerSaveData Player;

	UPROPERTY(SaveGame)
	FGameplayTag MissionState;

	UPROPERTY(SaveGame)
	FGameplayTag DoorState;

	UPROPERTY(SaveGame)
	TArray<FDarkwellContainerSaveData> Containers;

	UPROPERTY(SaveGame)
	TArray<FDarkwellWorldPickupSaveData> WorldPickups;

	UPROPERTY(SaveGame)
	TArray<FDarkwellEnemySaveData> Enemies;

	static bool IsSupportedVersion(int32 Version)
	{
		return Version >= MinimumSupportedVersion && Version <= CurrentVersion;
	}
};
