#include "DarkwellEditorDiagnostics.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "PlayInEditorDataTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/AsyncTaskNotification.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWindow.h"

DEFINE_LOG_CATEGORY_STATIC(LogDarkwellEditorLifecycle, Log, All);

namespace
{
    TStrongObjectPtr<ULevelEditorPlaySettings> PerformanceSettings;
    FText OverrideName() { return FText::FromString(TEXT("Darkwell performance capture")); }
    void RestorePerformanceSettings(bool = false)
    {
        if (GEditor)
            for (FEditorViewportClient* Viewport : GEditor->GetAllViewportClients())
                Viewport->RemoveRealtimeOverride(OverrideName(), false);
        PerformanceSettings.Reset();
    }
}

void UDarkwellEditorDiagnostics::StartPerformancePIE()
{
    check(GEditor && !GEditor->PlayWorld);
    RestorePerformanceSettings();
    for (FEditorViewportClient* Viewport : GEditor->GetAllViewportClients())
        Viewport->AddRealtimeOverride(false, OverrideName());
    PerformanceSettings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
    PerformanceSettings->NewWindowWidth = 1920;
    PerformanceSettings->NewWindowHeight = 1080;
    PerformanceSettings->NewWindowPosition = FIntPoint(30,30);
    PerformanceSettings->CenterNewWindow = false;
    FRequestPlaySessionParams Params;
    Params.WorldType = EPlaySessionWorldType::PlayInEditor;
    Params.EditorPlaySettings = PerformanceSettings.Get();
    Params.bAllowOnlineSubsystem = false;
    GEditor->RequestPlaySession(Params);
}

int32 UDarkwellEditorDiagnostics::GetRealtimeEditorViewportCount()
{
    int32 Count = 0;
    if (GEditor)
        for (const FEditorViewportClient* Viewport : GEditor->GetAllViewportClients())
            Count += Viewport->IsRealtime() ? 1 : 0;
    return Count;
}

void UDarkwellEditorDiagnostics::FocusPerformancePIE()
{
    // Explicit benchmark setup only. Never called by ordinary gameplay or getters.
    if (GEngine && GEngine->GameViewport)
        if (const auto Window = GEngine->GameViewport->GetWindow()) Window->BringToFront(true);
}

class FDarkwellEditorModule final : public IModuleInterface
{
    FDelegateHandle EndPIEHandle, ExitHandle, SlateHandle;
    TUniquePtr<FAsyncTaskNotification> ProbeNotification;
public:
    void StartupModule() override
    {
        EndPIEHandle = FEditorDelegates::EndPIE.AddStatic(&RestorePerformanceSettings);
        ExitHandle = FEditorDelegates::OnEditorPreExit.AddRaw(this, &FDarkwellEditorModule::BeforeEditorExit);
        if (FSlateApplication::IsInitialized())
            SlateHandle = FSlateApplication::Get().OnPreShutdown().AddRaw(this, &FDarkwellEditorModule::BeforeSlateShutdown);
        if (!IsRunningCommandlet() && FParse::Param(FCommandLine::Get(), TEXT("DarkwellExitNotificationProbe")))
        {
            FAsyncTaskNotificationConfig Config;
            Config.TitleText = FText::FromString(TEXT("Darkwell shutdown notification lifetime probe"));
            Config.ProgressText = FText::FromString(TEXT("Rendered notification; complete after engine exit starts"));
            ProbeNotification = MakeUnique<FAsyncTaskNotification>(Config);
        }
    }
    void ShutdownModule() override
    {
        if (ProbeNotification)
        {
            // Reproduce the legacy ExecutePythonScript driver's late completion:
            // module shutdown follows Slate shutdown, and SetComplete enqueues a
            // new ticker capture holding the rendered notification widget.
            ProbeNotification->SetComplete(true);
            ProbeNotification.Reset();
            UE_LOG(LogDarkwellEditorLifecycle, Display, TEXT("DARKWELL_EXIT probe_notification_completed_after_slate=%d"), !FSlateApplication::IsInitialized());
        }
        FEditorDelegates::EndPIE.Remove(EndPIEHandle);
        FEditorDelegates::OnEditorPreExit.Remove(ExitHandle);
        if (FSlateApplication::IsInitialized()) FSlateApplication::Get().OnPreShutdown().Remove(SlateHandle);
        RestorePerformanceSettings();
    }
    void BeforeEditorExit()
    {
        UE_LOG(LogDarkwellEditorLifecycle, Display, TEXT("DARKWELL_EXIT editor_pre_exit pie=%d"), GEditor && GEditor->PlayWorld ? 1 : 0);
        if (ProbeNotification && FParse::Param(FCommandLine::Get(), TEXT("DarkwellCompleteNotificationBeforeExit")))
        {
            // Deliberately enqueue the same public async notification update after
            // EngineLoop::Exit's initial ticker Reset. No project world is involved.
            ProbeNotification->SetComplete(true);
            ProbeNotification.Reset();
            UE_LOG(LogDarkwellEditorLifecycle, Display, TEXT("DARKWELL_EXIT late_notification_enqueued"));
        }
    }
    void BeforeSlateShutdown()
    {
        UE_LOG(LogDarkwellEditorLifecycle, Display, TEXT("DARKWELL_EXIT slate_pre_shutdown"));
    }
};
IMPLEMENT_MODULE(FDarkwellEditorModule, DarkwellEditor)
