#include "SightWeaveHardCoverage.h"
#include "SightWeaveWorldSubsystem.h"
#include "SightWeaveSettings.h"
#include "Algo/BinarySearch.h"

namespace {constexpr double BinCm=80;FIntPoint Bin(FVector2D P){return FIntPoint(FMath::FloorToInt(P.X/BinCm),FMath::FloorToInt(P.Y/BinCm));}}
void FSightWeaveHardCoverage::AddBoundary(FVector2D A,FVector2D B,double Padding)
{
 FBox2D Box(ForceInit);Box+=A;Box+=B;Box=Box.ExpandBy(Padding);
 const int32 I=Boundaries.Add(Box);const auto Min=Bin(Box.Min),Max=Bin(Box.Max);
 Edges.Emplace(A,B);BoundaryPadding=FMath::Max(BoundaryPadding,Padding);
 EdgePredicates.Add(BuildingPredicate);
 for(int Y=Min.Y;Y<=Max.Y;++Y)for(int X=Min.X;X<=Max.X;++X)BoundaryBins.FindOrAdd({X,Y}).Add(I);
}
FSightWeaveHardCoverage::FSightWeaveHardCoverage(USightWeaveWorldSubsystem* InRuntime,
 TSharedPtr<const FSightWeaveFrameSnapshot,ESPMode::ThreadSafe> Snapshot,
 FSightWeaveKnowledgeOwnerId InOwner,FSightWeaveFloorId InFloor,double Height)
 :Runtime(InRuntime),Frame(MoveTemp(Snapshot)),Owner(InOwner),Floor(InFloor),Z(Height)
{
 check(Frame);
 const auto& T=GetDefault<USightWeaveSettings>()->GeometryTolerances;
 const double E=FMath::Max3(T.PointOnEdgeEpsilon,T.PointInPolygonEpsilon,T.DuplicateVertexEpsilon);
 auto Eligible=[&](const auto& D){return D.bActive && D.KnowledgeOwnerId==Owner && D.FloorId==Floor
  && Z>=D.HeightRange.ZMin-T.HeightOverlapEpsilon && Z<=D.HeightRange.ZMax+T.HeightOverlapEpsilon;};
 const auto* FloorEntry=Frame->Floors.FindByPredicate([&](const auto& F){return F.FloorId==Floor;});
 if(!FloorEntry || !FloorEntry->bEnabled || !FloorEntry->bActiveForQueries
  || Z<FloorEntry->HeightRange.ZMin-T.HeightOverlapEpsilon || Z>FloorEntry->HeightRange.ZMax+T.HeightOverlapEpsilon)return;
 for(const auto& V:Frame->VisionSources)if(Eligible(V.Description) && V.Polygon.IsValid())
 {
  const FBox2D VB=FBox2D(V.Polygon.BoundsMin,V.Polygon.BoundsMax).ExpandBy(E);
  if(V.Description.IlluminationPolicy==ESightWeaveIlluminationPolicy::BypassLegalIllumination)PossibleSupport.Add(VB);
  else for(int I:V.CompatibleIlluminationSourceIndices)
  {
   if(!Frame->IlluminationSources.IsValidIndex(I))continue;
   const auto& L=Frame->IlluminationSources[I];if(!Eligible(L.Description) || !L.Polygon.IsValid())continue;
   const auto LB=FBox2D(L.Polygon.BoundsMin,L.Polygon.BoundsMax).ExpandBy(E);
   if(VB.Intersect(LB))PossibleSupport.Emplace(FVector2D(FMath::Max(VB.Min.X,LB.Min.X),FMath::Max(VB.Min.Y,LB.Min.Y)),FVector2D(FMath::Min(VB.Max.X,LB.Max.X),FMath::Min(VB.Max.Y,LB.Max.Y)));
  }
 }
 auto Polygon=[&](const auto& P){for(int I=0;I<P.Vertices.Num();++I)AddBoundary(FVector2D(P.Vertices[I]),FVector2D(P.Vertices[(I+1)%P.Vertices.Num()]),E);};
 for(const auto& V:Frame->VisionSources)if(Eligible(V.Description))
 {
  BuildingPredicate=int32(&V-Frame->VisionSources.GetData());EligibleVisions.Add(BuildingPredicate);
  Polygon(V.Polygon);
  if(V.Description.Shape!=ESightWeaveSourceShape::Radial)
  {
   Circles.Emplace(V.PolarOrigin,V.Description.Range+T.PointOnEdgeEpsilon);
   Circles.Emplace(V.PolarOrigin,V.Description.NearAwarenessRadius+T.PointOnEdgeEpsilon);
   CirclePredicates.Add(BuildingPredicate);CirclePredicates.Add(BuildingPredicate);
   const double A=FMath::Acos(V.NominalMinimumCosine);
   for(double Sign:{-1.,1.})
   {const double S=FMath::Sin(A*Sign),C=FMath::Cos(A*Sign);const auto F=V.NominalForward;
    AddBoundary(V.PolarOrigin,V.PolarOrigin+FVector2D(F.X*C-F.Y*S,F.X*S+F.Y*C)*(V.Description.Range+E),E);}
  }
 }
 for(const auto& L:Frame->IlluminationSources)if(Eligible(L.Description))
 {
  const int I=int32(&L-Frame->IlluminationSources.GetData());EligibleLights.Add(I);
  BuildingPredicate=Frame->VisionSources.Num()+I;Polygon(L.Polygon);
 }
 BuildingPredicate=-1;
 for(const auto& S:Frame->HardSuppressions)
  if(S.Description.bEnabled && S.Description.FloorId==Floor && Z>=S.Description.HeightRange.ZMin-T.HeightOverlapEpsilon && Z<=S.Description.HeightRange.ZMax+T.HeightOverlapEpsilon)
   {Circles.Emplace(S.Description.Center,S.Description.Radius);CirclePredicates.Add(-1);}
 for(const auto& F:Frame->Floors)if(F.FloorId==Floor)
 {const auto A=F.BoundsMin-FVector2D(E),B=F.BoundsMax+FVector2D(E);
  AddBoundary(A,{B.X,A.Y},E);AddBoundary({B.X,A.Y},B,E);AddBoundary(B,{A.X,B.Y},E);AddBoundary({A.X,B.Y},A,E);}
}
bool FSightWeaveHardCoverage::Contains(FVector2D P) const
{
 auto* R=Runtime.Get();if(!R || !R->IsHardCoverageReady() || P.ContainsNaN())return false;
 if(const auto* Found=Points.Find(P))return *Found;
 const bool V=ContainsUncached(P);
 // Bound exact-coordinate memoization. Eviction changes cost, never authority.
 if(Points.Num()>=65536)Points.Reset();Points.Add(P,V);return V;
}
bool FSightWeaveHardCoverage::ContainsUncached(FVector2D P) const
{
 auto* R=Runtime.Get();if(!R || !R->IsHardCoverageReady() || P.ContainsNaN())return false;
 if(!PossibleSupport.ContainsByPredicate([&](const FBox2D& B){return P.X>=B.Min.X && P.Y>=B.Min.Y && P.X<=B.Max.X && P.Y<=B.Max.Y;}))return false;
 ++ExactQueries;R->QueryCapturedEffectiveLive(*Frame,Owner,Floor,FVector(P,Z),Scratch);
 return Scratch.bAuthoritative && Scratch.bVisible;
}
bool FSightWeaveHardCoverage::TryUniform(const FBox2D& B,bool& V) const
{
 if(!B.bIsValid || B.GetSize().GetMin()<=0 || !Runtime.IsValid() || !Runtime->IsHardCoverageReady())return false;
 if(!PossibleSupport.ContainsByPredicate([&](const FBox2D& Support){return B.Intersect(Support);}))
 {V=false;++UniformHits;return true;}
 const FRegionKey Key{B.Min,B.Max};
 if(const auto* Cached=Regions.Find(Key)){if(*Cached<0)return false;V=*Cached>0;++UniformHits;return true;}
 auto Ambiguous=[&](){if(Regions.Num()>=32768)Regions.Reset();Regions.Add(Key,-1);return false;};
 TArray<int32,TInlineAllocator<16>> Mixed;
 const auto Min=Bin(B.Min),Max=Bin(B.Max);
 for(int Y=Min.Y;Y<=Max.Y;++Y)for(int X=Min.X;X<=Max.X;++X)
  if(const auto* List=BoundaryBins.Find({X,Y}))for(int I:*List)if(B.Intersect(Boundaries[I]))
  {
   // An edge's AABB is only broad phase. Diagonal shadow edges must not turn
   // their entire bounding rectangle into unresolved fine-sample work.
   const auto Box=B.ExpandBy(BoundaryPadding);const auto A=Edges[I].Key,D=Edges[I].Value-A;
   double Lo=0,Hi=1;
   for(int Axis=0;Axis<2 && Lo<=Hi;++Axis)
   {
    if(D[Axis]==0){if(A[Axis]<Box.Min[Axis] || A[Axis]>Box.Max[Axis])Lo=2;}
    else {double L=(Box.Min[Axis]-A[Axis])/D[Axis],H=(Box.Max[Axis]-A[Axis])/D[Axis];if(L>H)Swap(L,H);Lo=FMath::Max(Lo,L);Hi=FMath::Min(Hi,H);}
   }
   if(Lo<=Hi)Mixed.AddUnique(EdgePredicates[I]);
  }
 for(int CircleIndex=0;CircleIndex<Circles.Num();++CircleIndex)
 {
  const auto& C=Circles[CircleIndex];
  const auto P=C.Key;const double Near=FVector2D::Distance(P,{FMath::Clamp(P.X,B.Min.X,B.Max.X),FMath::Clamp(P.Y,B.Min.Y,B.Max.Y)});
  const double Far=FMath::Max(FMath::Max(FVector2D::Distance(P,B.Min),FVector2D::Distance(P,B.Max)),
   FMath::Max(FVector2D::Distance(P,{B.Min.X,B.Max.Y}),FVector2D::Distance(P,{B.Max.X,B.Min.Y})));
  if(Near<=C.Value && Far>=C.Value)Mixed.AddUnique(CirclePredicates[CircleIndex]);
 }
 if(!Mixed.IsEmpty())
 {
  // Three-valued composition. A light edge in a proven blind region cannot
  // force fine work; conversely any uncertainty that can affect EffectiveLive
  // falls through to the exact query. Geometry evaluation is shared with Runtime.
  bool AnyProven=false,AnyUncertain=false;
  const auto Center=B.GetCenter();
  for(int I:EligibleVisions)
  {
   const int Vision=Mixed.Contains(I)?-1:Runtime->ContainsCapturedGeometry(*Frame,true,I,Center)?1:0;
   if(Vision==0)continue;
   const auto& Source=Frame->VisionSources[I];
   int Light=Source.Description.IlluminationPolicy==ESightWeaveIlluminationPolicy::BypassLegalIllumination?1:0;
   if(Light==0)for(int L:Source.CompatibleIlluminationSourceIndices)
   {
    if(!EligibleLights.Contains(L))continue;
    const int Value=Mixed.Contains(Frame->VisionSources.Num()+L)?-1:Runtime->ContainsCapturedGeometry(*Frame,false,L,Center)?1:0;
    if(Value==1){Light=1;break;}if(Value==-1)Light=-1;
   }
   if(Light==0)continue;
   AnyProven|=Vision==1 && Light==1;
   AnyUncertain|=Vision==-1 || Light==-1;
  }
  if(!AnyProven && !AnyUncertain){V=false;++UniformHits;if(Regions.Num()>=32768)Regions.Reset();Regions.Add(Key,0);return true;}
  if(!AnyProven || Mixed.Contains(-1))return Ambiguous();
 }
 V=Contains(B.GetCenter());++UniformHits;if(Regions.Num()>=32768)Regions.Reset();Regions.Add(Key,V?1:0);return true;
}

FSightWeaveHardCoverageSet::FSightWeaveHardCoverageSet(USightWeaveWorldSubsystem& R,
 TSharedPtr<const FSightWeaveFrameSnapshot,ESPMode::ThreadSafe> S,
 FSightWeaveKnowledgeOwnerId O,FSightWeaveFloorId F):Runtime(&R),Frame(MoveTemp(S)),Owner(O),Floor(F)
{
 const double E=GetDefault<USightWeaveSettings>()->GeometryTolerances.HeightOverlapEpsilon;
 for(const auto& V:Frame->VisionSources)if(V.Description.bActive && V.Description.KnowledgeOwnerId==Owner && V.Description.FloorId==Floor && V.Polygon.IsValid())
  Bounds+=FBox2D(V.Polygon.BoundsMin,V.Polygon.BoundsMax);
 auto Add=[&](const FSightWeaveHeightRange& H,double Padding){Cuts.AddUnique(double(H.ZMin)-Padding);Cuts.AddUnique(double(H.ZMax)+Padding);};
 for(const auto& D:Frame->Floors)if(D.FloorId==Floor)Add(D.HeightRange,E);
 for(const auto& V:Frame->VisionSources)if(V.Description.KnowledgeOwnerId==Owner && V.Description.FloorId==Floor)Add(V.Description.HeightRange,E);
 for(const auto& L:Frame->IlluminationSources)if(L.Description.KnowledgeOwnerId==Owner && L.Description.FloorId==Floor)Add(L.Description.HeightRange,E);
 for(const auto& H:Frame->HardSuppressions)if(H.Description.FloorId==Floor)Add(H.Description.HeightRange,E);
 Cuts.Sort();
 if(Cuts.IsEmpty()){HeightClasses.Add(0);return;}
 TMap<FString,int32> Classes;
 for(int I=0;I<2*Cuts.Num()+1;++I)
 {
  const int C=I/2;
  const double Z=I%2?Cuts[C]:C==0?Cuts[C]-1:C==Cuts.Num()?Cuts[C-1]+1:(Cuts[C-1]+Cuts[C])*.5;
  FString Signature;
  auto Bit=[&](const auto& H){Signature.AppendChar(Z>=double(H.ZMin)-E && Z<=double(H.ZMax)+E?TEXT('1'):TEXT('0'));};
  for(const auto& D:Frame->Floors)if(D.FloorId==Floor)Bit(D.HeightRange);
  for(const auto& V:Frame->VisionSources)if(V.Description.KnowledgeOwnerId==Owner && V.Description.FloorId==Floor)Bit(V.Description.HeightRange);
  for(const auto& L:Frame->IlluminationSources)if(L.Description.KnowledgeOwnerId==Owner && L.Description.FloorId==Floor)Bit(L.Description.HeightRange);
  for(const auto& H:Frame->HardSuppressions)if(H.Description.FloorId==Floor)Bit(H.Description.HeightRange);
  if(!Classes.Contains(Signature))Classes.Add(Signature,Classes.Num());
  HeightClasses.Add(Classes.FindChecked(Signature));
 }
}
bool FSightWeaveHardCoverageSet::IsReady() const
{return Runtime.IsValid() && Runtime->IsHardCoverageReady() && Frame.IsValid() && Frame->bPublished;}

TSharedRef<FSightWeaveHardCoverage> FSightWeaveHardCoverageSet::AtHeight(double Z) const
{
 const int32 I=Algo::LowerBound(Cuts,Z);
 const int32 Key=HeightClasses[I<Cuts.Num() && Cuts[I]==Z?2*I+1:2*I];
 if(const auto* P=Planes.Find(Key))return *P;
 // A retained immutable frame can outlive its world. Queries on that view are
 // fail-closed, including a height class first requested after world teardown.
 auto P=MakeShared<FSightWeaveHardCoverage>(Runtime.Get(),Frame,Owner,Floor,Z);
 Planes.Add(Key,P);return P;
}

void FSightWeaveHardCoverage::RasterizeConservative(const FBox2D& B,FIntPoint Size,TArray<float>& Values) const
{
 check(B.bIsValid && Size.X>0 && Size.Y>0);Values.SetNumUninitialized(Size.X*Size.Y);
 const auto Step=B.GetSize()/FVector2D(Size);
 auto Fill=[&](auto&& Self,int X0,int Y0,int X1,int Y1)->void
 {
  bool V=false;
  if(TryUniform(FBox2D(B.Min+Step*FVector2D(X0,Y0),B.Min+Step*FVector2D(X1,Y1)),V))
  {for(int Y=Y0;Y<Y1;++Y)for(int X=X0;X<X1;++X)Values[Y*Size.X+X]=V?1.f:0.f;return;}
  if(X1-X0>4 || Y1-Y0>4)
  {if(X1-X0>=Y1-Y0){const int M=(X0+X1)/2;Self(Self,X0,Y0,M,Y1);Self(Self,M,Y0,X1,Y1);}
   else{const int M=(Y0+Y1)/2;Self(Self,X0,Y0,X1,M);Self(Self,X0,M,X1,Y1);}return;}
  for(int Y=Y0;Y<Y1;++Y)for(int X=X0;X<X1;++X)
  {bool Legal=true;for(auto O:{FVector2D(0),FVector2D(1,0),FVector2D(0,1),FVector2D(1),FVector2D(.5)})
   if(!Contains(B.Min+Step*(FVector2D(X,Y)+O))){Legal=false;break;}
   Values[Y*Size.X+X]=Legal?1.f:0.f;}
 };
 Fill(Fill,0,0,Size.X,Size.Y);
}

void FSightWeaveHardCoverage::RasterizeArea(const FBox2D& B,FIntPoint Size,TArray<float>& Values) const
{
 check(B.bIsValid && Size.X>0 && Size.Y>0);
 Values.Init(0.f,Size.X*Size.Y);
 bool Constant=false;
 if(TryUniform(B,Constant)){if(Constant)for(float& V:Values)V=1.f;return;}
 constexpr int32 Samples=4;
 const int32 Width=Size.X*Samples,Height=Size.Y*Samples;
 const FVector2D Step=B.GetSize()/FVector2D(Width,Height);
 TArray<TArray<int32>> RowEdges;RowEdges.SetNum(Height);
 for(int I=0;I<Boundaries.Num();++I)
 {
  const int A=FMath::Clamp(FMath::FloorToInt((Boundaries[I].Min.Y-B.Min.Y)/Step.Y-.5),0,Height);
  const int End=FMath::Clamp(FMath::CeilToInt((Boundaries[I].Max.Y-B.Min.Y)/Step.Y-.5)+1,0,Height);
  for(int Y=A;Y<End;++Y)RowEdges[Y].Add(I);
 }
 TArray<FIntPoint> Intervals;
 for(int32 Y=0;Y<Height;++Y)
 {
  const double WorldY=B.Min.Y+(Y+.5)*Step.Y;
  if(!PossibleSupport.ContainsByPredicate([&](const FBox2D& Support){return WorldY>=Support.Min.Y && WorldY<=Support.Max.Y;}))continue;
  Intervals.Reset();
  auto Boundary=[&](double Min,double Max)
  {
   if(Max<B.Min.X || Min>B.Max.X)return;
   int32 Begin=FMath::Clamp(FMath::CeilToInt((Min-B.Min.X)/Step.X-.5),0,Width);
   int32 End=FMath::Clamp(FMath::FloorToInt((Max-B.Min.X)/Step.X-.5)+1,0,Width);
   // Recheck converted endpoints at the exact oracle sample coordinates.
   // Zero-length intervals still split constant runs across a boundary that
   // lies between samples, without spending queries on arbitrary guard pixels.
   auto Position=[&](int X){return B.Min.X+(X+.5)*Step.X;};
   while(Begin>0 && Position(Begin-1)>=Min)--Begin;
   while(Begin<Width && Position(Begin)<Min)++Begin;
   while(End>Begin && Position(End-1)>Max)--End;
   while(End<Width && Position(End)<=Max)++End;
   Intervals.Emplace(Begin,FMath::Max(Begin,End));
  };
  for(int32 I:RowEdges[Y])
  {
   const auto& Box=Boundaries[I];if(WorldY<Box.Min.Y || WorldY>Box.Max.Y)continue;
   const auto A=Edges[I].Key,D=Edges[I].Value-A;
   if(D.Y==0){Boundary(Box.Min.X,Box.Max.X);continue;}
   double T0=(WorldY-BoundaryPadding-A.Y)/D.Y,T1=(WorldY+BoundaryPadding-A.Y)/D.Y;
   if(T0>T1)Swap(T0,T1);T0=FMath::Max(0.,T0);T1=FMath::Min(1.,T1);
   if(T0>T1)continue;
   const double X0=A.X+T0*D.X,X1=A.X+T1*D.X;
   Boundary(FMath::Min(X0,X1)-BoundaryPadding,FMath::Max(X0,X1)+BoundaryPadding);
  }
  for(const auto& C:Circles)
  {
   const double DY=WorldY-C.Key.Y;
   if(FMath::Abs(DY)>C.Value)continue;
   const double DX=FMath::Sqrt(FMath::Max(0.,C.Value*C.Value-DY*DY));
   Boundary(C.Key.X-DX,C.Key.X-DX);Boundary(C.Key.X+DX,C.Key.X+DX);
  }
  Intervals.Sort([](FIntPoint A,FIntPoint C){return A.X<C.X;});
  auto Emit=[&](int32 Begin,int32 End,bool Exact)
  {
   if(Begin>=End)return;
   bool Legal=false;
   if(!Exact)Legal=ContainsUncached(FVector2D(B.Min.X+(Begin+.5)*Step.X,WorldY));
   for(int32 X=Begin;X<End;++X)
   {
    if(Exact)Legal=ContainsUncached(FVector2D(B.Min.X+(X+.5)*Step.X,WorldY));
    if(Legal)Values[(Y/Samples)*Size.X+X/Samples]+=1.f/(Samples*Samples);
   }
  };
  int32 Cursor=0;
  for(int32 I=0;I<Intervals.Num();)
  {
   const int32 Begin=Intervals[I].X;int32 End=Intervals[I++].Y;
   while(I<Intervals.Num() && Intervals[I].X<=End){End=FMath::Max(End,Intervals[I].Y);++I;}
   Emit(Cursor,Begin,false);Emit(Begin,End,true);Cursor=End;
  }
  Emit(Cursor,Width,false);
 }
}
