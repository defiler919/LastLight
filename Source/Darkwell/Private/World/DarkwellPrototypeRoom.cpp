// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/DarkwellPrototypeRoom.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ADarkwellPrototypeRoom::ADarkwellPrototypeRoom()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* CubeMesh = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;

	CreateBlock(TEXT("Floor"), CubeMesh, FVector(0.0f, 0.0f, -20.0f), FVector(20.0f, 16.0f, 0.4f));
	CreateBlock(TEXT("WestWall"), CubeMesh, FVector(-1000.0f, 0.0f, 150.0f), FVector(0.4f, 16.0f, 3.0f));
	CreateBlock(TEXT("EastWall"), CubeMesh, FVector(1000.0f, 0.0f, 150.0f), FVector(0.4f, 16.0f, 3.0f));
	CreateBlock(TEXT("SouthWall"), CubeMesh, FVector(0.0f, -800.0f, 150.0f), FVector(20.0f, 0.4f, 3.0f));
	CreateBlock(TEXT("NorthWall"), CubeMesh, FVector(0.0f, 800.0f, 150.0f), FVector(20.0f, 0.4f, 3.0f));

	CreateBlock(TEXT("DividerSouth"), CubeMesh, FVector(200.0f, -445.0f, 150.0f), FVector(0.4f, 7.1f, 3.0f));
	CreateBlock(TEXT("DividerNorth"), CubeMesh, FVector(200.0f, 445.0f, 150.0f), FVector(0.4f, 7.1f, 3.0f));
	CreateBlock(TEXT("DividerLintel"), CubeMesh, FVector(200.0f, 0.0f, 275.0f), FVector(0.4f, 1.8f, 0.5f));

	CreateBlock(TEXT("CrateA"), CubeMesh, FVector(-300.0f, 360.0f, 60.0f), FVector(1.2f, 1.2f, 1.2f));
	CreateBlock(TEXT("CrateB"), CubeMesh, FVector(-170.0f, 420.0f, 45.0f), FVector(0.9f, 0.9f, 0.9f));
	CreateBlock(TEXT("Cover"), CubeMesh, FVector(570.0f, -320.0f, 55.0f), FVector(2.8f, 0.7f, 1.1f));

	EntranceLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EntranceLight"));
	EntranceLight->SetupAttachment(SceneRoot);
	EntranceLight->SetRelativeLocation(FVector(-520.0f, -260.0f, 240.0f));
	EntranceLight->SetLightColor(FLinearColor(0.18f, 0.28f, 0.55f));
	EntranceLight->SetIntensity(1800.0f);
	EntranceLight->SetAttenuationRadius(700.0f);

	FarRoomLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FarRoomLight"));
	FarRoomLight->SetupAttachment(SceneRoot);
	FarRoomLight->SetRelativeLocation(FVector(610.0f, 270.0f, 210.0f));
	FarRoomLight->SetLightColor(FLinearColor(0.55f, 0.08f, 0.025f));
	FarRoomLight->SetIntensity(1300.0f);
	FarRoomLight->SetAttenuationRadius(560.0f);
}

UStaticMeshComponent* ADarkwellPrototypeRoom::CreateBlock(
	const FName Name,
	UStaticMesh* CubeMesh,
	const FVector& RelativeLocation,
	const FVector& RelativeScale)
{
	UStaticMeshComponent* Block = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Block->SetupAttachment(SceneRoot);
	Block->SetStaticMesh(CubeMesh);
	Block->SetRelativeLocation(RelativeLocation);
	Block->SetRelativeScale3D(RelativeScale);
	Block->SetCollisionProfileName(TEXT("BlockAll"));
	ArchitectureMeshes.Add(Block);
	return Block;
}
