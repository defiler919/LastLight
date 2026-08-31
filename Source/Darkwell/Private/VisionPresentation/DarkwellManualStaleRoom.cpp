#include "VisionPresentation/DarkwellManualStaleRoom.h"
#include "VisionPresentation/DarkwellMode2SolidComponent.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "VisionPresentation/DarkwellRememberablePropComponent.h"
#include "VisionPresentation/DarkwellRememberedPropSubsystem.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NativeGameplayTags.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"
#include "Misc/CommandLine.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/DarkwellCharacter.h"
#include "Combat/DarkwellLoadoutComponent.h"
#include "Gameplay/DarkwellGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogDarkwellManualStale,Log,All);
namespace Darkwell::ManualStale
{
 UE_DEFINE_GAMEPLAY_TAG_STATIC(Armed,"Lab.ManualStale.Switch.Armed");
 UE_DEFINE_GAMEPLAY_TAG_STATIC(Waiting,"Lab.ManualStale.Switch.WaitingForExit");
 void SetCVar(const TCHAR* Name,int32 Value)
 { if(auto* Var=IConsoleManager::Get().FindConsoleVariable(Name)) Var->Set(Value,ECVF_SetByConsole); }
}

ADarkwellManualStaleRoom::ADarkwellManualStaleRoom()
{
 Mode2Presentation=CreateDefaultSubobject<UDarkwellMode2SolidComponent>(TEXT("Mode2SolidPresentation"));
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RoomRoot")));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
 const FVector Centers[]={{-25,0,-15},{-25,800,125},{-25,-800,125},{950,0,125},{125,0,125},{-700,-710,125},{-700,0,125},{-700,710,125},{-1000,0,125}};
 const FVector Sizes[]={{1950,1600,30},{1950,30,250},{1950,30,250},{30,1600,250},{1650,30,250},{30,180,250},{30,760,250},{30,180,250},{30,1600,250}};
 for(int32 I=0;I<UE_ARRAY_COUNT(Centers);++I)
 {
  auto* Part=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("RoomStructure%d"),I));
  Part->SetupAttachment(GetRootComponent()); Part->SetStaticMesh(Cube.Object);
  Part->SetRelativeLocation(Centers[I]); Part->SetRelativeScale3D(Sizes[I]/100);
  Part->SetCollisionProfileName(TEXT("BlockAll")); Part->SetRenderCustomDepth(false); Structure.Add(Part);
 }
 PressureDisc=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PressureSwitch"));
 PressureDisc->SetupAttachment(GetRootComponent()); PressureDisc->SetStaticMesh(Cylinder.Object);
 PressureDisc->SetRelativeLocation(FVector(500,-450,3)); PressureDisc->SetRelativeScale3D(FVector(2,2,.06));
 PressureDisc->SetCollisionEnabled(ECollisionEnabled::NoCollision); PressureDisc->SetCastShadow(false);
 CabinetPreview=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EditorCabinetPreview"));
 CabinetPreview->SetupAttachment(GetRootComponent()); CabinetPreview->SetStaticMesh(Cube.Object);
 CabinetPreview->SetRelativeLocation(FVector(500,500,95)); CabinetPreview->SetRelativeScale3D(FVector(4.4,1.6,1.9));
 CabinetPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 PressureState=Darkwell::ManualStale::Armed;
}

ADarkwellManualStaleRoom* ADarkwellManualStaleRoom::FindActive(const UWorld* World)
{
 if(!Darkwell::PropLab::IsLabWorld(World) || World->URL.HasOption(TEXT("PropLabOriginal"))) return nullptr;
 for(const TCHAR* Flag:{TEXT("StaleLabAuto"),TEXT("PropLabCapture="),TEXT("PropLabComparisonCapture")})
  if(FCString::Stristr(FCommandLine::Get(),Flag)) return nullptr;
 for(TActorIterator<ADarkwellManualStaleRoom> It(World);It;++It) return *It;
 return nullptr;
}
void ADarkwellManualStaleRoom::BeginPlay()
{
 Super::BeginPlay(); CabinetPreview->SetVisibility(false);
 const bool bActive=FindActive(GetWorld())==this;
 SetActorHiddenInGame(!bActive); SetActorEnableCollision(bActive);
}
FName ADarkwellManualStaleRoom::CabinetId() { return TEXT("Lab.ManualStale.Cabinet"); }
bool ADarkwellManualStaleRoom::HasActualCabinet() const { return IsValid(Cabinet); }
bool ADarkwellManualStaleRoom::IsSwitchArmed() const { return PressureState==Darkwell::ManualStale::Armed; }
float ADarkwellManualStaleRoom::GetRemainingOpacity() const
{
 if(!ObservedProxy.IsValid() || DisplayedOpacity.IsEmpty()) return 0;
 float Sum=0; for(float Value:DisplayedOpacity) Sum+=Value; return Sum/DisplayedOpacity.Num();
}
FBox2D ADarkwellManualStaleRoom::FloorBounds() const
{ return FBox2D(FVector2D(GetActorLocation())+FVector2D(-1050,-850),FVector2D(GetActorLocation())+FVector2D(1000,850)); }
void ADarkwellManualStaleRoom::BuildOccluders(TArray<FDarkwellVisionIntegrationSegment>& Out) const
{
 Out.Reset();
 for(int32 I=1;I<Structure.Num();++I)
 {
  const FVector P=Structure[I]->GetComponentLocation(),S=Structure[I]->GetComponentScale()*100;
  const FVector2D E=S.X>S.Y?FVector2D(S.X/2,0):FVector2D(0,S.Y/2);
  auto& Seg=Out.AddDefaulted_GetRef(); Seg.A=FVector2D(P)-E; Seg.B=FVector2D(P)+E; Seg.ZMin=0; Seg.ZMax=250;
 }
}
void ADarkwellManualStaleRoom::BindRoomPresentation(UTexture* Raw,FVector2D Min,FVector2D Inv)
{
 auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Darkwell/Vision/PropLab/M_PropLabSurface.M_PropLabSurface"));
 TArray<UStaticMeshComponent*> Parts; for(UStaticMeshComponent* Part:Structure) Parts.Add(Part); Parts.Add(PressureDisc);
 for(auto* Part:Parts)
 {
  auto* Mat=UMaterialInstanceDynamic::Create(Parent,this);
  Mat->SetTextureParameterValue(TEXT("DarkwellLiveCoverageTexture"),Raw);
  Mat->SetVectorParameterValue(TEXT("FogWorldMin"),FLinearColor(Min.X,Min.Y,0,0));
  Mat->SetVectorParameterValue(TEXT("FogWorldInvExtent"),FLinearColor(Inv.X,Inv.Y,0,0));
  Mat->SetVectorParameterValue(TEXT("OriginalBaseColorTint"),Part==PressureDisc?FLinearColor(.8f,.16f,.06f):FLinearColor(.28f,.32f,.35f));
  Mat->SetScalarParameterValue(TEXT("OriginalUVScale"),Part==Structure[0]?16:2);
  Part->SetMaterial(0,Mat); Materials.Add(Mat);
 }
}
void ADarkwellManualStaleRoom::SpawnActualCabinet()
{
 const FTransform Transform(CabinetPosition());
 Cabinet=GetWorld()->SpawnActorDeferred<ADarkwellPropLabFurniture>(ADarkwellPropLabFurniture::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 Cabinet->SetActorHiddenInGame(true); // Registration and the first authority query precede any source rendering.
 Cabinet->StableId=CabinetId(); Cabinet->Shape=0; Cabinet->Dimensions=FVector(440,160,190);
 Cabinet->bIndividualWorktop=false; Cabinet->Tint=FLinearColor(.14f,.48f,.25f); Cabinet->Memory->bRememberFromStart=false;
 Cabinet->FinishSpawning(Transform);
 if(!Cabinet->HasActorBegunPlay()) Cabinet->DispatchBeginPlay();
 Cabinet->Memory->ApplySourceLiveState(false); Cabinet->Memory->ApplySourceGeometryVisibility(false);
 Cabinet->SetActorHiddenInGame(false);
}
void ADarkwellManualStaleRoom::ToggleActualCabinet()
{
 // The pressure plate only changes physical existence. Destroy unregisters the
 // source while retaining its record; respawn reuses that StableID record.
 // Neither path clears, freezes, creates or invalidates a remembered snapshot.
 if(HasActualCabinet()) { Cabinet->Destroy(); Cabinet=nullptr; }
 else SpawnActualCabinet();
 ++ToggleCount;
 UE_LOG(LogDarkwellManualStale,Display,TEXT("MANUAL_SWITCH count=%d actual=%s cabinetCoverage=%.6f"),ToggleCount,HasActualCabinet()?TEXT("PRESENT"):TEXT("ABSENT"),GetCabinetCoverage());
}
bool ADarkwellManualStaleRoom::ResetRoom(ADarkwellCharacter* Player)
{
 if(FindActive(GetWorld())!=this || !Player) return false;
 Mode2Presentation->ResetPresentation();
 if(HasActualCabinet()) Cabinet->Destroy();
 auto* Memory=GetWorld()->GetSubsystem<UDarkwellRememberedPropSubsystem>();
 Memory->ReleaseLabVerificationSubject(); Memory->SetLabVerificationSubject(CabinetId(),true);
 Evidence=FDarkwellEmptyVerification(); DisplayedOpacity.Reset(); ObservedProxy.Reset(); OpacityTexture=nullptr;
 PressureState=Darkwell::ManualStale::Armed; ToggleCount=0; Seconds=0; bStarted=true;
 Darkwell::ManualStale::SetCVar(TEXT("r.Darkwell.ProjectFogVisual.PropRelocationPolicy"),0);
 Darkwell::ManualStale::SetCVar(TEXT("r.Darkwell.ProjectFogVisual.LabRoute"),0);
 Player->RestorePersistentState(Player->GetActorTransform(),Player->GetMaxHealth(),DarkwellGameplayTags::State_Player_Alive,FGameplayTag());
 Player->GetLoadoutComponent()->RestorePersistentState(2,100,0,100,DarkwellGameplayTags::Equipment_Left_Shotgun,DarkwellGameplayTags::Equipment_Right_Torch);
 // Per-instance laboratory tuning: freeze consumption, never refill per frame.
 // Reloading the original layout recreates the player with its original defaults.
 for(const TCHAR* Name:{TEXT("TorchDrainPerSecond"),TEXT("TorchDeterrentExtraDrainPerSecond"),TEXT("LanternBaseDrainPerSecond"),TEXT("LanternFocusExtraDrainPerSecond")})
  if(auto* Rate=FindFProperty<FFloatProperty>(Player->GetLoadoutComponent()->GetClass(),Name)) Rate->SetPropertyValue_InContainer(Player->GetLoadoutComponent(),0.f);
 SpawnActualCabinet(); TeleportPlayer(Player,true);
 UE_LOG(LogDarkwellManualStale,Display,TEXT("MANUAL_RESET identity=%s actual=PRESENT snapshot=EMPTY freeInput=1 noTimer=1"),*CabinetId().ToString());
 return true;
}
void ADarkwellManualStaleRoom::TeleportPlayer(ADarkwellCharacter* Player,bool bTop)
{
 if(!Player) return;
 Player->GetCharacterMovement()->StopMovementImmediately(); Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
 Player->SetActorLocation(GetActorLocation()+(bTop?FVector(500,150,92):FVector(500,-670,92)),false,nullptr,ETeleportType::TeleportPhysics);
 Player->SetActorRotation(FRotator(0,90,0));
 if(auto* Boom=Player->FindComponentByClass<USpringArmComponent>())
 { Boom->SetRelativeRotation(FRotator(-65,90,0)); Boom->TargetArmLength=1250; Boom->TargetOffset=FVector::ZeroVector; }
}
float ADarkwellManualStaleRoom::GetCabinetCoverage() const
{
 const auto* Fog=GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>(); if(!Fog) return 0;
 float Maximum=0;
 // Include the doors/handle footprint and interior, not only the actor origin.
 for(int32 X=0;X<=22;++X) for(int32 Y=0;Y<=9;++Y)
  Maximum=FMath::Max(Maximum,Fog->EvaluateLiveCoverageAtWorldPoint(FVector2D(CabinetPosition())+FVector2D(-220+20*X,-90+20*Y)));
 return Maximum;
}
void ADarkwellManualStaleRoom::AttachObservedSnapshot(AActor* Proxy)
{
 ObservedProxy=Proxy; FBox Bounds(ForceInit); TInlineComponentArray<UStaticMeshComponent*> Meshes(Proxy);
 for(auto* Mesh:Meshes) Bounds+=Mesh->Bounds.GetBox();
 Evidence.Initialize(FBox2D(FVector2D(Bounds.Min),FVector2D(Bounds.Max)));
 DisplayedOpacity.Init(1,Evidence.Cells.Num());
 OpacityTexture=UTexture2D::CreateTransient(Evidence.Size.X,Evidence.Size.Y,PF_G8);
 OpacityTexture->SRGB=false; OpacityTexture->Filter=TF_Nearest; OpacityTexture->AddressX=TA_Clamp; OpacityTexture->AddressY=TA_Clamp;
 OpacityTexture->NeverStream=true; OpacityTexture->UpdateResource();
 auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Darkwell/Vision/PropLab/M_PropLabStaleMemory.M_PropLabStaleMemory"));
 for(auto* Mesh:Meshes)
 {
  auto* Mat=UMaterialInstanceDynamic::Create(Parent,this); Mat->SetScalarParameterValue(TEXT("ForceRemembered"),1);
  Mat->SetVectorParameterValue(TEXT("OriginalBaseColorTint"),FLinearColor(.14f,.48f,.25f)); Mat->SetScalarParameterValue(TEXT("OriginalUVScale"),1);
  Mat->SetTextureParameterValue(TEXT("StaleOpacity"),OpacityTexture);
  const FVector2D Min=Evidence.Bounds.Min,Inv=FVector2D(1,1)/Evidence.Bounds.GetSize();
  Mat->SetVectorParameterValue(TEXT("StaleMinInv"),FLinearColor(Min.X,Min.Y,Inv.X,Inv.Y)); Mesh->SetMaterial(0,Mat);
 }
}
void ADarkwellManualStaleRoom::ApplyErasure(int32 Mode)
{
 if(!OpacityTexture || !ObservedProxy.IsValid()) return;
 uint8* Bytes=new uint8[Evidence.Cells.Num()]; bool bAny=false;
 for(int32 I=0;I<Evidence.Cells.Num();++I)
 {
  float Candidate=Evidence.Opacity(I,Mode,Seconds);
  // Switching to whole-object mode cannot resurrect already erased cells, and
  // a fade already in progress finishes instead of hanging at partial opacity.
  if(DisplayedOpacity[I]<1) Candidate=FMath::Min(Candidate,Evidence.Opacity(I,2,Seconds));
  DisplayedOpacity[I]=FMath::Min(DisplayedOpacity[I],Candidate);
  Bytes[I]=uint8(FMath::RoundToInt(255*DisplayedOpacity[I])); bAny|=DisplayedOpacity[I]>0;
 }
 auto* Region=new FUpdateTextureRegion2D(0,0,0,0,Evidence.Size.X,Evidence.Size.Y);
 OpacityTexture->UpdateTextureRegions(0,1,Region,Evidence.Size.X,1,Bytes,[](uint8* Data,const FUpdateTextureRegion2D* Regions){delete[] Data;delete Regions;});
 if(Evidence.IsObjectEmpty() && !bAny) GetWorld()->GetSubsystem<UDarkwellRememberedPropSubsystem>()->FinishLabVerificationSnapshot();
}
void ADarkwellManualStaleRoom::UpdateObservation(float DeltaSeconds,ADarkwellCharacter* Player)
{
 if(!bStarted || !Player || FindActive(GetWorld())!=this) return;
 Seconds+=DeltaSeconds;
 const FVector Offset=Player->GetActorLocation()-SwitchPosition();
 const bool bInside=FVector2D(Offset).SizeSquared()<=FMath::Square(SwitchRadius) && FMath::Abs(Offset.Z)<160;
 if(!bInside) PressureState=Darkwell::ManualStale::Armed;
 else if(IsSwitchArmed())
 {
  PressureState=Darkwell::ManualStale::Waiting;
  if(GetCabinetCoverage()>0) UE_LOG(LogDarkwellManualStale,Error,TEXT("MANUAL_FAIL pressure switch has legal cabinet coverage"));
  ToggleActualCabinet();
 }
 auto* Memory=GetWorld()->GetSubsystem<UDarkwellRememberedPropSubsystem>();
 bool Live=false,Valid=false; FVector At; AActor* Proxy=nullptr;
 Memory->TryGetRecordForTesting(CabinetId(),Live,Valid,At,Proxy);
 if(Valid && Proxy && ObservedProxy.Get()!=Proxy) AttachObservedSnapshot(Proxy);
 if(Valid && Proxy)
 {
  auto* Fog=GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>();
  Evidence.Observe(DeltaSeconds,Seconds,[Fog](FVector2D P){return Fog->EvaluateLiveCoverageAtWorldPoint(P);},
   [this](const FBox2D&){return HasActualCabinet() && Cabinet->GetActorEnableCollision();});
  ApplyErasure(Darkwell::PropLab::PresentationMode(GetWorld()));
 }
 // ApplyErasure alone owns the existing finish timing. The rendering component
 // only reads that result, and never creates/clears a snapshot or occupancy.
 Memory->TryGetRecordForTesting(CabinetId(),Live,Valid,At,Proxy);
 Mode2Presentation->Update(DeltaSeconds,HasActualCabinet()?Cabinet.Get():nullptr,Valid?Proxy:nullptr,Live,Evidence,DisplayedOpacity,Structure[0]);
 Report();
}
void ADarkwellManualStaleRoom::EnforceMode2Presentation(AActor* Snapshot)
{
 if(Darkwell::PropLab::PresentationMode(GetWorld())==2)
  Mode2Presentation->EnforceSource(HasActualCabinet()?Cabinet.Get():nullptr,Snapshot);
}
void ADarkwellManualStaleRoom::Report()
{
 bool Live=false,Valid=false; FVector At; AActor* Proxy=nullptr;
 GetWorld()->GetSubsystem<UDarkwellRememberedPropSubsystem>()->TryGetRecordForTesting(CabinetId(),Live,Valid,At,Proxy);
 Status=FString::Printf(TEXT("MANUAL STALE ROOM | Mode %d | Policy 0 | ENEMY 0\nCabinet Actual: %s | Remembered Snapshot: %s | StableID: %s\nOld Occupancy Verified: %.1f%% | Object Empty Confirmed: %d | Pressure Switch: %s\nLiveCoverage at Cabinet: %.6f | Source Live: %d | Toggles: %d\nFree movement / mouse aim; right corridor connects rooms. Darkwell.PropLab stalemanual help"),
  Darkwell::PropLab::PresentationMode(GetWorld()),HasActualCabinet()?TEXT("PRESENT"):TEXT("ABSENT"),Valid?TEXT("VALID"):TEXT("EMPTY"),*CabinetId().ToString(),
  100*Evidence.VerifiedFraction(),Evidence.IsObjectEmpty(),IsSwitchArmed()?TEXT("ARMED"):TEXT("WAITING_FOR_EXIT"),GetCabinetCoverage(),Live,ToggleCount);
 if(GEngine) GEngine->AddOnScreenDebugMessage(0xDA473,0,FColor::Cyan,Status);
}
void ADarkwellManualStaleRoom::Command(const TArray<FString>& Args)
{
 auto* Player=Cast<ADarkwellCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
 if(Args.Num()==2 && Args[1]==TEXT("reset")) ResetRoom(Player);
 else if(Args.Num()==3 && Args[1]==TEXT("mode") && (Args[2]==TEXT("0") || Args[2]==TEXT("1") || Args[2]==TEXT("2")))
  Darkwell::ManualStale::SetCVar(TEXT("r.Darkwell.ProjectFogVisual.PropPresentationMode"),FCString::Atoi(*Args[2]));
 else if(Args.Num()==3 && Args[1]==TEXT("teleport") && (Args[2]==TEXT("top") || Args[2]==TEXT("bottom"))) TeleportPlayer(Player,Args[2]==TEXT("top"));
 else UE_LOG(LogDarkwellManualStale,Display,TEXT("Darkwell.PropLab stalemanual reset | mode 0/1/2 | teleport top/bottom | help. Walk on the lower-left disc, leave it to rearm. No timer, forced movement or automatic restore. Mode changes affect remaining memory only. Darkwell.PropLab original opens the unchanged automatic-route layout."));
 UE_LOG(LogDarkwellManualStale,Display,TEXT("MANUAL_COMMAND %s mode=%d toggles=%d actual=%d verified=%.6f"),*FString::Join(Args,TEXT(" ")),Darkwell::PropLab::PresentationMode(GetWorld()),ToggleCount,HasActualCabinet(),Evidence.VerifiedFraction());
}
