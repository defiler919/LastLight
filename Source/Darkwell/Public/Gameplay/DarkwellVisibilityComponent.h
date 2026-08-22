// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/DarkwellVisibilityMath.h"
#include "DarkwellVisibilityComponent.generated.h"

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
	EDarkwellFogCellState GetWorldLocationState(const FVector& WorldLocation) const;
	bool IsWorldLocationCurrentlyVisible(const FVector& WorldLocation) const;
	bool IsWorldLocationExplored(const FVector& WorldLocation) const;
	float GetVisionSourceMargin(const FVector& WorldLocation) const;
	float GetCurrentMaximumVisionRange() const;
	void BuildVisualOcclusionRanges(int32 SampleCount, TArray<float>& OutRanges) const;
	float GetCellSize() const { return CellSize; }
	int32 GetExploredCellCount() const { return ExploredCells.Num(); }

	TArray<FIntPoint> CaptureExploredCells() const;
	void RestoreExploredCells(const TArray<FIntPoint>& SavedCells);
	void RefreshVisibility();

private:
	bool IsCellInsideAnyVisionSource(const FVector& CellCenter) const;
	static float GetConeMargin(
		const FVector& Origin,
		const FVector& Facing,
		const FVector& Target,
		float Range,
		float HalfAngleDegrees);
	bool HasLineOfSightToCell(const FVector& CellCenter, const FCollisionQueryParams& QueryParams) const;
	void UpdateFogSubjects();

	UPROPERTY(EditDefaultsOnly, Category = "Fog", meta = (ClampMin = "50.0"))
	float CellSize = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Fog", meta = (ClampMin = "0.02"))
	float RefreshIntervalSeconds = 0.10f;

	UPROPERTY(EditDefaultsOnly, Category = "Vision", meta = (ClampMin = "0.0"))
	float AwarenessRadius = 220.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vision", meta = (ClampMin = "0.0"))
	float UnlitVisionRange = 760.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vision", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float UnlitVisionHalfAngleDegrees = 52.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vision|Torch", meta = (ClampMin = "0.0"))
	float TorchVisionRange = 1250.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vision|Torch", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float TorchVisionHalfAngleDegrees = 50.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vision|Torch", meta = (ClampMin = "0.0"))
	float TorchDeterrentVisionRange = 1750.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vision|Torch", meta = (ClampMin = "0.0"))
	float ReloadLightRadius = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vision|Lantern", meta = (ClampMin = "0.0"))
	float LanternBaseVisionRadius = 680.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vision|Lantern", meta = (ClampMin = "0.0"))
	float LanternFocusVisionRange = 1900.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vision|Lantern", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float LanternFocusHalfAngleDegrees = 18.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vision|Lantern", meta = (ClampMin = "0.0"))
	float LanternFlashVisionRange = 1550.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vision|Lantern", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float LanternFlashHalfAngleDegrees = 54.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Vision", meta = (ClampMin = "0.0"))
	float VisibilityTraceHeight = 52.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Fog", meta = (ClampMin = "128", ClampMax = "1048576"))
	int32 MaximumRememberedCells = 262144;

	TSet<FIntPoint> VisibleCells;
	TSet<FIntPoint> ExploredCells;
	float RefreshTimeRemaining = 0.0f;
};
