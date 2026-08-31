#include "VisionPresentation/DarkwellStalePropLabComponent.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "VisionPresentation/DarkwellRememberedPropSubsystem.h"
#include "VisionPresentation/DarkwellRememberablePropComponent.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "AI/DarkwellStalkerCharacter.h"
#include "Player/DarkwellCharacter.h"
#include "Combat/DarkwellLoadoutComponent.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NativeGameplayTags.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogDarkwellStaleLab,Log,All);
namespace Darkwell::StaleLab
{
 UE_DEFINE_GAMEPLAY_TAG_STATIC(Observing,"Lab.StaleProp.Phase.ObservingInitialObject");
 UE_DEFINE_GAMEPLAY_TAG_STATIC(Remembered,"Lab.StaleProp.Phase.Remembered");
 UE_DEFINE_GAMEPLAY_TAG_STATIC(Mutated,"Lab.StaleProp.Phase.MutatedOffscreen");
 UE_DEFINE_GAMEPLAY_TAG_STATIC(Rescanning,"Lab.StaleProp.Phase.RescanningOldLocation");
 UE_DEFINE_GAMEPLAY_TAG_STATIC(Verified,"Lab.StaleProp.Phase.VerifiedEmpty");
 void CVar(const TCHAR* Name,int32 Value) { if(auto* V=IConsoleManager::Get().FindConsoleVariable(Name)) V->Set(Value,ECVF_SetByConsole); }
}

float UDarkwellStalePropLabComponent::ScanYaw(float Time,int32 Case)
{
 float Yaw=-40;
 if(Time>=10 && Time<20) Yaw=FMath::Lerp(-40.f,90.f,(Time-10)/10);
 else if(Time>=20 && Time<22) Yaw=Case==4?90+4*FMath::Sin((Time-20)*2*PI):90;
 else if(Time>=22 && Time<32) Yaw=FMath::Lerp(90.f,220.f,(Time-22)/10);
 else if(Time>=32) Yaw=-90;
 if(Case==3 && Time>=10 && Time<32) Yaw=180-Yaw;
 return Yaw;
}

bool UDarkwellStalePropLabComponent::Start(int32 InMode,int32 InCase)
{
 if(!Darkwell::PropLab::IsLabWorld(GetWorld()) || InMode<0 || InMode>2 || InCase<0 || InCase>5) return false;
 Stop();
 Mode=InMode; Scenario=InCase; Seconds=0; Frame=0; bMutated=false; bCaptureComplete=false;
 GhostMaterials.Reset(); OpacityTexture=nullptr; Evidence=FDarkwellEmptyVerification();
 Phase=Darkwell::StaleLab::Observing; ClearReason=TEXT("NoEmptyEvidence");
 Darkwell::StaleLab::CVar(TEXT("r.Darkwell.ProjectFogVisual.PropPresentationMode"),Mode);
 Darkwell::StaleLab::CVar(TEXT("r.Darkwell.ProjectFogVisual.PropRelocationPolicy"),0);
 Darkwell::StaleLab::CVar(TEXT("r.Darkwell.ProjectFogVisual.LabRoute"),0);
 for(TActorIterator<ADarkwellStalkerCharacter> It(GetWorld());It;++It) It->Destroy();
 for(TActorIterator<ADarkwellPropLabFurniture> It(GetWorld());It;++It)
 { CollisionBackup.Add(*It,It->GetActorEnableCollision()); It->SetActorEnableCollision(false); }
 StableId=*FString::Printf(TEXT("Lab.Stale.%c"),TCHAR('A'+Scenario));
 OldPosition=Scenario==5?FVector(-230,-300,0):FVector(400,-300,0);
 Dimensions=Scenario==0?FVector(60,50,45):Scenario==1?FVector(82,76,190):FVector(1400,110,90);
 if(Scenario==5) Dimensions.X=1000;
 GetWorld()->GetSubsystem<UDarkwellRememberedPropSubsystem>()->SetLabVerificationSubject(StableId);
 SpawnSubject();
 bRunning=true;
 if(auto* Player=Cast<ADarkwellCharacter>(UGameplayStatics::GetPlayerPawn(this,0)))
 {
  Player->RestorePersistentState(Player->GetActorTransform(),Player->GetMaxHealth(),DarkwellGameplayTags::State_Player_Alive,FGameplayTag());
  Player->GetLoadoutComponent()->RestorePersistentState(2,100,0,100,DarkwellGameplayTags::Equipment_Left_Shotgun.GetTag(),DarkwellGameplayTags::Equipment_Right_Torch.GetTag());
 }
 SetView(90,true);
 UE_LOG(LogDarkwellStaleLab,Display,TEXT("STALE_START mode=%d policy=0 case=%c duration=36 id=%s enemy=0 grid=10cm legal=.99 all5 dwell=.10 whole=100percent fade=.20"),Mode,TCHAR('A'+Scenario),*StableId.ToString());
 return true;
}

void UDarkwellStalePropLabComponent::SpawnSubject()
{
 const FTransform Transform(OldPosition);
 Subject=GetWorld()->SpawnActorDeferred<ADarkwellPropLabFurniture>(ADarkwellPropLabFurniture::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 Subject->StableId=StableId; Subject->Shape=Scenario==1?1:Scenario==0?3:4;
 Subject->Dimensions=Dimensions; Subject->Tint=FLinearColor(.24f,.40f,.56f);
 Subject->Memory->bRememberFromStart=false;
 Subject->FinishSpawning(Transform);
}

void UDarkwellStalePropLabComponent::Stop()
{
 if(!Darkwell::PropLab::IsLabWorld(GetWorld())) return;
 if(IsValid(Subject)) Subject->Destroy();
 Subject=nullptr; Ghost.Reset();
 if(auto* Memory=GetWorld()->GetSubsystem<UDarkwellRememberedPropSubsystem>()) Memory->ReleaseLabVerificationSubject();
 for(auto& Pair:CollisionBackup) if(Pair.Key.IsValid()) Pair.Key->SetActorEnableCollision(Pair.Value);
 CollisionBackup.Reset(); bRunning=false;
 if(auto* Player=Cast<ADarkwellCharacter>(UGameplayStatics::GetPlayerPawn(this,0)))
 {
  Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
  Player->RestorePersistentState(Player->GetActorTransform(),Player->GetMaxHealth(),DarkwellGameplayTags::State_Player_Alive,FGameplayTag());
  Player->GetLoadoutComponent()->RestorePersistentState(2,100,0,100,DarkwellGameplayTags::Equipment_Left_Shotgun.GetTag(),DarkwellGameplayTags::Equipment_Right_Torch.GetTag());
 }
}

void UDarkwellStalePropLabComponent::SetView(float Yaw,bool bRemembering)
{
 auto* Player=Cast<ADarkwellCharacter>(UGameplayStatics::GetPlayerPawn(this,0)); if(!Player) return;
 FVector Position(OldPosition.X,-700,92);
 // F is first remembered from the open side, then tested from behind the existing half-wall.
 if(Scenario==5) Position.Y=bRemembering?0:-850;
 Player->GetCharacterMovement()->StopMovementImmediately(); Player->GetCharacterMovement()->SetMovementMode(MOVE_None);
 Player->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
 Player->SetActorRotation(FRotator(0,Yaw,0)); Player->AimAtWorldPoint(Position+FRotator(0,Yaw,0).Vector()*1000);
 if(auto* Controller=Player->GetController()) Controller->SetControlRotation(FRotator(0,Yaw,0));
 if(auto* Boom=Player->FindComponentByClass<USpringArmComponent>())
 { Boom->SetRelativeRotation(FRotator(-65,90,0)); Boom->TargetArmLength=Scenario<2?700:1250; Boom->TargetOffset=FVector(0,400,0); }
 Player->GetLoadoutComponent()->RestorePersistentState(2,100,0,100,DarkwellGameplayTags::Equipment_Left_Shotgun.GetTag(),DarkwellGameplayTags::Equipment_Right_Torch.GetTag());
}

void UDarkwellStalePropLabComponent::BeforeActors(float DeltaSeconds)
{
 if(!bRunning) return;
 if(!GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>()->IsActive()) return;
 Darkwell::StaleLab::CVar(TEXT("r.Darkwell.ProjectFogVisual.PropPresentationMode"),Mode);
 Darkwell::StaleLab::CVar(TEXT("r.Darkwell.ProjectFogVisual.PropRelocationPolicy"),0);
 for(TActorIterator<ADarkwellStalkerCharacter> It(GetWorld());It;++It) It->Destroy();
 Seconds+=DeltaSeconds;
 if(Seconds>=6 && Seconds<8) Phase=Darkwell::StaleLab::Remembered;
 if(Seconds<6) SetView((Scenario==5?-90.f:90.f)+70*FMath::Sin(Seconds*PI/3),true);
 else SetView(Seconds<10?-90:ScanYaw(Seconds,Scenario),false);
 // Freeze the observed A record BEFORE offscreen mutation and ordinary refresh.
 if(Seconds>=8 && !bMutated)
 {
  if(!PrepareGhost()) { UE_LOG(LogDarkwellStaleLab,Error,TEXT("STALE_FAIL missing legally remembered snapshot")); Stop(); return; }
  if(Scenario==0) { Subject->Destroy(); Subject=nullptr; }
  else Subject->SetActorLocation(FVector(3000,3000,0),false,nullptr,ETeleportType::TeleportPhysics);
  bMutated=true; Phase=Darkwell::StaleLab::Mutated;
  UE_LOG(LogDarkwellStaleLab,Display,TEXT("STALE_MUTATION t=%.6f id=%s event=%s B=(3000,3000)"),Seconds,*StableId.ToString(),Scenario==0?TEXT("destroy"):TEXT("move"));
 }
}

bool UDarkwellStalePropLabComponent::PrepareGhost()
{
 Ghost=GetWorld()->GetSubsystem<UDarkwellRememberedPropSubsystem>()->FreezeLabVerificationSnapshot();
 if(!Ghost.IsValid()) return false;
 FBox Bounds(ForceInit);
 TInlineComponentArray<UStaticMeshComponent*> Meshes(Ghost.Get());
 for(auto* Mesh:Meshes) Bounds+=Mesh->Bounds.GetBox();
 Evidence.Initialize(FBox2D(FVector2D(Bounds.Min),FVector2D(Bounds.Max)));
 OpacityTexture=UTexture2D::CreateTransient(Evidence.Size.X,Evidence.Size.Y,PF_G8);
 OpacityTexture->SRGB=false; OpacityTexture->Filter=TF_Nearest;
 OpacityTexture->AddressX=TA_Clamp; OpacityTexture->AddressY=TA_Clamp;
 OpacityTexture->NeverStream=true; OpacityTexture->UpdateResource();
 auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Darkwell/Vision/PropLab/M_PropLabStaleMemory.M_PropLabStaleMemory"));
 if(!Parent) return false;
 for(auto* Mesh:Meshes)
 {
  auto* MID=UMaterialInstanceDynamic::Create(Parent,this);
  MID->SetScalarParameterValue(TEXT("ForceRemembered"),1);
  MID->SetVectorParameterValue(TEXT("OriginalBaseColorTint"),FLinearColor(.24f,.40f,.56f));
  MID->SetScalarParameterValue(TEXT("OriginalUVScale"),1);
  MID->SetTextureParameterValue(TEXT("StaleOpacity"),OpacityTexture);
  const FVector2D Min=Evidence.Bounds.Min, Inv=FVector2D(1,1)/Evidence.Bounds.GetSize();
  MID->SetVectorParameterValue(TEXT("StaleMinInv"),FLinearColor(Min.X,Min.Y,Inv.X,Inv.Y));
  Mesh->SetMaterial(0,MID); GhostMaterials.Add(MID);
 }
 UpdateOpacity(); return true;
}

void UDarkwellStalePropLabComponent::ObserveEmpty(float DeltaSeconds)
{
 auto* Fog=GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>();
 TArray<FBox2D> Occupied;
 // Only actual collision-enabled furniture occupies this isolated lane. Hidden
 // original comparison props have collision disabled and are restored on reset.
 for(TActorIterator<ADarkwellPropLabFurniture> It(GetWorld());It;++It) if(It->GetActorEnableCollision())
  for(UStaticMeshComponent* Mesh:It->Memory->GetMemoryPrimitives())
  { const FBox Box=Mesh->Bounds.GetBox(); Occupied.Add(FBox2D(FVector2D(Box.Min),FVector2D(Box.Max))); }
 Evidence.Observe(DeltaSeconds,Seconds,[Fog](FVector2D P){return Fog->EvaluateLiveCoverageAtWorldPoint(P);},
  [&Occupied](const FBox2D& Box){return Occupied.ContainsByPredicate([&](const FBox2D& Other){return Other.Intersect(Box);});});
 if(Evidence.IsObjectEmpty()) { Phase=Darkwell::StaleLab::Verified; ClearReason=TEXT("AllCellsLegalUnoccludedEmpty100pct"); }
 else { Phase=Darkwell::StaleLab::Rescanning; ClearReason=Evidence.VerifiedFraction()>0?TEXT("PartialEvidence_UnverifiedCellsRetained"):TEXT("NoEmptyEvidence"); }
}

void UDarkwellStalePropLabComponent::UpdateOpacity()
{
 if(!OpacityTexture || Evidence.Cells.IsEmpty()) return;
 uint8* Bytes=new uint8[Evidence.Cells.Num()]; bool bAny=false;
 const float WholeOpacity=Evidence.IsObjectEmpty()?0.f:1.f;
 for(int32 I=0;I<Evidence.Cells.Num();++I)
 { const float Value=Mode==0?WholeOpacity:Evidence.Opacity(I,Mode,Seconds); Bytes[I]=uint8(FMath::RoundToInt(255*Value)); bAny|=Value>0; }
 auto* Region=new FUpdateTextureRegion2D(0,0,0,0,Evidence.Size.X,Evidence.Size.Y);
 OpacityTexture->UpdateTextureRegions(0,1,Region,Evidence.Size.X,1,Bytes,
  [](uint8* Data,const FUpdateTextureRegion2D* Regions){delete[] Data; delete Regions;});
 if(!bAny && Ghost.IsValid())
 { GetWorld()->GetSubsystem<UDarkwellRememberedPropSubsystem>()->FinishLabVerificationSnapshot(); Ghost.Reset(); }
}

void UDarkwellStalePropLabComponent::AfterActors(float DeltaSeconds)
{
 if(!bRunning) { if(!Status.IsEmpty() && GEngine) GEngine->AddOnScreenDebugMessage(0xDA472,0,FColor::Cyan,Status); return; }
 if(bMutated && Seconds>=10) ObserveEmpty(DeltaSeconds);
 if(bMutated) UpdateOpacity();
 Report(true);
 if(Seconds>=36)
 {
  UE_LOG(LogDarkwellStaleLab,Display,TEXT("STALE_COMPLETE mode=%d case=%c verified=%.6f whole=%d ghost=%d frames=%d reason=%s"),Mode,TCHAR('A'+Scenario),Evidence.VerifiedFraction(),Evidence.IsObjectEmpty(),Ghost.IsValid(),Frame,*ClearReason);
  if((Scenario!=5 && !Evidence.IsObjectEmpty()) || (Scenario==5 && (Evidence.IsObjectEmpty() || Evidence.VerifiedFraction()<=0)))
   UE_LOG(LogDarkwellStaleLab,Error,TEXT("STALE_FAIL unexpected terminal evidence"));
  Status=FString::Printf(TEXT("STALE ROUND FINISHED | MODE %d POLICY 0 CASE %c | %.1f%% empty=%d ghost=%d | %s | Reset to stable Lab; ENEMY 0"),Mode,TCHAR('A'+Scenario),100*Evidence.VerifiedFraction(),Evidence.IsObjectEmpty(),Ghost.IsValid(),*ClearReason);
  Stop(); bCaptureComplete=true;
 }
}

void UDarkwellStalePropLabComponent::Report(bool bCapture)
{
 bool Live=false,Valid=false; FVector At; AActor* Proxy=nullptr;
 GetWorld()->GetSubsystem<UDarkwellRememberedPropSubsystem>()->TryGetRecordForTesting(StableId,Live,Valid,At,Proxy);
 const bool bGhostPresent=Proxy && !Proxy->IsHidden();
 const FString PhaseName=Phase.ToString().Replace(TEXT("Lab.StaleProp.Phase."),TEXT(""));
 Status=FString::Printf(TEXT("STALE LAB | MODE %d POLICY 0 | CASE %c t=%.2f | %s | ENEMY 0\n%s | verified %.1f%% | object empty %d | ghost %d | %s"),Mode,TCHAR('A'+Scenario),Seconds,*PhaseName,*StableId.ToString(),Evidence.VerifiedFraction()*100,Evidence.IsObjectEmpty(),bGhostPresent,*ClearReason);
 if(GEngine) GEngine->AddOnScreenDebugMessage(0xDA472,0,FColor::Cyan,Status);
 auto* Player=Cast<ADarkwellCharacter>(UGameplayStatics::GetPlayerPawn(this,0)); if(!Player) return;
 const FVector P=Player->GetActorLocation(), Camera=Player->GetTopDownCamera()->GetComponentLocation();
 int32 EnemyCount=0; for(TActorIterator<ADarkwellStalkerCharacter> It(GetWorld());It;++It) ++EnemyCount;
 if(EnemyCount || (bMutated && Live)) UE_LOG(LogDarkwellStaleLab,Error,TEXT("STALE_FAIL enemy=%d newLocationLive=%d"),EnemyCount,Live);
 float Opacity=Evidence.IsObjectEmpty()?0:1;
 if(Mode!=0 && !Evidence.Cells.IsEmpty())
 { Opacity=0; for(int32 I=0;I<Evidence.Cells.Num();++I) Opacity+=Evidence.Opacity(I,Mode,Seconds); Opacity/=Evidence.Cells.Num(); }
 UE_LOG(LogDarkwellStaleLab,Display,TEXT("STALE_FRAME n=%d mode=%d case=%c t=%.6f phase=%s verified=%.6f empty=%d ghost=%d opacity=%.6f live=%d yaw=%.6f player=(%.3f,%.3f,%.3f) camera=(%.3f,%.3f,%.3f) torch=%.2f enemy=%d"),Frame,Mode,TCHAR('A'+Scenario),Seconds,*PhaseName,Evidence.VerifiedFraction(),Evidence.IsObjectEmpty(),bMutated?Ghost.IsValid():Proxy!=nullptr,Opacity,Live,Player->GetActorRotation().Yaw,P.X,P.Y,P.Z,Camera.X,Camera.Y,Camera.Z,Player->GetLoadoutComponent()->GetTorchCharge(),EnemyCount);
 uint64 Hash=14695981039346656037ull;
 for(const auto& Cell:Evidence.Cells) { Hash^=Cell.VerifiedAt>=0?1ull:0ull; Hash*=1099511628211ull; }
 UE_LOG(LogDarkwellStaleLab,Display,TEXT("STALE_GRID n=%d size=%dx%d verifiedHash=%llu"),Frame,Evidence.Size.X,Evidence.Size.Y,Hash);
 FString Label;
 if(bCapture && FParse::Value(FCommandLine::Get(),TEXT("StaleLabCapture="),Label))
  FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("PropGameplayLab"),FPaths::GetCleanFilename(Label),FString::Printf(TEXT("frame_%04d.png"),Frame)),true,false);
 ++Frame;
}
