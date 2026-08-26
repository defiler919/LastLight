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
	FeatherResourceGenerationMismatch,
	FeatherRevisionMismatch,
	FeatherUnavailable,
	WorldTeardown
};

namespace SightWeave::VisualFeather
{
	inline constexpr float MaximumWidthCentimeters = 100.0f;
	inline constexpr float DevelopmentDefaultWidthCentimeters = 50.0f;
	inline constexpr int32 MaximumRadiusTexels = 40;
	inline constexpr int32 TransformWorkSize = SightWeave::SparseAtlas::InteriorTileSize
		+ MaximumRadiusTexels * 2;
}

/** Presentation-only setting. It never participates in CPU visibility authority. */
struct SIGHTWEAVERUNTIME_API FSightWeaveVisualFeatherSettings
{
	float WidthCentimeters = 0.0f;

	bool IsValid() const
	{
		return FMath::IsFinite(WidthCentimeters)
			&& WidthCentimeters >= 0.0f
			&& WidthCentimeters <= SightWeave::VisualFeather::MaximumWidthCentimeters;
	}
	bool IsEnabled() const { return IsValid() && WidthCentimeters > 0.0f; }
	bool IsEquivalentTo(const FSightWeaveVisualFeatherSettings& Other) const
	{
		return WidthCentimeters == Other.WidthCentimeters;
	}
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
		uint64 PresentationRevision,
		FSightWeaveVisualFeatherSettings VisualFeather = FSightWeaveVisualFeatherSettings());

	bool IsEnabled() const { return bEnabled; }
	bool IsValid() const;
	FSightWeaveRenderWorldIdentity GetWorldIdentity() const { return WorldIdentity; }
	FSightWeaveKnowledgeOwnerId GetKnowledgeOwnerId() const { return KnowledgeOwnerId; }
	FSightWeaveFloorId GetFloorId() const { return FloorId; }
	ESightWeaveRenderPrecisionTier GetPrecisionTier() const { return PrecisionTier; }
	uint64 GetPresentationRevision() const { return PresentationRevision; }
	const FSightWeaveVisualFeatherSettings& GetVisualFeather() const { return VisualFeather; }
	bool IsEquivalentTo(const FSightWeaveViewPresentationSelection& Other) const;

private:
	FSightWeaveRenderWorldIdentity WorldIdentity;
	FSightWeaveKnowledgeOwnerId KnowledgeOwnerId;
	FSightWeaveFloorId FloorId;
	ESightWeaveRenderPrecisionTier PrecisionTier = ESightWeaveRenderPrecisionTier::Standard;
	uint64 PresentationRevision = 0;
	FSightWeaveVisualFeatherSettings VisualFeather;
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
	const FSightWeaveVisualFeatherSettings& GetVisualFeather() const { return VisualFeather; }
	uint64 GetFeatherResourceGeneration() const { return FeatherResourceGeneration; }
	uint64 GetFeatherAppliedRevision() const { return FeatherAppliedRevision; }
	uint64 GetFeatherSettingsRevision() const { return FeatherSettingsRevision; }
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
	FSightWeaveVisualFeatherSettings VisualFeather;
	uint64 FeatherResourceGeneration = 0;
	uint64 FeatherAppliedRevision = 0;
	uint64 FeatherSettingsRevision = 0;
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
		uint64 ResidencyGeneration,
		uint64 FeatherResourceGeneration = 0,
		uint64 FeatherAppliedRevision = 0,
		uint64 FeatherSettingsRevision = 0);
	static ESightWeavePresentationBindingFailure Validate(
		const FSightWeaveViewPresentationBinding& Binding);
};

struct SIGHTWEAVERUNTIME_API FSightWeavePresentationAtlasLookup
{
	FIntPoint LogicalCoordinate = FIntPoint::ZeroValue;
	FIntPoint InteriorTexel = FIntPoint::ZeroValue;
	FIntPoint AtlasTexel = FIntPoint::ZeroValue;
	bool bValid = false;
};

/** CPU reference for the exact floor-relative mapping implemented by the hard-mask shader. */
class SIGHTWEAVERUNTIME_API FSightWeavePresentationMapping final
{
public:
	static FSightWeavePresentationAtlasLookup MapWorldPosition(
		const FSightWeaveSparseScopeKey& ScopeKey,
		const FSightWeaveSparsePhysicalAddress& Address,
		const FVector2D& WorldPosition);
};
