#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "DarkwellBlackRegionEventAdapter.generated.h"
class ADarkwellBlackRegionTrigger;

/** Event edges control an existing fixed trigger. F remains a manual override between edges. */
UCLASS(ClassGroup=(SightWeave), meta=(BlueprintSpawnableComponent))
class DARKWELL_API UDarkwellBlackRegionEventAdapter : public UActorComponent
{
 GENERATED_BODY()
public:
 UDarkwellBlackRegionEventAdapter();
 UFUNCTION(BlueprintCallable, Category="Black Region|Event") bool BeginEvent();
 UFUNCTION(BlueprintCallable, Category="Black Region|Event") void EndEvent();
 UFUNCTION(BlueprintPure, Category="Black Region|Event") bool IsEventStarted() const;
 UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Black Region|Event")
 TObjectPtr<ADarkwellBlackRegionTrigger> Target;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
private:
 UPROPERTY(Transient) FGameplayTag EventState;
 TWeakObjectPtr<ADarkwellBlackRegionTrigger> StartedTarget;
};
