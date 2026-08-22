// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DarkwellPlayerController.generated.h"

class ADarkwellWorkbench;
class UDarkwellInventoryComponent;

UENUM()
enum class EDarkwellMenuScreen : uint8
{
	None,
	Main,
	Pause,
	Settings
};

UENUM()
enum class EDarkwellMenuAction : uint8
{
	NewGame,
	ContinueGame,
	ResumeGame,
	SaveGame,
	LoadGame,
	Settings,
	CycleDisplayMode,
	Back,
	ReturnToMainMenu,
	Quit
};

/** Player controller responsible for mouse-to-world aiming in the top-down prototype. */
UCLASS()
class DARKWELL_API ADarkwellPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADarkwellPlayerController();
	void RequestRestartCurrentLevel();
	void OpenBackpack();
	void OpenContainer(UDarkwellInventoryComponent* ContainerInventory, const FText& ContainerTitle);
	void OpenWorkbench(ADarkwellWorkbench* Workbench);
	void CloseInventory();
	bool IsInventoryOpen() const { return bInventoryOpen; }
	UDarkwellInventoryComponent* GetExternalInventory() const { return ExternalInventory.Get(); }
	ADarkwellWorkbench* GetActiveWorkbench() const { return ActiveWorkbench.Get(); }
	const FText& GetInventoryContextTitle() const { return InventoryContextTitle; }
	bool TakeAllFromContainer();
	bool TryCraftAtWorkbench();
	void HandleInventoryClick(bool bSecondaryClick);
	bool SelectOrMoveInventorySlot(UDarkwellInventoryComponent* Inventory, int32 SlotIndex);
	bool IsInventorySlotSelected(const UDarkwellInventoryComponent* Inventory, int32 SlotIndex) const;
	void ClearInventorySlotSelection();
	void SetInventoryMessage(const FText& Message);
	FText GetInventoryMessage() const;
	bool IsMenuOpen() const { return MenuScreen != EDarkwellMenuScreen::None; }
	EDarkwellMenuScreen GetMenuScreen() const { return MenuScreen; }
	bool IsMenuActionEnabled(EDarkwellMenuAction Action) const;
	FText GetDisplayModeText() const;
	void ExecuteMenuAction(EDarkwellMenuAction Action);

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual bool InputKey(const struct FInputKeyEventArgs& Params) override;

private:
	bool UpdateMenuInput();
	bool UpdateSaveLoadInput();
	bool UpdateDeathRestartInput();
	bool UpdateInventoryInput();
	void UpdateInventoryContext();
	void UpdateWeaponWheelInput();
	void UpdateAimFromCursor();
	void OpenMenu(EDarkwellMenuScreen Screen);
	void CloseMenu();
	void ApplyGameplayInputMode();
	void ApplyMenuInputMode();
	void CloseActiveContainer();

	bool bRestartKeyWasDown = false;
	bool bRestartRequested = false;
	bool bQuickSaveKeyWasDown = false;
	bool bQuickLoadKeyWasDown = false;
	UPROPERTY(VisibleInstanceOnly, Category = "Menu")
	EDarkwellMenuScreen MenuScreen = EDarkwellMenuScreen::None;
	UPROPERTY(VisibleInstanceOnly, Category = "Menu")
	EDarkwellMenuScreen SettingsReturnScreen = EDarkwellMenuScreen::Main;
	UPROPERTY(VisibleInstanceOnly, Category = "Inventory")
	bool bInventoryOpen = false;
	bool bEscapeKeyWasDown = false;

	TWeakObjectPtr<UDarkwellInventoryComponent> ExternalInventory;
	TWeakObjectPtr<UDarkwellInventoryComponent> SelectedInventory;
	int32 SelectedInventorySlot = INDEX_NONE;
	TWeakObjectPtr<ADarkwellWorkbench> ActiveWorkbench;
	UPROPERTY(VisibleInstanceOnly, Category = "Inventory")
	FText InventoryContextTitle;
	FText InventoryMessage;
	double InventoryMessageExpiresAt = 0.0;
};
