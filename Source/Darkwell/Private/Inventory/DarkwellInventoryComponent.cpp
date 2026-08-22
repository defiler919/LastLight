// Copyright Epic Games, Inc. All Rights Reserved.

#include "Inventory/DarkwellInventoryComponent.h"

#include "Inventory/DarkwellItemCatalog.h"

UDarkwellInventoryComponent::UDarkwellInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDarkwellInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureSlotArray();
}

void UDarkwellInventoryComponent::InitializeInventory(const int32 InSlotCapacity)
{
	SlotCapacity = FMath::Clamp(InSlotCapacity, 1, 64);
	EnsureSlotArray();
}

int32 UDarkwellInventoryComponent::AddItem(const FGameplayTag ItemTag, const int32 RequestedQuantity)
{
	EnsureSlotArray();
	const int32 Added = AddToSlots(Slots, ItemTag, RequestedQuantity);
	if (Added > 0)
	{
		BroadcastChanged();
	}
	return Added;
}

int32 UDarkwellInventoryComponent::RemoveItem(const FGameplayTag ItemTag, const int32 RequestedQuantity)
{
	EnsureSlotArray();
	const int32 Removed = RemoveFromSlots(Slots, ItemTag, RequestedQuantity);
	if (Removed > 0)
	{
		BroadcastChanged();
	}
	return Removed;
}

bool UDarkwellInventoryComponent::CanAddItem(const FGameplayTag ItemTag, const int32 RequestedQuantity) const
{
	if (!ItemTag.IsValid() || RequestedQuantity <= 0)
	{
		return false;
	}

	TArray<FDarkwellItemStack> SimulatedSlots = Slots;
	if (SimulatedSlots.Num() < SlotCapacity)
	{
		SimulatedSlots.SetNum(SlotCapacity);
	}
	return AddToSlots(SimulatedSlots, ItemTag, RequestedQuantity) == RequestedQuantity;
}

int32 UDarkwellInventoryComponent::GetTotalQuantity(const FGameplayTag ItemTag) const
{
	int32 Total = 0;
	for (const FDarkwellItemStack& Stack : Slots)
	{
		if (!Stack.IsEmpty() && Stack.ItemTag == ItemTag)
		{
			Total += Stack.Quantity;
		}
	}
	return Total;
}

int32 UDarkwellInventoryComponent::GetTotalItemCount() const
{
	int32 Total = 0;
	for (const FDarkwellItemStack& Stack : Slots)
	{
		if (!Stack.IsEmpty())
		{
			Total += Stack.Quantity;
		}
	}
	return Total;
}

int32 UDarkwellInventoryComponent::TransferSlotTo(
	UDarkwellInventoryComponent& Target,
	const int32 SourceSlotIndex,
	const int32 RequestedQuantity)
{
	EnsureSlotArray();
	Target.EnsureSlotArray();
	if (&Target == this || !Slots.IsValidIndex(SourceSlotIndex) || Slots[SourceSlotIndex].IsEmpty())
	{
		return 0;
	}

	FDarkwellItemStack& SourceStack = Slots[SourceSlotIndex];
	const int32 QuantityToMove = RequestedQuantity > 0
		? FMath::Min(RequestedQuantity, SourceStack.Quantity)
		: SourceStack.Quantity;
	const int32 Added = AddToSlots(Target.Slots, SourceStack.ItemTag, QuantityToMove);
	if (Added <= 0)
	{
		return 0;
	}

	SourceStack.Quantity -= Added;
	if (SourceStack.Quantity <= 0)
	{
		SourceStack.Clear();
	}
	BroadcastChanged();
	Target.BroadcastChanged();
	return Added;
}

int32 UDarkwellInventoryComponent::TransferAllTo(UDarkwellInventoryComponent& Target)
{
	if (&Target == this)
	{
		return 0;
	}

	int32 TotalMoved = 0;
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		TotalMoved += TransferSlotTo(Target, SlotIndex);
	}
	return TotalMoved;
}

bool UDarkwellInventoryComponent::SplitStack(const int32 SourceSlotIndex)
{
	EnsureSlotArray();
	if (!Slots.IsValidIndex(SourceSlotIndex) || Slots[SourceSlotIndex].Quantity < 2)
	{
		return false;
	}

	const int32 EmptySlotIndex = Slots.IndexOfByPredicate([](const FDarkwellItemStack& Stack)
	{
		return Stack.IsEmpty();
	});
	if (EmptySlotIndex == INDEX_NONE)
	{
		return false;
	}

	FDarkwellItemStack& SourceStack = Slots[SourceSlotIndex];
	FDarkwellItemStack& SplitStack = Slots[EmptySlotIndex];
	const int32 SplitQuantity = FMath::DivideAndRoundUp(SourceStack.Quantity, 2);
	SplitStack.ItemTag = SourceStack.ItemTag;
	SplitStack.Quantity = SplitQuantity;
	SourceStack.Quantity -= SplitQuantity;
	BroadcastChanged();
	return true;
}

bool UDarkwellInventoryComponent::MoveSlotWithinInventory(
	const int32 SourceSlotIndex,
	const int32 TargetSlotIndex)
{
	EnsureSlotArray();
	if (SourceSlotIndex == TargetSlotIndex
		|| !Slots.IsValidIndex(SourceSlotIndex)
		|| !Slots.IsValidIndex(TargetSlotIndex)
		|| Slots[SourceSlotIndex].IsEmpty())
	{
		return false;
	}

	FDarkwellItemStack& SourceStack = Slots[SourceSlotIndex];
	FDarkwellItemStack& TargetStack = Slots[TargetSlotIndex];
	if (TargetStack.IsEmpty())
	{
		TargetStack = SourceStack;
		SourceStack.Clear();
	}
	else if (TargetStack.ItemTag == SourceStack.ItemTag)
	{
		const int32 MaxStack = Darkwell::ItemCatalog::GetMaxStack(SourceStack.ItemTag);
		const int32 QuantityMoved = FMath::Min(SourceStack.Quantity, MaxStack - TargetStack.Quantity);
		if (QuantityMoved <= 0)
		{
			return false;
		}

		TargetStack.Quantity += QuantityMoved;
		SourceStack.Quantity -= QuantityMoved;
		if (SourceStack.Quantity <= 0)
		{
			SourceStack.Clear();
		}
	}
	else
	{
		Swap(SourceStack, TargetStack);
	}

	BroadcastChanged();
	return true;
}

bool UDarkwellInventoryComponent::TryExchange(
	const FGameplayTag CostItem,
	const int32 CostQuantity,
	const FGameplayTag ResultItem,
	const int32 ResultQuantity)
{
	EnsureSlotArray();
	if (!CostItem.IsValid() || !ResultItem.IsValid() || CostQuantity <= 0 || ResultQuantity <= 0)
	{
		return false;
	}

	TArray<FDarkwellItemStack> SimulatedSlots = Slots;
	if (RemoveFromSlots(SimulatedSlots, CostItem, CostQuantity) != CostQuantity
		|| AddToSlots(SimulatedSlots, ResultItem, ResultQuantity) != ResultQuantity)
	{
		return false;
	}

	Slots = MoveTemp(SimulatedSlots);
	BroadcastChanged();
	return true;
}

void UDarkwellInventoryComponent::RestoreState(
	const int32 SavedSlotCapacity,
	const TArray<FDarkwellItemStack>& SavedSlots)
{
	SlotCapacity = FMath::Clamp(SavedSlotCapacity > 0 ? SavedSlotCapacity : SavedSlots.Num(), 1, 64);
	Slots.SetNum(SlotCapacity);
	for (FDarkwellItemStack& Stack : Slots)
	{
		Stack.Clear();
	}

	const int32 SlotsToRestore = FMath::Min(SlotCapacity, SavedSlots.Num());
	for (int32 SlotIndex = 0; SlotIndex < SlotsToRestore; ++SlotIndex)
	{
		const FDarkwellItemStack& SavedStack = SavedSlots[SlotIndex];
		if (SavedStack.IsEmpty())
		{
			continue;
		}

		const int32 MaxStack = Darkwell::ItemCatalog::GetMaxStack(SavedStack.ItemTag);
		FDarkwellItemStack& RestoredStack = Slots[SlotIndex];
		RestoredStack.ItemTag = SavedStack.ItemTag;
		RestoredStack.Quantity = FMath::Min(SavedStack.Quantity, MaxStack);
		const int32 Overflow = FMath::Max(0, SavedStack.Quantity - RestoredStack.Quantity);
		if (Overflow > 0)
		{
			AddToSlots(Slots, SavedStack.ItemTag, Overflow);
		}
	}

	BroadcastChanged();
}

const FDarkwellItemStack* UDarkwellInventoryComponent::GetSlot(const int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? &Slots[SlotIndex] : nullptr;
}

void UDarkwellInventoryComponent::EnsureSlotArray()
{
	SlotCapacity = FMath::Clamp(SlotCapacity, 1, 64);
	if (Slots.Num() != SlotCapacity)
	{
		Slots.SetNum(SlotCapacity);
	}
}

void UDarkwellInventoryComponent::BroadcastChanged()
{
	InventoryChanged.Broadcast();
}

int32 UDarkwellInventoryComponent::AddToSlots(
	TArray<FDarkwellItemStack>& TargetSlots,
	const FGameplayTag ItemTag,
	const int32 RequestedQuantity)
{
	if (!ItemTag.IsValid() || RequestedQuantity <= 0)
	{
		return 0;
	}

	const int32 MaxStack = Darkwell::ItemCatalog::GetMaxStack(ItemTag);
	int32 Remaining = RequestedQuantity;
	for (FDarkwellItemStack& Stack : TargetSlots)
	{
		if (!Stack.IsEmpty() && Stack.ItemTag == ItemTag && Stack.Quantity < MaxStack)
		{
			const int32 AddedToStack = FMath::Min(Remaining, MaxStack - Stack.Quantity);
			Stack.Quantity += AddedToStack;
			Remaining -= AddedToStack;
			if (Remaining <= 0)
			{
				return RequestedQuantity;
			}
		}
	}

	for (FDarkwellItemStack& Stack : TargetSlots)
	{
		if (Stack.IsEmpty())
		{
			const int32 AddedToStack = FMath::Min(Remaining, MaxStack);
			Stack.ItemTag = ItemTag;
			Stack.Quantity = AddedToStack;
			Remaining -= AddedToStack;
			if (Remaining <= 0)
			{
				break;
			}
		}
	}
	return RequestedQuantity - Remaining;
}

int32 UDarkwellInventoryComponent::RemoveFromSlots(
	TArray<FDarkwellItemStack>& TargetSlots,
	const FGameplayTag ItemTag,
	const int32 RequestedQuantity)
{
	if (!ItemTag.IsValid() || RequestedQuantity <= 0)
	{
		return 0;
	}

	int32 Remaining = RequestedQuantity;
	for (int32 SlotIndex = TargetSlots.Num() - 1; SlotIndex >= 0; --SlotIndex)
	{
		FDarkwellItemStack& Stack = TargetSlots[SlotIndex];
		if (!Stack.IsEmpty() && Stack.ItemTag == ItemTag)
		{
			const int32 RemovedFromStack = FMath::Min(Remaining, Stack.Quantity);
			Stack.Quantity -= RemovedFromStack;
			Remaining -= RemovedFromStack;
			if (Stack.Quantity <= 0)
			{
				Stack.Clear();
			}
			if (Remaining <= 0)
			{
				break;
			}
		}
	}
	return RequestedQuantity - Remaining;
}
