#pragma once
#include "CoreMinimal.h"
#include "Visibility/DarkwellVisionIntegrationFixture.h"
#include "SightWeaveTypes.h"
#include "DarkwellApartmentLab.generated.h"
class ADarkwellDoor;
class ADarkwellObjectMemoryScene;
class ADarkwellBlackRegionTrigger;
class UStaticMeshComponent;
class UDarkwellStaticEnvironmentSubsystem;

/** Product-scale manual fixture; uses the existing adapter, object memory and F actors. */
UCLASS()
class DARKWELL_API ADarkwellApartmentLab : public ADarkwellVisionIntegrationFixture
{
 GENERATED_BODY()
public:
 ADarkwellApartmentLab();
 virtual void BeginPlay() override;
 virtual void Tick(float Dt) override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual FBox2D GetSightWeaveFloorBounds() const override;
 virtual void BuildSightWeaveOccluderSegments(TArray<FDarkwellVisionIntegrationSegment>& Out) const override;
 virtual void BuildSightWeaveStaticSurfaces(TArray<FDarkwellVisionIntegrationSurface>& Out) const override { Out.Reset(); }
 virtual bool HasDynamicSightWeaveOccluders() const override { return true; }
 virtual bool EnableDarkwellProjectFogP4(UTexture* Raw,FVector2D Min,FVector2D Inv) override;
 virtual void DisableDarkwellProjectFog() override {}
 UFUNCTION(BlueprintCallable,Category="Lab|Testing") void SetDoorsOpenForTesting(bool Open);
 UFUNCTION(BlueprintPure,Category="Lab|Testing") UDarkwellStaticEnvironmentSubsystem* GetStaticKnowledge() const;
 UFUNCTION(BlueprintCallable,Category="Lab|Testing") void SetObserverPoseForTesting(FVector Location,float Yaw);
 UPROPERTY(Transient) TObjectPtr<ADarkwellObjectMemoryScene> MemoryScene;
 UPROPERTY(Transient) TObjectPtr<ADarkwellBlackRegionTrigger> Trigger;
 UPROPERTY(Transient) TArray<TObjectPtr<ADarkwellDoor>> Doors;
 UPROPERTY(Transient) TArray<TObjectPtr<AActor>> Sources;
private:
 void RegisterSource(AActor* Actor,FName Id,FLinearColor Tint,bool Whole,bool Moving=false);
 AActor* Box(FName Id,FVector Location,FVector Size,FLinearColor Tint,bool Whole=false,float Yaw=0,bool Immutable=false);
 TArray<FDarkwellVisionIntegrationSegment> FixedSegments;
 FSightWeaveIlluminationSourceHandle EnvironmentLight;
 bool bPlayerReady=false;
};
