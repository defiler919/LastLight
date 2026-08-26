#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "SightWeaveSparseAtlas.h"
#include "SightWeaveSparseAtlasTestReadback.h"

namespace SightWeave::M3P2::PerformanceTests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	struct FDistribution
	{
		double P50 = 0.0;
		double P95 = 0.0;
		double P99 = 0.0;
		double Maximum = 0.0;
	};

	FDistribution Distribution(TArray<double> Samples)
	{
		FDistribution Result;
		if (Samples.IsEmpty())
		{
			return Result;
		}
		Samples.Sort();
		auto AtPercentile = [&Samples](const double Percentile)
		{
			const int32 Index = FMath::Clamp(
				FMath::CeilToInt32(Percentile * Samples.Num()) - 1,
				0,
				Samples.Num() - 1);
			return Samples[Index];
		};
		Result.P50 = AtPercentile(0.50);
		Result.P95 = AtPercentile(0.95);
		Result.P99 = AtPercentile(0.99);
		Result.Maximum = Samples.Last();
		return Result;
	}

	FSightWeaveRenderProfileIdentity CommonProfile()
	{
		return FSightWeaveRenderProfileIdentity::FromProfile(
			FSightWeaveIlluminationCompatibilityProfile());
	}

	FSightWeaveSparseRenderPacketBuildInput MakeInput(
		const int32 SourceCount,
		const uint64 Revision = 1,
		const TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe>& Previous = nullptr)
	{
		FSightWeaveSparseRenderPacketBuildInput Input;
		Input.WorldIdentity.Serial = 6101;
		Input.PacketRevision = Revision;
		Input.RegistryRevision = Revision;
		Input.PublishedSnapshotRevision = Revision;
		Input.PreviousPacket = Previous;
		FSightWeaveSparseScopeBuildInput& Scope = Input.Scopes.AddDefaulted_GetRef();
		Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("PerfOwner")));
		Scope.FloorId = FSightWeaveFloorId(FName(TEXT("PerfFloor")));
		Scope.PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
		Scope.MaximumActiveTiles = 128;
		const FSightWeaveRenderProfileIdentity Profile = CommonProfile();
		for (int32 Index = 0; Index < SourceCount; ++Index)
		{
			const double X = 50.0 + static_cast<double>(Index % 8) * 240.0;
			const double Y = 50.0 + static_cast<double>(Index / 8) * 240.0;
			FSightWeaveSparsePolygonInput& Polygon = Scope.Polygons.AddDefaulted_GetRef();
			Polygon.StableSourceId = Index + 1;
			Polygon.SourceRevision = Revision;
			Polygon.Layer = ESightWeaveRenderMaskLayer::Bypass;
			Polygon.CompatibilityProfile = Profile;
			Polygon.WorldVertices = {
				FVector2D(X, Y),
				FVector2D(X + 160.0, Y),
				FVector2D(X + 160.0, Y + 160.0),
				FVector2D(X, Y + 160.0)
			};
		}
		return Input;
	}

	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> BuildSubmitPacket(
		FAutomationTestBase* Test,
		const int32 DirtyTileCount)
	{
		FSightWeaveSparseRenderPacketBuildInput Initial = MakeInput(1);
		FSightWeaveSparseScopeBuildInput& Scope = Initial.Scopes[0];
		Scope.Polygons[0].WorldVertices = {
			FVector2D(1.0, 1.0),
			FVector2D(DirtyTileCount * 2480.0, 1.0),
			FVector2D(DirtyTileCount * 2480.0, 100.0),
			FVector2D(1.0, 100.0)
		};
		const FSightWeaveSparseRenderPacketBuildResult Built =
			FSightWeaveSparseRenderPacketBuilder::Build(Initial);
		if (!Built.Succeeded())
		{
			Test->AddError(TEXT("M3.2 submit benchmark packet failed to build"));
			return nullptr;
		}
		return Built.Packet;
	}

	void LogDistribution(
		FAutomationTestBase* Test,
		const TCHAR* Prefix,
		const TCHAR* Case,
		const int32 Samples,
		const FDistribution& Value)
	{
		Test->AddInfo(FString::Printf(
			TEXT("%s case=%s samples=%d p50_us=%.3f p95_us=%.3f p99_us=%.3f max_us=%.3f"),
			Prefix,
			Case,
			Samples,
			Value.P50,
			Value.P95,
			Value.P99,
			Value.Maximum));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P2PacketAndSubmitPerformanceTest,
	"SightWeave.M3P2.Performance.PacketBuildAndGameThreadSubmit",
	SightWeave::M3P2::PerformanceTests::TestFlags)

bool FSightWeaveM3P2PacketAndSubmitPerformanceTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P2::PerformanceTests;
	constexpr int32 WarmupCount = 256;
	constexpr int32 SampleCount = 2048;
	for (const int32 SourceCount : { 2, 8, 32 })
	{
		const FSightWeaveSparseRenderPacketBuildInput Input = MakeInput(SourceCount);
		for (int32 Index = 0; Index < WarmupCount; ++Index)
		{
			if (!FSightWeaveSparseRenderPacketBuilder::Build(Input).Succeeded())
			{
				AddError(TEXT("M3.2 packet benchmark warmup failed"));
				return false;
			}
		}
		TArray<double> Samples;
		Samples.Reserve(SampleCount);
		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			const double StartSeconds = FPlatformTime::Seconds();
			const FSightWeaveSparseRenderPacketBuildResult Built =
				FSightWeaveSparseRenderPacketBuilder::Build(Input);
			Samples.Add((FPlatformTime::Seconds() - StartSeconds) * 1000000.0);
			if (!Built.Succeeded())
			{
				AddError(TEXT("M3.2 packet benchmark sample failed"));
				return false;
			}
		}
		const FDistribution Stats = Distribution(MoveTemp(Samples));
		LogDistribution(
			this,
			TEXT("M3P2_GT_PACKET_BUILD"),
			*FString::Printf(TEXT("sources_%d"), SourceCount),
			SampleCount,
			Stats);
		TestTrue(
			*FString::Printf(TEXT("%d-source packet p95 remains under the frozen 250 us budget"), SourceCount),
			Stats.P95 < 250.0);
	}

	const TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> OneDirty =
		BuildSubmitPacket(this, 1);
	const TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> EightDirty =
		BuildSubmitPacket(this, 8);
	if (!OneDirty.IsValid() || !EightDirty.IsValid())
	{
		return false;
	}
	FSightWeaveSparseRenderPacketBuildInput NoChangeInput = MakeInput(1, 2, OneDirty);
	NoChangeInput.Scopes[0].Polygons[0].WorldVertices = {
		FVector2D(1.0, 1.0),
		FVector2D(2480.0, 1.0),
		FVector2D(2480.0, 100.0),
		FVector2D(1.0, 100.0)
	};
	const FSightWeaveSparseRenderPacketBuildResult NoChangeBuilt =
		FSightWeaveSparseRenderPacketBuilder::Build(NoChangeInput);
	if (!NoChangeBuilt.Succeeded() || NoChangeBuilt.Packet->HasMaskWork())
	{
		AddError(TEXT("M3.2 no-change submit benchmark packet is invalid or dirty"));
		return false;
	}
	struct FSubmitCase
	{
		const TCHAR* Name;
		TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet;
	};
	const FSubmitCase SubmitCases[] = {
		{ TEXT("no_change"), NoChangeBuilt.Packet },
		{ TEXT("one_dirty_tile"), OneDirty },
		{ TEXT("eight_dirty_tiles"), EightDirty }
	};
	for (const FSubmitCase& SubmitCase : SubmitCases)
	{
		TArray<double> Samples =
			FSightWeaveSparseAtlasTestReadback::BenchmarkGameThreadSubmitMicroseconds(
				SubmitCase.Packet,
				WarmupCount,
				SampleCount);
		const FDistribution Stats = Distribution(MoveTemp(Samples));
		LogDistribution(
			this,
			TEXT("M3P2_GT_SUBMIT"),
			SubmitCase.Name,
			SampleCount,
			Stats);
		TestTrue(
			*FString::Printf(TEXT("%s submit p95 remains under 250 us"), SubmitCase.Name),
			Stats.P95 < 250.0);
	}
	return true;
}

#endif
