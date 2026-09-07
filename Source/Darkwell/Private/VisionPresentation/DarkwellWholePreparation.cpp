#include "VisionPresentation/DarkwellObjectMemoryScene.h"
#include "VisionPresentation/DarkwellHistoryPreparation.h"
#include "VisionPresentation/DarkwellRememberablePropComponent.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"
#include "UObject/ObjectKey.h"

namespace
{
struct FPreparationTimer
{
 double& Total; double* Maximum; double Start = FPlatformTime::Seconds();
 FPreparationTimer(double& InTotal, double* InMaximum = nullptr) : Total(InTotal), Maximum(InMaximum) {}
 ~FPreparationTimer() { const double Elapsed = FPlatformTime::Seconds()-Start; Total += Elapsed; if(Maximum) *Maximum=FMath::Max(*Maximum,Elapsed); }
};
TAutoConsoleVariable<int32> Mode(TEXT("r.Darkwell.ObjectMemory.WholeGeometryPreparation"), 0,
	TEXT("First stationary Whole geometry: 0 synchronous, 1 shadow parity, 2 prepare and consume."));
TAutoConsoleVariable<int32> FrameGuard(TEXT("r.Darkwell.ObjectMemory.WholePreparationFrameGuard"), 1,
	TEXT("Gate optional preparation after foreground work and check in-flight budget. 0: legacy diagnostic oracle."));
uint64 PreparationFrame(const ADarkwellObjectMemoryScene& Scene)
{
#if WITH_DEV_AUTOMATION_TESTS
 if (Scene.WholePreparationFrameForTesting != MAX_uint64) return Scene.WholePreparationFrameForTesting;
#endif
 return GFrameCounter;
}
int32 PreparationWorkLimit(const ADarkwellObjectMemoryScene& Scene)
{
#if WITH_DEV_AUTOMATION_TESTS
 return Scene.WholePreparationWorkForTesting;
#else
 return 65536;
#endif
}
double PreparationTimeLimit(const ADarkwellObjectMemoryScene& Scene)
{
#if WITH_DEV_AUTOMATION_TESTS
 if (Scene.WholePreparationFrameForTesting != MAX_uint64) return DBL_MAX;
#endif
 return .001;
}

}

struct FDarkwellWholePreparationState
{
	using FGeometry = ADarkwellObjectMemoryScene::FPrimitiveGeometrySnapshot;
	struct FPart { FBox Bounds; FTransform Pose; uint64 Primitive, Mesh; };
	struct FInput
	{
		FName Id;
		FObjectKey Host, World, Source, Policy;
		FGuid History;
		uint32 Epoch = 0;
		uint64 Content = 0, GeometryRevision = 0, PolicyRevision = 0, MovingRevision = 0;
		FResolvedSightWeaveObjectPolicy Rules;
		FTransform Pose;
		FBox2D Bounds;
		FIntPoint Size;
		TArray<FPart> Parts;
		TArray<FGeometry> Capture;
		bool Matches(const FInput& B) const
		{
			if (Id != B.Id || Host != B.Host || World != B.World || Source != B.Source || Policy != B.Policy
				|| History != B.History || Epoch != B.Epoch || Content != B.Content
				|| GeometryRevision != B.GeometryRevision || PolicyRevision != B.PolicyRevision
				|| MovingRevision != B.MovingRevision || Rules.RevealMode != B.Rules.RevealMode
				|| Rules.HistoryMode != B.Rules.HistoryMode || Rules.MinimumObservedSpanCm != B.Rules.MinimumObservedSpanCm
				|| !Pose.Equals(B.Pose, 0) || Bounds.Min != B.Bounds.Min || Bounds.Max != B.Bounds.Max
				|| Size != B.Size || Parts.Num() != B.Parts.Num() || Capture.Num() != B.Capture.Num()) return false;
			for (int32 I = 0; I < Parts.Num(); ++I)
			{
				const auto& P = Parts[I]; const auto& Q = B.Parts[I];
				if (P.Bounds.Min != Q.Bounds.Min || P.Bounds.Max != Q.Bounds.Max || !P.Pose.Equals(Q.Pose, 0)
					|| P.Primitive != Q.Primitive || P.Mesh != Q.Mesh) return false;
			}
			for (int32 I = 0; I < Capture.Num(); ++I)
			{
				const auto& P = Capture[I]; const auto& Q = B.Capture[I];
				if (P.LocalBounds.Min != Q.LocalBounds.Min || P.LocalBounds.Max != Q.LocalBounds.Max
					|| !P.WorldTransform.Equals(Q.WorldTransform, 0) || P.PrimitiveIndex != Q.PrimitiveIndex
					|| P.bCachedPlanarProjection != Q.bCachedPlanarProjection || P.PlanarMinZ != Q.PlanarMinZ
					|| P.PlanarMaxZ != Q.PlanarMaxZ || P.ToleranceScale != Q.ToleranceScale
					|| P.ProjectionBounds.Min != Q.ProjectionBounds.Min || P.ProjectionBounds.Max != Q.ProjectionBounds.Max
					|| P.ProjectionToleranceFactor != Q.ProjectionToleranceFactor
					|| P.ProjectionRoundoffMargin != Q.ProjectionRoundoffMargin) return false;
			}
			return true;
		}
	};
	struct FEntry { FInput Input; Darkwell::HistoryPreparation::FMaskJob Job; };
	TArray<FEntry> Entries;
	Darkwell::HistoryPreparation::FFrameBudget Budget;
	uint64 Generation = 1, Serial = 0;
	int32 RoundRobin = 0, LastMode = 0;
	bool bCharging = false;
	uint64 SuppressedFrame = MAX_uint64;
	uint64 SuppressedFrames = 0;
	uint64 Requests = 0, Hits = 0, Fallbacks = 0, Cancelled = 0, Shadow = 0;
	double LastSealStartSeconds = 0;
 uint64 DiagnosticFrame = MAX_uint64;
 double RequestSeconds=0, SnapshotSeconds=0, AdmissionSeconds=0, StepSeconds=0, MaxStepSeconds=0, CancelSeconds=0, TakeSeconds=0;
 void BeginDiagnostics(uint64 Frame) { if(Frame==DiagnosticFrame) return; DiagnosticFrame=Frame; RequestSeconds=SnapshotSeconds=AdmissionSeconds=StepSeconds=MaxStepSeconds=CancelSeconds=TakeSeconds=0; }


	static bool Snapshot(ADarkwellObjectMemoryScene& Scene, ADarkwellObjectMemoryScene::FTrackedProp& Prop, FInput& Out)
	{
		FPreparationTimer SnapshotTimer(Scene.WholePreparation->SnapshotSeconds);
		const auto* Source = Prop.bExists ? Prop.Actual.Get() : nullptr;
		const auto* Policy = Prop.ObjectPolicy.Get();
		const int32 Index = Prop.History.GetCurrentIndex();
		if (!Source || Source->IsActorBeingDestroyed() || !Policy || Index == INDEX_NONE || !Scene.IsCaptureEligible(Prop)
			|| Policy->IsSightWeaveMoving() || !Prop.RevealObservation.IsConfirmed()) return false;
		const auto& Record = Prop.History.GetRecords()[Index];
		const auto* Visual = Prop.Visuals.Find(Record.Epoch);
		const auto* Memory = Source->FindComponentByClass<UDarkwellRememberablePropComponent>();
		if (!Memory || !Visual || Visual->bPresentationRetired || !Record.bCurrentObservedLocation
			|| !Record.bConfirmedWholeCapture || !Record.bCaptureRevisionValid || Record.FineHistory.IsInitialized()
			|| !Record.LastLegalCaptureMask.IsEmpty() || !Record.GeometryFootprint.IsEmpty()
			|| Record.CapturePolicyRevision != Prop.PolicyRevision
			|| Record.CaptureGeometryRevision != Prop.CurrentLive.GeometryResets
			|| !Record.SnapshotTransform.Equals(Prop.CurrentLive.LastLegalPose, 0)
			|| !Record.SnapshotTransform.Equals(Source->GetActorTransform(), 0)
			|| Record.ContentRevision != Memory->ComputeMemoryContentRevision()) return false;
		Out.Rules = Policy->GetResolvedPolicy();
		if (Out.Rules.RevealMode != ESightWeaveRevealMode::WholeObjectAfterSpan
			|| Out.Rules.HistoryMode == ESightWeaveHistoryMode::Never) return false;
		Out.Bounds = Record.SpatialMemory.GetBounds();
		Out.Size = Record.SpatialMemory.GetSize() * FDarkwellHistoryGridV2::SamplesPerCell;
		if (!Out.Bounds.bIsValid || Out.Size.X <= 0 || Out.Size.Y <= 0 || int64(Out.Size.X) * Out.Size.Y > 65536
			|| Prop.CurrentLive.Parts.IsEmpty() || Prop.CurrentLive.Parts.Num() > 16
			|| Visual->PartGeometry.IsEmpty() || Visual->PartGeometry.Num() > 16) return false;
		Out.Id = Prop.StableId; Out.Host = FObjectKey(&Scene); Out.World = FObjectKey(Scene.GetWorld()); Out.Source = FObjectKey(Source);
		Out.Policy = FObjectKey(Policy); Out.History = Prop.History.GetPreparationLifetime();
		Out.Epoch = Record.Epoch; Out.Content = Record.ContentRevision;
		Out.GeometryRevision = Prop.CurrentLive.GeometryResets; Out.PolicyRevision = Prop.PolicyRevision;
		Out.MovingRevision = Policy->GetMovingRevision(); Out.Pose = Record.SnapshotTransform;
		for (const auto& Part : Prop.CurrentLive.Parts)
		{
			if (!Part.Pose.Equals(Part.Geometry.RelativeTransform * Out.Pose, 0)) return false;
			Out.Parts.Add({Part.Geometry.LocalBounds, Part.Pose, Part.Geometry.PrimitiveKey, Part.Geometry.MeshKey});
		}
		Out.Capture = Visual->PartGeometry;
		// Do not trust the host's tolerance-based physical revision as an exact domain.
		const auto ActualGeometry = Scene.ActualPartGeometry(*Source);
		if (ActualGeometry.Num() != Out.Capture.Num()) return false;
		for (int32 I = 0; I < ActualGeometry.Num(); ++I)
			if (ActualGeometry[I].LocalBounds.Min != Out.Capture[I].LocalBounds.Min
				|| ActualGeometry[I].LocalBounds.Max != Out.Capture[I].LocalBounds.Max
				|| !ActualGeometry[I].WorldTransform.Equals(Out.Capture[I].WorldTransform, 0)) return false;
		return true;
	}
};

ADarkwellObjectMemoryScene::ADarkwellObjectMemoryScene()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ObjectMemoryRoot")));
	PrimaryActorTick.bCanEverTick = false;
	WholePreparation = MakeShared<FDarkwellWholePreparationState>();
}
ADarkwellObjectMemoryScene::~ADarkwellObjectMemoryScene() = default;

void ADarkwellObjectMemoryScene::InvalidateWholePreparation(const FName Id)
{
	check(IsInGameThread());
	if (!WholePreparation) return;
	auto& S = *WholePreparation;
	S.BeginDiagnostics(PreparationFrame(*this));
	FPreparationTimer CancelTimer(S.CancelSeconds);
	const bool bHadEntries = !S.Entries.IsEmpty();
	const double Start = FPlatformTime::Seconds();
	S.Cancelled += S.Entries.RemoveAll([&](const auto& E) { return Id.IsNone() || E.Input.Id == Id; });
	if (Id.IsNone()) { if (S.Generation != MAX_uint64) ++S.Generation; }
	if (bHadEntries && !S.bCharging) { S.Budget.Begin(PreparationFrame(*this)); S.Budget.Charge(FPlatformTime::Seconds() - Start, 0); }
	// Preserve this engine frame's expenditure, including through ResetMemory.
}

void ADarkwellObjectMemoryScene::RequestWholePreparation(FTrackedProp& Prop, bool bFrameTail)
{
	check(IsInGameThread());
	if (FrameGuard.GetValueOnGameThread() != 0 && !bFrameTail) return;
	auto& S = *WholePreparation;
	S.BeginDiagnostics(PreparationFrame(*this));
	FPreparationTimer RequestTimer(S.RequestSeconds);
	const int32 CurrentMode = Mode.GetValueOnGameThread();
	if (S.LastMode != CurrentMode) { InvalidateWholePreparation(); S.LastMode = CurrentMode; }
	if (CurrentMode <= 0 || CurrentMode > 2 || S.Generation == MAX_uint64 || S.Serial == MAX_uint64) return;
	S.Budget.Begin(PreparationFrame(*this));
	if (!S.Budget.CanAdvance(PreparationTimeLimit(*this), PreparationWorkLimit(*this))) return;
	const double Start = FPlatformTime::Seconds();
	TGuardValue<bool> Charging(S.bCharging, true);
	ON_SCOPE_EXIT { S.Budget.Charge(FPlatformTime::Seconds() - Start, 0); };
	// P1's conservative multi-host fallback avoids multiplying a world budget.
	int32 Hosts = 0;
	for (TActorIterator<ADarkwellObjectMemoryScene> It(GetWorld()); It; ++It) ++Hosts;
	if (Hosts != 1) { InvalidateWholePreparation(); return; }
	FDarkwellWholePreparationState::FInput Input;
	if (!FDarkwellWholePreparationState::Snapshot(*this, Prop, Input)) InvalidateWholePreparation(Prop.StableId);
	else
	{
		const int32 Existing = S.Entries.IndexOfByPredicate([&](const auto& E) { return E.Input.Id == Prop.StableId; });
		if (Existing != INDEX_NONE && !S.Entries[Existing].Input.Matches(Input)) InvalidateWholePreparation(Prop.StableId);
		if (FrameGuard.GetValueOnGameThread() != 0
			&& !S.Budget.CanAdvance(PreparationTimeLimit(*this), PreparationWorkLimit(*this), FPlatformTime::Seconds() - Start)) return;
		if (!S.Entries.ContainsByPredicate([&](const auto& E) { return E.Input.Id == Prop.StableId; }) && S.Entries.Num() < 8)
		{
			FPreparationTimer AdmissionTimer(S.AdmissionSeconds);
			auto& Entry = S.Entries.AddDefaulted_GetRef();
			Entry.Input = MoveTemp(Input);
			const uint64 Request = ++S.Serial;
			Entry.Job.Initialize({S.Generation, Request, Entry.Input.Epoch, Request, Request}, Entry.Input.Size.X * Entry.Input.Size.Y);
			++S.Requests;
		}
	}
}

void ADarkwellObjectMemoryScene::AdvanceWholePreparation()
{
	check(IsInGameThread());
	auto& S = *WholePreparation;
	S.BeginDiagnostics(PreparationFrame(*this));
	S.Budget.Begin(PreparationFrame(*this));
	const bool bAccountTail = FrameGuard.GetValueOnGameThread() != 0 && Mode.GetValueOnGameThread() > 0;
	const double TailStart = FPlatformTime::Seconds(), SpentBeforeTail = S.Budget.SpentSeconds;
	// Nested request/step/cancel scopes already charge their own work. Include
	// loop, lookup and temporary-container cleanup once, without double charging.
	auto ChargeTailOverhead = [&]() {
		if (bAccountTail) S.Budget.Charge(FMath::Max(0.0, FPlatformTime::Seconds() - TailStart
			- (S.Budget.SpentSeconds - SpentBeforeTail)), 0);
	};
	ON_SCOPE_EXIT { ChargeTailOverhead(); };
	if (FrameGuard.GetValueOnGameThread() != 0)
	{
		const int32 CurrentMode = Mode.GetValueOnGameThread();
		if (S.LastMode != CurrentMode) { InvalidateWholePreparation(); S.LastMode = CurrentMode; }
		S.Budget.Begin(PreparationFrame(*this));
		if (CurrentMode <= 0 || CurrentMode > 2) return;
		// All authoritative Current/evidence/resource work has finished. Never
		// add discretionary geometry to resource creation or >=1ms native work.
		// The frame latch survives repeated UpdateMemory and Reset in this frame.
		double ForegroundUs = RuntimeFrame.UpdateTrackedUs;
#if WITH_DEV_AUTOMATION_TESTS
		if (WholePreparationFrameForTesting != MAX_uint64) ForegroundUs = WholePreparationForegroundUsForTesting;
#endif
		if (RuntimeFrame.TextureCreations || RuntimeFrame.MidCreations || RuntimeFrame.CapMeshRebuilds
			|| ForegroundUs >= 1000.0)
		{
			if (S.SuppressedFrame != PreparationFrame(*this)) ++S.SuppressedFrames;
			S.SuppressedFrame = PreparationFrame(*this);
		}
		if (S.SuppressedFrame == PreparationFrame(*this)) return;
		for (auto& Pair : Tracked) { ChargeTailOverhead(); RequestWholePreparation(Pair.Value, true); }
	}
	if (Mode.GetValueOnGameThread() != S.LastMode || Mode.GetValueOnGameThread() == 0) { InvalidateWholePreparation(); return; }
	S.Budget.Begin(PreparationFrame(*this));
	int32 Hosts = 0;
	for (TActorIterator<ADarkwellObjectMemoryScene> It(GetWorld()); It; ++It) ++Hosts;
	if (Hosts != 1) { InvalidateWholePreparation(); return; }
	if (bHoldWholePreparationForTesting) return;
	TSet<int32> Validated;
	int32 Skipped = 0;
	while (!S.Entries.IsEmpty() && Skipped < S.Entries.Num())
	{
		ChargeTailOverhead();
		if (!S.Budget.CanAdvance(PreparationTimeLimit(*this), PreparationWorkLimit(*this))) break;
		const double Start = FPlatformTime::Seconds();
		int32 Work = 0;
		TGuardValue<bool> Charging(S.bCharging, true);
		ON_SCOPE_EXIT { S.Budget.Charge(FPlatformTime::Seconds() - Start, Work); };
		S.RoundRobin %= S.Entries.Num();
		auto& Entry = S.Entries[S.RoundRobin++];
		if (Entry.Job.bRevoked || Entry.Job.IsReady()) { ++Skipped; continue; }
		auto* Prop = Tracked.Find(Entry.Input.Id);
		FDarkwellWholePreparationState::FInput Current;
		if (!Validated.Contains(S.RoundRobin - 1) && (!Prop || !FDarkwellWholePreparationState::Snapshot(*this, *Prop, Current) || !Entry.Input.Matches(Current)))
		{
			Entry.Job.Revoke(); ++S.Cancelled; ++Skipped;
			continue;
		}
		Validated.Add(S.RoundRobin - 1);
		// Snapshot/matching is charged on scope exit; include it before starting
		// the next indivisible chunk. A soft budget cannot preempt an OS stall.
		if (FrameGuard.GetValueOnGameThread() != 0
			&& !S.Budget.CanAdvance(PreparationTimeLimit(*this), PreparationWorkLimit(*this), FPlatformTime::Seconds() - Start)) break;
		const auto& Input = Entry.Input;
		{ FPreparationTimer StepTimer(S.StepSeconds, &S.MaxStepSeconds);
		Work = Entry.Job.Step(PreparationFrame(*this), FMath::Min(128, PreparationWorkLimit(*this) - S.Budget.Work), [&](const bool bWhole, const int32 Index)
		{
			if (!bWhole) return PrepareFootprintCell(Input.Bounds, Input.Size, Index, Input.Capture);
			const FVector2D Step = Input.Bounds.GetSize() / FVector2D(Input.Size);
			const FVector2D Min = Input.Bounds.Min + Step * FVector2D(Index % Input.Size.X, Index / Input.Size.X);
			for (const auto& Part : Input.Parts)
				if (FDarkwellCurrentLiveGrid::IntersectsWholeCell(Part.Bounds, Part.Pose, FBox2D(Min, Min + Step))) return true;
			return false;
		});
		}
		if (Work == 0) ++Skipped; else Skipped = 0;
	}
}

bool ADarkwellObjectMemoryScene::TakeWholePreparation(FTrackedProp& Prop, TBitArray<>& Whole, TBitArray<>& Footprint)
{
	check(IsInGameThread());
	auto& S = *WholePreparation;
	S.BeginDiagnostics(PreparationFrame(*this));
	FPreparationTimer TakeTimer(S.TakeSeconds);
	S.LastSealStartSeconds = FPlatformTime::Seconds();
	const int32 Index = S.Entries.IndexOfByPredicate([&](const auto& E) { return E.Input.Id == Prop.StableId; });
	if (Index == INDEX_NONE) { ++S.Fallbacks; return false; }
	S.Budget.Begin(PreparationFrame(*this));
	const double Start = FPlatformTime::Seconds();
	TGuardValue<bool> Charging(S.bCharging, true);
	ON_SCOPE_EXIT { S.Budget.Charge(FPlatformTime::Seconds() - Start, 0); };
	auto Entry = MoveTemp(S.Entries[Index]);
	S.Entries.RemoveAt(Index);
	FDarkwellWholePreparationState::FInput Current;
	const int32 CurrentMode = Mode.GetValueOnGameThread();
	const bool bValid = CurrentMode == S.LastMode && (CurrentMode == 1 || CurrentMode == 2)
		&& FDarkwellWholePreparationState::Snapshot(*this, Prop, Current) && Entry.Input.Matches(Current)
		&& Entry.Job.Take(Entry.Job.Ticket, Whole, Footprint);
	bool bUse = bValid && CurrentMode == 2;
	if (bValid && CurrentMode == 1)
	{
		TBitArray<> Oracle;
		Prop.CurrentLive.BuildFullGeometryMask(Current.Bounds, Current.Size, Oracle);
		const auto ExpectedFootprint = BuildCaptureGeometryFootprint(Current.Bounds, Current.Size, Current.Capture, false);
		ensureAlwaysMsgf(Oracle == Whole && ExpectedFootprint == Footprint, TEXT("FirstWholeGeometry shadow parity"));
		++S.Shadow;
	}
	if (bUse) ++S.Hits; else ++S.Fallbacks;
	return bUse;
}

FString ADarkwellObjectMemoryScene::GetWholePreparationTelemetry() const
{
 const auto& S = *WholePreparation;
 int32 Ready = 0, Revoked = 0; SIZE_T Bytes = 0;
 for (const auto& E : S.Entries) {
  Ready += E.Job.IsReady(); Revoked += E.Job.bRevoked;
  Bytes += E.Input.Parts.GetAllocatedSize() + E.Input.Capture.GetAllocatedSize()
   + E.Job.Whole.GetAllocatedSize() + E.Job.Footprint.GetAllocatedSize();
 }
 FString Result = FString::Printf(TEXT("{\"requests\":%llu,\"hits\":%llu,\"fallbacks\":%llu,\"shadow\":%llu,\"cancelled\":%llu,\"pending\":%d,\"ready\":%d,\"rejected\":%d,\"bytes\":%llu,\"frame_work\":%d,\"frame_ms\":%.6f,\"seal_age_ms\":%.6f,\"request_ms\":%.6f,\"snapshot_ms\":%.6f,\"admission_ms\":%.6f,\"step_ms\":%.6f,\"max_step_ms\":%.6f,\"cancel_ms\":%.6f,\"take_ms\":%.6f,\"current_geometry_ms\":%.6f,\"current_advance_ms\":%.6f,\"current_visual_ms\":%.6f,\"current_textures_ms\":%.6f,\"current_cap_ms\":%.6f,\"history_texture_ms\":%.6f,\"proxy_create_ms\":%.6f,\"proxy_bind_ms\":%.6f}"),
  S.Requests,S.Hits,S.Fallbacks,S.Shadow,S.Cancelled,S.Entries.Num(),Ready,Revoked,uint64(Bytes),S.Budget.Work,S.Budget.SpentSeconds*1000, S.LastSealStartSeconds > 0 ? (FPlatformTime::Seconds()-S.LastSealStartSeconds)*1000 : -1, S.RequestSeconds*1000,S.SnapshotSeconds*1000,S.AdmissionSeconds*1000,S.StepSeconds*1000,S.MaxStepSeconds*1000,S.CancelSeconds*1000,S.TakeSeconds*1000,RuntimeFrame.CurrentGeometryUs/1000,RuntimeFrame.CurrentAdvanceUs/1000,RuntimeFrame.CurrentVisualUs/1000,RuntimeFrame.CurrentTexturesUs/1000,RuntimeFrame.CurrentCapUs/1000,RuntimeFrame.HistoryTextureCreateUs/1000,RuntimeFrame.ProxyCreateUs/1000,RuntimeFrame.ProxyBindUs/1000);
 return Result.LeftChop(1) + FString::Printf(TEXT(",\"suppressed_frames\":%llu,\"suppressed\":%d,\"material_load_ms\":%.6f,\"mid_create_ms\":%.6f,\"proxy_register_ms\":%.6f}"), S.SuppressedFrames, S.SuppressedFrame == PreparationFrame(*this) ? 1 : 0, RuntimeFrame.ProxyMaterialLoadUs/1000, RuntimeFrame.ProxyMidUs/1000, RuntimeFrame.ProxyRegisterUs/1000);
}

void ADarkwellObjectMemoryScene::SetWholePreparationDiagnosticForTesting(int32 Action)
{
 check(IsInGameThread());
 bHoldWholePreparationForTesting = Action == 1;
 if (Action == 2) InvalidateWholePreparation();
}
