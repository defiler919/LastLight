// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/DarkwellInteractable.h"
#include "DarkwellAmmoPickup.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/** Cursor-focused shell pickup used by the greybox resource loop. */
UCLASS()
class DARKWELL_API ADarkwellAmmoPickup : public AActor, public IDarkwellInteractable
{
	GENERATED_BODY()

public:
	ADarkwellAmmoPickup();
	virtual void BeginPlay() override;
	void ConfigurePickup(FName InPersistentId);
	FName GetPersistentId() const { return PersistentId; }
	int32 GetRemainingQuantity() const { return ShellCount; }
	void RestoreRemainingQuantity(int32 Quantity);

	virtual bool CanInteract(const ADarkwellCharacter& Character) const override;
	virtual void Interact(ADarkwellCharacter& Character) override;
	virtual FText GetInteractionPrompt(const ADarkwellCharacter& Character) const override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UPointLightComponent> PickupLight;

	UPROPERTY(EditDefaultsOnly, Category = "Pickup", meta = (ClampMin = "1"))
	int32 ShellCount = 4;

	UPROPERTY(EditInstanceOnly, Category = "Persistence")
	FName PersistentId;
};
