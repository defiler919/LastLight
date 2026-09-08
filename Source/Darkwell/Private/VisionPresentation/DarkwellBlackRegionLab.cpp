#include "VisionPresentation/DarkwellBlackRegionTrigger.h"
#include "VisionPresentation/DarkwellBlackRegionEventAdapter.h"
#include "VisionPresentation/DarkwellCleanBlackRegionLab.h"
#include "VisionPresentation/DarkwellMemoryRegionSubsystem.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "VisionPresentation/DarkwellMovingPropLabRoom.h"
#include "VisionPresentation/DarkwellRememberablePropComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
namespace
{
 const FName DemoName(TEXT("BlackRegionLabTrigger"));
 const FName CabinetId(TEXT("Lab.InWorld.Rotate.Cabinet"));
 void Feedback(const FString& Message)
 {
  UE_LOG(LogTemp,Display,TEXT("BLACK_REGION_LAB %s"),*Message);
  if(GEngine) GEngine->AddOnScreenDebugMessage(0x424C4143,12,FColor::Cyan,Message);
 }
 ADarkwellBlackRegionTrigger* FindDemo(UWorld* World)
 {
  for(TActorIterator<ADarkwellBlackRegionTrigger> It(World);It;++It) if(It->GetFName()==DemoName) return *It;
  return nullptr;
 }
 FAutoConsoleCommandWithWorldAndArgs BlackRegionLab(TEXT("Darkwell.BlackRegionLab"),
  TEXT("open | event_begin | event_end | event_status | setup partial/whole | activate | deactivate | status. Single fixed AABB; reopen world to change mode."),
  FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
  {
   if(!World || !World->IsGameWorld()) { Feedback(TEXT("Use a running game/PIE world.")); return; }
   const FString Command=Args.IsEmpty()?TEXT("help"):Args[0].ToLower();
   if(Command==TEXT("open"))
   {
    UGameplayStatics::OpenLevel(World,TEXT("/Game/Maps/L_BlackRegionLab")); return;
   }
   if(!Darkwell::PropLab::IsLabWorld(World)) { Feedback(TEXT("Enter demo: Darkwell.BlackRegionLab open")); return; }
   if(Command==TEXT("event_begin") || Command==TEXT("event_end") || Command==TEXT("event_status"))
   {
    for(TActorIterator<ADarkwellCleanBlackRegionLab> It(World);It;++It)
    {
     auto* Event=It->DemoEvent.Get();
     if(!IsValid(Event) || !IsValid(It->Trigger))
     { Feedback(TEXT("Lab event/target was removed; reopen the Lab to run another event.")); return; }
     if(Command==TEXT("event_begin") && !Event->BeginEvent())
     { Feedback(TEXT("Lab blackout event rejected: target/authority not ready.")); return; }
     if(Command==TEXT("event_end")) Event->EndEvent();
     Feedback(FString::Printf(TEXT("Lab blackout event=%s trigger=%s; repeated edges are idempotent; F remains manual override."),
      Event->IsEventStarted()?TEXT("STARTED"):TEXT("IDLE"),It->Trigger->IsActive()?TEXT("ACTIVE"):TEXT("INACTIVE")));
     return;
    }
    Feedback(TEXT("The blackout test event requires the clean Black Region Lab.")); return;
   }
   auto* Demo=FindDemo(World);
   if(Command==TEXT("setup"))
   {
    if(Demo || World->GetSubsystem<UDarkwellMemoryRegionSubsystem>()->IsConfigured())
    { Feedback(TEXT("Demo/region already exists. Use activate/deactivate; reopen world to change its fixed box.")); return; }
    if(Args.Num()!=2 || (Args[1]!=TEXT("partial") && Args[1]!=TEXT("whole")))
    { Feedback(TEXT("Use: Darkwell.BlackRegionLab setup partial (or whole)")); return; }
    auto* Room=ADarkwellMovingPropLabRoom::FindActive(World);
    ADarkwellPropLabFurniture* Cabinet=nullptr;
    for(TActorIterator<ADarkwellPropLabFurniture> It(World);It;++It) if(It->StableId==CabinetId) Cabinet=*It;
    if(!Room || !Cabinet) { Feedback(TEXT("Enter in-world controls first: Darkwell.BlackRegionLab open")); return; }
    const bool Whole=Args[1]==TEXT("whole");
    if(!Room->ResetTrackedRevealPolicyForLab(CabinetId,Whole?ESightWeaveRevealMode::WholeObjectAfterSpan:ESightWeaveRevealMode::SpatialPartial,100,ESightWeaveHistoryMode::StationaryOnly)) return;
    auto Pose=Room->GetTrackedTransform(CabinetId); Pose.SetRotation(FRotator(0,37,0).Quaternion());
    if(!Room->SetTrackedTransformForTesting(CabinetId,Pose)) return;
    FBox Physical(ForceInit);
    for(const auto& Part:Cabinet->Memory->GetMemoryPrimitives())
     if(Part && Part->GetStaticMesh()) Physical+=Part->GetStaticMesh()->GetBoundingBox().TransformBy(UDarkwellRememberablePropComponent::GetPrimitiveTransform(*Part));
    const FVector2D Center(Physical.GetCenter());
    const FBox2D B=Whole?FBox2D(FVector2D(Physical.Min)-FVector2D(5),FVector2D(Physical.Max)+FVector2D(5)):
     FBox2D(FVector2D(Center.X-19.83,Physical.Min.Y-5),FVector2D(Center.X+20.17,Physical.Max.Y+5));
    FActorSpawnParameters Spawn; Spawn.Name=DemoName;
    Demo=World->SpawnActor<ADarkwellBlackRegionTrigger>(FVector(B.GetCenter(),0),FRotator::ZeroRotator,Spawn);
    if(!Demo) return;
    Demo->HalfExtentXY=B.GetExtent(); Demo->OnConstruction(Demo->GetActorTransform());
    Demo->BoundsGuide->SetHiddenInGame(false); // Non-colliding Lab boundary guide only.
    Feedback(TEXT("INACTIVE. Observe the 37-degree cabinet, turn away; activate; observe/leave; deactivate; observe again. WASD + mouse; console: Darkwell.BlackRegionLab activate/deactivate/status."));
    return;
   }
   if(!Demo) { Feedback(TEXT("Use Darkwell.BlackRegionLab setup partial (or whole) first.")); return; }
   if(Command==TEXT("activate") && !Demo->Activate()) { Feedback(Demo->GetLastFailure()); return; }
   if(Command==TEXT("deactivate")) Demo->Deactivate();
   if(Command==TEXT("status") || Command==TEXT("activate") || Command==TEXT("deactivate"))
   { Feedback(Demo->IsActive()?TEXT("ACTIVE: Clear+Block. Live stays normal; leaving Live is Unknown."):TEXT("INACTIVE: Block released. Cleared memory returns only after new legal observation.")); return; }
   Feedback(TEXT("Darkwell.BlackRegionLab: open | event_begin | event_end | event_status | setup partial/whole | activate | deactivate | status"));
  }));
}
#endif
