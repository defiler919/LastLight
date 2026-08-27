#include "SightWeaveMemory.h"

#include "Algo/Sort.h"

namespace SightWeaveMemoryPrivate
{
	constexpr double TileBoundaryBias = 1.0e-6;

	bool IsFinite(const FVector2D& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y);
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
			if (A.CanonicalCapabilities[Index] != B.CanonicalCapabilities[Index])
			{
				return A.CanonicalCapabilities[Index].LexicalLess(B.CanonicalCapabilities[Index]);
			}
		}
		return A.CanonicalCapabilities.Num() < B.CanonicalCapabilities.Num();
	}

	bool ProfilesEqual(
		TConstArrayView<FSightWeaveRenderProfileIdentity> A,
		TConstArrayView<FSightWeaveRenderProfileIdentity> B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (!A[Index].IsEquivalentTo(B[Index]))
			{
				return false;
			}
		}
		return true;
	}

	bool ProfilesAreSubset(
		TConstArrayView<FSightWeaveRenderProfileIdentity> Candidates,
		TConstArrayView<FSightWeaveRenderProfileIdentity> Allowed)
	{
		for (const FSightWeaveRenderProfileIdentity& Candidate : Candidates)
		{
			if (!Allowed.ContainsByPredicate(
					[&Candidate](const FSightWeaveRenderProfileIdentity& Existing)
					{
						return Existing.IsEquivalentTo(Candidate);
					}))
			{
				return false;
			}
		}
		return true;
	}

	void AddCanonicalProfile(
		TArray<FSightWeaveRenderProfileIdentity>& Profiles,
		const FSightWeaveRenderProfileIdentity& Profile)
	{
		if (Profile.IsValid()
			&& !Profiles.ContainsByPredicate(
				[&Profile](const FSightWeaveRenderProfileIdentity& Existing)
				{
					return Existing.IsEquivalentTo(Profile);
				}))
		{
			Profiles.Add(Profile);
		}
	}

	void BuildCanonicalProfiles(
		const FSightWeaveFrameSnapshot& Snapshot,
		const FSightWeaveKnowledgeOwnerId Owner,
		const FSightWeaveFloorId Floor,
		TArray<FSightWeaveRenderProfileIdentity>& OutProfiles)
	{
		OutProfiles.Reset();
		for (const FSightWeaveVisionSnapshotEntry& Vision : Snapshot.VisionSources)
		{
			if (Vision.Description.bActive
				&& Vision.Description.KnowledgeOwnerId == Owner
				&& Vision.Description.FloorId == Floor)
			{
				AddCanonicalProfile(
					OutProfiles,
					FSightWeaveRenderProfileIdentity::FromProfile(
						Vision.Description.Compatibility));
			}
		}
		OutProfiles.Sort(ProfileLess);
	}

	bool TileCoordinateLess(const FIntPoint& A, const FIntPoint& B)
	{
		return A.X < B.X || (A.X == B.X && A.Y < B.Y);
	}

	void SetPackedRange(TArray<uint8>& Bits, const int32 Row, int32 FirstX, int32 LastX)
	{
		FirstX = FMath::Clamp(FirstX, 0, SightWeave::Memory::InteriorTileSize - 1);
		LastX = FMath::Clamp(LastX, 0, SightWeave::Memory::InteriorTileSize - 1);
		if (FirstX > LastX)
		{
			return;
		}
		const int32 RowOffset = Row * SightWeave::Memory::RowBytes;
		const int32 FirstByte = FirstX >> 3;
		const int32 LastByte = LastX >> 3;
		if (FirstByte == LastByte)
		{
			const uint8 FirstMask = static_cast<uint8>(0xffu << (FirstX & 7));
			const uint8 LastMask = static_cast<uint8>(0xffu >> (7 - (LastX & 7)));
			Bits[RowOffset + FirstByte] |= FirstMask & LastMask;
			return;
		}
		Bits[RowOffset + FirstByte] |= static_cast<uint8>(0xffu << (FirstX & 7));
		for (int32 ByteIndex = FirstByte + 1; ByteIndex < LastByte; ++ByteIndex)
		{
			Bits[RowOffset + ByteIndex] = 0xffu;
		}
		Bits[RowOffset + LastByte] |= static_cast<uint8>(0xffu >> (7 - (LastX & 7)));
	}

	void RasterizePolygon(
		TConstArrayView<FVector> Vertices,
		const FSightWeaveMemoryScopeKey& Scope,
		const FIntPoint LogicalCoordinate,
		TArray<uint8>& InOutBits)
	{
		if (Vertices.Num() < 3)
		{
			return;
		}
		const double CentimetersPerTexel = SightWeaveCentimetersPerTexel(Scope.PrecisionTier);
		const double InteriorSpan = SightWeave::Memory::InteriorTileSize * CentimetersPerTexel;
		const FVector2D TileMinimum = Scope.FloorOrigin
			+ FVector2D(LogicalCoordinate) * InteriorSpan;
		TArray<double, TInlineAllocator<64>> Crossings;
		for (int32 Row = 0; Row < SightWeave::Memory::InteriorTileSize; ++Row)
		{
			const double SampleY = TileMinimum.Y
				+ (static_cast<double>(Row) + 0.5) * CentimetersPerTexel;
			Crossings.Reset();
			for (int32 VertexIndex = 0; VertexIndex < Vertices.Num(); ++VertexIndex)
			{
				const FVector& A3 = Vertices[VertexIndex];
				const FVector& B3 = Vertices[(VertexIndex + 1) % Vertices.Num()];
				const double AY = A3.Y;
				const double BY = B3.Y;
				if (!((AY <= SampleY && BY > SampleY) || (BY <= SampleY && AY > SampleY)))
				{
					continue;
				}
				const double Alpha = (SampleY - AY) / (BY - AY);
				Crossings.Add(A3.X + Alpha * (B3.X - A3.X));
			}
			Crossings.Sort();
			for (int32 CrossingIndex = 0; CrossingIndex + 1 < Crossings.Num(); CrossingIndex += 2)
			{
				const double MinimumX = Crossings[CrossingIndex];
				const double MaximumX = Crossings[CrossingIndex + 1];
				const int32 FirstX = FMath::CeilToInt(
					(MinimumX - TileMinimum.X) / CentimetersPerTexel - 0.5 - TileBoundaryBias);
				const int32 LastX = FMath::FloorToInt(
					(MaximumX - TileMinimum.X) / CentimetersPerTexel - 0.5 + TileBoundaryBias);
				SetPackedRange(InOutBits, Row, FirstX, LastX);
			}
		}
	}

	bool BuildPolygonBounds(TConstArrayView<FVector> Vertices, FBox2D& OutBounds)
	{
		OutBounds = FBox2D(ForceInit);
		if (Vertices.Num() < 3)
		{
			return false;
		}
		for (const FVector& Vertex : Vertices)
		{
			if (!FMath::IsFinite(Vertex.X) || !FMath::IsFinite(Vertex.Y))
			{
				return false;
			}
			OutBounds += FVector2D(Vertex.X, Vertex.Y);
		}
		return OutBounds.bIsValid;
	}

	bool AddBoundsTiles(
		const FBox2D& Bounds,
		const FSightWeaveMemoryScopeKey& Scope,
		TArray<FIntPoint>& InOutTiles)
	{
		const double InteriorSpan =
			SightWeave::Memory::InteriorTileSize * SightWeaveCentimetersPerTexel(Scope.PrecisionTier);
		const int64 MinimumX = FMath::FloorToInt64((Bounds.Min.X - Scope.FloorOrigin.X) / InteriorSpan);
		const int64 MinimumY = FMath::FloorToInt64((Bounds.Min.Y - Scope.FloorOrigin.Y) / InteriorSpan);
		const int64 MaximumX = FMath::FloorToInt64(
			(Bounds.Max.X - Scope.FloorOrigin.X - TileBoundaryBias) / InteriorSpan);
		const int64 MaximumY = FMath::FloorToInt64(
			(Bounds.Max.Y - Scope.FloorOrigin.Y - TileBoundaryBias) / InteriorSpan);
		if (MinimumX < MIN_int32 || MinimumY < MIN_int32
			|| MaximumX > MAX_int32 || MaximumY > MAX_int32
			|| MaximumX < MinimumX || MaximumY < MinimumY)
		{
			return false;
		}
		for (int64 X = MinimumX; X <= MaximumX; ++X)
		{
			for (int64 Y = MinimumY; Y <= MaximumY; ++Y)
			{
				const FIntPoint Candidate(static_cast<int32>(X), static_cast<int32>(Y));
				InOutTiles.AddUnique(Candidate);
			}
		}
		return true;
	}

	struct FProfileMasks
	{
		FSightWeaveRenderProfileIdentity Profile;
		TArray<uint8> Vision;
		TArray<uint8> Illumination;
	};

	FProfileMasks& FindOrAddProfileMasks(
		TArray<FProfileMasks>& Masks,
		const FSightWeaveRenderProfileIdentity& Profile)
	{
		FProfileMasks* Existing = Masks.FindByPredicate(
			[&Profile](const FProfileMasks& Candidate)
			{
				return Candidate.Profile.IsEquivalentTo(Profile);
			});
		if (Existing)
		{
			return *Existing;
		}
		FProfileMasks& Added = Masks.AddDefaulted_GetRef();
		Added.Profile = Profile;
		Added.Vision.SetNumZeroed(SightWeave::Memory::PackedBytesPerTile);
		Added.Illumination.SetNumZeroed(SightWeave::Memory::PackedBytesPerTile);
		return Added;
	}
}

using namespace SightWeaveMemoryPrivate;

bool FSightWeaveMemoryScopeKey::IsValid() const
{
	if (!WorldIdentity.IsValid()
		|| WorldGeneration == 0
		|| !KnowledgeOwnerId.IsValid()
		|| !FloorId.IsValid()
		|| !IsFinite(FloorOrigin)
		|| SightWeaveCentimetersPerTexel(PrecisionTier) <= 0.0f)
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

bool FSightWeaveMemoryScopeKey::IsEquivalentTo(const FSightWeaveMemoryScopeKey& Other) const
{
	return WorldIdentity == Other.WorldIdentity
		&& WorldGeneration == Other.WorldGeneration
		&& KnowledgeOwnerId == Other.KnowledgeOwnerId
		&& FloorId == Other.FloorId
		&& FloorOrigin == Other.FloorOrigin
		&& PrecisionTier == Other.PrecisionTier
		&& ProfilesEqual(CanonicalProfiles, Other.CanonicalProfiles);
}

bool FSightWeaveMemoryTileKey::IsEquivalentTo(const FSightWeaveMemoryTileKey& Other) const
{
	return LogicalCoordinate == Other.LogicalCoordinate && Scope.IsEquivalentTo(Other.Scope);
}

bool FSightWeavePackedMemoryTile::IsValid() const
{
	return Key.IsValid() && PackedBits.Num() == SightWeave::Memory::PackedBytesPerTile;
}

bool FSightWeavePackedMemoryTile::IsEmpty() const
{
	return !PackedBits.ContainsByPredicate([](const uint8 Byte) { return Byte != 0; });
}

bool FSightWeavePackedMemoryTile::TestBit(const FIntPoint InteriorTexel) const
{
	if (!IsValid()
		|| InteriorTexel.X < 0 || InteriorTexel.X >= SightWeave::Memory::InteriorTileSize
		|| InteriorTexel.Y < 0 || InteriorTexel.Y >= SightWeave::Memory::InteriorTileSize)
	{
		return false;
	}
	const int32 ByteIndex =
		InteriorTexel.Y * SightWeave::Memory::RowBytes + (InteriorTexel.X >> 3);
	return (PackedBits[ByteIndex] & (1u << (InteriorTexel.X & 7))) != 0;
}

bool FSightWeaveMemoryAuthority::Configure(
	const FSightWeaveMemoryScopeKey& InScope,
	const int32 InMaximumTiles)
{
	check(IsInGameThread());
	Reset();
	if (!InScope.IsValid() || InMaximumTiles <= 0)
	{
		LastFailure = ESightWeaveMemoryFailure::InvalidScope;
		return false;
	}
	Scope = InScope;
	MaximumTiles = InMaximumTiles;
	bConfigured = true;
	bNeedsFullRebuild = true;
	LastFailure = ESightWeaveMemoryFailure::None;
	return true;
}

void FSightWeaveMemoryAuthority::Reset()
{
	check(IsInGameThread());
	Scope = FSightWeaveMemoryScopeKey();
	Tiles.Reset();
	DirtyLogicalTiles.Reset();
	RemovedTiles.Reset();
	MaximumTiles = 0;
	MemoryRevision = 0;
	LastSnapshotRevision = 0;
	NextPacketRevision = 1;
	LastFailure = ESightWeaveMemoryFailure::NotConfigured;
	bConfigured = false;
	bNeedsFullRebuild = false;
}

int64 FSightWeaveMemoryAuthority::GetPackedAuthorityBytes() const
{
	return static_cast<int64>(Tiles.Num()) * SightWeave::Memory::PackedBytesPerTile;
}

FSightWeaveMemoryUpdateDiagnostics FSightWeaveMemoryAuthority::WriteEffectiveLive(
	const FSightWeaveFrameSnapshot& Snapshot)
{
	check(IsInGameThread());
	FSightWeaveMemoryUpdateDiagnostics Result;
	Result.PriorMemoryRevision = MemoryRevision;
	Result.MemoryRevision = MemoryRevision;
	Result.SnapshotRevision = Snapshot.Revision.GetValue() > 0
		? static_cast<uint64>(Snapshot.Revision.GetValue())
		: 0;
	if (!bConfigured)
	{
		Result.Failure = LastFailure = ESightWeaveMemoryFailure::NotConfigured;
		return Result;
	}
	if (!Snapshot.bPublished || Result.SnapshotRevision == 0)
	{
		Result.Failure = LastFailure = ESightWeaveMemoryFailure::InvalidSnapshot;
		return Result;
	}
	if (Result.SnapshotRevision < LastSnapshotRevision)
	{
		Result.Failure = LastFailure = ESightWeaveMemoryFailure::StaleSnapshot;
		return Result;
	}
	if (Result.SnapshotRevision == LastSnapshotRevision)
	{
		Result.bDuplicateSnapshot = true;
		Result.AllocatedTileCount = Tiles.Num();
		Result.PackedAuthorityBytes = GetPackedAuthorityBytes();
		LastFailure = ESightWeaveMemoryFailure::None;
		return Result;
	}

	const FSightWeaveFloorDefinition* Floor = Snapshot.Floors.FindByPredicate(
		[this](const FSightWeaveFloorDefinition& Candidate)
		{
			return Candidate.FloorId == Scope.FloorId;
		});
	if (!Floor || !Floor->IsValid() || !Floor->bEnabled || !Floor->bActiveForQueries
		|| Floor->BoundsMin != Scope.FloorOrigin)
	{
		Result.Failure = LastFailure = ESightWeaveMemoryFailure::ScopeMismatch;
		return Result;
	}
	TArray<FSightWeaveRenderProfileIdentity> SnapshotProfiles;
	BuildCanonicalProfiles(Snapshot, Scope.KnowledgeOwnerId, Scope.FloorId, SnapshotProfiles);
	if (!ProfilesAreSubset(SnapshotProfiles, Scope.CanonicalProfiles))
	{
		Result.Failure = LastFailure = ESightWeaveMemoryFailure::ProfileMismatch;
		return Result;
	}

	TArray<FIntPoint> CandidateTiles;
	for (const FSightWeaveVisionSnapshotEntry& Vision : Snapshot.VisionSources)
	{
		if (!Vision.Description.bActive
			|| Vision.Description.KnowledgeOwnerId != Scope.KnowledgeOwnerId
			|| Vision.Description.FloorId != Scope.FloorId)
		{
			continue;
		}
		FBox2D Bounds(ForceInit);
		if (!BuildPolygonBounds(Vision.Polygon.Vertices, Bounds)
			|| !AddBoundsTiles(Bounds, Scope, CandidateTiles))
		{
			Result.Failure = LastFailure = ESightWeaveMemoryFailure::InvalidCoordinate;
			return Result;
		}
	}
	CandidateTiles.Sort(TileCoordinateLess);
	Result.CandidateTileCount = CandidateTiles.Num();
	if (CandidateTiles.Num() > MaximumTiles)
	{
		Result.Failure = LastFailure = ESightWeaveMemoryFailure::CapacityExceeded;
		return Result;
	}

	for (const FIntPoint LogicalCoordinate : CandidateTiles)
	{
		TArray<FProfileMasks> ProfileMasks;
		TArray<uint8> Bypass;
		TArray<uint8> Suppression;
		Bypass.SetNumZeroed(SightWeave::Memory::PackedBytesPerTile);
		Suppression.SetNumZeroed(SightWeave::Memory::PackedBytesPerTile);

		for (const FSightWeaveVisionSnapshotEntry& Vision : Snapshot.VisionSources)
		{
			if (!Vision.Description.bActive
				|| Vision.Description.KnowledgeOwnerId != Scope.KnowledgeOwnerId
				|| Vision.Description.FloorId != Scope.FloorId
				|| !Vision.Polygon.IsValid())
			{
				continue;
			}
			if (Vision.Description.IlluminationPolicy
				== ESightWeaveIlluminationPolicy::BypassLegalIllumination)
			{
				RasterizePolygon(Vision.Polygon.Vertices, Scope, LogicalCoordinate, Bypass);
				continue;
			}
			const FSightWeaveRenderProfileIdentity Profile =
				FSightWeaveRenderProfileIdentity::FromProfile(Vision.Description.Compatibility);
			FProfileMasks& Masks = FindOrAddProfileMasks(ProfileMasks, Profile);
			RasterizePolygon(Vision.Polygon.Vertices, Scope, LogicalCoordinate, Masks.Vision);
			for (const int32 IlluminationIndex : Vision.CompatibleIlluminationSourceIndices)
			{
				if (!Snapshot.IlluminationSources.IsValidIndex(IlluminationIndex))
				{
					Result.Failure = LastFailure = ESightWeaveMemoryFailure::InvalidSnapshot;
					return Result;
				}
				const FSightWeaveIlluminationSnapshotEntry& Illumination =
					Snapshot.IlluminationSources[IlluminationIndex];
				if (Illumination.Description.bActive
					&& Illumination.Description.KnowledgeOwnerId == Scope.KnowledgeOwnerId
					&& Illumination.Description.FloorId == Scope.FloorId
					&& Illumination.Polygon.IsValid())
				{
					RasterizePolygon(
						Illumination.Polygon.Vertices,
						Scope,
						LogicalCoordinate,
						Masks.Illumination);
				}
			}
		}
		for (const FSightWeaveHardSuppressionSnapshotEntry& HardSuppression : Snapshot.HardSuppressions)
		{
			const FSightWeaveHardSuppressionDescription& Description = HardSuppression.Description;
			if (!Description.bEnabled || Description.FloorId != Scope.FloorId)
			{
				continue;
			}
			constexpr int32 CircleSteps = 64;
			TArray<FVector, TInlineAllocator<CircleSteps>> Vertices;
			for (int32 Step = 0; Step < CircleSteps; ++Step)
			{
				const double Angle = 2.0 * PI * static_cast<double>(Step) / CircleSteps;
				Vertices.Emplace(
					Description.Center.X + FMath::Cos(Angle) * Description.Radius,
					Description.Center.Y + FMath::Sin(Angle) * Description.Radius,
					0.0);
			}
			RasterizePolygon(Vertices, Scope, LogicalCoordinate, Suppression);
		}

		TArray<uint8> Effective;
		Effective = Bypass;
		for (const FProfileMasks& Masks : ProfileMasks)
		{
			for (int32 ByteIndex = 0; ByteIndex < SightWeave::Memory::PackedBytesPerTile; ++ByteIndex)
			{
				Effective[ByteIndex] |= Masks.Vision[ByteIndex] & Masks.Illumination[ByteIndex];
			}
		}
		for (int32 ByteIndex = 0; ByteIndex < SightWeave::Memory::PackedBytesPerTile; ++ByteIndex)
		{
			Effective[ByteIndex] &= ~Suppression[ByteIndex];
		}
		if (!Effective.ContainsByPredicate([](const uint8 Byte) { return Byte != 0; }))
		{
			continue;
		}

		FSightWeavePackedMemoryTile* Tile = FindTile(LogicalCoordinate);
		if (!Tile)
		{
			if (Tiles.Num() >= MaximumTiles)
			{
				Result.Failure = LastFailure = ESightWeaveMemoryFailure::CapacityExceeded;
				return Result;
			}
			FSightWeavePackedMemoryTile& Added = Tiles.AddDefaulted_GetRef();
			Added.Key.Scope = Scope;
			Added.Key.LogicalCoordinate = LogicalCoordinate;
			Added.PackedBits.SetNumZeroed(SightWeave::Memory::PackedBytesPerTile);
			Tiles.Sort([](const FSightWeavePackedMemoryTile& A, const FSightWeavePackedMemoryTile& B)
			{
				return TileCoordinateLess(A.Key.LogicalCoordinate, B.Key.LogicalCoordinate);
			});
			Tile = FindTile(LogicalCoordinate);
		}
		bool bTileChanged = false;
		for (int32 ByteIndex = 0; ByteIndex < SightWeave::Memory::PackedBytesPerTile; ++ByteIndex)
		{
			const uint8 Prior = Tile->PackedBits[ByteIndex];
			Tile->PackedBits[ByteIndex] |= Effective[ByteIndex];
			bTileChanged |= Prior != Tile->PackedBits[ByteIndex];
		}
		if (bTileChanged)
		{
			DirtyLogicalTiles.AddUnique(LogicalCoordinate);
			++Result.ChangedTileCount;
		}
	}

	LastSnapshotRevision = Result.SnapshotRevision;
	if (Result.ChangedTileCount > 0)
	{
		++MemoryRevision;
		Result.bAuthorityChanged = true;
	}
	Result.MemoryRevision = MemoryRevision;
	Result.AllocatedTileCount = Tiles.Num();
	Result.DirtyTileCount = DirtyLogicalTiles.Num();
	Result.PackedAuthorityBytes = GetPackedAuthorityBytes();
	LastFailure = ESightWeaveMemoryFailure::None;
	return Result;
}

bool FSightWeaveMemoryAuthority::QueryHardMemory(const FVector WorldLocation) const
{
	return QueryHardMemory2D(FVector2D(WorldLocation.X, WorldLocation.Y));
}

bool FSightWeaveMemoryAuthority::QueryHardMemory2D(const FVector2D WorldLocation) const
{
	FIntPoint LogicalTile;
	FIntPoint InteriorTexel;
	if (!bConfigured
		|| !WorldToTileAndTexel(Scope, WorldLocation, LogicalTile, InteriorTexel))
	{
		return false;
	}
	const FSightWeavePackedMemoryTile* Tile = FindTile(LogicalTile);
	return Tile && Tile->TestBit(InteriorTexel);
}

TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe>
FSightWeaveMemoryAuthority::PublishPacket(const bool bForceFullRebuild)
{
	check(IsInGameThread());
	TSharedRef<FSightWeaveMemoryPacket, ESPMode::ThreadSafe> Packet =
		MakeShared<FSightWeaveMemoryPacket, ESPMode::ThreadSafe>();
	Packet->Scope = Scope;
	Packet->MemoryRevision = MemoryRevision;
	Packet->SnapshotRevision = LastSnapshotRevision;
	Packet->PacketRevision = NextPacketRevision++;
	Packet->PackedAuthorityBytes = GetPackedAuthorityBytes();
	Packet->bFullRebuild = bForceFullRebuild || bNeedsFullRebuild;
	if (!bConfigured || !Scope.IsValid())
	{
		Packet->Failure = ESightWeaveMemoryFailure::NotConfigured;
		return Packet;
	}
	if (Packet->bFullRebuild)
	{
		Packet->DirtyTiles = Tiles;
	}
	else
	{
		for (const FIntPoint Coordinate : DirtyLogicalTiles)
		{
			if (const FSightWeavePackedMemoryTile* Tile = FindTile(Coordinate))
			{
				Packet->DirtyTiles.Add(*Tile);
			}
		}
	}
	Packet->RemovedTiles = RemovedTiles;
	Packet->bValid = true;
	Packet->Failure = ESightWeaveMemoryFailure::None;
	DirtyLogicalTiles.Reset();
	RemovedTiles.Reset();
	bNeedsFullRebuild = false;
	return Packet;
}

bool FSightWeaveMemoryAuthority::BuildScopeForSnapshot(
	const FSightWeaveFrameSnapshot& Snapshot,
	const FSightWeaveRenderWorldIdentity WorldIdentity,
	const uint64 WorldGeneration,
	const FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
	const FSightWeaveFloorId FloorId,
	const ESightWeaveRenderPrecisionTier PrecisionTier,
	FSightWeaveMemoryScopeKey& OutScope)
{
	OutScope = FSightWeaveMemoryScopeKey();
	const FSightWeaveFloorDefinition* Floor = Snapshot.Floors.FindByPredicate(
		[FloorId](const FSightWeaveFloorDefinition& Candidate)
		{
			return Candidate.FloorId == FloorId;
		});
	if (!Snapshot.bPublished || !Floor || !Floor->IsValid() || !Floor->bEnabled)
	{
		return false;
	}
	OutScope.WorldIdentity = WorldIdentity;
	OutScope.WorldGeneration = WorldGeneration;
	OutScope.KnowledgeOwnerId = KnowledgeOwnerId;
	OutScope.FloorId = FloorId;
	OutScope.FloorOrigin = Floor->BoundsMin;
	OutScope.PrecisionTier = PrecisionTier;
	BuildCanonicalProfiles(Snapshot, KnowledgeOwnerId, FloorId, OutScope.CanonicalProfiles);
	return OutScope.IsValid();
}

bool FSightWeaveMemoryAuthority::WorldToTileAndTexel(
	const FSightWeaveMemoryScopeKey& InScope,
	const FVector2D WorldLocation,
	FIntPoint& OutLogicalTile,
	FIntPoint& OutInteriorTexel)
{
	if (!InScope.IsValid() || !IsFinite(WorldLocation))
	{
		return false;
	}
	const double CentimetersPerTexel = SightWeaveCentimetersPerTexel(InScope.PrecisionTier);
	const FVector2D Local = WorldLocation - InScope.FloorOrigin;
	const int64 TexelX = FMath::FloorToInt64(Local.X / CentimetersPerTexel);
	const int64 TexelY = FMath::FloorToInt64(Local.Y / CentimetersPerTexel);
	const int64 TileX = FMath::FloorToInt64(
		static_cast<double>(TexelX) / SightWeave::Memory::InteriorTileSize);
	const int64 TileY = FMath::FloorToInt64(
		static_cast<double>(TexelY) / SightWeave::Memory::InteriorTileSize);
	if (TileX < MIN_int32 || TileX > MAX_int32 || TileY < MIN_int32 || TileY > MAX_int32)
	{
		return false;
	}
	OutLogicalTile = FIntPoint(static_cast<int32>(TileX), static_cast<int32>(TileY));
	OutInteriorTexel = FIntPoint(
		static_cast<int32>(TexelX - TileX * SightWeave::Memory::InteriorTileSize),
		static_cast<int32>(TexelY - TileY * SightWeave::Memory::InteriorTileSize));
	return OutInteriorTexel.X >= 0
		&& OutInteriorTexel.X < SightWeave::Memory::InteriorTileSize
		&& OutInteriorTexel.Y >= 0
		&& OutInteriorTexel.Y < SightWeave::Memory::InteriorTileSize;
}

FSightWeavePackedMemoryTile* FSightWeaveMemoryAuthority::FindTile(const FIntPoint LogicalCoordinate)
{
	return Tiles.FindByPredicate(
		[LogicalCoordinate](const FSightWeavePackedMemoryTile& Tile)
		{
			return Tile.Key.LogicalCoordinate == LogicalCoordinate;
		});
}

const FSightWeavePackedMemoryTile* FSightWeaveMemoryAuthority::FindTile(
	const FIntPoint LogicalCoordinate) const
{
	return Tiles.FindByPredicate(
		[LogicalCoordinate](const FSightWeavePackedMemoryTile& Tile)
		{
			return Tile.Key.LogicalCoordinate == LogicalCoordinate;
		});
}
