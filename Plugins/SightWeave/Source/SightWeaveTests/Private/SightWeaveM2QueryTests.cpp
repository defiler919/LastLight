#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformMemory.h"
#include "Misc/AutomationTest.h"
#include "RHIGlobals.h"
#include "RenderingThread.h"
#include "SightWeaveWorldSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace SightWeave::M2::QueryTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	class FTestWorld
	{
	public:
		explicit FTestWorld(const TCHAR* BaseName)
		{
			const FName WorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), FName(BaseName));
			World = NewObject<UWorld>(GetTransientPackage(), WorldName, RF_Transient);
			if (!World || !GEngine) return;
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

	const FSightWeaveFloorId Ground(FName(TEXT("Ground")));
	const FSightWeaveKnowledgeOwnerId Local(FName(TEXT("Local")));

	FSightWeaveFloorDefinition Floor(
		const FSightWeaveFloorId FloorId = Ground,
		const bool bActive = true,
		const float ZMin = 0.0f,
		const float ZMax = 300.0f)
	{
		FSightWeaveFloorDefinition Result;
		Result.FloorId = FloorId;
		Result.BoundsMin = FVector2D(-10000.0, -10000.0);
		Result.BoundsMax = FVector2D(10000.0, 10000.0);
		Result.HeightRange.ZMin = ZMin;
		Result.HeightRange.ZMax = ZMax;
		Result.bActiveForQueries = bActive;
		return Result;
	}

	FSightWeaveVisionSourceDescription Vision(
		const ESightWeaveIlluminationPolicy Policy,
		TArray<FName> Accepted = {},
		const FVector Location = FVector::ZeroVector,
		const float Range = 500.0f,
		const FSightWeaveKnowledgeOwnerId Owner = Local,
		const FSightWeaveFloorId FloorId = Ground)
	{
		FSightWeaveVisionSourceDescription Result;
		Result.KnowledgeOwnerId = Owner;
		Result.FloorId = FloorId;
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
		TArray<FName> Capabilities,
		const FVector Location = FVector::ZeroVector,
		const float Range = 500.0f,
		const FSightWeaveKnowledgeOwnerId Owner = Local,
		const FSightWeaveFloorId FloorId = Ground)
	{
		FSightWeaveIlluminationSourceDescription Result;
		Result.KnowledgeOwnerId = Owner;
		Result.FloorId = FloorId;
		Result.Transform.SetLocation(Location);
		Result.Shape = ESightWeaveSourceShape::Radial;
		Result.Range = Range;
		Result.HalfAngleDegrees = 180.0f;
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		Result.EmittedCapabilities = MoveTemp(Capabilities);
		return Result;
	}

	FSightWeaveSegment2D Wall(
		const double X,
		const double YMin,
		const double YMax,
		const FSightWeaveFloorId FloorId = Ground,
		const float ZMin = 0.0f,
		const float ZMax = 300.0f)
	{
		FSightWeaveSegment2D Result;
		Result.A = FVector2D(X, YMin);
		Result.B = FVector2D(X, YMax);
		Result.FloorId = FloorId;
		Result.HeightRange.ZMin = ZMin;
		Result.HeightRange.ZMax = ZMax;
		return Result;
	}

	bool SetupGround(FAutomationTestBase& Test, USightWeaveWorldSubsystem* Subsystem)
	{
		return Test.TestNotNull(TEXT("Subsystem exists"), Subsystem)
			&& Test.TestTrue(TEXT("Ground floor registers"), Subsystem->RegisterFloor(Floor(), nullptr));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2VisibleCompatibilityTest,
	"SightWeave.M2.Query.Compatibility.VisibleVisible",
	SightWeave::M2::QueryTests::TestFlags)

bool FSightWeaveM2VisibleCompatibilityTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2::QueryTests;
	FTestWorld World(TEXT("SightWeaveVisibleCompatibility"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (SetupGround(*this, Subsystem))
	{
		const FSightWeaveVisionSourceHandle VisionHandle = Subsystem->RegisterVisionSource(
			Vision(ESightWeaveIlluminationPolicy::RequiresLegalIllumination, { FName(TEXT("Visible")) }), nullptr);
		const FSightWeaveIlluminationSourceHandle LightHandle = Subsystem->RegisterIlluminationSource(
			Light({ FName(TEXT("Visible")) }), nullptr);
		const FSightWeaveVisibilityQueryResult Result = Subsystem->QueryEffectiveLiveAtLocation(Local, Ground, FVector(100.0, 0.0, 100.0));
		TestTrue(TEXT("Compatible coverage is authoritative live"), Result.bAuthoritative && Result.bVisible);
		TestTrue(TEXT("Vision attribution is exact"), Result.ContributingVisionSources.Contains(VisionHandle));
		TestTrue(TEXT("Illumination attribution is exact"), Result.ContributingIlluminationSources.Contains(LightHandle));
		TestTrue(TEXT("Compatible hard live may write memory later"), Result.bEligibleForMemoryWrite);
		TestFalse(TEXT("Gated coverage does not report bypass"), Result.bUsedBypass);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2CompatibilityIsolationTest,
	"SightWeave.M2.Query.Compatibility.ChannelOwnerAndFloorIsolation",
	SightWeave::M2::QueryTests::TestFlags)

bool FSightWeaveM2CompatibilityIsolationTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2::QueryTests;
	FTestWorld World(TEXT("SightWeaveCompatibilityIsolation"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (SetupGround(*this, Subsystem))
	{
		const FSightWeaveKnowledgeOwnerId OtherOwner(FName(TEXT("Other")));
		Subsystem->RegisterVisionSource(
			Vision(ESightWeaveIlluminationPolicy::RequiresLegalIllumination, { FName(TEXT("Visible")) }), nullptr);
		Subsystem->RegisterIlluminationSource(Light({ FName(TEXT("Infrared")) }), nullptr);
		Subsystem->RegisterIlluminationSource(Light({ FName(TEXT("Visible")) }, FVector::ZeroVector, 500.0f, OtherOwner), nullptr);
		const FSightWeaveVisibilityQueryResult Result = Subsystem->QueryEffectiveLiveAtLocation(Local, Ground, FVector(100.0, 0.0, 100.0));
		TestFalse(TEXT("Wrong channel and wrong owner cannot satisfy source"), Result.bVisible);
		TestTrue(TEXT("Point is geometrically in vision"), Result.bInVisionPolygon);
		TestTrue(TEXT("Illumination rejection is explicit"), Result.bRejectedByIllumination);
		TestTrue(TEXT("No foreign illumination is attributed"), Result.ContributingIlluminationSources.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2InfraredAndMultiChannelTest,
	"SightWeave.M2.Query.Compatibility.InfraredAndMultiChannel",
	SightWeave::M2::QueryTests::TestFlags)

bool FSightWeaveM2InfraredAndMultiChannelTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2::QueryTests;
	FTestWorld World(TEXT("SightWeaveInfraredMultiChannel"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (SetupGround(*this, Subsystem))
	{
		const FSightWeaveVisionSourceHandle InfraredSource = Subsystem->RegisterVisionSource(
			Vision(ESightWeaveIlluminationPolicy::RequiresLegalIllumination, { FName(TEXT("Infrared")) }), nullptr);
		const FSightWeaveVisionSourceHandle MultiSource = Subsystem->RegisterVisionSource(
			Vision(ESightWeaveIlluminationPolicy::RequiresLegalIllumination,
				{ FName(TEXT("Infrared")), FName(TEXT("Visible")), FName(TEXT("Infrared")) }), nullptr);
		Subsystem->RegisterIlluminationSource(Light({ FName(TEXT("Infrared")) }), nullptr);
		const FSightWeaveVisibilityQueryResult Result = Subsystem->QueryEffectiveLiveAtLocation(Local, Ground, FVector(100.0, 0.0, 100.0));
		TestTrue(TEXT("Infrared satisfies infrared and multi-channel profiles"), Result.bVisible);
		TestTrue(TEXT("Infrared source contributes"), Result.ContributingVisionSources.Contains(InfraredSource));
		TestTrue(TEXT("Multi-channel source contributes"), Result.ContributingVisionSources.Contains(MultiSource));
		const FSightWeaveFrameSnapshot Snapshot = Subsystem->GetPublishedSnapshot();
		const FSightWeaveVisionSnapshotEntry* Entry = Snapshot.VisionSources.FindByPredicate(
			[MultiSource](const FSightWeaveVisionSnapshotEntry& Candidate) { return Candidate.Handle == MultiSource; });
		TestNotNull(TEXT("Multi-channel entry is published"), Entry);
		if (Entry)
		{
			TestEqual(TEXT("Complete set is normalized and deduplicated"), Entry->Description.Compatibility.AcceptedCapabilities.Num(), 2);
			TestTrue(TEXT("Normalized order is stable"), Entry->Description.Compatibility.AcceptedCapabilities[0] == FName(TEXT("Infrared")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2IlluminationAloneTest,
	"SightWeave.M2.Query.EffectiveLive.IlluminationAlone",
	SightWeave::M2::QueryTests::TestFlags)

bool FSightWeaveM2IlluminationAloneTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2::QueryTests;
	FTestWorld World(TEXT("SightWeaveIlluminationAlone"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (SetupGround(*this, Subsystem))
	{
		const FSightWeaveIlluminationSourceHandle LightHandle = Subsystem->RegisterIlluminationSource(
			Light({ FName(TEXT("Visible")) }), nullptr);
		const FVector Point(100.0, 0.0, 100.0);
		const FSightWeaveIlluminationQueryResult Illumination = Subsystem->QueryLegalIlluminationAtLocation(Local, Ground, Point);
		const FSightWeaveVisibilityQueryResult Live = Subsystem->QueryEffectiveLiveAtLocation(Local, Ground, Point);
		TestTrue(TEXT("Pure illumination query sees light"), Illumination.bLegallyIlluminated);
		TestTrue(TEXT("Pure illumination attribution is exact"), Illumination.ContributingIlluminationSources.Contains(LightHandle));
		TestFalse(TEXT("Pure illumination never writes memory"), Illumination.bEligibleForMemoryWrite);
		TestFalse(TEXT("Illumination alone is not live vision"), Live.bVisible);
		TestFalse(TEXT("Illumination alone is not memory eligible"), Live.bEligibleForMemoryWrite);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2GatedExtentAndProfileUpdateTest,
	"SightWeave.M2.Query.Compatibility.GatedExtentAndProfileRevision",
	SightWeave::M2::QueryTests::TestFlags)

bool FSightWeaveM2GatedExtentAndProfileUpdateTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2::QueryTests;
	FTestWorld World(TEXT("SightWeaveGatedExtentProfile"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (SetupGround(*this, Subsystem))
	{
		FSightWeaveVisionSourceDescription Description = Vision(
			ESightWeaveIlluminationPolicy::RequiresLegalIllumination,
			{ FName(TEXT("Visible")) });
		const FSightWeaveVisionSourceHandle Source = Subsystem->RegisterVisionSource(Description, nullptr);
		Subsystem->RegisterIlluminationSource(Light({ FName(TEXT("Visible")) }, FVector::ZeroVector, 50.0f), nullptr);
		Subsystem->RegisterIlluminationSource(Light({ FName(TEXT("Infrared")) }, FVector::ZeroVector, 500.0f), nullptr);
		const FVector Point(100.0, 0.0, 100.0);
		const FSightWeaveVisibilityQueryResult RawVision = Subsystem->QueryPureVisionAtLocation(Local, Ground, Point);
		const FSightWeaveVisibilityQueryResult Before = Subsystem->QueryEffectiveLiveAtLocation(Local, Ground, Point);
		TestTrue(TEXT("Pure vision remains independent of illumination"), RawVision.bVisible);
		TestFalse(TEXT("Pure vision alone is not memory-write eligible"), RawVision.bEligibleForMemoryWrite);
		TestFalse(TEXT("Compatible light outside its own polygon does not satisfy gating"), Before.bVisible);
		const int64 BeforeRevision = Before.SnapshotRevision.GetValue();
		Description.Compatibility.AcceptedCapabilities = { FName(TEXT("Infrared")) };
		TestTrue(TEXT("Compatibility profile updates"), Subsystem->UpdateVisionSource(Source, Description));
		const FSightWeaveVisibilityQueryResult After = Subsystem->QueryEffectiveLiveAtLocation(Local, Ground, Point);
		TestTrue(TEXT("Updated infrared profile resolves infrared light"), After.bVisible);
		TestTrue(TEXT("Compatibility update advances snapshot revision"), After.SnapshotRevision.GetValue() > BeforeRevision);
		TestEqual(TEXT("Compatibility update rebuilds only the changed vision source"),
			Subsystem->GetPublishedSnapshot().RebuiltVisionPolygonCount, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2BypassTest,
	"SightWeave.M2.Query.EffectiveLive.BypassWithoutCompatibility",
	SightWeave::M2::QueryTests::TestFlags)

bool FSightWeaveM2BypassTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2::QueryTests;
	FTestWorld World(TEXT("SightWeaveBypass"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (SetupGround(*this, Subsystem))
	{
		FSightWeaveVisionSourceDescription Description = Vision(
			ESightWeaveIlluminationPolicy::BypassLegalIllumination,
			{ FName(TEXT("MustBeDiscarded")) });
		const FSightWeaveVisionSourceHandle Handle = Subsystem->RegisterVisionSource(Description, nullptr);
		const FSightWeaveVisibilityQueryResult Result = Subsystem->QueryEffectiveLiveAtLocation(Local, Ground, FVector(100.0, 0.0, 100.0));
		TestTrue(TEXT("Bypass is live without illumination"), Result.bVisible && Result.bUsedBypass);
		TestTrue(TEXT("Bypass attribution remains a normal source"), Result.ContributingVisionSources.Contains(Handle));
		TestTrue(TEXT("Bypass has no illumination attribution"), Result.ContributingIlluminationSources.IsEmpty());
		const FSightWeaveFrameSnapshot Snapshot = Subsystem->GetPublishedSnapshot();
		const FSightWeaveVisionSnapshotEntry* Entry = Snapshot.VisionSources.FindByPredicate(
			[Handle](const FSightWeaveVisionSnapshotEntry& Candidate) { return Candidate.Handle == Handle; });
		TestNotNull(TEXT("Bypass entry is published"), Entry);
		if (Entry)
		{
			TestTrue(TEXT("Bypass never carries a compatibility key"), Entry->Description.Compatibility.AcceptedCapabilities.IsEmpty());
			TestTrue(TEXT("Bypass never resolves compatible illumination"), Entry->CompatibleIlluminationSources.IsEmpty());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2OcclusionHeightFloorTest,
	"SightWeave.M2.Query.EffectiveLive.OcclusionHeightAndFloor",
	SightWeave::M2::QueryTests::TestFlags)

bool FSightWeaveM2OcclusionHeightFloorTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2::QueryTests;
	FTestWorld World(TEXT("SightWeaveOcclusionHeightFloor"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (SetupGround(*this, Subsystem))
	{
		const FSightWeaveFloorId Upper(FName(TEXT("Upper")));
		TestTrue(TEXT("Inactive upper floor registers"), Subsystem->RegisterFloor(Floor(Upper, false, 400.0f, 700.0f), nullptr));
		Subsystem->RegisterVisionSource(Vision(ESightWeaveIlluminationPolicy::BypassLegalIllumination), nullptr);
		Subsystem->RegisterOccluder({ Wall(50.0, -500.0, 500.0) }, false, true, nullptr);
		const FSightWeaveVisibilityQueryResult Blocked = Subsystem->QueryEffectiveLiveAtLocation(Local, Ground, FVector(100.0, 0.0, 100.0));
		TestFalse(TEXT("Wall blocks bypass source"), Blocked.bVisible);
		TestTrue(TEXT("Ordinary occlusion is reported"), Blocked.bOccluded);
		const FSightWeaveVisibilityQueryResult WrongHeight = Subsystem->QueryEffectiveLiveAtLocation(Local, Ground, FVector(20.0, 0.0, 500.0));
		TestFalse(TEXT("Floor height rejects query"), WrongHeight.bVisible);
		TestTrue(TEXT("Height rejection flag is explicit"), (WrongHeight.RejectionFlags & static_cast<int32>(ESightWeaveQueryRejectionReason::HeightMismatch)) != 0);
		const FSightWeaveVisibilityQueryResult WrongFloor = Subsystem->QueryEffectiveLiveAtLocation(Local, Upper, FVector(20.0, 0.0, 500.0));
		TestFalse(TEXT("Inactive different floor never leaks coverage"), WrongFloor.bVisible);
		TestTrue(TEXT("Different floor unavailability is explicit"), (WrongFloor.RejectionFlags & static_cast<int32>(ESightWeaveQueryRejectionReason::FloorUnavailable)) != 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2SuppressionTest,
	"SightWeave.M2.Query.EffectiveLive.HardSuppressionLifecycle",
	SightWeave::M2::QueryTests::TestFlags)

bool FSightWeaveM2SuppressionTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2::QueryTests;
	FTestWorld World(TEXT("SightWeaveHardSuppression"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (SetupGround(*this, Subsystem))
	{
		const FSightWeaveVisionSourceHandle Source = Subsystem->RegisterVisionSource(
			Vision(ESightWeaveIlluminationPolicy::BypassLegalIllumination), nullptr);
		FSightWeaveHardSuppressionDescription Suppression;
		Suppression.FloorId = Ground;
		Suppression.HeightRange.ZMin = 0.0f;
		Suppression.HeightRange.ZMax = 300.0f;
		Suppression.Center = FVector2D(100.0, 0.0);
		Suppression.Radius = 50.0f;
		const FSightWeaveHardSuppressionHandle SuppressionHandle = Subsystem->RegisterHardLiveSuppression(Suppression, nullptr);
		const FVector Point(100.0, 0.0, 100.0);
		const FSightWeaveVisibilityQueryResult Suppressed = Subsystem->QueryVisionSourceHardLiveAtLocation(Source, Local, Ground, Point);
		TestFalse(TEXT("Suppression is applied after bypass union"), Suppressed.bVisible);
		TestTrue(TEXT("Suppression rejection is explicit"), Suppressed.bRejectedBySuppression);
		TestTrue(TEXT("Suppression attribution is exact"), Suppressed.ContributingSuppressions.Contains(SuppressionHandle));
		FSightWeaveQuerySampleSet Samples;
		Samples.Rule = ESightWeaveSampleRule::AnySample;
		Samples.Samples = { Point };
		TestFalse(TEXT("Suppression affects multi-sample query"), Subsystem->QuerySamples(Local, Ground, Samples).bVisible);
		TestFalse(TEXT("Suppression affects bounds query"),
			Subsystem->QueryBounds(Local, Ground, FBox(Point - FVector(1.0), Point + FVector(1.0)), ESightWeaveSampleRule::AnySample).bVisible);
		FSightWeaveQueryRequest Request;
		Request.KnowledgeOwnerId = Local;
		Request.FloorId = Ground;
		Request.SampleSet = Samples;
		TArray<FSightWeaveVisibilityQueryResult> Batch;
		Subsystem->QueryBatch({ Request }, Batch);
		TestTrue(TEXT("Suppression affects batch query"), Batch.Num() == 1 && !Batch[0].bVisible);
		Suppression.Center = FVector2D(1000.0, 0.0);
		TestTrue(TEXT("Suppression updates"), Subsystem->UpdateHardLiveSuppression(SuppressionHandle, Suppression));
		TestTrue(TEXT("Moved suppression restores live query"), Subsystem->QueryEffectiveLiveAtLocation(Local, Ground, Point).bVisible);
		TestTrue(TEXT("Suppression unregisters"), Subsystem->UnregisterHardLiveSuppression(SuppressionHandle));
		TestFalse(TEXT("Stale suppression handle remains invalid"), Subsystem->IsHardLiveSuppressionHandleValid(SuppressionHandle));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2UnionAndSourceSpecificTest,
	"SightWeave.M2.Query.EffectiveLive.UnionEightRemoteAndSourceSpecific",
	SightWeave::M2::QueryTests::TestFlags)

bool FSightWeaveM2UnionAndSourceSpecificTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2::QueryTests;
	FTestWorld World(TEXT("SightWeaveUnionSources"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (SetupGround(*this, Subsystem))
	{
		TArray<FSightWeaveVisionSourceHandle> Handles;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Handles.Add(Subsystem->RegisterVisionSource(Vision(
				ESightWeaveIlluminationPolicy::BypassLegalIllumination,
				{},
				FVector(Index * 1000.0, 0.0, 0.0),
				250.0f), nullptr));
		}
		const FVector RemotePoint(7000.0, 0.0, 100.0);
		const FSightWeaveVisibilityQueryResult Union = Subsystem->QueryEffectiveLiveAtLocation(Local, Ground, RemotePoint);
		TestTrue(TEXT("Eight-source union includes remote source"), Union.bVisible);
		TestTrue(TEXT("Remote source is attributed"), Union.ContributingVisionSources.Contains(Handles.Last()));
		TestFalse(TEXT("Unrelated source-specific query does not inherit union"),
			Subsystem->QueryVisionSourceHardLiveAtLocation(Handles[0], Local, Ground, RemotePoint).bVisible);
		TestTrue(TEXT("Matching source-specific query is live"),
			Subsystem->QueryVisionSourceHardLiveAtLocation(Handles.Last(), Local, Ground, RemotePoint).bVisible);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2SampleRulesTest,
	"SightWeave.M2.Query.Samples.AnchorAnyAllRequired",
	SightWeave::M2::QueryTests::TestFlags)

bool FSightWeaveM2SampleRulesTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2::QueryTests;
	FTestWorld World(TEXT("SightWeaveSampleRules"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (SetupGround(*this, Subsystem))
	{
		Subsystem->RegisterVisionSource(Vision(ESightWeaveIlluminationPolicy::BypassLegalIllumination, {}, FVector::ZeroVector, 100.0f), nullptr);
		FSightWeaveQuerySampleSet Samples;
		Samples.Samples = { FVector(0.0, 0.0, 100.0), FVector(200.0, 0.0, 100.0) };
		Samples.AnchorIndex = 1;
		Samples.Rule = ESightWeaveSampleRule::Anchor;
		TestFalse(TEXT("Anchor uses selected sample"), Subsystem->QuerySamples(Local, Ground, Samples).bVisible);
		Samples.Rule = ESightWeaveSampleRule::AnySample;
		TestTrue(TEXT("Any sample passes"), Subsystem->QuerySamples(Local, Ground, Samples).bVisible);
		Samples.Rule = ESightWeaveSampleRule::AllSamples;
		TestFalse(TEXT("All samples rejects partial coverage"), Subsystem->QuerySamples(Local, Ground, Samples).bVisible);
		Samples.Rule = ESightWeaveSampleRule::RequiredCount;
		Samples.RequiredCount = 1;
		TestTrue(TEXT("Required count one passes"), Subsystem->QuerySamples(Local, Ground, Samples).bVisible);
		Samples.RequiredCount = 2;
		const FSightWeaveVisibilityQueryResult RequiredTwo = Subsystem->QuerySamples(Local, Ground, Samples);
		TestFalse(TEXT("Required count two rejects"), RequiredTwo.bVisible);
		TestEqual(TEXT("Sample count is reported"), RequiredTwo.EvaluatedSampleCount, 2);
		TestEqual(TEXT("Passing sample count is reported"), RequiredTwo.PassingSampleCount, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2BoundsBatchBoundaryTest,
	"SightWeave.M2.Query.BoundsBatchBoundaryAndAttribution",
	SightWeave::M2::QueryTests::TestFlags)

bool FSightWeaveM2BoundsBatchBoundaryTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2::QueryTests;
	FTestWorld World(TEXT("SightWeaveBoundsBatchBoundary"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (SetupGround(*this, Subsystem))
	{
		const FSightWeaveVisionSourceHandle Handle = Subsystem->RegisterVisionSource(
			Vision(ESightWeaveIlluminationPolicy::BypassLegalIllumination, {}, FVector::ZeroVector, 100.0f), nullptr);
		const FSightWeaveVisibilityQueryResult Boundary = Subsystem->QueryEffectiveLiveAtLocation(Local, Ground, FVector(100.0, 0.0, 100.0));
		TestTrue(TEXT("Exact polygon/range boundary is inclusive"), Boundary.bVisible);
		TestTrue(TEXT("Boundary attribution is preserved"), Boundary.ContributingVisionSources.Contains(Handle));
		const FBox Bounds(FVector(-10.0, -10.0, 90.0), FVector(10.0, 10.0, 110.0));
		TestTrue(TEXT("Bounds all-samples query passes"), Subsystem->QueryBounds(Local, Ground, Bounds, ESightWeaveSampleRule::AllSamples).bVisible);

		FSightWeaveQueryRequest Inside;
		Inside.KnowledgeOwnerId = Local;
		Inside.FloorId = Ground;
		Inside.SampleSet.Samples = { FVector(0.0, 0.0, 100.0) };
		FSightWeaveQueryRequest Outside = Inside;
		Outside.SampleSet.Samples = { FVector(500.0, 0.0, 100.0) };
		TArray<FSightWeaveVisibilityQueryResult> Results;
		Subsystem->QueryBatch({ Inside, Outside }, Results);
		TestEqual(TEXT("Batch result count matches request count"), Results.Num(), 2);
		if (Results.Num() == 2)
		{
			TestTrue(TEXT("Batch inside query passes"), Results[0].bVisible);
			TestFalse(TEXT("Batch outside query rejects"), Results[1].bVisible);
			TestEqual(TEXT("Batch uses one immutable revision"), Results[0].SnapshotRevision.GetValue(), Results[1].SnapshotRevision.GetValue());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2SnapshotTest,
	"SightWeave.M2.Query.ImmutableSnapshotRevisionAndDirtyRebuild",
	SightWeave::M2::QueryTests::TestFlags)

bool FSightWeaveM2SnapshotTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2::QueryTests;
	FTestWorld World(TEXT("SightWeaveImmutableSnapshot"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (SetupGround(*this, Subsystem))
	{
		const FSightWeaveVisionSourceHandle First = Subsystem->RegisterVisionSource(
			Vision(ESightWeaveIlluminationPolicy::BypassLegalIllumination), nullptr);
		const FSightWeaveVisionSourceHandle Second = Subsystem->RegisterVisionSource(
			Vision(ESightWeaveIlluminationPolicy::BypassLegalIllumination, {}, FVector(1000.0, 0.0, 0.0)), nullptr);
		const FSightWeaveFrameSnapshot Before = Subsystem->GetPublishedSnapshot();
		const FSightWeaveVisionSnapshotEntry* BeforeSecond = Before.VisionSources.FindByPredicate(
			[Second](const FSightWeaveVisionSnapshotEntry& Candidate) { return Candidate.Handle == Second; });
		FSightWeaveVisionSourceDescription Updated = Vision(ESightWeaveIlluminationPolicy::BypassLegalIllumination);
		Updated.Range = 750.0f;
		TestTrue(TEXT("Source update succeeds"), Subsystem->UpdateVisionSource(First, Updated));
		const FSightWeaveFrameSnapshot After = Subsystem->GetPublishedSnapshot();
		const FSightWeaveVisionSnapshotEntry* AfterSecond = After.VisionSources.FindByPredicate(
			[Second](const FSightWeaveVisionSnapshotEntry& Candidate) { return Candidate.Handle == Second; });
		TestTrue(TEXT("Registry update advances published revision"), After.Revision.GetValue() > Before.Revision.GetValue());
		TestEqual(TEXT("Only changed source polygon rebuilds"), After.RebuiltVisionPolygonCount, 1);
		TestEqual(TEXT("Copied prior snapshot stays immutable"), Before.VisionSources.Num(), 2);
		if (BeforeSecond && AfterSecond)
		{
			TestEqual(TEXT("Unchanged source keeps polygon revision"),
				AfterSecond->Polygon.Revision.GetValue(), BeforeSecond->Polygon.Revision.GetValue());
		}
		const FSightWeaveVisibilityQueryResult Query = Subsystem->QueryEffectiveLiveAtLocation(Local, Ground, FVector(700.0, 0.0, 100.0));
		TestEqual(TEXT("Query identifies the published snapshot"), Query.SnapshotRevision.GetValue(), After.Revision.GetValue());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2DynamicDoorQueryTest,
	"SightWeave.M2.Query.DynamicDoor.OpenCloseDeterminism",
	SightWeave::M2::QueryTests::TestFlags)

bool FSightWeaveM2DynamicDoorQueryTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2::QueryTests;
	FTestWorld World(TEXT("SightWeaveDynamicDoorQuery"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (SetupGround(*this, Subsystem))
	{
		Subsystem->RegisterVisionSource(Vision(ESightWeaveIlluminationPolicy::BypassLegalIllumination), nullptr);
		const FSightWeaveSegment2D Closed = Wall(50.0, -100.0, 100.0);
		const FSightWeaveSegment2D Open = Wall(50.0, 200.0, 300.0);
		const FSightWeaveOccluderHandle Door = Subsystem->RegisterOccluder({ Closed }, true, true, nullptr);
		const FVector Point(100.0, 0.0, 100.0);
		TestFalse(TEXT("Closed door blocks point"), Subsystem->QueryEffectiveLiveAtLocation(Local, Ground, Point).bVisible);
		const int64 ClosedRevision = Subsystem->GetOccluderGeometryRevision(Door).GetValue();
		TestTrue(TEXT("Door opens by local index update"), Subsystem->UpdateOccluder(Door, { Open }, true, true));
		TestTrue(TEXT("Open doorway exposes point"), Subsystem->QueryEffectiveLiveAtLocation(Local, Ground, Point).bVisible);
		TestTrue(TEXT("Door geometry revision advances"), Subsystem->GetOccluderGeometryRevision(Door).GetValue() > ClosedRevision);
		TestTrue(TEXT("Door closes repeatedly"), Subsystem->UpdateOccluder(Door, { Closed }, true, true));
		const FSightWeaveFrameSnapshot FirstClosed = Subsystem->GetPublishedSnapshot();
		TestFalse(TEXT("Reclosed door blocks point"), Subsystem->QueryEffectiveLiveAtLocation(Local, Ground, Point).bVisible);
		TestTrue(TEXT("Door opens a second time"), Subsystem->UpdateOccluder(Door, { Open }, true, true));
		TestTrue(TEXT("Door closes a second time"), Subsystem->UpdateOccluder(Door, { Closed }, true, true));
		const FSightWeaveFrameSnapshot SecondClosed = Subsystem->GetPublishedSnapshot();
		TestEqual(TEXT("Repeated closed solve has stable vertex count"),
			SecondClosed.VisionSources[0].Polygon.Vertices.Num(), FirstClosed.VisionSources[0].Polygon.Vertices.Num());
		TestFalse(TEXT("Repeated closed state remains blocked"), Subsystem->QueryEffectiveLiveAtLocation(Local, Ground, Point).bVisible);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2InvalidQueryTest,
	"SightWeave.M2.Query.InvalidInputAndStaleHandle",
	SightWeave::M2::QueryTests::TestFlags)

bool FSightWeaveM2InvalidQueryTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2::QueryTests;
	FTestWorld World(TEXT("SightWeaveInvalidQueryM2"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (SetupGround(*this, Subsystem))
	{
		TestTrue(TEXT("Invalid floor is explicit"),
			Subsystem->QueryEffectiveLiveAtLocation(Local, FSightWeaveFloorId(), FVector::ZeroVector).Status == ESightWeaveQueryStatus::InvalidFloor);
		TestTrue(TEXT("Invalid owner is explicit"),
			Subsystem->QueryEffectiveLiveAtLocation(FSightWeaveKnowledgeOwnerId(), Ground, FVector::ZeroVector).Status == ESightWeaveQueryStatus::InvalidInput);
		const FVector NaN(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f);
		TestTrue(TEXT("Non-finite point is explicit"),
			Subsystem->QueryEffectiveLiveAtLocation(Local, Ground, NaN).Status == ESightWeaveQueryStatus::InvalidInput);
		const FSightWeaveVisionSourceHandle Handle = Subsystem->RegisterVisionSource(
			Vision(ESightWeaveIlluminationPolicy::BypassLegalIllumination), nullptr);
		TestTrue(TEXT("Source unregisters"), Subsystem->UnregisterVisionSource(Handle));
		TestTrue(TEXT("Stale source-specific handle is explicit"),
			Subsystem->QueryVisionSourceHardLiveAtLocation(Handle, Local, Ground, FVector::ZeroVector).Status == ESightWeaveQueryStatus::InvalidHandle);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P2CpuWorldResourceLifetimeTest,
	"SightWeave.M4P2.ResourceLifetime.CpuWorldCycles",
	SightWeave::M2::QueryTests::TestFlags)

bool FSightWeaveM4P2CpuWorldResourceLifetimeTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2::QueryTests;
	constexpr int32 CycleCount = 40;
	FlushRenderingCommands();
	const int64 BaselineReservedVirtualBytes = GRHIGlobals.ReservedResources.VirtualSize;
	const FPlatformMemoryStats BaselineMemory = FPlatformMemory::GetStats();
	FString ReservedSamples;
	FString ProcessPhysicalSamples;
	FString ProcessVirtualSamples;
	for (int32 CycleIndex = 0; CycleIndex < CycleCount; ++CycleIndex)
	{
		{
			FTestWorld World(TEXT("SightWeaveM4P2CpuWorldLifetime"));
			USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
			if (!TestNotNull(TEXT("Lifecycle subsystem exists"), Subsystem)
				|| !TestTrue(TEXT("Lifecycle floor registers"), Subsystem->RegisterFloor(Floor(), nullptr)))
			{
				return false;
			}
		}
		FlushRenderingCommands();
		const int64 ReservedVirtualBytes = GRHIGlobals.ReservedResources.VirtualSize;
		const FPlatformMemoryStats Memory = FPlatformMemory::GetStats();
		if (CycleIndex > 0)
		{
			ReservedSamples += TEXT(",");
			ProcessPhysicalSamples += TEXT(",");
			ProcessVirtualSamples += TEXT(",");
		}
		ReservedSamples += FString::Printf(TEXT("%lld"), ReservedVirtualBytes);
		ProcessPhysicalSamples += FString::Printf(TEXT("%llu"), Memory.UsedPhysical);
		ProcessVirtualSamples += FString::Printf(TEXT("%llu"), Memory.UsedVirtual);
		TestEqual(
			*FString::Printf(TEXT("Cycle %d returns to reserved-resource baseline"), CycleIndex),
			ReservedVirtualBytes,
			BaselineReservedVirtualBytes);
	}
	AddInfo(FString::Printf(
		TEXT("M4P2_RESOURCE_LIFETIME kind=cpu_world cycles=%d baseline_reserved_virtual_bytes=%lld baseline_process_physical_bytes=%llu baseline_process_virtual_bytes=%llu reserved_virtual_samples=[%s] process_physical_samples=[%s] process_virtual_samples=[%s]"),
		CycleCount,
		BaselineReservedVirtualBytes,
		BaselineMemory.UsedPhysical,
		BaselineMemory.UsedVirtual,
		*ReservedSamples,
		*ProcessPhysicalSamples,
		*ProcessVirtualSamples));
	return true;
}

#endif
