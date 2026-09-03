#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Player/DarkwellCharacter.h"
#include "Interaction/DarkwellInteractionComponent.h"
#include "VisionPresentation/DarkwellMovingPropLabRoom.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "Visibility/SightWeave/DarkwellSightWeaveWorldSubsystem.h"

namespace Darkwell::MovingLiveTests
{
	const FName Id(TEXT("Lab.InWorld.Rotate.Cabinet"));
	using Mode = ESightWeaveHistoryMode;
	struct FRoom
	{
		UWorld* World;
		ADarkwellCharacter* Player;
		ADarkwellPropGameplayLab* Fixture;
		ADarkwellMovingPropLabRoom* Room;
		UDarkwellSightWeaveWorldSubsystem* Adapter;
		FRoom()
		{
			UPackage* Package = CreatePackage(TEXT("/Game/Maps/L_ProjectFogPropGameplayLab"));
			World = NewObject<UWorld>(Package, MakeUniqueObjectName(Package, UWorld::StaticClass(), TEXT("MovingLive")), RF_Transient);
			World->WorldType = EWorldType::Game;
			GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
			World->InitializeNewWorld(UWorld::InitializationValues().InitializeScenes(true)
				.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
			World->URL.AddOption(TEXT("PropLabOriginal")); World->URL.AddOption(TEXT("InWorldControls"));
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Player = World->SpawnActor<ADarkwellCharacter>(ADarkwellCharacter::StaticClass(), FVector(-300,100,92), FRotator(0,90,0), P);
			Player->DispatchBeginPlay();
			Fixture = World->SpawnActor<ADarkwellPropGameplayLab>();
			Fixture->PostInitializeComponents(); Fixture->DispatchBeginPlay();
			Room = ADarkwellMovingPropLabRoom::FindActive(World);
			Adapter = World->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
			Adapter->RequestSightWeaveAuthority(Fixture); Room->ResetRoom(Player);
			Face(90);
		}
		~FRoom() { Fixture->Destroy(); World->DestroyWorld(true); GEngine->DestroyWorldContext(World); }
		void Face(float Yaw) { Player->SetActorLocation(FVector(-300,100,92)); Player->SetActorRotation(FRotator(0,Yaw,0)); }
		void Step(int32 Frames=1)
		{
			for (int32 I=0; I<Frames; ++I) { Adapter->Tick(1.f/60); Room->UpdateRoom(1.f/60,Player); Fixture->Tick(1.f/60); }
		}
		bool UseRotation()
		{
			auto* C = Room->GetControlForTesting(EDarkwellMovingPropLabControlKind::VisibleRotate);
			Player->SetActorLocation(C->GetActorLocation()+FVector(0,-190,0));
			Player->SetActorRotation(FRotator(0,90,0)); Step(2);
			World->UpdateWorldComponents(true,false);
			auto* Interaction=Player->GetInteractionComponent(); Interaction->UpdateFocusedActorFromWorld();
			return Interaction->GetFocusedActor()==C && Interaction->GetFocusedPrompt().ToString().Contains(TEXT("F")) && Interaction->TryInteract();
		}
	};
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellMovingLiveReproduction,
 "Darkwell.PropLab.MovingLiveContinuity.ContinuousVisibleRotationDoesNotResetLive", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellMovingLiveReproduction::RunTest(const FString&)
{
 using namespace Darkwell::MovingLiveTests;
 FRoom F; F.Step(30);
 const FString Before=F.Room->GetMovingLiveTelemetry(Id);
 AddInfo(TEXT("MOVING_LIVE_BEFORE ")+Before);
 TestTrue(TEXT("Source starts fully Live"),Before.Contains(TEXT("\"live\":[1.000000,1.000000,1.000000]")));
 F.Room->StartTrackedRotationForTesting(Id,180,4);
 int32 Collapsed=0;
 for(int32 I=0;I<240;++I) {
  F.Step(); const FString Row=F.Room->GetMovingLiveTelemetry(Id);
  TestTrue(TEXT("Stable interior appearance every motion frame"),Row.Contains(TEXT("\"appearance\":[1.000000,1.000000,1.000000]")));
  TestTrue(TEXT("Fixed four current textures across world AABB changes"),Row.Contains(TEXT("\"texture_creations\":4,")));
  AddInfo(FString::Printf(TEXT("MOVING_LIVE_FRAME %d "),I)+Row);
  if(Row.Contains(TEXT("\"appearance\":[0.083333,0.083333,0.083333]"))) ++Collapsed;
  TestEqual(TEXT("Single current epoch"),F.Room->GetCurrentEpochCountForTesting(Id),1);
  TestEqual(TEXT("No stale epoch during continuous sight"),F.Room->GetStaleEpochCountForTesting(Id),0);
 }
 // Exact same 240-frame baseline route now asserts preserved local evidence.
 TestEqual(TEXT("No repeated appearance collapse"),Collapsed,0);
 TestTrue(TEXT("Ordinary pose changes never reinitialize"),F.Room->GetMovingLiveTelemetry(Id).Contains(TEXT("\"initialize\":1,")));
 AddInfo(FString::Printf(TEXT("MOVING_LIVE_BASELINE_COLLAPSED=%d/240"),Collapsed));
 return true;
}
#endif
