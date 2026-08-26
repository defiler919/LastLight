#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SightWeaveRenderPacket.h"

#include <limits>

namespace SightWeave::M3P1::PacketTests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FSightWeaveRenderPacketBuildInput MakeInput(const uint64 Revision = 1, const uint64 WorldSerial = 1)
	{
		FSightWeaveRenderPacketBuildInput Input;
		Input.WorldIdentity.Serial = WorldSerial;
		Input.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("Player")));
		Input.FloorId = FSightWeaveFloorId(FName(TEXT("Ground")));
		FSightWeaveIlluminationCompatibilityProfile Profile;
		Profile.AcceptedCapabilities = { FName(TEXT("Visible")), FName(TEXT("Thermal")) };
		Input.CompatibilityProfile = FSightWeaveRenderProfileIdentity::FromProfile(Profile);
		Input.PacketRevision = Revision;
		Input.RegistryRevision = 7;
		Input.PublishedSnapshotRevision = 11;
		Input.PhysicalWorldBounds = FBox2D(FVector2D::ZeroVector, FVector2D(2560.0, 2560.0));
		Input.DirtyReason = ESightWeaveRenderDirtyReason::SourceChanged;
		return Input;
	}

	FSightWeaveRenderPolygonInput MakePolygon(
		const FSightWeaveRenderPacketBuildInput& Input,
		const int64 StableId,
		const ESightWeaveRenderMaskLayer Layer,
		std::initializer_list<FVector2D> Vertices)
	{
		FSightWeaveRenderPolygonInput Polygon;
		Polygon.StableSourceId = StableId;
		Polygon.Layer = Layer;
		Polygon.KnowledgeOwnerId = Input.KnowledgeOwnerId;
		Polygon.FloorId = Input.FloorId;
		Polygon.CompatibilityProfile = Input.CompatibilityProfile;
		for (const FVector2D& Vertex : Vertices)
		{
			Polygon.WorldVertices.Add(Vertex);
		}
		return Polygon;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P1PacketValidationTest,
	"SightWeave.M3P1.Packet.DefaultInvalidAndEmptyBlack",
	SightWeave::M3P1::PacketTests::TestFlags)

bool FSightWeaveM3P1PacketValidationTest::RunTest(const FString& Parameters)
{
	FSightWeaveRenderPacketBuildResult Invalid =
		FSightWeaveRenderPacketBuilder::Build(FSightWeaveRenderPacketBuildInput());
	TestFalse(TEXT("Default packet input fails closed"), Invalid.Succeeded());
	TestEqual(TEXT("Default failure is world identity"), Invalid.Failure,
		ESightWeaveRenderPacketFailure::InvalidWorldIdentity);

	const FSightWeaveRenderPacketBuildResult Empty = FSightWeaveRenderPacketBuilder::Build(
		SightWeave::M3P1::PacketTests::MakeInput());
	if (!TestTrue(TEXT("A scoped empty packet is a valid black clear"), Empty.Succeeded()))
	{
		return false;
	}
	TestEqual(TEXT("Empty packet has no vertices"), Empty.Packet->GetVertices().Num(), 0);
	TestEqual(TEXT("Empty packet has no indices"), Empty.Packet->GetIndices().Num(), 0);
	TestNotEqual(TEXT("Valid empty packet is deterministically hashed"), Empty.Packet->GetContentHash(), uint64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P1DeterministicTriangulationTest,
	"SightWeave.M3P1.Triangulation.ConvexWindingDuplicateCollinearDeterminism",
	SightWeave::M3P1::PacketTests::TestFlags)

bool FSightWeaveM3P1DeterministicTriangulationTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P1::PacketTests;
	FSightWeaveRenderPacketBuildInput Canonical = MakeInput();
	Canonical.Polygons.Add(MakePolygon(Canonical, 9, ESightWeaveRenderMaskLayer::Vision,
		{ FVector2D(100.0, 100.0), FVector2D(500.0, 100.0), FVector2D(500.0, 500.0), FVector2D(100.0, 500.0) }));
	FSightWeaveRenderPacketBuildInput Reversed = MakeInput();
	Reversed.Polygons.Add(MakePolygon(Reversed, 9, ESightWeaveRenderMaskLayer::Vision,
		{ FVector2D(500.0, 100.0), FVector2D(100.0, 100.0), FVector2D(100.0, 500.0),
			FVector2D(500.0, 500.0), FVector2D(500.0, 300.0), FVector2D(500.0, 100.0) }));

	const FSightWeaveRenderPacketBuildResult A = FSightWeaveRenderPacketBuilder::Build(Canonical);
	const FSightWeaveRenderPacketBuildResult B = FSightWeaveRenderPacketBuilder::Build(Reversed);
	if (!TestTrue(TEXT("Canonical convex polygon triangulates"), A.Succeeded())
		|| !TestTrue(TEXT("Reversed duplicate/collinear polygon triangulates"), B.Succeeded()))
	{
		return false;
	}
	TestEqual(TEXT("A quad emits two triangles"), A.Packet->GetRange(ESightWeaveRenderMaskLayer::Vision).GetTriangleCount(), uint32(2));
	TestEqual(TEXT("Canonical serialization hash ignores winding/start/redundant vertices"),
		A.Packet->GetContentHash(), B.Packet->GetContentHash());
	TestTrue(TEXT("Redundant input records removals"),
		B.RemovedDuplicateVertexCount > 0 && B.RemovedCollinearVertexCount > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P1ConcaveTriangulationTest,
	"SightWeave.M3P1.Triangulation.ConcaveEarClipping",
	SightWeave::M3P1::PacketTests::TestFlags)

bool FSightWeaveM3P1ConcaveTriangulationTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P1::PacketTests;
	FSightWeaveRenderPacketBuildInput Input = MakeInput();
	Input.Polygons.Add(MakePolygon(Input, 1, ESightWeaveRenderMaskLayer::Bypass,
		{ FVector2D(100.0, 100.0), FVector2D(600.0, 100.0), FVector2D(600.0, 600.0),
			FVector2D(350.0, 350.0), FVector2D(100.0, 600.0) }));
	const FSightWeaveRenderPacketBuildResult Result = FSightWeaveRenderPacketBuilder::Build(Input);
	if (!TestTrue(TEXT("Concave simple polygon triangulates"), Result.Succeeded()))
	{
		return false;
	}
	TestEqual(TEXT("Five concave vertices emit three triangles"),
		Result.Packet->GetRange(ESightWeaveRenderMaskLayer::Bypass).GetTriangleCount(), uint32(3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P1InvalidTriangulationTest,
	"SightWeave.M3P1.Triangulation.InvalidInputsFailClosed",
	SightWeave::M3P1::PacketTests::TestFlags)

bool FSightWeaveM3P1InvalidTriangulationTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P1::PacketTests;
	auto FailureFor = [](std::initializer_list<FVector2D> Vertices)
	{
		FSightWeaveRenderPacketBuildInput Input = MakeInput();
		Input.Polygons.Add(MakePolygon(Input, 1, ESightWeaveRenderMaskLayer::Vision, Vertices));
		return FSightWeaveRenderPacketBuilder::Build(Input).Failure;
	};
	TestEqual(TEXT("Empty polygon is degenerate"), FailureFor({}),
		ESightWeaveRenderPacketFailure::DegeneratePolygon);
	TestEqual(TEXT("Zero-area polygon is degenerate"), FailureFor(
		{ FVector2D(0.0, 0.0), FVector2D(100.0, 0.0), FVector2D(200.0, 0.0) }),
		ESightWeaveRenderPacketFailure::DegeneratePolygon);
	TestEqual(TEXT("Self-intersection is rejected"), FailureFor(
		{ FVector2D(0.0, 0.0), FVector2D(200.0, 200.0), FVector2D(0.0, 200.0), FVector2D(200.0, 0.0) }),
		ESightWeaveRenderPacketFailure::DegeneratePolygon);
	TestEqual(TEXT("NaN is rejected"), FailureFor(
		{ FVector2D(0.0, 0.0), FVector2D(std::numeric_limits<double>::quiet_NaN(), 1.0), FVector2D(0.0, 1.0) }),
		ESightWeaveRenderPacketFailure::NonFiniteVertex);
	TestEqual(TEXT("Infinity is rejected"), FailureFor(
		{ FVector2D(0.0, 0.0), FVector2D(std::numeric_limits<double>::infinity(), 1.0), FVector2D(0.0, 1.0) }),
		ESightWeaveRenderPacketFailure::NonFiniteVertex);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P1TileAndScopeTest,
	"SightWeave.M3P1.Packet.TileBoundaryGutterOutsideAndScope",
	SightWeave::M3P1::PacketTests::TestFlags)

bool FSightWeaveM3P1TileAndScopeTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P1::PacketTests;
	FSightWeaveRenderPacketBuildInput Input = MakeInput();
	Input.Polygons.Add(MakePolygon(Input, 1, ESightWeaveRenderMaskLayer::Vision,
		{ FVector2D(-100.0, 20.0), FVector2D(80.0, 20.0), FVector2D(80.0, 80.0), FVector2D(-100.0, 80.0) }));
	Input.Polygons.Add(MakePolygon(Input, 2, ESightWeaveRenderMaskLayer::Vision,
		{ FVector2D(3000.0, 3000.0), FVector2D(3100.0, 3000.0), FVector2D(3000.0, 3100.0) }));
	const FSightWeaveRenderPacketBuildResult Crossing = FSightWeaveRenderPacketBuilder::Build(Input);
	TestTrue(TEXT("Boundary/gutter crossing polygon remains rasterizable"), Crossing.Succeeded());
	TestEqual(TEXT("Fully outside polygon is omitted"), Crossing.OutsidePolygonCount, 1);
	TestEqual(TEXT("Only crossing polygon contributes"), Crossing.AcceptedPolygonCount, 1);

	FSightWeaveRenderPacketBuildInput WrongOwner = MakeInput();
	WrongOwner.Polygons.Add(MakePolygon(WrongOwner, 1, ESightWeaveRenderMaskLayer::Vision,
		{ FVector2D(0.0, 0.0), FVector2D(100.0, 0.0), FVector2D(0.0, 100.0) }));
	WrongOwner.Polygons[0].KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("Other")));
	TestEqual(TEXT("Owner mismatch fails closed"), FSightWeaveRenderPacketBuilder::Build(WrongOwner).Failure,
		ESightWeaveRenderPacketFailure::ScopeMismatch);
	WrongOwner.Polygons[0].KnowledgeOwnerId = WrongOwner.KnowledgeOwnerId;
	WrongOwner.Polygons[0].CompatibilityProfile.StableHash ^= 1;
	TestEqual(TEXT("Profile mismatch fails closed"), FSightWeaveRenderPacketBuilder::Build(WrongOwner).Failure,
		ESightWeaveRenderPacketFailure::ProfileMismatch);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P1RevisionLifecycleTest,
	"SightWeave.M3P1.Lifecycle.RevisionAndWorldIsolation",
	SightWeave::M3P1::PacketTests::TestFlags)

bool FSightWeaveM3P1RevisionLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P1::PacketTests;
	const FSightWeaveRenderPacketBuildResult Revision2 =
		FSightWeaveRenderPacketBuilder::Build(MakeInput(2, 41));
	const FSightWeaveRenderPacketBuildResult Revision1 =
		FSightWeaveRenderPacketBuilder::Build(MakeInput(1, 41));
	const FSightWeaveRenderPacketBuildResult OtherWorld =
		FSightWeaveRenderPacketBuilder::Build(MakeInput(3, 42));
	if (!Revision2.Succeeded() || !Revision1.Succeeded() || !OtherWorld.Succeeded())
	{
		AddError(TEXT("Revision test setup packet failed"));
		return false;
	}

	FSightWeaveRenderPacketRevisionGate Gate(FSightWeaveRenderWorldIdentity{ 41 });
	TestEqual(TEXT("First packet is accepted"), Gate.ClassifyAndCommit(*Revision2.Packet),
		ESightWeaveRenderPacketDisposition::Accepted);
	TestEqual(TEXT("Same packet is duplicate"), Gate.ClassifyAndCommit(*Revision2.Packet),
		ESightWeaveRenderPacketDisposition::Duplicate);
	TestEqual(TEXT("Older packet is stale"), Gate.ClassifyAndCommit(*Revision1.Packet),
		ESightWeaveRenderPacketDisposition::Stale);
	TestEqual(TEXT("Other world is isolated"), Gate.ClassifyAndCommit(*OtherWorld.Packet),
		ESightWeaveRenderPacketDisposition::WorldMismatch);
	Gate.Reset();
	TestEqual(TEXT("World restart resets accepted revision"), Gate.GetAcceptedRevision(), uint64(0));
	TestEqual(TEXT("Formerly stale revision can enter a new lifetime"), Gate.ClassifyAndCommit(*Revision1.Packet),
		ESightWeaveRenderPacketDisposition::Accepted);
	return true;
}

#endif
