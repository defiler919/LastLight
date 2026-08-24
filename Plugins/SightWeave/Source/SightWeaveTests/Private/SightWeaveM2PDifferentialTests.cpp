#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SightWeaveGeometry.h"
#include "SightWeaveSettings.h"
#include "SightWeaveWorldSubsystem.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace SightWeave::M2P::DifferentialTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	const FSightWeaveFloorId Ground(FName(TEXT("Ground")));
	const FSightWeaveFloorId Upper(FName(TEXT("Upper")));
	const FSightWeaveKnowledgeOwnerId Local(FName(TEXT("Local")));
	const FSightWeaveKnowledgeOwnerId Remote(FName(TEXT("Remote")));

	FSightWeaveSegment2D Segment(
		const FVector2D A,
		const FVector2D B,
		const int64 StableId,
		const FSightWeaveFloorId FloorId = Ground,
		const float ZMin = 0.0f,
		const float ZMax = 300.0f)
	{
		FSightWeaveSegment2D Result;
		Result.A = A;
		Result.B = B;
		Result.FloorId = FloorId;
		Result.HeightRange.ZMin = ZMin;
		Result.HeightRange.ZMax = ZMax;
		Result.StableId = StableId;
		Result.SourceEdgeIndices.Add(static_cast<int32>(StableId));
		return Result;
	}

	FSightWeaveReferenceSolveInput RadialInput()
	{
		FSightWeaveReferenceSolveInput Input;
		Input.Origin = FVector(0.0, 0.0, 100.0);
		Input.Forward = FVector2D(1.0, 0.0);
		Input.Shape = ESightWeaveSourceShape::Radial;
		Input.Range = 1000.0;
		Input.HalfAngleDegrees = 180.0;
		Input.FloorId = Ground;
		Input.HeightRange.ZMin = 0.0f;
		Input.HeightRange.ZMax = 300.0f;
		Input.Tolerances.RadialBoundarySteps = 96;
		return Input;
	}

	double AbsoluteArea(TConstArrayView<FVector> Vertices)
	{
		double TwiceArea = 0.0;
		for (int32 Index = 0; Index < Vertices.Num(); ++Index)
		{
			const FVector& A = Vertices[Index];
			const FVector& B = Vertices[(Index + 1) % Vertices.Num()];
			TwiceArea += A.X * B.Y - B.X * A.Y;
		}
		return FMath::Abs(TwiceArea) * 0.5;
	}

	FBox2D Bounds(TConstArrayView<FVector> Vertices)
	{
		FBox2D Result(ForceInit);
		for (const FVector& Vertex : Vertices)
		{
			Result += FVector2D(Vertex);
		}
		return Result;
	}

	double Cross2D(const FVector2D& A, const FVector2D& B)
	{
		return A.X * B.Y - A.Y * B.X;
	}

	double DistanceSquaredToSegment(const FVector2D& Point, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D Edge = B - A;
		const double LengthSquared = Edge.SizeSquared();
		if (LengthSquared <= UE_DOUBLE_SMALL_NUMBER)
		{
			return FVector2D::DistSquared(Point, A);
		}
		const double Fraction = FMath::Clamp(FVector2D::DotProduct(Point - A, Edge) / LengthSquared, 0.0, 1.0);
		return FVector2D::DistSquared(Point, A + Edge * Fraction);
	}

	int32 Orientation(const FVector2D& A, const FVector2D& B, const FVector2D& C, const double Epsilon)
	{
		const double Value = Cross2D(B - A, C - A);
		return FMath::Abs(Value) <= Epsilon ? 0 : (Value > 0.0 ? 1 : -1);
	}

	bool SegmentsIntersectInclusive(
		const FVector2D& A0,
		const FVector2D& A1,
		const FVector2D& B0,
		const FVector2D& B1,
		const double Epsilon)
	{
		const int32 O1 = Orientation(A0, A1, B0, Epsilon);
		const int32 O2 = Orientation(A0, A1, B1, Epsilon);
		const int32 O3 = Orientation(B0, B1, A0, Epsilon);
		const int32 O4 = Orientation(B0, B1, A1, Epsilon);
		if (O1 != O2 && O3 != O4)
		{
			return true;
		}
		const double Squared = Epsilon * Epsilon;
		return (O1 == 0 && DistanceSquaredToSegment(B0, A0, A1) <= Squared)
			|| (O2 == 0 && DistanceSquaredToSegment(B1, A0, A1) <= Squared)
			|| (O3 == 0 && DistanceSquaredToSegment(A0, B0, B1) <= Squared)
			|| (O4 == 0 && DistanceSquaredToSegment(A1, B0, B1) <= Squared);
	}

	FString FirstTopologyFailure(
		TConstArrayView<FVector> Vertices,
		const FSightWeaveGeometryTolerances& Tolerances)
	{
		const double Epsilon = FMath::Max(
			1.0e-9,
			FMath::Min(Tolerances.PointOnEdgeEpsilon, Tolerances.DuplicateVertexEpsilon) * 0.01);
		for (int32 AIndex = 0; AIndex < Vertices.Num(); ++AIndex)
		{
			const int32 ANext = (AIndex + 1) % Vertices.Num();
			const FVector2D A0(Vertices[AIndex]);
			const FVector2D A1(Vertices[ANext]);
			if (FVector2D::DistSquared(A0, A1) <= FMath::Square(Tolerances.DuplicateVertexEpsilon))
			{
				return FString::Printf(TEXT("short edge %d-%d (%s -> %s)"), AIndex, ANext, *A0.ToString(), *A1.ToString());
			}
			for (int32 BIndex = AIndex + 1; BIndex < Vertices.Num(); ++BIndex)
			{
				const int32 BNext = (BIndex + 1) % Vertices.Num();
				if (ANext == BIndex || BNext == AIndex)
				{
					continue;
				}
				const FVector2D B0(Vertices[BIndex]);
				const FVector2D B1(Vertices[BNext]);
				if (SegmentsIntersectInclusive(A0, A1, B0, B1, Epsilon))
				{
					return FString::Printf(
						TEXT("edges %d-%d and %d-%d (%s -> %s; %s -> %s)"),
						AIndex, ANext, BIndex, BNext, *A0.ToString(), *A1.ToString(), *B0.ToString(), *B1.ToString());
				}
			}
		}
		return TEXT("no reproduced reason");
	}

	bool CompareSolve(
		FAutomationTestBase& Test,
		const FString& Label,
		const FSightWeaveReferenceSolveInput& Input,
		const bool bDenseClassification)
	{
		const FSightWeaveReferenceSolveResult Reference = Geometry::SolveReferencePolygon(Input);
		const FSightWeaveReferenceSolveResult Optimized = Geometry::SolveOptimizedPolygon(Input);
		bool bMatched = true;
		auto Check = [&Test, &Label, &bMatched](const bool bCondition, const TCHAR* Detail)
		{
			if (!bCondition)
			{
				Test.AddError(FString::Printf(TEXT("%s: %s"), *Label, Detail));
				bMatched = false;
			}
		};

		if (Reference.bSucceeded != Optimized.bSucceeded)
		{
			Test.AddError(FString::Printf(
				TEXT("%s: success differs; Reference=%s error='%s', Optimized=%s error='%s', optimized-simple=%s"),
				*Label,
				Reference.bSucceeded ? TEXT("true") : TEXT("false"),
				*Reference.Error,
				Optimized.bSucceeded ? TEXT("true") : TEXT("false"),
				*Optimized.Error,
				Geometry::IsSimplePolygon(Optimized.Vertices, Input.Tolerances) ? TEXT("true") : TEXT("false")));
			Test.AddError(FString::Printf(TEXT("%s topology detail: %s"), *Label, *FirstTopologyFailure(Optimized.Vertices, Input.Tolerances)));
			bMatched = false;
		}
		Check(Reference.CandidateSegmentCount == Optimized.CandidateSegmentCount, TEXT("candidate segment count differs"));
		Check(Reference.CastRayCount == Optimized.CastRayCount, TEXT("cast ray count differs"));
		Check(Reference.CandidateAnglesRadians.Num() == Optimized.CandidateAnglesRadians.Num(), TEXT("candidate angle count differs"));
		Check(Reference.CandidateDistances.Num() == Optimized.CandidateDistances.Num(), TEXT("candidate distance count differs"));
		Check(Reference.CandidateBoundaryPoints.Num() == Optimized.CandidateBoundaryPoints.Num(), TEXT("candidate boundary count differs"));
		Check(Reference.Vertices.Num() == Optimized.Vertices.Num(), TEXT("vertex count differs"));
		if (!Reference.bSucceeded || !Optimized.bSucceeded)
		{
			return bMatched;
		}

		for (int32 Index = 0; Index < FMath::Min(Reference.CandidateAnglesRadians.Num(), Optimized.CandidateAnglesRadians.Num()); ++Index)
		{
			Check(FMath::IsNearlyEqual(Reference.CandidateAnglesRadians[Index], Optimized.CandidateAnglesRadians[Index], 1.0e-12),
				TEXT("candidate angle differs"));
		}
		for (int32 Index = 0; Index < FMath::Min(Reference.CandidateDistances.Num(), Optimized.CandidateDistances.Num()); ++Index)
		{
			Check(FMath::IsNearlyEqual(Reference.CandidateDistances[Index], Optimized.CandidateDistances[Index], 1.0e-6),
				TEXT("nearest-hit distance differs"));
		}
		for (int32 Index = 0; Index < FMath::Min(Reference.CandidateBoundaryPoints.Num(), Optimized.CandidateBoundaryPoints.Num()); ++Index)
		{
			Check(Reference.CandidateBoundaryPoints[Index].Equals(Optimized.CandidateBoundaryPoints[Index], 1.0e-5),
				TEXT("critical boundary point differs"));
		}
		for (int32 Index = 0; Index < FMath::Min(Reference.Vertices.Num(), Optimized.Vertices.Num()); ++Index)
		{
			Check(Reference.Vertices[Index].Equals(Optimized.Vertices[Index], 1.0e-5), TEXT("canonical vertex differs"));
		}

		const FBox2D ReferenceBounds = Bounds(Reference.Vertices);
		const FBox2D OptimizedBounds = Bounds(Optimized.Vertices);
		Check(ReferenceBounds.Min.Equals(OptimizedBounds.Min, 1.0e-5)
			&& ReferenceBounds.Max.Equals(OptimizedBounds.Max, 1.0e-5), TEXT("polygon bounds differ"));
		Check(FMath::IsNearlyEqual(AbsoluteArea(Reference.Vertices), AbsoluteArea(Optimized.Vertices), 0.01),
			TEXT("polygon area differs"));

		const int32 GridSteps = bDenseClassification ? 21 : 9;
		for (int32 Y = 0; Y < GridSteps; ++Y)
		{
			for (int32 X = 0; X < GridSteps; ++X)
			{
				const FVector2D Point(
					-1050.0 + 2100.0 * static_cast<double>(X) / (GridSteps - 1),
					-1050.0 + 2100.0 * static_cast<double>(Y) / (GridSteps - 1));
				Check(
					Geometry::IsPointInPolygon(Point, Reference.Vertices, Input.Tolerances)
						== Geometry::IsPointInPolygon(Point, Optimized.Vertices, Input.Tolerances),
					TEXT("dense point classification differs"));
			}
		}

		for (int32 Index = 0; Index < Reference.CandidateBoundaryPoints.Num(); Index += FMath::Max(1, Reference.CandidateBoundaryPoints.Num() / 24))
		{
			const FVector2D Boundary = Reference.CandidateBoundaryPoints[Index];
			const FVector2D Direction = (Boundary - FVector2D(Input.Origin)).GetSafeNormal();
			for (const double Offset : { -0.06, 0.0, 0.06 })
			{
				const FVector2D Point = Boundary + Direction * Offset;
				Check(
					Geometry::IsPointInPolygon(Point, Reference.Vertices, Input.Tolerances)
						== Geometry::IsPointInPolygon(Point, Optimized.Vertices, Input.Tolerances),
					TEXT("boundary-epsilon classification differs"));
			}
		}

		const FSightWeaveReferenceSolveResult Repeated = Geometry::SolveOptimizedPolygon(Input);
		Check(Repeated.Vertices.Num() == Optimized.Vertices.Num(), TEXT("repeat vertex count differs"));
		for (int32 Index = 0; Index < FMath::Min(Repeated.Vertices.Num(), Optimized.Vertices.Num()); ++Index)
		{
			Check(Repeated.Vertices[Index].Equals(Optimized.Vertices[Index], 1.0e-12), TEXT("optimized repeat is nondeterministic"));
		}
		return bMatched;
	}

	TArray<FSightWeaveSegment2D> RandomTangentialSegments(FRandomStream& Random, const int32 Count)
	{
		TArray<FSightWeaveSegment2D> Result;
		Result.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			// Every segment stays strictly inside its own disjoint polar sector. This
			// preserves randomized relevant endpoint events without creating crossing
			// authored geometry that the Reference oracle intentionally rejects.
			const double Angle = -PI + 2.0 * PI * (static_cast<double>(Index) + 0.5) / Count;
			const double Radius = Random.FRandRange(180.0f, 940.0f);
			const double SectorHalfAngle = PI / Count;
			const double HalfLength = Radius * FMath::Tan(SectorHalfAngle * 0.3) * Random.FRandRange(0.2f, 0.8f);
			const FVector2D Center(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius);
			const FVector2D Tangent(-FMath::Sin(Angle) * HalfLength, FMath::Cos(Angle) * HalfLength);
			Result.Add(Segment(Center - Tangent, Center + Tangent, Index + 1));
		}
		return Result;
	}

	class FTestWorld
	{
	public:
		explicit FTestWorld(const TCHAR* BaseName)
		{
			const FName WorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), FName(BaseName));
			World = NewObject<UWorld>(GetTransientPackage(), WorldName, RF_Transient);
			if (!World || !GEngine)
			{
				return;
			}
			World->WorldType = EWorldType::Game;
			FWorldContext& Context = GEngine->CreateNewWorldContext(World->WorldType);
			Context.SetCurrentWorld(World);
			World->InitializeNewWorld(UWorld::InitializationValues()
				.AllowAudioPlayback(false)
				.CreatePhysicsScene(false)
				.RequiresHitProxies(false)
				.CreateNavigation(false)
				.CreateAISystem(false)
				.ShouldSimulatePhysics(false)
				.SetTransactional(false));
		}

		~FTestWorld()
		{
			if (World && GEngine)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(true);
			}
		}

		USightWeaveWorldSubsystem* GetSubsystem() const
		{
			return World ? World->GetSubsystem<USightWeaveWorldSubsystem>() : nullptr;
		}

	private:
		UWorld* World = nullptr;
	};

	FSightWeaveFloorDefinition Floor(const FSightWeaveFloorId FloorId, const bool bActive, const float ZMin, const float ZMax)
	{
		FSightWeaveFloorDefinition Result;
		Result.FloorId = FloorId;
		Result.BoundsMin = FVector2D(-2000.0, -2000.0);
		Result.BoundsMax = FVector2D(2000.0, 2000.0);
		Result.HeightRange.ZMin = ZMin;
		Result.HeightRange.ZMax = ZMax;
		Result.bActiveForQueries = bActive;
		return Result;
	}

	FSightWeaveVisionSourceDescription Vision(
		const ESightWeaveIlluminationPolicy Policy,
		const FVector Location,
		const float Range,
		TArray<FName> Accepted = {})
	{
		FSightWeaveVisionSourceDescription Result;
		Result.KnowledgeOwnerId = Local;
		Result.FloorId = Ground;
		Result.Transform.SetLocation(Location);
		Result.Shape = ESightWeaveSourceShape::Radial;
		Result.Range = Range;
		Result.HalfAngleDegrees = 180.0f;
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		Result.IlluminationPolicy = Policy;
		Result.Compatibility.AcceptedCapabilities = MoveTemp(Accepted);
		return Result;
	}

	FSightWeaveIlluminationSourceDescription Light(
		const FVector Location,
		const float Range,
		TArray<FName> Capabilities)
	{
		FSightWeaveIlluminationSourceDescription Result;
		Result.KnowledgeOwnerId = Local;
		Result.FloorId = Ground;
		Result.Transform.SetLocation(Location);
		Result.Range = Range;
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		Result.EmittedCapabilities = MoveTemp(Capabilities);
		return Result;
	}

	struct FNamedQuery
	{
		FString Label;
		FSightWeaveVisibilityQueryResult Result;
	};

	struct FRuntimeTrace
	{
		TArray<FNamedQuery> Queries;
		TArray<int64> SnapshotRevisions;
		TArray<int64> DoorRevisions;
		bool bNoChangeVisionPreservedRevision = false;
		bool bNoChangeDoorPreservedRevision = false;
		bool bEveryQueryUsedCurrentSnapshot = true;
	};

	void AddQuery(
		FRuntimeTrace& Trace,
		USightWeaveWorldSubsystem* Subsystem,
		const TCHAR* Label,
		const FSightWeaveVisibilityQueryResult& Result)
	{
		Trace.Queries.Add({ Label, Result });
		Trace.bEveryQueryUsedCurrentSnapshot &=
			Result.SnapshotRevision == Subsystem->GetPublishedSnapshot().Revision;
	}

	FRuntimeTrace BuildRuntimeTrace(FAutomationTestBase& Test, const ESightWeaveSolverMode Mode)
	{
		USightWeaveSettings* Settings = GetMutableDefault<USightWeaveSettings>();
		TGuardValue<ESightWeaveSolverMode> ModeGuard(Settings->SolverMode, Mode);
		FTestWorld World(Mode == ESightWeaveSolverMode::Reference ? TEXT("SightWeaveM2PReference") : TEXT("SightWeaveM2POptimized"));
		USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
		FRuntimeTrace Trace;
		if (!Test.TestNotNull(TEXT("Differential subsystem exists"), Subsystem))
		{
			return Trace;
		}
		Test.TestTrue(TEXT("Ground floor registers"), Subsystem->RegisterFloor(Floor(Ground, true, 0.0f, 300.0f), nullptr));
		Test.TestTrue(TEXT("Upper floor registers"), Subsystem->RegisterFloor(Floor(Upper, false, 400.0f, 700.0f), nullptr));

		const FSightWeaveVisionSourceDescription BypassDescription = Vision(
			ESightWeaveIlluminationPolicy::BypassLegalIllumination, FVector::ZeroVector, 300.0f);
		FSightWeaveVisionSourceDescription GatedDescription = Vision(
			ESightWeaveIlluminationPolicy::RequiresLegalIllumination, FVector(400.0, 0.0, 0.0), 350.0f, { FName(TEXT("Visible")) });
		const FSightWeaveVisionSourceHandle Bypass = Subsystem->RegisterVisionSource(BypassDescription, nullptr);
		const FSightWeaveVisionSourceHandle Gated = Subsystem->RegisterVisionSource(GatedDescription, nullptr);
		Subsystem->RegisterIlluminationSource(Light(FVector(400.0, 0.0, 0.0), 180.0f, { FName(TEXT("Visible")) }), nullptr);

		FSightWeaveSegment2D ClosedDoor = Segment(FVector2D(75.0, -80.0), FVector2D(75.0, 80.0), 1);
		FSightWeaveSegment2D OpenDoor = Segment(FVector2D(75.0, 700.0), FVector2D(75.0, 780.0), 2);
		const FSightWeaveOccluderHandle Door = Subsystem->RegisterOccluder({ ClosedDoor }, true, true, nullptr);
		FSightWeaveHardSuppressionDescription Suppression;
		Suppression.FloorId = Ground;
		Suppression.HeightRange.ZMin = 0.0f;
		Suppression.HeightRange.ZMax = 300.0f;
		Suppression.Center = FVector2D(400.0, 100.0);
		Suppression.Radius = 35.0f;
		Subsystem->RegisterHardLiveSuppression(Suppression, nullptr);

		auto Effective = [Subsystem](const FSightWeaveKnowledgeOwnerId Owner, const FSightWeaveFloorId FloorId, const FVector Point)
		{
			return Subsystem->QueryEffectiveLiveAtLocation(Owner, FloorId, Point);
		};
		AddQuery(Trace, Subsystem, TEXT("closed-occluded"), Effective(Local, Ground, FVector(150.0, 0.0, 100.0)));
		AddQuery(Trace, Subsystem, TEXT("bypass-visible"), Effective(Local, Ground, FVector(20.0, 0.0, 100.0)));
		AddQuery(Trace, Subsystem, TEXT("pure-vision"), Subsystem->QueryPureVisionAtLocation(Local, Ground, FVector(20.0, 0.0, 100.0)));
		AddQuery(Trace, Subsystem, TEXT("gated-lit"), Effective(Local, Ground, FVector(450.0, 0.0, 100.0)));
		AddQuery(Trace, Subsystem, TEXT("gated-dark"), Effective(Local, Ground, FVector(650.0, 0.0, 100.0)));
		AddQuery(Trace, Subsystem, TEXT("hard-suppressed"), Effective(Local, Ground, FVector(400.0, 100.0, 100.0)));
		AddQuery(Trace, Subsystem, TEXT("source-specific-bypass"),
			Subsystem->QueryVisionSourceHardLiveAtLocation(Bypass, Local, Ground, FVector(20.0, 0.0, 100.0)));
		AddQuery(Trace, Subsystem, TEXT("source-specific-gated"),
			Subsystem->QueryVisionSourceHardLiveAtLocation(Gated, Local, Ground, FVector(450.0, 0.0, 100.0)));
		AddQuery(Trace, Subsystem, TEXT("owner-isolation"), Effective(Remote, Ground, FVector(20.0, 0.0, 100.0)));
		AddQuery(Trace, Subsystem, TEXT("inactive-floor"), Effective(Local, Upper, FVector(20.0, 0.0, 500.0)));
		AddQuery(Trace, Subsystem, TEXT("height-mismatch"), Effective(Local, Ground, FVector(20.0, 0.0, 500.0)));
		AddQuery(Trace, Subsystem, TEXT("wall-boundary"), Effective(Local, Ground, FVector(75.0, 0.0, 100.0)));
		AddQuery(Trace, Subsystem, TEXT("wall-boundary-inside-epsilon"), Effective(Local, Ground, FVector(74.96, 0.0, 100.0)));
		AddQuery(Trace, Subsystem, TEXT("wall-boundary-outside-epsilon"), Effective(Local, Ground, FVector(75.06, 0.0, 100.0)));

		Trace.SnapshotRevisions.Add(Subsystem->GetPublishedSnapshot().Revision.GetValue());
		Trace.DoorRevisions.Add(Subsystem->GetOccluderGeometryRevision(Door).GetValue());
		const int64 BeforeNoChangeVision = Subsystem->GetPublishedSnapshot().Revision.GetValue();
		Test.TestTrue(TEXT("No-change source update succeeds"), Subsystem->UpdateVisionSource(Bypass, BypassDescription));
		Trace.bNoChangeVisionPreservedRevision = Subsystem->GetPublishedSnapshot().Revision.GetValue() == BeforeNoChangeVision;

		Test.TestTrue(TEXT("Door opens"), Subsystem->UpdateOccluder(Door, { OpenDoor }, true, true));
		Trace.SnapshotRevisions.Add(Subsystem->GetPublishedSnapshot().Revision.GetValue());
		Trace.DoorRevisions.Add(Subsystem->GetOccluderGeometryRevision(Door).GetValue());
		AddQuery(Trace, Subsystem, TEXT("open-visible"), Effective(Local, Ground, FVector(150.0, 0.0, 100.0)));
		const int64 BeforeNoChangeDoor = Subsystem->GetPublishedSnapshot().Revision.GetValue();
		Test.TestTrue(TEXT("No-change door update succeeds"), Subsystem->UpdateOccluder(Door, { OpenDoor }, true, true));
		Trace.bNoChangeDoorPreservedRevision = Subsystem->GetPublishedSnapshot().Revision.GetValue() == BeforeNoChangeDoor;

		GatedDescription.Compatibility.AcceptedCapabilities = { FName(TEXT("Infrared")) };
		Test.TestTrue(TEXT("Capability profile changes"), Subsystem->UpdateVisionSource(Gated, GatedDescription));
		Trace.SnapshotRevisions.Add(Subsystem->GetPublishedSnapshot().Revision.GetValue());
		AddQuery(Trace, Subsystem, TEXT("capability-rejected"), Effective(Local, Ground, FVector(450.0, 0.0, 100.0)));
		GatedDescription.Compatibility.AcceptedCapabilities = { FName(TEXT("Visible")) };
		Test.TestTrue(TEXT("Capability profile restores"), Subsystem->UpdateVisionSource(Gated, GatedDescription));
		AddQuery(Trace, Subsystem, TEXT("capability-restored"), Effective(Local, Ground, FVector(450.0, 0.0, 100.0)));

		FSightWeaveQuerySampleSet Samples;
		Samples.Samples = { FVector(20.0, 0.0, 100.0), FVector(900.0, 0.0, 100.0) };
		Samples.Rule = ESightWeaveSampleRule::AnySample;
		AddQuery(Trace, Subsystem, TEXT("samples-any"), Subsystem->QuerySamples(Local, Ground, Samples));
		Samples.Rule = ESightWeaveSampleRule::AllSamples;
		AddQuery(Trace, Subsystem, TEXT("samples-all"), Subsystem->QuerySamples(Local, Ground, Samples));

		FSightWeaveQueryRequest Inside;
		Inside.KnowledgeOwnerId = Local;
		Inside.FloorId = Ground;
		Inside.SampleSet.Samples = { FVector(20.0, 0.0, 100.0) };
		FSightWeaveQueryRequest Outside = Inside;
		Outside.SampleSet.Samples = { FVector(900.0, 0.0, 100.0) };
		TArray<FSightWeaveVisibilityQueryResult> Batch;
		Subsystem->QueryBatch({ Inside, Outside }, Batch);
		for (int32 Index = 0; Index < Batch.Num(); ++Index)
		{
			AddQuery(Trace, Subsystem, Index == 0 ? TEXT("batch-inside") : TEXT("batch-outside"), Batch[Index]);
		}

		Test.TestTrue(TEXT("Door recloses"), Subsystem->UpdateOccluder(Door, { ClosedDoor }, true, true));
		Trace.SnapshotRevisions.Add(Subsystem->GetPublishedSnapshot().Revision.GetValue());
		Trace.DoorRevisions.Add(Subsystem->GetOccluderGeometryRevision(Door).GetValue());
		AddQuery(Trace, Subsystem, TEXT("reclosed-occluded"), Effective(Local, Ground, FVector(150.0, 0.0, 100.0)));
		return Trace;
	}

	bool QueryResultsEqual(const FSightWeaveVisibilityQueryResult& A, const FSightWeaveVisibilityQueryResult& B)
	{
		return A.Status == B.Status
			&& A.KnowledgeState == B.KnowledgeState
			&& A.bVisible == B.bVisible
			&& A.bAuthoritative == B.bAuthoritative
			&& A.bInVisionPolygon == B.bInVisionPolygon
			&& A.bHasLegalIllumination == B.bHasLegalIllumination
			&& A.bUsedBypass == B.bUsedBypass
			&& A.bOccluded == B.bOccluded
			&& A.bRejectedByIllumination == B.bRejectedByIllumination
			&& A.bRejectedBySuppression == B.bRejectedBySuppression
			&& A.bEligibleForMemoryWrite == B.bEligibleForMemoryWrite
			&& A.RejectionFlags == B.RejectionFlags
			&& A.Revision == B.Revision
			&& A.SnapshotRevision == B.SnapshotRevision
			&& A.KnowledgeOwnerId == B.KnowledgeOwnerId
			&& A.FloorId == B.FloorId
			&& A.ContributingVisionSources == B.ContributingVisionSources
			&& A.ContributingIlluminationSources == B.ContributingIlluminationSources
			&& A.ContributingSuppressions == B.ContributingSuppressions
			&& A.EvaluatedSampleCount == B.EvaluatedSampleCount
			&& A.PassingSampleCount == B.PassingSampleCount;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2PManualGeometryDifferentialTest,
	"SightWeave.M2P.Differential.Geometry.ManualAdversarial",
	SightWeave::M2P::DifferentialTests::TestFlags)

bool FSightWeaveM2PManualGeometryDifferentialTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P::DifferentialTests;
	TArray<TPair<FString, FSightWeaveReferenceSolveInput>> Cases;
	Cases.Emplace(TEXT("unoccluded-radial"), RadialInput());

	FSightWeaveReferenceSolveInput Cone = RadialInput();
	Cone.Shape = ESightWeaveSourceShape::DirectionalCone;
	Cone.Forward = FVector2D(0.6, 0.8);
	Cone.HalfAngleDegrees = 37.0;
	Cone.NearAwarenessRadius = 65.0;
	Cases.Emplace(TEXT("directional-cone-near-awareness"), Cone);

	FSightWeaveReferenceSolveInput LongWall = RadialInput();
	LongWall.Segments = { Segment(FVector2D(350.0, -2000.0), FVector2D(350.0, 2000.0), 1) };
	Cases.Emplace(TEXT("long-wall"), LongWall);

	FSightWeaveReferenceSolveInput Corners = RadialInput();
	Corners.Segments = {
		Segment(FVector2D(250.0, -400.0), FVector2D(250.0, 400.0), 1),
		Segment(FVector2D(250.0, 0.0), FVector2D(700.0, 0.0), 2),
		Segment(FVector2D(-500.0, 300.0), FVector2D(500.0, 300.0), 3) };
	Cases.Emplace(TEXT("l-and-t-corners"), Corners);

	FSightWeaveReferenceSolveInput Collinear = RadialInput();
	Collinear.Segments = {
		Segment(FVector2D(500.0, -600.0), FVector2D(500.0, -100.0), 1),
		Segment(FVector2D(500.0, -100.0), FVector2D(500.0, 100.0), 2),
		Segment(FVector2D(500.0, 100.0), FVector2D(500.0, 600.0), 3),
		Segment(FVector2D(500.0, 100.0), FVector2D(500.0, -100.0), 4) };
	Cases.Emplace(TEXT("collinear-shared-and-duplicate-endpoints"), Collinear);

	FSightWeaveReferenceSolveInput Doorway = RadialInput();
	Doorway.Segments = {
		Segment(FVector2D(500.0, -1200.0), FVector2D(500.0, -0.06), 1),
		Segment(FVector2D(500.0, 0.06), FVector2D(500.0, 1200.0), 2) };
	Cases.Emplace(TEXT("narrow-opening-near-weld-threshold"), Doorway);

	FSightWeaveReferenceSolveInput OnEdge = RadialInput();
	OnEdge.Segments = { Segment(FVector2D(0.0, -1000.0), FVector2D(0.0, 1000.0), 1) };
	Cases.Emplace(TEXT("source-on-edge"), OnEdge);
	OnEdge.Origin.X = -0.06;
	Cases.Emplace(TEXT("source-near-edge"), OnEdge);

	FSightWeaveReferenceSolveInput Filtered = RadialInput();
	Filtered.Segments = {
		Segment(FVector2D(300.0, -1000.0), FVector2D(300.0, 1000.0), 1, Upper),
		Segment(FVector2D(500.0, -1000.0), FVector2D(500.0, 1000.0), 2, Ground, 400.0f, 500.0f),
		Segment(FVector2D(700.0, -1000.0), FVector2D(700.0, 1000.0), 3, Ground, 100.0f, 200.0f) };
	Cases.Emplace(TEXT("floor-and-height-filtering"), Filtered);

	for (const TPair<FString, FSightWeaveReferenceSolveInput>& Case : Cases)
	{
		CompareSolve(*this, Case.Key, Case.Value, true);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2PRandomGeometryDifferentialTest,
	"SightWeave.M2P.Differential.Geometry.FixedSeedRandom",
	SightWeave::M2P::DifferentialTests::TestFlags)

bool FSightWeaveM2PRandomGeometryDifferentialTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P::DifferentialTests;
	FRandomStream Random(0x51A7E);
	constexpr int32 CaseCount = 96;
	for (int32 CaseIndex = 0; CaseIndex < CaseCount; ++CaseIndex)
	{
		FSightWeaveReferenceSolveInput Input = RadialInput();
		Input.Shape = static_cast<ESightWeaveSourceShape>(CaseIndex % 3);
		Input.Origin = FVector(Random.FRandRange(-75.0f, 75.0f), Random.FRandRange(-75.0f, 75.0f), 100.0f);
		const double ForwardAngle = Random.FRandRange(-PI, PI);
		Input.Forward = FVector2D(FMath::Cos(ForwardAngle), FMath::Sin(ForwardAngle));
		Input.Range = Random.FRandRange(500.0f, 1200.0f);
		Input.HalfAngleDegrees = Input.Shape == ESightWeaveSourceShape::Radial ? 180.0 : Random.FRandRange(15.0f, 120.0f);
		Input.NearAwarenessRadius = Input.Shape == ESightWeaveSourceShape::CameraCone ? Random.FRandRange(0.0f, 100.0f) : 0.0;
		Input.Tolerances.RadialBoundarySteps = 32 + 16 * (CaseIndex % 7);
		Input.Segments = RandomTangentialSegments(Random, 8 + CaseIndex % 57);
		CompareSolve(*this, FString::Printf(TEXT("fixed-seed-random-%03d"), CaseIndex), Input, false);
	}
	AddInfo(FString::Printf(TEXT("Compared %d fixed-seed random Reference/Optimized solves"), CaseCount));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2PRuntimeAuthorityDifferentialTest,
	"SightWeave.M2P.Differential.Runtime.AuthorityAndUpdates",
	SightWeave::M2P::DifferentialTests::TestFlags)

bool FSightWeaveM2PRuntimeAuthorityDifferentialTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P::DifferentialTests;
	const FRuntimeTrace Reference = BuildRuntimeTrace(*this, ESightWeaveSolverMode::Reference);
	const FRuntimeTrace Optimized = BuildRuntimeTrace(*this, ESightWeaveSolverMode::Optimized);
	TestEqual(TEXT("Runtime trace query count matches"), Reference.Queries.Num(), Optimized.Queries.Num());
	for (int32 Index = 0; Index < FMath::Min(Reference.Queries.Num(), Optimized.Queries.Num()); ++Index)
	{
		TestEqual(TEXT("Runtime trace labels match"), Reference.Queries[Index].Label, Optimized.Queries[Index].Label);
		TestTrue(
			*FString::Printf(TEXT("Reference/Optimized authority result matches: %s"), *Reference.Queries[Index].Label),
			QueryResultsEqual(Reference.Queries[Index].Result, Optimized.Queries[Index].Result));
	}
	TestTrue(TEXT("Reference no-change vision update preserves revision"), Reference.bNoChangeVisionPreservedRevision);
	TestTrue(TEXT("Optimized no-change vision update preserves revision"), Optimized.bNoChangeVisionPreservedRevision);
	TestTrue(TEXT("Reference no-change door update preserves revision"), Reference.bNoChangeDoorPreservedRevision);
	TestTrue(TEXT("Optimized no-change door update preserves revision"), Optimized.bNoChangeDoorPreservedRevision);
	TestTrue(TEXT("Reference queries never observe a stale snapshot"), Reference.bEveryQueryUsedCurrentSnapshot);
	TestTrue(TEXT("Optimized queries never observe a stale snapshot"), Optimized.bEveryQueryUsedCurrentSnapshot);
	TestTrue(TEXT("Snapshot revision sequence matches"), Reference.SnapshotRevisions == Optimized.SnapshotRevisions);
	TestTrue(TEXT("Door geometry revision sequence matches"), Reference.DoorRevisions == Optimized.DoorRevisions);
	return true;
}

#endif
