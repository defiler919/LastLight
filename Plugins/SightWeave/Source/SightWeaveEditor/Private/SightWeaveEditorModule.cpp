#include "SightWeaveLabSupport.h"
#include "SightWeaveM4P1LabFixture.h"

#include "Camera/CameraActor.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "SightWeaveComponents.h"
#include "SightWeaveRenderWorldSubsystem.h"
#include "SightWeaveSettings.h"
#include "SightWeaveWorldSubsystem.h"

#define LOCTEXT_NAMESPACE "FSightWeaveEditorModule"

DEFINE_LOG_CATEGORY_STATIC(LogSightWeaveEditor, Log, All);

namespace
{
	struct FSightWeaveLabWorldState
	{
		TWeakObjectPtr<UWorld> World;
		bool bIsolationApplied = false;
		bool bMemoryConfigured = false;
		bool bMemoryFixtureTransitionApplied = false;
		int32 BoundCameraSelection = INDEX_NONE;
		bool bFailureLogged = false;
		bool bHealthyLogged = false;
		int32 M4P1LoggedState = INDEX_NONE;
		TUniquePtr<FSightWeaveM4P1LabFixture> M4P1Fixture;
	};

	bool GSettingsSectionRegistered = false;

	TAutoConsoleVariable<int32> CVarSightWeaveLabCamera(
		TEXT("SightWeave.Lab.Camera"),
		static_cast<int32>(ESightWeaveLabCamera::Overview),
		TEXT("Select the active SightWeave Lab camera by stable Actor label.\n")
		TEXT("0: Overview, 1: Closeup, 2: DynamicDoor, 3: PageBoundary, 4: Rotated45."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarSightWeaveLabMode(
		TEXT("SightWeave.Lab.Mode"),
		static_cast<int32>(ESightWeaveLabMode::M3P5),
		TEXT("Select the isolated SightWeave Lab milestone.\n")
		TEXT("0: M2, 1: M3P3, 2: M3P4, 3: M3P5, 4: M4P1."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarSightWeaveM4P1State(
		TEXT("SightWeave.Lab.M4P1.State"),
		1,
		TEXT("Drive the primary M4P1 transition fixture.\n")
		TEXT("0: Live, 1: Remembered, 2: Reacquired, 3: SuppressedBlocked, ")
		TEXT("4: Cleared, 5: IdentityReuse."),
		ECVF_Default);

	FString GetActorLabel(const AActor* Actor)
	{
		return IsValid(Actor) ? Actor->GetActorLabel() : FString();
	}

	bool IsPageBoundaryFixture(const FString& ActorLabel)
	{
		return ActorLabel.StartsWith(TEXT("SW_M3P3_PageBoundary"))
			|| ActorLabel.StartsWith(TEXT("SW_M3P4_PageBoundary"));
	}

	bool SetVisionSourceEnabledByLabel(
		UWorld* World,
		const TCHAR* Label,
		const bool bEnabled)
	{
		if (!IsValid(World) || !IsValid(World->PersistentLevel))
		{
			return false;
		}
		for (AActor* Actor : World->PersistentLevel->Actors)
		{
			if (IsValid(Actor) && GetActorLabel(Actor) == Label)
			{
				if (USightWeaveVisionSourceComponent* Component =
					Actor->FindComponentByClass<USightWeaveVisionSourceComponent>())
				{
					Component->SetVisionSourceEnabled(bEnabled);
					return true;
				}
			}
		}
		return false;
	}

	double GetPageBoundaryFixtureY(const FString& ActorLabel)
	{
		if (ActorLabel == TEXT("SW_M3P3_PageBoundary_Label"))
		{
			return SightWeave::Lab::SafePageBoundaryY - 1200.0;
		}
		if (ActorLabel == TEXT("SW_M3P4_PageBoundary_Label"))
		{
			return SightWeave::Lab::SafePageBoundaryY + 1100.0;
		}
		return SightWeave::Lab::SafePageBoundaryY;
	}

	template <typename ComponentType, typename SetterType>
	void SetActorComponentsEnabled(
		AActor* Actor,
		const bool bEnabled,
		SetterType Setter,
		int32& EnabledCount,
		int32& DisabledCount)
	{
		TInlineComponentArray<ComponentType*> Components(Actor);
		for (ComponentType* Component : Components)
		{
			(Component->*Setter)(bEnabled);
			if (bEnabled)
			{
				++EnabledCount;
			}
			else
			{
				++DisabledCount;
			}
		}
	}
}

namespace SightWeave::Lab
{
	bool IsSettingsSectionRegistered()
	{
		return GSettingsSectionRegistered;
	}

	ESightWeaveLabMode ResolveModeFromCommandLine()
	{
		FString RequestedMode;
		if (FParse::Value(FCommandLine::Get(), TEXT("SightWeaveLabMode="), RequestedMode))
		{
			if (RequestedMode.Equals(TEXT("M2"), ESearchCase::IgnoreCase))
			{
				return ESightWeaveLabMode::M2;
			}
			if (RequestedMode.Equals(TEXT("M3P3"), ESearchCase::IgnoreCase)
				|| RequestedMode.Equals(TEXT("M3.3"), ESearchCase::IgnoreCase))
			{
				return ESightWeaveLabMode::M3P3;
			}
			if (RequestedMode.Equals(TEXT("M3P4"), ESearchCase::IgnoreCase)
				|| RequestedMode.Equals(TEXT("M3.4"), ESearchCase::IgnoreCase))
			{
				return ESightWeaveLabMode::M3P4;
			}
			if (RequestedMode.Equals(TEXT("M3P5"), ESearchCase::IgnoreCase)
				|| RequestedMode.Equals(TEXT("M3.5"), ESearchCase::IgnoreCase))
			{
				return ESightWeaveLabMode::M3P5;
			}
			if (RequestedMode.Equals(TEXT("M4P1"), ESearchCase::IgnoreCase)
				|| RequestedMode.Equals(TEXT("M4.1"), ESearchCase::IgnoreCase))
			{
				return ESightWeaveLabMode::M4P1;
			}
		}

		return ESightWeaveLabMode::M3P5;
	}

	const TCHAR* LexToString(const ESightWeaveLabMode Mode)
	{
		switch (Mode)
		{
		case ESightWeaveLabMode::M2:
			return TEXT("M2");
		case ESightWeaveLabMode::M3P3:
			return TEXT("M3P3");
		case ESightWeaveLabMode::M3P4:
			return TEXT("M3P4");
		case ESightWeaveLabMode::M4P1:
			return TEXT("M4P1");
		case ESightWeaveLabMode::M3P5:
		default:
			return TEXT("M3P5");
		}
	}

	const TCHAR* GetCameraLabel(
		const ESightWeaveLabMode Mode,
		const ESightWeaveLabCamera Camera)
	{
		if (Mode == ESightWeaveLabMode::M4P1)
		{
			switch (Camera)
			{
			case ESightWeaveLabCamera::Closeup:
				return TEXT("SW_M4P1_Camera1_Transition");
			case ESightWeaveLabCamera::DynamicDoor:
				return TEXT("SW_M4P1_Camera2_PolicyMatrix");
			case ESightWeaveLabCamera::PageBoundary:
				return TEXT("SW_M4P1_Camera3_PageBoundary");
			case ESightWeaveLabCamera::Rotated45:
				return TEXT("SW_M4P1_Camera4_Rotated45");
			case ESightWeaveLabCamera::Overview:
			default:
				return TEXT("SW_M4P1_Camera0_Overview");
			}
		}
		if (Mode == ESightWeaveLabMode::M3P4 || Mode == ESightWeaveLabMode::M3P5)
		{
			switch (Camera)
			{
			case ESightWeaveLabCamera::Closeup:
				return Mode == ESightWeaveLabMode::M3P5
					? TEXT("SW_M3P5_RememberedCamera")
					: TEXT("SW_M3P4_CloseupCamera");
			case ESightWeaveLabCamera::DynamicDoor:
				return Mode == ESightWeaveLabMode::M3P5
					? TEXT("SW_M3P5_DynamicLeakCamera")
					: TEXT("SW_M3P4_DynamicDoorCamera");
			case ESightWeaveLabCamera::PageBoundary:
				return Mode == ESightWeaveLabMode::M3P5
					? TEXT("SW_M3P5_PageBoundaryCamera")
					: TEXT("SW_M3P4_PageBoundaryCamera");
			case ESightWeaveLabCamera::Rotated45:
				return Mode == ESightWeaveLabMode::M3P5
					? TEXT("SW_M3P5_Rotated45Camera")
					: TEXT("SW_M3P4_Rotated45Camera");
			case ESightWeaveLabCamera::Overview:
			default:
				return Mode == ESightWeaveLabMode::M3P5
					? TEXT("SW_M3P5_OverviewCamera")
					: TEXT("SW_M3P4_OverviewCamera");
			}
		}

		return Mode == ESightWeaveLabMode::M2
			? TEXT("SW_M2_01_OverviewCamera")
			: TEXT("SW_M3P3_OverviewCamera");
	}

	bool IsLabWorld(const UWorld* World)
	{
		return IsValid(World)
			&& FPackageName::GetShortName(World->GetOutermost()->GetName())
				.EndsWith(TEXT("L_SightWeave_Lab"));
	}

	bool IsFixtureEnabled(const FString& ActorLabel, const ESightWeaveLabMode Mode)
	{
		switch (Mode)
		{
		case ESightWeaveLabMode::M2:
			return ActorLabel.StartsWith(TEXT("SW_M2_"));
		case ESightWeaveLabMode::M3P3:
			return ActorLabel.StartsWith(TEXT("SW_M3P3_"));
		case ESightWeaveLabMode::M3P4:
			return ActorLabel.StartsWith(TEXT("SW_M3P4_")) || IsPageBoundaryFixture(ActorLabel);
		case ESightWeaveLabMode::M4P1:
			return ActorLabel.StartsWith(TEXT("SW_M4P1_"));
		case ESightWeaveLabMode::M3P5:
		default:
			return ActorLabel.StartsWith(TEXT("SW_M3P5_"))
				|| ActorLabel.StartsWith(TEXT("SW_M3P3_PageBoundary"));
		}
	}

	FSightWeaveLabIsolationResult ApplyFixtureIsolation(
		UWorld* World,
		const ESightWeaveLabMode Mode)
	{
		FSightWeaveLabIsolationResult Result;
		if (!IsLabWorld(World) || !IsValid(World->PersistentLevel))
		{
			return Result;
		}

		for (AActor* Actor : World->PersistentLevel->Actors)
		{
			if (!IsValid(Actor))
			{
				continue;
			}

			const FString ActorLabel = GetActorLabel(Actor);
			const bool bEnable = IsFixtureEnabled(ActorLabel, Mode);

			SetActorComponentsEnabled<USightWeaveVisionSourceComponent>(
				Actor,
				bEnable,
				&USightWeaveVisionSourceComponent::SetVisionSourceEnabled,
				Result.EnabledVisionSources,
				Result.DisabledVisionSources);
			SetActorComponentsEnabled<USightWeaveIlluminationSourceComponent>(
				Actor,
				bEnable,
				&USightWeaveIlluminationSourceComponent::SetIlluminationSourceEnabled,
				Result.EnabledIlluminationSources,
				Result.DisabledIlluminationSources);
			SetActorComponentsEnabled<USightWeaveOccluderComponent>(
				Actor,
				bEnable,
				&USightWeaveOccluderComponent::SetOccluderEnabled,
				Result.EnabledOccluders,
				Result.DisabledOccluders);
			SetActorComponentsEnabled<USightWeaveHardSuppressionComponent>(
				Actor,
				bEnable,
				&USightWeaveHardSuppressionComponent::SetHardSuppressionEnabled,
				Result.EnabledSuppressions,
				Result.DisabledSuppressions);
			SetActorComponentsEnabled<USightWeaveStaticEnvironmentComponent>(
				Actor,
				bEnable,
				&USightWeaveStaticEnvironmentComponent::SetStaticEnvironmentEnabled,
				Result.EnabledStaticEnvironments,
				Result.DisabledStaticEnvironments);
			SetActorComponentsEnabled<USightWeaveMemoryModifierComponent>(
				Actor,
				bEnable,
				&USightWeaveMemoryModifierComponent::SetMemoryModifierEnabled,
				Result.EnabledMemoryModifiers,
				Result.DisabledMemoryModifiers);

			const double CorrectedPageBoundaryY = GetPageBoundaryFixtureY(ActorLabel);
			if (IsPageBoundaryFixture(ActorLabel)
				&& Actor->FindComponentByClass<USightWeaveVisionSourceComponent>()
				&& !FMath::IsNearlyEqual(Actor->GetActorLocation().Y, CorrectedPageBoundaryY))
			{
				FVector Location = Actor->GetActorLocation();
				Location.Y = CorrectedPageBoundaryY;
				Actor->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
				++Result.AdjustedPageBoundaryActors;
			}
		}

		return Result;
	}
}

class FSightWeaveEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		RegisterSettings();
		LabMode = SightWeave::Lab::ResolveModeFromCommandLine();
		if (IConsoleVariable* ModeVariable =
			IConsoleManager::Get().FindConsoleVariable(TEXT("SightWeave.Lab.Mode")))
		{
			ModeVariable->Set(static_cast<int32>(LabMode), ECVF_SetByCode);
		}
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FSightWeaveEditorModule::Tick));
		bTickerRegistered = true;
	}

	virtual void ShutdownModule() override
	{
		if (bTickerRegistered)
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			bTickerRegistered = false;
		}

		LabWorldStates.Reset();
		UnregisterSettings();
	}

private:
	void RegisterSettings()
	{
		ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>(TEXT("Settings"));
		SettingsModule.RegisterSettings(
			TEXT("Project"),
			TEXT("Plugins"),
			TEXT("SightWeave"),
			LOCTEXT("SightWeaveSettingsName", "SightWeave"),
			LOCTEXT("SightWeaveSettingsDescription", "Configure SightWeave authority and presentation defaults."),
			GetMutableDefault<USightWeaveSettings>());
		bRegisteredSettings = true;
		GSettingsSectionRegistered = bRegisteredSettings;
	}

	void UnregisterSettings()
	{
		if (bRegisteredSettings && FModuleManager::Get().IsModuleLoaded(TEXT("Settings")))
		{
			FModuleManager::GetModuleChecked<ISettingsModule>(TEXT("Settings"))
				.UnregisterSettings(TEXT("Project"), TEXT("Plugins"), TEXT("SightWeave"));
		}
		bRegisteredSettings = false;
		GSettingsSectionRegistered = false;
	}

	bool Tick(float)
	{
		LabWorldStates.RemoveAll([](const FSightWeaveLabWorldState& State)
		{
			return !State.World.IsValid();
		});

		if (!GEngine)
		{
			return true;
		}

		const ESightWeaveLabMode RequestedMode = static_cast<ESightWeaveLabMode>(FMath::Clamp(
			CVarSightWeaveLabMode.GetValueOnGameThread(),
			static_cast<int32>(ESightWeaveLabMode::M2),
			static_cast<int32>(ESightWeaveLabMode::M4P1)));
		if (RequestedMode != LabMode)
		{
			for (FSightWeaveLabWorldState& State : LabWorldStates)
			{
				State.M4P1Fixture.Reset();
				State.bIsolationApplied = false;
				State.bMemoryConfigured = false;
				State.bMemoryFixtureTransitionApplied = false;
				State.BoundCameraSelection = INDEX_NONE;
				State.bFailureLogged = false;
				State.bHealthyLogged = false;
				State.M4P1LoggedState = INDEX_NONE;
			}
			LabMode = RequestedMode;
			UE_LOG(LogSightWeaveEditor, Display, TEXT("Lab mode changed mode=%s"),
				SightWeave::Lab::LexToString(LabMode));
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!SightWeave::Lab::IsLabWorld(World)
				|| (World->WorldType != EWorldType::PIE
					&& World->WorldType != EWorldType::Game
					&& World->WorldType != EWorldType::GamePreview))
			{
				continue;
			}

			FSightWeaveLabWorldState* State = LabWorldStates.FindByPredicate(
				[World](const FSightWeaveLabWorldState& Candidate)
				{
					return Candidate.World.Get() == World;
				});
			if (!State)
			{
				State = &LabWorldStates.AddDefaulted_GetRef();
				State->World = World;
			}

			if (!State->bIsolationApplied)
			{
				const FSightWeaveLabIsolationResult Result =
					SightWeave::Lab::ApplyFixtureIsolation(World, LabMode);
				State->bIsolationApplied = true;
				UE_LOG(
					LogSightWeaveEditor,
					Display,
					TEXT("Lab isolation mode=%s vision=%d/%d illumination=%d/%d occluders=%d/%d suppression=%d/%d static=%d/%d memoryModifiers=%d/%d pageBoundaryAdjusted=%d"),
					SightWeave::Lab::LexToString(LabMode),
					Result.EnabledVisionSources,
					Result.DisabledVisionSources,
					Result.EnabledIlluminationSources,
					Result.DisabledIlluminationSources,
					Result.EnabledOccluders,
					Result.DisabledOccluders,
					Result.EnabledSuppressions,
					Result.DisabledSuppressions,
					Result.EnabledStaticEnvironments,
					Result.DisabledStaticEnvironments,
					Result.EnabledMemoryModifiers,
					Result.DisabledMemoryModifiers,
					Result.AdjustedPageBoundaryActors);
			}

			if (LabMode == ESightWeaveLabMode::M3P5)
			{
				PrepareM3P5Memory(World, *State);
			}
			else if (LabMode == ESightWeaveLabMode::M4P1)
			{
				if (!State->M4P1Fixture)
				{
					State->M4P1Fixture = MakeUnique<FSightWeaveM4P1LabFixture>();
					if (!State->M4P1Fixture->Initialize(World))
					{
						State->M4P1Fixture.Reset();
						continue;
					}
					State->BoundCameraSelection = INDEX_NONE;
				}
				const int32 RequestedState = FMath::Clamp(
					CVarSightWeaveM4P1State.GetValueOnGameThread(), 0, 5);
				if (State->M4P1Fixture->GetAppliedState() != RequestedState)
				{
					if (!State->M4P1Fixture->ApplyState(RequestedState))
					{
						continue;
					}
					State->BoundCameraSelection = INDEX_NONE;
				}
				if (State->M4P1LoggedState != RequestedState)
				{
					State->M4P1LoggedState = RequestedState;
					UE_LOG(LogSightWeaveEditor, Display,
						TEXT("M4P1 Lab state=%d live=%d proxies=%d retainedSnapshots=%d"),
						RequestedState,
						State->M4P1Fixture->GetVisibleLiveCount(),
						State->M4P1Fixture->GetVisibleProxyCount(),
						State->M4P1Fixture->GetRetainedSnapshotCount());
				}
				State->M4P1Fixture->Tick();
			}

			const int32 CameraSelection = FMath::Clamp(
				CVarSightWeaveLabCamera.GetValueOnGameThread(),
				static_cast<int32>(ESightWeaveLabCamera::Overview),
				static_cast<int32>(ESightWeaveLabCamera::Rotated45));
			if (State->BoundCameraSelection != CameraSelection
				&& BindCamera(World, static_cast<ESightWeaveLabCamera>(CameraSelection)))
			{
				State->BoundCameraSelection = CameraSelection;
			}

			ReportDiagnostics(World, *State);
		}

		return true;
	}

	static void PrepareM3P5Memory(UWorld* World, FSightWeaveLabWorldState& State)
	{
		USightWeaveWorldSubsystem* Subsystem =
			IsValid(World) ? World->GetSubsystem<USightWeaveWorldSubsystem>() : nullptr;
		if (!Subsystem)
		{
			return;
		}
		const FSightWeaveKnowledgeOwnerId Owner(FName(TEXT("Local")));
		const FSightWeaveFloorId Floor(FName(TEXT("Ground")));
		if (!State.bMemoryConfigured)
		{
			State.bMemoryConfigured = Subsystem->ConfigureExplorationMemory(
				Owner,
				Floor,
				GetDefault<USightWeaveSettings>()->ExplorationMemoryPrecisionTier,
				SightWeave::SparseAtlas::StandardActiveTileCapacity);
			if (!State.bMemoryConfigured)
			{
				return;
			}
			UE_LOG(LogSightWeaveEditor, Display,
				TEXT("M3.5 Lab exploration memory configured before fixture transition"));
		}
		if (State.bMemoryFixtureTransitionApplied || !IsValid(World->PersistentLevel))
		{
			return;
		}

		bool bModifiersReady = true;
		for (AActor* Actor : World->PersistentLevel->Actors)
		{
			if (!IsValid(Actor) || !GetActorLabel(Actor).StartsWith(TEXT("SW_M3P5_")))
			{
				continue;
			}
			TInlineComponentArray<USightWeaveMemoryModifierComponent*> Modifiers(Actor);
			for (USightWeaveMemoryModifierComponent* Modifier : Modifiers)
			{
				bModifiersReady &= Modifier->RefreshMemoryModifierRegistration();
			}
		}
		FSightWeaveMemoryScopeKey Scope;
		if (!bModifiersReady || !Subsystem->GetExplorationMemoryScope(Scope))
		{
			return;
		}
		auto ClearCircle = [Subsystem, &Scope](const FVector2D Center, const float Radius)
		{
			FSightWeaveMemoryRegion Region;
			Region.Scope = Scope;
			Region.HeightRange = { 0.0f, 300.0f };
			Region.Shape = ESightWeaveMemoryRegionShape::Circle;
			Region.Center = Center;
			Region.Radius = Radius;
			return Subsystem->ClearExplorationMemory(Region);
		};
		const bool bCleared = ClearCircle(FVector2D(47000.0, 8500.0), 420.0f)
			&& ClearCircle(FVector2D(53000.0, 8500.0), 850.0f);
		const bool bRememberDisabled = SetVisionSourceEnabledByLabel(
			World,
			TEXT("SW_M3P5_RememberOnce"),
			false);
		const bool bBlockProbeDisabled = SetVisionSourceEnabledByLabel(
			World,
			TEXT("SW_M3P5_BlockProbe"),
			false);
		const bool bNegativeTileDisabled = SetVisionSourceEnabledByLabel(
			World,
			TEXT("SW_M3P5_NegativeTileRememberOnce"),
			false);
		const bool bRememberedSample = Subsystem->QueryHardMemoryAtLocation(
			FVector(48000.0, 8500.0, 100.0));
		const bool bLiveSample = Subsystem->QueryHardMemoryAtLocation(
			FVector(57000.0, 8500.0, 100.0));
		const bool bClearedSample = Subsystem->QueryHardMemoryAtLocation(
			FVector(47000.0, 8500.0, 100.0));
		const bool bBlockedSample = Subsystem->QueryHardMemoryAtLocation(
			FVector(53000.0, 8500.0, 100.0));
		const bool bSuppressedSample = Subsystem->QueryHardMemoryAtLocation(
			FVector(49500.0, 8500.0, 100.0));
		const bool bSuppressionActive = Subsystem->IsMemoryPresentationSuppressedAtLocation(
			FVector(49500.0, 8500.0, 100.0));
		const bool bUnknownSample = Subsystem->QueryHardMemoryAtLocation(
			FVector(61500.0, 8500.0, 100.0));
		const bool bMemoryStateReady = bRememberedSample
			&& bLiveSample
			&& !bClearedSample
			&& !bBlockedSample
			&& bSuppressedSample
			&& bSuppressionActive
			&& !bUnknownSample;
		State.bMemoryFixtureTransitionApplied =
			bCleared && bRememberDisabled && bBlockProbeDisabled && bNegativeTileDisabled
			&& bMemoryStateReady;
		if (State.bMemoryFixtureTransitionApplied)
		{
			UE_LOG(LogSightWeaveEditor, Display,
				TEXT("M3.5 Lab transitioned to remembered/live/unknown with clear/block/suppress fixtures samples=remembered:%d live:%d clear:%d block:%d suppressed:%d/%d unknown:%d"),
				bRememberedSample,
				bLiveSample,
				bClearedSample,
				bBlockedSample,
				bSuppressedSample,
				bSuppressionActive,
				bUnknownSample);
		}
	}

	bool BindCamera(UWorld* World, const ESightWeaveLabCamera Camera) const
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (!IsValid(PlayerController) || !IsValid(World->PersistentLevel))
		{
			return false;
		}

		const FString ExpectedLabel(SightWeave::Lab::GetCameraLabel(LabMode, Camera));
		for (AActor* Actor : World->PersistentLevel->Actors)
		{
			if (IsValid(Actor)
				&& Actor->IsA<ACameraActor>()
				&& GetActorLabel(Actor).Equals(ExpectedLabel, ESearchCase::CaseSensitive))
			{
				PlayerController->SetViewTarget(Actor);
				UE_LOG(LogSightWeaveEditor, Display, TEXT("Lab camera bound mode=%s actor=%s"),
					SightWeave::Lab::LexToString(LabMode), *ExpectedLabel);
				return true;
			}
		}

		return false;
	}

	static void ReportDiagnostics(UWorld* World, FSightWeaveLabWorldState& State)
	{
		USightWeaveRenderWorldSubsystem* RenderSubsystem =
			World->GetSubsystem<USightWeaveRenderWorldSubsystem>();
		if (!RenderSubsystem)
		{
			return;
		}

		const FSightWeaveRenderWorldDiagnostics& Diagnostics = RenderSubsystem->GetDiagnostics();
		if (Diagnostics.LastBuildFailure != ESightWeaveSparsePacketFailure::None && !State.bFailureLogged)
		{
			State.bFailureLogged = true;
			UE_LOG(LogSightWeaveEditor, Warning,
				TEXT("Lab render packet fail-closed failure=%d desiredTiles=%d capacity=%d published=%llu clears=%llu dirty=%llu removed=%llu"),
				static_cast<int32>(Diagnostics.LastBuildFailure),
				Diagnostics.LastDesiredTileCount,
				Diagnostics.LastMaximumActiveTiles,
				Diagnostics.PublishedPacketCount,
				Diagnostics.FailClosedClearCount,
				Diagnostics.SubmittedDirtyTileCount,
				Diagnostics.SubmittedRemovedTileCount);
		}

		if (Diagnostics.PublishedPacketCount > 0
			&& Diagnostics.LastBuildFailure == ESightWeaveSparsePacketFailure::None
			&& !State.bHealthyLogged)
		{
			State.bHealthyLogged = true;
			UE_LOG(LogSightWeaveEditor, Display,
				TEXT("Lab render packet healthy desiredTiles=%d capacity=%d published=%llu packetRevision=%llu snapshotRevision=%llu dirty=%llu removed=%llu presentation=%s"),
				Diagnostics.LastDesiredTileCount,
				Diagnostics.LastMaximumActiveTiles,
				Diagnostics.PublishedPacketCount,
				Diagnostics.LastSubmittedPacketRevision,
				Diagnostics.LastSubmittedSnapshotRevision,
				Diagnostics.SubmittedDirtyTileCount,
				Diagnostics.SubmittedRemovedTileCount,
				Diagnostics.bPresentationEnabled ? TEXT("enabled") : TEXT("disabled"));
		}
	}

	ESightWeaveLabMode LabMode = ESightWeaveLabMode::M3P5;
	FTSTicker::FDelegateHandle TickerHandle;
	TArray<FSightWeaveLabWorldState> LabWorldStates;
	bool bTickerRegistered = false;
	bool bRegisteredSettings = false;
};

IMPLEMENT_MODULE(FSightWeaveEditorModule, SightWeaveEditor)

#undef LOCTEXT_NAMESPACE
