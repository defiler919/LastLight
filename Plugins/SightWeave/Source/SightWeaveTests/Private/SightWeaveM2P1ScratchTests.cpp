#if WITH_DEV_AUTOMATION_TESTS

#include "Async/ParallelFor.h"
#include "Misc/AutomationTest.h"
#include "SightWeaveGeometry.h"

namespace SightWeave::M2P1::ScratchTests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FSightWeaveReferenceSolveInput MakeDenseInput(const int32 SegmentCount)
	{
		const FSightWeaveFloorId FloorId(FName(TEXT("Ground")));
		FSightWeaveReferenceSolveInput Input;
		Input.Origin = FVector(37.0, -19.0, 100.0);
		Input.Forward = FVector2D(1.0, 0.0);
		Input.Shape = ESightWeaveSourceShape::Radial;
		Input.Range = 1200.0;
		Input.FloorId = FloorId;
		Input.HeightRange.ZMin = 0.0f;
		Input.HeightRange.ZMax = 300.0f;
		Input.Segments.Reserve(SegmentCount);

		constexpr int32 SegmentsPerRing = 128;
		const int32 RingCount = FMath::Max(1, FMath::DivideAndRoundUp(SegmentCount, SegmentsPerRing));
		for (int32 Index = 0; Index < SegmentCount; ++Index)
		{
			const int32 RingIndex = Index / SegmentsPerRing;
			const int32 IndexInRing = Index % SegmentsPerRing;
			const double Radius = 180.0 + 850.0 * (static_cast<double>(RingIndex) + 0.5) / RingCount;
			const double Angle = -PI + 2.0 * PI
				* (static_cast<double>(IndexInRing) + 0.25 * (RingIndex % 2))
				/ SegmentsPerRing;
			const FVector2D Center(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius);
			const FVector2D Offset(-FMath::Sin(Angle) * 1.5, FMath::Cos(Angle) * 1.5);

			FSightWeaveSegment2D& Segment = Input.Segments.AddDefaulted_GetRef();
			Segment.A = Center - Offset;
			Segment.B = Center + Offset;
			Segment.FloorId = FloorId;
			Segment.HeightRange.ZMin = 0.0f;
			Segment.HeightRange.ZMax = 300.0f;
			Segment.StableId = Index + 1;
		}
		return Input;
	}

	bool ResultsMatch(
		const FSightWeaveReferenceSolveResult& Expected,
		const FSightWeaveReferenceSolveResult& Actual)
	{
		if (Expected.bSucceeded != Actual.bSucceeded
			|| Expected.CandidateSegmentCount != Actual.CandidateSegmentCount
			|| Expected.CastRayCount != Actual.CastRayCount
			|| Expected.Vertices.Num() != Actual.Vertices.Num()
			|| Expected.CandidateAnglesRadians.Num() != Actual.CandidateAnglesRadians.Num()
			|| Expected.CandidateDistances.Num() != Actual.CandidateDistances.Num()
			|| Expected.CandidateBoundaryPoints.Num() != Actual.CandidateBoundaryPoints.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Expected.Vertices.Num(); ++Index)
		{
			if (!Expected.Vertices[Index].Equals(Actual.Vertices[Index], 1.0e-9))
			{
				return false;
			}
		}
		for (int32 Index = 0; Index < Expected.CandidateAnglesRadians.Num(); ++Index)
		{
			if (Expected.CandidateAnglesRadians[Index] != Actual.CandidateAnglesRadians[Index]
				|| Expected.CandidateDistances[Index] != Actual.CandidateDistances[Index]
				|| !Expected.CandidateBoundaryPoints[Index].Equals(
					Actual.CandidateBoundaryPoints[Index],
					1.0e-9))
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P1ScratchConcurrentIsolationTest,
	"SightWeave.M2P1.Scratch.ConcurrentIsolation",
	SightWeave::M2P1::ScratchTests::TestFlags)

bool FSightWeaveM2P1ScratchConcurrentIsolationTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P1::ScratchTests;
	const FSightWeaveReferenceSolveInput Input = MakeDenseInput(1024);
	FSightWeaveReferenceSolveResult Expected;
	SightWeave::Geometry::SolveOptimizedPolygonInto(Input, Expected);
	if (!TestTrue(TEXT("Reference optimized solve succeeds"), Expected.bSucceeded))
	{
		return false;
	}

	constexpr int32 WorkerCount = 8;
	constexpr int32 RepeatsPerWorker = 16;
	TArray<int32> FailureCounts;
	FailureCounts.Init(0, WorkerCount);
	ParallelFor(WorkerCount, [&](const int32 WorkerIndex)
	{
		FSightWeaveReferenceSolveResult Result;
		for (int32 Repeat = 0; Repeat < RepeatsPerWorker; ++Repeat)
		{
			SightWeave::Geometry::SolveOptimizedPolygonInto(Input, Result);
			if (!ResultsMatch(Expected, Result))
			{
				++FailureCounts[WorkerIndex];
			}
		}
	});

	int32 TotalFailures = 0;
	for (const int32 FailureCount : FailureCounts)
	{
		TotalFailures += FailureCount;
	}
	TestEqual(TEXT("Concurrent per-thread scratch results remain deterministic"), TotalFailures, 0);
	return TotalFailures == 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P1ScratchReentrancyTest,
	"SightWeave.M2P1.Scratch.Reentrancy",
	SightWeave::M2P1::ScratchTests::TestFlags)

bool FSightWeaveM2P1ScratchReentrancyTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("Eight nested leases use distinct persistent or stack-overflow frames"),
		SightWeave::Geometry::Testing::ExerciseOptimizedSolverScratchReentrancy(8));
	return true;
}

#endif
