// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Interaction/DarkwellInteractable.h"
#include "DarkwellWorkbench.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
class UDarkwellCraftingRecipe;

/** Shell-crafting station that opens alongside the player's backpack. */
UCLASS()
class DARKWELL_API ADarkwellWorkbench : public AActor, public IDarkwellInteractable
{
	GENERATED_BODY()

public:
	ADarkwellWorkbench();

	bool TryCraftShells(ADarkwellCharacter& Character) const;
	const UDarkwellCraftingRecipe* GetRecipeDefinition() const;
	FText GetRecipeDisplayName() const;
	FGameplayTag GetCostItemTag() const;
	FGameplayTag GetResultItemTag() const;
	int32 GetScrapCost() const;
	int32 GetShellYield() const;

	virtual bool CanInteract(const ADarkwellCharacter& Character) const override;
	virtual void Interact(ADarkwellCharacter& Character) override;
	virtual FText GetInteractionPrompt(const ADarkwellCharacter& Character) const override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Workbench")
	TObjectPtr<UStaticMeshComponent> WorkbenchMesh;

	UPROPERTY(VisibleAnywhere, Category = "Workbench")
	TObjectPtr<UPointLightComponent> WorkbenchLight;

	UPROPERTY(EditDefaultsOnly, Category = "Crafting")
	TSoftObjectPtr<UDarkwellCraftingRecipe> RecipeDefinition;

	UPROPERTY(EditDefaultsOnly, Category = "Crafting|Fallback", meta = (ClampMin = "1"))
	int32 FallbackScrapCost = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Crafting|Fallback", meta = (ClampMin = "1"))
	int32 FallbackShellYield = 2;
};
