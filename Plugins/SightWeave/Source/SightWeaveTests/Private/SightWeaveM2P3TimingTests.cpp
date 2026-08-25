#if WITH_DEV_AUTOMATION_TESTS

#include "SightWeaveM2P3Timing.h"

#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"

namespace SightWeave::M2P3::TimingTests
{
	using namespace SightWeave::M2P3::Timing;

	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	struct FDistribution
	{
		double Minimum = 0.0;
		double Median = 0.0;
		double P95 = 0.0;
		double P99 = 0.0;
		double Maximum = 0.0;
	};

	double NearestRank(const TArray<double>& Sorted, const double Percentile)
	{
		if (Sorted.IsEmpty())
		{
			return 0.0;
		}
		const int32 Rank = FMath::Clamp(
			FMath::CeilToInt(Percentile * Sorted.Num()) - 1,
			0,
			Sorted.Num() - 1);
		return Sorted[Rank];
	}

	FDistribution Summarize(TArray<double> Values)
	{
		Values.Sort();
		FDistribution Result;
		if (!Values.IsEmpty())
		{
			Result.Minimum = Values[0];
			Result.Median = NearestRank(Values, 0.50);
			Result.P95 = NearestRank(Values, 0.95);
			Result.P99 = NearestRank(Values, 0.99);
			Result.Maximum = Values.Last();
		}
		return Result;
	}

	void MeasureControl(
		const bool bCompute,
		const FFixedWorkControls& Controls,
		TArray<FTimingSample>& OutSamples)
	{
		uint64 Result = 0;
		FDualClockTimer Timer;
		Timer.Start();
		Result = bCompute ? Controls.RunCompute() : Controls.RunMemory();
		OutSamples.Add(Timer.Stop());
		ConsumeControlResult(Result);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P3DualClockCalibrationTest,
	"SightWeave.M2P3.Timing.DualClockCalibration",
	SightWeave::M2P3::TimingTests::TestFlags)

bool FSightWeaveM2P3DualClockCalibrationTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P3::Timing;
	using namespace SightWeave::M2P3::TimingTests;

	const FPlatformCapabilities Capabilities = GetPlatformCapabilities();
	AddInfo(FString::Printf(
		TEXT("M2P3_TIMING_CAPABILITIES platform=%s cycles=%d thread_time=%d processor=%d page_faults=%d frequency=%d elevated=%d etw_context_switch_expected=%d trace_requested=%d"),
		PLATFORM_WINDOWS ? TEXT("Windows") : TEXT("Other"),
		Capabilities.bThreadCycleTimeAvailable,
		Capabilities.bThreadKernelUserTimeAvailable,
		Capabilities.bProcessorNumberAvailable,
		Capabilities.bProcessPageFaultCountAvailable,
		Capabilities.bProcessorFrequencyAvailable,
		Capabilities.bRunningElevated,
		Capabilities.bContextSwitchEtwExpectedAvailable,
		Capabilities.bContextSwitchTraceRequested));

#if !PLATFORM_WINDOWS
	AddInfo(TEXT("M2P3 dual-clock Windows evidence is platform-guarded; generic wall-clock smoke continues."));
#else
	if (!TestTrue(TEXT("QueryThreadCycleTime is available"), Capabilities.bThreadCycleTimeAvailable)
		|| !TestTrue(TEXT("GetThreadTimes is available"), Capabilities.bThreadKernelUserTimeAvailable)
		|| !TestTrue(TEXT("Processor-number evidence is available"), Capabilities.bProcessorNumberAvailable)
		|| !TestTrue(TEXT("Process page-fault evidence is available"), Capabilities.bProcessPageFaultCountAvailable))
	{
		return false;
	}
#endif

	FFixedWorkControls Controls;
	for (int32 Warmup = 0; Warmup < 8; ++Warmup)
	{
		ConsumeControlResult(Controls.RunCompute());
		ConsumeControlResult(Controls.RunMemory());
	}

	TArray<FTimingSample> EmptySamples;
	TArray<FTimingSample> ComputeSamples;
	TArray<FTimingSample> MemorySamples;
	EmptySamples.Reserve(101);
	ComputeSamples.Reserve(101);
	MemorySamples.Reserve(101);
	for (int32 SampleIndex = 0; SampleIndex < 101; ++SampleIndex)
	{
		FDualClockTimer EmptyTimer;
		EmptyTimer.Start();
		EmptySamples.Add(EmptyTimer.Stop());
		MeasureControl(true, Controls, ComputeSamples);
		MeasureControl(false, Controls, MemorySamples);
	}

	TArray<double> EmptyWall;
	TArray<double> ComputeWall;
	TArray<double> ComputeCpu;
	TArray<double> ComputeCycles;
	TArray<double> MemoryWall;
	TArray<double> MemoryCpu;
	EmptyWall.Reserve(101);
	ComputeWall.Reserve(101);
	ComputeCpu.Reserve(101);
	ComputeCycles.Reserve(101);
	MemoryWall.Reserve(101);
	MemoryCpu.Reserve(101);
	int32 ComputeNonZeroCpuCount = 0;
	int32 MemoryNonZeroCpuCount = 0;
	uint64 MinimumNonZeroCpu100ns = MAX_uint64;
	for (const FTimingSample& Sample : EmptySamples)
	{
		EmptyWall.Add(Sample.WallMicroseconds);
		TestFalse(TEXT("Empty timing sample is internally valid"), Sample.bMeasurementAnomaly);
	}
	for (const FTimingSample& Sample : ComputeSamples)
	{
		ComputeWall.Add(Sample.WallMicroseconds);
		ComputeCpu.Add(Sample.ThreadCpuMicroseconds);
		ComputeCycles.Add(static_cast<double>(Sample.ThreadCycles));
		ComputeNonZeroCpuCount += Sample.ThreadCpuMicroseconds > 0.0 ? 1 : 0;
		if (Sample.ThreadCpuMicroseconds > 0.0)
		{
			MinimumNonZeroCpu100ns = FMath::Min(
				MinimumNonZeroCpu100ns,
				static_cast<uint64>(FMath::RoundToDouble(Sample.ThreadCpuMicroseconds * 10.0)));
		}
		TestTrue(TEXT("Compute sample has a cycle delta"), Sample.bThreadCycleTimeValid && Sample.ThreadCycles > 0);
		TestFalse(TEXT("Compute timing sample is internally valid"), Sample.bMeasurementAnomaly);
	}
	for (const FTimingSample& Sample : MemorySamples)
	{
		MemoryWall.Add(Sample.WallMicroseconds);
		MemoryCpu.Add(Sample.ThreadCpuMicroseconds);
		MemoryNonZeroCpuCount += Sample.ThreadCpuMicroseconds > 0.0 ? 1 : 0;
		if (Sample.ThreadCpuMicroseconds > 0.0)
		{
			MinimumNonZeroCpu100ns = FMath::Min(
				MinimumNonZeroCpu100ns,
				static_cast<uint64>(FMath::RoundToDouble(Sample.ThreadCpuMicroseconds * 10.0)));
		}
		TestTrue(TEXT("Memory sample has a cycle delta"), Sample.bThreadCycleTimeValid && Sample.ThreadCycles > 0);
		TestFalse(TEXT("Memory timing sample is internally valid"), Sample.bMeasurementAnomaly);
	}

	const FDistribution EmptyWallDistribution = Summarize(MoveTemp(EmptyWall));
	const FDistribution ComputeWallDistribution = Summarize(MoveTemp(ComputeWall));
	const FDistribution ComputeCpuDistribution = Summarize(MoveTemp(ComputeCpu));
	const FDistribution ComputeCycleDistribution = Summarize(MoveTemp(ComputeCycles));
	const FDistribution MemoryWallDistribution = Summarize(MoveTemp(MemoryWall));
	const FDistribution MemoryCpuDistribution = Summarize(MoveTemp(MemoryCpu));
	AddInfo(FString::Printf(
		TEXT("M2P3_TIMING_CALIBRATION empty_wall_median_us=%.3f empty_wall_p99_us=%.3f compute_wall_median_us=%.3f compute_wall_p99_us=%.3f compute_cpu_median_us=%.3f compute_cycles_median=%.0f compute_nonzero_cpu=%d/101 memory_wall_median_us=%.3f memory_wall_p99_us=%.3f memory_cpu_median_us=%.3f memory_nonzero_cpu=%d/101 minimum_nonzero_thread_time_100ns=%llu per_sample_getthreadtimes_usable=%d"),
		EmptyWallDistribution.Median,
		EmptyWallDistribution.P99,
		ComputeWallDistribution.Median,
		ComputeWallDistribution.P99,
		ComputeCpuDistribution.Median,
		ComputeCycleDistribution.Median,
		ComputeNonZeroCpuCount,
		MemoryWallDistribution.Median,
		MemoryWallDistribution.P99,
		MemoryCpuDistribution.Median,
		MemoryNonZeroCpuCount,
		MinimumNonZeroCpu100ns == MAX_uint64 ? 0 : MinimumNonZeroCpu100ns,
		ComputeNonZeroCpuCount >= 91 && MemoryNonZeroCpuCount >= 91));

	TestTrue(TEXT("Empty-probe median overhead remains below 50 us"), EmptyWallDistribution.Median < 50.0);
	TestTrue(TEXT("Fixed compute control targets approximately 100 us"),
		ComputeWallDistribution.Median >= 40.0 && ComputeWallDistribution.Median <= 400.0);
	TestTrue(TEXT("Fixed memory control targets approximately 100-200 us"),
		MemoryWallDistribution.Median >= 60.0 && MemoryWallDistribution.Median <= 500.0);
#if PLATFORM_WINDOWS
	FDualClockTimer AggregateTimer;
	AggregateTimer.Start();
	uint64 AggregateResult = 0;
	for (int32 Repeat = 0; Repeat < 192; ++Repeat)
	{
		AggregateResult ^= Controls.RunCompute();
	}
	const FTimingSample AggregateSample = AggregateTimer.Stop();
	ConsumeControlResult(AggregateResult);
	AddInfo(FString::Printf(
		TEXT("M2P3_TIMING_AGGREGATE wall_us=%.3f thread_cpu_us=%.3f cycles=%llu"),
		AggregateSample.WallMicroseconds,
		AggregateSample.ThreadCpuMicroseconds,
		AggregateSample.ThreadCycles));
	TestTrue(TEXT("GetThreadTimes advances for an aggregate fixed workload"),
		AggregateSample.bThreadCpuTimeValid && AggregateSample.ThreadCpuMicroseconds > 0.0);
#endif

	FDualClockTimer SleepTimer;
	SleepTimer.Start();
	FPlatformProcess::SleepNoStats(0.020f);
	const FTimingSample SleepSample = SleepTimer.Stop();
	AddInfo(FString::Printf(
		TEXT("M2P3_TIMING_SLEEP wall_us=%.3f thread_cpu_us=%.3f cycles=%llu start_thread=%u end_thread=%u start_processor=%d end_processor=%d migrated=%d"),
		SleepSample.WallMicroseconds,
		SleepSample.ThreadCpuMicroseconds,
		SleepSample.ThreadCycles,
		SleepSample.StartThreadId,
		SleepSample.EndThreadId,
		SleepSample.StartProcessorIndex,
		SleepSample.EndProcessorIndex,
		SleepSample.bThreadMigrated));
	TestTrue(TEXT("Sleep proves wall clock includes descheduled duration"), SleepSample.WallMicroseconds >= 10000.0);
#if PLATFORM_WINDOWS
	TestTrue(TEXT("Thread cycles exclude most sleep duration"),
		SleepSample.bThreadCycleTimeValid
			&& static_cast<double>(SleepSample.ThreadCycles) < ComputeCycleDistribution.Median);
	TestTrue(TEXT("Coarse thread execution time excludes most sleep duration"),
		SleepSample.bThreadCpuTimeValid
			&& SleepSample.ThreadCpuMicroseconds < SleepSample.WallMicroseconds * 0.25);
#endif
	return true;
}

#endif
