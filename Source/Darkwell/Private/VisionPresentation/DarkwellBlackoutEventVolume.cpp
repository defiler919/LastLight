#include "VisionPresentation/DarkwellBlackoutEventVolume.h"
#include "VisionPresentation/DarkwellBlackRegionEventAdapter.h"
#include "VisionPresentation/DarkwellBlackRegionTrigger.h"
#include "Player/DarkwellCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

ADarkwellBlackoutEventVolume::ADarkwellBlackoutEventVolume()
{
 PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bStartWithTickEnabled=false;
 PrimaryActorTick.TickGroup=TG_PostUpdateWork;
 EventBounds=CreateDefaultSubobject<UBoxComponent>(TEXT("EventBounds")); SetRootComponent(EventBounds);
 EventBounds->SetBoxExtent(FVector(150,140,140));
 EventBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 EventBounds->SetCollisionObjectType(ECC_WorldDynamic);
 EventBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
 EventBounds->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
 EventBounds->SetGenerateOverlapEvents(true);
 EventAdapter=CreateDefaultSubobject<UDarkwellBlackRegionEventAdapter>(TEXT("EventAdapter"));
}
void ADarkwellBlackoutEventVolume::BeginPlay()
{
 Super::BeginPlay();
 EventBounds->OnComponentBeginOverlap.AddDynamic(this,&ThisClass::Enter);
 EventBounds->OnComponentEndOverlap.AddDynamic(this,&ThisClass::Leave);
 EventBounds->UpdateOverlaps(); Reconcile();
}
void ADarkwellBlackoutEventVolume::Reconcile()
{
 if(!IsValid(EventAdapter) || IsActorBeingDestroyed() || (!HasActorBegunPlay() && !IsActorBeginningPlay()) || GetWorld()->bIsTearingDown) return;
 TArray<AActor*> Actors; EventBounds->GetOverlappingActors(Actors,ADarkwellCharacter::StaticClass());
 for(AActor* Actor:Actors)
 {
  auto* Player=CastChecked<ADarkwellCharacter>(Actor);
  if(!Player->IsAlive() || Player->IsActorBeingDestroyed() || !EventBounds->IsOverlappingComponent(Player->GetCapsuleComponent())) continue;
  if(Participant.IsValid()) return; // Duplicate component/body notifications are not new edges.
  Participant=Player;
  DeathHandle=Player->OnDied.AddUObject(this,&ThisClass::Finish);
  Player->OnEndPlay.AddDynamic(this,&ThisClass::ParticipantEnded);
  // Startup overlap can precede floor authority. Retry only this pending entry,
  // never an accepted event or a later manual F override.
  const bool Started=EventAdapter->BeginEvent();
  UE_LOG(LogTemp,Display,TEXT("BLACKOUT_VOLUME entry=%s started=%d adapter_begun=%d target=%s failure=%s"),*Player->GetName(),Started,EventAdapter->HasBegunPlay(),*GetNameSafe(EventAdapter->Target),IsValid(EventAdapter->Target)?*EventAdapter->Target->GetLastFailure():TEXT("missing target"));
  SetActorTickEnabled(!Started); return;
 }
}
void ADarkwellBlackoutEventVolume::Enter(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent* Part,int32,bool,const FHitResult&)
{
 auto* Player=Cast<ADarkwellCharacter>(Other);
 if(Player && Part==Player->GetCapsuleComponent()) Reconcile();
}
void ADarkwellBlackoutEventVolume::Leave(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent* Part,int32)
{
 auto* Player=Participant.Get();
 if(Player && Other==Player && Part==Player->GetCapsuleComponent()
  && !EventBounds->IsOverlappingComponent(Part)) Finish();
}
void ADarkwellBlackoutEventVolume::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 auto* Player=Participant.Get();
 if(!IsValid(EventAdapter) || !Player || !Player->IsAlive() || !EventBounds->IsOverlappingComponent(Player->GetCapsuleComponent())) { Finish(); return; }
 if(EventAdapter->BeginEvent()) SetActorTickEnabled(false);
}
void ADarkwellBlackoutEventVolume::Finish()
{
 if(!Participant.IsExplicitlyNull()) UE_LOG(LogTemp,Display,TEXT("BLACKOUT_VOLUME end participant=%s"),*GetNameSafe(Participant.Get()));
 SetActorTickEnabled(false);
 if(auto* Player=Participant.Get())
 {
  Player->OnDied.Remove(DeathHandle);
  Player->OnEndPlay.RemoveDynamic(this,&ThisClass::ParticipantEnded);
 }
 DeathHandle.Reset(); Participant.Reset();
 if(IsValid(EventAdapter)) EventAdapter->EndEvent();
}
void ADarkwellBlackoutEventVolume::ParticipantEnded(AActor*,EEndPlayReason::Type) { Finish(); }
void ADarkwellBlackoutEventVolume::EndPlay(EEndPlayReason::Type Reason) { Finish(); Super::EndPlay(Reason); }
void ADarkwellBlackoutEventVolume::Destroyed() { Finish(); Super::Destroyed(); }
