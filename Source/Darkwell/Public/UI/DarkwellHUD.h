// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DarkwellHUD.generated.h"

/** Minimal native HUD for greybox controls, resources, and interaction prompts. */
UCLASS()
class DARKWELL_API ADarkwellHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	bool HandleMenuPointer(const FVector2D& ScreenPosition);
	bool HandleInventoryPointer(const FVector2D& ScreenPosition, bool bSecondaryClick, bool bControlDown);

private:
	void DrawMenuInterface();
	void DrawInventoryInterface();
	void DrawInventoryPanel(
		const FVector2D& Origin,
		const FVector2D& Size,
		const FText& Title,
		const class UDarkwellInventoryComponent& Inventory,
		bool bPlayerPanel);
	void DrawInventorySlot(
		const FVector2D& Origin,
		int32 SlotIndex,
		const struct FDarkwellItemStack& Stack,
		bool bSelected);
	FVector2D GetBackpackPanelOrigin(bool bDualPanel) const;
	FVector2D GetContextPanelOrigin() const;
	bool FindInventorySlotAt(
		const FVector2D& ScreenPosition,
		const FVector2D& PanelOrigin,
		int32 SlotCount,
		int32& OutSlotIndex) const;
};
