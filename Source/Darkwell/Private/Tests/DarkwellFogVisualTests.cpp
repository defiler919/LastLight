#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"

namespace Darkwell::FogVisualTests
{
	constexpr EAutomationTestFlags Flags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FDarkwellFogVisualSourceSnapshot MakeBodySource(const FVector2D Center)
	{
		FDarkwellFogVisualSourceSnapshot Source;
		Source.BodyCenter = Center;
		Source.ConeOrigin = Center;
		Source.ConeForward = FVector2D(1.0, 0.0);
		Source.BodyRadiusCentimeters = 120.0f;
		Source.ConeRangeCentimeters = 1250.0f;
		Source.ConeHalfAngleDegrees = 45.0f;
		Source.AuthorityRevision = 1;
		Source.bConeLegallyLive = false;
		return Source;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellFogRememberedFromStartContractTest,
	"Darkwell.FogVisual.P1.RememberedFromStartContract",
	Darkwell::FogVisualTests::Flags)

bool FDarkwellFogRememberedFromStartContractTest::RunTest(const FString& Parameters)
{
	TestNotEqual(TEXT("UnknownUntilExplored remains a distinct future policy"),
		EDarkwellInitialKnowledgePolicy::UnknownUntilExplored,
		EDarkwellInitialKnowledgePolicy::RememberedFromStart);
	TestNotEqual(TEXT("FullyLive remains a distinct future policy"),
		EDarkwellInitialKnowledgePolicy::FullyLive,
		EDarkwellInitialKnowledgePolicy::RememberedFromStart);
	for (const float LiveCoverage : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
	{
		const float KnownCoverage = 1.0f;
		const float RememberedWeight = KnownCoverage - LiveCoverage;
		const float UnknownWeight = 1.0f - KnownCoverage;
		TestTrue(TEXT("Live plus Remembered is exactly one"),
			FMath::IsNearlyEqual(LiveCoverage + RememberedWeight, 1.0f));
		TestEqual(TEXT("RememberedFromStart has no Unknown weight"),
			UnknownWeight, 0.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellFogContinuousMappingTest,
	"Darkwell.FogVisual.P1.ContinuousMapping",
	Darkwell::FogVisualTests::Flags)

bool FDarkwellFogContinuousMappingTest::RunTest(const FString& Parameters)
{
	FDarkwellFogVisualMapping Mapping;
	Mapping.WorldMin = FVector2D(-1750.0, -1250.0);
	Mapping.WorldExtent = FVector2D(3500.0, 2500.0);
	Mapping.InvWorldExtent = FVector2D(1.0 / 3500.0, 1.0 / 2500.0);
	Mapping.TextureExtent = FIntPoint(1400, 1000);
	Mapping.CentimetersPerTexel = 2.5f;
	TestTrue(TEXT("P1 mapping is valid"), Mapping.IsValid());
	TestTrue(TEXT("World minimum maps exactly to zero"),
		Mapping.WorldToUV(Mapping.WorldMin).Equals(FVector2D::ZeroVector));
	TestTrue(TEXT("World maximum maps exactly to one"),
		Mapping.WorldToUV(Mapping.WorldMin + Mapping.WorldExtent)
			.Equals(FVector2D(1.0, 1.0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellFogSubTexelCoverageTest,
	"Darkwell.FogVisual.P1.RawCoverage.SubTexelContinuity",
	Darkwell::FogVisualTests::Flags)

bool FDarkwellFogSubTexelCoverageTest::RunTest(const FString& Parameters)
{
	using namespace Darkwell::FogVisualTests;
	constexpr float Texel = 2.5f;
	constexpr float Width = 2.5f;
	const FVector2D EdgePoint(120.0, 0.0);
	TArray<float> Coverages;
	for (const float TexelOffset : {-0.5f, -0.25f, 0.0f, 0.25f, 0.5f})
	{
		const FDarkwellFogVisualSourceSnapshot Source =
			MakeBodySource(FVector2D(TexelOffset * Texel, 0.0));
		Coverages.Add(FDarkwellContinuousVisibilityBuilder::EvaluateNoOcclusionCoverage(
			Source, EdgePoint, Width));
	}
	TestTrue(TEXT("Quarter-texel motion creates fractional raw coverage"),
		Coverages[1] > 0.0f && Coverages[1] < 0.5f
			&& Coverages[3] > 0.5f && Coverages[3] < 1.0f);
	for (int32 Index = 1; Index < Coverages.Num(); ++Index)
	{
		TestTrue(TEXT("Raw coverage changes monotonically every quarter texel"),
			Coverages[Index] > Coverages[Index - 1]);
	}
	TestTrue(TEXT("Half-texel samples reach the analytic endpoints"),
		FMath::IsNearlyZero(Coverages[0], 1.0e-5f)
			&& FMath::IsNearlyEqual(Coverages.Last(), 1.0f, 1.0e-5f));

	const FVector2D DiagonalEdge = FVector2D(120.0, 0.0).GetRotated(45.0);
	const FVector2D DiagonalStep = FVector2D(1.0, 1.0).GetSafeNormal() * (0.25f * Texel);
	const float DiagonalBefore =
		FDarkwellContinuousVisibilityBuilder::EvaluateNoOcclusionCoverage(
			MakeBodySource(-DiagonalStep), DiagonalEdge, Width);
	const float DiagonalAfter =
		FDarkwellContinuousVisibilityBuilder::EvaluateNoOcclusionCoverage(
			MakeBodySource(DiagonalStep), DiagonalEdge, Width);
	TestTrue(TEXT("Diagonal quarter-texel motion changes fractional coverage"),
		DiagonalBefore > 0.0f && DiagonalBefore < 0.5f
			&& DiagonalAfter > 0.5f && DiagonalAfter < 1.0f);
	return true;
}

#endif
