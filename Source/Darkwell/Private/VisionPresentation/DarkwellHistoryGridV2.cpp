#include "VisionPresentation/DarkwellHistoryGridV2.h"
#include "NativeGameplayTags.h"

namespace Darkwell::HistoryGridV2
{
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Never, "Lab.HistoryEvidence.NeverObserved");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Unresolved, "Lab.HistoryEvidence.Unresolved");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Empty, "Lab.HistoryEvidence.VerifiedEmpty");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Superseded, "Lab.HistoryEvidence.SupersededByNewerEvidence");
}
FGameplayTag FDarkwellHistoryGridV2::NeverObserved() { return Darkwell::HistoryGridV2::Never; }
FGameplayTag FDarkwellHistoryGridV2::Unresolved() { return Darkwell::HistoryGridV2::Unresolved; }
FGameplayTag FDarkwellHistoryGridV2::VerifiedEmpty() { return Darkwell::HistoryGridV2::Empty; }
FGameplayTag FDarkwellHistoryGridV2::Superseded() { return Darkwell::HistoryGridV2::Superseded; }

void FDarkwellHistoryGridV2::Initialize(const FDarkwellSpatialPropMemory& SealedMemory)
{
	check(SealedMemory.IsAbsent());
	Bounds = SealedMemory.GetBounds();
	const FIntPoint Coarse = SealedMemory.GetSize();
	Size = Coarse * SamplesPerCell;
	Samples.SetNum(Size.X * Size.Y);
	ActiveSamples.Reset();
	ActiveFlags.Init(false, Samples.Num());
	MutableEvidence.Init(true, Samples.Num());
	TArray<FLinearColor> FrozenPresentation;
	SealedMemory.BuildConservativePresentation(SamplesPerCell, FrozenPresentation);
	for (int32 Y = 0; Y < Size.Y; ++Y) for (int32 X = 0; X < Size.X; ++X)
	{
		const auto& Cell = SealedMemory.GetCells()[(Y / SamplesPerCell) * Coarse.X + X / SamplesPerCell];
		FSample& Sample = Samples[Y * Size.X + X];
		Sample = FSample();
		Sample.InitialRemembered = Cell.InitialRemembered;
		Sample.Opacity = Cell.StaleOpacity;
		Sample.FrozenAAEnvelope = Cell.StaleOpacity > 0 ? FrozenPresentation[Y * Size.X + X].B / Cell.StaleOpacity : 0;
		Sample.State = Sample.InitialRemembered > 0 ? Unresolved() : NeverObserved();
	}
}

void FDarkwellHistoryGridV2::Initialize(const FDarkwellSpatialPropMemory& SealedMemory, const TBitArray<>& CaptureMask)
{
 Initialize(SealedMemory);
 check(CaptureMask.Num()==Samples.Num());
 for(int32 I=0;I<Samples.Num();++I)
 {
  auto& Sample=Samples[I];
  Sample.InitialRemembered=Sample.Opacity=CaptureMask[I]?1.f:0.f;
  Sample.State=CaptureMask[I]?Unresolved():NeverObserved();
 }
}

void FDarkwellHistoryGridV2::RestrictToRecordedGeometry(const TBitArray<>& Footprint)
{
	check(Footprint.Num() == Samples.Num());
	for (int32 I = 0; I < Samples.Num(); ++I) if (!Footprint[I])
	{
		Samples[I].InitialRemembered = Samples[I].Opacity = 0;
		Samples[I].State = NeverObserved();
		MutableEvidence[I] = true;
	}
}

bool FDarkwellHistoryGridV2::Advance(float DeltaSeconds, TConstArrayView<float> Coverage,
	const TBitArray<>& Occupied, const TBitArray<>& Ownership)
{
	TArray<int32> Dirty;
	Dirty.Reserve(Samples.Num());
	for (int32 Index = 0; Index < Samples.Num(); ++Index)
	{
		Dirty.Add(Index);
	}
	bool bTopologyChanged = false;
	return AdvanceDirty(DeltaSeconds, Coverage, Occupied, Ownership, Dirty, bTopologyChanged);
}

bool FDarkwellHistoryGridV2::AdvanceDirty(float DeltaSeconds,
	TConstArrayView<float> Coverage, const TBitArray<>& Occupied,
	const TBitArray<>& Ownership, TConstArrayView<int32> DirtyIndices,
	bool& bOutTopologyChanged)
{
	bOutTopologyChanged = false;
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0 || Coverage.Num() != Samples.Num()
		|| Occupied.Num() != Samples.Num() || Ownership.Num() != Samples.Num()) return false;
	if (ActiveFlags.Num() != Samples.Num()) ActiveFlags.Init(false, Samples.Num());
	auto Activate = [&](const int32 Index)
	{
		if (!ActiveFlags[Index])
		{
			ActiveFlags[Index] = true;
			ActiveSamples.Add(Index);
		}
	};
	for (const int32 Index : DirtyIndices)
	{
		if (!Samples.IsValidIndex(Index)) continue;
		FSample& S = Samples[Index];
		// Superseded is terminal in the existing state machine. Its evidence and
		// zero dwell cannot change again; avoid rewriting every retained epoch.
		if (S.State == Superseded()) continue;
		if (Ownership[Index])
		{
			if (S.State != Superseded()) bOutTopologyChanged = true;
			S.State = Superseded();
			MutableEvidence[Index] = false;
			S.EmptyDwell = 0.0f;
			continue;
		}
		const bool bLegalEmpty = !Occupied[Index] && FMath::IsFinite(Coverage[Index])
			&& Coverage[Index] >= FDarkwellSpatialPropMemory::LegalCoverage;
		if (!S.bVerifiedEmpty)
		{
			if (bLegalEmpty)
			{
				// A revision-matched conservative footprint is a spatial proof,
				// not an exposure-duration vote. Record the fact before fading.
				// Leaving the view cannot revoke knowledge already obtained.
				S.EmptyDwell += FMath::Min(DeltaSeconds, 1.f / 30.f);
				S.bVerifiedEmpty = true;
				S.State = VerifiedEmpty();
				bOutTopologyChanged = true;
				if (S.Opacity > 0.0f) Activate(Index);
			}
		}
		else if (S.Opacity > 0.0f) Activate(Index);
	}

	const float Dt = FMath::Min(DeltaSeconds, .20f);
	bool bPresentationChanged = false;
	for (int32 ActiveIndex = ActiveSamples.Num() - 1; ActiveIndex >= 0; --ActiveIndex)
	{
		const int32 Index = ActiveSamples[ActiveIndex];
		if (!ActiveFlags.IsValidIndex(Index) || !ActiveFlags[Index])
		{
			ActiveSamples.RemoveAtSwap(ActiveIndex, 1, EAllowShrinking::No);
			continue;
		}
		FSample& S = Samples[Index];
		if (Ownership[Index])
		{
			if (S.State != Superseded()) bOutTopologyChanged = true;
			S.State = Superseded();
			MutableEvidence[Index] = false;
			S.EmptyDwell = 0.0f;
			ActiveFlags[Index] = false;
			ActiveSamples.RemoveAtSwap(ActiveIndex, 1, EAllowShrinking::No);
			bPresentationChanged = true;
			continue;
		}
		if (S.bVerifiedEmpty)
		{
			const float PreviousOpacity = S.Opacity;
			S.Opacity = FMath::Max(0.f, S.Opacity - Dt / FDarkwellSpatialPropMemory::EmptyFadeSeconds);
			bPresentationChanged |= S.Opacity != PreviousOpacity;
			if (S.Opacity <= 0.0f)
			{
				ActiveFlags[Index] = false;
				ActiveSamples.RemoveAtSwap(ActiveIndex, 1, EAllowShrinking::No);
			}
		}
	}
	return bPresentationChanged || bOutTopologyChanged;
}

void FDarkwellHistoryGridV2::FilterMutableEvidence(TBitArray<>& CandidateIndices) const
{
 check(CandidateIndices.Num()==MutableEvidence.Num());
 CandidateIndices.CombineWithBitwiseAND(MutableEvidence,EBitwiseOperatorFlags::MinSize);
}

bool FDarkwellHistoryGridV2::HasResidualSurface() const
{
	return Samples.ContainsByPredicate([](const FSample& S)
	{
		return (S.State == Unresolved() && S.InitialRemembered > 0)
			|| (S.State == VerifiedEmpty() && S.Opacity > 0);
	});
}
void FDarkwellHistoryGridV2::BuildPresentation(TArray<FLinearColor>& OutPixels) const
{
	OutPixels.SetNumUninitialized(Samples.Num());
	for (int32 I = 0; I < Samples.Num(); ++I)
	{
		const FSample& S = Samples[I];
		// Keep the frozen 4x4 envelope and bilinear RGB. Binary A is loaded
		// unfiltered by M_MovingAccumulatedMemory at the FINAL shader output.
		const bool Gate = S.State == Unresolved() || (S.State == VerifiedEmpty() && S.Opacity > 0);
		OutPixels[I] = FLinearColor(0, 0, S.Opacity * S.FrozenAAEnvelope, Gate ? 1.f : 0.f);
	}
}
bool FDarkwellHistoryGridV2::IsFullyVerifiedEmpty() const
{
	return IsInitialized() && !Samples.ContainsByPredicate([](const FSample& S)
	{
		return S.InitialRemembered > 0 && (!S.bVerifiedEmpty || S.Opacity > 0);
	});
}
int32 FDarkwellHistoryGridV2::Count(FGameplayTag State) const
{
	int32 Result = 0;
	for (const FSample& S : Samples) Result += S.State == State;
	return Result;
}
bool FDarkwellHistoryGridV2::CanEmitCap(int32 RetainedIndex, int32 NeighborIndex) const
{
	if (!Samples.IsValidIndex(RetainedIndex) || !Samples.IsValidIndex(NeighborIndex)) return false;
	const auto& Source = Samples[RetainedIndex];
	const auto& Neighbor = Samples[NeighborIndex];
	return Source.State == Unresolved() && Source.InitialRemembered > 0
		&& (Neighbor.State == NeverObserved() || Neighbor.State == VerifiedEmpty());
}
int32 FDarkwellHistoryGridV2::CountMixedCoarseCells() const
{
	int32 Result = 0;
	for (int32 Y = 0; Y < Size.Y; Y += SamplesPerCell) for (int32 X = 0; X < Size.X; X += SamplesPerCell)
	{
		bool Empty = false, Owned = false, Unknown = false;
		for (int32 SY = 0; SY < SamplesPerCell; ++SY) for (int32 SX = 0; SX < SamplesPerCell; ++SX)
		{
			const auto State = Samples[(Y + SY) * Size.X + X + SX].State;
			Empty |= State == VerifiedEmpty(); Owned |= State == Superseded(); Unknown |= State == Unresolved();
		}
		Result += Empty && Owned && Unknown;
	}
	return Result;
}
uint64 FDarkwellHistoryGridV2::EvidenceHash() const
{
	uint64 Hash = 1469598103934665603ull;
	for (const FSample& S : Samples)
	{
		const uint64 State = S.State == NeverObserved() ? 0 : S.State == Unresolved() ? 1
			: S.State == VerifiedEmpty() ? 2 : 3;
		const uint64 Bits = State | (uint64(S.bVerifiedEmpty) << 2)
			| (uint64(S.InitialRemembered > 0) << 3)
			| (uint64(FMath::RoundToInt(S.Opacity * 255)) << 8)
			| (uint64(FMath::RoundToInt(S.FrozenAAEnvelope * 255)) << 16);
		Hash = (Hash ^ Bits) * 1099511628211ull;
	}
	return Hash;
}

uint64 FDarkwellHistoryGridV2::StateHash() const
{
	uint64 Hash = 1469598103934665603ull;
	for (const FSample& S : Samples)
	{
		const uint64 State = S.State == NeverObserved() ? 0 : S.State == Unresolved() ? 1
			: S.State == VerifiedEmpty() ? 2 : 3;
		Hash = (Hash ^ State) * 1099511628211ull;
	}
	return Hash;
}
