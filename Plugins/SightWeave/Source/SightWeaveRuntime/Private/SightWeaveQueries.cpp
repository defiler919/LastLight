#include "SightWeaveQueries.h"

bool FSightWeaveHardSuppressionDescription::IsValid() const
{
	return FloorId.IsValid()
		&& HeightRange.IsValid()
		&& !Center.ContainsNaN()
		&& FMath::IsFinite(Radius)
		&& Radius > 0.0f;
}

bool FSightWeaveQuerySampleSet::IsValid() const
{
	if (Samples.IsEmpty()
		|| Samples.ContainsByPredicate([](const FVector& Sample) { return Sample.ContainsNaN(); }))
	{
		return false;
	}
	if (Rule == ESightWeaveSampleRule::Anchor)
	{
		return Samples.IsValidIndex(AnchorIndex);
	}
	return Rule != ESightWeaveSampleRule::RequiredCount
		|| (RequiredCount > 0 && RequiredCount <= Samples.Num());
}
