#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarkwellBlackoutEventVolume.generated.h"
class UBoxComponent;
class UPrimitiveComponent;
class ADarkwellCharacter;
class UDarkwellBlackRegionEventAdapter;

/** Single-player event box, independent of the target's fixed knowledge AABB. */
UCLASS()
class DARKWELL_API ADarkwellBlackoutEventVolume : public AActor
{
 GENERATED_BODY()
public:
 ADarkwellBlackoutEventVolume();
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 virtual void EndPlay(EEndPlayReason::Type Reason) override;
 virtual void Destroyed() override;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Blackout Event") TObjectPtr<UBoxComponent> EventBounds;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Blackout Event") TObjectPtr<UDarkwellBlackRegionEventAdapter> EventAdapter;
private:
 UFUNCTION() void Enter(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,int32 BodyIndex,bool bSweep,const FHitResult& Hit);
 UFUNCTION() void Leave(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,int32 BodyIndex);
 UFUNCTION() void ParticipantEnded(AActor* Actor,EEndPlayReason::Type Reason);
 void Reconcile();
 void Finish();
 TWeakObjectPtr<ADarkwellCharacter> Participant;
 FDelegateHandle DeathHandle;
};
