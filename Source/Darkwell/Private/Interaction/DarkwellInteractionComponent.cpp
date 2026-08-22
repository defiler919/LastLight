// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/DarkwellInteractionComponent.h"

#include "GameFramework/Actor.h"
#include "Interaction/DarkwellInteractable.h"
#include "Player/DarkwellCharacter.h"

UDarkwellInteractionComponent::UDarkwellInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDarkwellInteractionComponent::UpdateFocusedActor(AActor* Candidate)
{
	AActor* NewFocusedActor = IsValidCandidate(Candidate) ? Candidate : nullptr;
	if (FocusedActor.Get() == NewFocusedActor)
	{
		return;
	}

	if (AActor* PreviousActor = FocusedActor.Get())
	{
		if (IDarkwellInteractable* PreviousInteractable = Cast<IDarkwellInteractable>(PreviousActor))
		{
			PreviousInteractable->OnInteractionFocusChanged(false);
		}
	}

	FocusedActor = NewFocusedActor;
	if (IDarkwellInteractable* NewInteractable = Cast<IDarkwellInteractable>(NewFocusedActor))
	{
		NewInteractable->OnInteractionFocusChanged(true);
	}
}

bool UDarkwellInteractionComponent::TryInteract()
{
	AActor* Candidate = FocusedActor.Get();
	if (!IsValidCandidate(Candidate))
	{
		FocusedActor.Reset();
		return false;
	}

	ADarkwellCharacter* Character = Cast<ADarkwellCharacter>(GetOwner());
	IDarkwellInteractable* Interactable = Cast<IDarkwellInteractable>(Candidate);
	check(Character && Interactable);
	Interactable->Interact(*Character);
	if (FocusedActor.Get() == Candidate)
	{
		UpdateFocusedActor(Candidate);
	}
	return true;
}

AActor* UDarkwellInteractionComponent::GetFocusedActor() const
{
	return FocusedActor.Get();
}

FText UDarkwellInteractionComponent::GetFocusedPrompt() const
{
	const ADarkwellCharacter* Character = Cast<ADarkwellCharacter>(GetOwner());
	AActor* Candidate = FocusedActor.Get();
	const IDarkwellInteractable* Interactable = Cast<IDarkwellInteractable>(Candidate);
	return Character && Interactable && IsValidCandidate(Candidate)
		? Interactable->GetInteractionPrompt(*Character)
		: FText::GetEmpty();
}

bool UDarkwellInteractionComponent::IsValidCandidate(AActor* Candidate) const
{
	const ADarkwellCharacter* Character = Cast<ADarkwellCharacter>(GetOwner());
	const IDarkwellInteractable* Interactable = Cast<IDarkwellInteractable>(Candidate);
	if (!Character || !IsValid(Candidate) || !Interactable)
	{
		return false;
	}

	const float DistanceSquared = FVector::DistSquared(Character->GetActorLocation(), Candidate->GetActorLocation());
	return DistanceSquared <= FMath::Square(MaxInteractionDistance) && Interactable->CanInteract(*Character);
}
