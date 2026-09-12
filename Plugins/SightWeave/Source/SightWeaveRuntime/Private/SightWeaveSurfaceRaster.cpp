#include "SightWeaveWorldSubsystem.h"
#include "SightWeaveSettings.h"
#include "SightWeaveNominalShape.h"
#include "Algo/Sort.h"
#include "Async/ParallelFor.h"

namespace
{
using FRuns=TArray<FIntPoint,TInlineAllocator<16>>; // half-open cell intervals
void Union(FRuns& Runs)
{
 Runs.Sort([](auto A,auto B){return A.X<B.X;});
 int Write=0;for(auto R:Runs)
 {if(R.X>=R.Y)continue;if(Write && R.X<=Runs[Write-1].Y)Runs[Write-1].Y=FMath::Max(Runs[Write-1].Y,R.Y);else Runs[Write++]=R;}
 Runs.SetNum(Write,EAllowShrinking::No);
}
void Subtract(FRuns& Runs,int Low,int High)
{
 if(Low>=High)return;FRuns Next;
 for(auto R:Runs){if(R.Y<=Low || R.X>=High){Next.Add(R);continue;}if(R.X<Low)Next.Add({R.X,Low});if(R.Y>High)Next.Add({High,R.Y});}
 Runs=MoveTemp(Next);
}
bool Contains(const FRuns& Runs,int Cell)
{for(auto R:Runs)if(Cell>=R.X && Cell<R.Y)return true;return false;}
FRuns Intersect(const FRuns& A,const FRuns& B)
{FRuns Out;for(auto X:A)for(auto Y:B)if(FMath::Max(X.X,Y.X)<FMath::Min(X.Y,Y.Y))Out.Add({FMath::Max(X.X,Y.X),FMath::Min(X.Y,Y.Y)});Union(Out);return Out;}

// Projection of a convex shadow intersected with a support column. An outer
// projection excludes possible occlusion from positive certificates; an inner
// projection supplies a real blocked witness at each height it covers.
bool Project(const FSightWeaveSurfaceShadow& Shadow,int H,double Low,double High,double& Min,double& Max)
{
 if(Shadow.Polygon.Num()<3 || Low>Shadow.Bounds.Max[H] || High<Shadow.Bounds.Min[H])return false;
 Min=DBL_MAX;Max=-DBL_MAX;
 auto Add=[&](double V){Min=FMath::Min(Min,V);Max=FMath::Max(Max,V);};
 for(int I=0;I<Shadow.Polygon.Num();++I)
 {
  const auto A=Shadow.Polygon[I],B=Shadow.Polygon[(I+1)%Shadow.Polygon.Num()];
  if(A[H]>=Low && A[H]<=High)Add(A[1-H]);
  if(A[H]==B[H])continue;
  for(double Edge:{Low,High})if(Edge>=FMath::Min(A[H],B[H]) && Edge<=FMath::Max(A[H],B[H]))
   Add(A[1-H]+(B[1-H]-A[1-H])*(Edge-A[H])/(B[H]-A[H]));
 }
 return Min<Max;
}
}

bool USightWeaveWorldSubsystem::BuildSurfaceFaceSpans(FSightWeaveKnowledgeOwnerId Owner,FName Id,ESightWeaveBoxFace Face,FIntPoint Size,
 TArray<FSightWeaveSurfaceCellSpan>& Out,FSightWeaveSurfaceQueryStats* Stats) const
{
 check(IsInGameThread());Out.Reset();const auto Frame=AcquirePublishedSnapshot();
 if(!Owner.IsValid() || !Frame || !Frame->SurfaceScene || Size.GetMin()<=0 || Size.GetMax()>4096)return false;
 const auto* Receiver=Frame->SurfaceScene->Find(Id);if(!Receiver)return false;
 const auto& Box=Receiver->Box;FVector Center,Normal,U,V;
 if(!Box.Resolve(Face,{0,0},Center,Normal))return false;
 const EAxis::Type Axes[]{EAxis::X,EAxis::Y,EAxis::Z};const int FaceAxis=int(Face)/2;
 const FVector UD=Box.Pose.GetUnitAxis(Axes[(FaceAxis+1)%3]),VD=Box.Pose.GetUnitAxis(Axes[(FaceAxis+2)%3]);
 U=UD*Box.HalfExtent[(FaceAxis+1)%3];V=VD*Box.HalfExtent[(FaceAxis+2)%3];
 // Test directions before adding/subtracting a large world origin. Otherwise
 // world-coordinate rounding could hide a small tilt and select a wrong axis.
 const int H=UD.Z==0 && VD.X==0 && VD.Y==0?0:VD.Z==0 && UD.X==0 && UD.Y==0?1:INDEX_NONE;
 if(H==INDEX_NONE)return false;const int Vertical=1-H,N=Size[Vertical];const double ZAxis=(Vertical==0?U:V).Z;
 if(FMath::Abs(ZAxis)<1.e-8)return false;
 const double CellRounding=FMath::Max(1.e-8,64*DBL_EPSILON*(Center.GetAbsMax()+FMath::Abs(ZAxis)+1)*N/(2*FMath::Abs(ZAxis)));
 const auto* Floor=Frame->Floors.FindByPredicate([&](const auto& F){return F.FloorId==Box.Floor;});if(!Floor)return false;
 // Preserve the existing conservative suppression boundary behavior exactly.
 for(const auto& S:Frame->HardSuppressions)if(S.Description.bEnabled && S.Description.FloorId==Box.Floor)return false;
 const auto& T=GetDefault<USightWeaveSettings>()->GeometryTolerances;
 // Workers read ONLY the captured immutable Runtime snapshot and plain values.
 // No UObject, publication, mutable subsystem cache, or Knowledge write crosses
 // threads. ParallelFor joins before the exact game-thread fallback below.
 const int BatchCount=Size[H]>=256?FMath::Min(4,FMath::DivideAndRoundUp(Size[H],128)):1;
 TArray<TArray<FSightWeaveSurfaceCellSpan>> Batches;Batches.SetNum(BatchCount);
 auto BuildColumns=[&](int Batch)
 {
 auto& Spans=Batches[Batch];
 FVector ColumnNormal=Normal;
 struct FColumnBeam {FSightWeaveSurfaceBeamProof Proof;int Column=INDEX_NONE;FRuns Clear,Possible;};
 TArray<FColumnBeam> Beams;
 auto Beam=[&](FVector Eye)->FColumnBeam&
 {if(auto* P=Beams.FindByPredicate([&](const auto& B){return B.Proof.Origin==Eye;}))return *P;auto& P=Beams.AddDefaulted_GetRef();P.Proof=Frame->SurfaceScene->PrepareFaceBeam(Box,Face,Eye);return P;};
 TMap<FIntVector,int32> LastSpans;
 auto Emit=[&](int Column,int A,int B,bool Live,bool Unresolved=false)
 {
  if(A>=B)return;const FIntVector Key(A,B,Live?1:Unresolved?2:0);
  if(auto* Previous=LastSpans.Find(Key))if(Spans[*Previous].Cells.Max[H]==Column)
  {Spans[*Previous].Cells.Max[H]=Column+1;return;}
  FIntRect R;R.Min[H]=Column;R.Max[H]=Column+1;R.Min[Vertical]=A;R.Max[Vertical]=B;
  LastSpans.Add(Key,Spans.Add({R,Live,Unresolved}));
 };
 for(int Column=Size[H]*Batch/BatchCount;Column<Size[H]*(Batch+1)/BatchCount;++Column)
 {
  const double Low=2.*Column/Size[H]-1,High=2.*(Column+1)/Size[H]-1;
  FVector2D A=FVector2D::ZeroVector,B=A;A[H]=Low;B[H]=High;FVector PA,PB;
  Box.Resolve(Face,A,PA,ColumnNormal);Box.Resolve(Face,B,PB,ColumnNormal);
  const bool InFloor=Floor->bEnabled && Floor->bActiveForQueries && FMath::Min(PA.X,PB.X)>=Floor->BoundsMin.X-T.PointOnEdgeEpsilon
   && FMath::Max(PA.X,PB.X)<=Floor->BoundsMax.X+T.PointOnEdgeEpsilon && FMath::Min(PA.Y,PB.Y)>=Floor->BoundsMin.Y-T.PointOnEdgeEpsilon && FMath::Max(PA.Y,PB.Y)<=Floor->BoundsMax.Y+T.PointOnEdgeEpsilon;
  if(!InFloor){Emit(Column,0,N,false);continue;}
  auto Coverage=[&](const auto& E,FRuns& Certain,FRuns& Possible)
  {
   const auto& D=E.Description;const FVector Eye=D.Transform.GetLocation();
   const double Near=[&](){if constexpr(requires{D.NearAwarenessRadius;})return double(D.NearAwarenessRadius);else return 0.;}();
   if(!D.bActive || D.FloorId!=Box.Floor || Near>0 || FVector::DotProduct(ColumnNormal,Eye-Center)<=1.e-4)return;
   const FBox2D XY(FVector2D(FMath::Min(PA.X,PB.X),FMath::Min(PA.Y,PB.Y)),FVector2D(FMath::Max(PA.X,PB.X),FMath::Max(PA.Y,PB.Y)));
   if(D.Shape!=ESightWeaveSourceShape::Radial && (D.HalfAngleDegrees>90 || XY.ComputeSquaredDistanceToPoint(FVector2D(Eye))<=FMath::Square(T.PointOnEdgeEpsilon)))return;
   if(!SightWeave::IsPointInNominalShape(PA,E.PolarOrigin,E.NominalForward,D.Shape,D.Range,E.NominalMinimumCosine,Near,T.PointOnEdgeEpsilon)
    || !SightWeave::IsPointInNominalShape(PB,E.PolarOrigin,E.NominalForward,D.Shape,D.Range,E.NominalMinimumCosine,Near,T.PointOnEdgeEpsilon))return;
   const double ZMin=FMath::Max(D.HeightRange.ZMin,Floor->HeightRange.ZMin)-T.HeightOverlapEpsilon,ZMax=FMath::Min(D.HeightRange.ZMax,Floor->HeightRange.ZMax)+T.HeightOverlapEpsilon;
   if(ZMin>ZMax)return;
   double Min=(ZMin-Center.Z)/ZAxis,Max=(ZMax-Center.Z)/ZAxis;if(Min>Max)Swap(Min,Max);
   Min=FMath::Clamp((Min+1)*.5*N,-2.,double(N)+2);Max=FMath::Clamp((Max+1)*.5*N,-2.,double(N)+2);
   auto Cell=[&](int I){return FMath::Clamp(I,0,N);};
   // Outward/inward rounding bands absorb integer/plane rounding. They are sent
   // back to TrySurfaceRegion, never discarded or promoted by the rasterizer.
   Certain.Add({Cell(FMath::CeilToInt(Min+CellRounding)),Cell(FMath::FloorToInt(Max-CellRounding))});
   Possible.Add({Cell(FMath::FloorToInt(Min-CellRounding)),Cell(FMath::CeilToInt(Max+CellRounding))});Union(Certain);Union(Possible);
   if(Possible.IsEmpty())return;
   auto& P=Beam(Eye);
   // Vision and legal light often share an origin. Their nominal predicates
   // stay separate; their identical geometric column projection is reused.
   if(P.Column!=Column)
   {
   P.Column=Column;P.Clear.Reset();P.Possible={{0,N}};if(P.Proof.bComplete)P.Clear.Add({0,N});
   for(const auto& S:P.Proof.ClearShadows)
   {if(P.Clear.IsEmpty())break;double L,R;if(Project(S,H,Low-1.e-12,High+1.e-12,L,R))Subtract(P.Clear,Cell(FMath::FloorToInt((L+1)*.5*N-1.e-8)),Cell(FMath::CeilToInt((R+1)*.5*N+1.e-8)));}
   for(const auto& S:P.Proof.BlockedShadows)
   // A strictly interior blocked witness is enough to disqualify a FULL support
   // cell. This does not assert that every point in that cell is invisible.
   {if(P.Possible.IsEmpty())break;double L,R;if(Project(S,H,Low+1.e-12,High-1.e-12,L,R) && R-L>1.e-10)Subtract(P.Possible,Cell(FMath::FloorToInt((L+1)*.5*N+1.e-8)),Cell(FMath::CeilToInt((R+1)*.5*N-1.e-8)));}
   }
   Certain=Intersect(Certain,P.Clear);Possible=Intersect(Possible,P.Possible);
  };
  FRuns Certain,Possible;
  for(const auto& E:Frame->VisionSources)if(E.Description.KnowledgeOwnerId==Owner)
  {
   FRuns VC,VP;Coverage(E,VC,VP);if(VP.IsEmpty())continue;
   if(E.Description.IlluminationPolicy==ESightWeaveIlluminationPolicy::BypassLegalIllumination){Certain.Append(VC);Possible.Append(VP);continue;}
   for(int I:E.CompatibleIlluminationSourceIndices)if(Frame->IlluminationSources.IsValidIndex(I))
   {FRuns LC,LP;Coverage(Frame->IlluminationSources[I],LC,LP);Certain.Append(Intersect(VC,LC));Possible.Append(Intersect(VP,LP));}
  }
  Union(Certain);Union(Possible);
  TArray<int,TInlineAllocator<64>> Cuts{0,N};for(auto R:Certain){Cuts.Add(R.X);Cuts.Add(R.Y);}for(auto R:Possible){Cuts.Add(R.X);Cuts.Add(R.Y);}Cuts.Sort();
  for(int I=1;I<Cuts.Num();++I)
  {
   const int Start=Cuts[I-1],End=Cuts[I];if(Start==End)continue;
   if(Contains(Certain,Start)){Emit(Column,Start,End,true);continue;}
   if(!Contains(Possible,Start)){Emit(Column,Start,End,false);continue;}
   Emit(Column,Start,End,false,true);
  }
 }
 };
 ParallelFor(BatchCount,BuildColumns,EParallelForFlags::Unbalanced);
 for(auto& Batch:Batches)Out.Append(MoveTemp(Batch));
 // Coalesce equal contact bands BEFORE invoking the exact fallback. A long
 // wall's horizontal edge should be one region, not thousands of identical
 // per-column preparations. Only genuinely varying boundaries subdivide.
 auto Prepared=MoveTemp(Out);Out.Reset();
 auto Resolve=[&](auto&& Self,const FIntRect& Cells)->void
 {
  const FBox2D UV(FVector2D(double(Cells.Min.X)/Size.X,double(Cells.Min.Y)/Size.Y)*2-FVector2D(1),
   FVector2D(double(Cells.Max.X)/Size.X,double(Cells.Max.Y)/Size.Y)*2-FVector2D(1));
  bool Live=false;if(TrySurfaceRegion(Owner,Id,Face,UV,Live,Stats)){Out.Add({Cells,Live,false});return;}
  if((Cells.Width()==1 || Cells.Height()==1) && RejectSurfaceCellStrip(Owner,Id,Face,UV,Cells.Width()==1?0:1))
  {Out.Add({Cells,false,true});return;}
  if(Cells.Width()==1 && Cells.Height()==1){Out.Add({Cells,false,true});return;}
  FIntRect A=Cells,B=Cells;const int Axis=Cells.Width()>=Cells.Height()?0:1;const int Mid=(Cells.Min[Axis]+Cells.Max[Axis])/2;A.Max[Axis]=Mid;B.Min[Axis]=Mid;Self(Self,A);Self(Self,B);
 };
 for(const auto& Span:Prepared)if(Span.bUnresolved)Resolve(Resolve,Span.Cells);else Out.Add(Span);
 return true;
}
