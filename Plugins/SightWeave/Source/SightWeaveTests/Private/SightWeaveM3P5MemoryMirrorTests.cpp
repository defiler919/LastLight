#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/PlatformTime.h"
#include "RHIGlobals.h"
#include "Misc/AutomationTest.h"
#include "SightWeaveMemory.h"
#include "SightWeaveMemoryTestReadback.h"

namespace SightWeaveM3P5MemoryMirrorTests
{
	constexpr double ReadbackTimeoutSeconds = 30.0;
	const FSightWeaveKnowledgeOwnerId Owner(FName(TEXT("Local")));
	const FSightWeaveFloorId FloorId(FName(TEXT("Ground")));

	FSightWeaveFrameSnapshot MakeMirrorSnapshot()
	{
		FSightWeaveFrameSnapshot Snapshot;
		Snapshot.Revision = FSightWeaveRevision(1);
		Snapshot.bPublished = true;
		FSightWeaveFloorDefinition& Floor = Snapshot.Floors.AddDefaulted_GetRef();
		Floor.FloorId = FloorId;
		Floor.BoundsMin = FVector2D(-1000000.0, -1000000.0);
		Floor.BoundsMax = FVector2D(1000000.0, 1000000.0);
		Floor.HeightRange = { 0.0f, 300.0f };

		FSightWeaveVisionSnapshotEntry& Vision = Snapshot.VisionSources.AddDefaulted_GetRef();
		Vision.Handle = FSightWeaveVisionSourceHandle(91);
		Vision.Description.KnowledgeOwnerId = Owner;
		Vision.Description.FloorId = FloorId;
		Vision.Description.bActive = true;
		Vision.Description.IlluminationPolicy =
			ESightWeaveIlluminationPolicy::BypassLegalIllumination;
		Vision.Description.Compatibility.AcceptedCapabilities = { FName(TEXT("Visible")) };
		Vision.Description.Compatibility.Normalize();
		Vision.Polygon.SourceHandle = Vision.Handle;
		Vision.Polygon.KnowledgeOwnerId = Owner;
		Vision.Polygon.FloorId = FloorId;
		Vision.Polygon.Revision = FSightWeaveRevision(1);
		Vision.Polygon.SourceRevision = FSightWeaveRevision(1);
		Vision.Polygon.Vertices = {
			FVector(-1000100.0, -1000100.0, 100.0),
			FVector(-994000.0, -1000100.0, 100.0),
			FVector(-994000.0, -994000.0, 100.0),
			FVector(-1000100.0, -994000.0, 100.0)
		};
		Vision.SourceRevision = FSightWeaveRevision(1);
		return Snapshot;
	}

	struct FPacketFixture
	{
		TArray<TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe>> Packets;
		FSightWeaveMemoryTileKey SelectedTile;
	};

	bool BuildFixture(FPacketFixture& OutFixture)
	{
		const FSightWeaveFrameSnapshot Snapshot = MakeMirrorSnapshot();
		FSightWeaveMemoryScopeKey Scope;
		if (!FSightWeaveMemoryAuthority::BuildScopeForSnapshot(
				Snapshot,
				FSightWeaveRenderWorldIdentity { 9501 },
				9501,
				Owner,
				FloorId,
				ESightWeaveRenderPrecisionTier::Standard,
				Scope))
		{
			return false;
		}
		FSightWeaveMemoryAuthority Authority;
		if (!Authority.Configure(
				Scope,
				SightWeave::SparseAtlas::StandardActiveTileCapacity)
			|| !Authority.WriteEffectiveLive(Snapshot).Succeeded())
		{
			return false;
		}
		OutFixture.Packets.Add(Authority.PublishPacket());
		OutFixture.Packets.Add(Authority.PublishPacket());
		OutFixture.SelectedTile.Scope = Scope;
		OutFixture.SelectedTile.LogicalCoordinate = FIntPoint::ZeroValue;
		return OutFixture.Packets[0].IsValid()
			&& OutFixture.Packets[0]->IsValid()
			&& OutFixture.Packets[1].IsValid()
			&& OutFixture.Packets[1]->IsValid();
	}

	struct FContext
	{
		TSharedPtr<FSightWeaveMemoryTestReadback, ESPMode::ThreadSafe> Request;
		double StartSeconds = FPlatformTime::Seconds();
		bool bExpectNullRHI = false;
	};

	class FWaitForMemoryMirror final : public IAutomationLatentCommand
	{
	public:
		FWaitForMemoryMirror(TSharedPtr<FContext> InContext, FAutomationTestBase* InTest)
			: Context(MoveTemp(InContext))
			, Test(InTest)
		{
		}

		virtual bool Update() override
		{
			Context->Request->Poll();
			if (!Context->Request->IsFinished())
			{
				if (FPlatformTime::Seconds() - Context->StartSeconds > ReadbackTimeoutSeconds)
				{
					Test->AddError(TEXT("M3.5 memory mirror readback timed out"));
					return true;
				}
				return false;
			}
			FSightWeaveMemoryReadbackResult Result;
			if (!Context->Request->TryTakeResult(Result))
			{
				Test->AddError(TEXT("M3.5 memory mirror result unavailable"));
				return true;
			}
			Test->TestEqual(TEXT("Two publications were sampled"), Result.Updates.Num(), 2);
			if (Context->bExpectNullRHI)
			{
				Test->TestEqual(
					TEXT("NullRHI is explicit"),
					Result.Availability,
					ESightWeaveRenderAvailability::NullRHI);
				Test->TestEqual(
					TEXT("NullRHI readback fails closed"),
					Result.Status,
					ESightWeaveMemoryReadbackStatus::Failed);
				if (!Result.Updates.IsEmpty())
				{
					Test->TestEqual(TEXT("NullRHI uploads no tiles"), Result.Updates[0].UploadCount, uint64(0));
					Test->TestEqual(TEXT("NullRHI allocates no page"), Result.Updates[0].AllocatedPageCount, 0);
					Test->TestEqual(TEXT("NullRHI has no resident tile"), Result.Updates[0].ResidentTileCount, 0);
				}
				return true;
			}

			Test->TestEqual(
				TEXT("D3D12 mirror completes"),
				Result.Status,
				ESightWeaveMemoryReadbackStatus::Complete);
			Test->TestEqual(
				TEXT("D3D12 mirror is available"),
				Result.Availability,
				ESightWeaveRenderAvailability::Available);
			Test->TestEqual(TEXT("Mirror remains binary"), Result.NonBinaryTexelCount, 0);
			Test->TestEqual(
				TEXT("Interior and four-texel gutters reconstruct from neighboring authority"),
				Result.WhiteTexelCount,
				SightWeave::SparseAtlas::PhysicalTileSize * SightWeave::SparseAtlas::PhysicalTileSize);
			if (Result.Updates.Num() == 2)
			{
				Test->TestTrue(TEXT("Initial publication uploads resident tiles"), Result.Updates[0].UploadDelta > 0);
				Test->TestEqual(TEXT("No-change publication uploads zero tiles"), Result.Updates[1].UploadDelta, uint64(0));
				Test->TestFalse(TEXT("No-change publication schedules no mirror work"), Result.Updates[1].bProducedMirrorWork);
				Test->TestEqual(
					TEXT("No-change publication allocates no new page"),
					Result.Updates[1].AllocatedPageCount,
					Result.Updates[0].AllocatedPageCount);
				Test->TestEqual(
					TEXT("No-change publication preserves resource generation"),
					Result.Updates[1].ResourceGeneration,
					Result.Updates[0].ResourceGeneration);
			}
			return true;
		}

	private:
		TSharedPtr<FContext> Context;
		FAutomationTestBase* Test = nullptr;
	};
}

namespace SightWeaveM3P5MemoryMirrorTests
{

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P5MemoryMirrorNullRHITest,
	"SightWeave.M3P5.Memory.Mirror.NullRHIFailClosedNoAllocation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSightWeaveM3P5MemoryMirrorNullRHITest::RunTest(const FString& Parameters)
{
	if (!GUsingNullRHI)
	{
		AddInfo(TEXT("NullRHI-only M3.5 memory mirror assertions skipped on rendered RHI"));
		return true;
	}
	FPacketFixture Fixture;
	if (!TestTrue(TEXT("Memory mirror fixture builds"), BuildFixture(Fixture)))
	{
		return false;
	}
	const TSharedPtr<FContext> Context = MakeShared<FContext>();
	Context->bExpectNullRHI = true;
	Context->Request = FSightWeaveMemoryTestReadback::StartSequence(
		MoveTemp(Fixture.Packets),
		Fixture.SelectedTile);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForMemoryMirror(Context, this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P5MemoryMirrorD3D12Test,
	"SightWeave.M3P5.Memory.Mirror.D3D12PackedUploadGutterNoChange",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter
		| EAutomationTestFlags::NonNullRHI)

bool FSightWeaveM3P5MemoryMirrorD3D12Test::RunTest(const FString& Parameters)
{
	if (GUsingNullRHI)
	{
		AddInfo(TEXT("D3D12 M3.5 memory mirror assertions skipped on NullRHI"));
		return true;
	}
	FPacketFixture Fixture;
	if (!TestTrue(TEXT("Memory mirror fixture builds"), BuildFixture(Fixture)))
	{
		return false;
	}
	const TSharedPtr<FContext> Context = MakeShared<FContext>();
	Context->Request = FSightWeaveMemoryTestReadback::StartSequence(
		MoveTemp(Fixture.Packets),
		Fixture.SelectedTile);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForMemoryMirror(Context, this));
	return true;
}

}

#endif
