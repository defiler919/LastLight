#include "SightWeavePreparedEventIndex.h"

namespace
{
	bool TolerancesEqual(
		const FSightWeaveGeometryTolerances& A,
		const FSightWeaveGeometryTolerances& B)
	{
		return A.AuthoringWeldEpsilon == B.AuthoringWeldEpsilon
			&& A.ZeroLengthEpsilon == B.ZeroLengthEpsilon
			&& A.RayParallelEpsilon == B.RayParallelEpsilon
			&& A.EndpointAngularEpsilonDegrees == B.EndpointAngularEpsilonDegrees
			&& A.PointOnEdgeEpsilon == B.PointOnEdgeEpsilon
			&& A.PointInPolygonEpsilon == B.PointInPolygonEpsilon
			&& A.DuplicateVertexEpsilon == B.DuplicateVertexEpsilon
			&& A.HeightOverlapEpsilon == B.HeightOverlapEpsilon
			&& A.RadialBoundarySteps == B.RadialBoundarySteps;
	}
}

void FSightWeavePreparedEventIndex::Initialize(
	const int32 InMaximumEntries,
	const int64 InMaximumBytes)
{
	MaximumEntries = FMath::Clamp(InMaximumEntries, 1, 1024);
	MaximumBytes = FMath::Clamp<int64>(InMaximumBytes, 1ll * 1024ll * 1024ll, 1ll * 1024ll * 1024ll * 1024ll);
	Slots.Reset();
	Slots.SetNum(MaximumEntries);
	NextGeneration = 1;
	Stats = {};
}

void FSightWeavePreparedEventIndex::InvalidateAll()
{
	for (FSlot& Slot : Slots)
	{
		if (Slot.bValid)
		{
			++Stats.InvalidatedEntryCount;
		}
		Slot.bValid = false;
		Slot.BindingCount = 0;
		Slot.LastUsedRevision = 0;
		if (Slot.Cache.IsValid())
		{
			Slot.Cache->bInputInvariantInitialized = false;
			Slot.Cache->bAbsoluteEndpointEventsValid = false;
		}
		Slot.Cache.Reset();
	}
	UpdateLiveStats();
}

void FSightWeavePreparedEventIndex::Reset()
{
	Slots.Reset();
	MaximumEntries = 32;
	MaximumBytes = 64ll * 1024ll * 1024ll;
	NextGeneration = 1;
	Stats = {};
}

FSightWeavePreparedEventIndex::FAcquireResult FSightWeavePreparedEventIndex::Acquire(
	const FSightWeaveReferenceSolveInput& Input,
	const TSharedPtr<FSightWeaveOptimizedSolveCache>& CurrentBinding,
	const uint64 Revision)
{
	FAcquireResult Result;
	for (FSlot& Slot : Slots)
	{
		if (Slot.bValid && Slot.Cache.IsValid() && MatchesInput(*Slot.Cache, Input))
		{
			if (Slot.Cache != CurrentBinding)
			{
				Release(CurrentBinding);
				++Slot.BindingCount;
			}
			Slot.LastUsedRevision = Revision;
			++Stats.HitCount;
			Result.Cache = Slot.Cache;
			Result.bHit = true;
			UpdateLiveStats();
			return Result;
		}
	}

	++Stats.MissCount;
	++Stats.FullRebuildCount;
	const int64 EstimatedBytes = EstimateAllocatedBytes(Input);
	if (EstimatedBytes > MaximumBytes)
	{
		Release(CurrentBinding);
		++Stats.OversizedEntryCount;
		++Stats.CapacityFallbackCount;
		Result.bCapacityFallback = true;
		UpdateLiveStats();
		return Result;
	}

	FSlot* Selected = FindSlot(CurrentBinding);
	const bool bReusingExclusiveCurrent = Selected && Selected->BindingCount == 1;
	if (!Selected || Selected->BindingCount != 1)
	{
		Release(CurrentBinding);
		Selected = nullptr;
		for (FSlot& Slot : Slots)
		{
			if (!Slot.bValid)
			{
				Selected = &Slot;
				break;
			}
		}
		if (!Selected)
		{
			for (FSlot& Slot : Slots)
			{
				if (Slot.BindingCount == 0
					&& (!Selected
						|| Slot.LastUsedRevision < Selected->LastUsedRevision
						|| (Slot.LastUsedRevision == Selected->LastUsedRevision
							&& Slot.Generation < Selected->Generation)))
				{
					Selected = &Slot;
				}
			}
		}
	}

	if (!Selected)
	{
		++Stats.CapacityFallbackCount;
		Result.bCapacityFallback = true;
		UpdateLiveStats();
		return Result;
	}

	if (Selected->bValid && !bReusingExclusiveCurrent)
	{
		++Stats.EvictionCount;
	}
	if (!Selected->Cache.IsValid())
	{
		Selected->Cache = MakeShared<FSightWeaveOptimizedSolveCache>();
	}
	Selected->Cache->bInputInvariantInitialized = false;
	Selected->Cache->bAbsoluteEndpointEventsValid = false;
	Selected->bValid = true;
	Selected->BindingCount = 1;
	Selected->LastUsedRevision = Revision;
	Selected->Generation = NextGeneration++;
	Result.Cache = Selected->Cache;
	UpdateLiveStats();
	return Result;
}

bool FSightWeavePreparedEventIndex::Commit(
	const TSharedPtr<FSightWeaveOptimizedSolveCache>& Cache)
{
	FSlot* Slot = FindSlot(Cache);
	if (!Slot || !Slot->bValid)
	{
		return false;
	}

	UpdateLiveStats();
	const int64 EntryBytes = GetAllocatedBytes(*Cache);
	while (EntryBytes <= MaximumBytes && Stats.LiveAllocatedBytes > MaximumBytes)
	{
		FSlot* EvictionCandidate = nullptr;
		for (FSlot& Candidate : Slots)
		{
			if (&Candidate == Slot || !Candidate.bValid || Candidate.BindingCount != 0)
			{
				continue;
			}
			if (!EvictionCandidate
				|| Candidate.LastUsedRevision < EvictionCandidate->LastUsedRevision
				|| (Candidate.LastUsedRevision == EvictionCandidate->LastUsedRevision
					&& Candidate.Generation < EvictionCandidate->Generation))
			{
				EvictionCandidate = &Candidate;
			}
		}
		if (!EvictionCandidate)
		{
			break;
		}
		EvictionCandidate->bValid = false;
		EvictionCandidate->Cache.Reset();
		++Stats.EvictionCount;
		UpdateLiveStats();
	}
	if (EntryBytes <= MaximumBytes && Stats.LiveAllocatedBytes <= MaximumBytes)
	{
		return true;
	}

	if (EntryBytes > MaximumBytes)
	{
		++Stats.OversizedEntryCount;
	}
	else
	{
		++Stats.CapacityFallbackCount;
	}
	Slot->bValid = false;
	Slot->BindingCount = 0;
	Cache->bInputInvariantInitialized = false;
	Cache->bAbsoluteEndpointEventsValid = false;
	Slot->Cache.Reset();
	UpdateLiveStats();
	return false;
}

void FSightWeavePreparedEventIndex::Release(
	const TSharedPtr<FSightWeaveOptimizedSolveCache>& Cache)
{
	if (FSlot* Slot = FindSlot(Cache))
	{
		Slot->BindingCount = FMath::Max(0, Slot->BindingCount - 1);
	}
	UpdateLiveStats();
}

bool FSightWeavePreparedEventIndex::MatchesInput(
	const FSightWeaveOptimizedSolveCache& Cache,
	const FSightWeaveReferenceSolveInput& Input)
{
	const FVector2D Origin(Input.Origin.X, Input.Origin.Y);
	if (!Cache.bInputInvariantInitialized
		|| Cache.Origin.X != Origin.X
		|| Cache.Origin.Y != Origin.Y
		|| Cache.FloorId != Input.FloorId
		|| Cache.HeightRange.ZMin != Input.HeightRange.ZMin
		|| Cache.HeightRange.ZMax != Input.HeightRange.ZMax
		|| !TolerancesEqual(Cache.Tolerances, Input.Tolerances)
		|| Cache.SegmentSlots.Num() != Input.Segments.Num())
	{
		return false;
	}
	for (int32 SegmentIndex = 0; SegmentIndex < Input.Segments.Num(); ++SegmentIndex)
	{
		if (!Cache.SegmentSlots[SegmentIndex].Matches(Input.Segments[SegmentIndex]))
		{
			return false;
		}
	}
	return true;
}

int64 FSightWeavePreparedEventIndex::GetAllocatedBytes(
	const FSightWeaveOptimizedSolveCache& Cache)
{
	return static_cast<int64>(Cache.SegmentSlots.GetAllocatedSize())
		+ static_cast<int64>(Cache.CandidateSegments.GetAllocatedSize())
		+ static_cast<int64>(Cache.SortedAbsoluteEndpointAngles.GetAllocatedSize())
		+ static_cast<int64>(Cache.AbsoluteEndpointAngleSortBuffer.GetAllocatedSize())
		+ static_cast<int64>(Cache.SortedAbsoluteEndpointDirections.GetAllocatedSize());
}

int64 FSightWeavePreparedEventIndex::EstimateAllocatedBytes(
	const FSightWeaveReferenceSolveInput& Input)
{
	constexpr int64 ConservativeBytesPerSegment = 512;
	return FMath::Max<int64>(4096, static_cast<int64>(Input.Segments.Num()) * ConservativeBytesPerSegment);
}

FSightWeavePreparedEventIndex::FSlot* FSightWeavePreparedEventIndex::FindSlot(
	const TSharedPtr<FSightWeaveOptimizedSolveCache>& Cache)
{
	if (!Cache.IsValid())
	{
		return nullptr;
	}
	return Slots.FindByPredicate([&Cache](const FSlot& Slot) { return Slot.Cache == Cache; });
}

const FSightWeavePreparedEventIndex::FSlot* FSightWeavePreparedEventIndex::FindSlot(
	const TSharedPtr<FSightWeaveOptimizedSolveCache>& Cache) const
{
	if (!Cache.IsValid())
	{
		return nullptr;
	}
	return Slots.FindByPredicate([&Cache](const FSlot& Slot) { return Slot.Cache == Cache; });
}

void FSightWeavePreparedEventIndex::UpdateLiveStats()
{
	Stats.LiveEntryCount = 0;
	Stats.SourceBindingCount = 0;
	Stats.LiveAllocatedBytes = 0;
	for (const FSlot& Slot : Slots)
	{
		if (!Slot.bValid || !Slot.Cache.IsValid())
		{
			continue;
		}
		++Stats.LiveEntryCount;
		Stats.SourceBindingCount += Slot.BindingCount;
		Stats.LiveAllocatedBytes += GetAllocatedBytes(*Slot.Cache);
	}
	Stats.HighWaterAllocatedBytes = FMath::Max(
		Stats.HighWaterAllocatedBytes,
		Stats.LiveAllocatedBytes);
}
