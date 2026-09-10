#include "VisionPresentation/DarkwellSurfaceKnowledgeSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SightWeaveWorldSubsystem.h"

bool UDarkwellSurfaceKnowledgeSubsystem::RegisterFixedBox(UStaticMeshComponent* Mesh,FName Id,FLinearColor Tint,bool StaticDomain,float Span,uint32 Version)
{
 if(!Mesh || Mesh->GetWorld()!=GetWorld() || !Mesh->GetStaticMesh() || Id.IsNone() || Version==0 || Domains.Num()>=128
  || Mesh->GetStaticMesh()->GetPathName()!=TEXT("/Engine/BasicShapes/Cube.Cube")
  || Mesh->GetComponentTransform().GetScale3D().GetMin()<=0
  || Domains.ContainsByPredicate([&](const auto& D){return D.Id==Id || D.Mesh==Mesh;}))return false;
 auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Darkwell/Vision/SurfaceKnowledge/M_SurfaceKnowledgeV1.M_SurfaceKnowledgeV1"));if(!Parent)return false;
 auto& D=Domains.AddDefaulted_GetRef();D.Id=Id;D.Mesh=Mesh;D.RegisteredTransform=Mesh->GetComponentTransform();D.bStatic=StaticDomain;D.WholeSpan=Span;
 D.ContentVersion=Version;
 D.Material=UMaterialInstanceDynamic::Create(Parent,this);Resources.Add(D.Material);
 D.Material->SetVectorParameterValue(TEXT("SurfaceTint"),Tint);D.Material->SetScalarParameterValue(TEXT("SurfaceReady"),0);
 for(int I=0;I<Mesh->GetNumMaterials();++I){auto* Original=Mesh->GetMaterial(I);D.OriginalMaterials.Add(Original);Resources.AddUnique(Original);Mesh->SetMaterial(I,D.Material);}
 return true;
}
void UDarkwellSurfaceKnowledgeSubsystem::Publish(FDomain& D)
{
 if(!D.Knowledge.bDirty || !D.Atlas)return;
 for(auto& P:D.Knowledge.Faces)
 {
  const auto Local=P.Dirty;if(Local.Width()==0)continue;const bool Transpose=P.Size.Y>P.Size.X;
  const FIntRect Rect=Transpose?FIntRect(Local.Min.Y,Local.Min.X,Local.Max.Y,Local.Max.X):Local;
  const int Bytes=Rect.Area()*sizeof(FColor);auto* Data=new uint8[Bytes];auto* Colors=reinterpret_cast<FColor*>(Data);
  for(int Y=0;Y<Rect.Height();++Y)for(int X=0;X<Rect.Width();++X)
  {const int PX=Rect.Min.X+X,PY=Rect.Min.Y+Y;const int I=Transpose?PX*P.Size.X+PY:PY*P.Size.X+PX;Colors[Y*Rect.Width()+X]=FColor(P.Live[I]?255:0,P.Known[I]?255:0,P.Hidden[I]?255:0,255);}
  D.Atlas->UpdateTextureRegions(0,1,new FUpdateTextureRegion2D(Rect.Min.X,Rect.Min.Y+P.Offset,0,0,Rect.Width(),Rect.Height()),Rect.Width()*sizeof(FColor),sizeof(FColor),Data,
   [](uint8* Bytes,const FUpdateTextureRegion2D* R){delete[] Bytes;delete R;});
  UploadBytes+=Bytes;P.Dirty=FIntRect(0,0,0,0);
 }
 D.Knowledge.bDirty=false;
}
bool UDarkwellSurfaceKnowledgeSubsystem::OwnsMesh(const UStaticMeshComponent* Mesh) const
{return Domains.ContainsByPredicate([&](const auto& D){return D.Mesh.Get()==Mesh;});}
void UDarkwellSurfaceKnowledgeSubsystem::Tick(float)
{
 const double Start=FPlatformTime::Seconds();ExactSamples=Proofs=UploadBytes=0;
 auto* R=GetWorld()->GetSubsystem<USightWeaveWorldSubsystem>();FSightWeaveMemoryScopeKey Scope;
 if(!R || !R->GetExplorationMemoryScope(Scope)){for(auto& D:Domains)D.Material->SetScalarParameterValue(TEXT("SurfaceReady"),0);return;}
 // Register every new domain BEFORE sampling any domain from the final snapshot.
 for(auto& D:Domains)
 {
  auto* M=D.Mesh.Get();
  if(!M || M->IsBeingDestroyed())
  {if(D.bReady){R->UnregisterSurfaceBox(D.Id);D.bReady=false;D.Material->SetScalarParameterValue(TEXT("SurfaceReady"),0);}continue;}
  if(D.bViolated)continue;
  if(!M->GetStaticMesh() || M->GetStaticMesh()->GetPathName()!=TEXT("/Engine/BasicShapes/Cube.Cube") || !M->GetComponentTransform().Equals(D.RegisteredTransform,0))
  {D.bViolated=true;R->UnregisterSurfaceBox(D.Id);D.Material->SetScalarParameterValue(TEXT("SurfaceReady"),0);UE_LOG(LogTemp,Error,TEXT("Fixed Surface Knowledge domain moved: %s; immutable declaration violated, knowledge retained, presentation disabled"),*D.Id.ToString());continue;}
  if(D.bReady)continue;
  FSightWeaveSurfaceBox Box;Box.Id=D.Id;Box.Floor=Scope.FloorId;
  const auto Bounds=M->GetStaticMesh()->GetBoundingBox();Box.Pose=FTransform(D.RegisteredTransform.GetRotation(),D.RegisteredTransform.TransformPosition(Bounds.GetCenter()));Box.HalfExtent=Bounds.GetExtent()*D.RegisteredTransform.GetScale3D();
  if(!D.Knowledge.Initialize(Box,Scope.KnowledgeOwnerId,D.ContentVersion))
  {D.bViolated=true;UE_LOG(LogTemp,Error,TEXT("Surface domain registration/capacity rejected: %s"),*D.Id.ToString());continue;}
  int64 Resident=0;for(const auto& Existing:Domains)if(Existing.bReady)Resident+=int64(Existing.Knowledge.AtlasSize.X)*Existing.Knowledge.AtlasSize.Y;
  if(Resident+int64(D.Knowledge.AtlasSize.X)*D.Knowledge.AtlasSize.Y>32*1024*1024 || !R->RegisterSurfaceBox(Box,this))
  {D.bViolated=true;UE_LOG(LogTemp,Error,TEXT("Surface domain GPU budget/receiver rejected: %s"),*D.Id.ToString());continue;}
  const auto S=D.Knowledge.AtlasSize;D.Atlas=UTexture2D::CreateTransient(S.X,S.Y,PF_B8G8R8A8);Resources.Add(D.Atlas);
  D.Atlas->SRGB=false;D.Atlas->Filter=TF_Nearest;D.Atlas->AddressX=D.Atlas->AddressY=TA_Clamp;
  auto& Bulk=D.Atlas->GetPlatformData()->Mips[0].BulkData;auto* Initial=static_cast<FColor*>(Bulk.Lock(LOCK_READ_WRITE));for(int I=0;I<S.X*S.Y;++I)Initial[I]=FColor::Black;Bulk.Unlock();D.Atlas->UpdateResource();
  D.Material->SetTextureParameterValue(TEXT("SurfaceAtlas"),D.Atlas);
  D.Material->SetVectorParameterValue(TEXT("SurfaceOrigin"),FLinearColor(Box.Pose.GetLocation()));
  D.Material->SetVectorParameterValue(TEXT("SurfaceExtent"),FLinearColor(Box.HalfExtent));
  for(int A=0;A<3;++A){FVector Axis=FVector::ZeroVector;Axis[A]=1;D.Material->SetVectorParameterValue(*FString::Printf(TEXT("SurfaceAxis%d"),A),FLinearColor(Box.Pose.TransformVector(Axis)));}
  for(int F=0;F<6;++F){const auto& P=D.Knowledge.Faces[F];D.Material->SetVectorParameterValue(*FString::Printf(TEXT("SurfaceFace%d"),F),FLinearColor(P.Size.X,P.Size.Y,P.Offset,P.Size.Y>P.Size.X?1:0));}
  D.Knowledge.SetBlock(D.bStatic?StaticBlock:ObjectBlock,true);D.Scope=Scope;D.bReady=true;
 }
 for(auto& D:Domains)if(D.bReady && !D.bViolated && D.Mesh.IsValid())
 {
  const bool SameScope=D.Scope.IsEquivalentTo(Scope);
  D.Material->SetScalarParameterValue(TEXT("SurfaceReady"),SameScope?1:0);if(!SameScope)continue;
  D.Knowledge.Observe(*R);ExactSamples+=D.Knowledge.ExactSamples;Proofs+=D.Knowledge.Proofs;
  if(!D.bRecognized && D.WholeSpan>0 && D.Knowledge.LiveSpanCm()>=D.WholeSpan)D.bRecognized=true;
  Publish(D);
 }
 UpdateUs=(FPlatformTime::Seconds()-Start)*1.e6;
}
void UDarkwellSurfaceKnowledgeSubsystem::Clear(const FBox2D& Region,bool StaticDomain)
{for(auto& D:Domains)if(D.bStatic==StaticDomain && Find(D.Id)){D.Knowledge.Clear(Region);const auto B=FBox(-D.Knowledge.Box.HalfExtent,D.Knowledge.Box.HalfExtent).TransformBy(D.Knowledge.Box.Pose);if(Region.Intersect(FBox2D(FVector2D(B.Min),FVector2D(B.Max))))D.bRecognized=false;Publish(D);}}
void UDarkwellSurfaceKnowledgeSubsystem::SetBlock(const FBox2D& Region,bool Enabled,bool StaticDomain)
{(StaticDomain?StaticBlock:ObjectBlock)=Enabled?Region:FBox2D(ForceInit);for(auto& D:Domains)if(D.bStatic==StaticDomain){D.Knowledge.SetBlock(Region,Enabled);if(Find(D.Id)){D.Knowledge.Observe(*GetWorld()->GetSubsystem<USightWeaveWorldSubsystem>());Publish(D);}}}
void UDarkwellSurfaceKnowledgeSubsystem::UnregisterDomain(FName Id)
{
 const int I=Domains.IndexOfByPredicate([&](const auto& D){return D.Id==Id;});if(I==INDEX_NONE)return;auto& D=Domains[I];
 if(auto* R=GetWorld()->GetSubsystem<USightWeaveWorldSubsystem>())R->UnregisterSurfaceBox(Id);
 if(auto* M=D.Mesh.Get())for(int J=0;J<D.OriginalMaterials.Num();++J)M->SetMaterial(J,D.OriginalMaterials[J]);
 if(D.Atlas){D.Atlas->ReleaseResource();Resources.Remove(D.Atlas);}Resources.Remove(D.Material);Domains.RemoveAt(I);
}
bool UDarkwellSurfaceKnowledgeSubsystem::SaveDomain(FName Id,TArray<uint8>& Out) const
{
 const auto* D=Domains.FindByPredicate([&](const auto& P){return P.Id==Id;});if(!D || !Find(Id))return false;
 D->Knowledge.Save(Out);Out.Insert(D->bRecognized?1:0,0);return true;
}
bool UDarkwellSurfaceKnowledgeSubsystem::RestoreDomain(FName Id,const TArray<uint8>& Data)
{
 auto* D=Domains.FindByPredicate([&](const auto& P){return P.Id==Id;});if(!D || !Find(Id) || Data.IsEmpty() || Data[0]>1)return false;
 auto Bytes=Data;Bytes.RemoveAt(0);if(!D->Knowledge.Load(Bytes))return false;D->bRecognized=Data[0]!=0 && D->WholeSpan>0;Publish(*D);return true;
}
const FDarkwellSurfaceKnowledge* UDarkwellSurfaceKnowledgeSubsystem::Find(FName Id) const
{const auto* D=Domains.FindByPredicate([&](const auto& P){return P.Id==Id;});FSightWeaveMemoryScopeKey Scope;const auto* R=GetWorld()->GetSubsystem<USightWeaveWorldSubsystem>();return D && D->bReady && !D->bViolated && R && R->GetExplorationMemoryScope(Scope) && D->Scope.IsEquivalentTo(Scope)?&D->Knowledge:nullptr;}
bool UDarkwellSurfaceKnowledgeSubsystem::IsWholeRecognized(FName Id) const
{const auto* D=Domains.FindByPredicate([&](const auto& P){return P.Id==Id;});return D && Find(Id) && D->bRecognized;}
UTexture2D* UDarkwellSurfaceKnowledgeSubsystem::GetAtlas(FName Id) const
{const auto* D=Domains.FindByPredicate([&](const auto& P){return P.Id==Id;});return D?D->Atlas:nullptr;}
FString UDarkwellSurfaceKnowledgeSubsystem::GetTelemetry() const
{
 int64 Cells=0,AtlasBytes=0;int Ready=0;uint64 Unresolved=0;
 for(const auto& D:Domains)if(D.bReady){++Ready;for(const auto& F:D.Knowledge.Faces)Cells+=F.Known.Num();AtlasBytes+=int64(D.Knowledge.AtlasSize.X)*D.Knowledge.AtlasSize.Y*4;Unresolved+=D.Knowledge.UnresolvedCells;}
 return FString::Printf(TEXT("{\"domains\":%d,\"ready\":%d,\"cells\":%lld,\"atlas_bytes\":%lld,\"authority_queries\":%llu,\"certified_regions\":%llu,\"unresolved_cells\":%llu,\"upload_bytes\":%llu,\"update_us\":%.3f}"),Domains.Num(),Ready,Cells,AtlasBytes,ExactSamples,Proofs,Unresolved,UploadBytes,UpdateUs);
}
void UDarkwellSurfaceKnowledgeSubsystem::Deinitialize()
{if(auto* R=GetWorld()->GetSubsystem<USightWeaveWorldSubsystem>())R->UnregisterAllForOwner(this);for(auto& D:Domains)if(D.Atlas)D.Atlas->ReleaseResource();Domains.Reset();Resources.Reset();Super::Deinitialize();}
