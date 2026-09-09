#include "VisionPresentation/DarkwellStaticEnvironmentSubsystem.h"
#include "Components/MeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Engine/World.h"
#include "SightWeaveWorldSubsystem.h"

namespace
{
 constexpr int Slots=4096,TableWidth=2048;
 uint32 Slot(FIntPoint K){return (uint32(K.X)*73856093u ^ uint32(K.Y)*19349663u)&(Slots-1);}
 UTexture2D* Texture(int X,int Y,EPixelFormat Format)
 {
  auto* T=UTexture2D::CreateTransient(X,Y,Format);check(T);T->SRGB=false;T->Filter=TF_Nearest;T->NeverStream=true;
  T->AddressX=TA_Clamp;T->AddressY=TA_Clamp;
  auto& Data=T->GetPlatformData()->Mips[0].BulkData;FMemory::Memzero(Data.Lock(LOCK_READ_WRITE),Data.GetBulkDataSize());Data.Unlock();T->UpdateResource();return T;
 }
 void Upload(UTexture2D* T,int X,int Y,int W,int H,int Bytes,const void* Data)
 {
  check(T && T->GetResource());auto* Copy=new uint8[W*H*Bytes];FMemory::Memcpy(Copy,Data,W*H*Bytes);
  T->UpdateTextureRegions(0,1,new FUpdateTextureRegion2D(X,Y,0,0,W,H),W*Bytes,Bytes,Copy,
   [](uint8* P,const FUpdateTextureRegion2D* R){delete[] P;delete R;});
 }
}
void UDarkwellStaticEnvironmentSubsystem::EnsureResources()
{
 if(!PageTable){PageTable=Texture(TableWidth,Slots/TableWidth,PF_A32B32G32R32F);PageEntries.SetNumZeroed(Slots);}
 if(!Atlas)Atlas=Texture(PageSide*FDarkwellStaticKnowledge::Side,PageSide*FDarkwellStaticKnowledge::Side,PF_B8G8R8A8);
}
bool UDarkwellStaticEnvironmentSubsystem::RegisterImmutable(UMeshComponent* M,FLinearColor Tint)
{
 if(!M || M->GetWorld()!=GetWorld() || Meshes.Contains(M))return false;
 auto* Parent=LoadObject<UMaterial>(nullptr,TEXT("/Game/Darkwell/Vision/ProjectFog/M_DarkwellStaticKnowledge.M_DarkwellStaticKnowledge"));
 if(!ensureAlwaysMsgf(Parent,TEXT("Static Knowledge material is missing")))return false;
 EnsureResources();
 auto* MID=UMaterialInstanceDynamic::Create(Parent,this);MID->SetVectorParameterValue(TEXT("StaticTint"),Tint);
 MID->SetScalarParameterValue(TEXT("StaticReady"),0);
 for(int I=0;I<M->GetNumMaterials();++I)M->SetMaterial(I,MID);
 M->UpdateBounds();const auto B=M->Bounds.GetBox();Knowledge.Declare(FBox2D(FVector2D(B.Min),FVector2D(B.Max)));
 Materials.Add(MID);Meshes.Add(M);Bind();return true;
}
void UDarkwellStaticEnvironmentSubsystem::Bind()
{
 const auto* Fog=GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>();
 const auto& Map=Fog->GetMapping();
 for(auto M:Materials)
 {
  M->SetTextureParameterValue(TEXT("StaticAtlas"),Atlas);M->SetTextureParameterValue(TEXT("StaticPages"),PageTable);
  M->SetScalarParameterValue(TEXT("StaticPageSide"),PageSide);
  M->SetScalarParameterValue(TEXT("StaticLookupProbes"),LookupProbes);
  M->SetScalarParameterValue(TEXT("StaticBlocked"),Block.bIsValid?1:0);
  M->SetVectorParameterValue(TEXT("StaticBlockBounds"),Block.bIsValid?FLinearColor(Block.Min.X,Block.Min.Y,Block.Max.X,Block.Max.Y):FLinearColor::Black);
  if(Fog->IsActive())
  {
   M->SetTextureParameterValue(TEXT("DarkwellLiveCoverageTexture"),Fog->GetLiveCoverageTexture());
   M->SetVectorParameterValue(TEXT("FogWorldMin"),FLinearColor(Map.WorldMin.X,Map.WorldMin.Y,0,0));
   M->SetVectorParameterValue(TEXT("FogWorldInvExtent"),FLinearColor(Map.InvWorldExtent.X,Map.InvWorldExtent.Y,0,0));
  }
  M->SetScalarParameterValue(TEXT("StaticReady"),Fog->IsActive() && bScopeValid && bPresentationCapacityValid?1:0);
 }
 BoundLive=Fog->GetLiveCoverageTexture();
 bBoundFogActive=Fog->IsActive();
}
void UDarkwellStaticEnvironmentSubsystem::Publish()
{
 TRACE_CPUPROFILER_EVENT_SCOPE(Darkwell_StaticKnowledge_Publish);
 const double Start=FPlatformTime::Seconds();Uploads=0;
 auto Dirty=Knowledge.TakeDirty();if(Dirty.IsEmpty())return;EnsureResources();
 if(!bPresentationCapacityValid)return;
 int Required=Pages.Num();for(auto K:Dirty)Required+=!Pages.Contains(K);
 if(Required>Slots)
 {
  // This slice has bounded GPU residency, not an eviction/streaming policy.
  // Keep CPU authority intact and fail closed explicitly in every build.
  bPresentationCapacityValid=false;Bind();
  UE_LOG(LogTemp,Error,TEXT("Static Knowledge GPU residency exhausted (%d > %d tiles); presentation disabled, CPU knowledge retained"),Required,Slots);
  return;
 }
 bool LookupChanged=false;
 for(auto K:Dirty)if(!Pages.Contains(K))
 {
  const int Page=Pages.Num();Pages.Add(K,Page);uint32 H=Slot(K);int Probe=0;
  for(;Probe<Slots;++Probe,H=(H+1)&(Slots-1))if(PageEntries[H].Z==0)break;
  if(Probe+1>LookupProbes){LookupProbes=Probe+1;LookupChanged=true;}
  PageEntries[H]=FVector4f(K.X,K.Y,Page+1,0);PageTableDirty=true;
 }
 bool Resized=false;
 while(Pages.Num()>PageSide*PageSide){PageSide*=2;Resized=true;}
 if(Resized)
 {
  checkf(PageSide*FDarkwellStaticKnowledge::Side<=8192,TEXT("Static knowledge GPU residency capacity exceeded"));
  Atlas=Texture(PageSide*FDarkwellStaticKnowledge::Side,PageSide*FDarkwellStaticKnowledge::Side,PF_B8G8R8A8);
  for(const auto& P:Pages)Dirty.Add(P.Key);Bind();
 }
 else if(LookupChanged)Bind();
 if(PageTableDirty){Upload(PageTable,0,0,TableWidth,Slots/TableWidth,sizeof(FVector4f),PageEntries.GetData());PageTableDirty=false;}
 TArray<FColor> Pixels;Pixels.SetNumUninitialized(FDarkwellStaticKnowledge::Side*FDarkwellStaticKnowledge::Side);
 for(auto K:Dirty)
 {
  const auto& T=Knowledge.GetTiles().FindChecked(K);for(int I=0;I<Pixels.Num();++I)Pixels[I]=T.Known[I]?FColor::White:FColor::Black;
  const int Page=Pages.FindChecked(K);Upload(Atlas,(Page%PageSide)*FDarkwellStaticKnowledge::Side,(Page/PageSide)*FDarkwellStaticKnowledge::Side,FDarkwellStaticKnowledge::Side,FDarkwellStaticKnowledge::Side,sizeof(FColor),Pixels.GetData());++Uploads;
 }
 PublishUs=(FPlatformTime::Seconds()-Start)*1e6;
}
void UDarkwellStaticEnvironmentSubsystem::UpdateKnowledge()
{
 TRACE_CPUPROFILER_EVENT_SCOPE(Darkwell_StaticKnowledge_Update);
 const double Start=FPlatformTime::Seconds();Uploads=0;PublishUs=0;Knowledge.Stats={};
 if(Materials.IsEmpty())return;
 const auto* Fog=GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>();
 FSightWeaveMemoryScopeKey Current;
 const bool HasCurrent=GetWorld()->GetSubsystem<USightWeaveWorldSubsystem>()->GetExplorationMemoryScope(Current);
 if(HasCurrent && !bHasScope){Scope=Current;bHasScope=true;}
 const bool Valid=HasCurrent && Current.IsEquivalentTo(Scope);
 if(Valid!=bScopeValid){bScopeValid=Valid;LastDraw=MAX_uint64;Bind();}
 if(BoundLive!=Fog->GetLiveCoverageTexture() || bBoundFogActive!=Fog->IsActive())Bind();
 if(!Fog->IsActive() || !bScopeValid)return;
 const auto Revision=Fog->GetDiagnostics().CoverageDrawCount;
 if(LastDraw!=Revision)
 {
  LastDraw=Revision;const auto& Map=Fog->GetMapping();
  const auto R=FDarkwellContinuousVisibilityBuilder::GetCoverageDrawRect(Fog->GetPublishedSource(),Map,2.5f);
  if(R.Width()>0 && R.Height()>0)Knowledge.Observe(Fog->GetPublishedSource(),Fog->GetPublishedSegments(),FBox2D(Map.WorldMin+FVector2D(R.Min)*Map.CentimetersPerTexel,Map.WorldMin+FVector2D(R.Max)*Map.CentimetersPerTexel));
  Publish();
 }
 UpdateUs=(FPlatformTime::Seconds()-Start)*1e6;
}
void UDarkwellStaticEnvironmentSubsystem::ClearMemory(const FBox2D& Region)
{Knowledge.Clear(Region);LastDraw=MAX_uint64;Publish();}
void UDarkwellStaticEnvironmentSubsystem::SetMemoryWriteBlock(const FBox2D& Region,bool Enabled)
{Block=Enabled?Region:FBox2D(ForceInit);Knowledge.SetBlock(Region,Enabled);LastDraw=MAX_uint64;if(!Materials.IsEmpty())Bind();}
FString UDarkwellStaticEnvironmentSubsystem::GetTelemetry() const
{
 const auto& S=Knowledge.Stats;
 return FString::Printf(TEXT("{\"objects\":%d,\"declared_tiles\":%d,\"resident_tiles\":%d,\"candidate_tiles\":%llu,\"touched_tiles\":%llu,\"tested_samples\":%llu,\"queries\":%llu,\"written_samples\":%llu,\"uniform_proofs\":%llu,\"uploads\":%d,\"update_us\":%.3f,\"observe_us\":%.3f,\"publish_us\":%.3f,\"knowledge_bytes\":%d,\"atlas_bytes\":%d}"),Meshes.Num(),Knowledge.DeclaredTiles(),Knowledge.GetTiles().Num(),S.Candidates,S.TouchedTiles,S.TestedSamples,S.Queries,S.WrittenSamples,S.UniformProofs,Uploads,UpdateUs,S.UpdateUs,PublishUs,Knowledge.GetTiles().Num()*FDarkwellStaticKnowledge::Side*FDarkwellStaticKnowledge::Side/8,Atlas?Atlas->GetSizeX()*Atlas->GetSizeY()*4:0);
}
float UDarkwellStaticEnvironmentSubsystem::GetLegalCoverage(FVector2D Point) const
{return bScopeValid?GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>()->QueryLiveCoverageAtWorldPoint(Point).Coverage:0;}
void UDarkwellStaticEnvironmentSubsystem::Deinitialize()
{Materials.Reset();Meshes.Reset();Atlas=nullptr;PageTable=nullptr;Knowledge={};Pages.Reset();PageEntries.Reset();Super::Deinitialize();}
