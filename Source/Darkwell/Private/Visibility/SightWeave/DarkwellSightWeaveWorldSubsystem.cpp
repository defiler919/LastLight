// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visibility/SightWeave/DarkwellSightWeaveWorldSubsystem.h"

#include "Engine/World.h"
#include "SightWeaveWorldSubsystem.h"
#if !UE_SERVER
#include "SightWeaveRenderWorldSubsystem.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogDarkwellSightWeave, Log, All);

namespace Darkwell::SightWeaveAdapter
{
	uint64 NextWorldGeneration = 1;
}

void UDarkwellSightWeaveWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Diagnostics = FDarkwellVisibilityAuthorityDiagnostics();
	Diagnostics.WorldGeneration = Darkwell::SightWeaveAdapter::NextWorldGeneration++;
	if (const UWorld* World = GetWorld())
	{
		Diagnostics.WorldName = World->GetFName();
	}

	RuntimeSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<USightWeaveWorldSubsystem>()
		: nullptr;
	Diagnostics.bRuntimeServiceAvailable = RuntimeSubsystem
		&& RuntimeSubsystem->IsSightWeaveInitialized();

#if !UE_SERVER
	RenderSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<USightWeaveRenderWorldSubsystem>()
		: nullptr;
	Diagnostics.bRenderServiceAvailable = RenderSubsystem != nullptr;
#else
	Diagnostics.bRenderServiceAvailable = false;
#endif

	ResetToLegacy();
	UE_LOG(
		LogDarkwellSightWeave,
		Log,
		TEXT("World=%s authority=Legacy generation=%llu runtime=%d render=%d"),
		*Diagnostics.WorldName.ToString(),
		Diagnostics.WorldGeneration,
		Diagnostics.bRuntimeServiceAvailable ? 1 : 0,
		Diagnostics.bRenderServiceAvailable ? 1 : 0);
}

void UDarkwellSightWeaveWorldSubsystem::Deinitialize()
{
	UE_LOG(
		LogDarkwellSightWeave,
		Log,
		TEXT("World=%s authority=%s teardown generation=%llu"),
		*Diagnostics.WorldName.ToString(),
		Diagnostics.ActiveMode == EDarkwellVisibilityAuthorityMode::SightWeave
			? TEXT("SightWeave")
			: TEXT("Legacy"),
		Diagnostics.WorldGeneration);

#if !UE_SERVER
	if (RenderSubsystem)
	{
		RenderSubsystem->ClearPresentationScope();
	}
	RenderSubsystem = nullptr;
#endif
	RuntimeSubsystem = nullptr;
	ResetToLegacy();
	Super::Deinitialize();
}

bool UDarkwellSightWeaveWorldSubsystem::HasRequiredSightWeaveServices() const
{
	if (!Diagnostics.bRuntimeServiceAvailable || !RuntimeSubsystem)
	{
		return false;
	}
#if UE_SERVER
	return true;
#else
	return Diagnostics.bRenderServiceAvailable && RenderSubsystem;
#endif
}

void UDarkwellSightWeaveWorldSubsystem::ResetToLegacy()
{
	Diagnostics.RequestedMode = EDarkwellVisibilityAuthorityMode::Legacy;
	Diagnostics.ActiveMode = EDarkwellVisibilityAuthorityMode::Legacy;
	Diagnostics.State = EDarkwellVisibilityAuthorityState::Legacy;
	Diagnostics.FailureReason.Reset();
	Diagnostics.AuthorityRevision = 0;
	Diagnostics.RuntimeSnapshotRevision = 0;
	Diagnostics.FloorCount = 0;
	Diagnostics.VisionSourceCount = 0;
	Diagnostics.IlluminationSourceCount = 0;
	Diagnostics.OccluderCount = 0;
	Diagnostics.StaticEnvironmentCount = 0;
	Diagnostics.SubjectCount = 0;
	Diagnostics.bLegacyWritesEnabled = true;
	Diagnostics.bLegacyPresentationEnabled = true;
	Diagnostics.bSightWeavePresentationEnabled = false;
}
