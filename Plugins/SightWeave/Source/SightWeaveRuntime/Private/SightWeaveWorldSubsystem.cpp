#include "SightWeaveWorldSubsystem.h"

#include "SightWeaveSettings.h"

void USightWeaveWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ResetState();
	SpatialIndex.SetCellSize(GetDefault<USightWeaveSettings>()->SpatialCellSizeCentimeters);
	bSightWeaveInitialized = true;
}

void USightWeaveWorldSubsystem::Deinitialize()
{
	ResetState();
	bSightWeaveInitialized = false;
	Super::Deinitialize();
}

bool USightWeaveWorldSubsystem::RegisterFloor(
	const FSightWeaveFloorDefinition& Definition,
	UObject* Owner)
{
	if (!bSightWeaveInitialized
		|| Floors.Contains(Definition.FloorId)
		|| !IsFloorDefinitionAllowed(Definition, nullptr))
	{
		return false;
	}
	FSightWeaveFloorDefinition Stored = Definition;
	AdvanceRevision();
	Stored.Revision = Revision;
	Floors.Add(Stored.FloorId, Stored);
	if (Owner)
	{
		FloorOwners.Add(Stored.FloorId, Owner);
	}
	return true;
}

bool USightWeaveWorldSubsystem::UpdateFloor(
	const FSightWeaveFloorId ExistingFloorId,
	const FSightWeaveFloorDefinition& Definition)
{
	if (!bSightWeaveInitialized
		|| ExistingFloorId != Definition.FloorId
		|| !Floors.Contains(ExistingFloorId)
		|| !IsFloorDefinitionAllowed(Definition, &ExistingFloorId))
	{
		return false;
	}
	FSightWeaveFloorDefinition Stored = Definition;
	AdvanceRevision();
	Stored.Revision = Revision;
	Floors.FindChecked(ExistingFloorId) = Stored;
	for (const TPair<int64, FSightWeaveVisionSourceDescription>& Pair : VisionSources)
	{
		if (Pair.Value.FloorId == ExistingFloorId)
		{
			DirtyVisionSources.Add(Pair.Key);
		}
	}
	for (const TPair<int64, FSightWeaveIlluminationSourceDescription>& Pair : IlluminationSources)
	{
		if (Pair.Value.FloorId == ExistingFloorId)
		{
			DirtyIlluminationSources.Add(Pair.Key);
		}
	}
	return true;
}

bool USightWeaveWorldSubsystem::UnregisterFloor(const FSightWeaveFloorId FloorId)
{
	if (!bSightWeaveInitialized || Floors.Remove(FloorId) == 0)
	{
		return false;
	}
	FloorOwners.Remove(FloorId);
	for (const TPair<int64, FSightWeaveVisionSourceDescription>& Pair : VisionSources)
	{
		if (Pair.Value.FloorId == FloorId)
		{
			DirtyVisionSources.Add(Pair.Key);
		}
	}
	for (const TPair<int64, FSightWeaveIlluminationSourceDescription>& Pair : IlluminationSources)
	{
		if (Pair.Value.FloorId == FloorId)
		{
			DirtyIlluminationSources.Add(Pair.Key);
		}
	}
	AdvanceRevision();
	return true;
}

bool USightWeaveWorldSubsystem::IsFloorRegistered(const FSightWeaveFloorId FloorId) const
{
	return bSightWeaveInitialized && FloorId.IsValid() && Floors.Contains(FloorId);
}

FSightWeaveFloorId USightWeaveWorldSubsystem::GetActiveFloorId() const
{
	for (const TPair<FSightWeaveFloorId, FSightWeaveFloorDefinition>& Pair : Floors)
	{
		if (Pair.Value.bEnabled && Pair.Value.bActiveForQueries)
		{
			return Pair.Key;
		}
	}
	return FSightWeaveFloorId();
}

FSightWeaveVisionSourceHandle USightWeaveWorldSubsystem::RegisterVisionSource(
	const FSightWeaveVisionSourceDescription& Description,
	UObject* Owner)
{
	if (!bSightWeaveInitialized || !Description.IsValid())
	{
		return FSightWeaveVisionSourceHandle();
	}

	FSightWeaveVisionSourceDescription NormalizedDescription = Description;
	NormalizedDescription.Compatibility.Normalize();
	const int64 NewId = NextVisionSourceId++;
	VisionSources.Add(NewId, MoveTemp(NormalizedDescription));
	if (Owner)
	{
		VisionOwners.Add(NewId, Owner);
	}
	DirtyVisionSources.Add(NewId);
	AdvanceRevision();
	return FSightWeaveVisionSourceHandle(NewId);
}

bool USightWeaveWorldSubsystem::UpdateVisionSource(
	const FSightWeaveVisionSourceHandle Handle,
	const FSightWeaveVisionSourceDescription& Description)
{
	if (!bSightWeaveInitialized || !Description.IsValid() || !VisionSources.Contains(Handle.GetValue()))
	{
		return false;
	}

	FSightWeaveVisionSourceDescription NormalizedDescription = Description;
	NormalizedDescription.Compatibility.Normalize();
	VisionSources.FindChecked(Handle.GetValue()) = MoveTemp(NormalizedDescription);
	DirtyVisionSources.Add(Handle.GetValue());
	AdvanceRevision();
	return true;
}

bool USightWeaveWorldSubsystem::UnregisterVisionSource(const FSightWeaveVisionSourceHandle Handle)
{
	if (!bSightWeaveInitialized || VisionSources.Remove(Handle.GetValue()) == 0)
	{
		return false;
	}

	VisionOwners.Remove(Handle.GetValue());
	DirtyVisionSources.Remove(Handle.GetValue());
	AdvanceRevision();
	return true;
}

bool USightWeaveWorldSubsystem::IsVisionSourceHandleValid(const FSightWeaveVisionSourceHandle Handle) const
{
	return bSightWeaveInitialized && Handle.IsValid() && VisionSources.Contains(Handle.GetValue());
}

FSightWeaveIlluminationSourceHandle USightWeaveWorldSubsystem::RegisterIlluminationSource(
	const FSightWeaveIlluminationSourceDescription& Description,
	UObject* Owner)
{
	if (!bSightWeaveInitialized || !Description.IsValid())
	{
		return FSightWeaveIlluminationSourceHandle();
	}

	FSightWeaveIlluminationSourceDescription NormalizedDescription = Description;
	NormalizedDescription.NormalizeCapabilities();
	const int64 NewId = NextIlluminationSourceId++;
	IlluminationSources.Add(NewId, MoveTemp(NormalizedDescription));
	if (Owner)
	{
		IlluminationOwners.Add(NewId, Owner);
	}
	DirtyIlluminationSources.Add(NewId);
	AdvanceRevision();
	return FSightWeaveIlluminationSourceHandle(NewId);
}

bool USightWeaveWorldSubsystem::UpdateIlluminationSource(
	const FSightWeaveIlluminationSourceHandle Handle,
	const FSightWeaveIlluminationSourceDescription& Description)
{
	if (!bSightWeaveInitialized || !Description.IsValid() || !IlluminationSources.Contains(Handle.GetValue()))
	{
		return false;
	}

	FSightWeaveIlluminationSourceDescription NormalizedDescription = Description;
	NormalizedDescription.NormalizeCapabilities();
	IlluminationSources.FindChecked(Handle.GetValue()) = MoveTemp(NormalizedDescription);
	DirtyIlluminationSources.Add(Handle.GetValue());
	AdvanceRevision();
	return true;
}

bool USightWeaveWorldSubsystem::UnregisterIlluminationSource(const FSightWeaveIlluminationSourceHandle Handle)
{
	if (!bSightWeaveInitialized || IlluminationSources.Remove(Handle.GetValue()) == 0)
	{
		return false;
	}

	IlluminationOwners.Remove(Handle.GetValue());
	DirtyIlluminationSources.Remove(Handle.GetValue());
	AdvanceRevision();
	return true;
}

bool USightWeaveWorldSubsystem::IsIlluminationSourceHandleValid(const FSightWeaveIlluminationSourceHandle Handle) const
{
	return bSightWeaveInitialized && Handle.IsValid() && IlluminationSources.Contains(Handle.GetValue());
}

FSightWeaveOccluderHandle USightWeaveWorldSubsystem::RegisterOccluder(
	const TArray<FSightWeaveSegment2D>& Segments,
	const bool bDynamic,
	const bool bEnabled,
	UObject* Owner)
{
	if (!bSightWeaveInitialized || Segments.IsEmpty())
	{
		return FSightWeaveOccluderHandle();
	}
	const FSightWeaveOccluderHandle Handle(NextOccluderId++);
	TArray<FSightWeaveSegment2D> Prepared = PrepareOccluderSegments(Handle, Segments, bDynamic);
	if (Prepared.IsEmpty())
	{
		return FSightWeaveOccluderHandle();
	}
	const FSightWeaveFloorId FloorId = Prepared[0].FloorId;
	if (Prepared.ContainsByPredicate([FloorId](const FSightWeaveSegment2D& Segment) { return Segment.FloorId != FloorId; }))
	{
		return FSightWeaveOccluderHandle();
	}

	FOccluderRecord Record;
	Record.Segments = MoveTemp(Prepared);
	Record.bDynamic = bDynamic;
	Record.bEnabled = bEnabled;
	for (const FSightWeaveSegment2D& Segment : Record.Segments)
	{
		Record.Bounds += Segment.A;
		Record.Bounds += Segment.B;
	}
	if (bEnabled && !SpatialIndex.InsertOccluder(Handle, Record.Segments, bDynamic))
	{
		return FSightWeaveOccluderHandle();
	}
	AdvanceRevision();
	Record.GeometryRevision = Revision;
	Occluders.Add(Handle.GetValue(), MoveTemp(Record));
	if (Owner)
	{
		OccluderOwners.Add(Handle.GetValue(), Owner);
	}
	const FOccluderRecord& Stored = Occluders.FindChecked(Handle.GetValue());
	MarkSourcesAffectedByOccluderChange(FSightWeaveFloorId(), FBox2D(ForceInit), FloorId, Stored.Bounds);
	return Handle;
}

bool USightWeaveWorldSubsystem::UpdateOccluder(
	const FSightWeaveOccluderHandle Handle,
	const TArray<FSightWeaveSegment2D>& Segments,
	const bool bDynamic,
	const bool bEnabled)
{
	FOccluderRecord* Record = Occluders.Find(Handle.GetValue());
	if (!bSightWeaveInitialized || !Record || Segments.IsEmpty())
	{
		return false;
	}
	TArray<FSightWeaveSegment2D> Prepared = PrepareOccluderSegments(Handle, Segments, bDynamic);
	if (Prepared.IsEmpty())
	{
		return false;
	}
	const FSightWeaveFloorId NewFloor = Prepared[0].FloorId;
	if (Prepared.ContainsByPredicate([NewFloor](const FSightWeaveSegment2D& Segment) { return Segment.FloorId != NewFloor; }))
	{
		return false;
	}
	const FSightWeaveFloorId OldFloor = Record->Segments.IsEmpty() ? FSightWeaveFloorId() : Record->Segments[0].FloorId;
	const FBox2D OldBounds = Record->Bounds;
	FBox2D NewBounds(ForceInit);
	for (const FSightWeaveSegment2D& Segment : Prepared)
	{
		NewBounds += Segment.A;
		NewBounds += Segment.B;
	}

	if (Record->bEnabled && bEnabled)
	{
		FBox2D IndexedOldBounds(ForceInit);
		FBox2D IndexedNewBounds(ForceInit);
		if (!SpatialIndex.UpdateOccluder(Handle, Prepared, bDynamic, IndexedOldBounds, IndexedNewBounds))
		{
			return false;
		}
	}
	else if (Record->bEnabled)
	{
		if (!SpatialIndex.RemoveOccluder(Handle))
		{
			return false;
		}
	}
	else if (bEnabled)
	{
		if (!SpatialIndex.InsertOccluder(Handle, Prepared, bDynamic))
		{
			return false;
		}
	}

	Record->Segments = MoveTemp(Prepared);
	Record->Bounds = NewBounds;
	Record->bDynamic = bDynamic;
	Record->bEnabled = bEnabled;
	AdvanceRevision();
	Record->GeometryRevision = Revision;
	MarkSourcesAffectedByOccluderChange(OldFloor, OldBounds, NewFloor, NewBounds);
	return true;
}

bool USightWeaveWorldSubsystem::UnregisterOccluder(const FSightWeaveOccluderHandle Handle)
{
	FOccluderRecord* Record = Occluders.Find(Handle.GetValue());
	if (!bSightWeaveInitialized || !Record)
	{
		return false;
	}
	const FSightWeaveFloorId OldFloor = Record->Segments.IsEmpty() ? FSightWeaveFloorId() : Record->Segments[0].FloorId;
	const FBox2D OldBounds = Record->Bounds;
	if (Record->bEnabled)
	{
		SpatialIndex.RemoveOccluder(Handle);
	}
	Occluders.Remove(Handle.GetValue());
	OccluderOwners.Remove(Handle.GetValue());
	AdvanceRevision();
	MarkSourcesAffectedByOccluderChange(OldFloor, OldBounds, FSightWeaveFloorId(), FBox2D(ForceInit));
	return true;
}

bool USightWeaveWorldSubsystem::IsOccluderHandleValid(const FSightWeaveOccluderHandle Handle) const
{
	return bSightWeaveInitialized && Handle.IsValid() && Occluders.Contains(Handle.GetValue());
}

FSightWeaveRevision USightWeaveWorldSubsystem::GetOccluderGeometryRevision(const FSightWeaveOccluderHandle Handle) const
{
	const FOccluderRecord* Record = Occluders.Find(Handle.GetValue());
	return Record ? Record->GeometryRevision : FSightWeaveRevision();
}

FSightWeaveSubjectRevealHandle USightWeaveWorldSubsystem::ApplySubjectRevealOverride(
	const FSightWeaveSubjectRevealSpecification& Specification,
	UObject* Owner)
{
	if (!bSightWeaveInitialized || !Specification.IsValid())
	{
		return FSightWeaveSubjectRevealHandle();
	}

	const int64 NewId = NextSubjectRevealId++;
	SubjectReveals.Add(NewId, Specification);
	if (Owner)
	{
		SubjectRevealOwners.Add(NewId, Owner);
	}
	AdvanceRevision();
	return FSightWeaveSubjectRevealHandle(NewId);
}

bool USightWeaveWorldSubsystem::UpdateSubjectRevealOverride(
	const FSightWeaveSubjectRevealHandle Handle,
	const FSightWeaveSubjectRevealSpecification& Specification)
{
	if (!bSightWeaveInitialized || !Specification.IsValid() || !SubjectReveals.Contains(Handle.GetValue()))
	{
		return false;
	}

	SubjectReveals.FindChecked(Handle.GetValue()) = Specification;
	AdvanceRevision();
	return true;
}

bool USightWeaveWorldSubsystem::RemoveSubjectRevealOverride(const FSightWeaveSubjectRevealHandle Handle)
{
	if (!bSightWeaveInitialized || SubjectReveals.Remove(Handle.GetValue()) == 0)
	{
		return false;
	}

	SubjectRevealOwners.Remove(Handle.GetValue());
	AdvanceRevision();
	return true;
}

bool USightWeaveWorldSubsystem::IsSubjectRevealHandleValid(const FSightWeaveSubjectRevealHandle Handle) const
{
	return bSightWeaveInitialized && Handle.IsValid() && SubjectReveals.Contains(Handle.GetValue());
}

int32 USightWeaveWorldSubsystem::UnregisterAllForOwner(UObject* Owner)
{
	if (!bSightWeaveInitialized || !Owner)
	{
		return 0;
	}

	int32 RemovedCount = 0;
	TArray<FSightWeaveFloorId> OwnedFloors;
	for (const TPair<FSightWeaveFloorId, TWeakObjectPtr<UObject>>& Pair : FloorOwners)
	{
		if (Pair.Value.Get() == Owner)
		{
			OwnedFloors.Add(Pair.Key);
		}
	}
	for (const FSightWeaveFloorId FloorId : OwnedFloors)
	{
		RemovedCount += UnregisterFloor(FloorId) ? 1 : 0;
	}

	TArray<FSightWeaveVisionSourceHandle> OwnedVision;
	for (const TPair<int64, TWeakObjectPtr<UObject>>& Pair : VisionOwners)
	{
		if (Pair.Value.Get() == Owner) OwnedVision.Emplace(Pair.Key);
	}
	for (const FSightWeaveVisionSourceHandle Handle : OwnedVision)
	{
		RemovedCount += UnregisterVisionSource(Handle) ? 1 : 0;
	}

	TArray<FSightWeaveIlluminationSourceHandle> OwnedIllumination;
	for (const TPair<int64, TWeakObjectPtr<UObject>>& Pair : IlluminationOwners)
	{
		if (Pair.Value.Get() == Owner) OwnedIllumination.Emplace(Pair.Key);
	}
	for (const FSightWeaveIlluminationSourceHandle Handle : OwnedIllumination)
	{
		RemovedCount += UnregisterIlluminationSource(Handle) ? 1 : 0;
	}

	TArray<FSightWeaveOccluderHandle> OwnedOccluders;
	for (const TPair<int64, TWeakObjectPtr<UObject>>& Pair : OccluderOwners)
	{
		if (Pair.Value.Get() == Owner) OwnedOccluders.Emplace(Pair.Key);
	}
	for (const FSightWeaveOccluderHandle Handle : OwnedOccluders)
	{
		RemovedCount += UnregisterOccluder(Handle) ? 1 : 0;
	}

	TArray<FSightWeaveSubjectRevealHandle> OwnedReveals;
	for (const TPair<int64, TWeakObjectPtr<UObject>>& Pair : SubjectRevealOwners)
	{
		if (Pair.Value.Get() == Owner) OwnedReveals.Emplace(Pair.Key);
	}
	for (const FSightWeaveSubjectRevealHandle Handle : OwnedReveals)
	{
		RemovedCount += RemoveSubjectRevealOverride(Handle) ? 1 : 0;
	}
	return RemovedCount;
}

FSightWeaveVisibilityQueryResult USightWeaveWorldSubsystem::QueryVisibilityAtLocation(
	const FSightWeaveFloorId FloorId,
	const FVector WorldLocation) const
{
	if (!FloorId.IsValid())
	{
		return MakeQueryResult(ESightWeaveQueryStatus::InvalidFloor, FloorId);
	}
	if (WorldLocation.ContainsNaN())
	{
		return MakeQueryResult(ESightWeaveQueryStatus::InvalidInput, FloorId);
	}
	return MakeQueryResult(ESightWeaveQueryStatus::NotReady, FloorId);
}

FSightWeaveVisibilityQueryResult USightWeaveWorldSubsystem::QueryVisionSourceAtLocation(
	const FSightWeaveVisionSourceHandle Handle,
	const FSightWeaveFloorId FloorId,
	const FVector WorldLocation) const
{
	if (!IsVisionSourceHandleValid(Handle))
	{
		return MakeQueryResult(ESightWeaveQueryStatus::InvalidHandle, FloorId);
	}
	return QueryVisibilityAtLocation(FloorId, WorldLocation);
}

void USightWeaveWorldSubsystem::QueryOccluderSegments(
	const FSightWeaveFloorId FloorId,
	const FBox2D& Bounds,
	const FSightWeaveHeightRange& HeightRange,
	TArray<FSightWeaveSegment2D>& OutSegments) const
{
	const FSightWeaveGeometryTolerances& Tolerances = GetDefault<USightWeaveSettings>()->GeometryTolerances;
	SpatialIndex.Query(FloorId, Bounds, HeightRange, Tolerances.HeightOverlapEpsilon, OutSegments);
}

bool USightWeaveWorldSubsystem::IsFloorDefinitionAllowed(
	const FSightWeaveFloorDefinition& Definition,
	const FSightWeaveFloorId* IgnoreFloor) const
{
	if (!Definition.IsValid())
	{
		return false;
	}
	if (Definition.bEnabled && Definition.bActiveForQueries)
	{
		for (const TPair<FSightWeaveFloorId, FSightWeaveFloorDefinition>& Pair : Floors)
		{
			if ((!IgnoreFloor || Pair.Key != *IgnoreFloor)
				&& Pair.Value.bEnabled
				&& Pair.Value.bActiveForQueries)
			{
				return false;
			}
		}
	}
	return true;
}

void USightWeaveWorldSubsystem::MarkSourcesAffectedByOccluderChange(
	const FSightWeaveFloorId OldFloor,
	const FBox2D& OldBounds,
	const FSightWeaveFloorId NewFloor,
	const FBox2D& NewBounds)
{
	auto IntersectsAffectedBounds = [&OldFloor, &OldBounds, &NewFloor, &NewBounds](
		const FSightWeaveFloorId SourceFloor,
		const FBox2D& SourceBounds)
	{
		return (OldFloor.IsValid() && SourceFloor == OldFloor && OldBounds.bIsValid && SourceBounds.Intersect(OldBounds))
			|| (NewFloor.IsValid() && SourceFloor == NewFloor && NewBounds.bIsValid && SourceBounds.Intersect(NewBounds));
	};

	for (const TPair<int64, FSightWeaveVisionSourceDescription>& Pair : VisionSources)
	{
		const FVector Origin = Pair.Value.Transform.GetLocation();
		const double Radius = Pair.Value.Range;
		const FBox2D SourceBounds(
			FVector2D(Origin.X - Radius, Origin.Y - Radius),
			FVector2D(Origin.X + Radius, Origin.Y + Radius));
		if (IntersectsAffectedBounds(Pair.Value.FloorId, SourceBounds))
		{
			DirtyVisionSources.Add(Pair.Key);
		}
	}
	for (const TPair<int64, FSightWeaveIlluminationSourceDescription>& Pair : IlluminationSources)
	{
		const FVector Origin = Pair.Value.Transform.GetLocation();
		const double Radius = Pair.Value.Range;
		const FBox2D SourceBounds(
			FVector2D(Origin.X - Radius, Origin.Y - Radius),
			FVector2D(Origin.X + Radius, Origin.Y + Radius));
		if (IntersectsAffectedBounds(Pair.Value.FloorId, SourceBounds))
		{
			DirtyIlluminationSources.Add(Pair.Key);
		}
	}
}

TArray<FSightWeaveSegment2D> USightWeaveWorldSubsystem::PrepareOccluderSegments(
	const FSightWeaveOccluderHandle Handle,
	TConstArrayView<FSightWeaveSegment2D> Segments,
	const bool bDynamic)
{
	FSightWeaveGeometryTolerances Tolerances = GetDefault<USightWeaveSettings>()->GeometryTolerances;
	Tolerances.Normalize();
	FSightWeaveNormalizationResult Normalized = SightWeave::Geometry::NormalizeSegments(Segments, Tolerances, false);
	for (FSightWeaveSegment2D& Segment : Normalized.Segments)
	{
		Segment.OccluderHandle = Handle;
		Segment.bDynamic = bDynamic;
		Segment.StableId = NextSegmentId++;
	}
	return MoveTemp(Normalized.Segments);
}

void USightWeaveWorldSubsystem::AdvanceRevision()
{
	Revision = FSightWeaveRevision(Revision.GetValue() + 1);
}

void USightWeaveWorldSubsystem::ResetState()
{
	Floors.Reset();
	VisionSources.Reset();
	IlluminationSources.Reset();
	SubjectReveals.Reset();
	Occluders.Reset();
	SpatialIndex.Reset();
	DirtyVisionSources.Reset();
	DirtyIlluminationSources.Reset();
	FloorOwners.Reset();
	VisionOwners.Reset();
	IlluminationOwners.Reset();
	SubjectRevealOwners.Reset();
	OccluderOwners.Reset();
	NextVisionSourceId = 1;
	NextIlluminationSourceId = 1;
	NextSubjectRevealId = 1;
	NextOccluderId = 1;
	NextSegmentId = 1;
	Revision = FSightWeaveRevision();
}

FSightWeaveVisibilityQueryResult USightWeaveWorldSubsystem::MakeQueryResult(
	const ESightWeaveQueryStatus Status,
	const FSightWeaveFloorId FloorId) const
{
	FSightWeaveVisibilityQueryResult Result;
	Result.Status = Status;
	Result.KnowledgeState = ESightWeaveKnowledgeState::Unknown;
	Result.bVisible = false;
	Result.Revision = Revision;
	Result.FloorId = FloorId;
	return Result;
}
