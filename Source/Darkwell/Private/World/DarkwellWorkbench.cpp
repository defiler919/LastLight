// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/DarkwellWorkbench.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "Inventory/DarkwellCraftingRecipe.h"
#include "Inventory/DarkwellInventoryComponent.h"
#include "Player/DarkwellCharacter.h"
#include "Player/DarkwellPlayerController.h"
#include "UObject/ConstructorHelpers.h"

ADarkwellWorkbench::ADarkwellWorkbench()
{
	PrimaryActorTick.bCanEverTick = false;

	WorkbenchMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WorkbenchMesh"));
	SetRootComponent(WorkbenchMesh);
	WorkbenchMesh->SetRelativeScale3D(FVector(1.05f, 0.55f, 0.48f));
	WorkbenchMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WorkbenchMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	WorkbenchMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	WorkbenchLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("WorkbenchLight"));
	WorkbenchLight->SetupAttachment(WorkbenchMesh);
	WorkbenchLight->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	WorkbenchLight->SetLightColor(FLinearColor(0.1f, 0.85f, 0.75f));
	WorkbenchLight->SetIntensity(1100.0f);
	WorkbenchLight->SetAttenuationRadius(330.0f);
	WorkbenchLight->SetCastShadows(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		WorkbenchMesh->SetStaticMesh(CubeMesh.Object);
	}

	RecipeDefinition = TSoftObjectPtr<UDarkwellCraftingRecipe>(FSoftObjectPath(
		TEXT("/Game/Data/Recipes/DA_Recipe_ShotgunShells.DA_Recipe_ShotgunShells")));
}

bool ADarkwellWorkbench::TryCraftShells(ADarkwellCharacter& Character) const
{
	UDarkwellInventoryComponent* Inventory = Character.GetInventoryComponent();
	return Inventory && Inventory->TryExchange(
		GetCostItemTag(),
		GetScrapCost(),
		GetResultItemTag(),
		GetShellYield());
}

const UDarkwellCraftingRecipe* ADarkwellWorkbench::GetRecipeDefinition() const
{
	return RecipeDefinition.LoadSynchronous();
}

FText ADarkwellWorkbench::GetRecipeDisplayName() const
{
	const UDarkwellCraftingRecipe* Recipe = GetRecipeDefinition();
	return Recipe && !Recipe->DisplayName.IsEmpty()
		? Recipe->DisplayName
		: NSLOCTEXT("Darkwell", "FallbackShellRecipe", "Shotgun shells");
}

FGameplayTag ADarkwellWorkbench::GetCostItemTag() const
{
	const UDarkwellCraftingRecipe* Recipe = GetRecipeDefinition();
	return Recipe && Recipe->CostItemTag.IsValid()
		? Recipe->CostItemTag
		: DarkwellGameplayTags::Item_Material_Scrap;
}

FGameplayTag ADarkwellWorkbench::GetResultItemTag() const
{
	const UDarkwellCraftingRecipe* Recipe = GetRecipeDefinition();
	return Recipe && Recipe->ResultItemTag.IsValid()
		? Recipe->ResultItemTag
		: DarkwellGameplayTags::Item_Ammo_ShotgunShell;
}

int32 ADarkwellWorkbench::GetScrapCost() const
{
	const UDarkwellCraftingRecipe* Recipe = GetRecipeDefinition();
	return Recipe ? FMath::Max(1, Recipe->CostQuantity) : FMath::Max(1, FallbackScrapCost);
}

int32 ADarkwellWorkbench::GetShellYield() const
{
	const UDarkwellCraftingRecipe* Recipe = GetRecipeDefinition();
	return Recipe ? FMath::Max(1, Recipe->ResultQuantity) : FMath::Max(1, FallbackShellYield);
}

bool ADarkwellWorkbench::CanInteract(const ADarkwellCharacter& Character) const
{
	return Character.CanAcceptGameplayInput();
}

void ADarkwellWorkbench::Interact(ADarkwellCharacter& Character)
{
	if (ADarkwellPlayerController* PlayerController = Cast<ADarkwellPlayerController>(Character.GetController()))
	{
		PlayerController->OpenWorkbench(this);
	}
}

FText ADarkwellWorkbench::GetInteractionPrompt(const ADarkwellCharacter& Character) const
{
	return NSLOCTEXT("Darkwell", "UseWorkbench", "Use shell workbench");
}
