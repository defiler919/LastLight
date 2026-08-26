#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "SightWeaveSparseAtlas.h"
#include "SightWeaveSparseAtlasTestReadback.h"

namespace SightWeave::M3P2::D3D12Tests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::NonNullRHI
		| EAutomationTestFlags::EngineFilter;
	constexpr double ReadbackTimeoutSeconds = 30.0;
	constexpr double BoundaryDistanceCentimeters = 10.0 * 0.7071067811865476 + 0.001;

	struct FLayerPolygon
	{
		ESightWeaveRenderMaskLayer Layer = ESightWeaveRenderMaskLayer::Vision;
		int64 StableId = 0;
		FSightWeaveRenderProfileIdentity Profile;
		TArray<FVector2D> Vertices;
	};

	struct FExpectedReadback
	{
		TSharedPtr<FSightWeaveSparseAtlasTestReadback, ESPMode::ThreadSafe> Request;
		FSightWeaveSparseReadbackExpectation Expectation;
		FBox2D PhysicalBounds = FBox2D(ForceInit);
		TArray<FLayerPolygon> Polygons;
		TArray<int32> ExpectedDirtyCounts;
		TArray<uint64> ExpectedDispatchDeltas;
		uint64 ExpectedDuplicateCount = 0;
		uint64 ExpectedStaleCount = 0;
		int32 ExpectedFinalPages = INDEX_NONE;
		int32 ExpectedFinalResidents = INDEX_NONE;
		bool bExpectAllBlack = false;
		bool bExpectDiscardedStale = false;
	};

	struct FCaseContext
	{
		FString Name;
		double StartSeconds = FPlatformTime::Seconds();
		TArray<FExpectedReadback> Readbacks;
	};

	FSightWeaveRenderProfileIdentity Profile(std::initializer_list<const TCHAR*> Capabilities)
	{
		FSightWeaveIlluminationCompatibilityProfile Source;
		for (const TCHAR* Capability : Capabilities)
		{
			Source.AcceptedCapabilities.Add(FName(Capability));
		}
		return FSightWeaveRenderProfileIdentity::FromProfile(Source);
	}

	TArray<FVector2D> Rectangle(
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

	FLayerPolygon Polygon(
		const ESightWeaveRenderMaskLayer Layer,
		const int64 StableId,
		const FSightWeaveRenderProfileIdentity& CompatibilityProfile,
		TArray<FVector2D> Vertices)
	{
		FLayerPolygon Result;
		Result.Layer = Layer;
		Result.StableId = StableId;
		Result.Profile = CompatibilityProfile;
		Result.Vertices = MoveTemp(Vertices);
		return Result;
	}

	FSightWeaveSparseRenderPacketBuildInput MakeInput(
		const uint64 Revision,
		const uint64 WorldSerial,
		const TArray<FLayerPolygon>& Polygons,
		const int32 Capacity = SightWeave::SparseAtlas::StandardActiveTileCapacity,
		const TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe>& Previous = nullptr)
	{
		FSightWeaveSparseRenderPacketBuildInput Input;
		Input.WorldIdentity.Serial = WorldSerial;
		Input.PacketRevision = Revision;
		Input.RegistryRevision = Revision + 100;
		Input.PublishedSnapshotRevision = Revision + 200;
		Input.PreviousPacket = Previous;
		FSightWeaveSparseScopeBuildInput& Scope = Input.Scopes.AddDefaulted_GetRef();
		Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(FName(TEXT("GpuOwner")));
		Scope.FloorId = FSightWeaveFloorId(FName(TEXT("GpuFloor")));
		Scope.PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
		Scope.MaximumActiveTiles = Capacity;
		for (const FLayerPolygon& Source : Polygons)
		{
			FSightWeaveSparsePolygonInput& Destination = Scope.Polygons.AddDefaulted_GetRef();
			Destination.StableSourceId = Source.StableId;
			Destination.SourceRevision = Revision;
			Destination.Layer = Source.Layer;
			Destination.CompatibilityProfile = Source.Profile;
			Destination.WorldVertices = Source.Vertices;
		}
		return Input;
	}

	TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> BuildPacket(
		FAutomationTestBase* Test,
		const uint64 Revision,
		const uint64 WorldSerial,
		const TArray<FLayerPolygon>& Polygons,
		const int32 Capacity = SightWeave::SparseAtlas::StandardActiveTileCapacity,
		const TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe>& Previous = nullptr)
	{
		const FSightWeaveSparseRenderPacketBuildResult Built =
			FSightWeaveSparseRenderPacketBuilder::Build(
				MakeInput(Revision, WorldSerial, Polygons, Capacity, Previous));
		if (!Built.Succeeded())
		{
			Test->AddError(FString::Printf(
				TEXT("M3.2 GPU packet failed to build: %d"),
				static_cast<int32>(Built.Failure)));
			return nullptr;
		}
		return Built.Packet;
	}

	const FSightWeaveSparseRenderTile* FindTile(
		const FSightWeaveSparseRenderPacket& Packet,
		const FIntPoint Coordinate)
	{
		return Packet.GetTiles().FindByPredicate([Coordinate](const FSightWeaveSparseRenderTile& Tile)
		{
			return Tile.Identity.TileKey.LogicalCoordinate == Coordinate;
		});
	}

	void AddReadback(
		FAutomationTestBase* Test,
		FCaseContext& Context,
		TArray<TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe>> Packets,
		const FIntPoint SelectedCoordinate,
		const TArray<FLayerPolygon>& FinalPolygons,
		TArray<int32> ExpectedDirtyCounts = {},
		TArray<uint64> ExpectedDispatchDeltas = {})
	{
		if (Packets.IsEmpty() || !Packets.Last().IsValid())
		{
			Test->AddError(Context.Name + TEXT(": invalid sparse packet sequence"));
			return;
		}
		const FSightWeaveSparseRenderTile* Tile = FindTile(*Packets.Last(), SelectedCoordinate);
		if (!Tile)
		{
			Test->AddError(FString::Printf(
				TEXT("%s: selected tile (%d,%d) is absent"),
				*Context.Name,
				SelectedCoordinate.X,
				SelectedCoordinate.Y));
			return;
		}
		FExpectedReadback& Expected = Context.Readbacks.AddDefaulted_GetRef();
		Expected.Expectation.TileIdentity = Tile->Identity;
		Expected.Expectation.PacketRevision = Packets.Last()->GetPacketRevision();
		Expected.PhysicalBounds = Tile->PhysicalWorldBounds;
		Expected.Polygons = FinalPolygons;
		Expected.ExpectedDirtyCounts = MoveTemp(ExpectedDirtyCounts);
		Expected.ExpectedDispatchDeltas = MoveTemp(ExpectedDispatchDeltas);
		Expected.Request = FSightWeaveSparseAtlasTestReadback::StartSequence(
			MoveTemp(Packets),
			Tile->Identity);
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

	bool ContainsInclusive(const TArray<FVector2D>& Vertices, const FVector2D& Point)
	{
		bool bInside = false;
		for (int32 I = 0, J = Vertices.Num() - 1; I < Vertices.Num(); J = I++)
		{
			const FVector2D& A = Vertices[J];
			const FVector2D& B = Vertices[I];
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
		const double T = FMath::Clamp(
			FVector2D::DotProduct(Point - A, AB) / LengthSquared,
			0.0,
			1.0);
		return FVector2D::Distance(Point, A + T * AB);
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

	bool AnyContains(
		const TArray<FLayerPolygon>& Polygons,
		const ESightWeaveRenderMaskLayer Layer,
		const FVector2D& Point,
		const FSightWeaveRenderProfileIdentity* RequiredProfile = nullptr)
	{
		for (const FLayerPolygon& PolygonData : Polygons)
		{
			if (PolygonData.Layer == Layer
				&& (!RequiredProfile || PolygonData.Profile.IsEquivalentTo(*RequiredProfile))
				&& ContainsInclusive(PolygonData.Vertices, Point))
			{
				return true;
			}
		}
		return false;
	}

	uint8 ExpectedAt(const TArray<FLayerPolygon>& Polygons, const FVector2D& Point)
	{
		bool bProfileLive = false;
		TArray<FSightWeaveRenderProfileIdentity> Profiles;
		for (const FLayerPolygon& PolygonData : Polygons)
		{
			if (PolygonData.Layer == ESightWeaveRenderMaskLayer::Vision
				&& !Profiles.ContainsByPredicate([&PolygonData](const FSightWeaveRenderProfileIdentity& Existing)
				{
					return Existing.IsEquivalentTo(PolygonData.Profile);
				}))
			{
				Profiles.Add(PolygonData.Profile);
			}
		}
		for (const FSightWeaveRenderProfileIdentity& CompatibilityProfile : Profiles)
		{
			bProfileLive |= AnyContains(
				Polygons,
				ESightWeaveRenderMaskLayer::Vision,
				Point,
				&CompatibilityProfile)
				&& AnyContains(
					Polygons,
					ESightWeaveRenderMaskLayer::Illumination,
					Point,
					&CompatibilityProfile);
		}
		const bool bEffective = bProfileLive
			|| AnyContains(Polygons, ESightWeaveRenderMaskLayer::Bypass, Point);
		const bool bSuppressed = AnyContains(
			Polygons,
			ESightWeaveRenderMaskLayer::Suppression,
			Point);
		return bEffective && !bSuppressed ? 255 : 0;
	}

	bool ValidatePixels(
		FAutomationTestBase* Test,
		const FString& CaseName,
		const FExpectedReadback& Expected,
		const FSightWeaveSparseReadbackResult& Result)
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
		Check(Result.Status == ESightWeaveSparseReadbackStatus::Complete,
			FString::Printf(TEXT("readback failed: %s"), *Result.Failure));
		Check(Result.Availability == ESightWeaveRenderAvailability::Available,
			TEXT("sparse D3D12 render state was unavailable"));
		Check(Result.Width == 256 && Result.Height == 256, TEXT("physical tile is not 256 x 256"));
		Check(Result.RowPitchInPixels >= 256 && Result.BufferHeight >= 256,
			TEXT("invalid readback row pitch or height"));
		Check(Result.Pixels.Num() == 256 * 256, TEXT("unexpected readback pixel count"));
		Check(Result.NonBinaryTexelCount == 0, TEXT("hard mask contains a non-binary texel"));
		Check(Result.ZeroTexelCount + Result.WhiteTexelCount == 256 * 256,
			TEXT("binary counters do not cover the tile"));
		Check(Result.DuplicatePacketCount == Expected.ExpectedDuplicateCount,
			TEXT("unexpected duplicate packet count"));
		Check(Result.StalePacketCount == Expected.ExpectedStaleCount,
			TEXT("unexpected stale packet count"));
		Check(Result.RejectedPacketCount == 0, TEXT("sparse packet was unexpectedly rejected"));
		Check(Result.bGPUTimestampAvailable, TEXT("D3D12 absolute GPU timestamp was unavailable"));
		if (!Expected.ExpectedDirtyCounts.IsEmpty())
		{
			Check(Result.Updates.Num() == Expected.ExpectedDirtyCounts.Num(),
				TEXT("unexpected persistent update sample count"));
			for (int32 Index = 0;
				Index < Result.Updates.Num() && Index < Expected.ExpectedDirtyCounts.Num();
				++Index)
			{
				Check(Result.Updates[Index].RequestedDirtyTileCount == Expected.ExpectedDirtyCounts[Index],
					FString::Printf(TEXT("update %d dirty request count mismatch"), Index));
			}
		}
		if (!Expected.ExpectedDispatchDeltas.IsEmpty())
		{
			for (int32 Index = 0;
				Index < Result.Updates.Num() && Index < Expected.ExpectedDispatchDeltas.Num();
				++Index)
			{
				Check(Result.Updates[Index].DirtyTileDispatchDelta == Expected.ExpectedDispatchDeltas[Index],
					FString::Printf(TEXT("update %d dispatch delta mismatch"), Index));
			}
		}
		if (Expected.ExpectedFinalPages != INDEX_NONE && !Result.Updates.IsEmpty())
		{
			Check(Result.Updates.Last().AllocatedPageCount == Expected.ExpectedFinalPages,
				TEXT("unexpected persistent page count"));
		}
		if (Expected.ExpectedFinalResidents != INDEX_NONE && !Result.Updates.IsEmpty())
		{
			Check(Result.Updates.Last().ResidentTileCount == Expected.ExpectedFinalResidents,
				TEXT("unexpected final resident tile count"));
		}
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
				const uint8 ExpectedValue = ExpectedAt(Expected.Polygons, WorldCenter);
				const uint8 ActualValue = Result.Pixels[Y * 256 + X];
				if (ExpectedValue == ActualValue)
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
							TEXT("%s: non-boundary mismatch at (%d,%d), expected %d actual %d"),
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
			TEXT("%d texels differ outside the frozen boundary class"),
			NonBoundaryMismatchCount));
		if (Expected.bExpectAllBlack)
		{
			Check(Result.WhiteTexelCount == 0, TEXT("slot reuse retained stale white texels"));
		}
		Test->AddInfo(FString::Printf(
			TEXT("M3P2_GPU_PARITY case=%s tile=(%d,%d) page=%d slot=%d black=%d white=%d non_binary=%d boundary_class=%d gpu_us=%.3f readback_us=%.3f"),
			*CaseName,
			Expected.Expectation.TileIdentity.TileKey.LogicalCoordinate.X,
			Expected.Expectation.TileIdentity.TileKey.LogicalCoordinate.Y,
			Result.PhysicalAddress.PageIndex,
			Result.PhysicalAddress.SlotIndex,
			Result.ZeroTexelCount,
			Result.WhiteTexelCount,
			Result.NonBinaryTexelCount,
			BoundaryMismatchCount,
			Result.GPUWorkMicroseconds,
			Result.ReadbackEndToEndMicroseconds));
		for (int32 Index = 0; Index < Result.Updates.Num(); ++Index)
		{
			const FSightWeaveSparseUpdateSample& Sample = Result.Updates[Index];
			Test->AddInfo(FString::Printf(
				TEXT("M3P2_UPDATE case=%s sample=%d revision=%llu dirty=%d work=%s dispatch=%llu pages=%d residents=%d resource_generation=%llu page_allocations=%llu scratch_allocations=%llu gt_submit_us=%.3f rt_consume_us=%.3f rt_dirty_us=%.3f rt_rdg_us=%.3f clear_us=%.3f raster_us=%.3f publication_us=%.3f"),
				*CaseName,
				Index,
				Sample.PacketRevision,
				Sample.RequestedDirtyTileCount,
				Sample.bProducedMaskWork ? TEXT("true") : TEXT("false"),
				Sample.DirtyTileDispatchDelta,
				Sample.AllocatedPageCount,
				Sample.ResidentTileCount,
				Sample.ResourceGeneration,
				Sample.PageAllocationCount,
				Sample.ScratchAllocationCount,
				Sample.GameThreadSubmitMicroseconds,
				Sample.RenderThreadPacketConsumeMicroseconds,
				Sample.RenderThreadDirtySchedulingMicroseconds,
				Sample.RenderThreadRDGSetupMicroseconds,
				Sample.TileClearSetupMicroseconds,
				Sample.RasterSetupMicroseconds,
				Sample.PublicationMicroseconds));
		}
		return bSuccess;
	}

	TSharedPtr<FCaseContext> BuildCase(FAutomationTestBase* Test, const FString& Name)
	{
		TSharedPtr<FCaseContext> Context = MakeShared<FCaseContext>();
		Context->Name = Name;
		const FSightWeaveRenderProfileIdentity Visible = Profile({ TEXT("Visible") });
		const FSightWeaveRenderProfileIdentity Infrared = Profile({ TEXT("Infrared") });
		const FSightWeaveRenderProfileIdentity Common = Profile({});
		TArray<FLayerPolygon> Geometry;

		if (Name == TEXT("HorizontalSeam"))
		{
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Bypass, 1, Common,
				Rectangle(2300.0, 300.0, 2700.0, 900.0)));
			const auto Packet = BuildPacket(Test, 1, 4101, Geometry);
			AddReadback(Test, *Context, { Packet }, FIntPoint(0, 0), Geometry);
			AddReadback(Test, *Context, { Packet }, FIntPoint(1, 0), Geometry);
		}
		else if (Name == TEXT("VerticalSeam"))
		{
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Bypass, 1, Common,
				Rectangle(300.0, 2300.0, 900.0, 2700.0)));
			const auto Packet = BuildPacket(Test, 1, 4102, Geometry);
			AddReadback(Test, *Context, { Packet }, FIntPoint(0, 0), Geometry);
			AddReadback(Test, *Context, { Packet }, FIntPoint(0, 1), Geometry);
		}
		else if (Name == TEXT("DiagonalSeam"))
		{
			Geometry.Add(Polygon(
				ESightWeaveRenderMaskLayer::Bypass,
				1,
				Common,
				{ FVector2D(2180.0, 2180.0), FVector2D(2780.0, 2380.0), FVector2D(2380.0, 2780.0) }));
			const auto Packet = BuildPacket(Test, 1, 4103, Geometry);
			for (const FIntPoint Coordinate : { FIntPoint(0, 0), FIntPoint(0, 1), FIntPoint(1, 0), FIntPoint(1, 1) })
			{
				AddReadback(Test, *Context, { Packet }, Coordinate, Geometry);
			}
		}
		else if (Name == TEXT("FourTileCorner"))
		{
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Vision, 1, Visible,
				Rectangle(2200.0, 2200.0, 2800.0, 2800.0)));
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Illumination, 2, Visible,
				Rectangle(2350.0, 2100.0, 2650.0, 2900.0)));
			const auto Packet = BuildPacket(Test, 1, 4104, Geometry);
			for (const FIntPoint Coordinate : { FIntPoint(0, 0), FIntPoint(0, 1), FIntPoint(1, 0), FIntPoint(1, 1) })
			{
				AddReadback(Test, *Context, { Packet }, Coordinate, Geometry);
			}
		}
		else if (Name == TEXT("ProfilesBypassSuppression"))
		{
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Vision, 1, Visible,
				Rectangle(100.0, 100.0, 1100.0, 1100.0)));
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Illumination, 2, Visible,
				Rectangle(500.0, 500.0, 1500.0, 1500.0)));
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Vision, 3, Infrared,
				Rectangle(1200.0, 200.0, 2200.0, 1000.0)));
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Illumination, 4, Infrared,
				Rectangle(1500.0, 400.0, 2300.0, 1300.0)));
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Bypass, 5, Common,
				Rectangle(200.0, 1600.0, 800.0, 2200.0)));
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Suppression, 6, Common,
				Rectangle(600.0, 600.0, 900.0, 1900.0)));
			const auto Packet = BuildPacket(Test, 1, 4105, Geometry);
			AddReadback(Test, *Context, { Packet }, FIntPoint(0, 0), Geometry);
		}
		else if (Name == TEXT("NegativeCoordinates"))
		{
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Bypass, 1, Common,
				Rectangle(-2650.0, -900.0, -2250.0, -200.0)));
			const auto Packet = BuildPacket(Test, 1, 4106, Geometry);
			AddReadback(Test, *Context, { Packet }, FIntPoint(-2, -1), Geometry);
			AddReadback(Test, *Context, { Packet }, FIntPoint(-1, -1), Geometry);
		}
		else if (Name == TEXT("PersistentNoChangeOneDirty"))
		{
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Bypass, 1, Common,
				Rectangle(200.0, 200.0, 900.0, 900.0)));
			const auto Revision1 = BuildPacket(Test, 1, 4107, Geometry);
			const auto Revision2 = BuildPacket(Test, 2, 4107, Geometry, 128, Revision1);
			TArray<FLayerPolygon> Edited = Geometry;
			Edited[0].Vertices = Rectangle(250.0, 250.0, 950.0, 950.0);
			const auto Revision3 = BuildPacket(Test, 3, 4107, Edited, 128, Revision2);
			AddReadback(
				Test,
				*Context,
				{ Revision1, Revision2, Revision3 },
				FIntPoint(0, 0),
				Edited,
				{ 1, 0, 1 },
				{ 1, 0, 1 });
			if (!Context->Readbacks.IsEmpty())
			{
				Context->Readbacks[0].ExpectedFinalPages = 1;
				Context->Readbacks[0].ExpectedFinalResidents = 1;
			}
		}
		else if (Name == TEXT("SlotReuseClearsBlack"))
		{
			TArray<FLayerPolygon> WhiteGeometry;
			WhiteGeometry.Add(Polygon(ESightWeaveRenderMaskLayer::Bypass, 1, Common,
				Rectangle(100.0, 100.0, 800.0, 800.0)));
			const auto Revision1 = BuildPacket(Test, 1, 4108, WhiteGeometry, 1);
			TArray<FLayerPolygon> BlackGeometry;
			BlackGeometry.Add(Polygon(ESightWeaveRenderMaskLayer::Vision, 1, Visible,
				Rectangle(2600.0, 100.0, 3300.0, 800.0)));
			const auto Revision2 = BuildPacket(Test, 2, 4108, BlackGeometry, 1, Revision1);
			AddReadback(
				Test,
				*Context,
				{ Revision1, Revision2 },
				FIntPoint(1, 0),
				BlackGeometry,
				{ 1, 1 },
				{ 1, 1 });
			if (!Context->Readbacks.IsEmpty())
			{
				Context->Readbacks[0].bExpectAllBlack = true;
				Context->Readbacks[0].ExpectedFinalPages = 1;
				Context->Readbacks[0].ExpectedFinalResidents = 1;
			}
		}
		else if (Name == TEXT("StaleDuplicateNoWork"))
		{
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Bypass, 1, Common,
				Rectangle(200.0, 200.0, 900.0, 900.0)));
			const auto Revision2 = BuildPacket(Test, 2, 4109, Geometry);
			const auto Revision1 = BuildPacket(Test, 1, 4109, Geometry);
			const auto Revision3 = BuildPacket(Test, 3, 4109, Geometry, 128, Revision2);
			AddReadback(
				Test,
				*Context,
				{ Revision2, Revision2, Revision1, Revision3 },
				FIntPoint(0, 0),
				Geometry,
				{ 1, 1, 1, 0 },
				{ 1, 0, 0, 0 });
			if (!Context->Readbacks.IsEmpty())
			{
				Context->Readbacks[0].ExpectedDuplicateCount = 1;
				Context->Readbacks[0].ExpectedStaleCount = 1;
			}
		}
		else if (Name == TEXT("AsyncReadbackStaleRejection"))
		{
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Bypass, 1, Common,
				Rectangle(200.0, 200.0, 900.0, 900.0)));
			const auto Packet = BuildPacket(Test, 1, 4112, Geometry);
			AddReadback(Test, *Context, { Packet }, FIntPoint(0, 0), Geometry);
			if (!Context->Readbacks.IsEmpty())
			{
				++Context->Readbacks[0].Expectation.PacketRevision;
				Context->Readbacks[0].bExpectDiscardedStale = true;
			}
		}
		else if (Name == TEXT("PageBoundary65"))
		{
			const double Span = 2480.0;
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Bypass, 1, Common,
				Rectangle(1.0, 1.0, 65.0 * Span, 100.0)));
			const auto Packet = BuildPacket(Test, 1, 4110, Geometry, 128);
			AddReadback(Test, *Context, { Packet }, FIntPoint(64, 0), Geometry, { 65 }, { 65 });
			if (!Context->Readbacks.IsEmpty())
			{
				Context->Readbacks[0].ExpectedFinalPages = 2;
				Context->Readbacks[0].ExpectedFinalResidents = 65;
			}
		}
		else if (Name == TEXT("Capacity128"))
		{
			const double Span = 2480.0;
			Geometry.Add(Polygon(ESightWeaveRenderMaskLayer::Bypass, 1, Common,
				Rectangle(1.0, 1.0, 128.0 * Span, 100.0)));
			const auto Packet = BuildPacket(Test, 1, 4111, Geometry, 128);
			AddReadback(Test, *Context, { Packet }, FIntPoint(127, 0), Geometry, { 128 }, { 128 });
			if (!Context->Readbacks.IsEmpty())
			{
				Context->Readbacks[0].ExpectedFinalPages = 2;
				Context->Readbacks[0].ExpectedFinalResidents = 128;
			}
		}
		return Context;
	}

	class FWaitForSparseReadback final : public IAutomationLatentCommand
	{
	public:
		FWaitForSparseReadback(TSharedPtr<FCaseContext> InContext, FAutomationTestBase* InTest)
			: Context(MoveTemp(InContext))
			, Test(InTest)
		{
		}

		virtual bool Update() override
		{
			if (!Context.IsValid() || Context->Readbacks.IsEmpty())
			{
				Test->AddError(TEXT("M3.2 sparse GPU context has no readbacks"));
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
					Test->AddError(Context->Name + TEXT(": sparse GPU readback timed out"));
					return true;
				}
				return false;
			}
			for (FExpectedReadback& Expected : Context->Readbacks)
			{
				FSightWeaveSparseReadbackResult Result;
				if (!Expected.Request->TryTakeResult(Expected.Expectation, Result))
				{
					Test->AddError(Context->Name + TEXT(": completed sparse result was unavailable"));
					continue;
				}
				if (Expected.bExpectDiscardedStale)
				{
					Test->TestEqual(
						*FString::Printf(TEXT("%s stale readback is discarded"), *Context->Name),
						Result.Status,
						ESightWeaveSparseReadbackStatus::DiscardedStale);
					Test->TestEqual(
						*FString::Printf(TEXT("%s stale readback releases pixels"), *Context->Name),
						Result.Pixels.Num(),
						0);
					continue;
				}
				ValidatePixels(Test, Context->Name, Expected, Result);
			}
			return true;
		}

	private:
		TSharedPtr<FCaseContext> Context;
		FAutomationTestBase* Test = nullptr;
	};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
	FSightWeaveM3P2D3D12ReadbackTest,
	"SightWeave.M3P2.D3D12.SparseReadback",
	SightWeave::M3P2::D3D12Tests::TestFlags)

void FSightWeaveM3P2D3D12ReadbackTest::GetTests(
	TArray<FString>& OutBeautifiedNames,
	TArray<FString>& OutTestCommands) const
{
	static const TCHAR* Cases[] = {
		TEXT("HorizontalSeam"),
		TEXT("VerticalSeam"),
		TEXT("DiagonalSeam"),
		TEXT("FourTileCorner"),
		TEXT("ProfilesBypassSuppression"),
		TEXT("NegativeCoordinates"),
		TEXT("PersistentNoChangeOneDirty"),
		TEXT("SlotReuseClearsBlack"),
		TEXT("StaleDuplicateNoWork"),
		TEXT("AsyncReadbackStaleRejection"),
		TEXT("PageBoundary65"),
		TEXT("Capacity128")
	};
	for (const TCHAR* Case : Cases)
	{
		OutBeautifiedNames.Add(Case);
		OutTestCommands.Add(Case);
	}
}

bool FSightWeaveM3P2D3D12ReadbackTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P2::D3D12Tests;
	TSharedPtr<FCaseContext> Context = BuildCase(this, Parameters);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitForSparseReadback(Context, this));
	return true;
}

#endif
