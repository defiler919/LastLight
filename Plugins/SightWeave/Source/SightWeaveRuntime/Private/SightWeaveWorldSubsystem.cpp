#include "SightWeaveWorldSubsystem.h"

#include "Algo/Sort.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Async/ParallelFor.h"
#endif
#include "HAL/PlatformTime.h"
#include "SightWeaveOptimizedSolveCache.h"
#include "SightWeavePreparedEventIndex.h"
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
		const FVector2D& Origin,
		const FVector2D& Forward,
		const ESightWeaveSourceShape Shape,
		const double Range,
		const double MinimumCosine,
		const double NearAwarenessRadius,
		const double Epsilon)
	{
		const FVector2D Offset(WorldLocation.X - Origin.X, WorldLocation.Y - Origin.Y);
		const double DistanceSquared = Offset.SizeSquared();
		if (DistanceSquared > FMath::Square(Range + Epsilon))
		{
			return false;
		}
		if (DistanceSquared <= FMath::Square(NearAwarenessRadius + Epsilon)
			|| Shape == ESightWeaveSourceShape::Radial)
		{
			return true;
		}
		const double Distance = FMath::Sqrt(DistanceSquared);
		return FVector2D::DotProduct(Forward, Offset) >= MinimumCosine * Distance;
	}

	bool AreCapabilitiesCompatible(
		const FSightWeaveIlluminationCompatibilityProfile& Profile,
		TConstArrayView<FName> EmittedCapabilities)
	{
		return EmittedCapabilities.ContainsByPredicate(
			[&Profile](const FName Capability) { return Profile.Accepts(Capability); });
	}

	bool HasIdenticalVisionAndIlluminationGeometry(
		const FSightWeaveVisionSnapshotEntry& Vision,
		const FSightWeaveIlluminationSourceDescription& Illumination)
	{
		return Vision.Description.bActive
			&& Vision.Polygon.IsValid()
			&& Vision.Description.Transform.Equals(Illumination.Transform, 0.0)
			&& Vision.Description.FloorId == Illumination.FloorId
			&& Vision.Description.HeightRange.ZMin == Illumination.HeightRange.ZMin
			&& Vision.Description.HeightRange.ZMax == Illumination.HeightRange.ZMax
			&& Vision.Description.Shape == Illumination.Shape
			&& Vision.Description.Range == Illumination.Range
			&& (Vision.Description.Shape == ESightWeaveSourceShape::Radial
				|| Vision.Description.HalfAngleDegrees == Illumination.HalfAngleDegrees)
			&& Vision.Description.NearAwarenessRadius == 0.0f;
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

	double NormalizeQueryRadians(double Angle)
	{
		constexpr double TwoPi = 2.0 * PI;
		if (Angle < -PI) Angle += TwoPi;
		else if (Angle >= PI) Angle -= TwoPi;
		return Angle;
	}

	double CrossQuery2D(const FVector2D& A, const FVector2D& B)
	{
		return A.X * B.Y - A.Y * B.X;
	}

	void BuildPolarAngleUpperBoundLut(
		TConstArrayView<double> Angles,
		TArray<int32>& OutUpperBounds)
	{
		// This LUT only seeds the exact upper-bound refinement below; reducing its
		// density changes no query result and makes dirty-source publication
		// materially cheaper when several polygons rebuild together.
		constexpr int32 BinCount = 256;
		OutUpperBounds.SetNumUninitialized(BinCount);
		int32 UpperBound = 0;
		for (int32 Bin = 0; Bin < BinCount; ++Bin)
		{
			const double CenterAngle = -PI
				+ (2.0 * PI) * (static_cast<double>(Bin) + 0.5) / static_cast<double>(BinCount);
			while (UpperBound < Angles.Num() && Angles[UpperBound] <= CenterAngle)
			{
				++UpperBound;
			}
			OutUpperBounds[Bin] = UpperBound;
		}
	}

	bool IsPointInPolarVisibilityPolygon(
		const FVector2D& Point,
		const FVector2D& Origin,
		const double ForwardAngle,
		const bool bFullCircle,
		TConstArrayView<double> Angles,
		TConstArrayView<FVector2D> BoundaryPoints,
		TConstArrayView<int32> AngleUpperBoundLut,
		TConstArrayView<FVector> Vertices,
		const FVector2D& BoundsMin,
		const FVector2D& BoundsMax,
		const FSightWeaveGeometryTolerances& Tolerances)
	{
		auto ExactFallback = [&]()
		{
			return SightWeave::Geometry::IsPointInPolygon(Point, Vertices, Tolerances);
		};
		if (Angles.Num() < 2 || Angles.Num() != BoundaryPoints.Num())
		{
			return ExactFallback();
		}
		const double BoundsEpsilon = FMath::Max3(
			Tolerances.PointOnEdgeEpsilon,
			Tolerances.PointInPolygonEpsilon,
			Tolerances.DuplicateVertexEpsilon);
		if (Point.X < BoundsMin.X - BoundsEpsilon
			|| Point.X > BoundsMax.X + BoundsEpsilon
			|| Point.Y < BoundsMin.Y - BoundsEpsilon
			|| Point.Y > BoundsMax.Y + BoundsEpsilon)
		{
			return false;
		}

		const FVector2D Offset = Point - Origin;
		const double QueryDistanceSquared = Offset.SizeSquared();
		if (QueryDistanceSquared <= FMath::Square(Tolerances.PointOnEdgeEpsilon))
		{
			return ExactFallback();
		}
		const double RelativeAngle = NormalizeQueryRadians(
			FMath::Atan2(Offset.Y, Offset.X) - ForwardAngle);

		int32 FirstGreater = 0;
		if (!AngleUpperBoundLut.IsEmpty())
		{
			const double Normalized = FMath::Clamp((RelativeAngle + PI) / (2.0 * PI), 0.0, 1.0);
			const int32 Bin = FMath::Min(
				FMath::FloorToInt(Normalized * AngleUpperBoundLut.Num()),
				AngleUpperBoundLut.Num() - 1);
			FirstGreater = FMath::Clamp(AngleUpperBoundLut[Bin], 0, Angles.Num());
			while (FirstGreater > 0 && Angles[FirstGreater - 1] > RelativeAngle)
			{
				--FirstGreater;
			}
			while (FirstGreater < Angles.Num() && Angles[FirstGreater] <= RelativeAngle)
			{
				++FirstGreater;
			}
		}
		else
		{
			int32 UpperBound = Angles.Num();
			while (FirstGreater < UpperBound)
			{
				const int32 Middle = FirstGreater + (UpperBound - FirstGreater) / 2;
				if (Angles[Middle] <= RelativeAngle)
				{
					FirstGreater = Middle + 1;
				}
				else
				{
					UpperBound = Middle;
				}
			}
		}

		int32 LowerIndex = INDEX_NONE;
		int32 UpperIndex = INDEX_NONE;
		if (bFullCircle)
		{
			LowerIndex = FirstGreater == 0 ? Angles.Num() - 1 : FirstGreater - 1;
			UpperIndex = FirstGreater == Angles.Num() ? 0 : FirstGreater;
		}
		else
		{
			if (RelativeAngle < Angles[0] - 1.0e-12
				|| RelativeAngle > Angles.Last() + 1.0e-12)
			{
				return ExactFallback();
			}
			UpperIndex = FMath::Clamp(FirstGreater, 1, Angles.Num() - 1);
			LowerIndex = UpperIndex - 1;
		}

		const FVector2D A = BoundaryPoints[LowerIndex];
		const FVector2D B = BoundaryPoints[UpperIndex];
		const FVector2D Edge = B - A;
		const double Denominator = CrossQuery2D(Offset, Edge);
		if (FMath::Square(Denominator)
			<= FMath::Square(Tolerances.RayParallelEpsilon) * QueryDistanceSquared)
		{
			return ExactFallback();
		}
		const double Numerator = CrossQuery2D(A - Origin, Edge);
		const double EdgeLengthSquared = Edge.SizeSquared();
		if (!FMath::IsFinite(Denominator)
			|| !FMath::IsFinite(Numerator)
			|| (Numerator > 0.0 && Denominator < 0.0)
			|| (Numerator < 0.0 && Denominator > 0.0)
			|| EdgeLengthSquared <= UE_DOUBLE_SMALL_NUMBER)
		{
			return ExactFallback();
		}

		const double BoundaryEpsilon = FMath::Max3(
			Tolerances.PointOnEdgeEpsilon,
			Tolerances.PointInPolygonEpsilon,
			Tolerances.DuplicateVertexEpsilon);
		// Parameterize the query point itself as t=1 on the unnormalized ray.
		// |denominator - numerator| is algebraically identical to the former
		// distance-scaled boundary difference, without sqrt or division.
		const double ScaledRadialDifference = FMath::Abs(Denominator - Numerator);
		if (FMath::Square(ScaledRadialDifference)
			<= FMath::Square(BoundaryEpsilon) * EdgeLengthSquared)
		{
			return ExactFallback();
		}
		return Denominator > 0.0 ? Numerator > Denominator : Numerator < Denominator;
	}

	bool IsPointInVisionSnapshotEntry(
		const FVector2D& Point,
		const FSightWeaveVisionSnapshotEntry& Entry,
		const FSightWeaveGeometryTolerances& Tolerances)
	{
		return IsPointInPolarVisibilityPolygon(
			Point,
			Entry.PolarOrigin,
			Entry.PolarForwardAngleRadians,
			Entry.bPolarBoundaryFullCircle,
			Entry.CandidateAnglesRadians,
			Entry.CandidateBoundaryPoints,
			Entry.PolarAngleUpperBoundLut,
			Entry.Polygon.Vertices,
			Entry.Polygon.BoundsMin,
			Entry.Polygon.BoundsMax,
			Tolerances);
	}

	bool IsPointInIlluminationSnapshotEntry(
		const FVector2D& Point,
		const FSightWeaveIlluminationSnapshotEntry& Entry,
		const FSightWeaveGeometryTolerances& Tolerances)
	{
		return IsPointInPolarVisibilityPolygon(
			Point,
			Entry.PolarOrigin,
			Entry.PolarForwardAngleRadians,
			Entry.bPolarBoundaryFullCircle,
			Entry.CandidateAnglesRadians,
			Entry.CandidateBoundaryPoints,
			Entry.PolarAngleUpperBoundLut,
			Entry.Polygon.Vertices,
			Entry.Polygon.BoundsMin,
			Entry.Polygon.BoundsMax,
			Tolerances);
	}
}

void USightWeaveWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ResetState();
	const USightWeaveSettings* Settings = GetDefault<USightWeaveSettings>();
	SpatialIndex.SetCellSize(Settings->SpatialCellSizeCentimeters);
	PreparedEventIndex = MakeShared<FSightWeavePreparedEventIndex>();
	PreparedEventIndex->Initialize(
		Settings->MaximumPreparedOriginEntries,
		Settings->MaximumPreparedOriginBytes);
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
	const FSightWeaveVisionSourceDescription& ExistingDescription = VisionSources.FindChecked(Handle.GetValue());
	if (FSightWeaveVisionSourceDescription::StaticStruct()->CompareScriptStruct(
		&ExistingDescription,
		&NormalizedDescription,
		0))
	{
		return true;
	}

	VisionSources.FindChecked(Handle.GetValue()) = MoveTemp(NormalizedDescription);
	DirtyVisionSources.Add(Handle.GetValue());
	PendingVisionSnapshotRebuilds.Add(Handle.GetValue());
	AdvanceRevision();
	VisionSourceRevisions.Add(Handle.GetValue(), Revision);
	PublishSnapshot();
	return true;
}

bool USightWeaveWorldSubsystem::UpdateVisionSourceTransform(
	const FSightWeaveVisionSourceHandle Handle,
	const FTransform& Transform)
{
	FSightWeaveVisionSourceDescription* Description = VisionSources.Find(Handle.GetValue());
	if (!bSightWeaveInitialized || !Description || Transform.ContainsNaN())
	{
		return false;
	}
	if (Description->Transform.Equals(Transform, 0.0))
	{
		return true;
	}
	Description->Transform = Transform;
	DirtyVisionSources.Add(Handle.GetValue());
	PendingVisionSnapshotRebuilds.Add(Handle.GetValue());
	AdvanceRevision();
	VisionSourceRevisions.FindChecked(Handle.GetValue()) = Revision;
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
	CachedVisionSolveSegments.Remove(Handle.GetValue());
	CachedVisionCandidateQueryKeys.Remove(Handle.GetValue());
	if (PreparedEventIndex.IsValid())
	{
		PreparedEventIndex->Release(CachedVisionPreparedSolves.FindRef(Handle.GetValue()));
	}
	CachedVisionPreparedSolves.Remove(Handle.GetValue());
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
	const FSightWeaveIlluminationSourceDescription& ExistingDescription =
		IlluminationSources.FindChecked(Handle.GetValue());
	if (FSightWeaveIlluminationSourceDescription::StaticStruct()->CompareScriptStruct(
		&ExistingDescription,
		&NormalizedDescription,
		0))
	{
		return true;
	}
	IlluminationSources.FindChecked(Handle.GetValue()) = MoveTemp(NormalizedDescription);
	DirtyIlluminationSources.Add(Handle.GetValue());
	PendingIlluminationSnapshotRebuilds.Add(Handle.GetValue());
	AdvanceRevision();
	IlluminationSourceRevisions.Add(Handle.GetValue(), Revision);
	PublishSnapshot();
	return true;
}

bool USightWeaveWorldSubsystem::UpdateIlluminationSourceTransform(
	const FSightWeaveIlluminationSourceHandle Handle,
	const FTransform& Transform)
{
	FSightWeaveIlluminationSourceDescription* Description = IlluminationSources.Find(Handle.GetValue());
	if (!bSightWeaveInitialized || !Description || Transform.ContainsNaN())
	{
		return false;
	}
	if (Description->Transform.Equals(Transform, 0.0))
	{
		return true;
	}
	Description->Transform = Transform;
	DirtyIlluminationSources.Add(Handle.GetValue());
	PendingIlluminationSnapshotRebuilds.Add(Handle.GetValue());
	AdvanceRevision();
	IlluminationSourceRevisions.FindChecked(Handle.GetValue()) = Revision;
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
	CachedIlluminationSolveSegments.Remove(Handle.GetValue());
	CachedIlluminationCandidateQueryKeys.Remove(Handle.GetValue());
	if (PreparedEventIndex.IsValid())
	{
		PreparedEventIndex->Release(CachedIlluminationPreparedSolves.FindRef(Handle.GetValue()));
	}
	CachedIlluminationPreparedSolves.Remove(Handle.GetValue());
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
	CachedVisionSolveSegments.Reset();
	CachedIlluminationSolveSegments.Reset();
	CachedVisionCandidateQueryKeys.Reset();
	CachedIlluminationCandidateQueryKeys.Reset();
	CachedVisionPreparedSolves.Reset();
	CachedIlluminationPreparedSolves.Reset();
	if (PreparedEventIndex.IsValid())
	{
		PreparedEventIndex->InvalidateAll();
	}
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
#if WITH_DEV_AUTOMATION_TESTS
	LastDynamicUpdateStageMetrics = {};
	const double PrepareStartSeconds = FPlatformTime::Seconds();
#endif
	const int64 NextSegmentIdBeforePrepare = NextSegmentId;
	TArray<FSightWeaveSegment2D>& Prepared = DynamicPreparedSegmentsScratch;
	if (!PrepareDynamicOccluderSegmentsInto(Handle, Segments, bDynamic, Prepared))
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
	bool bSegmentsUnchanged = Prepared.Num() == Record->Segments.Num();
	for (int32 SegmentIndex = 0; bSegmentsUnchanged && SegmentIndex < Prepared.Num(); ++SegmentIndex)
	{
		bSegmentsUnchanged = FSightWeaveSegment2D::StaticStruct()->CompareScriptStruct(
			&Prepared[SegmentIndex],
			&Record->Segments[SegmentIndex],
			0);
	}
	if (bSegmentsUnchanged && Record->bDynamic == bDynamic && Record->bEnabled == bEnabled)
	{
		NextSegmentId = NextSegmentIdBeforePrepare;
		return true;
	}
	const FSightWeaveFloorId OldFloor = Record->Segments.IsEmpty() ? FSightWeaveFloorId() : Record->Segments[0].FloorId;
	const FBox2D OldBounds = Record->Bounds;
	FBox2D NewBounds(ForceInit);
	for (const FSightWeaveSegment2D& Segment : Prepared)
	{
		NewBounds += Segment.A;
		NewBounds += Segment.B;
	}
#if WITH_DEV_AUTOMATION_TESTS
	const double SpatialStartSeconds = FPlatformTime::Seconds();
	LastDynamicUpdateStageMetrics.PrepareAndCompareMicroseconds =
		(SpatialStartSeconds - PrepareStartSeconds) * 1000000.0;
#endif

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
#if WITH_DEV_AUTOMATION_TESTS
	const double DirtyStartSeconds = FPlatformTime::Seconds();
	LastDynamicUpdateStageMetrics.SpatialIndexMicroseconds =
		(DirtyStartSeconds - SpatialStartSeconds) * 1000000.0;
#endif
	const double HeightOverlapEpsilon = GetDefault<USightWeaveSettings>()->GeometryTolerances.HeightOverlapEpsilon;
	auto PatchCachedSegments = [
		&Prepared,
		&OldSegments = Record->Segments,
		Handle,
		bEnabled,
		HeightOverlapEpsilon](auto& SegmentCaches, const auto& SourceDescriptions)
	{
		for (auto& CachePair : SegmentCaches)
		{
			const auto* Description = SourceDescriptions.Find(CachePair.Key);
			TArray<FSightWeaveSegment2D>& CachedSegments = CachePair.Value;
			if (!Description)
			{
				CachedSegments.Reset();
				continue;
			}
			const FVector SourceOrigin = Description->Transform.GetLocation();
			const FBox2D SourceBounds(
				FVector2D(SourceOrigin.X - Description->Range, SourceOrigin.Y - Description->Range),
				FVector2D(SourceOrigin.X + Description->Range, SourceOrigin.Y + Description->Range));
			auto IsRelevant = [Description, &SourceBounds, bEnabled, HeightOverlapEpsilon](
				const FSightWeaveSegment2D& Segment)
			{
				return bEnabled
					&& Segment.FloorId == Description->FloorId
					&& SightWeave::Geometry::HeightRangesOverlap(
						Segment.HeightRange,
						Description->HeightRange,
						HeightOverlapEpsilon)
					&& SourceBounds.Intersect(Segment.GetBounds());
			};
			auto LowerBoundStableId = [&CachedSegments](const int64 StableId)
			{
				int32 Lower = 0;
				int32 Upper = CachedSegments.Num();
				while (Lower < Upper)
				{
					const int32 Middle = Lower + (Upper - Lower) / 2;
					if (CachedSegments[Middle].StableId < StableId)
					{
						Lower = Middle + 1;
					}
					else
					{
						Upper = Middle;
					}
				}
				return Lower;
			};

			for (const FSightWeaveSegment2D& OldSegment : OldSegments)
			{
				const int32 CachedIndex = LowerBoundStableId(OldSegment.StableId);
				if (!CachedSegments.IsValidIndex(CachedIndex)
					|| CachedSegments[CachedIndex].StableId != OldSegment.StableId
					|| CachedSegments[CachedIndex].OccluderHandle != Handle)
				{
					continue;
				}
				const FSightWeaveSegment2D* Replacement = Prepared.FindByPredicate(
					[&OldSegment, &IsRelevant](const FSightWeaveSegment2D& Segment)
					{
						return Segment.StableId == OldSegment.StableId
							&& IsRelevant(Segment);
					});
				if (Replacement)
				{
					CachedSegments[CachedIndex] = *Replacement;
				}
				else
				{
					CachedSegments.RemoveAt(CachedIndex, 1, EAllowShrinking::No);
				}
			}
			for (const FSightWeaveSegment2D& Segment : Prepared)
			{
				if (!IsRelevant(Segment))
				{
					continue;
				}
				const int32 CachedIndex = LowerBoundStableId(Segment.StableId);
				if (!CachedSegments.IsValidIndex(CachedIndex)
					|| CachedSegments[CachedIndex].StableId != Segment.StableId)
				{
					CachedSegments.Insert(Segment, CachedIndex);
				}
			}
		}
	};
	PatchCachedSegments(CachedVisionSolveSegments, VisionSources);
	PatchCachedSegments(CachedIlluminationSolveSegments, IlluminationSources);

	Swap(Record->Segments, Prepared);
	Record->Bounds = NewBounds;
	Record->bDynamic = bDynamic;
	Record->bEnabled = bEnabled;
	AdvanceRevision();
	LastOccluderRevision = Revision;
	Record->GeometryRevision = Revision;
	MarkSourcesAffectedByOccluderChange(OldFloor, OldBounds, NewFloor, NewBounds);
#if WITH_DEV_AUTOMATION_TESTS
	const double PublicationStartSeconds = FPlatformTime::Seconds();
	LastDynamicUpdateStageMetrics.DirtyDiscoveryMicroseconds =
		(PublicationStartSeconds - DirtyStartSeconds) * 1000000.0;
#endif
	PublishSnapshot();
#if WITH_DEV_AUTOMATION_TESTS
	LastDynamicUpdateStageMetrics.PublicationMicroseconds =
		(FPlatformTime::Seconds() - PublicationStartSeconds) * 1000000.0;
#endif
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
	CachedVisionSolveSegments.Reset();
	CachedIlluminationSolveSegments.Reset();
	CachedVisionCandidateQueryKeys.Reset();
	CachedIlluminationCandidateQueryKeys.Reset();
	CachedVisionPreparedSolves.Reset();
	CachedIlluminationPreparedSolves.Reset();
	if (PreparedEventIndex.IsValid())
	{
		PreparedEventIndex->InvalidateAll();
	}
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

void USightWeaveWorldSubsystem::QueryEffectiveLiveAtLocationInto(
	const FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
	const FSightWeaveFloorId FloorId,
	const FVector WorldLocation,
	FSightWeaveVisibilityQueryResult& OutResult) const
{
	QueryEffectiveLiveInternalInto(
		KnowledgeOwnerId,
		FloorId,
		WorldLocation,
		nullptr,
		false,
		OutResult);
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
	FSightWeaveVisibilityQueryResult Result;
	QueryEffectiveLiveInternalInto(
		KnowledgeOwnerId,
		FloorId,
		WorldLocation,
		RestrictToSource,
		bPureVision,
		Result);
	return Result;
}

void USightWeaveWorldSubsystem::QueryEffectiveLiveInternalInto(
	const FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
	const FSightWeaveFloorId FloorId,
	const FVector WorldLocation,
	const FSightWeaveVisionSourceHandle* RestrictToSource,
	const bool bPureVision,
	FSightWeaveVisibilityQueryResult& Result) const
{
	if (!FloorId.IsValid())
	{
		InitializeQueryResult(Result, ESightWeaveQueryStatus::InvalidFloor, KnowledgeOwnerId, FloorId);
		return;
	}
	if (!KnowledgeOwnerId.IsValid() || WorldLocation.ContainsNaN())
	{
		InitializeQueryResult(Result, ESightWeaveQueryStatus::InvalidInput, KnowledgeOwnerId, FloorId);
		return;
	}
	const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> Snapshot = PublishedSnapshot;
	if (!bSightWeaveInitialized || !Snapshot.IsValid() || !Snapshot->bPublished)
	{
		InitializeQueryResult(Result, ESightWeaveQueryStatus::NotReady, KnowledgeOwnerId, FloorId);
		return;
	}
	const FSightWeaveFloorDefinition* Floor = Snapshot->Floors.FindByPredicate(
		[FloorId](const FSightWeaveFloorDefinition& Candidate) { return Candidate.FloorId == FloorId; });
	if (!Floor)
	{
		InitializeQueryResult(
			Result,
			Snapshot->Floors.IsEmpty() ? ESightWeaveQueryStatus::NotReady : ESightWeaveQueryStatus::InvalidFloor,
			KnowledgeOwnerId,
			FloorId);
		return;
	}

	const FSightWeaveGeometryTolerances& Tolerances = GetDefault<USightWeaveSettings>()->GeometryTolerances;
	QueryEffectiveLiveValidated(
		KnowledgeOwnerId,
		FloorId,
		WorldLocation,
		RestrictToSource,
		bPureVision,
		*Snapshot,
		*Floor,
		Tolerances,
		Result);
}

void USightWeaveWorldSubsystem::QueryEffectiveLiveValidated(
	const FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
	const FSightWeaveFloorId FloorId,
	const FVector WorldLocation,
	const FSightWeaveVisionSourceHandle* RestrictToSource,
	const bool bPureVision,
	const FSightWeaveFrameSnapshot& Snapshot,
	const FSightWeaveFloorDefinition& Floor,
	const FSightWeaveGeometryTolerances& Tolerances,
	FSightWeaveVisibilityQueryResult& Result,
	const FSightWeaveVisionSnapshotEntry* const* PrefilteredVisionEntries,
	const int32 PrefilteredVisionEntryCount,
	const uint64 PrefilteredIlluminationEligibilityMask,
	const bool bPrefilteredHeightMismatch,
	const bool bUsePrefilteredBatchState) const
{
	InitializeQueryResult(
		Result,
		ESightWeaveQueryStatus::AuthoritativeResult,
		KnowledgeOwnerId,
		FloorId,
		&Snapshot.Revision);
	Result.bAuthoritative = true;
	const FVector2D Point2D(WorldLocation.X, WorldLocation.Y);
	if (!bUsePrefilteredBatchState)
	{
		const bool bInsideFloorBounds = WorldLocation.X >= Floor.BoundsMin.X - Tolerances.PointOnEdgeEpsilon
			&& WorldLocation.X <= Floor.BoundsMax.X + Tolerances.PointOnEdgeEpsilon
			&& WorldLocation.Y >= Floor.BoundsMin.Y - Tolerances.PointOnEdgeEpsilon
			&& WorldLocation.Y <= Floor.BoundsMax.Y + Tolerances.PointOnEdgeEpsilon;
		if (!Floor.bEnabled || !Floor.bActiveForQueries || !bInsideFloorBounds)
		{
			Result.RejectionFlags |= static_cast<int32>(ESightWeaveQueryRejectionReason::FloorUnavailable);
			return;
		}
		if (!IsPointInHeightRange(WorldLocation.Z, Floor.HeightRange, Tolerances.HeightOverlapEpsilon))
		{
			Result.RejectionFlags |= static_cast<int32>(ESightWeaveQueryRejectionReason::HeightMismatch);
			return;
		}
	}

	bool bNominalVisionCoverage = false;
	bool bHeightMismatch = bUsePrefilteredBatchState && bPrefilteredHeightMismatch;
	bool bEffectiveBeforeSuppression = false;
	uint64 EvaluatedIlluminationMask = 0;
	uint64 LegalIlluminationMask = 0;
	uint64 ContributedIlluminationMask = 0;
	auto HasLegalIllumination = [&](const int32 IlluminationIndex)
	{
		if (!Snapshot.IlluminationSources.IsValidIndex(IlluminationIndex))
		{
			return false;
		}
		const uint64 Bit = IlluminationIndex < 64 ? (uint64{1} << IlluminationIndex) : 0;
		if (Bit != 0 && (EvaluatedIlluminationMask & Bit) != 0)
		{
			return (LegalIlluminationMask & Bit) != 0;
		}
		const FSightWeaveIlluminationSnapshotEntry& Illumination = Snapshot.IlluminationSources[IlluminationIndex];
		const bool bLegal = bUsePrefilteredBatchState
			? Bit != 0
				&& (PrefilteredIlluminationEligibilityMask & Bit) != 0
				&& IsPointInIlluminationSnapshotEntry(Point2D, Illumination, Tolerances)
			: Illumination.Description.bActive
				&& IsPointInHeightRange(
					WorldLocation.Z,
					Illumination.Description.HeightRange,
					Tolerances.HeightOverlapEpsilon)
				&& Illumination.Polygon.IsValid()
				&& IsPointInIlluminationSnapshotEntry(Point2D, Illumination, Tolerances);
		if (Bit != 0)
		{
			EvaluatedIlluminationMask |= Bit;
			if (bLegal) LegalIlluminationMask |= Bit;
		}
		return bLegal;
	};
	auto EvaluateVisionEntry = [&](const FSightWeaveVisionSnapshotEntry& Entry)
	{
		const bool bDirectionalNominalCoverage = Entry.Description.Shape != ESightWeaveSourceShape::Radial
			&& IsPointInNominalShape(
				WorldLocation,
				Entry.PolarOrigin,
				Entry.NominalForward,
				Entry.Description.Shape,
				Entry.Description.Range,
				Entry.NominalMinimumCosine,
				Entry.Description.NearAwarenessRadius,
				Tolerances.PointOnEdgeEpsilon);
		if (Entry.Description.Shape != ESightWeaveSourceShape::Radial)
		{
			bNominalVisionCoverage |= bDirectionalNominalCoverage;
			if (!bDirectionalNominalCoverage)
			{
				return;
			}
		}
		if (!Entry.Polygon.IsValid()
			|| !IsPointInVisionSnapshotEntry(Point2D, Entry, Tolerances))
		{
			if (Entry.Description.Shape == ESightWeaveSourceShape::Radial)
			{
				bNominalVisionCoverage |= IsPointInNominalShape(
					WorldLocation,
					Entry.PolarOrigin,
					Entry.NominalForward,
					Entry.Description.Shape,
					Entry.Description.Range,
					Entry.NominalMinimumCosine,
					Entry.Description.NearAwarenessRadius,
					Tolerances.PointOnEdgeEpsilon);
			}
			return;
		}

		bNominalVisionCoverage = true;
		Result.bInVisionPolygon = true;
		Result.ContributingVisionSources.Add(Entry.Handle);
		if (bPureVision)
		{
			bEffectiveBeforeSuppression = true;
			return;
		}
		if (Entry.Description.IlluminationPolicy == ESightWeaveIlluminationPolicy::BypassLegalIllumination)
		{
			Result.bUsedBypass = true;
			bEffectiveBeforeSuppression = true;
			return;
		}

		bool bThisSourceHasIllumination = false;
		for (const int32 IlluminationIndex : Entry.CompatibleIlluminationSourceIndices)
		{
			if (!HasLegalIllumination(IlluminationIndex))
			{
				continue;
			}
			bThisSourceHasIllumination = true;
			Result.bHasLegalIllumination = true;
			const uint64 IlluminationBit = IlluminationIndex < 64 ? (uint64{1} << IlluminationIndex) : 0;
			if (IlluminationBit == 0)
			{
				AddUniqueHandle(
					Result.ContributingIlluminationSources,
					Snapshot.IlluminationSources[IlluminationIndex].Handle);
			}
			else if ((ContributedIlluminationMask & IlluminationBit) == 0)
			{
				ContributedIlluminationMask |= IlluminationBit;
				Result.ContributingIlluminationSources.Add(
					Snapshot.IlluminationSources[IlluminationIndex].Handle);
			}
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
	};
	if (bUsePrefilteredBatchState)
	{
		for (int32 EntryIndex = 0; EntryIndex < PrefilteredVisionEntryCount; ++EntryIndex)
		{
			EvaluateVisionEntry(*PrefilteredVisionEntries[EntryIndex]);
		}
	}
	else
	{
		for (const FSightWeaveVisionSnapshotEntry& Entry : Snapshot.VisionSources)
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
			EvaluateVisionEntry(Entry);
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

	if (!bPureVision && bEffectiveBeforeSuppression && !Snapshot.HardSuppressions.IsEmpty())
	{
		Result.bRejectedBySuppression = IsPointSuppressed(Snapshot, FloorId, WorldLocation, Result.ContributingSuppressions);
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
	return;
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
			Entry.PolarOrigin,
			Entry.NominalForward,
			Entry.Description.Shape,
			Entry.Description.Range,
			Entry.NominalMinimumCosine,
			0.0,
			Tolerances.PointOnEdgeEpsilon);
		if (Entry.Polygon.IsValid()
			&& IsPointInIlluminationSnapshotEntry(Point2D, Entry, Tolerances))
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
	OutResults.SetNum(Requests.Num(), EAllowShrinking::No);
	const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> Snapshot = PublishedSnapshot;
	const FSightWeaveGeometryTolerances& Tolerances = GetDefault<USightWeaveSettings>()->GeometryTolerances;
	if (Requests.Num() >= 256
		&& Requests.Num() <= 512
		&& bSightWeaveInitialized
		&& Snapshot.IsValid()
		&& Snapshot->bPublished
		&& Snapshot->HardSuppressions.IsEmpty()
		&& Snapshot->VisionSources.Num() <= 16
		&& Snapshot->IlluminationSources.Num() <= 64)
	{
		const FSightWeaveFloorId SharedFloorId = Requests[0].FloorId;
		const FSightWeaveKnowledgeOwnerId SharedKnowledgeOwnerId = Requests[0].KnowledgeOwnerId;
		const bool bFirstAnchorValid = Requests[0].SampleSet.Rule == ESightWeaveSampleRule::Anchor
			&& Requests[0].SampleSet.Samples.IsValidIndex(Requests[0].SampleSet.AnchorIndex);
		const double SharedZ = bFirstAnchorValid
			? Requests[0].SampleSet.Samples[Requests[0].SampleSet.AnchorIndex].Z
			: 0.0;
		const FSightWeaveFloorDefinition* SharedFloor = SharedFloorId.IsValid()
			? Snapshot->Floors.FindByPredicate(
				[SharedFloorId](const FSightWeaveFloorDefinition& Candidate)
				{
					return Candidate.FloorId == SharedFloorId;
				})
			: nullptr;
		bool bUniformValidatedBatch = SharedFloor
			&& SharedKnowledgeOwnerId.IsValid()
			&& bFirstAnchorValid
			&& FMath::IsFinite(SharedZ)
			&& SharedFloor->bEnabled
			&& SharedFloor->bActiveForQueries
			&& IsPointInHeightRange(SharedZ, SharedFloor->HeightRange, Tolerances.HeightOverlapEpsilon);
		for (const FSightWeaveQueryRequest& Request : Requests)
		{
			if (!bUniformValidatedBatch)
			{
				break;
			}
			const bool bValidAnchor = Request.SampleSet.Rule == ESightWeaveSampleRule::Anchor
				&& Request.SampleSet.Samples.IsValidIndex(Request.SampleSet.AnchorIndex)
				&& !Request.SampleSet.Samples[Request.SampleSet.AnchorIndex].ContainsNaN();
			if (!bValidAnchor
				|| Request.KnowledgeOwnerId != SharedKnowledgeOwnerId
				|| Request.FloorId != SharedFloorId
				|| Request.SampleSet.Samples[Request.SampleSet.AnchorIndex].Z != SharedZ)
			{
				bUniformValidatedBatch = false;
				break;
			}
			const FVector& Location = Request.SampleSet.Samples[Request.SampleSet.AnchorIndex];
			if (Location.X < SharedFloor->BoundsMin.X - Tolerances.PointOnEdgeEpsilon
				|| Location.X > SharedFloor->BoundsMax.X + Tolerances.PointOnEdgeEpsilon
				|| Location.Y < SharedFloor->BoundsMin.Y - Tolerances.PointOnEdgeEpsilon
				|| Location.Y > SharedFloor->BoundsMax.Y + Tolerances.PointOnEdgeEpsilon)
			{
				bUniformValidatedBatch = false;
				break;
			}
		}
		if (bUniformValidatedBatch)
		{
			const FSightWeaveVisionSnapshotEntry* PrefilteredVisionEntries[16];
			int32 PrefilteredVisionEntryCount = 0;
			bool bPrefilteredHeightMismatch = false;
			for (const FSightWeaveVisionSnapshotEntry& Entry : Snapshot->VisionSources)
			{
				if (Entry.Description.KnowledgeOwnerId != SharedKnowledgeOwnerId
					|| Entry.Description.FloorId != SharedFloorId
					|| !Entry.Description.bActive)
				{
					continue;
				}
				if (!IsPointInHeightRange(SharedZ, Entry.Description.HeightRange, Tolerances.HeightOverlapEpsilon))
				{
					bPrefilteredHeightMismatch = true;
					continue;
				}
				PrefilteredVisionEntries[PrefilteredVisionEntryCount++] = &Entry;
			}
			uint64 PrefilteredIlluminationEligibilityMask = 0;
			for (int32 IlluminationIndex = 0;
				IlluminationIndex < Snapshot->IlluminationSources.Num();
				++IlluminationIndex)
			{
				const FSightWeaveIlluminationSnapshotEntry& Illumination =
					Snapshot->IlluminationSources[IlluminationIndex];
				if (Illumination.Description.bActive
					&& IsPointInHeightRange(
						SharedZ,
						Illumination.Description.HeightRange,
						Tolerances.HeightOverlapEpsilon)
					&& Illumination.Polygon.IsValid())
				{
					PrefilteredIlluminationEligibilityMask |= uint64{1} << IlluminationIndex;
				}
			}
			for (int32 RequestIndex = 0; RequestIndex < Requests.Num(); ++RequestIndex)
			{
				const FSightWeaveQueryRequest& Request = Requests[RequestIndex];
				QueryEffectiveLiveValidated(
					Request.KnowledgeOwnerId,
					Request.FloorId,
					Request.SampleSet.Samples[Request.SampleSet.AnchorIndex],
					nullptr,
					false,
					*Snapshot,
					*SharedFloor,
					Tolerances,
					OutResults[RequestIndex],
					PrefilteredVisionEntries,
					PrefilteredVisionEntryCount,
					PrefilteredIlluminationEligibilityMask,
					bPrefilteredHeightMismatch,
					true);
			}
			return;
		}
	}
	FSightWeaveFloorId CachedFloorId;
	const FSightWeaveFloorDefinition* CachedFloor = nullptr;
	for (int32 RequestIndex = 0; RequestIndex < Requests.Num(); ++RequestIndex)
	{
		const FSightWeaveQueryRequest& Request = Requests[RequestIndex];
		const bool bValidAnchor = Request.SampleSet.Rule == ESightWeaveSampleRule::Anchor
			&& Request.SampleSet.Samples.IsValidIndex(Request.SampleSet.AnchorIndex)
			&& !Request.SampleSet.Samples[Request.SampleSet.AnchorIndex].ContainsNaN();
		const bool bCanUseValidatedPath = bSightWeaveInitialized
			&& Snapshot.IsValid()
			&& Snapshot->bPublished
			&& Request.KnowledgeOwnerId.IsValid()
			&& Request.FloorId.IsValid()
			&& bValidAnchor;
		if (bCanUseValidatedPath)
		{
			if (Request.FloorId != CachedFloorId)
			{
				CachedFloorId = Request.FloorId;
				CachedFloor = Snapshot->Floors.FindByPredicate(
					[&Request](const FSightWeaveFloorDefinition& Candidate)
					{
						return Candidate.FloorId == Request.FloorId;
					});
			}
			if (CachedFloor)
			{
				QueryEffectiveLiveValidated(
					Request.KnowledgeOwnerId,
					Request.FloorId,
					Request.SampleSet.Samples[Request.SampleSet.AnchorIndex],
					nullptr,
					false,
					*Snapshot,
					*CachedFloor,
					Tolerances,
					OutResults[RequestIndex]);
				continue;
			}
		}

		if (bValidAnchor)
		{
			QueryEffectiveLiveInternalInto(
				Request.KnowledgeOwnerId,
				Request.FloorId,
				Request.SampleSet.Samples[Request.SampleSet.AnchorIndex],
				nullptr,
				false,
				OutResults[RequestIndex]);
		}
		else
		{
			OutResults[RequestIndex] = QuerySamples(
				Request.KnowledgeOwnerId,
				Request.FloorId,
				Request.SampleSet);
		}
	}
}

FSightWeaveRevision USightWeaveWorldSubsystem::PublishSnapshot()
{
	if (!bSightWeaveInitialized)
	{
		return FSightWeaveRevision();
	}
	if (PublishedSnapshot.IsValid()
		&& PublishedSnapshot->Revision == Revision
		&& PendingVisionSnapshotRebuilds.IsEmpty()
		&& PendingIlluminationSnapshotRebuilds.IsEmpty())
	{
		return Revision;
	}

	TArray<int64>& DirtyVision = PublicationDirtyVisionIds;
	TArray<int64>& DirtyIllumination = PublicationDirtyIlluminationIds;
	DirtyVision.Reset();
	DirtyVision.Reserve(PendingVisionSnapshotRebuilds.Num());
	for (const int64 SourceId : PendingVisionSnapshotRebuilds)
	{
		DirtyVision.Add(SourceId);
	}
	DirtyIllumination.Reset();
	DirtyIllumination.Reserve(PendingIlluminationSnapshotRebuilds.Num());
	for (const int64 SourceId : PendingIlluminationSnapshotRebuilds)
	{
		DirtyIllumination.Add(SourceId);
	}
	DirtyVision.Sort();
	DirtyIllumination.Sort();
#if WITH_DEV_AUTOMATION_TESTS
	const double VisionRebuildStartSeconds = FPlatformTime::Seconds();
#endif
	for (const int64 SourceId : DirtyVision)
	{
		RebuildVisionSnapshotEntry(SourceId);
	}
#if WITH_DEV_AUTOMATION_TESTS
	const double IlluminationRebuildStartSeconds = FPlatformTime::Seconds();
	LastDynamicUpdateStageMetrics.VisionRebuildMicroseconds =
		(IlluminationRebuildStartSeconds - VisionRebuildStartSeconds) * 1000000.0;
#endif
	for (const int64 SourceId : DirtyIllumination)
	{
		RebuildIlluminationSnapshotEntry(SourceId);
	}
#if WITH_DEV_AUTOMATION_TESTS
	const double SnapshotMaterializationStartSeconds = FPlatformTime::Seconds();
	LastDynamicUpdateStageMetrics.IlluminationRebuildMicroseconds =
		(SnapshotMaterializationStartSeconds - IlluminationRebuildStartSeconds) * 1000000.0;
#endif
	PendingVisionSnapshotRebuilds.Reset();
	PendingIlluminationSnapshotRebuilds.Reset();

	TSharedPtr<FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> NewSharedSnapshot;
	if (StandbySnapshot.IsValid() && StandbySnapshot.GetSharedReferenceCount() == 1)
	{
		NewSharedSnapshot = MoveTemp(StandbySnapshot);
	}
	else
	{
		StandbySnapshot.Reset();
		NewSharedSnapshot = MakeShared<FSightWeaveFrameSnapshot, ESPMode::ThreadSafe>();
	}
	FSightWeaveFrameSnapshot& NewSnapshot = *NewSharedSnapshot;
	NewSnapshot.Revision = Revision;
	NewSnapshot.RebuiltVisionPolygonCount = DirtyVision.Num();
	NewSnapshot.RebuiltIlluminationPolygonCount = DirtyIllumination.Num();
	NewSnapshot.bPublished = true;

	Floors.GenerateValueArray(NewSnapshot.Floors);
	NewSnapshot.Floors.Sort([](const FSightWeaveFloorDefinition& A, const FSightWeaveFloorDefinition& B)
	{
		return A.FloorId.GetValue().LexicalLess(B.FloorId.GetValue());
	});

	TArray<int64>& VisionIds = PublicationVisionIds;
	CachedVisionSnapshotEntries.GenerateKeyArray(VisionIds);
	VisionIds.Sort();
	NewSnapshot.VisionSources.SetNum(VisionIds.Num(), EAllowShrinking::No);
	for (int32 SourceIndex = 0; SourceIndex < VisionIds.Num(); ++SourceIndex)
	{
		NewSnapshot.VisionSources[SourceIndex] =
			CachedVisionSnapshotEntries.FindChecked(VisionIds[SourceIndex]);
	}

	TArray<int64>& IlluminationIds = PublicationIlluminationIds;
	CachedIlluminationSnapshotEntries.GenerateKeyArray(IlluminationIds);
	IlluminationIds.Sort();
	NewSnapshot.IlluminationSources.SetNum(IlluminationIds.Num(), EAllowShrinking::No);
	for (int32 SourceIndex = 0; SourceIndex < IlluminationIds.Num(); ++SourceIndex)
	{
		NewSnapshot.IlluminationSources[SourceIndex] =
			CachedIlluminationSnapshotEntries.FindChecked(IlluminationIds[SourceIndex]);
	}

	int32 EnabledSegmentCount = 0;
	for (const TPair<int64, FOccluderRecord>& Pair : Occluders)
	{
		if (Pair.Value.bEnabled)
		{
			EnabledSegmentCount += Pair.Value.Segments.Num();
		}
	}
	NewSnapshot.OccluderSegments.SetNum(EnabledSegmentCount, EAllowShrinking::No);
	int32 SegmentWriteIndex = 0;
	for (const TPair<int64, FOccluderRecord>& Pair : Occluders)
	{
		if (Pair.Value.bEnabled)
		{
			for (const FSightWeaveSegment2D& Segment : Pair.Value.Segments)
			{
				NewSnapshot.OccluderSegments[SegmentWriteIndex++] = Segment;
			}
		}
	}
	NewSnapshot.OccluderSegments.Sort([](const FSightWeaveSegment2D& A, const FSightWeaveSegment2D& B)
	{
		return A.StableId < B.StableId;
	});

	TArray<int64>& SuppressionIds = PublicationSuppressionIds;
	HardSuppressions.GenerateKeyArray(SuppressionIds);
	SuppressionIds.Sort();
	NewSnapshot.HardSuppressions.SetNum(SuppressionIds.Num(), EAllowShrinking::No);
	for (int32 SuppressionIndex = 0; SuppressionIndex < SuppressionIds.Num(); ++SuppressionIndex)
	{
		const int64 SuppressionId = SuppressionIds[SuppressionIndex];
		FSightWeaveHardSuppressionSnapshotEntry& Entry = NewSnapshot.HardSuppressions[SuppressionIndex];
		Entry.Handle = FSightWeaveHardSuppressionHandle(SuppressionId);
		Entry.Description = HardSuppressions.FindChecked(SuppressionId);
		Entry.Revision = HardSuppressionRevisions.FindRef(SuppressionId);
	}

	ResolveSnapshotCompatibility(NewSnapshot);
	TSharedPtr<FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> PreviousSnapshot =
		MoveTemp(PublishedSnapshot);
	PublishedSnapshot = MoveTemp(NewSharedSnapshot);
	StandbySnapshot = MoveTemp(PreviousSnapshot);
#if WITH_DEV_AUTOMATION_TESTS
	LastDynamicUpdateStageMetrics.SnapshotMaterializationMicroseconds =
		(FPlatformTime::Seconds() - SnapshotMaterializationStartSeconds) * 1000000.0;
#endif
	return Revision;
}

FSightWeaveFrameSnapshot USightWeaveWorldSubsystem::GetPublishedSnapshot() const
{
	const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> Snapshot = PublishedSnapshot;
	return Snapshot.IsValid() ? *Snapshot : FSightWeaveFrameSnapshot();
}

FSightWeavePreparedEventIndexStats USightWeaveWorldSubsystem::GetPreparedEventIndexStats() const
{
	return PreparedEventIndex.IsValid()
		? PreparedEventIndex->GetStats()
		: FSightWeavePreparedEventIndexStats();
}

#if WITH_DEV_AUTOMATION_TESTS
bool USightWeaveWorldSubsystem::ConfigurePreparedEventIndexForTesting(
	const int32 MaximumEntries,
	const int64 MaximumBytes)
{
	if (!PreparedEventIndex.IsValid()
		|| !VisionSources.IsEmpty()
		|| !IlluminationSources.IsEmpty())
	{
		return false;
	}
	PreparedEventIndex->Initialize(MaximumEntries, MaximumBytes);
	return true;
}

bool USightWeaveWorldSubsystem::ExercisePreparedEventIndexConcurrentIsolationForTesting(
	const FSightWeaveReferenceSolveInput& Input,
	const int32 WorkerCount,
	const int32 RepeatsPerWorker)
{
	if (WorkerCount <= 0 || WorkerCount > 64
		|| RepeatsPerWorker <= 0 || RepeatsPerWorker > 128)
	{
		return false;
	}

	FSightWeaveReferenceSolveResult Expected;
	SightWeave::Geometry::SolveOptimizedPolygonInto(Input, Expected);
	if (!Expected.bSucceeded)
	{
		return false;
	}
	auto ResultsMatch = [&Expected](const FSightWeaveReferenceSolveResult& Result)
	{
		return Result.bSucceeded == Expected.bSucceeded
			&& Result.CandidateSegmentCount == Expected.CandidateSegmentCount
			&& Result.CastRayCount == Expected.CastRayCount
			&& Result.Vertices == Expected.Vertices
			&& Result.CandidateAnglesRadians == Expected.CandidateAnglesRadians
			&& Result.CandidateDistances == Expected.CandidateDistances
			&& Result.CandidateBoundaryPoints == Expected.CandidateBoundaryPoints;
	};

	TArray<int32> FailureCounts;
	FailureCounts.Init(0, WorkerCount);
	ParallelFor(WorkerCount, [&](const int32 WorkerIndex)
	{
		FSightWeavePreparedEventIndex Index;
		Index.Initialize(2, 4ll * 1024ll * 1024ll);
		TSharedPtr<FSightWeaveOptimizedSolveCache> Binding;
		FSightWeaveReferenceSolveResult Result;
		for (int32 Repeat = 0; Repeat < RepeatsPerWorker; ++Repeat)
		{
			const FSightWeavePreparedEventIndex::FAcquireResult Acquisition =
				Index.Acquire(Input, Binding, static_cast<uint64>(Repeat + 1));
			Binding = Acquisition.Cache;
			if (!Binding.IsValid())
			{
				++FailureCounts[WorkerIndex];
				continue;
			}
			SightWeave::Geometry::SolveOptimizedPolygonIntoValidatedCache(Input, Result, *Binding);
			if (!Index.Commit(Binding) || !ResultsMatch(Result))
			{
				++FailureCounts[WorkerIndex];
			}
		}
		const FSightWeavePreparedEventIndexStats Stats = Index.GetStats();
		if (Stats.MissCount != 1
			|| Stats.HitCount != RepeatsPerWorker - 1
			|| Stats.LiveEntryCount != 1
			|| Stats.SourceBindingCount != 1
			|| Stats.LiveAllocatedBytes <= 0
			|| Stats.LiveAllocatedBytes > 4ll * 1024ll * 1024ll)
		{
			++FailureCounts[WorkerIndex];
		}
	});
	return !FailureCounts.ContainsByPredicate([](const int32 Count) { return Count != 0; });
}
#endif

void USightWeaveWorldSubsystem::RebuildVisionSnapshotEntry(const int64 SourceId)
{
	const FSightWeaveVisionSourceDescription* Description = VisionSources.Find(SourceId);
	if (!Description)
	{
		CachedVisionSnapshotEntries.Remove(SourceId);
		return;
	}

	FSightWeaveVisionSnapshotEntry& Entry = CachedVisionSnapshotEntries.FindOrAdd(SourceId);
	FSightWeaveReferenceSolveResult SolveResult;
	SolveResult.Vertices = MoveTemp(Entry.Polygon.Vertices);
	SolveResult.CandidateAnglesRadians = MoveTemp(Entry.CandidateAnglesRadians);
	SolveResult.CandidateDistances = MoveTemp(Entry.CandidateDistances);
	SolveResult.CandidateBoundaryPoints = MoveTemp(Entry.CandidateBoundaryPoints);
	Entry.CompatibleIlluminationSources.Reset();
	Entry.CompatibleIlluminationSourceIndices.Reset();
	Entry.PolarAngleUpperBoundLut.Reset();
	Entry.CandidateSegmentCount = 0;
	Entry.CandidateRayCount = 0;
	Entry.SolveTimeMicroseconds = 0.0;
	Entry.PolarOrigin = FVector2D::ZeroVector;
	Entry.PolarForwardAngleRadians = 0.0;
	Entry.NominalForward = FVector2D(1.0, 0.0);
	Entry.NominalMinimumCosine = -1.0;
	Entry.bPolarBoundaryFullCircle = false;
	Entry.Handle = FSightWeaveVisionSourceHandle(SourceId);
	Entry.Description = *Description;
	Entry.SourceRevision = VisionSourceRevisions.FindRef(SourceId);
	Entry.Polygon.SourceHandle = Entry.Handle;
	Entry.Polygon.KnowledgeOwnerId = Description->KnowledgeOwnerId;
	Entry.Polygon.FloorId = Description->FloorId;
	Entry.Polygon.Revision = Revision;
	Entry.Polygon.SourceRevision = Entry.SourceRevision;
	Entry.Polygon.OccluderRevision = LastOccluderRevision;
	Entry.Polygon.BoundsMin = FVector2D::ZeroVector;
	Entry.Polygon.BoundsMax = FVector2D::ZeroVector;

	const FSightWeaveFloorDefinition* Floor = Floors.Find(Description->FloorId);
	if (Description->bActive && Floor && Floor->bEnabled && Floor->bActiveForQueries)
	{
		FSightWeaveReferenceSolveInput Input;
		Input.Origin = Description->Transform.GetLocation();
		const FVector Forward3 = Description->Transform.GetUnitAxis(EAxis::X);
		Input.Forward = FVector2D(Forward3.X, Forward3.Y);
		if (!Input.Forward.Normalize()) Input.Forward = FVector2D(1.0, 0.0);
		Entry.PolarOrigin = FVector2D(Input.Origin.X, Input.Origin.Y);
		Entry.PolarForwardAngleRadians = FMath::Atan2(Input.Forward.Y, Input.Forward.X);
		Entry.NominalForward = Input.Forward;
		Input.Shape = Description->Shape;
		Input.Range = Description->Range;
		Input.HalfAngleDegrees = Description->HalfAngleDegrees;
		Input.NearAwarenessRadius = Description->NearAwarenessRadius;
		Entry.bPolarBoundaryFullCircle =
			Input.Shape == ESightWeaveSourceShape::Radial || Input.NearAwarenessRadius > 0.0;
		Input.FloorId = Description->FloorId;
		Input.HeightRange = Description->HeightRange;
		const USightWeaveSettings* Settings = GetDefault<USightWeaveSettings>();
		Input.Tolerances = Settings->GeometryTolerances;
		Input.Tolerances.Normalize();
		Entry.NominalMinimumCosine = FMath::Cos(FMath::DegreesToRadians(
			Description->HalfAngleDegrees + Input.Tolerances.PointOnEdgeEpsilon));
		const FVector Origin = Description->Transform.GetLocation();
		const FBox2D QueryBounds(
			FVector2D(Origin.X - Description->Range, Origin.Y - Description->Range),
			FVector2D(Origin.X + Description->Range, Origin.Y + Description->Range));
		TArray<FSightWeaveSegment2D>* CachedSegments = CachedVisionSolveSegments.Find(SourceId);
		if (!CachedSegments)
		{
			CachedSegments = &CachedVisionSolveSegments.Add(SourceId);
		}
		FSourceCandidateQueryKey* CachedQueryKey = CachedVisionCandidateQueryKeys.Find(SourceId);
		if (!CachedQueryKey
			|| !CachedQueryKey->Matches(Description->FloorId, Description->HeightRange, QueryBounds))
		{
			QueryOccluderSegments(
				Description->FloorId,
				QueryBounds,
				Description->HeightRange,
				*CachedSegments);
			FSourceCandidateQueryKey& NewQueryKey = CachedVisionCandidateQueryKeys.FindOrAdd(SourceId);
			NewQueryKey.FloorId = Description->FloorId;
			NewQueryKey.HeightRange = Description->HeightRange;
			NewQueryKey.Bounds = QueryBounds;
		}
		Input.Segments = MoveTemp(*CachedSegments);

		const double StartSeconds = FPlatformTime::Seconds();
		TSharedPtr<FSightWeaveOptimizedSolveCache>& PreparedCache =
			CachedVisionPreparedSolves.FindOrAdd(SourceId);
#if UE_BUILD_SHIPPING
		const bool bUsePreparedIndex = true;
#else
		const bool bUsePreparedIndex = Settings->SolverMode == ESightWeaveSolverMode::Optimized;
#endif
		if (bUsePreparedIndex && PreparedEventIndex.IsValid())
		{
			const FSightWeavePreparedEventIndex::FAcquireResult Acquisition =
				PreparedEventIndex->Acquire(Input, PreparedCache, Revision.GetValue());
			PreparedCache = Acquisition.Cache;
			if (PreparedCache.IsValid())
			{
				SightWeave::Geometry::SolveOptimizedPolygonIntoValidatedCache(Input, SolveResult, *PreparedCache);
				if (!PreparedEventIndex->Commit(PreparedCache))
				{
					PreparedCache.Reset();
				}
			}
			else
			{
				SightWeave::Geometry::SolveOptimizedPolygonInto(Input, SolveResult);
			}
		}
		else
		{
			if (PreparedEventIndex.IsValid())
			{
				PreparedEventIndex->Release(PreparedCache);
			}
			PreparedCache.Reset();
#if !UE_BUILD_SHIPPING
			SightWeave::Geometry::SolvePolygonInto(Input, Settings->SolverMode, SolveResult);
#endif
		}
		*CachedSegments = MoveTemp(Input.Segments);
		Entry.SolveTimeMicroseconds = (FPlatformTime::Seconds() - StartSeconds) * 1000000.0;
		Entry.CandidateSegmentCount = SolveResult.CandidateSegmentCount;
		Entry.CandidateRayCount = SolveResult.CastRayCount;
		Entry.CandidateAnglesRadians = MoveTemp(SolveResult.CandidateAnglesRadians);
		Entry.CandidateDistances = MoveTemp(SolveResult.CandidateDistances);
		Entry.CandidateBoundaryPoints = MoveTemp(SolveResult.CandidateBoundaryPoints);
		BuildPolarAngleUpperBoundLut(Entry.CandidateAnglesRadians, Entry.PolarAngleUpperBoundLut);
		if (SolveResult.bSucceeded)
		{
			Entry.Polygon.Vertices = MoveTemp(SolveResult.Vertices);
			SetPolygonBounds(Entry.Polygon.Vertices, Entry.Polygon.BoundsMin, Entry.Polygon.BoundsMax);
		}
	}
}

void USightWeaveWorldSubsystem::RebuildIlluminationSnapshotEntry(const int64 SourceId)
{
	const FSightWeaveIlluminationSourceDescription* Description = IlluminationSources.Find(SourceId);
	if (!Description)
	{
		CachedIlluminationSnapshotEntries.Remove(SourceId);
		return;
	}

	FSightWeaveIlluminationSnapshotEntry& Entry = CachedIlluminationSnapshotEntries.FindOrAdd(SourceId);
	FSightWeaveReferenceSolveResult SolveResult;
	SolveResult.Vertices = MoveTemp(Entry.Polygon.Vertices);
	SolveResult.CandidateAnglesRadians = MoveTemp(Entry.CandidateAnglesRadians);
	SolveResult.CandidateDistances = MoveTemp(Entry.CandidateDistances);
	SolveResult.CandidateBoundaryPoints = MoveTemp(Entry.CandidateBoundaryPoints);
	Entry.PolarAngleUpperBoundLut.Reset();
	Entry.CandidateSegmentCount = 0;
	Entry.CandidateRayCount = 0;
	Entry.SolveTimeMicroseconds = 0.0;
	Entry.PolarOrigin = FVector2D::ZeroVector;
	Entry.PolarForwardAngleRadians = 0.0;
	Entry.NominalForward = FVector2D(1.0, 0.0);
	Entry.NominalMinimumCosine = -1.0;
	Entry.bPolarBoundaryFullCircle = false;
	Entry.Handle = FSightWeaveIlluminationSourceHandle(SourceId);
	Entry.Description = *Description;
	Entry.SourceRevision = IlluminationSourceRevisions.FindRef(SourceId);
	Entry.Polygon.SourceHandle = Entry.Handle;
	Entry.Polygon.KnowledgeOwnerId = Description->KnowledgeOwnerId;
	Entry.Polygon.FloorId = Description->FloorId;
	Entry.Polygon.Revision = Revision;
	Entry.Polygon.SourceRevision = Entry.SourceRevision;
	Entry.Polygon.OccluderRevision = LastOccluderRevision;
	Entry.Polygon.BoundsMin = FVector2D::ZeroVector;
	Entry.Polygon.BoundsMax = FVector2D::ZeroVector;

	const FSightWeaveFloorDefinition* Floor = Floors.Find(Description->FloorId);
	if (Description->bActive && Floor && Floor->bEnabled && Floor->bActiveForQueries)
	{
		FSightWeaveReferenceSolveInput Input;
		Input.Origin = Description->Transform.GetLocation();
		const FVector Forward3 = Description->Transform.GetUnitAxis(EAxis::X);
		Input.Forward = FVector2D(Forward3.X, Forward3.Y);
		if (!Input.Forward.Normalize()) Input.Forward = FVector2D(1.0, 0.0);
		Entry.PolarOrigin = FVector2D(Input.Origin.X, Input.Origin.Y);
		Entry.PolarForwardAngleRadians = FMath::Atan2(Input.Forward.Y, Input.Forward.X);
		Entry.NominalForward = Input.Forward;
		Input.Shape = Description->Shape;
		Input.Range = Description->Range;
		Input.HalfAngleDegrees = Description->HalfAngleDegrees;
		Input.NearAwarenessRadius = 0.0;
		Entry.bPolarBoundaryFullCircle = Input.Shape == ESightWeaveSourceShape::Radial;
		Input.FloorId = Description->FloorId;
		Input.HeightRange = Description->HeightRange;
		const USightWeaveSettings* Settings = GetDefault<USightWeaveSettings>();
		Input.Tolerances = Settings->GeometryTolerances;
		Input.Tolerances.Normalize();
		Entry.NominalMinimumCosine = FMath::Cos(FMath::DegreesToRadians(
			Description->HalfAngleDegrees + Input.Tolerances.PointOnEdgeEpsilon));
		const FVector Origin = Description->Transform.GetLocation();
		const FBox2D QueryBounds(
			FVector2D(Origin.X - Description->Range, Origin.Y - Description->Range),
			FVector2D(Origin.X + Description->Range, Origin.Y + Description->Range));
		TArray<FSightWeaveSegment2D>* CachedSegments = CachedIlluminationSolveSegments.Find(SourceId);
		if (!CachedSegments)
		{
			CachedSegments = &CachedIlluminationSolveSegments.Add(SourceId);
		}
		FSourceCandidateQueryKey* CachedQueryKey = CachedIlluminationCandidateQueryKeys.Find(SourceId);
		if (!CachedQueryKey
			|| !CachedQueryKey->Matches(Description->FloorId, Description->HeightRange, QueryBounds))
		{
			QueryOccluderSegments(
				Description->FloorId,
				QueryBounds,
				Description->HeightRange,
				*CachedSegments);
			FSourceCandidateQueryKey& NewQueryKey = CachedIlluminationCandidateQueryKeys.FindOrAdd(SourceId);
			NewQueryKey.FloorId = Description->FloorId;
			NewQueryKey.HeightRange = Description->HeightRange;
			NewQueryKey.Bounds = QueryBounds;
		}
		Input.Segments = MoveTemp(*CachedSegments);

		const double StartSeconds = FPlatformTime::Seconds();
		const FSightWeaveVisionSnapshotEntry* SharedGeometry = nullptr;
		int64 SharedGeometrySourceId = MAX_int64;
		for (const TPair<int64, FSightWeaveVisionSnapshotEntry>& Pair : CachedVisionSnapshotEntries)
		{
			if (Pair.Key < SharedGeometrySourceId
				&& HasIdenticalVisionAndIlluminationGeometry(Pair.Value, *Description))
			{
				SharedGeometry = &Pair.Value;
				SharedGeometrySourceId = Pair.Key;
			}
		}
		TSharedPtr<FSightWeaveOptimizedSolveCache>& PreparedCache =
			CachedIlluminationPreparedSolves.FindOrAdd(SourceId);
#if UE_BUILD_SHIPPING
		const bool bUsePreparedIndex = true;
#else
		const bool bUsePreparedIndex = Settings->SolverMode == ESightWeaveSolverMode::Optimized;
#endif
		if (bUsePreparedIndex && PreparedEventIndex.IsValid())
		{
			const FSightWeavePreparedEventIndex::FAcquireResult Acquisition =
				PreparedEventIndex->Acquire(Input, PreparedCache, Revision.GetValue());
			PreparedCache = Acquisition.Cache;
			if (SharedGeometry)
			{
				SolveResult.bSucceeded = true;
				SolveResult.CandidateSegmentCount = SharedGeometry->CandidateSegmentCount;
				SolveResult.CastRayCount = SharedGeometry->CandidateRayCount;
				SolveResult.Vertices = SharedGeometry->Polygon.Vertices;
				SolveResult.CandidateAnglesRadians = SharedGeometry->CandidateAnglesRadians;
				SolveResult.CandidateDistances = SharedGeometry->CandidateDistances;
				SolveResult.CandidateBoundaryPoints = SharedGeometry->CandidateBoundaryPoints;
				if (PreparedCache.IsValid() && !PreparedEventIndex->Commit(PreparedCache))
				{
					PreparedCache.Reset();
				}
			}
			else if (PreparedCache.IsValid())
			{
				SightWeave::Geometry::SolveOptimizedPolygonIntoValidatedCache(Input, SolveResult, *PreparedCache);
				if (!PreparedEventIndex->Commit(PreparedCache))
				{
					PreparedCache.Reset();
				}
			}
			else
			{
				SightWeave::Geometry::SolveOptimizedPolygonInto(Input, SolveResult);
			}
		}
		else
		{
			if (PreparedEventIndex.IsValid())
			{
				PreparedEventIndex->Release(PreparedCache);
			}
			PreparedCache.Reset();
#if !UE_BUILD_SHIPPING
			SightWeave::Geometry::SolvePolygonInto(Input, Settings->SolverMode, SolveResult);
#endif
		}
		*CachedSegments = MoveTemp(Input.Segments);
		Entry.SolveTimeMicroseconds = (FPlatformTime::Seconds() - StartSeconds) * 1000000.0;
		Entry.CandidateSegmentCount = SolveResult.CandidateSegmentCount;
		Entry.CandidateRayCount = SolveResult.CastRayCount;
		Entry.CandidateAnglesRadians = MoveTemp(SolveResult.CandidateAnglesRadians);
		Entry.CandidateDistances = MoveTemp(SolveResult.CandidateDistances);
		Entry.CandidateBoundaryPoints = MoveTemp(SolveResult.CandidateBoundaryPoints);
		BuildPolarAngleUpperBoundLut(Entry.CandidateAnglesRadians, Entry.PolarAngleUpperBoundLut);
		if (SolveResult.bSucceeded)
		{
			Entry.Polygon.Vertices = MoveTemp(SolveResult.Vertices);
			SetPolygonBounds(Entry.Polygon.Vertices, Entry.Polygon.BoundsMin, Entry.Polygon.BoundsMax);
		}
	}
}

void USightWeaveWorldSubsystem::ResolveSnapshotCompatibility(FSightWeaveFrameSnapshot& Snapshot) const
{
	for (FSightWeaveVisionSnapshotEntry& Vision : Snapshot.VisionSources)
	{
		Vision.CompatibleIlluminationSources.Reset();
		Vision.CompatibleIlluminationSourceIndices.Reset();
		if (!Vision.Description.bActive
			|| Vision.Description.IlluminationPolicy == ESightWeaveIlluminationPolicy::BypassLegalIllumination)
		{
			continue;
		}
		for (int32 IlluminationIndex = 0; IlluminationIndex < Snapshot.IlluminationSources.Num(); ++IlluminationIndex)
		{
			const FSightWeaveIlluminationSnapshotEntry& Illumination = Snapshot.IlluminationSources[IlluminationIndex];
			if (Illumination.Description.bActive
				&& Illumination.Description.KnowledgeOwnerId == Vision.Description.KnowledgeOwnerId
				&& Illumination.Description.FloorId == Vision.Description.FloorId
				&& AreCapabilitiesCompatible(
					Vision.Description.Compatibility,
					Illumination.Description.EmittedCapabilities))
			{
				Vision.CompatibleIlluminationSources.Add(Illumination.Handle);
				Vision.CompatibleIlluminationSourceIndices.Add(IlluminationIndex);
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

bool USightWeaveWorldSubsystem::PrepareDynamicOccluderSegmentsInto(
	const FSightWeaveOccluderHandle Handle,
	const TConstArrayView<FSightWeaveSegment2D> Segments,
	const bool bDynamic,
	TArray<FSightWeaveSegment2D>& OutPrepared)
{
	// A single moving door edge needs only the one-segment subset of the exact
	// normalizer: finite/floor validation, endpoint weld and zero-length rejection,
	// attribution initialization, and runtime identity assignment. Retain the
	// outer and nested arrays so this common mutation remains allocation-stable.
	if (Segments.Num() == 1)
	{
		FSightWeaveGeometryTolerances Tolerances =
			GetDefault<USightWeaveSettings>()->GeometryTolerances;
		Tolerances.Normalize();
		const FSightWeaveSegment2D& Input = Segments[0];
		const double LengthSquared = FVector2D::DistSquared(Input.A, Input.B);
		if (!Input.IsFinite()
			|| !Input.FloorId.IsValid()
			|| LengthSquared <= FMath::Square(Tolerances.AuthoringWeldEpsilon)
			|| LengthSquared <= FMath::Square(Tolerances.ZeroLengthEpsilon))
		{
			OutPrepared.Reset();
			return false;
		}

		OutPrepared.SetNum(1, EAllowShrinking::No);
		FSightWeaveSegment2D& Output = OutPrepared[0];
		Output.A = Input.A;
		Output.B = Input.B;
		Output.FloorId = Input.FloorId;
		Output.HeightRange = Input.HeightRange;
		Output.OccluderHandle = Handle;
		Output.bDynamic = bDynamic;
		Output.StableId = NextSegmentId++;
		Output.SourceEdgeIndices.Reset();
		if (Input.SourceEdgeIndices.IsEmpty())
		{
			Output.SourceEdgeIndices.Add(0);
		}
		else
		{
			Output.SourceEdgeIndices.Append(Input.SourceEdgeIndices);
		}
		return true;
	}

	OutPrepared = PrepareOccluderSegments(Handle, Segments, bDynamic);
	return !OutPrepared.IsEmpty();
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
	CachedVisionSolveSegments.Reset();
	CachedIlluminationSolveSegments.Reset();
	CachedVisionCandidateQueryKeys.Reset();
	CachedIlluminationCandidateQueryKeys.Reset();
	CachedVisionPreparedSolves.Reset();
	CachedIlluminationPreparedSolves.Reset();
	if (PreparedEventIndex.IsValid())
	{
		PreparedEventIndex->Reset();
		PreparedEventIndex.Reset();
	}
	DynamicPreparedSegmentsScratch.Reset();
	PublishedSnapshot.Reset();
	StandbySnapshot.Reset();
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
	InitializeQueryResult(Result, Status, KnowledgeOwnerId, FloorId);
	return Result;
}

void USightWeaveWorldSubsystem::InitializeQueryResult(
	FSightWeaveVisibilityQueryResult& Result,
	const ESightWeaveQueryStatus Status,
	const FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
	const FSightWeaveFloorId FloorId,
	const FSightWeaveRevision* SnapshotRevision) const
{
	Result.Status = Status;
	Result.KnowledgeState = ESightWeaveKnowledgeState::Unknown;
	Result.bVisible = false;
	Result.bAuthoritative = false;
	Result.bInVisionPolygon = false;
	Result.bHasLegalIllumination = false;
	Result.bUsedBypass = false;
	Result.bOccluded = false;
	Result.bRejectedByIllumination = false;
	Result.bRejectedBySuppression = false;
	Result.bEligibleForMemoryWrite = false;
	Result.RejectionFlags = 0;
	if (SnapshotRevision)
	{
		Result.SnapshotRevision = *SnapshotRevision;
	}
	else
	{
		const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> Snapshot = PublishedSnapshot;
		Result.SnapshotRevision = Snapshot.IsValid() ? Snapshot->Revision : Revision;
	}
	Result.Revision = Result.SnapshotRevision;
	Result.KnowledgeOwnerId = KnowledgeOwnerId;
	Result.FloorId = FloorId;
	Result.ContributingVisionSources.Reset();
	Result.ContributingIlluminationSources.Reset();
	Result.ContributingSuppressions.Reset();
	Result.EvaluatedSampleCount = 0;
	Result.PassingSampleCount = 0;
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
