#include "VisionPresentation/DarkwellObjectMemoryScene.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Components/DynamicMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SceneView.h"
#include "ConvexVolume.h"
#include "HAL/IConsoleManager.h"

namespace
{
 TAutoConsoleVariable<int32> CVarHistoryResidency(TEXT("r.Darkwell.ObjectMemory.HistoryResidency"),0,
  TEXT("0: eager oracle. 1: conservative old-history camera demand. No evidence or display delay."));
 constexpr double CapturePinSeconds=5.0, OutsideHoldSeconds=1.0;
 constexpr float PrefetchMargin=500.f, RetainMargin=1000.f, NearRadius=1000.f;
}

void ADarkwellObjectMemoryScene::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 const bool Enabled=CVarHistoryResidency.GetValueOnGameThread()==1;
 if(!Enabled && !bResidencyWasEnabled) return;
 FConvexVolume Frustum;
 FVector Camera=FVector::ZeroVector;
 bool Valid=false;
 // PostUpdateWork is after UWorld's UpdateCameraManager. Use the actual
 // projection (including aspect constraints), not player heading or light cone.
 auto* PC=GetWorld()->GetFirstPlayerController();
 auto* Local=PC?PC->GetLocalPlayer():nullptr;
 FSceneViewProjectionData Projection;
 if(Local && Local->ViewportClient && Local->ViewportClient->Viewport
  && GetWorld()->GetNumPlayerControllers()==1
  && Local->GetProjectionData(Local->ViewportClient->Viewport,Projection))
 {
  Camera=Projection.ViewOrigin;
  const FMatrix VP=Projection.ComputeViewProjectionMatrix();
  if(!Camera.ContainsNaN() && !VP.ContainsNaN())
  {
   GetViewFrustumBounds(Frustum,VP,true);
   Valid=Frustum.Planes.Num()>=4;
  }
 }
 ApplyPresentationDemand(Valid?&Frustum:nullptr,Camera,GetWorld()->GetTimeSeconds(),Enabled);
}

void ADarkwellObjectMemoryScene::ApplyPresentationDemand(
 const FConvexVolume* Frustum,FVector Camera,double Now,bool bEnabled)
{
 check(IsInGameThread());
 const double Start=FPlatformTime::Seconds();
 bResidencyWasEnabled=bEnabled;
 Residency.Needed=Residency.Missing=0;
 for(auto& Pair:Tracked)
 {
  auto& Prop=Pair.Value;
  for(auto& Record:Prop.History.GetMutableRecords())
  {
   auto* V=Prop.Visuals.Find(Record.Epoch);
   if(!V || Record.bCurrentObservedLocation || V->bPresentationRetired || !Record.FineHistory.IsInitialized()) continue;
   // A0 manual diagnostics are independent; mode changes never consume their tickets.
   if(V->bRenderResourcesReleased && !V->bAutoResidencyReleased) continue;
   FBox Bounds(ForceInit);
   for(const auto& Box:V->PartBounds) Bounds+=Box;
   // Cap is part of conservative contribution bounds, even without a component.
   for(const auto& Q:V->CapQuads) {Bounds+=Q.A;Bounds+=Q.B;Bounds+=Q.C;Bounds+=Q.D;}
   const bool Valid=Bounds.IsValid && !Bounds.Min.ContainsNaN() && !Bounds.Max.ContainsNaN();
   const FVector Center=Valid?Bounds.GetCenter():Camera;
   const float Radius=Valid?Bounds.GetExtent().Size():0.f;
   const bool Pinned=V->LastCaptureTime<0 || Now-V->LastCaptureTime<CapturePinSeconds;
   const bool Near=FVector::Dist(Camera,Center)<=Radius+NearRadius;
   const bool Needed=!bEnabled || !Frustum || !Valid || Pinned || Near
    || Frustum->IntersectSphere(Center,Radius+PrefetchMargin);
   const bool Retain=Needed || Frustum->IntersectSphere(Center,Radius+RetainMargin);
   if(Retain) V->LastDemandTime=Now;
   if(Needed) ++Residency.Needed;
   if(Needed && V->bAutoResidencyReleased)
   {
    const double RebuildStart=FPlatformTime::Seconds();
    const auto Before=RuntimeFrame;
    if(RebuildHistoricalPresentation(Prop,Record,*V)) ++Residency.Rebuilds;
    else ++Residency.Failures;
    Residency.RebuildUploads+=RuntimeFrame.GpuTextureUploads-Before.GpuTextureUploads;
    Residency.RebuildCaps+=RuntimeFrame.CapMeshRebuilds-Before.CapMeshRebuilds;
    Residency.RebuildGeometryTests+=RuntimeFrame.PrimitiveGeometryTests-Before.PrimitiveGeometryTests;
    Residency.MaxRebuildMs=FMath::Max(Residency.MaxRebuildMs,(FPlatformTime::Seconds()-RebuildStart)*1000.0);
   }
   if(Needed && !V->Render.Proxy.IsValid()) ++Residency.Missing;
   if(V->bAutoResidencyReleased) bResidencyWasEnabled=true; // retry failed mode-0 restoration
   if(!bEnabled || Retain || V->bRenderResourcesReleased || Now-V->LastDemandTime<OutsideHoldSeconds) continue;
   // Only persistent captured assets can be automatically evicted. Unknown or
   // transient content remains resident; no knowledge is inferred from residency.
   bool Rebuildable=!Record.Primitives.IsEmpty();
   for(const auto& P:Record.Primitives)
    Rebuildable &= P.Mesh.IsValid() && P.Mesh.Get()->IsAsset() && !P.Mesh.Get()->HasAnyFlags(RF_Transient);
   if(!Rebuildable) continue;
   ReleaseRenderResources(*V);
   V->bRenderResourcesReleased=V->bAutoResidencyReleased=true;
   V->PresentationRequestSerial=++NextPresentationRequestSerial;
   ++Residency.Evictions;
   Prop.bDiagnosticsDirty=true;
  }
 }
 Residency.FrameMs=(FPlatformTime::Seconds()-Start)*1000.0;
 Residency.MaxFrameMs=FMath::Max(Residency.MaxFrameMs,Residency.FrameMs);
}

FString ADarkwellObjectMemoryScene::GetPresentationResidencyTelemetry() const
{
 uint64 N=0,K=0,Proxies=0,Textures=0,Materials=0,Caps=0,Bytes=0;
 for(const auto& Pair:Tracked) for(const auto& R:Pair.Value.History.GetRecords())
 {
  if(R.bCurrentObservedLocation) continue;
  ++N;
  const auto* V=Pair.Value.Visuals.Find(R.Epoch); if(!V) continue;
  if(V->Render.Proxy.IsValid()) {++Proxies;++K;}
  if(auto* T=V->Render.Texture.Get()) {++Textures;Bytes+=uint64(T->GetSizeX())*T->GetSizeY()*8;}
  Caps+=V->Render.Cap.IsValid();
  for(const auto& M:V->Render.Materials) Materials+=M.IsValid();
 }
 return FString::Printf(TEXT("{\"n\":%llu,\"k\":%llu,\"proxies\":%llu,\"textures\":%llu,\"mids\":%llu,\"caps\":%llu,\"texture_payload_bytes\":%llu,\"evictions\":%llu,\"rebuilds\":%llu,\"failures\":%llu,\"needed\":%llu,\"missing\":%llu,\"rebuild_uploads\":%llu,\"rebuild_caps\":%llu,\"rebuild_geometry_tests\":%llu,\"frame_ms\":%.6f,\"max_frame_ms\":%.6f,\"max_rebuild_ms\":%.6f}"),
 N,K,Proxies,Textures,Materials,Caps,Bytes,Residency.Evictions,Residency.Rebuilds,Residency.Failures,Residency.Needed,Residency.Missing,
 Residency.RebuildUploads,Residency.RebuildCaps,Residency.RebuildGeometryTests,Residency.FrameMs,Residency.MaxFrameMs,Residency.MaxRebuildMs);
}
