// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Gameplay/DarkwellVisibilityMath.h"
#include "UObject/Interface.h"
#include "DarkwellFogSubject.generated.h"

/** Implemented by actors whose presentation must obey the player's fog knowledge. */
UINTERFACE(MinimalAPI)
class UDarkwellFogSubject : public UInterface
{
	GENERATED_BODY()
};

class DARKWELL_API IDarkwellFogSubject
{
	GENERATED_BODY()

public:
	virtual void SetPlayerFogState(EDarkwellFogCellState NewState) = 0;
};
