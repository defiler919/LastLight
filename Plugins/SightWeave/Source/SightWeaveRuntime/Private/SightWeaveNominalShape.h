#pragma once
#include "SightWeaveGeometry.h"
#include "SightWeaveSurface.h"

namespace SightWeave
{
// Shared by exact Hard queries and conservative Surface cell-strip proofs.
// Keep one nominal predicate, including the origin/near-awareness union.
inline bool IsPointInNominalShape(const FVector& WorldLocation,const FVector2D& Origin,const FVector2D& Forward,
 ESightWeaveSourceShape Shape,double Range,double MinimumCosine,double NearAwarenessRadius,double Epsilon)
{
 const FVector2D Offset(WorldLocation.X-Origin.X,WorldLocation.Y-Origin.Y);
 const double DistanceSquared=Offset.SizeSquared();
 if(DistanceSquared>FMath::Square(Range+Epsilon))return false;
 if(DistanceSquared<=FMath::Square(NearAwarenessRadius+Epsilon) || Shape==ESightWeaveSourceShape::Radial)return true;
 const double Distance=FMath::Sqrt(DistanceSquared);
 return FVector2D::DotProduct(Forward,Offset)>=MinimumCosine*Distance;
}

template<typename EntryType,typename ToleranceType>
bool IsSurfaceSourcePointContained(const EntryType& Entry,FSightWeaveFloorId Floor,const FVector& Point,
 const ToleranceType& T,const FSightWeaveSurfaceContext& Surface)
{
 const auto& D=Entry.Description;
 const double Near=[&](){if constexpr(requires{D.NearAwarenessRadius;})return double(D.NearAwarenessRadius);else return 0.;}();
 return D.bActive && D.FloorId==Floor && Point.Z>=D.HeightRange.ZMin-T.HeightOverlapEpsilon && Point.Z<=D.HeightRange.ZMax+T.HeightOverlapEpsilon
  && IsPointInNominalShape(Point,Entry.PolarOrigin,Entry.NominalForward,D.Shape,D.Range,Entry.NominalMinimumCosine,Near,T.PointOnEdgeEpsilon)
  && FVector::DotProduct(Surface.Normal,D.Transform.GetLocation()-Point)>1.e-4
  && Surface.Unoccluded(Floor,D.Transform.GetLocation(),Point);
}
}
