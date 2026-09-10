#if WITH_DEV_AUTOMATION_TESTS
#include "DarkwellSurfaceObservationFixture.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SightWeaveWorldSubsystem.h"
#include "SightWeaveHardCoverage.h"
#include "VisionPresentation/DarkwellStaticKnowledge.h"
#include "UObject/Package.h"
#include "Player/DarkwellCharacter.h"
#include "Player/DarkwellObserverComponent.h"
#include "VisionPresentation/DarkwellSurfaceKnowledge.h"
#include "VisionPresentation/DarkwellSurfaceKnowledgeSubsystem.h"
#include "VisionPresentation/DarkwellStaticEnvironmentSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "RHICommandList.h"
#include "RenderingThread.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Components/PointLightComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "VisionPresentation/DarkwellObjectMemoryScene.h"
#include "VisionPresentation/DarkwellRememberablePropComponent.h"
#include "SightWeaveObjectPolicy.h"
#include "SightWeaveRenderWorldSubsystem.h"
#include "VisionPresentation/DarkwellApartmentLab.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"

using namespace Darkwell::SurfaceObservationTests;
namespace
{
constexpr auto SurfaceTestFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
struct FRuntimeWorld
{
 UWorld* World=nullptr;
 USightWeaveWorldSubsystem* Runtime=nullptr;
 FSightWeaveFloorId Floor{FName(TEXT("SurfaceFixture"))};
 FSightWeaveKnowledgeOwnerId Owner{FName(TEXT("Observer"))};
 FRuntimeWorld(bool Physics=false)
 {
  if(!GEngine) return;
  World=NewObject<UWorld>(GetTransientPackage(),MakeUniqueObjectName(GetTransientPackage(),UWorld::StaticClass(),TEXT("SurfaceContract")),RF_Transient);
  World->WorldType=EWorldType::Game;
  GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
  World->InitializeNewWorld(UWorld::InitializationValues().InitializeScenes(Physics).AllowAudioPlayback(false)
   .CreatePhysicsScene(Physics).RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceGeometryContract,"Darkwell.SightWeave.SurfaceObservation.Spec.Geometry",SurfaceTestFlags)
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceKnowledgeContract,"Darkwell.SightWeave.SurfaceObservation.Spec.Knowledge",SurfaceTestFlags)
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceCurrentGaps,"Darkwell.SightWeave.SurfaceObservation.LegacyConsumers.KnownGaps",SurfaceTestFlags)
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
 AddInfo(TEXT("Legacy XY consumer gaps reproduced. RuntimeV1 tests cover the new receiver API; Object/Static/P4 surface migration remains separate."));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceRuntimePolicies,"Darkwell.SightWeave.SurfaceObservation.CurrentModel.PolicyAndMemory",SurfaceTestFlags)
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

namespace
{
FSightWeaveSurfaceBox Receiver(FRuntimeWorld& W,double Height=200)
{
 FSightWeaveSurfaceBox B;B.Id=TEXT("Cabinet");B.Floor=W.Floor;B.Pose=FTransform(FVector(0,0,Height/2));B.HalfExtent={40,30,Height/2};return B;
}
FSightWeaveSurfaceSample Sample(ESightWeaveBoxFace Face,FVector2D UV=FVector2D::ZeroVector)
{return {TEXT("Cabinet"),Face,UV};}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceV1Geometry,"Darkwell.SightWeave.SurfaceObservation.RuntimeV1.Geometry",SurfaceTestFlags)
bool FSurfaceV1Geometry::RunTest(const FString&)
{
 FRuntimeWorld W;if(!TestNotNull(TEXT("Runtime"),W.Runtime))return false;
 auto Box=Receiver(W);TestTrue(TEXT("Register receiver"),W.Runtime->RegisterSurfaceBox(Box));
 auto V=W.Vision({-200,0,140});auto H=W.Runtime->RegisterVisionSource(V,nullptr);
 auto Visible=[&](ESightWeaveBoxFace Face){return W.Runtime->QuerySurfaceSample(W.Owner,Sample(Face)).Hard.bEligibleForMemoryWrite;};
 TestTrue(TEXT("Real receiver front"),Visible(ESightWeaveBoxFace::NegativeX));
 TestFalse(TEXT("Real receiver rejects high top"),Visible(ESightWeaveBoxFace::Top));
 TestFalse(TEXT("Real receiver rejects unseen side"),Visible(ESightWeaveBoxFace::PositiveY));
 const auto Before=W.Runtime->AcquirePublishedSnapshot();
 V.Transform.SetLocation({-200,0,260});W.Runtime->UpdateVisionSource(H,V);
 TestTrue(TEXT("Raised real observer sees same top"),Visible(ESightWeaveBoxFace::Top));
 TestTrue(TEXT("Observer change reuses immutable geometry acceleration"),Before->SurfaceScene==W.Runtime->AcquirePublishedSnapshot()->SurfaceScene);
 V.Transform.SetLocation({0,200,140});W.Runtime->UpdateVisionSource(H,V);
 TestTrue(TEXT("New side becomes eligible"),Visible(ESightWeaveBoxFace::PositiveY));
 TestFalse(TEXT("Old front no longer live"),Visible(ESightWeaveBoxFace::NegativeX));
 Box=Receiver(W,75);W.Runtime->UpdateSurfaceBox(Box);V.Transform.SetLocation({-200,0,140});W.Runtime->UpdateVisionSource(H,V);
 TestTrue(TEXT("Low top real receiver"),Visible(ESightWeaveBoxFace::Top));
 FSightWeaveSegment2D Wall;Wall.A={-110,-50};Wall.B={-110,50};Wall.FloorId=W.Floor;Wall.HeightRange={0,80};
 const auto WallHandle=W.Runtime->RegisterOccluder({Wall},true,true,nullptr);
 TestFalse(TEXT("Legacy XY still blocked, cannot be surface broad-phase rejection"),W.Query({0,0,75}).bVisible);
 TestTrue(TEXT("Finite wall: surface ray clears low obstacle"),Visible(ESightWeaveBoxFace::Top));
 Wall.HeightRange.ZMax=180;W.Runtime->UpdateOccluder(WallHandle,{Wall},true,true);
 TestFalse(TEXT("Finite wall raised now blocks"),Visible(ESightWeaveBoxFace::Top));
 W.Runtime->UnregisterOccluder(WallHandle);
 auto Block=Box;Block.Id=TEXT("External");Block.Pose=FTransform(FVector(-110,0,90));Block.HalfExtent={10,50,90};W.Runtime->RegisterSurfaceBox(Block);
 TestFalse(TEXT("External finite solid blocks"),Visible(ESightWeaveBoxFace::Top));
 Block.HalfExtent.Z=40;Block.Pose.SetLocation({-110,0,40});W.Runtime->UpdateSurfaceBox(Block);
 TestTrue(TEXT("External low solid can be seen over"),Visible(ESightWeaveBoxFace::Top));
 W.Runtime->UnregisterSurfaceBox(Block.Id);
 Box=Receiver(W);W.Runtime->UpdateSurfaceBox(Box);
 // Independent oracle: manually specify each local face; do not use production Resolve.
 const FVector Centers[]={{-40,0,100},{40,0,100},{0,-30,100},{0,30,100},{0,0,0},{0,0,200}};
 const FVector Normals[]={{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
 int Checks=0;
 for(double Yaw:{0.,37.,90.})
 {
  const FRotator Rotation(Yaw==37?20:0,Yaw,0);
  const FTransform Pose(Rotation,FVector(0,0,0));Box.Pose=FTransform(Rotation,Pose.TransformPosition(FVector(0,0,100)));W.Runtime->UpdateSurfaceBox(Box);
  auto Solid=Cabinet();Solid.Pose=Pose;const TArray<FSolid> Solids{Solid};
  for(FVector Eye:{FVector(-200,0,140),FVector(0,200,140),FVector(200,0,260),FVector(0,0,100),FVector(-200,0,200)})
  {
   V.Transform.SetLocation(Pose.TransformPosition(Eye));W.Runtime->UpdateVisionSource(H,V);
   for(int Face=0;Face<6;++Face)
   {
    const FPatch P{TEXT("Oracle"),Pose.TransformPosition(Centers[Face]),Pose.TransformVector(Normals[Face]),FVector::ZeroVector,FVector::ZeroVector};
    const auto Q=W.Runtime->QuerySurfaceSample(W.Owner,Sample(ESightWeaveBoxFace(Face)));
    TestTrue(TEXT("Independent world receiver coordinates"),Q.WorldPoint.Equals(P.Center,1.e-6));
    TestEqual(TEXT("Real surface matches independent solid oracle"),Q.Hard.bVisible,GeometricSupport(Pose.TransformPosition(Eye),P,Solids));++Checks;
   }
  }
 }
 TestEqual(TEXT("All independent oracle cases executed"),Checks,90);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceV1Policy,"Darkwell.SightWeave.SurfaceObservation.RuntimeV1.Policy",SurfaceTestFlags)
bool FSurfaceV1Policy::RunTest(const FString&)
{
 FRuntimeWorld W;if(!TestNotNull(TEXT("Runtime"),W.Runtime))return false;
 W.Runtime->RegisterSurfaceBox(Receiver(W));auto V=W.Vision({-200,0,140},false);V.Shape=ESightWeaveSourceShape::DirectionalCone;V.HalfAngleDegrees=45;
 auto H=W.Runtime->RegisterVisionSource(V,nullptr);const auto FrontSample=Sample(ESightWeaveBoxFace::NegativeX);
 auto Q=[&](){return W.Runtime->QuerySurfaceSample(W.Owner,FrontSample).Hard;};
 TestFalse(TEXT("Surface vision requires legal light"),Q().bEligibleForMemoryWrite);
 FSightWeaveIlluminationSourceDescription L;L.KnowledgeOwnerId=W.Owner;L.FloorId=W.Floor;L.Transform=FTransform(FVector(-200,0,140));L.HeightRange={-100,500};L.Range=800;L.EmittedCapabilities={FName(TEXT("Other"))};
 auto Light=W.Runtime->RegisterIlluminationSource(L,nullptr);
 TestFalse(TEXT("Incompatible surface light"),Q().bVisible);
 L.EmittedCapabilities={FName(TEXT("Visible"))};W.Runtime->UpdateIlluminationSource(Light,L);
 TestTrue(TEXT("Same Runtime light attribution"),Q().bVisible && Q().ContributingIlluminationSources.Contains(Light));
 L.Transform.SetLocation({200,0,140});W.Runtime->UpdateIlluminationSource(Light,L);
 TestFalse(TEXT("Lamp behind self solid cannot illuminate front"),Q().bVisible);
 L.Transform.SetLocation({-200,200,140});W.Runtime->UpdateIlluminationSource(Light,L);
 auto Block=Receiver(W);Block.Id=TEXT("LightBlocker");Block.Pose=FTransform(FVector(-120,100,100));Block.HalfExtent={20,20,100};W.Runtime->RegisterSurfaceBox(Block);
 TestFalse(TEXT("Light-only external occlusion rejects observer-visible surface"),Q().bVisible);
 W.Runtime->UnregisterSurfaceBox(Block.Id);TestTrue(TEXT("Removing light blocker restores surface"),Q().bVisible);
 L.HeightRange={200,300};W.Runtime->UpdateIlluminationSource(Light,L);TestFalse(TEXT("Surface light height remains legal predicate"),Q().bVisible);
 L.HeightRange={-100,500};W.Runtime->UpdateIlluminationSource(Light,L);
 V.Transform.SetRotation(FRotator(0,180,0).Quaternion());W.Runtime->UpdateVisionSource(H,V);TestFalse(TEXT("Direction away refuses surface"),Q().bVisible);
 // Multiple sources may not borrow the bypass policy of a backfacing observer.
 auto Behind=W.Vision({200,0,140});auto BehindH=W.Runtime->RegisterVisionSource(Behind,nullptr);
 TestFalse(TEXT("No cross-source pose/policy borrowing"),Q().bVisible);W.Runtime->UnregisterVisionSource(BehindH);
 auto Body=W.Vision({-80,0,140});Body.Range=120;auto BodyH=W.Runtime->RegisterVisionSource(Body,nullptr);
 L.bActive=false;W.Runtime->UpdateIlluminationSource(Light,L);
 TestTrue(TEXT("Near body retains dark bypass for actual front"),Q().bUsedBypass && Q().bVisible);
 TestFalse(TEXT("Near body never grants unseen top"),W.Runtime->QuerySurfaceSample(W.Owner,Sample(ESightWeaveBoxFace::Top)).Hard.bVisible);
 FSightWeaveHardSuppressionDescription S;S.FloorId=W.Floor;S.Center={-40,0};S.Radius=20;S.HeightRange={-100,500};
 auto Supp=W.Runtime->RegisterHardLiveSuppression(S,nullptr);TestTrue(TEXT("Surface suppression wins over bypass"),Q().bRejectedBySuppression && !Q().bEligibleForMemoryWrite);
 W.Runtime->UnregisterHardLiveSuppression(Supp);W.Runtime->UnregisterVisionSource(BodyH);
 TestFalse(TEXT("Removing only legal source removes surface live"),Q().bVisible);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceV1Cache,"Darkwell.SightWeave.SurfaceObservation.RuntimeV1.BatchLifecycle",SurfaceTestFlags)
bool FSurfaceV1Cache::RunTest(const FString&)
{
 FRuntimeWorld W;if(!TestNotNull(TEXT("Runtime"),W.Runtime))return false;
 auto Box=Receiver(W);W.Runtime->RegisterSurfaceBox(Box);auto V=W.Vision({-200,0,140});auto H=W.Runtime->RegisterVisionSource(V,nullptr);
 TArray<FSightWeaveSurfaceSample> Samples{Sample(ESightWeaveBoxFace::NegativeX),Sample(ESightWeaveBoxFace::Top)};
 TArray<FSightWeaveSurfaceResult> Results;FSightWeaveSurfaceQueryCache Cache;FSightWeaveSurfaceQueryStats Stats;
 W.Runtime->QuerySurfaceSamples(W.Owner,Samples,Results,&Cache,&Stats);
 TestTrue(TEXT("Same box has distinct surface results"),Results[0].Hard.bVisible && !Results[1].Hard.bVisible);
 const auto OldFrame=Cache.Frame;const uint64 OldRevision=Results[0].ReceiverRevision;
 W.Runtime->QuerySurfaceSamples(W.Owner,Samples,Results,&Cache,&Stats);TestEqual(TEXT("Exact repeated batch cached"),Stats.CacheHits,uint64(2));
 V.Transform.SetLocation({-200,0,260});W.Runtime->UpdateVisionSource(H,V);W.Runtime->QuerySurfaceSamples(W.Owner,Samples,Results,&Cache,&Stats);
 TestTrue(TEXT("Eye update invalidates result cache"),Results[1].Hard.bVisible && Cache.Frame!=OldFrame);
 TestTrue(TEXT("Old scene immutable"),OldFrame->SurfaceScene->Find(Box.Id)->Box.HalfExtent.Z==100);
 Box.HalfExtent.Z=150;W.Runtime->UpdateSurfaceBox(Box);W.Runtime->QuerySurfaceSamples(W.Owner,Samples,Results,&Cache,&Stats);
 TestTrue(TEXT("Geometry revision invalidates cached surface"),Results[0].ReceiverRevision!=OldRevision);
 TestTrue(TEXT("Old geometry remains unchanged"),OldFrame->SurfaceScene->Find(Box.Id)->Box.HalfExtent.Z==100);
 TestFalse(TEXT("Duplicate identity rejected"),W.Runtime->RegisterSurfaceBox(Box));
 auto Invalid=Box;Invalid.Pose.SetScale3D({2,1,1});TestFalse(TEXT("Unsupported scale rejected explicitly"),W.Runtime->UpdateSurfaceBox(Invalid));
 TestFalse(TEXT("Out of domain sample rejected"),W.Runtime->QuerySurfaceSample(W.Owner,Sample(ESightWeaveBoxFace::Top,{1.01,0})).Hard.bAuthoritative);
 W.Runtime->UnregisterSurfaceBox(Box.Id);W.Runtime->QuerySurfaceSamples(W.Owner,Samples,Results,&Cache,&Stats);TestEqual(TEXT("Removed receiver rejected"),Results[0].Hard.Status,ESightWeaveQueryStatus::InvalidHandle);
 W.Runtime->RegisterSurfaceBox(Box);TestTrue(TEXT("Re-registration has new lifetime revision"),W.Runtime->QuerySurfaceSample(W.Owner,Samples[0]).ReceiverRevision>OldRevision);
 auto* LifetimeOwner=NewObject<UDarkwellObserverComponent>(W.World);auto Owned=Box;Owned.Id=TEXT("Owned");W.Runtime->RegisterSurfaceBox(Owned,LifetimeOwner);
 W.Runtime->UnregisterAllForOwner(LifetimeOwner);TestEqual(TEXT("Explicit owner teardown removes surface"),W.Runtime->QuerySurfaceSample(W.Owner,{Owned.Id,ESightWeaveBoxFace::Top,{0,0}}).Hard.Status,ESightWeaveQueryStatus::InvalidHandle);
 W.Runtime->QuerySurfaceSamples(FSightWeaveKnowledgeOwnerId(FName(TEXT("Other"))),Samples,Results,&Cache,&Stats);TestFalse(TEXT("Owner isolation invalidates cache"),Results[0].Hard.bVisible);
 FSightWeaveIlluminationSourceDescription Lamp;Lamp.FloorId=W.Floor;Lamp.KnowledgeOwnerId=W.Owner;Lamp.Transform=FTransform(FVector(-200,0,140));
 auto LampH=W.Runtime->RegisterIlluminationSource(Lamp,nullptr);
 const auto Revision=W.Runtime->AcquirePublishedSnapshot()->Revision.GetValue();
 const FSightWeaveVisionSourceHandle Eyes[]={H};const FSightWeaveIlluminationSourceHandle Lamps[]={LampH};
 TestTrue(TEXT("Atomic separate observer/lamp update"),W.Runtime->UpdateSourceGroupPoses(Eyes,Lamps,FTransform(FVector(-200,0,300)),FTransform(FVector(-200,0,120))));
 TestEqual(TEXT("Exactly one revision for both poses"),W.Runtime->AcquirePublishedSnapshot()->Revision.GetValue(),Revision+1);
 const auto Stable=W.Runtime->AcquirePublishedSnapshot();const FSightWeaveIlluminationSourceHandle Bad[]={FSightWeaveIlluminationSourceHandle(999999)};
 TestFalse(TEXT("Invalid handle rejects entire pose transaction"),W.Runtime->UpdateSourceGroupPoses(Eyes,Bad,FTransform(FVector(0,0,100)),FTransform::Identity));
 TestTrue(TEXT("Rejected transaction publishes nothing"),Stable==W.Runtime->AcquirePublishedSnapshot());
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceV1Observer,"Darkwell.SightWeave.SurfaceObservation.RuntimeV1.ObserverPose",SurfaceTestFlags)
bool FSurfaceV1Observer::RunTest(const FString&)
{
 FRuntimeWorld W(true);if(!TestNotNull(TEXT("Runtime"),W.Runtime))return false;
 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Character=W.World->SpawnActor<ADarkwellCharacter>(FVector(0,0,88),FRotator::ZeroRotator,Params);
 if(!TestNotNull(TEXT("Character"),Character))return false;
 auto* Observer=Character->GetObserverComponent();TestEqual(TEXT("Standing eye from capsule feet"),Observer->GetObserverPose().GetLocation().Z,162.);
 Observer->SetObserverWorldDirection(FRotator(0,90,0));Character->SetActorRotation(FRotator(0,180,0));
 TestTrue(TEXT("Observer yaw independent of body"),Observer->GetObserverPose().GetRotation().GetForwardVector().Equals(FVector(0,1,0),1.e-6));
 Observer->SetObserverWorldPose(FTransform(FVector(100,200,260)));TestTrue(TEXT("Dynamic position exact"),Observer->GetObserverPose().GetLocation()==FVector(100,200,260));
 Observer->ClearObserverWorldPose();Observer->StandingHeightCm=95;TestEqual(TEXT("Dynamic stance configuration"),Observer->GetObserverPose().GetLocation().Z,95.);
 Character->AddActorWorldOffset({0,0,100});TestEqual(TEXT("Standing on elevated geometry follows world feet"),Observer->GetObserverPose().GetLocation().Z,195.);
 Observer->ClearObserverWorldDirection();TestTrue(TEXT("Default body driver restored"),Observer->GetObserverPose().GetRotation().Equals(Character->GetActorQuat()));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceV1Performance,"Darkwell.SightWeave.SurfaceObservation.RuntimeV1.BoundedPerformance",SurfaceTestFlags)
bool FSurfaceV1Performance::RunTest(const FString&)
{
 FRuntimeWorld W;if(!TestNotNull(TEXT("Runtime"),W.Runtime))return false;
 W.Runtime->RegisterSurfaceBox(Receiver(W));auto MovingVision=W.Vision({-200,0,140});const auto MovingHandle=W.Runtime->RegisterVisionSource(MovingVision,nullptr);
 // Off-ray geometry remains registered to measure acceleration, not removed.
 for(int I=0;I<256;++I){auto B=Receiver(W);B.Id=FName(*FString::Printf(TEXT("Far%d"),I));B.Pose.SetLocation({double(I%16)*100,2000.+double(I/16)*100,100});W.Runtime->RegisterSurfaceBox(B);}
 TArray<FSightWeaveSurfaceSample> Samples;for(int Y=0;Y<20;++Y)for(int X=0;X<20;++X)Samples.Add(Sample(ESightWeaveBoxFace::NegativeX,{-.9+X*.09,-.9+Y*.09}));
 TArray<FSightWeaveSurfaceResult> Results;FSightWeaveSurfaceQueryCache Cache;FSightWeaveSurfaceQueryStats Stats;
 const double Start=FPlatformTime::Seconds();W.Runtime->QuerySurfaceSamples(W.Owner,Samples,Results,&Cache,&Stats);const double Cold=(FPlatformTime::Seconds()-Start)*1.e6;
 TestEqual(TEXT("Exact batch count"),Stats.ExactSamples,uint64(400));
 TestTrue(TEXT("BVH avoids all-solid scan"),Stats.PrimitiveTests<400*8);
 for(const auto& R:Results)TestTrue(TEXT("Bounded workload still fully legal"),R.Hard.bVisible);
 TArray<double> Times;for(int I=0;I<100;++I){const double T=FPlatformTime::Seconds();W.Runtime->QuerySurfaceSamples(W.Owner,Samples,Results,&Cache,&Stats);Times.Add((FPlatformTime::Seconds()-T)*1.e6);}
 Times.Sort();TestEqual(TEXT("Warm batches do not add exact geometry work"),Stats.ExactSamples,uint64(400));
 TestEqual(TEXT("All warm requests served by exact cache"),Stats.CacheHits,uint64(40000));
 AddInfo(FString::Printf(TEXT("SURFACE_V1_PERF boxes=257 samples=400 cold_us=%.3f warm_p95_us=%.3f warm_p99_us=%.3f nodes=%llu primitives=%llu cache_hits=%llu"),Cold,Times[94],Times[98],Stats.NodeVisits,Stats.PrimitiveTests,Stats.CacheHits));
 TArray<double> DirtyTimes,PublishTimes;const auto Scene=W.Runtime->AcquirePublishedSnapshot()->SurfaceScene;
 for(int I=0;I<100;++I)
 {
  MovingVision.Transform.SetLocation({-200,0,140.+I*.1});const double P=FPlatformTime::Seconds();W.Runtime->UpdateVisionSource(MovingHandle,MovingVision);
  PublishTimes.Add((FPlatformTime::Seconds()-P)*1.e6);const double T=FPlatformTime::Seconds();
  W.Runtime->QuerySurfaceSamples(W.Owner,Samples,Results,&Cache,&Stats);DirtyTimes.Add((FPlatformTime::Seconds()-T)*1.e6);
 }
 DirtyTimes.Sort();PublishTimes.Sort();
 TestTrue(TEXT("Moving observer never rebuilds surface BVH"),Scene==W.Runtime->AcquirePublishedSnapshot()->SurfaceScene);
 AddInfo(FString::Printf(TEXT("SURFACE_V1_DIRTY samples=400 query_p95_us=%.3f query_p99_us=%.3f source_publish_p95_us=%.3f source_publish_p99_us=%.3f"),DirtyTimes[94],DirtyTimes[98],PublishTimes[94],PublishTimes[98]));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceKnowledgeV1Facts,"Darkwell.SightWeave.SurfaceKnowledge.FactsAndArchive",SurfaceTestFlags)
bool FSurfaceKnowledgeV1Facts::RunTest(const FString&)
{
 FRuntimeWorld W;auto B=Receiver(W);W.Runtime->RegisterSurfaceBox(B);
 auto V=W.Vision({-200,0,162});auto H=W.Runtime->RegisterVisionSource(V,nullptr);
 FDarkwellSurfaceKnowledge K;TestTrue(TEXT("Initialize durable domain"),K.Initialize(B,W.Owner));K.Observe(*W.Runtime);
 auto Known=[&](ESightWeaveBoxFace F){return K.IsKnown(F,{0,0});};
 TestTrue(TEXT("Front learned"),Known(ESightWeaveBoxFace::NegativeX));
 TestFalse(TEXT("Top not learned from Whole/front"),Known(ESightWeaveBoxFace::Top));
 TestFalse(TEXT("Back not learned"),Known(ESightWeaveBoxFace::PositiveX));
 TestEqual(TEXT("Unoccluded large front uses four authority corner queries"),K.ExactSamples,uint64(4));
 K.Observe(*W.Runtime);TestEqual(TEXT("Unchanged revision performs zero proof work"),K.Proofs,uint64(0));
 TArray<uint8> Archive;K.Save(Archive);FDarkwellSurfaceKnowledge Restored;Restored.Initialize(B,W.Owner);
 TestTrue(TEXT("Archive accepts same stable identity/domain"),Restored.Load(Archive));
 TestTrue(TEXT("Archived front known"),Restored.IsKnown(ESightWeaveBoxFace::NegativeX,{0,0}));
 TestFalse(TEXT("Live never persisted"),Restored.IsLive(ESightWeaveBoxFace::NegativeX,{0,0}));
 auto Other=B;Other.Id=TEXT("Other");Restored.Initialize(Other,W.Owner);TestFalse(TEXT("Wrong domain refused"),Restored.Load(Archive));
 Restored.Initialize(B,W.Owner,2);TestFalse(TEXT("Content version mismatch refused"),Restored.Load(Archive));
 Restored.Initialize(B,W.Owner);auto Truncated=Archive;Truncated.SetNum(Archive.Num()-1);TestFalse(TEXT("Truncation refused transactionally"),Restored.Load(Truncated));
 V.Transform.SetLocation({0,200,162});W.Runtime->UpdateVisionSource(H,V);K.Observe(*W.Runtime);
 TestTrue(TEXT("New side learned independently"),Known(ESightWeaveBoxFace::PositiveY));
 TestTrue(TEXT("Old front retained"),Known(ESightWeaveBoxFace::NegativeX));
 TestFalse(TEXT("Old front no longer live"),K.IsLive(ESightWeaveBoxFace::NegativeX,{0,0}));
 V.Transform.SetLocation({0,200,260});W.Runtime->UpdateVisionSource(H,V);K.Observe(*W.Runtime);
 TestTrue(TEXT("Raised observer learns top"),Known(ESightWeaveBoxFace::Top));
 TestFalse(TEXT("Same XY bottom independent"),Known(ESightWeaveBoxFace::Bottom));
 const FBox2D All({-100,-100},{100,100});K.SetBlock(All,true);K.Clear(All);K.Observe(*W.Runtime);
 TestFalse(TEXT("Clear plus block prevents surface rewrite"),Known(ESightWeaveBoxFace::Top));
 TestTrue(TEXT("Memory block does not block Live"),K.IsLive(ESightWeaveBoxFace::Top,{0,0}));
 K.SetBlock(All,false);K.Observe(*W.Runtime);TestTrue(TEXT("Fresh legal observation relearns"),Known(ESightWeaveBoxFace::Top));
 V.bActive=false;W.Runtime->UpdateVisionSource(H,V);K.Observe(*W.Runtime);K.Clear(All);K.Observe(*W.Runtime);
 TestFalse(TEXT("Clear without observation stays unknown"),Known(ESightWeaveBoxFace::Top));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceRegionProof,"Darkwell.SightWeave.SurfaceKnowledge.RegionProof",SurfaceTestFlags)
bool FSurfaceRegionProof::RunTest(const FString&)
{
 FRuntimeWorld W;W.Runtime->RegisterSurfaceBox(Receiver(W));auto V=W.Vision({-200,0,162});auto H=W.Runtime->RegisterVisionSource(V,nullptr);
 const FBox2D Whole({-1,-1},{1,1});bool Value=false;
 TestTrue(TEXT("Clear face region certified"),W.Runtime->TrySurfaceRegion(W.Owner,TEXT("Cabinet"),ESightWeaveBoxFace::NegativeX,Whole,Value)&&Value);
 auto Obstacle=Receiver(W);Obstacle.Id=TEXT("Thin");Obstacle.Pose=FTransform(FVector(-120,8,130));Obstacle.HalfExtent={1,.05,70};W.Runtime->RegisterSurfaceBox(Obstacle);
 TestFalse(TEXT("Thin blocker invalidates corner-only proof"),W.Runtime->TrySurfaceRegion(W.Owner,TEXT("Cabinet"),ESightWeaveBoxFace::NegativeX,Whole,Value));
 int Certified=0;
 for(int Y=0;Y<8;++Y)for(int X=0;X<8;++X)
 {
  FBox2D Region(FVector2D(X,Y)/4-FVector2D(1),FVector2D(X+1,Y+1)/4-FVector2D(1));
  if(!W.Runtime->TrySurfaceRegion(W.Owner,TEXT("Cabinet"),ESightWeaveBoxFace::NegativeX,Region,Value))continue;
  ++Certified;for(int B=0;B<=8;++B)for(int A=0;A<=8;++A)
   TestEqual(TEXT("Every certified region agrees with dense exact Hard oracle"),W.Runtime->QuerySurfaceSample(W.Owner,Sample(ESightWeaveBoxFace::NegativeX,Region.Min+Region.GetSize()*FVector2D(A,B)/8)).Hard.bEligibleForMemoryWrite,Value);
 }
 TestTrue(TEXT("Some unobstructed subregions certified"),Certified>0);
 W.Runtime->UnregisterSurfaceBox(Obstacle.Id);V.IlluminationPolicy=ESightWeaveIlluminationPolicy::RequiresLegalIllumination;W.Runtime->UpdateVisionSource(H,V);
 TestTrue(TEXT("No legal light certifies false"),W.Runtime->TrySurfaceRegion(W.Owner,TEXT("Cabinet"),ESightWeaveBoxFace::NegativeX,Whole,Value)&&!Value);
 W.Runtime->UpdateSurfaceBox(Receiver(W,75));V.IlluminationPolicy=ESightWeaveIlluminationPolicy::BypassLegalIllumination;W.Runtime->UpdateVisionSource(H,V);
 Obstacle.Pose=FTransform(FVector(-100,0,200));Obstacle.HalfExtent={35,150,200};W.Runtime->RegisterSurfaceBox(Obstacle);
 TestTrue(TEXT("Convex shadow covers receiver even across eye/target planes"),W.Runtime->TrySurfaceRegion(W.Owner,TEXT("Cabinet"),ESightWeaveBoxFace::Top,Whole,Value)&&!Value);
 for(int Y=0;Y<=20;++Y)for(int X=0;X<=20;++X)TestFalse(TEXT("Clipped convex shadow matches dense exact authority"),W.Runtime->QuerySurfaceSample(W.Owner,Sample(ESightWeaveBoxFace::Top,FVector2D(X,Y)/10-FVector2D(1))).Hard.bVisible);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceKnowledgeV1P4,"Darkwell.SightWeave.SurfaceKnowledge.ObjectStaticP4",SurfaceTestFlags)
bool FSurfaceKnowledgeV1P4::RunTest(const FString&)
{
 FRuntimeWorld W(true);W.Runtime->ConfigureExplorationMemory(W.Owner,W.Floor,ESightWeaveRenderPrecisionTier::Ultra);
 // Match the production P4 Adapter: the old full-screen composite must not
 // own the view alongside the project-owned surface presenter.
 W.World->GetSubsystem<USightWeaveRenderWorldSubsystem>()->SetPresentationSuppressed(true);
 auto* S=W.World->GetSubsystem<UDarkwellSurfaceKnowledgeSubsystem>();auto* Static=W.World->GetSubsystem<UDarkwellStaticEnvironmentSubsystem>();
 auto* Actor=W.World->SpawnActor<AActor>();auto* Mesh=NewObject<UStaticMeshComponent>(Actor);Actor->AddInstanceComponent(Mesh);Actor->SetRootComponent(Mesh);
 Mesh->SetMobility(EComponentMobility::Movable);Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Mesh->SetRelativeScale3D({.8,.6,2});Mesh->RegisterComponent();Actor->SetActorLocation({0,0,100});Actor->SetActorHiddenInGame(false);Actor->DispatchBeginPlay();
 TestTrue(TEXT("Static surface admission"),Static->RegisterImmutableSurfaceBox(Mesh,TEXT("Cabinet"),FLinearColor(.6f,.2f,.05f)));
 TestFalse(TEXT("Duplicate identity rejected"),S->RegisterFixedBox(Mesh,TEXT("Cabinet"),FLinearColor::White,false));
 auto V=W.Vision({-200,0,162});auto H=W.Runtime->RegisterVisionSource(V,nullptr);S->Tick(0);
 const auto* K=S->Find(TEXT("Cabinet"));if(!TestNotNull(TEXT("Static CPU surface domain"),K))return false;
 TestTrue(TEXT("Static front known"),K->IsKnown(ESightWeaveBoxFace::NegativeX,{0,0}));TestFalse(TEXT("Static top unknown"),K->IsKnown(ESightWeaveBoxFace::Top,{0,0}));
 UTextureRenderTarget2D* Target=nullptr;USceneCaptureComponent2D* Capture=nullptr;
 if(!GUsingNullRHI)
 {
  auto* CaptureActor=W.World->SpawnActor<AActor>();Capture=NewObject<USceneCaptureComponent2D>(CaptureActor);CaptureActor->AddInstanceComponent(Capture);CaptureActor->SetRootComponent(Capture);CaptureActor->SetActorHiddenInGame(false);Capture->RegisterComponent();
  Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;Capture->ProjectionType=ECameraProjectionMode::Perspective;Capture->FOVAngle=60;
  Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;Capture->ShowFlags.SetTemporalAA(false);Capture->ShowFlags.SetMotionBlur(false);
  auto* Light=NewObject<UPointLightComponent>(CaptureActor);CaptureActor->AddInstanceComponent(Light);Light->SetIntensity(1000000);Light->SetAttenuationRadius(1000);Light->RegisterComponent();Light->SetWorldLocation({-200,0,300});
  Target=NewObject<UTextureRenderTarget2D>(CaptureActor);Target->InitCustomFormat(64,64,PF_B8G8R8A8,false);Target->UpdateResourceImmediate();Capture->TextureTarget=Target;
 }
 int CaptureNumber=0;FString ReportPath;FParse::Value(FCommandLine::Get(),TEXT("ReportExportPath="),ReportPath);
 const FString CaptureRoot=ReportPath.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("SurfaceKnowledgeCaptures"):FPaths::GetPath(ReportPath)/TEXT("Captures");IFileManager::Get().MakeDirectory(*CaptureRoot,true);
 auto Pixel=[&](bool Top)
 {
  if(!Capture)return FColor::Black;
  Capture->SetWorldLocation(Top?FVector(0,0,500):FVector(-400,0,100));Capture->SetWorldRotation(Top?FRotator(-90,0,0):FRotator::ZeroRotator);
  FAssetCompilingManager::Get().FinishAllCompilation();if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
  W.World->UpdateWorldComponents(false,false);W.World->SendAllEndOfFrameUpdates();Capture->CaptureScene();FlushRenderingCommands();
  if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
  W.World->SendAllEndOfFrameUpdates();Capture->CaptureScene();FlushRenderingCommands();
  TArray<FColor> Pixels;Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
  TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(64,64,MakeArrayView(Pixels),PNG);FFileHelper::SaveArrayToFile(PNG,*(CaptureRoot/FString::Printf(TEXT("SurfaceP4_%d.png"),CaptureNumber++)));
  const auto C=Pixels.IsValidIndex(32*64+32)?Pixels[32*64+32]:FColor::Magenta;AddInfo(FString::Printf(TEXT("Surface P4 camera top=%d center=%s"),Top,*C.ToString()));return C;
 };
 auto Mirror=[&]()
 {
  if(GUsingNullRHI)return;
  auto* Atlas=S->GetAtlas(TEXT("Cabinet"));FlushRenderingCommands();const auto Texture=Atlas->GetResource()->TextureRHI;const auto Size=K->AtlasSize;TArray<FColor> GPU,CPU;
  ENQUEUE_RENDER_COMMAND(SurfaceKnowledgeMirror)([&GPU,Texture,Size](FRHICommandListImmediate& Cmd){Cmd.ReadSurfaceData(Texture,FIntRect(0,0,Size.X,Size.Y),GPU,FReadSurfaceDataFlags(RCM_UNorm));});FlushRenderingCommands();K->Pixels(CPU);
  TestTrue(TEXT("Every GPU texel exactly mirrors CPU Live/Known/Suppressed"),GPU==CPU);
 };
 if(Capture)
 {
  auto* Original=Mesh->GetMaterial(0);auto* Control=NewObject<UMaterial>();Control->SetShadingModel(MSM_Unlit);
  auto* E=NewObject<UMaterialExpressionConstant3Vector>(Control);E->Constant=FLinearColor(.5,0,0);Control->GetExpressionCollection().AddExpression(E);Control->GetEditorOnlyData()->EmissiveColor.Expression=E;Control->PostEditChange();Mesh->SetMaterial(0,Control);
  TestTrue(TEXT("Positive control: capture renders actual mesh"),Pixel(false).R>20);Mesh->SetMaterial(0,Original);
  AddInfo(FString::Printf(TEXT("Surface mesh visible=%d hidden=%d renderstate=%d position=%s"),Mesh->IsVisible(),Actor->IsHidden(),Mesh->IsRenderStateCreated(),*Mesh->GetComponentLocation().ToString()));
 }
 Mirror();if(Capture){TestTrue(TEXT("Real front mesh shows Live tint"),Pixel(false).R>40);TestTrue(TEXT("Top-view camera cannot reveal unknown top"),Pixel(true).R<5);}
 V.bActive=false;W.Runtime->UpdateVisionSource(H,V);S->Tick(0);Mirror();
 if(Capture){const auto C=Pixel(false);TestTrue(TEXT("Observed front renders gray after losing Live"),C.R>15 && FMath::Abs(int(C.R)-int(C.G))<4);TestTrue(TEXT("Unknown top stays black in memory"),Pixel(true).R<5);}
 Static->SetMemoryWriteBlock(FBox2D({-100,-100},{100,100}),true);S->Tick(0);Mirror();if(Capture)TestTrue(TEXT("Block hides stored gray without erasing fact"),Pixel(false).R<5);
 TestTrue(TEXT("Block retained CPU fact"),K->IsKnown(ESightWeaveBoxFace::NegativeX,{0,0}));
 Static->ClearMemory(FBox2D({-100,-100},{100,100}));TestFalse(TEXT("Static Clear reaches Surface"),K->IsKnown(ESightWeaveBoxFace::NegativeX,{0,0}));
 Static->SetMemoryWriteBlock(FBox2D({-100,-100},{100,100}),false);V.bActive=true;V.Transform.SetLocation({-200,0,260});W.Runtime->UpdateVisionSource(H,V);S->Tick(0);
 TestTrue(TEXT("Static elevated observer learns top"),K->IsKnown(ESightWeaveBoxFace::Top,{0,0}));V.bActive=false;W.Runtime->UpdateVisionSource(H,V);S->Tick(0);Mirror();
 TestTrue(TEXT("Static consumer API addresses the explicit surface"),Static->HasSurfaceKnowledge(TEXT("Cabinet"),ESightWeaveBoxFace::Top,{0,0}));
 if(Capture)TestTrue(TEXT("New known top now renders gray"),Pixel(true).R>15);
 const auto InitialBytes=S->UploadBytes;S->Tick(0);TestEqual(TEXT("Stable revision zero uploads"),S->UploadBytes,uint64(0));TestEqual(TEXT("Stable revision zero exact queries"),S->ExactSamples,uint64(0));
 AddInfo(FString::Printf(TEXT("SURFACE_KNOWLEDGE_P4 atlas=%dx%d changed_upload_bytes=%llu stable_update_us=%.3f"),K->AtlasSize.X,K->AtlasSize.Y,InitialBytes,S->UpdateUs));
 W.Runtime->DisableExplorationMemory();S->Tick(0);TestNull(TEXT("Inactive scope cannot expose stored Surface facts"),S->Find(TEXT("Cabinet")));if(Capture)TestTrue(TEXT("Inactive scope presentation fails closed"),Pixel(true).R<5);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceKnowledgeObject,"Darkwell.SightWeave.SurfaceKnowledge.ObjectWholeAndModifiers",SurfaceTestFlags)
bool FSurfaceKnowledgeObject::RunTest(const FString&)
{
 FRuntimeWorld W(true);W.Runtime->ConfigureExplorationMemory(W.Owner,W.Floor,ESightWeaveRenderPrecisionTier::Ultra);
 auto* S=W.World->GetSubsystem<UDarkwellSurfaceKnowledgeSubsystem>();auto* Scene=W.World->SpawnActor<ADarkwellObjectMemoryScene>();
 auto* A=W.World->SpawnActor<AActor>();auto* M=NewObject<UStaticMeshComponent>(A);A->AddInstanceComponent(M);A->SetRootComponent(M);M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));M->SetRelativeScale3D({.8,.6,2});M->RegisterComponent();A->SetActorLocation({0,0,100});
 auto* Original=M->GetMaterial(0);auto* Memory=NewObject<UDarkwellRememberablePropComponent>(A);A->AddInstanceComponent(Memory);Memory->bUseSpatialMemory=true;Memory->bUseFixedSurfaceKnowledge=true;Memory->bRememberFromStart=false;Memory->ConfigureStableId(TEXT("Object"));Memory->AddMemoryPrimitive(M);Memory->RegisterComponent();
 auto* Policy=NewObject<USightWeaveObjectPolicyComponent>(A);A->AddInstanceComponent(Policy);Policy->bOverrideRevealMode=true;Policy->RevealMode=ESightWeaveRevealMode::WholeObjectAfterSpan;Policy->bOverrideMinimumObservedSpan=true;Policy->MinimumObservedSpanCm=40;Policy->bOverrideHistoryMode=true;Policy->HistoryMode=ESightWeaveHistoryMode::StationaryOnly;Policy->RegisterComponent();
 TestTrue(TEXT("Real ObjectMemory registration routes explicit surface"),Scene->RegisterRememberable(Memory,Policy));
 auto V=W.Vision({-200,0,162});auto H=W.Runtime->RegisterVisionSource(V,nullptr);S->Tick(0);
 TestTrue(TEXT("Whole recognition from legal front span"),S->IsWholeRecognized(TEXT("Object")));
 TestTrue(TEXT("Object consumer exposes Whole separately"),Scene->IsSurfaceObjectRecognized(TEXT("Object")));TestTrue(TEXT("Object consumer addresses front surface"),Scene->HasSurfaceKnowledge(TEXT("Object"),ESightWeaveBoxFace::NegativeX,{0,0}));
 TestFalse(TEXT("Whole never fills top"),S->Find(TEXT("Object"))->IsKnown(ESightWeaveBoxFace::Top,{0,0}));
 TArray<uint8> Saved;TestTrue(TEXT("Consumer archive exported"),S->SaveDomain(TEXT("Object"),Saved));
 V.bActive=false;W.Runtime->UpdateVisionSource(H,V);S->Tick(0);
 FSightWeaveMemoryScopeKey Scope;W.Runtime->GetExplorationMemoryScope(Scope);FSightWeaveMemoryModifierDescription Modifier;Modifier.Region.Scope=Scope;Modifier.Region.HeightRange={-100,500};Modifier.Region.Radius=300;Modifier.Operation=ESightWeaveMemoryModifierOperation::SuppressMemoryPresentation;
 const auto Mod=W.Runtime->RegisterMemoryModifier(Modifier);TestTrue(TEXT("Real Runtime suppression modifier"),Mod.IsValid());S->Tick(0);
 auto* K=S->Find(TEXT("Object"));TestTrue(TEXT("Presentation suppression retains surface fact"),K->IsKnown(ESightWeaveBoxFace::NegativeX,{0,0}));
 TestTrue(TEXT("Published modifier suppresses front"),K->Faces[0].Hidden[K->Faces[0].Known.Num()/2]);
 W.Runtime->UnregisterMemoryModifier(Mod);S->Tick(0);TestFalse(TEXT("Modifier-only revision removes presentation suppression"),K->Faces[0].Hidden[K->Faces[0].Known.Num()/2]);
 const FBox2D All({-100,-100},{100,100});Scene->ClearMemoryInRegion(All);TestFalse(TEXT("Clear resets Object recognition"),S->IsWholeRecognized(TEXT("Object")));TestFalse(TEXT("Clear reaches Surface facts"),K->IsKnown(ESightWeaveBoxFace::NegativeX,{0,0}));
 TestTrue(TEXT("Restore facts/recognition independently"),S->RestoreDomain(TEXT("Object"),Saved));TestTrue(TEXT("Whole restored"),S->IsWholeRecognized(TEXT("Object")));TestFalse(TEXT("Restore still does not fill top"),K->IsKnown(ESightWeaveBoxFace::Top,{0,0}));
 Scene->SetMemoryWriteBlock(All,true);Scene->ClearMemoryInRegion(All);V.bActive=true;W.Runtime->UpdateVisionSource(H,V);S->Tick(0);TestFalse(TEXT("Object block stops re-observation"),K->IsKnown(ESightWeaveBoxFace::NegativeX,{0,0}));
 Scene->SetMemoryWriteBlock(All,false);S->Tick(0);TestTrue(TEXT("Unblocked legal front relearned"),K->IsKnown(ESightWeaveBoxFace::NegativeX,{0,0}));
 Scene->ResetMemory();TestNull(TEXT("Reset releases surface domain"),S->Find(TEXT("Object")));TestTrue(TEXT("Reset restores source material ownership"),M->GetMaterial(0)==Original);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceKnowledgePerf,"Darkwell.SightWeave.SurfaceKnowledge.BoundedPerformance",SurfaceTestFlags)
bool FSurfaceKnowledgePerf::RunTest(const FString&)
{
 FRuntimeWorld W;auto B=Receiver(W);W.Runtime->RegisterSurfaceBox(B);auto V=W.Vision({-200,0,162});auto H=W.Runtime->RegisterVisionSource(V,nullptr);
 FDarkwellSurfaceKnowledge K;K.Initialize(B,W.Owner);TArray<double> Times;uint64 Queries=0,Unresolved=0;
 for(int I=0;I<100;++I){V.Transform.SetLocation({-200,double(I%20),162.+I*.01});W.Runtime->UpdateVisionSource(H,V);const double T=FPlatformTime::Seconds();K.Observe(*W.Runtime);Times.Add((FPlatformTime::Seconds()-T)*1e6);Queries+=K.ExactSamples;Unresolved+=K.UnresolvedCells;}
 Times.Sort();AddInfo(FString::Printf(TEXT("SURFACE_KNOWLEDGE_DIRTY p95_us=%.3f p99_us=%.3f authority_corner_queries=%llu unresolved=%llu cells=%d"),Times[94],Times[98],Queries,Unresolved,K.Faces[0].Known.Num()*2+K.Faces[2].Known.Num()*2+K.Faces[4].Known.Num()*2));
 TestEqual(TEXT("Clear moving-view front needs four queries per revision"),Queries,uint64(400));TestEqual(TEXT("Clear front completely certified"),Unresolved,uint64(0));
 auto Block=B;Block.Id=TEXT("Opaque");Block.Pose=FTransform(FVector(-100,0,100));Block.HalfExtent={10,100,150};W.Runtime->RegisterSurfaceBox(Block);
 const double Start=FPlatformTime::Seconds();K.Observe(*W.Runtime);const double Us=(FPlatformTime::Seconds()-Start)*1e6;
 TestFalse(TEXT("Opaque solid removes Live front"),K.IsLive(ESightWeaveBoxFace::NegativeX,{0,0}));TestTrue(TEXT("Opaque solid preserves Known front"),K.IsKnown(ESightWeaveBoxFace::NegativeX,{0,0}));TestEqual(TEXT("Convex shadow denial avoids per-cell query explosion"),K.ExactSamples,uint64(0));
 AddInfo(FString::Printf(TEXT("SURFACE_KNOWLEDGE_OCCLUDED us=%.3f certified_regions=%llu queries=%llu"),Us,K.Proofs,K.ExactSamples));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceKnowledgeApartment,"Darkwell.SightWeave.SurfaceKnowledge.ApartmentScale",SurfaceTestFlags)
bool FSurfaceKnowledgeApartment::RunTest(const FString&)
{
 FRuntimeWorld W(true);W.World->GetSubsystem<USightWeaveRenderWorldSubsystem>()->SetPresentationSuppressed(true);
 W.Runtime->ConfigureExplorationMemory(W.Owner,W.Floor,ESightWeaveRenderPrecisionTier::Ultra);
 auto* Apartment=W.World->SpawnActor<ADarkwellApartmentLab>();Apartment->DispatchBeginPlay();
 TArray<FDarkwellVisionIntegrationSegment> Segments;Apartment->BuildSightWeaveOccluderSegments(Segments);TArray<FSightWeaveSegment2D> Walls;
 for(const auto& P:Segments){FSightWeaveSegment2D D;D.A=P.A;D.B=P.B;D.FloorId=W.Floor;D.HeightRange={P.ZMin,P.ZMax};Walls.Add(D);}W.Runtime->RegisterOccluder(Walls,true,true,nullptr);
 auto* S=W.World->GetSubsystem<UDarkwellSurfaceKnowledgeSubsystem>();auto V=W.Vision({-160,-260,162});V.Shape=ESightWeaveSourceShape::DirectionalCone;V.HalfAngleDegrees=45;auto H=W.Runtime->RegisterVisionSource(V,nullptr);
 S->Tick(0);AddInfo(TEXT("SURFACE_APARTMENT_COLD ")+S->GetTelemetry());
 TestNull(TEXT("Flat floors retain the explicit legacy compatibility path"),S->Find(TEXT("Apartment.Floor.0.0")));
 TestNotNull(TEXT("Apartment Whole furniture uses surface domain"),S->Find(TEXT("Apartment.CoffeeTable.Whole")));
 TestNotNull(TEXT("Rotated wardrobe uses surface domain"),S->Find(TEXT("Apartment.Wardrobe.Partial37")));
 TArray<double> Times;uint64 MaxQueries=0,MaxUpload=0;
 for(int I=0;I<20;++I)
 {V.Transform=FTransform(FRotator(0,I*4.,0),FVector(-160+I*2.,-260,162));W.Runtime->UpdateVisionSource(H,V);S->Tick(0);Times.Add(S->UpdateUs);MaxQueries=FMath::Max(MaxQueries,S->ExactSamples);MaxUpload=FMath::Max(MaxUpload,S->UploadBytes);}
 Times.Sort();AddInfo(FString::Printf(TEXT("SURFACE_APARTMENT_DIRTY p95_us=%.3f max_us=%.3f max_authority_queries=%llu max_upload_bytes=%llu"),Times[18],Times[19],MaxQueries,MaxUpload));
 S->Tick(0);TestEqual(TEXT("Stationary apartment has no Surface query work"),S->ExactSamples,uint64(0));TestEqual(TEXT("Stationary apartment has no Surface upload work"),S->UploadBytes,uint64(0));AddInfo(TEXT("SURFACE_APARTMENT_STABLE ")+S->GetTelemetry());
 Apartment->Destroy();return true;
}
#endif
