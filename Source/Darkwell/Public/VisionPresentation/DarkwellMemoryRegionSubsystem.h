#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SightWeaveMemory.h"
#include "Subsystems/WorldSubsystem.h"
#include "DarkwellMemoryRegionSubsystem.generated.h"

class UTexture2D;
class UMaterialInstanceDynamic;
class UDarkwellFogVisualSubsystem;

/** One bounded XY room region in this world's current player/floor domain.
 * CPU bits own ground knowledge. Object histories remain owned by their scenes.
 * No GPU readback, persistence, alternate shapes or live suppression. */
UCLASS()
class DARKWELL_API UDarkwellMemoryRegionSubsystem : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual void Deinitialize() override;
 /** Exclusive gameplay control of this world's existing single fixed region.
  * Releasing control never removes the configured box or its cleared knowledge. */
 bool AcquireGameplayControl(AActor* Owner, FVector2D Min, FVector2D Max);
 void ReleaseGameplayControl(AActor* Owner);
 bool IsGameplayControlledBy(const AActor* Owner) const;
 /** Idempotent for the same box. A different box requires a new world in this slice.
  * Grid is 2.5 cm, maximum 256 x 256. Straddling Whole records are refused; Partial uses sample-center membership. */
 UFUNCTION(BlueprintCallable, Category="SightWeave|Memory")
 bool ConfigureRegion(FVector2D Min, FVector2D Max);
 UFUNCTION(BlueprintCallable, Category="SightWeave|Memory") bool ClearMemory();
 UFUNCTION(BlueprintCallable, Category="SightWeave|Memory") bool SetBlockMemoryWrites(bool bEnabled);
 UFUNCTION(BlueprintPure, Category="SightWeave|Memory") FGameplayTag QueryKnowledge(FVector2D Point) const;
 bool HasStoredMemory(FVector2D Point) const;
 bool IsBlocked() const { return bBlocked; }
 bool IsConfigured() const { return Bounds.bIsValid; }
 const FBox2D& GetBounds() const { return Bounds; }
 FIntPoint GetSize() const { return Size; }
 UTexture2D* GetPresentationTexture() const { return PresentationTexture; }
 uint64 GetAuthorityRevision() const { return AuthorityRevision; }
 int32 GetStoredSampleCount() const { return RememberedBits.CountSetBits(); }
 void ObservePublishedCoverage(const UDarkwellFogVisualSubsystem& Fog);
 void BindMaterial(UMaterialInstanceDynamic* Material) const;
 static FGameplayTag Unknown();
 static FGameplayTag Remembered();
private:
 void ReleaseBlockRegistration(bool bPublish);
 TWeakObjectPtr<AActor> GameplayOwner;
 bool ValidateObjectBoundaries() const;
 int32 IndexAt(FVector2D Point) const;
 void Publish();
 bool ValidateRuntimeScope() const;
 FSightWeaveMemoryRegion RuntimeRegion;
 FSightWeaveMemoryModifierHandle RuntimeBlock;
 bool bHasRuntimeScope=false;
 FBox2D Bounds=FBox2D(ForceInit);
 FIntPoint Size=FIntPoint::ZeroValue;
 TBitArray<> RememberedBits;
 bool bBlocked=false;
 uint64 AuthorityRevision=0;
 UPROPERTY(Transient) TObjectPtr<UTexture2D> PresentationTexture;
};
