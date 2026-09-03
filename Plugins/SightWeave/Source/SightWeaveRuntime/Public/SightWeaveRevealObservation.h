#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SightWeaveObjectPolicy.h"

/** One registered object's legal observation session. No world, UObject or identity access.
 * Footprint samples and axis steps are supplied in object-local order/world centimeters.
 * Confirmation grants object presentation only; hosts must still apply occlusion/gates.
 */
struct SIGHTWEAVERUNTIME_API FSightWeaveRevealObservation
{
	bool Initialize(const FResolvedSightWeaveObjectPolicy& Policy, FIntPoint Size,
		FVector2D CellSpanCm, const TBitArray<>& Footprint);
	void Observe(bool bValidRevision, const TBitArray<>& CurrentLegalObservationMask);
	void Reset();
	bool IsInitialized() const { return Footprint.Num()>0; }
	bool IsConfirmed() const;
	FGameplayTag GetState() const { return State; }
	float GetObservedSpanCm() const { return ObservedSpanCm; }
	float GetEffectiveMinimumSpanCm() const { return EffectiveMinimumSpanCm; }
	float GetMaximumSpanCm() const { return MaximumSpanCm; }
	const TBitArray<>& GetTentativeMask() const { return TentativeConfirmationMask; }
	uint64 GetSpanEvaluations() const { return SpanEvaluations; }
	static float LongestContinuousSpan(FIntPoint Size,FVector2D CellSpanCm,
		const TBitArray<>& Footprint,const TBitArray<>& Observed);
private:
	FIntPoint Size=FIntPoint::ZeroValue;
	FVector2D CellSpanCm=FVector2D::ZeroVector;
	TBitArray<> Footprint, TentativeConfirmationMask;
	FGameplayTag State;
	float ObservedSpanCm=0, EffectiveMinimumSpanCm=0, MaximumSpanCm=0;
	uint64 SpanEvaluations=0;
};
