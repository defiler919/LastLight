#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SightWeavePresentation.h"

namespace SightWeave::M3P4::PresentationTests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FSightWeaveRenderProfileIdentity VisibleProfile()
	{
		FSightWeaveIlluminationCompatibilityProfile Source;
		Source.AcceptedCapabilities.Add(FName(TEXT("Visible")));
		return FSightWeaveRenderProfileIdentity::FromProfile(Source);
	}

	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> BuildPacket(
		FAutomationTestBase& Test)
	{
		const FSightWeaveRenderProfileIdentity Profile = VisibleProfile();
		FSightWeaveSparseRenderPacketBuildInput Input;
		Input.WorldIdentity.Serial = 34001;
		Input.PacketRevision = 11;
		Input.RegistryRevision = 12;
		Input.PublishedSnapshotRevision = 13;
		FSightWeaveSparseScopeBuildInput& Scope = Input.Scopes.AddDefaulted_GetRef();
		Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("FeatherOwner")));
		Scope.FloorId = FSightWeaveFloorId(FName(TEXT("FeatherFloor")));
		Scope.PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
		Scope.MaximumActiveTiles = 4;
		for (const ESightWeaveRenderMaskLayer Layer : {
			ESightWeaveRenderMaskLayer::Vision,
			ESightWeaveRenderMaskLayer::Illumination })
		{
			FSightWeaveSparsePolygonInput& Polygon = Scope.Polygons.AddDefaulted_GetRef();
			Polygon.StableSourceId = Scope.Polygons.Num();
			Polygon.SourceRevision = 1;
			Polygon.Layer = Layer;
			Polygon.CompatibilityProfile = Profile;
			Polygon.WorldVertices = {
				FVector2D(0.0, 0.0), FVector2D(1000.0, 0.0),
				FVector2D(1000.0, 1000.0), FVector2D(0.0, 1000.0)
			};
		}
		const FSightWeaveSparseRenderPacketBuildResult Built =
			FSightWeaveSparseRenderPacketBuilder::Build(Input);
		Test.TestTrue(TEXT("M3.4 binding packet builds"), Built.Succeeded());
		return Built.Packet;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P4SafetyContractTest,
	"SightWeave.M3P4.Presentation.SafetyContract",
	SightWeave::M3P4::PresentationTests::TestFlags)

bool FSightWeaveM3P4SafetyContractTest::RunTest(const FString& Parameters)
{
	FSightWeaveVisualFeatherSettings Settings;
	TestTrue(TEXT("Zero width is valid"), Settings.IsValid());
	TestFalse(TEXT("Zero width is disabled"), Settings.IsEnabled());
	for (const float Width : { 10.0f, 25.0f, 50.0f, 100.0f })
	{
		Settings.WidthCentimeters = Width;
		TestTrue(*FString::Printf(TEXT("Width %.0f cm is valid"), Width), Settings.IsValid());
		TestTrue(TEXT("Positive width enables visual feather"), Settings.IsEnabled());
	}
	Settings.WidthCentimeters = -1.0f;
	TestFalse(TEXT("Negative width is rejected"), Settings.IsValid());
	Settings.WidthCentimeters = 100.01f;
	TestFalse(TEXT("Width above the frozen maximum is rejected"), Settings.IsValid());
	Settings.WidthCentimeters = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Non-finite width is rejected"), Settings.IsValid());

	TestEqual(TEXT("Hard zero always composes exact zero"),
		FSightWeaveVisualFeatherMath::ComposeWeight(false, 1.0f), 0.0f);
	TestEqual(TEXT("Hard zero rejects a non-finite visual value"),
		FSightWeaveVisualFeatherMath::ComposeWeight(false,
			std::numeric_limits<float>::quiet_NaN()), 0.0f);
	TestEqual(TEXT("Hard one clamps negative visual weight"),
		FSightWeaveVisualFeatherMath::ComposeWeight(true, -0.5f), 0.0f);
	TestEqual(TEXT("Hard one clamps weight above one"),
		FSightWeaveVisualFeatherMath::ComposeWeight(true, 1.5f), 1.0f);
	TestEqual(TEXT("Hard one preserves an in-range visual weight"),
		FSightWeaveVisualFeatherMath::ComposeWeight(true, 0.375f), 0.375f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P4BindingIsolationTest,
	"SightWeave.M3P4.Presentation.FeatherBindingIsolation",
	SightWeave::M3P4::PresentationTests::TestFlags)

bool FSightWeaveM3P4BindingIsolationTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P4::PresentationTests;
	const TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet =
		BuildPacket(*this);
	if (!Packet.IsValid())
	{
		return false;
	}
	FSightWeaveVisualFeatherSettings Feather;
	Feather.WidthCentimeters = 50.0f;
	const FSightWeaveViewPresentationSelection Selection =
		FSightWeaveViewPresentationSelection::Enabled(
			Packet->GetWorldIdentity(),
			FSightWeaveKnowledgeOwnerId(FName(TEXT("FeatherOwner"))),
			FSightWeaveFloorId(FName(TEXT("FeatherFloor"))),
			ESightWeaveRenderPrecisionTier::Standard,
			21,
			Feather);
	TestTrue(TEXT("Feather selection is valid"), Selection.IsValid());
	TestEqual(TEXT("Missing feather resources fail closed"),
		FSightWeavePresentationBindingBuilder::Build(*Packet, Selection, 31, 41).Failure,
		ESightWeavePresentationBindingFailure::FeatherResourceGenerationMismatch);
	TestEqual(TEXT("Hard/feather packet revision mismatch fails closed"),
		FSightWeavePresentationBindingBuilder::Build(*Packet, Selection, 31, 41, 51, 10, 21).Failure,
		ESightWeavePresentationBindingFailure::FeatherRevisionMismatch);
	TestEqual(TEXT("Stale feather settings revision fails closed"),
		FSightWeavePresentationBindingBuilder::Build(*Packet, Selection, 31, 41, 51, 11, 20).Failure,
		ESightWeavePresentationBindingFailure::FeatherRevisionMismatch);
	const FSightWeavePresentationBindingBuildResult Matching =
		FSightWeavePresentationBindingBuilder::Build(*Packet, Selection, 31, 41, 51, 11, 21);
	TestTrue(TEXT("Matching hard/feather provenance binds"), Matching.Succeeded());
	if (Matching.Succeeded())
	{
		TestEqual(TEXT("Binding preserves world-space width"),
			Matching.Binding->GetVisualFeather().WidthCentimeters, 50.0f);
		TestEqual(TEXT("Binding preserves feather generation"),
			Matching.Binding->GetFeatherResourceGeneration(), uint64(51));
	}

	const FSightWeaveViewPresentationSelection HardOnly =
		FSightWeaveViewPresentationSelection::Enabled(
			Packet->GetWorldIdentity(),
			FSightWeaveKnowledgeOwnerId(FName(TEXT("FeatherOwner"))),
			FSightWeaveFloorId(FName(TEXT("FeatherFloor"))),
			ESightWeaveRenderPrecisionTier::Standard,
			22);
	TestTrue(TEXT("Width zero binds without feather resources"),
		FSightWeavePresentationBindingBuilder::Build(*Packet, HardOnly, 31, 41).Succeeded());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P4BoundedTransformTest,
	"SightWeave.M3P4.Presentation.BoundedWorldTransform",
	SightWeave::M3P4::PresentationTests::TestFlags)

bool FSightWeaveM3P4BoundedTransformTest::RunTest(const FString& Parameters)
{
	for (const ESightWeaveRenderPrecisionTier Tier : {
		ESightWeaveRenderPrecisionTier::Coarse,
		ESightWeaveRenderPrecisionTier::Standard,
		ESightWeaveRenderPrecisionTier::Fine,
		ESightWeaveRenderPrecisionTier::Ultra })
	{
		const int32 Radius = FMath::CeilToInt(
			SightWeave::VisualFeather::MaximumWidthCentimeters
			/ SightWeaveCentimetersPerTexel(Tier));
		TestTrue(TEXT("Maximum world width remains inside the fixed transform halo"),
			Radius <= SightWeave::VisualFeather::MaximumRadiusTexels);
	}
	TestEqual(TEXT("Transform work size is fixed and bounded"),
		SightWeave::VisualFeather::TransformWorkSize, 328);
	return true;
}

#endif
