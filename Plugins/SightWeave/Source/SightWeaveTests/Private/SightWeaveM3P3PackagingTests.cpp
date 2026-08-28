#if WITH_DEV_AUTOMATION_TESTS

#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace SightWeave::M3P3::PackagingTests
{
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	bool Load(
		FAutomationTestBase& Test,
		const FString& BaseDir,
		const TCHAR* RelativePath,
		FString& OutContents)
	{
		const FString Path = FPaths::Combine(BaseDir, RelativePath);
		Test.TestTrue(*FString::Printf(TEXT("Plugin contains %s"), RelativePath), FPaths::FileExists(Path));
		const bool bLoaded = FFileHelper::LoadFileToString(OutContents, *Path);
		Test.TestTrue(*FString::Printf(TEXT("Plugin can read %s"), RelativePath), bLoaded);
		return bLoaded;
	}

	void Excludes(
		FAutomationTestBase& Test,
		const FString& Label,
		const FString& Contents,
		const TCHAR* Forbidden)
	{
		Test.TestFalse(
			*FString::Printf(TEXT("%s excludes %s"), *Label, Forbidden),
			Contents.Contains(Forbidden, ESearchCase::CaseSensitive));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P3PackagingBoundariesTest,
	"SightWeave.M3P3.Packaging.HardMaskShippingBoundaries",
	SightWeave::M3P3::PackagingTests::TestFlags)

bool FSightWeaveM3P3PackagingBoundariesTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P3::PackagingTests;
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SightWeave"));
	if (!TestTrue(TEXT("SightWeave plugin is discoverable"), Plugin.IsValid()))
	{
		return false;
	}

	const FString BaseDir = Plugin->GetBaseDir();
	FString Descriptor;
	FString RenderRules;
	FString ViewExtension;
	FString RenderStateHeader;
	FString ShaderHeader;
	FString ShaderRegistration;
	FString ShaderSource;
	FString ReadbackHeader;
	FString ReadbackSource;
	FString BenchmarkHeader;
	FString BenchmarkSource;
	const bool bLoaded =
		Load(*this, BaseDir, TEXT("SightWeave.uplugin"), Descriptor)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/SightWeaveRender.Build.cs"), RenderRules)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeaveSceneViewExtension.cpp"), ViewExtension)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeaveSparseAtlasRenderState.h"), RenderStateHeader)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeaveTileShaders.h"), ShaderHeader)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeaveTileShaders.cpp"), ShaderRegistration)
		&& Load(*this, BaseDir, TEXT("Shaders/Private/SightWeaveSingleTile.usf"), ShaderSource)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Public/SightWeavePresentationTestReadback.h"), ReadbackHeader)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeavePresentationTestReadback.cpp"), ReadbackSource)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Public/SightWeavePresentationBenchmark.h"), BenchmarkHeader)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeavePresentationBenchmark.cpp"), BenchmarkSource);
	if (!bLoaded)
	{
		return false;
	}

	TestTrue(TEXT("Presentation render module remains Runtime/PostConfigInit"),
		Descriptor.Contains(TEXT("\"Name\": \"SightWeaveRender\""))
		&& Descriptor.Contains(TEXT("\"Type\": \"Runtime\""))
		&& Descriptor.Contains(TEXT("\"LoadingPhase\": \"PostConfigInit\"")));
	for (const TCHAR* Forbidden : {
		TEXT("\"Darkwell\""), TEXT("\"UnrealEd\""), TEXT("\"SightWeaveTests\""),
		TEXT("AutomationTest"), TEXT("SceneCapture") })
	{
		Excludes(*this, TEXT("Render module rules"), RenderRules, Forbidden);
	}

	TestTrue(TEXT("SVE injects through the supported Tonemap after-pass callback"),
		ViewExtension.Contains(TEXT("PassId == EPostProcessingPass::Tonemap"))
		&& ViewExtension.Contains(TEXT("PostProcessPassAfterTonemap_RenderThread"))
		&& ViewExtension.Contains(TEXT("AddHardMaskComposite_RenderThread")));
	Excludes(*this, TEXT("Production SVE"), ViewExtension, TEXT("SceneCapture"));
	TestTrue(TEXT("Production shader reconstructs translated world from scene depth"),
		ShaderSource.Contains(TEXT("SceneDepthTexture.Load"))
		&& ShaderSource.Contains(TEXT("SvPositionToTranslatedWorld"))
		&& ShaderSource.Contains(TEXT("TranslatedFloorOrigin")));
	TestTrue(TEXT("Production shader uses negative-safe logical tile floor mapping"),
		ShaderSource.Contains(TEXT("floor(LocalPosition / InteriorSpan)")));
	TestTrue(TEXT("Production shader uses integer atlas loads and the four-texel gutter"),
		ShaderSource.Contains(TEXT("AtlasPage0.Load"))
		&& ShaderSource.Contains(TEXT("SlotOrigin + int2(4, 4) + InteriorTexel")));
	TestTrue(TEXT("HardLive branch still preserves the current Scene Color before later fallbacks"),
		ShaderSource.Contains(TEXT("const bool bVisible = SightWeaveIsHardLive(TranslatedWorld.xy)"))
		&& ShaderSource.Contains(TEXT("if (bVisible)"))
		&& ShaderSource.Contains(TEXT("return SceneColorTexture.Load(int3(SceneColorPixel, 0));")));
	for (const TCHAR* Forbidden : {
		TEXT("TemporalHistory"), TEXT("LastSeen"), TEXT("SceneCapture") })
	{
		Excludes(*this, TEXT("Hard-mask shader"), ShaderSource, Forbidden);
	}

	TestTrue(TEXT("Readback API and implementation are development-only"),
		ReadbackHeader.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS"))
		&& ReadbackSource.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS")));
	TestTrue(TEXT("Benchmark API and implementation are development-only"),
		BenchmarkHeader.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS"))
		&& BenchmarkSource.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS")));
	TestTrue(TEXT("Test shaders are declared and registered behind the development guard"),
		ShaderHeader.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS"))
		&& ShaderHeader.Contains(TEXT("FSightWeavePresentationBenchmarkPixelShader"))
		&& ShaderRegistration.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS"))
		&& ShaderRegistration.Contains(TEXT("SightWeavePresentationBenchmarkPS")));
	TestTrue(TEXT("Production render state keeps test entry points behind the development guard"),
		RenderStateHeader.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS"))
		&& RenderStateHeader.Contains(TEXT("AddPresentationBenchmarkComposite_RenderThread")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P4PackagingBoundariesTest,
	"SightWeave.M3P4.Packaging.InwardFeatherShippingBoundaries",
	SightWeave::M3P3::PackagingTests::TestFlags)

bool FSightWeaveM3P4PackagingBoundariesTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P3::PackagingTests;
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SightWeave"));
	if (!TestTrue(TEXT("SightWeave plugin is discoverable"), Plugin.IsValid()))
	{
		return false;
	}

	const FString BaseDir = Plugin->GetBaseDir();
	FString RuntimeRules;
	FString RenderRules;
	FString PresentationHeader;
	FString ViewExtension;
	FString RenderStateHeader;
	FString RenderStateSource;
	FString ShaderHeader;
	FString ShaderRegistration;
	FString ShaderSource;
	FString ReadbackHeader;
	FString ReadbackSource;
	FString BenchmarkHeader;
	FString BenchmarkSource;
	const bool bLoaded =
		Load(*this, BaseDir, TEXT("Source/SightWeaveRuntime/SightWeaveRuntime.Build.cs"), RuntimeRules)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/SightWeaveRender.Build.cs"), RenderRules)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRuntime/Public/SightWeavePresentation.h"), PresentationHeader)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeaveSceneViewExtension.cpp"), ViewExtension)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeaveSparseAtlasRenderState.h"), RenderStateHeader)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeaveSparseAtlasRenderState.cpp"), RenderStateSource)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeaveTileShaders.h"), ShaderHeader)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeaveTileShaders.cpp"), ShaderRegistration)
		&& Load(*this, BaseDir, TEXT("Shaders/Private/SightWeaveSingleTile.usf"), ShaderSource)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Public/SightWeavePresentationTestReadback.h"), ReadbackHeader)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeavePresentationTestReadback.cpp"), ReadbackSource)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Public/SightWeavePresentationBenchmark.h"), BenchmarkHeader)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeavePresentationBenchmark.cpp"), BenchmarkSource);
	if (!bLoaded)
	{
		return false;
	}

	for (const TCHAR* Forbidden : {
		TEXT("\"Darkwell\""), TEXT("\"SightWeaveTests\""), TEXT("\"UnrealEd\""),
		TEXT("\"AIModule\""), TEXT("\"UMG\""), TEXT("SceneCapture") })
	{
		Excludes(*this, TEXT("Runtime module rules"), RuntimeRules, Forbidden);
		Excludes(*this, TEXT("Render module rules"), RenderRules, Forbidden);
	}
	TestTrue(TEXT("Visual Feather is explicitly presentation-only and bounded"),
		PresentationHeader.Contains(TEXT("Presentation-only setting"))
		&& PresentationHeader.Contains(TEXT("MaximumWidthCentimeters = 100.0f"))
		&& PresentationHeader.Contains(TEXT("WidthCentimeters = 0.0f")));
	TestTrue(TEXT("Production SVE derives Feather before the post-tonemap composite"),
		ViewExtension.Contains(TEXT("ProcessVisualFeather_RenderThread(GraphBuilder)"))
		&& ViewExtension.Contains(TEXT("PassId == EPostProcessingPass::Tonemap"))
		&& ViewExtension.Contains(TEXT("AddHardMaskComposite_RenderThread")));
	TestTrue(TEXT("Production Feather composite hard-gates before continuous sampling"),
		ShaderSource.Contains(TEXT("if (!SightWeaveIsHardLive(TranslatedWorld.xy))"))
		&& ShaderSource.Contains(TEXT("return 0.0f;"))
		&& ShaderSource.Contains(TEXT("SceneColorTexture.Load(int3(SceneColorPixel, 0)) * VisualFeatherWeight")));
	TestTrue(TEXT("Feather derives from logical page-table lookup rather than physical adjacency"),
		ShaderSource.Contains(TEXT("SightWeaveFloorDiv(LogicalTexel.x, 248)"))
		&& ShaderSource.Contains(TEXT("SightWeaveFindPageTableEntry(LogicalCoordinate)"))
		&& RenderStateSource.Contains(TEXT("MarkFeatherDirtyAround_RenderThread")));
	TestTrue(TEXT("Width zero releases Feather resources and remains on the hard path"),
		RenderStateSource.Contains(TEXT("!PresentationSelection.GetVisualFeather().IsEnabled()"))
		&& RenderStateSource.Contains(TEXT("ReleaseFeatherResources_RenderThread()"))
		&& RenderStateSource.Contains(TEXT("FSightWeaveHardMaskCompositePixelShader")));
	for (const TCHAR* Forbidden : {
		TEXT("SceneCapture"), TEXT("MemoryLayer"),
		TEXT("TemporalHistory"), TEXT("DamageSourceReveal") })
	{
		Excludes(*this, TEXT("Production view extension"), ViewExtension, Forbidden);
		Excludes(*this, TEXT("Production Feather render state"), RenderStateSource, Forbidden);
		Excludes(*this, TEXT("Production Feather shader"), ShaderSource, Forbidden);
	}
	Excludes(*this, TEXT("Production view extension"), ViewExtension, TEXT("LastSeen"));
	Excludes(*this, TEXT("Production Feather shader"), ShaderSource, TEXT("LastSeen"));
	TestTrue(TEXT("M4P1 Last-Seen composition is the reserved production proxy path"),
		RenderStateSource.Contains(TEXT("LastSeenProxyStencilValue"))
		&& RenderStateSource.Contains(TEXT("LastSeenProxyNeutralIntensity")));
	TestTrue(TEXT("Readback and benchmark C++ APIs remain development-only"),
		ReadbackHeader.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS"))
		&& ReadbackSource.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS"))
		&& BenchmarkHeader.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS"))
		&& BenchmarkSource.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS")));
	TestTrue(TEXT("Feather readback and benchmark shaders remain behind development guards"),
		ShaderHeader.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS"))
		&& ShaderHeader.Contains(TEXT("FSightWeaveFeatherPresentationTestPixelShader"))
		&& ShaderHeader.Contains(TEXT("FSightWeaveFeatherPresentationBenchmarkPixelShader"))
		&& ShaderRegistration.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS"))
		&& ShaderRegistration.Contains(TEXT("SightWeaveFeatherPresentationTestPS"))
		&& ShaderRegistration.Contains(TEXT("SightWeaveFeatherPresentationBenchmarkPS")));
	TestTrue(TEXT("Render-state test entry points remain development-only"),
		RenderStateHeader.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS"))
		&& RenderStateHeader.Contains(TEXT("AddPresentationTestComposite_RenderThread"))
		&& RenderStateHeader.Contains(TEXT("AddPresentationBenchmarkComposite_RenderThread")));
	return true;
}

#endif
