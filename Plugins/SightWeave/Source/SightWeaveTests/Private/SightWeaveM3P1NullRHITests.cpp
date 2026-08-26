#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "RHI.h"
#include "SightWeaveRenderPacket.h"
#include "SightWeaveRenderTestReadback.h"

namespace SightWeave::M3P1::NullRHITests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FSightWeaveRenderPacketBuildResult MakePacket()
	{
		FSightWeaveRenderPacketBuildInput Input;
		Input.WorldIdentity.Serial = 9001;
		Input.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("NullOwner")));
		Input.FloorId = FSightWeaveFloorId(FName(TEXT("NullFloor")));
		Input.CompatibilityProfile = FSightWeaveRenderProfileIdentity::FromProfile(
			FSightWeaveIlluminationCompatibilityProfile());
		Input.PacketRevision = 1;
		Input.RegistryRevision = 2;
		Input.PublishedSnapshotRevision = 3;
		Input.PhysicalWorldBounds = FBox2D(FVector2D::ZeroVector, FVector2D(2560.0, 2560.0));
		Input.DirtyReason = ESightWeaveRenderDirtyReason::ExplicitClear;
		return FSightWeaveRenderPacketBuilder::Build(Input);
	}

	struct FNullContext
	{
		TSharedPtr<FSightWeaveRenderTestReadback, ESPMode::ThreadSafe> Request;
		FSightWeaveRenderReadbackExpectation Expectation;
		double StartSeconds = FPlatformTime::Seconds();
	};

	class FWaitForNullRHI final : public IAutomationLatentCommand
	{
	public:
		FWaitForNullRHI(TSharedPtr<FNullContext> InContext, FAutomationTestBase* InTest)
			: Context(MoveTemp(InContext)), Test(InTest)
		{
		}

		virtual bool Update() override
		{
			Context->Request->Poll();
			if (!Context->Request->IsFinished())
			{
				if (FPlatformTime::Seconds() - Context->StartSeconds > 5.0)
				{
					Test->AddError(TEXT("NullRHI fail-closed request timed out"));
					return true;
				}
				return false;
			}
			FSightWeaveRenderReadbackResult Result;
			if (!Context->Request->TryTakeResult(Context->Expectation, Result))
			{
				Test->AddError(TEXT("NullRHI result was unavailable"));
				return true;
			}
			Test->TestEqual(TEXT("NullRHI is an explicit failed readback, never a false black success"),
				Result.Status,
				ESightWeaveRenderReadbackStatus::Failed);
			Test->TestEqual(TEXT("NullRHI availability is classified"),
				Result.Availability,
				ESightWeaveRenderAvailability::NullRHI);
			Test->TestEqual(TEXT("NullRHI creates no GPU pixels"), Result.Pixels.Num(), 0);
			return true;
		}

	private:
		TSharedPtr<FNullContext> Context;
		FAutomationTestBase* Test = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P1NullRHIFailClosedTest,
	"SightWeave.M3P1.NullRHI.FailClosedNoGPUWork",
	SightWeave::M3P1::NullRHITests::TestFlags)

bool FSightWeaveM3P1NullRHIFailClosedTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P1::NullRHITests;
	if (!GUsingNullRHI)
	{
		AddInfo(TEXT("NullRHI-only assertion skipped on a rendered RHI"));
		return true;
	}
	const FSightWeaveRenderPacketBuildResult Built = MakePacket();
	if (!TestTrue(TEXT("NullRHI test packet builds on CPU"), Built.Succeeded()))
	{
		return false;
	}
	TSharedPtr<FNullContext> Context = MakeShared<FNullContext>();
	Context->Request = FSightWeaveRenderTestReadback::Start(Built.Packet);
	Context->Expectation.WorldIdentity = Built.Packet->GetWorldIdentity();
	Context->Expectation.KnowledgeOwnerId = Built.Packet->GetKnowledgeOwnerId();
	Context->Expectation.FloorId = Built.Packet->GetFloorId();
	Context->Expectation.CompatibilityProfile = Built.Packet->GetCompatibilityProfile();
	Context->Expectation.PacketRevision = Built.Packet->GetPacketRevision();
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForNullRHI(Context, this));
	return true;
}

#endif
