#include "SightWeaveDebug.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "SightWeaveSettings.h"
#include "SightWeaveWorldSubsystem.h"

FSightWeaveDebugData USightWeaveWorldSubsystem::BuildDebugData() const
{
	FSightWeaveDebugData Data;
	Data.Snapshot = GetPublishedSnapshot();
	Data.GeometryTolerances = GetDefault<USightWeaveSettings>()->GeometryTolerances;
	Data.GeometryTolerances.Normalize();
	Data.SpatialIndexStats = SpatialIndex.GetStats();
	SpatialIndex.GetDebugCells(Data.SpatialCells);
	return Data;
}

bool USightWeaveWorldSubsystem::DrawDebugSnapshot(
	const FSightWeaveDebugDrawOptions& Options,
	const TArray<FSightWeaveDebugQueryMarker>& QueryMarkers) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	UWorld* World = GetWorld();
	if (!World || !bSightWeaveInitialized)
	{
		return false;
	}
	const FSightWeaveDebugData Data = BuildDebugData();
	const float Duration = FMath::Max(0.0f, Options.DurationSeconds);
	const float Thickness = FMath::Max(0.1f, Options.Thickness);
	auto DrawPolygon = [World, Duration, Thickness](TConstArrayView<FVector> Vertices, const FColor Color)
	{
		if (Vertices.Num() < 2) return;
		for (int32 Index = 0; Index < Vertices.Num(); ++Index)
		{
			DrawDebugLine(
				World,
				Vertices[Index],
				Vertices[(Index + 1) % Vertices.Num()],
				Color,
				false,
				Duration,
				0,
				Thickness);
		}
	};
	auto DrawCircleXY = [World, Duration, Thickness](const FVector& Center, const double Radius, const FColor Color)
	{
		DrawDebugCircle(
			World,
			Center,
			Radius,
			64,
			Color,
			false,
			Duration,
			0,
			Thickness,
			FVector(1.0, 0.0, 0.0),
			FVector(0.0, 1.0, 0.0),
			false);
	};

	if (Options.bDrawFloors)
	{
		for (const FSightWeaveFloorDefinition& Floor : Data.Snapshot.Floors)
		{
			const FVector Center(
				(Floor.BoundsMin.X + Floor.BoundsMax.X) * 0.5,
				(Floor.BoundsMin.Y + Floor.BoundsMax.Y) * 0.5,
				(Floor.HeightRange.ZMin + Floor.HeightRange.ZMax) * 0.5);
			const FVector Extent(
				(Floor.BoundsMax.X - Floor.BoundsMin.X) * 0.5,
				(Floor.BoundsMax.Y - Floor.BoundsMin.Y) * 0.5,
				(Floor.HeightRange.ZMax - Floor.HeightRange.ZMin) * 0.5);
			const FColor Color = Floor.bEnabled && Floor.bActiveForQueries ? FColor::Green : FColor::Silver;
			DrawDebugBox(World, Center, Extent, Color, false, Duration, 0, Thickness);
			DrawDebugString(
				World,
				FVector(Floor.BoundsMin.X, Floor.BoundsMin.Y, Floor.HeightRange.ZMax + 20.0),
				FString::Printf(TEXT("Floor %s rev %lld active=%d"),
					*Floor.FloorId.GetValue().ToString(),
					Floor.Revision.GetValue(),
					Floor.bActiveForQueries ? 1 : 0),
				nullptr,
				Color,
				Duration,
				false,
				1.0f);
		}
	}

	if (Options.bDrawSpatialCells)
	{
		for (const FSightWeaveSpatialCellDebug& Cell : Data.SpatialCells)
		{
			const FSightWeaveFloorDefinition* Floor = Data.Snapshot.Floors.FindByPredicate(
				[&Cell](const FSightWeaveFloorDefinition& Candidate) { return Candidate.FloorId == Cell.FloorId; });
			const double Z = Floor ? Floor->HeightRange.ZMin + 5.0 : 5.0;
			const FVector Center(
				(Cell.BoundsMin.X + Cell.BoundsMax.X) * 0.5,
				(Cell.BoundsMin.Y + Cell.BoundsMax.Y) * 0.5,
				Z);
			const FVector Extent(
				(Cell.BoundsMax.X - Cell.BoundsMin.X) * 0.5,
				(Cell.BoundsMax.Y - Cell.BoundsMin.Y) * 0.5,
				1.0);
			DrawDebugBox(World, Center, Extent, FColor(80, 120, 255), false, Duration, 0, 0.5f);
			DrawDebugString(World, Center, FString::FromInt(Cell.SegmentCount), nullptr, FColor(80, 120, 255), Duration, false, 0.7f);
		}
	}

	if (Options.bDrawOccluders)
	{
		for (const FSightWeaveSegment2D& Segment : Data.Snapshot.OccluderSegments)
		{
			const double ZMid = (Segment.HeightRange.ZMin + Segment.HeightRange.ZMax) * 0.5;
			const FVector A(Segment.A.X, Segment.A.Y, ZMid);
			const FVector B(Segment.B.X, Segment.B.Y, ZMid);
			const FColor Color = Segment.bDynamic ? FColor::Orange : FColor::White;
			DrawDebugLine(World, A, B, Color, false, Duration, 0, Thickness);
			DrawDebugPoint(World, A, 10.0f, Color, false, Duration, 0);
			DrawDebugPoint(World, B, 10.0f, Color, false, Duration, 0);
			DrawDebugLine(
				World,
				FVector(Segment.A.X, Segment.A.Y, Segment.HeightRange.ZMin),
				FVector(Segment.A.X, Segment.A.Y, Segment.HeightRange.ZMax),
				Color,
				false,
				Duration,
				0,
				0.5f);
		}
	}

	for (const FSightWeaveVisionSnapshotEntry& Entry : Data.Snapshot.VisionSources)
	{
		const FVector Origin = Entry.Description.Transform.GetLocation();
		const bool bBypass = Entry.Description.IlluminationPolicy == ESightWeaveIlluminationPolicy::BypassLegalIllumination;
		const FColor Color = bBypass ? FColor::Cyan : FColor::Green;
		if (Options.bDrawSources)
		{
			DrawDebugSphere(World, Origin, 18.0f, 12, Color, false, Duration, 0, Thickness);
			if (Entry.Description.Shape == ESightWeaveSourceShape::Radial)
			{
				DrawCircleXY(Origin, Entry.Description.Range, Color);
			}
			else
			{
				const FVector Forward3 = Entry.Description.Transform.GetUnitAxis(EAxis::X);
				const double ForwardAngle = FMath::Atan2(Forward3.Y, Forward3.X);
				const double HalfAngle = FMath::DegreesToRadians(Entry.Description.HalfAngleDegrees);
				for (const double Angle : { ForwardAngle - HalfAngle, ForwardAngle + HalfAngle })
				{
					DrawDebugLine(World, Origin, Origin + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * Entry.Description.Range,
						Color, false, Duration, 0, Thickness);
				}
			}
			const FString Capabilities = FString::JoinBy(
				Entry.Description.Compatibility.AcceptedCapabilities,
				TEXT("+"),
				[](const FName Capability) { return Capability.ToString(); });
			DrawDebugString(
				World,
				Origin + FVector(0.0, 0.0, 35.0),
				FString::Printf(TEXT("Vision %lld %s owner=%s compat=%s rays=%d"),
					Entry.Handle.GetValue(),
					bBypass ? TEXT("BYPASS") : TEXT("GATED"),
					*Entry.Description.KnowledgeOwnerId.GetValue().ToString(),
					bBypass ? TEXT("<none>") : *Capabilities,
					Entry.CandidateRayCount),
				nullptr,
				Color,
				Duration,
				false,
				0.8f);
		}
		if (Options.bDrawCandidateRays)
		{
			for (const double Angle : Entry.CandidateAnglesRadians)
			{
				DrawDebugLine(
					World,
					Origin,
					Origin + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * Entry.Description.Range,
					FColor(60, 100, 60),
					false,
					Duration,
					0,
					0.25f);
			}
		}
		if (Options.bDrawVisionPolygons)
		{
			DrawPolygon(Entry.Polygon.Vertices, Color);
		}
	}

	for (const FSightWeaveIlluminationSnapshotEntry& Entry : Data.Snapshot.IlluminationSources)
	{
		const FVector Origin = Entry.Description.Transform.GetLocation();
		if (Options.bDrawSources)
		{
			DrawDebugSphere(World, Origin, 14.0f, 10, FColor::Yellow, false, Duration, 0, Thickness);
			if (Entry.Description.Shape == ESightWeaveSourceShape::Radial)
			{
				DrawCircleXY(Origin, Entry.Description.Range, FColor::Yellow);
			}
			const FString Capabilities = FString::JoinBy(
				Entry.Description.EmittedCapabilities,
				TEXT("+"),
				[](const FName Capability) { return Capability.ToString(); });
			DrawDebugString(
				World,
				Origin + FVector(0.0, 0.0, 55.0),
				FString::Printf(TEXT("Legal light %lld owner=%s emits=%s rays=%d"),
					Entry.Handle.GetValue(),
					*Entry.Description.KnowledgeOwnerId.GetValue().ToString(),
					*Capabilities,
					Entry.CandidateRayCount),
				nullptr,
				FColor::Yellow,
				Duration,
				false,
				0.8f);
		}
		if (Options.bDrawCandidateRays)
		{
			for (const double Angle : Entry.CandidateAnglesRadians)
			{
				DrawDebugLine(World, Origin,
					Origin + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * Entry.Description.Range,
					FColor(100, 100, 40), false, Duration, 0, 0.25f);
			}
		}
		if (Options.bDrawIlluminationPolygons)
		{
			DrawPolygon(Entry.Polygon.Vertices, FColor::Yellow);
		}
	}

	if (Options.bDrawHardSuppressions)
	{
		for (const FSightWeaveHardSuppressionSnapshotEntry& Entry : Data.Snapshot.HardSuppressions)
		{
			const double Z = (Entry.Description.HeightRange.ZMin + Entry.Description.HeightRange.ZMax) * 0.5;
			const FVector Center(Entry.Description.Center.X, Entry.Description.Center.Y, Z);
			DrawCircleXY(Center, Entry.Description.Radius, FColor::Red);
			DrawDebugString(World, Center + FVector(0.0, 0.0, 30.0),
				FString::Printf(TEXT("SuppressLiveVision %lld"), Entry.Handle.GetValue()),
				nullptr, FColor::Red, Duration, false, 0.8f);
		}
	}

	for (const FSightWeaveDebugQueryMarker& Marker : QueryMarkers)
	{
		const FColor Color = Marker.Result.bVisible ? FColor::Green : FColor::Red;
		DrawDebugSphere(World, Marker.WorldLocation, 25.0f, 12, Color, false, Duration, 0, Thickness);
		DrawDebugString(
			World,
			Marker.WorldLocation + FVector(0.0, 0.0, 35.0),
			FString::Printf(TEXT("Query live=%d vision=%d light=%d bypass=%d occ=%d illumReject=%d suppress=%d flags=0x%02x rev=%lld V=%d L=%d"),
				Marker.Result.bVisible ? 1 : 0,
				Marker.Result.bInVisionPolygon ? 1 : 0,
				Marker.Result.bHasLegalIllumination ? 1 : 0,
				Marker.Result.bUsedBypass ? 1 : 0,
				Marker.Result.bOccluded ? 1 : 0,
				Marker.Result.bRejectedByIllumination ? 1 : 0,
				Marker.Result.bRejectedBySuppression ? 1 : 0,
				Marker.Result.RejectionFlags,
				Marker.Result.SnapshotRevision.GetValue(),
				Marker.Result.ContributingVisionSources.Num(),
				Marker.Result.ContributingIlluminationSources.Num()),
			nullptr,
			Color,
			Duration,
			false,
			0.8f);
	}

	const FVector EpsilonLocation = Data.Snapshot.Floors.IsEmpty()
		? FVector::ZeroVector
		: FVector(Data.Snapshot.Floors[0].BoundsMin.X, Data.Snapshot.Floors[0].BoundsMin.Y,
			Data.Snapshot.Floors[0].HeightRange.ZMax + 80.0);
	DrawDebugString(
		World,
		EpsilonLocation,
		FString::Printf(TEXT("SightWeave snapshot=%lld eps weld=%.4f zero=%.4f parallel=%.3g angular=%.6fdeg edge=%.4f pip=%.4f vertex=%.4f height=%.4f cells=%d segments=%d candidates=%d"),
			Data.Snapshot.Revision.GetValue(),
			Data.GeometryTolerances.AuthoringWeldEpsilon,
			Data.GeometryTolerances.ZeroLengthEpsilon,
			Data.GeometryTolerances.RayParallelEpsilon,
			Data.GeometryTolerances.EndpointAngularEpsilonDegrees,
			Data.GeometryTolerances.PointOnEdgeEpsilon,
			Data.GeometryTolerances.PointInPolygonEpsilon,
			Data.GeometryTolerances.DuplicateVertexEpsilon,
			Data.GeometryTolerances.HeightOverlapEpsilon,
			Data.SpatialIndexStats.CellCount,
			Data.SpatialIndexStats.SegmentCount,
			Data.SpatialIndexStats.LastCandidateCount),
		nullptr,
		FColor::Cyan,
		Duration,
		false,
		0.8f);
	return true;
#endif
}
