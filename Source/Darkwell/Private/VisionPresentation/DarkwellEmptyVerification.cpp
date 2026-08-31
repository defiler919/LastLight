#include "VisionPresentation/DarkwellEmptyVerification.h"

void FDarkwellEmptyVerification::Initialize(const FBox2D& InBounds)
{
 Bounds = InBounds;
 Size = FIntPoint(FMath::Max(1,FMath::CeilToInt(Bounds.GetSize().X/CellSize)),
  FMath::Max(1,FMath::CeilToInt(Bounds.GetSize().Y/CellSize)));
 Cells.Empty(); Cells.SetNum(Size.X*Size.Y);
}
FBox2D FDarkwellEmptyVerification::CellBounds(int32 Index) const
{
 const FVector2D Step = Bounds.GetSize()/FVector2D(Size.X,Size.Y);
 const FVector2D Min = Bounds.Min+Step*FVector2D(Index%Size.X,Index/Size.X);
 return FBox2D(Min,Min+Step);
}
void FDarkwellEmptyVerification::Observe(float DeltaSeconds, float Seconds,
 TFunctionRef<float(FVector2D)> Coverage, TFunctionRef<bool(const FBox2D&)> Occupied)
{
 for(int32 Index=0;Index<Cells.Num();++Index)
 {
  FCell& Cell=Cells[Index];
  if(Cell.VerifiedAt>=0) continue;
  const FBox2D Box=CellBounds(Index);
  bool bLegal=true;
  for(const FVector2D Point : {Box.Min,Box.Max,FVector2D(Box.Min.X,Box.Max.Y),FVector2D(Box.Max.X,Box.Min.Y),Box.GetCenter()})
   bLegal &= Coverage(Point)>=LegalCoverage;
  if(!bLegal || Occupied(Box)) { Cell.Dwell=0; continue; }
  // A single long/stalled frame cannot supply the entire confirmation interval.
  Cell.Dwell+=FMath::Clamp(DeltaSeconds,0.f,1.f/30.f);
  if(Cell.Dwell+UE_SMALL_NUMBER>=ConfirmationSeconds) Cell.VerifiedAt=Seconds;
 }
}
float FDarkwellEmptyVerification::VerifiedFraction() const
{
 int32 Count=0; for(const FCell& Cell:Cells) Count+=Cell.VerifiedAt>=0;
 return Cells.IsEmpty()?0.f:float(Count)/Cells.Num();
}
bool FDarkwellEmptyVerification::IsObjectEmpty() const
{
 return !Cells.IsEmpty() && !Cells.ContainsByPredicate([](const FCell& Cell){return Cell.VerifiedAt<0;});
}
float FDarkwellEmptyVerification::Opacity(int32 Index,int32 Mode,float Seconds) const
{
 if(Mode==0) return IsObjectEmpty()?0.f:1.f;
 const float At=Cells[Index].VerifiedAt;
 return At<0?1.f:Mode==1?0.f:1.f-FMath::Clamp((Seconds-At)/FadeSeconds,0.f,1.f);
}
