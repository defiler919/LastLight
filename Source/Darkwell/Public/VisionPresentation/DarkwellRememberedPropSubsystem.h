// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DarkwellRememberedPropSubsystem.generated.h"

class AActor;
class UDarkwellRememberablePropComponent;
class UStaticMesh;

struct DARKWELL_API FDarkwellRememberedPropDecision
{
	bool bCurrentLive = false;
	bool bSnapshotValid = false;
	bool bShowCurrent = false;
	bool bShowProxy = false;
	bool bSnapshotChanged = false;
	bool bRetainPreviousSnapshot = false;
};

/** Pure policy used by runtime and deterministic A->B contract tests. */
struct DARKWELL_API FDarkwellRememberedPropState
{
	static constexpr float EnterCoverage = 0.50f;
	static constexpr float ExitCoverage = 0.25f;

	bool bWasLive = false;
	bool bSnapshotValid = false;
	FTransform SnapshotTransform = FTransform::Identity;
	uint64 AppearanceRevision = 0;

	void Initialize(const FTransform& InitialTransform, uint64 InitialAppearanceRevision);
	FDarkwellRememberedPropDecision Observe(
		bool bCurrentExists,
		const FTransform& CurrentTransform,
		float CurrentMaximumCoverage,
		float SnapshotMaximumCoverage,
		uint64 CurrentAppearanceRevision,
		bool bVerifyOldLocation = false);
	static bool ResolveObjectLive(bool bPreviouslyLive, float MaximumCoverage);
};

struct DARKWELL_API FDarkwellRememberedPropDiagnostics
{
	int32 RegisteredCount = 0;
	int32 LiveCount = 0;
	int32 ProxyCount = 0;
	int32 RetainedDestroyedCount = 0;
	uint64 SnapshotRevision = 0;
	uint64 DuplicateStableIdRejectCount = 0;
};

/** DARKWELL-only last-observed memory presentation; it never changes world authority. */
UCLASS()
class DARKWELL_API UDarkwellRememberedPropSubsystem final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }

	bool RegisterProp(UDarkwellRememberablePropComponent* Component);
	void UnregisterProp(
		UDarkwellRememberablePropComponent* Component,
		EEndPlayReason::Type EndPlayReason);
	void RefreshNowForTesting();
	bool TryGetRecordForTesting(
		FName StableId,
		bool& bOutCurrentLive,
		bool& bOutSnapshotValid,
		FVector& OutSnapshotLocation,
		AActor*& OutProxyActor) const;
	const FDarkwellRememberedPropDiagnostics& GetDiagnostics() const
	{
		return Diagnostics;
	}
	int32 GetUnverifiedSnapshotCount(FName StableId) const;
	// Explicit dedicated-lab opt-in only; the ordinary policy path is untouched.
	bool SetLabVerificationSubject(FName StableId);
	AActor* FreezeLabVerificationSnapshot();
	void FinishLabVerificationSnapshot();
	void ReleaseLabVerificationSubject();

private:
	struct FPrimitiveSnapshot
	{
		TSoftObjectPtr<UStaticMesh> Mesh;
		FTransform WorldTransform = FTransform::Identity;
	};

	struct FRecord
	{
		TWeakObjectPtr<UDarkwellRememberablePropComponent> Component;
		TWeakObjectPtr<AActor> ProxyActor;
		TArray<TWeakObjectPtr<AActor>> UnverifiedProxies;
		FDarkwellRememberedPropState State;
		TArray<FPrimitiveSnapshot> Primitives;
		FLinearColor Tint = FLinearColor::Gray;
		float UVScale = 5.0f;
		bool bDiagnosticStateValid = false;
		bool bDiagnosticLastLive = false;
		bool bDiagnosticLastProxy = false;
		bool bDiagnosticLastSnapshotValid = false;
	};

	void RefreshRecords();
	float EvaluateMaximumCoverage(
		const UDarkwellRememberablePropComponent& Component) const;
	float EvaluateMaximumSnapshotCoverage(const FRecord& Record) const;
	void CaptureSnapshot(FRecord& Record, UDarkwellRememberablePropComponent& Component);
	void RebuildProxy(FName StableId, FRecord& Record);
	void DestroyProxy(FRecord& Record);

	TMap<FName, FRecord> Records;
	FName LabVerificationSubject;
	bool bLabSnapshotFrozen = false;
	FDarkwellRememberedPropDiagnostics Diagnostics;
};
