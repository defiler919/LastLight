#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "SightWeaveComponents.h"
#include "SightWeaveWorldSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace SightWeave::M2::RuntimeTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

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

		UWorld* Get() const { return World; }
		USightWeaveWorldSubsystem* GetSubsystem() const
		{
			return World ? World->GetSubsystem<USightWeaveWorldSubsystem>() : nullptr;
		}

	private:
		UWorld* World = nullptr;
	};

	FSightWeaveFloorDefinition Floor(const TCHAR* Name, bool bActive, float ZMin = 0.0f, float ZMax = 300.0f)
	{
		FSightWeaveFloorDefinition Result;
		Result.FloorId = FSightWeaveFloorId(FName(Name));
		Result.BoundsMin = FVector2D(-5000.0, -5000.0);
		Result.BoundsMax = FVector2D(5000.0, 5000.0);
		Result.HeightRange.ZMin = ZMin;
		Result.HeightRange.ZMax = ZMax;
		Result.bActiveForQueries = bActive;
		return Result;
	}

	FSightWeaveSegment2D Segment(double X, const TCHAR* FloorName = TEXT("Ground"))
	{
		FSightWeaveSegment2D Result;
		Result.A = FVector2D(X, -100.0);
		Result.B = FVector2D(X, 100.0);
		Result.FloorId = FSightWeaveFloorId(FName(FloorName));
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		return Result;
	}

	FSightWeaveHeightRange Height()
	{
		FSightWeaveHeightRange Result;
		Result.ZMin = 50.0f;
		Result.ZMax = 150.0f;
		return Result;
	}

	USceneComponent* AddRoot(AActor* Actor)
	{
		USceneComponent* Root = NewObject<USceneComponent>(Actor);
		Actor->AddInstanceComponent(Root);
		Actor->SetRootComponent(Root);
		Root->RegisterComponent();
		return Root;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2FloorLifecycleTest,
	"SightWeave.M2.Runtime.FloorLifecycleAndSingleActiveRule",
	SightWeave::M2::RuntimeTests::TestFlags)

bool FSightWeaveM2FloorLifecycleTest::RunTest(const FString& Parameters)
{
	SightWeave::M2::RuntimeTests::FTestWorld World(TEXT("SightWeaveM2FloorLifecycle"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (TestNotNull(TEXT("Subsystem exists"), Subsystem))
	{
		TestTrue(TEXT("First active floor registers"), Subsystem->RegisterFloor(
			SightWeave::M2::RuntimeTests::Floor(TEXT("Ground"), true), nullptr));
		TestFalse(TEXT("A second active query floor is rejected"), Subsystem->RegisterFloor(
			SightWeave::M2::RuntimeTests::Floor(TEXT("Upper"), true, 400.0f, 700.0f), nullptr));
		TestTrue(TEXT("An inactive stacked floor registers separately"), Subsystem->RegisterFloor(
			SightWeave::M2::RuntimeTests::Floor(TEXT("Upper"), false, 400.0f, 700.0f), nullptr));
		TestEqual(TEXT("Two distinct floors are retained"), Subsystem->GetFloorCount(), 2);
		TestTrue(TEXT("Ground remains the explicit active floor"),
			Subsystem->GetActiveFloorId() == FSightWeaveFloorId(FName(TEXT("Ground"))));
		TestTrue(TEXT("Inactive floor unregisters"), Subsystem->UnregisterFloor(FSightWeaveFloorId(FName(TEXT("Upper")))));
		TestFalse(TEXT("Removed floor identity is invalid"), Subsystem->IsFloorRegistered(FSightWeaveFloorId(FName(TEXT("Upper")))));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2OccluderLifecycleTest,
	"SightWeave.M2.Runtime.OccluderLifecycleAndRevision",
	SightWeave::M2::RuntimeTests::TestFlags)

bool FSightWeaveM2OccluderLifecycleTest::RunTest(const FString& Parameters)
{
	SightWeave::M2::RuntimeTests::FTestWorld World(TEXT("SightWeaveM2OccluderLifecycle"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (TestNotNull(TEXT("Subsystem exists"), Subsystem))
	{
		const FSightWeaveOccluderHandle Handle = Subsystem->RegisterOccluder(
			{ SightWeave::M2::RuntimeTests::Segment(100.0) }, false, true, nullptr);
		TestTrue(TEXT("Occluder handle is valid"), Handle.IsValid());
		TestTrue(TEXT("Subsystem recognizes occluder"), Subsystem->IsOccluderHandleValid(Handle));
		const int64 InitialGeometryRevision = Subsystem->GetOccluderGeometryRevision(Handle).GetValue();
		TestTrue(TEXT("Occluder update succeeds"), Subsystem->UpdateOccluder(
			Handle, { SightWeave::M2::RuntimeTests::Segment(200.0) }, false, true));
		TestTrue(TEXT("Geometry revision advances"),
			Subsystem->GetOccluderGeometryRevision(Handle).GetValue() > InitialGeometryRevision);
		TestTrue(TEXT("Occluder unregisters"), Subsystem->UnregisterOccluder(Handle));
		TestFalse(TEXT("Stale occluder handle is rejected"), Subsystem->IsOccluderHandleValid(Handle));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2DisabledOccluderTest,
	"SightWeave.M2.Runtime.DisabledOccluderIndexing",
	SightWeave::M2::RuntimeTests::TestFlags)

bool FSightWeaveM2DisabledOccluderTest::RunTest(const FString& Parameters)
{
	SightWeave::M2::RuntimeTests::FTestWorld World(TEXT("SightWeaveM2DisabledOccluder"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (TestNotNull(TEXT("Subsystem exists"), Subsystem))
	{
		const FSightWeaveOccluderHandle Handle = Subsystem->RegisterOccluder(
			{ SightWeave::M2::RuntimeTests::Segment(100.0) }, false, false, nullptr);
		TestTrue(TEXT("Disabled occluder retains a stable handle"), Handle.IsValid());
		TestEqual(TEXT("Disabled geometry is absent from the index"), Subsystem->GetSpatialIndexStats().SegmentCount, 0);
		TestTrue(TEXT("Enabling updates the existing handle"), Subsystem->UpdateOccluder(
			Handle, { SightWeave::M2::RuntimeTests::Segment(100.0) }, false, true));
		TestEqual(TEXT("Enabled geometry enters the index"), Subsystem->GetSpatialIndexStats().SegmentCount, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2DynamicDoorIndexTest,
	"SightWeave.M2.Runtime.DynamicDoorLocalUpdate",
	SightWeave::M2::RuntimeTests::TestFlags)

bool FSightWeaveM2DynamicDoorIndexTest::RunTest(const FString& Parameters)
{
	SightWeave::M2::RuntimeTests::FTestWorld World(TEXT("SightWeaveM2DynamicDoorIndex"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (TestNotNull(TEXT("Subsystem exists"), Subsystem))
	{
		const FSightWeaveOccluderHandle Handle = Subsystem->RegisterOccluder(
			{ SightWeave::M2::RuntimeTests::Segment(100.0) }, true, true, nullptr);
		const int64 ClosedRevision = Subsystem->GetOccluderGeometryRevision(Handle).GetValue();
		TestTrue(TEXT("Door transform update succeeds"), Subsystem->UpdateOccluder(
			Handle, { SightWeave::M2::RuntimeTests::Segment(1100.0) }, true, true));
		TArray<FSightWeaveSegment2D> Results;
		Subsystem->QueryOccluderSegments(FSightWeaveFloorId(FName(TEXT("Ground"))),
			FBox2D(FVector2D(0.0, -200.0), FVector2D(200.0, 200.0)),
			SightWeave::M2::RuntimeTests::Height(), Results);
		TestEqual(TEXT("Closed-position cells contain no stale door"), Results.Num(), 0);
		Subsystem->QueryOccluderSegments(FSightWeaveFloorId(FName(TEXT("Ground"))),
			FBox2D(FVector2D(1000.0, -200.0), FVector2D(1200.0, 200.0)),
			SightWeave::M2::RuntimeTests::Height(), Results);
		TestEqual(TEXT("New-position cells contain one door segment"), Results.Num(), 1);
		TestTrue(TEXT("Door geometry revision advances"),
			Subsystem->GetOccluderGeometryRevision(Handle).GetValue() > ClosedRevision);
		TestEqual(TEXT("Dynamic update counter advances once"),
			Subsystem->GetSpatialIndexStats().DynamicUpdateCount, int64(1));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2AffectedDirtySourcesTest,
	"SightWeave.M2.Runtime.LocalDirtyInvalidation",
	SightWeave::M2::RuntimeTests::TestFlags)

bool FSightWeaveM2AffectedDirtySourcesTest::RunTest(const FString& Parameters)
{
	SightWeave::M2::RuntimeTests::FTestWorld World(TEXT("SightWeaveM2AffectedDirtySources"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (TestNotNull(TEXT("Subsystem exists"), Subsystem))
	{
		FSightWeaveVisionSourceDescription NearVision;
		NearVision.FloorId = FSightWeaveFloorId(FName(TEXT("Ground")));
		NearVision.Range = 1000.0f;
		FSightWeaveVisionSourceDescription FarVision = NearVision;
		FarVision.Transform.SetLocation(FVector(5000.0, 0.0, 0.0));
		Subsystem->RegisterVisionSource(NearVision, nullptr);
		Subsystem->RegisterVisionSource(FarVision, nullptr);

		FSightWeaveIlluminationSourceDescription NearLight;
		NearLight.FloorId = FSightWeaveFloorId(FName(TEXT("Ground")));
		NearLight.Range = 1000.0f;
		FSightWeaveIlluminationSourceDescription FarLight = NearLight;
		FarLight.Transform.SetLocation(FVector(5000.0, 0.0, 0.0));
		Subsystem->RegisterIlluminationSource(NearLight, nullptr);
		Subsystem->RegisterIlluminationSource(FarLight, nullptr);
		Subsystem->ClearDirtySourceFlags();

		Subsystem->RegisterOccluder({ SightWeave::M2::RuntimeTests::Segment(500.0) }, true, true, nullptr);
		TestEqual(TEXT("Only nearby vision source becomes dirty"), Subsystem->GetDirtyVisionSourceCount(), 1);
		TestEqual(TEXT("Only nearby illumination source becomes dirty"), Subsystem->GetDirtyIlluminationSourceCount(), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2ComponentRegistrationTest,
	"SightWeave.M2.Runtime.ComponentRegistrationLifecycle",
	SightWeave::M2::RuntimeTests::TestFlags)

bool FSightWeaveM2ComponentRegistrationTest::RunTest(const FString& Parameters)
{
	SightWeave::M2::RuntimeTests::FTestWorld World(TEXT("SightWeaveM2ComponentRegistration"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	AActor* Actor = World.Get() ? World.Get()->SpawnActor<AActor>() : nullptr;
	if (TestNotNull(TEXT("Subsystem exists"), Subsystem) && TestNotNull(TEXT("Fixture actor exists"), Actor))
	{
		USceneComponent* Root = SightWeave::M2::RuntimeTests::AddRoot(Actor);
		USightWeaveFloorComponent* Floor = NewObject<USightWeaveFloorComponent>(Actor);
		Floor->SetupAttachment(Root);
		Actor->AddInstanceComponent(Floor);
		Floor->RegisterComponent();
		USightWeaveVisionSourceComponent* Vision = NewObject<USightWeaveVisionSourceComponent>(Actor);
		Vision->SetupAttachment(Root);
		Actor->AddInstanceComponent(Vision);
		Vision->RegisterComponent();
		USightWeaveIlluminationSourceComponent* Illumination = NewObject<USightWeaveIlluminationSourceComponent>(Actor);
		Illumination->SetupAttachment(Root);
		Actor->AddInstanceComponent(Illumination);
		Illumination->RegisterComponent();
		USightWeaveOccluderComponent* Occluder = NewObject<USightWeaveOccluderComponent>(Actor);
		Occluder->SetupAttachment(Root);
		Actor->AddInstanceComponent(Occluder);
		Occluder->RegisterComponent();

		TestTrue(TEXT("Floor component self-registers"), Floor->IsFloorRegistered());
		TestTrue(TEXT("Vision component receives a strong handle"), Vision->GetVisionSourceHandle().IsValid());
		TestTrue(TEXT("Illumination component receives a distinct strong handle"), Illumination->GetIlluminationSourceHandle().IsValid());
		TestTrue(TEXT("Occluder component receives a distinct strong handle"), Occluder->GetOccluderHandle().IsValid());
		TestEqual(TEXT("Subsystem contains one registration of each component category"),
			Subsystem->GetFloorCount() + Subsystem->GetVisionSourceCount() + Subsystem->GetIlluminationSourceCount() + Subsystem->GetOccluderCount(), 4);

		Actor->Destroy();
		TestEqual(TEXT("Actor destruction unregisters its floor"), Subsystem->GetFloorCount(), 0);
		TestEqual(TEXT("Actor destruction unregisters its vision source"), Subsystem->GetVisionSourceCount(), 0);
		TestEqual(TEXT("Actor destruction unregisters its illumination source"), Subsystem->GetIlluminationSourceCount(), 0);
		TestEqual(TEXT("Actor destruction unregisters its occluder"), Subsystem->GetOccluderCount(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2TransformValidationTest,
	"SightWeave.M2.Runtime.OccluderTransformValidation",
	SightWeave::M2::RuntimeTests::TestFlags)

bool FSightWeaveM2TransformValidationTest::RunTest(const FString& Parameters)
{
	SightWeave::M2::RuntimeTests::FTestWorld World(TEXT("SightWeaveM2TransformValidation"));
	AActor* Actor = World.Get() ? World.Get()->SpawnActor<AActor>() : nullptr;
	if (TestNotNull(TEXT("Fixture actor exists"), Actor))
	{
		USceneComponent* Root = SightWeave::M2::RuntimeTests::AddRoot(Actor);
		USightWeaveOccluderComponent* Occluder = NewObject<USightWeaveOccluderComponent>(Actor);
		Occluder->SetupAttachment(Root);
		Occluder->SetRelativeScale3D(FVector(2.0, 1.0, 1.0));
		Actor->AddInstanceComponent(Occluder);
		Occluder->RegisterComponent();
		TestFalse(TEXT("Non-uniform XY scale is explicitly rejected"), Occluder->GetOccluderHandle().IsValid());
		TestTrue(TEXT("Rejected transform exposes a validation diagnostic"), !Occluder->GetLastValidationError().IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2ComponentDoorTransformTest,
	"SightWeave.M2.Runtime.DynamicDoorComponentTransform",
	SightWeave::M2::RuntimeTests::TestFlags)

bool FSightWeaveM2ComponentDoorTransformTest::RunTest(const FString& Parameters)
{
	SightWeave::M2::RuntimeTests::FTestWorld World(TEXT("SightWeaveM2ComponentDoorTransform"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	AActor* Actor = World.Get() ? World.Get()->SpawnActor<AActor>() : nullptr;
	if (TestNotNull(TEXT("Subsystem exists"), Subsystem) && TestNotNull(TEXT("Fixture actor exists"), Actor))
	{
		USceneComponent* Root = SightWeave::M2::RuntimeTests::AddRoot(Actor);
		USightWeaveOccluderComponent* Door = NewObject<USightWeaveOccluderComponent>(Actor);
		Door->SetupAttachment(Root);
		Door->bDynamic = true;
		Actor->AddInstanceComponent(Door);
		Door->RegisterComponent();
		const FSightWeaveOccluderHandle Handle = Door->GetOccluderHandle();
		const int64 ClosedRevision = Subsystem->GetOccluderGeometryRevision(Handle).GetValue();
		Door->SetRelativeLocation(FVector(1000.0, 0.0, 0.0));
		TArray<FSightWeaveSegment2D> Results;
		Subsystem->QueryOccluderSegments(FSightWeaveFloorId(FName(TEXT("Default"))),
			FBox2D(FVector2D(-100.0, -100.0), FVector2D(100.0, 100.0)),
			SightWeave::M2::RuntimeTests::Height(), Results);
		TestEqual(TEXT("Component transform removes old-cell geometry"), Results.Num(), 0);
		Subsystem->QueryOccluderSegments(FSightWeaveFloorId(FName(TEXT("Default"))),
			FBox2D(FVector2D(900.0, -100.0), FVector2D(1100.0, 100.0)),
			SightWeave::M2::RuntimeTests::Height(), Results);
		TestEqual(TEXT("Component transform inserts new-cell geometry"), Results.Num(), 1);
		TestTrue(TEXT("Component transform advances geometry revision"),
			Subsystem->GetOccluderGeometryRevision(Handle).GetValue() > ClosedRevision);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2HardSuppressionComponentLifecycleTest,
	"SightWeave.M2.Runtime.HardSuppressionComponentLifecycle",
	SightWeave::M2::RuntimeTests::TestFlags)

bool FSightWeaveM2HardSuppressionComponentLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2::RuntimeTests;
	FTestWorld World(TEXT("SightWeaveM2HardSuppressionComponent"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	AActor* Actor = World.Get() ? World.Get()->SpawnActor<AActor>() : nullptr;
	if (TestNotNull(TEXT("Subsystem exists"), Subsystem) && TestNotNull(TEXT("Fixture actor exists"), Actor))
	{
		TestTrue(TEXT("Ground floor registers"), Subsystem->RegisterFloor(Floor(TEXT("Ground"), true), nullptr));
		USceneComponent* Root = AddRoot(Actor);
		USightWeaveHardSuppressionComponent* Component = NewObject<USightWeaveHardSuppressionComponent>(Actor);
		Component->Description.FloorId = FSightWeaveFloorId(FName(TEXT("Ground")));
		Component->Description.HeightRange.ZMin = 0.0f;
		Component->Description.HeightRange.ZMax = 300.0f;
		Component->Description.Radius = 100.0f;
		Actor->AddInstanceComponent(Component);
		Component->SetupAttachment(Root);
		Component->RegisterComponent();
		TestTrue(TEXT("Authoring component registers suppression"), Component->GetHardSuppressionHandle().IsValid());
		TestEqual(TEXT("Subsystem owns one suppression"), Subsystem->GetHardLiveSuppressionCount(), 1);
		Component->SetRelativeLocation(FVector(250.0, 0.0, 0.0));
		const FSightWeaveFrameSnapshot Snapshot = Subsystem->GetPublishedSnapshot();
		TestTrue(TEXT("Component transform updates suppression center"),
			Snapshot.HardSuppressions.Num() == 1
			&& FMath::IsNearlyEqual(Snapshot.HardSuppressions[0].Description.Center.X, 250.0));
		Actor->Destroy();
		TestEqual(TEXT("Actor destruction unregisters suppression"), Subsystem->GetHardLiveSuppressionCount(), 0);
	}
	return true;
}

#endif
