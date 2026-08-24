#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SightWeaveTypes.h"

#include "SightWeaveWorldSubsystem.generated.h"

UCLASS()
class SIGHTWEAVERUNTIME_API USightWeaveWorldSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "SightWeave")
	bool IsSightWeaveInitialized() const { return bSightWeaveInitialized; }

	UFUNCTION(BlueprintPure, Category = "SightWeave")
	FSightWeaveRevision GetRevision() const { return Revision; }

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Vision")
	FSightWeaveVisionSourceHandle RegisterVisionSource(const FSightWeaveVisionSourceDescription& Description, UObject* Owner);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Vision")
	bool UpdateVisionSource(FSightWeaveVisionSourceHandle Handle, const FSightWeaveVisionSourceDescription& Description);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Vision")
	bool UnregisterVisionSource(FSightWeaveVisionSourceHandle Handle);

	UFUNCTION(BlueprintPure, Category = "SightWeave|Vision")
	bool IsVisionSourceHandleValid(FSightWeaveVisionSourceHandle Handle) const;

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Illumination")
	FSightWeaveIlluminationSourceHandle RegisterIlluminationSource(const FSightWeaveIlluminationSourceDescription& Description, UObject* Owner);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Illumination")
	bool UpdateIlluminationSource(FSightWeaveIlluminationSourceHandle Handle, const FSightWeaveIlluminationSourceDescription& Description);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Illumination")
	bool UnregisterIlluminationSource(FSightWeaveIlluminationSourceHandle Handle);

	UFUNCTION(BlueprintPure, Category = "SightWeave|Illumination")
	bool IsIlluminationSourceHandleValid(FSightWeaveIlluminationSourceHandle Handle) const;

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Subject Reveal")
	FSightWeaveSubjectRevealHandle ApplySubjectRevealOverride(const FSightWeaveSubjectRevealSpecification& Specification, UObject* Owner);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Subject Reveal")
	bool UpdateSubjectRevealOverride(FSightWeaveSubjectRevealHandle Handle, const FSightWeaveSubjectRevealSpecification& Specification);

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Subject Reveal")
	bool RemoveSubjectRevealOverride(FSightWeaveSubjectRevealHandle Handle);

	UFUNCTION(BlueprintPure, Category = "SightWeave|Subject Reveal")
	bool IsSubjectRevealHandleValid(FSightWeaveSubjectRevealHandle Handle) const;

	UFUNCTION(BlueprintCallable, Category = "SightWeave|Lifecycle")
	int32 UnregisterAllForOwner(UObject* Owner);

	UFUNCTION(BlueprintPure, Category = "SightWeave|Query")
	FSightWeaveVisibilityQueryResult QueryVisibilityAtLocation(FSightWeaveFloorId FloorId, FVector WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "SightWeave|Query")
	FSightWeaveVisibilityQueryResult QueryVisionSourceAtLocation(FSightWeaveVisionSourceHandle Handle, FSightWeaveFloorId FloorId, FVector WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "SightWeave|Diagnostics")
	int32 GetVisionSourceCount() const { return VisionSources.Num(); }

	UFUNCTION(BlueprintPure, Category = "SightWeave|Diagnostics")
	int32 GetIlluminationSourceCount() const { return IlluminationSources.Num(); }

	UFUNCTION(BlueprintPure, Category = "SightWeave|Diagnostics")
	int32 GetSubjectRevealCount() const { return SubjectReveals.Num(); }

private:
	void AdvanceRevision();
	void ResetState();
	FSightWeaveVisibilityQueryResult MakeQueryResult(ESightWeaveQueryStatus Status, FSightWeaveFloorId FloorId) const;

	bool bSightWeaveInitialized = false;
	int64 NextVisionSourceId = 1;
	int64 NextIlluminationSourceId = 1;
	int64 NextSubjectRevealId = 1;
	FSightWeaveRevision Revision;

	TMap<int64, FSightWeaveVisionSourceDescription> VisionSources;
	TMap<int64, FSightWeaveIlluminationSourceDescription> IlluminationSources;
	TMap<int64, FSightWeaveSubjectRevealSpecification> SubjectReveals;

	TMap<int64, TWeakObjectPtr<UObject>> VisionOwners;
	TMap<int64, TWeakObjectPtr<UObject>> IlluminationOwners;
	TMap<int64, TWeakObjectPtr<UObject>> SubjectRevealOwners;
};
