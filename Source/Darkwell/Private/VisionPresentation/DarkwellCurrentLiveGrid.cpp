#include "VisionPresentation/DarkwellCurrentLiveGrid.h"

namespace
{
 constexpr double CurrentLocalSampleSizeCm=2.5;
 FBox2D XY(const FBox& B) { return FBox2D(FVector2D(B.Min),FVector2D(B.Max)); }
 FIntPoint GridSize(const FBox2D& B) { return FIntPoint(FMath::CeilToInt(B.GetSize().X/CurrentLocalSampleSizeCm),FMath::CeilToInt(B.GetSize().Y/CurrentLocalSampleSizeCm)); }
 bool Upright(const FTransform& T) { return T.GetRotation().RotateVector(FVector::UpVector).Equals(FVector::UpVector,1.e-5); }
}
bool FDarkwellCurrentLiveGrid::FDescriptor::Matches(const FDescriptor& O) const
{
 return PrimitiveKey==O.PrimitiveKey && MeshKey==O.MeshKey && LocalBounds.Equals(O.LocalBounds)
  && RelativeTransform.Equals(O.RelativeTransform);
}
void FDarkwellCurrentLiveGrid::ResetGeometry(FName Id,TConstArrayView<FDescriptor> Descriptors,const FTransform& ActorPose)
{
 ++GeometryResets; Updates=0; Parts.Reset(); RegisteredScale=ActorPose.GetScale3D(); LastLegalPose=ActorPose;
 double Radius=0;
 for(const auto& D:Descriptors)
 {
  auto& P=Parts.AddDefaulted_GetRef(); P.Geometry=D; P.Pose=D.RelativeTransform*ActorPose;
  const FVector Scale=P.Pose.GetScale3D().GetAbs();
  const auto B=XY(D.LocalBounds); const auto Ext=B.GetSize();
  // Each primitive axis has a fixed <=2.5 cm physical footprint; a thin
  // door/handle does not inherit the body width as its Y sample density.
  P.Local.Initialize(Id,B,FMath::Max(Ext.X,Ext.Y)); P.Local.BeginPresent();
  P.Local.PrepareCurrentRaster(B,FIntPoint(FMath::Max(1,FMath::CeilToInt(Ext.X*Scale.X/CurrentLocalSampleSizeCm)),FMath::Max(1,FMath::CeilToInt(Ext.Y*Scale.Y/CurrentLocalSampleSizeCm))));
  P.Coverage.SetNumZeroed(P.Local.GetCells().Num()); P.ObservedAtPose.Init(false,P.Coverage.Num());
  const auto LS=P.Local.GetSize(); P.Corners.SetNumZeroed((LS.X+1)*(LS.Y+1));
  const double Diameter=FVector2D(Ext.X*Scale.X,Ext.Y*Scale.Y).Size();
  const int32 MaxCells=FMath::CeilToInt(Diameter/CurrentLocalSampleSizeCm)+2;
  P.AtlasCells=FIntPoint(MaxCells,MaxCells);
  const auto WB=XY(D.LocalBounds.TransformBy(P.Pose));
  P.Raster.Initialize(Id,WB); P.Raster.BeginPresent();
  P.Raster.PrepareCurrentRaster(WB,GridSize(WB),MaxCells*MaxCells);
  const FBox ActorBounds=D.LocalBounds.TransformBy(D.RelativeTransform);
  Radius=FMath::Max(Radius,FVector2D(ActorBounds.GetCenter()).Size()+FVector2D(ActorBounds.GetExtent()).Size());
 }
 const int32 MaxCells=FMath::CeilToInt(2*Radius*RegisteredScale.GetAbs().GetMax()/CurrentLocalSampleSizeCm)+2;
 AtlasCells=FIntPoint(MaxCells,MaxCells);
}
bool FDarkwellCurrentLiveGrid::MatchesGeometry(TConstArrayView<FDescriptor> D,const FTransform& Pose) const
{
 if(D.Num()!=Parts.Num() || !Pose.GetScale3D().Equals(RegisteredScale) || !Upright(Pose)) return false;
 for(int32 I=0;I<D.Num();++I) if(!Parts[I].Geometry.Matches(D[I]) || !Upright(D[I].RelativeTransform)) return false;
 return true;
}
bool FDarkwellCurrentLiveGrid::Advance(float Dt,const FTransform& ActorPose,TFunctionRef<float(FVector2D)> Query)
{
 if(!Upright(ActorPose)) return false;
 ++Updates; Queries=0; SamplesTouched=0; bFullyObservedAtPose=true;
 for(auto& P:Parts)
 {
  const auto Pose=P.Geometry.RelativeTransform*ActorPose;
  const bool Moved=!Pose.Equals(P.Pose,1.e-6);
  P.Pose=Pose; if(Moved) P.ObservedAtPose.SetRange(0,P.ObservedAtPose.Num(),false);
  const auto B=P.Local.GetBounds(); const auto S=P.Local.GetSize(); const auto Step=B.GetSize()/FVector2D(S);
  auto Legal=[&](FVector2D Local) { ++Queries; const float V=Query(FVector2D(Pose.TransformPosition(FVector(Local,P.Geometry.LocalBounds.GetCenter().Z)))); return FMath::IsFinite(V)?FMath::Clamp(V,0.f,1.f):0.f; };
  for(int32 Y=0;Y<=S.Y;++Y) for(int32 X=0;X<=S.X;++X) P.Corners[Y*(S.X+1)+X]=Legal(B.Min+Step*FVector2D(X,Y));
  for(int32 Y=0;Y<S.Y;++Y) for(int32 X=0;X<S.X;++X)
  {
   const int32 I=Y*S.X+X,K=Y*(S.X+1)+X;
   P.Coverage[I]=FMath::Min(Legal(B.Min+Step*FVector2D(X+.5,Y+.5)),FMath::Min(FMath::Min(P.Corners[K],P.Corners[K+1]),FMath::Min(P.Corners[K+S.X+1],P.Corners[K+S.X+2])));
   if(P.Coverage[I]>=FDarkwellSpatialPropMemory::LegalCoverage) P.ObservedAtPose[I]=true;
  }
  P.Local.Advance(Dt,P.Coverage); SamplesTouched+=P.Coverage.Num();
  for(int32 I=0;I<P.Coverage.Num();++I) bFullyObservedAtPose &= P.ObservedAtPose[I] && P.Local.GetCells()[I].DiscoveredPresent>0;
 }
 LastLegalPose=ActorPose;
 return true;
}
FDarkwellSpatialPropMemory::FCell FDarkwellCurrentLiveGrid::Sample(const FPart& P,FVector2D World,bool bClamp)
{
 const FVector Local=P.Pose.InverseTransformPosition(FVector(World,P.Pose.TransformPosition(P.Geometry.LocalBounds.GetCenter()).Z));
 const auto B=P.Local.GetBounds(); const auto S=P.Local.GetSize(); const FVector2D L(Local);
 if(!bClamp && !B.IsInside(L)) return {};
 const auto UV=(L-B.Min)/B.GetSize();
 const int32 I=FMath::Clamp(FMath::FloorToInt(UV.Y*S.Y),0,S.Y-1)*S.X+FMath::Clamp(FMath::FloorToInt(UV.X*S.X),0,S.X-1);
 auto C=P.Local.GetCells()[I];
 // Preserved appearance is not knowledge of the new world position.
 if(!P.ObservedAtPose[I]) { C.DiscoveredPresent=0; C.AppearanceBlend=0; C.LiveBlend=0; }
 return C;
}
void FDarkwellCurrentLiveGrid::WriteWorldSnapshot(FDarkwellSpatialPropMemory& Out,const FBox2D& Bounds)
{
 const auto S=GridSize(Bounds); auto Cells=Out.PrepareCurrentRaster(Bounds,S,AtlasCells.X*AtlasCells.Y);
 const auto Step=Bounds.GetSize()/FVector2D(S);
 FDarkwellSpatialPropMemory::FCell CompleteEnvelope;
 if(bFullyObservedAtPose)
 {
  CompleteEnvelope.DiscoveredPresent=1;
  CompleteEnvelope.AppearanceBlend=CompleteEnvelope.LiveBlend=1;
  for(const auto& P:Parts) for(const auto& C:P.Local.GetCells()) {
   CompleteEnvelope.AppearanceBlend=FMath::Min(CompleteEnvelope.AppearanceBlend,C.AppearanceBlend);
   CompleteEnvelope.LiveBlend=FMath::Min(CompleteEnvelope.LiveBlend,C.LiveBlend);
  }
 }
 for(int32 Y=0;Y<S.Y;++Y) for(int32 X=0;X<S.X;++X)
 {
  auto& C=Cells[Y*S.X+X]; C={}; const auto World=Bounds.Min+Step*FVector2D(X+.5,Y+.5);
  for(const auto& P:Parts)
  {
   const auto V=Sample(P,World,false);
   if(V.DiscoveredPresent>C.DiscoveredPresent || V.AppearanceBlend>C.AppearanceBlend) C=V;
  }
  // The compatibility world raster describes a geometry-clipped envelope.
  // Once every real primitive sample is observed, holes outside geometry are
  // not observation cuts. Match the original fully observed snapshot envelope;
  // neither source rendering nor fine ownership reads these padding cells.
  if(C.DiscoveredPresent==0 && bFullyObservedAtPose) C=CompleteEnvelope;
 }
}
void FDarkwellCurrentLiveGrid::WritePartRasters(TFunctionRef<float(FVector2D)> Query,bool bTransient)
{
 for(auto& P:Parts)
 {
  const auto B=XY(P.Geometry.LocalBounds.TransformBy(P.Pose)); const auto S=GridSize(B);
  auto Cells=P.Raster.PrepareCurrentRaster(B,S,P.AtlasCells.X*P.AtlasCells.Y); const auto Step=B.GetSize()/FVector2D(S);
  for(int32 Y=0;Y<S.Y;++Y) for(int32 X=0;X<S.X;++X)
  {
   const auto Min=B.Min+Step*FVector2D(X,Y); auto C=Sample(P,Min+Step*.5,true);
   float Coverage=1;
   for(const FVector2D Offset : {FVector2D(0),FVector2D(1,0),FVector2D(0,1),FVector2D(1),FVector2D(.5)})
   { ++Queries; Coverage=FMath::Min(Coverage,Query(Min+Step*Offset)); }
   C.CurrentLegalCoverage=Coverage;
   if(Coverage<FDarkwellSpatialPropMemory::LegalCoverage && bTransient) C.AppearanceBlend=0;
   Cells[Y*S.X+X]=C;
  }
 }
}
bool FDarkwellCurrentLiveGrid::HasObservedContributionAt(FVector2D World,int32 PrimitiveIndex) const
{
 for(int32 I=0;I<Parts.Num();++I)
 {
  if(PrimitiveIndex!=INDEX_NONE && PrimitiveIndex!=I) continue;
  const auto C=Sample(Parts[I],World,false);
  if(C.DiscoveredPresent>0 && C.AppearanceBlend>0) return true;
 }
 return false;
}
uint64 FDarkwellCurrentLiveGrid::StateHash() const
{
 uint64 H=1469598103934665603ull;
 for(const auto& P:Parts) for(const auto& C:P.Local.GetCells())
  for(float V : {C.DiscoveredPresent,C.AppearanceBlend,C.LiveBlend})
  { H=(H^uint64(FMath::RoundToInt(V*100000)))*1099511628211ull; }
 return H;
}
void FDarkwellCurrentLiveGrid::CopyAtlasWithClampBorder(TConstArrayView<FLinearColor> Pixels,
 FIntPoint Size,FIntPoint Atlas,TArrayView<FFloat16Color> Out)
{
 check(Size.X>0 && Size.Y>0 && Size.X<Atlas.X && Size.Y<Atlas.Y);
 check(Pixels.Num()==Size.X*Size.Y && Out.Num()==Atlas.X*Atlas.Y);
 FMemory::Memzero(Out.GetData(),Out.Num()*sizeof(FFloat16Color));
 // At the physical maximum bound bilinear reads half the last texel and
 // half the next texel. Zero padding would make a fully observed side 50%
 // dithered. Reproduce TA_Clamp of the logical texture without changing its
 // coordinates, interior AA samples or legal gate. Illegal edge texels stay
 // zero. All other padding is cleared, including a previous larger rectangle.
 for(int32 Y=0;Y<=Size.Y;++Y) for(int32 X=0;X<=Size.X;++X)
  Out[Y*Atlas.X+X]=FFloat16Color(Pixels[FMath::Min(Y,Size.Y-1)*Size.X+FMath::Min(X,Size.X-1)]);
}
