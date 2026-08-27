#pragma once

#include "CoreMinimal.h"
#include "SightWeaveMemory.h"

#include "SightWeaveStaticEnvironment.generated.h"

namespace SightWeave::StaticEnvironment
{
	inline constexpr int32 BytesPerTile =
		SightWeave::Memory::InteriorTileSize * SightWeave::Memory::InteriorTileSize;
	inline constexpr int32 DefaultMaximumTiles =
		SightWeave::SparseAtlas::StandardActiveTileCapacity;
}

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveStaticEnvironmentHandle
{
	GENERATED_BODY()

	FSightWeaveStaticEnvironmentHandle() = default;
	explicit FSightWeaveStaticEnvironmentHandle(const int64 InValue) : Value(InValue) {}

	bool IsValid() const { return Value > 0; }
	int64 GetValue() const { return Value; }
	friend bool operator==(const FSightWeaveStaticEnvironmentHandle& A,
		const FSightWeaveStaticEnvironmentHandle& B)
	{
		return A.Value == B.Value;
	}

private:
	UPROPERTY(VisibleAnywhere, Category = "SightWeave")
	int64 Value = 0;
};

/** Explicit immutable 2.5D footprint; eligibility is never inferred from an Actor. */
struct SIGHTWEAVERUNTIME_API FSightWeaveStaticEnvironmentDescription
{
	FSightWeaveKnowledgeOwnerId KnowledgeOwnerId;
	FSightWeaveFloorId FloorId;
	FSightWeaveHeightRange HeightRange;
	TArray<FVector2D> WorldFootprint;
	uint8 NeutralIntensity = 112;
	bool bExplicitlyImmutable = false;
	bool bEnabled = true;

	bool IsValid() const;
	bool IsEquivalentTo(const FSightWeaveStaticEnvironmentDescription& Other) const;
};

struct SIGHTWEAVERUNTIME_API FSightWeaveStaticEnvironmentTile
{
	FSightWeaveMemoryTileKey Key;
	TArray<uint8> Attributes;

	bool IsValid() const;
	bool IsEmpty() const;
	uint8 Sample(FIntPoint InteriorTexel) const;
};

class SIGHTWEAVERUNTIME_API FSightWeaveStaticEnvironmentPacket final
{
public:
	bool IsValid() const { return bValid; }
	const FSightWeaveMemoryScopeKey& GetScope() const { return Scope; }
	uint64 GetEligibilityRevision() const { return EligibilityRevision; }
	uint64 GetPacketRevision() const { return PacketRevision; }
	TConstArrayView<FSightWeaveStaticEnvironmentTile> GetTiles() const { return Tiles; }
	int64 GetAttributeBytes() const { return AttributeBytes; }

private:
	friend class FSightWeaveStaticEnvironmentAuthority;
	bool bValid = false;
	FSightWeaveMemoryScopeKey Scope;
	uint64 EligibilityRevision = 0;
	uint64 PacketRevision = 0;
	int64 AttributeBytes = 0;
	TArray<FSightWeaveStaticEnvironmentTile> Tiles;
};

/** Game-thread registry and deterministic CPU-derived neutral-attribute authority. */
class SIGHTWEAVERUNTIME_API FSightWeaveStaticEnvironmentAuthority final
{
public:
	bool Configure(const FSightWeaveMemoryScopeKey& InScope, int32 InMaximumTiles);
	void Disable();
	void Reset();
	FSightWeaveStaticEnvironmentHandle Register(
		const FSightWeaveStaticEnvironmentDescription& Description);
	bool Update(
		FSightWeaveStaticEnvironmentHandle Handle,
		const FSightWeaveStaticEnvironmentDescription& Description);
	bool Unregister(FSightWeaveStaticEnvironmentHandle Handle);
	bool IsHandleValid(FSightWeaveStaticEnvironmentHandle Handle) const;
	TSharedPtr<const FSightWeaveStaticEnvironmentPacket, ESPMode::ThreadSafe> PublishPacket();

	bool IsConfigured() const { return bConfigured; }
	uint64 GetEligibilityRevision() const { return EligibilityRevision; }
	int32 GetTileCount() const { return Tiles.Num(); }
	int64 GetAttributeBytes() const;

private:
	struct FRecord
	{
		FSightWeaveStaticEnvironmentHandle Handle;
		FSightWeaveStaticEnvironmentDescription Description;
	};
	bool Rebuild();

	FSightWeaveMemoryScopeKey Scope;
	TArray<FRecord> Records;
	TArray<FSightWeaveStaticEnvironmentTile> Tiles;
	int32 MaximumTiles = 0;
	int64 NextHandle = 1;
	uint64 EligibilityRevision = 0;
	uint64 NextPacketRevision = 1;
	bool bConfigured = false;
};
