// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/DarkwellFogSubject.h"
#include "Interaction/DarkwellInteractable.h"
#include "DarkwellScrapPickup.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/** Stackable crafting-material pickup for the inventory and workbench loop. */
UCLASS()
class DARKWELL_API ADarkwellScrapPickup : public AActor, public IDarkwellInteractable, public IDarkwellFogSubject
{
	GENERATED_BODY()

public:
	ADarkwellScrapPickup();
	virtual void BeginPlay() override;
	void ConfigurePickup(FName InPersistentId);
	FName GetPersistentId() const { return PersistentId; }
	int32 GetRemainingQuantity() const { return ScrapQuantity; }
	bool IsPickupPresentationVisible() const { return bFogPresentationVisible; }
	void RestoreRemainingQuantity(int32 Quantity);

	virtual bool CanInteract(const ADarkwellCharacter& Character) const override;
	virtual void Interact(ADarkwellCharacter& Character) override;
	virtual FText GetInteractionPrompt(const ADarkwellCharacter& Character) const override;
	virtual void OnInteractionFocusChanged(bool bFocused) override;
	virtual void SetPlayerFogState(EDarkwellFogCellState NewState) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UPointLightComponent> PickupLight;

	UPROPERTY(EditDefaultsOnly, Category = "Pickup", meta = (ClampMin = "1"))
	int32 ScrapQuantity = 4;

	UPROPERTY(EditInstanceOnly, Category = "Persistence")
	FName PersistentId;

	bool bFogPresentationVisible = false;
};
