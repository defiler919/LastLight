#include "VisionPresentation/DarkwellStaticKnowledge.h"
#include "VisionPresentation/DarkwellMemoryRegionSamples.h"

FIntPoint FDarkwellStaticKnowledge::Key(FVector2D P)
{return {int32(FMath::FloorToInt(P.X/TileCm)),int32(FMath::FloorToInt(P.Y/TileCm))};}
FBox2D FDarkwellStaticKnowledge::Bounds(FIntPoint K)
{const FVector2D A=FVector2D(K)*TileCm;return FBox2D(A,A+FVector2D(TileCm));}
void FDarkwellStaticKnowledge::Declare(const FBox2D& B)
{
 check(B.bIsValid && !B.Min.ContainsNaN() && !B.Max.ContainsNaN());
 const auto A=Key(B.Min),Z=Key(B.Max);
 for(int Y=A.Y;Y<=Z.Y;++Y)for(int X=A.X;X<=Z.X;++X) Eligible.Add({X,Y});
}
bool FDarkwellStaticKnowledge::IsBlocked(FVector2D P) const
{return Darkwell::MemoryRegionSamples::Contains(Block,P);}
bool FDarkwellStaticKnowledge::HasMemory(FVector2D P) const
{
 if(P.ContainsNaN())return false;
 const auto K=Key(P);const auto* T=Tiles.Find(K);if(!T)return false;
 const FVector2D L=(P-Bounds(K).Min)/SampleCm;
 const int X=FMath::FloorToInt(L.X),Y=FMath::FloorToInt(L.Y);
 check(X>=0 && Y>=0 && X<Side && Y<Side);return T->Known[Y*Side+X];
}
bool FDarkwellStaticKnowledge::FullyOccluded(const FDarkwellFogVisualSourceSnapshot& S,const FBox2D& B,TConstArrayView<FDarkwellFogVisualSegment> Segments)
{
 auto Shadow=[&](FVector2D O)
 {
  const FVector2D C[]{B.Min,FVector2D(B.Max.X,B.Min.Y),B.Max,FVector2D(B.Min.X,B.Max.Y)};
  // The strict shadow of one segment is convex. Four blocked corners prove the
  // entire rectangle; four DIFFERENT blockers would not prove its interior.
  for(const auto& Segment:Segments)
  {
   if(!Segment.IsValid())continue;bool All=true;
   for(auto P:C)All &= FDarkwellContinuousVisibilityBuilder::IsBlockedBySegments(O,P,MakeArrayView(&Segment,1));
   if(All)return true;
  }
  return false;
 };
 return (S.BodyRadiusCentimeters<=0 || Shadow(S.BodyCenter)) && (!S.bConeLegallyLive || Shadow(S.ConeOrigin));
}
void FDarkwellStaticKnowledge::Observe(const FDarkwellFogVisualSourceSnapshot& S,TConstArrayView<FDarkwellFogVisualSegment> Segments,const FBox2D& Area)
{
 TRACE_CPUPROFILER_EVENT_SCOPE(Darkwell_StaticKnowledge_Observe);
 Stats={};const double Start=FPlatformTime::Seconds();
 if(!S.IsValid() || !Area.bIsValid)return;
 const auto A=Key(Area.Min),Z=Key(Area.Max);
 for(int TY=A.Y;TY<=Z.Y;++TY)for(int TX=A.X;TX<=Z.X;++TX)
 {
  const FIntPoint K(TX,TY);if(!Eligible.Contains(K))continue;++Stats.Candidates;
  auto* T=Tiles.Find(K);if(T && T->Count==Side*Side)continue;
  const auto B=Bounds(K);bool Changed=false,Touched=false;
  auto Fill=[&](auto&& Self,int X0,int Y0,int X1,int Y1)->void
  {
   const FBox2D R(B.Min+FVector2D(X0,Y0)*SampleCm,B.Min+FVector2D(X1,Y1)*SampleCm);
   if(Block.bIsValid && R.Min.X>=Block.Min.X && R.Min.Y>=Block.Min.Y && R.Max.X<=Block.Max.X && R.Max.Y<=Block.Max.Y)return;
   float Uniform=0;const bool Proven=FDarkwellContinuousVisibilityBuilder::TryUniformCoverage(S,R,Segments,Uniform);
   if(Proven || FullyOccluded(S,R,Segments))
   {
    ++Stats.UniformProofs;if(!Proven || Uniform<.99f)return;
   }
   else if(X1-X0>4 || Y1-Y0>4)
   {
    if(X1-X0>=Y1-Y0){const int M=(X0+X1)/2;Self(Self,X0,Y0,M,Y1);Self(Self,M,Y0,X1,Y1);}
    else {const int M=(Y0+Y1)/2;Self(Self,X0,Y0,X1,M);Self(Self,X0,M,X1,Y1);}return;
   }
   for(int Y=Y0;Y<Y1;++Y)for(int X=X0;X<X1;++X)
   {
    const int I=Y*Side+X;if(T && T->Known[I])continue;
    const auto Min=B.Min+FVector2D(X,Y)*SampleCm;if(IsBlocked(Min+FVector2D(SampleCm*.5)))continue;
    Touched=true;++Stats.TestedSamples;bool Legal=Proven;
    if(!Proven)
    {
     Legal=true;for(auto O:{FVector2D(0),FVector2D(1,0),FVector2D(0,1),FVector2D(1),FVector2D(.5)})
     {++Stats.Queries;const auto Q=FDarkwellContinuousVisibilityBuilder::QuerySourceCoverage(S,Min+O*SampleCm,Segments);if(!Q.bValid || Q.Coverage<.99f){Legal=false;break;}}
    }
    if(Legal){if(!T)T=&Tiles.Add(K);T->Known[I]=true;++T->Count;++Stats.WrittenSamples;Changed=true;}
   }
  };
  Fill(Fill,0,0,Side,Side);
  Stats.TouchedTiles+=Touched;if(Changed)Dirty.Add(K);
 }
 Stats.UpdateUs=(FPlatformTime::Seconds()-Start)*1.e6;
}
void FDarkwellStaticKnowledge::Clear(const FBox2D& Region)
{
 if(!Region.bIsValid)return;const auto A=Key(Region.Min),Z=Key(Region.Max);
 for(int Y=A.Y;Y<=Z.Y;++Y)for(int X=A.X;X<=Z.X;++X)
 {
  const FIntPoint K(X,Y);auto* T=Tiles.Find(K);if(!T)continue;bool Changed=false;
  Darkwell::MemoryRegionSamples::VisitInside(Bounds(K),{Side,Side},Region,[&](int I){if(T->Known[I]){T->Known[I]=false;--T->Count;Changed=true;}});
  if(Changed)Dirty.Add(K);
 }
}
