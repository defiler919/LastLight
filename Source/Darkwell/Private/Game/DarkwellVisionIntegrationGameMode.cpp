// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/DarkwellVisionIntegrationGameMode.h"

#include "EngineUtils.h"
#include "Game/DarkwellGameState.h"
#include "Player/DarkwellCharacter.h"
#include "Player/DarkwellPlayerController.h"
#include "UI/DarkwellHUD.h"
#include "Visibility/DarkwellVisionIntegrationFixture.h"
#include "Visibility/SightWeave/DarkwellSightWeaveWorldSubsystem.h"

ADarkwellVisionIntegrationGameMode::ADarkwellVisionIntegrationGameMode()
{
	DefaultPawnClass = ADarkwellCharacter::StaticClass();
	PlayerControllerClass = ADarkwellPlayerController::StaticClass();
	GameStateClass = ADarkwellGameState::StaticClass();
	HUDClass = ADarkwellHUD::StaticClass();
}

void ADarkwellVisionIntegrationGameMode::StartPlay()
{
	Super::StartPlay();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ADarkwellVisionIntegrationFixture* Fixture = nullptr;
	for (TActorIterator<ADarkwellVisionIntegrationFixture> It(World); It; ++It)
	{
		if (Fixture)
		{
			UE_LOG(LogTemp, Error,
				TEXT("M6P1 World=%s authority=SightWeave duplicate integration fixtures"),
				*World->GetName());
			return;
		}
		Fixture = *It;
	}

	UDarkwellSightWeaveWorldSubsystem* Adapter =
		World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
	if (!Fixture || !Adapter || !Adapter->RequestSightWeaveAuthority(Fixture))
	{
		UE_LOG(LogTemp, Error,
			TEXT("M6P1 World=%s authority=SightWeave request failed fixture=%d adapter=%d"),
			*World->GetName(), Fixture ? 1 : 0, Adapter ? 1 : 0);
	}
}
