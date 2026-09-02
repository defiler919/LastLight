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
	/** Only the immediately preceding valid publication may supply a sweep. */
	bool GetHistoricalRotationSweep(uint64 PreviousDrawRevision,
		FDarkwellFogVisualSourceSnapshot& OutPrevious, FDarkwellFogVisualSourceSnapshot& OutCurrent,
		TConstArrayView<FDarkwellFogVisualSegment>& OutOccluders) const;

	bool IsActive() const { return Diagnostics.bActive; }
	UTextureRenderTarget2D* GetLiveCoverageTexture() const { return LiveCoverageTexture; }
	const FDarkwellFogVisualMapping& GetMapping() const { return Mapping; }
	const FDarkwellFogVisualDiagnostics& GetDiagnostics() const { return Diagnostics; }

private:
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
