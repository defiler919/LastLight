#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SightWeaveObjectPolicy.generated.h"

/** Controls capture of gray object history, never visibility/empty-space authority. */
UENUM(BlueprintType)
enum class ESightWeaveHistoryMode : uint8
{
	Always,
	StationaryOnly,
	Never
};

UENUM(BlueprintType)
enum class ESightWeaveObjectPolicySource : uint8
{
	UseProjectDefault,
	Override
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FResolvedSightWeaveObjectPolicy
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SightWeave|Object History")
	ESightWeaveHistoryMode HistoryMode = ESightWeaveHistoryMode::Always;

	static FResolvedSightWeaveObjectPolicy Resolve(ESightWeaveHistoryMode ProjectDefault,
		ESightWeaveObjectPolicySource Source, ESightWeaveHistoryMode Override);
};

/** Explicit per-object capture lifecycle. Contains no identity, transform or world evidence. */
struct SIGHTWEAVERUNTIME_API FSightWeaveObjectHistoryCapture
{
	void Initialize(FResolvedSightWeaveObjectPolicy InPolicy);
	bool SetMoving(bool bInMoving);
	void ObserveLegally();
	bool IsMoving() const { return bMoving; }
	uint64 GetMovingRevision() const { return MovingRevision; }
	bool RequiresFreshStationaryObservation() const { return bRequiresFreshStationaryObservation; }
	bool IsHistoryEligible() const;
	FResolvedSightWeaveObjectPolicy GetPolicy() const { return Policy; }
private:
	FResolvedSightWeaveObjectPolicy Policy;
	uint64 MovingRevision = 0;
	bool bMoving = false;
	bool bRequiresFreshStationaryObservation = true;
};

/** Optional policy authoring/explicit-motion endpoint. Does not register a second subject identity.
 * Configure before registration; changing authoring properties requires an explicit host reset
 * and component re-registration. No runtime policy migration is performed.
 */
UCLASS(ClassGroup=(SightWeave), BlueprintType, meta=(BlueprintSpawnableComponent))
class SIGHTWEAVERUNTIME_API USightWeaveObjectPolicyComponent final : public UActorComponent
{
	GENERATED_BODY()
public:
	USightWeaveObjectPolicyComponent();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SightWeave|Object History")
	ESightWeaveObjectPolicySource PolicySource = ESightWeaveObjectPolicySource::UseProjectDefault;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SightWeave|Object History",
		meta=(EditCondition="PolicySource == ESightWeaveObjectPolicySource::Override"))
	ESightWeaveHistoryMode HistoryMode = ESightWeaveHistoryMode::Always;

	UFUNCTION(BlueprintCallable, Category="SightWeave|Object History")
	void SetSightWeaveMoving(bool bMoving) { Capture.SetMoving(bMoving); }
	UFUNCTION(BlueprintPure, Category="SightWeave|Object History")
	bool IsSightWeaveMoving() const { return Capture.IsMoving(); }
	UFUNCTION(BlueprintPure, Category="SightWeave|Object History")
	ESightWeaveHistoryMode GetResolvedHistoryMode() const { return Capture.GetPolicy().HistoryMode; }
	UFUNCTION(BlueprintPure, Category="SightWeave|Object History")
	int64 GetMovingRevision() const { return static_cast<int64>(Capture.GetMovingRevision()); }
	UFUNCTION(BlueprintPure, Category="SightWeave|Object History")
	bool IsHistoryEligible() const { return Capture.IsHistoryEligible(); }
	UFUNCTION(BlueprintPure, Category="SightWeave|Object History")
	bool RequiresFreshStationaryObservation() const { return Capture.RequiresFreshStationaryObservation(); }
	/** Host adapter calls only after new valid legal spatial evidence, not on motion completion. */
	void NotifyLegalObservation() { Capture.ObserveLegally(); }
	FResolvedSightWeaveObjectPolicy GetResolvedPolicy() const { return Capture.GetPolicy(); }
protected:
	virtual void OnRegister() override;
private:
	FSightWeaveObjectHistoryCapture Capture;
};
