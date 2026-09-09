#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "SightWeaveHardCoverage.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/Float16.h"

namespace
{
 void TextureUpload(UTexture2D*& T,int W,int H,EPixelFormat Format,int Bytes,const void* Source)
 {
  if(!T || T->GetSizeX()!=W || T->GetSizeY()!=H)
  {if(T)T->ReleaseResource();T=UTexture2D::CreateTransient(W,H,Format);T->SRGB=false;T->NeverStream=true;T->Filter=TF_Bilinear;T->AddressX=TA_Clamp;T->AddressY=TA_Clamp;T->UpdateResource();}
  auto* Data=new uint8[W*H*Bytes];FMemory::Memcpy(Data,Source,W*H*Bytes);
  T->UpdateTextureRegions(0,1,new FUpdateTextureRegion2D(0,0,0,0,W,H),W*Bytes,Bytes,Data,
   [](uint8* P,const FUpdateTextureRegion2D* R){delete[] P;delete R;});
 }
}
void UDarkwellFogVisualSubsystem::BindHardPresentation(UMaterialInstanceDynamic* M)
{
 if(!M)return;
 HardMaterials.AddUnique(M);
 M->SetScalarParameterValue(TEXT("HardLayerCount"),HardLayerCount);
 if(HardCoverageAtlas)M->SetTextureParameterValue(TEXT("HardCoverageAtlas"),HardCoverageAtlas);
 if(HardHeightBands)M->SetTextureParameterValue(TEXT("HardHeightBands"),HardHeightBands);
}
bool UDarkwellFogVisualSubsystem::UpdateHardPresentation(const FDarkwellFogVisualSourceSnapshot& S)
{
 TRACE_CPUPROFILER_EVENT_SCOPE(Darkwell_HardCoverage_Presentation);
 const double Start=FPlatformTime::Seconds();HardPresentationUs=0;HardPresentationQueries=0;
 HardPresentationPhases=FVector::ZeroVector;
 if(!S.HardAuthority)
 {HardLayerCount=0;for(auto M:HardMaterials)if(M.IsValid())M->SetScalarParameterValue(TEXT("HardLayerCount"),0);return true;}
 const auto& Cuts=S.HardAuthority->HeightCuts();
 const int32 N=2*Cuts.Num()+1,W=Mapping.TextureExtent.X,H=Mapping.TextureExtent.Y;
 // Explicit capacity failure; never wrap a height band or reuse another band's texture.
 if(W<=0 || H<=0 || int64(H+2)*N>16384){UE_LOG(LogTemp,Error,TEXT("Hard coverage height atlas exceeds device extent"));return false;}
 TArray<FVector4f> Bands;Bands.SetNum(N);
 TArray<FFloat16> Pixels;Pixels.SetNumZeroed(W*(H+2)*N);
 TMap<const FSightWeaveHardCoverage*,int32> RenderedPlanes;
 for(int Layer=0;Layer<N;++Layer)
 {
  const int I=Layer/2;double Z;
  if(Layer%2){Z=Cuts[I];Bands[Layer]=FVector4f(Z,Z,1,0);}
  else {const double Min=I?Cuts[I-1]:-double(FLT_MAX),Max=I<Cuts.Num()?Cuts[I]:double(FLT_MAX);
   Z=I==0?Max-1:I==Cuts.Num()?Min+1:(Min+Max)*.5;Bands[Layer]=FVector4f(Min,Max,0,0);}
  const double PrepareStart=FPlatformTime::Seconds();
  auto Plane=S.HardAuthority->AtHeight(Z);
  HardPresentationPhases.X+=(FPlatformTime::Seconds()-PrepareStart)*1e6;
  if(const auto* Existing=RenderedPlanes.Find(&Plane.Get()))
  {FMemory::Memcpy(&Pixels[Layer*(H+2)*W],&Pixels[*Existing*(H+2)*W],(H+2)*W*sizeof(FFloat16));continue;}
  RenderedPlanes.Add(&Plane.Get(),Layer);
  TArray<float> Area;
  const double RasterStart=FPlatformTime::Seconds();
  Plane->RasterizeArea(FBox2D(Mapping.WorldMin,Mapping.WorldMin+FVector2D(W,H)*Mapping.CentimetersPerTexel),FIntPoint(W,H),Area);
  HardPresentationPhases.Y+=(FPlatformTime::Seconds()-RasterStart)*1e6;
  for(int Y=0;Y<H;++Y)for(int X=0;X<W;++X)Pixels[(Layer*(H+2)+1+Y)*W+X]=FFloat16(Area[Y*W+X]);  FMemory::Memcpy(&Pixels[Layer*(H+2)*W],&Pixels[(Layer*(H+2)+1)*W],W*sizeof(FFloat16));
  FMemory::Memcpy(&Pixels[(Layer*(H+2)+H+1)*W],&Pixels[(Layer*(H+2)+H)*W],W*sizeof(FFloat16));
 }
 const double UploadStart=FPlatformTime::Seconds();
 UTexture2D* Atlas=HardCoverageAtlas;TextureUpload(Atlas,W,(H+2)*N,PF_R16F,sizeof(FFloat16),Pixels.GetData());HardCoverageAtlas=Atlas;
 UTexture2D* Meta=HardHeightBands;TextureUpload(Meta,N,1,PF_A32B32G32R32F,sizeof(FVector4f),Bands.GetData());HardHeightBands=Meta;
 HardLayerCount=N;
 HardMaterials.RemoveAll([](const auto& M){return !M.IsValid();});
 // Binding may add to the registry, so iterate a stable copy.
 const auto Materials=HardMaterials;for(auto M:Materials)BindHardPresentation(M.Get());
 BindHardPresentation(CoverageMaterial);CoverageMaterial->SetScalarParameterValue(TEXT("HardQueryHeight"),S.HardHeight);
 for(const auto& P:RenderedPlanes)HardPresentationQueries+=P.Key->ExactQueries;
 HardPresentationUs=(FPlatformTime::Seconds()-Start)*1e6;
 HardPresentationPhases.Z=(FPlatformTime::Seconds()-UploadStart)*1e6;
 return true;
}
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
bool UDarkwellFogVisualSubsystem::ReadHardPresentationForTesting(double Height,TArray<FLinearColor>& Pixels)
{
 if(!CoverageMaterial || HardLayerCount==0)return false;
 auto* Target=NewObject<UTextureRenderTarget2D>(this);
 Target->RenderTargetFormat=RTF_R16f;Target->InitAutoFormat(Mapping.TextureExtent.X,Mapping.TextureExtent.Y);Target->UpdateResourceImmediate(true);
 CoverageMaterial->SetScalarParameterValue(TEXT("HardQueryHeight"),Height);
 UKismetRenderingLibrary::DrawMaterialToRenderTarget(GetWorld(),Target,CoverageMaterial);
 FlushRenderingCommands();
 const bool Result=Target->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(Pixels,FReadSurfaceDataFlags(RCM_MinMax));
 Target->ReleaseResource();
 CoverageMaterial->SetScalarParameterValue(TEXT("HardQueryHeight"),LastSource.HardHeight);
 return Result;
}
#endif
