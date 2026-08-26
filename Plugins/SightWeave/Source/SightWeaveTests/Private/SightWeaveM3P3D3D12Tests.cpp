#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "SightWeavePresentationBenchmark.h"
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

	struct FBenchmarkContext
	{
		FString Name;
		double StartSeconds = FPlatformTime::Seconds();
		int32 SourceCount = 0;
		int32 ResidentTileCount = 0;
		FIntPoint Extent = FIntPoint::ZeroValue;
		TSharedPtr<FSightWeavePresentationBenchmark, ESPMode::ThreadSafe> Request;
	};

	double Percentile95(TArray<double> Samples)
	{
		Samples.Sort();
		return Samples.IsEmpty()
			? 0.0
			: Samples[FMath::Clamp(FMath::CeilToInt(Samples.Num() * 0.95) - 1, 0, Samples.Num() - 1)];
	}

	TSharedPtr<FBenchmarkContext> BuildBenchmarkCase(
		FAutomationTestBase* Test,
		const FString& Name)
	{
		const TSharedPtr<FBenchmarkContext> Context = MakeShared<FBenchmarkContext>();
		Context->Name = Name;
		const bool b1440 = Name.StartsWith(TEXT("1440p"));
		Context->Extent = b1440 ? FIntPoint(2560, 1440) : FIntPoint(1920, 1080);
		if (Name.EndsWith(TEXT("Tiles1Sources2")))
		{
			Context->ResidentTileCount = 1;
			Context->SourceCount = 2;
		}
		else if (Name.EndsWith(TEXT("Tiles8Sources8")))
		{
			Context->ResidentTileCount = 8;
			Context->SourceCount = 8;
		}
		else if (Name.EndsWith(TEXT("Tiles128Sources32")))
		{
			Context->ResidentTileCount = 128;
			Context->SourceCount = 32;
		}
		else
		{
			Test->AddError(TEXT("Unknown M3.3 benchmark case: ") + Name);
			return Context;
		}

		constexpr double Span = 2480.0;
		TArray<FPolygon> Polygons;
		for (int32 SourceIndex = 0; SourceIndex < Context->SourceCount; ++SourceIndex)
		{
			const int32 TilesPerSource = FMath::Max(
				1,
				Context->ResidentTileCount / Context->SourceCount);
			const int32 FirstTile = Context->ResidentTileCount < Context->SourceCount
				? SourceIndex % Context->ResidentTileCount
				: SourceIndex * TilesPerSource;
			const int32 LastTileExclusive = FMath::Min(
				Context->ResidentTileCount,
				FirstTile + TilesPerSource);
			FPolygon& Bypass = Polygons.AddDefaulted_GetRef();
			Bypass.Layer = ESightWeaveRenderMaskLayer::Bypass;
			Bypass.StableId = 1000 + SourceIndex;
			Bypass.Vertices = Rectangle(
				FirstTile * Span + 100.0,
				100.0,
				LastTileExclusive * Span - 100.0,
				600.0);
		}
		const uint64 WorldSerial = 3400
			+ static_cast<uint64>(b1440 ? 100 : 0)
			+ static_cast<uint64>(Context->ResidentTileCount);
		const TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet =
			BuildPacket(Test, WorldSerial, Polygons, 128);
		if (!Packet.IsValid())
		{
			return Context;
		}

		TArray<double> BindingSamples;
		BindingSamples.Reserve(2048);
		for (int32 Index = 0; Index < 2048; ++Index)
		{
			const double Start = FPlatformTime::Seconds();
			const FSightWeaveViewPresentationSelection Sample =
				FSightWeaveViewPresentationSelection::Enabled(
					Packet->GetWorldIdentity(),
					FSightWeaveKnowledgeOwnerId(FName(TEXT("PresentationOwner"))),
					FSightWeaveFloorId(FName(TEXT("PresentationFloor"))),
					ESightWeaveRenderPrecisionTier::Standard,
					static_cast<uint64>(Index + 1));
			BindingSamples.Add((FPlatformTime::Seconds() - Start) * 1000000.0);
			if (!Sample.IsValid())
			{
				Test->AddError(TEXT("Benchmark selection construction failed"));
				break;
			}
		}
		const double BindingP95 = Percentile95(MoveTemp(BindingSamples));
		Test->TestTrue(TEXT("GT presentation selection p95 remains below 0.25 ms"), BindingP95 < 250.0);
		UE_LOG(LogTemp, Display,
			TEXT("M3P3_GT_BINDING case=%s samples=2048 p95_us=%.3f"),
			*Name,
			BindingP95);

		const FSightWeaveViewPresentationSelection Selection =
			FSightWeaveViewPresentationSelection::Enabled(
				Packet->GetWorldIdentity(),
				FSightWeaveKnowledgeOwnerId(FName(TEXT("PresentationOwner"))),
				FSightWeaveFloorId(FName(TEXT("PresentationFloor"))),
				ESightWeaveRenderPrecisionTier::Standard,
				1);
		const FVector2f WorldStep(
			static_cast<float>(Context->ResidentTileCount * Span / Context->Extent.X),
			1000.0f / static_cast<float>(Context->Extent.Y));
		Context->Request = FSightWeavePresentationBenchmark::Start(
			Packet,
			Selection,
			Context->Extent,
			FVector2f::ZeroVector,
			WorldStep,
			64);
		return Context;
	}

	class FWaitForPresentationBenchmark final : public IAutomationLatentCommand
	{
	public:
		FWaitForPresentationBenchmark(
			TSharedPtr<FBenchmarkContext> InContext,
			FAutomationTestBase* InTest)
			: Context(MoveTemp(InContext)), Test(InTest)
		{
		}

		virtual bool Update() override
		{
			if (!Context.IsValid() || !Context->Request.IsValid())
			{
				Test->AddError(TEXT("M3.3 presentation benchmark context is invalid"));
				return true;
			}
			Context->Request->Poll();
			if (!Context->Request->IsFinished())
			{
				if (FPlatformTime::Seconds() - Context->StartSeconds > 90.0)
				{
					Test->AddError(Context->Name + TEXT(": presentation benchmark timed out"));
					return true;
				}
				return false;
			}

			FSightWeavePresentationBenchmarkResult Result;
			if (!Context->Request->TryTakeResult(Result)
				|| !Test->TestEqual(TEXT("Benchmark completes"), Result.Status,
					ESightWeavePresentationReadbackStatus::Complete))
			{
				Test->AddError(Result.Failure);
				return true;
			}
			const double ViewSetupP95 = Percentile95(Result.WarmRenderThreadViewSetupMicroseconds);
			const double CompositeSetupP95 = Percentile95(Result.WarmRenderThreadCompositeSetupMicroseconds);
			const double GPUP95 = Percentile95(Result.WarmGPUCompositeMicroseconds);
			const double ResolutionBudget = Context->Extent.Y == 1080 ? 1000.0 : 1500.0;
			const double PressureBudget = Context->Extent.Y == 1080 ? 2000.0 : 3000.0;
			Test->TestEqual(TEXT("Expected resident tile count"), Result.ResidentTileCount,
				Context->ResidentTileCount);
			Test->TestEqual(TEXT("Camera/view-only setup does not re-upload page table"),
				Result.FinalPageTableUploadCount, Result.InitialPageTableUploadCount);
			Test->TestEqual(TEXT("Warmed composite does not allocate atlas pages"),
				Result.FinalPageAllocationCount, Result.InitialPageAllocationCount);
			Test->TestEqual(TEXT("Warmed composite does not allocate scratch textures"),
				Result.FinalScratchAllocationCount, Result.InitialScratchAllocationCount);
			Test->TestEqual(TEXT("Warmed composite does not regenerate persistent resources"),
				Result.FinalResourceGeneration, Result.InitialResourceGeneration);
			Test->TestTrue(TEXT("GT submit remains below 0.25 ms"),
				Result.GameThreadSubmitMicroseconds < 250.0);
			Test->TestTrue(TEXT("RT warmed composite setup remains below 0.20 ms"),
				CompositeSetupP95 < 200.0);
			Test->TestTrue(TEXT("Warmed composite meets resolution budget"), GPUP95 < ResolutionBudget);
			if (Context->SourceCount == 32)
			{
				Test->TestTrue(TEXT("32-source pressure meets advised budget"), GPUP95 < PressureBudget);
			}
			Test->TestTrue(TEXT("Persistent live-mask allocation stays below 32 MiB"),
				Result.PersistentGPUBytes <= 32ull * 1024ull * 1024ull);
			UE_LOG(LogTemp, Display,
				TEXT("M3P3_PERF case=%s resolution=%dx%d sources=%d residents=%d warm_samples=%d gt_submit_us=%.3f rt_bind_submit_us=%.3f cold_rt_setup_us=%.3f cold_gpu_total_us=%.3f warm_view_setup_p95_us=%.3f warm_composite_setup_p95_us=%.3f warm_gpu_composite_p95_us=%.3f pages=%d persistent_bytes=%llu transient_output_bytes=%llu page_uploads=%llu allocations=%llu/%llu resource_generation=%llu"),
				*Context->Name,
				Context->Extent.X,
				Context->Extent.Y,
				Context->SourceCount,
				Result.ResidentTileCount,
				Result.WarmGPUCompositeMicroseconds.Num(),
				Result.GameThreadSubmitMicroseconds,
				Result.RenderThreadBindingSubmitMicroseconds,
				Result.ColdRenderThreadSetupMicroseconds,
				Result.ColdGPUTotalMicroseconds,
				ViewSetupP95,
				CompositeSetupP95,
				GPUP95,
				Result.AllocatedPageCount,
				Result.PersistentGPUBytes,
				Result.TransientOutputBytes,
				Result.FinalPageTableUploadCount,
				Result.FinalPageAllocationCount,
				Result.FinalScratchAllocationCount,
				Result.FinalResourceGeneration);
			return true;
		}

	private:
		TSharedPtr<FBenchmarkContext> Context;
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

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FSightWeaveM3P3D3D12PresentationPerformanceTest,
	"SightWeave.M3P3.D3D12.PresentationPerformance",
	SightWeave::M3P3::D3D12Tests::TestFlags)

void FSightWeaveM3P3D3D12PresentationPerformanceTest::GetTests(
	TArray<FString>& OutBeautifiedNames,
	TArray<FString>& OutTestCommands) const
{
	static const TCHAR* Cases[] = {
		TEXT("1080p.Tiles1Sources2"),
		TEXT("1080p.Tiles8Sources8"),
		TEXT("1080p.Tiles128Sources32"),
		TEXT("1440p.Tiles1Sources2"),
		TEXT("1440p.Tiles8Sources8"),
		TEXT("1440p.Tiles128Sources32")
	};
	for (const TCHAR* Case : Cases)
	{
		OutBeautifiedNames.Add(Case);
		OutTestCommands.Add(Case);
	}
}

bool FSightWeaveM3P3D3D12PresentationPerformanceTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P3::D3D12Tests;
	const TSharedPtr<FBenchmarkContext> Context = BuildBenchmarkCase(this, Parameters);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPresentationBenchmark(Context, this));
	return true;
}

#endif
