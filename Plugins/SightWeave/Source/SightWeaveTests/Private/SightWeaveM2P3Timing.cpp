#include "SightWeaveM2P3Timing.h"

#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformTLS.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
	#include <psapi.h>
	#include <powrprof.h>
#include "Windows/HideWindowsPlatformTypes.h"

#ifndef PROCESSOR_POWER_INFORMATION
typedef struct _PROCESSOR_POWER_INFORMATION
{
	ULONG Number;
	ULONG MaxMhz;
	ULONG CurrentMhz;
	ULONG MhzLimit;
	ULONG MaxIdleState;
	ULONG CurrentIdleState;
} PROCESSOR_POWER_INFORMATION, *PPROCESSOR_POWER_INFORMATION;
#endif
#endif

namespace SightWeave::M2P3::Timing
{
	namespace
	{
		volatile uint64 GControlResultSink = 0;

		uint64 RotateLeft(const uint64 Value, const uint32 Count)
		{
			return (Value << Count) | (Value >> (64u - Count));
		}

#if PLATFORM_WINDOWS
		uint64 FileTimeTo100ns(const FILETIME& Time)
		{
			return (static_cast<uint64>(Time.dwHighDateTime) << 32)
				| static_cast<uint64>(Time.dwLowDateTime);
		}

		void QueryThreadCounters(
			uint64& OutCycles,
			uint64& OutKernel100ns,
			uint64& OutUser100ns,
			bool& bOutCyclesValid,
			bool& bOutTimesValid)
		{
			FILETIME CreationTime = {};
			FILETIME ExitTime = {};
			FILETIME KernelTime = {};
			FILETIME UserTime = {};
			ULONG64 Cycles = 0;
			const HANDLE ThreadHandle = ::GetCurrentThread();
			bOutTimesValid = ::GetThreadTimes(
				ThreadHandle,
				&CreationTime,
				&ExitTime,
				&KernelTime,
				&UserTime) != 0;
			bOutCyclesValid = ::QueryThreadCycleTime(ThreadHandle, &Cycles) != 0;
			OutCycles = static_cast<uint64>(Cycles);
			OutKernel100ns = FileTimeTo100ns(KernelTime);
			OutUser100ns = FileTimeTo100ns(UserTime);
		}

		bool QueryProcessorIndex(int32& OutProcessorIndex)
		{
			PROCESSOR_NUMBER Processor = {};
			::GetCurrentProcessorNumberEx(&Processor);
			const WORD GroupCount = ::GetActiveProcessorGroupCount();
			if (Processor.Group >= GroupCount)
			{
				return false;
			}
			uint32 LinearIndex = 0;
			for (WORD Group = 0; Group < Processor.Group; ++Group)
			{
				const DWORD ActiveCount = ::GetActiveProcessorCount(Group);
				if (ActiveCount == 0)
				{
					return false;
				}
				LinearIndex += ActiveCount;
			}
			LinearIndex += Processor.Number;
			OutProcessorIndex = static_cast<int32>(LinearIndex);
			return true;
		}

		bool QueryPageFaultCount(uint32& OutPageFaultCount)
		{
			PROCESS_MEMORY_COUNTERS Counters = {};
			Counters.cb = sizeof(Counters);
			if (!::GetProcessMemoryInfo(::GetCurrentProcess(), &Counters, sizeof(Counters)))
			{
				return false;
			}
			OutPageFaultCount = Counters.PageFaultCount;
			return true;
		}

		bool QueryProcessorFrequency(
			const int32 ProcessorIndex,
			uint32& OutCurrentMhz,
			uint32& OutMaximumMhz)
		{
			constexpr uint32 MaximumTrackedProcessors = 256;
			const DWORD ProcessorCount = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
			if (ProcessorIndex < 0
				|| ProcessorCount == 0
				|| ProcessorCount > MaximumTrackedProcessors
				|| static_cast<DWORD>(ProcessorIndex) >= ProcessorCount)
			{
				return false;
			}
			PROCESSOR_POWER_INFORMATION Information[MaximumTrackedProcessors] = {};
			const ULONG Status = ::CallNtPowerInformation(
				ProcessorInformation,
				nullptr,
				0,
				Information,
				ProcessorCount * sizeof(PROCESSOR_POWER_INFORMATION));
			if (Status != ERROR_SUCCESS)
			{
				return false;
			}
			const PROCESSOR_POWER_INFORMATION& Processor = Information[ProcessorIndex];
			OutCurrentMhz = Processor.CurrentMhz;
			OutMaximumMhz = Processor.MaxMhz;
			return Processor.MaxMhz > 0;
		}

		bool IsRunningElevated()
		{
			HANDLE Token = nullptr;
			if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &Token))
			{
				return false;
			}
			TOKEN_ELEVATION Elevation = {};
			DWORD ReturnLength = 0;
			const bool bSucceeded = ::GetTokenInformation(
				Token,
				TokenElevation,
				&Elevation,
				sizeof(Elevation),
				&ReturnLength) != 0;
			::CloseHandle(Token);
			return bSucceeded && Elevation.TokenIsElevated != 0;
		}
#endif

		void CapturePlatformSnapshot(
			FDualClockTimer::FPlatformSnapshot& OutSnapshot,
			const bool bCaptureWallFirst,
			const bool bCaptureAuxiliaryEvidence)
		{
			if (bCaptureWallFirst)
			{
				OutSnapshot.WallCycles = FPlatformTime::Cycles64();
#if PLATFORM_WINDOWS
				QueryThreadCounters(
					OutSnapshot.ThreadCycles,
					OutSnapshot.KernelTime100ns,
					OutSnapshot.UserTime100ns,
					OutSnapshot.bThreadCyclesValid,
					OutSnapshot.bThreadTimesValid);
#endif
			}

			OutSnapshot.ThreadId = FPlatformTLS::GetCurrentThreadId();
#if PLATFORM_WINDOWS
			OutSnapshot.bProcessorValid = QueryProcessorIndex(OutSnapshot.ProcessorIndex);
			OutSnapshot.bPageFaultsValid = bCaptureAuxiliaryEvidence
				&& QueryPageFaultCount(OutSnapshot.ProcessPageFaultCount);
			OutSnapshot.bFrequencyValid = bCaptureAuxiliaryEvidence
				&& OutSnapshot.bProcessorValid
				&& QueryProcessorFrequency(
					OutSnapshot.ProcessorIndex,
					OutSnapshot.CurrentMhz,
					OutSnapshot.MaximumMhz);
#else
			OutSnapshot.ProcessorIndex = static_cast<int32>(FPlatformProcess::GetCurrentCoreNumber());
			OutSnapshot.bProcessorValid = OutSnapshot.ProcessorIndex >= 0;
#endif
			if (!bCaptureWallFirst)
			{
				QueryThreadCounters(
					OutSnapshot.ThreadCycles,
					OutSnapshot.KernelTime100ns,
					OutSnapshot.UserTime100ns,
					OutSnapshot.bThreadCyclesValid,
					OutSnapshot.bThreadTimesValid);
				OutSnapshot.WallCycles = FPlatformTime::Cycles64();
			}
		}
	}

	void FDualClockTimer::Start()
	{
		StartSnapshot = {};
		CapturePlatformSnapshot(StartSnapshot, false, bCaptureAuxiliaryEvidence);
		bStarted = true;
	}

	FTimingSample FDualClockTimer::Stop()
	{
		FTimingSample Result;
		FPlatformSnapshot EndSnapshot;
		CapturePlatformSnapshot(EndSnapshot, true, bCaptureAuxiliaryEvidence);
		if (!bStarted)
		{
			Result.bMeasurementAnomaly = true;
			return Result;
		}
		bStarted = false;

		Result.StartThreadId = StartSnapshot.ThreadId;
		Result.EndThreadId = EndSnapshot.ThreadId;
		Result.QpcBegin = StartSnapshot.WallCycles;
		Result.QpcEnd = EndSnapshot.WallCycles;
		Result.QpcFrequency = static_cast<uint64>(FMath::RoundToDouble(
			1.0 / FPlatformTime::GetSecondsPerCycle64()));
		Result.ProcessId = FPlatformProcess::GetCurrentProcessId();
		Result.StartProcessorIndex = StartSnapshot.ProcessorIndex;
		Result.EndProcessorIndex = EndSnapshot.ProcessorIndex;
		Result.StartCurrentMhz = StartSnapshot.CurrentMhz;
		Result.EndCurrentMhz = EndSnapshot.CurrentMhz;
		Result.StartMaximumMhz = StartSnapshot.MaximumMhz;
		Result.EndMaximumMhz = EndSnapshot.MaximumMhz;
		Result.bProcessorNumberValid = StartSnapshot.bProcessorValid && EndSnapshot.bProcessorValid;
		Result.bThreadMigrated = Result.bProcessorNumberValid
			&& StartSnapshot.ProcessorIndex != EndSnapshot.ProcessorIndex;
		Result.bProcessorFrequencyValid = StartSnapshot.bFrequencyValid && EndSnapshot.bFrequencyValid;
		Result.bFrequencyChanged = Result.bProcessorFrequencyValid
			&& (StartSnapshot.CurrentMhz != EndSnapshot.CurrentMhz
				|| StartSnapshot.MaximumMhz != EndSnapshot.MaximumMhz);

		const bool bSameThread = StartSnapshot.ThreadId == EndSnapshot.ThreadId;
		const bool bWallMonotonic = EndSnapshot.WallCycles >= StartSnapshot.WallCycles;
		Result.WallMicroseconds = bWallMonotonic
			? FPlatformTime::ToSeconds64(EndSnapshot.WallCycles - StartSnapshot.WallCycles) * 1000000.0
			: 0.0;

		Result.bThreadCycleTimeValid = bSameThread
			&& StartSnapshot.bThreadCyclesValid
			&& EndSnapshot.bThreadCyclesValid
			&& EndSnapshot.ThreadCycles >= StartSnapshot.ThreadCycles;
		if (Result.bThreadCycleTimeValid)
		{
			Result.ThreadCycles = EndSnapshot.ThreadCycles - StartSnapshot.ThreadCycles;
		}

		Result.bThreadCpuTimeValid = bSameThread
			&& StartSnapshot.bThreadTimesValid
			&& EndSnapshot.bThreadTimesValid
			&& EndSnapshot.KernelTime100ns >= StartSnapshot.KernelTime100ns
			&& EndSnapshot.UserTime100ns >= StartSnapshot.UserTime100ns;
		if (Result.bThreadCpuTimeValid)
		{
			const uint64 KernelDelta = EndSnapshot.KernelTime100ns - StartSnapshot.KernelTime100ns;
			const uint64 UserDelta = EndSnapshot.UserTime100ns - StartSnapshot.UserTime100ns;
			Result.KernelMicroseconds = static_cast<double>(KernelDelta) * 0.1;
			Result.UserMicroseconds = static_cast<double>(UserDelta) * 0.1;
			Result.ThreadCpuMicroseconds = Result.KernelMicroseconds + Result.UserMicroseconds;
		}

		Result.bProcessPageFaultCountValid = StartSnapshot.bPageFaultsValid
			&& EndSnapshot.bPageFaultsValid
			&& EndSnapshot.ProcessPageFaultCount >= StartSnapshot.ProcessPageFaultCount;
		if (Result.bProcessPageFaultCountValid)
		{
			Result.ProcessPageFaultDelta =
				EndSnapshot.ProcessPageFaultCount - StartSnapshot.ProcessPageFaultCount;
		}

		Result.bMeasurementAnomaly = !bSameThread
			|| !bWallMonotonic
			|| (StartSnapshot.bThreadCyclesValid != EndSnapshot.bThreadCyclesValid)
			|| (StartSnapshot.bThreadTimesValid != EndSnapshot.bThreadTimesValid);
		return Result;
	}

	FFixedWorkControls::FFixedWorkControls()
	{
		uint64 State = 0xD1B54A32D192ED03ull;
		for (int32 Index = 0; Index < MemoryWords.Num(); ++Index)
		{
			State ^= State >> 12;
			State ^= State << 25;
			State ^= State >> 27;
			MemoryWords[Index] = State * 0x2545F4914F6CDD1Dull + static_cast<uint64>(Index);
		}
	}

	uint64 FFixedWorkControls::RunCompute() const
	{
		// The volatile seed changes after every consumed control result. Work count
		// remains fixed, but unity/LTCG cannot fold the deterministic loop to a
		// constant return value in a larger attribution translation unit.
		uint64 State = GControlResultSink ^ 0x9E3779B97F4A7C15ull;
		for (uint32 Index = 0; Index < 49152; ++Index)
		{
			State ^= State >> 12;
			State ^= State << 25;
			State ^= State >> 27;
			State = RotateLeft(State * 0x2545F4914F6CDD1Dull + Index, 17);
		}
		return State;
	}

	uint64 FFixedWorkControls::RunMemory() const
	{
		uint64 Result = 0xA24BAED4963EE407ull;
		for (int32 Pass = 0; Pass < 32; ++Pass)
		{
			for (int32 Index = 0; Index < MemoryWords.Num(); ++Index)
			{
				Result = RotateLeft(Result ^ MemoryWords[Index], 9) + static_cast<uint64>(Pass + Index);
			}
		}
		return Result;
	}

	FPlatformCapabilities GetPlatformCapabilities()
	{
		FPlatformCapabilities Result;
		Result.bContextSwitchTraceRequested = FParse::Param(FCommandLine::Get(), TEXT("trace"))
			|| FCString::Strifind(FCommandLine::Get(), TEXT("contextswitch")) != nullptr;
#if PLATFORM_WINDOWS
		uint64 Cycles = 0;
		uint64 Kernel = 0;
		uint64 User = 0;
		QueryThreadCounters(
			Cycles,
			Kernel,
			User,
			Result.bThreadCycleTimeAvailable,
			Result.bThreadKernelUserTimeAvailable);
		int32 ProcessorIndex = INDEX_NONE;
		Result.bProcessorNumberAvailable = QueryProcessorIndex(ProcessorIndex);
		uint32 PageFaults = 0;
		Result.bProcessPageFaultCountAvailable = QueryPageFaultCount(PageFaults);
		uint32 CurrentMhz = 0;
		uint32 MaximumMhz = 0;
		Result.bProcessorFrequencyAvailable = Result.bProcessorNumberAvailable
			&& QueryProcessorFrequency(ProcessorIndex, CurrentMhz, MaximumMhz);
		Result.bRunningElevated = IsRunningElevated();
		Result.bContextSwitchEtwExpectedAvailable = Result.bRunningElevated;
#endif
		return Result;
	}

	void ConsumeControlResult(const uint64 Result)
	{
		GControlResultSink = GControlResultSink ^ Result;
	}
}
