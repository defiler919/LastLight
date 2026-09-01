#include "VisionPresentation/DarkwellManualStaleRoom.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "VisionPresentation/DarkwellRememberablePropComponent.h"
#include "VisionPresentation/DarkwellRememberedPropSubsystem.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Math/Float16Color.h"
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
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RoomRoot")));
 StaleCap=CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("Mode2StaleCutCap"));
 StaleCap->SetupAttachment(GetRootComponent());
 StaleCap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 StaleCap->SetGenerateOverlapEvents(false);
 StaleCap->SetCastShadow(false);
 StaleCap->SetReceivesDecals(false);
 StaleCap->SetVisibility(false);
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
 // Only the actual manual cabinet casts this shadow, including while the
 // existing authority hides it. Keep the same components/visibility path;
 // do not enable the unused furniture parts or any remembered proxy.
 for(UStaticMeshComponent* Part:Cabinet->Memory->GetMemoryPrimitives())
 { Part->SetCastShadow(true); Part->SetCastHiddenShadow(true); }
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
 if(HasActualCabinet()) Cabinet->Destroy();
 auto* Memory=GetWorld()->GetSubsystem<UDarkwellRememberedPropSubsystem>();
 Memory->ReleaseLabVerificationSubject(); Memory->SetLabVerificationSubject(CabinetId(),true);
 Evidence=FDarkwellEmptyVerification(); DisplayedOpacity.Reset(); ObservedProxy.Reset(); OpacityTexture=nullptr;
 SpatialMemory=FDarkwellSpatialPropMemory(); SpatialSource.Reset(); SpatialProxy.Reset(); SpatialTexture=nullptr;
 SpatialPresentationSize=FIntPoint::ZeroValue;
 SpatialProxyMaterials.Reset(); LegacyProxyMaterials.Reset();
 OriginalPartBounds.Reset(); ClearStaleCap();
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
 LegacyProxyMaterials.Reset();
 for(auto* Mesh:Meshes)
 {
  auto* Mat=UMaterialInstanceDynamic::Create(Parent,this); Mat->SetScalarParameterValue(TEXT("ForceRemembered"),1);
  Mat->SetVectorParameterValue(TEXT("OriginalBaseColorTint"),FLinearColor(.14f,.48f,.25f)); Mat->SetScalarParameterValue(TEXT("OriginalUVScale"),1);
  Mat->SetTextureParameterValue(TEXT("StaleOpacity"),OpacityTexture);
  const FVector2D Min=Evidence.Bounds.Min,Inv=FVector2D(1,1)/Evidence.Bounds.GetSize();
  Mat->SetVectorParameterValue(TEXT("StaleMinInv"),FLinearColor(Min.X,Min.Y,Inv.X,Inv.Y)); Mesh->SetMaterial(0,Mat);
  LegacyProxyMaterials.Add(Mat);
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
 UpdateSpatialMemory(DeltaSeconds);
 UpdateStaleCap(Darkwell::PropLab::PresentationMode(GetWorld()));
 bool Live=false,Valid=false; FVector At; AActor* Proxy=nullptr;
 Memory->TryGetRecordForTesting(CabinetId(),Live,Valid,At,Proxy);
 if(Valid && Proxy && ObservedProxy.Get()!=Proxy) AttachObservedSnapshot(Proxy);
 if(Valid && Proxy)
 {
  auto* Fog=GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>();
  Evidence.Observe(DeltaSeconds,Seconds,[Fog](FVector2D P){return Fog->EvaluateLiveCoverageAtWorldPoint(P);},
   [this](const FBox2D&){return HasActualCabinet() && Cabinet->GetActorEnableCollision();});
  ApplyErasure(Darkwell::PropLab::PresentationMode(GetWorld()));
  if(Darkwell::PropLab::PresentationMode(GetWorld())==2) BindSpatialProxy(Proxy);
  else
  {
   TInlineComponentArray<UStaticMeshComponent*> Meshes(Proxy);
   for(int32 I=0;I<Meshes.Num() && I<LegacyProxyMaterials.Num();++I)
    if(Meshes[I]->GetMaterial(0)!=LegacyProxyMaterials[I]) Meshes[I]->SetMaterial(0,LegacyProxyMaterials[I]);
  }
 }
 Report();
}
void ADarkwellManualStaleRoom::BindSpatialParameters(UMaterialInstanceDynamic* Material) const
{
 const FBox2D& Box=SpatialMemory.GetBounds();
 if(!SpatialTexture || !Box.bIsValid) { Material->SetScalarParameterValue(TEXT("SpatialReady"),0); return; }
 const FVector2D Inv=FVector2D(1,1)/Box.GetSize();
 Material->SetTextureParameterValue(TEXT("SpatialStateTexture"),SpatialTexture);
 Material->SetVectorParameterValue(TEXT("SpatialMinInv"),FLinearColor(Box.Min.X,Box.Min.Y,Inv.X,Inv.Y));
 Material->SetScalarParameterValue(TEXT("SpatialReady"),1);
}
void ADarkwellManualStaleRoom::BindSpatialProxy(AActor* Proxy)
{
 if(!Proxy || FindActive(GetWorld())!=this || Darkwell::PropLab::PresentationMode(GetWorld())!=2) return;
 TInlineComponentArray<UStaticMeshComponent*> Meshes(Proxy);
 if(SpatialProxy.Get()!=Proxy)
 {
  SpatialProxy=Proxy; SpatialProxyMaterials.Reset();
  auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Darkwell/Vision/PropLab/M_ManualAccumulatedMemory.M_ManualAccumulatedMemory"));
  for(int32 I=0;I<Meshes.Num();++I)
  {
   auto* Mat=UMaterialInstanceDynamic::Create(Parent,this);
   Mat->SetVectorParameterValue(TEXT("OriginalBaseColorTint"),FLinearColor(.14f,.48f,.25f));
   SpatialProxyMaterials.Add(Mat);
  }
 }
 for(int32 I=0;I<Meshes.Num();++I)
 {
  BindSpatialParameters(SpatialProxyMaterials[I]);
  if(Meshes[I]->GetMaterial(0)!=SpatialProxyMaterials[I]) Meshes[I]->SetMaterial(0,SpatialProxyMaterials[I]);
 }
}
void ADarkwellManualStaleRoom::UpdateSpatialMemory(float DeltaSeconds)
{
 if(SpatialMemory.GetCells().IsEmpty())
 {
  if(!HasActualCabinet()) return;
  // Read the original fixed footprint ONCE for texture coordinates. Never
  // change component transforms/bounds or derive replacement geometry.
  FBox Box(ForceInit);
  OriginalPartBounds.Reset();
  for(UStaticMeshComponent* Part:Cabinet->Memory->GetMemoryPrimitives())
  { const FBox PartBox=Part->Bounds.GetBox(); Box+=PartBox; OriginalPartBounds.Add(PartBox); }
  SpatialMemory.Initialize(CabinetId(),FBox2D(FVector2D(Box.Min),FVector2D(Box.Max)));
  const FIntPoint Size=SpatialMemory.GetSize();
  // Authority remains one immutable 2.5 cm cell. Four presentation samples per
  // cell provide a conservative inward ramp; the visible side ends in a zero
  // guard sample so bilinear filtering cannot discover or erase early.
  SpatialPresentationSize=Size*4;
  SpatialTexture=UTexture2D::CreateTransient(SpatialPresentationSize.X,SpatialPresentationSize.Y,PF_FloatRGBA);
  SpatialTexture->SRGB=false; SpatialTexture->Filter=TF_Bilinear;
  SpatialTexture->AddressX=TA_Clamp; SpatialTexture->AddressY=TA_Clamp; SpatialTexture->NeverStream=true;
  auto& Bulk=SpatialTexture->GetPlatformData()->Mips[0].BulkData;
  FMemory::Memzero(Bulk.Lock(LOCK_READ_WRITE),Bulk.GetBulkDataSize()); Bulk.Unlock();
  SpatialTexture->UpdateResource();
 }
 if(HasActualCabinet() && (!SpatialMemory.IsPresent() || SpatialSource.Get()!=Cabinet.Get()))
 { SpatialMemory.BeginPresent(); SpatialSource=Cabinet; }
 else if(!HasActualCabinet() && !SpatialMemory.IsAbsent())
 { SpatialMemory.BeginAbsent(); SpatialSource.Reset(); }
 const FIntPoint Size=SpatialMemory.GetSize(); const FBox2D& Box=SpatialMemory.GetBounds();
 const FVector2D Step=Box.GetSize()/FVector2D(Size.X,Size.Y);
 const auto* Fog=GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>();
 TArray<float> Corners,Coverage; Corners.SetNumUninitialized((Size.X+1)*(Size.Y+1)); Coverage.SetNumUninitialized(Size.X*Size.Y);
 for(int32 Y=0;Y<=Size.Y;++Y) for(int32 X=0;X<=Size.X;++X)
  Corners[Y*(Size.X+1)+X]=Fog->EvaluateLiveCoverageAtWorldPoint(Box.Min+Step*FVector2D(X,Y));
 for(int32 Y=0;Y<Size.Y;++Y) for(int32 X=0;X<Size.X;++X)
 {
  const int32 K=Y*(Size.X+1)+X;
  // All corners AND center must be legal: visual history cannot enlarge sight.
  Coverage[Y*Size.X+X]=FMath::Min(FMath::Min(Corners[K],Corners[K+1]),FMath::Min(Corners[K+Size.X+1],Corners[K+Size.X+2]));
  Coverage[Y*Size.X+X]=FMath::Min(Coverage[Y*Size.X+X],Fog->EvaluateLiveCoverageAtWorldPoint(Box.Min+Step*FVector2D(X+.5,Y+.5)));
 }
 SpatialMemory.Advance(DeltaSeconds,Coverage);
 constexpr int32 Samples=4;
 TArray<FLinearColor> PresentationPixels;
 check(SpatialMemory.BuildConservativePresentation(Samples,PresentationPixels)==SpatialPresentationSize);
 auto* Pixels=new FFloat16Color[SpatialPresentationSize.X*SpatialPresentationSize.Y];
 for(int32 I=0;I<PresentationPixels.Num();++I) Pixels[I]=FFloat16Color(PresentationPixels[I]);
 auto* Region=new FUpdateTextureRegion2D(0,0,0,0,SpatialPresentationSize.X,SpatialPresentationSize.Y);
 SpatialTexture->UpdateTextureRegions(0,1,Region,SpatialPresentationSize.X*sizeof(FFloat16Color),sizeof(FFloat16Color),reinterpret_cast<uint8*>(Pixels),
  [](uint8* Data,const FUpdateTextureRegion2D* R){delete[] reinterpret_cast<FFloat16Color*>(Data);delete R;});
 if(HasActualCabinet()) for(UMaterialInstanceDynamic* Mat:Cabinet->Materials) BindSpatialParameters(Mat);
}
void ADarkwellManualStaleRoom::ClearStaleCap()
{
 if(!StaleCap) return;
 StaleCap->SetMesh(UE::Geometry::FDynamicMesh3());
 StaleCap->SetVisibility(false);
 StaleCapSignature=0; StaleCapTriangleCount=0;
}
void ADarkwellManualStaleRoom::UpdateStaleCap(int32 Mode)
{
 using namespace UE::Geometry;
 const auto Cells=SpatialMemory.GetCells();
 const FIntPoint Size=SpatialMemory.GetSize();
 const bool bPresent=SpatialMemory.IsPresent();
 const bool bAbsent=SpatialMemory.IsAbsent();
 if(Mode!=2 || (!bPresent && !bAbsent) || Cells.IsEmpty() || OriginalPartBounds.IsEmpty())
 { if(StaleCapTriangleCount>0 || StaleCap->IsVisible()) ClearStaleCap(); return; }

 uint64 Signature=(uint64(SpatialMemory.GetGeneration())<<1 | uint64(bPresent))*1099511628211ull;
 for(const auto& C:Cells)
 {
  const uint64 Bits=bPresent ? (C.DiscoveredPresent>0?1ull:0ull)
                              : ((C.InitialRemembered>0?1ull:0ull)|(C.VerifiedEmpty>0?2ull:0ull));
  Signature=(Signature^Bits)*1099511628211ull;
 }
 if(Signature==StaleCapSignature) return;
 StaleCapSignature=Signature;

 FDynamicMesh3 Mesh;
 const FBox2D& Bounds=SpatialMemory.GetBounds();
 const FVector2D Step=Bounds.GetSize()/FVector2D(Size.X,Size.Y);
 const FVector Origin=GetActorLocation();
 // The same fixed authority-grid boundary closes both directions.  For an
 // absent generation it separates retained stale memory from verified floor;
 // for a present generation it separates discovered source from undiscovered
 // source.  It never reads temporal opacity, camera position, or AA samples.
 auto IsSubmittedSide=[&](int32 X,int32 Y)
 {
  if(X<0 || Y<0 || X>=Size.X || Y>=Size.Y) return false;
  const auto& C=Cells[Y*Size.X+X];
  return bPresent ? C.DiscoveredPresent>0 : C.InitialRemembered>0 && C.VerifiedEmpty==0;
 };
 auto IsCutSide=[&](int32 X,int32 Y)
 {
  if(X<0 || Y<0 || X>=Size.X || Y>=Size.Y) return false;
  const auto& C=Cells[Y*Size.X+X];
  return bPresent ? C.DiscoveredPresent==0 : C.InitialRemembered>0 && C.VerifiedEmpty>0;
 };
 auto AddQuad=[&](const FVector& A,const FVector& B,const FVector& C,const FVector& D)
 {
  const int32 IA=Mesh.AppendVertex(FVector3d(A-Origin));
  const int32 IB=Mesh.AppendVertex(FVector3d(B-Origin));
  const int32 IC=Mesh.AppendVertex(FVector3d(C-Origin));
  const int32 ID=Mesh.AppendVertex(FVector3d(D-Origin));
  Mesh.AppendTriangle(IA,IB,IC); Mesh.AppendTriangle(IA,IC,ID);
 };
 auto AddVerticalEdge=[&](double X,double Y0,double Y1)
 {
  for(const FBox& Part:OriginalPartBounds)
  {
   if(X<Part.Min.X-UE_KINDA_SMALL_NUMBER || X>Part.Max.X+UE_KINDA_SMALL_NUMBER) continue;
   const double A=FMath::Max(Y0,Part.Min.Y),B=FMath::Min(Y1,Part.Max.Y);
   if(B-A<=UE_KINDA_SMALL_NUMBER) continue;
   AddQuad(FVector(X,A,Part.Min.Z),FVector(X,B,Part.Min.Z),FVector(X,B,Part.Max.Z),FVector(X,A,Part.Max.Z));
  }
 };
 auto AddHorizontalEdge=[&](double Y,double X0,double X1)
 {
  for(const FBox& Part:OriginalPartBounds)
  {
   if(Y<Part.Min.Y-UE_KINDA_SMALL_NUMBER || Y>Part.Max.Y+UE_KINDA_SMALL_NUMBER) continue;
   const double A=FMath::Max(X0,Part.Min.X),B=FMath::Min(X1,Part.Max.X);
   if(B-A<=UE_KINDA_SMALL_NUMBER) continue;
   AddQuad(FVector(A,Y,Part.Min.Z),FVector(B,Y,Part.Min.Z),FVector(B,Y,Part.Max.Z),FVector(A,Y,Part.Max.Z));
  }
 };
 for(int32 Y=0;Y<Size.Y;++Y) for(int32 X=0;X<Size.X;++X)
 {
  if(!IsSubmittedSide(X,Y)) continue;
  const double X0=Bounds.Min.X+X*Step.X,X1=X0+Step.X;
  const double Y0=Bounds.Min.Y+Y*Step.Y,Y1=Y0+Step.Y;
  if(IsCutSide(X-1,Y)) AddVerticalEdge(X0,Y0,Y1);
  if(IsCutSide(X+1,Y)) AddVerticalEdge(X1,Y0,Y1);
  if(IsCutSide(X,Y-1)) AddHorizontalEdge(Y0,X0,X1);
  if(IsCutSide(X,Y+1)) AddHorizontalEdge(Y1,X0,X1);
 }
 StaleCapTriangleCount=Mesh.TriangleCount();
 StaleCap->SetMesh(MoveTemp(Mesh));
 if(StaleCapTriangleCount>0)
 {
  if(!StaleCap->GetMaterial(0))
   StaleCap->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Darkwell/Vision/PropLab/M_ManualStaleCutCap.M_ManualStaleCutCap")));
  StaleCap->SetVisibility(true);
 }
 else StaleCap->SetVisibility(false);
}
TArray<int32> ADarkwellManualStaleRoom::GetSpatialKnowledgeBits() const
{
 TArray<int32> Bits; Bits.Reserve(SpatialMemory.GetCells().Num());
 for(const auto& C:SpatialMemory.GetCells()) Bits.Add((C.DiscoveredPresent>0?1:0)|(C.VerifiedEmpty>0?2:0)|(C.RemainingStale>0?4:0)|(C.CurrentLegalCoverage>=.99f?8:0));
 return Bits;
}
FString ADarkwellManualStaleRoom::GetSpatialTelemetry() const
{
 float Current=0,Discovered=0,Verified=0,Remaining=0,Live=0,Source=0,ProxyOpacity=0;
 const auto Cells=SpatialMemory.GetCells();
 for(int32 I=0;I<Cells.Num();++I)
 {
  const auto& C=Cells[I]; Current+=C.CurrentLegalCoverage>=.99f; Discovered+=C.DiscoveredPresent; Verified+=C.VerifiedEmpty;
  Remaining+=C.RemainingStale; Live+=C.LiveBlend; const auto P=SpatialMemory.Presentation(I); Source+=P.R; ProxyOpacity+=P.B;
 }
 const float N=FMath::Max(1,Cells.Num());
 bool WasLive=false,Valid=false; FVector At; AActor* Proxy=nullptr;
 GetWorld()->GetSubsystem<UDarkwellRememberedPropSubsystem>()->TryGetRecordForTesting(CabinetId(),WasLive,Valid,At,Proxy);
 return FString::Printf(TEXT("{\"actual\":\"%s\",\"current\":%.8f,\"discovered\":%.8f,\"verified\":%.8f,\"remaining\":%.8f,\"live\":%.8f,\"sourceOpacity\":%.8f,\"proxyOpacity\":%.8f,\"snapshot\":%s,\"generation\":%u,\"toggles\":%d,\"cells\":%d,\"capTriangles\":%d,\"capVisible\":%s,\"presentationWidth\":%d,\"presentationHeight\":%d}"),
  HasActualCabinet()?TEXT("PRESENT"):TEXT("ABSENT"),Current/N,Discovered/N,Verified/N,Remaining/N,Live/N,Source/N,ProxyOpacity/N,Valid?TEXT("true"):TEXT("false"),SpatialMemory.GetGeneration(),ToggleCount,Cells.Num(),StaleCapTriangleCount,StaleCap->IsVisible()?TEXT("true"):TEXT("false"),SpatialPresentationSize.X,SpatialPresentationSize.Y);
}
void ADarkwellManualStaleRoom::Report()
{
 bool Live=false,Valid=false; FVector At; AActor* Proxy=nullptr;
 GetWorld()->GetSubsystem<UDarkwellRememberedPropSubsystem>()->TryGetRecordForTesting(CabinetId(),Live,Valid,At,Proxy);
 Status=FString::Printf(TEXT("MANUAL STALE ROOM | Mode %d | Policy 0 | ENEMY 0\nCabinet Actual: %s | Remembered Snapshot: %s | StableID: %s\nOld Occupancy Verified: %.1f%% | Object Empty Confirmed: %d | Pressure Switch: %s\nLiveCoverage at Cabinet: %.6f | Source Live: %d | Toggles: %d | Dark Gray Cap Tris: %d\nFree movement / mouse aim; right corridor connects rooms. Darkwell.PropLab stalemanual help"),
  Darkwell::PropLab::PresentationMode(GetWorld()),HasActualCabinet()?TEXT("PRESENT"):TEXT("ABSENT"),Valid?TEXT("VALID"):TEXT("EMPTY"),*CabinetId().ToString(),
  100*Evidence.VerifiedFraction(),Evidence.IsObjectEmpty(),IsSwitchArmed()?TEXT("ARMED"):TEXT("WAITING_FOR_EXIT"),GetCabinetCoverage(),Live,ToggleCount,StaleCapTriangleCount);
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
