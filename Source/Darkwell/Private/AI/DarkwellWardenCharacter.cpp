// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/DarkwellWardenCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "UObject/ConstructorHelpers.h"

ADarkwellWardenCharacter::ADarkwellWardenCharacter()
{
	EnemyArchetype = DarkwellGameplayTags::Enemy_Archetype_Warden;
	ThreatName = NSLOCTEXT("Darkwell", "EnemyWardenName", "WARDEN");

	MaxHealth = 260.0f;
	TorchDeterrenceRangeScale = 0.58f;
	TorchBoundarySafetyMargin = 35.0f;
	TorchBoundaryBuffer = 95.0f;
	LanternFocusSecondsToStun = 3.0f;
	LanternFocusDecayPerSecond = 0.12f;
	LanternFocusStunDuration = 5.0f;
	AttackRange = 155.0f;
	AttackDamage = 28.0f;
	AttackInterval = 1.65f;

	IdleStateColor = FLinearColor(0.12f, 0.025f, 0.2f);
	HuntingStateColor = FLinearColor(0.95f, 0.02f, 0.48f);
	InvestigatingStateColor = FLinearColor(0.82f, 0.18f, 0.05f);
	RepelledStateColor = FLinearColor(0.48f, 0.32f, 1.0f);
	StunnedStateColor = FLinearColor(0.7f, 0.96f, 1.0f);

	GetCapsuleComponent()->InitCapsuleSize(52.0f, 96.0f);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = 205.0f;
		Movement->MaxAcceleration = 780.0f;
		Movement->BrakingDecelerationWalking = 720.0f;
		Movement->RotationRate = FRotator(0.0f, 260.0f, 0.0f);
	}

	GreyboxBody->SetRelativeScale3D(FVector(0.84f, 0.84f, 1.85f));
	FacingMarker->SetRelativeLocation(FVector(70.0f, 0.0f, 18.0f));
	FacingMarker->SetRelativeScale3D(FVector(0.58f, 0.14f, 0.14f));
	StateLight->SetRelativeLocation(FVector(0.0f, 0.0f, 112.0f));
	StateLight->SetAttenuationRadius(340.0f);

	ArmorShell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArmorShell"));
	ArmorShell->SetupAttachment(GetCapsuleComponent());
	ArmorShell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArmorShell->SetCanEverAffectNavigation(false);
	ArmorShell->SetRelativeLocation(FVector(0.0f, 0.0f, 54.0f));
	ArmorShell->SetRelativeScale3D(FVector(0.58f, 1.16f, 0.26f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		ArmorShell->SetStaticMesh(CubeMesh.Object);
	}
}
