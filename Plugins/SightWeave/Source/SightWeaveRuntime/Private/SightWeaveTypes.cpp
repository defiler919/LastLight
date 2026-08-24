#include "SightWeaveTypes.h"

namespace SightWeave
{
	void NormalizeNames(TArray<FName>& Names)
	{
		Names.RemoveAll([](const FName Name) { return Name.IsNone(); });
		Names.Sort([](const FName A, const FName B) { return A.LexicalLess(B); });
		for (int32 Index = Names.Num() - 1; Index > 0; --Index)
		{
			if (Names[Index] == Names[Index - 1])
			{
				Names.RemoveAt(Index);
			}
		}
	}
}

void FSightWeaveIlluminationCompatibilityProfile::Normalize()
{
	SightWeave::NormalizeNames(AcceptedCapabilities);
}

bool FSightWeaveIlluminationCompatibilityProfile::Accepts(const FName Capability) const
{
	return !Capability.IsNone() && AcceptedCapabilities.Contains(Capability);
}

bool FSightWeaveIlluminationCompatibilityProfile::IsEquivalentTo(const FSightWeaveIlluminationCompatibilityProfile& Other) const
{
	TArray<FName> Left = AcceptedCapabilities;
	TArray<FName> Right = Other.AcceptedCapabilities;
	SightWeave::NormalizeNames(Left);
	SightWeave::NormalizeNames(Right);
	return Left == Right;
}

FSightWeaveVisionSourceDescription::FSightWeaveVisionSourceDescription()
	: FloorId(FName(TEXT("Default")))
{
}

bool FSightWeaveVisionSourceDescription::IsValid() const
{
	const bool bShapeValuesValid = FMath::IsFinite(Range)
		&& FMath::IsFinite(HalfAngleDegrees)
		&& FMath::IsFinite(NearAwarenessRadius)
		&& Range > 0.0f
		&& HalfAngleDegrees >= 0.0f
		&& HalfAngleDegrees <= 180.0f
		&& NearAwarenessRadius >= 0.0f
		&& NearAwarenessRadius <= Range;
	const bool bCompatibilityValid = IlluminationPolicy == ESightWeaveIlluminationPolicy::BypassLegalIllumination
		|| Compatibility.AcceptedCapabilities.ContainsByPredicate([](const FName Capability) { return !Capability.IsNone(); });
	return !Transform.ContainsNaN() && FloorId.IsValid() && HeightRange.IsValid() && bShapeValuesValid && bCompatibilityValid;
}

FSightWeaveIlluminationSourceDescription::FSightWeaveIlluminationSourceDescription()
	: FloorId(FName(TEXT("Default")))
{
	EmittedCapabilities.Add(FName(TEXT("Visible")));
}

void FSightWeaveIlluminationSourceDescription::NormalizeCapabilities()
{
	SightWeave::NormalizeNames(EmittedCapabilities);
}

bool FSightWeaveIlluminationSourceDescription::IsValid() const
{
	return !Transform.ContainsNaN()
		&& FloorId.IsValid()
		&& HeightRange.IsValid()
		&& FMath::IsFinite(Range)
		&& Range > 0.0f
		&& FMath::IsFinite(HalfAngleDegrees)
		&& HalfAngleDegrees >= 0.0f
		&& HalfAngleDegrees <= 180.0f
		&& EmittedCapabilities.ContainsByPredicate([](const FName Capability) { return !Capability.IsNone(); });
}

bool FSightWeaveSubjectRevealSpecification::IsValid() const
{
	return !KnowledgeOwnerId.IsNone()
		&& !SubjectId.IsNone()
		&& FMath::IsFinite(DurationSeconds)
		&& DurationSeconds >= 0.0f;
}
