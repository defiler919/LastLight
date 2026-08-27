#pragma once

#include "CoreMinimal.h"
#include "SightWeaveRenderPacket.h"

namespace SightWeave::SparseAtlas
{
	inline constexpr int32 PhysicalTileSize = 256;
	inline constexpr int32 InteriorTileSize = 248;
	inline constexpr int32 GutterTexels = 4;
	inline constexpr int32 PageSize = 2048;
	inline constexpr int32 SlotsPerPageAxis = PageSize / PhysicalTileSize;
	inline constexpr int32 SlotsPerPage = SlotsPerPageAxis * SlotsPerPageAxis;
	inline constexpr int32 StandardActiveTileCapacity = 128;
	inline constexpr int32 MaximumActiveProfiles = 32;
	inline constexpr uint64 PageBytes = static_cast<uint64>(PageSize) * PageSize;
}

UENUM(BlueprintType)
enum class ESightWeaveRenderPrecisionTier : uint8
{
	Coarse,
	Standard,
	Fine,
	Ultra
};

SIGHTWEAVERUNTIME_API float SightWeaveCentimetersPerTexel(ESightWeaveRenderPrecisionTier Tier);
SIGHTWEAVERUNTIME_API int32 SightWeaveDefaultActiveTileCapacity(ESightWeaveRenderPrecisionTier Tier);

struct SIGHTWEAVERUNTIME_API FSightWeaveSparseScopeKey
{
	FSightWeaveRenderWorldIdentity WorldIdentity;
	FSightWeaveKnowledgeOwnerId KnowledgeOwnerId;
	FSightWeaveFloorId FloorId;
	ESightWeaveRenderPrecisionTier PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
	FVector2D FloorOrigin = FVector2D::ZeroVector;

	bool IsValid() const;
	bool IsEquivalentTo(const FSightWeaveSparseScopeKey& Other) const;
	bool HasSameOwnerFloor(const FSightWeaveSparseScopeKey& Other) const;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSparseTileKey
{
	FSightWeaveSparseScopeKey Scope;
	FIntPoint LogicalCoordinate = FIntPoint::ZeroValue;

	bool IsValid() const { return Scope.IsValid(); }
	bool IsEquivalentTo(const FSightWeaveSparseTileKey& Other) const;
};

/** Full sequence equality is authoritative; StableHash is only an accelerator. */
struct SIGHTWEAVERUNTIME_API FSightWeaveSparseTileIdentity
{
	FSightWeaveSparseTileKey TileKey;
	TArray<FSightWeaveRenderProfileIdentity> CanonicalProfiles;

	bool IsValid() const;
	bool IsEquivalentTo(const FSightWeaveSparseTileIdentity& Other) const;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSparsePhysicalAddress
{
	int32 PageIndex = INDEX_NONE;
	int32 SlotIndex = INDEX_NONE;

	bool IsValid() const;
	FIntPoint GetSlotOrigin() const;
	FIntRect GetSlotRect() const;
	int32 GetLinearIndex() const;
};

enum class ESightWeaveSparseResidencyDisposition : uint8
{
	Existing,
	Allocated,
	Reused,
	CapacityExceeded,
	Invalid
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSparseResidencyResult
{
	ESightWeaveSparseResidencyDisposition Disposition = ESightWeaveSparseResidencyDisposition::Invalid;
	FSightWeaveSparsePhysicalAddress Address;
	FSightWeaveSparseTileIdentity EvictedIdentity;
	bool bRequiresBlackClear = false;

	bool Succeeded() const
	{
		return Disposition == ESightWeaveSparseResidencyDisposition::Existing
			|| Disposition == ESightWeaveSparseResidencyDisposition::Allocated
			|| Disposition == ESightWeaveSparseResidencyDisposition::Reused;
	}
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSparseResidencySlot
{
	FSightWeaveSparsePhysicalAddress Address;
	FSightWeaveSparseTileIdentity Identity;
	uint64 DesiredRevision = 0;
	uint64 AppliedRevision = 0;
	uint64 LastUseSerial = 0;
	int32 PinCount = 0;
	int32 InFlightCount = 0;
	int32 ReadbackCount = 0;
	bool bOccupied = false;
	bool bRequiresBlackClear = true;

	bool IsProtected() const { return PinCount > 0 || InFlightCount > 0 || ReadbackCount > 0; }
};

/** Deterministic bounded residency. Full identity equality prevents hash-collision aliasing. */
class SIGHTWEAVERUNTIME_API FSightWeaveSparseAtlasResidency final
{
public:
	explicit FSightWeaveSparseAtlasResidency(
		int32 InCapacity = SightWeave::SparseAtlas::StandardActiveTileCapacity);

	FSightWeaveSparseResidencyResult Acquire(
		const FSightWeaveSparseTileIdentity& Identity,
		uint64 DesiredRevision);
	bool Release(
		const FSightWeaveSparseTileIdentity& Identity,
		FSightWeaveSparsePhysicalAddress* OutAddress = nullptr);
	bool MarkBlackCleared(const FSightWeaveSparsePhysicalAddress& Address);
	bool MarkApplied(const FSightWeaveSparsePhysicalAddress& Address, uint64 AppliedRevision);
	bool AddPin(const FSightWeaveSparsePhysicalAddress& Address);
	bool RemovePin(const FSightWeaveSparsePhysicalAddress& Address);
	bool AddInFlight(const FSightWeaveSparsePhysicalAddress& Address);
	bool RemoveInFlight(const FSightWeaveSparsePhysicalAddress& Address);
	bool AddReadback(const FSightWeaveSparsePhysicalAddress& Address);
	bool RemoveReadback(const FSightWeaveSparsePhysicalAddress& Address);
	const FSightWeaveSparseResidencySlot* Find(const FSightWeaveSparseTileIdentity& Identity) const;
	const FSightWeaveSparseResidencySlot* Find(const FSightWeaveSparsePhysicalAddress& Address) const;
	TConstArrayView<FSightWeaveSparseResidencySlot> GetSlots() const { return Slots; }
	int32 GetCapacity() const { return Slots.Num(); }
	int32 GetResidentCount() const;
	uint64 GetAllocationCount() const { return AllocationCount; }
	uint64 GetReuseCount() const { return ReuseCount; }
	uint64 GetEvictionCount() const { return EvictionCount; }
	uint64 GetCapacityFailureCount() const { return CapacityFailureCount; }
	void Reset();

private:
	FSightWeaveSparseResidencySlot* FindMutable(const FSightWeaveSparsePhysicalAddress& Address);

	TArray<FSightWeaveSparseResidencySlot> Slots;
	uint64 AccessSerial = 0;
	uint64 AllocationCount = 0;
	uint64 ReuseCount = 0;
	uint64 EvictionCount = 0;
	uint64 CapacityFailureCount = 0;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSparsePolygonInput
{
	int64 StableSourceId = 0;
	uint64 SourceRevision = 0;
	ESightWeaveRenderMaskLayer Layer = ESightWeaveRenderMaskLayer::Vision;
	FSightWeaveRenderProfileIdentity CompatibilityProfile;
	TArray<FVector2D> WorldVertices;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSparseScopeBuildInput
{
	FSightWeaveKnowledgeOwnerId KnowledgeOwnerId;
	FSightWeaveFloorId FloorId;
	FVector2D FloorOrigin = FVector2D::ZeroVector;
	ESightWeaveRenderPrecisionTier PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
	int32 MaximumActiveTiles = 0;
	TArray<FSightWeaveSparsePolygonInput> Polygons;
};

struct FSightWeaveSparseRenderPacket;

struct SIGHTWEAVERUNTIME_API FSightWeaveSparseRenderPacketBuildInput
{
	FSightWeaveRenderWorldIdentity WorldIdentity;
	uint64 PacketRevision = 0;
	uint64 RegistryRevision = 0;
	uint64 PublishedSnapshotRevision = 0;
	bool bForceFullRebuild = false;
	TArray<FSightWeaveSparseScopeBuildInput> Scopes;
	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> PreviousPacket;
};

enum class ESightWeaveSparsePacketFailure : uint8
{
	None,
	InvalidWorldIdentity,
	InvalidRevision,
	InvalidScope,
	InvalidPrecision,
	InvalidProfile,
	InvalidPolygon,
	CapacityExceeded,
	InvalidPreviousPacket,
	InvalidTile
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSparseProfileGeometry
{
	FSightWeaveRenderProfileIdentity Identity;
	FSightWeaveRenderTriangleRange VisionRange;
	FSightWeaveRenderTriangleRange IlluminationRange;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSparseRenderTile
{
	FSightWeaveSparseTileIdentity Identity;
	FBox2D PhysicalWorldBounds = FBox2D(ForceInit);
	float CentimetersPerTexel = SightWeave::RenderPacket::StandardCentimetersPerTexel;
	TArray<FSightWeaveSparseProfileGeometry> Profiles;
	FSightWeaveRenderTriangleRange BypassRange;
	FSightWeaveRenderTriangleRange SuppressionRange;
	TArray<FVector2f> Vertices;
	TArray<uint32> Indices;
	uint64 ContentHash = 0;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSparseRenderScope
{
	FSightWeaveSparseScopeKey ScopeKey;
	ESightWeaveSparsePacketFailure Failure = ESightWeaveSparsePacketFailure::None;
	int32 MaximumActiveTiles = 0;
	int32 DesiredTileCount = 0;
	bool bFullRebuild = false;

	bool IsValid() const { return Failure == ESightWeaveSparsePacketFailure::None; }
};

/** Immutable, self-contained sparse-atlas packet. Dirty indices select work; every tile owns a full redraw. */
struct SIGHTWEAVERUNTIME_API FSightWeaveSparseRenderPacket final
{
public:
	bool IsValid() const { return bValid; }
	ESightWeaveSparsePacketFailure GetFailure() const { return Failure; }
	FSightWeaveRenderWorldIdentity GetWorldIdentity() const { return WorldIdentity; }
	uint64 GetPacketRevision() const { return PacketRevision; }
	uint64 GetRegistryRevision() const { return RegistryRevision; }
	uint64 GetPublishedSnapshotRevision() const { return PublishedSnapshotRevision; }
	uint64 GetContentHash() const { return ContentHash; }
	TConstArrayView<FSightWeaveSparseRenderScope> GetScopes() const { return Scopes; }
	TConstArrayView<FSightWeaveSparseRenderTile> GetTiles() const { return Tiles; }
	TConstArrayView<int32> GetDirtyTileIndices() const { return DirtyTileIndices; }
	TConstArrayView<FSightWeaveSparseTileIdentity> GetRemovedTiles() const { return RemovedTiles; }
	bool HasMaskWork() const { return !DirtyTileIndices.IsEmpty() || !RemovedTiles.IsEmpty(); }

private:
	friend class FSightWeaveSparseRenderPacketBuilder;
	bool bValid = false;
	ESightWeaveSparsePacketFailure Failure = ESightWeaveSparsePacketFailure::InvalidWorldIdentity;
	FSightWeaveRenderWorldIdentity WorldIdentity;
	uint64 PacketRevision = 0;
	uint64 RegistryRevision = 0;
	uint64 PublishedSnapshotRevision = 0;
	uint64 ContentHash = 0;
	TArray<FSightWeaveSparseRenderScope> Scopes;
	TArray<FSightWeaveSparseRenderTile> Tiles;
	TArray<int32> DirtyTileIndices;
	TArray<FSightWeaveSparseTileIdentity> RemovedTiles;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveSparseRenderPacketBuildResult
{
	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet;
	ESightWeaveSparsePacketFailure Failure = ESightWeaveSparsePacketFailure::None;
	int32 DirtyTileCount = 0;
	int32 RemovedTileCount = 0;
	int32 FullRebuildScopeCount = 0;
	int32 FailedScopeCount = 0;

	bool Succeeded() const { return Packet.IsValid() && Packet->IsValid(); }
};

class SIGHTWEAVERUNTIME_API FSightWeaveSparseRenderPacketBuilder final
{
public:
	static FSightWeaveSparseRenderPacketBuildResult Build(
		const FSightWeaveSparseRenderPacketBuildInput& Input);
	static ESightWeaveSparsePacketFailure Validate(const FSightWeaveSparseRenderPacket& Packet);
	static FIntPoint WorldToLogicalTile(
		const FVector2D& WorldPoint,
		const FVector2D& FloorOrigin,
		ESightWeaveRenderPrecisionTier PrecisionTier);
	static FBox2D LogicalTileToPhysicalBounds(
		const FIntPoint& LogicalCoordinate,
		const FVector2D& FloorOrigin,
		ESightWeaveRenderPrecisionTier PrecisionTier);
};
