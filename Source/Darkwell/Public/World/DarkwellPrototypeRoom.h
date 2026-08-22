// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarkwellPrototypeRoom.generated.h"

class UPointLightComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** Native greybox architecture so the gameplay loop remains usable without external art packs. */
UCLASS()
class DARKWELL_API ADarkwellPrototypeRoom : public AActor
{
	GENERATED_BODY()

public:
	ADarkwellPrototypeRoom();

private:
	UStaticMeshComponent* CreateBlock(
		FName Name,
		UStaticMesh* CubeMesh,
		const FVector& RelativeLocation,
		const FVector& RelativeScale);

	UPROPERTY(VisibleAnywhere, Category = "Prototype")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Prototype")
	TArray<TObjectPtr<UStaticMeshComponent>> ArchitectureMeshes;

	UPROPERTY(VisibleAnywhere, Category = "Prototype")
	TObjectPtr<UPointLightComponent> EntranceLight;

	UPROPERTY(VisibleAnywhere, Category = "Prototype")
	TObjectPtr<UPointLightComponent> FarRoomLight;
};
