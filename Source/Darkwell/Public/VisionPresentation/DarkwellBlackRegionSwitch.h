#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/DarkwellInteractable.h"
#include "DarkwellBlackRegionSwitch.generated.h"

class ADarkwellBlackRegionTrigger;

/** A nearby, facing F interaction. The target remains the sole owner of region rules/state. */
UCLASS(Blueprintable)
class DARKWELL_API ADarkwellBlackRegionSwitch : public AActor, public IDarkwellInteractable
{
 GENERATED_BODY()
public:
 ADarkwellBlackRegionSwitch();
 virtual bool CanInteract(const ADarkwellCharacter& Character) const override;
 virtual void Interact(ADarkwellCharacter& Character) override;
 virtual FText GetInteractionPrompt(const ADarkwellCharacter& Character) const override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void Destroyed() override;
 UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Black Region")
 TObjectPtr<ADarkwellBlackRegionTrigger> Target;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction", meta=(ClampMin="1",ClampMax="300"))
 float InteractionDistance=150;
private:
 void ReleaseTarget();
};
