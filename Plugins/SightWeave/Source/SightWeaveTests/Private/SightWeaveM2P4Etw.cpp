#include "SightWeaveM2P4Etw.h"

namespace SightWeave::M2P4::Etw
{
	namespace
	{
		enum class EThreadState : uint8
		{
			Running,
			Ready,
			Blocked,
			Unknown
		};

		bool IsReadyState(const uint8 State, const uint8 WaitReason)
		{
			// Microsoft CSwitch state values: Ready=1, Standby=3,
			// DeferredReady=7. Quantum end, preemption, and yield are runnable
			// reasons even if a provider reports an ambiguous state byte.
			return State == 1 || State == 3 || State == 7
				|| WaitReason == 30 || WaitReason == 32 || WaitReason == 33;
		}

		void AddTicks(
			FSampleAttribution& Result,
			const EThreadState State,
			const int32 Core,
			const uint64 Ticks,
			const double MicrosecondsPerTick)
		{
			const double Microseconds = static_cast<double>(Ticks) * MicrosecondsPerTick;
			switch (State)
			{
			case EThreadState::Running:
				Result.OnCpuMicroseconds += Microseconds;
				Result.CoreResidencyMicroseconds.FindOrAdd(Core) += Microseconds;
				break;
			case EThreadState::Ready:
				Result.ReadyMicroseconds += Microseconds;
				break;
			case EThreadState::Blocked:
				Result.BlockedMicroseconds += Microseconds;
				break;
			default:
				Result.UnresolvedMicroseconds += Microseconds;
				break;
			}
		}
	}

	FSampleAttribution AttributeSample(
		const FSampleMarker& Marker,
		TConstArrayView<FSchedulingEvent> Events,
		const bool bEventsLost)
	{
		FSampleAttribution Result;
		Result.bEventsLost = bEventsLost;
		if (Marker.QpcFrequency == 0 || Marker.QpcEnd < Marker.QpcBegin)
		{
			Result.bConflictingState = true;
			return Result;
		}

		const double MicrosecondsPerTick = 1000000.0 / static_cast<double>(Marker.QpcFrequency);
		const uint64 WallTicks = Marker.QpcEnd - Marker.QpcBegin;
		Result.WallMicroseconds = static_cast<double>(WallTicks) * MicrosecondsPerTick;
		Result.bBeginRunning = true; // The marker is emitted by the measured thread.

		int32 FirstCandidate = 0;
		int32 CandidateEnd = Events.Num();
		while (FirstCandidate < CandidateEnd)
		{
			const int32 Middle = FirstCandidate + (CandidateEnd - FirstCandidate) / 2;
			if (Events[Middle].Qpc < Marker.QpcBegin)
			{
				FirstCandidate = Middle + 1;
			}
			else
			{
				CandidateEnd = Middle;
			}
		}

		TArray<FSchedulingEvent, TInlineAllocator<32>> RelevantEvents;
		for (int32 EventIndex = FirstCandidate; EventIndex < Events.Num(); ++EventIndex)
		{
			const FSchedulingEvent& Event = Events[EventIndex];
			if (Event.Qpc > Marker.QpcEnd)
			{
				break;
			}
			const bool bRelevantThread = Event.Type == ESchedulingEventType::ReadyThread
				? Event.ReadyThreadId == Marker.ThreadId
				: Event.OldThreadId == Marker.ThreadId || Event.NewThreadId == Marker.ThreadId;
			if (bRelevantThread)
			{
				RelevantEvents.Add(Event);
			}
		}
		RelevantEvents.Sort([](const FSchedulingEvent& A, const FSchedulingEvent& B)
		{
			if (A.Qpc != B.Qpc)
			{
				return A.Qpc < B.Qpc;
			}
			// A switch-out establishes the non-running state before a same-QPC
			// ReadyThread notification; switch-in is handled by the same record.
			return static_cast<uint8>(A.Type) < static_cast<uint8>(B.Type);
		});

		EThreadState State = EThreadState::Running;
		int32 CurrentCore = Marker.StartProcessor;
		int32 LastRunningCore = Marker.StartProcessor;
		uint64 Cursor = Marker.QpcBegin;
		for (const FSchedulingEvent& Event : RelevantEvents)
		{
			if (Event.Qpc < Cursor)
			{
				Result.bConflictingState = true;
				continue;
			}
			AddTicks(Result, State, CurrentCore, Event.Qpc - Cursor, MicrosecondsPerTick);
			Cursor = Event.Qpc;

			if (Event.Type == ESchedulingEventType::ReadyThread)
			{
				if (State == EThreadState::Running)
				{
					Result.bConflictingState = true;
				}
				else
				{
					State = EThreadState::Ready;
				}
				continue;
			}

			const bool bSwitchingOut = Event.OldThreadId == Marker.ThreadId
				&& Event.NewThreadId != Marker.ThreadId;
			const bool bSwitchingIn = Event.NewThreadId == Marker.ThreadId
				&& Event.OldThreadId != Marker.ThreadId;
			if (bSwitchingOut)
			{
				if (State != EThreadState::Running)
				{
					Result.bConflictingState = true;
				}
				State = IsReadyState(Event.OldThreadState, Event.OldThreadWaitReason)
					? EThreadState::Ready
					: EThreadState::Blocked;
				CurrentCore = INDEX_NONE;
				++Result.ContextSwitchCount;
				if (State == EThreadState::Ready)
				{
					++Result.PreemptionCount;
				}
			}
			if (bSwitchingIn)
			{
				if (State == EThreadState::Running)
				{
					Result.bConflictingState = true;
				}
				if (LastRunningCore != INDEX_NONE && LastRunningCore != Event.Core)
				{
					++Result.MigrationCount;
				}
				LastRunningCore = Event.Core;
				CurrentCore = Event.Core;
				State = EThreadState::Running;
			}
		}
		AddTicks(Result, State, CurrentCore, Marker.QpcEnd - Cursor, MicrosecondsPerTick);
		Result.bEndRunning = State == EThreadState::Running;

		const double Accounted = Result.OnCpuMicroseconds
			+ Result.ReadyMicroseconds
			+ Result.BlockedMicroseconds
			+ Result.UnresolvedMicroseconds;
		const double ClosureTolerance = FMath::Max(0.05, Result.WallMicroseconds * 1.0e-6);
		Result.bTimelineClosed = !Result.bEventsLost
			&& !Result.bConflictingState
			&& Result.bBeginRunning
			&& Result.bEndRunning
			&& FMath::Abs(Accounted - Result.WallMicroseconds) <= ClosureTolerance;
		if (!Result.bTimelineClosed)
		{
			Result.UnresolvedMicroseconds = Result.WallMicroseconds;
		}
		return Result;
	}

	bool IsMarkerThreadOwnershipValid(
		const FSampleMarker& Marker,
		const TMap<uint32, uint32>& ThreadProcessIds)
	{
		const uint32* ProcessId = ThreadProcessIds.Find(Marker.ThreadId);
		return ProcessId && *ProcessId == Marker.ProcessId;
	}
}
