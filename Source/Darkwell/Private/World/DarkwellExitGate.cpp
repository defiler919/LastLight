// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/DarkwellExitGate.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Game/DarkwellGameState.h"
#include "Player/DarkwellCharacter.h"
#include "UObject/ConstructorHelpers.h"

ADarkwellExitGate::ADarkwellExitGate()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ExitPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExitPanel"));
	ExitPanel->SetupAttachment(SceneRoot);
	ExitPanel->SetRelativeLocation(FVector(0.0f, 0.0f, 115.0f));
	ExitPanel->SetRelativeScale3D(FVector(0.22f, 1.25f, 2.3f));
	ExitPanel->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ExitPanel->SetCollisionResponseToAllChannels(ECR_Ignore);
	ExitPanel->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	StatusLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusLight"));
	StatusLight->SetupAttachment(SceneRoot);
	StatusLight->SetRelativeLocation(FVector(-35.0f, 0.0f, 210.0f));
	StatusLight->SetAttenuationRadius(360.0f);
	StatusLight->SetIntensity(1900.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		ExitPanel->SetStaticMesh(CubeMesh.Object);
	}
}

void ADarkwellExitGate::BeginPlay()
{
	Super::BeginPlay();
	if (ADarkwellGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ADarkwellGameState>() : nullptr)
	{
		GameState->OnMissionStateChanged.AddUObject(this, &ThisClass::HandleMissionStateChanged);
	}
	RefreshPresentation();
}

void ADarkwellExitGate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ADarkwellGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ADarkwellGameState>() : nullptr)
	{
		GameState->OnMissionStateChanged.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

bool ADarkwellExitGate::CanInteract(const ADarkwellCharacter& Character) const
{
	const ADarkwellGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ADarkwellGameState>() : nullptr;
	return Character.CanAcceptGameplayInput() && GameState && !GameState->IsEscapeComplete();
}

void ADarkwellExitGate::Interact(ADarkwellCharacter& Character)
{
	ADarkwellGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ADarkwellGameState>() : nullptr;
	if (GameState && GameState->CompleteEscape())
	{
		Character.CompleteEscape();
	}
}

FText ADarkwellExitGate::GetInteractionPrompt(const ADarkwellCharacter& Character) const
{
	const ADarkwellGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ADarkwellGameState>() : nullptr;
	return GameState && GameState->IsFuseCollected()
		? NSLOCTEXT("Darkwell", "UseEmergencyExit", "Use emergency exit")
		: NSLOCTEXT("Darkwell", "ExitNeedsFuse", "Exit locked - find generator fuse");
}

void ADarkwellExitGate::HandleMissionStateChanged(const FGameplayTag NewState)
{
	RefreshPresentation();
}

void ADarkwellExitGate::RefreshPresentation()
{
	const ADarkwellGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ADarkwellGameState>() : nullptr;
	const bool bReady = GameState && GameState->IsFuseCollected() && !GameState->IsEscapeComplete();
	StatusLight->SetLightColor(bReady
		? FLinearColor(0.16f, 1.0f, 0.24f)
		: FLinearColor(1.0f, 0.035f, 0.015f));
	StatusLight->SetIntensity(GameState && GameState->IsEscapeComplete() ? 0.0f : 1900.0f);
}
