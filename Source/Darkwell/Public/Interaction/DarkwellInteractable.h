// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DarkwellInteractable.generated.h"

class ADarkwellCharacter;

UINTERFACE(MinimalAPI)
class UDarkwellInteractable : public UInterface
{
	GENERATED_BODY()
};

/** Native contract for focused world interactions. Core interaction rules remain in C++. */
class DARKWELL_API IDarkwellInteractable
{
	GENERATED_BODY()

public:
	virtual bool CanInteract(const ADarkwellCharacter& Character) const = 0;
	virtual void Interact(ADarkwellCharacter& Character) = 0;
	virtual FText GetInteractionPrompt(const ADarkwellCharacter& Character) const = 0;
	virtual void OnInteractionFocusChanged(bool bFocused) {}
};
