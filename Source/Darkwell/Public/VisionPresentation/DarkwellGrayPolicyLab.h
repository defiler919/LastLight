#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "Interaction/DarkwellInteractable.h"
#include "Visibility/DarkwellVisionIntegrationFixture.h"
#include "DarkwellGrayPolicyLab.generated.h"

class ADarkwellCharacter;
class ADarkwellMovingPropLabRoom;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;
class UTextBlock;
class UWidgetComponent;
class STextBlock;
class SWidget;

struct FDarkwellGrayPolicyHitchFrame
{
	double TimestampSeconds = 0.0;
	uint64 Frame = 0;
	float FrameMs = 0.0f;
	float ViewYawDelta = 0.0f;
	float ObserverMovementDelta = 0.0f;
	uint64 CoverageRevision = 0;
	uint64 GeometryRevision = 0;
	int32 HistoryCount = 0;
	int32 CandidateCount = 0;
	int32 ActiveCount = 0;
	int32 SleepingCount = 0;
	int32 DirtyTileCount = 0;
	int64 WorkingSetDeltaBytes = 0;
	int32 UObjectDelta = 0;
	bool bGarbageCollecting = false;
};

namespace Darkwell::GrayPolicyLab
{
	inline constexpr TCHAR MapPath[] = TEXT("/Game/Maps/L_SightWeaveGrayPolicyLab");
	DARKWELL_API bool IsWorld(const UWorld* World);
}

UENUM()
enum class EDarkwellGrayPolicyLabControlKind : uint8
{
	EnterWhole,
	EnterPartial,
	EnterMoving,
	EnterNever,
	EnterOcclusion,
	EnterStress,
	ReturnToLobby,
	ResetCurrentRoom,
	ToggleMovingSubject,
	StartMove,
	StartRotate,
	AutoSweep90,
	AutoSweep160,
	RepeatSweep,
	CycleStressMode,
	ResetStress
};

/** CJK-capable world-space label backed by Unreal's bundled DroidSansFallback. */
UCLASS()
class DARKWELL_API UDarkwellGrayPolicyWorldLabelWidget final : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetLabel(const FText& InLabel, int32 InFontSize = 28);
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	FText Label;
	int32 FontSize = 28;
	TSharedPtr<STextBlock> LabelText;
};

class ADarkwellSightWeaveGrayPolicyLabDirector;

/** One native F-key control. The Director owns all room rules. */
UCLASS()
class DARKWELL_API ADarkwellGrayPolicyLabControl final
	: public AActor
	, public IDarkwellInteractable
{
	GENERATED_BODY()

public:
	ADarkwellGrayPolicyLabControl();
	void Configure(ADarkwellSightWeaveGrayPolicyLabDirector* InDirector,
		EDarkwellGrayPolicyLabControlKind InKind, const FText& InLabel);
	void FaceLabelToward(FVector WorldTarget);
	EDarkwellGrayPolicyLabControlKind GetKind() const { return Kind; }

	virtual bool CanInteract(const ADarkwellCharacter& Character) const override;
	virtual void Interact(ADarkwellCharacter& Character) override;
	virtual FText GetInteractionPrompt(const ADarkwellCharacter& Character) const override;
	virtual void OnInteractionFocusChanged(bool bFocused) override;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> ControlRoot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UWidgetComponent> LabelComponent;
	TWeakObjectPtr<ADarkwellSightWeaveGrayPolicyLabDirector> Director;
	FText Label;
	EDarkwellGrayPolicyLabControlKind Kind = EDarkwellGrayPolicyLabControlKind::ReturnToLobby;
	bool bFocused = false;
};

/** Independent, runtime-built Chinese manual-acceptance lab for the gray object policy. */
UCLASS()
class DARKWELL_API ADarkwellSightWeaveGrayPolicyLabDirector final
	: public ADarkwellVisionIntegrationFixture
{
	GENERATED_BODY()

public:
	ADarkwellSightWeaveGrayPolicyLabDirector();
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FBox2D GetSightWeaveFloorBounds() const override;
	virtual void BuildSightWeaveOccluderSegments(
		TArray<FDarkwellVisionIntegrationSegment>& OutSegments) const override;
	virtual void BuildSightWeaveStaticSurfaces(
		TArray<FDarkwellVisionIntegrationSurface>& OutSurfaces) const override;
	virtual bool EnableDarkwellProjectFogP4(
		UTexture* LiveCoverageTexture, FVector2D WorldMin, FVector2D InvWorldExtent) override;
	virtual void DisableDarkwellProjectFog() override;

	bool ActivateControl(EDarkwellGrayPolicyLabControlKind Kind, ADarkwellCharacter& Character);
	bool CanActivateControl(EDarkwellGrayPolicyLabControlKind Kind) const;
	FText GetControlPrompt(EDarkwellGrayPolicyLabControlKind Kind) const;

	UFUNCTION(BlueprintPure, Category="Gray Policy Lab") int32 GetCurrentRoom() const { return CurrentRoom; }
	UFUNCTION(BlueprintPure, Category="Gray Policy Lab") int32 GetStressMode() const { return StressMode; }
	UFUNCTION(BlueprintPure, Category="Gray Policy Lab") bool IsStressActive() const { return StressMode != 0; }
	UFUNCTION(BlueprintPure, Category="Gray Policy Lab") FString GetChineseGuidanceForTesting() const;
	UFUNCTION(BlueprintPure, Category="Gray Policy Lab") FString GetRuntimeStatusForTesting() const;
	UFUNCTION(BlueprintPure, Category="Gray Policy Lab") int32 GetRoomControlCountForTesting() const;
	UFUNCTION(BlueprintCallable, Category="Gray Policy Lab|Testing") bool SetStressModeForTesting(int32 Mode);
	UFUNCTION(BlueprintCallable, Category="Gray Policy Lab|Testing") void StartSweepForTesting(float Degrees, bool bRepeat);
	UFUNCTION(BlueprintCallable, Category="Gray Policy Lab|Testing")
	bool TeleportToRoomForTesting(int32 Room, ADarkwellCharacter* Character);
	UFUNCTION(BlueprintCallable, Category="Gray Policy Lab|Testing")
	bool ResetCurrentRoomForTesting(ADarkwellCharacter* Character);
	/** Reads the actual game viewport; the global Shot flag can be consumed by an editor viewport. */
	UFUNCTION(BlueprintCallable, Category="Gray Policy Lab|Testing")
	bool CaptureGameViewportForTesting(const FString& Filename);
	ADarkwellMovingPropLabRoom* GetRuntimeRoomForTesting() const { return RuntimeRoom.Get(); }
	static FVector GetRoomCenterForTesting(int32 Room);
	static int32 GetExpectedControlCountForTesting() { return 27; }
	static TArray<FName> GetStableIdsForRoomForTesting(int32 Room);

private:
	void BuildEnvironment();
	void BuildControls();
	void AddAreaFloor(FVector Center, FVector2D Size, const FLinearColor& Tint);
	void AddWall(FVector Center, FVector Size, const FLinearColor& Tint, bool bOccluder);
	ADarkwellGrayPolicyLabControl* AddControl(
		EDarkwellGrayPolicyLabControlKind Kind, const FText& Label, FVector Location);
	void TeleportPlayer(ADarkwellCharacter& Character, int32 Room);
	void ResetRoom(int32 Room);
	void StartSweep(float Degrees, bool bRepeat);
	void UpdateSweep(float DeltaSeconds, ADarkwellCharacter& Character);
	void AttachScreenGuidance();
	void DetachScreenGuidance();
	FText BuildScreenGuidance() const;
	FText BuildRoomGuidance(int32 Room) const;
	FString ResolveCheckoutSha() const;
	void UpdateFrameStatistics(float DeltaSeconds);
	void EmitHitchIfNeeded(float DeltaSeconds);
	void RefreshSpatialPresentation();

	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> LabMeshes;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> LabMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<ADarkwellGrayPolicyLabControl>> Controls;
	TArray<FDarkwellVisionIntegrationSegment> Occluders;
	TWeakObjectPtr<ADarkwellMovingPropLabRoom> RuntimeRoom;
	TSharedPtr<SWidget> ScreenGuidance;
	TArray<float> FrameTimesMs;
	TArray<FDarkwellGrayPolicyHitchFrame> HitchRing;
	FString CheckoutSha;
	FString CachedStatistics;
	FVector2D FogMin = FVector2D::ZeroVector;
	FVector2D FogInv = FVector2D::ZeroVector;
	UPROPERTY(Transient) TObjectPtr<UTexture> RawCoverage;
	int32 CurrentRoom = 0;
	int32 StressMode = 0;
	int32 StatisticsFrame = 0;
	int32 HitchRingNext = 0;
	int32 PreviousUObjectCount = 0;
	uint64 PreviousWorkingSetBytes = 0;
	float StatisticsAccumulator = 0.0f;
	float PreviousViewYaw = 0.0f;
	FVector PreviousObserverLocation = FVector::ZeroVector;
	float SweepStartYaw = 0.0f;
	float SweepTargetYaw = 0.0f;
	float SweepElapsed = 0.0f;
	float SweepDuration = 0.0f;
	bool bRepeatSweep = false;
	bool bInitialized = false;
	bool bHasPreviousHitchFrame = false;
};
