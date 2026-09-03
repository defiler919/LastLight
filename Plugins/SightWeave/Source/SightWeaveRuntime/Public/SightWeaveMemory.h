#pragma once

#include "CoreMinimal.h"
#include "SightWeaveQueries.h"
#include "SightWeaveSparseAtlas.h"

namespace SightWeave::Memory
{
	inline constexpr int32 InteriorTileSize = SightWeave::SparseAtlas::InteriorTileSize;
	inline constexpr int32 RowBytes = InteriorTileSize / 8;
	inline constexpr int32 PackedBytesPerTile = RowBytes * InteriorTileSize;
	inline constexpr int32 DefaultMaximumTiles = 256;
}

enum class ESightWeaveMemoryFailure : uint8
{
	None,
	NotConfigured,
	InvalidScope,
	InvalidSnapshot,
	StaleSnapshot,
	ScopeMismatch,
	ProfileMismatch,
	InvalidCoordinate,
	CapacityExceeded,
	InvalidPersistentState,
	DuplicatePersistentModifier
};

struct SIGHTWEAVERUNTIME_API FSightWeaveMemoryModifierHandle
{
	FSightWeaveMemoryModifierHandle() = default;
	explicit FSightWeaveMemoryModifierHandle(const int64 InValue) : Value(InValue) {}

	bool IsValid() const { return Value > 0; }
	int64 GetValue() const { return Value; }
	friend bool operator==(const FSightWeaveMemoryModifierHandle& A, const FSightWeaveMemoryModifierHandle& B)
	{
		return A.Value == B.Value;
	}

private:
	int64 Value = 0;
};

UENUM(BlueprintType)
enum class ESightWeaveMemoryRegionShape : uint8
{
	Circle,
	AxisAlignedBox,
	RotatedBox,
	Polygon
};

UENUM(BlueprintType)
enum class ESightWeaveMemoryModifierOperation : uint8
{
	BlockMemoryWrites,
	SuppressMemoryPresentation
};

/** Snapshot participation is always opt-in. Existing modifiers remain transient. */
UENUM(BlueprintType)
enum class ESightWeaveMemoryModifierPersistence : uint8
{
	Transient,
	Persistent
};

struct SIGHTWEAVERUNTIME_API FSightWeaveMemoryScopeKey
{
	FSightWeaveRenderWorldIdentity WorldIdentity;
	uint64 WorldGeneration = 0;
	FSightWeaveKnowledgeOwnerId KnowledgeOwnerId;
	FSightWeaveFloorId FloorId;
	FVector2D FloorOrigin = FVector2D::ZeroVector;
	float FloorPlaneZ = 0.0f;
	ESightWeaveRenderPrecisionTier PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
	TArray<FSightWeaveRenderProfileIdentity> CanonicalProfiles;

	bool IsValid() const;
	bool IsEquivalentTo(const FSightWeaveMemoryScopeKey& Other) const;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveMemoryTileKey
{
	FSightWeaveMemoryScopeKey Scope;
	FIntPoint LogicalCoordinate = FIntPoint::ZeroValue;

	bool IsValid() const { return Scope.IsValid(); }
	bool IsEquivalentTo(const FSightWeaveMemoryTileKey& Other) const;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveMemoryRegion
{
	FSightWeaveMemoryScopeKey Scope;
	FSightWeaveHeightRange HeightRange;
	ESightWeaveMemoryRegionShape Shape = ESightWeaveMemoryRegionShape::Circle;
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D HalfExtents = FVector2D(100.0, 100.0);
	float Radius = 100.0f;
	float RotationDegrees = 0.0f;
	TArray<FVector2D> PolygonVertices;
	bool bEnabled = true;

	bool IsValid() const;
	bool IsEquivalentTo(const FSightWeaveMemoryRegion& Other) const;
	bool ContainsWorldLocation(FVector WorldLocation) const;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveMemoryModifierDescription
{
	ESightWeaveMemoryModifierOperation Operation =
		ESightWeaveMemoryModifierOperation::BlockMemoryWrites;
	FSightWeaveMemoryRegion Region;
	ESightWeaveMemoryModifierPersistence Persistence =
		ESightWeaveMemoryModifierPersistence::Transient;
	FName StablePersistenceId = NAME_None;

	bool IsValid() const { return Region.IsValid(); }
	bool HasValidPersistenceMetadata() const
	{
		return Persistence == ESightWeaveMemoryModifierPersistence::Transient
			|| (Persistence == ESightWeaveMemoryModifierPersistence::Persistent
				&& !StablePersistenceId.IsNone());
	}
	bool IsEquivalentTo(const FSightWeaveMemoryModifierDescription& Other) const;
};

struct SIGHTWEAVERUNTIME_API FSightWeavePackedMemoryTile
{
	FSightWeaveMemoryTileKey Key;
	TArray<uint8> PackedBits;

	bool IsValid() const;
	bool IsEmpty() const;
	bool TestBit(FIntPoint InteriorTexel) const;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveMemoryUpdateDiagnostics
{
	ESightWeaveMemoryFailure Failure = ESightWeaveMemoryFailure::None;
	uint64 PriorMemoryRevision = 0;
	uint64 MemoryRevision = 0;
	uint64 SnapshotRevision = 0;
	int32 CandidateTileCount = 0;
	int32 AllocatedTileCount = 0;
	int32 ChangedTileCount = 0;
	int32 DirtyTileCount = 0;
	int64 PackedAuthorityBytes = 0;
	bool bAuthorityChanged = false;
	bool bDuplicateSnapshot = false;

	bool Succeeded() const { return Failure == ESightWeaveMemoryFailure::None; }
};

/** Plain-data durable state used to prepare a replacement authority off to the side. */
struct SIGHTWEAVERUNTIME_API FSightWeaveMemoryPersistentState
{
	FSightWeaveMemoryScopeKey Scope;
	TArray<FSightWeavePackedMemoryTile> Tiles;
	TArray<FSightWeaveMemoryModifierDescription> PersistentModifiers;
};

/**
 * Immutable owned CPU-authority publication. The render module may mirror this
 * packet, but cannot mutate the authority that produced it.
 */
class SIGHTWEAVERUNTIME_API FSightWeaveMemoryPacket final
{
public:
	bool IsValid() const { return bValid; }
	ESightWeaveMemoryFailure GetFailure() const { return Failure; }
	const FSightWeaveMemoryScopeKey& GetScope() const { return Scope; }
	uint64 GetMemoryRevision() const { return MemoryRevision; }
	uint64 GetSnapshotRevision() const { return SnapshotRevision; }
	uint64 GetPacketRevision() const { return PacketRevision; }
	uint64 GetModifierRevision() const { return ModifierRevision; }
	bool IsFullRebuild() const { return bFullRebuild; }
	TConstArrayView<FSightWeavePackedMemoryTile> GetDirtyTiles() const { return DirtyTiles; }
	TConstArrayView<FSightWeavePackedMemoryTile> GetAuthorityTiles() const
	{
		return AuthorityTiles.IsValid()
			? MakeArrayView(*AuthorityTiles)
			: TConstArrayView<FSightWeavePackedMemoryTile>();
	}
	TConstArrayView<FSightWeaveMemoryTileKey> GetRemovedTiles() const { return RemovedTiles; }
	TConstArrayView<FSightWeaveMemoryModifierDescription> GetPresentationSuppressions() const
	{
		return PresentationSuppressions;
	}
	int64 GetPackedAuthorityBytes() const { return PackedAuthorityBytes; }
	bool HasMirrorWork() const
	{
		return bFullRebuild || bModifierStateChanged
			|| !DirtyTiles.IsEmpty() || !RemovedTiles.IsEmpty();
	}

private:
	friend class FSightWeaveMemoryAuthority;

	bool bValid = false;
	ESightWeaveMemoryFailure Failure = ESightWeaveMemoryFailure::NotConfigured;
	FSightWeaveMemoryScopeKey Scope;
	uint64 MemoryRevision = 0;
	uint64 SnapshotRevision = 0;
	uint64 PacketRevision = 0;
	uint64 ModifierRevision = 0;
	int64 PackedAuthorityBytes = 0;
	bool bFullRebuild = false;
	bool bModifierStateChanged = false;
	TArray<FSightWeavePackedMemoryTile> DirtyTiles;
	TArray<FSightWeaveMemoryTileKey> RemovedTiles;
	TArray<FSightWeaveMemoryModifierDescription> PresentationSuppressions;
	TSharedPtr<const TArray<FSightWeavePackedMemoryTile>, ESPMode::ThreadSafe> AuthorityTiles;
};

/**
 * Game-thread-only packed HardMemory authority. It consumes immutable CPU
 * visibility snapshots and has no camera, viewport, Scene Color, or RHI input.
 */
class SIGHTWEAVERUNTIME_API FSightWeaveMemoryAuthority final
{
public:
	bool Configure(const FSightWeaveMemoryScopeKey& InScope, int32 InMaximumTiles);
	void Reset();

	bool IsConfigured() const { return bConfigured; }
	const FSightWeaveMemoryScopeKey& GetScope() const { return Scope; }
	uint64 GetMemoryRevision() const { return MemoryRevision; }
	uint64 GetLastSnapshotRevision() const { return LastSnapshotRevision; }
	uint64 GetModifierRevision() const { return ModifierRevision; }
	uint64 GetPersistenceGuardRevision() const { return PersistenceGuardRevision; }
	int32 GetMaximumTiles() const { return MaximumTiles; }
	int32 GetAllocatedTileCount() const { return Tiles.Num(); }
	int64 GetPackedAuthorityBytes() const;
	ESightWeaveMemoryFailure GetLastFailure() const { return LastFailure; }

	FSightWeaveMemoryUpdateDiagnostics WriteEffectiveLive(const FSightWeaveFrameSnapshot& Snapshot);
	bool ClearMemory(const FSightWeaveMemoryRegion& Region);
	FSightWeaveMemoryModifierHandle RegisterModifier(
		const FSightWeaveMemoryModifierDescription& Description);
	bool UpdateModifier(
		FSightWeaveMemoryModifierHandle Handle,
		const FSightWeaveMemoryModifierDescription& Description);
	bool UnregisterModifier(FSightWeaveMemoryModifierHandle Handle);
	bool IsModifierHandleValid(FSightWeaveMemoryModifierHandle Handle) const;
	bool IsMemoryPresentationSuppressed(FVector WorldLocation) const;
	int32 GetModifierCount() const { return Modifiers.Num(); }
	bool QueryHardMemory(FVector WorldLocation) const;
	bool QueryHardMemory2D(FVector2D WorldLocation) const;
	ESightWeaveMemoryFailure ExportPersistentState(
		FSightWeaveMemoryPersistentState& OutState) const;
	/** Mutates only the receiving staging copy; commit is a later move assignment. */
	ESightWeaveMemoryFailure PreparePersistentReplacement(
		const FSightWeaveMemoryPersistentState& State);
	void FinalizePreparedPersistentReplacement(
		uint64 PriorMemoryRevision,
		uint64 PriorModifierRevision,
		uint64 PriorGuardRevision);
	TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> PublishPacket(bool bForceFullRebuild = false);

	static bool BuildScopeForSnapshot(
		const FSightWeaveFrameSnapshot& Snapshot,
		FSightWeaveRenderWorldIdentity WorldIdentity,
		uint64 WorldGeneration,
		FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
		FSightWeaveFloorId FloorId,
		ESightWeaveRenderPrecisionTier PrecisionTier,
		FSightWeaveMemoryScopeKey& OutScope);
	static bool WorldToTileAndTexel(
		const FSightWeaveMemoryScopeKey& Scope,
		FVector2D WorldLocation,
		FIntPoint& OutLogicalTile,
		FIntPoint& OutInteriorTexel);

private:
	FSightWeavePackedMemoryTile* FindTile(FIntPoint LogicalCoordinate);
	const FSightWeavePackedMemoryTile* FindTile(FIntPoint LogicalCoordinate) const;

	FSightWeaveMemoryScopeKey Scope;
	struct FModifierRecord
	{
		FSightWeaveMemoryModifierHandle Handle;
		FSightWeaveMemoryModifierDescription Description;
	};
	TArray<FSightWeavePackedMemoryTile> Tiles;
	TSharedPtr<const TArray<FSightWeavePackedMemoryTile>, ESPMode::ThreadSafe>
		PublishedAuthorityTiles;
	TArray<FModifierRecord> Modifiers;
	TArray<FIntPoint> DirtyLogicalTiles;
	TArray<FSightWeaveMemoryTileKey> RemovedTiles;
	int32 MaximumTiles = 0;
	uint64 MemoryRevision = 0;
	uint64 LastSnapshotRevision = 0;
	uint64 NextPacketRevision = 1;
	uint64 ModifierRevision = 0;
	uint64 PersistenceGuardRevision = 0;
	int64 NextModifierId = 1;
	ESightWeaveMemoryFailure LastFailure = ESightWeaveMemoryFailure::NotConfigured;
	bool bConfigured = false;
	bool bNeedsFullRebuild = false;
	bool bModifierStateDirty = false;
};

#if WITH_DEV_AUTOMATION_TESTS
SIGHTWEAVERUNTIME_API bool SightWeaveMemoryRasterMatchesFullRowsForTesting(TConstArrayView<FVector> Vertices,
 const FSightWeaveMemoryScopeKey& Scope,FIntPoint Tile);
#endif
