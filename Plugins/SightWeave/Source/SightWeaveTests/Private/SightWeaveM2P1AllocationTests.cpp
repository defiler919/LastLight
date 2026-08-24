#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/MemoryTrace.h"
#include "SightWeaveGeometry.h"
#include "SightWeaveWorldSubsystem.h"
#include "Trace/Analysis.h"
#include "Trace/Analyzer.h"
#include "Trace/DataStream.h"
#include "Trace/Trace.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

UE_TRACE_CHANNEL_DEFINE(
	SightWeaveAllocationChannel,
	"Editor-only scope markers for deterministic SightWeave allocation measurements.");

UE_TRACE_EVENT_BEGIN(SightWeave, AllocationScope)
	UE_TRACE_EVENT_FIELD(uint8, Workload)
	UE_TRACE_EVENT_FIELD(uint8, Phase)
	UE_TRACE_EVENT_FIELD(uint16, Sample)
UE_TRACE_EVENT_END()

namespace SightWeave::M2P1::AllocationTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	const FSightWeaveFloorId Ground(FName(TEXT("Ground")));
	const FSightWeaveKnowledgeOwnerId Local(FName(TEXT("Local")));

	enum class EWorkload : uint8
	{
		Solver2x64,
		Solver8x64,
		Solver4096Total,
		Solver4096PerSource,
		PointQuery,
		Batch512,
		SourceTransformUpdate,
		DynamicDoorUpdate,
		CleanPublication,
		NoChangeUpdate,
		Count
	};

	const TCHAR* WorkloadName(const EWorkload Workload)
	{
		switch (Workload)
		{
		case EWorkload::Solver2x64: return TEXT("solver_2x64");
		case EWorkload::Solver8x64: return TEXT("solver_8x64");
		case EWorkload::Solver4096Total: return TEXT("solver_4096_total");
		case EWorkload::Solver4096PerSource: return TEXT("solver_4096_per_source");
		case EWorkload::PointQuery: return TEXT("point_query");
		case EWorkload::Batch512: return TEXT("batch_512");
		case EWorkload::SourceTransformUpdate: return TEXT("source_transform_update");
		case EWorkload::DynamicDoorUpdate: return TEXT("dynamic_door_update");
		case EWorkload::CleanPublication: return TEXT("clean_publication");
		case EWorkload::NoChangeUpdate: return TEXT("no_change_update");
		default: return TEXT("unknown");
		}
	}

	class FAllocationScope
	{
	public:
		FAllocationScope(const EWorkload InWorkload, const uint16 InSample)
			: Workload(InWorkload)
			, Sample(InSample)
		{
			UE_TRACE_LOG(SightWeave, AllocationScope, SightWeaveAllocationChannel)
				<< AllocationScope.Workload(static_cast<uint8>(Workload))
				<< AllocationScope.Phase(0)
				<< AllocationScope.Sample(Sample);
		}

		~FAllocationScope()
		{
			UE_TRACE_LOG(SightWeave, AllocationScope, SightWeaveAllocationChannel)
				<< AllocationScope.Workload(static_cast<uint8>(Workload))
				<< AllocationScope.Phase(1)
				<< AllocationScope.Sample(Sample);
		}

	private:
		EWorkload Workload;
		uint16 Sample;
	};

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

	FSightWeaveFloorDefinition Floor()
	{
		FSightWeaveFloorDefinition Result;
		Result.FloorId = Ground;
		Result.BoundsMin = FVector2D(-20000.0, -20000.0);
		Result.BoundsMax = FVector2D(20000.0, 20000.0);
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		return Result;
	}

	TArray<FSightWeaveSegment2D> MakeSegments(const int32 Count, const int32 Seed, const bool bDense)
	{
		FRandomStream Random(Seed);
		TArray<FSightWeaveSegment2D> Segments;
		Segments.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			constexpr int32 SegmentsPerRing = 128;
			const int32 RingCount = FMath::Max(1, FMath::DivideAndRoundUp(Count, SegmentsPerRing));
			const int32 RingIndex = Index / SegmentsPerRing;
			const int32 IndexInRing = Index % SegmentsPerRing;
			const double Radius = bDense
				? 180.0 + 850.0 * (static_cast<double>(RingIndex) + 0.5) / RingCount
				: Random.FRandRange(100.0f, 1050.0f);
			const double PolarAngle = bDense
				? -PI + 2.0 * PI * (static_cast<double>(IndexInRing) + 0.25 * (RingIndex % 2)) / SegmentsPerRing
				: Random.FRandRange(-PI, PI);
			const FVector2D Center(FMath::Cos(PolarAngle) * Radius, FMath::Sin(PolarAngle) * Radius);
			const double SegmentAngle = bDense ? PolarAngle + PI * 0.5 : Random.FRandRange(-PI, PI);
			const double HalfLength = bDense ? 1.5 : Random.FRandRange(12.0f, 70.0f);
			const FVector2D Offset(FMath::Cos(SegmentAngle) * HalfLength, FMath::Sin(SegmentAngle) * HalfLength);
			FSightWeaveSegment2D& Segment = Segments.AddDefaulted_GetRef();
			Segment.A = Center - Offset;
			Segment.B = Center + Offset;
			Segment.FloorId = Ground;
			Segment.HeightRange.ZMin = 0.0f;
			Segment.HeightRange.ZMax = 300.0f;
			Segment.StableId = Index + 1;
		}
		return Segments;
	}

	FSightWeaveReferenceSolveInput SolveInput(
		const TArray<FSightWeaveSegment2D>& Segments,
		const int32 SourceIndex,
		const int32 SourceCount)
	{
		FSightWeaveReferenceSolveInput Input;
		const double SourceAngle = 2.0 * PI * static_cast<double>(SourceIndex) / FMath::Max(SourceCount, 1);
		Input.Origin = FVector(FMath::Cos(SourceAngle) * 75.0, FMath::Sin(SourceAngle) * 75.0, 100.0);
		Input.Forward = FVector2D(-FMath::Cos(SourceAngle), -FMath::Sin(SourceAngle));
		Input.Shape = ESightWeaveSourceShape::Radial;
		Input.Range = 1200.0;
		Input.HalfAngleDegrees = 180.0;
		Input.FloorId = Ground;
		Input.HeightRange.ZMin = 0.0f;
		Input.HeightRange.ZMax = 300.0f;
		Input.Segments = Segments;
		return Input;
	}

	void TraceSolverWorkload(
		const EWorkload Workload,
		const int32 SourceCount,
		const int32 SegmentsPerSource,
		const bool bDense,
		int64& ResultSink)
	{
		TArray<FSightWeaveReferenceSolveInput> Inputs;
		Inputs.Reserve(SourceCount);
		for (int32 SourceIndex = 0; SourceIndex < SourceCount; ++SourceIndex)
		{
			const TArray<FSightWeaveSegment2D> Segments = MakeSegments(
				SegmentsPerSource,
				0x51A7E + SourceIndex * 7919,
				bDense);
			Inputs.Add(SolveInput(Segments, SourceIndex, SourceCount));
		}

		TArray<FSightWeaveReferenceSolveResult> Results;
		Results.SetNum(SourceCount);
		for (int32 SourceIndex = 0; SourceIndex < Inputs.Num(); ++SourceIndex)
		{
			SightWeave::Geometry::SolvePolygonInto(
				Inputs[SourceIndex],
				ESightWeaveSolverMode::Optimized,
				Results[SourceIndex]);
			ResultSink += Results[SourceIndex].Vertices.Num();
		}

		for (uint16 Sample = 0; Sample < 3; ++Sample)
		{
			FAllocationScope Scope(Workload, Sample);
			for (int32 SourceIndex = 0; SourceIndex < Inputs.Num(); ++SourceIndex)
			{
				SightWeave::Geometry::SolvePolygonInto(
					Inputs[SourceIndex],
					ESightWeaveSolverMode::Optimized,
					Results[SourceIndex]);
				ResultSink += Results[SourceIndex].Vertices.Num();
			}
		}
	}

	FSightWeaveVisionSourceDescription Vision(const FVector Location, const ESightWeaveSourceShape Shape)
	{
		FSightWeaveVisionSourceDescription Result;
		Result.KnowledgeOwnerId = Local;
		Result.FloorId = Ground;
		Result.Transform.SetLocation(Location);
		Result.Shape = Shape;
		Result.Range = 1200.0f;
		Result.HalfAngleDegrees = 55.0f;
		Result.NearAwarenessRadius = Shape == ESightWeaveSourceShape::CameraCone ? 75.0f : 0.0f;
		Result.HeightRange.ZMin = 0.0f;
		Result.HeightRange.ZMax = 300.0f;
		Result.IlluminationPolicy = ESightWeaveIlluminationPolicy::BypassLegalIllumination;
		return Result;
	}

	void EmitScope(const EWorkload Workload, const uint16 Sample, TFunctionRef<void()> Operation)
	{
		FAllocationScope Scope(Workload, Sample);
		Operation();
	}

	struct FAllocationSample
	{
		EWorkload Workload = EWorkload::Solver2x64;
		uint16 Sample = 0;
		uint32 ThreadId = 0;
		uint64 AllocationCalls = 0;
		uint64 ReallocationCalls = 0;
		uint64 FreeCalls = 0;
		uint64 AllocatedBytes = 0;
		uint64 PeakTemporaryBytes = 0;
		uint64 CurrentTemporaryBytes = 0;
		TArray<uint64> AllocationSizes;
		TArray<uint32> AllocationCallstackIds;
		TMap<uint64, uint64> TemporaryAllocations;
	};

	class FAllocationAnalyzer final : public UE::Trace::IAnalyzer
	{
	public:
		virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override
		{
			FInterfaceBuilder& Builder = Context.InterfaceBuilder;
			Builder.RouteEvent(RouteScope, "SightWeave", "AllocationScope");
			Builder.RouteEvent(RouteInit, "Memory", "Init");
			Builder.RouteEvent(RouteAlloc, "Memory", "Alloc");
			Builder.RouteEvent(RouteAllocSystem, "Memory", "AllocSystem");
			Builder.RouteEvent(RouteFree, "Memory", "Free");
			Builder.RouteEvent(RouteFreeSystem, "Memory", "FreeSystem");
			Builder.RouteEvent(RouteReallocAlloc, "Memory", "ReallocAlloc");
			Builder.RouteEvent(RouteReallocAllocSystem, "Memory", "ReallocAllocSystem");
			Builder.RouteEvent(RouteReallocFree, "Memory", "ReallocFree");
			Builder.RouteEvent(RouteReallocFreeSystem, "Memory", "ReallocFreeSystem");
		}

		virtual bool OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context) override
		{
			if (RouteId == RouteInit)
			{
				SizeShift = Context.EventData.GetValue<uint8>("SizeShift", 3);
				return true;
			}

			if (RouteId == RouteScope)
			{
				const EWorkload Workload = static_cast<EWorkload>(Context.EventData.GetValue<uint8>("Workload"));
				const uint8 Phase = Context.EventData.GetValue<uint8>("Phase");
				const uint16 Sample = Context.EventData.GetValue<uint16>("Sample");
				const uint32 ThreadId = Context.ThreadInfo.GetId();
				if (Phase == 0)
				{
					FAllocationSample& Active = ActiveScopes.FindOrAdd(ThreadId);
					Active = FAllocationSample();
					Active.Workload = Workload;
					Active.Sample = Sample;
					Active.ThreadId = ThreadId;
				}
				else if (FAllocationSample* Active = ActiveScopes.Find(ThreadId))
				{
					Samples.Add(MoveTemp(*Active));
					ActiveScopes.Remove(ThreadId);
				}
				return true;
			}

			FAllocationSample* Active = ActiveScopes.Find(Context.ThreadInfo.GetId());
			if (!Active)
			{
				return true;
			}

			const uint64 Address = Context.EventData.GetValue<uint64>("Address");
			switch (RouteId)
			{
			case RouteAlloc:
			case RouteAllocSystem:
			{
				const uint64 Size = DecodeSize(Context.EventData);
				++Active->AllocationCalls;
				Active->AllocatedBytes += Size;
				Active->AllocationSizes.Add(Size);
				Active->AllocationCallstackIds.Add(Context.EventData.GetValue<uint32>("CallstackId"));
				TrackAllocation(*Active, Address, Size);
				break;
			}
			case RouteReallocAlloc:
			case RouteReallocAllocSystem:
			{
				const uint64 Size = DecodeSize(Context.EventData);
				++Active->ReallocationCalls;
				Active->AllocatedBytes += Size;
				TrackAllocation(*Active, Address, Size);
				break;
			}
			case RouteFree:
			case RouteFreeSystem:
				++Active->FreeCalls;
				TrackFree(*Active, Address);
				break;
			case RouteReallocFree:
			case RouteReallocFreeSystem:
				TrackFree(*Active, Address);
				break;
			default:
				break;
			}
			return true;
		}

		const TArray<FAllocationSample>& GetSamples() const
		{
			return Samples;
		}

		FString MakeCsv() const
		{
			FString Csv(TEXT("workload,sample,thread_id,allocation_calls,reallocation_calls,free_calls,allocated_bytes,peak_temporary_bytes,end_temporary_bytes,allocation_details\n"));
			for (const FAllocationSample& Sample : Samples)
			{
				FString AllocationDetails;
				for (int32 Index = 0; Index < Sample.AllocationSizes.Num(); ++Index)
				{
					if (Index > 0) AllocationDetails += TEXT("|");
					AllocationDetails += FString::Printf(
						TEXT("%llu@%u"),
						Sample.AllocationSizes[Index],
						Sample.AllocationCallstackIds.IsValidIndex(Index)
							? Sample.AllocationCallstackIds[Index]
							: 0);
				}
				Csv += FString::Printf(
					TEXT("%s,%u,%u,%llu,%llu,%llu,%llu,%llu,%llu,%s\n"),
					WorkloadName(Sample.Workload),
					Sample.Sample,
					Sample.ThreadId,
					Sample.AllocationCalls,
					Sample.ReallocationCalls,
					Sample.FreeCalls,
					Sample.AllocatedBytes,
					Sample.PeakTemporaryBytes,
					Sample.CurrentTemporaryBytes,
					*AllocationDetails);
			}
			return Csv;
		}

	private:
		enum : uint16
		{
			RouteScope = 1,
			RouteInit,
			RouteAlloc,
			RouteAllocSystem,
			RouteFree,
			RouteFreeSystem,
			RouteReallocAlloc,
			RouteReallocAllocSystem,
			RouteReallocFree,
			RouteReallocFreeSystem
		};

		uint64 DecodeSize(const FEventData& EventData) const
		{
			const uint64 Upper = EventData.GetValue<uint32>("Size");
			const uint8 Packed = EventData.GetValue<uint8>("AlignmentPow2_SizeLower");
			const uint64 LowerMask = (uint64(1) << SizeShift) - 1;
			return (Upper << SizeShift) | (Packed & LowerMask);
		}

		static void TrackAllocation(FAllocationSample& Sample, const uint64 Address, const uint64 Size)
		{
			if (Address == 0 || Size == 0) return;
			if (const uint64* PreviousSize = Sample.TemporaryAllocations.Find(Address))
			{
				Sample.CurrentTemporaryBytes -= *PreviousSize;
			}
			Sample.TemporaryAllocations.Add(Address, Size);
			Sample.CurrentTemporaryBytes += Size;
			Sample.PeakTemporaryBytes = FMath::Max(Sample.PeakTemporaryBytes, Sample.CurrentTemporaryBytes);
		}

		static void TrackFree(FAllocationSample& Sample, const uint64 Address)
		{
			if (const uint64* Size = Sample.TemporaryAllocations.Find(Address))
			{
				Sample.CurrentTemporaryBytes -= *Size;
				Sample.TemporaryAllocations.Remove(Address);
			}
		}

		uint8 SizeShift = 3;
		TMap<uint32, FAllocationSample> ActiveScopes;
		TArray<FAllocationSample> Samples;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P1AllocationCaptureTest,
	"SightWeave.M2P1.Allocation.Capture",
	SightWeave::M2P1::AllocationTests::TestFlags)

bool FSightWeaveM2P1AllocationCaptureTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P1::AllocationTests;
	if (!FParse::Param(FCommandLine::Get(), TEXT("SightWeaveAllocationCapture")))
	{
		AddInfo(TEXT("Allocation capture requires the explicit -SightWeaveAllocationCapture mode."));
		return true;
	}
	if (!TestTrue(TEXT("SightWeave allocation trace channel is enabled"), UE_TRACE_CHANNELEXPR_IS_ENABLED(SightWeaveAllocationChannel))
		|| !TestTrue(TEXT("Engine memory allocation trace channel is enabled"), UE_TRACE_CHANNELEXPR_IS_ENABLED(MemAllocChannel)))
	{
		AddError(TEXT("Run with -trace=memory,sightweaveallocation and an explicit -tracefile path."));
		return false;
	}

	int64 ResultSink = 0;
	TraceSolverWorkload(EWorkload::Solver2x64, 2, 64, false, ResultSink);
	TraceSolverWorkload(EWorkload::Solver8x64, 8, 64, false, ResultSink);
	TraceSolverWorkload(EWorkload::Solver4096Total, 8, 512, true, ResultSink);
	TraceSolverWorkload(EWorkload::Solver4096PerSource, 8, 4096, true, ResultSink);

	FTestWorld World(TEXT("SightWeaveM2P1AllocationCapture"));
	USightWeaveWorldSubsystem* Subsystem = World.GetSubsystem();
	if (!TestNotNull(TEXT("Subsystem exists"), Subsystem)
		|| !TestTrue(TEXT("Floor registers"), Subsystem->RegisterFloor(Floor(), nullptr)))
	{
		return false;
	}

	TArray<FSightWeaveSegment2D> StaticSegments = MakeSegments(64, 0x51A7E, false);
	for (FSightWeaveSegment2D& Segment : StaticSegments) Segment.StableId = 0;
	TestTrue(TEXT("Static fixture registers"), Subsystem->RegisterOccluder(StaticSegments, false, true, nullptr).IsValid());

	TArray<FSightWeaveVisionSourceHandle> VisionHandles;
	TArray<FSightWeaveVisionSourceDescription> VisionDescriptions;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const double Angle = 2.0 * PI * Index / 4.0;
		VisionDescriptions.Add(Vision(
			FVector(FMath::Cos(Angle) * 100.0, FMath::Sin(Angle) * 100.0, 100.0),
			Index % 2 == 0 ? ESightWeaveSourceShape::Radial : ESightWeaveSourceShape::CameraCone));
		VisionHandles.Add(Subsystem->RegisterVisionSource(VisionDescriptions.Last(), nullptr));
	}

	FSightWeaveVisibilityQueryResult PointResult;
	Subsystem->QueryEffectiveLiveAtLocationInto(Local, Ground, FVector(400.0, 0.0, 100.0), PointResult);
	for (uint16 Sample = 0; Sample < 3; ++Sample)
	{
		EmitScope(EWorkload::PointQuery, Sample, [&]
		{
			Subsystem->QueryEffectiveLiveAtLocationInto(
				Local,
				Ground,
				FVector(400.0, static_cast<double>(Sample), 100.0),
				PointResult);
		});
	}

	TArray<FSightWeaveQueryRequest> Requests;
	Requests.Reserve(512);
	for (int32 Index = 0; Index < 512; ++Index)
	{
		const double Angle = 2.0 * PI * Index / 512.0;
		FSightWeaveQueryRequest& Request = Requests.AddDefaulted_GetRef();
		Request.KnowledgeOwnerId = Local;
		Request.FloorId = Ground;
		Request.SampleSet.Samples.Add(FVector(FMath::Cos(Angle) * 500.0, FMath::Sin(Angle) * 500.0, 100.0));
	}
	TArray<FSightWeaveVisibilityQueryResult> BatchResults;
	Subsystem->QueryBatch(Requests, BatchResults);
	for (uint16 Sample = 0; Sample < 3; ++Sample)
	{
		EmitScope(EWorkload::Batch512, Sample, [&] { Subsystem->QueryBatch(Requests, BatchResults); });
	}

	FSightWeaveSegment2D Door;
	Door.A = FVector2D(250.0, -100.0);
	Door.B = FVector2D(250.0, 100.0);
	Door.FloorId = Ground;
	Door.HeightRange.ZMin = 0.0f;
	Door.HeightRange.ZMax = 300.0f;
	TArray<FSightWeaveSegment2D> DoorSegments;
	DoorSegments.Add(Door);
	const FSightWeaveOccluderHandle DoorHandle =
		Subsystem->RegisterOccluder(DoorSegments, true, true, nullptr);
	for (int32 Warmup = 0; Warmup < 2; ++Warmup)
	{
		const double X = Warmup % 2 == 0 ? 850.0 : 250.0;
		DoorSegments[0].A.X = X;
		DoorSegments[0].B.X = X;
		Subsystem->UpdateOccluder(DoorHandle, DoorSegments, true, true);
	}
	for (uint16 Sample = 0; Sample < 3; ++Sample)
	{
		const double X = Sample % 2 == 0 ? 850.0 : 250.0;
		DoorSegments[0].A.X = X;
		DoorSegments[0].B.X = X;
		EmitScope(EWorkload::DynamicDoorUpdate, Sample, [&]
		{
			Subsystem->UpdateOccluder(DoorHandle, DoorSegments, true, true);
		});
	}

	for (uint16 Sample = 0; Sample < 3; ++Sample)
	{
		FSightWeaveVisionSourceDescription& Description = VisionDescriptions[0];
		Description.Transform.SetLocation(FVector(Sample % 2 == 0 ? 5.0 : 0.0, 0.0, 100.0));
		EmitScope(EWorkload::SourceTransformUpdate, Sample, [&]
		{
			Subsystem->UpdateVisionSource(VisionHandles[0], Description);
		});
	}

	for (uint16 Sample = 0; Sample < 3; ++Sample)
	{
		EmitScope(EWorkload::CleanPublication, Sample, [&] { Subsystem->PublishSnapshot(); });
	}

	for (uint16 Sample = 0; Sample < 3; ++Sample)
	{
		EmitScope(EWorkload::NoChangeUpdate, Sample, [&]
		{
			Subsystem->UpdateVisionSource(VisionHandles[0], VisionDescriptions[0]);
		});
	}

	TestTrue(TEXT("Capture work produced authoritative results"), PointResult.bAuthoritative && BatchResults.Num() == 512);
	TestTrue(TEXT("Solver results were consumed"), ResultSink > 0);
	AddInfo(TEXT("M2P1_ALLOCATION_CAPTURE scopes=30 samples_per_workload=3 method=UE_startup_memory_trace_current_thread_scope"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM2P1AllocationAnalyzeTest,
	"SightWeave.M2P1.Allocation.Analyze",
	SightWeave::M2P1::AllocationTests::TestFlags)

bool FSightWeaveM2P1AllocationAnalyzeTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M2P1::AllocationTests;
	if (!FParse::Param(FCommandLine::Get(), TEXT("SightWeaveAllocationAnalyze")))
	{
		AddInfo(TEXT("Allocation analysis requires the explicit -SightWeaveAllocationAnalyze mode."));
		return true;
	}
	FString TracePath;
	FString ReportPath;
	if (!FParse::Value(FCommandLine::Get(), TEXT("SightWeaveAllocationTrace="), TracePath)
		|| !FParse::Value(FCommandLine::Get(), TEXT("SightWeaveAllocationReport="), ReportPath))
	{
		AddError(TEXT("Pass -SightWeaveAllocationTrace=<utrace> and -SightWeaveAllocationReport=<csv>."));
		return false;
	}

	TracePath = FPaths::ConvertRelativePathToFull(TracePath);
	ReportPath = FPaths::ConvertRelativePathToFull(ReportPath);
	UE::Trace::FFileDataStream DataStream;
	if (!TestTrue(TEXT("Allocation trace opens"), DataStream.Open(*TracePath)))
	{
		return false;
	}

	FAllocationAnalyzer Analyzer;
	UE::Trace::FAnalysisContext AnalysisContext;
	AnalysisContext.AddAnalyzer(Analyzer);
	AnalysisContext.Process(DataStream).Wait();

	const TArray<FAllocationSample>& Samples = Analyzer.GetSamples();
	TestEqual(TEXT("All expected workload samples were analyzed"), Samples.Num(), 30);
	TArray<int32> Counts;
	Counts.Init(0, static_cast<int32>(EWorkload::Count));
	for (const FAllocationSample& Sample : Samples)
	{
		const int32 Index = static_cast<int32>(Sample.Workload);
		if (Counts.IsValidIndex(Index)) ++Counts[Index];
	}
	for (int32 Index = 0; Index < Counts.Num(); ++Index)
	{
		TestEqual(*FString::Printf(TEXT("Workload %s has three samples"), WorkloadName(static_cast<EWorkload>(Index))), Counts[Index], 3);
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	TestTrue(TEXT("Allocation CSV report writes"), FFileHelper::SaveStringToFile(
		Analyzer.MakeCsv(),
		*ReportPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	AddInfo(FString::Printf(TEXT("M2P1_ALLOCATION_REPORT path=%s samples=%d"), *ReportPath, Samples.Num()));
	return true;
}

#endif
