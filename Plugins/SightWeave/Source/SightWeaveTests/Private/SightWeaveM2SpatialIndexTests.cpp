#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SightWeaveSpatialIndex.h"

namespace SightWeave::M2::SpatialTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FSightWeaveSegment2D Segment(
		const int64 Id,
		const FVector2D A,
		const FVector2D B,
		const TCHAR* Floor = TEXT("Ground"),
		float ZMin = 0.0f,
		float ZMax = 300.0f)
	{
		FSightWeaveSegment2D Result;
		Result.StableId = Id;
		Result.A = A;
		Result.B = B;
		Result.FloorId = FSightWeaveFloorId(FName(Floor));
		Result.HeightRange.ZMin = ZMin;
		Result.HeightRange.ZMax = ZMax;
		return Result;
	}

	FSightWeaveHeightRange QueryHeight()
	{
		FSightWeaveHeightRange Result;
		Result.ZMin = 50.0f;
		Result.ZMax = 150.0f;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2StaticBuildTest,
	"SightWeave.M2.SpatialIndex.StaticBuild",
	SightWeave::M2::SpatialTests::TestFlags)

bool FSightWeaveM2StaticBuildTest::RunTest(const FString& Parameters)
{
	FSightWeaveFloorSpatialIndex Index(100.0);
	const TArray<FSightWeaveSegment2D> Segments = {
		SightWeave::M2::SpatialTests::Segment(3, FVector2D(0.0, 0.0), FVector2D(80.0, 0.0)),
		SightWeave::M2::SpatialTests::Segment(1, FVector2D(500.0, 0.0), FVector2D(580.0, 0.0)) };
	TestTrue(TEXT("Static build succeeds"), Index.BuildStatic(Segments));
	const FSightWeaveSpatialIndexStats Stats = Index.GetStats();
	TestEqual(TEXT("All static segments are indexed"), Stats.StaticSegmentCount, 2);
	TestEqual(TEXT("Static build counter increments"), Stats.StaticBuildCount, int64(1));
	TestTrue(TEXT("Grid has occupied cells"), Stats.CellCount >= 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2LocalQueryTest,
	"SightWeave.M2.SpatialIndex.LocalQuery",
	SightWeave::M2::SpatialTests::TestFlags)

bool FSightWeaveM2LocalQueryTest::RunTest(const FString& Parameters)
{
	FSightWeaveFloorSpatialIndex Index(100.0);
	Index.BuildStatic({
		SightWeave::M2::SpatialTests::Segment(1, FVector2D(0.0, 0.0), FVector2D(80.0, 0.0)),
		SightWeave::M2::SpatialTests::Segment(2, FVector2D(1000.0, 0.0), FVector2D(1080.0, 0.0)) });
	TArray<FSightWeaveSegment2D> Results;
	Index.Query(
		FSightWeaveFloorId(FName(TEXT("Ground"))),
		FBox2D(FVector2D(-10.0, -10.0), FVector2D(90.0, 10.0)),
		SightWeave::M2::SpatialTests::QueryHeight(),
		0.01,
		Results);
	TestEqual(TEXT("Local query does not scan a remote cell"), Results.Num(), 1);
	TestEqual(TEXT("Local segment is returned"), Results[0].StableId, int64(1));
	TestEqual(TEXT("Candidate stat reports the filtered result"), Index.GetStats().LastCandidateCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2FloorIsolationIndexTest,
	"SightWeave.M2.SpatialIndex.FloorIsolation",
	SightWeave::M2::SpatialTests::TestFlags)

bool FSightWeaveM2FloorIsolationIndexTest::RunTest(const FString& Parameters)
{
	FSightWeaveFloorSpatialIndex Index(100.0);
	Index.BuildStatic({
		SightWeave::M2::SpatialTests::Segment(1, FVector2D(0.0, 0.0), FVector2D(80.0, 0.0), TEXT("Ground")),
		SightWeave::M2::SpatialTests::Segment(2, FVector2D(0.0, 0.0), FVector2D(80.0, 0.0), TEXT("Upper")) });
	TArray<FSightWeaveSegment2D> Results;
	Index.Query(
		FSightWeaveFloorId(FName(TEXT("Upper"))),
		FBox2D(FVector2D(-10.0, -10.0), FVector2D(90.0, 10.0)),
		SightWeave::M2::SpatialTests::QueryHeight(),
		0.01,
		Results);
	TestEqual(TEXT("Only the requested floor is searched"), Results.Num(), 1);
	TestEqual(TEXT("Upper-floor segment is returned"), Results[0].StableId, int64(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2HeightIsolationIndexTest,
	"SightWeave.M2.SpatialIndex.HeightIsolation",
	SightWeave::M2::SpatialTests::TestFlags)

bool FSightWeaveM2HeightIsolationIndexTest::RunTest(const FString& Parameters)
{
	FSightWeaveFloorSpatialIndex Index(100.0);
	Index.BuildStatic({
		SightWeave::M2::SpatialTests::Segment(1, FVector2D(0.0, 0.0), FVector2D(80.0, 0.0), TEXT("Ground"), -100.0f, 0.0f),
		SightWeave::M2::SpatialTests::Segment(2, FVector2D(0.0, 20.0), FVector2D(80.0, 20.0), TEXT("Ground"), 100.0f, 200.0f) });
	TArray<FSightWeaveSegment2D> Results;
	Index.Query(
		FSightWeaveFloorId(FName(TEXT("Ground"))),
		FBox2D(FVector2D(-10.0, -10.0), FVector2D(90.0, 30.0)),
		SightWeave::M2::SpatialTests::QueryHeight(),
		0.01,
		Results);
	TestEqual(TEXT("Only height-overlapping segment is returned"), Results.Num(), 1);
	TestEqual(TEXT("Height-overlapping stable ID is retained"), Results[0].StableId, int64(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2DynamicInsertRemoveTest,
	"SightWeave.M2.SpatialIndex.DynamicInsertRemove",
	SightWeave::M2::SpatialTests::TestFlags)

bool FSightWeaveM2DynamicInsertRemoveTest::RunTest(const FString& Parameters)
{
	FSightWeaveFloorSpatialIndex Index(100.0);
	const FSightWeaveOccluderHandle Handle(10);
	const TArray<FSightWeaveSegment2D> Segments = {
		SightWeave::M2::SpatialTests::Segment(100, FVector2D(0.0, 0.0), FVector2D(80.0, 0.0)) };
	TestTrue(TEXT("Dynamic occluder inserts"), Index.InsertOccluder(Handle, Segments, true));
	TestTrue(TEXT("Dynamic stable ID is indexed"), Index.ContainsSegment(100));
	TestTrue(TEXT("Dynamic handle is indexed"), Index.ContainsOccluder(Handle));
	FBox2D OldBounds(ForceInit);
	TestTrue(TEXT("Dynamic occluder removes"), Index.RemoveOccluder(Handle, &OldBounds));
	TestTrue(TEXT("Old bounds were preserved"),
		OldBounds.bIsValid
		&& OldBounds.Min.Equals(FVector2D(0.0, 0.0), 1.0e-9)
		&& OldBounds.Max.Equals(FVector2D(80.0, 0.0), 1.0e-9));
	TestFalse(TEXT("Removed stable ID is gone"), Index.ContainsSegment(100));
	TestFalse(TEXT("Removed handle is gone"), Index.ContainsOccluder(Handle));
	TestEqual(TEXT("No occupied cells remain"), Index.GetStats().CellCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2DynamicUpdateTest,
	"SightWeave.M2.SpatialIndex.DynamicTransformUpdate",
	SightWeave::M2::SpatialTests::TestFlags)

bool FSightWeaveM2DynamicUpdateTest::RunTest(const FString& Parameters)
{
	FSightWeaveFloorSpatialIndex Index(100.0);
	const FSightWeaveOccluderHandle Handle(10);
	Index.InsertOccluder(Handle, {
		SightWeave::M2::SpatialTests::Segment(100, FVector2D(0.0, 0.0), FVector2D(80.0, 0.0)) }, true);
	FBox2D OldBounds(ForceInit);
	FBox2D NewBounds(ForceInit);
	TestTrue(TEXT("Dynamic update succeeds"), Index.UpdateOccluder(Handle, {
		SightWeave::M2::SpatialTests::Segment(101, FVector2D(1000.0, 0.0), FVector2D(1080.0, 0.0)) }, true, OldBounds, NewBounds));
	TArray<FSightWeaveSegment2D> Results;
	Index.Query(FSightWeaveFloorId(FName(TEXT("Ground"))),
		FBox2D(FVector2D(-10.0, -10.0), FVector2D(90.0, 10.0)),
		SightWeave::M2::SpatialTests::QueryHeight(), 0.01, Results);
	TestEqual(TEXT("Old cells have no stale segment"), Results.Num(), 0);
	Index.Query(FSightWeaveFloorId(FName(TEXT("Ground"))),
		FBox2D(FVector2D(990.0, -10.0), FVector2D(1090.0, 10.0)),
		SightWeave::M2::SpatialTests::QueryHeight(), 0.01, Results);
	TestEqual(TEXT("New cells contain exactly the moved segment"), Results.Num(), 1);
	TestEqual(TEXT("New stable identity is returned"), Results[0].StableId, int64(101));
	TestFalse(TEXT("Old stable identity is gone"), Index.ContainsSegment(100));
	TestEqual(TEXT("Dynamic update is counted"), Index.GetStats().DynamicUpdateCount, int64(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2StableCandidateOrderTest,
	"SightWeave.M2.SpatialIndex.StableCandidateOrder",
	SightWeave::M2::SpatialTests::TestFlags)

bool FSightWeaveM2StableCandidateOrderTest::RunTest(const FString& Parameters)
{
	FSightWeaveFloorSpatialIndex Index(100.0);
	Index.BuildStatic({
		SightWeave::M2::SpatialTests::Segment(50, FVector2D(0.0, 0.0), FVector2D(80.0, 0.0)),
		SightWeave::M2::SpatialTests::Segment(10, FVector2D(0.0, 20.0), FVector2D(80.0, 20.0)),
		SightWeave::M2::SpatialTests::Segment(30, FVector2D(0.0, 40.0), FVector2D(80.0, 40.0)) });
	TArray<FSightWeaveSegment2D> Results;
	Index.Query(FSightWeaveFloorId(FName(TEXT("Ground"))),
		FBox2D(FVector2D(-10.0, -10.0), FVector2D(90.0, 50.0)),
		SightWeave::M2::SpatialTests::QueryHeight(), 0.01, Results);
	TestEqual(TEXT("All candidates are found"), Results.Num(), 3);
	TestTrue(TEXT("Candidates are StableId sorted"),
		Results[0].StableId == 10 && Results[1].StableId == 30 && Results[2].StableId == 50);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2DuplicateStableIdTest,
	"SightWeave.M2.SpatialIndex.DuplicateStableIdRejected",
	SightWeave::M2::SpatialTests::TestFlags)

bool FSightWeaveM2DuplicateStableIdTest::RunTest(const FString& Parameters)
{
	FSightWeaveFloorSpatialIndex Index(100.0);
	TestFalse(TEXT("Static build reports duplicate stable identity"), Index.BuildStatic({
		SightWeave::M2::SpatialTests::Segment(1, FVector2D(0.0, 0.0), FVector2D(80.0, 0.0)),
		SightWeave::M2::SpatialTests::Segment(1, FVector2D(200.0, 0.0), FVector2D(280.0, 0.0)) }));
	TestEqual(TEXT("Only the first stable identity remains"), Index.GetStats().SegmentCount, 1);
	return true;
}

#endif
