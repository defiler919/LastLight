#pragma once

#include "CoreMinimal.h"
#include "SightWeaveGeometry.h"

#include "SightWeaveSpatialIndex.generated.h"

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveSpatialIndexStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Spatial Index")
	int32 FloorCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Spatial Index")
	int32 CellCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Spatial Index")
	int32 SegmentCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Spatial Index")
	int32 StaticSegmentCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Spatial Index")
	int32 DynamicSegmentCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Spatial Index")
	int32 LastCandidateCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Spatial Index")
	int64 StaticBuildCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Spatial Index")
	int64 DynamicInsertCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Spatial Index")
	int64 DynamicRemoveCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Spatial Index")
	int64 DynamicUpdateCount = 0;
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveSpatialCellDebug
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Spatial Index")
	FSightWeaveFloorId FloorId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Spatial Index")
	FIntPoint Cell = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Spatial Index")
	FVector2D BoundsMin = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Spatial Index")
	FVector2D BoundsMax = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Spatial Index")
	int32 SegmentCount = 0;
};

/**
 * Deterministic floor-local uniform grid containing only plain geometry data.
 * The class has no UObject references and is safe to copy into later solve jobs.
 */
class SIGHTWEAVERUNTIME_API FSightWeaveFloorSpatialIndex
{
public:
	explicit FSightWeaveFloorSpatialIndex(double InCellSize = 500.0);

	void Reset();
	void SetCellSize(double InCellSize);
	double GetCellSize() const { return CellSize; }

	bool BuildStatic(TConstArrayView<FSightWeaveSegment2D> Segments);

	bool InsertOccluder(
		FSightWeaveOccluderHandle Handle,
		TConstArrayView<FSightWeaveSegment2D> Segments,
		bool bDynamic,
		FBox2D* OutBounds = nullptr);

	bool RemoveOccluder(
		FSightWeaveOccluderHandle Handle,
		FBox2D* OutOldBounds = nullptr);

	bool UpdateOccluder(
		FSightWeaveOccluderHandle Handle,
		TConstArrayView<FSightWeaveSegment2D> Segments,
		bool bDynamic,
		FBox2D& OutOldBounds,
		FBox2D& OutNewBounds);

	void Query(
		FSightWeaveFloorId FloorId,
		const FBox2D& Bounds,
		const FSightWeaveHeightRange& HeightRange,
		double HeightEpsilon,
		TArray<FSightWeaveSegment2D>& OutSegments) const;

	bool ContainsSegment(int64 StableId) const;
	bool ContainsOccluder(FSightWeaveOccluderHandle Handle) const;
	FSightWeaveSpatialIndexStats GetStats() const;
	void GetDebugCells(TArray<FSightWeaveSpatialCellDebug>& OutCells) const;

private:
	struct FSegmentEntry
	{
		FSightWeaveSegment2D Segment;
		TArray<FIntPoint> Cells;
	};

	struct FFloorData
	{
		TMap<FIntPoint, TArray<int64>> CellSegmentIds;
		TMap<int64, FSegmentEntry> Segments;
	};

	FIntPoint ToCell(const FVector2D& Point) const;
	void GetCellsForBounds(const FBox2D& Bounds, TArray<FIntPoint>& OutCells) const;
	bool InsertSegment(const FSightWeaveSegment2D& Segment);
	bool RemoveSegment(int64 StableId);
	FBox2D CalculateOccluderBounds(TConstArrayView<int64> StableIds) const;

	double CellSize = 500.0;
	TMap<FSightWeaveFloorId, FFloorData> Floors;
	TMap<int64, FSightWeaveFloorId> SegmentFloors;
	TMap<FSightWeaveOccluderHandle, TArray<int64>> OccluderSegments;
	TSet<int64> StaticSegmentIds;
	TSet<int64> DynamicSegmentIds;
	TArray<TArray<int64>> ReusableCellIdArrays;
	TArray<TArray<int32>> ReusableSourceEdgeIndexArrays;
	TArray<FIntPoint> UpdateCellsScratch;

	mutable int32 LastCandidateCount = 0;
	int64 StaticBuildCount = 0;
	int64 DynamicInsertCount = 0;
	int64 DynamicRemoveCount = 0;
	int64 DynamicUpdateCount = 0;
};
