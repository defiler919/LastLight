#pragma once

#include "CoreMinimal.h"
#include "SightWeaveMemory.h"
#include "UObject/SoftObjectPath.h"

#include "SightWeaveSubjectMemory.generated.h"

enum class ESightWeavePersistenceProviderResult : uint8;
struct FSightWeaveProviderPayloadRecord;

/** Owned provider staging data. It may not mutate formal provider state before commit. */
class SIGHTWEAVERUNTIME_API ISightWeavePersistencePreparedPayload
{
public:
	virtual ~ISightWeavePersistencePreparedPayload() = default;
};

UENUM(BlueprintType)
enum class ESightWeaveSubjectMemoryPolicy : uint8
{
	NeverRemember,
	VisibleOnly,
	StaticEnvironment,
	LastSeenSnapshot,
	Custom
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveSubjectHandle
{
	GENERATED_BODY()

public:
	FSightWeaveSubjectHandle() = default;
	explicit FSightWeaveSubjectHandle(const int64 InValue) : Value(InValue) {}

	bool IsValid() const { return Value > 0; }
	int64 GetValue() const { return Value; }

	friend bool operator==(const FSightWeaveSubjectHandle& A, const FSightWeaveSubjectHandle& B)
	{
		return A.Value == B.Value;
	}
	friend bool operator!=(const FSightWeaveSubjectHandle& A, const FSightWeaveSubjectHandle& B)
	{
		return !(A == B);
	}
	friend uint32 GetTypeHash(const FSightWeaveSubjectHandle& Handle)
	{
		return GetTypeHash(Handle.Value);
	}

private:
	UPROPERTY(VisibleAnywhere, Category = "SightWeave")
	int64 Value = 0;
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveSubjectIdentity
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	FName StableId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave", meta = (ClampMin = "1"))
	int64 InstanceGeneration = 0;

	bool IsValid() const { return !StableId.IsNone() && InstanceGeneration > 0; }
	bool IsEquivalentTo(const FSightWeaveSubjectIdentity& Other) const
	{
		return StableId == Other.StableId
			&& InstanceGeneration == Other.InstanceGeneration;
	}
};

enum class ESightWeaveSubjectCaptureReason : uint8
{
	None,
	LiveToNonLive
};

enum class ESightWeaveSubjectSnapshotValidity : uint32
{
	None = 0,
	Identity = 1u << 0,
	Scope = 1u << 1,
	Transform = 1u << 2,
	Bounds = 1u << 3,
	OpaqueStaticMesh = 1u << 4,
	StableMaterials = 1u << 5,
	Transition = 1u << 6
};
ENUM_CLASS_FLAGS(ESightWeaveSubjectSnapshotValidity);

namespace SightWeave::SubjectMemory
{
	inline constexpr ESightWeaveSubjectSnapshotValidity RequiredBasicSnapshotValidity =
		ESightWeaveSubjectSnapshotValidity::Identity
		| ESightWeaveSubjectSnapshotValidity::Scope
		| ESightWeaveSubjectSnapshotValidity::Transform
		| ESightWeaveSubjectSnapshotValidity::Bounds
		| ESightWeaveSubjectSnapshotValidity::OpaqueStaticMesh
		| ESightWeaveSubjectSnapshotValidity::StableMaterials
		| ESightWeaveSubjectSnapshotValidity::Transition;
}

struct SIGHTWEAVERUNTIME_API FSightWeaveBasicStaticMeshSnapshotCandidate
{
	FTransform WorldTransform = FTransform::Identity;
	FBox WorldBounds = FBox(ForceInit);
	FSoftObjectPath StaticMeshAsset;
	TArray<FSoftObjectPath> MaterialOverrides;
	FName VisualVariantId = NAME_None;
	bool bOpaqueStaticMesh = false;
	bool bHasDynamicMaterial = false;
	bool bHasUnsupportedComponents = false;

	bool IsValid() const;
	bool IsEquivalentTo(const FSightWeaveBasicStaticMeshSnapshotCandidate& Other) const;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSubjectRegistration
{
	FSightWeaveSubjectIdentity Identity;
	FSightWeaveMemoryScopeKey Scope;
	ESightWeaveSubjectMemoryPolicy Policy = ESightWeaveSubjectMemoryPolicy::NeverRemember;
	FName CustomProviderName = NAME_None;
	uint32 CustomProviderVersion = 0;

	bool IsValid() const;
	bool IsEquivalentTo(const FSightWeaveSubjectRegistration& Other) const;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveLastSeenSnapshotDescriptor
{
	FSightWeaveSubjectIdentity Identity;
	FSightWeaveMemoryScopeKey Scope;
	ESightWeaveSubjectMemoryPolicy Policy = ESightWeaveSubjectMemoryPolicy::NeverRemember;
	uint64 SnapshotRevision = 0;
	uint64 EligibilityRevision = 0;
	uint64 SourceLiveRevision = 0;
	FTransform WorldTransform = FTransform::Identity;
	FBox WorldBounds = FBox(ForceInit);
	FSoftObjectPath StaticMeshAsset;
	TArray<FSoftObjectPath> MaterialOverrides;
	FName VisualVariantId = NAME_None;
	ESightWeaveSubjectCaptureReason CaptureReason = ESightWeaveSubjectCaptureReason::None;
	uint64 CaptureTransitionIdentity = 0;
	ESightWeaveSubjectSnapshotValidity Validity = ESightWeaveSubjectSnapshotValidity::None;

	bool IsValid() const;
	bool IsEquivalentTo(const FSightWeaveLastSeenSnapshotDescriptor& Other) const;
};

enum class ESightWeaveSubjectTransitionFailure : uint8
{
	None,
	NotConfigured,
	InvalidHandle,
	InvalidRegistration,
	InvalidObservation,
	IdentityMismatch,
	ScopeMismatch,
	StaleObservation,
	StaleSourceLiveRevision,
	InvalidTransition,
	DuplicateTransition,
	NotMemoryEligible,
	UnsupportedSubject,
	MissingCustomProvider,
	CustomProviderMismatch,
	CustomProviderRejected,
	InvalidCustomProviderResult
};

enum class ESightWeaveSubjectTransitionDisposition : uint8
{
	Rejected,
	LiveAccepted,
	NonLiveAccepted,
	SnapshotCaptured
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSubjectObservation
{
	FSightWeaveSubjectIdentity Identity;
	FSightWeaveMemoryScopeKey Scope;
	uint64 ObservationRevision = 0;
	uint64 EligibilityRevision = 0;
	uint64 SourceLiveRevision = 0;
	uint64 TransitionIdentity = 0;
	bool bHardLive = false;
	bool bEligibleForMemoryWrite = false;
	FSightWeaveBasicStaticMeshSnapshotCandidate BasicSnapshot;

	bool HasValidHeader() const;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSubjectTransitionResult
{
	ESightWeaveSubjectTransitionFailure Failure =
		ESightWeaveSubjectTransitionFailure::None;
	ESightWeaveSubjectTransitionDisposition Disposition =
		ESightWeaveSubjectTransitionDisposition::Rejected;
	uint64 PriorSnapshotRevision = 0;
	uint64 SnapshotRevision = 0;
	bool bProxyMustHide = true;

	bool Succeeded() const
	{
		return Failure == ESightWeaveSubjectTransitionFailure::None;
	}
};

/**
 * Synchronous, non-owning host extension used only at a Custom policy falling edge.
 * The authority never retains this object or any gameplay/render object referenced by it.
 */
class SIGHTWEAVERUNTIME_API ISightWeaveSubjectSnapshotProvider
{
public:
	virtual ~ISightWeaveSubjectSnapshotProvider() = default;

	virtual FName GetSightWeaveProviderName() const = 0;
	virtual uint32 GetSightWeaveProviderVersion() const = 0;
	virtual bool BuildSightWeaveSnapshotCandidate(
		const FSightWeaveSubjectRegistration& Registration,
		const FSightWeaveSubjectObservation& FallingEdgeObservation,
		FSightWeaveBasicStaticMeshSnapshotCandidate& OutCandidate) const = 0;

	/** Optional M4P3 persistence extension; existing falling-edge providers remain source-compatible. */
	virtual bool SupportsSightWeavePersistence() const;
	virtual ESightWeavePersistenceProviderResult CaptureSightWeavePersistence(
		TArray<FSightWeaveProviderPayloadRecord>& OutPayloads) const;
	virtual ESightWeavePersistenceProviderResult PrepareSightWeavePersistence(
		const FSightWeaveProviderPayloadRecord& Payload,
		TUniquePtr<ISightWeavePersistencePreparedPayload>& OutPrepared) const;
	/** Contractually infallible after a successful prepare. */
	virtual void CommitSightWeavePersistence(
		TUniquePtr<ISightWeavePersistencePreparedPayload>&& Prepared);
};

enum class ESightWeaveSubjectPresentationState : uint8
{
	Hidden,
	Live,
	LastSeenProxy,
	StaticEnvironmentDelegated
};

enum class ESightWeaveSubjectPresentationFailure : uint8
{
	None,
	InvalidHandle,
	IdentityMismatch,
	ScopeMismatch,
	MissingSnapshot,
	InvalidSnapshot,
	SnapshotRevisionMismatch,
	EligibilityRevisionMismatch,
	SourceLiveRevisionMismatch,
	UnknownMemory,
	PresentationSuppressed,
	UnsupportedPolicy
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSubjectPresentationContext
{
	FSightWeaveSubjectIdentity Identity;
	FSightWeaveMemoryScopeKey Scope;
	uint64 SnapshotRevision = 0;
	uint64 EligibilityRevision = 0;
	uint64 SourceLiveRevision = 0;
	bool bHardLive = false;
	bool bHardMemoryAtSnapshot = false;
	bool bBlockMemoryWrites = false;
	bool bSuppressMemoryPresentation = false;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSubjectPresentationResult
{
	ESightWeaveSubjectPresentationState State =
		ESightWeaveSubjectPresentationState::Hidden;
	ESightWeaveSubjectPresentationFailure Failure =
		ESightWeaveSubjectPresentationFailure::None;
	uint64 SnapshotRevision = 0;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSubjectPersistentStateRecord
{
	FSightWeaveSubjectRegistration Registration;
	TOptional<FSightWeaveLastSeenSnapshotDescriptor> Snapshot;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSubjectPersistentState
{
	FSightWeaveMemoryScopeKey Scope;
	TArray<FSightWeaveSubjectPersistentStateRecord> Records;
};

/** Game-thread-only deterministic CPU authority. It owns no Actor or render object. */
class SIGHTWEAVERUNTIME_API FSightWeaveSubjectMemoryAuthority final
{
public:
	FSightWeaveSubjectHandle Register(const FSightWeaveSubjectRegistration& Registration);
	bool Update(
		FSightWeaveSubjectHandle Handle,
		const FSightWeaveSubjectRegistration& Registration);
	bool Unregister(FSightWeaveSubjectHandle Handle);
	bool IsHandleValid(FSightWeaveSubjectHandle Handle) const;
	void Reset();

	FSightWeaveSubjectTransitionResult SubmitObservation(
		FSightWeaveSubjectHandle Handle,
		const FSightWeaveSubjectObservation& Observation,
		const ISightWeaveSubjectSnapshotProvider* CustomProvider = nullptr);
	FSightWeaveSubjectPresentationResult EvaluatePresentation(
		FSightWeaveSubjectHandle Handle,
		const FSightWeaveSubjectPresentationContext& Context) const;
	int32 ClearSnapshots(const FSightWeaveMemoryRegion& Region);

	const FSightWeaveLastSeenSnapshotDescriptor* FindSnapshot(
		FSightWeaveSubjectHandle Handle) const;
	int32 GetSubjectCount() const { return Records.Num(); }
	int32 GetSnapshotCount() const;
	uint64 GetPersistenceGuardRevision() const { return PersistenceGuardRevision; }
	bool ExportPersistentState(
		const FSightWeaveMemoryScopeKey& Scope,
		FSightWeaveSubjectPersistentState& OutState) const;
	/** Mutates only the receiving staging copy; commit is a later move assignment. */
	bool PreparePersistentReplacement(const FSightWeaveSubjectPersistentState& State);
	void FinalizePreparedPersistentReplacement(uint64 PriorGuardRevision);
	FSightWeaveSubjectHandle FindHandleByIdentity(
		const FSightWeaveSubjectIdentity& Identity) const;

	static bool DoesSnapshotMatchRegistration(
		const FSightWeaveLastSeenSnapshotDescriptor& Snapshot,
		const FSightWeaveSubjectRegistration& Registration);

private:
	struct FRecord
	{
		FSightWeaveSubjectHandle Handle;
		FSightWeaveSubjectRegistration Registration;
		TOptional<FSightWeaveLastSeenSnapshotDescriptor> Snapshot;
		FSightWeaveBasicStaticMeshSnapshotCandidate LastLiveCandidate;
		uint64 LastObservationRevision = 0;
		uint64 LastLiveEligibilityRevision = 0;
		uint64 LastLiveSourceRevision = 0;
		uint64 LastConsumedTransitionIdentity = 0;
		uint64 NextSnapshotRevision = 1;
		bool bHasObservation = false;
		bool bWasHardLive = false;
		bool bLastLiveEligibleForMemoryWrite = false;
		bool bHasValidLastLiveCandidate = false;
	};

	FRecord* FindRecord(FSightWeaveSubjectHandle Handle);
	const FRecord* FindRecord(FSightWeaveSubjectHandle Handle) const;

	TArray<FRecord> Records;
	int64 NextHandle = 1;
	uint64 PersistenceGuardRevision = 0;
};
