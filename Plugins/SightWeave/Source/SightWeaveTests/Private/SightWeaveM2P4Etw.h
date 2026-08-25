#pragma once

#include "CoreMinimal.h"

namespace SightWeave::M2P4::Etw
{
	enum class ESchedulingEventType : uint8
	{
		ContextSwitch,
		ReadyThread
	};

	struct FSchedulingEvent
	{
		uint64 Qpc = 0;
		ESchedulingEventType Type = ESchedulingEventType::ContextSwitch;
		uint32 OldThreadId = 0;
		uint32 NewThreadId = 0;
		uint32 ReadyThreadId = 0;
		uint8 Core = 0;
		uint8 OldThreadState = 0;
		uint8 OldThreadWaitReason = 0;
	};

	struct FSampleMarker
	{
		FString Run;
		FString Operation;
		FString Scope;
		FString Stage;
		int32 Distribution = 0;
		int32 Sample = 0;
		int32 Invocation = 0;
		uint64 SampleId = 0;
		uint32 ProcessId = 0;
		uint32 ThreadId = 0;
		uint64 QpcBegin = 0;
		uint64 QpcEnd = 0;
		uint64 QpcFrequency = 0;
		double WallMicroseconds = 0.0;
		uint64 ThreadCycles = 0;
		int32 StartProcessor = INDEX_NONE;
		int32 EndProcessor = INDEX_NONE;
		bool bMarkerMigrated = false;
		bool bMeasurementAnomaly = false;
	};

	struct FSampleAttribution
	{
		double WallMicroseconds = 0.0;
		double OnCpuMicroseconds = 0.0;
		double ReadyMicroseconds = 0.0;
		double BlockedMicroseconds = 0.0;
		double UnresolvedMicroseconds = 0.0;
		int32 ContextSwitchCount = 0;
		int32 PreemptionCount = 0;
		int32 MigrationCount = 0;
		TMap<int32, double> CoreResidencyMicroseconds;
		bool bBeginRunning = false;
		bool bEndRunning = false;
		bool bTimelineClosed = false;
		bool bEventsLost = false;
		bool bConflictingState = false;
	};

	FSampleAttribution AttributeSample(
		const FSampleMarker& Marker,
		TConstArrayView<FSchedulingEvent> Events,
		bool bEventsLost);

	bool IsMarkerThreadOwnershipValid(
		const FSampleMarker& Marker,
		const TMap<uint32, uint32>& ThreadProcessIds);
}
