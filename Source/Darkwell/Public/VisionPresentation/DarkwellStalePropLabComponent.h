#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "VisionPresentation/DarkwellEmptyVerification.h"
#include "DarkwellStalePropLabComponent.generated.h"

class ADarkwellPropLabFurniture;
class UTexture2D;
class UMaterialInstanceDynamic;

/** Explicit transient experiment in the existing Lab. No production policy selection. */
UCLASS()
class DARKWELL_API UDarkwellStalePropLabComponent final : public UActorComponent
{
 GENERATED_BODY()
public:
 bool Start(int32 InMode, int32 InCase);
 void Stop();
 void BeforeActors(float DeltaSeconds);
 void AfterActors(float DeltaSeconds);
 bool IsRunning() const { return bRunning; }
 bool HasCompletedCapture() const { return bCaptureComplete; }
 static float ScanYaw(float Seconds, int32 Scenario);
 UFUNCTION(BlueprintPure, Category="Lab") FString GetStatus() const { return Status; }
private:
 void SpawnSubject();
 void SetView(float Yaw, bool bRemembering);
 bool PrepareGhost();
 void ObserveEmpty(float DeltaSeconds);
 void UpdateOpacity();
 void Report(bool bCapture);
 UPROPERTY(Transient) TObjectPtr<ADarkwellPropLabFurniture> Subject;
 UPROPERTY(Transient) TObjectPtr<UTexture2D> OpacityTexture;
 UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> GhostMaterials;
 TWeakObjectPtr<AActor> Ghost;
 TMap<TWeakObjectPtr<AActor>,bool> CollisionBackup;
 FDarkwellEmptyVerification Evidence;
 FGameplayTag Phase;
 FString Status, ClearReason=TEXT("NoEmptyEvidence");
 FName StableId;
 FVector OldPosition, Dimensions;
 float Seconds=0;
 int32 Mode=0, Scenario=0, Frame=0;
 bool bRunning=false, bMutated=false, bCaptureComplete=false;
};
