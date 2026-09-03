#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SightWeaveObjectPolicy.h"
#include "SightWeaveSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace SightWeave::RevealPolicyTests
{
 using Reveal = ESightWeaveRevealMode;
 using History = ESightWeaveHistoryMode;
 struct FWorld
 {
  UWorld* World;
  AActor* Actor;
  FWorld()
  {
   World=NewObject<UWorld>(GetTransientPackage(),MakeUniqueObjectName(GetTransientPackage(),UWorld::StaticClass(),TEXT("RevealPolicy")),RF_Transient);
   World->WorldType=EWorldType::Game;
   GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
   World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false));
   Actor=World->SpawnActor<AActor>();
  }
  ~FWorld() { World->DestroyWorld(true); GEngine->DestroyWorldContext(World); }
 };
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FSightWeaveRevealPolicyTest,"SightWeave.RevealPolicy",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FSightWeaveRevealPolicyTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
 for(const TCHAR* N : {TEXT("RevealModeDefaultResolution"),TEXT("PerFieldObjectOverrideResolution"),
  TEXT("LegacyPolicySourceMigration"),TEXT("PolicyIsolationAcrossObjects"),
  TEXT("PluginSettingsSerialization"),TEXT("BlueprintPublicSurfaceValidation")})
 { Names.Add(N); Commands.Add(N); }
}
bool FSightWeaveRevealPolicyTest::RunTest(const FString& Case)
{
 using namespace SightWeave::RevealPolicyTests;
 if(Case==TEXT("RevealModeDefaultResolution") || Case==TEXT("PerFieldObjectOverrideResolution"))
 {
  FResolvedSightWeaveObjectPolicy Native;
  TestTrue(TEXT("Native plugin compatibility defaults"),Native.RevealMode==Reveal::SpatialPartial && Native.HistoryMode==History::Always);
  for(Reveal R : {Reveal::WholeObjectAfterSpan,Reveal::SpatialPartial})
   for(History H : {History::Always,History::StationaryOnly,History::Never})
    for(int32 Bits=0;Bits<8;++Bits)
    {
     FResolvedSightWeaveObjectPolicy Defaults; Defaults.RevealMode=R; Defaults.MinimumObservedSpanCm=100; Defaults.HistoryMode=H;
     FSightWeaveObjectPolicyOverrides O;
     O.bOverrideRevealMode=(Bits&1)!=0; O.RevealMode=R==Reveal::SpatialPartial?Reveal::WholeObjectAfterSpan:Reveal::SpatialPartial;
     O.bOverrideMinimumObservedSpan=(Bits&2)!=0; O.MinimumObservedSpanCm=37.5;
     O.bOverrideHistoryMode=(Bits&4)!=0; O.HistoryMode=H==History::Never?History::Always:History::Never;
     const auto P=FResolvedSightWeaveObjectPolicy::Resolve(Defaults,O);
     TestTrue(TEXT("Reveal field independent"),P.RevealMode==(O.bOverrideRevealMode?O.RevealMode:R));
     TestTrue(TEXT("History field independent"),P.HistoryMode==(O.bOverrideHistoryMode?O.HistoryMode:H));
     TestEqual(TEXT("Span field independent"),P.MinimumObservedSpanCm,O.bOverrideMinimumObservedSpan?37.5f:100.f);
    }
  return true;
 }
 if(Case==TEXT("BlueprintPublicSurfaceValidation"))
 {
  const UClass* C=USightWeaveObjectPolicyComponent::StaticClass();
  for(const TCHAR* N : {TEXT("bOverrideRevealMode"),TEXT("RevealMode"),TEXT("bOverrideMinimumObservedSpan"),TEXT("MinimumObservedSpanCm"),TEXT("bOverrideHistoryMode"),TEXT("HistoryMode"),TEXT("PolicySource")})
  {
   const FProperty* P=FindFProperty<FProperty>(C,N);
   TestTrue(TEXT("Serializable Blueprint authoring field"),P && P->HasAllPropertyFlags(CPF_Edit|CPF_BlueprintVisible) && !P->HasAnyPropertyFlags(CPF_Transient));
  }
  for(const TCHAR* N : {TEXT("GetResolvedRevealMode"),TEXT("GetResolvedMinimumObservedSpanCm"),TEXT("GetResolvedHistoryMode"),TEXT("SetSightWeaveMoving")})
  {
   const UFunction* F=C->FindFunctionByName(N);
   TestTrue(TEXT("Public Blueprint query"),F && F->HasAnyFunctionFlags(FUNC_BlueprintCallable));
  }
  TestEqual(TEXT("Only two reveal modes plus generated sentinel"),StaticEnum<Reveal>()->NumEnums(),3);
  TestNull(TEXT("No global reveal runtime toggle"),C->FindFunctionByName(TEXT("SetGlobalRevealMode")));
  return true;
 }
 if(Case==TEXT("PluginSettingsSerialization"))
 {
  const FString Directory=FPaths::ProjectSavedDir()/TEXT("GrayObjectPolicy")/FGuid::NewGuid().ToString();
  IFileManager::Get().MakeDirectory(*Directory,true);
  const FString Path=FPaths::ConvertRelativePathToFull(Directory/TEXT("PluginSettings.ini"));
  TestTrue(TEXT("Write unique config fixture"),FFileHelper::SaveStringToFile(TEXT("[/Script/SightWeaveRuntime.SightWeaveSettings]\nDefaultRevealMode=WholeObjectAfterSpan\nDefaultMinimumObservedSpanCm=37.5\nDefaultHistoryMode=Never\n"),*Path));
  USightWeaveSettings* Settings=NewObject<USightWeaveSettings>();
  Settings->LoadConfig(USightWeaveSettings::StaticClass(),*Path);
  TestTrue(TEXT("Reveal config deserialization"),Settings->DefaultRevealMode==Reveal::WholeObjectAfterSpan);
  TestEqual(TEXT("Centimeter config deserialization"),Settings->DefaultMinimumObservedSpanCm,37.5f);
  TestTrue(TEXT("History config deserialization"),Settings->DefaultHistoryMode==History::Never);
  for(const TCHAR* N : {TEXT("DefaultRevealMode"),TEXT("DefaultMinimumObservedSpanCm"),TEXT("DefaultHistoryMode")})
  {
   const FProperty* P=FindFProperty<FProperty>(USightWeaveSettings::StaticClass(),N);
   TestTrue(TEXT("Config property retained"),P && P->HasAnyPropertyFlags(CPF_Config));
  }
  return true;
 }
 FWorld F;
 auto* A=NewObject<USightWeaveObjectPolicyComponent>(F.Actor);
 auto* B=NewObject<USightWeaveObjectPolicyComponent>(F.Actor);
 if(Case==TEXT("LegacyPolicySourceMigration"))
 {
  // Import the exact pre-Reveal property names and enum strings through Unreal's
  // property serializer. Newly added override flags remain their native false.
  for(auto Pair : {TPair<const TCHAR*,const TCHAR*>(TEXT("PolicySource"),TEXT("Override")),{TEXT("HistoryMode"),TEXT("Never")}})
  {
   const FProperty* P=FindFProperty<FProperty>(A->GetClass(),Pair.Key);
   TestTrue(TEXT("Legacy serialized property still imports"),P && P->ImportText_InContainer(Pair.Value,A,A,PPF_None)!=nullptr);
  }
  A->RegisterComponent(); B->RegisterComponent();
  TestTrue(TEXT("Legacy override keeps Never"),A->GetResolvedHistoryMode()==History::Never);
  TestEqual(TEXT("Legacy source does not capture reveal field"),A->GetResolvedRevealMode(),B->GetResolvedRevealMode());
  TestEqual(TEXT("Legacy source does not capture span field"),A->GetResolvedMinimumObservedSpanCm(),B->GetResolvedMinimumObservedSpanCm());
  TestFalse(TEXT("No flag reinterpretation on load"),A->bOverrideHistoryMode);
  return true;
 }
 A->bOverrideRevealMode=true; A->RevealMode=Reveal::SpatialPartial;
 A->bOverrideMinimumObservedSpan=true; A->MinimumObservedSpanCm=23;
 B->bOverrideRevealMode=true; B->RevealMode=Reveal::WholeObjectAfterSpan;
 B->bOverrideHistoryMode=true; B->HistoryMode=History::Never;
 A->RegisterComponent(); B->RegisterComponent();
 A->RevealMode=Reveal::WholeObjectAfterSpan; A->MinimumObservedSpanCm=900;
 A->SetSightWeaveMoving(true);
 TestTrue(TEXT("Resolved reveal is registration cached"),A->GetResolvedRevealMode()==Reveal::SpatialPartial);
 TestEqual(TEXT("Resolved span is registration cached"),A->GetResolvedMinimumObservedSpanCm(),23.f);
 TestTrue(TEXT("Neighbor retains independent reveal"),B->GetResolvedRevealMode()==Reveal::WholeObjectAfterSpan);
 TestTrue(TEXT("Neighbor retains independent capture"),B->GetResolvedHistoryMode()==History::Never);
 TestFalse(TEXT("Neighbor motion is independent"),B->IsSightWeaveMoving());
 return true;
}
#endif
