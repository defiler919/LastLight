#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SightWeaveGeometry.h"
#include "SightWeaveSettings.h"
#include "SightWeaveWorldSubsystem.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace SightWeave::M2P4::DynamicSectorTests
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

	FSightWeaveSegment2D Segment(const FVector2D A, const FVector2D B)
	{
		FSightWeaveSegment2D Result;
		Result.A = A;
		Result.B = B;
		Result.FloorId = Ground;
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		return Result;
	}

	TArray<FSightWeaveSegment2D> RoomSegments()
	{
		return {
			Segment(FVector2D(-1000.0, -1000.0), FVector2D(1000.0, -1000.0)),
			Segment(FVector2D(1000.0, -1000.0), FVector2D(1000.0, 1000.0)),
			Segment(FVector2D(1000.0, 1000.0), FVector2D(-1000.0, 1000.0)),
			Segment(FVector2D(-1000.0, 1000.0), FVector2D(-1000.0, -1000.0)) };
	}

	TArray<FSightWeaveSegment2D> Door(const double X)
	{
		return { Segment(FVector2D(X, -100.0), FVector2D(X, 100.0)) };
	}

	TArray<FSightWeaveSegment2D> SplitDoor(const double X)
	{
		return {
			Segment(FVector2D(X, -160.0), FVector2D(X, -20.0)),
			Segment(FVector2D(X, 20.0), FVector2D(X, 160.0)) };
	}

	FSightWeaveVisionSourceDescription Vision(const FVector Location = FVector(0.0, 0.0, 100.0))
	{
		FSightWeaveVisionSourceDescription Result;
		Result.KnowledgeOwnerId = Local;
		Result.FloorId = Ground;
		Result.Transform = FTransform(FQuat::Identity, Location);
		Result.Shape = ESightWeaveSourceShape::Radial;
		Result.Range = 1200.0f;
		Result.HalfAngleDegrees = 180.0f;
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		return Result;
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

	bool SnapshotMatchesFreshFullSolve(
		FAutomationTestBase& Test,
		USightWeaveWorldSubsystem& Subsystem,
		const FSightWeaveVisionSourceHandle Handle,
		const TCHAR* Label)
	{
		const FSightWeaveFrameSnapshot Snapshot = Subsystem.GetPublishedSnapshot();
		const FSightWeaveVisionSnapshotEntry* Entry = FindVisionEntry(Snapshot, Handle);
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s snapshot entry exists"), Label), Entry))
		{
			return false;
		}

		FSightWeaveReferenceSolveInput Input;
		Input.Origin = Entry->Description.Transform.GetLocation();
		const FVector Forward = Entry->Description.Transform.GetUnitAxis(EAxis::X);
		Input.Forward = FVector2D(Forward.X, Forward.Y).GetSafeNormal();
		Input.Shape = Entry->Description.Shape;
		Input.Range = Entry->Description.Range;
		Input.HalfAngleDegrees = Entry->Description.HalfAngleDegrees;
		Input.NearAwarenessRadius = Entry->Description.NearAwarenessRadius;
		Input.FloorId = Entry->Description.FloorId;
		Input.HeightRange = Entry->Description.HeightRange;
		Input.Tolerances = GetDefault<USightWeaveSettings>()->GeometryTolerances;
		Input.Tolerances.Normalize();
		Input.Segments = Snapshot.OccluderSegments;
		const FSightWeaveReferenceSolveResult Full = Geometry::SolveOptimizedPolygon(Input);

		bool bMatched = true;
		auto Exact = [&Test, Label, &bMatched](const bool bCondition, const TCHAR* Field)
		{
			if (!bCondition)
			{
				Test.AddError(FString::Printf(TEXT("%s incremental/full mismatch: %s"), Label, Field));
				bMatched = false;
			}
		};
		Exact(Full.bSucceeded, TEXT("fresh full solve failed"));
		Exact(Entry->CandidateSegmentCount == Full.CandidateSegmentCount, TEXT("candidate segment count"));
		Exact(Entry->CandidateRayCount == Full.CastRayCount, TEXT("candidate ray count"));
		Exact(Entry->CandidateAnglesRadians == Full.CandidateAnglesRadians, TEXT("candidate angles"));
		Exact(Entry->CandidateDistances == Full.CandidateDistances, TEXT("candidate distances"));
		Exact(Entry->CandidateBoundaryPoints == Full.CandidateBoundaryPoints, TEXT("candidate boundary points"));
		Exact(Entry->Polygon.Vertices == Full.Vertices, TEXT("canonical vertices"));
		return bMatched;
	}

	bool RegisterCommonScene(
		FAutomationTestBase& Test,
		USightWeaveWorldSubsystem* Subsystem,
		const TArray<FSightWeaveSegment2D>& InitialDoor,
		FSightWeaveOccluderHandle& OutDoor,
		FSightWeaveVisionSourceHandle& OutVision,
		const FVector VisionLocation = FVector(0.0, 0.0, 100.0))
	{
		if (!Test.TestNotNull(TEXT("Subsystem exists"), Subsystem)
			|| !Test.TestTrue(TEXT("Ground floor registers"), Subsystem->RegisterFloor(Floor(), nullptr))
			|| !Test.TestTrue(
				TEXT("Static room registers"),
				Subsystem->RegisterOccluder(RoomSegments(), false, true, nullptr).IsValid()))
		{
			return false;
		}
		OutDoor = Subsystem->RegisterOccluder(InitialDoor, true, true, nullptr);
		OutVision = Subsystem->RegisterVisionSource(Vision(VisionLocation), nullptr);
		return Test.TestTrue(TEXT("Dynamic door registers"), OutDoor.IsValid())
			&& Test.TestTrue(TEXT("Vision source registers"), OutVision.IsValid());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P4DynamicSectorSingleSegmentTest,
	"SightWeave.M2P4.DynamicSector.SingleSegmentExact",
	SightWeave::M2P4::DynamicSectorTests::TestFlags)

bool FSightWeaveM2P4DynamicSectorSingleSegmentTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P4::DynamicSectorTests;
	USightWeaveSettings* Settings = GetMutableDefault<USightWeaveSettings>();
	TGuardValue<ESightWeaveSolverMode> ModeGuard(Settings->SolverMode, ESightWeaveSolverMode::Optimized);
	FTestWorld World(TEXT("SightWeaveM2P4SingleSegment"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	FSightWeaveOccluderHandle DoorHandle;
	FSightWeaveVisionSourceHandle VisionHandle;
	if (!RegisterCommonScene(*this, Subsystem, Door(250.0), DoorHandle, VisionHandle))
	{
		return true;
	}

	TestTrue(TEXT("Door translates outward"), Subsystem->UpdateOccluder(DoorHandle, Door(850.0), true, true));
	const FSightWeaveDynamicUpdateStageMetrics& Outward = Subsystem->GetLastDynamicUpdateStageMetrics();
	TestEqual(TEXT("One incremental attempt"), Outward.VisionIncrementalAttemptCount, int64(1));
	TestEqual(TEXT("Incremental solve succeeds"), Outward.VisionIncrementalSuccessCount, int64(1));
	TestEqual(TEXT("No full fallback"), Outward.VisionIncrementalFallbackCount, int64(0));
	TestTrue(TEXT("Dirty sector is measured"), Outward.VisionIncrementalLastDirtyRadians > 0.0);
	TestTrue(TEXT("Maximum dirty sector is measured"), Outward.VisionIncrementalMaximumDirtyRadians > 0.0);
	TestTrue(TEXT("Unchanged rays are reused"), Outward.VisionIncrementalReusedRayCount > 0);
	TestTrue(TEXT("Dirty rays are rebuilt"), Outward.VisionIncrementalRebuiltRayCount > 0);
	TestTrue(TEXT("No fallback reason on success"),
		Outward.VisionIncrementalLastFallbackReason == ESightWeaveIncrementalSectorFallbackReason::None);
	SnapshotMatchesFreshFullSolve(*this, *Subsystem, VisionHandle, TEXT("outward"));

	TestTrue(TEXT("Door translates inward"), Subsystem->UpdateOccluder(DoorHandle, Door(250.0), true, true));
	const FSightWeaveDynamicUpdateStageMetrics& Inward = Subsystem->GetLastDynamicUpdateStageMetrics();
	TestEqual(TEXT("Return incremental solve succeeds"), Inward.VisionIncrementalSuccessCount, int64(1));
	TestEqual(TEXT("Return has no full fallback"), Inward.VisionIncrementalFallbackCount, int64(0));
	SnapshotMatchesFreshFullSolve(*this, *Subsystem, VisionHandle, TEXT("inward"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P4DynamicSectorCyclicSeamTest,
	"SightWeave.M2P4.DynamicSector.CyclicSeamExact",
	SightWeave::M2P4::DynamicSectorTests::TestFlags)

bool FSightWeaveM2P4DynamicSectorCyclicSeamTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P4::DynamicSectorTests;
	USightWeaveSettings* Settings = GetMutableDefault<USightWeaveSettings>();
	TGuardValue<ESightWeaveSolverMode> ModeGuard(Settings->SolverMode, ESightWeaveSolverMode::Optimized);
	FTestWorld World(TEXT("SightWeaveM2P4CyclicSeam"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	FSightWeaveOccluderHandle DoorHandle;
	FSightWeaveVisionSourceHandle VisionHandle;
	if (!RegisterCommonScene(*this, Subsystem, Door(-250.0), DoorHandle, VisionHandle))
	{
		return true;
	}

	TestTrue(TEXT("Seam door translates outward"), Subsystem->UpdateOccluder(DoorHandle, Door(-850.0), true, true));
	const FSightWeaveDynamicUpdateStageMetrics& Metrics = Subsystem->GetLastDynamicUpdateStageMetrics();
	TestEqual(TEXT("Seam update attempts incremental solve"), Metrics.VisionIncrementalAttemptCount, int64(1));
	TestEqual(TEXT("Seam update succeeds incrementally"), Metrics.VisionIncrementalSuccessCount, int64(1));
	TestEqual(TEXT("Seam update avoids fallback"), Metrics.VisionIncrementalFallbackCount, int64(0));
	TestTrue(TEXT("Seam update reuses cyclic neighbors"), Metrics.VisionIncrementalReusedRayCount > 0);
	SnapshotMatchesFreshFullSolve(*this, *Subsystem, VisionHandle, TEXT("cyclic seam"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P4DynamicSectorMultipleChangeFallbackTest,
	"SightWeave.M2P4.DynamicSector.MultipleChangeFallback",
	SightWeave::M2P4::DynamicSectorTests::TestFlags)

bool FSightWeaveM2P4DynamicSectorMultipleChangeFallbackTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P4::DynamicSectorTests;
	USightWeaveSettings* Settings = GetMutableDefault<USightWeaveSettings>();
	TGuardValue<ESightWeaveSolverMode> ModeGuard(Settings->SolverMode, ESightWeaveSolverMode::Optimized);
	FTestWorld World(TEXT("SightWeaveM2P4MultipleChange"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	FSightWeaveOccluderHandle DoorHandle;
	FSightWeaveVisionSourceHandle VisionHandle;
	if (!RegisterCommonScene(*this, Subsystem, SplitDoor(250.0), DoorHandle, VisionHandle))
	{
		return true;
	}

	TestTrue(TEXT("Both door edges translate"), Subsystem->UpdateOccluder(DoorHandle, SplitDoor(850.0), true, true));
	const FSightWeaveDynamicUpdateStageMetrics& Metrics = Subsystem->GetLastDynamicUpdateStageMetrics();
	TestEqual(TEXT("Fallback update attempts incremental solve"), Metrics.VisionIncrementalAttemptCount, int64(1));
	TestEqual(TEXT("Fallback update has no incremental success"), Metrics.VisionIncrementalSuccessCount, int64(0));
	TestEqual(TEXT("Fallback update executes one full solve"), Metrics.VisionIncrementalFallbackCount, int64(1));
	TestTrue(TEXT("Multiple changed edges are diagnosed"),
		Metrics.VisionIncrementalLastFallbackReason
			== ESightWeaveIncrementalSectorFallbackReason::MultipleChangedSegments);
	TestTrue(TEXT("Full fallback time is recorded"), Metrics.VisionIncrementalFallbackMicroseconds >= 0.0);
	SnapshotMatchesFreshFullSolve(*this, *Subsystem, VisionHandle, TEXT("multiple change fallback"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P4DynamicSectorPreparedIndexMissingTest,
	"SightWeave.M2P4.DynamicSector.PreparedIndexMissingFallback",
	SightWeave::M2P4::DynamicSectorTests::TestFlags)

bool FSightWeaveM2P4DynamicSectorPreparedIndexMissingTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P4::DynamicSectorTests;
	USightWeaveSettings* Settings = GetMutableDefault<USightWeaveSettings>();
	TGuardValue<ESightWeaveSolverMode> ModeGuard(Settings->SolverMode, ESightWeaveSolverMode::Optimized);
	FTestWorld World(TEXT("SightWeaveM2P4PreparedMissing"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (!TestNotNull(TEXT("Subsystem exists"), Subsystem)
		|| !TestTrue(TEXT("Ground floor registers"), Subsystem->RegisterFloor(Floor(), nullptr))
		|| !TestTrue(TEXT("One-slot prepared index config applies"),
			Subsystem->ConfigurePreparedEventIndexForTesting(1, 1024ll * 1024ll))
		|| !TestTrue(TEXT("Static room registers"),
			Subsystem->RegisterOccluder(RoomSegments(), false, true, nullptr).IsValid()))
	{
		return true;
	}
	const FSightWeaveOccluderHandle DoorHandle =
		Subsystem->RegisterOccluder(Door(250.0), true, true, nullptr);
	const FSightWeaveVisionSourceHandle A =
		Subsystem->RegisterVisionSource(Vision(FVector(0.0, 0.0, 100.0)), nullptr);
	const FSightWeaveVisionSourceHandle B =
		Subsystem->RegisterVisionSource(Vision(FVector(100.0, 0.0, 100.0)), nullptr);
	if (!TestTrue(TEXT("Dynamic door registers"), DoorHandle.IsValid())
		|| !TestTrue(TEXT("First vision registers"), A.IsValid())
		|| !TestTrue(TEXT("Second vision registers through full fallback"), B.IsValid()))
	{
		return true;
	}

	TestTrue(TEXT("Capacity-pressure door update succeeds"),
		Subsystem->UpdateOccluder(DoorHandle, Door(850.0), true, true));
	const FSightWeaveDynamicUpdateStageMetrics& Metrics = Subsystem->GetLastDynamicUpdateStageMetrics();
	TestEqual(TEXT("Both sources attempt dynamic-sector publication"), Metrics.VisionIncrementalAttemptCount, int64(2));
	TestEqual(TEXT("Retained binding succeeds incrementally"), Metrics.VisionIncrementalSuccessCount, int64(1));
	TestEqual(TEXT("Unbound source falls back exactly once"), Metrics.VisionIncrementalFallbackCount, int64(1));
	TestTrue(TEXT("Missing prepared binding is diagnosed"),
		Metrics.VisionIncrementalLastFallbackReason
			== ESightWeaveIncrementalSectorFallbackReason::PreparedIndexMissing);
	SnapshotMatchesFreshFullSolve(*this, *Subsystem, A, TEXT("retained binding"));
	SnapshotMatchesFreshFullSolve(*this, *Subsystem, B, TEXT("missing binding fallback"));
	return true;
}

#endif