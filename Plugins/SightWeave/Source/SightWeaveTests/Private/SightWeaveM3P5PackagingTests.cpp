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
	TestTrue(TEXT("Production composite resolves mutually exclusive Live before Remembered"),
		ShaderSource.Contains(TEXT("const bool bHardLive = SightWeaveIsHardLiveForSurface"))
		&& ShaderSource.Contains(TEXT("const uint State = SightWeaveResolvePresentationState("))
		&& ShaderSource.Contains(TEXT("if (State == SIGHTWEAVE_STATE_LIVE)"))
		&& ShaderSource.Contains(TEXT("return SceneColorTexture.Load"))
		&& ShaderSource.Contains(TEXT("SightWeaveRememberedSurfaceColor("))
		&& ShaderSource.Contains(TEXT("SightWeaveIsVisibleSubjectProxy(DepthPixel, DeviceZ)")));
	TestTrue(TEXT("Remembered branch requires immutable depth/stencil surface plus Memory eligibility"),
		ShaderSource.Contains(TEXT("SightWeaveSampleRememberedSurface("))
		&& ShaderSource.Contains(TEXT("SightWeaveIsRememberedEligibleForSurface("))
		&& ShaderSource.Contains(TEXT("SurfaceStencil != StaticEnvironmentStencilValue"))
		&& ShaderSource.Contains(TEXT("abs(SceneDepthCentimeters - SurfaceDepthCentimeters)"))
		&& ShaderSource.Contains(TEXT("const float Attribute = SightWeaveSampleStaticAttribute")));
	TestTrue(TEXT("Filtered static scene is parameterized and does not reuse current SceneColor"),
		ShaderSource.Contains(TEXT("RememberedBrightness"))
		&& ShaderSource.Contains(TEXT("RememberedContrast"))
		&& ShaderSource.Contains(TEXT("RememberedDetailStrength"))
		&& ShaderSource.Contains(TEXT("SightWeaveRememberedSurfaceColor")));
	for (const TCHAR* Forbidden : {
		TEXT("SceneCapture"), TEXT("TemporalHistory"), TEXT("LastSeen"),
		TEXT("DamageSourceReveal") })
	{
		Excludes(*this, TEXT("Production M3.5 view extension"), ViewExtension, Forbidden);
		Excludes(*this, TEXT("Production M3.5 shader"), ShaderSource, Forbidden);
	}
	TestTrue(TEXT("M4P1 Last-Seen integration is presentation-only and stencil bounded"),
		RenderStateSource.Contains(TEXT("SightWeaveLastSeenProxyComponent.h"))
		&& RenderStateSource.Contains(TEXT("LastSeenProxyStencilValue"))
		&& RenderStateSource.Contains(TEXT("LastSeenProxyNeutralIntensity"))
		&& !RenderStateSource.Contains(TEXT("FSightWeaveSubjectMemoryAuthority"))
		&& !RenderStateSource.Contains(TEXT("FSightWeaveLastSeenSnapshotDescriptor")));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM3P5RememberedTemporalSpaceTest,
	"SightWeave.M3P5.Packaging.RememberedTemporalSpace",
	SightWeave::M3P5::PackagingTests::TestFlags)

bool FSightWeaveM3P5RememberedTemporalSpaceTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M3P5::PackagingTests;
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SightWeave"));
	if (!TestTrue(TEXT("SightWeave plugin is discoverable"), Plugin.IsValid()))
	{
		return false;
	}

	FString ViewExtension;
	FString RenderState;
	FString ShaderSource;
	const FString BaseDir = Plugin->GetBaseDir();
	if (!Load(*this, BaseDir,
			TEXT("Source/SightWeaveRender/Private/SightWeaveSceneViewExtension.cpp"),
			ViewExtension)
		|| !Load(*this, BaseDir,
			TEXT("Source/SightWeaveRender/Private/SightWeaveSparseAtlasRenderState.cpp"),
			RenderState)
		|| !Load(*this, BaseDir,
			TEXT("Shaders/Private/SightWeaveSingleTile.usf"),
			ShaderSource))
	{
		return false;
	}

	const int32 ShadingStart = ShaderSource.Find(
		TEXT("float4 SightWeaveRememberedSurfaceColor("),
		ESearchCase::CaseSensitive);
	const int32 ShadingEnd = ShaderSource.Find(
		TEXT("bool SightWeaveIntersectFloorPlane"),
		ESearchCase::CaseSensitive,
		ESearchDir::FromStart,
		ShadingStart);
	if (!TestTrue(TEXT("Formal Remembered shading function is bounded"),
		ShadingStart != INDEX_NONE && ShadingEnd > ShadingStart))
	{
		return false;
	}
	const FString Shading = ShaderSource.Mid(ShadingStart, ShadingEnd - ShadingStart);

	TestTrue(TEXT("Rejected formal control remains post-TSR/post-tonemap by default"),
		ViewExtension.Contains(TEXT("EPostProcessingPass SelectedPass = EPostProcessingPass::Tonemap"))
		&& ViewExtension.Contains(TEXT("r.SightWeave.Diagnostic.CompositePass"))
		&& ViewExtension.Contains(TEXT("Development/Editor and L_VisionIntegration only")));
	TestTrue(TEXT("Pre-TSR architecture proof is isolated to the integration map"),
		ViewExtension.Contains(TEXT("L_VisionIntegration"))
		&& ViewExtension.Contains(TEXT("EPostProcessingPass::BeforeDOF"))
		&& ViewExtension.Contains(TEXT("bAllowsPreTemporalUpscaleProof")));
	TestTrue(TEXT("Pre-TSR proof forces the non-production fixed Remembered input"),
		ViewExtension.Contains(TEXT("bPreTemporalUpscaleProof"))
		&& ShaderSource.Contains(TEXT("PreTemporalUpscaleProof"))
		&& ShaderSource.Contains(TEXT("DiagnosticMode == 13")));
	TestTrue(TEXT("Pre-TSR proof retains explicit resource-isolation diagnostics"),
		RenderState.Contains(TEXT("RequestedDiagnosticMode"))
		&& RenderState.Contains(TEXT("RequestedDiagnosticMode == 0")));
	TestTrue(TEXT("Remembered shading uses immutable attribute and stencil class"),
		Shading.Contains(TEXT("SightWeaveSampleStaticAttribute"))
		&& Shading.Contains(TEXT("OccluderSurfaceStencilValue")));
	TestTrue(TEXT("Remembered detail is continuous and world anchored"),
		Shading.Contains(TEXT("MemoryTranslatedFloorOrigin"))
		&& Shading.Contains(TEXT("cos(StablePhase.x)"))
		&& Shading.Contains(TEXT("cos(StablePhase.y)")));
	TestFalse(TEXT("Remembered shading does not derive a current-depth normal"),
		Shading.Contains(TEXT("ddx(")) || Shading.Contains(TEXT("ddy(")));
	TestFalse(TEXT("Remembered shading has no discontinuous frac cell pattern"),
		Shading.Contains(TEXT("frac(")));
	return true;
}

#endif
