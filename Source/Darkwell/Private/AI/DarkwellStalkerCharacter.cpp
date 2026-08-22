// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/DarkwellStalkerCharacter.h"

#include "AI/DarkwellStalkerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "UObject/ConstructorHelpers.h"

ADarkwellStalkerCharacter::ADarkwellStalkerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	EnemyArchetype = DarkwellGameplayTags::Enemy_Archetype_Stalker;
	ThreatName = NSLOCTEXT("Darkwell", "EnemyStalkerName", "STALKER");

	GetCapsuleComponent()->InitCapsuleSize(38.0f, 82.0f);
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.0f, 420.0f, 0.0f);
	Movement->MaxWalkSpeed = 285.0f;
	Movement->MaxAcceleration = 1100.0f;
	Movement->BrakingDecelerationWalking = 900.0f;

	AIControllerClass = ADarkwellStalkerController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GreyboxBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GreyboxBody"));
	GreyboxBody->SetupAttachment(GetCapsuleComponent());
	GreyboxBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GreyboxBody->SetCanEverAffectNavigation(false);
	GreyboxBody->SetRelativeScale3D(FVector(0.62f, 0.62f, 1.55f));

	FacingMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FacingMarker"));
	FacingMarker->SetupAttachment(GetCapsuleComponent());
	FacingMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FacingMarker->SetCanEverAffectNavigation(false);
	FacingMarker->SetRelativeLocation(FVector(52.0f, 0.0f, 12.0f));
	FacingMarker->SetRelativeScale3D(FVector(0.46f, 0.1f, 0.1f));

	StateLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StateLight"));
	StateLight->SetupAttachment(GetCapsuleComponent());
	StateLight->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	StateLight->SetAttenuationRadius(260.0f);
	StateLight->SetIntensity(1000.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		GreyboxBody->SetStaticMesh(CylinderMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		FacingMarker->SetStaticMesh(CubeMesh.Object);
	}
}

void ADarkwellStalkerCharacter::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	SetBehaviorState(DarkwellGameplayTags::State_Enemy_Idle);
}

void ADarkwellStalkerCharacter::SetPlayerFogState(const EDarkwellFogCellState NewState)
{
	SetActorHiddenInGame(NewState != EDarkwellFogCellState::Visible);
}

float ADarkwellStalkerCharacter::TakeDamage(
	const float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	const float AppliedDamage = FMath::Min(Health, FMath::Max(0.0f, DamageAmount));
	Health -= AppliedDamage;
	if (Health <= 0.0f)
	{
		SetBehaviorState(DarkwellGameplayTags::State_Enemy_Dead);
		ApplyDeadState();
	}
	return AppliedDamage;
}

void ADarkwellStalkerCharacter::SetBehaviorState(const FGameplayTag NewState)
{
	if (!IsAlive() && NewState != DarkwellGameplayTags::State_Enemy_Dead)
	{
		return;
	}

	BehaviorState = NewState;
	RefreshPresentation();
}

bool ADarkwellStalkerCharacter::IsAlive() const
{
	return Health > 0.0f && BehaviorState != DarkwellGameplayTags::State_Enemy_Dead;
}

void ADarkwellStalkerCharacter::RestorePersistentState(
	const FTransform& SavedTransform,
	const float SavedHealth,
	const FGameplayTag SavedBehaviorState,
	const bool bSavedAlive)
{
	SetActorTransform(SavedTransform, false, nullptr, ETeleportType::TeleportPhysics);
	Health = FMath::Clamp(SavedHealth, 0.0f, MaxHealth);
	if (!bSavedAlive || Health <= 0.0f || SavedBehaviorState == DarkwellGameplayTags::State_Enemy_Dead)
	{
		Health = 0.0f;
		BehaviorState = DarkwellGameplayTags::State_Enemy_Dead;
		ApplyDeadState();
		RefreshPresentation();
		return;
	}

	BehaviorState = SavedBehaviorState.IsValid()
		? SavedBehaviorState
		: DarkwellGameplayTags::State_Enemy_Idle;
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GreyboxBody->SetRelativeRotation(FRotator::ZeroRotator);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Walking);
	}
	RefreshPresentation();
}

void ADarkwellStalkerCharacter::ApplyDeadState()
{
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GreyboxBody->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
	DetachFromControllerPendingDestroy();
}

void ADarkwellStalkerCharacter::RefreshPresentation()
{
	if (!StateLight)
	{
		return;
	}

	FLinearColor Color = IdleStateColor;
	float Intensity = 550.0f;
	if (BehaviorState == DarkwellGameplayTags::State_Enemy_Hunting)
	{
		Color = HuntingStateColor;
		Intensity = 1700.0f;
	}
	else if (BehaviorState == DarkwellGameplayTags::State_Enemy_Investigating)
	{
		Color = InvestigatingStateColor;
		Intensity = 1150.0f;
	}
	else if (BehaviorState == DarkwellGameplayTags::State_Enemy_Repelled)
	{
		Color = RepelledStateColor;
		Intensity = 2100.0f;
	}
	else if (BehaviorState == DarkwellGameplayTags::State_Enemy_LightStunned)
	{
		Color = StunnedStateColor;
		Intensity = 3200.0f;
	}
	else if (BehaviorState == DarkwellGameplayTags::State_Enemy_Dead)
	{
		Color = FLinearColor::Black;
		Intensity = 0.0f;
	}

	StateLight->SetLightColor(Color);
	StateLight->SetIntensity(Intensity);
}
