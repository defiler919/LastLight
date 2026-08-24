#pragma once

#include "Components/SceneComponent.h"
#include "SightWeaveGeometry.h"
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
