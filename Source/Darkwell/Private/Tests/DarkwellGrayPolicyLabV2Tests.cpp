#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/Level.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "VisionPresentation/DarkwellGrayPolicyLab.h"
#include "Widgets/SWidget.h"
#include "Layout/Children.h"

namespace Darkwell::GrayPolicyLabV2Tests
{
	struct FScopedMap
	{
		TStrongObjectPtr<UWorld> World;
		explicit FScopedMap(const TCHAR* Path)
		{
			UPackage* Package=LoadPackage(nullptr,Path,LOAD_None);
			World.Reset(Package?UWorld::FindWorldInPackage(Package):nullptr);
		}
		~FScopedMap()
		{
			// Raw package loading can initialize world subsystems. A later test's
			// GC must not become responsible for tearing down this map. Never clean
			// a user/editor world which already has an engine context.
			if(World.IsValid() && !GEngine->GetWorldContextFromWorld(World.Get()))
				World->DestroyWorld(false);
		}
	};

	template <typename T>
	int32 CountExactActors(const UWorld* World)
	{
		int32 Count = 0;
		if (!World || !World->PersistentLevel) return Count;
		for (const AActor* Actor : World->PersistentLevel->Actors)
		{
			if (Actor && Actor->GetClass() == T::StaticClass()) ++Count;
		}
		return Count;
	}

	bool GuidanceContains(const TCHAR* Text)
	{
		return GetDefault<ADarkwellSightWeaveGrayPolicyLabDirector>()
			->GetChineseGuidanceForTesting().Contains(Text);
	}
}

#define GRAY_LAB_SIMPLE_TEST(ClassName, TestName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestName, \
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

GRAY_LAB_SIMPLE_TEST(FGrayPolicyLabMapLoads, "Darkwell.GrayPolicyLabV2.MapLoads")
bool FGrayPolicyLabMapLoads::RunTest(const FString&)
{
	TestTrue(TEXT("Dedicated map package exists"), FPackageName::DoesPackageExist(Darkwell::GrayPolicyLab::MapPath));
	const Darkwell::GrayPolicyLabV2Tests::FScopedMap Map(Darkwell::GrayPolicyLab::MapPath);
	TestNotNull(TEXT("Dedicated map loads"), Map.World.Get());
	return true;
}

GRAY_LAB_SIMPLE_TEST(FGrayPolicyLabMapCheck, "Darkwell.GrayPolicyLabV2.MapCheck")
bool FGrayPolicyLabMapCheck::RunTest(const FString&)
{
	const Darkwell::GrayPolicyLabV2Tests::FScopedMap Map(Darkwell::GrayPolicyLab::MapPath);
	const UWorld* World=Map.World.Get();
	TestNotNull(TEXT("Map world"), World);
	TestEqual(TEXT("Exactly one native Director"),
		Darkwell::GrayPolicyLabV2Tests::CountExactActors<ADarkwellSightWeaveGrayPolicyLabDirector>(World), 1);
	return true;
}

GRAY_LAB_SIMPLE_TEST(FGrayPolicyLabPlayerStarts, "Darkwell.GrayPolicyLabV2.PlayerStarts")
bool FGrayPolicyLabPlayerStarts::RunTest(const FString&)
{
	const Darkwell::GrayPolicyLabV2Tests::FScopedMap Map(Darkwell::GrayPolicyLab::MapPath);
	const UWorld* World=Map.World.Get();
	TestEqual(TEXT("Exactly one PlayerStart"), Darkwell::GrayPolicyLabV2Tests::CountExactActors<APlayerStart>(World), 1);
	return true;
}

GRAY_LAB_SIMPLE_TEST(FGrayPolicyLabChinese, "Darkwell.GrayPolicyLabV2.ChineseGuidanceExists")
bool FGrayPolicyLabChinese::RunTest(const FString&)
{
	using namespace Darkwell::GrayPolicyLabV2Tests;
	for (const TCHAR* Title : {TEXT("整体显示"), TEXT("局部切块"), TEXT("移动物体"),
		TEXT("永不记忆"), TEXT("遮挡与快速扫视"), TEXT("性能压力测试")})
	{
		TestTrue(FString::Printf(TEXT("Chinese title exists: %s"), Title), GuidanceContains(Title));
	}
	TestTrue(TEXT("Bundled CJK fallback exists"), FPaths::FileExists(FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansFallback.ttf")));
	return true;
}

GRAY_LAB_SIMPLE_TEST(FGrayPolicyLabIsolation, "Darkwell.GrayPolicyLabV2.RoomIsolation")
bool FGrayPolicyLabIsolation::RunTest(const FString&)
{
	TSet<FVector> Centers;
	for (int32 Room = 0; Room <= 6; ++Room) Centers.Add(ADarkwellSightWeaveGrayPolicyLabDirector::GetRoomCenterForTesting(Room));
	TestEqual(TEXT("Lobby and six unique room centers"), Centers.Num(), 7);
	for (int32 A = 1; A <= 6; ++A) for (int32 B = A + 1; B <= 6; ++B)
	{
		TestTrue(TEXT("Rooms are farther apart than one view range"), FVector::Dist2D(
			ADarkwellSightWeaveGrayPolicyLabDirector::GetRoomCenterForTesting(A),
			ADarkwellSightWeaveGrayPolicyLabDirector::GetRoomCenterForTesting(B)) > 2200.0f);
	}
	return true;
}

GRAY_LAB_SIMPLE_TEST(FGrayPolicyLabLocalReset, "Darkwell.GrayPolicyLabV2.LocalReset")
bool FGrayPolicyLabLocalReset::RunTest(const FString&)
{
	for (int32 Room = 1; Room <= 5; ++Room)
	{
		TestTrue(TEXT("Each functional room owns target IDs"),
			!ADarkwellSightWeaveGrayPolicyLabDirector::GetStableIdsForRoomForTesting(Room).IsEmpty());
	}
	return true;
}

GRAY_LAB_SIMPLE_TEST(FGrayPolicyLabReturn, "Darkwell.GrayPolicyLabV2.ReturnToLobby")
bool FGrayPolicyLabReturn::RunTest(const FString&)
{
	TestEqual(TEXT("Lobby is room zero"), ADarkwellSightWeaveGrayPolicyLabDirector::GetRoomCenterForTesting(0), FVector::ZeroVector);
	TestEqual(TEXT("All expected native controls are declared"), ADarkwellSightWeaveGrayPolicyLabDirector::GetExpectedControlCountForTesting(), 27);
	return true;
}

GRAY_LAB_SIMPLE_TEST(FGrayPolicyLabWhole, "Darkwell.GrayPolicyLabV2.WholeObjectRoom")
bool FGrayPolicyLabWhole::RunTest(const FString&)
{
	TestTrue(TEXT("Whole room guidance states 100 cm threshold"), Darkwell::GrayPolicyLabV2Tests::GuidanceContains(TEXT("100 厘米")));
	TestEqual(TEXT("Whole room has one core object"), ADarkwellSightWeaveGrayPolicyLabDirector::GetStableIdsForRoomForTesting(1).Num(), 1);
	return true;
}

GRAY_LAB_SIMPLE_TEST(FGrayPolicyLabPartial, "Darkwell.GrayPolicyLabV2.SpatialPartialRoom")
bool FGrayPolicyLabPartial::RunTest(const FString&)
{
	TestTrue(TEXT("Partial guidance names cap"), Darkwell::GrayPolicyLabV2Tests::GuidanceContains(TEXT("深灰封口")));
	TestEqual(TEXT("Partial room has one core object"), ADarkwellSightWeaveGrayPolicyLabDirector::GetStableIdsForRoomForTesting(2).Num(), 1);
	return true;
}

GRAY_LAB_SIMPLE_TEST(FGrayPolicyLabMoving, "Darkwell.GrayPolicyLabV2.MovingRoom")
bool FGrayPolicyLabMoving::RunTest(const FString&)
{
	TestTrue(TEXT("Moving guidance requires fresh observation"), Darkwell::GrayPolicyLabV2Tests::GuidanceContains(TEXT("重新合法观察")));
	TestEqual(TEXT("Moving room has Whole and Partial subjects"), ADarkwellSightWeaveGrayPolicyLabDirector::GetStableIdsForRoomForTesting(3).Num(), 2);
	return true;
}

GRAY_LAB_SIMPLE_TEST(FGrayPolicyLabNever, "Darkwell.GrayPolicyLabV2.NeverRoom")
bool FGrayPolicyLabNever::RunTest(const FString&)
{
	TestTrue(TEXT("Never room is explicitly a negative control"), Darkwell::GrayPolicyLabV2Tests::GuidanceContains(TEXT("负对照")));
	return true;
}

GRAY_LAB_SIMPLE_TEST(FGrayPolicyLabOcclusion, "Darkwell.GrayPolicyLabV2.OcclusionRoom")
bool FGrayPolicyLabOcclusion::RunTest(const FString&)
{
	TestTrue(TEXT("Occlusion guidance forbids confirmation without legal contact"), Darkwell::GrayPolicyLabV2Tests::GuidanceContains(TEXT("完全被挡住的对象不能确认")));
	TestEqual(TEXT("Occlusion room has two policy subjects"), ADarkwellSightWeaveGrayPolicyLabDirector::GetStableIdsForRoomForTesting(5).Num(), 2);
	return true;
}

GRAY_LAB_SIMPLE_TEST(FGrayPolicyLabStressDormant, "Darkwell.GrayPolicyLabV2.StressDormantByDefault")
bool FGrayPolicyLabStressDormant::RunTest(const FString&)
{
	const auto* Director = GetDefault<ADarkwellSightWeaveGrayPolicyLabDirector>();
	TestEqual(TEXT("Stress mode defaults to off"), Director->GetStressMode(), 0);
	TestFalse(TEXT("Stress is dormant by default"), Director->IsStressActive());
	return true;
}

GRAY_LAB_SIMPLE_TEST(FGrayPolicyLabStressModes, "Darkwell.GrayPolicyLabV2.StressModes")
bool FGrayPolicyLabStressModes::RunTest(const FString&)
{
	for (const TCHAR* Mode : {TEXT("1 个对象"), TEXT("8 个对象"), TEXT("32 个对象"),
		TEXT("64 条同区域历史"), TEXT("64 条同 StableID 多姿态历史"), TEXT("184 条分布历史"), TEXT("六种策略混合")})
	{
		TestTrue(FString::Printf(TEXT("Stress guidance contains %s"), Mode),
			Darkwell::GrayPolicyLabV2Tests::GuidanceContains(Mode));
	}
	return true;
}

GRAY_LAB_SIMPLE_TEST(FGrayPolicyLabNoCrossRoom, "Darkwell.GrayPolicyLabV2.NoCrossRoomEvidence")
bool FGrayPolicyLabNoCrossRoom::RunTest(const FString&)
{
	TSet<FName> Ids;
	for (int32 Room = 1; Room <= 5; ++Room)
	{
		for (const FName Id : ADarkwellSightWeaveGrayPolicyLabDirector::GetStableIdsForRoomForTesting(Room))
		{
			TestFalse(TEXT("Stable IDs are unique across rooms"), Ids.Contains(Id));
			Ids.Add(Id);
		}
	}
	return true;
}

GRAY_LAB_SIMPLE_TEST(FGrayPolicyLabLegacyLoads, "Darkwell.GrayPolicyLabV2.LegacyMapStillLoads")
bool FGrayPolicyLabLegacyLoads::RunTest(const FString&)
{
	constexpr TCHAR Legacy[] = TEXT("/Game/Maps/L_ProjectFogPropGameplayLab");
	TestTrue(TEXT("Legacy map package remains"), FPackageName::DoesPackageExist(Legacy));
	const Darkwell::GrayPolicyLabV2Tests::FScopedMap Map(Legacy);
	TestNotNull(TEXT("Legacy map still loads"), Map.World.Get());
	return true;
}

#undef GRAY_LAB_SIMPLE_TEST

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellGrayLabelRelease,
 "Darkwell.PropLab.GrayPolicyLabV2.LabelSlateLifetime", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDarkwellGrayLabelRelease::RunTest(const FString&)
{
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
 auto* Label=NewObject<UDarkwellGrayPolicyWorldLabelWidget>(World);
 Label->Initialize();
 Label->SetLabel(FText::FromString(TEXT("Lifetime check")));
 TSharedPtr<SWidget> Root=Label->TakeWidget();
 TWeakPtr<SWidget> Text;
 TFunction<void(const TSharedRef<SWidget>&)> FindText=[&](const TSharedRef<SWidget>& Node)
 {
  if(Node->GetType()==TEXT("STextBlock")) Text=Node;
  auto* Children=Node->GetChildren();
  for(int32 Index=0;Children && Index<Children->Num();++Index) FindText(Children->GetChildAt(Index));
 };
 FindText(Root.ToSharedRef());
 TestTrue(TEXT("Concrete label text was built"),Text.IsValid());
 Root.Reset();
 Label->ReleaseSlateResources(true);
 TestFalse(TEXT("Released widget retains no concrete text layout"),Text.IsValid());
 // Rebuilding must still be supported while the UObject itself stays alive.
 Root=Label->TakeWidget();
 TestTrue(TEXT("Released label can rebuild"),Root.IsValid());
 Root.Reset(); Label->ReleaseSlateResources(true);
 World->DestroyWorld(false);
 return true;
}

#endif
