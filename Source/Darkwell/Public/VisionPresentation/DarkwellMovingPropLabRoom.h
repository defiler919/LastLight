#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Visibility/DarkwellVisionIntegrationFixture.h"
#include "VisionPresentation/DarkwellSpatialObservationHistory.h"
#include "DarkwellMovingPropLabRoom.generated.h"

class ADarkwellCharacter;
class ADarkwellPropLabFurniture;
class UDynamicMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTexture2D;

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
	bool TryDuplicateStableIdForTesting(FName StableId);

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
		int32 CapTriangles = 0;
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
		bool bExists = false;
	};

	ADarkwellPropLabFurniture* SpawnTracked(
		FName StableId,
		int32 Shape,
		FVector Dimensions,
		FLinearColor Tint,
		const FTransform& Transform);
	void DestroyTracked();
	void DestroyVisual(FRecordVisual& Visual);
	FBox2D ActualBounds(const ADarkwellPropLabFurniture& Prop) const;
	TArray<FBox> ActualPartBounds(const ADarkwellPropLabFurniture& Prop) const;
	TArray<float> ConservativeCoverage(const FBox2D& Bounds) const;
	bool IsOccupiedByActual(FVector2D Point, FName IgnoredStableId) const;
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
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> OwnedMaterials;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> OwnedTextures;
	UPROPERTY(Transient) TArray<TObjectPtr<UDynamicMeshComponent>> OwnedCaps;
	TMap<FName, FTrackedProp> Tracked;
	TWeakObjectPtr<ADarkwellPropLabFurniture> MotionProp;
	FTransform MotionStart = FTransform::Identity;
	FTransform MotionEnd = FTransform::Identity;
	float MotionSeconds = 0.0f;
	float MotionDuration = 0.0f;
	FString Status;
	int32 Scenario = 0;
	int32 ScenarioPhase = 0;
	int32 MultiCount = 0;
	bool bMotionActive = false;
	bool bStarted = false;
};
