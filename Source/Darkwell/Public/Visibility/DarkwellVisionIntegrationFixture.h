// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarkwellVisionIntegrationFixture.generated.h"

class UDirectionalLightComponent;
class USceneComponent;
class UStaticMeshComponent;

struct DARKWELL_API FDarkwellVisionIntegrationSegment
{
	FVector2D A = FVector2D::ZeroVector;
	FVector2D B = FVector2D::ZeroVector;
	float ZMin = 0.0f;
	float ZMax = 250.0f;
};

struct DARKWELL_API FDarkwellVisionIntegrationSurface
{
	TArray<FVector2D> WorldFootprint;
	uint8 NeutralIntensity = 112;
};

/** Native greybox/static-memory declaration placed in the dedicated integration map. */
UCLASS()
class DARKWELL_API ADarkwellVisionIntegrationFixture final : public AActor
{
	GENERATED_BODY()

public:
	ADarkwellVisionIntegrationFixture();

	FBox2D GetSightWeaveFloorBounds() const;
	void BuildSightWeaveOccluderSegments(
		TArray<FDarkwellVisionIntegrationSegment>& OutSegments) const;
	void BuildSightWeaveStaticSurfaces(
		TArray<FDarkwellVisionIntegrationSurface>& OutSurfaces) const;

private:
	UPROPERTY(VisibleAnywhere, Category = "Integration")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Integration")
	TObjectPtr<UStaticMeshComponent> Ground;

	UPROPERTY(VisibleAnywhere, Category = "Integration")
	TObjectPtr<UStaticMeshComponent> WallSouth;

	UPROPERTY(VisibleAnywhere, Category = "Integration")
	TObjectPtr<UStaticMeshComponent> WallNorth;

	UPROPERTY(VisibleAnywhere, Category = "Integration")
	TObjectPtr<UStaticMeshComponent> MemoryLandmark;

	/** Rendered illumination only; it is never a legal-light authority. */
	UPROPERTY(VisibleAnywhere, Category = "Integration")
	TObjectPtr<UDirectionalLightComponent> GreyboxKeyLight;
};
