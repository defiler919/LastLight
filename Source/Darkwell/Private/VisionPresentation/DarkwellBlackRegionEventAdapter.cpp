#include "VisionPresentation/DarkwellBlackRegionEventAdapter.h"
#include "VisionPresentation/DarkwellBlackoutTiming.h"
#include "VisionPresentation/DarkwellBlackRegionTrigger.h"
#include "Engine/World.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EventIdle,"Darkwell.BlackRegion.Event.Idle");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_EventStarted,"Darkwell.BlackRegion.Event.Started");

UDarkwellBlackRegionEventAdapter::UDarkwellBlackRegionEventAdapter()
{
 PrimaryComponentTick.bCanEverTick=false;
 EventState=TAG_EventIdle;
}
bool UDarkwellBlackRegionEventAdapter::IsEventStarted() const
{
 return EventState==TAG_EventStarted;
}
bool UDarkwellBlackRegionEventAdapter::BeginEvent()
{
 DW_BLACKOUT_SCOPE(BeginEvent);
 if(!HasBegunPlay() || !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed()
  || !GetWorld() || GetWorld()->bIsTearingDown) return false;
 // Duplicate notifications never re-clear, even after a manual F override.
 if(IsEventStarted()) return StartedTarget.IsValid();
 if(!IsValid(Target) || Target->GetWorld()!=GetWorld() || !Target->Activate()) return false;
 StartedTarget=Target; EventState=TAG_EventStarted;
 return true;
}
void UDarkwellBlackRegionEventAdapter::EndEvent()
{
 DW_BLACKOUT_SCOPE(EndEvent);
 if(!IsEventStarted()) return;
 if(auto* Trigger=StartedTarget.Get()) Trigger->Deactivate();
 StartedTarget.Reset(); EventState=TAG_EventIdle;
}
void UDarkwellBlackRegionEventAdapter::EndPlay(EEndPlayReason::Type Reason)
{
 EndEvent(); Super::EndPlay(Reason);
}
void UDarkwellBlackRegionEventAdapter::OnComponentDestroyed(bool bDestroyingHierarchy)
{
 EndEvent(); Super::OnComponentDestroyed(bDestroyingHierarchy);
}
