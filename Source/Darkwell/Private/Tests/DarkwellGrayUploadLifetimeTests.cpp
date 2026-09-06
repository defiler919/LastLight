#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Texture2D.h"
#include "HAL/PlatformMemory.h"
#include "EngineGlobals.h"
#include "Misc/AutomationTest.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "UObject/StrongObjectPtr.h"
#include <atomic>

class FGrayUploadAfterEngineFrames final : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    uint64 StartFrame;
public:
    explicit FGrayUploadAfterEngineFrames(FAutomationTestBase* InTest)
        : Test(InTest), StartFrame(GFrameCounter) {}
    bool Update() override
    {
        if (GFrameCounter - StartFrame < 12) return false;
        const FPlatformMemoryStats Memory = FPlatformMemory::GetStats();
        Test->AddInfo(FString::Printf(TEXT("GRAY_UPLOAD_AFTER_ENGINE_FRAMES elapsed_frames=%llu working_set=%llu used_virtual=%llu"),
            GFrameCounter - StartFrame, uint64(Memory.UsedPhysical), uint64(Memory.UsedVirtual)));
        return true;
    }
};

// Isolate the deferred upload cleanup used by gray presentation. This is a
// bounded diagnostic, not a frame benchmark or a replacement for the soak.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkwellGrayUploadLifetimeProbe,
    "Darkwell.Stabilization.Diagnostics.UploadCleanup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkwellGrayUploadLifetimeProbe::RunTest(const FString&)
{
    constexpr uint32 Width = 256, Height = 144, PixelBytes = 8;
    constexpr uint32 BufferBytes = Width * Height * PixelBytes;
    constexpr int32 UploadCount = 2048;
    TStrongObjectPtr<UTexture2D> Texture(UTexture2D::CreateTransient(Width, Height, PF_FloatRGBA));
    if (!TestNotNull(TEXT("Transient non-streaming texture"), Texture.Get())) return false;
    Texture->NeverStream = true;
    Texture->UpdateResource();
    FlushRenderingCommands();
    if (GUsingNullRHI)
    {
        // NullRHI suppresses texture resource creation entirely. This control
        // distinguishes a real NullRHI run from a malformed command-line flag.
        TestNull(TEXT("NullRHI has no presentation texture resource"), Texture->GetResource());
        AddInfo(TEXT("GRAY_UPLOAD_LIFETIME null_rhi=1 texture_resource=0 uploads=0"));
        return true;
    }
    if (!TestNotNull(TEXT("Initialized texture resource"), Texture->GetResource())) return false;

    struct FCounters
    {
        std::atomic<int64> Pending{0};
        std::atomic<int32> Completed{0};
    };
    for (bool bPeriodicFlush : {false, true})
    {
        const auto Counters = MakeShared<FCounters, ESPMode::ThreadSafe>();
        int64 PeakPending = 0;
        auto Report = [&](const TCHAR* Phase)
        {
            const FPlatformMemoryStats Memory = FPlatformMemory::GetStats();
            AddInfo(FString::Printf(TEXT("GRAY_UPLOAD_LIFETIME null_rhi=%d periodic_flush=%d phase=%s uploads=%d buffer_bytes=%u pending_bytes=%lld completed=%d peak_pending_bytes=%lld working_set=%llu used_virtual=%llu"),
                GUsingNullRHI, bPeriodicFlush, Phase, UploadCount, BufferBytes,
                Counters->Pending.load(), Counters->Completed.load(), PeakPending,
                uint64(Memory.UsedPhysical), uint64(Memory.UsedVirtual)));
        };
        Report(TEXT("before"));
        for (int32 Index = 0; Index < UploadCount; ++Index)
        {
            uint8* Data = new uint8[BufferBytes];
            FMemory::Memzero(Data, BufferBytes);
            auto* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Width, Height);
            const int64 Pending = Counters->Pending.fetch_add(BufferBytes) + BufferBytes;
            PeakPending = FMath::Max(PeakPending, Pending);
            Texture->UpdateTextureRegions(0, 1, Region, Width * PixelBytes, PixelBytes, Data,
                [Counters](uint8* Pixels, const FUpdateTextureRegion2D* Update)
                {
                    delete[] Pixels;
                    delete Update;
                    Counters->Pending.fetch_sub(BufferBytes);
                    Counters->Completed.fetch_add(1);
                });
            // Explicit experimental intervention only, never runtime policy.
            if (bPeriodicFlush && (Index + 1) % 32 == 0) FlushRenderingCommands();
        }
        Report(TEXT("queued"));
        FlushRenderingCommands();
        Report(TEXT("flushed"));
        TestEqual(TEXT("Every upload buffer cleanup completes"), Counters->Completed.load(), UploadCount);
        TestEqual(TEXT("No upload buffers remain after drain"), Counters->Pending.load(), int64(0));
    }
    ADD_LATENT_AUTOMATION_COMMAND(FGrayUploadAfterEngineFrames(this));
    return true;
}
#endif
