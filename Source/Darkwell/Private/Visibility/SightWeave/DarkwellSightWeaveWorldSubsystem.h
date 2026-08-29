// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Visibility/DarkwellVisibilityAuthority.h"
#include "DarkwellSightWeaveWorldSubsystem.generated.h"

class USightWeaveWorldSubsystem;
class USightWeaveRenderWorldSubsystem;

/**
 * DARKWELL-owned visibility authority boundary. M6P1 registrations and consumers
 * are added behind this world-scoped facade; Legacy remains the default.
 */
UCLASS()
class DARKWELL_API UDarkwellSightWeaveWorldSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	EDarkwellVisibilityAuthorityMode GetAuthorityMode() const
	{
		return Diagnostics.ActiveMode;
	}
	EDarkwellVisibilityAuthorityState GetAuthorityState() const
	{
		return Diagnostics.State;
	}
	bool IsSightWeaveAuthorityActive() const
	{
		return Diagnostics.State == EDarkwellVisibilityAuthorityState::ActiveSightWeave
			&& Diagnostics.ActiveMode == EDarkwellVisibilityAuthorityMode::SightWeave;
	}
	const FDarkwellVisibilityAuthorityDiagnostics& GetDiagnostics() const
	{
		return Diagnostics;
	}
	bool HasRequiredSightWeaveServices() const;

private:
	void ResetToLegacy();

	UPROPERTY(Transient)
	TObjectPtr<USightWeaveWorldSubsystem> RuntimeSubsystem;

	/** Render is a non-Server private module seam and is never reflected/serialized. */
	USightWeaveRenderWorldSubsystem* RenderSubsystem = nullptr;

	FDarkwellVisibilityAuthorityDiagnostics Diagnostics;
};
