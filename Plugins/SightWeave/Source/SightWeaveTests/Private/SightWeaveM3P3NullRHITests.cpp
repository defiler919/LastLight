#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "RHI.h"
#include "SightWeavePresentationTestReadback.h"

namespace SightWeave::M3P3::NullRHITests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> BuildPacket(
		FAutomationTestBase* Test)
	{
		FSightWeaveIlluminationCompatibilityProfile SourceProfile;
		SourceProfile.AcceptedCapabilities.Add(FName(TEXT("Visible")));
		const FSightWeaveRenderProfileIdentity Profile =
			FSightWeaveRenderProfileIdentity::FromProfile(SourceProfile);
		FSightWeaveSparseRenderPacketBuildInput Input;
		Input.WorldIdentity.Serial = 3320;
		Input.PacketRevision = 1;
		Input.RegistryRevision = 2;
		Input.PublishedSnapshotRevision = 3;
		FSightWeaveSparseScopeBuildInput& Scope = Input.Scopes.AddDefaulted_GetRef();
		Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("NullPresentationOwner")));
		Scope.FloorId = FSightWeaveFloorId(FName(TEXT("NullPresentationFloor")));
		Scope.PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
		Scope.MaximumActiveTiles = 1;
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
				FVector2D(100.0, 100.0),
				FVector2D(800.0, 100.0),
				FVector2D(800.0, 800.0),
				FVector2D(100.0, 800.0)
			};
		}
		const FSightWeaveSparseRenderPacketBuildResult Built =
			FSightWeaveSparseRenderPacketBuilder::Build(Input);
		if (!Built.Succeeded())
		{
			Test->AddError(TEXT("M3.3 NullRHI packet failed to build"));
		}
		return Built.Packet;
	}

	struct FContext
	{
		TSharedPtr<FSightWeavePresentationTestReadback, ESPMode::ThreadSafe> Request;
		double StartSeconds = FPlatformTime::Seconds();
	};

	class FWaitForNullPresentation final : public IAutomationLatentCommand
	{
	public:
		FWaitForNullPresentation(TSharedPtr<FContext> InContext, FAutomationTestBase* InTest)
			: Context(MoveTemp(InContext)), Test(InTest)
		{
		}

		virtual bool Update() override
		{
			Context->Request->Poll();
			if (!Context->Request->IsFinished())
			{
				if (FPlatformTime::Seconds() - Context->StartSeconds > 20.0)
				{
					Test->AddError(TEXT("M3.3 NullRHI presentation request timed out"));
					return true;
				}
				return false;
			}
			FSightWeavePresentationReadbackResult Result;
			if (!Context->Request->TryTakeResult(Result))
			{
				Test->AddError(TEXT("M3.3 NullRHI presentation result unavailable"));
				return true;
			}
			Test->TestEqual(TEXT("NullRHI presentation is explicitly unavailable"),
				Result.Status,
				ESightWeavePresentationReadbackStatus::Failed);
			Test->TestEqual(TEXT("NullRHI presentation returns no pixels"),
				Result.Pixels.Num(),
				0);
			Test->TestEqual(TEXT("NullRHI uploads no presentation page table"),
				Result.FinalPageTableUploadCount,
				uint64(0));
			return true;
		}

	private:
		TSharedPtr<FContext> Context;
		FAutomationTestBase* Test = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P3NullRHIPresentationFailClosedTest,
	"SightWeave.M3P3.NullRHI.PresentationFailClosedNoGPUAllocation",
	SightWeave::M3P3::NullRHITests::TestFlags)

bool FSightWeaveM3P3NullRHIPresentationFailClosedTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P3::NullRHITests;
	if (!GUsingNullRHI)
	{
		AddInfo(TEXT("NullRHI-only M3.3 assertions skipped on a rendered RHI"));
		return true;
	}
	const TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet =
		BuildPacket(this);
	if (!Packet.IsValid())
	{
		return false;
	}
	const FSightWeaveViewPresentationSelection Selection =
		FSightWeaveViewPresentationSelection::Enabled(
			Packet->GetWorldIdentity(),
			FSightWeaveKnowledgeOwnerId(FName(TEXT("NullPresentationOwner"))),
			FSightWeaveFloorId(FName(TEXT("NullPresentationFloor"))),
			ESightWeaveRenderPrecisionTier::Standard,
			1);
	const TSharedPtr<FContext> Context = MakeShared<FContext>();
	Context->Request = FSightWeavePresentationTestReadback::Start(
		Packet,
		Selection,
		{ FVector2f(200.0f, 200.0f) },
		{ FVector4f(1.0f, 1.0f, 1.0f, 1.0f) });
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForNullPresentation(Context, this));
	return true;
}

#endif
