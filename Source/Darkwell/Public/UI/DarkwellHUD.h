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
	ADarkwellHUD();
	virtual void Tick(float DeltaSeconds) override;
	virtual void DrawHUD() override;
	bool HandleMenuPointer(const FVector2D& ScreenPosition);
	bool HandleInventoryPointer(const FVector2D& ScreenPosition, bool bSecondaryClick, bool bControlDown);

private:
	void UpdateFogOfWar(
		class ADarkwellCharacter& Character,
		const FIntPoint& ViewportSize,
		float DeltaSeconds);
	bool EnsureFogComposite(class ADarkwellCharacter& Character);
	void SetFogCompositeWeight(class ADarkwellCharacter* Character, float Weight);
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

	UPROPERTY(Transient)
	TObjectPtr<class UTexture2D> FogTexture;

	UPROPERTY(EditDefaultsOnly, Category = "Fog")
	TObjectPtr<class UMaterialInterface> FogCompositeMaterial;

	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> FogCompositeMID;

	TWeakObjectPtr<class UCameraComponent> FogCompositeCamera;

	TArray<FColor> FogTexturePixels;
	TArray<float> FogRememberedCoverage;
	TArray<float> FogRememberedScratch;
	TArray<float> FogOcclusionRanges;
	int32 FogTextureWidth = 0;
	int32 FogTextureHeight = 0;
	float FogUpdateTimeRemaining = 0.0f;
	FVector FogMemoryWorldCorners[4]{};
	uint64 FogMemoryRevision = ~uint64(0);
	bool bFogMemoryProjectionValid = false;
};
