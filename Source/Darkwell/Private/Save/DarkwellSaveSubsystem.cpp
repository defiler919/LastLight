// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/DarkwellSaveSubsystem.h"

#include "AI/DarkwellStalkerCharacter.h"
#include "AI/DarkwellStalkerController.h"
#include "Combat/DarkwellLoadoutComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DarkwellGameState.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "Inventory/DarkwellInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/DarkwellCharacter.h"
#include "Player/DarkwellPlayerController.h"
#include "Save/DarkwellSaveGame.h"
#include "World/DarkwellAmmoPickup.h"
#include "World/DarkwellDoor.h"
#include "World/DarkwellFusePickup.h"
#include "World/DarkwellScrapPickup.h"
#include "World/DarkwellStorageContainer.h"

DEFINE_LOG_CATEGORY_STATIC(LogDarkwellSave, Log, All);

namespace
{
	constexpr int32 SaveUserIndex = 0;
	const FString ContinueSlotName(TEXT("Darkwell_Continue"));

	FDarkwellInventorySaveData CaptureInventory(const UDarkwellInventoryComponent& Inventory)
	{
		FDarkwellInventorySaveData Data;
		Data.SlotCapacity = Inventory.GetSlotCapacity();
		Data.Slots = Inventory.GetSlots();
		return Data;
	}
}

const FString& UDarkwellSaveSubsystem::GetContinueSlotName()
{
	return ContinueSlotName;
}

bool UDarkwellSaveSubsystem::RequestSave(UWorld& World)
{
	return BeginSave(World, false, FName(TEXT("Manual")));
}

bool UDarkwellSaveSubsystem::RequestAutosave(UWorld& World, const FName Reason)
{
	if (bOperationInProgress)
	{
		UE_LOG(LogDarkwellSave, Verbose, TEXT("Skipped autosave %s because another save operation is active"), *Reason.ToString());
		return false;
	}
	return BeginSave(World, true, Reason);
}

bool UDarkwellSaveSubsystem::BeginSave(UWorld& World, const bool bAutosave, const FName Reason)
{
	if (bOperationInProgress)
	{
		SetStatus(NSLOCTEXT("Darkwell", "SaveBusy", "SAVE OPERATION IN PROGRESS"));
		return false;
	}

	UDarkwellSaveGame* SaveData = CaptureCurrentGame(World);
	if (!SaveData)
	{
		SetStatus(NSLOCTEXT("Darkwell", "SaveCaptureFailed", "QUICKSAVE FAILED"));
		return false;
	}

	bOperationInProgress = true;
	bCurrentSaveIsAutosave = bAutosave;
	ActiveSaveReason = Reason;
	SetStatus(bAutosave
		? NSLOCTEXT("Darkwell", "Autosaving", "AUTOSAVING...")
		: NSLOCTEXT("Darkwell", "Saving", "SAVING..."), 10.0);
	FAsyncSaveGameToSlotDelegate CompletionDelegate;
	CompletionDelegate.BindUObject(this, &ThisClass::HandleSaveCompleted);
	UGameplayStatics::AsyncSaveGameToSlot(SaveData, ContinueSlotName, SaveUserIndex, CompletionDelegate);
	return true;
}

bool UDarkwellSaveSubsystem::RequestLoad(UWorld& World)
{
	if (bOperationInProgress)
	{
		SetStatus(NSLOCTEXT("Darkwell", "LoadBusy", "SAVE OPERATION IN PROGRESS"));
		return false;
	}

	if (!UGameplayStatics::DoesSaveGameExist(ContinueSlotName, SaveUserIndex))
	{
		SetStatus(NSLOCTEXT("Darkwell", "NoQuickSave", "NO QUICKSAVE FOUND"));
		return false;
	}

	ActiveWorld = &World;
	bOperationInProgress = true;
	SetStatus(NSLOCTEXT("Darkwell", "Loading", "LOADING QUICKSAVE..."), 10.0);
	FAsyncLoadGameFromSlotDelegate CompletionDelegate;
	CompletionDelegate.BindUObject(this, &ThisClass::HandleLoadCompleted);
	UGameplayStatics::AsyncLoadGameFromSlot(ContinueSlotName, SaveUserIndex, CompletionDelegate);
	return true;
}

bool UDarkwellSaveSubsystem::RequestNewGame(UWorld& World)
{
	if (bOperationInProgress)
	{
		SetStatus(NSLOCTEXT("Darkwell", "NewGameBusy", "SAVE OPERATION IN PROGRESS"));
		return false;
	}

	const FString LevelName = UGameplayStatics::GetCurrentLevelName(&World, true);
	if (LevelName.IsEmpty())
	{
		return false;
	}

	UGameplayStatics::DeleteGameInSlot(ContinueSlotName, SaveUserIndex);
	PendingLoad = nullptr;
	CurrentPickupQuantities.Reset();
	bEnterGameplayOnNextWorld = true;
	SetStatus(NSLOCTEXT("Darkwell", "StartingNewGame", "STARTING NEW GAME..."), 10.0);
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*LevelName));
	return true;
}

bool UDarkwellSaveSubsystem::RequestReturnToMainMenu(UWorld& World)
{
	if (bOperationInProgress)
	{
		SetStatus(NSLOCTEXT("Darkwell", "MainMenuBusy", "SAVE OPERATION IN PROGRESS"));
		return false;
	}

	const FString LevelName = UGameplayStatics::GetCurrentLevelName(&World, true);
	if (LevelName.IsEmpty())
	{
		return false;
	}

	PendingLoad = nullptr;
	CurrentPickupQuantities.Reset();
	bEnterGameplayOnNextWorld = false;
	SetStatus(FText::GetEmpty(), 0.0);
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*LevelName));
	return true;
}

bool UDarkwellSaveSubsystem::HasContinueSave() const
{
	return UGameplayStatics::DoesSaveGameExist(ContinueSlotName, SaveUserIndex);
}

void UDarkwellSaveSubsystem::BeginWorldSession(UWorld& World)
{
	ActiveWorld = &World;
	CurrentPickupQuantities.Reset();
}

void UDarkwellSaveSubsystem::CompleteWorldStart()
{
	bEnterGameplayOnNextWorld = false;
}

bool UDarkwellSaveSubsystem::ApplyPendingLoad(UWorld& World)
{
	if (!PendingLoad)
	{
		return false;
	}

	if (!UDarkwellSaveGame::IsSupportedVersion(PendingLoad->SaveVersion))
	{
		UE_LOG(LogDarkwellSave, Error, TEXT("Cannot apply unsupported save version %d"), PendingLoad->SaveVersion);
		PendingLoad = nullptr;
		bOperationInProgress = false;
		SetStatus(NSLOCTEXT("Darkwell", "UnsupportedSave", "QUICKSAVE VERSION NOT SUPPORTED"));
		return false;
	}

	ADarkwellCharacter* Character = Cast<ADarkwellCharacter>(UGameplayStatics::GetPlayerCharacter(&World, 0));
	ADarkwellGameState* GameState = World.GetGameState<ADarkwellGameState>();
	if (!Character || !GameState)
	{
		UE_LOG(LogDarkwellSave, Error, TEXT("Cannot apply save without the DARKWELL player and game state"));
		PendingLoad = nullptr;
		bOperationInProgress = false;
		SetStatus(NSLOCTEXT("Darkwell", "RestoreFailed", "QUICKSAVE RESTORE FAILED"));
		return false;
	}

	if (ADarkwellPlayerController* PlayerController = Cast<ADarkwellPlayerController>(Character->GetController()))
	{
		PlayerController->CloseInventory();
	}

	GameState->RestoreMissionState(PendingLoad->MissionState);
	Character->GetInventoryComponent()->RestoreState(
		PendingLoad->Player.Inventory.SlotCapacity,
		PendingLoad->Player.Inventory.Slots);
	Character->GetLoadoutComponent()->RestorePersistentState(
		PendingLoad->Player.LoadedShells,
		PendingLoad->Player.TorchCharge,
		PendingLoad->SaveVersion >= 3 ? PendingLoad->Player.TorchHeat : 0.0f,
		PendingLoad->SaveVersion >= 3 ? PendingLoad->Player.LanternFuel : 100.0f,
		PendingLoad->Player.EquippedLeftHandItem,
		PendingLoad->Player.EquippedRightHandItem);
	Character->RestorePersistentState(
		PendingLoad->Player.Transform,
		PendingLoad->Player.Health,
		PendingLoad->Player.LifeState,
		PendingLoad->Player.CompletionState);
	Character->GrantLoadProtection(3.0f);

	TMap<FName, const FDarkwellContainerSaveData*> ContainerDataById;
	for (const FDarkwellContainerSaveData& ContainerData : PendingLoad->Containers)
	{
		if (!ContainerData.PersistentId.IsNone())
		{
			ContainerDataById.Add(ContainerData.PersistentId, &ContainerData);
		}
	}
	for (TActorIterator<ADarkwellStorageContainer> It(&World); It; ++It)
	{
		if (const FDarkwellContainerSaveData* const* Data = ContainerDataById.Find(It->GetPersistentId()))
		{
			It->GetInventoryComponent()->RestoreState((*Data)->Inventory.SlotCapacity, (*Data)->Inventory.Slots);
		}
	}

	for (TActorIterator<ADarkwellDoor> It(&World); It; ++It)
	{
		It->RestoreDoorState(PendingLoad->DoorState);
		break;
	}

	TMap<FName, const FDarkwellEnemySaveData*> EnemyDataById;
	for (const FDarkwellEnemySaveData& EnemyData : PendingLoad->Enemies)
	{
		if (!EnemyData.PersistentId.IsNone())
		{
			EnemyDataById.Add(EnemyData.PersistentId, &EnemyData);
		}
	}
	for (TActorIterator<ADarkwellStalkerCharacter> It(&World); It; ++It)
	{
		if (const FDarkwellEnemySaveData* const* Data = EnemyDataById.Find(It->GetPersistentId()))
		{
			It->RestorePersistentState(
				(*Data)->Transform,
				(*Data)->Health,
				(*Data)->BehaviorState,
				(*Data)->bAlive);
		}
		if (ADarkwellStalkerController* Controller = Cast<ADarkwellStalkerController>(It->GetController()))
		{
			Controller->ApplyLoadSafetyGrace(3.0f);
		}
	}

	CurrentPickupQuantities.Reset();
	for (const FDarkwellWorldPickupSaveData& PickupData : PendingLoad->WorldPickups)
	{
		if (!PickupData.PersistentId.IsNone())
		{
			CurrentPickupQuantities.Add(PickupData.PersistentId, FMath::Max(0, PickupData.Quantity));
		}
	}
	for (TActorIterator<ADarkwellAmmoPickup> It(&World); It; ++It)
	{
		int32 Quantity = 0;
		if (TryGetWorldPickupQuantity(It->GetPersistentId(), Quantity))
		{
			It->RestoreRemainingQuantity(Quantity);
		}
	}
	for (TActorIterator<ADarkwellScrapPickup> It(&World); It; ++It)
	{
		int32 Quantity = 0;
		if (TryGetWorldPickupQuantity(It->GetPersistentId(), Quantity))
		{
			It->RestoreRemainingQuantity(Quantity);
		}
	}

	if (GameState->IsFuseCollected())
	{
		for (TActorIterator<ADarkwellFusePickup> It(&World); It; ++It)
		{
			It->Destroy();
		}
	}

	UE_LOG(LogDarkwellSave, Display, TEXT("Applied save version %d from %s"), PendingLoad->SaveVersion, *PendingLoad->SavedAtUtc.ToString());
	PendingLoad = nullptr;
	bOperationInProgress = false;
	SetStatus(NSLOCTEXT("Darkwell", "QuickSaveLoaded", "QUICKSAVE LOADED"));
	return true;
}

void UDarkwellSaveSubsystem::RegisterWorldPickup(const FName PersistentId, const int32 InitialQuantity)
{
	if (!PersistentId.IsNone() && !CurrentPickupQuantities.Contains(PersistentId))
	{
		CurrentPickupQuantities.Add(PersistentId, FMath::Max(0, InitialQuantity));
	}
}

void UDarkwellSaveSubsystem::UpdateWorldPickupQuantity(const FName PersistentId, const int32 Quantity)
{
	if (!PersistentId.IsNone())
	{
		CurrentPickupQuantities.Add(PersistentId, FMath::Max(0, Quantity));
	}
}

bool UDarkwellSaveSubsystem::TryGetWorldPickupQuantity(const FName PersistentId, int32& OutQuantity) const
{
	if (const int32* Quantity = CurrentPickupQuantities.Find(PersistentId))
	{
		OutQuantity = *Quantity;
		return true;
	}
	return false;
}

FText UDarkwellSaveSubsystem::GetStatusMessage() const
{
	return FPlatformTime::Seconds() <= StatusExpiresAtPlatformSeconds ? StatusMessage : FText::GetEmpty();
}

UDarkwellSaveGame* UDarkwellSaveSubsystem::CaptureCurrentGame(UWorld& World) const
{
	const ADarkwellCharacter* Character = Cast<ADarkwellCharacter>(UGameplayStatics::GetPlayerCharacter(&World, 0));
	const ADarkwellGameState* GameState = World.GetGameState<ADarkwellGameState>();
	const UDarkwellLoadoutComponent* Loadout = Character ? Character->GetLoadoutComponent() : nullptr;
	const UDarkwellInventoryComponent* Inventory = Character ? Character->GetInventoryComponent() : nullptr;
	if (!Character || !GameState || !Loadout || !Inventory)
	{
		return nullptr;
	}

	UDarkwellSaveGame* SaveData = Cast<UDarkwellSaveGame>(UGameplayStatics::CreateSaveGameObject(UDarkwellSaveGame::StaticClass()));
	if (!SaveData)
	{
		return nullptr;
	}

	SaveData->SaveVersion = UDarkwellSaveGame::CurrentVersion;
	SaveData->MapName = UGameplayStatics::GetCurrentLevelName(&World, true);
	SaveData->SavedAtUtc = FDateTime::UtcNow();
	SaveData->MissionState = GameState->GetMissionState();
	SaveData->Player.Transform = Character->GetActorTransform();
	SaveData->Player.Health = Character->GetHealth();
	SaveData->Player.LifeState = Character->GetLifeState();
	SaveData->Player.CompletionState = Character->GetCompletionState();
	SaveData->Player.Inventory = CaptureInventory(*Inventory);
	SaveData->Player.LoadedShells = Loadout->GetLoadedShells();
	SaveData->Player.TorchCharge = Loadout->GetTorchCharge();
	SaveData->Player.TorchHeat = Loadout->GetTorchHeat();
	SaveData->Player.LanternFuel = Loadout->GetLanternFuel();
	SaveData->Player.bTorchOn = Loadout->IsTorchOn();
	SaveData->Player.EquippedLeftHandItem = Loadout->GetEquippedLeftHandItem();
	SaveData->Player.EquippedRightHandItem = Loadout->GetEquippedRightHandItem();

	for (TActorIterator<ADarkwellStorageContainer> It(&World); It; ++It)
	{
		if (It->GetPersistentId().IsNone())
		{
			UE_LOG(LogDarkwellSave, Warning, TEXT("Skipping storage container without a persistent ID: %s"), *It->GetName());
			continue;
		}

		FDarkwellContainerSaveData& ContainerData = SaveData->Containers.AddDefaulted_GetRef();
		ContainerData.PersistentId = It->GetPersistentId();
		ContainerData.Inventory = CaptureInventory(*It->GetInventoryComponent());
	}
	SaveData->Containers.Sort([](const FDarkwellContainerSaveData& A, const FDarkwellContainerSaveData& B)
	{
		return A.PersistentId.LexicalLess(B.PersistentId);
	});

	for (TActorIterator<ADarkwellDoor> It(&World); It; ++It)
	{
		SaveData->DoorState = It->GetDoorState();
		break;
	}

	for (const TPair<FName, int32>& PickupPair : CurrentPickupQuantities)
	{
		FDarkwellWorldPickupSaveData& PickupData = SaveData->WorldPickups.AddDefaulted_GetRef();
		PickupData.PersistentId = PickupPair.Key;
		PickupData.Quantity = FMath::Max(0, PickupPair.Value);
	}
	SaveData->WorldPickups.Sort([](const FDarkwellWorldPickupSaveData& A, const FDarkwellWorldPickupSaveData& B)
	{
		return A.PersistentId.LexicalLess(B.PersistentId);
	});

	for (TActorIterator<ADarkwellStalkerCharacter> It(&World); It; ++It)
	{
		if (It->GetPersistentId().IsNone())
		{
			UE_LOG(LogDarkwellSave, Warning, TEXT("Skipping enemy without a persistent ID: %s"), *It->GetName());
			continue;
		}

		FDarkwellEnemySaveData& EnemyData = SaveData->Enemies.AddDefaulted_GetRef();
		EnemyData.PersistentId = It->GetPersistentId();
		EnemyData.Transform = It->GetActorTransform();
		EnemyData.Health = It->GetHealth();
		EnemyData.BehaviorState = It->GetBehaviorState();
		EnemyData.bAlive = It->IsAlive();
	}
	SaveData->Enemies.Sort([](const FDarkwellEnemySaveData& A, const FDarkwellEnemySaveData& B)
	{
		return A.PersistentId.LexicalLess(B.PersistentId);
	});
	return SaveData;
}

void UDarkwellSaveSubsystem::HandleSaveCompleted(const FString& SlotName, const int32 UserIndex, const bool bSuccess)
{
	bOperationInProgress = false;
	const bool bWasAutosave = bCurrentSaveIsAutosave;
	bCurrentSaveIsAutosave = false;
	SetStatus(bSuccess
		? (bWasAutosave
			? NSLOCTEXT("Darkwell", "AutosaveComplete", "AUTOSAVE COMPLETE")
			: NSLOCTEXT("Darkwell", "QuickSaveComplete", "SAVE COMPLETE"))
		: NSLOCTEXT("Darkwell", "QuickSaveFailed", "SAVE FAILED"));
	if (bSuccess)
	{
		UE_LOG(
			LogDarkwellSave,
			Display,
			TEXT("%s slot %s completed successfully (reason: %s)"),
			bWasAutosave ? TEXT("Autosave") : TEXT("Save"),
			*SlotName,
			*ActiveSaveReason.ToString());
	}
	else
	{
		UE_LOG(LogDarkwellSave, Error, TEXT("Save slot %s failed"), *SlotName);
	}
}

void UDarkwellSaveSubsystem::HandleLoadCompleted(
	const FString& SlotName,
	const int32 UserIndex,
	USaveGame* LoadedGameData)
{
	UDarkwellSaveGame* LoadedSave = Cast<UDarkwellSaveGame>(LoadedGameData);
	if (!LoadedSave || !UDarkwellSaveGame::IsSupportedVersion(LoadedSave->SaveVersion) || LoadedSave->MapName.IsEmpty())
	{
		bOperationInProgress = false;
		SetStatus(LoadedSave
			? NSLOCTEXT("Darkwell", "UnsupportedQuickSave", "QUICKSAVE VERSION NOT SUPPORTED")
			: NSLOCTEXT("Darkwell", "QuickLoadFailed", "QUICKLOAD FAILED"));
		return;
	}

	PendingLoad = LoadedSave;
	bEnterGameplayOnNextWorld = true;
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*PendingLoad->MapName));
}

void UDarkwellSaveSubsystem::SetStatus(const FText& Message, const double DurationSeconds)
{
	StatusMessage = Message;
	StatusExpiresAtPlatformSeconds = FPlatformTime::Seconds() + FMath::Max(0.0, DurationSeconds);
}
