#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "RenderingThread.h"
#include "SightWeaveRenderWorldSubsystem.h"
#include "SightWeavePresentation.h"
#include "SightWeaveSparseAtlas.h"
#include "SightWeaveWorldSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace SightWeave::M3P5::PresentationIntegrationTests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	class FTestWorld final
	{
	public:
		FTestWorld()
		{
			const FName WorldName = MakeUniqueObjectName(
				GetTransientPackage(),
				UWorld::StaticClass(),
				FName(TEXT("SightWeaveM3P5PresentationPrecision")));
			World = NewObject<UWorld>(GetTransientPackage(), WorldName, RF_Transient);
			if (!World || !GEngine)
			{
				return;
			}
			World->WorldType = EWorldType::PIE;
			FWorldContext& Context = GEngine->CreateNewWorldContext(World->WorldType);
			Context.SetCurrentWorld(World);
			World->InitializeNewWorld(UWorld::InitializationValues()
				.AllowAudioPlayback(false)
				.CreatePhysicsScene(false)
				.RequiresHitProxies(false)
				.CreateNavigation(false)
				.CreateAISystem(false)
				.ShouldSimulatePhysics(false)
				.SetTransactional(false));
		}

		~FTestWorld()
		{
			if (World && GEngine)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(true);
			}
		}

		UWorld* Get() const { return World; }

	private:
		UWorld* World = nullptr;
	};

	FSightWeaveFloorDefinition MakeFloor()
	{
		FSightWeaveFloorDefinition Floor;
		Floor.FloorId = FSightWeaveFloorId(FName(TEXT("Ground")));
		Floor.BoundsMin = FVector2D(-10000.0, -10000.0);
		Floor.BoundsMax = FVector2D(10000.0, 10000.0);
		Floor.HeightRange = { 0.0f, 300.0f };
		Floor.bEnabled = true;
		Floor.bActiveForQueries = true;
		return Floor;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P5PresentationPrecisionIntegrationTest,
	"SightWeave.M3P5.Memory.PresentationPrecisionIntegration",
	SightWeave::M3P5::PresentationIntegrationTests::TestFlags)

bool FSightWeaveM3P5PresentationPrecisionIntegrationTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P5::PresentationIntegrationTests;
	FTestWorld World;
	if (!TestNotNull(TEXT("M3.5 precision integration world exists"), World.Get()))
	{
		return false;
	}
	USightWeaveWorldSubsystem* Runtime =
		World.Get()->GetSubsystem<USightWeaveWorldSubsystem>();
	USightWeaveRenderWorldSubsystem* Render =
		World.Get()->GetSubsystem<USightWeaveRenderWorldSubsystem>();
	if (!TestNotNull(TEXT("Runtime subsystem exists"), Runtime)
		|| !TestNotNull(TEXT("Render subsystem exists"), Render))
	{
		return false;
	}

	const FSightWeaveFloorDefinition Floor = MakeFloor();
	const FSightWeaveKnowledgeOwnerId LocalOwner(FName(TEXT("Local")));
	TestTrue(TEXT("Active floor registers"), Runtime->RegisterFloor(Floor, World.Get()));
	TestEqual(
		TEXT("Live presentation starts at the legacy Standard tier"),
		Render->GetPresentationSelection().GetPrecisionTier(),
		ESightWeaveRenderPrecisionTier::Standard);

	TestTrue(
		TEXT("Coarse exploration memory configures"),
		Runtime->ConfigureExplorationMemory(
			LocalOwner,
			Floor.FloorId,
			ESightWeaveRenderPrecisionTier::Coarse,
			SightWeaveDefaultActiveTileCapacity(ESightWeaveRenderPrecisionTier::Coarse)));
	TestEqual(
		TEXT("Published memory scope drives the default live presentation tier"),
		Render->GetPresentationSelection().GetPrecisionTier(),
		ESightWeaveRenderPrecisionTier::Coarse);

	TestTrue(
		TEXT("Explicit Fine presentation remains supported"),
		Render->SetPresentationScope(
			LocalOwner,
			Floor.FloorId,
			ESightWeaveRenderPrecisionTier::Fine));
	TestEqual(
		TEXT("Explicit precision overrides memory-derived default"),
		Render->GetPresentationSelection().GetPrecisionTier(),
		ESightWeaveRenderPrecisionTier::Fine);

	Render->ClearPresentationScope();
	TestEqual(
		TEXT("Clearing explicit scope restores the memory-compatible tier"),
		Render->GetPresentationSelection().GetPrecisionTier(),
		ESightWeaveRenderPrecisionTier::Coarse);

	Runtime->DisableExplorationMemory();
	TestEqual(
		TEXT("Disabling memory restores the legacy Standard default"),
		Render->GetPresentationSelection().GetPrecisionTier(),
		ESightWeaveRenderPrecisionTier::Standard);
	FlushRenderingCommands();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P5BypassCanonicalProfileTest,
	"SightWeave.M3P5.Memory.BypassCanonicalProfileBinding",
	SightWeave::M3P5::PresentationIntegrationTests::TestFlags)

bool FSightWeaveM3P5BypassCanonicalProfileTest::RunTest(const FString& Parameters)
{
	const FSightWeaveRenderWorldIdentity WorldIdentity { 3551 };
	const FSightWeaveKnowledgeOwnerId Owner(FName(TEXT("Local")));
	const FSightWeaveFloorId Floor(FName(TEXT("Ground")));
	FSightWeaveIlluminationCompatibilityProfile Compatibility;
	Compatibility.AcceptedCapabilities.Add(FName(TEXT("Visible")));
	Compatibility.Normalize();
	const FSightWeaveRenderProfileIdentity ExpectedProfile =
		FSightWeaveRenderProfileIdentity::FromProfile(Compatibility);

	FSightWeaveSparseRenderPacketBuildInput Input;
	Input.WorldIdentity = WorldIdentity;
	Input.PacketRevision = 1;
	Input.RegistryRevision = 2;
	Input.PublishedSnapshotRevision = 3;
	FSightWeaveSparseScopeBuildInput& Scope = Input.Scopes.AddDefaulted_GetRef();
	Scope.KnowledgeOwnerId = Owner;
	Scope.FloorId = Floor;
	Scope.PrecisionTier = ESightWeaveRenderPrecisionTier::Coarse;
	Scope.MaximumActiveTiles = SightWeaveDefaultActiveTileCapacity(Scope.PrecisionTier);
	FSightWeaveSparsePolygonInput& Bypass = Scope.Polygons.AddDefaulted_GetRef();
	Bypass.StableSourceId = 1;
	Bypass.SourceRevision = 1;
	Bypass.Layer = ESightWeaveRenderMaskLayer::Bypass;
	Bypass.CompatibilityProfile = ExpectedProfile;
	Bypass.WorldVertices = {
		FVector2D(0.0, 0.0),
		FVector2D(1000.0, 0.0),
		FVector2D(1000.0, 1000.0),
		FVector2D(0.0, 1000.0)
	};

	const FSightWeaveSparseRenderPacketBuildResult Built =
		FSightWeaveSparseRenderPacketBuilder::Build(Input);
	if (!TestTrue(TEXT("Bypass-only sparse packet builds"), Built.Succeeded()))
	{
		return false;
	}
	const FSightWeaveViewPresentationSelection Selection =
		FSightWeaveViewPresentationSelection::Enabled(
			WorldIdentity,
			Owner,
			Floor,
			ESightWeaveRenderPrecisionTier::Coarse,
			1);
	const FSightWeavePresentationBindingBuildResult Binding =
		FSightWeavePresentationBindingBuilder::Build(*Built.Packet, Selection, 1, 1);
	if (!TestTrue(TEXT("Bypass-only presentation binding builds"), Binding.Succeeded()))
	{
		return false;
	}
	TestEqual(
		TEXT("Bypass profile is retained in the presentation binding"),
		Binding.Binding->GetCanonicalProfiles().Num(),
		1);
	TestTrue(
		TEXT("Bypass binding profile matches memory scope canonical identity"),
		Binding.Binding->GetCanonicalProfiles()[0].IsEquivalentTo(ExpectedProfile));
	return true;
}

#endif
