#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SightWeaveGeometry.h"
#include "SightWeaveSparseAtlas.h"
#include "SightWeaveTypes.h"

#include "SightWeaveSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "SightWeave"))
class SIGHTWEAVERUNTIME_API USightWeaveSettings final : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USightWeaveSettings();

	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M1 Defaults")
	FSightWeaveFloorId DefaultFloorId;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M1 Defaults")
	FSightWeaveHeightRange DefaultHeightRange;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "M1 Defaults", meta = (ClampMin = "0.0001"))
	float BoundaryEpsilonCentimeters = 0.1f;

	/** M2 CPU-authority tolerances. Invalid config values are normalized before use. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Geometry")
	FSightWeaveGeometryTolerances GeometryTolerances;

	/** Reference and Verify are diagnostic-only; Shipping always uses Optimized. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Geometry")
	ESightWeaveSolverMode SolverMode = ESightWeaveSolverMode::Optimized;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Spatial Index", meta = (ClampMin = "1.0"))
	double SpatialCellSizeCentimeters = 500.0;

	/** Maximum exact observer-origin preparations retained by one world. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Prepared Event Index", meta = (ClampMin = "1", ClampMax = "1024"))
	int32 MaximumPreparedOriginEntries = 32;

	/** Hard retained-capacity budget for prepared observer-origin entries. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Prepared Event Index", meta = (ClampMin = "1048576", ClampMax = "1073741824"))
	int64 MaximumPreparedOriginBytes = 64ll * 1024ll * 1024ll;

	/** Selects an exact kinetic-order attempt only; it never changes correctness semantics. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Prepared Event Index", meta = (ClampMin = "0.0", ClampMax = "10000.0"))
	double SmallTranslationAttemptCentimeters = 25.0;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Prepared Event Index", meta = (ClampMin = "0", ClampMax = "1048576"))
	int32 MaximumKineticEndpointSwaps = 16384;

	/** Presentation-only inward feather width. HardLive remains the sole eligibility gate. */
	UPROPERTY(Config, EditAnywhere, Category = "Visual Presentation", meta = (ClampMin = "0.0", ClampMax = "100.0", Units = "cm"))
	float VisualFeatherWidthCentimeters = 50.0f;

	/** M3.5 CPU/GPU exploration-memory precision; selected by the frozen four-tier experiment. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Exploration Memory")
	ESightWeaveRenderPrecisionTier ExplorationMemoryPrecisionTier =
		ESightWeaveRenderPrecisionTier::Coarse;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bEnableRuntimeDebug = false;
};
