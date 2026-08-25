#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "SightWeaveWorldSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace SightWeave::M2P2::MotionTests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	const FSightWeaveFloorId Ground(FName(TEXT("Ground")));
	const FSightWeaveFloorId Upper(FName(TEXT("Upper")));
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

	FSightWeaveFloorDefinition Floor(
		const FSightWeaveFloorId FloorId,
		const bool bActive,
		const float ZMin,
		const float ZMax)
	{
		FSightWeaveFloorDefinition Result;
		Result.FloorId = FloorId;
		Result.BoundsMin = FVector2D(-20000.0, -20000.0);
		Result.BoundsMax = FVector2D(20000.0, 20000.0);
		Result.HeightRange.ZMin = ZMin;
		Result.HeightRange.ZMax = ZMax;
		Result.bActiveForQueries = bActive;
		return Result;
	}

	FSightWeaveSegment2D Segment(
		const FVector2D A,
		const FVector2D B,
		const FSightWeaveFloorId FloorId = Ground,
		const float ZMin = 0.0f,
		const float ZMax = 300.0f)
	{
		FSightWeaveSegment2D Result;
		Result.A = A;
		Result.B = B;
		Result.FloorId = FloorId;
		Result.HeightRange.ZMin = ZMin;
		Result.HeightRange.ZMax = ZMax;
		return Result;
	}

	TArray<FSightWeaveSegment2D> StaticScene()
	{
		TArray<FSightWeaveSegment2D> Result;
		Result.Reserve(128);
		// Four rooms and a long wall provide lateral motion, doorway, endpoint-order
		// crossing, diagonal, and room-transition paths without invalid intersections.
		for (int32 Room = -2; Room <= 1; ++Room)
		{
			const double X0 = Room * 1400.0;
			const double X1 = X0 + 1200.0;
			Result.Add(Segment(FVector2D(X0, -700.0), FVector2D(X1, -700.0)));
			Result.Add(Segment(FVector2D(X0, 700.0), FVector2D(X1, 700.0)));
			Result.Add(Segment(FVector2D(X0, -700.0), FVector2D(X0, 700.0)));
			Result.Add(Segment(FVector2D(X1, -700.0), FVector2D(X1, -120.0)));
			Result.Add(Segment(FVector2D(X1, 120.0), FVector2D(X1, 700.0)));
		}
		for (int32 Index = 0; Index < 48; ++Index)
		{
			const double Angle = 2.0 * PI * static_cast<double>(Index) / 48.0;
			const double Radius = 900.0 + 12.0 * Index;
			const FVector2D Center(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius);
			const FVector2D Tangent(-FMath::Sin(Angle) * 18.0, FMath::Cos(Angle) * 18.0);
			Result.Add(Segment(Center - Tangent, Center + Tangent));
		}
		return Result;
	}

	FSightWeaveVisionSourceDescription Vision(
		const ESightWeaveSourceShape Shape,
		const ESightWeaveIlluminationPolicy Policy,
		const FSightWeaveKnowledgeOwnerId Owner = Local,
		const FSightWeaveFloorId FloorId = Ground,
		const float ZMin = 0.0f,
		const float ZMax = 300.0f)
	{
		FSightWeaveVisionSourceDescription Result;
		Result.Transform.SetLocation(FVector(0.0, 0.0, 100.0));
		Result.KnowledgeOwnerId = Owner;
		Result.FloorId = FloorId;
		Result.HeightRange.ZMin = ZMin;
		Result.HeightRange.ZMax = ZMax;
		Result.Shape = Shape;
		Result.Range = 1200.0f;
		Result.HalfAngleDegrees = Shape == ESightWeaveSourceShape::Radial ? 180.0f : 55.0f;
		Result.NearAwarenessRadius = Shape == ESightWeaveSourceShape::CameraCone ? 75.0f : 0.0f;
		Result.IlluminationPolicy = Policy;
		if (Policy == ESightWeaveIlluminationPolicy::RequiresLegalIllumination)
		{
			Result.Compatibility.AcceptedCapabilities = {
				FName(TEXT("Infrared")), FName(TEXT("Visible")) };
		}
		return Result;
	}

	FSightWeaveIlluminationSourceDescription Light(
		const ESightWeaveSourceShape Shape,
		const FName Capability,
		const float Range = 1200.0f,
		const float ZMin = 0.0f,
		const float ZMax = 300.0f)
	{
		FSightWeaveIlluminationSourceDescription Result;
		Result.Transform.SetLocation(FVector(0.0, 0.0, 100.0));
		Result.KnowledgeOwnerId = Local;
		Result.FloorId = Ground;
		Result.HeightRange.ZMin = ZMin;
		Result.HeightRange.ZMax = ZMax;
		Result.Shape = Shape;
		Result.Range = Range;
		Result.HalfAngleDegrees = Shape == ESightWeaveSourceShape::Radial ? 180.0f : 55.0f;
		Result.EmittedCapabilities = { Capability };
		return Result;
	}

	uint64 SnapshotAllocatedBytes(const FSightWeaveFrameSnapshot& Snapshot)
	{
		uint64 Bytes = Snapshot.Floors.GetAllocatedSize()
			+ Snapshot.OccluderSegments.GetAllocatedSize()
			+ Snapshot.VisionSources.GetAllocatedSize()
			+ Snapshot.IlluminationSources.GetAllocatedSize()
			+ Snapshot.HardSuppressions.GetAllocatedSize();
		for (const FSightWeaveVisionSnapshotEntry& Entry : Snapshot.VisionSources)
		{
			Bytes += Entry.Description.Compatibility.AcceptedCapabilities.GetAllocatedSize();
			Bytes += Entry.Polygon.Vertices.GetAllocatedSize();
			Bytes += Entry.CompatibleIlluminationSources.GetAllocatedSize();
			Bytes += Entry.CompatibleIlluminationSourceIndices.GetAllocatedSize();
			Bytes += Entry.CandidateAnglesRadians.GetAllocatedSize();
			Bytes += Entry.CandidateDistances.GetAllocatedSize();
			Bytes += Entry.CandidateBoundaryPoints.GetAllocatedSize();
			Bytes += Entry.PolarAngleUpperBoundLut.GetAllocatedSize();
		}
		for (const FSightWeaveIlluminationSnapshotEntry& Entry : Snapshot.IlluminationSources)
		{
			Bytes += Entry.Description.EmittedCapabilities.GetAllocatedSize();
			Bytes += Entry.Polygon.Vertices.GetAllocatedSize();
			Bytes += Entry.CandidateAnglesRadians.GetAllocatedSize();
			Bytes += Entry.CandidateDistances.GetAllocatedSize();
			Bytes += Entry.CandidateBoundaryPoints.GetAllocatedSize();
			Bytes += Entry.PolarAngleUpperBoundLut.GetAllocatedSize();
		}
		return Bytes;
	}

	struct FMotionSamples
	{
		TArray<double> TotalMicroseconds;
		TArray<double> SolveMicroseconds;
		TArray<double> PublicationEstimateMicroseconds;
		int64 RevisionChanges = 0;
		int64 VisionRebuilds = 0;
		int64 IlluminationRebuilds = 0;
		int64 CandidateSegments = 0;
		int64 Events = 0;
		int64 ReusedEvents = 0;
		int64 Vertices = 0;
		int64 CacheHits = 0;
		int64 CacheMisses = 0;
		int64 PreparedHits = 0;
		int64 PreparedMisses = 0;
		int64 PreparedFullRebuilds = 0;
		int64 PreparedEvictions = 0;
		int64 PreparedCapacityFallbacks = 0;
		int64 PreparedLiveBytes = 0;
		int64 PreparedHighWaterBytes = 0;
		uint64 MaximumSnapshotBytes = 0;
		bool bSucceeded = true;
	};

	double SnapshotSolveMicroseconds(
		const FSightWeaveFrameSnapshot& Snapshot,
		const FSightWeaveVisionSourceHandle Handle)
	{
		const FSightWeaveVisionSnapshotEntry* Entry = Snapshot.VisionSources.FindByPredicate(
			[Handle](const FSightWeaveVisionSnapshotEntry& Candidate) { return Candidate.Handle == Handle; });
		return Entry ? Entry->SolveTimeMicroseconds : 0.0;
	}

	void AccumulatePreparedIndexDelta(
		FMotionSamples& Samples,
		const FSightWeavePreparedEventIndexStats& Before,
		const FSightWeavePreparedEventIndexStats& After)
	{
		Samples.PreparedHits += After.HitCount - Before.HitCount;
		Samples.PreparedMisses += After.MissCount - Before.MissCount;
		Samples.PreparedFullRebuilds += After.FullRebuildCount - Before.FullRebuildCount;
		Samples.PreparedEvictions += After.EvictionCount - Before.EvictionCount;
		Samples.PreparedCapacityFallbacks +=
			After.CapacityFallbackCount - Before.CapacityFallbackCount;
		Samples.PreparedLiveBytes = After.LiveAllocatedBytes;
		Samples.PreparedHighWaterBytes = After.HighWaterAllocatedBytes;
	}

	double SnapshotSolveMicroseconds(const FSightWeaveFrameSnapshot& Snapshot)
	{
		double TotalMicroseconds = 0.0;
		for (const FSightWeaveVisionSnapshotEntry& Entry : Snapshot.VisionSources)
		{
			TotalMicroseconds += Entry.SolveTimeMicroseconds;
		}
		for (const FSightWeaveIlluminationSnapshotEntry& Entry : Snapshot.IlluminationSources)
		{
			TotalMicroseconds += Entry.SolveTimeMicroseconds;
		}
		return TotalMicroseconds;
	}

	double SnapshotSolveMicroseconds(
		const FSightWeaveFrameSnapshot& Snapshot,
		const FSightWeaveIlluminationSourceHandle Handle)
	{
		const FSightWeaveIlluminationSnapshotEntry* Entry = Snapshot.IlluminationSources.FindByPredicate(
			[Handle](const FSightWeaveIlluminationSnapshotEntry& Candidate) { return Candidate.Handle == Handle; });
		return Entry ? Entry->SolveTimeMicroseconds : 0.0;
	}

	struct FEntryCounts
	{
		int64 Candidates = 0;
		int64 Events = 0;
		int64 Vertices = 0;
	};

	FEntryCounts EntryCounts(
		const FSightWeaveFrameSnapshot& Snapshot,
		const FSightWeaveVisionSourceHandle VisionHandle,
		const FSightWeaveIlluminationSourceHandle IlluminationHandle)
	{
		FEntryCounts Counts;
		for (const FSightWeaveVisionSnapshotEntry& Entry : Snapshot.VisionSources)
		{
			if (VisionHandle.IsValid() && Entry.Handle != VisionHandle)
			{
				continue;
			}
			if (IlluminationHandle.IsValid())
			{
				continue;
			}
			Counts.Candidates += Entry.CandidateSegmentCount;
			Counts.Events += Entry.CandidateRayCount;
			Counts.Vertices += Entry.Polygon.Vertices.Num();
		}
		for (const FSightWeaveIlluminationSnapshotEntry& Entry : Snapshot.IlluminationSources)
		{
			if (IlluminationHandle.IsValid() && Entry.Handle != IlluminationHandle)
			{
				continue;
			}
			if (VisionHandle.IsValid())
			{
				continue;
			}
			Counts.Candidates += Entry.CandidateSegmentCount;
			Counts.Events += Entry.CandidateRayCount;
			Counts.Vertices += Entry.Polygon.Vertices.Num();
		}
		return Counts;
	}

	void AccumulateSnapshotCounters(
		FMotionSamples& Samples,
		const FSightWeaveFrameSnapshot& Snapshot,
		const bool bPublished,
		const FSightWeaveVisionSourceHandle VisionHandle = FSightWeaveVisionSourceHandle(),
		const FSightWeaveIlluminationSourceHandle IlluminationHandle = FSightWeaveIlluminationSourceHandle())
	{
		const FEntryCounts Counts = EntryCounts(Snapshot, VisionHandle, IlluminationHandle);
		Samples.CandidateSegments += Counts.Candidates;
		Samples.Vertices += Counts.Vertices;
		if (bPublished)
		{
			Samples.VisionRebuilds += Snapshot.RebuiltVisionPolygonCount;
			Samples.IlluminationRebuilds += Snapshot.RebuiltIlluminationPolygonCount;
			Samples.CacheMisses += Snapshot.RebuiltVisionPolygonCount
				+ Snapshot.RebuiltIlluminationPolygonCount;
			Samples.Events += Counts.Events;
		}
		else
		{
			++Samples.CacheHits;
			Samples.ReusedEvents += Counts.Events;
		}
		Samples.MaximumSnapshotBytes = FMath::Max(
			Samples.MaximumSnapshotBytes,
			SnapshotAllocatedBytes(Snapshot));
	}

	void AccumulateSnapshot(
		FMotionSamples& Samples,
		const FSightWeaveFrameSnapshot& Snapshot,
		const double SolveMicroseconds,
		const double TotalMicroseconds,
		const bool bPublished,
		const FSightWeaveVisionSourceHandle VisionHandle = FSightWeaveVisionSourceHandle(),
		const FSightWeaveIlluminationSourceHandle IlluminationHandle = FSightWeaveIlluminationSourceHandle())
	{
		Samples.SolveMicroseconds.Add(SolveMicroseconds);
		Samples.PublicationEstimateMicroseconds.Add(FMath::Max(0.0, TotalMicroseconds - SolveMicroseconds));
		AccumulateSnapshotCounters(
			Samples,
			Snapshot,
			bPublished,
			VisionHandle,
			IlluminationHandle);
	}

	void LogMotion(
		FAutomationTestBase& Test,
		const TCHAR* Name,
		const FMotionSamples& Samples,
		const int32 Repeats,
		const TCHAR* Strategy)
	{
		const FDistribution Total = Distribution(Samples.TotalMicroseconds);
		const FDistribution Solve = Distribution(Samples.SolveMicroseconds);
		const FDistribution Publication = Distribution(Samples.PublicationEstimateMicroseconds);
		Test.AddInfo(FString::Printf(
			TEXT("M2P2_MOTION name=%s strategy=%s repeats=%d total_us=%.3f/%.3f/%.3f/%.3f source_cpu_us=%.3f/%.3f/%.3f/%.3f main_thread_us=%.3f/%.3f/%.3f/%.3f publication_estimate_us=%.3f/%.3f/%.3f/%.3f revision_changes=%lld vision_rebuilds=%lld illumination_rebuilds=%lld cache_hits=%lld cache_misses=%lld prepared_hits=%lld prepared_misses=%lld prepared_full_rebuilds=%lld prepared_evictions=%lld prepared_capacity_fallbacks=%lld prepared_live_bytes=%lld prepared_high_water_bytes=%lld reused_events=%lld rebuilt_events=%lld candidates_accumulated=%lld vertices_accumulated=%lld snapshot_bytes_max=%llu allocation_scope=separate_startup_trace"),
			Name,
			Strategy,
			Repeats,
			Total.Median, Total.P95, Total.P99, Total.Max,
			Solve.Median, Solve.P95, Solve.P99, Solve.Max,
			Total.Median, Total.P95, Total.P99, Total.Max,
			Publication.Median, Publication.P95, Publication.P99, Publication.Max,
			Samples.RevisionChanges,
			Samples.VisionRebuilds,
			Samples.IlluminationRebuilds,
			Samples.CacheHits,
			Samples.CacheMisses,
			Samples.PreparedHits,
			Samples.PreparedMisses,
			Samples.PreparedFullRebuilds,
			Samples.PreparedEvictions,
			Samples.PreparedCapacityFallbacks,
			Samples.PreparedLiveBytes,
			Samples.PreparedHighWaterBytes,
			Samples.ReusedEvents,
			Samples.Events,
			Samples.CandidateSegments,
			Samples.Vertices,
			Samples.MaximumSnapshotBytes));
	}

	template <typename MutatorType>
	FMotionSamples TimeVisionMotion(
		USightWeaveWorldSubsystem* Subsystem,
		const FSightWeaveVisionSourceHandle Handle,
		FSightWeaveVisionSourceDescription& Description,
		const int32 Warmups,
		const int32 Repeats,
		const bool bTransformOnly,
		MutatorType&& Mutator)
	{
		for (int32 Index = 0; Index < Warmups; ++Index)
		{
			Mutator(Index, Description);
			if (bTransformOnly)
			{
				Subsystem->UpdateVisionSourceTransform(Handle, Description.Transform);
			}
			else
			{
				Subsystem->UpdateVisionSource(Handle, Description);
			}
		}
		FMotionSamples Samples;
		const FSightWeavePreparedEventIndexStats PreparedBefore =
			Subsystem->GetPreparedEventIndexStats();
		Samples.TotalMicroseconds.Reserve(Repeats);
		Samples.SolveMicroseconds.Reserve(Repeats);
		Samples.PublicationEstimateMicroseconds.Reserve(Repeats);
		for (int32 Index = 0; Index < Repeats; ++Index)
		{
			Mutator(Index, Description);
			const FSightWeaveRevision Before = Subsystem->GetRevision();
			const double StartSeconds = FPlatformTime::Seconds();
			Samples.bSucceeded &= bTransformOnly
				? Subsystem->UpdateVisionSourceTransform(Handle, Description.Transform)
				: Subsystem->UpdateVisionSource(Handle, Description);
			const double TotalMicroseconds = (FPlatformTime::Seconds() - StartSeconds) * 1000000.0;
			Samples.TotalMicroseconds.Add(TotalMicroseconds);
			const bool bPublished = Subsystem->GetRevision() != Before;
			Samples.RevisionChanges += bPublished ? 1 : 0;
			const FSightWeaveFrameSnapshot Snapshot = Subsystem->GetPublishedSnapshot();
			AccumulateSnapshot(
				Samples,
				Snapshot,
				bPublished ? SnapshotSolveMicroseconds(Snapshot, Handle) : 0.0,
				TotalMicroseconds,
				bPublished,
				Handle);
		}
		AccumulatePreparedIndexDelta(
			Samples,
			PreparedBefore,
			Subsystem->GetPreparedEventIndexStats());
		return Samples;
	}

	template <typename MutatorType>
	FMotionSamples TimeIlluminationMotion(
		USightWeaveWorldSubsystem* Subsystem,
		const FSightWeaveIlluminationSourceHandle Handle,
		FSightWeaveIlluminationSourceDescription& Description,
		const int32 Warmups,
		const int32 Repeats,
		const bool bTransformOnly,
		MutatorType&& Mutator)
	{
		for (int32 Index = 0; Index < Warmups; ++Index)
		{
			Mutator(Index, Description);
			if (bTransformOnly)
			{
				Subsystem->UpdateIlluminationSourceTransform(Handle, Description.Transform);
			}
			else
			{
				Subsystem->UpdateIlluminationSource(Handle, Description);
			}
		}
		FMotionSamples Samples;
		const FSightWeavePreparedEventIndexStats PreparedBefore =
			Subsystem->GetPreparedEventIndexStats();
		Samples.TotalMicroseconds.Reserve(Repeats);
		for (int32 Index = 0; Index < Repeats; ++Index)
		{
			Mutator(Index, Description);
			const FSightWeaveRevision Before = Subsystem->GetRevision();
			const double StartSeconds = FPlatformTime::Seconds();
			Samples.bSucceeded &= bTransformOnly
				? Subsystem->UpdateIlluminationSourceTransform(Handle, Description.Transform)
				: Subsystem->UpdateIlluminationSource(Handle, Description);
			const double TotalMicroseconds = (FPlatformTime::Seconds() - StartSeconds) * 1000000.0;
			Samples.TotalMicroseconds.Add(TotalMicroseconds);
			const bool bPublished = Subsystem->GetRevision() != Before;
			Samples.RevisionChanges += bPublished ? 1 : 0;
			const FSightWeaveFrameSnapshot Snapshot = Subsystem->GetPublishedSnapshot();
			double SolveMicroseconds = 0.0;
			if (const FSightWeaveIlluminationSnapshotEntry* Entry = Snapshot.IlluminationSources.FindByPredicate(
				[Handle](const FSightWeaveIlluminationSnapshotEntry& Candidate) { return Candidate.Handle == Handle; }))
			{
				SolveMicroseconds = Entry->SolveTimeMicroseconds;
			}
			AccumulateSnapshot(
				Samples,
				Snapshot,
				bPublished ? SolveMicroseconds : 0.0,
				TotalMicroseconds,
				bPublished,
				FSightWeaveVisionSourceHandle(),
				Handle);
		}
		AccumulatePreparedIndexDelta(
			Samples,
			PreparedBefore,
			Subsystem->GetPreparedEventIndexStats());
		return Samples;
	}

	void SetYaw(FTransform& Transform, const double YawDegrees)
	{
		Transform.SetRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(YawDegrees)));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P2MotionTraceBenchmarkTest,
	"SightWeave.M2P2.Performance.MotionTrace",
	SightWeave::M2P2::MotionTests::TestFlags)

bool FSightWeaveM2P2MotionTraceBenchmarkTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P2::MotionTests;
	constexpr int32 Warmups = 4;
	constexpr int32 Repeats = 101;
	FTestWorld World(TEXT("SightWeaveM2P2MotionTrace"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (!TestNotNull(TEXT("Motion subsystem exists"), Subsystem)
		|| !TestTrue(TEXT("Ground floor registers"),
			Subsystem->RegisterFloor(Floor(Ground, true, 0.0f, 300.0f), nullptr))
		|| !TestTrue(TEXT("Upper floor registers"),
			Subsystem->RegisterFloor(Floor(Upper, false, 400.0f, 700.0f), nullptr))
		|| !TestTrue(TEXT("Static scene registers"),
			Subsystem->RegisterOccluder(StaticScene(), false, true, nullptr).IsValid()))
	{
		return false;
	}

	FSightWeaveVisionSourceDescription Body = Vision(
		ESightWeaveSourceShape::Radial,
		ESightWeaveIlluminationPolicy::BypassLegalIllumination);
	FSightWeaveVisionSourceDescription PlayerCone = Vision(
		ESightWeaveSourceShape::DirectionalCone,
		ESightWeaveIlluminationPolicy::RequiresLegalIllumination);
	FSightWeaveVisionSourceDescription Camera = Vision(
		ESightWeaveSourceShape::CameraCone,
		ESightWeaveIlluminationPolicy::RequiresLegalIllumination);
	FSightWeaveIlluminationSourceDescription Torch = Light(
		ESightWeaveSourceShape::DirectionalCone, FName(TEXT("Visible")), 1200.0f);
	FSightWeaveIlluminationSourceDescription Lantern = Light(
		ESightWeaveSourceShape::Radial, FName(TEXT("Infrared")), 650.0f);

	const double ColdStartSeconds = FPlatformTime::Seconds();
	const FSightWeaveVisionSourceHandle BodyHandle = Subsystem->RegisterVisionSource(Body, nullptr);
	const double ColdMicroseconds = (FPlatformTime::Seconds() - ColdStartSeconds) * 1000000.0;
	TestTrue(TEXT("Cold body source registers"), BodyHandle.IsValid());
	const FSightWeaveVisionSourceHandle PlayerHandle = Subsystem->RegisterVisionSource(PlayerCone, nullptr);
	const FSightWeaveVisionSourceHandle CameraHandle = Subsystem->RegisterVisionSource(Camera, nullptr);
	const FSightWeaveIlluminationSourceHandle TorchHandle = Subsystem->RegisterIlluminationSource(Torch, nullptr);
	const FSightWeaveIlluminationSourceHandle LanternHandle = Subsystem->RegisterIlluminationSource(Lantern, nullptr);
	TestTrue(TEXT("Shared-origin fixture registers"),
		PlayerHandle.IsValid() && CameraHandle.IsValid() && TorchHandle.IsValid() && LanternHandle.IsValid());
	const FSightWeaveFrameSnapshot ColdSnapshot = Subsystem->GetPublishedSnapshot();
	const FSightWeaveVisionSnapshotEntry* ColdBodyEntry = ColdSnapshot.VisionSources.FindByPredicate(
		[BodyHandle](const FSightWeaveVisionSnapshotEntry& Entry) { return Entry.Handle == BodyHandle; });
	AddInfo(FString::Printf(
		TEXT("M2P2_MOTION name=cold_source_register strategy=full_build repeats=1 total_us=%.3f snapshot_bytes=%llu candidates=%d events=%d vertices=%d"),
		ColdMicroseconds,
		SnapshotAllocatedBytes(ColdSnapshot),
		ColdBodyEntry ? ColdBodyEntry->CandidateSegmentCount : 0,
		ColdBodyEntry ? ColdBodyEntry->CandidateRayCount : 0,
		ColdBodyEntry ? ColdBodyEntry->Polygon.Vertices.Num() : 0));

	auto RunVision = [&](const TCHAR* Name, const TCHAR* Strategy, const FSightWeaveVisionSourceHandle Handle,
		FSightWeaveVisionSourceDescription& Description, auto&& Mutator)
	{
		const FMotionSamples Samples = TimeVisionMotion(
			Subsystem, Handle, Description, Warmups, Repeats, false, Forward<decltype(Mutator)>(Mutator));
		TestTrue(*FString::Printf(TEXT("%s updates succeed"), Name), Samples.bSucceeded);
		LogMotion(*this, Name, Samples, Repeats, Strategy);
		return Samples;
	};
	auto RunVisionTransform = [&](const TCHAR* Name, const TCHAR* Strategy,
		const FSightWeaveVisionSourceHandle Handle,
		FSightWeaveVisionSourceDescription& Description, auto&& Mutator)
	{
		const FMotionSamples Samples = TimeVisionMotion(
			Subsystem, Handle, Description, Warmups, Repeats, true, Forward<decltype(Mutator)>(Mutator));
		TestTrue(*FString::Printf(TEXT("%s updates succeed"), Name), Samples.bSucceeded);
		LogMotion(*this, Name, Samples, Repeats, Strategy);
		return Samples;
	};
	auto RunLight = [&](const TCHAR* Name, const TCHAR* Strategy,
		const FSightWeaveIlluminationSourceHandle Handle,
		FSightWeaveIlluminationSourceDescription& Description, auto&& Mutator)
	{
		const FMotionSamples Samples = TimeIlluminationMotion(
			Subsystem, Handle, Description, Warmups, Repeats, false, Forward<decltype(Mutator)>(Mutator));
		TestTrue(*FString::Printf(TEXT("%s updates succeed"), Name), Samples.bSucceeded);
		LogMotion(*this, Name, Samples, Repeats, Strategy);
		return Samples;
	};
	auto RunLightTransform = [&](const TCHAR* Name, const TCHAR* Strategy,
		const FSightWeaveIlluminationSourceHandle Handle,
		FSightWeaveIlluminationSourceDescription& Description, auto&& Mutator)
	{
		const FMotionSamples Samples = TimeIlluminationMotion(
			Subsystem, Handle, Description, Warmups, Repeats, true, Forward<decltype(Mutator)>(Mutator));
		TestTrue(*FString::Printf(TEXT("%s updates succeed"), Name), Samples.bSucceeded);
		LogMotion(*this, Name, Samples, Repeats, Strategy);
		return Samples;
	};

	const FSightWeaveRevision NoChangeRevision = Subsystem->GetRevision();
	const FMotionSamples NoChange = RunVisionTransform(
		TEXT("no_change"), TEXT("cache_hit_no_publish"), BodyHandle, Body,
		[](int32, FSightWeaveVisionSourceDescription&) {});
	TestEqual(TEXT("No-change trace preserves revision"), Subsystem->GetRevision(), NoChangeRevision);
	TestEqual(TEXT("No-change trace reports no revision changes"), NoChange.RevisionChanges, int64(0));

	const FMotionSamples RadialRotation = RunVisionTransform(
		TEXT("radial_rotation_small"), TEXT("rotation_only_radial"), BodyHandle, Body,
		[](const int32 Index, FSightWeaveVisionSourceDescription& Description)
		{
			SetYaw(Description.Transform, Index % 2 == 0 ? 0.5 : 0.0);
		});
	TestEqual(TEXT("Radial orientation changes remain public revisions"),
		RadialRotation.RevisionChanges, int64(Repeats));

	RunVisionTransform(TEXT("cone_rotation_small"), TEXT("rotation_only_cone"), PlayerHandle, PlayerCone,
		[](const int32 Index, FSightWeaveVisionSourceDescription& Description)
		{
			SetYaw(Description.Transform, Index % 2 == 0 ? 0.5 : 0.0);
		});
	RunVisionTransform(TEXT("cone_rotation_large"), TEXT("rotation_only_cone_full_recut"), PlayerHandle, PlayerCone,
		[](const int32 Index, FSightWeaveVisionSourceDescription& Description)
		{
			SetYaw(Description.Transform, Index % 2 == 0 ? 135.0 : -135.0);
		});
	RunVisionTransform(TEXT("cone_rotation_wrap_180_360"), TEXT("rotation_wrap"), PlayerHandle, PlayerCone,
		[](const int32 Index, FSightWeaveVisionSourceDescription& Description)
		{
			SetYaw(Description.Transform, Index % 2 == 0 ? 179.75 : -179.75);
		});
	RunVisionTransform(TEXT("camera_cone_rotation"), TEXT("rotation_only_camera"), CameraHandle, Camera,
		[](const int32 Index, FSightWeaveVisionSourceDescription& Description)
		{
			SetYaw(Description.Transform, Index % 2 == 0 ? 1.0 : 0.0);
		});
	RunLightTransform(TEXT("torch_direction"), TEXT("rotation_only_light"), TorchHandle, Torch,
		[](const int32 Index, FSightWeaveIlluminationSourceDescription& Description)
		{
			SetYaw(Description.Transform, Index % 2 == 0 ? 2.0 : 0.0);
		});
	RunLightTransform(TEXT("lantern_rotation"), TEXT("rotation_only_radial_light"), LanternHandle, Lantern,
		[](const int32 Index, FSightWeaveIlluminationSourceDescription& Description)
		{
			SetYaw(Description.Transform, Index % 2 == 0 ? 2.0 : 0.0);
		});

	auto Translation = [](const double StepX, const double StepY)
	{
		return [StepX, StepY](const int32 Index, FSightWeaveVisionSourceDescription& Description)
		{
			const double Sign = Index % 2 == 0 ? 1.0 : 0.0;
			Description.Transform.SetLocation(FVector(StepX * Sign, StepY * Sign, 100.0));
		};
	};
	RunVisionTransform(TEXT("translate_1cm"), TEXT("small_translation"), PlayerHandle, PlayerCone, Translation(1.0, 0.0));
	RunVisionTransform(TEXT("translate_5cm"), TEXT("small_translation"), PlayerHandle, PlayerCone, Translation(5.0, 0.0));
	RunVisionTransform(TEXT("translate_20cm"), TEXT("small_translation"), PlayerHandle, PlayerCone, Translation(20.0, 0.0));
	RunVisionTransform(TEXT("translate_diagonal"), TEXT("small_translation"), PlayerHandle, PlayerCone, Translation(5.0, 5.0));
	RunVisionTransform(TEXT("translate_along_wall"), TEXT("small_translation"), PlayerHandle, PlayerCone,
		[](const int32 Index, FSightWeaveVisionSourceDescription& Description)
		{
			Description.Transform.SetLocation(FVector(-50.0, Index % 2 == 0 ? 5.0 : 0.0, 100.0));
		});
	RunVisionTransform(TEXT("translate_endpoint_order_crossing"), TEXT("small_translation_order_guard"),
		PlayerHandle, PlayerCone,
		[](const int32 Index, FSightWeaveVisionSourceDescription& Description)
		{
			Description.Transform.SetLocation(FVector(Index % 2 == 0 ? 599.95 : 600.05, 0.0, 100.0));
		});
	RunVisionTransform(TEXT("translate_room_transition"), TEXT("translation_candidate_change"), PlayerHandle, PlayerCone,
		[](const int32 Index, FSightWeaveVisionSourceDescription& Description)
		{
			Description.Transform.SetLocation(FVector(Index % 2 == 0 ? -100.0 : 1500.0, 0.0, 100.0));
		});
	RunVisionTransform(TEXT("teleport"), TEXT("teleport_full_rebuild"), PlayerHandle, PlayerCone,
		[](const int32 Index, FSightWeaveVisionSourceDescription& Description)
		{
			Description.Transform.SetLocation(FVector(Index % 2 == 0 ? -6000.0 : 6000.0, 5000.0, 100.0));
		});
	RunVisionTransform(TEXT("camera_switch"), TEXT("teleport_camera"), CameraHandle, Camera,
		[](const int32 Index, FSightWeaveVisionSourceDescription& Description)
		{
			Description.Transform.SetLocation(FVector(Index % 2 == 0 ? -2500.0 : 2500.0, 0.0, 100.0));
		});
	PlayerCone.Transform.SetLocation(FVector(0.0, 0.0, 100.0));
	SetYaw(PlayerCone.Transform, 0.0);
	TestTrue(TEXT("Player source restores before profile traces"),
		Subsystem->UpdateVisionSource(PlayerHandle, PlayerCone));
	RunVision(TEXT("range_change"), TEXT("profile_full_rebuild"), PlayerHandle, PlayerCone,
		[](const int32 Index, FSightWeaveVisionSourceDescription& Description)
		{
			Description.Range = Index % 2 == 0 ? 900.0f : 1200.0f;
		});
	RunVision(TEXT("height_change"), TEXT("height_full_rebuild"), PlayerHandle, PlayerCone,
		[](const int32 Index, FSightWeaveVisionSourceDescription& Description)
		{
			Description.HeightRange.ZMax = Index % 2 == 0 ? 180.0f : 300.0f;
		});
	RunVision(TEXT("floor_change"), TEXT("floor_full_rebuild"), PlayerHandle, PlayerCone,
		[](const int32 Index, FSightWeaveVisionSourceDescription& Description)
		{
			Description.FloorId = Index % 2 == 0 ? Upper : Ground;
			Description.Transform.SetLocation(FVector(0.0, 0.0, Index % 2 == 0 ? 500.0 : 100.0));
		});
	PlayerCone.FloorId = Ground;
	PlayerCone.Transform.SetLocation(FVector(0.0, 0.0, 100.0));
	PlayerCone.Range = 1200.0f;
	PlayerCone.HeightRange.ZMax = 300.0f;
	TestTrue(TEXT("Player source restores after profile traces"),
		Subsystem->UpdateVisionSource(PlayerHandle, PlayerCone));

	TArray<FSightWeaveSegment2D> DoorSegments = {
		Segment(FVector2D(250.0, -100.0), FVector2D(250.0, 100.0)) };
	const FSightWeaveOccluderHandle Door = Subsystem->RegisterOccluder(DoorSegments, true, true, nullptr);
	FMotionSamples DoorSamples;
	for (int32 Warmup = 0; Warmup < Warmups; ++Warmup)
	{
		const double X = Warmup % 2 == 0 ? 250.0 : 850.0;
		DoorSegments[0].A.X = X;
		DoorSegments[0].B.X = X;
		Subsystem->UpdateOccluder(Door, DoorSegments, true, true);
	}
	const FSightWeavePreparedEventIndexStats DoorPreparedBefore =
		Subsystem->GetPreparedEventIndexStats();
	for (int32 Index = 0; Index < Repeats; ++Index)
	{
		const double X = Index % 2 == 0 ? 250.0 : 850.0;
		DoorSegments[0].A.X = X;
		DoorSegments[0].B.X = X;
		const FSightWeaveRevision Before = Subsystem->GetRevision();
		const double StartSeconds = FPlatformTime::Seconds();
		DoorSamples.bSucceeded &= Subsystem->UpdateOccluder(Door, DoorSegments, true, true);
		const double TotalMicroseconds = (FPlatformTime::Seconds() - StartSeconds) * 1000000.0;
		DoorSamples.TotalMicroseconds.Add(TotalMicroseconds);
		const bool bPublished = Subsystem->GetRevision() != Before;
		DoorSamples.RevisionChanges += bPublished ? 1 : 0;
		const FSightWeaveFrameSnapshot Snapshot = Subsystem->GetPublishedSnapshot();
		AccumulateSnapshot(
			DoorSamples,
			Snapshot,
			bPublished ? SnapshotSolveMicroseconds(Snapshot) : 0.0,
			TotalMicroseconds,
			bPublished);
	}
	AccumulatePreparedIndexDelta(
		DoorSamples,
		DoorPreparedBefore,
		Subsystem->GetPreparedEventIndexStats());
	TestTrue(TEXT("Dynamic door trace succeeds"), DoorSamples.bSucceeded);
	LogMotion(*this, TEXT("dynamic_door_toggle"), DoorSamples, Repeats,
		TEXT("dynamic_overlay_invalidate_snapshot_upper_bound"));

	FMotionSamples DoorAndMove;
	const FSightWeavePreparedEventIndexStats DoorAndMovePreparedBefore =
		Subsystem->GetPreparedEventIndexStats();
	for (int32 Index = 0; Index < Repeats; ++Index)
	{
		const double X = Index % 2 == 0 ? 250.0 : 850.0;
		DoorSegments[0].A.X = X;
		DoorSegments[0].B.X = X;
		PlayerCone.Transform.SetLocation(FVector(Index % 2 == 0 ? 5.0 : 0.0, 0.0, 100.0));
		const FSightWeaveRevision Before = Subsystem->GetRevision();
		const double DoorStartSeconds = FPlatformTime::Seconds();
		DoorAndMove.bSucceeded &= Subsystem->UpdateOccluder(Door, DoorSegments, true, true);
		const double DoorMicroseconds = (FPlatformTime::Seconds() - DoorStartSeconds) * 1000000.0;
		const FSightWeaveRevision BeforeVision = Subsystem->GetRevision();
		const FSightWeaveFrameSnapshot DoorSnapshot = Subsystem->GetPublishedSnapshot();
		const bool bDoorPublished = BeforeVision != Before;
		const double DoorSolveMicroseconds = bDoorPublished ? SnapshotSolveMicroseconds(DoorSnapshot) : 0.0;
		AccumulateSnapshotCounters(DoorAndMove, DoorSnapshot, bDoorPublished);

		const double VisionStartSeconds = FPlatformTime::Seconds();
		DoorAndMove.bSucceeded &= Subsystem->UpdateVisionSourceTransform(PlayerHandle, PlayerCone.Transform);
		const double VisionMicroseconds = (FPlatformTime::Seconds() - VisionStartSeconds) * 1000000.0;
		const FSightWeaveFrameSnapshot VisionSnapshot = Subsystem->GetPublishedSnapshot();
		const bool bVisionPublished = Subsystem->GetRevision() != BeforeVision;
		const double VisionSolveMicroseconds = bVisionPublished
			? SnapshotSolveMicroseconds(VisionSnapshot, PlayerHandle)
			: 0.0;
		AccumulateSnapshotCounters(DoorAndMove, VisionSnapshot, bVisionPublished, PlayerHandle);

		const double TotalMicroseconds = DoorMicroseconds + VisionMicroseconds;
		const double SolveMicroseconds = DoorSolveMicroseconds + VisionSolveMicroseconds;
		DoorAndMove.TotalMicroseconds.Add(TotalMicroseconds);
		DoorAndMove.SolveMicroseconds.Add(SolveMicroseconds);
		DoorAndMove.PublicationEstimateMicroseconds.Add(FMath::Max(0.0, TotalMicroseconds - SolveMicroseconds));
		DoorAndMove.RevisionChanges += Subsystem->GetRevision().GetValue() - Before.GetValue();
	}
	AccumulatePreparedIndexDelta(
		DoorAndMove,
		DoorAndMovePreparedBefore,
		Subsystem->GetPreparedEventIndexStats());
	TestTrue(TEXT("Door plus movement trace succeeds"), DoorAndMove.bSucceeded);
	LogMotion(*this, TEXT("dynamic_door_plus_motion"), DoorAndMove, Repeats,
		TEXT("dynamic_snapshot_upper_bound_plus_target_motion"));

	FMotionSamples SharedOrigin;
	for (int32 Warmup = 0; Warmup < Warmups; ++Warmup)
	{
		const FVector Location(Warmup % 2 == 0 ? 5.0 : 0.0, 0.0, 100.0);
		PlayerCone.Transform.SetLocation(Location);
		Body.Transform.SetLocation(Location);
		Torch.Transform.SetLocation(Location);
		Lantern.Transform.SetLocation(Location);
		Subsystem->UpdateVisionSourceTransform(PlayerHandle, PlayerCone.Transform);
		Subsystem->UpdateVisionSourceTransform(BodyHandle, Body.Transform);
		Subsystem->UpdateIlluminationSourceTransform(TorchHandle, Torch.Transform);
		Subsystem->UpdateIlluminationSourceTransform(LanternHandle, Lantern.Transform);
	}
	const FSightWeavePreparedEventIndexStats SharedOriginPreparedBefore =
		Subsystem->GetPreparedEventIndexStats();
	for (int32 Index = 0; Index < Repeats; ++Index)
	{
		const FVector Location(Index % 2 == 0 ? 5.0 : 0.0, 0.0, 100.0);
		PlayerCone.Transform.SetLocation(Location);
		Body.Transform.SetLocation(Location);
		Torch.Transform.SetLocation(Location);
		Lantern.Transform.SetLocation(Location);
		const FSightWeaveRevision Before = Subsystem->GetRevision();
		double TotalMicroseconds = 0.0;
		double SolveMicroseconds = 0.0;

		FSightWeaveRevision BeforeUpdate = Subsystem->GetRevision();
		double StartSeconds = FPlatformTime::Seconds();
		SharedOrigin.bSucceeded &= Subsystem->UpdateVisionSourceTransform(PlayerHandle, PlayerCone.Transform);
		TotalMicroseconds += (FPlatformTime::Seconds() - StartSeconds) * 1000000.0;
		FSightWeaveFrameSnapshot Snapshot = Subsystem->GetPublishedSnapshot();
		bool bPublished = Subsystem->GetRevision() != BeforeUpdate;
		SolveMicroseconds += bPublished ? SnapshotSolveMicroseconds(Snapshot, PlayerHandle) : 0.0;
		AccumulateSnapshotCounters(SharedOrigin, Snapshot, bPublished, PlayerHandle);

		BeforeUpdate = Subsystem->GetRevision();
		StartSeconds = FPlatformTime::Seconds();
		SharedOrigin.bSucceeded &= Subsystem->UpdateVisionSourceTransform(BodyHandle, Body.Transform);
		TotalMicroseconds += (FPlatformTime::Seconds() - StartSeconds) * 1000000.0;
		Snapshot = Subsystem->GetPublishedSnapshot();
		bPublished = Subsystem->GetRevision() != BeforeUpdate;
		SolveMicroseconds += bPublished ? SnapshotSolveMicroseconds(Snapshot, BodyHandle) : 0.0;
		AccumulateSnapshotCounters(SharedOrigin, Snapshot, bPublished, BodyHandle);

		BeforeUpdate = Subsystem->GetRevision();
		StartSeconds = FPlatformTime::Seconds();
		SharedOrigin.bSucceeded &= Subsystem->UpdateIlluminationSourceTransform(TorchHandle, Torch.Transform);
		TotalMicroseconds += (FPlatformTime::Seconds() - StartSeconds) * 1000000.0;
		Snapshot = Subsystem->GetPublishedSnapshot();
		bPublished = Subsystem->GetRevision() != BeforeUpdate;
		SolveMicroseconds += bPublished ? SnapshotSolveMicroseconds(Snapshot, TorchHandle) : 0.0;
		AccumulateSnapshotCounters(
			SharedOrigin,
			Snapshot,
			bPublished,
			FSightWeaveVisionSourceHandle(),
			TorchHandle);

		BeforeUpdate = Subsystem->GetRevision();
		StartSeconds = FPlatformTime::Seconds();
		SharedOrigin.bSucceeded &= Subsystem->UpdateIlluminationSourceTransform(LanternHandle, Lantern.Transform);
		TotalMicroseconds += (FPlatformTime::Seconds() - StartSeconds) * 1000000.0;
		Snapshot = Subsystem->GetPublishedSnapshot();
		bPublished = Subsystem->GetRevision() != BeforeUpdate;
		SolveMicroseconds += bPublished ? SnapshotSolveMicroseconds(Snapshot, LanternHandle) : 0.0;
		AccumulateSnapshotCounters(
			SharedOrigin,
			Snapshot,
			bPublished,
			FSightWeaveVisionSourceHandle(),
			LanternHandle);

		SharedOrigin.TotalMicroseconds.Add(TotalMicroseconds);
		SharedOrigin.SolveMicroseconds.Add(SolveMicroseconds);
		SharedOrigin.PublicationEstimateMicroseconds.Add(FMath::Max(0.0, TotalMicroseconds - SolveMicroseconds));
		SharedOrigin.RevisionChanges += Subsystem->GetRevision().GetValue() - Before.GetValue();
	}
	AccumulatePreparedIndexDelta(
		SharedOrigin,
		SharedOriginPreparedBefore,
		Subsystem->GetPreparedEventIndexStats());
	TestTrue(TEXT("Shared-origin trace succeeds"), SharedOrigin.bSucceeded);
	LogMotion(*this, TEXT("shared_origin_four_sources"), SharedOrigin, Repeats, TEXT("shared_origin_prepared_event_index"));

	FSightWeaveVisionSourceDescription DifferentHeight = Vision(
		ESightWeaveSourceShape::Radial,
		ESightWeaveIlluminationPolicy::BypassLegalIllumination,
		Local,
		Ground,
		150.0f,
		250.0f);
	FSightWeaveVisionSourceDescription DifferentOwner = Vision(
		ESightWeaveSourceShape::Radial,
		ESightWeaveIlluminationPolicy::BypassLegalIllumination,
		Remote);
	const FSightWeaveVisionSourceHandle HeightHandle = Subsystem->RegisterVisionSource(DifferentHeight, nullptr);
	const FSightWeaveVisionSourceHandle OwnerHandle = Subsystem->RegisterVisionSource(DifferentOwner, nullptr);
	const FSightWeaveRevision IsolationBefore = Subsystem->GetRevision();
	Body.Transform.SetLocation(FVector(12.0, 0.0, 100.0));
	TestTrue(TEXT("Primary shared source updates"),
		Subsystem->UpdateVisionSourceTransform(BodyHandle, Body.Transform));
	const FSightWeaveFrameSnapshot IsolationSnapshot = Subsystem->GetPublishedSnapshot();
	const FSightWeaveVisionSnapshotEntry* HeightEntry = IsolationSnapshot.VisionSources.FindByPredicate(
		[HeightHandle](const FSightWeaveVisionSnapshotEntry& Entry) { return Entry.Handle == HeightHandle; });
	const FSightWeaveVisionSnapshotEntry* OwnerEntry = IsolationSnapshot.VisionSources.FindByPredicate(
		[OwnerHandle](const FSightWeaveVisionSnapshotEntry& Entry) { return Entry.Handle == OwnerHandle; });
	TestTrue(TEXT("Different-height source remains independently published"),
		HeightEntry && HeightEntry->Description.HeightRange.ZMin == 150.0f);
	TestTrue(TEXT("Different-owner source remains independently published"),
		OwnerEntry && OwnerEntry->Description.KnowledgeOwnerId == Remote);
	TestTrue(TEXT("Primary update advances revision"), IsolationBefore < Subsystem->GetRevision());

	FMotionSamples Reregister;
	const FSightWeavePreparedEventIndexStats ReregisterPreparedBefore =
		Subsystem->GetPreparedEventIndexStats();
	FSightWeaveVisionSourceHandle ReregisterHandle = CameraHandle;
	for (int32 Index = 0; Index < 11; ++Index)
	{
		const FSightWeaveRevision Before = Subsystem->GetRevision();
		const double StartSeconds = FPlatformTime::Seconds();
		Reregister.bSucceeded &= Subsystem->UnregisterVisionSource(ReregisterHandle);
		ReregisterHandle = Subsystem->RegisterVisionSource(Camera, nullptr);
		Reregister.bSucceeded &= ReregisterHandle.IsValid();
		const double TotalMicroseconds = (FPlatformTime::Seconds() - StartSeconds) * 1000000.0;
		Reregister.TotalMicroseconds.Add(TotalMicroseconds);
		Reregister.RevisionChanges += Subsystem->GetRevision().GetValue() - Before.GetValue();
		const FSightWeaveFrameSnapshot Snapshot = Subsystem->GetPublishedSnapshot();
		AccumulateSnapshot(
			Reregister,
			Snapshot,
			SnapshotSolveMicroseconds(Snapshot, ReregisterHandle),
			TotalMicroseconds,
			true,
			ReregisterHandle);
	}
	AccumulatePreparedIndexDelta(
		Reregister,
		ReregisterPreparedBefore,
		Subsystem->GetPreparedEventIndexStats());
	TestTrue(TEXT("Source re-registration trace succeeds"), Reregister.bSucceeded);
	LogMotion(*this, TEXT("source_reregister"), Reregister, 11, TEXT("lifecycle_full_rebuild"));

	return true;
}

#endif
