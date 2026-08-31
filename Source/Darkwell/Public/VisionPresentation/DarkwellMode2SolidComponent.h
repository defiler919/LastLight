#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DarkwellMode2SolidComponent.generated.h"

class UDynamicMeshComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UTexture2D;
class ADarkwellPropLabFurniture;
struct FDarkwellEmptyVerification;

/** Rendering only. Explicit axis-aligned box binding for the manual cabinet,
 * not a general mesh solidifier, identity record or visibility authority. */
UCLASS()
class DARKWELL_API UDarkwellMode2SolidComponent final : public UActorComponent
{
 GENERATED_BODY()
public:
 void ResetPresentation();
 void Update(float DeltaSeconds, ADarkwellPropLabFurniture* Actual, AActor* Snapshot,
  bool bLive, const FDarkwellEmptyVerification& Evidence, const TArray<float>& Opacity,
  UStaticMeshComponent* Floor);
 // Also called after the adapter's ordinary visibility writes: no one-frame full source leak.
 void EnforceSource(ADarkwellPropLabFurniture* Actual, AActor* Snapshot);
 UFUNCTION(BlueprintPure, Category="Lab") float GetRevealFraction() const { return RevealFraction; }
 UFUNCTION(BlueprintPure, Category="Lab") int32 GetCapTriangles() const { return CapTriangles; }
 UFUNCTION(BlueprintPure, Category="Lab") int32 GetLiveTriangles() const { return LiveTriangles; }
 UFUNCTION(BlueprintPure, Category="Lab") int32 GetShadowSources() const;
 static float AdvanceReveal(float Previous, float Coverage, float DeltaSeconds);
 static float SampleOpacity(const FDarkwellEmptyVerification& Evidence, const TArray<float>& Values, FVector2D P);
private:
 UPROPERTY(Transient) TObjectPtr<UDynamicMeshComponent> SolidMemory;
 UPROPERTY(Transient) TObjectPtr<UDynamicMeshComponent> SpatialLive;
 UPROPERTY(Transient) TObjectPtr<UTexture2D> RevealTexture;
 UPROPERTY(Transient) TObjectPtr<UMaterialInterface> OriginalFloor;
 UPROPERTY(Transient) TObjectPtr<UMaterialInterface> LitFloor;
 TWeakObjectPtr<ADarkwellPropLabFurniture> LastActual;
 TWeakObjectPtr<UStaticMeshComponent> BoundFloor;
 TArray<float> Reveal;
 FBox2D RevealBounds;
 FIntPoint RevealSize=FIntPoint::ZeroValue;
 float RevealFraction=0;
 int32 CapTriangles=0, LiveTriangles=0;
 bool bEnabled=false;
};
