// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/DarkwellStorageContainer.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "Inventory/DarkwellInventoryComponent.h"
#include "Player/DarkwellCharacter.h"
#include "Player/DarkwellPlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "VisionPresentation/DarkwellRememberablePropComponent.h"

ADarkwellStorageContainer::ADarkwellStorageContainer()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	StorageBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StorageBody"));
	StorageBody->SetupAttachment(SceneRoot);
	StorageBody->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	MovingPanelPivot = CreateDefaultSubobject<USceneComponent>(TEXT("MovingPanelPivot"));
	MovingPanelPivot->SetupAttachment(SceneRoot);

	MovingPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MovingPanel"));
	MovingPanel->SetupAttachment(MovingPanelPivot);
	MovingPanel->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MovingPanel->SetCollisionResponseToAllChannels(ECR_Ignore);
	MovingPanel->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	MovingPanel->SetCanEverAffectNavigation(false);

	StatusMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatusMarker"));
	StatusMarker->SetupAttachment(SceneRoot);
	StatusMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StatusMarker->SetCanEverAffectNavigation(false);

	StorageLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StorageLight"));
	StorageLight->SetupAttachment(SceneRoot);
	StorageLight->SetLightColor(FLinearColor(0.22f, 0.48f, 0.88f));
	StorageLight->SetIntensity(720.0f);
	StorageLight->SetAttenuationRadius(280.0f);
	StorageLight->SetCastShadows(false);

	InventoryComponent = CreateDefaultSubobject<UDarkwellInventoryComponent>(TEXT("InventoryComponent"));
	InventoryComponent->InitializeInventory(8);
	RememberablePropComponent = CreateDefaultSubobject<UDarkwellRememberablePropComponent>(
		TEXT("RememberablePropComponent"));
	RememberablePropComponent->AddMemoryPrimitive(StorageBody);
	RememberablePropComponent->AddMemoryPrimitive(MovingPanel);
	RememberablePropComponent->AddLiveOnlyComponent(StatusMarker);
	RememberablePropComponent->AddLiveOnlyComponent(StorageLight);
	DisplayName = NSLOCTEXT("Darkwell", "StorageDefaultName", "Storage");

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		StorageBody->SetStaticMesh(CubeMesh.Object);
		MovingPanel->SetStaticMesh(CubeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		StatusMarker->SetStaticMesh(SphereMesh.Object);
	}

	ApplyStorageStyle();
}

void ADarkwellStorageContainer::BeginPlay()
{
	Super::BeginPlay();
	InventoryComponent->OnInventoryChanged().AddUObject(this, &ThisClass::HandleInventoryChanged);
	HandleInventoryChanged();
}

void ADarkwellStorageContainer::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bFogPresentationLive)
	{
		SetActorTickEnabled(false);
		return;
	}

	const FRotator CurrentRotation = MovingPanelPivot->GetRelativeRotation();
	const float CurrentOpenAngle = StorageStyle == EDarkwellStorageStyle::Chest
		? -CurrentRotation.Roll
		: CurrentRotation.Yaw;
	const float NewOpenAngle = FMath::FInterpConstantTo(
		CurrentOpenAngle,
		TargetOpenAngle,
		DeltaSeconds,
		DegreesPerSecond);
	ApplyMovingPanelAngle(NewOpenAngle);

	if (FMath::IsNearlyEqual(NewOpenAngle, TargetOpenAngle, 0.1f))
	{
		ApplyMovingPanelAngle(TargetOpenAngle);
		SetActorTickEnabled(false);
	}
}

void ADarkwellStorageContainer::ConfigureStorage(
	const FName InPersistentId,
	const FText& InDisplayName,
	const int32 ScrapQuantity,
	const int32 ShellQuantity,
	const EDarkwellStorageStyle InStyle)
{
	PersistentId = InPersistentId;
	RememberablePropComponent->ConfigureStableId(InPersistentId);
	DisplayName = InDisplayName;
	StorageStyle = InStyle;
	ApplyStorageStyle();
	InventoryComponent->AddItem(DarkwellGameplayTags::Item_Material_Scrap, ScrapQuantity);
	InventoryComponent->AddItem(DarkwellGameplayTags::Item_Ammo_ShotgunShell, ShellQuantity);
	HandleInventoryChanged();
}

bool ADarkwellStorageContainer::HasLoot() const
{
	return InventoryComponent && !InventoryComponent->IsEmpty();
}

void ADarkwellStorageContainer::SetContainerOpen(const bool bShouldOpen)
{
	bContainerOpen = bShouldOpen;
	TargetOpenAngle = bContainerOpen ? OpenAngle : (HasLoot() ? 0.0f : SearchedAjarAngle);
	RefreshPresentation();

	const FRotator CurrentRotation = MovingPanelPivot->GetRelativeRotation();
	const float CurrentOpenAngle = StorageStyle == EDarkwellStorageStyle::Chest
		? -CurrentRotation.Roll
		: CurrentRotation.Yaw;
	SetActorTickEnabled(bFogPresentationLive
		&& !FMath::IsNearlyEqual(CurrentOpenAngle, TargetOpenAngle, 0.1f));
}

bool ADarkwellStorageContainer::CanInteract(const ADarkwellCharacter& Character) const
{
	return Character.CanAcceptGameplayInput();
}

void ADarkwellStorageContainer::Interact(ADarkwellCharacter& Character)
{
	if (ADarkwellPlayerController* PlayerController = Cast<ADarkwellPlayerController>(Character.GetController()))
	{
		PlayerController->OpenContainer(InventoryComponent, DisplayName);
	}
}

FText ADarkwellStorageContainer::GetInteractionPrompt(const ADarkwellCharacter& Character) const
{
	if (!HasLoot())
	{
		return FText::Format(NSLOCTEXT("Darkwell", "InspectEmptyStorage", "Inspect {0} (empty)"), DisplayName);
	}

	return FText::Format(
		NSLOCTEXT("Darkwell", "SearchStorage", "Search {0} ({1} items)"),
		DisplayName,
		FText::AsNumber(InventoryComponent->GetTotalItemCount()));
}

void ADarkwellStorageContainer::OnInteractionFocusChanged(const bool bFocused)
{
	bInteractionFocused = bFocused;
	RefreshPresentation();
}

void ADarkwellStorageContainer::SetPlayerFogState(const EDarkwellFogCellState NewState)
{
	const bool bShouldBeLive = NewState == EDarkwellFogCellState::Visible;
	if (bShouldBeLive == bFogPresentationLive)
	{
		return;
	}

	bFogPresentationLive = bShouldBeLive;
	if (!bFogPresentationLive)
	{
		SetActorTickEnabled(false);
		return;
	}

	ApplyMovingPanelAngle(TargetOpenAngle);
	RefreshPresentation();
	SetActorTickEnabled(false);
}

void ADarkwellStorageContainer::ApplyStorageStyle()
{
	if (StorageStyle == EDarkwellStorageStyle::Cabinet)
	{
		StorageBody->SetRelativeLocation(FVector(0.0f, 0.0f, 28.0f));
		StorageBody->SetRelativeScale3D(FVector(0.65f, 0.4f, 1.05f));
		MovingPanelPivot->SetRelativeLocation(FVector(-32.5f, -22.0f, 28.0f));
		MovingPanel->SetRelativeLocation(FVector(32.5f, 0.0f, 0.0f));
		MovingPanel->SetRelativeScale3D(FVector(0.65f, 0.06f, 1.0f));
		StatusMarker->SetRelativeLocation(FVector(0.0f, 0.0f, 105.0f));
		StorageLight->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	}
	else
	{
		StorageBody->SetRelativeLocation(FVector(0.0f, 0.0f, -8.0f));
		StorageBody->SetRelativeScale3D(FVector(0.85f, 0.55f, 0.38f));
		MovingPanelPivot->SetRelativeLocation(FVector(0.0f, 27.5f, 15.0f));
		MovingPanel->SetRelativeLocation(FVector(0.0f, -27.5f, 0.0f));
		MovingPanel->SetRelativeScale3D(FVector(0.85f, 0.55f, 0.08f));
		StatusMarker->SetRelativeLocation(FVector(0.0f, 0.0f, 55.0f));
		StorageLight->SetRelativeLocation(FVector(0.0f, 0.0f, 78.0f));
	}

	StatusMarker->SetRelativeScale3D(FVector(0.12f));
	ApplyMovingPanelAngle(TargetOpenAngle);
	RefreshPresentation();
}

void ADarkwellStorageContainer::ApplyMovingPanelAngle(const float OpenAmountDegrees)
{
	MovingPanelPivot->SetRelativeRotation(StorageStyle == EDarkwellStorageStyle::Chest
		? FRotator(0.0f, 0.0f, -OpenAmountDegrees)
		: FRotator(0.0f, OpenAmountDegrees, 0.0f));
}

void ADarkwellStorageContainer::HandleInventoryChanged()
{
	if (!bContainerOpen)
	{
		TargetOpenAngle = HasLoot() ? 0.0f : SearchedAjarAngle;
	}
	RefreshPresentation();

	const FRotator CurrentRotation = MovingPanelPivot->GetRelativeRotation();
	const float CurrentOpenAngle = StorageStyle == EDarkwellStorageStyle::Chest
		? -CurrentRotation.Roll
		: CurrentRotation.Yaw;
	SetActorTickEnabled(bFogPresentationLive
		&& !FMath::IsNearlyEqual(CurrentOpenAngle, TargetOpenAngle, 0.1f));
}

void ADarkwellStorageContainer::RefreshPresentation()
{
	if (!bFogPresentationLive)
	{
		return;
	}

	const bool bHasLoot = HasLoot();
	if (bHasLoot)
	{
		StorageLight->SetLightColor(FLinearColor(0.12f, 0.65f, 1.0f));
		StorageLight->SetIntensity(bInteractionFocused ? 1250.0f : (bContainerOpen ? 950.0f : 720.0f));
		StorageLight->SetAttenuationRadius(bInteractionFocused ? 340.0f : 280.0f);
		StatusMarker->SetVisibility(!bContainerOpen, true);
		StatusMarker->SetRelativeScale3D(FVector(bInteractionFocused ? 0.16f : 0.12f));
	}
	else
	{
		StorageLight->SetLightColor(FLinearColor(0.45f, 0.16f, 0.035f));
		StorageLight->SetIntensity(bInteractionFocused ? 300.0f : 90.0f);
		StorageLight->SetAttenuationRadius(bInteractionFocused ? 190.0f : 130.0f);
		StatusMarker->SetVisibility(false, true);
	}
}
