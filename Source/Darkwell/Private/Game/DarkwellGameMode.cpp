// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/DarkwellGameMode.h"

#include "AI/DarkwellStalkerCharacter.h"
#include "AI/DarkwellWardenCharacter.h"
#include "Game/DarkwellGameState.h"
#include "Save/DarkwellSaveSubsystem.h"
#include "Player/DarkwellCharacter.h"
#include "Player/DarkwellPlayerController.h"
#include "UI/DarkwellHUD.h"
#include "World/DarkwellAmmoPickup.h"
#include "World/DarkwellDoor.h"
#include "World/DarkwellExitGate.h"
#include "World/DarkwellFusePickup.h"
#include "World/DarkwellPracticeTarget.h"
#include "World/DarkwellPrototypeRoom.h"
#include "World/DarkwellScrapPickup.h"
#include "World/DarkwellStorageContainer.h"
#include "World/DarkwellWorkbench.h"

#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	template <typename TActor>
	bool HasActorOfType(const UWorld& World)
	{
		for (TActorIterator<TActor> It(&World); It; ++It)
		{
			return true;
		}
		return false;
	}

	template <typename TActor>
	bool HasExactActorClass(const UWorld& World)
	{
		for (TActorIterator<TActor> It(&World); It; ++It)
		{
			if (It->GetClass() == TActor::StaticClass())
			{
				return true;
			}
		}
		return false;
	}
}

ADarkwellGameMode::ADarkwellGameMode()
{
	DefaultPawnClass = ADarkwellCharacter::StaticClass();
	PlayerControllerClass = ADarkwellPlayerController::StaticClass();
	GameStateClass = ADarkwellGameState::StaticClass();
	HUDClass = ADarkwellHUD::StaticClass();
}

void ADarkwellGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	if (UWorld* World = GetWorld())
	{
		if (UDarkwellSaveSubsystem* SaveSubsystem = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UDarkwellSaveSubsystem>()
			: nullptr)
		{
			SaveSubsystem->BeginWorldSession(*World);
		}
	}
}

void ADarkwellGameMode::StartPlay()
{
	Super::StartPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	UDarkwellSaveSubsystem* SaveSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UDarkwellSaveSubsystem>()
		: nullptr;

	if (!HasActorOfType<ADarkwellPrototypeRoom>(*World))
	{
		World->SpawnActor<ADarkwellPrototypeRoom>(FVector::ZeroVector, FRotator::ZeroRotator);
	}

	if (!HasActorOfType<ADarkwellDoor>(*World))
	{
		World->SpawnActor<ADarkwellDoor>(FVector(200.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	}

	if (!HasActorOfType<ADarkwellFusePickup>(*World))
	{
		World->SpawnActor<ADarkwellFusePickup>(FVector(760.0f, 540.0f, 38.0f), FRotator::ZeroRotator);
	}

	if (!HasActorOfType<ADarkwellExitGate>(*World))
	{
		World->SpawnActor<ADarkwellExitGate>(
			FVector(-760.0f, -735.0f, 0.0f),
			FRotator(0.0f, 90.0f, 0.0f));
	}

	if (!HasActorOfType<ADarkwellAmmoPickup>(*World))
	{
		if (ADarkwellAmmoPickup* WestAmmo = World->SpawnActor<ADarkwellAmmoPickup>(
			FVector(-360.0f, 230.0f, 22.0f), FRotator::ZeroRotator))
		{
			WestAmmo->ConfigurePickup(FName(TEXT("Pickup.Ammo.WestHall")));
		}
		if (ADarkwellAmmoPickup* EastAmmo = World->SpawnActor<ADarkwellAmmoPickup>(
			FVector(520.0f, -520.0f, 22.0f), FRotator::ZeroRotator))
		{
			EastAmmo->ConfigurePickup(FName(TEXT("Pickup.Ammo.EastRoom")));
		}
	}

	if (!HasActorOfType<ADarkwellScrapPickup>(*World))
	{
		if (ADarkwellScrapPickup* NorthScrap = World->SpawnActor<ADarkwellScrapPickup>(
			FVector(-470.0f, 520.0f, 22.0f), FRotator::ZeroRotator))
		{
			NorthScrap->ConfigurePickup(FName(TEXT("Pickup.Scrap.NorthRoom")));
		}
		if (ADarkwellScrapPickup* SouthScrap = World->SpawnActor<ADarkwellScrapPickup>(
			FVector(690.0f, -250.0f, 22.0f), FRotator::ZeroRotator))
		{
			SouthScrap->ConfigurePickup(FName(TEXT("Pickup.Scrap.SouthRoom")));
		}
	}

	if (!HasActorOfType<ADarkwellStorageContainer>(*World))
	{
		ADarkwellStorageContainer* Chest = World->SpawnActor<ADarkwellStorageContainer>(
			FVector(-620.0f, -500.0f, 55.0f),
			FRotator::ZeroRotator);
		if (Chest)
		{
			Chest->ConfigureStorage(
				FName(TEXT("Container.SupplyChest")),
				NSLOCTEXT("Darkwell", "SupplyChest", "Supply chest"),
				7,
				3,
				EDarkwellStorageStyle::Chest);
		}

		ADarkwellStorageContainer* Cabinet = World->SpawnActor<ADarkwellStorageContainer>(
			FVector(620.0f, -570.0f, 55.0f),
			FRotator(0.0f, 90.0f, 0.0f));
		if (Cabinet)
		{
			Cabinet->ConfigureStorage(
				FName(TEXT("Container.WorkshopCabinet")),
				NSLOCTEXT("Darkwell", "WorkshopCabinet", "Workshop cabinet"),
				5,
				2,
				EDarkwellStorageStyle::Cabinet);
		}
	}

	if (!HasActorOfType<ADarkwellWorkbench>(*World))
	{
		World->SpawnActor<ADarkwellWorkbench>(FVector(520.0f, 650.0f, 50.0f), FRotator::ZeroRotator);
	}

	if (!HasActorOfType<ADarkwellPracticeTarget>(*World))
	{
		World->SpawnActor<ADarkwellPracticeTarget>(FVector(650.0f, 330.0f, 90.0f), FRotator::ZeroRotator);
		World->SpawnActor<ADarkwellPracticeTarget>(FVector(720.0f, -340.0f, 90.0f), FRotator::ZeroRotator);
	}

	if (!HasExactActorClass<ADarkwellStalkerCharacter>(*World))
	{
		if (ADarkwellStalkerCharacter* Stalker = World->SpawnActor<ADarkwellStalkerCharacter>(
			FVector(-650.0f, 430.0f, 92.0f),
			FRotator(0.0f, -35.0f, 0.0f)))
		{
			Stalker->ConfigurePersistentId(FName(TEXT("Enemy.Stalker.Prototype")));
		}
	}

	if (!HasActorOfType<ADarkwellWardenCharacter>(*World))
	{
		if (ADarkwellWardenCharacter* Warden = World->SpawnActor<ADarkwellWardenCharacter>(
			FVector(650.0f, 120.0f, 106.0f),
			FRotator(0.0f, 180.0f, 0.0f)))
		{
			Warden->ConfigurePersistentId(FName(TEXT("Enemy.Warden.FuseGuard")));
		}
	}

	if (SaveSubsystem)
	{
		SaveSubsystem->ApplyPendingLoad(*World);
		SaveSubsystem->CompleteWorldStart();
	}
}
