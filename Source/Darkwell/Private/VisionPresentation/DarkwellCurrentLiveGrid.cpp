#include "VisionPresentation/DarkwellCurrentLiveGrid.h"
#include "VisionPresentation/DarkwellHistoricalVisibilitySweep.h"

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
  P.Coverage.SetNumZeroed(P.Local.GetCells().Num()); P.LastLegalCaptureMask.Init(false,P.Coverage.Num()); P.CurrentLegalObservationMask.Init(false,P.Coverage.Num());
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
 WholeAppearance.Initialize(Id,FBox2D(FVector2D(0),FVector2D(1)),1); WholeAppearance.BeginPresent();
 FBox ObjectBounds(ForceInit);
 for(const auto& P:Parts) ObjectBounds+=P.Geometry.LocalBounds.TransformBy(P.Geometry.RelativeTransform);
 const auto B=XY(ObjectBounds); const auto E=B.GetSize();
 ObservationSize=FIntPoint(FMath::Max(1,FMath::CeilToInt(E.X*FMath::Abs(RegisteredScale.X)/CurrentLocalSampleSizeCm)),FMath::Max(1,FMath::CeilToInt(E.Y*FMath::Abs(RegisteredScale.Y)/CurrentLocalSampleSizeCm)));
 const auto Step=E/FVector2D(ObservationSize); ObservationStepCm=Step*FVector2D(RegisteredScale.GetAbs());
 ObservationFootprint.Init(false,ObservationSize.X*ObservationSize.Y);
 ObservationPartIndices.SetNum(Parts.Num());
 for(int32 PartIndex=0;PartIndex<Parts.Num();++PartIndex)
 {
  const auto& P=Parts[PartIndex]; auto& Indices=ObservationPartIndices[PartIndex]; Indices.Init(INDEX_NONE,ObservationFootprint.Num());
  for(int32 I=0;I<Indices.Num();++I)
  {
   const auto Center=B.Min+Step*FVector2D(I%ObservationSize.X+.5,I/ObservationSize.X+.5);
   const auto L=FVector2D(P.Geometry.RelativeTransform.InverseTransformPosition(FVector(Center,0)));
   const auto PB=P.Local.GetBounds(); const auto PS=P.Local.GetSize();
   if(!PB.IsInside(L)) continue;
   const auto UV=(L-PB.Min)/PB.GetSize();
   Indices[I]=FMath::Clamp(FMath::FloorToInt(UV.Y*PS.Y),0,PS.Y-1)*PS.X+FMath::Clamp(FMath::FloorToInt(UV.X*PS.X),0,PS.X-1);
   ObservationFootprint[I]=true;
  }
 }
}

void FDarkwellCurrentLiveGrid::BuildCurrentLegalObservationMask(TBitArray<>& Out) const
{
 Out.Init(false,ObservationFootprint.Num());
 for(int32 PartIndex=0;PartIndex<Parts.Num();++PartIndex)
 {
  const auto& P=Parts[PartIndex]; const auto& Indices=ObservationPartIndices[PartIndex];
  for(int32 I=0;I<Indices.Num();++I)
   if(Indices[I]!=INDEX_NONE && P.CurrentLegalObservationMask[Indices[I]]) Out[I]=true;
 }
}

void FDarkwellCurrentLiveGrid::ApplyWholeObjectPresentation(float Dt,FDarkwellSpatialPropMemory& Snapshot,
 TFunctionRef<float(FVector2D)> OcclusionPermission)
{
 WholeAppearance.Advance(Dt,TArray<float>{1.f});
 const auto Appearance=WholeAppearance.GetCells()[0];
 bool bWholeUnoccluded=true;
 auto Apply=[&](FDarkwellSpatialPropMemory& Raster)
 {
  const auto B=Raster.GetBounds(); const auto S=Raster.GetSize(); const auto Step=B.GetSize()/FVector2D(S);
  auto Cells=Raster.PrepareCurrentRaster(B,S);
  for(int32 I=0;I<Cells.Num();++I)
  {
   const auto Min=B.Min+Step*FVector2D(I%S.X,I/S.X); float Gate=1;
   for(auto O : {FVector2D(0),FVector2D(1,0),FVector2D(0,1),FVector2D(1),FVector2D(.5)}) Gate=FMath::Min(Gate,OcclusionPermission(Min+Step*O));
   auto& C=Cells[I]; const bool Allowed=Gate>=FDarkwellSpatialPropMemory::LegalCoverage;
   bWholeUnoccluded &= Allowed;
   C.DiscoveredPresent=Allowed?1:0;
   C.AppearanceBlend=Allowed?FMath::Max(C.AppearanceBlend,Appearance.AppearanceBlend):0;
   C.LiveBlend=Allowed?FMath::Max(C.LiveBlend,Appearance.LiveBlend):0;
   // CurrentLegalCoverage remains the real field. Whole permission never feeds it.
  }
 };
 for(auto& P:Parts) { P.bWholePresentation=true; Apply(P.Raster); }
 Apply(Snapshot);
 bFullyObservedAtPose=bWholeUnoccluded;
}
bool FDarkwellCurrentLiveGrid::MatchesGeometry(TConstArrayView<FDescriptor> D,const FTransform& Pose) const
{
 if(D.Num()!=Parts.Num() || !Pose.GetScale3D().Equals(RegisteredScale) || !Upright(Pose)) return false;
 for(int32 I=0;I<D.Num();++I) if(!Parts[I].Geometry.Matches(D[I]) || !Upright(D[I].RelativeTransform)) return false;
 return true;
}
bool FDarkwellCurrentLiveGrid::HasAnyLegalObservation(const FTransform& ActorPose,
 TFunctionRef<float(FVector2D)> Query,TFunctionRef<bool(const FBox2D&,float&)> Uniform)
{
 Queries=0; SamplesTouched=0;
 for(const auto& P:Parts)
 {
  const auto Pose=P.Geometry.RelativeTransform*ActorPose;
  float V;
  if(Uniform(XY(P.Geometry.LocalBounds.TransformBy(Pose)),V)) { if(V>=FDarkwellSpatialPropMemory::LegalCoverage) return true; continue; }
  const auto B=P.Local.GetBounds(); const auto S=P.Local.GetSize(); const auto Step=B.GetSize()/FVector2D(S);
  for(int32 Y=0;Y<S.Y;++Y) for(int32 X=0;X<S.X;++X)
  {
   // Existential contact stops at the first proven original footprint; it never
   // maintains a confirmed object's dense tentative/capture observation masks.
   ++SamplesTouched; bool Legal=true;
   for(auto O:{FVector2D(.5),FVector2D(0),FVector2D(1,0),FVector2D(0,1),FVector2D(1)})
   {
    const auto L=B.Min+Step*(FVector2D(X,Y)+O); ++Queries;
    if(Query(FVector2D(Pose.TransformPosition(FVector(L,P.Geometry.LocalBounds.GetCenter().Z))))<FDarkwellSpatialPropMemory::LegalCoverage) { Legal=false; break; }
   }
   if(Legal) return true;
  }
 }
 return false;
}
void FDarkwellCurrentLiveGrid::AdvanceWholeUnoccluded(float Dt,const FTransform& ActorPose,
 FDarkwellSpatialPropMemory& Snapshot,const FBox2D& Bounds,TConstArrayView<float> Coverage)
{
 ++Updates; Queries=0; SamplesTouched=0;
 WholeAppearance.Advance(Dt,TArray<float>{1.f});
 auto Cell=WholeAppearance.GetCells()[0];
 const auto Size=GridSize(Bounds); auto Cells=Snapshot.PrepareCurrentRaster(Bounds,Size,AtlasCells.X*AtlasCells.Y);
 check(Cells.Num()==Coverage.Num());
 for(int32 I=0;I<Cells.Num();++I) { Cell.CurrentLegalCoverage=Coverage[I]; Cells[I]=Cell; }
 for(auto& P:Parts)
 {
  P.Pose=P.Geometry.RelativeTransform*ActorPose;
  P.bWholePresentation=P.bUniformWholePresentation=true;
  P.WholeBounds=XY(P.Geometry.LocalBounds.TransformBy(P.Pose));
  // Uniform RGB is object presentation. A is unused by the original source
  // material; authoritative coverage stays in the canonical raster above.
  P.WholePixel=FLinearColor(Cell.AppearanceBlend,Cell.LiveBlend,0,0);
  P.CurrentLegalObservationMask.Empty(); P.LastLegalCaptureMask.Empty();
 }
 bFullyObservedAtPose=true; LastLegalPose=ActorPose;
}
bool FDarkwellCurrentLiveGrid::Advance(float Dt,const FTransform& ActorPose,TFunctionRef<float(FVector2D)> Query,TFunction<bool(const FBox2D&,float&)> Uniform)
{
 if(!Upright(ActorPose)) return false;
 ++Updates; Queries=0; SamplesTouched=0; bFullyObservedAtPose=true;
 for(auto& P:Parts)
 {
  const auto Pose=P.Geometry.RelativeTransform*ActorPose;
  const bool Moved=!Pose.Equals(P.Pose,1.e-6);
  P.bUniformWholePresentation=false; P.bWholePresentation=false;
  if(P.LastLegalCaptureMask.Num()!=P.Coverage.Num()) P.LastLegalCaptureMask.Init(false,P.Coverage.Num());
  if(P.CurrentLegalObservationMask.Num()!=P.Coverage.Num()) P.CurrentLegalObservationMask.Init(false,P.Coverage.Num());
  P.Pose=Pose; if(Moved) P.LastLegalCaptureMask.SetRange(0,P.LastLegalCaptureMask.Num(),false);
  const auto B=P.Local.GetBounds(); const auto S=P.Local.GetSize(); const auto Step=B.GetSize()/FVector2D(S);
  auto Legal=[&](FVector2D Local) { ++Queries; const float V=Query(FVector2D(Pose.TransformPosition(FVector(Local,P.Geometry.LocalBounds.GetCenter().Z)))); return FMath::IsFinite(V)?FMath::Clamp(V,0.f,1.f):0.f; };
  float Constant=0; const bool ConstantRegion=Uniform && Uniform(XY(P.Geometry.LocalBounds.TransformBy(Pose)),Constant);
  if(!ConstantRegion) for(int32 Y=0;Y<=S.Y;++Y) for(int32 X=0;X<=S.X;++X) P.Corners[Y*(S.X+1)+X]=Legal(B.Min+Step*FVector2D(X,Y));
  for(int32 Y=0;Y<S.Y;++Y) for(int32 X=0;X<S.X;++X)
  {
   const int32 I=Y*S.X+X,K=Y*(S.X+1)+X;
   P.Coverage[I]=ConstantRegion?Constant:FMath::Min(Legal(B.Min+Step*FVector2D(X+.5,Y+.5)),FMath::Min(FMath::Min(P.Corners[K],P.Corners[K+1]),FMath::Min(P.Corners[K+S.X+1],P.Corners[K+S.X+2])));
   P.CurrentLegalObservationMask[I]=P.Coverage[I]>=FDarkwellSpatialPropMemory::LegalCoverage;
   if(P.CurrentLegalObservationMask[I]) P.LastLegalCaptureMask[I]=true;
  }
  P.Local.Advance(Dt,P.Coverage); SamplesTouched+=P.Coverage.Num();
  for(int32 I=0;I<P.Coverage.Num();++I) bFullyObservedAtPose &= P.LastLegalCaptureMask[I] && P.Local.GetCells()[I].DiscoveredPresent>0;
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
 if(!P.LastLegalCaptureMask[I]) { C.DiscoveredPresent=0; C.AppearanceBlend=0; C.LiveBlend=0; }
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
void FDarkwellCurrentLiveGrid::WritePartRasters(TFunctionRef<float(FVector2D)> Query,bool bTransient,TFunction<bool(const FBox2D&,float&)> Uniform)
{
 for(auto& P:Parts)
 {
  const auto B=XY(P.Geometry.LocalBounds.TransformBy(P.Pose)); const auto S=GridSize(B);
  auto Cells=P.Raster.PrepareCurrentRaster(B,S,P.AtlasCells.X*P.AtlasCells.Y); const auto Step=B.GetSize()/FVector2D(S);
  float Constant=0; const bool ConstantRegion=Uniform && Uniform(B,Constant);
  for(int32 Y=0;Y<S.Y;++Y) for(int32 X=0;X<S.X;++X)
  {
   const auto Min=B.Min+Step*FVector2D(X,Y); auto C=Sample(P,Min+Step*.5,true);
   float Coverage=ConstantRegion?Constant:1;
   if(!ConstantRegion) for(const FVector2D Offset : {FVector2D(0),FVector2D(1,0),FVector2D(0,1),FVector2D(1),FVector2D(.5)})
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
  const auto& P=Parts[I];
  if(P.bWholePresentation)
  {
   const auto L=FVector2D(P.Pose.InverseTransformPosition(FVector(World,P.Pose.GetLocation().Z)));
   if(!P.Local.GetBounds().IsInside(L)) continue;
   if(P.bUniformWholePresentation) return P.WholePixel.R>0;
   const auto B=P.Raster.GetBounds(); const auto S=P.Raster.GetSize(); const auto UV=(World-B.Min)/B.GetSize();
   const int32 Cell=FMath::Clamp(FMath::FloorToInt(UV.Y*S.Y),0,S.Y-1)*S.X+FMath::Clamp(FMath::FloorToInt(UV.X*S.X),0,S.X-1);
   if(P.Raster.GetCells()[Cell].DiscoveredPresent>0) return true;
   continue;
  }
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

bool FDarkwellCurrentLiveGrid::BuildSweptObservationMask(const FTransform& ActorPose,
 const FDarkwellFogVisualSourceSnapshot& Previous,const FDarkwellFogVisualSourceSnapshot& Current,
 TConstArrayView<FDarkwellFogVisualSegment> Occluders,TBitArray<>& Out,bool bContactOnly)
{
 Queries=0; SamplesTouched=0; Out.Empty();
 if(!bContactOnly) Out.Init(false,ObservationFootprint.Num());
 bool Contact=false;
 for(int32 PartIndex=0;PartIndex<Parts.Num();++PartIndex)
 {
  const auto& P=Parts[PartIndex]; const auto Pose=P.Geometry.RelativeTransform*ActorPose;
  const auto B=P.Local.GetBounds(); const auto S=P.Local.GetSize(); const auto Step=B.GetSize()/FVector2D(S);
  TBitArray<> Swept(false,S.X*S.Y);
  for(int32 Y=0;Y<S.Y;++Y) for(int32 X=0;X<S.X;++X)
  {
   FVector2D Points[5]; int32 K=0;
   for(auto O:{FVector2D(0),FVector2D(1,0),FVector2D(1),FVector2D(0,1),FVector2D(.5)})
   { const auto L=B.Min+Step*(FVector2D(X,Y)+O); Points[K++]=FVector2D(Pose.TransformPosition(FVector(L,P.Geometry.LocalBounds.GetCenter().Z))); }
   ++SamplesTouched;
   if(FDarkwellHistoricalVisibilitySweep::ProvePointSetCoverage(Previous,Current,Occluders,Points,Queries))
   { Contact=true; if(bContactOnly) return true; Swept[Y*S.X+X]=true; }
  }
  if(!bContactOnly) for(int32 I=0;I<Out.Num();++I)
   if(ObservationPartIndices[PartIndex][I]!=INDEX_NONE && Swept[ObservationPartIndices[PartIndex][I]]) Out[I]=true;
 }
 return Contact;
}
