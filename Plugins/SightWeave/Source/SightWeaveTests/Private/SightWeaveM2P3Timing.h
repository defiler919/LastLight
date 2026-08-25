#pragma once

#include "Containers/StaticArray.h"
#include "CoreMinimal.h"

namespace SightWeave::M2P3::Timing
{
	struct FPlatformCapabilities
	{
		bool bThreadCycleTimeAvailable = false;
		bool bThreadKernelUserTimeAvailable = false;
		bool bProcessorNumberAvailable = false;
		bool bProcessPageFaultCountAvailable = false;
		bool bProcessorFrequencyAvailable = false;
		bool bContextSwitchTraceRequested = false;
		bool bContextSwitchEtwExpectedAvailable = false;
		bool bRunningElevated = false;
	};

	struct FTimingSample
	{
		uint64 QpcBegin = 0;
		uint64 QpcEnd = 0;
		uint64 QpcFrequency = 0;
		uint32 ProcessId = 0;
		double WallMicroseconds = 0.0;
		double ThreadCpuMicroseconds = 0.0;
		double KernelMicroseconds = 0.0;
		double UserMicroseconds = 0.0;
		uint64 ThreadCycles = 0;
		uint32 StartThreadId = 0;
		uint32 EndThreadId = 0;
		int32 StartProcessorIndex = INDEX_NONE;
		int32 EndProcessorIndex = INDEX_NONE;
		uint32 StartCurrentMhz = 0;
		uint32 EndCurrentMhz = 0;
		uint32 StartMaximumMhz = 0;
		uint32 EndMaximumMhz = 0;
		uint32 ProcessPageFaultDelta = 0;
		bool bThreadCycleTimeValid = false;
		bool bThreadCpuTimeValid = false;
		bool bProcessorNumberValid = false;
		bool bProcessPageFaultCountValid = false;
		bool bProcessorFrequencyValid = false;
		bool bThreadMigrated = false;
		bool bFrequencyChanged = false;
		bool bMeasurementAnomaly = false;
	};

	class FDualClockTimer
	{
	public:
		explicit FDualClockTimer(bool bInCaptureAuxiliaryEvidence = true)
			: bCaptureAuxiliaryEvidence(bInCaptureAuxiliaryEvidence)
		{
		}

		void Start();
		FTimingSample Stop();
		void SetCaptureAuxiliaryEvidence(bool bEnabled)
		{
			bCaptureAuxiliaryEvidence = bEnabled;
		}

		struct FPlatformSnapshot
		{
			uint64 WallCycles = 0;
			uint64 ThreadCycles = 0;
			uint64 KernelTime100ns = 0;
			uint64 UserTime100ns = 0;
			uint32 ThreadId = 0;
			int32 ProcessorIndex = INDEX_NONE;
			uint32 ProcessPageFaultCount = 0;
			uint32 CurrentMhz = 0;
			uint32 MaximumMhz = 0;
			bool bThreadCyclesValid = false;
			bool bThreadTimesValid = false;
			bool bProcessorValid = false;
			bool bPageFaultsValid = false;
			bool bFrequencyValid = false;
		};

	private:
		FPlatformSnapshot StartSnapshot;
		bool bCaptureAuxiliaryEvidence = true;
		bool bStarted = false;
	};

	/** Fixed-work, allocation-free controls used only for attribution. */
	class FFixedWorkControls
	{
	public:
		FFixedWorkControls();

		uint64 RunCompute() const;
		uint64 RunMemory() const;

	private:
		alignas(64) TStaticArray<uint64, 8192> MemoryWords;
	};

	FPlatformCapabilities GetPlatformCapabilities();
	void ConsumeControlResult(uint64 Result);
}
