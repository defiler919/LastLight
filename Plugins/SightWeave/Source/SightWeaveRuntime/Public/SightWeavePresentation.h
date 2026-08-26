#pragma once

#include "CoreMinimal.h"
#include "SightWeaveSparseAtlas.h"

enum class ESightWeavePresentationBindingFailure : uint8
{
	None,
	Disabled,
	InvalidSelection,
	InvalidPacket,
	WorldMismatch,
	ScopeMissing,
	ScopeUnavailable,
	RevisionMismatch,
	ResourceGenerationMismatch,
	ResidencyGenerationMismatch,
	ResidencyIncomplete,
	WorldTeardown
};

/** Immutable GT selection. Enabled selections fail black until a matching RT binding is complete. */
class SIGHTWEAVERUNTIME_API FSightWeaveViewPresentationSelection final
{
public:
	static FSightWeaveViewPresentationSelection Disabled(
		FSightWeaveRenderWorldIdentity WorldIdentity,
		uint64 PresentationRevision);
	static FSightWeaveViewPresentationSelection Enabled(
		FSightWeaveRenderWorldIdentity WorldIdentity,
		FSightWeaveKnowledgeOwnerId KnowledgeOwnerId,
		FSightWeaveFloorId FloorId,
		ESightWeaveRenderPrecisionTier PrecisionTier,
		uint64 PresentationRevision);

	bool IsEnabled() const { return bEnabled; }
	bool IsValid() const;
	FSightWeaveRenderWorldIdentity GetWorldIdentity() const { return WorldIdentity; }
	FSightWeaveKnowledgeOwnerId GetKnowledgeOwnerId() const { return KnowledgeOwnerId; }
	FSightWeaveFloorId GetFloorId() const { return FloorId; }
	ESightWeaveRenderPrecisionTier GetPrecisionTier() const { return PrecisionTier; }
	uint64 GetPresentationRevision() const { return PresentationRevision; }
	bool IsEquivalentTo(const FSightWeaveViewPresentationSelection& Other) const;

private:
	FSightWeaveRenderWorldIdentity WorldIdentity;
	FSightWeaveKnowledgeOwnerId KnowledgeOwnerId;
	FSightWeaveFloorId FloorId;
	ESightWeaveRenderPrecisionTier PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
	uint64 PresentationRevision = 0;
	bool bEnabled = false;
};

/**
 * Immutable RT presentation binding for one already-unioned EffectiveLiveMask scope.
 * Full canonical profile equality is authoritative; StableHash never defines identity.
 */
class SIGHTWEAVERUNTIME_API FSightWeaveViewPresentationBinding final
{
public:
	bool IsValid() const { return bValid; }
	ESightWeavePresentationBindingFailure GetFailure() const { return Failure; }
	FSightWeaveRenderWorldIdentity GetWorldIdentity() const { return ScopeKey.WorldIdentity; }
	const FSightWeaveSparseScopeKey& GetScopeKey() const { return ScopeKey; }
	TConstArrayView<FSightWeaveRenderProfileIdentity> GetCanonicalProfiles() const
	{
		return CanonicalProfiles;
	}
	uint64 GetResourceGeneration() const { return ResourceGeneration; }
	uint64 GetResidencyGeneration() const { return ResidencyGeneration; }
	uint64 GetPacketRevision() const { return PacketRevision; }
	uint64 GetRegistryRevision() const { return RegistryRevision; }
	uint64 GetPublishedSnapshotRevision() const { return PublishedSnapshotRevision; }
	uint64 GetPresentationRevision() const { return PresentationRevision; }
	bool IsEffectiveUnionScope() const { return bEffectiveUnionScope; }
	bool IsEquivalentTo(const FSightWeaveViewPresentationBinding& Other) const;

private:
	friend class FSightWeavePresentationBindingBuilder;
	bool bValid = false;
	bool bEffectiveUnionScope = true;
	ESightWeavePresentationBindingFailure Failure =
		ESightWeavePresentationBindingFailure::InvalidSelection;
	FSightWeaveSparseScopeKey ScopeKey;
	TArray<FSightWeaveRenderProfileIdentity> CanonicalProfiles;
	uint64 ResourceGeneration = 0;
	uint64 ResidencyGeneration = 0;
	uint64 PacketRevision = 0;
	uint64 RegistryRevision = 0;
	uint64 PublishedSnapshotRevision = 0;
	uint64 PresentationRevision = 0;
};

struct SIGHTWEAVERUNTIME_API FSightWeavePresentationBindingBuildResult
{
	TSharedPtr<const FSightWeaveViewPresentationBinding, ESPMode::ThreadSafe> Binding;
	ESightWeavePresentationBindingFailure Failure =
		ESightWeavePresentationBindingFailure::InvalidSelection;

	bool Succeeded() const { return Binding.IsValid() && Binding->IsValid(); }
};

class SIGHTWEAVERUNTIME_API FSightWeavePresentationBindingBuilder final
{
public:
	static FSightWeavePresentationBindingBuildResult Build(
		const FSightWeaveSparseRenderPacket& Packet,
		const FSightWeaveViewPresentationSelection& Selection,
		uint64 ResourceGeneration,
		uint64 ResidencyGeneration);
	static ESightWeavePresentationBindingFailure Validate(
		const FSightWeaveViewPresentationBinding& Binding);
};
