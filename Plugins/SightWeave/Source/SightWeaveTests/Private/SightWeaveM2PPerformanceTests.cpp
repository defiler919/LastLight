#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "SightWeaveGeometry.h"
#include "SightWeaveWorldSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace SightWeave::M2P::PerformanceTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	const FSightWeaveFloorId Ground(FName(TEXT("Ground")));
	const FSightWeaveKnowledgeOwnerId Local(FName(TEXT("Local")));

	class FTestWorld
	{
	public:
		explicit FTestWorld(const TCHAR* BaseName)
		{
			const FName WorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), FName(BaseName));
			World = NewObject<UWorld>(GetTransientPackage(), WorldName, RF_Transient);
			if (!World || !GEngine) return;
			World->WorldType = EWorldType::Game;
			FWorldContext& Context = GEngine->CreateNewWorldContext(World->WorldType);
			Context.SetCurrentWorld(World);
			World->InitializeNewWorld(UWorld::InitializationValues()
				.AllowAudioPlayback(false)
				.CreatePhysicsScene(false)
				.RequiresHitProxies(false)
				.CreateNavigation(false)
				.CreateAISystem(false)
				.ShouldSimulatePhysics(false)
				.SetTransactional(false));
		}

		~FTestWorld()
		{
			if (World && GEngine)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(true);
			}
		}

		USightWeaveWorldSubsystem* GetSubsystem() const
		{
			return World ? World->GetSubsystem<USightWeaveWorldSubsystem>() : nullptr;
		}

	private:
		UWorld* World = nullptr;
	};

	struct FDistribution
	{
		double Median = 0.0;
		double P95 = 0.0;
		double P99 = 0.0;
		double Max = 0.0;
	};

	FDistribution Distribution(TArray<double> Samples)
	{
		FDistribution Result;
		if (Samples.IsEmpty()) return Result;
		Samples.Sort();
		auto Percentile = [&Samples](const double Fraction)
		{
			const int32 Index = FMath::Clamp(
				FMath::CeilToInt(Fraction * static_cast<double>(Samples.Num())) - 1,
				0,
				Samples.Num() - 1);
			return Samples[Index];
		};
		Result.Median = Percentile(0.50);
		Result.P95 = Percentile(0.95);
		Result.P99 = Percentile(0.99);
		Result.Max = Samples.Last();
		return Result;
	}

	FSightWeaveFloorDefinition Floor()
	{
		FSightWeaveFloorDefinition Result;
		Result.FloorId = Ground;
		Result.BoundsMin = FVector2D(-20000.0, -20000.0);
		Result.BoundsMax = FVector2D(20000.0, 20000.0);
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		return Result;
	}

	TArray<FSightWeaveSegment2D> MakeSegments(const int32 Count, const int32 Seed, const bool bDense)
	{
		FRandomStream Random(Seed);
		TArray<FSightWeaveSegment2D> Segments;
		Segments.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			// Extended dense fixtures are deliberately non-intersecting authored geometry.
			// Fixed rings retain thousands of relevant endpoint events without asking the
			// Reference oracle to accept crossing/invalid authoring input.
			constexpr int32 SegmentsPerRing = 128;
			const int32 RingCount = FMath::Max(1, FMath::DivideAndRoundUp(Count, SegmentsPerRing));
			const int32 RingIndex = Index / SegmentsPerRing;
			const int32 IndexInRing = Index % SegmentsPerRing;
			const double Radius = bDense
				? 180.0 + 850.0 * (static_cast<double>(RingIndex) + 0.5) / RingCount
				: Random.FRandRange(100.0f, 1050.0f);
			const double PolarAngle = bDense
				? -PI + 2.0 * PI * (static_cast<double>(IndexInRing) + 0.25 * (RingIndex % 2)) / SegmentsPerRing
				: Random.FRandRange(-PI, PI);
			const FVector2D Center(FMath::Cos(PolarAngle) * Radius, FMath::Sin(PolarAngle) * Radius);
			const double SegmentAngle = bDense
				? PolarAngle + PI * 0.5
				: Random.FRandRange(-PI, PI);
			const double HalfLength = bDense ? 1.5 : Random.FRandRange(12.0f, 70.0f);
			const FVector2D Offset(FMath::Cos(SegmentAngle) * HalfLength, FMath::Sin(SegmentAngle) * HalfLength);
			FSightWeaveSegment2D& Segment = Segments.AddDefaulted_GetRef();
			Segment.A = Center - Offset;
			Segment.B = Center + Offset;
			Segment.FloorId = Ground;
			Segment.HeightRange.ZMin = 0.0f;
			Segment.HeightRange.ZMax = 300.0f;
			Segment.StableId = Index + 1;
		}
		return Segments;
	}

	FSightWeaveReferenceSolveInput SolveInput(
		const TArray<FSightWeaveSegment2D>& Segments,
		const int32 SourceIndex,
		const int32 SourceCount,
		const ESightWeaveSourceShape Shape)
	{
		FSightWeaveReferenceSolveInput Input;
		const double SourceAngle = 2.0 * PI * static_cast<double>(SourceIndex) / FMath::Max(SourceCount, 1);
		Input.Origin = FVector(FMath::Cos(SourceAngle) * 75.0, FMath::Sin(SourceAngle) * 75.0, 100.0);
		Input.Forward = FVector2D(-FMath::Cos(SourceAngle), -FMath::Sin(SourceAngle));
		Input.Shape = Shape;
		Input.Range = 1200.0;
		Input.HalfAngleDegrees = Shape == ESightWeaveSourceShape::Radial ? 180.0 : 55.0;
		Input.NearAwarenessRadius = Shape == ESightWeaveSourceShape::CameraCone ? 75.0 : 0.0;
		Input.FloorId = Ground;
		Input.HeightRange.ZMin = 0.0f;
		Input.HeightRange.ZMax = 300.0f;
		Input.Segments = Segments;
		return Input;
	}

	struct FReferenceSample
	{
		double Total = 0.0;
		double Wall = 0.0;
		double Boundary = 0.0;
		double CandidateEvents = 0.0;
		double Sort = 0.0;
		double Acceleration = 0.0;
		double RayCast = 0.0;
		double PostProcess = 0.0;
		double Topology = 0.0;
		int64 Candidates = 0;
		int64 Rays = 0;
		int64 Vertices = 0;
		uint64 WorkingBytes = 0;
		uint64 TraversedNodes = 0;
		uint64 TestedSegments = 0;
		TArray<double> SolveTotals;
		bool bValid = true;
	};

	struct FSolverFixture
	{
		TArray<FSightWeaveReferenceSolveInput> Inputs;
		TArray<FSightWeaveReferenceSolveResult> Results;
	};

	FSolverFixture MakeSolverFixture(
		const int32 SourceCount,
		const int32 SegmentsPerSource,
		const ESightWeaveSourceShape Shape,
		const bool bDense,
		const int32 Seed)
	{
		FSolverFixture Fixture;
		Fixture.Inputs.Reserve(SourceCount);
		for (int32 SourceIndex = 0; SourceIndex < SourceCount; ++SourceIndex)
		{
			const TArray<FSightWeaveSegment2D> Segments = MakeSegments(
				SegmentsPerSource,
				Seed + SourceIndex * 7919,
				bDense);
			Fixture.Inputs.Add(SolveInput(Segments, SourceIndex, SourceCount, Shape));
		}
		Fixture.Results.SetNum(SourceCount);
		return Fixture;
	}

	FReferenceSample RunSolverSample(FSolverFixture& Fixture, const ESightWeaveSolverMode SolverMode)
	{
		FReferenceSample Sample;
		Sample.SolveTotals.Reserve(Fixture.Inputs.Num());
		const double WallStartSeconds = FPlatformTime::Seconds();
		for (int32 SourceIndex = 0; SourceIndex < Fixture.Inputs.Num(); ++SourceIndex)
		{
			FSightWeaveReferenceSolveResult& Solve = Fixture.Results[SourceIndex];
			SightWeave::Geometry::SolvePolygonInto(Fixture.Inputs[SourceIndex], SolverMode, Solve);
			Sample.bValid &= Solve.bSucceeded;
			Sample.Total += Solve.StageMetrics.TotalMicroseconds;
			Sample.SolveTotals.Add(Solve.StageMetrics.TotalMicroseconds);
			Sample.Boundary += Solve.StageMetrics.BoundaryEventMicroseconds;
			Sample.CandidateEvents += Solve.StageMetrics.CandidateFilterAndEndpointEventMicroseconds;
			Sample.Sort += Solve.StageMetrics.EventSortDeduplicateMicroseconds;
			Sample.Acceleration += Solve.StageMetrics.AccelerationBuildMicroseconds;
			Sample.RayCast += Solve.StageMetrics.RayCastMicroseconds;
			Sample.PostProcess += Solve.StageMetrics.PolygonPostProcessMicroseconds;
			Sample.Topology += Solve.StageMetrics.TopologyValidationMicroseconds;
			Sample.Candidates += Solve.CandidateSegmentCount;
			Sample.Rays += Solve.CastRayCount;
			Sample.Vertices += Solve.Vertices.Num();
			Sample.WorkingBytes += Solve.StageMetrics.WorkingSetAllocatedBytes;
			Sample.TraversedNodes += Solve.StageMetrics.TraversedAccelerationNodes;
			Sample.TestedSegments += Solve.StageMetrics.TestedSegments;
		}
		Sample.Wall = (FPlatformTime::Seconds() - WallStartSeconds) * 1000000.0;
		return Sample;
	}

	void LogSolverDistribution(
		FAutomationTestBase& Test,
		const TCHAR* SolverName,
		const ESightWeaveSolverMode SolverMode,
		const TCHAR* Name,
		const int32 SourceCount,
		const int32 SegmentsPerSource,
		const ESightWeaveSourceShape Shape,
		const bool bDense,
		const int32 Warmups,
		const int32 Repeats)
	{
		FSolverFixture Fixture = MakeSolverFixture(
			SourceCount,
			SegmentsPerSource,
			Shape,
			bDense,
			0x51A7E);
		for (int32 Warmup = 0; Warmup < Warmups; ++Warmup)
		{
			RunSolverSample(Fixture, SolverMode);
		}
		TArray<double> Total;
		TArray<double> Wall;
		TArray<double> SingleSolve;
		TArray<double> Boundary;
		TArray<double> CandidateEvents;
		TArray<double> Sort;
		TArray<double> Acceleration;
		TArray<double> RayCast;
		TArray<double> PostProcess;
		TArray<double> Topology;
		FReferenceSample Last;
		for (int32 Repeat = 0; Repeat < Repeats; ++Repeat)
		{
			Last = RunSolverSample(Fixture, SolverMode);
			Test.TestTrue(*FString::Printf(TEXT("%s %s sample succeeds"), Name, SolverName), Last.bValid);
			Total.Add(Last.Total);
			Wall.Add(Last.Wall);
			SingleSolve.Append(Last.SolveTotals);
			Boundary.Add(Last.Boundary);
			CandidateEvents.Add(Last.CandidateEvents);
			Sort.Add(Last.Sort);
			Acceleration.Add(Last.Acceleration);
			RayCast.Add(Last.RayCast);
			PostProcess.Add(Last.PostProcess);
			Topology.Add(Last.Topology);
		}
		const FDistribution TotalStats = Distribution(Total);
		const FDistribution WallStats = Distribution(Wall);
		const FDistribution SingleSolveStats = Distribution(SingleSolve);
		const FDistribution BoundaryStats = Distribution(Boundary);
		const FDistribution CandidateStats = Distribution(CandidateEvents);
		const FDistribution SortStats = Distribution(Sort);
		const FDistribution AccelerationStats = Distribution(Acceleration);
		const FDistribution RayStats = Distribution(RayCast);
		const FDistribution PostStats = Distribution(PostProcess);
		const FDistribution TopologyStats = Distribution(Topology);
		Test.AddInfo(FString::Printf(
			TEXT("M2P_SOLVER mode=%s name=%s sources=%d segments_per_source=%d relevant_segments_sum=%d shape=%d dense=%d repeats=%d candidates=%lld rays=%lld vertices=%lld working_bytes=%llu traversed_nodes=%llu tested_segments=%llu total_us=%.3f/%.3f/%.3f/%.3f single_solve_us=%.3f/%.3f/%.3f/%.3f sequential_wall_us=%.3f/%.3f/%.3f/%.3f boundary_us=%.3f/%.3f/%.3f/%.3f candidate_us=%.3f/%.3f/%.3f/%.3f sort_us=%.3f/%.3f/%.3f/%.3f acceleration_us=%.3f/%.3f/%.3f/%.3f ray_us=%.3f/%.3f/%.3f/%.3f post_us=%.3f/%.3f/%.3f/%.3f topology_us=%.3f/%.3f/%.3f/%.3f"),
			SolverName,
			Name,
			SourceCount,
			SegmentsPerSource,
			SourceCount * SegmentsPerSource,
			static_cast<int32>(Shape),
			bDense ? 1 : 0,
			Repeats,
			Last.Candidates,
			Last.Rays,
			Last.Vertices,
			Last.WorkingBytes,
			Last.TraversedNodes,
			Last.TestedSegments,
			TotalStats.Median, TotalStats.P95, TotalStats.P99, TotalStats.Max,
			SingleSolveStats.Median, SingleSolveStats.P95, SingleSolveStats.P99, SingleSolveStats.Max,
			WallStats.Median, WallStats.P95, WallStats.P99, WallStats.Max,
			BoundaryStats.Median, BoundaryStats.P95, BoundaryStats.P99, BoundaryStats.Max,
			CandidateStats.Median, CandidateStats.P95, CandidateStats.P99, CandidateStats.Max,
			SortStats.Median, SortStats.P95, SortStats.P99, SortStats.Max,
			AccelerationStats.Median, AccelerationStats.P95, AccelerationStats.P99, AccelerationStats.Max,
			RayStats.Median, RayStats.P95, RayStats.P99, RayStats.Max,
			PostStats.Median, PostStats.P95, PostStats.P99, PostStats.Max,
			TopologyStats.Median, TopologyStats.P95, TopologyStats.P99, TopologyStats.Max));
	}

	void LogReferenceDistribution(
		FAutomationTestBase& Test,
		const TCHAR* Name,
		const int32 SourceCount,
		const int32 SegmentsPerSource,
		const ESightWeaveSourceShape Shape,
		const bool bDense,
		const int32 Warmups,
		const int32 Repeats)
	{
		LogSolverDistribution(
			Test,
			TEXT("reference"),
			ESightWeaveSolverMode::Reference,
			Name,
			SourceCount,
			SegmentsPerSource,
			Shape,
			bDense,
			Warmups,
			Repeats);
	}

	void LogOptimizedDistribution(
		FAutomationTestBase& Test,
		const TCHAR* Name,
		const int32 SourceCount,
		const int32 SegmentsPerSource,
		const ESightWeaveSourceShape Shape,
		const bool bDense,
		const int32 Warmups,
		const int32 Repeats)
	{
		LogSolverDistribution(
			Test,
			TEXT("optimized"),
			ESightWeaveSolverMode::Optimized,
			Name,
			SourceCount,
			SegmentsPerSource,
			Shape,
			bDense,
			Warmups,
			Repeats);
	}

	template <typename CallbackType>
	FDistribution TimeOperation(const int32 Warmups, const int32 Repeats, CallbackType&& Callback)
	{
		for (int32 Index = 0; Index < Warmups; ++Index) Callback(Index);
		TArray<double> Samples;
		Samples.Reserve(Repeats);
		for (int32 Index = 0; Index < Repeats; ++Index)
		{
			const double Start = FPlatformTime::Seconds();
			Callback(Index);
			Samples.Add((FPlatformTime::Seconds() - Start) * 1000000.0);
		}
		return Distribution(MoveTemp(Samples));
	}

	void LogDistribution(FAutomationTestBase& Test, const TCHAR* Name, const FDistribution& Stats, const TCHAR* Extra = TEXT(""))
	{
		Test.AddInfo(FString::Printf(
			TEXT("M2P_BASELINE_RUNTIME name=%s median_us=%.3f p95_us=%.3f p99_us=%.3f max_us=%.3f %s"),
			Name, Stats.Median, Stats.P95, Stats.P99, Stats.Max, Extra));
	}

	FSightWeaveVisionSourceDescription Vision(
		const FVector Location,
		const ESightWeaveSourceShape Shape,
		const ESightWeaveIlluminationPolicy Policy)
	{
		FSightWeaveVisionSourceDescription Result;
		Result.KnowledgeOwnerId = Local;
		Result.FloorId = Ground;
		Result.Transform.SetLocation(Location);
		Result.Shape = Shape;
		Result.Range = 1200.0f;
		Result.HalfAngleDegrees = 55.0f;
		Result.NearAwarenessRadius = Shape == ESightWeaveSourceShape::CameraCone ? 75.0f : 0.0f;
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		Result.IlluminationPolicy = Policy;
		if (Policy == ESightWeaveIlluminationPolicy::RequiresLegalIllumination)
		{
			Result.Compatibility.AcceptedCapabilities = { FName(TEXT("Infrared")), FName(TEXT("Visible")) };
		}
		return Result;
	}

	FSightWeaveIlluminationSourceDescription Light(const FVector Location, const FName Capability)
	{
		FSightWeaveIlluminationSourceDescription Result;
		Result.KnowledgeOwnerId = Local;
		Result.FloorId = Ground;
		Result.Transform.SetLocation(Location);
		Result.Range = 1200.0f;
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		Result.EmittedCapabilities = { Capability };
		return Result;
	}

	uint64 QueryResultAllocatedBytes(TConstArrayView<FSightWeaveVisibilityQueryResult> Results)
	{
		uint64 Bytes = 0;
		for (const FSightWeaveVisibilityQueryResult& Result : Results)
		{
			Bytes += Result.ContributingVisionSources.GetAllocatedSize();
			Bytes += Result.ContributingIlluminationSources.GetAllocatedSize();
			Bytes += Result.ContributingSuppressions.GetAllocatedSize();
		}
		return Bytes;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2PReferenceStageBaselineTest,
	"SightWeave.M2P.Performance.Baseline.ReferenceStages",
	SightWeave::M2P::PerformanceTests::TestFlags)

bool FSightWeaveM2PReferenceStageBaselineTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P::PerformanceTests;
	const bool bExtended = FParse::Param(FCommandLine::Get(), TEXT("SightWeaveExtendedBenchmarks"));
	LogReferenceDistribution(*this, TEXT("typical_2x64_radial"), 2, 64, ESightWeaveSourceShape::Radial, false, 2, 11);
	LogReferenceDistribution(*this, TEXT("typical_8x64_radial"), 8, 64, ESightWeaveSourceShape::Radial, false, 2, 11);
	LogReferenceDistribution(*this, TEXT("typical_8x64_cone"), 8, 64, ESightWeaveSourceShape::DirectionalCone, false, 2, 11);
	if (bExtended)
	{
		LogReferenceDistribution(*this, TEXT("typical_8x256_radial"), 8, 256, ESightWeaveSourceShape::Radial, false, 1, 7);
		LogReferenceDistribution(*this, TEXT("dense_8x1024_radial"), 8, 1024, ESightWeaveSourceShape::Radial, true, 1, 5);
		LogReferenceDistribution(*this, TEXT("dense_8x512_total4096"), 8, 512, ESightWeaveSourceShape::Radial, true, 1, 5);
		LogReferenceDistribution(*this, TEXT("dense_8x4096_each"), 8, 4096, ESightWeaveSourceShape::Radial, true, 0, 3);
	}
	else
	{
		AddInfo(TEXT("Extended 256/1024/4096 workloads require -SightWeaveExtendedBenchmarks."));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2POptimizedStagePerformanceTest,
	"SightWeave.M2P.Performance.Optimized.SolverStages",
	SightWeave::M2P::PerformanceTests::TestFlags)

bool FSightWeaveM2POptimizedStagePerformanceTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P::PerformanceTests;
	const bool bExtended = FParse::Param(FCommandLine::Get(), TEXT("SightWeaveExtendedBenchmarks"));
	LogOptimizedDistribution(*this, TEXT("typical_2x64_radial"), 2, 64, ESightWeaveSourceShape::Radial, false, 4, 31);
	LogOptimizedDistribution(*this, TEXT("typical_8x64_radial"), 8, 64, ESightWeaveSourceShape::Radial, false, 4, 31);
	LogOptimizedDistribution(*this, TEXT("typical_8x64_cone"), 8, 64, ESightWeaveSourceShape::DirectionalCone, false, 4, 31);
	if (bExtended)
	{
		LogOptimizedDistribution(*this, TEXT("typical_8x256_radial"), 8, 256, ESightWeaveSourceShape::Radial, false, 2, 21);
		LogOptimizedDistribution(*this, TEXT("dense_8x1024_radial"), 8, 1024, ESightWeaveSourceShape::Radial, true, 2, 21);
		LogOptimizedDistribution(*this, TEXT("dense_8x512_total4096"), 8, 512, ESightWeaveSourceShape::Radial, true, 2, 21);
		LogOptimizedDistribution(*this, TEXT("dense_8x4096_each"), 8, 4096, ESightWeaveSourceShape::Radial, true, 1, 11);
	}
	else
	{
		AddInfo(TEXT("Extended 256/1024/4096 workloads require -SightWeaveExtendedBenchmarks."));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2PRuntimePipelineBaselineTest,
	"SightWeave.M2P.Performance.Baseline.RuntimePipeline",
	SightWeave::M2P::PerformanceTests::TestFlags)

bool FSightWeaveM2PRuntimePipelineBaselineTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P::PerformanceTests;
	FTestWorld World(TEXT("SightWeaveM2PRuntimeBaseline"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (!TestNotNull(TEXT("Subsystem exists"), Subsystem)
		|| !TestTrue(TEXT("Floor registers"), Subsystem->RegisterFloor(Floor(), nullptr)))
	{
		return true;
	}

	TArray<FSightWeaveSegment2D> StaticSegments = MakeSegments(64, 0x51A7E, false);
	for (FSightWeaveSegment2D& Segment : StaticSegments) Segment.StableId = 0;
	TestTrue(TEXT("Static fixture registers"), Subsystem->RegisterOccluder(StaticSegments, false, true, nullptr).IsValid());

	TArray<FSightWeaveVisionSourceHandle> VisionHandles;
	TArray<FSightWeaveVisionSourceDescription> VisionDescriptions;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const double Angle = 2.0 * PI * Index / 4.0;
		const ESightWeaveSourceShape Shape = Index % 2 == 0 ? ESightWeaveSourceShape::Radial : ESightWeaveSourceShape::CameraCone;
		const ESightWeaveIlluminationPolicy Policy = Index < 2
			? ESightWeaveIlluminationPolicy::BypassLegalIllumination
			: ESightWeaveIlluminationPolicy::RequiresLegalIllumination;
		VisionDescriptions.Add(Vision(FVector(FMath::Cos(Angle) * 100.0, FMath::Sin(Angle) * 100.0, 100.0), Shape, Policy));
		VisionHandles.Add(Subsystem->RegisterVisionSource(VisionDescriptions.Last(), nullptr));
	}
	Subsystem->RegisterIlluminationSource(Light(FVector(100.0, 0.0, 100.0), FName(TEXT("Visible"))), nullptr);
	Subsystem->RegisterIlluminationSource(Light(FVector(-100.0, 0.0, 100.0), FName(TEXT("Infrared"))), nullptr);

	TArray<FSightWeaveSegment2D> SpatialResults;
	const FDistribution Spatial = TimeOperation(5, 101, [&](int32)
	{
		Subsystem->QueryOccluderSegments(
			Ground,
			FBox2D(FVector2D(-1200.0, -1200.0), FVector2D(1200.0, 1200.0)),
			Floor().HeightRange,
			SpatialResults);
	});
	LogDistribution(*this, TEXT("spatial_query_64"), Spatial,
		*FString::Printf(TEXT("candidates=%d allocated_bytes=%llu"), SpatialResults.Num(), SpatialResults.GetAllocatedSize()));

	const FDistribution SnapshotPublish = TimeOperation(3, 31, [&](int32)
	{
		Subsystem->PublishSnapshot();
	});
	LogDistribution(*this, TEXT("snapshot_publish_copy_4v2l64s"), SnapshotPublish);

	FSightWeaveFrameSnapshot SnapshotCopy;
	const FDistribution SnapshotGetCopy = TimeOperation(3, 31, [&](int32)
	{
		SnapshotCopy = Subsystem->GetPublishedSnapshot();
	});
	LogDistribution(*this, TEXT("snapshot_public_value_copy_4v2l64s"), SnapshotGetCopy,
		*FString::Printf(TEXT("vision=%d illumination=%d segments=%d"),
			SnapshotCopy.VisionSources.Num(), SnapshotCopy.IlluminationSources.Num(), SnapshotCopy.OccluderSegments.Num()));

	FSightWeaveVisibilityQueryResult PointResult;
	const FDistribution PointQuery = TimeOperation(20, 501, [&](int32 Index)
	{
		const double Angle = 2.0 * PI * (Index % 64) / 64.0;
		PointResult = Subsystem->QueryEffectiveLiveAtLocation(
			Local,
			Ground,
			FVector(FMath::Cos(Angle) * 400.0, FMath::Sin(Angle) * 400.0, 100.0));
	});
	LogDistribution(*this, TEXT("authority_point_query"), PointQuery);

	TArray<FSightWeaveQueryRequest> Requests;
	Requests.Reserve(512);
	for (int32 Index = 0; Index < 512; ++Index)
	{
		const double Angle = 2.0 * PI * Index / 512.0;
		FSightWeaveQueryRequest& Request = Requests.AddDefaulted_GetRef();
		Request.KnowledgeOwnerId = Local;
		Request.FloorId = Ground;
		Request.SampleSet.Samples.Add(FVector(FMath::Cos(Angle) * 500.0, FMath::Sin(Angle) * 500.0, 100.0));
	}
	TArray<FSightWeaveVisibilityQueryResult> BatchResults;
	Subsystem->QueryBatch(Requests, BatchResults);
	const uint64 BatchOuterBytesBefore = BatchResults.GetAllocatedSize();
	const uint64 BatchInnerBytesBefore = QueryResultAllocatedBytes(BatchResults);
	const FDistribution Batch = TimeOperation(10, 101, [&](int32)
	{
		Subsystem->QueryBatch(Requests, BatchResults);
	});
	LogDistribution(*this, TEXT("authority_batch_512"), Batch,
		*FString::Printf(TEXT("outer_bytes=%llu inner_bytes=%llu steady_capacity_growth_bytes=%llu"),
			BatchResults.GetAllocatedSize(),
			QueryResultAllocatedBytes(BatchResults),
			(BatchResults.GetAllocatedSize() - BatchOuterBytesBefore)
				+ (QueryResultAllocatedBytes(BatchResults) - BatchInnerBytesBefore)));

	FSightWeaveSegment2D Door;
	Door.A = FVector2D(250.0, -100.0);
	Door.B = FVector2D(250.0, 100.0);
	Door.FloorId = Ground;
	Door.HeightRange.ZMin = 0.0f;
	Door.HeightRange.ZMax = 300.0f;
	const FSightWeaveOccluderHandle DoorHandle = Subsystem->RegisterOccluder({ Door }, true, true, nullptr);
	TArray<double> DoorPrepareSamples;
	TArray<double> DoorSpatialSamples;
	TArray<double> DoorDirtySamples;
	TArray<double> DoorPublicationSamples;
	TArray<double> DoorVisionRebuildSamples;
	TArray<double> DoorIlluminationRebuildSamples;
	TArray<double> DoorMaterializationSamples;
	for (int32 Warmup = 0; Warmup < 10; ++Warmup)
	{
		const double X = Warmup % 2 == 0 ? 250.0 : 850.0;
		Door.A.X = X;
		Door.B.X = X;
		Subsystem->UpdateOccluder(DoorHandle, { Door }, true, true);
	}
	TArray<double> DoorTotalSamples;
	DoorTotalSamples.Reserve(101);
	DoorPrepareSamples.Reserve(101);
	DoorSpatialSamples.Reserve(101);
	DoorDirtySamples.Reserve(101);
	DoorPublicationSamples.Reserve(101);
	DoorVisionRebuildSamples.Reserve(101);
	DoorIlluminationRebuildSamples.Reserve(101);
	DoorMaterializationSamples.Reserve(101);
	for (int32 SampleIndex = 0; SampleIndex < 101; ++SampleIndex)
	{
		const double X = SampleIndex % 2 == 0 ? 250.0 : 850.0;
		Door.A.X = X;
		Door.B.X = X;
		const double DoorStartSeconds = FPlatformTime::Seconds();
		Subsystem->UpdateOccluder(DoorHandle, { Door }, true, true);
		DoorTotalSamples.Add((FPlatformTime::Seconds() - DoorStartSeconds) * 1000000.0);
		const FSightWeaveDynamicUpdateStageMetrics& Stages = Subsystem->GetLastDynamicUpdateStageMetrics();
		DoorPrepareSamples.Add(Stages.PrepareAndCompareMicroseconds);
		DoorSpatialSamples.Add(Stages.SpatialIndexMicroseconds);
		DoorDirtySamples.Add(Stages.DirtyDiscoveryMicroseconds);
		DoorPublicationSamples.Add(Stages.PublicationMicroseconds);
		DoorVisionRebuildSamples.Add(Stages.VisionRebuildMicroseconds);
		DoorIlluminationRebuildSamples.Add(Stages.IlluminationRebuildMicroseconds);
		DoorMaterializationSamples.Add(Stages.SnapshotMaterializationMicroseconds);
	}
	const FDistribution DoorUpdate = Distribution(DoorTotalSamples);
	LogDistribution(*this, TEXT("dynamic_door_update_solve_publish"), DoorUpdate,
		*FString::Printf(TEXT("dynamic_updates=%lld"), Subsystem->GetSpatialIndexStats().DynamicUpdateCount));
	const FDistribution DoorPrepare = Distribution(DoorPrepareSamples);
	const FDistribution DoorSpatial = Distribution(DoorSpatialSamples);
	const FDistribution DoorDirty = Distribution(DoorDirtySamples);
	const FDistribution DoorPublication = Distribution(DoorPublicationSamples);
	const FDistribution DoorVisionRebuild = Distribution(DoorVisionRebuildSamples);
	const FDistribution DoorIlluminationRebuild = Distribution(DoorIlluminationRebuildSamples);
	const FDistribution DoorMaterialization = Distribution(DoorMaterializationSamples);
	AddInfo(FString::Printf(
		TEXT("M2P_DYNAMIC_STAGES prepare_us=%.3f/%.3f/%.3f/%.3f spatial_us=%.3f/%.3f/%.3f/%.3f dirty_us=%.3f/%.3f/%.3f/%.3f publication_us=%.3f/%.3f/%.3f/%.3f"),
		DoorPrepare.Median, DoorPrepare.P95, DoorPrepare.P99, DoorPrepare.Max,
		DoorSpatial.Median, DoorSpatial.P95, DoorSpatial.P99, DoorSpatial.Max,
		DoorDirty.Median, DoorDirty.P95, DoorDirty.P99, DoorDirty.Max,
		DoorPublication.Median, DoorPublication.P95, DoorPublication.P99, DoorPublication.Max));
	AddInfo(FString::Printf(
		TEXT("M2P_PUBLICATION_STAGES vision_rebuild_us=%.3f/%.3f/%.3f/%.3f illumination_rebuild_us=%.3f/%.3f/%.3f/%.3f materialization_us=%.3f/%.3f/%.3f/%.3f"),
		DoorVisionRebuild.Median, DoorVisionRebuild.P95, DoorVisionRebuild.P99, DoorVisionRebuild.Max,
		DoorIlluminationRebuild.Median, DoorIlluminationRebuild.P95, DoorIlluminationRebuild.P99, DoorIlluminationRebuild.Max,
		DoorMaterialization.Median, DoorMaterialization.P95, DoorMaterialization.P99, DoorMaterialization.Max));

	int32 SourceUpdateIndex = 0;
	const FDistribution SourceUpdate = TimeOperation(10, 101, [&](int32)
	{
		FSightWeaveVisionSourceDescription& Description = VisionDescriptions[0];
		Description.Transform.SetLocation(FVector(SourceUpdateIndex++ % 2 == 0 ? 0.0 : 5.0, 0.0, 100.0));
		Subsystem->UpdateVisionSource(VisionHandles[0], Description);
	});
	LogDistribution(*this, TEXT("source_transform_update_solve_publish"), SourceUpdate);

	const FSightWeaveRevision RevisionBeforeNoChange = Subsystem->GetRevision();
	const FDistribution NoChangeSourceUpdate = TimeOperation(5, 101, [&](int32)
	{
		Subsystem->UpdateVisionSource(VisionHandles[0], VisionDescriptions[0]);
	});
	LogDistribution(*this, TEXT("no_change_source_update"), NoChangeSourceUpdate,
		*FString::Printf(TEXT("revision_before=%lld revision_after=%lld"),
			RevisionBeforeNoChange.GetValue(),
			Subsystem->GetRevision().GetValue()));
	TestEqual(TEXT("No-change source update preserves revision"), Subsystem->GetRevision(), RevisionBeforeNoChange);

	TestTrue(TEXT("Runtime benchmark point query remains authoritative"), PointResult.bAuthoritative);
	TestEqual(TEXT("Runtime benchmark batch count"), BatchResults.Num(), 512);
	return true;
}

#endif
