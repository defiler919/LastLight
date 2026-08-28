#if WITH_DEV_AUTOMATION_TESTS

#include "SightWeaveM2P3Timing.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "SightWeaveWorldSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace SightWeave::M2P3::SoakTests
{
	using namespace SightWeave::M2P3::Timing;

	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr int32 MinimumFrameCount = 36000;
	constexpr int32 WarmupFrameCount = 2400;
	constexpr double SimulatedFramesPerSecond = 60.0;
	constexpr double SlowFrameThresholdMicroseconds = 1000.0;
	constexpr int32 MaximumSustainedSlowFrames = 60;
	const FSightWeaveFloorId Ground(FName(TEXT("Ground")));
	const FSightWeaveKnowledgeOwnerId Local(FName(TEXT("Local")));

	enum class EClassification : uint8
	{
		WithinBudget,
		PluginCpuOverrun,
		SchedulerPreemption,
		CoreMigrationFrequency,
		MeasurementInstrumentationAnomaly,
		Unknown
	};

	const TCHAR* ClassificationName(const EClassification Classification)
	{
		switch (Classification)
		{
		case EClassification::WithinBudget: return TEXT("Within budget");
		case EClassification::PluginCpuOverrun: return TEXT("Plugin CPU overrun");
		case EClassification::SchedulerPreemption: return TEXT("Scheduler/preemption");
		case EClassification::CoreMigrationFrequency: return TEXT("Core migration/frequency");
		case EClassification::MeasurementInstrumentationAnomaly:
			return TEXT("Measurement/instrumentation anomaly");
		case EClassification::Unknown: return TEXT("Unknown");
		default: return TEXT("Unknown");
		}
	}

	struct FDistribution
	{
		double Median = 0.0;
		double P95 = 0.0;
		double P99 = 0.0;
		double P999 = 0.0;
		double Maximum = 0.0;
	};

	FDistribution Summarize(TArray<double> Values)
	{
		FDistribution Result;
		if (Values.IsEmpty())
		{
			return Result;
		}
		Values.Sort();
		auto Percentile = [&Values](const double Fraction)
		{
			return Values[FMath::Clamp(
				FMath::CeilToInt(Fraction * Values.Num()) - 1,
				0,
				Values.Num() - 1)];
		};
		Result.Median = Percentile(0.50);
		Result.P95 = Percentile(0.95);
		Result.P99 = Percentile(0.99);
		Result.P999 = Percentile(0.999);
		Result.Maximum = Values.Last();
		return Result;
	}

	class FTestWorld
	{
	public:
		FTestWorld()
		{
			const FName WorldName = MakeUniqueObjectName(
				GetTransientPackage(),
				UWorld::StaticClass(),
				FName(TEXT("SightWeaveM2P3Soak")));
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

		UWorld* GetWorld() const { return World; }
		USightWeaveWorldSubsystem* GetSubsystem() const
		{
			return World ? World->GetSubsystem<USightWeaveWorldSubsystem>() : nullptr;
		}

	private:
		UWorld* World = nullptr;
	};

	FSightWeaveFloorDefinition MakeFloor()
	{
		FSightWeaveFloorDefinition Floor;
		Floor.FloorId = Ground;
		Floor.BoundsMin = FVector2D(-5000.0, -5000.0);
		Floor.BoundsMax = FVector2D(5000.0, 5000.0);
		Floor.HeightRange.ZMin = 0.0f;
		Floor.HeightRange.ZMax = 300.0f;
		return Floor;
	}

	TArray<FSightWeaveSegment2D> MakeRoomSegments()
	{
		TArray<FSightWeaveSegment2D> Segments;
		Segments.Reserve(100);
		for (int32 RoomY = -2; RoomY <= 2; ++RoomY)
		{
			for (int32 RoomX = -2; RoomX <= 2; ++RoomX)
			{
				const FVector2D Center(RoomX * 650.0, RoomY * 650.0);
				const double Half = 240.0;
				const FVector2D Corners[] =
				{
					Center + FVector2D(-Half, -Half),
					Center + FVector2D(Half, -Half),
					Center + FVector2D(Half, Half),
					Center + FVector2D(-Half, Half)
				};
				for (int32 Edge = 0; Edge < 4; ++Edge)
				{
					FSightWeaveSegment2D& Segment = Segments.AddDefaulted_GetRef();
					Segment.A = Corners[Edge];
					Segment.B = Corners[(Edge + 1) % 4];
					Segment.FloorId = Ground;
					Segment.HeightRange.ZMin = 0.0f;
					Segment.HeightRange.ZMax = 300.0f;
				}
			}
		}
		return Segments;
	}

	FSightWeaveVisionSourceDescription MakeVision(
		const FVector Location,
		const ESightWeaveSourceShape Shape,
		const float Range,
		const ESightWeaveIlluminationPolicy IlluminationPolicy)
	{
		FSightWeaveVisionSourceDescription Vision;
		Vision.KnowledgeOwnerId = Local;
		Vision.FloorId = Ground;
		Vision.Transform.SetLocation(Location);
		Vision.Shape = Shape;
		Vision.Range = Range;
		Vision.HalfAngleDegrees = 50.0f;
		Vision.NearAwarenessRadius = Shape == ESightWeaveSourceShape::CameraCone ? 90.0f : 0.0f;
		Vision.HeightRange.ZMin = 0.0f;
		Vision.HeightRange.ZMax = 300.0f;
		Vision.IlluminationPolicy = IlluminationPolicy;
		if (IlluminationPolicy == ESightWeaveIlluminationPolicy::RequiresLegalIllumination)
		{
			Vision.Compatibility.AcceptedCapabilities =
				{ FName(TEXT("Visible")), FName(TEXT("Infrared")) };
		}
		return Vision;
	}

	FSightWeaveIlluminationSourceDescription MakeLight(
		const FVector Location,
		const float Range,
		const FName Capability)
	{
		FSightWeaveIlluminationSourceDescription Light;
		Light.KnowledgeOwnerId = Local;
		Light.FloorId = Ground;
		Light.Transform.SetLocation(Location);
		Light.Range = Range;
		Light.HeightRange.ZMin = 0.0f;
		Light.HeightRange.ZMax = 300.0f;
		Light.EmittedCapabilities = { Capability };
		return Light;
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

	FTimingSample MeasureControl(const bool bCompute, const FFixedWorkControls& Controls)
	{
		FDualClockTimer Timer(false);
		Timer.Start();
		const uint64 Result = bCompute ? Controls.RunCompute() : Controls.RunMemory();
		const FTimingSample Sample = Timer.Stop();
		ConsumeControlResult(Result);
		return Sample;
	}

	struct FFrameRow
	{
		int32 Frame = 0;
		FTimingSample Total;
		FTimingSample ComputeControl;
		FTimingSample MemoryControl;
		FTimingSample CameraUpdate;
		FTimingSample DoorUpdate;
		FTimingSample Batch;
		int32 bDoorChanged = 0;
		int32 bRemoteChanged = 0;
		int32 bTorchMoved = 0;
		int64 PreparedHitDelta = 0;
		int64 PreparedMissDelta = 0;
		int64 PreparedRebuildDelta = 0;
		int64 PreparedEvictionDelta = 0;
		int64 RevisionBefore = 0;
		int64 RevisionAfter = 0;
		uint64 CapacityGrowthBytes = 0;
		EClassification Classification = EClassification::WithinBudget;
	};

	bool GetCaptureParameters(FString& OutDirectory, FString& OutMode, int32& OutFrameCount)
	{
		if (!FParse::Param(FCommandLine::Get(), TEXT("SightWeaveM2P3SoakCapture"))
			|| !FParse::Value(FCommandLine::Get(), TEXT("SightWeaveM2P3SoakOutput="), OutDirectory)
			|| !FParse::Value(FCommandLine::Get(), TEXT("SightWeaveM2P3SoakMode="), OutMode)
			|| !FParse::Value(FCommandLine::Get(), TEXT("SightWeaveM2P3SoakFrames="), OutFrameCount))
		{
			return false;
		}
		OutDirectory = FPaths::ConvertRelativePathToFull(OutDirectory);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P3FrameSoakTest,
	"SightWeave.M2P3.Soak.FrameLevel",
	SightWeave::M2P3::SoakTests::TestFlags)

bool FSightWeaveM2P3FrameSoakTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P3::SoakTests;
	FString OutputDirectory;
	FString Mode;
	int32 FrameCount = 0;
	if (!GetCaptureParameters(OutputDirectory, Mode, FrameCount))
	{
		AddInfo(TEXT("Frame soak requires explicit capture/output/mode/frame arguments."));
		return true;
	}
	if (!TestTrue(TEXT("Soak is at least ten simulated minutes at 60 Hz"), FrameCount >= MinimumFrameCount))
	{
		return false;
	}

	FTestWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();
	USightWeaveWorldSubsystem* Subsystem = TestWorld.GetSubsystem();
	if (!TestNotNull(TEXT("Soak world exists"), World)
		|| !TestNotNull(TEXT("Soak subsystem exists"), Subsystem)
		|| !TestTrue(TEXT("Soak floor registers"), Subsystem->RegisterFloor(MakeFloor(), nullptr)))
	{
		return false;
	}

	TArray<FSightWeaveSegment2D> RoomSegments = MakeRoomSegments();
	if (!TestTrue(
		TEXT("Multiple static rooms register"),
		Subsystem->RegisterOccluder(RoomSegments, false, true, nullptr).IsValid()))
	{
		return false;
	}
	FSightWeaveSegment2D Door;
	Door.A = FVector2D(250.0, -110.0);
	Door.B = FVector2D(250.0, 110.0);
	Door.FloorId = Ground;
	Door.HeightRange.ZMin = 0.0f;
	Door.HeightRange.ZMax = 300.0f;
	TArray<FSightWeaveSegment2D> DoorSegments;
	DoorSegments.Add(Door);
	const FSightWeaveOccluderHandle DoorHandle =
		Subsystem->RegisterOccluder(DoorSegments, true, true, nullptr);

	FSightWeaveVisionSourceDescription Camera = MakeVision(
		FVector(0.0, 0.0, 100.0),
		ESightWeaveSourceShape::CameraCone,
		1500.0f,
		ESightWeaveIlluminationPolicy::RequiresLegalIllumination);
	FSightWeaveVisionSourceDescription CloseRadial = MakeVision(
		FVector(0.0, 0.0, 100.0),
		ESightWeaveSourceShape::Radial,
		300.0f,
		ESightWeaveIlluminationPolicy::BypassLegalIllumination);
	FSightWeaveVisionSourceDescription RemoteCamera = MakeVision(
		FVector(1300.0, 0.0, 100.0),
		ESightWeaveSourceShape::CameraCone,
		1200.0f,
		ESightWeaveIlluminationPolicy::BypassLegalIllumination);
	FSightWeaveVisionSourceDescription GuardCamera = MakeVision(
		FVector(-650.0, 0.0, 100.0),
		ESightWeaveSourceShape::CameraCone,
		1100.0f,
		ESightWeaveIlluminationPolicy::RequiresLegalIllumination);
	const FSightWeaveVisionSourceHandle CameraHandle = Subsystem->RegisterVisionSource(Camera, nullptr);
	const FSightWeaveVisionSourceHandle CloseRadialHandle =
		Subsystem->RegisterVisionSource(CloseRadial, nullptr);
	const FSightWeaveVisionSourceHandle RemoteCameraHandle =
		Subsystem->RegisterVisionSource(RemoteCamera, nullptr);
	const FSightWeaveVisionSourceHandle GuardCameraHandle =
		Subsystem->RegisterVisionSource(GuardCamera, nullptr);

	FSightWeaveIlluminationSourceDescription Torch =
		MakeLight(FVector(0.0, 0.0, 100.0), 850.0f, FName(TEXT("Visible")));
	FSightWeaveIlluminationSourceDescription Lantern =
		MakeLight(FVector(-650.0, 0.0, 100.0), 1000.0f, FName(TEXT("Infrared")));
	const FSightWeaveIlluminationSourceHandle TorchHandle =
		Subsystem->RegisterIlluminationSource(Torch, nullptr);
	const FSightWeaveIlluminationSourceHandle LanternHandle =
		Subsystem->RegisterIlluminationSource(Lantern, nullptr);

	if (!TestTrue(TEXT("Door handle is valid"), DoorHandle.IsValid())
		|| !TestTrue(TEXT("Camera handle is valid"), CameraHandle.IsValid())
		|| !TestTrue(TEXT("Close radial bypass handle is valid"), CloseRadialHandle.IsValid())
		|| !TestTrue(TEXT("Remote camera handle is valid"), RemoteCameraHandle.IsValid())
		|| !TestTrue(TEXT("Guard camera handle is valid"), GuardCameraHandle.IsValid())
		|| !TestTrue(TEXT("Torch handle is valid"), TorchHandle.IsValid())
		|| !TestTrue(TEXT("Lantern handle is valid"), LanternHandle.IsValid()))
	{
		return false;
	}

	TArray<FSightWeaveQueryRequest> Requests;
	Requests.Reserve(512);
	for (int32 RequestIndex = 0; RequestIndex < 512; ++RequestIndex)
	{
		const double Angle = 2.0 * PI * RequestIndex / 512.0;
		const double Radius = 180.0 + 1050.0 * static_cast<double>(RequestIndex % 17) / 16.0;
		FSightWeaveQueryRequest& Request = Requests.AddDefaulted_GetRef();
		Request.KnowledgeOwnerId = Local;
		Request.FloorId = Ground;
		Request.SampleSet.Samples.Add(FVector(
			FMath::Cos(Angle) * Radius,
			FMath::Sin(Angle) * Radius,
			100.0));
	}
	TArray<FSightWeaveVisibilityQueryResult> Results;

	FFixedWorkControls Controls;
	uint64 CorrectnessFailures = 0;
	auto ExecuteFrame = [&](const int32 AbsoluteFrame, FFrameRow* Row)
	{
		const FSightWeavePreparedEventIndexStats PreparedBefore = Subsystem->GetPreparedEventIndexStats();
		const int64 RevisionBefore = Subsystem->GetRevision().GetValue();
		FDualClockTimer TotalTimer;
		TotalTimer.Start();

		const double Phase = AbsoluteFrame * 0.0075;
		Camera.Transform.SetLocation(FVector(
			FMath::Sin(Phase) * 70.0,
			FMath::Cos(Phase * 0.7) * 55.0,
			100.0));
		Camera.Transform.SetRotation(FQuat(FVector::UpVector, Phase));
		FDualClockTimer CameraTimer(false);
		CameraTimer.Start();
		const bool bCameraUpdated = Subsystem->UpdateVisionSourceTransform(CameraHandle, Camera.Transform);
		const FTimingSample CameraSample = CameraTimer.Stop();

		bool bTorchMoved = false;
		if (AbsoluteFrame % 4 == 0)
		{
			Torch.Transform.SetLocation(Camera.Transform.GetLocation() + FVector(20.0, -15.0, 0.0));
			bTorchMoved = Subsystem->UpdateIlluminationSourceTransform(TorchHandle, Torch.Transform);
		}

		bool bDoorChanged = false;
		FTimingSample DoorSample;
		if (AbsoluteFrame % 30 == 0)
		{
			const double DoorX = (AbsoluteFrame / 30) % 2 == 0 ? 850.0 : 250.0;
			DoorSegments[0].A.X = DoorX;
			DoorSegments[0].B.X = DoorX;
			FDualClockTimer DoorTimer(false);
			DoorTimer.Start();
			bDoorChanged = Subsystem->UpdateOccluder(DoorHandle, DoorSegments, true, true);
			DoorSample = DoorTimer.Stop();
		}

		bool bRemoteChanged = false;
		if (AbsoluteFrame % 300 == 0)
		{
			RemoteCamera.bActive = (AbsoluteFrame / 300) % 2 == 0;
			bRemoteChanged = Subsystem->UpdateVisionSource(RemoteCameraHandle, RemoteCamera);
		}

		FDualClockTimer BatchTimer(false);
		BatchTimer.Start();
		Subsystem->QueryBatch(Requests, Results);
		const FTimingSample BatchSample = BatchTimer.Stop();
		World->Tick(LEVELTICK_All, 1.0f / static_cast<float>(SimulatedFramesPerSecond));
		const FTimingSample TotalSample = TotalTimer.Stop();

		const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> Snapshot =
			Subsystem->AcquirePublishedSnapshotForTesting();
		const int64 RevisionAfter = Subsystem->GetRevision().GetValue();
		const bool bSynchronous = Snapshot.IsValid()
			&& Snapshot->Revision.GetValue() == RevisionAfter;
		const FSightWeaveBatchQueryDiagnostics& BatchDiagnostics =
			Subsystem->GetLastBatchQueryDiagnostics();
		if (!bCameraUpdated
			|| Results.Num() != Requests.Num()
			|| !BatchDiagnostics.bFastPath
			|| !bSynchronous)
		{
			++CorrectnessFailures;
		}

		if (Row)
		{
			const FSightWeavePreparedEventIndexStats PreparedAfter = Subsystem->GetPreparedEventIndexStats();
			Row->Frame = AbsoluteFrame;
			Row->Total = TotalSample;
			Row->CameraUpdate = CameraSample;
			Row->DoorUpdate = DoorSample;
			Row->Batch = BatchSample;
			Row->bDoorChanged = bDoorChanged;
			Row->bRemoteChanged = bRemoteChanged;
			Row->bTorchMoved = bTorchMoved;
			Row->PreparedHitDelta = PreparedAfter.HitCount - PreparedBefore.HitCount;
			Row->PreparedMissDelta = PreparedAfter.MissCount - PreparedBefore.MissCount;
			Row->PreparedRebuildDelta = PreparedAfter.FullRebuildCount - PreparedBefore.FullRebuildCount;
			Row->PreparedEvictionDelta = PreparedAfter.EvictionCount - PreparedBefore.EvictionCount;
			Row->RevisionBefore = RevisionBefore;
			Row->RevisionAfter = RevisionAfter;
		}
	};

	for (int32 Warmup = 0; Warmup < WarmupFrameCount; ++Warmup)
	{
		ExecuteFrame(Warmup, nullptr);
	}
	const uint64 WarmedCapacityBytes =
		Results.GetAllocatedSize() + QueryResultAllocatedBytes(Results);
	const FSightWeavePreparedEventIndexStats SoakPreparedBefore = Subsystem->GetPreparedEventIndexStats();
	const int64 InitialRevision = Subsystem->GetRevision().GetValue();

	TArray<FFrameRow> Rows;
	Rows.SetNum(FrameCount);
	const double SoakStartSeconds = FPlatformTime::Seconds();
	for (int32 FrameIndex = 0; FrameIndex < FrameCount; ++FrameIndex)
	{
		FFrameRow& Row = Rows[FrameIndex];
		Row.ComputeControl = MeasureControl(true, Controls);
		ExecuteFrame(FrameIndex + WarmupFrameCount, &Row);
		Row.MemoryControl = MeasureControl(false, Controls);
		const uint64 CurrentCapacityBytes =
			Results.GetAllocatedSize() + QueryResultAllocatedBytes(Results);
		Row.CapacityGrowthBytes = CurrentCapacityBytes > WarmedCapacityBytes
			? CurrentCapacityBytes - WarmedCapacityBytes
			: 0;
	}
	const double ActualElapsedSeconds = FPlatformTime::Seconds() - SoakStartSeconds;
	const FSightWeavePreparedEventIndexStats SoakPreparedAfter = Subsystem->GetPreparedEventIndexStats();

	TArray<double> WallValues;
	TArray<double> CycleValues;
	TArray<double> ComputeCycleValues;
	TArray<double> MemoryCycleValues;
	WallValues.Reserve(FrameCount);
	CycleValues.Reserve(FrameCount);
	ComputeCycleValues.Reserve(FrameCount);
	MemoryCycleValues.Reserve(FrameCount);
	for (const FFrameRow& Row : Rows)
	{
		WallValues.Add(Row.Total.WallMicroseconds);
		CycleValues.Add(static_cast<double>(Row.Total.ThreadCycles));
		ComputeCycleValues.Add(static_cast<double>(Row.ComputeControl.ThreadCycles));
		MemoryCycleValues.Add(static_cast<double>(Row.MemoryControl.ThreadCycles));
	}
	const FDistribution Wall = Summarize(MoveTemp(WallValues));
	const FDistribution Cycles = Summarize(MoveTemp(CycleValues));
	const double MedianComputeCycles = Summarize(MoveTemp(ComputeCycleValues)).Median;
	const double MedianMemoryCycles = Summarize(MoveTemp(MemoryCycleValues)).Median;
	const double MedianWallPerCycle = Wall.Median / FMath::Max(1.0, Cycles.Median);

	int32 ClassificationCounts[6] = {};
	int32 CurrentAboveP99 = 0;
	int32 MaximumConsecutiveAboveP99 = 0;
	int32 CurrentSlow = 0;
	int32 MaximumConsecutiveSlow = 0;
	uint64 MaximumCapacityGrowthBytes = 0;
	for (FFrameRow& Row : Rows)
	{
		MaximumCapacityGrowthBytes = FMath::Max(MaximumCapacityGrowthBytes, Row.CapacityGrowthBytes);
		CurrentAboveP99 = Row.Total.WallMicroseconds > Wall.P99 ? CurrentAboveP99 + 1 : 0;
		MaximumConsecutiveAboveP99 = FMath::Max(MaximumConsecutiveAboveP99, CurrentAboveP99);
		CurrentSlow = Row.Total.WallMicroseconds > SlowFrameThresholdMicroseconds ? CurrentSlow + 1 : 0;
		MaximumConsecutiveSlow = FMath::Max(MaximumConsecutiveSlow, CurrentSlow);
		if (Row.Total.WallMicroseconds <= SlowFrameThresholdMicroseconds)
		{
			++ClassificationCounts[static_cast<int32>(Row.Classification)];
			continue;
		}
		const double TargetWallPerCycle = Row.Total.WallMicroseconds
			/ FMath::Max(1.0, static_cast<double>(Row.Total.ThreadCycles));
		const bool bControlsCycleStable =
			static_cast<double>(Row.ComputeControl.ThreadCycles) <= MedianComputeCycles * 1.25
			&& static_cast<double>(Row.MemoryControl.ThreadCycles) <= MedianMemoryCycles * 1.25;
		if (Row.Total.bMeasurementAnomaly || !Row.Total.bThreadCycleTimeValid)
		{
			Row.Classification = EClassification::MeasurementInstrumentationAnomaly;
		}
		else if ((Row.Total.bThreadMigrated || Row.Total.bFrequencyChanged)
			&& TargetWallPerCycle > MedianWallPerCycle * 1.20)
		{
			Row.Classification = EClassification::CoreMigrationFrequency;
		}
		else if (static_cast<double>(Row.Total.ThreadCycles) > Cycles.Median * 2.0
			&& TargetWallPerCycle <= MedianWallPerCycle * 1.25
			&& bControlsCycleStable)
		{
			Row.Classification = EClassification::PluginCpuOverrun;
		}
		else if (TargetWallPerCycle > MedianWallPerCycle * 1.50
			&& (static_cast<double>(Row.Total.ThreadCycles) <= Cycles.Median * 1.50
				|| Row.Total.ProcessPageFaultDelta > 0))
		{
			Row.Classification = EClassification::SchedulerPreemption;
		}
		else
		{
			Row.Classification = EClassification::Unknown;
		}
		++ClassificationCounts[static_cast<int32>(Row.Classification)];
	}

	IFileManager::Get().MakeDirectory(*OutputDirectory, true);
	FString Csv(TEXT("frame,wall_us,thread_cpu_us,thread_cycles,start_processor,end_processor,migrated,start_mhz,end_mhz,frequency_changed,page_faults,compute_wall_us,compute_cycles,memory_wall_us,memory_cycles,camera_wall_us,camera_cycles,door_changed,door_wall_us,door_cycles,remote_changed,torch_moved,batch_wall_us,batch_cycles,prepared_hits,prepared_misses,prepared_rebuilds,prepared_evictions,revision_before,revision_after,capacity_growth_bytes,classification\n"));
	Csv.Reserve(FrameCount * 400);
	for (const FFrameRow& Row : Rows)
	{
		Csv += FString::Printf(
			TEXT("%d,%.3f,%.3f,%llu,%d,%d,%d,%u,%u,%d,%u,%.3f,%llu,%.3f,%llu,%.3f,%llu,%d,%.3f,%llu,%d,%d,%.3f,%llu,%lld,%lld,%lld,%lld,%lld,%lld,%llu,%s\n"),
			Row.Frame,
			Row.Total.WallMicroseconds,
			Row.Total.ThreadCpuMicroseconds,
			Row.Total.ThreadCycles,
			Row.Total.StartProcessorIndex,
			Row.Total.EndProcessorIndex,
			Row.Total.bThreadMigrated,
			Row.Total.StartCurrentMhz,
			Row.Total.EndCurrentMhz,
			Row.Total.bFrequencyChanged,
			Row.Total.ProcessPageFaultDelta,
			Row.ComputeControl.WallMicroseconds,
			Row.ComputeControl.ThreadCycles,
			Row.MemoryControl.WallMicroseconds,
			Row.MemoryControl.ThreadCycles,
			Row.CameraUpdate.WallMicroseconds,
			Row.CameraUpdate.ThreadCycles,
			Row.bDoorChanged,
			Row.DoorUpdate.WallMicroseconds,
			Row.DoorUpdate.ThreadCycles,
			Row.bRemoteChanged,
			Row.bTorchMoved,
			Row.Batch.WallMicroseconds,
			Row.Batch.ThreadCycles,
			Row.PreparedHitDelta,
			Row.PreparedMissDelta,
			Row.PreparedRebuildDelta,
			Row.PreparedEvictionDelta,
			Row.RevisionBefore,
			Row.RevisionAfter,
			Row.CapacityGrowthBytes,
			ClassificationName(Row.Classification));
	}
	const FString CsvPath = FPaths::Combine(OutputDirectory, TEXT("frames.csv"));
	const FString SummaryPath = FPaths::Combine(OutputDirectory, TEXT("summary.json"));
	const FString Summary = FString::Printf(
		TEXT("{\n  \"schema\": 1,\n  \"mode\": \"%s\",\n  \"warmup_frames\": %d,\n  \"frames\": %d,\n  \"simulated_fps\": %.1f,\n  \"simulated_seconds\": %.3f,\n  \"actual_elapsed_seconds\": %.3f,\n  \"priority_or_affinity_modified\": false,\n  \"wall_us\": {\"p50\": %.3f, \"p95\": %.3f, \"p99\": %.3f, \"p999\": %.3f, \"max\": %.3f},\n  \"thread_cycles\": {\"p50\": %.0f, \"p95\": %.0f, \"p99\": %.0f, \"p999\": %.0f, \"max\": %.0f},\n  \"slow_threshold_us\": %.1f,\n  \"max_consecutive_above_p99\": %d,\n  \"max_consecutive_above_slow_threshold\": %d,\n  \"classifications\": {\"within\": %d, \"plugin_cpu\": %d, \"scheduler\": %d, \"migration_frequency\": %d, \"anomaly\": %d, \"unknown\": %d},\n  \"capacity_growth_bytes_max\": %llu,\n  \"prepared\": {\"hits\": %lld, \"misses\": %lld, \"rebuilds\": %lld, \"evictions\": %lld},\n  \"initial_revision\": %lld,\n  \"final_revision\": %lld,\n  \"correctness_failures\": %llu\n}\n"),
		*Mode,
		WarmupFrameCount,
		FrameCount,
		SimulatedFramesPerSecond,
		FrameCount / SimulatedFramesPerSecond,
		ActualElapsedSeconds,
		Wall.Median,
		Wall.P95,
		Wall.P99,
		Wall.P999,
		Wall.Maximum,
		Cycles.Median,
		Cycles.P95,
		Cycles.P99,
		Cycles.P999,
		Cycles.Maximum,
		SlowFrameThresholdMicroseconds,
		MaximumConsecutiveAboveP99,
		MaximumConsecutiveSlow,
		ClassificationCounts[0],
		ClassificationCounts[1],
		ClassificationCounts[2],
		ClassificationCounts[3],
		ClassificationCounts[4],
		ClassificationCounts[5],
		MaximumCapacityGrowthBytes,
		SoakPreparedAfter.HitCount - SoakPreparedBefore.HitCount,
		SoakPreparedAfter.MissCount - SoakPreparedBefore.MissCount,
		SoakPreparedAfter.FullRebuildCount - SoakPreparedBefore.FullRebuildCount,
		SoakPreparedAfter.EvictionCount - SoakPreparedBefore.EvictionCount,
		InitialRevision,
		Subsystem->GetRevision().GetValue(),
		CorrectnessFailures);
	const bool bCsvSaved = FFileHelper::SaveStringToFile(Csv, *CsvPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	const bool bSummarySaved = FFileHelper::SaveStringToFile(
		Summary,
		*SummaryPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	TestTrue(TEXT("Soak frame CSV saves"), bCsvSaved);
	TestTrue(TEXT("Soak summary saves"), bSummarySaved);
	TestEqual(TEXT("Soak has no correctness failures"), CorrectnessFailures, uint64(0));
	TestEqual(TEXT("Warmed query result capacity does not grow"), MaximumCapacityGrowthBytes, uint64(0));
	TestTrue(
		TEXT("Soak exercises prepared-index hits"),
		SoakPreparedAfter.HitCount > SoakPreparedBefore.HitCount);
	TestTrue(
		TEXT("Soak exercises exact prepared rebuilds"),
		SoakPreparedAfter.FullRebuildCount > SoakPreparedBefore.FullRebuildCount);
	TestTrue(
		TEXT("No sustained one-second run of >1 ms total measured frames"),
		MaximumConsecutiveSlow <= MaximumSustainedSlowFrames);
	AddInfo(FString::Printf(
		TEXT("M2P3_SOAK mode=%s frames=%d simulated_seconds=%.1f actual_seconds=%.3f wall_us=%.3f/%.3f/%.3f/%.3f/%.3f cycles=%.0f/%.0f/%.0f/%.0f/%.0f max_p99_streak=%d max_1ms_streak=%d capacity_growth=%llu prepared=%lld/%lld/%lld/%lld classifications=%d/%d/%d/%d/%d/%d csv=%s"),
		*Mode,
		FrameCount,
		FrameCount / SimulatedFramesPerSecond,
		ActualElapsedSeconds,
		Wall.Median,
		Wall.P95,
		Wall.P99,
		Wall.P999,
		Wall.Maximum,
		Cycles.Median,
		Cycles.P95,
		Cycles.P99,
		Cycles.P999,
		Cycles.Maximum,
		MaximumConsecutiveAboveP99,
		MaximumConsecutiveSlow,
		MaximumCapacityGrowthBytes,
		SoakPreparedAfter.HitCount - SoakPreparedBefore.HitCount,
		SoakPreparedAfter.MissCount - SoakPreparedBefore.MissCount,
		SoakPreparedAfter.FullRebuildCount - SoakPreparedBefore.FullRebuildCount,
		SoakPreparedAfter.EvictionCount - SoakPreparedBefore.EvictionCount,
		ClassificationCounts[0],
		ClassificationCounts[1],
		ClassificationCounts[2],
		ClassificationCounts[3],
		ClassificationCounts[4],
		ClassificationCounts[5],
		*CsvPath));
	return true;
}

#endif
