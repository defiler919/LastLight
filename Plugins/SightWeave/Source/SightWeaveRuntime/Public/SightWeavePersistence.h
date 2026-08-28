#pragma once

#include "CoreMinimal.h"
#include "SightWeaveSubjectMemory.h"

namespace SightWeave::Persistence
{
	inline constexpr uint16 FormatVersion = 1;
	inline constexpr uint16 HeaderBytes = 80;
	inline constexpr int64 DefaultMaximumCanonicalBytes = 64ll * 1024ll * 1024ll;
	inline constexpr int64 DefaultMaximumStoredBlobBytes = 64ll * 1024ll * 1024ll;
	inline constexpr int32 CompressionThresholdBytes = 4096;
	inline constexpr uint32 MaximumCollectionEntries = 1u << 20;
}

enum class ESightWeaveSnapshotCompressionMethod : uint8
{
	None = 0,
	Zlib = 1
};

enum class ESightWeaveSnapshotResult : uint8
{
	Succeeded,
	SucceededWithProviderFallback,
	EmptyBlob,
	Truncated,
	InvalidMagic,
	UnsupportedLegacyVersion,
	FutureVersion,
	InvalidHeader,
	InvalidFlags,
	InvalidCompressionMethod,
	SizeOverflow,
	SizeLimitExceeded,
	SizeMismatch,
	CompressionFailed,
	DecompressionFailed,
	ChecksumMismatch,
	PayloadMalformed,
	TrailingPayload,
	InvalidScope,
	MissingTargetScope,
	DuplicateScope,
	DuplicateTile,
	InvalidTile,
	DuplicateModifierId,
	InvalidPersistentModifier,
	DuplicateSubjectId,
	InvalidSubject,
	InvalidReference,
	DuplicateProviderId,
	InvalidProviderPayload,
	ProviderCaptureFailed,
	ProviderVersionMismatch,
	ProviderPrepareFailed,
	TargetChanged,
	WorldTornDown,
	CommitInvariantFailed
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSnapshotLimits
{
	int64 MaximumCanonicalBytes = SightWeave::Persistence::DefaultMaximumCanonicalBytes;
	int64 MaximumStoredBlobBytes = SightWeave::Persistence::DefaultMaximumStoredBlobBytes;

	bool IsValid() const;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSnapshotBlob
{
	TArray<uint8> Bytes;

	bool IsEmpty() const { return Bytes.IsEmpty(); }
	int64 Num() const { return Bytes.Num(); }
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSnapshotDiagnostic
{
	ESightWeaveSnapshotResult Result = ESightWeaveSnapshotResult::PayloadMalformed;
	ESightWeaveSnapshotCompressionMethod CompressionMethod =
		ESightWeaveSnapshotCompressionMethod::None;
	uint16 FormatVersion = 0;
	int64 FailureOffset = INDEX_NONE;
	int64 CanonicalBytes = 0;
	int64 StoredBytes = 0;
	uint32 ScopeCount = 0;
	uint32 ProviderPayloadCount = 0;
	FName PrimaryId = NAME_None;
	FString Detail;

	bool Succeeded() const
	{
		return Result == ESightWeaveSnapshotResult::Succeeded
			|| Result == ESightWeaveSnapshotResult::SucceededWithProviderFallback;
	}
};

struct SIGHTWEAVERUNTIME_API FSightWeavePersistentScopeKey
{
	FName StableScopeId = NAME_None;
	FSightWeaveKnowledgeOwnerId KnowledgeOwnerId;
	FSightWeaveFloorId FloorId;
	FVector2D FloorOrigin = FVector2D::ZeroVector;
	float FloorPlaneZ = 0.0f;
	ESightWeaveRenderPrecisionTier PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
	TArray<FSightWeaveRenderProfileIdentity> CanonicalProfiles;

	bool IsValid() const;
};

struct SIGHTWEAVERUNTIME_API FSightWeavePersistentRegion
{
	FSightWeaveHeightRange HeightRange;
	ESightWeaveMemoryRegionShape Shape = ESightWeaveMemoryRegionShape::Circle;
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D HalfExtents = FVector2D(100.0, 100.0);
	float Radius = 100.0f;
	float RotationDegrees = 0.0f;
	TArray<FVector2D> PolygonVertices;
	bool bEnabled = true;

	bool IsValid() const;
};

struct SIGHTWEAVERUNTIME_API FSightWeavePersistentModifierRecord
{
	FName StableId = NAME_None;
	ESightWeaveMemoryModifierOperation Operation =
		ESightWeaveMemoryModifierOperation::BlockMemoryWrites;
	FSightWeavePersistentRegion Region;

	bool IsValid() const;
};

struct SIGHTWEAVERUNTIME_API FSightWeavePersistentLastSeenRecord
{
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
	ESightWeaveSubjectSnapshotValidity Validity =
		ESightWeaveSubjectSnapshotValidity::None;

	bool IsValid() const;
};

struct SIGHTWEAVERUNTIME_API FSightWeavePersistentSubjectRecord
{
	FSightWeaveSubjectIdentity Identity;
	ESightWeaveSubjectMemoryPolicy Policy = ESightWeaveSubjectMemoryPolicy::NeverRemember;
	FName CustomProviderId = NAME_None;
	uint32 CustomProviderVersion = 0;
	TOptional<FSightWeavePersistentLastSeenRecord> LastSeen;

	bool IsValid() const;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSnapshotScopeRecord
{
	FSightWeavePersistentScopeKey Scope;
	TArray<FSightWeavePackedMemoryTile> MemoryTiles;
	TArray<FSightWeavePersistentModifierRecord> PersistentModifiers;
	TArray<FSightWeavePersistentSubjectRecord> Subjects;

	bool IsValid() const;
};

enum class ESightWeaveProviderDomainType : uint8
{
	Subject,
	Region,
	Semantic
};

struct SIGHTWEAVERUNTIME_API FSightWeaveProviderDomain
{
	ESightWeaveProviderDomainType Type = ESightWeaveProviderDomainType::Semantic;
	FName StableScopeId = NAME_None;
	FSightWeaveSubjectIdentity SubjectIdentity;
	FSightWeavePersistentRegion Region;
	FName SemanticDomainId = NAME_None;

	bool IsValid() const;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveProviderPayloadRecord
{
	FName ProviderId = NAME_None;
	uint32 SchemaVersion = 0;
	FSightWeaveProviderDomain Domain;
	TArray<uint8> Payload;

	bool IsValid() const;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveCanonicalSnapshot
{
	TArray<FSightWeaveSnapshotScopeRecord> Scopes;
	TArray<FSightWeaveProviderPayloadRecord> ProviderPayloads;
};

/** Deterministic, bounded V1 codec. It owns no file, slot, UObject, or RHI behavior. */
class SIGHTWEAVERUNTIME_API FSightWeavePersistence final
{
public:
	static FSightWeaveSnapshotDiagnostic BuildBlob(
		const FSightWeaveCanonicalSnapshot& Snapshot,
		FSightWeaveSnapshotBlob& OutBlob,
		const FSightWeaveSnapshotLimits& Limits = FSightWeaveSnapshotLimits());

	static FSightWeaveSnapshotDiagnostic ParseBlob(
		const FSightWeaveSnapshotBlob& Blob,
		FSightWeaveCanonicalSnapshot& OutSnapshot,
		const FSightWeaveSnapshotLimits& Limits = FSightWeaveSnapshotLimits());
};
