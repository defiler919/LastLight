#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Visibility/DarkwellVisionIntegrationFixture.h"
#include "VisionPresentation/DarkwellEmptyVerification.h"
#include "DarkwellManualStaleRoom.generated.h"

class ADarkwellPropLabFurniture;
class ADarkwellCharacter;
class UTexture2D;
class UMaterialInstanceDynamic;
class UDarkwellMode2SolidComponent;

/** Physical room and pressure input only exist in the independent development Lab. */
UCLASS()
class DARKWELL_API ADarkwellManualStaleRoom final : public AActor
{
 GENERATED_BODY()
public:
 ADarkwellManualStaleRoom();
 virtual void BeginPlay() override;
 static ADarkwellManualStaleRoom* FindActive(const UWorld* World);
 static FName CabinetId();
 FBox2D FloorBounds() const;
 void BuildOccluders(TArray<FDarkwellVisionIntegrationSegment>& Out) const;
 void BindRoomPresentation(UTexture* Raw, FVector2D Min, FVector2D Inv);
 bool ResetRoom(ADarkwellCharacter* Player);
 void UpdateObservation(float DeltaSeconds, ADarkwellCharacter* Player);
 void EnforceMode2Presentation(AActor* Snapshot);
 void Command(const TArray<FString>& Args);
 void TeleportPlayer(ADarkwellCharacter* Player, bool bTop);
 bool IsStarted() const { return bStarted; }
 UFUNCTION(BlueprintPure, Category="Lab") FString GetStatus() const { return Status; }
 UFUNCTION(BlueprintPure, Category="Lab") bool HasActualCabinet() const;
 UFUNCTION(BlueprintPure, Category="Lab") int32 GetToggleCount() const { return ToggleCount; }
 UFUNCTION(BlueprintPure, Category="Lab") float GetVerifiedFraction() const { return Evidence.VerifiedFraction(); }
 UFUNCTION(BlueprintPure, Category="Lab") float GetRemainingOpacity() const;
 UFUNCTION(BlueprintPure, Category="Lab") bool IsSwitchArmed() const;
 UFUNCTION(BlueprintPure, Category="Lab") float GetCabinetCoverage() const;
 UFUNCTION(BlueprintPure, Category="Lab") UDarkwellMode2SolidComponent* GetMode2Presentation() const { return Mode2Presentation; }
 // With the accepted yaw=90 top-down camera, world +X is screen-left.
 FVector CabinetPosition() const { return GetActorLocation()+FVector(500,500,0); }
 FVector SwitchPosition() const { return GetActorLocation()+FVector(500,-450,0); }
 static constexpr float SwitchRadius=100.f;
private:
 UPROPERTY(VisibleAnywhere) TObjectPtr<UDarkwellMode2SolidComponent> Mode2Presentation;
 void SpawnActualCabinet();
 void ToggleActualCabinet(); // Deliberately has no memory/evidence API access.
 void AttachObservedSnapshot(AActor* Proxy);
 void ApplyErasure(int32 Mode);
 void Report();
 UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> Structure;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PressureDisc;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> CabinetPreview;
 UPROPERTY(Transient) TObjectPtr<ADarkwellPropLabFurniture> Cabinet;
 UPROPERTY(Transient) TObjectPtr<UTexture2D> OpacityTexture;
 UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
 TWeakObjectPtr<AActor> ObservedProxy;
 FDarkwellEmptyVerification Evidence;
 TArray<float> DisplayedOpacity;
 FGameplayTag PressureState;
 FString Status;
 float Seconds=0;
 int32 ToggleCount=0;
 bool bStarted=false;
};
