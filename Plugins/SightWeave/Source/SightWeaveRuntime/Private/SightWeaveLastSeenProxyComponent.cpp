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
	HideAndClear();
	if (Presentation.State != ESightWeaveSubjectPresentationState::LastSeenProxy
		|| Presentation.Failure != ESightWeaveSubjectPresentationFailure::None
		|| Presentation.SnapshotRevision == 0
		|| Presentation.SnapshotRevision != Snapshot.SnapshotRevision
		|| !Snapshot.IsValid())
	{
		return false;
	}

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
	EnforceRenderOnlyConfiguration();
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
	EnforceRenderOnlyConfiguration();
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
		&& !bAffectDistanceFieldLighting;
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
	bAffectDynamicIndirectLighting = false;
	bAffectDistanceFieldLighting = false;
	SetReceivesDecals(false);
}
