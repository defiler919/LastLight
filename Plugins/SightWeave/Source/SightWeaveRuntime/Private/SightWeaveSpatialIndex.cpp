#include "SightWeaveSpatialIndex.h"

FSightWeaveFloorSpatialIndex::FSightWeaveFloorSpatialIndex(double InCellSize)
{
	SetCellSize(InCellSize);
}

void FSightWeaveFloorSpatialIndex::Reset()
{
	Floors.Reset();
	SegmentFloors.Reset();
	OccluderSegments.Reset();
	StaticSegmentIds.Reset();
	DynamicSegmentIds.Reset();
	LastCandidateCount = 0;
	StaticBuildCount = 0;
	DynamicInsertCount = 0;
	DynamicRemoveCount = 0;
	DynamicUpdateCount = 0;
}

void FSightWeaveFloorSpatialIndex::SetCellSize(double InCellSize)
{
	CellSize = FMath::IsFinite(InCellSize) && InCellSize > UE_DOUBLE_SMALL_NUMBER ? InCellSize : 500.0;
}

bool FSightWeaveFloorSpatialIndex::BuildStatic(TConstArrayView<FSightWeaveSegment2D> Segments)
{
	const int64 PreviousStaticBuildCount = StaticBuildCount;
	Reset();
	bool bAllInserted = true;
	for (const FSightWeaveSegment2D& Segment : Segments)
	{
		if (!InsertSegment(Segment))
		{
			bAllInserted = false;
			continue;
		}
		StaticSegmentIds.Add(Segment.StableId);
		if (Segment.OccluderHandle.IsValid())
		{
			OccluderSegments.FindOrAdd(Segment.OccluderHandle).Add(Segment.StableId);
		}
	}
	StaticBuildCount = PreviousStaticBuildCount + 1;
	return bAllInserted;
}

bool FSightWeaveFloorSpatialIndex::InsertOccluder(
	const FSightWeaveOccluderHandle Handle,
	TConstArrayView<FSightWeaveSegment2D> Segments,
	const bool bDynamic,
	FBox2D* OutBounds)
{
	if (!Handle.IsValid() || Segments.IsEmpty() || OccluderSegments.Contains(Handle))
	{
		return false;
	}

	TArray<int64> InsertedIds;
	for (FSightWeaveSegment2D Segment : Segments)
	{
		Segment.OccluderHandle = Handle;
		Segment.bDynamic = bDynamic;
		if (!InsertSegment(Segment))
		{
			for (const int64 InsertedId : InsertedIds)
			{
				RemoveSegment(InsertedId);
			}
			return false;
		}
		InsertedIds.Add(Segment.StableId);
		(bDynamic ? DynamicSegmentIds : StaticSegmentIds).Add(Segment.StableId);
	}
	InsertedIds.Sort();
	OccluderSegments.Add(Handle, InsertedIds);
	if (bDynamic)
	{
		++DynamicInsertCount;
	}
	if (OutBounds)
	{
		*OutBounds = CalculateOccluderBounds(InsertedIds);
	}
	return true;
}

bool FSightWeaveFloorSpatialIndex::RemoveOccluder(
	const FSightWeaveOccluderHandle Handle,
	FBox2D* OutOldBounds)
{
	const TArray<int64>* StableIds = OccluderSegments.Find(Handle);
	if (!StableIds)
	{
		return false;
	}
	if (OutOldBounds)
	{
		*OutOldBounds = CalculateOccluderBounds(*StableIds);
	}
	bool bRemovedDynamic = false;
	for (const int64 StableId : *StableIds)
	{
		bRemovedDynamic |= DynamicSegmentIds.Contains(StableId);
		RemoveSegment(StableId);
	}
	OccluderSegments.Remove(Handle);
	if (bRemovedDynamic)
	{
		++DynamicRemoveCount;
	}
	return true;
}

bool FSightWeaveFloorSpatialIndex::UpdateOccluder(
	const FSightWeaveOccluderHandle Handle,
	TConstArrayView<FSightWeaveSegment2D> Segments,
	const bool bDynamic,
	FBox2D& OutOldBounds,
	FBox2D& OutNewBounds)
{
	const TArray<int64>* ExistingIds = OccluderSegments.Find(Handle);
	if (!ExistingIds || Segments.IsEmpty())
	{
		return false;
	}
	OutOldBounds = CalculateOccluderBounds(*ExistingIds);
	if (!RemoveOccluder(Handle, nullptr))
	{
		return false;
	}
	if (!InsertOccluder(Handle, Segments, bDynamic, &OutNewBounds))
	{
		return false;
	}
	if (bDynamic)
	{
		++DynamicUpdateCount;
	}
	return true;
}

void FSightWeaveFloorSpatialIndex::Query(
	const FSightWeaveFloorId FloorId,
	const FBox2D& Bounds,
	const FSightWeaveHeightRange& HeightRange,
	const double HeightEpsilon,
	TArray<FSightWeaveSegment2D>& OutSegments) const
{
	OutSegments.Reset();
	LastCandidateCount = 0;
	const FFloorData* Floor = Floors.Find(FloorId);
	if (!Floor || !Bounds.bIsValid || !HeightRange.IsValid())
	{
		return;
	}

	TArray<FIntPoint> Cells;
	GetCellsForBounds(Bounds, Cells);
	TSet<int64> CandidateIds;
	for (const FIntPoint& Cell : Cells)
	{
		if (const TArray<int64>* CellIds = Floor->CellSegmentIds.Find(Cell))
		{
			for (const int64 StableId : *CellIds)
			{
				CandidateIds.Add(StableId);
			}
		}
	}
	TArray<int64> SortedIds = CandidateIds.Array();
	SortedIds.Sort();
	for (const int64 StableId : SortedIds)
	{
		const FSegmentEntry* Entry = Floor->Segments.Find(StableId);
		if (Entry
			&& Entry->Segment.GetBounds().Intersect(Bounds)
			&& SightWeave::Geometry::HeightRangesOverlap(Entry->Segment.HeightRange, HeightRange, HeightEpsilon))
		{
			OutSegments.Add(Entry->Segment);
		}
	}
	LastCandidateCount = OutSegments.Num();
}

bool FSightWeaveFloorSpatialIndex::ContainsSegment(const int64 StableId) const
{
	return SegmentFloors.Contains(StableId);
}

bool FSightWeaveFloorSpatialIndex::ContainsOccluder(const FSightWeaveOccluderHandle Handle) const
{
	return OccluderSegments.Contains(Handle);
}

FSightWeaveSpatialIndexStats FSightWeaveFloorSpatialIndex::GetStats() const
{
	FSightWeaveSpatialIndexStats Stats;
	Stats.FloorCount = Floors.Num();
	for (const TPair<FSightWeaveFloorId, FFloorData>& Pair : Floors)
	{
		Stats.CellCount += Pair.Value.CellSegmentIds.Num();
		Stats.SegmentCount += Pair.Value.Segments.Num();
	}
	Stats.StaticSegmentCount = StaticSegmentIds.Num();
	Stats.DynamicSegmentCount = DynamicSegmentIds.Num();
	Stats.LastCandidateCount = LastCandidateCount;
	Stats.StaticBuildCount = StaticBuildCount;
	Stats.DynamicInsertCount = DynamicInsertCount;
	Stats.DynamicRemoveCount = DynamicRemoveCount;
	Stats.DynamicUpdateCount = DynamicUpdateCount;
	return Stats;
}

void FSightWeaveFloorSpatialIndex::GetDebugCells(TArray<FSightWeaveSpatialCellDebug>& OutCells) const
{
	OutCells.Reset();
	for (const TPair<FSightWeaveFloorId, FFloorData>& FloorPair : Floors)
	{
		for (const TPair<FIntPoint, TArray<int64>>& CellPair : FloorPair.Value.CellSegmentIds)
		{
			FSightWeaveSpatialCellDebug& DebugCell = OutCells.AddDefaulted_GetRef();
			DebugCell.FloorId = FloorPair.Key;
			DebugCell.Cell = CellPair.Key;
			DebugCell.BoundsMin = FVector2D(CellPair.Key.X * CellSize, CellPair.Key.Y * CellSize);
			DebugCell.BoundsMax = DebugCell.BoundsMin + FVector2D(CellSize, CellSize);
			DebugCell.SegmentCount = CellPair.Value.Num();
		}
	}
	OutCells.Sort([](const FSightWeaveSpatialCellDebug& A, const FSightWeaveSpatialCellDebug& B)
	{
		if (A.FloorId != B.FloorId)
		{
			return A.FloorId.GetValue().LexicalLess(B.FloorId.GetValue());
		}
		return A.Cell.X < B.Cell.X || (A.Cell.X == B.Cell.X && A.Cell.Y < B.Cell.Y);
	});
}

FIntPoint FSightWeaveFloorSpatialIndex::ToCell(const FVector2D& Point) const
{
	return FIntPoint(
		FMath::FloorToInt(Point.X / CellSize),
		FMath::FloorToInt(Point.Y / CellSize));
}

void FSightWeaveFloorSpatialIndex::GetCellsForBounds(const FBox2D& Bounds, TArray<FIntPoint>& OutCells) const
{
	OutCells.Reset();
	if (!Bounds.bIsValid)
	{
		return;
	}
	const FIntPoint MinCell = ToCell(Bounds.Min);
	const FIntPoint MaxCell = ToCell(Bounds.Max);
	for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
	{
		for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
		{
			OutCells.Add(FIntPoint(X, Y));
		}
	}
}

bool FSightWeaveFloorSpatialIndex::InsertSegment(const FSightWeaveSegment2D& Segment)
{
	if (!Segment.IsFinite()
		|| !Segment.FloorId.IsValid()
		|| Segment.StableId <= 0
		|| SegmentFloors.Contains(Segment.StableId))
	{
		return false;
	}
	FFloorData& Floor = Floors.FindOrAdd(Segment.FloorId);
	FSegmentEntry Entry;
	Entry.Segment = Segment;
	GetCellsForBounds(Segment.GetBounds(), Entry.Cells);
	Entry.Cells.Sort([](const FIntPoint& A, const FIntPoint& B)
	{
		return A.X < B.X || (A.X == B.X && A.Y < B.Y);
	});
	for (const FIntPoint& Cell : Entry.Cells)
	{
		TArray<int64>& CellIds = Floor.CellSegmentIds.FindOrAdd(Cell);
		CellIds.Add(Segment.StableId);
		CellIds.Sort();
	}
	Floor.Segments.Add(Segment.StableId, MoveTemp(Entry));
	SegmentFloors.Add(Segment.StableId, Segment.FloorId);
	return true;
}

bool FSightWeaveFloorSpatialIndex::RemoveSegment(const int64 StableId)
{
	const FSightWeaveFloorId* FoundFloorId = SegmentFloors.Find(StableId);
	if (!FoundFloorId)
	{
		return false;
	}
	const FSightWeaveFloorId FloorId = *FoundFloorId;
	FFloorData* Floor = Floors.Find(FloorId);
	FSegmentEntry* Entry = Floor ? Floor->Segments.Find(StableId) : nullptr;
	if (!Floor || !Entry)
	{
		return false;
	}
	for (const FIntPoint& Cell : Entry->Cells)
	{
		if (TArray<int64>* CellIds = Floor->CellSegmentIds.Find(Cell))
		{
			CellIds->RemoveSingle(StableId);
			if (CellIds->IsEmpty())
			{
				Floor->CellSegmentIds.Remove(Cell);
			}
		}
	}
	Floor->Segments.Remove(StableId);
	StaticSegmentIds.Remove(StableId);
	DynamicSegmentIds.Remove(StableId);
	SegmentFloors.Remove(StableId);
	if (Floor->Segments.IsEmpty())
	{
		Floors.Remove(FloorId);
	}
	return true;
}

FBox2D FSightWeaveFloorSpatialIndex::CalculateOccluderBounds(TConstArrayView<int64> StableIds) const
{
	FBox2D Bounds(ForceInit);
	for (const int64 StableId : StableIds)
	{
		const FSightWeaveFloorId* FloorId = SegmentFloors.Find(StableId);
		const FFloorData* Floor = FloorId ? Floors.Find(*FloorId) : nullptr;
		const FSegmentEntry* Entry = Floor ? Floor->Segments.Find(StableId) : nullptr;
		if (Entry)
		{
			Bounds += Entry->Segment.A;
			Bounds += Entry->Segment.B;
		}
	}
	return Bounds;
}
