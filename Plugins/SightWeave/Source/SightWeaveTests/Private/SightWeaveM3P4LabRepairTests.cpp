#if WITH_DEV_AUTOMATION_TESTS

#include "ISettingsContainer.h"
#include "ISettingsModule.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "SightWeaveLabSupport.h"
#include "SightWeaveSparseAtlas.h"

namespace SightWeave::M3P4::LabRepairTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter;

	FSightWeaveSparseRenderPacketBuildResult BuildLabFootprint(const double PageBoundaryY)
	{
		FSightWeaveSparseRenderPacketBuildInput Input;
		Input.WorldIdentity.Serial = 34001;
		Input.PacketRevision = 1;
		Input.RegistryRevision = 1;
		Input.PublishedSnapshotRevision = 1;

		FSightWeaveSparseScopeBuildInput& Scope = Input.Scopes.AddDefaulted_GetRef();
		Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("Local")));
		Scope.FloorId = FSightWeaveFloorId(FName(TEXT("Ground")));
		Scope.FloorOrigin = FVector2D(-8500.0, -6500.0);
		Scope.PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
		Scope.MaximumActiveTiles = SightWeave::SparseAtlas::StandardActiveTileCapacity;

		FSightWeaveSparsePolygonInput& Overview = Scope.Polygons.AddDefaulted_GetRef();
		Overview.StableSourceId = 1;
		Overview.SourceRevision = 1;
		Overview.Layer = ESightWeaveRenderMaskLayer::Bypass;
		Overview.WorldVertices = {
			FVector2D(22500.0, -1300.0), FVector2D(39500.0, -1300.0),
			FVector2D(39500.0, 15700.0), FVector2D(22500.0, 15700.0)
		};

		constexpr double ConeRange = 160000.0;
		const double ConeHalfWidth = FMath::Tan(0.2 * PI / 180.0) * ConeRange;
		FSightWeaveSparsePolygonInput& PageBoundary = Scope.Polygons.AddDefaulted_GetRef();
		PageBoundary.StableSourceId = 2;
		PageBoundary.SourceRevision = 1;
		PageBoundary.Layer = ESightWeaveRenderMaskLayer::Bypass;
		PageBoundary.WorldVertices = {
			FVector2D(-8000.0, PageBoundaryY),
			FVector2D(152000.0, PageBoundaryY - ConeHalfWidth),
			FVector2D(152000.0, PageBoundaryY + ConeHalfWidth)
		};

		return FSightWeaveSparseRenderPacketBuilder::Build(Input);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P4LabFixtureIsolationTest,
	"SightWeave.M3P4.Lab.FixtureIsolation",
	SightWeave::M3P4::LabRepairTests::TestFlags)

bool FSightWeaveM3P4LabFixtureIsolationTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("M3.4 enables its overview source"),
		SightWeave::Lab::IsFixtureEnabled(TEXT("SW_M3P4_OverviewBypass"), ESightWeaveLabMode::M3P4));
	TestTrue(TEXT("M3.4 reuses the corrected page-boundary source"),
		SightWeave::Lab::IsFixtureEnabled(TEXT("SW_M3P3_PageBoundaryVision"), ESightWeaveLabMode::M3P4));
	TestFalse(TEXT("M3.4 disables the broad M3.3 presentation source"),
		SightWeave::Lab::IsFixtureEnabled(TEXT("SW_M3P3_PresentationVision"), ESightWeaveLabMode::M3P4));
	TestFalse(TEXT("M3.4 disables all M2 authority fixtures"),
		SightWeave::Lab::IsFixtureEnabled(TEXT("SW_M2_20_DebugVision"), ESightWeaveLabMode::M3P4));

	TestTrue(TEXT("M3.3 enables its presentation source"),
		SightWeave::Lab::IsFixtureEnabled(TEXT("SW_M3P3_PresentationVision"), ESightWeaveLabMode::M3P3));
	TestFalse(TEXT("M3.3 disables M3.4 fixtures"),
		SightWeave::Lab::IsFixtureEnabled(TEXT("SW_M3P4_OverviewBypass"), ESightWeaveLabMode::M3P3));
	TestTrue(TEXT("M2 mode enables M2 fixtures"),
		SightWeave::Lab::IsFixtureEnabled(TEXT("SW_M2_20_DebugVision"), ESightWeaveLabMode::M2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P4LabPageBoundaryCapacityTest,
	"SightWeave.M3P4.Lab.PageBoundaryCapacity",
	SightWeave::M3P4::LabRepairTests::TestFlags)

bool FSightWeaveM3P4LabPageBoundaryCapacityTest::RunTest(const FString& Parameters)
{
	constexpr double GroundMinimumY = -6500.0;
	constexpr double TileSpan = 2480.0;
	constexpr double ConeRange = 160000.0;
	constexpr double ConeHalfAngleRadians = 0.2 * PI / 180.0;
	const double ConeHalfWidth = FMath::Tan(ConeHalfAngleRadians) * ConeRange;
	const int32 MinimumRow = FMath::FloorToInt(
		(SightWeave::Lab::SafePageBoundaryY - ConeHalfWidth - GroundMinimumY) / TileSpan);
	const int32 MaximumRow = FMath::FloorToInt(
		(SightWeave::Lab::SafePageBoundaryY + ConeHalfWidth - GroundMinimumY) / TileSpan);

	TestEqual(TEXT("Corrected cone minimum remains in logical row 7"), MinimumRow, 7);
	TestEqual(TEXT("Corrected cone maximum remains in logical row 7"), MaximumRow, 7);
	TestTrue(TEXT("One-row 65-column fixture remains below frozen sparse capacity"),
		65 <= SightWeave::SparseAtlas::StandardActiveTileCapacity);
	TestTrue(TEXT("The former two-row fixture exceeded frozen sparse capacity"),
		65 * 2 > SightWeave::SparseAtlas::StandardActiveTileCapacity);

	const FSightWeaveSparseRenderPacketBuildResult Corrected =
		SightWeave::M3P4::LabRepairTests::BuildLabFootprint(SightWeave::Lab::SafePageBoundaryY);
	if (TestTrue(TEXT("Corrected Lab footprint builds a packet"), Corrected.Succeeded())
		&& TestEqual(TEXT("Corrected Lab footprint has one scope"), Corrected.Packet->GetScopes().Num(), 1))
	{
		const FSightWeaveSparseRenderScope& Scope = Corrected.Packet->GetScopes()[0];
		TestEqual(TEXT("Corrected Lab scope remains valid"), Scope.Failure,
			ESightWeaveSparsePacketFailure::None);
		TestEqual(TEXT("Corrected combined Lab footprint uses 113 logical tiles"),
			Scope.DesiredTileCount, 113);
		TestTrue(TEXT("Corrected Lab footprint fits the frozen sparse capacity"),
			Scope.DesiredTileCount <= Scope.MaximumActiveTiles);
	}

	const FSightWeaveSparseRenderPacketBuildResult Former =
		SightWeave::M3P4::LabRepairTests::BuildLabFootprint(11000.0);
	if (TestTrue(TEXT("Former Lab footprint returns a fail-closed packet"), Former.Succeeded())
		&& TestEqual(TEXT("Former Lab footprint has one scope"), Former.Packet->GetScopes().Num(), 1))
	{
		const FSightWeaveSparseRenderScope& Scope = Former.Packet->GetScopes()[0];
		TestEqual(TEXT("Former Lab footprint reproduces capacity fail-closed"), Scope.Failure,
			ESightWeaveSparsePacketFailure::CapacityExceeded);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P4SettingsRegistrationTest,
	"SightWeave.M3P4.Editor.SettingsRegistration",
	SightWeave::M3P4::LabRepairTests::TestFlags)

bool FSightWeaveM3P4SettingsRegistrationTest::RunTest(const FString& Parameters)
{
	FModuleManager::Get().LoadModule(TEXT("SightWeaveEditor"));
	ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>(TEXT("Settings"));
	const ISettingsContainerPtr ProjectSettings = SettingsModule.GetContainer(TEXT("Project"));
	TestTrue(TEXT("Project > Plugins > SightWeave settings section is registered"),
		ProjectSettings.IsValid()
			&& ProjectSettings->GetSection(TEXT("Plugins"), TEXT("SightWeave")).IsValid());
	return true;
}

#endif
