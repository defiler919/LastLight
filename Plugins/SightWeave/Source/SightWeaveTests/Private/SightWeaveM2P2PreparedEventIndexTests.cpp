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
	const FSightWeaveKnowledgeOwnerId Local(FName(TEXT("Local")));

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
	FSightWeaveReferenceSolveResult LastResult;
	const bool bSucceeded = SightWeave::Geometry::Testing::MeasureCachedOptimizedForwardSequence(
		Input,
		Forwards,
		Total,
		Candidate,
		Sort,
		Acceleration,
		LastResult);
	Total.RemoveAt(0, Warmups, EAllowShrinking::No);
	Candidate.RemoveAt(0, Warmups, EAllowShrinking::No);
	Sort.RemoveAt(0, Warmups, EAllowShrinking::No);
	Acceleration.RemoveAt(0, Warmups, EAllowShrinking::No);

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

	TestTrue(TEXT("All cached 4096 solves succeed"), bSucceeded);
	TestEqual(TEXT("Representative warmed sample count"), Total.Num(), Repeats);
	TestTrue(TEXT("4096/source median is below 1 ms"), TotalStats.Median < 1000.0);
	TestTrue(TEXT("4096/source p99 is below 2 ms"), TotalStats.P99 < 2000.0);
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

#endif
