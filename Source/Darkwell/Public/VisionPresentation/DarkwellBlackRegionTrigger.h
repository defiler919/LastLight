#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "DarkwellBlackRegionTrigger.generated.h"

class UBoxComponent;

/** Placeable fixed world-XY black region. Knowledge belongs to the region subsystem.
 * Starts inactive. One configured AABB per world; rotation/scale do not change its shape. */
UCLASS(Blueprintable, ClassGroup=(SightWeave))
class DARKWELL_API ADarkwellBlackRegionTrigger : public AActor
{
 GENERATED_BODY()
public:
 ADarkwellBlackRegionTrigger();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void Destroyed() override;
 UFUNCTION(BlueprintCallable, Category="SightWeave|Black Region") bool Activate();
 UFUNCTION(BlueprintCallable, Category="SightWeave|Black Region") void Deactivate();
 UFUNCTION(BlueprintPure, Category="SightWeave|Black Region") bool IsActive() const;
 UFUNCTION(BlueprintPure, Category="SightWeave|Black Region") FGameplayTag GetState() const { return State; }
 UFUNCTION(BlueprintPure, Category="SightWeave|Black Region") FString GetLastFailure() const { return LastFailure; }
 /** Centimetres in world axes; maximum full extent is 640 cm per axis. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SightWeave|Black Region", meta=(ClampMin="1",ClampMax="320"))
 FVector2D HalfExtentXY=FVector2D(100,100);
 /** Editor guide only. Knowledge uses the existing floor-domain XY region, not this Z height. */
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SightWeave|Black Region") TObjectPtr<UBoxComponent> BoundsGuide;
 FBox2D GetFixedBounds() const;
private:
 UPROPERTY(Transient, VisibleInstanceOnly, Category="SightWeave|Black Region") FGameplayTag State;
 UPROPERTY(Transient, VisibleInstanceOnly, Category="SightWeave|Black Region") FString LastFailure;
 FBox2D FixedBounds=FBox2D(ForceInit);
};
