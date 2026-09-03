#pragma once
#include "CoreMinimal.h"
#include "VisionPresentation/DarkwellSpatialPropMemory.h"

/** Project adapter state for rigid, upright (XY/yaw) primitive observations.
 * No identity registry, texture, actor, historical evidence or coverage authority.
 * Local sample IDs never change with a world AABB. A world raster is derived,
 * not fed back into local state. Geometry changes require explicit ResetGeometry.
 */
struct DARKWELL_API FDarkwellCurrentLiveGrid
{
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
  TBitArray<> ObservedAtPose;
  FTransform Pose=FTransform::Identity;
  FIntPoint AtlasCells=FIntPoint::ZeroValue;
 };
 void ResetGeometry(FName Id,TConstArrayView<FDescriptor> Descriptors,const FTransform& ActorPose);
 bool MatchesGeometry(TConstArrayView<FDescriptor> Descriptors,const FTransform& ActorPose) const;
 bool Advance(float Dt,const FTransform& ActorPose,TFunctionRef<float(FVector2D)> LegalCoverage);
 void WriteWorldSnapshot(FDarkwellSpatialPropMemory& Out,const FBox2D& Bounds);
 void WritePartRasters(TFunctionRef<float(FVector2D)> LegalCoverage,bool bTransient);
 uint64 StateHash() const;
 TArray<FPart> Parts;
 FTransform LastLegalPose=FTransform::Identity;
 uint64 Updates=0, GeometryResets=0, Queries=0, SamplesTouched=0;
 FIntPoint AtlasCells=FIntPoint::ZeroValue;
private:
 static FDarkwellSpatialPropMemory::FCell Sample(const FPart& P,FVector2D World,bool bClamp);
 FVector RegisteredScale=FVector::OneVector;
};
