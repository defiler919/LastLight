#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "RHI.h"
#include "SightWeaveSparseAtlas.h"
#include "SightWeaveSparseAtlasTestReadback.h"

namespace SightWeave::M3P2::NullRHITests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> BuildPacket(
		FAutomationTestBase* Test)
	{
		FSightWeaveSparseRenderPacketBuildInput Input;
		Input.WorldIdentity.Serial = 5101;
		Input.PacketRevision = 1;
		Input.RegistryRevision = 1;
		Input.PublishedSnapshotRevision = 1;
		FSightWeaveSparseScopeBuildInput& Scope = Input.Scopes.AddDefaulted_GetRef();
		Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("NullOwner")));
		Scope.FloorId = FSightWeaveFloorId(FName(TEXT("NullFloor")));
		Scope.PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
		Scope.MaximumActiveTiles = 1;
		FSightWeaveSparsePolygonInput& Polygon = Scope.Polygons.AddDefaulted_GetRef();
		Polygon.StableSourceId = 1;
		Polygon.SourceRevision = 1;
		Polygon.Layer = ESightWeaveRenderMaskLayer::Bypass;
		Polygon.CompatibilityProfile = FSightWeaveRenderProfileIdentity::FromProfile(
			FSightWeaveIlluminationCompatibilityProfile());
		Polygon.WorldVertices = {
			FVector2D(100.0, 100.0),
			FVector2D(800.0, 100.0),
			FVector2D(800.0, 800.0),
			FVector2D(100.0, 800.0)
		};
		const FSightWeaveSparseRenderPacketBuildResult Built =
			FSightWeaveSparseRenderPacketBuilder::Build(Input);
		if (!Built.Succeeded())
		{
			Test->AddError(TEXT("NullRHI sparse packet failed to build"));
			return nullptr;
		}
		return Built.Packet;
	}

	struct FNullContext
	{
		TSharedPtr<FSightWeaveSparseAtlasTestReadback, ESPMode::ThreadSafe> Request;
		FSightWeaveSparseReadbackExpectation Expectation;
		double StartSeconds = FPlatformTime::Seconds();
	};

	class FWaitForNullRHI final : public IAutomationLatentCommand
	{
	public:
		FWaitForNullRHI(TSharedPtr<FNullContext> InContext, FAutomationTestBase* InTest)
			: Context(MoveTemp(InContext))
			, Test(InTest)
		{
		}

		virtual bool Update() override
		{
			Context->Request->Poll();
			if (!Context->Request->IsFinished())
			{
				if (FPlatformTime::Seconds() - Context->StartSeconds > 20.0)
				{
					Test->AddError(TEXT("M3.2 NullRHI fail-black request timed out"));
					return true;
				}
				return false;
			}
			FSightWeaveSparseReadbackResult Result;
			if (!Context->Request->TryTakeResult(Context->Expectation, Result))
			{
				Test->AddError(TEXT("M3.2 NullRHI result was unavailable"));
				return true;
			}
			Test->TestEqual(TEXT("NullRHI never publishes a readable GPU tile"),
				Result.Status, ESightWeaveSparseReadbackStatus::Failed);
			Test->TestEqual(TEXT("NullRHI availability is explicit"),
				Result.Availability, ESightWeaveRenderAvailability::NullRHI);
			Test->TestEqual(TEXT("NullRHI returns no stale or white pixel payload"), Result.Pixels.Num(), 0);
			Test->TestEqual(TEXT("NullRHI allocates no atlas page"),
				Result.Updates[0].AllocatedPageCount, 0);
			Test->TestEqual(TEXT("NullRHI allocates no scratch texture"),
				Result.Updates[0].ScratchAllocationCount, uint64(0));
			Test->TestFalse(TEXT("NullRHI schedules no mask work"), Result.Updates[0].bProducedMaskWork);
			return true;
		}

	private:
		TSharedPtr<FNullContext> Context;
		FAutomationTestBase* Test = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P2NullRHIFailBlackTest,
	"SightWeave.M3P2.NullRHI.SparseAtlasFailBlackNoGPUAllocation",
	SightWeave::M3P2::NullRHITests::TestFlags)

bool FSightWeaveM3P2NullRHIFailBlackTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P2::NullRHITests;
	if (!GUsingNullRHI)
	{
		AddInfo(TEXT("NullRHI-only M3.2 assertions skipped on a rendered RHI"));
		return true;
	}
	const TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet =
		BuildPacket(this);
	if (!Packet.IsValid() || Packet->GetTiles().IsEmpty())
	{
		return false;
	}
	TSharedPtr<FNullContext> Context = MakeShared<FNullContext>();
	Context->Expectation.TileIdentity = Packet->GetTiles()[0].Identity;
	Context->Expectation.PacketRevision = Packet->GetPacketRevision();
	Context->Request = FSightWeaveSparseAtlasTestReadback::StartSequence(
		{ Packet },
		Packet->GetTiles()[0].Identity);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForNullRHI(Context, this));
	return true;
}

#endif
