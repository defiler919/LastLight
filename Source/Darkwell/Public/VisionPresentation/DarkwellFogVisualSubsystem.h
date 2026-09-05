// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DarkwellFogVisualSubsystem.generated.h"

class ADarkwellVisionIntegrationFixture;
class UMaterialInstanceDynamic;
class UTextureRenderTarget2D;

/** Cached continuous 2D occluder geometry supplied by the project adapter. */
struct DARKWELL_API FDarkwellFogVisualSegment
{
	FVector2D A = FVector2D::ZeroVector;
	FVector2D B = FVector2D::ZeroVector;

	bool IsValid() const;
};

/** Extensible initial-knowledge policy. The current rebuild implements only RememberedFromStart. */
UENUM()
enum class EDarkwellInitialKnowledgePolicy : uint8
{
	UnknownUntilExplored,
	RememberedFromStart,
	FullyLive
};

/** Immutable project-facing source data copied from the SightWeave authority adapter. */
struct DARKWELL_API FDarkwellFogVisualSourceSnapshot
{
	FVector2D BodyCenter = FVector2D::ZeroVector;
	FVector2D ConeOrigin = FVector2D::ZeroVector;
	FVector2D ConeForward = FVector2D(1.0, 0.0);
	float BodyRadiusCentimeters = 0.0f;
	float ConeRangeCentimeters = 0.0f;
	float ConeHalfAngleDegrees = 0.0f;
	uint64 AuthorityRevision = 0;
	bool bConeLegallyLive = false;

	bool IsValid() const;
	bool IsEquivalentTo(const FDarkwellFogVisualSourceSnapshot& Other) const;
};

/** Why a valid project-side analytic coverage query returned zero. */
enum class EDarkwellFogCoverageZeroReason : uint8
{
	None,
	SubsystemInactive,
	SourceInvalid,
	PointInvalid,
	ConeNotLegallyLive,
	Occluded,
	OutsideLegalSource
};

/**
 * Revisioned CPU coverage query used by Development/Editor validation.
 * A fail-closed numeric zero is not evidence unless bValid is true.
 */
struct DARKWELL_API FDarkwellFogVisualCoverageQuery
{
	float Coverage = 0.0f;
	uint64 AuthorityRevision = 0;
	uint64 CoverageDrawRevision = 0;
	EDarkwellFogCoverageZeroReason ZeroReason =
		EDarkwellFogCoverageZeroReason::SubsystemInactive;
	bool bValid = false;
	bool bBodyBlocked = false;
	bool bConeBlocked = false;
};

/** Stable world-to-texture mapping for the DARKWELL-owned continuous coverage field. */
struct DARKWELL_API FDarkwellFogVisualMapping
{
	FVector2D WorldMin = FVector2D::ZeroVector;
	FVector2D WorldExtent = FVector2D::ZeroVector;
	FVector2D InvWorldExtent = FVector2D::ZeroVector;
	FIntPoint TextureExtent = FIntPoint::ZeroValue;
	float CentimetersPerTexel = 0.0f;

	bool IsValid() const;
	FVector2D WorldToUV(const FVector2D& WorldPosition) const;
};

/** CPU oracle matching the P1 analytic shader. It exists for sub-texel contract tests only. */
class DARKWELL_API FDarkwellContinuousVisibilityBuilder final
{
public:
	/** Shared body/cone + segment-occlusion query used by live and swept evidence. */
	static FDarkwellFogVisualCoverageQuery QuerySourceCoverage(
		const FDarkwellFogVisualSourceSnapshot& Source, const FVector2D& WorldPosition,
		TConstArrayView<FDarkwellFogVisualSegment> Occluders);
	/** Exact 0/1 proof for an entire rectangle; false means sample at original precision. */
 static bool TryUniformCoverage(const FDarkwellFogVisualSourceSnapshot& Source,const FBox2D& Bounds,
  TConstArrayView<FDarkwellFogVisualSegment> Occluders,float& Value);
 static bool IsOcclusionFree(const FVector2D& Origin,const FBox2D& Bounds,TConstArrayView<FDarkwellFogVisualSegment> Occluders);
 static float EvaluateNoOcclusionCoverage(
		const FDarkwellFogVisualSourceSnapshot& Source,
		const FVector2D& WorldPosition,
		float TransitionWidthCentimeters);
	static bool IsBlockedBySegments(
		const FVector2D& SourceOrigin,
		const FVector2D& WorldPosition,
		TConstArrayView<FDarkwellFogVisualSegment> Segments);
};

/** CPU oracle for the project wall/box material's stable object-local sampling. */
class DARKWELL_API FDarkwellFogSurfaceCoverageMath final
{
public:
	static bool ResolveWallSideSamples(
		const FVector2D& SurfaceWorldPosition,
		const FVector2D& WallOrigin,
		const FVector2D& WallNormal,
		const FVector2D& WallTangent,
		float HalfThicknessCentimeters,
		float ExteriorEpsilonCentimeters,
		FVector2D& OutSideA,
		FVector2D& OutSideB);
	static float CombineWallSides(float SideA, float SideB);
	static float CombineBoxSides(float PositiveX, float NegativeX, float PositiveY, float NegativeY);
};

struct DARKWELL_API FDarkwellFogVisualDiagnostics
{
	EDarkwellInitialKnowledgePolicy InitialKnowledgePolicy =
		EDarkwellInitialKnowledgePolicy::RememberedFromStart;
	uint64 ActivationRevision = 0;
	uint64 LastAuthorityRevision = 0;
	uint64 CoverageDrawCount = 0;
	int64 LastIntermediatePixelCount = -1;
	float LastMinimumCoverage = 0.0f;
	float LastMaximumCoverage = 0.0f;
	bool bActive = false;
	bool bOldSightWeavePresentationSuppressed = false;
	bool bP1NoOcclusion = true;
	bool bP3SurfaceCoverage = false;
	bool bP4DynamicSubjects = false;
	int32 CachedOccluderSegmentCount = 0;
};

/**
 * DARKWELL-owned fog presentation lifetime. SightWeave supplies source legality;
 * this subsystem builds the independent continuous visual field.
 */
UCLASS()
class DARKWELL_API UDarkwellFogVisualSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	/** Ordinary host entry. The host supplies its revisioned legal source and physical segments. */
	bool ActivateForWorld(const FBox2D& WorldBounds,
		const FDarkwellFogVisualSourceSnapshot& Source,
		TConstArrayView<FDarkwellFogVisualSegment> OccluderSegments);

	bool ActivateP1(
		ADarkwellVisionIntegrationFixture* Fixture,
		const FDarkwellFogVisualSourceSnapshot& Source);
	bool ActivateP2(
		ADarkwellVisionIntegrationFixture* Fixture,
		const FDarkwellFogVisualSourceSnapshot& Source,
		TConstArrayView<FDarkwellFogVisualSegment> OccluderSegments);
	bool ActivateP3(
		ADarkwellVisionIntegrationFixture* Fixture,
		const FDarkwellFogVisualSourceSnapshot& Source,
		TConstArrayView<FDarkwellFogVisualSegment> OccluderSegments);
	bool ActivateP4(
		ADarkwellVisionIntegrationFixture* Fixture,
		const FDarkwellFogVisualSourceSnapshot& Source,
		TConstArrayView<FDarkwellFogVisualSegment> OccluderSegments);
	bool UpdateSource(const FDarkwellFogVisualSourceSnapshot& Source);
	void Deactivate();
	/** CPU query matching the formal analytic body/cone and segment occlusion path. */
	float EvaluateLiveCoverageAtWorldPoint(const FVector2D& WorldPosition) const;
	/** Same analytic query with validity, revision and zero-reason diagnostics. */
	FDarkwellFogVisualCoverageQuery QueryLiveCoverageAtWorldPoint(
		const FVector2D& WorldPosition) const;
	/** Object-only post-confirmation gate. Does not write/expand legal coverage. */
	FDarkwellFogVisualCoverageQuery QueryObjectOcclusionAtWorldPoint(const FVector2D& WorldPosition) const;
	/** Only the immediately preceding valid publication may supply a sweep. */
	bool GetHistoricalRotationSweep(uint64 PreviousDrawRevision,
		FDarkwellFogVisualSourceSnapshot& OutPrevious, FDarkwellFogVisualSourceSnapshot& OutCurrent,
		TConstArrayView<FDarkwellFogVisualSegment>& OutOccluders) const;

	bool IsActive() const { return Diagnostics.bActive; }
	/** Optional forensic audit; disabled during timing runs. Exact positions, one revision/frame. */
	bool TryUniformCoverage(const FBox2D& Bounds,float& Value) const;
 bool IsObjectOcclusionFree(const FBox2D& Bounds) const;
 FDarkwellFogVisualCoverageQuery QueryCanonicalCoverageRaster(const FBox2D& Bounds,FIntPoint Size,TArray<float>& Values,uint64& QueryRequests) const;
 uint64 GetCoverageComputationsForTesting() const { return CanonicalComputations; }
 uint64 GetCoverageCacheHitsForTesting() const { return CanonicalCacheHits; }
 void BeginCoverageAuditForTesting() { bCoverageAudit = true; AuditPoints.Reset(); AuditQueries = AuditDuplicates = 0; }
	void EndCoverageAuditForTesting(uint64& Queries, uint64& Duplicates) { bCoverageAudit = false; Queries = AuditQueries; Duplicates = AuditDuplicates; AuditPoints.Reset(); }
	UTextureRenderTarget2D* GetLiveCoverageTexture() const { return LiveCoverageTexture; }
	const FDarkwellFogVisualMapping& GetMapping() const { return Mapping; }
	const FDarkwellFogVisualDiagnostics& GetDiagnostics() const { return Diagnostics; }

private:
	struct FCoverageRasterKey
 {
  FVector2D Min,Max; FIntPoint Size;
  bool operator==(const FCoverageRasterKey& O) const { return Min==O.Min && Max==O.Max && Size==O.Size; }
  friend uint32 GetTypeHash(const FCoverageRasterKey& K) { return HashCombine(HashCombine(GetTypeHash(K.Min),GetTypeHash(K.Max)),GetTypeHash(K.Size)); }
 };
 struct FCachedCoverageRaster { TArray<float> Values; FDarkwellFogVisualCoverageQuery Result; };
 void RefreshCanonicalCoverageCache() const;
 mutable uint64 CanonicalAuthority=MAX_uint64,CanonicalDraw=MAX_uint64;
 mutable bool bCanonicalActive=false;
 mutable TMap<FVector2D,FDarkwellFogVisualCoverageQuery> CanonicalPoints, CanonicalOcclusionPoints;
 mutable TMap<FCoverageRasterKey,FCachedCoverageRaster> CanonicalRasters;
 mutable uint64 CanonicalComputations=0,CanonicalCacheHits=0;
 bool bCoverageAudit = false;
	mutable TSet<FVector2D> AuditPoints;
	mutable uint64 AuditQueries = 0, AuditDuplicates = 0;
	bool Activate(
		ADarkwellVisionIntegrationFixture* Fixture,
		const FDarkwellFogVisualSourceSnapshot& Source,
		TConstArrayView<FDarkwellFogVisualSegment> OccluderSegments,
		int32 PresentationPhase);
	bool CreateResources(const FBox2D& WorldBounds);
	bool DrawCoverage(const FDarkwellFogVisualSourceSnapshot& Source);
	void UpdateMaterialParameters(const FDarkwellFogVisualSourceSnapshot& Source);
	bool UpdateOccluderParameters(TConstArrayView<FDarkwellFogVisualSegment> Segments);
	void TryDiagnosticReadback();

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> LiveCoverageTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CoverageMaterial;

	UPROPERTY(Transient)
	TWeakObjectPtr<ADarkwellVisionIntegrationFixture> ActiveFixture;

	FDarkwellFogVisualMapping Mapping;
	FDarkwellFogVisualSourceSnapshot LastSource;
	FDarkwellFogVisualSourceSnapshot PreviousSource;
	bool bSourceContinuityValid = false;
	TArray<FDarkwellFogVisualSegment> CachedOccluderSegments;
	FDarkwellFogVisualDiagnostics Diagnostics;
	int32 DiagnosticReadbackFrameCount = 0;
};
