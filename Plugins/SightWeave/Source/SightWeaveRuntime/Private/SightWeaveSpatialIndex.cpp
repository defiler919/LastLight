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
	ReusableCellIdArrays.Reset();
	ReusableSourceEdgeIndexArrays.Reset();
	UpdateCellsScratch.Reset();
	QueryCellsScratch.Reset();
	QuerySortedIdsScratch.Reset();
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

	// A transform-only update preserves stable IDs, floor, and segment count.
	// Update those entries and their cell memberships in place so warmed dynamic
	// authority dispatch does not destroy/recreate nested array storage.
	bool bCanUpdateInPlace = ExistingIds->Num() == Segments.Num();
	bool bPreviouslyDynamic = false;
	for (const FSightWeaveSegment2D& Segment : Segments)
	{
		const FSightWeaveFloorId* ExistingFloorId = SegmentFloors.Find(Segment.StableId);
		FFloorData* Floor = ExistingFloorId ? Floors.Find(*ExistingFloorId) : nullptr;
		bCanUpdateInPlace = bCanUpdateInPlace
			&& ExistingIds->Contains(Segment.StableId)
			&& ExistingFloorId
			&& *ExistingFloorId == Segment.FloorId
			&& Floor
			&& Floor->Segments.Contains(Segment.StableId);
		bPreviouslyDynamic |= DynamicSegmentIds.Contains(Segment.StableId);
	}
	if (bCanUpdateInPlace)
	{
		OutNewBounds = FBox2D(ForceInit);
		for (const FSightWeaveSegment2D& Segment : Segments)
		{
			FFloorData& Floor = Floors.FindChecked(Segment.FloorId);
			FSegmentEntry& Entry = Floor.Segments.FindChecked(Segment.StableId);
			GetCellsForBounds(Segment.GetBounds(), UpdateCellsScratch);
			UpdateCellsScratch.Sort([](const FIntPoint& A, const FIntPoint& B)
			{
				return A.X < B.X || (A.X == B.X && A.Y < B.Y);
			});

			for (const FIntPoint& OldCell : Entry.Cells)
			{
				if (UpdateCellsScratch.Contains(OldCell))
				{
					continue;
				}
				if (TArray<int64>* CellIds = Floor.CellSegmentIds.Find(OldCell))
				{
					CellIds->RemoveSingle(Segment.StableId);
					if (CellIds->IsEmpty())
					{
						ReusableCellIdArrays.Add(MoveTemp(*CellIds));
						Floor.CellSegmentIds.Remove(OldCell);
					}
				}
			}
			for (const FIntPoint& NewCell : UpdateCellsScratch)
			{
				if (Entry.Cells.Contains(NewCell))
				{
					continue;
				}
				TArray<int64>* CellIds = Floor.CellSegmentIds.Find(NewCell);
				if (!CellIds)
				{
					TArray<int64>& NewCellIds = Floor.CellSegmentIds.Add(NewCell);
					if (!ReusableCellIdArrays.IsEmpty())
					{
						NewCellIds = MoveTemp(ReusableCellIdArrays.Last());
						ReusableCellIdArrays.Pop(EAllowShrinking::No);
						NewCellIds.Reset();
					}
					CellIds = &NewCellIds;
				}
				CellIds->Add(Segment.StableId);
				CellIds->Sort();
			}

			Entry.Segment.A = Segment.A;
			Entry.Segment.B = Segment.B;
			Entry.Segment.FloorId = Segment.FloorId;
			Entry.Segment.HeightRange = Segment.HeightRange;
			Entry.Segment.OccluderHandle = Segment.OccluderHandle;
			Entry.Segment.bDynamic = Segment.bDynamic;
			Entry.Segment.StableId = Segment.StableId;
			Entry.Segment.SourceEdgeIndices.Reset();
			Entry.Segment.SourceEdgeIndices.Append(Segment.SourceEdgeIndices);
			Swap(Entry.Cells, UpdateCellsScratch);
			OutNewBounds += Segment.A;
			OutNewBounds += Segment.B;
		}

		if (bPreviouslyDynamic != bDynamic)
		{
			for (const int64 StableId : *ExistingIds)
			{
				(bPreviouslyDynamic ? DynamicSegmentIds : StaticSegmentIds).Remove(StableId);
				(bDynamic ? DynamicSegmentIds : StaticSegmentIds).Add(StableId);
			}
		}
		if (bPreviouslyDynamic) ++DynamicRemoveCount;
		if (bDynamic) ++DynamicInsertCount;
		if (bDynamic) ++DynamicUpdateCount;
		return true;
	}

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
	LastCandidateCount = 0;
	const FFloorData* Floor = Floors.Find(FloorId);
	if (!Floor || !Bounds.bIsValid || !HeightRange.IsValid())
	{
		OutSegments.Reset();
		return;
	}

	QueryCellsScratch.Reset();
	GetCellsForBounds(Bounds, QueryCellsScratch);
	QuerySortedIdsScratch.Reset();
	for (const FIntPoint& Cell : QueryCellsScratch)
	{
		if (const TArray<int64>* CellIds = Floor->CellSegmentIds.Find(Cell))
		{
			for (const int64 StableId : *CellIds)
			{
				QuerySortedIdsScratch.Add(StableId);
			}
		}
	}
	QuerySortedIdsScratch.Sort();
	int32 UniqueWriteIndex = 0;
	for (const int64 StableId : QuerySortedIdsScratch)
	{
		if (UniqueWriteIndex == 0 || QuerySortedIdsScratch[UniqueWriteIndex - 1] != StableId)
		{
			QuerySortedIdsScratch[UniqueWriteIndex++] = StableId;
		}
	}
	QuerySortedIdsScratch.SetNum(UniqueWriteIndex, EAllowShrinking::No);
	int32 SegmentWriteIndex = 0;
	for (const int64 StableId : QuerySortedIdsScratch)
	{
		const FSegmentEntry* Entry = Floor->Segments.Find(StableId);
		if (Entry
			&& Entry->Segment.GetBounds().Intersect(Bounds)
			&& SightWeave::Geometry::HeightRangesOverlap(Entry->Segment.HeightRange, HeightRange, HeightEpsilon))
		{
			if (OutSegments.IsValidIndex(SegmentWriteIndex))
			{
				OutSegments[SegmentWriteIndex] = Entry->Segment;
			}
			else
			{
				OutSegments.Add(Entry->Segment);
			}
			++SegmentWriteIndex;
		}
	}
	OutSegments.SetNum(SegmentWriteIndex, EAllowShrinking::No);
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
	Entry.Segment.A = Segment.A;
	Entry.Segment.B = Segment.B;
	Entry.Segment.FloorId = Segment.FloorId;
	Entry.Segment.HeightRange = Segment.HeightRange;
	Entry.Segment.OccluderHandle = Segment.OccluderHandle;
	Entry.Segment.bDynamic = Segment.bDynamic;
	Entry.Segment.StableId = Segment.StableId;
	if (!ReusableSourceEdgeIndexArrays.IsEmpty())
	{
		Entry.Segment.SourceEdgeIndices = MoveTemp(ReusableSourceEdgeIndexArrays.Last());
		ReusableSourceEdgeIndexArrays.Pop(EAllowShrinking::No);
	}
	Entry.Segment.SourceEdgeIndices.Reset();
	Entry.Segment.SourceEdgeIndices.Append(Segment.SourceEdgeIndices);
	GetCellsForBounds(Segment.GetBounds(), Entry.Cells);
	Entry.Cells.Sort([](const FIntPoint& A, const FIntPoint& B)
	{
		return A.X < B.X || (A.X == B.X && A.Y < B.Y);
	});
	for (const FIntPoint& Cell : Entry.Cells)
	{
		TArray<int64>* ExistingCellIds = Floor.CellSegmentIds.Find(Cell);
		if (!ExistingCellIds)
		{
			TArray<int64>& NewCellIds = Floor.CellSegmentIds.Add(Cell);
			if (!ReusableCellIdArrays.IsEmpty())
			{
				NewCellIds = MoveTemp(ReusableCellIdArrays.Last());
				ReusableCellIdArrays.Pop(EAllowShrinking::No);
				NewCellIds.Reset();
			}
			ExistingCellIds = &NewCellIds;
		}
		TArray<int64>& CellIds = *ExistingCellIds;
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
				ReusableCellIdArrays.Add(MoveTemp(*CellIds));
				Floor->CellSegmentIds.Remove(Cell);
			}
		}
	}
	ReusableSourceEdgeIndexArrays.Add(MoveTemp(Entry->Segment.SourceEdgeIndices));
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
