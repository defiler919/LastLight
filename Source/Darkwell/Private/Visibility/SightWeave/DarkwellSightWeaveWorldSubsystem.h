// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SightWeaveStaticEnvironment.h"
#include "SightWeaveSubjectMemory.h"
#include "SightWeaveTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Visibility/DarkwellVisibilityAuthority.h"
#include "DarkwellSightWeaveWorldSubsystem.generated.h"

class ADarkwellCharacter;
class ADarkwellStalkerCharacter;
class ADarkwellVisionIntegrationFixture;
class USightWeaveRenderWorldSubsystem;
class USightWeaveWorldSubsystem;
class UDarkwellFogVisualSubsystem;
struct FDarkwellFogVisualSourceSnapshot;

/** DARKWELL-owned, world-scoped boundary between gameplay and SightWeave. */
UCLASS()
class DARKWELL_API UDarkwellSightWeaveWorldSubsystem final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Accepts one fixture-owned request. Activation may wait briefly for spawned actors. */
	bool RequestSightWeaveAuthority(ADarkwellVisionIntegrationFixture* Fixture);
	bool TryGetSubjectSnapshot(
		FName StableSubjectId,
		FDarkwellVisibilitySubjectSnapshot& OutSnapshot) const;

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
	bool TryActivate();
	bool ValidateAndBuildDescriptions(
		ADarkwellCharacter*& OutPlayer,
		ADarkwellStalkerCharacter*& OutStalker,
		FSightWeaveFloorDefinition& OutFloor,
		FSightWeaveVisionSourceDescription& OutBody,
		FSightWeaveVisionSourceDescription& OutCone,
		FSightWeaveIlluminationSourceDescription& OutTorch,
		TArray<FSightWeaveSegment2D>& OutSegments,
		TArray<FSightWeaveStaticEnvironmentDescription>& OutStatic,
		FString& OutFailure) const;
	void UpdateDynamicAuthority();
	void UpdateSubjectAuthority();
	FDarkwellFogVisualSourceSnapshot BuildFogVisualSourceSnapshot() const;
	void SetLegacyConsumersEnabled(bool bEnabled);
	void RollbackToLegacy(const FString& FailureReason, bool bRestoreConsumers);
	void ResetToLegacy();

	UPROPERTY(Transient)
	TObjectPtr<USightWeaveWorldSubsystem> RuntimeSubsystem;

	/** Render is a non-Server private module seam and is never reflected/serialized. */
	USightWeaveRenderWorldSubsystem* RenderSubsystem = nullptr;
	UPROPERTY(Transient)
	TObjectPtr<UDarkwellFogVisualSubsystem> FogVisualSubsystem;

	UPROPERTY(Transient)
	TWeakObjectPtr<ADarkwellVisionIntegrationFixture> RequestedFixture;

	UPROPERTY(Transient)
	TWeakObjectPtr<ADarkwellCharacter> Player;

	UPROPERTY(Transient)
	TWeakObjectPtr<ADarkwellStalkerCharacter> Stalker;

	FSightWeaveFloorId FloorId;
	FSightWeaveKnowledgeOwnerId KnowledgeOwnerId;
	FSightWeaveVisionSourceHandle BodyVisionHandle;
	FSightWeaveVisionSourceHandle ConeVisionHandle;
	FSightWeaveIlluminationSourceHandle TorchIlluminationHandle;
	FSightWeaveOccluderHandle OccluderHandle;
	TArray<FSightWeaveStaticEnvironmentHandle> StaticEnvironmentHandles;
	FSightWeaveSubjectMemoryAuthority SubjectAuthority;
	FSightWeaveSubjectHandle StalkerSubjectHandle;
	FSightWeaveSubjectRegistration StalkerSubjectRegistration;
	FSightWeaveVisionSourceDescription BodyDescription;
	FSightWeaveVisionSourceDescription ConeDescription;
	FSightWeaveIlluminationSourceDescription TorchDescription;
	TMap<FName, FDarkwellVisibilitySubjectSnapshot> SubjectSnapshots;
	FDarkwellVisibilityAuthorityDiagnostics Diagnostics;
	double RequestAgeSeconds = 0.0;
	uint64 NextAuthorityRevision = 1;
	uint64 NextObservationRevision = 1;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bool bDiagnosticProofCameraActive = false;
	bool bDiagnosticSubjectControllerSuppressed = false;
	bool bDiagnosticLastSubjectStateValid = false;
	bool bDiagnosticLastSubjectHardLive = false;
	double DiagnosticToolCycleElapsedSeconds = 0.0;
	int32 LastDiagnosticToolCyclePhase = INDEX_NONE;
	int32 DiagnosticCoverageReadbackFrameCount = 0;
	bool bDiagnosticCoverageReadbackComplete = false;
#endif
};
