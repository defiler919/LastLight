#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "VisionPresentation/DarkwellApartmentLab.h"
#include "VisionPresentation/DarkwellObjectMemoryScene.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "VisionPresentation/DarkwellBlackRegionTrigger.h"
#include "Visibility/SightWeave/DarkwellSightWeaveWorldSubsystem.h"
#include "Player/DarkwellCharacter.h"
#include "Player/DarkwellPlayerController.h"
#include "Combat/DarkwellLoadoutComponent.h"
#include "World/DarkwellDoor.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "SightWeaveWorldSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellApartmentContracts,"Darkwell.Apartment.LayoutDoorsAuthority",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellApartmentContracts::RunTest(const FString&)
{
 auto* Package=CreatePackage(TEXT("/Game/Maps/L_SightWeaveApartmentLab"));
 auto* W=NewObject<UWorld>(Package,MakeUniqueObjectName(Package,UWorld::StaticClass(),TEXT("ApartmentContract")),RF_Transient);
 W->WorldType=EWorldType::Game;GEngine->CreateNewWorldContext(W->WorldType).SetCurrentWorld(W);
 W->InitializeNewWorld(UWorld::InitializationValues().InitializeScenes(true).AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false));
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* P=W->SpawnActor<ADarkwellCharacter>(FVector(0,-350,92),FRotator(0,90,0),Spawn);P->PostInitializeComponents();P->DispatchBeginPlay();
 auto* PC=W->SpawnActor<ADarkwellPlayerController>();PC->PostInitializeComponents();PC->Possess(P);
 auto* Lab=W->SpawnActor<ADarkwellApartmentLab>();Lab->PostInitializeComponents();Lab->DispatchBeginPlay();
 for(ADarkwellDoor* D:Lab->Doors){D->PostInitializeComponents();D->DispatchBeginPlay();}
 Lab->Trigger->PostInitializeComponents();Lab->Trigger->DispatchBeginPlay();
 TestEqual(TEXT("Unknown on initialization"),Lab->MemoryScene->GetTotalSpatialRecordCount(),0);
 TestEqual(TEXT("Three actual native doors"),Lab->Doors.Num(),3);
 auto* Adapter=W->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
 auto* Fog=W->GetSubsystem<UDarkwellFogVisualSubsystem>();
 TestTrue(TEXT("Request current adapter"),Adapter->RequestSightWeaveAuthority(Lab));Adapter->Tick(1.f/60);
 TestTrue(TEXT("New authority active"),Adapter->IsSightWeaveAuthorityActive());
 TestTrue(TEXT("P4 active"),Fog->IsActive());
 TestEqual(TEXT("16 bounded wall/door/furniture segments"),Fog->GetDiagnostics().CachedOccluderSegmentCount,16);
 const FVector PointsFrom[]{FVector(0,-300,92),FVector(0,100,92),FVector(0,100,92)};
 const FVector2D PointsTo[]{FVector2D(0,0),FVector2D(-350,100),FVector2D(350,100)};
 const float Yaws[]{90,180,0};
 for(int Door=0;Door<3;++Door) for(int Repeat=0;Repeat<2;++Repeat)
 {
  P->SetActorLocationAndRotation(PointsFrom[Door],FRotator(0,Yaws[Door],0));
  ADarkwellDoor* D=Lab->Doors[Door];D->RestoreDoorState(DarkwellGameplayTags::State_World_Door_Closed);Adapter->Tick(1.f/60);
  TestEqual(TEXT("Closed door blocks legal coverage"),Fog->EvaluateLiveCoverageAtWorldPoint(PointsTo[Door]),0.f);
  D->Interact(*P); // existing F target entry point; exercise the actual hinge animation
  for(int Frame=0;Frame<45;++Frame){D->Tick(1.f/60);Adapter->Tick(1.f/60);}
  TestTrue(TEXT("Open doorway admits legal coverage"),Fog->EvaluateLiveCoverageAtWorldPoint(PointsTo[Door])>0.9f);
  FSightWeaveQuerySampleSet Q;Q.Samples={FVector(PointsTo[Door],100)};Q.Rule=ESightWeaveSampleRule::AnySample;Q.RequiredCount=1;
  const auto Result=W->GetSubsystem<USightWeaveWorldSubsystem>()->QuerySamples(FSightWeaveKnowledgeOwnerId(TEXT("Local")),FSightWeaveFloorId(TEXT("Darkwell.Integration.Ground")),Q);
  TestTrue(TEXT("CPU authority agrees with open doorway"),Result.bAuthoritative && Result.bVisible);
  D->RestoreDoorState(DarkwellGameplayTags::State_World_Door_Closed);Adapter->Tick(1.f/60);
  TestEqual(TEXT("Reclose is immediate, no stale P4"),Fog->EvaluateLiveCoverageAtWorldPoint(PointsTo[Door]),0.f);
 }
 P->SetActorLocationAndRotation(FVector(0,250,92),FRotator(0,0,0));Adapter->Tick(1.f/60);
 TestEqual(TEXT("Long wall blocks outside door"),Fog->EvaluateLiveCoverageAtWorldPoint(FVector2D(350,250)),0.f);
 Lab->Tick(1.f/60); // register the existing environmental illumination source
 P->GetLoadoutComponent()->RestorePersistentState(2,0,0,0,DarkwellGameplayTags::Equipment_Left_Shotgun,DarkwellGameplayTags::Equipment_Right_Torch);
 P->SetActorLocationAndRotation(FVector(0,100,92),FRotator(0,90,0));Adapter->Tick(1.f/60);
 TestTrue(TEXT("Living environment is legal without portable light"),Fog->EvaluateLiveCoverageAtWorldPoint(FVector2D(0,350))>0.9f);
 P->SetActorRotation(FRotator(0,-90,0));Adapter->Tick(1.f/60);
 TestEqual(TEXT("Environment does not widen fixed view cone"),Fog->EvaluateLiveCoverageAtWorldPoint(FVector2D(0,350)),0.f);
 P->SetActorLocationAndRotation(FVector(350,170,92),FRotator(0,90,0));Adapter->Tick(1.f/60);
 TestEqual(TEXT("Bedroom beyond awareness needs legal illumination"),Fog->EvaluateLiveCoverageAtWorldPoint(FVector2D(350,430)),0.f);
 TestTrue(TEXT("Existing Trigger activates"),Lab->Trigger->Activate());
 TestTrue(TEXT("Repeated activation idempotent"),Lab->Trigger->Activate());
 Lab->Trigger->Deactivate();Lab->Trigger->Deactivate();TestFalse(TEXT("Block released"),Lab->Trigger->IsActive());
 Lab->Destroy();W->DestroyWorld(true);GEngine->DestroyWorldContext(W);
 return true;
}
#endif
