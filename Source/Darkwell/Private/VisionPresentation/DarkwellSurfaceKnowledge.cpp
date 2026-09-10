#include "VisionPresentation/DarkwellSurfaceKnowledge.h"
#include "VisionPresentation/DarkwellMemoryRegionSamples.h"
#include "SightWeaveWorldSubsystem.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

bool FDarkwellSurfaceKnowledge::Initialize(const FSightWeaveSurfaceBox& B,FSightWeaveKnowledgeOwnerId O,uint32 Version)
{
 if(!B.IsValid() || !O.IsValid() || Version==0)return false;
 TArray<FFace> New;int Width=0,Height=0;
 for(int F=0;F<6;++F)
 {
  const int A=F/2;FFace P;P.Size={FMath::CeilToInt32(2*B.HalfExtent[(A+1)%3]/SampleCm),FMath::CeilToInt32(2*B.HalfExtent[(A+2)%3]/SampleCm)};
  if(P.Size.X>4096 || P.Size.Y>4096 || int64(P.Size.X)*P.Size.Y>4194304)return false;
  P.Offset=Height;Height+=FMath::Min(P.Size.X,P.Size.Y);Width=FMath::Max(Width,FMath::Max(P.Size.X,P.Size.Y));New.Add(MoveTemp(P));
 }
 if(Height>8192 || int64(Width)*Height>16777216)return false;
 for(auto& P:New){P.Known.Init(false,P.Size.X*P.Size.Y);P.Live.Init(false,P.Known.Num());P.Hidden.Init(false,P.Known.Num());}
 Box=B;Owner=O;ContentVersion=Version;Faces=MoveTemp(New);AtlasSize={Width,Height};LastFrame=MAX_uint64;bDirty=true;return true;
}
FVector FDarkwellSurfaceKnowledge::Position(int F,int X,int Y) const
{FVector P,N;Box.Resolve(ESightWeaveBoxFace(F),FVector2D((X+.5)/Faces[F].Size.X,(Y+.5)/Faces[F].Size.Y)*2-FVector2D(1),P,N);return P;}
int FDarkwellSurfaceKnowledge::Index(int F,FVector2D UV) const
{if(!Faces.IsValidIndex(F) || UV.ContainsNaN() || UV.GetAbsMax()>1)return INDEX_NONE;const auto S=Faces[F].Size;return FMath::Min(S.Y-1,FMath::FloorToInt((UV.Y+1)*.5*S.Y))*S.X+FMath::Min(S.X-1,FMath::FloorToInt((UV.X+1)*.5*S.X));}
bool FDarkwellSurfaceKnowledge::IsKnown(ESightWeaveBoxFace F,FVector2D UV) const
{const int I=Index(int(F),UV);return I!=INDEX_NONE && Faces[int(F)].Known[I];}
bool FDarkwellSurfaceKnowledge::IsLive(ESightWeaveBoxFace F,FVector2D UV) const
{const int I=Index(int(F),UV);return I!=INDEX_NONE && Faces[int(F)].Live[I];}
void FDarkwellSurfaceKnowledge::MarkDirty(int F,int X0,int Y0,int X1,int Y1)
{auto& D=Faces[F].Dirty;const FIntRect R(X0,Y0,X1,Y1);if(D.Width()==0)D=R;else D.Union(R);bDirty=true;}
bool FDarkwellSurfaceKnowledge::Observe(USightWeaveWorldSubsystem& R)
{
 Proofs=ExactSamples=UnresolvedCells=0;FSightWeaveSurfaceQueryStats QueryStats;
 auto Frame=R.AcquirePublishedSnapshot();if(!Frame)return false;
 const auto Memory=R.AcquirePublishedMemoryPacket();const uint64 Modifier=Memory?Memory->GetModifierRevision():0;
 if(LastFrame==uint64(Frame->Revision.GetValue()) && LastModifier==Modifier)return false;
 LastModifier=Modifier;
 LastFrame=Frame->Revision.GetValue();Proofs=ExactSamples=0;
 const auto* Current=Frame->SurfaceScene?Frame->SurfaceScene->Find(Box.Id):nullptr;
 // Immutable/stationary declaration violation: disable Live, retain facts at the
 // captured domain. Never stretch old Known onto new geometry or a new pose.
 const bool Same=Current && Current->Box.Pose.Equals(Box.Pose,0) && Current->Box.HalfExtent==Box.HalfExtent && Current->Box.Floor==Box.Floor;
 bool Changed=false;
 for(int F=0;F<Faces.Num();++F)
 {
  auto& Face=Faces[F];const auto S=Face.Size;
  FBox FaceBounds(ForceInit);for(auto C:{FIntPoint(0,0),FIntPoint(S.X-1,0),FIntPoint(0,S.Y-1),S-FIntPoint(1,1)})FaceBounds+=Position(F,C.X,C.Y);
  const FBox2D FaceXY(FVector2D(FaceBounds.Min),FVector2D(FaceBounds.Max));
  const bool FaceBlock=Block.bIsValid && Block.Intersect(FaceXY);
  bool HasFaceModifiers=false;
  if(Memory && Memory->GetScope().KnowledgeOwnerId==Owner && Memory->GetScope().FloorId==Box.Floor)
   for(const auto& M:Memory->GetPresentationSuppressions())
   {
    const auto& G=M.Region;if(FaceBounds.Max.Z<G.HeightRange.ZMin || FaceBounds.Min.Z>G.HeightRange.ZMax)continue;
    if(G.Shape==ESightWeaveMemoryRegionShape::Circle){if(!FaceXY.Intersect(FBox2D(G.Center-FVector2D(G.Radius),G.Center+FVector2D(G.Radius))))continue;}
    else if(G.Shape==ESightWeaveMemoryRegionShape::AxisAlignedBox){if(!FaceXY.Intersect(FBox2D(G.Center-G.HalfExtents,G.Center+G.HalfExtents)))continue;}
    HasFaceModifiers=true;break;
   }
  auto Visit=[&](auto&& Self,int X0,int Y0,int X1,int Y1)->void
  {
   const FBox2D UV(FVector2D(double(X0)/S.X,double(Y0)/S.Y)*2-FVector2D(1),FVector2D(double(X1)/S.X,double(Y1)/S.Y)*2-FVector2D(1));
   bool Value=false;const bool Uniform=!Same || R.TrySurfaceRegion(Owner,Box.Id,ESightWeaveBoxFace(F),UV,Value,&QueryStats);
   Proofs+=Uniform;
   if(!Uniform && (X1-X0>1 || Y1-Y0>1))
   {if(X1-X0>=Y1-Y0){const int M=(X0+X1)/2;Self(Self,X0,Y0,M,Y1);Self(Self,M,Y0,X1,Y1);}else{const int M=(Y0+Y1)/2;Self(Self,X0,Y0,X1,M);Self(Self,X0,M,X1,Y1);}return;}
   if(!Uniform)
   {
    // Even five passing points cannot authorize an unsampled cell interior.
    // Preserve sub-centimeter unresolved boundaries as Unknown, never expand.
    ++UnresolvedCells;Value=false;
   }
   if(!FaceBlock && !HasFaceModifiers)
   {
    for(int Y=Y0;Y<Y1;++Y)
    {
     const int I=Y*S.X+X0,End=Y*S.X+X1,Count=X1-X0;
     const bool RowChanged=Face.Live.CountSetBits(I,End)!=(Value?Count:0) || Face.Hidden.CountSetBits(I,End)>0 || (Value && Face.Known.CountSetBits(I,End)!=Count);
     if(RowChanged){Changed=true;MarkDirty(F,X0,Y,X1,Y+1);}
     Face.Live.SetRange(I,Count,Value);Face.Hidden.SetRange(I,Count,false);if(Value)Face.Known.SetRange(I,Count,true);
    }
    return;
   }
   for(int Y=Y0;Y<Y1;++Y)for(int X=X0;X<X1;++X)
   {const int I=Y*S.X+X;bool CellChanged=false;if(Face.Live[I]!=Value){Face.Live[I]=Value;CellChanged=true;}
    const bool HasModifiers=HasFaceModifiers;
    const FVector P=Block.bIsValid || HasModifiers?Position(F,X,Y):FVector::ZeroVector;bool Blocked=Block.bIsValid && Darkwell::MemoryRegionSamples::Contains(Block,FVector2D(P)),Hidden=Blocked;
    if(HasModifiers)
     for(const auto& M:Memory->GetPresentationSuppressions())if(M.Region.ContainsWorldLocation(P))
     {Hidden=true;Blocked|=M.Operation==ESightWeaveMemoryModifierOperation::BlockMemoryWrites;}
    if(Face.Hidden[I]!=Hidden){Face.Hidden[I]=Hidden;CellChanged=true;}
    if(Value && !Face.Known[I] && !Blocked){Face.Known[I]=true;CellChanged=true;}
    if(CellChanged){Changed=true;MarkDirty(F,X,Y,X+1,Y+1);}}
  };
  Visit(Visit,0,0,S.X,S.Y);
 }
 ExactSamples=QueryStats.ExactSamples;bDirty|=Changed;return Changed;
}
void FDarkwellSurfaceKnowledge::Clear(const FBox2D& Region)
{
 for(int F=0;F<Faces.Num();++F){auto& P=Faces[F];FBox2D Bounds(ForceInit);
  for(auto C:{FIntPoint(0,0),FIntPoint(P.Size.X-1,0),FIntPoint(0,P.Size.Y-1),P.Size-FIntPoint(1,1)})Bounds+=FVector2D(Position(F,C.X,C.Y));
  if(!Region.Intersect(Bounds))continue;
  if(Darkwell::MemoryRegionSamples::Contains(Region,Bounds.Min) && Darkwell::MemoryRegionSamples::Contains(Region,Bounds.Max))
  {if(P.Known.CountSetBits()>0){MarkDirty(F,0,0,P.Size.X,P.Size.Y);P.Known.SetRange(0,P.Known.Num(),false);}continue;}
  for(int Y=0;Y<P.Size.Y;++Y)for(int X=0;X<P.Size.X;++X)
  if(Darkwell::MemoryRegionSamples::Contains(Region,FVector2D(Position(F,X,Y)))){const int I=Y*P.Size.X+X;if(P.Known[I])MarkDirty(F,X,Y,X+1,Y+1);P.Known[I]=false;}}
 LastFrame=MAX_uint64; // next Observe is new evidence; Clear itself never observes.
}
void FDarkwellSurfaceKnowledge::SetBlock(const FBox2D& Region,bool Enabled)
{Block=Enabled?Region:FBox2D(ForceInit);LastFrame=MAX_uint64;bDirty=true;}
double FDarkwellSurfaceKnowledge::LiveSpanCm() const
{
 double Span=0;for(int F=0;F<Faces.Num();++F){const auto& P=Faces[F];int Min=P.Size.X,Max=-1;
  for(int I=0;I<P.Live.Num();++I)if(P.Live[I] && P.Known[I] && !P.Hidden[I]){Min=FMath::Min(Min,I%P.Size.X);Max=FMath::Max(Max,I%P.Size.X);}
  if(Max>=Min)Span=FMath::Max(Span,(Max-Min+1)*2*Box.HalfExtent[(F/2+1)%3]/P.Size.X);}
 return Span;
}
void FDarkwellSurfaceKnowledge::Pixels(TArray<FColor>& Out) const
{
 Out.Init(FColor::Black,AtlasSize.X*AtlasSize.Y);
 for(const auto& P:Faces)for(int Y=0;Y<P.Size.Y;++Y)for(int X=0;X<P.Size.X;++X)
 {const int I=Y*P.Size.X+X;const bool Transpose=P.Size.Y>P.Size.X;Out[((Transpose?X:Y)+P.Offset)*AtlasSize.X+(Transpose?Y:X)]=FColor(P.Live[I]?255:0,P.Known[I]?255:0,P.Hidden[I]?255:0,255);}
}
void FDarkwellSurfaceKnowledge::Save(TArray<uint8>& Out) const
{
 Out.Reset();FMemoryWriter W(Out);uint32 Schema=1,V=ContentVersion;FString Id=Box.Id.ToString(),O=Owner.GetValue().ToString(),Floor=Box.Floor.GetValue().ToString();FVector E=Box.HalfExtent;FTransform Pose=Box.Pose;
 W<<Schema<<V<<Id<<O<<Floor<<E<<Pose;
 for(const auto& F:Faces){uint32 Count=F.Known.Num();W<<Count;for(uint32 I=0;I<Count;I+=8){uint8 Byte=0;for(uint32 J=0;J<8 && I+J<Count;++J)if(F.Known[I+J])Byte|=1<<J;W<<Byte;}}
}
bool FDarkwellSurfaceKnowledge::Load(const TArray<uint8>& Data)
{
 if(Data.Num()>16777216 || Faces.Num()!=6)return false;
 FMemoryReader R(Data);R.ArMaxSerializeSize=16777216;uint32 Schema=0,V=0;FString Id,O,Floor;FVector E;FTransform Pose;
 R<<Schema<<V<<Id<<O<<Floor<<E<<Pose;
 if(R.IsError() || Schema!=1 || V!=ContentVersion || Id!=Box.Id.ToString() || O!=Owner.GetValue().ToString() || Floor!=Box.Floor.GetValue().ToString() || E!=Box.HalfExtent || !Pose.Equals(Box.Pose,0))return false;
 TArray<TBitArray<>> Bits;for(int F=0;F<6;++F){uint32 Count=0;R<<Count;if(R.IsError() || Count!=uint32(Faces[F].Known.Num()) || R.TotalSize()-R.Tell()<(Count+7)/8)return false;
  auto& B=Bits.AddDefaulted_GetRef();B.Init(false,Count);for(uint32 I=0;I<Count;I+=8){uint8 Byte=0;R<<Byte;for(uint32 J=0;J<8 && I+J<Count;++J)B[I+J]=(Byte&(1<<J))!=0;}}
 if(R.Tell()!=R.TotalSize())return false;
 for(int F=0;F<6;++F){Faces[F].Known=MoveTemp(Bits[F]);MarkDirty(F,0,0,Faces[F].Size.X,Faces[F].Size.Y);}bDirty=true;return true;
}

