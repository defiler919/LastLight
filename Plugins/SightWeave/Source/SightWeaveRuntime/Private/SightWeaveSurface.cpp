#include "SightWeaveSurface.h"
#include "SightWeaveWorldSubsystem.h"
#include "SightWeaveSettings.h"
#include "SightWeaveNominalShape.h"
#include "Algo/Sort.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeExit.h"

namespace
{
constexpr double Contact=1.e-4; // cm; endpoint contact, not a coverage expansion
TAutoConsoleVariable<int32> CVarSurfacePreparedProofs(TEXT("SightWeave.Surface.PreparedProofs"),1,TEXT("Reuse per-snapshot convex face shadows. 0 runs the reference beam proof for differential diagnostics."));
TAutoConsoleVariable<int32> CVarSurfaceProofProfile(TEXT("SightWeave.Surface.Profile"),0,TEXT("Log region proof phase timings per published snapshot (diagnostic only)."));
struct FProofProfile {uint64 Total=0,Blocked=0,Clear=0,Corner=0,Prepare=0,Calls=0,ClearHits=0,BlockedHits=0;} ProofProfile;
template<typename F> auto ProfileProof(F&& Function,uint64& Cycles)
{
 if(!CVarSurfaceProofProfile.GetValueOnGameThread())return Function();
 const uint64 Start=FPlatformTime::Cycles64();auto Result=Function();Cycles+=FPlatformTime::Cycles64()-Start;return Result;
}
bool Interval(FVector A,FVector B,const FBox& Box,double& Lo,double& Hi)
{
 const FVector D=B-A; Lo=0; Hi=1;
 for(int I=0;I<3;++I)
 {
  if(FMath::Abs(D[I])<1.e-12) {if(A[I]<Box.Min[I] || A[I]>Box.Max[I])return false;continue;}
  double L=(Box.Min[I]-A[I])/D[I],H=(Box.Max[I]-A[I])/D[I];if(L>H)Swap(L,H);
  Lo=FMath::Max(Lo,L);Hi=FMath::Min(Hi,H);if(Lo>Hi)return false;
 }
 return true;
}
bool WallBlocks(const FSightWeaveSegment2D& W,FVector A,FVector B)
{
 const FVector2D D(B-A),E=W.B-W.A,O=W.A-FVector2D(A);
 const double Cross=D.X*E.Y-D.Y*E.X;
 const double Epsilon=Contact/FMath::Max(1.,(B-A).Length());
 if(FMath::Abs(Cross)>1.e-10)
 {
  const double T=(O.X*E.Y-O.Y*E.X)/Cross,U=(O.X*D.Y-O.Y*D.X)/Cross;
  const double Z=FMath::Lerp(A.Z,B.Z,T);
  return T>Epsilon && T<1-Epsilon && U>=0 && U<=1 && Z>=W.HeightRange.ZMin && Z<=W.HeightRange.ZMax;
 }
 // Coplanar rays: intersect the finite vertical wall rectangle, including
 // vertical rays whose XY does not change. No infinite-height wall fallback.
 if(FMath::Abs(O.X*E.Y-O.Y*E.X)>Contact*E.Length())return false;
 double Lo,Hi;
 const FBox Bounds(FVector(FMath::Min(W.A.X,W.B.X),FMath::Min(W.A.Y,W.B.Y),W.HeightRange.ZMin),
  FVector(FMath::Max(W.A.X,W.B.X),FMath::Max(W.A.Y,W.B.Y),W.HeightRange.ZMax));
 return Interval(A,B,Bounds,Lo,Hi) && Hi>Epsilon && Lo<1-Epsilon && Hi-Lo>Epsilon;
}
}

bool FSightWeaveSurfaceBox::IsValid() const
{
 return !Id.IsNone() && Floor.IsValid() && !Pose.ContainsNaN() && Pose.GetRotation().IsNormalized()
  && Pose.GetScale3D().Equals(FVector::OneVector,1.e-8) && !HalfExtent.ContainsNaN()
  && HalfExtent.GetMin()>Contact && HalfExtent.GetMax()<=1.e6 && Pose.GetLocation().GetAbsMax()<=1.e9;
}
bool FSightWeaveSurfaceBox::Resolve(ESightWeaveBoxFace Face,FVector2D UV,FVector& P,FVector& N) const
{
 if(uint8(Face)>5 || UV.ContainsNaN() || UV.GetAbsMax()>1)return false;
 const int Axis=uint8(Face)/2;const double Sign=(uint8(Face)%2)==0?-1:1;
 FVector L=FVector::ZeroVector;N=FVector::ZeroVector;N[Axis]=Sign;L[Axis]=HalfExtent[Axis]*Sign;
 L[(Axis+1)%3]=UV.X*HalfExtent[(Axis+1)%3];L[(Axis+2)%3]=UV.Y*HalfExtent[(Axis+2)%3];
 P=Pose.TransformPosition(L);N=Pose.TransformVectorNoScale(N);return true;
}
FSightWeaveSurfaceScene::FSightWeaveSurfaceScene(TArray<FReceiver> Receivers,TConstArrayView<FSightWeaveSegment2D> Walls):Boxes(MoveTemp(Receivers))
{
 for(int I=0;I<Boxes.Num();++I)
 {Ids.Add(Boxes[I].Box.Id,I);FPrimitive P;P.BoxIndex=I;P.Bounds=FBox(-Boxes[I].Box.HalfExtent,Boxes[I].Box.HalfExtent).TransformBy(Boxes[I].Box.Pose);Primitives.Add(P);}
 for(const auto& W:Walls)
 {FPrimitive P;P.Wall=W;P.Bounds=FBox(FVector(FMath::Min(W.A.X,W.B.X),FMath::Min(W.A.Y,W.B.Y),W.HeightRange.ZMin),FVector(FMath::Max(W.A.X,W.B.X),FMath::Max(W.A.Y,W.B.Y),W.HeightRange.ZMax));Primitives.Add(P);}
 TArray<int32> Order;for(int I=0;I<Primitives.Num();++I)Order.Add(I);if(!Order.IsEmpty())Build(Order,0,Order.Num());
}
int32 FSightWeaveSurfaceScene::Build(TArray<int32>& Order,int32 Begin,int32 End)
{
 const int32 Index=Nodes.AddDefaulted();FBox B(ForceInit);for(int I=Begin;I<End;++I)B+=Primitives[Order[I]].Bounds;Nodes[Index].Bounds=B;
 if(End-Begin==1){Nodes[Index].Primitive=Order[Begin];return Index;}
 const FVector Ext=B.GetSize();const int Axis=Ext.X>=Ext.Y && Ext.X>=Ext.Z?0:Ext.Y>=Ext.Z?1:2;
 Algo::Sort(MakeArrayView(Order.GetData()+Begin,End-Begin),[&](int A,int C){return Primitives[A].Bounds.GetCenter()[Axis]<Primitives[C].Bounds.GetCenter()[Axis];});
 const int Mid=(Begin+End)/2;const int Left=Build(Order,Begin,Mid),Right=Build(Order,Mid,End);Nodes[Index].Left=Left;Nodes[Index].Right=Right;return Index;
}
const FSightWeaveSurfaceScene::FReceiver* FSightWeaveSurfaceScene::Find(FName Id) const
{const auto* I=Ids.Find(Id);return I?&Boxes[*I]:nullptr;}
bool FSightWeaveSurfaceScene::Unoccluded(FSightWeaveFloorId Floor,FVector A,FVector B,FSightWeaveSurfaceQueryStats* Stats) const
{
 if(Nodes.IsEmpty())return true;
 TArray<int32,TInlineAllocator<64>> Stack;Stack.Add(0);
 while(!Stack.IsEmpty())
 {
  const auto& Node=Nodes[Stack.Pop(EAllowShrinking::No)];if(Stats)++Stats->NodeVisits;
  double Lo,Hi;if(!Interval(A,B,Node.Bounds,Lo,Hi))continue;
  if(Node.Primitive==INDEX_NONE){Stack.Add(Node.Left);Stack.Add(Node.Right);continue;}
  const auto& P=Primitives[Node.Primitive];
  if(P.BoxIndex!=INDEX_NONE)
  {
   const auto& Box=Boxes[P.BoxIndex].Box;if(Box.Floor!=Floor)continue;if(Stats)++Stats->PrimitiveTests;
   const FVector LocalA=Box.Pose.InverseTransformPosition(A),LocalB=Box.Pose.InverseTransformPosition(B);
   const FVector E=Box.HalfExtent-FVector(Contact);
   if(Interval(LocalA,LocalB,FBox(-E,E),Lo,Hi) && Lo<Hi && Hi>0 && Lo<1)return false;
  }
  else if(P.Wall.FloorId==Floor){if(Stats)++Stats->PrimitiveTests;if(WallBlocks(P.Wall,A,B))return false;}
 }
 return true;
}

bool USightWeaveWorldSubsystem::RegisterSurfaceBox(const FSightWeaveSurfaceBox& Box,UObject* Owner)
{
 if(!bSightWeaveInitialized || !Box.IsValid() || !Floors.Contains(Box.Floor) || SurfaceBoxes.Contains(Box.Id))return false;
 SurfaceBoxes.Add(Box.Id,{Box,++SurfaceSerial});if(Owner)SurfaceOwners.Add(Box.Id,Owner);
 bSurfaceSceneDirty=true;AdvanceRevision();PublishSnapshot();return true;
}

bool FSightWeaveSurfaceScene::BeamClear(FSightWeaveFloorId Floor,FName Receiver,FVector Origin,TConstArrayView<FVector> Corners) const
{
 FBox Beam(ForceInit);Beam+=Origin;for(auto P:Corners)Beam+=P;
 FVector PlaneNormal=Corners.Num()>=3?FVector::CrossProduct(Corners[1]-Corners[0],Corners[2]-Corners[0]).GetSafeNormal():FVector::ZeroVector;
 if(!Corners.IsEmpty() && FVector::DotProduct(PlaneNormal,Origin-Corners[0])<0)PlaneNormal=-PlaneNormal;
 // SAT over a five-vertex beam and a convex proxy. A separating plane proves
 // no ray through the receiver rectangle can hit that proxy. Bounds alone are
 // only the BVH broad phase, not a reason to reject a visible surface.
 auto Disjoint=[&](TConstArrayView<FVector> Vertices,TConstArrayView<FVector> Edges,TConstArrayView<FVector> Normals)
 {
  TArray<FVector,TInlineAllocator<8>> BeamEdges;
  for(int I=0;I<Corners.Num();++I){BeamEdges.Add(Corners[I]-Origin);BeamEdges.Add(Corners[(I+1)%Corners.Num()]-Corners[I]);}
  auto Separated=[&](FVector Axis)
  {
   if(!Axis.Normalize())return false;
   double MinA=FVector::DotProduct(Axis,Origin),MaxA=MinA,MinB=DBL_MAX,MaxB=-DBL_MAX;
   for(auto P:Corners){const double D=FVector::DotProduct(Axis,P);MinA=FMath::Min(MinA,D);MaxA=FMath::Max(MaxA,D);}
   for(auto P:Vertices){const double D=FVector::DotProduct(Axis,P);MinB=FMath::Min(MinB,D);MaxB=FMath::Max(MaxB,D);}
   return MaxA<MinB-Contact || MaxB<MinA-Contact;
  };
  for(auto N:Normals)if(Separated(N))return true;
  if(Corners.Num()>=3 && Separated(FVector::CrossProduct(Corners[1]-Corners[0],Corners[2]-Corners[0])))return true;
  for(int I=0;I<Corners.Num();++I)if(Separated(FVector::CrossProduct(Corners[I]-Origin,Corners[(I+1)%Corners.Num()]-Origin)))return true;
  for(auto A:BeamEdges)for(auto B:Edges)if(Separated(FVector::CrossProduct(A,B)))return true;
  return false;
 };
 TArray<int32,TInlineAllocator<64>> Stack;if(!Nodes.IsEmpty())Stack.Add(0);
 while(!Stack.IsEmpty())
 {
  const auto& N=Nodes[Stack.Pop(EAllowShrinking::No)];if(!Beam.Intersect(N.Bounds))continue;
  if(N.Primitive==INDEX_NONE){Stack.Add(N.Left);Stack.Add(N.Right);continue;}
  const auto& P=Primitives[N.Primitive];
  if(!PlaneNormal.IsNearlyZero())
  {
   const double Center=FVector::DotProduct(PlaneNormal,P.Bounds.GetCenter()),Radius=FVector::DotProduct(PlaneNormal.GetAbs(),P.Bounds.GetExtent());
   // Contact at a terminal plane cannot intersect the open observation beam.
   if(Center+Radius<=FVector::DotProduct(PlaneNormal,Corners[0]) || Center-Radius>=FVector::DotProduct(PlaneNormal,Origin))continue;
  }
  if(P.BoxIndex!=INDEX_NONE)
  {
   const auto& B=Boxes[P.BoxIndex].Box;if(B.Floor!=Floor || B.Id==Receiver)continue;
   TArray<FVector,TInlineAllocator<8>> Vertices;for(int I=0;I<8;++I)Vertices.Add(B.Pose.TransformPosition(B.HalfExtent*FVector(I&1?1:-1,I&2?1:-1,I&4?1:-1)));
   const FVector Axes[]{B.Pose.GetUnitAxis(EAxis::X),B.Pose.GetUnitAxis(EAxis::Y),B.Pose.GetUnitAxis(EAxis::Z)};
   if(!Disjoint(Vertices,Axes,Axes))return false;
  }
  else if(P.Wall.FloorId==Floor)
  {
   const auto& W=P.Wall;const FVector Vertices[]{FVector(W.A,W.HeightRange.ZMin),FVector(W.B,W.HeightRange.ZMin),FVector(W.B,W.HeightRange.ZMax),FVector(W.A,W.HeightRange.ZMax)};
   const FVector Edges[]{FVector(W.B-W.A,0),FVector::UpVector};const FVector Normals[]{FVector::CrossProduct(Edges[0],Edges[1])};
   if(!Disjoint(Vertices,Edges,Normals))return false;
  }
 }
 return true;
}
bool FSightWeaveSurfaceScene::BeamBlocked(FSightWeaveFloorId Floor,FVector Origin,FVector Normal,TConstArrayView<FVector> Corners) const
{
 if(Corners.IsEmpty())return false;
 const double Plane=FVector::DotProduct(Normal,Corners[0]),Eye=FVector::DotProduct(Normal,Origin);
 if(Eye<=Plane+Contact)return false;
 FBox Beam(ForceInit);Beam+=Origin;for(auto P:Corners)Beam+=P;
 TArray<int32,TInlineAllocator<64>> Stack;if(!Nodes.IsEmpty())Stack.Add(0);
 while(!Stack.IsEmpty())
 {
  const auto& N=Nodes[Stack.Pop(EAllowShrinking::No)];if(!Beam.Intersect(N.Bounds))continue;
  if(N.Primitive==INDEX_NONE){Stack.Add(N.Left);Stack.Add(N.Right);continue;}
  const auto& P=Primitives[N.Primitive];
  if((P.BoxIndex!=INDEX_NONE?Boxes[P.BoxIndex].Box.Floor:P.Wall.FloorId)!=Floor)continue;
  // Clip the convex blocker conceptually to the open eye/target slab. Its
  // conic hull from the eye, intersected with the receiver plane, is convex.
  // All four blocked vertices therefore prove the whole receiver rectangle,
  // even when the furniture touches the floor or extends above the eye.
  // Walls additionally need a uniform endpoint-contact margin; unlike box
  // slab tests their exact predicate shortens each ray by a world distance.
  if(P.BoxIndex==INDEX_NONE)
  {
   FVector NWall=FVector::CrossProduct(FVector(P.Wall.B-P.Wall.A,0),FVector::UpVector).GetSafeNormal();
   const double From=FVector::DotProduct(NWall,Origin-FVector(P.Wall.A,0));if(FMath::Abs(From)<=Contact)continue;
   bool Opposite=true;for(auto C:Corners){const double To=FVector::DotProduct(NWall,C-FVector(P.Wall.A,0));Opposite&=From>0?To<-Contact:To>Contact;}if(!Opposite)continue;
  }
  bool All=true;
  for(auto C:Corners)
  {
   if(P.BoxIndex==INDEX_NONE){if(!WallBlocks(P.Wall,Origin,C)){All=false;break;}}
   else
   {
    const auto& B=Boxes[P.BoxIndex].Box;double Lo,Hi;const FVector E=B.HalfExtent-FVector(Contact);
    if(!Interval(B.Pose.InverseTransformPosition(Origin),B.Pose.InverseTransformPosition(C),FBox(-E,E),Lo,Hi) || Lo>=Hi || Hi<=0 || Lo>=1){All=false;break;}
   }
  }
  if(All)return true;
 }
 return false;
}
bool USightWeaveWorldSubsystem::TrySurfaceRegion(FSightWeaveKnowledgeOwnerId Owner,FName Id,ESightWeaveBoxFace Face,const FBox2D& UV,bool& Value,FSightWeaveSurfaceQueryStats* Stats) const
{
 check(IsInGameThread());
 const auto Frame=AcquirePublishedSnapshot();if(!Owner.IsValid() || !Frame || !Frame->SurfaceScene || !UV.bIsValid)return false;
 const bool UsePrepared=CVarSurfacePreparedProofs.GetValueOnGameThread()!=0;
 if(SurfaceRegionFrame!=Frame || SurfaceRegionOwner!=Owner || bSurfaceRegionPrepared!=UsePrepared)
 {
  if(CVarSurfaceProofProfile.GetValueOnGameThread() && ProofProfile.Calls)
   UE_LOG(LogTemp,Display,TEXT("SURFACE_PROOF_PHASE calls=%llu total_us=%.3f blocked_us=%.3f clear_us=%.3f corner_us=%.3f prepare_us=%.3f clear_hits=%llu blocked_hits=%llu"),ProofProfile.Calls,FPlatformTime::ToSeconds64(ProofProfile.Total)*1.e6,FPlatformTime::ToSeconds64(ProofProfile.Blocked)*1.e6,FPlatformTime::ToSeconds64(ProofProfile.Clear)*1.e6,FPlatformTime::ToSeconds64(ProofProfile.Corner)*1.e6,FPlatformTime::ToSeconds64(ProofProfile.Prepare)*1.e6,ProofProfile.ClearHits,ProofProfile.BlockedHits);
  ProofProfile={};SurfaceRegionCorners.Reset();SurfaceBeamProofs.Reset();SurfaceRegionFrame=Frame;SurfaceRegionOwner=Owner;bSurfaceRegionPrepared=UsePrepared;
 }
 const bool Profiling=CVarSurfaceProofProfile.GetValueOnGameThread()!=0;const uint64 ProfileStart=Profiling?FPlatformTime::Cycles64():0;
 ON_SCOPE_EXIT {if(Profiling){ProofProfile.Total+=FPlatformTime::Cycles64()-ProfileStart;++ProofProfile.Calls;}};
 if(SurfaceProofReceiver!=Id || SurfaceProofFace!=Face)
 {SurfaceBeamProofs.Reset();if(UsePrepared)SurfaceRegionCorners.Reset();SurfaceProofReceiver=Id;SurfaceProofFace=Face;}
 const auto* R=Frame->SurfaceScene->Find(Id);if(!R)return false;
 const auto* Floor=Frame->Floors.FindByPredicate([&](const auto& F){return F.FloorId==R->Box.Floor;});if(!Floor)return false;
 const FVector2D Points[]{UV.Min,{UV.Max.X,UV.Min.Y},UV.Max,{UV.Min.X,UV.Max.Y}};TArray<FVector,TInlineAllocator<4>> World;FVector Normal;
 for(auto P:Points){FVector Q;if(!R->Box.Resolve(Face,P,Q,Normal))return false;World.Add(Q);}
 bool AnyFacing=false;for(const auto& V:Frame->VisionSources)
  AnyFacing|=V.Description.bActive && V.Description.KnowledgeOwnerId==Owner && V.Description.FloorId==Floor->FloorId
   && FVector::DotProduct(Normal,V.Description.Transform.GetLocation()-World[0])>1.e-4;
 if(!AnyFacing){Value=false;return true;}
 FBox2D XY(ForceInit);for(auto P:World)XY+=FVector2D(P);
 for(const auto& S:Frame->HardSuppressions)if(S.Description.bEnabled && S.Description.FloorId==Floor->FloorId)
 {
  const auto& D=S.Description;bool All=true;for(auto P:World)All&=(FVector2D(P)-D.Center).SizeSquared()<=FMath::Square(D.Radius) && P.Z>=D.HeightRange.ZMin && P.Z<=D.HeightRange.ZMax;
  if(All){Value=false;return true;}
  if(XY.Intersect(FBox2D(D.Center-FVector2D(D.Radius),D.Center+FVector2D(D.Radius))))return false;
 }
 const auto& T=GetDefault<USightWeaveSettings>()->GeometryTolerances;
 auto Prepared=[&](FVector Eye)->const FSightWeaveSurfaceBeamProof&
 {
  if(const auto* Found=SurfaceBeamProofs.FindByPredicate([&](const auto& P){return P.Origin==Eye;}))return *Found;
  auto& New=SurfaceBeamProofs.Add_GetRef(ProfileProof([&](){return Frame->SurfaceScene->PrepareFaceBeam(R->Box,Face,Eye);},ProofProfile.Prepare));
  return New;
 };
 auto ClearBeam=[&](FVector Eye)
 {
  if(UsePrepared)
  {
   const auto& P=Prepared(Eye);if(P.IsClear(UV))return true;
   // An inner shadow intersects at least one ray, so an all-ray clear proof
   // is impossible. Keep the original SAT only for the contact/degenerate gap.
   if(P.BlockedShadows.ContainsByPredicate([&](const auto& S){return S.Intersects(UV);}))return false;
  }
  return ProfileProof([&](){return Frame->SurfaceScene->BeamClear(Floor->FloorId,Id,Eye,World);},ProofProfile.Clear);
 };
 // These are rejection bounds of the same nominal predicates. They can only
 // reject a region wholly outside; ambiguous geometry still uses exact Hard.
 auto Possible=[&](const auto& E)
 {
  const auto& D=E.Description;const FVector Eye=D.Transform.GetLocation();
  if(!D.bActive || D.FloorId!=Floor->FloorId || FVector::DotProduct(Normal,Eye-World[0])<=1.e-4)return false;
  FBox Bounds(ForceInit);for(auto P:World)Bounds+=P;
  if(Bounds.Max.Z<D.HeightRange.ZMin-T.HeightOverlapEpsilon || Bounds.Min.Z>D.HeightRange.ZMax+T.HeightOverlapEpsilon)return false;
  const double Near=[&](){if constexpr(requires{D.NearAwarenessRadius;})return double(D.NearAwarenessRadius);else return 0.;}();
  if(D.Shape!=ESightWeaveSourceShape::Radial && D.HalfAngleDegrees<=90 && XY.ComputeSquaredDistanceToPoint(FVector2D(Eye))>FMath::Square(Near+T.PointOnEdgeEpsilon))
  {
   const double C=E.NominalMinimumCosine,S=FMath::Sqrt(FMath::Max(0.,1-C*C));const FVector2D F=E.NominalForward,Side(-F.Y,F.X);
   for(auto N:{F*S+Side*C,F*S-Side*C})
   {double Maximum=-DBL_MAX;for(auto P:World)Maximum=FMath::Max(Maximum,FVector2D::DotProduct(N,FVector2D(P)-E.PolarOrigin));if(Maximum<0)return false;}
  }
  if(XY.ComputeSquaredDistanceToPoint(FVector2D(Eye))>FMath::Square(D.Range+T.PointOnEdgeEpsilon))return false;
  if(UsePrepared)
  {
   const auto& Proof=Prepared(Eye);
   if(Proof.IsClear(UV)){if(Profiling)++ProofProfile.ClearHits;return true;}
   if(Proof.IsBlocked(UV)){if(Profiling)++ProofProfile.BlockedHits;return false;}
   // BeamBlocked requires ONE convex blocker to contain the full rectangle.
   // If no outer shadow contains it, the exact denial test cannot succeed.
   if(Proof.bComplete && !Proof.ClearShadows.ContainsByPredicate([&](const auto& S){return S.Contains(UV);}))return true;
  }
  return !ProfileProof([&](){return Frame->SurfaceScene->BeamBlocked(Floor->FloorId,Eye,Normal,World);},ProofProfile.Blocked);
 };
 bool PossiblePair=false;
 for(const auto& V:Frame->VisionSources)if(V.Description.KnowledgeOwnerId==Owner && Possible(V))
 {
  if(V.Description.IlluminationPolicy==ESightWeaveIlluminationPolicy::BypassLegalIllumination){PossiblePair=true;break;}
  for(int I:V.CompatibleIlluminationSourceIndices)if(Frame->IlluminationSources.IsValidIndex(I) && Possible(Frame->IlluminationSources[I])){PossiblePair=true;break;}
 }
 if(!PossiblePair){Value=false;return true;}
 auto Convex=[&](const auto& D)
 {
  if(D.Shape==ESightWeaveSourceShape::Radial)return true;
  if(D.HalfAngleDegrees>90)return false;
  // The tiny origin-awareness disc is a union with the cone, not convex.
  return XY.ComputeSquaredDistanceToPoint(FVector2D(D.Transform.GetLocation()))>FMath::Square(T.PointOnEdgeEpsilon);
 };
 for(const auto& V:Frame->VisionSources)
 {
  if(!Convex(V.Description) || V.Description.NearAwarenessRadius>0)continue;
  if(!V.Description.bActive || V.Description.KnowledgeOwnerId!=Owner || V.Description.FloorId!=Floor->FloorId)continue;
  // A different source may have made PossiblePair true. Do not send distant
  // cells through the short-range body source's corner cache/evaluator.
  if(UsePrepared && (FVector::DotProduct(Normal,V.Description.Transform.GetLocation()-World[0])<=Contact
   || XY.ComputeSquaredDistanceToPoint(FVector2D(V.Description.Transform.GetLocation()))>FMath::Square(V.Description.Range+T.PointOnEdgeEpsilon)))continue;
  TArray<FSightWeaveIlluminationSourceHandle,TInlineAllocator<4>> Common;bool All=true;
  FSightWeaveSurfaceContext Context{Frame->SurfaceScene.Get(),Normal,Stats};
  FVector2D CornerUV;
  auto CornerOcclusion=[&](FVector Eye,FVector Point)
  {
   const auto& P=Prepared(Eye);const FBox2D At(CornerUV,CornerUV);
   if(P.IsClear(At))return true;if(P.IsBlocked(At))return false;
   return Frame->SurfaceScene->Unoccluded(Floor->FloorId,Eye,Point,Stats);
  };
  TFunctionRef<bool(FVector,FVector)> OcclusionRef(CornerOcclusion);
  if(UsePrepared)Context.PreparedOcclusion=&OcclusionRef;
  for(int I=0;I<World.Num();++I)
  {
   CornerUV=Points[I];
   const FSightWeaveSurfaceRegionCorner Key{{Id,Face,Points[I]},V.Handle};
   const auto* Evidence=SurfaceRegionCorners.Find(Key);
   if(Evidence){if(Stats)++Stats->CacheHits;}
   else
   {
    FSightWeaveSurfaceCornerEvidence Q;
    if(Stats)++Stats->ExactSamples;
    ProfileProof([&]()
    {
     if(!UsePrepared)
     {
      FSightWeaveVisibilityQueryResult Full;
      QueryEffectiveLiveValidated(Owner,Floor->FloorId,World[I],&V.Handle,false,*Frame,*Floor,T,Full,nullptr,0,0,false,false,&Context);
      Q.bVisible=Full.bVisible;Q.ContributingIlluminationSources.Append(Full.ContributingIlluminationSources);
     }
     else
     {
      // Compact source-restricted evidence uses the SAME surface predicate as
      // Hard EffectiveLive. Owner/source policy and floor are fixed here;
      // hard suppression was already conservatively excluded for the region.
      const FVector P=World[I];
      if(Floor->bEnabled && Floor->bActiveForQueries && P.X>=Floor->BoundsMin.X-T.PointOnEdgeEpsilon && P.X<=Floor->BoundsMax.X+T.PointOnEdgeEpsilon
       && P.Y>=Floor->BoundsMin.Y-T.PointOnEdgeEpsilon && P.Y<=Floor->BoundsMax.Y+T.PointOnEdgeEpsilon
       && P.Z>=Floor->HeightRange.ZMin-T.HeightOverlapEpsilon && P.Z<=Floor->HeightRange.ZMax+T.HeightOverlapEpsilon
       && SightWeave::IsSurfaceSourcePointContained(V,Floor->FloorId,P,T,Context))
      {
       Q.bVisible=V.Description.IlluminationPolicy==ESightWeaveIlluminationPolicy::BypassLegalIllumination;
       if(!Q.bVisible)for(int L:V.CompatibleIlluminationSourceIndices)
        if(Frame->IlluminationSources.IsValidIndex(L) && SightWeave::IsSurfaceSourcePointContained(Frame->IlluminationSources[L],Floor->FloorId,P,T,Context))
        {Q.bVisible=true;Q.ContributingIlluminationSources.Add(Frame->IlluminationSources[L].Handle);}
      }
     }
     return true;
    },ProofProfile.Corner);
    if(SurfaceRegionCorners.Num()>=4096)SurfaceRegionCorners.Reset();Evidence=&SurfaceRegionCorners.Add(Key,MoveTemp(Q));
   }
   if(!Evidence->bVisible){All=false;break;}
   if(I==0)Common.Append(Evidence->ContributingIlluminationSources);else Common.RemoveAll([&](auto H){return !Evidence->ContributingIlluminationSources.Contains(H);});
  }
  if(!All || !ClearBeam(V.Description.Transform.GetLocation()))continue;
  if(V.Description.IlluminationPolicy==ESightWeaveIlluminationPolicy::BypassLegalIllumination){Value=true;return true;}
  for(const auto& L:Frame->IlluminationSources)if(Common.Contains(L.Handle) && Convex(L.Description)
   && ClearBeam(L.Description.Transform.GetLocation())){Value=true;return true;}
 }
 return false;
}
bool USightWeaveWorldSubsystem::RejectSurfaceCellStrip(FSightWeaveKnowledgeOwnerId Owner,FName Id,ESightWeaveBoxFace Face,const FBox2D& UV,int32 Axis) const
{
 check(IsInGameThread());const auto Frame=AcquirePublishedSnapshot();
 if(!CVarSurfacePreparedProofs.GetValueOnGameThread() || !Frame || Frame!=SurfaceRegionFrame || Owner!=SurfaceRegionOwner
  || Id!=SurfaceProofReceiver || Face!=SurfaceProofFace || !UV.bIsValid || (Axis!=0 && Axis!=1))return false;
 const auto* Receiver=Frame->SurfaceScene->Find(Id);if(!Receiver)return false;
 FVector Point,Normal;if(!Receiver->Box.Resolve(Face,UV.Min,Point,Normal) || UV.Max.GetAbsMax()>1)return false;
 FVector Along,Across;FVector2D LongUV=UV.Min,ThinUV=UV.Min;LongUV[1-Axis]=UV.Max[1-Axis];ThinUV[Axis]=UV.Max[Axis];
 Receiver->Box.Resolve(Face,LongUV,Along,Normal);Receiver->Box.Resolve(Face,ThinUV,Across,Normal);
 const bool Vertical=FVector2D(Along)==FVector2D(Point);
 const auto& T=GetDefault<USightWeaveSettings>()->GeometryTolerances;
 auto Blocked=[&](FVector Eye)
 {
  auto* Proof=SurfaceBeamProofs.FindByPredicate([&](const auto& P){return P.Origin==Eye;});
  if(!Proof)Proof=&SurfaceBeamProofs.Add_GetRef(Frame->SurfaceScene->PrepareFaceBeam(Receiver->Box,Face,Eye));
  return Proof->BlockedShadows.ContainsByPredicate([&](const auto& S){return S.CrossesCellStrip(UV,Axis);});
 };
 auto CannotCover=[&](const auto& E)
 {
  const auto& D=E.Description;
  const double Near=[&](){if constexpr(requires{D.NearAwarenessRadius;})return double(D.NearAwarenessRadius);else return 0.;}();
  // Every cell of a vertical strip contains both XY endpoints. An endpoint
  // outside the shared nominal predicate is a witness for every cell, at any Z.
  if(Vertical && (!SightWeave::IsPointInNominalShape(Point,E.PolarOrigin,E.NominalForward,D.Shape,D.Range,E.NominalMinimumCosine,Near,T.PointOnEdgeEpsilon)
   || !SightWeave::IsPointInNominalShape(Across,E.PolarOrigin,E.NominalForward,D.Shape,D.Range,E.NominalMinimumCosine,Near,T.PointOnEdgeEpsilon)))return true;
  return Blocked(D.Transform.GetLocation());
 };
 for(const auto& V:Frame->VisionSources)
 {
  const auto& D=V.Description;
  if(!D.bActive || D.KnowledgeOwnerId!=Owner || D.FloorId!=Receiver->Box.Floor || FVector::DotProduct(Normal,D.Transform.GetLocation()-Point)<=1.e-4)continue;
  if(CannotCover(V))continue;
  if(D.IlluminationPolicy==ESightWeaveIlluminationPolicy::BypassLegalIllumination)return false;
  for(int I:V.CompatibleIlluminationSourceIndices)if(Frame->IlluminationSources.IsValidIndex(I))
  {
   const auto& L=Frame->IlluminationSources[I].Description;
   if(L.bActive && L.FloorId==Receiver->Box.Floor && FVector::DotProduct(Normal,L.Transform.GetLocation()-Point)>1.e-4 && !CannotCover(Frame->IlluminationSources[I]))return false;
  }
 }
 return true;
}

bool USightWeaveWorldSubsystem::UpdateSurfaceBox(const FSightWeaveSurfaceBox& Box)
{
 auto* Existing=SurfaceBoxes.Find(Box.Id);if(!bSightWeaveInitialized || !Existing || !Box.IsValid() || !Floors.Contains(Box.Floor))return false;
 if(Existing->Box.Floor==Box.Floor && Existing->Box.Pose.Equals(Box.Pose,0) && Existing->Box.HalfExtent==Box.HalfExtent)return true;
 *Existing={Box,++SurfaceSerial};bSurfaceSceneDirty=true;AdvanceRevision();PublishSnapshot();return true;
}
bool USightWeaveWorldSubsystem::UnregisterSurfaceBox(FName Id)
{
 if(!bSightWeaveInitialized || !SurfaceBoxes.Remove(Id))return false;
 SurfaceOwners.Remove(Id);bSurfaceSceneDirty=true;AdvanceRevision();PublishSnapshot();return true;
}
FSightWeaveSurfaceResult USightWeaveWorldSubsystem::QuerySurfaceSample(FSightWeaveKnowledgeOwnerId Owner,const FSightWeaveSurfaceSample& Sample) const
{
 TArray<FSightWeaveSurfaceResult> Results;QuerySurfaceSamples(Owner,MakeArrayView(&Sample,1),Results);return Results[0];
}
void USightWeaveWorldSubsystem::QuerySurfaceSamples(FSightWeaveKnowledgeOwnerId Owner,TConstArrayView<FSightWeaveSurfaceSample> Samples,
 TArray<FSightWeaveSurfaceResult>& Results,FSightWeaveSurfaceQueryCache* Cache,FSightWeaveSurfaceQueryStats* Stats) const
{
 check(IsInGameThread());const auto Frame=AcquirePublishedSnapshot();Results.Reset(Samples.Num());
 if(Cache && (Cache->Frame!=Frame || Cache->Owner!=Owner)){Cache->Samples.Reset();Cache->Frame=Frame;Cache->Owner=Owner;}
 for(const auto& Sample:Samples)
 {
  if(Cache)if(const auto* Hit=Cache->Samples.Find(Sample)){Results.Add(*Hit);if(Stats)++Stats->CacheHits;continue;}
  FSightWeaveSurfaceResult R;R.Sample=Sample;R.Hard=MakeQueryResult(ESightWeaveQueryStatus::InvalidInput,Owner,{});
  const auto* Receiver=Frame && Frame->SurfaceScene?Frame->SurfaceScene->Find(Sample.Receiver):nullptr;
  if(!bSightWeaveInitialized || !Frame)R.Hard.Status=ESightWeaveQueryStatus::NotReady;
  else if(!Receiver)R.Hard.Status=ESightWeaveQueryStatus::InvalidHandle;
  else
  {
   FVector Normal;R.ReceiverRevision=Receiver->Revision;R.Hard.FloorId=Receiver->Box.Floor;
   const auto* Floor=Frame->Floors.FindByPredicate([&](const auto& F){return F.FloorId==Receiver->Box.Floor;});
   if(!Floor)R.Hard.Status=ESightWeaveQueryStatus::InvalidFloor;
   else if(Owner.IsValid() && Receiver->Box.Resolve(Sample.Face,Sample.UV,R.WorldPoint,Normal))
   {
    FSightWeaveSurfaceContext Context{Frame->SurfaceScene.Get(),Normal,Stats};if(Stats)++Stats->ExactSamples;
    QueryEffectiveLiveValidated(Owner,Floor->FloorId,R.WorldPoint,nullptr,false,*Frame,*Floor,GetDefault<USightWeaveSettings>()->GeometryTolerances,
     R.Hard,nullptr,0,0,false,false,&Context);
   }
  }
  // Invalid/non-finite requests are never hashed into reusable evidence.
  if(Cache && R.Hard.bAuthoritative){if(Cache->Samples.Num()>=4096)Cache->Samples.Reset();Cache->Samples.Add(Sample,R);}
  Results.Add(MoveTemp(R));
 }
}
