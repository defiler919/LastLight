#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "SightWeaveMemory.h"
#include "SightWeaveMemoryPresentationTestReadback.h"
#include "SightWeaveStaticEnvironment.h"

namespace SightWeaveM3P5CompositeTests
{
	constexpr double ReadbackTimeoutSeconds = 30.0;
	const FSightWeaveKnowledgeOwnerId Owner(FName(TEXT("PresentationOwner")));
	const FSightWeaveFloorId FloorId(FName(TEXT("PresentationFloor")));

	TArray<FVector2D> Rectangle2D(
		const double MinX,
		const double MinY,
		const double MaxX,
		const double MaxY)
	{
		return {
			FVector2D(MinX, MinY),
			FVector2D(MaxX, MinY),
			FVector2D(MaxX, MaxY),
			FVector2D(MinX, MaxY)
		};
	}

	TArray<FVector> Rectangle3D(
		const double MinX,
		const double MinY,
		const double MaxX,
		const double MaxY)
	{
		return {
			FVector(MinX, MinY, 100.0),
			FVector(MaxX, MinY, 100.0),
			FVector(MaxX, MaxY, 100.0),
			FVector(MinX, MaxY, 100.0)
		};
	}

	FSightWeaveRenderProfileIdentity Profile()
	{
		FSightWeaveIlluminationCompatibilityProfile Source;
		Source.AcceptedCapabilities.Add(FName(TEXT("Visible")));
		return FSightWeaveRenderProfileIdentity::FromProfile(Source);
	}

	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> BuildLivePacket(
		FAutomationTestBase* Test,
		const uint64 WorldSerial)
	{
		FSightWeaveSparseRenderPacketBuildInput Input;
		Input.WorldIdentity.Serial = WorldSerial;
		Input.PacketRevision = 1;
		Input.RegistryRevision = 2;
		Input.PublishedSnapshotRevision = 3;
		FSightWeaveSparseScopeBuildInput& Scope = Input.Scopes.AddDefaulted_GetRef();
		Scope.KnowledgeOwnerId = Owner;
		Scope.FloorId = FloorId;
		Scope.PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
		Scope.MaximumActiveTiles = SightWeave::SparseAtlas::StandardActiveTileCapacity;
		for (const ESightWeaveRenderMaskLayer Layer : {
			ESightWeaveRenderMaskLayer::Vision,
			ESightWeaveRenderMaskLayer::Illumination })
		{
			FSightWeaveSparsePolygonInput& Polygon = Scope.Polygons.AddDefaulted_GetRef();
			Polygon.StableSourceId = Layer == ESightWeaveRenderMaskLayer::Vision ? 1 : 2;
			Polygon.SourceRevision = 1;
			Polygon.Layer = Layer;
			Polygon.CompatibilityProfile = Profile();
			Polygon.WorldVertices = Rectangle2D(100.0, 100.0, 400.0, 400.0);
		}
		const FSightWeaveSparseRenderPacketBuildResult Built =
			FSightWeaveSparseRenderPacketBuilder::Build(Input);
		if (!Built.Succeeded())
		{
			Test->AddError(FString::Printf(
				TEXT("M3.5 live packet failed to build: %d"),
				static_cast<int32>(Built.Failure)));
		}
		return Built.Packet;
	}

	FSightWeaveFrameSnapshot BuildMemorySnapshot()
	{
		FSightWeaveFrameSnapshot Snapshot;
		Snapshot.Revision = FSightWeaveRevision(1);
		Snapshot.bPublished = true;
		FSightWeaveFloorDefinition& Floor = Snapshot.Floors.AddDefaulted_GetRef();
		Floor.FloorId = FloorId;
		Floor.BoundsMin = FVector2D::ZeroVector;
		Floor.BoundsMax = FVector2D(10000.0, 10000.0);
		Floor.HeightRange = { 0.0f, 300.0f };
		FSightWeaveVisionSnapshotEntry& Vision = Snapshot.VisionSources.AddDefaulted_GetRef();
		Vision.Handle = FSightWeaveVisionSourceHandle(1);
		Vision.Description.KnowledgeOwnerId = Owner;
		Vision.Description.FloorId = FloorId;
		Vision.Description.bActive = true;
		Vision.Description.IlluminationPolicy =
			ESightWeaveIlluminationPolicy::BypassLegalIllumination;
		Vision.Description.Compatibility.AcceptedCapabilities = { FName(TEXT("Visible")) };
		Vision.Description.Compatibility.Normalize();
		Vision.Polygon.SourceHandle = Vision.Handle;
		Vision.Polygon.KnowledgeOwnerId = Owner;
		Vision.Polygon.FloorId = FloorId;
		Vision.Polygon.Vertices = Rectangle3D(0.0, 0.0, 2200.0, 2200.0);
		Vision.Polygon.Revision = FSightWeaveRevision(1);
		Vision.Polygon.SourceRevision = FSightWeaveRevision(1);
		Vision.SourceRevision = FSightWeaveRevision(1);
		return Snapshot;
	}

	struct FFixture
	{
		TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> LivePacket;
		TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> MemoryPacket;
		TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> SuppressedMemoryPacket;
		TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> WrongWorldMemoryPacket;
		TSharedPtr<const FSightWeaveStaticEnvironmentPacket, ESPMode::ThreadSafe> StaticPacket;
		TSharedPtr<const FSightWeaveStaticEnvironmentPacket, ESPMode::ThreadSafe> WrongWorldStaticPacket;
		FSightWeaveViewPresentationSelection Selection;
	};

	bool BuildFixture(FAutomationTestBase* Test, FFixture& Out)
	{
		constexpr uint64 WorldSerial = 3550;
		Out.LivePacket = BuildLivePacket(Test, WorldSerial);
		const FSightWeaveFrameSnapshot Snapshot = BuildMemorySnapshot();
		FSightWeaveMemoryScopeKey Scope;
		if (!Out.LivePacket.IsValid()
			|| !FSightWeaveMemoryAuthority::BuildScopeForSnapshot(
				Snapshot,
				FSightWeaveRenderWorldIdentity { WorldSerial },
				WorldSerial,
				Owner,
				FloorId,
				ESightWeaveRenderPrecisionTier::Standard,
				Scope))
		{
			return false;
		}
		FSightWeaveMemoryAuthority Memory;
		if (!Memory.Configure(Scope, SightWeave::SparseAtlas::StandardActiveTileCapacity)
			|| !Memory.WriteEffectiveLive(Snapshot).Succeeded())
		{
			return false;
		}
		Out.MemoryPacket = Memory.PublishPacket(true);
		FSightWeaveMemoryModifierDescription Suppression;
		Suppression.Operation = ESightWeaveMemoryModifierOperation::SuppressMemoryPresentation;
		Suppression.Region.Scope = Scope;
		Suppression.Region.HeightRange = { 0.0f, 300.0f };
		Suppression.Region.Shape = ESightWeaveMemoryRegionShape::AxisAlignedBox;
		Suppression.Region.Center = FVector2D(1000.0, 1000.0);
		Suppression.Region.HalfExtents = FVector2D(200.0, 200.0);
		if (!Memory.RegisterModifier(Suppression).IsValid())
		{
			return false;
		}
		Out.SuppressedMemoryPacket = Memory.PublishPacket();
		FSightWeaveStaticEnvironmentAuthority StaticEnvironment;
		if (!StaticEnvironment.Configure(
				Scope,
				SightWeave::StaticEnvironment::DefaultMaximumTiles))
		{
			return false;
		}
		FSightWeaveStaticEnvironmentDescription Description;
		Description.KnowledgeOwnerId = Owner;
		Description.FloorId = FloorId;
		Description.HeightRange = { 0.0f, 300.0f };
		Description.WorldFootprint = Rectangle2D(0.0, 0.0, 1400.0, 1400.0);
		Description.NeutralIntensity = 112;
		Description.bExplicitlyImmutable = true;
		if (!StaticEnvironment.Register(Description).IsValid())
		{
			return false;
		}
		Out.StaticPacket = StaticEnvironment.PublishPacket();
		FSightWeaveMemoryScopeKey WrongWorldScope;
		if (!FSightWeaveMemoryAuthority::BuildScopeForSnapshot(
				Snapshot,
				FSightWeaveRenderWorldIdentity { WorldSerial + 1 },
				WorldSerial + 1,
				Owner,
				FloorId,
				ESightWeaveRenderPrecisionTier::Standard,
				WrongWorldScope))
		{
			return false;
		}
		FSightWeaveMemoryAuthority WrongWorldMemory;
		FSightWeaveStaticEnvironmentAuthority WrongWorldStatic;
		if (!WrongWorldMemory.Configure(
				WrongWorldScope,
				SightWeave::SparseAtlas::StandardActiveTileCapacity)
			|| !WrongWorldMemory.WriteEffectiveLive(Snapshot).Succeeded()
			|| !WrongWorldStatic.Configure(
				WrongWorldScope,
				SightWeave::StaticEnvironment::DefaultMaximumTiles)
			|| !WrongWorldStatic.Register(Description).IsValid())
		{
			return false;
		}
		Out.WrongWorldMemoryPacket = WrongWorldMemory.PublishPacket(true);
		Out.WrongWorldStaticPacket = WrongWorldStatic.PublishPacket();
		Out.Selection = FSightWeaveViewPresentationSelection::Enabled(
			Out.LivePacket->GetWorldIdentity(),
			Owner,
			FloorId,
			ESightWeaveRenderPrecisionTier::Standard,
			1);
		return Out.MemoryPacket.IsValid()
			&& Out.MemoryPacket->IsValid()
			&& Out.SuppressedMemoryPacket.IsValid()
			&& Out.SuppressedMemoryPacket->IsValid()
			&& Out.StaticPacket.IsValid()
			&& Out.StaticPacket->IsValid()
			&& Out.WrongWorldMemoryPacket.IsValid()
			&& Out.WrongWorldMemoryPacket->IsValid()
			&& Out.WrongWorldStaticPacket.IsValid()
			&& Out.WrongWorldStaticPacket->IsValid();
	}

	struct FContext
	{
		FString Name;
		double StartSeconds = FPlatformTime::Seconds();
		TSharedPtr<FSightWeaveMemoryPresentationTestReadback, ESPMode::ThreadSafe> Request;
		TArray<FColor> Expected;
	};

	class FWaitForReadbacks final : public IAutomationLatentCommand
	{
	public:
		FWaitForReadbacks(TArray<TSharedPtr<FContext>> InContexts, FAutomationTestBase* InTest)
			: Contexts(MoveTemp(InContexts)), Test(InTest)
		{
		}

		virtual bool Update() override
		{
			bool bAllFinished = true;
			for (const TSharedPtr<FContext>& Context : Contexts)
			{
				if (!Context.IsValid() || !Context->Request.IsValid())
				{
					Test->AddError(TEXT("M3.5 composite readback context is invalid"));
					return true;
				}
				Context->Request->Poll();
				bAllFinished &= Context->Request->IsFinished();
				if (!Context->Request->IsFinished()
					&& FPlatformTime::Seconds() - Context->StartSeconds > ReadbackTimeoutSeconds)
				{
					Test->AddError(Context->Name + TEXT(": GPU readback timed out"));
					return true;
				}
			}
			if (!bAllFinished)
			{
				return false;
			}
			for (const TSharedPtr<FContext>& Context : Contexts)
			{
				FSightWeaveMemoryPresentationReadbackResult Result;
				if (!Context->Request->TryTakeResult(Result))
				{
					Test->AddError(Context->Name + TEXT(": result unavailable"));
					continue;
				}
				Test->TestTrue(*FString::Printf(TEXT("%s completes"), *Context->Name), Result.bComplete);
				Test->TestTrue(*FString::Printf(TEXT("%s has no failure"), *Context->Name), Result.Failure.IsEmpty());
				Test->TestEqual(TEXT("Live mirror remains available"), Result.LiveAvailability,
					ESightWeaveRenderAvailability::Available);
				Test->TestEqual(TEXT("Memory mirror remains available"), Result.MemoryAvailability,
					ESightWeaveRenderAvailability::Available);
				Test->TestEqual(TEXT("Static attribute mirror remains available"),
					Result.StaticEnvironmentAvailability, ESightWeaveRenderAvailability::Available);
				Test->TestEqual(TEXT("Expected pixel count"), Result.Pixels.Num(), Context->Expected.Num());
				for (int32 Index = 0; Index < FMath::Min(Result.Pixels.Num(), Context->Expected.Num()); ++Index)
				{
					Test->TestEqual(
						*FString::Printf(TEXT("%s pixel %d"), *Context->Name, Index),
						Result.Pixels[Index],
						Context->Expected[Index]);
				}
			}
			return true;
		}

	private:
		TArray<TSharedPtr<FContext>> Contexts;
		FAutomationTestBase* Test = nullptr;
	};
}

namespace SightWeaveM3P5CompositeTests
{

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P5ThreeStateCompositeD3D12Test,
	"SightWeave.M3P5.Composite.ThreeStateAndMemoryFailure.D3D12",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::NonNullRHI
		| EAutomationTestFlags::EngineFilter)

bool FSightWeaveM3P5ThreeStateCompositeD3D12Test::RunTest(const FString& Parameters)
{
	FFixture Fixture;
	if (!BuildFixture(this, Fixture))
	{
		AddError(TEXT("M3.5 composite fixture failed to build"));
		return false;
	}
	const TArray<FVector2f> Positions = {
		FVector2f(200.0f, 200.0f),
		FVector2f(1000.0f, 1000.0f),
		FVector2f(1800.0f, 1800.0f),
		FVector2f(3000.0f, 3000.0f)
	};
	const TArray<FVector4f> Colors = {
		FVector4f(30.0f / 255.0f, 80.0f / 255.0f, 160.0f / 255.0f, 1.0f),
		FVector4f(0.9f, 0.1f, 0.2f, 1.0f),
		FVector4f(0.1f, 0.9f, 0.2f, 1.0f),
		FVector4f(0.2f, 0.1f, 0.9f, 1.0f)
	};
	TArray<TSharedPtr<FContext>> Contexts;
	TSharedPtr<FContext>& Normal = Contexts.Add_GetRef(MakeShared<FContext>());
	Normal->Name = TEXT("ThreeState");
	Normal->Expected = {
		FColor(30, 80, 160, 255),
		FColor(112, 112, 112, 255),
		FColor(0, 0, 0, 0),
		FColor(0, 0, 0, 0)
	};
	Normal->Request = FSightWeaveMemoryPresentationTestReadback::Start(
		Fixture.LivePacket,
		Fixture.Selection,
		Fixture.MemoryPacket,
		Fixture.StaticPacket,
		Positions,
		Colors);
	TSharedPtr<FContext>& FailedMemory = Contexts.Add_GetRef(MakeShared<FContext>());
	FailedMemory->Name = TEXT("ForcedMemoryUnavailable");
	FailedMemory->Expected = {
		FColor(30, 80, 160, 255),
		FColor(0, 0, 0, 0),
		FColor(0, 0, 0, 0),
		FColor(0, 0, 0, 0)
	};
	FailedMemory->Request = FSightWeaveMemoryPresentationTestReadback::Start(
		Fixture.LivePacket,
		Fixture.Selection,
		Fixture.MemoryPacket,
		Fixture.StaticPacket,
		Positions,
		Colors,
		true);
	TSharedPtr<FContext>& Suppressed = Contexts.Add_GetRef(MakeShared<FContext>());
	Suppressed->Name = TEXT("PresentationSuppressed");
	Suppressed->Expected = {
		FColor(30, 80, 160, 255),
		FColor(0, 0, 0, 0),
		FColor(0, 0, 0, 0),
		FColor(0, 0, 0, 0)
	};
	Suppressed->Request = FSightWeaveMemoryPresentationTestReadback::Start(
		Fixture.LivePacket,
		Fixture.Selection,
		Fixture.SuppressedMemoryPacket,
		Fixture.StaticPacket,
		Positions,
		Colors);
	TSharedPtr<FContext>& WrongWorld = Contexts.Add_GetRef(MakeShared<FContext>());
	WrongWorld->Name = TEXT("WrongMemoryWorldRejected");
	WrongWorld->Expected = {
		FColor(30, 80, 160, 255),
		FColor(0, 0, 0, 0),
		FColor(0, 0, 0, 0),
		FColor(0, 0, 0, 0)
	};
	WrongWorld->Request = FSightWeaveMemoryPresentationTestReadback::Start(
		Fixture.LivePacket,
		Fixture.Selection,
		Fixture.WrongWorldMemoryPacket,
		Fixture.WrongWorldStaticPacket,
		Positions,
		Colors);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForReadbacks(MoveTemp(Contexts), this));
	return true;
}

}

#endif
