// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AI/DarkwellStalkerCharacter.h"
#include "DarkwellWardenCharacter.generated.h"

class UStaticMeshComponent;

/** Slow high-threat fuse guard: resists the torch boundary but remains vulnerable to full lantern focus. */
UCLASS()
class DARKWELL_API ADarkwellWardenCharacter : public ADarkwellStalkerCharacter
{
	GENERATED_BODY()

public:
	ADarkwellWardenCharacter();

private:
	UPROPERTY(VisibleAnywhere, Category = "Enemy|Presentation")
	TObjectPtr<UStaticMeshComponent> ArmorShell;
};
