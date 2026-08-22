// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "DarkwellInventoryComponent.generated.h"

USTRUCT(BlueprintType)
struct DARKWELL_API FDarkwellItemStack
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, SaveGame, Category = "Inventory")
	FGameplayTag ItemTag;

	UPROPERTY(VisibleAnywhere, SaveGame, Category = "Inventory")
	int32 Quantity = 0;

	bool IsEmpty() const { return !ItemTag.IsValid() || Quantity <= 0; }
	void Clear()
	{
		ItemTag = FGameplayTag::EmptyTag;
		Quantity = 0;
	}
};

/** Fixed-slot, stack-based inventory shared by the player, containers, and crafting transactions. */
UCLASS(ClassGroup = (Darkwell), meta = (BlueprintSpawnableComponent))
class DARKWELL_API UDarkwellInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDarkwellInventoryComponent();
	virtual void BeginPlay() override;

	void InitializeInventory(int32 InSlotCapacity);
	int32 AddItem(FGameplayTag ItemTag, int32 RequestedQuantity);
	int32 RemoveItem(FGameplayTag ItemTag, int32 RequestedQuantity);
	bool CanAddItem(FGameplayTag ItemTag, int32 RequestedQuantity) const;
	int32 GetTotalQuantity(FGameplayTag ItemTag) const;
	int32 GetTotalItemCount() const;
	bool IsEmpty() const { return GetTotalItemCount() <= 0; }

	/** Moves up to RequestedQuantity from one source slot. A non-positive request means the whole stack. */
	int32 TransferSlotTo(UDarkwellInventoryComponent& Target, int32 SourceSlotIndex, int32 RequestedQuantity = 0);
	int32 TransferAllTo(UDarkwellInventoryComponent& Target);
	/** Splits the larger half of a stack into a free slot in this inventory. */
	bool SplitStack(int32 SourceSlotIndex);
	/** Moves, merges, or swaps a stack between two slots in this inventory. */
	bool MoveSlotWithinInventory(int32 SourceSlotIndex, int32 TargetSlotIndex);
	/** Atomically consumes one item and grants another; no mutation occurs when either side cannot complete. */
	bool TryExchange(FGameplayTag CostItem, int32 CostQuantity, FGameplayTag ResultItem, int32 ResultQuantity);
	/** Replaces all slots from validated persistent data and emits one change notification. */
	void RestoreState(int32 SavedSlotCapacity, const TArray<FDarkwellItemStack>& SavedSlots);

	int32 GetSlotCapacity() const { return SlotCapacity; }
	const TArray<FDarkwellItemStack>& GetSlots() const { return Slots; }
	const FDarkwellItemStack* GetSlot(int32 SlotIndex) const;

	FSimpleMulticastDelegate& OnInventoryChanged() { return InventoryChanged; }

private:
	void EnsureSlotArray();
	void BroadcastChanged();
	static int32 AddToSlots(TArray<FDarkwellItemStack>& TargetSlots, FGameplayTag ItemTag, int32 RequestedQuantity);
	static int32 RemoveFromSlots(TArray<FDarkwellItemStack>& TargetSlots, FGameplayTag ItemTag, int32 RequestedQuantity);

	UPROPERTY(EditDefaultsOnly, Category = "Inventory", meta = (ClampMin = "1", ClampMax = "64"))
	int32 SlotCapacity = 12;

	UPROPERTY(VisibleInstanceOnly, Category = "Inventory")
	TArray<FDarkwellItemStack> Slots;

	FSimpleMulticastDelegate InventoryChanged;
};
