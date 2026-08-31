#pragma once

#include "CoreMinimal.h"
#include "Visibility/DarkwellVisionIntegrationFixture.h"
#include "DarkwellPropGameplayLab.generated.h"

class UTextureRenderTarget2D;
class UDarkwellRememberablePropComponent;
struct FDarkwellLabCaptureWriter;

namespace Darkwell::PropLab
{
 DARKWELL_API bool IsLabWorld(const UWorld* World);
 DARKWELL_API int32 PresentationMode(const UWorld* World);
 DARKWELL_API int32 RelocationPolicy(const UWorld* World);
 /** Shared deterministic trajectory, independent of mode/policy. 30 seconds. */
 DARKWELL_API float ComparisonYaw(float Seconds);
 DARKWELL_API FVector ComparisonPlayerPosition();
}

/** Graybox asset binding only. All identity and controls remain native. */
UCLASS()
class DARKWELL_API ADarkwellPropLabFurniture : public AActor
{
 GENERATED_BODY()
public:
 ADarkwellPropLabFurniture();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 UPROPERTY(EditAnywhere, Category="Lab") FName StableId;
 /** 0 cabinet, 1 refrigerator, 2 shelf, 3 box, 4 counter. */
 UPROPERTY(EditAnywhere, Category="Lab") int32 Shape = 0;
 UPROPERTY(EditAnywhere, Category="Lab") FVector Dimensions = FVector(60, 60, 90);
 UPROPERTY(EditAnywhere, Category="Lab") bool bIndividualWorktop = true;
 UPROPERTY(EditAnywhere, Category="Lab") FLinearColor Tint = FLinearColor(0.24f, 0.40f, 0.56f);
 void BindPresentation(UTexture* Raw, UTexture* Soft, FVector2D Min, FVector2D Inv, int32 Mode);
 UPROPERTY(VisibleAnywhere, Category="Lab") TObjectPtr<UDarkwellRememberablePropComponent> Memory;
 UPROPERTY(VisibleAnywhere, Category="Lab") TArray<TObjectPtr<UStaticMeshComponent>> Parts;
 UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
};

/** Dedicated fixture. It extends the existing project fog adapter, never the plugin. */
UCLASS()
class DARKWELL_API ADarkwellPropGameplayLab : public ADarkwellVisionIntegrationFixture
{
 GENERATED_BODY()
public:
 ADarkwellPropGameplayLab();
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void Tick(float DeltaSeconds) override;
 virtual FBox2D GetSightWeaveFloorBounds() const override;
 virtual void BuildSightWeaveOccluderSegments(TArray<FDarkwellVisionIntegrationSegment>& Out) const override;
 virtual void BuildSightWeaveStaticSurfaces(TArray<FDarkwellVisionIntegrationSurface>& Out) const override;
 virtual bool EnableDarkwellProjectFogP4(UTexture* Raw, FVector2D Min, FVector2D Inv) override;
 virtual void DisableDarkwellProjectFog() override;
 void Event(const FString& Command);
 int32 GetActiveRoute() const { return LastRoute; }
 float GetRouteTime() const { return RouteTime; }
 bool IsEnemyEnabled() const { return LabEnemy.IsValid(); }
private:
 UPROPERTY(VisibleAnywhere, Category="Lab") TObjectPtr<class UDarkwellStalePropLabComponent> StaleLab;
 bool bStaleAutoStarted = false;
 void AdvanceRouteBeforeActors(UWorld* World, ELevelTick TickType, float DeltaSeconds);
 void UpdateSoftCoverage(float DeltaSeconds);
 void RunRoute(float DeltaSeconds);
 void CaptureEvidence();
 void CaptureComparisonEvidence();
 void SetEnemyEnabled(bool bEnabled);
 void RestoreComparisonTools(bool bForceTorch);
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> LabStructure;
 UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> StructureMaterials;
 UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SoftMaterial;
 UPROPERTY(Transient) TArray<TObjectPtr<UTextureRenderTarget2D>> SoftTargets;
 UPROPERTY(Transient) TObjectPtr<UTexture> RawCoverage;
 FVector2D FogMin, FogInv;
 int32 SoftIndex = 0;
 int32 LastMode = -1, LastPolicy = -1, LastRoute = -1;
 float Elapsed = 0, RouteTime = 0;
 int32 CaptureIndex = 0;
 TSharedPtr<FDarkwellLabCaptureWriter, ESPMode::ThreadSafe> CaptureWriter;
 FDelegateHandle ScreenshotHandle;
 FDelegateHandle RouteTickHandle;
 FString LastEvent = TEXT("Ready: use Darkwell.PropLab help");
 TMap<FName, FTransform> InitialTransforms;
 TWeakObjectPtr<class ADarkwellStalkerCharacter> LabEnemy;
 bool bPlayerInitialized = false;
 bool bMaintainTools = true;
};
