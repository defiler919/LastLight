#pragma once

#include "CoreMinimal.h"
#include "SightWeaveSubjectMemory.h"

class AActor;
class UPrimitiveComponent;
class USightWeaveLastSeenProxyComponent;
class USightWeaveVisionSourceComponent;
class UWorld;

/** PIE-only, transient M4.1 visual fixture. It never owns gameplay authority. */
class FSightWeaveM4P1LabFixture final
{
public:
	~FSightWeaveM4P1LabFixture();

	bool Initialize(UWorld* InWorld);
	bool ApplyState(int32 InState);
	void Tick();
	void Shutdown();

	bool IsReady() const { return bReady; }
	int32 GetAppliedState() const { return AppliedState; }
	int32 GetVisibleProxyCount() const { return VisibleProxyCount; }
	int32 GetVisibleLiveCount() const { return VisibleLiveCount; }
	int32 GetRetainedSnapshotCount() const { return Authority.GetSnapshotCount(); }

private:
	bool BuildCameras();
	bool RebuildSubjects(int32 InState);
	bool ApplyPrimaryState(int32 InState);
	bool ResetPrimaryRegistration();
	void DestroyActors(TArray<TWeakObjectPtr<AActor>>& Actors);

	TWeakObjectPtr<UWorld> World;
	TArray<TWeakObjectPtr<AActor>> CameraActors;
	TArray<TWeakObjectPtr<AActor>> SubjectActors;
	FSightWeaveSubjectMemoryAuthority Authority;
	FSightWeaveMemoryScopeKey Scope;
	TWeakObjectPtr<USightWeaveVisionSourceComponent> StaticEnvironmentCaptureSource;
	TWeakObjectPtr<USightWeaveVisionSourceComponent> PresentationScopeAnchorSource;
	TWeakObjectPtr<UPrimitiveComponent> PrimaryLivePresentation;
	TWeakObjectPtr<USightWeaveLastSeenProxyComponent> PrimaryProxyPresentation;
	TWeakObjectPtr<USightWeaveVisionSourceComponent> PrimaryLiveSource;
	FSightWeaveSubjectRegistration PrimaryRegistration;
	FSightWeaveBasicStaticMeshSnapshotCandidate PrimaryCandidate;
	FSightWeaveSubjectHandle PrimaryHandle;
	FVector StaticEnvironmentSampleLocation = FVector::ZeroVector;
	uint64 PrimaryObservationRevision = 1;
	uint64 PrimaryTransitionIdentity = 1001;
	bool bPrimaryHardLive = false;
	int32 AppliedState = INDEX_NONE;
	int32 VisibleProxyCount = 0;
	int32 VisibleLiveCount = 0;
	bool bReady = false;
};
