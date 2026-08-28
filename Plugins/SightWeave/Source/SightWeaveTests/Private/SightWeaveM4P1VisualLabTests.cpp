#if WITH_DEV_AUTOMATION_TESTS

#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "SightWeaveLabSupport.h"

#include "HAL/FileManager.h"
#include "UnrealClient.h"

namespace SightWeave::M4P1::VisualLabTests
{
	constexpr EAutomationTestFlags ContractFlags = EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter;
	constexpr EAutomationTestFlags VisualFlags = ContractFlags
		| EAutomationTestFlags::NonNullRHI;

	enum class EExpectation : uint8
	{
		Overview,
		Live,
		Proxy,
		StrictBlack,
		Boundary,
		YawIsolation
	};
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(
	FSightWeaveM4P1TakeScreenshotCommand,
	FString,
	ScreenshotFilename);

bool FSightWeaveM4P1TakeScreenshotCommand::Update()
{
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenshotFilename), true);
	IFileManager::Get().Delete(*ScreenshotFilename, false, true);
	FScreenshotRequest::RequestScreenshot(
		ScreenshotFilename,
		/* bInShowUI = */ false,
		/* bAddUniqueSuffix = */ false,
		/* bHdrScreenshot = */ false,
		FIntRect(),
		/* bInRestrictToGameViewport = */ true);
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(
	FSightWeaveM4P1ValidateScreenshotCommand,
	FAutomationTestBase*,
	Test,
	FString,
	ScreenshotFilename,
	SightWeave::M4P1::VisualLabTests::EExpectation,
	Expectation);

bool FSightWeaveM4P1ValidateScreenshotCommand::Update()
{
	if (!IFileManager::Get().FileExists(*ScreenshotFilename))
	{
		Test->AddError(FString::Printf(TEXT("M4P1 screenshot was not written: %s"),
			*ScreenshotFilename));
		return true;
	}

	FImage SourceImage;
	if (!FImageUtils::LoadImage(*ScreenshotFilename, SourceImage)
		|| SourceImage.SizeX <= 0
		|| SourceImage.SizeY <= 0)
	{
		Test->AddError(FString::Printf(TEXT("M4P1 screenshot could not be decoded: %s"),
			*ScreenshotFilename));
		return true;
	}
	FImage BGRAImage;
	SourceImage.CopyTo(BGRAImage, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	FImage LinearImage;
	SourceImage.CopyTo(LinearImage, ERawImageFormat::RGBA32F, EGammaSpace::Linear);

	uint64 BlackCount = 0;
	uint64 NonBlackCount = 0;
	uint64 NeutralCount = 0;
	uint64 ExactProxyCount = 0;
	uint64 NonFiniteCount = 0;
	uint64 CenterNonBlackCount = 0;
	const int32 CenterMinX = SourceImage.SizeX * 42 / 100;
	const int32 CenterMaxX = SourceImage.SizeX * 58 / 100;
	const int32 CenterMinY = SourceImage.SizeY * 36 / 100;
	const int32 CenterMaxY = SourceImage.SizeY * 64 / 100;
	const TArrayView64<FColor> Pixels = BGRAImage.AsBGRA8();
	const TArrayView64<FLinearColor> LinearPixels = LinearImage.AsRGBA32F();
	for (int32 Y = 0; Y < SourceImage.SizeY; ++Y)
	{
		for (int32 X = 0; X < SourceImage.SizeX; ++X)
		{
			const int64 PixelIndex = static_cast<int64>(Y) * SourceImage.SizeX + X;
			const FColor Pixel = Pixels[PixelIndex];
			const bool bBlack = Pixel.R == 0 && Pixel.G == 0 && Pixel.B == 0;
			BlackCount += bBlack ? 1 : 0;
			NonBlackCount += bBlack ? 0 : 1;
			if (!bBlack
				&& X >= CenterMinX && X < CenterMaxX
				&& Y >= CenterMinY && Y < CenterMaxY)
			{
				++CenterNonBlackCount;
			}
			const int32 Minimum = FMath::Min3<int32>(Pixel.R, Pixel.G, Pixel.B);
			const int32 Maximum = FMath::Max3<int32>(Pixel.R, Pixel.G, Pixel.B);
			NeutralCount += Minimum >= 70 && Maximum <= 190 && Maximum - Minimum <= 4 ? 1 : 0;
			ExactProxyCount += Pixel.R == 117 && Pixel.G == 117 && Pixel.B == 117 ? 1 : 0;
			const FLinearColor Linear = LinearPixels[PixelIndex];
			NonFiniteCount += FMath::IsFinite(Linear.R)
				&& FMath::IsFinite(Linear.G)
				&& FMath::IsFinite(Linear.B)
				&& FMath::IsFinite(Linear.A) ? 0 : 1;
		}
	}
	const uint64 TotalPixels = static_cast<uint64>(SourceImage.SizeX) * SourceImage.SizeY;
	Test->AddInfo(FString::Printf(
		TEXT("M4P1_PIXEL_METRICS file=%s size=%dx%d black=%llu nonblack=%llu neutral=%llu exactProxy=%llu centerNonblack=%llu nonfinite=%llu"),
		*FPaths::GetCleanFilename(ScreenshotFilename),
		SourceImage.SizeX,
		SourceImage.SizeY,
		BlackCount,
		NonBlackCount,
		NeutralCount,
		ExactProxyCount,
		CenterNonBlackCount,
		NonFiniteCount));
	Test->TestEqual(TEXT("Pixel readback has no non-finite channels"), NonFiniteCount, uint64(0));

	using namespace SightWeave::M4P1::VisualLabTests;
	switch (Expectation)
	{
	case EExpectation::StrictBlack:
		Test->TestEqual(TEXT("Fail-black viewport has zero RGB leakage"), NonBlackCount, uint64(0));
		break;
	case EExpectation::Live:
		Test->TestTrue(TEXT("Live fixture has non-black Scene Color at camera center"),
			CenterNonBlackCount >= 256);
		Test->TestEqual(TEXT("Reacquired live fixture has no exact proxy-color residue"),
			ExactProxyCount, uint64(0));
		break;
	case EExpectation::Proxy:
		Test->TestTrue(TEXT("Remembered proxy has deterministic neutral pixels"),
			ExactProxyCount >= 256);
		Test->TestEqual(TEXT("Last-Seen view has zero non-proxy RGB leakage"),
			NonBlackCount, ExactProxyCount);
		Test->TestTrue(TEXT("Proxy fixture retains strict black surroundings"),
			BlackCount * 2 > TotalPixels);
		break;
	case EExpectation::Boundary:
		Test->TestTrue(TEXT("Boundary/yaw fixture has deterministic neutral proxy pixels"),
			ExactProxyCount >= 256);
		Test->TestEqual(TEXT("Boundary/yaw negative regions have zero RGB leakage"),
			NonBlackCount, ExactProxyCount);
		Test->TestTrue(TEXT("Boundary/yaw proxy count excludes hidden negative fixtures"),
			ExactProxyCount <= 4500);
		Test->TestTrue(TEXT("Boundary/yaw fixture has fail-black negative regions"),
			BlackCount * 2 > TotalPixels);
		break;
	case EExpectation::YawIsolation:
		Test->TestTrue(TEXT("Yaw fixture has deterministic neutral proxy pixels"),
			ExactProxyCount >= 256);
		Test->TestEqual(TEXT("Yaw/identity/scope/custom negative regions have zero RGB leakage"),
			NonBlackCount, ExactProxyCount);
		Test->TestTrue(TEXT("Yaw proxy count excludes hidden identity/scope/custom fixtures"),
			ExactProxyCount <= 1600);
		Test->TestTrue(TEXT("Yaw fixture has fail-black negative regions"),
			BlackCount * 2 > TotalPixels);
		break;
	case EExpectation::Overview:
	default:
		Test->TestTrue(TEXT("Overview contains deterministic Last-Seen proxy pixels"),
			ExactProxyCount >= 256);
		Test->TestTrue(TEXT("Overview contains fail-black policy regions"),
			BlackCount > TotalPixels / 4);
		break;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P1LabModeContractTest,
	"SightWeave.M4P1.Lab.ModeContract",
	SightWeave::M4P1::VisualLabTests::ContractFlags)

bool FSightWeaveM4P1LabModeContractTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("M4P1 mode string is stable"),
		FString(SightWeave::Lab::LexToString(ESightWeaveLabMode::M4P1)),
		FString(TEXT("M4P1")));
	const TCHAR* ExpectedLabels[] = {
		TEXT("SW_M4P1_Camera0_Overview"),
		TEXT("SW_M4P1_Camera1_Transition"),
		TEXT("SW_M4P1_Camera2_PolicyMatrix"),
		TEXT("SW_M4P1_Camera3_PageBoundary"),
		TEXT("SW_M4P1_Camera4_Rotated45")
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedLabels); ++Index)
	{
		TestEqual(*FString::Printf(TEXT("M4P1 camera %d label is stable"), Index),
			FString(SightWeave::Lab::GetCameraLabel(
				ESightWeaveLabMode::M4P1,
				static_cast<ESightWeaveLabCamera>(Index))),
			FString(ExpectedLabels[Index]));
	}
	TestTrue(TEXT("M4P1 enables its transient fixture labels"),
		SightWeave::Lab::IsFixtureEnabled(
			TEXT("SW_M4P1_LastSeenRemembered"), ESightWeaveLabMode::M4P1));
	TestFalse(TEXT("M4P1 isolates M3P5 fixtures"),
		SightWeave::Lab::IsFixtureEnabled(TEXT("SW_M3P5_LiveNow"), ESightWeaveLabMode::M4P1));
	TestFalse(TEXT("M3P5 isolates M4P1 fixtures"),
		SightWeave::Lab::IsFixtureEnabled(
			TEXT("SW_M4P1_LastSeenRemembered"), ESightWeaveLabMode::M3P5));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P1VisualLabTest,
	"SightWeave.M4P1.Visual.LastSeenLab",
	SightWeave::M4P1::VisualLabTests::VisualFlags)

bool FSightWeaveM4P1VisualLabTest::RunTest(const FString& Parameters)
{
	using namespace SightWeave::M4P1::VisualLabTests;
	if (GUsingNullRHI)
	{
		AddError(TEXT("M4P1 visual Lab requires a real RHI."));
		return true;
	}
	AddExpectedError(TEXT("Presentation composite state=fail-binding"),
		EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(TEXT("Presentation composite state=submitted-feather"),
		EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(
		TEXT("Unable to find RecastNavMesh instance while trying to create UCrowdManager instance"),
		EAutomationExpectedErrorFlags::Contains, -1);
	AddExpectedError(TEXT("Console variable 'r.MotionVectorSimulation' used in the render thread"),
		EAutomationExpectedErrorFlags::Contains, -1);

	const FString ScreenshotDirectory = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots/M4P1")));
	auto AddCapture = [this, &ScreenshotDirectory](
		const int32 Camera,
		const int32 State,
		const TCHAR* Filename,
		const EExpectation Expectation)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(FString::Printf(
			TEXT("SightWeave.Lab.M4P1.State %d"), State)));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.75f));
		ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(FString::Printf(
			TEXT("SightWeave.Lab.Camera %d"), Camera)));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.75f));
		const FString Screenshot = FPaths::Combine(
			ScreenshotDirectory, FString(Filename) + TEXT(".png"));
		ADD_LATENT_AUTOMATION_COMMAND(FSightWeaveM4P1TakeScreenshotCommand(Screenshot));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
		ADD_LATENT_AUTOMATION_COMMAND(
			FSightWeaveM4P1ValidateScreenshotCommand(this, Screenshot, Expectation));
	};

	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/SightWeave/Maps/L_SightWeave_Lab")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("SightWeave.Lab.Mode 4")));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("SightWeave.Lab.M4P1.State 1")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(6.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("setres 1920x1080")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));

	AddCapture(0, 0, TEXT("M4P1_Camera0_Overview"), EExpectation::Overview);
	AddCapture(1, 1, TEXT("M4P1_Camera1_LastSeen"), EExpectation::Proxy);
	AddCapture(1, 2, TEXT("M4P1_Camera1_Reacquired"), EExpectation::Live);
	AddCapture(2, 1, TEXT("M4P1_Camera2_NoDynamicLeak"), EExpectation::StrictBlack);
	AddCapture(3, 1, TEXT("M4P1_Camera3_PageBoundary"), EExpectation::Boundary);
	AddCapture(3, 3, TEXT("M4P1_Camera3_ClearSuppression"), EExpectation::Boundary);
	AddCapture(4, 1, TEXT("M4P1_Camera4_Yaw45"), EExpectation::YawIsolation);
	AddCapture(4, 5, TEXT("M4P1_Camera4_IdentityReuse"), EExpectation::YawIsolation);
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
