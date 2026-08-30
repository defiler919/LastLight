// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/DarkwellFogSubject.h"
#include "Interaction/DarkwellInteractable.h"
#include "DarkwellStorageContainer.generated.h"

class UDarkwellInventoryComponent;
class UDarkwellRememberablePropComponent;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EDarkwellStorageStyle : uint8
{
	Chest,
	Cabinet
};

/** Shared greybox storage used for chests and cabinets. */
UCLASS()
class DARKWELL_API ADarkwellStorageContainer : public AActor, public IDarkwellInteractable, public IDarkwellFogSubject
{
	GENERATED_BODY()

public:
	ADarkwellStorageContainer();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void ConfigureStorage(
		FName InPersistentId,
		const FText& InDisplayName,
		int32 ScrapQuantity,
		int32 ShellQuantity,
		EDarkwellStorageStyle InStyle = EDarkwellStorageStyle::Chest);
	UDarkwellInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
	const FText& GetDisplayName() const { return DisplayName; }
	FName GetPersistentId() const { return PersistentId; }
	UDarkwellRememberablePropComponent* GetRememberablePropComponent() const
	{
		return RememberablePropComponent;
	}
	bool HasLoot() const;
	bool IsContainerOpen() const { return bContainerOpen; }
	void SetContainerOpen(bool bShouldOpen);

	virtual bool CanInteract(const ADarkwellCharacter& Character) const override;
	virtual void Interact(ADarkwellCharacter& Character) override;
	virtual FText GetInteractionPrompt(const ADarkwellCharacter& Character) const override;
	virtual void OnInteractionFocusChanged(bool bFocused) override;
	virtual void SetPlayerFogState(EDarkwellFogCellState NewState) override;

private:
	void ApplyStorageStyle();
	void ApplyMovingPanelAngle(float OpenAmountDegrees);
	void HandleInventoryChanged();
	void RefreshPresentation();

	UPROPERTY(VisibleAnywhere, Category = "Storage")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Storage")
	TObjectPtr<UStaticMeshComponent> StorageBody;

	UPROPERTY(VisibleAnywhere, Category = "Storage")
	TObjectPtr<USceneComponent> MovingPanelPivot;

	UPROPERTY(VisibleAnywhere, Category = "Storage")
	TObjectPtr<UStaticMeshComponent> MovingPanel;

	UPROPERTY(VisibleAnywhere, Category = "Storage")
	TObjectPtr<UStaticMeshComponent> StatusMarker;

	UPROPERTY(VisibleAnywhere, Category = "Storage")
	TObjectPtr<UPointLightComponent> StorageLight;

	UPROPERTY(VisibleAnywhere, Category = "Storage")
	TObjectPtr<UDarkwellInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, Category = "Fog Memory")
	TObjectPtr<UDarkwellRememberablePropComponent> RememberablePropComponent;

	UPROPERTY(VisibleInstanceOnly, Category = "Storage")
	FText DisplayName;

	UPROPERTY(EditInstanceOnly, Category = "Persistence")
	FName PersistentId;

	UPROPERTY(VisibleInstanceOnly, Category = "Storage")
	EDarkwellStorageStyle StorageStyle = EDarkwellStorageStyle::Chest;

	UPROPERTY(VisibleInstanceOnly, Category = "Storage")
	bool bContainerOpen = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Storage")
	bool bInteractionFocused = false;

	UPROPERTY(EditDefaultsOnly, Category = "Storage", meta = (ClampMin = "1.0", ClampMax = "150.0"))
	float OpenAngle = 78.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Storage", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float SearchedAjarAngle = 14.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Storage", meta = (ClampMin = "1.0"))
	float DegreesPerSecond = 180.0f;

	float TargetOpenAngle = 0.0f;
	bool bFogPresentationLive = true;
};
