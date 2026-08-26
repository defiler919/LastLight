#pragma once

#include "SightWeaveGeometry.h"
#include "SightWeaveOptimizedSolveCache.h"
#include "SightWeavePreparedEventIndexStats.h"

/**
 * Game-thread-only, world-owned cache of exact observer-origin preparation.
 * Published snapshots never reference these mutable entries.
 */
class FSightWeavePreparedEventIndex
{
public:
	struct FAcquireResult
	{
		TSharedPtr<FSightWeaveOptimizedSolveCache> Cache;
		bool bHit = false;
		bool bCapacityFallback = false;
	};

	void Initialize(int32 InMaximumEntries, int64 InMaximumBytes);
	void InvalidateAll();
	void Reset();

	FAcquireResult Acquire(
		const FSightWeaveReferenceSolveInput& Input,
		const TSharedPtr<FSightWeaveOptimizedSolveCache>& CurrentBinding,
		uint64 Revision);
	bool TryReuseExactResult(
		const FSightWeaveReferenceSolveInput& Input,
		const TSharedPtr<FSightWeaveOptimizedSolveCache>& Cache,
		FSightWeaveReferenceSolveResult& OutResult);
	bool StoreExactResult(
		const FSightWeaveReferenceSolveInput& Input,
		const FSightWeaveReferenceSolveResult& Result,
		const TSharedPtr<FSightWeaveOptimizedSolveCache>& Cache);

	/** Returns false when the bounded index cannot retain this preparation. */
	bool Commit(const TSharedPtr<FSightWeaveOptimizedSolveCache>& Cache);
	void Release(const TSharedPtr<FSightWeaveOptimizedSolveCache>& Cache);

	FSightWeavePreparedEventIndexStats GetStats() const { return Stats; }

private:
	struct FSlot
	{
		TSharedPtr<FSightWeaveOptimizedSolveCache> Cache;
		uint64 Generation = 0;
		uint64 LastUsedRevision = 0;
		int32 BindingCount = 0;
		bool bValid = false;
	};

	static bool MatchesInput(
		const FSightWeaveOptimizedSolveCache& Cache,
		const FSightWeaveReferenceSolveInput& Input);
	static bool MatchesExactResultInput(
		const FSightWeaveOptimizedSolveCache& Cache,
		const FSightWeaveReferenceSolveInput& Input);
	static int64 GetPreparationAllocatedBytes(const FSightWeaveOptimizedSolveCache& Cache);
	static int64 GetExactResultAllocatedBytes(const FSightWeaveOptimizedSolveCache& Cache);
	static int64 GetAllocatedBytes(const FSightWeaveOptimizedSolveCache& Cache);
	static int64 EstimateAllocatedBytes(const FSightWeaveReferenceSolveInput& Input);
	FSlot* FindSlot(const TSharedPtr<FSightWeaveOptimizedSolveCache>& Cache);
	const FSlot* FindSlot(const TSharedPtr<FSightWeaveOptimizedSolveCache>& Cache) const;
	void UpdateLiveStats();

	TArray<FSlot> Slots;
	int32 MaximumEntries = 32;
	int64 MaximumBytes = 64ll * 1024ll * 1024ll;
	uint64 NextGeneration = 1;
	FSightWeavePreparedEventIndexStats Stats;
};
