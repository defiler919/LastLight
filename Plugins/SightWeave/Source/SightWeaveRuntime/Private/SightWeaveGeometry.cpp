#include "SightWeaveGeometry.h"

#include "Algo/Unique.h"
#include "HAL/PlatformTime.h"
#include "HAL/ThreadSingleton.h"
#include "Templates/Sorting.h"

DEFINE_LOG_CATEGORY_STATIC(LogSightWeaveGeometry, Log, All);

namespace
{
	constexpr double SightWeaveTwoPi = 2.0 * PI;

	double Cross2D(const FVector2D& A, const FVector2D& B)
	{
		return A.X * B.Y - A.Y * B.X;
	}

	double NormalizeRadians(double Angle)
	{
		while (Angle < -PI)
		{
			Angle += SightWeaveTwoPi;
		}
		while (Angle >= PI)
		{
			Angle -= SightWeaveTwoPi;
		}
		return Angle;
	}

	bool IsFiniteVector(const FVector2D& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y);
	}

	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	double DistanceSquaredToSegment(const FVector2D& Point, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D Edge = B - A;
		const double EdgeLengthSquared = Edge.SizeSquared();
		if (EdgeLengthSquared <= UE_DOUBLE_SMALL_NUMBER)
		{
			return FVector2D::DistSquared(Point, A);
		}
		const double Fraction = FMath::Clamp(FVector2D::DotProduct(Point - A, Edge) / EdgeLengthSquared, 0.0, 1.0);
		return FVector2D::DistSquared(Point, A + Edge * Fraction);
	}

	bool SameMetadata(const FSightWeaveSegment2D& A, const FSightWeaveSegment2D& B, double Epsilon)
	{
		return A.FloorId == B.FloorId
			&& A.OccluderHandle == B.OccluderHandle
			&& A.bDynamic == B.bDynamic
			&& FMath::IsNearlyEqual(A.HeightRange.ZMin, B.HeightRange.ZMin, Epsilon)
			&& FMath::IsNearlyEqual(A.HeightRange.ZMax, B.HeightRange.ZMax, Epsilon);
	}

	bool LexicalPointLess(const FVector2D& A, const FVector2D& B)
	{
		return A.X < B.X || (A.X == B.X && A.Y < B.Y);
	}

	void CanonicalEndpoints(const FSightWeaveSegment2D& Segment, FVector2D& OutFirst, FVector2D& OutSecond)
	{
		if (LexicalPointLess(Segment.B, Segment.A))
		{
			OutFirst = Segment.B;
			OutSecond = Segment.A;
		}
		else
		{
			OutFirst = Segment.A;
			OutSecond = Segment.B;
		}
	}

	bool PointsEqual(const FVector2D& A, const FVector2D& B, double Epsilon)
	{
		return FVector2D::DistSquared(A, B) <= Epsilon * Epsilon;
	}

	bool TryMergeCollinear(
		const FSightWeaveSegment2D& A,
		const FSightWeaveSegment2D& B,
		double Epsilon,
		FSightWeaveSegment2D& OutMerged)
	{
		if (!SameMetadata(A, B, Epsilon))
		{
			return false;
		}

		FVector2D Shared;
		FVector2D OuterA;
		FVector2D OuterB;
		if (PointsEqual(A.A, B.A, Epsilon))
		{
			Shared = A.A;
			OuterA = A.B;
			OuterB = B.B;
		}
		else if (PointsEqual(A.A, B.B, Epsilon))
		{
			Shared = A.A;
			OuterA = A.B;
			OuterB = B.A;
		}
		else if (PointsEqual(A.B, B.A, Epsilon))
		{
			Shared = A.B;
			OuterA = A.A;
			OuterB = B.B;
		}
		else if (PointsEqual(A.B, B.B, Epsilon))
		{
			Shared = A.B;
			OuterA = A.A;
			OuterB = B.A;
		}
		else
		{
			return false;
		}

		const FVector2D DirectionA = (OuterA - Shared).GetSafeNormal();
		const FVector2D DirectionB = (OuterB - Shared).GetSafeNormal();
		if (DirectionA.IsNearlyZero() || DirectionB.IsNearlyZero())
		{
			return false;
		}

		const double AngularTolerance = FMath::Max(1.0e-9, Epsilon * 1.0e-3);
		if (FMath::Abs(Cross2D(DirectionA, DirectionB)) > AngularTolerance
			|| FVector2D::DotProduct(DirectionA, DirectionB) > -1.0 + AngularTolerance)
		{
			return false;
		}

		OutMerged = A;
		OutMerged.A = OuterA;
		OutMerged.B = OuterB;
		OutMerged.StableId = A.StableId > 0 && B.StableId > 0
			? FMath::Min(A.StableId, B.StableId)
			: FMath::Max(A.StableId, B.StableId);
		OutMerged.SourceEdgeIndices.Append(B.SourceEdgeIndices);
		OutMerged.SourceEdgeIndices.Sort();
		OutMerged.SourceEdgeIndices.SetNum(Algo::Unique(OutMerged.SourceEdgeIndices));
		return true;
	}

	int32 Orientation(const FVector2D& A, const FVector2D& B, const FVector2D& C, double Epsilon)
	{
		const double Value = Cross2D(B - A, C - A);
		if (FMath::Abs(Value) <= Epsilon)
		{
			return 0;
		}
		return Value > 0.0 ? 1 : -1;
	}

	bool PointOnSegment(const FVector2D& Point, const FVector2D& A, const FVector2D& B, double Epsilon)
	{
		return DistanceSquaredToSegment(Point, A, B) <= Epsilon * Epsilon;
	}

	bool SegmentsIntersectInclusiveUnchecked(
		const FVector2D& A0,
		const FVector2D& A1,
		const FVector2D& B0,
		const FVector2D& B1,
		double Epsilon)
	{
		const int32 O1 = Orientation(A0, A1, B0, Epsilon);
		const int32 O2 = Orientation(A0, A1, B1, Epsilon);
		const int32 O3 = Orientation(B0, B1, A0, Epsilon);
		const int32 O4 = Orientation(B0, B1, A1, Epsilon);
		if (O1 != O2 && O3 != O4)
		{
			return true;
		}
		return (O1 == 0 && PointOnSegment(B0, A0, A1, Epsilon))
			|| (O2 == 0 && PointOnSegment(B1, A0, A1, Epsilon))
			|| (O3 == 0 && PointOnSegment(A0, B0, B1, Epsilon))
			|| (O4 == 0 && PointOnSegment(A1, B0, B1, Epsilon));
	}

	bool SegmentsIntersectInclusive(
		const FVector2D& A0,
		const FVector2D& A1,
		const FVector2D& B0,
		const FVector2D& B1,
		double Epsilon)
	{
		if (FMath::Max(A0.X, A1.X) + Epsilon < FMath::Min(B0.X, B1.X)
			|| FMath::Max(B0.X, B1.X) + Epsilon < FMath::Min(A0.X, A1.X)
			|| FMath::Max(A0.Y, A1.Y) + Epsilon < FMath::Min(B0.Y, B1.Y)
			|| FMath::Max(B0.Y, B1.Y) + Epsilon < FMath::Min(A0.Y, A1.Y))
		{
			return false;
		}
		return SegmentsIntersectInclusiveUnchecked(A0, A1, B0, B1, Epsilon);
	}

	double SourceRadiusAtRelativeAngle(const FSightWeaveReferenceSolveInput& Input, double RelativeAngle)
	{
		if (Input.Shape == ESightWeaveSourceShape::Radial)
		{
			return Input.Range;
		}

		const double HalfAngleRadians = FMath::DegreesToRadians(Input.HalfAngleDegrees);
		if (FMath::Abs(RelativeAngle) <= HalfAngleRadians + 1.0e-12)
		{
			return Input.Range;
		}
		return Input.NearAwarenessRadius;
	}

	void AddUniqueAngle(TArray<double>& Angles, double Angle)
	{
		Angles.Add(NormalizeRadians(Angle));
	}

	void SortAndDeduplicateAngles(TArray<double>& Angles)
	{
		Angles.Sort();
		for (int32 Index = Angles.Num() - 1; Index > 0; --Index)
		{
			if (FMath::Abs(Angles[Index] - Angles[Index - 1]) <= 1.0e-12)
			{
				Angles.RemoveAt(Index);
			}
		}
	}

	void CalculateBounds(TConstArrayView<FVector> Vertices, FVector2D& OutMin, FVector2D& OutMax)
	{
		if (Vertices.IsEmpty())
		{
			OutMin = FVector2D::ZeroVector;
			OutMax = FVector2D::ZeroVector;
			return;
		}
		OutMin = FVector2D(Vertices[0].X, Vertices[0].Y);
		OutMax = OutMin;
		for (const FVector& Vertex : Vertices)
		{
			OutMin.X = FMath::Min(OutMin.X, Vertex.X);
			OutMin.Y = FMath::Min(OutMin.Y, Vertex.Y);
			OutMax.X = FMath::Max(OutMax.X, Vertex.X);
			OutMax.Y = FMath::Max(OutMax.Y, Vertex.Y);
		}
	}

	struct FSightWeavePreparedSegment
	{
		FVector2D OffsetA = FVector2D::ZeroVector;
		FVector2D Vector = FVector2D::ZeroVector;
		double RayDistanceNumerator = 0.0;
		double AAngle = 0.0;
		double BAngle = 0.0;
		double MinimumEndpointDistance = 0.0;
		double OriginDistanceSquared = 0.0;
		double FractionEpsilon = 0.0;
		int64 StableId = 0;
		bool bOriginOnSegment = false;
	};

	struct FSightWeaveAngularInterval
	{
		double StartAngle = 0.0;
		double EndAngle = 0.0;
		int32 SegmentIndex = INDEX_NONE;
	};

	struct FSightWeaveActiveInterval
	{
		double EndAngle = 0.0;
		double OriginDistanceSquared = 0.0;
		int64 StableId = 0;
		int32 SegmentIndex = INDEX_NONE;
	};

	struct FSightWeaveSolverFrame
	{
		TArray<FSightWeavePreparedSegment> CandidateSegments;
		TArray<FSightWeaveAngularInterval> AngularIntervals;
		TArray<FSightWeaveAngularInterval> AngularIntervalSortBuffer;
		TArray<FSightWeaveActiveInterval> ActiveIntervals;
		TArray<double> AngleSortBuffer;
		TArray<double> EndpointAngles;
		TArray<double> EndpointAngleSortBuffer;
		TArray<FVector2D> EndpointDirections;
		TArray<FVector2D> CandidateDirections;
		TArray<double> BoundaryAngles;

		void Reset()
		{
			CandidateSegments.Reset();
			AngularIntervals.Reset();
			AngularIntervalSortBuffer.Reset();
			ActiveIntervals.Reset();
			AngleSortBuffer.Reset();
			EndpointAngles.Reset();
			EndpointAngleSortBuffer.Reset();
			EndpointDirections.Reset();
			CandidateDirections.Reset();
			BoundaryAngles.Reset();
		}

		void TrimAbnormalHighWater()
		{
			constexpr uint64 MaximumRetainedBytes = 8ull * 1024ull * 1024ull;
			const uint64 RetainedBytes = CandidateSegments.GetAllocatedSize()
				+ AngularIntervals.GetAllocatedSize()
				+ AngularIntervalSortBuffer.GetAllocatedSize()
				+ ActiveIntervals.GetAllocatedSize()
				+ AngleSortBuffer.GetAllocatedSize()
				+ EndpointAngles.GetAllocatedSize()
				+ EndpointAngleSortBuffer.GetAllocatedSize()
				+ EndpointDirections.GetAllocatedSize()
				+ CandidateDirections.GetAllocatedSize()
				+ BoundaryAngles.GetAllocatedSize();
			if (RetainedBytes > MaximumRetainedBytes)
			{
				CandidateSegments.Empty();
				AngularIntervals.Empty();
				AngularIntervalSortBuffer.Empty();
				ActiveIntervals.Empty();
				AngleSortBuffer.Empty();
				EndpointAngles.Empty();
				EndpointAngleSortBuffer.Empty();
				EndpointDirections.Empty();
				CandidateDirections.Empty();
				BoundaryAngles.Empty();
			}
		}
	};

	struct FSightWeaveThreadSolverScratch final : TThreadSingleton<FSightWeaveThreadSolverScratch>
	{
		friend TThreadSingleton<FSightWeaveThreadSolverScratch>;
		static constexpr int32 ReentrantFrameCount = 4;
		FSightWeaveSolverFrame Frames[ReentrantFrameCount];
		int32 ActiveDepth = 0;
	};

	class FSightWeaveSolverFrameLease
	{
	public:
		FSightWeaveSolverFrameLease()
			: Scratch(FSightWeaveThreadSolverScratch::Get())
		{
			const int32 FrameIndex = Scratch.ActiveDepth++;
			Frame = FrameIndex < FSightWeaveThreadSolverScratch::ReentrantFrameCount
				? &Scratch.Frames[FrameIndex]
				: &OverflowFrame;
			Frame->Reset();
		}

		~FSightWeaveSolverFrameLease()
		{
			Frame->TrimAbnormalHighWater();
			check(Scratch.ActiveDepth > 0);
			--Scratch.ActiveDepth;
		}

		FSightWeaveSolverFrame& Get() const { return *Frame; }

	private:
		FSightWeaveThreadSolverScratch& Scratch;
		FSightWeaveSolverFrame OverflowFrame;
		FSightWeaveSolverFrame* Frame = nullptr;
	};

#if WITH_DEV_AUTOMATION_TESTS
	bool ExerciseScratchReentrancy(
		const int32 RemainingDepth,
		TArray<const FSightWeaveSolverFrame*>& ActiveFrames)
	{
		FSightWeaveSolverFrameLease Lease;
		const FSightWeaveSolverFrame* Frame = &Lease.Get();
		if (ActiveFrames.Contains(Frame))
		{
			return false;
		}

		ActiveFrames.Add(Frame);
		const bool bNestedFramesWereDistinct = RemainingDepth <= 1
			|| ExerciseScratchReentrancy(RemainingDepth - 1, ActiveFrames);
		ActiveFrames.Pop(EAllowShrinking::No);
		return bNestedFramesWereDistinct;
	}
#endif

	void ResetOptimizedSolveResult(FSightWeaveReferenceSolveResult& Result)
	{
		Result.bSucceeded = false;
		Result.Vertices.Reset();
		Result.CandidateAnglesRadians.Reset();
		Result.CandidateDistances.Reset();
		Result.CandidateBoundaryPoints.Reset();
		Result.CandidateSegmentCount = 0;
		Result.CastRayCount = 0;
		Result.SolverModeUsed = ESightWeaveSolverMode::Optimized;
		Result.bVerificationMatched = false;
		Result.bUsedReferenceFallback = false;
		Result.StageMetrics = {};
		Result.VerificationError.Reset();
		Result.Error.Reset();
	}

	void AddClippedAngularInterval(
		TArray<FSightWeaveAngularInterval>& Intervals,
		const double UnwrappedStart,
		const double UnwrappedEnd,
		const int32 SegmentIndex)
	{
		for (int32 Wrap = -1; Wrap <= 1; ++Wrap)
		{
			const double Offset = static_cast<double>(Wrap) * SightWeaveTwoPi;
			const double Start = FMath::Max(-PI, UnwrappedStart + Offset);
			const double End = FMath::Min(PI, UnwrappedEnd + Offset);
			if (Start <= End)
			{
				Intervals.Add({ Start, End, SegmentIndex });
			}
		}
	}

	FORCEINLINE uint64 OrderedDoubleRadixKey(const double Value)
	{
		uint64 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		const uint64 Mask = (uint64(0) - (Bits >> 63)) | (uint64(1) << 63);
		return Bits ^ Mask;
	}

	struct FSightWeaveAngularIntervalRadixKey
	{
		FORCEINLINE uint64 operator()(const FSightWeaveAngularInterval& Interval) const
		{
			return OrderedDoubleRadixKey(Interval.StartAngle);
		}
	};

	void BuildAngularIntervals(
		const TArray<FSightWeavePreparedSegment>& Segments,
		const FVector2D& Origin,
		const double ForwardAngle,
		const FSightWeaveGeometryTolerances& Tolerances,
		TArray<FSightWeaveAngularInterval>& OutIntervals,
		TArray<FSightWeaveAngularInterval>& SortBuffer)
	{
		OutIntervals.Reset();
		OutIntervals.Reserve(Segments.Num() * 2);
		const double NumericAngularPadding = FMath::Max(1.0e-12, Tolerances.RayParallelEpsilon);
		for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
		{
			const FSightWeavePreparedSegment& Segment = Segments[SegmentIndex];
			if (Segment.bOriginOnSegment)
			{
				OutIntervals.Add({ -PI, PI, SegmentIndex });
				continue;
			}

			const double SignedSpan = NormalizeRadians(Segment.BAngle - Segment.AAngle);
			const double AngularPadding = FMath::Max(
				NumericAngularPadding,
				FMath::Atan2(
					Tolerances.PointOnEdgeEpsilon,
					FMath::Max(Segment.MinimumEndpointDistance, Tolerances.PointOnEdgeEpsilon)));
			const double Start = (SignedSpan >= 0.0 ? Segment.AAngle : Segment.BAngle) - AngularPadding;
			const double End = Start + FMath::Abs(SignedSpan) + 2.0 * AngularPadding;
			AddClippedAngularInterval(OutIntervals, Start, End, SegmentIndex);
		}

		auto IntervalLess = [&Segments](const FSightWeaveAngularInterval& A, const FSightWeaveAngularInterval& B)
		{
			if (A.StartAngle != B.StartAngle)
			{
				return A.StartAngle < B.StartAngle;
			}
			if (A.EndAngle != B.EndAngle)
			{
				return A.EndAngle < B.EndAngle;
			}
			const int64 AStableId = Segments[A.SegmentIndex].StableId;
			const int64 BStableId = Segments[B.SegmentIndex].StableId;
			return AStableId != BStableId ? AStableId < BStableId : A.SegmentIndex < B.SegmentIndex;
		};
		if (OutIntervals.Num() < 256)
		{
			OutIntervals.Sort(IntervalLess);
			return;
		}

		SortBuffer.SetNumUninitialized(OutIntervals.Num());
		RadixSort64<ERadixSortBufferState::IsInitialized>(
			OutIntervals.GetData(),
			SortBuffer.GetData(),
			OutIntervals.Num(),
			FSightWeaveAngularIntervalRadixKey());
		for (int32 RunStart = 0; RunStart < OutIntervals.Num();)
		{
			int32 RunEnd = RunStart + 1;
			while (RunEnd < OutIntervals.Num()
				&& OutIntervals[RunEnd].StartAngle == OutIntervals[RunStart].StartAngle)
			{
				++RunEnd;
			}
			for (int32 Index = RunStart + 1; Index < RunEnd; ++Index)
			{
				int32 InsertIndex = Index;
				while (InsertIndex > RunStart
					&& IntervalLess(OutIntervals[InsertIndex], OutIntervals[InsertIndex - 1]))
				{
					OutIntervals.Swap(InsertIndex, InsertIndex - 1);
					--InsertIndex;
				}
			}
			RunStart = RunEnd;
		}
	}

	bool IntersectTrustedRaySegment(
		const FVector2D& NormalizedRayDirection,
		const FSightWeavePreparedSegment& Segment,
		const FSightWeaveGeometryTolerances& Tolerances,
		double& OutRayDistance)
	{
		const double Denominator = Cross2D(NormalizedRayDirection, Segment.Vector);
		if (FMath::Abs(Denominator) <= Tolerances.RayParallelEpsilon)
		{
			return false;
		}
		const double RayDistance = Segment.RayDistanceNumerator / Denominator;
		const double SegmentFractionNumerator = Cross2D(Segment.OffsetA, NormalizedRayDirection);
		// A numerator between zero and the denominator is unambiguously on the
		// closed segment and does not need a division. Endpoint-adjacent rays fall
		// back to the exact established quotient so epsilon behavior is unchanged.
		const bool bFractionDefinitelyInside = Denominator > 0.0
			? SegmentFractionNumerator >= 0.0 && SegmentFractionNumerator <= Denominator
			: SegmentFractionNumerator <= 0.0 && SegmentFractionNumerator >= Denominator;
		if (RayDistance < -Tolerances.PointOnEdgeEpsilon)
		{
			return false;
		}
		if (!bFractionDefinitelyInside)
		{
			const double SegmentFraction = SegmentFractionNumerator / Denominator;
			if (SegmentFraction < -Segment.FractionEpsilon
				|| SegmentFraction > 1.0 + Segment.FractionEpsilon)
			{
				return false;
			}
		}
		OutRayDistance = FMath::Max(0.0, RayDistance);
		return true;
	}

	struct FSightWeaveDoubleRadixKey
	{
		FORCEINLINE uint64 operator()(const double Value) const
		{
			return OrderedDoubleRadixKey(Value);
		}
	};

	void SortAndDeduplicateAnglesLinear(TArray<double>& Angles, TArray<double>& SortBuffer)
	{
		if (Angles.Num() < 256)
		{
			Angles.Sort();
		}
		else
		{
			SortBuffer.SetNumUninitialized(Angles.Num());
			RadixSort64<ERadixSortBufferState::IsInitialized>(
				Angles.GetData(),
				SortBuffer.GetData(),
				Angles.Num(),
				FSightWeaveDoubleRadixKey());
		}
		if (Angles.Num() < 2)
		{
			return;
		}
		int32 WriteIndex = 1;
		for (int32 ReadIndex = 1; ReadIndex < Angles.Num(); ++ReadIndex)
		{
			if (FMath::Abs(Angles[ReadIndex] - Angles[WriteIndex - 1]) > 1.0e-12)
			{
				Angles[WriteIndex++] = Angles[ReadIndex];
			}
		}
		Angles.SetNum(WriteIndex, EAllowShrinking::No);
	}

	void BuildRadialEndpointEvents(
		TArray<double>& EndpointAngles,
		TArray<double>& EndpointSortBuffer,
		TConstArrayView<double> BoundaryAngles,
		const double ForwardAngle,
		const double AngularEpsilon,
		TArray<double>& OutAngles,
		TArray<FVector2D>& EndpointDirections,
		TArray<FVector2D>& OutDirections)
	{
		if (EndpointAngles.Num() >= 256)
		{
			EndpointSortBuffer.SetNumUninitialized(EndpointAngles.Num());
			RadixSort64<ERadixSortBufferState::IsInitialized>(
				EndpointAngles.GetData(),
				EndpointSortBuffer.GetData(),
				EndpointAngles.Num(),
				FSightWeaveDoubleRadixKey());
		}
		else
		{
			EndpointAngles.Sort();
		}
		EndpointDirections.SetNumUninitialized(EndpointAngles.Num());
		for (int32 EndpointIndex = 0; EndpointIndex < EndpointAngles.Num(); ++EndpointIndex)
		{
			const double WorldAngle = ForwardAngle + EndpointAngles[EndpointIndex];
			EndpointDirections[EndpointIndex] = FVector2D(
				FMath::Cos(WorldAngle),
				FMath::Sin(WorldAngle));
		}

		const double Offsets[] = { -AngularEpsilon, 0.0, AngularEpsilon };
		const double OffsetCos = FMath::Cos(AngularEpsilon);
		const double OffsetSin = FMath::Sin(AngularEpsilon);
		int32 Starts[3] = {};
		int32 Positions[3] = {};
		double CurrentValues[3] = {};
		for (int32 SequenceIndex = 0; SequenceIndex < 3; ++SequenceIndex)
		{
			double Previous = EndpointAngles.IsEmpty()
				? 0.0
				: NormalizeRadians(EndpointAngles[0] + Offsets[SequenceIndex]);
			for (int32 Index = 1; Index < EndpointAngles.Num(); ++Index)
			{
				const double Value = NormalizeRadians(EndpointAngles[Index] + Offsets[SequenceIndex]);
				if (Value < Previous)
				{
					Starts[SequenceIndex] = Index;
					break;
				}
				Previous = Value;
			}
			if (!EndpointAngles.IsEmpty())
			{
				CurrentValues[SequenceIndex] = NormalizeRadians(
					EndpointAngles[Starts[SequenceIndex]] + Offsets[SequenceIndex]);
			}
		}

		OutAngles.Reserve(BoundaryAngles.Num() + EndpointAngles.Num() * 3);
		OutDirections.Reserve(BoundaryAngles.Num() + EndpointAngles.Num() * 3);
		int32 BoundaryPosition = 0;
		const int32 EndpointCount = EndpointAngles.Num();
		while (BoundaryPosition < BoundaryAngles.Num()
			|| Positions[0] < EndpointCount
			|| Positions[1] < EndpointCount
			|| Positions[2] < EndpointCount)
		{
			double NextValue = BoundaryPosition < BoundaryAngles.Num()
				? BoundaryAngles[BoundaryPosition]
				: TNumericLimits<double>::Max();
			int32 SelectedSequence = INDEX_NONE;
			for (int32 SequenceIndex = 0; SequenceIndex < 3; ++SequenceIndex)
			{
				if (Positions[SequenceIndex] < EndpointCount
					&& CurrentValues[SequenceIndex] < NextValue)
				{
					NextValue = CurrentValues[SequenceIndex];
					SelectedSequence = SequenceIndex;
				}
			}

			if (OutAngles.IsEmpty()
				|| FMath::Abs(NextValue - OutAngles.Last()) > 1.0e-12)
			{
				OutAngles.Add(NextValue);
				if (SelectedSequence == INDEX_NONE)
				{
					const double WorldAngle = ForwardAngle + NextValue;
					OutDirections.Add(FVector2D(FMath::Cos(WorldAngle), FMath::Sin(WorldAngle)));
				}
				else
				{
					int32 EndpointIndex = Starts[SelectedSequence] + Positions[SelectedSequence];
					if (EndpointIndex >= EndpointCount)
					{
						EndpointIndex -= EndpointCount;
					}
					const FVector2D BaseDirection = EndpointDirections[EndpointIndex];
					if (SelectedSequence == 1)
					{
						OutDirections.Add(BaseDirection);
					}
					else
					{
						const double SignedSin = SelectedSequence == 0 ? -OffsetSin : OffsetSin;
						OutDirections.Add(FVector2D(
							BaseDirection.X * OffsetCos - BaseDirection.Y * SignedSin,
							BaseDirection.X * SignedSin + BaseDirection.Y * OffsetCos));
					}
				}
			}
			if (SelectedSequence == INDEX_NONE)
			{
				++BoundaryPosition;
				continue;
			}

			const int32 SequenceIndex = SelectedSequence;
			++Positions[SequenceIndex];
			if (Positions[SequenceIndex] < EndpointCount)
			{
				int32 EndpointIndex = Starts[SequenceIndex] + Positions[SequenceIndex];
				if (EndpointIndex >= EndpointCount)
				{
					EndpointIndex -= EndpointCount;
				}
				CurrentValues[SequenceIndex] = NormalizeRadians(
					EndpointAngles[EndpointIndex] + Offsets[SequenceIndex]);
			}
		}
	}

	bool HasLocalVisibilityTopologyDegeneracy(
		TConstArrayView<FVector> Vertices,
		const FSightWeaveGeometryTolerances& Tolerances)
	{
		if (Vertices.Num() < 3)
		{
			return true;
		}

		// The optimized output is a polar-ordered, star-shaped boundary, so remote
		// edges cannot cross by construction. The Reference oracle can still reject
		// tightly clustered endpoint/cone events when its inclusive topology epsilon
		// treats near-collinear edges as touching. Match that established local policy
		// in O(vertices) without restoring the Reference O(vertices^2) hot path.
		const double TopologyEpsilon = FMath::Max(
			1.0e-9,
			FMath::Min(Tolerances.PointOnEdgeEpsilon, Tolerances.DuplicateVertexEpsilon) * 0.01);
		const int32 VertexCount = Vertices.Num();
		for (int32 AIndex = 0; AIndex < VertexCount; ++AIndex)
		{
			const int32 ANext = AIndex + 1 < VertexCount ? AIndex + 1 : 0;
			const FVector2D A0(Vertices[AIndex]);
			const FVector2D A1(Vertices[ANext]);
			const double AMinX = FMath::Min(A0.X, A1.X);
			const double AMaxX = FMath::Max(A0.X, A1.X);
			const double AMinY = FMath::Min(A0.Y, A1.Y);
			const double AMaxY = FMath::Max(A0.Y, A1.Y);
			if (FVector2D::DistSquared(A0, A1) <= FMath::Square(Tolerances.DuplicateVertexEpsilon))
			{
				return true;
			}

			// A polar-ordered visibility polygon cannot cross a remote angular
			// wedge. The only epsilon-induced topology mismatch observed by the
			// Reference oracle is the immediately separated edge pair (i, i + 2).
			// Keep that parity guard without testing two geometrically disjoint
			// wedges for every emitted vertex.
			for (int32 Offset = 2; Offset <= 2 && Offset < VertexCount - 1; ++Offset)
			{
				const int32 UnwrappedBIndex = AIndex + Offset;
				const int32 BIndex = UnwrappedBIndex < VertexCount
					? UnwrappedBIndex
					: UnwrappedBIndex - VertexCount;
				const int32 BNext = BIndex + 1 < VertexCount ? BIndex + 1 : 0;
				if (ANext == BIndex || BNext == AIndex)
				{
					continue;
				}
				const FVector2D B0(Vertices[BIndex]);
				const FVector2D B1(Vertices[BNext]);
				if (AMaxX + TopologyEpsilon < FMath::Min(B0.X, B1.X)
					|| FMath::Max(B0.X, B1.X) + TopologyEpsilon < AMinX
					|| AMaxY + TopologyEpsilon < FMath::Min(B0.Y, B1.Y)
					|| FMath::Max(B0.Y, B1.Y) + TopologyEpsilon < AMinY)
				{
					continue;
				}
				if (SegmentsIntersectInclusiveUnchecked(A0, A1, B0, B1, TopologyEpsilon))
				{
					return true;
				}
			}
		}
		return false;
	}

	bool SolveResultsMatch(
		const FSightWeaveReferenceSolveResult& Optimized,
		const FSightWeaveReferenceSolveResult& Reference,
		const FSightWeaveGeometryTolerances& Tolerances,
		FString& OutError)
	{
		if (Optimized.bSucceeded != Reference.bSucceeded)
		{
			OutError = TEXT("success state differs");
			return false;
		}
		if (Optimized.CandidateSegmentCount != Reference.CandidateSegmentCount
			|| Optimized.CastRayCount != Reference.CastRayCount)
		{
			OutError = TEXT("candidate segment or ray count differs");
			return false;
		}
		if (Optimized.CandidateAnglesRadians.Num() != Reference.CandidateAnglesRadians.Num())
		{
			OutError = TEXT("candidate angle count differs");
			return false;
		}
		if (Optimized.CandidateDistances.Num() != Reference.CandidateDistances.Num())
		{
			OutError = TEXT("candidate distance count differs");
			return false;
		}
		if (Optimized.CandidateBoundaryPoints.Num() != Reference.CandidateBoundaryPoints.Num())
		{
			OutError = TEXT("candidate boundary point count differs");
			return false;
		}
		for (int32 Index = 0; Index < Optimized.CandidateAnglesRadians.Num(); ++Index)
		{
			if (!FMath::IsNearlyEqual(
				Optimized.CandidateAnglesRadians[Index],
				Reference.CandidateAnglesRadians[Index],
				1.0e-12))
			{
				OutError = FString::Printf(TEXT("candidate angle %d differs"), Index);
				return false;
			}
			if (!FMath::IsNearlyEqual(
				Optimized.CandidateDistances[Index],
				Reference.CandidateDistances[Index],
				FMath::Max(Tolerances.DuplicateVertexEpsilon, 1.0e-6)))
			{
				OutError = FString::Printf(TEXT("candidate distance %d differs"), Index);
				return false;
			}
			if (FVector2D::DistSquared(
				Optimized.CandidateBoundaryPoints[Index],
				Reference.CandidateBoundaryPoints[Index])
				> FMath::Square(FMath::Max(Tolerances.DuplicateVertexEpsilon, 1.0e-6)))
			{
				OutError = FString::Printf(TEXT("candidate boundary point %d differs"), Index);
				return false;
			}
		}
		if (Optimized.Vertices.Num() != Reference.Vertices.Num())
		{
			OutError = FString::Printf(
				TEXT("vertex count differs (%d optimized, %d reference)"),
				Optimized.Vertices.Num(),
				Reference.Vertices.Num());
			return false;
		}
		const double VertexToleranceSquared = FMath::Square(
			FMath::Max(Tolerances.DuplicateVertexEpsilon, 1.0e-6));
		for (int32 Index = 0; Index < Optimized.Vertices.Num(); ++Index)
		{
			if (FVector::DistSquared2D(Optimized.Vertices[Index], Reference.Vertices[Index])
				> VertexToleranceSquared)
			{
				OutError = FString::Printf(TEXT("vertex %d differs"), Index);
				return false;
			}
		}
		return true;
	}
}

bool FSightWeaveGeometryTolerances::IsValid() const
{
	return FMath::IsFinite(AuthoringWeldEpsilon) && AuthoringWeldEpsilon >= 0.0
		&& FMath::IsFinite(ZeroLengthEpsilon) && ZeroLengthEpsilon > 0.0
		&& FMath::IsFinite(RayParallelEpsilon) && RayParallelEpsilon > 0.0
		&& FMath::IsFinite(EndpointAngularEpsilonDegrees) && EndpointAngularEpsilonDegrees > 0.0
		&& FMath::IsFinite(PointOnEdgeEpsilon) && PointOnEdgeEpsilon >= 0.0
		&& FMath::IsFinite(PointInPolygonEpsilon) && PointInPolygonEpsilon >= 0.0
		&& FMath::IsFinite(DuplicateVertexEpsilon) && DuplicateVertexEpsilon > 0.0
		&& FMath::IsFinite(HeightOverlapEpsilon) && HeightOverlapEpsilon >= 0.0
		&& RadialBoundarySteps >= 8
		&& RadialBoundarySteps <= 2048;
}

void FSightWeaveGeometryTolerances::Normalize()
{
	if (!FMath::IsFinite(AuthoringWeldEpsilon) || AuthoringWeldEpsilon < 0.0) AuthoringWeldEpsilon = 0.1;
	if (!FMath::IsFinite(ZeroLengthEpsilon) || ZeroLengthEpsilon <= 0.0) ZeroLengthEpsilon = 0.001;
	if (!FMath::IsFinite(RayParallelEpsilon) || RayParallelEpsilon <= 0.0) RayParallelEpsilon = 1.0e-9;
	if (!FMath::IsFinite(EndpointAngularEpsilonDegrees) || EndpointAngularEpsilonDegrees <= 0.0) EndpointAngularEpsilonDegrees = 0.0025;
	if (!FMath::IsFinite(PointOnEdgeEpsilon) || PointOnEdgeEpsilon < 0.0) PointOnEdgeEpsilon = 0.05;
	if (!FMath::IsFinite(PointInPolygonEpsilon) || PointInPolygonEpsilon < 0.0) PointInPolygonEpsilon = 0.05;
	if (!FMath::IsFinite(DuplicateVertexEpsilon) || DuplicateVertexEpsilon <= 0.0) DuplicateVertexEpsilon = 0.01;
	if (!FMath::IsFinite(HeightOverlapEpsilon) || HeightOverlapEpsilon < 0.0) HeightOverlapEpsilon = 0.01;
	RadialBoundarySteps = FMath::Clamp(RadialBoundarySteps, 8, 2048);
}

bool FSightWeaveSegment2D::IsFinite() const
{
	return IsFiniteVector(A) && IsFiniteVector(B) && HeightRange.IsValid();
}

FBox2D FSightWeaveSegment2D::GetBounds() const
{
	FBox2D Bounds(ForceInit);
	Bounds += A;
	Bounds += B;
	return Bounds;
}

namespace SightWeave::Geometry
{
	bool HeightRangesOverlap(
		const FSightWeaveHeightRange& A,
		const FSightWeaveHeightRange& B,
		double Epsilon)
	{
		return A.IsValid()
			&& B.IsValid()
			&& FMath::IsFinite(Epsilon)
			&& Epsilon >= 0.0
			&& static_cast<double>(A.ZMax) + Epsilon >= static_cast<double>(B.ZMin)
			&& static_cast<double>(B.ZMax) + Epsilon >= static_cast<double>(A.ZMin);
	}

	FSightWeaveRaySegmentHit IntersectRaySegment(
		const FVector2D& RayOrigin,
		const FVector2D& RayDirection,
		const FSightWeaveSegment2D& Segment,
		const FSightWeaveGeometryTolerances& Tolerances)
	{
		FSightWeaveRaySegmentHit Result;
		if (!IsFiniteVector(RayOrigin)
			|| !IsFiniteVector(RayDirection)
			|| RayDirection.IsNearlyZero()
			|| !Segment.IsFinite()
			|| !Tolerances.IsValid())
		{
			return Result;
		}

		const FVector2D Direction = RayDirection.GetSafeNormal();
		const FVector2D SegmentVector = Segment.B - Segment.A;
		if (SegmentVector.SizeSquared() <= FMath::Square(Tolerances.ZeroLengthEpsilon))
		{
			return Result;
		}

		const double Denominator = Cross2D(Direction, SegmentVector);
		if (FMath::Abs(Denominator) <= Tolerances.RayParallelEpsilon)
		{
			return Result;
		}

		const FVector2D Offset = Segment.A - RayOrigin;
		const double RayDistance = Cross2D(Offset, SegmentVector) / Denominator;
		const double SegmentFraction = Cross2D(Offset, Direction) / Denominator;
		const double FractionEpsilon = Tolerances.PointOnEdgeEpsilon / FMath::Max(SegmentVector.Size(), 1.0);
		if (RayDistance < -Tolerances.PointOnEdgeEpsilon
			|| SegmentFraction < -FractionEpsilon
			|| SegmentFraction > 1.0 + FractionEpsilon)
		{
			return Result;
		}

		Result.bHit = true;
		Result.RayDistance = FMath::Max(0.0, RayDistance);
		Result.SegmentFraction = FMath::Clamp(SegmentFraction, 0.0, 1.0);
		Result.Point = RayOrigin + Direction * Result.RayDistance;
		Result.StableSegmentId = Segment.StableId;
		return Result;
	}

	FSightWeaveNormalizationResult NormalizeSegments(
		TConstArrayView<FSightWeaveSegment2D> Input,
		const FSightWeaveGeometryTolerances& InTolerances,
		bool bMergeCollinear)
	{
		FSightWeaveGeometryTolerances Tolerances = InTolerances;
		Tolerances.Normalize();
		FSightWeaveNormalizationResult Result;
		TArray<FVector2D> WeldPoints;

		for (int32 Index = 0; Index < Input.Num(); ++Index)
		{
			FSightWeaveSegment2D Segment = Input[Index];
			if (!Segment.IsFinite() || !Segment.FloorId.IsValid())
			{
				++Result.RemovedInvalid;
				continue;
			}
			if (Segment.SourceEdgeIndices.IsEmpty())
			{
				Segment.SourceEdgeIndices.Add(Index);
			}
			if (Segment.StableId <= 0)
			{
				Segment.StableId = static_cast<int64>(Index) + 1;
			}

			auto WeldEndpoint = [&WeldPoints, &Result, &Tolerances](FVector2D& Endpoint)
			{
				for (const FVector2D& Existing : WeldPoints)
				{
					if (PointsEqual(Endpoint, Existing, Tolerances.AuthoringWeldEpsilon))
					{
						if (Endpoint != Existing)
						{
							Endpoint = Existing;
							++Result.WeldedEndpoints;
						}
						return;
					}
				}
				WeldPoints.Add(Endpoint);
			};
			WeldEndpoint(Segment.A);
			WeldEndpoint(Segment.B);

			if (FVector2D::DistSquared(Segment.A, Segment.B) <= FMath::Square(Tolerances.ZeroLengthEpsilon))
			{
				++Result.RemovedZeroLength;
				continue;
			}

			bool bDuplicate = false;
			for (const FSightWeaveSegment2D& Existing : Result.Segments)
			{
				if (!SameMetadata(Segment, Existing, Tolerances.HeightOverlapEpsilon))
				{
					continue;
				}
				const bool bSameDirection = PointsEqual(Segment.A, Existing.A, Tolerances.AuthoringWeldEpsilon)
					&& PointsEqual(Segment.B, Existing.B, Tolerances.AuthoringWeldEpsilon);
				const bool bReverseDirection = PointsEqual(Segment.A, Existing.B, Tolerances.AuthoringWeldEpsilon)
					&& PointsEqual(Segment.B, Existing.A, Tolerances.AuthoringWeldEpsilon);
				if (bSameDirection || bReverseDirection)
				{
					bDuplicate = true;
					break;
				}
			}
			if (bDuplicate)
			{
				++Result.RemovedDuplicates;
				continue;
			}
			Result.Segments.Add(MoveTemp(Segment));
		}

		if (bMergeCollinear)
		{
			bool bMerged = true;
			while (bMerged)
			{
				bMerged = false;
				for (int32 AIndex = 0; AIndex < Result.Segments.Num() && !bMerged; ++AIndex)
				{
					for (int32 BIndex = AIndex + 1; BIndex < Result.Segments.Num(); ++BIndex)
					{
						FSightWeaveSegment2D Merged;
						if (TryMergeCollinear(Result.Segments[AIndex], Result.Segments[BIndex], Tolerances.AuthoringWeldEpsilon, Merged))
						{
							Result.Segments[AIndex] = MoveTemp(Merged);
							Result.Segments.RemoveAt(BIndex);
							++Result.CollinearMerges;
							bMerged = true;
							break;
						}
					}
				}
			}
		}

		Result.Segments.Sort([](const FSightWeaveSegment2D& A, const FSightWeaveSegment2D& B)
		{
			if (A.FloorId != B.FloorId)
			{
				return A.FloorId.GetValue().LexicalLess(B.FloorId.GetValue());
			}
			FVector2D A0;
			FVector2D A1;
			FVector2D B0;
			FVector2D B1;
			CanonicalEndpoints(A, A0, A1);
			CanonicalEndpoints(B, B0, B1);
			if (A0 != B0) return LexicalPointLess(A0, B0);
			if (A1 != B1) return LexicalPointLess(A1, B1);
			return A.StableId < B.StableId;
		});
		return Result;
	}

	bool IsPointInPolygon(
		const FVector2D& Point,
		TConstArrayView<FVector> Vertices,
		const FSightWeaveGeometryTolerances& Tolerances)
	{
		if (!IsFiniteVector(Point) || Vertices.Num() < 3 || !Tolerances.IsValid())
		{
			return false;
		}

		bool bInside = false;
		for (int32 Index = 0, Previous = Vertices.Num() - 1; Index < Vertices.Num(); Previous = Index++)
		{
			const FVector2D A(Vertices[Previous].X, Vertices[Previous].Y);
			const FVector2D B(Vertices[Index].X, Vertices[Index].Y);
			if (!IsFiniteVector(A) || !IsFiniteVector(B))
			{
				return false;
			}
			if (DistanceSquaredToSegment(Point, A, B) <= FMath::Square(Tolerances.PointOnEdgeEpsilon))
			{
				return true;
			}
			const bool bCrossesY = (A.Y > Point.Y) != (B.Y > Point.Y);
			if (bCrossesY)
			{
				const double IntersectionX = A.X + (Point.Y - A.Y) * (B.X - A.X) / (B.Y - A.Y);
				if (Point.X <= IntersectionX + Tolerances.PointInPolygonEpsilon)
				{
					bInside = !bInside;
				}
			}
		}
		return bInside;
	}

	bool IsSimplePolygon(
		TConstArrayView<FVector> Vertices,
		const FSightWeaveGeometryTolerances& Tolerances)
	{
		if (Vertices.Num() < 3 || !Tolerances.IsValid())
		{
			return false;
		}
		for (const FVector& Vertex : Vertices)
		{
			if (!IsFiniteVector(Vertex))
			{
				return false;
			}
		}
		// Topology tests need a substantially tighter tolerance than inclusive gameplay
		// containment. Endpoint +/- epsilon rays intentionally create short collinear
		// boundary runs which are not self intersections.
		const double TopologyEpsilon = FMath::Max(
			1.0e-9,
			FMath::Min(Tolerances.PointOnEdgeEpsilon, Tolerances.DuplicateVertexEpsilon) * 0.01);
		for (int32 AIndex = 0; AIndex < Vertices.Num(); ++AIndex)
		{
			const int32 ANext = (AIndex + 1) % Vertices.Num();
			const FVector2D A0(Vertices[AIndex].X, Vertices[AIndex].Y);
			const FVector2D A1(Vertices[ANext].X, Vertices[ANext].Y);
			if (FVector2D::DistSquared(A0, A1) <= FMath::Square(Tolerances.DuplicateVertexEpsilon))
			{
				return false;
			}
			for (int32 BIndex = AIndex + 1; BIndex < Vertices.Num(); ++BIndex)
			{
				const int32 BNext = (BIndex + 1) % Vertices.Num();
				if (AIndex == BIndex || ANext == BIndex || BNext == AIndex)
				{
					continue;
				}
				const FVector2D B0(Vertices[BIndex].X, Vertices[BIndex].Y);
				const FVector2D B1(Vertices[BNext].X, Vertices[BNext].Y);
				if (SegmentsIntersectInclusive(A0, A1, B0, B1, TopologyEpsilon))
				{
					return false;
				}
			}
		}
		return true;
	}

	FSightWeaveReferenceSolveResult SolveReferencePolygon(const FSightWeaveReferenceSolveInput& Input)
	{
		FSightWeaveReferenceSolveResult Result;
		Result.SolverModeUsed = ESightWeaveSolverMode::Reference;
		const double TotalStartSeconds = FPlatformTime::Seconds();
		if (!IsFiniteVector(Input.Origin)
			|| !IsFiniteVector(Input.Forward)
			|| Input.Forward.IsNearlyZero()
			|| !Input.FloorId.IsValid()
			|| !Input.HeightRange.IsValid()
			|| !FMath::IsFinite(Input.Range)
			|| Input.Range <= 0.0
			|| !FMath::IsFinite(Input.HalfAngleDegrees)
			|| Input.HalfAngleDegrees < 0.0
			|| Input.HalfAngleDegrees > 180.0
			|| !FMath::IsFinite(Input.NearAwarenessRadius)
			|| Input.NearAwarenessRadius < 0.0
			|| Input.NearAwarenessRadius > Input.Range
			|| !Input.Tolerances.IsValid())
		{
			Result.Error = TEXT("Invalid reference solve input");
			return Result;
		}

		const FVector2D Origin(Input.Origin.X, Input.Origin.Y);
		const FVector2D Forward = Input.Forward.GetSafeNormal();
		const double ForwardAngle = FMath::Atan2(Forward.Y, Forward.X);
		const double HalfAngleRadians = FMath::DegreesToRadians(Input.HalfAngleDegrees);
		const double AngularEpsilon = FMath::DegreesToRadians(Input.Tolerances.EndpointAngularEpsilonDegrees);
		const bool bFullCircle = Input.Shape == ESightWeaveSourceShape::Radial || Input.NearAwarenessRadius > 0.0;

		const double BoundaryEventStartSeconds = FPlatformTime::Seconds();
		if (bFullCircle)
		{
			for (int32 Step = 0; Step < Input.Tolerances.RadialBoundarySteps; ++Step)
			{
				AddUniqueAngle(Result.CandidateAnglesRadians,
					-PI + SightWeaveTwoPi * static_cast<double>(Step) / Input.Tolerances.RadialBoundarySteps);
			}
			if (Input.Shape != ESightWeaveSourceShape::Radial)
			{
				for (const double Boundary : { -HalfAngleRadians, HalfAngleRadians })
				{
					AddUniqueAngle(Result.CandidateAnglesRadians, Boundary - AngularEpsilon);
					AddUniqueAngle(Result.CandidateAnglesRadians, Boundary);
					AddUniqueAngle(Result.CandidateAnglesRadians, Boundary + AngularEpsilon);
				}
			}
		}
		else
		{
			const int32 ArcSteps = FMath::Max(2, FMath::CeilToInt(
				Input.Tolerances.RadialBoundarySteps * (2.0 * Input.HalfAngleDegrees / 360.0)));
			for (int32 Step = 0; Step <= ArcSteps; ++Step)
			{
				const double Alpha = static_cast<double>(Step) / ArcSteps;
				Result.CandidateAnglesRadians.Add(FMath::Lerp(-HalfAngleRadians, HalfAngleRadians, Alpha));
			}
			Result.CandidateAnglesRadians.Add(-HalfAngleRadians);
			Result.CandidateAnglesRadians.Add(HalfAngleRadians);
		}
		Result.StageMetrics.BoundaryEventMicroseconds =
			(FPlatformTime::Seconds() - BoundaryEventStartSeconds) * 1000000.0;

		const double CandidateEventStartSeconds = FPlatformTime::Seconds();
		TArray<const FSightWeaveSegment2D*> CandidateSegments;
		CandidateSegments.Reserve(Input.Segments.Num());
		Result.CandidateAnglesRadians.Reserve(
			Result.CandidateAnglesRadians.Num() + Input.Segments.Num() * 6);
		for (const FSightWeaveSegment2D& Segment : Input.Segments)
		{
			if (!Segment.IsFinite()
				|| Segment.FloorId != Input.FloorId
				|| !HeightRangesOverlap(Segment.HeightRange, Input.HeightRange, Input.Tolerances.HeightOverlapEpsilon))
			{
				continue;
			}
			CandidateSegments.Add(&Segment);
			for (const FVector2D Endpoint : { Segment.A, Segment.B })
			{
				const double EndpointAngle = NormalizeRadians(FMath::Atan2(Endpoint.Y - Origin.Y, Endpoint.X - Origin.X) - ForwardAngle);
				for (const double Offset : { -AngularEpsilon, 0.0, AngularEpsilon })
				{
					const double CandidateAngle = NormalizeRadians(EndpointAngle + Offset);
					if (SourceRadiusAtRelativeAngle(Input, CandidateAngle) > 0.0)
					{
						AddUniqueAngle(Result.CandidateAnglesRadians, CandidateAngle);
					}
				}
			}
		}
		Result.CandidateSegmentCount = CandidateSegments.Num();
		Result.StageMetrics.CandidateFilterAndEndpointEventMicroseconds =
			(FPlatformTime::Seconds() - CandidateEventStartSeconds) * 1000000.0;

		const double EventSortStartSeconds = FPlatformTime::Seconds();
		SortAndDeduplicateAngles(Result.CandidateAnglesRadians);

		if (!bFullCircle)
		{
			Result.CandidateAnglesRadians.RemoveAll([HalfAngleRadians](double Angle)
			{
				return Angle < -HalfAngleRadians - 1.0e-12 || Angle > HalfAngleRadians + 1.0e-12;
			});
			Result.Vertices.Add(Input.Origin);
		}
		Result.Vertices.Reserve(Result.CandidateAnglesRadians.Num() + (bFullCircle ? 0 : 1));
		Result.CandidateDistances.Reserve(Result.CandidateAnglesRadians.Num());
		Result.CandidateBoundaryPoints.Reserve(Result.CandidateAnglesRadians.Num());
		Result.StageMetrics.EventSortDeduplicateMicroseconds =
			(FPlatformTime::Seconds() - EventSortStartSeconds) * 1000000.0;

		const double RayCastStartSeconds = FPlatformTime::Seconds();
		for (const double RelativeAngle : Result.CandidateAnglesRadians)
		{
			const double MaximumDistance = SourceRadiusAtRelativeAngle(Input, RelativeAngle);
			if (MaximumDistance <= 0.0)
			{
				continue;
			}
			const double WorldAngle = ForwardAngle + RelativeAngle;
			const FVector2D Direction(FMath::Cos(WorldAngle), FMath::Sin(WorldAngle));
			double ClosestDistance = MaximumDistance;
			int64 ClosestStableId = MAX_int64;
			for (const FSightWeaveSegment2D* Segment : CandidateSegments)
			{
				++Result.StageMetrics.TestedSegments;
				const FSightWeaveRaySegmentHit Hit = IntersectRaySegment(Origin, Direction, *Segment, Input.Tolerances);
				if (!Hit.bHit
					|| Hit.RayDistance <= Input.Tolerances.PointOnEdgeEpsilon
					|| Hit.RayDistance > MaximumDistance + Input.Tolerances.PointOnEdgeEpsilon)
				{
					continue;
				}
				const bool bCloser = Hit.RayDistance < ClosestDistance - Input.Tolerances.DuplicateVertexEpsilon;
				const bool bStableTie = FMath::Abs(Hit.RayDistance - ClosestDistance) <= Input.Tolerances.DuplicateVertexEpsilon
					&& Hit.StableSegmentId < ClosestStableId;
				if (bCloser || bStableTie)
				{
					ClosestDistance = Hit.RayDistance;
					ClosestStableId = Hit.StableSegmentId;
				}
			}
			Result.CandidateDistances.Add(ClosestDistance);
			++Result.CastRayCount;
			const FVector2D Vertex2D = Origin + Direction * ClosestDistance;
			Result.CandidateBoundaryPoints.Add(Vertex2D);
			const FVector Vertex(Vertex2D.X, Vertex2D.Y, Input.Origin.Z);
			if (Result.Vertices.IsEmpty()
				|| FVector::DistSquared2D(Result.Vertices.Last(), Vertex) > FMath::Square(Input.Tolerances.DuplicateVertexEpsilon))
			{
				Result.Vertices.Add(Vertex);
			}
		}
		Result.StageMetrics.RayCastMicroseconds =
			(FPlatformTime::Seconds() - RayCastStartSeconds) * 1000000.0;

		const double PostProcessStartSeconds = FPlatformTime::Seconds();
		if (Result.Vertices.Num() >= 2
			&& FVector::DistSquared2D(Result.Vertices[0], Result.Vertices.Last()) <= FMath::Square(Input.Tolerances.DuplicateVertexEpsilon))
		{
			Result.Vertices.Pop();
		}

		if (Result.Vertices.Num() < 3)
		{
			Result.Error = TEXT("Reference solve emitted fewer than three vertices");
			Result.StageMetrics.PolygonPostProcessMicroseconds =
				(FPlatformTime::Seconds() - PostProcessStartSeconds) * 1000000.0;
			Result.StageMetrics.TotalMicroseconds =
				(FPlatformTime::Seconds() - TotalStartSeconds) * 1000000.0;
			return Result;
		}
		Result.StageMetrics.PolygonPostProcessMicroseconds =
			(FPlatformTime::Seconds() - PostProcessStartSeconds) * 1000000.0;
		Result.StageMetrics.WorkingSetAllocatedBytes =
			Result.Vertices.GetAllocatedSize()
			+ Result.CandidateAnglesRadians.GetAllocatedSize()
			+ Result.CandidateDistances.GetAllocatedSize()
			+ Result.CandidateBoundaryPoints.GetAllocatedSize()
			+ CandidateSegments.GetAllocatedSize();

		const double TopologyStartSeconds = FPlatformTime::Seconds();
		if (!IsSimplePolygon(Result.Vertices, Input.Tolerances))
		{
			Result.Error = TEXT("Reference solve emitted a non-simple polygon");
			Result.StageMetrics.TopologyValidationMicroseconds =
				(FPlatformTime::Seconds() - TopologyStartSeconds) * 1000000.0;
			Result.StageMetrics.TotalMicroseconds =
				(FPlatformTime::Seconds() - TotalStartSeconds) * 1000000.0;
			return Result;
		}
		Result.StageMetrics.TopologyValidationMicroseconds =
			(FPlatformTime::Seconds() - TopologyStartSeconds) * 1000000.0;

		Result.bSucceeded = true;
		Result.StageMetrics.TotalMicroseconds =
			(FPlatformTime::Seconds() - TotalStartSeconds) * 1000000.0;
		return Result;
	}

	void SolveOptimizedPolygonInto(
		const FSightWeaveReferenceSolveInput& Input,
		FSightWeaveReferenceSolveResult& Result)
	{
		ResetOptimizedSolveResult(Result);
		const double TotalStartSeconds = FPlatformTime::Seconds();
		if (!IsFiniteVector(Input.Origin)
			|| !IsFiniteVector(Input.Forward)
			|| Input.Forward.IsNearlyZero()
			|| !Input.FloorId.IsValid()
			|| !Input.HeightRange.IsValid()
			|| !FMath::IsFinite(Input.Range)
			|| Input.Range <= 0.0
			|| !FMath::IsFinite(Input.HalfAngleDegrees)
			|| Input.HalfAngleDegrees < 0.0
			|| Input.HalfAngleDegrees > 180.0
			|| !FMath::IsFinite(Input.NearAwarenessRadius)
			|| Input.NearAwarenessRadius < 0.0
			|| Input.NearAwarenessRadius > Input.Range
			|| !Input.Tolerances.IsValid())
		{
			Result.Error = TEXT("Invalid optimized solve input");
			return;
		}

		FSightWeaveSolverFrameLease FrameLease;
		FSightWeaveSolverFrame& SolverFrame = FrameLease.Get();
		TArray<FSightWeavePreparedSegment>& CandidateSegments = SolverFrame.CandidateSegments;
		TArray<FSightWeaveAngularInterval>& AngularIntervals = SolverFrame.AngularIntervals;
		TArray<FSightWeaveAngularInterval>& AngularIntervalSortBuffer = SolverFrame.AngularIntervalSortBuffer;
		TArray<FSightWeaveActiveInterval>& ActiveIntervals = SolverFrame.ActiveIntervals;
		TArray<double>& AngleSortBuffer = SolverFrame.AngleSortBuffer;
		TArray<double>& EndpointAngles = SolverFrame.EndpointAngles;
		TArray<double>& EndpointAngleSortBuffer = SolverFrame.EndpointAngleSortBuffer;
		TArray<FVector2D>& EndpointDirections = SolverFrame.EndpointDirections;
		TArray<FVector2D>& CandidateDirections = SolverFrame.CandidateDirections;
		TArray<double>& BoundaryAngles = SolverFrame.BoundaryAngles;

		const FVector2D Origin(Input.Origin.X, Input.Origin.Y);
		const FVector2D Forward = Input.Forward.GetSafeNormal();
		const double ForwardAngle = FMath::Atan2(Forward.Y, Forward.X);
		const double HalfAngleRadians = FMath::DegreesToRadians(Input.HalfAngleDegrees);
		const double AngularEpsilon = FMath::DegreesToRadians(Input.Tolerances.EndpointAngularEpsilonDegrees);
		const bool bFullCircle = Input.Shape == ESightWeaveSourceShape::Radial || Input.NearAwarenessRadius > 0.0;
		const bool bPureRadial = Input.Shape == ESightWeaveSourceShape::Radial;

		const double BoundaryEventStartSeconds = FPlatformTime::Seconds();
		if (bFullCircle)
		{
			for (int32 Step = 0; Step < Input.Tolerances.RadialBoundarySteps; ++Step)
			{
				const double BoundaryAngle =
					-PI + SightWeaveTwoPi * static_cast<double>(Step) / Input.Tolerances.RadialBoundarySteps;
				if (bPureRadial)
				{
					BoundaryAngles.Add(BoundaryAngle);
				}
				else
				{
					AddUniqueAngle(Result.CandidateAnglesRadians, BoundaryAngle);
				}
			}
			if (Input.Shape != ESightWeaveSourceShape::Radial)
			{
				for (const double Boundary : { -HalfAngleRadians, HalfAngleRadians })
				{
					AddUniqueAngle(Result.CandidateAnglesRadians, Boundary - AngularEpsilon);
					AddUniqueAngle(Result.CandidateAnglesRadians, Boundary);
					AddUniqueAngle(Result.CandidateAnglesRadians, Boundary + AngularEpsilon);
				}
			}
		}
		else
		{
			const int32 ArcSteps = FMath::Max(2, FMath::CeilToInt(
				Input.Tolerances.RadialBoundarySteps * (2.0 * Input.HalfAngleDegrees / 360.0)));
			for (int32 Step = 0; Step <= ArcSteps; ++Step)
			{
				const double Alpha = static_cast<double>(Step) / ArcSteps;
				Result.CandidateAnglesRadians.Add(FMath::Lerp(-HalfAngleRadians, HalfAngleRadians, Alpha));
			}
			Result.CandidateAnglesRadians.Add(-HalfAngleRadians);
			Result.CandidateAnglesRadians.Add(HalfAngleRadians);
		}
		Result.StageMetrics.BoundaryEventMicroseconds =
			(FPlatformTime::Seconds() - BoundaryEventStartSeconds) * 1000000.0;

		const double CandidateEventStartSeconds = FPlatformTime::Seconds();
		CandidateSegments.Reserve(Input.Segments.Num());
		if (bPureRadial)
		{
			EndpointAngles.Reserve(Input.Segments.Num() * 2);
		}
		else
		{
			Result.CandidateAnglesRadians.Reserve(
				Result.CandidateAnglesRadians.Num() + Input.Segments.Num() * 6);
		}
		for (const FSightWeaveSegment2D& Segment : Input.Segments)
		{
			if (!Segment.IsFinite()
				|| Segment.FloorId != Input.FloorId
				|| !HeightRangesOverlap(Segment.HeightRange, Input.HeightRange, Input.Tolerances.HeightOverlapEpsilon))
			{
				continue;
			}
			FSightWeavePreparedSegment& Prepared = CandidateSegments.AddDefaulted_GetRef();
			Prepared.OffsetA = Segment.A - Origin;
			Prepared.Vector = Segment.B - Segment.A;
			const FVector2D OffsetB = Prepared.OffsetA + Prepared.Vector;
			Prepared.RayDistanceNumerator = Cross2D(Prepared.OffsetA, Prepared.Vector);
			Prepared.AAngle = NormalizeRadians(
				FMath::Atan2(Prepared.OffsetA.Y, Prepared.OffsetA.X) - ForwardAngle);
			Prepared.BAngle = NormalizeRadians(
				FMath::Atan2(OffsetB.Y, OffsetB.X) - ForwardAngle);
			Prepared.MinimumEndpointDistance = FMath::Sqrt(FMath::Min(
				Prepared.OffsetA.SizeSquared(),
				OffsetB.SizeSquared()));
			Prepared.FractionEpsilon =
				Input.Tolerances.PointOnEdgeEpsilon / FMath::Max(Prepared.Vector.Size(), 1.0);
			Prepared.StableId = Segment.StableId;
			Prepared.OriginDistanceSquared = DistanceSquaredToSegment(
				FVector2D::ZeroVector,
				Prepared.OffsetA,
				OffsetB);
			Prepared.bOriginOnSegment = Prepared.OriginDistanceSquared
				<= FMath::Square(Input.Tolerances.PointOnEdgeEpsilon);
			const double PreparedEndpointAngles[] = { Prepared.AAngle, Prepared.BAngle };
			for (int32 EndpointIndex = 0; EndpointIndex < 2; ++EndpointIndex)
			{
				const double EndpointAngle = PreparedEndpointAngles[EndpointIndex];
				if (bPureRadial)
				{
					EndpointAngles.Add(EndpointAngle);
					continue;
				}
				const double Offsets[] = { -AngularEpsilon, 0.0, AngularEpsilon };
				for (int32 OffsetIndex = 0; OffsetIndex < 3; ++OffsetIndex)
				{
					const double CandidateAngle = NormalizeRadians(EndpointAngle + Offsets[OffsetIndex]);
					if (SourceRadiusAtRelativeAngle(Input, CandidateAngle) > 0.0)
					{
						AddUniqueAngle(Result.CandidateAnglesRadians, CandidateAngle);
					}
				}
			}
		}
		Result.CandidateSegmentCount = CandidateSegments.Num();
		Result.StageMetrics.CandidateFilterAndEndpointEventMicroseconds =
			(FPlatformTime::Seconds() - CandidateEventStartSeconds) * 1000000.0;

		const double EventSortStartSeconds = FPlatformTime::Seconds();
		if (bPureRadial)
		{
			BuildRadialEndpointEvents(
				EndpointAngles,
				EndpointAngleSortBuffer,
				BoundaryAngles,
				ForwardAngle,
				AngularEpsilon,
				Result.CandidateAnglesRadians,
				EndpointDirections,
				CandidateDirections);
		}
		else
		{
			SortAndDeduplicateAnglesLinear(Result.CandidateAnglesRadians, AngleSortBuffer);
		}
		if (!bFullCircle)
		{
			Result.CandidateAnglesRadians.RemoveAll([HalfAngleRadians](const double Angle)
			{
				return Angle < -HalfAngleRadians - 1.0e-12 || Angle > HalfAngleRadians + 1.0e-12;
			});
		}
		const int32 CandidateRayCount = Result.CandidateAnglesRadians.Num();
		Result.Vertices.SetNumUninitialized(CandidateRayCount + (bFullCircle ? 0 : 1));
		Result.CandidateDistances.SetNumUninitialized(CandidateRayCount);
		Result.CandidateBoundaryPoints.SetNumUninitialized(CandidateRayCount);
		Result.StageMetrics.EventSortDeduplicateMicroseconds =
			(FPlatformTime::Seconds() - EventSortStartSeconds) * 1000000.0;

		const double AccelerationStartSeconds = FPlatformTime::Seconds();
		BuildAngularIntervals(
			CandidateSegments,
			Origin,
			ForwardAngle,
			Input.Tolerances,
			AngularIntervals,
			AngularIntervalSortBuffer);
		Result.StageMetrics.AccelerationBuildMicroseconds =
			(FPlatformTime::Seconds() - AccelerationStartSeconds) * 1000000.0;

		const double RayCastStartSeconds = FPlatformTime::Seconds();
		int32 NextIntervalIndex = 0;
		int32 VertexWriteIndex = 0;
		if (!bFullCircle)
		{
			Result.Vertices[VertexWriteIndex++] = Input.Origin;
		}
		ActiveIntervals.Reserve(AngularIntervals.Num());
		double EarliestActiveEndAngle = TNumericLimits<double>::Max();
		auto ActiveIntervalLess = [](const FSightWeaveActiveInterval& A, const FSightWeaveActiveInterval& B)
		{
			return A.OriginDistanceSquared < B.OriginDistanceSquared
				|| (A.OriginDistanceSquared == B.OriginDistanceSquared
					&& (A.StableId < B.StableId
						|| (A.StableId == B.StableId && A.SegmentIndex < B.SegmentIndex)));
		};
		for (int32 RayIndex = 0; RayIndex < CandidateRayCount; ++RayIndex)
		{
			const double RelativeAngle = Result.CandidateAnglesRadians[RayIndex];
			while (NextIntervalIndex < AngularIntervals.Num()
				&& AngularIntervals[NextIntervalIndex].StartAngle <= RelativeAngle + 1.0e-12)
			{
				const FSightWeaveAngularInterval& Interval = AngularIntervals[NextIntervalIndex++];
				const FSightWeavePreparedSegment& Segment = CandidateSegments[Interval.SegmentIndex];
				const FSightWeaveActiveInterval NewActiveInterval = {
					Interval.EndAngle,
					Segment.OriginDistanceSquared,
					Segment.StableId,
					Interval.SegmentIndex };
				int32 InsertLower = 0;
				int32 InsertUpper = ActiveIntervals.Num();
				while (InsertLower < InsertUpper)
				{
					const int32 Middle = InsertLower + (InsertUpper - InsertLower) / 2;
					if (ActiveIntervalLess(ActiveIntervals[Middle], NewActiveInterval))
					{
						InsertLower = Middle + 1;
					}
					else
					{
						InsertUpper = Middle;
					}
				}
				ActiveIntervals.Insert(NewActiveInterval, InsertLower);
				EarliestActiveEndAngle = FMath::Min(EarliestActiveEndAngle, Interval.EndAngle);
			}
			if (EarliestActiveEndAngle < RelativeAngle - 1.0e-12)
			{
				EarliestActiveEndAngle = TNumericLimits<double>::Max();
				for (int32 ActiveIndex = ActiveIntervals.Num() - 1; ActiveIndex >= 0; --ActiveIndex)
				{
					if (ActiveIntervals[ActiveIndex].EndAngle < RelativeAngle - 1.0e-12)
					{
						ActiveIntervals.RemoveAt(ActiveIndex, 1, EAllowShrinking::No);
					}
					else
					{
						EarliestActiveEndAngle = FMath::Min(
							EarliestActiveEndAngle,
							ActiveIntervals[ActiveIndex].EndAngle);
					}
				}
			}

			const double MaximumDistance = bPureRadial
				? Input.Range
				: SourceRadiusAtRelativeAngle(Input, RelativeAngle);
			const FVector2D Direction = bPureRadial
				? CandidateDirections[RayIndex]
				: FVector2D(
					FMath::Cos(ForwardAngle + RelativeAngle),
					FMath::Sin(ForwardAngle + RelativeAngle));
			double ClosestDistance = MaximumDistance;
			int64 ClosestStableId = MAX_int64;
			for (const FSightWeaveActiveInterval& ActiveInterval : ActiveIntervals)
			{
				if (ActiveInterval.OriginDistanceSquared > FMath::Square(
						ClosestDistance + Input.Tolerances.DuplicateVertexEpsilon))
				{
					break;
				}
				const FSightWeavePreparedSegment& Segment = CandidateSegments[ActiveInterval.SegmentIndex];
				++Result.StageMetrics.TestedSegments;
				double HitDistance = 0.0;
				if (!IntersectTrustedRaySegment(
						Direction,
						Segment,
						Input.Tolerances,
						HitDistance)
					|| HitDistance <= Input.Tolerances.PointOnEdgeEpsilon
					|| HitDistance > MaximumDistance + Input.Tolerances.PointOnEdgeEpsilon)
				{
					continue;
				}
				const bool bCloser =
					HitDistance < ClosestDistance - Input.Tolerances.DuplicateVertexEpsilon;
				const bool bStableTie =
					FMath::Abs(HitDistance - ClosestDistance) <= Input.Tolerances.DuplicateVertexEpsilon
					&& Segment.StableId < ClosestStableId;
				if (bCloser || bStableTie)
				{
					ClosestDistance = HitDistance;
					ClosestStableId = Segment.StableId;
				}
			}

			Result.CandidateDistances[RayIndex] = ClosestDistance;
			const FVector2D Vertex2D = Origin + Direction * ClosestDistance;
			Result.CandidateBoundaryPoints[RayIndex] = Vertex2D;
			const FVector Vertex(Vertex2D.X, Vertex2D.Y, Input.Origin.Z);
			if (VertexWriteIndex == 0
				|| FVector::DistSquared2D(Result.Vertices[VertexWriteIndex - 1], Vertex)
					> FMath::Square(Input.Tolerances.DuplicateVertexEpsilon))
			{
				Result.Vertices[VertexWriteIndex++] = Vertex;
			}
		}
		Result.CastRayCount = CandidateRayCount;
		Result.Vertices.SetNum(VertexWriteIndex, EAllowShrinking::No);
		Result.StageMetrics.RayCastMicroseconds =
			(FPlatformTime::Seconds() - RayCastStartSeconds) * 1000000.0;

		const double PostProcessStartSeconds = FPlatformTime::Seconds();
		if (Result.Vertices.Num() >= 2
			&& FVector::DistSquared2D(Result.Vertices[0], Result.Vertices.Last())
				<= FMath::Square(Input.Tolerances.DuplicateVertexEpsilon))
		{
			Result.Vertices.Pop(EAllowShrinking::No);
		}
		if (Result.Vertices.Num() < 3)
		{
			Result.Error = TEXT("Optimized solve emitted fewer than three vertices");
			Result.StageMetrics.PolygonPostProcessMicroseconds =
				(FPlatformTime::Seconds() - PostProcessStartSeconds) * 1000000.0;
			Result.StageMetrics.TotalMicroseconds =
				(FPlatformTime::Seconds() - TotalStartSeconds) * 1000000.0;
			return;
		}
		Result.StageMetrics.PolygonPostProcessMicroseconds =
			(FPlatformTime::Seconds() - PostProcessStartSeconds) * 1000000.0;
		const double TopologyStartSeconds = FPlatformTime::Seconds();
		if (HasLocalVisibilityTopologyDegeneracy(Result.Vertices, Input.Tolerances))
		{
			Result.Error = TEXT("Optimized solve emitted a local topology degeneracy");
			Result.StageMetrics.TopologyValidationMicroseconds =
				(FPlatformTime::Seconds() - TopologyStartSeconds) * 1000000.0;
			Result.StageMetrics.TotalMicroseconds =
				(FPlatformTime::Seconds() - TotalStartSeconds) * 1000000.0;
			return;
		}
		Result.StageMetrics.TopologyValidationMicroseconds =
			(FPlatformTime::Seconds() - TopologyStartSeconds) * 1000000.0;
		Result.StageMetrics.WorkingSetAllocatedBytes =
			Result.Vertices.GetAllocatedSize()
			+ Result.CandidateAnglesRadians.GetAllocatedSize()
			+ Result.CandidateDistances.GetAllocatedSize()
			+ Result.CandidateBoundaryPoints.GetAllocatedSize()
			+ CandidateSegments.GetAllocatedSize()
			+ AngularIntervals.GetAllocatedSize()
			+ AngularIntervalSortBuffer.GetAllocatedSize()
			+ ActiveIntervals.GetAllocatedSize()
			+ AngleSortBuffer.GetAllocatedSize()
			+ EndpointAngles.GetAllocatedSize()
			+ EndpointAngleSortBuffer.GetAllocatedSize()
			+ EndpointDirections.GetAllocatedSize()
			+ CandidateDirections.GetAllocatedSize()
			+ BoundaryAngles.GetAllocatedSize();
		Result.bSucceeded = true;
		Result.StageMetrics.TotalMicroseconds =
			(FPlatformTime::Seconds() - TotalStartSeconds) * 1000000.0;
	}

	FSightWeaveReferenceSolveResult SolveOptimizedPolygon(const FSightWeaveReferenceSolveInput& Input)
	{
		FSightWeaveReferenceSolveResult Result;
		SolveOptimizedPolygonInto(Input, Result);
		return Result;
	}

	void SolvePolygonInto(
		const FSightWeaveReferenceSolveInput& Input,
		const ESightWeaveSolverMode Mode,
		FSightWeaveReferenceSolveResult& Result)
	{
#if UE_BUILD_SHIPPING
		SolveOptimizedPolygonInto(Input, Result);
#else
		if (Mode == ESightWeaveSolverMode::Reference)
		{
			Result = SolveReferencePolygon(Input);
			return;
		}
		SolveOptimizedPolygonInto(Input, Result);
		if (Mode != ESightWeaveSolverMode::Verify)
		{
			return;
		}

		FSightWeaveReferenceSolveResult Reference = SolveReferencePolygon(Input);
		FString VerificationError;
		if (SolveResultsMatch(Result, Reference, Input.Tolerances, VerificationError))
		{
			Result.bVerificationMatched = true;
			return;
		}

		Reference.bVerificationMatched = false;
		Reference.bUsedReferenceFallback = true;
		Reference.VerificationError = MoveTemp(VerificationError);
		UE_LOG(
			LogSightWeaveGeometry,
			Error,
			TEXT("Optimized solver verification mismatch; using reference result: %s"),
			*Reference.VerificationError);
		Result = MoveTemp(Reference);
#endif
	}

	FSightWeaveReferenceSolveResult SolvePolygon(
		const FSightWeaveReferenceSolveInput& Input,
		const ESightWeaveSolverMode Mode)
	{
		FSightWeaveReferenceSolveResult Result;
		SolvePolygonInto(Input, Mode, Result);
		return Result;
	}

#if WITH_DEV_AUTOMATION_TESTS
	namespace Testing
	{
		bool ExerciseOptimizedSolverScratchReentrancy(const int32 Depth)
		{
			if (Depth <= 0 || Depth > 64)
			{
				return false;
			}

			TArray<const FSightWeaveSolverFrame*> ActiveFrames;
			ActiveFrames.Reserve(Depth);
			return ExerciseScratchReentrancy(Depth, ActiveFrames)
				&& ActiveFrames.IsEmpty();
		}
	}
#endif
}
