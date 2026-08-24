#pragma once

#include "CoreMinimal.h"
#include "SightWeaveTypes.h"

#include "SightWeaveGeometry.generated.h"

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveGeometryTolerances
{
	GENERATED_BODY()

	/** World-space endpoint welding distance in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Geometry", meta = (ClampMin = "0.0"))
	double AuthoringWeldEpsilon = 0.1;

	/** World-space length at or below which an authored edge is rejected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Geometry", meta = (ClampMin = "0.0"))
	double ZeroLengthEpsilon = 0.001;

	/** Dimensionless cross-product tolerance used for parallel ray/segment tests. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Geometry", meta = (ClampMin = "0.0"))
	double RayParallelEpsilon = 1.0e-9;

	/** Angular event offset in degrees around each occluder endpoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Geometry", meta = (ClampMin = "0.0"))
	double EndpointAngularEpsilonDegrees = 0.0025;

	/** World-space tolerance for treating a query point as lying on an edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Geometry", meta = (ClampMin = "0.0"))
	double PointOnEdgeEpsilon = 0.05;

	/** World-space tolerance used by stable polygon containment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Geometry", meta = (ClampMin = "0.0"))
	double PointInPolygonEpsilon = 0.05;

	/** World-space distance used to collapse adjacent solve vertices. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Geometry", meta = (ClampMin = "0.0"))
	double DuplicateVertexEpsilon = 0.01;

	/** World-space overlap tolerance for source and occluder height bands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Geometry", meta = (ClampMin = "0.0"))
	double HeightOverlapEpsilon = 0.01;

	/** Deterministic curved-boundary tessellation. Must be at least eight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Geometry", meta = (ClampMin = "8", ClampMax = "2048"))
	int32 RadialBoundarySteps = 128;

	bool IsValid() const;
	void Normalize();
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveSegment2D
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Geometry")
	FVector2D A = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Geometry")
	FVector2D B = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Geometry")
	FSightWeaveFloorId FloorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Geometry")
	FSightWeaveHeightRange HeightRange;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	FSightWeaveOccluderHandle OccluderHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Geometry")
	bool bDynamic = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	int64 StableId = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	TArray<int32> SourceEdgeIndices;

	bool IsFinite() const;
	FBox2D GetBounds() const;
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeavePolygon
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	FSightWeaveVisionSourceHandle SourceHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	FSightWeaveKnowledgeOwnerId KnowledgeOwnerId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	FSightWeaveFloorId FloorId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	TArray<FVector> Vertices;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	FVector2D BoundsMin = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	FVector2D BoundsMax = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	FSightWeaveRevision Revision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	FSightWeaveRevision SourceRevision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	FSightWeaveRevision OccluderRevision;

	bool IsValid() const { return FloorId.IsValid() && Vertices.Num() >= 3; }
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveIlluminationPolygon
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	FSightWeaveIlluminationSourceHandle SourceHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	FSightWeaveKnowledgeOwnerId KnowledgeOwnerId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	FSightWeaveFloorId FloorId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	TArray<FVector> Vertices;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	FVector2D BoundsMin = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	FVector2D BoundsMax = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	FSightWeaveRevision Revision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	FSightWeaveRevision SourceRevision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Geometry")
	FSightWeaveRevision OccluderRevision;

	bool IsValid() const { return FloorId.IsValid() && Vertices.Num() >= 3; }
};

struct SIGHTWEAVERUNTIME_API FSightWeaveRaySegmentHit
{
	bool bHit = false;
	double RayDistance = 0.0;
	double SegmentFraction = 0.0;
	FVector2D Point = FVector2D::ZeroVector;
	int64 StableSegmentId = 0;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveNormalizationResult
{
	TArray<FSightWeaveSegment2D> Segments;
	int32 RemovedInvalid = 0;
	int32 RemovedZeroLength = 0;
	int32 RemovedDuplicates = 0;
	int32 WeldedEndpoints = 0;
	int32 CollinearMerges = 0;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveReferenceSolveInput
{
	FVector Origin = FVector::ZeroVector;
	FVector2D Forward = FVector2D(1.0, 0.0);
	ESightWeaveSourceShape Shape = ESightWeaveSourceShape::Radial;
	double Range = 1000.0;
	double HalfAngleDegrees = 180.0;
	double NearAwarenessRadius = 0.0;
	FSightWeaveFloorId FloorId;
	FSightWeaveHeightRange HeightRange;
	FSightWeaveGeometryTolerances Tolerances;
	TArray<FSightWeaveSegment2D> Segments;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveReferenceSolveResult
{
	bool bSucceeded = false;
	TArray<FVector> Vertices;
	TArray<double> CandidateAnglesRadians;
	int32 CandidateSegmentCount = 0;
	int32 CastRayCount = 0;
	FString Error;
};

namespace SightWeave::Geometry
{
	SIGHTWEAVERUNTIME_API bool HeightRangesOverlap(
		const FSightWeaveHeightRange& A,
		const FSightWeaveHeightRange& B,
		double Epsilon);

	SIGHTWEAVERUNTIME_API FSightWeaveRaySegmentHit IntersectRaySegment(
		const FVector2D& RayOrigin,
		const FVector2D& RayDirection,
		const FSightWeaveSegment2D& Segment,
		const FSightWeaveGeometryTolerances& Tolerances);

	SIGHTWEAVERUNTIME_API FSightWeaveNormalizationResult NormalizeSegments(
		TConstArrayView<FSightWeaveSegment2D> Input,
		const FSightWeaveGeometryTolerances& Tolerances,
		bool bMergeCollinear = true);

	/** Inclusive boundary policy: a point within PointOnEdgeEpsilon is inside. */
	SIGHTWEAVERUNTIME_API bool IsPointInPolygon(
		const FVector2D& Point,
		TConstArrayView<FVector> Vertices,
		const FSightWeaveGeometryTolerances& Tolerances);

	SIGHTWEAVERUNTIME_API bool IsSimplePolygon(
		TConstArrayView<FVector> Vertices,
		const FSightWeaveGeometryTolerances& Tolerances);

	SIGHTWEAVERUNTIME_API FSightWeaveReferenceSolveResult SolveReferencePolygon(
		const FSightWeaveReferenceSolveInput& Input);
}
