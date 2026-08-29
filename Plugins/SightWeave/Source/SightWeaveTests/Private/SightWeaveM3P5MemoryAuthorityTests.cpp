#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SightWeaveMemory.h"

namespace SightWeaveM3P5MemoryAuthorityTests
{
	const FSightWeaveKnowledgeOwnerId LocalOwner(FName(TEXT("Local")));
	const FSightWeaveFloorId GroundFloor(FName(TEXT("Ground")));

	FSightWeavePolygon MakePolygon(
		const FSightWeaveVisionSourceHandle Handle,
		const TArray<FVector>& Vertices)
	{
		FSightWeavePolygon Result;
		Result.SourceHandle = Handle;
		Result.KnowledgeOwnerId = LocalOwner;
		Result.FloorId = GroundFloor;
		Result.Vertices = Vertices;
		Result.Revision = FSightWeaveRevision(1);
		Result.SourceRevision = FSightWeaveRevision(1);
		return Result;
	}

	FSightWeaveIlluminationPolygon MakeIlluminationPolygon(
		const FSightWeaveIlluminationSourceHandle Handle,
		const TArray<FVector>& Vertices)
	{
		FSightWeaveIlluminationPolygon Result;
		Result.SourceHandle = Handle;
		Result.KnowledgeOwnerId = LocalOwner;
		Result.FloorId = GroundFloor;
		Result.Vertices = Vertices;
		Result.Revision = FSightWeaveRevision(1);
		Result.SourceRevision = FSightWeaveRevision(1);
		return Result;
	}

	TArray<FVector> BoxVertices(
		const double MinimumX,
		const double MinimumY,
		const double MaximumX,
		const double MaximumY)
	{
		return {
			FVector(MinimumX, MinimumY, 100.0),
			FVector(MaximumX, MinimumY, 100.0),
			FVector(MaximumX, MaximumY, 100.0),
			FVector(MinimumX, MaximumY, 100.0)
		};
	}

	FSightWeaveFrameSnapshot MakeSnapshot(
		const int64 Revision,
		const bool bBypass,
		const TArray<FVector>& VisionVertices,
		const TArray<FVector>& IlluminationVertices = {})
	{
		FSightWeaveFrameSnapshot Snapshot;
		Snapshot.Revision = FSightWeaveRevision(Revision);
		Snapshot.bPublished = true;
		FSightWeaveFloorDefinition& Floor = Snapshot.Floors.AddDefaulted_GetRef();
		Floor.FloorId = GroundFloor;
		Floor.BoundsMin = FVector2D(-1000000.0, -1000000.0);
		Floor.BoundsMax = FVector2D(1000000.0, 1000000.0);
		Floor.HeightRange = { 0.0f, 300.0f };

		FSightWeaveVisionSnapshotEntry& Vision = Snapshot.VisionSources.AddDefaulted_GetRef();
		Vision.Handle = FSightWeaveVisionSourceHandle(1);
		Vision.Description.KnowledgeOwnerId = LocalOwner;
		Vision.Description.FloorId = GroundFloor;
		Vision.Description.bActive = true;
		Vision.Description.IlluminationPolicy = bBypass
			? ESightWeaveIlluminationPolicy::BypassLegalIllumination
			: ESightWeaveIlluminationPolicy::RequiresLegalIllumination;
		Vision.Description.Compatibility.AcceptedCapabilities = { FName(TEXT("Visible")) };
		Vision.Description.Compatibility.Normalize();
		Vision.Polygon = MakePolygon(Vision.Handle, VisionVertices);
		Vision.SourceRevision = FSightWeaveRevision(Revision);

		if (!bBypass)
		{
			FSightWeaveIlluminationSnapshotEntry& Illumination =
				Snapshot.IlluminationSources.AddDefaulted_GetRef();
			Illumination.Handle = FSightWeaveIlluminationSourceHandle(1);
			Illumination.Description.KnowledgeOwnerId = LocalOwner;
			Illumination.Description.FloorId = GroundFloor;
			Illumination.Description.bActive = true;
			Illumination.Description.EmittedCapabilities = { FName(TEXT("Visible")) };
			Illumination.Polygon = MakeIlluminationPolygon(
				Illumination.Handle,
				IlluminationVertices);
			Illumination.SourceRevision = FSightWeaveRevision(Revision);
			Vision.CompatibleIlluminationSources.Add(Illumination.Handle);
			Vision.CompatibleIlluminationSourceIndices.Add(0);
		}
		return Snapshot;
	}

	bool Configure(
		FSightWeaveMemoryAuthority& Authority,
		const FSightWeaveFrameSnapshot& Snapshot,
		const ESightWeaveRenderPrecisionTier Precision =
			ESightWeaveRenderPrecisionTier::Standard,
		const uint64 WorldSerial = 1)
	{
		FSightWeaveMemoryScopeKey Scope;
		return FSightWeaveMemoryAuthority::BuildScopeForSnapshot(
				Snapshot,
				FSightWeaveRenderWorldIdentity { WorldSerial },
				WorldSerial,
				LocalOwner,
				GroundFloor,
				Precision,
				Scope)
			&& Authority.Configure(Scope, 256);
	}

	FSightWeaveMemoryRegion MakeRegion(
		const FSightWeaveMemoryAuthority& Authority,
		const ESightWeaveMemoryRegionShape Shape,
		const FVector2D Center,
		const FVector2D HalfExtents = FVector2D(100.0, 100.0))
	{
		FSightWeaveMemoryRegion Region;
		Region.Scope = Authority.GetScope();
		Region.HeightRange = { 0.0f, 300.0f };
		Region.Shape = Shape;
		Region.Center = Center;
		Region.HalfExtents = HalfExtents;
		Region.Radius = static_cast<float>(HalfExtents.X);
		if (Shape == ESightWeaveMemoryRegionShape::Polygon)
		{
			Region.PolygonVertices = {
				Center + FVector2D(-HalfExtents.X, -HalfExtents.Y),
				Center + FVector2D(HalfExtents.X, -HalfExtents.Y),
				Center + FVector2D(HalfExtents.X, HalfExtents.Y),
				Center + FVector2D(-HalfExtents.X, HalfExtents.Y)
			};
		}
		return Region;
	}
}

namespace SightWeaveM3P5MemoryAuthorityTests
{

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P5PackedMappingTest,
	"SightWeave.M3P5.Memory.Authority.PackedMappingNegativeAndLarge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSightWeaveM3P5PackedMappingTest::RunTest(const FString& Parameters)
{
	FSightWeaveFrameSnapshot Snapshot =
		MakeSnapshot(1, true, BoxVertices(-100.0, -100.0, 100.0, 100.0));
	FSightWeaveMemoryAuthority Authority;
	TestTrue(TEXT("Authority configures"), Configure(Authority, Snapshot));

	FIntPoint Tile;
	FIntPoint Texel;
	TestTrue(
		TEXT("Negative coordinate maps"),
		FSightWeaveMemoryAuthority::WorldToTileAndTexel(
			Authority.GetScope(),
			FVector2D(-1000000.1, -1000000.1),
			Tile,
			Texel));
	TestEqual(TEXT("Negative X tile"), Tile.X, -1);
	TestEqual(TEXT("Negative Y tile"), Tile.Y, -1);
	TestEqual(TEXT("Negative X interior"), Texel.X, 247);
	TestEqual(TEXT("Negative Y interior"), Texel.Y, 247);

	const FVector2D LargePoint(
		Authority.GetScope().FloorOrigin.X + 1000000.0,
		Authority.GetScope().FloorOrigin.Y + 2000000.0);
	TestTrue(
		TEXT("Large coordinate maps deterministically"),
		FSightWeaveMemoryAuthority::WorldToTileAndTexel(
			Authority.GetScope(),
			LargePoint,
			Tile,
			Texel));
	TestTrue(TEXT("Large mapping remains signed-positive"), Tile.X > 0 && Tile.Y > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P5FirstExplorationTest,
	"SightWeave.M3P5.Memory.Authority.FirstExploreNoChangeOffscreen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSightWeaveM3P5FirstExplorationTest::RunTest(const FString& Parameters)
{
	const TArray<FVector> Coverage =
		BoxVertices(-1000100.0, -1000100.0, -999700.0, -999700.0);
	FSightWeaveFrameSnapshot Snapshot = MakeSnapshot(1, true, Coverage);
	FSightWeaveMemoryAuthority Authority;
	TestTrue(TEXT("Authority configures without a camera"), Configure(Authority, Snapshot));
	const FSightWeaveMemoryUpdateDiagnostics First = Authority.WriteEffectiveLive(Snapshot);
	TestTrue(TEXT("Offscreen CPU source writes"), First.Succeeded() && First.bAuthorityChanged);
	TestEqual(TEXT("First write advances once"), Authority.GetMemoryRevision(), uint64(1));
	TestTrue(TEXT("Inside point remembered"), Authority.QueryHardMemory2D(FVector2D(-1000000.0, -1000000.0)));
	TestFalse(TEXT("Outside point unknown"), Authority.QueryHardMemory2D(FVector2D(-999000.0, -999000.0)));

	const TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> FirstPacket =
		Authority.PublishPacket();
	TestTrue(TEXT("Initial immutable packet is valid"), FirstPacket.IsValid() && FirstPacket->IsValid());
	TestTrue(TEXT("Initial packet is full"), FirstPacket->IsFullRebuild());
	TestTrue(TEXT("Initial packet owns packed dirty payload"), !FirstPacket->GetDirtyTiles().IsEmpty());

	const FSightWeaveMemoryUpdateDiagnostics Duplicate = Authority.WriteEffectiveLive(Snapshot);
	TestTrue(TEXT("Duplicate snapshot succeeds"), Duplicate.Succeeded() && Duplicate.bDuplicateSnapshot);
	TestEqual(TEXT("Duplicate does not advance memory"), Authority.GetMemoryRevision(), uint64(1));
	const TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> DuplicatePacket =
		Authority.PublishPacket();
	TestFalse(TEXT("Duplicate packet schedules no mirror work"), DuplicatePacket->HasMirrorWork());

	Snapshot.Revision = FSightWeaveRevision(2);
	const FSightWeaveMemoryUpdateDiagnostics NoChange = Authority.WriteEffectiveLive(Snapshot);
	TestTrue(TEXT("New provenance with identical bits succeeds"), NoChange.Succeeded());
	TestFalse(TEXT("No-change does not mutate authority"), NoChange.bAuthorityChanged);
	TestEqual(TEXT("No-change revision remains stable"), Authority.GetMemoryRevision(), uint64(1));
	TestFalse(TEXT("No-change publishes zero mirror work"), Authority.PublishPacket()->HasMirrorWork());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P5ConcaveTileEdgeLeakTest,
	"SightWeave.M3P5.Memory.Authority.ConcavePolygonDoesNotLeakAtTileEdge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSightWeaveM3P5ConcaveTileEdgeLeakTest::RunTest(const FString& Parameters)
{
	constexpr double Origin = -1000000.0;
	const TArray<FVector> ConcaveCoverage = {
		FVector(Origin + 100.0, Origin + 100.0, 100.0),
		FVector(Origin + 4000.0, Origin + 100.0, 100.0),
		FVector(Origin + 4000.0, Origin + 4000.0, 100.0),
		FVector(Origin + 3000.0, Origin + 4000.0, 100.0),
		FVector(Origin + 3000.0, Origin + 1000.0, 100.0),
		FVector(Origin + 100.0, Origin + 1000.0, 100.0)
	};
	const FSightWeaveFrameSnapshot Snapshot = MakeSnapshot(1, true, ConcaveCoverage);
	FSightWeaveMemoryAuthority Authority;
	TestTrue(TEXT("Authority configures"), Configure(Authority, Snapshot));
	TestTrue(TEXT("Concave coverage writes"), Authority.WriteEffectiveLive(Snapshot).Succeeded());
	TestTrue(
		TEXT("Upper right leg is remembered"),
		Authority.QueryHardMemory2D(FVector2D(Origin + 3500.0, Origin + 3000.0)));
	TestTrue(
		TEXT("Lower cross-tile leg is remembered"),
		Authority.QueryHardMemory2D(FVector2D(Origin + 1000.0, Origin + 500.0)));
	TestFalse(
		TEXT("Out-of-tile scanline interval does not clamp into a false edge texel"),
		Authority.QueryHardMemory2D(FVector2D(Origin + 2475.0, Origin + 3000.0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P5CompatibilityAndSeamTest,
	"SightWeave.M3P5.Memory.Authority.CompatibilitySeamDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSightWeaveM3P5CompatibilityAndSeamTest::RunTest(const FString& Parameters)
{
	const TArray<FVector> Vision =
		BoxVertices(-1000200.0, -1000200.0, -994900.0, -994900.0);
	const TArray<FVector> Light =
		BoxVertices(-1000200.0, -1000200.0, -997500.0, -994900.0);
	const FSightWeaveFrameSnapshot Snapshot = MakeSnapshot(1, false, Vision, Light);
	FSightWeaveMemoryAuthority A;
	FSightWeaveMemoryAuthority B;
	TestTrue(TEXT("A configures"), Configure(A, Snapshot));
	TestTrue(TEXT("B configures"), Configure(B, Snapshot));
	TestTrue(TEXT("A writes"), A.WriteEffectiveLive(Snapshot).Succeeded());
	TestTrue(TEXT("B writes"), B.WriteEffectiveLive(Snapshot).Succeeded());
	TestTrue(TEXT("Compatible overlap remembered"), A.QueryHardMemory2D(FVector2D(-999000.0, -999000.0)));
	TestFalse(TEXT("Vision without compatible light remains unknown"), A.QueryHardMemory2D(FVector2D(-996000.0, -999000.0)));

	const double SeamX = -1000000.0 + 2480.0;
	TestTrue(TEXT("Left of logical seam remembered"), A.QueryHardMemory2D(FVector2D(SeamX - 0.1, -999000.0)));
	TestTrue(TEXT("Right of logical seam remembered"), A.QueryHardMemory2D(FVector2D(SeamX + 0.1, -999000.0)));

	const TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> PacketA = A.PublishPacket();
	const TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> PacketB = B.PublishPacket();
	TestEqual(TEXT("Deterministic tile count"), PacketA->GetDirtyTiles().Num(), PacketB->GetDirtyTiles().Num());
	for (int32 Index = 0; Index < PacketA->GetDirtyTiles().Num(); ++Index)
	{
		TestTrue(
			*FString::Printf(TEXT("Deterministic packed bytes %d"), Index),
			PacketA->GetDirtyTiles()[Index].PackedBits
				== PacketB->GetDirtyTiles()[Index].PackedBits);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P5IsolationLifecycleTest,
	"SightWeave.M3P5.Memory.Authority.IsolationLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSightWeaveM3P5IsolationLifecycleTest::RunTest(const FString& Parameters)
{
	const FSightWeaveFrameSnapshot Snapshot =
		MakeSnapshot(1, true, BoxVertices(-1000100.0, -1000100.0, -999700.0, -999700.0));
	FSightWeaveMemoryAuthority OldWorld;
	FSightWeaveMemoryAuthority NewWorld;
	TestTrue(TEXT("Old world configures"), Configure(OldWorld, Snapshot, ESightWeaveRenderPrecisionTier::Standard, 11));
	TestTrue(TEXT("New world configures"), Configure(NewWorld, Snapshot, ESightWeaveRenderPrecisionTier::Standard, 12));
	TestFalse(
		TEXT("World lifetime is part of exact scope identity"),
		OldWorld.GetScope().IsEquivalentTo(NewWorld.GetScope()));
	TestTrue(TEXT("Old world writes"), OldWorld.WriteEffectiveLive(Snapshot).Succeeded());
	TestFalse(TEXT("New world starts empty"), NewWorld.QueryHardMemory2D(FVector2D(-1000000.0, -1000000.0)));

	FSightWeaveMemoryScopeKey CollisionA = OldWorld.GetScope();
	FSightWeaveMemoryScopeKey CollisionB = CollisionA;
	CollisionA.CanonicalProfiles[0].CanonicalCapabilities = { FName(TEXT("Visible")) };
	CollisionB.CanonicalProfiles[0].CanonicalCapabilities = { FName(TEXT("Infrared")) };
	CollisionA.CanonicalProfiles[0].StableHash = 0x1234;
	CollisionB.CanonicalProfiles[0].StableHash = 0x1234;
	TestFalse(TEXT("Forced hash collision does not imply scope equality"), CollisionA.IsEquivalentTo(CollisionB));

	FSightWeaveFrameSnapshot Removed = Snapshot;
	Removed.Revision = FSightWeaveRevision(2);
	Removed.VisionSources.Reset();
	TestTrue(TEXT("Source removal publication succeeds"), OldWorld.WriteEffectiveLive(Removed).Succeeded());
	TestTrue(
		TEXT("Source removal does not erase monotonic memory"),
		OldWorld.QueryHardMemory2D(FVector2D(-1000000.0, -1000000.0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P5ClearAndReexploreTest,
	"SightWeave.M3P5.Memory.Modifier.ClearAndReexplore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSightWeaveM3P5ClearAndReexploreTest::RunTest(const FString& Parameters)
{
	FSightWeaveFrameSnapshot Snapshot =
		MakeSnapshot(1, true, BoxVertices(-1000300.0, -1000300.0, -999700.0, -999700.0));
	FSightWeaveMemoryAuthority Authority;
	TestTrue(TEXT("Configures"), Configure(Authority, Snapshot));
	TestTrue(TEXT("Explores"), Authority.WriteEffectiveLive(Snapshot).Succeeded());
	const FVector2D Center(-1000000.0, -1000000.0);
	TestTrue(TEXT("Center starts remembered"), Authority.QueryHardMemory2D(Center));
	const uint64 BeforeClearRevision = Authority.GetMemoryRevision();
	TestTrue(
		TEXT("Clear mutation succeeds"),
		Authority.ClearMemory(MakeRegion(
			Authority,
			ESightWeaveMemoryRegionShape::Circle,
			Center,
			FVector2D(150.0, 150.0))));
	TestFalse(TEXT("Clear returns the center to Unknown"), Authority.QueryHardMemory2D(Center));
	TestEqual(TEXT("Clear advances authority once"), Authority.GetMemoryRevision(), BeforeClearRevision + 1);

	Snapshot.Revision = FSightWeaveRevision(2);
	TestTrue(TEXT("Later snapshot re-explores"), Authority.WriteEffectiveLive(Snapshot).Succeeded());
	TestTrue(TEXT("Cleared center can be remembered again"), Authority.QueryHardMemory2D(Center));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P5BlockAndSuppressTest,
	"SightWeave.M3P5.Memory.Modifier.BlockSuppressOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSightWeaveM3P5BlockAndSuppressTest::RunTest(const FString& Parameters)
{
	FSightWeaveFrameSnapshot Snapshot =
		MakeSnapshot(1, true, BoxVertices(-1000500.0, -1000500.0, -999800.0, -999800.0));
	FSightWeaveMemoryAuthority Authority;
	TestTrue(TEXT("Configures"), Configure(Authority, Snapshot));
	TestTrue(TEXT("Initial exploration"), Authority.WriteEffectiveLive(Snapshot).Succeeded());
	const FVector ExistingPoint(-1000200.0, -1000200.0, 100.0);
	const FVector NewBlockedPoint(-999500.0, -999500.0, 100.0);
	TestTrue(TEXT("Existing point remembered"), Authority.QueryHardMemory(ExistingPoint));

	FSightWeaveMemoryModifierDescription Block;
	Block.Operation = ESightWeaveMemoryModifierOperation::BlockMemoryWrites;
	Block.Region = MakeRegion(
		Authority,
		ESightWeaveMemoryRegionShape::AxisAlignedBox,
		FVector2D(-999500.0, -999500.0),
		FVector2D(180.0, 180.0));
	const FSightWeaveMemoryModifierHandle BlockHandle = Authority.RegisterModifier(Block);
	TestTrue(TEXT("Block registers"), BlockHandle.IsValid());
	TestTrue(TEXT("Block suppresses remembered presentation"), Authority.IsMemoryPresentationSuppressed(NewBlockedPoint));
	TestTrue(TEXT("Block does not clear old authority"), Authority.QueryHardMemory(ExistingPoint));

	Snapshot.VisionSources[0].Polygon =
		MakePolygon(
			Snapshot.VisionSources[0].Handle,
			BoxVertices(-1000500.0, -1000500.0, -999300.0, -999300.0));
	Snapshot.Revision = FSightWeaveRevision(2);
	TestTrue(TEXT("Blocked update succeeds"), Authority.WriteEffectiveLive(Snapshot).Succeeded());
	TestFalse(TEXT("Block prevents new HardMemory"), Authority.QueryHardMemory(NewBlockedPoint));

	FSightWeaveMemoryModifierDescription Suppress;
	Suppress.Operation = ESightWeaveMemoryModifierOperation::SuppressMemoryPresentation;
	Suppress.Region = MakeRegion(
		Authority,
		ESightWeaveMemoryRegionShape::Circle,
		FVector2D(ExistingPoint.X, ExistingPoint.Y),
		FVector2D(100.0, 100.0));
	const uint64 BeforeSuppressRevision = Authority.GetMemoryRevision();
	const FSightWeaveMemoryModifierHandle SuppressHandle = Authority.RegisterModifier(Suppress);
	TestTrue(TEXT("Suppress registers"), SuppressHandle.IsValid());
	TestTrue(TEXT("Suppress hides presentation"), Authority.IsMemoryPresentationSuppressed(ExistingPoint));
	TestTrue(TEXT("Suppress preserves authority"), Authority.QueryHardMemory(ExistingPoint));
	TestEqual(TEXT("Suppress does not revise HardMemory"), Authority.GetMemoryRevision(), BeforeSuppressRevision);

	const TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> ModifierPacket =
		Authority.PublishPacket();
	TestTrue(TEXT("Modifier state schedules derived presentation work"), ModifierPacket->HasMirrorWork());
	TestEqual(TEXT("Block and suppress are both published"), ModifierPacket->GetPresentationSuppressions().Num(), 2);

	TestTrue(TEXT("Suppress unregisters"), Authority.UnregisterModifier(SuppressHandle));
	TestFalse(TEXT("Suppress teardown restores presentation"), Authority.IsMemoryPresentationSuppressed(ExistingPoint));
	TestTrue(TEXT("Block unregisters"), Authority.UnregisterModifier(BlockHandle));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P5ModifierShapesTest,
	"SightWeave.M3P5.Memory.Modifier.ShapesMoveDestroyScopeHeight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSightWeaveM3P5ModifierShapesTest::RunTest(const FString& Parameters)
{
	const FSightWeaveFrameSnapshot Snapshot =
		MakeSnapshot(1, true, BoxVertices(-1001000.0, -1001000.0, -998000.0, -998000.0));
	FSightWeaveMemoryAuthority Authority;
	TestTrue(TEXT("Configures"), Configure(Authority, Snapshot));

	const ESightWeaveMemoryRegionShape Shapes[] =
	{
		ESightWeaveMemoryRegionShape::Circle,
		ESightWeaveMemoryRegionShape::AxisAlignedBox,
		ESightWeaveMemoryRegionShape::RotatedBox,
		ESightWeaveMemoryRegionShape::Polygon
	};
	for (int32 ShapeIndex = 0; ShapeIndex < UE_ARRAY_COUNT(Shapes); ++ShapeIndex)
	{
		const FVector2D Center(-1000800.0 + ShapeIndex * 300.0, -1000800.0);
		FSightWeaveMemoryModifierDescription Description;
		Description.Operation = ESightWeaveMemoryModifierOperation::SuppressMemoryPresentation;
		Description.Region = MakeRegion(Authority, Shapes[ShapeIndex], Center);
		Description.Region.RotationDegrees = Shapes[ShapeIndex]
				== ESightWeaveMemoryRegionShape::RotatedBox
			? 45.0f
			: 0.0f;
		const FSightWeaveMemoryModifierHandle Handle = Authority.RegisterModifier(Description);
		TestTrue(*FString::Printf(TEXT("Shape %d registers"), ShapeIndex), Handle.IsValid());
		TestTrue(
			*FString::Printf(TEXT("Shape %d contains center"), ShapeIndex),
			Authority.IsMemoryPresentationSuppressed(FVector(Center, 100.0)));
	}

	FSightWeaveMemoryModifierDescription Moving;
	Moving.Operation = ESightWeaveMemoryModifierOperation::SuppressMemoryPresentation;
	Moving.Region = MakeRegion(
		Authority,
		ESightWeaveMemoryRegionShape::RotatedBox,
		FVector2D(-999000.0, -999000.0));
	const FSightWeaveMemoryModifierHandle MovingHandle = Authority.RegisterModifier(Moving);
	const FVector OldPoint(-999000.0, -999000.0, 100.0);
	const FVector NewPoint(-998700.0, -999000.0, 100.0);
	TestTrue(TEXT("Moving modifier starts at old point"), Authority.IsMemoryPresentationSuppressed(OldPoint));
	Moving.Region.Center = FVector2D(NewPoint.X, NewPoint.Y);
	TestTrue(TEXT("Moving modifier updates"), Authority.UpdateModifier(MovingHandle, Moving));
	TestFalse(TEXT("Old bounds are released"), Authority.IsMemoryPresentationSuppressed(OldPoint));
	TestTrue(TEXT("New bounds are active"), Authority.IsMemoryPresentationSuppressed(NewPoint));
	TestTrue(TEXT("Destroy succeeds"), Authority.UnregisterModifier(MovingHandle));
	TestFalse(TEXT("Destroy leaves no stale state"), Authority.IsMemoryPresentationSuppressed(NewPoint));

	FSightWeaveMemoryModifierDescription HeightFiltered;
	HeightFiltered.Operation = ESightWeaveMemoryModifierOperation::SuppressMemoryPresentation;
	HeightFiltered.Region = MakeRegion(
		Authority,
		ESightWeaveMemoryRegionShape::Circle,
		FVector2D(-998300.0, -998300.0));
	HeightFiltered.Region.HeightRange = { 0.0f, 50.0f };
	TestTrue(TEXT("Height modifier registers"), Authority.RegisterModifier(HeightFiltered).IsValid());
	TestFalse(
		TEXT("Height mismatch does not suppress"),
		Authority.IsMemoryPresentationSuppressed(FVector(-998300.0, -998300.0, 100.0)));

	FSightWeaveMemoryModifierDescription WrongScope = HeightFiltered;
	WrongScope.Region.Scope.KnowledgeOwnerId =
		FSightWeaveKnowledgeOwnerId(FName(TEXT("OtherOwner")));
	TestFalse(TEXT("Scope mismatch is rejected"), Authority.RegisterModifier(WrongScope).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P5ClearBlockPriorityTest,
	"SightWeave.M3P5.Memory.Modifier.ClearBlockPriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSightWeaveM3P5ClearBlockPriorityTest::RunTest(const FString& Parameters)
{
	FSightWeaveFrameSnapshot Snapshot =
		MakeSnapshot(1, true, BoxVertices(-1000300.0, -1000300.0, -999700.0, -999700.0));
	FSightWeaveMemoryAuthority Authority;
	TestTrue(TEXT("Configures"), Configure(Authority, Snapshot));
	TestTrue(TEXT("Initial exploration"), Authority.WriteEffectiveLive(Snapshot).Succeeded());
	const FVector Center(-1000000.0, -1000000.0, 100.0);

	const FSightWeaveMemoryRegion Region = MakeRegion(
		Authority,
		ESightWeaveMemoryRegionShape::AxisAlignedBox,
		FVector2D(Center.X, Center.Y),
		FVector2D(150.0, 150.0));
	FSightWeaveMemoryModifierDescription Block;
	Block.Operation = ESightWeaveMemoryModifierOperation::BlockMemoryWrites;
	Block.Region = Region;
	TestTrue(TEXT("Block registers"), Authority.RegisterModifier(Block).IsValid());
	TestTrue(TEXT("Clear applies before blocked reacquisition"), Authority.ClearMemory(Region));
	Snapshot.Revision = FSightWeaveRevision(2);
	TestTrue(TEXT("Overlapped update succeeds"), Authority.WriteEffectiveLive(Snapshot).Succeeded());
	TestFalse(TEXT("Clear plus block remains Unknown"), Authority.QueryHardMemory(Center));
	return true;
}

}

#endif
