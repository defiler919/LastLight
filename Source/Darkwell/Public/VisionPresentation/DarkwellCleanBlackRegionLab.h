#pragma once
#include "CoreMinimal.h"
#include "Visibility/DarkwellVisionIntegrationFixture.h"
#include "DarkwellCleanBlackRegionLab.generated.h"
class ADarkwellObjectMemoryScene;
class ADarkwellBlackRegionTrigger;
class ADarkwellBlackRegionSwitch;
class UDarkwellBlackRegionEventAdapter;

/** Small manual fixture: static sources only; no scripted motion; manual F console. */
UCLASS()
class DARKWELL_API ADarkwellCleanBlackRegionLab : public ADarkwellVisionIntegrationFixture
{
 GENERATED_BODY()
public:
 ADarkwellCleanBlackRegionLab();
 virtual void BeginPlay() override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void Tick(float DeltaSeconds) override;
 virtual FBox2D GetSightWeaveFloorBounds() const override;
 virtual void BuildSightWeaveOccluderSegments(TArray<FDarkwellVisionIntegrationSegment>& Out) const override;
 virtual void BuildSightWeaveStaticSurfaces(TArray<FDarkwellVisionIntegrationSurface>& Out) const override;
 virtual bool EnableDarkwellProjectFogP4(UTexture* Raw,FVector2D Min,FVector2D Inv) override;
 virtual void DisableDarkwellProjectFog() override {}
 UPROPERTY(Transient) TObjectPtr<ADarkwellObjectMemoryScene> MemoryScene;
 UPROPERTY(Transient) TObjectPtr<ADarkwellBlackRegionTrigger> Trigger;
 UPROPERTY(Transient) TObjectPtr<ADarkwellBlackRegionSwitch> Console;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UDarkwellBlackRegionEventAdapter> DemoEvent;
 UPROPERTY(Transient) TArray<TObjectPtr<AActor>> Sources;
private:
 bool bPlayerReady=false;
};
