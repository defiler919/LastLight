// Copyright Epic Games, Inc. All Rights Reserved.

#include "VisionPresentation/DarkwellRememberablePropComponent.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gameplay/DarkwellFogSubject.h"
#include "VisionPresentation/DarkwellRememberedPropSubsystem.h"

UDarkwellRememberablePropComponent::UDarkwellRememberablePropComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDarkwellRememberablePropComponent::ConfigureStableId(const FName InStableId)
{
	if (StableId == InStableId)
	{
		TryRegister();
		return;
	}
	if (bRegistered)
	{
		if (UDarkwellRememberedPropSubsystem* Subsystem =
			GetWorld() ? GetWorld()->GetSubsystem<UDarkwellRememberedPropSubsystem>() : nullptr)
		{
			Subsystem->UnregisterProp(this, EEndPlayReason::RemovedFromWorld);
		}
		bRegistered = false;
	}
	StableId = InStableId;
	TryRegister();
}

void UDarkwellRememberablePropComponent::AddMemoryPrimitive(
	UStaticMeshComponent* Primitive)
{
	if (Primitive)
	{
		MemoryPrimitives.AddUnique(Primitive);
	}
}

void UDarkwellRememberablePropComponent::AddLiveOnlyComponent(
	USceneComponent* Component)
{
	if (Component)
	{
		LiveOnlyComponents.AddUnique(Component);
	}
}

FTransform UDarkwellRememberablePropComponent::GetPrimitiveTransform(const UStaticMeshComponent& Primitive)
{
	FTransform Result = FTransform::Identity;
	const USceneComponent* Part = &Primitive;
	const USceneComponent* Root = Primitive.GetOwner()->GetRootComponent();
	while (Part && Part != Root)
	{
		Result = Result * Part->GetRelativeTransform();
		Part = Part->GetAttachParent();
	}
	return Result;
}

uint64 UDarkwellRememberablePropComponent::ComputeMemoryContentRevision() const
{
	uint64 Hash = 1469598103934665603ull;
	auto Mix = [&Hash](uint64 V) { Hash = (Hash ^ V) * 1099511628211ull; };
	Mix(MemoryContentRevision);
	Mix(GetTypeHash(RememberedTint)); Mix(GetTypeHash(RememberedUVScale));
	for (const UStaticMeshComponent* Primitive : MemoryPrimitives)
	{
		Mix(GetTypeHash(Primitive));
		if (!Primitive) continue;
		Mix(GetTypeHash(Primitive->GetStaticMesh()));
		const FTransform Relative = GetPrimitiveTransform(*Primitive);
		Mix(GetTypeHash(Relative.GetTranslation()));
		Mix(GetTypeHash(Relative.GetRotation()));
		Mix(GetTypeHash(Relative.GetScale3D()));
		for (int32 Index = 0; Index < Primitive->GetNumMaterials(); ++Index)
			Mix(GetTypeHash(Primitive->GetMaterial(Index)));
	}
	return Hash;
}

uint64 UDarkwellRememberablePropComponent::ComputeAppearanceRevision() const
{
	uint32 Hash = GetTypeHash(StableId);
	for (const UStaticMeshComponent* Primitive : MemoryPrimitives)
	{
		if (!Primitive)
		{
			continue;
		}
		Hash = HashCombineFast(Hash, GetTypeHash(Primitive->GetStaticMesh()));
		Hash = HashCombineFast(Hash, GetTypeHash(Primitive->GetComponentTransform().ToHumanReadableString()));
		for (int32 Index = 0; Index < Primitive->GetNumMaterials(); ++Index)
		{
			Hash = HashCombineFast(Hash, GetTypeHash(Primitive->GetMaterial(Index)));
		}
	}
	return static_cast<uint64>(Hash);
}

FTransform UDarkwellRememberablePropComponent::GetObservationTransform() const
{
	for (const UStaticMeshComponent* Primitive : MemoryPrimitives)
	{
		if (Primitive)
		{
			return Primitive->GetComponentTransform();
		}
	}
	return GetOwner() ? GetOwner()->GetActorTransform() : FTransform::Identity;
}

void UDarkwellRememberablePropComponent::ApplySourceGeometryVisibility(const bool bVisible)
{
	for (UStaticMeshComponent* Primitive : MemoryPrimitives)
	{
		if (!Primitive) continue;
		const bool* WasVisible = VisibilityBeforeHide.Find(Primitive);
		Primitive->SetVisibility(bVisible && (!WasVisible || *WasVisible), false);
	}
}

void UDarkwellRememberablePropComponent::ApplySourceLiveState(const bool bLive)
{
	if (bSourceLive == bLive)
	{
		return;
	}
	bSourceLive = bLive;
	TArray<USceneComponent*> Controlled;
	for (UStaticMeshComponent* Primitive : MemoryPrimitives)
	{
		if (Primitive)
		{
			Controlled.AddUnique(Primitive);
		}
	}
	for (USceneComponent* Component : LiveOnlyComponents)
	{
		if (Component)
		{
			Controlled.AddUnique(Component);
		}
	}
	if (!bLive)
	{
		VisibilityBeforeHide.Reset();
		for (USceneComponent* Component : Controlled)
		{
			VisibilityBeforeHide.Add(Component, Component->IsVisible());
			Component->SetVisibility(false, true);
		}
	}
	else
	{
		for (USceneComponent* Component : Controlled)
		{
			const bool* WasVisible = VisibilityBeforeHide.Find(Component);
			Component->SetVisibility(!WasVisible || *WasVisible, true);
		}
		VisibilityBeforeHide.Reset();
	}
	if (AActor* Owner = GetOwner(); Owner && Owner->Implements<UDarkwellFogSubject>())
	{
		Cast<IDarkwellFogSubject>(Owner)->SetPlayerFogState(
			bLive ? EDarkwellFogCellState::Visible : EDarkwellFogCellState::Explored);
	}
}

void UDarkwellRememberablePropComponent::BeginPlay()
{
	Super::BeginPlay();
	TryRegister();
}

void UDarkwellRememberablePropComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (bRegistered)
	{
		if (UDarkwellRememberedPropSubsystem* Subsystem =
			GetWorld() ? GetWorld()->GetSubsystem<UDarkwellRememberedPropSubsystem>() : nullptr)
		{
			Subsystem->UnregisterProp(this, EndPlayReason);
		}
		bRegistered = false;
	}
	Super::EndPlay(EndPlayReason);
}

void UDarkwellRememberablePropComponent::TryRegister()
{
	if (bRegistered || bUseSpatialMemory || StableId.IsNone() || MemoryPrimitives.IsEmpty()
		|| !HasBegunPlay() || !GetWorld())
	{
		return;
	}
	if (UDarkwellRememberedPropSubsystem* Subsystem =
		GetWorld()->GetSubsystem<UDarkwellRememberedPropSubsystem>())
	{
		bRegistered = Subsystem->RegisterProp(this);
	}
}
