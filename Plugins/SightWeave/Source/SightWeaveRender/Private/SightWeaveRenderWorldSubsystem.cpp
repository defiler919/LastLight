#include "SightWeaveRenderWorldSubsystem.h"

#include "Engine/World.h"
#include "HAL/ThreadSafeCounter64.h"
#include "SceneViewExtension.h"
#include "SightWeaveQueries.h"
#include "SightWeaveSceneViewExtension.h"
#include "SightWeaveSettings.h"
#include "SightWeaveWorldSubsystem.h"
#include "Subsystems/SubsystemCollection.h"

namespace
{
	FThreadSafeCounter64 GNextSightWeaveRenderWorldSerial;

	const FSightWeaveFloorDefinition* FindFloor(
		const FSightWeaveFrameSnapshot& Snapshot,
		const FSightWeaveFloorId FloorId)
	{
		return Snapshot.Floors.FindByPredicate([FloorId](const FSightWeaveFloorDefinition& Floor)
		{
			return Floor.FloorId == FloorId;
		});
	}

	FIntPoint CalculateLogicalTile(
		const FVector2D& WorldPoint,
		const FVector2D& FloorOrigin)
	{
		const double InteriorWorldSpan =
			static_cast<double>(SightWeave::RenderPacket::InteriorTileSize)
			* SightWeave::RenderPacket::StandardCentimetersPerTexel;
		return FIntPoint(
			FMath::FloorToInt((WorldPoint.X - FloorOrigin.X) / InteriorWorldSpan),
			FMath::FloorToInt((WorldPoint.Y - FloorOrigin.Y) / InteriorWorldSpan));
	}

	FBox2D CalculatePhysicalBounds(
		const FIntPoint TileCoordinate,
		const FVector2D& FloorOrigin)
	{
		const double CentimetersPerTexel = SightWeave::RenderPacket::StandardCentimetersPerTexel;
		const double InteriorWorldSpan =
			static_cast<double>(SightWeave::RenderPacket::InteriorTileSize) * CentimetersPerTexel;
		const double GutterWorldSpan =
			static_cast<double>(SightWeave::RenderPacket::GutterTexels) * CentimetersPerTexel;
		const FVector2D PhysicalMin(
			FloorOrigin.X + static_cast<double>(TileCoordinate.X) * InteriorWorldSpan - GutterWorldSpan,
			FloorOrigin.Y + static_cast<double>(TileCoordinate.Y) * InteriorWorldSpan - GutterWorldSpan);
		const double PhysicalWorldSpan =
			static_cast<double>(SightWeave::RenderPacket::PhysicalTileSize) * CentimetersPerTexel;
		return FBox2D(PhysicalMin, PhysicalMin + FVector2D(PhysicalWorldSpan, PhysicalWorldSpan));
	}

	void CopyPolygonVertices(TConstArrayView<FVector> Vertices, TArray<FVector2D>& OutVertices)
	{
		OutVertices.Reset(Vertices.Num());
		for (const FVector& Vertex : Vertices)
		{
			OutVertices.Emplace(Vertex.X, Vertex.Y);
		}
	}

	FSightWeaveRenderPolygonInput& AddPolygon(
		FSightWeaveRenderPacketBuildInput& Input,
		const int64 StableSourceId,
		const ESightWeaveRenderMaskLayer Layer)
	{
		FSightWeaveRenderPolygonInput& Polygon = Input.Polygons.AddDefaulted_GetRef();
		Polygon.StableSourceId = StableSourceId;
		Polygon.Layer = Layer;
		Polygon.KnowledgeOwnerId = Input.KnowledgeOwnerId;
		Polygon.FloorId = Input.FloorId;
		Polygon.CompatibilityProfile = Input.CompatibilityProfile;
		return Polygon;
	}
}

void USightWeaveRenderWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<USightWeaveWorldSubsystem>();

	WorldIdentity.Serial = static_cast<uint64>(GNextSightWeaveRenderWorldSerial.Increment());
	SceneViewExtension = FSceneViewExtensions::NewExtension<FSightWeaveSceneViewExtension>(
		GetWorld(),
		WorldIdentity);

	if (USightWeaveWorldSubsystem* Runtime = GetWorld()->GetSubsystem<USightWeaveWorldSubsystem>())
	{
		SnapshotPublishedHandle = Runtime->OnSnapshotPublished().AddUObject(
			this,
			&USightWeaveRenderWorldSubsystem::HandleSnapshotPublished);
		BuildAndSubmitPacket(Runtime->AcquirePublishedSnapshot());
	}
}

void USightWeaveRenderWorldSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (USightWeaveWorldSubsystem* Runtime = World->GetSubsystem<USightWeaveWorldSubsystem>())
		{
			Runtime->OnSnapshotPublished().Remove(SnapshotPublishedHandle);
		}
	}
	SnapshotPublishedHandle.Reset();
	if (SceneViewExtension.IsValid())
	{
		SceneViewExtension->Shutdown(WorldIdentity);
		SceneViewExtension.Reset();
	}
	WorldIdentity = FSightWeaveRenderWorldIdentity();
	LastKnowledgeOwnerId = FSightWeaveKnowledgeOwnerId();
	LastFloorId = FSightWeaveFloorId();
	LastProfile = FSightWeaveRenderProfileIdentity();
	LastPhysicalWorldBounds = FBox2D(ForceInit);
	Super::Deinitialize();
}

void USightWeaveRenderWorldSubsystem::HandleSnapshotPublished(
	TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> Snapshot)
{
	BuildAndSubmitPacket(Snapshot);
}

void USightWeaveRenderWorldSubsystem::BuildAndSubmitPacket(
	const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe>& Snapshot)
{
	check(IsInGameThread());
	if (!WorldIdentity.IsValid() || !SceneViewExtension.IsValid() || !Snapshot.IsValid() || !Snapshot->bPublished)
	{
		return;
	}

	const uint64 SnapshotRevision = static_cast<uint64>(Snapshot->Revision.GetValue());
	const FSightWeaveVisionSnapshotEntry* FirstActiveVision = nullptr;
	for (const FSightWeaveVisionSnapshotEntry& Vision : Snapshot->VisionSources)
	{
		if (Vision.Description.bActive)
		{
			FirstActiveVision = &Vision;
			break;
		}
	}
	if (!FirstActiveVision)
	{
		if (LastKnowledgeOwnerId.IsValid() && LastFloorId.IsValid() && LastProfile.IsValid())
		{
			SubmitFailClosedClear(SnapshotRevision, ESightWeaveRenderPacketFailure::None);
		}
		return;
	}

	const FSightWeaveKnowledgeOwnerId TargetOwner = FirstActiveVision->Description.KnowledgeOwnerId;
	const FSightWeaveFloorId TargetFloor = FirstActiveVision->Description.FloorId;
	if (!TargetOwner.IsValid() || !TargetFloor.IsValid())
	{
		SubmitFailClosedClear(SnapshotRevision, ESightWeaveRenderPacketFailure::InvalidScope);
		return;
	}
	for (const FSightWeaveVisionSnapshotEntry& Vision : Snapshot->VisionSources)
	{
		if (Vision.Description.bActive
			&& (Vision.Description.KnowledgeOwnerId != TargetOwner || Vision.Description.FloorId != TargetFloor))
		{
			SubmitFailClosedClear(SnapshotRevision, ESightWeaveRenderPacketFailure::ScopeMismatch);
			return;
		}
	}

	FSightWeaveRenderProfileIdentity TargetProfile;
	bool bProfileSelected = false;
	for (const FSightWeaveVisionSnapshotEntry& Vision : Snapshot->VisionSources)
	{
		if (!Vision.Description.bActive
			|| Vision.Description.IlluminationPolicy == ESightWeaveIlluminationPolicy::BypassLegalIllumination)
		{
			continue;
		}
		const FSightWeaveRenderProfileIdentity Candidate =
			FSightWeaveRenderProfileIdentity::FromProfile(Vision.Description.Compatibility);
		if (!Candidate.IsValid() || (bProfileSelected && !TargetProfile.IsEquivalentTo(Candidate)))
		{
			SubmitFailClosedClear(SnapshotRevision, ESightWeaveRenderPacketFailure::ProfileMismatch);
			return;
		}
		TargetProfile = Candidate;
		bProfileSelected = true;
	}
	if (!bProfileSelected)
	{
		TargetProfile = FSightWeaveRenderProfileIdentity::FromProfile(
			FSightWeaveIlluminationCompatibilityProfile());
	}

	const FSightWeaveFloorDefinition* Floor = FindFloor(*Snapshot, TargetFloor);
	if (!Floor || !Floor->IsValid() || !Floor->bEnabled || !Floor->bActiveForQueries)
	{
		SubmitFailClosedClear(SnapshotRevision, ESightWeaveRenderPacketFailure::InvalidScope);
		return;
	}
	const FVector FirstLocation = FirstActiveVision->Description.Transform.GetLocation();
	const FIntPoint TileCoordinate = CalculateLogicalTile(
		FVector2D(FirstLocation.X, FirstLocation.Y),
		Floor->BoundsMin);
	const FBox2D PhysicalWorldBounds = CalculatePhysicalBounds(TileCoordinate, Floor->BoundsMin);

	FSightWeaveRenderPacketBuildInput Input;
	Input.WorldIdentity = WorldIdentity;
	Input.KnowledgeOwnerId = TargetOwner;
	Input.FloorId = TargetFloor;
	Input.CompatibilityProfile = TargetProfile;
	Input.PacketRevision = NextPacketRevision++;
	Input.RegistryRevision = SnapshotRevision;
	Input.PublishedSnapshotRevision = SnapshotRevision;
	Input.TileCoordinate = TileCoordinate;
	Input.PhysicalWorldBounds = PhysicalWorldBounds;
	Input.DirtyReason = ESightWeaveRenderDirtyReason::SourceChanged
		| ESightWeaveRenderDirtyReason::RegistryChanged;
	Input.bFullTile = true;

	TSet<int32> CompatibleIlluminationIndices;
	for (const FSightWeaveVisionSnapshotEntry& Vision : Snapshot->VisionSources)
	{
		if (!Vision.Description.bActive)
		{
			continue;
		}
		const ESightWeaveRenderMaskLayer Layer =
			Vision.Description.IlluminationPolicy == ESightWeaveIlluminationPolicy::BypassLegalIllumination
			? ESightWeaveRenderMaskLayer::Bypass
			: ESightWeaveRenderMaskLayer::Vision;
		FSightWeaveRenderPolygonInput& Polygon = AddPolygon(Input, Vision.Handle.GetValue(), Layer);
		CopyPolygonVertices(Vision.Polygon.Vertices, Polygon.WorldVertices);
		if (Layer == ESightWeaveRenderMaskLayer::Vision)
		{
			for (const int32 IlluminationIndex : Vision.CompatibleIlluminationSourceIndices)
			{
				if (!Snapshot->IlluminationSources.IsValidIndex(IlluminationIndex))
				{
					SubmitFailClosedClear(SnapshotRevision, ESightWeaveRenderPacketFailure::InvalidIndexData);
					return;
				}
				CompatibleIlluminationIndices.Add(IlluminationIndex);
			}
		}
	}

	TArray<int32> SortedIlluminationIndices = CompatibleIlluminationIndices.Array();
	SortedIlluminationIndices.Sort();
	for (const int32 IlluminationIndex : SortedIlluminationIndices)
	{
		const FSightWeaveIlluminationSnapshotEntry& Illumination = Snapshot->IlluminationSources[IlluminationIndex];
		if (!Illumination.Description.bActive
			|| Illumination.Description.KnowledgeOwnerId != TargetOwner
			|| Illumination.Description.FloorId != TargetFloor)
		{
			SubmitFailClosedClear(SnapshotRevision, ESightWeaveRenderPacketFailure::ScopeMismatch);
			return;
		}
		FSightWeaveRenderPolygonInput& Polygon = AddPolygon(
			Input,
			Illumination.Handle.GetValue(),
			ESightWeaveRenderMaskLayer::Illumination);
		CopyPolygonVertices(Illumination.Polygon.Vertices, Polygon.WorldVertices);
	}

	const int32 RadialSteps = FMath::Max(
		8,
		GetDefault<USightWeaveSettings>()->GeometryTolerances.RadialBoundarySteps);
	for (const FSightWeaveHardSuppressionSnapshotEntry& Suppression : Snapshot->HardSuppressions)
	{
		if (!Suppression.Description.bEnabled || Suppression.Description.FloorId != TargetFloor)
		{
			continue;
		}
		FSightWeaveRenderPolygonInput& Polygon = AddPolygon(
			Input,
			Suppression.Handle.GetValue(),
			ESightWeaveRenderMaskLayer::Suppression);
		Polygon.WorldVertices.Reserve(RadialSteps);
		for (int32 Step = 0; Step < RadialSteps; ++Step)
		{
			const double Angle = 2.0 * PI * static_cast<double>(Step) / static_cast<double>(RadialSteps);
			Polygon.WorldVertices.Add(Suppression.Description.Center + FVector2D(
				FMath::Cos(Angle) * Suppression.Description.Radius,
				FMath::Sin(Angle) * Suppression.Description.Radius));
		}
	}

	const FSightWeaveRenderPacketBuildResult BuildResult = FSightWeaveRenderPacketBuilder::Build(Input);
	if (!BuildResult.Succeeded())
	{
		SubmitFailClosedClear(SnapshotRevision, BuildResult.Failure);
		return;
	}
	LastKnowledgeOwnerId = TargetOwner;
	LastFloorId = TargetFloor;
	LastProfile = TargetProfile;
	LastTileCoordinate = TileCoordinate;
	LastPhysicalWorldBounds = PhysicalWorldBounds;
	Diagnostics.LastBuildFailure = ESightWeaveRenderPacketFailure::None;
	SubmitPacket(BuildResult.Packet);
}

void USightWeaveRenderWorldSubsystem::SubmitFailClosedClear(
	const uint64 SnapshotRevision,
	const ESightWeaveRenderPacketFailure Failure)
{
	Diagnostics.LastBuildFailure = Failure;
	++Diagnostics.FailClosedClearCount;
	if (!LastKnowledgeOwnerId.IsValid()
		|| !LastFloorId.IsValid()
		|| !LastProfile.IsValid()
		|| !LastPhysicalWorldBounds.bIsValid)
	{
		return;
	}

	FSightWeaveRenderPacketBuildInput ClearInput;
	ClearInput.WorldIdentity = WorldIdentity;
	ClearInput.KnowledgeOwnerId = LastKnowledgeOwnerId;
	ClearInput.FloorId = LastFloorId;
	ClearInput.CompatibilityProfile = LastProfile;
	ClearInput.PacketRevision = NextPacketRevision++;
	ClearInput.RegistryRevision = SnapshotRevision;
	ClearInput.PublishedSnapshotRevision = SnapshotRevision;
	ClearInput.TileCoordinate = LastTileCoordinate;
	ClearInput.PhysicalWorldBounds = LastPhysicalWorldBounds;
	ClearInput.DirtyReason = ESightWeaveRenderDirtyReason::ExplicitClear;
	ClearInput.bFullTile = true;
	const FSightWeaveRenderPacketBuildResult Clear = FSightWeaveRenderPacketBuilder::Build(ClearInput);
	if (Clear.Succeeded())
	{
		SubmitPacket(Clear.Packet);
	}
}

void USightWeaveRenderWorldSubsystem::SubmitPacket(
	TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe> Packet)
{
	if (!Packet.IsValid() || !SceneViewExtension.IsValid())
	{
		return;
	}
	++Diagnostics.PublishedPacketCount;
	Diagnostics.LastSubmittedPacketRevision = Packet->GetPacketRevision();
	Diagnostics.LastSubmittedSnapshotRevision = Packet->GetPublishedSnapshotRevision();
	SceneViewExtension->SubmitPacket(MoveTemp(Packet));
}
