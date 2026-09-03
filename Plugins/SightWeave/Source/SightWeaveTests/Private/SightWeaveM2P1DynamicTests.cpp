#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SightWeaveWorldSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace SightWeave::M2P1::DynamicTests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	const FSightWeaveFloorId Ground(FName(TEXT("Ground")));
	const FSightWeaveKnowledgeOwnerId Local(FName(TEXT("Local")));

	class FTestWorld
	{
	public:
		explicit FTestWorld(const TCHAR* BaseName)
		{
			const FName WorldName = MakeUniqueObjectName(
				GetTransientPackage(),
				UWorld::StaticClass(),
				FName(BaseName));
			World = NewObject<UWorld>(GetTransientPackage(), WorldName, RF_Transient);
			if (!World || !GEngine)
			{
				return;
			}
			World->WorldType = EWorldType::Game;
			FWorldContext& Context = GEngine->CreateNewWorldContext(World->WorldType);
			Context.SetCurrentWorld(World);
			World->InitializeNewWorld(UWorld::InitializationValues()
				.InitializeScenes(false)
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

	FSightWeaveFloorDefinition Floor()
	{
		FSightWeaveFloorDefinition Result;
		Result.FloorId = Ground;
		Result.BoundsMin = FVector2D(-2000.0, -2000.0);
		Result.BoundsMax = FVector2D(2000.0, 2000.0);
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		return Result;
	}

	FSightWeaveVisionSourceDescription Vision()
	{
		FSightWeaveVisionSourceDescription Result;
		Result.KnowledgeOwnerId = Local;
		Result.FloorId = Ground;
		Result.Transform.SetLocation(FVector(0.0, 0.0, 100.0));
		Result.Shape = ESightWeaveSourceShape::Radial;
		Result.Range = 1200.0f;
		Result.HalfAngleDegrees = 180.0f;
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		Result.IlluminationPolicy = ESightWeaveIlluminationPolicy::BypassLegalIllumination;
		return Result;
	}

	FSightWeaveSegment2D DoorSegment(const double X)
	{
		FSightWeaveSegment2D Result;
		Result.A = FVector2D(X, -100.0);
		Result.B = FVector2D(X, 100.0);
		Result.FloorId = Ground;
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		return Result;
	}

	bool SetupWorld(
		FAutomationTestBase& Test,
		USightWeaveWorldSubsystem* Subsystem,
		const double DoorX,
		FSightWeaveOccluderHandle& OutDoor)
	{
		if (!Test.TestNotNull(TEXT("Dynamic test subsystem exists"), Subsystem))
		{
			return false;
		}
		if (!Test.TestTrue(TEXT("Dynamic test floor registers"), Subsystem->RegisterFloor(Floor(), nullptr)))
		{
			return false;
		}
		if (!Test.TestTrue(
			TEXT("Dynamic test vision source registers"),
			Subsystem->RegisterVisionSource(Vision(), nullptr).IsValid()))
		{
			return false;
		}
		TArray<FSightWeaveSegment2D> Segments;
		Segments.Add(DoorSegment(DoorX));
		OutDoor = Subsystem->RegisterOccluder(Segments, true, true, nullptr);
		return Test.TestTrue(TEXT("Dynamic test door registers"), OutDoor.IsValid());
	}

	const FSightWeaveSegment2D* FindDoor(
		const FSightWeaveFrameSnapshot& Snapshot,
		const FSightWeaveOccluderHandle Door)
	{
		return Snapshot.OccluderSegments.FindByPredicate(
			[Door](const FSightWeaveSegment2D& Segment)
			{
				return Segment.OccluderHandle == Door;
			});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P1HeldSnapshotRapidUpdateTest,
	"SightWeave.M2P1.Publication.HeldReaderRapidUpdatesAndTeardown",
	SightWeave::M2P1::DynamicTests::TestFlags)

bool FSightWeaveM2P1HeldSnapshotRapidUpdateTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P1::DynamicTests;
	TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> HeldSnapshot;
	FSightWeaveOccluderHandle HeldDoor;
	int64 HeldRevision = 0;
	{
		FTestWorld World(TEXT("SightWeaveM2P1HeldSnapshot"));
		USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
		if (!SetupWorld(*this, Subsystem, 250.0, HeldDoor))
		{
			return false;
		}
		HeldSnapshot = Subsystem->AcquirePublishedSnapshotForTesting();
		if (!TestTrue(TEXT("Published snapshot reader is acquired"), HeldSnapshot.IsValid()))
		{
			return false;
		}
		HeldRevision = HeldSnapshot->Revision.GetValue();
		const FSightWeaveSegment2D* HeldSegment = FindDoor(*HeldSnapshot, HeldDoor);
		TestTrue(TEXT("Held reader contains original door"),
			HeldSegment && FMath::IsNearlyEqual(HeldSegment->A.X, 250.0));

		TArray<FSightWeaveSegment2D> DoorSegments;
		DoorSegments.Add(DoorSegment(250.0));
		int64 PreviousRevision = HeldRevision;
		for (int32 UpdateIndex = 0; UpdateIndex < 32; ++UpdateIndex)
		{
			const double X = UpdateIndex % 2 == 0 ? 850.0 : 250.0;
			DoorSegments[0].A.X = X;
			DoorSegments[0].B.X = X;
			TestTrue(TEXT("Rapid door update succeeds"),
				Subsystem->UpdateOccluder(HeldDoor, DoorSegments, true, true));
			const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> Current =
				Subsystem->AcquirePublishedSnapshotForTesting();
			const FSightWeaveSegment2D* CurrentSegment = Current.IsValid()
				? FindDoor(*Current, HeldDoor)
				: nullptr;
			TestTrue(TEXT("Every update publishes the current door"),
				CurrentSegment && FMath::IsNearlyEqual(CurrentSegment->A.X, X));
			TestTrue(TEXT("Every changed update advances revision"),
				Current.IsValid() && Current->Revision.GetValue() > PreviousRevision);
			if (Current.IsValid()) PreviousRevision = Current->Revision.GetValue();
		}

		HeldSegment = FindDoor(*HeldSnapshot, HeldDoor);
		TestEqual(TEXT("Held reader revision remains immutable"),
			HeldSnapshot->Revision.GetValue(), HeldRevision);
		TestTrue(TEXT("Held reader geometry remains immutable"),
			HeldSegment && FMath::IsNearlyEqual(HeldSegment->A.X, 250.0));
	}

	const FSightWeaveSegment2D* TeardownSegment = HeldSnapshot.IsValid()
		? FindDoor(*HeldSnapshot, HeldDoor)
		: nullptr;
	TestTrue(TEXT("Held plain-data snapshot survives world teardown safely"),
		HeldSnapshot.IsValid()
			&& HeldSnapshot->Revision.GetValue() == HeldRevision
			&& TeardownSegment
			&& FMath::IsNearlyEqual(TeardownSegment->A.X, 250.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P1DynamicMultiWorldIsolationTest,
	"SightWeave.M2P1.Publication.DynamicMultiWorldIsolation",
	SightWeave::M2P1::DynamicTests::TestFlags)

bool FSightWeaveM2P1DynamicMultiWorldIsolationTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P1::DynamicTests;
	FTestWorld WorldA(TEXT("SightWeaveM2P1WorldA"));
	FTestWorld WorldB(TEXT("SightWeaveM2P1WorldB"));
	USightWeaveWorldSubsystem* SubsystemA = WorldA.GetSubsystem();
	USightWeaveWorldSubsystem* SubsystemB = WorldB.GetSubsystem();
	FSightWeaveOccluderHandle DoorA;
	FSightWeaveOccluderHandle DoorB;
	if (!SetupWorld(*this, SubsystemA, 100.0, DoorA)
		|| !SetupWorld(*this, SubsystemB, 700.0, DoorB))
	{
		return false;
	}

	const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> BeforeB =
		SubsystemB->AcquirePublishedSnapshotForTesting();
	TArray<FSightWeaveSegment2D> DoorSegments;
	DoorSegments.Add(DoorSegment(400.0));
	TestTrue(TEXT("World A door updates"),
		SubsystemA->UpdateOccluder(DoorA, DoorSegments, true, true));
	const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> AfterA =
		SubsystemA->AcquirePublishedSnapshotForTesting();
	const TSharedPtr<const FSightWeaveFrameSnapshot, ESPMode::ThreadSafe> AfterB =
		SubsystemB->AcquirePublishedSnapshotForTesting();
	const FSightWeaveSegment2D* SegmentA = AfterA.IsValid() ? FindDoor(*AfterA, DoorA) : nullptr;
	const FSightWeaveSegment2D* SegmentB = AfterB.IsValid() ? FindDoor(*AfterB, DoorB) : nullptr;
	TestTrue(TEXT("World A publishes only its changed door"),
		SegmentA && FMath::IsNearlyEqual(SegmentA->A.X, 400.0));
	TestTrue(TEXT("World B retains its independent door"),
		SegmentB && FMath::IsNearlyEqual(SegmentB->A.X, 700.0));
	TestEqual(TEXT("World B revision is unchanged by World A"),
		AfterB.IsValid() ? AfterB->Revision.GetValue() : int64(-1),
		BeforeB.IsValid() ? BeforeB->Revision.GetValue() : int64(-2));
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSightWeaveAtomicSourceGroupTransform,
 "SightWeave.ObjectPolicy.AtomicSourceGroupTransform",
 SightWeave::M2P1::DynamicTests::TestFlags)
bool FSightWeaveAtomicSourceGroupTransform::RunTest(const FString& Parameters)
{
 using namespace SightWeave::M2P1::DynamicTests;
 FTestWorld W(TEXT("AtomicSourceGroup")); auto* S=W.GetSubsystem();
 TestTrue(TEXT("Floor"),S->RegisterFloor(Floor(),nullptr));
 auto V=Vision(); const auto A=S->RegisterVisionSource(V,nullptr);
 V.Range=500; const auto B=S->RegisterVisionSource(V,nullptr);
 FSightWeaveIlluminationSourceDescription L; L.KnowledgeOwnerId=Local; L.FloorId=Ground;
 L.Range=700; L.Transform=V.Transform; L.HeightRange=V.HeightRange;
 const auto C=S->RegisterIlluminationSource(L,nullptr);
 TestTrue(TEXT("Light"),C.IsValid());
 const FSightWeaveVisionSourceHandle Vs[]={A,B}; const FSightWeaveIlluminationSourceHandle Ls[]={C};
 const auto Held=S->GetPublishedSnapshot(); const auto Prior=S->GetRevision().GetValue();
 const FTransform Pose(FRotator(0,63,0),FVector(300,200,100));
 TestTrue(TEXT("One coherent publication"),S->UpdateSourceGroupTransform(Vs,Ls,Pose));
 TestEqual(TEXT("Exactly one authority revision"),S->GetRevision().GetValue(),Prior+1);
 const auto Next=S->GetPublishedSnapshot();
 for(const auto& Source:Next.VisionSources) TestTrue(TEXT("Vision pose"),Source.Description.Transform.Equals(Pose,0));
 TestTrue(TEXT("Light pose"),Next.IlluminationSources[0].Description.Transform.Equals(Pose,0));
 TestEqual(TEXT("Metadata retained"),Next.VisionSources[1].Description.Range,500.f);
 TestTrue(TEXT("Held publication immutable"),Held.VisionSources[0].Description.Transform.Equals(Vision().Transform,0));
 TestTrue(TEXT("Unchanged no-op"),S->UpdateSourceGroupTransform(Vs,Ls,Pose));
 TestEqual(TEXT("No duplicate publication"),S->GetRevision().GetValue(),Prior+1);
 const FSightWeaveIlluminationSourceHandle Bad[]={FSightWeaveIlluminationSourceHandle(99999)};
 TestFalse(TEXT("Reject invalid group atomically"),S->UpdateSourceGroupTransform(Vs,Bad,FTransform::Identity));
 TestEqual(TEXT("Rejected group does not publish"),S->GetRevision().GetValue(),Prior+1);
 TestTrue(TEXT("Rejected group leaves sources unchanged"),S->GetPublishedSnapshot().VisionSources[0].Description.Transform.Equals(Pose,0));
 return true;
}

#endif
