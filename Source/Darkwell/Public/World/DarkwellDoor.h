// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Interaction/DarkwellInteractable.h"
#include "DarkwellDoor.generated.h"

class USceneComponent;
class UPointLightComponent;
class UStaticMeshComponent;

/** Greybox swinging door driven by native interaction and gameplay-tag state. */
UCLASS()
class DARKWELL_API ADarkwellDoor : public AActor, public IDarkwellInteractable
{
	GENERATED_BODY()

public:
	ADarkwellDoor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract(const ADarkwellCharacter& Character) const override;
	virtual void Interact(ADarkwellCharacter& Character) override;
	virtual FText GetInteractionPrompt(const ADarkwellCharacter& Character) const override;

	const FGameplayTag& GetDoorState() const { return DoorState; }
	bool RestoreDoorState(FGameplayTag SavedDoorState);

private:
	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<USceneComponent> DoorHinge;

	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<UStaticMeshComponent> DoorPanel;

	UPROPERTY(VisibleAnywhere, Category = "Door")
	TObjectPtr<UPointLightComponent> PassageLight;

	UPROPERTY(EditDefaultsOnly, Category = "Door", meta = (ClampMin = "1.0", ClampMax = "179.0"))
	float OpenAngle = 95.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Door", meta = (ClampMin = "1.0"))
	float DegreesPerSecond = 150.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Door")
	FGameplayTag DoorState;

	void SetDoorOpen(bool bShouldOpen);
	void RefreshPassageLight() const;

	float TargetYaw = 0.0f;
};
