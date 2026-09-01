// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/DarkwellHUD.h"

#include "AI/DarkwellStalkerCharacter.h"
#include "AI/DarkwellStalkerController.h"
#include "Async/ParallelFor.h"
#include "Combat/DarkwellLoadoutComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/Canvas.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "CanvasItem.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Game/DarkwellGameState.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "Gameplay/DarkwellVisibilityComponent.h"
#include "Interaction/DarkwellInteractionComponent.h"
#include "Inventory/DarkwellInventoryComponent.h"
#include "Inventory/DarkwellItemCatalog.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/DarkwellCharacter.h"
#include "Player/DarkwellPlayerController.h"
#include "Player/DarkwellPlayerMath.h"
#include "Save/DarkwellSaveSubsystem.h"
#include "World/DarkwellWorkbench.h"
#include "Visibility/SightWeave/DarkwellSightWeaveWorldSubsystem.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float InventorySlotSize = 76.0f;
	constexpr float InventorySlotGap = 9.0f;
	constexpr int32 InventoryColumns = 4;
	constexpr float InventoryPanelWidth = 388.0f;
	constexpr float InventoryPanelHeight = 458.0f;
	constexpr float InventoryPanelGap = 36.0f;
	constexpr float InventorySlotStartX = 29.0f;
	constexpr float InventorySlotStartY = 78.0f;
	constexpr float MenuButtonWidth = 440.0f;
	constexpr float MenuButtonHeight = 58.0f;
	constexpr float MenuButtonGap = 12.0f;
	constexpr float MenuPanelWidth = 540.0f;
	constexpr float MenuHeaderHeight = 128.0f;

	struct FDarkwellMenuEntry
	{
		EDarkwellMenuAction Action;
		FText Label;
		bool bEnabled;
	};

	TArray<FDarkwellMenuEntry> BuildMenuEntries(const ADarkwellPlayerController& Controller)
	{
		TArray<FDarkwellMenuEntry> Entries;
		auto Add = [&Entries, &Controller](const EDarkwellMenuAction Action, const FText& Label)
		{
			Entries.Add({Action, Label, Controller.IsMenuActionEnabled(Action)});
		};
		switch (Controller.GetMenuScreen())
		{
		case EDarkwellMenuScreen::Main:
			Add(EDarkwellMenuAction::NewGame, NSLOCTEXT("Darkwell", "MenuNewGame", "NEW GAME"));
			Add(EDarkwellMenuAction::ContinueGame, NSLOCTEXT("Darkwell", "MenuContinue", "CONTINUE"));
			Add(EDarkwellMenuAction::Settings, NSLOCTEXT("Darkwell", "MenuSettings", "SETTINGS"));
			Add(EDarkwellMenuAction::Quit, NSLOCTEXT("Darkwell", "MenuQuit", "QUIT"));
			break;
		case EDarkwellMenuScreen::Pause:
			Add(EDarkwellMenuAction::ResumeGame, NSLOCTEXT("Darkwell", "MenuResume", "RESUME"));
			Add(EDarkwellMenuAction::SaveGame, NSLOCTEXT("Darkwell", "MenuSave", "SAVE GAME"));
			Add(EDarkwellMenuAction::LoadGame, NSLOCTEXT("Darkwell", "MenuLoad", "LOAD GAME"));
			Add(EDarkwellMenuAction::Settings, NSLOCTEXT("Darkwell", "MenuPauseSettings", "SETTINGS"));
			Add(EDarkwellMenuAction::ReturnToMainMenu, NSLOCTEXT("Darkwell", "MenuReturnMain", "RETURN TO MAIN MENU"));
			break;
		case EDarkwellMenuScreen::Settings:
			Add(EDarkwellMenuAction::CycleDisplayMode, Controller.GetDisplayModeText());
			Add(EDarkwellMenuAction::Back, NSLOCTEXT("Darkwell", "MenuBack", "BACK"));
			break;
		default:
			break;
		}
		return Entries;
	}

	FVector2D GetMenuButtonOrigin(const float ClipX, const float ClipY, const int32 EntryCount, const int32 Index)
	{
		const float PanelHeight = MenuHeaderHeight + EntryCount * MenuButtonHeight
			+ FMath::Max(0, EntryCount - 1) * MenuButtonGap + 44.0f;
		return FVector2D(
			(ClipX - MenuButtonWidth) * 0.5f,
			(ClipY - PanelHeight) * 0.5f + MenuHeaderHeight + Index * (MenuButtonHeight + MenuButtonGap));
	}

	bool IsPointInside(const FVector2D& Point, const FVector2D& Origin, const FVector2D& Size)
	{
		return Point.X >= Origin.X && Point.Y >= Origin.Y
			&& Point.X <= Origin.X + Size.X && Point.Y <= Origin.Y + Size.Y;
	}
}

ADarkwellHUD::ADarkwellHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FogMaterialFinder(
		TEXT("/Game/UI/Fog/M_FogMemoryComposite.M_FogMemoryComposite"));
	if (FogMaterialFinder.Succeeded())
	{
		FogCompositeMaterial = FogMaterialFinder.Object;
	}
}

void ADarkwellHUD::SetLegacyFogAuthorityEnabled(const bool bEnabled)
{
	bLegacyFogAuthorityEnabled = bEnabled;
	if (!bLegacyFogAuthorityEnabled)
	{
		SetFogCompositeWeight(nullptr, 0.0f);
		FogUpdateTimeRemaining = 0.0f;
	}
}

void ADarkwellHUD::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!PlayerOwner)
	{
		return;
	}

	ADarkwellCharacter* Character = Cast<ADarkwellCharacter>(PlayerOwner->GetPawn());
	const ADarkwellPlayerController* DarkwellController =
		Cast<ADarkwellPlayerController>(PlayerOwner);
	if (!Character || !bLegacyFogAuthorityEnabled
		|| (DarkwellController && DarkwellController->IsMenuOpen()))
	{
		SetFogCompositeWeight(Character, 0.0f);
		return;
	}

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PlayerOwner->GetViewportSize(ViewportWidth, ViewportHeight);
	if (ViewportWidth > 0 && ViewportHeight > 0)
	{
		UpdateFogOfWar(
			*Character,
			FIntPoint(ViewportWidth, ViewportHeight),
			DeltaSeconds);
	}
}

void ADarkwellHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas || !PlayerOwner)
	{
		return;
	}

	const ADarkwellPlayerController* DarkwellController = Cast<ADarkwellPlayerController>(PlayerOwner);
	if (DarkwellController && DarkwellController->IsMenuOpen())
	{
		DrawMenuInterface();
		return;
	}

	const ADarkwellCharacter* Character = Cast<ADarkwellCharacter>(PlayerOwner->GetPawn());
	if (!Character)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	const UDarkwellLoadoutComponent* Loadout = Character->GetLoadoutComponent();
	if (Font && Loadout)
	{
		const float HealthRatio = FMath::Clamp(Character->GetHealth() / Character->GetMaxHealth(), 0.0f, 1.0f);
		const FString WeaponState = Loadout->IsReloading() ? TEXT("  RELOADING") : FString();
		FString RightToolStatus;
		if (Loadout->GetEquippedRightHandItem() == DarkwellGameplayTags::Equipment_Right_Torch)
		{
			const TCHAR* TorchState = Loadout->IsTorchOverheated()
				? TEXT("OVERHEATED")
				: (Loadout->IsTorchDeterrentActive()
					? TEXT("DETERRENT")
					: (Loadout->IsTorchSwinging()
						? TEXT("SWING")
						: (Loadout->IsReloading() ? TEXT("LOW") : (Loadout->IsTorchOn() ? TEXT("READY") : TEXT("EMPTY")))));
			RightToolStatus = FString::Printf(
				TEXT("TORCH  %d%%  HEAT %d%% [%s]"),
				FMath::RoundToInt(Loadout->GetTorchCharge()),
				FMath::RoundToInt(Loadout->GetTorchHeat()),
				TorchState);
		}
		else
		{
			const TCHAR* LanternState = Loadout->IsLanternFlashActive()
				? TEXT("FLASH")
				: (Loadout->IsLanternFocused()
					? TEXT("FOCUS")
					: (Loadout->GetLanternFlashCooldownRemaining() > 0.0f
						? TEXT("RECHARGING")
						: (Loadout->IsLanternOn() ? TEXT("BASE") : TEXT("EMPTY"))));
			RightToolStatus = FString::Printf(
				TEXT("LANTERN  %d%%  [%s]"),
				FMath::RoundToInt(Loadout->GetLanternFuel()),
				LanternState);
		}
		const FString Status = FString::Printf(
			TEXT("HEALTH  %d%%    SHOTGUN  %d / %d%s    %s%s"),
			FMath::RoundToInt(100.0f * Character->GetHealth() / Character->GetMaxHealth()),
			Loadout->GetLoadedShells(),
			Loadout->GetReserveShells(),
			*WeaponState,
			*RightToolStatus,
			Character->IsSprinting() ? TEXT("    SPRINTING") : TEXT(""));
		DrawText(Status, FLinearColor(0.92f, 0.9f, 0.78f), 35.0f, Canvas->ClipY - 70.0f, Font, 1.15f, false);

		constexpr float HealthBarWidth = 250.0f;
		constexpr float HealthBarHeight = 12.0f;
		const float HealthBarY = Canvas->ClipY - 94.0f;
		DrawRect(FLinearColor(0.025f, 0.025f, 0.03f, 0.9f), 35.0f, HealthBarY, HealthBarWidth, HealthBarHeight);
		const FLinearColor HealthColor = FLinearColor::LerpUsingHSV(
			FLinearColor(0.82f, 0.035f, 0.02f),
			FLinearColor(0.18f, 0.78f, 0.24f),
			HealthRatio);
		DrawRect(HealthColor, 37.0f, HealthBarY + 2.0f, (HealthBarWidth - 4.0f) * HealthRatio, HealthBarHeight - 4.0f);
	}

	if (Font && GetWorld())
	{
		const UDarkwellVisibilityComponent* Visibility = Character->GetVisibilityComponent();
		const UDarkwellSightWeaveWorldSubsystem* Adapter =
			GetWorld()->GetSubsystem<UDarkwellSightWeaveWorldSubsystem>();
		const bool bUseSightWeave = Adapter && Adapter->IsSightWeaveAuthorityActive();
		int32 ThreatIndex = 0;
		for (TActorIterator<ADarkwellStalkerCharacter> It(GetWorld()); It; ++It)
		{
			FDarkwellVisibilitySubjectSnapshot SightWeaveSnapshot;
			const bool bThreatVisible = bUseSightWeave
				? Adapter->TryGetSubjectSnapshot(It->GetPersistentId(), SightWeaveSnapshot)
					&& SightWeaveSnapshot.bHardLive
					&& SightWeaveSnapshot.AuthorityRevision
						== It->GetAppliedVisibilityAuthorityRevision()
				: (!Visibility
					|| Visibility->IsWorldLocationCurrentlyVisible(It->GetActorLocation()));
			if (!It->IsAlive() || ThreatIndex >= 4 || !bThreatVisible)
			{
				continue;
			}

			const FGameplayTag EnemyState = It->GetBehaviorState();
			const ADarkwellStalkerController* EnemyController =
				Cast<ADarkwellStalkerController>(It->GetController());
			const float LanternStunBuildup = EnemyController
				? EnemyController->GetLanternStunBuildup()
				: 0.0f;
			FString StateLabel = TEXT("IDLE");
			FLinearColor StateColor(0.58f, 0.58f, 0.58f);
			if (EnemyState == DarkwellGameplayTags::State_Enemy_Hunting)
			{
				StateLabel = TEXT("HUNTING");
				StateColor = FLinearColor(1.0f, 0.08f, 0.03f);
			}
			else if (EnemyState == DarkwellGameplayTags::State_Enemy_Investigating)
			{
				StateLabel = TEXT("INVESTIGATING");
				StateColor = FLinearColor(1.0f, 0.45f, 0.05f);
			}
			else if (EnemyState == DarkwellGameplayTags::State_Enemy_Repelled)
			{
				StateLabel = TEXT("HELD AT BAY");
				StateColor = FLinearColor(0.25f, 0.65f, 1.0f);
			}
			else if (EnemyState == DarkwellGameplayTags::State_Enemy_LightStunned)
			{
				StateLabel = TEXT("LIGHT-STUNNED");
				if (EnemyController)
				{
					StateLabel = FString::Printf(
						TEXT("LIGHT-STUNNED  %.1fs"),
						EnemyController->GetLanternStunSecondsRemaining());
				}
				StateColor = FLinearColor(0.72f, 0.92f, 1.0f);
			}

			const float ThreatY = 35.0f + static_cast<float>(ThreatIndex) * 58.0f;
			DrawText(
				FString::Printf(
					TEXT("THREAT  %s  %s"),
					*It->GetThreatName().ToString(),
					*StateLabel),
				StateColor,
				35.0f,
				ThreatY,
				Font,
				1.1f,
				false);
			if (LanternStunBuildup > UE_KINDA_SMALL_NUMBER)
			{
				constexpr float StunBarWidth = 190.0f;
				DrawText(
					FString::Printf(TEXT("LANTERN STUN  %d%%"), FMath::RoundToInt(LanternStunBuildup * 100.0f)),
					FLinearColor(0.62f, 0.86f, 1.0f),
					35.0f,
					ThreatY + 22.0f,
					Font,
					0.9f,
					false);
				DrawRect(
					FLinearColor(0.02f, 0.04f, 0.08f, 0.9f),
					35.0f,
					ThreatY + 41.0f,
					StunBarWidth,
					8.0f);
				DrawRect(
					FLinearColor(0.35f, 0.78f, 1.0f),
					37.0f,
					ThreatY + 43.0f,
					(StunBarWidth - 4.0f) * LanternStunBuildup,
					4.0f);
			}
			++ThreatIndex;
		}
	}

	const ADarkwellGameState* MissionGameState = GetWorld()
		? GetWorld()->GetGameState<ADarkwellGameState>()
		: nullptr;
	if (Font && MissionGameState)
	{
		const FString Objective = Darkwell::PropLab::IsLabWorld(GetWorld())
			? TEXT("PROP GAMEPLAY LAB  |  Darkwell.PropLab help  |  MOVING MEMORY: SpatialEvidenceOnly") : FString::Printf(
			TEXT("OBJECTIVE  %s"),
			*MissionGameState->GetObjectiveText().ToString());
		float Width = 0.0f;
		float Height = 0.0f;
		GetTextSize(Objective, Width, Height, Font, 1.05f);
		DrawText(
			Objective,
			MissionGameState->IsFuseCollected()
				? FLinearColor(0.25f, 1.0f, 0.36f)
				: FLinearColor(1.0f, 0.72f, 0.16f),
			(Canvas->ClipX - Width) * 0.5f,
			35.0f,
			Font,
			1.05f,
			false);
	}

	if (Font && GetGameInstance())
	{
		if (const UDarkwellSaveSubsystem* SaveSubsystem = GetGameInstance()->GetSubsystem<UDarkwellSaveSubsystem>())
		{
			const FText SaveStatus = SaveSubsystem->GetStatusMessage();
			if (!SaveStatus.IsEmpty())
			{
				float Width = 0.0f;
				float Height = 0.0f;
				GetTextSize(SaveStatus.ToString(), Width, Height, Font, 1.0f);
				DrawText(
					SaveStatus.ToString(),
					FLinearColor(0.35f, 0.85f, 1.0f),
					(Canvas->ClipX - Width) * 0.5f,
					62.0f,
					Font,
					1.0f,
					false);
			}
		}
	}

	if (Font)
	{
		DrawText(
			TEXT("WASD MOVE   SHIFT SPRINT   MOUSE AIM   LMB TAP FIRE / HOLD AIM   RMB TAP/HOLD TOOL   R RELOAD   Q/E WHEELS   F INTERACT   TAB BACKPACK   F5 SAVE   F9 LOAD"),
			FLinearColor(0.55f, 0.58f, 0.62f),
			35.0f,
			Canvas->ClipY - 42.0f,
			Font,
			0.9f,
			false);
	}

	const EDarkwellWeaponWheelSide ActiveWheel = Character->GetActiveWeaponWheel();
	if (Font && ActiveWheel != EDarkwellWeaponWheelSide::None)
	{
		const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
		constexpr float Radius = 165.0f;
		const FLinearColor WheelColor = ActiveWheel == EDarkwellWeaponWheelSide::Left
			? FLinearColor(0.95f, 0.55f, 0.12f)
			: FLinearColor(0.25f, 0.65f, 1.0f);

		DrawRect(
			FLinearColor(0.01f, 0.012f, 0.018f, 0.82f),
			Center.X - Radius - 35.0f,
			Center.Y - Radius - 35.0f,
			(Radius + 35.0f) * 2.0f,
			(Radius + 35.0f) * 2.0f);

		constexpr int32 SegmentCount = 64;
		for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
		{
			const float AngleA = UE_TWO_PI * static_cast<float>(SegmentIndex) / SegmentCount;
			const float AngleB = UE_TWO_PI * static_cast<float>(SegmentIndex + 1) / SegmentCount;
			const FVector2D PointA = Center + FVector2D(FMath::Cos(AngleA), FMath::Sin(AngleA)) * Radius;
			const FVector2D PointB = Center + FVector2D(FMath::Cos(AngleB), FMath::Sin(AngleB)) * Radius;
			FCanvasLineItem RingSegment(PointA, PointB);
			RingSegment.SetColor(WheelColor);
			RingSegment.LineThickness = 3.0f;
			Canvas->DrawItem(RingSegment);
		}

		auto DrawCenteredAt = [this, Font](
			const FString& Text,
			const float CenterX,
			const float Y,
			const FLinearColor& Color,
			const float Scale)
		{
			float Width = 0.0f;
			float Height = 0.0f;
			GetTextSize(Text, Width, Height, Font, Scale);
			DrawText(Text, Color, CenterX - Width * 0.5f, Y, Font, Scale, false);
		};

		const bool bLeftWheel = ActiveWheel == EDarkwellWeaponWheelSide::Left;
		DrawCenteredAt(
			bLeftWheel ? TEXT("LEFT-HAND WEAPON WHEEL") : TEXT("RIGHT-HAND TOOL WHEEL"),
			Center.X,
			Center.Y - Radius - 22.0f,
			WheelColor,
			1.2f);

		if (bLeftWheel)
		{
			DrawCenteredAt(TEXT("SHOTGUN"), Center.X, Center.Y - 28.0f, FLinearColor::White, 1.35f);
			DrawCenteredAt(TEXT("EQUIPPED"), Center.X, Center.Y + 2.0f, WheelColor, 0.95f);
			DrawCenteredAt(TEXT("MORE FIREARMS - FUTURE"), Center.X, Center.Y + 92.0f, FLinearColor(0.42f, 0.44f, 0.48f), 0.82f);
		}
		else
		{
			const bool bTorchEquipped = Loadout
				&& Loadout->GetEquippedRightHandItem() == DarkwellGameplayTags::Equipment_Right_Torch;
			FCanvasLineItem Divider(
				FVector2D(Center.X, Center.Y - Radius + 18.0f),
				FVector2D(Center.X, Center.Y + Radius - 18.0f));
			Divider.SetColor(FLinearColor(0.3f, 0.34f, 0.42f));
			Divider.LineThickness = 2.0f;
			Canvas->DrawItem(Divider);

			DrawCenteredAt(TEXT("TORCH"), Center.X - 82.0f, Center.Y - 20.0f, bTorchEquipped ? FLinearColor::White : FLinearColor(0.62f, 0.65f, 0.7f), 1.2f);
			DrawCenteredAt(bTorchEquipped ? TEXT("EQUIPPED") : TEXT("AVAILABLE"), Center.X - 82.0f, Center.Y + 10.0f, bTorchEquipped ? WheelColor : FLinearColor(0.42f, 0.45f, 0.5f), 0.9f);
			DrawCenteredAt(TEXT("LANTERN"), Center.X + 82.0f, Center.Y - 20.0f, bTorchEquipped ? FLinearColor(0.62f, 0.65f, 0.7f) : FLinearColor::White, 1.2f);
			DrawCenteredAt(bTorchEquipped ? TEXT("AVAILABLE") : TEXT("EQUIPPED"), Center.X + 82.0f, Center.Y + 10.0f, bTorchEquipped ? FLinearColor(0.42f, 0.45f, 0.5f) : WheelColor, 0.9f);
		}

		DrawCenteredAt(
			bLeftWheel ? TEXT("HOLD Q") : TEXT("RELEASE E TO SWITCH"),
			Center.X,
			Center.Y + Radius - 32.0f,
			FLinearColor(0.68f, 0.7f, 0.74f),
			0.85f);
	}

	if (Character->IsInventoryOpen())
	{
		DrawInventoryInterface();
	}

	const UDarkwellInteractionComponent* Interaction = Character->GetInteractionComponent();
	const FText Prompt = Interaction ? Interaction->GetFocusedPrompt() : FText::GetEmpty();
	if (Font && !Prompt.IsEmpty())
	{
		const FString PromptString = FString::Printf(TEXT("[F]  %s"), *Prompt.ToString());
		float Width = 0.0f;
		float Height = 0.0f;
		GetTextSize(PromptString, Width, Height, Font, 1.25f);
		DrawText(
			PromptString,
			FLinearColor(1.0f, 0.68f, 0.2f),
			(Canvas->ClipX - Width) * 0.5f,
			Canvas->ClipY * 0.72f,
			Font,
			1.25f,
			false);
	}

	const float DamageFeedbackAlpha = Character->GetDamageFeedbackAlpha();
	if (DamageFeedbackAlpha > 0.0f)
	{
		DrawRect(
			FLinearColor(0.72f, 0.0f, 0.0f, 0.28f * DamageFeedbackAlpha),
			0.0f,
			0.0f,
			Canvas->ClipX,
			Canvas->ClipY);
	}

	if (Font && Character->HasEscaped())
	{
		DrawRect(FLinearColor(0.0f, 0.018f, 0.006f, 0.78f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);

		auto DrawVictoryText = [this, Font](
			const FString& Text,
			const float Y,
			const FLinearColor& Color,
			const float Scale)
		{
			float Width = 0.0f;
			float Height = 0.0f;
			GetTextSize(Text, Width, Height, Font, Scale);
			DrawText(Text, Color, (Canvas->ClipX - Width) * 0.5f, Y, Font, Scale, false);
		};

		DrawVictoryText(
			TEXT("YOU ESCAPED"),
			Canvas->ClipY * 0.41f,
			FLinearColor(0.18f, 1.0f, 0.32f),
			2.8f);
		DrawVictoryText(
			TEXT("[R]  PLAY AGAIN"),
			Canvas->ClipY * 0.53f,
			FLinearColor(0.92f, 0.94f, 0.86f),
			1.25f);
	}
	else if (Font && !Character->IsAlive())
	{
		DrawRect(FLinearColor(0.012f, 0.0f, 0.0f, 0.76f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);

		auto DrawDeathText = [this, Font](
			const FString& Text,
			const float Y,
			const FLinearColor& Color,
			const float Scale)
		{
			float Width = 0.0f;
			float Height = 0.0f;
			GetTextSize(Text, Width, Height, Font, Scale);
			DrawText(Text, Color, (Canvas->ClipX - Width) * 0.5f, Y, Font, Scale, false);
		};

		DrawDeathText(
			TEXT("YOU DIED"),
			Canvas->ClipY * 0.41f,
			FLinearColor(0.9f, 0.035f, 0.02f),
			2.8f);
		DrawDeathText(
			TEXT("[R]  RESTART"),
			Canvas->ClipY * 0.53f,
			FLinearColor(0.92f, 0.9f, 0.82f),
			1.25f);
	}
}

void ADarkwellHUD::UpdateFogOfWar(
	ADarkwellCharacter& Character,
	const FIntPoint& ViewportSize,
	const float DeltaSeconds)
{
	UDarkwellVisibilityComponent* Visibility = Character.GetVisibilityComponent();
	if (!PlayerOwner || !Visibility || ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		return;
	}
	if (!EnsureFogComposite(Character))
	{
		return;
	}
	SetFogCompositeWeight(&Character, 1.0f);

	FogUpdateTimeRemaining -= DeltaSeconds;
	if (FogUpdateTimeRemaining > 0.0f)
	{
		return;
	}
	constexpr float FogUpdateIntervalSeconds = 1.0f / 30.0f;
	FogUpdateTimeRemaining = FogUpdateIntervalSeconds;

	const float PlaneHeight = Character.GetActorLocation().Z;
	auto DeprojectToPlane = [this, PlaneHeight](const FVector2D& ScreenPoint, FVector& OutWorldPoint)
	{
		FVector RayOrigin;
		FVector RayDirection;
		return PlayerOwner->DeprojectScreenPositionToWorld(
			ScreenPoint.X,
			ScreenPoint.Y,
			RayOrigin,
			RayDirection)
			&& Darkwell::PlayerMath::TryIntersectHorizontalPlane(
				RayOrigin,
				RayDirection,
				PlaneHeight,
				OutWorldPoint);
	};

	const FVector2D ScreenCorners[] =
	{
		FVector2D(0.0f, 0.0f),
		FVector2D(ViewportSize.X, 0.0f),
		FVector2D(0.0f, ViewportSize.Y),
		FVector2D(ViewportSize.X, ViewportSize.Y)
	};
	FVector WorldCorners[UE_ARRAY_COUNT(ScreenCorners)];
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ScreenCorners); ++Index)
	{
		if (!DeprojectToPlane(ScreenCorners[Index], WorldCorners[Index]))
		{
			return;
		}
	}

	const int32 DesiredTextureWidth = FMath::Clamp(
		FMath::CeilToInt(ViewportSize.X * 0.5f),
		320,
		1024);
	const int32 DesiredTextureHeight = FMath::Clamp(
		FMath::CeilToInt(ViewportSize.Y * 0.5f),
		180,
		576);
	const bool bTextureSizeChanged = !FogTexture
		|| FogTextureWidth != DesiredTextureWidth
		|| FogTextureHeight != DesiredTextureHeight;
	if (bTextureSizeChanged)
	{
		FogTexture = UTexture2D::CreateTransient(
			DesiredTextureWidth,
			DesiredTextureHeight,
			PF_B8G8R8A8);
		if (!FogTexture)
		{
			return;
		}

		FogTexture->SRGB = false;
		FogTexture->Filter = TF_Bilinear;
		FogTexture->AddressX = TA_Clamp;
		FogTexture->AddressY = TA_Clamp;
		FogTexture->NeverStream = true;
		FogTexture->UpdateResource();
		FogTextureWidth = DesiredTextureWidth;
		FogTextureHeight = DesiredTextureHeight;
		FogTexturePixels.Init(FColor(0, 0, 0, 255), FogTextureWidth * FogTextureHeight);
		FogRememberedCoverage.Init(0.0f, FogTextureWidth * FogTextureHeight);
		FogRememberedScratch.Init(0.0f, FogTextureWidth * FogTextureHeight);
		bFogMemoryProjectionValid = false;
		FogCompositeMID->SetTextureParameterValue(TEXT("FogMaskTexture"), FogTexture);
	}
	const int32 FogPixelCount = FogTextureWidth * FogTextureHeight;
	if (FogTexturePixels.Num() != FogPixelCount
		|| FogRememberedCoverage.Num() != FogPixelCount
		|| FogRememberedScratch.Num() != FogPixelCount)
	{
		FogTexturePixels.Init(FColor(0, 0, 0, 255), FogPixelCount);
		FogRememberedCoverage.Init(0.0f, FogPixelCount);
		FogRememberedScratch.Init(0.0f, FogPixelCount);
		bFogMemoryProjectionValid = false;
	}

	constexpr int32 OcclusionSampleCount = 1024;
	Visibility->BuildVisualOcclusionRanges(OcclusionSampleCount, FogOcclusionRanges);
	FDarkwellVisionPresentationState VisionState;
	Visibility->BuildVisualPresentationState(VisionState);
	const float MaximumWorldPixelWidth = FMath::Max(
		FVector::Distance(WorldCorners[0], WorldCorners[1]),
		FVector::Distance(WorldCorners[2], WorldCorners[3])) / FogTextureWidth;
	const float MaximumWorldPixelHeight = FMath::Max(
		FVector::Distance(WorldCorners[0], WorldCorners[2]),
		FVector::Distance(WorldCorners[1], WorldCorners[3])) / FogTextureHeight;
	const float WorldPixelRadius = 0.5f * FMath::Sqrt(
		FMath::Square(MaximumWorldPixelWidth) + FMath::Square(MaximumWorldPixelHeight));
	const float VisionEdgeFeatherWorldUnits = FMath::Max(18.0f, WorldPixelRadius * 2.0f);
	auto SampleOcclusionDistance = [this](float SamplePosition)
	{
		const int32 SampleCount = FogOcclusionRanges.Num();
		SamplePosition = FMath::Fmod(SamplePosition, static_cast<float>(SampleCount));
		if (SamplePosition < 0.0f)
		{
			SamplePosition += SampleCount;
		}
		const int32 FirstIndex = FMath::FloorToInt(SamplePosition) % SampleCount;
		const int32 SecondIndex = (FirstIndex + 1) % SampleCount;
		const float Alpha = SamplePosition - FMath::Floor(SamplePosition);
		return FMath::Lerp(
			FogOcclusionRanges[FirstIndex],
			FogOcclusionRanges[SecondIndex],
			Alpha);
	};
	auto EvaluateRevealWithTolerance = [this, &VisionState, &SampleOcclusionDistance](
		const FVector2D& WorldPoint,
		const float RevealTolerance)
	{
		const FVector2D Delta = WorldPoint - VisionState.SightOrigin;
		const float Distance = Delta.Size();
		if (Distance > VisionState.SightRange + RevealTolerance)
		{
			return false;
		}

		const float SightMargin = VisionState.EvaluateSightMarginFromDelta(Delta, Distance);
		const float LitSightMargin = FMath::Min(
			SightMargin,
			VisionState.EvaluateIlluminationMargin(WorldPoint));
		const float FinalMargin = FMath::Max(
			VisionState.EvaluateAwarenessMarginFromDistance(Distance),
			LitSightMargin);
		if (FinalMargin < -RevealTolerance || Distance <= UE_KINDA_SMALL_NUMBER)
		{
			return FinalMargin >= -RevealTolerance;
		}

		float NormalizedAngle = FMath::Atan2(Delta.Y, Delta.X) / UE_TWO_PI;
		if (NormalizedAngle < 0.0f)
		{
			NormalizedAngle += 1.0f;
		}
		return FogOcclusionRanges.Num() < 3
			|| SampleOcclusionDistance(NormalizedAngle * FogOcclusionRanges.Num())
				>= Distance - RevealTolerance;
	};

	// Record the current screen-visible silhouette in a separate 10 cm presentation grid.
	// The 100 cm grid remains authoritative for gameplay and AI; it is never used
	// to reconstruct this visible contour.
	const float PresentationCellSize = Visibility->GetPresentationCellSize();
	const float PresentationRecordTolerance = VisionEdgeFeatherWorldUnits
		+ PresentationCellSize * UE_INV_SQRT_2;
	float MinimumWorldX = WorldCorners[0].X;
	float MaximumWorldX = WorldCorners[0].X;
	float MinimumWorldY = WorldCorners[0].Y;
	float MaximumWorldY = WorldCorners[0].Y;
	for (int32 Index = 1; Index < UE_ARRAY_COUNT(WorldCorners); ++Index)
	{
		MinimumWorldX = FMath::Min(MinimumWorldX, WorldCorners[Index].X);
		MaximumWorldX = FMath::Max(MaximumWorldX, WorldCorners[Index].X);
		MinimumWorldY = FMath::Min(MinimumWorldY, WorldCorners[Index].Y);
		MaximumWorldY = FMath::Max(MaximumWorldY, WorldCorners[Index].Y);
	}
	const float PresentationRecordRange = VisionState.SightRange + PresentationRecordTolerance;
	MinimumWorldX = FMath::Max(MinimumWorldX, VisionState.SightOrigin.X - PresentationRecordRange);
	MaximumWorldX = FMath::Min(MaximumWorldX, VisionState.SightOrigin.X + PresentationRecordRange);
	MinimumWorldY = FMath::Max(MinimumWorldY, VisionState.SightOrigin.Y - PresentationRecordRange);
	MaximumWorldY = FMath::Min(MaximumWorldY, VisionState.SightOrigin.Y + PresentationRecordRange);
	const FIntPoint MinimumPresentationCell(
		FMath::FloorToInt(MinimumWorldX / PresentationCellSize),
		FMath::FloorToInt(MinimumWorldY / PresentationCellSize));
	const FIntPoint MaximumPresentationCell(
		FMath::FloorToInt(MaximumWorldX / PresentationCellSize),
		FMath::FloorToInt(MaximumWorldY / PresentationCellSize));
	// Record every 30 Hz mask state rather than sampling memory at a slower rate.
	// Otherwise a turning cone can visibly light a boundary between 10 Hz samples
	// and leave that exact strip unknown after the cone moves away. The tolerance
	// includes the visible mask feather and half a cell diagonal, so every portion
	// of a presentation cell that reached the screen is conservatively retained.
	for (int32 CellY = MinimumPresentationCell.Y; CellY <= MaximumPresentationCell.Y; ++CellY)
	{
		for (int32 CellX = MinimumPresentationCell.X; CellX <= MaximumPresentationCell.X; ++CellX)
		{
			const FIntPoint Cell(CellX, CellY);
			const FVector CellCenter = Darkwell::VisibilityMath::CellToWorldCenter(
				Cell,
				PresentationCellSize,
				PlaneHeight);
			if (EvaluateRevealWithTolerance(
				FVector2D(CellCenter.X, CellCenter.Y),
				PresentationRecordTolerance))
			{
				Visibility->RecordExploredPresentationCell(Cell);
			}
		}
	}
	const uint64 CurrentMemoryRevision = Visibility->GetPresentationMemoryRevision();
	bool bMemoryProjectionMoved = !bFogMemoryProjectionValid;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(WorldCorners) && !bMemoryProjectionMoved; ++Index)
	{
		bMemoryProjectionMoved = !FogMemoryWorldCorners[Index].Equals(WorldCorners[Index], 0.25f);
	}
	const bool bRebuildRememberedMask = bTextureSizeChanged
		|| bMemoryProjectionMoved
		|| CurrentMemoryRevision != FogMemoryRevision;

	auto SampleRememberedCoverage = [Visibility, PresentationCellSize](
		const FVector2D& WorldPoint)
	{
		const FVector2D CenterGrid(
			WorldPoint.X / PresentationCellSize - 0.5f,
			WorldPoint.Y / PresentationCellSize - 0.5f);
		const FIntPoint BaseCell(
			FMath::FloorToInt(CenterGrid.X),
			FMath::FloorToInt(CenterGrid.Y));
		const float AlphaX = CenterGrid.X - BaseCell.X;
		const float AlphaY = CenterGrid.Y - BaseCell.Y;
		auto IsExplored = [Visibility](const FIntPoint& Cell)
		{
			return Visibility->IsPresentationCellExplored(Cell) ? 1.0f : 0.0f;
		};
		const float Bottom = FMath::Lerp(
			IsExplored(BaseCell),
			IsExplored(BaseCell + FIntPoint(1, 0)),
			AlphaX);
		const float Top = FMath::Lerp(
			IsExplored(BaseCell + FIntPoint(0, 1)),
			IsExplored(BaseCell + FIntPoint(1, 1)),
			AlphaX);
		return FMath::SmoothStep(0.35f, 0.65f, FMath::Lerp(Bottom, Top, AlphaY));
	};

	ParallelFor(FogTextureHeight, [
		this,
		&WorldCorners,
		&VisionState,
		&SampleOcclusionDistance,
		&SampleRememberedCoverage,
		VisionEdgeFeatherWorldUnits,
		WorldPixelRadius,
		bRebuildRememberedMask](const int32 Y)
	{
		const float V = (static_cast<float>(Y) + 0.5f) / FogTextureHeight;
		const FVector WorldLeft = FMath::Lerp(WorldCorners[0], WorldCorners[2], V);
		const FVector WorldRight = FMath::Lerp(WorldCorners[1], WorldCorners[3], V);
		const FVector WorldStep = (WorldRight - WorldLeft) / FogTextureWidth;
		FVector WorldPoint = WorldLeft + WorldStep * 0.5f;
		for (int32 X = 0; X < FogTextureWidth; ++X)
		{
			const FVector2D WorldPoint2D(WorldPoint.X, WorldPoint.Y);
			const FVector2D VisionDelta = WorldPoint2D - VisionState.SightOrigin;
			const float VisionDistance = VisionDelta.Size();
			float CurrentReveal = 0.0f;
			if (VisionDistance <= VisionState.SightRange + VisionEdgeFeatherWorldUnits)
			{
				float FinalMaskMargin =
					VisionState.EvaluateAwarenessMarginFromDistance(VisionDistance);
				const float SightMargin = VisionState.EvaluateSightMarginFromDelta(
					VisionDelta,
					VisionDistance);
				if (SightMargin > -VisionEdgeFeatherWorldUnits)
				{
					const float IlluminationMargin = VisionState.EvaluateIlluminationMargin(WorldPoint2D);
					FinalMaskMargin = FMath::Max(
						FinalMaskMargin,
						FMath::Min(SightMargin, IlluminationMargin));
				}
				if (FinalMaskMargin > -VisionEdgeFeatherWorldUnits
					&& VisionDistance > UE_KINDA_SMALL_NUMBER && FogOcclusionRanges.Num() >= 3)
				{
					float NormalizedAngle = FMath::Atan2(VisionDelta.Y, VisionDelta.X) / UE_TWO_PI;
					if (NormalizedAngle < 0.0f)
					{
						NormalizedAngle += 1.0f;
					}
					const float CenterSamplePosition = NormalizedAngle * FogOcclusionRanges.Num();
					const float AngularPixelRadius = FMath::Atan2(
						WorldPixelRadius,
						VisionDistance) * FogOcclusionRanges.Num() / UE_TWO_PI;
					float RevealSum = 0.0f;
					for (const float SampleOffset :
						{ -AngularPixelRadius, 0.0f, AngularPixelRadius })
					{
						const float OcclusionMargin = SampleOcclusionDistance(
							CenterSamplePosition + SampleOffset) - VisionDistance;
						RevealSum += FMath::SmoothStep(
							-VisionEdgeFeatherWorldUnits,
							VisionEdgeFeatherWorldUnits,
							FMath::Min(FinalMaskMargin, OcclusionMargin));
					}
					CurrentReveal = RevealSum / 3.0f;
				}
				else if (FinalMaskMargin > -VisionEdgeFeatherWorldUnits)
				{
					CurrentReveal = FMath::SmoothStep(
						-VisionEdgeFeatherWorldUnits,
						VisionEdgeFeatherWorldUnits,
						FinalMaskMargin);
				}
			}
			const int32 PixelIndex = Y * FogTextureWidth + X;
			FColor& Pixel = FogTexturePixels[PixelIndex];
			Pixel.R = static_cast<uint8>(FMath::RoundToInt(
				255.0f * FMath::Clamp(CurrentReveal, 0.0f, 1.0f)));
			Pixel.B = 0;
			Pixel.A = 255;
			if (bRebuildRememberedMask)
			{
				FogRememberedCoverage[PixelIndex] = SampleRememberedCoverage(WorldPoint2D);
			}
			WorldPoint += WorldStep;
		}
	});

	// The sparse 10 cm memory field is exact but its binary contour still forms
	// visible stairs at shallow angles. Three small separable box passes produce
	// a Gaussian-like continuous field in linear time; a narrow, inward-biased
	// threshold turns that field back into a crisp one-pixel presentation edge.
	// This changes presentation only and never writes authoritative knowledge.
	constexpr int32 MemoryContourRadius = 3;
	constexpr int32 MemoryContourPassCount = 3;
	const int32 MemoryContourWindow = MemoryContourRadius * 2 + 1;
	auto BlurRememberedHorizontal = [this, MemoryContourRadius, MemoryContourWindow](
		const TArray<float>& Source,
		TArray<float>& Destination)
	{
		ParallelFor(FogTextureHeight, [this, &Source, &Destination, MemoryContourRadius, MemoryContourWindow](const int32 Y)
		{
			float Sum = 0.0f;
			for (int32 Offset = -MemoryContourRadius; Offset <= MemoryContourRadius; ++Offset)
			{
				Sum += Source[
					Y * FogTextureWidth + FMath::Clamp(Offset, 0, FogTextureWidth - 1)];
			}
			for (int32 X = 0; X < FogTextureWidth; ++X)
			{
				Destination[Y * FogTextureWidth + X] = Sum / MemoryContourWindow;
				const int32 RemoveX = FMath::Clamp(
					X - MemoryContourRadius,
					0,
					FogTextureWidth - 1);
				const int32 AddX = FMath::Clamp(
					X + MemoryContourRadius + 1,
					0,
					FogTextureWidth - 1);
				Sum += Source[Y * FogTextureWidth + AddX]
					- Source[Y * FogTextureWidth + RemoveX];
			}
		});
	};
	auto BlurRememberedVertical = [this, MemoryContourRadius, MemoryContourWindow](
		const TArray<float>& Source,
		TArray<float>& Destination)
	{
		ParallelFor(FogTextureWidth, [this, &Source, &Destination, MemoryContourRadius, MemoryContourWindow](const int32 X)
		{
			float Sum = 0.0f;
			for (int32 Offset = -MemoryContourRadius; Offset <= MemoryContourRadius; ++Offset)
			{
				Sum += Source[
					FMath::Clamp(Offset, 0, FogTextureHeight - 1) * FogTextureWidth + X];
			}
			for (int32 Y = 0; Y < FogTextureHeight; ++Y)
			{
				Destination[Y * FogTextureWidth + X] = Sum / MemoryContourWindow;
				const int32 RemoveY = FMath::Clamp(
					Y - MemoryContourRadius,
					0,
					FogTextureHeight - 1);
				const int32 AddY = FMath::Clamp(
					Y + MemoryContourRadius + 1,
					0,
					FogTextureHeight - 1);
				Sum += Source[AddY * FogTextureWidth + X]
					- Source[RemoveY * FogTextureWidth + X];
			}
		});
	};
	if (bRebuildRememberedMask)
	{
		for (int32 PassIndex = 0; PassIndex < MemoryContourPassCount; ++PassIndex)
		{
			BlurRememberedHorizontal(FogRememberedCoverage, FogRememberedScratch);
			BlurRememberedVertical(FogRememberedScratch, FogRememberedCoverage);
		}
		ParallelFor(FogTexturePixels.Num(), [this](const int32 PixelIndex)
		{
			const float ContinuousRememberedCoverage = FMath::SmoothStep(
				0.54f,
				0.60f,
				FogRememberedCoverage[PixelIndex]);
			FogTexturePixels[PixelIndex].G = static_cast<uint8>(FMath::RoundToInt(
				255.0f * ContinuousRememberedCoverage));
		});
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(WorldCorners); ++Index)
		{
			FogMemoryWorldCorners[Index] = WorldCorners[Index];
		}
		FogMemoryRevision = CurrentMemoryRevision;
		bFogMemoryProjectionValid = true;
	}

	const uint32 SourcePitch = FogTextureWidth * sizeof(FColor);
	const SIZE_T UploadSize = static_cast<SIZE_T>(SourcePitch) * FogTextureHeight;
	uint8* UploadData = static_cast<uint8*>(FMemory::Malloc(UploadSize));
	FMemory::Memcpy(UploadData, FogTexturePixels.GetData(), UploadSize);
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(
		0,
		0,
		0,
		0,
		FogTextureWidth,
		FogTextureHeight);
	FogTexture->UpdateTextureRegions(
		0,
		1,
		Region,
		SourcePitch,
		sizeof(FColor),
		UploadData,
		[](uint8* SourceData, const FUpdateTextureRegion2D* SourceRegions)
		{
			FMemory::Free(SourceData);
			delete SourceRegions;
		});

}

bool ADarkwellHUD::EnsureFogComposite(ADarkwellCharacter& Character)
{
	UCameraComponent* Camera = Character.GetTopDownCamera();
	if (!Camera || !FogCompositeMaterial)
	{
		return false;
	}
	if (!FogCompositeMID)
	{
		FogCompositeMID = UMaterialInstanceDynamic::Create(FogCompositeMaterial, this);
		if (!FogCompositeMID)
		{
			return false;
		}
	}
	if (FogCompositeCamera.Get() != Camera)
	{
		if (UCameraComponent* PreviousCamera = FogCompositeCamera.Get())
		{
			PreviousCamera->AddOrUpdateBlendable(FogCompositeMID, 0.0f);
		}
		Camera->AddOrUpdateBlendable(FogCompositeMID, 1.0f);
		FogCompositeCamera = Camera;
		bFogMemoryProjectionValid = false;
	}
	if (FogTexture)
	{
		FogCompositeMID->SetTextureParameterValue(TEXT("FogMaskTexture"), FogTexture);
	}
	return true;
}

void ADarkwellHUD::SetFogCompositeWeight(ADarkwellCharacter* Character, const float Weight)
{
	UCameraComponent* Camera = Character ? Character->GetTopDownCamera() : FogCompositeCamera.Get();
	if (Camera && FogCompositeMID)
	{
		Camera->AddOrUpdateBlendable(FogCompositeMID, Weight);
		FogCompositeCamera = Camera;
	}
}

void ADarkwellHUD::DrawMenuInterface()
{
	ADarkwellPlayerController* DarkwellController = Cast<ADarkwellPlayerController>(PlayerOwner);
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Canvas || !DarkwellController || !Font)
	{
		return;
	}

	const TArray<FDarkwellMenuEntry> Entries = BuildMenuEntries(*DarkwellController);
	const float PanelHeight = MenuHeaderHeight + Entries.Num() * MenuButtonHeight
		+ FMath::Max(0, Entries.Num() - 1) * MenuButtonGap + 44.0f;
	const FVector2D PanelOrigin((Canvas->ClipX - MenuPanelWidth) * 0.5f, (Canvas->ClipY - PanelHeight) * 0.5f);
	DrawRect(FLinearColor(0.003f, 0.006f, 0.01f, 0.9f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
	DrawRect(FLinearColor(0.018f, 0.026f, 0.034f, 0.98f), PanelOrigin.X, PanelOrigin.Y, MenuPanelWidth, PanelHeight);
	DrawRect(FLinearColor(0.08f, 0.72f, 0.62f, 1.0f), PanelOrigin.X, PanelOrigin.Y, MenuPanelWidth, 6.0f);

	FString Title = TEXT("DARKWELL");
	FLinearColor TitleColor(0.74f, 1.0f, 0.91f);
	if (DarkwellController->GetMenuScreen() == EDarkwellMenuScreen::Pause)
	{
		Title = TEXT("PAUSED");
		TitleColor = FLinearColor(1.0f, 0.72f, 0.22f);
	}
	else if (DarkwellController->GetMenuScreen() == EDarkwellMenuScreen::Settings)
	{
		Title = TEXT("SETTINGS");
	}
	float TitleWidth = 0.0f;
	float TitleHeight = 0.0f;
	GetTextSize(Title, TitleWidth, TitleHeight, Font, 2.0f);
	DrawText(Title, TitleColor, (Canvas->ClipX - TitleWidth) * 0.5f, PanelOrigin.Y + 32.0f, Font, 2.0f, false);
	if (DarkwellController->GetMenuScreen() == EDarkwellMenuScreen::Main)
	{
		const FString Subtitle(TEXT("DESCEND. SURVIVE. ESCAPE."));
		float Width = 0.0f;
		float Height = 0.0f;
		GetTextSize(Subtitle, Width, Height, Font, 0.82f);
		DrawText(Subtitle, FLinearColor(0.42f, 0.52f, 0.55f), (Canvas->ClipX - Width) * 0.5f, PanelOrigin.Y + 82.0f, Font, 0.82f, false);
	}

	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FDarkwellMenuEntry& Entry = Entries[Index];
		const FVector2D Origin = GetMenuButtonOrigin(Canvas->ClipX, Canvas->ClipY, Entries.Num(), Index);
		const FLinearColor ButtonColor = Entry.bEnabled
			? FLinearColor(0.055f, 0.13f, 0.15f, 1.0f)
			: FLinearColor(0.035f, 0.04f, 0.045f, 1.0f);
		const FLinearColor TextColor = Entry.bEnabled
			? FLinearColor(0.85f, 0.96f, 0.92f)
			: FLinearColor(0.28f, 0.31f, 0.32f);
		DrawRect(ButtonColor, Origin.X, Origin.Y, MenuButtonWidth, MenuButtonHeight);
		DrawRect(Entry.bEnabled ? FLinearColor(0.08f, 0.72f, 0.62f, 1.0f) : FLinearColor(0.15f, 0.17f, 0.18f, 1.0f), Origin.X, Origin.Y, 5.0f, MenuButtonHeight);
		const FString Label = Entry.Label.ToString();
		float Width = 0.0f;
		float Height = 0.0f;
		GetTextSize(Label, Width, Height, Font, 1.08f);
		DrawText(Label, TextColor, Origin.X + (MenuButtonWidth - Width) * 0.5f, Origin.Y + 17.0f, Font, 1.08f, false);
	}

	if (GetGameInstance())
	{
		if (const UDarkwellSaveSubsystem* SaveSubsystem = GetGameInstance()->GetSubsystem<UDarkwellSaveSubsystem>())
		{
			const FText Status = SaveSubsystem->GetStatusMessage();
			if (!Status.IsEmpty())
			{
				float Width = 0.0f;
				float Height = 0.0f;
				GetTextSize(Status.ToString(), Width, Height, Font, 0.82f);
				DrawText(Status.ToString(), FLinearColor(0.35f, 0.85f, 1.0f), (Canvas->ClipX - Width) * 0.5f, PanelOrigin.Y + PanelHeight - 26.0f, Font, 0.82f, false);
			}
		}
	}
}

bool ADarkwellHUD::HandleMenuPointer(const FVector2D& ScreenPosition)
{
	ADarkwellPlayerController* DarkwellController = Cast<ADarkwellPlayerController>(PlayerOwner);
	if (!DarkwellController || !DarkwellController->IsMenuOpen())
	{
		return false;
	}

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	DarkwellController->GetViewportSize(ViewportWidth, ViewportHeight);
	if (ViewportWidth <= 0 || ViewportHeight <= 0)
	{
		return false;
	}

	const TArray<FDarkwellMenuEntry> Entries = BuildMenuEntries(*DarkwellController);
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FVector2D Origin = GetMenuButtonOrigin(
			static_cast<float>(ViewportWidth),
			static_cast<float>(ViewportHeight),
			Entries.Num(),
			Index);
		if (IsPointInside(ScreenPosition, Origin, FVector2D(MenuButtonWidth, MenuButtonHeight)))
		{
			if (Entries[Index].bEnabled)
			{
				DarkwellController->ExecuteMenuAction(Entries[Index].Action);
			}
			return true;
		}
	}
	return false;
}

bool ADarkwellHUD::HandleInventoryPointer(
	const FVector2D& ScreenPosition,
	const bool bSecondaryClick,
	const bool bControlDown)
{
	ADarkwellPlayerController* DarkwellController = Cast<ADarkwellPlayerController>(PlayerOwner);
	ADarkwellCharacter* Character = DarkwellController ? Cast<ADarkwellCharacter>(DarkwellController->GetPawn()) : nullptr;
	UDarkwellInventoryComponent* PlayerInventory = Character ? Character->GetInventoryComponent() : nullptr;
	if (!DarkwellController || !PlayerInventory || !DarkwellController->IsInventoryOpen())
	{
		return false;
	}

	UDarkwellInventoryComponent* ExternalInventory = DarkwellController->GetExternalInventory();
	ADarkwellWorkbench* Workbench = DarkwellController->GetActiveWorkbench();
	const bool bDualPanel = ExternalInventory || Workbench;
	int32 SlotIndex = INDEX_NONE;
	if (FindInventorySlotAt(
		ScreenPosition,
		GetBackpackPanelOrigin(bDualPanel),
		PlayerInventory->GetSlotCapacity(),
		SlotIndex))
	{
		const FDarkwellItemStack* Stack = PlayerInventory->GetSlot(SlotIndex);
		bool bSucceeded = false;
		if (bControlDown && ExternalInventory && Stack && !Stack->IsEmpty())
		{
			DarkwellController->ClearInventorySlotSelection();
			bSucceeded = PlayerInventory->TransferSlotTo(*ExternalInventory, SlotIndex) > 0;
		}
		else if (bSecondaryClick && Stack && !Stack->IsEmpty())
		{
			DarkwellController->ClearInventorySlotSelection();
			bSucceeded = PlayerInventory->SplitStack(SlotIndex);
		}
		else if (!bSecondaryClick && !bControlDown)
		{
			DarkwellController->SelectOrMoveInventorySlot(PlayerInventory, SlotIndex);
			return true;
		}

		if (bControlDown || bSecondaryClick)
		{
			DarkwellController->SetInventoryMessage(bSucceeded
				? NSLOCTEXT("Darkwell", "InventoryActionComplete", "Inventory updated")
				: NSLOCTEXT("Darkwell", "InventoryActionBlocked", "No room or stack cannot be split"));
		}
		return true;
	}

	if (ExternalInventory && FindInventorySlotAt(
		ScreenPosition,
		GetContextPanelOrigin(),
		ExternalInventory->GetSlotCapacity(),
		SlotIndex))
	{
		const FDarkwellItemStack* Stack = ExternalInventory->GetSlot(SlotIndex);
		if (bControlDown && Stack && !Stack->IsEmpty())
		{
			DarkwellController->ClearInventorySlotSelection();
			const bool bMoved = ExternalInventory->TransferSlotTo(*PlayerInventory, SlotIndex) > 0;
			DarkwellController->SetInventoryMessage(bMoved
				? NSLOCTEXT("Darkwell", "ContainerTransferComplete", "Items transferred")
				: NSLOCTEXT("Darkwell", "ContainerTransferBlocked", "Backpack has no room"));
		}
		else if (bSecondaryClick && Stack && !Stack->IsEmpty())
		{
			DarkwellController->ClearInventorySlotSelection();
			const bool bSplit = ExternalInventory->SplitStack(SlotIndex);
			DarkwellController->SetInventoryMessage(bSplit
				? NSLOCTEXT("Darkwell", "ContainerSplitComplete", "Stack split in container")
				: NSLOCTEXT("Darkwell", "ContainerSplitBlocked", "No free container slot or stack cannot be split"));
		}
		else if (!bSecondaryClick && !bControlDown)
		{
			DarkwellController->SelectOrMoveInventorySlot(ExternalInventory, SlotIndex);
		}
		return true;
	}

	const FVector2D ContextOrigin = GetContextPanelOrigin();
	if (ExternalInventory && !bSecondaryClick
		&& IsPointInside(
			ScreenPosition,
			ContextOrigin + FVector2D(29.0f, InventoryPanelHeight - 63.0f),
			FVector2D(InventoryPanelWidth - 58.0f, 38.0f)))
	{
		DarkwellController->TakeAllFromContainer();
		return true;
	}

	if (Workbench && !bSecondaryClick
		&& IsPointInside(
			ScreenPosition,
			ContextOrigin + FVector2D(29.0f, 145.0f),
			FVector2D(InventoryPanelWidth - 58.0f, 104.0f)))
	{
		DarkwellController->TryCraftAtWorkbench();
		return true;
	}
	return false;
}

void ADarkwellHUD::DrawInventoryInterface()
{
	if (!Canvas || !PlayerOwner)
	{
		return;
	}

	ADarkwellPlayerController* DarkwellController = Cast<ADarkwellPlayerController>(PlayerOwner);
	ADarkwellCharacter* Character = DarkwellController ? Cast<ADarkwellCharacter>(DarkwellController->GetPawn()) : nullptr;
	UDarkwellInventoryComponent* PlayerInventory = Character ? Character->GetInventoryComponent() : nullptr;
	if (!DarkwellController || !PlayerInventory)
	{
		return;
	}

	UDarkwellInventoryComponent* ExternalInventory = DarkwellController->GetExternalInventory();
	ADarkwellWorkbench* Workbench = DarkwellController->GetActiveWorkbench();
	const bool bDualPanel = ExternalInventory || Workbench;
	DrawRect(FLinearColor(0.005f, 0.007f, 0.011f, 0.76f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);

	DrawInventoryPanel(
		GetBackpackPanelOrigin(bDualPanel),
		FVector2D(InventoryPanelWidth, InventoryPanelHeight),
		NSLOCTEXT("Darkwell", "BackpackPanel", "BACKPACK"),
		*PlayerInventory,
		true);

	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (ExternalInventory)
	{
		const int32 ExternalItemCount = ExternalInventory->GetTotalItemCount();
		const FText ExternalTitle = ExternalItemCount > 0
			? FText::Format(
				NSLOCTEXT("Darkwell", "ContainerPanelWithCount", "{0}  -  {1} ITEMS"),
				DarkwellController->GetInventoryContextTitle(),
				FText::AsNumber(ExternalItemCount))
			: FText::Format(
				NSLOCTEXT("Darkwell", "EmptyContainerPanel", "{0}  -  EMPTY"),
				DarkwellController->GetInventoryContextTitle());
		DrawInventoryPanel(
			GetContextPanelOrigin(),
			FVector2D(InventoryPanelWidth, InventoryPanelHeight),
			ExternalTitle,
			*ExternalInventory,
			false);

		const FVector2D ButtonOrigin = GetContextPanelOrigin() + FVector2D(29.0f, InventoryPanelHeight - 63.0f);
		DrawRect(
			ExternalItemCount > 0 ? FLinearColor(0.12f, 0.35f, 0.48f, 0.95f) : FLinearColor(0.08f, 0.09f, 0.1f, 0.9f),
			ButtonOrigin.X,
			ButtonOrigin.Y,
			InventoryPanelWidth - 58.0f,
			38.0f);
		if (Font)
		{
			DrawText(
				(ExternalItemCount > 0
					? NSLOCTEXT("Darkwell", "TakeAllButton", "TAKE ALL  [T]")
					: NSLOCTEXT("Darkwell", "ContainerEmptyButton", "CONTAINER EMPTY")).ToString(),
				ExternalItemCount > 0 ? FLinearColor::White : FLinearColor(0.4f, 0.42f, 0.44f),
				ButtonOrigin.X + (ExternalItemCount > 0 ? 103.0f : 91.0f),
				ButtonOrigin.Y + 9.0f,
				Font,
				1.0f,
				false);
		}
	}
	else if (Workbench)
	{
		const FVector2D Origin = GetContextPanelOrigin();
		DrawRect(FLinearColor(0.018f, 0.026f, 0.032f, 0.97f), Origin.X, Origin.Y, InventoryPanelWidth, InventoryPanelHeight);
		DrawRect(FLinearColor(0.08f, 0.62f, 0.55f, 0.95f), Origin.X, Origin.Y, InventoryPanelWidth, 5.0f);
		if (Font)
		{
			DrawText(NSLOCTEXT("Darkwell", "ShellWorkbenchPanel", "SHELL WORKBENCH").ToString(), FLinearColor(0.65f, 1.0f, 0.9f), Origin.X + 29.0f, Origin.Y + 27.0f, Font, 1.28f, false);
			DrawText(NSLOCTEXT("Darkwell", "RecipeLabel", "RECIPE").ToString(), FLinearColor(0.52f, 0.58f, 0.6f), Origin.X + 29.0f, Origin.Y + 91.0f, Font, 0.9f, false);
		}

		const FVector2D RecipeOrigin = Origin + FVector2D(29.0f, 145.0f);
		DrawRect(FLinearColor(0.04f, 0.12f, 0.13f, 1.0f), RecipeOrigin.X, RecipeOrigin.Y, InventoryPanelWidth - 58.0f, 104.0f);
		DrawRect(FLinearColor(0.1f, 0.75f, 0.62f, 0.9f), RecipeOrigin.X, RecipeOrigin.Y, 5.0f, 104.0f);
		if (Font)
		{
			const Darkwell::ItemCatalog::FItemSpec& CostSpec = Darkwell::ItemCatalog::GetSpec(Workbench->GetCostItemTag());
			const Darkwell::ItemCatalog::FItemSpec& ResultSpec = Darkwell::ItemCatalog::GetSpec(Workbench->GetResultItemTag());
			constexpr float RecipeIconSize = 72.0f;
			const FVector2D RecipeIconOrigin = RecipeOrigin + FVector2D(15.0f, 16.0f);
			if (UTexture2D* RecipeIcon = ResultSpec.Icon.LoadSynchronous())
			{
				FCanvasTileItem IconItem(RecipeIconOrigin, RecipeIcon->GetResource(), FVector2D(RecipeIconSize), FLinearColor::White);
				IconItem.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(IconItem);
			}
			else
			{
				DrawRect(ResultSpec.Color * 0.65f, RecipeIconOrigin.X, RecipeIconOrigin.Y, RecipeIconSize, RecipeIconSize);
			}

			const float RecipeTextX = RecipeOrigin.X + 102.0f;
			DrawText(
				Workbench->GetRecipeDisplayName().ToString().ToUpper(),
				FLinearColor::White,
				RecipeTextX,
				RecipeOrigin.Y + 17.0f,
				Font,
				1.1f,
				false);
			const FText RecipeExchange = FText::Format(
				NSLOCTEXT("Darkwell", "RecipeExchangeFormat", "{0} {1}  ->  {2} {3}"),
				FText::AsNumber(Workbench->GetScrapCost()),
				CostSpec.ShortName,
				FText::AsNumber(Workbench->GetShellYield()),
				ResultSpec.ShortName);
			DrawText(
				RecipeExchange.ToString(),
				FLinearColor(0.65f, 0.86f, 0.82f),
				RecipeTextX,
				RecipeOrigin.Y + 49.0f,
				Font,
				0.95f,
				false);
			DrawText(NSLOCTEXT("Darkwell", "ClickToCraft", "CLICK TO CRAFT").ToString(), FLinearColor(0.2f, 1.0f, 0.73f), RecipeTextX, RecipeOrigin.Y + 76.0f, Font, 0.88f, false);
		}
	}

	if (Font)
	{
		const FString Help = ExternalInventory
			? TEXT("LMB  SELECT / PLACE     CTRL + LMB  QUICK TRANSFER     RMB  SPLIT HERE     T  TAKE ALL")
			: TEXT("LMB  SELECT / PLACE     RMB  SPLIT STACK     F / TAB / ESC  CLOSE");
		float HelpWidth = 0.0f;
		float HelpHeight = 0.0f;
		GetTextSize(Help, HelpWidth, HelpHeight, Font, 0.92f);
		DrawText(Help, FLinearColor(0.72f, 0.74f, 0.76f), (Canvas->ClipX - HelpWidth) * 0.5f, Canvas->ClipY - 64.0f, Font, 0.92f, false);

		const FText Message = DarkwellController->GetInventoryMessage();
		if (!Message.IsEmpty())
		{
			float MessageWidth = 0.0f;
			float MessageHeight = 0.0f;
			GetTextSize(Message.ToString(), MessageWidth, MessageHeight, Font, 1.05f);
			DrawText(Message.ToString(), FLinearColor(1.0f, 0.72f, 0.2f), (Canvas->ClipX - MessageWidth) * 0.5f, Canvas->ClipY - 105.0f, Font, 1.05f, false);
		}
	}
}

void ADarkwellHUD::DrawInventoryPanel(
	const FVector2D& Origin,
	const FVector2D& Size,
	const FText& Title,
	const UDarkwellInventoryComponent& Inventory,
	const bool bPlayerPanel)
{
	DrawRect(FLinearColor(0.018f, 0.022f, 0.029f, 0.97f), Origin.X, Origin.Y, Size.X, Size.Y);
	DrawRect(
		bPlayerPanel ? FLinearColor(0.88f, 0.42f, 0.09f) : FLinearColor(0.12f, 0.52f, 0.78f),
		Origin.X,
		Origin.Y,
		Size.X,
		5.0f);

	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (Font)
	{
		DrawText(Title.ToString(), FLinearColor(0.9f, 0.92f, 0.9f), Origin.X + 29.0f, Origin.Y + 27.0f, Font, 1.28f, false);
	}

	for (int32 SlotIndex = 0; SlotIndex < Inventory.GetSlotCapacity(); ++SlotIndex)
	{
		const FDarkwellItemStack* Stack = Inventory.GetSlot(SlotIndex);
		if (Stack)
		{
			const ADarkwellPlayerController* DarkwellController = Cast<ADarkwellPlayerController>(PlayerOwner);
			const bool bSelected = DarkwellController
				&& DarkwellController->IsInventorySlotSelected(&Inventory, SlotIndex);
			DrawInventorySlot(Origin, SlotIndex, *Stack, bSelected);
		}
	}
}

void ADarkwellHUD::DrawInventorySlot(
	const FVector2D& Origin,
	const int32 SlotIndex,
	const FDarkwellItemStack& Stack,
	const bool bSelected)
{
	const int32 Column = SlotIndex % InventoryColumns;
	const int32 Row = SlotIndex / InventoryColumns;
	const FVector2D SlotOrigin = Origin + FVector2D(
		InventorySlotStartX + Column * (InventorySlotSize + InventorySlotGap),
		InventorySlotStartY + Row * (InventorySlotSize + InventorySlotGap));
	DrawRect(
		bSelected ? FLinearColor(1.0f, 0.52f, 0.08f, 1.0f) : FLinearColor(0.035f, 0.041f, 0.052f, 1.0f),
		SlotOrigin.X,
		SlotOrigin.Y,
		InventorySlotSize,
		InventorySlotSize);
	if (bSelected)
	{
		DrawRect(FLinearColor(0.035f, 0.041f, 0.052f, 1.0f), SlotOrigin.X + 4.0f, SlotOrigin.Y + 4.0f, InventorySlotSize - 8.0f, InventorySlotSize - 8.0f);
	}
	DrawRect(FLinearColor(0.12f, 0.13f, 0.16f, 0.8f), SlotOrigin.X + 2.0f, SlotOrigin.Y + 2.0f, InventorySlotSize - 4.0f, 2.0f);
	if (Stack.IsEmpty())
	{
		return;
	}

	const Darkwell::ItemCatalog::FItemSpec& Spec = Darkwell::ItemCatalog::GetSpec(Stack.ItemTag);
	const FVector2D IconOrigin = SlotOrigin + FVector2D(7.0f, 7.0f);
	const FVector2D IconSize(InventorySlotSize - 14.0f);
	if (UTexture2D* IconTexture = Spec.Icon.LoadSynchronous())
	{
		FCanvasTileItem IconItem(
			IconOrigin,
			IconTexture->GetResource(),
			IconSize,
			FLinearColor::White);
		IconItem.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(IconItem);
	}
	else
	{
		DrawRect(Spec.Color * 0.55f, IconOrigin.X, IconOrigin.Y, IconSize.X, IconSize.Y);
	}
	DrawRect(FLinearColor(0.01f, 0.014f, 0.02f, 0.82f), IconOrigin.X, SlotOrigin.Y + 48.0f, IconSize.X, 21.0f);
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (Font)
	{
		const FString ShortName = Spec.ShortName.IsEmpty()
			? Spec.DisplayName.ToString().ToUpper().Left(10)
			: Spec.ShortName.ToString().ToUpper().Left(10);
		DrawText(ShortName, FLinearColor::White, SlotOrigin.X + 9.0f, SlotOrigin.Y + 52.0f, Font, 0.72f, false);

		const FString QuantityText = FString::Printf(TEXT("x%d"), Stack.Quantity);
		float QuantityWidth = 0.0f;
		float QuantityHeight = 0.0f;
		GetTextSize(QuantityText, QuantityWidth, QuantityHeight, Font, 0.8f);
		const float QuantityX = SlotOrigin.X + InventorySlotSize - QuantityWidth - 7.0f;
		DrawText(QuantityText, FLinearColor(0.01f, 0.01f, 0.01f, 0.9f), QuantityX + 1.0f, SlotOrigin.Y + 10.0f, Font, 0.8f, false);
		DrawText(
			QuantityText,
			FLinearColor(0.95f, 0.95f, 0.86f),
			QuantityX,
			SlotOrigin.Y + 9.0f,
			Font,
			0.8f,
			false);
	}
}

FVector2D ADarkwellHUD::GetBackpackPanelOrigin(const bool bDualPanel) const
{
	int32 ViewportWidth = 1920;
	int32 ViewportHeight = 1080;
	if (PlayerOwner)
	{
		PlayerOwner->GetViewportSize(ViewportWidth, ViewportHeight);
	}

	const float TotalWidth = bDualPanel
		? InventoryPanelWidth * 2.0f + InventoryPanelGap
		: InventoryPanelWidth;
	return FVector2D(
		static_cast<float>(ViewportWidth) * 0.5f - TotalWidth * 0.5f,
		FMath::Max(75.0f, (static_cast<float>(ViewportHeight) - InventoryPanelHeight) * 0.5f));
}

FVector2D ADarkwellHUD::GetContextPanelOrigin() const
{
	const FVector2D BackpackOrigin = GetBackpackPanelOrigin(true);
	return BackpackOrigin + FVector2D(InventoryPanelWidth + InventoryPanelGap, 0.0f);
}

bool ADarkwellHUD::FindInventorySlotAt(
	const FVector2D& ScreenPosition,
	const FVector2D& PanelOrigin,
	const int32 SlotCount,
	int32& OutSlotIndex) const
{
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		const int32 Column = SlotIndex % InventoryColumns;
		const int32 Row = SlotIndex / InventoryColumns;
		const FVector2D SlotOrigin = PanelOrigin + FVector2D(
			InventorySlotStartX + Column * (InventorySlotSize + InventorySlotGap),
			InventorySlotStartY + Row * (InventorySlotSize + InventorySlotGap));
		if (IsPointInside(ScreenPosition, SlotOrigin, FVector2D(InventorySlotSize, InventorySlotSize)))
		{
			OutSlotIndex = SlotIndex;
			return true;
		}
	}
	return false;
}
