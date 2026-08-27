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
	CapacityExceeded
};

struct SIGHTWEAVERUNTIME_API FSightWeaveMemoryScopeKey
{
	FSightWeaveRenderWorldIdentity WorldIdentity;
	uint64 WorldGeneration = 0;
	FSightWeaveKnowledgeOwnerId KnowledgeOwnerId;
	FSightWeaveFloorId FloorId;
	FVector2D FloorOrigin = FVector2D::ZeroVector;
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
	bool IsFullRebuild() const { return bFullRebuild; }
	TConstArrayView<FSightWeavePackedMemoryTile> GetDirtyTiles() const { return DirtyTiles; }
	TConstArrayView<FSightWeaveMemoryTileKey> GetRemovedTiles() const { return RemovedTiles; }
	int64 GetPackedAuthorityBytes() const { return PackedAuthorityBytes; }
	bool HasMirrorWork() const { return bFullRebuild || !DirtyTiles.IsEmpty() || !RemovedTiles.IsEmpty(); }

private:
	friend class FSightWeaveMemoryAuthority;

	bool bValid = false;
	ESightWeaveMemoryFailure Failure = ESightWeaveMemoryFailure::NotConfigured;
	FSightWeaveMemoryScopeKey Scope;
	uint64 MemoryRevision = 0;
	uint64 SnapshotRevision = 0;
	uint64 PacketRevision = 0;
	int64 PackedAuthorityBytes = 0;
	bool bFullRebuild = false;
	TArray<FSightWeavePackedMemoryTile> DirtyTiles;
	TArray<FSightWeaveMemoryTileKey> RemovedTiles;
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
	int32 GetAllocatedTileCount() const { return Tiles.Num(); }
	int64 GetPackedAuthorityBytes() const;
	ESightWeaveMemoryFailure GetLastFailure() const { return LastFailure; }

	FSightWeaveMemoryUpdateDiagnostics WriteEffectiveLive(const FSightWeaveFrameSnapshot& Snapshot);
	bool QueryHardMemory(FVector WorldLocation) const;
	bool QueryHardMemory2D(FVector2D WorldLocation) const;
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
	TArray<FSightWeavePackedMemoryTile> Tiles;
	TArray<FIntPoint> DirtyLogicalTiles;
	TArray<FSightWeaveMemoryTileKey> RemovedTiles;
	int32 MaximumTiles = 0;
	uint64 MemoryRevision = 0;
	uint64 LastSnapshotRevision = 0;
	uint64 NextPacketRevision = 1;
	ESightWeaveMemoryFailure LastFailure = ESightWeaveMemoryFailure::NotConfigured;
	bool bConfigured = false;
	bool bNeedsFullRebuild = false;
};
