#include "VisionPresentation/DarkwellSpatialPropMemory.h"
#include "NativeGameplayTags.h"

namespace Darkwell::SpatialPropMemory
{
 UE_DEFINE_GAMEPLAY_TAG_STATIC(Present,"Lab.ManualStale.Actual.Present");
 UE_DEFINE_GAMEPLAY_TAG_STATIC(Absent,"Lab.ManualStale.Actual.Absent");
}

void FDarkwellSpatialPropMemory::Initialize(FName InStableId,const FBox2D& InBounds,float CellSize)
{
 StableId=InStableId; Bounds=InBounds; Generation=0; ActualState=FGameplayTag();
 check(InBounds.bIsValid && CellSize>0 && InBounds.GetSize().GetMin()>0);
 Size=FIntPoint(FMath::CeilToInt(InBounds.GetSize().X/CellSize),FMath::CeilToInt(InBounds.GetSize().Y/CellSize));
 Cells.Empty(); Cells.SetNum(Size.X*Size.Y);
}
bool FDarkwellSpatialPropMemory::IsPresent() const { return ActualState==Darkwell::SpatialPropMemory::Present; }
bool FDarkwellSpatialPropMemory::IsAbsent() const { return ActualState==Darkwell::SpatialPropMemory::Absent; }
void FDarkwellSpatialPropMemory::BeginPresent()
{
 ++Generation; ActualState=Darkwell::SpatialPropMemory::Present;
 for(FCell& C:Cells)
 {
  // Only UNVERIFIED old knowledge survives. The new generation starts unknown.
  const float Retained=C.RemainingStale;
  C=FCell(); C.InitialRemembered=Retained; C.RemainingStale=Retained; C.StaleOpacity=Retained;
 }
}
void FDarkwellSpatialPropMemory::BeginAbsent()
{
 ++Generation; ActualState=Darkwell::SpatialPropMemory::Absent;
 for(FCell& C:Cells)
 {
  C.InitialRemembered=FMath::Max(C.DiscoveredPresent,C.RemainingStale);
  C.RemainingStale=C.InitialRemembered; C.StaleOpacity=C.InitialRemembered;
  C.VerifiedEmpty=0; C.EmptyDwell=0; C.CurrentLegalCoverage=0; C.LiveBlend=0; C.ExitAge=0;
 }
}
bool FDarkwellSpatialPropMemory::Advance(float DeltaSeconds,TConstArrayView<float> Coverage)
{
 if(Coverage.Num()!=Cells.Num() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds<0) return false;
 const float Dt=FMath::Min(DeltaSeconds,.20f);
 for(int32 I=0;I<Cells.Num();++I)
 {
  FCell& C=Cells[I];
  C.CurrentLegalCoverage=FMath::IsFinite(Coverage[I])?FMath::Clamp(Coverage[I],0.f,1.f):0.f;
  const bool bLegal=C.CurrentLegalCoverage>=LegalCoverage;
  if(IsPresent())
  {
   if(bLegal && C.DiscoveredPresent==0)
   {
    C.DiscoveredPresent=1;
    // Re-observing an old gray cell cannot briefly erase its visible surface.
    C.AppearanceBlend=C.RemainingStale;
   }
   if(C.DiscoveredPresent>0)
   {
    C.AppearanceBlend=FMath::Min(1.f,C.AppearanceBlend+Dt/EnterSeconds);
    if(bLegal) { C.ExitAge=0; C.LiveBlend=FMath::Min(1.f,C.LiveBlend+Dt/EnterSeconds); }
    else
    {
     const float AfterHold=FMath::Max(0.f,C.ExitAge+Dt-ExitHoldSeconds)-FMath::Max(0.f,C.ExitAge-ExitHoldSeconds);
     C.ExitAge=FMath::Min(C.ExitAge+Dt,1.f);
     C.LiveBlend=FMath::Max(0.f,C.LiveBlend-AfterHold/ExitSeconds);
    }
   }
  }
  else if(IsAbsent())
  {
   if(C.VerifiedEmpty==0)
   {
    // Same conservative dwell rule as the existing empty-evidence adapter.
    C.EmptyDwell=bLegal?C.EmptyDwell+FMath::Min(Dt,1.f/30.f):0.f;
    if(C.EmptyDwell+UE_SMALL_NUMBER>=EmptyConfirmationSeconds) C.VerifiedEmpty=1;
   }
   C.RemainingStale=C.InitialRemembered*(1-C.VerifiedEmpty);
   C.StaleOpacity=FMath::Max(C.RemainingStale,C.StaleOpacity-Dt/EmptyFadeSeconds);
  }
 }
 return true;
}
FLinearColor FDarkwellSpatialPropMemory::Presentation(int32 Index) const
{
 const FCell& C=Cells[Index];
 if(IsPresent()) return FLinearColor(C.AppearanceBlend,C.LiveBlend,C.DiscoveredPresent>0?0.f:C.StaleOpacity,C.CurrentLegalCoverage);
 return FLinearColor(0,0,C.StaleOpacity,C.CurrentLegalCoverage);
}

FIntPoint FDarkwellSpatialPropMemory::BuildConservativePresentation(int32 SamplesPerCell,TArray<FLinearColor>& OutPixels) const
{
 OutPixels.Reset();
 if(SamplesPerCell<2 || Cells.IsEmpty()) return FIntPoint::ZeroValue;
 const FIntPoint ResultSize=Size*SamplesPerCell;
 OutPixels.SetNumUninitialized(ResultSize.X*ResultSize.Y);
 TArray<FLinearColor> Coarse; Coarse.SetNumUninitialized(Cells.Num());
 for(int32 I=0;I<Cells.Num();++I) Coarse[I]=Presentation(I);
 auto Channel=[](const FLinearColor& P,int32 C) { return C==0?P.R:C==1?P.G:C==2?P.B:P.A; };
 auto Visible=[&](int32 X,int32 Y,int32 C)
 { return X>=0 && Y>=0 && X<Size.X && Y<Size.Y && Channel(Coarse[Y*Size.X+X],C)>0; };
 for(int32 Y=0;Y<Size.Y;++Y) for(int32 X=0;X<Size.X;++X)
  for(int32 SY=0;SY<SamplesPerCell;++SY) for(int32 SX=0;SX<SamplesPerCell;++SX)
  {
   const FLinearColor Base=Coarse[Y*Size.X+X]; FLinearColor Result=Base;
   for(int32 C=0;C<3;++C)
   {
    const float Value=Channel(Base,C); if(Value<=0) continue;
    float Guard=1; const float Half=SamplesPerCell*.5f;
    if(X>0 && !Visible(X-1,Y,C)) Guard=FMath::Min(Guard,FMath::Clamp(SX/Half,0.f,1.f));
    if(X+1<Size.X && !Visible(X+1,Y,C)) Guard=FMath::Min(Guard,FMath::Clamp((SamplesPerCell-1-SX)/Half,0.f,1.f));
    if(Y>0 && !Visible(X,Y-1,C)) Guard=FMath::Min(Guard,FMath::Clamp(SY/Half,0.f,1.f));
    if(Y+1<Size.Y && !Visible(X,Y+1,C)) Guard=FMath::Min(Guard,FMath::Clamp((SamplesPerCell-1-SY)/Half,0.f,1.f));
    if(C==0) Result.R=Value*Guard; else if(C==1) Result.G=Value*Guard; else Result.B=Value*Guard;
   }
   OutPixels[(Y*SamplesPerCell+SY)*ResultSize.X+X*SamplesPerCell+SX]=Result;
  }
 return ResultSize;
}
