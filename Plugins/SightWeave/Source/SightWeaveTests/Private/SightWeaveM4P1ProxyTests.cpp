#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SightWeaveLastSeenProxyComponent.h"

namespace SightWeaveM4P1ProxyTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter;

	FSightWeaveLastSeenSnapshotDescriptor MakeSnapshot()
	{
		FSightWeaveLastSeenSnapshotDescriptor Snapshot;
		Snapshot.Identity.StableId = FName(TEXT("ProxySubject"));
		Snapshot.Identity.InstanceGeneration = 1;
		Snapshot.Scope.WorldIdentity = FSightWeaveRenderWorldIdentity { 801 };
		Snapshot.Scope.WorldGeneration = 801;
		Snapshot.Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("Local")));
		Snapshot.Scope.FloorId = FSightWeaveFloorId(FName(TEXT("Ground")));
		Snapshot.Scope.FloorOrigin = FVector2D(-1000.0, -1000.0);
		Snapshot.Scope.FloorPlaneZ = 0.0f;
		Snapshot.Scope.PrecisionTier = ESightWeaveRenderPrecisionTier::Coarse;
		FSightWeaveRenderProfileIdentity& Profile =
			Snapshot.Scope.CanonicalProfiles.AddDefaulted_GetRef();
		Profile.CanonicalCapabilities = { FName(TEXT("Visible")) };
		Profile.StableHash = 0x9876;
		Snapshot.Policy = ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot;
		Snapshot.SnapshotRevision = 4;
		Snapshot.EligibilityRevision = 20;
		Snapshot.SourceLiveRevision = 30;
		Snapshot.WorldTransform = FTransform(FRotator(0.0, 45.0, 0.0), FVector(100.0, 200.0, 300.0));
		Snapshot.WorldBounds = FBox(FVector(50.0, 150.0, 250.0), FVector(150.0, 250.0, 350.0));
		Snapshot.StaticMeshAsset = FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"));
		Snapshot.MaterialOverrides = {
			FSoftObjectPath(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"))
		};
		Snapshot.VisualVariantId = FName(TEXT("Opaque"));
		Snapshot.CaptureReason = ESightWeaveSubjectCaptureReason::LiveToNonLive;
		Snapshot.CaptureTransitionIdentity = 40;
		Snapshot.Validity = SightWeave::SubjectMemory::RequiredBasicSnapshotValidity;
		return Snapshot;
	}
}

using namespace SightWeaveM4P1ProxyTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P1ProxyLifecycleTest,
	"SightWeave.M4P1.Proxy.RenderOnlyLifecycle",
	Flags)

bool FSightWeaveM4P1ProxyLifecycleTest::RunTest(const FString& Parameters)
{
	USightWeaveLastSeenProxyComponent* Proxy =
		NewObject<USightWeaveLastSeenProxyComponent>(GetTransientPackage());
	TestNotNull(TEXT("Transient proxy created"), Proxy);
	if (!Proxy)
	{
		return true;
	}
	TestTrue(TEXT("Proxy begins render-only"), Proxy->HasRenderOnlyConfiguration());
	TestFalse(TEXT("Proxy begins hidden"), Proxy->IsVisible());
	TestTrue(TEXT("Proxy begins without stale mesh"), Proxy->GetStaticMesh() == nullptr);

	const FSightWeaveLastSeenSnapshotDescriptor Snapshot = MakeSnapshot();
	FSightWeaveSubjectPresentationResult Presentation;
	Presentation.State = ESightWeaveSubjectPresentationState::LastSeenProxy;
	Presentation.SnapshotRevision = Snapshot.SnapshotRevision;
	TestTrue(TEXT("Matching immutable snapshot presents"),
		Proxy->PresentSnapshot(Snapshot, Presentation));
	TestTrue(TEXT("Presented proxy visible"), Proxy->IsVisible());
	TestTrue(TEXT("Presented proxy owns mesh only"), Proxy->GetStaticMesh() != nullptr);
	TestEqual(TEXT("Presented revision retained"), Proxy->GetPresentedSnapshotRevision(), uint64(4));
	TestEqual(TEXT("Frozen transform applied"), Proxy->GetComponentTransform().GetLocation(),
		FVector(100.0, 200.0, 300.0));
	TestTrue(TEXT("Presentation preserves render-only constraints"),
		Proxy->HasRenderOnlyConfiguration());

	Presentation.SnapshotRevision = 3;
	TestFalse(TEXT("Stale presentation revision fails closed"),
		Proxy->PresentSnapshot(Snapshot, Presentation));
	TestFalse(TEXT("Revision failure hides proxy"), Proxy->IsVisible());
	TestTrue(TEXT("Revision failure clears stale mesh"), Proxy->GetStaticMesh() == nullptr);

	Presentation.SnapshotRevision = Snapshot.SnapshotRevision;
	TestTrue(TEXT("Valid descriptor can be presented again"),
		Proxy->PresentSnapshot(Snapshot, Presentation));
	Proxy->DestroyComponent();
	TestEqual(TEXT("Teardown clears presented revision"),
		Proxy->GetPresentedSnapshotRevision(), uint64(0));
	TestFalse(TEXT("Teardown hides proxy"), Proxy->IsVisible());
	TestTrue(TEXT("Teardown clears mesh"), Proxy->GetStaticMesh() == nullptr);
	return true;
}

#endif
