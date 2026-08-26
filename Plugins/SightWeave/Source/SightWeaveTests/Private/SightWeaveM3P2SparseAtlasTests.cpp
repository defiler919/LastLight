#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SightWeaveSparseAtlas.h"

namespace SightWeave::M3P2::SparseAtlasTests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FSightWeaveRenderProfileIdentity MakeProfile(std::initializer_list<const TCHAR*> Capabilities)
	{
		FSightWeaveIlluminationCompatibilityProfile Profile;
		for (const TCHAR* Capability : Capabilities)
		{
			Profile.AcceptedCapabilities.Add(FName(Capability));
		}
		return FSightWeaveRenderProfileIdentity::FromProfile(Profile);
	}

	FSightWeaveSparseScopeBuildInput MakeScope(
		const TCHAR* Owner = TEXT("Player"),
		const TCHAR* Floor = TEXT("Ground"),
		const FVector2D Origin = FVector2D::ZeroVector,
		const int32 Capacity = SightWeave::SparseAtlas::StandardActiveTileCapacity)
	{
		FSightWeaveSparseScopeBuildInput Scope;
		Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(Owner));
		Scope.FloorId = FSightWeaveFloorId(FName(Floor));
		Scope.FloorOrigin = Origin;
		Scope.PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
		Scope.MaximumActiveTiles = Capacity;
		return Scope;
	}

	FSightWeaveSparsePolygonInput MakeRectangle(
		const int64 StableSourceId,
		const ESightWeaveRenderMaskLayer Layer,
		const FSightWeaveRenderProfileIdentity& Profile,
		const FVector2D Minimum,
		const FVector2D Maximum)
	{
		FSightWeaveSparsePolygonInput Polygon;
		Polygon.StableSourceId = StableSourceId;
		Polygon.SourceRevision = 1;
		Polygon.Layer = Layer;
		Polygon.CompatibilityProfile = Profile;
		Polygon.WorldVertices =
		{
			Minimum,
			FVector2D(Maximum.X, Minimum.Y),
			Maximum,
			FVector2D(Minimum.X, Maximum.Y)
		};
		return Polygon;
	}

	FSightWeaveSparseRenderPacketBuildInput MakePacketInput(const uint64 Revision = 1)
	{
		FSightWeaveSparseRenderPacketBuildInput Input;
		Input.WorldIdentity.Serial = 71;
		Input.PacketRevision = Revision;
		Input.RegistryRevision = Revision;
		Input.PublishedSnapshotRevision = Revision;
		return Input;
	}

	FSightWeaveSparseTileIdentity MakeIdentity(
		const int32 CoordinateX,
		const TCHAR* Owner = TEXT("Player"),
		const TCHAR* Floor = TEXT("Ground"),
		const uint64 WorldSerial = 71)
	{
		FSightWeaveSparseTileIdentity Identity;
		Identity.TileKey.Scope.WorldIdentity.Serial = WorldSerial;
		Identity.TileKey.Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(Owner));
		Identity.TileKey.Scope.FloorId = FSightWeaveFloorId(FName(Floor));
		Identity.TileKey.Scope.PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
		Identity.TileKey.LogicalCoordinate = FIntPoint(CoordinateX, 0);
		Identity.CanonicalProfiles.Add(MakeProfile({ TEXT("Visible") }));
		return Identity;
	}

	const FSightWeaveSparseRenderTile* FindTile(
		const FSightWeaveSparseRenderPacket& Packet,
		const FIntPoint Coordinate)
	{
		return Packet.GetTiles().FindByPredicate([Coordinate](const FSightWeaveSparseRenderTile& Tile)
		{
			return Tile.Identity.TileKey.LogicalCoordinate == Coordinate;
		});
	}

	FSightWeaveSparseRenderPacketBuildResult BuildStrip(const int32 TileCount, const int32 Capacity)
	{
		FSightWeaveSparseRenderPacketBuildInput Input = MakePacketInput();
		FSightWeaveSparseScopeBuildInput Scope = MakeScope(TEXT("Player"), TEXT("Ground"), FVector2D::ZeroVector, Capacity);
		const double Span = SightWeave::SparseAtlas::InteriorTileSize
			* static_cast<double>(SightWeaveCentimetersPerTexel(ESightWeaveRenderPrecisionTier::Standard));
		Scope.Polygons.Add(MakeRectangle(
			1,
			ESightWeaveRenderMaskLayer::Bypass,
			MakeProfile({}),
			FVector2D(1.0, 1.0),
			FVector2D(TileCount * Span, 100.0)));
		Input.Scopes.Add(MoveTemp(Scope));
		return FSightWeaveSparseRenderPacketBuilder::Build(Input);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P2SparseLayoutMappingTest,
	"SightWeave.M3P2.SparseAtlas.LayoutMappingNegativeCoordinates",
	SightWeave::M3P2::SparseAtlasTests::TestFlags)

bool FSightWeaveM3P2SparseLayoutMappingTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P2::SparseAtlasTests;
	using namespace SightWeave::SparseAtlas;
	TestEqual(TEXT("Physical tile is frozen at 256"), PhysicalTileSize, 256);
	TestEqual(TEXT("Interior tile is frozen at 248"), InteriorTileSize, 248);
	TestEqual(TEXT("Gutter is frozen at four texels"), GutterTexels, 4);
	TestEqual(TEXT("Page is frozen at 2048"), PageSize, 2048);
	TestEqual(TEXT("A page has 64 slots"), SlotsPerPage, 64);
	TestEqual(TEXT("PF_G8 page accounting is four MiB"), PageBytes, 4ull * 1024ull * 1024ull);

	const double Span = InteriorTileSize
		* static_cast<double>(SightWeaveCentimetersPerTexel(ESightWeaveRenderPrecisionTier::Standard));
	TestEqual(TEXT("Origin maps to tile zero"),
		FSightWeaveSparseRenderPacketBuilder::WorldToLogicalTile(
			FVector2D::ZeroVector, FVector2D::ZeroVector, ESightWeaveRenderPrecisionTier::Standard),
		FIntPoint::ZeroValue);
	TestEqual(TEXT("Negative epsilon maps with floor semantics"),
		FSightWeaveSparseRenderPacketBuilder::WorldToLogicalTile(
			FVector2D(-0.001, -0.001), FVector2D::ZeroVector, ESightWeaveRenderPrecisionTier::Standard),
		FIntPoint(-1, -1));
	TestEqual(TEXT("Negative boundary is stable"),
		FSightWeaveSparseRenderPacketBuilder::WorldToLogicalTile(
			FVector2D(-Span, -Span), FVector2D::ZeroVector, ESightWeaveRenderPrecisionTier::Standard),
		FIntPoint(-1, -1));
	TestEqual(TEXT("Positive boundary enters the adjacent tile"),
		FSightWeaveSparseRenderPacketBuilder::WorldToLogicalTile(
			FVector2D(Span, Span), FVector2D::ZeroVector, ESightWeaveRenderPrecisionTier::Standard),
		FIntPoint(1, 1));

	const FBox2D Bounds = FSightWeaveSparseRenderPacketBuilder::LogicalTileToPhysicalBounds(
		FIntPoint::ZeroValue, FVector2D::ZeroVector, ESightWeaveRenderPrecisionTier::Standard);
	TestEqual(TEXT("Physical minimum includes four-texel gutter"), Bounds.Min, FVector2D(-40.0, -40.0));
	TestEqual(TEXT("Physical maximum includes four-texel gutter"), Bounds.Max, FVector2D(2520.0, 2520.0));

	const FSightWeaveSparsePhysicalAddress LastFirstPage{ 0, 63 };
	const FSightWeaveSparsePhysicalAddress FirstSecondPage{ 1, 0 };
	TestEqual(TEXT("Last slot uses bottom-right page cell"), LastFirstPage.GetSlotOrigin(), FIntPoint(1792, 1792));
	TestEqual(TEXT("Second page slot origin restarts at zero"), FirstSecondPage.GetSlotOrigin(), FIntPoint::ZeroValue);
	TestEqual(TEXT("Second page begins at linear slot 64"), FirstSecondPage.GetLinearIndex(), 64);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P2SparseResidencyTest,
	"SightWeave.M3P2.Residency.DeterministicAllocationReuseEvictionAndProtection",
	SightWeave::M3P2::SparseAtlasTests::TestFlags)

bool FSightWeaveM3P2SparseResidencyTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P2::SparseAtlasTests;
	FSightWeaveSparseAtlasResidency Residency(2);
	const FSightWeaveSparseTileIdentity A = MakeIdentity(0);
	const FSightWeaveSparseTileIdentity B = MakeIdentity(1);
	const FSightWeaveSparseTileIdentity C = MakeIdentity(2);

	const FSightWeaveSparseResidencyResult AllocatedA = Residency.Acquire(A, 1);
	const FSightWeaveSparseResidencyResult AllocatedB = Residency.Acquire(B, 1);
	TestEqual(TEXT("First allocation uses linear slot zero"), AllocatedA.Address.GetLinearIndex(), 0);
	TestEqual(TEXT("Second allocation uses linear slot one"), AllocatedB.Address.GetLinearIndex(), 1);
	TestEqual(TEXT("Two slots are resident"), Residency.GetResidentCount(), 2);
	TestTrue(TEXT("Fresh slots require a black clear"), AllocatedA.bRequiresBlackClear && AllocatedB.bRequiresBlackClear);
	TestTrue(TEXT("Applied tile records a completed clear"), Residency.MarkApplied(AllocatedA.Address, 1));
	TestFalse(TEXT("Applied tile no longer requires a clear"), Residency.Find(AllocatedA.Address)->bRequiresBlackClear);

	const FSightWeaveSparseResidencyResult ExistingA = Residency.Acquire(A, 2);
	TestEqual(TEXT("Equivalent full identity reuses its existing address"),
		ExistingA.Disposition, ESightWeaveSparseResidencyDisposition::Existing);
	const FSightWeaveSparseResidencyResult ReusedForC = Residency.Acquire(C, 2);
	TestEqual(TEXT("LRU eviction deterministically chooses B after A is touched"),
		ReusedForC.Address.GetLinearIndex(), AllocatedB.Address.GetLinearIndex());
	TestTrue(TEXT("Eviction reports the complete old identity"), ReusedForC.EvictedIdentity.IsEquivalentTo(B));
	TestTrue(TEXT("Reused physical storage must be cleared black"), ReusedForC.bRequiresBlackClear);
	TestEqual(TEXT("One deterministic eviction is counted"), Residency.GetEvictionCount(), uint64(1));

	FSightWeaveSparseAtlasResidency Protected(1);
	const FSightWeaveSparseResidencyResult ProtectedA = Protected.Acquire(A, 1);
	TestTrue(TEXT("Resident slot can be pinned"), Protected.AddPin(ProtectedA.Address));
	const FSightWeaveSparseResidencyResult Failed = Protected.Acquire(B, 2);
	TestEqual(TEXT("All-protected capacity fails closed"),
		Failed.Disposition, ESightWeaveSparseResidencyDisposition::CapacityExceeded);
	TestTrue(TEXT("Pinned identity is retained after failed allocation"), Protected.Find(A) != nullptr);
	TestTrue(TEXT("Pin can be released"), Protected.RemovePin(ProtectedA.Address));
	const FSightWeaveSparseResidencyResult AfterUnpin = Protected.Acquire(B, 2);
	TestEqual(TEXT("Unprotected slot becomes deterministically reusable"),
		AfterUnpin.Disposition, ESightWeaveSparseResidencyDisposition::Reused);

	FSightWeaveSparseTileIdentity CollisionA = MakeIdentity(10);
	FSightWeaveSparseTileIdentity CollisionB = CollisionA;
	CollisionA.CanonicalProfiles[0] = MakeProfile({ TEXT("Visible") });
	CollisionB.CanonicalProfiles[0] = MakeProfile({ TEXT("Infrared") });
	CollisionB.CanonicalProfiles[0].StableHash = CollisionA.CanonicalProfiles[0].StableHash;
	TestFalse(TEXT("Equal profile hashes do not replace full sequence equality"),
		CollisionA.IsEquivalentTo(CollisionB));
	FSightWeaveSparseAtlasResidency CollisionResidency(2);
	const FSightWeaveSparseResidencyResult CollisionResultA = CollisionResidency.Acquire(CollisionA, 1);
	const FSightWeaveSparseResidencyResult CollisionResultB = CollisionResidency.Acquire(CollisionB, 1);
	TestNotEqual(TEXT("Colliding unequal profiles receive isolated physical slots"),
		CollisionResultA.Address.GetLinearIndex(), CollisionResultB.Address.GetLinearIndex());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P2SparseDirtySchedulingTest,
	"SightWeave.M3P2.DirtyScheduling.AddMoveDeleteCoalescingAndNoChange",
	SightWeave::M3P2::SparseAtlasTests::TestFlags)

bool FSightWeaveM3P2SparseDirtySchedulingTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P2::SparseAtlasTests;
	const FSightWeaveRenderProfileIdentity Visible = MakeProfile({ TEXT("Visible") });
	FSightWeaveSparseRenderPacketBuildInput InitialInput = MakePacketInput(1);
	FSightWeaveSparseScopeBuildInput InitialScope = MakeScope();
	InitialScope.Polygons.Add(MakeRectangle(
		1, ESightWeaveRenderMaskLayer::Vision, Visible, FVector2D(100.0, 100.0), FVector2D(500.0, 500.0)));
	InitialScope.Polygons.Add(MakeRectangle(
		2, ESightWeaveRenderMaskLayer::Illumination, Visible, FVector2D(50.0, 50.0), FVector2D(600.0, 600.0)));
	InitialInput.Scopes.Add(InitialScope);
	const FSightWeaveSparseRenderPacketBuildResult Initial =
		FSightWeaveSparseRenderPacketBuilder::Build(InitialInput);
	if (!TestTrue(TEXT("Initial one-tile packet builds"), Initial.Succeeded()))
	{
		return false;
	}
	TestEqual(TEXT("Initial scope redraws one dirty tile"), Initial.DirtyTileCount, 1);
	TestEqual(TEXT("Initial packet owns one tile"), Initial.Packet->GetTiles().Num(), 1);

	FSightWeaveSparseRenderPacketBuildInput NoChangeInput = InitialInput;
	NoChangeInput.PacketRevision = 2;
	NoChangeInput.RegistryRevision = 2;
	NoChangeInput.PublishedSnapshotRevision = 2;
	NoChangeInput.PreviousPacket = Initial.Packet;
	const FSightWeaveSparseRenderPacketBuildResult NoChange =
		FSightWeaveSparseRenderPacketBuilder::Build(NoChangeInput);
	TestTrue(TEXT("Revision-only update remains valid"), NoChange.Succeeded());
	TestFalse(TEXT("No-change produces no mask work"), NoChange.Packet->HasMaskWork());
	TestEqual(TEXT("No-change does not force scope rebuild"), NoChange.FullRebuildScopeCount, 0);

	FSightWeaveSparseRenderPacketBuildInput EditedInput = NoChangeInput;
	EditedInput.PacketRevision = 3;
	EditedInput.RegistryRevision = 3;
	EditedInput.PublishedSnapshotRevision = 3;
	EditedInput.PreviousPacket = NoChange.Packet;
	EditedInput.Scopes[0].Polygons[0].WorldVertices[2] = FVector2D(700.0, 700.0);
	const FSightWeaveSparseRenderPacketBuildResult Edited =
		FSightWeaveSparseRenderPacketBuilder::Build(EditedInput);
	TestEqual(TEXT("One changed tile does not redraw the entire atlas"), Edited.DirtyTileCount, 1);
	TestEqual(TEXT("In-place edit does not remove residency"), Edited.RemovedTileCount, 0);

	FSightWeaveSparseRenderPacketBuildInput MovedInput = MakePacketInput(4);
	FSightWeaveSparseScopeBuildInput MovedScope = MakeScope();
	MovedScope.Polygons.Add(MakeRectangle(
		1, ESightWeaveRenderMaskLayer::Vision, Visible, FVector2D(2600.0, 100.0), FVector2D(3000.0, 500.0)));
	MovedScope.Polygons.Add(MakeRectangle(
		2, ESightWeaveRenderMaskLayer::Illumination, Visible, FVector2D(2550.0, 50.0), FVector2D(3100.0, 600.0)));
	MovedInput.Scopes.Add(MoveTemp(MovedScope));
	MovedInput.PreviousPacket = Edited.Packet;
	const FSightWeaveSparseRenderPacketBuildResult Moved =
		FSightWeaveSparseRenderPacketBuilder::Build(MovedInput);
	TestEqual(TEXT("Move schedules the new tile once"), Moved.DirtyTileCount, 1);
	TestEqual(TEXT("Move schedules the old tile for removal once"), Moved.RemovedTileCount, 1);
	TestTrue(TEXT("Moved packet contains logical tile one"), FindTile(*Moved.Packet, FIntPoint(1, 0)) != nullptr);

	FSightWeaveSparseRenderPacketBuildInput DeletedInput = MakePacketInput(5);
	DeletedInput.Scopes.Add(MakeScope());
	DeletedInput.PreviousPacket = Moved.Packet;
	const FSightWeaveSparseRenderPacketBuildResult Deleted =
		FSightWeaveSparseRenderPacketBuilder::Build(DeletedInput);
	TestEqual(TEXT("Delete schedules no new dirty tile"), Deleted.DirtyTileCount, 0);
	TestEqual(TEXT("Delete schedules the old tile for black removal"), Deleted.RemovedTileCount, 1);
	TestTrue(TEXT("Delete packet contains no active tiles"), Deleted.Packet->GetTiles().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P2SparseMultiTileCapacityTest,
	"SightWeave.M3P2.SparseAtlas.MultiTilePageBoundaryCapacityAndGutter",
	SightWeave::M3P2::SparseAtlasTests::TestFlags)

bool FSightWeaveM3P2SparseMultiTileCapacityTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P2::SparseAtlasTests;
	const FSightWeaveRenderProfileIdentity Visible = MakeProfile({ TEXT("Visible") });
	FSightWeaveSparseRenderPacketBuildInput CrossingInput = MakePacketInput();
	FSightWeaveSparseScopeBuildInput CrossingScope = MakeScope();
	CrossingScope.Polygons.Add(MakeRectangle(
		1, ESightWeaveRenderMaskLayer::Vision, Visible, FVector2D(2400.0, 2400.0), FVector2D(2560.0, 2560.0)));
	CrossingScope.Polygons.Add(MakeRectangle(
		2, ESightWeaveRenderMaskLayer::Illumination, Visible, FVector2D(2350.0, 2350.0), FVector2D(2610.0, 2610.0)));
	CrossingInput.Scopes.Add(MoveTemp(CrossingScope));
	const FSightWeaveSparseRenderPacketBuildResult Crossing =
		FSightWeaveSparseRenderPacketBuilder::Build(CrossingInput);
	if (!TestTrue(TEXT("Four-tile corner packet builds"), Crossing.Succeeded()))
	{
		return false;
	}
	TestEqual(TEXT("Corner overlap allocates four logical tiles"), Crossing.Packet->GetTiles().Num(), 4);
	TestTrue(TEXT("Left-bottom tile exists"), FindTile(*Crossing.Packet, FIntPoint(0, 0)) != nullptr);
	TestTrue(TEXT("Right-bottom tile exists"), FindTile(*Crossing.Packet, FIntPoint(1, 0)) != nullptr);
	TestTrue(TEXT("Left-top tile exists"), FindTile(*Crossing.Packet, FIntPoint(0, 1)) != nullptr);
	TestTrue(TEXT("Right-top tile exists"), FindTile(*Crossing.Packet, FIntPoint(1, 1)) != nullptr);
	for (const FSightWeaveSparseRenderTile& Tile : Crossing.Packet->GetTiles())
	{
		TestEqual(TEXT("Every tile keeps the frozen gutter"),
			static_cast<int32>(FMath::RoundToDouble(
				(Tile.PhysicalWorldBounds.GetSize().X / Tile.CentimetersPerTexel)
				- SightWeave::SparseAtlas::InteriorTileSize)),
			SightWeave::SparseAtlas::GutterTexels * 2);
	}

	for (const int32 TileCount : { 1, 2, 8, 64, 65, 128 })
	{
		const FSightWeaveSparseRenderPacketBuildResult Strip = BuildStrip(TileCount, 128);
		if (!TestTrue(*FString::Printf(TEXT("%d-tile capacity sample builds"), TileCount), Strip.Succeeded()))
		{
			return false;
		}
		TestEqual(*FString::Printf(TEXT("%d-tile sample has exact logical count"), TileCount),
			Strip.Packet->GetTiles().Num(), TileCount);
	}
	const FSightWeaveSparseRenderPacketBuildResult CapacityPlusOne = BuildStrip(129, 128);
	TestTrue(TEXT("Capacity plus one returns a valid fail-black packet"), CapacityPlusOne.Succeeded());
	TestEqual(TEXT("Capacity plus one marks exactly one failed scope"), CapacityPlusOne.FailedScopeCount, 1);
	TestEqual(TEXT("Capacity plus one records bounded failure"),
		CapacityPlusOne.Packet->GetScopes()[0].Failure, ESightWeaveSparsePacketFailure::CapacityExceeded);
	TestTrue(TEXT("Failed scope publishes no white-capable tile geometry"), CapacityPlusOne.Packet->GetTiles().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P2SparseScopeIsolationTest,
	"SightWeave.M3P2.Scope.WorldOwnerFloorProfileIsolationAndHashCollision",
	SightWeave::M3P2::SparseAtlasTests::TestFlags)

bool FSightWeaveM3P2SparseScopeIsolationTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P2::SparseAtlasTests;
	const FSightWeaveRenderProfileIdentity Visible = MakeProfile({ TEXT("Visible") });
	FSightWeaveRenderProfileIdentity Infrared = MakeProfile({ TEXT("Infrared") });
	Infrared.StableHash = Visible.StableHash;

	FSightWeaveSparseRenderPacketBuildInput Input = MakePacketInput();
	FSightWeaveSparseScopeBuildInput PlayerGround = MakeScope(TEXT("Player"), TEXT("Ground"));
	PlayerGround.Polygons.Add(MakeRectangle(
		1, ESightWeaveRenderMaskLayer::Vision, Visible, FVector2D(10.0, 10.0), FVector2D(200.0, 200.0)));
	PlayerGround.Polygons.Add(MakeRectangle(
		2, ESightWeaveRenderMaskLayer::Illumination, Visible, FVector2D(10.0, 10.0), FVector2D(200.0, 200.0)));
	PlayerGround.Polygons.Add(MakeRectangle(
		3, ESightWeaveRenderMaskLayer::Vision, Infrared, FVector2D(20.0, 20.0), FVector2D(210.0, 210.0)));
	PlayerGround.Polygons.Add(MakeRectangle(
		4, ESightWeaveRenderMaskLayer::Illumination, Infrared, FVector2D(20.0, 20.0), FVector2D(210.0, 210.0)));
	Input.Scopes.Add(MoveTemp(PlayerGround));

	FSightWeaveSparseScopeBuildInput OtherOwner = MakeScope(TEXT("Drone"), TEXT("Ground"));
	OtherOwner.Polygons.Add(MakeRectangle(
		5, ESightWeaveRenderMaskLayer::Bypass, MakeProfile({}), FVector2D(10.0, 10.0), FVector2D(200.0, 200.0)));
	Input.Scopes.Add(MoveTemp(OtherOwner));
	FSightWeaveSparseScopeBuildInput OtherFloor = MakeScope(TEXT("Player"), TEXT("Basement"));
	OtherFloor.Polygons.Add(MakeRectangle(
		6, ESightWeaveRenderMaskLayer::Bypass, MakeProfile({}), FVector2D(10.0, 10.0), FVector2D(200.0, 200.0)));
	Input.Scopes.Add(MoveTemp(OtherFloor));

	const FSightWeaveSparseRenderPacketBuildResult Result =
		FSightWeaveSparseRenderPacketBuilder::Build(Input);
	if (!TestTrue(TEXT("Multi-owner/floor/profile packet builds"), Result.Succeeded()))
	{
		return false;
	}
	TestEqual(TEXT("Three isolated scopes produce three tiles"), Result.Packet->GetTiles().Num(), 3);
	const FSightWeaveSparseRenderTile* ProfileTile = Result.Packet->GetTiles().FindByPredicate(
		[](const FSightWeaveSparseRenderTile& Tile)
		{
			return Tile.Identity.TileKey.Scope.KnowledgeOwnerId
				== FSightWeaveKnowledgeOwnerId(FName(TEXT("Player")))
				&& Tile.Identity.TileKey.Scope.FloorId == FSightWeaveFloorId(FName(TEXT("Ground")));
		});
	if (!TestNotNull(TEXT("Player/Ground tile remains isolated"), ProfileTile))
	{
		return false;
	}
	TestEqual(TEXT("Hash-colliding Visible and Infrared remain two profiles"),
		ProfileTile->Profiles.Num(), 2);
	TestFalse(TEXT("Colliding profile sequences remain unequal"),
		ProfileTile->Profiles[0].Identity.IsEquivalentTo(ProfileTile->Profiles[1].Identity));

	FSightWeaveSparseRenderPacketBuildInput OtherWorld = Input;
	OtherWorld.WorldIdentity.Serial = 72;
	OtherWorld.PacketRevision = 2;
	OtherWorld.RegistryRevision = 2;
	OtherWorld.PublishedSnapshotRevision = 2;
	OtherWorld.PreviousPacket = Result.Packet;
	const FSightWeaveSparseRenderPacketBuildResult RejectedPrevious =
		FSightWeaveSparseRenderPacketBuilder::Build(OtherWorld);
	TestFalse(TEXT("Previous packet from another world lifetime is rejected"), RejectedPrevious.Succeeded());
	TestEqual(TEXT("World mismatch is classified explicitly"),
		RejectedPrevious.Failure, ESightWeaveSparsePacketFailure::InvalidPreviousPacket);
	return true;
}

#endif
