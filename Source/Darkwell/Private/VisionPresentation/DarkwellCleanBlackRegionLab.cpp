#include "VisionPresentation/DarkwellCleanBlackRegionLab.h"
#include "VisionPresentation/DarkwellObjectMemoryScene.h"
#include "VisionPresentation/DarkwellBlackRegionTrigger.h"
#include "VisionPresentation/DarkwellBlackRegionSwitch.h"
#include "VisionPresentation/DarkwellBlackRegionEventAdapter.h"
#include "VisionPresentation/DarkwellBlackoutEventVolume.h"
#include "VisionPresentation/DarkwellRememberablePropComponent.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "Player/DarkwellCharacter.h"
#include "Combat/DarkwellLoadoutComponent.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "SightWeaveObjectPolicy.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformApplicationMisc.h"

ADarkwellCleanBlackRegionLab::ADarkwellCleanBlackRegionLab()
{
 DemoEvent=CreateDefaultSubobject<UDarkwellBlackRegionEventAdapter>(TEXT("LabBlackoutEvent"));
 PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickGroup=TG_PostUpdateWork;
 // Inherit only the adapter's floor interface, light and camera. None of the
 // integration stress geometry or its RememberedFromStart proof is admitted.
 TArray<UStaticMeshComponent*> Meshes; GetComponents(Meshes);
 for(auto* Mesh:Meshes) { Mesh->SetVisibility(false); Mesh->SetHiddenInGame(true); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
 GetRememberablePropComponent()->ResetMemoryPrimitives();
 GetRememberablePropComponent()->ConfigureStableId(NAME_None);
 GetRememberablePropComponent()->bRememberFromStart=false;
}
FBox2D ADarkwellCleanBlackRegionLab::GetSightWeaveFloorBounds() const
{ return FBox2D(FVector2D(-320,-220),FVector2D(320,220)); }
void ADarkwellCleanBlackRegionLab::BuildSightWeaveOccluderSegments(TArray<FDarkwellVisionIntegrationSegment>& Out) const
{
 Out.Reset(); const auto B=GetSightWeaveFloorBounds();
 const FVector2D C[]{B.Min,FVector2D(B.Max.X,B.Min.Y),B.Max,FVector2D(B.Min.X,B.Max.Y)};
 for(int32 I=0;I<4;++I) { auto& S=Out.AddDefaulted_GetRef(); S.A=C[I]; S.B=C[(I+1)%4]; S.ZMin=0; S.ZMax=250; }
}
void ADarkwellCleanBlackRegionLab::BuildSightWeaveStaticSurfaces(TArray<FDarkwellVisionIntegrationSurface>& Out) const
{ Out.Reset(); } // No pre-known static declaration; ground uses the same sample knowledge as other static sources.
void ADarkwellCleanBlackRegionLab::BeginPlay()
{
 Super::BeginPlay();
 MemoryScene=GetWorld()->SpawnActor<ADarkwellObjectMemoryScene>();
 auto Spawn=[&](FName Id,FVector Location,FVector Size,float Yaw,FLinearColor Tint,bool Whole)
 {
  AActor* A=Id==TEXT("BlackLab.Console") ? GetWorld()->SpawnActor<ADarkwellBlackRegionSwitch>() : GetWorld()->SpawnActor<AActor>(); A->SetOwner(this);
  auto* Root=A->GetRootComponent();
  if(!Root) { Root=NewObject<USceneComponent>(A); A->SetRootComponent(Root); A->AddInstanceComponent(Root); Root->RegisterComponent(); }
  auto* Mesh=A->FindComponentByClass<UStaticMeshComponent>();
  if(!Mesh) { Mesh=NewObject<UStaticMeshComponent>(A); Mesh->SetupAttachment(Root); A->AddInstanceComponent(Mesh); }
  Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
  Mesh->SetMobility(EComponentMobility::Movable); Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
  // This fixed plane is below both props. Its translucent history must draw
  // behind their history, independently of proxy centers and camera distance.
  if(Id==TEXT("BlackLab.Ground")) Mesh->TranslucencySortPriority=-1;
  Mesh->SetRelativeScale3D(Size/100); if(!Mesh->IsRegistered()) Mesh->RegisterComponent();
  A->SetActorLocation(Location); A->SetActorRotation(FRotator(0,Yaw,0));
  auto* Memory=NewObject<UDarkwellRememberablePropComponent>(A); A->AddInstanceComponent(Memory);
  Memory->bUseSpatialMemory=true; Memory->bRememberFromStart=false;
  Memory->AddMemoryPrimitive(Mesh); Memory->ConfigureStableId(Id); Memory->SetMemoryAppearance(Tint,1); Memory->RegisterComponent();
  auto* Policy=NewObject<USightWeaveObjectPolicyComponent>(A); A->AddInstanceComponent(Policy);
  Policy->bOverrideRevealMode=true; Policy->RevealMode=Whole?ESightWeaveRevealMode::WholeObjectAfterSpan:ESightWeaveRevealMode::SpatialPartial;
  Policy->bOverrideMinimumObservedSpan=true; Policy->MinimumObservedSpanCm=Id==TEXT("BlackLab.Console")?20:60;
  Policy->bOverrideHistoryMode=true; Policy->HistoryMode=ESightWeaveHistoryMode::StationaryOnly;
  Policy->RegisterComponent();
  if(!MemoryScene->RegisterRememberable(Memory,Policy))
  { UE_LOG(LogTemp,Error,TEXT("CLEAN_BLACK_LAB registration failed: %s"),*Id.ToString()); A->Destroy(); return; }
  Sources.Add(A);
 };
 Spawn(TEXT("BlackLab.Ground"),FVector(0,0,-5),FVector(600,400,10),0,FLinearColor(.13,.16,.19),false);
 Spawn(TEXT("BlackLab.Whole"),FVector(-100,80,40),FVector(80,60,80),0,FLinearColor(.6,.35,.15),true);
 Spawn(TEXT("BlackLab.Partial"),FVector(130,80,55),FVector(140,60,110),37,FLinearColor(.15,.42,.6),false);
 FActorSpawnParameters P; P.Name=TEXT("BlackRegionLabTrigger");
 Trigger=GetWorld()->SpawnActor<ADarkwellBlackRegionTrigger>(FVector(-15,85,0),FRotator::ZeroRotator,P);
 Trigger->HalfExtentXY=FVector2D(145,55); Trigger->OnConstruction(Trigger->GetActorTransform());
 Spawn(TEXT("BlackLab.Console"),FVector(-230,-30,40),FVector(40,35,80),0,FLinearColor(.2,.65,.35),true);
 Console=Sources.IsEmpty()?nullptr:Cast<ADarkwellBlackRegionSwitch>(Sources.Last());
 if(Console) Console->Target=Trigger;
 DemoEvent->Target=Trigger;
 const FTransform EventPose(FVector(10,-30,90));
 EventVolume=GetWorld()->SpawnActorDeferred<ADarkwellBlackoutEventVolume>(ADarkwellBlackoutEventVolume::StaticClass(),EventPose,this);
 EventVolume->EventAdapter->Target=Trigger;
 EventVolume->EventBounds->SetHiddenInGame(false); EventVolume->EventBounds->ShapeColor=FColor::Cyan;
 EventVolume->FinishSpawning(EventPose);
 UE_LOG(LogTemp,Display,TEXT("CLEAN_BLACK_LAB initial=Unknown records=%d sources=4 (ground,Whole,Partial37,console) moving_room=0 trigger=Inactive"),MemoryScene->GetTotalSpatialRecordCount());
}
bool ADarkwellCleanBlackRegionLab::EnableDarkwellProjectFogP4(UTexture* Raw,FVector2D Min,FVector2D Inv)
{
 for(AActor* A:Sources) for(UStaticMeshComponent* Part:A->FindComponentByClass<UDarkwellRememberablePropComponent>()->GetMemoryPrimitives())
  if(auto* MID=Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)))
  {
   MID->SetTextureParameterValue(TEXT("DarkwellLiveCoverageTexture"),Raw);
  GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>()->BindHardPresentation(MID);
   MID->SetVectorParameterValue(TEXT("FogWorldMin"),FLinearColor(Min.X,Min.Y,0,0));
   MID->SetVectorParameterValue(TEXT("FogWorldInvExtent"),FLinearColor(Inv.X,Inv.Y,0,0));
  }
 return true;
}
void ADarkwellCleanBlackRegionLab::Tick(float Dt)
{
 Super::Tick(Dt);
 auto* Player=Cast<ADarkwellCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
 if(!Player || !MemoryScene) return;
 if(!bPlayerReady)
 {
  if(auto* Boom=Player->FindComponentByClass<USpringArmComponent>())
  { Boom->SetRelativeRotation(FRotator(-65,90,0)); Boom->TargetArmLength=800; }
  Player->GetLoadoutComponent()->RestorePersistentState(2,100,0,100,DarkwellGameplayTags::Equipment_Left_Shotgun,DarkwellGameplayTags::Equipment_Right_Torch);
  bPlayerReady=true;
  if(GEngine) GEngine->AddOnScreenDebugMessage(0x424C4143,20,FColor::Cyan,TEXT("BLACK REGION | WASD + mouse | approach green console, face it + F (150 cm) | orange Whole, blue Partial 37 deg"));
 }
#if !UE_BUILD_SHIPPING
 AdvanceBlackoutProbe(*Player);
#endif
 if(GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>()->IsActive()) MemoryScene->UpdateMemory(Dt,Player->GetActorLocation());
}
#if !UE_BUILD_SHIPPING
void ADarkwellCleanBlackRegionLab::AdvanceBlackoutProbe(ADarkwellCharacter& Player)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("DarkwellBlackoutProbe"))) return;
 const double Now=FPlatformTime::Seconds();
 if(BlackoutProbeFrame==0) BlackoutProbeCSV=TEXT("frame,cycle,phase,wall_ms,active,started\n");
 if(BlackoutProbePrevious)
  BlackoutProbeCSV+=FString::Printf(TEXT("%d,%d,%d,%.6f,%d,%d\n"),BlackoutProbeFrame-1,(BlackoutProbeFrame-301)/150,(BlackoutProbeFrame-301)%150,(Now-BlackoutProbePrevious)*1000,Trigger->IsActive(),EventVolume->EventAdapter->IsEventStarted());
 BlackoutProbePrevious=Now;
 const int32 F=BlackoutProbeFrame++;
 if(F>=300 && !FPlatformApplicationMisc::IsThisApplicationForeground())
 { UE_LOG(LogTemp,Error,TEXT("BLACKOUT_GAME_PROBE_INVALID foreground lost frame=%d"),F); FPlatformMisc::RequestExit(false); return; }
 // Warm up shaders, create genuine observation, then measure twenty complete
 // collision-enabled transitions. No capture/readback/flush in measured frames.
 const int32 Phase=F<300?-1:(F-300)%150,Cycle=F<300?-1:(F-300)/150;
 if(F<300) Player.SetActorRotation(FRotator(0,F<180?45:-90,0));
 if(Cycle>=20)
 {
  FString Output;
  if(!FParse::Value(FCommandLine::Get(),TEXT("DarkwellBlackoutProbeOutput="),Output)) Output=FPaths::ProjectSavedDir()/TEXT("BlackoutProbe");
  IFileManager::Get().MakeDirectory(*Output,true);
  FFileHelper::SaveStringToFile(BlackoutProbeCSV,*(Output/TEXT("game_frames.csv")));
  UE_LOG(LogTemp,Display,TEXT("BLACKOUT_GAME_PROBE_COMPLETE cycles=20 %s"),*MemoryScene->GetStorageTelemetry());
  FPlatformMisc::RequestExit(false); return;
 }
 if(Phase==0 || Phase==60)
 {
  const double Start=FPlatformTime::Seconds();
  Player.SetActorLocation(FVector(Phase==0?-80:-200,-130,92));
  UE_LOG(LogTemp,Display,TEXT("BLACKOUT_GAME_EDGE cycle=%d phase=%d move_ms=%.6f active=%d started=%d"),Cycle,Phase,(FPlatformTime::Seconds()-Start)*1000,Trigger->IsActive(),EventVolume->EventAdapter->IsEventStarted());
 }
 if(Phase>=0) Player.SetActorRotation(FRotator(0,(Phase>=30 && Phase<50) || (Phase>=90 && Phase<120)?45:-90,0));
}
#endif
void ADarkwellCleanBlackRegionLab::EndPlay(EEndPlayReason::Type Reason)
{
#if !UE_BUILD_SHIPPING
 if(!BlackoutProbeCSV.IsEmpty())
 {
  FString Output;
  if(!FParse::Value(FCommandLine::Get(),TEXT("DarkwellBlackoutProbeOutput="),Output)) Output=FPaths::ProjectSavedDir()/TEXT("BlackoutProbe");
  IFileManager::Get().MakeDirectory(*Output,true);
  FFileHelper::SaveStringToFile(BlackoutProbeCSV,*(Output/TEXT("game_frames.csv")));
 }
#endif
 if(IsValid(EventVolume)) EventVolume->Destroy();
 if(IsValid(DemoEvent)) DemoEvent->EndEvent();
 if(IsValid(Console)) Console->Destroy();
 if(Trigger) Trigger->Destroy();
 if(MemoryScene) MemoryScene->Destroy();
 for(AActor* A:Sources) if(IsValid(A)) A->Destroy();
 Sources.Reset(); Super::EndPlay(Reason);
}
