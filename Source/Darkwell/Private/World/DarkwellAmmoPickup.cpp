// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/DarkwellAmmoPickup.h"

#include "Combat/DarkwellLoadoutComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "Inventory/DarkwellInventoryComponent.h"
#include "Player/DarkwellCharacter.h"
#include "Save/DarkwellSaveSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ADarkwellAmmoPickup::ADarkwellAmmoPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	SetRootComponent(PickupMesh);
	PickupMesh->SetRelativeScale3D(FVector(0.32f, 0.32f, 0.18f));
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PickupLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PickupLight"));
	PickupLight->SetupAttachment(PickupMesh);
	PickupLight->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	PickupLight->SetLightColor(FLinearColor(1.0f, 0.35f, 0.08f));
	PickupLight->SetIntensity(900.0f);
	PickupLight->SetAttenuationRadius(220.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		PickupMesh->SetStaticMesh(CylinderMesh.Object);
	}
}

void ADarkwellAmmoPickup::BeginPlay()
{
	Super::BeginPlay();
	SetPlayerFogState(EDarkwellFogCellState::Unexplored);
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UDarkwellSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UDarkwellSaveSubsystem>())
		{
			SaveSubsystem->RegisterWorldPickup(PersistentId, ShellCount);
		}
	}
}

void ADarkwellAmmoPickup::SetPlayerFogState(const EDarkwellFogCellState NewState)
{
	bFogPresentationVisible = NewState == EDarkwellFogCellState::Visible;
	PickupMesh->SetHiddenInGame(!bFogPresentationVisible);
	PickupMesh->SetCollisionResponseToChannel(
		ECC_Visibility,
		bFogPresentationVisible ? ECR_Block : ECR_Ignore);
}

void ADarkwellAmmoPickup::ConfigurePickup(const FName InPersistentId)
{
	PersistentId = InPersistentId;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UDarkwellSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UDarkwellSaveSubsystem>())
		{
			SaveSubsystem->RegisterWorldPickup(PersistentId, ShellCount);
		}
	}
}

void ADarkwellAmmoPickup::RestoreRemainingQuantity(const int32 Quantity)
{
	ShellCount = FMath::Max(0, Quantity);
	if (ShellCount <= 0)
	{
		Destroy();
	}
}

bool ADarkwellAmmoPickup::CanInteract(const ADarkwellCharacter& Character) const
{
	const UDarkwellInventoryComponent* Inventory = Character.GetInventoryComponent();
	return Inventory && Inventory->CanAddItem(DarkwellGameplayTags::Item_Ammo_ShotgunShell, 1);
}

void ADarkwellAmmoPickup::Interact(ADarkwellCharacter& Character)
{
	if (UDarkwellLoadoutComponent* Loadout = Character.GetLoadoutComponent())
	{
		const int32 AddedShells = Loadout->AddReserveShells(ShellCount);
		ShellCount -= AddedShells;
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UDarkwellSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UDarkwellSaveSubsystem>())
			{
				SaveSubsystem->UpdateWorldPickupQuantity(PersistentId, ShellCount);
			}
		}
		if (ShellCount <= 0)
		{
			Destroy();
		}
	}
}

FText ADarkwellAmmoPickup::GetInteractionPrompt(const ADarkwellCharacter& Character) const
{
	return FText::Format(
		NSLOCTEXT("Darkwell", "TakeShells", "Take {0} shells"),
		FText::AsNumber(ShellCount));
}

void ADarkwellAmmoPickup::OnInteractionFocusChanged(const bool bFocused)
{
	PickupMesh->SetRelativeScale3D(bFocused
		? FVector(0.4f, 0.4f, 0.225f)
		: FVector(0.32f, 0.32f, 0.18f));
}
