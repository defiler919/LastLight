#pragma once

#include "CoreMinimal.h"
#include "SightWeavePresentation.h"

#if WITH_DEV_AUTOMATION_TESTS

enum class ESightWeavePresentationReadbackStatus : uint8
{
	Pending,
	Complete,
	Failed
};

struct SIGHTWEAVERENDER_API FSightWeavePresentationReadbackResult
{
	ESightWeavePresentationReadbackStatus Status =
		ESightWeavePresentationReadbackStatus::Pending;
	FString Failure;
	TArray<FColor> Pixels;
	uint64 InitialPageTableUploadCount = 0;
	uint64 FinalPageTableUploadCount = 0;
	uint64 FeatherPageAllocationCount = 0;
	uint64 FeatherScratchAllocationCount = 0;
	uint64 FeatherTileDispatchCount = 0;
	uint64 FeatherResourceGeneration = 0;
	double RenderThreadCompositeSetupMicroseconds = 0.0;
	double ReadbackEndToEndMicroseconds = 0.0;
	bool bGPUTimestampAvailable = false;
	double GPUCompositeMicroseconds = 0.0;
};

/** Test-only deterministic scene-color composite followed by asynchronous GPU readback. */
class SIGHTWEAVERENDER_API FSightWeavePresentationTestReadback final
	: public TSharedFromThis<FSightWeavePresentationTestReadback, ESPMode::ThreadSafe>
{
public:
	static TSharedRef<FSightWeavePresentationTestReadback, ESPMode::ThreadSafe> Start(
		TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> Packet,
		FSightWeaveViewPresentationSelection Selection,
		TArray<FVector2f> TranslatedWorldPositions,
		TArray<FVector4f> SceneColors,
		int32 RepeatedCameraOnlyViewSetups = 8);

	void Poll();
	bool TryTakeResult(FSightWeavePresentationReadbackResult& OutResult);
	bool IsFinished() const;

private:
	struct FState;
	explicit FSightWeavePresentationTestReadback(TSharedRef<FState, ESPMode::ThreadSafe> InState);

	TSharedRef<FState, ESPMode::ThreadSafe> State;
};

#endif
