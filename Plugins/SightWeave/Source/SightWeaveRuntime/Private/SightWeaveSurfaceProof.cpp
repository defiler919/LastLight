#include "SightWeaveSurface.h"

namespace
{
// A world-space margin on normalized planes. The two envelopes deliberately
// leave a gap; ambiguous/contact geometry falls back to the existing beam test.
constexpr double Margin=1.e-4;
struct FShadowPlane { FVector Normal; double Offset; };
using FPlanes=TArray<FShadowPlane,TInlineAllocator<16>>;

bool AddPlane(FPlanes& Planes,FVector N,FVector Point,FVector Interior)
{
 if(!N.Normalize())return false;
 if(FVector::DotProduct(N,Interior-Point)>0)N=-N;
 Planes.Add({N,FVector::DotProduct(N,Point)});return true;
}

FSightWeaveSurfaceShadow ClipFace(const FSightWeaveSurfaceBox& Box,ESightWeaveBoxFace Face,const FPlanes& Planes,double Padding)
{
 FSightWeaveSurfaceShadow Out;
 Out.Polygon={{-1,-1},{1,-1},{1,1},{-1,1}};
 FVector Center,N,U,V;Box.Resolve(Face,{0,0},Center,N);Box.Resolve(Face,{1,0},U,N);Box.Resolve(Face,{0,1},V,N);U-=Center;V-=Center;
 for(const auto& Plane:Planes)
 {
  const double A=FVector::DotProduct(Plane.Normal,U),B=FVector::DotProduct(Plane.Normal,V);
  const double C=Plane.Offset+Padding-FVector::DotProduct(Plane.Normal,Center);
  decltype(Out.Polygon) Next;
  if(Out.Polygon.IsEmpty())break;
  FVector2D Previous=Out.Polygon.Last();double DP=A*Previous.X+B*Previous.Y-C;
  for(const auto Current:Out.Polygon)
  {
   const double DC=A*Current.X+B*Current.Y-C;
   if((DP<=0)!=(DC<=0))Next.Add(Previous+(Current-Previous)*(DP/(DP-DC)));
   if(DC<=0)Next.Add(Current);
   Previous=Current;DP=DC;
  }
  Out.Polygon=MoveTemp(Next);
 }
 for(auto P:Out.Polygon)Out.Bounds+=P;
 return Out;
}

// Convex shadow volume of a box: eye-facing box planes plus silhouette planes
// through the eye and edges separating a facing and a non-facing box face.
// This is conv(box + rays away from eye), so it also handles boxes crossing
// the receiver plane or extending above the eye without finite projection caps.
bool BoxPlanes(const FSightWeaveSurfaceBox& Box,FVector Eye,double Shrink,FPlanes& Planes)
{
 const FVector E=Box.HalfExtent-FVector(Shrink),LocalEye=Box.Pose.InverseTransformPosition(Eye);
 bool Facing[3][2],Outside=false,OnBoundary=false;
 for(int A=0;A<3;++A)for(int S=0;S<2;++S)
 {
  const double Sign=S?1.:-1.,Distance=Sign*LocalEye[A]-E[A];
  OnBoundary|=Distance==0;Outside|=Distance>0;
  Facing[A][S]=Distance>0;
  if(Facing[A][S])
  {
   FVector N=FVector::ZeroVector;N[A]=Sign;
   const FVector WorldN=Box.Pose.TransformVectorNoScale(N);
   Planes.Add({WorldN,FVector::DotProduct(WorldN,Box.Pose.TransformPosition(N*E[A]))});
  }
 }
 // A tangent face is not an eye-facing face. When the eye is outside another
 // slab the silhouette still defines a valid shadow, including exact alignment
 // with a wall's side. Only an eye ON the solid without an outside slab needs
 // fallback; treating that boundary as an interior eye would overstate shadow.
 if(!Outside && OnBoundary)return false;
 for(int A=0;A<3;++A)for(int B=A+1;B<3;++B)for(int SA=0;SA<2;++SA)for(int SB=0;SB<2;++SB)
 {
  if(Facing[A][SA]==Facing[B][SB])continue;
  const int C=3-A-B;FVector P=FVector::ZeroVector;P[A]=(SA?1:-1)*E[A];P[B]=(SB?1:-1)*E[B];
  FVector Edge=FVector::ZeroVector;Edge[C]=1;
  const FVector WorldP=Box.Pose.TransformPosition(P);
  if(!AddPlane(Planes,FVector::CrossProduct(Box.Pose.TransformVectorNoScale(Edge),WorldP-Eye),Eye,Box.Pose.GetLocation()))return false;
 }
 // No planes means the eye is strictly inside the solid: every ray starts
 // inside it and its shadow covers the complete receiver.
 return true;
}

bool WallPlanes(const FSightWeaveSegment2D& W,FVector Eye,FPlanes& Planes)
{
 const FVector P[]{FVector(W.A,W.HeightRange.ZMin),FVector(W.B,W.HeightRange.ZMin),FVector(W.B,W.HeightRange.ZMax),FVector(W.A,W.HeightRange.ZMax)};
 const FVector Center=(P[0]+P[2])*.5;
 FVector Normal=FVector::CrossProduct(P[1]-P[0],P[3]-P[0]);if(!Normal.Normalize())return false;
 const double From=FVector::DotProduct(Normal,Eye-P[0]);if(FMath::Abs(From)<=Margin)return false;
 if(From<0)Normal=-Normal;
 Planes.Add({Normal,FVector::DotProduct(Normal,P[0])});
 for(int I=0;I<4;++I)if(!AddPlane(Planes,FVector::CrossProduct(P[I]-Eye,P[(I+1)%4]-Eye),Eye,Center))return false;
 return true;
}
}

bool FSightWeaveSurfaceShadow::Intersects(const FBox2D& R) const
{
 if(Polygon.Num()<3 || !Bounds.Intersect(R))return false;
 // Polygon winding is CCW after clipping; an edge with the whole rectangle
 // strictly outside separates them. Touching remains ambiguous/occluded.
 const FVector2D Center=R.GetCenter(),Extent=R.GetExtent();
 for(int I=0;I<Polygon.Num();++I)
 {
  const FVector2D A=Polygon[I],E=Polygon[(I+1)%Polygon.Num()]-A;
  const FVector2D N(-E.Y,E.X);
  if(FVector2D::DotProduct(N,Center-A)+FVector2D::DotProduct(N.GetAbs(),Extent)<-1.e-12)return false;
 }
 return true;
}
bool FSightWeaveSurfaceShadow::Contains(const FBox2D& R) const
{
 if(Polygon.Num()<3 || !Bounds.IsInsideOrOn(R.Min) || !Bounds.IsInsideOrOn(R.Max))return false;
 const FVector2D Center=R.GetCenter(),Extent=R.GetExtent();
 for(int I=0;I<Polygon.Num();++I)
 {
  const FVector2D A=Polygon[I],E=Polygon[(I+1)%Polygon.Num()]-A,N(-E.Y,E.X);
  if(FVector2D::DotProduct(N,Center-A)-FVector2D::DotProduct(N.GetAbs(),Extent)<0)return false;
 }
 return true;
}
bool FSightWeaveSurfaceBeamProof::IsClear(const FBox2D& R) const
{if(!bComplete)return false;for(const auto& S:ClearShadows)if(S.Intersects(R))return false;return true;}
bool FSightWeaveSurfaceBeamProof::IsBlocked(const FBox2D& R) const
{for(const auto& S:BlockedShadows)if(S.Contains(R))return true;return false;}

bool FSightWeaveSurfaceShadow::CrossesCellStrip(const FBox2D& R,int32 Axis) const
{
 if(Polygon.Num()<3 || !Bounds.Intersect(R) || (Axis!=0 && Axis!=1))return false;
 // Find a line parallel to the long axis entirely inside the INNER shadow.
 // That line supplies a truly blocked point to every cell along the strip;
 // none of those full support cells can pass the all-ray positive proof.
 double Low=R.Min[Axis],High=R.Max[Axis];const int Other=1-Axis;
 for(int I=0;I<Polygon.Num();++I)
 {
  const FVector2D A=Polygon[I],E=Polygon[(I+1)%Polygon.Num()]-A,N(-E.Y,E.X);
  const double Bound=FVector2D::DotProduct(N,A)-FMath::Min(N[Other]*R.Min[Other],N[Other]*R.Max[Other]);
  if(N[Axis]==0){if(Bound>0)return false;continue;}
  if(N[Axis]>0)Low=FMath::Max(Low,Bound/N[Axis]);else High=FMath::Min(High,Bound/N[Axis]);
  if(Low>High)return false;
 }
 return High-Low>1.e-10;
}

FSightWeaveSurfaceBeamProof FSightWeaveSurfaceScene::PrepareFaceBeam(const FSightWeaveSurfaceBox& Receiver,ESightWeaveBoxFace Face,FVector Origin) const
{
 FSightWeaveSurfaceBeamProof Out;Out.Origin=Origin;
 FVector Center,Normal;if(!Receiver.IsValid() || Origin.ContainsNaN() || !Receiver.Resolve(Face,{0,0},Center,Normal)
  || FVector::DotProduct(Normal,Origin-Center)<=Margin){Out.bComplete=false;return Out;}
 const double TargetPlane=FVector::DotProduct(Normal,Center),EyePlane=FVector::DotProduct(Normal,Origin);
 for(const auto& P:Primitives)
 {
  // Match the open terminal-slab exclusion in BeamClear before constructing
  // shadows; geometry behind the target or eye cannot obstruct these rays.
  const double Mid=FVector::DotProduct(Normal,P.Bounds.GetCenter()),Radius=FVector::DotProduct(Normal.GetAbs(),P.Bounds.GetExtent());
  if(Mid+Radius<=TargetPlane || Mid-Radius>=EyePlane)continue;
  FPlanes Clear,Blocked;bool ClearValid=false,BlockedValid=false;
  if(P.BoxIndex!=INDEX_NONE)
  {
   const auto& B=Boxes[P.BoxIndex].Box;if(B.Floor!=Receiver.Floor || B.Id==Receiver.Id)continue;
   // Preserve the reference SAT's WORLD-space separation margin. Padding only
   // the projected silhouette would shrink that margin near the eye. Expanding
   // the solid by Margin in each local axis encloses its Margin sphere offset.
   ClearValid=BoxPlanes(B,Origin,-Margin,Clear);BlockedValid=BoxPlanes(B,Origin,Margin,Blocked);
  }
  else
  {
   if(P.Wall.FloorId!=Receiver.Floor)continue;
   const auto& W=P.Wall;const FVector2D Edge=W.B-W.A;
   FSightWeaveSurfaceBox Envelope;Envelope.Pose=FTransform(FRotator(0,FMath::RadiansToDegrees(FMath::Atan2(Edge.Y,Edge.X)),0),FVector((W.A+W.B)*.5,(W.HeightRange.ZMin+W.HeightRange.ZMax)*.5));
   Envelope.HalfExtent={Edge.Length()*.5+Margin,Margin,(W.HeightRange.ZMax-W.HeightRange.ZMin)*.5+Margin};
   ClearValid=BoxPlanes(Envelope,Origin,0,Clear);BlockedValid=WallPlanes(W,Origin,Blocked);
  }
  if(ClearValid)
  {
   auto Shadow=ClipFace(Receiver,Face,Clear,Margin);
   if(Shadow.Polygon.Num()>=3)Out.ClearShadows.Add(MoveTemp(Shadow));
  }
  else Out.bComplete=false;
  if(BlockedValid)
  {
   auto Shadow=ClipFace(Receiver,Face,Blocked,-Margin);
   if(Shadow.Polygon.Num()>=3)Out.BlockedShadows.Add(MoveTemp(Shadow));
  }
 }
 return Out;
}
