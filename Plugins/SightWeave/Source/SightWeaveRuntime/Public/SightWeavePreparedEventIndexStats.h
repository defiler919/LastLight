#pragma once

#include "CoreMinimal.h"

#include "SightWeavePreparedEventIndexStats.generated.h"

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeavePreparedEventIndexStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Prepared Event Index")
	int32 LiveEntryCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Prepared Event Index")
	int32 SourceBindingCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Prepared Event Index")
	int64 HitCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Prepared Event Index")
	int64 MissCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Prepared Event Index")
	int64 FullRebuildCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Prepared Event Index")
	int64 EvictionCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Prepared Event Index")
	int64 CapacityFallbackCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Prepared Event Index")
	int64 OversizedEntryCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Prepared Event Index")
	int64 InvalidatedEntryCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Prepared Event Index")
	int64 LiveAllocatedBytes = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Prepared Event Index")
	int64 HighWaterAllocatedBytes = 0;
};
