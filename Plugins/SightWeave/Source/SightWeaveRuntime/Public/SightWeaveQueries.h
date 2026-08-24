#pragma once

#include "CoreMinimal.h"
#include "SightWeaveGeometry.h"

#include "SightWeaveQueries.generated.h"

UENUM(BlueprintType, meta = (Bitflags))
enum class ESightWeaveQueryRejectionReason : uint8
{
	None = 0 UMETA(Hidden),
	OutsideVision = 1 << 0,
	MissingCompatibleIllumination = 1 << 1,
	SuppressedLiveVision = 1 << 2,
	FloorUnavailable = 1 << 3,
	HeightMismatch = 1 << 4,
	InactiveSource = 1 << 5
};
ENUM_CLASS_FLAGS(ESightWeaveQueryRejectionReason);

UENUM(BlueprintType)
enum class ESightWeaveSampleRule : uint8
{
	Anchor,
	AnySample,
	AllSamples,
	RequiredCount
};

/** M2-only hard-live suppression. It does not clear or present memory. */
USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveHardSuppressionDescription
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|M2 Hard Live Suppression")
	FSightWeaveFloorId FloorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|M2 Hard Live Suppression")
	FSightWeaveHeightRange HeightRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|M2 Hard Live Suppression")
	FVector2D Center = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|M2 Hard Live Suppression", meta = (ClampMin = "0.0"))
	float Radius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|M2 Hard Live Suppression")
	bool bEnabled = true;

	bool IsValid() const;
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveQuerySampleSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Query")
	TArray<FVector> Samples;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Query", meta = (ClampMin = "0"))
	int32 AnchorIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Query")
	ESightWeaveSampleRule Rule = ESightWeaveSampleRule::Anchor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Query", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	bool IsValid() const;
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveIlluminationQueryResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Query")
	ESightWeaveQueryStatus Status = ESightWeaveQueryStatus::NotReady;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Query")
	bool bAuthoritative = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Query")
	bool bLegallyIlluminated = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Query")
	bool bOccluded = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Query")
	FSightWeaveKnowledgeOwnerId KnowledgeOwnerId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Query")
	FSightWeaveFloorId FloorId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Query")
	FSightWeaveRevision SnapshotRevision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Query")
	TArray<FSightWeaveIlluminationSourceHandle> ContributingIlluminationSources;

	/** Pure illumination never authorizes a memory write. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Query")
	bool bEligibleForMemoryWrite = false;
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveQueryRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Query")
	FSightWeaveKnowledgeOwnerId KnowledgeOwnerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Query")
	FSightWeaveFloorId FloorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SightWeave|Query")
	FSightWeaveQuerySampleSet SampleSet;
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveVisionSnapshotEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	FSightWeaveVisionSourceHandle Handle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	FSightWeaveVisionSourceDescription Description;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	FSightWeavePolygon Polygon;

	/** Empty for bypass entries by construction. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	TArray<FSightWeaveIlluminationSourceHandle> CompatibleIlluminationSources;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	FSightWeaveRevision SourceRevision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	int32 CandidateSegmentCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	int32 CandidateRayCount = 0;

	/** Stable endpoint/boundary event angles used by the reference solver. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	TArray<double> CandidateAnglesRadians;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	double SolveTimeMicroseconds = 0.0;
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveIlluminationSnapshotEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	FSightWeaveIlluminationSourceHandle Handle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	FSightWeaveIlluminationSourceDescription Description;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	FSightWeaveIlluminationPolygon Polygon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	FSightWeaveRevision SourceRevision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	int32 CandidateSegmentCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	int32 CandidateRayCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	TArray<double> CandidateAnglesRadians;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	double SolveTimeMicroseconds = 0.0;
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveHardSuppressionSnapshotEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	FSightWeaveHardSuppressionHandle Handle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	FSightWeaveHardSuppressionDescription Description;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	FSightWeaveRevision Revision;
};

USTRUCT(BlueprintType)
struct SIGHTWEAVERUNTIME_API FSightWeaveFrameSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	FSightWeaveRevision Revision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	TArray<FSightWeaveFloorDefinition> Floors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	TArray<FSightWeaveSegment2D> OccluderSegments;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	TArray<FSightWeaveVisionSnapshotEntry> VisionSources;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	TArray<FSightWeaveIlluminationSnapshotEntry> IlluminationSources;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	TArray<FSightWeaveHardSuppressionSnapshotEntry> HardSuppressions;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	int32 RebuiltVisionPolygonCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	int32 RebuiltIlluminationPolygonCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SightWeave|Snapshot")
	bool bPublished = false;
};
