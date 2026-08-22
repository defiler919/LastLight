// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/DarkwellLoadoutRules.h"

EDarkwellTorchPresentationMode Darkwell::LoadoutRules::ResolveTorchPresentation(
	const bool bEquipped,
	const bool bHasFuel,
	const bool bReloading,
	const bool bSwinging,
	const bool bDeterrentHeld)
{
	if (!bEquipped || !bHasFuel)
	{
		return EDarkwellTorchPresentationMode::Off;
	}

	if (bReloading)
	{
		return EDarkwellTorchPresentationMode::ReloadPool;
	}

	if (bDeterrentHeld)
	{
		return EDarkwellTorchPresentationMode::Deterrent;
	}

	return bSwinging
		? EDarkwellTorchPresentationMode::Swing
		: EDarkwellTorchPresentationMode::Idle;
}

bool Darkwell::LoadoutRules::IsTorchDeterrentActive(
	const EDarkwellTorchPresentationMode PresentationMode)
{
	return PresentationMode == EDarkwellTorchPresentationMode::Deterrent;
}

EDarkwellRightToolGesture Darkwell::LoadoutRules::ResolveGesture(
	const float HeldSeconds,
	const float HoldThresholdSeconds)
{
	if (HeldSeconds < 0.0f)
	{
		return EDarkwellRightToolGesture::None;
	}

	return HeldSeconds >= FMath::Max(0.0f, HoldThresholdSeconds)
		? EDarkwellRightToolGesture::Hold
		: EDarkwellRightToolGesture::Tap;
}

float Darkwell::LoadoutRules::UpdateTorchHeat(
	const float CurrentHeat,
	const float HeatGainPerSecond,
	const float CoolPerSecond,
	const float DeltaSeconds,
	const bool bHeating)
{
	const float SafeDelta = FMath::Max(0.0f, DeltaSeconds);
	const float Delta = bHeating
		? FMath::Max(0.0f, HeatGainPerSecond) * SafeDelta
		: -FMath::Max(0.0f, CoolPerSecond) * SafeDelta;
	return FMath::Clamp(CurrentHeat + Delta, 0.0f, 100.0f);
}
