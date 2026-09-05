#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "VisionPresentation/DarkwellObjectMemoryScene.h"
#include "VisionPresentation/DarkwellRememberablePropComponent.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellWholeReobservation,
 "Darkwell.ObjectMemory.WholeReobservation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellWholeReobservation::RunTest(const FString&)
{
	// Ordinary host, no Lab subclass, identities, fixtures, map or special policy path.
	for(const float Dt:{1.f/30,1.f/60,1.f/120,1.f/144,.20f})
	{
		UPackage* Package=CreatePackage(TEXT("/Temp/WholeReobservation"));
		UWorld* World=NewObject<UWorld>(Package,MakeUniqueObjectName(Package,UWorld::StaticClass(),TEXT("ObserverHost")),RF_Transient);
		World->WorldType=EWorldType::Game;
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeNewWorld(UWorld::InitializationValues().InitializeScenes(true).AllowAudioPlayback(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false));
		auto* Scene=World->SpawnActor<ADarkwellObjectMemoryScene>(); Scene->DispatchBeginPlay();
		auto* Fog=World->GetSubsystem<UDarkwellFogVisualSubsystem>();
		auto* Actor=World->SpawnActor<AActor>();
		auto* Mesh=NewObject<UStaticMeshComponent>(Actor);
		Actor->SetRootComponent(Mesh); Actor->AddInstanceComponent(Mesh);
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
		Mesh->SetMobility(EComponentMobility::Movable); Mesh->RegisterComponent();
		Actor->SetActorTransform(FTransform(FRotator::ZeroRotator,FVector(-450,0,72.5),FVector(1.8,.75,1.45)));
		const FTransform OriginalPose=Actor->GetActorTransform();
		auto* Memory=NewObject<UDarkwellRememberablePropComponent>(Actor);
		Memory->bUseSpatialMemory=true; const FName Id(TEXT("StorageCabinet"));
		Memory->ConfigureStableId(Id); Memory->AddMemoryPrimitive(Mesh);
		Actor->AddInstanceComponent(Memory); Memory->RegisterComponent();
		auto* Policy=NewObject<USightWeaveObjectPolicyComponent>(Actor);
		Policy->bOverrideRevealMode=Policy->bOverrideHistoryMode=true;
		Policy->RevealMode=ESightWeaveRevealMode::WholeObjectAfterSpan;
		Policy->HistoryMode=ESightWeaveHistoryMode::StationaryOnly;
		Actor->AddInstanceComponent(Policy); Policy->RegisterComponent(); Actor->DispatchBeginPlay();
		TestTrue(TEXT("Register ordinary source"),Scene->RegisterRememberable(Memory,Policy));
		FDarkwellFogVisualSourceSnapshot Source;
		Source.BodyRadiusCentimeters=60; Source.ConeRangeCentimeters=1200; Source.ConeHalfAngleDegrees=52;
		Source.AuthorityRevision=1; Source.bConeLegallyLive=true;
		auto View=[&](FVector2D Position,float Yaw)
		{
			Source.BodyCenter=Source.ConeOrigin=Position;
			Source.ConeForward=FVector2D(FMath::Cos(FMath::DegreesToRadians(Yaw)),FMath::Sin(FMath::DegreesToRadians(Yaw)));
			++Source.AuthorityRevision; Fog->UpdateSource(Source);
		};
		Source.BodyCenter=Source.ConeOrigin=FVector2D(70,-1000); Source.ConeForward=FVector2D(0,-1);
		const TArray<FDarkwellFogVisualSegment> Walls{{FVector2D(-1300,-400),FVector2D(-260,-400)},
			{FVector2D(260,-400),FVector2D(1300,-400)}};
		TestTrue(TEXT("Real wall authority publishes"),Fog->ActivateForWorld(FBox2D(FVector2D(-2000,-2000),FVector2D(2000,1000)),Source,Walls));
		auto Step=[&](float Seconds){for(int32 I=0;I<FMath::CeilToInt(Seconds/Dt);++I) Scene->UpdateMemory(Dt,FVector(Source.BodyCenter,92));};
		Step(.4f); TestEqual(TEXT("Unconfirmed Whole leaves no gray"),Scene->GetSpatialRecordCount(Id),0);
		View(FVector2D(70,-1000),115); Step(.5f);
		TestTrue(TEXT("Far partial contact confirms ordinary Whole"),Scene->IsRevealConfirmedForTesting(Id));
		TestTrue(TEXT("Initial view is physically partial"),Scene->GetLastLegalCoverageRatioForTesting(Id)>0 && Scene->GetLastLegalCoverageRatioForTesting(Id)<.95f);
		TestEqual(TEXT("Whole permission cannot see world point behind wall"),Fog->QueryLiveCoverageAtWorldPoint(FVector2D(-900,0)).Coverage,0.f);
		View(FVector2D(70,-1000),-90); Step(.4f);
		// Independent cuboid interior: not BuildFullGeometryMask, cached mask or a
		// second invocation of the capture initializer under test.
		auto CheckHistory=[&](const TCHAR* Stage)
		{
			const auto& Prop=Scene->Tracked.FindChecked(Id);
			if(!TestEqual(FString(Stage)+TEXT(" one state"),Prop.History.GetRecords().Num(),1)) return;
			const auto& R=Prop.History.GetRecords()[0]; const auto* V=Prop.Visuals.Find(R.Epoch);
			int32 Tested=0,MissingCapture=0,MissingAA=0,MissingSubmitted=0;
			const auto Size=R.FineHistory.GetSize(); const auto Bounds=R.FineHistory.GetBounds();
			for(int32 Y=0;Y<Size.Y;++Y) for(int32 X=0;X<Size.X;++X)
			{
				const auto P=Bounds.Min+Bounds.GetSize()/FVector2D(Size)*FVector2D(X+.5,Y+.5);
				if(P.X<=-535 || P.X>=-365 || FMath::Abs(P.Y)>=32.5) continue;
				++Tested; const int32 I=Y*Size.X+X; const auto& S=R.FineHistory.GetSamples()[I];
				MissingCapture+=!R.LastLegalCaptureMask[I] || S.InitialRemembered<.999f;
				MissingAA+=S.FrozenAAEnvelope<.999f;
				MissingSubmitted+=!V || !V->SubmittedPresentation.IsValidIndex(I) || V->SubmittedPresentation[I].B*V->SubmittedPresentation[I].A<.999f;
			}
			TestTrue(TEXT("Independent interior has meaningful coverage"),Tested>100);
			TestEqual(FString(Stage)+TEXT(" complete geometric capture"),MissingCapture,0);
			TestEqual(FString(Stage)+TEXT(" no wall burned into frozen AA"),MissingAA,0);
			TestEqual(FString(Stage)+TEXT(" complete submitted history"),MissingSubmitted,0);
			TestTrue(TEXT("No hidden pose change"),R.SnapshotTransform.Equals(OriginalPose));
			if(V && V->Proxy.IsValid())
			{
				TInlineComponentArray<UStaticMeshComponent*> Parts(V->Proxy.Get());
				for(const auto* Part:Parts) { auto* MID=Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)); UTexture* Bound=nullptr; float Ready=0;
					TestTrue(TEXT("Actual visible proxy selects the submitted texture"),MID&&MID->GetTextureParameterValue(TEXT("SpatialStateTexture"),Bound)&&Bound==V->Texture.Get());
					TestTrue(TEXT("Actual proxy is ready"),MID&&MID->GetScalarParameterValue(TEXT("SpatialReady"),Ready)&&Ready==1&&!V->Proxy->IsHidden()); }
			}
			AddInfo(FString::Printf(TEXT("REOBSERVATION dt=%.6f stage=%s interior=%d missing_capture=%d missing_AA=%d missing_pixels=%d %s"),Dt,Stage,Tested,MissingCapture,MissingAA,MissingSubmitted,*Scene->GetCaptureRefreshAuditForTesting(Id)));
		};
		CheckHistory(TEXT("H1"));
		auto& Prop=Scene->Tracked.FindChecked(Id);
		const uint32 Epoch=Prop.History.GetRecords()[0].Epoch;
		const auto InitialProxy=Prop.Visuals.FindChecked(Epoch).Proxy; const auto InitialTexture=Prop.Visuals.FindChecked(Epoch).Texture;
		TArray<FLinearColor> SequencePixels;
		for(int32 Cycle=0;Cycle<4;++Cycle)
		{
			View(FVector2D(70,-250),155); Step(.5f);
			TestEqual(TEXT("Full reacquisition continues existing state"),Prop.History.GetRecords().Num(),1);
			TestTrue(TEXT("Near contact is fully legal"),Scene->GetLastLegalCoverageRatioForTesting(Id)>.99f);
			if(Cycle%2) for(float Yaw=155;Yaw<270;Yaw+=280*Dt)
			{ View(FVector2D(70,-250),Yaw); Scene->UpdateMemory(Dt,FVector(Source.BodyCenter,92)); }
			View(FVector2D(70,-250),-90); Scene->UpdateMemory(Dt,FVector(Source.BodyCenter,92));
			CheckHistory(TEXT("H2 first exit frame")); Step(.4f);
			TestTrue(TEXT("Repeated observation reuses proxy and texture"),InitialProxy==Prop.Visuals.FindChecked(Epoch).Proxy && InitialTexture==Prop.Visuals.FindChecked(Epoch).Texture);
			SequencePixels=Prop.Visuals.FindChecked(Epoch).SubmittedPresentation;
			View(FVector2D(70,-1000),115); Step(.4f); View(FVector2D(70,-1000),-90); Step(.4f);
			CheckHistory(TEXT("Repeated far exit"));
		}
		// Separate direct-full reference after finishing the continuous sequence.
		Scene->ResetMemory(); TestTrue(TEXT("Register independent reference"),Scene->RegisterRememberable(Memory,Policy));
		View(FVector2D(70,-250),155); Step(.5f); View(FVector2D(70,-250),-90); Step(.4f);
		CheckHistory(TEXT("Direct full reference"));
		const auto& Ref=Scene->Tracked.FindChecked(Id); const auto& RefR=Ref.History.GetRecords()[0];
		TestTrue(TEXT("Partial then full equals direct full pixel-for-pixel"),SequencePixels==Ref.Visuals.FindChecked(RefR.Epoch).SubmittedPresentation);
		Scene->ResetMemory(); Scene->Destroy(); World->DestroyWorld(true); GEngine->DestroyWorldContext(World);
	}
	return true;
}
#endif
