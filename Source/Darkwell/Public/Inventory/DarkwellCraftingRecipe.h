// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DarkwellCraftingRecipe.generated.h"

/** One atomic inventory exchange offered by a workbench. */
UCLASS(BlueprintType)
class DARKWELL_API UDarkwellCraftingRecipe : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType PrimaryAssetType;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cost", meta = (Categories = "Item"))
	FGameplayTag CostItemTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cost", meta = (ClampMin = "1"))
	int32 CostQuantity = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Result", meta = (Categories = "Item"))
	FGameplayTag ResultItemTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Result", meta = (ClampMin = "1"))
	int32 ResultQuantity = 1;
};
