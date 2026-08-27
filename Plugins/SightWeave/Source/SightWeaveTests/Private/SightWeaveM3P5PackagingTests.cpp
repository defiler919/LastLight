#if WITH_DEV_AUTOMATION_TESTS

#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace SightWeave::M3P5::PackagingTests
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
		Test.TestTrue(*FString::Printf(TEXT("Plugin contains %s"), RelativePath),
			FPaths::FileExists(Path));
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
	FSightWeaveM3P5PackagingBoundariesTest,
	"SightWeave.M3P5.Packaging.StaticEnvironmentMemoryShippingBoundaries",
	SightWeave::M3P5::PackagingTests::TestFlags)

bool FSightWeaveM3P5PackagingBoundariesTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P5::PackagingTests;
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SightWeave"));
	if (!TestTrue(TEXT("SightWeave plugin is discoverable"), Plugin.IsValid()))
	{
		return false;
	}

	const FString BaseDir = Plugin->GetBaseDir();
	FString Descriptor;
	FString RuntimeRules;
	FString RenderRules;
	FString MemoryHeader;
	FString StaticHeader;
	FString ViewExtension;
	FString RenderStateHeader;
	FString RenderStateSource;
	FString ShaderHeader;
	FString ShaderRegistration;
	FString ShaderSource;
	FString MemoryReadbackHeader;
	FString MemoryReadbackSource;
	FString CompositeReadbackHeader;
	FString CompositeReadbackSource;
	const bool bLoaded =
		Load(*this, BaseDir, TEXT("SightWeave.uplugin"), Descriptor)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRuntime/SightWeaveRuntime.Build.cs"), RuntimeRules)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/SightWeaveRender.Build.cs"), RenderRules)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRuntime/Public/SightWeaveMemory.h"), MemoryHeader)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRuntime/Public/SightWeaveStaticEnvironment.h"), StaticHeader)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeaveSceneViewExtension.cpp"), ViewExtension)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeaveSparseAtlasRenderState.h"), RenderStateHeader)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeaveSparseAtlasRenderState.cpp"), RenderStateSource)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeaveTileShaders.h"), ShaderHeader)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeaveTileShaders.cpp"), ShaderRegistration)
		&& Load(*this, BaseDir, TEXT("Shaders/Private/SightWeaveSingleTile.usf"), ShaderSource)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Public/SightWeaveMemoryTestReadback.h"), MemoryReadbackHeader)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeaveMemoryTestReadback.cpp"), MemoryReadbackSource)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Public/SightWeaveMemoryPresentationTestReadback.h"), CompositeReadbackHeader)
		&& Load(*this, BaseDir, TEXT("Source/SightWeaveRender/Private/SightWeaveMemoryPresentationTestReadback.cpp"), CompositeReadbackSource);
	if (!bLoaded)
	{
		return false;
	}

	TestTrue(TEXT("Only Runtime and Render modules can enter a game target"),
		Descriptor.Contains(TEXT("\"Name\": \"SightWeaveRender\""))
		&& Descriptor.Contains(TEXT("\"Name\": \"SightWeaveRuntime\""))
		&& Descriptor.Contains(TEXT("\"Name\": \"SightWeaveEditor\""))
		&& Descriptor.Contains(TEXT("\"Type\": \"Editor\""))
		&& Descriptor.Contains(TEXT("\"Name\": \"SightWeaveTests\"")));
	for (const TCHAR* Forbidden : {
		TEXT("\"Darkwell\""), TEXT("\"UnrealEd\""), TEXT("\"SightWeaveEditor\""),
		TEXT("\"SightWeaveTests\""), TEXT("AutomationTest"), TEXT("SceneCapture") })
	{
		Excludes(*this, TEXT("Runtime rules"), RuntimeRules, Forbidden);
		Excludes(*this, TEXT("Render rules"), RenderRules, Forbidden);
	}

	for (const TCHAR* Forbidden : {
		TEXT("SceneCapture"), TEXT("SceneColor"), TEXT("Camera"), TEXT("Viewport"),
		TEXT("RHIGPUReadback"), TEXT("SaveGame"), TEXT("LastSeen") })
	{
		Excludes(*this, TEXT("CPU Memory authority API"), MemoryHeader, Forbidden);
	}
	TestTrue(TEXT("CPU Memory authority publishes immutable owned packets"),
		MemoryHeader.Contains(TEXT("Immutable owned CPU-authority publication"))
		&& MemoryHeader.Contains(TEXT("TSharedPtr<const FSightWeaveMemoryPacket"))
		&& MemoryHeader.Contains(TEXT("WriteEffectiveLive")));
	TestTrue(TEXT("Static eligibility is explicit immutable authored data"),
		StaticHeader.Contains(TEXT("FSightWeaveStaticEnvironmentDescription"))
		&& StaticHeader.Contains(TEXT("bExplicitlyImmutable"))
		&& StaticHeader.Contains(TEXT("FSightWeaveStaticEnvironmentPacket")));
	for (const TCHAR* Forbidden : {
		TEXT("SceneCapture"), TEXT("SceneColor"), TEXT("TemporalHistory"),
		TEXT("LastSeen"), TEXT("Particle"), TEXT("SkeletalMesh") })
	{
		Excludes(*this, TEXT("Static environment authority API"), StaticHeader, Forbidden);
	}

	TestTrue(TEXT("Production SVE prepares Memory/static resources before composite"),
		ViewExtension.Contains(TEXT("PrepareMemoryPresentationResources_RenderThread"))
		&& ViewExtension.Contains(TEXT("AddHardMaskComposite_RenderThread")));
	TestTrue(TEXT("Production composite orders HardLive before remembered Memory"),
		ShaderSource.Contains(TEXT("const bool bVisible = SightWeaveIsHardLive(TranslatedWorld.xy)"))
		&& ShaderSource.Contains(TEXT("SightWeaveRememberedEnvironment(DepthPixel)"))
		&& ShaderSource.Contains(TEXT("return bVisible")));
	TestTrue(TEXT("Remembered branch requires Memory, eligibility, and fixed attribute"),
		ShaderSource.Contains(TEXT("SightWeaveSampleMemory(TranslatedFloorPosition) < 0.5f"))
		&& ShaderSource.Contains(TEXT("const float Attribute = SightWeaveSampleStaticAttribute"))
		&& ShaderSource.Contains(TEXT("return Attribute > 0.0f ? float4(Attribute.xxx, 1.0f) : 0.0f")));
	for (const TCHAR* Forbidden : {
		TEXT("SceneCapture"), TEXT("TemporalHistory"), TEXT("LastSeen"),
		TEXT("DamageSourceReveal") })
	{
		Excludes(*this, TEXT("Production M3.5 view extension"), ViewExtension, Forbidden);
		Excludes(*this, TEXT("Production M3.5 render state"), RenderStateSource, Forbidden);
		Excludes(*this, TEXT("Production M3.5 shader"), ShaderSource, Forbidden);
	}

	TestTrue(TEXT("Memory mirror readback API and implementation are development-only"),
		MemoryReadbackHeader.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS"))
		&& MemoryReadbackSource.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS"))
		&& MemoryReadbackHeader.Contains(TEXT("FSightWeaveMemoryTestReadback")));
	TestTrue(TEXT("Memory composite readback API and implementation are development-only"),
		CompositeReadbackHeader.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS"))
		&& CompositeReadbackSource.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS"))
		&& CompositeReadbackHeader.Contains(TEXT("FSightWeaveMemoryPresentationTestReadback")));
	TestTrue(TEXT("M3.5 test shader declaration and registration are development-only"),
		ShaderHeader.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS"))
		&& ShaderHeader.Contains(TEXT("FSightWeaveMemoryPresentationTestPixelShader"))
		&& ShaderRegistration.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS"))
		&& ShaderRegistration.Contains(TEXT("SightWeaveMemoryPresentationTestPS")));
	TestTrue(TEXT("M3.5 render-state injection entry point is development-only"),
		RenderStateHeader.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS"))
		&& RenderStateHeader.Contains(TEXT("AddMemoryPresentationTestComposite_RenderThread")));
	return true;
}

#endif
