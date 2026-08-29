// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visibility/DarkwellVisionIntegrationFixture.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "UObject/ConstructorHelpers.h"

namespace Darkwell::VisionIntegrationFixture
{
	constexpr double FloorHalfX = 1500.0;
	constexpr double FloorHalfY = 1000.0;
	constexpr double WallX = -20.0;
	constexpr double WallSouthMinY = -650.0;
	constexpr double WallSouthMaxY = -150.0;
	constexpr double WallNorthMinY = 150.0;
	constexpr double WallNorthMaxY = 650.0;

	TArray<FVector2D> Rectangle(
		const double MinX,
		const double MinY,
		const double MaxX,
		const double MaxY)
	{
		return {
			FVector2D(MinX, MinY),
			FVector2D(MaxX, MinY),
			FVector2D(MaxX, MaxY),
			FVector2D(MinX, MaxY)
		};
	}
}

ADarkwellVisionIntegrationFixture::ADarkwellVisionIntegrationFixture()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	SceneRoot->SetMobility(EComponentMobility::Static);

	Ground = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ground"));
	Ground->SetupAttachment(SceneRoot);
	Ground->SetRelativeLocation(FVector(0.0f, 0.0f, -50.0f));
	Ground->SetRelativeScale3D(FVector(30.0f, 20.0f, 1.0f));
	Ground->SetMobility(EComponentMobility::Static);
	Ground->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	WallSouth = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallSouth"));
	WallSouth->SetupAttachment(SceneRoot);
	WallSouth->SetRelativeLocation(FVector(0.0f, -400.0f, 100.0f));
	WallSouth->SetRelativeScale3D(FVector(0.4f, 5.0f, 2.0f));
	WallSouth->SetMobility(EComponentMobility::Static);
	WallSouth->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	WallNorth = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallNorth"));
	WallNorth->SetupAttachment(SceneRoot);
	WallNorth->SetRelativeLocation(FVector(0.0f, 400.0f, 100.0f));
	WallNorth->SetRelativeScale3D(FVector(0.4f, 5.0f, 2.0f));
	WallNorth->SetMobility(EComponentMobility::Static);
	WallNorth->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	MemoryLandmark = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MemoryLandmark"));
	MemoryLandmark->SetupAttachment(SceneRoot);
	MemoryLandmark->SetRelativeLocation(FVector(650.0f, 500.0f, 75.0f));
	MemoryLandmark->SetRelativeScale3D(FVector(1.5f));
	MemoryLandmark->SetMobility(EComponentMobility::Static);
	MemoryLandmark->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	GreyboxKeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("GreyboxKeyLight"));
	GreyboxKeyLight->SetupAttachment(SceneRoot);
	GreyboxKeyLight->SetRelativeRotation(FRotator(-55.0f, -35.0f, 0.0f));
	GreyboxKeyLight->SetIntensity(4.0f);
	GreyboxKeyLight->SetLightColor(FLinearColor(0.78f, 0.84f, 1.0f));
	GreyboxKeyLight->SetMobility(EComponentMobility::Stationary);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Ground->SetStaticMesh(CubeMesh.Object);
		WallSouth->SetStaticMesh(CubeMesh.Object);
		WallNorth->SetStaticMesh(CubeMesh.Object);
		MemoryLandmark->SetStaticMesh(CubeMesh.Object);
	}
}

FBox2D ADarkwellVisionIntegrationFixture::GetSightWeaveFloorBounds() const
{
	using namespace Darkwell::VisionIntegrationFixture;
	const FVector Origin = GetActorLocation();
	return FBox2D(
		FVector2D(Origin.X - FloorHalfX - 250.0, Origin.Y - FloorHalfY - 250.0),
		FVector2D(Origin.X + FloorHalfX + 250.0, Origin.Y + FloorHalfY + 250.0));
}

void ADarkwellVisionIntegrationFixture::BuildSightWeaveOccluderSegments(
	TArray<FDarkwellVisionIntegrationSegment>& OutSegments) const
{
	using namespace Darkwell::VisionIntegrationFixture;
	const FVector Origin = GetActorLocation();
	OutSegments.Reset(2);
	FDarkwellVisionIntegrationSegment& South = OutSegments.AddDefaulted_GetRef();
	South.A = FVector2D(Origin.X + WallX, Origin.Y + WallSouthMinY);
	South.B = FVector2D(Origin.X + WallX, Origin.Y + WallSouthMaxY);
	South.ZMin = Origin.Z - 10.0f;
	South.ZMax = Origin.Z + 250.0f;
	FDarkwellVisionIntegrationSegment& North = OutSegments.AddDefaulted_GetRef();
	North.A = FVector2D(Origin.X + WallX, Origin.Y + WallNorthMinY);
	North.B = FVector2D(Origin.X + WallX, Origin.Y + WallNorthMaxY);
	North.ZMin = South.ZMin;
	North.ZMax = South.ZMax;
}

void ADarkwellVisionIntegrationFixture::BuildSightWeaveStaticSurfaces(
	TArray<FDarkwellVisionIntegrationSurface>& OutSurfaces) const
{
	using namespace Darkwell::VisionIntegrationFixture;
	const FVector Origin = GetActorLocation();
	OutSurfaces.Reset(4);
	FDarkwellVisionIntegrationSurface& Floor = OutSurfaces.AddDefaulted_GetRef();
	Floor.WorldFootprint = Rectangle(
		Origin.X - FloorHalfX,
		Origin.Y - FloorHalfY,
		Origin.X + FloorHalfX,
		Origin.Y + FloorHalfY);
	Floor.NeutralIntensity = 74;
	FDarkwellVisionIntegrationSurface& South = OutSurfaces.AddDefaulted_GetRef();
	South.WorldFootprint = Rectangle(
		Origin.X - 25.0,
		Origin.Y + WallSouthMinY,
		Origin.X + 25.0,
		Origin.Y + WallSouthMaxY);
	South.NeutralIntensity = 150;
	FDarkwellVisionIntegrationSurface& North = OutSurfaces.AddDefaulted_GetRef();
	North.WorldFootprint = Rectangle(
		Origin.X - 25.0,
		Origin.Y + WallNorthMinY,
		Origin.X + 25.0,
		Origin.Y + WallNorthMaxY);
	North.NeutralIntensity = 150;
	FDarkwellVisionIntegrationSurface& Landmark = OutSurfaces.AddDefaulted_GetRef();
	Landmark.WorldFootprint = Rectangle(
		Origin.X + 575.0,
		Origin.Y + 425.0,
		Origin.X + 725.0,
		Origin.Y + 575.0);
	Landmark.NeutralIntensity = 196;
}
