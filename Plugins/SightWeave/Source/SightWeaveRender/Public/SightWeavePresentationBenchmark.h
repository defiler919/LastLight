#pragma once

#include "CoreMinimal.h"
#include "SightWeavePresentation.h"
#include "SightWeavePresentationTestReadback.h"

#if WITH_DEV_AUTOMATION_TESTS

struct SIGHTWEAVERENDER_API FSightWeavePresentationBenchmarkResult
{
	ESightWeavePresentationReadbackStatus Status =
		ESightWeavePresentationReadbackStatus::Pending;
	FString Failure;
	FIntPoint OutputExtent = FIntPoint::ZeroValue;
	double GameThreadSubmitMicroseconds = 0.0;
	double RenderThreadBindingSubmitMicroseconds = 0.0;
	double ColdRenderThreadSetupMicroseconds = 0.0;
	double ColdGPUTotalMicroseconds = 0.0;
	TArray<double> WarmRenderThreadViewSetupMicroseconds;
	TArray<double> WarmRenderThreadCompositeSetupMicroseconds;
	TArray<double> WarmGPUCompositeMicroseconds;
	uint64 InitialPageTableUploadCount = 0;
	uint64 FinalPageTableUploadCount = 0;
	uint64 InitialPageAllocationCount = 0;
	uint64 FinalPageAllocationCount = 0;
	uint64 InitialScratchAllocationCount = 0;
	uint64 FinalScratchAllocationCount = 0;
	uint64 InitialResourceGeneration = 0;
	uint64 FinalResourceGeneration = 0;
	uint64 InitialFeatherTileDispatchCount = 0;
	uint64 FinalFeatherTileDispatchCount = 0;
	uint64 FeatherPageAllocationCount = 0;
	uint64 FeatherScratchAllocationCount = 0;
	uint64 FeatherResourceGeneration = 0;
	int32 ResidentTileCount = 0;
	int32 AllocatedPageCount = 0;
	int32 AllocatedFeatherPageCount = 0;
	uint64 PersistentGPUBytes = 0;
	uint64 TransientOutputBytes = 0;
};

/** Test-only full-frame hard-mask timing harness; never compiled into Shipping. */
class SIGHTWEAVERENDER_API FSightWeavePresentationBenchmark final
	: public TSharedFromThis<FSightWeavePresentationBenchmark, ESPMode::ThreadSafe>
{
public:
	static TSharedRef<FSightWeavePresentationBenchmark, ESPMode::ThreadSafe> Start(
		TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet,
		FSightWeaveViewPresentationSelection Selection,
		FIntPoint OutputExtent,
		FVector2f TestWorldMin,
		FVector2f TestWorldStep,
		int32 WarmSampleCount = 32);

	void Poll();
	bool TryTakeResult(FSightWeavePresentationBenchmarkResult& OutResult);
	bool IsFinished() const;

private:
	struct FState;
	explicit FSightWeavePresentationBenchmark(TSharedRef<FState, ESPMode::ThreadSafe> InState);
	TSharedRef<FState, ESPMode::ThreadSafe> State;
};

#endif
