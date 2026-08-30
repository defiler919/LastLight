// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarkwellVisionIntegrationFixture.generated.h"

class UDirectionalLightComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;
class UTexture;

namespace Darkwell::SightWeaveSurface
{
	inline constexpr int32 SurfaceCategoryCustomPrimitiveDataIndex = 0;
	inline constexpr int32 WallSampleDirectionXCustomPrimitiveDataIndex = 1;
	inline constexpr int32 WallSampleDirectionYCustomPrimitiveDataIndex = 2;
	inline constexpr int32 WallSampleDistanceCustomPrimitiveDataIndex = 3;
	inline constexpr float GroundCategory = 0.0f;
	inline constexpr float WallOrCubeSideCategory = 1.0f;
	inline constexpr float RememberableStaticCategory = 2.0f;
	inline constexpr float NeverRememberCategory = 3.0f;
	inline constexpr float WallConservativeSampleBiasCentimeters = 7.5f;
	inline constexpr float FixtureWallHalfThicknessCentimeters = 20.0f;
	inline constexpr float FixtureWallSampleDistanceCentimeters =
		FixtureWallHalfThicknessCentimeters + WallConservativeSampleBiasCentimeters;
}

struct DARKWELL_API FDarkwellSightWeaveSurfaceWeights
{
	float Live = 0.0f;
	float Remembered = 0.0f;
	float Unknown = 1.0f;
};

/** CPU oracle for the project surface material's state precedence and wall sampling. */
class DARKWELL_API FDarkwellSightWeaveSurfaceMath final
{
public:
	static float ResolveConservativeState(
		float Center,
		float PositiveNormal,
		float NegativeNormal,
		float SurfaceCategory);
	static FDarkwellSightWeaveSurfaceWeights ResolveWeights(
		float EncodedState,
		float LiveFeather,
		float RememberedValidity,
		float ScopeValidity,
		float SurfaceCategory);
};

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
	bool EnableSightWeaveSurfaceMaterial(
		UTexture* StateTexture,
		FVector2D WorldMin,
		FVector2D InvWorldExtent,
		bool bDiagnosticFogOff = false);
	void DisableSightWeaveSurfaceMaterial();
	bool IsSightWeaveSurfaceMaterialEnabled() const
	{
		return SurfaceGroundMaterial != nullptr
			&& SurfaceWallMaterial != nullptr
			&& SurfaceStaticMaterial != nullptr;
	}

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

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SurfaceGroundMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SurfaceWallMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SurfaceStaticMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OriginalGroundMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OriginalWallSouthMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OriginalWallNorthMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OriginalStaticMaterial;

	bool bSurfaceMaterialOverrideCaptured = false;
};
