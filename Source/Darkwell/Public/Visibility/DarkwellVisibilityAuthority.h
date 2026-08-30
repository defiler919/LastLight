// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DarkwellVisibilityAuthority.generated.h"

/** Exactly one visibility authority may control a DARKWELL world session. */
UENUM()
enum class EDarkwellVisibilityAuthorityMode : uint8
{
	Legacy,
	SightWeave
};

/** Read-only activation state; only ActiveSightWeave changes consumer ownership. */
UENUM()
enum class EDarkwellVisibilityAuthorityState : uint8
{
	Legacy,
	SightWeaveRequested,
	ActiveSightWeave,
	SightWeaveFailed
};

/** One neutral product snapshot shared by subject presentation and HUD consumers. */
USTRUCT()
struct DARKWELL_API FDarkwellVisibilitySubjectSnapshot
{
	GENERATED_BODY()

	FName StableSubjectId = NAME_None;
	EDarkwellVisibilityAuthorityMode AuthorityMode =
		EDarkwellVisibilityAuthorityMode::Legacy;
	uint64 AuthorityRevision = 0;
	int64 SourceSnapshotRevision = 0;
	bool bAuthoritative = false;
	bool bHardLive = false;

	bool IsUsableFor(FName SubjectId) const
	{
		return bAuthoritative
			&& !StableSubjectId.IsNone()
			&& StableSubjectId == SubjectId
			&& AuthorityRevision > 0;
	}
};

/** Product-facing diagnostics contain no renderer-owned types or resources. */
USTRUCT()
struct DARKWELL_API FDarkwellVisibilityAuthorityDiagnostics
{
	GENERATED_BODY()

	EDarkwellVisibilityAuthorityMode RequestedMode =
		EDarkwellVisibilityAuthorityMode::Legacy;
	EDarkwellVisibilityAuthorityMode ActiveMode =
		EDarkwellVisibilityAuthorityMode::Legacy;
	EDarkwellVisibilityAuthorityState State =
		EDarkwellVisibilityAuthorityState::Legacy;
	FName WorldName = NAME_None;
	FString FailureReason;
	uint64 WorldGeneration = 0;
	uint64 AuthorityRevision = 0;
	int64 RuntimeSnapshotRevision = 0;
	int32 FloorCount = 0;
	int32 VisionSourceCount = 0;
	int32 IlluminationSourceCount = 0;
	int32 OccluderCount = 0;
	int32 StaticEnvironmentCount = 0;
	int32 SubjectCount = 0;
	bool bRuntimeServiceAvailable = false;
	bool bRenderServiceAvailable = false;
	bool bLegacyWritesEnabled = true;
	bool bLegacyPresentationEnabled = true;
	bool bSightWeavePresentationEnabled = false;
	bool bProjectFogPresentationEnabled = false;
};
