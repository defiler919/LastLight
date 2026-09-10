#include "SightWeaveSurface.h"
#include "SightWeaveWorldSubsystem.h"
#include "SightWeaveSettings.h"
#include "Algo/Sort.h"

namespace
{
constexpr double Contact=1.e-4; // cm; endpoint contact, not a coverage expansion
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
