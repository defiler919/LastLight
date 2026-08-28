#pragma once

#include "Components/StaticMeshComponent.h"
#include "SightWeaveSubjectMemory.h"

#include "SightWeaveLastSeenProxyComponent.generated.h"

namespace SightWeave::SubjectMemory
{
	/** Reserved render-only stencil marker consumed by the M3 presentation composite. */
	inline constexpr int32 LastSeenProxyStencilValue = 246;
	inline constexpr float LastSeenProxyNeutralIntensity = 0.46f;
}

/**
 * Presentation-only static mesh for one immutable Last-Seen descriptor.
 * It owns no subject authority and deliberately has no gameplay-facing API.
 */
UCLASS(NotBlueprintable, ClassGroup = (SightWeave), Transient)
class SIGHTWEAVERUNTIME_API USightWeaveLastSeenProxyComponent final : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	USightWeaveLastSeenProxyComponent();

	bool PresentSnapshot(
		const FSightWeaveLastSeenSnapshotDescriptor& Snapshot,
		const FSightWeaveSubjectPresentationResult& Presentation);
	void HideAndClear();

	uint64 GetPresentedSnapshotRevision() const { return PresentedSnapshotRevision; }
	bool HasRenderOnlyConfiguration() const;

protected:
	virtual void OnRegister() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	void EnforceRenderOnlyConfiguration();

	uint64 PresentedSnapshotRevision = 0;
};

/** Applies one CPU-authoritative presentation result without changing gameplay state. */
class SIGHTWEAVERUNTIME_API FSightWeaveSubjectProxyPresentationBridge final
{
public:
	static bool Apply(
		const FSightWeaveSubjectPresentationResult& Presentation,
		const FSightWeaveLastSeenSnapshotDescriptor* Snapshot,
		UPrimitiveComponent* LivePresentation,
		USightWeaveLastSeenProxyComponent* ProxyPresentation);
};
