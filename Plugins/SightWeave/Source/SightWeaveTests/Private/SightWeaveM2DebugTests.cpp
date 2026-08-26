#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SightWeaveActors.h"
#include "SightWeaveWorldSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace SightWeave::M2::DebugTests
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

	FSightWeaveFloorDefinition Floor()
	{
		FSightWeaveFloorDefinition Result;
		Result.FloorId = Ground;
		Result.BoundsMin = FVector2D(-10000.0, -10000.0);
		Result.BoundsMax = FVector2D(10000.0, 10000.0);
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		return Result;
	}

	FSightWeaveVisionSourceDescription Vision(const FVector Location, const bool bBypass = true)
	{
		FSightWeaveVisionSourceDescription Result;
		Result.KnowledgeOwnerId = Local;
		Result.FloorId = Ground;
		Result.Transform.SetLocation(Location);
		Result.Shape = ESightWeaveSourceShape::Radial;
		Result.Range = 1200.0f;
		Result.HalfAngleDegrees = 180.0f;
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		Result.IlluminationPolicy = bBypass
			? ESightWeaveIlluminationPolicy::BypassLegalIllumination
			: ESightWeaveIlluminationPolicy::RequiresLegalIllumination;
		if (!bBypass) Result.Compatibility.AcceptedCapabilities = { FName(TEXT("Visible")) };
		return Result;
	}

	FSightWeaveIlluminationSourceDescription Light(const FVector Location)
	{
		FSightWeaveIlluminationSourceDescription Result;
		Result.KnowledgeOwnerId = Local;
		Result.FloorId = Ground;
		Result.Transform.SetLocation(Location);
		Result.Shape = ESightWeaveSourceShape::Radial;
		Result.Range = 1200.0f;
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		Result.EmittedCapabilities = { FName(TEXT("Visible")) };
		return Result;
	}

	FSightWeaveSegment2D Segment(const FVector2D A, const FVector2D B, const bool bDynamic = false)
	{
		FSightWeaveSegment2D Result;
		Result.A = A;
		Result.B = B;
		Result.FloorId = Ground;
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		Result.bDynamic = bDynamic;
		return Result;
	}

	struct FProfileMetrics
	{
		int32 SourceCount = 0;
		int32 TotalSegments = 0;
		int32 TotalCandidates = 0;
		int32 TotalRays = 0;
		int32 TotalVertices = 0;
		double TotalSolveMicroseconds = 0.0;
		uint32 GeometryHash = 0;
		bool bValid = true;
	};

	FProfileMetrics RunDeterministicProfile(const TCHAR* WorldName, const int32 SourceCount)
	{
		FProfileMetrics Metrics;
		Metrics.SourceCount = SourceCount;
		FTestWorld World(WorldName);
		USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
		if (!Subsystem || !Subsystem->RegisterFloor(Floor(), nullptr))
		{
			Metrics.bValid = false;
			return Metrics;
		}

		FRandomStream Random(0x51A7E);
		for (int32 Index = 0; Index < 64; ++Index)
		{
			const FVector2D Center(Random.FRandRange(-1000.0f, 1000.0f), Random.FRandRange(-1000.0f, 1000.0f));
			const double Angle = Random.FRandRange(0.0f, 2.0f * PI);
			const double HalfLength = Random.FRandRange(20.0f, 90.0f);
			const FVector2D Offset(FMath::Cos(Angle) * HalfLength, FMath::Sin(Angle) * HalfLength);
			if (!Subsystem->RegisterOccluder({ Segment(Center - Offset, Center + Offset) }, false, true, nullptr).IsValid())
			{
				Metrics.bValid = false;
			}
		}
		for (int32 Index = 0; Index < SourceCount; ++Index)
		{
			const double Angle = 2.0 * PI * Index / SourceCount;
			const FVector Location(FMath::Cos(Angle) * 250.0, FMath::Sin(Angle) * 250.0, 100.0);
			if (!Subsystem->RegisterVisionSource(Vision(Location), nullptr).IsValid())
			{
				Metrics.bValid = false;
			}
		}

		const FSightWeaveDebugData Data = Subsystem->BuildDebugData();
		Metrics.TotalSegments = Data.SpatialIndexStats.SegmentCount;
		for (const FSightWeaveVisionSnapshotEntry& Entry : Data.Snapshot.VisionSources)
		{
			Metrics.TotalCandidates += Entry.CandidateSegmentCount;
			Metrics.TotalRays += Entry.CandidateRayCount;
			Metrics.TotalVertices += Entry.Polygon.Vertices.Num();
			Metrics.TotalSolveMicroseconds += Entry.SolveTimeMicroseconds;
			Metrics.bValid &= Entry.Polygon.IsValid() && FMath::IsFinite(Entry.SolveTimeMicroseconds);
			for (const FVector& Vertex : Entry.Polygon.Vertices)
			{
				const FIntVector Quantized(
					FMath::RoundToInt(Vertex.X * 100.0),
					FMath::RoundToInt(Vertex.Y * 100.0),
					FMath::RoundToInt(Vertex.Z * 100.0));
				Metrics.GeometryHash = HashCombine(Metrics.GeometryHash, GetTypeHash(Quantized));
			}
		}
		return Metrics;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2DebugDataCompletenessTest,
	"SightWeave.M2.Debug.DataCompletenessAndDraw",
	SightWeave::M2::DebugTests::TestFlags)

bool FSightWeaveM2DebugDataCompletenessTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2::DebugTests;
	FTestWorld World(TEXT("SightWeaveDebugCompleteness"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (TestNotNull(TEXT("Subsystem exists"), Subsystem))
	{
		TestTrue(TEXT("Floor registers"), Subsystem->RegisterFloor(Floor(), nullptr));
		Subsystem->RegisterOccluder({ Segment(FVector2D(100.0, -300.0), FVector2D(100.0, 300.0), true) }, true, true, nullptr);
		Subsystem->RegisterVisionSource(Vision(FVector(0.0, 0.0, 100.0), false), nullptr);
		Subsystem->RegisterVisionSource(Vision(FVector(-300.0, 0.0, 100.0), true), nullptr);
		Subsystem->RegisterIlluminationSource(Light(FVector::ZeroVector), nullptr);
		FSightWeaveHardSuppressionDescription Suppression;
		Suppression.FloorId = Ground;
		Suppression.HeightRange.ZMin = 0.0f;
		Suppression.HeightRange.ZMax = 300.0f;
		Suppression.Center = FVector2D(-300.0, 0.0);
		Suppression.Radius = 50.0f;
		Subsystem->RegisterHardLiveSuppression(Suppression, nullptr);

		const FVector QueryPoint(-300.0, 0.0, 100.0);
		const FSightWeaveVisibilityQueryResult Query = Subsystem->QueryEffectiveLiveAtLocation(Local, Ground, QueryPoint);
		const FSightWeaveDebugData Data = Subsystem->BuildDebugData();
		TestEqual(TEXT("Debug snapshot contains floor"), Data.Snapshot.Floors.Num(), 1);
		TestEqual(TEXT("Debug snapshot contains occluder"), Data.Snapshot.OccluderSegments.Num(), 1);
		TestEqual(TEXT("Debug snapshot keeps distinct vision entries"), Data.Snapshot.VisionSources.Num(), 2);
		TestEqual(TEXT("Debug snapshot keeps distinct illumination entry"), Data.Snapshot.IlluminationSources.Num(), 1);
		TestEqual(TEXT("Debug snapshot contains hard suppression"), Data.Snapshot.HardSuppressions.Num(), 1);
		TestTrue(TEXT("Actual normalized epsilon policy is available"), Data.GeometryTolerances.IsValid());
		TestTrue(TEXT("Spatial statistics expose cells and segments"), Data.SpatialIndexStats.CellCount > 0 && Data.SpatialIndexStats.SegmentCount == 1);
		TestTrue(TEXT("Spatial cell geometry is available"), !Data.SpatialCells.IsEmpty());
		TestTrue(TEXT("Reference candidate events are inspectable"),
			Data.Snapshot.VisionSources.ContainsByPredicate([](const FSightWeaveVisionSnapshotEntry& Entry)
			{
				return Entry.CandidateRayCount > 0 && !Entry.CandidateAnglesRadians.IsEmpty();
			}));
		TestEqual(TEXT("Debug and query share a snapshot revision"), Data.Snapshot.Revision.GetValue(), Query.SnapshotRevision.GetValue());

		FSightWeaveDebugDrawOptions Options;
		Options.bDrawCandidateRays = true;
		Options.bDrawSpatialCells = true;
		FSightWeaveDebugQueryMarker Marker;
		Marker.WorldLocation = QueryPoint;
		Marker.Result = Query;
		TestTrue(TEXT("Non-Shipping explicit DrawDebug API executes"), Subsystem->DrawDebugSnapshot(Options, { Marker }));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2DebugSnapshotStabilityTest,
	"SightWeave.M2.Debug.ImmutableDataAndStableCells",
	SightWeave::M2::DebugTests::TestFlags)

bool FSightWeaveM2DebugSnapshotStabilityTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2::DebugTests;
	FTestWorld World(TEXT("SightWeaveDebugStability"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (TestNotNull(TEXT("Subsystem exists"), Subsystem))
	{
		Subsystem->RegisterFloor(Floor(), nullptr);
		const FSightWeaveOccluderHandle Door = Subsystem->RegisterOccluder(
			{ Segment(FVector2D(100.0, -100.0), FVector2D(100.0, 100.0), true) }, true, true, nullptr);
		Subsystem->RegisterVisionSource(Vision(FVector::ZeroVector), nullptr);
		const FSightWeaveDebugData Before = Subsystem->BuildDebugData();
		TestTrue(TEXT("Door updates"), Subsystem->UpdateOccluder(
			Door, { Segment(FVector2D(700.0, -100.0), FVector2D(700.0, 100.0), true) }, true, true));
		const FSightWeaveDebugData After = Subsystem->BuildDebugData();
		TestTrue(TEXT("Published debug revision advances"), After.Snapshot.Revision.GetValue() > Before.Snapshot.Revision.GetValue());
		TestTrue(TEXT("Copied debug data remains immutable"),
			Before.Snapshot.OccluderSegments.Num() == 1
			&& FMath::IsNearlyEqual(Before.Snapshot.OccluderSegments[0].A.X, 100.0));
		TestTrue(TEXT("New debug data contains only new door segment"),
			After.Snapshot.OccluderSegments.Num() == 1
			&& FMath::IsNearlyEqual(After.Snapshot.OccluderSegments[0].A.X, 700.0));
		for (int32 Index = 1; Index < After.SpatialCells.Num(); ++Index)
		{
			const FSightWeaveSpatialCellDebug& Previous = After.SpatialCells[Index - 1];
			const FSightWeaveSpatialCellDebug& Current = After.SpatialCells[Index];
			TestTrue(TEXT("Debug cells use deterministic floor/X/Y ordering"),
				Previous.FloorId.GetValue().LexicalLess(Current.FloorId.GetValue())
				|| Previous.FloorId == Current.FloorId
					&& (Previous.Cell.X < Current.Cell.X
						|| Previous.Cell.X == Current.Cell.X && Previous.Cell.Y < Current.Cell.Y));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2ReferencePerformanceTest,
	"SightWeave.M2.Debug.Performance.ReferenceTwoAndEightSources",
	SightWeave::M2::DebugTests::TestFlags)

bool FSightWeaveM2ReferencePerformanceTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2::DebugTests;
	const FProfileMetrics TwoA = RunDeterministicProfile(TEXT("SightWeaveProfileTwoA"), 2);
	const FProfileMetrics TwoB = RunDeterministicProfile(TEXT("SightWeaveProfileTwoB"), 2);
	const FProfileMetrics EightA = RunDeterministicProfile(TEXT("SightWeaveProfileEightA"), 8);
	const FProfileMetrics EightB = RunDeterministicProfile(TEXT("SightWeaveProfileEightB"), 8);
	TestTrue(TEXT("Two-source reference profile is valid"), TwoA.bValid && TwoB.bValid);
	TestTrue(TEXT("Eight-source reference profile is valid"), EightA.bValid && EightB.bValid);
	TestEqual(TEXT("Two-source deterministic vertex count"), TwoA.TotalVertices, TwoB.TotalVertices);
	TestEqual(TEXT("Two-source deterministic candidate count"), TwoA.TotalCandidates, TwoB.TotalCandidates);
	TestEqual(TEXT("Two-source deterministic ray count"), TwoA.TotalRays, TwoB.TotalRays);
	TestEqual(TEXT("Two-source deterministic geometry hash"), TwoA.GeometryHash, TwoB.GeometryHash);
	TestEqual(TEXT("Eight-source deterministic vertex count"), EightA.TotalVertices, EightB.TotalVertices);
	TestEqual(TEXT("Eight-source deterministic candidate count"), EightA.TotalCandidates, EightB.TotalCandidates);
	TestEqual(TEXT("Eight-source deterministic ray count"), EightA.TotalRays, EightB.TotalRays);
	TestEqual(TEXT("Eight-source deterministic geometry hash"), EightA.GeometryHash, EightB.GeometryHash);
	TestTrue(TEXT("Two-source reference sample completes within a broad non-flaky ceiling"), TwoA.TotalSolveMicroseconds < 1000000.0);
	TestTrue(TEXT("Eight-source reference sample completes within a broad non-flaky ceiling"), EightA.TotalSolveMicroseconds < 4000000.0);
	AddInfo(FString::Printf(TEXT("PROFILE Reference 2 sources: segments=%d candidates=%d rays=%d vertices=%d solve_us=%.3f seed=0x51A7E"),
		TwoA.TotalSegments, TwoA.TotalCandidates, TwoA.TotalRays, TwoA.TotalVertices, TwoA.TotalSolveMicroseconds));
	AddInfo(FString::Printf(TEXT("PROFILE Reference 8 sources: segments=%d candidates=%d rays=%d vertices=%d solve_us=%.3f seed=0x51A7E"),
		EightA.TotalSegments, EightA.TotalCandidates, EightA.TotalRays, EightA.TotalVertices, EightA.TotalSolveMicroseconds));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2LabComponentFixturesTest,
	"SightWeave.M2.Debug.LabComponentFixtures",
	SightWeave::M2::DebugTests::TestFlags)

bool FSightWeaveM2LabComponentFixturesTest::RunTest(const FString& Parameters)
{
	UPackage* Package = LoadPackage(nullptr, TEXT("/SightWeave/Maps/L_SightWeave_Lab"), LOAD_None);
	UWorld* World = Package ? UWorld::FindWorldInPackage(Package) : nullptr;
	if (!TestNotNull(TEXT("M2 lab package contains a world"), World))
	{
		return true;
	}
	int32 FloorCount = 0;
	int32 VisionCount = 0;
	int32 IlluminationCount = 0;
	int32 OccluderCount = 0;
	int32 DynamicOccluderCount = 0;
	int32 SuppressionCount = 0;
	int32 DebugQueryCount = 0;
	int32 StressSourceCount = 0;
	bool bHasDebugMarker = false;
	bool bAllBypassProfilesEmpty = true;
	for (ULevel* Level : World->GetLevels())
	{
		if (!Level) continue;
		for (AActor* Actor : Level->Actors)
		{
			if (!Actor) continue;
#if WITH_EDITOR
			const FString Label = Actor->GetActorLabel();
			StressSourceCount += Label.StartsWith(TEXT("SW_M2_12_Stress_")) ? 1 : 0;
			bHasDebugMarker |= Label == TEXT("SW_M2_20_DebugQueryMarker");
#endif
			FloorCount += Actor->FindComponentByClass<USightWeaveFloorComponent>() ? 1 : 0;
			if (const USightWeaveVisionSourceComponent* Vision = Actor->FindComponentByClass<USightWeaveVisionSourceComponent>())
			{
				++VisionCount;
				if (Vision->Description.IlluminationPolicy == ESightWeaveIlluminationPolicy::BypassLegalIllumination)
				{
					bAllBypassProfilesEmpty &= Vision->Description.Compatibility.AcceptedCapabilities.IsEmpty();
				}
			}
			IlluminationCount += Actor->FindComponentByClass<USightWeaveIlluminationSourceComponent>() ? 1 : 0;
			if (const USightWeaveOccluderComponent* Occluder = Actor->FindComponentByClass<USightWeaveOccluderComponent>())
			{
				++OccluderCount;
				DynamicOccluderCount += Occluder->bDynamic ? 1 : 0;
			}
			SuppressionCount += Actor->FindComponentByClass<USightWeaveHardSuppressionComponent>() ? 1 : 0;
			DebugQueryCount += Actor->FindComponentByClass<USightWeaveDebugQueryComponent>() ? 1 : 0;
		}
	}
	TestEqual(TEXT("Lab has two explicit floors"), FloorCount, 2);
	TestEqual(TEXT("Lab has thirty-two real vision-source components"), VisionCount, 32);
	TestEqual(TEXT("Lab has four real legal-illumination components"), IlluminationCount, 4);
	TestEqual(TEXT("Lab has twenty-eight explicit occluder components"), OccluderCount, 28);
	TestEqual(TEXT("Lab has one real dynamic-door occluder"), DynamicOccluderCount, 1);
	TestEqual(TEXT("Lab has one hard-live suppression component"), SuppressionCount, 1);
	TestEqual(TEXT("Lab has one no-tick authoritative debug query component"), DebugQueryCount, 1);
	TestEqual(TEXT("Lab has eight independently authored stress sources"), StressSourceCount, 8);
	TestTrue(TEXT("Lab has a named debug query marker"), bHasDebugMarker);
	TestTrue(TEXT("Every authored bypass source has no compatibility key"), bAllBypassProfilesEmpty);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P3LabPresentationFixturesTest,
	"SightWeave.M3P3.Lab.PresentationFixtures",
	SightWeave::M2::DebugTests::TestFlags)

bool FSightWeaveM3P3LabPresentationFixturesTest::RunTest(const FString& Parameters)
{
	UPackage* Package = LoadPackage(nullptr, TEXT("/SightWeave/Maps/L_SightWeave_Lab"), LOAD_None);
	UWorld* World = Package ? UWorld::FindWorldInPackage(Package) : nullptr;
	if (!TestNotNull(TEXT("M3.3 lab package contains a world"), World))
	{
		return true;
	}
	int32 M3OccluderCount = 0;
	int32 M3VisionCount = 0;
	int32 TileSeamMarkerCount = 0;
	int32 CameraCount = 0;
	bool bHasOverviewLabel = false;
	bool bHasPageBoundaryLabel = false;
	bool bHasPageBoundaryVision = false;
	bool bGroundCoversPageBoundary = false;
	for (ULevel* Level : World->GetLevels())
	{
		if (!Level) continue;
		for (AActor* Actor : Level->Actors)
		{
			if (!Actor) continue;
#if WITH_EDITOR
			const FString Label = Actor->GetActorLabel();
			if (Label.StartsWith(TEXT("SW_M3P3_")))
			{
				M3OccluderCount += Actor->FindComponentByClass<USightWeaveOccluderComponent>() ? 1 : 0;
				if (const USightWeaveVisionSourceComponent* Vision =
					Actor->FindComponentByClass<USightWeaveVisionSourceComponent>())
				{
					++M3VisionCount;
					TestEqual(TEXT("M3.3 Lab vision remains Local-owner scoped"),
						Vision->Description.KnowledgeOwnerId,
						FSightWeaveKnowledgeOwnerId(FName(TEXT("Local"))));
					bHasPageBoundaryVision |= Label == TEXT("SW_M3P3_PageBoundaryVision")
						&& FMath::IsNearlyEqual(Vision->Description.Range, 160000.0f);
				}
				TileSeamMarkerCount += Label.StartsWith(TEXT("SW_M3P3_TileSeam_")) ? 1 : 0;
				CameraCount += Label.EndsWith(TEXT("Camera")) ? 1 : 0;
				bHasOverviewLabel |= Label == TEXT("SW_M3P3_Presentation_Label");
				bHasPageBoundaryLabel |= Label == TEXT("SW_M3P3_PageBoundary_Label");
			}
#endif
			if (const USightWeaveFloorComponent* Floor =
				Actor->FindComponentByClass<USightWeaveFloorComponent>())
			{
				bGroundCoversPageBoundary |= Floor->Definition.FloorId
					== FSightWeaveFloorId(FName(TEXT("Ground")))
					&& Floor->Definition.BoundsMax.X >= 154000.0;
			}
		}
	}
	TestEqual(TEXT("M3.3 Lab has straight/L/T/diagonal/page-boundary occluders"),
		M3OccluderCount, 7);
	TestEqual(TEXT("M3.3 Lab has overview and page-boundary live sources"),
		M3VisionCount, 2);
	TestEqual(TEXT("M3.3 Lab marks five logical tile seams"), TileSeamMarkerCount, 5);
	TestEqual(TEXT("M3.3 Lab has overview and page-boundary cameras"), CameraCount, 2);
	TestTrue(TEXT("M3.3 Lab overview label exists"), bHasOverviewLabel);
	TestTrue(TEXT("M3.3 Lab page-boundary label exists"), bHasPageBoundaryLabel);
	TestTrue(TEXT("M3.3 Lab narrow source crosses logical tile 63/64"), bHasPageBoundaryVision);
	TestTrue(TEXT("Ground floor bounds include the page-boundary fixture"), bGroundCoversPageBoundary);
	return true;
}

#endif
