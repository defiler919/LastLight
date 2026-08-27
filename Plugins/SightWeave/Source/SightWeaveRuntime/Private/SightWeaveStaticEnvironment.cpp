#include "SightWeaveStaticEnvironment.h"

#include "Algo/Sort.h"
#include "SightWeaveGeometry.h"
#include "SightWeaveSettings.h"

namespace SightWeaveStaticEnvironmentPrivate
{
	constexpr double BoundaryBias = 1.0e-6;

	bool TileLess(const FSightWeaveStaticEnvironmentTile& A,
		const FSightWeaveStaticEnvironmentTile& B)
	{
		return A.Key.LogicalCoordinate.X < B.Key.LogicalCoordinate.X
			|| (A.Key.LogicalCoordinate.X == B.Key.LogicalCoordinate.X
				&& A.Key.LogicalCoordinate.Y < B.Key.LogicalCoordinate.Y);
	}

	bool BuildBounds(
		TConstArrayView<FVector2D> Vertices,
		FBox2D& OutBounds)
	{
		OutBounds = FBox2D(ForceInit);
		for (const FVector2D Vertex : Vertices)
		{
			if (!FMath::IsFinite(Vertex.X) || !FMath::IsFinite(Vertex.Y))
			{
				return false;
			}
			OutBounds += Vertex;
		}
		return Vertices.Num() >= 3 && OutBounds.bIsValid;
	}

	void Rasterize(
		const FSightWeaveStaticEnvironmentDescription& Description,
		const FSightWeaveMemoryScopeKey& Scope,
		FSightWeaveStaticEnvironmentTile& Tile)
	{
		const double CentimetersPerTexel = SightWeaveCentimetersPerTexel(Scope.PrecisionTier);
		const double InteriorSpan = SightWeave::Memory::InteriorTileSize * CentimetersPerTexel;
		const FVector2D TileMinimum = Scope.FloorOrigin
			+ FVector2D(Tile.Key.LogicalCoordinate) * InteriorSpan;
		TArray<double, TInlineAllocator<64>> Crossings;
		for (int32 Row = 0; Row < SightWeave::Memory::InteriorTileSize; ++Row)
		{
			const double SampleY = TileMinimum.Y + (Row + 0.5) * CentimetersPerTexel;
			Crossings.Reset();
			for (int32 Index = 0; Index < Description.WorldFootprint.Num(); ++Index)
			{
				const FVector2D A = Description.WorldFootprint[Index];
				const FVector2D B =
					Description.WorldFootprint[(Index + 1) % Description.WorldFootprint.Num()];
				if (!((A.Y <= SampleY && B.Y > SampleY)
					|| (B.Y <= SampleY && A.Y > SampleY)))
				{
					continue;
				}
				const double Alpha = (SampleY - A.Y) / (B.Y - A.Y);
				Crossings.Add(A.X + Alpha * (B.X - A.X));
			}
			Crossings.Sort();
			for (int32 Pair = 0; Pair + 1 < Crossings.Num(); Pair += 2)
			{
				const int32 FirstX = FMath::Clamp(
					FMath::CeilToInt(
						(Crossings[Pair] - TileMinimum.X) / CentimetersPerTexel
						- 0.5 - BoundaryBias),
					0,
					SightWeave::Memory::InteriorTileSize - 1);
				const int32 LastX = FMath::Clamp(
					FMath::FloorToInt(
						(Crossings[Pair + 1] - TileMinimum.X) / CentimetersPerTexel
						- 0.5 + BoundaryBias),
					0,
					SightWeave::Memory::InteriorTileSize - 1);
				for (int32 X = FirstX; X <= LastX; ++X)
				{
					uint8& Value = Tile.Attributes[
						Row * SightWeave::Memory::InteriorTileSize + X];
					Value = FMath::Max(Value, Description.NeutralIntensity);
				}
			}
		}
	}
}

using namespace SightWeaveStaticEnvironmentPrivate;

bool FSightWeaveStaticEnvironmentDescription::IsValid() const
{
	if (!KnowledgeOwnerId.IsValid()
		|| !FloorId.IsValid()
		|| !HeightRange.IsValid()
		|| NeutralIntensity == 0
		|| !bExplicitlyImmutable
		|| WorldFootprint.Num() < 3)
	{
		return false;
	}
	TArray<FVector> Vertices;
	Vertices.Reserve(WorldFootprint.Num());
	for (const FVector2D Vertex : WorldFootprint)
	{
		if (!FMath::IsFinite(Vertex.X) || !FMath::IsFinite(Vertex.Y))
		{
			return false;
		}
		Vertices.Emplace(Vertex, HeightRange.ZMin);
	}
	FSightWeaveGeometryTolerances Tolerances =
		GetDefault<USightWeaveSettings>()->GeometryTolerances;
	Tolerances.Normalize();
	return SightWeave::Geometry::IsSimplePolygon(Vertices, Tolerances);
}

bool FSightWeaveStaticEnvironmentDescription::IsEquivalentTo(
	const FSightWeaveStaticEnvironmentDescription& Other) const
{
	return KnowledgeOwnerId == Other.KnowledgeOwnerId
		&& FloorId == Other.FloorId
		&& HeightRange.ZMin == Other.HeightRange.ZMin
		&& HeightRange.ZMax == Other.HeightRange.ZMax
		&& WorldFootprint == Other.WorldFootprint
		&& NeutralIntensity == Other.NeutralIntensity
		&& bExplicitlyImmutable == Other.bExplicitlyImmutable
		&& bEnabled == Other.bEnabled;
}

bool FSightWeaveStaticEnvironmentTile::IsValid() const
{
	return Key.IsValid()
		&& Attributes.Num() == SightWeave::StaticEnvironment::BytesPerTile;
}

bool FSightWeaveStaticEnvironmentTile::IsEmpty() const
{
	return !Attributes.ContainsByPredicate([](const uint8 Value) { return Value != 0; });
}

uint8 FSightWeaveStaticEnvironmentTile::Sample(const FIntPoint InteriorTexel) const
{
	return IsValid()
		&& InteriorTexel.X >= 0
		&& InteriorTexel.X < SightWeave::Memory::InteriorTileSize
		&& InteriorTexel.Y >= 0
		&& InteriorTexel.Y < SightWeave::Memory::InteriorTileSize
		? Attributes[
			InteriorTexel.Y * SightWeave::Memory::InteriorTileSize + InteriorTexel.X]
		: 0;
}

bool FSightWeaveStaticEnvironmentAuthority::Configure(
	const FSightWeaveMemoryScopeKey& InScope,
	const int32 InMaximumTiles)
{
	check(IsInGameThread());
	if (!InScope.IsValid() || InMaximumTiles <= 0)
	{
		return false;
	}
	const FSightWeaveMemoryScopeKey PreviousScope = Scope;
	const int32 PreviousMaximumTiles = MaximumTiles;
	const bool bWasConfigured = bConfigured;
	Scope = InScope;
	MaximumTiles = InMaximumTiles;
	bConfigured = true;
	if (Rebuild())
	{
		return true;
	}
	Scope = PreviousScope;
	MaximumTiles = PreviousMaximumTiles;
	bConfigured = bWasConfigured;
	return false;
}

void FSightWeaveStaticEnvironmentAuthority::Disable()
{
	check(IsInGameThread());
	Scope = FSightWeaveMemoryScopeKey();
	Tiles.Reset();
	MaximumTiles = 0;
	bConfigured = false;
}

void FSightWeaveStaticEnvironmentAuthority::Reset()
{
	check(IsInGameThread());
	Disable();
	Records.Reset();
	NextHandle = 1;
	EligibilityRevision = 0;
	NextPacketRevision = 1;
}

FSightWeaveStaticEnvironmentHandle FSightWeaveStaticEnvironmentAuthority::Register(
	const FSightWeaveStaticEnvironmentDescription& Description)
{
	check(IsInGameThread());
	if (!Description.IsValid())
	{
		return FSightWeaveStaticEnvironmentHandle();
	}
	FRecord& Record = Records.AddDefaulted_GetRef();
	Record.Handle = FSightWeaveStaticEnvironmentHandle(NextHandle++);
	Record.Description = Description;
	if (bConfigured && !Rebuild())
	{
		Records.Pop();
		return FSightWeaveStaticEnvironmentHandle();
	}
	return Record.Handle;
}

bool FSightWeaveStaticEnvironmentAuthority::Update(
	const FSightWeaveStaticEnvironmentHandle Handle,
	const FSightWeaveStaticEnvironmentDescription& Description)
{
	check(IsInGameThread());
	FRecord* Record = Records.FindByPredicate(
		[Handle](const FRecord& Candidate) { return Candidate.Handle == Handle; });
	if (!Record || !Description.IsValid())
	{
		return false;
	}
	if (Record->Description.IsEquivalentTo(Description))
	{
		return true;
	}
	const FSightWeaveStaticEnvironmentDescription Prior = Record->Description;
	Record->Description = Description;
	if (bConfigured && !Rebuild())
	{
		Record->Description = Prior;
		return false;
	}
	return true;
}

bool FSightWeaveStaticEnvironmentAuthority::Unregister(
	const FSightWeaveStaticEnvironmentHandle Handle)
{
	check(IsInGameThread());
	const int32 Index = Records.IndexOfByPredicate(
		[Handle](const FRecord& Candidate) { return Candidate.Handle == Handle; });
	if (Index == INDEX_NONE)
	{
		return false;
	}
	const FRecord Removed = Records[Index];
	Records.RemoveAt(Index);
	if (bConfigured && !Rebuild())
	{
		Records.Insert(Removed, Index);
		return false;
	}
	return true;
}

bool FSightWeaveStaticEnvironmentAuthority::IsHandleValid(
	const FSightWeaveStaticEnvironmentHandle Handle) const
{
	return Handle.IsValid()
		&& Records.ContainsByPredicate(
			[Handle](const FRecord& Candidate) { return Candidate.Handle == Handle; });
}

bool FSightWeaveStaticEnvironmentAuthority::Rebuild()
{
	check(IsInGameThread());
	if (!bConfigured)
	{
		return false;
	}
	TArray<FSightWeaveStaticEnvironmentTile> Rebuilt;
	const double InteriorSpan = SightWeave::Memory::InteriorTileSize
		* SightWeaveCentimetersPerTexel(Scope.PrecisionTier);
	for (const FRecord& Record : Records)
	{
		const FSightWeaveStaticEnvironmentDescription& Description = Record.Description;
		if (!Description.bEnabled
			|| Description.KnowledgeOwnerId != Scope.KnowledgeOwnerId
			|| Description.FloorId != Scope.FloorId
			|| Scope.FloorPlaneZ < Description.HeightRange.ZMin
			|| Scope.FloorPlaneZ > Description.HeightRange.ZMax)
		{
			continue;
		}
		FBox2D Bounds;
		if (!BuildBounds(Description.WorldFootprint, Bounds))
		{
			return false;
		}
		const int64 MinX = FMath::FloorToInt64((Bounds.Min.X - Scope.FloorOrigin.X) / InteriorSpan);
		const int64 MinY = FMath::FloorToInt64((Bounds.Min.Y - Scope.FloorOrigin.Y) / InteriorSpan);
		const int64 MaxX = FMath::FloorToInt64(
			(Bounds.Max.X - Scope.FloorOrigin.X - BoundaryBias) / InteriorSpan);
		const int64 MaxY = FMath::FloorToInt64(
			(Bounds.Max.Y - Scope.FloorOrigin.Y - BoundaryBias) / InteriorSpan);
		if (MinX < MIN_int32 || MinY < MIN_int32 || MaxX > MAX_int32 || MaxY > MAX_int32)
		{
			return false;
		}
		for (int64 X = MinX; X <= MaxX; ++X)
		{
			for (int64 Y = MinY; Y <= MaxY; ++Y)
			{
				const FIntPoint Coordinate(static_cast<int32>(X), static_cast<int32>(Y));
				FSightWeaveStaticEnvironmentTile* Tile = Rebuilt.FindByPredicate(
					[Coordinate](const FSightWeaveStaticEnvironmentTile& Candidate)
					{
						return Candidate.Key.LogicalCoordinate == Coordinate;
					});
				if (!Tile)
				{
					FSightWeaveStaticEnvironmentTile& Added = Rebuilt.AddDefaulted_GetRef();
					Added.Key.Scope = Scope;
					Added.Key.LogicalCoordinate = Coordinate;
					Added.Attributes.SetNumZeroed(SightWeave::StaticEnvironment::BytesPerTile);
					Tile = &Added;
				}
				Rasterize(Description, Scope, *Tile);
			}
		}
	}
	Rebuilt.RemoveAll([](const FSightWeaveStaticEnvironmentTile& Tile) { return Tile.IsEmpty(); });
	Rebuilt.Sort(TileLess);
	if (Rebuilt.Num() > MaximumTiles)
	{
		return false;
	}
	bool bChanged = Rebuilt.Num() != Tiles.Num();
	for (int32 Index = 0; !bChanged && Index < Rebuilt.Num(); ++Index)
	{
		bChanged = !Rebuilt[Index].Key.IsEquivalentTo(Tiles[Index].Key)
			|| Rebuilt[Index].Attributes != Tiles[Index].Attributes;
	}
	if (bChanged)
	{
		Tiles = MoveTemp(Rebuilt);
		++EligibilityRevision;
	}
	return true;
}

TSharedPtr<const FSightWeaveStaticEnvironmentPacket, ESPMode::ThreadSafe>
FSightWeaveStaticEnvironmentAuthority::PublishPacket()
{
	check(IsInGameThread());
	TSharedRef<FSightWeaveStaticEnvironmentPacket, ESPMode::ThreadSafe> Packet =
		MakeShared<FSightWeaveStaticEnvironmentPacket, ESPMode::ThreadSafe>();
	Packet->Scope = Scope;
	Packet->EligibilityRevision = EligibilityRevision;
	Packet->PacketRevision = NextPacketRevision++;
	Packet->AttributeBytes = GetAttributeBytes();
	Packet->Tiles = Tiles;
	Packet->bValid = bConfigured && Scope.IsValid();
	return Packet;
}

int64 FSightWeaveStaticEnvironmentAuthority::GetAttributeBytes() const
{
	return static_cast<int64>(Tiles.Num()) * SightWeave::StaticEnvironment::BytesPerTile;
}
