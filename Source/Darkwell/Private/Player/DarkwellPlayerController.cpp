// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/DarkwellPlayerController.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Inventory/DarkwellInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Player/DarkwellCharacter.h"
#include "Player/DarkwellPlayerMath.h"
#include "Save/DarkwellSaveSubsystem.h"
#include "UI/DarkwellHUD.h"
#include "World/DarkwellStorageContainer.h"
#include "World/DarkwellWorkbench.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "HAL/IConsoleManager.h"

ADarkwellPlayerController::ADarkwellPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bShouldPerformFullTickWhenPaused = true;
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	DefaultMouseCursor = EMouseCursor::Crosshairs;
}

void ADarkwellPlayerController::BeginPlay()
{
	Super::BeginPlay();
	ApplyGameplayInputMode();
	const UDarkwellSaveSubsystem* SaveSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UDarkwellSaveSubsystem>()
		: nullptr;
	if (!Darkwell::PropLab::IsLabWorld(GetWorld())
		&& (!SaveSubsystem || SaveSubsystem->ShouldShowMainMenuOnWorldStart()))
	{
		OpenMenu(EDarkwellMenuScreen::Main);
	}
	else
	{
		CloseMenu();
	}
}

void ADarkwellPlayerController::ApplyGameplayInputMode()
{
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockOnCapture);
	SetInputMode(InputMode);
}

void ADarkwellPlayerController::ApplyMenuInputMode()
{
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void ADarkwellPlayerController::PlayerTick(const float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (UpdateMenuInput())
	{
		return;
	}
	if (UpdateSaveLoadInput())
	{
		return;
	}
	if (UpdateDeathRestartInput())
	{
		return;
	}
	if (UpdateInventoryInput())
	{
		return;
	}

	UpdateWeaponWheelInput();
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (Darkwell::PropLab::IsLabWorld(GetWorld()))
	{
		const auto* Route = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Darkwell.ProjectFogVisual.LabRoute"));
		if (Route && Route->GetInt() != 0) return;
	}
#endif
	UpdateAimFromCursor();
}

bool ADarkwellPlayerController::UpdateMenuInput()
{
	return IsMenuOpen();
}

bool ADarkwellPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	const bool bHandledBySuper = Super::InputKey(Params);
	if (Params.Event != IE_Pressed)
	{
		return bHandledBySuper;
	}

	if (Params.Key == EKeys::Escape)
	{
		if (MenuScreen == EDarkwellMenuScreen::Settings)
		{
			OpenMenu(SettingsReturnScreen);
		}
		else if (MenuScreen == EDarkwellMenuScreen::Pause)
		{
			CloseMenu();
		}
		else if (MenuScreen == EDarkwellMenuScreen::Main)
		{
			// The front-end intentionally has no implicit escape destination.
		}
		else if (bInventoryOpen)
		{
			CloseInventory();
		}
		else
		{
			OpenMenu(EDarkwellMenuScreen::Pause);
		}
		return true;
	}

	if (Params.Key == EKeys::Enter && IsMenuOpen())
	{
		if (MenuScreen == EDarkwellMenuScreen::Main)
		{
			ExecuteMenuAction(IsMenuActionEnabled(EDarkwellMenuAction::ContinueGame)
				? EDarkwellMenuAction::ContinueGame
				: EDarkwellMenuAction::NewGame);
		}
		else if (MenuScreen == EDarkwellMenuScreen::Pause)
		{
			ExecuteMenuAction(EDarkwellMenuAction::ResumeGame);
		}
		return true;
	}

	if (Params.Key == EKeys::LeftMouseButton && IsMenuOpen())
	{
		float MouseX = 0.0f;
		float MouseY = 0.0f;
		if (GetMousePosition(MouseX, MouseY))
		{
			if (ADarkwellHUD* DarkwellHUD = Cast<ADarkwellHUD>(GetHUD()))
			{
				DarkwellHUD->HandleMenuPointer(FVector2D(MouseX, MouseY));
			}
		}
		return true;
	}
	return bHandledBySuper;
}

void ADarkwellPlayerController::OpenMenu(const EDarkwellMenuScreen Screen)
{
	if (Screen == EDarkwellMenuScreen::None)
	{
		CloseMenu();
		return;
	}
	CloseInventory();
	MenuScreen = Screen;
	bShowMouseCursor = true;
	ApplyMenuInputMode();
	SetPause(true);
}

void ADarkwellPlayerController::CloseMenu()
{
	MenuScreen = EDarkwellMenuScreen::None;
	bShowMouseCursor = true;
	SetPause(false);
	ApplyGameplayInputMode();
}

bool ADarkwellPlayerController::IsMenuActionEnabled(const EDarkwellMenuAction Action) const
{
	const UDarkwellSaveSubsystem* SaveSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UDarkwellSaveSubsystem>()
		: nullptr;
	if (Action == EDarkwellMenuAction::ContinueGame || Action == EDarkwellMenuAction::LoadGame)
	{
		return SaveSubsystem && SaveSubsystem->HasContinueSave() && !SaveSubsystem->IsOperationInProgress();
	}
	if (Action == EDarkwellMenuAction::SaveGame)
	{
		const ADarkwellCharacter* PlayerCharacter = Cast<ADarkwellCharacter>(GetPawn());
		return SaveSubsystem && !SaveSubsystem->IsOperationInProgress() && PlayerCharacter && PlayerCharacter->IsAlive();
	}
	return true;
}

FText ADarkwellPlayerController::GetDisplayModeText() const
{
	const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings)
	{
		return NSLOCTEXT("Darkwell", "DisplayModeUnknown", "DISPLAY MODE");
	}
	switch (Settings->GetFullscreenMode())
	{
	case EWindowMode::Fullscreen:
		return NSLOCTEXT("Darkwell", "DisplayModeFullscreen", "DISPLAY: FULLSCREEN");
	case EWindowMode::Windowed:
		return NSLOCTEXT("Darkwell", "DisplayModeWindowed", "DISPLAY: WINDOWED");
	case EWindowMode::WindowedFullscreen:
	default:
		return NSLOCTEXT("Darkwell", "DisplayModeBorderless", "DISPLAY: BORDERLESS");
	}
}

void ADarkwellPlayerController::ExecuteMenuAction(const EDarkwellMenuAction Action)
{
	if (!IsMenuActionEnabled(Action))
	{
		return;
	}
	UDarkwellSaveSubsystem* SaveSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UDarkwellSaveSubsystem>()
		: nullptr;
	UWorld* World = GetWorld();
	switch (Action)
	{
	case EDarkwellMenuAction::NewGame:
		CloseMenu();
		if (SaveSubsystem && World && !SaveSubsystem->RequestNewGame(*World))
		{
			OpenMenu(EDarkwellMenuScreen::Main);
		}
		break;
	case EDarkwellMenuAction::ContinueGame:
	case EDarkwellMenuAction::LoadGame:
		CloseMenu();
		if (SaveSubsystem && World && !SaveSubsystem->RequestLoad(*World))
		{
			OpenMenu(Action == EDarkwellMenuAction::ContinueGame ? EDarkwellMenuScreen::Main : EDarkwellMenuScreen::Pause);
		}
		break;
	case EDarkwellMenuAction::ResumeGame:
		CloseMenu();
		break;
	case EDarkwellMenuAction::SaveGame:
		if (SaveSubsystem && World)
		{
			SaveSubsystem->RequestSave(*World);
		}
		break;
	case EDarkwellMenuAction::Settings:
		SettingsReturnScreen = MenuScreen == EDarkwellMenuScreen::Pause
			? EDarkwellMenuScreen::Pause
			: EDarkwellMenuScreen::Main;
		OpenMenu(EDarkwellMenuScreen::Settings);
		break;
	case EDarkwellMenuAction::CycleDisplayMode:
		if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
		{
			EWindowMode::Type NextMode = EWindowMode::WindowedFullscreen;
			if (Settings->GetFullscreenMode() == EWindowMode::WindowedFullscreen)
			{
				NextMode = EWindowMode::Fullscreen;
			}
			else if (Settings->GetFullscreenMode() == EWindowMode::Fullscreen)
			{
				NextMode = EWindowMode::Windowed;
			}
			Settings->SetFullscreenMode(NextMode);
			Settings->ApplySettings(false);
			Settings->SaveSettings();
		}
		break;
	case EDarkwellMenuAction::Back:
		OpenMenu(SettingsReturnScreen);
		break;
	case EDarkwellMenuAction::ReturnToMainMenu:
		CloseMenu();
		if (SaveSubsystem && World && !SaveSubsystem->RequestReturnToMainMenu(*World))
		{
			OpenMenu(EDarkwellMenuScreen::Pause);
		}
		break;
	case EDarkwellMenuAction::Quit:
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
		break;
	default:
		break;
	}
}

bool ADarkwellPlayerController::UpdateSaveLoadInput()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const bool bQuickSaveDown = IsInputKeyDown(EKeys::F5);
	const bool bQuickLoadDown = IsInputKeyDown(EKeys::F9);
	const bool bQuickSavePressed = bQuickSaveDown && !bQuickSaveKeyWasDown;
	const bool bQuickLoadPressed = bQuickLoadDown && !bQuickLoadKeyWasDown;
	bQuickSaveKeyWasDown = bQuickSaveDown;
	bQuickLoadKeyWasDown = bQuickLoadDown;

	UDarkwellSaveSubsystem* SaveSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UDarkwellSaveSubsystem>()
		: nullptr;
	if (!SaveSubsystem || !GetWorld())
	{
		return false;
	}

	if (bQuickLoadPressed)
	{
		CloseInventory();
		SaveSubsystem->RequestLoad(*GetWorld());
		return true;
	}

	const ADarkwellCharacter* PlayerCharacter = Cast<ADarkwellCharacter>(GetPawn());
	if (bQuickSavePressed && PlayerCharacter && PlayerCharacter->CanAcceptGameplayInput())
	{
		CloseInventory();
		SaveSubsystem->RequestSave(*GetWorld());
		return true;
	}
	return false;
#endif
}

void ADarkwellPlayerController::OpenBackpack()
{
	ADarkwellCharacter* PlayerCharacter = Cast<ADarkwellCharacter>(GetPawn());
	if (!PlayerCharacter || !PlayerCharacter->CanAcceptGameplayInput())
	{
		return;
	}

	bInventoryOpen = true;
	ClearInventorySlotSelection();
	CloseActiveContainer();
	ExternalInventory.Reset();
	ActiveWorkbench.Reset();
	InventoryContextTitle = NSLOCTEXT("Darkwell", "BackpackTitle", "Backpack");
	PlayerCharacter->UpdateInteractionFocus(nullptr);
}

void ADarkwellPlayerController::OpenContainer(
	UDarkwellInventoryComponent* ContainerInventory,
	const FText& ContainerTitle)
{
	ADarkwellCharacter* PlayerCharacter = Cast<ADarkwellCharacter>(GetPawn());
	if (!PlayerCharacter || !PlayerCharacter->CanAcceptGameplayInput() || !IsValid(ContainerInventory))
	{
		return;
	}

	bInventoryOpen = true;
	ClearInventorySlotSelection();
	CloseActiveContainer();
	ExternalInventory = ContainerInventory;
	ActiveWorkbench.Reset();
	InventoryContextTitle = ContainerTitle;
	if (ADarkwellStorageContainer* Container = Cast<ADarkwellStorageContainer>(ContainerInventory->GetOwner()))
	{
		Container->SetContainerOpen(true);
	}
	PlayerCharacter->UpdateInteractionFocus(nullptr);
}

void ADarkwellPlayerController::OpenWorkbench(ADarkwellWorkbench* Workbench)
{
	ADarkwellCharacter* PlayerCharacter = Cast<ADarkwellCharacter>(GetPawn());
	if (!PlayerCharacter || !PlayerCharacter->CanAcceptGameplayInput() || !IsValid(Workbench))
	{
		return;
	}

	bInventoryOpen = true;
	ClearInventorySlotSelection();
	CloseActiveContainer();
	ExternalInventory.Reset();
	ActiveWorkbench = Workbench;
	InventoryContextTitle = NSLOCTEXT("Darkwell", "WorkbenchTitle", "Shell workbench");
	PlayerCharacter->UpdateInteractionFocus(nullptr);
}

void ADarkwellPlayerController::CloseInventory()
{
	bInventoryOpen = false;
	ClearInventorySlotSelection();
	CloseActiveContainer();
	ExternalInventory.Reset();
	ActiveWorkbench.Reset();
	InventoryContextTitle = FText::GetEmpty();
}

void ADarkwellPlayerController::CloseActiveContainer()
{
	if (UDarkwellInventoryComponent* ContainerInventory = ExternalInventory.Get())
	{
		if (ADarkwellStorageContainer* Container = Cast<ADarkwellStorageContainer>(ContainerInventory->GetOwner()))
		{
			Container->SetContainerOpen(false);
		}
	}
}

bool ADarkwellPlayerController::TakeAllFromContainer()
{
	ADarkwellCharacter* PlayerCharacter = Cast<ADarkwellCharacter>(GetPawn());
	UDarkwellInventoryComponent* PlayerInventory = PlayerCharacter ? PlayerCharacter->GetInventoryComponent() : nullptr;
	UDarkwellInventoryComponent* ContainerInventory = ExternalInventory.Get();
	if (!PlayerInventory || !ContainerInventory)
	{
		return false;
	}

	const int32 Moved = ContainerInventory->TransferAllTo(*PlayerInventory);
	if (Moved > 0)
	{
		SetInventoryMessage(FText::Format(
			NSLOCTEXT("Darkwell", "TakeAllMoved", "Took {0} items"),
			FText::AsNumber(Moved)));
		CloseInventory();
	}
	else
	{
		SetInventoryMessage(NSLOCTEXT("Darkwell", "TakeAllBlocked", "Nothing could be transferred"));
	}
	return Moved > 0;
}

bool ADarkwellPlayerController::TryCraftAtWorkbench()
{
	ADarkwellCharacter* PlayerCharacter = Cast<ADarkwellCharacter>(GetPawn());
	ADarkwellWorkbench* Workbench = ActiveWorkbench.Get();
	if (!PlayerCharacter || !Workbench)
	{
		return false;
	}

	const bool bCrafted = Workbench->TryCraftShells(*PlayerCharacter);
	if (bCrafted && GetWorld())
	{
		if (UDarkwellSaveSubsystem* SaveSubsystem = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UDarkwellSaveSubsystem>()
			: nullptr)
		{
			SaveSubsystem->RequestAutosave(*GetWorld(), FName(TEXT("WorkbenchCrafted")));
		}
	}
	SetInventoryMessage(bCrafted
		? FText::Format(
			NSLOCTEXT("Darkwell", "CraftedShells", "Crafted {0} shotgun shells"),
			FText::AsNumber(Workbench->GetShellYield()))
		: NSLOCTEXT("Darkwell", "CraftingBlocked", "Need scrap and enough backpack space"));
	return bCrafted;
}

void ADarkwellPlayerController::HandleInventoryClick(const bool bSecondaryClick)
{
	if (!bInventoryOpen)
	{
		return;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return;
	}

	if (ADarkwellHUD* DarkwellHUD = Cast<ADarkwellHUD>(GetHUD()))
	{
		const bool bControlDown = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
		DarkwellHUD->HandleInventoryPointer(FVector2D(MouseX, MouseY), bSecondaryClick, bControlDown);
	}
}

bool ADarkwellPlayerController::SelectOrMoveInventorySlot(
	UDarkwellInventoryComponent* Inventory,
	const int32 SlotIndex)
{
	if (!bInventoryOpen || !IsValid(Inventory) || !Inventory->GetSlot(SlotIndex))
	{
		return false;
	}

	if (!SelectedInventory.IsValid())
	{
		const FDarkwellItemStack* Stack = Inventory->GetSlot(SlotIndex);
		if (!Stack || Stack->IsEmpty())
		{
			return false;
		}

		SelectedInventory = Inventory;
		SelectedInventorySlot = SlotIndex;
		SetInventoryMessage(NSLOCTEXT("Darkwell", "InventoryStackSelected", "Stack selected; choose a slot"));
		return true;
	}

	if (SelectedInventory.Get() != Inventory)
	{
		ClearInventorySlotSelection();
		return SelectOrMoveInventorySlot(Inventory, SlotIndex);
	}

	if (SelectedInventorySlot == SlotIndex)
	{
		ClearInventorySlotSelection();
		SetInventoryMessage(NSLOCTEXT("Darkwell", "InventoryMoveCancelled", "Move cancelled"));
		return true;
	}

	const int32 SourceSlotIndex = SelectedInventorySlot;
	ClearInventorySlotSelection();
	const bool bMoved = Inventory->MoveSlotWithinInventory(SourceSlotIndex, SlotIndex);
	SetInventoryMessage(bMoved
		? NSLOCTEXT("Darkwell", "InventoryMoveComplete", "Stack moved")
		: NSLOCTEXT("Darkwell", "InventoryMoveBlocked", "Stack could not be moved"));
	return bMoved;
}

bool ADarkwellPlayerController::IsInventorySlotSelected(
	const UDarkwellInventoryComponent* Inventory,
	const int32 SlotIndex) const
{
	return SelectedInventory.Get() == Inventory && SelectedInventorySlot == SlotIndex;
}

void ADarkwellPlayerController::ClearInventorySlotSelection()
{
	SelectedInventory.Reset();
	SelectedInventorySlot = INDEX_NONE;
}

void ADarkwellPlayerController::SetInventoryMessage(const FText& Message)
{
	InventoryMessage = Message;
	InventoryMessageExpiresAt = GetWorld() ? GetWorld()->GetTimeSeconds() + 2.2 : 0.0;
}

FText ADarkwellPlayerController::GetInventoryMessage() const
{
	return GetWorld() && GetWorld()->GetTimeSeconds() <= InventoryMessageExpiresAt
		? InventoryMessage
		: FText::GetEmpty();
}

bool ADarkwellPlayerController::UpdateDeathRestartInput()
{
	const bool bRestartKeyDown = IsInputKeyDown(EKeys::R);
	const ADarkwellCharacter* DarkwellCharacter = Cast<ADarkwellCharacter>(GetPawn());
	const bool bShouldRestart = DarkwellCharacter
		&& !DarkwellCharacter->CanAcceptGameplayInput()
		&& bRestartKeyDown
		&& !bRestartKeyWasDown;
	bRestartKeyWasDown = bRestartKeyDown;

	if (bShouldRestart)
	{
		RequestRestartCurrentLevel();
		return true;
	}

	return false;
}

bool ADarkwellPlayerController::UpdateInventoryInput()
{
	const bool bEscapeDown = IsInputKeyDown(EKeys::Escape);
	const bool bEscapePressed = bEscapeDown && !bEscapeKeyWasDown;
	bEscapeKeyWasDown = bEscapeDown;

	ADarkwellCharacter* PlayerCharacter = Cast<ADarkwellCharacter>(GetPawn());
	if (!PlayerCharacter || !PlayerCharacter->CanAcceptGameplayInput())
	{
		CloseInventory();
		return false;
	}

	if (!bInventoryOpen)
	{
		return false;
	}

	UpdateInventoryContext();
	if (!bInventoryOpen)
	{
		return false;
	}

	PlayerCharacter->UpdateInteractionFocus(nullptr);
	PlayerCharacter->UpdateWeaponWheelInput(false, false);
	if (bEscapePressed)
	{
		CloseInventory();
		return true;
	}
	return true;
}

void ADarkwellPlayerController::UpdateInventoryContext()
{
	const ADarkwellCharacter* PlayerCharacter = Cast<ADarkwellCharacter>(GetPawn());
	const AActor* ContextActor = ActiveWorkbench.IsValid()
		? static_cast<const AActor*>(ActiveWorkbench.Get())
		: (ExternalInventory.IsValid() ? ExternalInventory->GetOwner() : nullptr);
	if (ContextActor && PlayerCharacter
		&& FVector::DistSquared(ContextActor->GetActorLocation(), PlayerCharacter->GetActorLocation()) > FMath::Square(380.0f))
	{
		CloseInventory();
	}
}

void ADarkwellPlayerController::RequestRestartCurrentLevel()
{
	if (bRestartRequested)
	{
		return;
	}

	bRestartRequested = true;
	const FString LevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	if (LevelName.IsEmpty())
	{
		bRestartRequested = false;
		return;
	}

	UGameplayStatics::OpenLevel(this, FName(*LevelName));
}

void ADarkwellPlayerController::UpdateWeaponWheelInput()
{
	if (ADarkwellCharacter* DarkwellCharacter = Cast<ADarkwellCharacter>(GetPawn()))
	{
		DarkwellCharacter->UpdateWeaponWheelInput(
			IsInputKeyDown(EKeys::Q),
			IsInputKeyDown(EKeys::E));
	}
}

void ADarkwellPlayerController::UpdateAimFromCursor()
{
	ADarkwellCharacter* DarkwellCharacter = Cast<ADarkwellCharacter>(GetPawn());
	if (!DarkwellCharacter || !IsLocalController())
	{
		return;
	}

	if (!DarkwellCharacter->CanAcceptGameplayInput())
	{
		DarkwellCharacter->UpdateInteractionFocus(nullptr);
		return;
	}

	if (DarkwellCharacter->IsWeaponWheelOpen())
	{
		DarkwellCharacter->UpdateInteractionFocus(nullptr);
		return;
	}

	FVector RayOrigin;
	FVector RayDirection;
	if (!DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return;
	}

	FVector AimPoint;
	FHitResult HitResult;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DarkwellCursorAim), false, DarkwellCharacter);
	constexpr double AimTraceDistance = 100000.0;
	const FVector TraceEnd = RayOrigin + RayDirection * AimTraceDistance;
	if (GetWorld()->LineTraceSingleByChannel(
		HitResult,
		RayOrigin,
		TraceEnd,
		ECC_Visibility,
		QueryParams))
	{
		AimPoint = HitResult.ImpactPoint;
	}
	else if (!Darkwell::PlayerMath::TryIntersectHorizontalPlane(
		RayOrigin,
		RayDirection,
		DarkwellCharacter->GetActorLocation().Z,
		AimPoint))
	{
		DarkwellCharacter->UpdateInteractionFocus(nullptr);
		return;
	}
	DarkwellCharacter->RefreshFacingInteractionFocus();

	DarkwellCharacter->AimAtWorldPoint(AimPoint);
}
