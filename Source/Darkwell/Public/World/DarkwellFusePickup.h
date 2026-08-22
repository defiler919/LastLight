// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/DarkwellInteractable.h"
#include "DarkwellFusePickup.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/** Greybox mission pickup that powers the emergency exit. */
UCLASS()
class DARKWELL_API ADarkwellFusePickup : public AActor, public IDarkwellInteractable
{
	GENERATED_BODY()

public:
	ADarkwellFusePickup();

	virtual bool CanInteract(const ADarkwellCharacter& Character) const override;
	virtual void Interact(ADarkwellCharacter& Character) override;
	virtual FText GetInteractionPrompt(const ADarkwellCharacter& Character) const override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Mission")
	TObjectPtr<UStaticMeshComponent> FuseMesh;

	UPROPERTY(VisibleAnywhere, Category = "Mission")
	TObjectPtr<UPointLightComponent> FuseLight;
};
