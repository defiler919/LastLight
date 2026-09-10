#include "Player/DarkwellObserverComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

UDarkwellObserverComponent::UDarkwellObserverComponent(){PrimaryComponentTick.bCanEverTick=false;}
FTransform UDarkwellObserverComponent::GetObserverPose() const
{
 const AActor* Owner=GetOwner();if(!Owner)return FTransform::Identity;
 FTransform Pose=WorldPose.IsSet()?WorldPose.GetValue():FTransform(Owner->GetActorQuat(),Owner->GetActorLocation());
 if(!WorldPose.IsSet())
 {
  const ACharacter* Character=Cast<ACharacter>(Owner);
  const double Half=Character?Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight():0;
  Pose.AddToTranslation(FVector(0,0,(FMath::IsFinite(StandingHeightCm)?FMath::Max(0.f,StandingHeightCm):162.f)-Half));
 }
 if(WorldDirection.IsSet())Pose.SetRotation(WorldDirection.GetValue());
 return Pose;
}
bool UDarkwellObserverComponent::SetObserverWorldPose(const FTransform& Pose)
{
 if(Pose.ContainsNaN() || !Pose.GetRotation().IsNormalized() || !Pose.GetScale3D().Equals(FVector::OneVector))return false;
 WorldPose=Pose;return true;
}
bool UDarkwellObserverComponent::SetObserverWorldDirection(FRotator Direction)
{if(Direction.ContainsNaN())return false;WorldDirection=Direction.Quaternion();return true;}
