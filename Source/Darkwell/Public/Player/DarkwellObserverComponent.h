#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DarkwellObserverComponent.generated.h"

/** Gameplay owns the observer. SightWeave only consumes the resulting pose. */
UCLASS(ClassGroup=(Darkwell),meta=(BlueprintSpawnableComponent))
class DARKWELL_API UDarkwellObserverComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UDarkwellObserverComponent();
 /** Default standing offset from capsule feet, not a maximum observable Z. */
 UPROPERTY(EditAnywhere,Category="Observation",meta=(ClampMin="0"))
 float StandingHeightCm=162;
 FTransform GetObserverPose() const;
 bool SetObserverWorldPose(const FTransform& Pose);
 void ClearObserverWorldPose() { WorldPose.Reset(); }
 bool SetObserverWorldDirection(FRotator Direction);
 void ClearObserverWorldDirection() { WorldDirection.Reset(); }
private:
 TOptional<FTransform> WorldPose;
 TOptional<FQuat> WorldDirection;
};
