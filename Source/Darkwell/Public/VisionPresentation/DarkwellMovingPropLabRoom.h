#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/DarkwellInteractable.h"
#include "Visibility/DarkwellVisionIntegrationFixture.h"
#include "VisionPresentation/DarkwellSpatialObservationHistory.h"
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
class DARKWELL_API ADarkwellMovingPropLabRoom final : public AActor
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
	UFUNCTION(BlueprintPure, Category="Lab") int32 GetTrackedIdentityCount() const { return Tracked.Num(); }
	UFUNCTION(BlueprintPure, Category="Lab") int32 GetTotalSpatialRecordCount() const;
	UFUNCTION(BlueprintPure, Category="Lab") int32 GetSpatialRecordCount(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab") bool IsActualPresent(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab") int32 GetTotalProxyCount() const;
	UFUNCTION(BlueprintPure, Category="Lab") int32 GetTotalCapTriangles() const;
	UFUNCTION(BlueprintPure, Category="Lab") FString GetTelemetry() const;
	UFUNCTION(BlueprintPure, Category="Lab") FString GetMotionState() const;
	UFUNCTION(BlueprintPure, Category="Lab") FString GetCurrentInteraction() const;
	UFUNCTION(BlueprintPure, Category="Lab") FString GetObjectPositionLabel() const;
	UFUNCTION(BlueprintPure, Category="Lab") FVector GetPressurePlatePosition() const;
	UFUNCTION(BlueprintPure, Category="Lab") FTransform GetTrackedTransform(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab") bool IsInWorldControlMode() const { return bInWorldControls; }
	bool TryDuplicateStableIdForTesting(FName StableId);
	bool DoSpatialRecordTexturesMatchForTesting(FName StableId) const;
	bool ActivateInWorldControl(EDarkwellMovingPropLabControlKind Kind, ADarkwellCharacter& Character);
	bool CanActivateInWorldControl(EDarkwellMovingPropLabControlKind Kind) const;
	bool CanFocusInWorldControl() const { return bInWorldControls && bStarted; }
	bool IsInWorldControlCompleted(EDarkwellMovingPropLabControlKind Kind) const;
	FText GetInWorldControlPrompt(EDarkwellMovingPropLabControlKind Kind) const;
	FText GetInWorldControlDisplay(EDarkwellMovingPropLabControlKind Kind) const;
	FColor GetInWorldControlColor(EDarkwellMovingPropLabControlKind Kind) const;
	ADarkwellMovingPropLabControl* GetControlForTesting(EDarkwellMovingPropLabControlKind Kind) const;
	int32 GetHiddenFreezeCountForTesting(FName StableId) const;
	int32 GetHistoricalProxyVisibilityTransitionsForTesting(FName StableId) const;
	int32 GetHistoricalProxyCreationCountForTesting(FName StableId) const;
	int32 GetHistoricalTextureUploadCountForTesting(FName StableId) const;
	uint64 GetHistoricalVisualSignatureForTesting(FName StableId) const;
	FString GetHistoricalVisualTelemetryForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetCurrentEpochCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetStaleEpochCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetVisibleHistoricalProxyCountForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") int32 GetMaxOverlapContributorsForTesting(FName StableId) const;
	UFUNCTION(BlueprintPure, Category="Lab|Diagnostics") float GetLastLegalCoverageRatioForTesting(FName StableId) const;
	float GetNewestHistoricalYawForTesting(FName StableId) const;
	bool StartTrackedRotationForTesting(FName StableId, float TargetYaw, float Duration);

	bool SelectScenario(int32 InScenario, ADarkwellCharacter* Player);
	bool AdvanceScenario(ADarkwellCharacter* Player);
	bool SetMultiCount(int32 Count, ADarkwellCharacter* Player);
	void StopMotion();

private:
	struct FRecordVisual
	{
		uint32 Epoch = 0;
		TWeakObjectPtr<AActor> Proxy;
		TWeakObjectPtr<UTexture2D> Texture;
		TWeakObjectPtr<UDynamicMeshComponent> Cap;
		TArray<FBox> PartBounds;
		uint64 CapSignature = 0;
		uint64 TextureSignature = 0;
		int32 CapTriangles = 0;
		int32 ProxyCreationCount = 0;
		int32 TextureCreationCount = 0;
		int32 TextureUploadCount = 0;
		int32 ProxyVisibilityTransitions = 0;
		FIntPoint HistoricalTextureSize = FIntPoint::ZeroValue;
		TBitArray<> SuppressedByCurrentEvidence;
		bool bHasProxyVisibilitySample = false;
		bool bLastProxyVisible = false;
	};

	struct FTrackedProp
	{
		FName StableId;
		TWeakObjectPtr<ADarkwellPropLabFurniture> Actual;
		FDarkwellSpatialObservationHistory History;
		TMap<uint32, FRecordVisual> Visuals;
		FTransform InitialTransform = FTransform::Identity;
		FTransform LastPhysicalTransform = FTransform::Identity;
		FVector Dimensions = FVector::ZeroVector;
		FLinearColor Tint = FLinearColor::Gray;
		int32 Shape = 0;
		int32 HiddenFreezeCount = 0;
		int32 MaxOverlapContributors = 0;
		float LastLegalCoverageRatio = 0.0f;
		bool bExists = false;
	};

	struct FActiveMotion
	{
		TWeakObjectPtr<ADarkwellPropLabFurniture> Prop;
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
		const FTransform& Transform);
	void DestroyTracked();
	void DestroyTracked(FName StableId);
	void DestroyVisual(FRecordVisual& Visual);
	void ConfigureInWorldProps();
	void SpawnInWorldControls();
	void DestroyInWorldControls();
	bool ResetCurrentInWorldZone();
	void ResetInWorldControlState();
	bool IsCurrentInWorldControlBusy() const;
	void MarkActiveInWorldControlCompleted();
	FString GetNextInWorldControlLabel() const;
	bool FreezeCurrentForHiddenMotion(FTrackedProp& Prop, const TCHAR* Reason);
	void StartMotion(ADarkwellPropLabFurniture* Prop, const FTransform& Target, float Duration);
	void UpdateInWorldAutomation(float DeltaSeconds, ADarkwellCharacter* Player);
	void UpdatePressurePlate(ADarkwellCharacter* Player);
	void CompleteInWorldMotionGroup();
	FName GetInWorldPropId(EDarkwellMovingPropLabControlKind Kind) const;
	FBox2D ActualBounds(const ADarkwellPropLabFurniture& Prop) const;
	TArray<FBox> ActualPartBounds(const ADarkwellPropLabFurniture& Prop) const;
	TArray<float> ConservativeCoverage(const FBox2D& Bounds) const;
	bool IsOccupiedByActual(FVector2D Point, FName IgnoredStableId) const;
	bool HasCurrentObservedContributionAt(const FTrackedProp& Prop, FVector2D Point) const;
	void UpdateHistoricalContributionExclusion(
		FTrackedProp& Prop,
		FDarkwellSpatialObservationRecord& Record);
	int32 ComputeMaxOverlapContributors(const FTrackedProp& Prop) const;
	void LogRotationFrame(const FTrackedProp& Prop) const;
	void UpdateTracked(FTrackedProp& Prop, float DeltaSeconds);
	bool SetTrackedExists(FName StableId, bool bExists);
	void EnsureRecordVisual(FTrackedProp& Prop, FDarkwellSpatialObservationRecord& Record);
	void UpdateRecordTexture(FTrackedProp& Prop, FDarkwellSpatialObservationRecord& Record);
	void UpdateRecordCap(FTrackedProp& Prop, FDarkwellSpatialObservationRecord& Record);
	AActor* SpawnMemoryProxy(const FTrackedProp& Prop, const FDarkwellSpatialObservationRecord& Record);
	void BindProxyMaterial(FTrackedProp& Prop, FDarkwellSpatialObservationRecord& Record, AActor* Proxy);
	void TeleportPlayer(ADarkwellCharacter* Player, FVector Location, float Yaw) const;
	void ConfigureScenarioProps(int32 InScenario);
	void UpdateDeterministicMotion(float DeltaSeconds);
	void Report();

	UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> Structure;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> PressurePlate;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> PressureLabel;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> OwnedMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> OwnedTextures;
	UPROPERTY(Transient) TArray<TObjectPtr<UDynamicMeshComponent>> OwnedCaps;
	TMap<FName, FTrackedProp> Tracked;
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
};
