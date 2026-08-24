#include "SightWeaveWorldSubsystem.h"

#include "Algo/Sort.h"
#include "HAL/PlatformTime.h"
#include "SightWeaveSettings.h"

namespace
{
	bool IsPointInHeightRange(const double Z, const FSightWeaveHeightRange& Range, const double Epsilon)
	{
		return Z >= static_cast<double>(Range.ZMin) - Epsilon
			&& Z <= static_cast<double>(Range.ZMax) + Epsilon;
	}

	template <typename HandleType>
	void AddUniqueHandle(TArray<HandleType>& Handles, const HandleType Handle)
	{
		if (!Handles.Contains(Handle))
		{
			Handles.Add(Handle);
		}
	}

	bool IsPointInNominalShape(
		const FVector& WorldLocation,
		const FTransform& Transform,
		const ESightWeaveSourceShape Shape,
		const double Range,
		const double HalfAngleDegrees,
		const double NearAwarenessRadius,
		const double Epsilon)
	{
		const FVector Origin = Transform.GetLocation();
		const FVector2D Offset(WorldLocation.X - Origin.X, WorldLocation.Y - Origin.Y);
		const double Distance = Offset.Size();
		if (Distance > Range + Epsilon)
		{
			return false;
		}
		if (Distance <= NearAwarenessRadius + Epsilon || Shape == ESightWeaveSourceShape::Radial)
		{
			return true;
		}
		FVector Forward3 = Transform.GetUnitAxis(EAxis::X);
		FVector2D Forward(Forward3.X, Forward3.Y);
		if (!Forward.Normalize())
		{
			Forward = FVector2D(1.0, 0.0);
		}
		const FVector2D Direction = Distance > UE_DOUBLE_SMALL_NUMBER ? Offset / Distance : Forward;
		const double Cosine = FMath::Clamp(FVector2D::DotProduct(Forward, Direction), -1.0, 1.0);
		return FMath::RadiansToDegrees(FMath::Acos(Cosine)) <= HalfAngleDegrees + Epsilon;
	}

	bool AreCapabilitiesCompatible(
		const FSightWeaveIlluminationCompatibilityProfile& Profile,
		TConstArrayView<FName> EmittedCapabilities)
	{
		return EmittedCapabilities.ContainsByPredicate(
			[&Profile](const FName Capability) { return Profile.Accepts(Capability); });
	}

	void SetPolygonBounds(TConstArrayView<FVector> Vertices, FVector2D& OutMin, FVector2D& OutMax)
	{
		FBox2D Bounds(ForceInit);
		for (const FVector& Vertex : Vertices)
		{
			Bounds += FVector2D(Vertex.X, Vertex.Y);
		}
		OutMin = Bounds.bIsValid ? Bounds.Min : FVector2D::ZeroVector;
		OutMax = Bounds.bIsValid ? Bounds.Max : FVector2D::ZeroVector;
	}
}

void USightWeaveWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ResetState();
	SpatialIndex.SetCellSize(GetDefault<USightWeaveSettings>()->SpatialCellSizeCentimeters);
	bSightWeaveInitialized = true;
	PublishSnapshot();
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
	for (const TPair<int64, FSightWeaveVisionSourceDescription>& Pair : VisionSources)
	{
		if (Pair.Value.FloorId == Stored.FloorId)
		{
			DirtyVisionSources.Add(Pair.Key);
			PendingVisionSnapshotRebuilds.Add(Pair.Key);
		}
	}
	for (const TPair<int64, FSightWeaveIlluminationSourceDescription>& Pair : IlluminationSources)
	{
		if (Pair.Value.FloorId == Stored.FloorId)
		{
			DirtyIlluminationSources.Add(Pair.Key);
			PendingIlluminationSnapshotRebuilds.Add(Pair.Key);
		}
	}
	PublishSnapshot();
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
			PendingVisionSnapshotRebuilds.Add(Pair.Key);
		}
	}
	for (const TPair<int64, FSightWeaveIlluminationSourceDescription>& Pair : IlluminationSources)
	{
		if (Pair.Value.FloorId == ExistingFloorId)
		{
			DirtyIlluminationSources.Add(Pair.Key);
			PendingIlluminationSnapshotRebuilds.Add(Pair.Key);
		}
	}
	PublishSnapshot();
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
			PendingVisionSnapshotRebuilds.Add(Pair.Key);
		}
	}
	for (const TPair<int64, FSightWeaveIlluminationSourceDescription>& Pair : IlluminationSources)
	{
		if (Pair.Value.FloorId == FloorId)
		{
			DirtyIlluminationSources.Add(Pair.Key);
			PendingIlluminationSnapshotRebuilds.Add(Pair.Key);
		}
	}
	AdvanceRevision();
	PublishSnapshot();
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
	if (NormalizedDescription.IlluminationPolicy == ESightWeaveIlluminationPolicy::BypassLegalIllumination)
	{
		NormalizedDescription.Compatibility.AcceptedCapabilities.Reset();
	}
	const int64 NewId = NextVisionSourceId++;
	VisionSources.Add(NewId, MoveTemp(NormalizedDescription));
	if (Owner)
	{
		VisionOwners.Add(NewId, Owner);
	}
	DirtyVisionSources.Add(NewId);
	PendingVisionSnapshotRebuilds.Add(NewId);
	AdvanceRevision();
	VisionSourceRevisions.Add(NewId, Revision);
	PublishSnapshot();
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
	if (NormalizedDescription.IlluminationPolicy == ESightWeaveIlluminationPolicy::BypassLegalIllumination)
	{
		NormalizedDescription.Compatibility.AcceptedCapabilities.Reset();
	}
	VisionSources.FindChecked(Handle.GetValue()) = MoveTemp(NormalizedDescription);
	DirtyVisionSources.Add(Handle.GetValue());
	PendingVisionSnapshotRebuilds.Add(Handle.GetValue());
	AdvanceRevision();
	VisionSourceRevisions.Add(Handle.GetValue(), Revision);
	PublishSnapshot();
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
	PendingVisionSnapshotRebuilds.Remove(Handle.GetValue());
	VisionSourceRevisions.Remove(Handle.GetValue());
	CachedVisionSnapshotEntries.Remove(Handle.GetValue());
	AdvanceRevision();
	PublishSnapshot();
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
	PendingIlluminationSnapshotRebuilds.Add(NewId);
	AdvanceRevision();
	IlluminationSourceRevisions.Add(NewId, Revision);
	PublishSnapshot();
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
	PendingIlluminationSnapshotRebuilds.Add(Handle.GetValue());
	AdvanceRevision();
	IlluminationSourceRevisions.Add(Handle.GetValue(), Revision);
	PublishSnapshot();
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
	PendingIlluminationSnapshotRebuilds.Remove(Handle.GetValue());
	IlluminationSourceRevisions.Remove(Handle.GetValue());
	CachedIlluminationSnapshotEntries.Remove(Handle.GetValue());
	AdvanceRevision();
	PublishSnapshot();
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
	LastOccluderRevision = Revision;
	Record.GeometryRevision = Revision;
	Occluders.Add(Handle.GetValue(), MoveTemp(Record));
	if (Owner)
	{
		OccluderOwners.Add(Handle.GetValue(), Owner);
	}
	const FOccluderRecord& Stored = Occluders.FindChecked(Handle.GetValue());
	MarkSourcesAffectedByOccluderChange(FSightWeaveFloorId(), FBox2D(ForceInit), FloorId, Stored.Bounds);
	PublishSnapshot();
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
	// Preserve per-edge stable IDs across ordinary transform updates so exact-distance ties
	// do not change merely because a dynamic occluder moved and returned.
	for (int32 SegmentIndex = 0; SegmentIndex < FMath::Min(Prepared.Num(), Record->Segments.Num()); ++SegmentIndex)
	{
		Prepared[SegmentIndex].StableId = Record->Segments[SegmentIndex].StableId;
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
	LastOccluderRevision = Revision;
	Record->GeometryRevision = Revision;
	MarkSourcesAffectedByOccluderChange(OldFloor, OldBounds, NewFloor, NewBounds);
	PublishSnapshot();
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
	LastOccluderRevision = Revision;
	MarkSourcesAffectedByOccluderChange(OldFloor, OldBounds, FSightWeaveFloorId(), FBox2D(ForceInit));
	PublishSnapshot();
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

FSightWeaveHardSuppressionHandle USightWeaveWorldSubsystem::RegisterHardLiveSuppression(
	const FSightWeaveHardSuppressionDescription& Description,
	UObject* Owner)
{
	if (!bSightWeaveInitialized || !Description.IsValid())
	{
		return FSightWeaveHardSuppressionHandle();
	}
	const int64 NewId = NextHardSuppressionId++;
	HardSuppressions.Add(NewId, Description);
	if (Owner)
	{
		HardSuppressionOwners.Add(NewId, Owner);
	}
	AdvanceRevision();
	HardSuppressionRevisions.Add(NewId, Revision);
	PublishSnapshot();
	return FSightWeaveHardSuppressionHandle(NewId);
}

bool USightWeaveWorldSubsystem::UpdateHardLiveSuppression(
	const FSightWeaveHardSuppressionHandle Handle,
	const FSightWeaveHardSuppressionDescription& Description)
{
	if (!bSightWeaveInitialized || !Description.IsValid() || !HardSuppressions.Contains(Handle.GetValue()))
	{
		return false;
	}
	HardSuppressions.FindChecked(Handle.GetValue()) = Description;
	AdvanceRevision();
	HardSuppressionRevisions.Add(Handle.GetValue(), Revision);
	PublishSnapshot();
	return true;
}

bool USightWeaveWorldSubsystem::UnregisterHardLiveSuppression(const FSightWeaveHardSuppressionHandle Handle)
{
	if (!bSightWeaveInitialized || HardSuppressions.Remove(Handle.GetValue()) == 0)
	{
		return false;
	}
	HardSuppressionOwners.Remove(Handle.GetValue());
	HardSuppressionRevisions.Remove(Handle.GetValue());
	AdvanceRevision();
	PublishSnapshot();
	return true;
}

bool USightWeaveWorldSubsystem::IsHardLiveSuppressionHandleValid(
	const FSightWeaveHardSuppressionHandle Handle) const
{
	return bSightWeaveInitialized && Handle.IsValid() && HardSuppressions.Contains(Handle.GetValue());
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
	PublishSnapshot();
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
	PublishSnapshot();
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
	PublishSnapshot();
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

	TArray<FSightWeaveHardSuppressionHandle> OwnedSuppressions;
	for (const TPair<int64, TWeakObjectPtr<UObject>>& Pair : HardSuppressionOwners)
	{
		if (Pair.Value.Get() == Owner) OwnedSuppressions.Emplace(Pair.Key);
	}
	for (const FSightWeaveHardSuppressionHandle Handle : OwnedSuppressions)
	{
		RemovedCount += UnregisterHardLiveSuppression(Handle) ? 1 : 0;
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
	return QueryEffectiveLiveAtLocation(
		FSightWeaveKnowledgeOwnerId(FName(TEXT("Local"))),
		FloorId,
		WorldLocation);
}

FSightWeaveVisibilityQueryResult USightWeaveWorldSubsystem::QueryVisionSourceAtLocation(
	const FSightWeaveVisionSourceHandle Handle,
	const FSightWeaveFloorId FloorId,
	const FVector WorldLocation) const
{
	if (!IsVisionSourceHandleValid(Handle))
	{
		return MakeQueryResult(
			ESightWeaveQueryStatus::InvalidHandle,
			FSightWeaveKnowledgeOwnerId(FName(TEXT("Local"))),
			FloorId);
	}
	const FSightWeaveVisionSourceDescription& Source = VisionSources.FindChecked(Handle.GetValue());
	return QueryVisionSourceHardLiveAtLocation(Handle, Source.KnowledgeOwnerId, FloorId, WorldLocation);
}

FSightWeaveVisibilityQueryResult USightWeaveWorldSubsystem::QueryPureVisionAtLocation(
	const FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
	const FSightWeaveFloorId FloorId,
	const FVector WorldLocation) const
{
	return QueryEffectiveLiveInternal(KnowledgeOwnerId, FloorId, WorldLocation, nullptr, true);
}

FSightWeaveVisibilityQueryResult USightWeaveWorldSubsystem::QueryEffectiveLiveAtLocation(
	const FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
	const FSightWeaveFloorId FloorId,
	const FVector WorldLocation) const
{
	return QueryEffectiveLiveInternal(KnowledgeOwnerId, FloorId, WorldLocation, nullptr, false);
}

FSightWeaveVisibilityQueryResult USightWeaveWorldSubsystem::QueryVisionSourceHardLiveAtLocation(
	const FSightWeaveVisionSourceHandle Handle,
	const FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
	const FSightWeaveFloorId FloorId,
	const FVector WorldLocation) const
{
	if (!IsVisionSourceHandleValid(Handle))
	{
		return MakeQueryResult(ESightWeaveQueryStatus::InvalidHandle, KnowledgeOwnerId, FloorId);
	}
	return QueryEffectiveLiveInternal(KnowledgeOwnerId, FloorId, WorldLocation, &Handle, false);
}

FSightWeaveVisibilityQueryResult USightWeaveWorldSubsystem::QueryEffectiveLiveInternal(
	const FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
	const FSightWeaveFloorId FloorId,
	const FVector WorldLocation,
	const FSightWeaveVisionSourceHandle* RestrictToSource,
	const bool bPureVision) const
{
	if (!FloorId.IsValid())
	{
		return MakeQueryResult(ESightWeaveQueryStatus::InvalidFloor, KnowledgeOwnerId, FloorId);
	}
	if (!KnowledgeOwnerId.IsValid() || WorldLocation.ContainsNaN())
	{
		return MakeQueryResult(ESightWeaveQueryStatus::InvalidInput, KnowledgeOwnerId, FloorId);
	}
	const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> Snapshot = PublishedSnapshot;
	if (!bSightWeaveInitialized || !Snapshot.IsValid() || !Snapshot->bPublished)
	{
		return MakeQueryResult(ESightWeaveQueryStatus::NotReady, KnowledgeOwnerId, FloorId);
	}
	const FSightWeaveFloorDefinition* Floor = Snapshot->Floors.FindByPredicate(
		[FloorId](const FSightWeaveFloorDefinition& Candidate) { return Candidate.FloorId == FloorId; });
	if (!Floor)
	{
		return MakeQueryResult(
			Snapshot->Floors.IsEmpty() ? ESightWeaveQueryStatus::NotReady : ESightWeaveQueryStatus::InvalidFloor,
			KnowledgeOwnerId,
			FloorId);
	}

	FSightWeaveVisibilityQueryResult Result = MakeQueryResult(
		ESightWeaveQueryStatus::AuthoritativeResult,
		KnowledgeOwnerId,
		FloorId);
	Result.bAuthoritative = true;
	const FSightWeaveGeometryTolerances& Tolerances = GetDefault<USightWeaveSettings>()->GeometryTolerances;
	const FVector2D Point2D(WorldLocation.X, WorldLocation.Y);
	const bool bInsideFloorBounds = WorldLocation.X >= Floor->BoundsMin.X - Tolerances.PointOnEdgeEpsilon
		&& WorldLocation.X <= Floor->BoundsMax.X + Tolerances.PointOnEdgeEpsilon
		&& WorldLocation.Y >= Floor->BoundsMin.Y - Tolerances.PointOnEdgeEpsilon
		&& WorldLocation.Y <= Floor->BoundsMax.Y + Tolerances.PointOnEdgeEpsilon;
	if (!Floor->bEnabled || !Floor->bActiveForQueries || !bInsideFloorBounds)
	{
		Result.RejectionFlags |= static_cast<int32>(ESightWeaveQueryRejectionReason::FloorUnavailable);
		return Result;
	}
	if (!IsPointInHeightRange(WorldLocation.Z, Floor->HeightRange, Tolerances.HeightOverlapEpsilon))
	{
		Result.RejectionFlags |= static_cast<int32>(ESightWeaveQueryRejectionReason::HeightMismatch);
		return Result;
	}

	bool bNominalVisionCoverage = false;
	bool bHeightMismatch = false;
	bool bEffectiveBeforeSuppression = false;
	for (const FSightWeaveVisionSnapshotEntry& Entry : Snapshot->VisionSources)
	{
		if ((RestrictToSource && Entry.Handle != *RestrictToSource)
			|| Entry.Description.KnowledgeOwnerId != KnowledgeOwnerId
			|| Entry.Description.FloorId != FloorId
			|| !Entry.Description.bActive)
		{
			continue;
		}
		if (!IsPointInHeightRange(WorldLocation.Z, Entry.Description.HeightRange, Tolerances.HeightOverlapEpsilon))
		{
			bHeightMismatch = true;
			continue;
		}
		bNominalVisionCoverage |= IsPointInNominalShape(
			WorldLocation,
			Entry.Description.Transform,
			Entry.Description.Shape,
			Entry.Description.Range,
			Entry.Description.HalfAngleDegrees,
			Entry.Description.NearAwarenessRadius,
			Tolerances.PointOnEdgeEpsilon);
		if (!Entry.Polygon.IsValid()
			|| !SightWeave::Geometry::IsPointInPolygon(Point2D, Entry.Polygon.Vertices, Tolerances))
		{
			continue;
		}

		Result.bInVisionPolygon = true;
		AddUniqueHandle(Result.ContributingVisionSources, Entry.Handle);
		if (bPureVision)
		{
			bEffectiveBeforeSuppression = true;
			continue;
		}
		if (Entry.Description.IlluminationPolicy == ESightWeaveIlluminationPolicy::BypassLegalIllumination)
		{
			Result.bUsedBypass = true;
			bEffectiveBeforeSuppression = true;
			continue;
		}

		bool bThisSourceHasIllumination = false;
		for (const FSightWeaveIlluminationSourceHandle CompatibleHandle : Entry.CompatibleIlluminationSources)
		{
			const FSightWeaveIlluminationSnapshotEntry* Illumination = Snapshot->IlluminationSources.FindByPredicate(
				[CompatibleHandle](const FSightWeaveIlluminationSnapshotEntry& Candidate)
				{
					return Candidate.Handle == CompatibleHandle;
				});
			if (!Illumination
				|| !Illumination->Description.bActive
				|| !IsPointInHeightRange(WorldLocation.Z, Illumination->Description.HeightRange, Tolerances.HeightOverlapEpsilon)
				|| !Illumination->Polygon.IsValid()
				|| !SightWeave::Geometry::IsPointInPolygon(Point2D, Illumination->Polygon.Vertices, Tolerances))
			{
				continue;
			}
			bThisSourceHasIllumination = true;
			Result.bHasLegalIllumination = true;
			AddUniqueHandle(Result.ContributingIlluminationSources, CompatibleHandle);
		}
		if (bThisSourceHasIllumination)
		{
			bEffectiveBeforeSuppression = true;
		}
		else
		{
			Result.bRejectedByIllumination = true;
			Result.RejectionFlags |= static_cast<int32>(ESightWeaveQueryRejectionReason::MissingCompatibleIllumination);
		}
	}

	if (!Result.bInVisionPolygon)
	{
		Result.bOccluded = bNominalVisionCoverage;
		Result.RejectionFlags |= static_cast<int32>(ESightWeaveQueryRejectionReason::OutsideVision);
		if (bHeightMismatch)
		{
			Result.RejectionFlags |= static_cast<int32>(ESightWeaveQueryRejectionReason::HeightMismatch);
		}
	}

	if (!bPureVision && bEffectiveBeforeSuppression)
	{
		Result.bRejectedBySuppression = IsPointSuppressed(*Snapshot, FloorId, WorldLocation, Result.ContributingSuppressions);
		if (Result.bRejectedBySuppression)
		{
			Result.RejectionFlags |= static_cast<int32>(ESightWeaveQueryRejectionReason::SuppressedLiveVision);
		}
	}
	Result.bVisible = bEffectiveBeforeSuppression && (bPureVision || !Result.bRejectedBySuppression);
	Result.KnowledgeState = Result.bVisible ? ESightWeaveKnowledgeState::Visible : ESightWeaveKnowledgeState::Unknown;
	Result.bEligibleForMemoryWrite = Result.bVisible && !bPureVision;
	Result.EvaluatedSampleCount = 1;
	Result.PassingSampleCount = Result.bVisible ? 1 : 0;
	return Result;
}

FSightWeaveIlluminationQueryResult USightWeaveWorldSubsystem::QueryLegalIlluminationAtLocation(
	const FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
	const FSightWeaveFloorId FloorId,
	const FVector WorldLocation) const
{
	if (!FloorId.IsValid())
	{
		return MakeIlluminationQueryResult(ESightWeaveQueryStatus::InvalidFloor, KnowledgeOwnerId, FloorId);
	}
	if (!KnowledgeOwnerId.IsValid() || WorldLocation.ContainsNaN())
	{
		return MakeIlluminationQueryResult(ESightWeaveQueryStatus::InvalidInput, KnowledgeOwnerId, FloorId);
	}
	const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> Snapshot = PublishedSnapshot;
	if (!bSightWeaveInitialized || !Snapshot.IsValid() || !Snapshot->bPublished)
	{
		return MakeIlluminationQueryResult(ESightWeaveQueryStatus::NotReady, KnowledgeOwnerId, FloorId);
	}
	const FSightWeaveFloorDefinition* Floor = Snapshot->Floors.FindByPredicate(
		[FloorId](const FSightWeaveFloorDefinition& Candidate) { return Candidate.FloorId == FloorId; });
	if (!Floor)
	{
		return MakeIlluminationQueryResult(
			Snapshot->Floors.IsEmpty() ? ESightWeaveQueryStatus::NotReady : ESightWeaveQueryStatus::InvalidFloor,
			KnowledgeOwnerId,
			FloorId);
	}
	FSightWeaveIlluminationQueryResult Result = MakeIlluminationQueryResult(
		ESightWeaveQueryStatus::AuthoritativeResult,
		KnowledgeOwnerId,
		FloorId);
	Result.bAuthoritative = true;
	const FSightWeaveGeometryTolerances& Tolerances = GetDefault<USightWeaveSettings>()->GeometryTolerances;
	const bool bInsideFloorBounds = WorldLocation.X >= Floor->BoundsMin.X - Tolerances.PointOnEdgeEpsilon
		&& WorldLocation.X <= Floor->BoundsMax.X + Tolerances.PointOnEdgeEpsilon
		&& WorldLocation.Y >= Floor->BoundsMin.Y - Tolerances.PointOnEdgeEpsilon
		&& WorldLocation.Y <= Floor->BoundsMax.Y + Tolerances.PointOnEdgeEpsilon;
	if (!Floor->bEnabled || !Floor->bActiveForQueries || !bInsideFloorBounds)
	{
		return Result;
	}
	if (!IsPointInHeightRange(WorldLocation.Z, Floor->HeightRange, Tolerances.HeightOverlapEpsilon))
	{
		return Result;
	}
	const FVector2D Point2D(WorldLocation.X, WorldLocation.Y);
	bool bNominalCoverage = false;
	for (const FSightWeaveIlluminationSnapshotEntry& Entry : Snapshot->IlluminationSources)
	{
		if (Entry.Description.KnowledgeOwnerId != KnowledgeOwnerId
			|| Entry.Description.FloorId != FloorId
			|| !Entry.Description.bActive
			|| !IsPointInHeightRange(WorldLocation.Z, Entry.Description.HeightRange, Tolerances.HeightOverlapEpsilon))
		{
			continue;
		}
		bNominalCoverage |= IsPointInNominalShape(
			WorldLocation,
			Entry.Description.Transform,
			Entry.Description.Shape,
			Entry.Description.Range,
			Entry.Description.HalfAngleDegrees,
			0.0,
			Tolerances.PointOnEdgeEpsilon);
		if (Entry.Polygon.IsValid()
			&& SightWeave::Geometry::IsPointInPolygon(Point2D, Entry.Polygon.Vertices, Tolerances))
		{
			Result.bLegallyIlluminated = true;
			AddUniqueHandle(Result.ContributingIlluminationSources, Entry.Handle);
		}
	}
	Result.bOccluded = bNominalCoverage && !Result.bLegallyIlluminated;
	return Result;
}

bool USightWeaveWorldSubsystem::IsPointSuppressed(
	const FSightWeaveFrameSnapshot& Snapshot,
	const FSightWeaveFloorId FloorId,
	const FVector WorldLocation,
	TArray<FSightWeaveHardSuppressionHandle>& OutHandles) const
{
	const double HeightEpsilon = GetDefault<USightWeaveSettings>()->GeometryTolerances.HeightOverlapEpsilon;
	const FVector2D Point(WorldLocation.X, WorldLocation.Y);
	for (const FSightWeaveHardSuppressionSnapshotEntry& Entry : Snapshot.HardSuppressions)
	{
		const FSightWeaveHardSuppressionDescription& Description = Entry.Description;
		if (Description.bEnabled
			&& Description.FloorId == FloorId
			&& IsPointInHeightRange(WorldLocation.Z, Description.HeightRange, HeightEpsilon)
			&& FVector2D::DistSquared(Point, Description.Center) <= FMath::Square(static_cast<double>(Description.Radius)))
		{
			OutHandles.Add(Entry.Handle);
		}
	}
	return !OutHandles.IsEmpty();
}

FSightWeaveVisibilityQueryResult USightWeaveWorldSubsystem::QuerySamples(
	const FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
	const FSightWeaveFloorId FloorId,
	const FSightWeaveQuerySampleSet& SampleSet) const
{
	if (!SampleSet.IsValid())
	{
		return MakeQueryResult(ESightWeaveQueryStatus::InvalidInput, KnowledgeOwnerId, FloorId);
	}
	if (SampleSet.Rule == ESightWeaveSampleRule::Anchor)
	{
		return QueryEffectiveLiveAtLocation(KnowledgeOwnerId, FloorId, SampleSet.Samples[SampleSet.AnchorIndex]);
	}

	FSightWeaveVisibilityQueryResult Result = MakeQueryResult(
		ESightWeaveQueryStatus::AuthoritativeResult,
		KnowledgeOwnerId,
		FloorId);
	Result.bAuthoritative = true;
	for (const FVector& Sample : SampleSet.Samples)
	{
		const FSightWeaveVisibilityQueryResult SampleResult = QueryEffectiveLiveAtLocation(
			KnowledgeOwnerId,
			FloorId,
			Sample);
		if (SampleResult.Status != ESightWeaveQueryStatus::AuthoritativeResult)
		{
			return SampleResult;
		}
		++Result.EvaluatedSampleCount;
		Result.PassingSampleCount += SampleResult.bVisible ? 1 : 0;
		Result.bInVisionPolygon |= SampleResult.bInVisionPolygon;
		Result.bHasLegalIllumination |= SampleResult.bHasLegalIllumination;
		Result.bUsedBypass |= SampleResult.bUsedBypass;
		Result.bOccluded |= SampleResult.bOccluded;
		Result.bRejectedByIllumination |= SampleResult.bRejectedByIllumination;
		Result.bRejectedBySuppression |= SampleResult.bRejectedBySuppression;
		Result.RejectionFlags |= SampleResult.RejectionFlags;
		for (const FSightWeaveVisionSourceHandle Handle : SampleResult.ContributingVisionSources)
		{
			AddUniqueHandle(Result.ContributingVisionSources, Handle);
		}
		for (const FSightWeaveIlluminationSourceHandle Handle : SampleResult.ContributingIlluminationSources)
		{
			AddUniqueHandle(Result.ContributingIlluminationSources, Handle);
		}
		for (const FSightWeaveHardSuppressionHandle Handle : SampleResult.ContributingSuppressions)
		{
			AddUniqueHandle(Result.ContributingSuppressions, Handle);
		}
	}
	Result.bVisible = SampleSet.Rule == ESightWeaveSampleRule::AnySample
		? Result.PassingSampleCount > 0
		: SampleSet.Rule == ESightWeaveSampleRule::AllSamples
			? Result.PassingSampleCount == Result.EvaluatedSampleCount
			: Result.PassingSampleCount >= SampleSet.RequiredCount;
	Result.KnowledgeState = Result.bVisible ? ESightWeaveKnowledgeState::Visible : ESightWeaveKnowledgeState::Unknown;
	Result.bEligibleForMemoryWrite = Result.bVisible;
	return Result;
}

FSightWeaveVisibilityQueryResult USightWeaveWorldSubsystem::QueryBounds(
	const FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
	const FSightWeaveFloorId FloorId,
	const FBox WorldBounds,
	const ESightWeaveSampleRule Rule,
	const int32 RequiredCount) const
{
	if (!WorldBounds.IsValid || WorldBounds.Min.ContainsNaN() || WorldBounds.Max.ContainsNaN())
	{
		return MakeQueryResult(ESightWeaveQueryStatus::InvalidInput, KnowledgeOwnerId, FloorId);
	}
	FSightWeaveQuerySampleSet Samples;
	Samples.Rule = Rule;
	Samples.RequiredCount = RequiredCount;
	Samples.AnchorIndex = 0;
	Samples.Samples.Add(WorldBounds.GetCenter());
	for (const double X : { WorldBounds.Min.X, WorldBounds.Max.X })
	{
		for (const double Y : { WorldBounds.Min.Y, WorldBounds.Max.Y })
		{
			for (const double Z : { WorldBounds.Min.Z, WorldBounds.Max.Z })
			{
				Samples.Samples.Emplace(X, Y, Z);
			}
		}
	}
	return QuerySamples(KnowledgeOwnerId, FloorId, Samples);
}

void USightWeaveWorldSubsystem::QueryBatch(
	const TArray<FSightWeaveQueryRequest>& Requests,
	TArray<FSightWeaveVisibilityQueryResult>& OutResults) const
{
	OutResults.Reset(Requests.Num());
	for (const FSightWeaveQueryRequest& Request : Requests)
	{
		OutResults.Add(QuerySamples(Request.KnowledgeOwnerId, Request.FloorId, Request.SampleSet));
	}
}

FSightWeaveRevision USightWeaveWorldSubsystem::PublishSnapshot()
{
	if (!bSightWeaveInitialized)
	{
		return FSightWeaveRevision();
	}

	TArray<int64> DirtyVision = PendingVisionSnapshotRebuilds.Array();
	TArray<int64> DirtyIllumination = PendingIlluminationSnapshotRebuilds.Array();
	DirtyVision.Sort();
	DirtyIllumination.Sort();
	for (const int64 SourceId : DirtyVision)
	{
		RebuildVisionSnapshotEntry(SourceId);
	}
	for (const int64 SourceId : DirtyIllumination)
	{
		RebuildIlluminationSnapshotEntry(SourceId);
	}
	PendingVisionSnapshotRebuilds.Reset();
	PendingIlluminationSnapshotRebuilds.Reset();

	FSightWeaveFrameSnapshot NewSnapshot;
	NewSnapshot.Revision = Revision;
	NewSnapshot.RebuiltVisionPolygonCount = DirtyVision.Num();
	NewSnapshot.RebuiltIlluminationPolygonCount = DirtyIllumination.Num();
	NewSnapshot.bPublished = true;

	Floors.GenerateValueArray(NewSnapshot.Floors);
	NewSnapshot.Floors.Sort([](const FSightWeaveFloorDefinition& A, const FSightWeaveFloorDefinition& B)
	{
		return A.FloorId.GetValue().LexicalLess(B.FloorId.GetValue());
	});

	TArray<int64> VisionIds;
	CachedVisionSnapshotEntries.GenerateKeyArray(VisionIds);
	VisionIds.Sort();
	for (const int64 SourceId : VisionIds)
	{
		NewSnapshot.VisionSources.Add(CachedVisionSnapshotEntries.FindChecked(SourceId));
	}

	TArray<int64> IlluminationIds;
	CachedIlluminationSnapshotEntries.GenerateKeyArray(IlluminationIds);
	IlluminationIds.Sort();
	for (const int64 SourceId : IlluminationIds)
	{
		NewSnapshot.IlluminationSources.Add(CachedIlluminationSnapshotEntries.FindChecked(SourceId));
	}

	for (const TPair<int64, FOccluderRecord>& Pair : Occluders)
	{
		if (Pair.Value.bEnabled)
		{
			NewSnapshot.OccluderSegments.Append(Pair.Value.Segments);
		}
	}
	NewSnapshot.OccluderSegments.Sort([](const FSightWeaveSegment2D& A, const FSightWeaveSegment2D& B)
	{
		return A.StableId < B.StableId;
	});

	TArray<int64> SuppressionIds;
	HardSuppressions.GenerateKeyArray(SuppressionIds);
	SuppressionIds.Sort();
	for (const int64 SuppressionId : SuppressionIds)
	{
		FSightWeaveHardSuppressionSnapshotEntry& Entry = NewSnapshot.HardSuppressions.AddDefaulted_GetRef();
		Entry.Handle = FSightWeaveHardSuppressionHandle(SuppressionId);
		Entry.Description = HardSuppressions.FindChecked(SuppressionId);
		Entry.Revision = HardSuppressionRevisions.FindRef(SuppressionId);
	}

	ResolveSnapshotCompatibility(NewSnapshot);
	TSharedRef<FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> NewSharedSnapshot =
		MakeShared<FSightWeaveFrameSnapshot, ESPMode::ThreadSafe>(MoveTemp(NewSnapshot));
	PublishedSnapshot = NewSharedSnapshot;
	return Revision;
}

FSightWeaveFrameSnapshot USightWeaveWorldSubsystem::GetPublishedSnapshot() const
{
	const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> Snapshot = PublishedSnapshot;
	return Snapshot.IsValid() ? *Snapshot : FSightWeaveFrameSnapshot();
}

void USightWeaveWorldSubsystem::RebuildVisionSnapshotEntry(const int64 SourceId)
{
	const FSightWeaveVisionSourceDescription* Description = VisionSources.Find(SourceId);
	if (!Description)
	{
		CachedVisionSnapshotEntries.Remove(SourceId);
		return;
	}

	FSightWeaveVisionSnapshotEntry Entry;
	Entry.Handle = FSightWeaveVisionSourceHandle(SourceId);
	Entry.Description = *Description;
	Entry.SourceRevision = VisionSourceRevisions.FindRef(SourceId);
	Entry.Polygon.SourceHandle = Entry.Handle;
	Entry.Polygon.KnowledgeOwnerId = Description->KnowledgeOwnerId;
	Entry.Polygon.FloorId = Description->FloorId;
	Entry.Polygon.Revision = Revision;
	Entry.Polygon.SourceRevision = Entry.SourceRevision;
	Entry.Polygon.OccluderRevision = LastOccluderRevision;

	const FSightWeaveFloorDefinition* Floor = Floors.Find(Description->FloorId);
	if (Description->bActive && Floor && Floor->bEnabled && Floor->bActiveForQueries)
	{
		FSightWeaveReferenceSolveInput Input;
		Input.Origin = Description->Transform.GetLocation();
		const FVector Forward3 = Description->Transform.GetUnitAxis(EAxis::X);
		Input.Forward = FVector2D(Forward3.X, Forward3.Y);
		if (!Input.Forward.Normalize()) Input.Forward = FVector2D(1.0, 0.0);
		Input.Shape = Description->Shape;
		Input.Range = Description->Range;
		Input.HalfAngleDegrees = Description->HalfAngleDegrees;
		Input.NearAwarenessRadius = Description->NearAwarenessRadius;
		Input.FloorId = Description->FloorId;
		Input.HeightRange = Description->HeightRange;
		const USightWeaveSettings* Settings = GetDefault<USightWeaveSettings>();
		Input.Tolerances = Settings->GeometryTolerances;
		Input.Tolerances.Normalize();
		const FVector Origin = Description->Transform.GetLocation();
		const FBox2D QueryBounds(
			FVector2D(Origin.X - Description->Range, Origin.Y - Description->Range),
			FVector2D(Origin.X + Description->Range, Origin.Y + Description->Range));
		QueryOccluderSegments(Description->FloorId, QueryBounds, Description->HeightRange, Input.Segments);

		const double StartSeconds = FPlatformTime::Seconds();
		FSightWeaveReferenceSolveResult SolveResult = SightWeave::Geometry::SolvePolygon(Input, Settings->SolverMode);
		Entry.SolveTimeMicroseconds = (FPlatformTime::Seconds() - StartSeconds) * 1000000.0;
		Entry.CandidateSegmentCount = SolveResult.CandidateSegmentCount;
		Entry.CandidateRayCount = SolveResult.CastRayCount;
		Entry.CandidateAnglesRadians = MoveTemp(SolveResult.CandidateAnglesRadians);
		if (SolveResult.bSucceeded)
		{
			Entry.Polygon.Vertices = MoveTemp(SolveResult.Vertices);
			SetPolygonBounds(Entry.Polygon.Vertices, Entry.Polygon.BoundsMin, Entry.Polygon.BoundsMax);
		}
	}
	CachedVisionSnapshotEntries.Add(SourceId, MoveTemp(Entry));
}

void USightWeaveWorldSubsystem::RebuildIlluminationSnapshotEntry(const int64 SourceId)
{
	const FSightWeaveIlluminationSourceDescription* Description = IlluminationSources.Find(SourceId);
	if (!Description)
	{
		CachedIlluminationSnapshotEntries.Remove(SourceId);
		return;
	}

	FSightWeaveIlluminationSnapshotEntry Entry;
	Entry.Handle = FSightWeaveIlluminationSourceHandle(SourceId);
	Entry.Description = *Description;
	Entry.SourceRevision = IlluminationSourceRevisions.FindRef(SourceId);
	Entry.Polygon.SourceHandle = Entry.Handle;
	Entry.Polygon.KnowledgeOwnerId = Description->KnowledgeOwnerId;
	Entry.Polygon.FloorId = Description->FloorId;
	Entry.Polygon.Revision = Revision;
	Entry.Polygon.SourceRevision = Entry.SourceRevision;
	Entry.Polygon.OccluderRevision = LastOccluderRevision;

	const FSightWeaveFloorDefinition* Floor = Floors.Find(Description->FloorId);
	if (Description->bActive && Floor && Floor->bEnabled && Floor->bActiveForQueries)
	{
		FSightWeaveReferenceSolveInput Input;
		Input.Origin = Description->Transform.GetLocation();
		const FVector Forward3 = Description->Transform.GetUnitAxis(EAxis::X);
		Input.Forward = FVector2D(Forward3.X, Forward3.Y);
		if (!Input.Forward.Normalize()) Input.Forward = FVector2D(1.0, 0.0);
		Input.Shape = Description->Shape;
		Input.Range = Description->Range;
		Input.HalfAngleDegrees = Description->HalfAngleDegrees;
		Input.NearAwarenessRadius = 0.0;
		Input.FloorId = Description->FloorId;
		Input.HeightRange = Description->HeightRange;
		const USightWeaveSettings* Settings = GetDefault<USightWeaveSettings>();
		Input.Tolerances = Settings->GeometryTolerances;
		Input.Tolerances.Normalize();
		const FVector Origin = Description->Transform.GetLocation();
		const FBox2D QueryBounds(
			FVector2D(Origin.X - Description->Range, Origin.Y - Description->Range),
			FVector2D(Origin.X + Description->Range, Origin.Y + Description->Range));
		QueryOccluderSegments(Description->FloorId, QueryBounds, Description->HeightRange, Input.Segments);

		const double StartSeconds = FPlatformTime::Seconds();
		FSightWeaveReferenceSolveResult SolveResult = SightWeave::Geometry::SolvePolygon(Input, Settings->SolverMode);
		Entry.SolveTimeMicroseconds = (FPlatformTime::Seconds() - StartSeconds) * 1000000.0;
		Entry.CandidateSegmentCount = SolveResult.CandidateSegmentCount;
		Entry.CandidateRayCount = SolveResult.CastRayCount;
		Entry.CandidateAnglesRadians = MoveTemp(SolveResult.CandidateAnglesRadians);
		if (SolveResult.bSucceeded)
		{
			Entry.Polygon.Vertices = MoveTemp(SolveResult.Vertices);
			SetPolygonBounds(Entry.Polygon.Vertices, Entry.Polygon.BoundsMin, Entry.Polygon.BoundsMax);
		}
	}
	CachedIlluminationSnapshotEntries.Add(SourceId, MoveTemp(Entry));
}

void USightWeaveWorldSubsystem::ResolveSnapshotCompatibility(FSightWeaveFrameSnapshot& Snapshot) const
{
	for (FSightWeaveVisionSnapshotEntry& Vision : Snapshot.VisionSources)
	{
		Vision.CompatibleIlluminationSources.Reset();
		if (!Vision.Description.bActive
			|| Vision.Description.IlluminationPolicy == ESightWeaveIlluminationPolicy::BypassLegalIllumination)
		{
			continue;
		}
		for (const FSightWeaveIlluminationSnapshotEntry& Illumination : Snapshot.IlluminationSources)
		{
			if (Illumination.Description.bActive
				&& Illumination.Description.KnowledgeOwnerId == Vision.Description.KnowledgeOwnerId
				&& Illumination.Description.FloorId == Vision.Description.FloorId
				&& AreCapabilitiesCompatible(
					Vision.Description.Compatibility,
					Illumination.Description.EmittedCapabilities))
			{
				Vision.CompatibleIlluminationSources.Add(Illumination.Handle);
			}
		}
	}
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
			PendingVisionSnapshotRebuilds.Add(Pair.Key);
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
			PendingIlluminationSnapshotRebuilds.Add(Pair.Key);
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
	HardSuppressions.Reset();
	HardSuppressionRevisions.Reset();
	SpatialIndex.Reset();
	DirtyVisionSources.Reset();
	DirtyIlluminationSources.Reset();
	PendingVisionSnapshotRebuilds.Reset();
	PendingIlluminationSnapshotRebuilds.Reset();
	VisionSourceRevisions.Reset();
	IlluminationSourceRevisions.Reset();
	CachedVisionSnapshotEntries.Reset();
	CachedIlluminationSnapshotEntries.Reset();
	PublishedSnapshot.Reset();
	FloorOwners.Reset();
	VisionOwners.Reset();
	IlluminationOwners.Reset();
	SubjectRevealOwners.Reset();
	OccluderOwners.Reset();
	HardSuppressionOwners.Reset();
	NextVisionSourceId = 1;
	NextIlluminationSourceId = 1;
	NextSubjectRevealId = 1;
	NextOccluderId = 1;
	NextHardSuppressionId = 1;
	NextSegmentId = 1;
	Revision = FSightWeaveRevision();
	LastOccluderRevision = FSightWeaveRevision();
}

FSightWeaveVisibilityQueryResult USightWeaveWorldSubsystem::MakeQueryResult(
	const ESightWeaveQueryStatus Status,
	const FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
	const FSightWeaveFloorId FloorId) const
{
	FSightWeaveVisibilityQueryResult Result;
	Result.Status = Status;
	Result.KnowledgeState = ESightWeaveKnowledgeState::Unknown;
	Result.bVisible = false;
	const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> Snapshot = PublishedSnapshot;
	Result.SnapshotRevision = Snapshot.IsValid() ? Snapshot->Revision : Revision;
	Result.Revision = Result.SnapshotRevision;
	Result.KnowledgeOwnerId = KnowledgeOwnerId;
	Result.FloorId = FloorId;
	return Result;
}

FSightWeaveIlluminationQueryResult USightWeaveWorldSubsystem::MakeIlluminationQueryResult(
	const ESightWeaveQueryStatus Status,
	const FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
	const FSightWeaveFloorId FloorId) const
{
	FSightWeaveIlluminationQueryResult Result;
	Result.Status = Status;
	Result.KnowledgeOwnerId = KnowledgeOwnerId;
	Result.FloorId = FloorId;
	const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> Snapshot = PublishedSnapshot;
	Result.SnapshotRevision = Snapshot.IsValid() ? Snapshot->Revision : Revision;
	return Result;
}
