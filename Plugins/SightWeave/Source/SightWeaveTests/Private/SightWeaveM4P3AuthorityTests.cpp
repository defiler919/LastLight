#include "Misc/AutomationTest.h"

#include "SightWeavePersistence.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace SightWeaveM4P3AuthorityTests
{
	constexpr EAutomationTestFlags Flags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FSightWeaveMemoryScopeKey MakeScope(const uint64 WorldSerial = 1001)
	{
		FSightWeaveMemoryScopeKey Scope;
		Scope.WorldIdentity = FSightWeaveRenderWorldIdentity { WorldSerial };
		Scope.WorldGeneration = WorldSerial;
		Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("Local")));
		Scope.FloorId = FSightWeaveFloorId(FName(TEXT("Ground")));
		Scope.FloorOrigin = FVector2D(-1000.0, -1000.0);
		Scope.FloorPlaneZ = 0.0f;
		Scope.PrecisionTier = ESightWeaveRenderPrecisionTier::Coarse;
		FSightWeaveRenderProfileIdentity& Profile = Scope.CanonicalProfiles.AddDefaulted_GetRef();
		Profile.CanonicalCapabilities = { FName(TEXT("Visible")) };
		Profile.StableHash = 0x1234;
		return Scope;
	}

	FSightWeaveMemoryPersistentState MakeMemoryState(
		const FSightWeaveMemoryScopeKey& Scope,
		const uint8 FirstByte = 1)
	{
		FSightWeaveMemoryPersistentState State;
		State.Scope = Scope;
		FSightWeavePackedMemoryTile& Tile = State.Tiles.AddDefaulted_GetRef();
		Tile.Key.Scope = Scope;
		Tile.Key.LogicalCoordinate = FIntPoint::ZeroValue;
		Tile.PackedBits.SetNumZeroed(SightWeave::Memory::PackedBytesPerTile);
		Tile.PackedBits[0] = FirstByte;
		return State;
	}

	FSightWeaveMemoryModifierDescription MakeModifier(
		const FSightWeaveMemoryScopeKey& Scope,
		const ESightWeaveMemoryModifierPersistence Persistence,
		const FName StableId = NAME_None)
	{
		FSightWeaveMemoryModifierDescription Modifier;
		Modifier.Operation = ESightWeaveMemoryModifierOperation::SuppressMemoryPresentation;
		Modifier.Region.Scope = Scope;
		Modifier.Region.HeightRange = { 0.0f, 300.0f };
		Modifier.Region.Shape = ESightWeaveMemoryRegionShape::Circle;
		Modifier.Region.Center = FVector2D::ZeroVector;
		Modifier.Region.Radius = 50.0f;
		Modifier.Persistence = Persistence;
		Modifier.StablePersistenceId = StableId;
		return Modifier;
	}

	FSightWeaveLastSeenSnapshotDescriptor MakeLastSeen(
		const FSightWeaveSubjectRegistration& Registration,
		const uint64 Revision = 1)
	{
		FSightWeaveLastSeenSnapshotDescriptor Snapshot;
		Snapshot.Identity = Registration.Identity;
		Snapshot.Scope = Registration.Scope;
		Snapshot.Policy = Registration.Policy;
		Snapshot.SnapshotRevision = Revision;
		Snapshot.EligibilityRevision = 10 + Revision;
		Snapshot.SourceLiveRevision = 20 + Revision;
		Snapshot.WorldTransform = FTransform(FVector(10.0 * Revision, 20.0, 50.0));
		Snapshot.WorldBounds = FBox(FVector(-10.0, -10.0, 0.0), FVector(10.0, 10.0, 100.0));
		Snapshot.StaticMeshAsset = FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"));
		Snapshot.MaterialOverrides = {
			FSoftObjectPath(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"))
		};
		Snapshot.VisualVariantId = FName(TEXT("Default"));
		Snapshot.CaptureReason = ESightWeaveSubjectCaptureReason::LiveToNonLive;
		Snapshot.CaptureTransitionIdentity = 100 + Revision;
		Snapshot.Validity = SightWeave::SubjectMemory::RequiredBasicSnapshotValidity;
		return Snapshot;
	}

	FSightWeaveSubjectPersistentStateRecord MakeSubject(
		const FSightWeaveMemoryScopeKey& Scope,
		const FName SubjectId,
		const ESightWeaveSubjectMemoryPolicy Policy,
		const FName ProviderId = NAME_None,
		const uint8 Revision = 1)
	{
		FSightWeaveSubjectPersistentStateRecord Record;
		Record.Registration.Identity.StableId = SubjectId;
		Record.Registration.Identity.InstanceGeneration = 1;
		Record.Registration.Scope = Scope;
		Record.Registration.Policy = Policy;
		if (Policy == ESightWeaveSubjectMemoryPolicy::Custom)
		{
			Record.Registration.CustomProviderName = ProviderId;
			Record.Registration.CustomProviderVersion = 1;
		}
		if (Policy == ESightWeaveSubjectMemoryPolicy::Custom
			|| Policy == ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot)
		{
			Record.Snapshot = MakeLastSeen(Record.Registration, Revision);
		}
		return Record;
	}

	class FPreparedByte final : public ISightWeavePersistencePreparedPayload
	{
	public:
		explicit FPreparedByte(const uint8 InValue) : Value(InValue) {}
		uint8 Value = 0;
	};

	class FProvider final : public ISightWeaveSubjectSnapshotProvider
	{
	public:
		FProvider(const FName InId, const FSightWeaveSubjectIdentity& InSubject, const uint8 InValue)
			: Id(InId), Subject(InSubject), CaptureValue(InValue)
		{
		}

		virtual FName GetSightWeaveProviderName() const override { return Id; }
		virtual uint32 GetSightWeaveProviderVersion() const override { return 1; }
		virtual bool BuildSightWeaveSnapshotCandidate(
			const FSightWeaveSubjectRegistration& Registration,
			const FSightWeaveSubjectObservation& FallingEdgeObservation,
			FSightWeaveBasicStaticMeshSnapshotCandidate& OutCandidate) const override
		{
			OutCandidate.WorldTransform = FTransform::Identity;
			OutCandidate.WorldBounds = FBox(FVector(-1.0), FVector(1.0));
			OutCandidate.StaticMeshAsset = FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"));
			OutCandidate.bOpaqueStaticMesh = true;
			return true;
		}
		virtual bool SupportsSightWeavePersistence() const override { return true; }
		virtual ESightWeavePersistenceProviderResult CaptureSightWeavePersistence(
			TArray<FSightWeaveProviderPayloadRecord>& OutPayloads) const override
		{
			if (bFailCapture)
			{
				return ESightWeavePersistenceProviderResult::CaptureFailed;
			}
			FSightWeaveProviderPayloadRecord& Payload = OutPayloads.AddDefaulted_GetRef();
			Payload.ProviderId = Id;
			Payload.SchemaVersion = 1;
			Payload.Domain.Type = ESightWeaveProviderDomainType::Subject;
			Payload.Domain.StableScopeId = FName(TEXT("scope.main"));
			Payload.Domain.SubjectIdentity = Subject;
			Payload.Payload = { CaptureValue };
			return ESightWeavePersistenceProviderResult::Succeeded;
		}
		virtual ESightWeavePersistenceProviderResult PrepareSightWeavePersistence(
			const FSightWeaveProviderPayloadRecord& Payload,
			TUniquePtr<ISightWeavePersistencePreparedPayload>& OutPrepared) const override
		{
			++PrepareCount;
			if (bFailPrepare || Payload.Payload.Num() != 1)
			{
				return ESightWeavePersistenceProviderResult::PrepareFailed;
			}
			OutPrepared = MakeUnique<FPreparedByte>(Payload.Payload[0]);
			return ESightWeavePersistenceProviderResult::Succeeded;
		}
		virtual void CommitSightWeavePersistence(
			TUniquePtr<ISightWeavePersistencePreparedPayload>&& Prepared) override
		{
			FPreparedByte* Byte = static_cast<FPreparedByte*>(Prepared.Get());
			CommittedValue = Byte->Value;
			++CommitCount;
		}

		FName Id;
		FSightWeaveSubjectIdentity Subject;
		uint8 CaptureValue = 0;
		mutable int32 PrepareCount = 0;
		int32 CommitCount = 0;
		uint8 CommittedValue = 0;
		bool bFailCapture = false;
		mutable bool bFailPrepare = false;
	};

	FSightWeavePersistenceScopeBinding MakeBinding(
		FSightWeaveMemoryAuthority& Memory,
		FSightWeaveSubjectMemoryAuthority* Subjects = nullptr,
		int32* PublicationCount = nullptr)
	{
		FSightWeavePersistenceScopeBinding Binding;
		Binding.StableScopeId = FName(TEXT("scope.main"));
		Binding.MemoryAuthority = &Memory;
		Binding.SubjectAuthority = Subjects;
		if (PublicationCount)
		{
			Binding.PublishDerivedState = [PublicationCount]() { ++*PublicationCount; };
		}
		return Binding;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P3PersistentModifierTest,
	"SightWeave.M4P3.Persistence.Authority.PersistentModifier",
	SightWeaveM4P3AuthorityTests::Flags)

bool FSightWeaveM4P3PersistentModifierTest::RunTest(const FString& Parameters)
{
	using namespace SightWeaveM4P3AuthorityTests;
	const FSightWeaveMemoryScopeKey SourceScope = MakeScope();
	FSightWeaveMemoryAuthority Source;
	TestTrue(TEXT("Source configures"), Source.Configure(SourceScope, 8));
	TestEqual(TEXT("Source state seeds"), Source.PreparePersistentReplacement(
		MakeMemoryState(SourceScope)), ESightWeaveMemoryFailure::None);
	TestTrue(TEXT("Transient modifier registers"), Source.RegisterModifier(MakeModifier(
		SourceScope, ESightWeaveMemoryModifierPersistence::Transient)).IsValid());
	TestTrue(TEXT("Persistent modifier registers"), Source.RegisterModifier(MakeModifier(
		SourceScope,
		ESightWeaveMemoryModifierPersistence::Persistent,
		FName(TEXT("Door.Lock")))).IsValid());

	FSightWeavePersistenceProviderRegistry EmptyProviders;
	FSightWeaveSnapshotBlob FirstBlob;
	FSightWeavePersistenceScopeBinding SourceBinding = MakeBinding(Source);
	const FSightWeaveSnapshotDiagnostic Captured = FSightWeavePersistence::Capture(
		MakeArrayView(&SourceBinding, 1), EmptyProviders, FirstBlob);
	TestTrue(TEXT("Authority capture succeeds"), Captured.Succeeded());
	FSightWeaveCanonicalSnapshot Parsed;
	TestTrue(TEXT("Captured blob parses"), FSightWeavePersistence::ParseBlob(
		FirstBlob, Parsed).Succeeded());
	TestEqual(TEXT("Only persistent modifier serialized"),
		Parsed.Scopes[0].PersistentModifiers.Num(), 1);

	TestTrue(TEXT("A later transient modifier registers"), Source.RegisterModifier(MakeModifier(
		SourceScope, ESightWeaveMemoryModifierPersistence::Transient)).IsValid());
	FSightWeaveSnapshotBlob SecondBlob;
	TestTrue(TEXT("Second capture succeeds"), FSightWeavePersistence::Capture(
		MakeArrayView(&SourceBinding, 1), EmptyProviders, SecondBlob).Succeeded());
	TestEqual(TEXT("Transient registration does not affect bytes"), FirstBlob.Bytes, SecondBlob.Bytes);

	const FSightWeaveMemoryScopeKey TargetScope = MakeScope(2002);
	FSightWeaveMemoryAuthority Target;
	TestTrue(TEXT("Target configures"), Target.Configure(TargetScope, 8));
	TestTrue(TEXT("Target transient remains registered"), Target.RegisterModifier(MakeModifier(
		TargetScope, ESightWeaveMemoryModifierPersistence::Transient)).IsValid());
	FSightWeavePersistenceScopeBinding TargetBinding = MakeBinding(Target);
	TestTrue(TEXT("Restore succeeds"), FSightWeavePersistence::Restore(
		FirstBlob, MakeArrayView(&TargetBinding, 1), EmptyProviders).Succeeded());
	FSightWeaveMemoryPersistentState Restored;
	TestEqual(TEXT("Restored state exports"), Target.ExportPersistentState(Restored),
		ESightWeaveMemoryFailure::None);
	TestEqual(TEXT("One persistent modifier restored"), Restored.PersistentModifiers.Num(), 1);
	TestEqual(TEXT("Transient plus persistent active"), Target.GetModifierCount(), 2);
	TestEqual(TEXT("Memory tile restored"), Restored.Tiles.Num(), 1);

	FSightWeaveMemoryAuthority Invalid;
	TestTrue(TEXT("Invalid fixture configures"), Invalid.Configure(MakeScope(3003), 8));
	TestTrue(TEXT("Persistent modifier with missing ID can exist until capture"),
		Invalid.RegisterModifier(MakeModifier(
			Invalid.GetScope(), ESightWeaveMemoryModifierPersistence::Persistent)).IsValid());
	FSightWeavePersistenceScopeBinding InvalidBinding = MakeBinding(Invalid);
	FSightWeaveSnapshotBlob InvalidBlob;
	TestEqual(TEXT("Missing stable ID fails capture"), FSightWeavePersistence::Capture(
		MakeArrayView(&InvalidBinding, 1), EmptyProviders, InvalidBlob).Result,
		ESightWeaveSnapshotResult::InvalidPersistentModifier);
	TestTrue(TEXT("Failed capture returns no partial blob"), InvalidBlob.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P3ProviderAtomicTest,
	"SightWeave.M4P3.Persistence.Authority.ProviderAtomic",
	SightWeaveM4P3AuthorityTests::Flags)

bool FSightWeaveM4P3ProviderAtomicTest::RunTest(const FString& Parameters)
{
	using namespace SightWeaveM4P3AuthorityTests;
	const FSightWeaveMemoryScopeKey Scope = MakeScope();
	FSightWeaveMemoryAuthority SourceMemory;
	SourceMemory.Configure(Scope, 8);
	SourceMemory.PreparePersistentReplacement(MakeMemoryState(Scope, 0x03));
	FSightWeaveSubjectMemoryAuthority SourceSubjects;
	FSightWeaveSubjectPersistentState SubjectState;
	SubjectState.Scope = Scope;
	SubjectState.Records.Add(MakeSubject(
		Scope, FName(TEXT("Custom.A")), ESightWeaveSubjectMemoryPolicy::Custom,
		FName(TEXT("Provider.A"))));
	TestTrue(TEXT("Source subjects seed"), SourceSubjects.PreparePersistentReplacement(SubjectState));
	FProvider Provider(FName(TEXT("Provider.A")),
		SubjectState.Records[0].Registration.Identity, 0x5a);
	FSightWeavePersistenceProviderRegistry CaptureRegistry;
	TestTrue(TEXT("Provider registers"), CaptureRegistry.Register(Provider));
	TestFalse(TEXT("Duplicate provider registration is rejected"), CaptureRegistry.Register(Provider));
	FSightWeavePersistenceScopeBinding SourceBinding = MakeBinding(SourceMemory, &SourceSubjects);
	FSightWeaveSnapshotBlob Blob;
	TestTrue(TEXT("Provider snapshot captures"), FSightWeavePersistence::Capture(
		MakeArrayView(&SourceBinding, 1), CaptureRegistry, Blob).Succeeded());

	FSightWeaveMemoryAuthority TargetMemory;
	TargetMemory.Configure(MakeScope(2002), 8);
	FSightWeaveSubjectMemoryAuthority TargetSubjects;
	int32 Publications = 0;
	FSightWeavePersistenceScopeBinding TargetBinding = MakeBinding(
		TargetMemory, &TargetSubjects, &Publications);
	const FSightWeaveSnapshotDiagnostic Restored = FSightWeavePersistence::Restore(
		Blob, MakeArrayView(&TargetBinding, 1), CaptureRegistry);
	TestEqual(TEXT("Provider restore succeeds without fallback"), Restored.Result,
		ESightWeaveSnapshotResult::Succeeded);
	TestEqual(TEXT("Provider prepared once"), Provider.PrepareCount, 1);
	TestEqual(TEXT("Provider committed once"), Provider.CommitCount, 1);
	TestEqual(TEXT("Provider committed canonical byte"), Provider.CommittedValue, uint8(0x5a));
	TestEqual(TEXT("Derived publication runs after commit"), Publications, 1);
	TestEqual(TEXT("Subject restored"), TargetSubjects.GetSnapshotCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P3MissingProviderTest,
	"SightWeave.M4P3.Persistence.Authority.MissingProviderLocalFailBlack",
	SightWeaveM4P3AuthorityTests::Flags)

bool FSightWeaveM4P3MissingProviderTest::RunTest(const FString& Parameters)
{
	using namespace SightWeaveM4P3AuthorityTests;
	const FSightWeaveMemoryScopeKey Scope = MakeScope();
	FSightWeaveMemoryAuthority SourceMemory;
	SourceMemory.Configure(Scope, 8);
	SourceMemory.PreparePersistentReplacement(MakeMemoryState(Scope, 0x0f));
	FSightWeaveSubjectPersistentState SubjectState;
	SubjectState.Scope = Scope;
	SubjectState.Records.Add(MakeSubject(
		Scope, FName(TEXT("Custom.A")), ESightWeaveSubjectMemoryPolicy::Custom,
		FName(TEXT("Provider.A")), 1));
	SubjectState.Records.Add(MakeSubject(
		Scope, FName(TEXT("Custom.B")), ESightWeaveSubjectMemoryPolicy::Custom,
		FName(TEXT("Provider.B")), 2));
	SubjectState.Records.Add(MakeSubject(
		Scope, FName(TEXT("Core.LastSeen")), ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot,
		NAME_None, 3));
	FSightWeaveSubjectMemoryAuthority SourceSubjects;
	SourceSubjects.PreparePersistentReplacement(SubjectState);
	FProvider ProviderA(FName(TEXT("Provider.A")),
		SubjectState.Records[0].Registration.Identity, 0xa1);
	FProvider ProviderB(FName(TEXT("Provider.B")),
		SubjectState.Records[1].Registration.Identity, 0xb2);
	FSightWeavePersistenceProviderRegistry FullRegistry;
	FullRegistry.Register(ProviderB);
	FullRegistry.Register(ProviderA);
	FSightWeavePersistenceScopeBinding SourceBinding = MakeBinding(SourceMemory, &SourceSubjects);
	FSightWeaveSnapshotBlob Blob;
	TestTrue(TEXT("Two-provider fixture captures"), FSightWeavePersistence::Capture(
		MakeArrayView(&SourceBinding, 1), FullRegistry, Blob).Succeeded());

	FSightWeaveMemoryAuthority TargetMemory;
	TargetMemory.Configure(MakeScope(2002), 8);
	FSightWeaveSubjectMemoryAuthority TargetSubjects;
	FSightWeavePersistenceProviderRegistry MissingARegistry;
	MissingARegistry.Register(ProviderB);
	FSightWeavePersistenceScopeBinding TargetBinding = MakeBinding(TargetMemory, &TargetSubjects);
	const FSightWeaveSnapshotDiagnostic Fallback = FSightWeavePersistence::Restore(
		Blob, MakeArrayView(&TargetBinding, 1), MissingARegistry);
	TestEqual(TEXT("Missing provider is explicit degraded success"), Fallback.Result,
		ESightWeaveSnapshotResult::SucceededWithProviderFallback);
	TestTrue(TEXT("Missing provider ID reported"),
		Fallback.MissingProviderIds.Contains(FName(TEXT("Provider.A"))));
	TestEqual(TEXT("Other provider commits"), ProviderB.CommitCount, 1);
	TestEqual(TEXT("Only missing provider subject is black"), TargetSubjects.GetSnapshotCount(), 2);
	const FSightWeaveSubjectHandle MissingHandle = TargetSubjects.FindHandleByIdentity(
		SubjectState.Records[0].Registration.Identity);
	TestTrue(TEXT("Missing-provider registration remains addressable"), MissingHandle.IsValid());
	TestNull(TEXT("Missing-provider Last-Seen cleared"), TargetSubjects.FindSnapshot(MissingHandle));
	TestEqual(TEXT("Core memory tile remains restored"), TargetMemory.GetAllocatedTileCount(), 1);

	FullRegistry.Unregister(FName(TEXT("Provider.B")));
	FullRegistry.Register(ProviderB);
	const FSightWeaveSnapshotDiagnostic Complete = FSightWeavePersistence::Restore(
		Blob, MakeArrayView(&TargetBinding, 1), FullRegistry);
	TestEqual(TEXT("Provider return restores complete domain"), Complete.Result,
		ESightWeaveSnapshotResult::Succeeded);
	TestEqual(TEXT("All three snapshots restored"), TargetSubjects.GetSnapshotCount(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P3RollbackTest,
	"SightWeave.M4P3.Persistence.Authority.ProviderFailureRollback",
	SightWeaveM4P3AuthorityTests::Flags)

bool FSightWeaveM4P3RollbackTest::RunTest(const FString& Parameters)
{
	using namespace SightWeaveM4P3AuthorityTests;
	const FSightWeaveMemoryScopeKey SourceScope = MakeScope();
	FSightWeaveMemoryAuthority SourceMemory;
	SourceMemory.Configure(SourceScope, 8);
	SourceMemory.PreparePersistentReplacement(MakeMemoryState(SourceScope, 0xff));
	FSightWeaveSubjectPersistentState SourceSubjectState;
	SourceSubjectState.Scope = SourceScope;
	SourceSubjectState.Records.Add(MakeSubject(
		SourceScope, FName(TEXT("Custom.A")), ESightWeaveSubjectMemoryPolicy::Custom,
		FName(TEXT("Provider.A"))));
	FSightWeaveSubjectMemoryAuthority SourceSubjects;
	SourceSubjects.PreparePersistentReplacement(SourceSubjectState);
	FProvider Provider(FName(TEXT("Provider.A")),
		SourceSubjectState.Records[0].Registration.Identity, 0x44);
	FSightWeavePersistenceProviderRegistry Registry;
	Registry.Register(Provider);
	FSightWeavePersistenceScopeBinding SourceBinding = MakeBinding(SourceMemory, &SourceSubjects);
	FSightWeaveSnapshotBlob Blob;
	TestTrue(TEXT("Rollback fixture captures"), FSightWeavePersistence::Capture(
		MakeArrayView(&SourceBinding, 1), Registry, Blob).Succeeded());

	const FSightWeaveMemoryScopeKey TargetScope = MakeScope(2002);
	FSightWeaveMemoryAuthority TargetMemory;
	TargetMemory.Configure(TargetScope, 8);
	TargetMemory.PreparePersistentReplacement(MakeMemoryState(TargetScope, 0x01));
	FSightWeaveSubjectMemoryAuthority TargetSubjects;
	const uint64 MemoryRevisionBefore = TargetMemory.GetMemoryRevision();
	const uint64 MemoryGuardBefore = TargetMemory.GetPersistenceGuardRevision();
	const uint64 SubjectGuardBefore = TargetSubjects.GetPersistenceGuardRevision();
	const int32 TilesBefore = TargetMemory.GetAllocatedTileCount();
	int32 Publications = 0;
	FSightWeavePersistenceScopeBinding TargetBinding = MakeBinding(
		TargetMemory, &TargetSubjects, &Publications);
	Provider.bFailPrepare = true;
	const FSightWeaveSnapshotDiagnostic Failed = FSightWeavePersistence::Restore(
		Blob, MakeArrayView(&TargetBinding, 1), Registry);
	TestEqual(TEXT("Provider prepare failure is structured"), Failed.Result,
		ESightWeaveSnapshotResult::ProviderPrepareFailed);
	TestEqual(TEXT("Memory revision unchanged"), TargetMemory.GetMemoryRevision(),
		MemoryRevisionBefore);
	TestEqual(TEXT("Memory guard unchanged"), TargetMemory.GetPersistenceGuardRevision(),
		MemoryGuardBefore);
	TestEqual(TEXT("Subject guard unchanged"), TargetSubjects.GetPersistenceGuardRevision(),
		SubjectGuardBefore);
	TestEqual(TEXT("Tile count unchanged"), TargetMemory.GetAllocatedTileCount(), TilesBefore);
	TestEqual(TEXT("Provider did not commit"), Provider.CommitCount, 0);
	TestEqual(TEXT("Derived publication did not run"), Publications, 0);
	return true;
}

#endif
