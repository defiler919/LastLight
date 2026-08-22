// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarkwellPracticeTarget.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/** Simple damageable target that closes the greybox shotgun feedback loop. */
UCLASS()
class DARKWELL_API ADarkwellPracticeTarget : public AActor
{
	GENERATED_BODY()

public:
	ADarkwellPracticeTarget();

	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator,
		AActor* DamageCauser) override;

	float GetHealth() const { return Health; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Target")
	TObjectPtr<UStaticMeshComponent> TargetMesh;

	UPROPERTY(VisibleAnywhere, Category = "Target")
	TObjectPtr<UPointLightComponent> TargetLight;

	UPROPERTY(EditDefaultsOnly, Category = "Target", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Target")
	float Health = 100.0f;
};
