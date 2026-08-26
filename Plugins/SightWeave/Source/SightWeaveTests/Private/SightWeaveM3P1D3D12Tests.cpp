#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "SightWeaveRenderPacket.h"
#include "SightWeaveRenderTestReadback.h"

namespace SightWeave::M3P1::D3D12Tests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::NonNullRHI
		| EAutomationTestFlags::EngineFilter;
	constexpr double BoundaryDistanceCentimeters =
		SightWeave::RenderPacket::StandardCentimetersPerTexel * 0.7071067811865476 + 0.001;
	constexpr double ReadbackTimeoutSeconds = 20.0;

	struct FLayerPolygon
	{
		ESightWeaveRenderMaskLayer Layer = ESightWeaveRenderMaskLayer::Vision;
		int64 StableId = 0;
		TArray<FVector2D> Vertices;
	};

	struct FExpectedReadback
	{
		TSharedPtr<FSightWeaveRenderTestReadback, ESPMode::ThreadSafe> Request;
		FSightWeaveRenderReadbackExpectation Expectation;
		FBox2D PhysicalBounds = FBox2D(ForceInit);
		TArray<FLayerPolygon> Polygons;
		bool bExpectDiscardedStale = false;
		uint64 ExpectedDispatchCount = 1;
	};

	struct FCaseContext
	{
		FString Name;
		double StartSeconds = FPlatformTime::Seconds();
		TArray<FExpectedReadback> Readbacks;
		bool bHashesMustMatch = false;
	};

	TArray<FVector2D> Rectangle(const double MinX, const double MinY, const double MaxX, const double MaxY)
	{
		return {
			FVector2D(MinX, MinY),
			FVector2D(MaxX, MinY),
			FVector2D(MaxX, MaxY),
			FVector2D(MinX, MaxY)
		};
	}

	FLayerPolygon Polygon(
		const ESightWeaveRenderMaskLayer Layer,
		const int64 StableId,
		TArray<FVector2D> Vertices)
	{
		FLayerPolygon Result;
		Result.Layer = Layer;
		Result.StableId = StableId;
		Result.Vertices = MoveTemp(Vertices);
		return Result;
	}

	FSightWeaveRenderPacketBuildInput MakeInput(
		const uint64 Revision,
		const uint64 WorldSerial,
		const FBox2D& PhysicalBounds,
		const TArray<FLayerPolygon>& Polygons)
	{
		FSightWeaveRenderPacketBuildInput Input;
		Input.WorldIdentity.Serial = WorldSerial;
		Input.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("GpuOwner")));
		Input.FloorId = FSightWeaveFloorId(FName(TEXT("GpuFloor")));
		FSightWeaveIlluminationCompatibilityProfile Profile;
		Profile.AcceptedCapabilities.Add(FName(TEXT("Visible")));
		Input.CompatibilityProfile = FSightWeaveRenderProfileIdentity::FromProfile(Profile);
		Input.PacketRevision = Revision;
		Input.RegistryRevision = Revision + 100;
		Input.PublishedSnapshotRevision = Revision + 200;
		Input.PhysicalWorldBounds = PhysicalBounds;
		Input.DirtyReason = Polygons.IsEmpty()
			? ESightWeaveRenderDirtyReason::ExplicitClear
			: ESightWeaveRenderDirtyReason::SourceChanged;
		for (const FLayerPolygon& Source : Polygons)
		{
			FSightWeaveRenderPolygonInput& Destination = Input.Polygons.AddDefaulted_GetRef();
			Destination.StableSourceId = Source.StableId;
			Destination.Layer = Source.Layer;
			Destination.KnowledgeOwnerId = Input.KnowledgeOwnerId;
			Destination.FloorId = Input.FloorId;
			Destination.CompatibilityProfile = Input.CompatibilityProfile;
			Destination.WorldVertices = Source.Vertices;
		}
		return Input;
	}

	TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe> BuildPacket(
		FAutomationTestBase* Test,
		const uint64 Revision,
		const uint64 WorldSerial,
		const FBox2D& PhysicalBounds,
		const TArray<FLayerPolygon>& Polygons)
	{
		const FSightWeaveRenderPacketBuildResult Built = FSightWeaveRenderPacketBuilder::Build(
			MakeInput(Revision, WorldSerial, PhysicalBounds, Polygons));
		if (!Built.Succeeded())
		{
			Test->AddError(FString::Printf(
				TEXT("D3D12 test packet failed to build: %d"),
				static_cast<int32>(Built.Failure)));
			return nullptr;
		}
		return Built.Packet;
	}

	FSightWeaveRenderReadbackExpectation ExpectationFor(const FSightWeaveRenderPacket& Packet)
	{
		FSightWeaveRenderReadbackExpectation Result;
		Result.WorldIdentity = Packet.GetWorldIdentity();
		Result.KnowledgeOwnerId = Packet.GetKnowledgeOwnerId();
		Result.FloorId = Packet.GetFloorId();
		Result.CompatibilityProfile = Packet.GetCompatibilityProfile();
		Result.PacketRevision = Packet.GetPacketRevision();
		return Result;
	}

	void AddSingleReadback(
		FAutomationTestBase* Test,
		FCaseContext& Context,
		const uint64 Revision,
		const uint64 WorldSerial,
		const FBox2D& PhysicalBounds,
		TArray<FLayerPolygon> Polygons)
	{
		TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe> Packet =
			BuildPacket(Test, Revision, WorldSerial, PhysicalBounds, Polygons);
		if (!Packet.IsValid())
		{
			return;
		}
		FExpectedReadback& Expected = Context.Readbacks.AddDefaulted_GetRef();
		Expected.Request = FSightWeaveRenderTestReadback::Start(Packet);
		Expected.Expectation = ExpectationFor(*Packet);
		Expected.PhysicalBounds = PhysicalBounds;
		Expected.Polygons = MoveTemp(Polygons);
	}

	bool PointOnSegment(const FVector2D& Point, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const FVector2D AP = Point - A;
		const double Cross = AB.X * AP.Y - AB.Y * AP.X;
		if (FMath::Abs(Cross) > 0.0001)
		{
			return false;
		}
		const double Dot = FVector2D::DotProduct(AP, AB);
		return Dot >= -0.0001 && Dot <= AB.SquaredLength() + 0.0001;
	}

	bool ContainsInclusive(const TArray<FVector2D>& PolygonVertices, const FVector2D& Point)
	{
		bool bInside = false;
		for (int32 I = 0, J = PolygonVertices.Num() - 1; I < PolygonVertices.Num(); J = I++)
		{
			const FVector2D& A = PolygonVertices[J];
			const FVector2D& B = PolygonVertices[I];
			if (PointOnSegment(Point, A, B))
			{
				return true;
			}
			if ((A.Y > Point.Y) != (B.Y > Point.Y)
				&& Point.X < (B.X - A.X) * (Point.Y - A.Y) / (B.Y - A.Y) + A.X)
			{
				bInside = !bInside;
			}
		}
		return bInside;
	}

	double DistanceToSegment(const FVector2D& Point, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const double LengthSquared = AB.SquaredLength();
		if (LengthSquared <= UE_DOUBLE_SMALL_NUMBER)
		{
			return FVector2D::Distance(Point, A);
		}
		const double T = FMath::Clamp(FVector2D::DotProduct(Point - A, AB) / LengthSquared, 0.0, 1.0);
		return FVector2D::Distance(Point, A + T * AB);
	}

	bool AnyContains(
		const TArray<FLayerPolygon>& Polygons,
		const ESightWeaveRenderMaskLayer Layer,
		const FVector2D& Point)
	{
		for (const FLayerPolygon& PolygonData : Polygons)
		{
			if (PolygonData.Layer == Layer && ContainsInclusive(PolygonData.Vertices, Point))
			{
				return true;
			}
		}
		return false;
	}

	double DistanceToAnyBoundary(const TArray<FLayerPolygon>& Polygons, const FVector2D& Point)
	{
		double Minimum = TNumericLimits<double>::Max();
		for (const FLayerPolygon& PolygonData : Polygons)
		{
			for (int32 I = 0; I < PolygonData.Vertices.Num(); ++I)
			{
				Minimum = FMath::Min(
					Minimum,
					DistanceToSegment(
						Point,
						PolygonData.Vertices[I],
						PolygonData.Vertices[(I + 1) % PolygonData.Vertices.Num()]));
			}
		}
		return Minimum;
	}

	bool ValidatePixels(
		FAutomationTestBase* Test,
		const FString& CaseName,
		const FExpectedReadback& Expected,
		const FSightWeaveRenderReadbackResult& Result)
	{
		bool bSuccess = true;
		auto Check = [&](const bool bCondition, const FString& Message)
		{
			if (!bCondition)
			{
				Test->AddError(CaseName + TEXT(": ") + Message);
				bSuccess = false;
			}
		};
		Check(Result.Status == ESightWeaveRenderReadbackStatus::Complete, FString::Printf(
			TEXT("readback did not complete: %s"),
			*Result.Failure));
		Check(Result.Availability == ESightWeaveRenderAvailability::Available, FString::Printf(
			TEXT("render path was unavailable (availability=%d)"),
			static_cast<int32>(Result.Availability)));
		Check(Result.Width == 256 && Result.Height == 256, TEXT("unexpected physical tile dimensions"));
		Check(Result.RowPitchInPixels >= 256 && Result.BufferHeight >= 256, TEXT("invalid readback pitch/height"));
		Check(Result.Pixels.Num() == 256 * 256, TEXT("unexpected pixel count"));
		Check(Result.ZeroTexelCount + Result.WhiteTexelCount == 256 * 256, TEXT("binary counters do not cover tile"));
		Check(Result.NonBinaryTexelCount == 0, TEXT("PF_G8 mask contains non-binary texels"));
		Check(Result.bPF_G8Texture2D && Result.bPF_G8RenderTarget && Result.bPF_G8ShaderResource,
			TEXT("PF_G8 lacks required Texture2D/RTV/SRV capabilities"));
		Check(Result.RasterDispatchCount == Expected.ExpectedDispatchCount,
			TEXT("unexpected redraw dispatch count"));
		if (!bSuccess)
		{
			return false;
		}

		int32 NonBoundaryMismatchCount = 0;
		int32 BoundaryMismatchCount = 0;
		for (int32 Y = 0; Y < 256; ++Y)
		{
			for (int32 X = 0; X < 256; ++X)
			{
				const FVector2D WorldCenter(
					Expected.PhysicalBounds.Min.X + (static_cast<double>(X) + 0.5) * 10.0,
					Expected.PhysicalBounds.Min.Y + (static_cast<double>(Y) + 0.5) * 10.0);
				const bool bVision = AnyContains(Expected.Polygons, ESightWeaveRenderMaskLayer::Vision, WorldCenter);
				const bool bIllumination = AnyContains(Expected.Polygons, ESightWeaveRenderMaskLayer::Illumination, WorldCenter);
				const bool bBypass = AnyContains(Expected.Polygons, ESightWeaveRenderMaskLayer::Bypass, WorldCenter);
				const bool bSuppression = AnyContains(Expected.Polygons, ESightWeaveRenderMaskLayer::Suppression, WorldCenter);
				const uint8 ExpectedValue = ((bVision && bIllumination) || bBypass) && !bSuppression ? 255 : 0;
				const uint8 ActualValue = Result.Pixels[Y * 256 + X];
				if (ActualValue == ExpectedValue)
				{
					continue;
				}
				if (DistanceToAnyBoundary(Expected.Polygons, WorldCenter) <= BoundaryDistanceCentimeters)
				{
					++BoundaryMismatchCount;
				}
				else
				{
					if (NonBoundaryMismatchCount < 8)
					{
						Test->AddError(FString::Printf(
							TEXT("%s: non-boundary texel mismatch at (%d,%d), expected %d actual %d"),
							*CaseName,
							X,
							Y,
							ExpectedValue,
							ActualValue));
					}
					++NonBoundaryMismatchCount;
				}
			}
		}
		Check(NonBoundaryMismatchCount == 0, FString::Printf(
			TEXT("%d texels differ beyond the frozen half-texel-diagonal boundary class"),
			NonBoundaryMismatchCount));
		Test->AddInfo(FString::Printf(
			TEXT("%s readback: hash=%llu black=%d white=%d boundary-class=%d row-pitch=%d PF_G8_UAV=%s"),
			*CaseName,
			Result.MaskHash,
			Result.ZeroTexelCount,
			Result.WhiteTexelCount,
			BoundaryMismatchCount,
			Result.RowPitchInPixels,
			Result.bPF_G8UAV ? TEXT("true") : TEXT("false")));
		return bSuccess;
	}

	TSharedPtr<FCaseContext> BuildCase(FAutomationTestBase* Test, const FString& Name)
	{
		const FBox2D Bounds(FVector2D(0.0, 0.0), FVector2D(2560.0, 2560.0));
		const FBox2D GutterBounds(FVector2D(-40.0, -40.0), FVector2D(2520.0, 2520.0));
		TSharedPtr<FCaseContext> Context = MakeShared<FCaseContext>();
		Context->Name = Name;
		TArray<FLayerPolygon> Geometry;

		if (Name == TEXT("EmptyBlack"))
		{
			AddSingleReadback(Test, *Context, 1, 1001, Bounds, {});
		}
		else if (Name == TEXT("VisionOnlyBlack"))
		{
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Vision, 1, Rectangle(200, 200, 1200, 1200)));
			AddSingleReadback(Test, *Context, 1, 1002, Bounds, MoveTemp(Geometry));
		}
		else if (Name == TEXT("IlluminationOnlyBlack"))
		{
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Illumination, 1, Rectangle(200, 200, 1200, 1200)));
			AddSingleReadback(Test, *Context, 1, 1003, Bounds, MoveTemp(Geometry));
		}
		else if (Name == TEXT("VisionIlluminationIntersection"))
		{
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Vision, 1, Rectangle(200, 200, 1200, 1200)));
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Illumination, 2, Rectangle(600, 600, 1600, 1600)));
			AddSingleReadback(Test, *Context, 1, 1004, Bounds, MoveTemp(Geometry));
		}
		else if (Name == TEXT("BypassUnion"))
		{
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Bypass, 1, Rectangle(300, 300, 1000, 900)));
			AddSingleReadback(Test, *Context, 1, 1005, Bounds, MoveTemp(Geometry));
		}
		else if (Name == TEXT("SuppressionLast"))
		{
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Vision, 1, Rectangle(200, 200, 1600, 1600)));
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Illumination, 2, Rectangle(200, 200, 1600, 1600)));
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Suppression, 3, Rectangle(700, 700, 1100, 1100)));
			AddSingleReadback(Test, *Context, 1, 1006, Bounds, MoveTemp(Geometry));
		}
		else if (Name == TEXT("DisjointVisionLightBlack"))
		{
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Vision, 1, Rectangle(100, 100, 600, 600)));
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Illumination, 2, Rectangle(1000, 1000, 1500, 1500)));
			AddSingleReadback(Test, *Context, 1, 1007, Bounds, MoveTemp(Geometry));
		}
		else if (Name == TEXT("StraightWallBoundary"))
		{
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Bypass, 1, Rectangle(0, 0, 1280, 2560)));
			AddSingleReadback(Test, *Context, 1, 1008, Bounds, MoveTemp(Geometry));
		}
		else if (Name == TEXT("LCorner"))
		{
			TArray<FVector2D> L = {
				{200, 200}, {1400, 200}, {1400, 500}, {500, 500}, {500, 1500}, {200, 1500}
			};
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Vision, 1, L));
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Illumination, 2, L));
			AddSingleReadback(Test, *Context, 1, 1009, Bounds, MoveTemp(Geometry));
		}
		else if (Name == TEXT("TCorner"))
		{
			TArray<FVector2D> T = {
				{500, 200}, {800, 200}, {800, 600}, {1300, 600}, {1300, 900},
				{800, 900}, {800, 1500}, {500, 1500}, {500, 900}, {100, 900}, {100, 600}, {500, 600}
			};
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Bypass, 1, MoveTemp(T)));
			AddSingleReadback(Test, *Context, 1, 1010, Bounds, MoveTemp(Geometry));
		}
		else if (Name == TEXT("VertexEdgeInclusiveBoundary"))
		{
			TArray<FVector2D> Triangle = { {400, 400}, {1600, 400}, {400, 1600} };
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Vision, 1, Triangle));
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Illumination, 2, Triangle));
			AddSingleReadback(Test, *Context, 1, 1011, Bounds, MoveTemp(Geometry));
		}
		else if (Name == TEXT("TileEdgeAndGutter"))
		{
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Bypass, 1, Rectangle(-80, -20, 100, 900)));
			AddSingleReadback(Test, *Context, 1, 1012, GutterBounds, MoveTemp(Geometry));
		}
		else if (Name == TEXT("StalePacketRejected"))
		{
			TArray<FLayerPolygon> Revision2Geometry;
			Revision2Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Bypass, 1, Rectangle(200, 200, 900, 900)));
			const TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe> Revision2 =
				BuildPacket(Test, 2, 1013, Bounds, Revision2Geometry);
			const TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe> Revision1 =
				BuildPacket(Test, 1, 1013, Bounds, {});
			if (Revision2.IsValid() && Revision1.IsValid())
			{
				FExpectedReadback& Expected = Context->Readbacks.AddDefaulted_GetRef();
				Expected.Request = FSightWeaveRenderTestReadback::StartSequence({ Revision2, Revision1 }, 0);
				Expected.Expectation = ExpectationFor(*Revision2);
				Expected.PhysicalBounds = Bounds;
				Expected.Polygons = MoveTemp(Revision2Geometry);
			}
		}
		else if (Name == TEXT("DuplicateNoRedispatch"))
		{
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Bypass, 1, Rectangle(200, 200, 900, 900)));
			const TSharedPtr<const FSightWeaveRenderPacket, ESPMode::ThreadSafe> Packet =
				BuildPacket(Test, 1, 1014, Bounds, Geometry);
			if (Packet.IsValid())
			{
				FExpectedReadback& Expected = Context->Readbacks.AddDefaulted_GetRef();
				Expected.Request = FSightWeaveRenderTestReadback::StartSequence({ Packet, Packet }, 0);
				Expected.Expectation = ExpectationFor(*Packet);
				Expected.PhysicalBounds = Bounds;
				Expected.Polygons = MoveTemp(Geometry);
			}
		}
		else if (Name == TEXT("SourceDeleteClearBlack"))
		{
			AddSingleReadback(Test, *Context, 2, 1015, Bounds, {});
		}
		else if (Name == TEXT("WorldTeardownRestartIsolation"))
		{
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Bypass, 1, Rectangle(200, 200, 900, 900)));
			AddSingleReadback(Test, *Context, 1, 2016, Bounds, MoveTemp(Geometry));
			AddSingleReadback(Test, *Context, 1, 3016, Bounds, {});
		}
		else if (Name == TEXT("DamageRevealExcluded"))
		{
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Bypass, 1, Rectangle(200, 200, 900, 900)));
			AddSingleReadback(Test, *Context, 1, 1017, Bounds, Geometry);
			AddSingleReadback(Test, *Context, 1, 1017, Bounds, MoveTemp(Geometry));
			Context->bHashesMustMatch = true;
		}
		else if (Name == TEXT("FixedSeedRepeatHash"))
		{
			FRandomStream Random(0x51A7);
			for (int32 Index = 0; Index < 8; ++Index)
			{
				const double X = Random.RandRange(10, 160) * 10.0;
				const double Y = Random.RandRange(10, 160) * 10.0;
				const double Size = Random.RandRange(8, 24) * 10.0;
				Geometry.Add(Polygon(
					ESightWeaveRenderMaskLayer::Bypass,
					Index + 1,
					Rectangle(X, Y, X + Size, Y + Size)));
			}
			AddSingleReadback(Test, *Context, 1, 1018, Bounds, Geometry);
			AddSingleReadback(Test, *Context, 1, 1018, Bounds, MoveTemp(Geometry));
			Context->bHashesMustMatch = true;
		}
		else if (Name == TEXT("StaleReadbackDiscarded"))
		{
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Bypass, 1, Rectangle(200, 200, 900, 900)));
			AddSingleReadback(Test, *Context, 1, 1019, Bounds, MoveTemp(Geometry));
			if (!Context->Readbacks.IsEmpty())
			{
				Context->Readbacks[0].Expectation.PacketRevision = 2;
				Context->Readbacks[0].bExpectDiscardedStale = true;
			}
		}
		return Context;
	}

	class FWaitForReadback final : public IAutomationLatentCommand
	{
	public:
		FWaitForReadback(TSharedPtr<FCaseContext> InContext, FAutomationTestBase* InTest)
			: Context(MoveTemp(InContext))
			, Test(InTest)
		{
		}

		virtual bool Update() override
		{
			if (!Context.IsValid())
			{
				Test->AddError(TEXT("D3D12 context was not created"));
				return true;
			}
			if (Context->Readbacks.IsEmpty())
			{
				Test->AddError(Context->Name + TEXT(": no readback request was created"));
				return true;
			}
			bool bAllFinished = true;
			for (FExpectedReadback& Expected : Context->Readbacks)
			{
				Expected.Request->Poll();
				bAllFinished &= Expected.Request->IsFinished();
			}
			if (!bAllFinished)
			{
				if (FPlatformTime::Seconds() - Context->StartSeconds > ReadbackTimeoutSeconds)
				{
					Test->AddError(Context->Name + TEXT(": asynchronous GPU readback timed out after 20 seconds"));
					return true;
				}
				return false;
			}

			TArray<uint64> Hashes;
			for (FExpectedReadback& Expected : Context->Readbacks)
			{
				FSightWeaveRenderReadbackResult Result;
				if (!Expected.Request->TryTakeResult(Expected.Expectation, Result))
				{
					Test->AddError(Context->Name + TEXT(": completed readback result was unavailable"));
					continue;
				}
				if (Expected.bExpectDiscardedStale)
				{
					Test->TestEqual(
						*FString::Printf(TEXT("%s stale result is discarded"), *Context->Name),
						Result.Status,
						ESightWeaveRenderReadbackStatus::DiscardedStale);
					Test->TestEqual(
						*FString::Printf(TEXT("%s stale result releases pixels"), *Context->Name),
						Result.Pixels.Num(),
						0);
					continue;
				}
				ValidatePixels(Test, Context->Name, Expected, Result);
				Hashes.Add(Result.MaskHash);
			}
			if (Context->bHashesMustMatch && Hashes.Num() == 2)
			{
				Test->TestEqual(
					*FString::Printf(TEXT("%s repeated readback hash"), *Context->Name),
					Hashes[0],
					Hashes[1]);
			}
			return true;
		}

	private:
		TSharedPtr<FCaseContext> Context;
		FAutomationTestBase* Test = nullptr;
	};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FSightWeaveM3P1D3D12ReadbackTest,
	"SightWeave.M3P1.D3D12.Readback",
	SightWeave::M3P1::D3D12Tests::TestFlags)

void FSightWeaveM3P1D3D12ReadbackTest::GetTests(
	TArray<FString>& OutBeautifiedNames,
	TArray<FString>& OutTestCommands) const
{
	static const TCHAR* Cases[] = {
		TEXT("EmptyBlack"),
		TEXT("VisionOnlyBlack"),
		TEXT("IlluminationOnlyBlack"),
		TEXT("VisionIlluminationIntersection"),
		TEXT("BypassUnion"),
		TEXT("SuppressionLast"),
		TEXT("DisjointVisionLightBlack"),
		TEXT("StraightWallBoundary"),
		TEXT("LCorner"),
		TEXT("TCorner"),
		TEXT("VertexEdgeInclusiveBoundary"),
		TEXT("TileEdgeAndGutter"),
		TEXT("StalePacketRejected"),
		TEXT("DuplicateNoRedispatch"),
		TEXT("SourceDeleteClearBlack"),
		TEXT("WorldTeardownRestartIsolation"),
		TEXT("DamageRevealExcluded"),
		TEXT("FixedSeedRepeatHash"),
		TEXT("StaleReadbackDiscarded")
	};
	for (const TCHAR* Case : Cases)
	{
		OutBeautifiedNames.Add(Case);
		OutTestCommands.Add(Case);
	}
}

bool FSightWeaveM3P1D3D12ReadbackTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P1::D3D12Tests;
	TSharedPtr<FCaseContext> Context = BuildCase(this, Parameters);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForReadback(Context, this));
	return true;
}

#endif
