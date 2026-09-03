#pragma once
#include "SightWeaveSettings.h"

/** Existing regression suites exercise the frozen SpatialPartial/Always default
 * project. Scope that original config to the fixture; preserve every assertion.
 * Registration caches per-object values; this is test setup, never a runtime switch.
 */
struct FDarkwellLegacyObjectPolicyFixture
{
 TGuardValue<ESightWeaveRevealMode> Reveal{GetMutableDefault<USightWeaveSettings>()->DefaultRevealMode,ESightWeaveRevealMode::SpatialPartial};
 TGuardValue<float> Span{GetMutableDefault<USightWeaveSettings>()->DefaultMinimumObservedSpanCm,100.f};
 TGuardValue<ESightWeaveHistoryMode> History{GetMutableDefault<USightWeaveSettings>()->DefaultHistoryMode,ESightWeaveHistoryMode::Always};
};
