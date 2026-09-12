#include "VisionPresentation/DarkwellSurfaceKnowledge.h"
#include "VisionPresentation/DarkwellMemoryRegionSamples.h"
#include "SightWeaveWorldSubsystem.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "HAL/IConsoleManager.h"

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
 static const auto* PreparedMode=IConsoleManager::Get().FindConsoleVariable(TEXT("SightWeave.Surface.PreparedProofs"));
 const bool UseStrips=PreparedMode && PreparedMode->GetInt()!=0;
 for(int F=0;F<Faces.Num();++F)
 {
  auto& Face=Faces[F];const auto S=Face.Size;
  static const auto* Profile=IConsoleManager::Get().FindConsoleVariable(TEXT("SightWeave.Surface.Profile"));const double FaceStart=Profile && Profile->GetInt()?FPlatformTime::Seconds():0;
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
  auto Apply=[&](int X0,int Y0,int X1,int Y1,bool Value)
  {
   if(!FaceBlock && !HasFaceModifiers)
   {
    for(int Y=Y0;Y<Y1;++Y)
    {
     const int I=Y*S.X+X0,End=Y*S.X+X1,Count=X1-X0;
     if(UseStrips)
     {
      // One masked word pass replaces three population counts and up to three
      // range writes. Preserve neighbouring cells and the bit-array tail.
      auto* Live=Face.Live.GetData();auto* Known=Face.Known.GetData();auto* Hidden=Face.Hidden.GetData();bool RowChanged=false;
      for(int W=I/32;W<=(End-1)/32;++W)
      {
       uint32 Mask=~uint32(0);if(W==I/32)Mask&=~uint32(0)<<(I%32);if(W==(End-1)/32 && End%32)Mask&=(uint32(1)<<(End%32))-1;
       const uint32 NewLive=Value?Live[W]|Mask:Live[W]&~Mask,NewKnown=Value?Known[W]|Mask:Known[W],NewHidden=Hidden[W]&~Mask;
       if(NewLive!=Live[W] || NewKnown!=Known[W] || NewHidden!=Hidden[W])
       {RowChanged=true;Live[W]=NewLive;Known[W]=NewKnown;Hidden[W]=NewHidden;}
      }
      if(RowChanged){Changed=true;MarkDirty(F,X0,Y,X1,Y+1);}continue;
     }
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
  auto Visit=[&](auto&& Self,int X0,int Y0,int X1,int Y1)->void
  {
   const FBox2D UV(FVector2D(double(X0)/S.X,double(Y0)/S.Y)*2-FVector2D(1),FVector2D(double(X1)/S.X,double(Y1)/S.Y)*2-FVector2D(1));
   bool Value=false;const bool Uniform=!Same || R.TrySurfaceRegion(Owner,Box.Id,ESightWeaveBoxFace(F),UV,Value,&QueryStats);
   Proofs+=Uniform;
   const bool RejectedStrip=!Uniform && UseStrips && (X1-X0==1 || Y1-Y0==1)
    && R.RejectSurfaceCellStrip(Owner,Box.Id,ESightWeaveBoxFace(F),UV,X1-X0==1?0:1);
   if(!Uniform && !RejectedStrip && (X1-X0>1 || Y1-Y0>1))
   {
    const bool SplitX=X1-X0>=Y1-Y0;
    if(SplitX){const int M=(X0+X1)/2;Self(Self,X0,Y0,M,Y1);Self(Self,M,Y0,X1,Y1);}else{const int M=(Y0+Y1)/2;Self(Self,X0,Y0,X1,M);Self(Self,X0,M,X1,Y1);}return;
   }
   if(!Uniform)
   {
    // Even five passing points cannot authorize an unsampled cell interior.
    // Preserve sub-centimeter unresolved boundaries as Unknown, never expand.
    UnresolvedCells+=uint64(X1-X0)*(Y1-Y0);Value=false;
   }
   Apply(X0,Y0,X1,Y1,Value);
  };
  bool WholeValue=false;TArray<FSightWeaveSurfaceCellSpan> Spans;
  if(Same && UseStrips)
  {
   if(R.TrySurfaceRegion(Owner,Box.Id,ESightWeaveBoxFace(F),FBox2D({-1,-1},{1,1}),WholeValue,&QueryStats))
   {++Proofs;Apply(0,0,S.X,S.Y,WholeValue);}
   else if(R.BuildSurfaceFaceSpans(Owner,Box.Id,ESightWeaveBoxFace(F),S,Spans,&QueryStats))
   {
    for(const auto& Span:Spans){if(Span.bUnresolved)UnresolvedCells+=Span.Cells.Area();else ++Proofs;}
    if(!FaceBlock && !HasFaceModifiers)
    {
     // Raster certificates can be narrow columns. Build Live once, then update
     // the three persistent bit planes in one contiguous pass instead of doing
     // read/modify/write on every column crossing the same storage word.
     TBitArray<> Next(false,Face.Live.Num());
     for(const auto& Span:Spans)if(Span.bLive)
      for(int Y=Span.Cells.Min.Y;Y<Span.Cells.Max.Y;++Y)Next.SetRange(Y*S.X+Span.Cells.Min.X,Span.Cells.Width(),true);
     auto* Live=Face.Live.GetData();auto* Known=Face.Known.GetData();auto* Hidden=Face.Hidden.GetData();const auto* New=Next.GetData();
     for(int W=0;W<(Face.Live.Num()+31)/32;++W)
     {
      const uint32 Difference=(Live[W]^New[W])|(New[W]&~Known[W])|Hidden[W];
      if(!Difference)continue;
      const int First=W*32+FMath::CountTrailingZeros(Difference),Last=FMath::Min(Face.Live.Num()-1,W*32+31-int(FMath::CountLeadingZeros(Difference)));
      if(First/S.X==Last/S.X)MarkDirty(F,First%S.X,First/S.X,Last%S.X+1,Last/S.X+1);
      else MarkDirty(F,0,First/S.X,S.X,Last/S.X+1);
      Live[W]=New[W];Known[W]|=New[W];Hidden[W]=0;Changed=true;
     }
    }
    else for(const auto& Span:Spans)Apply(Span.Cells.Min.X,Span.Cells.Min.Y,Span.Cells.Max.X,Span.Cells.Max.Y,Span.bLive);
   }
   else Visit(Visit,0,0,S.X,S.Y);
  }
  else Visit(Visit,0,0,S.X,S.Y);
  if(FaceStart && FPlatformTime::Seconds()-FaceStart>.0005)UE_LOG(LogTemp,Display,TEXT("SURFACE_FACE_PROFILE id=%s face=%d us=%.3f spans=%d"),*Box.Id.ToString(),F,(FPlatformTime::Seconds()-FaceStart)*1.e6,Spans.Num());
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
 double Span=0;for(int F=0;F<Faces.Num();++F){const auto& P=Faces[F];int Previous=INDEX_NONE,Run=0,Longest=0;
  for(TConstSetBitIterator<> It(P.Live);It;++It)
  {
   const int I=It.GetIndex();if(!P.Known[I] || P.Hidden[I]){Previous=INDEX_NONE;Run=0;continue;}
   Run=I==Previous+1 && I%P.Size.X!=0?Run+1:1;Previous=I;Longest=FMath::Max(Longest,Run);
  }
  Span=FMath::Max(Span,Longest*2*Box.HalfExtent[(F/2+1)%3]/P.Size.X);}
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

