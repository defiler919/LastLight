#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SightWeaveSubjectMemory.h"

namespace SightWeaveM4P1SubjectPolicyTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter;

	FSightWeaveMemoryScopeKey MakeScope(
		const uint64 WorldSerial = 701,
		const TCHAR* Owner = TEXT("Local"),
		const TCHAR* Floor = TEXT("Ground"),
		const ESightWeaveRenderPrecisionTier Precision =
			ESightWeaveRenderPrecisionTier::Coarse,
		const TCHAR* Profile = TEXT("Visible"))
	{
		FSightWeaveMemoryScopeKey Scope;
		Scope.WorldIdentity = FSightWeaveRenderWorldIdentity { WorldSerial };
		Scope.WorldGeneration = WorldSerial;
		Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(Owner));
		Scope.FloorId = FSightWeaveFloorId(FName(Floor));
		Scope.FloorOrigin = FVector2D(-1000.0, -1000.0);
		Scope.FloorPlaneZ = 0.0f;
		Scope.PrecisionTier = Precision;
		FSightWeaveRenderProfileIdentity& Identity = Scope.CanonicalProfiles.AddDefaulted_GetRef();
		Identity.CanonicalCapabilities = { FName(Profile) };
		Identity.StableHash = 0x1234;
		return Scope;
	}

	FSightWeaveSubjectRegistration MakeRegistration(
		const ESightWeaveSubjectMemoryPolicy Policy,
		const int64 Generation = 1,
		const FSightWeaveMemoryScopeKey* ScopeOverride = nullptr)
	{
		FSightWeaveSubjectRegistration Registration;
		Registration.Identity.StableId = FName(TEXT("SubjectA"));
		Registration.Identity.InstanceGeneration = Generation;
		Registration.Scope = ScopeOverride ? *ScopeOverride : MakeScope();
		Registration.Policy = Policy;
		if (Policy == ESightWeaveSubjectMemoryPolicy::Custom)
		{
			Registration.CustomProviderName = FName(TEXT("TestProvider"));
			Registration.CustomProviderVersion = 1;
		}
		return Registration;
	}

	FSightWeaveBasicStaticMeshSnapshotCandidate MakeCandidate(const FVector Location)
	{
		FSightWeaveBasicStaticMeshSnapshotCandidate Candidate;
		Candidate.WorldTransform = FTransform(Location);
		Candidate.WorldBounds = FBox(Location - FVector(50.0), Location + FVector(50.0));
		Candidate.StaticMeshAsset = FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"));
		Candidate.MaterialOverrides = {
			FSoftObjectPath(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"))
		};
		Candidate.VisualVariantId = FName(TEXT("Default"));
		Candidate.bOpaqueStaticMesh = true;
		return Candidate;
	}

	FSightWeaveSubjectObservation MakeObservation(
		const FSightWeaveSubjectRegistration& Registration,
		const uint64 ObservationRevision,
		const bool bHardLive,
		const uint64 TransitionIdentity = 0,
		const uint64 SourceLiveRevision = 1)
	{
		FSightWeaveSubjectObservation Observation;
		Observation.Identity = Registration.Identity;
		Observation.Scope = Registration.Scope;
		Observation.ObservationRevision = ObservationRevision;
		Observation.EligibilityRevision = 11;
		Observation.SourceLiveRevision = SourceLiveRevision;
		Observation.TransitionIdentity = TransitionIdentity;
		Observation.bHardLive = bHardLive;
		Observation.bEligibleForMemoryWrite = true;
		Observation.BasicSnapshot = MakeCandidate(FVector(100.0, 200.0, 50.0));
		return Observation;
	}

	FSightWeaveSubjectPresentationContext MakePresentationContext(
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

	bool CaptureOnce(
		FAutomationTestBase& Test,
		FSightWeaveSubjectMemoryAuthority& Authority,
		const FSightWeaveSubjectHandle Handle,
		const FSightWeaveSubjectRegistration& Registration,
		const uint64 Transition = 100)
	{
		const FSightWeaveSubjectTransitionResult Live = Authority.SubmitObservation(
			Handle,
			MakeObservation(Registration, 1, true));
		const FSightWeaveSubjectTransitionResult Lost = Authority.SubmitObservation(
			Handle,
			MakeObservation(Registration, 2, false, Transition));
		return Test.TestTrue(TEXT("Live observation accepted"), Live.Succeeded())
			&& Test.TestEqual(
				TEXT("Falling edge captures exactly one snapshot"),
				Lost.Disposition,
				ESightWeaveSubjectTransitionDisposition::SnapshotCaptured);
	}
}

using namespace SightWeaveM4P1SubjectPolicyTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P1PolicyMatrixTest,
	"SightWeave.M4P1.Subject.Policy.Matrix",
	Flags)

bool FSightWeaveM4P1PolicyMatrixTest::RunTest(const FString& Parameters)
{
	const ESightWeaveSubjectMemoryPolicy Policies[] = {
		ESightWeaveSubjectMemoryPolicy::NeverRemember,
		ESightWeaveSubjectMemoryPolicy::VisibleOnly,
		ESightWeaveSubjectMemoryPolicy::StaticEnvironment,
		ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot,
		ESightWeaveSubjectMemoryPolicy::Custom
	};
	for (const ESightWeaveSubjectMemoryPolicy Policy : Policies)
	{
		FSightWeaveSubjectMemoryAuthority Authority;
		const FSightWeaveSubjectRegistration Registration = MakeRegistration(Policy);
		const FSightWeaveSubjectHandle Handle = Authority.Register(Registration);
		TestTrue(TEXT("Policy registers"), Handle.IsValid());
		const FSightWeaveSubjectPresentationResult Live = Authority.EvaluatePresentation(
			Handle,
			MakePresentationContext(Registration, nullptr, true));
		if (Policy == ESightWeaveSubjectMemoryPolicy::StaticEnvironment)
		{
			TestEqual(TEXT("StaticEnvironment delegates to M3.5"), Live.State,
				ESightWeaveSubjectPresentationState::StaticEnvironmentDelegated);
		}
		else
		{
			TestEqual(TEXT("Live subject uses live presentation"), Live.State,
				ESightWeaveSubjectPresentationState::Live);
		}
		Authority.SubmitObservation(Handle, MakeObservation(Registration, 1, true));
		const FSightWeaveSubjectTransitionResult Lost = Authority.SubmitObservation(
			Handle,
			MakeObservation(Registration, 2, false, 10));
		if (Policy == ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot)
		{
			TestEqual(TEXT("LastSeen captures"), Lost.Disposition,
				ESightWeaveSubjectTransitionDisposition::SnapshotCaptured);
			TestEqual(TEXT("LastSeen owns one snapshot"), Authority.GetSnapshotCount(), 1);
		}
		else
		{
			TestEqual(TEXT("Non-LastSeen policy owns no built-in snapshot"),
				Authority.GetSnapshotCount(), 0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P1FallingEdgeTest,
	"SightWeave.M4P1.Subject.Snapshot.FallingEdgeReacquireRevision",
	Flags)

bool FSightWeaveM4P1FallingEdgeTest::RunTest(const FString& Parameters)
{
	FSightWeaveSubjectMemoryAuthority Authority;
	const FSightWeaveSubjectRegistration Registration = MakeRegistration(
		ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot);
	const FSightWeaveSubjectHandle Handle = Authority.Register(Registration);
	TestTrue(TEXT("Registers"), Handle.IsValid());
	TestTrue(TEXT("First edge captured"), CaptureOnce(*this, Authority, Handle, Registration));
	const FSightWeaveLastSeenSnapshotDescriptor* First = Authority.FindSnapshot(Handle);
	if (!TestNotNull(TEXT("First descriptor exists"), First))
	{
		return true;
	}
	TestEqual(TEXT("First snapshot revision"), First->SnapshotRevision, uint64(1));
	TestEqual(TEXT("Source live revision copied"), First->SourceLiveRevision, uint64(1));
	TestEqual(TEXT("Transition identity copied"), First->CaptureTransitionIdentity, uint64(100));

	const FSightWeaveSubjectTransitionResult RemainNonLive = Authority.SubmitObservation(
		Handle,
		MakeObservation(Registration, 3, false, 101));
	TestEqual(TEXT("Remaining non-live does not capture"), RemainNonLive.Disposition,
		ESightWeaveSubjectTransitionDisposition::NonLiveAccepted);
	TestEqual(TEXT("No per-frame snapshot revision churn"),
		Authority.FindSnapshot(Handle)->SnapshotRevision, uint64(1));

	TestTrue(TEXT("Reacquire accepted"), Authority.SubmitObservation(
		Handle,
		MakeObservation(Registration, 4, true, 0, 2)).Succeeded());
	const FSightWeaveSubjectPresentationResult Reacquired = Authority.EvaluatePresentation(
		Handle,
		MakePresentationContext(Registration, Authority.FindSnapshot(Handle), true));
	TestEqual(TEXT("Reacquire immediately selects live"), Reacquired.State,
		ESightWeaveSubjectPresentationState::Live);
	const FSightWeaveSubjectTransitionResult SecondLost = Authority.SubmitObservation(
		Handle,
		MakeObservation(Registration, 5, false, 102, 2));
	TestEqual(TEXT("Second falling edge captures"), SecondLost.Disposition,
		ESightWeaveSubjectTransitionDisposition::SnapshotCaptured);
	TestEqual(TEXT("Second edge advances exactly once"),
		Authority.FindSnapshot(Handle)->SnapshotRevision, uint64(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P1ClearSuppressUnknownTest,
	"SightWeave.M4P1.Subject.Snapshot.ClearSuppressUnknown",
	Flags)

bool FSightWeaveM4P1ClearSuppressUnknownTest::RunTest(const FString& Parameters)
{
	FSightWeaveSubjectMemoryAuthority Authority;
	const FSightWeaveSubjectRegistration Registration = MakeRegistration(
		ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot);
	const FSightWeaveSubjectHandle Handle = Authority.Register(Registration);
	CaptureOnce(*this, Authority, Handle, Registration);
	const FSightWeaveLastSeenSnapshotDescriptor* Snapshot = Authority.FindSnapshot(Handle);
	FSightWeaveSubjectPresentationContext Context = MakePresentationContext(
		Registration,
		Snapshot,
		false);
	TestEqual(TEXT("Remembered valid snapshot selects proxy"),
		Authority.EvaluatePresentation(Handle, Context).State,
		ESightWeaveSubjectPresentationState::LastSeenProxy);
	Context.bSuppressMemoryPresentation = true;
	TestEqual(TEXT("Suppression hides proxy"),
		Authority.EvaluatePresentation(Handle, Context).State,
		ESightWeaveSubjectPresentationState::Hidden);
	TestEqual(TEXT("Suppression retains descriptor"), Authority.GetSnapshotCount(), 1);
	Context.bSuppressMemoryPresentation = false;
	Context.bHardMemoryAtSnapshot = false;
	TestEqual(TEXT("Unknown is strict black"),
		Authority.EvaluatePresentation(Handle, Context).State,
		ESightWeaveSubjectPresentationState::Hidden);

	FSightWeaveMemoryRegion Clear;
	Clear.Scope = Registration.Scope;
	Clear.HeightRange = { 0.0f, 300.0f };
	Clear.Shape = ESightWeaveMemoryRegionShape::Circle;
	Clear.Center = FVector2D(100.0, 200.0);
	Clear.Radius = 100.0f;
	TestEqual(TEXT("Clear removes intersecting snapshot"), Authority.ClearSnapshots(Clear), 1);
	TestEqual(TEXT("Clear leaves no descriptor"), Authority.GetSnapshotCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P1IsolationTest,
	"SightWeave.M4P1.Subject.Isolation.WorldScopeGenerationRevision",
	Flags)

bool FSightWeaveM4P1IsolationTest::RunTest(const FString& Parameters)
{
	FSightWeaveSubjectMemoryAuthority Authority;
	const FSightWeaveSubjectRegistration Registration = MakeRegistration(
		ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot);
	const FSightWeaveSubjectHandle Handle = Authority.Register(Registration);
	CaptureOnce(*this, Authority, Handle, Registration);
	const FSightWeaveLastSeenSnapshotDescriptor* Snapshot = Authority.FindSnapshot(Handle);
	FSightWeaveSubjectPresentationContext Context = MakePresentationContext(
		Registration,
		Snapshot,
		false);

	FSightWeaveMemoryScopeKey CollisionScope = Context.Scope;
	CollisionScope.CanonicalProfiles[0].CanonicalCapabilities = { FName(TEXT("Infrared")) };
	CollisionScope.CanonicalProfiles[0].StableHash =
		Context.Scope.CanonicalProfiles[0].StableHash;
	Context.Scope = CollisionScope;
	TestEqual(TEXT("Forced profile-hash collision fails exact scope"),
		Authority.EvaluatePresentation(Handle, Context).Failure,
		ESightWeaveSubjectPresentationFailure::ScopeMismatch);
	Context = MakePresentationContext(Registration, Snapshot, false);
	--Context.SnapshotRevision;
	TestEqual(TEXT("Stale snapshot revision fails closed"),
		Authority.EvaluatePresentation(Handle, Context).Failure,
		ESightWeaveSubjectPresentationFailure::SnapshotRevisionMismatch);
	Context = MakePresentationContext(Registration, Snapshot, false);
	Context.Identity.InstanceGeneration = 2;
	TestEqual(TEXT("Identity reuse with new generation rejects old snapshot"),
		Authority.EvaluatePresentation(Handle, Context).Failure,
		ESightWeaveSubjectPresentationFailure::IdentityMismatch);

	const FSightWeaveMemoryScopeKey NewWorldScope = MakeScope(702);
	const FSightWeaveSubjectRegistration NewWorldRegistration = MakeRegistration(
		ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot,
		1,
		&NewWorldScope);
	TestFalse(TEXT("Old-world snapshot cannot match new world"),
		FSightWeaveSubjectMemoryAuthority::DoesSnapshotMatchRegistration(
			*Snapshot,
			NewWorldRegistration));
	Authority.Reset();
	TestEqual(TEXT("PIE/world teardown clears subjects"), Authority.GetSubjectCount(), 0);
	TestEqual(TEXT("PIE/world teardown clears snapshots"), Authority.GetSnapshotCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P1UnsupportedFailClosedTest,
	"SightWeave.M4P1.Subject.Unsupported.FailClosed",
	Flags)

bool FSightWeaveM4P1UnsupportedFailClosedTest::RunTest(const FString& Parameters)
{
	FSightWeaveSubjectMemoryAuthority Authority;
	const FSightWeaveSubjectRegistration Registration = MakeRegistration(
		ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot);
	const FSightWeaveSubjectHandle Handle = Authority.Register(Registration);
	FSightWeaveSubjectObservation Live = MakeObservation(Registration, 1, true);
	Live.BasicSnapshot.bHasUnsupportedComponents = true;
	TestTrue(TEXT("Unsupported subject may still be accepted as live"),
		Authority.SubmitObservation(Handle, Live).Succeeded());
	const FSightWeaveSubjectTransitionResult Lost = Authority.SubmitObservation(
		Handle,
		MakeObservation(Registration, 2, false, 1));
	TestEqual(TEXT("Unsupported snapshot capture fails closed"), Lost.Failure,
		ESightWeaveSubjectTransitionFailure::UnsupportedSubject);
	TestEqual(TEXT("Unsupported subject creates no descriptor"), Authority.GetSnapshotCount(), 0);

	FSightWeaveSubjectMemoryAuthority CustomAuthority;
	const FSightWeaveSubjectRegistration Custom = MakeRegistration(
		ESightWeaveSubjectMemoryPolicy::Custom);
	const FSightWeaveSubjectHandle CustomHandle = CustomAuthority.Register(Custom);
	CustomAuthority.SubmitObservation(CustomHandle, MakeObservation(Custom, 1, true));
	const FSightWeaveSubjectTransitionResult CustomLost = CustomAuthority.SubmitObservation(
		CustomHandle,
		MakeObservation(Custom, 2, false, 1));
	TestEqual(TEXT("Custom without registered provider fails closed"), CustomLost.Failure,
		ESightWeaveSubjectTransitionFailure::MissingCustomProvider);
	TestEqual(TEXT("Missing provider creates no descriptor"), CustomAuthority.GetSnapshotCount(), 0);
	return true;
}

#endif
