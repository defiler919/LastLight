#include "VisionPresentation/DarkwellSurfaceKnowledgeSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SightWeaveWorldSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Async/ParallelFor.h"

bool UDarkwellSurfaceKnowledgeSubsystem::RegisterFixedBox(UStaticMeshComponent* Mesh,FName Id,FLinearColor Tint,bool StaticDomain,float Span,uint32 Version)
{
 if(!Mesh || Mesh->GetWorld()!=GetWorld() || !Mesh->GetStaticMesh() || Id.IsNone() || Version==0 || !FMath::IsFinite(Span) || Span<-1 || Domains.Num()>=128
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
  const int Batches=Rect.Area()>=65536?4:1;
  ParallelFor(Batches,[&](int Batch)
  {
   for(int Y=Rect.Height()*Batch/Batches;Y<Rect.Height()*(Batch+1)/Batches;++Y)for(int X=0;X<Rect.Width();++X)
   {const int PX=Rect.Min.X+X,PY=Rect.Min.Y+Y;const int I=Transpose?PX*P.Size.X+PY:PY*P.Size.X+PX;Colors[Y*Rect.Width()+X]=FColor(P.Live[I]?255:0,P.Known[I]?255:0,P.Hidden[I]?255:0,255);}
  },EParallelForFlags::Unbalanced);
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
 ObserveUs=RecognitionUs=PublishUs=0;
 // A dead component cannot be re-observed. Release its domain and render
 // resources instead of consuming the fixed-domain budget until world exit.
 for(int I=Domains.Num()-1;I>=0;--I)
  if(!Domains[I].Mesh.IsValid() || Domains[I].Mesh->IsBeingDestroyed())UnregisterDomain(Domains[I].Id);
 auto* R=GetWorld()->GetSubsystem<USightWeaveWorldSubsystem>();FSightWeaveMemoryScopeKey Scope;
 if(!R || !R->GetExplorationMemoryScope(Scope)){for(auto& D:Domains)D.Material->SetScalarParameterValue(TEXT("SurfaceReady"),0);UpdateUs=(FPlatformTime::Seconds()-Start)*1.e6;return;}
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
 uint64 ComparedCells=0;
 for(auto& D:Domains)if(D.bReady && !D.bViolated && D.Mesh.IsValid())
 {
  const bool SameScope=D.Scope.IsEquivalentTo(Scope);
  D.Material->SetScalarParameterValue(TEXT("SurfaceReady"),SameScope?1:0);if(!SameScope)continue;
  const double ObserveStart=FPlatformTime::Seconds();
#if WITH_DEV_AUTOMATION_TESTS
  static const bool GameParity=FParse::Param(FCommandLine::Get(),TEXT("SurfaceGameParity"));
  TUniquePtr<FDarkwellSurfaceKnowledge> Reference;if(GameParity)Reference=MakeUnique<FDarkwellSurfaceKnowledge>(D.Knowledge);
#endif
  D.Knowledge.Observe(*R);ExactSamples+=D.Knowledge.ExactSamples;Proofs+=D.Knowledge.Proofs;
#if WITH_DEV_AUTOMATION_TESTS
  if(Reference)
  {
   static auto* Mode=IConsoleManager::Get().FindConsoleVariable(TEXT("SightWeave.Surface.PreparedProofs"));const int Saved=Mode->GetInt();Mode->Set(0,ECVF_SetByCode);Reference->Observe(*R);Mode->Set(Saved,ECVF_SetByCode);
   for(int F=0;F<6;++F)
   {
    ComparedCells+=Reference->Faces[F].Known.Num();
    if(Reference->Faces[F].Known!=D.Knowledge.Faces[F].Known || Reference->Faces[F].Live!=D.Knowledge.Faces[F].Live || Reference->Faces[F].Hidden!=D.Knowledge.Faces[F].Hidden)
    {
     UE_LOG(LogTemp,Error,TEXT("SURFACE_GAME_PARITY_MISMATCH domain=%s face=%d"),*D.Id.ToString(),F);
     for(int I=0;I<Reference->Faces[F].Known.Num();++I)if(Reference->Faces[F].Live[I]!=D.Knowledge.Faces[F].Live[I])
     {
      const auto Size=Reference->Faces[F].Size;const int X=I%Size.X,Y=I/Size.X;
      const FBox2D UV(FVector2D(double(X)/Size.X,double(Y)/Size.Y)*2-FVector2D(1),FVector2D(double(X+1)/Size.X,double(Y+1)/Size.Y)*2-FVector2D(1));
      bool A=false,B=false;Mode->Set(0,ECVF_SetByCode);const bool UA=R->TrySurfaceRegion(D.Scope.KnowledgeOwnerId,D.Id,ESightWeaveBoxFace(F),UV,A);Mode->Set(Saved,ECVF_SetByCode);const bool UB=R->TrySurfaceRegion(D.Scope.KnowledgeOwnerId,D.Id,ESightWeaveBoxFace(F),UV,B);
      UE_LOG(LogTemp,Display,TEXT("SURFACE_PARITY_DETAIL cell=%d,%d ref=%d actual=%d reference_cell=%d/%d prepared_cell=%d/%d"),X,Y,int(Reference->Faces[F].Live[I]),int(D.Knowledge.Faces[F].Live[I]),UA,A,UB,B);break;
     }
    }
   }
  }
#endif
  const double RecognitionStart=FPlatformTime::Seconds();ObserveUs+=(RecognitionStart-ObserveStart)*1.e6;
  static const auto* Profile=IConsoleManager::Get().FindConsoleVariable(TEXT("SightWeave.Surface.Profile"));
  if(Profile && Profile->GetInt() && (RecognitionStart-ObserveStart)>.001)
   UE_LOG(LogTemp,Display,TEXT("SURFACE_DOMAIN_PROFILE id=%s observe_us=%.3f queries=%llu regions=%llu unresolved=%llu"),*D.Id.ToString(),(RecognitionStart-ObserveStart)*1.e6,D.Knowledge.ExactSamples,D.Knowledge.Proofs,D.Knowledge.UnresolvedCells);
  if(!D.bRecognized && D.WholeSpan>=0)
  {
   const double Observed=D.Knowledge.LiveSpanCm(),Threshold=FMath::Min(double(D.WholeSpan),2*D.Knowledge.Box.HalfExtent.GetMax());
   if(Observed>0 && Observed>=Threshold)D.bRecognized=true;
  }
  const double PublishStart=FPlatformTime::Seconds();RecognitionUs+=(PublishStart-RecognitionStart)*1.e6;
  Publish(D);
  PublishUs+=(FPlatformTime::Seconds()-PublishStart)*1.e6;
 }
 UpdateUs=(FPlatformTime::Seconds()-Start)*1.e6;
 if(ComparedCells)UE_LOG(LogTemp,Display,TEXT("SURFACE_GAME_PARITY compared_cells=%llu; diagnostic reference work invalidates performance timings"),ComparedCells);
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
 // Original materials are shared references; reconstruct the small owner list
 // so repeated register/reset does not retain every historical material.
 Resources.Reset();for(const auto& Remaining:Domains)
 {Resources.AddUnique(Remaining.Material);if(Remaining.Atlas)Resources.AddUnique(Remaining.Atlas);for(auto* Original:Remaining.OriginalMaterials)Resources.AddUnique(Original);}
}
bool UDarkwellSurfaceKnowledgeSubsystem::SaveDomain(FName Id,TArray<uint8>& Out) const
{
 const auto* D=Domains.FindByPredicate([&](const auto& P){return P.Id==Id;});if(!D || !Find(Id))return false;
 D->Knowledge.Save(Out);Out.Insert(D->bRecognized?1:0,0);return true;
}
bool UDarkwellSurfaceKnowledgeSubsystem::RestoreDomain(FName Id,const TArray<uint8>& Data)
{
 auto* D=Domains.FindByPredicate([&](const auto& P){return P.Id==Id;});if(!D || !Find(Id) || Data.IsEmpty() || Data[0]>1)return false;
 auto Bytes=Data;Bytes.RemoveAt(0);if(!D->Knowledge.Load(Bytes))return false;D->bRecognized=Data[0]!=0 && D->WholeSpan>=0;Publish(*D);return true;
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
 return FString::Printf(TEXT("{\"domains\":%d,\"ready\":%d,\"cells\":%lld,\"atlas_bytes\":%lld,\"authority_queries\":%llu,\"certified_regions\":%llu,\"unresolved_cells\":%llu,\"upload_bytes\":%llu,\"update_us\":%.3f,\"observe_us\":%.3f,\"recognition_us\":%.3f,\"publish_us\":%.3f}"),Domains.Num(),Ready,Cells,AtlasBytes,ExactSamples,Proofs,Unresolved,UploadBytes,UpdateUs,ObserveUs,RecognitionUs,PublishUs);
}
void UDarkwellSurfaceKnowledgeSubsystem::Deinitialize()
{while(!Domains.IsEmpty())UnregisterDomain(Domains.Last().Id);Super::Deinitialize();}
