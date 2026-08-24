#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "Misc/AutomationTest.h"
#include "SightWeaveGeometry.h"

#include <limits>

namespace SightWeave::M2::Tests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FSightWeaveSegment2D MakeSegment(
		const FVector2D A,
		const FVector2D B,
		const int64 StableId = 1,
		const TCHAR* Floor = TEXT("Ground"),
		const float ZMin = 0.0f,
		const float ZMax = 300.0f)
	{
		FSightWeaveSegment2D Segment;
		Segment.A = A;
		Segment.B = B;
		Segment.FloorId = FSightWeaveFloorId(FName(Floor));
		Segment.HeightRange.ZMin = ZMin;
		Segment.HeightRange.ZMax = ZMax;
		Segment.StableId = StableId;
		Segment.SourceEdgeIndices.Add(static_cast<int32>(StableId));
		return Segment;
	}

	FSightWeaveReferenceSolveInput MakeRadialInput()
	{
		FSightWeaveReferenceSolveInput Input;
		Input.FloorId = FSightWeaveFloorId(FName(TEXT("Ground")));
		Input.HeightRange.ZMin = 50.0f;
		Input.HeightRange.ZMax = 150.0f;
		Input.Range = 1000.0;
		Input.Shape = ESightWeaveSourceShape::Radial;
		Input.Tolerances.RadialBoundarySteps = 64;
		return Input;
	}

	bool VerticesEqual(TConstArrayView<FVector> A, TConstArrayView<FVector> B, double Epsilon = 1.0e-9)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (!A[Index].Equals(B[Index], Epsilon))
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2RaySegmentIntersectionTest,
	"SightWeave.M2.Geometry.RaySegmentIntersection",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2RaySegmentIntersectionTest::RunTest(const FString& Parameters)
{
	const FSightWeaveSegment2D Segment = SightWeave::M2::Tests::MakeSegment(FVector2D(5.0, -2.0), FVector2D(5.0, 2.0));
	const FSightWeaveRaySegmentHit Hit = SightWeave::Geometry::IntersectRaySegment(
		FVector2D::ZeroVector, FVector2D(1.0, 0.0), Segment, FSightWeaveGeometryTolerances());
	TestTrue(TEXT("Analytic ray intersects the segment"), Hit.bHit);
	TestEqual(TEXT("Ray distance is exact"), Hit.RayDistance, 5.0);
	TestTrue(TEXT("Intersection point is exact"), Hit.Point.Equals(FVector2D(5.0, 0.0), 1.0e-9));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2ParallelRayTest,
	"SightWeave.M2.Geometry.ParallelAndNearParallel",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2ParallelRayTest::RunTest(const FString& Parameters)
{
	const FSightWeaveSegment2D Segment = SightWeave::M2::Tests::MakeSegment(FVector2D(1.0, 1.0), FVector2D(5.0, 1.0));
	const FSightWeaveGeometryTolerances Tolerances;
	TestFalse(TEXT("Parallel ray has no hit"), SightWeave::Geometry::IntersectRaySegment(
		FVector2D::ZeroVector, FVector2D(1.0, 0.0), Segment, Tolerances).bHit);
	TestFalse(TEXT("Near-parallel ray inside epsilon has no hit"), SightWeave::Geometry::IntersectRaySegment(
		FVector2D::ZeroVector, FVector2D(1.0, 1.0e-12), Segment, Tolerances).bHit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2EndpointRayTest,
	"SightWeave.M2.Geometry.EndpointHit",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2EndpointRayTest::RunTest(const FString& Parameters)
{
	const FSightWeaveSegment2D Segment = SightWeave::M2::Tests::MakeSegment(FVector2D(5.0, 0.0), FVector2D(5.0, 5.0));
	const FSightWeaveRaySegmentHit Hit = SightWeave::Geometry::IntersectRaySegment(
		FVector2D::ZeroVector, FVector2D(1.0, 0.0), Segment, FSightWeaveGeometryTolerances());
	TestTrue(TEXT("Endpoint contact is a deterministic hit"), Hit.bHit);
	TestEqual(TEXT("Endpoint fraction is zero"), Hit.SegmentFraction, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2NormalizationTest,
	"SightWeave.M2.Geometry.NormalizeZeroDuplicateReverse",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2NormalizationTest::RunTest(const FString& Parameters)
{
	TArray<FSightWeaveSegment2D> Input;
	Input.Add(SightWeave::M2::Tests::MakeSegment(FVector2D(0.0, 0.0), FVector2D(100.0, 0.0), 1));
	Input.Add(SightWeave::M2::Tests::MakeSegment(FVector2D(100.0, 0.0), FVector2D(0.0, 0.0), 2));
	Input.Add(SightWeave::M2::Tests::MakeSegment(FVector2D(20.0, 20.0), FVector2D(20.00001, 20.0), 3));
	const FSightWeaveNormalizationResult Result = SightWeave::Geometry::NormalizeSegments(
		Input, FSightWeaveGeometryTolerances(), false);
	TestEqual(TEXT("Only one unique nonzero segment remains"), Result.Segments.Num(), 1);
	TestEqual(TEXT("Reverse duplicate is removed"), Result.RemovedDuplicates, 1);
	TestEqual(TEXT("Zero-length segment is removed"), Result.RemovedZeroLength, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2WeldAndMergeTest,
	"SightWeave.M2.Geometry.WeldAndCollinearMerge",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2WeldAndMergeTest::RunTest(const FString& Parameters)
{
	TArray<FSightWeaveSegment2D> Input;
	Input.Add(SightWeave::M2::Tests::MakeSegment(FVector2D(0.0, 0.0), FVector2D(100.0, 0.0), 1));
	Input.Add(SightWeave::M2::Tests::MakeSegment(FVector2D(100.05, 0.0), FVector2D(200.0, 0.0), 2));
	const FSightWeaveNormalizationResult Result = SightWeave::Geometry::NormalizeSegments(
		Input, FSightWeaveGeometryTolerances(), true);
	TestEqual(TEXT("Safe contiguous collinear edges merge"), Result.Segments.Num(), 1);
	TestEqual(TEXT("One collinear merge is recorded"), Result.CollinearMerges, 1);
	TestTrue(TEXT("Merged source mapping is preserved"), Result.Segments[0].SourceEdgeIndices.Num() == 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2NarrowOpeningTest,
	"SightWeave.M2.Geometry.NarrowOpeningPreserved",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2NarrowOpeningTest::RunTest(const FString& Parameters)
{
	TArray<FSightWeaveSegment2D> Input;
	Input.Add(SightWeave::M2::Tests::MakeSegment(FVector2D(0.0, 0.0), FVector2D(100.0, 0.0), 1));
	Input.Add(SightWeave::M2::Tests::MakeSegment(FVector2D(100.5, 0.0), FVector2D(200.0, 0.0), 2));
	const FSightWeaveNormalizationResult Result = SightWeave::Geometry::NormalizeSegments(
		Input, FSightWeaveGeometryTolerances(), true);
	TestEqual(TEXT("A gap larger than weld epsilon remains open"), Result.Segments.Num(), 2);
	TestEqual(TEXT("No merge bridges an authored opening"), Result.CollinearMerges, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2DeterministicOrderingTest,
	"SightWeave.M2.Geometry.DeterministicOrdering",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2DeterministicOrderingTest::RunTest(const FString& Parameters)
{
	TArray<FSightWeaveSegment2D> Forward;
	Forward.Add(SightWeave::M2::Tests::MakeSegment(FVector2D(50.0, 10.0), FVector2D(60.0, 10.0), 20));
	Forward.Add(SightWeave::M2::Tests::MakeSegment(FVector2D(-50.0, 5.0), FVector2D(-40.0, 5.0), 10));
	TArray<FSightWeaveSegment2D> Reverse = Forward;
	Algo::Reverse(Reverse);
	const FSightWeaveNormalizationResult A = SightWeave::Geometry::NormalizeSegments(Forward, FSightWeaveGeometryTolerances(), false);
	const FSightWeaveNormalizationResult B = SightWeave::Geometry::NormalizeSegments(Reverse, FSightWeaveGeometryTolerances(), false);
	TestEqual(TEXT("Both inputs normalize to the same count"), A.Segments.Num(), B.Segments.Num());
	for (int32 Index = 0; Index < A.Segments.Num(); ++Index)
	{
		TestTrue(TEXT("Normalized endpoint order is input-order independent"),
			A.Segments[Index].A == B.Segments[Index].A && A.Segments[Index].B == B.Segments[Index].B);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2NonFiniteTest,
	"SightWeave.M2.Geometry.NonFiniteRejected",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2NonFiniteTest::RunTest(const FString& Parameters)
{
	FSightWeaveSegment2D Segment = SightWeave::M2::Tests::MakeSegment(FVector2D::ZeroVector, FVector2D(100.0, 0.0));
	Segment.A.X = std::numeric_limits<double>::quiet_NaN();
	const TArray<FSightWeaveSegment2D> Input = { Segment };
	const FSightWeaveNormalizationResult Result = SightWeave::Geometry::NormalizeSegments(
		Input, FSightWeaveGeometryTolerances(), true);
	TestEqual(TEXT("Non-finite geometry is not retained"), Result.Segments.Num(), 0);
	TestEqual(TEXT("Non-finite rejection is diagnosed"), Result.RemovedInvalid, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2PointBoundaryTest,
	"SightWeave.M2.Geometry.PointOnPolygonBoundary",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2PointBoundaryTest::RunTest(const FString& Parameters)
{
	const TArray<FVector> Polygon = {
		FVector(0.0, 0.0, 0.0), FVector(100.0, 0.0, 0.0),
		FVector(100.0, 100.0, 0.0), FVector(0.0, 100.0, 0.0) };
	const FSightWeaveGeometryTolerances Tolerances;
	TestTrue(TEXT("Boundary is inclusively inside"), SightWeave::Geometry::IsPointInPolygon(FVector2D(50.0, 0.0), Polygon, Tolerances));
	TestTrue(TEXT("Interior is inside"), SightWeave::Geometry::IsPointInPolygon(FVector2D(50.0, 50.0), Polygon, Tolerances));
	TestFalse(TEXT("Exterior is outside"), SightWeave::Geometry::IsPointInPolygon(FVector2D(150.0, 50.0), Polygon, Tolerances));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2RadialPolygonTest,
	"SightWeave.M2.Geometry.Polygon.RadialUnoccluded",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2RadialPolygonTest::RunTest(const FString& Parameters)
{
	const FSightWeaveReferenceSolveResult Result = SightWeave::Geometry::SolveReferencePolygon(
		SightWeave::M2::Tests::MakeRadialInput());
	TestTrue(TEXT("Unoccluded radial solve succeeds"), Result.bSucceeded);
	TestEqual(TEXT("Radial tessellation has a stable count"), Result.Vertices.Num(), 64);
	TestTrue(TEXT("Radial polygon is simple"), SightWeave::Geometry::IsSimplePolygon(
		Result.Vertices, FSightWeaveGeometryTolerances()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2ConePolygonTest,
	"SightWeave.M2.Geometry.Polygon.DirectionalAndCameraCone",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2ConePolygonTest::RunTest(const FString& Parameters)
{
	FSightWeaveReferenceSolveInput Input = SightWeave::M2::Tests::MakeRadialInput();
	Input.Shape = ESightWeaveSourceShape::DirectionalCone;
	Input.HalfAngleDegrees = 45.0;
	const FSightWeaveReferenceSolveResult Directional = SightWeave::Geometry::SolveReferencePolygon(Input);
	Input.Shape = ESightWeaveSourceShape::CameraCone;
	const FSightWeaveReferenceSolveResult Camera = SightWeave::Geometry::SolveReferencePolygon(Input);
	TestTrue(TEXT("Directional cone solve succeeds"), Directional.bSucceeded);
	TestTrue(TEXT("Camera cone solve succeeds"), Camera.bSucceeded);
	TestTrue(TEXT("Cone begins at the source origin"), Directional.Vertices[0].Equals(Input.Origin, 1.0e-9));
	TestTrue(TEXT("Equivalent cone shapes share deterministic geometry"),
		SightWeave::M2::Tests::VerticesEqual(Directional.Vertices, Camera.Vertices));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2StraightWallTest,
	"SightWeave.M2.Geometry.Polygon.StraightWall",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2StraightWallTest::RunTest(const FString& Parameters)
{
	FSightWeaveReferenceSolveInput Input = SightWeave::M2::Tests::MakeRadialInput();
	Input.Segments.Add(SightWeave::M2::Tests::MakeSegment(FVector2D(500.0, -1000.0), FVector2D(500.0, 1000.0), 7));
	const FSightWeaveReferenceSolveResult Result = SightWeave::Geometry::SolveReferencePolygon(Input);
	TestTrue(TEXT("Straight-wall solve succeeds"), Result.bSucceeded);
	int32 WallVertexCount = 0;
	for (const FVector& Vertex : Result.Vertices)
	{
		if (FVector::Dist2D(Vertex, Input.Origin) < Input.Range - 0.1)
		{
			TestTrue(TEXT("Occluded vertex remains on authored wall line"), FMath::Abs(Vertex.X - 500.0) <= 0.001);
			++WallVertexCount;
		}
	}
	TestTrue(TEXT("Reference solver emitted wall-bound vertices"), WallVertexCount >= 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2DiagonalWallTest,
	"SightWeave.M2.Geometry.Polygon.DiagonalWall",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2DiagonalWallTest::RunTest(const FString& Parameters)
{
	FSightWeaveReferenceSolveInput Input = SightWeave::M2::Tests::MakeRadialInput();
	Input.Segments.Add(SightWeave::M2::Tests::MakeSegment(FVector2D(250.0, -250.0), FVector2D(750.0, 250.0), 8));
	const FSightWeaveReferenceSolveResult Result = SightWeave::Geometry::SolveReferencePolygon(Input);
	TestTrue(TEXT("Diagonal-wall solve succeeds"), Result.bSucceeded);
	int32 DiagonalHits = 0;
	for (const FVector& Vertex : Result.Vertices)
	{
		if (Vertex.X >= 250.0 && Vertex.X <= 750.0 && FMath::Abs(Vertex.Y - (Vertex.X - 500.0)) <= 0.01)
		{
			++DiagonalHits;
		}
	}
	TestTrue(TEXT("Analytic vertices remain on the diagonal segment"), DiagonalHits >= 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2CornersTest,
	"SightWeave.M2.Geometry.Polygon.LAndTCorners",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2CornersTest::RunTest(const FString& Parameters)
{
	FSightWeaveReferenceSolveInput LInput = SightWeave::M2::Tests::MakeRadialInput();
	LInput.Segments = {
		SightWeave::M2::Tests::MakeSegment(FVector2D(400.0, 0.0), FVector2D(700.0, 0.0), 1),
		SightWeave::M2::Tests::MakeSegment(FVector2D(700.0, 0.0), FVector2D(700.0, 400.0), 2) };
	FSightWeaveReferenceSolveInput TInput = LInput;
	TInput.Segments.Add(SightWeave::M2::Tests::MakeSegment(FVector2D(550.0, -300.0), FVector2D(550.0, 300.0), 3));
	const FSightWeaveReferenceSolveResult LResult = SightWeave::Geometry::SolveReferencePolygon(LInput);
	const FSightWeaveReferenceSolveResult TResult = SightWeave::Geometry::SolveReferencePolygon(TInput);
	TestTrue(TEXT("L-corner solve succeeds"), LResult.bSucceeded);
	TestTrue(TEXT("T-corner solve succeeds"), TResult.bSucceeded);
	TestTrue(TEXT("L-corner polygon remains simple"), SightWeave::Geometry::IsSimplePolygon(LResult.Vertices, LInput.Tolerances));
	TestTrue(TEXT("T-corner polygon remains simple"), SightWeave::Geometry::IsSimplePolygon(TResult.Vertices, TInput.Tolerances));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2RoomTest,
	"SightWeave.M2.Geometry.Polygon.Room",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2RoomTest::RunTest(const FString& Parameters)
{
	FSightWeaveReferenceSolveInput Input = SightWeave::M2::Tests::MakeRadialInput();
	Input.Segments = {
		SightWeave::M2::Tests::MakeSegment(FVector2D(-300.0, -300.0), FVector2D(300.0, -300.0), 1),
		SightWeave::M2::Tests::MakeSegment(FVector2D(300.0, -300.0), FVector2D(300.0, 300.0), 2),
		SightWeave::M2::Tests::MakeSegment(FVector2D(300.0, 300.0), FVector2D(-300.0, 300.0), 3),
		SightWeave::M2::Tests::MakeSegment(FVector2D(-300.0, 300.0), FVector2D(-300.0, -300.0), 4) };
	const FSightWeaveReferenceSolveResult Result = SightWeave::Geometry::SolveReferencePolygon(Input);
	TestTrue(TEXT("Closed room solve succeeds"), Result.bSucceeded);
	for (const FVector& Vertex : Result.Vertices)
	{
		TestTrue(TEXT("Room clips every vertex to its walls"), FMath::Abs(Vertex.X) <= 300.01 && FMath::Abs(Vertex.Y) <= 300.01);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2DoorwayTest,
	"SightWeave.M2.Geometry.Polygon.Doorway",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2DoorwayTest::RunTest(const FString& Parameters)
{
	FSightWeaveReferenceSolveInput Input = SightWeave::M2::Tests::MakeRadialInput();
	Input.Segments = {
		SightWeave::M2::Tests::MakeSegment(FVector2D(500.0, -1000.0), FVector2D(500.0, -100.0), 1),
		SightWeave::M2::Tests::MakeSegment(FVector2D(500.0, 100.0), FVector2D(500.0, 1000.0), 2) };
	const FSightWeaveReferenceSolveResult Result = SightWeave::Geometry::SolveReferencePolygon(Input);
	TestTrue(TEXT("Doorway solve succeeds"), Result.bSucceeded);
	TestTrue(TEXT("A point beyond the deliberate opening is visible"), SightWeave::Geometry::IsPointInPolygon(
		FVector2D(800.0, 0.0), Result.Vertices, Input.Tolerances));
	TestFalse(TEXT("A point beyond the solid wall remains blocked"), SightWeave::Geometry::IsPointInPolygon(
		FVector2D(800.0, 500.0), Result.Vertices, Input.Tolerances));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2SourceOnNearEdgeTest,
	"SightWeave.M2.Geometry.Polygon.SourceOnAndNearEdge",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2SourceOnNearEdgeTest::RunTest(const FString& Parameters)
{
	FSightWeaveReferenceSolveInput Input = SightWeave::M2::Tests::MakeRadialInput();
	Input.Segments.Add(SightWeave::M2::Tests::MakeSegment(FVector2D(0.0, -1000.0), FVector2D(0.0, 1000.0), 1));
	const FSightWeaveReferenceSolveResult OnEdge = SightWeave::Geometry::SolveReferencePolygon(Input);
	Input.Origin.X = -0.1;
	const FSightWeaveReferenceSolveResult NearEdge = SightWeave::Geometry::SolveReferencePolygon(Input);
	TestTrue(TEXT("Source exactly on a segment remains deterministic and valid"), OnEdge.bSucceeded);
	TestTrue(TEXT("Source near a segment remains deterministic and valid"), NearEdge.bSucceeded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2HeightFilterTest,
	"SightWeave.M2.Geometry.Polygon.HeightBandFiltering",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2HeightFilterTest::RunTest(const FString& Parameters)
{
	FSightWeaveReferenceSolveInput Below = SightWeave::M2::Tests::MakeRadialInput();
	Below.Segments.Add(SightWeave::M2::Tests::MakeSegment(FVector2D(500.0, -1000.0), FVector2D(500.0, 1000.0), 1, TEXT("Ground"), -100.0f, 0.0f));
	FSightWeaveReferenceSolveInput Through = SightWeave::M2::Tests::MakeRadialInput();
	Through.Segments.Add(SightWeave::M2::Tests::MakeSegment(FVector2D(500.0, -1000.0), FVector2D(500.0, 1000.0), 1, TEXT("Ground"), 100.0f, 200.0f));
	const FSightWeaveReferenceSolveResult BelowResult = SightWeave::Geometry::SolveReferencePolygon(Below);
	const FSightWeaveReferenceSolveResult ThroughResult = SightWeave::Geometry::SolveReferencePolygon(Through);
	TestTrue(TEXT("Both height cases solve"), BelowResult.bSucceeded && ThroughResult.bSucceeded);
	TestTrue(TEXT("Nonoverlapping blocker is excluded"), SightWeave::Geometry::IsPointInPolygon(FVector2D(800.0, 0.0), BelowResult.Vertices, Below.Tolerances));
	TestFalse(TEXT("Overlapping blocker occludes"), SightWeave::Geometry::IsPointInPolygon(FVector2D(800.0, 0.0), ThroughResult.Vertices, Through.Tolerances));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2FloorFilterTest,
	"SightWeave.M2.Geometry.Polygon.FloorIsolation",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2FloorFilterTest::RunTest(const FString& Parameters)
{
	FSightWeaveReferenceSolveInput Input = SightWeave::M2::Tests::MakeRadialInput();
	Input.Segments.Add(SightWeave::M2::Tests::MakeSegment(
		FVector2D(500.0, -1000.0), FVector2D(500.0, 1000.0), 1, TEXT("Upper")));
	const FSightWeaveReferenceSolveResult Result = SightWeave::Geometry::SolveReferencePolygon(Input);
	TestTrue(TEXT("Cross-floor geometry is ignored"), Result.bSucceeded);
	TestEqual(TEXT("No cross-floor candidate is retained"), Result.CandidateSegmentCount, 0);
	TestTrue(TEXT("Cross-floor wall cannot block the query floor"), SightWeave::Geometry::IsPointInPolygon(
		FVector2D(800.0, 0.0), Result.Vertices, Input.Tolerances));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2RepeatDeterminismTest,
	"SightWeave.M2.Geometry.Polygon.RepeatDeterminism",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2RepeatDeterminismTest::RunTest(const FString& Parameters)
{
	FSightWeaveReferenceSolveInput Input = SightWeave::M2::Tests::MakeRadialInput();
	Input.Segments = {
		SightWeave::M2::Tests::MakeSegment(FVector2D(300.0, -600.0), FVector2D(300.0, 600.0), 4),
		SightWeave::M2::Tests::MakeSegment(FVector2D(-500.0, 200.0), FVector2D(700.0, 200.0), 2) };
	const FSightWeaveReferenceSolveResult First = SightWeave::Geometry::SolveReferencePolygon(Input);
	const FSightWeaveReferenceSolveResult Second = SightWeave::Geometry::SolveReferencePolygon(Input);
	TestTrue(TEXT("Repeated solves succeed"), First.bSucceeded && Second.bSucceeded);
	TestTrue(TEXT("Repeated solves emit exactly identical ordered vertices"),
		SightWeave::M2::Tests::VerticesEqual(First.Vertices, Second.Vertices));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2LateralAndRotationStabilityTest,
	"SightWeave.M2.Geometry.Polygon.LateralAndRotationStability",
	SightWeave::M2::Tests::TestFlags)

bool FSightWeaveM2LateralAndRotationStabilityTest::RunTest(const FString& Parameters)
{
	FSightWeaveReferenceSolveInput Input = SightWeave::M2::Tests::MakeRadialInput();
	Input.Shape = ESightWeaveSourceShape::DirectionalCone;
	Input.HalfAngleDegrees = 65.0;
	Input.Segments.Add(SightWeave::M2::Tests::MakeSegment(FVector2D(500.0, -1000.0), FVector2D(500.0, 1000.0), 1));
	for (int32 Step = 0; Step < 12; ++Step)
	{
		Input.Origin.Y = -110.0 + Step * 20.0;
		const double Rotation = FMath::DegreesToRadians(-10.0 + Step * 2.0);
		Input.Forward = FVector2D(FMath::Cos(Rotation), FMath::Sin(Rotation));
		const FSightWeaveReferenceSolveResult Result = SightWeave::Geometry::SolveReferencePolygon(Input);
		TestTrue(TEXT("Every lateral/rotation sample solves"), Result.bSucceeded);
		for (const FVector& Vertex : Result.Vertices)
		{
			const double DistanceFromSource = FVector::Dist2D(Vertex, Input.Origin);
			if (DistanceFromSource > Input.Tolerances.PointOnEdgeEpsilon
				&& DistanceFromSource < Input.Range - 0.1)
			{
				TestTrue(TEXT("Motion never waves vertices off the stable wall"), FMath::Abs(Vertex.X - 500.0) <= 0.001);
			}
		}
	}
	return true;
}

#endif
