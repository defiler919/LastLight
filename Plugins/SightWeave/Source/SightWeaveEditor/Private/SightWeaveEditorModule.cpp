#include "SightWeaveLabSupport.h"

#include "Camera/CameraActor.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "SightWeaveComponents.h"
#include "SightWeaveRenderWorldSubsystem.h"
#include "SightWeaveSettings.h"

#define LOCTEXT_NAMESPACE "FSightWeaveEditorModule"

DEFINE_LOG_CATEGORY_STATIC(LogSightWeaveEditor, Log, All);

namespace
{
	struct FSightWeaveLabWorldState
	{
		TWeakObjectPtr<UWorld> World;
		bool bIsolationApplied = false;
		bool bCameraBound = false;
		bool bFailureLogged = false;
		bool bHealthyLogged = false;
	};

	FString GetActorLabel(const AActor* Actor)
	{
		return IsValid(Actor) ? Actor->GetActorLabel() : FString();
	}

	bool IsPageBoundaryFixture(const FString& ActorLabel)
	{
		return ActorLabel.StartsWith(TEXT("SW_M3P3_PageBoundary"))
			|| ActorLabel.StartsWith(TEXT("SW_M3P4_PageBoundary"));
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

	const TCHAR* GetOverviewCameraLabel(const ESightWeaveLabMode Mode)
	{
		switch (Mode)
		{
		case ESightWeaveLabMode::M2:
			return TEXT("SW_M2_01_OverviewCamera");
		case ESightWeaveLabMode::M3P3:
			return TEXT("SW_M3P3_OverviewCamera");
		case ESightWeaveLabMode::M3P4:
		default:
			return TEXT("SW_M3P4_OverviewCamera");
		}
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
		}

		return ESightWeaveLabMode::M3P4;
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
		default:
			return TEXT("M3P4");
		}
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
		default:
			return ActorLabel.StartsWith(TEXT("SW_M3P4_")) || IsPageBoundaryFixture(ActorLabel);
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

			const double CorrectedPageBoundaryY = GetPageBoundaryFixtureY(ActorLabel);
			if (IsPageBoundaryFixture(ActorLabel)
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
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FSightWeaveEditorModule::Tick));
	}

	virtual void ShutdownModule() override
	{
		if (TickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			TickerHandle = FDelegateHandle();
		}

		LabWorldStates.Reset();
		UnregisterSettings();
	}

private:
	void RegisterSettings()
	{
		ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>(TEXT("Settings"));
		if (!SettingsModule.GetSection(TEXT("Project"), TEXT("Plugins"), TEXT("SightWeave")).IsValid())
		{
			SettingsModule.RegisterSettings(
				TEXT("Project"),
				TEXT("Plugins"),
				TEXT("SightWeave"),
				LOCTEXT("SightWeaveSettingsName", "SightWeave"),
				LOCTEXT("SightWeaveSettingsDescription", "Configure SightWeave authority and presentation defaults."),
				GetMutableDefault<USightWeaveSettings>());
			bRegisteredSettings = true;
		}
	}

	void UnregisterSettings()
	{
		if (bRegisteredSettings && FModuleManager::Get().IsModuleLoaded(TEXT("Settings")))
		{
			FModuleManager::GetModuleChecked<ISettingsModule>(TEXT("Settings"))
				.UnregisterSettings(TEXT("Project"), TEXT("Plugins"), TEXT("SightWeave"));
		}
		bRegisteredSettings = false;
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
					TEXT("Lab isolation mode=%s vision=%d/%d illumination=%d/%d occluders=%d/%d suppression=%d/%d pageBoundaryAdjusted=%d"),
					SightWeave::Lab::LexToString(LabMode),
					Result.EnabledVisionSources,
					Result.DisabledVisionSources,
					Result.EnabledIlluminationSources,
					Result.DisabledIlluminationSources,
					Result.EnabledOccluders,
					Result.DisabledOccluders,
					Result.EnabledSuppressions,
					Result.DisabledSuppressions,
					Result.AdjustedPageBoundaryActors);
			}

			if (!State->bCameraBound)
			{
				State->bCameraBound = BindOverviewCamera(World);
			}

			ReportDiagnostics(World, *State);
		}

		return true;
	}

	bool BindOverviewCamera(UWorld* World) const
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (!IsValid(PlayerController) || !IsValid(World->PersistentLevel))
		{
			return false;
		}

		const FString ExpectedLabel(GetOverviewCameraLabel(LabMode));
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

	ESightWeaveLabMode LabMode = ESightWeaveLabMode::M3P4;
	FDelegateHandle TickerHandle;
	TArray<FSightWeaveLabWorldState> LabWorldStates;
	bool bRegisteredSettings = false;
};

IMPLEMENT_MODULE(FSightWeaveEditorModule, SightWeaveEditor)

#undef LOCTEXT_NAMESPACE
