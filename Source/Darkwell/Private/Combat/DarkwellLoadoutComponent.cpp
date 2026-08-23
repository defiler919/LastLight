// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/DarkwellLoadoutComponent.h"

#include "AI/DarkwellStalkerCharacter.h"
#include "AI/DarkwellStalkerController.h"
#include "Combat/DarkwellLoadoutRules.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "Gameplay/DarkwellResourceMath.h"
#include "Inventory/DarkwellInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Hearing.h"
#include "Player/DarkwellCharacter.h"

UDarkwellLoadoutComponent::UDarkwellLoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDarkwellLoadoutComponent::BeginPlay()
{
	Super::BeginPlay();
	LoadedShells = FMath::Clamp(LoadedShells, 0, MaxLoadedShells);
	TorchCharge = FMath::Max(0.0f, TorchCharge);
	TorchHeat = FMath::Clamp(TorchHeat, 0.0f, 100.0f);
	LanternFuel = FMath::Max(0.0f, LanternFuel);
	EquippedLeftHandItem = DarkwellGameplayTags::Equipment_Left_Shotgun;
	EquippedRightHandItem = DarkwellGameplayTags::Equipment_Right_Torch;
	bOwnerIncapacitated = false;
	ClearRightHandActionStates();
	RefreshRightHandPresentation();
	RefreshWeaponState();
	AddReserveShells(StartingReserveShells);
}

void UDarkwellLoadoutComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (IsReloading())
	{
		ReloadTimeRemaining -= DeltaTime;
		if (ReloadTimeRemaining <= 0.0f)
		{
			CompleteReload();
		}
	}


	TickRightHandTool(DeltaTime);
}

void UDarkwellLoadoutComponent::SetRightHandPresentation(
	UStaticMeshComponent* InTorchMesh,
	UPointLightComponent* InTorchLight,
	UStaticMeshComponent* InLanternMesh,
	UPointLightComponent* InLanternBaseLight,
	USpotLightComponent* InLanternFocusLight)
{
	TorchMesh = InTorchMesh;
	TorchLight = InTorchLight;
	LanternMesh = InLanternMesh;
	LanternBaseLight = InLanternBaseLight;
	LanternFocusLight = InLanternFocusLight;
	if (TorchLight && !bHasRaisedTorchPresentation)
	{
		RaisedTorchRelativeLocation = TorchLight->GetRelativeLocation();
		RaisedTorchIntensity = TorchLight->Intensity;
		RaisedTorchRadius = TorchLight->AttenuationRadius;
		bHasRaisedTorchPresentation = true;
	}
	if (TorchMesh)
	{
		RaisedTorchMeshRelativeLocation = TorchMesh->GetRelativeLocation();
		RaisedTorchMeshRelativeRotation = TorchMesh->GetRelativeRotation();
	}
	RefreshRightHandPresentation();
}

bool UDarkwellLoadoutComponent::BeginRightHandUse()
{
	if (bOwnerIncapacitated || IsReloading() || RightHandHeldSeconds >= 0.0f)
	{
		return false;
	}

	const bool bTorchReady = EquippedRightHandItem == DarkwellGameplayTags::Equipment_Right_Torch
		&& TorchCharge > 0.0f && !bTorchOverheated;
	const bool bLanternReady = EquippedRightHandItem == DarkwellGameplayTags::Equipment_Right_Lantern
		&& LanternFuel > 0.0f;
	if (!bTorchReady && !bLanternReady)
	{
		return false;
	}

	RightHandHeldSeconds = 0.0f;
	bHoldActionStarted = false;
	return true;
}

bool UDarkwellLoadoutComponent::EndRightHandUse()
{
	if (RightHandHeldSeconds < 0.0f)
	{
		return false;
	}

	const EDarkwellRightToolGesture Gesture = Darkwell::LoadoutRules::ResolveGesture(
		RightHandHeldSeconds,
		RightHandHoldThreshold);
	const bool bWasHold = bHoldActionStarted || Gesture == EDarkwellRightToolGesture::Hold;
	RightHandHeldSeconds = -1.0f;
	bHoldActionStarted = false;

	if (bWasHold)
	{
		RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Torch_Deterrent);
		RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Lantern_Focused);
		RefreshRightHandPresentation();
		return true;
	}

	if (Gesture == EDarkwellRightToolGesture::Tap)
	{
		if (EquippedRightHandItem == DarkwellGameplayTags::Equipment_Right_Torch)
		{
			TriggerTorchSwing();
		}
		else if (EquippedRightHandItem == DarkwellGameplayTags::Equipment_Right_Lantern)
		{
			TriggerLanternFlash();
		}
		return true;
	}
	return false;
}

void UDarkwellLoadoutComponent::CancelRightHandUse()
{
	RightHandHeldSeconds = -1.0f;
	bHoldActionStarted = false;
	RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Torch_Deterrent);
	RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Lantern_Focused);
	RefreshRightHandPresentation();
}

bool UDarkwellLoadoutComponent::CycleRightHandItem()
{
	return EquipRightHandItem(
		EquippedRightHandItem == DarkwellGameplayTags::Equipment_Right_Torch
			? DarkwellGameplayTags::Equipment_Right_Lantern.GetTag()
			: DarkwellGameplayTags::Equipment_Right_Torch.GetTag());
}

bool UDarkwellLoadoutComponent::EquipRightHandItem(const FGameplayTag ItemTag)
{
	if (ItemTag != DarkwellGameplayTags::Equipment_Right_Torch
		&& ItemTag != DarkwellGameplayTags::Equipment_Right_Lantern)
	{
		return false;
	}
	if (EquippedRightHandItem == ItemTag)
	{
		return true;
	}

	CancelRightHandUse();
	TorchSwingTimeRemaining = 0.0f;
	LanternFlashTimeRemaining = 0.0f;
	RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Torch_Swinging);
	RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Lantern_Flashing);
	EquippedRightHandItem = ItemTag;
	RefreshRightHandPresentation();
	return true;
}

bool UDarkwellLoadoutComponent::TryFire(const FVector& AimPoint, const float AimProgress)
{
	if (EquippedLeftHandItem != DarkwellGameplayTags::Equipment_Left_Shotgun
		|| IsReloading()
		|| LoadedShells <= 0
		|| !GetWorld())
	{
		if (LoadedShells <= 0)
		{
			BeginReload();
		}
		return false;
	}

	AActor* Owner = GetOwner();
	APawn* PawnOwner = Cast<APawn>(Owner);
	if (!Owner || !PawnOwner)
	{
		return false;
	}

	--LoadedShells;
	RefreshWeaponState();
	UAISense_Hearing::ReportNoiseEvent(
		GetWorld(),
		Owner->GetActorLocation(),
		1.0f,
		Owner,
		3000.0f,
		TEXT("Shotgun"));

	const FVector TraceStart = Owner->GetActorLocation() + Owner->GetActorForwardVector() * 65.0f + FVector::UpVector * 48.0f;
	const FVector AimDirection = (AimPoint - TraceStart).GetSafeNormal(UE_SMALL_NUMBER, Owner->GetActorForwardVector());
	FRandomStream ShotRandom(FPlatformTime::Cycles());
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DarkwellShotgun), false, Owner);
	const float ActiveSpreadHalfAngleDegrees = FMath::Lerp(
		HipFireSpreadHalfAngleDegrees,
		AimedSpreadHalfAngleDegrees,
		FMath::Clamp(AimProgress, 0.0f, 1.0f));

	for (int32 PelletIndex = 0; PelletIndex < PelletCount; ++PelletIndex)
	{
		const FVector PelletDirection = ShotRandom.VRandCone(
			AimDirection,
			FMath::DegreesToRadians(ActiveSpreadHalfAngleDegrees));
		const FVector TraceEnd = TraceStart + PelletDirection * ShotRange;
		FHitResult HitResult;
		const bool bHit = GetWorld()->LineTraceSingleByChannel(
			HitResult,
			TraceStart,
			TraceEnd,
			ECC_Visibility,
			QueryParams);

		const FVector DebugEnd = bHit ? HitResult.ImpactPoint : TraceEnd;
#if ENABLE_DRAW_DEBUG
		DrawDebugLine(GetWorld(), TraceStart, DebugEnd, FColor(255, 185, 80), false, 0.18f, 0, 1.25f);
#endif

		if (bHit && HitResult.GetActor())
		{
			UGameplayStatics::ApplyPointDamage(
				HitResult.GetActor(),
				PelletDamage,
				PelletDirection,
				HitResult,
				PawnOwner->GetController(),
				Owner,
				UDamageType::StaticClass());
		}
	}

	return true;
}

bool UDarkwellLoadoutComponent::BeginReload()
{
	if (IsReloading() || LoadedShells >= MaxLoadedShells || GetReserveShells() <= 0)
	{
		return false;
	}

	RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Weapon_Ready);
	RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Weapon_Empty);
	RuntimeStates.AddTag(DarkwellGameplayTags::State_Player_Weapon_Reloading);
	ReloadTimeRemaining = ReloadDuration;
	CancelRightHandUse();
	RefreshRightHandPresentation();
	return true;
}

void UDarkwellLoadoutComponent::DeactivateForOwnerIncapacitated()
{
	ReloadTimeRemaining = 0.0f;
	RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Weapon_Reloading);
	bOwnerIncapacitated = true;
	CancelRightHandUse();
	ClearRightHandActionStates();
	RefreshRightHandPresentation();
	RefreshWeaponState();
}

int32 UDarkwellLoadoutComponent::AddReserveShells(const int32 Amount)
{
	ADarkwellCharacter* Character = Cast<ADarkwellCharacter>(GetOwner());
	UDarkwellInventoryComponent* Inventory = Character ? Character->GetInventoryComponent() : nullptr;
	return Inventory
		? Inventory->AddItem(DarkwellGameplayTags::Item_Ammo_ShotgunShell, Amount)
		: 0;
}

void UDarkwellLoadoutComponent::RestorePersistentState(
	const int32 SavedLoadedShells,
	const float SavedTorchCharge,
	const float SavedTorchHeat,
	const float SavedLanternFuel,
	const FGameplayTag SavedLeftHandItem,
	const FGameplayTag SavedRightHandItem)
{
	ReloadTimeRemaining = 0.0f;
	LoadedShells = FMath::Clamp(SavedLoadedShells, 0, MaxLoadedShells);
	TorchCharge = FMath::Max(0.0f, SavedTorchCharge);
	TorchHeat = FMath::Clamp(SavedTorchHeat, 0.0f, 100.0f);
	LanternFuel = FMath::Max(0.0f, SavedLanternFuel);
	bTorchOverheated = TorchHeat >= 100.0f;
	bOwnerIncapacitated = false;
	EquippedLeftHandItem = SavedLeftHandItem.IsValid()
		? SavedLeftHandItem
		: DarkwellGameplayTags::Equipment_Left_Shotgun;
	EquippedRightHandItem = SavedRightHandItem.IsValid()
		&& (SavedRightHandItem == DarkwellGameplayTags::Equipment_Right_Torch
			|| SavedRightHandItem == DarkwellGameplayTags::Equipment_Right_Lantern)
			? SavedRightHandItem
			: DarkwellGameplayTags::Equipment_Right_Torch.GetTag();
	RuntimeStates.Reset();
	ClearRightHandActionStates();
	RefreshWeaponState();
	RefreshRightHandPresentation();
}

int32 UDarkwellLoadoutComponent::GetReserveShells() const
{
	const ADarkwellCharacter* Character = Cast<ADarkwellCharacter>(GetOwner());
	const UDarkwellInventoryComponent* Inventory = Character ? Character->GetInventoryComponent() : nullptr;
	return Inventory
		? Inventory->GetTotalQuantity(DarkwellGameplayTags::Item_Ammo_ShotgunShell)
		: 0;
}

bool UDarkwellLoadoutComponent::IsTorchOn() const
{
	return RuntimeStates.HasTagExact(DarkwellGameplayTags::State_Player_Torch_On);
}

bool UDarkwellLoadoutComponent::IsTorchDeterrentActive() const
{
	return Darkwell::LoadoutRules::IsTorchDeterrentActive(
		Darkwell::LoadoutRules::ResolveTorchPresentation(
			EquippedRightHandItem == DarkwellGameplayTags::Equipment_Right_Torch,
			TorchCharge > 0.0f && !bOwnerIncapacitated,
			IsReloading(),
			IsTorchSwinging(),
			RuntimeStates.HasTagExact(DarkwellGameplayTags::State_Player_Torch_Deterrent)));
}

bool UDarkwellLoadoutComponent::IsTorchSwinging() const
{
	return RuntimeStates.HasTagExact(DarkwellGameplayTags::State_Player_Torch_Swinging);
}

bool UDarkwellLoadoutComponent::IsLanternOn() const
{
	return RuntimeStates.HasTagExact(DarkwellGameplayTags::State_Player_Lantern_On);
}

bool UDarkwellLoadoutComponent::IsLanternFocused() const
{
	return RuntimeStates.HasTagExact(DarkwellGameplayTags::State_Player_Lantern_Focused);
}

bool UDarkwellLoadoutComponent::IsLanternFlashActive() const
{
	return RuntimeStates.HasTagExact(DarkwellGameplayTags::State_Player_Lantern_Flashing);
}

EDarkwellLightPressureKind UDarkwellLoadoutComponent::GetActiveLightPressure(
	float& OutRange,
	float& OutFacingThreshold) const
{
	OutRange = 0.0f;
	OutFacingThreshold = 1.0f;
	if (IsTorchDeterrentActive())
	{
		OutRange = TorchDeterrentRange;
		OutFacingThreshold = FMath::Cos(FMath::DegreesToRadians(TorchDeterrentHalfAngleDegrees));
		return EDarkwellLightPressureKind::TorchDeterrent;
	}
	if (IsLanternFocused() && LanternFuel > 0.0f && !IsReloading() && !bOwnerIncapacitated)
	{
		OutRange = LanternFocusRange;
		OutFacingThreshold = FMath::Cos(FMath::DegreesToRadians(LanternFocusHalfAngleDegrees));
		return EDarkwellLightPressureKind::LanternFocus;
	}
	return EDarkwellLightPressureKind::None;
}

bool UDarkwellLoadoutComponent::IsReloading() const
{
	return RuntimeStates.HasTagExact(DarkwellGameplayTags::State_Player_Weapon_Reloading);
}

void UDarkwellLoadoutComponent::CompleteReload()
{
	ADarkwellCharacter* Character = Cast<ADarkwellCharacter>(GetOwner());
	UDarkwellInventoryComponent* Inventory = Character ? Character->GetInventoryComponent() : nullptr;
	if (Inventory)
	{
		const int32 RequestedShells = FMath::Max(0, MaxLoadedShells - LoadedShells);
		LoadedShells += Inventory->RemoveItem(DarkwellGameplayTags::Item_Ammo_ShotgunShell, RequestedShells);
	}
	ReloadTimeRemaining = 0.0f;
	RefreshWeaponState();
	RefreshRightHandPresentation();
}

void UDarkwellLoadoutComponent::TickRightHandTool(const float DeltaTime)
{
	const float SafeDelta = FMath::Max(0.0f, DeltaTime);
	bool bPresentationChanged = false;

	LanternFlashCooldownRemaining = FMath::Max(0.0f, LanternFlashCooldownRemaining - SafeDelta);
	if (TorchSwingTimeRemaining > 0.0f)
	{
		TorchSwingTimeRemaining = FMath::Max(0.0f, TorchSwingTimeRemaining - SafeDelta);
		if (TorchSwingTimeRemaining <= 0.0f)
		{
			RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Torch_Swinging);
			bPresentationChanged = true;
		}
	}
	if (LanternFlashTimeRemaining > 0.0f)
	{
		LanternFlashTimeRemaining = FMath::Max(0.0f, LanternFlashTimeRemaining - SafeDelta);
		if (LanternFlashTimeRemaining <= 0.0f)
		{
			RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Lantern_Flashing);
			bPresentationChanged = true;
		}
	}

	if (RightHandHeldSeconds >= 0.0f)
	{
		RightHandHeldSeconds += SafeDelta;
		if (!bHoldActionStarted
			&& Darkwell::LoadoutRules::ResolveGesture(RightHandHeldSeconds, RightHandHoldThreshold)
				== EDarkwellRightToolGesture::Hold)
		{
			bHoldActionStarted = true;
			if (EquippedRightHandItem == DarkwellGameplayTags::Equipment_Right_Torch
				&& TorchCharge > 0.0f && !bTorchOverheated && !IsReloading())
			{
				RuntimeStates.AddTag(DarkwellGameplayTags::State_Player_Torch_Deterrent);
			}
			else if (EquippedRightHandItem == DarkwellGameplayTags::Equipment_Right_Lantern
				&& LanternFuel > 0.0f && !IsReloading())
			{
				RuntimeStates.AddTag(DarkwellGameplayTags::State_Player_Lantern_Focused);
			}
			bPresentationChanged = true;
		}
	}

	if (!bOwnerIncapacitated && EquippedRightHandItem == DarkwellGameplayTags::Equipment_Right_Torch && TorchCharge > 0.0f)
	{
		const float DrainRate = TorchDrainPerSecond
			+ (IsTorchDeterrentActive() ? TorchDeterrentExtraDrainPerSecond : 0.0f);
		TorchCharge = Darkwell::ResourceMath::DrainResource(TorchCharge, DrainRate, SafeDelta);
		if (TorchCharge <= 0.0f)
		{
			CancelRightHandUse();
			bPresentationChanged = true;
		}
	}

	const bool bHeating = IsTorchDeterrentActive();
	TorchHeat = Darkwell::LoadoutRules::UpdateTorchHeat(
		TorchHeat,
		TorchHoldHeatPerSecond,
		TorchCoolPerSecond,
		SafeDelta,
		bHeating);
	if (!bTorchOverheated && TorchHeat >= 100.0f)
	{
		bTorchOverheated = true;
		RuntimeStates.AddTag(DarkwellGameplayTags::State_Player_Torch_Overheated);
		CancelRightHandUse();
		bPresentationChanged = true;
	}
	else if (bTorchOverheated && TorchHeat <= TorchOverheatRecoveryThreshold)
	{
		bTorchOverheated = false;
		RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Torch_Overheated);
		bPresentationChanged = true;
	}

	if (!bOwnerIncapacitated && EquippedRightHandItem == DarkwellGameplayTags::Equipment_Right_Lantern && LanternFuel > 0.0f)
	{
		const float DrainRate = LanternBaseDrainPerSecond
			+ (IsLanternFocused() ? LanternFocusExtraDrainPerSecond : 0.0f);
		LanternFuel = Darkwell::ResourceMath::DrainResource(LanternFuel, DrainRate, SafeDelta);
		if (LanternFuel <= 0.0f)
		{
			CancelRightHandUse();
			bPresentationChanged = true;
		}
	}

	if (TorchSwingTimeRemaining > 0.0f || bPresentationChanged)
	{
		RefreshRightHandPresentation();
	}
}

void UDarkwellLoadoutComponent::TriggerTorchSwing()
{
	if (EquippedRightHandItem != DarkwellGameplayTags::Equipment_Right_Torch
		|| TorchCharge <= 0.0f || bTorchOverheated || IsReloading() || bOwnerIncapacitated)
	{
		return;
	}

	TorchSwingTimeRemaining = TorchSwingDuration;
	TorchHeat = FMath::Clamp(TorchHeat + TorchSwingHeat, 0.0f, 100.0f);
	RuntimeStates.AddTag(DarkwellGameplayTags::State_Player_Torch_Swinging);
	if (TorchHeat >= 100.0f)
	{
		bTorchOverheated = true;
		RuntimeStates.AddTag(DarkwellGameplayTags::State_Player_Torch_Overheated);
	}

	AActor* Owner = GetOwner();
	APawn* PawnOwner = Cast<APawn>(Owner);
	if (Owner && PawnOwner && GetWorld())
	{
		const FVector Origin = Owner->GetActorLocation();
		const FVector Facing = Owner->GetActorForwardVector().GetSafeNormal2D();
		const float FacingThreshold = FMath::Cos(FMath::DegreesToRadians(TorchSwingHalfAngleDegrees));
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DarkwellTorchSwing), false, Owner);
		for (TActorIterator<ADarkwellStalkerCharacter> It(GetWorld()); It; ++It)
		{
			if (!It->IsAlive())
			{
				continue;
			}
			const FVector ToEnemy = It->GetActorLocation() - Origin;
			if (ToEnemy.Size2D() > TorchSwingRange
				|| FVector::DotProduct(Facing, ToEnemy.GetSafeNormal2D()) < FacingThreshold)
			{
				continue;
			}
			FHitResult Hit;
			const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
				Hit,
				Origin + FVector::UpVector * 45.0f,
				It->GetActorLocation() + FVector::UpVector * 45.0f,
				ECC_Visibility,
				QueryParams);
			if (bBlocked && Hit.GetActor() != *It)
			{
				continue;
			}
			UGameplayStatics::ApplyDamage(*It, TorchSwingDamage, PawnOwner->GetController(), Owner, UDamageType::StaticClass());
			if (ADarkwellStalkerController* Controller = Cast<ADarkwellStalkerController>(It->GetController()))
			{
				Controller->ApplyLightControl(TorchSwingControlDuration);
			}
		}
		UAISense_Hearing::ReportNoiseEvent(GetWorld(), Origin, 0.35f, Owner, 650.0f, TEXT("TorchSwing"));
	}
	RefreshRightHandPresentation();
}

void UDarkwellLoadoutComponent::TriggerLanternFlash()
{
	if (EquippedRightHandItem != DarkwellGameplayTags::Equipment_Right_Lantern
		|| LanternFuel < LanternFlashFuelCost || LanternFlashCooldownRemaining > 0.0f
		|| IsReloading() || bOwnerIncapacitated)
	{
		return;
	}

	LanternFuel = FMath::Max(0.0f, LanternFuel - LanternFlashFuelCost);
	LanternFlashTimeRemaining = LanternFlashDuration;
	LanternFlashCooldownRemaining = LanternFlashCooldown;
	RuntimeStates.AddTag(DarkwellGameplayTags::State_Player_Lantern_Flashing);

	AActor* Owner = GetOwner();
	if (Owner && GetWorld())
	{
		const FVector Origin = Owner->GetActorLocation() + FVector::UpVector * 50.0f;
		const FVector Facing = Owner->GetActorForwardVector().GetSafeNormal2D();
		const float FacingThreshold = FMath::Cos(FMath::DegreesToRadians(LanternFlashHalfAngleDegrees));
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DarkwellLanternFlash), false, Owner);
		for (TActorIterator<ADarkwellStalkerCharacter> It(GetWorld()); It; ++It)
		{
			if (!It->IsAlive())
			{
				continue;
			}
			const FVector ToEnemy = It->GetActorLocation() - Origin;
			if (ToEnemy.Size2D() > LanternFlashRange
				|| FVector::DotProduct(Facing, ToEnemy.GetSafeNormal2D()) < FacingThreshold)
			{
				continue;
			}

			FHitResult Hit;
			const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
				Hit,
				Origin,
				It->GetActorLocation() + FVector::UpVector * 45.0f,
				ECC_Visibility,
				QueryParams);
			if (bBlocked && Hit.GetActor() != *It)
			{
				continue;
			}
			if (ADarkwellStalkerController* Controller = Cast<ADarkwellStalkerController>(It->GetController()))
			{
				Controller->ApplyLightControl(LanternFlashControlDuration);
			}
		}
	}
	RefreshRightHandPresentation();
}

void UDarkwellLoadoutComponent::RefreshRightHandPresentation()
{
	const bool bTorchEquipped = EquippedRightHandItem == DarkwellGameplayTags::Equipment_Right_Torch;
	const bool bLanternEquipped = EquippedRightHandItem == DarkwellGameplayTags::Equipment_Right_Lantern;
	const bool bTorchHasLight = bTorchEquipped && TorchCharge > 0.0f && !bOwnerIncapacitated;
	const bool bLanternHasLight = bLanternEquipped
		&& (LanternFuel > 0.0f || IsLanternFlashActive())
		&& !bOwnerIncapacitated;

	if (bTorchOverheated)
	{
		RuntimeStates.AddTag(DarkwellGameplayTags::State_Player_Torch_Overheated);
	}
	else
	{
		RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Torch_Overheated);
	}

	if (bTorchHasLight)
	{
		RuntimeStates.AddTag(DarkwellGameplayTags::State_Player_Torch_On);
	}
	else
	{
		RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Torch_On);
	}
	if (bLanternHasLight)
	{
		RuntimeStates.AddTag(DarkwellGameplayTags::State_Player_Lantern_On);
	}
	else
	{
		RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Lantern_On);
	}

	const EDarkwellTorchPresentationMode PresentationMode =
		Darkwell::LoadoutRules::ResolveTorchPresentation(
			bTorchEquipped,
			bTorchHasLight,
			IsReloading(),
			IsTorchSwinging(),
			RuntimeStates.HasTagExact(DarkwellGameplayTags::State_Player_Torch_Deterrent));

	if (PresentationMode == EDarkwellTorchPresentationMode::ReloadPool)
	{
		RuntimeStates.AddTag(DarkwellGameplayTags::State_Player_Torch_Lowered);
	}
	else
	{
		RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Torch_Lowered);
	}

	if (TorchMesh)
	{
		TorchMesh->SetVisibility(bTorchEquipped && !bOwnerIncapacitated);
		TorchMesh->SetRelativeLocation(RaisedTorchMeshRelativeLocation);
		TorchMesh->SetRelativeRotation(RaisedTorchMeshRelativeRotation);
		if (PresentationMode == EDarkwellTorchPresentationMode::Swing && TorchSwingDuration > 0.0f)
		{
			const float Progress = 1.0f - FMath::Clamp(TorchSwingTimeRemaining / TorchSwingDuration, 0.0f, 1.0f);
			TorchMesh->SetRelativeRotation(
				RaisedTorchMeshRelativeRotation + FRotator(0.0f, FMath::Lerp(-105.0f, 105.0f, Progress), 18.0f));
			TorchMesh->SetRelativeLocation(RaisedTorchMeshRelativeLocation + FVector(22.0f, 0.0f, 4.0f));
		}
	}
	if (LanternMesh)
	{
		LanternMesh->SetVisibility(bLanternEquipped && !bOwnerIncapacitated);
	}

	if (TorchLight)
	{
		TorchLight->SetVisibility(PresentationMode != EDarkwellTorchPresentationMode::Off);
		if (PresentationMode == EDarkwellTorchPresentationMode::ReloadPool)
		{
			TorchLight->SetRelativeLocation(ReloadTorchRelativeLocation);
			TorchLight->SetIntensity(ReloadTorchIntensity);
			TorchLight->SetAttenuationRadius(ReloadTorchRadius);
		}
		else if (bHasRaisedTorchPresentation)
		{
			TorchLight->SetRelativeLocation(RaisedTorchRelativeLocation);
			if (PresentationMode == EDarkwellTorchPresentationMode::Deterrent)
			{
				TorchLight->SetIntensity(12000.0f);
				TorchLight->SetAttenuationRadius(1800.0f);
			}
			else if (PresentationMode == EDarkwellTorchPresentationMode::Swing)
			{
				TorchLight->SetIntensity(9000.0f);
				TorchLight->SetAttenuationRadius(1350.0f);
			}
			else
			{
				TorchLight->SetIntensity(RaisedTorchIntensity);
				TorchLight->SetAttenuationRadius(RaisedTorchRadius);
			}
		}
	}

	const bool bFlashing = IsLanternFlashActive() && bLanternHasLight;
	const bool bFocused = IsLanternFocused() && bLanternHasLight && !IsReloading();
	if (LanternBaseLight)
	{
		LanternBaseLight->SetVisibility(bLanternHasLight);
		LanternBaseLight->SetIntensity(bFlashing ? 18000.0f : (bFocused ? 1800.0f : 3000.0f));
		LanternBaseLight->SetAttenuationRadius(bFlashing ? 1550.0f : 900.0f);
	}
	if (LanternFocusLight)
	{
		LanternFocusLight->SetVisibility(bFocused || bFlashing);
		LanternFocusLight->SetIntensity(bFlashing ? 28000.0f : 13500.0f);
		LanternFocusLight->SetAttenuationRadius(bFlashing ? 1500.0f : 2200.0f);
		LanternFocusLight->SetInnerConeAngle(bFlashing ? 32.0f : 8.0f);
		LanternFocusLight->SetOuterConeAngle(bFlashing ? 55.0f : 18.0f);
	}
}

void UDarkwellLoadoutComponent::ClearRightHandActionStates()
{
	RightHandHeldSeconds = -1.0f;
	bHoldActionStarted = false;
	TorchSwingTimeRemaining = 0.0f;
	LanternFlashTimeRemaining = 0.0f;
	RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Torch_Swinging);
	RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Torch_Deterrent);
	RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Lantern_Focused);
	RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Lantern_Flashing);
}

void UDarkwellLoadoutComponent::RefreshWeaponState()
{
	RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Weapon_Ready);
	RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Weapon_Empty);
	RuntimeStates.RemoveTag(DarkwellGameplayTags::State_Player_Weapon_Reloading);
	RuntimeStates.AddTag(
		LoadedShells > 0
			? DarkwellGameplayTags::State_Player_Weapon_Ready
			: DarkwellGameplayTags::State_Player_Weapon_Empty);
}
