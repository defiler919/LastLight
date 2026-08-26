#if WITH_DEV_AUTOMATION_TESTS

#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace SightWeave::M3P1::PackagingTests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	bool LoadPluginFile(
		FAutomationTestBase& Test,
		const FString& PluginBaseDir,
		const TCHAR* RelativePath,
		FString& OutContents)
	{
		const FString Path = FPaths::Combine(PluginBaseDir, RelativePath);
		Test.TestTrue(*FString::Printf(TEXT("Packaged plugin contains %s"), RelativePath), FPaths::FileExists(Path));
		const bool bLoaded = FFileHelper::LoadFileToString(OutContents, *Path);
		Test.TestTrue(*FString::Printf(TEXT("Packaged plugin can read %s"), RelativePath), bLoaded);
		return bLoaded;
	}

	void TestDoesNotContain(
		FAutomationTestBase& Test,
		const FString& Label,
		const FString& Contents,
		const TCHAR* ForbiddenText)
	{
		Test.TestFalse(
			*FString::Printf(TEXT("%s excludes %s"), *Label, ForbiddenText),
			Contents.Contains(ForbiddenText, ESearchCase::CaseSensitive));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P1PackagingModuleBoundariesTest,
	"SightWeave.M3P1.Packaging.ModuleBoundariesAndShaderSource",
	SightWeave::M3P1::PackagingTests::TestFlags)

bool FSightWeaveM3P1PackagingModuleBoundariesTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P1::PackagingTests;

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SightWeave"));
	if (!TestTrue(TEXT("SightWeave plugin is discoverable"), Plugin.IsValid()))
	{
		return false;
	}

	const FString PluginBaseDir = Plugin->GetBaseDir();
	FString Descriptor;
	FString RuntimeRules;
	FString RenderRules;
	FString ShaderSource;
	const bool bFilesLoaded =
		LoadPluginFile(*this, PluginBaseDir, TEXT("SightWeave.uplugin"), Descriptor)
		&& LoadPluginFile(
			*this,
			PluginBaseDir,
			TEXT("Source/SightWeaveRuntime/SightWeaveRuntime.Build.cs"),
			RuntimeRules)
		&& LoadPluginFile(
			*this,
			PluginBaseDir,
			TEXT("Source/SightWeaveRender/SightWeaveRender.Build.cs"),
			RenderRules)
		&& LoadPluginFile(
			*this,
			PluginBaseDir,
			TEXT("Shaders/Private/SightWeaveSingleTile.usf"),
			ShaderSource);
	if (!bFilesLoaded)
	{
		return false;
	}

	TestTrue(TEXT("Render module is present in the descriptor"), Descriptor.Contains(TEXT("\"SightWeaveRender\"")));
	TestTrue(TEXT("Render module loads before runtime publication"), Descriptor.Contains(TEXT("\"PostConfigInit\"")));
	TestDoesNotContain(*this, TEXT("Runtime rules"), RuntimeRules, TEXT("\"SightWeaveRender\""));

	const TCHAR* ForbiddenRenderDependencies[] =
	{
		TEXT("\"Darkwell\""),
		TEXT("\"UnrealEd\""),
		TEXT("\"SightWeaveEditor\""),
		TEXT("\"SightWeaveTests\""),
		TEXT("AutomationTest")
	};
	for (const TCHAR* ForbiddenDependency : ForbiddenRenderDependencies)
	{
		TestDoesNotContain(*this, TEXT("Render rules"), RenderRules, ForbiddenDependency);
	}

	TestTrue(TEXT("Shader source maps the engine platform contract"), ShaderSource.Contains(TEXT("/Engine/Public/Platform.ush")));
	TestTrue(TEXT("Shader source contains the raster vertex entry point"), ShaderSource.Contains(TEXT("SightWeaveSmokeVS")));
	TestTrue(TEXT("Shader source contains the binary mask pixel entry point"), ShaderSource.Contains(TEXT("SightWeaveSmokePS")));
	TestTrue(TEXT("Shader source contains the combine entry point"), ShaderSource.Contains(TEXT("SightWeaveCombinePS")));
	return true;
}

#endif
