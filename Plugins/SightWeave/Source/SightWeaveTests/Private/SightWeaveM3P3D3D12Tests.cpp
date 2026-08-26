#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "SightWeavePresentationTestReadback.h"

namespace SightWeave::M3P3::D3D12Tests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::NonNullRHI
		| EAutomationTestFlags::EngineFilter;
	constexpr double ReadbackTimeoutSeconds = 30.0;

	struct FPolygon
	{
		ESightWeaveRenderMaskLayer Layer = ESightWeaveRenderMaskLayer::Vision;
		int64 StableId = 0;
		TArray<FVector2D> Vertices;
	};

	struct FCaseContext
	{
		FString Name;
		double StartSeconds = FPlatformTime::Seconds();
		TSharedPtr<FSightWeavePresentationTestReadback, ESPMode::ThreadSafe> Request;
		TArray<bool> ExpectedLive;
		TArray<uint8> ExpectedGray;
		bool bExpectStablePageTable = true;
	};

	FSightWeaveRenderProfileIdentity Profile()
	{
		FSightWeaveIlluminationCompatibilityProfile Source;
		Source.AcceptedCapabilities.Add(FName(TEXT("Visible")));
		return FSightWeaveRenderProfileIdentity::FromProfile(Source);
	}

	TArray<FVector2D> Rectangle(
		const double MinX,
		const double MinY,
		const double MaxX,
		const double MaxY)
	{
		return {
			FVector2D(MinX, MinY),
			FVector2D(MaxX, MinY),
			FVector2D(MaxX, MaxY),
			FVector2D(MinX, MaxY)
		};
	}

	void AddLiveRectangle(
		TArray<FPolygon>& Polygons,
		int64& StableId,
		const double MinX,
		const double MinY,
		const double MaxX,
		const double MaxY)
	{
		FPolygon& Vision = Polygons.AddDefaulted_GetRef();
		Vision.Layer = ESightWeaveRenderMaskLayer::Vision;
		Vision.StableId = StableId++;
		Vision.Vertices = Rectangle(MinX, MinY, MaxX, MaxY);
		FPolygon& Illumination = Polygons.AddDefaulted_GetRef();
		Illumination.Layer = ESightWeaveRenderMaskLayer::Illumination;
		Illumination.StableId = StableId++;
		Illumination.Vertices = Rectangle(MinX, MinY, MaxX, MaxY);
	}

	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> BuildPacket(
		FAutomationTestBase* Test,
		const uint64 WorldSerial,
		const TArray<FPolygon>& Polygons,
		const int32 Capacity = SightWeave::SparseAtlas::StandardActiveTileCapacity)
	{
		const FSightWeaveRenderProfileIdentity CompatibilityProfile = Profile();
		FSightWeaveSparseRenderPacketBuildInput Input;
		Input.WorldIdentity.Serial = WorldSerial;
		Input.PacketRevision = 1;
		Input.RegistryRevision = 2;
		Input.PublishedSnapshotRevision = 3;
		FSightWeaveSparseScopeBuildInput& Scope = Input.Scopes.AddDefaulted_GetRef();
		Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("PresentationOwner")));
		Scope.FloorId = FSightWeaveFloorId(FName(TEXT("PresentationFloor")));
		Scope.PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
		Scope.MaximumActiveTiles = Capacity;
		for (const FPolygon& Source : Polygons)
		{
			FSightWeaveSparsePolygonInput& Destination = Scope.Polygons.AddDefaulted_GetRef();
			Destination.StableSourceId = Source.StableId;
			Destination.SourceRevision = 1;
			Destination.Layer = Source.Layer;
			Destination.CompatibilityProfile = CompatibilityProfile;
			Destination.WorldVertices = Source.Vertices;
		}
		const FSightWeaveSparseRenderPacketBuildResult Built =
			FSightWeaveSparseRenderPacketBuilder::Build(Input);
		if (!Built.Succeeded())
		{
			Test->AddError(FString::Printf(
				TEXT("M3.3 D3D12 packet failed to build: %d"),
				static_cast<int32>(Built.Failure)));
		}
		return Built.Packet;
	}

	TSharedPtr<FCaseContext> BuildCase(FAutomationTestBase* Test, const FString& Name)
	{
		const TSharedPtr<FCaseContext> Context = MakeShared<FCaseContext>();
		Context->Name = Name;
		TArray<FPolygon> Polygons;
		TArray<FVector2f> Positions;
		int64 StableId = 1;
		uint64 WorldSerial = 3310;
		bool bWrongScope = false;

		if (Name == TEXT("BasicHardComposite"))
		{
			AddLiveRectangle(Polygons, StableId, 100.0, 100.0, 800.0, 800.0);
			Positions = {
				FVector2f(200.0f, 200.0f),
				FVector2f(900.0f, 900.0f),
				FVector2f(3000.0f, 3000.0f)
			};
			Context->ExpectedLive = { true, false, false };
		}
		else if (Name == TEXT("NegativeLogicalTiles"))
		{
			WorldSerial = 3311;
			AddLiveRectangle(Polygons, StableId, -4900.0, -2400.0, -2600.0, -100.0);
			Positions = {
				FVector2f(-4000.0f, -1000.0f),
				FVector2f(-2500.0f, -1000.0f),
				FVector2f(-5000.0f, -1000.0f)
			};
			Context->ExpectedLive = { true, false, false };
		}
		else if (Name == TEXT("TileSeamsAndFourTileCorner"))
		{
			WorldSerial = 3312;
			AddLiveRectangle(Polygons, StableId, 2000.0, 2000.0, 3000.0, 3000.0);
			Positions = {
				FVector2f(2480.0f, 2200.0f),
				FVector2f(2200.0f, 2480.0f),
				FVector2f(2480.0f, 2480.0f),
				FVector2f(1990.0f, 2480.0f)
			};
			Context->ExpectedLive = { true, true, true, false };
		}
		else if (Name == TEXT("PageBoundarySlot63And64"))
		{
			WorldSerial = 3313;
			const double Span = 2480.0;
			for (int32 TileIndex = 0; TileIndex < 65; ++TileIndex)
			{
				const double X = TileIndex * Span;
				AddLiveRectangle(Polygons, StableId, X + 100.0, 100.0, X + 500.0, 500.0);
			}
			Positions = {
				FVector2f(200.0f, 200.0f),
				FVector2f(static_cast<float>(63.0 * Span + 200.0), 200.0f),
				FVector2f(static_cast<float>(64.0 * Span + 200.0), 200.0f),
				FVector2f(static_cast<float>(64.0 * Span + 800.0), 200.0f)
			};
			Context->ExpectedLive = { true, true, true, false };
		}
		else if (Name == TEXT("WrongScopeFailsBlack"))
		{
			WorldSerial = 3314;
			AddLiveRectangle(Polygons, StableId, 100.0, 100.0, 800.0, 800.0);
			Positions = { FVector2f(200.0f, 200.0f) };
			Context->ExpectedLive = { false };
			bWrongScope = true;
		}
		else if (Name == TEXT("OldWorldFailsBlack"))
		{
			WorldSerial = 3315;
			AddLiveRectangle(Polygons, StableId, 100.0, 100.0, 800.0, 800.0);
			Positions = { FVector2f(200.0f, 200.0f) };
			Context->ExpectedLive = { false };
		}
		else
		{
			Test->AddError(TEXT("Unknown M3.3 D3D12 case: ") + Name);
			return Context;
		}

		const TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet =
			BuildPacket(Test, WorldSerial, Polygons);
		if (!Packet.IsValid())
		{
			return Context;
		}
		FSightWeaveRenderWorldIdentity SelectionWorld = Packet->GetWorldIdentity();
		if (Name == TEXT("OldWorldFailsBlack"))
		{
			SelectionWorld.Serial += 1000;
		}
		const FSightWeaveViewPresentationSelection Selection =
			FSightWeaveViewPresentationSelection::Enabled(
				SelectionWorld,
				FSightWeaveKnowledgeOwnerId(FName(
					bWrongScope ? TEXT("OtherOwner") : TEXT("PresentationOwner"))),
				FSightWeaveFloorId(FName(TEXT("PresentationFloor"))),
				ESightWeaveRenderPrecisionTier::Standard,
				1);
		TArray<FVector4f> Colors;
		for (int32 Index = 0; Index < Positions.Num(); ++Index)
		{
			const uint8 Gray = static_cast<uint8>(64 + Index * 32);
			Context->ExpectedGray.Add(Gray);
			const float Value = static_cast<float>(Gray) / 255.0f;
			Colors.Emplace(Value, Value, Value, Value);
		}
		Context->Request = FSightWeavePresentationTestReadback::Start(
			Packet,
			Selection,
			MoveTemp(Positions),
			MoveTemp(Colors),
			8);
		return Context;
	}

	class FWaitForPresentationReadback final : public IAutomationLatentCommand
	{
	public:
		FWaitForPresentationReadback(
			TSharedPtr<FCaseContext> InContext,
			FAutomationTestBase* InTest)
			: Context(MoveTemp(InContext))
			, Test(InTest)
		{
		}

		virtual bool Update() override
		{
			if (!Context.IsValid() || !Context->Request.IsValid())
			{
				Test->AddError(TEXT("M3.3 presentation GPU context is invalid"));
				return true;
			}
			Context->Request->Poll();
			if (!Context->Request->IsFinished())
			{
				if (FPlatformTime::Seconds() - Context->StartSeconds > ReadbackTimeoutSeconds)
				{
					Test->AddError(Context->Name + TEXT(": presentation GPU readback timed out"));
					return true;
				}
				return false;
			}

			FSightWeavePresentationReadbackResult Result;
			if (!Context->Request->TryTakeResult(Result))
			{
				Test->AddError(Context->Name + TEXT(": completed readback result unavailable"));
				return true;
			}
			if (!Test->TestEqual(
				TEXT("Presentation readback completes"),
				Result.Status,
				ESightWeavePresentationReadbackStatus::Complete))
			{
				Test->AddError(Result.Failure);
				return true;
			}
			Test->TestEqual(TEXT("GPU output sample count"),
				Result.Pixels.Num(), Context->ExpectedLive.Num());
			for (int32 Index = 0;
				Index < Result.Pixels.Num() && Index < Context->ExpectedLive.Num();
				++Index)
			{
				const FColor Pixel = Result.Pixels[Index];
				if (Context->ExpectedLive[Index])
				{
					const int32 Expected = Context->ExpectedGray[Index];
					Test->TestTrue(
						*FString::Printf(TEXT("Live sample %d preserves scene color"), Index),
						FMath::Abs(static_cast<int32>(Pixel.R) - Expected) <= 1
							&& FMath::Abs(static_cast<int32>(Pixel.G) - Expected) <= 1
							&& FMath::Abs(static_cast<int32>(Pixel.B) - Expected) <= 1);
				}
				else
				{
					Test->TestTrue(
						*FString::Printf(TEXT("Black sample %d has no RGB leakage"), Index),
						Pixel.R == 0 && Pixel.G == 0 && Pixel.B == 0);
				}
			}
			Test->TestEqual(TEXT("Camera-only view setup does not upload page table"),
				Result.FinalPageTableUploadCount,
				Result.InitialPageTableUploadCount);
			UE_LOG(LogTemp, Display,
				TEXT("M3P3_PRESENTATION_GPU case=%s samples=%d page_table_uploads=%llu setup_us=%.3f gpu_timestamp=%s gpu_us=%.3f readback_us=%.3f"),
				*Context->Name,
				Result.Pixels.Num(),
				Result.FinalPageTableUploadCount,
				Result.RenderThreadCompositeSetupMicroseconds,
				Result.bGPUTimestampAvailable ? TEXT("true") : TEXT("false"),
				Result.GPUCompositeMicroseconds,
				Result.ReadbackEndToEndMicroseconds);
			return true;
		}

	private:
		TSharedPtr<FCaseContext> Context;
		FAutomationTestBase* Test = nullptr;
	};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FSightWeaveM3P3D3D12PresentationReadbackTest,
	"SightWeave.M3P3.D3D12.PresentationReadback",
	SightWeave::M3P3::D3D12Tests::TestFlags)

void FSightWeaveM3P3D3D12PresentationReadbackTest::GetTests(
	TArray<FString>& OutBeautifiedNames,
	TArray<FString>& OutTestCommands) const
{
	static const TCHAR* Cases[] = {
		TEXT("BasicHardComposite"),
		TEXT("NegativeLogicalTiles"),
		TEXT("TileSeamsAndFourTileCorner"),
		TEXT("PageBoundarySlot63And64"),
		TEXT("WrongScopeFailsBlack"),
		TEXT("OldWorldFailsBlack")
	};
	for (const TCHAR* Case : Cases)
	{
		OutBeautifiedNames.Add(Case);
		OutTestCommands.Add(Case);
	}
}

bool FSightWeaveM3P3D3D12PresentationReadbackTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P3::D3D12Tests;
	const TSharedPtr<FCaseContext> Context = BuildCase(this, Parameters);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPresentationReadback(Context, this));
	return true;
}

#endif
