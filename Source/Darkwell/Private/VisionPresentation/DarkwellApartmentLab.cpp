#include "VisionPresentation/DarkwellApartmentLab.h"
#include "VisionPresentation/DarkwellObjectMemoryScene.h"
#include "VisionPresentation/DarkwellRememberablePropComponent.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "VisionPresentation/DarkwellBlackRegionTrigger.h"
#include "VisionPresentation/DarkwellBlackRegionSwitch.h"
#include "World/DarkwellDoor.h"
#include "Player/DarkwellCharacter.h"
#include "Combat/DarkwellLoadoutComponent.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "SightWeaveObjectPolicy.h"
#include "SightWeaveWorldSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

ADarkwellApartmentLab::ADarkwellApartmentLab()
{
 PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickGroup=TG_PostUpdateWork;
 TArray<UStaticMeshComponent*> Meshes; GetComponents(Meshes);
 for(auto* M:Meshes) { M->SetVisibility(false); M->SetHiddenInGame(true); M->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
 GetRememberablePropComponent()->ResetMemoryPrimitives();
 GetRememberablePropComponent()->ConfigureStableId(NAME_None);
 GetRememberablePropComponent()->bRememberFromStart=false;
}
FBox2D ADarkwellApartmentLab::GetSightWeaveFloorBounds() const
{ return FBox2D(FVector2D(-610,-510),FVector2D(610,510)); }

void ADarkwellApartmentLab::RegisterSource(AActor* A,FName Id,FLinearColor Tint,bool Whole,bool Moving)
{
 auto* Memory=NewObject<UDarkwellRememberablePropComponent>(A); A->AddInstanceComponent(Memory);
 Memory->bUseSpatialMemory=true; Memory->bRememberFromStart=false;
 TArray<UStaticMeshComponent*> Parts; A->GetComponents(Parts);
 for(auto* Part:Parts) Memory->AddMemoryPrimitive(Part);
 Memory->ConfigureStableId(Id); Memory->SetMemoryAppearance(Tint,1); Memory->RegisterComponent();
 auto* Policy=NewObject<USightWeaveObjectPolicyComponent>(A); A->AddInstanceComponent(Policy);
 Policy->bOverrideRevealMode=true; Policy->RevealMode=Whole?ESightWeaveRevealMode::WholeObjectAfterSpan:ESightWeaveRevealMode::SpatialPartial;
 Policy->bOverrideMinimumObservedSpan=true; Policy->MinimumObservedSpanCm=40;
 Policy->bOverrideHistoryMode=true; Policy->HistoryMode=Moving?ESightWeaveHistoryMode::Always:ESightWeaveHistoryMode::StationaryOnly;
 Policy->RegisterComponent();
 ensureAlwaysMsgf(MemoryScene->RegisterRememberable(Memory,Policy),TEXT("Apartment registration failed: %s"),*Id.ToString());
 Sources.Add(A);
}
AActor* ADarkwellApartmentLab::Box(FName Id,FVector Location,FVector Size,FLinearColor Tint,bool Whole,float Yaw)
{
 auto* A=GetWorld()->SpawnActor<AActor>(); A->SetOwner(this);
 auto* M=NewObject<UStaticMeshComponent>(A); A->SetRootComponent(M); A->AddInstanceComponent(M);
 M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
 M->SetMobility(EComponentMobility::Movable); M->SetCollisionProfileName(TEXT("BlockAllDynamic"));
 M->SetRelativeScale3D(Size/100); M->RegisterComponent();
 A->SetActorLocationAndRotation(Location,FRotator(0,Yaw,0));
 if(Id.ToString().Contains(TEXT("Floor"))) M->TranslucencySortPriority=-1;
 RegisterSource(A,Id,Tint,Whole); return A;
}
void ADarkwellApartmentLab::BeginPlay()
{
 Super::BeginPlay();
 MemoryScene=GetWorld()->SpawnActor<ADarkwellObjectMemoryScene>();
 const FLinearColor Floor(0.18f,0.19f,0.20f),Wall(0.42f,0.43f,0.4f),Wood(0.34f,0.22f,0.13f),Blue(0.13f,0.34f,0.5f);
 // 12 x 10 m: south entry, central living room, west kitchen, east bedroom.
 // Modest floor pieces retain identical authority precision while bounding each object's grid.
 for(int X=0;X<4;++X) for(int Y=0;Y<4;++Y)
  Box(*FString::Printf(TEXT("Apartment.Floor.%d.%d"),X,Y),FVector(-450+300*X,-375+250*Y,-5),FVector(300,250,10),Floor);
 auto WallLine=[&](FVector2D A,FVector2D B)
 {
  const FVector2D D=B-A,C=(A+B)*0.5f;
  const float Yaw=FMath::RadiansToDegrees(FMath::Atan2(D.Y,D.X));
  Box(*FString::Printf(TEXT("Apartment.Wall.%d"),FixedSegments.Num()),FVector(C.X,C.Y,120),FVector(D.Size(),20,240),Wall,false,Yaw);
  auto& S=FixedSegments.AddDefaulted_GetRef(); S.A=A;S.B=B;S.ZMin=0;S.ZMax=240;
 };
 WallLine({-600,-500},{600,-500}); WallLine({600,-500},{600,500});
 WallLine({600,500},{-600,500}); WallLine({-600,500},{-600,-500});
 for(float X:{-200.f,200.f}) { WallLine({X,-140},{X,40}); WallLine({X,160},{X,500}); }
 WallLine({-600,-140},{-60,-140}); WallLine({60,-140},{600,-140});
 // Existing 160 cm panel scaled to a 120 cm clear opening; Z remains 220 cm.
 const FVector Locations[]{FVector(0,-140,0),FVector(-200,100,0),FVector(200,100,0)};
 for(int I=0;I<3;++I)
 {
  auto* D=GetWorld()->SpawnActor<ADarkwellDoor>(Locations[I],FRotator(0,I==0?90:0,0)); D->SetOwner(this); D->SetActorScale3D(FVector(1,0.75f,1));
  TArray<UPointLightComponent*> Lights; D->GetComponents(Lights);
  for(auto* L:Lights) L->SetIntensity(0); // decorative passage indicator is not legal light
  Doors.Add(D); RegisterSource(D,*FString::Printf(TEXT("Apartment.Door.%d"),I),Wood,false,true);
  // Lintel preserves a complete wall above each opening without closing its XY aperture.
  Box(*FString::Printf(TEXT("Apartment.Lintel.%d"),I),Locations[I]+FVector(0,0,230),I==0?FVector(120,20,20):FVector(20,120,20),Wall);
 }
 Box(TEXT("Apartment.Sofa.Seat"),{0,370,30},{200,75,60},Blue);
 Box(TEXT("Apartment.Sofa.Back"),{0,410,65},{200,20,100},Blue);
 Box(TEXT("Apartment.CoffeeTable.Whole"),{0,220,25},{80,55,50},Wood,true);
 Box(TEXT("Apartment.Kitchen.Counter"),{-540,300,45},{75,230,90},Wall);
 Box(TEXT("Apartment.Kitchen.Dining"),{-365,280,38},{100,70,76},Wood,true);
 Box(TEXT("Apartment.Kitchen.Fridge"),{-510,-50,95},{70,65,190},Wall);
 Box(TEXT("Apartment.Bed"),{405,320,28},{150,220,56},Blue);
 Box(TEXT("Apartment.Bedside.Whole"),{530,410,30},{45,50,60},Wood,true);
 Box(TEXT("Apartment.Wardrobe.Partial37"),{445,-55,100},{140,55,200},Blue,false,37);
 Box(TEXT("Apartment.Entry.Cabinet"),{-410,-410,95},{150,45,190},Wood);
 // Three tall furniture planes are explicit 2D occluders, like the wall declarations.
 auto Plane=[&](FVector2D A,FVector2D B){auto& S=FixedSegments.AddDefaulted_GetRef();S.A=A;S.B=B;S.ZMin=0;S.ZMax=200;};
 Plane({-510,-82.5},{-510,-17.5});
 const FVector2D Dir(FMath::Cos(FMath::DegreesToRadians(37.f)),FMath::Sin(FMath::DegreesToRadians(37.f)));
 Plane(FVector2D(445,-55)-Dir*70,FVector2D(445,-55)+Dir*70);
 Plane({-485,-410},{-335,-410});
 Trigger=GetWorld()->SpawnActor<ADarkwellBlackRegionTrigger>(FVector(400,180,0),FRotator::ZeroRotator);
 Trigger->SetOwner(this); Trigger->HalfExtentXY=FVector2D(200,320); Trigger->OnConstruction(Trigger->GetActorTransform());
 auto* Console=GetWorld()->SpawnActor<ADarkwellBlackRegionSwitch>(FVector(100,-55,40),FRotator::ZeroRotator);
 Console->SetOwner(this); Console->Target=Trigger;
 RegisterSource(Console,TEXT("Apartment.BlackoutConsole"),FLinearColor(0.15f,0.6f,0.25f),true);
 UE_LOG(LogTemp,Display,TEXT("APARTMENT initial=Unknown records=%d doors=3 fixedSegments=%d Whole+Partial37 bedroomBlackout=Inactive"),MemoryScene->GetTotalSpatialRecordCount(),FixedSegments.Num());
}
void ADarkwellApartmentLab::BuildSightWeaveOccluderSegments(TArray<FDarkwellVisionIntegrationSegment>& Out) const
{
 Out=FixedSegments;
 for(ADarkwellDoor* D:Doors) if(IsValid(D))
 {
  const auto* M=D->FindComponentByClass<UStaticMeshComponent>();
  const FTransform T=M->GetComponentTransform();
  auto& S=Out.AddDefaulted_GetRef(); S.A=FVector2D(T.TransformPosition(FVector(0,-50,0))); S.B=FVector2D(T.TransformPosition(FVector(0,50,0)));S.ZMin=0;S.ZMax=220;
 }
}
bool ADarkwellApartmentLab::EnableDarkwellProjectFogP4(UTexture* Raw,FVector2D Min,FVector2D Inv)
{
 for(AActor* A:Sources) for(UPrimitiveComponent* M:A->FindComponentByClass<UDarkwellRememberablePropComponent>()->GetMemoryPrimitives())
  if(auto* MID=Cast<UMaterialInstanceDynamic>(M->GetMaterial(0)))
  { MID->SetTextureParameterValue(TEXT("DarkwellLiveCoverageTexture"),Raw);MID->SetVectorParameterValue(TEXT("FogWorldMin"),FLinearColor(Min.X,Min.Y,0,0));MID->SetVectorParameterValue(TEXT("FogWorldInvExtent"),FLinearColor(Inv.X,Inv.Y,0,0)); }
 return true;
}
void ADarkwellApartmentLab::Tick(float Dt)
{
 Super::Tick(Dt);
 auto* P=Cast<ADarkwellCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
 auto* Fog=GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>();
 if(!P || !MemoryScene || !Fog->IsActive()) return;
 if(!bPlayerReady)
 {
  if(auto* B=P->FindComponentByClass<USpringArmComponent>()){B->SetRelativeRotation(FRotator(-65,90,0));B->TargetArmLength=1100;B->bDoCollisionTest=false;}
  P->GetLoadoutComponent()->RestorePersistentState(2,100,0,100,DarkwellGameplayTags::Equipment_Left_Shotgun,DarkwellGameplayTags::Equipment_Right_Torch);
  FSightWeaveIlluminationSourceDescription L; L.Transform=FTransform(FVector(0,250,100));
  L.FloorId=FSightWeaveFloorId(TEXT("Darkwell.Integration.Ground"));L.KnowledgeOwnerId=FSightWeaveKnowledgeOwnerId(TEXT("Local"));
  L.HeightRange.ZMin=-100;L.HeightRange.ZMax=300;L.Shape=ESightWeaveSourceShape::Radial;L.Range=350;L.HalfAngleDegrees=180;L.EmittedCapabilities={TEXT("Darkwell.Visible.Environment")};L.NormalizeCapabilities();
  EnvironmentLight=GetWorld()->GetSubsystem<USightWeaveWorldSubsystem>()->RegisterIlluminationSource(L,this);
  auto* Lamp=NewObject<UPointLightComponent>(this); AddInstanceComponent(Lamp); Lamp->SetupAttachment(GetRootComponent());
  Lamp->SetRelativeLocation(FVector(0,250,180)); Lamp->SetIntensity(1200); Lamp->SetAttenuationRadius(350); Lamp->SetLightColor(FLinearColor(1,0.85f,0.65f)); Lamp->SetCastShadows(true); Lamp->RegisterComponent();
  ensureAlways(EnvironmentLight.IsValid()); bPlayerReady=true;
  if(GEngine) GEngine->AddOnScreenDebugMessage(0x415054,30,FColor::Cyan,TEXT("APARTMENT | WASD + mouse | F: doors / green bedroom blackout console | west kitchen, east bedroom"));
 }
 MemoryScene->UpdateMemory(Dt,P->GetActorLocation());
}
void ADarkwellApartmentLab::EndPlay(EEndPlayReason::Type Reason)
{
 if(auto* R=GetWorld()->GetSubsystem<USightWeaveWorldSubsystem>()) if(EnvironmentLight.IsValid()) R->UnregisterIlluminationSource(EnvironmentLight);
 if(IsValid(Trigger)) Trigger->Destroy();
 if(IsValid(MemoryScene)) MemoryScene->Destroy();
 for(AActor* A:Sources) if(IsValid(A)) A->Destroy();
 Sources.Reset();Doors.Reset();Super::EndPlay(Reason);
}
