#include "SightWeaveRevealObservation.h"
#include "NativeGameplayTags.h"

namespace SightWeave::RevealObservation
{
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Unobserved,"SightWeave.ObjectReveal.Unobserved");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Tentative,"SightWeave.ObjectReveal.Tentative");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Confirmed,"SightWeave.ObjectReveal.Confirmed");
}

float FSightWeaveRevealObservation::LongestContinuousSpan(FIntPoint InSize,FVector2D Step,
	const TBitArray<>& Shape,const TBitArray<>& Observed)
{
	if(InSize.X<=0 || InSize.Y<=0 || int64(InSize.X)*InSize.Y!=Shape.Num() || Observed.Num()!=Shape.Num()) return 0;
	float Longest=0;
	for(int32 Y=0;Y<InSize.Y;++Y)
	{
		int32 Run=0;
		for(int32 X=0;X<InSize.X;++X)
		{
			const int32 I=Y*InSize.X+X;
			Run=Shape[I] && Observed[I]?Run+1:0;
			Longest=FMath::Max(Longest,float(Run*Step.X));
		}
	}
	for(int32 X=0;X<InSize.X;++X)
	{
		int32 Run=0;
		for(int32 Y=0;Y<InSize.Y;++Y)
		{
			const int32 I=Y*InSize.X+X;
			Run=Shape[I] && Observed[I]?Run+1:0;
			Longest=FMath::Max(Longest,float(Run*Step.Y));
		}
	}
	return Longest;
}
bool FSightWeaveRevealObservation::Initialize(const FResolvedSightWeaveObjectPolicy& Policy,
	FIntPoint InSize,FVector2D Step,const TBitArray<>& Shape)
{
	Reset();
	if(InSize.X<=0 || InSize.Y<=0 || int64(InSize.X)*InSize.Y!=Shape.Num()
		|| !FMath::IsFinite(Step.X) || !FMath::IsFinite(Step.Y) || Step.GetMin()<=0) return false;
	Size=InSize; CellSpanCm=Step; Footprint=Shape;
	TentativeConfirmationMask.Init(false,Shape.Num());
	MaximumSpanCm=LongestContinuousSpan(Size,Step,Shape,Shape);
	const float Configured=FMath::IsFinite(Policy.MinimumObservedSpanCm)?FMath::Max(0.f,Policy.MinimumObservedSpanCm):100.f;
	EffectiveMinimumSpanCm=FMath::Min(Configured,MaximumSpanCm);
	return MaximumSpanCm>0;
}
void FSightWeaveRevealObservation::Reset()
{
	Footprint.Empty(); TentativeConfirmationMask.Empty(); Size=FIntPoint::ZeroValue;
	State=SightWeave::RevealObservation::Unobserved;
	ObservedSpanCm=EffectiveMinimumSpanCm=MaximumSpanCm=0; SpanEvaluations=0;
}
bool FSightWeaveRevealObservation::IsConfirmed() const { return State==SightWeave::RevealObservation::Confirmed; }
void FSightWeaveRevealObservation::Observe(bool bValidRevision,const TBitArray<>& Mask)
{
	if(!bValidRevision || IsConfirmed() || !IsInitialized() || Mask.Num()!=Footprint.Num()) return;
	bool bContact=false,bChanged=false;
	for(TConstSetBitIterator<> It(Mask);It;++It)
	{
		const int32 I=It.GetIndex();
		if(!Footprint[I]) continue;
		bContact=true;
		if(!TentativeConfirmationMask[I]) { TentativeConfirmationMask[I]=true; bChanged=true; }
	}
	if(!bContact)
	{
		TentativeConfirmationMask.Init(false,Footprint.Num()); ObservedSpanCm=0;
		State=SightWeave::RevealObservation::Unobserved; return;
	}
	State=SightWeave::RevealObservation::Tentative;
	if(bChanged) { ++SpanEvaluations; ObservedSpanCm=LongestContinuousSpan(Size,CellSpanCm,Footprint,TentativeConfirmationMask); }
	if(ObservedSpanCm+UE_KINDA_SMALL_NUMBER>=EffectiveMinimumSpanCm)
	{
		State=SightWeave::RevealObservation::Confirmed;
		// Confirmation persists; hot paths no longer maintain tentative bits/span.
		TentativeConfirmationMask.Empty();
	}
}
