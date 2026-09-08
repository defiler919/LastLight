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
#include "VisionPresentation/DarkwellBlackRegionSwitch.h"
#include "VisionPresentation/DarkwellBlackRegionEventAdapter.h"
#include "VisionPresentation/DarkwellBlackoutEventVolume.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DamageEvents.h"
#include "Interaction/DarkwellInteractionComponent.h"
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
 bool bEventDemo=false;
 bool bVolumeDemo=false;
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
  const FString Dir=(Root.IsEmpty()?FPaths::ProjectSavedDir():Root)/(bVolumeDemo?TEXT("Captures/VolumeBlackLab"):bEventDemo?TEXT("Captures/EventBlackLab"):TEXT("Captures/CleanBlackLab"));
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
 explicit FDarkwellCleanLabFrames(FAutomationTestBase* InTest,bool Probe=false,bool EventDemo=false,bool VolumeDemo=false):Test(InTest),bPartialProbe(Probe),bEventDemo(EventDemo),bVolumeDemo(VolumeDemo)
 {
  const auto Value=FPlatformMisc::GetEnvironmentVariable(TEXT("DARKWELL_PARTIAL_PROBE_YAW"));
  if(bPartialProbe && !Value.IsEmpty()) ProbeYaw=FCString::Atof(*Value);
 }
 virtual ~FDarkwellCleanLabFrames()
 {
  if(Fixture) Fixture->Destroy();
  if(World) { if(World->HasBegunPlay()) World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(true); GEngine->DestroyWorldContext(World); }
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
   Fixture->EventVolume->PostInitializeComponents(); Fixture->EventVolume->DispatchBeginPlay();
   Fixture->Console->PostInitializeComponents(); Fixture->Console->DispatchBeginPlay();
   Scene=Fixture->MemoryScene; Trigger=Fixture->Trigger; Trigger->PostInitializeComponents(); Trigger->DispatchBeginPlay();
   Test->TestEqual(TEXT("No pre-observed records"),Scene->GetTotalSpatialRecordCount(),0);
   Test->TestEqual(TEXT("Ground, Whole, Partial and Unknown console"),Scene->GetTrackedIdentityCount(),4);
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
   if(bVolumeDemo) { World->InitializeActorsForPlay(FURL()); World->SetBegunPlay(true); World->Tick(LEVELTICK_All,1.f/60); } // Publish physics bodies and enable real overlap dispatch.
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
  if(bVolumeDemo && (Frame==121 || Frame==211))
  {
   Player->SetActorLocation(FVector(Frame==121?-80:-200,-130,92));
   auto* Volume=Fixture->EventVolume.Get(); auto* Event=Volume->EventAdapter.Get();
   Test->AddInfo(FString::Printf(TEXT("VOLUME_OVERLAP frame=%d world_begun=%d actor_begun=%d capsule_events=%d overlaps=%d box=%s player=%s"),Frame,World->HasBegunPlay(),Volume->HasActorBegunPlay(),Player->GetCapsuleComponent()->GetGenerateOverlapEvents(),Volume->EventBounds->IsOverlappingComponent(Player->GetCapsuleComponent()),*Volume->GetActorLocation().ToString(),*Player->GetActorLocation().ToString()));
   Test->TestEqual(TEXT("Real capsule overlap drives event edge"),Event->IsEventStarted(),Frame==121);
   Test->TestEqual(TEXT("Overlap drives fixed target"),Trigger->IsActive(),Frame==121);
   const auto Revision=World->GetSubsystem<UDarkwellMemoryRegionSubsystem>()->GetAuthorityRevision();
   Player->GetCapsuleComponent()->UpdateOverlaps(); Volume->EventBounds->UpdateOverlaps();
   Test->TestEqual(TEXT("Repeated overlap updates are idempotent"),World->GetSubsystem<UDarkwellMemoryRegionSubsystem>()->GetAuthorityRevision(),Revision);
  }
  else if(bEventDemo && (Frame==121 || Frame==211))
  {
   auto* Event=Fixture->DemoEvent.Get(); auto* Region=World->GetSubsystem<UDarkwellMemoryRegionSubsystem>();
   const auto Location=Player->GetActorLocation();
   Player->SetActorLocation(Location+FVector(1000,0,0));
   Test->TestFalse(TEXT("Event does not require console proximity"),Fixture->Console->CanInteract(*Player));
   if(Frame==121)
   {
    Test->TestTrue(TEXT("Event begins remotely"),Event->BeginEvent());
    const auto Revision=Region->GetAuthorityRevision();
    Test->TestTrue(TEXT("Duplicate start succeeds"),Event->BeginEvent());
    Test->TestEqual(TEXT("Duplicate start never re-clears"),Region->GetAuthorityRevision(),Revision);
   }
   else
   {
    Event->EndEvent(); const auto Revision=Region->GetAuthorityRevision(); Event->EndEvent();
    Test->TestEqual(TEXT("Duplicate end has no effect"),Region->GetAuthorityRevision(),Revision);
   }
   Test->TestEqual(TEXT("Event controls target on each edge"),Trigger->IsActive(),Frame==121);
   Test->TestEqual(TEXT("Event edge state"),Event->IsEventStarted(),Frame==121);
   Player->SetActorLocation(Location);
  }
  else if(Frame==121 || Frame==211)
  {
   auto* Interaction=Player->GetInteractionComponent();
   const auto Location=Player->GetActorLocation(); const auto Rotation=Player->GetActorRotation();
   Player->SetActorLocation(Fixture->Console->GetActorLocation()+FVector(0,-200,52));
   Player->SetActorRotation(FRotator(0,90,0));
   Test->TestFalse(TEXT("Outside console distance rejects F path"),Interaction->TryInteract());
   Fixture->Console->Interact(*Player);
   Test->TestEqual(TEXT("Direct out-of-range call also leaves state unchanged"),Trigger->IsActive(),Frame==211);
   Player->SetActorLocation(Location); Player->SetActorRotation(FRotator(0,-90,0));
   Test->TestFalse(TEXT("Facing away rejects F path"),Interaction->TryInteract());
   Player->SetActorRotation((Fixture->Console->GetActorLocation()-Location).Rotation());
   Interaction->UpdateFocusedActorFromWorld();
   Test->TestEqual(TEXT("World query focuses console"),Interaction->GetFocusedActor(),static_cast<AActor*>(Fixture->Console));
   Test->TestFalse(TEXT("Focused state/action prompt exists"),Interaction->GetFocusedPrompt().IsEmpty());
   Test->TestTrue(TEXT("F handler interaction path toggles target"),Interaction->TryInteract());
   Test->TestEqual(TEXT("Target state reflects toggle"),Trigger->IsActive(),Frame==121);
   Test->TestTrue(TEXT("Prompt changes to actual target state"),Interaction->GetFocusedPrompt().ToString().Contains(Frame==121?TEXT("ACTIVE - Deactivate"):TEXT("INACTIVE - Activate")));
   Player->SetActorRotation(Rotation);
  }
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
  if(bVolumeDemo)
  {
   auto* Volume=Fixture->EventVolume.Get(); auto* Event=Volume->EventAdapter.Get();
   auto* Region=World->GetSubsystem<UDarkwellMemoryRegionSubsystem>();
   Player->SetActorLocation(FVector(-100,-30,92));
   Test->TestTrue(TEXT("Reentry starts again"),Event->IsEventStarted());
   Player->SetActorRotation((Fixture->Console->GetActorLocation()-Player->GetActorLocation()).Rotation());
   Test->TestTrue(TEXT("F works while overlapping"),Player->GetInteractionComponent()->TryInteract());
   Test->TestFalse(TEXT("F overrides target off"),Trigger->IsActive());
   Player->GetCapsuleComponent()->UpdateOverlaps();
   Test->TestFalse(TEXT("Duplicate overlap does not undo F"),Trigger->IsActive());
   Test->TestTrue(TEXT("F reactivates"),Player->GetInteractionComponent()->TryInteract());
   Player->TakeDamage(1000,FDamageEvent(),nullptr,nullptr);
   Test->TestFalse(TEXT("Death immediately ends event"),Event->IsEventStarted());
   Test->TestFalse(TEXT("Death immediately unblocks"),Region->IsBlocked());
   Player->SetActorLocation(FVector(-200,-130,92)); Player->SetActorLocation(FVector(-80,-130,92));
   Test->TestFalse(TEXT("Corpse cannot start event"),Event->IsEventStarted());
   Player->Destroy();
   FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   Player=World->SpawnActor<ADarkwellCharacter>(FVector(-200,-130,92),FRotator::ZeroRotator,Spawn);
   if(!Player->HasActorBegunPlay()) { Player->PostInitializeComponents(); Player->DispatchBeginPlay(); } Player->SetActorLocation(FVector(-80,-130,92));
   Test->TestTrue(TEXT("New live pawn starts event"),Event->IsEventStarted());
   Player->Destroy();
   Test->TestFalse(TEXT("Pawn destruction ends event"),Event->IsEventStarted());
   Test->TestFalse(TEXT("Pawn destruction unblocks"),Region->IsBlocked());
   Player=World->SpawnActor<ADarkwellCharacter>(FVector(-200,-130,92),FRotator::ZeroRotator,Spawn);
   if(!Player->HasActorBegunPlay()) { Player->PostInitializeComponents(); Player->DispatchBeginPlay(); } Player->SetActorLocation(FVector(-80,-130,92));
   Test->TestTrue(TEXT("Volume active before removal"),Event->IsEventStarted());
   Volume->Destroy();
   Test->TestFalse(TEXT("Volume destruction unblocks"),Region->IsBlocked());
   const FTransform Pose(FVector(10,-30,90));
   auto* Replacement=World->SpawnActorDeferred<ADarkwellBlackoutEventVolume>(ADarkwellBlackoutEventVolume::StaticClass(),Pose);
   Replacement->EventAdapter->Target=Trigger; Replacement->FinishSpawning(Pose);
   Test->TestTrue(TEXT("Spawned volume discovers player already inside"),Replacement->EventAdapter->IsEventStarted());
   World->EndPlay(EEndPlayReason::Quit);
   Test->TestFalse(TEXT("World EndPlay releases Volume block"),Region->IsBlocked());
   Test->TestFalse(TEXT("World EndPlay ends event"),Replacement->EventAdapter->IsEventStarted());
   return true;
  }
  if(bEventDemo)
  {
   auto* Event=Fixture->DemoEvent.Get(); auto* Region=World->GetSubsystem<UDarkwellMemoryRegionSubsystem>();
   auto* Interaction=Player->GetInteractionComponent();
   Test->TestTrue(TEXT("Repeated event cycle"),Event->BeginEvent());
   Player->SetActorRotation((Fixture->Console->GetActorLocation()-Player->GetActorLocation()).Rotation());
   Test->TestTrue(TEXT("F can override a started event"),Interaction->TryInteract());
   Test->TestFalse(TEXT("F turned trigger off"),Trigger->IsActive());
   Test->TestTrue(TEXT("Duplicate start acknowledged after F"),Event->BeginEvent());
   Test->TestFalse(TEXT("Duplicate start cannot undo F override"),Trigger->IsActive());
   Test->TestTrue(TEXT("F can turn it on again"),Interaction->TryInteract());
   Event->EndEvent(); Test->TestFalse(TEXT("End edge deactivates"),Trigger->IsActive());
   Test->TestTrue(TEXT("F independent after event"),Interaction->TryInteract());
   Event->EndEvent(); Test->TestTrue(TEXT("Duplicate end cannot cancel later F activation"),Trigger->IsActive());
   Trigger->Deactivate();
   Test->TestTrue(TEXT("Start before component destruction"),Event->BeginEvent());
   Event->DestroyComponent(); Test->TestFalse(TEXT("Component destruction releases Block"),Region->IsBlocked());
   Test->TestFalse(TEXT("Destroyed component cannot restart"),Event->BeginEvent());
   auto* Replacement=NewObject<UDarkwellBlackRegionEventAdapter>(Fixture); Fixture->AddInstanceComponent(Replacement);
   Replacement->Target=Trigger; Replacement->RegisterComponent();
   if(!Replacement->HasBegunPlay()) Replacement->BeginPlay();
   Test->TestTrue(TEXT("Start before world teardown"),Replacement->BeginEvent());
   const auto Revision=Scene->GeometryRevision;
   const auto Records=Scene->GetTotalSpatialRecordCount();
   World->BeginTearingDown(); Replacement->EndEvent();
   Test->TestFalse(TEXT("Teardown releases runtime Block"),Region->IsBlocked());
   Test->TestFalse(TEXT("Teardown releases owner"),Region->IsGameplayControlledBy(Trigger));
   Test->TestEqual(TEXT("Teardown does not dispatch scene rebuild"),Scene->GeometryRevision,Revision);
   Test->TestEqual(TEXT("Teardown does not generate history"),Scene->GetTotalSpatialRecordCount(),Records);
   Test->TestFalse(TEXT("No restart during teardown"),Replacement->BeginEvent());
   return true;
  }
  Test->TestTrue(TEXT("Reactivation for switch destruction"),Trigger->Activate());
  Fixture->Console->Destroy();
  Test->TestFalse(TEXT("Destroy console releases target Block"),World->GetSubsystem<UDarkwellMemoryRegionSubsystem>()->IsBlocked());
  Test->TestFalse(TEXT("Destroyed console cannot interact"),Fixture->Console->CanInteract(*Player));
  Trigger->Deactivate(); Trigger->Deactivate();
  Test->TestTrue(TEXT("Reactivation for target destruction"),Trigger->Activate());
  Trigger->Destroy();
  Test->TestFalse(TEXT("Destroy target releases Block"),World->GetSubsystem<UDarkwellMemoryRegionSubsystem>()->IsBlocked());
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellBlackEventDemo,"Darkwell.BlackRegion.EventDemo",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellBlackEventDemo::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FDarkwellCleanLabFrames(this,false,true));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellBlackVolumeDemo,"Darkwell.BlackRegion.VolumeDemo",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellBlackVolumeDemo::RunTest(const FString&)
{
 ADD_LATENT_AUTOMATION_COMMAND(FDarkwellCleanLabFrames(this,false,false,true));
 return true;
}
#endif
