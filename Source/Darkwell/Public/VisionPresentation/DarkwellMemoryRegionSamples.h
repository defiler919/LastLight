#pragma once
#include "CoreMinimal.h"

/** Fixed world XY sample-center membership. Max is excluded at every resolution. */
namespace Darkwell::MemoryRegionSamples
{
 inline bool Contains(const FBox2D& B,FVector2D P)
 { return B.bIsValid && P.X>=B.Min.X && P.Y>=B.Min.Y && P.X<B.Max.X && P.Y<B.Max.Y; }
 inline FVector2D Center(const FBox2D& B,FIntPoint S,int32 I)
 { return B.Min+B.GetSize()/FVector2D(S)*FVector2D(I%S.X+.5,I/S.X+.5); }
 /** Limit work conservatively; Contains remains the final membership oracle. */
 template<class F> void VisitInside(const FBox2D& B,FIntPoint S,const FBox2D& Region,F&& Visit)
 {
  if(!B.bIsValid || !Region.bIsValid || S.X<=0 || S.Y<=0 || !B.Intersect(Region)) return;
  const auto Step=B.GetSize()/FVector2D(S);
  check(Step.X>0 && Step.Y>0);
  const int32 MinX=FMath::Clamp(FMath::FloorToInt((Region.Min.X-B.Min.X)/Step.X),0,S.X-1);
  const int32 MinY=FMath::Clamp(FMath::FloorToInt((Region.Min.Y-B.Min.Y)/Step.Y),0,S.Y-1);
  const int32 MaxX=FMath::Clamp(FMath::CeilToInt((Region.Max.X-B.Min.X)/Step.X),0,S.X-1);
  const int32 MaxY=FMath::Clamp(FMath::CeilToInt((Region.Max.Y-B.Min.Y)/Step.Y),0,S.Y-1);
  for(int32 Y=MinY;Y<=MaxY;++Y) for(int32 X=MinX;X<=MaxX;++X)
  {
   const int32 I=Y*S.X+X;
   if(Contains(Region,Center(B,S,I))) Visit(I);
  }
 }
}
