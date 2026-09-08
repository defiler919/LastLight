#include "VisionPresentation/DarkwellBlackRegionSwitch.h"
#include "VisionPresentation/DarkwellBlackoutTiming.h"
#include "VisionPresentation/DarkwellBlackRegionTrigger.h"
#include "Player/DarkwellCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"

ADarkwellBlackRegionSwitch::ADarkwellBlackRegionSwitch()
{
 PrimaryActorTick.bCanEverTick=false;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
 auto* Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Console"));
 Mesh->SetupAttachment(RootComponent);
 Mesh->SetRelativeScale3D(FVector(.4,.35,.8));
 Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 if(Cube.Succeeded()) Mesh->SetStaticMesh(Cube.Object);
}

bool ADarkwellBlackRegionSwitch::CanInteract(const ADarkwellCharacter& Character) const
{
 return HasActorBegunPlay() && !IsActorBeingDestroyed() && IsValid(Target) && !Target->IsActorBeingDestroyed()
  && Character.CanAcceptGameplayInput()
  && FVector::DistSquared2D(Character.GetActorLocation(),GetActorLocation())<=FMath::Square(InteractionDistance);
}

void ADarkwellBlackRegionSwitch::Interact(ADarkwellCharacter& Character)
{
 DW_BLACKOUT_ROOT(Interact);
 if(!CanInteract(Character)) return;
 if(Target->IsActive()) Target->Deactivate();
 else if(!Target->Activate())
 {
  UE_LOG(LogTemp,Warning,TEXT("BLACK_REGION_SWITCH activation rejected: %s"),*Target->GetLastFailure());
  if(GEngine) GEngine->AddOnScreenDebugMessage(INDEX_NONE,5,FColor::Orange,Target->GetLastFailure());
 }
}

FText ADarkwellBlackRegionSwitch::GetInteractionPrompt(const ADarkwellCharacter& Character) const
{
 if(!CanInteract(Character)) return FText::GetEmpty();
 return Target->IsActive()
  ? NSLOCTEXT("Darkwell","BlackRegionSwitchActive","Black region ACTIVE - Deactivate (new observation restores memory)")
  : NSLOCTEXT("Darkwell","BlackRegionSwitchInactive","Black region INACTIVE - Activate (clear and block memory)");
}

void ADarkwellBlackRegionSwitch::ReleaseTarget()
{
 if(IsValid(Target)) Target->Deactivate();
}
void ADarkwellBlackRegionSwitch::EndPlay(EEndPlayReason::Type Reason)
{
 ReleaseTarget(); Super::EndPlay(Reason);
}
void ADarkwellBlackRegionSwitch::Destroyed()
{
 ReleaseTarget(); Super::Destroyed();
}
