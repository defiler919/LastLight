#if WITH_DEV_AUTOMATION_TESTS

#include "SightWeaveM2P3Timing.h"
#include "SightWeaveM2P4Etw.h"

#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
	#include "Windows/AllowWindowsPlatformTypes.h"
	#include <evntrace.h>
	#include <evntcons.h>
	#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace SightWeave::M2P4::EtwTests
{
	using namespace SightWeave::M2P3::Timing;
	using namespace SightWeave::M2P4::Etw;

	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	uint64 ParseUnsigned64(const FString& Value)
	{
		return FCString::Strtoui64(*Value, nullptr, 10);
	}

	bool ParseMarkers(
		const FString& MarkerDirectory,
		TArray<FSampleMarker>& OutMarkers,
		FString& OutError)
	{
		TArray<FString> Paths;
		IFileManager::Get().FindFilesRecursive(
			Paths,
			*MarkerDirectory,
			TEXT("*-etw-markers.csv"),
			true,
			false);
		if (Paths.IsEmpty())
		{
			OutError = FString::Printf(
				TEXT("No *-etw-markers.csv files found under %s"),
				*MarkerDirectory);
			return false;
		}
		Paths.Sort();
		for (const FString& Path : Paths)
		{
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *Path))
			{
				OutError = FString::Printf(TEXT("Cannot read marker CSV %s"), *Path);
				return false;
			}
			TArray<FString> Lines;
			Text.ParseIntoArrayLines(Lines, false);
			for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
			{
				if (Lines[LineIndex].TrimStartAndEnd().IsEmpty())
				{
					continue;
				}
				TArray<FString> Fields;
				Lines[LineIndex].ParseIntoArray(Fields, TEXT(","), false);
				if (Fields.Num() != 19)
				{
					OutError = FString::Printf(
						TEXT("Marker CSV schema mismatch %s:%d fields=%d"),
						*Path,
						LineIndex + 1,
						Fields.Num());
					return false;
				}
				FSampleMarker& Marker = OutMarkers.AddDefaulted_GetRef();
				Marker.Run = Fields[0];
				Marker.Operation = Fields[1];
				Marker.Distribution = FCString::Atoi(*Fields[2]);
				Marker.Sample = FCString::Atoi(*Fields[3]);
				Marker.SampleId = ParseUnsigned64(Fields[4]);
				Marker.Scope = Fields[5];
				Marker.Stage = Fields[6];
				Marker.Invocation = FCString::Atoi(*Fields[7]);
				Marker.ProcessId = static_cast<uint32>(ParseUnsigned64(Fields[8]));
				Marker.ThreadId = static_cast<uint32>(ParseUnsigned64(Fields[9]));
				Marker.QpcBegin = ParseUnsigned64(Fields[10]);
				Marker.QpcEnd = ParseUnsigned64(Fields[11]);
				Marker.QpcFrequency = ParseUnsigned64(Fields[12]);
				Marker.WallMicroseconds = FCString::Atod(*Fields[13]);
				Marker.ThreadCycles = ParseUnsigned64(Fields[14]);
				Marker.StartProcessor = FCString::Atoi(*Fields[15]);
				Marker.EndProcessor = FCString::Atoi(*Fields[16]);
				Marker.bMarkerMigrated = FCString::Atoi(*Fields[17]) != 0;
				Marker.bMeasurementAnomaly = FCString::Atoi(*Fields[18]) != 0;
				if (Marker.ProcessId == 0
					|| Marker.ThreadId == 0
					|| Marker.QpcBegin == 0
					|| Marker.QpcEnd < Marker.QpcBegin
					|| Marker.QpcFrequency == 0)
				{
					OutError = FString::Printf(
						TEXT("Invalid marker identity/clock at %s:%d"),
						*Path,
						LineIndex + 1);
					return false;
				}
			}
		}
		return !OutMarkers.IsEmpty();
	}

	FString CoreResidencyString(const TMap<int32, double>& Residency)
	{
		TArray<int32> Cores;
		Residency.GetKeys(Cores);
		Cores.Sort();
		FString Result;
		for (const int32 Core : Cores)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT("|");
			}
			Result += FString::Printf(TEXT("%d:%.3f"), Core, Residency[Core]);
		}
		return Result;
	}

	FString TimelineString(
		const FSampleMarker& Marker,
		const FSampleAttribution& Attribution)
	{
		const double MicrosecondsPerTick = 1000000.0
			/ static_cast<double>(Marker.QpcFrequency);
		FString Result;
		for (const FAttributedInterval& Interval : Attribution.Intervals)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT("|");
			}
			const double BeginMicroseconds = static_cast<double>(
				Interval.QpcBegin - Marker.QpcBegin) * MicrosecondsPerTick;
			const double EndMicroseconds = static_cast<double>(
				Interval.QpcEnd - Marker.QpcBegin) * MicrosecondsPerTick;
			const TCHAR* State = TEXT("unresolved");
			switch (Interval.State)
			{
			case EAttributedIntervalState::OnCpu: State = TEXT("cpu"); break;
			case EAttributedIntervalState::Ready: State = TEXT("ready"); break;
			case EAttributedIntervalState::Blocked: State = TEXT("blocked"); break;
			default: break;
			}
			if (Interval.State == EAttributedIntervalState::OnCpu)
			{
				Result += FString::Printf(
					TEXT("%s@%d:%.3f-%.3f"),
					State,
					Interval.Core,
					BeginMicroseconds,
					EndMicroseconds);
			}
			else
			{
				Result += FString::Printf(
					TEXT("%s:%.3f-%.3f"),
					State,
					BeginMicroseconds,
					EndMicroseconds);
			}
		}
		return Result;
	}

	const TCHAR* ProvisionalClassification(
		const FSampleMarker& Marker,
		const FSampleAttribution& Attribution)
	{
		if (!Attribution.bTimelineClosed || Marker.bMeasurementAnomaly)
		{
			return TEXT("Unknown");
		}
		if (Marker.Scope != TEXT("total"))
		{
			return TEXT("Evidence");
		}
		double Limit = 0.0;
		if (Marker.Operation == TEXT("batch_512"))
		{
			Limit = 200.0;
		}
		else if (Marker.Operation.StartsWith(TEXT("dynamic_door_")))
		{
			Limit = 250.0;
		}
		else
		{
			return TEXT("Evidence");
		}
		if (Attribution.WallMicroseconds <= Limit)
		{
			return TEXT("Within budget");
		}
		if (Attribution.OnCpuMicroseconds > Limit)
		{
			// A later aggregation step must also identify the internal stage
			// growth before promoting this candidate to the final Plugin CPU label.
			return TEXT("Plugin CPU candidate");
		}
		if (Attribution.ReadyMicroseconds + Attribution.BlockedMicroseconds > 0.0)
		{
			return TEXT("Scheduler/preemption");
		}
		return TEXT("Unknown");
	}

#if PLATFORM_WINDOWS
	class FWindowsEtwReader
	{
	public:
		bool Read(
			const FString& TracePath,
			const TSet<uint32>& TargetThreadIds,
			FString& OutError)
		{
			Targets = TargetThreadIds;
			EVENT_TRACE_LOGFILEW LogFile = {};
			LogFile.LogFileName = const_cast<WCHAR*>(*TracePath);
			LogFile.ProcessTraceMode =
				PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_RAW_TIMESTAMP;
			LogFile.EventRecordCallback = &FWindowsEtwReader::EventRecordCallback;
			Active = this;
			TRACEHANDLE Handle = ::OpenTraceW(&LogFile);
			if (Handle == INVALID_PROCESSTRACE_HANDLE)
			{
				const uint32 Error = ::GetLastError();
				Active = nullptr;
				OutError = FString::Printf(TEXT("OpenTraceW failed: 0x%08x"), Error);
				return false;
			}
			const ULONG Status = ::ProcessTrace(&Handle, 1, nullptr, nullptr);
			::CloseTrace(Handle);
			Active = nullptr;
			EventsLost = LogFile.EventsLost;
			BuffersLost = LogFile.LogfileHeader.BuffersLost;
			QpcFrequency = static_cast<uint64>(LogFile.LogfileHeader.PerfFreq.QuadPart);
			if (Status != ERROR_SUCCESS)
			{
				OutError = FString::Printf(TEXT("ProcessTrace failed: 0x%08x"), Status);
				return false;
			}
			if (MalformedEvents > 0)
			{
				OutError = FString::Printf(TEXT("Malformed kernel scheduling events: %u"), MalformedEvents);
				return false;
			}
			Events.Sort([](const FSchedulingEvent& A, const FSchedulingEvent& B)
			{
				if (A.Qpc != B.Qpc)
				{
					return A.Qpc < B.Qpc;
				}
				return static_cast<uint8>(A.Type) < static_cast<uint8>(B.Type);
			});
			return true;
		}

		TArray<FSchedulingEvent> Events;
		TMap<uint32, uint32> ThreadProcessIds;
		uint64 QpcFrequency = 0;
		uint32 EventsLost = 0;
		uint32 BuffersLost = 0;
		uint32 ContextSwitchEvents = 0;
		uint32 ReadyThreadEvents = 0;
		uint32 MalformedEvents = 0;

	private:
		static void WINAPI EventRecordCallback(PEVENT_RECORD Event)
		{
			if (Active)
			{
				Active->OnEvent(Event);
			}
		}

		void OnEvent(PEVENT_RECORD Event)
		{
			static const GUID ThreadProvider =
				{ 0x3d6fa8d1, 0xfe05, 0x11d0, { 0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c } };
			if (!::IsEqualGUID(Event->EventHeader.ProviderId, ThreadProvider))
			{
				return;
			}
			const uint8 Opcode = Event->EventHeader.EventDescriptor.Opcode;
			const uint8* Data = static_cast<const uint8*>(Event->UserData);
			if ((Opcode == 1 || Opcode == 3) && Event->UserDataLength >= 8)
			{
				uint32 ProcessId = 0;
				uint32 ThreadId = 0;
				FMemory::Memcpy(&ProcessId, Data, sizeof(ProcessId));
				FMemory::Memcpy(&ThreadId, Data + 4, sizeof(ThreadId));
				if (Targets.Contains(ThreadId))
				{
					ThreadProcessIds.Add(ThreadId, ProcessId);
				}
				return;
			}
			if (Opcode == 36)
			{
				if (Event->UserDataLength < 24)
				{
					++MalformedEvents;
					return;
				}
				uint32 NewThreadId = 0;
				uint32 OldThreadId = 0;
				FMemory::Memcpy(&NewThreadId, Data, sizeof(NewThreadId));
				FMemory::Memcpy(&OldThreadId, Data + 4, sizeof(OldThreadId));
				++ContextSwitchEvents;
				if (Targets.Contains(NewThreadId) || Targets.Contains(OldThreadId))
				{
					FSchedulingEvent& Result = Events.AddDefaulted_GetRef();
					Result.Qpc = static_cast<uint64>(Event->EventHeader.TimeStamp.QuadPart);
					Result.Type = ESchedulingEventType::ContextSwitch;
					Result.NewThreadId = NewThreadId;
					Result.OldThreadId = OldThreadId;
					Result.Core = Event->BufferContext.ProcessorNumber;
					Result.OldThreadWaitReason = Data[12];
					Result.OldThreadState = Data[14];
				}
				return;
			}
			if (Opcode == 50)
			{
				if (Event->UserDataLength < 8)
				{
					++MalformedEvents;
					return;
				}
				uint32 ThreadId = 0;
				FMemory::Memcpy(&ThreadId, Data, sizeof(ThreadId));
				++ReadyThreadEvents;
				if (Targets.Contains(ThreadId))
				{
					FSchedulingEvent& Result = Events.AddDefaulted_GetRef();
					Result.Qpc = static_cast<uint64>(Event->EventHeader.TimeStamp.QuadPart);
					Result.Type = ESchedulingEventType::ReadyThread;
					Result.ReadyThreadId = ThreadId;
					Result.Core = Event->BufferContext.ProcessorNumber;
				}
			}
		}

		TSet<uint32> Targets;
		static FWindowsEtwReader* Active;
	};

	FWindowsEtwReader* FWindowsEtwReader::Active = nullptr;
#endif

	class FBusyWorker final : public FRunnable
	{
	public:
		explicit FBusyWorker(TAtomic<bool>& InStop)
			: Stop(InStop)
		{
		}

		virtual uint32 Run() override
		{
			uint64 State = 0x9e3779b97f4a7c15ull;
			while (!Stop.Load())
			{
				for (uint32 Index = 0; Index < 16384; ++Index)
				{
					State ^= State >> 12;
					State ^= State << 25;
					State ^= State >> 27;
				}
				Sink = State;
			}
			return static_cast<uint32>(State);
		}

	private:
		TAtomic<bool>& Stop;
		volatile uint64 Sink = 0;
	};

	void AppendCalibrationMarker(
		FString& Csv,
		const TCHAR* Operation,
		const int32 SampleIndex,
		const FTimingSample& Timing)
	{
		const uint64 SampleId = 0xf000000000000000ull | static_cast<uint64>(SampleIndex);
		Csv += FString::Printf(
			TEXT("calibration,%s,0,%d,%llu,total,total,0,%u,%u,%llu,%llu,%llu,%.3f,%llu,%d,%d,%d,%d\n"),
			Operation,
			SampleIndex,
			SampleId,
			Timing.ProcessId,
			Timing.StartThreadId,
			Timing.QpcBegin,
			Timing.QpcEnd,
			Timing.QpcFrequency,
			Timing.WallMicroseconds,
			Timing.ThreadCycles,
			Timing.StartProcessorIndex,
			Timing.EndProcessorIndex,
			Timing.bThreadMigrated,
			Timing.bMeasurementAnomaly);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P4EtwSyntheticCorrelationTest,
	"SightWeave.M2P4.ETW.SyntheticCorrelation",
	SightWeave::M2P4::EtwTests::TestFlags)

bool FSightWeaveM2P4EtwSyntheticCorrelationTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P4::Etw;
	FSampleMarker Marker;
	Marker.ProcessId = 100;
	Marker.ThreadId = 200;
	Marker.QpcBegin = 1000;
	Marker.QpcEnd = 1100;
	Marker.QpcFrequency = 1000000;
	Marker.StartProcessor = 0;
	Marker.EndProcessor = 0;

	const FSampleAttribution Compute = AttributeSample(Marker, {}, false);
	TestTrue(TEXT("Pure compute closes"), Compute.bTimelineClosed);
	TestEqual(TEXT("Pure compute on-CPU"), Compute.OnCpuMicroseconds, 100.0);
	TestEqual(TEXT("Pure compute off-CPU"), Compute.ReadyMicroseconds + Compute.BlockedMicroseconds, 0.0);
	TestEqual(TEXT("Pure compute interval count"), Compute.Intervals.Num(), 1);
	TestTrue(TEXT("Pure compute interval is on-CPU"),
		Compute.Intervals[0].State == EAttributedIntervalState::OnCpu);

	TArray<FSchedulingEvent> SleepEvents;
	FSchedulingEvent& SleepOut = SleepEvents.AddDefaulted_GetRef();
	SleepOut.Qpc = 1010;
	SleepOut.OldThreadId = 200;
	SleepOut.NewThreadId = 201;
	SleepOut.Core = 0;
	SleepOut.OldThreadState = 5;
	SleepOut.OldThreadWaitReason = 4;
	FSchedulingEvent& SleepReady = SleepEvents.AddDefaulted_GetRef();
	SleepReady.Qpc = 1080;
	SleepReady.Type = ESchedulingEventType::ReadyThread;
	SleepReady.ReadyThreadId = 200;
	FSchedulingEvent& SleepIn = SleepEvents.AddDefaulted_GetRef();
	SleepIn.Qpc = 1090;
	SleepIn.OldThreadId = 201;
	SleepIn.NewThreadId = 200;
	SleepIn.Core = 0;
	const FSampleAttribution Sleep = AttributeSample(Marker, SleepEvents, false);
	TestTrue(TEXT("Sleep timeline closes"), Sleep.bTimelineClosed);
	TestEqual(TEXT("Sleep on-CPU"), Sleep.OnCpuMicroseconds, 20.0);
	TestEqual(TEXT("Sleep blocked"), Sleep.BlockedMicroseconds, 70.0);
	TestEqual(TEXT("Sleep ready"), Sleep.ReadyMicroseconds, 10.0);
	TestEqual(TEXT("Sleep interval count"), Sleep.Intervals.Num(), 4);
	TestTrue(TEXT("Sleep interval sequence"),
		Sleep.Intervals[0].State == EAttributedIntervalState::OnCpu
		&& Sleep.Intervals[1].State == EAttributedIntervalState::Blocked
		&& Sleep.Intervals[2].State == EAttributedIntervalState::Ready
		&& Sleep.Intervals[3].State == EAttributedIntervalState::OnCpu);

	TArray<FSchedulingEvent> YieldEvents;
	FSchedulingEvent& YieldOut = YieldEvents.AddDefaulted_GetRef();
	YieldOut.Qpc = 1010;
	YieldOut.OldThreadId = 200;
	YieldOut.NewThreadId = 201;
	YieldOut.Core = 0;
	YieldOut.OldThreadState = 1;
	YieldOut.OldThreadWaitReason = 33;
	FSchedulingEvent& YieldIn = YieldEvents.AddDefaulted_GetRef();
	YieldIn.Qpc = 1070;
	YieldIn.OldThreadId = 201;
	YieldIn.NewThreadId = 200;
	YieldIn.Core = 2;
	const FSampleAttribution Yield = AttributeSample(Marker, YieldEvents, false);
	TestTrue(TEXT("Yield timeline closes"), Yield.bTimelineClosed);
	TestEqual(TEXT("Yield on-CPU"), Yield.OnCpuMicroseconds, 40.0);
	TestEqual(TEXT("Yield ready"), Yield.ReadyMicroseconds, 60.0);
	TestEqual(TEXT("Yield migration"), Yield.MigrationCount, 1);
	TestEqual(TEXT("Yield preemption interval"), Yield.PreemptionCount, 1);

	const FSampleAttribution Lost = AttributeSample(Marker, YieldEvents, true);
	TestFalse(TEXT("Lost events fail closed"), Lost.bTimelineClosed);
	TestEqual(TEXT("Lost interval becomes unresolved"), Lost.UnresolvedMicroseconds, 100.0);

	TMap<uint32, uint32> Ownership;
	Ownership.Add(200, 100);
	TestTrue(TEXT("PID/TID ownership matches"), IsMarkerThreadOwnershipValid(Marker, Ownership));
	Ownership[200] = 101;
	TestFalse(TEXT("Same TID in another process is rejected"), IsMarkerThreadOwnershipValid(Marker, Ownership));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P4EtwCalibrationCaptureTest,
	"SightWeave.M2P4.ETW.Calibration.Capture",
	SightWeave::M2P4::EtwTests::TestFlags)

bool FSightWeaveM2P4EtwCalibrationCaptureTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P3::Timing;
	using namespace SightWeave::M2P4::EtwTests;
	if (!FParse::Param(FCommandLine::Get(), TEXT("SightWeaveM2P4EtwCalibrationCapture")))
	{
		AddInfo(TEXT("M2P.4 ETW calibration capture requires an explicit command-line flag."));
		return true;
	}
	FString OutputDirectory;
	if (!FParse::Value(FCommandLine::Get(), TEXT("SightWeaveM2P4Output="), OutputDirectory))
	{
		AddError(TEXT("Pass -SightWeaveM2P4Output=<directory>."));
		return false;
	}
	OutputDirectory = FPaths::ConvertRelativePathToFull(OutputDirectory);
	IFileManager::Get().MakeDirectory(*OutputDirectory, true);

	const FPlatformCapabilities Capabilities = GetPlatformCapabilities();
	AddInfo(FString::Printf(
		TEXT("M2P4_ETW_CAPABILITIES elevated=%d expected=%d cycles=%d qpc_frequency=%llu"),
		Capabilities.bRunningElevated,
		Capabilities.bContextSwitchEtwExpectedAvailable,
		Capabilities.bThreadCycleTimeAvailable,
		static_cast<uint64>(FMath::RoundToDouble(1.0 / FPlatformTime::GetSecondsPerCycle64()))));

	FFixedWorkControls Controls;
	for (int32 Warmup = 0; Warmup < 8; ++Warmup)
	{
		ConsumeControlResult(Controls.RunCompute());
		ConsumeControlResult(Controls.RunMemory());
	}
	FString Csv(TEXT("run,operation,distribution,sample,sample_id,scope,stage,invocation,pid,tid,qpc_begin,qpc_end,qpc_frequency,wall_us,thread_cycles,start_processor,end_processor,migrated,measurement_anomaly\n"));
	int32 SampleIndex = 0;
	volatile uint64 MarkerProbeSink = 0;
	auto Measure = [&](const TCHAR* Name, TFunctionRef<void()> Work)
	{
		FDualClockTimer Timer;
		Timer.Start();
		Work();
		AppendCalibrationMarker(Csv, Name, SampleIndex++, Timer.Stop());
	};
	for (int32 Repeat = 0; Repeat < 31; ++Repeat)
	{
		Measure(TEXT("empty"), [] {});
		Measure(TEXT("fixed_compute"), [&]
		{
			ConsumeControlResult(Controls.RunCompute());
		});
		Measure(TEXT("fixed_memory"), [&]
		{
			ConsumeControlResult(Controls.RunMemory());
		});
		Measure(TEXT("stage_marker_probe"), [&]
		{
			FDualClockTimer StageTimer;
			StageTimer.Start();
			const FTimingSample StageSample = StageTimer.Stop();
			MarkerProbeSink = MarkerProbeSink
				^ StageSample.QpcBegin
				^ StageSample.QpcEnd
				^ StageSample.ThreadCycles;
		});
	}
	for (int32 Repeat = 0; Repeat < 11; ++Repeat)
	{
		Measure(TEXT("sleep_10ms"), [] { FPlatformProcess::Sleep(0.010f); });
		Measure(TEXT("yield_256"), []
		{
			for (int32 Yield = 0; Yield < 256; ++Yield)
			{
				FPlatformProcess::YieldThread();
			}
		});
	}

	TAtomic<bool> StopWorkers(false);
	const int32 WorkerCount = FMath::Clamp(
		FPlatformMisc::NumberOfCoresIncludingHyperthreads() + 2,
		4,
		32);
	TArray<TUniquePtr<FBusyWorker>> Workers;
	TArray<FRunnableThread*> Threads;
	for (int32 WorkerIndex = 0; WorkerIndex < WorkerCount; ++WorkerIndex)
	{
		Workers.Add(MakeUnique<FBusyWorker>(StopWorkers));
		Threads.Add(FRunnableThread::Create(
			Workers.Last().Get(),
			*FString::Printf(TEXT("SightWeaveM2P4Busy%d"), WorkerIndex)));
	}
	FPlatformProcess::Sleep(0.025f);
	for (int32 Repeat = 0; Repeat < 31; ++Repeat)
	{
		Measure(TEXT("fixed_compute_under_load"), [&]
		{
			ConsumeControlResult(Controls.RunCompute());
		});
	}
	for (int32 Repeat = 0; Repeat < 11; ++Repeat)
	{
		Measure(TEXT("yield_256_under_load"), []
		{
			for (int32 Yield = 0; Yield < 256; ++Yield)
			{
				FPlatformProcess::YieldThread();
			}
		});
	}
	StopWorkers.Store(true);
	for (FRunnableThread* Thread : Threads)
	{
		if (Thread)
		{
			Thread->WaitForCompletion();
			delete Thread;
		}
	}

	const FString Path = FPaths::Combine(OutputDirectory, TEXT("calibration-etw-markers.csv"));
	const bool bSaved = FFileHelper::SaveStringToFile(
		Csv,
		*Path,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	TestTrue(TEXT("Calibration marker CSV saves"), bSaved);
	AddInfo(FString::Printf(
		TEXT("M2P4_ETW_CALIBRATION_MARKERS path=%s rows=%d workers=%d"),
		*Path,
		SampleIndex,
		WorkerCount));
	return bSaved;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P4EtwAnalyzeTest,
	"SightWeave.M2P4.ETW.Analyze",
	SightWeave::M2P4::EtwTests::TestFlags)

bool FSightWeaveM2P4EtwAnalyzeTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P4::Etw;
	using namespace SightWeave::M2P4::EtwTests;
	if (!FParse::Param(FCommandLine::Get(), TEXT("SightWeaveM2P4EtwAnalyze")))
	{
		AddInfo(TEXT("M2P.4 ETW analysis requires explicit trace/marker/report arguments."));
		return true;
	}
	FString TracePath;
	FString MarkerDirectory;
	FString ReportPath;
	if (!FParse::Value(FCommandLine::Get(), TEXT("SightWeaveM2P4EtwTrace="), TracePath)
		|| !FParse::Value(FCommandLine::Get(), TEXT("SightWeaveM2P4EtwMarkerDirectory="), MarkerDirectory)
		|| !FParse::Value(FCommandLine::Get(), TEXT("SightWeaveM2P4EtwReport="), ReportPath))
	{
		AddError(TEXT("Pass ETW trace, marker directory, and report paths."));
		return false;
	}
	TracePath = FPaths::ConvertRelativePathToFull(TracePath);
	MarkerDirectory = FPaths::ConvertRelativePathToFull(MarkerDirectory);
	ReportPath = FPaths::ConvertRelativePathToFull(ReportPath);
	if (FPaths::FileExists(ReportPath))
	{
		AddError(FString::Printf(TEXT("Refusing to overwrite ETW report %s"), *ReportPath));
		return false;
	}
	TArray<FSampleMarker> Markers;
	FString Error;
	if (!TestTrue(TEXT("ETW markers parse"), ParseMarkers(MarkerDirectory, Markers, Error)))
	{
		AddError(Error);
		return false;
	}

#if !PLATFORM_WINDOWS
	AddError(TEXT("M2P.4 ETW analysis is Windows-only and platform guarded."));
	return false;
#else
	TSet<uint32> TargetThreadIds;
	for (const FSampleMarker& Marker : Markers)
	{
		TargetThreadIds.Add(Marker.ThreadId);
	}
	FWindowsEtwReader Reader;
	if (!TestTrue(TEXT("Kernel ETL parses"), Reader.Read(TracePath, TargetThreadIds, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("No ETW events lost"), Reader.EventsLost, uint32(0));
	TestEqual(TEXT("No ETW buffers lost"), Reader.BuffersLost, uint32(0));
	TestTrue(TEXT("CSwitch events are present"), Reader.ContextSwitchEvents > 0);
	TestTrue(TEXT("ReadyThread events are present"), Reader.ReadyThreadEvents > 0);

	FString Csv(TEXT("run,operation,distribution,sample,sample_id,scope,stage,invocation,pid,tid,qpc_begin,qpc_end,wall_us,marker_wall_us,thread_cycles,start_processor,end_processor,marker_migrated,measurement_anomaly,on_cpu_us,ready_us,blocked_us,unresolved_us,context_switches,preemptions,migrations,core_residency,timeline,events_lost,buffers_lost,timeline_closed,classification\n"));
	int32 UnknownCount = 0;
	for (const FSampleMarker& Marker : Markers)
	{
		const bool bOwnershipValid = IsMarkerThreadOwnershipValid(
			Marker,
			Reader.ThreadProcessIds);
		const bool bClockMatches = Reader.QpcFrequency == Marker.QpcFrequency;
		FSampleAttribution Attribution = AttributeSample(
			Marker,
			Reader.Events,
			Reader.EventsLost > 0 || Reader.BuffersLost > 0);
		if (!bOwnershipValid || !bClockMatches)
		{
			Attribution.bTimelineClosed = false;
			Attribution.UnresolvedMicroseconds = Attribution.WallMicroseconds;
		}
		const TCHAR* Classification = ProvisionalClassification(Marker, Attribution);
		UnknownCount += FCString::Strcmp(Classification, TEXT("Unknown")) == 0 ? 1 : 0;
		Csv += FString::Printf(
			TEXT("%s,%s,%d,%d,%llu,%s,%s,%d,%u,%u,%llu,%llu,%.3f,%.3f,%llu,%d,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%d,%d,%d,%s,%s,%u,%u,%d,%s\n"),
			*Marker.Run,
			*Marker.Operation,
			Marker.Distribution,
			Marker.Sample,
			Marker.SampleId,
			*Marker.Scope,
			*Marker.Stage,
			Marker.Invocation,
			Marker.ProcessId,
			Marker.ThreadId,
			Marker.QpcBegin,
			Marker.QpcEnd,
			Attribution.WallMicroseconds,
			Marker.WallMicroseconds,
			Marker.ThreadCycles,
			Marker.StartProcessor,
			Marker.EndProcessor,
			Marker.bMarkerMigrated,
			Marker.bMeasurementAnomaly,
			Attribution.OnCpuMicroseconds,
			Attribution.ReadyMicroseconds,
			Attribution.BlockedMicroseconds,
			Attribution.UnresolvedMicroseconds,
			Attribution.ContextSwitchCount,
			Attribution.PreemptionCount,
			Attribution.MigrationCount,
			*CoreResidencyString(Attribution.CoreResidencyMicroseconds),
			*TimelineString(Marker, Attribution),
			Reader.EventsLost,
			Reader.BuffersLost,
			Attribution.bTimelineClosed,
			Classification);
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	const bool bSaved = FFileHelper::SaveStringToFile(
		Csv,
		*ReportPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	TestTrue(TEXT("ETW attribution report saves"), bSaved);
	TestEqual(TEXT("All ETW marker timelines close"), UnknownCount, 0);
	AddInfo(FString::Printf(
		TEXT("M2P4_ETW_ANALYSIS markers=%d scheduling_events=%d cswitch=%u ready=%u lost=%u/%u unknown=%d report=%s"),
		Markers.Num(),
		Reader.Events.Num(),
		Reader.ContextSwitchEvents,
		Reader.ReadyThreadEvents,
		Reader.EventsLost,
		Reader.BuffersLost,
		UnknownCount,
		*ReportPath));
	return bSaved && UnknownCount == 0;
#endif
}

#endif
