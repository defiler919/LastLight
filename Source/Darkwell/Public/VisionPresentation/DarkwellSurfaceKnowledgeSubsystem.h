#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VisionPresentation/DarkwellSurfaceKnowledge.h"
#include "SightWeaveMemory.h"
#include "DarkwellSurfaceKnowledgeSubsystem.generated.h"
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTexture2D;

/** Explicit fixed Box domains shared by ObjectMemory and Static Environment.
 * Mutable/legacy sources remain with their existing epoch presenter. */
UCLASS()
class DARKWELL_API UDarkwellSurfaceKnowledgeSubsystem : public UTickableWorldSubsystem
{
 GENERATED_BODY()
public:
 // Negative span means no Whole policy; zero means first legal contact.
 bool RegisterFixedBox(UStaticMeshComponent* Mesh,FName StableDomain,FLinearColor Tint,bool StaticDomain,float WholeSpan=-1,uint32 ContentVersion=1);
 UFUNCTION(BlueprintPure) FString GetTelemetry() const;
 bool OwnsMesh(const UStaticMeshComponent* Mesh) const;
 void UnregisterDomain(FName Id);
 bool SaveDomain(FName Id,TArray<uint8>& Out) const;
 bool RestoreDomain(FName Id,const TArray<uint8>& Data);
 void Clear(const FBox2D& Region,bool StaticDomain);
 void SetBlock(const FBox2D& Region,bool Enabled,bool StaticDomain);
 virtual void Tick(float DeltaTime) override;
 virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(DarkwellSurfaceKnowledge,STATGROUP_Tickables); }
 virtual void Deinitialize() override;
 const FDarkwellSurfaceKnowledge* Find(FName Id) const;
 bool IsWholeRecognized(FName Id) const;
 UTexture2D* GetAtlas(FName Id) const;
#if WITH_DEV_AUTOMATION_TESTS
 TArray<FName> GetDomainIdsForTesting() const {TArray<FName> Ids;for(const auto& D:Domains)Ids.Add(D.Id);return Ids;}
#endif
 uint64 ExactSamples=0,Proofs=0,UploadBytes=0;
 double UpdateUs=0, ObserveUs=0, RecognitionUs=0, PublishUs=0;
private:
 struct FDomain
 {
  TWeakObjectPtr<UStaticMeshComponent> Mesh;
  FName Id;
  FTransform RegisteredTransform;
  FSightWeaveMemoryScopeKey Scope;
  FDarkwellSurfaceKnowledge Knowledge;
  UMaterialInstanceDynamic* Material=nullptr;
  UTexture2D* Atlas=nullptr;
  TArray<class UMaterialInterface*> OriginalMaterials;
  bool bStatic=false,bReady=false,bViolated=false,bRecognized=false;
  float WholeSpan=-1;
  uint32 ContentVersion=1;
 };
 TArray<FDomain> Domains;
 UPROPERTY(Transient) TArray<TObjectPtr<UObject>> Resources;
 FBox2D ObjectBlock=FBox2D(ForceInit),StaticBlock=FBox2D(ForceInit);
 void Publish(FDomain& D);
};
