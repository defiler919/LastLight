#include "VisionPresentation/DarkwellBlackRegionTrigger.h"
#include "VisionPresentation/DarkwellBlackoutTiming.h"
#include "VisionPresentation/DarkwellMemoryRegionSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_BlackRegionInactive,"Darkwell.BlackRegion.Inactive");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_BlackRegionActive,"Darkwell.BlackRegion.Active");

ADarkwellBlackRegionTrigger::ADarkwellBlackRegionTrigger()
{
 PrimaryActorTick.bCanEverTick=false;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
 BoundsGuide=CreateDefaultSubobject<UBoxComponent>(TEXT("FixedWorldXYBounds"));
 BoundsGuide->SetupAttachment(GetRootComponent()); BoundsGuide->SetAbsolute(true,true,true);
 BoundsGuide->SetCollisionEnabled(ECollisionEnabled::NoCollision); BoundsGuide->SetGenerateOverlapEvents(false);
 BoundsGuide->SetHiddenInGame(true); BoundsGuide->ShapeColor=FColor(180,70,230);
 State=TAG_BlackRegionInactive;
}

FBox2D ADarkwellBlackRegionTrigger::GetFixedBounds() const
{
 if(FixedBounds.bIsValid) return FixedBounds;
 const FVector2D Center(GetActorLocation());
 return FBox2D(Center-HalfExtentXY,Center+HalfExtentXY);
}

void ADarkwellBlackRegionTrigger::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);
 const auto B=GetFixedBounds();
 BoundsGuide->SetWorldLocation(FVector(B.GetCenter(),GetActorLocation().Z+100));
 BoundsGuide->SetWorldRotation(FRotator::ZeroRotator); BoundsGuide->SetWorldScale3D(FVector::OneVector);
 BoundsGuide->SetBoxExtent(FVector(B.GetExtent(),100));
}

bool ADarkwellBlackRegionTrigger::IsActive() const
{
 const auto* Region=GetWorld()?GetWorld()->GetSubsystem<UDarkwellMemoryRegionSubsystem>():nullptr;
 return State==TAG_BlackRegionActive && Region && Region->IsGameplayControlledBy(this) && Region->IsBlocked();
}

bool ADarkwellBlackRegionTrigger::Activate()
{
 DW_BLACKOUT_SCOPE(Activate);
 if(IsActive()) return true;
 LastFailure.Reset();
 if(!GetWorld() || !GetWorld()->IsGameWorld() || IsActorBeingDestroyed()
  || !IsActorInitialized() || (!HasActorBegunPlay() && !IsActorBeginningPlay()))
 { LastFailure=TEXT("Activation requires an initialized actor in play; ended actors cannot reacquire Block."); return false; }
 auto* Region=GetWorld()->GetSubsystem<UDarkwellMemoryRegionSubsystem>();
 const auto B=GetFixedBounds();
 if(!Region || !Region->AcquireGameplayControl(this,B.Min,B.Max))
 { LastFailure=TEXT("Region unavailable: authority not ready, invalid/changed AABB, Whole straddle, or another owner/block."); return false; }
 // Block first prevents Clear's same-call legal Live requery from being sealed
 // as new memory by a subsequent block. No tick or knowledge logic lives here.
 if(!Region->ClearAndBlockMemory())
 {
  Region->ReleaseGameplayControl(this); State=TAG_BlackRegionInactive;
  LastFailure=TEXT("Clear+Block rejected by region authority; acquired block released."); return false;
 }
 FixedBounds=B; State=TAG_BlackRegionActive;
 return true;
}

void ADarkwellBlackRegionTrigger::Deactivate()
{
 DW_BLACKOUT_SCOPE(Deactivate);
 if(auto* Region=GetWorld()?GetWorld()->GetSubsystem<UDarkwellMemoryRegionSubsystem>():nullptr)
  Region->ReleaseGameplayControl(this);
 State=TAG_BlackRegionInactive;
}

void ADarkwellBlackRegionTrigger::EndPlay(EEndPlayReason::Type Reason)
{
 Deactivate(); Super::EndPlay(Reason);
}

void ADarkwellBlackRegionTrigger::Destroyed()
{
 Deactivate(); Super::Destroyed();
}
