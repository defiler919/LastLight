// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DarkwellRememberablePropComponent.generated.h"

class USceneComponent;
class UStaticMeshComponent;

/** Declares one stable, environment-only object eligible for last-observed fog memory. */
UCLASS(ClassGroup=(Vision), meta=(BlueprintSpawnableComponent))
class DARKWELL_API UDarkwellRememberablePropComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	UDarkwellRememberablePropComponent();

	void ConfigureStableId(FName InStableId);
	void AddMemoryPrimitive(UStaticMeshComponent* Primitive);
	void ResetMemoryPrimitives() { check(!HasBegunPlay()); MemoryPrimitives.Reset(); }
	bool bRememberFromStart = true;
	void SetMemoryAppearance(FLinearColor Tint, float UVScale) { RememberedTint = Tint; RememberedUVScale = UVScale; }
	void AddLiveOnlyComponent(USceneComponent* Component);
	FName GetStableId() const { return StableId; }
	TConstArrayView<TObjectPtr<UStaticMeshComponent>> GetMemoryPrimitives() const
	{
		return MemoryPrimitives;
	}
	FLinearColor GetRememberedTint() const { return RememberedTint; }
	float GetRememberedUVScale() const { return RememberedUVScale; }
	uint64 ComputeAppearanceRevision() const;
	FTransform GetObservationTransform() const;
	void ApplySourceLiveState(bool bLive);
	/** Geometry presentation only; does not reveal LiveOnly effects or change subject state. */
	void ApplySourceGeometryVisibility(bool bVisible);
	bool IsSourceLive() const { return bSourceLive; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void TryRegister();

	UPROPERTY(EditInstanceOnly, Category="Fog Memory")
	FName StableId;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> MemoryPrimitives;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> LiveOnlyComponents;

	UPROPERTY(EditAnywhere, Category="Fog Memory")
	FLinearColor RememberedTint = FLinearColor(0.62f, 0.66f, 0.70f, 1.0f);

	UPROPERTY(EditAnywhere, Category="Fog Memory", meta=(ClampMin="0.1"))
	float RememberedUVScale = 5.0f;

	TMap<TWeakObjectPtr<USceneComponent>, bool> VisibilityBeforeHide;
	bool bSourceLive = true;
	bool bRegistered = false;
};
