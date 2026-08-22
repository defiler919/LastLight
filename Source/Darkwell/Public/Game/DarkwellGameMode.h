// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DarkwellGameMode.generated.h"

/** Default game mode for the DARKWELL greybox prototype. */
UCLASS()
class DARKWELL_API ADarkwellGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADarkwellGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
};
