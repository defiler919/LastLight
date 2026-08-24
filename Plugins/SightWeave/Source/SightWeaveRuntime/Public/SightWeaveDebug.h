#pragma once

#include "CoreMinimal.h"
#include "SightWeaveQueries.h"
#include "SightWeaveSpatialIndex.h"

#include "SightWeaveDebug.generated.h"

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveDebugQueryMarker
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Debug")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Debug")
	FSightWeaveVisibilityQueryResult Result;
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveDebugData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Debug")
	FSightWeaveFrameSnapshot Snapshot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Debug")
	FSightWeaveGeometryTolerances GeometryTolerances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Debug")
	FSightWeaveSpatialIndexStats SpatialIndexStats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Debug")
	TArray<FSightWeaveSpatialCellDebug> SpatialCells;
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveDebugDrawOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Debug")
	bool bDrawFloors = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Debug")
	bool bDrawOccluders = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Debug")
	bool bDrawSources = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Debug")
	bool bDrawCandidateRays = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Debug")
	bool bDrawVisionPolygons = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Debug")
	bool bDrawIlluminationPolygons = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Debug")
	bool bDrawHardSuppressions = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Debug")
	bool bDrawSpatialCells = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Debug", meta = (ClampMin = "0.0"))
	float DurationSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Debug", meta = (ClampMin = "0.1"))
	float Thickness = 2.0f;
};
