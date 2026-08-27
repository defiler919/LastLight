#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "SightWeaveLastSeenProxyComponent.h"

namespace SightWeaveM4P1TransitionFixtureTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter;

	FSightWeaveSubjectRegistration MakeRegistration()
	{
		FSightWeaveSubjectRegistration Registration;
		Registration.Identity.StableId = FName(TEXT("LabFixtureSubject"));
		Registration.Identity.InstanceGeneration = 1;
		Registration.Scope.WorldIdentity = FSightWeaveRenderWorldIdentity { 901 };
		Registration.Scope.WorldGeneration = 901;
		Registration.Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("Local")));
		Registration.Scope.FloorId = FSightWeaveFloorId(FName(TEXT("Ground")));
		Registration.Scope.FloorOrigin = FVector2D(-1000.0, -1000.0);
		Registration.Scope.FloorPlaneZ = 0.0f;
		Registration.Scope.PrecisionTier = ESightWeaveRenderPrecisionTier::Coarse;
		FSightWeaveRenderProfileIdentity& Profile =
			Registration.Scope.CanonicalProfiles.AddDefaulted_GetRef();
		Profile.CanonicalCapabilities = { FName(TEXT("Visible")) };
		Profile.StableHash = 0x4321;
		Registration.Policy = ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot;
		return Registration;
	}

	FSightWeaveSubjectObservation MakeObservation(
		const FSightWeaveSubjectRegistration& Registration,
		const uint64 ObservationRevision,
		const bool bHardLive,
		const uint64 TransitionIdentity = 0)
	{
		FSightWeaveSubjectObservation Observation;
		Observation.Identity = Registration.Identity;
		Observation.Scope = Registration.Scope;
		Observation.ObservationRevision = ObservationRevision;
		Observation.EligibilityRevision = 10;
		Observation.SourceLiveRevision = 20;
		Observation.TransitionIdentity = TransitionIdentity;
		Observation.bHardLive = bHardLive;
		Observation.bEligibleForMemoryWrite = true;
		Observation.BasicSnapshot.WorldTransform = FTransform(FVector(250.0, 0.0, 50.0));
		Observation.BasicSnapshot.WorldBounds =
			FBox(FVector(200.0, -50.0, 0.0), FVector(300.0, 50.0, 100.0));
		Observation.BasicSnapshot.StaticMeshAsset =
			FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"));
		Observation.BasicSnapshot.MaterialOverrides = {
			FSoftObjectPath(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"))
		};
		Observation.BasicSnapshot.bOpaqueStaticMesh = true;
		return Observation;
	}

	FSightWeaveSubjectPresentationContext MakeContext(
		const FSightWeaveSubjectRegistration& Registration,
		const FSightWeaveLastSeenSnapshotDescriptor* Snapshot,
		const bool bHardLive)
	{
		FSightWeaveSubjectPresentationContext Context;
		Context.Identity = Registration.Identity;
		Context.Scope = Registration.Scope;
		Context.bHardLive = bHardLive;
		Context.bHardMemoryAtSnapshot = true;
		if (Snapshot)
		{
			Context.SnapshotRevision = Snapshot->SnapshotRevision;
			Context.EligibilityRevision = Snapshot->EligibilityRevision;
			Context.SourceLiveRevision = Snapshot->SourceLiveRevision;
		}
		return Context;
	}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P1TransitionFixtureTest,
	"SightWeave.M4P1.Lab.GeneratedFixture.FullTransition",
	Flags)

bool FSightWeaveM4P1TransitionFixtureTest::RunTest(const FString& Parameters)
{
	FSightWeaveSubjectMemoryAuthority Authority;
	const FSightWeaveSubjectRegistration Registration = MakeRegistration();
	const FSightWeaveSubjectHandle Handle = Authority.Register(Registration);
	UStaticMeshComponent* Live = NewObject<UStaticMeshComponent>(GetTransientPackage());
	USightWeaveLastSeenProxyComponent* Proxy =
		NewObject<USightWeaveLastSeenProxyComponent>(GetTransientPackage());

	Authority.SubmitObservation(Handle, MakeObservation(Registration, 1, true));
	FSightWeaveSubjectPresentationResult Presentation = Authority.EvaluatePresentation(
		Handle,
		MakeContext(Registration, nullptr, true));
	TestTrue(TEXT("Initial HardLive applies"),
		FSightWeaveSubjectProxyPresentationBridge::Apply(Presentation, nullptr, Live, Proxy));
	TestTrue(TEXT("HardLive shows real presentation"), Live->IsVisible());
	TestFalse(TEXT("HardLive hides proxy"), Proxy->IsVisible());

	const FSightWeaveSubjectTransitionResult Lost = Authority.SubmitObservation(
		Handle,
		MakeObservation(Registration, 2, false, 100));
	TestEqual(TEXT("Falling edge captured"), Lost.Disposition,
		ESightWeaveSubjectTransitionDisposition::SnapshotCaptured);
	const FSightWeaveLastSeenSnapshotDescriptor* Snapshot = Authority.FindSnapshot(Handle);
	Presentation = Authority.EvaluatePresentation(
		Handle,
		MakeContext(Registration, Snapshot, false));
	TestTrue(TEXT("Remembered result applies"),
		FSightWeaveSubjectProxyPresentationBridge::Apply(Presentation, Snapshot, Live, Proxy));
	TestFalse(TEXT("Remembered state hides real presentation"), Live->IsVisible());
	TestTrue(TEXT("Remembered state shows proxy"), Proxy->IsVisible());

	FSightWeaveSubjectPresentationContext Suppressed = MakeContext(Registration, Snapshot, false);
	Suppressed.bSuppressMemoryPresentation = true;
	Presentation = Authority.EvaluatePresentation(Handle, Suppressed);
	TestFalse(TEXT("Suppression deliberately applies fail black"),
		FSightWeaveSubjectProxyPresentationBridge::Apply(Presentation, Snapshot, Live, Proxy));
	TestFalse(TEXT("Suppression hides real presentation"), Live->IsVisible());
	TestFalse(TEXT("Suppression hides proxy"), Proxy->IsVisible());

	Presentation = Authority.EvaluatePresentation(
		Handle,
		MakeContext(Registration, Snapshot, false));
	TestTrue(TEXT("Exact revisions restore proxy after suppression"),
		FSightWeaveSubjectProxyPresentationBridge::Apply(Presentation, Snapshot, Live, Proxy));
	Authority.SubmitObservation(Handle, MakeObservation(Registration, 3, true));
	Presentation = Authority.EvaluatePresentation(
		Handle,
		MakeContext(Registration, Snapshot, true));
	TestTrue(TEXT("Reacquire applies immediately"),
		FSightWeaveSubjectProxyPresentationBridge::Apply(Presentation, Snapshot, Live, Proxy));
	TestTrue(TEXT("Reacquire shows real presentation"), Live->IsVisible());
	TestFalse(TEXT("Reacquire clears proxy"), Proxy->IsVisible());

	FSightWeaveMemoryRegion ClearRegion;
	ClearRegion.Scope = Registration.Scope;
	ClearRegion.Shape = ESightWeaveMemoryRegionShape::Circle;
	ClearRegion.Center = FVector2D(250.0, 0.0);
	ClearRegion.Radius = 500.0f;
	TestEqual(TEXT("Clear removes descriptor"), Authority.ClearSnapshots(ClearRegion), 1);
	Presentation = Authority.EvaluatePresentation(
		Handle,
		MakeContext(Registration, nullptr, false));
	TestFalse(TEXT("Cleared state applies black"),
		FSightWeaveSubjectProxyPresentationBridge::Apply(Presentation, nullptr, Live, Proxy));
	TestFalse(TEXT("Clear leaves real presentation hidden"), Live->IsVisible());
	TestFalse(TEXT("Clear leaves proxy hidden"), Proxy->IsVisible());
	TestTrue(TEXT("Proxy remains render-only throughout"), Proxy->HasRenderOnlyConfiguration());
	return true;
}

}

#endif
