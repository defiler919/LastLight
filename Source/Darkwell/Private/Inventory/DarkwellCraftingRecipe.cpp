// Copyright Epic Games, Inc. All Rights Reserved.

#include "Inventory/DarkwellCraftingRecipe.h"

const FPrimaryAssetType UDarkwellCraftingRecipe::PrimaryAssetType(TEXT("DarkwellRecipe"));

FPrimaryAssetId UDarkwellCraftingRecipe::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, GetFName());
}
