// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/DarkwellDoor.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "Player/DarkwellCharacter.h"
#include "UObject/ConstructorHelpers.h"

ADarkwellDoor::ADarkwellDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DoorHinge = CreateDefaultSubobject<USceneComponent>(TEXT("DoorHinge"));
	DoorHinge->SetupAttachment(SceneRoot);
	DoorHinge->SetRelativeLocation(FVector(0.0f, -80.0f, 0.0f));

	DoorPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorPanel"));
	DoorPanel->SetupAttachment(DoorHinge);
	DoorPanel->SetRelativeLocation(FVector(0.0f, 80.0f, 110.0f));
	DoorPanel->SetRelativeScale3D(FVector(0.16f, 1.6f, 2.2f));
	DoorPanel->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	PassageLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PassageLight"));
	PassageLight->SetupAttachment(SceneRoot);
	PassageLight->SetRelativeLocation(FVector(0.0f, 0.0f, 225.0f));
	PassageLight->SetIntensity(900.0f);
	PassageLight->SetAttenuationRadius(300.0f);
	PassageLight->SetCastShadows(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		DoorPanel->SetStaticMesh(CubeMesh.Object);
	}
}

void ADarkwellDoor::BeginPlay()
{
	Super::BeginPlay();
	DoorState = DarkwellGameplayTags::State_World_Door_Closed;
	TargetYaw = 0.0f;
	DoorHinge->SetRelativeRotation(FRotator::ZeroRotator);
	RefreshPassageLight();
}

void ADarkwellDoor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float CurrentYaw = DoorHinge->GetRelativeRotation().Yaw;
	const float NewYaw = FMath::FInterpConstantTo(CurrentYaw, TargetYaw, DeltaSeconds, DegreesPerSecond);
	DoorHinge->SetRelativeRotation(FRotator(0.0f, NewYaw, 0.0f));

	if (FMath::IsNearlyEqual(NewYaw, TargetYaw, 0.1f))
	{
		DoorHinge->SetRelativeRotation(FRotator(0.0f, TargetYaw, 0.0f));
		SetActorTickEnabled(false);
	}
}

bool ADarkwellDoor::CanInteract(const ADarkwellCharacter& Character) const
{
	return Character.CanAcceptGameplayInput();
}

void ADarkwellDoor::Interact(ADarkwellCharacter& Character)
{
	SetDoorOpen(DoorState.MatchesTagExact(DarkwellGameplayTags::State_World_Door_Closed));
}

FText ADarkwellDoor::GetInteractionPrompt(const ADarkwellCharacter& Character) const
{
	return DoorState.MatchesTagExact(DarkwellGameplayTags::State_World_Door_Open)
		? NSLOCTEXT("Darkwell", "ClosePassageDoor", "Close passage door")
		: NSLOCTEXT("Darkwell", "OpenPassageDoor", "Open passage door");
}

bool ADarkwellDoor::RestoreDoorState(const FGameplayTag SavedDoorState)
{
	if (SavedDoorState != DarkwellGameplayTags::State_World_Door_Open
		&& SavedDoorState != DarkwellGameplayTags::State_World_Door_Closed)
	{
		return false;
	}

	DoorState = SavedDoorState;
	TargetYaw = DoorState == DarkwellGameplayTags::State_World_Door_Open ? OpenAngle : 0.0f;
	DoorHinge->SetRelativeRotation(FRotator(0.0f, TargetYaw, 0.0f));
	SetActorTickEnabled(false);
	RefreshPassageLight();
	return true;
}

void ADarkwellDoor::SetDoorOpen(const bool bShouldOpen)
{
	const FGameplayTag DesiredState = bShouldOpen
		? DarkwellGameplayTags::State_World_Door_Open
		: DarkwellGameplayTags::State_World_Door_Closed;

	if (DoorState.MatchesTagExact(DesiredState))
	{
		return;
	}

	DoorState = DesiredState;
	TargetYaw = bShouldOpen ? OpenAngle : 0.0f;
	RefreshPassageLight();
	SetActorTickEnabled(true);
}

void ADarkwellDoor::RefreshPassageLight() const
{
	const bool bOpen = DoorState.MatchesTagExact(DarkwellGameplayTags::State_World_Door_Open);
	PassageLight->SetLightColor(bOpen
		? FLinearColor(0.15f, 1.0f, 0.2f)
		: FLinearColor(1.0f, 0.28f, 0.04f));
}
