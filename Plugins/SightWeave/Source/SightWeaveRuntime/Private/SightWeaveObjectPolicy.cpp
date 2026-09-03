#include "SightWeaveObjectPolicy.h"
#include "SightWeaveSettings.h"

FResolvedSightWeaveObjectPolicy FResolvedSightWeaveObjectPolicy::Resolve(
	const FResolvedSightWeaveObjectPolicy& Defaults, const FSightWeaveObjectPolicyOverrides& Overrides)
{
	FResolvedSightWeaveObjectPolicy Result = Defaults;
	if (Overrides.bOverrideRevealMode) Result.RevealMode = Overrides.RevealMode;
	if (Overrides.bOverrideMinimumObservedSpan) Result.MinimumObservedSpanCm = Overrides.MinimumObservedSpanCm;
	// Keep old serialized Override/HistoryMode authoring effective. The new field
	// is additive; UseProjectDefault never prevents a per-field override.
	if (Overrides.bOverrideHistoryMode || Overrides.PolicySource == ESightWeaveObjectPolicySource::Override)
		Result.HistoryMode = Overrides.HistoryMode;
	if (!FMath::IsFinite(Result.MinimumObservedSpanCm))
		Result.MinimumObservedSpanCm = FMath::IsFinite(Defaults.MinimumObservedSpanCm) ? Defaults.MinimumObservedSpanCm : 100.f;
	Result.MinimumObservedSpanCm = FMath::Max(0.f, Result.MinimumObservedSpanCm);
	return Result;
}

FResolvedSightWeaveObjectPolicy FResolvedSightWeaveObjectPolicy::Resolve(
	ESightWeaveHistoryMode ProjectDefault, ESightWeaveObjectPolicySource Source,
	ESightWeaveHistoryMode Override)
{
	FResolvedSightWeaveObjectPolicy Result;
	Result.HistoryMode = Source == ESightWeaveObjectPolicySource::Override ? Override : ProjectDefault;
	return Result;
}

void FSightWeaveObjectHistoryCapture::Initialize(FResolvedSightWeaveObjectPolicy InPolicy)
{
	Policy = InPolicy;
	bMoving = false;
	MovingRevision = 0;
	bRequiresFreshStationaryObservation = true;
}

bool FSightWeaveObjectHistoryCapture::SetMoving(bool bInMoving)
{
	if (bMoving == bInMoving) return false;
	bMoving = bInMoving;
	++MovingRevision;
	if (Policy.HistoryMode == ESightWeaveHistoryMode::StationaryOnly)
		bRequiresFreshStationaryObservation = true;
	return true;
}

void FSightWeaveObjectHistoryCapture::ObserveLegally()
{
	if (!bMoving) bRequiresFreshStationaryObservation = false;
}

bool FSightWeaveObjectHistoryCapture::IsHistoryEligible() const
{
	return Policy.HistoryMode == ESightWeaveHistoryMode::Always
		|| (Policy.HistoryMode == ESightWeaveHistoryMode::StationaryOnly
			&& !bMoving && !bRequiresFreshStationaryObservation);
}

USightWeaveObjectPolicyComponent::USightWeaveObjectPolicyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USightWeaveObjectPolicyComponent::OnRegister()
{
	Super::OnRegister();
	const USightWeaveSettings* Settings = GetDefault<USightWeaveSettings>();
	FResolvedSightWeaveObjectPolicy Defaults;
	Defaults.RevealMode = Settings->DefaultRevealMode;
	Defaults.MinimumObservedSpanCm = Settings->DefaultMinimumObservedSpanCm;
	Defaults.HistoryMode = Settings->DefaultHistoryMode;
	FSightWeaveObjectPolicyOverrides Overrides;
	Overrides.bOverrideRevealMode = bOverrideRevealMode;
	Overrides.RevealMode = RevealMode;
	Overrides.bOverrideMinimumObservedSpan = bOverrideMinimumObservedSpan;
	Overrides.MinimumObservedSpanCm = MinimumObservedSpanCm;
	Overrides.bOverrideHistoryMode = bOverrideHistoryMode;
	Overrides.HistoryMode = HistoryMode;
	Overrides.PolicySource = PolicySource;
	Capture.Initialize(FResolvedSightWeaveObjectPolicy::Resolve(Defaults, Overrides));
}
