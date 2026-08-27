#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "SightWeaveComponents.h"
#include "SightWeaveLabSupport.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UObject/Package.h"

namespace SightWeave::M3P5::LabTests
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
	FSightWeaveM3P5TakeLabScreenshotCommand,
	FString,
	ScreenshotFilename);

bool FSightWeaveM3P5TakeLabScreenshotCommand::Update()
{
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenshotFilename), true);
	FScreenshotRequest::RequestScreenshot(
		ScreenshotFilename,
		/* bInShowUI = */ false,
		/* bAddUniqueSuffix = */ false,
		/* bHdrScreenshot = */ false,
		FIntRect(),
		/* bInRestrictToGameViewport = */ true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P5LabModeContractTest,
	"SightWeave.M3P5.Lab.ModeContract",
	SightWeave::M3P5::LabTests::TestFlags)

bool FSightWeaveM3P5LabModeContractTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("M3.5 mode string is stable"),
		FString(SightWeave::Lab::LexToString(ESightWeaveLabMode::M3P5)),
		FString(TEXT("M3P5")));
	TestEqual(TEXT("M3.5 overview camera label is stable"),
		FString(SightWeave::Lab::GetCameraLabel(
			ESightWeaveLabMode::M3P5, ESightWeaveLabCamera::Overview)),
		FString(TEXT("SW_M3P5_OverviewCamera")));
	TestEqual(TEXT("M3.5 remembered camera label is stable"),
		FString(SightWeave::Lab::GetCameraLabel(
			ESightWeaveLabMode::M3P5, ESightWeaveLabCamera::Closeup)),
		FString(TEXT("SW_M3P5_RememberedCamera")));
	TestEqual(TEXT("M3.5 dynamic-leak camera label is stable"),
		FString(SightWeave::Lab::GetCameraLabel(
			ESightWeaveLabMode::M3P5, ESightWeaveLabCamera::DynamicDoor)),
		FString(TEXT("SW_M3P5_DynamicLeakCamera")));
	TestEqual(TEXT("M3.5 page-boundary camera label is stable"),
		FString(SightWeave::Lab::GetCameraLabel(
			ESightWeaveLabMode::M3P5, ESightWeaveLabCamera::PageBoundary)),
		FString(TEXT("SW_M3P5_PageBoundaryCamera")));
	TestEqual(TEXT("M3.5 rotated camera label is stable"),
		FString(SightWeave::Lab::GetCameraLabel(
			ESightWeaveLabMode::M3P5, ESightWeaveLabCamera::Rotated45)),
		FString(TEXT("SW_M3P5_Rotated45Camera")));

	TestTrue(TEXT("M3.5 enables its own fixtures"),
		SightWeave::Lab::IsFixtureEnabled(TEXT("SW_M3P5_LiveNow"), ESightWeaveLabMode::M3P5));
	TestTrue(TEXT("M3.5 reuses the safe page-boundary fixture"),
		SightWeave::Lab::IsFixtureEnabled(TEXT("SW_M3P3_PageBoundaryVision"), ESightWeaveLabMode::M3P5));
	TestFalse(TEXT("M3.5 disables M3.4 fixtures"),
		SightWeave::Lab::IsFixtureEnabled(TEXT("SW_M3P4_OverviewBypass"), ESightWeaveLabMode::M3P5));
	TestFalse(TEXT("M3.4 disables M3.5 fixtures"),
		SightWeave::Lab::IsFixtureEnabled(TEXT("SW_M3P5_LiveNow"), ESightWeaveLabMode::M3P4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P5LabAuthoredFixturesTest,
	"SightWeave.M3P5.Lab.AuthoredFixtures",
	SightWeave::M3P5::LabTests::TestFlags)

bool FSightWeaveM3P5LabAuthoredFixturesTest::RunTest(const FString& Parameters)
{
	UPackage* Package = LoadPackage(nullptr, TEXT("/SightWeave/Maps/L_SightWeave_Lab"), LOAD_None);
	UWorld* World = Package ? UWorld::FindWorldInPackage(Package) : nullptr;
	if (!TestNotNull(TEXT("M3.5 Lab package contains a world"), World))
	{
		return true;
	}

	const TSet<FString> RequiredLabels = {
		TEXT("SW_M3P5_StaticGroundMemory"),
		TEXT("SW_M3P5_StraightWall"),
		TEXT("SW_M3P5_L_A"),
		TEXT("SW_M3P5_L_B"),
		TEXT("SW_M3P5_T_Top"),
		TEXT("SW_M3P5_T_Stem"),
		TEXT("SW_M3P5_DiagonalWall"),
		TEXT("SW_M3P5_Room_North"),
		TEXT("SW_M3P5_Room_South"),
		TEXT("SW_M3P5_Room_East"),
		TEXT("SW_M3P5_Room_West"),
		TEXT("SW_M3P5_RememberOnce"),
		TEXT("SW_M3P5_LiveNow"),
		TEXT("SW_M3P5_BlockProbe"),
		TEXT("SW_M3P5_BlockMemoryWrites"),
		TEXT("SW_M3P5_SuppressMemoryPresentation"),
		TEXT("SW_M3P5_NegativeTileRememberOnce"),
		TEXT("SW_M3P5_NegativeTileStaticMemory"),
		TEXT("SW_M3P5_PageBoundaryStaticMemory"),
		TEXT("SW_M3P5_DynamicDoor"),
		TEXT("SW_M3P5_MovingMeshLeakSentinel"),
		TEXT("SW_M3P5_CurrentLightLeakSentinel"),
		TEXT("SW_M3P5_EmissiveParticleLeakSentinel"),
		TEXT("SW_M3P5_OverviewCamera"),
		TEXT("SW_M3P5_Rotated45Camera"),
		TEXT("SW_M3P5_RememberedCamera"),
		TEXT("SW_M3P5_DynamicLeakCamera"),
		TEXT("SW_M3P5_PageBoundaryCamera")
	};

	TSet<FString> FoundLabels;
	int32 StaticEnvironmentCount = 0;
	int32 MemoryModifierCount = 0;
	int32 BlockModifierCount = 0;
	int32 SuppressModifierCount = 0;
	int32 M3P5VisionSourceCount = 0;
	bool bAllStaticEnvironmentIsExplicit = true;
	bool bAllStaticEnvironmentHasFootprint = true;
	bool bLeakSentinelsHaveNoStaticMemory = true;
	bool bDynamicDoorHasNoStaticMemory = true;

	for (ULevel* Level : World->GetLevels())
	{
		if (!Level)
		{
			continue;
		}
		for (AActor* Actor : Level->Actors)
		{
			if (!Actor)
			{
				continue;
			}
#if WITH_EDITOR
			const FString Label = Actor->GetActorLabel();
			if (RequiredLabels.Contains(Label))
			{
				FoundLabels.Add(Label);
			}
			M3P5VisionSourceCount += Label.StartsWith(TEXT("SW_M3P5_"))
				&& Actor->FindComponentByClass<USightWeaveVisionSourceComponent>() ? 1 : 0;

			const USightWeaveStaticEnvironmentComponent* StaticEnvironment =
				Actor->FindComponentByClass<USightWeaveStaticEnvironmentComponent>();
			if (StaticEnvironment)
			{
				++StaticEnvironmentCount;
				bAllStaticEnvironmentIsExplicit &= StaticEnvironment->bExplicitlyImmutable;
				bAllStaticEnvironmentHasFootprint &= StaticEnvironment->LocalFootprint.Num() >= 3;
			}

			if (const USightWeaveMemoryModifierComponent* Modifier =
				Actor->FindComponentByClass<USightWeaveMemoryModifierComponent>())
			{
				++MemoryModifierCount;
				BlockModifierCount += Modifier->Operation
					== ESightWeaveMemoryModifierOperation::BlockMemoryWrites ? 1 : 0;
				SuppressModifierCount += Modifier->Operation
					== ESightWeaveMemoryModifierOperation::SuppressMemoryPresentation ? 1 : 0;
			}

			if (Label == TEXT("SW_M3P5_DynamicDoor"))
			{
				bDynamicDoorHasNoStaticMemory = StaticEnvironment == nullptr;
			}
			if (Label == TEXT("SW_M3P5_MovingMeshLeakSentinel")
				|| Label == TEXT("SW_M3P5_CurrentLightLeakSentinel")
				|| Label == TEXT("SW_M3P5_EmissiveParticleLeakSentinel"))
			{
				bLeakSentinelsHaveNoStaticMemory &= StaticEnvironment == nullptr;
			}
#endif
		}
	}

	for (const FString& RequiredLabel : RequiredLabels)
	{
		TestTrue(*FString::Printf(TEXT("Lab contains %s"), *RequiredLabel),
			FoundLabels.Contains(RequiredLabel));
	}
	TestEqual(TEXT("Lab has thirteen explicitly authored static-memory regions"),
		StaticEnvironmentCount, 13);
	TestEqual(TEXT("Lab has two authored memory modifiers"), MemoryModifierCount, 2);
	TestEqual(TEXT("Lab has one BlockMemoryWrites modifier"), BlockModifierCount, 1);
	TestEqual(TEXT("Lab has one SuppressMemoryPresentation modifier"), SuppressModifierCount, 1);
	TestEqual(TEXT("Lab has four M3.5 memory-transition vision sources"), M3P5VisionSourceCount, 4);
	TestTrue(TEXT("Every static-memory region explicitly opts into immutability"),
		bAllStaticEnvironmentIsExplicit);
	TestTrue(TEXT("Every static-memory region owns an explicit polygon"),
		bAllStaticEnvironmentHasFootprint);
	TestTrue(TEXT("Dynamic door is excluded from static memory"), bDynamicDoorHasNoStaticMemory);
	TestTrue(TEXT("Moving/light/emissive sentinels are excluded from static memory"),
		bLeakSentinelsHaveNoStaticMemory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P5LabVisualCaptureTest,
	"SightWeave.M3P5.Visual.LabCapture",
	SightWeave::M3P5::LabTests::TestFlags)

bool FSightWeaveM3P5LabVisualCaptureTest::RunTest(const FString& Parameters)
{
	if (GUsingNullRHI)
	{
		AddError(TEXT("M3.5 Lab visual capture requires a real RHI."));
		return true;
	}
	// PIE deliberately observes two pre-configuration fail-closed transitions
	// before the Lab configures memory, followed by one healthy submitted state.
	// The render diagnostic uses Warning verbosity for every state transition.
	AddExpectedError(
		TEXT("Presentation composite state=fail-binding"),
		EAutomationExpectedErrorFlags::Contains,
		2);
	AddExpectedError(
		TEXT("Presentation composite state=submitted-feather"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	// This visualization-only map may emit a missing-navigation warning depending
	// on the host project's navigation defaults. UE's offscreen PIE bootstrap may
	// likewise emit an engine-owned motion-vector CVar note. Suppress either when
	// present without requiring environment-specific noise for the test to pass.
	AddExpectedError(
		TEXT("Unable to find RecastNavMesh instance while trying to create UCrowdManager instance"),
		EAutomationExpectedErrorFlags::Contains,
		-1);
	AddExpectedError(
		TEXT("Console variable 'r.MotionVectorSimulation' used in the render thread"),
		EAutomationExpectedErrorFlags::Contains,
		-1);

	const FString ScreenshotFilename = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots/M3P5_PIE_Overview.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/SightWeave/Maps/L_SightWeave_Lab")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("setres 1920x1080")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FSightWeaveM3P5TakeLabScreenshotCommand(ScreenshotFilename));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
