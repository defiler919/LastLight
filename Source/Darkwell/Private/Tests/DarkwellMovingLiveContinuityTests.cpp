#if WITH_DEV_AUTOMATION_TESTS
#include "DarkwellLegacyObjectPolicyFixture.h"
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
	int32 TextureCreations(const FString& Telemetry)
	{
		const FString Key=TEXT("\"texture_creations\":");
		const int32 Start=Telemetry.Find(Key);
		return Start==INDEX_NONE?-1:FCString::Atoi(*Telemetry.Mid(Start+Key.Len()));
	}
	struct FRoom
	{
		FDarkwellLegacyObjectPolicyFixture LegacyPolicy;
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
 int32 Collapsed=0; TArray<double> GT;
 const auto Initial=F.Room->GetHistoryRuntimeFrameTelemetryForTesting();
 uint64 Queries=0,Uploads=0,HistoricalScans=0,CapRebuilds=0;
 for(int32 I=0;I<240;++I) {
  F.Step(); const auto Perf=F.Room->GetHistoryRuntimeFrameTelemetryForTesting();
  GT.Add(Perf.MovingPropLabGameThreadUs); Queries+=Perf.CoverageQueries; Uploads+=Perf.TextureUploads;
  HistoricalScans+=Perf.FineSamplesScanned; CapRebuilds+=Perf.CapMeshRebuilds;
  const FString Row=F.Room->GetMovingLiveTelemetry(Id);
  TestTrue(TEXT("Stable interior appearance every motion frame"),Row.Contains(TEXT("\"appearance\":[1.000000,1.000000,1.000000]")));
  TestEqual(TEXT("Current allocations do not grow across world AABB changes"),TextureCreations(Row),TextureCreations(Before));
  AddInfo(FString::Printf(TEXT("MOVING_LIVE_FRAME %d "),I)+Row);
  if(Row.Contains(TEXT("\"appearance\":[0.083333,0.083333,0.083333]"))) ++Collapsed;
  TestEqual(TEXT("Single current epoch"),F.Room->GetCurrentEpochCountForTesting(Id),1);
  TestEqual(TEXT("No stale epoch during continuous sight"),F.Room->GetStaleEpochCountForTesting(Id),0);
 }
 // Exact same 240-frame baseline route now asserts preserved local evidence.
 TestEqual(TEXT("No repeated appearance collapse"),Collapsed,0);
 TestTrue(TEXT("Ordinary pose changes never reinitialize"),F.Room->GetMovingLiveTelemetry(Id).Contains(TEXT("\"initialize\":1,")));
 AddInfo(FString::Printf(TEXT("MOVING_LIVE_BASELINE_COLLAPSED=%d/240"),Collapsed));
 const auto End=F.Room->GetHistoryRuntimeFrameTelemetryForTesting();
 double Sum=0; for(double T:GT) Sum+=T; GT.Sort();
 AddInfo(FString::Printf(TEXT("MOVING_LIVE_PERF mean_us=%.3f p50_us=%.3f p95_us=%.3f p99_us=%.3f peak_us=%.3f queries_per_frame=%.3f uploads_per_frame=%.3f history_scans=%llu cap_rebuilds=%llu uobject_delta=%d working_set_delta=%lld"),
 Sum/240,GT[119],GT[227],GT[237],GT.Last(),double(Queries)/240,double(Uploads)/240,HistoricalScans,CapRebuilds,End.UObjectCount-Initial.UObjectCount,int64(End.ProcessWorkingSetBytes)-int64(Initial.ProcessWorkingSetBytes)));
 TestTrue(TEXT("Motion upload lifetime stays bounded"),End.ProcessWorkingSetBytes<=Initial.ProcessWorkingSetBytes+64ull*1024ull*1024ull);
 TestEqual(TEXT("Motion does not scan histories that do not exist"),HistoricalScans,uint64(0));
 TestEqual(TEXT("Fully discovered motion has no cap boundary rebuild"),CapRebuilds,uint64(0));
 F.Step(60); double Idle=0; for(int32 I=0;I<600;++I) { F.Step();Idle+=F.Room->GetHistoryRuntimeFrameTelemetryForTesting().MovingPropLabGameThreadUs; }
 AddInfo(FString::Printf(TEXT("MOVING_LIVE_IDLE_600 mean_us=%.3f"),Idle/600));
 return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FDarkwellMovingLiveContract,
 "Darkwell.PropLab.MovingLiveContinuity", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FDarkwellMovingLiveContract::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
 for(const TCHAR* N : {TEXT("ContinuousVisibleTranslationDoesNotResetLive"),TEXT("RotationAcrossWorldBoundsDimensionSwap"),
 TEXT("PartialCoverageMovingContinuity"),TEXT("ViewLossStartsNewEntryOnlyWhenAppropriate"),TEXT("AlwaysFreezesLastLegalMovingPose"),
 TEXT("StationaryOnlyMovingLiveContinuity"),TEXT("NeverMovingLiveContinuity"),TEXT("FrameRateInvariantMovingLive"),TEXT("TopologyChangeRequiresExplicitReset"),
 TEXT("OwnershipDirtyRegionsAreIncremental")})
 { Names.Add(N); Commands.Add(N); }
}
bool FDarkwellMovingLiveContract::RunTest(const FString& Case)
{
 using namespace Darkwell::MovingLiveTests;
 if(Case==TEXT("RotationAcrossWorldBoundsDimensionSwap") || Case==TEXT("PartialCoverageMovingContinuity")
    || Case==TEXT("FrameRateInvariantMovingLive") || Case==TEXT("TopologyChangeRequiresExplicitReset")
    || Case==TEXT("OwnershipDirtyRegionsAreIncremental"))
 {
  using Grid=FDarkwellCurrentLiveGrid;
  TArray<Grid::FDescriptor> Descriptors{
   {1,10,FBox(FVector(-75,-40,0),FVector(75,40,190)),FTransform::Identity},
   {2,10,FBox(FVector(-70,-2,0),FVector(70,2,180)),FTransform(FVector(0,-43,0))},
   {3,10,FBox(FVector(-3,-3,-20),FVector(3,3,20)),FTransform(FVector(55,-50,90))}};
  Grid G; G.ResetGeometry(Id,Descriptors,FTransform::Identity);
  auto Full=[](FVector2D){return 1.f;};
  if(Case==TEXT("OwnershipDirtyRegionsAreIncremental"))
  {
   auto Left=[](const FVector2D Point){return Point.X<0?1.f:0.f;};
   G.Advance(.2f,FTransform::Identity,Left);
   TestTrue(TEXT("First legal ownership reports conservative dirty regions"),!G.OwnershipDirtyRegions.IsEmpty());
   G.Advance(.2f,FTransform::Identity,Left);
   TestTrue(TEXT("Unchanged captured ownership reports no dirty regions"),G.OwnershipDirtyRegions.IsEmpty());
   G.Advance(.2f,FTransform::Identity,Full);
   TestTrue(TEXT("Newly captured remainder reports new dirty regions"),!G.OwnershipDirtyRegions.IsEmpty());
   return true;
  }
  if(Case==TEXT("TopologyChangeRequiresExplicitReset"))
  {
   G.Advance(.2f,FTransform::Identity,Full); const uint64 Hash=G.StateHash();
   TestTrue(TEXT("Rigid pose retains descriptor identity"),G.MatchesGeometry(Descriptors,FTransform(FRotator(0,90,0),FVector(100,0,0))));
   Descriptors[2].MeshKey=99;
   TestFalse(TEXT("Changed primitive requires explicit reset"),G.MatchesGeometry(Descriptors,FTransform::Identity));
   TestEqual(TEXT("Detection cannot erase observation"),G.StateHash(),Hash);
   G.ResetGeometry(Id,Descriptors,FTransform::Identity);
   TestEqual(TEXT("Only explicit reset changes geometry generation"),G.GeometryResets,uint64(2));
   TestTrue(TEXT("New descriptor accepted"),G.MatchesGeometry(Descriptors,FTransform::Identity));
   for(const auto& P:G.Parts) for(const auto& C:P.Local.GetCells()) TestEqual(TEXT("New topology cannot reuse old appearance"),C.AppearanceBlend,0.f);
   return true;
  }
  if(Case==TEXT("FrameRateInvariantMovingLive"))
  {
   uint64 Reference=0;
   for(int32 Hz : {30,60,120,144})
   {
    Grid H; H.ResetGeometry(Id,Descriptors,FTransform::Identity);
    for(int32 I=0;I<Hz;++I) H.Advance(1.f/Hz,FTransform::Identity,Full);
    for(int32 I=1;I<=4*Hz;++I) H.Advance(1.f/Hz,FTransform(FRotator(0,180.*I/(4*Hz),0),FVector::ZeroVector),Full);
    if(!Reference) Reference=H.StateHash();
    TestEqual(TEXT("Ordered local-state hash identical across rates"),H.StateHash(),Reference);
    TestEqual(TEXT("No destructive geometry reset during motion"),H.GeometryResets,uint64(1));
    for(const auto& P:H.Parts) { TestEqual(TEXT("Per-primitive initialization fixed"),P.Local.GetInitializeCount(),uint64(1));
     for(const auto& C:P.Local.GetCells()) TestEqual(TEXT("All final local samples fully Live"),C.LiveBlend,1.f); }
    AddInfo(FString::Printf(TEXT("MOVING_LIVE_RATE hz=%d hash=%llu samples=%llu resets=%llu"),Hz,H.StateHash(),H.SamplesTouched,H.GeometryResets));
   }
   FString RoomReference;
   for(int32 Hz : {30,60,120,144})
   {
    FRoom R;
    auto Step=[&]() { R.Adapter->Tick(1.f/Hz);R.Room->UpdateRoom(1.f/Hz,R.Player);R.Fixture->Tick(1.f/Hz); };
    for(int32 I=0;I<Hz;++I) Step();
    const int32 InitialTextures=TextureCreations(R.Room->GetMovingLiveTelemetry(Id));
    R.Room->StartTrackedRotationForTesting(Id,180,4);
    for(int32 I=0;I<4*Hz;++I)
    {
     Step(); const auto Row=R.Room->GetMovingLiveTelemetry(Id);
     TestEqual(TEXT("Real Lab texture creation stays fixed at every rate"),TextureCreations(Row),InitialTextures);
     TestTrue(TEXT("Real Lab current never destructively reinitializes"),Row.Contains(TEXT("\"initialize\":1,")));
     TestTrue(TEXT("Real Lab stable interior stays solid at every rate"),Row.Contains(TEXT("\"appearance\":[1.000000,1.000000,1.000000]")));
     TestEqual(TEXT("Real Lab has one continuous current"),R.Room->GetCurrentEpochCountForTesting(Id),1);
     TestEqual(TEXT("Real Lab has no motion chain"),R.Room->GetStaleEpochCountForTesting(Id),0);
    }
    const auto Row=R.Room->GetMovingLiveTelemetry(Id);
    const FString Key=TEXT("\"local_state_hash\":\""); const int32 Start=Row.Find(Key)+Key.Len();
    const int32 End=Row.Find(TEXT("\""),ESearchCase::CaseSensitive,ESearchDir::FromStart,Start);
    const FString Hash=Row.Mid(Start,End-Start);
    if(RoomReference.IsEmpty()) RoomReference=Hash;
    TestEqual(TEXT("Real Lab ordered local state identical across rates"),Hash,RoomReference);
    AddInfo(FString::Printf(TEXT("MOVING_LIVE_REAL_RATE hz=%d %s"),Hz,*Row));
   }
   return true;
  }
  if(Case==TEXT("PartialCoverageMovingContinuity"))
  {
   auto Partial=[](FVector2D W){return W.X<0 ? 1.f:0.f;};
   for(int32 I=0;I<30;++I) G.Advance(1.f/60,FTransform::Identity,Partial);
   TestFalse(TEXT("Half sight cannot reveal whole object"),G.bFullyObservedAtPose);
   const uint64 Before=G.StateHash();
   G.WritePartRasters(Partial,true);
   TestEqual(TEXT("AA/raster construction cannot change local evidence"),G.StateHash(),Before);
   int32 Hidden=0,Live=0;
   for(int32 I=0;I<60;++I)
   {
    G.Advance(1.f/60,FTransform(FRotator(0,I*.5,0),FVector::ZeroVector),Partial);
    G.WritePartRasters(Partial,true);
    for(const auto& P:G.Parts) for(const auto& C:P.Raster.GetCells())
     if(C.CurrentLegalCoverage==0) { ++Hidden; TestEqual(TEXT("Current illegal samples hard zero"),C.AppearanceBlend,0.f); }
     else if(C.AppearanceBlend==1) ++Live;
   }
   TestTrue(TEXT("Both visible and hidden populations exercised"),Live>0 && Hidden>0);
   G.Advance(1.f/60,G.LastLegalPose,Full);
   int32 Entering=0;
   for(const auto& P:G.Parts) for(const auto& C:P.Local.GetCells()) Entering+=C.AppearanceBlend>0 && C.AppearanceBlend<1;
   TestTrue(TEXT("Newly legal area retains .20 entry, not WholeObject"),Entering>0);
   TestEqual(TEXT("Enter unchanged"),FDarkwellSpatialPropMemory::EnterSeconds,.20f);
   TestEqual(TEXT("Exit unchanged"),FDarkwellSpatialPropMemory::ExitSeconds,.18f);
   return true;
  }
  G.Advance(.2f,FTransform::Identity,Full); const uint64 Hash=G.StateHash();
  const auto Atlas=G.AtlasCells; TArray<FIntPoint> Dimensions;
  for(const auto& P:G.Parts) Dimensions.Add(P.Local.GetSize());
  FDarkwellSpatialPropMemory World; World.Initialize(Id,FBox2D(FVector2D(-80),FVector2D(80))); World.BeginPresent();
  for(float Yaw : {0.f,45.f,89.f,90.f,91.f,135.f,180.f})
  {
   FTransform Pose(FRotator(0,Yaw,0),FVector::ZeroVector);
   G.Advance(1.f/60,Pose,Full); G.WritePartRasters(Full,false);
   for(int32 PartIndex=0;PartIndex<G.Parts.Num();++PartIndex)
   {
    const auto& P=G.Parts[PartIndex]; const auto L=P.Geometry.LocalBounds.Min+P.Geometry.LocalBounds.GetSize()*FVector(.001,.001,.5);
    TestTrue(TEXT("Fine primitive edge ownership reads local evidence, not coarse world cell center"),G.HasObservedContributionAt(FVector2D(P.Pose.TransformPosition(L)),PartIndex));
   }
   TestEqual(TEXT("90 degree swap preserves local evidence"),G.StateHash(),Hash);
   TestEqual(TEXT("Atlas capacity does not rotate"),G.AtlasCells,Atlas);
   FBox Bounds(ForceInit);
   for(int32 I=0;I<G.Parts.Num();++I) {
    TestEqual(TEXT("Primitive dimensions stable"),G.Parts[I].Local.GetSize(),Dimensions[I]);
    TestEqual(TEXT("Independent primitive identity retained"),G.Parts[I].Geometry.PrimitiveKey,uint64(I+1));
    Bounds+=G.Parts[I].Geometry.LocalBounds.TransformBy(G.Parts[I].Pose);
   }
   G.WriteWorldSnapshot(World,FBox2D(FVector2D(Bounds.Min),FVector2D(Bounds.Max)));
   TestEqual(TEXT("Derived world raster does not reinitialize"),World.GetInitializeCount(),uint64(1));
   // Final ownership must inverse-map into a real part; the auxiliary
   // geometry-clipped envelope is not a source of occupancy in handle gaps.
   const auto S=World.GetSize();const auto B=World.GetBounds();const auto Step=B.GetSize()/FVector2D(S);
   for(int32 I=0;I<World.GetCells().Num();++I)
   {
    const auto W=B.Min+Step*FVector2D(I%S.X+.5,I/S.X+.5); bool Inside=false;
    for(const auto& P:G.Parts) {
     auto L=P.Pose.InverseTransformPosition(FVector(W,P.Pose.TransformPosition(P.Geometry.LocalBounds.GetCenter()).Z));
     Inside|=FBox2D(FVector2D(P.Geometry.LocalBounds.Min),FVector2D(P.Geometry.LocalBounds.Max)).IsInside(FVector2D(L));
    }
    TestEqual(TEXT("No body/door/handle empty gap claims ownership"),G.HasObservedContributionAt(W),Inside);
   }
  }
  return true;
 }
 FRoom F;
 const bool Never=Case.StartsWith(TEXT("Never")), Stationary=Case.StartsWith(TEXT("StationaryOnly"));
 if(Never||Stationary) TestTrue(TEXT("Explicit policy registration"),F.Room->ResetTrackedPolicyForLab(Id,Never?Mode::Never:Mode::StationaryOnly));
 F.Step(30);
 if(Case.StartsWith(TEXT("ContinuousVisibleTranslation")))
 {
  const int32 InitialTextures=TextureCreations(F.Room->GetMovingLiveTelemetry(Id));
  const FTransform Start=F.Room->GetTrackedTransform(Id);
  F.Room->GetObjectPolicyForTesting(Id)->SetSightWeaveMoving(true);
  for(int32 I=1;I<=240;++I) {
   auto Pose=Start;Pose.AddToTranslation(FVector(120.*I/240,0,0));F.Room->SetTrackedTransformForTesting(Id,Pose);F.Step();
   const auto Row=F.Room->GetMovingLiveTelemetry(Id);
   TestTrue(TEXT("Translation keeps stable appearance"),Row.Contains(TEXT("\"appearance\":[1.000000,1.000000,1.000000]")));
   TestEqual(TEXT("Texture count stable"),TextureCreations(Row),InitialTextures);
   TestEqual(TEXT("No path histories"),F.Room->GetStaleEpochCountForTesting(Id),0);
  }
  F.Room->GetObjectPolicyForTesting(Id)->SetSightWeaveMoving(false); return true;
 }
 F.Room->StartTrackedRotationForTesting(Id,180,4);
 const bool LastPose=Case.StartsWith(TEXT("AlwaysFreezes")), Invalid=Case.StartsWith(TEXT("ViewLoss"));
 const int32 Frames=LastPose||Invalid?80:120;
 for(int32 I=0;I<Frames;++I) { F.Step();
  TestTrue(TEXT("All capture modes preserve current appearance"),F.Room->GetMovingLiveTelemetry(Id).Contains(TEXT("\"appearance\":[1.000000,1.000000,1.000000]")));
  TestEqual(TEXT("No stale while continuously legal"),F.Room->GetStaleEpochCountForTesting(Id),0);
  if(Never) TestEqual(TEXT("Never has no history resources"),F.Room->GetHistoricalPresentationResourceCountForTesting(Id),0);
 }
 const float LastYaw=F.Room->GetTrackedTransform(Id).Rotator().Yaw;
 F.Face(-90);
 if(Invalid) {
  F.Room->InjectInvalidCoverageOnceForTesting(Id);F.Step();
  TestEqual(TEXT("Invalid revision cannot seal historical evidence"),F.Room->GetStaleEpochCountForTesting(Id),0);
  TestFalse(TEXT("Invalid coverage hides source"),F.Room->IsCurrentSourceVisibleForTesting(Id));
 }
 F.Step();
 if(LastPose||Invalid) {
  TestEqual(TEXT("Exactly one last legal freeze"),F.Room->GetStaleEpochCountForTesting(Id),1);
  TestEqual(TEXT("Fully observed pose has no artificial AABB cut caps"),F.Room->GetVisibleHistoricalCapCountForTesting(Id),0);
  TestTrue(TEXT("Snapshot is last legal pose, not origin or hidden next frame"),FMath::Abs(F.Room->GetNewestHistoricalYawForTesting(Id)-LastYaw)<.01);
 }
 else {
  TestEqual(TEXT("Transient moving pose abandoned"),F.Room->GetCurrentEpochCountForTesting(Id),0);
  TestEqual(TEXT("Moving history/proxy/cap/resources zero"),F.Room->GetHistoricalPresentationResourceCountForTesting(Id),0);
 }
 F.Step(270); TestEqual(TEXT("Hidden finish cannot create observation"),F.Room->GetCurrentEpochCountForTesting(Id),0);
 F.Face(90);F.Step();
 TestTrue(TEXT("New observation uses real entry transition"),F.Room->GetMovingLiveTelemetry(Id).Contains(TEXT("\"appearance\":[0.083333,0.083333,0.083333]")));
 F.Step(30);F.Face(-90);F.Step(30);
 if(Stationary) TestEqual(TEXT("Only freshly observed final static pose freezes"),F.Room->GetStaleEpochCountForTesting(Id),1);
 if(Never) TestEqual(TEXT("Never remains zero after repeat view loss"),F.Room->GetHistoricalPresentationResourceCountForTesting(Id),0);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellMovingLiveAtlasClamp,
 "Darkwell.PropLab.MovingLiveContinuity.CurrentAtlasBilinearBoundary", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellMovingLiveAtlasClamp::RunTest(const FString&)
{
 const FIntPoint Atlas(28,28); TArray<FFloat16Color> Packed;Packed.SetNumUninitialized(Atlas.X*Atlas.Y);
 for(const FIntPoint Size : {FIntPoint(24,8),FIntPoint(8,24),FIntPoint(4,4)})
 {
  TArray<FLinearColor> Pixels;Pixels.Init(FLinearColor(1,1,0,1),Size.X*Size.Y);
  FDarkwellCurrentLiveGrid::CopyAtlasWithClampBorder(Pixels,Size,Atlas,Packed);
  // GPU bilinear at the maximum XY corner averages these four texels.
  float Alpha=0;
  for(int32 Y=Size.Y-1;Y<=Size.Y;++Y) for(int32 X=Size.X-1;X<=Size.X;++X) Alpha+=Packed[Y*Atlas.X+X].R.GetFloat()*.25f;
  TestEqual(TEXT("Solid physical corner remains one, not old zero-padding quarter-alpha"),Alpha,1.f);
  for(int32 Y=0;Y<Atlas.Y;++Y) for(int32 X=0;X<Atlas.X;++X)
   if(X>Size.X || Y>Size.Y) TestEqual(TEXT("Old larger pose is cleared outside clamp border"),Packed[Y*Atlas.X+X].R.GetFloat(),0.f);
  for(int32 Y=0;Y<Size.Y;++Y) Pixels[Y*Size.X+Size.X-1]=FLinearColor::Transparent;
  Pixels[0]=FLinearColor(.25,.5,0,1);
  FDarkwellCurrentLiveGrid::CopyAtlasWithClampBorder(Pixels,Size,Atlas,Packed);
  for(int32 Y=0;Y<=Size.Y;++Y) TestEqual(TEXT("Illegal boundary stays strictly zero, no coverage dilation"),Packed[Y*Atlas.X+Size.X].R.GetFloat(),0.f);
  TestEqual(TEXT("Existing inward AA values are unchanged"),Packed[0].R.GetFloat(),.25f);
  TestEqual(TEXT("Existing live blend is unchanged"),Packed[0].G.GetFloat(),.5f);
 }
 return true;
}
#endif
