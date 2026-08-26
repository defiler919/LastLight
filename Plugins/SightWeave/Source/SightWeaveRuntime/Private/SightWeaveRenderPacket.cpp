#include "SightWeaveRenderPacket.h"

#include "Algo/Reverse.h"
#include "Algo/Unique.h"
#include "Containers/StringConv.h"

namespace
{
	constexpr double DuplicateEpsilonSquared = 1.0e-4;
	constexpr double CollinearDistanceEpsilon = 1.0e-4;
	constexpr double AreaEpsilon = 1.0e-6;
	constexpr uint64 FnvOffset = 14695981039346656037ull;
	constexpr uint64 FnvPrime = 1099511628211ull;

	bool IsFinite(const FVector2D& Point)
	{
		return FMath::IsFinite(Point.X) && FMath::IsFinite(Point.Y);
	}

	double Cross(const FVector2D& A, const FVector2D& B, const FVector2D& C)
	{
		return (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
	}

	double SignedDoubleArea(TConstArrayView<FVector2D> Polygon)
	{
		double Area = 0.0;
		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			const FVector2D& A = Polygon[Index];
			const FVector2D& B = Polygon[(Index + 1) % Polygon.Num()];
			Area += A.X * B.Y - A.Y * B.X;
		}
		return Area;
	}

	bool LexicalPointLess(const FVector2D& A, const FVector2D& B)
	{
		return A.X < B.X || (A.X == B.X && A.Y < B.Y);
	}

	void RotateToCanonicalStart(TArray<FVector2D>& Vertices)
	{
		int32 BestIndex = 0;
		for (int32 Index = 1; Index < Vertices.Num(); ++Index)
		{
			if (LexicalPointLess(Vertices[Index], Vertices[BestIndex]))
			{
				BestIndex = Index;
			}
		}
		if (BestIndex != 0)
		{
			TArray<FVector2D> Rotated;
			Rotated.Reserve(Vertices.Num());
			for (int32 Offset = 0; Offset < Vertices.Num(); ++Offset)
			{
				Rotated.Add(Vertices[(BestIndex + Offset) % Vertices.Num()]);
			}
			Vertices = MoveTemp(Rotated);
		}
	}

	bool PointOnSegment(const FVector2D& Point, const FVector2D& A, const FVector2D& B)
	{
		if (FMath::Abs(Cross(A, B, Point)) > CollinearDistanceEpsilon)
		{
			return false;
		}
		return Point.X >= FMath::Min(A.X, B.X) - CollinearDistanceEpsilon
			&& Point.X <= FMath::Max(A.X, B.X) + CollinearDistanceEpsilon
			&& Point.Y >= FMath::Min(A.Y, B.Y) - CollinearDistanceEpsilon
			&& Point.Y <= FMath::Max(A.Y, B.Y) + CollinearDistanceEpsilon;
	}

	int32 Orientation(const FVector2D& A, const FVector2D& B, const FVector2D& C)
	{
		const double Value = Cross(A, B, C);
		return Value > CollinearDistanceEpsilon ? 1 : Value < -CollinearDistanceEpsilon ? -1 : 0;
	}

	bool SegmentsIntersect(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D)
	{
		const int32 O1 = Orientation(A, B, C);
		const int32 O2 = Orientation(A, B, D);
		const int32 O3 = Orientation(C, D, A);
		const int32 O4 = Orientation(C, D, B);
		if (O1 != O2 && O3 != O4)
		{
			return true;
		}
		return (O1 == 0 && PointOnSegment(C, A, B))
			|| (O2 == 0 && PointOnSegment(D, A, B))
			|| (O3 == 0 && PointOnSegment(A, C, D))
			|| (O4 == 0 && PointOnSegment(B, C, D));
	}

	bool IsSimple(TConstArrayView<FVector2D> Polygon)
	{
		for (int32 EdgeA = 0; EdgeA < Polygon.Num(); ++EdgeA)
		{
			const int32 EdgeANext = (EdgeA + 1) % Polygon.Num();
			for (int32 EdgeB = EdgeA + 1; EdgeB < Polygon.Num(); ++EdgeB)
			{
				const int32 EdgeBNext = (EdgeB + 1) % Polygon.Num();
				if (EdgeA == EdgeB || EdgeANext == EdgeB || EdgeBNext == EdgeA)
				{
					continue;
				}
				if (SegmentsIntersect(Polygon[EdgeA], Polygon[EdgeANext], Polygon[EdgeB], Polygon[EdgeBNext]))
				{
					return false;
				}
			}
		}
		return true;
	}

	bool PointInTriangleInclusive(
		const FVector2D& Point,
		const FVector2D& A,
		const FVector2D& B,
		const FVector2D& C)
	{
		return Cross(A, B, Point) >= -CollinearDistanceEpsilon
			&& Cross(B, C, Point) >= -CollinearDistanceEpsilon
			&& Cross(C, A, Point) >= -CollinearDistanceEpsilon;
	}

	void RemoveDuplicateVertices(TArray<FVector2D>& Vertices, int32& RemovedCount)
	{
		TArray<FVector2D> Unique;
		Unique.Reserve(Vertices.Num());
		for (const FVector2D& Vertex : Vertices)
		{
			if (Unique.IsEmpty() || FVector2D::DistSquared(Unique.Last(), Vertex) > DuplicateEpsilonSquared)
			{
				Unique.Add(Vertex);
			}
			else
			{
				++RemovedCount;
			}
		}
		if (Unique.Num() > 1 && FVector2D::DistSquared(Unique[0], Unique.Last()) <= DuplicateEpsilonSquared)
		{
			Unique.Pop(EAllowShrinking::No);
			++RemovedCount;
		}
		Vertices = MoveTemp(Unique);
	}

	void RemoveCollinearVertices(TArray<FVector2D>& Vertices, int32& RemovedCount)
	{
		bool bRemoved = true;
		while (bRemoved && Vertices.Num() >= 3)
		{
			bRemoved = false;
			for (int32 Index = 0; Index < Vertices.Num(); ++Index)
			{
				const FVector2D& Previous = Vertices[(Index + Vertices.Num() - 1) % Vertices.Num()];
				const FVector2D& Current = Vertices[Index];
				const FVector2D& Next = Vertices[(Index + 1) % Vertices.Num()];
				const double EdgeLength = FMath::Sqrt(FVector2D::DistSquared(Previous, Next));
				const double DistanceNumerator = FMath::Abs(Cross(Previous, Current, Next));
				if (EdgeLength <= UE_DOUBLE_SMALL_NUMBER
					|| DistanceNumerator <= CollinearDistanceEpsilon * EdgeLength)
				{
					Vertices.RemoveAt(Index, 1, EAllowShrinking::No);
					++RemovedCount;
					bRemoved = true;
					break;
				}
			}
		}
	}

	ESightWeaveRenderPacketFailure NormalizePolygon(
		TConstArrayView<FVector2D> Input,
		TArray<FVector2D>& OutVertices,
		int32& RemovedDuplicateCount,
		int32& RemovedCollinearCount)
	{
		if (Input.Num() > SightWeave::RenderPacket::MaximumPolygonVertices)
		{
			return ESightWeaveRenderPacketFailure::TooManyVertices;
		}
		OutVertices.Reset(Input.Num());
		for (const FVector2D& Vertex : Input)
		{
			if (!IsFinite(Vertex))
			{
				return ESightWeaveRenderPacketFailure::NonFiniteVertex;
			}
			OutVertices.Add(Vertex);
		}
		RemoveDuplicateVertices(OutVertices, RemovedDuplicateCount);
		RemoveCollinearVertices(OutVertices, RemovedCollinearCount);
		if (OutVertices.Num() < 3)
		{
			return ESightWeaveRenderPacketFailure::DegeneratePolygon;
		}
		const double Area = SignedDoubleArea(OutVertices);
		if (!FMath::IsFinite(Area) || FMath::Abs(Area) <= AreaEpsilon)
		{
			return ESightWeaveRenderPacketFailure::DegeneratePolygon;
		}
		if (Area < 0.0)
		{
			Algo::Reverse(OutVertices);
		}
		RotateToCanonicalStart(OutVertices);
		return IsSimple(OutVertices)
			? ESightWeaveRenderPacketFailure::None
			: ESightWeaveRenderPacketFailure::NonSimplePolygon;
	}

	bool Triangulate(TConstArrayView<FVector2D> Vertices, TArray<uint32>& OutLocalIndices)
	{
		OutLocalIndices.Reset((Vertices.Num() - 2) * 3);
		TArray<int32> Remaining;
		Remaining.Reserve(Vertices.Num());
		for (int32 Index = 0; Index < Vertices.Num(); ++Index)
		{
			Remaining.Add(Index);
		}

		while (Remaining.Num() > 3)
		{
			bool bFoundEar = false;
			for (int32 Candidate = 0; Candidate < Remaining.Num(); ++Candidate)
			{
				const int32 Previous = Remaining[(Candidate + Remaining.Num() - 1) % Remaining.Num()];
				const int32 Current = Remaining[Candidate];
				const int32 Next = Remaining[(Candidate + 1) % Remaining.Num()];
				if (Cross(Vertices[Previous], Vertices[Current], Vertices[Next]) <= AreaEpsilon)
				{
					continue;
				}

				bool bContainsOtherVertex = false;
				for (const int32 Test : Remaining)
				{
					if (Test != Previous && Test != Current && Test != Next
						&& PointInTriangleInclusive(
							Vertices[Test], Vertices[Previous], Vertices[Current], Vertices[Next]))
					{
						bContainsOtherVertex = true;
						break;
					}
				}
				if (bContainsOtherVertex)
				{
					continue;
				}

				OutLocalIndices.Add(static_cast<uint32>(Previous));
				OutLocalIndices.Add(static_cast<uint32>(Current));
				OutLocalIndices.Add(static_cast<uint32>(Next));
				Remaining.RemoveAt(Candidate, 1, EAllowShrinking::No);
				bFoundEar = true;
				break;
			}
			if (!bFoundEar)
			{
				return false;
			}
		}
		OutLocalIndices.Add(static_cast<uint32>(Remaining[0]));
		OutLocalIndices.Add(static_cast<uint32>(Remaining[1]));
		OutLocalIndices.Add(static_cast<uint32>(Remaining[2]));
		return OutLocalIndices.Num() == (Vertices.Num() - 2) * 3;
	}

	bool IsOutsideTile(TConstArrayView<FVector2D> Polygon, const FBox2D& TileBounds)
	{
		FBox2D PolygonBounds(ForceInit);
		for (const FVector2D& Vertex : Polygon)
		{
			PolygonBounds += Vertex;
		}
		return PolygonBounds.Max.X < TileBounds.Min.X
			|| PolygonBounds.Min.X > TileBounds.Max.X
			|| PolygonBounds.Max.Y < TileBounds.Min.Y
			|| PolygonBounds.Min.Y > TileBounds.Max.Y;
	}

	void HashBytes(uint64& Hash, const void* Data, int32 NumBytes)
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

	uint64 ComputeProfileHash(TConstArrayView<FName> Capabilities)
	{
		uint64 Hash = FnvOffset;
		const uint32 Count = static_cast<uint32>(Capabilities.Num());
		HashValue(Hash, Count);
		for (const FName Capability : Capabilities)
		{
			HashName(Hash, Capability);
		}
		return Hash == 0 ? 1 : Hash;
	}

	uint64 ComputePacketHash(const FSightWeaveRenderPacket& Packet)
	{
		uint64 Hash = FnvOffset;
		HashValue(Hash, Packet.GetWorldIdentity().Serial);
		HashName(Hash, Packet.GetKnowledgeOwnerId().GetValue());
		HashName(Hash, Packet.GetFloorId().GetValue());
		HashValue(Hash, Packet.GetCompatibilityProfile().StableHash);
		HashValue(Hash, Packet.GetPacketRevision());
		HashValue(Hash, Packet.GetRegistryRevision());
		HashValue(Hash, Packet.GetPublishedSnapshotRevision());
		const FIntPoint TileCoordinate = Packet.GetTileCoordinate();
		HashValue(Hash, TileCoordinate.X);
		HashValue(Hash, TileCoordinate.Y);
		const FBox2D& Bounds = Packet.GetPhysicalWorldBounds();
		HashValue(Hash, Bounds.Min.X);
		HashValue(Hash, Bounds.Min.Y);
		HashValue(Hash, Bounds.Max.X);
		HashValue(Hash, Bounds.Max.Y);
		HashValue(Hash, Packet.GetCentimetersPerTexel());
		const int32 Gutter = Packet.GetGutter();
		HashValue(Hash, Gutter);
		const uint8 DirtyReason = static_cast<uint8>(Packet.GetDirtyReason());
		const uint8 FullTile = Packet.IsFullTile() ? 1 : 0;
		HashValue(Hash, DirtyReason);
		HashValue(Hash, FullTile);
		for (int32 LayerIndex = 0; LayerIndex < static_cast<int32>(ESightWeaveRenderMaskLayer::Count); ++LayerIndex)
		{
			const FSightWeaveRenderTriangleRange& Range =
				Packet.GetRange(static_cast<ESightWeaveRenderMaskLayer>(LayerIndex));
			HashValue(Hash, Range.FirstVertex);
			HashValue(Hash, Range.VertexCount);
			HashValue(Hash, Range.FirstIndex);
			HashValue(Hash, Range.IndexCount);
		}
		for (const FVector2f& Vertex : Packet.GetVertices())
		{
			uint32 XBits = 0;
			uint32 YBits = 0;
			FMemory::Memcpy(&XBits, &Vertex.X, sizeof(XBits));
			FMemory::Memcpy(&YBits, &Vertex.Y, sizeof(YBits));
			HashValue(Hash, XBits);
			HashValue(Hash, YBits);
		}
		for (const uint32 Index : Packet.GetIndices())
		{
			HashValue(Hash, Index);
		}
		return Hash == 0 ? 1 : Hash;
	}
}

FSightWeaveRenderProfileIdentity FSightWeaveRenderProfileIdentity::FromProfile(
	const FSightWeaveIlluminationCompatibilityProfile& Profile)
{
	FSightWeaveRenderProfileIdentity Result;
	for (const FName Capability : Profile.AcceptedCapabilities)
	{
		if (!Capability.IsNone())
		{
			Result.CanonicalCapabilities.Add(Capability);
		}
	}
	Result.CanonicalCapabilities.Sort(FNameLexicalLess());
	Result.CanonicalCapabilities.SetNum(
		Algo::Unique(Result.CanonicalCapabilities),
		EAllowShrinking::No);
	if (!Result.CanonicalCapabilities.IsEmpty())
	{
		Result.StableHash = ComputeProfileHash(Result.CanonicalCapabilities);
	}
	return Result;
}

bool FSightWeaveRenderProfileIdentity::IsEquivalentTo(
	const FSightWeaveRenderProfileIdentity& Other) const
{
	return StableHash == Other.StableHash && CanonicalCapabilities == Other.CanonicalCapabilities;
}

const FSightWeaveRenderTriangleRange& FSightWeaveRenderPacket::GetRange(
	const ESightWeaveRenderMaskLayer Layer) const
{
	const int32 Index = static_cast<int32>(Layer);
	check(Index >= 0 && Index < Ranges.Num());
	return Ranges[Index];
}

FSightWeaveRenderPacketBuildResult FSightWeaveRenderPacketBuilder::Build(
	const FSightWeaveRenderPacketBuildInput& Input)
{
	FSightWeaveRenderPacketBuildResult Result;
	TSharedRef<FSightWeaveRenderPacket, ESPMode::ThreadSafe> MutablePacket =
		MakeShared<FSightWeaveRenderPacket, ESPMode::ThreadSafe>();
	MutablePacket->WorldIdentity = Input.WorldIdentity;
	MutablePacket->KnowledgeOwnerId = Input.KnowledgeOwnerId;
	MutablePacket->FloorId = Input.FloorId;
	MutablePacket->CompatibilityProfile = Input.CompatibilityProfile;
	MutablePacket->PacketRevision = Input.PacketRevision;
	MutablePacket->RegistryRevision = Input.RegistryRevision;
	MutablePacket->PublishedSnapshotRevision = Input.PublishedSnapshotRevision;
	MutablePacket->TileCoordinate = Input.TileCoordinate;
	MutablePacket->PhysicalWorldBounds = Input.PhysicalWorldBounds;
	MutablePacket->CentimetersPerTexel = Input.CentimetersPerTexel;
	MutablePacket->Gutter = Input.Gutter;
	MutablePacket->DirtyReason = Input.DirtyReason;
	MutablePacket->bFullTile = Input.bFullTile;
	Result.Packet = MutablePacket;

	auto Fail = [&Result, &MutablePacket](const ESightWeaveRenderPacketFailure Failure)
	{
		MutablePacket->bValid = false;
		MutablePacket->Failure = Failure;
		MutablePacket->Vertices.Reset();
		MutablePacket->Indices.Reset();
		MutablePacket->ContentHash = 0;
		for (FSightWeaveRenderTriangleRange& Range : MutablePacket->Ranges)
		{
			Range = FSightWeaveRenderTriangleRange();
		}
		Result.Failure = Failure;
		return Result;
	};

	if (!Input.WorldIdentity.IsValid())
	{
		return Fail(ESightWeaveRenderPacketFailure::InvalidWorldIdentity);
	}
	if (!Input.KnowledgeOwnerId.IsValid() || !Input.FloorId.IsValid())
	{
		return Fail(ESightWeaveRenderPacketFailure::InvalidScope);
	}
	if (!Input.CompatibilityProfile.IsValid())
	{
		return Fail(ESightWeaveRenderPacketFailure::InvalidProfile);
	}
	if (Input.PacketRevision == 0 || Input.RegistryRevision == 0 || Input.PublishedSnapshotRevision == 0)
	{
		return Fail(ESightWeaveRenderPacketFailure::InvalidRevision);
	}
	const double ExpectedSpan = static_cast<double>(SightWeave::RenderPacket::PhysicalTileSize)
		* static_cast<double>(Input.CentimetersPerTexel);
	if (!Input.PhysicalWorldBounds.bIsValid
		|| !IsFinite(Input.PhysicalWorldBounds.Min)
		|| !IsFinite(Input.PhysicalWorldBounds.Max)
		|| !FMath::IsFinite(Input.CentimetersPerTexel)
		|| Input.CentimetersPerTexel <= 0.0f
		|| Input.Gutter != SightWeave::RenderPacket::GutterTexels
		|| !FMath::IsNearlyEqual(Input.PhysicalWorldBounds.GetSize().X, ExpectedSpan, 1.0e-3)
		|| !FMath::IsNearlyEqual(Input.PhysicalWorldBounds.GetSize().Y, ExpectedSpan, 1.0e-3))
	{
		return Fail(ESightWeaveRenderPacketFailure::InvalidTile);
	}

	const FVector2D TileSize = Input.PhysicalWorldBounds.GetSize();
	MutablePacket->WorldToTileUvScale = FVector2f(
		static_cast<float>(1.0 / TileSize.X),
		static_cast<float>(1.0 / TileSize.Y));
	MutablePacket->WorldToTileUvBias = FVector2f(
		static_cast<float>(-Input.PhysicalWorldBounds.Min.X / TileSize.X),
		static_cast<float>(-Input.PhysicalWorldBounds.Min.Y / TileSize.Y));

	TArray<int32> PolygonOrder;
	PolygonOrder.Reserve(Input.Polygons.Num());
	for (int32 PolygonIndex = 0; PolygonIndex < Input.Polygons.Num(); ++PolygonIndex)
	{
		PolygonOrder.Add(PolygonIndex);
	}
	PolygonOrder.Sort([&Input](const int32 A, const int32 B)
	{
		const FSightWeaveRenderPolygonInput& Left = Input.Polygons[A];
		const FSightWeaveRenderPolygonInput& Right = Input.Polygons[B];
		return Left.Layer != Right.Layer
			? static_cast<uint8>(Left.Layer) < static_cast<uint8>(Right.Layer)
			: Left.StableSourceId < Right.StableSourceId;
	});

	TArray<FVector2D> Normalized;
	TArray<uint32> LocalIndices;
	int64 PreviousSourceId = MIN_int64;
	ESightWeaveRenderMaskLayer PreviousLayer = ESightWeaveRenderMaskLayer::Count;
	for (const int32 PolygonIndex : PolygonOrder)
	{
		const FSightWeaveRenderPolygonInput& Polygon = Input.Polygons[PolygonIndex];
		if (Polygon.StableSourceId <= 0
			|| static_cast<uint8>(Polygon.Layer) >= static_cast<uint8>(ESightWeaveRenderMaskLayer::Count)
			|| (Polygon.Layer == PreviousLayer && Polygon.StableSourceId == PreviousSourceId))
		{
			return Fail(ESightWeaveRenderPacketFailure::InvalidSourceIdentity);
		}
		PreviousLayer = Polygon.Layer;
		PreviousSourceId = Polygon.StableSourceId;
		if (Polygon.KnowledgeOwnerId != Input.KnowledgeOwnerId || Polygon.FloorId != Input.FloorId)
		{
			return Fail(ESightWeaveRenderPacketFailure::ScopeMismatch);
		}
		if (Polygon.CompatibilityProfileHash != Input.CompatibilityProfile.StableHash)
		{
			return Fail(ESightWeaveRenderPacketFailure::ProfileMismatch);
		}

		const ESightWeaveRenderPacketFailure NormalizationFailure = NormalizePolygon(
			Polygon.WorldVertices,
			Normalized,
			Result.RemovedDuplicateVertexCount,
			Result.RemovedCollinearVertexCount);
		if (NormalizationFailure != ESightWeaveRenderPacketFailure::None)
		{
			return Fail(NormalizationFailure);
		}
		if (IsOutsideTile(Normalized, Input.PhysicalWorldBounds))
		{
			++Result.OutsidePolygonCount;
			continue;
		}
		if (MutablePacket->Vertices.Num() + Normalized.Num() > SightWeave::RenderPacket::MaximumPacketVertices)
		{
			return Fail(ESightWeaveRenderPacketFailure::TooManyVertices);
		}
		if (!Triangulate(Normalized, LocalIndices))
		{
			return Fail(ESightWeaveRenderPacketFailure::TriangulationFailed);
		}

		FSightWeaveRenderTriangleRange& Range =
			MutablePacket->Ranges[static_cast<int32>(Polygon.Layer)];
		if (Range.VertexCount == 0)
		{
			Range.FirstVertex = static_cast<uint32>(MutablePacket->Vertices.Num());
			Range.FirstIndex = static_cast<uint32>(MutablePacket->Indices.Num());
		}
		const uint32 BaseVertex = static_cast<uint32>(MutablePacket->Vertices.Num());
		for (const FVector2D& Vertex : Normalized)
		{
			const FVector2D Local = Vertex - Input.PhysicalWorldBounds.Min;
			const FVector2f Packed(static_cast<float>(Local.X), static_cast<float>(Local.Y));
			if (!FMath::IsFinite(Packed.X) || !FMath::IsFinite(Packed.Y))
			{
				return Fail(ESightWeaveRenderPacketFailure::NonFiniteVertex);
			}
			MutablePacket->Vertices.Add(Packed);
		}
		for (const uint32 LocalIndex : LocalIndices)
		{
			MutablePacket->Indices.Add(BaseVertex + LocalIndex);
		}
		Range.VertexCount += static_cast<uint32>(Normalized.Num());
		Range.IndexCount += static_cast<uint32>(LocalIndices.Num());
		++Result.AcceptedPolygonCount;
	}
	uint32 FirstVertex = 0;
	uint32 FirstIndex = 0;
	for (FSightWeaveRenderTriangleRange& Range : MutablePacket->Ranges)
	{
		Range.FirstVertex = FirstVertex;
		Range.FirstIndex = FirstIndex;
		FirstVertex += Range.VertexCount;
		FirstIndex += Range.IndexCount;
	}

	MutablePacket->bValid = true;
	MutablePacket->Failure = ESightWeaveRenderPacketFailure::None;
	const ESightWeaveRenderPacketFailure ValidationFailure = Validate(*MutablePacket);
	if (ValidationFailure != ESightWeaveRenderPacketFailure::None)
	{
		return Fail(ValidationFailure);
	}
	MutablePacket->ContentHash = ComputePacketHash(*MutablePacket);
	Result.Failure = ESightWeaveRenderPacketFailure::None;
	return Result;
}

ESightWeaveRenderPacketFailure FSightWeaveRenderPacketBuilder::Validate(
	const FSightWeaveRenderPacket& Packet)
{
	if (!Packet.WorldIdentity.IsValid())
	{
		return ESightWeaveRenderPacketFailure::InvalidWorldIdentity;
	}
	if (!Packet.KnowledgeOwnerId.IsValid() || !Packet.FloorId.IsValid())
	{
		return ESightWeaveRenderPacketFailure::InvalidScope;
	}
	if (!Packet.CompatibilityProfile.IsValid())
	{
		return ESightWeaveRenderPacketFailure::InvalidProfile;
	}
	if (Packet.PacketRevision == 0 || Packet.RegistryRevision == 0 || Packet.PublishedSnapshotRevision == 0)
	{
		return ESightWeaveRenderPacketFailure::InvalidRevision;
	}
	if (Packet.Indices.Num() % 3 != 0)
	{
		return ESightWeaveRenderPacketFailure::InvalidIndexData;
	}
	uint32 ExpectedFirstVertex = 0;
	uint32 ExpectedFirstIndex = 0;
	for (const FSightWeaveRenderTriangleRange& Range : Packet.Ranges)
	{
		if (Range.FirstVertex != ExpectedFirstVertex
			|| Range.FirstIndex != ExpectedFirstIndex
			|| Range.IndexCount % 3 != 0
			|| static_cast<uint64>(Range.FirstVertex) + Range.VertexCount > static_cast<uint64>(Packet.Vertices.Num())
			|| static_cast<uint64>(Range.FirstIndex) + Range.IndexCount > static_cast<uint64>(Packet.Indices.Num()))
		{
			return ESightWeaveRenderPacketFailure::InvalidIndexData;
		}
		ExpectedFirstVertex += Range.VertexCount;
		ExpectedFirstIndex += Range.IndexCount;
	}
	if (ExpectedFirstVertex != static_cast<uint32>(Packet.Vertices.Num())
		|| ExpectedFirstIndex != static_cast<uint32>(Packet.Indices.Num()))
	{
		return ESightWeaveRenderPacketFailure::InvalidIndexData;
	}
	for (const uint32 Index : Packet.Indices)
	{
		if (Index >= static_cast<uint32>(Packet.Vertices.Num()))
		{
			return ESightWeaveRenderPacketFailure::InvalidIndexData;
		}
	}
	return ESightWeaveRenderPacketFailure::None;
}

ESightWeaveRenderPacketDisposition FSightWeaveRenderPacketRevisionGate::ClassifyAndCommit(
	const FSightWeaveRenderPacket& Packet)
{
	if (!Packet.IsValid()
		|| FSightWeaveRenderPacketBuilder::Validate(Packet) != ESightWeaveRenderPacketFailure::None)
	{
		return ESightWeaveRenderPacketDisposition::Invalid;
	}
	if (Packet.GetWorldIdentity() != WorldIdentity)
	{
		return ESightWeaveRenderPacketDisposition::WorldMismatch;
	}
	if (Packet.GetPacketRevision() < AcceptedRevision)
	{
		++StalePacketCount;
		return ESightWeaveRenderPacketDisposition::Stale;
	}
	if (Packet.GetPacketRevision() == AcceptedRevision)
	{
		if (Packet.GetContentHash() == AcceptedHash)
		{
			++DuplicatePacketCount;
			return ESightWeaveRenderPacketDisposition::Duplicate;
		}
		return ESightWeaveRenderPacketDisposition::RevisionConflict;
	}
	AcceptedRevision = Packet.GetPacketRevision();
	AcceptedHash = Packet.GetContentHash();
	++AcceptedPacketCount;
	return ESightWeaveRenderPacketDisposition::Accepted;
}

void FSightWeaveRenderPacketRevisionGate::Reset()
{
	AcceptedRevision = 0;
	AcceptedHash = 0;
	AcceptedPacketCount = 0;
	DuplicatePacketCount = 0;
	StalePacketCount = 0;
}
