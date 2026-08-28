#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "RenderingThread.h"
#include "SightWeaveMemoryTestReadback.h"
#include "SightWeavePersistence.h"
#include "SightWeaveRenderWorldSubsystem.h"
#include "SightWeaveWorldSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace SightWeaveM4P3DerivedRebuildTests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr double ReadbackTimeoutSeconds = 30.0;
	const FSightWeaveKnowledgeOwnerId Owner(FName(TEXT("Local")));
	const FSightWeaveFloorId FloorId(FName(TEXT("Ground")));
	const FName StableScopeId(TEXT("scope.world"));

	class FTestWorld final
	{
	public:
		explicit FTestWorld(const TCHAR* BaseName)
		{
			const FName WorldName = MakeUniqueObjectName(
				GetTransientPackage(), UWorld::StaticClass(), FName(BaseName));
			World = NewObject<UWorld>(GetTransientPackage(), WorldName, RF_Transient);
			if (!World || !GEngine)
			{
				return;
			}
			World->WorldType = EWorldType::PIE;
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

	private:
		UWorld* World = nullptr;
	};

	FSightWeaveFloorDefinition MakeFloor()
	{
		FSightWeaveFloorDefinition Floor;
		Floor.FloorId = FloorId;
		Floor.BoundsMin = FVector2D(-10000.0, -10000.0);
		Floor.BoundsMax = FVector2D(10000.0, 10000.0);
		Floor.HeightRange = { 0.0f, 300.0f };
		Floor.bEnabled = true;
		Floor.bActiveForQueries = true;
		return Floor;
	}

	bool ConfigureWorld(
		FTestWorld& World,
		USightWeaveWorldSubsystem*& OutRuntime,
		USightWeaveRenderWorldSubsystem*& OutRender,
		FSightWeaveMemoryScopeKey& OutScope)
	{
		if (!World.Get())
		{
			return false;
		}
		OutRuntime = World.Get()->GetSubsystem<USightWeaveWorldSubsystem>();
		OutRender = World.Get()->GetSubsystem<USightWeaveRenderWorldSubsystem>();
		return OutRuntime
			&& OutRender
			&& OutRuntime->RegisterFloor(MakeFloor(), World.Get())
			&& OutRuntime->ConfigureExplorationMemory(
				Owner,
				FloorId,
				ESightWeaveRenderPrecisionTier::Standard,
				8)
			&& OutRuntime->GetExplorationMemoryScope(OutScope);
	}

	FVector TileCenter(const FSightWeaveMemoryScopeKey& Scope)
	{
		const double HalfTile = 0.5
			* SightWeave::Memory::InteriorTileSize
			* SightWeaveCentimetersPerTexel(Scope.PrecisionTier);
		return FVector(
			Scope.FloorOrigin.X + HalfTile,
			Scope.FloorOrigin.Y + HalfTile,
			Scope.FloorPlaneZ + 50.0);
	}

	FSightWeaveMemoryPersistentState MakeState(
		const FSightWeaveMemoryScopeKey& Scope,
		const uint8 Fill)
	{
		FSightWeaveMemoryPersistentState State;
		State.Scope = Scope;
		FSightWeavePackedMemoryTile& Tile = State.Tiles.AddDefaulted_GetRef();
		Tile.Key.Scope = Scope;
		Tile.Key.LogicalCoordinate = FIntPoint::ZeroValue;
		Tile.PackedBits.Init(Fill, SightWeave::Memory::PackedBytesPerTile);
		FSightWeaveMemoryModifierDescription& Modifier =
			State.PersistentModifiers.AddDefaulted_GetRef();
		Modifier.Operation = ESightWeaveMemoryModifierOperation::SuppressMemoryPresentation;
		Modifier.Region.Scope = Scope;
		Modifier.Region.HeightRange = { 0.0f, 300.0f };
		Modifier.Region.Shape = ESightWeaveMemoryRegionShape::Circle;
		Modifier.Region.Center = FVector2D(TileCenter(Scope));
		Modifier.Region.Radius = 100.0f;
		Modifier.Persistence = ESightWeaveMemoryModifierPersistence::Persistent;
		Modifier.StablePersistenceId = FName(TEXT("Persistent.RestoreSuppression"));
		return State;
	}

	bool CaptureState(
		const FSightWeaveMemoryScopeKey& Scope,
		const uint8 Fill,
		FSightWeaveSnapshotBlob& OutBlob)
	{
		FSightWeaveMemoryAuthority Source;
		if (!Source.Configure(Scope, 8)
			|| Source.PreparePersistentReplacement(MakeState(Scope, Fill))
				!= ESightWeaveMemoryFailure::None)
		{
			return false;
		}
		FSightWeavePersistenceScopeBinding Binding;
		Binding.StableScopeId = StableScopeId;
		Binding.MemoryAuthority = &Source;
		FSightWeavePersistenceProviderRegistry Providers;
		return FSightWeavePersistence::Capture(
			MakeArrayView(&Binding, 1), Providers, OutBlob).Succeeded();
	}

	bool RestoreState(
		USightWeaveWorldSubsystem& Runtime,
		const FSightWeaveSnapshotBlob& Blob,
		FSightWeaveSnapshotDiagnostic* OutDiagnostic = nullptr)
	{
		FSightWeavePersistenceProviderRegistry Providers;
		const FSightWeaveSnapshotDiagnostic Diagnostic =
			Runtime.RestorePersistenceSnapshot(
				Blob, StableScopeId, nullptr, Providers);
		if (OutDiagnostic)
		{
			*OutDiagnostic = Diagnostic;
		}
		return Diagnostic.Succeeded();
	}

	FSightWeaveMemoryRegion WholeFirstTile(const FSightWeaveMemoryScopeKey& Scope)
	{
		FSightWeaveMemoryRegion Region;
		Region.Scope = Scope;
		Region.HeightRange = { 0.0f, 300.0f };
		Region.Shape = ESightWeaveMemoryRegionShape::Circle;
		Region.Center = FVector2D(TileCenter(Scope));
		Region.Radius = 2000.0f;
		return Region;
	}

	int32 ExpectedPresentedTexelCount(const FSightWeaveMemoryPacket& Packet)
	{
		int32 Count = 0;
		const FSightWeaveMemoryScopeKey& Scope = Packet.GetScope();
		const double CentimetersPerTexel = SightWeaveCentimetersPerTexel(Scope.PrecisionTier);
		for (int32 PhysicalY = 0; PhysicalY < SightWeave::SparseAtlas::PhysicalTileSize;
			++PhysicalY)
		{
			for (int32 PhysicalX = 0; PhysicalX < SightWeave::SparseAtlas::PhysicalTileSize;
				++PhysicalX)
			{
				const int32 InteriorX = PhysicalX - SightWeave::SparseAtlas::GutterTexels;
				const int32 InteriorY = PhysicalY - SightWeave::SparseAtlas::GutterTexels;
				if (InteriorX < 0 || InteriorY < 0
					|| InteriorX >= SightWeave::Memory::InteriorTileSize
					|| InteriorY >= SightWeave::Memory::InteriorTileSize)
				{
					continue;
				}
				const FVector WorldLocation(
					Scope.FloorOrigin.X + (InteriorX + 0.5) * CentimetersPerTexel,
					Scope.FloorOrigin.Y + (InteriorY + 0.5) * CentimetersPerTexel,
					Scope.FloorPlaneZ);
				const bool bSuppressed =
					Packet.GetPresentationSuppressions().ContainsByPredicate(
						[WorldLocation](const FSightWeaveMemoryModifierDescription& Modifier)
						{
							return Modifier.Region.ContainsWorldLocation(WorldLocation);
						});
				Count += !bSuppressed;
			}
		}
		return Count;
	}

	struct FReadbackContext
	{
		TSharedPtr<FSightWeaveMemoryTestReadback, ESPMode::ThreadSafe> Request;
		double StartSeconds = FPlatformTime::Seconds();
		int32 ExpectedWhiteTexels = 0;
	};

	class FWaitForRestoredMirror final : public IAutomationLatentCommand
	{
	public:
		FWaitForRestoredMirror(
			TSharedPtr<FReadbackContext> InContext,
			FAutomationTestBase* InTest)
			: Context(MoveTemp(InContext)), Test(InTest)
		{
		}

		virtual bool Update() override
		{
			Context->Request->Poll();
			if (!Context->Request->IsFinished())
			{
				if (FPlatformTime::Seconds() - Context->StartSeconds
					> ReadbackTimeoutSeconds)
				{
					Test->AddError(TEXT("M4P3 restored memory GPU readback timed out"));
					return true;
				}
				return false;
			}
			FSightWeaveMemoryReadbackResult Result;
			if (!Context->Request->TryTakeResult(Result))
			{
				Test->AddError(TEXT("M4P3 restored memory GPU readback result unavailable"));
				return true;
			}
			Test->TestEqual(
				TEXT("Restored D3D12 mirror completes"),
				Result.Status,
				ESightWeaveMemoryReadbackStatus::Complete);
			Test->TestEqual(
				TEXT("Restored D3D12 mirror is available"),
				Result.Availability,
				ESightWeaveRenderAvailability::Available);
			Test->TestEqual(TEXT("Restored mirror has one update"), Result.Updates.Num(), 1);
			Test->TestEqual(TEXT("Restored mirror remains binary"), Result.NonBinaryTexelCount, 0);
			Test->TestEqual(
				TEXT("Restored tile reconstructs with fail-black gutters and suppression"),
				Result.WhiteTexelCount,
				Context->ExpectedWhiteTexels);
			if (!Result.Updates.IsEmpty())
			{
				Test->TestTrue(
					TEXT("Restore creates GPU mirror work"),
					Result.Updates[0].bProducedMirrorWork);
				Test->TestTrue(
					TEXT("Restore uploads the resident tile"),
					Result.Updates[0].UploadDelta > 0);
				Test->TestEqual(
					TEXT("Restore creates exactly one resident GPU tile"),
					Result.Updates[0].ResidentTileCount,
					1);
			}
			return true;
		}

	private:
		TSharedPtr<FReadbackContext> Context;
		FAutomationTestBase* Test = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P3WorldIsolationRestoreTest,
	"SightWeave.M4P3.Persistence.Derived.WorldIsolationRestoreClearReacquire",
	SightWeaveM4P3DerivedRebuildTests::TestFlags)

bool FSightWeaveM4P3WorldIsolationRestoreTest::RunTest(const FString& Parameters)
{
	using namespace SightWeaveM4P3DerivedRebuildTests;
	uint64 FirstIdentity = 0;
	FSightWeaveSnapshotBlob Blob;
	{
		FTestWorld WorldA(TEXT("SightWeaveM4P3RestoreA"));
		FTestWorld WorldB(TEXT("SightWeaveM4P3RestoreB"));
		USightWeaveWorldSubsystem* RuntimeA = nullptr;
		USightWeaveWorldSubsystem* RuntimeB = nullptr;
		USightWeaveRenderWorldSubsystem* RenderA = nullptr;
		USightWeaveRenderWorldSubsystem* RenderB = nullptr;
		FSightWeaveMemoryScopeKey ScopeA;
		FSightWeaveMemoryScopeKey ScopeB;
		if (!TestTrue(TEXT("PIE world A configures"),
				ConfigureWorld(WorldA, RuntimeA, RenderA, ScopeA))
			|| !TestTrue(TEXT("PIE world B configures"),
				ConfigureWorld(WorldB, RuntimeB, RenderB, ScopeB)))
		{
			return false;
		}
		FirstIdentity = RenderA->GetWorldIdentity().Serial;
		TestNotEqual(TEXT("PIE worlds have isolated render identities"),
			RenderA->GetWorldIdentity().Serial, RenderB->GetWorldIdentity().Serial);
		TestNotEqual(TEXT("PIE worlds have isolated memory generations"),
			ScopeA.WorldGeneration, ScopeB.WorldGeneration);
		TestTrue(TEXT("World A source captures"), CaptureState(ScopeA, 0xff, Blob));
		TestTrue(TEXT("World A restore succeeds"), RestoreState(*RuntimeA, Blob));
		TestTrue(TEXT("World A CPU authority restores"),
			RuntimeA->QueryHardMemoryAtLocation(TileCenter(ScopeA)));
		TestFalse(TEXT("World B remains untouched"),
			RuntimeB->QueryHardMemoryAtLocation(TileCenter(ScopeB)));
		TestTrue(TEXT("Persistent suppression restores"),
			RuntimeA->IsMemoryPresentationSuppressedAtLocation(TileCenter(ScopeA)));
		const FSightWeaveImmutableMemoryPacketPtr RestoredPacket =
			RuntimeA->AcquirePublishedMemoryPacket();
		TestTrue(TEXT("Restore publishes a full memory rebuild"),
			RestoredPacket.IsValid() && RestoredPacket->IsFullRebuild());
		TestEqual(TEXT("Full rebuild carries one authority tile"),
			RestoredPacket.IsValid() ? RestoredPacket->GetAuthorityTiles().Num() : 0, 1);
		TestTrue(TEXT("Restored tile clears"),
			RuntimeA->ClearExplorationMemory(WholeFirstTile(ScopeA)));
		TestFalse(TEXT("Clear removes restored CPU memory"),
			RuntimeA->QueryHardMemoryAtLocation(TileCenter(ScopeA)));
		TestTrue(TEXT("Snapshot reacquires after clear"), RestoreState(*RuntimeA, Blob));
		TestTrue(TEXT("Reacquire restores CPU memory"),
			RuntimeA->QueryHardMemoryAtLocation(TileCenter(ScopeA)));
		FlushRenderingCommands();
	}
	FlushRenderingCommands();

	FTestWorld Restarted(TEXT("SightWeaveM4P3RestoreRestarted"));
	USightWeaveWorldSubsystem* Runtime = nullptr;
	USightWeaveRenderWorldSubsystem* Render = nullptr;
	FSightWeaveMemoryScopeKey Scope;
	if (!TestTrue(TEXT("Restarted PIE world configures"),
		ConfigureWorld(Restarted, Runtime, Render, Scope)))
	{
		return false;
	}
	TestNotEqual(TEXT("Restart gets a new render identity"),
		Render->GetWorldIdentity().Serial, FirstIdentity);
	TestFalse(TEXT("Restart does not inherit restored authority"),
		Runtime->QueryHardMemoryAtLocation(TileCenter(Scope)));
	TestTrue(TEXT("Restart can restore the same durable snapshot"),
		RestoreState(*Runtime, Blob));
	TestTrue(TEXT("Restart remaps durable data to its own scope"),
		Runtime->QueryHardMemoryAtLocation(TileCenter(Scope)));
	FlushRenderingCommands();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P3D3D12DerivedRebuildTest,
	"SightWeave.M4P3.Persistence.Derived.D3D12FullRebuildReadback",
	SightWeaveM4P3DerivedRebuildTests::TestFlags
		| EAutomationTestFlags::NonNullRHI)

bool FSightWeaveM4P3D3D12DerivedRebuildTest::RunTest(const FString& Parameters)
{
	using namespace SightWeaveM4P3DerivedRebuildTests;
	if (GUsingNullRHI)
	{
		AddInfo(TEXT("D3D12 M4P3 derived rebuild assertions skipped on NullRHI"));
		return true;
	}

	FSightWeaveImmutableMemoryPacketPtr RestoredPacket;
	FIntPoint SelectedLogicalTile = FIntPoint::ZeroValue;
	{
		FTestWorld World(TEXT("SightWeaveM4P3D3D12Restore"));
		USightWeaveWorldSubsystem* Runtime = nullptr;
		USightWeaveRenderWorldSubsystem* Render = nullptr;
		FSightWeaveMemoryScopeKey Scope;
		if (!TestTrue(TEXT("D3D12 restore world configures"),
			ConfigureWorld(World, Runtime, Render, Scope)))
		{
			return false;
		}
		FSightWeaveSnapshotBlob Blob;
		if (!TestTrue(TEXT("D3D12 restore fixture captures"),
			CaptureState(Scope, 0xff, Blob)))
		{
			return false;
		}
		FSightWeaveSnapshotDiagnostic Diagnostic;
		TestTrue(TEXT("D3D12 WorldSubsystem restore succeeds"),
			RestoreState(*Runtime, Blob, &Diagnostic));
		TestTrue(TEXT("Restored CPU authority is immediately correct"),
			Runtime->QueryHardMemoryAtLocation(TileCenter(Scope)));
		RestoredPacket = Runtime->AcquirePublishedMemoryPacket();
		TestTrue(TEXT("WorldSubsystem publishes a valid restore packet"),
			RestoredPacket.IsValid() && RestoredPacket->IsValid());
		TestTrue(TEXT("Restore packet explicitly requires full derived rebuild"),
			RestoredPacket.IsValid() && RestoredPacket->IsFullRebuild());
		TestEqual(TEXT("Restore packet contains canonical CPU authority"),
			RestoredPacket.IsValid() ? RestoredPacket->GetAuthorityTiles().Num() : 0, 1);
		TestTrue(TEXT("Render subsystem keeps the restored presentation scope"),
			Render->GetPresentationSelection().IsEnabled()
				&& Render->GetPresentationSelection().GetKnowledgeOwnerId() == Owner
				&& Render->GetPresentationSelection().GetFloorId() == FloorId);
		UE_LOG(LogTemp, Display,
			TEXT("SIGHTWEAVE_M4P3_DERIVED prepare_us=%.3f validate_us=%.3f commit_us=%.3f publish_us=%.3f packet_revision=%llu memory_revision=%llu full_rebuild=1 authority_tiles=1"),
			Diagnostic.PrepareMicroseconds,
			Diagnostic.ValidateMicroseconds,
			Diagnostic.CommitMicroseconds,
			Diagnostic.DerivedPublicationMicroseconds,
			RestoredPacket.IsValid() ? RestoredPacket->GetPacketRevision() : 0,
			RestoredPacket.IsValid() ? RestoredPacket->GetMemoryRevision() : 0);
		FlushRenderingCommands();
	}

	if (!RestoredPacket.IsValid())
	{
		return false;
	}
	FSightWeaveMemoryTileKey SelectedTile;
	SelectedTile.Scope = RestoredPacket->GetScope();
	SelectedTile.LogicalCoordinate = SelectedLogicalTile;
	TArray<FSightWeaveImmutableMemoryPacketPtr> Packets { RestoredPacket };
	const TSharedPtr<FReadbackContext> Context = MakeShared<FReadbackContext>();
	Context->ExpectedWhiteTexels = ExpectedPresentedTexelCount(*RestoredPacket);
	Context->Request = FSightWeaveMemoryTestReadback::StartSequence(
		MoveTemp(Packets), SelectedTile);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForRestoredMirror(Context, this));
	return true;
}

#endif
