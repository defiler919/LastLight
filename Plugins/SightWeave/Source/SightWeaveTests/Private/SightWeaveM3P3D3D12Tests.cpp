#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/AllOf.h"
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
	constexpr int32 PresentationGPUWarmupCount = 8;
	constexpr int32 PresentationWarmSampleCount = 64;

	struct FPolygon
	{
		ESightWeaveRenderMaskLayer Layer = ESightWeaveRenderMaskLayer::Vision;
		int64 StableId = 0;
		uint64 SourceRevision = 1;
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
		const int32 Capacity = SightWeave::SparseAtlas::StandardActiveTileCapacity,
		const uint64 PacketRevision = 1,
		TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> PreviousPacket = nullptr)
	{
		const FSightWeaveRenderProfileIdentity CompatibilityProfile = Profile();
		FSightWeaveSparseRenderPacketBuildInput Input;
		Input.WorldIdentity.Serial = WorldSerial;
		Input.PacketRevision = PacketRevision;
		Input.RegistryRevision = PacketRevision + 1;
		Input.PublishedSnapshotRevision = PacketRevision + 2;
		Input.PreviousPacket = MoveTemp(PreviousPacket);
		FSightWeaveSparseScopeBuildInput& Scope = Input.Scopes.AddDefaulted_GetRef();
		Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("PresentationOwner")));
		Scope.FloorId = FSightWeaveFloorId(FName(TEXT("PresentationFloor")));
		Scope.PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
		Scope.MaximumActiveTiles = Capacity;
		for (const FPolygon& Source : Polygons)
		{
			FSightWeaveSparsePolygonInput& Destination = Scope.Polygons.AddDefaulted_GetRef();
			Destination.StableSourceId = Source.StableId;
			Destination.SourceRevision = Source.SourceRevision;
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
		float FeatherWidthCentimeters = 0.0f;
		FString UpdateMode = TEXT("NoChange");
		double BindingP50Microseconds = 0.0;
		double BindingP95Microseconds = 0.0;
		double BindingP99Microseconds = 0.0;
		double PacketBuildP50Microseconds = 0.0;
		double PacketBuildP95Microseconds = 0.0;
		double PacketBuildP99Microseconds = 0.0;
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

	double Percentile(TArray<double> Samples, const double Quantile)
	{
		Samples.Sort();
		return Samples.IsEmpty()
			? 0.0
			: Samples[FMath::Clamp(
				FMath::CeilToInt(Samples.Num() * Quantile) - 1,
				0,
				Samples.Num() - 1)];
	}

	FString FormatRawSamples(const TArray<double>& Samples)
	{
		FString Formatted;
		for (int32 Index = 0; Index < Samples.Num(); ++Index)
		{
			if (Index > 0)
			{
				Formatted += TEXT(",");
			}
			Formatted += FString::Printf(TEXT("%.3f"), Samples[Index]);
		}
		return Formatted;
	}

	TSharedPtr<FBenchmarkContext> BuildBenchmarkCase(
		FAutomationTestBase* Test,
		const FString& Name,
		const float FeatherWidthCentimeters = 0.0f,
		const FString& UpdateMode = TEXT("NoChange"))
	{
		const TSharedPtr<FBenchmarkContext> Context = MakeShared<FBenchmarkContext>();
		Context->Name = Name;
		Context->FeatherWidthCentimeters = FeatherWidthCentimeters;
		Context->UpdateMode = UpdateMode;
		const bool b1440 = Name.Contains(TEXT("1440p"));
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
		Context->BindingP50Microseconds = Percentile(BindingSamples, 0.50);
		Context->BindingP95Microseconds = Percentile(BindingSamples, 0.95);
		Context->BindingP99Microseconds = Percentile(MoveTemp(BindingSamples), 0.99);
		Test->TestTrue(TEXT("GT presentation selection p95 remains below 0.25 ms"),
			Context->BindingP95Microseconds < 250.0);
		UE_LOG(LogTemp, Display,
			TEXT("M3P3_GT_BINDING case=%s samples=2048 p50_us=%.3f p95_us=%.3f p99_us=%.3f"),
			*Name,
			Context->BindingP50Microseconds,
			Context->BindingP95Microseconds,
			Context->BindingP99Microseconds);

		const FSightWeaveViewPresentationSelection Selection =
			FSightWeaveViewPresentationSelection::Enabled(
				Packet->GetWorldIdentity(),
				FSightWeaveKnowledgeOwnerId(FName(TEXT("PresentationOwner"))),
				FSightWeaveFloorId(FName(TEXT("PresentationFloor"))),
				ESightWeaveRenderPrecisionTier::Standard,
				1,
				FSightWeaveVisualFeatherSettings{ FeatherWidthCentimeters });
		const FVector2f WorldStep(
			static_cast<float>(Context->ResidentTileCount * Span / Context->Extent.X),
			1000.0f / static_cast<float>(Context->Extent.Y));
		TArray<TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe>> WarmPackets;
		TArray<double> PacketBuildSamples;
		if (UpdateMode != TEXT("NoChange"))
		{
			constexpr int32 UpdateSampleCount =
				PresentationGPUWarmupCount + PresentationWarmSampleCount;
			WarmPackets.Reserve(UpdateSampleCount);
			PacketBuildSamples.Reserve(UpdateSampleCount);
			TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Previous = Packet;
			for (int32 SampleIndex = 0; SampleIndex < UpdateSampleCount; ++SampleIndex)
			{
				TArray<FPolygon> UpdatedPolygons = Polygons;
				const bool bAlternate = (SampleIndex & 1) != 0;
				int32 ModifiedSourceCount = 1;
				if (UpdateMode == TEXT("Dirty8"))
				{
					ModifiedSourceCount = FMath::Min(8, UpdatedPolygons.Num());
				}
				for (int32 SourceIndex = 0; SourceIndex < ModifiedSourceCount; ++SourceIndex)
				{
					FPolygon& Updated = UpdatedPolygons[SourceIndex];
					const double Offset = bAlternate ? 12.0 : -12.0;
					for (FVector2D& Vertex : Updated.Vertices)
					{
						Vertex.Y += Offset;
					}
					Updated.SourceRevision = static_cast<uint64>(SampleIndex + 2);
				}
				const double PacketBuildStart = FPlatformTime::Seconds();
				const TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> UpdatedPacket =
					BuildPacket(Test, WorldSerial, UpdatedPolygons, 128,
						static_cast<uint64>(SampleIndex + 2), Previous);
				PacketBuildSamples.Add((FPlatformTime::Seconds() - PacketBuildStart) * 1000000.0);
				if (!UpdatedPacket.IsValid())
				{
					break;
				}
				WarmPackets.Add(UpdatedPacket);
				Previous = UpdatedPacket;
			}
			Context->PacketBuildP50Microseconds = Percentile(PacketBuildSamples, 0.50);
			Context->PacketBuildP95Microseconds = Percentile(PacketBuildSamples, 0.95);
			Context->PacketBuildP99Microseconds = Percentile(PacketBuildSamples, 0.99);
			Test->TestTrue(TEXT("GT packet build and dirty expansion p95 remains below 0.25 ms"),
				Context->PacketBuildP95Microseconds < 250.0);
		}
		Context->Request = FSightWeavePresentationBenchmark::Start(
			Packet,
			Selection,
			Context->Extent,
			FVector2f::ZeroVector,
			WorldStep,
			PresentationGPUWarmupCount + PresentationWarmSampleCount,
			MoveTemp(WarmPackets));
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
			auto AfterWarmup = [](const TArray<double>& Samples)
			{
				TArray<double> Warmed;
				const int32 First = FMath::Min(PresentationGPUWarmupCount, Samples.Num());
				Warmed.Reserve(Samples.Num() - First);
				for (int32 Index = First; Index < Samples.Num(); ++Index)
				{
					Warmed.Add(Samples[Index]);
				}
				return Warmed;
			};
			const TArray<double> WarmViewSetup = AfterWarmup(Result.WarmRenderThreadViewSetupMicroseconds);
			const TArray<double> WarmPacketSubmit = AfterWarmup(Result.WarmRenderThreadPacketSubmitMicroseconds);
			const TArray<double> WarmMaskSetup = AfterWarmup(Result.WarmRenderThreadMaskSetupMicroseconds);
			const TArray<double> WarmFeatherSetup = AfterWarmup(Result.WarmRenderThreadFeatherSetupMicroseconds);
			const TArray<double> WarmCompositeSetup = AfterWarmup(Result.WarmRenderThreadCompositeSetupMicroseconds);
			const TArray<double> WarmGPUFeather = AfterWarmup(Result.WarmGPUFeatherMicroseconds);
			const TArray<double> WarmGPUComposite = AfterWarmup(Result.WarmGPUCompositeMicroseconds);
			const TArray<double> WarmGPUTotal = AfterWarmup(Result.WarmGPUTotalMicroseconds);
			const double ViewSetupP95 = Percentile95(WarmViewSetup);
			const double PacketSubmitP95 = Percentile95(WarmPacketSubmit);
			const double MaskSetupP95 = Percentile95(WarmMaskSetup);
			const double FeatherSetupP95 = Percentile95(WarmFeatherSetup);
			const double CompositeSetupP95 = Percentile95(WarmCompositeSetup);
			const double GPUFeatherP95 = Percentile95(WarmGPUFeather);
			const double GPUP95 = Percentile95(WarmGPUComposite);
			const double GPUTotalP95 = Percentile95(WarmGPUTotal);
			const double GPUTotalP50 = Percentile(WarmGPUTotal, 0.50);
			const double GPUTotalP99 = Percentile(WarmGPUTotal, 0.99);
			const double GPUTotalMax = Percentile(WarmGPUTotal, 1.00);
			TArray<double> RTTotalSamples;
			int32 RTTotalCount = Result.WarmRenderThreadPacketSubmitMicroseconds.Num();
			RTTotalCount = FMath::Min(
				RTTotalCount, Result.WarmRenderThreadMaskSetupMicroseconds.Num());
			RTTotalCount = FMath::Min(
				RTTotalCount, Result.WarmRenderThreadFeatherSetupMicroseconds.Num());
			RTTotalCount = FMath::Min(
				RTTotalCount, Result.WarmRenderThreadViewSetupMicroseconds.Num());
			RTTotalCount = FMath::Min(
				RTTotalCount, Result.WarmRenderThreadCompositeSetupMicroseconds.Num());
			RTTotalSamples.Reserve(RTTotalCount);
			for (int32 Index = PresentationGPUWarmupCount; Index < RTTotalCount; ++Index)
			{
				RTTotalSamples.Add(
					Result.WarmRenderThreadPacketSubmitMicroseconds[Index]
					+ Result.WarmRenderThreadMaskSetupMicroseconds[Index]
					+ Result.WarmRenderThreadFeatherSetupMicroseconds[Index]
					+ Result.WarmRenderThreadViewSetupMicroseconds[Index]
					+ Result.WarmRenderThreadCompositeSetupMicroseconds[Index]);
			}
			const double RTTotalP50 = Percentile(RTTotalSamples, 0.50);
			const double RTTotalP95 = Percentile(RTTotalSamples, 0.95);
			const double RTTotalP99 = Percentile(MoveTemp(RTTotalSamples), 0.99);
			const double ResolutionBudget = Context->Extent.Y == 1080 ? 1000.0 : 1500.0;
			const double PressureBudget = Context->Extent.Y == 1080 ? 2000.0 : 3000.0;
			Test->TestEqual(TEXT("Expected resident tile count"), Result.ResidentTileCount,
				Context->ResidentTileCount);
			if (Context->UpdateMode == TEXT("NoChange"))
			{
				Test->TestEqual(TEXT("Camera/view-only setup does not re-upload page table"),
					Result.FinalPageTableUploadCount, Result.InitialPageTableUploadCount);
				Test->TestEqual(TEXT("Warmed composite does not allocate atlas pages"),
					Result.FinalPageAllocationCount, Result.InitialPageAllocationCount);
				Test->TestEqual(TEXT("Warmed composite does not allocate scratch textures"),
					Result.FinalScratchAllocationCount, Result.InitialScratchAllocationCount);
				Test->TestEqual(TEXT("Warmed composite does not regenerate persistent resources"),
					Result.FinalResourceGeneration, Result.InitialResourceGeneration);
				Test->TestTrue(TEXT("Warmed no-change does not dispatch Feather work"),
					Algo::AllOf(Result.WarmFeatherTileDispatchCounts,
						[](const uint64 Count) { return Count == 0; }));
			}
			else
			{
				const int32 ExpectedDirtyTiles = Context->UpdateMode == TEXT("Dirty8") ? 8 : 1;
				Test->TestTrue(TEXT("Incremental packets contain the requested dirty tile count"),
					Algo::AllOf(Result.WarmRequestedDirtyTileCounts,
						[ExpectedDirtyTiles](const int32 Count) { return Count == ExpectedDirtyTiles; }));
				if (Context->FeatherWidthCentimeters > 0.0f)
				{
					Test->TestTrue(TEXT("Incremental changes dispatch bounded Feather work"),
						Algo::AllOf(Result.WarmFeatherTileDispatchCounts,
							[](const uint64 Count) { return Count > 0 && Count <= 128; }));
				}
			}
			if (Context->FeatherWidthCentimeters == 0.0f)
			{
				Test->TestEqual(TEXT("Width zero allocates no Feather pages"),
					Result.AllocatedFeatherPageCount, 0);
				Test->TestEqual(TEXT("Width zero allocates no Feather scratch"),
					Result.FeatherScratchAllocationCount, uint64(0));
				Test->TestEqual(TEXT("Width zero dispatches no Feather tiles"),
					Result.FinalFeatherTileDispatchCount, uint64(0));
			}
			else
			{
				Test->TestTrue(TEXT("Enabled Feather allocates derived pages"),
					Result.AllocatedFeatherPageCount > 0);
				Test->TestTrue(TEXT("Enabled Feather allocates two bounded scratch textures"),
					Result.FeatherScratchAllocationCount == 2);
			}
			Test->TestTrue(TEXT("GT submit remains below 0.25 ms"),
				Result.GameThreadSubmitMicroseconds < 250.0);
			Test->TestTrue(TEXT("RT packet submit remains below 0.20 ms"), PacketSubmitP95 < 200.0);
			Test->TestTrue(TEXT("RT mask dirty setup remains below 0.20 ms"), MaskSetupP95 < 200.0);
			Test->TestTrue(TEXT("RT Feather setup remains below 0.20 ms"), FeatherSetupP95 < 200.0);
			Test->TestTrue(TEXT("RT warmed composite setup remains below 0.20 ms"),
				CompositeSetupP95 < 200.0);
			Test->TestTrue(
				Context->SourceCount == 32
					? TEXT("32-source pressure meets advised budget")
					: TEXT("Warmed mask/Feather/composite meets resolution budget"),
				GPUTotalP95 < (Context->SourceCount == 32 ? PressureBudget : ResolutionBudget));
			Test->TestTrue(TEXT("Persistent live-mask allocation stays below 32 MiB"),
				Result.PersistentGPUBytes <= 32ull * 1024ull * 1024ull);
			UE_LOG(LogTemp, Display,
				TEXT("M3P4_PERF case=%s mode=%s width_cm=%.1f resolution=%dx%d sources=%d residents=%d warm_samples=%d gt_submit_us=%.3f gt_packet_build_p95_us=%.3f rt_bind_submit_us=%.3f cold_rt_setup_us=%.3f cold_gpu_feather_us=%.3f cold_gpu_composite_us=%.3f cold_gpu_total_us=%.3f warm_packet_submit_p95_us=%.3f warm_mask_setup_p95_us=%.3f warm_view_setup_p95_us=%.3f warm_feather_setup_p95_us=%.3f warm_composite_setup_p95_us=%.3f warm_gpu_feather_p95_us=%.3f warm_gpu_composite_p95_us=%.3f warm_gpu_total_p95_us=%.3f pages=%d feather_pages=%d persistent_bytes=%llu transient_output_bytes=%llu page_uploads=%llu allocations=%llu/%llu feather_allocations=%llu/%llu feather_dispatches=%llu/%llu resource_generation=%llu"),
				*Context->Name,
				*Context->UpdateMode,
				Context->FeatherWidthCentimeters,
				Context->Extent.X,
				Context->Extent.Y,
				Context->SourceCount,
				Result.ResidentTileCount,
				WarmGPUComposite.Num(),
				Result.GameThreadSubmitMicroseconds,
				Context->PacketBuildP95Microseconds,
				Result.RenderThreadBindingSubmitMicroseconds,
				Result.ColdRenderThreadSetupMicroseconds,
				Result.ColdGPUFeatherMicroseconds,
				Result.ColdGPUCompositeMicroseconds,
				Result.ColdGPUTotalMicroseconds,
				PacketSubmitP95,
				MaskSetupP95,
				ViewSetupP95,
				FeatherSetupP95,
				CompositeSetupP95,
				GPUFeatherP95,
				GPUP95,
				GPUTotalP95,
				Result.AllocatedPageCount,
				Result.AllocatedFeatherPageCount,
				Result.PersistentGPUBytes,
				Result.TransientOutputBytes,
				Result.FinalPageTableUploadCount,
				Result.FinalPageAllocationCount,
				Result.FinalScratchAllocationCount,
				Result.FeatherPageAllocationCount,
				Result.FeatherScratchAllocationCount,
				Result.InitialFeatherTileDispatchCount,
				Result.FinalFeatherTileDispatchCount,
				Result.FinalResourceGeneration);
			UE_LOG(LogTemp, Display,
				TEXT("M4P2_PRESENTATION_PERCENTILES case=%s mode=%s resolution=%dx%d sources=%d residents=%d samples=%d gpu_warmup=%d gt_binding_p50_us=%.3f gt_binding_p95_us=%.3f gt_binding_p99_us=%.3f gt_packet_build_p50_us=%.3f gt_packet_build_p95_us=%.3f gt_packet_build_p99_us=%.3f rt_total_p50_us=%.3f rt_total_p95_us=%.3f rt_total_p99_us=%.3f gpu_total_p50_us=%.3f gpu_total_p95_us=%.3f gpu_total_p99_us=%.3f gpu_total_max_us=%.3f"),
				*Context->Name,
				*Context->UpdateMode,
				Context->Extent.X,
				Context->Extent.Y,
				Context->SourceCount,
				Result.ResidentTileCount,
				WarmGPUTotal.Num(),
				PresentationGPUWarmupCount,
				Context->BindingP50Microseconds,
				Context->BindingP95Microseconds,
				Context->BindingP99Microseconds,
				Context->PacketBuildP50Microseconds,
				Context->PacketBuildP95Microseconds,
				Context->PacketBuildP99Microseconds,
				RTTotalP50,
				RTTotalP95,
				RTTotalP99,
				GPUTotalP50,
				GPUTotalP95,
				GPUTotalP99,
				GPUTotalMax);
			UE_LOG(LogTemp, Display,
				TEXT("M4P2_PRESENTATION_GPU_RAW case=%s mode=%s warmup_count=%d all_total_us=[%s] stable_total_us=[%s] stable_feather_us=[%s] stable_composite_us=[%s]"),
				*Context->Name,
				*Context->UpdateMode,
				PresentationGPUWarmupCount,
				*FormatRawSamples(Result.WarmGPUTotalMicroseconds),
				*FormatRawSamples(WarmGPUTotal),
				*FormatRawSamples(WarmGPUFeather),
				*FormatRawSamples(WarmGPUComposite));
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

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FSightWeaveM3P4D3D12FeatherPresentationPerformanceTest,
	"SightWeave.M3P4.D3D12.FeatherPresentationPerformance",
	SightWeave::M3P3::D3D12Tests::TestFlags)

void FSightWeaveM3P4D3D12FeatherPresentationPerformanceTest::GetTests(
	TArray<FString>& OutBeautifiedNames,
	TArray<FString>& OutTestCommands) const
{
	static const TCHAR* Cases[] = {
		TEXT("Width0.1080p.Tiles1Sources2"),
		TEXT("Width0.1080p.Tiles8Sources8"),
		TEXT("Width0.1080p.Tiles128Sources32"),
		TEXT("Width0.1440p.Tiles1Sources2"),
		TEXT("Width0.1440p.Tiles8Sources8"),
		TEXT("Width0.1440p.Tiles128Sources32"),
		TEXT("Width50.1080p.Tiles1Sources2"),
		TEXT("Width50.1080p.Tiles8Sources8"),
		TEXT("Width50.1080p.Tiles128Sources32"),
		TEXT("Width50.1440p.Tiles1Sources2"),
		TEXT("Width50.1440p.Tiles8Sources8"),
		TEXT("Width50.1440p.Tiles128Sources32"),
		TEXT("Width100.1080p.Tiles1Sources2"),
		TEXT("Width100.1080p.Tiles8Sources8"),
		TEXT("Width100.1080p.Tiles128Sources32"),
		TEXT("Width100.1440p.Tiles1Sources2"),
		TEXT("Width100.1440p.Tiles8Sources8"),
		TEXT("Width100.1440p.Tiles128Sources32"),
		TEXT("Width50.1080p.Dirty1.Tiles1Sources2"),
		TEXT("Width50.1080p.Dirty8.Tiles8Sources8"),
		TEXT("Width50.1080p.DynamicContinuous.Tiles8Sources8")
	};
	for (const TCHAR* Case : Cases)
	{
		OutBeautifiedNames.Add(Case);
		OutTestCommands.Add(Case);
	}
}

bool FSightWeaveM3P4D3D12FeatherPresentationPerformanceTest::RunTest(
	const FString& Parameters)
{
	using namespace SightWeave::M3P3::D3D12Tests;
	const float FeatherWidth = Parameters.StartsWith(TEXT("Width100"))
		? 100.0f
		: Parameters.StartsWith(TEXT("Width50")) ? 50.0f : 0.0f;
	FString UpdateMode = TEXT("NoChange");
	if (Parameters.Contains(TEXT("DynamicContinuous")))
	{
		UpdateMode = TEXT("DynamicContinuous");
	}
	else if (Parameters.Contains(TEXT("Dirty8")))
	{
		UpdateMode = TEXT("Dirty8");
	}
	else if (Parameters.Contains(TEXT("Dirty1")))
	{
		UpdateMode = TEXT("Dirty1");
	}
	const TSharedPtr<FBenchmarkContext> Context = BuildBenchmarkCase(
		this,
		Parameters,
		FeatherWidth,
		UpdateMode);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForPresentationBenchmark(Context, this));
	return true;
}

#endif
