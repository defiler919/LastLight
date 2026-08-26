#pragma once

#include "CoreMinimal.h"
#include "SightWeaveTypes.h"

/** M3.1 owns one physical tile only. These values are part of the frozen packet contract. */
namespace SightWeave::RenderPacket
{
	inline constexpr int32 PhysicalTileSize = 256;
	inline constexpr int32 InteriorTileSize = 248;
	inline constexpr int32 GutterTexels = 4;
	inline constexpr float StandardCentimetersPerTexel = 10.0f;
	inline constexpr int32 MaximumPolygonVertices = 4096;
	inline constexpr int32 MaximumPacketVertices = 65536;
}

enum class ESightWeaveRenderMaskLayer : uint8
{
	Vision,
	Illumination,
	Bypass,
	Suppression,
	Count
};

enum class ESightWeaveRenderDirtyReason : uint8
{
	None = 0,
	RegistryChanged = 1 << 0,
	SourceChanged = 1 << 1,
	SuppressionChanged = 1 << 2,
	ExplicitClear = 1 << 3,
	WorldTeardown = 1 << 4
};
ENUM_CLASS_FLAGS(ESightWeaveRenderDirtyReason);

enum class ESightWeaveRenderPacketFailure : uint8
{
	None,
	InvalidWorldIdentity,
	InvalidScope,
	InvalidProfile,
	InvalidRevision,
	InvalidTile,
	InvalidSourceIdentity,
	ScopeMismatch,
	ProfileMismatch,
	NonFiniteVertex,
	TooManyVertices,
	DegeneratePolygon,
	NonSimplePolygon,
	TriangulationFailed,
	InvalidIndexData
};

enum class ESightWeaveRenderPacketDisposition : uint8
{
	Accepted,
	Duplicate,
	Stale,
	RevisionConflict,
	WorldMismatch,
	Invalid
};

struct SIGHTWEAVERUNTIME_API FSightWeaveRenderWorldIdentity
{
	uint64 Serial = 0;

	bool IsValid() const { return Serial != 0; }
	friend bool operator==(const FSightWeaveRenderWorldIdentity& A, const FSightWeaveRenderWorldIdentity& B)
	{
		return A.Serial == B.Serial;
	}
	friend bool operator!=(const FSightWeaveRenderWorldIdentity& A, const FSightWeaveRenderWorldIdentity& B)
	{
		return !(A == B);
	}
};

/** Canonical, order-independent identity for one frozen M3 illumination compatibility profile. */
struct SIGHTWEAVERUNTIME_API FSightWeaveRenderProfileIdentity
{
	TArray<FName> CanonicalCapabilities;
	uint64 StableHash = 0;

	static FSightWeaveRenderProfileIdentity FromProfile(
		const FSightWeaveIlluminationCompatibilityProfile& Profile);
	bool IsValid() const { return StableHash != 0; }
	bool IsEquivalentTo(const FSightWeaveRenderProfileIdentity& Other) const;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveRenderTriangleRange
{
	uint32 FirstVertex = 0;
	uint32 VertexCount = 0;
	uint32 FirstIndex = 0;
	uint32 IndexCount = 0;

	bool IsEmpty() const { return IndexCount == 0; }
	uint32 GetTriangleCount() const { return IndexCount / 3; }
};

/** Owned builder input. Callers never expose mutable snapshot storage to the render thread. */
struct SIGHTWEAVERUNTIME_API FSightWeaveRenderPolygonInput
{
	int64 StableSourceId = 0;
	ESightWeaveRenderMaskLayer Layer = ESightWeaveRenderMaskLayer::Vision;
	FSightWeaveKnowledgeOwnerId KnowledgeOwnerId;
	FSightWeaveFloorId FloorId;
	FSightWeaveRenderProfileIdentity CompatibilityProfile;
	TArray<FVector2D> WorldVertices;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveRenderPacketBuildInput
{
	FSightWeaveRenderWorldIdentity WorldIdentity;
	FSightWeaveKnowledgeOwnerId KnowledgeOwnerId;
	FSightWeaveFloorId FloorId;
	FSightWeaveRenderProfileIdentity CompatibilityProfile;
	uint64 PacketRevision = 0;
	uint64 RegistryRevision = 0;
	uint64 PublishedSnapshotRevision = 0;
	FIntPoint TileCoordinate = FIntPoint::ZeroValue;
	FBox2D PhysicalWorldBounds = FBox2D(ForceInit);
	float CentimetersPerTexel = SightWeave::RenderPacket::StandardCentimetersPerTexel;
	int32 Gutter = SightWeave::RenderPacket::GutterTexels;
	ESightWeaveRenderDirtyReason DirtyReason = ESightWeaveRenderDirtyReason::None;
	bool bFullTile = true;
	TArray<FSightWeaveRenderPolygonInput> Polygons;
};

/**
 * Compact immutable packet consumed by SightWeaveRender. All fields are private;
 * publication returns only a thread-safe const shared pointer.
 */
class SIGHTWEAVERUNTIME_API FSightWeaveRenderPacket final
{
public:
	bool IsValid() const { return bValid; }
	ESightWeaveRenderPacketFailure GetFailure() const { return Failure; }
	FSightWeaveRenderWorldIdentity GetWorldIdentity() const { return WorldIdentity; }
	FSightWeaveKnowledgeOwnerId GetKnowledgeOwnerId() const { return KnowledgeOwnerId; }
	FSightWeaveFloorId GetFloorId() const { return FloorId; }
	const FSightWeaveRenderProfileIdentity& GetCompatibilityProfile() const { return CompatibilityProfile; }
	uint64 GetPacketRevision() const { return PacketRevision; }
	uint64 GetRegistryRevision() const { return RegistryRevision; }
	uint64 GetPublishedSnapshotRevision() const { return PublishedSnapshotRevision; }
	FIntPoint GetTileCoordinate() const { return TileCoordinate; }
	const FBox2D& GetPhysicalWorldBounds() const { return PhysicalWorldBounds; }
	FVector2f GetWorldToTileUvScale() const { return WorldToTileUvScale; }
	FVector2f GetWorldToTileUvBias() const { return WorldToTileUvBias; }
	float GetCentimetersPerTexel() const { return CentimetersPerTexel; }
	int32 GetGutter() const { return Gutter; }
	ESightWeaveRenderDirtyReason GetDirtyReason() const { return DirtyReason; }
	bool IsFullTile() const { return bFullTile; }
	uint64 GetContentHash() const { return ContentHash; }
	TConstArrayView<FVector2f> GetVertices() const { return Vertices; }
	TConstArrayView<uint32> GetIndices() const { return Indices; }
	const FSightWeaveRenderTriangleRange& GetRange(ESightWeaveRenderMaskLayer Layer) const;

private:
	friend class FSightWeaveRenderPacketBuilder;

	bool bValid = false;
	ESightWeaveRenderPacketFailure Failure = ESightWeaveRenderPacketFailure::InvalidWorldIdentity;
	FSightWeaveRenderWorldIdentity WorldIdentity;
	FSightWeaveKnowledgeOwnerId KnowledgeOwnerId;
	FSightWeaveFloorId FloorId;
	FSightWeaveRenderProfileIdentity CompatibilityProfile;
	uint64 PacketRevision = 0;
	uint64 RegistryRevision = 0;
	uint64 PublishedSnapshotRevision = 0;
	FIntPoint TileCoordinate = FIntPoint::ZeroValue;
	FBox2D PhysicalWorldBounds = FBox2D(ForceInit);
	FVector2f WorldToTileUvScale = FVector2f::ZeroVector;
	FVector2f WorldToTileUvBias = FVector2f::ZeroVector;
	float CentimetersPerTexel = SightWeave::RenderPacket::StandardCentimetersPerTexel;
	int32 Gutter = SightWeave::RenderPacket::GutterTexels;
	ESightWeaveRenderDirtyReason DirtyReason = ESightWeaveRenderDirtyReason::None;
	bool bFullTile = true;
	TStaticArray<FSightWeaveRenderTriangleRange, static_cast<int32>(ESightWeaveRenderMaskLayer::Count)> Ranges;
	TArray<FVector2f> Vertices;
	TArray<uint32> Indices;
	uint64 ContentHash = 0;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveRenderPacketBuildResult
{
	TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe> Packet;
	ESightWeaveRenderPacketFailure Failure = ESightWeaveRenderPacketFailure::None;
	int32 AcceptedPolygonCount = 0;
	int32 OutsidePolygonCount = 0;
	int32 RemovedDuplicateVertexCount = 0;
	int32 RemovedCollinearVertexCount = 0;

	bool Succeeded() const { return Packet.IsValid() && Packet->IsValid(); }
};

class SIGHTWEAVERUNTIME_API FSightWeaveRenderPacketBuilder final
{
public:
	/** Builds a new owned immutable packet. Invalid input still returns a fail-closed black packet. */
	static FSightWeaveRenderPacketBuildResult Build(const FSightWeaveRenderPacketBuildInput& Input);

	/** Independent validation used at the GT/RT trust boundary. */
	static ESightWeaveRenderPacketFailure Validate(const FSightWeaveRenderPacket& Packet);
};

/** Render-consumer revision gate. One instance belongs to exactly one world lifetime. */
class SIGHTWEAVERUNTIME_API FSightWeaveRenderPacketRevisionGate final
{
public:
	explicit FSightWeaveRenderPacketRevisionGate(FSightWeaveRenderWorldIdentity InWorldIdentity)
		: WorldIdentity(InWorldIdentity)
	{
	}

	ESightWeaveRenderPacketDisposition ClassifyAndCommit(const FSightWeaveRenderPacket& Packet);
	void Reset();
	uint64 GetAcceptedRevision() const { return AcceptedRevision; }
	uint64 GetAcceptedHash() const { return AcceptedHash; }
	uint64 GetAcceptedPacketCount() const { return AcceptedPacketCount; }
	uint64 GetDuplicatePacketCount() const { return DuplicatePacketCount; }
	uint64 GetStalePacketCount() const { return StalePacketCount; }

private:
	FSightWeaveRenderWorldIdentity WorldIdentity;
	uint64 AcceptedRevision = 0;
	uint64 AcceptedHash = 0;
	uint64 AcceptedPacketCount = 0;
	uint64 DuplicatePacketCount = 0;
	uint64 StalePacketCount = 0;
};
