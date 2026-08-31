// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarkwellVisionIntegrationFixture.generated.h"

class UDirectionalLightComponent;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UDarkwellRememberablePropComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTexture;

namespace Darkwell::SightWeaveSurface
{
	inline constexpr int32 SurfaceCategoryCustomPrimitiveDataIndex = 0;
	// CPD[1]/[2] are intentionally unused. A primitive-wide direction cannot
	// represent the independently rendered faces of a wall or six-sided mesh.
	inline constexpr int32 WallSampleDistanceCustomPrimitiveDataIndex = 3;
	inline constexpr float GroundCategory = 0.0f;
	inline constexpr float WallOrCubeSideCategory = 1.0f;
	inline constexpr float RememberableStaticCategory = 2.0f;
	inline constexpr float NeverRememberCategory = 3.0f;
	inline constexpr float WallConservativeSampleBiasCentimeters = 7.5f;
	inline constexpr float FixtureWallHalfThicknessCentimeters = 20.0f;
	inline constexpr float FixtureWallSampleDistanceCentimeters =
		FixtureWallHalfThicknessCentimeters + WallConservativeSampleBiasCentimeters;
	inline constexpr float VerticalSurfaceNormalZThreshold = 0.75f;
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
	static FVector2D ResolveSurfaceSampleWorldPosition(
		FVector2D WorldPosition,
		FVector GeometricNormalWS,
		float SampleDistanceCentimeters);
	static FDarkwellSightWeaveSurfaceWeights ResolveWeights(
		float KnownCoverage,
		float LiveCoverage,
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
class DARKWELL_API ADarkwellVisionIntegrationFixture : public AActor
{
	GENERATED_BODY()

public:
	ADarkwellVisionIntegrationFixture();

	virtual FBox2D GetSightWeaveFloorBounds() const;
	virtual void BuildSightWeaveOccluderSegments(
		TArray<FDarkwellVisionIntegrationSegment>& OutSegments) const;
	virtual void BuildSightWeaveStaticSurfaces(
		TArray<FDarkwellVisionIntegrationSurface>& OutSurfaces) const;
	bool EnableSightWeaveSurfaceMaterial(
		UTexture* StateTexture,
		FVector2D WorldMin,
		FVector2D InvWorldExtent,
		bool bDiagnosticFogOff = false);
	void DisableSightWeaveSurfaceMaterial();
	bool EnableDarkwellProjectFogP1(
		UTexture* LiveCoverageTexture,
		FVector2D WorldMin,
		FVector2D InvWorldExtent);
	bool EnableDarkwellProjectFogP2(
		UTexture* LiveCoverageTexture,
		FVector2D WorldMin,
		FVector2D InvWorldExtent);
	bool EnableDarkwellProjectFogP3(
		UTexture* LiveCoverageTexture,
		FVector2D WorldMin,
		FVector2D InvWorldExtent);
	virtual bool EnableDarkwellProjectFogP4(
		UTexture* LiveCoverageTexture,
		FVector2D WorldMin,
		FVector2D InvWorldExtent);
	virtual void DisableDarkwellProjectFog();
	bool IsDarkwellProjectFogEnabled() const
	{
		return ProjectFogGroundMaterial != nullptr;
	}
	bool IsSightWeaveSurfaceMaterialEnabled() const
	{
		return SurfaceGroundMaterial != nullptr
			&& SurfaceWallMaterial != nullptr
			&& SurfaceStaticMaterial != nullptr;
	}
	UStaticMeshComponent* GetRememberablePropProof() const { return RememberablePropProof; }
	UDarkwellRememberablePropComponent* GetRememberablePropComponent() const
	{
		return RememberablePropComponent;
	}

private:
	bool EnableDarkwellProjectFog(
		UTexture* LiveCoverageTexture,
		FVector2D WorldMin,
		FVector2D InvWorldExtent,
		bool bShowOcclusionFixture,
		bool bEnableSurfaceCoverage);
	void ConfigureProjectFogSurfaceMaterial(
		UMaterialInstanceDynamic& Material,
		int32 OccluderIndex,
		bool bEnableSurfaceCoverage) const;
	TArray<UStaticMeshComponent*> GetProjectFogOccluderComponents() const;

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

	/** Movable environment-object proof; never registered as a SightWeave occluder. */
	UPROPERTY(VisibleAnywhere, Category = "Integration")
	TObjectPtr<UStaticMeshComponent> RememberablePropProof;

	UPROPERTY(VisibleAnywhere, Category = "Integration")
	TObjectPtr<UDarkwellRememberablePropComponent> RememberablePropComponent;

	UPROPERTY(VisibleAnywhere, Category = "Integration")
	TObjectPtr<UStaticMeshComponent> RotatedWall;

	UPROPERTY(VisibleAnywhere, Category = "Integration")
	TObjectPtr<UStaticMeshComponent> ConcaveWallVertical;

	UPROPERTY(VisibleAnywhere, Category = "Integration")
	TObjectPtr<UStaticMeshComponent> ConcaveWallHorizontal;

	UPROPERTY(VisibleAnywhere, Category = "Integration")
	TObjectPtr<UStaticMeshComponent> JunctionWallVertical;

	UPROPERTY(VisibleAnywhere, Category = "Integration")
	TObjectPtr<UStaticMeshComponent> JunctionWallHorizontal;

	/** Rendered illumination only; it is never a legal-light authority. */
	UPROPERTY(VisibleAnywhere, Category = "Integration")
	TObjectPtr<UDirectionalLightComponent> GreyboxKeyLight;

	/** Development proof camera used only when the project diagnostic mode selects it. */
	UPROPERTY(VisibleAnywhere, Category = "Integration")
	TObjectPtr<UCameraComponent> ProofCamera;

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

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ProjectFogGroundMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ProjectFogOccluderMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ProjectFogOriginalGroundMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> ProjectFogOriginalOccluderMaterials;

	bool bProjectFogOverrideCaptured = false;
	TArray<bool> ProjectFogOriginalOccluderHidden;
	TArray<ECollisionEnabled::Type> ProjectFogOriginalOccluderCollision;
};
