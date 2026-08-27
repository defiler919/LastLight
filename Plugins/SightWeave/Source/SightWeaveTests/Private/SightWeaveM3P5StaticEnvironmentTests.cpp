#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SightWeaveStaticEnvironment.h"

namespace SightWeaveM3P5StaticEnvironmentTests
{
	FSightWeaveMemoryScopeKey MakeScope(const uint64 WorldSerial = 501)
	{
		FSightWeaveMemoryScopeKey Scope;
		Scope.WorldIdentity = FSightWeaveRenderWorldIdentity { WorldSerial };
		Scope.WorldGeneration = WorldSerial;
		Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("Local")));
		Scope.FloorId = FSightWeaveFloorId(FName(TEXT("Ground")));
		Scope.FloorOrigin = FVector2D::ZeroVector;
		Scope.FloorPlaneZ = 0.0f;
		Scope.PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
		return Scope;
	}

	FSightWeaveStaticEnvironmentDescription MakeDescription(
		const uint8 Intensity,
		const FVector2D Minimum,
		const FVector2D Maximum)
	{
		FSightWeaveStaticEnvironmentDescription Description;
		Description.KnowledgeOwnerId =
			FSightWeaveKnowledgeOwnerId(FName(TEXT("Local")));
		Description.FloorId = FSightWeaveFloorId(FName(TEXT("Ground")));
		Description.HeightRange = { -10.0f, 300.0f };
		Description.WorldFootprint = {
			Minimum,
			FVector2D(Maximum.X, Minimum.Y),
			Maximum,
			FVector2D(Minimum.X, Maximum.Y)
		};
		Description.NeutralIntensity = Intensity;
		Description.bExplicitlyImmutable = true;
		return Description;
	}

	uint8 SamplePacket(
		const FSightWeaveStaticEnvironmentPacket& Packet,
		const FVector2D WorldLocation)
	{
		FIntPoint Coordinate;
		FIntPoint Texel;
		if (!FSightWeaveMemoryAuthority::WorldToTileAndTexel(
				Packet.GetScope(), WorldLocation, Coordinate, Texel))
		{
			return 0;
		}
		const FSightWeaveStaticEnvironmentTile* Tile =
			Packet.GetTiles().FindByPredicate(
				[Coordinate](const FSightWeaveStaticEnvironmentTile& Candidate)
				{
					return Candidate.Key.LogicalCoordinate == Coordinate;
				});
		return Tile ? Tile->Sample(Texel) : 0;
	}
}

using namespace SightWeaveM3P5StaticEnvironmentTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P5StaticEnvironmentAuthorityTest,
	"SightWeave.M3P5.Memory.StaticEnvironment.ExplicitEligibilityDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSightWeaveM3P5StaticEnvironmentAuthorityTest::RunTest(const FString& Parameters)
{
	FSightWeaveStaticEnvironmentAuthority Authority;
	TestTrue(TEXT("Static authority configures exact memory scope"), Authority.Configure(MakeScope(), 128));

	FSightWeaveStaticEnvironmentDescription Uncertain =
		MakeDescription(80, FVector2D(-200.0, -200.0), FVector2D(2800.0, 2800.0));
	Uncertain.bExplicitlyImmutable = false;
	TestFalse(
		TEXT("Uncertain environment cannot be registered"),
		Authority.Register(Uncertain).IsValid());

	const FSightWeaveStaticEnvironmentHandle Base = Authority.Register(
		MakeDescription(80, FVector2D(-200.0, -200.0), FVector2D(2800.0, 2800.0)));
	const FSightWeaveStaticEnvironmentHandle Detail = Authority.Register(
		MakeDescription(144, FVector2D(100.0, 100.0), FVector2D(500.0, 500.0)));
	TestTrue(TEXT("Explicit base registers"), Base.IsValid());
	TestTrue(TEXT("Explicit detail registers"), Detail.IsValid());
	const TSharedPtr<const FSightWeaveStaticEnvironmentPacket, ESPMode::ThreadSafe> Packet =
		Authority.PublishPacket();
	TestTrue(TEXT("Static packet valid"), Packet.IsValid() && Packet->IsValid());
	TestEqual(TEXT("Base neutral cue sampled"), SamplePacket(*Packet, FVector2D(1000.0, 1000.0)), uint8(80));
	TestEqual(TEXT("Overlap uses order-independent max"), SamplePacket(*Packet, FVector2D(200.0, 200.0)), uint8(144));
	TestEqual(TEXT("Negative logical coordinate sampled"), SamplePacket(*Packet, FVector2D(-100.0, -100.0)), uint8(80));
	TestEqual(TEXT("Outside explicit footprint fails black"), SamplePacket(*Packet, FVector2D(5000.0, 5000.0)), uint8(0));

	const uint64 StableRevision = Authority.GetEligibilityRevision();
	TestTrue(TEXT("Equivalent update succeeds"), Authority.Update(Base,
		MakeDescription(80, FVector2D(-200.0, -200.0), FVector2D(2800.0, 2800.0))));
	TestEqual(TEXT("Equivalent update advances no revision"), Authority.GetEligibilityRevision(), StableRevision);
	TestTrue(TEXT("Teardown removes detail"), Authority.Unregister(Detail));
	const TSharedPtr<const FSightWeaveStaticEnvironmentPacket, ESPMode::ThreadSafe> Removed =
		Authority.PublishPacket();
	TestEqual(TEXT("Removed detail leaves base cue"), SamplePacket(*Removed, FVector2D(200.0, 200.0)), uint8(80));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P5StaticEnvironmentIsolationTest,
	"SightWeave.M3P5.Memory.StaticEnvironment.ScopeAndHeightIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSightWeaveM3P5StaticEnvironmentIsolationTest::RunTest(const FString& Parameters)
{
	FSightWeaveStaticEnvironmentAuthority Authority;
	TestTrue(TEXT("Configures"), Authority.Configure(MakeScope(777), 128));
	FSightWeaveStaticEnvironmentDescription WrongOwner =
		MakeDescription(100, FVector2D::ZeroVector, FVector2D(1000.0, 1000.0));
	WrongOwner.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("Remote")));
	TestTrue(TEXT("Valid but mismatched authored record registers"), Authority.Register(WrongOwner).IsValid());
	FSightWeaveStaticEnvironmentDescription WrongHeight =
		MakeDescription(100, FVector2D::ZeroVector, FVector2D(1000.0, 1000.0));
	WrongHeight.HeightRange = { 100.0f, 200.0f };
	TestTrue(TEXT("Height-mismatched record registers"), Authority.Register(WrongHeight).IsValid());
	const TSharedPtr<const FSightWeaveStaticEnvironmentPacket, ESPMode::ThreadSafe> Packet =
		Authority.PublishPacket();
	TestEqual(TEXT("Owner and floor-plane height mismatch contribute zero tiles"), Packet->GetTiles().Num(), 0);
	Authority.Reset();
	TestFalse(TEXT("World teardown clears handles"), Authority.IsHandleValid(FSightWeaveStaticEnvironmentHandle(1)));
	return true;
}

#endif
