#include "SightWeaveLastSeenProxyComponent.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

USightWeaveLastSeenProxyComponent::USightWeaveLastSeenProxyComponent()
{
	EnforceRenderOnlyConfiguration();
	SetVisibility(false, true);
}

bool USightWeaveLastSeenProxyComponent::PresentSnapshot(
	const FSightWeaveLastSeenSnapshotDescriptor& Snapshot,
	const FSightWeaveSubjectPresentationResult& Presentation)
{
	check(IsInGameThread());
	if (Presentation.State != ESightWeaveSubjectPresentationState::LastSeenProxy
		|| Presentation.Failure != ESightWeaveSubjectPresentationFailure::None
		|| Presentation.SnapshotRevision == 0
		|| Presentation.SnapshotRevision != Snapshot.SnapshotRevision
		|| !Snapshot.IsValid())
	{
		HideAndClear();
		return false;
	}

	// A snapshot revision is immutable. Re-applying the same descriptor must not
	// recreate the component scene proxy or enqueue a fresh transform every tick.
	if (PresentedSnapshot.IsSet()
		&& PresentedSnapshot->IsEquivalentTo(Snapshot)
		&& PresentedSnapshotRevision == Snapshot.SnapshotRevision
		&& IsVisible()
		&& bRenderCustomDepth)
	{
		return true;
	}

	HideAndClear();

	UStaticMesh* LoadedStaticMesh = Cast<UStaticMesh>(Snapshot.StaticMeshAsset.TryLoad());
	if (!LoadedStaticMesh)
	{
		return false;
	}

	TArray<UMaterialInterface*> Materials;
	Materials.Reserve(Snapshot.MaterialOverrides.Num());
	for (const FSoftObjectPath& MaterialPath : Snapshot.MaterialOverrides)
	{
		UMaterialInterface* Material = Cast<UMaterialInterface>(MaterialPath.TryLoad());
		if (!Material || Material->GetBlendMode() != BLEND_Opaque)
		{
			return false;
		}
		Materials.Add(Material);
	}

	SetStaticMesh(LoadedStaticMesh);
	for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
	{
		SetMaterial(MaterialIndex, Materials[MaterialIndex]);
	}
	SetWorldTransform(
		Snapshot.WorldTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	PresentedSnapshotRevision = Snapshot.SnapshotRevision;
	PresentedSnapshot = Snapshot;
	EnforceRenderOnlyConfiguration();
	SetRenderCustomDepth(true);
	SetVisibility(true, true);
	return true;
}

void USightWeaveLastSeenProxyComponent::HideAndClear()
{
	check(IsInGameThread());
	SetVisibility(false, true);
	for (int32 MaterialIndex = 0; MaterialIndex < GetNumMaterials(); ++MaterialIndex)
	{
		SetMaterial(MaterialIndex, nullptr);
	}
	SetStaticMesh(nullptr);
	PresentedSnapshotRevision = 0;
	PresentedSnapshot.Reset();
	EnforceRenderOnlyConfiguration();
	SetRenderCustomDepth(false);
}

bool USightWeaveLastSeenProxyComponent::HasRenderOnlyConfiguration() const
{
	return GetCollisionEnabled() == ECollisionEnabled::NoCollision
		&& !GetGenerateOverlapEvents()
		&& !CanEverAffectNavigation()
		&& !PrimaryComponentTick.bCanEverTick
		&& !PrimaryComponentTick.IsTickFunctionEnabled()
		&& !IsSimulatingPhysics()
		&& !CastShadow
		&& !bAffectDynamicIndirectLighting
		&& !bAffectDistanceFieldLighting
		&& !bRenderInMainPass
		&& !bRenderInDepthPass
		&& CustomDepthStencilValue == SightWeave::SubjectMemory::LastSeenProxyStencilValue;
}

void USightWeaveLastSeenProxyComponent::OnRegister()
{
	EnforceRenderOnlyConfiguration();
	Super::OnRegister();
}

void USightWeaveLastSeenProxyComponent::OnComponentDestroyed(const bool bDestroyingHierarchy)
{
	HideAndClear();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void USightWeaveLastSeenProxyComponent::EnforceRenderOnlyConfiguration()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.SetTickFunctionEnable(false);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetGenerateOverlapEvents(false);
	SetNotifyRigidBodyCollision(false);
	SetSimulatePhysics(false);
	SetEnableGravity(false);
	SetCanEverAffectNavigation(false);
	SetCastShadow(false);
	SetRenderInMainPass(false);
	SetRenderInDepthPass(false);
	SetCustomDepthStencilValue(SightWeave::SubjectMemory::LastSeenProxyStencilValue);
	bAffectDynamicIndirectLighting = false;
	bAffectDistanceFieldLighting = false;
	SetReceivesDecals(false);
}

bool FSightWeaveSubjectProxyPresentationBridge::Apply(
	const FSightWeaveSubjectPresentationResult& Presentation,
	const FSightWeaveLastSeenSnapshotDescriptor* Snapshot,
	UPrimitiveComponent* LivePresentation,
	USightWeaveLastSeenProxyComponent* ProxyPresentation)
{
	check(IsInGameThread());
	if (!LivePresentation || !ProxyPresentation)
	{
		if (ProxyPresentation)
		{
			ProxyPresentation->HideAndClear();
		}
		if (LivePresentation)
		{
			LivePresentation->SetVisibility(false, true);
		}
		return false;
	}

	if (Presentation.Failure != ESightWeaveSubjectPresentationFailure::None)
	{
		LivePresentation->SetVisibility(false, true);
		ProxyPresentation->HideAndClear();
		return false;
	}

	switch (Presentation.State)
	{
	case ESightWeaveSubjectPresentationState::Live:
		// Hide the render-only proxy first. GT -> RT commands preserve this order,
		// so a reacquired live primitive can never share a frame with its proxy.
		ProxyPresentation->HideAndClear();
		LivePresentation->SetVisibility(true, true);
		return true;
	case ESightWeaveSubjectPresentationState::LastSeenProxy:
		LivePresentation->SetVisibility(false, true);
		return Snapshot && ProxyPresentation->PresentSnapshot(*Snapshot, Presentation);
	case ESightWeaveSubjectPresentationState::Hidden:
	case ESightWeaveSubjectPresentationState::StaticEnvironmentDelegated:
	default:
		LivePresentation->SetVisibility(false, true);
		ProxyPresentation->HideAndClear();
		return false;
	}
}
