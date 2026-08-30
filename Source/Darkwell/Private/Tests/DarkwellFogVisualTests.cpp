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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellFogContinuousSegmentOcclusionTest,
	"Darkwell.FogVisual.P2.ContinuousSegmentOcclusion",
	Darkwell::FogVisualTests::Flags)

bool FDarkwellFogContinuousSegmentOcclusionTest::RunTest(const FString& Parameters)
{
	const FDarkwellFogVisualSegment Wall{
		FVector2D(0.0, -100.0),
		FVector2D(0.0, 100.0)
	};
	const TArray<FDarkwellFogVisualSegment> Segments{Wall};
	TestTrue(TEXT("Finite non-degenerate segment is valid"), Wall.IsValid());
	TestTrue(TEXT("Free space directly behind the wall is blocked"),
		FDarkwellContinuousVisibilityBuilder::IsBlockedBySegments(
			FVector2D(-200.0, 0.0), FVector2D(200.0, 0.0), Segments));
	TestFalse(TEXT("Free space before the wall remains Live"),
		FDarkwellContinuousVisibilityBuilder::IsBlockedBySegments(
			FVector2D(-200.0, 0.0), FVector2D(-50.0, 0.0), Segments));
	TestFalse(TEXT("A ray past the wall end remains Live"),
		FDarkwellContinuousVisibilityBuilder::IsBlockedBySegments(
			FVector2D(-200.0, 0.0), FVector2D(200.0, 250.0), Segments));
	TestTrue(TEXT("The same wall blocks from the opposite legal side"),
		FDarkwellContinuousVisibilityBuilder::IsBlockedBySegments(
			FVector2D(200.0, 0.0), FVector2D(-200.0, 0.0), Segments));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDarkwellFogWallSurfaceCoverageTest,
	"Darkwell.FogVisual.P3.ObjectLocalSurfaceCoverage",
	Darkwell::FogVisualTests::Flags)

bool FDarkwellFogWallSurfaceCoverageTest::RunTest(const FString& Parameters)
{
	FVector2D NorthFaceA;
	FVector2D NorthFaceB;
	FVector2D SouthFaceA;
	FVector2D SouthFaceB;
	const FVector2D Origin(0.0, 100.0);
	const FVector2D Normal(1.0, 0.0);
	const FVector2D Tangent(0.0, 1.0);
	TestTrue(TEXT("North rendered face resolves stable wall samples"),
		FDarkwellFogSurfaceCoverageMath::ResolveWallSideSamples(
			FVector2D(20.0, 160.0), Origin, Normal, Tangent,
			20.0f, 7.5f, NorthFaceA, NorthFaceB));
	TestTrue(TEXT("South rendered face resolves stable wall samples"),
		FDarkwellFogSurfaceCoverageMath::ResolveWallSideSamples(
			FVector2D(-20.0, 160.0), Origin, Normal, Tangent,
			20.0f, 7.5f, SouthFaceA, SouthFaceB));
	TestTrue(TEXT("Opposite rendered faces share the same local cross-section samples"),
		NorthFaceA.Equals(SouthFaceA, 1.0e-4)
			&& NorthFaceB.Equals(SouthFaceB, 1.0e-4));
	TestTrue(TEXT("Wall state is symmetric when either legal side is Live"),
		FMath::IsNearlyEqual(
			FDarkwellFogSurfaceCoverageMath::CombineWallSides(1.0f, 0.0f),
			FDarkwellFogSurfaceCoverageMath::CombineWallSides(0.0f, 1.0f)));
	TestEqual(TEXT("Four-sided box uses the maximum exterior Live coverage"),
		FDarkwellFogSurfaceCoverageMath::CombineBoxSides(0.0f, 0.25f, 0.75f, 0.5f),
		0.75f);
	return true;
}

#endif
