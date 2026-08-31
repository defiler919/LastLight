// Copyright Epic Games, Inc. All Rights Reserved.

#include "VisionPresentation/DarkwellRememberedPropSubsystem.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "VisionPresentation/DarkwellRememberablePropComponent.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "VisionPresentation/DarkwellManualStaleRoom.h"

DEFINE_LOG_CATEGORY_STATIC(LogDarkwellRememberedProp, Log, All);

namespace Darkwell::RememberedProp
{
	constexpr TCHAR SurfaceMaterialPath[] =
		TEXT("/Game/Darkwell/Vision/ProjectFog/M_DarkwellFogSurface.M_DarkwellFogSurface");

	bool TransformsMatch(const FTransform& A, const FTransform& B)
	{
		return A.GetLocation().Equals(B.GetLocation(), 1.0f)
			&& A.GetRotation().Equals(B.GetRotation(), 1.0e-4f)
			&& A.GetScale3D().Equals(B.GetScale3D(), 1.0e-4f);
	}
}

void FDarkwellRememberedPropState::Initialize(
	const FTransform& InitialTransform,
	const uint64 InitialAppearanceRevision)
{
	bWasLive = false;
	bSnapshotValid = true;
	SnapshotTransform = InitialTransform;
	AppearanceRevision = InitialAppearanceRevision;
}

bool FDarkwellRememberedPropState::ResolveObjectLive(
	const bool bPreviouslyLive,
	const float MaximumCoverage)
{
	return FMath::Clamp(MaximumCoverage, 0.0f, 1.0f)
		>= (bPreviouslyLive ? ExitCoverage : EnterCoverage);
}

FDarkwellRememberedPropDecision FDarkwellRememberedPropState::Observe(
	const bool bCurrentExists,
	const FTransform& CurrentTransform,
	const float CurrentMaximumCoverage,
	const float SnapshotMaximumCoverage,
	const uint64 CurrentAppearanceRevision,
	const bool bVerifyOldLocation)
{
	FDarkwellRememberedPropDecision Result;
	Result.bCurrentLive = bCurrentExists
		&& ResolveObjectLive(bWasLive, CurrentMaximumCoverage);
	if (Result.bCurrentLive)
	{
		Result.bRetainPreviousSnapshot = bVerifyOldLocation && bSnapshotValid
			&& !Darkwell::RememberedProp::TransformsMatch(SnapshotTransform, CurrentTransform)
			&& SnapshotMaximumCoverage < EnterCoverage;
		Result.bSnapshotChanged = !bSnapshotValid
			|| !Darkwell::RememberedProp::TransformsMatch(SnapshotTransform, CurrentTransform)
			|| AppearanceRevision != CurrentAppearanceRevision;
		bSnapshotValid = true;
		SnapshotTransform = CurrentTransform;
		AppearanceRevision = CurrentAppearanceRevision;
	}
	else if (bSnapshotValid
		&& SnapshotMaximumCoverage >= EnterCoverage)
	{
		const bool bCurrentStillAtSnapshot = bCurrentExists
			&& Darkwell::RememberedProp::TransformsMatch(
				SnapshotTransform, CurrentTransform);
		if (!bCurrentStillAtSnapshot)
		{
			bSnapshotValid = false;
			Result.bSnapshotChanged = true;
		}
	}
	bWasLive = Result.bCurrentLive;
	Result.bSnapshotValid = bSnapshotValid;
	Result.bShowCurrent = Result.bCurrentLive;
	Result.bShowProxy = bSnapshotValid && !Result.bCurrentLive;
	return Result;
}

void UDarkwellRememberedPropSubsystem::Deinitialize()
{
	for (TPair<FName, FRecord>& Pair : Records)
	{
		if (UDarkwellRememberablePropComponent* Component = Pair.Value.Component.Get())
		{
			Component->ApplySourceLiveState(true);
		}
		DestroyProxy(Pair.Value);
		for (auto Proxy : Pair.Value.UnverifiedProxies)
		{
			if (Proxy.IsValid()) Proxy->Destroy();
		}
	}
	Records.Reset();
	Diagnostics = FDarkwellRememberedPropDiagnostics();
	Super::Deinitialize();
}

void UDarkwellRememberedPropSubsystem::Tick(const float DeltaTime)
{
	RefreshRecords();
}

TStatId UDarkwellRememberedPropSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(
		UDarkwellRememberedPropSubsystem,
		STATGROUP_Tickables);
}

bool UDarkwellRememberedPropSubsystem::RegisterProp(
	UDarkwellRememberablePropComponent* Component)
{
	if (!Component || Component->GetWorld() != GetWorld()
		|| Component->GetStableId().IsNone()
		|| Component->GetMemoryPrimitives().IsEmpty())
	{
		return false;
	}
	const FName StableId = Component->GetStableId();
	if (FRecord* Existing = Records.Find(StableId))
	{
		if (Existing->Component.IsValid() && Existing->Component.Get() != Component)
		{
			++Diagnostics.DuplicateStableIdRejectCount;
			UE_LOG(LogDarkwellRememberedProp, Warning,
				TEXT("Duplicate RememberableProp stable id rejected: %s"),
				*StableId.ToString());
			return false;
		}
		Existing->Component = Component;
		return true;
	}

	FRecord& Record = Records.Add(StableId);
	Record.Component = Component;
	Record.Tint = Component->GetRememberedTint();
	Record.UVScale = Component->GetRememberedUVScale();
	Record.State.Initialize(
		Component->GetObservationTransform(),
		Component->ComputeAppearanceRevision());
	CaptureSnapshot(Record, *Component);
	RebuildProxy(StableId, Record);
	if (!Component->bRememberFromStart)
	{
		Record.State.bSnapshotValid = false;
		DestroyProxy(Record);
	}
	++Diagnostics.SnapshotRevision;
	RefreshRecords();
	return true;
}

void UDarkwellRememberedPropSubsystem::UnregisterProp(
	UDarkwellRememberablePropComponent* Component,
	const EEndPlayReason::Type EndPlayReason)
{
	if (!Component)
	{
		return;
	}
	FRecord* Record = Records.Find(Component->GetStableId());
	if (!Record || Record->Component.Get() != Component)
	{
		return;
	}
	Record->Component.Reset();
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		return;
	}
	DestroyProxy(*Record);
	for (auto Proxy : Record->UnverifiedProxies)
	{
		if (Proxy.IsValid()) Proxy->Destroy();
	}
	Records.Remove(Component->GetStableId());
}

void UDarkwellRememberedPropSubsystem::RefreshNowForTesting()
{
	RefreshRecords();
}

bool UDarkwellRememberedPropSubsystem::SetLabVerificationSubject(FName StableId, bool bManualObservation)
{
 if(!Darkwell::PropLab::IsLabWorld(GetWorld()) || StableId.IsNone()) return false;
 LabVerificationSubject=StableId; bLabSnapshotFrozen=false; bLabManualObservation=bManualObservation; return true;
}
AActor* UDarkwellRememberedPropSubsystem::FreezeLabVerificationSnapshot()
{
 if(!Darkwell::PropLab::IsLabWorld(GetWorld())) return nullptr;
 FRecord* Record=Records.Find(LabVerificationSubject);
 if(!Record || !Record->State.bSnapshotValid || !Record->ProxyActor.IsValid()) return nullptr;
 bLabSnapshotFrozen=true;
 Record->ProxyActor->SetActorHiddenInGame(false);
 return Record->ProxyActor.Get();
}
void UDarkwellRememberedPropSubsystem::FinishLabVerificationSnapshot()
{
 if(!Darkwell::PropLab::IsLabWorld(GetWorld()) || (!bLabSnapshotFrozen && !bLabManualObservation)) return;
 if(FRecord* Record=Records.Find(LabVerificationSubject))
 { Record->State.bSnapshotValid=false; DestroyProxy(*Record); }
}
void UDarkwellRememberedPropSubsystem::ReleaseLabVerificationSubject()
{
 if(!Darkwell::PropLab::IsLabWorld(GetWorld())) return;
 if(FRecord* Record=Records.Find(LabVerificationSubject)) DestroyProxy(*Record);
 Records.Remove(LabVerificationSubject);
 LabVerificationSubject=NAME_None; bLabSnapshotFrozen=false; bLabManualObservation=false;
}

int32 UDarkwellRememberedPropSubsystem::GetUnverifiedSnapshotCount(FName StableId) const
{
	const FRecord* Record = Records.Find(StableId);
	return Record ? Record->UnverifiedProxies.Num() : 0;
}

bool UDarkwellRememberedPropSubsystem::TryGetRecordForTesting(
	const FName StableId,
	bool& bOutCurrentLive,
	bool& bOutSnapshotValid,
	FVector& OutSnapshotLocation,
	AActor*& OutProxyActor) const
{
	const FRecord* Record = Records.Find(StableId);
	if (!Record)
	{
		return false;
	}
	bOutCurrentLive = Record->State.bWasLive;
	bOutSnapshotValid = Record->State.bSnapshotValid;
	OutSnapshotLocation = Record->State.SnapshotTransform.GetLocation();
	OutProxyActor = Record->ProxyActor.Get();
	return true;
}

void UDarkwellRememberedPropSubsystem::RefreshRecords()
{
	UDarkwellFogVisualSubsystem* Fog = GetWorld()
		? GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>() : nullptr;
	Diagnostics.RegisteredCount = Records.Num();
	Diagnostics.LiveCount = 0;
	Diagnostics.ProxyCount = 0;
	Diagnostics.RetainedDestroyedCount = 0;
	if (!Fog || !Fog->IsActive())
	{
		for (TPair<FName, FRecord>& Pair : Records)
		{
			if (UDarkwellRememberablePropComponent* Component = Pair.Value.Component.Get())
			{
				Component->ApplySourceLiveState(true);
			}
			if (AActor* Proxy = Pair.Value.ProxyActor.Get())
			{
				Proxy->SetActorHiddenInGame(true);
			}
		}
		return;
	}
	for (TPair<FName, FRecord>& Pair : Records)
	{
		FRecord& Record = Pair.Value;
		UDarkwellRememberablePropComponent* Component = Record.Component.Get();
		if(Darkwell::PropLab::IsLabWorld(GetWorld()) && !LabVerificationSubject.IsNone())
		{
			if(Pair.Key!=LabVerificationSubject)
			{
				if(Component) { Component->ApplySourceLiveState(false); Component->ApplySourceGeometryVisibility(false); }
				if(Record.ProxyActor.IsValid()) Record.ProxyActor->SetActorHiddenInGame(true);
				for(auto Proxy:Record.UnverifiedProxies) if(Proxy.IsValid()) Proxy->SetActorHiddenInGame(true);
				continue;
			}
			if(bLabSnapshotFrozen)
			{
				// Source B still uses legal live authority. A is owned solely by the
				// independent lab empty-evidence grid, never .50/.25 or known movement.
				const bool bLive=Component && FDarkwellRememberedPropState::ResolveObjectLive(
					Record.State.bWasLive,EvaluateMaximumCoverage(*Component));
				Record.State.bWasLive=bLive;
				if(Component) { Component->ApplySourceLiveState(bLive); Component->ApplySourceGeometryVisibility(bLive); }
				Diagnostics.LiveCount+=bLive;
				Diagnostics.ProxyCount+=Record.ProxyActor.IsValid();
				continue;
			}
		}
		const bool bCurrentExists = Component && IsValid(Component->GetOwner());
		const FTransform CurrentTransform = bCurrentExists
			? Component->GetObservationTransform()
			: FTransform::Identity;
		const float CurrentCoverage = Fog && bCurrentExists
			? EvaluateMaximumCoverage(*Component) : 0.0f;
		// The free-play laboratory observes PRESENT through the ordinary identity
		// authority, but only its independent grid may confirm ABSENT. A switch
		// never invalidates a snapshot. This opt-in cannot affect other worlds.
		const bool bManualSubject = Darkwell::PropLab::IsLabWorld(GetWorld())
			&& bLabManualObservation && Pair.Key == LabVerificationSubject;
		const float SnapshotCoverage = Fog && Record.State.bSnapshotValid && !bManualSubject
			? EvaluateMaximumSnapshotCoverage(Record) : 0.0f;
		const uint64 AppearanceRevision = bCurrentExists
			? Component->ComputeAppearanceRevision() : Record.State.AppearanceRevision;
		const FDarkwellRememberedPropDecision Decision = Record.State.Observe(
			bCurrentExists,
			CurrentTransform,
			CurrentCoverage,
			SnapshotCoverage,
			AppearanceRevision,
			Darkwell::PropLab::IsLabWorld(GetWorld()) && Darkwell::PropLab::RelocationPolicy(GetWorld()) == 0);
		// Retired snapshots belong to this StableID record, never to appearance or pixels.
		for (int32 Index = Record.UnverifiedProxies.Num() - 1; Index >= 0; --Index)
		{
			AActor* Old = Record.UnverifiedProxies[Index].Get();
			float Coverage = 0.0f;
			if (Old)
			{
				TInlineComponentArray<UStaticMeshComponent*> Meshes(Old);
				for (const auto* Mesh : Meshes)
				{
					for (double X : {-1.0, 1.0}) for (double Y : {-1.0, 1.0})
					{
						const FVector Point = Mesh->Bounds.Origin + Mesh->Bounds.BoxExtent * FVector(X, Y, 0);
						Coverage = FMath::Max(Coverage, Fog->EvaluateLiveCoverageAtWorldPoint(FVector2D(Point)));
					}
					Coverage = FMath::Max(Coverage, Fog->EvaluateLiveCoverageAtWorldPoint(FVector2D(Mesh->Bounds.Origin)));
				}
			}
			if (!Old || Darkwell::PropLab::RelocationPolicy(GetWorld()) == 1 || Coverage >= FDarkwellRememberedPropState::EnterCoverage)
			{
				if (Old) Old->Destroy();
				Record.UnverifiedProxies.RemoveAt(Index);
			}
		}
		// Known, unchanged geometry may display a surface sweep below the object
		// identity threshold. This never changes Observe(), LiveOnly effects, or
		// an unseen relocated/replaced source. Mode 0 and other maps stay exact.
		const bool bLab = Darkwell::PropLab::IsLabWorld(GetWorld());
		const bool bKnownSurfaceGeometry = bLab && LabVerificationSubject.IsNone()
			&& Darkwell::PropLab::PresentationMode(GetWorld()) != 0
			&& bCurrentExists && Record.State.bSnapshotValid
			&& Component->GetOwner()->IsA<ADarkwellPropLabFurniture>()
			&& Darkwell::RememberedProp::TransformsMatch(Record.State.SnapshotTransform, CurrentTransform)
			&& Record.State.AppearanceRevision == AppearanceRevision;
        const bool bManualSpatial = bManualSubject && Darkwell::PropLab::PresentationMode(GetWorld())==2;
        const bool bShowProxyGeometry = bManualSpatial || (Decision.bShowProxy && !bKnownSurfaceGeometry);
		if (Component)
		{
			bool bMaskedManualGeometry=false;
			if (bManualSubject)
				if (auto* Furniture=Cast<ADarkwellPropLabFurniture>(Component->GetOwner()))
					bMaskedManualGeometry=Furniture->SetManualFixedRevealEnabled(Darkwell::PropLab::PresentationMode(GetWorld())==2);
			Component->ApplySourceLiveState(Decision.bShowCurrent);
			if (bLab) Component->ApplySourceGeometryVisibility(Decision.bShowCurrent || bKnownSurfaceGeometry || bMaskedManualGeometry);
		}
		if (Decision.bSnapshotChanged && Decision.bSnapshotValid && Component)
		{
			if (Decision.bRetainPreviousSnapshot && Record.ProxyActor.IsValid())
			{
				Record.ProxyActor->SetActorHiddenInGame(false);
				Record.UnverifiedProxies.Add(Record.ProxyActor);
				Record.ProxyActor.Reset();
			}
			CaptureSnapshot(Record, *Component);
			RebuildProxy(Pair.Key, Record);
			++Diagnostics.SnapshotRevision;
		}
		else if (!Decision.bSnapshotValid)
		{
			DestroyProxy(Record);
		}
		if (AActor* Proxy = Record.ProxyActor.Get())
		{
            // Bind before first submission: snapshotValid never grants full
            // gray visibility. B excludes every surface owned by current D.
            if (bManualSpatial)
                if (auto* Room=ADarkwellManualStaleRoom::FindActive(GetWorld())) Room->BindSpatialProxy(Proxy);
			Proxy->SetActorHiddenInGame(!bShowProxyGeometry);
		}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (!Record.bDiagnosticStateValid
			|| Record.bDiagnosticLastLive != Decision.bShowCurrent
			|| Record.bDiagnosticLastProxy != bShowProxyGeometry
			|| Record.bDiagnosticLastSnapshotValid != Decision.bSnapshotValid)
		{
			Record.bDiagnosticStateValid = true;
			Record.bDiagnosticLastLive = Decision.bShowCurrent;
			Record.bDiagnosticLastProxy = bShowProxyGeometry;
			Record.bDiagnosticLastSnapshotValid = Decision.bSnapshotValid;
			const FVector SnapshotLocation = Record.State.SnapshotTransform.GetLocation();
			UE_LOG(LogDarkwellRememberedProp, Display,
				TEXT("RememberedPropState id=%s currentLive=%d proxy=%d snapshotValid=%d snapshot=(%.1f,%.1f,%.1f)"),
				*Pair.Key.ToString(),
				Decision.bShowCurrent ? 1 : 0,
				bShowProxyGeometry ? 1 : 0,
				Decision.bSnapshotValid ? 1 : 0,
				SnapshotLocation.X,
				SnapshotLocation.Y,
				SnapshotLocation.Z);
		}
#endif
		Diagnostics.LiveCount += Decision.bShowCurrent ? 1 : 0;
		Diagnostics.ProxyCount += bShowProxyGeometry ? 1 : 0;
		Diagnostics.RetainedDestroyedCount += !bCurrentExists
			&& Decision.bShowProxy ? 1 : 0;
	}
}

float UDarkwellRememberedPropSubsystem::EvaluateMaximumCoverage(
	const UDarkwellRememberablePropComponent& Component) const
{
	const UDarkwellFogVisualSubsystem* Fog = GetWorld()
		? GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>() : nullptr;
	float Maximum = 0.0f;
	for (const UStaticMeshComponent* Primitive : Component.GetMemoryPrimitives())
	{
		if (!Primitive || !Fog)
		{
			continue;
		}
		const FVector Center = Primitive->Bounds.Origin;
		const FVector Extent = Primitive->Bounds.BoxExtent;
		for (const double XSign : {-1.0, 1.0})
		{
			for (const double YSign : {-1.0, 1.0})
			{
				Maximum = FMath::Max(Maximum, Fog->EvaluateLiveCoverageAtWorldPoint(
					FVector2D(Center.X + Extent.X * XSign, Center.Y + Extent.Y * YSign)));
			}
		}
		Maximum = FMath::Max(Maximum, Fog->EvaluateLiveCoverageAtWorldPoint(
			FVector2D(Center.X, Center.Y)));
	}
	return Maximum;
}

float UDarkwellRememberedPropSubsystem::EvaluateMaximumSnapshotCoverage(
	const FRecord& Record) const
{
	const UDarkwellFogVisualSubsystem* Fog = GetWorld()
		? GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>() : nullptr;
	const AActor* Proxy = Record.ProxyActor.Get();
	if (!Fog || !Proxy)
	{
		return 0.0f;
	}
	float Maximum = 0.0f;
	TInlineComponentArray<UStaticMeshComponent*> Meshes(Proxy);
	for (const UStaticMeshComponent* Mesh : Meshes)
	{
		const FVector Center = Mesh->Bounds.Origin;
		const FVector Extent = Mesh->Bounds.BoxExtent;
		for (const double XSign : {-1.0, 1.0})
		{
			for (const double YSign : {-1.0, 1.0})
			{
				Maximum = FMath::Max(Maximum, Fog->EvaluateLiveCoverageAtWorldPoint(
					FVector2D(Center.X + Extent.X * XSign, Center.Y + Extent.Y * YSign)));
			}
		}
		Maximum = FMath::Max(Maximum, Fog->EvaluateLiveCoverageAtWorldPoint(
			FVector2D(Center.X, Center.Y)));
	}
	return Maximum;
}

void UDarkwellRememberedPropSubsystem::CaptureSnapshot(
	FRecord& Record,
	UDarkwellRememberablePropComponent& Component)
{
	Record.Primitives.Reset();
	for (const UStaticMeshComponent* Primitive : Component.GetMemoryPrimitives())
	{
		if (!Primitive || !Primitive->GetStaticMesh())
		{
			continue;
		}
		FPrimitiveSnapshot& Snapshot = Record.Primitives.AddDefaulted_GetRef();
		Snapshot.Mesh = Primitive->GetStaticMesh();
		Snapshot.WorldTransform = Primitive->GetComponentTransform();
	}
}

void UDarkwellRememberedPropSubsystem::RebuildProxy(
	const FName StableId,
	FRecord& Record)
{
	DestroyProxy(Record);
	if (!GetWorld() || Record.Primitives.IsEmpty())
	{
		return;
	}
	UMaterialInterface* SurfaceParent = LoadObject<UMaterialInterface>(
		nullptr, Darkwell::PropLab::IsLabWorld(GetWorld())
			? TEXT("/Game/Darkwell/Vision/PropLab/M_PropLabSurface.M_PropLabSurface")
			: Darkwell::RememberedProp::SurfaceMaterialPath);
	if (!SurfaceParent)
	{
		UE_LOG(LogDarkwellRememberedProp, Error,
			TEXT("RememberedProp proxy material is missing"));
		return;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = MakeUniqueObjectName(
		GetWorld(), AActor::StaticClass(),
		*FString::Printf(TEXT("Remembered_%s"), *StableId.ToString()));
	SpawnParameters.ObjectFlags |= RF_Transient;
	AActor* Proxy = GetWorld()->SpawnActor<AActor>(
		AActor::StaticClass(), FTransform::Identity, SpawnParameters);
	if (!Proxy)
	{
		return;
	}
	Proxy->SetActorEnableCollision(false);
	USceneComponent* Root = NewObject<USceneComponent>(Proxy, TEXT("MemoryRoot"));
	Proxy->SetRootComponent(Root);
	Root->RegisterComponent();
	for (int32 Index = 0; Index < Record.Primitives.Num(); ++Index)
	{
		const FPrimitiveSnapshot& Snapshot = Record.Primitives[Index];
		UStaticMesh* Mesh = Snapshot.Mesh.LoadSynchronous();
		if (!Mesh)
		{
			continue;
		}
		UStaticMeshComponent* ProxyMesh = NewObject<UStaticMeshComponent>(
			Proxy, *FString::Printf(TEXT("MemoryMesh_%d"), Index));
		ProxyMesh->SetupAttachment(Root);
		ProxyMesh->SetStaticMesh(Mesh);
		ProxyMesh->SetWorldTransform(Snapshot.WorldTransform);
		ProxyMesh->SetMobility(EComponentMobility::Movable);
		ProxyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ProxyMesh->SetGenerateOverlapEvents(false);
		ProxyMesh->SetCanEverAffectNavigation(false);
		ProxyMesh->SetCastShadow(false);
		ProxyMesh->SetAffectDynamicIndirectLighting(false);
		ProxyMesh->SetAffectDistanceFieldLighting(false);
		ProxyMesh->SetVisibleInRayTracing(false);
		ProxyMesh->SetRenderCustomDepth(false);
		ProxyMesh->SetReceivesDecals(false);
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(
			SurfaceParent, ProxyMesh);
		if (Material)
		{
			Material->SetScalarParameterValue(TEXT("ForceRemembered"), 1.0f);
			Material->SetScalarParameterValue(TEXT("OriginalUVScale"), Record.UVScale);
			Material->SetVectorParameterValue(TEXT("OriginalBaseColorTint"), Record.Tint);
			Material->SetScalarParameterValue(TEXT("GroundCoverageWeight"), 0.0f);
			Material->SetScalarParameterValue(TEXT("WallCoverageWeight"), 0.0f);
			Material->SetScalarParameterValue(TEXT("BoxCoverageWeight"), 0.0f);
			ProxyMesh->SetMaterial(0, Material);
		}
		ProxyMesh->RegisterComponent();
	}
	Record.ProxyActor = Proxy;
}

void UDarkwellRememberedPropSubsystem::DestroyProxy(FRecord& Record)
{
	if (AActor* Proxy = Record.ProxyActor.Get())
	{
		Proxy->Destroy();
	}
	Record.ProxyActor.Reset();
}
