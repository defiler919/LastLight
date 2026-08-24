#pragma once

#include "SightWeaveGeometry.h"

struct FSightWeaveOptimizedPreparedSegment
{
	FVector2D OffsetA = FVector2D::ZeroVector;
	FVector2D Vector = FVector2D::ZeroVector;
	double RayDistanceNumerator = 0.0;
	double AAngle = 0.0;
	double BAngle = 0.0;
	double AngularPadding = 0.0;
	double OriginDistanceSquared = 0.0;
	double FractionEpsilon = 0.0;
	int64 StableId = 0;
	bool bOriginOnSegment = false;
};

struct FSightWeaveOptimizedPreparedSegmentSlot
{
	FVector2D A = FVector2D::ZeroVector;
	FVector2D B = FVector2D::ZeroVector;
	FSightWeaveFloorId FloorId;
	FSightWeaveHeightRange HeightRange;
	int64 StableId = 0;
	bool bHasKey = false;
	bool bCandidate = false;
	FSightWeaveOptimizedPreparedSegment Prepared;

	bool Matches(const FSightWeaveSegment2D& Segment) const
	{
		return bHasKey
			&& A.X == Segment.A.X
			&& A.Y == Segment.A.Y
			&& B.X == Segment.B.X
			&& B.Y == Segment.B.Y
			&& FloorId == Segment.FloorId
			&& HeightRange.ZMin == Segment.HeightRange.ZMin
			&& HeightRange.ZMax == Segment.HeightRange.ZMax
			&& StableId == Segment.StableId;
	}

	void StoreKey(const FSightWeaveSegment2D& Segment)
	{
		A = Segment.A;
		B = Segment.B;
		FloorId = Segment.FloorId;
		HeightRange = Segment.HeightRange;
		StableId = Segment.StableId;
		bHasKey = true;
	}
};

/**
 * Mutable cache owned by one world/source on the game thread. It retains only
 * plain geometry values and never owns UObject or world references.
 */
struct FSightWeaveOptimizedSolveCache
{
	bool bInputInvariantInitialized = false;
	FVector2D Origin = FVector2D::ZeroVector;
	FVector2D Forward = FVector2D(1.0, 0.0);
	FSightWeaveFloorId FloorId;
	FSightWeaveHeightRange HeightRange;
	FSightWeaveGeometryTolerances Tolerances;
	TArray<FSightWeaveOptimizedPreparedSegmentSlot> SegmentSlots;
	TArray<FSightWeaveOptimizedPreparedSegment> CandidateSegments;
};
