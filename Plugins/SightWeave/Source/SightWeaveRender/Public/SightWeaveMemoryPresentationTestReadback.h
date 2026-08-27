#pragma once

#include "CoreMinimal.h"
#include "SightWeaveMemory.h"
#include "SightWeavePresentation.h"
#include "SightWeaveRenderWorldSubsystem.h"
#include "SightWeaveSparseAtlas.h"
#include "SightWeaveStaticEnvironment.h"

#if WITH_DEV_AUTOMATION_TESTS

struct SIGHTWEAVERENDER_API FSightWeaveMemoryPresentationReadbackResult
{
	bool bComplete = false;
	ESightWeaveRenderAvailability LiveAvailability = ESightWeaveRenderAvailability::Unknown;
	ESightWeaveRenderAvailability MemoryAvailability = ESightWeaveRenderAvailability::Unknown;
	ESightWeaveRenderAvailability StaticEnvironmentAvailability =
		ESightWeaveRenderAvailability::Unknown;
	int32 Width = 0;
	int32 RowPitchInPixels = 0;
	int32 BufferHeight = 0;
	FString Failure;
	TArray<FColor> Pixels;
};

class SIGHTWEAVERENDER_API FSightWeaveMemoryPresentationTestReadback final
	: public TSharedFromThis<FSightWeaveMemoryPresentationTestReadback, ESPMode::ThreadSafe>
{
public:
	static TSharedRef<FSightWeaveMemoryPresentationTestReadback, ESPMode::ThreadSafe> Start(
		TSharedPtr<const FSightWeaveSparseRenderPacket, ESPMode::ThreadSafe> LivePacket,
		FSightWeaveViewPresentationSelection Selection,
		TSharedPtr<const FSightWeaveMemoryPacket, ESPMode::ThreadSafe> MemoryPacket,
		TSharedPtr<const FSightWeaveStaticEnvironmentPacket, ESPMode::ThreadSafe> StaticPacket,
		TArray<FVector2f> WorldPositions,
		TArray<FVector4f> SceneColors,
		bool bForceMemoryUnavailable = false);

	void Poll();
	bool TryTakeResult(FSightWeaveMemoryPresentationReadbackResult& OutResult);
	bool IsFinished() const;

private:
	struct FState;
	explicit FSightWeaveMemoryPresentationTestReadback(
		TSharedRef<FState, ESPMode::ThreadSafe> InState);
	TSharedRef<FState, ESPMode::ThreadSafe> State;
};

#endif
