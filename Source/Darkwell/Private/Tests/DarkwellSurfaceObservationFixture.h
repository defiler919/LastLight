#pragma once

#if WITH_DEV_AUTOMATION_TESTS
#include "CoreMinimal.h"

// Test-only analytic specification, NEVER a gameplay visibility service.
// Models finite planar receivers and open-segment / solid-box intersections.
// Legal source policy is deliberately absent: tests obtain it from Runtime.
namespace Darkwell::SurfaceObservationTests
{
constexpr double ContactCm = 1.e-4;

struct FPatch
{
 FName Id;
 FVector Center, Normal, U, V; // U/V are half edges, normal is geometric outward.
 TArray<FVector> Support() const
 { return {Center, Center-U-V, Center+U-V, Center-U+V, Center+U+V}; }
};

struct FSolid
{
 FBox LocalBounds;
 FTransform Pose = FTransform::Identity;
};

// Slab interval clipped to the open eye-target segment. A receiver's own solid
// is NOT excluded. Touching its endpoint is legal; traversing its interior isn't.
inline bool CrossesInterior(FVector Eye, FVector Target, const FSolid& Solid)
{
 Eye = Solid.Pose.InverseTransformPosition(Eye);
 Target = Solid.Pose.InverseTransformPosition(Target);
 const FVector D = Target-Eye;
 double Enter = 0, Leave = 1;
 for(int Axis=0; Axis<3; ++Axis)
 {
  const double Low=Solid.LocalBounds.Min[Axis]+ContactCm;
  const double High=Solid.LocalBounds.Max[Axis]-ContactCm;
  if(Low>=High) return false;
  if(FMath::Abs(D[Axis])<1.e-12)
  { if(Eye[Axis]<=Low || Eye[Axis]>=High) return false; continue; }
  double A=(Low-Eye[Axis])/D[Axis], B=(High-Eye[Axis])/D[Axis];
  if(A>B) Swap(A,B);
  Enter=FMath::Max(Enter,A); Leave=FMath::Min(Leave,B);
  if(Enter>=Leave) return false;
 }
 return Enter<Leave && Leave>0 && Enter<1;
}

inline bool GeometricSupport(FVector Eye, const FPatch& Patch, TConstArrayView<FSolid> Solids)
{
 for(const FVector P:Patch.Support())
 {
  if(FVector::DotProduct(Patch.Normal,Eye-P)<=ContactCm) return false;
  for(const FSolid& Solid:Solids) if(CrossesInterior(Eye,P,Solid)) return false;
 }
 return true;
}

inline FPatch Top(double Z)
{ return {TEXT("Top"),{0,0,Z},FVector::UpVector,{10,0,0},{0,10,0}}; }
inline FPatch Front()
{ return {TEXT("Front"),{-40,0,100},{-1,0,0},{0,10,0},{0,0,10}}; }
inline FPatch Side()
{ return {TEXT("Side"),{0,30,100},{0,1,0},{10,0,0},{0,0,10}}; }
inline FSolid Cabinet()
{ return {FBox(FVector(-40,-30,0),FVector(40,30,200))}; }

// Executable reference ledger for one stable object/content revision. Each key
// denotes ONE support cell within a surface, not automatic whole-face knowledge.
// A production ledger must use Runtime-issued surface evidence, never this bool.
struct FReferenceLedger
{
 TSet<FName> Known;
 bool bWholeRecognized=false, bBlocked=false;
 void Observe(FName Cell, bool bLegal) { if(bLegal && !bBlocked) Known.Add(Cell); }
 void Clear() { Known.Reset(); }
};
}
#endif
