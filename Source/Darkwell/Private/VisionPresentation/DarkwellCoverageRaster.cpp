#include "VisionPresentation/DarkwellFogVisualSubsystem.h"

namespace
{
 double Cross(FVector2D A,FVector2D B) { return A.X*B.Y-A.Y*B.X; }
 bool SegmentTriangle(FVector2D A,FVector2D B,FVector2D O,FVector2D P,FVector2D Q)
 {
  // Conservative clipping against the three closed half planes. Degenerate
  // triangles cannot supply an unoccluded proof close to their line segment.
  const double Area=Cross(P-O,Q-O);
  if(FMath::Abs(Area)<1.e-8) return false;
  const double Sign=Area>0?1:-1;
  double Lo=0,Hi=1;
  const FVector2D V[]{O,P,Q};
  for(int32 I=0;I<3;++I)
  {
   const auto Edge=V[(I+1)%3]-V[I];
   const double DA=Sign*Cross(Edge,A-V[I]), DB=Sign*Cross(Edge,B-V[I]);
   constexpr double Epsilon=.001;
   if(DA < -Epsilon && DB < -Epsilon) return false;
   if(DA < -Epsilon) Lo=FMath::Max(Lo,(-Epsilon-DA)/(DB-DA));
   if(DB < -Epsilon) Hi=FMath::Min(Hi,(-Epsilon-DA)/(DB-DA));
   if(Lo>Hi) return false;
  }
  return true;
 }
 void Corners(const FBox2D& B,FVector2D* C)
 { C[0]=B.Min; C[1]=FVector2D(B.Max.X,B.Min.Y); C[2]=B.Max; C[3]=FVector2D(B.Min.X,B.Max.Y); }
 double MinimumDistance(FVector2D O,const FBox2D& B)
 { return FVector2D::Distance(O,FVector2D(FMath::Clamp(O.X,B.Min.X,B.Max.X),FMath::Clamp(O.Y,B.Min.Y,B.Max.Y))); }
}

bool FDarkwellContinuousVisibilityBuilder::IsOcclusionFree(const FVector2D& Origin,const FBox2D& Bounds,
 TConstArrayView<FDarkwellFogVisualSegment> Occluders)
{
 if(!Bounds.bIsValid || Bounds.GetSize().GetMin()<=0) return false;
 FVector2D C[4]; Corners(Bounds,C);
 FBox2D FanBounds=Bounds; FanBounds+=Origin;
 for(const auto& Wall:Occluders)
 {
  if(!Wall.IsValid()) continue;
  FBox2D WallBounds(ForceInit); WallBounds+=Wall.A; WallBounds+=Wall.B;
  if(!FanBounds.Intersect(WallBounds)) continue;
  for(int32 I=0;I<4;++I) if(SegmentTriangle(Wall.A,Wall.B,Origin,C[I],C[(I+1)%4])) return false;
 }
 return true;
}

bool FDarkwellContinuousVisibilityBuilder::TryUniformCoverage(const FDarkwellFogVisualSourceSnapshot& Source,
 const FBox2D& Bounds,TConstArrayView<FDarkwellFogVisualSegment> Occluders,float& Value)
{
 if(!Source.IsValid() || !Bounds.bIsValid || Bounds.GetSize().GetMin()<=0) return false;
 // 1.25 cm is half of the unchanged 2.5 cm transition. A small proof margin
 // keeps float rounding at the analytic boundary on the original sample path.
 constexpr double Margin=1.251;
 FVector2D C[4]; Corners(Bounds,C);
 bool FullBody=true,FullCone=Source.bConeLegallyLive;
 double MaxLeft=-DBL_MAX,MaxRight=-DBL_MAX;
 const auto F=Source.ConeForward.GetSafeNormal();
 const double Sin=FMath::Sin(FMath::DegreesToRadians(Source.ConeHalfAngleDegrees));
 const double Cos=FMath::Cos(FMath::DegreesToRadians(Source.ConeHalfAngleDegrees));
 for(auto P:C)
 {
  FullBody &= FVector2D::Distance(P,Source.BodyCenter)<=Source.BodyRadiusCentimeters-Margin;
  const auto D=P-Source.ConeOrigin; const double Along=FVector2D::DotProduct(D,F),Side=Cross(D,F);
  const double Left=Along*Sin-Side*Cos,Right=Along*Sin+Side*Cos;
  MaxLeft=FMath::Max(MaxLeft,Left); MaxRight=FMath::Max(MaxRight,Right);
  FullCone &= Left>=Margin && Right>=Margin && D.Size()<=Source.ConeRangeCentimeters-Margin;
 }
 if((FullBody && IsOcclusionFree(Source.BodyCenter,Bounds,Occluders)) ||
    (FullCone && IsOcclusionFree(Source.ConeOrigin,Bounds,Occluders))) { Value=1; return true; }
 const bool NoBody=MinimumDistance(Source.BodyCenter,Bounds)>=Source.BodyRadiusCentimeters+Margin;
 const bool NoCone=!Source.bConeLegallyLive || MaxLeft<=-Margin || MaxRight<=-Margin ||
  MinimumDistance(Source.ConeOrigin,Bounds)>=Source.ConeRangeCentimeters+Margin;
 if(NoBody && NoCone) { Value=0; return true; }
 return false;
}

void UDarkwellFogVisualSubsystem::RefreshCanonicalCoverageCache() const
{
 if(CanonicalAuthority!=Diagnostics.LastAuthorityRevision || CanonicalDraw!=Diagnostics.CoverageDrawCount || bCanonicalActive!=Diagnostics.bActive)
 {
  CanonicalAuthority=Diagnostics.LastAuthorityRevision; CanonicalDraw=Diagnostics.CoverageDrawCount; bCanonicalActive=Diagnostics.bActive;
  CanonicalPoints.Reset(); CanonicalOcclusionPoints.Reset(); CanonicalRasters.Reset();
 }
}
bool UDarkwellFogVisualSubsystem::TryUniformCoverage(const FBox2D& Bounds,float& Value) const
{
 return Diagnostics.bActive && FDarkwellContinuousVisibilityBuilder::TryUniformCoverage(LastSource,Bounds,CachedOccluderSegments,Value);
}
bool UDarkwellFogVisualSubsystem::IsObjectOcclusionFree(const FBox2D& Bounds) const
{
 if(!Diagnostics.bActive || !LastSource.IsValid()) return false;
 return (LastSource.BodyRadiusCentimeters>0 && FDarkwellContinuousVisibilityBuilder::IsOcclusionFree(LastSource.BodyCenter,Bounds,CachedOccluderSegments)) ||
  (LastSource.bConeLegallyLive && FDarkwellContinuousVisibilityBuilder::IsOcclusionFree(LastSource.ConeOrigin,Bounds,CachedOccluderSegments));
}
FDarkwellFogVisualCoverageQuery UDarkwellFogVisualSubsystem::QueryCanonicalCoverageRaster(
 const FBox2D& Bounds,FIntPoint Size,TArray<float>& Values,uint64& QueryRequests) const
{
 RefreshCanonicalCoverageCache();
 const FCoverageRasterKey Key{Bounds.Min,Bounds.Max,Size};
 if(const auto* Cached=CanonicalRasters.Find(Key)) { Values=Cached->Values; ++CanonicalCacheHits; return Cached->Result; }
 Values.Reset();
 if(!Bounds.bIsValid || Size.X<=0 || Size.Y<=0) return {};
 ++QueryRequests;
 auto Result=QueryLiveCoverageAtWorldPoint(Bounds.GetCenter());
 if(!Result.bValid) return Result;
 Values.SetNumUninitialized(Size.X*Size.Y);
 const auto Step=Bounds.GetSize()/FVector2D(Size);
 auto Sample=[&](FVector2D P) { ++QueryRequests; return QueryLiveCoverageAtWorldPoint(P).Coverage; };
 // Recursive tiles retain the exact existing corner/center coordinates. Only
 // mathematically uniform 0/1 tiles bypass evaluation; no resolution is changed.
 auto Fill=[&](auto&& Self,int32 X0,int32 Y0,int32 X1,int32 Y1)->void
 {
  float Uniform;
  const FBox2D Tile(Bounds.Min+Step*FVector2D(X0,Y0),Bounds.Min+Step*FVector2D(X1,Y1));
  if(TryUniformCoverage(Tile,Uniform))
  {
   for(int32 Y=Y0;Y<Y1;++Y) for(int32 X=X0;X<X1;++X) Values[Y*Size.X+X]=Uniform;
   return;
  }
  if(X1-X0>4 || Y1-Y0>4)
  {
   if(X1-X0>=Y1-Y0) { const int32 M=(X0+X1)/2; Self(Self,X0,Y0,M,Y1); Self(Self,M,Y0,X1,Y1); }
   else { const int32 M=(Y0+Y1)/2; Self(Self,X0,Y0,X1,M); Self(Self,X0,M,X1,Y1); }
   return;
  }
  for(int32 Y=Y0;Y<Y1;++Y) for(int32 X=X0;X<X1;++X)
  {
   float V=1;
   for(auto O:{FVector2D(0),FVector2D(1,0),FVector2D(0,1),FVector2D(1),FVector2D(.5)})
    V=FMath::Min(V,Sample(Bounds.Min+Step*(FVector2D(X,Y)+O)));
   Values[Y*Size.X+X]=V;
  }
 };
 Fill(Fill,0,0,Size.X,Size.Y);
 if(Values.ContainsByPredicate([](float V){return V>0;})) Result.ZeroReason=EDarkwellFogCoverageZeroReason::None;
 FCachedCoverageRaster Cached; Cached.Values=Values; Cached.Result=Result; CanonicalRasters.Add(Key,MoveTemp(Cached));
 return Result;
}
