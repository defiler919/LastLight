#pragma once
#include "CoreMinimal.h"

/** Fixed world XY sample-center membership. Max is excluded at every resolution. */
namespace Darkwell::MemoryRegionSamples
{
 inline bool Contains(const FBox2D& B,FVector2D P)
 { return B.bIsValid && P.X>=B.Min.X && P.Y>=B.Min.Y && P.X<B.Max.X && P.Y<B.Max.Y; }
 inline FVector2D Center(const FBox2D& B,FIntPoint S,int32 I)
 { return B.Min+B.GetSize()/FVector2D(S)*FVector2D(I%S.X+.5,I/S.X+.5); }
}
