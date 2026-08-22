// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Inventory/DarkwellItemDefinition.h"

class UTexture2D;

namespace Darkwell::ItemCatalog
{
	struct FItemSpec
	{
		FGameplayTag ItemTag;
		FText DisplayName;
		FText ShortName;
		FText Description;
		EDarkwellItemCategory Category = EDarkwellItemCategory::Miscellaneous;
		int32 MaxStack = 1;
		FLinearColor Color = FLinearColor::White;
		TSoftObjectPtr<UTexture2D> Icon;
	};

	/** Resolves scanned item-definition assets with native fallbacks for safe startup and tests. */
	DARKWELL_API const FItemSpec& GetSpec(FGameplayTag ItemTag);
	DARKWELL_API int32 GetMaxStack(FGameplayTag ItemTag);
	DARKWELL_API void Refresh();
}
