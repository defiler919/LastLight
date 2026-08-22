// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Interaction/DarkwellInteractable.h"
#include "DarkwellExitGate.generated.h"

class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;

/** Mission exit that remains locked until the generator fuse is collected. */
UCLASS()
class DARKWELL_API ADarkwellExitGate : public AActor, public IDarkwellInteractable
{
	GENERATED_BODY()

public:
	ADarkwellExitGate();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual bool CanInteract(const ADarkwellCharacter& Character) const override;
	virtual void Interact(ADarkwellCharacter& Character) override;
	virtual FText GetInteractionPrompt(const ADarkwellCharacter& Character) const override;

private:
	void HandleMissionStateChanged(FGameplayTag NewState);
	void RefreshPresentation();

	UPROPERTY(VisibleAnywhere, Category = "Mission")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Mission")
	TObjectPtr<UStaticMeshComponent> ExitPanel;

	UPROPERTY(VisibleAnywhere, Category = "Mission")
	TObjectPtr<UPointLightComponent> StatusLight;
};
