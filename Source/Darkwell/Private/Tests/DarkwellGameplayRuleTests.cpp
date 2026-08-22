// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "AI/DarkwellEnemyMath.h"
#include "AI/DarkwellStalkerCharacter.h"
#include "AI/DarkwellWardenCharacter.h"
#include "Combat/DarkwellLoadoutRules.h"
#include "Game/DarkwellMissionRules.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "Gameplay/DarkwellResourceMath.h"
#include "Gameplay/DarkwellSurvivalRules.h"
#include "Engine/AssetManager.h"
#include "Inventory/DarkwellCraftingRecipe.h"
#include "Inventory/DarkwellInventoryComponent.h"
#include "Inventory/DarkwellItemCatalog.h"
#include "Inventory/DarkwellItemDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Save/DarkwellSaveGame.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellSaveSerializationTest,
	"Darkwell.Gameplay.Save.SerializationRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellSaveSerializationTest::RunTest(const FString& Parameters)
{
	UDarkwellInventoryComponent* Inventory = NewObject<UDarkwellInventoryComponent>();
	Inventory->InitializeInventory(4);
	Inventory->AddItem(DarkwellGameplayTags::Item_Material_Scrap, 25);
	const TArray<FDarkwellItemStack> SavedSlots = Inventory->GetSlots();
	Inventory->RemoveItem(DarkwellGameplayTags::Item_Material_Scrap, 25);
	Inventory->RestoreState(4, SavedSlots);
	TestEqual(TEXT("Inventory state restores its slot capacity"), Inventory->GetSlotCapacity(), 4);
	TestEqual(
		TEXT("Inventory state restores all stacked quantities"),
		Inventory->GetTotalQuantity(DarkwellGameplayTags::Item_Material_Scrap),
		25);

	UDarkwellSaveGame* SaveData = NewObject<UDarkwellSaveGame>();
	SaveData->MapName = TEXT("L_Prototype");
	SaveData->SavedAtUtc = FDateTime(2026, 8, 22, 2, 0, 0);
	SaveData->MissionState = DarkwellGameplayTags::State_Mission_ReachExit;
	SaveData->DoorState = DarkwellGameplayTags::State_World_Door_Open;
	SaveData->Player.Transform = FTransform(FRotator(0.0f, 35.0f, 0.0f), FVector(120.0f, -80.0f, 100.0f));
	SaveData->Player.Health = 64.0f;
	SaveData->Player.LifeState = DarkwellGameplayTags::State_Player_Alive;
	SaveData->Player.Inventory.SlotCapacity = Inventory->GetSlotCapacity();
	SaveData->Player.Inventory.Slots = Inventory->GetSlots();
	SaveData->Player.LoadedShells = 1;
	SaveData->Player.TorchCharge = 37.5f;
	SaveData->Player.TorchHeat = 42.0f;
	SaveData->Player.LanternFuel = 71.0f;
	SaveData->Player.bTorchOn = true;
	SaveData->Player.EquippedLeftHandItem = DarkwellGameplayTags::Equipment_Left_Shotgun;
	SaveData->Player.EquippedRightHandItem = DarkwellGameplayTags::Equipment_Right_Torch;

	FDarkwellContainerSaveData& Container = SaveData->Containers.AddDefaulted_GetRef();
	Container.PersistentId = FName(TEXT("Container.Test"));
	Container.Inventory = SaveData->Player.Inventory;
	FDarkwellWorldPickupSaveData& Pickup = SaveData->WorldPickups.AddDefaulted_GetRef();
	Pickup.PersistentId = FName(TEXT("Pickup.Test"));
	Pickup.Quantity = 0;
	FDarkwellEnemySaveData& Enemy = SaveData->Enemies.AddDefaulted_GetRef();
	Enemy.PersistentId = FName(TEXT("Enemy.Stalker.Test"));
	Enemy.Transform = FTransform(FRotator(0.0f, -25.0f, 0.0f), FVector(-400.0f, 300.0f, 92.0f));
	Enemy.Health = 41.0f;
	Enemy.BehaviorState = DarkwellGameplayTags::State_Enemy_Investigating;
	Enemy.bAlive = true;

	TArray<uint8> SerializedData;
	TestTrue(TEXT("SaveGame serializes into memory"), UGameplayStatics::SaveGameToMemory(SaveData, SerializedData));
	UDarkwellSaveGame* LoadedData = Cast<UDarkwellSaveGame>(UGameplayStatics::LoadGameFromMemory(SerializedData));
	TestNotNull(TEXT("Serialized SaveGame loads as the DARKWELL type"), LoadedData);
	if (LoadedData)
	{
		TestTrue(TEXT("Current save version is supported"), UDarkwellSaveGame::IsSupportedVersion(LoadedData->SaveVersion));
		TestFalse(TEXT("Future save versions are rejected"), UDarkwellSaveGame::IsSupportedVersion(LoadedData->SaveVersion + 1));
		TestEqual(TEXT("Map name survives serialization"), LoadedData->MapName, FString(TEXT("L_Prototype")));
		TestEqual(TEXT("Player health survives serialization"), LoadedData->Player.Health, 64.0f);
		TestEqual(TEXT("Loaded shells survive serialization"), LoadedData->Player.LoadedShells, 1);
		TestEqual(TEXT("Torch charge survives serialization"), LoadedData->Player.TorchCharge, 37.5f);
		TestEqual(TEXT("Torch heat survives serialization"), LoadedData->Player.TorchHeat, 42.0f);
		TestEqual(TEXT("Lantern fuel survives serialization"), LoadedData->Player.LanternFuel, 71.0f);
		TestTrue(TEXT("Torch power state survives serialization"), LoadedData->Player.bTorchOn);
		TestTrue(TEXT("Mission tag survives serialization"), LoadedData->MissionState == DarkwellGameplayTags::State_Mission_ReachExit);
		TestEqual(TEXT("Container ID survives serialization"), LoadedData->Containers[0].PersistentId, FName(TEXT("Container.Test")));
		TestEqual(TEXT("Collected pickup state survives serialization"), LoadedData->WorldPickups[0].Quantity, 0);
		TestEqual(TEXT("Enemy ID survives serialization"), LoadedData->Enemies[0].PersistentId, FName(TEXT("Enemy.Stalker.Test")));
		TestEqual(TEXT("Enemy health survives serialization"), LoadedData->Enemies[0].Health, 41.0f);
		TestTrue(TEXT("Enemy behavior survives serialization"), LoadedData->Enemies[0].BehaviorState == DarkwellGameplayTags::State_Enemy_Investigating);
		TestTrue(TEXT("Enemy life state survives serialization"), LoadedData->Enemies[0].bAlive);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellSaveVersionCompatibilityTest,
	"Darkwell.Gameplay.Save.VersionCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellSaveVersionCompatibilityTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("The oldest declared save version remains loadable"),
		UDarkwellSaveGame::IsSupportedVersion(UDarkwellSaveGame::MinimumSupportedVersion));
	TestTrue(
		TEXT("The current save version remains loadable"),
		UDarkwellSaveGame::IsSupportedVersion(UDarkwellSaveGame::CurrentVersion));
	TestFalse(TEXT("Pre-release save versions are rejected"), UDarkwellSaveGame::IsSupportedVersion(0));
	TestFalse(
		TEXT("Unknown future save versions are rejected"),
		UDarkwellSaveGame::IsSupportedVersion(UDarkwellSaveGame::CurrentVersion + 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellItemDefinitionsTest,
	"Darkwell.Gameplay.Inventory.ItemDefinitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellItemDefinitionsTest::RunTest(const FString& Parameters)
{
	const UDarkwellItemDefinition* ShellDefinition = LoadObject<UDarkwellItemDefinition>(
		nullptr,
		TEXT("/Game/Data/Items/DA_Item_ShotgunShells.DA_Item_ShotgunShells"));
	const UDarkwellItemDefinition* ScrapDefinition = LoadObject<UDarkwellItemDefinition>(
		nullptr,
		TEXT("/Game/Data/Items/DA_Item_Scrap.DA_Item_Scrap"));
	const UDarkwellCraftingRecipe* ShellRecipe = LoadObject<UDarkwellCraftingRecipe>(
		nullptr,
		TEXT("/Game/Data/Recipes/DA_Recipe_ShotgunShells.DA_Recipe_ShotgunShells"));
	TestNotNull(TEXT("Shotgun shells have a data-driven item definition"), ShellDefinition);
	TestNotNull(TEXT("Scrap has a data-driven item definition"), ScrapDefinition);
	TestNotNull(TEXT("The shell workbench has a data-driven recipe"), ShellRecipe);

	if (ShellDefinition && ScrapDefinition && ShellRecipe)
	{
		TArray<FPrimaryAssetId> ItemDefinitionIds;
		UAssetManager::Get().GetPrimaryAssetIdList(UDarkwellItemDefinition::PrimaryAssetType, ItemDefinitionIds);
		TestTrue(
			TEXT("Asset Manager scans the shotgun shell definition"),
			ItemDefinitionIds.Contains(ShellDefinition->GetPrimaryAssetId()));
		TestTrue(
			TEXT("Asset Manager scans the scrap definition"),
			ItemDefinitionIds.Contains(ScrapDefinition->GetPrimaryAssetId()));

		TArray<FPrimaryAssetId> RecipeIds;
		UAssetManager::Get().GetPrimaryAssetIdList(UDarkwellCraftingRecipe::PrimaryAssetType, RecipeIds);
		TestTrue(
			TEXT("Asset Manager scans the shell workbench recipe"),
			RecipeIds.Contains(ShellRecipe->GetPrimaryAssetId()));

		TestTrue(TEXT("Shell definition owns the durable shell tag"), ShellDefinition->ItemTag == DarkwellGameplayTags::Item_Ammo_ShotgunShell);
		TestEqual(TEXT("Shell definition owns its stack limit"), ShellDefinition->MaxStack, 12);
		TestTrue(TEXT("Scrap definition owns the durable material tag"), ScrapDefinition->ItemTag == DarkwellGameplayTags::Item_Material_Scrap);
		TestEqual(TEXT("Scrap definition owns its stack limit"), ScrapDefinition->MaxStack, 20);
		TestTrue(TEXT("Recipe consumes scrap"), ShellRecipe->CostItemTag == DarkwellGameplayTags::Item_Material_Scrap);
		TestEqual(TEXT("Recipe consumes two scrap"), ShellRecipe->CostQuantity, 2);
		TestTrue(TEXT("Recipe produces shells"), ShellRecipe->ResultItemTag == DarkwellGameplayTags::Item_Ammo_ShotgunShell);
		TestEqual(TEXT("Recipe produces two shells"), ShellRecipe->ResultQuantity, 2);
	}

	Darkwell::ItemCatalog::Refresh();
	const Darkwell::ItemCatalog::FItemSpec& ShellSpec = Darkwell::ItemCatalog::GetSpec(DarkwellGameplayTags::Item_Ammo_ShotgunShell);
	const Darkwell::ItemCatalog::FItemSpec& ScrapSpec = Darkwell::ItemCatalog::GetSpec(DarkwellGameplayTags::Item_Material_Scrap);
	TestEqual(TEXT("Catalog resolves the shell stack limit"), ShellSpec.MaxStack, 12);
	TestEqual(TEXT("Catalog resolves the scrap stack limit"), ScrapSpec.MaxStack, 20);
	TestTrue(TEXT("Catalog exposes a shell short name"), !ShellSpec.ShortName.IsEmpty());
	TestTrue(TEXT("Catalog exposes a scrap short name"), !ScrapSpec.ShortName.IsEmpty());
	TestTrue(TEXT("Catalog exposes a shell description"), !ShellSpec.Description.IsEmpty());
	TestTrue(TEXT("Catalog exposes a scrap description"), !ScrapSpec.Description.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellInventoryStackingTest,
	"Darkwell.Gameplay.Inventory.StackingAndSplitting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellInventoryStackingTest::RunTest(const FString& Parameters)
{
	UDarkwellInventoryComponent* Inventory = NewObject<UDarkwellInventoryComponent>();
	Inventory->InitializeInventory(4);
	TestEqual(
		TEXT("Scrap fills its first stack and spills into the next slot"),
		Inventory->AddItem(DarkwellGameplayTags::Item_Material_Scrap, 25),
		25);
	TestEqual(TEXT("All scrap is counted across stacks"), Inventory->GetTotalQuantity(DarkwellGameplayTags::Item_Material_Scrap), 25);
	TestTrue(TEXT("A populated stack can split into a free slot"), Inventory->SplitStack(0));
	TestEqual(TEXT("Splitting preserves total quantity"), Inventory->GetTotalQuantity(DarkwellGameplayTags::Item_Material_Scrap), 25);
	TestEqual(TEXT("The split stack leaves half in the source"), Inventory->GetSlot(0)->Quantity, 10);
	TestEqual(TEXT("The larger half occupies the free slot"), Inventory->GetSlot(2)->Quantity, 10);
	TestTrue(TEXT("A stack can move into a chosen empty slot"), Inventory->MoveSlotWithinInventory(2, 3));
	TestTrue(TEXT("Matching stacks merge when moved together"), Inventory->MoveSlotWithinInventory(3, 0));
	TestEqual(TEXT("Moving preserves and merges the selected quantities"), Inventory->GetSlot(0)->Quantity, 20);

	UDarkwellInventoryComponent* SwapInventory = NewObject<UDarkwellInventoryComponent>();
	SwapInventory->InitializeInventory(2);
	SwapInventory->AddItem(DarkwellGameplayTags::Item_Material_Scrap, 1);
	SwapInventory->AddItem(DarkwellGameplayTags::Item_Ammo_ShotgunShell, 2);
	TestTrue(TEXT("Different item stacks swap slots"), SwapInventory->MoveSlotWithinInventory(0, 1));
	TestTrue(TEXT("The material stack reached its selected slot"), SwapInventory->GetSlot(1)->ItemTag == DarkwellGameplayTags::Item_Material_Scrap);
	TestTrue(TEXT("The ammunition stack reached the source slot"), SwapInventory->GetSlot(0)->ItemTag == DarkwellGameplayTags::Item_Ammo_ShotgunShell);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellInventoryTransferTest,
	"Darkwell.Gameplay.Inventory.ContainerTransfer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellInventoryTransferTest::RunTest(const FString& Parameters)
{
	UDarkwellInventoryComponent* Source = NewObject<UDarkwellInventoryComponent>();
	UDarkwellInventoryComponent* Target = NewObject<UDarkwellInventoryComponent>();
	Source->InitializeInventory(2);
	Target->InitializeInventory(1);
	Source->AddItem(DarkwellGameplayTags::Item_Material_Scrap, 15);
	TestEqual(TEXT("Container totals all item quantities"), Source->GetTotalItemCount(), 15);
	TestEqual(TEXT("A whole stack transfers to a compatible empty container"), Source->TransferSlotTo(*Target, 0), 15);
	TestEqual(TEXT("Source stack is cleared after transfer"), Source->GetTotalQuantity(DarkwellGameplayTags::Item_Material_Scrap), 0);
	TestTrue(TEXT("A looted container reports empty"), Source->IsEmpty());
	TestEqual(TEXT("Target receives the transferred stack"), Target->GetTotalQuantity(DarkwellGameplayTags::Item_Material_Scrap), 15);

	Source->AddItem(DarkwellGameplayTags::Item_Ammo_ShotgunShell, 4);
	TestEqual(TEXT("An incompatible full target rejects transfer"), Source->TransferAllTo(*Target), 0);
	TestEqual(TEXT("Rejected transfer leaves the source untouched"), Source->GetTotalQuantity(DarkwellGameplayTags::Item_Ammo_ShotgunShell), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellInventoryCraftingTransactionTest,
	"Darkwell.Gameplay.Inventory.CraftingTransaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellInventoryCraftingTransactionTest::RunTest(const FString& Parameters)
{
	UDarkwellInventoryComponent* Inventory = NewObject<UDarkwellInventoryComponent>();
	Inventory->InitializeInventory(1);
	Inventory->AddItem(DarkwellGameplayTags::Item_Material_Scrap, 2);
	TestTrue(
		TEXT("Crafting can consume a stack and reuse its freed slot atomically"),
		Inventory->TryExchange(
			DarkwellGameplayTags::Item_Material_Scrap,
			2,
			DarkwellGameplayTags::Item_Ammo_ShotgunShell,
			2));
	TestEqual(TEXT("Crafting consumed its material"), Inventory->GetTotalQuantity(DarkwellGameplayTags::Item_Material_Scrap), 0);
	TestEqual(TEXT("Crafting granted its result"), Inventory->GetTotalQuantity(DarkwellGameplayTags::Item_Ammo_ShotgunShell), 2);

	TestFalse(
		TEXT("Missing materials reject a crafting transaction"),
		Inventory->TryExchange(
			DarkwellGameplayTags::Item_Material_Scrap,
			2,
			DarkwellGameplayTags::Item_Ammo_ShotgunShell,
			2));
	TestEqual(TEXT("A rejected transaction preserves existing items"), Inventory->GetTotalQuantity(DarkwellGameplayTags::Item_Ammo_ShotgunShell), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellShotgunReloadMathTest,
	"Darkwell.Gameplay.Resources.ShotgunReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellShotgunReloadMathTest::RunTest(const FString& Parameters)
{
	int32 Loaded = 0;
	int32 Reserve = 5;
	TestEqual(TEXT("A reload fills both barrels"), Darkwell::ResourceMath::TransferShells(2, Loaded, Reserve), 2);
	TestEqual(TEXT("Two shells are loaded"), Loaded, 2);
	TestEqual(TEXT("Transferred shells leave reserve"), Reserve, 3);

	Loaded = 1;
	Reserve = 0;
	TestEqual(TEXT("An empty reserve transfers nothing"), Darkwell::ResourceMath::TransferShells(2, Loaded, Reserve), 0);
	TestEqual(TEXT("The existing loaded shell is preserved"), Loaded, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellPickupCapacityTest,
	"Darkwell.Gameplay.Resources.PickupCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellPickupCapacityTest::RunTest(const FString& Parameters)
{
	int32 Reserve = 10;
	TestEqual(TEXT("Only available reserve capacity is accepted"), Darkwell::ResourceMath::AddToReserve(4, 12, Reserve), 2);
	TestEqual(TEXT("Reserve is clamped to capacity"), Reserve, 12);
	TestEqual(TEXT("A full reserve rejects the next pickup"), Darkwell::ResourceMath::AddToReserve(4, 12, Reserve), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellTorchDrainTest,
	"Darkwell.Gameplay.Resources.TorchDrain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellTorchDrainTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Torch charge drains by rate over time"), Darkwell::ResourceMath::DrainResource(10.0f, 2.0f, 3.0f), 4.0f);
	TestEqual(TEXT("Torch charge never crosses zero"), Darkwell::ResourceMath::DrainResource(1.0f, 2.0f, 3.0f), 0.0f);
	TestEqual(TEXT("Negative delta time cannot add charge"), Darkwell::ResourceMath::DrainResource(5.0f, 2.0f, -1.0f), 5.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellPlayerSurvivalRulesTest,
	"Darkwell.Gameplay.Survival.PlayerDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellPlayerSurvivalRulesTest::RunTest(const FString& Parameters)
{
	using namespace Darkwell::SurvivalRules;

	TestEqual(TEXT("Damage is clamped to remaining health"), ComputeAppliedDamage(10.0f, 16.0f), 10.0f);
	TestEqual(TEXT("Negative damage cannot heal the player"), ComputeAppliedDamage(75.0f, -8.0f), 0.0f);
	TestTrue(TEXT("An alive vulnerable player can take damage"), CanApplyDamage(100.0f, 2.0, 2.0));
	TestFalse(TEXT("Invulnerability rejects overlapping hits"), CanApplyDamage(84.0f, 2.2, 2.65));
	TestFalse(TEXT("A dead player cannot take additional damage"), CanApplyDamage(0.0f, 8.0, 0.0));
	TestEqual(TEXT("Damage feedback begins at full strength"), ComputeDamageFeedbackAlpha(4.0, 4.0, 0.5f), 1.0f);
	TestEqual(TEXT("Damage feedback fades linearly"), ComputeDamageFeedbackAlpha(4.25, 4.0, 0.5f), 0.5f);
	TestEqual(TEXT("Damage feedback expires"), ComputeDamageFeedbackAlpha(4.5, 4.0, 0.5f), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellMissionProgressionTest,
	"Darkwell.Gameplay.Mission.Progression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellMissionProgressionTest::RunTest(const FString& Parameters)
{
	using Darkwell::MissionRules::ResolveMissionState;

	FGameplayTag State = DarkwellGameplayTags::State_Mission_FindFuse;
	TestEqual(
		TEXT("The locked exit cannot skip the fuse objective"),
		ResolveMissionState(State, EDarkwellMissionEvent::UseExit),
		State);

	State = ResolveMissionState(State, EDarkwellMissionEvent::CollectFuse);
	TestEqual(TEXT("Collecting the fuse advances the objective"), State, DarkwellGameplayTags::State_Mission_ReachExit.GetTag());
	TestEqual(
		TEXT("Collecting the fuse twice does not advance again"),
		ResolveMissionState(State, EDarkwellMissionEvent::CollectFuse),
		State);

	State = ResolveMissionState(State, EDarkwellMissionEvent::UseExit);
	TestEqual(TEXT("Using the powered exit completes the mission"), State, DarkwellGameplayTags::State_Mission_Escaped.GetTag());
	TestEqual(
		TEXT("A completed mission remains completed"),
		ResolveMissionState(State, EDarkwellMissionEvent::UseExit),
		State);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellNativeStateTagsTest,
	"Darkwell.Gameplay.Tags.NativeStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellNativeStateTagsTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Alive player state tag is registered"), DarkwellGameplayTags::State_Player_Alive.GetTag().IsValid());
	TestTrue(TEXT("Dead player state tag is registered"), DarkwellGameplayTags::State_Player_Dead.GetTag().IsValid());
	TestTrue(TEXT("Escaped player state tag is registered"), DarkwellGameplayTags::State_Player_Escaped.GetTag().IsValid());
	TestTrue(TEXT("Torch state tag is registered"), DarkwellGameplayTags::State_Player_Torch_On.GetTag().IsValid());
	TestTrue(TEXT("Lowered torch tag is registered"), DarkwellGameplayTags::State_Player_Torch_Lowered.GetTag().IsValid());
	TestTrue(TEXT("Torch swing tag is registered"), DarkwellGameplayTags::State_Player_Torch_Swinging.GetTag().IsValid());
	TestTrue(TEXT("Torch deterrent tag is registered"), DarkwellGameplayTags::State_Player_Torch_Deterrent.GetTag().IsValid());
	TestTrue(TEXT("Torch overheat tag is registered"), DarkwellGameplayTags::State_Player_Torch_Overheated.GetTag().IsValid());
	TestTrue(TEXT("Lantern focus tag is registered"), DarkwellGameplayTags::State_Player_Lantern_Focused.GetTag().IsValid());
	TestTrue(TEXT("Lantern flash tag is registered"), DarkwellGameplayTags::State_Player_Lantern_Flashing.GetTag().IsValid());
	TestTrue(TEXT("Reload state tag is registered"), DarkwellGameplayTags::State_Player_Weapon_Reloading.GetTag().IsValid());
	TestTrue(TEXT("Door state tag is registered"), DarkwellGameplayTags::State_World_Door_Closed.GetTag().IsValid());
	TestTrue(TEXT("Enemy hunting tag is registered"), DarkwellGameplayTags::State_Enemy_Hunting.GetTag().IsValid());
	TestTrue(TEXT("Enemy repelled tag is registered"), DarkwellGameplayTags::State_Enemy_Repelled.GetTag().IsValid());
	TestTrue(TEXT("Enemy light-stunned tag is registered"), DarkwellGameplayTags::State_Enemy_LightStunned.GetTag().IsValid());
	TestTrue(TEXT("Stalker archetype tag is registered"), DarkwellGameplayTags::Enemy_Archetype_Stalker.GetTag().IsValid());
	TestTrue(TEXT("Warden archetype tag is registered"), DarkwellGameplayTags::Enemy_Archetype_Warden.GetTag().IsValid());
	TestTrue(TEXT("Find-fuse mission tag is registered"), DarkwellGameplayTags::State_Mission_FindFuse.GetTag().IsValid());
	TestTrue(TEXT("Reach-exit mission tag is registered"), DarkwellGameplayTags::State_Mission_ReachExit.GetTag().IsValid());
	TestTrue(TEXT("Escaped mission tag is registered"), DarkwellGameplayTags::State_Mission_Escaped.GetTag().IsValid());
	TestTrue(TEXT("Left-hand shotgun equipment tag is registered"), DarkwellGameplayTags::Equipment_Left_Shotgun.GetTag().IsValid());
	TestTrue(TEXT("Right-hand torch equipment tag is registered"), DarkwellGameplayTags::Equipment_Right_Torch.GetTag().IsValid());
	TestTrue(TEXT("Right-hand lantern equipment tag is registered"), DarkwellGameplayTags::Equipment_Right_Lantern.GetTag().IsValid());
	TestTrue(TEXT("Shotgun shell item tag is registered"), DarkwellGameplayTags::Item_Ammo_ShotgunShell.GetTag().IsValid());
	TestTrue(TEXT("Scrap item tag is registered"), DarkwellGameplayTags::Item_Material_Scrap.GetTag().IsValid());
	TestNotEqual(
		TEXT("Alive and dead player states are distinct"),
		DarkwellGameplayTags::State_Player_Alive.GetTag(),
		DarkwellGameplayTags::State_Player_Dead.GetTag());
	TestNotEqual(
		TEXT("Torch and lantern equipment categories are distinct"),
		DarkwellGameplayTags::Equipment_Right_Torch.GetTag(),
		DarkwellGameplayTags::Equipment_Right_Lantern.GetTag());
	TestNotEqual(
		TEXT("Open and closed door tags are distinct"),
		DarkwellGameplayTags::State_World_Door_Open.GetTag(),
		DarkwellGameplayTags::State_World_Door_Closed.GetTag());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellReloadTorchRuleTest,
	"Darkwell.Gameplay.Loadout.ReloadTorch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellReloadTorchRuleTest::RunTest(const FString& Parameters)
{
	const EDarkwellTorchPresentationMode Raised =
		Darkwell::LoadoutRules::ResolveTorchPresentation(true, true, false, false, false);
	const EDarkwellTorchPresentationMode Held =
		Darkwell::LoadoutRules::ResolveTorchPresentation(true, true, false, false, true);
	const EDarkwellTorchPresentationMode Reloading =
		Darkwell::LoadoutRules::ResolveTorchPresentation(true, true, true, false, true);

	TestEqual(TEXT("An equipped lit torch uses its idle presentation"), Raised, EDarkwellTorchPresentationMode::Idle);
	TestFalse(TEXT("An idle torch cannot repel an enemy"), Darkwell::LoadoutRules::IsTorchDeterrentActive(Raised));
	TestEqual(TEXT("Holding the torch forward uses its deterrent presentation"), Held, EDarkwellTorchPresentationMode::Deterrent);
	TestTrue(TEXT("A held-forward torch can repel an enemy"), Darkwell::LoadoutRules::IsTorchDeterrentActive(Held));
	TestEqual(
		TEXT("Reloading lowers a lit torch to the foot-level pool"),
		Reloading,
		EDarkwellTorchPresentationMode::ReloadPool);
	TestFalse(
		TEXT("The reload light pool cannot repel an enemy"),
		Darkwell::LoadoutRules::IsTorchDeterrentActive(Reloading));
	TestEqual(
		TEXT("An unlit torch remains off during reload"),
		Darkwell::LoadoutRules::ResolveTorchPresentation(true, false, true, false, false),
		EDarkwellTorchPresentationMode::Off);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellRightToolGestureTest,
	"Darkwell.Gameplay.Loadout.RightToolGestures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellRightToolGestureTest::RunTest(const FString& Parameters)
{
	using namespace Darkwell::LoadoutRules;
	TestEqual(TEXT("A quick release resolves to a tap"), ResolveGesture(0.12f, 0.28f), EDarkwellRightToolGesture::Tap);
	TestEqual(TEXT("Crossing the threshold resolves to a hold"), ResolveGesture(0.28f, 0.28f), EDarkwellRightToolGesture::Hold);
	TestEqual(TEXT("An inactive timer resolves to no gesture"), ResolveGesture(-1.0f, 0.28f), EDarkwellRightToolGesture::None);
	TestEqual(TEXT("Holding adds normalized torch heat"), UpdateTorchHeat(92.0f, 20.0f, 10.0f, 1.0f, true), 100.0f);
	TestEqual(TEXT("Releasing cools normalized torch heat"), UpdateTorchHeat(30.0f, 20.0f, 10.0f, 2.0f, false), 10.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellEnemyIntentTest,
	"Darkwell.Gameplay.Enemy.Intent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellEnemyIntentTest::RunTest(const FString& Parameters)
{
	const float FacingThreshold = FMath::Cos(FMath::DegreesToRadians(36.0f));
	TestEqual(
		TEXT("A seen player is hunted without active light pressure"),
		Darkwell::EnemyMath::ChooseIntent(true, true, false, 300.0f, 1.0f, 760.0f, FacingThreshold, 130.0f),
		EDarkwellEnemyIntent::Hunt);
	TestEqual(
		TEXT("A stalker inside the held-torch boundary retreats"),
		Darkwell::EnemyMath::ChooseIntent(true, true, true, 500.0f, 0.95f, 760.0f, FacingThreshold, 130.0f),
		EDarkwellEnemyIntent::Repel);
	TestEqual(
		TEXT("A stalker just outside the held-torch boundary stops approaching"),
		Darkwell::EnemyMath::ChooseIntent(true, true, true, 820.0f, 0.95f, 760.0f, FacingThreshold, 130.0f),
		EDarkwellEnemyIntent::HoldAtBay);
	TestEqual(
		TEXT("A distant stalker can approach until it reaches the torch boundary buffer"),
		Darkwell::EnemyMath::ChooseIntent(true, true, true, 920.0f, 0.95f, 760.0f, FacingThreshold, 130.0f),
		EDarkwellEnemyIntent::Hunt);
	TestEqual(
		TEXT("Lantern focus does not change enemy intent before its stun meter fills"),
		Darkwell::EnemyMath::ChooseIntent(true, true, false, 500.0f, 1.0f, 1750.0f, FacingThreshold, 130.0f),
		EDarkwellEnemyIntent::Hunt);
	TestEqual(
		TEXT("An alerted stalker investigates after losing sight"),
		Darkwell::EnemyMath::ChooseIntent(false, true, true, 300.0f, 1.0f, 760.0f, FacingThreshold, 130.0f),
		EDarkwellEnemyIntent::Investigate);
	TestEqual(
		TEXT("An unaware stalker remains idle"),
		Darkwell::EnemyMath::ChooseIntent(false, false, false, 300.0f, 1.0f, 760.0f, FacingThreshold, 130.0f),
		EDarkwellEnemyIntent::Idle);

	const FVector Boundary = Darkwell::EnemyMath::MakeBoundaryDestination(
		FVector(100.0f, 0.0f, 50.0f),
		FVector::ZeroVector,
		500.0f);
	TestTrue(
		TEXT("A retreat destination lands on the requested boundary and preserves enemy height"),
		Boundary.Equals(FVector(500.0f, 0.0f, 50.0f), 0.01f));

	float StunBuildup = Darkwell::EnemyMath::UpdateLanternStunBuildup(0.25f, true, 1.5f, 3.0f, 0.18f);
	TestTrue(TEXT("Lantern focus accumulates normalized stun buildup"), FMath::IsNearlyEqual(StunBuildup, 0.75f));
	StunBuildup = Darkwell::EnemyMath::UpdateLanternStunBuildup(StunBuildup, false, 1.0f, 3.0f, 0.18f);
	TestTrue(TEXT("Missing the target slowly decays stun buildup"), FMath::IsNearlyEqual(StunBuildup, 0.57f));
	StunBuildup = Darkwell::EnemyMath::UpdateLanternStunBuildup(StunBuildup, true, 3.0f, 3.0f, 0.18f);
	TestEqual(TEXT("Lantern stun buildup clamps when full"), StunBuildup, 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellEnemyArchetypeTest,
	"Darkwell.Gameplay.Enemy.Archetypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellEnemyArchetypeTest::RunTest(const FString& Parameters)
{
	const ADarkwellStalkerCharacter* Stalker = GetDefault<ADarkwellStalkerCharacter>();
	const ADarkwellWardenCharacter* Warden = GetDefault<ADarkwellWardenCharacter>();
	TestNotNull(TEXT("The stalker native archetype exists"), Stalker);
	TestNotNull(TEXT("The warden native archetype exists"), Warden);
	if (!Stalker || !Warden)
	{
		return false;
	}

	TestTrue(
		TEXT("The stalker owns its durable archetype tag"),
		Stalker->GetEnemyArchetype() == DarkwellGameplayTags::Enemy_Archetype_Stalker);
	TestTrue(
		TEXT("The warden owns its durable archetype tag"),
		Warden->GetEnemyArchetype() == DarkwellGameplayTags::Enemy_Archetype_Warden);
	TestTrue(TEXT("The warden has more health than the stalker"), Warden->GetMaxHealth() > Stalker->GetMaxHealth());
	TestTrue(TEXT("The warden hits harder than the stalker"), Warden->GetAttackDamage() > Stalker->GetAttackDamage());
	TestTrue(
		TEXT("The warden pushes deeper through held torch light"),
		Warden->GetTorchDeterrenceRangeScale() < Stalker->GetTorchDeterrenceRangeScale());
	TestEqual(TEXT("A full lantern focus stuns the warden for five seconds"), Warden->GetLanternFocusStunDuration(), 5.0f);

	const float FacingThreshold = FMath::Cos(FMath::DegreesToRadians(36.0f));
	const float WardenTorchRange = 760.0f * Warden->GetTorchDeterrenceRangeScale();
	TestEqual(
		TEXT("The warden can keep advancing through the stalker's outer torch boundary"),
		Darkwell::EnemyMath::ChooseIntent(
			true,
			true,
			true,
			600.0f,
			0.95f,
			WardenTorchRange,
			FacingThreshold,
			Warden->GetTorchBoundaryBuffer()),
		EDarkwellEnemyIntent::Hunt);
	TestEqual(
		TEXT("The warden still retreats when it penetrates its close torch boundary"),
		Darkwell::EnemyMath::ChooseIntent(
			true,
			true,
			true,
			400.0f,
			0.95f,
			WardenTorchRange,
			FacingThreshold,
			Warden->GetTorchBoundaryBuffer()),
		EDarkwellEnemyIntent::Repel);
	return true;
}

#endif
