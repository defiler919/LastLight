#pragma once

#include "Components/SceneComponent.h"
#include "SightWeaveDebug.h"
#include "SightWeaveGeometry.h"
#include "SightWeaveQueries.h"
#include "SightWeaveStaticEnvironment.h"
#include "SightWeaveTypes.h"

#include "SightWeaveComponents.generated.h"

UCLASS(ClassGroup = (SightWeave), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class SIGHTWEAVERUNTIME_API USightWeaveFloorComponent final : public USceneComponent
{
	GENERATED_BODY()

public:
	USightWeaveFloorComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Floor")
	FSightWeaveFloorDefinition Definition;

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Floor")
	bool RefreshFloorRegistration();

	UFUNCTION(BlueprintPure, Category = "SightWeave|Floor")
	bool IsFloorRegistered() const { return bRegistered; }

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	FSightWeaveFloorDefinition BuildWorldDefinition() const;
	bool bRegistered = false;
	FSightWeaveFloorId RegisteredFloorId;
};

UCLASS(ClassGroup = (SightWeave), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class SIGHTWEAVERUNTIME_API USightWeaveVisionSourceComponent final : public USceneComponent
{
	GENERATED_BODY()

public:
	USightWeaveVisionSourceComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Vision")
	FSightWeaveVisionSourceDescription Description;

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Vision")
	void SetVisionSourceEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Vision")
	bool RefreshVisionSourceRegistration();

	UFUNCTION(BlueprintPure, Category = "SightWeave|Vision")
	FSightWeaveVisionSourceHandle GetVisionSourceHandle() const { return Handle; }

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	FSightWeaveVisionSourceDescription BuildWorldDescription() const;
	FSightWeaveVisionSourceHandle Handle;
};

UCLASS(ClassGroup = (SightWeave), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class SIGHTWEAVERUNTIME_API USightWeaveIlluminationSourceComponent final : public USceneComponent
{
	GENERATED_BODY()

public:
	USightWeaveIlluminationSourceComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Legal Illumination")
	FSightWeaveIlluminationSourceDescription Description;

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Legal Illumination")
	void SetIlluminationSourceEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Legal Illumination")
	bool RefreshIlluminationSourceRegistration();

	UFUNCTION(BlueprintPure, Category = "SightWeave|Legal Illumination")
	FSightWeaveIlluminationSourceHandle GetIlluminationSourceHandle() const { return Handle; }

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	FSightWeaveIlluminationSourceDescription BuildWorldDescription() const;
	FSightWeaveIlluminationSourceHandle Handle;
};

UCLASS(ClassGroup = (SightWeave), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class SIGHTWEAVERUNTIME_API USightWeaveOccluderComponent final : public USceneComponent
{
	GENERATED_BODY()

public:
	USightWeaveOccluderComponent();

	/** Consecutive local XY points form edges; bClosedContour adds the final-to-first edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Occluder")
	TArray<FVector2D> LocalPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Occluder")
	bool bClosedContour = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Occluder")
	FSightWeaveFloorId FloorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Occluder")
	FSightWeaveHeightRange LocalHeightRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Occluder")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Occluder")
	bool bDynamic = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Occluder")
	bool bMergeSafeCollinearSegments = true;

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Occluder")
	void SetOccluderEnabled(bool bInEnabled);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Occluder")
	bool RefreshOccluderRegistration();

	UFUNCTION(BlueprintPure, Category = "SightWeave|Occluder")
	FSightWeaveOccluderHandle GetOccluderHandle() const { return Handle; }

	UFUNCTION(BlueprintPure, Category = "SightWeave|Occluder")
	FString GetLastValidationError() const { return LastValidationError; }

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	bool BuildWorldSegments(TArray<FSightWeaveSegment2D>& OutSegments);
	FSightWeaveOccluderHandle Handle;
	FString LastValidationError;
};

/** Authorable circle for the deliberately minimal M2 hard-live suppression API. */
UCLASS(ClassGroup = (SightWeave), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class SIGHTWEAVERUNTIME_API USightWeaveHardSuppressionComponent final : public USceneComponent
{
	GENERATED_BODY()

public:
	USightWeaveHardSuppressionComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|M2 Hard Live Suppression")
	FSightWeaveHardSuppressionDescription Description;

	UFUNCTION(BlueprintCallable, Category = "SightWeave|M2 Hard Live Suppression")
	void SetHardSuppressionEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|M2 Hard Live Suppression")
	bool RefreshHardSuppressionRegistration();

	UFUNCTION(BlueprintPure, Category = "SightWeave|M2 Hard Live Suppression")
	FSightWeaveHardSuppressionHandle GetHardSuppressionHandle() const { return Handle; }

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	bool BuildWorldDescription(FSightWeaveHardSuppressionDescription& OutDescription) const;
	FSightWeaveHardSuppressionHandle Handle;
};

/** Explicit immutable 2.5D neutral-attribute authoring; never infers eligibility. */
UCLASS(ClassGroup = (SightWeave), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class SIGHTWEAVERUNTIME_API USightWeaveStaticEnvironmentComponent final : public USceneComponent
{
	GENERATED_BODY()

public:
	USightWeaveStaticEnvironmentComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Static Environment")
	FSightWeaveKnowledgeOwnerId KnowledgeOwnerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Static Environment")
	FSightWeaveFloorId FloorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Static Environment")
	FSightWeaveHeightRange LocalHeightRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Static Environment")
	TArray<FVector2D> LocalFootprint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Static Environment",
		meta = (ClampMin = "1", ClampMax = "255"))
	uint8 NeutralIntensity = 112;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Static Environment")
	bool bExplicitlyImmutable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Static Environment")
	bool bEnabled = true;

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Static Environment")
	bool RefreshStaticEnvironmentRegistration();

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Static Environment")
	void SetStaticEnvironmentEnabled(bool bInEnabled);

	UFUNCTION(BlueprintPure, Category = "SightWeave|Static Environment")
	FSightWeaveStaticEnvironmentHandle GetStaticEnvironmentHandle() const { return Handle; }

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	bool BuildWorldDescription(FSightWeaveStaticEnvironmentDescription& OutDescription) const;
	FSightWeaveStaticEnvironmentHandle Handle;
};

/** World-authored BlockMemoryWrites/SuppressMemoryPresentation region. */
UCLASS(ClassGroup = (SightWeave), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class SIGHTWEAVERUNTIME_API USightWeaveMemoryModifierComponent final : public USceneComponent
{
	GENERATED_BODY()

public:
	USightWeaveMemoryModifierComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Memory")
	FSightWeaveKnowledgeOwnerId KnowledgeOwnerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Memory")
	FSightWeaveFloorId FloorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Memory")
	FSightWeaveHeightRange LocalHeightRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Memory")
	ESightWeaveMemoryModifierOperation Operation =
		ESightWeaveMemoryModifierOperation::BlockMemoryWrites;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Memory")
	ESightWeaveMemoryRegionShape Shape = ESightWeaveMemoryRegionShape::Circle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Memory")
	FVector2D LocalCenter = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Memory")
	FVector2D HalfExtents = FVector2D(100.0, 100.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Memory", meta = (ClampMin = "0.01"))
	float Radius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Memory")
	float RotationDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Memory")
	TArray<FVector2D> LocalPolygonVertices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Memory")
	bool bEnabled = true;

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Memory")
	bool RefreshMemoryModifierRegistration();

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Memory")
	void SetMemoryModifierEnabled(bool bInEnabled);

	FSightWeaveMemoryModifierHandle GetMemoryModifierHandle() const { return Handle; }

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	bool BuildWorldDescription(FSightWeaveMemoryModifierDescription& OutDescription) const;
	FSightWeaveMemoryModifierHandle Handle;
};

/** No-tick marker that samples and optionally draws one authoritative query at BeginPlay. */
UCLASS(ClassGroup = (SightWeave), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class SIGHTWEAVERUNTIME_API USightWeaveDebugQueryComponent final : public USceneComponent
{
	GENERATED_BODY()

public:
	USightWeaveDebugQueryComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Debug")
	FSightWeaveKnowledgeOwnerId KnowledgeOwnerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Debug")
	FSightWeaveFloorId FloorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Debug")
	bool bDrawAtBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Debug")
	FSightWeaveDebugDrawOptions DrawOptions;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Debug")
	FSightWeaveVisibilityQueryResult LastResult;

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Debug")
	FSightWeaveVisibilityQueryResult RefreshDebugQuery();

protected:
	virtual void BeginPlay() override;
};
