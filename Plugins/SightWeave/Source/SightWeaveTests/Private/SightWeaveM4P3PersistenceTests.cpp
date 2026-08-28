#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SightWeavePersistence.h"

namespace SightWeaveM4P3PersistenceTests
{
	FSightWeaveRenderProfileIdentity MakeProfile(std::initializer_list<const TCHAR*> Capabilities)
	{
		FSightWeaveIlluminationCompatibilityProfile Profile;
		for (const TCHAR* Capability : Capabilities)
		{
			Profile.AcceptedCapabilities.Add(FName(Capability));
		}
		return FSightWeaveRenderProfileIdentity::FromProfile(Profile);
	}

	FSightWeavePackedMemoryTile MakeTile(const FIntPoint Coordinate, const uint8 Fill)
	{
		FSightWeavePackedMemoryTile Tile;
		Tile.Key.LogicalCoordinate = Coordinate;
		Tile.PackedBits.Init(Fill, SightWeave::Memory::PackedBytesPerTile);
		return Tile;
	}

	FSightWeavePersistentLastSeenRecord MakeLastSeen(const uint64 Revision)
	{
		FSightWeavePersistentLastSeenRecord Record;
		Record.SnapshotRevision = Revision;
		Record.EligibilityRevision = 7;
		Record.SourceLiveRevision = 9;
		Record.WorldTransform = FTransform(
			FQuat(FVector::UpVector, 0.25),
			FVector(125.0, -50.0, 30.0),
			FVector(1.0));
		Record.WorldBounds = FBox(FVector(100.0, -75.0, 0.0), FVector(150.0, -25.0, 60.0));
		Record.StaticMeshAsset = FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"));
		Record.MaterialOverrides.Add(
			FSoftObjectPath(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));
		Record.VisualVariantId = FName(TEXT("Closed"));
		Record.CaptureReason = ESightWeaveSubjectCaptureReason::LiveToNonLive;
		Record.CaptureTransitionIdentity = 11;
		Record.Validity = SightWeave::SubjectMemory::RequiredBasicSnapshotValidity;
		return Record;
	}

	FSightWeaveSnapshotScopeRecord MakeScope(const TCHAR* ScopeId, const int32 TileCount)
	{
		FSightWeaveSnapshotScopeRecord Record;
		Record.Scope.StableScopeId = FName(ScopeId);
		Record.Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("Player.One")));
		Record.Scope.FloorId = FSightWeaveFloorId(FName(TEXT("Floor.Basement")));
		Record.Scope.FloorOrigin = FVector2D(-6200.0, 1240.0);
		Record.Scope.FloorPlaneZ = -200.0f;
		Record.Scope.PrecisionTier = ESightWeaveRenderPrecisionTier::Coarse;
		Record.Scope.CanonicalProfiles.Add(MakeProfile({ TEXT("Visible"), TEXT("Infrared") }));
		for (int32 Index = 0; Index < TileCount; ++Index)
		{
			Record.MemoryTiles.Add(MakeTile(FIntPoint(Index - 2, Index % 3), 0x5a));
		}

		FSightWeavePersistentModifierRecord Modifier;
		Modifier.StableId = FName(TEXT("Blackout.Room.7"));
		Modifier.Operation = ESightWeaveMemoryModifierOperation::BlockMemoryWrites;
		Modifier.Region.Shape = ESightWeaveMemoryRegionShape::RotatedBox;
		Modifier.Region.Center = FVector2D(10.0, -20.0);
		Modifier.Region.HalfExtents = FVector2D(150.0, 90.0);
		Modifier.Region.RotationDegrees = 30.0f;
		Record.PersistentModifiers.Add(Modifier);

		FSightWeavePersistentSubjectRecord Subject;
		Subject.Identity.StableId = FName(TEXT("Door.Persistent.12"));
		Subject.Identity.InstanceGeneration = 3;
		Subject.Policy = ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot;
		Subject.LastSeen = MakeLastSeen(5);
		Record.Subjects.Add(MoveTemp(Subject));
		return Record;
	}

	FSightWeaveProviderPayloadRecord MakeProvider()
	{
		FSightWeaveProviderPayloadRecord Provider;
		Provider.ProviderId = FName(TEXT("Provider.Doors"));
		Provider.SchemaVersion = 4;
		Provider.Domain.Type = ESightWeaveProviderDomainType::Subject;
		Provider.Domain.StableScopeId = FName(TEXT("Scope.Main"));
		Provider.Domain.SubjectIdentity.StableId = FName(TEXT("Door.Persistent.12"));
		Provider.Domain.SubjectIdentity.InstanceGeneration = 3;
		Provider.Payload = { 1, 3, 3, 7 };
		return Provider;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P3PersistenceRawRoundTripTest,
	"SightWeave.M4P3.Persistence.Format.RawRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSightWeaveM4P3PersistenceRawRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace SightWeaveM4P3PersistenceTests;
	FSightWeaveCanonicalSnapshot Input;
	Input.Scopes.Add(MakeScope(TEXT("Scope.Main"), 0));
	Input.ProviderPayloads.Add(MakeProvider());
	FSightWeaveSnapshotBlob Blob;
	const FSightWeaveSnapshotDiagnostic Built = FSightWeavePersistence::BuildBlob(Input, Blob);
	TestTrue(TEXT("Raw V1 builds"), Built.Succeeded());
	TestEqual(TEXT("Small payload remains raw"), Built.CompressionMethod,
		ESightWeaveSnapshotCompressionMethod::None);
	TestEqual(TEXT("Header format version is V1"), Built.FormatVersion,
		SightWeave::Persistence::FormatVersion);
	FSightWeaveCanonicalSnapshot Parsed;
	const FSightWeaveSnapshotDiagnostic Restored = FSightWeavePersistence::ParseBlob(Blob, Parsed);
	TestTrue(TEXT("Raw V1 parses"), Restored.Succeeded());
	TestEqual(TEXT("One scope restored"), Parsed.Scopes.Num(), 1);
	TestEqual(TEXT("One provider payload restored"), Parsed.ProviderPayloads.Num(), 1);
	TestEqual(TEXT("One subject restored"), Parsed.Scopes[0].Subjects.Num(), 1);
	TestTrue(TEXT("Last-Seen restored"), Parsed.Scopes[0].Subjects[0].LastSeen.IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P3PersistenceCompressedRoundTripTest,
	"SightWeave.M4P3.Persistence.Format.CompressedRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSightWeaveM4P3PersistenceCompressedRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace SightWeaveM4P3PersistenceTests;
	FSightWeaveCanonicalSnapshot Input;
	Input.Scopes.Add(MakeScope(TEXT("Scope.Main"), 8));
	FSightWeaveSnapshotBlob Blob;
	const FSightWeaveSnapshotDiagnostic Built = FSightWeavePersistence::BuildBlob(Input, Blob);
	TestTrue(TEXT("Compressed V1 builds"), Built.Succeeded());
	TestEqual(TEXT("Large compressible payload uses Zlib"), Built.CompressionMethod,
		ESightWeaveSnapshotCompressionMethod::Zlib);
	TestTrue(TEXT("Stored blob is smaller than canonical payload plus header"),
		Built.StoredBytes < Built.CanonicalBytes + SightWeave::Persistence::HeaderBytes);
	FSightWeaveCanonicalSnapshot Parsed;
	const FSightWeaveSnapshotDiagnostic Restored = FSightWeavePersistence::ParseBlob(Blob, Parsed);
	TestTrue(TEXT("Compressed V1 parses"), Restored.Succeeded());
	TestEqual(TEXT("Eight tiles restored"), Parsed.Scopes[0].MemoryTiles.Num(), 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P3PersistenceDeterminismTest,
	"SightWeave.M4P3.Persistence.Format.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSightWeaveM4P3PersistenceDeterminismTest::RunTest(const FString& Parameters)
{
	using namespace SightWeaveM4P3PersistenceTests;
	FSightWeaveCanonicalSnapshot A;
	A.Scopes.Add(MakeScope(TEXT("Scope.Z"), 1));
	A.Scopes.Add(MakeScope(TEXT("Scope.A"), 2));
	A.ProviderPayloads.Add(MakeProvider());
	FSightWeaveCanonicalSnapshot B = A;
	Algo::Reverse(B.Scopes);
	for (FSightWeaveSnapshotScopeRecord& Scope : B.Scopes)
	{
		Algo::Reverse(Scope.MemoryTiles);
		Algo::Reverse(Scope.Subjects);
		Algo::Reverse(Scope.PersistentModifiers);
	}
	FSightWeaveSnapshotBlob BlobA;
	FSightWeaveSnapshotBlob BlobB;
	const auto ResultA = FSightWeavePersistence::BuildBlob(A, BlobA);
	const auto ResultB = FSightWeavePersistence::BuildBlob(B, BlobB);
	TestTrue(TEXT("A builds"), ResultA.Succeeded());
	TestTrue(TEXT("B builds"), ResultB.Succeeded());
	TestEqual(TEXT("Insertion order does not affect V1 bytes"), BlobA.Bytes, BlobB.Bytes);
	FSightWeaveSnapshotBlob BlobASecond;
	TestTrue(TEXT("Repeated A builds"), FSightWeavePersistence::BuildBlob(A, BlobASecond).Succeeded());
	TestEqual(TEXT("Repeated save is byte-identical"), BlobA.Bytes, BlobASecond.Bytes);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P3PersistenceEnvelopeFailureTest,
	"SightWeave.M4P3.Persistence.Format.EnvelopeFailures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSightWeaveM4P3PersistenceEnvelopeFailureTest::RunTest(const FString& Parameters)
{
	using namespace SightWeaveM4P3PersistenceTests;
	FSightWeaveCanonicalSnapshot Input;
	Input.Scopes.Add(MakeScope(TEXT("Scope.Main"), 2));
	FSightWeaveSnapshotBlob Valid;
	TestTrue(TEXT("Fixture builds"), FSightWeavePersistence::BuildBlob(Input, Valid).Succeeded());
	FSightWeaveCanonicalSnapshot Parsed;
	FSightWeaveSnapshotBlob Empty;
	TestEqual(TEXT("Empty rejected"), FSightWeavePersistence::ParseBlob(Empty, Parsed).Result,
		ESightWeaveSnapshotResult::EmptyBlob);
	FSightWeaveSnapshotBlob Truncated = Valid;
	Truncated.Bytes.SetNum(20);
	TestEqual(TEXT("Truncated header rejected"),
		FSightWeavePersistence::ParseBlob(Truncated, Parsed).Result,
		ESightWeaveSnapshotResult::Truncated);
	FSightWeaveSnapshotBlob Magic = Valid;
	Magic.Bytes[0] ^= 0xff;
	TestEqual(TEXT("Bad magic rejected"), FSightWeavePersistence::ParseBlob(Magic, Parsed).Result,
		ESightWeaveSnapshotResult::InvalidMagic);
	FSightWeaveSnapshotBlob Future = Valid;
	Future.Bytes[8] = 2;
	Future.Bytes[9] = 0;
	TestEqual(TEXT("Future version rejected"),
		FSightWeavePersistence::ParseBlob(Future, Parsed).Result,
		ESightWeaveSnapshotResult::FutureVersion);
	FSightWeaveSnapshotBlob Flags = Valid;
	Flags.Bytes[12] |= 0x80;
	TestEqual(TEXT("Unknown flags rejected"),
		FSightWeavePersistence::ParseBlob(Flags, Parsed).Result,
		ESightWeaveSnapshotResult::InvalidFlags);
	FSightWeaveSnapshotBlob Checksum = Valid;
	Checksum.Bytes[48] ^= 0x01;
	const ESightWeaveSnapshotResult CorruptResult =
		FSightWeavePersistence::ParseBlob(Checksum, Parsed).Result;
	TestEqual(TEXT("Mismatched checksum rejected before payload parsing"), CorruptResult,
		ESightWeaveSnapshotResult::ChecksumMismatch);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P3PersistenceDuplicateAndLimitTest,
	"SightWeave.M4P3.Persistence.Format.DuplicatesAndLimits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSightWeaveM4P3PersistenceDuplicateAndLimitTest::RunTest(const FString& Parameters)
{
	using namespace SightWeaveM4P3PersistenceTests;
	FSightWeaveCanonicalSnapshot DuplicateScope;
	DuplicateScope.Scopes.Add(MakeScope(TEXT("Scope.Main"), 0));
	DuplicateScope.Scopes.Add(MakeScope(TEXT("scope.main"), 0));
	FSightWeaveSnapshotBlob Blob;
	TestEqual(TEXT("Duplicate semantic scope ID rejected"),
		FSightWeavePersistence::BuildBlob(DuplicateScope, Blob).Result,
		ESightWeaveSnapshotResult::DuplicateScope);

	FSightWeaveCanonicalSnapshot DuplicateTile;
	DuplicateTile.Scopes.Add(MakeScope(TEXT("Scope.Main"), 1));
	const FSightWeavePackedMemoryTile DuplicateMemoryTile = DuplicateTile.Scopes[0].MemoryTiles[0];
	DuplicateTile.Scopes[0].MemoryTiles.Add(DuplicateMemoryTile);
	TestEqual(TEXT("Duplicate tile rejected"),
		FSightWeavePersistence::BuildBlob(DuplicateTile, Blob).Result,
		ESightWeaveSnapshotResult::DuplicateTile);

	FSightWeaveCanonicalSnapshot Valid;
	Valid.Scopes.Add(MakeScope(TEXT("Scope.Main"), 1));
	FSightWeaveSnapshotLimits Tiny;
	Tiny.MaximumCanonicalBytes = 512;
	Tiny.MaximumStoredBlobBytes = 1024;
	TestEqual(TEXT("Canonical allocation limit enforced"),
		FSightWeavePersistence::BuildBlob(Valid, Blob, Tiny).Result,
		ESightWeaveSnapshotResult::SizeLimitExceeded);
	return true;
}

#endif
