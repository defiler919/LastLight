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

	TArray<FSightWeaveSegment2D> DoorSpan(const double X, const double HalfHeight)
	{
		return { Segment(FVector2D(X, -HalfHeight), FVector2D(X, HalfHeight)) };
	}

	TArray<FSightWeaveSegment2D> DoorEndpoints(const FVector2D A, const FVector2D B)
	{
		return { Segment(A, B) };
	}

	TArray<FSightWeaveSegment2D> SplitDoor(const double X)
	{
		return {
			Segment(FVector2D(X, -160.0), FVector2D(X, -20.0)),
			Segment(FVector2D(X, 20.0), FVector2D(X, 160.0)) };
	}

	FSightWeaveVisionSourceDescription Vision(
		const FVector Location = FVector(0.0, 0.0, 100.0),
		const ESightWeaveSourceShape Shape = ESightWeaveSourceShape::Radial)
	{
		FSightWeaveVisionSourceDescription Result;
		Result.KnowledgeOwnerId = Local;
		Result.FloorId = Ground;
		Result.Transform = FTransform(FQuat::Identity, Location);
		Result.Shape = Shape;
		Result.Range = 1200.0f;
		Result.HalfAngleDegrees = Shape == ESightWeaveSourceShape::Radial ? 180.0f : 55.0f;
		Result.NearAwarenessRadius = Shape == ESightWeaveSourceShape::Radial ? 0.0f : 75.0f;
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		return Result;
	}

	FSightWeaveIlluminationSourceDescription Illumination(
		const FVector Location = FVector(0.0, 0.0, 100.0))
	{
		FSightWeaveIlluminationSourceDescription Result;
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
	TestEqual(TEXT("Cold target has no exact-result reuse"), Outward.VisionExactResultReuseCount, int64(0));
	TestTrue(TEXT("Dirty sector is measured"), Outward.VisionIncrementalLastDirtyRadians > 0.0);
	TestTrue(TEXT("Maximum dirty sector is measured"), Outward.VisionIncrementalMaximumDirtyRadians > 0.0);
	TestTrue(TEXT("Unchanged rays are reused"), Outward.VisionIncrementalReusedRayCount > 0);
	TestTrue(TEXT("Dirty rays are rebuilt"), Outward.VisionIncrementalRebuiltRayCount > 0);
	TestTrue(TEXT("No fallback reason on success"),
		Outward.VisionIncrementalLastFallbackReason == ESightWeaveIncrementalSectorFallbackReason::None);
	SnapshotMatchesFreshFullSolve(*this, *Subsystem, VisionHandle, TEXT("outward"));

	TestTrue(TEXT("Door translates inward"), Subsystem->UpdateOccluder(DoorHandle, Door(250.0), true, true));
	const FSightWeaveDynamicUpdateStageMetrics& Inward = Subsystem->GetLastDynamicUpdateStageMetrics();
	TestEqual(TEXT("Resident return skips incremental solve"), Inward.VisionIncrementalAttemptCount, int64(0));
	TestEqual(TEXT("Resident return reuses one exact result"), Inward.VisionExactResultReuseCount, int64(1));
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
	FSightWeaveM2P5SharedPreparedCacheDynamicSectorTest,
	"SightWeave.M2P5.VisionTail.SharedPreparedCacheDynamicSectorExact",
	SightWeave::M2P4::DynamicSectorTests::TestFlags)

bool FSightWeaveM2P5SharedPreparedCacheDynamicSectorTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P4::DynamicSectorTests;
	USightWeaveSettings* Settings = GetMutableDefault<USightWeaveSettings>();
	TGuardValue<ESightWeaveSolverMode> ModeGuard(Settings->SolverMode, ESightWeaveSolverMode::Optimized);
	FTestWorld World(TEXT("SightWeaveM2P5SharedPreparedCache"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	FSightWeaveOccluderHandle DoorHandle;
	FSightWeaveVisionSourceHandle VisionHandle;
	if (!RegisterCommonScene(*this, Subsystem, Door(250.0), DoorHandle, VisionHandle))
	{
		return true;
	}
	const FSightWeaveIlluminationSourceHandle IlluminationHandle =
		Subsystem->RegisterIlluminationSource(Illumination(), nullptr);
	if (!TestTrue(TEXT("Geometry-compatible illumination registers"), IlluminationHandle.IsValid()))
	{
		return true;
	}

	for (int32 UpdateIndex = 0; UpdateIndex < 6; ++UpdateIndex)
	{
		const bool bOutward = (UpdateIndex % 2) == 0;
		const double DoorX = bOutward ? 850.0 : 250.0;
		const FString Prefix = FString::Printf(TEXT("shared-cache update %d"), UpdateIndex + 1);
		TestTrue(
			*FString::Printf(TEXT("%s publishes"), *Prefix),
			Subsystem->UpdateOccluder(DoorHandle, Door(DoorX), true, true));
		const FSightWeaveDynamicUpdateStageMetrics& Metrics =
			Subsystem->GetLastDynamicUpdateStageMetrics();
		const bool bColdTarget = UpdateIndex == 0;
		TestEqual(
			*FString::Printf(TEXT("%s records the expected incremental attempts"), *Prefix),
			Metrics.VisionIncrementalAttemptCount,
			bColdTarget ? int64(1) : int64(0));
		TestEqual(
			*FString::Printf(TEXT("%s records the expected incremental successes"), *Prefix),
			Metrics.VisionIncrementalSuccessCount,
			bColdTarget ? int64(1) : int64(0));
		TestEqual(
			*FString::Printf(TEXT("%s records the expected exact-result reuse"), *Prefix),
			Metrics.VisionExactResultReuseCount,
			bColdTarget ? int64(0) : int64(1));
		TestEqual(
			*FString::Printf(TEXT("%s avoids full fallback"), *Prefix),
			Metrics.VisionIncrementalFallbackCount,
			int64(0));
		TestTrue(
			*FString::Printf(TEXT("%s has no fallback reason"), *Prefix),
			Metrics.VisionIncrementalLastFallbackReason
				== ESightWeaveIncrementalSectorFallbackReason::None);
		if (bColdTarget)
		{
			TestTrue(
				*FString::Printf(TEXT("%s reuses unchanged rays"), *Prefix),
				Metrics.VisionIncrementalReusedRayCount > 0);
			TestTrue(
				*FString::Printf(TEXT("%s rebuilds dirty rays"), *Prefix),
				Metrics.VisionIncrementalRebuiltRayCount > 0);
		}
		SnapshotMatchesFreshFullSolve(*this, *Subsystem, VisionHandle, *Prefix);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P5BroadDynamicDoorExactTest,
	"SightWeave.M2P5.VisionTail.Broad4V2LDynamicDoorExact",
	SightWeave::M2P4::DynamicSectorTests::TestFlags)

bool FSightWeaveM2P5BroadDynamicDoorExactTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P4::DynamicSectorTests;
	USightWeaveSettings* Settings = GetMutableDefault<USightWeaveSettings>();
	TGuardValue<ESightWeaveSolverMode> ModeGuard(Settings->SolverMode, ESightWeaveSolverMode::Optimized);
	FTestWorld World(TEXT("SightWeaveM2P5Broad4V2L"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (!TestNotNull(TEXT("Subsystem exists"), Subsystem)
		|| !TestTrue(TEXT("Ground floor registers"), Subsystem->RegisterFloor(Floor(), nullptr))
		|| !TestTrue(TEXT("Static room registers"),
			Subsystem->RegisterOccluder(RoomSegments(), false, true, nullptr).IsValid()))
	{
		return true;
	}
	const FSightWeaveOccluderHandle DoorHandle =
		Subsystem->RegisterOccluder(Door(250.0), true, true, nullptr);
	TArray<FSightWeaveVisionSourceHandle> VisionHandles;
	for (int32 SourceIndex = 0; SourceIndex < 4; ++SourceIndex)
	{
		const double Angle = 2.0 * PI * SourceIndex / 4.0;
		const ESightWeaveSourceShape Shape = SourceIndex % 2 == 0
			? ESightWeaveSourceShape::Radial
			: ESightWeaveSourceShape::CameraCone;
		VisionHandles.Add(Subsystem->RegisterVisionSource(
			Vision(
				FVector(FMath::Cos(Angle) * 100.0, FMath::Sin(Angle) * 100.0, 100.0),
				Shape),
			nullptr));
		TestTrue(
			*FString::Printf(TEXT("Broad vision source %d registers"), SourceIndex + 1),
			VisionHandles.Last().IsValid());
	}
	const FSightWeaveIlluminationSourceHandle Visible =
		Subsystem->RegisterIlluminationSource(
			Illumination(FVector(100.0, 0.0, 100.0)), nullptr);
	const FSightWeaveIlluminationSourceHandle Infrared =
		Subsystem->RegisterIlluminationSource(
			Illumination(FVector(-100.0, 0.0, 100.0)), nullptr);
	if (!TestTrue(TEXT("Broad dynamic door registers"), DoorHandle.IsValid())
		|| !TestTrue(TEXT("Visible illumination registers"), Visible.IsValid())
		|| !TestTrue(TEXT("Infrared illumination registers"), Infrared.IsValid())
		|| VisionHandles.ContainsByPredicate([](const FSightWeaveVisionSourceHandle Handle)
			{ return !Handle.IsValid(); }))
	{
		return true;
	}

	const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> HeldSnapshot =
		Subsystem->AcquirePublishedSnapshotForTesting();
	const int64 HeldRevision = HeldSnapshot.IsValid() ? HeldSnapshot->Revision.GetValue() : 0;
	const TArray<FVector> HeldVertices = HeldSnapshot.IsValid()
		? FindVisionEntry(*HeldSnapshot, VisionHandles[0])->Polygon.Vertices
		: TArray<FVector>();
	TestTrue(TEXT("Broad test acquires immutable reader"), HeldSnapshot.IsValid());

	for (int32 UpdateIndex = 0; UpdateIndex < 8; ++UpdateIndex)
	{
		const double DoorX = UpdateIndex % 2 == 0 ? 850.0 : 250.0;
		const FString Prefix = FString::Printf(TEXT("broad update %d"), UpdateIndex + 1);
		TestTrue(*FString::Printf(TEXT("%s publishes"), *Prefix),
			Subsystem->UpdateOccluder(DoorHandle, Door(DoorX), true, true));
		const FSightWeaveDynamicUpdateStageMetrics& Metrics =
			Subsystem->GetLastDynamicUpdateStageMetrics();
		const bool bColdTarget = UpdateIndex == 0;
		TestEqual(*FString::Printf(TEXT("%s records expected incremental attempts"), *Prefix),
			Metrics.VisionIncrementalAttemptCount, bColdTarget ? int64(4) : int64(0));
		TestEqual(*FString::Printf(TEXT("%s records expected incremental successes"), *Prefix),
			Metrics.VisionIncrementalSuccessCount, bColdTarget ? int64(4) : int64(0));
		TestEqual(*FString::Printf(TEXT("%s records expected exact-result reuse"), *Prefix),
			Metrics.VisionExactResultReuseCount, bColdTarget ? int64(0) : int64(4));
		TestEqual(*FString::Printf(TEXT("%s has no fallback"), *Prefix),
			Metrics.VisionIncrementalFallbackCount, int64(0));
		TestEqual(*FString::Printf(TEXT("%s records four source diagnostics"), *Prefix),
			Metrics.VisionSourceDiagnosticCount, 4);
		TestEqual(*FString::Printf(TEXT("%s has no diagnostic overflow"), *Prefix),
			Metrics.VisionSourceDiagnosticOverflowCount, 0);
		if (bColdTarget)
		{
			TestTrue(*FString::Printf(TEXT("%s reuses unchanged rays"), *Prefix),
				Metrics.VisionIncrementalReusedRayCount > 0);
			TestTrue(*FString::Printf(TEXT("%s rebuilds dirty rays"), *Prefix),
				Metrics.VisionIncrementalRebuiltRayCount > 0);
		}
		int64 PreviousSourceId = 0;
		for (int32 SourceIndex = 0; SourceIndex < Metrics.VisionSourceDiagnosticCount; ++SourceIndex)
		{
			const FSightWeaveVisionSourceSolveDiagnostics& Diagnostic =
				Metrics.VisionSourceDiagnostics[SourceIndex];
			TestTrue(*FString::Printf(TEXT("%s source IDs are ordered"), *Prefix),
				Diagnostic.SourceId > PreviousSourceId);
			TestTrue(*FString::Printf(TEXT("%s source has no fallback reason"), *Prefix),
				Diagnostic.FallbackReason == ESightWeaveIncrementalSectorFallbackReason::None);
			TestEqual(*FString::Printf(TEXT("%s source exact-result state is explicit"), *Prefix),
				Diagnostic.bExactResultReused, !bColdTarget);
			if (bColdTarget)
			{
				TestTrue(*FString::Printf(TEXT("%s source reuses rays"), *Prefix),
					Diagnostic.ReusedRayCount > 0);
				TestTrue(*FString::Printf(TEXT("%s source rebuilds rays"), *Prefix),
					Diagnostic.RebuiltRayCount > 0);
			}
			PreviousSourceId = Diagnostic.SourceId;
		}
		for (int32 SourceIndex = 0; SourceIndex < VisionHandles.Num(); ++SourceIndex)
		{
			SnapshotMatchesFreshFullSolve(
				*this,
				*Subsystem,
				VisionHandles[SourceIndex],
				*FString::Printf(TEXT("%s source %d"), *Prefix, SourceIndex + 1));
		}
	}
	TestEqual(TEXT("Held reader revision remains immutable"),
		HeldSnapshot->Revision.GetValue(), HeldRevision);
	const FSightWeaveVisionSnapshotEntry* HeldVision =
		FindVisionEntry(*HeldSnapshot, VisionHandles[0]);
	TestTrue(TEXT("Held reader retains original polygon"),
		HeldVision && HeldVision->Polygon.Vertices == HeldVertices);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P5DynamicDoorMovementMatrixTest,
	"SightWeave.M2P5.VisionTail.DynamicDoorMovementMatrixExact",
	SightWeave::M2P4::DynamicSectorTests::TestFlags)

bool FSightWeaveM2P5DynamicDoorMovementMatrixTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P4::DynamicSectorTests;
	USightWeaveSettings* Settings = GetMutableDefault<USightWeaveSettings>();
	TGuardValue<ESightWeaveSolverMode> ModeGuard(Settings->SolverMode, ESightWeaveSolverMode::Optimized);
	struct FMovementCase
	{
		const TCHAR* Label;
		TArray<FSightWeaveSegment2D> Initial;
		TArray<FSightWeaveSegment2D> Updated;
	};
	const TArray<FMovementCase> Cases = {
		{ TEXT("one_centimeter"), Door(250.0), Door(251.0) },
		{ TEXT("five_centimeters"), Door(250.0), Door(255.0) },
		{ TEXT("twenty_centimeters"), Door(250.0), Door(270.0) },
		{ TEXT("narrow_door"), DoorSpan(250.0, 100.0), DoorSpan(250.0, 20.0) },
		{ TEXT("wide_door"), DoorSpan(250.0, 100.0), DoorSpan(250.0, 300.0) },
		{ TEXT("teleport"), Door(250.0), Door(850.0) },
		{ TEXT("rotation"), Door(250.0),
			DoorEndpoints(FVector2D(200.0, -100.0), FVector2D(300.0, 100.0)) } };
	for (const FMovementCase& Movement : Cases)
	{
		FTestWorld World(*FString::Printf(TEXT("SightWeaveM2P5Movement_%s"), Movement.Label));
		USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
		FSightWeaveOccluderHandle DoorHandle;
		FSightWeaveVisionSourceHandle VisionHandle;
		if (!RegisterCommonScene(
				*this, Subsystem, Movement.Initial, DoorHandle, VisionHandle)
			|| !TestTrue(
				*FString::Printf(TEXT("%s shared illumination registers"), Movement.Label),
				Subsystem->RegisterIlluminationSource(Illumination(), nullptr).IsValid()))
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("%s publishes"), Movement.Label),
			Subsystem->UpdateOccluder(DoorHandle, Movement.Updated, true, true));
		const FSightWeaveDynamicUpdateStageMetrics& Metrics =
			Subsystem->GetLastDynamicUpdateStageMetrics();
		TestEqual(*FString::Printf(TEXT("%s attempts incremental solve"), Movement.Label),
			Metrics.VisionIncrementalAttemptCount, int64(1));
		TestEqual(*FString::Printf(TEXT("%s succeeds incrementally"), Movement.Label),
			Metrics.VisionIncrementalSuccessCount, int64(1));
		TestEqual(*FString::Printf(TEXT("%s avoids fallback"), Movement.Label),
			Metrics.VisionIncrementalFallbackCount, int64(0));
		TestTrue(*FString::Printf(TEXT("%s has no fallback reason"), Movement.Label),
			Metrics.VisionIncrementalLastFallbackReason
				== ESightWeaveIncrementalSectorFallbackReason::None);
		TestTrue(*FString::Printf(TEXT("%s reuses rays"), Movement.Label),
			Metrics.VisionIncrementalReusedRayCount > 0);
		TestTrue(*FString::Printf(TEXT("%s rebuilds rays"), Movement.Label),
			Metrics.VisionIncrementalRebuiltRayCount > 0);
		SnapshotMatchesFreshFullSolve(*this, *Subsystem, VisionHandle, Movement.Label);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P5FixedSeedIncrementalDifferentialTest,
	"SightWeave.M2P5.VisionTail.FixedSeedIncrementalDifferential",
	SightWeave::M2P4::DynamicSectorTests::TestFlags)

bool FSightWeaveM2P5FixedSeedIncrementalDifferentialTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P4::DynamicSectorTests;
	USightWeaveSettings* Settings = GetMutableDefault<USightWeaveSettings>();
	TGuardValue<ESightWeaveSolverMode> ModeGuard(Settings->SolverMode, ESightWeaveSolverMode::Optimized);
	FRandomStream Random(0x5A17E5);
	TArray<TArray<FSightWeaveSegment2D>> States;
	States.Reserve(24);
	for (int32 StateIndex = 0; StateIndex < 24; ++StateIndex)
	{
		const double X = Random.FRandRange(220.0f, 850.0f);
		const double HalfHeight = Random.FRandRange(20.0f, 300.0f);
		const double Shear = Random.FRandRange(-50.0f, 50.0f);
		States.Add(DoorEndpoints(
			FVector2D(X - Shear, -HalfHeight),
			FVector2D(X + Shear, HalfHeight)));
	}

	FTestWorld World(TEXT("SightWeaveM2P5FixedSeedIncremental"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	FSightWeaveOccluderHandle DoorHandle;
	FSightWeaveVisionSourceHandle VisionHandle;
	if (!RegisterCommonScene(*this, Subsystem, States[0], DoorHandle, VisionHandle)
		|| !TestTrue(TEXT("Fixed-seed shared illumination registers"),
			Subsystem->RegisterIlluminationSource(Illumination(), nullptr).IsValid()))
	{
		return true;
	}

	TArray<TArray<FVector>> WarmVertices;
	WarmVertices.SetNum(States.Num());
	{
		const FSightWeaveFrameSnapshot InitialSnapshot = Subsystem->GetPublishedSnapshot();
		const FSightWeaveVisionSnapshotEntry* InitialEntry =
			FindVisionEntry(InitialSnapshot, VisionHandle);
		WarmVertices[0] = InitialEntry ? InitialEntry->Polygon.Vertices : TArray<FVector>();
	}
	for (int32 StateIndex = 1; StateIndex < States.Num(); ++StateIndex)
	{
		TestTrue(TEXT("Fixed-seed warm publication succeeds"),
			Subsystem->UpdateOccluder(DoorHandle, States[StateIndex], true, true));
		const FSightWeaveDynamicUpdateStageMetrics& Metrics =
			Subsystem->GetLastDynamicUpdateStageMetrics();
		TestEqual(TEXT("Fixed-seed warm path succeeds incrementally"),
			Metrics.VisionIncrementalSuccessCount, int64(1));
		TestEqual(TEXT("Fixed-seed cold target does not reuse an exact result"),
			Metrics.VisionExactResultReuseCount, int64(0));
		TestEqual(TEXT("Fixed-seed warm path has no fallback"),
			Metrics.VisionIncrementalFallbackCount, int64(0));
		const FSightWeaveFrameSnapshot Snapshot = Subsystem->GetPublishedSnapshot();
		const FSightWeaveVisionSnapshotEntry* Entry =
			FindVisionEntry(Snapshot, VisionHandle);
		WarmVertices[StateIndex] = Entry ? Entry->Polygon.Vertices : TArray<FVector>();
		SnapshotMatchesFreshFullSolve(
			*this,
			*Subsystem,
			VisionHandle,
			*FString::Printf(TEXT("fixed-seed warm state %d"), StateIndex));
	}
	const FSightWeavePreparedEventIndexStats WarmPrepared =
		Subsystem->GetPreparedEventIndexStats();

	for (int32 StateIndex = 0; StateIndex < States.Num(); ++StateIndex)
	{
		TestTrue(TEXT("Fixed-seed replay publication succeeds"),
			Subsystem->UpdateOccluder(DoorHandle, States[StateIndex], true, true));
		const FSightWeaveDynamicUpdateStageMetrics& Metrics =
			Subsystem->GetLastDynamicUpdateStageMetrics();
		TestEqual(TEXT("Fixed-seed replay skips incremental solve"),
			Metrics.VisionIncrementalAttemptCount, int64(0));
		TestEqual(TEXT("Fixed-seed replay reuses one exact result"),
			Metrics.VisionExactResultReuseCount, int64(1));
		TestEqual(TEXT("Fixed-seed replay path has no fallback"),
			Metrics.VisionIncrementalFallbackCount, int64(0));
		const FSightWeaveFrameSnapshot Snapshot = Subsystem->GetPublishedSnapshot();
		const FSightWeaveVisionSnapshotEntry* Entry =
			FindVisionEntry(Snapshot, VisionHandle);
		TestTrue(TEXT("Fixed-seed polygon is bitwise deterministic"),
			Entry && Entry->Polygon.Vertices == WarmVertices[StateIndex]);
		SnapshotMatchesFreshFullSolve(
			*this,
			*Subsystem,
			VisionHandle,
			*FString::Printf(TEXT("fixed-seed replay state %d"), StateIndex));
	}
	const FSightWeavePreparedEventIndexStats ReplayPrepared =
		Subsystem->GetPreparedEventIndexStats();
	TestEqual(TEXT("Prepared entries do not rebuild after fixed-seed warmup"),
		ReplayPrepared.FullRebuildCount, WarmPrepared.FullRebuildCount);
	TestEqual(TEXT("Every fixed-seed replay is an exact-result hit"),
		ReplayPrepared.ExactResultHitCount,
		WarmPrepared.ExactResultHitCount + States.Num());
	TestEqual(TEXT("Prepared high-water bytes do not grow after fixed-seed warmup"),
		ReplayPrepared.HighWaterAllocatedBytes, WarmPrepared.HighWaterAllocatedBytes);
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
