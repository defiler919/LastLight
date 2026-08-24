#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SightWeaveGeometry.h"
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

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Spatial Index", meta = (ClampMin = "1.0"))
	double SpatialCellSizeCentimeters = 500.0;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bEnableRuntimeDebug = false;
};
