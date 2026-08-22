// Copyright Epic Games, Inc. All Rights Reserved.

#include "Inventory/DarkwellItemDefinition.h"

const FPrimaryAssetType UDarkwellItemDefinition::PrimaryAssetType(TEXT("DarkwellItem"));

FPrimaryAssetId UDarkwellItemDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, GetFName());
}
