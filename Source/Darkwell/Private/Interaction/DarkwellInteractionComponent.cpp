// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/DarkwellInteractionComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interaction/DarkwellInteractable.h"
#include "Player/DarkwellCharacter.h"
#include "Player/DarkwellPlayerMath.h"

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

void UDarkwellInteractionComponent::UpdateFocusedActorFromWorld()
{
	UpdateFocusedActor(FindBestFacingProximityActor());
}

bool UDarkwellInteractionComponent::TryInteract()
{
	UpdateFocusedActorFromWorld();
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

AActor* UDarkwellInteractionComponent::FindBestFacingProximityActor() const
{
	const ADarkwellCharacter* Character = Cast<ADarkwellCharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Character || !World || MaxInteractionDistance <= 0.0f)
	{
		return nullptr;
	}

	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DarkwellFacingWorldInteraction), false, Character);
	TArray<FOverlapResult> Overlaps;
	if (!World->OverlapMultiByObjectType(
		Overlaps,
		Character->GetActorLocation(),
		FQuat::Identity,
		ObjectTypes,
		FCollisionShape::MakeSphere(MaxInteractionDistance),
		QueryParams))
	{
		return nullptr;
	}

	AActor* BestCandidate = nullptr;
	float BestAlignment = -2.0f;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (!Candidate)
		{
			continue;
		}

		const IDarkwellInteractable* Interactable = Cast<IDarkwellInteractable>(Candidate);
		if (!Interactable || !IsValidCandidate(Candidate))
		{
			continue;
		}

		const UPrimitiveComponent* OverlapComponent = Overlap.GetComponent();
		const FVector CandidateLocation = OverlapComponent
			? OverlapComponent->Bounds.Origin
			: Candidate->GetActorLocation();
		FCollisionQueryParams VisibilityParams(SCENE_QUERY_STAT(DarkwellFacingInteractionVisibility), true, Character);
		FHitResult VisibilityHit;
		const bool bVisibilityBlocked = World->LineTraceSingleByChannel(
			VisibilityHit,
			Character->GetActorLocation(),
			CandidateLocation,
			ECC_Visibility,
			VisibilityParams);
		if (bVisibilityBlocked && VisibilityHit.GetActor() != Candidate)
		{
			continue;
		}

		const FVector CandidateOffset = CandidateLocation - Character->GetActorLocation();
		if (!Darkwell::PlayerMath::IsFacingProximityCandidate(
			Character->GetActorForwardVector(),
			CandidateOffset,
			MaxInteractionDistance,
			FacingInteractionHalfAngleDegrees))
		{
			continue;
		}

		const FVector PlanarOffset(CandidateOffset.X, CandidateOffset.Y, 0.0f);
		const float DistanceSquared = PlanarOffset.SizeSquared();
		const float Alignment = FVector::DotProduct(
			Character->GetActorForwardVector().GetSafeNormal2D(),
			PlanarOffset.GetSafeNormal());
		if (Darkwell::PlayerMath::IsFacingInteractionCandidatePreferred(
			Alignment,
			DistanceSquared,
			BestAlignment,
			BestDistanceSquared))
		{
			BestAlignment = Alignment;
			BestDistanceSquared = DistanceSquared;
			BestCandidate = Candidate;
		}
	}

	return BestCandidate;
}

bool UDarkwellInteractionComponent::IsValidCandidate(AActor* Candidate) const
{
	const ADarkwellCharacter* Character = Cast<ADarkwellCharacter>(GetOwner());
	const IDarkwellInteractable* Interactable = Cast<IDarkwellInteractable>(Candidate);
	if (!Character || !IsValid(Candidate) || !Interactable)
	{
		return false;
	}

	const FBox CandidateBounds = Candidate->GetComponentsBoundingBox(true);
	const float DistanceSquared = CandidateBounds.IsValid
		? CandidateBounds.ComputeSquaredDistanceToPoint(Character->GetActorLocation())
		: FVector::DistSquared(Character->GetActorLocation(), Candidate->GetActorLocation());
	return DistanceSquared <= FMath::Square(MaxInteractionDistance) && Interactable->CanInteract(*Character);
}
