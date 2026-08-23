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
	void UpdateFocusedActorFromWorld();
	bool TryInteract();

	AActor* GetFocusedActor() const;
	FText GetFocusedPrompt() const;

private:
	AActor* FindBestFacingProximityActor() const;
	bool IsValidCandidate(AActor* Candidate) const;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0.0"))
	float MaxInteractionDistance = 300.0f;

	/** Forgiving half-angle used for every world interaction. */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float FacingInteractionHalfAngleDegrees = 60.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Interaction")
	TWeakObjectPtr<AActor> FocusedActor;
};
