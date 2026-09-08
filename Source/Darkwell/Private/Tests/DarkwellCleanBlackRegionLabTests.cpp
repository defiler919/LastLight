#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Player/DarkwellCharacter.h"
#include "VisionPresentation/DarkwellCleanBlackRegionLab.h"
#include "VisionPresentation/DarkwellMovingPropLabRoom.h"
#include "VisionPresentation/DarkwellObjectMemoryScene.h"
#include "VisionPresentation/DarkwellBlackRegionTrigger.h"
#include "VisionPresentation/DarkwellMemoryRegionSubsystem.h"
#include "Visibility/SightWeave/DarkwellSightWeaveWorldSubsystem.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"


class FDarkwellCleanLabFrames : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 UWorld* World=nullptr;
 ADarkwellCharacter* Player=nullptr;
 ADarkwellCleanBlackRegionLab* Fixture=nullptr;
 ADarkwellObjectMemoryScene* Scene=nullptr;
 ADarkwellBlackRegionTrigger* Trigger=nullptr;
 UDarkwellSightWeaveWorldSubsystem* Adapter=nullptr;
 USceneCaptureComponent2D* Capture=nullptr;
 UTextureRenderTarget2D* Target=nullptr;
 int32 Frame=0,IdleRecords=0;
 bool bPartialProbe=false;
 float ProbeYaw=-15;
 void Render(const TCHAR* Name=nullptr)
 {
  FAssetCompilingManager::Get().FinishAllCompilation();
  if(GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
  World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
  if(!Name) return;
  for(TActorIterator<AActor> It(World);It;++It)
   if(It->GetName().StartsWith(TEXT("SpatialMemory_BlackLab")))
   {
    FVector Center,Extent; It->GetActorBounds(false,Center,Extent);
    Test->AddInfo(FString::Printf(TEXT("CLEAN_PROXY %s %s pose=%s center=%s extent=%s hidden=%d"),Name,*It->GetName(),*It->GetActorTransform().ToHumanReadableString(),*Center.ToString(),*Extent.ToString(),It->IsHidden()));
    if(It->GetName().Contains(TEXT("BlackLab.Ground")))
    {
     TArray<UStaticMeshComponent*> Parts; It->GetComponents(Parts);
     for(const auto* Part:Parts) Test->TestEqual(TEXT("Floor history preserves its authored layer"),Part->TranslucencySortPriority,-1);
    }
   }
  TArray<FColor> Pixels; Test->TestTrue(TEXT("D3D12 image readback"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels));
  Test->TestTrue(TEXT("Capture contains a rendered scene, not an empty frame"),Pixels.ContainsByPredicate([](FColor C){return C.R>80 || C.G>80 || C.B>80;}));
  const FString Root=FPlatformMisc::GetEnvironmentVariable(TEXT("DARKWELL_UNKNOWN_TEST_OUTPUT"));
  const FString Dir=(Root.IsEmpty()?FPaths::ProjectSavedDir():Root)/TEXT("Captures/CleanBlackLab");
  IFileManager::Get().MakeDirectory(*Dir,true); TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(768,768,Pixels,PNG);
  Test->TestTrue(TEXT("Save real-frame Lab scene"),FFileHelper::SaveArrayToFile(PNG,*(Dir/(FString(Name)+TEXT(".png")))));
  Test->AddInfo(FString::Printf(TEXT("CLEAN_LAB_FRAME %s engine_frame=%llu %s"),Name,GFrameCounter,*Scene->GetStorageTelemetry()));
  if(bPartialProbe)
  {
   const auto& Prop=Scene->Tracked.FindChecked(TEXT("BlackLab.Partial"));
   for(const auto& Record:Prop.History.GetRecords())
   {
    if(!Record.FineHistory.IsInitialized()) continue;
    FString Fine=TEXT("index,capture,geometry,initial,opacity,envelope,state\n");
    const auto& Samples=Record.FineHistory.GetSamples();
    for(int32 I=0;I<Samples.Num();++I)
    {const auto& S=Samples[I];Fine+=FString::Printf(TEXT("%d,%d,%d,%.9g,%.9g,%.9g,%s\n"),I,Record.LastLegalCaptureMask[I]?1:0,Record.GeometryFootprint[I]?1:0,S.InitialRemembered,S.Opacity,S.FrozenAAEnvelope,*S.State.ToString());}
    FFileHelper::SaveStringToFile(Fine,*(Dir/(FString(Name)+TEXT("_fine.csv"))));
    const auto* Visual=Prop.Visuals.Find(Record.Epoch);
    if(FCString::Strcmp(Name,TEXT("probe_left"))==0)
    {
     const auto& Part=Prop.CurrentLive.Parts[0]; const auto Size=Record.FineHistory.GetSize();
     int32 Checked=0,Missing=0;
     for(int32 X=5;X<Part.Local.GetSize().X-5;++X)
     {
      const int32 Row=ProbeYaw<0?0:Part.Local.GetSize().Y-1;
      if(!Part.LastLegalCaptureMask[Row*Part.Local.GetSize().X+X]) continue;
      const auto B=Part.Local.GetBounds();
      const auto L=B.Min+B.GetSize()*FVector2D((X+.5)/Part.Local.GetSize().X,ProbeYaw<0?0.00001:0.99999);
      const auto W=FVector2D(Part.Pose.TransformPosition(FVector(L,0)));
      const auto UV=(W-Record.FineHistory.GetBounds().Min)/Record.FineHistory.GetBounds().GetSize();
      const int32 I=FMath::FloorToInt(UV.Y*Size.Y)*Size.X+FMath::FloorToInt(UV.X*Size.X);
      ++Checked; Missing+=!Record.LastLegalCaptureMask[I] || !Visual || Visual->SubmittedPresentation[I].A<1 || Visual->SubmittedPresentation[I].B<=0;
     }
     Test->TestTrue(TEXT("Partially observed front has enough proven edge samples"),Checked>20);
     Test->TestEqual(TEXT("Proven physical edge survives capture and final gate without a fence"),Missing,0);
     Test->AddInfo(FString::Printf(TEXT("PARTIAL_EDGE observer_yaw=%.3f checked=%d missing=%d"),ProbeYaw,Checked,Missing));
    }
        if(Visual && !Visual->SubmittedPresentation.IsEmpty())
    for(int32 Channel=2;Channel<4;++Channel)
    {
     TArray<FColor> Mask; for(const auto& P:Visual->SubmittedPresentation) {const uint8 V=FMath::RoundToInt(FMath::Clamp(Channel==2?P.B:P.A,0.f,1.f)*255);Mask.Add(FColor(V,V,V,255));}
     TArray64<uint8> Bytes;const auto S=Record.FineHistory.GetSize();FImageUtils::PNGCompressImageArray(S.X,S.Y,Mask,Bytes);
     FFileHelper::SaveArrayToFile(Bytes,*(Dir/FString::Printf(TEXT("%s_history_%d.png"),Name,Channel)));
    }
   }
   for(int32 PartIndex=0;PartIndex<Prop.CurrentLive.Parts.Num();++PartIndex)
   {
    const auto& Part=Prop.CurrentLive.Parts[PartIndex];
    FString CSV=TEXT("index,coverage,observation,capture,appearance,live\n");
    for(int32 I=0;I<Part.Local.GetCells().Num();++I)
    { const auto& C=Part.Local.GetCells()[I]; CSV+=FString::Printf(TEXT("%d,%.9g,%d,%d,%.9g,%.9g\n"),I,Part.Coverage[I],Part.CurrentLegalObservationMask[I]?1:0,Part.LastLegalCaptureMask[I]?1:0,C.AppearanceBlend,C.LiveBlend); }
    FFileHelper::SaveStringToFile(CSV,*(Dir/(FString(Name)+TEXT("_local.csv"))));
    CSV=TEXT("index,R,G,B,A\n");
    for(int32 I=0;I<Part.Raster.GetCells().Num();++I)
    {const auto C=Part.Raster.Presentation(I); CSV+=FString::Printf(TEXT("%d,%.9g,%.9g,%.9g,%.9g\n"),I,C.R,C.G,C.B,C.A);}
    FFileHelper::SaveStringToFile(CSV,*(Dir/(FString(Name)+TEXT("_world.csv"))));
    const auto Size=Part.Raster.GetSize()*4;
    const auto& Submitted=Prop.CurrentPresentation.LivePixels[PartIndex];
    if(Submitted.Num()==Size.X*Size.Y)
    for(int32 Channel=0;Channel<4;++Channel)
    {
     TArray<FColor> Mask; for(const auto& P:Submitted) {const float V=Channel==0?P.R:Channel==1?P.G:Channel==2?P.B:P.A; const uint8 B=FMath::RoundToInt(FMath::Clamp(V,0.f,1.f)*255);Mask.Add(FColor(B,B,B,255));}
     TArray64<uint8> Bytes;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Mask,Bytes);
     FFileHelper::SaveArrayToFile(Bytes,*(Dir/FString::Printf(TEXT("%s_submitted_%d.png"),Name,Channel)));
    }
    Test->AddInfo(FString::Printf(TEXT("PARTIAL_PROBE %s parts=%d local=%dx%d raster=%dx%d bounds=%s pose=%s"),Name,Prop.CurrentLive.Parts.Num(),Part.Local.GetSize().X,Part.Local.GetSize().Y,Part.Raster.GetSize().X,Part.Raster.GetSize().Y,*Part.Raster.GetBounds().ToString(),*Part.Pose.ToHumanReadableString()));
   }
  }

 }
public:
 explicit FDarkwellCleanLabFrames(FAutomationTestBase* InTest,bool Probe=false):Test(InTest),bPartialProbe(Probe)
 {
  const auto Value=FPlatformMisc::GetEnvironmentVariable(TEXT("DARKWELL_PARTIAL_PROBE_YAW"));
  if(bPartialProbe && !Value.IsEmpty()) ProbeYaw=FCString::Atof(*Value);
 }
 virtual ~FDarkwellCleanLabFrames()
 {
  if(Fixture) Fixture->Destroy();
  if(World) { World->DestroyWorld(true); GEngine->DestroyWorldContext(World); }
 }
 virtual bool Update() override
 {
  if(!World)
  {
   auto* Package=CreatePackage(TEXT("/Game/Maps/L_BlackRegionLab"));
   World=NewObject<UWorld>(Package,MakeUniqueObjectName(Package,UWorld::StaticClass(),TEXT("CleanBlackLab")),RF_Transient);
   World->WorldType=EWorldType::Game; GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
   World->InitializeNewWorld(UWorld::InitializationValues().InitializeScenes(true).AllowAudioPlayback(false)
    .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false));
   FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   Player=World->SpawnActor<ADarkwellCharacter>(FVector(-200,-130,92),FRotator(0,-90,0),P);
   Player->PostInitializeComponents(); Player->DispatchBeginPlay();
   Fixture=World->SpawnActor<ADarkwellCleanBlackRegionLab>(); Fixture->PostInitializeComponents(); Fixture->DispatchBeginPlay();
   Scene=Fixture->MemoryScene; Trigger=Fixture->Trigger; Trigger->PostInitializeComponents(); Trigger->DispatchBeginPlay();
   Test->TestEqual(TEXT("No pre-observed records"),Scene->GetTotalSpatialRecordCount(),0);
   Test->TestEqual(TEXT("Only ground, Whole and Partial sources"),Scene->GetTrackedIdentityCount(),3);
   Test->TestNull(TEXT("No Moving/Multi room"),ADarkwellMovingPropLabRoom::FindActive(World));
   Test->TestFalse(TEXT("Default inactive"),Trigger->IsActive());
   auto* Camera=World->SpawnActor<ASceneCapture2D>();
   Camera->SetActorLocation(FVector(0,-500,700)); Camera->SetActorRotation((FVector(0,30,30)-Camera->GetActorLocation()).Rotation());
   Capture=Camera->GetCaptureComponent2D(); Capture->ProjectionType=ECameraProjectionMode::Orthographic; Capture->OrthoWidth=700;
   Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false; Capture->bAlwaysPersistRenderingState=true;
   Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR; Capture->ShowFlags.SetTemporalAA(true);
   Capture->PostProcessSettings.bOverride_AutoExposureMinBrightness=true; Capture->PostProcessSettings.bOverride_AutoExposureMaxBrightness=true;
   Capture->PostProcessSettings.AutoExposureMinBrightness=Capture->PostProcessSettings.AutoExposureMaxBrightness=1;
   Target=NewObject<UTextureRenderTarget2D>(World); Target->InitCustomFormat(768,768,PF_B8G8R8A8,false); Target->UpdateResourceImmediate(true); Capture->TextureTarget=Target;
   Render(); Render(TEXT("00_unknown"));
   Adapter=World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
   Test->TestTrue(TEXT("Clean fixture uses existing authority"),Adapter->RequestSightWeaveAuthority(Fixture));
  }
  ++Frame;
  if(bPartialProbe)
  {
   Player->SetActorRotation(FRotator(0,Frame<=180?ProbeYaw:Frame<=210?-90:45,0));
   Adapter->Tick(1.f/60); Scene->UpdateMemory(1.f/60,Player->GetActorLocation());
   Render(Frame==30?TEXT("probe_30"):Frame==90?TEXT("probe_90"):Frame==180?TEXT("probe_180"):Frame==210?TEXT("probe_left"):Frame==270?TEXT("probe_full"):nullptr);
   return Frame>=270;
  }
  if(Frame==61 || Frame==151 || Frame==241) Player->SetActorRotation(FRotator(0,45,0));
  if(Frame==91 || Frame==181 || Frame==271) Player->SetActorRotation(FRotator(0,-90,0));
  if(Frame==121) Test->TestTrue(TEXT("Box fully contains Whole and cuts Partial"),Trigger->Activate());
  if(Frame==211) Trigger->Deactivate();
  Adapter->Tick(1.f/60); Scene->UpdateMemory(1.f/60,Player->GetActorLocation());
  if(Frame==30) IdleRecords=Scene->GetTotalSpatialRecordCount();
  if(Frame==60)
  {
   Test->TestTrue(TEXT("Authority active"),Adapter->IsSightWeaveAuthorityActive());
   Test->TestEqual(TEXT("Unobserved Whole stays Unknown"),Scene->GetSpatialRecordCount(TEXT("BlackLab.Whole")),0);
   Test->TestEqual(TEXT("Unobserved Partial stays Unknown"),Scene->GetSpatialRecordCount(TEXT("BlackLab.Partial")),0);
   Test->TestEqual(TEXT("Idle creates no history"),Scene->GetTotalSpatialRecordCount(),IdleRecords);
  }
  if(Frame==90)
  {
   Test->TestTrue(TEXT("Legal observation establishes Whole"),Scene->GetSpatialRecordCount(TEXT("BlackLab.Whole"))>0);
   Test->TestTrue(TEXT("Legal observation establishes Partial"),Scene->GetSpatialRecordCount(TEXT("BlackLab.Partial"))>0);
  }
  const TCHAR* Name=nullptr;
  switch(Frame)
  {
   case 90:Name=TEXT("01_live");break;
   case 120:Name=TEXT("02_memory");break;
   case 150:Name=TEXT("03_clear");break;
   case 180:Name=TEXT("04_blocked_live");break;
   case 210:Name=TEXT("05_left_blocked");break;
   case 211:Name=TEXT("06_deactivated_first_frame");break;
   case 240:Name=TEXT("06_deactivated_idle");break;
   case 300:Name=TEXT("07_reobserved");break;
  }
  Render(Name);
  if(Frame<300) return false;
  Test->TestFalse(TEXT("Deactivation releases block"),World->GetSubsystem<UDarkwellMemoryRegionSubsystem>()->IsBlocked());
  return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellCleanBlackRegionLabTest,
 "Darkwell.BlackRegion.CleanLab",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellCleanBlackRegionLabTest::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FDarkwellCleanLabFrames(this));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellCleanPartialProbe,"Darkwell.BlackRegion.CurrentPartialProbe",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellCleanPartialProbe::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FDarkwellCleanLabFrames(this,true));
 return true;
}
#endif
