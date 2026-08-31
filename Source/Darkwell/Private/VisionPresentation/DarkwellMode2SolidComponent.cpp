#include "VisionPresentation/DarkwellMode2SolidComponent.h"
#include "VisionPresentation/DarkwellEmptyVerification.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "VisionPresentation/DarkwellRememberablePropComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace Darkwell::Mode2Solid
{
 using namespace UE::Geometry;
 constexpr double TessellationCm=2.5;
 struct FPoint { FVector2D P; float Value; };

 void Triangle(FDynamicMesh3& Mesh,FVector A,FVector B,FVector C,int32 Material)
 {
  const FVector N=FVector::CrossProduct(B-A,C-A).GetSafeNormal();
  if(N.IsNearlyZero()) return;
  const int32 X=Mesh.AppendVertex(A),Y=Mesh.AppendVertex(B),Z=Mesh.AppendVertex(C);
  const int32 T=Mesh.AppendTriangle(X,Y,Z);
  Mesh.Attributes()->GetMaterialID()->SetValue(T,Material);
  auto* Normals=Mesh.Attributes()->PrimaryNormals();
  Normals->SetTriangle(T,FIndex3i(Normals->AppendElement(FVector3f(N)),Normals->AppendElement(FVector3f(N)),Normals->AppendElement(FVector3f(N))));
 }

 // Marching triangles avoids ambiguous saddle cells. Only external edges and
 // the interpolated zero contour get side walls; no internal voxel walls.
 void ExtrudeTriangle(FDynamicMesh3& Mesh,const FPoint* Points,const FBox& Box,int32& Caps)
 {
  TArray<FPoint,TInlineAllocator<4>> Polygon;
  for(int32 I=0;I<3;++I)
  {
   const FPoint A=Points[I],B=Points[(I+1)%3];
   if(A.Value>0) Polygon.Add(A);
   if((A.Value>0)!=(B.Value>0)) Polygon.Add({FMath::Lerp(A.P,B.P,double(A.Value/(A.Value-B.Value))),0});
  }
  if(Polygon.Num()<3) return;
  auto At=[](FVector2D P,double Z){return FVector(P.X,P.Y,Z);};
  for(int32 I=1;I+1<Polygon.Num();++I)
  {
   Triangle(Mesh,At(Polygon[0].P,Box.Max.Z),At(Polygon[I].P,Box.Max.Z),At(Polygon[I+1].P,Box.Max.Z),0);
   Triangle(Mesh,At(Polygon[0].P,Box.Min.Z),At(Polygon[I+1].P,Box.Min.Z),At(Polygon[I].P,Box.Min.Z),0);
  }
  for(int32 I=0;I<Polygon.Num();++I)
  {
   const FPoint A=Polygon[I],B=Polygon[(I+1)%Polygon.Num()];
   const bool bCut=A.Value==0 && B.Value==0;
   const bool bExterior=(FMath::IsNearlyEqual(A.P.X,Box.Min.X,.001) && FMath::IsNearlyEqual(B.P.X,Box.Min.X,.001))
    || (FMath::IsNearlyEqual(A.P.X,Box.Max.X,.001) && FMath::IsNearlyEqual(B.P.X,Box.Max.X,.001))
    || (FMath::IsNearlyEqual(A.P.Y,Box.Min.Y,.001) && FMath::IsNearlyEqual(B.P.Y,Box.Min.Y,.001))
    || (FMath::IsNearlyEqual(A.P.Y,Box.Max.Y,.001) && FMath::IsNearlyEqual(B.P.Y,Box.Max.Y,.001));
   if(!bCut && !bExterior) continue;
   Triangle(Mesh,At(A.P,Box.Min.Z),At(B.P,Box.Min.Z),At(B.P,Box.Max.Z),bCut?1:0);
   Triangle(Mesh,At(A.P,Box.Min.Z),At(B.P,Box.Max.Z),At(A.P,Box.Max.Z),bCut?1:0);
   if(bCut) Caps+=2;
  }
 }
 void BoxMesh(FDynamicMesh3& Mesh,const FBox& Box,TFunctionRef<float(FVector2D)> Field,int32& Caps)
 {
  const int32 NX=FMath::Max(1,FMath::CeilToInt((Box.Max.X-Box.Min.X)/TessellationCm));
  const int32 NY=FMath::Max(1,FMath::CeilToInt((Box.Max.Y-Box.Min.Y)/TessellationCm));
  TArray<FPoint> Samples; Samples.SetNum((NX+1)*(NY+1));
  for(int32 Y=0;Y<=NY;++Y) for(int32 X=0;X<=NX;++X)
  {
   auto& S=Samples[Y*(NX+1)+X];
   S.P=FVector2D(FMath::Lerp(Box.Min.X,Box.Max.X,double(X)/NX),FMath::Lerp(Box.Min.Y,Box.Max.Y,double(Y)/NY));
   S.Value=Field(S.P);
  }
  for(int32 Y=0;Y<NY;++Y) for(int32 X=0;X<NX;++X)
  {
   const int32 I=Y*(NX+1)+X;
   auto Full=[&](int32 Column)
   {
    const int32 J=Y*(NX+1)+Column;
    return Samples[J].Value>0 && Samples[J+1].Value>0 && Samples[J+NX+1].Value>0 && Samples[J+NX+2].Value>0;
   };
   // Keep the fine contour samples, but merge interior runs. The cabinet does
   // not need tens of thousands of coplanar triangles rebuilt every frame.
   if(Full(X))
   {
    int32 End=X; while(End+1<NX && Full(End+1)) ++End;
    const FPoint A[]={Samples[I],Samples[Y*(NX+1)+End+1],Samples[(Y+1)*(NX+1)+End+1]};
    const FPoint B[]={Samples[I],Samples[(Y+1)*(NX+1)+End+1],Samples[I+NX+1]};
    ExtrudeTriangle(Mesh,A,Box,Caps); ExtrudeTriangle(Mesh,B,Box,Caps);
    X=End; continue;
   }
   const FPoint A[]={Samples[I],Samples[I+1],Samples[I+NX+2]};
   const FPoint B[]={Samples[I],Samples[I+NX+2],Samples[I+NX+1]};
   ExtrudeTriangle(Mesh,A,Box,Caps); ExtrudeTriangle(Mesh,B,Box,Caps);
  }
 }
 UDynamicMeshComponent* CreateMesh(AActor* Owner,const TCHAR* Name)
 {
  auto* Mesh=NewObject<UDynamicMeshComponent>(Owner,Name,RF_Transient);
  Owner->AddInstanceComponent(Mesh); // Transient only; never part of the saved map.
  Mesh->SetMobility(EComponentMobility::Movable);
  Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetGenerateOverlapEvents(false);
  Mesh->SetCanEverAffectNavigation(false); Mesh->SetCastShadow(false);
  Mesh->SetAffectDynamicIndirectLighting(false); Mesh->SetAffectDistanceFieldLighting(false);
  Mesh->SetVisibleInRayTracing(false); Mesh->SetReceivesDecals(false);
  Mesh->RegisterComponent(); return Mesh;
 }
 UMaterialInterface* Material(const TCHAR* Name)
 {
  return LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Darkwell/Vision/PropLab/%s.%s"),Name,Name));
 }
}

float UDarkwellMode2SolidComponent::AdvanceReveal(float Previous,float Coverage,float DeltaSeconds)
{
 // Visual ramp starts in this frame. No dwell, whole-object threshold or
 // hysteresis. Losing legal coverage immediately closes the rendering gate.
 return Coverage>=FDarkwellEmptyVerification::LegalCoverage
  ? FMath::Min(1.f,Previous+FMath::Max(0.f,DeltaSeconds)/FDarkwellEmptyVerification::FadeSeconds):0.f;
}
float UDarkwellMode2SolidComponent::SampleOpacity(const FDarkwellEmptyVerification& E,const TArray<float>& Values,FVector2D P)
{
 if(Values.Num()!=E.Size.X*E.Size.Y || Values.IsEmpty()) return 0;
 const FVector2D Q=(P-E.Bounds.Min)/E.Bounds.GetSize()*FVector2D(E.Size.X,E.Size.Y)-FVector2D(.5,.5);
 const int32 X=FMath::FloorToInt(Q.X),Y=FMath::FloorToInt(Q.Y);
 auto Sample=[&](int32 I,int32 J){return Values[FMath::Clamp(J,0,E.Size.Y-1)*E.Size.X+FMath::Clamp(I,0,E.Size.X-1)];};
 auto Weights=[](double T,double* W)
 {
  W[0]=FMath::Pow(1-T,3)/6; W[1]=(3*T*T*T-6*T*T+4)/6;
  W[2]=(-3*T*T*T+3*T*T+3*T+1)/6; W[3]=T*T*T/6;
 };
 double WX[4],WY[4]; Weights(Q.X-X,WX); Weights(Q.Y-Y,WY);
 double Value=0;
 for(int32 J=0;J<4;++J) for(int32 I=0;I<4;++I) Value+=WX[I]*WY[J]*Sample(X+I-1,Y+J-1);
 return Value;
}
void UDarkwellMode2SolidComponent::ResetPresentation()
{
 if(LastActual.IsValid()) for(UStaticMeshComponent* Part:LastActual->Memory->GetMemoryPrimitives())
 { Part->SetCastHiddenShadow(false); Part->SetHiddenInGame(false,false); }
 if(BoundFloor.IsValid() && OriginalFloor) BoundFloor->SetMaterial(0,OriginalFloor);
 if(SolidMemory) SolidMemory->SetVisibility(false);
 if(SpatialLive) SpatialLive->SetVisibility(false);
 LastActual.Reset(); BoundFloor.Reset(); OriginalFloor=nullptr;
 Reveal.Reset(); RevealFraction=0; CapTriangles=0; LiveTriangles=0; bEnabled=false;
}
int32 UDarkwellMode2SolidComponent::GetShadowSources() const
{
 int32 Count=0;
 if(bEnabled && LastActual.IsValid()) for(const UStaticMeshComponent* Part:LastActual->Memory->GetMemoryPrimitives()) Count+=Part->CastShadow && Part->bCastHiddenShadow;
 return Count;
}
void UDarkwellMode2SolidComponent::EnforceSource(ADarkwellPropLabFurniture* Actual,AActor* Snapshot)
{
 if(!bEnabled) return;
 if(IsValid(Actual)) for(UStaticMeshComponent* Part:Actual->Memory->GetMemoryPrimitives())
 {
  // The original current geometry is the sole shadow caster throughout the
  // reveal. Neither dynamic display mesh casts any shadow, even when complete.
  Part->SetCastHiddenShadow(true); Part->SetHiddenInGame(true,false);
 }
 if(IsValid(Snapshot)) Snapshot->SetActorHiddenInGame(true);
}
void UDarkwellMode2SolidComponent::Update(float Dt,ADarkwellPropLabFurniture* Actual,AActor* Snapshot,
 bool bLive,const FDarkwellEmptyVerification& Evidence,const TArray<float>& Opacity,UStaticMeshComponent* Floor)
{
 using namespace Darkwell::Mode2Solid;
 if(Darkwell::PropLab::PresentationMode(GetWorld())!=2)
 { if(bEnabled) ResetPresentation(); return; }
 bEnabled=true;
 if(!SolidMemory)
 {
  SolidMemory=CreateMesh(GetOwner(),TEXT("Mode2ClosedMemory"));
  SolidMemory->SetMaterial(0,Material(TEXT("M_ManualMode2Memory")));
  SolidMemory->SetMaterial(1,Material(TEXT("M_ManualMode2Cut")));
  SpatialLive=CreateMesh(GetOwner(),TEXT("Mode2SpatialLive"));
 }
 if(Floor && !BoundFloor.IsValid())
 {
  BoundFloor=Floor; OriginalFloor=Floor->GetMaterial(0);
  if(!LitFloor) LitFloor=Material(TEXT("M_ManualMode2Floor"));
  Floor->SetMaterial(0,LitFloor);
 }
 auto* Fog=GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>();
 if(LastActual.Get()!=Actual || (IsValid(Actual) && Reveal.IsEmpty()) || (!IsValid(Actual) && !Reveal.IsEmpty()))
 {
  LastActual=Actual; Reveal.Reset(); RevealFraction=0;
  if(IsValid(Actual))
  {
   FBox Bounds(ForceInit); for(const UStaticMeshComponent* Part:Actual->Memory->GetMemoryPrimitives()) Bounds+=Part->Bounds.GetBox();
   RevealBounds=FBox2D(FVector2D(Bounds.Min),FVector2D(Bounds.Max));
   RevealSize=FIntPoint(FMath::CeilToInt(RevealBounds.GetSize().X/TessellationCm)+1,FMath::CeilToInt(RevealBounds.GetSize().Y/TessellationCm)+1);
   Reveal.Init(0,RevealSize.X*RevealSize.Y);
   RevealTexture=UTexture2D::CreateTransient(RevealSize.X,RevealSize.Y,PF_G8);
   RevealTexture->SRGB=false; RevealTexture->Filter=TF_Bilinear; RevealTexture->AddressX=TA_Clamp; RevealTexture->AddressY=TA_Clamp;
   RevealTexture->NeverStream=true; RevealTexture->UpdateResource();
   auto* MID=UMaterialInstanceDynamic::Create(Material(TEXT("M_ManualMode2Live")),this);
   MID->SetTextureParameterValue(TEXT("Reveal"),RevealTexture);
   const FVector2D Inv=FVector2D(1,1)/RevealBounds.GetSize();
   MID->SetVectorParameterValue(TEXT("RevealMinInv"),FLinearColor(RevealBounds.Min.X,RevealBounds.Min.Y,Inv.X,Inv.Y));
   SpatialLive->SetMaterial(0,MID); SpatialLive->SetMaterial(1,MID);
  }
 }
 FDynamicMesh3 LiveMesh; LiveMesh.EnableAttributes(); LiveMesh.Attributes()->EnableMaterialID();
 LiveTriangles=0; bool bAnyCoverage=false;
 if(IsValid(Actual) && Fog)
 {
  uint8* Bytes=new uint8[Reveal.Num()]; float Sum=0;
  for(int32 Y=0;Y<RevealSize.Y;++Y) for(int32 X=0;X<RevealSize.X;++X)
  {
   const FVector2D P=RevealBounds.Min+RevealBounds.GetSize()*FVector2D(double(X)/(RevealSize.X-1),double(Y)/(RevealSize.Y-1));
   const int32 I=Y*RevealSize.X+X;
   const float Coverage=Fog->EvaluateLiveCoverageAtWorldPoint(P);
   Reveal[I]=AdvanceReveal(Reveal[I],Coverage,Dt); Sum+=Reveal[I]; bAnyCoverage|=Coverage>=.99f;
   Bytes[I]=uint8(FMath::RoundToInt(Reveal[I]*255));
  }
  RevealFraction=Sum/Reveal.Num();
  auto* Region=new FUpdateTextureRegion2D(0,0,0,0,RevealSize.X,RevealSize.Y);
  RevealTexture->UpdateTextureRegions(0,1,Region,RevealSize.X,1,Bytes,[](uint8* Data,const FUpdateTextureRegion2D* R){delete[] Data;delete R;});
  if(bAnyCoverage) for(const UStaticMeshComponent* Part:Actual->Memory->GetMemoryPrimitives())
   BoxMesh(LiveMesh,Part->Bounds.GetBox(),[Fog](FVector2D P){return Fog->EvaluateLiveCoverageAtWorldPoint(P)-.99f;},LiveTriangles);
 }
 LiveTriangles=LiveMesh.TriangleCount();
 SpatialLive->SetMesh(MoveTemp(LiveMesh)); SpatialLive->SetVisibility(LiveTriangles>0);
 FDynamicMesh3 GhostMesh; GhostMesh.EnableAttributes(); GhostMesh.Attributes()->EnableMaterialID(); CapTriangles=0;
 if(IsValid(Snapshot) && (!IsValid(Actual) || (!bLive && !bAnyCoverage)))
 {
  TInlineComponentArray<UStaticMeshComponent*> Parts(Snapshot);
  // C2 reconstruction removes the 10cm stair corners. The .8 isosurface is
  // deliberately inward: a zero cell contributes at least (23/48)^2 to
  // every point inside itself, so its reconstructed value is <= .7704 even
  // if all neighbours remain. No opaque cut can fill confirmed empty floor.
  // This is a visual edge inset (roughly 4cm on a straight edge), not a change
  // to the 10cm/all-five/.99/.10s authority or .20s finish time.
  for(const UStaticMeshComponent* Part:Parts) BoxMesh(GhostMesh,Part->Bounds.GetBox(),[&](FVector2D P){return SampleOpacity(Evidence,Opacity,P)-.8f;},CapTriangles);
 }
 const bool bGhost=GhostMesh.TriangleCount()>0;
 SolidMemory->SetMesh(MoveTemp(GhostMesh)); SolidMemory->SetVisibility(bGhost);
 EnforceSource(Actual,Snapshot);
}
