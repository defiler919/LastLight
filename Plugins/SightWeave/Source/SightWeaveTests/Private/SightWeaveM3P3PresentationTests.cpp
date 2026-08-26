#if WITH_DEV_AUTOMATION_TESTS

#include "Math/OrthoMatrix.h"
#include "Math/PerspectiveMatrix.h"
#include "Misc/AutomationTest.h"
#include "SceneView.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P3PresentationMappingContractTest,
	"SightWeave.M3P3.Presentation.MappingContract",
	SightWeave::M3P3::PresentationTests::TestFlags)

bool FSightWeaveM3P3PresentationMappingContractTest::RunTest(const FString& Parameters)
{
	FSightWeaveSparseScopeKey Scope;
	Scope.WorldIdentity.Serial = 3303;
	Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("MappingOwner")));
	Scope.FloorId = FSightWeaveFloorId(FName(TEXT("MappingFloor")));
	Scope.FloorOrigin = FVector2D(100000000.0, -200000000.0);
	Scope.PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
	FSightWeaveSparsePhysicalAddress Address;
	Address.PageIndex = 1;
	Address.SlotIndex = 63;
	const double Span = SightWeave::SparseAtlas::InteriorTileSize
		* SightWeaveCentimetersPerTexel(Scope.PrecisionTier);

	auto Map = [&Scope, &Address](const FVector2D& World)
	{
		return FSightWeavePresentationMapping::MapWorldPosition(Scope, Address, World);
	};
	const FSightWeavePresentationAtlasLookup Origin = Map(Scope.FloorOrigin);
	TestTrue(TEXT("Floor origin maps successfully"), Origin.bValid);
	TestEqual(TEXT("Floor origin logical coordinate"), Origin.LogicalCoordinate, FIntPoint(0, 0));
	TestEqual(TEXT("Floor origin maps to first interior texel"), Origin.InteriorTexel, FIntPoint(0, 0));
	TestEqual(TEXT("Slot 63 respects gutter and physical slot origin"),
		Origin.AtlasTexel,
		FIntPoint(7 * 256 + 4, 7 * 256 + 4));

	const FSightWeavePresentationAtlasLookup Negative = Map(
		Scope.FloorOrigin + FVector2D(-0.01, -0.01));
	TestEqual(TEXT("Negative world offset floors to negative logical tile"),
		Negative.LogicalCoordinate,
		FIntPoint(-1, -1));
	TestEqual(TEXT("Negative seam selects the last interior texel"),
		Negative.InteriorTexel,
		FIntPoint(247, 247));

	const FSightWeavePresentationAtlasLookup ExactSeam = Map(
		Scope.FloorOrigin + FVector2D(Span, Span));
	TestEqual(TEXT("Exact diagonal seam selects the next tile"),
		ExactSeam.LogicalCoordinate,
		FIntPoint(1, 1));
	TestEqual(TEXT("Exact seam maps to texel zero"),
		ExactSeam.InteriorTexel,
		FIntPoint(0, 0));

	const FSightWeavePresentationAtlasLookup FourTileCorner = Map(
		Scope.FloorOrigin + FVector2D(Span - 0.001, Span - 0.001));
	TestEqual(TEXT("Four-tile corner remains in the lower tile before the seam"),
		FourTileCorner.LogicalCoordinate,
		FIntPoint(0, 0));
	TestEqual(TEXT("Four-tile corner clamps to the final interior texel"),
		FourTileCorner.InteriorTexel,
		FIntPoint(247, 247));

	const FSightWeavePresentationAtlasLookup Large = Map(
		Scope.FloorOrigin + FVector2D(Span * 100000.0 + 15.0, -Span * 100000.0 + 25.0));
	TestEqual(TEXT("Large positive logical coordinate remains exact"),
		Large.LogicalCoordinate.X,
		100000);
	TestEqual(TEXT("Large negative logical coordinate remains exact"),
		Large.LogicalCoordinate.Y,
		-100000);
	TestEqual(TEXT("Large-coordinate local texel remains floor relative"),
		Large.InteriorTexel,
		FIntPoint(1, 2));

	Address.SlotIndex = 0;
	const FSightWeavePresentationAtlasLookup PageOneSlotZero = Map(Scope.FloorOrigin);
	TestEqual(TEXT("Page 1 slot 0 uses its own page-local texel"),
		PageOneSlotZero.AtlasTexel,
		FIntPoint(4, 4));
	TestFalse(TEXT("Non-finite positions fail closed"),
		FSightWeavePresentationMapping::MapWorldPosition(
			Scope,
			Address,
			FVector2D(std::numeric_limits<double>::quiet_NaN(), 0.0)).bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P3ViewReconstructionContractTest,
	"SightWeave.M3P3.Presentation.ViewReconstructionContract",
	SightWeave::M3P3::PresentationTests::TestFlags)

bool FSightWeaveM3P3ViewReconstructionContractTest::RunTest(const FString& Parameters)
{
	struct FViewCase
	{
		FIntPoint Resolution;
		double FovDegrees = 60.0;
		double RotationDegrees = 0.0;
		FVector Camera = FVector(0.0, 0.0, 10000.0);
		bool bOrthographic = false;
	};
	const TArray<FViewCase> Cases = {
		{ FIntPoint(1920, 1080), 60.0, 0.0, FVector(0.0, 0.0, 10000.0), false },
		{ FIntPoint(2560, 1440), 90.0, 45.0, FVector(2500.0, -1500.0, 10000.0), false },
		{ FIntPoint(1280, 720), 75.0, 90.0, FVector(-4000.0, 3000.0, 12000.0), false },
		{ FIntPoint(1920, 1080), 0.0, 0.0, FVector(0.0, 0.0, 10000.0), true },
		{ FIntPoint(2560, 1440), 0.0, 45.0, FVector(2500.0, -1500.0, 10000.0), true },
		{ FIntPoint(960, 540), 0.0, 90.0, FVector(-4000.0, 3000.0, 12000.0), true }
	};
	const TArray<FVector> RelativeWorldPoints = {
		FVector(5.0, 5.0, 0.0),
		FVector(805.0, -595.0, 0.0),
		FVector(-1195.0, 905.0, 0.0)
	};

	FSightWeaveSparseScopeKey Scope;
	Scope.WorldIdentity.Serial = 3304;
	Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("ViewOwner")));
	Scope.FloorId = FSightWeaveFloorId(FName(TEXT("ViewFloor")));
	Scope.FloorOrigin = FVector2D(-5000.0, -5000.0);
	Scope.PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
	FSightWeaveSparsePhysicalAddress Address;
	Address.PageIndex = 0;
	Address.SlotIndex = 0;

	for (int32 CaseIndex = 0; CaseIndex < Cases.Num(); ++CaseIndex)
	{
		const FViewCase& Case = Cases[CaseIndex];
		const double Radians = FMath::DegreesToRadians(Case.RotationDegrees);
		const FVector Up(FMath::Sin(Radians), FMath::Cos(Radians), 0.0);
		const FVector Target(Case.Camera.X, Case.Camera.Y, 0.0);
		const FMatrix ViewMatrix = FLookAtMatrix(Case.Camera, Target, Up);
		const double Aspect = static_cast<double>(Case.Resolution.X) / Case.Resolution.Y;
		const FMatrix ProjectionMatrix = Case.bOrthographic
			? FMatrix(FReversedZOrthoMatrix(
				10000.0,
				10000.0 / Aspect,
				1.0 / 20000.0,
				0.0))
			: FMatrix(FReversedZPerspectiveMatrix(
				FMath::DegreesToRadians(Case.FovDegrees * 0.5),
				static_cast<double>(Case.Resolution.X),
				static_cast<double>(Case.Resolution.Y),
				10.0));
		const FMatrix ViewProjection = ViewMatrix * ProjectionMatrix;
		const FMatrix InverseViewProjection = ViewProjection.Inverse();
		const FIntRect ViewRect(FIntPoint::ZeroValue, Case.Resolution);

		for (int32 PointIndex = 0; PointIndex < RelativeWorldPoints.Num(); ++PointIndex)
		{
			const FVector WorldPoint = Target + RelativeWorldPoints[PointIndex];
			FVector2D ScreenPosition;
			const bool bProjected = FSceneView::ProjectWorldToScreen(
				WorldPoint,
				ViewRect,
				ViewProjection,
				ScreenPosition,
				false);
			TestTrue(*FString::Printf(TEXT("View case %d point %d projects"), CaseIndex, PointIndex),
				bProjected);
			if (!bProjected)
			{
				continue;
			}
			const FVector2D PixelCenter(
				FMath::FloorToDouble(ScreenPosition.X) + 0.5,
				FMath::FloorToDouble(ScreenPosition.Y) + 0.5);
			FVector RayOrigin;
			FVector RayDirection;
			FSceneView::DeprojectScreenToWorld(
				PixelCenter,
				ViewRect,
				InverseViewProjection,
				RayOrigin,
				RayDirection);
			if (!TestTrue(*FString::Printf(TEXT("View case %d ray intersects floor"), CaseIndex),
				FMath::Abs(RayDirection.Z) > UE_DOUBLE_SMALL_NUMBER))
			{
				continue;
			}
			const FVector Reconstructed = RayOrigin
				+ RayDirection * (-RayOrigin.Z / RayDirection.Z);
			const double ReconstructionError = FVector::Dist2D(Reconstructed, WorldPoint);
			auto ReconstructPixel = [&ViewRect, &InverseViewProjection](const FVector2D& Pixel)
			{
				FVector Origin;
				FVector Direction;
				FSceneView::DeprojectScreenToWorld(
					Pixel,
					ViewRect,
					InverseViewProjection,
					Origin,
					Direction);
				return Origin + Direction * (-Origin.Z / Direction.Z);
			};
			const FVector AdjacentX = ReconstructPixel(PixelCenter + FVector2D(1.0, 0.0));
			const FVector AdjacentY = ReconstructPixel(PixelCenter + FVector2D(0.0, 1.0));
			const double PixelFootprintBound = 0.75 * (
				FVector::Dist2D(Reconstructed, AdjacentX)
				+ FVector::Dist2D(Reconstructed, AdjacentY)) + 0.01;
			TestTrue(*FString::Printf(
				TEXT("View case %d point %d reconstruction stays inside one pixel footprint (error=%.6f bound=%.6f)"),
				CaseIndex,
				PointIndex,
				ReconstructionError,
				PixelFootprintBound),
				ReconstructionError <= PixelFootprintBound);
			FVector2D Reprojected;
			TestTrue(TEXT("Reconstructed floor point reprojects"),
				FSceneView::ProjectWorldToScreen(
					Reconstructed,
					ViewRect,
					ViewProjection,
					Reprojected,
					false));
			TestTrue(TEXT("Reconstructed floor point returns within one screen pixel X"),
				FMath::Abs(FMath::FloorToInt(Reprojected.X) - FMath::FloorToInt(PixelCenter.X)) <= 1);
			TestTrue(TEXT("Reconstructed floor point returns within one screen pixel Y"),
				FMath::Abs(FMath::FloorToInt(Reprojected.Y) - FMath::FloorToInt(PixelCenter.Y)) <= 1);
			const FSightWeavePresentationAtlasLookup Actual =
				FSightWeavePresentationMapping::MapWorldPosition(
					Scope,
					Address,
					FVector2D(Reconstructed.X, Reconstructed.Y));
			TestTrue(TEXT("Reconstructed view position maps into the sparse atlas"),
				Actual.bValid);
			TestTrue(TEXT("Reconstructed view position stays inside the selected slot interior"),
				Actual.InteriorTexel.X >= 0 && Actual.InteriorTexel.X < 248
					&& Actual.InteriorTexel.Y >= 0 && Actual.InteriorTexel.Y < 248);
		}
	}
	return true;
}

#endif
