// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DarkwellInteractionComponent.generated.h"

class AActor;

/** Owns focus validation and interaction dispatch for the local player. */
UCLASS(ClassGroup = (Darkwell), meta = (BlueprintSpawnableComponent))
class DARKWELL_API UDarkwellInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDarkwellInteractionComponent();

	void UpdateFocusedActor(AActor* Candidate);
	bool TryInteract();

	AActor* GetFocusedActor() const;
	FText GetFocusedPrompt() const;

private:
	bool IsValidCandidate(AActor* Candidate) const;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0.0"))
	float MaxInteractionDistance = 300.0f;

	TWeakObjectPtr<AActor> FocusedActor;
};
