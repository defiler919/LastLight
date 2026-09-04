#pragma once
#include "CoreMinimal.h"
#include "Math/Float16Color.h"
#include "VisionPresentation/DarkwellSpatialPropMemory.h"
struct FDarkwellFogVisualSourceSnapshot;
struct FDarkwellFogVisualSegment;

/** Project adapter state for rigid, upright (XY/yaw) primitive observations.
 * No identity registry, texture, actor, historical evidence or coverage authority.
 * Local sample IDs never change with a world AABB. A world raster is derived,
 * not fed back into local state. Geometry changes require explicit ResetGeometry.
 */
struct DARKWELL_API FDarkwellCurrentLiveGrid
{
 enum class EDividerSource : uint8
 {
  ViewEdge,
  WallOcclusion,
  WholeCurrentMask,
  PartialCurrentMask,
  HistorySurface,
  HistoryCap,
  MixedCurrentHistory,
  Unknown
 };
 struct FDividerDiagnostics
 {
  EDividerSource Source=EDividerSource::Unknown;
  TBitArray<> FullGeometryMask;
  TBitArray<> RawLiveCoverage;
  bool bObjectHasLegalContact=false;
  TBitArray<> PhysicalOcclusionGate;
  TBitArray<> WholePresentationMask;
  TBitArray<> CurrentLegalObservationMask;
  TBitArray<> LastLegalCaptureMask;
  TBitArray<> FrozenHistoryMask;
  TBitArray<> CapMask;
  TBitArray<> FinalCurrentContribution;
  TBitArray<> FinalHistoricalContribution;
  float MinimumAppearance=0;
  float MaximumAppearance=0;
 };
 struct FDescriptor
 {
  uint64 PrimitiveKey=0, MeshKey=0;
  FBox LocalBounds=FBox(ForceInit);
  FTransform RelativeTransform=FTransform::Identity;
  bool Matches(const FDescriptor& Other) const;
 };
 struct FPart
 {
  FDescriptor Geometry;
  FDarkwellSpatialPropMemory Local, Raster;
  TArray<float> Coverage, Corners;
  // Current contact and accumulated knowledge at the last legal rigid pose.
  TBitArray<> CurrentLegalObservationMask, LastLegalCaptureMask;
#if WITH_DEV_AUTOMATION_TESTS
  /** Last occlusion-only Whole gate in derived raster space. Diagnostic storage only. */
  TBitArray<> PhysicalOcclusionMask;
#endif
  FTransform Pose=FTransform::Identity;
  FIntPoint AtlasCells=FIntPoint::ZeroValue;
  bool bWholePresentation=false, bUniformWholePresentation=false;
  FBox2D WholeBounds;
  FLinearColor WholePixel;
 };
 void ResetGeometry(FName Id,TConstArrayView<FDescriptor> Descriptors,const FTransform& ActorPose);
 bool MatchesGeometry(TConstArrayView<FDescriptor> Descriptors,const FTransform& ActorPose) const;
 bool Advance(float Dt,const FTransform& ActorPose,TFunctionRef<float(FVector2D)> LegalCoverage,TFunction<bool(const FBox2D&,float&)> Uniform={});
 void WriteWorldSnapshot(FDarkwellSpatialPropMemory& Out,const FBox2D& Bounds);
 void WritePartRasters(TFunctionRef<float(FVector2D)> LegalCoverage,bool bTransient,TFunction<bool(const FBox2D&,float&)> Uniform={});
 bool HasAnyLegalObservation(const FTransform& ActorPose,TFunctionRef<float(FVector2D)> Query,TFunctionRef<bool(const FBox2D&,float&)> Uniform);
 bool BuildSweptObservationMask(const FTransform& ActorPose,const FDarkwellFogVisualSourceSnapshot& Previous,
  const FDarkwellFogVisualSourceSnapshot& Current,TConstArrayView<FDarkwellFogVisualSegment> Occluders,
  TBitArray<>& Out,bool bContactOnly);
 void AdvanceWholeUnoccluded(float Dt,const FTransform& ActorPose,FDarkwellSpatialPropMemory& Snapshot,const FBox2D& Bounds,TConstArrayView<float> Coverage);
 /** Confirmed Whole path. Cone/range coverage is retained only as diagnostics;
  * presentation is object blend multiplied by the physical-occlusion-only gate. */
 void AdvanceWholeWithOcclusion(float Dt,const FTransform& ActorPose,
  FDarkwellSpatialPropMemory& Snapshot,const FBox2D& Bounds,TConstArrayView<float> Coverage,
  TFunctionRef<float(FVector2D)> PhysicalOcclusionGate);
 /** Rasterizes registered primitive projections, preserving primitive gaps and AABB corners. */
 bool BuildFullGeometryMask(const FBox2D& Bounds,FIntPoint Size,TBitArray<>& Out) const;
 bool IsUniformWholePresentation() const { return !Parts.IsEmpty() && Parts[0].bUniformWholePresentation; }
 /** Object-local continuous footprint, independent of presentation alpha. */
 void BuildCurrentLegalObservationMask(TBitArray<>& Out) const;
 void ApplyWholeObjectPresentation(float Dt,FDarkwellSpatialPropMemory& Snapshot,
  TFunctionRef<float(FVector2D)> OcclusionPermission);
 /** On-demand development diagnostic. It performs no readback and does not alter presentation. */
 bool GetDividerDiagnostics(int32 PartIndex,FDividerDiagnostics& Out) const;
 static const TCHAR* DividerSourceName(EDividerSource Source);
 FIntPoint ObservationSize=FIntPoint::ZeroValue;
 FVector2D ObservationStepCm=FVector2D::ZeroVector;
 TBitArray<> ObservationFootprint;
 /** Exact primitive-local evidence query, including fine historical ownership. */
 bool HasObservedContributionAt(FVector2D World,int32 PrimitiveIndex=INDEX_NONE) const;
 uint64 StateHash() const;
 /** Preserve clamp sampling at the active rectangle inside a reusable atlas.
  * One duplicated border texel, not new coverage or an expanded AA footprint. */
 static void CopyAtlasWithClampBorder(TConstArrayView<FLinearColor> Pixels,FIntPoint Size,
  FIntPoint Atlas,TArrayView<FFloat16Color> Out);
 TArray<FPart> Parts;
	/** Conservative world regions whose binary current ownership became true this update. */
	TArray<FBox2D> OwnershipDirtyRegions;
 FTransform LastLegalPose=FTransform::Identity;
 bool bFullyObservedAtPose=false;
 uint64 Updates=0, GeometryResets=0, Queries=0, SamplesTouched=0;
 FIntPoint AtlasCells=FIntPoint::ZeroValue;
private:
 TArray<TArray<int32>> ObservationPartIndices;
 FDarkwellSpatialPropMemory WholeAppearance;
 static FDarkwellSpatialPropMemory::FCell Sample(const FPart& P,FVector2D World,bool bClamp);
 FVector RegisteredScale=FVector::OneVector;
};
