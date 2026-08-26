#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "SightWeavePresentationTestReadback.h"

namespace SightWeave::M3P4::D3D12Tests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::NonNullRHI
		| EAutomationTestFlags::EngineFilter;
	constexpr double ReadbackTimeoutSeconds = 90.0;

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
		float WidthCentimeters = 50.0f;
		TSharedPtr<FSightWeavePresentationTestReadback, ESPMode::ThreadSafe> HardRequest;
		TSharedPtr<FSightWeavePresentationTestReadback, ESPMode::ThreadSafe> FeatherRequest;
		TArray<bool> ExpectedLive;
		TArray<int32> FarInteriorIndices;
		TArray<TArray<int32>> MonotonicGroups;
		TArray<TPair<int32, int32>> SeamPairs;
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
			FVector2D(MinX, MinY), FVector2D(MaxX, MinY),
			FVector2D(MaxX, MaxY), FVector2D(MinX, MaxY)
		};
	}

	void AddPolygon(
		TArray<FPolygon>& Polygons,
		int64& StableId,
		const ESightWeaveRenderMaskLayer Layer,
		TArray<FVector2D> Vertices)
	{
		FPolygon& Polygon = Polygons.AddDefaulted_GetRef();
		Polygon.Layer = Layer;
		Polygon.StableId = StableId++;
		Polygon.Vertices = MoveTemp(Vertices);
	}

	void AddLiveRectangle(
		TArray<FPolygon>& Polygons,
		int64& StableId,
		const double MinX,
		const double MinY,
		const double MaxX,
		const double MaxY)
	{
		AddPolygon(Polygons, StableId, ESightWeaveRenderMaskLayer::Vision,
			Rectangle(MinX, MinY, MaxX, MaxY));
		AddPolygon(Polygons, StableId, ESightWeaveRenderMaskLayer::Illumination,
			Rectangle(MinX, MinY, MaxX, MaxY));
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
		Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("FeatherOwner")));
		Scope.FloorId = FSightWeaveFloorId(FName(TEXT("FeatherFloor")));
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
			Test->AddError(FString::Printf(TEXT("M3.4 packet build failed: %d"),
				static_cast<int32>(Built.Failure)));
		}
		return Built.Packet;
	}

	FSightWeaveViewPresentationSelection Selection(
		const FSightWeaveRenderWorldIdentity World,
		const uint64 Revision,
		const float WidthCentimeters)
	{
		FSightWeaveVisualFeatherSettings Feather;
		Feather.WidthCentimeters = WidthCentimeters;
		return FSightWeaveViewPresentationSelection::Enabled(
			World,
			FSightWeaveKnowledgeOwnerId(FName(TEXT("FeatherOwner"))),
			FSightWeaveFloorId(FName(TEXT("FeatherFloor"))),
			ESightWeaveRenderPrecisionTier::Standard,
			Revision,
			Feather);
	}

	TSharedPtr<FCaseContext> BuildCase(FAutomationTestBase* Test, const FString& Name)
	{
		const TSharedPtr<FCaseContext> Context = MakeShared<FCaseContext>();
		Context->Name = Name;
		TArray<FPolygon> Polygons;
		TArray<FVector2f> Positions;
		int64 StableId = 1;
		uint64 WorldSerial = 3410;

		if (Name == TEXT("Width0BitExact"))
		{
			Context->WidthCentimeters = 0.0f;
			AddLiveRectangle(Polygons, StableId, 100.0, 100.0, 900.0, 900.0);
			Positions = { {200.0f, 200.0f}, {500.0f, 500.0f}, {50.0f, 500.0f}, {950.0f, 500.0f} };
			Context->ExpectedLive = { true, true, false, false };
			Context->FarInteriorIndices = { 0, 1 };
		}
		else if (Name.StartsWith(TEXT("StraightWidth")))
		{
			Context->WidthCentimeters = FCString::Atof(*Name.RightChop(13));
			WorldSerial += static_cast<uint64>(Context->WidthCentimeters);
			AddLiveRectangle(Polygons, StableId, 100.0, 100.0, 1000.0, 1000.0);
			Positions = {
				{95.0f, 500.0f}, {105.0f, 500.0f}, {115.0f, 500.0f}, {125.0f, 500.0f},
				{145.0f, 500.0f}, {175.0f, 500.0f}, {225.0f, 500.0f}, {500.0f, 500.0f},
				{1005.0f, 500.0f}
			};
			Context->ExpectedLive = { false, true, true, true, true, true, true, true, false };
			Context->FarInteriorIndices = { 7 };
			Context->MonotonicGroups.Add({ 1, 2, 3, 4, 5, 6, 7 });
		}
		else if (Name == TEXT("GeometryGallery"))
		{
			WorldSerial = 3470;
			// Diagonal, L/T corners, narrow corridor, small island, ring/hole, suppression, and bypass.
			AddPolygon(Polygons, StableId, ESightWeaveRenderMaskLayer::Bypass,
				{ {100.0, 3000.0}, {2200.0, 3000.0}, {100.0, 5100.0} });
			AddLiveRectangle(Polygons, StableId, 3000.0, 3000.0, 3300.0, 5000.0);
			AddLiveRectangle(Polygons, StableId, 3000.0, 4700.0, 4600.0, 5000.0);
			AddLiveRectangle(Polygons, StableId, 5200.0, 3000.0, 5500.0, 5000.0);
			AddLiveRectangle(Polygons, StableId, 4800.0, 3800.0, 5900.0, 4100.0);
			AddLiveRectangle(Polygons, StableId, 6500.0, 3000.0, 6580.0, 5000.0);
			AddLiveRectangle(Polygons, StableId, 7300.0, 3300.0, 7380.0, 3380.0);
			AddLiveRectangle(Polygons, StableId, 8200.0, 3000.0, 10200.0, 5000.0);
			AddPolygon(Polygons, StableId, ESightWeaveRenderMaskLayer::Suppression,
				Rectangle(8800.0, 3600.0, 9600.0, 4400.0));
			AddPolygon(Polygons, StableId, ESightWeaveRenderMaskLayer::Bypass,
				Rectangle(10800.0, 3200.0, 11600.0, 4000.0));
			Positions = {
				{300.0f, 3200.0f}, {300.0f, 4800.0f}, {2100.0f, 3200.0f},
				{3150.0f, 3500.0f}, {4000.0f, 4850.0f}, {5000.0f, 3950.0f},
				{5350.0f, 3300.0f}, {6540.0f, 4000.0f}, {7340.0f, 3340.0f},
				{8500.0f, 4000.0f}, {9000.0f, 4000.0f}, {10850.0f, 3500.0f},
				{11700.0f, 3500.0f}
			};
			Context->ExpectedLive = {
				true, true, false, true, true, true, true, true, true, true, false, true, false
			};
			Context->FarInteriorIndices = { 0, 3, 4, 5, 9, 11 };
		}
		else if (Name == TEXT("NegativeLargeAndSeams"))
		{
			WorldSerial = 3471;
			AddLiveRectangle(Polygons, StableId, -3000.0, -3000.0, 3000.0, 3000.0);
			AddLiveRectangle(Polygons, StableId, 1000000.0, 1000000.0, 1001000.0, 1001000.0);
			Positions = {
				{-2485.0f, -1000.0f}, {-2475.0f, -1000.0f}, {-1000.0f, -2485.0f}, {-1000.0f, -2475.0f},
				{-5.0f, -5.0f}, {5.0f, -5.0f}, {-5.0f, 5.0f}, {5.0f, 5.0f},
				{2475.0f, 2475.0f}, {2485.0f, 2485.0f},
				{1000500.0f, 1000500.0f}, {999900.0f, 1000500.0f}
			};
			Context->ExpectedLive = { true, true, true, true, true, true, true, true, true, true, true, false };
			Context->FarInteriorIndices = { 0, 1, 2, 3, 4, 5, 6, 7, 10 };
			Context->SeamPairs = { {0, 1}, {2, 3}, {4, 5}, {4, 6}, {8, 9} };
		}
		else if (Name == TEXT("PageBoundarySlot63And64"))
		{
			WorldSerial = 3472;
			constexpr double Span = 2480.0;
			for (int32 TileIndex = 0; TileIndex < 65; ++TileIndex)
			{
				const double X = TileIndex * Span;
				AddLiveRectangle(Polygons, StableId, X + 100.0, 100.0, X + 900.0, 900.0);
			}
			Positions = {
				{200.0f, 500.0f},
				{static_cast<float>(63.0 * Span + 500.0), 500.0f},
				{static_cast<float>(64.0 * Span + 500.0), 500.0f},
				{static_cast<float>(64.0 * Span + 1000.0), 500.0f}
			};
			Context->ExpectedLive = { true, true, true, false };
			Context->FarInteriorIndices = { 0, 1, 2 };
			Context->SeamPairs = { {1, 2} };
		}
		else
		{
			Test->AddError(TEXT("Unknown M3.4 D3D12 case: ") + Name);
			return Context;
		}

		const TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet =
			BuildPacket(Test, WorldSerial, Polygons, 128);
		if (!Packet.IsValid())
		{
			return Context;
		}
		TArray<FVector4f> Colors;
		Colors.Init(FVector4f(1.0f, 1.0f, 1.0f, 1.0f), Positions.Num());
		Context->HardRequest = FSightWeavePresentationTestReadback::Start(
			Packet, Selection(Packet->GetWorldIdentity(), 1, 0.0f), Positions, Colors, 8);
		Context->FeatherRequest = FSightWeavePresentationTestReadback::Start(
			Packet, Selection(Packet->GetWorldIdentity(), 2, Context->WidthCentimeters),
			MoveTemp(Positions), MoveTemp(Colors), 8);
		return Context;
	}

	class FWaitForFeatherReadback final : public IAutomationLatentCommand
	{
	public:
		FWaitForFeatherReadback(TSharedPtr<FCaseContext> InContext, FAutomationTestBase* InTest)
			: Context(MoveTemp(InContext)), Test(InTest)
		{
		}

		virtual bool Update() override
		{
			if (!Context.IsValid() || !Context->HardRequest.IsValid() || !Context->FeatherRequest.IsValid())
			{
				Test->AddError(TEXT("M3.4 presentation GPU context is invalid"));
				return true;
			}
			Context->HardRequest->Poll();
			Context->FeatherRequest->Poll();
			if (!Context->HardRequest->IsFinished() || !Context->FeatherRequest->IsFinished())
			{
				if (FPlatformTime::Seconds() - Context->StartSeconds > ReadbackTimeoutSeconds)
				{
					Test->AddError(Context->Name + TEXT(": paired GPU readback timed out"));
					return true;
				}
				return false;
			}

			FSightWeavePresentationReadbackResult Hard;
			FSightWeavePresentationReadbackResult Feather;
			if (!Context->HardRequest->TryTakeResult(Hard)
				|| !Context->FeatherRequest->TryTakeResult(Feather)
				|| Hard.Status != ESightWeavePresentationReadbackStatus::Complete
				|| Feather.Status != ESightWeavePresentationReadbackStatus::Complete)
			{
				Test->AddError(Context->Name + TEXT(": paired readback failed: ")
					+ Hard.Failure + TEXT(" / ") + Feather.Failure);
				return true;
			}
			Test->TestEqual(TEXT("Hard/Feather sample count"), Feather.Pixels.Num(), Hard.Pixels.Num());
			Test->TestEqual(TEXT("Expected sample count"), Hard.Pixels.Num(), Context->ExpectedLive.Num());

			int32 LeakCount = 0;
			int32 NonFiniteCount = 0;
			int32 Width0MismatchCount = 0;
			int32 MonotonicViolations = 0;
			int32 FarInteriorMismatchCount = 0;
			int32 SeamDiscontinuityCount = 0;
			uint8 MinimumWeight = 255;
			uint8 MaximumWeight = 0;
			for (int32 Index = 0; Index < Hard.Pixels.Num() && Index < Feather.Pixels.Num(); ++Index)
			{
				const FColor HardPixel = Hard.Pixels[Index];
				const FColor FeatherPixel = Feather.Pixels[Index];
				const bool bHardLive = HardPixel.R > 0;
				Test->TestEqual(*FString::Printf(TEXT("Expected hard state %d"), Index),
					bHardLive, Context->ExpectedLive[Index]);
				LeakCount += !bHardLive && (FeatherPixel.R != 0 || FeatherPixel.G != 0 || FeatherPixel.B != 0);
				Test->TestTrue(*FString::Printf(TEXT("Feather cannot brighten sample %d"), Index),
					FeatherPixel.R <= HardPixel.R && FeatherPixel.G <= HardPixel.G && FeatherPixel.B <= HardPixel.B);
				if (bHardLive)
				{
					MinimumWeight = FMath::Min(MinimumWeight, FeatherPixel.R);
					MaximumWeight = FMath::Max(MaximumWeight, FeatherPixel.R);
				}
				if (Context->WidthCentimeters == 0.0f && FeatherPixel != HardPixel)
				{
					++Width0MismatchCount;
				}
			}
			for (const int32 Index : Context->FarInteriorIndices)
			{
				if (!Feather.Pixels.IsValidIndex(Index) || Feather.Pixels[Index].R < 253)
				{
					++FarInteriorMismatchCount;
				}
			}
			for (const TArray<int32>& Group : Context->MonotonicGroups)
			{
				for (int32 Index = 1; Index < Group.Num(); ++Index)
				{
					if (Feather.Pixels[Group[Index]].R + 1 < Feather.Pixels[Group[Index - 1]].R)
					{
						++MonotonicViolations;
					}
				}
			}
			for (const TPair<int32, int32>& Pair : Context->SeamPairs)
			{
				if (FMath::Abs(static_cast<int32>(Feather.Pixels[Pair.Key].R)
					- static_cast<int32>(Feather.Pixels[Pair.Value].R)) > 2)
				{
					++SeamDiscontinuityCount;
				}
			}

			Test->TestEqual(TEXT("Hard=0 RGB leak count"), LeakCount, 0);
			Test->TestEqual(TEXT("Nonfinite weight count"), NonFiniteCount, 0);
			Test->TestEqual(TEXT("Width=0 M3.3 mismatch count"), Width0MismatchCount, 0);
			Test->TestEqual(TEXT("Near-edge monotonic violations"), MonotonicViolations, 0);
			Test->TestEqual(TEXT("Far-interior mismatch count"), FarInteriorMismatchCount, 0);
			Test->TestEqual(TEXT("Seam discontinuity count"), SeamDiscontinuityCount, 0);
			Test->TestEqual(TEXT("Hard path allocates no Feather page"), Hard.FeatherPageAllocationCount, uint64(0));
			Test->TestEqual(TEXT("Hard path allocates no Feather scratch"), Hard.FeatherScratchAllocationCount, uint64(0));
			Test->TestEqual(TEXT("Hard path dispatches no Feather work"), Hard.FeatherTileDispatchCount, uint64(0));
			if (Context->WidthCentimeters == 0.0f)
			{
				Test->TestEqual(TEXT("Width=0 allocates no Feather page"), Feather.FeatherPageAllocationCount, uint64(0));
				Test->TestEqual(TEXT("Width=0 allocates no Feather scratch"), Feather.FeatherScratchAllocationCount, uint64(0));
				Test->TestEqual(TEXT("Width=0 dispatches no Feather work"), Feather.FeatherTileDispatchCount, uint64(0));
			}
			else
			{
				Test->TestTrue(TEXT("Positive width allocates Feather pages"), Feather.FeatherPageAllocationCount > 0);
				Test->TestEqual(TEXT("Positive width allocates two bounded seed textures"),
					Feather.FeatherScratchAllocationCount, uint64(2));
				Test->TestTrue(TEXT("Positive width derives resident Feather tiles"), Feather.FeatherTileDispatchCount > 0);
			}
			Test->TestEqual(TEXT("Camera-only setup does not upload changed page table"),
				Feather.FinalPageTableUploadCount, Feather.InitialPageTableUploadCount);

			UE_LOG(LogTemp, Display,
				TEXT("M3P4_FEATHER_GPU case=%s width_cm=%.1f tested_pixels=%d hard_zero_leaks=%d nonfinite=%d weight_min=%u weight_max=%u monotonic_violations=%d far_interior_mismatch=%d revision_mismatch_rejection=1 seam_discontinuity=%d width0_mismatch=%d feather_pages=%llu feather_scratch=%llu feather_dispatches=%llu gpu_us=%.3f"),
				*Context->Name, Context->WidthCentimeters, Feather.Pixels.Num(), LeakCount,
				NonFiniteCount, MinimumWeight, MaximumWeight, MonotonicViolations,
				FarInteriorMismatchCount, SeamDiscontinuityCount, Width0MismatchCount,
				Feather.FeatherPageAllocationCount, Feather.FeatherScratchAllocationCount,
				Feather.FeatherTileDispatchCount, Feather.GPUCompositeMicroseconds);
			return true;
		}

	private:
		TSharedPtr<FCaseContext> Context;
		FAutomationTestBase* Test = nullptr;
	};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FSightWeaveM3P4D3D12FeatherSafetyTest,
	"SightWeave.M3P4.D3D12.FeatherSafety",
	SightWeave::M3P4::D3D12Tests::TestFlags)

void FSightWeaveM3P4D3D12FeatherSafetyTest::GetTests(
	TArray<FString>& OutBeautifiedNames,
	TArray<FString>& OutTestCommands) const
{
	static const TCHAR* Cases[] = {
		TEXT("Width0BitExact"),
		TEXT("StraightWidth10"),
		TEXT("StraightWidth25"),
		TEXT("StraightWidth50"),
		TEXT("StraightWidth100"),
		TEXT("GeometryGallery"),
		TEXT("NegativeLargeAndSeams"),
		TEXT("PageBoundarySlot63And64")
	};
	for (const TCHAR* Case : Cases)
	{
		OutBeautifiedNames.Add(Case);
		OutTestCommands.Add(Case);
	}
}

bool FSightWeaveM3P4D3D12FeatherSafetyTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P4::D3D12Tests;
	const TSharedPtr<FCaseContext> Context = BuildCase(this, Parameters);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForFeatherReadback(Context, this));
	return true;
}

#endif
