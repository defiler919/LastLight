#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/DarkwellInteractable.h"
#include "Visibility/DarkwellVisionIntegrationFixture.h"
#include "VisionPresentation/DarkwellSpatialObservationHistory.h"
#include "VisionPresentation/DarkwellObjectMemoryScene.h"
#include "SightWeaveObjectPolicy.h"
#include "SightWeaveRevealObservation.h"
#include "DarkwellMovingPropLabRoom.generated.h"

class ADarkwellCharacter;
class ADarkwellPropLabFurniture;
class UDynamicMeshComponent;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UTexture2D;
class ADarkwellMovingPropLabRoom;

UENUM()
enum class EDarkwellMovingPropLabControlKind : uint8
{
	VisibleTranslate,
	VisibleRotate,
	HiddenAtoB,
	CoverageBoundary,
	AtoBtoC,
	MultiProp,
	ResetCurrent
};

/** Lab-only world interaction driven by the existing F-key interaction path. */
UCLASS()
class DARKWELL_API ADarkwellMovingPropLabControl final
	: public AActor
	, public IDarkwellInteractable
{
	GENERATED_BODY()

public:
	ADarkwellMovingPropLabControl();
	void Configure(
		ADarkwellMovingPropLabRoom* InRoom,
		EDarkwellMovingPropLabControlKind InKind,
		const FText& InLabel);
	void RefreshDisplay();

	virtual bool CanInteract(const ADarkwellCharacter& Character) const override;
	virtual void Interact(ADarkwellCharacter& Character) override;
	virtual FText GetInteractionPrompt(const ADarkwellCharacter& Character) const override;
	virtual void OnInteractionFocusChanged(bool bFocused) override;

	UFUNCTION(BlueprintPure, Category="Lab") EDarkwellMovingPropLabControlKind GetControlKind() const { return Kind; }
	UFUNCTION(BlueprintCallable, Category="Lab") bool TriggerForLabEvidence(ADarkwellCharacter* Character);
	EDarkwellMovingPropLabControlKind GetKind() const { return Kind; }

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> ControlRoot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> StatusIndicator;
	TWeakObjectPtr<ADarkwellMovingPropLabRoom> Room;
	FText BaseLabel;
	EDarkwellMovingPropLabControlKind Kind = EDarkwellMovingPropLabControlKind::VisibleTranslate;
	bool bFocused = false;
};

/** Development-only, runtime-spawned basic-geometry room for moving prop rules. */
UCLASS()
class DARKWELL_API ADarkwellMovingPropLabRoom final : public ADarkwellObjectMemoryScene
{
	GENERATED_BODY()

public:

	ADarkwellMovingPropLabRoom();
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

	static ADarkwellMovingPropLabRoom* FindActive(const UWorld* World);
	FBox2D FloorBounds() const;
	void BuildOccluders(TArray<FDarkwellVisionIntegrationSegment>& Out) const;
	void BindRoomPresentation(UTexture* Raw, FVector2D Min, FVector2D Inv);
	bool ResetRoom(ADarkwellCharacter* Player);
	void UpdateRoom(float DeltaSeconds, ADarkwellCharacter* Player);
	void Command(const TArray<FString>& Args);

	UFUNCTION(BlueprintPure, Category="Lab") FString GetStatus() const { return Status; }
	UFUNCTION(BlueprintPure, Category="Lab") int32 GetScenario() const { return Scenario; }
	UFUNCTION(BlueprintPure, Category="Lab") int32 GetScenarioPhase() const { return ScenarioPhase; }

	UFUNCTION(BlueprintPure, Category="Lab") FString GetTelemetry() const;
	UFUNCTION(BlueprintPure, Category="Lab") FString GetMotionState() const;
	UFUNCTION(BlueprintPure, Category="Lab") FString GetCurrentInteraction() const;
	UFUNCTION(BlueprintPure, Category="Lab") FString GetObjectPositionLabel() const;
	UFUNCTION(BlueprintPure, Category="Lab") FVector GetPressurePlatePosition() const;

	UFUNCTION(BlueprintPure, Category="Lab") bool IsInWorldControlMode() const { return bInWorldControls; }
	bool TryDuplicateStableIdForTesting(FName StableId);

	bool ActivateInWorldControl(EDarkwellMovingPropLabControlKind Kind, ADarkwellCharacter& Character);
	bool CanActivateInWorldControl(EDarkwellMovingPropLabControlKind Kind) const;
	bool CanFocusInWorldControl() const { return bInWorldControls && bStarted; }
	bool IsInWorldControlCompleted(EDarkwellMovingPropLabControlKind Kind) const;
	FText GetInWorldControlPrompt(EDarkwellMovingPropLabControlKind Kind) const;
	FText GetInWorldControlDisplay(EDarkwellMovingPropLabControlKind Kind) const;
	FColor GetInWorldControlColor(EDarkwellMovingPropLabControlKind Kind) const;
	UFUNCTION(BlueprintCallable, Category="Lab|Testing")
	bool TriggerInWorldControlForTesting(EDarkwellMovingPropLabControlKind Kind, ADarkwellCharacter* Character);
	ADarkwellMovingPropLabControl* GetControlForTesting(EDarkwellMovingPropLabControlKind Kind) const;

	/** On-demand development snapshot. No GPU readback and no per-frame string work. */

	/** Read-only cross-episode surface/cap evidence for the architecture audit. */

	bool ConfigureHistoricalEpochCountForTesting(FName StableId, int32 HistoricalEpochs);

	UFUNCTION(BlueprintCallable, Category="Lab|Testing")
	bool StartTrackedRotationForTesting(FName StableId, float TargetYaw, float Duration);

	/** Explicit per-object reset/re-registration, never an in-place mode change. */
	UFUNCTION(BlueprintCallable, Category="Lab|History Policy")
	bool ResetTrackedPolicyForLab(FName StableId, ESightWeaveHistoryMode Mode);
	UFUNCTION(BlueprintCallable, Category="Lab|Object Policy")
	bool ResetTrackedRevealPolicyForLab(FName StableId, ESightWeaveRevealMode RevealMode, float MinimumSpanCm, ESightWeaveHistoryMode HistoryMode);

	bool SelectScenario(int32 InScenario, ADarkwellCharacter* Player);
	bool AdvanceScenario(ADarkwellCharacter* Player);
	bool SetMultiCount(int32 Count, ADarkwellCharacter* Player);
	void StopMotion();
	/** Dedicated V2-map entry points. They retain the same authoritative history path. */
	bool ConfigureForGrayPolicyLab(ADarkwellCharacter* Player);
	bool ResetGrayPolicyRoom(int32 RoomIndex);
	bool ToggleGrayPolicyMovingSubject();
	UFUNCTION(BlueprintCallable, Category="Lab|Testing")
	bool StartGrayPolicyMotion(bool bRotate);
	UFUNCTION(BlueprintCallable, Category="Lab|Testing")
	bool SetGrayPolicyStressMode(int32 Mode);

	FName GetGrayPolicyMovingSubject() const;

private:
	friend class FDarkwellCapPartialClipTest;
	friend class FDarkwellCapCoplanarContactTest;
	friend class FDarkwellGrayHistoryCapacityCurrentTest;
	friend class FDarkwellPlanarProjectionParity;
	friend class FDarkwellRepeatedHistoryEvidenceParity;
	friend class FDarkwellMemoryEpisodeContract;
	friend class FDarkwellObservedContentContract;
	struct FActiveMotion
	{
		TWeakObjectPtr<AActor> Prop;
		FTransform Start = FTransform::Identity;
		FTransform End = FTransform::Identity;
		float Seconds = 0.0f;
		float Duration = 0.0f;
	};

	ADarkwellPropLabFurniture* SpawnTracked(
		FName StableId,
		int32 Shape,
		FVector Dimensions,
		FLinearColor Tint,
		const FTransform& Transform,
		ESightWeaveObjectPolicySource PolicySource = ESightWeaveObjectPolicySource::UseProjectDefault,
		ESightWeaveHistoryMode HistoryMode = ESightWeaveHistoryMode::Always,
		const FResolvedSightWeaveObjectPolicy* PerFieldPolicy = nullptr);

	void DestroyTracked();
	void DestroyTracked(FName StableId);

	void ConfigureInWorldProps();
	void SpawnInWorldControls();
	void DestroyInWorldControls();
	bool ResetCurrentInWorldZone();
	void ResetInWorldControlState();
	bool IsCurrentInWorldControlBusy() const;
	void MarkActiveInWorldControlCompleted();
	FString GetNextInWorldControlLabel() const;

	void StartMotion(AActor* Prop, const FTransform& Target, float Duration);
	void UpdateInWorldAutomation(float DeltaSeconds, ADarkwellCharacter* Player);
	void UpdatePressurePlate(ADarkwellCharacter* Player);
	void CompleteInWorldMotionGroup();
	FName GetInWorldPropId(EDarkwellMovingPropLabControlKind Kind) const;

	virtual void LogRotationFrame(const FTrackedProp& Prop) const override;

	bool SetTrackedExists(FName StableId, bool bExists);

	void TeleportPlayer(ADarkwellCharacter* Player, FVector Location, float Yaw) const;
	void ConfigureScenarioProps(int32 InScenario);
	void ConfigureGrayPolicyLabProps();
	void DestroyGrayStressProps();
	void UpdateDeterministicMotion(float DeltaSeconds);

	void Report();

	UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> Structure;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PressurePlate;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> PressureLabel;
	UPROPERTY(Transient) TArray<TObjectPtr<ADarkwellMovingPropLabControl>> InWorldControls;
	TArray<FActiveMotion> ActiveMotions;
	TSet<EDarkwellMovingPropLabControlKind> CompletedInWorldControls;
	FString Status;
	FString CurrentInteraction = TEXT("NONE");
	EDarkwellMovingPropLabControlKind ActiveControl = EDarkwellMovingPropLabControlKind::VisibleTranslate;
	float AutoDelaySeconds = 0.0f;
	int32 Scenario = 0;
	int32 ScenarioPhase = 0;
	int32 MultiCount = 0;
	int32 HiddenMoveIndex = 0;
	bool bMotionActive = false;
	bool bStarted = false;
	bool bInWorldControls = false;
	bool bInWorldScenarioSelected = false;
	bool bInWorldFinished = false;
	bool bPressureWaitingForExit = false;
	bool bGrayPolicyLab = false;
	bool bGrayPartialMovingActive = false;
	int32 GrayStressMode = 0;
};
