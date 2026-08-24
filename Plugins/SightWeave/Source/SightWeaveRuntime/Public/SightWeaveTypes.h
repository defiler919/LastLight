#pragma once

#include "CoreMinimal.h"

#include "SightWeaveTypes.generated.h"

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveVisionSourceHandle
{
	GENERATED_BODY()

public:
	FSightWeaveVisionSourceHandle() = default;
	explicit FSightWeaveVisionSourceHandle(const int64 InValue) : Value(InValue) {}

	bool IsValid() const { return Value > 0; }

	int64 GetValue() const { return Value; }

	friend bool operator==(const FSightWeaveVisionSourceHandle& A, const FSightWeaveVisionSourceHandle& B) { return A.Value == B.Value; }
	friend bool operator!=(const FSightWeaveVisionSourceHandle& A, const FSightWeaveVisionSourceHandle& B) { return !(A == B); }
	friend uint32 GetTypeHash(const FSightWeaveVisionSourceHandle& Handle) { return GetTypeHash(Handle.Value); }

private:
	UPROPERTY(VisibleAnywhere, Category = "SightWeave")
	int64 Value = 0;
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveIlluminationSourceHandle
{
	GENERATED_BODY()

public:
	FSightWeaveIlluminationSourceHandle() = default;
	explicit FSightWeaveIlluminationSourceHandle(const int64 InValue) : Value(InValue) {}

	bool IsValid() const { return Value > 0; }

	int64 GetValue() const { return Value; }

	friend bool operator==(const FSightWeaveIlluminationSourceHandle& A, const FSightWeaveIlluminationSourceHandle& B) { return A.Value == B.Value; }
	friend bool operator!=(const FSightWeaveIlluminationSourceHandle& A, const FSightWeaveIlluminationSourceHandle& B) { return !(A == B); }
	friend uint32 GetTypeHash(const FSightWeaveIlluminationSourceHandle& Handle) { return GetTypeHash(Handle.Value); }

private:
	UPROPERTY(VisibleAnywhere, Category = "SightWeave")
	int64 Value = 0;
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveSubjectRevealHandle
{
	GENERATED_BODY()

public:
	FSightWeaveSubjectRevealHandle() = default;
	explicit FSightWeaveSubjectRevealHandle(const int64 InValue) : Value(InValue) {}

	bool IsValid() const { return Value > 0; }

	int64 GetValue() const { return Value; }

	friend bool operator==(const FSightWeaveSubjectRevealHandle& A, const FSightWeaveSubjectRevealHandle& B) { return A.Value == B.Value; }
	friend bool operator!=(const FSightWeaveSubjectRevealHandle& A, const FSightWeaveSubjectRevealHandle& B) { return !(A == B); }
	friend uint32 GetTypeHash(const FSightWeaveSubjectRevealHandle& Handle) { return GetTypeHash(Handle.Value); }

private:
	UPROPERTY(VisibleAnywhere, Category = "SightWeave")
	int64 Value = 0;
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveRevision
{
	GENERATED_BODY()

public:
	FSightWeaveRevision() = default;
	explicit FSightWeaveRevision(const int64 InValue) : Value(InValue) {}

	int64 GetValue() const { return Value; }

	friend bool operator==(const FSightWeaveRevision& A, const FSightWeaveRevision& B) { return A.Value == B.Value; }
	friend bool operator!=(const FSightWeaveRevision& A, const FSightWeaveRevision& B) { return !(A == B); }
	friend bool operator<(const FSightWeaveRevision& A, const FSightWeaveRevision& B) { return A.Value < B.Value; }

private:
	UPROPERTY(VisibleAnywhere, Category = "SightWeave")
	int64 Value = 0;
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveFloorId
{
	GENERATED_BODY()

public:
	FSightWeaveFloorId() = default;
	explicit FSightWeaveFloorId(const FName InValue) : Value(InValue) {}

	bool IsValid() const { return !Value.IsNone(); }

	FName GetValue() const { return Value; }

	friend bool operator==(const FSightWeaveFloorId& A, const FSightWeaveFloorId& B) { return A.Value == B.Value; }
	friend bool operator!=(const FSightWeaveFloorId& A, const FSightWeaveFloorId& B) { return !(A == B); }
	friend uint32 GetTypeHash(const FSightWeaveFloorId& FloorId) { return GetTypeHash(FloorId.Value); }

private:
	UPROPERTY(EditAnywhere, Category = "SightWeave")
	FName Value = NAME_None;
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveHeightRange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	float ZMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	float ZMax = 300.0f;

	bool IsValid() const { return FMath::IsFinite(ZMin) && FMath::IsFinite(ZMax) && ZMin <= ZMax; }
};

UENUM(BlueprintType)
enum class ESightWeaveSourceShape : uint8
{
	Radial,
	DirectionalCone,
	CameraCone
};

UENUM(BlueprintType)
enum class ESightWeaveIlluminationPolicy : uint8
{
	RequiresLegalIllumination,
	BypassLegalIllumination
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveIlluminationCompatibilityProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	TArray<FName> AcceptedCapabilities;

	void Normalize();
	bool Accepts(FName Capability) const;
	bool IsEquivalentTo(const FSightWeaveIlluminationCompatibilityProfile& Other) const;
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveVisionSourceDescription
{
	GENERATED_BODY()

	FSightWeaveVisionSourceDescription();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	FSightWeaveFloorId FloorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	FSightWeaveHeightRange HeightRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	ESightWeaveSourceShape Shape = ESightWeaveSourceShape::DirectionalCone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave", meta = (ClampMin = "0.0"))
	float Range = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float HalfAngleDegrees = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave", meta = (ClampMin = "0.0"))
	float NearAwarenessRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	bool bActive = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	ESightWeaveIlluminationPolicy IlluminationPolicy = ESightWeaveIlluminationPolicy::BypassLegalIllumination;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	FSightWeaveIlluminationCompatibilityProfile Compatibility;

	bool IsValid() const;
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveIlluminationSourceDescription
{
	GENERATED_BODY()

	FSightWeaveIlluminationSourceDescription();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	FSightWeaveFloorId FloorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	FSightWeaveHeightRange HeightRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	ESightWeaveSourceShape Shape = ESightWeaveSourceShape::Radial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave", meta = (ClampMin = "0.0"))
	float Range = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float HalfAngleDegrees = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	bool bActive = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	TArray<FName> EmittedCapabilities;

	void NormalizeCapabilities();
	bool IsValid() const;
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveSubjectRevealSpecification
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	FName KnowledgeOwnerId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	FName SubjectId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	FName Reason = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave", meta = (ClampMin = "0.0"))
	float DurationSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	bool bIgnoreDarkness = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	bool bIgnoreOrdinaryOcclusion = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave")
	bool bIgnoreSuppression = false;

	bool IsValid() const;
};

UENUM(BlueprintType)
enum class ESightWeaveKnowledgeState : uint8
{
	Unknown,
	Remembered,
	Visible
};

UENUM(BlueprintType)
enum class ESightWeaveQueryStatus : uint8
{
	AuthoritativeResult,
	NotReady,
	Unsupported,
	InvalidHandle,
	InvalidFloor,
	InvalidInput
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveVisibilityQueryResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	ESightWeaveQueryStatus Status = ESightWeaveQueryStatus::NotReady;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	ESightWeaveKnowledgeState KnowledgeState = ESightWeaveKnowledgeState::Unknown;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	bool bVisible = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	FSightWeaveRevision Revision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	FSightWeaveFloorId FloorId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	TArray<FSightWeaveVisionSourceHandle> ContributingVisionSources;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave")
	TArray<FSightWeaveIlluminationSourceHandle> ContributingIlluminationSources;
};
