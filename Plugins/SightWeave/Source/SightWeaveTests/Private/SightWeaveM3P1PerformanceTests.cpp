#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "SightWeaveRenderPacket.h"

namespace SightWeave::M3P1::PerformanceTests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr int32 WarmupIterations = 256;
	constexpr int32 SampleIterations = 2048;

	FSightWeaveRenderPacketBuildInput MakePressureInput(const int32 SourceCount)
	{
		FSightWeaveRenderPacketBuildInput Input;
		Input.WorldIdentity.Serial = 9001;
		Input.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("PerfOwner")));
		Input.FloorId = FSightWeaveFloorId(FName(TEXT("PerfFloor")));
		FSightWeaveIlluminationCompatibilityProfile Profile;
		Profile.AcceptedCapabilities.Add(FName(TEXT("Visible")));
		Input.CompatibilityProfile = FSightWeaveRenderProfileIdentity::FromProfile(Profile);
		Input.PacketRevision = 1;
		Input.RegistryRevision = 1;
		Input.PublishedSnapshotRevision = 1;
		Input.PhysicalWorldBounds = FBox2D(FVector2D::ZeroVector, FVector2D(2560.0, 2560.0));
		Input.DirtyReason = ESightWeaveRenderDirtyReason::SourceChanged;
		Input.Polygons.Reserve(SourceCount);
		for (int32 SourceIndex = 0; SourceIndex < SourceCount; ++SourceIndex)
		{
			const int32 Column = SourceIndex % 8;
			const int32 Row = SourceIndex / 8;
			const double MinX = 40.0 + Column * 290.0;
			const double MinY = 40.0 + Row * 520.0;
			FSightWeaveRenderPolygonInput& Polygon = Input.Polygons.AddDefaulted_GetRef();
			Polygon.StableSourceId = SourceIndex + 1;
			Polygon.Layer = static_cast<ESightWeaveRenderMaskLayer>(
				SourceIndex % static_cast<int32>(ESightWeaveRenderMaskLayer::Count));
			Polygon.KnowledgeOwnerId = Input.KnowledgeOwnerId;
			Polygon.FloorId = Input.FloorId;
			Polygon.CompatibilityProfile = Input.CompatibilityProfile;
			Polygon.WorldVertices = {
				FVector2D(MinX, MinY),
				FVector2D(MinX + 180.0, MinY),
				FVector2D(MinX + 180.0, MinY + 180.0),
				FVector2D(MinX, MinY + 180.0)
			};
		}
		return Input;
	}

	double Percentile(const TArray<double>& SortedSamples, const double Quantile)
	{
		const int32 Index = FMath::Clamp(
			FMath::CeilToInt(Quantile * SortedSamples.Num()) - 1,
			0,
			SortedSamples.Num() - 1);
		return SortedSamples[Index];
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P1PacketBuildPressureTest,
	"SightWeave.M3P1.Performance.PacketBuildPressure",
	SightWeave::M3P1::PerformanceTests::TestFlags)

bool FSightWeaveM3P1PacketBuildPressureTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P1::PerformanceTests;
	uint64 HashSink = 0;
	for (const int32 SourceCount : { 2, 8, 32 })
	{
		const FSightWeaveRenderPacketBuildInput Input = MakePressureInput(SourceCount);
		for (int32 Warmup = 0; Warmup < WarmupIterations; ++Warmup)
		{
			const FSightWeaveRenderPacketBuildResult Result = FSightWeaveRenderPacketBuilder::Build(Input);
			if (!Result.Succeeded())
			{
				AddError(FString::Printf(TEXT("%d-source packet warmup failed: %d"),
					SourceCount, static_cast<int32>(Result.Failure)));
				return false;
			}
			HashSink += Result.Packet->GetContentHash();
		}

		TArray<double> SamplesMicroseconds;
		SamplesMicroseconds.Reserve(SampleIterations);
		uint64 ExpectedHash = 0;
		int32 ExpectedVertices = INDEX_NONE;
		int32 ExpectedIndices = INDEX_NONE;
		for (int32 Sample = 0; Sample < SampleIterations; ++Sample)
		{
			const uint64 StartCycles = FPlatformTime::Cycles64();
			const FSightWeaveRenderPacketBuildResult Result = FSightWeaveRenderPacketBuilder::Build(Input);
			const uint64 EndCycles = FPlatformTime::Cycles64();
			if (!Result.Succeeded())
			{
				AddError(FString::Printf(TEXT("%d-source packet sample failed: %d"),
					SourceCount, static_cast<int32>(Result.Failure)));
				return false;
			}
			SamplesMicroseconds.Add(
				FPlatformTime::ToSeconds64(EndCycles - StartCycles) * 1000000.0);
			if (Sample == 0)
			{
				ExpectedHash = Result.Packet->GetContentHash();
				ExpectedVertices = Result.Packet->GetVertices().Num();
				ExpectedIndices = Result.Packet->GetIndices().Num();
			}
			else if (Result.Packet->GetContentHash() != ExpectedHash
				|| Result.Packet->GetVertices().Num() != ExpectedVertices
				|| Result.Packet->GetIndices().Num() != ExpectedIndices)
			{
				AddError(FString::Printf(
					TEXT("%d-source immutable payload/hash changed at sample %d"),
					SourceCount,
					Sample));
				return false;
			}
			HashSink += Result.Packet->GetContentHash();
		}

		SamplesMicroseconds.Sort();
		const double P50 = Percentile(SamplesMicroseconds, 0.50);
		const double P95 = Percentile(SamplesMicroseconds, 0.95);
		const double P99 = Percentile(SamplesMicroseconds, 0.99);
		const double Maximum = SamplesMicroseconds.Last();
		const uint64 PacketBufferBytes =
			static_cast<uint64>(ExpectedVertices) * sizeof(FVector2f)
			+ static_cast<uint64>(ExpectedIndices) * sizeof(uint32);
		AddInfo(FString::Printf(
			TEXT("M3P1_PRELIMINARY_PACKET_BUILD sources=%d warmup=%d samples=%d p50_us=%.3f p95_us=%.3f p99_us=%.3f max_us=%.3f vertices=%d indices=%d packet_buffer_bytes=%llu deterministic_hash=%llu suggested_gt_p95_under_250us=%s"),
			SourceCount,
			WarmupIterations,
			SampleIterations,
			P50,
			P95,
			P99,
			Maximum,
			ExpectedVertices,
			ExpectedIndices,
			PacketBufferBytes,
			ExpectedHash,
			P95 < 250.0 ? TEXT("true") : TEXT("false")));
	}
	TestNotEqual(TEXT("Benchmark results remain observable"), HashSink, uint64(0));
	return true;
}

#endif
