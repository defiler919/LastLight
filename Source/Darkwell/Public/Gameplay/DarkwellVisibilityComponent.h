// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/DarkwellVisibilityMath.h"
#include "DarkwellVisibilityComponent.generated.h"

class AActor;
class ULocalLightComponent;

struct DARKWELL_API FDarkwellLightPresentationSource
{
	FVector WorldOrigin = FVector::ZeroVector;
	FVector2D Facing = FVector2D(1.0f, 0.0f);
	float Range = 0.0f;
	float SinHalfAngle = 0.0f;
	float CosHalfAngle = 1.0f;
	bool bCone = false;
	const AActor* SourceOwner = nullptr;

	float EvaluateMargin(const FVector2D& WorldPoint) const;
};

/**
 * Immutable, allocation-free snapshot used by the high-resolution visual mask.
 * Sight and illumination are deliberately independent: final visibility is their intersection.
 */
struct DARKWELL_API FDarkwellVisionPresentationState
{
	static constexpr int32 MaximumLightCount = 32;

	FVector2D SightOrigin = FVector2D::ZeroVector;
	FVector2D SightFacing = FVector2D(1.0f, 0.0f);
	float SightRange = 0.0f;
	float SightSinHalfAngle = 0.0f;
	float SightCosHalfAngle = 1.0f;
	float AwarenessRadius = 0.0f;
	FDarkwellLightPresentationSource Lights[MaximumLightCount];
	int32 LightCount = 0;

	void SetSightCone(
		const FVector2D& Origin,
		const FVector2D& Facing,
		float Range,
		float HalfAngleDegrees);
	void SetAwarenessRadius(float Radius);
	void AddLightCircle(const FVector& Origin, float Range, const AActor* SourceOwner = nullptr);
	void AddLightCone(
		const FVector& Origin,
		const FVector2D& Facing,
		float Range,
		float HalfAngleDegrees,
		const AActor* SourceOwner = nullptr);
	float EvaluateSightMargin(const FVector2D& WorldPoint) const;
	float EvaluateSightMarginFromDelta(const FVector2D& Delta, float Distance) const;
	float EvaluateAwarenessMarginFromDistance(float Distance) const;
	float EvaluateIlluminationMargin(const FVector2D& WorldPoint) const;
	float EvaluateMargin(const FVector2D& WorldPoint) const;
	float EvaluateMarginFromDelta(const FVector2D& Delta, float Distance) const;
};

/**
 * Owns the player's authoritative visibility and explored-world knowledge.
 * Rendering consumes this state, but presentation never decides what the player knows.
 */
UCLASS(ClassGroup = (Darkwell), meta = (BlueprintSpawnableComponent))
class DARKWELL_API UDarkwellVisibilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDarkwellVisibilityComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	EDarkwellFogCellState GetCellState(const FIntPoint& Cell) const;
	bool IsCellExplored(const FIntPoint& Cell) const { return ExploredCells.Contains(Cell); }
	EDarkwellFogCellState GetWorldLocationState(const FVector& WorldLocation) const;
	bool IsWorldLocationCurrentlyVisible(const FVector& WorldLocation) const;
	bool IsWorldLocationExplored(const FVector& WorldLocation) const;
	float GetVisionSourceMargin(const FVector& WorldLocation) const;
	float GetCurrentMaximumVisionRange() const;
	void BuildVisualPresentationState(FDarkwellVisionPresentationState& OutState) const;
	void BuildVisualOcclusionRanges(int32 SampleCount, TArray<float>& OutRanges) const;
	float GetCellSize() const { return CellSize; }
	float GetPresentationCellSize() const { return PresentationCellSize; }
	int32 GetExploredCellCount() const { return ExploredCells.Num(); }
	uint64 GetPresentationMemoryRevision() const { return PresentationMemoryRevision; }
	bool IsPresentationCellExplored(const FIntPoint& Cell) const
	{
		return ExploredPresentationCells.Contains(Cell);
	}

	TArray<FIntPoint> CaptureExploredCells() const;
	TArray<FIntPoint> CaptureExploredPresentationCells() const;
	void RestoreExploredCells(
		const TArray<FIntPoint>& SavedCells,
		const TArray<FIntPoint>& SavedPresentationCells = TArray<FIntPoint>(),
		float SavedPresentationCellSize = 0.0f);
	bool RecordExploredPresentationCell(const FIntPoint& Cell);
	void RefreshVisibility();
	void SetVisibilityAuthorityEnabled(bool bEnabled);
	bool IsVisibilityAuthorityEnabled() const { return bAuthorityEnabled; }

private:
	void AddActiveLocalLights(FDarkwellVisionPresentationState& OutState) const;
	bool IsCellIlluminated(
		const FVector& CellCenter,
		const FDarkwellVisionPresentationState& State,
		const FCollisionQueryParams& QueryParams) const;
	bool HasLightLineOfSightToCell(
		const FDarkwellLightPresentationSource& Light,
		const FVector& CellCenter,
		const FCollisionQueryParams& QueryParams) const;
	bool HasLineOfSightToCell(const FVector& CellCenter, const FCollisionQueryParams& QueryParams) const;
	void UpdateFogSubjects();

	UPROPERTY(EditDefaultsOnly, Category = "Fog", meta = (ClampMin = "50.0"))
	float CellSize = 100.0f;

	/** Fine, presentation-only memory grid. Gameplay visibility remains on CellSize. */
	UPROPERTY(EditDefaultsOnly, Category = "Fog", meta = (ClampMin = "10.0", ClampMax = "50.0"))
	float PresentationCellSize = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Fog", meta = (ClampMin = "0.02"))
	float RefreshIntervalSeconds = 0.10f;

	UPROPERTY(EditDefaultsOnly, Category = "Vision", meta = (ClampMin = "0.0"))
	float SightRange = 2200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vision", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float SightHalfAngleDegrees = 52.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vision", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float AimedSightHalfAngleDegrees = 35.0f;

	/** A tiny wall-occluded body-awareness disk; it does not depend on light or facing. */
	UPROPERTY(EditDefaultsOnly, Category = "Vision", meta = (ClampMin = "0.0"))
	float AwarenessRadius = 120.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vision", meta = (ClampMin = "0.0"))
	float VisibilityTraceHeight = 52.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Fog", meta = (ClampMin = "128", ClampMax = "1048576"))
	int32 MaximumRememberedCells = 262144;

	UPROPERTY(EditDefaultsOnly, Category = "Fog", meta = (ClampMin = "512", ClampMax = "4194304"))
	int32 MaximumRememberedPresentationCells = 1048576;

	TSet<FIntPoint> VisibleCells;
	TSet<FIntPoint> ExploredCells;
	TSet<FIntPoint> ExploredPresentationCells;
	uint64 PresentationMemoryRevision = 0;
	float RefreshTimeRemaining = 0.0f;
	bool bAuthorityEnabled = true;
};
