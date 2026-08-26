#include "SightWeaveGeometry.h"

#include "SightWeaveOptimizedSolveCache.h"

#include "Algo/Unique.h"
#include "Containers/StaticArray.h"
#include "HAL/PlatformTime.h"
#include "HAL/ThreadSingleton.h"
#include "Templates/Sorting.h"

DEFINE_LOG_CATEGORY_STATIC(LogSightWeaveGeometry, Log, All);

namespace
{
	constexpr double SightWeaveTwoPi = 2.0 * PI;

#if WITH_DEV_AUTOMATION_TESTS
	FSightWeaveVisionSolveSubstageProbe GVisionSolveSubstageProbe = nullptr;
	bool GVisionSolveMicroTimingEnabled = false;
	thread_local int64 GVisionSolveDiagnosticSourceId = 0;

	class FScopedVisionSolveSubstage
	{
	public:
		FScopedVisionSolveSubstage(
			FSightWeaveVisionSolveDiagnostics& InDiagnostics,
			const ESightWeaveVisionSolveSubstage InStage,
			const bool bInEmitProbe = true)
			: Diagnostics(InDiagnostics)
			, Stage(InStage)
			, bEmitProbe(bInEmitProbe)
			, bEnabled(GVisionSolveSubstageProbe != nullptr
				&& (bEmitProbe || GVisionSolveMicroTimingEnabled))
		{
			if (!bEnabled)
			{
				return;
			}
			StartCycles = FPlatformTime::Cycles64();
			if (bEmitProbe)
			{
				GVisionSolveSubstageProbe(Stage, GVisionSolveDiagnosticSourceId, true);
			}
		}

		~FScopedVisionSolveSubstage()
		{
			if (!bEnabled)
			{
				return;
			}
			if (bEmitProbe)
			{
				GVisionSolveSubstageProbe(Stage, GVisionSolveDiagnosticSourceId, false);
			}
			const uint64 ElapsedCycles = FPlatformTime::Cycles64() - StartCycles;
			FSightWeaveVisionSolveSubstageMetrics& Metrics =
				Diagnostics.Substages[static_cast<int32>(Stage)];
			Metrics.PlatformCycles += ElapsedCycles;
			Metrics.WallMicroseconds += static_cast<double>(ElapsedCycles)
				* FPlatformTime::GetSecondsPerCycle64() * 1000000.0;
			++Metrics.InvocationCount;
		}

	private:
		FSightWeaveVisionSolveDiagnostics& Diagnostics;
		ESightWeaveVisionSolveSubstage Stage;
		uint64 StartCycles = 0;
		bool bEmitProbe = true;
		bool bEnabled = false;
	};
#endif

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

	using FSightWeavePreparedSegment = FSightWeaveOptimizedPreparedSegment;

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
		TArray<double> PreviousCandidateAngles;
		TArray<double> PreviousCandidateDistances;
		TArray<FVector2D> PreviousCandidateBoundaryPoints;
		TArray<uint8> IncrementalReusedRays;

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
			PreviousCandidateAngles.Reset();
			PreviousCandidateDistances.Reset();
			PreviousCandidateBoundaryPoints.Reset();
			IncrementalReusedRays.Reset();
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
				+ BoundaryAngles.GetAllocatedSize()
				+ PreviousCandidateAngles.GetAllocatedSize()
				+ PreviousCandidateDistances.GetAllocatedSize()
				+ PreviousCandidateBoundaryPoints.GetAllocatedSize()
				+ IncrementalReusedRays.GetAllocatedSize();
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
				PreviousCandidateAngles.Empty();
				PreviousCandidateDistances.Empty();
				PreviousCandidateBoundaryPoints.Empty();
				IncrementalReusedRays.Empty();
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

	void ResetOptimizedSolveResult(
		FSightWeaveReferenceSolveResult& Result,
		const bool bPreserveVisionSolveDiagnostics = false)
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
#if WITH_DEV_AUTOMATION_TESTS
		if (!bPreserveVisionSolveDiagnostics)
		{
			Result.VisionSolveDiagnostics = {};
		}
#endif
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
		for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
		{
			const FSightWeavePreparedSegment& Segment = Segments[SegmentIndex];
			if (Segment.bOriginOnSegment)
			{
				OutIntervals.Add({ -PI, PI, SegmentIndex });
				continue;
			}

			const double SignedSpan = NormalizeRadians(Segment.BAngle - Segment.AAngle);
			const double Start =
				(SignedSpan >= 0.0 ? Segment.AAngle : Segment.BAngle) - Segment.AngularPadding;
			const double End = Start + FMath::Abs(SignedSpan) + 2.0 * Segment.AngularPadding;
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

	struct FSightWeaveDirtyAngularSector
	{
		double StartAngle = 0.0;
		double EndAngle = 0.0;
	};

	struct FSightWeaveIncrementalSolveContext
	{
		TStaticArray<FSightWeaveDirtyAngularSector, 4> DirtySectors;
		int32 DirtySectorCount = 0;
		FSightWeaveIncrementalSectorDiagnostics* Diagnostics = nullptr;
	};

	bool TolerancesExactlyMatch(
		const FSightWeaveGeometryTolerances& A,
		const FSightWeaveGeometryTolerances& B)
	{
		return A.AuthoringWeldEpsilon == B.AuthoringWeldEpsilon
			&& A.ZeroLengthEpsilon == B.ZeroLengthEpsilon
			&& A.RayParallelEpsilon == B.RayParallelEpsilon
			&& A.EndpointAngularEpsilonDegrees == B.EndpointAngularEpsilonDegrees
			&& A.PointOnEdgeEpsilon == B.PointOnEdgeEpsilon
			&& A.PointInPolygonEpsilon == B.PointInPolygonEpsilon
			&& A.DuplicateVertexEpsilon == B.DuplicateVertexEpsilon
			&& A.HeightOverlapEpsilon == B.HeightOverlapEpsilon
			&& A.RadialBoundarySteps == B.RadialBoundarySteps;
	}

	bool SegmentPreparedKeyMatches(
		const FSightWeaveSegment2D& A,
		const FSightWeaveSegment2D& B)
	{
		return A.A.X == B.A.X
			&& A.A.Y == B.A.Y
			&& A.B.X == B.B.X
			&& A.B.Y == B.B.Y
			&& A.FloorId == B.FloorId
			&& A.HeightRange.ZMin == B.HeightRange.ZMin
			&& A.HeightRange.ZMax == B.HeightRange.ZMax
			&& A.StableId == B.StableId;
	}

	FSightWeavePreparedSegment PrepareIncrementalSegment(
		const FSightWeaveSegment2D& Segment,
		const FVector2D& Origin,
		const double ForwardAngle,
		const FSightWeaveGeometryTolerances& Tolerances)
	{
		FSightWeavePreparedSegment Prepared;
		Prepared.OffsetA = Segment.A - Origin;
		Prepared.Vector = Segment.B - Segment.A;
		const FVector2D OffsetB = Prepared.OffsetA + Prepared.Vector;
		Prepared.RayDistanceNumerator = Cross2D(Prepared.OffsetA, Prepared.Vector);
		Prepared.AbsoluteAAngle = FMath::Atan2(Prepared.OffsetA.Y, Prepared.OffsetA.X);
		Prepared.AbsoluteBAngle = FMath::Atan2(OffsetB.Y, OffsetB.X);
		Prepared.AAngle = NormalizeRadians(Prepared.AbsoluteAAngle - ForwardAngle);
		Prepared.BAngle = NormalizeRadians(Prepared.AbsoluteBAngle - ForwardAngle);
		const double MinimumEndpointDistance = FMath::Sqrt(FMath::Min(
			Prepared.OffsetA.SizeSquared(),
			OffsetB.SizeSquared()));
		Prepared.AngularPadding = FMath::Max(
			FMath::Max(1.0e-12, Tolerances.RayParallelEpsilon),
			FMath::Atan2(
				Tolerances.PointOnEdgeEpsilon,
				FMath::Max(MinimumEndpointDistance, Tolerances.PointOnEdgeEpsilon)));
		Prepared.FractionEpsilon =
			Tolerances.PointOnEdgeEpsilon / FMath::Max(Prepared.Vector.Size(), 1.0);
		Prepared.StableId = Segment.StableId;
		Prepared.OriginDistanceSquared = DistanceSquaredToSegment(
			FVector2D::ZeroVector,
			Prepared.OffsetA,
			OffsetB);
		Prepared.bOriginOnSegment = Prepared.OriginDistanceSquared
			<= FMath::Square(Tolerances.PointOnEdgeEpsilon);
		return Prepared;
	}

	bool AddDirtyProjection(
		const FSightWeavePreparedSegment& Segment,
		const double EndpointEpsilon,
		FSightWeaveIncrementalSolveContext& Context)
	{
		const double SignedSpan = NormalizeRadians(Segment.BAngle - Segment.AAngle);
		const double Padding = Segment.AngularPadding + EndpointEpsilon + 1.0e-12;
		const double Start =
			(SignedSpan >= 0.0 ? Segment.AAngle : Segment.BAngle) - Padding;
		const double End = Start + FMath::Abs(SignedSpan) + 2.0 * Padding;
		for (int32 Wrap = -1; Wrap <= 1; ++Wrap)
		{
			const double Offset = static_cast<double>(Wrap) * SightWeaveTwoPi;
			const double ClippedStart = FMath::Max(-PI, Start + Offset);
			const double ClippedEnd = FMath::Min(PI, End + Offset);
			if (ClippedStart <= ClippedEnd)
			{
				if (Context.DirtySectorCount >= Context.DirtySectors.Num())
				{
					return false;
				}
				Context.DirtySectors[Context.DirtySectorCount++] = {
					ClippedStart,
					ClippedEnd };
			}
		}
		return true;
	}

	void SortAndMergeDirtySectors(FSightWeaveIncrementalSolveContext& Context)
	{
		for (int32 Index = 1; Index < Context.DirtySectorCount; ++Index)
		{
			int32 InsertIndex = Index;
			while (InsertIndex > 0
				&& Context.DirtySectors[InsertIndex].StartAngle
					< Context.DirtySectors[InsertIndex - 1].StartAngle)
			{
				Swap(Context.DirtySectors[InsertIndex], Context.DirtySectors[InsertIndex - 1]);
				--InsertIndex;
			}
		}

		int32 WriteIndex = 0;
		for (int32 ReadIndex = 0; ReadIndex < Context.DirtySectorCount; ++ReadIndex)
		{
			if (WriteIndex > 0
				&& Context.DirtySectors[ReadIndex].StartAngle
					<= Context.DirtySectors[WriteIndex - 1].EndAngle + 1.0e-12)
			{
				Context.DirtySectors[WriteIndex - 1].EndAngle = FMath::Max(
					Context.DirtySectors[WriteIndex - 1].EndAngle,
					Context.DirtySectors[ReadIndex].EndAngle);
			}
			else
			{
				Context.DirtySectors[WriteIndex++] = Context.DirtySectors[ReadIndex];
			}
		}
		Context.DirtySectorCount = WriteIndex;
	}

	bool IsAngleInDirtySector(
		const double Angle,
		const FSightWeaveIncrementalSolveContext& Context)
	{
		for (int32 Index = 0; Index < Context.DirtySectorCount; ++Index)
		{
			if (Angle >= Context.DirtySectors[Index].StartAngle - 1.0e-12
				&& Angle <= Context.DirtySectors[Index].EndAngle + 1.0e-12)
			{
				return true;
			}
		}
		return false;
	}

	bool ValidateIncrementalSeamGuards(
		const TArray<double>& CandidateAngles,
		const TArray<uint8>& ReusedRays,
		const FSightWeaveIncrementalSolveContext& Context)
	{
		check(CandidateAngles.Num() == ReusedRays.Num());
		for (int32 SectorIndex = 0; SectorIndex < Context.DirtySectorCount; ++SectorIndex)
		{
			const FSightWeaveDirtyAngularSector& Sector = Context.DirtySectors[SectorIndex];
			int32 Lower = 0;
			while (Lower < CandidateAngles.Num() && CandidateAngles[Lower] < Sector.StartAngle)
			{
				++Lower;
			}
			if (Lower > 0
				&& !IsAngleInDirtySector(CandidateAngles[Lower - 1], Context)
				&& ReusedRays[Lower - 1] == 0)
			{
				return false;
			}

			int32 Upper = Lower;
			while (Upper < CandidateAngles.Num() && CandidateAngles[Upper] <= Sector.EndAngle)
			{
				++Upper;
			}
			if (Upper < CandidateAngles.Num()
				&& !IsAngleInDirtySector(CandidateAngles[Upper], Context)
				&& ReusedRays[Upper] == 0)
			{
				return false;
			}
		}

		if (Context.DirtySectorCount > 0 && CandidateAngles.IsEmpty())
		{
			return false;
		}

		for (int32 SectorIndex = 0; SectorIndex < Context.DirtySectorCount; ++SectorIndex)
		{
			const FSightWeaveDirtyAngularSector& Sector = Context.DirtySectors[SectorIndex];
			if (Sector.StartAngle <= -PI + 1.0e-12)
			{
				const int32 CyclicLower = CandidateAngles.Num() - 1;
				if (!IsAngleInDirtySector(CandidateAngles[CyclicLower], Context)
					&& ReusedRays[CyclicLower] == 0)
				{
					return false;
				}
			}
			if (Sector.EndAngle >= PI - 1.0e-12)
			{
				if (!IsAngleInDirtySector(CandidateAngles[0], Context)
					&& ReusedRays[0] == 0)
				{
					return false;
				}
			}
		}
		return true;
	}

	bool PrepareIncrementalSolveContext(
		const FSightWeaveReferenceSolveInput& Input,
		const FSightWeaveReferenceSolveResult& PreviousResult,
		const FSightWeaveOptimizedSolveCache& Cache,
		const FSightWeaveIncrementalSectorRequest& Request,
		FSightWeaveIncrementalSolveContext& OutContext,
		FSightWeaveIncrementalSectorDiagnostics& OutDiagnostics)
	{
		OutContext.Diagnostics = &OutDiagnostics;
		if (Request.OldSegments.IsEmpty()
			|| Request.NewSegments.IsEmpty()
			|| Request.OldSegments.Num() != Request.NewSegments.Num())
		{
			OutDiagnostics.FallbackReason =
				ESightWeaveIncrementalSectorFallbackReason::InvalidRequest;
			return false;
		}
		if (PreviousResult.CandidateAnglesRadians.IsEmpty()
			|| PreviousResult.CandidateAnglesRadians.Num() != PreviousResult.CandidateDistances.Num()
			|| PreviousResult.CandidateAnglesRadians.Num()
				!= PreviousResult.CandidateBoundaryPoints.Num())
		{
			OutDiagnostics.FallbackReason =
				ESightWeaveIncrementalSectorFallbackReason::PreviousResultMissing;
			return false;
		}

		const FVector2D Origin(Input.Origin.X, Input.Origin.Y);
		const FVector2D Forward = Input.Forward.GetSafeNormal();
		if (!Cache.bInputInvariantInitialized
			|| Cache.Origin.X != Origin.X
			|| Cache.Origin.Y != Origin.Y
			|| Cache.Forward.X != Forward.X
			|| Cache.Forward.Y != Forward.Y
			|| Cache.FloorId != Input.FloorId
			|| Cache.HeightRange.ZMin != Input.HeightRange.ZMin
			|| Cache.HeightRange.ZMax != Input.HeightRange.ZMax
			|| !TolerancesExactlyMatch(Cache.Tolerances, Input.Tolerances)
			|| Cache.SegmentSlots.Num() != Input.Segments.Num())
		{
			OutDiagnostics.FallbackReason =
				ESightWeaveIncrementalSectorFallbackReason::CacheRevisionMismatch;
			return false;
		}

		const FSightWeaveSegment2D* ChangedOld = nullptr;
		const FSightWeaveSegment2D* ChangedNew = nullptr;
		for (const FSightWeaveSegment2D& OldSegment : Request.OldSegments)
		{
			const FSightWeaveSegment2D* NewSegment = nullptr;
			for (const FSightWeaveSegment2D& Candidate : Request.NewSegments)
			{
				if (Candidate.StableId == OldSegment.StableId)
				{
					NewSegment = &Candidate;
					break;
				}
			}
			if (!NewSegment)
			{
				OutDiagnostics.FallbackReason =
					ESightWeaveIncrementalSectorFallbackReason::MultipleChangedSegments;
				return false;
			}
			if (!SegmentPreparedKeyMatches(OldSegment, *NewSegment))
			{
				if (ChangedOld)
				{
					OutDiagnostics.FallbackReason =
						ESightWeaveIncrementalSectorFallbackReason::MultipleChangedSegments;
					return false;
				}
				ChangedOld = &OldSegment;
				ChangedNew = NewSegment;
			}
		}
		if (!ChangedOld || !ChangedNew
			|| ChangedOld->StableId <= 0
			|| ChangedOld->StableId != ChangedNew->StableId
			|| ChangedOld->FloorId != ChangedNew->FloorId
			|| ChangedOld->HeightRange.ZMin != ChangedNew->HeightRange.ZMin
			|| ChangedOld->HeightRange.ZMax != ChangedNew->HeightRange.ZMax)
		{
			OutDiagnostics.FallbackReason =
				ESightWeaveIncrementalSectorFallbackReason::InvalidRequest;
			return false;
		}
#if WITH_DEV_AUTOMATION_TESTS
		OutDiagnostics.DirtySegmentCount = 1;
#endif

		int32 ChangedSlotIndex = INDEX_NONE;
		int32 ChangedSlotCount = 0;
		for (int32 SegmentIndex = 0; SegmentIndex < Input.Segments.Num(); ++SegmentIndex)
		{
			const FSightWeaveOptimizedPreparedSegmentSlot& Slot = Cache.SegmentSlots[SegmentIndex];
			if (!Slot.Matches(Input.Segments[SegmentIndex]))
			{
				++ChangedSlotCount;
				ChangedSlotIndex = SegmentIndex;
			}
		}
		if (ChangedSlotCount != 1
			|| !Input.Segments.IsValidIndex(ChangedSlotIndex)
			|| Input.Segments[ChangedSlotIndex].StableId != ChangedNew->StableId
			|| !Cache.SegmentSlots[ChangedSlotIndex].Matches(*ChangedOld)
			|| !SegmentPreparedKeyMatches(Input.Segments[ChangedSlotIndex], *ChangedNew))
		{
			OutDiagnostics.FallbackReason = ChangedSlotCount > 1
				? ESightWeaveIncrementalSectorFallbackReason::MultipleChangedSegments
				: ESightWeaveIncrementalSectorFallbackReason::CacheRevisionMismatch;
			return false;
		}

		const FSightWeavePreparedSegment& OldPrepared =
			Cache.SegmentSlots[ChangedSlotIndex].Prepared;
		const FSightWeavePreparedSegment NewPrepared = PrepareIncrementalSegment(
			*ChangedNew,
			Origin,
			FMath::Atan2(Forward.Y, Forward.X),
			Input.Tolerances);
		const double NearDistance = FMath::Max(
			Input.Tolerances.PointOnEdgeEpsilon,
			Input.Tolerances.DuplicateVertexEpsilon) * 4.0;
		if (OldPrepared.bOriginOnSegment
			|| NewPrepared.bOriginOnSegment
			|| OldPrepared.OriginDistanceSquared <= FMath::Square(NearDistance)
			|| NewPrepared.OriginDistanceSquared <= FMath::Square(NearDistance))
		{
			OutDiagnostics.FallbackReason =
				ESightWeaveIncrementalSectorFallbackReason::SourceNearChangedSegment;
			return false;
		}

		const double EndpointEpsilon =
			FMath::DegreesToRadians(Input.Tolerances.EndpointAngularEpsilonDegrees);
		if (!AddDirtyProjection(OldPrepared, EndpointEpsilon, OutContext)
			|| !AddDirtyProjection(NewPrepared, EndpointEpsilon, OutContext))
		{
			OutDiagnostics.FallbackReason =
				ESightWeaveIncrementalSectorFallbackReason::MultipleChangedSegments;
			return false;
		}
		SortAndMergeDirtySectors(OutContext);
#if WITH_DEV_AUTOMATION_TESTS
		OutDiagnostics.DirtySectorCount = OutContext.DirtySectorCount;
#endif
		for (int32 Index = 0; Index < OutContext.DirtySectorCount; ++Index)
		{
			OutDiagnostics.DirtyRadians +=
				OutContext.DirtySectors[Index].EndAngle - OutContext.DirtySectors[Index].StartAngle;
		}
		if (OutContext.DirtySectorCount == 0 || OutDiagnostics.DirtyRadians > PI)
		{
			OutDiagnostics.FallbackReason =
				ESightWeaveIncrementalSectorFallbackReason::DirtySectorTooLarge;
			return false;
		}
		return true;
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
		const bool bEndpointPreparationComplete,
		const double AngularEpsilon,
		TArray<double>& OutAngles,
		TArray<FVector2D>& EndpointDirections,
		TArray<FVector2D>& OutDirections)
	{
		if (!bEndpointPreparationComplete && EndpointAngles.Num() >= 256)
		{
			EndpointSortBuffer.SetNumUninitialized(EndpointAngles.Num());
			RadixSort64<ERadixSortBufferState::IsInitialized>(
				EndpointAngles.GetData(),
				EndpointSortBuffer.GetData(),
				EndpointAngles.Num(),
				FSightWeaveDoubleRadixKey());
		}
		else if (!bEndpointPreparationComplete)
		{
			EndpointAngles.Sort();
		}
		if (!bEndpointPreparationComplete)
		{
			EndpointDirections.SetNumUninitialized(EndpointAngles.Num());
			for (int32 EndpointIndex = 0; EndpointIndex < EndpointAngles.Num(); ++EndpointIndex)
			{
				const double WorldAngle = ForwardAngle + EndpointAngles[EndpointIndex];
				EndpointDirections[EndpointIndex] = FVector2D(
					FMath::Cos(WorldAngle),
					FMath::Sin(WorldAngle));
			}
		}
		check(EndpointDirections.Num() == EndpointAngles.Num());

		const double Offsets[] = { -AngularEpsilon, 0.0, AngularEpsilon };
		const double OffsetCos = FMath::Cos(AngularEpsilon);
		const double OffsetSin = FMath::Sin(AngularEpsilon);
		int32 Starts[3] = {};
		int32 Positions[3] = {};
		double CurrentValues[3] = {};
		const bool bSingleWrapOffsets = AngularEpsilon < SightWeaveTwoPi;
		if (bSingleWrapOffsets && !EndpointAngles.IsEmpty())
		{
			auto LowerBoundAngle = [&EndpointAngles](const double Threshold)
			{
				int32 Lower = 0;
				int32 Upper = EndpointAngles.Num();
				while (Lower < Upper)
				{
					const int32 Middle = Lower + (Upper - Lower) / 2;
					if (EndpointAngles[Middle] < Threshold)
					{
						Lower = Middle + 1;
					}
					else
					{
						Upper = Middle;
					}
				}
				return Lower;
			};
			Starts[0] = LowerBoundAngle(-PI + AngularEpsilon);
			if (Starts[0] == EndpointAngles.Num())
			{
				Starts[0] = 0;
			}
			Starts[2] = LowerBoundAngle(PI - AngularEpsilon);
			if (Starts[2] == EndpointAngles.Num())
			{
				Starts[2] = 0;
			}
		}
		else
		{
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
			}
		}
		auto NormalizeEndpointOffset = [bSingleWrapOffsets](const double Value)
		{
			if (!bSingleWrapOffsets)
			{
				return NormalizeRadians(Value);
			}
			if (Value < -PI)
			{
				return Value + SightWeaveTwoPi;
			}
			if (Value >= PI)
			{
				return Value - SightWeaveTwoPi;
			}
			return Value;
		};
		for (int32 SequenceIndex = 0; SequenceIndex < 3; ++SequenceIndex)
		{
			if (!EndpointAngles.IsEmpty())
			{
				CurrentValues[SequenceIndex] = NormalizeEndpointOffset(
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
				CurrentValues[SequenceIndex] = NormalizeEndpointOffset(
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

	void SolveOptimizedPolygonIntoInternal(
		const FSightWeaveReferenceSolveInput& Input,
		FSightWeaveReferenceSolveResult& Result,
		FSightWeaveOptimizedSolveCache* PreparedCache,
		const bool bPreparedInputValidated = false,
		FSightWeaveIncrementalSolveContext* IncrementalContext = nullptr,
		const bool bPreserveVisionSolveDiagnostics = false)
	{
		FSightWeaveSolverFrameLease FrameLease;
		FSightWeaveSolverFrame& SolverFrame = FrameLease.Get();
		if (IncrementalContext)
		{
			Swap(Result.CandidateAnglesRadians, SolverFrame.PreviousCandidateAngles);
			Swap(Result.CandidateDistances, SolverFrame.PreviousCandidateDistances);
			Swap(Result.CandidateBoundaryPoints, SolverFrame.PreviousCandidateBoundaryPoints);
		}
		ResetOptimizedSolveResult(Result, bPreserveVisionSolveDiagnostics);
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

		TArray<FSightWeavePreparedSegment>& CandidateSegments = PreparedCache
			? PreparedCache->CandidateSegments
			: SolverFrame.CandidateSegments;
		TArray<FSightWeaveAngularInterval>& AngularIntervals = SolverFrame.AngularIntervals;
		TArray<FSightWeaveAngularInterval>& AngularIntervalSortBuffer = SolverFrame.AngularIntervalSortBuffer;
		TArray<FSightWeaveActiveInterval>& ActiveIntervals = SolverFrame.ActiveIntervals;
		TArray<double>& AngleSortBuffer = SolverFrame.AngleSortBuffer;
		TArray<double>& EndpointAngles = SolverFrame.EndpointAngles;
		TArray<double>& EndpointAngleSortBuffer = SolverFrame.EndpointAngleSortBuffer;
		TArray<FVector2D>& EndpointDirections = SolverFrame.EndpointDirections;
		TArray<FVector2D>& CandidateDirections = SolverFrame.CandidateDirections;
		TArray<double>& BoundaryAngles = SolverFrame.BoundaryAngles;
		TArray<double>& PreviousCandidateAngles = SolverFrame.PreviousCandidateAngles;
		TArray<double>& PreviousCandidateDistances = SolverFrame.PreviousCandidateDistances;
		TArray<FVector2D>& PreviousCandidateBoundaryPoints =
			SolverFrame.PreviousCandidateBoundaryPoints;
		TArray<uint8>& IncrementalReusedRays = SolverFrame.IncrementalReusedRays;

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
#if WITH_DEV_AUTOMATION_TESTS
		TOptional<FScopedVisionSolveSubstage> CandidateCollectionStage;
		CandidateCollectionStage.Emplace(
			Result.VisionSolveDiagnostics,
			ESightWeaveVisionSolveSubstage::CandidateSegmentCollection);
#endif
		bool bPreparedForwardChanged = false;
		bool bPreparedCandidatesMatch = false;
		bool bPreparedInputInvariantMatches = false;
		bool bPreparedSlotLayoutMatches = false;
		if (PreparedCache)
		{
			const FSightWeaveGeometryTolerances& Cached = PreparedCache->Tolerances;
			const bool bTolerancesMatch =
				Cached.AuthoringWeldEpsilon == Input.Tolerances.AuthoringWeldEpsilon
				&& Cached.ZeroLengthEpsilon == Input.Tolerances.ZeroLengthEpsilon
				&& Cached.RayParallelEpsilon == Input.Tolerances.RayParallelEpsilon
				&& Cached.EndpointAngularEpsilonDegrees == Input.Tolerances.EndpointAngularEpsilonDegrees
				&& Cached.PointOnEdgeEpsilon == Input.Tolerances.PointOnEdgeEpsilon
				&& Cached.PointInPolygonEpsilon == Input.Tolerances.PointInPolygonEpsilon
				&& Cached.DuplicateVertexEpsilon == Input.Tolerances.DuplicateVertexEpsilon
				&& Cached.HeightOverlapEpsilon == Input.Tolerances.HeightOverlapEpsilon
				&& Cached.RadialBoundarySteps == Input.Tolerances.RadialBoundarySteps;
			bPreparedInputInvariantMatches = PreparedCache->bInputInvariantInitialized
				&& PreparedCache->Origin.X == Origin.X
				&& PreparedCache->Origin.Y == Origin.Y
				&& PreparedCache->FloorId == Input.FloorId
				&& PreparedCache->HeightRange.ZMin == Input.HeightRange.ZMin
				&& PreparedCache->HeightRange.ZMax == Input.HeightRange.ZMax
				&& bTolerancesMatch;
			bPreparedForwardChanged = bPreparedInputInvariantMatches
				&& (PreparedCache->Forward.X != Forward.X || PreparedCache->Forward.Y != Forward.Y);
			bPreparedSlotLayoutMatches = bPreparedInputInvariantMatches
				&& PreparedCache->SegmentSlots.Num() == Input.Segments.Num();
			if (bPreparedSlotLayoutMatches)
			{
				bPreparedCandidatesMatch = bPreparedInputValidated;
				for (int32 SegmentIndex = 0;
					!bPreparedCandidatesMatch && SegmentIndex < Input.Segments.Num();
					++SegmentIndex)
				{
					const FSightWeaveSegment2D& Segment = Input.Segments[SegmentIndex];
					const FSightWeaveOptimizedPreparedSegmentSlot& Slot =
						PreparedCache->SegmentSlots[SegmentIndex];
					if (!Slot.Matches(Segment))
					{
						break;
					}
					bPreparedCandidatesMatch = SegmentIndex + 1 == Input.Segments.Num();
				}
			}
			if (!bPreparedInputInvariantMatches)
			{
				PreparedCache->bInputInvariantInitialized = true;
				PreparedCache->Origin = Origin;
				PreparedCache->FloorId = Input.FloorId;
				PreparedCache->HeightRange = Input.HeightRange;
				PreparedCache->Tolerances = Input.Tolerances;
				for (FSightWeaveOptimizedPreparedSegmentSlot& Slot : PreparedCache->SegmentSlots)
				{
					Slot.bHasKey = false;
				}
				PreparedCache->bAbsoluteEndpointEventsValid = false;
			}
			PreparedCache->Forward = Forward;
			if (PreparedCache->SegmentSlots.Num() != Input.Segments.Num())
			{
				PreparedCache->SegmentSlots.SetNum(Input.Segments.Num(), EAllowShrinking::No);
				for (FSightWeaveOptimizedPreparedSegmentSlot& Slot : PreparedCache->SegmentSlots)
				{
					Slot.bHasKey = false;
				}
				PreparedCache->bAbsoluteEndpointEventsValid = false;
			}
		}
		if (bPreparedCandidatesMatch)
		{
			if (bPreparedForwardChanged)
			{
				int32 CandidateIndex = 0;
				for (FSightWeaveOptimizedPreparedSegmentSlot& Slot : PreparedCache->SegmentSlots)
				{
					if (!Slot.bCandidate)
					{
						continue;
					}
					Slot.Prepared.AAngle = NormalizeRadians(
						Slot.Prepared.AbsoluteAAngle - ForwardAngle);
					Slot.Prepared.BAngle = NormalizeRadians(
						Slot.Prepared.AbsoluteBAngle - ForwardAngle);
					check(CandidateSegments.IsValidIndex(CandidateIndex));
					CandidateSegments[CandidateIndex].AAngle = Slot.Prepared.AAngle;
					CandidateSegments[CandidateIndex].BAngle = Slot.Prepared.BAngle;
					++CandidateIndex;
				}
				check(CandidateIndex == CandidateSegments.Num());
			}
		}
		else
		{
			CandidateSegments.Reset();
			CandidateSegments.Reserve(Input.Segments.Num());
			bool bPreparedGeometryChanged =
				PreparedCache && (!bPreparedInputInvariantMatches || !bPreparedSlotLayoutMatches);
			for (int32 SegmentIndex = 0; SegmentIndex < Input.Segments.Num(); ++SegmentIndex)
			{
				const FSightWeaveSegment2D& Segment = Input.Segments[SegmentIndex];
				const bool bCandidate = Segment.IsFinite()
					&& Segment.FloorId == Input.FloorId
					&& HeightRangesOverlap(
						Segment.HeightRange,
						Input.HeightRange,
						Input.Tolerances.HeightOverlapEpsilon);
				FSightWeaveOptimizedPreparedSegmentSlot* PreparedSlot = nullptr;
				bool bPreparedSlotMatches = false;
				if (PreparedCache)
				{
					PreparedSlot = &PreparedCache->SegmentSlots[SegmentIndex];
					bPreparedSlotMatches = bPreparedInputInvariantMatches
						&& bPreparedSlotLayoutMatches
						&& PreparedSlot->Matches(Segment)
						&& PreparedSlot->bCandidate == bCandidate;
					if (!bPreparedSlotMatches)
					{
						PreparedSlot->StoreKey(Segment);
						PreparedSlot->bCandidate = bCandidate;
						bPreparedGeometryChanged = true;
					}
				}
				if (!bCandidate)
				{
					continue;
				}
				if (bPreparedSlotMatches)
				{
					if (bPreparedForwardChanged)
					{
						PreparedSlot->Prepared.AAngle = NormalizeRadians(
							PreparedSlot->Prepared.AbsoluteAAngle - ForwardAngle);
						PreparedSlot->Prepared.BAngle = NormalizeRadians(
							PreparedSlot->Prepared.AbsoluteBAngle - ForwardAngle);
					}
					CandidateSegments.Add(PreparedSlot->Prepared);
					continue;
				}

				FSightWeavePreparedSegment* Prepared = nullptr;
				if (PreparedCache)
				{
					Prepared = &PreparedSlot->Prepared;
				}
				else
				{
					Prepared = &CandidateSegments.AddDefaulted_GetRef();
				}

				{
#if WITH_DEV_AUTOMATION_TESTS
					FScopedVisionSolveSubstage CandidateNormalizationStage(
						Result.VisionSolveDiagnostics,
						ESightWeaveVisionSolveSubstage::CandidateEventNormalization);
#endif
					Prepared->OffsetA = Segment.A - Origin;
					Prepared->Vector = Segment.B - Segment.A;
					const FVector2D OffsetB = Prepared->OffsetA + Prepared->Vector;
					Prepared->RayDistanceNumerator = Cross2D(Prepared->OffsetA, Prepared->Vector);
					Prepared->AbsoluteAAngle = FMath::Atan2(Prepared->OffsetA.Y, Prepared->OffsetA.X);
					Prepared->AbsoluteBAngle = FMath::Atan2(OffsetB.Y, OffsetB.X);
					Prepared->AAngle = NormalizeRadians(Prepared->AbsoluteAAngle - ForwardAngle);
					Prepared->BAngle = NormalizeRadians(Prepared->AbsoluteBAngle - ForwardAngle);
					const double MinimumEndpointDistance = FMath::Sqrt(FMath::Min(
						Prepared->OffsetA.SizeSquared(),
						OffsetB.SizeSquared()));
					Prepared->AngularPadding = FMath::Max(
						FMath::Max(1.0e-12, Input.Tolerances.RayParallelEpsilon),
						FMath::Atan2(
							Input.Tolerances.PointOnEdgeEpsilon,
							FMath::Max(MinimumEndpointDistance, Input.Tolerances.PointOnEdgeEpsilon)));
					Prepared->FractionEpsilon =
						Input.Tolerances.PointOnEdgeEpsilon / FMath::Max(Prepared->Vector.Size(), 1.0);
					Prepared->StableId = Segment.StableId;
					Prepared->OriginDistanceSquared = DistanceSquaredToSegment(
						FVector2D::ZeroVector,
						Prepared->OffsetA,
						OffsetB);
					Prepared->bOriginOnSegment = Prepared->OriginDistanceSquared
						<= FMath::Square(Input.Tolerances.PointOnEdgeEpsilon);
				}
				if (PreparedCache)
				{
					CandidateSegments.Add(*Prepared);
				}
			}
			if (PreparedCache && bPreparedGeometryChanged)
			{
				PreparedCache->bAbsoluteEndpointEventsValid = false;
			}
		}
#if WITH_DEV_AUTOMATION_TESTS
		CandidateCollectionStage.Reset();
		TOptional<FScopedVisionSolveSubstage> AngularEventStage;
		AngularEventStage.Emplace(
			Result.VisionSolveDiagnostics,
			ESightWeaveVisionSolveSubstage::AngularEventPreparation);
#endif
		if (bPureRadial)
		{
			EndpointAngles.Reserve(CandidateSegments.Num() * 2);
		}
		else
		{
			Result.CandidateAnglesRadians.Reserve(
				Result.CandidateAnglesRadians.Num() + CandidateSegments.Num() * 6);
		}
		const bool bPreparedRadialEndpointEvents = bPureRadial && PreparedCache;
		if (bPreparedRadialEndpointEvents)
		{
			if (!PreparedCache->bAbsoluteEndpointEventsValid)
			{
				TArray<double>& AbsoluteAngles = PreparedCache->SortedAbsoluteEndpointAngles;
				AbsoluteAngles.Reset();
				AbsoluteAngles.Reserve(CandidateSegments.Num() * 2);
				for (const FSightWeavePreparedSegment& Prepared : CandidateSegments)
				{
					AbsoluteAngles.Add(NormalizeRadians(Prepared.AbsoluteAAngle));
					AbsoluteAngles.Add(NormalizeRadians(Prepared.AbsoluteBAngle));
				}
				SortAndDeduplicateAnglesLinear(
					AbsoluteAngles,
					PreparedCache->AbsoluteEndpointAngleSortBuffer);
				PreparedCache->SortedAbsoluteEndpointDirections.SetNumUninitialized(
					AbsoluteAngles.Num(),
					EAllowShrinking::No);
				for (int32 EndpointIndex = 0; EndpointIndex < AbsoluteAngles.Num(); ++EndpointIndex)
				{
					const double AbsoluteAngle = AbsoluteAngles[EndpointIndex];
					PreparedCache->SortedAbsoluteEndpointDirections[EndpointIndex] = FVector2D(
						FMath::Cos(AbsoluteAngle),
						FMath::Sin(AbsoluteAngle));
				}
				PreparedCache->bAbsoluteEndpointEventsValid = true;
			}

			const TArray<double>& AbsoluteAngles = PreparedCache->SortedAbsoluteEndpointAngles;
			EndpointAngles.SetNumUninitialized(AbsoluteAngles.Num(), EAllowShrinking::No);
			EndpointDirections.SetNumUninitialized(AbsoluteAngles.Num(), EAllowShrinking::No);
			int32 FirstEndpointIndex = 0;
			if (!AbsoluteAngles.IsEmpty())
			{
				const double SeamAngle = NormalizeRadians(ForwardAngle - PI);
				int32 Lower = 0;
				int32 Upper = AbsoluteAngles.Num();
				while (Lower < Upper)
				{
					const int32 Middle = Lower + (Upper - Lower) / 2;
					if (AbsoluteAngles[Middle] < SeamAngle)
					{
						Lower = Middle + 1;
					}
					else
					{
						Upper = Middle;
					}
				}
				FirstEndpointIndex = Lower == AbsoluteAngles.Num() ? 0 : Lower;
			}
			for (int32 WriteIndex = 0; WriteIndex < AbsoluteAngles.Num(); ++WriteIndex)
			{
				int32 ReadIndex = FirstEndpointIndex + WriteIndex;
				if (ReadIndex >= AbsoluteAngles.Num())
				{
					ReadIndex -= AbsoluteAngles.Num();
				}
				EndpointAngles[WriteIndex] = NormalizeRadians(
					AbsoluteAngles[ReadIndex] - ForwardAngle);
				EndpointDirections[WriteIndex] =
					PreparedCache->SortedAbsoluteEndpointDirections[ReadIndex];
			}
		}
		for (const FSightWeavePreparedSegment& Prepared : CandidateSegments)
		{
			const double PreparedEndpointAngles[] = { Prepared.AAngle, Prepared.BAngle };
			for (int32 EndpointIndex = 0; EndpointIndex < 2; ++EndpointIndex)
			{
				const double EndpointAngle = PreparedEndpointAngles[EndpointIndex];
				if (bPureRadial)
				{
					if (!bPreparedRadialEndpointEvents)
					{
						EndpointAngles.Add(EndpointAngle);
					}
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
#if WITH_DEV_AUTOMATION_TESTS
		AngularEventStage.Reset();
#endif
		Result.CandidateSegmentCount = CandidateSegments.Num();
		Result.StageMetrics.CandidateFilterAndEndpointEventMicroseconds =
			(FPlatformTime::Seconds() - CandidateEventStartSeconds) * 1000000.0;

		const double EventSortStartSeconds = FPlatformTime::Seconds();
		int32 CandidateRayCount = 0;
		{
#if WITH_DEV_AUTOMATION_TESTS
			FScopedVisionSolveSubstage EventSortStage(
				Result.VisionSolveDiagnostics,
				ESightWeaveVisionSolveSubstage::EventSortOrLocalMerge);
#endif
			if (bPureRadial)
			{
				BuildRadialEndpointEvents(
				EndpointAngles,
				EndpointAngleSortBuffer,
				BoundaryAngles,
				ForwardAngle,
				bPreparedRadialEndpointEvents,
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
			CandidateRayCount = Result.CandidateAnglesRadians.Num();
			Result.Vertices.SetNumUninitialized(CandidateRayCount + (bFullCircle ? 0 : 1));
			Result.CandidateDistances.SetNumUninitialized(CandidateRayCount);
			Result.CandidateBoundaryPoints.SetNumUninitialized(CandidateRayCount);
			if (IncrementalContext)
			{
				IncrementalReusedRays.SetNumZeroed(CandidateRayCount, EAllowShrinking::No);
			}
		}
#if WITH_DEV_AUTOMATION_TESTS
		Result.VisionSolveDiagnostics.EventCount =
			BoundaryAngles.Num() + EndpointAngles.Num() + CandidateRayCount;
#endif
		Result.StageMetrics.EventSortDeduplicateMicroseconds =
			(FPlatformTime::Seconds() - EventSortStartSeconds) * 1000000.0;

		const double AccelerationStartSeconds = FPlatformTime::Seconds();
		{
#if WITH_DEV_AUTOMATION_TESTS
			FScopedVisionSolveSubstage ActiveInitializationStage(
				Result.VisionSolveDiagnostics,
				ESightWeaveVisionSolveSubstage::ActiveSegmentInitialization);
#endif
			BuildAngularIntervals(
				CandidateSegments,
				Origin,
				ForwardAngle,
				Input.Tolerances,
				AngularIntervals,
				AngularIntervalSortBuffer);
		}
		Result.StageMetrics.AccelerationBuildMicroseconds =
			(FPlatformTime::Seconds() - AccelerationStartSeconds) * 1000000.0;

		const double RayCastStartSeconds = FPlatformTime::Seconds();
#if WITH_DEV_AUTOMATION_TESTS
		TOptional<FScopedVisionSolveSubstage> RaySweepStage;
		RaySweepStage.Emplace(
			Result.VisionSolveDiagnostics,
			ESightWeaveVisionSolveSubstage::RaySweep);
#endif
		int32 NextIntervalIndex = 0;
		int32 PreviousRayIndex = 0;
		int32 DirtySectorIndex = 0;
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
			{
#if WITH_DEV_AUTOMATION_TESTS
				FScopedVisionSolveSubstage ActiveUpdateStage(
					Result.VisionSolveDiagnostics,
					ESightWeaveVisionSolveSubstage::ActiveSegmentUpdate,
					false);
#endif
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
			}
#if WITH_DEV_AUTOMATION_TESTS
			Result.VisionSolveDiagnostics.MaximumActiveSetCount = FMath::Max(
				Result.VisionSolveDiagnostics.MaximumActiveSetCount,
				ActiveIntervals.Num());
#endif

			bool bCanReusePrevious = false;
			{
#if WITH_DEV_AUTOMATION_TESTS
				FScopedVisionSolveSubstage ReuseLookupStage(
					Result.VisionSolveDiagnostics,
					ESightWeaveVisionSolveSubstage::RayReuseLookup,
					false);
#endif
				while (IncrementalContext
					&& PreviousRayIndex < PreviousCandidateAngles.Num()
					&& PreviousCandidateAngles[PreviousRayIndex] < RelativeAngle)
				{
					++PreviousRayIndex;
				}
				bool bAngleInDirtySector = false;
				if (IncrementalContext)
				{
#if WITH_DEV_AUTOMATION_TESTS
					++Result.VisionSolveDiagnostics.ReuseValidationCount;
#endif
					while (DirtySectorIndex < IncrementalContext->DirtySectorCount
						&& RelativeAngle
							> IncrementalContext->DirtySectors[DirtySectorIndex].EndAngle + 1.0e-12)
					{
						++DirtySectorIndex;
					}
					if (DirtySectorIndex < IncrementalContext->DirtySectorCount)
					{
						const FSightWeaveDirtyAngularSector& DirtySector =
							IncrementalContext->DirtySectors[DirtySectorIndex];
						bAngleInDirtySector = RelativeAngle >= DirtySector.StartAngle - 1.0e-12
							&& RelativeAngle <= DirtySector.EndAngle + 1.0e-12;
					}
				}
				bCanReusePrevious = IncrementalContext
					&& PreviousCandidateAngles.IsValidIndex(PreviousRayIndex)
					&& PreviousCandidateAngles[PreviousRayIndex] == RelativeAngle
					&& PreviousCandidateDistances.IsValidIndex(PreviousRayIndex)
					&& PreviousCandidateBoundaryPoints.IsValidIndex(PreviousRayIndex)
					&& !bAngleInDirtySector
					&& FMath::IsFinite(PreviousCandidateDistances[PreviousRayIndex])
					&& IsFiniteVector(PreviousCandidateBoundaryPoints[PreviousRayIndex]);
			}

			double ClosestDistance = 0.0;
			FVector2D Vertex2D = FVector2D::ZeroVector;
			if (bCanReusePrevious)
			{
				ClosestDistance = PreviousCandidateDistances[PreviousRayIndex];
				Vertex2D = PreviousCandidateBoundaryPoints[PreviousRayIndex];
				IncrementalReusedRays[RayIndex] = 1;
				++IncrementalContext->Diagnostics->ReusedRayCount;
			}
			else
			{
#if WITH_DEV_AUTOMATION_TESTS
				FScopedVisionSolveSubstage ChangedIntersectionStage(
					Result.VisionSolveDiagnostics,
					ESightWeaveVisionSolveSubstage::ChangedRayIntersection,
					false);
#endif
				const double MaximumDistance = bPureRadial
					? Input.Range
					: SourceRadiusAtRelativeAngle(Input, RelativeAngle);
				const FVector2D Direction = bPureRadial
					? CandidateDirections[RayIndex]
					: FVector2D(
						FMath::Cos(ForwardAngle + RelativeAngle),
						FMath::Sin(ForwardAngle + RelativeAngle));
				ClosestDistance = MaximumDistance;
				int64 ClosestStableId = MAX_int64;
				double MaximumOriginDistanceSquared = FMath::Square(
					ClosestDistance + Input.Tolerances.DuplicateVertexEpsilon);
				for (const FSightWeaveActiveInterval& ActiveInterval : ActiveIntervals)
				{
					if (ActiveInterval.OriginDistanceSquared > MaximumOriginDistanceSquared)
					{
						break;
					}
					const FSightWeavePreparedSegment& Segment =
						CandidateSegments[ActiveInterval.SegmentIndex];
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
					bool bCloser = false;
					bool bStableTie = false;
					{
#if WITH_DEV_AUTOMATION_TESTS
						FScopedVisionSolveSubstage StableTieBreakStage(
							Result.VisionSolveDiagnostics,
							ESightWeaveVisionSolveSubstage::StableIdTieBreak,
							false);
#endif
						bCloser = HitDistance
							< ClosestDistance - Input.Tolerances.DuplicateVertexEpsilon;
						bStableTie = FMath::Abs(HitDistance - ClosestDistance)
								<= Input.Tolerances.DuplicateVertexEpsilon
							&& Segment.StableId < ClosestStableId;
					}
#if WITH_DEV_AUTOMATION_TESTS
					Result.VisionSolveDiagnostics.StableIdTieBreakCount += bStableTie ? 1 : 0;
#endif
					if (bCloser || bStableTie)
					{
						ClosestDistance = HitDistance;
						ClosestStableId = Segment.StableId;
						MaximumOriginDistanceSquared = FMath::Square(
							ClosestDistance + Input.Tolerances.DuplicateVertexEpsilon);
					}
				}
				Vertex2D = Origin + Direction * ClosestDistance;
				if (IncrementalContext)
				{
					++IncrementalContext->Diagnostics->RebuiltRayCount;
				}
			}

			{
#if WITH_DEV_AUTOMATION_TESTS
				FScopedVisionSolveSubstage VertexEmissionStage(
					Result.VisionSolveDiagnostics,
					ESightWeaveVisionSolveSubstage::PolygonVertexEmission,
					false);
#endif
				Result.CandidateDistances[RayIndex] = ClosestDistance;
				Result.CandidateBoundaryPoints[RayIndex] = Vertex2D;
				const FVector Vertex(Vertex2D.X, Vertex2D.Y, Input.Origin.Z);
				if (VertexWriteIndex == 0
					|| FVector::DistSquared2D(Result.Vertices[VertexWriteIndex - 1], Vertex)
						> FMath::Square(Input.Tolerances.DuplicateVertexEpsilon))
				{
					Result.Vertices[VertexWriteIndex++] = Vertex;
				}
			}
		}
		Result.CastRayCount = CandidateRayCount;
		Result.Vertices.SetNum(VertexWriteIndex, EAllowShrinking::No);
#if WITH_DEV_AUTOMATION_TESTS
		Result.VisionSolveDiagnostics.PolygonVertexCount = VertexWriteIndex;
		RaySweepStage.Reset();
#endif
		Result.StageMetrics.RayCastMicroseconds =
			(FPlatformTime::Seconds() - RayCastStartSeconds) * 1000000.0;
		bool bReuseValidationSucceeded = true;
		if (IncrementalContext)
		{
#if WITH_DEV_AUTOMATION_TESTS
			FScopedVisionSolveSubstage ReuseValidationStage(
				Result.VisionSolveDiagnostics,
				ESightWeaveVisionSolveSubstage::ReusedRayValidation);
#endif
			bReuseValidationSucceeded = ValidateIncrementalSeamGuards(
				Result.CandidateAnglesRadians,
				IncrementalReusedRays,
				*IncrementalContext);
		}
		if (!bReuseValidationSucceeded)
		{
			IncrementalContext->Diagnostics->FallbackReason =
				ESightWeaveIncrementalSectorFallbackReason::SeamValidationFailed;
			Result.Error = TEXT("Incremental solve could not validate dirty-sector seam guards");
			Result.StageMetrics.TotalMicroseconds =
				(FPlatformTime::Seconds() - TotalStartSeconds) * 1000000.0;
			return;
		}

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
		bool bHasTopologyDegeneracy = false;
		{
#if WITH_DEV_AUTOMATION_TESTS
			FScopedVisionSolveSubstage TopologyStage(
				Result.VisionSolveDiagnostics,
				ESightWeaveVisionSolveSubstage::TopologyDegeneracyValidation);
#endif
			bHasTopologyDegeneracy =
				HasLocalVisibilityTopologyDegeneracy(Result.Vertices, Input.Tolerances);
		}
		if (bHasTopologyDegeneracy)
		{
			if (IncrementalContext)
			{
				IncrementalContext->Diagnostics->FallbackReason =
					ESightWeaveIncrementalSectorFallbackReason::TopologyValidationFailed;
			}
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
			+ BoundaryAngles.GetAllocatedSize()
			+ PreviousCandidateAngles.GetAllocatedSize()
			+ PreviousCandidateDistances.GetAllocatedSize()
			+ PreviousCandidateBoundaryPoints.GetAllocatedSize()
			+ IncrementalReusedRays.GetAllocatedSize();
		Result.bSucceeded = true;
		Result.StageMetrics.TotalMicroseconds =
			(FPlatformTime::Seconds() - TotalStartSeconds) * 1000000.0;
	}

	void SolveOptimizedPolygonInto(
		const FSightWeaveReferenceSolveInput& Input,
		FSightWeaveReferenceSolveResult& Result)
	{
		SolveOptimizedPolygonIntoInternal(Input, Result, nullptr);
	}

	void SolveOptimizedPolygonIntoCached(
		const FSightWeaveReferenceSolveInput& Input,
		FSightWeaveReferenceSolveResult& Result,
		FSightWeaveOptimizedSolveCache& Cache)
	{
		SolveOptimizedPolygonIntoInternal(Input, Result, &Cache);
	}

	void SolveOptimizedPolygonIntoValidatedCache(
		const FSightWeaveReferenceSolveInput& Input,
		FSightWeaveReferenceSolveResult& Result,
		FSightWeaveOptimizedSolveCache& Cache)
	{
		SolveOptimizedPolygonIntoInternal(Input, Result, &Cache, true);
	}

	void SolveOptimizedPolygonIntoIncrementalDynamicSector(
		const FSightWeaveReferenceSolveInput& Input,
		FSightWeaveReferenceSolveResult& Result,
		FSightWeaveOptimizedSolveCache& Cache,
		const FSightWeaveIncrementalSectorRequest& Request,
		FSightWeaveIncrementalSectorDiagnostics& OutDiagnostics)
	{
		OutDiagnostics = {};
		OutDiagnostics.bAttempted = true;
#if WITH_DEV_AUTOMATION_TESTS
		Result.VisionSolveDiagnostics = {};
#endif
		const double IncrementalStartSeconds = FPlatformTime::Seconds();
		FSightWeaveIncrementalSolveContext IncrementalContext;
		bool bContextPrepared = false;
		{
#if WITH_DEV_AUTOMATION_TESTS
			FScopedVisionSolveSubstage DirtySectorStage(
				Result.VisionSolveDiagnostics,
				ESightWeaveVisionSolveSubstage::DirtySectorDetermination);
#endif
			bContextPrepared = PrepareIncrementalSolveContext(
				Input,
				Result,
				Cache,
				Request,
				IncrementalContext,
				OutDiagnostics);
		}
#if WITH_DEV_AUTOMATION_TESTS
		Result.VisionSolveDiagnostics.DirtySegmentCount = OutDiagnostics.DirtySegmentCount;
		Result.VisionSolveDiagnostics.DirtySectorCount = OutDiagnostics.DirtySectorCount;
#endif
		if (bContextPrepared)
		{
			SolveOptimizedPolygonIntoInternal(
				Input,
				Result,
				&Cache,
				false,
				&IncrementalContext,
				true);
		}
		OutDiagnostics.IncrementalMicroseconds =
			(FPlatformTime::Seconds() - IncrementalStartSeconds) * 1000000.0;
		{
#if WITH_DEV_AUTOMATION_TESTS
			FScopedVisionSolveSubstage FallbackStage(
				Result.VisionSolveDiagnostics,
				ESightWeaveVisionSolveSubstage::FallbackDetection);
#endif
			if (bContextPrepared && Result.bSucceeded)
			{
				OutDiagnostics.bSucceeded = true;
				return;
			}

			if (OutDiagnostics.FallbackReason
				== ESightWeaveIncrementalSectorFallbackReason::None)
			{
				OutDiagnostics.FallbackReason =
					ESightWeaveIncrementalSectorFallbackReason::IncrementalSolveFailed;
			}
		}
		const double FallbackStartSeconds = FPlatformTime::Seconds();
		SolveOptimizedPolygonIntoInternal(Input, Result, &Cache, false, nullptr, true);
		OutDiagnostics.FullFallbackMicroseconds =
			(FPlatformTime::Seconds() - FallbackStartSeconds) * 1000000.0;
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
		void SetVisionSolveSubstageProbe(
			const FSightWeaveVisionSolveSubstageProbe Probe,
			const bool bEnableMicroTiming)
		{
			GVisionSolveSubstageProbe = Probe;
			GVisionSolveMicroTimingEnabled = Probe != nullptr && bEnableMicroTiming;
			if (!Probe)
			{
				GVisionSolveDiagnosticSourceId = 0;
			}
		}

		void SetVisionSolveDiagnosticSource(const int64 SourceId)
		{
			GVisionSolveDiagnosticSourceId = SourceId;
		}

		void EmitVisionSolveSubstage(
			const ESightWeaveVisionSolveSubstage Stage,
			const bool bBegin)
		{
			if (GVisionSolveSubstageProbe)
			{
				GVisionSolveSubstageProbe(Stage, GVisionSolveDiagnosticSourceId, bBegin);
			}
		}

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

		bool MeasureCachedOptimizedForwardSequence(
			const FSightWeaveReferenceSolveInput& BaseInput,
			const TConstArrayView<FVector2D> Forwards,
			TArray<double>& OutTotalMicroseconds,
			TArray<double>& OutCandidateMicroseconds,
			TArray<double>& OutSortMicroseconds,
			TArray<double>& OutAccelerationMicroseconds,
			FSightWeaveReferenceSolveResult& OutLastResult)
		{
			OutTotalMicroseconds.Reset();
			OutCandidateMicroseconds.Reset();
			OutSortMicroseconds.Reset();
			OutAccelerationMicroseconds.Reset();
			OutTotalMicroseconds.Reserve(Forwards.Num());
			OutCandidateMicroseconds.Reserve(Forwards.Num());
			OutSortMicroseconds.Reserve(Forwards.Num());
			OutAccelerationMicroseconds.Reserve(Forwards.Num());

			FSightWeaveReferenceSolveInput Input = BaseInput;
			FSightWeaveOptimizedSolveCache Cache;
			bool bAllSucceeded = true;
			for (const FVector2D& Forward : Forwards)
			{
				Input.Forward = Forward;
				SolveOptimizedPolygonIntoCached(Input, OutLastResult, Cache);
				bAllSucceeded &= OutLastResult.bSucceeded;
				OutTotalMicroseconds.Add(OutLastResult.StageMetrics.TotalMicroseconds);
				OutCandidateMicroseconds.Add(
					OutLastResult.StageMetrics.CandidateFilterAndEndpointEventMicroseconds);
				OutSortMicroseconds.Add(OutLastResult.StageMetrics.EventSortDeduplicateMicroseconds);
				OutAccelerationMicroseconds.Add(
					OutLastResult.StageMetrics.AccelerationBuildMicroseconds);
			}
			return bAllSucceeded;
		}
	}
#endif
}
