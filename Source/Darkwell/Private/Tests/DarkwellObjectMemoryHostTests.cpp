#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/GarbageCollection.h"
#include "VisionPresentation/DarkwellObjectMemoryScene.h"
#include "VisionPresentation/DarkwellRememberablePropComponent.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellObjectMemoryOrdinaryHost,
 "Darkwell.ObjectMemory.OrdinaryHost",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellObjectMemoryOrdinaryHost::RunTest(const FString&)
{
	// No Lab actor, map, URL option, control, character, fixture or name filter.
	UPackage* Package=CreatePackage(TEXT("/Temp/OrdinaryMemoryHost"));
	UWorld* World=NewObject<UWorld>(Package,MakeUniqueObjectName(Package,UWorld::StaticClass(),TEXT("OrdinaryMemoryHost")),RF_Transient);
	World->WorldType=EWorldType::Game;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeNewWorld(UWorld::InitializationValues().InitializeScenes(true).AllowAudioPlayback(false)
		.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false));
	auto* Scene=World->SpawnActor<ADarkwellObjectMemoryScene>(); Scene->DispatchBeginPlay();
	auto* Fog=World->GetSubsystem<UDarkwellFogVisualSubsystem>();
	FDarkwellFogVisualSourceSnapshot Source;
	Source.BodyRadiusCentimeters=60; Source.ConeRangeCentimeters=1200; Source.ConeHalfAngleDegrees=52;
	Source.ConeForward=FVector2D(0,1); Source.bConeLegallyLive=true; Source.AuthorityRevision=1;
	TestTrue(TEXT("Ordinary host publishes legal analytic coverage"),Fog->ActivateForWorld(
		FBox2D(FVector2D(-1000,-1000),FVector2D(1000,1000)),Source,{}));
	TArray<AActor*> Actors; TArray<USightWeaveObjectPolicyComponent*> Policies;
	TArray<FName> Ids;
	for(int32 I=0;I<6;++I)
	{
		auto* Actor=World->SpawnActor<AActor>(); Actors.Add(Actor);
		auto* Mesh=NewObject<UStaticMeshComponent>(Actor);
		Actor->SetRootComponent(Mesh); Actor->AddInstanceComponent(Mesh);
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
		Mesh->SetMobility(EComponentMobility::Movable); Mesh->RegisterComponent();
		Actor->SetActorTransform(FTransform(FRotator::ZeroRotator,FVector((I%3-1)*210,400+(I/3)*220,60),FVector(1.5,.6,1.2)));
		auto* Memory=NewObject<UDarkwellRememberablePropComponent>(Actor);
		Memory->bUseSpatialMemory=true; Ids.Add(FName(*FString::Printf(TEXT("Furniture-%d"),I)));
		Memory->ConfigureStableId(Ids.Last()); Memory->AddMemoryPrimitive(Mesh);
		Actor->AddInstanceComponent(Memory); Memory->RegisterComponent();
		auto* Policy=NewObject<USightWeaveObjectPolicyComponent>(Actor); Policies.Add(Policy);
		Policy->bOverrideRevealMode=Policy->bOverrideHistoryMode=true;
		Policy->RevealMode=I<3?ESightWeaveRevealMode::WholeObjectAfterSpan:ESightWeaveRevealMode::SpatialPartial;
		Policy->HistoryMode=I%3==0?ESightWeaveHistoryMode::Always:I%3==1?ESightWeaveHistoryMode::StationaryOnly:ESightWeaveHistoryMode::Never;
		Actor->AddInstanceComponent(Policy); Policy->RegisterComponent(); Actor->DispatchBeginPlay();
		TestTrue(TEXT("Ordinary source registers"),Scene->RegisterRememberable(Memory,Policy));
		TestFalse(TEXT("Duplicate registration rejected"),Scene->RegisterRememberable(Memory,Policy));
		TestTrue(TEXT("Root mesh placement excludes world actor transform"),Memory->GetPrimitiveTransform(*Mesh).Equals(FTransform::Identity));
	}
	auto Step=[&](int32 N){for(int32 I=0;I<N;++I)Scene->UpdateMemory(1.f/60,FVector::ZeroVector);};
	Step(20);
	for(auto Id:Ids) TestTrue(TEXT("Ordinary source becomes live"),Scene->IsCurrentSourceVisibleForTesting(Id));
	for(int32 I=0;I<3;++I) TestTrue(TEXT("Whole span confirmation works outside Lab"),Scene->IsRevealConfirmedForTesting(Ids[I]));
	Source.bConeLegallyLive=false; ++Source.AuthorityRevision; Fog->UpdateSource(Source); Step(20);
	for(int32 I=0;I<6;++I) TestEqual(TEXT("Object-local knowledge retention outside Lab"),Scene->GetSpatialRecordCount(Ids[I]),I%3==2?0:1);
	TestTrue(TEXT("Always compatibility keeps gray in its stationary current raster"),Scene->GetMovingLiveTelemetry(Ids[3]).Contains(TEXT("\"live\":[0.000000,0.000000,0.000000]")));
	const FTransform OldPose=Actors[4]->GetActorTransform();
	Policies[4]->SetSightWeaveMoving(true); Actors[4]->AddActorWorldOffset(FVector(0,250,0)); Step(10);
	Policies[4]->SetSightWeaveMoving(false); Step(20);
	TestEqual(TEXT("Hidden stop adds no endpoint knowledge"),Scene->GetStaleEpochCountForTesting(Ids[4]),1);
	TestTrue(TEXT("Hidden stop preserves unverified old position"),Scene->GetVisibleHistoricalProxyCountForTesting(Ids[4])>0);
	// A dead source/policy must not be required by subsequent memory updates.
	Actors[0]->Destroy(); Step(10);
	TestTrue(TEXT("Destroyed source retains unverified knowledge"),Scene->GetVisibleHistoricalProxyCountForTesting(Ids[0])>0);
	Source.bConeLegallyLive=true; ++Source.AuthorityRevision; Fog->UpdateSource(Source); Step(30);
	TestTrue(TEXT("Fresh stationary observation admits the new pose"),Scene->GetCurrentEpochCountForTesting(Ids[4])==1);
	TestEqual(TEXT("Legal empty evidence removes destroyed source memory"),Scene->GetSpatialRecordCount(Ids[0]),0);
	FDarkwellFogVisualSourceSnapshot Invalid;
	TestFalse(TEXT("Invalid publication rejected"),Fog->UpdateSource(Invalid));
	TestFalse(TEXT("Invalid publication cannot keep stale legal evidence"),Fog->QueryLiveCoverageAtWorldPoint(FVector2D(0,400)).bValid);
	Scene->ResetMemory();
	TestEqual(TEXT("Explicit reset releases knowledge and presentation"),Scene->GetTotalSpatialRecordCount(),0);
	TestTrue(TEXT("Reset does not destroy ordinary source actors"),IsValid(Actors[4]));
	Scene->Destroy(); World->DestroyWorld(true); GEngine->DestroyWorldContext(World);
	CollectGarbage(RF_NoFlags);
	return true;
}
#endif
