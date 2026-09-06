#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Texture2D.h"
#include "EngineGlobals.h"
#include "HAL/IConsoleManager.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformMemory.h"
#include "Misc/AutomationTest.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"
#include <atomic>

namespace Darkwell::UploadAttribution
{
// Diagnostic interventions only. No flush, trim or upload policy is added to gameplay.
class FProbe final : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TStrongObjectPtr<UTexture2D> Texture;
    struct FCounters { std::atomic<int64> Pending{0}; std::atomic<int32> Completed{0}; };
    TSharedRef<FCounters, ESPMode::ThreadSafe> Counters = MakeShared<FCounters, ESPMode::ThreadSafe>();
    uint64 StartFrame = 0;
    int32 Stage = 0;
    int64 PeakPending = 0;

    void Report(const TCHAR* Phase, bool bDump = true)
    {
        uint32 RenderFrame = 0;
        ENQUEUE_RENDER_COMMAND(GrayReadRenderFrame)([&RenderFrame](FRHICommandListImmediate&) { RenderFrame = GFrameNumberRenderThread; });
        FlushRenderingCommands();
        uint64 DefaultBytes = 0, PlatformBytes = 0;
#if ENABLE_LOW_LEVEL_MEM_TRACKER
        FLowLevelMemTracker::Get().UpdateStatsPerFrame();
        DefaultBytes = FLowLevelMemTracker::Get().GetTotalTrackedMemory(ELLMTracker::Default);
        PlatformBytes = FLowLevelMemTracker::Get().GetTotalTrackedMemory(ELLMTracker::Platform);
#endif
        const FPlatformMemoryStats Memory = FPlatformMemory::GetStats();
        TRACE_BOOKMARK(TEXT("GrayUpload_%s"), Phase);
        Test->AddInfo(FString::Printf(TEXT("GRAY_UPLOAD_ATTR phase=%s frame=%llu render_frame=%u pending=%lld completed=%d peak_pending=%lld working_set=%llu used_virtual=%llu llm_default=%llu llm_platform=%llu"),
            Phase, GFrameCounter, RenderFrame, Counters->Pending.load(), Counters->Completed.load(), PeakPending,
            uint64(Memory.UsedPhysical), uint64(Memory.UsedVirtual), DefaultBytes, PlatformBytes));
        if (bDump)
        {
            UE_LOG(LogTemp, Display, TEXT("GRAY_UPLOAD_ALLOCS_BEGIN %s"), Phase);
            const bool bHandled = IConsoleManager::Get().ProcessUserConsoleInput(TEXT("D3D12.DumpTrackedAllocations"), *GLog, nullptr);
            Test->TestTrue(TEXT("D3D12 allocation dump command available"), bHandled);
            UE_LOG(LogTemp, Display, TEXT("GRAY_UPLOAD_ALLOCS_END %s"), Phase);
        }
    }

    void Submit(bool bPeriodicFlush)
    {
        constexpr uint32 Width = 256, Height = 144, Bytes = Width * Height * 8;
        const auto Cleanup = Counters;
        PeakPending = 0;
        for (int32 I = 0; I < 2048; ++I)
        {
            LLM_SCOPE_BYNAME(TEXT("Darkwell/UploadAttribution"));
            uint8* Pixels = new uint8[Bytes];
            FMemory::Memzero(Pixels, Bytes);
            auto* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Width, Height);
            PeakPending = FMath::Max(PeakPending, Cleanup->Pending.fetch_add(Bytes) + Bytes);
            Texture->UpdateTextureRegions(0, 1, Region, Width * 8, 8, Pixels,
                [Cleanup](uint8* Data, const FUpdateTextureRegion2D* R)
                {
                    delete[] Data; delete R;
                    Cleanup->Pending.fetch_sub(Bytes);
                    Cleanup->Completed.fetch_add(1);
                });
            if (bPeriodicFlush && (I + 1) % 32 == 0) FlushRenderingCommands();
        }
        // Capture queue size before Report deliberately drains render commands.
        Test->AddInfo(FString::Printf(TEXT("GRAY_UPLOAD_ATTR_QUEUED periodic=%d pending=%lld completed=%d peak_pending=%lld"),
            bPeriodicFlush, Counters->Pending.load(), Counters->Completed.load(), PeakPending));
        TRACE_BOOKMARK(TEXT("GrayUpload_%sQueued"), bPeriodicFlush ? TEXT("Batched") : TEXT("Burst"));
        Report(bPeriodicFlush ? TEXT("BatchedFlushed") : TEXT("BurstFlushed"));
        Test->TestEqual(TEXT("Upload callbacks drained"), Counters->Pending.load(), int64(0));
    }

public:
    explicit FProbe(FAutomationTestBase* InTest) : Test(InTest) {}
    bool Update() override
    {
        if (Stage == 0)
        {
            if (!Test->TestFalse(TEXT("Resource attribution requires real RHI"), GUsingNullRHI)) return true;
            Texture.Reset(UTexture2D::CreateTransient(256, 144, PF_FloatRGBA));
            if (!Test->TestNotNull(TEXT("Probe texture"), Texture.Get())) return true;
            Texture->NeverStream = true;
            Texture->UpdateResource();
            FlushRenderingCommands();
            if (!Test->TestNotNull(TEXT("Probe texture resource"), Texture->GetResource())) return true;
            Report(TEXT("Baseline"));
            Submit(false);
            Submit(true);
            Test->TestEqual(TEXT("All callbacks completed"), Counters->Completed.load(), 4096);
            StartFrame = GFrameCounter;
            Stage = 1;
            return false;
        }
        constexpr uint64 Frames[] = {12, 120, 360, 600};
        if (Stage <= 4)
        {
            if (GFrameCounter - StartFrame < Frames[Stage - 1]) return false;
            Report(*FString::Printf(TEXT("Frame%llu"), Frames[Stage - 1]));
            if (++Stage <= 4) return false;
            ENQUEUE_RENDER_COMMAND(GrayGPUIdle)([](FRHICommandListImmediate& RHICmdList) { RHICmdList.BlockUntilGPUIdle(); });
            Report(TEXT("GPUIdle"));
            ENQUEUE_RENDER_COMMAND(GrayRenderAllocatorTrim)([](FRHICommandListImmediate&) { FMemory::Trim(true); });
            FlushRenderingCommands();
            FMemory::Trim(true);
            Report(TEXT("AllocatorTrim"));
            Texture.Reset();
            CollectGarbage(RF_NoFlags, true);
            StartFrame = GFrameCounter;
            return false;
        }
        if (GFrameCounter - StartFrame < 60) return false;
        Report(TEXT("Released"));
        return true;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellGrayUploadAttributionProbe,
    "Darkwell.Stabilization.Diagnostics.UploadResourceAttribution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDarkwellGrayUploadAttributionProbe::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(Darkwell::UploadAttribution::FProbe(this));
    return true;
}
#endif
