#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SightWeaveGeometry.h"
#include "SightWeaveWorldSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace SightWeave::M2P2::PreparedEventIndexTests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	const FSightWeaveFloorId Ground(FName(TEXT("Ground")));
	const FSightWeaveFloorId Basement(FName(TEXT("Basement")));
	const FSightWeaveKnowledgeOwnerId Local(FName(TEXT("Local")));
	const FSightWeaveKnowledgeOwnerId Remote(FName(TEXT("Remote")));

	class FTestWorld
	{
	public:
		explicit FTestWorld(const TCHAR* BaseName)
		{
			const FName WorldName = MakeUniqueObjectName(
				GetTransientPackage(), UWorld::StaticClass(), FName(BaseName));
			World = NewObject<UWorld>(GetTransientPackage(), WorldName, RF_Transient);
			if (!World || !GEngine)
			{
				return;
			}
			World->WorldType = EWorldType::Game;
			FWorldContext& Context = GEngine->CreateNewWorldContext(World->WorldType);
			Context.SetCurrentWorld(World);
			World->InitializeNewWorld(UWorld::InitializationValues()
				.InitializeScenes(false)
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

	FSightWeaveFloorDefinition Floor()
	{
		FSightWeaveFloorDefinition Result;
		Result.FloorId = Ground;
		Result.BoundsMin = FVector2D(-5000.0, -5000.0);
		Result.BoundsMax = FVector2D(5000.0, 5000.0);
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		return Result;
	}

	FSightWeaveFloorDefinition InactiveBasementFloor()
	{
		FSightWeaveFloorDefinition Result = Floor();
		Result.FloorId = Basement;
		Result.bActiveForQueries = false;
		return Result;
	}

	TArray<FSightWeaveSegment2D> RoomSegments()
	{
		TArray<FSightWeaveSegment2D> Segments;
		const FVector2D Points[] = {
			FVector2D(-400.0, -400.0),
			FVector2D(400.0, -400.0),
			FVector2D(400.0, 400.0),
			FVector2D(-400.0, 400.0) };
		for (int32 Index = 0; Index < 4; ++Index)
		{
			FSightWeaveSegment2D& Segment = Segments.AddDefaulted_GetRef();
			Segment.A = Points[Index];
			Segment.B = Points[(Index + 1) % 4];
			Segment.FloorId = Ground;
			Segment.HeightRange.ZMin = 0.0f;
			Segment.HeightRange.ZMax = 300.0f;
		}
		return Segments;
	}

	FSightWeaveVisionSourceDescription VisionSource(
		const FVector& Location,
		const double Range = 800.0)
	{
		FSightWeaveVisionSourceDescription Result;
		Result.KnowledgeOwnerId = Local;
		Result.FloorId = Ground;
		Result.Transform = FTransform(FQuat::Identity, Location);
		Result.Shape = ESightWeaveSourceShape::Radial;
		Result.Range = Range;
		Result.HalfAngleDegrees = 180.0;
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		return Result;
	}

	FSightWeaveIlluminationSourceDescription IlluminationSource(const FVector& Location)
	{
		FSightWeaveIlluminationSourceDescription Result;
		Result.KnowledgeOwnerId = Local;
		Result.FloorId = Ground;
		Result.Transform = FTransform(FQuat::Identity, Location);
		Result.Shape = ESightWeaveSourceShape::Radial;
		Result.Range = 800.0;
		Result.HalfAngleDegrees = 180.0;
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		return Result;
	}

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
		if (Samples.IsEmpty())
		{
			return Result;
		}
		Samples.Sort();
		auto Percentile = [&Samples](const double Fraction)
		{
			return Samples[FMath::Clamp(
				FMath::CeilToInt(Fraction * Samples.Num()) - 1,
				0,
				Samples.Num() - 1)];
		};
		Result.Median = Percentile(0.50);
		Result.P95 = Percentile(0.95);
		Result.P99 = Percentile(0.99);
		Result.Max = Samples.Last();
		return Result;
	}

	FString RawSamples(TConstArrayView<double> Samples)
	{
		FString Result;
		for (int32 Index = 0; Index < Samples.Num(); ++Index)
		{
			if (Index > 0)
			{
				Result += TEXT(",");
			}
			Result += FString::Printf(TEXT("%.3f"), Samples[Index]);
		}
		return Result;
	}

	TArray<FSightWeaveSegment2D> MakeDenseSegments(const int32 Count)
	{
		TArray<FSightWeaveSegment2D> Segments;
		Segments.Reserve(Count);
		constexpr int32 SegmentsPerRing = 128;
		const int32 RingCount = FMath::Max(1, FMath::DivideAndRoundUp(Count, SegmentsPerRing));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const int32 RingIndex = Index / SegmentsPerRing;
			const int32 IndexInRing = Index % SegmentsPerRing;
			const double Radius =
				180.0 + 850.0 * (static_cast<double>(RingIndex) + 0.5) / RingCount;
			const double PolarAngle = -PI + 2.0 * PI
				* (static_cast<double>(IndexInRing) + 0.25 * (RingIndex % 2))
				/ SegmentsPerRing;
			const FVector2D Center(
				FMath::Cos(PolarAngle) * Radius,
				FMath::Sin(PolarAngle) * Radius);
			const FVector2D Offset(
				FMath::Cos(PolarAngle + PI * 0.5) * 1.5,
				FMath::Sin(PolarAngle + PI * 0.5) * 1.5);
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

	TArray<FSightWeaveSegment2D> DoorSegments(const double X)
	{
		TArray<FSightWeaveSegment2D> Segments;
		FSightWeaveSegment2D& Segment = Segments.AddDefaulted_GetRef();
		Segment.A = FVector2D(X, -100.0);
		Segment.B = FVector2D(X, 100.0);
		Segment.FloorId = Ground;
		Segment.HeightRange.ZMin = 0.0f;
		Segment.HeightRange.ZMax = 300.0f;
		return Segments;
	}

	const FSightWeaveVisionSnapshotEntry* FindVisionEntry(
		const FSightWeaveFrameSnapshot& Snapshot,
		const FSightWeaveVisionSourceHandle Handle)
	{
		return Snapshot.VisionSources.FindByPredicate(
			[Handle](const FSightWeaveVisionSnapshotEntry& Entry)
			{
				return Entry.Handle == Handle;
			});
	}

	const FSightWeaveIlluminationSnapshotEntry* FindIlluminationEntry(
		const FSightWeaveFrameSnapshot& Snapshot,
		const FSightWeaveIlluminationSourceHandle Handle)
	{
		return Snapshot.IlluminationSources.FindByPredicate(
			[Handle](const FSightWeaveIlluminationSnapshotEntry& Entry)
			{
				return Entry.Handle == Handle;
			});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P2PreparedEventIndex4096Test,
	"SightWeave.M2P2.Performance.PreparedEventIndex4096",
	SightWeave::M2P2::PreparedEventIndexTests::TestFlags)

bool FSightWeaveM2P2PreparedEventIndex4096Test::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P2::PreparedEventIndexTests;
	FSightWeaveReferenceSolveInput Input;
	Input.Origin = FVector(75.0, 0.0, 100.0);
	Input.Forward = FVector2D(-1.0, 0.0);
	Input.Shape = ESightWeaveSourceShape::Radial;
	Input.Range = 1200.0;
	Input.HalfAngleDegrees = 180.0;
	Input.FloorId = Ground;
	Input.HeightRange.ZMin = 0.0f;
	Input.HeightRange.ZMax = 300.0f;
	Input.Segments = MakeDenseSegments(4096);

	constexpr int32 Warmups = 3;
	constexpr int32 Repeats = 101;
	TArray<FVector2D> Forwards;
	Forwards.Reserve(Warmups + Repeats);
	for (int32 Index = 0; Index < Warmups + Repeats; ++Index)
	{
		const double Angle = FMath::DegreesToRadians(0.5 * static_cast<double>(Index));
		Forwards.Add(FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)));
	}

	TArray<double> Total;
	TArray<double> Candidate;
	TArray<double> Sort;
	TArray<double> Acceleration;
	TArray<double> RayCast;
	TArray<double> PostProcess;
	TArray<double> Topology;
	FSightWeaveReferenceSolveResult LastResult;
	const bool bSucceeded = USightWeaveWorldSubsystem::MeasurePreparedEventIndexForwardSequenceForTesting(
		Input,
		Forwards,
		Total,
		Candidate,
		Sort,
		Acceleration,
		RayCast,
		PostProcess,
		Topology,
		LastResult);
	AddInfo(FString::Printf(
		TEXT("M2P2_PREPARED_4096_WARMUP_RAW total_us=[%s] candidate_us=[%s] sort_us=[%s] acceleration_us=[%s] ray_us=[%s] post_us=[%s] topology_us=[%s]"),
		*RawSamples(TConstArrayView<double>(Total.GetData(), Warmups)),
		*RawSamples(TConstArrayView<double>(Candidate.GetData(), Warmups)),
		*RawSamples(TConstArrayView<double>(Sort.GetData(), Warmups)),
		*RawSamples(TConstArrayView<double>(Acceleration.GetData(), Warmups)),
		*RawSamples(TConstArrayView<double>(RayCast.GetData(), Warmups)),
		*RawSamples(TConstArrayView<double>(PostProcess.GetData(), Warmups)),
		*RawSamples(TConstArrayView<double>(Topology.GetData(), Warmups))));
	Total.RemoveAt(0, Warmups, EAllowShrinking::No);
	Candidate.RemoveAt(0, Warmups, EAllowShrinking::No);
	Sort.RemoveAt(0, Warmups, EAllowShrinking::No);
	Acceleration.RemoveAt(0, Warmups, EAllowShrinking::No);
	RayCast.RemoveAt(0, Warmups, EAllowShrinking::No);
	PostProcess.RemoveAt(0, Warmups, EAllowShrinking::No);
	Topology.RemoveAt(0, Warmups, EAllowShrinking::No);

	const FDistribution TotalStats = Distribution(Total);
	const FDistribution CandidateStats = Distribution(Candidate);
	const FDistribution SortStats = Distribution(Sort);
	const FDistribution AccelerationStats = Distribution(Acceleration);
	AddInfo(FString::Printf(
		TEXT("M2P2_PREPARED_4096 repeats=%d total_us=%.3f/%.3f/%.3f/%.3f candidate_us=%.3f/%.3f/%.3f/%.3f sort_us=%.3f/%.3f/%.3f/%.3f acceleration_us=%.3f/%.3f/%.3f/%.3f candidates=%d rays=%d vertices=%d"),
		Repeats,
		TotalStats.Median, TotalStats.P95, TotalStats.P99, TotalStats.Max,
		CandidateStats.Median, CandidateStats.P95, CandidateStats.P99, CandidateStats.Max,
		SortStats.Median, SortStats.P95, SortStats.P99, SortStats.Max,
		AccelerationStats.Median, AccelerationStats.P95, AccelerationStats.P99, AccelerationStats.Max,
		LastResult.CandidateSegmentCount,
		LastResult.CastRayCount,
		LastResult.Vertices.Num()));
	AddInfo(FString::Printf(
		TEXT("M2P2_PREPARED_4096_RAW total_us=[%s] candidate_us=[%s] sort_us=[%s] acceleration_us=[%s] ray_us=[%s] post_us=[%s] topology_us=[%s]"),
		*RawSamples(Total),
		*RawSamples(Candidate),
		*RawSamples(Sort),
		*RawSamples(Acceleration),
		*RawSamples(RayCast),
		*RawSamples(PostProcess),
		*RawSamples(Topology)));

	TestTrue(TEXT("All cached 4096 solves succeed"), bSucceeded);
	TestEqual(TEXT("Representative warmed sample count"), Total.Num(), Repeats);
	TestTrue(TEXT("4096/source median is below 1 ms"), TotalStats.Median < 1000.0);
	TestTrue(TEXT("4096/source p99 is below 2 ms"), TotalStats.P99 < 2000.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P5PreparedExactResultKeyTest,
	"SightWeave.M2P5.VisionTail.PreparedExactResultKey",
	SightWeave::M2P2::PreparedEventIndexTests::TestFlags)

bool FSightWeaveM2P5PreparedExactResultKeyTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P2::PreparedEventIndexTests;
	FSightWeaveReferenceSolveInput Baseline;
	Baseline.Origin = FVector(25.0, -10.0, 125.0);
	Baseline.Forward = FVector2D(0.8, 0.6);
	Baseline.Shape = ESightWeaveSourceShape::CameraCone;
	Baseline.Range = 900.0;
	Baseline.HalfAngleDegrees = 55.0;
	Baseline.NearAwarenessRadius = 25.0;
	Baseline.FloorId = Ground;
	Baseline.HeightRange.ZMin = 0.0f;
	Baseline.HeightRange.ZMax = 300.0f;
	Baseline.Segments = MakeDenseSegments(32);

	TestTrue(
		TEXT("Complete exact result key reuses a bitwise-identical solve"),
		USightWeaveWorldSubsystem::ExercisePreparedEventIndexExactResultReuseForTesting(
			Baseline,
			Baseline,
			true));

	FSightWeaveReferenceSolveInput RadialBaseline = Baseline;
	RadialBaseline.Shape = ESightWeaveSourceShape::Radial;
	RadialBaseline.NearAwarenessRadius = 0.0;
	FSightWeaveReferenceSolveInput RadialRotation = RadialBaseline;
	RadialRotation.Forward = FVector2D(-0.6, 0.8);
	TestTrue(
		TEXT("Radial forward changes reuse the canonical exact result"),
		USightWeaveWorldSubsystem::ExercisePreparedEventIndexExactResultReuseForTesting(
			RadialBaseline,
			RadialRotation,
			true));

	auto ExactKeyRejects = [this, &Baseline](
		const TCHAR* Label,
		TFunctionRef<void(FSightWeaveReferenceSolveInput&)> Mutate)
	{
		FSightWeaveReferenceSolveInput Candidate = Baseline;
		Mutate(Candidate);
		TestTrue(
			Label,
			USightWeaveWorldSubsystem::ExercisePreparedEventIndexExactResultReuseForTesting(
				Baseline,
				Candidate,
				false));
	};

	ExactKeyRejects(TEXT("Origin X mismatch fails closed"), [](auto& Input) { Input.Origin.X += 1.0; });
	ExactKeyRejects(TEXT("Origin Y mismatch fails closed"), [](auto& Input) { Input.Origin.Y += 1.0; });
	ExactKeyRejects(TEXT("Origin Z mismatch fails closed"), [](auto& Input) { Input.Origin.Z += 1.0; });
	ExactKeyRejects(TEXT("Forward mismatch fails closed"), [](auto& Input) { Input.Forward *= 0.5; });
	ExactKeyRejects(TEXT("Shape mismatch fails closed"), [](auto& Input) { Input.Shape = ESightWeaveSourceShape::Radial; });
	ExactKeyRejects(TEXT("Range mismatch fails closed"), [](auto& Input) { Input.Range += 1.0; });
	ExactKeyRejects(TEXT("Half angle mismatch fails closed"), [](auto& Input) { Input.HalfAngleDegrees += 1.0; });
	ExactKeyRejects(TEXT("Near awareness mismatch fails closed"), [](auto& Input) { Input.NearAwarenessRadius += 1.0; });
	ExactKeyRejects(TEXT("Floor mismatch fails closed"), [](auto& Input) { Input.FloorId = Basement; });
	ExactKeyRejects(TEXT("Height minimum mismatch fails closed"), [](auto& Input) { Input.HeightRange.ZMin += 1.0f; });
	ExactKeyRejects(TEXT("Height maximum mismatch fails closed"), [](auto& Input) { Input.HeightRange.ZMax -= 1.0f; });
	ExactKeyRejects(TEXT("Weld tolerance mismatch fails closed"), [](auto& Input) { Input.Tolerances.AuthoringWeldEpsilon += 0.01; });
	ExactKeyRejects(TEXT("Zero-length tolerance mismatch fails closed"), [](auto& Input) { Input.Tolerances.ZeroLengthEpsilon += 0.0001; });
	ExactKeyRejects(TEXT("Parallel tolerance mismatch fails closed"), [](auto& Input) { Input.Tolerances.RayParallelEpsilon *= 2.0; });
	ExactKeyRejects(TEXT("Angular tolerance mismatch fails closed"), [](auto& Input) { Input.Tolerances.EndpointAngularEpsilonDegrees += 0.0001; });
	ExactKeyRejects(TEXT("Point-on-edge tolerance mismatch fails closed"), [](auto& Input) { Input.Tolerances.PointOnEdgeEpsilon += 0.01; });
	ExactKeyRejects(TEXT("Point-in-polygon tolerance mismatch fails closed"), [](auto& Input) { Input.Tolerances.PointInPolygonEpsilon += 0.01; });
	ExactKeyRejects(TEXT("Duplicate-vertex tolerance mismatch fails closed"), [](auto& Input) { Input.Tolerances.DuplicateVertexEpsilon += 0.001; });
	ExactKeyRejects(TEXT("Height overlap tolerance mismatch fails closed"), [](auto& Input) { Input.Tolerances.HeightOverlapEpsilon += 0.001; });
	ExactKeyRejects(TEXT("Radial-step mismatch fails closed"), [](auto& Input) { Input.Tolerances.RadialBoundarySteps += 8; });
	ExactKeyRejects(TEXT("Segment A mismatch fails closed"), [](auto& Input) { Input.Segments[0].A.X += 1.0; });
	ExactKeyRejects(TEXT("Segment B mismatch fails closed"), [](auto& Input) { Input.Segments[0].B.Y += 1.0; });
	ExactKeyRejects(TEXT("Segment floor mismatch fails closed"), [](auto& Input) { Input.Segments[0].FloorId = Basement; });
	ExactKeyRejects(TEXT("Segment height mismatch fails closed"), [](auto& Input) { Input.Segments[0].HeightRange.ZMax -= 1.0f; });
	ExactKeyRejects(TEXT("Segment stable ID mismatch fails closed"), [](auto& Input) { ++Input.Segments[0].StableId; });
	ExactKeyRejects(TEXT("Segment count mismatch fails closed"), [](auto& Input) { Input.Segments.Pop(); });

	FSightWeaveReferenceSolveInput NonSemanticMetadata = Baseline;
	NonSemanticMetadata.Segments[0].bDynamic = !NonSemanticMetadata.Segments[0].bDynamic;
	TestTrue(
		TEXT("Non-solver dynamic metadata does not fragment exact results"),
		USightWeaveWorldSubsystem::ExercisePreparedEventIndexExactResultReuseForTesting(
			Baseline,
			NonSemanticMetadata,
			true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P2PreparedEventIndexSharingLifecycleTest,
	"SightWeave.M2P2.PreparedEventIndex.SharingLifecycle",
	SightWeave::M2P2::PreparedEventIndexTests::TestFlags)

bool FSightWeaveM2P2PreparedEventIndexSharingLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P2::PreparedEventIndexTests;
	FTestWorld World(TEXT("SightWeaveM2P2PreparedSharing"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (!TestNotNull(TEXT("Subsystem exists"), Subsystem)
		|| !TestTrue(TEXT("Floor registers"), Subsystem->RegisterFloor(Floor(), nullptr)))
	{
		return true;
	}
	const FSightWeaveOccluderHandle Room =
		Subsystem->RegisterOccluder(RoomSegments(), false, true, nullptr);
	if (!TestTrue(TEXT("Room registers"), Room.IsValid()))
	{
		return true;
	}

	FSightWeaveVisionSourceDescription Vision = VisionSource(FVector(0.0, 0.0, 100.0));
	Vision.HalfAngleDegrees = 55.0f;
	const FSightWeaveVisionSourceHandle VisionHandle =
		Subsystem->RegisterVisionSource(Vision, nullptr);
	TestTrue(TEXT("Vision registers"), VisionHandle.IsValid());
	const FSightWeavePreparedEventIndexStats AfterVision =
		Subsystem->GetPreparedEventIndexStats();
	TestEqual(TEXT("First origin is one miss"), AfterVision.MissCount, int64(1));
	TestEqual(TEXT("First origin has one live entry"), AfterVision.LiveEntryCount, 1);
	TestEqual(TEXT("First origin has one binding"), AfterVision.SourceBindingCount, 1);

	const FSightWeaveIlluminationSourceHandle IlluminationHandle =
		Subsystem->RegisterIlluminationSource(
			IlluminationSource(FVector(0.0, 0.0, 100.0)),
			nullptr);
	TestTrue(TEXT("Illumination registers"), IlluminationHandle.IsValid());
	const FSightWeavePreparedEventIndexStats AfterIllumination =
		Subsystem->GetPreparedEventIndexStats();
	TestEqual(TEXT("Compatible origin preparation is shared"), AfterIllumination.HitCount, int64(1));
	TestEqual(TEXT("Sharing retains one entry"), AfterIllumination.LiveEntryCount, 1);
	TestEqual(TEXT("Sharing records two source bindings"), AfterIllumination.SourceBindingCount, 2);
	const FSightWeaveFrameSnapshot CrossKindSnapshot = Subsystem->GetPublishedSnapshot();
	const FSightWeaveVisionSnapshotEntry* CrossKindVision =
		FindVisionEntry(CrossKindSnapshot, VisionHandle);
	const FSightWeaveIlluminationSnapshotEntry* CrossKindIllumination =
		FindIlluminationEntry(CrossKindSnapshot, IlluminationHandle);
	TestTrue(
		TEXT("Exact cross-kind geometry reuse preserves identical owned vertices"),
		CrossKindVision
			&& CrossKindIllumination
			&& CrossKindVision->Polygon.Vertices == CrossKindIllumination->Polygon.Vertices);
	TestTrue(
		TEXT("Cross-kind final polygon metadata remains source-owned"),
		CrossKindVision
			&& CrossKindIllumination
			&& CrossKindVision->Polygon.SourceHandle == VisionHandle
			&& CrossKindIllumination->Polygon.SourceHandle == IlluminationHandle
			&& CrossKindVision->Polygon.SourceRevision == CrossKindVision->SourceRevision
			&& CrossKindIllumination->Polygon.SourceRevision == CrossKindIllumination->SourceRevision);

	const FSightWeaveVisionSourceHandle ShortRangeHandle =
		Subsystem->RegisterVisionSource(VisionSource(FVector(0.0, 0.0, 100.0), 600.0), nullptr);
	TestTrue(TEXT("Short-range vision registers"), ShortRangeHandle.IsValid());
	const FSightWeavePreparedEventIndexStats AfterShortRange =
		Subsystem->GetPreparedEventIndexStats();
	TestEqual(TEXT("Range-isolated view shares identical exact candidates"), AfterShortRange.HitCount, int64(2));
	TestEqual(TEXT("Range-isolated view does not duplicate origin entry"), AfterShortRange.LiveEntryCount, 1);

	FSightWeaveVisionSourceDescription OtherHeight = VisionSource(FVector(0.0, 0.0, 100.0));
	OtherHeight.HeightRange.ZMin = 50.0f;
	OtherHeight.HeightRange.ZMax = 250.0f;
	const FSightWeaveVisionSourceHandle OtherHeightHandle =
		Subsystem->RegisterVisionSource(OtherHeight, nullptr);
	TestTrue(TEXT("Other-height vision registers"), OtherHeightHandle.IsValid());
	const FSightWeavePreparedEventIndexStats AfterOtherHeight =
		Subsystem->GetPreparedEventIndexStats();
	TestEqual(TEXT("Exact height isolation causes a miss"), AfterOtherHeight.MissCount, int64(2));
	TestEqual(TEXT("Exact height isolation owns another entry"), AfterOtherHeight.LiveEntryCount, 2);

	const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> HeldSnapshot =
		Subsystem->AcquirePublishedSnapshotForTesting();
	const FSightWeaveRevision HeldRevision = HeldSnapshot->Revision;
	const TArray<FVector> HeldVertices = HeldSnapshot->VisionSources[0].Polygon.Vertices;
	Vision.Transform.SetRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(45.0)));
	const int64 HitsBeforeRotation = Subsystem->GetPreparedEventIndexStats().HitCount;
	TestTrue(
		TEXT("Radial rotation updates"),
		Subsystem->UpdateVisionSourceTransform(VisionHandle, Vision.Transform));
	const FSightWeavePreparedEventIndexStats AfterRotation =
		Subsystem->GetPreparedEventIndexStats();
	TestEqual(TEXT("Rotation hits the exact origin entry"), AfterRotation.HitCount, HitsBeforeRotation + 1);
	TestEqual(TEXT("Held snapshot revision remains immutable"), HeldSnapshot->Revision, HeldRevision);
	TestTrue(
		TEXT("Held snapshot vertices remain immutable"),
		HeldSnapshot->VisionSources[0].Polygon.Vertices == HeldVertices);

	Vision.Transform.SetLocation(FVector(10.0, 0.0, 100.0));
	const int64 MissesBeforeTranslation = AfterRotation.MissCount;
	TestTrue(
		TEXT("Translation updates"),
		Subsystem->UpdateVisionSourceTransform(VisionHandle, Vision.Transform));
	const FSightWeavePreparedEventIndexStats AfterTranslation =
		Subsystem->GetPreparedEventIndexStats();
	TestEqual(TEXT("Exact origin change causes a miss"), AfterTranslation.MissCount, MissesBeforeTranslation + 1);
	TestEqual(TEXT("Shared old origin remains and translated origin is added"), AfterTranslation.LiveEntryCount, 3);
	TestTrue(TEXT("Prepared bytes are observable"), AfterTranslation.LiveAllocatedBytes > 0);
	TestTrue(
		TEXT("Prepared high water covers live bytes"),
		AfterTranslation.HighWaterAllocatedBytes >= AfterTranslation.LiveAllocatedBytes);

	TestTrue(TEXT("Vision unregisters"), Subsystem->UnregisterVisionSource(VisionHandle));
	TestEqual(
		TEXT("Unregister releases exactly one binding"),
		Subsystem->GetPreparedEventIndexStats().SourceBindingCount,
		AfterTranslation.SourceBindingCount - 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P2PreparedEventIndexBoundsEvictionTest,
	"SightWeave.M2P2.PreparedEventIndex.BoundsEviction",
	SightWeave::M2P2::PreparedEventIndexTests::TestFlags)

bool FSightWeaveM2P2PreparedEventIndexBoundsEvictionTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P2::PreparedEventIndexTests;
	FTestWorld World(TEXT("SightWeaveM2P2PreparedBounds"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (!TestNotNull(TEXT("Subsystem exists"), Subsystem)
		|| !TestTrue(TEXT("Floor registers"), Subsystem->RegisterFloor(Floor(), nullptr))
		|| !TestTrue(
			TEXT("One-slot index config applies before sources"),
			Subsystem->ConfigurePreparedEventIndexForTesting(1, 1024ll * 1024ll)))
	{
		return true;
	}
	TestTrue(
		TEXT("Room registers"),
		Subsystem->RegisterOccluder(RoomSegments(), false, true, nullptr).IsValid());

	FSightWeaveVisionSourceDescription A = VisionSource(FVector(-100.0, 0.0, 100.0));
	FSightWeaveVisionSourceDescription B = VisionSource(FVector(100.0, 0.0, 100.0));
	const FSightWeaveVisionSourceHandle AHandle = Subsystem->RegisterVisionSource(A, nullptr);
	const FSightWeaveVisionSourceHandle BHandle = Subsystem->RegisterVisionSource(B, nullptr);
	TestTrue(TEXT("First source registers"), AHandle.IsValid());
	TestTrue(TEXT("Second source registers through exact fallback"), BHandle.IsValid());
	const FSightWeavePreparedEventIndexStats AtCapacity =
		Subsystem->GetPreparedEventIndexStats();
	TestEqual(TEXT("One-slot limit remains hard"), AtCapacity.LiveEntryCount, 1);
	TestEqual(TEXT("Only retained source is bound"), AtCapacity.SourceBindingCount, 1);
	TestTrue(TEXT("Bound-slot pressure records capacity fallback"), AtCapacity.CapacityFallbackCount >= 1);

	TestTrue(TEXT("First source unregisters"), Subsystem->UnregisterVisionSource(AHandle));
	B.Transform.SetLocation(FVector(120.0, 0.0, 100.0));
	TestTrue(
		TEXT("Fallback source can acquire reclaimed slot"),
		Subsystem->UpdateVisionSourceTransform(BHandle, B.Transform));
	const FSightWeavePreparedEventIndexStats AfterReclaim =
		Subsystem->GetPreparedEventIndexStats();
	TestEqual(TEXT("Reclaim still respects one-slot limit"), AfterReclaim.LiveEntryCount, 1);
	TestEqual(TEXT("Reclaimed slot binds the remaining source"), AfterReclaim.SourceBindingCount, 1);
	TestTrue(TEXT("Deterministic unbound replacement records eviction"), AfterReclaim.EvictionCount >= 1);
	TestTrue(
		TEXT("Live prepared bytes respect configured cap"),
		AfterReclaim.LiveAllocatedBytes <= 1024ll * 1024ll);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P2TransformApiSemanticsTest,
	"SightWeave.M2P2.TransformAPI.Semantics",
	SightWeave::M2P2::PreparedEventIndexTests::TestFlags)

bool FSightWeaveM2P2TransformApiSemanticsTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P2::PreparedEventIndexTests;
	FTestWorld World(TEXT("SightWeaveM2P2TransformApi"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (!TestNotNull(TEXT("Subsystem exists"), Subsystem)
		|| !TestTrue(TEXT("Floor registers"), Subsystem->RegisterFloor(Floor(), nullptr)))
	{
		return true;
	}
	TestTrue(
		TEXT("Room registers"),
		Subsystem->RegisterOccluder(RoomSegments(), false, true, nullptr).IsValid());

	FSightWeaveVisionSourceDescription Vision = VisionSource(FVector(0.0, 0.0, 100.0));
	const FSightWeaveVisionSourceHandle VisionHandle =
		Subsystem->RegisterVisionSource(Vision, nullptr);
	TestTrue(TEXT("Vision registers"), VisionHandle.IsValid());
	const FSightWeaveRevision VisionRevision = Subsystem->GetRevision();
	const FSightWeavePreparedEventIndexStats VisionStats =
		Subsystem->GetPreparedEventIndexStats();
	TestTrue(
		TEXT("Exact no-change vision transform succeeds"),
		Subsystem->UpdateVisionSourceTransform(VisionHandle, Vision.Transform));
	TestEqual(TEXT("No-change vision transform preserves revision"), Subsystem->GetRevision(), VisionRevision);
	TestEqual(
		TEXT("No-change vision transform performs no index lookup"),
		Subsystem->GetPreparedEventIndexStats().HitCount,
		VisionStats.HitCount);

	FTransform InvalidTransform = Vision.Transform;
	InvalidTransform.SetLocation(FVector(std::numeric_limits<double>::quiet_NaN(), 0.0, 100.0));
	TestFalse(
		TEXT("NaN vision transform is rejected"),
		Subsystem->UpdateVisionSourceTransform(VisionHandle, InvalidTransform));
	TestFalse(
		TEXT("Invalid vision handle is rejected"),
		Subsystem->UpdateVisionSourceTransform(FSightWeaveVisionSourceHandle(), Vision.Transform));
	TestEqual(TEXT("Rejected vision transforms preserve revision"), Subsystem->GetRevision(), VisionRevision);

	Vision.Transform.SetLocation(FVector(20.0, 5.0, 100.0));
	Vision.Transform.SetRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(30.0)));
	TestTrue(
		TEXT("Valid vision transform succeeds"),
		Subsystem->UpdateVisionSourceTransform(VisionHandle, Vision.Transform));
	TestTrue(
		TEXT("Valid vision transform advances revision"),
		Subsystem->GetRevision().GetValue() > VisionRevision.GetValue());
	const FSightWeaveFrameSnapshot VisionSnapshot = Subsystem->GetPublishedSnapshot();
	const FSightWeaveVisionSnapshotEntry* VisionEntry = VisionSnapshot.VisionSources.FindByPredicate(
		[VisionHandle](const FSightWeaveVisionSnapshotEntry& Entry) { return Entry.Handle == VisionHandle; });
	TestTrue(
		TEXT("Vision transform API preserves non-transform metadata"),
		VisionEntry
			&& VisionEntry->Description.Transform.Equals(Vision.Transform, 0.0)
			&& VisionEntry->Description.KnowledgeOwnerId == Local
			&& VisionEntry->Description.FloorId == Ground
			&& VisionEntry->Description.Range == 800.0);

	FSightWeaveIlluminationSourceDescription Illumination =
		IlluminationSource(FVector(0.0, 0.0, 100.0));
	const FSightWeaveIlluminationSourceHandle IlluminationHandle =
		Subsystem->RegisterIlluminationSource(Illumination, nullptr);
	TestTrue(TEXT("Illumination registers"), IlluminationHandle.IsValid());
	const FSightWeaveRevision IlluminationRevision = Subsystem->GetRevision();
	TestTrue(
		TEXT("Exact no-change illumination transform succeeds"),
		Subsystem->UpdateIlluminationSourceTransform(IlluminationHandle, Illumination.Transform));
	TestEqual(
		TEXT("No-change illumination transform preserves revision"),
		Subsystem->GetRevision(),
		IlluminationRevision);
	TestFalse(
		TEXT("NaN illumination transform is rejected"),
		Subsystem->UpdateIlluminationSourceTransform(IlluminationHandle, InvalidTransform));
	TestFalse(
		TEXT("Invalid illumination handle is rejected"),
		Subsystem->UpdateIlluminationSourceTransform(
			FSightWeaveIlluminationSourceHandle(),
			Illumination.Transform));

	Illumination.Transform.SetLocation(FVector(-15.0, 10.0, 100.0));
	TestTrue(
		TEXT("Valid illumination transform succeeds"),
		Subsystem->UpdateIlluminationSourceTransform(IlluminationHandle, Illumination.Transform));
	const FSightWeaveFrameSnapshot IlluminationSnapshot = Subsystem->GetPublishedSnapshot();
	const FSightWeaveIlluminationSnapshotEntry* IlluminationEntry =
		IlluminationSnapshot.IlluminationSources.FindByPredicate(
			[IlluminationHandle](const FSightWeaveIlluminationSnapshotEntry& Entry)
			{
				return Entry.Handle == IlluminationHandle;
			});
	TestTrue(
		TEXT("Illumination transform API preserves non-transform metadata"),
		IlluminationEntry
			&& IlluminationEntry->Description.Transform.Equals(Illumination.Transform, 0.0)
			&& IlluminationEntry->Description.KnowledgeOwnerId == Local
			&& IlluminationEntry->Description.FloorId == Ground
			&& IlluminationEntry->Description.Range == 800.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P2PreparedEventIndexDynamicLifecycleTest,
	"SightWeave.M2P2.PreparedEventIndex.DynamicLifecycle",
	SightWeave::M2P2::PreparedEventIndexTests::TestFlags)

bool FSightWeaveM2P2PreparedEventIndexDynamicLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P2::PreparedEventIndexTests;
	FTestWorld World(TEXT("SightWeaveM2P2PreparedDynamicLifecycle"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (!TestNotNull(TEXT("Subsystem exists"), Subsystem)
		|| !TestTrue(TEXT("Floor registers"), Subsystem->RegisterFloor(Floor(), nullptr)))
	{
		return false;
	}

	const FSightWeaveOccluderHandle NearDoor =
		Subsystem->RegisterOccluder(DoorSegments(250.0), true, true, nullptr);
	const FSightWeaveOccluderHandle FarDoor =
		Subsystem->RegisterOccluder(DoorSegments(2450.0), true, true, nullptr);
	if (!TestTrue(TEXT("Near dynamic door registers"), NearDoor.IsValid())
		|| !TestTrue(TEXT("Far dynamic door registers"), FarDoor.IsValid()))
	{
		return false;
	}

	FSightWeaveVisionSourceDescription NearDescription =
		VisionSource(FVector(0.0, 0.0, 100.0), 800.0);
	FSightWeaveVisionSourceDescription FarDescription =
		VisionSource(FVector(2200.0, 0.0, 100.0), 800.0);
	const FSightWeaveVisionSourceHandle NearSource =
		Subsystem->RegisterVisionSource(NearDescription, nullptr);
	const FSightWeaveVisionSourceHandle FarSource =
		Subsystem->RegisterVisionSource(FarDescription, nullptr);
	if (!TestTrue(TEXT("Near source registers"), NearSource.IsValid())
		|| !TestTrue(TEXT("Far source registers"), FarSource.IsValid()))
	{
		return false;
	}

	const FSightWeavePreparedEventIndexStats InitialStats =
		Subsystem->GetPreparedEventIndexStats();
	TestEqual(TEXT("Two local preparations are retained"), InitialStats.LiveEntryCount, 2);
	TestEqual(TEXT("Two source bindings are retained"), InitialStats.SourceBindingCount, 2);
	const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> Held =
		Subsystem->AcquirePublishedSnapshotForTesting();
	if (!TestTrue(TEXT("Held snapshot is acquired"), Held.IsValid()))
	{
		return false;
	}
	const FSightWeaveVisionSnapshotEntry* HeldNear = FindVisionEntry(*Held, NearSource);
	const FSightWeaveVisionSnapshotEntry* HeldFar = FindVisionEntry(*Held, FarSource);
	if (!TestNotNull(TEXT("Held near entry exists"), HeldNear)
		|| !TestNotNull(TEXT("Held far entry exists"), HeldFar))
	{
		return false;
	}
	const FSightWeaveRevision HeldRevision = Held->Revision;
	const FSightWeaveRevision HeldFarSourceRevision = HeldFar->SourceRevision;
	const TArray<FVector> HeldNearVertices = HeldNear->Polygon.Vertices;
	const TArray<FVector> HeldFarVertices = HeldFar->Polygon.Vertices;

	int64 ExpectedMisses = InitialStats.MissCount;
	TestTrue(
		TEXT("Near door changes synchronously"),
		Subsystem->UpdateOccluder(NearDoor, DoorSegments(350.0), true, true));
	++ExpectedMisses;
	FSightWeavePreparedEventIndexStats Stats = Subsystem->GetPreparedEventIndexStats();
	TestEqual(TEXT("Near door rebuilds exactly one prepared origin"), Stats.MissCount, ExpectedMisses);
	TestEqual(TEXT("Local update retains the prior and new exact states"), Stats.LiveEntryCount, 3);
	const FSightWeaveFrameSnapshot AfterNearDoor = Subsystem->GetPublishedSnapshot();
	const FSightWeaveVisionSnapshotEntry* CurrentFar = FindVisionEntry(AfterNearDoor, FarSource);
	TestTrue(
		TEXT("Far source is not rebuilt by near-door locality"),
		CurrentFar
			&& CurrentFar->SourceRevision == HeldFarSourceRevision
			&& CurrentFar->Polygon.Vertices == HeldFarVertices);

	int64 ExpectedExactResultHits = Stats.ExactResultHitCount;
	for (int32 UpdateIndex = 0; UpdateIndex < 8; ++UpdateIndex)
	{
		const double DoorX = UpdateIndex % 2 == 0 ? 300.0 : 350.0;
		TestTrue(
			TEXT("Rapid near-door update succeeds"),
			Subsystem->UpdateOccluder(NearDoor, DoorSegments(DoorX), true, true));
		if (UpdateIndex == 0)
		{
			++ExpectedMisses;
		}
		else
		{
			++ExpectedExactResultHits;
		}
		Stats = Subsystem->GetPreparedEventIndexStats();
		TestEqual(TEXT("Only the first unseen door state rebuilds one origin"), Stats.MissCount, ExpectedMisses);
		TestEqual(TEXT("Resident two-state replay records exact-result hits"),
			Stats.ExactResultHitCount, ExpectedExactResultHits);
		TestEqual(TEXT("Rapid door states remain resident within the hard index bound"),
			Stats.LiveEntryCount, 4);
	}

	const FSightWeaveFrameSnapshot BeforeFarDoor = Subsystem->GetPublishedSnapshot();
	const FSightWeaveVisionSnapshotEntry* NearBeforeFarDoor =
		FindVisionEntry(BeforeFarDoor, NearSource);
	const FSightWeaveRevision NearRevisionBeforeFarDoor = NearBeforeFarDoor
		? NearBeforeFarDoor->SourceRevision
		: FSightWeaveRevision();
	TestTrue(
		TEXT("Far door changes synchronously"),
		Subsystem->UpdateOccluder(FarDoor, DoorSegments(2550.0), true, true));
	++ExpectedMisses;
	Stats = Subsystem->GetPreparedEventIndexStats();
	TestEqual(TEXT("Far door rebuilds exactly one prepared origin"), Stats.MissCount, ExpectedMisses);
	const FSightWeaveVisionSnapshotEntry* NearAfterFarDoor =
		nullptr;
	const FSightWeaveFrameSnapshot AfterFarDoorSnapshot = Subsystem->GetPublishedSnapshot();
	NearAfterFarDoor = FindVisionEntry(AfterFarDoorSnapshot, NearSource);
	TestTrue(
		TEXT("Near source is not rebuilt by far-door locality"),
		NearAfterFarDoor && NearAfterFarDoor->SourceRevision == NearRevisionBeforeFarDoor);

	TestTrue(
		TEXT("Door changes before simultaneous source motion"),
		Subsystem->UpdateOccluder(NearDoor, DoorSegments(325.0), true, true));
	++ExpectedMisses;
	NearDescription.Transform.SetLocation(FVector(10.0, 0.0, 100.0));
	TestTrue(
		TEXT("Source motion publishes immediately after door change"),
		Subsystem->UpdateVisionSourceTransform(NearSource, NearDescription.Transform));
	++ExpectedMisses;
	Stats = Subsystem->GetPreparedEventIndexStats();
	TestEqual(TEXT("Door plus motion performs two exact rebuilds"), Stats.MissCount, ExpectedMisses);
	const FSightWeaveFrameSnapshot AfterDoorAndMotion = Subsystem->GetPublishedSnapshot();
	const FSightWeaveVisionSnapshotEntry* MovedNear =
		FindVisionEntry(AfterDoorAndMotion, NearSource);
	TestTrue(
		TEXT("Latest publication contains moved source"),
		MovedNear && MovedNear->Description.Transform.Equals(NearDescription.Transform, 0.0));

	TestTrue(TEXT("Near source unregisters"), Subsystem->UnregisterVisionSource(NearSource));
	TestFalse(TEXT("Deleted source handle is immediately invalid"), Subsystem->IsVisionSourceHandleValid(NearSource));
	TestEqual(
		TEXT("Held revision survives source deletion"),
		Held->Revision,
		HeldRevision);
	TestTrue(
		TEXT("Held source polygon survives source deletion"),
		FindVisionEntry(*Held, NearSource)
			&& FindVisionEntry(*Held, NearSource)->Polygon.Vertices == HeldNearVertices);

	const int64 InvalidationsBeforeDelete = Stats.InvalidatedEntryCount;
	TestTrue(TEXT("Near occluder unregisters"), Subsystem->UnregisterOccluder(NearDoor));
	Stats = Subsystem->GetPreparedEventIndexStats();
	TestTrue(
		TEXT("Occluder deletion invalidates retained prepared entries"),
		Stats.InvalidatedEntryCount > InvalidationsBeforeDelete);
	TestEqual(TEXT("Unneeded prepared storage is reclaimed"), Stats.LiveEntryCount, 0);
	TestEqual(TEXT("Unneeded prepared bytes are reclaimed"), Stats.LiveAllocatedBytes, int64(0));

	FarDescription.Transform.SetRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(15.0)));
	TestTrue(
		TEXT("Remaining source rebuilds safely after global invalidation"),
		Subsystem->UpdateVisionSourceTransform(FarSource, FarDescription.Transform));
	TestEqual(
		TEXT("Remaining source reacquires one prepared entry"),
		Subsystem->GetPreparedEventIndexStats().LiveEntryCount,
		1);
	TestTrue(TEXT("Far occluder unregisters"), Subsystem->UnregisterOccluder(FarDoor));
	const FSightWeaveFrameSnapshot AfterFarDelete = Subsystem->GetPublishedSnapshot();
	TestTrue(
		TEXT("Remaining source stays published after occluder deletion"),
		FindVisionEntry(AfterFarDelete, FarSource) != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P2PreparedEventIndexIsolationDeterminismTest,
	"SightWeave.M2P2.PreparedEventIndex.IsolationColdWarmDeterminism",
	SightWeave::M2P2::PreparedEventIndexTests::TestFlags)

bool FSightWeaveM2P2PreparedEventIndexIsolationDeterminismTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P2::PreparedEventIndexTests;
	FTestWorld World(TEXT("SightWeaveM2P2PreparedIsolation"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (!TestNotNull(TEXT("Subsystem exists"), Subsystem)
		|| !TestTrue(TEXT("Ground registers"), Subsystem->RegisterFloor(Floor(), nullptr))
		|| !TestTrue(TEXT("Inactive basement registers"), Subsystem->RegisterFloor(InactiveBasementFloor(), nullptr))
		|| !TestTrue(TEXT("Room registers"), Subsystem->RegisterOccluder(RoomSegments(), false, true, nullptr).IsValid()))
	{
		return false;
	}

	FSightWeaveVisionSourceDescription LocalDescription = VisionSource(FVector(0.0, 0.0, 100.0));
	const FSightWeaveVisionSourceHandle LocalHandle =
		Subsystem->RegisterVisionSource(LocalDescription, nullptr);
	FSightWeaveVisionSourceDescription RemoteDescription = LocalDescription;
	RemoteDescription.KnowledgeOwnerId = Remote;
	const FSightWeaveVisionSourceHandle RemoteHandle =
		Subsystem->RegisterVisionSource(RemoteDescription, nullptr);
	FSightWeaveVisionSourceDescription HeightDescription = LocalDescription;
	HeightDescription.HeightRange.ZMin = 50.0f;
	HeightDescription.HeightRange.ZMax = 250.0f;
	const FSightWeaveVisionSourceHandle HeightHandle =
		Subsystem->RegisterVisionSource(HeightDescription, nullptr);
	FSightWeaveVisionSourceDescription BasementDescription = LocalDescription;
	BasementDescription.FloorId = Basement;
	const FSightWeaveVisionSourceHandle BasementHandle =
		Subsystem->RegisterVisionSource(BasementDescription, nullptr);
	FSightWeaveVisionSourceDescription RangeDescription =
		VisionSource(FVector(0.0, 0.0, 100.0), 600.0);
	const FSightWeaveVisionSourceHandle RangeHandle =
		Subsystem->RegisterVisionSource(RangeDescription, nullptr);
	FSightWeaveVisionSourceDescription ConeDescription = LocalDescription;
	ConeDescription.Shape = ESightWeaveSourceShape::CameraCone;
	ConeDescription.HalfAngleDegrees = 35.0f;
	const FSightWeaveVisionSourceHandle ConeHandle =
		Subsystem->RegisterVisionSource(ConeDescription, nullptr);
	if (!TestTrue(TEXT("Local source registers"), LocalHandle.IsValid())
		|| !TestTrue(TEXT("Remote-owner source registers"), RemoteHandle.IsValid())
		|| !TestTrue(TEXT("Different-height source registers"), HeightHandle.IsValid())
		|| !TestTrue(TEXT("Different-floor source registers"), BasementHandle.IsValid())
		|| !TestTrue(TEXT("Different-range source registers"), RangeHandle.IsValid())
		|| !TestTrue(TEXT("Different-cone source registers"), ConeHandle.IsValid()))
	{
		return false;
	}
	const FSightWeavePreparedEventIndexStats SharedStats = Subsystem->GetPreparedEventIndexStats();
	TestTrue(TEXT("Compatible origins share geometry preparation"), SharedStats.HitCount >= 3);
	TestTrue(TEXT("Height remains an exact cache-key boundary"), SharedStats.MissCount >= 2);
	FSightWeaveFloorDefinition InactiveGround = Floor();
	InactiveGround.bActiveForQueries = false;
	FSightWeaveFloorDefinition ActiveBasement = InactiveBasementFloor();
	ActiveBasement.bActiveForQueries = true;
	TestTrue(TEXT("Ground deactivates for floor-key proof"), Subsystem->UpdateFloor(Ground, InactiveGround));
	const int64 MissesBeforeBasementActivation = Subsystem->GetPreparedEventIndexStats().MissCount;
	TestTrue(TEXT("Basement activates for floor-key proof"), Subsystem->UpdateFloor(Basement, ActiveBasement));
	TestEqual(
		TEXT("Activating another floor builds a distinct prepared key"),
		Subsystem->GetPreparedEventIndexStats().MissCount,
		MissesBeforeBasementActivation + 1);
	TestTrue(TEXT("Basement deactivates after floor-key proof"), Subsystem->UpdateFloor(Basement, InactiveBasementFloor()));
	TestTrue(TEXT("Ground reactivates after floor-key proof"), Subsystem->UpdateFloor(Ground, Floor()));

	FSightWeaveVisionSourceDescription VisibleVision = LocalDescription;
	VisibleVision.IlluminationPolicy = ESightWeaveIlluminationPolicy::RequiresLegalIllumination;
	VisibleVision.Compatibility.AcceptedCapabilities.Add(FName(TEXT("Visible")));
	const FSightWeaveVisionSourceHandle VisibleVisionHandle =
		Subsystem->RegisterVisionSource(VisibleVision, nullptr);
	FSightWeaveVisionSourceDescription InfraredVision = VisibleVision;
	InfraredVision.Compatibility.AcceptedCapabilities.Reset();
	InfraredVision.Compatibility.AcceptedCapabilities.Add(FName(TEXT("Infrared")));
	const FSightWeaveVisionSourceHandle InfraredVisionHandle =
		Subsystem->RegisterVisionSource(InfraredVision, nullptr);
	FSightWeaveIlluminationSourceDescription VisibleLight =
		IlluminationSource(FVector(0.0, 0.0, 100.0));
	VisibleLight.EmittedCapabilities.Add(FName(TEXT("Visible")));
	const FSightWeaveIlluminationSourceHandle VisibleLightHandle =
		Subsystem->RegisterIlluminationSource(VisibleLight, nullptr);
	FSightWeaveIlluminationSourceDescription InfraredLight = VisibleLight;
	InfraredLight.EmittedCapabilities.Reset();
	InfraredLight.EmittedCapabilities.Add(FName(TEXT("Infrared")));
	const FSightWeaveIlluminationSourceHandle InfraredLightHandle =
		Subsystem->RegisterIlluminationSource(InfraredLight, nullptr);
	if (!TestTrue(TEXT("Visible vision registers"), VisibleVisionHandle.IsValid())
		|| !TestTrue(TEXT("Infrared vision registers"), InfraredVisionHandle.IsValid())
		|| !TestTrue(TEXT("Visible light registers"), VisibleLightHandle.IsValid())
		|| !TestTrue(TEXT("Infrared light registers"), InfraredLightHandle.IsValid()))
	{
		return false;
	}

	const FSightWeaveFrameSnapshot CapabilitySnapshot = Subsystem->GetPublishedSnapshot();
	const FSightWeaveVisionSnapshotEntry* VisibleEntry =
		FindVisionEntry(CapabilitySnapshot, VisibleVisionHandle);
	const FSightWeaveVisionSnapshotEntry* InfraredEntry =
		FindVisionEntry(CapabilitySnapshot, InfraredVisionHandle);
	TestTrue(
		TEXT("Visible capability binds only visible light"),
		VisibleEntry
			&& VisibleEntry->CompatibleIlluminationSources.Num() == 1
			&& VisibleEntry->CompatibleIlluminationSources[0] == VisibleLightHandle);
	TestTrue(
		TEXT("Infrared capability binds only infrared light"),
		InfraredEntry
			&& InfraredEntry->CompatibleIlluminationSources.Num() == 1
			&& InfraredEntry->CompatibleIlluminationSources[0] == InfraredLightHandle);

	const FSightWeaveVisibilityQueryResult LocalQuery =
		Subsystem->QueryEffectiveLiveAtLocation(Local, Ground, FVector(0.0, 0.0, 100.0));
	const FSightWeaveVisibilityQueryResult RemoteQuery =
		Subsystem->QueryEffectiveLiveAtLocation(Remote, Ground, FVector(0.0, 0.0, 100.0));
	const FSightWeaveVisibilityQueryResult BasementQuery =
		Subsystem->QueryEffectiveLiveAtLocation(Local, Basement, FVector(0.0, 0.0, 100.0));
	TestTrue(
		TEXT("Local query excludes remote-owner attribution"),
		LocalQuery.bVisible
			&& LocalQuery.ContributingVisionSources.Contains(LocalHandle)
			&& !LocalQuery.ContributingVisionSources.Contains(RemoteHandle));
	TestTrue(
		TEXT("Remote query excludes local-owner attribution"),
		RemoteQuery.bVisible
			&& RemoteQuery.ContributingVisionSources.Contains(RemoteHandle)
			&& !RemoteQuery.ContributingVisionSources.Contains(LocalHandle));
	TestTrue(
		TEXT("Inactive floor cannot leak ground visibility"),
		BasementQuery.bAuthoritative && !BasementQuery.bVisible);

	const FSightWeaveFrameSnapshot ColdSnapshot = Subsystem->GetPublishedSnapshot();
	const FSightWeaveVisionSnapshotEntry* ColdLocal = FindVisionEntry(ColdSnapshot, LocalHandle);
	if (!TestNotNull(TEXT("Cold local entry exists"), ColdLocal))
	{
		return false;
	}
	const TArray<FVector> ColdVertices = ColdLocal->Polygon.Vertices;
	const FSightWeaveRevision ColdSourceRevision = ColdLocal->SourceRevision;
	const FSightWeaveVisionSnapshotEntry* WarmSharedPeer =
		FindVisionEntry(ColdSnapshot, RemoteHandle);
	TestTrue(
		TEXT("Cold build and same-input shared hit have exact output"),
		WarmSharedPeer && WarmSharedPeer->Polygon.Vertices == ColdVertices);
	RangeDescription.Range = 700.0f;
	TestTrue(TEXT("Range-only update succeeds"), Subsystem->UpdateVisionSource(RangeHandle, RangeDescription));
	const FSightWeaveFrameSnapshot AfterRangeSnapshot = Subsystem->GetPublishedSnapshot();
	const FSightWeaveVisionSnapshotEntry* LocalAfterRange =
		FindVisionEntry(AfterRangeSnapshot, LocalHandle);
	TestTrue(
		TEXT("One source range change does not pollute shared peers"),
		LocalAfterRange
			&& LocalAfterRange->SourceRevision == ColdSourceRevision
			&& LocalAfterRange->Polygon.Vertices == ColdVertices);

	TArray<TArray<FVector>> FirstRotationTrace;
	FirstRotationTrace.Reserve(8);
	for (int32 RotationIndex = 0; RotationIndex < 8; ++RotationIndex)
	{
		LocalDescription.Transform.SetRotation(FQuat(
			FVector::UpVector,
			FMath::DegreesToRadians(17.0 * static_cast<double>(RotationIndex + 1))));
		TestTrue(
			TEXT("Deterministic radial rotation succeeds"),
			Subsystem->UpdateVisionSourceTransform(LocalHandle, LocalDescription.Transform));
		const FSightWeaveFrameSnapshot WarmSnapshot = Subsystem->GetPublishedSnapshot();
		const FSightWeaveVisionSnapshotEntry* WarmEntry =
			FindVisionEntry(WarmSnapshot, LocalHandle);
		TestTrue(
			TEXT("First deterministic radial trace entry is valid"),
			WarmEntry && WarmEntry->Polygon.IsValid());
		FirstRotationTrace.Add(WarmEntry ? WarmEntry->Polygon.Vertices : TArray<FVector>());
	}
	for (int32 RotationIndex = 0; RotationIndex < FirstRotationTrace.Num(); ++RotationIndex)
	{
		LocalDescription.Transform.SetRotation(FQuat(
			FVector::UpVector,
			FMath::DegreesToRadians(17.0 * static_cast<double>(RotationIndex + 1))));
		TestTrue(
			TEXT("Repeated deterministic radial rotation succeeds"),
			Subsystem->UpdateVisionSourceTransform(LocalHandle, LocalDescription.Transform));
		const FSightWeaveFrameSnapshot RepeatedSnapshot = Subsystem->GetPublishedSnapshot();
		const FSightWeaveVisionSnapshotEntry* RepeatedEntry =
			FindVisionEntry(RepeatedSnapshot, LocalHandle);
		TestTrue(
			TEXT("Repeated radial trace is bitwise deterministic"),
			RepeatedEntry && RepeatedEntry->Polygon.Vertices == FirstRotationTrace[RotationIndex]);
	}
	LocalDescription.Transform.SetLocation(FVector(25.0, 0.0, 100.0));
	TestTrue(
		TEXT("Translation builds another exact origin"),
		Subsystem->UpdateVisionSourceTransform(LocalHandle, LocalDescription.Transform));
	LocalDescription.Transform.SetLocation(FVector(0.0, 0.0, 100.0));
	TestTrue(
		TEXT("Returning to resident origin succeeds"),
		Subsystem->UpdateVisionSourceTransform(LocalHandle, LocalDescription.Transform));
	const FSightWeaveFrameSnapshot ReturnedSnapshot = Subsystem->GetPublishedSnapshot();
	const FSightWeaveVisionSnapshotEntry* ReturnedEntry =
		FindVisionEntry(ReturnedSnapshot, LocalHandle);
	TestTrue(
		TEXT("Returning to resident origin reproduces the same-rotation warm output"),
		ReturnedEntry
			&& !FirstRotationTrace.IsEmpty()
			&& ReturnedEntry->Polygon.Vertices == FirstRotationTrace.Last());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P2PreparedEventIndexMemoryPressureTest,
	"SightWeave.M2P2.PreparedEventIndex.MemoryPressureReclamation",
	SightWeave::M2P2::PreparedEventIndexTests::TestFlags)

bool FSightWeaveM2P2PreparedEventIndexMemoryPressureTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P2::PreparedEventIndexTests;
	constexpr int64 ByteCap = 1024ll * 1024ll;
	FTestWorld World(TEXT("SightWeaveM2P2PreparedMemoryPressure"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (!TestNotNull(TEXT("Subsystem exists"), Subsystem)
		|| !TestTrue(TEXT("Floor registers"), Subsystem->RegisterFloor(Floor(), nullptr))
		|| !TestTrue(
			TEXT("Byte-pressure configuration applies"),
			Subsystem->ConfigurePreparedEventIndexForTesting(4, ByteCap))
		|| !TestTrue(
			TEXT("Dense occluder registers"),
			Subsystem->RegisterOccluder(MakeDenseSegments(2048), false, true, nullptr).IsValid()))
	{
		return false;
	}

	FSightWeaveVisionSourceDescription A = VisionSource(FVector(0.0, 0.0, 100.0), 1200.0);
	FSightWeaveVisionSourceDescription B = VisionSource(FVector(5.0, 0.0, 100.0), 1200.0);
	const FSightWeaveVisionSourceHandle AHandle = Subsystem->RegisterVisionSource(A, nullptr);
	if (!TestTrue(TEXT("First dense source registers"), AHandle.IsValid()))
	{
		return false;
	}
	const FSightWeavePreparedEventIndexStats AfterA = Subsystem->GetPreparedEventIndexStats();
	TestEqual(TEXT("First dense entry is retained"), AfterA.LiveEntryCount, 1);
	TestTrue(TEXT("First dense entry has observable bytes"), AfterA.LiveAllocatedBytes > 0);
	TestTrue(TEXT("First dense entry respects byte cap"), AfterA.LiveAllocatedBytes <= ByteCap);

	const FSightWeaveVisionSourceHandle BHandle = Subsystem->RegisterVisionSource(B, nullptr);
	if (!TestTrue(TEXT("Second dense source remains exact through fallback"), BHandle.IsValid()))
	{
		return false;
	}
	const FSightWeavePreparedEventIndexStats AfterPressure =
		Subsystem->GetPreparedEventIndexStats();
	TestTrue(
		TEXT("Bound-entry byte pressure records exact fallback"),
		AfterPressure.CapacityFallbackCount > AfterA.CapacityFallbackCount);
	TestTrue(TEXT("Retained bytes remain within cap"), AfterPressure.LiveAllocatedBytes <= ByteCap);
	TestTrue(
		TEXT("Attempted pressure is disclosed by high water"),
		AfterPressure.HighWaterAllocatedBytes > AfterPressure.LiveAllocatedBytes);

	TestTrue(TEXT("First source releases its binding"), Subsystem->UnregisterVisionSource(AHandle));
	B.Transform.SetLocation(FVector(20.0, 0.0, 100.0));
	TestTrue(
		TEXT("Fallback source reacquires after old binding release"),
		Subsystem->UpdateVisionSourceTransform(BHandle, B.Transform));
	const FSightWeavePreparedEventIndexStats AfterReclaim =
		Subsystem->GetPreparedEventIndexStats();
	TestEqual(TEXT("One dense entry remains after reclaim"), AfterReclaim.LiveEntryCount, 1);
	TestEqual(TEXT("One dense source is bound after reclaim"), AfterReclaim.SourceBindingCount, 1);
	TestTrue(TEXT("Reclaim evicts the unbound high-water entry"), AfterReclaim.EvictionCount >= 1);
	TestTrue(TEXT("Reclaimed live bytes respect cap"), AfterReclaim.LiveAllocatedBytes <= ByteCap);

	TestTrue(
		TEXT("Additional geometry registers through oversized exact fallback"),
		Subsystem->RegisterOccluder(MakeDenseSegments(64), false, true, nullptr).IsValid());
	const FSightWeavePreparedEventIndexStats AfterOversized =
		Subsystem->GetPreparedEventIndexStats();
	TestTrue(TEXT("Oversized preparation is diagnosed"), AfterOversized.OversizedEntryCount >= 1);
	TestEqual(TEXT("Oversized preparation is not retained"), AfterOversized.LiveEntryCount, 0);
	TestEqual(TEXT("Oversized retained bytes are released"), AfterOversized.LiveAllocatedBytes, int64(0));
	TestTrue(
		TEXT("High-water evidence survives reclamation"),
		AfterOversized.HighWaterAllocatedBytes >= AfterReclaim.HighWaterAllocatedBytes);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P2PreparedEventIndexWorldLifecycleTest,
	"SightWeave.M2P2.PreparedEventIndex.WorldLifecycleIsolation",
	SightWeave::M2P2::PreparedEventIndexTests::TestFlags)

bool FSightWeaveM2P2PreparedEventIndexWorldLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P2::PreparedEventIndexTests;
	TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> HeldAcrossTeardown;
	for (int32 RestartIndex = 0; RestartIndex < 3; ++RestartIndex)
	{
		FTestWorld RestartedWorld(TEXT("SightWeaveM2P2PreparedRestart"));
		USightWeaveWorldSubsystem* RestartedSubsystem = RestartedWorld.GetSubsystem();
		if (!TestNotNull(TEXT("Restarted subsystem exists"), RestartedSubsystem))
		{
			return false;
		}
		const FSightWeavePreparedEventIndexStats EmptyStats =
			RestartedSubsystem->GetPreparedEventIndexStats();
		TestEqual(TEXT("Restart begins with zero prepared entries"), EmptyStats.LiveEntryCount, 0);
		TestEqual(TEXT("Restart begins with zero prepared bytes"), EmptyStats.LiveAllocatedBytes, int64(0));
		TestEqual(TEXT("Restart begins with zero prepared misses"), EmptyStats.MissCount, int64(0));
		if (!TestTrue(TEXT("Restart floor registers"), RestartedSubsystem->RegisterFloor(Floor(), nullptr))
			|| !TestTrue(
				TEXT("Restart room registers"),
				RestartedSubsystem->RegisterOccluder(RoomSegments(), false, true, nullptr).IsValid())
			|| !TestTrue(
				TEXT("Restart source registers"),
				RestartedSubsystem->RegisterVisionSource(
					VisionSource(FVector(0.0, 0.0, 100.0)),
					nullptr).IsValid()))
		{
			return false;
		}
		const FSightWeavePreparedEventIndexStats PopulatedStats =
			RestartedSubsystem->GetPreparedEventIndexStats();
		TestEqual(TEXT("Restart owns one prepared entry"), PopulatedStats.LiveEntryCount, 1);
		TestEqual(TEXT("Restart owns one prepared binding"), PopulatedStats.SourceBindingCount, 1);
		if (RestartIndex == 0)
		{
			HeldAcrossTeardown = RestartedSubsystem->AcquirePublishedSnapshotForTesting();
		}
	}
	TestTrue(
		TEXT("Plain-data held snapshot survives repeated world teardown"),
		HeldAcrossTeardown.IsValid()
			&& HeldAcrossTeardown->VisionSources.Num() == 1
			&& HeldAcrossTeardown->VisionSources[0].Polygon.IsValid());

	FTestWorld WorldA(TEXT("SightWeaveM2P2PreparedWorldA"));
	FTestWorld WorldB(TEXT("SightWeaveM2P2PreparedWorldB"));
	USightWeaveWorldSubsystem* SubsystemA = WorldA.GetSubsystem();
	USightWeaveWorldSubsystem* SubsystemB = WorldB.GetSubsystem();
	if (!TestNotNull(TEXT("World A subsystem exists"), SubsystemA)
		|| !TestNotNull(TEXT("World B subsystem exists"), SubsystemB)
		|| !TestTrue(TEXT("World A floor registers"), SubsystemA->RegisterFloor(Floor(), nullptr))
		|| !TestTrue(TEXT("World B floor registers"), SubsystemB->RegisterFloor(Floor(), nullptr))
		|| !TestTrue(TEXT("World A config is independent"), SubsystemA->ConfigurePreparedEventIndexForTesting(1, 1024ll * 1024ll))
		|| !TestTrue(TEXT("World B config is independent"), SubsystemB->ConfigurePreparedEventIndexForTesting(3, 4ll * 1024ll * 1024ll))
		|| !TestTrue(TEXT("World A room registers"), SubsystemA->RegisterOccluder(RoomSegments(), false, true, nullptr).IsValid())
		|| !TestTrue(TEXT("World B room registers"), SubsystemB->RegisterOccluder(RoomSegments(), false, true, nullptr).IsValid()))
	{
		return false;
	}
	FSightWeaveVisionSourceDescription SourceA = VisionSource(FVector(0.0, 0.0, 100.0));
	const FSightWeaveVisionSourceHandle HandleA = SubsystemA->RegisterVisionSource(SourceA, nullptr);
	const FSightWeaveVisionSourceHandle HandleB = SubsystemB->RegisterVisionSource(SourceA, nullptr);
	if (!TestTrue(TEXT("World A source registers"), HandleA.IsValid())
		|| !TestTrue(TEXT("World B source registers"), HandleB.IsValid()))
	{
		return false;
	}
	const FSightWeavePreparedEventIndexStats BeforeA = SubsystemA->GetPreparedEventIndexStats();
	const FSightWeavePreparedEventIndexStats BeforeB = SubsystemB->GetPreparedEventIndexStats();
	SourceA.Transform.SetRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(45.0)));
	TestTrue(
		TEXT("World A source rotation succeeds"),
		SubsystemA->UpdateVisionSourceTransform(HandleA, SourceA.Transform));
	const FSightWeavePreparedEventIndexStats AfterA = SubsystemA->GetPreparedEventIndexStats();
	const FSightWeavePreparedEventIndexStats AfterB = SubsystemB->GetPreparedEventIndexStats();
	TestEqual(TEXT("World A records its own cache hit"), AfterA.HitCount, BeforeA.HitCount + 1);
	TestEqual(TEXT("World B hit counter is isolated"), AfterB.HitCount, BeforeB.HitCount);
	TestEqual(TEXT("World B miss counter is isolated"), AfterB.MissCount, BeforeB.MissCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P2PreparedCacheConcurrentIsolationTest,
	"SightWeave.M2P2.PreparedEventIndex.ConcurrentScratchIsolation",
	SightWeave::M2P2::PreparedEventIndexTests::TestFlags)

bool FSightWeaveM2P2PreparedCacheConcurrentIsolationTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P2::PreparedEventIndexTests;
	FSightWeaveReferenceSolveInput Input;
	Input.Origin = FVector(37.0, -19.0, 100.0);
	Input.Forward = FVector2D(1.0, 0.0);
	Input.Shape = ESightWeaveSourceShape::CameraCone;
	Input.Range = 1200.0;
	Input.HalfAngleDegrees = 50.0;
	Input.FloorId = Ground;
	Input.HeightRange.ZMin = 0.0f;
	Input.HeightRange.ZMax = 300.0f;
	Input.Segments = MakeDenseSegments(1024);
	TestTrue(
		TEXT("Eight independent prepared indexes and solver scratch frames remain deterministic"),
		USightWeaveWorldSubsystem::ExercisePreparedEventIndexConcurrentIsolationForTesting(Input, 8, 8));
	return true;
}

#endif
