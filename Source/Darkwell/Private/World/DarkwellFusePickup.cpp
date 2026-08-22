// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/DarkwellFusePickup.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Game/DarkwellGameState.h"
#include "Player/DarkwellCharacter.h"
#include "Save/DarkwellSaveSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ADarkwellFusePickup::ADarkwellFusePickup()
{
	PrimaryActorTick.bCanEverTick = false;

	FuseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FuseMesh"));
	SetRootComponent(FuseMesh);
	FuseMesh->SetRelativeScale3D(FVector(0.18f, 0.18f, 0.48f));
	FuseMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	FuseMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	FuseMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	FuseLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FuseLight"));
	FuseLight->SetupAttachment(FuseMesh);
	FuseLight->SetRelativeLocation(FVector(0.0f, 0.0f, 45.0f));
	FuseLight->SetLightColor(FLinearColor(0.25f, 1.0f, 0.34f));
	FuseLight->SetIntensity(1450.0f);
	FuseLight->SetAttenuationRadius(280.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		FuseMesh->SetStaticMesh(CylinderMesh.Object);
	}
}

bool ADarkwellFusePickup::CanInteract(const ADarkwellCharacter& Character) const
{
	const ADarkwellGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ADarkwellGameState>() : nullptr;
	return Character.CanAcceptGameplayInput() && GameState && !GameState->IsFuseCollected();
}

void ADarkwellFusePickup::Interact(ADarkwellCharacter& Character)
{
	ADarkwellGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ADarkwellGameState>() : nullptr;
	if (GameState && GameState->CollectGeneratorFuse())
	{
		if (UDarkwellSaveSubsystem* SaveSubsystem = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UDarkwellSaveSubsystem>()
			: nullptr)
		{
			SaveSubsystem->RequestAutosave(*GetWorld(), FName(TEXT("GeneratorFuseCollected")));
		}
		Destroy();
	}
}

FText ADarkwellFusePickup::GetInteractionPrompt(const ADarkwellCharacter& Character) const
{
	return NSLOCTEXT("Darkwell", "TakeGeneratorFuse", "Take generator fuse");
}
