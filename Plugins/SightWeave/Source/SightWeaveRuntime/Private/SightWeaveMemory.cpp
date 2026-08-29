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
		if (LastX < 0
			|| FirstX >= SightWeave::Memory::InteriorTileSize
			|| FirstX > LastX)
		{
			return;
		}
		FirstX = FMath::Clamp(FirstX, 0, SightWeave::Memory::InteriorTileSize - 1);
		LastX = FMath::Clamp(LastX, 0, SightWeave::Memory::InteriorTileSize - 1);
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

	bool BuildRegionPolygon(const FSightWeaveMemoryRegion& Region, TArray<FVector>& OutVertices)
	{
		OutVertices.Reset();
		if (!Region.IsValid())
		{
			return false;
		}
		switch (Region.Shape)
		{
		case ESightWeaveMemoryRegionShape::Circle:
		{
			constexpr int32 CircleSteps = 64;
			OutVertices.Reserve(CircleSteps);
			for (int32 Step = 0; Step < CircleSteps; ++Step)
			{
				const double Angle = 2.0 * PI * static_cast<double>(Step) / CircleSteps;
				OutVertices.Emplace(
					Region.Center.X + FMath::Cos(Angle) * Region.Radius,
					Region.Center.Y + FMath::Sin(Angle) * Region.Radius,
					0.0);
			}
			break;
		}
		case ESightWeaveMemoryRegionShape::AxisAlignedBox:
		case ESightWeaveMemoryRegionShape::RotatedBox:
		{
			const double Radians = Region.Shape == ESightWeaveMemoryRegionShape::RotatedBox
				? FMath::DegreesToRadians(static_cast<double>(Region.RotationDegrees))
				: 0.0;
			const double Cosine = FMath::Cos(Radians);
			const double Sine = FMath::Sin(Radians);
			const FVector2D Corners[4] =
			{
				FVector2D(-Region.HalfExtents.X, -Region.HalfExtents.Y),
				FVector2D(Region.HalfExtents.X, -Region.HalfExtents.Y),
				FVector2D(Region.HalfExtents.X, Region.HalfExtents.Y),
				FVector2D(-Region.HalfExtents.X, Region.HalfExtents.Y)
			};
			for (const FVector2D Corner : Corners)
			{
				const FVector2D Rotated(
					Corner.X * Cosine - Corner.Y * Sine,
					Corner.X * Sine + Corner.Y * Cosine);
				OutVertices.Emplace(Region.Center + Rotated, 0.0);
			}
			break;
		}
		case ESightWeaveMemoryRegionShape::Polygon:
			OutVertices.Reserve(Region.PolygonVertices.Num());
			for (const FVector2D Vertex : Region.PolygonVertices)
			{
				OutVertices.Emplace(Vertex, 0.0);
			}
			break;
		default:
			return false;
		}
		return OutVertices.Num() >= 3;
	}

	bool PointInRegion(const FSightWeaveMemoryRegion& Region, const FVector WorldLocation)
	{
		if (!Region.bEnabled
			|| WorldLocation.Z < Region.HeightRange.ZMin
			|| WorldLocation.Z > Region.HeightRange.ZMax)
		{
			return false;
		}
		if (Region.Shape == ESightWeaveMemoryRegionShape::Circle)
		{
			return FVector2D::DistSquared(
				Region.Center,
				FVector2D(WorldLocation.X, WorldLocation.Y))
				<= FMath::Square(static_cast<double>(Region.Radius));
		}
		TArray<FVector> Vertices;
		if (!BuildRegionPolygon(Region, Vertices))
		{
			return false;
		}
		const FVector2D Point(WorldLocation.X, WorldLocation.Y);
		bool bInside = false;
		for (int32 Index = 0, Previous = Vertices.Num() - 1; Index < Vertices.Num(); Previous = Index++)
		{
			const FVector2D A(Vertices[Index].X, Vertices[Index].Y);
			const FVector2D B(Vertices[Previous].X, Vertices[Previous].Y);
			const bool bCrosses = (A.Y > Point.Y) != (B.Y > Point.Y);
			if (bCrosses
				&& Point.X < (B.X - A.X) * (Point.Y - A.Y) / (B.Y - A.Y) + A.X)
			{
				bInside = !bInside;
			}
		}
		return bInside;
	}

	bool HeightRangesOverlap(
		const FSightWeaveHeightRange& A,
		const FSightWeaveHeightRange& B)
	{
		return A.ZMin <= B.ZMax && A.ZMax >= B.ZMin;
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

bool FSightWeaveMemoryScopeKey::IsValid() const
{
	if (!WorldIdentity.IsValid()
		|| WorldGeneration == 0
		|| !KnowledgeOwnerId.IsValid()
		|| !FloorId.IsValid()
		|| !SightWeaveMemoryPrivate::IsFinite(FloorOrigin)
		|| !FMath::IsFinite(FloorPlaneZ)
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
		&& FloorPlaneZ == Other.FloorPlaneZ
		&& PrecisionTier == Other.PrecisionTier
		&& SightWeaveMemoryPrivate::ProfilesEqual(CanonicalProfiles, Other.CanonicalProfiles);
}

bool FSightWeaveMemoryTileKey::IsEquivalentTo(const FSightWeaveMemoryTileKey& Other) const
{
	return LogicalCoordinate == Other.LogicalCoordinate && Scope.IsEquivalentTo(Other.Scope);
}

bool FSightWeaveMemoryRegion::IsValid() const
{
	if (!Scope.IsValid() || !HeightRange.IsValid() || !SightWeaveMemoryPrivate::IsFinite(Center)
		|| !SightWeaveMemoryPrivate::IsFinite(HalfExtents) || !FMath::IsFinite(Radius)
		|| !FMath::IsFinite(RotationDegrees))
	{
		return false;
	}
	switch (Shape)
	{
	case ESightWeaveMemoryRegionShape::Circle:
		return Radius > 0.0f;
	case ESightWeaveMemoryRegionShape::AxisAlignedBox:
	case ESightWeaveMemoryRegionShape::RotatedBox:
		return HalfExtents.X > 0.0 && HalfExtents.Y > 0.0;
	case ESightWeaveMemoryRegionShape::Polygon:
		return PolygonVertices.Num() >= 3
			&& !PolygonVertices.ContainsByPredicate(
				[](const FVector2D& Vertex)
				{
					return !SightWeaveMemoryPrivate::IsFinite(Vertex);
				});
	default:
		return false;
	}
}

bool FSightWeaveMemoryRegion::IsEquivalentTo(const FSightWeaveMemoryRegion& Other) const
{
	return Scope.IsEquivalentTo(Other.Scope)
		&& HeightRange.ZMin == Other.HeightRange.ZMin
		&& HeightRange.ZMax == Other.HeightRange.ZMax
		&& Shape == Other.Shape
		&& Center == Other.Center
		&& HalfExtents == Other.HalfExtents
		&& Radius == Other.Radius
		&& RotationDegrees == Other.RotationDegrees
		&& PolygonVertices == Other.PolygonVertices
		&& bEnabled == Other.bEnabled;
}

bool FSightWeaveMemoryRegion::ContainsWorldLocation(const FVector WorldLocation) const
{
	return IsValid() && SightWeaveMemoryPrivate::PointInRegion(*this, WorldLocation);
}

bool FSightWeaveMemoryModifierDescription::IsEquivalentTo(
	const FSightWeaveMemoryModifierDescription& Other) const
{
	return Operation == Other.Operation
		&& Region.IsEquivalentTo(Other.Region)
		&& Persistence == Other.Persistence
		&& StablePersistenceId == Other.StablePersistenceId;
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
	PersistenceGuardRevision = 1;
	LastFailure = ESightWeaveMemoryFailure::None;
	return true;
}

void FSightWeaveMemoryAuthority::Reset()
{
	check(IsInGameThread());
	Scope = FSightWeaveMemoryScopeKey();
	Tiles.Reset();
	PublishedAuthorityTiles.Reset();
	Modifiers.Reset();
	DirtyLogicalTiles.Reset();
	RemovedTiles.Reset();
	MaximumTiles = 0;
	MemoryRevision = 0;
	LastSnapshotRevision = 0;
	NextPacketRevision = 1;
	ModifierRevision = 0;
	PersistenceGuardRevision = 0;
	NextModifierId = 1;
	LastFailure = ESightWeaveMemoryFailure::NotConfigured;
	bConfigured = false;
	bNeedsFullRebuild = false;
	bModifierStateDirty = false;
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
	SightWeaveMemoryPrivate::BuildCanonicalProfiles(
		Snapshot,
		Scope.KnowledgeOwnerId,
		Scope.FloorId,
		SnapshotProfiles);
	if (!SightWeaveMemoryPrivate::ProfilesAreSubset(SnapshotProfiles, Scope.CanonicalProfiles))
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
		if (!SightWeaveMemoryPrivate::BuildPolygonBounds(Vision.Polygon.Vertices, Bounds)
			|| !SightWeaveMemoryPrivate::AddBoundsTiles(Bounds, Scope, CandidateTiles))
		{
			Result.Failure = LastFailure = ESightWeaveMemoryFailure::InvalidCoordinate;
			return Result;
		}
	}
	CandidateTiles.Sort(SightWeaveMemoryPrivate::TileCoordinateLess);
	Result.CandidateTileCount = CandidateTiles.Num();
	if (CandidateTiles.Num() > MaximumTiles)
	{
		Result.Failure = LastFailure = ESightWeaveMemoryFailure::CapacityExceeded;
		return Result;
	}

	for (const FIntPoint LogicalCoordinate : CandidateTiles)
	{
		TArray<SightWeaveMemoryPrivate::FProfileMasks> ProfileMasks;
		TArray<uint8> Bypass;
		TArray<uint8> Suppression;
		TArray<uint8> WriteBlock;
		Bypass.SetNumZeroed(SightWeave::Memory::PackedBytesPerTile);
		Suppression.SetNumZeroed(SightWeave::Memory::PackedBytesPerTile);
		WriteBlock.SetNumZeroed(SightWeave::Memory::PackedBytesPerTile);

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
				SightWeaveMemoryPrivate::RasterizePolygon(
					Vision.Polygon.Vertices,
					Scope,
					LogicalCoordinate,
					Bypass);
				continue;
			}
			const FSightWeaveRenderProfileIdentity Profile =
				FSightWeaveRenderProfileIdentity::FromProfile(Vision.Description.Compatibility);
			SightWeaveMemoryPrivate::FProfileMasks& Masks =
				SightWeaveMemoryPrivate::FindOrAddProfileMasks(ProfileMasks, Profile);
			SightWeaveMemoryPrivate::RasterizePolygon(
				Vision.Polygon.Vertices,
				Scope,
				LogicalCoordinate,
				Masks.Vision);
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
					SightWeaveMemoryPrivate::RasterizePolygon(
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
			SightWeaveMemoryPrivate::RasterizePolygon(
				Vertices,
				Scope,
				LogicalCoordinate,
				Suppression);
		}
		for (const FModifierRecord& Modifier : Modifiers)
		{
			const FSightWeaveMemoryRegion& Region = Modifier.Description.Region;
			if (Modifier.Description.Operation
					!= ESightWeaveMemoryModifierOperation::BlockMemoryWrites
				|| !Region.bEnabled
				|| !Region.Scope.IsEquivalentTo(Scope)
				|| !SightWeaveMemoryPrivate::HeightRangesOverlap(
					Region.HeightRange,
					Floor->HeightRange))
			{
				continue;
			}
			TArray<FVector> RegionVertices;
			if (SightWeaveMemoryPrivate::BuildRegionPolygon(Region, RegionVertices))
			{
				SightWeaveMemoryPrivate::RasterizePolygon(
					RegionVertices,
					Scope,
					LogicalCoordinate,
					WriteBlock);
			}
		}

		TArray<uint8> Effective;
		Effective = Bypass;
		for (const SightWeaveMemoryPrivate::FProfileMasks& Masks : ProfileMasks)
		{
			for (int32 ByteIndex = 0; ByteIndex < SightWeave::Memory::PackedBytesPerTile; ++ByteIndex)
			{
				Effective[ByteIndex] |= Masks.Vision[ByteIndex] & Masks.Illumination[ByteIndex];
			}
		}
		for (int32 ByteIndex = 0; ByteIndex < SightWeave::Memory::PackedBytesPerTile; ++ByteIndex)
		{
			Effective[ByteIndex] &= ~Suppression[ByteIndex];
			Effective[ByteIndex] &= ~WriteBlock[ByteIndex];
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
				return SightWeaveMemoryPrivate::TileCoordinateLess(
					A.Key.LogicalCoordinate,
					B.Key.LogicalCoordinate);
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
		PublishedAuthorityTiles.Reset();
		Result.bAuthorityChanged = true;
	}
	Result.MemoryRevision = MemoryRevision;
	Result.AllocatedTileCount = Tiles.Num();
	Result.DirtyTileCount = DirtyLogicalTiles.Num();
	Result.PackedAuthorityBytes = GetPackedAuthorityBytes();
	LastFailure = ESightWeaveMemoryFailure::None;
	++PersistenceGuardRevision;
	return Result;
}

bool FSightWeaveMemoryAuthority::ClearMemory(const FSightWeaveMemoryRegion& Region)
{
	check(IsInGameThread());
	if (!bConfigured || !Region.IsValid() || !Region.bEnabled
		|| !Region.Scope.IsEquivalentTo(Scope))
	{
		LastFailure = ESightWeaveMemoryFailure::ScopeMismatch;
		return false;
	}
	TArray<FVector> RegionVertices;
	FBox2D Bounds(ForceInit);
	if (!SightWeaveMemoryPrivate::BuildRegionPolygon(Region, RegionVertices)
		|| !SightWeaveMemoryPrivate::BuildPolygonBounds(RegionVertices, Bounds))
	{
		LastFailure = ESightWeaveMemoryFailure::InvalidCoordinate;
		return false;
	}
	bool bChanged = false;
	for (int32 TileIndex = Tiles.Num() - 1; TileIndex >= 0; --TileIndex)
	{
		FSightWeavePackedMemoryTile& Tile = Tiles[TileIndex];
		const FBox2D TileBounds = FSightWeaveSparseRenderPacketBuilder::LogicalTileToPhysicalBounds(
			Tile.Key.LogicalCoordinate,
			Scope.FloorOrigin,
			Scope.PrecisionTier);
		if (!Bounds.Intersect(TileBounds))
		{
			continue;
		}
		TArray<uint8> ClearMask;
		ClearMask.SetNumZeroed(SightWeave::Memory::PackedBytesPerTile);
		SightWeaveMemoryPrivate::RasterizePolygon(
			RegionVertices,
			Scope,
			Tile.Key.LogicalCoordinate,
			ClearMask);
		bool bTileChanged = false;
		for (int32 ByteIndex = 0; ByteIndex < SightWeave::Memory::PackedBytesPerTile; ++ByteIndex)
		{
			const uint8 Prior = Tile.PackedBits[ByteIndex];
			Tile.PackedBits[ByteIndex] &= ~ClearMask[ByteIndex];
			bTileChanged |= Prior != Tile.PackedBits[ByteIndex];
		}
		if (!bTileChanged)
		{
			continue;
		}
		bChanged = true;
		if (Tile.IsEmpty())
		{
			RemovedTiles.Add(Tile.Key);
			DirtyLogicalTiles.Remove(Tile.Key.LogicalCoordinate);
			Tiles.RemoveAt(TileIndex, 1, EAllowShrinking::No);
		}
		else
		{
			DirtyLogicalTiles.AddUnique(Tile.Key.LogicalCoordinate);
		}
	}
	if (bChanged)
	{
		++MemoryRevision;
		++PersistenceGuardRevision;
		PublishedAuthorityTiles.Reset();
	}
	LastFailure = ESightWeaveMemoryFailure::None;
	return true;
}

FSightWeaveMemoryModifierHandle FSightWeaveMemoryAuthority::RegisterModifier(
	const FSightWeaveMemoryModifierDescription& Description)
{
	check(IsInGameThread());
	if (!bConfigured || !Description.IsValid()
		|| !Description.Region.Scope.IsEquivalentTo(Scope))
	{
		LastFailure = ESightWeaveMemoryFailure::ScopeMismatch;
		return FSightWeaveMemoryModifierHandle();
	}
	if (Description.Persistence == ESightWeaveMemoryModifierPersistence::Persistent
		&& !Description.StablePersistenceId.IsNone()
		&& Modifiers.ContainsByPredicate(
			[&Description](const FModifierRecord& Candidate)
			{
				return Candidate.Description.Persistence
						== ESightWeaveMemoryModifierPersistence::Persistent
					&& Candidate.Description.StablePersistenceId
						== Description.StablePersistenceId;
			}))
	{
		LastFailure = ESightWeaveMemoryFailure::DuplicatePersistentModifier;
		return FSightWeaveMemoryModifierHandle();
	}
	FModifierRecord& Added = Modifiers.AddDefaulted_GetRef();
	Added.Handle = FSightWeaveMemoryModifierHandle(NextModifierId++);
	Added.Description = Description;
	Modifiers.Sort([](const FModifierRecord& A, const FModifierRecord& B)
	{
		return A.Handle.GetValue() < B.Handle.GetValue();
	});
	++ModifierRevision;
	++PersistenceGuardRevision;
	bModifierStateDirty = true;
	LastFailure = ESightWeaveMemoryFailure::None;
	return Added.Handle;
}

bool FSightWeaveMemoryAuthority::UpdateModifier(
	const FSightWeaveMemoryModifierHandle Handle,
	const FSightWeaveMemoryModifierDescription& Description)
{
	check(IsInGameThread());
	if (!Handle.IsValid() || !Description.IsValid()
		|| !Description.Region.Scope.IsEquivalentTo(Scope))
	{
		return false;
	}
	FModifierRecord* Existing = Modifiers.FindByPredicate(
		[Handle](const FModifierRecord& Candidate) { return Candidate.Handle == Handle; });
	if (!Existing)
	{
		return false;
	}
	if (Description.Persistence == ESightWeaveMemoryModifierPersistence::Persistent
		&& !Description.StablePersistenceId.IsNone()
		&& Modifiers.ContainsByPredicate(
			[Handle, &Description](const FModifierRecord& Candidate)
			{
				return Candidate.Handle != Handle
					&& Candidate.Description.Persistence
						== ESightWeaveMemoryModifierPersistence::Persistent
					&& Candidate.Description.StablePersistenceId
						== Description.StablePersistenceId;
			}))
	{
		LastFailure = ESightWeaveMemoryFailure::DuplicatePersistentModifier;
		return false;
	}
	if (Existing->Description.IsEquivalentTo(Description))
	{
		return true;
	}
	Existing->Description = Description;
	++ModifierRevision;
	++PersistenceGuardRevision;
	bModifierStateDirty = true;
	return true;
}

bool FSightWeaveMemoryAuthority::UnregisterModifier(
	const FSightWeaveMemoryModifierHandle Handle)
{
	check(IsInGameThread());
	const int32 Removed = Modifiers.RemoveAll(
		[Handle](const FModifierRecord& Candidate) { return Candidate.Handle == Handle; });
	if (Removed == 0)
	{
		return false;
	}
	++ModifierRevision;
	++PersistenceGuardRevision;
	bModifierStateDirty = true;
	return true;
}

bool FSightWeaveMemoryAuthority::IsModifierHandleValid(
	const FSightWeaveMemoryModifierHandle Handle) const
{
	return Handle.IsValid()
		&& Modifiers.ContainsByPredicate(
			[Handle](const FModifierRecord& Candidate) { return Candidate.Handle == Handle; });
}

bool FSightWeaveMemoryAuthority::IsMemoryPresentationSuppressed(
	const FVector WorldLocation) const
{
	if (!bConfigured)
	{
		return true;
	}
	for (const FModifierRecord& Modifier : Modifiers)
	{
		if ((Modifier.Description.Operation
				== ESightWeaveMemoryModifierOperation::BlockMemoryWrites
				|| Modifier.Description.Operation
					== ESightWeaveMemoryModifierOperation::SuppressMemoryPresentation)
			&& Modifier.Description.Region.Scope.IsEquivalentTo(Scope)
			&& SightWeaveMemoryPrivate::PointInRegion(
				Modifier.Description.Region,
				WorldLocation))
		{
			return true;
		}
	}
	return false;
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

ESightWeaveMemoryFailure FSightWeaveMemoryAuthority::ExportPersistentState(
	FSightWeaveMemoryPersistentState& OutState) const
{
	check(IsInGameThread());
	OutState = FSightWeaveMemoryPersistentState();
	if (!bConfigured || !Scope.IsValid())
	{
		return ESightWeaveMemoryFailure::NotConfigured;
	}
	OutState.Scope = Scope;
	OutState.Tiles = Tiles;
	for (const FModifierRecord& Modifier : Modifiers)
	{
		if (Modifier.Description.Persistence
			!= ESightWeaveMemoryModifierPersistence::Persistent)
		{
			continue;
		}
		if (!Modifier.Description.IsValid()
			|| !Modifier.Description.HasValidPersistenceMetadata()
			|| !Modifier.Description.Region.Scope.IsEquivalentTo(Scope))
		{
			OutState = FSightWeaveMemoryPersistentState();
			return ESightWeaveMemoryFailure::InvalidPersistentState;
		}
		if (OutState.PersistentModifiers.ContainsByPredicate(
			[&Modifier](const FSightWeaveMemoryModifierDescription& Existing)
			{
				return Existing.StablePersistenceId
					== Modifier.Description.StablePersistenceId;
			}))
		{
			OutState = FSightWeaveMemoryPersistentState();
			return ESightWeaveMemoryFailure::DuplicatePersistentModifier;
		}
		OutState.PersistentModifiers.Add(Modifier.Description);
	}
	OutState.PersistentModifiers.Sort(
		[](const FSightWeaveMemoryModifierDescription& A,
			const FSightWeaveMemoryModifierDescription& B)
		{
			return A.StablePersistenceId.ToString().ToLower()
				< B.StablePersistenceId.ToString().ToLower();
		});
	return ESightWeaveMemoryFailure::None;
}

ESightWeaveMemoryFailure FSightWeaveMemoryAuthority::PreparePersistentReplacement(
	const FSightWeaveMemoryPersistentState& State)
{
	check(IsInGameThread());
	if (!bConfigured || !State.Scope.IsValid()
		|| !State.Scope.IsEquivalentTo(Scope)
		|| State.Tiles.Num() > MaximumTiles)
	{
		return ESightWeaveMemoryFailure::InvalidPersistentState;
	}
	TSet<FIntPoint> Coordinates;
	for (const FSightWeavePackedMemoryTile& Tile : State.Tiles)
	{
		if (!Tile.IsValid() || Tile.IsEmpty()
			|| !Tile.Key.Scope.IsEquivalentTo(Scope)
			|| Coordinates.Contains(Tile.Key.LogicalCoordinate))
		{
			return ESightWeaveMemoryFailure::InvalidPersistentState;
		}
		Coordinates.Add(Tile.Key.LogicalCoordinate);
	}
	TSet<FName> StableModifierIds;
	for (const FSightWeaveMemoryModifierDescription& Modifier : State.PersistentModifiers)
	{
		if (!Modifier.IsValid() || !Modifier.HasValidPersistenceMetadata()
			|| Modifier.Persistence != ESightWeaveMemoryModifierPersistence::Persistent
			|| !Modifier.Region.Scope.IsEquivalentTo(Scope))
		{
			return ESightWeaveMemoryFailure::InvalidPersistentState;
		}
		if (StableModifierIds.Contains(Modifier.StablePersistenceId))
		{
			return ESightWeaveMemoryFailure::DuplicatePersistentModifier;
		}
		StableModifierIds.Add(Modifier.StablePersistenceId);
	}

	Tiles = State.Tiles;
	Tiles.Sort([](const FSightWeavePackedMemoryTile& A, const FSightWeavePackedMemoryTile& B)
	{
		return SightWeaveMemoryPrivate::TileCoordinateLess(
			A.Key.LogicalCoordinate,
			B.Key.LogicalCoordinate);
	});
	Modifiers.RemoveAll([](const FModifierRecord& Record)
	{
		return Record.Description.Persistence
			== ESightWeaveMemoryModifierPersistence::Persistent;
	});
	for (const FSightWeaveMemoryModifierDescription& Modifier : State.PersistentModifiers)
	{
		FModifierRecord& Added = Modifiers.AddDefaulted_GetRef();
		Added.Handle = FSightWeaveMemoryModifierHandle(NextModifierId++);
		Added.Description = Modifier;
	}
	Modifiers.Sort([](const FModifierRecord& A, const FModifierRecord& B)
	{
		return A.Handle.GetValue() < B.Handle.GetValue();
	});
	++MemoryRevision;
	++ModifierRevision;
	++PersistenceGuardRevision;
	PublishedAuthorityTiles.Reset();
	DirtyLogicalTiles.Reset();
	RemovedTiles.Reset();
	bNeedsFullRebuild = true;
	bModifierStateDirty = true;
	LastFailure = ESightWeaveMemoryFailure::None;
	return ESightWeaveMemoryFailure::None;
}

void FSightWeaveMemoryAuthority::FinalizePreparedPersistentReplacement(
	const uint64 PriorMemoryRevision,
	const uint64 PriorModifierRevision,
	const uint64 PriorGuardRevision)
{
	check(IsInGameThread());
	MemoryRevision = PriorMemoryRevision + 1;
	ModifierRevision = PriorModifierRevision + 1;
	PersistenceGuardRevision = PriorGuardRevision + 1;
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
	Packet->ModifierRevision = ModifierRevision;
	Packet->PackedAuthorityBytes = GetPackedAuthorityBytes();
	Packet->bFullRebuild = bForceFullRebuild || bNeedsFullRebuild;
	Packet->bModifierStateChanged = bModifierStateDirty;
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
	if (!PublishedAuthorityTiles.IsValid())
	{
		PublishedAuthorityTiles =
			MakeShared<TArray<FSightWeavePackedMemoryTile>, ESPMode::ThreadSafe>(Tiles);
	}
	Packet->AuthorityTiles = PublishedAuthorityTiles;
	Packet->RemovedTiles = RemovedTiles;
	for (const FModifierRecord& Modifier : Modifiers)
	{
		if (Modifier.Description.Region.bEnabled
			&& (Modifier.Description.Operation
					== ESightWeaveMemoryModifierOperation::BlockMemoryWrites
				|| Modifier.Description.Operation
					== ESightWeaveMemoryModifierOperation::SuppressMemoryPresentation))
		{
			Packet->PresentationSuppressions.Add(Modifier.Description);
		}
	}
	Packet->bValid = true;
	Packet->Failure = ESightWeaveMemoryFailure::None;
	DirtyLogicalTiles.Reset();
	RemovedTiles.Reset();
	bNeedsFullRebuild = false;
	bModifierStateDirty = false;
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
	OutScope.FloorPlaneZ = Floor->HeightRange.ZMin;
	OutScope.PrecisionTier = PrecisionTier;
	SightWeaveMemoryPrivate::BuildCanonicalProfiles(
		Snapshot,
		KnowledgeOwnerId,
		FloorId,
		OutScope.CanonicalProfiles);
	return OutScope.IsValid();
}

bool FSightWeaveMemoryAuthority::WorldToTileAndTexel(
	const FSightWeaveMemoryScopeKey& InScope,
	const FVector2D WorldLocation,
	FIntPoint& OutLogicalTile,
	FIntPoint& OutInteriorTexel)
{
	if (!InScope.IsValid() || !SightWeaveMemoryPrivate::IsFinite(WorldLocation))
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
