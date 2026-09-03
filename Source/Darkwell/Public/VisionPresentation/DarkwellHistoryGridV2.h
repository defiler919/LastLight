#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "VisionPresentation/DarkwellSpatialPropMemory.h"

/** Per-observation evidence, never an actor identity or a renderer-owned cache. */
struct DARKWELL_API FDarkwellHistoryGridV2
{
	static constexpr int32 SamplesPerCell = 4;
	struct FSample
	{
		FGameplayTag State;
		float InitialRemembered = 0;
		float Opacity = 0;
		float FrozenAAEnvelope = 0;
		float EmptyDwell = 0; // Diagnostic exposure at proof; NOT a consecutive-time confirmation gate.
		bool bVerifiedEmpty = false; // Fact survives ownership changes; ownership never sets it.
	};
	static FGameplayTag NeverObserved();
	static FGameplayTag Unresolved();
	static FGameplayTag VerifiedEmpty();
	static FGameplayTag Superseded();
	void Initialize(const FDarkwellSpatialPropMemory& SealedMemory);
	void Initialize(const FDarkwellSpatialPropMemory& SealedMemory, const TBitArray<>& CaptureMask);
	void RestrictToRecordedGeometry(const TBitArray<>& Footprint);
	bool Advance(float DeltaSeconds, TConstArrayView<float> LegalCoverage,
		const TBitArray<>& ActualOccupied, const TBitArray<>& NewerObservedOwnership);
	bool AdvanceDirty(float DeltaSeconds, TConstArrayView<float> LegalCoverage,
		const TBitArray<>& ActualOccupied, const TBitArray<>& NewerObservedOwnership,
		TConstArrayView<int32> DirtyIndices, bool& bOutTopologyChanged);
	FIntPoint GetSize() const { return Size; }
	const FBox2D& GetBounds() const { return Bounds; }
	TConstArrayView<FSample> GetSamples() const { return Samples; }
	bool IsInitialized() const { return !Samples.IsEmpty(); }
	bool HasResidualSurface() const;
	void BuildPresentation(TArray<FLinearColor>& OutPixels) const;
	bool IsFullyVerifiedEmpty() const;
	bool CanEmitCap(int32 RetainedIndex, int32 NeighborIndex) const;
	uint64 EvidenceHash() const;
	/** Ordered four-state knowledge hash; excludes timing-dependent, hard-gated presentation fields. */
	uint64 StateHash() const;
	int32 Count(FGameplayTag State) const;
	int32 CountMixedCoarseCells() const;
	int32 GetActiveTransitionCount() const { return ActiveSamples.Num(); }
private:
	FBox2D Bounds;
	FIntPoint Size = FIntPoint::ZeroValue;
	TArray<FSample> Samples;
	TArray<int32> ActiveSamples;
	TBitArray<> ActiveFlags;
};
