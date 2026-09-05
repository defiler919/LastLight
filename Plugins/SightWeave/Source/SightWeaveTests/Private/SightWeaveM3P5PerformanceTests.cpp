#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/PlatformTime.h"
#include "RHIGlobals.h"
#include "Misc/AutomationTest.h"
#include "SightWeaveMemory.h"
#include "SightWeaveMemoryTestReadback.h"
#include "SightWeaveSettings.h"

namespace SightWeave::M3P5::PerformanceTests
{
	constexpr int32 WarmupCount = 8;
	constexpr int32 DirtySampleCount = 64;
	constexpr int32 NoChangeSampleCount = 16;
	constexpr double ReadbackTimeoutSeconds = 60.0;
	constexpr double ReferenceRegionCentimeters = 2480.0;
	constexpr double CpuDirtyP95BudgetMicroseconds = 250.0;
	constexpr double GpuDirtyP95BudgetMicroseconds = 250.0;
	constexpr double GtPacketP95BudgetMicroseconds = 250.0;
	constexpr double RtSetupP95BudgetMicroseconds = 200.0;
	constexpr uint64 LivePersistentBudgetBytes = 32ull * 1024ull * 1024ull;
	constexpr uint64 PluginRuntimeBudgetBytes = 64ull * 1024ull * 1024ull;
	constexpr uint64 FrozenMeasuredLivePersistentBytes = 18697216ull;
	const FSightWeaveKnowledgeOwnerId Owner(FName(TEXT("Local")));
	const FSightWeaveFloorId FloorId(FName(TEXT("Ground")));

	struct FStatistics
	{
		double P50 = 0.0;
		double P95 = 0.0;
		double P99 = 0.0;
		double Max = 0.0;
	};

	FStatistics Summarize(TArray<double> Samples)
	{
		FStatistics Result;
		if (Samples.IsEmpty())
		{
			return Result;
		}
		Samples.Sort();
		auto Percentile = [&Samples](const double Quantile)
		{
			const int32 Index = FMath::Clamp(
				FMath::CeilToInt(Quantile * Samples.Num()) - 1,
				0,
				Samples.Num() - 1);
			return Samples[Index];
		};
		Result.P50 = Percentile(0.50);
		Result.P95 = Percentile(0.95);
		Result.P99 = Percentile(0.99);
		Result.Max = Samples.Last();
		return Result;
	}

	const TCHAR* TierName(const ESightWeaveRenderPrecisionTier Tier)
	{
		switch (Tier)
		{
		case ESightWeaveRenderPrecisionTier::Ultra: return TEXT("Ultra_2p5cm");
		case ESightWeaveRenderPrecisionTier::Fine: return TEXT("Fine_5cm");
		case ESightWeaveRenderPrecisionTier::Standard: return TEXT("Standard_10cm");
		case ESightWeaveRenderPrecisionTier::Coarse: return TEXT("Coarse_25cm");
		default: return TEXT("Invalid");
		}
	}

	bool ParseTier(const FString& Parameters, ESightWeaveRenderPrecisionTier& OutTier)
	{
		if (Parameters == TEXT("Ultra")) OutTier = ESightWeaveRenderPrecisionTier::Ultra;
		else if (Parameters == TEXT("Fine")) OutTier = ESightWeaveRenderPrecisionTier::Fine;
		else if (Parameters == TEXT("Standard")) OutTier = ESightWeaveRenderPrecisionTier::Standard;
		else if (Parameters == TEXT("Coarse")) OutTier = ESightWeaveRenderPrecisionTier::Coarse;
		else return false;
		return true;
	}

	FSightWeaveFrameSnapshot MakeSnapshot(
		const int64 Revision,
		const double ExploredWidth,
		const double ExploredHeight = ReferenceRegionCentimeters)
	{
		FSightWeaveFrameSnapshot Snapshot;
		Snapshot.Revision = FSightWeaveRevision(Revision);
		Snapshot.bPublished = true;
		FSightWeaveFloorDefinition& Floor = Snapshot.Floors.AddDefaulted_GetRef();
		Floor.FloorId = FloorId;
		Floor.BoundsMin = FVector2D::ZeroVector;
		Floor.BoundsMax = FVector2D(200000.0, 200000.0);
		Floor.HeightRange = { 0.0f, 300.0f };

		FSightWeaveVisionSnapshotEntry& Vision = Snapshot.VisionSources.AddDefaulted_GetRef();
		Vision.Handle = FSightWeaveVisionSourceHandle(1);
		Vision.Description.KnowledgeOwnerId = Owner;
		Vision.Description.FloorId = FloorId;
		Vision.Description.HeightRange = { 0.0f, 300.0f };
		Vision.Description.bActive = true;
		Vision.Description.IlluminationPolicy =
			ESightWeaveIlluminationPolicy::BypassLegalIllumination;
		Vision.Description.Compatibility.AcceptedCapabilities = { FName(TEXT("Visible")) };
		Vision.Description.Compatibility.Normalize();
		Vision.Polygon.SourceHandle = Vision.Handle;
		Vision.Polygon.KnowledgeOwnerId = Owner;
		Vision.Polygon.FloorId = FloorId;
		Vision.Polygon.Revision = FSightWeaveRevision(Revision);
		Vision.Polygon.SourceRevision = FSightWeaveRevision(Revision);
		Vision.Polygon.Vertices = {
			FVector(0.0, 0.0, 100.0),
			FVector(ExploredWidth, 0.0, 100.0),
			FVector(ExploredWidth, ExploredHeight, 100.0),
			FVector(0.0, ExploredHeight, 100.0)
		};
		Vision.SourceRevision = FSightWeaveRevision(Revision);
		return Snapshot;
	}

	struct FCPUResult
	{
		ESightWeaveRenderPrecisionTier Tier = ESightWeaveRenderPrecisionTier::Standard;
		FStatistics DirtyUpdate;
		FStatistics PacketPublish;
		FStatistics NoChangeUpdate;
		double ColdUpdateMicroseconds = 0.0;
		double ClearMicroseconds = 0.0;
		double BlockRegisterMicroseconds = 0.0;
		double SuppressRegisterMicroseconds = 0.0;
		int32 ResidentTileCount = 0;
		int64 PackedBytes = 0;
		int32 MaximumDirtyTiles = 0;
		uint64 EstimatedSnapshotBytes = 0;
		TArray<TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe>> Packets;
		FSightWeaveMemoryTileKey SelectedTile;
	};

	bool RunCPUExperiment(const ESightWeaveRenderPrecisionTier Tier, FCPUResult& Out)
	{
		Out.Tier = Tier;
		const double Step = ReferenceRegionCentimeters / DirtySampleCount;
		const FSightWeaveFrameSnapshot ScopeSnapshot = MakeSnapshot(1, Step);
		FSightWeaveMemoryScopeKey Scope;
		if (!FSightWeaveMemoryAuthority::BuildScopeForSnapshot(
				ScopeSnapshot,
				FSightWeaveRenderWorldIdentity { 35000 + static_cast<uint64>(Tier) },
				35000 + static_cast<uint64>(Tier),
				Owner,
				FloorId,
				Tier,
				Scope))
		{
			return false;
		}

		auto RunWarmup = [&Scope, Step]()
		{
			FSightWeaveMemoryAuthority WarmAuthority;
			if (!WarmAuthority.Configure(Scope, 256))
			{
				return false;
			}
			for (int32 Index = 0; Index < WarmupCount; ++Index)
			{
				if (!WarmAuthority.WriteEffectiveLive(
						MakeSnapshot(Index + 1, Step * (Index + 1))).Succeeded())
				{
					return false;
				}
				WarmAuthority.PublishPacket();
			}
			return true;
		};
		if (!RunWarmup())
		{
			return false;
		}

		FSightWeaveMemoryAuthority Authority;
		if (!Authority.Configure(Scope, 256))
		{
			return false;
		}
		TArray<double> DirtySamples;
		TArray<double> PublishSamples;
		DirtySamples.Reserve(DirtySampleCount);
		PublishSamples.Reserve(DirtySampleCount);
		for (int32 Index = 0; Index < DirtySampleCount; ++Index)
		{
			const FSightWeaveFrameSnapshot Snapshot =
				MakeSnapshot(Index + 1, Step * (Index + 1));
			const double UpdateStart = FPlatformTime::Seconds();
			const FSightWeaveMemoryUpdateDiagnostics Diagnostics =
				Authority.WriteEffectiveLive(Snapshot);
			const double UpdateMicroseconds =
				(FPlatformTime::Seconds() - UpdateStart) * 1000000.0;
			if (!Diagnostics.Succeeded())
			{
				return false;
			}
			if (Index == 0)
			{
				Out.ColdUpdateMicroseconds = UpdateMicroseconds;
			}
			if (Index > 0)
			{
				DirtySamples.Add(UpdateMicroseconds);
			}
			Out.MaximumDirtyTiles = FMath::Max(Out.MaximumDirtyTiles, Diagnostics.DirtyTileCount);
			const double PublishStart = FPlatformTime::Seconds();
			TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> Packet =
				Authority.PublishPacket();
			const double PublishMicroseconds =
				(FPlatformTime::Seconds() - PublishStart) * 1000000.0;
			if (Index > 0)
			{
				PublishSamples.Add(PublishMicroseconds);
			}
			if (!Packet.IsValid() || !Packet->IsValid())
			{
				return false;
			}
			Out.Packets.Add(MoveTemp(Packet));
		}

		TArray<double> NoChangeSamples;
		NoChangeSamples.Reserve(NoChangeSampleCount);
		const FSightWeaveFrameSnapshot FinalSnapshot =
			MakeSnapshot(DirtySampleCount, ReferenceRegionCentimeters);
		for (int32 Index = 0; Index < NoChangeSampleCount; ++Index)
		{
			const double Start = FPlatformTime::Seconds();
			const FSightWeaveMemoryUpdateDiagnostics Diagnostics =
				Authority.WriteEffectiveLive(FinalSnapshot);
			NoChangeSamples.Add((FPlatformTime::Seconds() - Start) * 1000000.0);
			if (!Diagnostics.Succeeded() || !Diagnostics.bDuplicateSnapshot)
			{
				return false;
			}
			TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> Packet =
				Authority.PublishPacket();
			if (!Packet.IsValid() || Packet->HasMirrorWork())
			{
				return false;
			}
			Out.Packets.Add(MoveTemp(Packet));
		}

		Out.DirtyUpdate = Summarize(MoveTemp(DirtySamples));
		Out.PacketPublish = Summarize(MoveTemp(PublishSamples));
		Out.NoChangeUpdate = Summarize(MoveTemp(NoChangeSamples));
		Out.ResidentTileCount = Authority.GetAllocatedTileCount();
		Out.PackedBytes = Authority.GetPackedAuthorityBytes();
		Out.EstimatedSnapshotBytes = static_cast<uint64>(Out.PackedBytes);
		const TConstArrayView<FSightWeavePackedMemoryTile> Tiles =
			Out.Packets[DirtySampleCount - 1]->GetAuthorityTiles();
		if (Tiles.IsEmpty())
		{
			return false;
		}
		Out.SelectedTile = Tiles[0].Key;

		FSightWeaveMemoryRegion Clear;
		Clear.Scope = Scope;
		Clear.HeightRange = { 0.0f, 300.0f };
		Clear.Shape = ESightWeaveMemoryRegionShape::AxisAlignedBox;
		Clear.Center = FVector2D(620.0, 620.0);
		Clear.HalfExtents = FVector2D(620.0, 620.0);
		const double ClearStart = FPlatformTime::Seconds();
		if (!Authority.ClearMemory(Clear))
		{
			return false;
		}
		Out.ClearMicroseconds = (FPlatformTime::Seconds() - ClearStart) * 1000000.0;

		FSightWeaveMemoryModifierDescription Block;
		Block.Operation = ESightWeaveMemoryModifierOperation::BlockMemoryWrites;
		Block.Region = Clear;
		Block.Region.Shape = ESightWeaveMemoryRegionShape::Circle;
		Block.Region.Center = FVector2D(1860.0, 620.0);
		Block.Region.Radius = 250.0f;
		const double BlockStart = FPlatformTime::Seconds();
		if (!Authority.RegisterModifier(Block).IsValid())
		{
			return false;
		}
		Out.BlockRegisterMicroseconds =
			(FPlatformTime::Seconds() - BlockStart) * 1000000.0;

		FSightWeaveMemoryModifierDescription Suppress = Block;
		Suppress.Operation = ESightWeaveMemoryModifierOperation::SuppressMemoryPresentation;
		Suppress.Region.Center = FVector2D(1860.0, 1860.0);
		const double SuppressStart = FPlatformTime::Seconds();
		if (!Authority.RegisterModifier(Suppress).IsValid())
		{
			return false;
		}
		Out.SuppressRegisterMicroseconds =
			(FPlatformTime::Seconds() - SuppressStart) * 1000000.0;
		return true;
	}

	struct FContext
	{
		FCPUResult CPU;
		TSharedPtr<FSightWeaveMemoryTestReadback, ESPMode::ThreadSafe> Request;
		double StartSeconds = FPlatformTime::Seconds();
	};

	class FWaitForMemoryPerformance final : public IAutomationLatentCommand
	{
	public:
		FWaitForMemoryPerformance(TSharedPtr<FContext> InContext, FAutomationTestBase* InTest)
			: Context(MoveTemp(InContext)), Test(InTest)
		{
		}

		virtual bool Update() override
		{
			Context->Request->Poll();
			if (!Context->Request->IsFinished())
			{
				if (FPlatformTime::Seconds() - Context->StartSeconds > ReadbackTimeoutSeconds)
				{
					Test->AddError(TEXT("M3.5 memory performance readback timed out"));
					return true;
				}
				return false;
			}

			FSightWeaveMemoryReadbackResult Result;
			if (!Context->Request->TryTakeResult(Result)
				|| Result.Status != ESightWeaveMemoryReadbackStatus::Complete)
			{
				Test->AddError(FString::Printf(TEXT("M3.5 memory performance readback failed: %s"),
					*Result.Failure));
				return true;
			}

			TArray<double> RTConsume;
			TArray<double> RTResidency;
			TArray<double> RTPageTable;
			TArray<double> RTTotal;
			TArray<double> GPU;
			uint64 UploadBytes = 0;
			int32 MaximumExpandedDirtyTiles = 0;
			uint64 PersistentGPUBytes = 0;
			int32 NoChangeWorkCount = 0;
			for (int32 Index = 0; Index < Result.Updates.Num(); ++Index)
			{
				const FSightWeaveMemoryMirrorUpdateSample& Sample = Result.Updates[Index];
				PersistentGPUBytes = FMath::Max(PersistentGPUBytes, Sample.PersistentGPUBytes);
				if (Index >= DirtySampleCount)
				{
					NoChangeWorkCount += Sample.bProducedMirrorWork || Sample.UploadDelta > 0;
					continue;
				}
				UploadBytes += Sample.UploadBytes;
				MaximumExpandedDirtyTiles = FMath::Max(
					MaximumExpandedDirtyTiles,
					static_cast<int32>(Sample.UploadDelta));
				if (Index < WarmupCount)
				{
					continue; // cold + declared RHI warmup are never mixed into warmed percentiles
				}
				RTConsume.Add(Sample.RenderThreadPacketConsumeMicroseconds);
				RTResidency.Add(Sample.RenderThreadResidencyUploadSetupMicroseconds);
				RTPageTable.Add(Sample.RenderThreadPageTableSetupMicroseconds);
				RTTotal.Add(Sample.RenderThreadTotalSetupMicroseconds);
				if (Sample.bGPUTimestampAvailable)
				{
					GPU.Add(Sample.GPUWorkMicroseconds);
				}
			}

			const int32 GPUTimestampSampleCount = GPU.Num();
			const FStatistics RTConsumeStats = Summarize(MoveTemp(RTConsume));
			const FStatistics RTResidencyStats = Summarize(MoveTemp(RTResidency));
			const FStatistics RTPageTableStats = Summarize(MoveTemp(RTPageTable));
			const FStatistics RTTotalStats = Summarize(MoveTemp(RTTotal));
			const FStatistics GPUStats = Summarize(MoveTemp(GPU));
			const FSightWeaveMemoryMirrorUpdateSample& Cold = Result.Updates[0];
			const uint64 WorstCaseMemoryPages = 2;
			const uint64 WorstCaseAttributePages = 2;
			const uint64 WorstCaseCPUBytes =
				128ull * SightWeave::Memory::PackedBytesPerTile;
			const uint64 WorstCasePluginRuntimeBytes = FrozenMeasuredLivePersistentBytes
				+ (WorstCaseMemoryPages + WorstCaseAttributePages) * SightWeave::SparseAtlas::PageBytes
				+ WorstCaseCPUBytes
				+ 2ull * 128ull * sizeof(FIntVector4);
			const double BoundaryErrorCentimeters =
				0.5 * SightWeaveCentimetersPerTexel(Context->CPU.Tier);
			const bool bCpuPass =
				Context->CPU.DirtyUpdate.P95 < CpuDirtyP95BudgetMicroseconds;
			const bool bGtPass =
				Context->CPU.PacketPublish.P95 < GtPacketP95BudgetMicroseconds;
			const bool bRtPass = RTTotalStats.P95 < RtSetupP95BudgetMicroseconds;
			const bool bGpuPass =
				!GPUStats.Max || GPUStats.P95 < GpuDirtyP95BudgetMicroseconds;
			const bool bMemoryPass =
				FrozenMeasuredLivePersistentBytes <= LivePersistentBudgetBytes
				&& WorstCasePluginRuntimeBytes <= PluginRuntimeBudgetBytes;
			const bool bEligibleThisRun =
				bCpuPass && bGtPass && bRtPass && bGpuPass && bMemoryPass;

			Test->AddInfo(FString::Printf(
				TEXT("M3P5_MEMORY_PERF tier=%s cm_per_texel=%.1f region_cm=2480x2480 dirty_samples=%d gpu_warmup=%d gpu_warm_samples=%d cpu_tiles=%d cpu_bytes=%lld max_dirty_tiles=%d boundary_error_cm=%.2f cpu_dirty_p50_us=%.3f cpu_dirty_p95_us=%.3f cpu_dirty_max_us=%.3f cpu_nochange_p50_us=%.3f cpu_nochange_p95_us=%.3f cpu_nochange_max_us=%.3f gt_publish_p50_us=%.3f gt_publish_p95_us=%.3f gt_publish_max_us=%.3f clear_us=%.3f block_register_us=%.3f suppress_register_us=%.3f cold_cpu_us=%.3f cold_rt_us=%.3f cold_gpu_us=%.3f rt_consume_p50_us=%.3f rt_consume_p95_us=%.3f rt_consume_max_us=%.3f rt_residency_upload_p50_us=%.3f rt_residency_upload_p95_us=%.3f rt_residency_upload_max_us=%.3f rt_page_table_p50_us=%.3f rt_page_table_p95_us=%.3f rt_page_table_max_us=%.3f rt_total_p50_us=%.3f rt_total_p95_us=%.3f rt_total_max_us=%.3f gpu_dirty_p50_us=%.3f gpu_dirty_p95_us=%.3f gpu_dirty_max_us=%.3f gpu_timestamp_samples=%d upload_bytes=%llu max_expanded_dirty_tiles=%d gpu_persistent_bytes=%llu estimated_snapshot_bytes=%llu nochange_work=%d worst_plugin_runtime_bytes=%llu eligible_this_run=%d cpu_pass=%d gt_pass=%d rt_pass=%d gpu_pass=%d memory_pass=%d"),
				TierName(Context->CPU.Tier),
				SightWeaveCentimetersPerTexel(Context->CPU.Tier),
				DirtySampleCount - 1,
				WarmupCount,
				GPUTimestampSampleCount,
				Context->CPU.ResidentTileCount,
				Context->CPU.PackedBytes,
				Context->CPU.MaximumDirtyTiles,
				BoundaryErrorCentimeters,
				Context->CPU.DirtyUpdate.P50, Context->CPU.DirtyUpdate.P95, Context->CPU.DirtyUpdate.Max,
				Context->CPU.NoChangeUpdate.P50, Context->CPU.NoChangeUpdate.P95, Context->CPU.NoChangeUpdate.Max,
				Context->CPU.PacketPublish.P50, Context->CPU.PacketPublish.P95, Context->CPU.PacketPublish.Max,
				Context->CPU.ClearMicroseconds,
				Context->CPU.BlockRegisterMicroseconds,
				Context->CPU.SuppressRegisterMicroseconds,
				Context->CPU.ColdUpdateMicroseconds,
				Cold.RenderThreadTotalSetupMicroseconds,
				Cold.GPUWorkMicroseconds,
				RTConsumeStats.P50, RTConsumeStats.P95, RTConsumeStats.Max,
				RTResidencyStats.P50, RTResidencyStats.P95, RTResidencyStats.Max,
				RTPageTableStats.P50, RTPageTableStats.P95, RTPageTableStats.Max,
				RTTotalStats.P50, RTTotalStats.P95, RTTotalStats.Max,
				GPUStats.P50, GPUStats.P95, GPUStats.Max,
				GPUTimestampSampleCount,
				UploadBytes,
				MaximumExpandedDirtyTiles,
				PersistentGPUBytes,
				Context->CPU.EstimatedSnapshotBytes,
				NoChangeWorkCount,
				WorstCasePluginRuntimeBytes,
				bEligibleThisRun ? 1 : 0,
				bCpuPass ? 1 : 0,
				bGtPass ? 1 : 0,
				bRtPass ? 1 : 0,
				bGpuPass ? 1 : 0,
				bMemoryPass ? 1 : 0));
			Test->AddInfo(FString::Printf(
				TEXT("M4P2_MEMORY_PERCENTILES tier=%s cpu_dirty_p99_us=%.3f cpu_nochange_p99_us=%.3f gt_publish_p99_us=%.3f rt_consume_p99_us=%.3f rt_residency_upload_p99_us=%.3f rt_page_table_p99_us=%.3f rt_total_p99_us=%.3f gpu_dirty_p99_us=%.3f"),
				TierName(Context->CPU.Tier),
				Context->CPU.DirtyUpdate.P99,
				Context->CPU.NoChangeUpdate.P99,
				Context->CPU.PacketPublish.P99,
				RTConsumeStats.P99,
				RTResidencyStats.P99,
				RTPageTableStats.P99,
				RTTotalStats.P99,
				GPUStats.P99));

			Test->TestEqual(TEXT("Warmed no-change performs no mirror work"), NoChangeWorkCount, 0);
			Test->TestTrue(TEXT("Frozen M3 live persistent memory remains within 32 MiB"),
				FrozenMeasuredLivePersistentBytes <= LivePersistentBudgetBytes);
			Test->TestTrue(TEXT("Worst-case SightWeave runtime memory remains within 64 MiB"),
				WorstCasePluginRuntimeBytes <= PluginRuntimeBudgetBytes);
			if (Context->CPU.Tier == ESightWeaveRenderPrecisionTier::Coarse)
			{
				Test->TestTrue(TEXT("Selected CPU memory dirty update p95 is below 0.25 ms"), bCpuPass);
				Test->TestTrue(TEXT("Selected GT immutable packet publish p95 is below 0.25 ms"), bGtPass);
				Test->TestTrue(TEXT("Selected RT memory dirty/setup p95 is below 0.20 ms"), bRtPass);
				Test->TestTrue(TEXT("Selected GPU memory dirty update p95 is below 0.25 ms"), bGpuPass);
				Test->TestEqual(TEXT("Selected production memory precision is the passing Coarse tier"),
					GetDefault<USightWeaveSettings>()->ExplorationMemoryPrecisionTier,
					ESightWeaveRenderPrecisionTier::Coarse);
			}
			else
			{
				Test->AddInfo(FString::Printf(
					TEXT("Candidate-only benchmark: %s is not the selected production precision; budget eligibility is recorded above without becoming a regression gate."),
					TierName(Context->CPU.Tier)));
			}
			return true;
		}

	private:
		TSharedPtr<FContext> Context;
		FAutomationTestBase* Test = nullptr;
	};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FSightWeaveM3P5MemoryPrecisionPerformanceTest,
	"SightWeave.M3P5.Performance.MemoryPrecision",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter
		| EAutomationTestFlags::NonNullRHI)

void FSightWeaveM3P5MemoryPrecisionPerformanceTest::GetTests(
	TArray<FString>& OutBeautifiedNames,
	TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames = { TEXT("2p5cm"), TEXT("5cm"), TEXT("10cm"), TEXT("25cm") };
	OutTestCommands = { TEXT("Ultra"), TEXT("Fine"), TEXT("Standard"), TEXT("Coarse") };
}

bool FSightWeaveM3P5MemoryPrecisionPerformanceTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P5::PerformanceTests;
	if (GUsingNullRHI)
	{
		AddError(TEXT("M3.5 memory precision performance requires real D3D12/SM6 timestamps."));
		return true;
	}
	ESightWeaveRenderPrecisionTier Tier;
	if (!TestTrue(TEXT("Precision command is valid"), ParseTier(Parameters, Tier)))
	{
		return false;
	}
	const TSharedPtr<FContext> Context = MakeShared<FContext>();
	if (!TestTrue(TEXT("CPU precision experiment succeeds"), RunCPUExperiment(Tier, Context->CPU)))
	{
		return false;
	}
	Context->Request = FSightWeaveMemoryTestReadback::StartSequence(
		MoveTemp(Context->CPU.Packets),
		Context->CPU.SelectedTile);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForMemoryPerformance(Context, this));
	return true;
}

namespace SightWeave::M3P5::PerformanceTests
{
	constexpr int32 ScaleSampleCount = 32;
	constexpr int32 ScaleWarmupCount = 8;

	struct FScaleCase
	{
		FString Name;
		int32 ResidentColumns = 1;
		int32 ResidentRows = 1;
		int32 DirtyColumns = 1;
		int32 DirtyRows = 1;

		int32 ResidentTiles() const { return ResidentColumns * ResidentRows; }
		int32 DirtyTiles() const { return DirtyColumns * DirtyRows; }
	};

	bool ParseScaleCase(const FString& Parameters, FScaleCase& Out)
	{
		Out.Name = Parameters;
		if (Parameters == TEXT("Resident1Dirty1"))
		{
			return true;
		}
		if (Parameters == TEXT("Resident8Dirty8"))
		{
			Out.ResidentColumns = 8;
			Out.DirtyColumns = 8;
			return true;
		}
		if (Parameters == TEXT("Resident128Dirty32"))
		{
			Out.ResidentColumns = 16;
			Out.ResidentRows = 8;
			Out.DirtyColumns = 16;
			Out.DirtyRows = 2;
			return true;
		}
		return false;
	}

	struct FScaleContext
	{
		FScaleCase Case;
		FStatistics CPUWrite;
		FStatistics CPUClear;
		FStatistics PacketPublish;
		int64 PackedBytes = 0;
		TSharedPtr<FSightWeaveMemoryTestReadback, ESPMode::ThreadSafe> Request;
		double StartSeconds = FPlatformTime::Seconds();
	};

	bool BuildScaleSequence(
		const FScaleCase& ScaleCase,
		FScaleContext& Out,
		TArray<TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe>>& OutPackets,
		FSightWeaveMemoryTileKey& OutSelectedTile)
	{
		Out.Case = ScaleCase;
		const double TileSpan = SightWeave::Memory::InteriorTileSize
			* SightWeaveCentimetersPerTexel(ESightWeaveRenderPrecisionTier::Coarse);
		const double ResidentWidth = ScaleCase.ResidentColumns * TileSpan;
		const double ResidentHeight = ScaleCase.ResidentRows * TileSpan;
		const double DirtyWidth = ScaleCase.DirtyColumns * TileSpan;
		const double DirtyHeight = ScaleCase.DirtyRows * TileSpan;
		const FSightWeaveFrameSnapshot Initial = MakeSnapshot(1, ResidentWidth, ResidentHeight);
		FSightWeaveMemoryScopeKey Scope;
		if (!FSightWeaveMemoryAuthority::BuildScopeForSnapshot(
				Initial,
				FSightWeaveRenderWorldIdentity { 36000 + static_cast<uint64>(ScaleCase.ResidentTiles()) },
				36000 + static_cast<uint64>(ScaleCase.ResidentTiles()),
				Owner,
				FloorId,
				ESightWeaveRenderPrecisionTier::Coarse,
				Scope))
		{
			return false;
		}

		FSightWeaveMemoryAuthority Authority;
		if (!Authority.Configure(Scope, 128)
			|| !Authority.WriteEffectiveLive(Initial).Succeeded())
		{
			return false;
		}
		TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> InitialPacket =
			Authority.PublishPacket(true);
		if (!InitialPacket.IsValid()
			|| InitialPacket->GetAuthorityTiles().Num() != ScaleCase.ResidentTiles())
		{
			return false;
		}
		OutPackets.Add(MoveTemp(InitialPacket));

		FSightWeaveMemoryRegion Clear;
		Clear.Scope = Scope;
		Clear.HeightRange = { 0.0f, 300.0f };
		Clear.Shape = ESightWeaveMemoryRegionShape::AxisAlignedBox;
		Clear.Center = FVector2D(DirtyWidth * 0.5, DirtyHeight * 0.5);
		Clear.HalfExtents = FVector2D(DirtyWidth * 0.5, DirtyHeight * 0.5);
		TArray<double> ClearSamples;
		TArray<double> WriteSamples;
		TArray<double> PublishSamples;
		for (int32 SampleIndex = 0; SampleIndex < ScaleSampleCount; ++SampleIndex)
		{
			const double ClearStart = FPlatformTime::Seconds();
			if (!Authority.ClearMemory(Clear))
			{
				return false;
			}
			const double ClearMicroseconds =
				(FPlatformTime::Seconds() - ClearStart) * 1000000.0;
			TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> ClearPacket =
				Authority.PublishPacket();
			if (!ClearPacket.IsValid()
				|| ClearPacket->GetRemovedTiles().Num() != ScaleCase.DirtyTiles())
			{
				return false;
			}
			OutPackets.Add(MoveTemp(ClearPacket));

			const FSightWeaveFrameSnapshot Reexplore =
				MakeSnapshot(SampleIndex + 2, DirtyWidth, DirtyHeight);
			const double WriteStart = FPlatformTime::Seconds();
			const FSightWeaveMemoryUpdateDiagnostics Diagnostics =
				Authority.WriteEffectiveLive(Reexplore);
			const double WriteMicroseconds =
				(FPlatformTime::Seconds() - WriteStart) * 1000000.0;
			if (!Diagnostics.Succeeded()
				|| Diagnostics.DirtyTileCount != ScaleCase.DirtyTiles())
			{
				return false;
			}
			const double PublishStart = FPlatformTime::Seconds();
			TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> WritePacket =
				Authority.PublishPacket();
			const double PublishMicroseconds =
				(FPlatformTime::Seconds() - PublishStart) * 1000000.0;
			if (!WritePacket.IsValid()
				|| WritePacket->GetDirtyTiles().Num() != ScaleCase.DirtyTiles())
			{
				return false;
			}
			OutPackets.Add(MoveTemp(WritePacket));
			if (SampleIndex >= ScaleWarmupCount)
			{
				ClearSamples.Add(ClearMicroseconds);
				WriteSamples.Add(WriteMicroseconds);
				PublishSamples.Add(PublishMicroseconds);
			}
		}

		Out.CPUClear = Summarize(MoveTemp(ClearSamples));
		Out.CPUWrite = Summarize(MoveTemp(WriteSamples));
		Out.PacketPublish = Summarize(MoveTemp(PublishSamples));
		Out.PackedBytes = Authority.GetPackedAuthorityBytes();
		const TConstArrayView<FSightWeavePackedMemoryTile> FinalTiles =
			Authority.PublishPacket()->GetAuthorityTiles();
		if (FinalTiles.IsEmpty())
		{
			return false;
		}
		OutSelectedTile = FinalTiles[0].Key;
		return true;
	}

	class FWaitForScalePerformance final : public IAutomationLatentCommand
	{
	public:
		FWaitForScalePerformance(TSharedPtr<FScaleContext> InContext, FAutomationTestBase* InTest)
			: Context(MoveTemp(InContext)), Test(InTest)
		{
		}

		virtual bool Update() override
		{
			Context->Request->Poll();
			if (!Context->Request->IsFinished())
			{
				if (FPlatformTime::Seconds() - Context->StartSeconds > ReadbackTimeoutSeconds)
				{
					Test->AddError(TEXT("M3.5 memory scale readback timed out"));
					return true;
				}
				return false;
			}

			FSightWeaveMemoryReadbackResult Result;
			if (!Context->Request->TryTakeResult(Result)
				|| Result.Status != ESightWeaveMemoryReadbackStatus::Complete)
			{
				Test->AddError(TEXT("M3.5 memory scale readback failed: ") + Result.Failure);
				return true;
			}

			TArray<double> RTClear;
			TArray<double> RTTotal;
			TArray<double> GPUClear;
			TArray<double> GPU;
			uint64 DirtyUploadBytes = 0;
			uint64 PersistentGPUBytes = 0;
			int32 MaximumExpandedDirtyTiles = 0;
			int32 MatchedClearSamples = 0;
			int32 MatchedWriteSamples = 0;
			for (int32 CycleIndex = 0; CycleIndex < ScaleSampleCount; ++CycleIndex)
			{
				const FSightWeaveMemoryMirrorUpdateSample& Clear = Result.Updates[1 + CycleIndex * 2];
				const FSightWeaveMemoryMirrorUpdateSample& Write = Result.Updates[2 + CycleIndex * 2];
				PersistentGPUBytes = FMath::Max(
					PersistentGPUBytes,
					FMath::Max(Clear.PersistentGPUBytes, Write.PersistentGPUBytes));
				MatchedClearSamples +=
					Clear.RequestedRemovedTileCount == Context->Case.DirtyTiles();
				MatchedWriteSamples +=
					Write.RequestedDirtyTileCount == Context->Case.DirtyTiles();
				if (CycleIndex < ScaleWarmupCount)
				{
					continue;
				}
				RTClear.Add(Clear.RenderThreadTotalSetupMicroseconds);
				RTTotal.Add(Write.RenderThreadTotalSetupMicroseconds);
				if (Clear.bGPUTimestampAvailable)
				{
					GPUClear.Add(Clear.GPUWorkMicroseconds);
				}
				if (Write.bGPUTimestampAvailable)
				{
					GPU.Add(Write.GPUWorkMicroseconds);
				}
				DirtyUploadBytes += Write.UploadBytes;
				MaximumExpandedDirtyTiles = FMath::Max(
					MaximumExpandedDirtyTiles,
					static_cast<int32>(Write.UploadDelta));
			}

			const FStatistics RTClearStats = Summarize(MoveTemp(RTClear));
			const FStatistics RTStats = Summarize(MoveTemp(RTTotal));
			const FStatistics GPUClearStats = Summarize(MoveTemp(GPUClear));
			const FStatistics GPUStats = Summarize(MoveTemp(GPU));
			Test->TestEqual(TEXT("Scale clear packets contain exact removed count"),
				MatchedClearSamples, ScaleSampleCount);
			Test->TestEqual(TEXT("Scale write packets contain exact dirty count"),
				MatchedWriteSamples, ScaleSampleCount);
			Test->TestEqual(TEXT("Scale final resident tile count"),
				Result.Updates.Last().ResidentTileCount, Context->Case.ResidentTiles());
			Test->TestTrue(TEXT("Scale persistent GPU memory remains within plugin budget"),
				PersistentGPUBytes <= PluginRuntimeBudgetBytes);
			Test->AddInfo(FString::Printf(
				TEXT("M3P5_MEMORY_SCALE case=%s residents=%d dirty_tiles=%d warm_samples=%d cpu_bytes=%lld cpu_clear_p50_us=%.3f cpu_clear_p95_us=%.3f cpu_clear_max_us=%.3f cpu_write_p50_us=%.3f cpu_write_p95_us=%.3f cpu_write_max_us=%.3f gt_publish_p50_us=%.3f gt_publish_p95_us=%.3f gt_publish_max_us=%.3f rt_total_p50_us=%.3f rt_total_p95_us=%.3f rt_total_max_us=%.3f gpu_mirror_p50_us=%.3f gpu_mirror_p95_us=%.3f gpu_mirror_max_us=%.3f upload_bytes=%llu max_expanded_dirty_tiles=%d gpu_persistent_bytes=%llu"),
				*Context->Case.Name,
				Context->Case.ResidentTiles(),
				Context->Case.DirtyTiles(),
				ScaleSampleCount - ScaleWarmupCount,
				Context->PackedBytes,
				Context->CPUClear.P50, Context->CPUClear.P95, Context->CPUClear.Max,
				Context->CPUWrite.P50, Context->CPUWrite.P95, Context->CPUWrite.Max,
				Context->PacketPublish.P50, Context->PacketPublish.P95, Context->PacketPublish.Max,
				RTStats.P50, RTStats.P95, RTStats.Max,
				GPUStats.P50, GPUStats.P95, GPUStats.Max,
				DirtyUploadBytes,
				MaximumExpandedDirtyTiles,
				PersistentGPUBytes));
			Test->AddInfo(FString::Printf(
				TEXT("M4P2_MEMORY_SCALE_PERCENTILES case=%s cpu_clear_p99_us=%.3f cpu_write_p99_us=%.3f gt_publish_p99_us=%.3f rt_clear_p50_us=%.3f rt_clear_p95_us=%.3f rt_clear_p99_us=%.3f rt_write_p99_us=%.3f gpu_clear_p50_us=%.3f gpu_clear_p95_us=%.3f gpu_clear_p99_us=%.3f gpu_write_p99_us=%.3f"),
				*Context->Case.Name,
				Context->CPUClear.P99,
				Context->CPUWrite.P99,
				Context->PacketPublish.P99,
				RTClearStats.P50,
				RTClearStats.P95,
				RTClearStats.P99,
				RTStats.P99,
				GPUClearStats.P50,
				GPUClearStats.P95,
				GPUClearStats.P99,
				GPUStats.P99));
			return true;
		}

	private:
		TSharedPtr<FScaleContext> Context;
		FAutomationTestBase* Test = nullptr;
	};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FSightWeaveM3P5MemoryScalePerformanceTest,
	"SightWeave.M3P5.Performance.MemoryScale",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter
		| EAutomationTestFlags::NonNullRHI)

void FSightWeaveM3P5MemoryScalePerformanceTest::GetTests(
	TArray<FString>& OutBeautifiedNames,
	TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames = {
		TEXT("Resident1Dirty1"),
		TEXT("Resident8Dirty8"),
		TEXT("Resident128Dirty32")
	};
	OutTestCommands = OutBeautifiedNames;
}

bool FSightWeaveM3P5MemoryScalePerformanceTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P5::PerformanceTests;
	if (GUsingNullRHI)
	{
		AddError(TEXT("M3.5 memory scale performance requires real D3D12/SM6 timestamps."));
		return true;
	}
	FScaleCase ScaleCase;
	if (!TestTrue(TEXT("Memory scale command is valid"), ParseScaleCase(Parameters, ScaleCase)))
	{
		return false;
	}
	const TSharedPtr<FScaleContext> Context = MakeShared<FScaleContext>();
	TArray<TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe>> Packets;
	FSightWeaveMemoryTileKey SelectedTile;
	if (!TestTrue(TEXT("Memory scale packet sequence succeeds"),
		BuildScaleSequence(ScaleCase, *Context, Packets, SelectedTile)))
	{
		return false;
	}
	Context->Request = FSightWeaveMemoryTestReadback::StartSequence(
		MoveTemp(Packets), SelectedTile);
	Context->StartSeconds = FPlatformTime::Seconds();
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForScalePerformance(Context, this));
	return true;
}

#endif
