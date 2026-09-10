#if WITH_DEV_AUTOMATION_TESTS
#include "DarkwellSurfaceObservationFixture.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SightWeaveWorldSubsystem.h"
#include "SightWeaveHardCoverage.h"
#include "VisionPresentation/DarkwellStaticKnowledge.h"
#include "UObject/Package.h"

using namespace Darkwell::SurfaceObservationTests;
namespace
{
constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
struct FRuntimeWorld
{
 UWorld* World=nullptr;
 USightWeaveWorldSubsystem* Runtime=nullptr;
 FSightWeaveFloorId Floor{FName(TEXT("SurfaceFixture"))};
 FSightWeaveKnowledgeOwnerId Owner{FName(TEXT("Observer"))};
 FRuntimeWorld()
 {
  if(!GEngine) return;
  World=NewObject<UWorld>(GetTransientPackage(),MakeUniqueObjectName(GetTransientPackage(),UWorld::StaticClass(),TEXT("SurfaceContract")),RF_Transient);
  World->WorldType=EWorldType::Game;
  GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
  World->InitializeNewWorld(UWorld::InitializationValues().InitializeScenes(false).AllowAudioPlayback(false)
   .CreatePhysicsScene(false).RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false)
   .ShouldSimulatePhysics(false).SetTransactional(false));
  Runtime=World->GetSubsystem<USightWeaveWorldSubsystem>();
  if(Runtime)
  {
   FSightWeaveFloorDefinition F; F.FloorId=Floor; F.BoundsMin={-1000,-1000}; F.BoundsMax={1000,1000}; F.HeightRange={-100,500};
   if(!Runtime->RegisterFloor(F,nullptr)) Runtime=nullptr;
  }
 }
 ~FRuntimeWorld() { if(World) {GEngine->DestroyWorldContext(World); World->DestroyWorld(true);} }
 FSightWeaveVisionSourceDescription Vision(FVector Eye, bool Bypass=true) const
 {
  FSightWeaveVisionSourceDescription V; V.KnowledgeOwnerId=Owner; V.FloorId=Floor;
  V.HeightRange={-100,500}; V.Transform=FTransform(Eye); V.Shape=ESightWeaveSourceShape::Radial; V.Range=800;
  V.IlluminationPolicy=Bypass?ESightWeaveIlluminationPolicy::BypassLegalIllumination:ESightWeaveIlluminationPolicy::RequiresLegalIllumination;
  V.Compatibility.AcceptedCapabilities={FName(TEXT("Visible"))}; return V;
 }
 FSightWeaveVisibilityQueryResult Query(FVector P) const {return Runtime->QueryEffectiveLiveAtLocation(Owner,Floor,P);}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceGeometryContract,"Darkwell.SightWeave.SurfaceObservation.Spec.Geometry",Flags)
bool FSurfaceGeometryContract::RunTest(const FString&)
{
 const FVector Eye(-200,0,140);
 const TArray<FSolid> Tall{Cabinet()};
 const TArray<FSolid> Low{{FBox(FVector(-40,-30,0),FVector(40,30,75))}};
 TestTrue(TEXT("Low table top, including own solid and five support points"),GeometricSupport(Eye,Top(75),Low));
 TestTrue(TEXT("Cabinet front visible"),GeometricSupport(Eye,Front(),Tall));
 TestFalse(TEXT("Tall top not visible from below"),GeometricSupport(Eye,Top(200),Tall));
 TestTrue(TEXT("Raised observer sees the SAME top"),GeometricSupport({-200,0,260},Top(200),Tall));
 TestFalse(TEXT("Exactly coplanar eye has no positive front support"),GeometricSupport({-200,0,200},Top(200),Tall));
 auto HighFront=Front(); HighFront.Center.Z=170;
 TestTrue(TEXT("Front ABOVE 140 is visible: no global Z threshold"),GeometricSupport(Eye,HighFront,Tall));
 TestFalse(TEXT("Side initially unobserved"),GeometricSupport(Eye,Side(),Tall));
 TestTrue(TEXT("Walk around: side becomes visible"),GeometricSupport({0,200,140},Side(),Tall));
 TestFalse(TEXT("Walk around: old front no longer faces eye"),GeometricSupport({0,200,140},Front(),Tall));
 TestTrue(TEXT("Same XY low plane is visible"),GeometricSupport(Eye,Top(75),Low));
 TestFalse(TEXT("Same XY high plane is not visible"),GeometricSupport(Eye,Top(200),Tall));
 auto Underside=Top(75); Underside.Id=TEXT("Underside"); Underside.Normal=-FVector::UpVector;
 TestFalse(TEXT("Even identical XYZ may denote a distinct opposite face"),GeometricSupport(Eye,Underside,{}));
 TestTrue(TEXT("Self solid blocks a ray to far face"),CrossesInterior(Eye,{40,0,100},Cabinet()));
 TestFalse(TEXT("Receiver endpoint touching front is not self occlusion"),CrossesInterior(Eye,Front().Center,Cabinet()));
 TestTrue(TEXT("Observer inside solid cannot see through it"),CrossesInterior({0,0,100},Front().Center,Cabinet()));
 const TArray<FSolid> ShortBlocker{{FBox(FVector(-120,-50,0),FVector(-100,50,80))}};
 const TArray<FSolid> HighBlocker{{FBox(FVector(-120,-50,0),FVector(-100,50,180))}};
 TestTrue(TEXT("Ray clears short external blocker"),GeometricSupport(Eye,Top(75),ShortBlocker));
 TestFalse(TEXT("External blocker hides otherwise front-facing top"),GeometricSupport(Eye,Top(75),HighBlocker));
 // Center is clear, but positive-Y support intersects a narrow external solid.
 const TArray<FSolid> PartialBlocker{{FBox(FVector(-120,2,0),FVector(-100,20,180))}};
 TestFalse(TEXT("Center alone would pass"),CrossesInterior(Eye,Top(75).Center,PartialBlocker[0]));
 TestFalse(TEXT("A clear center does not grant the whole support cell"),GeometricSupport(Eye,Top(75),PartialBlocker));
 const FTransform Rotate(FRotator(0,37,0),FVector(340,-210,25));
 auto Rotated=Front(); Rotated.Center=Rotate.TransformPosition(Rotated.Center);
 Rotated.Normal=Rotate.TransformVectorNoScale(Rotated.Normal); Rotated.U=Rotate.TransformVectorNoScale(Rotated.U); Rotated.V=Rotate.TransformVectorNoScale(Rotated.V);
 auto Solid=Cabinet(); Solid.Pose=Rotate;
 const TArray<FSolid> RotatedSolids{Solid};
 TestTrue(TEXT("Rigid 37 degree fixture preserves front observation"),GeometricSupport(Rotate.TransformPosition(Eye),Rotated,RotatedSolids));
 // There is intentionally no camera argument anywhere in the observation oracle.
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceKnowledgeContract,"Darkwell.SightWeave.SurfaceObservation.Spec.Knowledge",Flags)
bool FSurfaceKnowledgeContract::RunTest(const FString&)
{
 FReferenceLedger K; const TArray<FSolid> Solids{Cabinet()};
 K.bWholeRecognized=true;
 TestEqual(TEXT("Whole identity recognition grants zero surface facts"),K.Known.Num(),0);
 auto Observe=[&](FVector Eye)
 {for(const auto& P:{Front(),Side(),Top(200)}) K.Observe(P.Id,GeometricSupport(Eye,P,Solids));};
 Observe({-200,0,140});
 TestTrue(TEXT("Front remembered"),K.Known.Contains(Front().Id));
 TestFalse(TEXT("Top unknown after Whole and front observation"),K.Known.Contains(Top(200).Id));
 Observe({0,200,140});
 TestTrue(TEXT("New side remembered"),K.Known.Contains(Side().Id));
 TestTrue(TEXT("Previously seen front retained while backfacing"),K.Known.Contains(Front().Id));
 K.bBlocked=true; K.Clear(); Observe({-200,0,260});
 TestEqual(TEXT("Clear then Block prevents new surface writes"),K.Known.Num(),0);
 K.bBlocked=false;
 TestEqual(TEXT("Release never restores surface facts"),K.Known.Num(),0);
 Observe({-200,0,260}); TestTrue(TEXT("New raised observation writes top"),K.Known.Contains(Top(200).Id));
 return true;
}

// Characterization is intentionally GREEN while the documented product gap
// exists. It does NOT assert surface acceptance. Replace it during receiver work.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceCurrentGaps,"Darkwell.SightWeave.SurfaceObservation.CurrentModel.KnownGaps",Flags)
bool FSurfaceCurrentGaps::RunTest(const FString&)
{
 FRuntimeWorld W; if(!TestNotNull(TEXT("Runtime"),W.Runtime)) return false;
 auto V=W.Vision({-200,0,140}); auto Handle=W.Runtime->RegisterVisionSource(V,nullptr);
 TestTrue(TEXT("Vision registered"),Handle.IsValid());
 TestTrue(TEXT("Current point model accepts low top"),W.Query(Top(75).Center).bEligibleForMemoryWrite);
 TestTrue(TEXT("Current point model accepts front"),W.Query(Front().Center).bEligibleForMemoryWrite);
 const TArray<FSolid> Tall{Cabinet()};
 const auto Below=W.Query(Top(200).Center);
 TestTrue(TEXT("GAP: old point query grants high top"),Below.bAuthoritative && Below.bEligibleForMemoryWrite);
 TestFalse(TEXT("Product requires rejection for the top receiver"),GeometricSupport({-200,0,140},Top(200),Tall));
 V.Transform.SetLocation({-200,0,260}); W.Runtime->UpdateVisionSource(Handle,V);
 TestTrue(TEXT("Raised eye point result remains true"),W.Query(Top(200).Center).bVisible);
 TestTrue(TEXT("Product geometry now accepts same receiver"),GeometricSupport({-200,0,260},Top(200),Tall));
 V.Transform.SetLocation({-200,0,140}); W.Runtime->UpdateVisionSource(Handle,V);
 TestTrue(TEXT("GAP: side point is live before walking around"),W.Query(Side().Center).bEligibleForMemoryWrite);
 TestFalse(TEXT("Product side is not observed yet"),GeometricSupport({-200,0,140},Side(),Tall));
 W.Runtime->PublishSnapshot();
 FDarkwellFogVisualSourceSnapshot S; S.AuthorityRevision=W.Runtime->AcquirePublishedSnapshot()->Revision.GetValue();
 S.BodyCenter=S.ConeOrigin={-200,0}; S.BodyRadiusCentimeters=800; S.ConeForward={1,0}; S.ConeRangeCentimeters=800; S.ConeHalfAngleDegrees=45;
 S.HardAuthority=MakeShared<FSightWeaveHardCoverageSet>(*W.Runtime,W.Runtime->AcquirePublishedSnapshot(),W.Owner,W.Floor);
 FDarkwellLayeredStaticKnowledge K; const FBox2D Area({-1,-1},{1,1}); K.Declare(Area); K.Observe(S,{},Area);
 TestTrue(TEXT("GAP: actual Static store remembers low XY plane"),K.HasMemory({.3125,.3125,75}));
 TestTrue(TEXT("GAP: actual Static store also remembers unobserved high XY plane"),K.HasMemory({.3125,.3125,200}));
 FSightWeaveSegment2D Wall; Wall.A={-110,-50}; Wall.B={-110,50}; Wall.FloorId=W.Floor; Wall.HeightRange={0,80};
 W.Runtime->RegisterOccluder({Wall},false,true,nullptr);
 TestFalse(TEXT("GAP: old height-overlap XY wall rejects view over low blocker"),W.Query(Top(75).Center).bVisible);
 const TArray<FSolid> LowBlocker{{FBox(FVector(-120,-50,0),FVector(-100,50,80))}};
 TestTrue(TEXT("Product sloped ray clears finite low blocker"),GeometricSupport({-200,0,140},Top(75),LowBlocker));
 AddInfo(TEXT("Known gaps reproduced; no production Surface Receiver exists. Spec tests are a reference, not integration acceptance."));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceRuntimePolicies,"Darkwell.SightWeave.SurfaceObservation.CurrentModel.PolicyAndMemory",Flags)
bool FSurfaceRuntimePolicies::RunTest(const FString&)
{
 FRuntimeWorld W; if(!TestNotNull(TEXT("Runtime"),W.Runtime)) return false;
 const FVector Eye(-200,0,140); const FVector P(0,0,75);
 auto V=W.Vision(Eye,false); V.Shape=ESightWeaveSourceShape::DirectionalCone; V.HalfAngleDegrees=45;
 auto H=W.Runtime->RegisterVisionSource(V,nullptr);
 TestFalse(TEXT("Vision without legal illumination does not write"),W.Query(P).bEligibleForMemoryWrite);
 FSightWeaveIlluminationSourceDescription L; L.KnowledgeOwnerId=W.Owner; L.FloorId=W.Floor;
 L.Transform=FTransform(Eye); L.HeightRange={-100,500}; L.Range=800; L.EmittedCapabilities={FName(TEXT("Other"))};
 auto Light=W.Runtime->RegisterIlluminationSource(L,nullptr);
 TestFalse(TEXT("Incompatible illumination cannot grant observation"),W.Query(P).bVisible);
 L.EmittedCapabilities={FName(TEXT("Visible"))}; W.Runtime->UpdateIlluminationSource(Light,L);
 TestTrue(TEXT("Compatible light enables original Hard write"),W.Query(P).bEligibleForMemoryWrite);
 V.Transform.SetRotation(FRotator(0,180,0).Quaternion()); W.Runtime->UpdateVisionSource(H,V);
 TestFalse(TEXT("Light alone outside observation direction cannot write"),W.Query(P).bEligibleForMemoryWrite);
 V.Transform.SetRotation(FQuat::Identity); W.Runtime->UpdateVisionSource(H,V);
 W.Runtime->PublishSnapshot();
 FDarkwellFogVisualSourceSnapshot S; S.AuthorityRevision=W.Runtime->AcquirePublishedSnapshot()->Revision.GetValue();
 // Legacy snapshot validity requires positive body metadata. HardAuthority is
 // the only coverage source; this metadata does not register a bypass source.
 S.BodyCenter=S.ConeOrigin={-200,0}; S.BodyRadiusCentimeters=120; S.ConeForward={1,0}; S.ConeRangeCentimeters=800; S.ConeHalfAngleDegrees=45; S.bConeLegallyLive=true;
 S.HardHeight=75; S.HardAuthority=MakeShared<FSightWeaveHardCoverageSet>(*W.Runtime,W.Runtime->AcquirePublishedSnapshot(),W.Owner,W.Floor);
 if(!TestTrue(TEXT("Valid Hard-backed snapshot"),S.IsValid() && S.HardAuthority->IsReady())) return false;
 FDarkwellStaticKnowledge K; const FBox2D Area({-1,-1},{1,1}); K.Declare(Area); K.Observe(S,{},Area);
 TestTrue(TEXT("Real store writes same Hard support"),K.HasMemory({.3125,.3125}));
 K.SetBlock(Area,true); K.Clear(Area); K.Observe(S,{},Area);
 TestFalse(TEXT("Real store Clear/Block prevents writes"),K.HasMemory({.3125,.3125}));
 TestTrue(TEXT("Block does not disable Hard Live"),W.Query(P).bVisible);
 K.SetBlock(Area,false); TestFalse(TEXT("Real store release does not restore"),K.HasMemory({.3125,.3125}));
 K.Observe(S,{},Area); TestTrue(TEXT("Real store reobservation restores"),K.HasMemory({.3125,.3125}));
 L.bActive=false; W.Runtime->UpdateIlluminationSource(Light,L);
 auto Body=W.Vision({-80,0,140}); Body.Range=120;
 W.Runtime->RegisterVisionSource(Body,nullptr);
 TestTrue(TEXT("Near circle retains darkness bypass"),W.Query(P).bUsedBypass && W.Query(P).bEligibleForMemoryWrite);
 const TArray<FSolid> Tall{Cabinet()};
 TestFalse(TEXT("SPEC: near bypass must not grant backfacing high top"),GeometricSupport({-80,0,140},Top(200),Tall));
 FSightWeaveHardSuppressionDescription Suppression; Suppression.FloorId=W.Floor; Suppression.Center={0,0}; Suppression.Radius=30; Suppression.HeightRange={-100,500};
 W.Runtime->RegisterHardLiveSuppression(Suppression,nullptr);
 TestTrue(TEXT("Suppression still defeats body bypass"),W.Query(P).bRejectedBySuppression && !W.Query(P).bEligibleForMemoryWrite);
 return true;
}
#endif
