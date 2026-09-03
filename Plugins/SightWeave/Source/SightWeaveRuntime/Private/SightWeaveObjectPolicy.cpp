#include "SightWeaveObjectPolicy.h"
#include "SightWeaveSettings.h"

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
	Capture.Initialize(FResolvedSightWeaveObjectPolicy::Resolve(
		GetDefault<USightWeaveSettings>()->DefaultHistoryMode, PolicySource, HistoryMode));
}
