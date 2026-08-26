#if WITH_DEV_AUTOMATION_TESTS

#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace SightWeave::M3P2::PackagingTests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	bool Load(
		FAutomationTestBase& Test,
		const FString& PluginBaseDir,
		const TCHAR* RelativePath,
		FString& OutContents)
	{
		const FString Path = FPaths::Combine(PluginBaseDir, RelativePath);
		Test.TestTrue(*FString::Printf(TEXT("Plugin contains %s"), RelativePath), FPaths::FileExists(Path));
		const bool bLoaded = FFileHelper::LoadFileToString(OutContents, *Path);
		Test.TestTrue(*FString::Printf(TEXT("Plugin can read %s"), RelativePath), bLoaded);
		return bLoaded;
	}

	void Excludes(
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
	FSightWeaveM3P2PackagingBoundariesTest,
	"SightWeave.M3P2.Packaging.SparseAtlasShippingBoundaries",
	SightWeave::M3P2::PackagingTests::TestFlags)

bool FSightWeaveM3P2PackagingBoundariesTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P2::PackagingTests;
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SightWeave"));
	if (!TestTrue(TEXT("SightWeave plugin is discoverable"), Plugin.IsValid()))
	{
		return false;
	}

	const FString BaseDir = Plugin->GetBaseDir();
	FString Descriptor;
	FString RuntimeRules;
	FString RenderRules;
	FString SparseContract;
	FString RenderStateHeader;
	FString RenderStateSource;
	FString ReadbackHeader;
	FString ReadbackSource;
	FString ShaderSource;
	const bool bLoaded =
		Load(*this, BaseDir, TEXT("SightWeave.uplugin"), Descriptor)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRuntime/SightWeaveRuntime.Build.cs"), RuntimeRules)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/SightWeaveRender.Build.cs"), RenderRules)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRuntime/Public/SightWeaveSparseAtlas.h"), SparseContract)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeaveSparseAtlasRenderState.h"), RenderStateHeader)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeaveSparseAtlasRenderState.cpp"), RenderStateSource)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Public/SightWeaveSparseAtlasTestReadback.h"), ReadbackHeader)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeaveSparseAtlasTestReadback.cpp"), ReadbackSource)
		&& Load(*this, BaseDir, TEXT("Shaders/Private/SightWeaveSingleTile.usf"), ShaderSource);
	if (!bLoaded)
	{
		return false;
	}

	TestTrue(TEXT("Render module remains Runtime"),
		Descriptor.Contains(TEXT("\"Name\": \"SightWeaveRender\""))
		&& Descriptor.Contains(TEXT("\"Type\": \"Runtime\"")));
	TestTrue(TEXT("Render shader mapping remains PostConfigInit"),
		Descriptor.Contains(TEXT("\"LoadingPhase\": \"PostConfigInit\"")));
	Excludes(*this, TEXT("Runtime rules"), RuntimeRules, TEXT("\"SightWeaveRender\""));
	for (const TCHAR* ForbiddenDependency : {
		TEXT("\"Darkwell\""),
		TEXT("\"UnrealEd\""),
		TEXT("\"SightWeaveEditor\""),
		TEXT("\"SightWeaveTests\""),
		TEXT("AutomationTest") })
	{
		Excludes(*this, TEXT("Render rules"), RenderRules, ForbiddenDependency);
	}

	TestTrue(TEXT("Sparse contract freezes 256 physical texels"),
		SparseContract.Contains(TEXT("PhysicalTileSize = 256")));
	TestTrue(TEXT("Sparse contract freezes 248 interior texels"),
		SparseContract.Contains(TEXT("InteriorTileSize = 248")));
	TestTrue(TEXT("Sparse contract freezes four gutter texels"),
		SparseContract.Contains(TEXT("GutterTexels = 4")));
	TestTrue(TEXT("Sparse contract freezes 2048 page texels"),
		SparseContract.Contains(TEXT("PageSize = 2048")));
	TestTrue(TEXT("Sparse contract freezes Standard capacity at 128"),
		SparseContract.Contains(TEXT("StandardActiveTileCapacity = 128")));
	TestTrue(TEXT("Render state owns persistent pooled pages"),
		RenderStateHeader.Contains(TEXT("TRefCountPtr<IPooledRenderTarget>"))
		&& RenderStateSource.Contains(TEXT("PF_G8")));
	TestTrue(TEXT("Sparse readback public API is development-only"),
		ReadbackHeader.StartsWith(TEXT("#pragma once"))
		&& ReadbackHeader.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS")));
	TestTrue(TEXT("Sparse readback implementation is development-only"),
		ReadbackSource.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS")));
	Excludes(*this, TEXT("Production sparse render state"), RenderStateHeader,
		TEXT("SightWeaveSparseAtlasTestReadback"));
	Excludes(*this, TEXT("Production sparse render implementation"), RenderStateSource,
		TEXT("SightWeaveSparseAtlasTestReadback"));

	TestTrue(TEXT("Shader maps the engine platform contract"),
		ShaderSource.Contains(TEXT("/Engine/Public/Platform.ush")));
	TestTrue(TEXT("Shader applies atlas viewport origins"),
		ShaderSource.Contains(TEXT("RasterTargetOriginX"))
		&& ShaderSource.Contains(TEXT("DestinationOriginX")));
	TestTrue(TEXT("Shader contains profile-aware atlas combine"),
		ShaderSource.Contains(TEXT("SightWeaveAtlasProfileCombinePS")));
	TestTrue(TEXT("Shader contains suppression-last atlas pass"),
		ShaderSource.Contains(TEXT("SightWeaveAtlasSuppressionPS")));
	return true;
}

#endif
