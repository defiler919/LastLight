// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DarkwellFogVisualSubsystem.generated.h"

class ADarkwellVisionIntegrationFixture;
class UMaterialInstanceDynamic;
class UTextureRenderTarget2D;

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
	static float EvaluateNoOcclusionCoverage(
		const FDarkwellFogVisualSourceSnapshot& Source,
		const FVector2D& WorldPosition,
		float TransitionWidthCentimeters);
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
	bool UpdateSource(const FDarkwellFogVisualSourceSnapshot& Source);
	void Deactivate();

	bool IsActive() const { return Diagnostics.bActive; }
	UTextureRenderTarget2D* GetLiveCoverageTexture() const { return LiveCoverageTexture; }
	const FDarkwellFogVisualMapping& GetMapping() const { return Mapping; }
	const FDarkwellFogVisualDiagnostics& GetDiagnostics() const { return Diagnostics; }

private:
	bool CreateResources(const FBox2D& WorldBounds);
	bool DrawCoverage(const FDarkwellFogVisualSourceSnapshot& Source);
	void UpdateMaterialParameters(const FDarkwellFogVisualSourceSnapshot& Source);
	void TryDiagnosticReadback();

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> LiveCoverageTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CoverageMaterial;

	UPROPERTY(Transient)
	TWeakObjectPtr<ADarkwellVisionIntegrationFixture> ActiveFixture;

	FDarkwellFogVisualMapping Mapping;
	FDarkwellFogVisualSourceSnapshot LastSource;
	FDarkwellFogVisualDiagnostics Diagnostics;
	int32 DiagnosticReadbackFrameCount = 0;
};
