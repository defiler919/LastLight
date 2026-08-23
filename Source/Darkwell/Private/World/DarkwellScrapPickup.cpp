// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/DarkwellScrapPickup.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "Inventory/DarkwellInventoryComponent.h"
#include "Player/DarkwellCharacter.h"
#include "Save/DarkwellSaveSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ADarkwellScrapPickup::ADarkwellScrapPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	SetRootComponent(PickupMesh);
	PickupMesh->SetRelativeScale3D(FVector(0.3f, 0.24f, 0.18f));
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PickupLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PickupLight"));
	PickupLight->SetupAttachment(PickupMesh);
	PickupLight->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	PickupLight->SetLightColor(FLinearColor(0.18f, 0.75f, 0.9f));
	PickupLight->SetIntensity(850.0f);
	PickupLight->SetAttenuationRadius(220.0f);
	PickupLight->SetCastShadows(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PickupMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void ADarkwellScrapPickup::BeginPlay()
{
	Super::BeginPlay();
	SetPlayerFogState(EDarkwellFogCellState::Unexplored);
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UDarkwellSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UDarkwellSaveSubsystem>())
		{
			SaveSubsystem->RegisterWorldPickup(PersistentId, ScrapQuantity);
		}
	}
}

void ADarkwellScrapPickup::SetPlayerFogState(const EDarkwellFogCellState NewState)
{
	bFogPresentationVisible = NewState == EDarkwellFogCellState::Visible;
	PickupMesh->SetHiddenInGame(!bFogPresentationVisible);
	PickupMesh->SetCollisionResponseToChannel(
		ECC_Visibility,
		bFogPresentationVisible ? ECR_Block : ECR_Ignore);
}

void ADarkwellScrapPickup::ConfigurePickup(const FName InPersistentId)
{
	PersistentId = InPersistentId;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UDarkwellSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UDarkwellSaveSubsystem>())
		{
			SaveSubsystem->RegisterWorldPickup(PersistentId, ScrapQuantity);
		}
	}
}

void ADarkwellScrapPickup::RestoreRemainingQuantity(const int32 Quantity)
{
	ScrapQuantity = FMath::Max(0, Quantity);
	if (ScrapQuantity <= 0)
	{
		Destroy();
	}
}

bool ADarkwellScrapPickup::CanInteract(const ADarkwellCharacter& Character) const
{
	const UDarkwellInventoryComponent* Inventory = Character.GetInventoryComponent();
	return Inventory && Inventory->CanAddItem(DarkwellGameplayTags::Item_Material_Scrap, 1);
}

void ADarkwellScrapPickup::Interact(ADarkwellCharacter& Character)
{
	if (UDarkwellInventoryComponent* Inventory = Character.GetInventoryComponent())
	{
		ScrapQuantity -= Inventory->AddItem(DarkwellGameplayTags::Item_Material_Scrap, ScrapQuantity);
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UDarkwellSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<UDarkwellSaveSubsystem>())
			{
				SaveSubsystem->UpdateWorldPickupQuantity(PersistentId, ScrapQuantity);
			}
		}
		if (ScrapQuantity <= 0)
		{
			Destroy();
		}
	}
}

FText ADarkwellScrapPickup::GetInteractionPrompt(const ADarkwellCharacter& Character) const
{
	return FText::Format(
		NSLOCTEXT("Darkwell", "TakeScrap", "Take {0} scrap"),
		FText::AsNumber(ScrapQuantity));
}

void ADarkwellScrapPickup::OnInteractionFocusChanged(const bool bFocused)
{
	PickupMesh->SetRelativeScale3D(bFocused
		? FVector(0.375f, 0.3f, 0.225f)
		: FVector(0.3f, 0.24f, 0.18f));
}
