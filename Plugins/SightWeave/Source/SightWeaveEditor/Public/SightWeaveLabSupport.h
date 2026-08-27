#pragma once

#include "CoreMinimal.h"

class UWorld;

enum class ESightWeaveLabMode : uint8
{
	M2,
	M3P3,
	M3P4
};

struct FSightWeaveLabIsolationResult
{
	int32 EnabledVisionSources = 0;
	int32 DisabledVisionSources = 0;
	int32 EnabledIlluminationSources = 0;
	int32 DisabledIlluminationSources = 0;
	int32 EnabledOccluders = 0;
	int32 DisabledOccluders = 0;
	int32 EnabledSuppressions = 0;
	int32 DisabledSuppressions = 0;
	int32 AdjustedPageBoundaryActors = 0;
};

namespace SightWeave::Lab
{
	/** Center of logical row 7 for the Lab Ground floor (-6500 + 7.5 * 2480). */
	inline constexpr double SafePageBoundaryY = 12100.0;

	SIGHTWEAVEEDITOR_API ESightWeaveLabMode ResolveModeFromCommandLine();
	SIGHTWEAVEEDITOR_API const TCHAR* LexToString(ESightWeaveLabMode Mode);
	SIGHTWEAVEEDITOR_API bool IsLabWorld(const UWorld* World);
	SIGHTWEAVEEDITOR_API bool IsFixtureEnabled(const FString& ActorLabel, ESightWeaveLabMode Mode);
	SIGHTWEAVEEDITOR_API bool IsSettingsSectionRegistered();
	SIGHTWEAVEEDITOR_API FSightWeaveLabIsolationResult ApplyFixtureIsolation(
		UWorld* World,
		ESightWeaveLabMode Mode);
}
