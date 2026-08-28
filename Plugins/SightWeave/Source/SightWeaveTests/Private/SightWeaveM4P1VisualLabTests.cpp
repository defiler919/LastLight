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

	struct FFrameMetrics
	{
		FIntPoint Size = FIntPoint::ZeroValue;
		FIntRect NonBlackBounds = FIntRect(0, 0, 0, 0);
		FIntRect ProxyBounds = FIntRect(0, 0, 0, 0);
		uint64 NonBlackCount = 0;
		uint64 ExactProxyCount = 0;
		double ProxyCentroidX = 0.0;
		double ProxyCentroidY = 0.0;
	};

	struct FRoiMetrics
	{
		uint64 NonBlackCount = 0;
		uint64 ExactProxyCount = 0;
	};

	struct FRoiDifference
	{
		uint64 ChangedPixelCount = 0;
		uint64 AbsoluteRgbError = 0;
	};

	bool LoadBgraImage(
		FAutomationTestBase& Test,
		const FString& Filename,
		FImage& OutImage)
	{
		FImage SourceImage;
		if (!FImageUtils::LoadImage(*Filename, SourceImage)
			|| SourceImage.SizeX <= 0
			|| SourceImage.SizeY <= 0)
		{
			Test.AddError(FString::Printf(TEXT("M4P1 ROI frame unreadable: %s"), *Filename));
			return false;
		}
		SourceImage.CopyTo(OutImage, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		return true;
	}

	FIntRect MakePercentRoi(
		const FImage& Image,
		const int32 MinXPct,
		const int32 MinYPct,
		const int32 MaxXPct,
		const int32 MaxYPct)
	{
		return FIntRect(
			Image.SizeX * MinXPct / 100,
			Image.SizeY * MinYPct / 100,
			Image.SizeX * MaxXPct / 100,
			Image.SizeY * MaxYPct / 100);
	}

	FRoiMetrics MeasureRoi(FImage& Image, const FIntRect& Roi)
	{
		FRoiMetrics Metrics;
		const TArrayView64<FColor> Pixels = Image.AsBGRA8();
		for (int32 Y = Roi.Min.Y; Y < Roi.Max.Y; ++Y)
		{
			for (int32 X = Roi.Min.X; X < Roi.Max.X; ++X)
			{
				const FColor Pixel = Pixels[static_cast<int64>(Y) * Image.SizeX + X];
				Metrics.NonBlackCount += Pixel.R != 0 || Pixel.G != 0 || Pixel.B != 0 ? 1 : 0;
				Metrics.ExactProxyCount +=
					Pixel.R == 117 && Pixel.G == 117 && Pixel.B == 117 ? 1 : 0;
			}
		}
		return Metrics;
	}

	FRoiDifference CompareRoi(FImage& A, FImage& B, const FIntRect& Roi)
	{
		FRoiDifference Difference;
		if (A.SizeX != B.SizeX || A.SizeY != B.SizeY)
		{
			Difference.ChangedPixelCount = MAX_uint64;
			Difference.AbsoluteRgbError = MAX_uint64;
			return Difference;
		}
		const TArrayView64<FColor> PixelsA = A.AsBGRA8();
		const TArrayView64<FColor> PixelsB = B.AsBGRA8();
		for (int32 Y = Roi.Min.Y; Y < Roi.Max.Y; ++Y)
		{
			for (int32 X = Roi.Min.X; X < Roi.Max.X; ++X)
			{
				const int64 Index = static_cast<int64>(Y) * A.SizeX + X;
				const FColor PixelA = PixelsA[Index];
				const FColor PixelB = PixelsB[Index];
				Difference.ChangedPixelCount +=
					PixelA.R != PixelB.R || PixelA.G != PixelB.G || PixelA.B != PixelB.B ? 1 : 0;
				Difference.AbsoluteRgbError += FMath::Abs(int32(PixelA.R) - int32(PixelB.R));
				Difference.AbsoluteRgbError += FMath::Abs(int32(PixelA.G) - int32(PixelB.G));
				Difference.AbsoluteRgbError += FMath::Abs(int32(PixelA.B) - int32(PixelB.B));
			}
		}
		return Difference;
	}

	bool LoadFrameMetrics(
		FAutomationTestBase& Test,
		const FString& Filename,
		FFrameMetrics& OutMetrics)
	{
		FImage SourceImage;
		if (!FImageUtils::LoadImage(*Filename, SourceImage)
			|| SourceImage.SizeX <= 0
			|| SourceImage.SizeY <= 0)
		{
			Test.AddError(FString::Printf(TEXT("M4P1 sequence frame unreadable: %s"),
				*Filename));
			return false;
		}
		FImage BGRAImage;
		SourceImage.CopyTo(BGRAImage, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		OutMetrics = FFrameMetrics();
		OutMetrics.Size = FIntPoint(SourceImage.SizeX, SourceImage.SizeY);
		FIntPoint NonBlackMin(SourceImage.SizeX, SourceImage.SizeY);
		FIntPoint NonBlackMax(-1, -1);
		FIntPoint ProxyMin(SourceImage.SizeX, SourceImage.SizeY);
		FIntPoint ProxyMax(-1, -1);
		double ProxyXSum = 0.0;
		double ProxyYSum = 0.0;
		const TArrayView64<FColor> Pixels = BGRAImage.AsBGRA8();
		for (int32 Y = 0; Y < SourceImage.SizeY; ++Y)
		{
			for (int32 X = 0; X < SourceImage.SizeX; ++X)
			{
				const FColor Pixel = Pixels[static_cast<int64>(Y) * SourceImage.SizeX + X];
				const bool bNonBlack = Pixel.R != 0 || Pixel.G != 0 || Pixel.B != 0;
				const bool bExactProxy = Pixel.R == 117 && Pixel.G == 117 && Pixel.B == 117;
				if (bNonBlack)
				{
					++OutMetrics.NonBlackCount;
					NonBlackMin.X = FMath::Min(NonBlackMin.X, X);
					NonBlackMin.Y = FMath::Min(NonBlackMin.Y, Y);
					NonBlackMax.X = FMath::Max(NonBlackMax.X, X);
					NonBlackMax.Y = FMath::Max(NonBlackMax.Y, Y);
				}
				if (bExactProxy)
				{
					++OutMetrics.ExactProxyCount;
					ProxyMin.X = FMath::Min(ProxyMin.X, X);
					ProxyMin.Y = FMath::Min(ProxyMin.Y, Y);
					ProxyMax.X = FMath::Max(ProxyMax.X, X);
					ProxyMax.Y = FMath::Max(ProxyMax.Y, Y);
					ProxyXSum += X;
					ProxyYSum += Y;
				}
			}
		}
		if (NonBlackMax.X >= 0)
		{
			OutMetrics.NonBlackBounds = FIntRect(NonBlackMin, NonBlackMax + FIntPoint(1, 1));
		}
		if (ProxyMax.X >= 0)
		{
			OutMetrics.ProxyBounds = FIntRect(ProxyMin, ProxyMax + FIntPoint(1, 1));
			OutMetrics.ProxyCentroidX = ProxyXSum / OutMetrics.ExactProxyCount;
			OutMetrics.ProxyCentroidY = ProxyYSum / OutMetrics.ExactProxyCount;
		}
		return true;
	}

	FString SequenceFrameFilename(
		const FString& Directory,
		const FString& Prefix,
		const int32 FrameIndex)
	{
		return FPaths::Combine(
			Directory,
			FString::Printf(TEXT("%s_%03d.png"), *Prefix, FrameIndex));
	}
}

class FSightWeaveM4P1CaptureSequenceCommand final : public IAutomationLatentCommand
{
public:
	FSightWeaveM4P1CaptureSequenceCommand(
		FAutomationTestBase* InTest,
		FString InDirectory,
		FString InPrefix,
		const int32 InFrameCount)
		: Test(InTest)
		, Directory(MoveTemp(InDirectory))
		, Prefix(MoveTemp(InPrefix))
		, FrameCount(InFrameCount)
	{
	}

	virtual bool Update() override
	{
		if (!Test || FrameIndex >= FrameCount)
		{
			return true;
		}
		if (++UpdateCount > FrameCount * 30)
		{
			Test->AddError(FString::Printf(
				TEXT("M4P1 sequence capture timed out prefix=%s frame=%d/%d"),
				*Prefix, FrameIndex, FrameCount));
			return true;
		}
		const FString Filename = SightWeave::M4P1::VisualLabTests::SequenceFrameFilename(
			Directory, Prefix, FrameIndex);
		if (!bScreenshotRequested)
		{
			IFileManager::Get().MakeDirectory(*Directory, true);
			IFileManager::Get().Delete(*Filename, false, true);
			FScreenshotRequest::RequestScreenshot(
				Filename, false, false, false, FIntRect(), true);
			bScreenshotRequested = true;
			return false;
		}
		if (!IFileManager::Get().FileExists(*Filename))
		{
			return false;
		}
		++FrameIndex;
		bScreenshotRequested = false;
		return FrameIndex >= FrameCount;
	}

private:
	FAutomationTestBase* Test = nullptr;
	FString Directory;
	FString Prefix;
	int32 FrameCount = 0;
	int32 FrameIndex = 0;
	int32 UpdateCount = 0;
	bool bScreenshotRequested = false;
};

class FSightWeaveM4P1ValidateRememberedSequenceCommand final : public IAutomationLatentCommand
{
public:
	FSightWeaveM4P1ValidateRememberedSequenceCommand(
		FAutomationTestBase* InTest,
		FString InDirectory,
		FString InPrefix,
		const int32 InFrameCount)
		: Test(InTest)
		, Directory(MoveTemp(InDirectory))
		, Prefix(MoveTemp(InPrefix))
		, FrameCount(InFrameCount)
	{
	}

	virtual bool Update() override
	{
		using namespace SightWeave::M4P1::VisualLabTests;
		FFrameMetrics Reference;
		uint64 MinimumCount = MAX_uint64;
		uint64 MaximumCount = 0;
		double MaximumCentroidDelta = 0.0;
		bool bAllLoaded = true;
		for (int32 FrameIndex = 0; FrameIndex < FrameCount; ++FrameIndex)
		{
			FFrameMetrics Metrics;
			const FString Filename = SequenceFrameFilename(Directory, Prefix, FrameIndex);
			if (!LoadFrameMetrics(*Test, Filename, Metrics))
			{
				bAllLoaded = false;
				continue;
			}
			Test->TestTrue(*FString::Printf(TEXT("Remembered frame %d contains proxy"), FrameIndex),
				Metrics.ExactProxyCount >= 256);
			Test->TestEqual(*FString::Printf(TEXT("Remembered frame %d color is deterministic"), FrameIndex),
				Metrics.NonBlackCount, Metrics.ExactProxyCount);
			if (FrameIndex == 0)
			{
				Reference = Metrics;
			}
			else
			{
				Test->TestEqual(*FString::Printf(TEXT("Remembered frame %d proxy bounds stable"), FrameIndex),
					Metrics.ProxyBounds, Reference.ProxyBounds);
				MaximumCentroidDelta = FMath::Max(MaximumCentroidDelta,
					FMath::Abs(Metrics.ProxyCentroidX - Reference.ProxyCentroidX));
				MaximumCentroidDelta = FMath::Max(MaximumCentroidDelta,
					FMath::Abs(Metrics.ProxyCentroidY - Reference.ProxyCentroidY));
			}
			MinimumCount = FMath::Min(MinimumCount, Metrics.ExactProxyCount);
			MaximumCount = FMath::Max(MaximumCount, Metrics.ExactProxyCount);
		}
		if (bAllLoaded)
		{
			Test->TestEqual(TEXT("Remembered 120-frame proxy pixel count stable"),
				MaximumCount, MinimumCount);
			Test->TestTrue(TEXT("Remembered 120-frame proxy centroid stable"),
				MaximumCentroidDelta <= 0.01);
			Test->AddInfo(FString::Printf(
				TEXT("M4P1_CONTINUOUS_REMEMBERED frames=%d count_min=%llu count_max=%llu bounds=(%d,%d)-(%d,%d) centroid_max_delta=%.6f"),
				FrameCount, MinimumCount, MaximumCount,
				Reference.ProxyBounds.Min.X, Reference.ProxyBounds.Min.Y,
				Reference.ProxyBounds.Max.X, Reference.ProxyBounds.Max.Y,
				MaximumCentroidDelta));
		}
		return true;
	}

private:
	FAutomationTestBase* Test = nullptr;
	FString Directory;
	FString Prefix;
	int32 FrameCount = 0;
};

class FSightWeaveM4P1ValidateReacquireSequenceCommand final : public IAutomationLatentCommand
{
public:
	FSightWeaveM4P1ValidateReacquireSequenceCommand(
		FAutomationTestBase* InTest,
		FString InDirectory,
		FString InPrefix,
		const int32 InFrameCount)
		: Test(InTest)
		, Directory(MoveTemp(InDirectory))
		, Prefix(MoveTemp(InPrefix))
		, FrameCount(InFrameCount)
	{
	}

	virtual bool Update() override
	{
		using namespace SightWeave::M4P1::VisualLabTests;
		TArray<FFrameMetrics> Frames;
		for (int32 FrameIndex = 0; FrameIndex < FrameCount; ++FrameIndex)
		{
			FFrameMetrics& Metrics = Frames.AddDefaulted_GetRef();
			if (!LoadFrameMetrics(*Test,
				SequenceFrameFilename(Directory, Prefix, FrameIndex), Metrics))
			{
				return true;
			}
		}
		const FFrameMetrics& LiveReference = Frames.Last();
		Test->TestTrue(TEXT("Reacquire sequence reaches stable live presentation"),
			LiveReference.NonBlackCount >= 256 && LiveReference.ExactProxyCount == 0);
		bool bLiveSeen = false;
		int32 ProxyOnlyFrames = 0;
		int32 BlackHandoffFrames = 0;
		int32 LiveFrames = 0;
		uint64 MaximumLiveProxyColorCollision = 0;
		for (int32 FrameIndex = 0; FrameIndex < Frames.Num(); ++FrameIndex)
		{
			const FFrameMetrics& Metrics = Frames[FrameIndex];
			const bool bProxy = Metrics.ExactProxyCount >= 256;
			const bool bLive = Metrics.NonBlackCount > Metrics.ExactProxyCount + 256;
			Test->TestFalse(*FString::Printf(TEXT("Reacquire frame %d has no live/proxy double display"), FrameIndex),
				bProxy && bLive);
			if (bProxy)
			{
				++ProxyOnlyFrames;
				Test->TestFalse(*FString::Printf(TEXT("Reacquire frame %d rejects stale proxy after live begins"), FrameIndex),
					bLiveSeen);
				Test->TestEqual(*FString::Printf(TEXT("Reacquire frame %d proxy-only color"), FrameIndex),
					Metrics.NonBlackCount, Metrics.ExactProxyCount);
			}
			else if (Metrics.NonBlackCount == 0)
			{
				++BlackHandoffFrames;
			}
			else
			{
				bLiveSeen = true;
				++LiveFrames;
				MaximumLiveProxyColorCollision = FMath::Max(
					MaximumLiveProxyColorCollision, Metrics.ExactProxyCount);
				Test->TestTrue(*FString::Printf(TEXT("Reacquire frame %d has no proxy-sized residue"), FrameIndex),
					Metrics.ExactProxyCount < 256);
				const FIntRect& Bounds = Metrics.NonBlackBounds;
				const FIntRect& ReferenceBounds = LiveReference.NonBlackBounds;
				const int32 MaximumBoundsDelta = FMath::Max(
					FMath::Max(
						FMath::Abs(Bounds.Min.X - ReferenceBounds.Min.X),
						FMath::Abs(Bounds.Min.Y - ReferenceBounds.Min.Y)),
					FMath::Max(
						FMath::Abs(Bounds.Max.X - ReferenceBounds.Max.X),
						FMath::Abs(Bounds.Max.Y - ReferenceBounds.Max.Y)));
				Test->TestTrue(*FString::Printf(TEXT("Reacquire frame %d has no halo bounds expansion"), FrameIndex),
					MaximumBoundsDelta <= 2);
				Test->TestTrue(*FString::Printf(TEXT("Reacquire frame %d has no halo pixel spike"), FrameIndex),
					Metrics.NonBlackCount <= LiveReference.NonBlackCount * 110 / 100);
			}
		}
		Test->TestTrue(TEXT("Reacquire sequence contains live frames"), bLiveSeen);
		Test->AddInfo(FString::Printf(
			TEXT("M4P1_CONTINUOUS_REACQUIRE frames=%d proxy_only=%d black_handoff=%d live=%d max_proxy_color_collision=%llu final_nonblack=%llu final_bounds=(%d,%d)-(%d,%d)"),
			FrameCount, ProxyOnlyFrames, BlackHandoffFrames, LiveFrames,
			MaximumLiveProxyColorCollision,
			LiveReference.NonBlackCount,
			LiveReference.NonBlackBounds.Min.X, LiveReference.NonBlackBounds.Min.Y,
			LiveReference.NonBlackBounds.Max.X, LiveReference.NonBlackBounds.Max.Y));
		return true;
	}

private:
	FAutomationTestBase* Test = nullptr;
	FString Directory;
	FString Prefix;
	int32 FrameCount = 0;
};

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

class FSightWeaveM4P1ValidateCamera34RoiCommand final : public IAutomationLatentCommand
{
public:
	FSightWeaveM4P1ValidateCamera34RoiCommand(
		FAutomationTestBase* InTest,
		FString InDirectory)
		: Test(InTest)
		, Directory(MoveTemp(InDirectory))
	{
	}

	virtual bool Update() override
	{
		using namespace SightWeave::M4P1::VisualLabTests;
		FImage Camera3State1;
		FImage Camera3State3;
		FImage Camera3Restored;
		FImage Camera3State4;
		FImage Camera4State1;
		FImage Camera4State5;
		if (!Load(TEXT("Camera3_State1.png"), Camera3State1)
			|| !Load(TEXT("Camera3_State3.png"), Camera3State3)
			|| !Load(TEXT("Camera3_Restored.png"), Camera3Restored)
			|| !Load(TEXT("Camera3_State4.png"), Camera3State4)
			|| !Load(TEXT("Camera4_State1.png"), Camera4State1)
			|| !Load(TEXT("Camera4_State5.png"), Camera4State5))
		{
			return true;
		}

		Test->TestEqual(TEXT("Camera 3 ROI captures have equal widths"),
			Camera3State3.SizeX, Camera3State1.SizeX);
		Test->TestEqual(TEXT("Camera 3 ROI captures have equal heights"),
			Camera3State3.SizeY, Camera3State1.SizeY);
		Test->TestEqual(TEXT("Camera 4 ROI captures have equal widths"),
			Camera4State5.SizeX, Camera4State1.SizeX);
		Test->TestEqual(TEXT("Camera 4 ROI captures have equal heights"),
			Camera4State5.SizeY, Camera4State1.SizeY);

		const FIntRect Camera3ControlRoi = MakePercentRoi(Camera3State1, 38, 24, 63, 76);
		const FIntRect Camera3TransitionRoi = MakePercentRoi(Camera3State1, 70, 24, 97, 76);
		const FIntRect Camera4YawRoi = MakePercentRoi(Camera4State1, 39, 20, 66, 80);
		const FIntRect Camera4OldGenerationRoi = MakePercentRoi(Camera4State1, 8, 20, 36, 80);

		const FRoiMetrics C3Control1 = MeasureRoi(Camera3State1, Camera3ControlRoi);
		const FRoiMetrics C3Control3 = MeasureRoi(Camera3State3, Camera3ControlRoi);
		const FRoiMetrics C3ControlRestored = MeasureRoi(Camera3Restored, Camera3ControlRoi);
		const FRoiMetrics C3Control4 = MeasureRoi(Camera3State4, Camera3ControlRoi);
		const FRoiMetrics C3Target1 = MeasureRoi(Camera3State1, Camera3TransitionRoi);
		const FRoiMetrics C3Target3 = MeasureRoi(Camera3State3, Camera3TransitionRoi);
		const FRoiMetrics C3TargetRestored = MeasureRoi(Camera3Restored, Camera3TransitionRoi);
		const FRoiMetrics C3Target4 = MeasureRoi(Camera3State4, Camera3TransitionRoi);
		const FRoiDifference C3Control13 = CompareRoi(
			Camera3State1, Camera3State3, Camera3ControlRoi);
		const FRoiDifference C3Control1Restored = CompareRoi(
			Camera3State1, Camera3Restored, Camera3ControlRoi);
		const FRoiDifference C3Control14 = CompareRoi(
			Camera3State1, Camera3State4, Camera3ControlRoi);
		const FRoiDifference C3Target13 = CompareRoi(
			Camera3State1, Camera3State3, Camera3TransitionRoi);
		const FRoiDifference C3TargetRestored3 = CompareRoi(
			Camera3Restored, Camera3State3, Camera3TransitionRoi);
		const FRoiDifference C3TargetRestored4 = CompareRoi(
			Camera3Restored, Camera3State4, Camera3TransitionRoi);

		Test->TestTrue(TEXT("Camera 3 page-boundary control ROI is visible in State 1"),
			C3Control1.ExactProxyCount >= 64);
		Test->TestEqual(TEXT("Camera 3 page-boundary control remains pixel-stable in State 3"),
			C3Control13.ChangedPixelCount, uint64(0));
		Test->TestEqual(TEXT("Camera 3 page-boundary control remains pixel-stable after restore"),
			C3Control1Restored.ChangedPixelCount, uint64(0));
		Test->TestEqual(TEXT("Camera 3 page-boundary control remains pixel-stable in State 4"),
			C3Control14.ChangedPixelCount, uint64(0));
		Test->TestEqual(TEXT("Camera 3 control exact color count remains stable in State 3"),
			C3Control3.ExactProxyCount, C3Control1.ExactProxyCount);
		Test->TestEqual(TEXT("Camera 3 control exact color count remains stable after restore"),
			C3ControlRestored.ExactProxyCount, C3Control1.ExactProxyCount);
		Test->TestEqual(TEXT("Camera 3 control exact color count remains stable in State 4"),
			C3Control4.ExactProxyCount, C3Control1.ExactProxyCount);
		Test->TestTrue(TEXT("Camera 3 transition ROI is visible in State 1"),
			C3Target1.ExactProxyCount >= 64);
		Test->TestEqual(TEXT("Camera 3 transition ROI is strict black in State 3"),
			C3Target3.NonBlackCount, uint64(0));
		Test->TestTrue(TEXT("Camera 3 transition ROI restores at its original position"),
			C3TargetRestored.ExactProxyCount >= 64);
		Test->TestEqual(TEXT("Camera 3 restored transition ROI matches State 1"),
			C3TargetRestored.ExactProxyCount, C3Target1.ExactProxyCount);
		Test->TestEqual(TEXT("Camera 3 transition ROI is strict black after clear"),
			C3Target4.NonBlackCount, uint64(0));
		Test->TestTrue(TEXT("Camera 3 State 1 to State 3 target ROI has nonzero AE difference"),
			C3Target13.ChangedPixelCount >= 64 && C3Target13.AbsoluteRgbError > 0);
		Test->TestTrue(TEXT("Camera 3 State 3 to restored State 1 target ROI has nonzero AE difference"),
			C3TargetRestored3.ChangedPixelCount >= 64 && C3TargetRestored3.AbsoluteRgbError > 0);
		Test->TestTrue(TEXT("Camera 3 restored State 1 to State 4 target ROI has nonzero AE difference"),
			C3TargetRestored4.ChangedPixelCount >= 64 && C3TargetRestored4.AbsoluteRgbError > 0);

		const FRoiMetrics C4Yaw1 = MeasureRoi(Camera4State1, Camera4YawRoi);
		const FRoiMetrics C4Yaw5 = MeasureRoi(Camera4State5, Camera4YawRoi);
		const FRoiMetrics C4Old1 = MeasureRoi(Camera4State1, Camera4OldGenerationRoi);
		const FRoiMetrics C4Old5 = MeasureRoi(Camera4State5, Camera4OldGenerationRoi);
		const FRoiDifference C4Yaw15 = CompareRoi(Camera4State1, Camera4State5, Camera4YawRoi);
		const FRoiDifference C4Old15 = CompareRoi(
			Camera4State1, Camera4State5, Camera4OldGenerationRoi);
		Test->TestTrue(TEXT("Camera 4 yaw45 control ROI is visible in State 1"),
			C4Yaw1.ExactProxyCount >= 64);
		Test->TestEqual(TEXT("Camera 4 yaw45 control remains pixel-stable in State 5"),
			C4Yaw15.ChangedPixelCount, uint64(0));
		Test->TestEqual(TEXT("Camera 4 yaw45 exact color count remains stable in State 5"),
			C4Yaw5.ExactProxyCount, C4Yaw1.ExactProxyCount);
		Test->TestTrue(TEXT("Camera 4 old-generation ROI is visible in State 1"),
			C4Old1.ExactProxyCount >= 64);
		Test->TestEqual(TEXT("Camera 4 old-generation ROI is strict black in State 5"),
			C4Old5.NonBlackCount, uint64(0));
		Test->TestTrue(TEXT("Camera 4 State 1 to State 5 target ROI has nonzero AE difference"),
			C4Old15.ChangedPixelCount >= 64 && C4Old15.AbsoluteRgbError > 0);

		Test->AddInfo(FString::Printf(
			TEXT("M4P1_CAMERA34_ROI camera=3 control_exact=[%llu,%llu,%llu,%llu] transition_nonblack=[%llu,%llu,%llu,%llu] transition_exact=[%llu,%llu,%llu,%llu] changed=[%llu,%llu,%llu] ae=[%llu,%llu,%llu]"),
			C3Control1.ExactProxyCount, C3Control3.ExactProxyCount,
			C3ControlRestored.ExactProxyCount, C3Control4.ExactProxyCount,
			C3Target1.NonBlackCount, C3Target3.NonBlackCount,
			C3TargetRestored.NonBlackCount, C3Target4.NonBlackCount,
			C3Target1.ExactProxyCount, C3Target3.ExactProxyCount,
			C3TargetRestored.ExactProxyCount, C3Target4.ExactProxyCount,
			C3Target13.ChangedPixelCount, C3TargetRestored3.ChangedPixelCount,
			C3TargetRestored4.ChangedPixelCount,
			C3Target13.AbsoluteRgbError, C3TargetRestored3.AbsoluteRgbError,
			C3TargetRestored4.AbsoluteRgbError));
		Test->AddInfo(FString::Printf(
			TEXT("M4P1_CAMERA34_ROI camera=4 yaw_exact=[%llu,%llu] old_generation_nonblack=[%llu,%llu] old_generation_exact=[%llu,%llu] changed=%llu ae=%llu"),
			C4Yaw1.ExactProxyCount, C4Yaw5.ExactProxyCount,
			C4Old1.NonBlackCount, C4Old5.NonBlackCount,
			C4Old1.ExactProxyCount, C4Old5.ExactProxyCount,
			C4Old15.ChangedPixelCount, C4Old15.AbsoluteRgbError));
		return true;
	}

private:
	bool Load(const TCHAR* Filename, FImage& OutImage) const
	{
		return SightWeave::M4P1::VisualLabTests::LoadBgraImage(
			*Test, FPaths::Combine(Directory, Filename), OutImage);
	}

	FAutomationTestBase* Test = nullptr;
	FString Directory;
};

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
		// Camera 0 is a distant overview and the automation viewport is host-sized.
		// Preserve the original 256-pixel gate at 1526x548 while scaling it by
		// image area for smaller offscreen viewports.
		const uint64 MinimumOverviewProxyPixels = FMath::Max<uint64>(
			64,
			TotalPixels * 256 / (static_cast<uint64>(1526) * 548));
		Test->TestTrue(TEXT("Overview contains deterministic Last-Seen proxy pixels"),
			ExactProxyCount >= MinimumOverviewProxyPixels);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P1Camera34ObservabilityTest,
	"SightWeave.M4P1.Visual.Camera34Observability",
	SightWeave::M4P1::VisualLabTests::VisualFlags)

bool FSightWeaveM4P1Camera34ObservabilityTest::RunTest(const FString& Parameters)
{
	if (GUsingNullRHI)
	{
		AddError(TEXT("M4P1 Camera 3/4 ROI validation requires a real RHI."));
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
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots/M4P1/Camera34ROI")));
	auto AddCapture = [this, &ScreenshotDirectory](
		const int32 Camera,
		const int32 State,
		const TCHAR* Filename)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(FString::Printf(
			TEXT("SightWeave.Lab.M4P1.State %d"), State)));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.75f));
		ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(FString::Printf(
			TEXT("SightWeave.Lab.Camera %d"), Camera)));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.75f));
		const FString Screenshot = FPaths::Combine(ScreenshotDirectory, Filename);
		ADD_LATENT_AUTOMATION_COMMAND(FSightWeaveM4P1TakeScreenshotCommand(Screenshot));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	};

	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/SightWeave/Maps/L_SightWeave_Lab")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("SightWeave.Lab.Mode 4")));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("SightWeave.Lab.M4P1.State 1")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(6.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("setres 1920x1080")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	AddCapture(3, 1, TEXT("Camera3_State1.png"));
	AddCapture(3, 3, TEXT("Camera3_State3.png"));
	AddCapture(3, 1, TEXT("Camera3_Restored.png"));
	AddCapture(3, 4, TEXT("Camera3_State4.png"));
	AddCapture(4, 1, TEXT("Camera4_State1.png"));
	AddCapture(4, 5, TEXT("Camera4_State5.png"));
	ADD_LATENT_AUTOMATION_COMMAND(
		FSightWeaveM4P1ValidateCamera34RoiCommand(this, ScreenshotDirectory));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSightWeaveM4P1ContinuousTransitionTest,
	"SightWeave.M4P1.Visual.ContinuousTransition",
	SightWeave::M4P1::VisualLabTests::VisualFlags)

bool FSightWeaveM4P1ContinuousTransitionTest::RunTest(const FString& Parameters)
{
	if (GUsingNullRHI)
	{
		AddError(TEXT("M4P1 continuous transition requires a real RHI."));
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

	const FString SequenceDirectory = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots/M4P1/Continuous")));
	constexpr int32 RememberedFrameCount = 120;
	constexpr int32 ReacquireFrameCount = 32;
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/SightWeave/Maps/L_SightWeave_Lab")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("SightWeave.Lab.Mode 4")));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("SightWeave.Lab.M4P1.State 0")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(6.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("setres 1920x1080")));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("SightWeave.Lab.Camera 1")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.25f));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("SightWeave.Lab.M4P1.State 1")));
	ADD_LATENT_AUTOMATION_COMMAND(FSightWeaveM4P1CaptureSequenceCommand(
		this, SequenceDirectory, TEXT("Remembered"), RememberedFrameCount));
	ADD_LATENT_AUTOMATION_COMMAND(FSightWeaveM4P1ValidateRememberedSequenceCommand(
		this, SequenceDirectory, TEXT("Remembered"), RememberedFrameCount));
	ADD_LATENT_AUTOMATION_COMMAND(FExecStringLatentCommand(TEXT("SightWeave.Lab.M4P1.State 2")));
	ADD_LATENT_AUTOMATION_COMMAND(FSightWeaveM4P1CaptureSequenceCommand(
		this, SequenceDirectory, TEXT("Reacquire"), ReacquireFrameCount));
	ADD_LATENT_AUTOMATION_COMMAND(FSightWeaveM4P1ValidateReacquireSequenceCommand(
		this, SequenceDirectory, TEXT("Reacquire"), ReacquireFrameCount));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
