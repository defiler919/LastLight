// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DarkwellSaveSubsystem.generated.h"

class UDarkwellSaveGame;
class USaveGame;

/** Captures, writes, reloads, and restores the single-player continuation slot. */
UCLASS()
class DARKWELL_API UDarkwellSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static const FString& GetContinueSlotName();

	bool RequestSave(UWorld& World);
	bool RequestAutosave(UWorld& World, FName Reason);
	bool RequestLoad(UWorld& World);
	bool RequestNewGame(UWorld& World);
	bool RequestReturnToMainMenu(UWorld& World);
	bool HasContinueSave() const;
	bool IsOperationInProgress() const { return bOperationInProgress; }
	bool ShouldShowMainMenuOnWorldStart() const { return !bEnterGameplayOnNextWorld; }
	bool HasPendingLoad() const { return PendingLoad != nullptr; }

	/** Called once per newly started gameplay world before default actors are spawned. */
	void BeginWorldSession(UWorld& World);
	/** Applies a loaded save after the game mode has spawned the native prototype actors. */
	bool ApplyPendingLoad(UWorld& World);
	/** Clears the one-shot world transition intent after native actors have spawned. */
	void CompleteWorldStart();

	void RegisterWorldPickup(FName PersistentId, int32 InitialQuantity);
	void UpdateWorldPickupQuantity(FName PersistentId, int32 Quantity);
	bool TryGetWorldPickupQuantity(FName PersistentId, int32& OutQuantity) const;

	FText GetStatusMessage() const;

private:
	bool BeginSave(UWorld& World, bool bAutosave, FName Reason);
	UDarkwellSaveGame* CaptureCurrentGame(UWorld& World) const;
	void HandleSaveCompleted(const FString& SlotName, int32 UserIndex, bool bSuccess);
	void HandleLoadCompleted(const FString& SlotName, int32 UserIndex, USaveGame* LoadedGameData);
	void SetStatus(const FText& Message, double DurationSeconds = 2.5);

	UPROPERTY(Transient)
	TObjectPtr<UDarkwellSaveGame> PendingLoad;

	TWeakObjectPtr<UWorld> ActiveWorld;
	TMap<FName, int32> CurrentPickupQuantities;
	FText StatusMessage;
	double StatusExpiresAtPlatformSeconds = 0.0;
	bool bOperationInProgress = false;
	bool bCurrentSaveIsAutosave = false;
	bool bEnterGameplayOnNextWorld = false;
	FName ActiveSaveReason;
};
