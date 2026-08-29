// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DarkwellVisionIntegrationGameMode.generated.h"

/** Native GameMode used only by /Game/Maps/L_VisionIntegration. */
UCLASS()
class DARKWELL_API ADarkwellVisionIntegrationGameMode final : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADarkwellVisionIntegrationGameMode();
	virtual void StartPlay() override;
};
