#include "SightWeaveSparseAtlas.h"

#include "Algo/Sort.h"
#include "Containers/StringConv.h"

namespace SightWeaveSparseAtlasPrivate
{
	constexpr uint64 FnvOffset = 14695981039346656037ull;
	constexpr uint64 FnvPrime = 1099511628211ull;
	constexpr double TileBoundaryBias = 1.0e-6;

	bool IsFinite(const FVector2D& Point)
	{
		return FMath::IsFinite(Point.X) && FMath::IsFinite(Point.Y);
	}

	void HashBytes(uint64& Hash, const void* Data, const int32 NumBytes)
	{
		const uint8* Bytes = static_cast<const uint8*>(Data);
		for (int32 Index = 0; Index < NumBytes; ++Index)
		{
			Hash ^= Bytes[Index];
			Hash *= FnvPrime;
		}
	}

	template <typename ValueType>
	void HashValue(uint64& Hash, const ValueType& Value)
	{
		HashBytes(Hash, &Value, sizeof(ValueType));
	}

	void HashName(uint64& Hash, const FName Name)
	{
		const FString Lower = Name.ToString().ToLower();
		const FTCHARToUTF8 Utf8(*Lower);
		HashBytes(Hash, Utf8.Get(), Utf8.Length());
		const uint8 Terminator = 0;
		HashValue(Hash, Terminator);
	}

	void HashProfile(uint64& Hash, const FSightWeaveRenderProfileIdentity& Profile)
	{
		HashValue(Hash, Profile.StableHash);
		const uint32 Count = static_cast<uint32>(Profile.CanonicalCapabilities.Num());
		HashValue(Hash, Count);
		for (const FName Capability : Profile.CanonicalCapabilities)
		{
			HashName(Hash, Capability);
		}
	}

	void HashScope(uint64& Hash, const FSightWeaveSparseScopeKey& Scope)
	{
		HashValue(Hash, Scope.WorldIdentity.Serial);
		HashName(Hash, Scope.KnowledgeOwnerId.GetValue());
		HashName(Hash, Scope.FloorId.GetValue());
		const uint8 Tier = static_cast<uint8>(Scope.PrecisionTier);
		HashValue(Hash, Tier);
		HashValue(Hash, Scope.FloorOrigin.X);
		HashValue(Hash, Scope.FloorOrigin.Y);
	}

	bool ProfileLess(
		const FSightWeaveRenderProfileIdentity& A,
		const FSightWeaveRenderProfileIdentity& B)
	{
		const int32 CommonCount = FMath::Min(
			A.CanonicalCapabilities.Num(),
			B.CanonicalCapabilities.Num());
		for (int32 Index = 0; Index < CommonCount; ++Index)
		{
			if (A.CanonicalCapabilities[Index] == B.CanonicalCapabilities[Index])
			{
				continue;
			}
			return A.CanonicalCapabilities[Index].LexicalLess(B.CanonicalCapabilities[Index]);
		}
		return A.CanonicalCapabilities.Num() < B.CanonicalCapabilities.Num();
	}

	bool ScopeLess(
		const FSightWeaveSparseScopeBuildInput& A,
		const FSightWeaveSparseScopeBuildInput& B)
	{
		if (A.KnowledgeOwnerId != B.KnowledgeOwnerId)
		{
			return A.KnowledgeOwnerId.GetValue().LexicalLess(B.KnowledgeOwnerId.GetValue());
		}
		if (A.FloorId != B.FloorId)
		{
			return A.FloorId.GetValue().LexicalLess(B.FloorId.GetValue());
		}
		if (A.PrecisionTier != B.PrecisionTier)
		{
			return static_cast<uint8>(A.PrecisionTier) < static_cast<uint8>(B.PrecisionTier);
		}
		return A.FloorOrigin.X != B.FloorOrigin.X
			? A.FloorOrigin.X < B.FloorOrigin.X
			: A.FloorOrigin.Y < B.FloorOrigin.Y;
	}

	bool TileCoordinateLess(const FIntPoint& A, const FIntPoint& B)
	{
		return A.X != B.X ? A.X < B.X : A.Y < B.Y;
	}

	bool BoundsOverlap(const FBox2D& A, const FBox2D& B)
	{
		return A.bIsValid && B.bIsValid
			&& A.Max.X >= B.Min.X && A.Min.X <= B.Max.X
			&& A.Max.Y >= B.Min.Y && A.Min.Y <= B.Max.Y;
	}

	bool BuildPolygonBounds(
		const FSightWeaveSparsePolygonInput& Polygon,
		FBox2D& OutBounds)
	{
		OutBounds = FBox2D(ForceInit);
		if (Polygon.StableSourceId <= 0 || Polygon.WorldVertices.Num() < 3)
		{
			return false;
		}
		for (const FVector2D& Vertex : Polygon.WorldVertices)
		{
			if (!IsFinite(Vertex))
			{
				return false;
			}
			OutBounds += Vertex;
		}
		return OutBounds.bIsValid;
	}

	bool AddUniqueProfile(
		TArray<FSightWeaveRenderProfileIdentity>& Profiles,
		const FSightWeaveRenderProfileIdentity& Candidate)
	{
		if (!Candidate.IsValid())
		{
			return false;
		}
		if (!Profiles.ContainsByPredicate([&Candidate](const FSightWeaveRenderProfileIdentity& Existing)
		{
			return Existing.IsEquivalentTo(Candidate);
		}))
		{
			Profiles.Add(Candidate);
		}
		return true;
	}

	void AppendLegacyRange(
		const FSightWeaveRenderPacket& Source,
		const ESightWeaveRenderMaskLayer Layer,
		FSightWeaveSparseRenderTile& Destination,
		FSightWeaveRenderTriangleRange& OutRange)
	{
		const FSightWeaveRenderTriangleRange& SourceRange = Source.GetRange(Layer);
		OutRange = FSightWeaveRenderTriangleRange();
		if (SourceRange.IsEmpty())
		{
			return;
		}
		OutRange.FirstVertex = static_cast<uint32>(Destination.Vertices.Num());
		OutRange.VertexCount = SourceRange.VertexCount;
		OutRange.FirstIndex = static_cast<uint32>(Destination.Indices.Num());
		OutRange.IndexCount = SourceRange.IndexCount;
		const TConstArrayView<FVector2f> SourceVertices = Source.GetVertices();
		const TConstArrayView<uint32> SourceIndices = Source.GetIndices();
		for (uint32 Offset = 0; Offset < SourceRange.VertexCount; ++Offset)
		{
			Destination.Vertices.Add(SourceVertices[SourceRange.FirstVertex + Offset]);
		}
		for (uint32 Offset = 0; Offset < SourceRange.IndexCount; ++Offset)
		{
			const uint32 SourceIndex = SourceIndices[SourceRange.FirstIndex + Offset];
			check(SourceIndex >= SourceRange.FirstVertex);
			Destination.Indices.Add(
				OutRange.FirstVertex + SourceIndex - SourceRange.FirstVertex);
		}
	}

	FSightWeaveRenderPacketBuildInput MakeLegacyInput(
		const FSightWeaveSparseRenderPacketBuildInput& PacketInput,
		const FSightWeaveSparseScopeBuildInput& ScopeInput,
		const FSightWeaveRenderProfileIdentity& Profile,
		const FIntPoint& LogicalCoordinate,
		const FBox2D& PhysicalBounds)
	{
		FSightWeaveRenderPacketBuildInput Result;
		Result.WorldIdentity = PacketInput.WorldIdentity;
		Result.KnowledgeOwnerId = ScopeInput.KnowledgeOwnerId;
		Result.FloorId = ScopeInput.FloorId;
		Result.CompatibilityProfile = Profile;
		Result.PacketRevision = PacketInput.PacketRevision;
		Result.RegistryRevision = PacketInput.RegistryRevision;
		Result.PublishedSnapshotRevision = PacketInput.PublishedSnapshotRevision;
		Result.TileCoordinate = LogicalCoordinate;
		Result.PhysicalWorldBounds = PhysicalBounds;
		Result.CentimetersPerTexel = SightWeaveCentimetersPerTexel(ScopeInput.PrecisionTier);
		Result.Gutter = SightWeave::SparseAtlas::GutterTexels;
		Result.DirtyReason = ESightWeaveRenderDirtyReason::SourceChanged;
		Result.bFullTile = true;
		return Result;
	}

	void AddLegacyPolygon(
		FSightWeaveRenderPacketBuildInput& Destination,
		const FSightWeaveSparsePolygonInput& Source)
	{
		FSightWeaveRenderPolygonInput& Polygon = Destination.Polygons.AddDefaulted_GetRef();
		Polygon.StableSourceId = Source.StableSourceId;
		Polygon.Layer = Source.Layer;
		Polygon.KnowledgeOwnerId = Destination.KnowledgeOwnerId;
		Polygon.FloorId = Destination.FloorId;
		Polygon.CompatibilityProfile = Destination.CompatibilityProfile;
		Polygon.WorldVertices = Source.WorldVertices;
	}

	uint64 ComputeTileHash(const FSightWeaveSparseRenderTile& Tile)
	{
		uint64 Hash = FnvOffset;
		HashScope(Hash, Tile.Identity.TileKey.Scope);
		HashValue(Hash, Tile.Identity.TileKey.LogicalCoordinate.X);
		HashValue(Hash, Tile.Identity.TileKey.LogicalCoordinate.Y);
		for (const FSightWeaveRenderProfileIdentity& Profile : Tile.Identity.CanonicalProfiles)
		{
			HashProfile(Hash, Profile);
		}
		for (const FSightWeaveSparseProfileGeometry& Profile : Tile.Profiles)
		{
			HashProfile(Hash, Profile.Identity);
			HashValue(Hash, Profile.VisionRange.FirstVertex);
			HashValue(Hash, Profile.VisionRange.VertexCount);
			HashValue(Hash, Profile.VisionRange.FirstIndex);
			HashValue(Hash, Profile.VisionRange.IndexCount);
			HashValue(Hash, Profile.IlluminationRange.FirstVertex);
			HashValue(Hash, Profile.IlluminationRange.VertexCount);
			HashValue(Hash, Profile.IlluminationRange.FirstIndex);
			HashValue(Hash, Profile.IlluminationRange.IndexCount);
		}
		HashValue(Hash, Tile.BypassRange.FirstVertex);
		HashValue(Hash, Tile.BypassRange.VertexCount);
		HashValue(Hash, Tile.BypassRange.FirstIndex);
		HashValue(Hash, Tile.BypassRange.IndexCount);
		HashValue(Hash, Tile.SuppressionRange.FirstVertex);
		HashValue(Hash, Tile.SuppressionRange.VertexCount);
		HashValue(Hash, Tile.SuppressionRange.FirstIndex);
		HashValue(Hash, Tile.SuppressionRange.IndexCount);
		for (const FVector2f& Vertex : Tile.Vertices)
		{
			uint32 XBits = 0;
			uint32 YBits = 0;
			FMemory::Memcpy(&XBits, &Vertex.X, sizeof(XBits));
			FMemory::Memcpy(&YBits, &Vertex.Y, sizeof(YBits));
			HashValue(Hash, XBits);
			HashValue(Hash, YBits);
		}
		for (const uint32 Index : Tile.Indices)
		{
			HashValue(Hash, Index);
		}
		return Hash == 0 ? 1 : Hash;
	}

	const FSightWeaveSparseRenderTile* FindPreviousTile(
		const FSightWeaveSparseRenderPacket& Packet,
		const FSightWeaveSparseTileIdentity& Identity)
	{
		return Packet.GetTiles().FindByPredicate([&Identity](const FSightWeaveSparseRenderTile& Tile)
		{
			return Tile.Identity.IsEquivalentTo(Identity);
		});
	}

	bool CurrentContainsIdentity(
		const FSightWeaveSparseRenderPacket& Packet,
		const FSightWeaveSparseTileIdentity& Identity)
	{
		return Packet.GetTiles().ContainsByPredicate([&Identity](const FSightWeaveSparseRenderTile& Tile)
		{
			return Tile.Identity.IsEquivalentTo(Identity);
		});
	}
}

using namespace SightWeaveSparseAtlasPrivate;

float SightWeaveCentimetersPerTexel(const ESightWeaveRenderPrecisionTier Tier)
{
	switch (Tier)
	{
	case ESightWeaveRenderPrecisionTier::Coarse: return 25.0f;
	case ESightWeaveRenderPrecisionTier::Standard: return 10.0f;
	case ESightWeaveRenderPrecisionTier::Fine: return 5.0f;
	case ESightWeaveRenderPrecisionTier::Ultra: return 2.5f;
	default: return 0.0f;
	}
}

int32 SightWeaveDefaultActiveTileCapacity(const ESightWeaveRenderPrecisionTier Tier)
{
	switch (Tier)
	{
	case ESightWeaveRenderPrecisionTier::Coarse: return 64;
	case ESightWeaveRenderPrecisionTier::Standard: return 128;
	case ESightWeaveRenderPrecisionTier::Fine: return 192;
	case ESightWeaveRenderPrecisionTier::Ultra: return 256;
	default: return 0;
	}
}

bool FSightWeaveSparseScopeKey::IsValid() const
{
	return WorldIdentity.IsValid()
		&& KnowledgeOwnerId.IsValid()
		&& FloorId.IsValid()
		&& IsFinite(FloorOrigin)
		&& SightWeaveCentimetersPerTexel(PrecisionTier) > 0.0f;
}

bool FSightWeaveSparseScopeKey::IsEquivalentTo(const FSightWeaveSparseScopeKey& Other) const
{
	return WorldIdentity == Other.WorldIdentity
		&& KnowledgeOwnerId == Other.KnowledgeOwnerId
		&& FloorId == Other.FloorId
		&& PrecisionTier == Other.PrecisionTier
		&& FloorOrigin == Other.FloorOrigin;
}

bool FSightWeaveSparseScopeKey::HasSameOwnerFloor(const FSightWeaveSparseScopeKey& Other) const
{
	return WorldIdentity == Other.WorldIdentity
		&& KnowledgeOwnerId == Other.KnowledgeOwnerId
		&& FloorId == Other.FloorId;
}

bool FSightWeaveSparseTileKey::IsEquivalentTo(const FSightWeaveSparseTileKey& Other) const
{
	return Scope.IsEquivalentTo(Other.Scope) && LogicalCoordinate == Other.LogicalCoordinate;
}

bool FSightWeaveSparseTileIdentity::IsValid() const
{
	if (!TileKey.IsValid())
	{
		return false;
	}
	for (const FSightWeaveRenderProfileIdentity& Profile : CanonicalProfiles)
	{
		if (!Profile.IsValid())
		{
			return false;
		}
	}
	return true;
}

bool FSightWeaveSparseTileIdentity::IsEquivalentTo(
	const FSightWeaveSparseTileIdentity& Other) const
{
	if (!TileKey.IsEquivalentTo(Other.TileKey)
		|| CanonicalProfiles.Num() != Other.CanonicalProfiles.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < CanonicalProfiles.Num(); ++Index)
	{
		if (!CanonicalProfiles[Index].IsEquivalentTo(Other.CanonicalProfiles[Index]))
		{
			return false;
		}
	}
	return true;
}

bool FSightWeaveSparsePhysicalAddress::IsValid() const
{
	return PageIndex >= 0
		&& SlotIndex >= 0
		&& SlotIndex < SightWeave::SparseAtlas::SlotsPerPage;
}

FIntPoint FSightWeaveSparsePhysicalAddress::GetSlotOrigin() const
{
	if (!IsValid())
	{
		return FIntPoint(INDEX_NONE, INDEX_NONE);
	}
	return FIntPoint(
		(SlotIndex % SightWeave::SparseAtlas::SlotsPerPageAxis)
			* SightWeave::SparseAtlas::PhysicalTileSize,
		(SlotIndex / SightWeave::SparseAtlas::SlotsPerPageAxis)
			* SightWeave::SparseAtlas::PhysicalTileSize);
}

FIntRect FSightWeaveSparsePhysicalAddress::GetSlotRect() const
{
	const FIntPoint Origin = GetSlotOrigin();
	return IsValid()
		? FIntRect(Origin, Origin + FIntPoint(
			SightWeave::SparseAtlas::PhysicalTileSize,
			SightWeave::SparseAtlas::PhysicalTileSize))
		: FIntRect();
}

int32 FSightWeaveSparsePhysicalAddress::GetLinearIndex() const
{
	return IsValid() ? PageIndex * SightWeave::SparseAtlas::SlotsPerPage + SlotIndex : INDEX_NONE;
}

FSightWeaveSparseAtlasResidency::FSightWeaveSparseAtlasResidency(const int32 InCapacity)
{
	const int32 Capacity = FMath::Max(0, InCapacity);
	Slots.SetNum(Capacity);
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		Slots[Index].Address.PageIndex = Index / SightWeave::SparseAtlas::SlotsPerPage;
		Slots[Index].Address.SlotIndex = Index % SightWeave::SparseAtlas::SlotsPerPage;
	}
}

FSightWeaveSparseResidencyResult FSightWeaveSparseAtlasResidency::Acquire(
	const FSightWeaveSparseTileIdentity& Identity,
	const uint64 DesiredRevision)
{
	FSightWeaveSparseResidencyResult Result;
	if (!Identity.IsValid() || DesiredRevision == 0)
	{
		return Result;
	}
	for (FSightWeaveSparseResidencySlot& Slot : Slots)
	{
		if (Slot.bOccupied && Slot.Identity.IsEquivalentTo(Identity))
		{
			Slot.LastUseSerial = ++AccessSerial;
			Slot.DesiredRevision = DesiredRevision;
			Result.Disposition = ESightWeaveSparseResidencyDisposition::Existing;
			Result.Address = Slot.Address;
			Result.bRequiresBlackClear = Slot.bRequiresBlackClear;
			return Result;
		}
	}
	for (FSightWeaveSparseResidencySlot& Slot : Slots)
	{
		if (!Slot.bOccupied)
		{
			Slot.bOccupied = true;
			Slot.Identity = Identity;
			Slot.DesiredRevision = DesiredRevision;
			Slot.AppliedRevision = 0;
			Slot.LastUseSerial = ++AccessSerial;
			Slot.bRequiresBlackClear = true;
			++AllocationCount;
			Result.Disposition = ESightWeaveSparseResidencyDisposition::Allocated;
			Result.Address = Slot.Address;
			Result.bRequiresBlackClear = true;
			return Result;
		}
	}

	FSightWeaveSparseResidencySlot* Candidate = nullptr;
	for (FSightWeaveSparseResidencySlot& Slot : Slots)
	{
		if (Slot.IsProtected())
		{
			continue;
		}
		if (!Candidate
			|| Slot.LastUseSerial < Candidate->LastUseSerial
			|| (Slot.LastUseSerial == Candidate->LastUseSerial
				&& Slot.Address.GetLinearIndex() < Candidate->Address.GetLinearIndex()))
		{
			Candidate = &Slot;
		}
	}
	if (!Candidate)
	{
		++CapacityFailureCount;
		Result.Disposition = ESightWeaveSparseResidencyDisposition::CapacityExceeded;
		return Result;
	}

	Result.Disposition = ESightWeaveSparseResidencyDisposition::Reused;
	Result.Address = Candidate->Address;
	Result.EvictedIdentity = Candidate->Identity;
	Result.bRequiresBlackClear = true;
	Candidate->Identity = Identity;
	Candidate->DesiredRevision = DesiredRevision;
	Candidate->AppliedRevision = 0;
	Candidate->LastUseSerial = ++AccessSerial;
	Candidate->bRequiresBlackClear = true;
	++ReuseCount;
	++EvictionCount;
	return Result;
}

bool FSightWeaveSparseAtlasResidency::Release(
	const FSightWeaveSparseTileIdentity& Identity,
	FSightWeaveSparsePhysicalAddress* OutAddress)
{
	for (FSightWeaveSparseResidencySlot& Slot : Slots)
	{
		if (!Slot.bOccupied || !Slot.Identity.IsEquivalentTo(Identity))
		{
			continue;
		}
		if (Slot.IsProtected())
		{
			return false;
		}
		if (OutAddress)
		{
			*OutAddress = Slot.Address;
		}
		Slot.bOccupied = false;
		Slot.Identity = FSightWeaveSparseTileIdentity();
		Slot.DesiredRevision = 0;
		Slot.AppliedRevision = 0;
		Slot.bRequiresBlackClear = true;
		return true;
	}
	return false;
}

FSightWeaveSparseResidencySlot* FSightWeaveSparseAtlasResidency::FindMutable(
	const FSightWeaveSparsePhysicalAddress& Address)
{
	const int32 Index = Address.GetLinearIndex();
	return Slots.IsValidIndex(Index) ? &Slots[Index] : nullptr;
}

bool FSightWeaveSparseAtlasResidency::MarkBlackCleared(
	const FSightWeaveSparsePhysicalAddress& Address)
{
	if (FSightWeaveSparseResidencySlot* Slot = FindMutable(Address))
	{
		Slot->bRequiresBlackClear = false;
		return true;
	}
	return false;
}

bool FSightWeaveSparseAtlasResidency::MarkApplied(
	const FSightWeaveSparsePhysicalAddress& Address,
	const uint64 AppliedRevision)
{
	if (FSightWeaveSparseResidencySlot* Slot = FindMutable(Address);
		Slot && Slot->bOccupied && AppliedRevision != 0)
	{
		Slot->AppliedRevision = AppliedRevision;
		Slot->DesiredRevision = AppliedRevision;
		Slot->bRequiresBlackClear = false;
		return true;
	}
	return false;
}

#define SIGHTWEAVE_SLOT_COUNTER(MethodName, FieldName, Delta, Guard) \
	bool FSightWeaveSparseAtlasResidency::MethodName(const FSightWeaveSparsePhysicalAddress& Address) \
	{ \
		if (FSightWeaveSparseResidencySlot* Slot = FindMutable(Address); Slot && (Guard)) \
		{ \
			Slot->FieldName += (Delta); \
			return true; \
		} \
		return false; \
	}

SIGHTWEAVE_SLOT_COUNTER(AddPin, PinCount, 1, Slot->bOccupied)
SIGHTWEAVE_SLOT_COUNTER(RemovePin, PinCount, -1, Slot->PinCount > 0)
SIGHTWEAVE_SLOT_COUNTER(AddInFlight, InFlightCount, 1, Slot->bOccupied)
SIGHTWEAVE_SLOT_COUNTER(RemoveInFlight, InFlightCount, -1, Slot->InFlightCount > 0)
SIGHTWEAVE_SLOT_COUNTER(AddReadback, ReadbackCount, 1, Slot->bOccupied)
SIGHTWEAVE_SLOT_COUNTER(RemoveReadback, ReadbackCount, -1, Slot->ReadbackCount > 0)

#undef SIGHTWEAVE_SLOT_COUNTER

const FSightWeaveSparseResidencySlot* FSightWeaveSparseAtlasResidency::Find(
	const FSightWeaveSparseTileIdentity& Identity) const
{
	return Slots.FindByPredicate([&Identity](const FSightWeaveSparseResidencySlot& Slot)
	{
		return Slot.bOccupied && Slot.Identity.IsEquivalentTo(Identity);
	});
}

const FSightWeaveSparseResidencySlot* FSightWeaveSparseAtlasResidency::Find(
	const FSightWeaveSparsePhysicalAddress& Address) const
{
	const int32 Index = Address.GetLinearIndex();
	return Slots.IsValidIndex(Index) ? &Slots[Index] : nullptr;
}

int32 FSightWeaveSparseAtlasResidency::GetResidentCount() const
{
	int32 ResidentCount = 0;
	for (const FSightWeaveSparseResidencySlot& Slot : Slots)
	{
		ResidentCount += Slot.bOccupied ? 1 : 0;
	}
	return ResidentCount;
}

void FSightWeaveSparseAtlasResidency::Reset()
{
	const int32 Capacity = Slots.Num();
	*this = FSightWeaveSparseAtlasResidency(Capacity);
}

FIntPoint FSightWeaveSparseRenderPacketBuilder::WorldToLogicalTile(
	const FVector2D& WorldPoint,
	const FVector2D& FloorOrigin,
	const ESightWeaveRenderPrecisionTier PrecisionTier)
{
	const double InteriorWorldSpan = static_cast<double>(SightWeave::SparseAtlas::InteriorTileSize)
		* SightWeaveCentimetersPerTexel(PrecisionTier);
	if (!IsFinite(WorldPoint) || !IsFinite(FloorOrigin) || InteriorWorldSpan <= 0.0)
	{
		return FIntPoint::ZeroValue;
	}
	const int64 TileX = FMath::FloorToInt64((WorldPoint.X - FloorOrigin.X) / InteriorWorldSpan);
	const int64 TileY = FMath::FloorToInt64((WorldPoint.Y - FloorOrigin.Y) / InteriorWorldSpan);
	return FIntPoint(
		static_cast<int32>(FMath::Clamp<int64>(TileX, MIN_int32, MAX_int32)),
		static_cast<int32>(FMath::Clamp<int64>(TileY, MIN_int32, MAX_int32)));
}

FBox2D FSightWeaveSparseRenderPacketBuilder::LogicalTileToPhysicalBounds(
	const FIntPoint& LogicalCoordinate,
	const FVector2D& FloorOrigin,
	const ESightWeaveRenderPrecisionTier PrecisionTier)
{
	const double CentimetersPerTexel = SightWeaveCentimetersPerTexel(PrecisionTier);
	const double InteriorWorldSpan = SightWeave::SparseAtlas::InteriorTileSize * CentimetersPerTexel;
	const double GutterWorldSpan = SightWeave::SparseAtlas::GutterTexels * CentimetersPerTexel;
	const FVector2D Minimum(
		FloorOrigin.X + static_cast<double>(LogicalCoordinate.X) * InteriorWorldSpan - GutterWorldSpan,
		FloorOrigin.Y + static_cast<double>(LogicalCoordinate.Y) * InteriorWorldSpan - GutterWorldSpan);
	const double PhysicalWorldSpan = SightWeave::SparseAtlas::PhysicalTileSize * CentimetersPerTexel;
	return FBox2D(Minimum, Minimum + FVector2D(PhysicalWorldSpan, PhysicalWorldSpan));
}

FSightWeaveSparseRenderPacketBuildResult FSightWeaveSparseRenderPacketBuilder::Build(
	const FSightWeaveSparseRenderPacketBuildInput& Input)
{
	FSightWeaveSparseRenderPacketBuildResult Result;
	TSharedRef<FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet =
		MakeShared<FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe>();
	Packet->WorldIdentity = Input.WorldIdentity;
	Packet->PacketRevision = Input.PacketRevision;
	Packet->RegistryRevision = Input.RegistryRevision;
	Packet->PublishedSnapshotRevision = Input.PublishedSnapshotRevision;
	Result.Packet = Packet;

	auto FailHeader = [&Result, &Packet](const ESightWeaveSparsePacketFailure Failure)
	{
		Packet->bValid = false;
		Packet->Failure = Failure;
		Packet->Scopes.Reset();
		Packet->Tiles.Reset();
		Packet->DirtyTileIndices.Reset();
		Packet->RemovedTiles.Reset();
		Packet->ContentHash = 0;
		Result.Failure = Failure;
		return Result;
	};

	if (!Input.WorldIdentity.IsValid())
	{
		return FailHeader(ESightWeaveSparsePacketFailure::InvalidWorldIdentity);
	}
	if (Input.PacketRevision == 0
		|| Input.RegistryRevision == 0
		|| Input.PublishedSnapshotRevision == 0)
	{
		return FailHeader(ESightWeaveSparsePacketFailure::InvalidRevision);
	}
	if (Input.PreviousPacket.IsValid()
		&& (!Input.PreviousPacket->IsValid()
			|| Input.PreviousPacket->GetWorldIdentity() != Input.WorldIdentity))
	{
		return FailHeader(ESightWeaveSparsePacketFailure::InvalidPreviousPacket);
	}

	TArray<FSightWeaveSparseScopeBuildInput> SortedScopes = Input.Scopes;
	SortedScopes.Sort(ScopeLess);
	for (int32 ScopeIndex = 0; ScopeIndex < SortedScopes.Num(); ++ScopeIndex)
	{
		const FSightWeaveSparseScopeBuildInput& ScopeInput = SortedScopes[ScopeIndex];
		FSightWeaveSparseRenderScope& Scope = Packet->Scopes.AddDefaulted_GetRef();
		Scope.ScopeKey.WorldIdentity = Input.WorldIdentity;
		Scope.ScopeKey.KnowledgeOwnerId = ScopeInput.KnowledgeOwnerId;
		Scope.ScopeKey.FloorId = ScopeInput.FloorId;
		Scope.ScopeKey.PrecisionTier = ScopeInput.PrecisionTier;
		Scope.ScopeKey.FloorOrigin = ScopeInput.FloorOrigin;
		Scope.MaximumActiveTiles = ScopeInput.MaximumActiveTiles > 0
			? ScopeInput.MaximumActiveTiles
			: SightWeaveDefaultActiveTileCapacity(ScopeInput.PrecisionTier);
		if (!Scope.ScopeKey.IsValid() || Scope.MaximumActiveTiles <= 0)
		{
			Scope.Failure = ESightWeaveSparsePacketFailure::InvalidScope;
			++Result.FailedScopeCount;
			continue;
		}
		if (ScopeIndex > 0)
		{
			const FSightWeaveSparseRenderScope& PreviousScope = Packet->Scopes[ScopeIndex - 1];
			if (Scope.ScopeKey.IsEquivalentTo(PreviousScope.ScopeKey))
			{
				Scope.Failure = ESightWeaveSparsePacketFailure::InvalidScope;
				++Result.FailedScopeCount;
				continue;
			}
		}

		TArray<FBox2D> PolygonBounds;
		PolygonBounds.SetNum(ScopeInput.Polygons.Num());
		TArray<FSightWeaveRenderProfileIdentity> Profiles;
		bool bInvalidPolygon = false;
		for (int32 PolygonIndex = 0; PolygonIndex < ScopeInput.Polygons.Num(); ++PolygonIndex)
		{
			const FSightWeaveSparsePolygonInput& Polygon = ScopeInput.Polygons[PolygonIndex];
			if (static_cast<uint8>(Polygon.Layer) >= static_cast<uint8>(ESightWeaveRenderMaskLayer::Count)
				|| !BuildPolygonBounds(Polygon, PolygonBounds[PolygonIndex]))
			{
				bInvalidPolygon = true;
				break;
			}
			if ((Polygon.Layer == ESightWeaveRenderMaskLayer::Vision
					|| Polygon.Layer == ESightWeaveRenderMaskLayer::Illumination)
				&& !AddUniqueProfile(Profiles, Polygon.CompatibilityProfile))
			{
				bInvalidPolygon = true;
				break;
			}
		}
		if (bInvalidPolygon || Profiles.Num() > SightWeave::SparseAtlas::MaximumActiveProfiles)
		{
			Scope.Failure = bInvalidPolygon
				? ESightWeaveSparsePacketFailure::InvalidPolygon
				: ESightWeaveSparsePacketFailure::InvalidProfile;
			++Result.FailedScopeCount;
			continue;
		}
		Profiles.Sort(ProfileLess);

		TArray<FIntPoint> LogicalTiles;
		const double InteriorWorldSpan = SightWeave::SparseAtlas::InteriorTileSize
			* static_cast<double>(SightWeaveCentimetersPerTexel(ScopeInput.PrecisionTier));
		bool bCapacityExceeded = false;
		for (const FBox2D& Bounds : PolygonBounds)
		{
			const double LocalMinX = (Bounds.Min.X - ScopeInput.FloorOrigin.X) / InteriorWorldSpan;
			const double LocalMinY = (Bounds.Min.Y - ScopeInput.FloorOrigin.Y) / InteriorWorldSpan;
			const double LocalMaxX = (Bounds.Max.X - ScopeInput.FloorOrigin.X - TileBoundaryBias) / InteriorWorldSpan;
			const double LocalMaxY = (Bounds.Max.Y - ScopeInput.FloorOrigin.Y - TileBoundaryBias) / InteriorWorldSpan;
			const int64 MinX = FMath::FloorToInt64(LocalMinX);
			const int64 MinY = FMath::FloorToInt64(LocalMinY);
			const int64 MaxX = FMath::FloorToInt64(LocalMaxX);
			const int64 MaxY = FMath::FloorToInt64(LocalMaxY);
			if (MinX < MIN_int32 || MinY < MIN_int32 || MaxX > MAX_int32 || MaxY > MAX_int32
				|| MaxX < MinX || MaxY < MinY)
			{
				bInvalidPolygon = true;
				break;
			}
			const uint64 TileWidth = static_cast<uint64>(MaxX - MinX) + 1;
			const uint64 TileHeight = static_cast<uint64>(MaxY - MinY) + 1;
			if (TileWidth > static_cast<uint64>(Scope.MaximumActiveTiles)
				|| TileHeight > static_cast<uint64>(Scope.MaximumActiveTiles)
				|| TileWidth * TileHeight > static_cast<uint64>(Scope.MaximumActiveTiles))
			{
				bCapacityExceeded = true;
				break;
			}
			for (int64 X = MinX; X <= MaxX; ++X)
			{
				for (int64 Y = MinY; Y <= MaxY; ++Y)
				{
					const FIntPoint Coordinate(static_cast<int32>(X), static_cast<int32>(Y));
					LogicalTiles.AddUnique(Coordinate);
					if (LogicalTiles.Num() > Scope.MaximumActiveTiles)
					{
						bCapacityExceeded = true;
						break;
					}
				}
				if (bCapacityExceeded)
				{
					break;
				}
			}
			if (bCapacityExceeded)
			{
				break;
			}
		}
		if (bInvalidPolygon)
		{
			Scope.Failure = ESightWeaveSparsePacketFailure::InvalidTile;
			++Result.FailedScopeCount;
			continue;
		}
		LogicalTiles.Sort(TileCoordinateLess);
		Scope.DesiredTileCount = LogicalTiles.Num();
		if (bCapacityExceeded || LogicalTiles.Num() > Scope.MaximumActiveTiles)
		{
			Scope.DesiredTileCount = FMath::Max(Scope.DesiredTileCount, Scope.MaximumActiveTiles + 1);
			Scope.Failure = ESightWeaveSparsePacketFailure::CapacityExceeded;
			++Result.FailedScopeCount;
			continue;
		}

		const bool bHadPreviousScope = Input.PreviousPacket.IsValid()
			&& Input.PreviousPacket->GetScopes().ContainsByPredicate([&Scope](
				const FSightWeaveSparseRenderScope& PreviousScope)
			{
				return PreviousScope.ScopeKey.IsEquivalentTo(Scope.ScopeKey)
					&& PreviousScope.IsValid();
			});
		Scope.bFullRebuild = Input.bForceFullRebuild || !bHadPreviousScope;
		Result.FullRebuildScopeCount += Scope.bFullRebuild;

		for (const FIntPoint& LogicalCoordinate : LogicalTiles)
		{
			FSightWeaveSparseRenderTile Tile;
			Tile.Identity.TileKey.Scope = Scope.ScopeKey;
			Tile.Identity.TileKey.LogicalCoordinate = LogicalCoordinate;
			Tile.CentimetersPerTexel = SightWeaveCentimetersPerTexel(ScopeInput.PrecisionTier);
			Tile.PhysicalWorldBounds = LogicalTileToPhysicalBounds(
				LogicalCoordinate,
				ScopeInput.FloorOrigin,
				ScopeInput.PrecisionTier);

			bool bTileFailed = false;
			for (const FSightWeaveRenderProfileIdentity& Profile : Profiles)
			{
				FSightWeaveRenderPacketBuildInput Legacy = MakeLegacyInput(
					Input,
					ScopeInput,
					Profile,
					LogicalCoordinate,
					Tile.PhysicalWorldBounds);
				for (int32 PolygonIndex = 0; PolygonIndex < ScopeInput.Polygons.Num(); ++PolygonIndex)
				{
					const FSightWeaveSparsePolygonInput& Polygon = ScopeInput.Polygons[PolygonIndex];
					if ((Polygon.Layer == ESightWeaveRenderMaskLayer::Vision
							|| Polygon.Layer == ESightWeaveRenderMaskLayer::Illumination)
						&& Polygon.CompatibilityProfile.IsEquivalentTo(Profile)
						&& BoundsOverlap(PolygonBounds[PolygonIndex], Tile.PhysicalWorldBounds))
					{
						AddLegacyPolygon(Legacy, Polygon);
					}
				}
				const FSightWeaveRenderPacketBuildResult Built = FSightWeaveRenderPacketBuilder::Build(Legacy);
				if (!Built.Succeeded())
				{
					bTileFailed = true;
					break;
				}
				FSightWeaveSparseProfileGeometry Geometry;
				Geometry.Identity = Profile;
				AppendLegacyRange(*Built.Packet, ESightWeaveRenderMaskLayer::Vision, Tile, Geometry.VisionRange);
				AppendLegacyRange(*Built.Packet, ESightWeaveRenderMaskLayer::Illumination, Tile, Geometry.IlluminationRange);
				if (!Geometry.VisionRange.IsEmpty() || !Geometry.IlluminationRange.IsEmpty())
				{
					Tile.Profiles.Add(MoveTemp(Geometry));
					Tile.Identity.CanonicalProfiles.Add(Profile);
				}
			}
			if (bTileFailed)
			{
				Scope.Failure = ESightWeaveSparsePacketFailure::InvalidPolygon;
				break;
			}

			FSightWeaveIlluminationCompatibilityProfile EmptyProfile;
			const FSightWeaveRenderProfileIdentity CommonProfile =
				FSightWeaveRenderProfileIdentity::FromProfile(EmptyProfile);
			FSightWeaveRenderPacketBuildInput Common = MakeLegacyInput(
				Input,
				ScopeInput,
				CommonProfile,
				LogicalCoordinate,
				Tile.PhysicalWorldBounds);
			for (int32 PolygonIndex = 0; PolygonIndex < ScopeInput.Polygons.Num(); ++PolygonIndex)
			{
				const FSightWeaveSparsePolygonInput& Polygon = ScopeInput.Polygons[PolygonIndex];
				if ((Polygon.Layer == ESightWeaveRenderMaskLayer::Bypass
						|| Polygon.Layer == ESightWeaveRenderMaskLayer::Suppression)
					&& BoundsOverlap(PolygonBounds[PolygonIndex], Tile.PhysicalWorldBounds))
				{
					AddLegacyPolygon(Common, Polygon);
				}
			}
			const FSightWeaveRenderPacketBuildResult CommonBuilt =
				FSightWeaveRenderPacketBuilder::Build(Common);
			if (!CommonBuilt.Succeeded())
			{
				Scope.Failure = ESightWeaveSparsePacketFailure::InvalidPolygon;
				break;
			}
			AppendLegacyRange(*CommonBuilt.Packet, ESightWeaveRenderMaskLayer::Bypass, Tile, Tile.BypassRange);
			AppendLegacyRange(*CommonBuilt.Packet, ESightWeaveRenderMaskLayer::Suppression, Tile, Tile.SuppressionRange);
			Tile.ContentHash = ComputeTileHash(Tile);
			Packet->Tiles.Add(MoveTemp(Tile));
		}

		if (!Scope.IsValid())
		{
			Packet->Tiles.RemoveAll([&Scope](const FSightWeaveSparseRenderTile& Tile)
			{
				return Tile.Identity.TileKey.Scope.IsEquivalentTo(Scope.ScopeKey);
			});
			++Result.FailedScopeCount;
		}
	}

	for (int32 TileIndex = 0; TileIndex < Packet->Tiles.Num(); ++TileIndex)
	{
		const FSightWeaveSparseRenderTile& Tile = Packet->Tiles[TileIndex];
		const FSightWeaveSparseRenderTile* Previous = Input.PreviousPacket.IsValid()
			? FindPreviousTile(*Input.PreviousPacket, Tile.Identity)
			: nullptr;
		const FSightWeaveSparseRenderScope* Scope = Packet->Scopes.FindByPredicate([&Tile](
			const FSightWeaveSparseRenderScope& Candidate)
		{
			return Candidate.ScopeKey.IsEquivalentTo(Tile.Identity.TileKey.Scope);
		});
		if (!Previous || Previous->ContentHash != Tile.ContentHash || (Scope && Scope->bFullRebuild))
		{
			Packet->DirtyTileIndices.Add(TileIndex);
		}
	}
	if (Input.PreviousPacket.IsValid())
	{
		for (const FSightWeaveSparseRenderTile& PreviousTile : Input.PreviousPacket->GetTiles())
		{
			if (!CurrentContainsIdentity(*Packet, PreviousTile.Identity))
			{
				Packet->RemovedTiles.Add(PreviousTile.Identity);
			}
		}
	}

	Packet->bValid = true;
	Packet->Failure = ESightWeaveSparsePacketFailure::None;
	const ESightWeaveSparsePacketFailure ValidationFailure = Validate(*Packet);
	if (ValidationFailure != ESightWeaveSparsePacketFailure::None)
	{
		return FailHeader(ValidationFailure);
	}
	uint64 PacketHash = FnvOffset;
	HashValue(PacketHash, Packet->WorldIdentity.Serial);
	HashValue(PacketHash, Packet->PacketRevision);
	HashValue(PacketHash, Packet->RegistryRevision);
	HashValue(PacketHash, Packet->PublishedSnapshotRevision);
	for (const FSightWeaveSparseRenderTile& Tile : Packet->Tiles)
	{
		HashValue(PacketHash, Tile.ContentHash);
	}
	for (const FSightWeaveSparseTileIdentity& Removed : Packet->RemovedTiles)
	{
		HashScope(PacketHash, Removed.TileKey.Scope);
		HashValue(PacketHash, Removed.TileKey.LogicalCoordinate.X);
		HashValue(PacketHash, Removed.TileKey.LogicalCoordinate.Y);
	}
	Packet->ContentHash = PacketHash == 0 ? 1 : PacketHash;
	Result.Failure = ESightWeaveSparsePacketFailure::None;
	Result.DirtyTileCount = Packet->DirtyTileIndices.Num();
	Result.RemovedTileCount = Packet->RemovedTiles.Num();
	return Result;
}

ESightWeaveSparsePacketFailure FSightWeaveSparseRenderPacketBuilder::Validate(
	const FSightWeaveSparseRenderPacket& Packet)
{
	if (!Packet.WorldIdentity.IsValid())
	{
		return ESightWeaveSparsePacketFailure::InvalidWorldIdentity;
	}
	if (Packet.PacketRevision == 0
		|| Packet.RegistryRevision == 0
		|| Packet.PublishedSnapshotRevision == 0)
	{
		return ESightWeaveSparsePacketFailure::InvalidRevision;
	}
	for (const int32 DirtyIndex : Packet.DirtyTileIndices)
	{
		if (!Packet.Tiles.IsValidIndex(DirtyIndex))
		{
			return ESightWeaveSparsePacketFailure::InvalidTile;
		}
	}
	for (const FSightWeaveSparseRenderTile& Tile : Packet.Tiles)
	{
		if (!Tile.Identity.IsValid()
			|| !Tile.PhysicalWorldBounds.bIsValid
			|| !IsFinite(Tile.PhysicalWorldBounds.Min)
			|| !IsFinite(Tile.PhysicalWorldBounds.Max)
			|| Tile.ContentHash == 0
			|| Tile.Indices.Num() % 3 != 0)
		{
			return ESightWeaveSparsePacketFailure::InvalidTile;
		}
		for (const uint32 Index : Tile.Indices)
		{
			if (Index >= static_cast<uint32>(Tile.Vertices.Num()))
			{
				return ESightWeaveSparsePacketFailure::InvalidTile;
			}
		}
	}
	for (const FSightWeaveSparseTileIdentity& Removed : Packet.RemovedTiles)
	{
		if (!Removed.IsValid())
		{
			return ESightWeaveSparsePacketFailure::InvalidTile;
		}
	}
	return ESightWeaveSparsePacketFailure::None;
}
