#include "SightWeavePresentation.h"

namespace SightWeavePresentationPrivate
{
	bool SightWeavePresentationProfileLess(
		const FSightWeaveRenderProfileIdentity& A,
		const FSightWeaveRenderProfileIdentity& B)
	{
		const int32 CommonCount = FMath::Min(
			A.CanonicalCapabilities.Num(),
			B.CanonicalCapabilities.Num());
		for (int32 Index = 0; Index < CommonCount; ++Index)
		{
			if (A.CanonicalCapabilities[Index] == B.CanonicalCapabilities[Index])
			{
				continue;
			}
			return A.CanonicalCapabilities[Index].LexicalLess(B.CanonicalCapabilities[Index]);
		}
		return A.CanonicalCapabilities.Num() < B.CanonicalCapabilities.Num();
	}

	bool ProfilesEqual(
		TConstArrayView<FSightWeaveRenderProfileIdentity> A,
		TConstArrayView<FSightWeaveRenderProfileIdentity> B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (!A[Index].IsEquivalentTo(B[Index]))
			{
				return false;
			}
		}
		return true;
	}
}

using namespace SightWeavePresentationPrivate;

FSightWeaveViewPresentationSelection FSightWeaveViewPresentationSelection::Disabled(
	const FSightWeaveRenderWorldIdentity InWorldIdentity,
	const uint64 InPresentationRevision)
{
	FSightWeaveViewPresentationSelection Result;
	Result.WorldIdentity = InWorldIdentity;
	Result.PresentationRevision = InPresentationRevision;
	return Result;
}

FSightWeaveViewPresentationSelection FSightWeaveViewPresentationSelection::Enabled(
	const FSightWeaveRenderWorldIdentity InWorldIdentity,
	const FSightWeaveKnowledgeOwnerId InKnowledgeOwnerId,
	const FSightWeaveFloorId InFloorId,
	const ESightWeaveRenderPrecisionTier InPrecisionTier,
	const uint64 InPresentationRevision)
{
	FSightWeaveViewPresentationSelection Result;
	Result.WorldIdentity = InWorldIdentity;
	Result.KnowledgeOwnerId = InKnowledgeOwnerId;
	Result.FloorId = InFloorId;
	Result.PrecisionTier = InPrecisionTier;
	Result.PresentationRevision = InPresentationRevision;
	Result.bEnabled = true;
	return Result;
}

bool FSightWeaveViewPresentationSelection::IsValid() const
{
	return WorldIdentity.IsValid()
		&& PresentationRevision != 0
		&& (!bEnabled
			|| (KnowledgeOwnerId.IsValid()
				&& FloorId.IsValid()
				&& SightWeaveCentimetersPerTexel(PrecisionTier) > 0.0f));
}

bool FSightWeaveViewPresentationSelection::IsEquivalentTo(
	const FSightWeaveViewPresentationSelection& Other) const
{
	return bEnabled == Other.bEnabled
		&& WorldIdentity == Other.WorldIdentity
		&& KnowledgeOwnerId == Other.KnowledgeOwnerId
		&& FloorId == Other.FloorId
		&& PrecisionTier == Other.PrecisionTier
		&& PresentationRevision == Other.PresentationRevision;
}

bool FSightWeaveViewPresentationBinding::IsEquivalentTo(
	const FSightWeaveViewPresentationBinding& Other) const
{
	return bValid == Other.bValid
		&& bEffectiveUnionScope == Other.bEffectiveUnionScope
		&& Failure == Other.Failure
		&& ScopeKey.IsEquivalentTo(Other.ScopeKey)
		&& ProfilesEqual(CanonicalProfiles, Other.CanonicalProfiles)
		&& ResourceGeneration == Other.ResourceGeneration
		&& ResidencyGeneration == Other.ResidencyGeneration
		&& PacketRevision == Other.PacketRevision
		&& RegistryRevision == Other.RegistryRevision
		&& PublishedSnapshotRevision == Other.PublishedSnapshotRevision
		&& PresentationRevision == Other.PresentationRevision;
}

FSightWeavePresentationBindingBuildResult FSightWeavePresentationBindingBuilder::Build(
	const FSightWeaveSparseRenderPacket& Packet,
	const FSightWeaveViewPresentationSelection& Selection,
	const uint64 ResourceGeneration,
	const uint64 ResidencyGeneration)
{
	FSightWeavePresentationBindingBuildResult Result;
	TSharedRef<FSightWeaveViewPresentationBinding, ESPMode::ThreadSafe> Binding =
		MakeShared<FSightWeaveViewPresentationBinding, ESPMode::ThreadSafe>();
	Result.Binding = Binding;

	auto Fail = [&Result, &Binding](const ESightWeavePresentationBindingFailure Failure)
	{
		Binding->bValid = false;
		Binding->Failure = Failure;
		Result.Failure = Failure;
		return Result;
	};

	if (!Selection.IsValid())
	{
		return Fail(ESightWeavePresentationBindingFailure::InvalidSelection);
	}
	if (!Selection.IsEnabled())
	{
		return Fail(ESightWeavePresentationBindingFailure::Disabled);
	}
	if (!Packet.IsValid()
		|| FSightWeaveSparseRenderPacketBuilder::Validate(Packet)
			!= ESightWeaveSparsePacketFailure::None)
	{
		return Fail(ESightWeavePresentationBindingFailure::InvalidPacket);
	}
	if (Packet.GetWorldIdentity() != Selection.GetWorldIdentity())
	{
		return Fail(ESightWeavePresentationBindingFailure::WorldMismatch);
	}
	if (ResourceGeneration == 0)
	{
		return Fail(ESightWeavePresentationBindingFailure::ResourceGenerationMismatch);
	}
	if (ResidencyGeneration == 0)
	{
		return Fail(ESightWeavePresentationBindingFailure::ResidencyGenerationMismatch);
	}

	const FSightWeaveSparseRenderScope* Scope = Packet.GetScopes().FindByPredicate(
		[&Selection](const FSightWeaveSparseRenderScope& Candidate)
		{
			return Candidate.ScopeKey.WorldIdentity == Selection.GetWorldIdentity()
				&& Candidate.ScopeKey.KnowledgeOwnerId == Selection.GetKnowledgeOwnerId()
				&& Candidate.ScopeKey.FloorId == Selection.GetFloorId()
				&& Candidate.ScopeKey.PrecisionTier == Selection.GetPrecisionTier();
		});
	if (!Scope)
	{
		return Fail(ESightWeavePresentationBindingFailure::ScopeMissing);
	}
	if (!Scope->IsValid())
	{
		return Fail(ESightWeavePresentationBindingFailure::ScopeUnavailable);
	}

	Binding->ScopeKey = Scope->ScopeKey;
	Binding->ResourceGeneration = ResourceGeneration;
	Binding->ResidencyGeneration = ResidencyGeneration;
	Binding->PacketRevision = Packet.GetPacketRevision();
	Binding->RegistryRevision = Packet.GetRegistryRevision();
	Binding->PublishedSnapshotRevision = Packet.GetPublishedSnapshotRevision();
	Binding->PresentationRevision = Selection.GetPresentationRevision();
	for (const FSightWeaveSparseRenderTile& Tile : Packet.GetTiles())
	{
		if (!Tile.Identity.TileKey.Scope.IsEquivalentTo(Scope->ScopeKey))
		{
			continue;
		}
		for (const FSightWeaveRenderProfileIdentity& Profile : Tile.Identity.CanonicalProfiles)
		{
			if (!Binding->CanonicalProfiles.ContainsByPredicate(
				[&Profile](const FSightWeaveRenderProfileIdentity& Existing)
				{
					return Existing.IsEquivalentTo(Profile);
				}))
			{
				Binding->CanonicalProfiles.Add(Profile);
			}
		}
	}
	Binding->CanonicalProfiles.Sort(SightWeavePresentationProfileLess);
	Binding->bValid = true;
	Binding->Failure = ESightWeavePresentationBindingFailure::None;
	const ESightWeavePresentationBindingFailure Validation = Validate(*Binding);
	if (Validation != ESightWeavePresentationBindingFailure::None)
	{
		return Fail(Validation);
	}
	Result.Failure = ESightWeavePresentationBindingFailure::None;
	return Result;
}

ESightWeavePresentationBindingFailure FSightWeavePresentationBindingBuilder::Validate(
	const FSightWeaveViewPresentationBinding& Binding)
{
	if (!Binding.bValid || !Binding.ScopeKey.IsValid())
	{
		return Binding.Failure == ESightWeavePresentationBindingFailure::None
			? ESightWeavePresentationBindingFailure::InvalidSelection
			: Binding.Failure;
	}
	if (!Binding.bEffectiveUnionScope)
	{
		return ESightWeavePresentationBindingFailure::ScopeUnavailable;
	}
	if (Binding.ResourceGeneration == 0)
	{
		return ESightWeavePresentationBindingFailure::ResourceGenerationMismatch;
	}
	if (Binding.ResidencyGeneration == 0)
	{
		return ESightWeavePresentationBindingFailure::ResidencyGenerationMismatch;
	}
	if (Binding.PacketRevision == 0
		|| Binding.RegistryRevision == 0
		|| Binding.PublishedSnapshotRevision == 0
		|| Binding.PresentationRevision == 0)
	{
		return ESightWeavePresentationBindingFailure::RevisionMismatch;
	}
	for (const FSightWeaveRenderProfileIdentity& Profile : Binding.CanonicalProfiles)
	{
		if (!Profile.IsValid())
		{
			return ESightWeavePresentationBindingFailure::ScopeUnavailable;
		}
	}
	return ESightWeavePresentationBindingFailure::None;
}
