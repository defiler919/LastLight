#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SightWeaveComponents.h"

#include "SightWeaveActors.generated.h"

/** Thin reusable authoring hosts; all authority remains in their components/subsystem. */
UCLASS(BlueprintType, Blueprintable)
class SIGHTWEAVERUNTIME_API ASightWeaveFloorActor final : public AActor
{
	GENERATED_BODY()

public:
	ASightWeaveFloorActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	TObjectPtr<USightWeaveFloorComponent> FloorComponent;
};

UCLASS(BlueprintType, Blueprintable)
class SIGHTWEAVERUNTIME_API ASightWeaveVisionSourceActor final : public AActor
{
	GENERATED_BODY()

public:
	ASightWeaveVisionSourceActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	TObjectPtr<USightWeaveVisionSourceComponent> VisionSourceComponent;
};

UCLASS(BlueprintType, Blueprintable)
class SIGHTWEAVERUNTIME_API ASightWeaveIlluminationSourceActor final : public AActor
{
	GENERATED_BODY()

public:
	ASightWeaveIlluminationSourceActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	TObjectPtr<USightWeaveIlluminationSourceComponent> IlluminationSourceComponent;
};

UCLASS(BlueprintType, Blueprintable)
class SIGHTWEAVERUNTIME_API ASightWeaveOccluderActor final : public AActor
{
	GENERATED_BODY()

public:
	ASightWeaveOccluderActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	TObjectPtr<USightWeaveOccluderComponent> OccluderComponent;
};

UCLASS(BlueprintType, Blueprintable)
class SIGHTWEAVERUNTIME_API ASightWeaveHardSuppressionActor final : public AActor
{
	GENERATED_BODY()

public:
	ASightWeaveHardSuppressionActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	TObjectPtr<USightWeaveHardSuppressionComponent> HardSuppressionComponent;
};

UCLASS(BlueprintType, Blueprintable)
class SIGHTWEAVERUNTIME_API ASightWeaveStaticEnvironmentActor final : public AActor
{
	GENERATED_BODY()

public:
	ASightWeaveStaticEnvironmentActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	TObjectPtr<USightWeaveStaticEnvironmentComponent> StaticEnvironmentComponent;
};

UCLASS(BlueprintType, Blueprintable)
class SIGHTWEAVERUNTIME_API ASightWeaveMemoryModifierActor final : public AActor
{
	GENERATED_BODY()

public:
	ASightWeaveMemoryModifierActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	TObjectPtr<USightWeaveMemoryModifierComponent> MemoryModifierComponent;
};

UCLASS(BlueprintType, Blueprintable)
class SIGHTWEAVERUNTIME_API ASightWeaveDebugQueryActor final : public AActor
{
	GENERATED_BODY()

public:
	ASightWeaveDebugQueryActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	TObjectPtr<USightWeaveDebugQueryComponent> DebugQueryComponent;
};
