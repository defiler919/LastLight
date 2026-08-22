// Copyright Epic Games, Inc. All Rights Reserved.

#include "Inventory/DarkwellItemCatalog.h"

#include "Engine/AssetManager.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "Inventory/DarkwellItemDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogDarkwellItems, Log, All);

namespace
{
	struct FDarkwellItemCatalogCache
	{
		bool bInitialized = false;
		TMap<FGameplayTag, Darkwell::ItemCatalog::FItemSpec> Specs;
	};

	FDarkwellItemCatalogCache& GetCache()
	{
		static FDarkwellItemCatalogCache Cache;
		return Cache;
	}

	Darkwell::ItemCatalog::FItemSpec MakeFallbackSpec(
		const FGameplayTag ItemTag,
		const FText& DisplayName,
		const FText& ShortName,
		const FText& Description,
		const EDarkwellItemCategory Category,
		const int32 MaxStack,
		const FLinearColor& Color)
	{
		Darkwell::ItemCatalog::FItemSpec Spec;
		Spec.ItemTag = ItemTag;
		Spec.DisplayName = DisplayName;
		Spec.ShortName = ShortName;
		Spec.Description = Description;
		Spec.Category = Category;
		Spec.MaxStack = MaxStack;
		Spec.Color = Color;
		return Spec;
	}

	void InitializeCatalog()
	{
		FDarkwellItemCatalogCache& Cache = GetCache();
		if (Cache.bInitialized)
		{
			return;
		}

		Cache.bInitialized = true;
		Cache.Specs.Add(
			DarkwellGameplayTags::Item_Ammo_ShotgunShell,
			MakeFallbackSpec(
				DarkwellGameplayTags::Item_Ammo_ShotgunShell,
				NSLOCTEXT("Darkwell", "ShotgunShellItem", "Shotgun shells"),
				NSLOCTEXT("Darkwell", "ShotgunShellShortName", "SHELLS"),
				NSLOCTEXT("Darkwell", "ShotgunShellDescription", "Two-barrel shotgun ammunition. Scarce, loud, and decisive."),
				EDarkwellItemCategory::Ammunition,
				12,
				FLinearColor(0.96f, 0.42f, 0.08f)));
		Cache.Specs.Add(
			DarkwellGameplayTags::Item_Material_Scrap,
			MakeFallbackSpec(
				DarkwellGameplayTags::Item_Material_Scrap,
				NSLOCTEXT("Darkwell", "ScrapItem", "Scrap"),
				NSLOCTEXT("Darkwell", "ScrapShortName", "SCRAP"),
				NSLOCTEXT("Darkwell", "ScrapDescription", "Salvaged metal and wire used at workbenches."),
				EDarkwellItemCategory::Material,
				20,
				FLinearColor(0.3f, 0.72f, 0.82f)));

		UAssetManager& AssetManager = UAssetManager::Get();
		TArray<FPrimaryAssetId> DefinitionIds;
		AssetManager.GetPrimaryAssetIdList(UDarkwellItemDefinition::PrimaryAssetType, DefinitionIds);
		for (const FPrimaryAssetId& DefinitionId : DefinitionIds)
		{
			const FSoftObjectPath DefinitionPath = AssetManager.GetPrimaryAssetPath(DefinitionId);
			const UDarkwellItemDefinition* Definition = Cast<UDarkwellItemDefinition>(DefinitionPath.TryLoad());
			if (!Definition || !Definition->ItemTag.IsValid())
			{
				UE_LOG(LogDarkwellItems, Warning, TEXT("Ignoring invalid item definition %s"), *DefinitionId.ToString());
				continue;
			}

			Darkwell::ItemCatalog::FItemSpec Spec;
			Spec.ItemTag = Definition->ItemTag;
			const Darkwell::ItemCatalog::FItemSpec* FallbackSpec = Cache.Specs.Find(Definition->ItemTag);
			Spec.DisplayName = Definition->DisplayName.IsEmpty()
				? (FallbackSpec ? FallbackSpec->DisplayName : FText::FromName(Definition->ItemTag.GetTagName()))
				: Definition->DisplayName;
			Spec.ShortName = Definition->ShortName.IsEmpty() ? Spec.DisplayName : Definition->ShortName;
			Spec.Description = Definition->Description.IsEmpty() && FallbackSpec
				? FallbackSpec->Description
				: Definition->Description;
			Spec.Category = Definition->Category;
			Spec.MaxStack = FMath::Max(1, Definition->MaxStack);
			Spec.Color = Definition->DisplayColor;
			Spec.Icon = Definition->Icon;
			Cache.Specs.Add(Spec.ItemTag, MoveTemp(Spec));
		}
	}
}

const Darkwell::ItemCatalog::FItemSpec& Darkwell::ItemCatalog::GetSpec(const FGameplayTag ItemTag)
{
	InitializeCatalog();
	if (const FItemSpec* Spec = GetCache().Specs.Find(ItemTag))
	{
		return *Spec;
	}

	static const FItemSpec Unknown = MakeFallbackSpec(
		FGameplayTag::EmptyTag,
		NSLOCTEXT("Darkwell", "UnknownItem", "Unknown item"),
		NSLOCTEXT("Darkwell", "UnknownItemShortName", "UNKNOWN"),
		NSLOCTEXT("Darkwell", "UnknownItemDescription", "No reliable information is available."),
		EDarkwellItemCategory::Miscellaneous,
		1,
		FLinearColor(0.45f, 0.45f, 0.45f));
	return Unknown;
}

int32 Darkwell::ItemCatalog::GetMaxStack(const FGameplayTag ItemTag)
{
	return FMath::Max(1, GetSpec(ItemTag).MaxStack);
}

void Darkwell::ItemCatalog::Refresh()
{
	FDarkwellItemCatalogCache& Cache = GetCache();
	Cache.Specs.Reset();
	Cache.bInitialized = false;
}
