#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VisionPresentation/DarkwellStaticKnowledge.h"
#include "SightWeaveMemory.h"
#include "SightWeaveSurface.h"
#include "DarkwellStaticEnvironmentSubsystem.generated.h"
class UMeshComponent;
class UMaterialInstanceDynamic;
class UTexture2D;

/** One sparse immutable-space store and shared GPU atlas per world.
 * Registration is explicit; UE Mobility is not evidence of immutability. */
UCLASS()
class DARKWELL_API UDarkwellStaticEnvironmentSubsystem : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 bool RegisterImmutable(UMeshComponent* Mesh,FLinearColor Tint);
 bool RegisterImmutableSurfaceBox(class UStaticMeshComponent* Mesh,FName StableDomain,FLinearColor Tint,uint32 ContentVersion=1);
 bool HasSurfaceKnowledge(FName StableDomain,ESightWeaveBoxFace Face,FVector2D UV) const;
 void UpdateKnowledge();
 void ClearMemory(const FBox2D& Region);
 void SetMemoryWriteBlock(const FBox2D& Region,bool Enabled);
 UFUNCTION(BlueprintPure) bool HasStoredMemory(FVector2D Point) const {return bScopeValid && Knowledge.HasMemory(FVector(Point,ReferenceHeight));}
 UFUNCTION(BlueprintPure) bool HasStoredMemoryAtHeight(FVector Point) const {return bScopeValid && Knowledge.HasMemory(Point);}
 UFUNCTION(BlueprintPure) float GetLegalCoverage(FVector2D Point) const;
 UFUNCTION(BlueprintPure) FString GetTelemetry() const;
 virtual void Deinitialize() override;
private:
 void EnsureResources();
 void Publish();
 void Bind();
 FDarkwellLayeredStaticKnowledge Knowledge;
 TMap<FIntVector,int32> Pages;
 TArray<FVector4f> PageEntries;
 UPROPERTY(Transient) TObjectPtr<UTexture2D> Atlas;
 UPROPERTY(Transient) TObjectPtr<UTexture2D> PageTable;
 UPROPERTY(Transient) TObjectPtr<UTexture2D> HeightBands;
 UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
 TArray<TWeakObjectPtr<UMeshComponent>> Meshes;
 TSet<FName> SurfaceDomains;
 FBox2D Block=FBox2D(ForceInit);
 uint64 LastDraw=MAX_uint64;
 int32 PageSide=16,Uploads=0,LookupProbes=1;
 double UpdateUs=0,PublishUs=0;
 bool PageTableDirty=false,bPresentationCapacityValid=true;
 uint64 BoundLayout=MAX_uint64;
 double ReferenceHeight=0;
 TWeakObjectPtr<UTexture> BoundLive;
 FSightWeaveMemoryScopeKey Scope;
 bool bHasScope=false,bScopeValid=false,bBoundFogActive=false;
};
