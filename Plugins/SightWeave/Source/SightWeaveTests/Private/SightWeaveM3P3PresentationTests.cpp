#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SightWeavePresentation.h"

namespace SightWeave::M3P3::PresentationTests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FSightWeaveRenderProfileIdentity Profile(std::initializer_list<const TCHAR*> Capabilities)
	{
		FSightWeaveIlluminationCompatibilityProfile Source;
		for (const TCHAR* Capability : Capabilities)
		{
			Source.AcceptedCapabilities.Add(FName(Capability));
		}
		return FSightWeaveRenderProfileIdentity::FromProfile(Source);
	}

	FSightWeaveSparsePolygonInput Rectangle(
		const int64 StableId,
		const ESightWeaveRenderMaskLayer Layer,
		const FSightWeaveRenderProfileIdentity& CompatibilityProfile,
		const double Offset)
	{
		FSightWeaveSparsePolygonInput Polygon;
		Polygon.StableSourceId = StableId;
		Polygon.SourceRevision = 1;
		Polygon.Layer = Layer;
		Polygon.CompatibilityProfile = CompatibilityProfile;
		Polygon.WorldVertices = {
			FVector2D(100.0 + Offset, 100.0),
			FVector2D(800.0 + Offset, 100.0),
			FVector2D(800.0 + Offset, 800.0),
			FVector2D(100.0 + Offset, 800.0)
		};
		return Polygon;
	}

	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> BuildPacket(
		FAutomationTestBase& Test,
		FSightWeaveRenderProfileIdentity Visible,
		FSightWeaveRenderProfileIdentity Infrared)
	{
		FSightWeaveSparseRenderPacketBuildInput Input;
		Input.WorldIdentity.Serial = 3301;
		Input.PacketRevision = 17;
		Input.RegistryRevision = 27;
		Input.PublishedSnapshotRevision = 37;
		FSightWeaveSparseScopeBuildInput& Scope = Input.Scopes.AddDefaulted_GetRef();
		Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("OwnerA")));
		Scope.FloorId = FSightWeaveFloorId(FName(TEXT("FloorA")));
		Scope.PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
		Scope.MaximumActiveTiles = 128;
		Scope.Polygons.Add(Rectangle(1, ESightWeaveRenderMaskLayer::Vision, Visible, 0.0));
		Scope.Polygons.Add(Rectangle(2, ESightWeaveRenderMaskLayer::Illumination, Visible, 0.0));
		Scope.Polygons.Add(Rectangle(3, ESightWeaveRenderMaskLayer::Vision, Infrared, 900.0));
		Scope.Polygons.Add(Rectangle(4, ESightWeaveRenderMaskLayer::Illumination, Infrared, 900.0));
		const FSightWeaveSparseRenderPacketBuildResult Built =
			FSightWeaveSparseRenderPacketBuilder::Build(Input);
		Test.TestTrue(TEXT("M3.3 presentation fixture builds"), Built.Succeeded());
		return Built.Packet;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P3PresentationBindingContractTest,
	"SightWeave.M3P3.Presentation.BindingContract",
	SightWeave::M3P3::PresentationTests::TestFlags)

bool FSightWeaveM3P3PresentationBindingContractTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P3::PresentationTests;
	FSightWeaveRenderProfileIdentity Visible = Profile({ TEXT("Visible") });
	FSightWeaveRenderProfileIdentity Infrared = Profile({ TEXT("Infrared") });
	Infrared.StableHash = Visible.StableHash;
	const TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet =
		BuildPacket(*this, Visible, Infrared);
	if (!Packet.IsValid())
	{
		return false;
	}

	FSightWeaveRenderWorldIdentity World;
	World.Serial = 3301;
	const FSightWeaveViewPresentationSelection Selection =
		FSightWeaveViewPresentationSelection::Enabled(
			World,
			FSightWeaveKnowledgeOwnerId(FName(TEXT("OwnerA"))),
			FSightWeaveFloorId(FName(TEXT("FloorA"))),
			ESightWeaveRenderPrecisionTier::Standard,
			47);
	TestTrue(TEXT("Enabled selection is valid"), Selection.IsValid());
	const FSightWeavePresentationBindingBuildResult Built =
		FSightWeavePresentationBindingBuilder::Build(*Packet, Selection, 57, 67);
	if (!TestTrue(TEXT("Matching immutable presentation binding builds"), Built.Succeeded()))
	{
		return false;
	}
	const FSightWeaveViewPresentationBinding& Binding = *Built.Binding;
	TestTrue(TEXT("Binding identifies completed EffectiveLiveMask union"),
		Binding.IsEffectiveUnionScope());
	TestEqual(TEXT("Binding preserves packet revision"), Binding.GetPacketRevision(), uint64(17));
	TestEqual(TEXT("Binding preserves registry revision"), Binding.GetRegistryRevision(), uint64(27));
	TestEqual(TEXT("Binding preserves snapshot revision"),
		Binding.GetPublishedSnapshotRevision(), uint64(37));
	TestEqual(TEXT("Binding preserves presentation revision"),
		Binding.GetPresentationRevision(), uint64(47));
	TestEqual(TEXT("Binding preserves resource generation"),
		Binding.GetResourceGeneration(), uint64(57));
	TestEqual(TEXT("Binding preserves residency generation"),
		Binding.GetResidencyGeneration(), uint64(67));
	TestEqual(TEXT("Hash-colliding complete profiles remain separate"),
		Binding.GetCanonicalProfiles().Num(), 2);
	TestFalse(TEXT("Hash collision never defines profile equality"),
		Binding.GetCanonicalProfiles()[0].IsEquivalentTo(Binding.GetCanonicalProfiles()[1]));

	const FSightWeaveViewPresentationSelection Disabled =
		FSightWeaveViewPresentationSelection::Disabled(World, 48);
	const FSightWeavePresentationBindingBuildResult DisabledResult =
		FSightWeavePresentationBindingBuilder::Build(*Packet, Disabled, 57, 67);
	TestEqual(TEXT("Disabled presentation is distinct from invalid resources"),
		DisabledResult.Failure, ESightWeavePresentationBindingFailure::Disabled);

	FSightWeaveRenderWorldIdentity OtherWorld;
	OtherWorld.Serial = 3302;
	const FSightWeavePresentationBindingBuildResult WrongWorld =
		FSightWeavePresentationBindingBuilder::Build(
			*Packet,
			FSightWeaveViewPresentationSelection::Enabled(
				OtherWorld,
				FSightWeaveKnowledgeOwnerId(FName(TEXT("OwnerA"))),
				FSightWeaveFloorId(FName(TEXT("FloorA"))),
				ESightWeaveRenderPrecisionTier::Standard,
				49),
			57,
			67);
	TestEqual(TEXT("Old or other world selection fails closed"),
		WrongWorld.Failure, ESightWeavePresentationBindingFailure::WorldMismatch);

	const FSightWeavePresentationBindingBuildResult WrongOwner =
		FSightWeavePresentationBindingBuilder::Build(
			*Packet,
			FSightWeaveViewPresentationSelection::Enabled(
				World,
				FSightWeaveKnowledgeOwnerId(FName(TEXT("OwnerB"))),
				FSightWeaveFloorId(FName(TEXT("FloorA"))),
				ESightWeaveRenderPrecisionTier::Standard,
				50),
			57,
			67);
	TestEqual(TEXT("Other owner cannot borrow the resident scope"),
		WrongOwner.Failure, ESightWeavePresentationBindingFailure::ScopeMissing);
	TestEqual(TEXT("Missing resource generation fails closed"),
		FSightWeavePresentationBindingBuilder::Build(*Packet, Selection, 0, 67).Failure,
		ESightWeavePresentationBindingFailure::ResourceGenerationMismatch);
	TestEqual(TEXT("Missing residency generation fails closed"),
		FSightWeavePresentationBindingBuilder::Build(*Packet, Selection, 57, 0).Failure,
		ESightWeavePresentationBindingFailure::ResidencyGenerationMismatch);
	return true;
}

#endif
