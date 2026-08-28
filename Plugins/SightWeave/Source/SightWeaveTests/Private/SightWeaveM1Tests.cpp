#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SightWeaveSettings.h"
#include "SightWeaveTypes.h"
#include "SightWeaveWorldSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include <limits>
#include <type_traits>

namespace SightWeave::Tests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FSightWeaveSubjectRevealSpecification MakeRevealSpecification()
	{
		FSightWeaveSubjectRevealSpecification Specification;
		Specification.KnowledgeOwnerId = FName(TEXT("TestKnowledgeOwner"));
		Specification.SubjectId = FName(TEXT("TestSubject"));
		Specification.Reason = FName(TEXT("M1Test"));
		Specification.DurationSeconds = 1.0f;
		return Specification;
	}

	class FTestWorld
	{
	public:
		explicit FTestWorld(const TCHAR* BaseName)
		{
			const FName WorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), FName(BaseName));
			World = NewObject<UWorld>(GetTransientPackage(), WorldName, RF_Transient);
			if (!World || !GEngine)
			{
				return;
			}

			World->WorldType = EWorldType::Game;
			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(World->WorldType);
			WorldContext.SetCurrentWorld(World);
			World->InitializeNewWorld(UWorld::InitializationValues()
				.InitializeScenes(false)
				.AllowAudioPlayback(false)
				.CreatePhysicsScene(false)
				.RequiresHitProxies(false)
				.CreateNavigation(false)
				.CreateAISystem(false)
				.ShouldSimulatePhysics(false)
				.SetTransactional(false));
		}

		~FTestWorld()
		{
			Reset();
		}

		FTestWorld(const FTestWorld&) = delete;
		FTestWorld& operator=(const FTestWorld&) = delete;

		UWorld* Get() const { return World; }

		USightWeaveWorldSubsystem* GetSubsystem() const
		{
			return World ? World->GetSubsystem<USightWeaveWorldSubsystem>() : nullptr;
		}

		void Reset()
		{
			if (World && GEngine)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(true);
			}
			World = nullptr;
		}

	private:
		UWorld* World = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveModuleLoadsTest,
	"SightWeave.M1.ModuleLoads",
	SightWeave::Tests::TestFlags)

bool FSightWeaveModuleLoadsTest::RunTest(const FString& Parameters)
{
	IModuleInterface* RuntimeModule = FModuleManager::Get().LoadModule(TEXT("SightWeaveRuntime"));
	TestNotNull(TEXT("SightWeaveRuntime loads"), RuntimeModule);
	TestTrue(TEXT("Runtime module remains loaded"), FModuleManager::Get().IsModuleLoaded(TEXT("SightWeaveRuntime")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveSettingsDefaultsTest,
	"SightWeave.M1.SettingsDefaults",
	SightWeave::Tests::TestFlags)

bool FSightWeaveSettingsDefaultsTest::RunTest(const FString& Parameters)
{
	const USightWeaveSettings* Settings = GetDefault<USightWeaveSettings>();
	TestNotNull(TEXT("Settings default object exists"), Settings);
	if (Settings)
	{
		TestTrue(TEXT("Default floor is valid"), Settings->DefaultFloorId.IsValid());
		TestTrue(TEXT("Default height range is valid"), Settings->DefaultHeightRange.IsValid());
		TestTrue(TEXT("Boundary epsilon is finite and positive"),
			FMath::IsFinite(Settings->BoundaryEpsilonCentimeters) && Settings->BoundaryEpsilonCentimeters > 0.0f);
		TestFalse(TEXT("Runtime debug is safely disabled by default"), Settings->bEnableRuntimeDebug);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveDefaultHandlesInvalidTest,
	"SightWeave.M1.DefaultHandlesInvalid",
	SightWeave::Tests::TestFlags)

bool FSightWeaveDefaultHandlesInvalidTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Default vision handle is invalid"), FSightWeaveVisionSourceHandle().IsValid());
	TestFalse(TEXT("Default illumination handle is invalid"), FSightWeaveIlluminationSourceHandle().IsValid());
	TestFalse(TEXT("Default reveal handle is invalid"), FSightWeaveSubjectRevealHandle().IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveHandleTypeSafetyTest,
	"SightWeave.M1.HandleTypeSafety",
	SightWeave::Tests::TestFlags)

bool FSightWeaveHandleTypeSafetyTest::RunTest(const FString& Parameters)
{
	constexpr bool bDistinctTypes = !std::is_same_v<FSightWeaveVisionSourceHandle, FSightWeaveIlluminationSourceHandle>
		&& !std::is_same_v<FSightWeaveVisionSourceHandle, FSightWeaveSubjectRevealHandle>
		&& !std::is_same_v<FSightWeaveIlluminationSourceHandle, FSightWeaveSubjectRevealHandle>;
	constexpr bool bNoCrossConversion = !std::is_convertible_v<FSightWeaveVisionSourceHandle, FSightWeaveIlluminationSourceHandle>
		&& !std::is_convertible_v<FSightWeaveVisionSourceHandle, FSightWeaveSubjectRevealHandle>
		&& !std::is_convertible_v<FSightWeaveIlluminationSourceHandle, FSightWeaveVisionSourceHandle>;
	TestTrue(TEXT("All handle categories are distinct C++ types"), bDistinctTypes);
	TestTrue(TEXT("Handle categories cannot be implicitly converted"), bNoCrossConversion);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveVisionRegistrationTest,
	"SightWeave.M1.VisionRegistration",
	SightWeave::Tests::TestFlags)

bool FSightWeaveVisionRegistrationTest::RunTest(const FString& Parameters)
{
	SightWeave::Tests::FTestWorld TestWorld(TEXT("SightWeaveVisionRegistration"));
	USightWeaveWorldSubsystem* Subsystem = TestWorld.GetSubsystem();
	TestNotNull(TEXT("World subsystem exists"), Subsystem);
	if (Subsystem)
	{
		const FSightWeaveVisionSourceHandle Handle = Subsystem->RegisterVisionSource(FSightWeaveVisionSourceDescription(), nullptr);
		TestTrue(TEXT("Registered vision handle is valid"), Handle.IsValid());
		TestTrue(TEXT("Subsystem recognizes vision handle"), Subsystem->IsVisionSourceHandleValid(Handle));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveIlluminationRegistrationTest,
	"SightWeave.M1.IlluminationRegistration",
	SightWeave::Tests::TestFlags)

bool FSightWeaveIlluminationRegistrationTest::RunTest(const FString& Parameters)
{
	SightWeave::Tests::FTestWorld TestWorld(TEXT("SightWeaveIlluminationRegistration"));
	USightWeaveWorldSubsystem* Subsystem = TestWorld.GetSubsystem();
	TestNotNull(TEXT("World subsystem exists"), Subsystem);
	if (Subsystem)
	{
		const FSightWeaveIlluminationSourceHandle Handle = Subsystem->RegisterIlluminationSource(FSightWeaveIlluminationSourceDescription(), nullptr);
		TestTrue(TEXT("Registered illumination handle is valid"), Handle.IsValid());
		TestTrue(TEXT("Subsystem recognizes illumination handle"), Subsystem->IsIlluminationSourceHandleValid(Handle));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveRevealRegistrationTest,
	"SightWeave.M1.SubjectRevealRegistration",
	SightWeave::Tests::TestFlags)

bool FSightWeaveRevealRegistrationTest::RunTest(const FString& Parameters)
{
	SightWeave::Tests::FTestWorld TestWorld(TEXT("SightWeaveRevealRegistration"));
	USightWeaveWorldSubsystem* Subsystem = TestWorld.GetSubsystem();
	TestNotNull(TEXT("World subsystem exists"), Subsystem);
	if (Subsystem)
	{
		const FSightWeaveSubjectRevealHandle Handle = Subsystem->ApplySubjectRevealOverride(
			SightWeave::Tests::MakeRevealSpecification(), nullptr);
		TestTrue(TEXT("Registered reveal handle is valid"), Handle.IsValid());
		TestTrue(TEXT("Subsystem recognizes reveal handle"), Subsystem->IsSubjectRevealHandleValid(Handle));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveDuplicateRegistrationTest,
	"SightWeave.M1.DuplicateRegistrationUnique",
	SightWeave::Tests::TestFlags)

bool FSightWeaveDuplicateRegistrationTest::RunTest(const FString& Parameters)
{
	SightWeave::Tests::FTestWorld TestWorld(TEXT("SightWeaveDuplicateRegistration"));
	USightWeaveWorldSubsystem* Subsystem = TestWorld.GetSubsystem();
	if (TestNotNull(TEXT("World subsystem exists"), Subsystem))
	{
		const FSightWeaveVisionSourceHandle First = Subsystem->RegisterVisionSource(FSightWeaveVisionSourceDescription(), nullptr);
		const FSightWeaveVisionSourceHandle Second = Subsystem->RegisterVisionSource(FSightWeaveVisionSourceDescription(), nullptr);
		TestTrue(TEXT("First handle is valid"), First.IsValid());
		TestTrue(TEXT("Second handle is valid"), Second.IsValid());
		TestNotEqual(TEXT("Repeated registration produces a unique handle"), First.GetValue(), Second.GetValue());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveStaleHandleTest,
	"SightWeave.M1.StaleHandleRejected",
	SightWeave::Tests::TestFlags)

bool FSightWeaveStaleHandleTest::RunTest(const FString& Parameters)
{
	SightWeave::Tests::FTestWorld TestWorld(TEXT("SightWeaveStaleHandle"));
	USightWeaveWorldSubsystem* Subsystem = TestWorld.GetSubsystem();
	if (TestNotNull(TEXT("World subsystem exists"), Subsystem))
	{
		const FSightWeaveVisionSourceHandle OldHandle = Subsystem->RegisterVisionSource(FSightWeaveVisionSourceDescription(), nullptr);
		TestTrue(TEXT("Initial removal succeeds"), Subsystem->UnregisterVisionSource(OldHandle));
		TestFalse(TEXT("Removed handle is invalid"), Subsystem->IsVisionSourceHandleValid(OldHandle));
		const FSightWeaveVisionSourceHandle NewHandle = Subsystem->RegisterVisionSource(FSightWeaveVisionSourceDescription(), nullptr);
		TestTrue(TEXT("New handle is valid"), Subsystem->IsVisionSourceHandleValid(NewHandle));
		TestNotEqual(TEXT("New registration never reuses stale identity"), OldHandle.GetValue(), NewHandle.GetValue());
		TestFalse(TEXT("Old identity does not hit new data"), Subsystem->IsVisionSourceHandleValid(OldHandle));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveDuplicateRemovalTest,
	"SightWeave.M1.DuplicateRemovalSafe",
	SightWeave::Tests::TestFlags)

bool FSightWeaveDuplicateRemovalTest::RunTest(const FString& Parameters)
{
	SightWeave::Tests::FTestWorld TestWorld(TEXT("SightWeaveDuplicateRemoval"));
	USightWeaveWorldSubsystem* Subsystem = TestWorld.GetSubsystem();
	if (TestNotNull(TEXT("World subsystem exists"), Subsystem))
	{
		const FSightWeaveIlluminationSourceHandle Handle = Subsystem->RegisterIlluminationSource(FSightWeaveIlluminationSourceDescription(), nullptr);
		TestTrue(TEXT("First removal succeeds"), Subsystem->UnregisterIlluminationSource(Handle));
		TestFalse(TEXT("Second removal safely fails"), Subsystem->UnregisterIlluminationSource(Handle));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveRevisionLifecycleTest,
	"SightWeave.M1.RevisionLifecycle",
	SightWeave::Tests::TestFlags)

bool FSightWeaveRevisionLifecycleTest::RunTest(const FString& Parameters)
{
	SightWeave::Tests::FTestWorld TestWorld(TEXT("SightWeaveRevisionLifecycle"));
	USightWeaveWorldSubsystem* Subsystem = TestWorld.GetSubsystem();
	if (TestNotNull(TEXT("World subsystem exists"), Subsystem))
	{
		const int64 InitialRevision = Subsystem->GetRevision().GetValue();
		FSightWeaveVisionSourceDescription Description;
		const FSightWeaveVisionSourceHandle Handle = Subsystem->RegisterVisionSource(Description, nullptr);
		const int64 RegisteredRevision = Subsystem->GetRevision().GetValue();
		Description.Transform.SetTranslation(FVector(100.0, 0.0, 0.0));
		TestTrue(TEXT("Update succeeds"), Subsystem->UpdateVisionSource(Handle, Description));
		const int64 UpdatedRevision = Subsystem->GetRevision().GetValue();
		TestTrue(TEXT("Removal succeeds"), Subsystem->UnregisterVisionSource(Handle));
		const int64 RemovedRevision = Subsystem->GetRevision().GetValue();
		TestTrue(TEXT("Registration advances revision"), RegisteredRevision > InitialRevision);
		TestTrue(TEXT("Update advances revision"), UpdatedRevision > RegisteredRevision);
		TestTrue(TEXT("Removal advances revision"), RemovedRevision > UpdatedRevision);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveWorldIsolationTest,
	"SightWeave.M1.WorldIsolation",
	SightWeave::Tests::TestFlags)

bool FSightWeaveWorldIsolationTest::RunTest(const FString& Parameters)
{
	SightWeave::Tests::FTestWorld FirstWorld(TEXT("SightWeaveWorldIsolationA"));
	SightWeave::Tests::FTestWorld SecondWorld(TEXT("SightWeaveWorldIsolationB"));
	USightWeaveWorldSubsystem* First = FirstWorld.GetSubsystem();
	USightWeaveWorldSubsystem* Second = SecondWorld.GetSubsystem();
	if (TestNotNull(TEXT("First subsystem exists"), First) && TestNotNull(TEXT("Second subsystem exists"), Second))
	{
		First->RegisterVisionSource(FSightWeaveVisionSourceDescription(), nullptr);
		TestEqual(TEXT("First world owns its registration"), First->GetVisionSourceCount(), 1);
		TestEqual(TEXT("Second world remains isolated"), Second->GetVisionSourceCount(), 0);
		TestTrue(TEXT("Subsystem instances are distinct"), First != Second);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveWorldRestartTest,
	"SightWeave.M1.WorldRestartClearsState",
	SightWeave::Tests::TestFlags)

bool FSightWeaveWorldRestartTest::RunTest(const FString& Parameters)
{
	{
		SightWeave::Tests::FTestWorld OldWorld(TEXT("SightWeaveRestartOld"));
		if (USightWeaveWorldSubsystem* OldSubsystem = OldWorld.GetSubsystem())
		{
			OldSubsystem->RegisterVisionSource(FSightWeaveVisionSourceDescription(), nullptr);
			TestEqual(TEXT("Old world has one registration"), OldSubsystem->GetVisionSourceCount(), 1);
		}
	}

	SightWeave::Tests::FTestWorld NewWorld(TEXT("SightWeaveRestartNew"));
	USightWeaveWorldSubsystem* NewSubsystem = NewWorld.GetSubsystem();
	if (TestNotNull(TEXT("New world subsystem exists"), NewSubsystem))
	{
		TestEqual(TEXT("Restarted world begins without old registration state"), NewSubsystem->GetVisionSourceCount(), 0);
		TestEqual(TEXT("Restarted world revision begins at zero"), NewSubsystem->GetRevision().GetValue(), int64(0));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveQueryNotReadyTest,
	"SightWeave.M1.QueryNotReady",
	SightWeave::Tests::TestFlags)

bool FSightWeaveQueryNotReadyTest::RunTest(const FString& Parameters)
{
	SightWeave::Tests::FTestWorld TestWorld(TEXT("SightWeaveQueryNotReady"));
	USightWeaveWorldSubsystem* Subsystem = TestWorld.GetSubsystem();
	if (TestNotNull(TEXT("World subsystem exists"), Subsystem))
	{
		const FSightWeaveVisibilityQueryResult Result = Subsystem->QueryVisibilityAtLocation(
			FSightWeaveFloorId(FName(TEXT("Default"))), FVector::ZeroVector);
		TestTrue(TEXT("Unimplemented authority reports NotReady"), Result.Status == ESightWeaveQueryStatus::NotReady);
		TestFalse(TEXT("NotReady never defaults to visible"), Result.bVisible);
		TestTrue(TEXT("NotReady result stays Unknown"), Result.KnowledgeState == ESightWeaveKnowledgeState::Unknown);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveInvalidQueryInputsTest,
	"SightWeave.M1.InvalidQueryInputs",
	SightWeave::Tests::TestFlags)

bool FSightWeaveInvalidQueryInputsTest::RunTest(const FString& Parameters)
{
	SightWeave::Tests::FTestWorld TestWorld(TEXT("SightWeaveInvalidQueryInputs"));
	USightWeaveWorldSubsystem* Subsystem = TestWorld.GetSubsystem();
	if (TestNotNull(TEXT("World subsystem exists"), Subsystem))
	{
		const FSightWeaveVisibilityQueryResult InvalidFloor = Subsystem->QueryVisibilityAtLocation(
			FSightWeaveFloorId(), FVector::ZeroVector);
		TestTrue(TEXT("Invalid floor is explicit"), InvalidFloor.Status == ESightWeaveQueryStatus::InvalidFloor);

		const FVector InvalidLocation(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f);
		const FSightWeaveVisibilityQueryResult InvalidInput = Subsystem->QueryVisibilityAtLocation(
			FSightWeaveFloorId(FName(TEXT("Default"))), InvalidLocation);
		TestTrue(TEXT("Invalid location is explicit"), InvalidInput.Status == ESightWeaveQueryStatus::InvalidInput);

		const FSightWeaveVisibilityQueryResult InvalidHandle = Subsystem->QueryVisionSourceAtLocation(
			FSightWeaveVisionSourceHandle(), FSightWeaveFloorId(FName(TEXT("Default"))), FVector::ZeroVector);
		TestTrue(TEXT("Invalid handle is explicit"), InvalidHandle.Status == ESightWeaveQueryStatus::InvalidHandle);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveLabMapLoadsTest,
	"SightWeave.M1.LabMapLoads",
	SightWeave::Tests::TestFlags)

bool FSightWeaveLabMapLoadsTest::RunTest(const FString& Parameters)
{
	UPackage* MapPackage = LoadPackage(nullptr, TEXT("/SightWeave/Maps/L_SightWeave_Lab"), LOAD_None);
	TestNotNull(TEXT("Lab map package loads"), MapPackage);
	if (MapPackage)
	{
		TestNotNull(TEXT("Lab map package contains a UWorld"), UWorld::FindWorldInPackage(MapPackage));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveDependencyIsolationTest,
	"SightWeave.M1.DependencyIsolation",
	SightWeave::Tests::TestFlags)

bool FSightWeaveDependencyIsolationTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SightWeave"));
	if (!TestTrue(TEXT("SightWeave plugin is discoverable"), Plugin.IsValid()))
	{
		return true;
	}

	TArray<FString> RuntimeFiles;
	const FString RuntimeSourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Source/SightWeaveRuntime"));
	IFileManager::Get().FindFilesRecursive(RuntimeFiles, *RuntimeSourceRoot, TEXT("*.*"), true, false);
	for (const FString& RuntimeFile : RuntimeFiles)
	{
		const FString Extension = FPaths::GetExtension(RuntimeFile);
		if (Extension.Equals(TEXT("h"), ESearchCase::IgnoreCase)
			|| Extension.Equals(TEXT("cpp"), ESearchCase::IgnoreCase)
			|| Extension.Equals(TEXT("cs"), ESearchCase::IgnoreCase))
		{
			FString Contents;
			if (FFileHelper::LoadFileToString(Contents, *RuntimeFile))
			{
				TestFalse(*FString::Printf(TEXT("Runtime source has no host-game reference: %s"), *RuntimeFile),
					Contents.Contains(TEXT("Darkwell"), ESearchCase::IgnoreCase));
			}
		}
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.ScanPathsSynchronous({ TEXT("/SightWeave") }, true);
	TArray<FName> Dependencies;
	AssetRegistry.GetDependencies(
		FName(TEXT("/SightWeave/Maps/L_SightWeave_Lab")),
		Dependencies,
		UE::AssetRegistry::EDependencyCategory::Package);
	for (const FName Dependency : Dependencies)
	{
		const FString PackageName = Dependency.ToString();
		TestFalse(*FString::Printf(TEXT("Lab map does not depend on host content: %s"), *PackageName),
			PackageName.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase));
		TestFalse(*FString::Printf(TEXT("Lab map does not depend on host runtime: %s"), *PackageName),
			PackageName.StartsWith(TEXT("/Script/Darkwell"), ESearchCase::IgnoreCase));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveCompatibilityProfileTest,
	"SightWeave.M1.CompatibilityProfilesRemainDistinct",
	SightWeave::Tests::TestFlags)

bool FSightWeaveCompatibilityProfileTest::RunTest(const FString& Parameters)
{
	FSightWeaveIlluminationCompatibilityProfile VisibleProfile;
	VisibleProfile.AcceptedCapabilities = { FName(TEXT("Visible")), FName(TEXT("Visible")) };
	VisibleProfile.Normalize();
	FSightWeaveIlluminationCompatibilityProfile InfraredProfile;
	InfraredProfile.AcceptedCapabilities = { FName(TEXT("Infrared")) };
	InfraredProfile.Normalize();
	TestEqual(TEXT("Normalization removes duplicates"), VisibleProfile.AcceptedCapabilities.Num(), 1);
	TestTrue(TEXT("Visible profile accepts visible"), VisibleProfile.Accepts(FName(TEXT("Visible"))));
	TestFalse(TEXT("Visible profile rejects infrared"), VisibleProfile.Accepts(FName(TEXT("Infrared"))));
	TestFalse(TEXT("Distinct complete accepted sets are not merged"), VisibleProfile.IsEquivalentTo(InfraredProfile));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveIlluminationPolicyTest,
	"SightWeave.M1.BypassAndGatedDescriptionsDistinct",
	SightWeave::Tests::TestFlags)

bool FSightWeaveIlluminationPolicyTest::RunTest(const FString& Parameters)
{
	FSightWeaveVisionSourceDescription BypassDescription;
	BypassDescription.IlluminationPolicy = ESightWeaveIlluminationPolicy::BypassLegalIllumination;
	FSightWeaveVisionSourceDescription GatedDescription;
	GatedDescription.IlluminationPolicy = ESightWeaveIlluminationPolicy::RequiresLegalIllumination;
	GatedDescription.Compatibility.AcceptedCapabilities = { FName(TEXT("Visible")) };
	TestTrue(TEXT("Bypass description is valid without a compatibility set"), BypassDescription.IsValid());
	TestTrue(TEXT("Gated description is valid with a complete accepted set"), GatedDescription.IsValid());
	TestTrue(TEXT("Policies remain explicitly distinguishable"),
		BypassDescription.IlluminationPolicy != GatedDescription.IlluminationPolicy);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveRevealRegistryIsolationTest,
	"SightWeave.M1.RevealRegistryIsolation",
	SightWeave::Tests::TestFlags)

bool FSightWeaveRevealRegistryIsolationTest::RunTest(const FString& Parameters)
{
	SightWeave::Tests::FTestWorld TestWorld(TEXT("SightWeaveRevealRegistryIsolation"));
	USightWeaveWorldSubsystem* Subsystem = TestWorld.GetSubsystem();
	if (TestNotNull(TEXT("World subsystem exists"), Subsystem))
	{
		const FSightWeaveSubjectRevealHandle RevealHandle = Subsystem->ApplySubjectRevealOverride(
			SightWeave::Tests::MakeRevealSpecification(), nullptr);
		TestTrue(TEXT("Reveal registration succeeds"), RevealHandle.IsValid());
		TestEqual(TEXT("Reveal registry contains the override"), Subsystem->GetSubjectRevealCount(), 1);
		TestEqual(TEXT("Reveal registration does not create a vision source"), Subsystem->GetVisionSourceCount(), 0);
		TestEqual(TEXT("Reveal registration does not create illumination"), Subsystem->GetIlluminationSourceCount(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveOwnershipCleanupTest,
	"SightWeave.M1.OwnershipCleanup",
	SightWeave::Tests::TestFlags)

bool FSightWeaveOwnershipCleanupTest::RunTest(const FString& Parameters)
{
	SightWeave::Tests::FTestWorld TestWorld(TEXT("SightWeaveOwnershipCleanup"));
	USightWeaveWorldSubsystem* Subsystem = TestWorld.GetSubsystem();
	if (TestNotNull(TEXT("World subsystem exists"), Subsystem))
	{
		UObject* Owner = NewObject<USceneComponent>(TestWorld.Get());
		Subsystem->RegisterVisionSource(FSightWeaveVisionSourceDescription(), Owner);
		Subsystem->RegisterIlluminationSource(FSightWeaveIlluminationSourceDescription(), Owner);
		Subsystem->ApplySubjectRevealOverride(SightWeave::Tests::MakeRevealSpecification(), Owner);
		const int64 BeforeCleanup = Subsystem->GetRevision().GetValue();
		TestEqual(TEXT("All three owner registrations are removed"), Subsystem->UnregisterAllForOwner(Owner), 3);
		TestEqual(TEXT("Vision registry is empty"), Subsystem->GetVisionSourceCount(), 0);
		TestEqual(TEXT("Illumination registry is empty"), Subsystem->GetIlluminationSourceCount(), 0);
		TestEqual(TEXT("Reveal registry is empty"), Subsystem->GetSubjectRevealCount(), 0);
		TestTrue(TEXT("Owner cleanup advances revision"), Subsystem->GetRevision().GetValue() > BeforeCleanup);
		TestEqual(TEXT("Repeated owner cleanup safely removes nothing"), Subsystem->UnregisterAllForOwner(Owner), 0);
	}
	return true;
}

#endif
