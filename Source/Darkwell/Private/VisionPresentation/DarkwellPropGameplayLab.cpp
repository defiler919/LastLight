#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "AI/DarkwellStalkerCharacter.h"
#include "BrainComponent.h"
#include "AIController.h"
#include "Combat/DarkwellLoadoutComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/DarkwellCharacter.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Engine/GameViewportClient.h"
#include "Async/Async.h"
#include <atomic>
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "NavigationSystem.h"
#include "VisionPresentation/DarkwellRememberedPropSubsystem.h"
#include "Visibility/SightWeave/DarkwellSightWeaveWorldSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "VisionPresentation/DarkwellRememberablePropComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDarkwellPropLab, Log, All);
struct FDarkwellLabCaptureWriter
{
 std::atomic<int32> Pending{0};
 std::atomic<int32> Failed{0};
};
namespace Darkwell::PropLab
{
 FVector ComparisonPlayerPosition() { return FVector(400,-700,92); }
 float ComparisonYaw(float Seconds)
 {
  // 2s gray; 6s to midpoint; 2s hold; 6s to far end; 12s reverse; 2s gray.
  if(Seconds<=2) return -30;
  if(Seconds<8) return -30+(Seconds-2)*(70.f/6.f);
  if(Seconds<10) return 40;
  if(Seconds<16) return 40+(Seconds-10)*(70.f/6.f);
  if(Seconds<28) return 110-(Seconds-16)*(70.f/6.f);
  return -30;
 }
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
 TAutoConsoleVariable<int32> Mode(TEXT("r.Darkwell.ProjectFogVisual.PropPresentationMode"), 0, TEXT("Lab only: 0 AcceptedWholeObject, 1 SurfaceSweepHard, 2 SurfaceSweepSoft."));
 TAutoConsoleVariable<int32> Policy(TEXT("r.Darkwell.ProjectFogVisual.PropRelocationPolicy"), 0, TEXT("Lab only: 0 VerifyOldLocation, 1 RecognizedIdentityRelocation."));
 TAutoConsoleVariable<int32> Route(TEXT("r.Darkwell.ProjectFogVisual.LabRoute"), 0, TEXT("Lab only: 0 manual, 1 fixed-camera 30s comparison, 2 oblique, 3 rotate, 4 parallel, 5 old-first, 6 new-first, 7 tool cycle, 8 twins, 9 destruction, 10 replacement, 11 horizontal, 12 vertical, 13 diagonal."));
 FAutoConsoleCommandWithWorldAndArgs Control(TEXT("Darkwell.PropLab"), TEXT("help/reset/fridge/cabinet/destroy/replace/swap/torch/lantern/dark; mode N; policy N; route N"),
  FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
  {
   if (!IsLabWorld(World)) { UE_LOG(LogDarkwellPropLab, Display, TEXT("PropLab command rejected outside laboratory")); return; }
   for (TActorIterator<ADarkwellPropGameplayLab> It(World); It; ++It) It->Event(FString::Join(Args, TEXT(" ")));
  }));
#endif
 bool IsLabWorld(const UWorld* World)
 {
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
  if (!World || !World->IsGameWorld()) return false;
  FString Name = World->GetOutermost()->GetName();
  const FString Prefix = World->StreamingLevelsPrefix;
  if (!Prefix.IsEmpty()) Name.ReplaceInline(*Prefix, TEXT(""));
  return Name == TEXT("/Game/Maps/L_ProjectFogPropGameplayLab");
#else
  return false;
#endif
 }
 int32 PresentationMode(const UWorld* World)
 {
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
  return IsLabWorld(World) ? FMath::Clamp(Mode.GetValueOnGameThread(), 0, 2) : 0;
#else
  return 0;
#endif
 }
 int32 RelocationPolicy(const UWorld* World)
 {
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
  return IsLabWorld(World) ? FMath::Clamp(Policy.GetValueOnGameThread(), 0, 1) : 0;
#else
  return 0;
#endif
 }
}

ADarkwellPropLabFurniture::ADarkwellPropLabFurniture()
{
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 Memory = CreateDefaultSubobject<UDarkwellRememberablePropComponent>(TEXT("Identity"));
 for (int32 I = 0; I < 12; ++I)
 {
  auto* Part = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Part%d"), I));
  Part->SetupAttachment(GetRootComponent());
  Part->SetStaticMesh(Cube.Object);
  Part->SetMobility(EComponentMobility::Movable);
  Part->SetRenderCustomDepth(false);
  Part->SetReceivesDecals(false);
  Parts.Add(Part);
 }
}

void ADarkwellPropLabFurniture::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);
 if (Memory->HasBegunPlay()) return;
 Memory->ResetMemoryPrimitives();
 Memory->SetMemoryAppearance(Tint,1.0f);
 for (UStaticMeshComponent* Part : Parts) { Part->SetVisibility(false); Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
 int32 Index = 0;
 auto Add = [&](FVector Center, FVector Size)
 {
  UStaticMeshComponent* Part = Parts[Index++];
  Part->SetRelativeLocation(Center);
  Part->SetRelativeScale3D(Size / 100.0);
  Part->SetVisibility(true);
  Part->SetCollisionProfileName(TEXT("BlockAllDynamic"));
  Memory->AddMemoryPrimitive(Part);
 };
 const double W = Dimensions.X, D = Dimensions.Y, H = Dimensions.Z;
 if (Shape == 2)
 {
  for (double X : {-1.0, 1.0}) for (double Y : {-1.0, 1.0}) Add(FVector(X*(W/2-3),Y*(D/2-3),H/2),FVector(6,6,H));
  // Shelves meet the inner post faces; no intersecting coplanar exterior skin.
  for (int32 I=0; I<4; ++I) Add(FVector(0,0,8+(H-16)*I/3),FVector(W-12,D,5));
 }
 else
 {
  Add(FVector(0,0,H/2),FVector(W,D,H));
  if (Shape == 0 || Shape == 1)
  {
   const int32 Doors = Shape == 1 ? 2 : 1;
   for (int32 I=0; I<Doors; ++I)
   {
    Add(FVector(0,-D/2-2,(I+0.5)*H/Doors),FVector(W-4,4,H/Doors-4));
    Add(FVector(W*.30,-D/2-6.5,(I+0.5)*H/Doors),FVector(3,5,Shape == 1 ? 28 : 16));
   }
   if (Shape == 0 && bIndividualWorktop) Add(FVector(0,0,H+2),FVector(W,D,4));
  }
 }
 Memory->ConfigureStableId(StableId);
}

void ADarkwellPropLabFurniture::BeginPlay()
{
 // Saved actor properties and native parts are bound before component BeginPlay.
 OnConstruction(GetActorTransform());
 auto* Parent = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Darkwell/Vision/PropLab/M_PropLabSurface.M_PropLabSurface"));
 for (UStaticMeshComponent* Part : Parts)
 {
  auto* Material = UMaterialInstanceDynamic::Create(Parent, this);
  Material->SetVectorParameterValue(TEXT("OriginalBaseColorTint"), Tint);
  Material->SetScalarParameterValue(TEXT("OriginalUVScale"), 1);
  Material->SetScalarParameterValue(TEXT("LabWholeObject"), 1);
  Part->SetMaterial(0, Material);
  Materials.Add(Material);
 }
 // Register the initial memory after its final material bindings exist.
 Super::BeginPlay();
}

void ADarkwellPropLabFurniture::BindPresentation(UTexture* Raw, UTexture* Soft, FVector2D Min, FVector2D Inv, int32 Mode)
{
 for (UMaterialInstanceDynamic* Material : Materials)
 {
  Material->SetTextureParameterValue(TEXT("DarkwellLiveCoverageTexture"), Raw);
  Material->SetTextureParameterValue(TEXT("LabSoftCoverageTexture"), Soft ? Soft : Raw);
  Material->SetVectorParameterValue(TEXT("FogWorldMin"), FLinearColor(Min.X,Min.Y,0,0));
  Material->SetVectorParameterValue(TEXT("FogWorldInvExtent"), FLinearColor(Inv.X,Inv.Y,0,0));
  Material->SetScalarParameterValue(TEXT("LabWholeObject"), Mode == 0 ? 1 : 0);
  Material->SetScalarParameterValue(TEXT("LabSoft"), Mode == 2 ? 1 : 0);
 }
}

ADarkwellPropGameplayLab::ADarkwellPropGameplayLab()
{
 PrimaryActorTick.bCanEverTick = true;
 PrimaryActorTick.TickGroup = TG_PostUpdateWork;
 // The production fixture keeps its exact defaults. This derived fixture disables its geometry.
 TInlineComponentArray<UStaticMeshComponent*> Inherited(this);
 for (auto* Mesh : Inherited) { Mesh->SetVisibility(false); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
 GetRememberablePropComponent()->ConfigureStableId(NAME_None);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 const FVector Centers[] = { {0,0,-15},{-1000,100,125},{-1000,-630,125},{-750,720,125},{-550,590,125},{-230,-530,52},{-560,415,46},{-920,255,46},{-1080,0,46} };
 const FVector Sizes[] = { {2400,1800,30},{25,700,250},{25,440,250},{520,25,250},{25,260,250},{320,28,104},{480,20,92},{20,240,92},{20,180,92} };
 for (int32 I=0; I<UE_ARRAY_COUNT(Centers); ++I)
 {
  auto* Part = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("LabStructure%d"),I));
  Part->SetupAttachment(GetRootComponent()); Part->SetStaticMesh(Cube.Object);
  Part->SetRelativeLocation(Centers[I]); Part->SetRelativeScale3D(Sizes[I]/100.0);
  Part->SetCollisionProfileName(TEXT("BlockAll")); Part->SetRenderCustomDepth(false);
  LabStructure.Add(Part);
 }
}

FBox2D ADarkwellPropGameplayLab::GetSightWeaveFloorBounds() const { return FBox2D(FVector2D(-1250,-950),FVector2D(1250,950)); }
void ADarkwellPropGameplayLab::BuildSightWeaveOccluderSegments(TArray<FDarkwellVisionIntegrationSegment>& Out) const
{
 Out.Reset();
 for (int32 I=1; I<LabStructure.Num(); ++I)
 {
  const FVector P = LabStructure[I]->GetComponentLocation(), S = LabStructure[I]->GetComponentScale()*100;
  const FVector2D E = S.X > S.Y ? FVector2D(S.X/2,0) : FVector2D(0,S.Y/2);
  auto& Segment = Out.AddDefaulted_GetRef(); Segment.A = FVector2D(P)-E; Segment.B = FVector2D(P)+E;
  Segment.ZMin=0; Segment.ZMax=S.Z;
 }
}
void ADarkwellPropGameplayLab::BuildSightWeaveStaticSurfaces(TArray<FDarkwellVisionIntegrationSurface>& Out) const
{
 Out.Reset(); auto& Floor = Out.AddDefaulted_GetRef();
 Floor.WorldFootprint = {FVector2D(-1200,-900),FVector2D(1200,-900),FVector2D(1200,900),FVector2D(-1200,900)};
 Floor.NeutralIntensity = 90;
}

bool ADarkwellPropGameplayLab::EnableDarkwellProjectFogP4(UTexture* Raw, FVector2D Min, FVector2D Inv)
{
 if (!Darkwell::PropLab::IsLabWorld(GetWorld())) return false;
 RawCoverage=Raw; FogMin=Min; FogInv=Inv;
 auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Darkwell/Vision/PropLab/M_PropLabSurface.M_PropLabSurface"));
 for (int32 I=0; I<LabStructure.Num(); ++I)
 {
  auto* Mat=UMaterialInstanceDynamic::Create(Parent,this);
  Mat->SetTextureParameterValue(TEXT("DarkwellLiveCoverageTexture"),Raw);
  Mat->SetVectorParameterValue(TEXT("FogWorldMin"),FLinearColor(Min.X,Min.Y,0,0));
  Mat->SetVectorParameterValue(TEXT("FogWorldInvExtent"),FLinearColor(Inv.X,Inv.Y,0,0));
  Mat->SetScalarParameterValue(TEXT("OriginalUVScale"),I==0 ? 18 : 3);
  Mat->SetVectorParameterValue(TEXT("OriginalBaseColorTint"),I==0 ? FLinearColor(.15f,.17f,.19f) : FLinearColor(.35f,.38f,.40f));
  LabStructure[I]->SetMaterial(0,Mat); StructureMaterials.Add(Mat);
 }
 SoftMaterial=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Darkwell/Vision/PropLab/M_PropLabSoft.M_PropLabSoft")),this);
 for (int32 I=0; I<2; ++I)
 {
  auto* RT=UKismetRenderingLibrary::CreateRenderTarget2D(this,1000,760,RTF_R16f,FLinearColor::Black,false);
  RT->Filter=TF_Bilinear; SoftTargets.Add(RT);
 }
 UE_LOG(LogDarkwellPropLab,Display,TEXT("PropLab activated continuous coverage, no composite/stencil; 8 occluders; TSR unchanged"));
 return true;
}
void ADarkwellPropGameplayLab::DisableDarkwellProjectFog() { RawCoverage=nullptr; SoftTargets.Reset(); SoftMaterial=nullptr; StructureMaterials.Reset(); }

void ADarkwellPropGameplayLab::BeginPlay()
{
 Super::BeginPlay();
 if (!Darkwell::PropLab::IsLabWorld(GetWorld())) { SetActorTickEnabled(false); return; }
 // Inputs must precede TickableGameObject authority updates. Material/capture stays
 // in PostUpdateWork, after those updates; never teleport a subject after authority.
 RouteTickHandle=FWorldDelegates::OnWorldPreActorTick.AddUObject(this,&ADarkwellPropGameplayLab::AdvanceRouteBeforeActors);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
 if(FParse::Param(FCommandLine::Get(),TEXT("PropLabAsyncCapture")))
 {
  CaptureWriter=MakeShared<FDarkwellLabCaptureWriter,ESPMode::ThreadSafe>();
  ScreenshotHandle=UGameViewportClient::OnScreenshotCaptured().AddLambda([Writer=CaptureWriter](int32 W,int32 H,const TArray<FColor>& Bitmap)
  {
   const FString Filename=FScreenshotRequest::GetFilename();
   Writer->Pending.fetch_add(1);
   Async(EAsyncExecution::ThreadPool,[Writer,Filename,W,H,Pixels=Bitmap]()
   {
    TArray64<uint8> Compressed;
    FImageUtils::PNGCompressImageArray(W,H,Pixels,Compressed);
    if(!FFileHelper::SaveArrayToFile(Compressed,*Filename)) Writer->Failed.fetch_add(1);
    Writer->Pending.fetch_sub(1);
   });
  });
 }
#endif
 for (TActorIterator<ADarkwellPropLabFurniture> It(GetWorld()); It; ++It) InitialTransforms.Add(It->StableId,It->GetActorTransform());
 UE_LOG(LogDarkwellPropLab,Display,TEXT("PropLab ready furniture=%d; controls: Darkwell.PropLab help"),InitialTransforms.Num());
}
void ADarkwellPropGameplayLab::EndPlay(EEndPlayReason::Type Reason)
{
 if(ScreenshotHandle.IsValid()) UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotHandle);
 if(RouteTickHandle.IsValid()) FWorldDelegates::OnWorldPreActorTick.Remove(RouteTickHandle);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
 if (Darkwell::PropLab::IsLabWorld(GetWorld()))
 {
  Darkwell::PropLab::Mode->Set(0,ECVF_SetByConsole); Darkwell::PropLab::Policy->Set(0,ECVF_SetByConsole); Darkwell::PropLab::Route->Set(0,ECVF_SetByConsole);
 }
#endif
 Super::EndPlay(Reason);
}
void ADarkwellPropGameplayLab::UpdateSoftCoverage(float DeltaSeconds)
{
 if (!SoftMaterial || SoftTargets.Num()!=2 || !RawCoverage) return;
 SoftMaterial->SetTextureParameterValue(TEXT("Raw"),RawCoverage);
 SoftMaterial->SetTextureParameterValue(TEXT("Previous"),SoftTargets[SoftIndex]);
 SoftMaterial->SetScalarParameterValue(TEXT("Step"),FMath::Min(DeltaSeconds,0.2f)/0.2f);
 SoftIndex=1-SoftIndex;
 UKismetRenderingLibrary::DrawMaterialToRenderTarget(this,SoftTargets[SoftIndex],SoftMaterial);
}
void ADarkwellPropGameplayLab::AdvanceRouteBeforeActors(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
 if (World!=GetWorld() || !Darkwell::PropLab::IsLabWorld(World)) return;
 if (Elapsed == 0)
 {
  if (auto* Player=UGameplayStatics::GetPlayerPawn(this,0))
   if(auto* Boom=Player->FindComponentByClass<USpringArmComponent>())
   { Boom->SetRelativeRotation(FRotator(-65,90,0)); Boom->TargetArmLength=1450; }
 }
 if(!bPlayerInitialized) if(auto* Player=Cast<ADarkwellCharacter>(UGameplayStatics::GetPlayerPawn(this,0)))
 {
  Player->RestorePersistentState(Player->GetActorTransform(),Player->GetMaxHealth(),DarkwellGameplayTags::State_Player_Alive,FGameplayTag());
  Player->GetLoadoutComponent()->RestorePersistentState(2,100,0,100,DarkwellGameplayTags::Equipment_Left_Shotgun,DarkwellGameplayTags::Equipment_Right_Torch);
  bPlayerInitialized=true;
 }
 Elapsed+=DeltaSeconds;
 if(FParse::Param(FCommandLine::Get(),TEXT("PropLabComparisonCapture")) && !World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>()->IsSightWeaveAuthorityActive()) return;
 RunRoute(DeltaSeconds);
 RestoreComparisonTools(LastRoute==1);
}
void ADarkwellPropGameplayLab::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 if (!Darkwell::PropLab::IsLabWorld(GetWorld())) return;
 RestoreComparisonTools(LastRoute==1);
 const int32 Mode=Darkwell::PropLab::PresentationMode(GetWorld()), Policy=Darkwell::PropLab::RelocationPolicy(GetWorld());
 if (Mode!=LastMode || Policy!=LastPolicy)
 {
  UE_LOG(LogDarkwellPropLab,Display,TEXT("PropLab MODE=%d POLICY=%d event=%s"),Mode,Policy,*LastEvent);
  LastMode=Mode; LastPolicy=Policy;
  for (UTextureRenderTarget2D* RT : SoftTargets) UKismetRenderingLibrary::ClearRenderTarget2D(this,RT);
 }
 if (Mode==2) UpdateSoftCoverage(DeltaSeconds);
 if (RawCoverage) for (TActorIterator<ADarkwellPropLabFurniture> It(GetWorld()); It; ++It)
  It->BindPresentation(RawCoverage,SoftTargets.Num()==2 ? SoftTargets[SoftIndex].Get() : RawCoverage.Get(),FogMin,FogInv,Mode);
 static const TCHAR* ModeNames[]={TEXT("AcceptedWholeObject"),TEXT("SurfaceSweepHard"),TEXT("SurfaceSweepSoft")};
 static const TCHAR* PolicyNames[]={TEXT("VerifyOldLocation"),TEXT("RecognizedIdentityRelocation")};
 if (GEngine) GEngine->AddOnScreenDebugMessage(0xDA471,0,FColor::Cyan,FString::Printf(TEXT("PROP LAB | MODE %d %s | POLICY %d %s | Route %d t=%.2f | ENEMY %d | %s"),Mode,ModeNames[Mode],Policy,PolicyNames[Policy],LastRoute,RouteTime,IsEnemyEnabled(),*LastEvent));
 CaptureEvidence();
}

void ADarkwellPropGameplayLab::CaptureEvidence()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
 if(FParse::Param(FCommandLine::Get(),TEXT("PropLabComparisonCapture"))) { CaptureComparisonEvidence(); return; }
 FString Relative;
 if(!FParse::Value(FCommandLine::Get(),TEXT("PropLabCapture="),Relative)) return;
 // Evidence is always local and ignored, regardless of caller-supplied label.
 Relative=FPaths::GetCleanFilename(Relative);
 const bool bLongRoute=LastRoute>=5;
 const float Duration=bLongRoute ? 21.f : 12.f;
 if(Elapsed>=2.f+CaptureIndex*.1f && Elapsed<Duration)
 {
  const FString Path=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("PropGameplayLab"),Relative,FString::Printf(TEXT("frame_%03d.png"),CaptureIndex++));
  FScreenshotRequest::RequestScreenshot(Path,true,false);
  auto* Adapter=GetWorld()->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
  auto* Memory=GetWorld()->GetSubsystem<UDarkwellRememberedPropSubsystem>();
  if(CaptureIndex==1)
  {
   auto* Navigation=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
   FNavLocation Projected;
   const bool bNavigationReady=Navigation && Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)
    && Navigation->ProjectPointToNavigation(FVector(0,-200,0),Projected,FVector(100,100,200));
   UE_LOG(LogDarkwellPropLab,Display,TEXT("LAB_NAV_READY=%d"),bNavigationReady);
   if(!bNavigationReady) UE_LOG(LogDarkwellPropLab,Error,TEXT("LAB_CONTRACT_FAIL navigation unavailable"));
  }
  if(!Adapter->IsSightWeaveAuthorityActive()) UE_LOG(LogDarkwellPropLab,Error,TEXT("LAB_CONTRACT_FAIL inactive authority"));
  for(const FName Id : {FName(TEXT("Lab.Fridge")),FName(TEXT("Lab.MobileCabinet")),FName(TEXT("Lab.TwinA")),FName(TEXT("Lab.TwinB")),FName(TEXT("Lab.DestroyBox")),FName(TEXT("Lab.ReplaceOld")),FName(TEXT("Lab.ReplaceNew"))})
  {
   bool Live=false,Valid=false; FVector Location; AActor* Proxy=nullptr;
   if(Memory->TryGetRecordForTesting(Id,Live,Valid,Location,Proxy))
   {
    UE_LOG(LogDarkwellPropLab,Display,TEXT("LAB_PROP frame=%d id=%s live=%d valid=%d at=(%.0f,%.0f) retired=%d"),CaptureIndex-1,*Id.ToString(),Live,Valid,Location.X,Location.Y,Memory->GetUnverifiedSnapshotCount(Id));
    if(LastPolicy==1 && Memory->GetUnverifiedSnapshotCount(Id)!=0) UE_LOG(LogDarkwellPropLab,Error,TEXT("LAB_CONTRACT_FAIL duplicate recognized identity"));
   }
  }
  for(TActorIterator<ADarkwellStalkerCharacter> It(GetWorld());It;++It)
  {
   FDarkwellVisibilitySubjectSnapshot Snapshot;
   const bool Found=Adapter->TryGetSubjectSnapshot(It->GetPersistentId(),Snapshot);
   const bool Hud=Found && Snapshot.bHardLive && Snapshot.AuthorityRevision==It->GetAppliedVisibilityAuthorityRevision();
   if(!Found || It->IsVisibleBySightWeaveAuthority()!=Snapshot.bHardLive || Hud!=Snapshot.bHardLive || It->IsHidden()==Snapshot.bHardLive)
    UE_LOG(LogDarkwellPropLab,Error,TEXT("LAB_CONTRACT_FAIL Stalker/HUD authority mismatch"));
   bool EnemyLive=false,EnemyValid=false; FVector EnemyMemory; AActor* EnemyProxy=nullptr;
   if(Memory->TryGetRecordForTesting(It->GetPersistentId(),EnemyLive,EnemyValid,EnemyMemory,EnemyProxy))
    UE_LOG(LogDarkwellPropLab,Error,TEXT("LAB_CONTRACT_FAIL NeverRemember enemy entered prop records"));
   UE_LOG(LogDarkwellPropLab,Display,TEXT("LAB_EVIDENCE frame=%d mode=%d policy=%d route=%d time=%.3f stalkerHard=%d hidden=%d hudEligible=%d authority=%llu memoryProps=%d"),
    CaptureIndex-1,LastMode,LastPolicy,LastRoute,Elapsed,Found&&Snapshot.bHardLive,It->IsHidden(),Hud,Snapshot.AuthorityRevision,Memory->GetDiagnostics().RegisteredCount);
  }
 }
 if(Elapsed>Duration+1 && (!CaptureWriter || CaptureWriter->Pending.load()==0))
 {
  if(CaptureWriter && CaptureWriter->Failed.load()!=0) UE_LOG(LogDarkwellPropLab,Error,TEXT("LAB_CONTRACT_FAIL screenshot write"));
  UE_LOG(LogDarkwellPropLab,Display,TEXT("LAB_CAPTURE_COMPLETE frames=%d"),CaptureIndex);
  UKismetSystemLibrary::QuitGame(this,nullptr,EQuitPreference::Quit,false);
 }
#endif
}

void ADarkwellPropGameplayLab::Event(const FString& Command)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
 if (!Darkwell::PropLab::IsLabWorld(GetWorld())) return;
 TArray<FString> Words; Command.ParseIntoArrayWS(Words);
 if (Words.Num()==2)
 {
  const int32 Value=FCString::Atoi(*Words[1]);
  if (Words[0]==TEXT("mode")) Darkwell::PropLab::Mode->Set(FMath::Clamp(Value,0,2),ECVF_SetByConsole);
  if (Words[0]==TEXT("policy")) Darkwell::PropLab::Policy->Set(FMath::Clamp(Value,0,1),ECVF_SetByConsole);
  if (Words[0]==TEXT("route")) { Darkwell::PropLab::Route->Set(FMath::Clamp(Value,0,13),ECVF_SetByConsole); LastRoute=-1; }
  if (Words[0]==TEXT("enemy")) SetEnemyEnabled(Value!=0);
 }
 if (Command==TEXT("reset")) { UGameplayStatics::OpenLevel(this,TEXT("/Game/Maps/L_ProjectFogPropGameplayLab")); return; }
 auto Find=[&](FName Id)->ADarkwellPropLabFurniture* { for(TActorIterator<ADarkwellPropLabFurniture> It(GetWorld());It;++It) if(It->StableId==Id) return *It; return nullptr; };
 if (Command==TEXT("fridge")) { if(auto* A=Find(TEXT("Lab.Fridge"))) A->SetActorLocation(FVector(650,340,0)); }
 if (Command==TEXT("cabinet")) { if(auto* A=Find(TEXT("Lab.MobileCabinet"))) A->SetActorLocation(FVector(720,-500,0)); }
 if (Command==TEXT("destroy")) { if(auto* A=Find(TEXT("Lab.DestroyBox"))) A->Destroy(); }
 if (Command==TEXT("swap"))
 {
  auto* A=Find(TEXT("Lab.TwinA")); auto* B=Find(TEXT("Lab.TwinB"));
  if(A && B) { const FTransform T=A->GetActorTransform(); A->SetActorTransform(B->GetActorTransform()); B->SetActorTransform(T); }
 }
 if (Command==TEXT("replace")) if(auto* A=Find(TEXT("Lab.ReplaceOld")))
 {
  const FTransform T=A->GetActorTransform(); A->Destroy();
  auto* B=GetWorld()->SpawnActorDeferred<ADarkwellPropLabFurniture>(ADarkwellPropLabFurniture::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  B->StableId=TEXT("Lab.ReplaceNew"); B->Shape=2; B->Dimensions=FVector(110,45,165); B->Tint=FLinearColor(.56f,.25f,.12f); B->Memory->bRememberFromStart=false; B->FinishSpawning(T);
 }
 if (auto* P=Cast<ADarkwellCharacter>(UGameplayStatics::GetPlayerPawn(this,0))) if(auto* L=P->GetLoadoutComponent())
 {
  if(Command==TEXT("torch") || Command==TEXT("lantern") || Command==TEXT("dark"))
  {
   bMaintainTools=Command!=TEXT("dark");
   L->RestorePersistentState(L->GetLoadedShells(),Command==TEXT("dark") ? 0 : 100,0,Command==TEXT("dark") ? 0 : 100,L->GetEquippedLeftHandItem(),
    Command==TEXT("lantern") ? DarkwellGameplayTags::Equipment_Right_Lantern.GetTag() : DarkwellGameplayTags::Equipment_Right_Torch.GetTag());
  }
 }
 LastEvent=Command;
 UE_LOG(LogDarkwellPropLab,Display,TEXT("PropLab EVENT %s mode=%d policy=%d"),*Command,Darkwell::PropLab::PresentationMode(GetWorld()),Darkwell::PropLab::RelocationPolicy(GetWorld()));
 if(Command==TEXT("help")) UE_LOG(LogDarkwellPropLab,Display,TEXT("Darkwell.PropLab: reset, fridge, cabinet, destroy, replace, swap, torch, lantern, dark; enemy 0/1 (default OFF); mode 0/1/2; policy 0/1; route 0..13 (0 manual;1 fixed-camera 30s comparison;2 oblique;3 rotate;4 parallel;5 A-first;6 B-first;7 tools;8 twins;9 destroy;10 replace;11 horizontal;12 vertical;13 diagonal). route 1 restarts without changing policy."));
#endif
}

void ADarkwellPropGameplayLab::RunRoute(float DeltaSeconds)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
 const int32 Route=Darkwell::PropLab::Route.GetValueOnGameThread();
 if(Route!=LastRoute)
 {
  if(Route>=1 && Route<=6) SetEnemyEnabled(false);
  if(auto* P=Cast<ADarkwellCharacter>(UGameplayStatics::GetPlayerPawn(this,0)))
  {
   if(auto* Move=P->GetCharacterMovement()) { Move->StopMovementImmediately(); Move->SetMovementMode(Route==0 ? MOVE_Walking : MOVE_None); }
   if(auto* Boom=P->FindComponentByClass<USpringArmComponent>())
   { Boom->SetRelativeRotation(FRotator(-65,90,0)); Boom->TargetArmLength=(Route==1 || Route==3 || Route>=11) ? 1250 : 1450; Boom->TargetOffset=(Route==1 || Route==3 || Route>=11) ? FVector(0,400,0) : FVector::ZeroVector; }
  }
  for(UTextureRenderTarget2D* RT : SoftTargets) UKismetRenderingLibrary::ClearRenderTarget2D(this,RT);
  for(TActorIterator<ADarkwellStalkerCharacter> It(GetWorld());It;++It)
   if(auto* AI=Cast<AAIController>(It->GetController())) { AI->SetActorTickEnabled(Route==0); AI->StopMovement(); }
  LastRoute=Route; RouteTime=0; UE_LOG(LogDarkwellPropLab,Display,TEXT("PropLab ROUTE %d"),Route);
 }
 if(Route==0) return;
 const float PreviousTime=RouteTime; RouteTime+=DeltaSeconds;
 auto* P=Cast<ADarkwellCharacter>(UGameplayStatics::GetPlayerPawn(this,0)); if(!P) return;
 FVector Position(-520,-190,92); float Yaw=90;
 const float T=FMath::Clamp((RouteTime-2)/8,0.f,1.f);
 if(Route==1) { Position=Darkwell::PropLab::ComparisonPlayerPosition(); Yaw=Darkwell::PropLab::ComparisonYaw(RouteTime); }
 if(Route==2) { Position=FVector(-80,-50,92); Yaw=FMath::Lerp(60.f,170.f,T); }
 if(Route==3) { Position=Darkwell::PropLab::ComparisonPlayerPosition(); Yaw=360*T; }
 if(Route==4) { Position.X=FMath::Lerp(-860.f,-170.f,T); Yaw=90; }
 if(Route>=11)
 {
  Position=Darkwell::PropLab::ComparisonPlayerPosition();
  const float Offset=150*FMath::Sin(2*PI*FMath::Clamp(RouteTime/12,0.f,1.f));
  if(Route==11 || Route==13) Position.X+=Offset;
  if(Route==12 || Route==13) Position.Y+=Offset;
  Yaw=40;
 }
 if(Route>=5 && Route<=10)
 {
  const int32 Phase=FMath::Min(4,FMath::FloorToInt(RouteTime/4));
  if(Route==7)
  {
   if(FMath::Min(4,FMath::FloorToInt(PreviousTime/4))!=Phase) Event(Phase==1 ? TEXT("lantern") : Phase==2 ? TEXT("dark") : TEXT("torch"));
   Yaw=90+35*FMath::Sin(RouteTime);
  }
  else
  {
   if(PreviousTime<4 && RouteTime>=4) Event(Route==8 ? TEXT("swap") : Route==9 ? TEXT("destroy") : Route==10 ? TEXT("replace") : TEXT("fridge"));
   Position=Phase<=1 ? FVector(0,-650,92) : FVector(Phase==2 ? (Route==5 ? -250 : 650) : (Route==5 ? 650 : -250),50,92);
   Yaw=Phase<=1 ? -90 : 90;
   if(Route>=8 && Phase>=2)
   {
    Position=FVector(Route==8 ? 250 : Route==9 ? 410 : 860,-100,92); Yaw=90;
   }
  }
 }
 P->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
 P->SetActorRotation(FRotator(0,Yaw,0));
 P->AimAtWorldPoint(Position+FRotator(0,Yaw,0).Vector()*1000);
 if(auto* C=P->GetController()) C->SetControlRotation(FRotator(0,Yaw,0));
 for(TActorIterator<ADarkwellStalkerCharacter> It(GetWorld());It;++It)
 {
  if(auto* AI=Cast<AAIController>(It->GetController())) if(AI->GetBrainComponent()) AI->GetBrainComponent()->StopLogic(TEXT("Lab reproducible route"));
  if(auto* Move=It->GetCharacterMovement()) Move->StopMovementImmediately();
  const bool bPositiveThreatControl=Route==7 && RouteTime>=12 && RouteTime<16;
  It->SetActorLocation(bPositiveThreatControl ? Position+FRotator(0,Yaw,0).Vector()*150
   : FVector(-720+60*FMath::Sin(RouteTime),460,92));
 }
#endif
}

void ADarkwellPropGameplayLab::SetEnemyEnabled(bool bEnabled)
{
 if(!Darkwell::PropLab::IsLabWorld(GetWorld())) return;
 if(bEnabled)
 {
  if(LastRoute>=1 && LastRoute<=6) { UE_LOG(LogDarkwellPropLab,Display,TEXT("PropLab enemy rejected: routes 1-6 are enemy-free")); return; }
  if(!LabEnemy.IsValid())
  {
   const FTransform Transform(FVector(-700,460,100));
   auto* Enemy=GetWorld()->SpawnActorDeferred<ADarkwellStalkerCharacter>(ADarkwellStalkerCharacter::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   if(Enemy) { Enemy->ConfigurePersistentId(TEXT("Lab.Stalker.NeverRemember")); Enemy->FinishSpawning(Transform); LabEnemy=Enemy; }
  }
 }
 else
 {
  for(TActorIterator<ADarkwellStalkerCharacter> It(GetWorld());It;++It) It->Destroy();
  LabEnemy.Reset();
 }
 UE_LOG(LogDarkwellPropLab,Display,TEXT("PropLab ENEMY=%d (explicit opt-in only)"),IsEnemyEnabled());
}

void ADarkwellPropGameplayLab::RestoreComparisonTools(bool bForceTorch)
{
 if(!Darkwell::PropLab::IsLabWorld(GetWorld()) || (!bMaintainTools && !bForceTorch)) return;
 if(auto* Player=Cast<ADarkwellCharacter>(UGameplayStatics::GetPlayerPawn(this,0)))
 {
  auto* L=Player->GetLoadoutComponent();
  // Lab-only fixed tool state; no production durability/weapon rule changes.
  L->RestorePersistentState(L->GetLoadedShells(),100,0,100,L->GetEquippedLeftHandItem(),
   bForceTorch ? DarkwellGameplayTags::Equipment_Right_Torch.GetTag() : L->GetEquippedRightHandItem());
 }
}

void ADarkwellPropGameplayLab::CaptureComparisonEvidence()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
 FString Relative;
 if(!FParse::Value(FCommandLine::Get(),TEXT("PropLabCapture="),Relative)) return;
 Relative=FPaths::GetCleanFilename(Relative);
 if(LastRoute<0) return;
 float FPS=30; FParse::Value(FCommandLine::Get(),TEXT("FPS="),FPS);
 const int32 Total=FMath::RoundToInt((LastRoute==1 ? 30.f : 12.f)*FPS);
 if(CaptureIndex<Total)
 {
  auto* Player=Cast<ADarkwellCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
  auto* Fog=GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>();
  auto* Memory=GetWorld()->GetSubsystem<UDarkwellRememberedPropSubsystem>();
  auto* Adapter=GetWorld()->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
  if(!Player || !Fog || !Memory || !Adapter || !Adapter->IsSightWeaveAuthorityActive())
  { UE_LOG(LogDarkwellPropLab,Error,TEXT("LAB_CONTRACT_FAIL comparison authority setup")); UKismetSystemLibrary::QuitGame(this,nullptr,EQuitPreference::Quit,false); return; }
  int32 EnemyCount=0;
  for(TActorIterator<ADarkwellStalkerCharacter> It(GetWorld());It;++It) ++EnemyCount;
  if(EnemyCount) UE_LOG(LogDarkwellPropLab,Error,TEXT("LAB_CONTRACT_FAIL enemy in comparison"));
  for(TActorIterator<ADarkwellPropLabFurniture> It(GetWorld());It;++It)
  {
   bool Live=false,Valid=false; FVector Remembered; AActor* Proxy=nullptr;
   Memory->TryGetRecordForTesting(It->StableId,Live,Valid,Remembered,Proxy);
   int32 Visible=0;
   for(UStaticMeshComponent* Part : It->Memory->GetMemoryPrimitives())
   {
    Visible+=Part->IsVisible();
    const FVector C=Part->Bounds.Origin,E=Part->Bounds.BoxExtent;
    float Maximum=Fog->EvaluateLiveCoverageAtWorldPoint(FVector2D(C));
    for(double X : {-1.0,1.0}) for(double Y : {-1.0,1.0}) Maximum=FMath::Max(Maximum,Fog->EvaluateLiveCoverageAtWorldPoint(FVector2D(C.X+X*E.X,C.Y+Y*E.Y)));
    if(CaptureIndex==0 || It->StableId==TEXT("Lab.Island"))
     UE_LOG(LogDarkwellPropLab,Display,TEXT("LAB_PART frame=%d id=%s part=%s live=%d visible=%d coverage=%.6f transform=%s bounds=%s extent=%s material=%s"),
      CaptureIndex,*It->StableId.ToString(),*Part->GetName(),Live,Part->IsVisible(),Maximum,*Part->GetComponentTransform().ToHumanReadableString().Replace(TEXT("\n"),TEXT(" ")),*C.ToString(),*E.ToString(),*GetNameSafe(Part->GetMaterial(0)));
   }
   if(Visible!=0 && Visible!=It->Memory->GetMemoryPrimitives().Num()) UE_LOG(LogDarkwellPropLab,Error,TEXT("LAB_CONTRACT_FAIL independent component visibility id=%s"),*It->StableId.ToString());
   if(LastMode==0 && (Visible>0)!=Live) UE_LOG(LogDarkwellPropLab,Error,TEXT("LAB_CONTRACT_FAIL whole-object identity visibility"));
   if(It->StableId!=TEXT("Lab.Island")) continue;
   if(It->Memory->GetMemoryPrimitives().Num()!=1) UE_LOG(LogDarkwellPropLab,Error,TEXT("LAB_CONTRACT_FAIL island is not single geometry"));
   const FVector C=It->Parts[0]->Bounds.Origin,E=It->Parts[0]->Bounds.BoxExtent;
   float Mean=0; int32 Covered=0;
   for(int32 X=0;X<201;++X) for(int32 Y=0;Y<9;++Y)
   {
    const float Raw=Fog->EvaluateLiveCoverageAtWorldPoint(FVector2D(C.X-E.X+2*E.X*X/200,C.Y-E.Y+2*E.Y*Y/8));
    Mean+=Raw; Covered+=(Raw>=.5f);
   }
   const FVector Camera=Player->GetTopDownCamera()->GetComponentLocation();
   const FVector P=Player->GetActorLocation();
   UE_LOG(LogDarkwellPropLab,Display,TEXT("LAB_COMPARE frame=%d mode=%d policy=%d route=%d time=%.6f yaw=%.6f player=(%.3f,%.3f,%.3f) camera=(%.3f,%.3f,%.3f) rawMean=%.6f covered=%.6f live=%d visible=%d parts=%d enemy=%d torch=%.3f health=%.3f"),
    CaptureIndex,LastMode,LastPolicy,LastRoute,RouteTime,Player->GetActorRotation().Yaw,P.X,P.Y,P.Z,Camera.X,Camera.Y,Camera.Z,Mean/1809,Covered/1809.f,Live,Visible,It->Memory->GetMemoryPrimitives().Num(),EnemyCount,Player->GetLoadoutComponent()->GetTorchCharge(),Player->GetHealth());
   if(auto* Controller=Cast<APlayerController>(Player->GetController()))
   {
    FString Points;
    for(const FVector2D Sign : {FVector2D(-1,-1),FVector2D(1,-1),FVector2D(1,1),FVector2D(-1,1)})
    {
     FVector2D Screen; Controller->ProjectWorldLocationToScreen(FVector(C.X+Sign.X*E.X,C.Y+Sign.Y*E.Y,C.Z+E.Z),Screen);
     Points+=FString::Printf(TEXT(" %.3f,%.3f"),Screen.X,Screen.Y);
    }
    UE_LOG(LogDarkwellPropLab,Display,TEXT("LAB_TOP frame=%d%s"),CaptureIndex,*Points);
   }
  }
  FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("PropGameplayLab"),Relative,FString::Printf(TEXT("frame_%03d.png"),CaptureIndex++)),true,false);
 }
 else if(!CaptureWriter || CaptureWriter->Pending.load()==0)
 {
  if(CaptureWriter && CaptureWriter->Failed.load()!=0) UE_LOG(LogDarkwellPropLab,Error,TEXT("LAB_CONTRACT_FAIL screenshot write"));
  UE_LOG(LogDarkwellPropLab,Display,TEXT("LAB_CAPTURE_COMPLETE frames=%d"),CaptureIndex);
  UKismetSystemLibrary::QuitGame(this,nullptr,EQuitPreference::Quit,false);
 }
#endif
}
