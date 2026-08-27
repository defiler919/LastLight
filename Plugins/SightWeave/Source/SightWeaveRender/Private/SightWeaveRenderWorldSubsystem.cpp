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

	void CopyPolygonVertices(TConstArrayView<FVector> Vertices, TArray<FVector2D>& OutVertices)
	{
		OutVertices.Reset(Vertices.Num());
		for (const FVector& Vertex : Vertices)
		{
			OutVertices.Emplace(Vertex.X, Vertex.Y);
		}
	}

	FSightWeaveSparseScopeBuildInput* FindScope(
		TArray<FSightWeaveSparseScopeBuildInput>& Scopes,
		const FSightWeaveKnowledgeOwnerId OwnerId,
		const FSightWeaveFloorId FloorId)
	{
		return Scopes.FindByPredicate([OwnerId, FloorId](const FSightWeaveSparseScopeBuildInput& Scope)
		{
			return Scope.KnowledgeOwnerId == OwnerId && Scope.FloorId == FloorId;
		});
	}

	FSightWeaveVisualFeatherSettings GetConfiguredVisualFeather()
	{
		FSightWeaveVisualFeatherSettings Result;
		Result.WidthCentimeters = FMath::Clamp(
			GetDefault<USightWeaveSettings>()->VisualFeatherWidthCentimeters,
			0.0f,
			SightWeave::VisualFeather::MaximumWidthCentimeters);
		return Result;
	}

	FSightWeaveSparsePolygonInput& AddPolygon(
		FSightWeaveSparseScopeBuildInput& Scope,
		const int64 StableSourceId,
		const uint64 SourceRevision,
		const ESightWeaveRenderMaskLayer Layer,
		const FSightWeaveRenderProfileIdentity& Profile)
	{
		FSightWeaveSparsePolygonInput& Polygon = Scope.Polygons.AddDefaulted_GetRef();
		Polygon.StableSourceId = StableSourceId;
		Polygon.SourceRevision = SourceRevision;
		Polygon.Layer = Layer;
		Polygon.CompatibilityProfile = Profile;
		return Polygon;
	}

	bool ContainsIlluminationForProfile(
		const FSightWeaveSparseScopeBuildInput& Scope,
		const int64 StableSourceId,
		const FSightWeaveRenderProfileIdentity& Profile)
	{
		return Scope.Polygons.ContainsByPredicate(
			[StableSourceId, &Profile](const FSightWeaveSparsePolygonInput& Polygon)
			{
				return Polygon.Layer == ESightWeaveRenderMaskLayer::Illumination
					&& Polygon.StableSourceId == StableSourceId
					&& Polygon.CompatibilityProfile.IsEquivalentTo(Profile);
			});
	}
}

void USightWeaveRenderWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<USightWeaveWorldSubsystem>();

	WorldIdentity.Serial = static_cast<uint64>(GNextSightWeaveRenderWorldSerial.Increment());
	PresentationSelection = FSightWeaveViewPresentationSelection::Disabled(
		WorldIdentity,
		NextPresentationRevision++);
	SceneViewExtension = FSceneViewExtensions::NewExtension<FSightWeaveSceneViewExtension>(
		GetWorld(),
		WorldIdentity);
	PublishPresentationSelection();

	if (USightWeaveWorldSubsystem* Runtime = GetWorld()->GetSubsystem<USightWeaveWorldSubsystem>())
	{
		SnapshotPublishedHandle = Runtime->OnSnapshotPublished().AddUObject(
			this,
			&USightWeaveRenderWorldSubsystem::HandleSnapshotPublished);
		BuildAndSubmitPacket(Runtime->AcquirePublishedSnapshot());
		MemoryPacketPublishedHandle = Runtime->OnMemoryPacketPublished().AddUObject(
			this,
			&USightWeaveRenderWorldSubsystem::HandleMemoryPacketPublished);
		HandleMemoryPacketPublished(Runtime->AcquirePublishedMemoryPacket());
	}
}

void USightWeaveRenderWorldSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (USightWeaveWorldSubsystem* Runtime = World->GetSubsystem<USightWeaveWorldSubsystem>())
		{
			Runtime->OnSnapshotPublished().Remove(SnapshotPublishedHandle);
			Runtime->OnMemoryPacketPublished().Remove(MemoryPacketPublishedHandle);
		}
	}
	SnapshotPublishedHandle.Reset();
	MemoryPacketPublishedHandle.Reset();
	if (SceneViewExtension.IsValid())
	{
		SceneViewExtension->Shutdown(WorldIdentity);
		SceneViewExtension.Reset();
	}
	LastPacket.Reset();
	PresentationSelection = FSightWeaveViewPresentationSelection();
	WorldIdentity = FSightWeaveRenderWorldIdentity();
	Super::Deinitialize();
}

void USightWeaveRenderWorldSubsystem::HandleMemoryPacketPublished(
	TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> Packet)
{
	if (SceneViewExtension.IsValid())
	{
		SceneViewExtension->SubmitMemoryPacket(MoveTemp(Packet));
	}
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
	UpdateDefaultPresentationSelection(*Snapshot);

	const uint64 SnapshotRevision = static_cast<uint64>(Snapshot->Revision.GetValue());
	FSightWeaveSparseRenderPacketBuildInput Input;
	Input.WorldIdentity = WorldIdentity;
	Input.PacketRevision = NextPacketRevision++;
	Input.RegistryRevision = SnapshotRevision;
	Input.PublishedSnapshotRevision = SnapshotRevision;
	Input.PreviousPacket = LastPacket;

	for (const FSightWeaveVisionSnapshotEntry& Vision : Snapshot->VisionSources)
	{
		if (!Vision.Description.bActive)
		{
			continue;
		}
		const FSightWeaveKnowledgeOwnerId OwnerId = Vision.Description.KnowledgeOwnerId;
		const FSightWeaveFloorId FloorId = Vision.Description.FloorId;
		const FSightWeaveFloorDefinition* Floor = FindFloor(*Snapshot, FloorId);
		if (!OwnerId.IsValid()
			|| !FloorId.IsValid()
			|| !Floor
			|| !Floor->IsValid()
			|| !Floor->bEnabled
			|| !Floor->bActiveForQueries)
		{
			SubmitFailClosedClear(SnapshotRevision, ESightWeaveSparsePacketFailure::InvalidScope);
			return;
		}

		FSightWeaveSparseScopeBuildInput* Scope = FindScope(Input.Scopes, OwnerId, FloorId);
		if (!Scope)
		{
			FSightWeaveSparseScopeBuildInput& Added = Input.Scopes.AddDefaulted_GetRef();
			Added.KnowledgeOwnerId = OwnerId;
			Added.FloorId = FloorId;
			Added.FloorOrigin = Floor->BoundsMin;
			Added.PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
			Added.MaximumActiveTiles = SightWeave::SparseAtlas::StandardActiveTileCapacity;
			Scope = &Added;
		}

		const bool bBypass = Vision.Description.IlluminationPolicy
			== ESightWeaveIlluminationPolicy::BypassLegalIllumination;
		const FSightWeaveRenderProfileIdentity Profile =
			FSightWeaveRenderProfileIdentity::FromProfile(Vision.Description.Compatibility);
		if (!Profile.IsValid())
		{
			SubmitFailClosedClear(SnapshotRevision, ESightWeaveSparsePacketFailure::InvalidProfile);
			return;
		}
		FSightWeaveSparsePolygonInput& VisionPolygon = AddPolygon(
			*Scope,
			Vision.Handle.GetValue(),
			static_cast<uint64>(Vision.SourceRevision.GetValue()),
			bBypass ? ESightWeaveRenderMaskLayer::Bypass : ESightWeaveRenderMaskLayer::Vision,
			Profile);
		CopyPolygonVertices(Vision.Polygon.Vertices, VisionPolygon.WorldVertices);

		if (bBypass)
		{
			continue;
		}
		for (const int32 IlluminationIndex : Vision.CompatibleIlluminationSourceIndices)
		{
			if (!Snapshot->IlluminationSources.IsValidIndex(IlluminationIndex))
			{
				SubmitFailClosedClear(SnapshotRevision, ESightWeaveSparsePacketFailure::InvalidPolygon);
				return;
			}
			const FSightWeaveIlluminationSnapshotEntry& Illumination =
				Snapshot->IlluminationSources[IlluminationIndex];
			if (!Illumination.Description.bActive
				|| Illumination.Description.KnowledgeOwnerId != OwnerId
				|| Illumination.Description.FloorId != FloorId)
			{
				SubmitFailClosedClear(SnapshotRevision, ESightWeaveSparsePacketFailure::InvalidScope);
				return;
			}
			const int64 IlluminationId = Illumination.Handle.GetValue();
			if (ContainsIlluminationForProfile(*Scope, IlluminationId, Profile))
			{
				continue;
			}
			FSightWeaveSparsePolygonInput& IlluminationPolygon = AddPolygon(
				*Scope,
				IlluminationId,
				static_cast<uint64>(Illumination.SourceRevision.GetValue()),
				ESightWeaveRenderMaskLayer::Illumination,
				Profile);
			CopyPolygonVertices(Illumination.Polygon.Vertices, IlluminationPolygon.WorldVertices);
		}
	}

	const int32 RadialSteps = FMath::Max(
		8,
		GetDefault<USightWeaveSettings>()->GeometryTolerances.RadialBoundarySteps);
	const FSightWeaveRenderProfileIdentity CommonProfile =
		FSightWeaveRenderProfileIdentity::FromProfile(FSightWeaveIlluminationCompatibilityProfile());
	for (FSightWeaveSparseScopeBuildInput& Scope : Input.Scopes)
	{
		for (const FSightWeaveHardSuppressionSnapshotEntry& Suppression : Snapshot->HardSuppressions)
		{
			if (!Suppression.Description.bEnabled || Suppression.Description.FloorId != Scope.FloorId)
			{
				continue;
			}
			FSightWeaveSparsePolygonInput& Polygon = AddPolygon(
				Scope,
				Suppression.Handle.GetValue(),
				static_cast<uint64>(Suppression.Revision.GetValue()),
				ESightWeaveRenderMaskLayer::Suppression,
				CommonProfile);
			Polygon.WorldVertices.Reserve(RadialSteps);
			for (int32 Step = 0; Step < RadialSteps; ++Step)
			{
				const double Angle = 2.0 * PI * static_cast<double>(Step) / static_cast<double>(RadialSteps);
				Polygon.WorldVertices.Add(Suppression.Description.Center + FVector2D(
					FMath::Cos(Angle) * Suppression.Description.Radius,
					FMath::Sin(Angle) * Suppression.Description.Radius));
			}
		}
	}
	if (Input.Scopes.IsEmpty() && !LastPacket.IsValid())
	{
		PublishPresentationSelection();
		return;
	}

	const FSightWeaveSparseRenderPacketBuildResult BuildResult =
		FSightWeaveSparseRenderPacketBuilder::Build(Input);
	if (!BuildResult.Succeeded())
	{
		SubmitFailClosedClear(SnapshotRevision, BuildResult.Failure);
		return;
	}
	Diagnostics.LastBuildFailure = ESightWeaveSparsePacketFailure::None;
	Diagnostics.LastDesiredTileCount = 0;
	Diagnostics.LastMaximumActiveTiles = 0;
	Diagnostics.FailClosedClearCount += static_cast<uint64>(BuildResult.FailedScopeCount);
	for (const FSightWeaveSparseRenderScope& Scope : BuildResult.Packet->GetScopes())
	{
		Diagnostics.LastDesiredTileCount = FMath::Max(
			Diagnostics.LastDesiredTileCount,
			Scope.DesiredTileCount);
		Diagnostics.LastMaximumActiveTiles = FMath::Max(
			Diagnostics.LastMaximumActiveTiles,
			Scope.MaximumActiveTiles);
		if (!Scope.IsValid() && Diagnostics.LastBuildFailure == ESightWeaveSparsePacketFailure::None)
		{
			Diagnostics.LastBuildFailure = Scope.Failure;
		}
	}
	SubmitPacket(BuildResult.Packet);
}

void USightWeaveRenderWorldSubsystem::SubmitFailClosedClear(
	const uint64 SnapshotRevision,
	const ESightWeaveSparsePacketFailure Failure)
{
	Diagnostics.LastBuildFailure = Failure;
	++Diagnostics.FailClosedClearCount;
	if (!LastPacket.IsValid())
	{
		return;
	}
	FSightWeaveSparseRenderPacketBuildInput ClearInput;
	ClearInput.WorldIdentity = WorldIdentity;
	ClearInput.PacketRevision = NextPacketRevision++;
	ClearInput.RegistryRevision = SnapshotRevision;
	ClearInput.PublishedSnapshotRevision = SnapshotRevision;
	ClearInput.PreviousPacket = LastPacket;
	const FSightWeaveSparseRenderPacketBuildResult Clear =
		FSightWeaveSparseRenderPacketBuilder::Build(ClearInput);
	if (Clear.Succeeded())
	{
		SubmitPacket(Clear.Packet);
	}
}

void USightWeaveRenderWorldSubsystem::SubmitPacket(
	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet)
{
	if (!Packet.IsValid() || !SceneViewExtension.IsValid())
	{
		return;
	}
	++Diagnostics.PublishedPacketCount;
	Diagnostics.LastSubmittedPacketRevision = Packet->GetPacketRevision();
	Diagnostics.LastSubmittedSnapshotRevision = Packet->GetPublishedSnapshotRevision();
	Diagnostics.SubmittedDirtyTileCount += static_cast<uint64>(Packet->GetDirtyTileIndices().Num());
	Diagnostics.SubmittedRemovedTileCount += static_cast<uint64>(Packet->GetRemovedTiles().Num());
	LastPacket = Packet;
	PublishPresentationSelection();
	SceneViewExtension->SubmitPacket(MoveTemp(Packet));
}

bool USightWeaveRenderWorldSubsystem::SetPresentationScope(
	const FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
	const FSightWeaveFloorId FloorId,
	const ESightWeaveRenderPrecisionTier PrecisionTier)
{
	check(IsInGameThread());
	if (!WorldIdentity.IsValid()
		|| !KnowledgeOwnerId.IsValid()
		|| !FloorId.IsValid()
		|| SightWeaveCentimetersPerTexel(PrecisionTier) <= 0.0f)
	{
		return false;
	}
	bHasExplicitPresentationScope = true;
	ExplicitPresentationOwner = KnowledgeOwnerId;
	ExplicitPresentationFloor = FloorId;
	ExplicitPresentationPrecision = PrecisionTier;
	PresentationSelection = FSightWeaveViewPresentationSelection::Enabled(
		WorldIdentity,
		KnowledgeOwnerId,
		FloorId,
		PrecisionTier,
		NextPresentationRevision++,
		GetConfiguredVisualFeather());
	PublishPresentationSelection();
	return true;
}

void USightWeaveRenderWorldSubsystem::ClearPresentationScope()
{
	check(IsInGameThread());
	bHasExplicitPresentationScope = false;
	ExplicitPresentationOwner = FSightWeaveKnowledgeOwnerId();
	ExplicitPresentationFloor = FSightWeaveFloorId();
	ExplicitPresentationPrecision = ESightWeaveRenderPrecisionTier::Standard;
	if (UWorld* World = GetWorld())
	{
		if (USightWeaveWorldSubsystem* Runtime = World->GetSubsystem<USightWeaveWorldSubsystem>())
		{
			const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> Snapshot =
				Runtime->AcquirePublishedSnapshot();
			if (Snapshot.IsValid() && Snapshot->bPublished)
			{
				UpdateDefaultPresentationSelection(*Snapshot);
				return;
			}
		}
	}
	PresentationSelection = FSightWeaveViewPresentationSelection::Disabled(
		WorldIdentity,
		NextPresentationRevision++);
	PublishPresentationSelection();
}

void USightWeaveRenderWorldSubsystem::UpdateDefaultPresentationSelection(
	const FSightWeaveFrameSnapshot& Snapshot)
{
	check(IsInGameThread());
	FSightWeaveKnowledgeOwnerId DesiredOwner;
	FSightWeaveFloorId DesiredFloor;
	ESightWeaveRenderPrecisionTier DesiredPrecision = ESightWeaveRenderPrecisionTier::Standard;
	bool bDesiredEnabled = false;
	const FSightWeaveVisualFeatherSettings DesiredVisualFeather = GetConfiguredVisualFeather();
	if (bHasExplicitPresentationScope)
	{
		DesiredOwner = ExplicitPresentationOwner;
		DesiredFloor = ExplicitPresentationFloor;
		DesiredPrecision = ExplicitPresentationPrecision;
		bDesiredEnabled = true;
	}
	else
	{
		const FSightWeaveFloorDefinition* ActiveFloor = nullptr;
		for (const FSightWeaveFloorDefinition& Floor : Snapshot.Floors)
		{
			if (!Floor.IsValid() || !Floor.bEnabled || !Floor.bActiveForQueries)
			{
				continue;
			}
			if (ActiveFloor)
			{
				ActiveFloor = nullptr;
				break;
			}
			ActiveFloor = &Floor;
		}
		if (ActiveFloor)
		{
			DesiredOwner = FSightWeaveKnowledgeOwnerId(FName(TEXT("Local")));
			DesiredFloor = ActiveFloor->FloorId;
			bDesiredEnabled = true;
		}
	}

	const bool bUnchanged = PresentationSelection.IsValid()
		&& PresentationSelection.IsEnabled() == bDesiredEnabled
		&& (!bDesiredEnabled
			|| (PresentationSelection.GetKnowledgeOwnerId() == DesiredOwner
				&& PresentationSelection.GetFloorId() == DesiredFloor
				&& PresentationSelection.GetPrecisionTier() == DesiredPrecision
				&& PresentationSelection.GetVisualFeather().IsEquivalentTo(DesiredVisualFeather)));
	if (bUnchanged)
	{
		return;
	}
	PresentationSelection = bDesiredEnabled
		? FSightWeaveViewPresentationSelection::Enabled(
			WorldIdentity,
			DesiredOwner,
			DesiredFloor,
			DesiredPrecision,
			NextPresentationRevision++,
			DesiredVisualFeather)
		: FSightWeaveViewPresentationSelection::Disabled(
			WorldIdentity,
			NextPresentationRevision++);
	PublishPresentationSelection();
}

void USightWeaveRenderWorldSubsystem::PublishPresentationSelection()
{
	check(IsInGameThread());
	Diagnostics.PresentationSelectionRevision = PresentationSelection.GetPresentationRevision();
	Diagnostics.bPresentationEnabled = PresentationSelection.IsEnabled();
	if (SceneViewExtension.IsValid() && PresentationSelection.IsValid())
	{
		SceneViewExtension->SubmitPresentationSelection(PresentationSelection);
	}
}
