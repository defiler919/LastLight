#if WITH_DEV_AUTOMATION_TESTS

#include "SightWeaveM2P3Timing.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "SightWeaveWorldSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace SightWeave::M2P3::AttributionTests
{
	using namespace SightWeave::M2P3::Timing;

	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr int32 DynamicStageCount = static_cast<int32>(ESightWeaveDynamicUpdateStage::Count);
	constexpr int32 BatchStageCount = static_cast<int32>(ESightWeaveBatchQueryStage::Count);
	constexpr int32 TotalStageCount = DynamicStageCount + BatchStageCount;
	constexpr double BatchWallLimitMicroseconds = 200.0;
	constexpr double DoorWallLimitMicroseconds = 250.0;
	const FSightWeaveFloorId Ground(FName(TEXT("Ground")));
	const FSightWeaveKnowledgeOwnerId Local(FName(TEXT("Local")));

	enum class EWorkload : uint8
	{
		Batch512,
		BroadDoor4V2L,
		DedicatedDoor,
		DoorPlusMotion,
		HeldReaderDoor
	};

	enum class EClassification : uint8
	{
		WithinBudget,
		PluginCpuOverrun,
		SchedulerPreemption,
		CoreMigrationFrequency,
		MeasurementInstrumentationAnomaly,
		Unknown
	};

	const TCHAR* WorkloadName(const EWorkload Workload)
	{
		switch (Workload)
		{
		case EWorkload::Batch512: return TEXT("batch_512");
		case EWorkload::BroadDoor4V2L: return TEXT("dynamic_door_broad_4v2l");
		case EWorkload::DedicatedDoor: return TEXT("dynamic_door_dedicated");
		case EWorkload::DoorPlusMotion: return TEXT("dynamic_door_plus_motion");
		case EWorkload::HeldReaderDoor: return TEXT("dynamic_door_held_reader_diagnostic");
		default: return TEXT("unknown");
		}
	}

	const TCHAR* ClassificationName(const EClassification Classification)
	{
		switch (Classification)
		{
		case EClassification::WithinBudget: return TEXT("Within budget");
		case EClassification::PluginCpuOverrun: return TEXT("Plugin CPU overrun");
		case EClassification::SchedulerPreemption: return TEXT("Scheduler/preemption");
		case EClassification::CoreMigrationFrequency: return TEXT("Core migration/frequency");
		case EClassification::MeasurementInstrumentationAnomaly: return TEXT("Measurement/instrumentation anomaly");
		case EClassification::Unknown: return TEXT("Unknown");
		default: return TEXT("Unknown");
		}
	}

	const TCHAR* const StageNames[TotalStageCount] =
	{
		TEXT("occluder_normalization"),
		TEXT("spatial_index_patch"),
		TEXT("affected_source_discovery"),
		TEXT("prepared_index_invalidation"),
		TEXT("vision_solve"),
		TEXT("illumination_solve"),
		TEXT("compatible_geometry_reuse"),
		TEXT("snapshot_materialization"),
		TEXT("compatibility_resolution"),
		TEXT("immutable_publication"),
		TEXT("batch_classification"),
		TEXT("batch_result_materialization")
	};

	class FTestWorld
	{
	public:
		explicit FTestWorld(const TCHAR* BaseName)
		{
			const FName WorldName = MakeUniqueObjectName(
				GetTransientPackage(),
				UWorld::StaticClass(),
				FName(BaseName));
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
		Result.Maximum = Values.Last();
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

	TArray<FSightWeaveSegment2D> MakeSegments(const int32 Count, const int32 Seed)
	{
		FRandomStream Random(Seed);
		TArray<FSightWeaveSegment2D> Segments;
		Segments.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const double Radius = Random.FRandRange(100.0f, 1050.0f);
			const double PolarAngle = Random.FRandRange(-PI, PI);
			const FVector2D Center(FMath::Cos(PolarAngle) * Radius, FMath::Sin(PolarAngle) * Radius);
			const double SegmentAngle = Random.FRandRange(-PI, PI);
			const double HalfLength = Random.FRandRange(12.0f, 70.0f);
			const FVector2D Offset(FMath::Cos(SegmentAngle) * HalfLength, FMath::Sin(SegmentAngle) * HalfLength);
			FSightWeaveSegment2D& Segment = Segments.AddDefaulted_GetRef();
			Segment.A = Center - Offset;
			Segment.B = Center + Offset;
			Segment.FloorId = Ground;
			Segment.HeightRange.ZMin = 0.0f;
			Segment.HeightRange.ZMax = 300.0f;
		}
		return Segments;
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
			Result.Compatibility.AcceptedCapabilities =
				{ FName(TEXT("Infrared")), FName(TEXT("Visible")) };
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

	void AccumulateTiming(FTimingSample& Destination, const FTimingSample& Source, const int32 PreviousCount)
	{
		if (PreviousCount == 0)
		{
			Destination = Source;
			return;
		}
		Destination.WallMicroseconds += Source.WallMicroseconds;
		Destination.ThreadCpuMicroseconds += Source.ThreadCpuMicroseconds;
		Destination.KernelMicroseconds += Source.KernelMicroseconds;
		Destination.UserMicroseconds += Source.UserMicroseconds;
		Destination.ThreadCycles += Source.ThreadCycles;
		Destination.EndThreadId = Source.EndThreadId;
		Destination.EndProcessorIndex = Source.EndProcessorIndex;
		Destination.EndCurrentMhz = Source.EndCurrentMhz;
		Destination.EndMaximumMhz = Source.EndMaximumMhz;
		Destination.ProcessPageFaultDelta += Source.ProcessPageFaultDelta;
		Destination.bThreadCycleTimeValid &= Source.bThreadCycleTimeValid;
		Destination.bThreadCpuTimeValid &= Source.bThreadCpuTimeValid;
		Destination.bProcessorNumberValid &= Source.bProcessorNumberValid;
		Destination.bProcessPageFaultCountValid &= Source.bProcessPageFaultCountValid;
		Destination.bProcessorFrequencyValid &= Source.bProcessorFrequencyValid;
		Destination.bThreadMigrated |= Source.bThreadMigrated;
		Destination.bFrequencyChanged |= Source.bFrequencyChanged;
		Destination.bMeasurementAnomaly |= Source.bMeasurementAnomaly;
	}

	struct FStageCollector
	{
		FStageCollector()
		{
			for (FDualClockTimer& Timer : Timers)
			{
				Timer.SetCaptureAuxiliaryEvidence(false);
			}
		}

		void Reset()
		{
			Samples = {};
			InvocationCounts = {};
			Active = {};
		}

		void Begin(const int32 StageIndex)
		{
			const bool bValidStage = StageIndex >= 0 && StageIndex < TotalStageCount;
			if (!bValidStage || Active[StageIndex])
			{
				if (bValidStage)
				{
					Samples[StageIndex].bMeasurementAnomaly = true;
				}
				return;
			}
			Active[StageIndex] = true;
			Timers[StageIndex].Start();
		}

		void End(const int32 StageIndex)
		{
			const bool bValidStage = StageIndex >= 0 && StageIndex < TotalStageCount;
			if (!bValidStage || !Active[StageIndex])
			{
				if (bValidStage)
				{
					Samples[StageIndex].bMeasurementAnomaly = true;
				}
				return;
			}
			Active[StageIndex] = false;
			const FTimingSample Sample = Timers[StageIndex].Stop();
			AccumulateTiming(Samples[StageIndex], Sample, InvocationCounts[StageIndex]++);
		}

		TStaticArray<FDualClockTimer, TotalStageCount> Timers;
		TStaticArray<FTimingSample, TotalStageCount> Samples;
		TStaticArray<int32, TotalStageCount> InvocationCounts;
		TStaticArray<bool, TotalStageCount> Active;
	};

	thread_local FStageCollector* GActiveStageCollector = nullptr;

	void DynamicStageProbe(const ESightWeaveDynamicUpdateStage Stage, const bool bBegin)
	{
		if (!GActiveStageCollector)
		{
			return;
		}
		const int32 StageIndex = static_cast<int32>(Stage);
		bBegin ? GActiveStageCollector->Begin(StageIndex) : GActiveStageCollector->End(StageIndex);
	}

	void BatchStageProbe(const ESightWeaveBatchQueryStage Stage, const bool bBegin)
	{
		if (!GActiveStageCollector)
		{
			return;
		}
		const int32 StageIndex = DynamicStageCount + static_cast<int32>(Stage);
		bBegin ? GActiveStageCollector->Begin(StageIndex) : GActiveStageCollector->End(StageIndex);
	}

	struct FProbeRegistration
	{
		FProbeRegistration()
		{
			USightWeaveWorldSubsystem::SetDynamicUpdateStageProbeForTesting(&DynamicStageProbe);
			USightWeaveWorldSubsystem::SetBatchQueryStageProbeForTesting(&BatchStageProbe);
		}

		~FProbeRegistration()
		{
			GActiveStageCollector = nullptr;
			USightWeaveWorldSubsystem::SetDynamicUpdateStageProbeForTesting(nullptr);
			USightWeaveWorldSubsystem::SetBatchQueryStageProbeForTesting(nullptr);
		}
	};

	struct FRawRow
	{
		EWorkload Workload = EWorkload::Batch512;
		EClassification Classification = EClassification::WithinBudget;
		int32 DistributionIndex = 0;
		int32 SampleIndex = 0;
		FTimingSample Total;
		FTimingSample ComputeControl;
		FTimingSample MemoryControl;
		TStaticArray<FTimingSample, TotalStageCount> Stages;
		int32 FastPath = 0;
		int32 VisionSourceCount = 0;
		int32 IlluminationSourceCount = 0;
		bool bCapacityMeasured = false;
		uint64 CapacityGrowthBytes = 0;
		int64 PreparedHitDelta = 0;
		int64 PreparedMissDelta = 0;
		int64 PreparedRebuildDelta = 0;
		int64 PreparedEvictionDelta = 0;
		int64 SnapshotRevisionBefore = 0;
		int64 SnapshotRevisionAfter = 0;
	};

	FTimingSample MeasureControl(const bool bCompute, const FFixedWorkControls& Controls)
	{
		uint64 Result = 0;
		FDualClockTimer Timer;
		Timer.Start();
		Result = bCompute ? Controls.RunCompute() : Controls.RunMemory();
		const FTimingSample Sample = Timer.Stop();
		ConsumeControlResult(Result);
		return Sample;
	}

	double MedianForRows(
		const TArray<FRawRow>& Rows,
		const EWorkload Workload,
		TFunctionRef<double(const FRawRow&)> Selector)
	{
		TArray<double> Values;
		for (const FRawRow& Row : Rows)
		{
			if (Row.Workload == Workload)
			{
				Values.Add(Selector(Row));
			}
		}
		return Summarize(MoveTemp(Values)).Median;
	}

	double ComparableTargetMedian(
		const TArray<FRawRow>& Rows,
		const FRawRow& Reference,
		const bool bWallPerCycle)
	{
		auto Gather = [&](const bool bRequireSameProcessor)
		{
			TArray<double> Values;
			for (const FRawRow& Candidate : Rows)
			{
				const bool bSameState = Reference.Workload == EWorkload::Batch512
					|| (Candidate.SampleIndex & 1) == (Reference.SampleIndex & 1);
				const bool bSameProcessor = !bRequireSameProcessor
					|| (!Candidate.Total.bThreadMigrated
						&& Candidate.Total.StartProcessorIndex == Reference.Total.StartProcessorIndex);
				if (Candidate.Workload == Reference.Workload && bSameState && bSameProcessor)
				{
					Values.Add(bWallPerCycle
						? Candidate.Total.WallMicroseconds
							/ FMath::Max(1.0, static_cast<double>(Candidate.Total.ThreadCycles))
						: static_cast<double>(Candidate.Total.ThreadCycles));
				}
			}
			return Values;
		};

		TArray<double> Comparable = Gather(true);
		if (Comparable.Num() < 8)
		{
			Comparable = Gather(false);
		}
		return Summarize(MoveTemp(Comparable)).Median;
	}

	void ClassifyRows(TArray<FRawRow>& Rows, const EWorkload Workload, const double WallLimitMicroseconds)
	{
		const double MedianComputeWallPerCycle = MedianForRows(Rows, Workload, [](const FRawRow& Row)
		{
			return Row.ComputeControl.WallMicroseconds
				/ FMath::Max(1.0, static_cast<double>(Row.ComputeControl.ThreadCycles));
		});
		const double MedianMemoryWallPerCycle = MedianForRows(Rows, Workload, [](const FRawRow& Row)
		{
			return Row.MemoryControl.WallMicroseconds
				/ FMath::Max(1.0, static_cast<double>(Row.MemoryControl.ThreadCycles));
		});

		for (FRawRow& Row : Rows)
		{
			if (Row.Workload != Workload || Row.Total.WallMicroseconds <= WallLimitMicroseconds)
			{
				continue;
			}
			const double TargetWallPerCycle = Row.Total.WallMicroseconds
				/ FMath::Max(1.0, static_cast<double>(Row.Total.ThreadCycles));
			const double MedianCycles = ComparableTargetMedian(Rows, Row, false);
			const double MedianWallPerCycle = ComparableTargetMedian(Rows, Row, true);
			const double ComputeWallPerCycle = Row.ComputeControl.WallMicroseconds
				/ FMath::Max(1.0, static_cast<double>(Row.ComputeControl.ThreadCycles));
			const double MemoryWallPerCycle = Row.MemoryControl.WallMicroseconds
				/ FMath::Max(1.0, static_cast<double>(Row.MemoryControl.ThreadCycles));
			if (Row.Total.bMeasurementAnomaly || !Row.Total.bThreadCycleTimeValid)
			{
				Row.Classification = EClassification::MeasurementInstrumentationAnomaly;
			}
			else if ((Row.Total.bThreadMigrated || Row.Total.bFrequencyChanged)
				&& TargetWallPerCycle > MedianWallPerCycle * 1.20)
			{
				Row.Classification = EClassification::CoreMigrationFrequency;
			}
			else if (static_cast<double>(Row.Total.ThreadCycles) > MedianCycles * 1.50
				&& TargetWallPerCycle <= MedianWallPerCycle * 1.25)
			{
				Row.Classification = EClassification::PluginCpuOverrun;
			}
			else if (TargetWallPerCycle > MedianWallPerCycle * 1.35
				&& (static_cast<double>(Row.Total.ThreadCycles) <= MedianCycles * 1.25
					|| Row.Total.ProcessPageFaultDelta > 0
					|| ComputeWallPerCycle > MedianComputeWallPerCycle * 1.35
					|| MemoryWallPerCycle > MedianMemoryWallPerCycle * 1.35))
			{
				Row.Classification = EClassification::SchedulerPreemption;
			}
			else
			{
				Row.Classification = EClassification::Unknown;
			}
		}
	}

	FString CsvHeader()
	{
		FString Header(TEXT("run,workload,distribution,sample,wall_us,thread_cpu_us,kernel_us,user_us,thread_cycles,start_thread,end_thread,start_processor,end_processor,migrated,start_mhz,end_mhz,frequency_changed,page_faults,measurement_anomaly,compute_wall_us,compute_thread_cpu_us,compute_cycles,compute_start_processor,compute_end_processor,compute_migrated,memory_wall_us,memory_thread_cpu_us,memory_cycles,memory_start_processor,memory_end_processor,memory_migrated,fast_path,vision_sources,illumination_sources,capacity_measured,capacity_growth_bytes,prepared_hit_delta,prepared_miss_delta,prepared_rebuild_delta,prepared_eviction_delta,snapshot_revision_before,snapshot_revision_after"));
		for (int32 StageIndex = 0; StageIndex < TotalStageCount; ++StageIndex)
		{
			Header += FString::Printf(
				TEXT(",%s_wall_us,%s_thread_cpu_us,%s_cycles"),
				StageNames[StageIndex],
				StageNames[StageIndex],
				StageNames[StageIndex]);
		}
		Header += TEXT(",classification,allocation_proof_status\n");
		return Header;
	}

	FString MakeCsv(const TArray<FRawRow>& Rows, const FString& RunLabel)
	{
		FString Csv = CsvHeader();
		for (const FRawRow& Row : Rows)
		{
			Csv += FString::Printf(
				TEXT("%s,%s,%d,%d,%.3f,%.3f,%.3f,%.3f,%llu,%u,%u,%d,%d,%d,%u,%u,%d,%u,%d,%.3f,%.3f,%llu,%d,%d,%d,%.3f,%.3f,%llu,%d,%d,%d,%d,%d,%d,%d,%llu,%lld,%lld,%lld,%lld,%lld,%lld"),
				*RunLabel,
				WorkloadName(Row.Workload),
				Row.DistributionIndex,
				Row.SampleIndex,
				Row.Total.WallMicroseconds,
				Row.Total.ThreadCpuMicroseconds,
				Row.Total.KernelMicroseconds,
				Row.Total.UserMicroseconds,
				Row.Total.ThreadCycles,
				Row.Total.StartThreadId,
				Row.Total.EndThreadId,
				Row.Total.StartProcessorIndex,
				Row.Total.EndProcessorIndex,
				Row.Total.bThreadMigrated,
				Row.Total.StartCurrentMhz,
				Row.Total.EndCurrentMhz,
				Row.Total.bFrequencyChanged,
				Row.Total.ProcessPageFaultDelta,
				Row.Total.bMeasurementAnomaly,
				Row.ComputeControl.WallMicroseconds,
				Row.ComputeControl.ThreadCpuMicroseconds,
				Row.ComputeControl.ThreadCycles,
				Row.ComputeControl.StartProcessorIndex,
				Row.ComputeControl.EndProcessorIndex,
				Row.ComputeControl.bThreadMigrated,
				Row.MemoryControl.WallMicroseconds,
				Row.MemoryControl.ThreadCpuMicroseconds,
				Row.MemoryControl.ThreadCycles,
				Row.MemoryControl.StartProcessorIndex,
				Row.MemoryControl.EndProcessorIndex,
				Row.MemoryControl.bThreadMigrated,
				Row.FastPath,
				Row.VisionSourceCount,
				Row.IlluminationSourceCount,
				Row.bCapacityMeasured,
				Row.CapacityGrowthBytes,
				Row.PreparedHitDelta,
				Row.PreparedMissDelta,
				Row.PreparedRebuildDelta,
				Row.PreparedEvictionDelta,
				Row.SnapshotRevisionBefore,
				Row.SnapshotRevisionAfter);
			for (int32 StageIndex = 0; StageIndex < TotalStageCount; ++StageIndex)
			{
				const FTimingSample& Stage = Row.Stages[StageIndex];
				Csv += FString::Printf(
					TEXT(",%.3f,%.3f,%llu"),
					Stage.WallMicroseconds,
					Stage.ThreadCpuMicroseconds,
					Stage.ThreadCycles);
			}
			Csv += FString::Printf(
				TEXT(",%s,%s\n"),
				ClassificationName(Row.Classification),
				Row.bCapacityMeasured && Row.CapacityGrowthBytes == 0
					? TEXT("zero_capacity_growth_separate_allocation_trace_required")
					: Row.bCapacityMeasured
						? TEXT("capacity_growth_detected")
						: TEXT("separate_allocation_trace_required"));
		}
		return Csv;
	}

	bool GetCaptureParameters(FString& OutDirectory, FString& OutRunLabel)
	{
		if (!FParse::Param(FCommandLine::Get(), TEXT("SightWeaveM2P3AttributionCapture")))
		{
			return false;
		}
		if (!FParse::Value(FCommandLine::Get(), TEXT("SightWeaveM2P3Output="), OutDirectory))
		{
			return false;
		}
		if (!FParse::Value(FCommandLine::Get(), TEXT("SightWeaveM2P3Run="), OutRunLabel))
		{
			OutRunLabel = TEXT("manual");
		}
		OutDirectory = FPaths::ConvertRelativePathToFull(OutDirectory);
		return true;
	}

	bool SaveRows(
		FAutomationTestBase& Test,
		const TArray<FRawRow>& Rows,
		const FString& Directory,
		const FString& RunLabel,
		const TCHAR* FileName)
	{
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = FPaths::Combine(Directory, FileName);
		const bool bSaved = FFileHelper::SaveStringToFile(
			MakeCsv(Rows, RunLabel),
			*Path,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		Test.TestTrue(*FString::Printf(TEXT("Raw attribution CSV writes: %s"), *Path), bSaved);
		Test.AddInfo(FString::Printf(TEXT("M2P3_ATTRIBUTION_REPORT path=%s rows=%d"), *Path, Rows.Num()));
		return bSaved;
	}

	void LogClassificationSummary(
		FAutomationTestBase& Test,
		const TArray<FRawRow>& Rows,
		const EWorkload Workload,
		const double WallLimit)
	{
		TArray<double> Wall;
		TArray<double> Cycles;
		int32 Counts[6] = {};
		for (const FRawRow& Row : Rows)
		{
			if (Row.Workload != Workload)
			{
				continue;
			}
			Wall.Add(Row.Total.WallMicroseconds);
			Cycles.Add(static_cast<double>(Row.Total.ThreadCycles));
			++Counts[static_cast<int32>(Row.Classification)];
		}
		const FDistribution WallStats = Summarize(MoveTemp(Wall));
		const FDistribution CycleStats = Summarize(MoveTemp(Cycles));
		Test.AddInfo(FString::Printf(
			TEXT("M2P3_ATTRIBUTION_SUMMARY workload=%s samples=%d wall_limit_us=%.0f wall_us=%.3f/%.3f/%.3f/%.3f cycles=%.0f/%.0f/%.0f/%.0f within=%d plugin_cpu=%d scheduler=%d migration_frequency=%d anomaly=%d unknown=%d"),
			WorkloadName(Workload),
			Counts[0] + Counts[1] + Counts[2] + Counts[3] + Counts[4] + Counts[5],
			WallLimit,
			WallStats.Median,
			WallStats.P95,
			WallStats.P99,
			WallStats.Maximum,
			CycleStats.Median,
			CycleStats.P95,
			CycleStats.P99,
			CycleStats.Maximum,
			Counts[0],
			Counts[1],
			Counts[2],
			Counts[3],
			Counts[4],
			Counts[5]));
	}

	struct FRuntimeFixture
	{
		TArray<FSightWeaveVisionSourceHandle> VisionHandles;
		TArray<FSightWeaveVisionSourceDescription> VisionDescriptions;
	};

	bool InitializeFixture(
		FAutomationTestBase& Test,
		USightWeaveWorldSubsystem* Subsystem,
		const int32 VisionCount,
		const int32 IlluminationCount,
		FRuntimeFixture& OutFixture)
	{
		if (!Test.TestNotNull(TEXT("Subsystem exists"), Subsystem)
			|| !Test.TestTrue(TEXT("Floor registers"), Subsystem->RegisterFloor(Floor(), nullptr)))
		{
			return false;
		}
		TArray<FSightWeaveSegment2D> StaticSegments = MakeSegments(64, 0x51A7E);
		if (!Test.TestTrue(
			TEXT("Static 64-segment fixture registers"),
			Subsystem->RegisterOccluder(StaticSegments, false, true, nullptr).IsValid()))
		{
			return false;
		}
		for (int32 SourceIndex = 0; SourceIndex < VisionCount; ++SourceIndex)
		{
			const double Angle = 2.0 * PI * SourceIndex / FMath::Max(1, VisionCount);
			const ESightWeaveSourceShape Shape = SourceIndex % 2 == 0
				? ESightWeaveSourceShape::Radial
				: ESightWeaveSourceShape::CameraCone;
			const ESightWeaveIlluminationPolicy Policy = SourceIndex < 2
				? ESightWeaveIlluminationPolicy::BypassLegalIllumination
				: ESightWeaveIlluminationPolicy::RequiresLegalIllumination;
			OutFixture.VisionDescriptions.Add(Vision(
				FVector(FMath::Cos(Angle) * 100.0, FMath::Sin(Angle) * 100.0, 100.0),
				Shape,
				Policy));
			OutFixture.VisionHandles.Add(
				Subsystem->RegisterVisionSource(OutFixture.VisionDescriptions.Last(), nullptr));
			if (!Test.TestTrue(
				*FString::Printf(TEXT("Vision source %d registers"), SourceIndex),
				OutFixture.VisionHandles.Last().IsValid()))
			{
				return false;
			}
		}
		for (int32 LightIndex = 0; LightIndex < IlluminationCount; ++LightIndex)
		{
			const double X = LightIndex % 2 == 0 ? 100.0 : -100.0;
			const FName Capability = LightIndex % 2 == 0
				? FName(TEXT("Visible"))
				: FName(TEXT("Infrared"));
			if (!Test.TestTrue(
				*FString::Printf(TEXT("Illumination source %d registers"), LightIndex),
				Subsystem->RegisterIlluminationSource(
					Light(FVector(X, 0.0, 100.0), Capability), nullptr).IsValid()))
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P3BatchAttributionTest,
	"SightWeave.M2P3.Attribution.Batch512",
	SightWeave::M2P3::AttributionTests::TestFlags)

bool FSightWeaveM2P3BatchAttributionTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P3::AttributionTests;
	FString OutputDirectory;
	FString RunLabel;
	if (!GetCaptureParameters(OutputDirectory, RunLabel))
	{
		AddInfo(TEXT("Batch attribution requires -SightWeaveM2P3AttributionCapture -SightWeaveM2P3Output=<directory>."));
		return true;
	}

	FProbeRegistration ProbeRegistration;
	FTestWorld World(TEXT("SightWeaveM2P3BatchAttribution"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	FRuntimeFixture Fixture;
	if (!InitializeFixture(*this, Subsystem, 4, 2, Fixture))
	{
		return false;
	}

	TArray<FSightWeaveQueryRequest> Requests;
	Requests.Reserve(512);
	for (int32 RequestIndex = 0; RequestIndex < 512; ++RequestIndex)
	{
		const double Angle = 2.0 * PI * RequestIndex / 512.0;
		FSightWeaveQueryRequest& Request = Requests.AddDefaulted_GetRef();
		Request.KnowledgeOwnerId = Local;
		Request.FloorId = Ground;
		Request.SampleSet.Samples.Add(
			FVector(FMath::Cos(Angle) * 500.0, FMath::Sin(Angle) * 500.0, 100.0));
	}

	TArray<FSightWeaveVisibilityQueryResult> Results;
	Subsystem->QueryBatch(Requests, Results);
	for (int32 RequestIndex = 0; RequestIndex < Requests.Num(); ++RequestIndex)
	{
		const FSightWeaveVisibilityQueryResult PointResult = Subsystem->QueryEffectiveLiveAtLocation(
			Requests[RequestIndex].KnowledgeOwnerId,
			Requests[RequestIndex].FloorId,
			Requests[RequestIndex].SampleSet.Samples[Requests[RequestIndex].SampleSet.AnchorIndex]);
		if (!FSightWeaveVisibilityQueryResult::StaticStruct()->CompareScriptStruct(
			&Results[RequestIndex],
			&PointResult,
			0))
		{
			AddError(FString::Printf(
				TEXT("Batch result differs from independent all-field point result at %d"),
				RequestIndex));
			return false;
		}
	}

	FFixedWorkControls Controls;
	for (int32 Warmup = 0; Warmup < 8; ++Warmup)
	{
		ConsumeControlResult(Controls.RunCompute());
		ConsumeControlResult(Controls.RunMemory());
	}
	TArray<FRawRow> Rows;
	Rows.Reserve(10 * 101);
	FStageCollector StageCollector;
	for (int32 DistributionIndex = 0; DistributionIndex < 10; ++DistributionIndex)
	{
		for (int32 Warmup = 0; Warmup < 10; ++Warmup)
		{
			Subsystem->QueryBatch(Requests, Results);
		}
		for (int32 SampleIndex = 0; SampleIndex < 101; ++SampleIndex)
		{
			FRawRow Row;
			Row.Workload = EWorkload::Batch512;
			Row.DistributionIndex = DistributionIndex;
			Row.SampleIndex = SampleIndex;
			Row.ComputeControl = MeasureControl(true, Controls);
			const uint64 OuterBytesBefore = Results.GetAllocatedSize();
			const uint64 InnerBytesBefore = QueryResultAllocatedBytes(Results);
			const FSightWeavePreparedEventIndexStats PreparedBefore =
				Subsystem->GetPreparedEventIndexStats();
			StageCollector.Reset();
			GActiveStageCollector = &StageCollector;
			FDualClockTimer TotalTimer;
			TotalTimer.Start();
			Subsystem->QueryBatch(Requests, Results);
			Row.Total = TotalTimer.Stop();
			GActiveStageCollector = nullptr;
			Row.MemoryControl = MeasureControl(false, Controls);
			Row.Stages = StageCollector.Samples;
			const FSightWeaveBatchQueryDiagnostics& Diagnostics =
				Subsystem->GetLastBatchQueryDiagnostics();
			Row.FastPath = Diagnostics.bFastPath ? 1 : 0;
			Row.VisionSourceCount = Diagnostics.VisionSourceCount;
			Row.IlluminationSourceCount = Diagnostics.IlluminationSourceCount;
			Row.bCapacityMeasured = true;
			Row.CapacityGrowthBytes =
				(Results.GetAllocatedSize() - OuterBytesBefore)
				+ (QueryResultAllocatedBytes(Results) - InnerBytesBefore);
			const FSightWeavePreparedEventIndexStats PreparedAfter =
				Subsystem->GetPreparedEventIndexStats();
			Row.PreparedHitDelta = PreparedAfter.HitCount - PreparedBefore.HitCount;
			Row.PreparedMissDelta = PreparedAfter.MissCount - PreparedBefore.MissCount;
			Row.PreparedRebuildDelta = PreparedAfter.FullRebuildCount - PreparedBefore.FullRebuildCount;
			Row.PreparedEvictionDelta = PreparedAfter.EvictionCount - PreparedBefore.EvictionCount;
			Row.SnapshotRevisionBefore = Subsystem->GetRevision().GetValue();
			const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> Published =
				Subsystem->AcquirePublishedSnapshotForTesting();
			Row.SnapshotRevisionAfter = Published.IsValid() ? Published->Revision.GetValue() : 0;
			TestEqual(TEXT("Batch sample retains zero capacity growth"), Row.CapacityGrowthBytes, uint64(0));
			TestTrue(TEXT("Batch sample uses uniform fast path"), Diagnostics.bFastPath);
			TestEqual(TEXT("Batch sample returns all 512 results"), Results.Num(), 512);
			Rows.Add(Row);
		}
	}
	ClassifyRows(Rows, EWorkload::Batch512, BatchWallLimitMicroseconds);
	LogClassificationSummary(*this, Rows, EWorkload::Batch512, BatchWallLimitMicroseconds);
	SaveRows(*this, Rows, OutputDirectory, RunLabel, TEXT("batch.csv"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P3DoorAttributionTest,
	"SightWeave.M2P3.Attribution.DynamicDoor",
	SightWeave::M2P3::AttributionTests::TestFlags)

bool FSightWeaveM2P3DoorAttributionTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P3::AttributionTests;
	FString OutputDirectory;
	FString RunLabel;
	if (!GetCaptureParameters(OutputDirectory, RunLabel))
	{
		AddInfo(TEXT("Door attribution requires -SightWeaveM2P3AttributionCapture -SightWeaveM2P3Output=<directory>."));
		return true;
	}

	FProbeRegistration ProbeRegistration;
	FFixedWorkControls Controls;
	TArray<FRawRow> Rows;
	Rows.Reserve(101 * 3 + 11);

	auto RunScenario = [this, &Controls, &Rows](
		const EWorkload Workload,
		const int32 VisionCount,
		const int32 IlluminationCount,
		const int32 SampleCount,
		const bool bMoveSource,
		const bool bHoldReader)
	{
		FTestWorld World(*FString::Printf(TEXT("SightWeaveM2P3Door%d"), static_cast<int32>(Workload)));
		USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
		FRuntimeFixture Fixture;
		if (!InitializeFixture(*this, Subsystem, VisionCount, IlluminationCount, Fixture))
		{
			return false;
		}
		FSightWeaveSegment2D Door;
		Door.A = FVector2D(250.0, -100.0);
		Door.B = FVector2D(250.0, 100.0);
		Door.FloorId = Ground;
		Door.HeightRange.ZMin = 0.0f;
		Door.HeightRange.ZMax = 300.0f;
		TArray<FSightWeaveSegment2D> DoorSegments;
		DoorSegments.Add(Door);
		const FSightWeaveOccluderHandle DoorHandle =
			Subsystem->RegisterOccluder(DoorSegments, true, true, nullptr);
		if (!TestTrue(TEXT("Dynamic door registers"), DoorHandle.IsValid()))
		{
			return false;
		}
		for (int32 Warmup = 0; Warmup < 10; ++Warmup)
		{
			const double X = Warmup % 2 == 0 ? 850.0 : 250.0;
			DoorSegments[0].A.X = X;
			DoorSegments[0].B.X = X;
			Subsystem->UpdateOccluder(DoorHandle, DoorSegments, true, true);
		}
		FStageCollector StageCollector;
		for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
		{
			FRawRow Row;
			Row.Workload = Workload;
			Row.SampleIndex = SampleIndex;
			Row.VisionSourceCount = VisionCount;
			Row.IlluminationSourceCount = IlluminationCount;
			Row.ComputeControl = MeasureControl(true, Controls);
			const FSightWeavePreparedEventIndexStats PreparedBefore =
				Subsystem->GetPreparedEventIndexStats();
			Row.SnapshotRevisionBefore = Subsystem->GetRevision().GetValue();
			TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> HeldSnapshot;
			if (bHoldReader)
			{
				HeldSnapshot = Subsystem->AcquirePublishedSnapshotForTesting();
			}
			const double X = SampleIndex % 2 == 0 ? 850.0 : 250.0;
			DoorSegments[0].A.X = X;
			DoorSegments[0].B.X = X;
			StageCollector.Reset();
			GActiveStageCollector = &StageCollector;
			FDualClockTimer TotalTimer;
			TotalTimer.Start();
			const bool bDoorUpdated =
				Subsystem->UpdateOccluder(DoorHandle, DoorSegments, true, true);
			bool bMotionUpdated = true;
			if (bMoveSource && !Fixture.VisionHandles.IsEmpty())
			{
				FTransform& Transform = Fixture.VisionDescriptions[0].Transform;
				Transform.SetLocation(FVector(SampleIndex % 2 == 0 ? 5.0 : 0.0, 0.0, 100.0));
				bMotionUpdated = Subsystem->UpdateVisionSourceTransform(
					Fixture.VisionHandles[0],
					Transform);
			}
			Row.Total = TotalTimer.Stop();
			GActiveStageCollector = nullptr;
			HeldSnapshot.Reset();
			Row.MemoryControl = MeasureControl(false, Controls);
			Row.Stages = StageCollector.Samples;
			const FSightWeavePreparedEventIndexStats PreparedAfter =
				Subsystem->GetPreparedEventIndexStats();
			Row.PreparedHitDelta = PreparedAfter.HitCount - PreparedBefore.HitCount;
			Row.PreparedMissDelta = PreparedAfter.MissCount - PreparedBefore.MissCount;
			Row.PreparedRebuildDelta = PreparedAfter.FullRebuildCount - PreparedBefore.FullRebuildCount;
			Row.PreparedEvictionDelta = PreparedAfter.EvictionCount - PreparedBefore.EvictionCount;
			const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> Published =
				Subsystem->AcquirePublishedSnapshotForTesting();
			Row.SnapshotRevisionAfter = Published.IsValid() ? Published->Revision.GetValue() : 0;
			TestTrue(TEXT("Dynamic door update succeeds"), bDoorUpdated);
			TestTrue(TEXT("Optional source motion succeeds"), bMotionUpdated);
			TestEqual(
				TEXT("Synchronous publication has no stale snapshot window"),
				Row.SnapshotRevisionAfter,
				Subsystem->GetRevision().GetValue());
			Rows.Add(Row);
		}
		return true;
	};

	for (int32 Warmup = 0; Warmup < 8; ++Warmup)
	{
		ConsumeControlResult(Controls.RunCompute());
		ConsumeControlResult(Controls.RunMemory());
	}
	if (!RunScenario(EWorkload::BroadDoor4V2L, 4, 2, 101, false, false)
		|| !RunScenario(EWorkload::DedicatedDoor, 1, 0, 101, false, false)
		|| !RunScenario(EWorkload::DoorPlusMotion, 4, 2, 101, true, false)
		|| !RunScenario(EWorkload::HeldReaderDoor, 4, 2, 11, false, true))
	{
		return false;
	}

	for (const EWorkload Workload :
		{ EWorkload::BroadDoor4V2L,
			EWorkload::DedicatedDoor,
			EWorkload::DoorPlusMotion,
			EWorkload::HeldReaderDoor })
	{
		ClassifyRows(Rows, Workload, DoorWallLimitMicroseconds);
		LogClassificationSummary(*this, Rows, Workload, DoorWallLimitMicroseconds);
	}
	SaveRows(*this, Rows, OutputDirectory, RunLabel, TEXT("door.csv"));
	return true;
}

#endif
