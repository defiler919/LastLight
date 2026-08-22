// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EDarkwellTorchPresentationMode : uint8
{
	Off,
	Idle,
	Swing,
	Deterrent,
	ReloadPool
};

enum class EDarkwellRightToolGesture : uint8
{
	None,
	Tap,
	Hold
};

namespace Darkwell::LoadoutRules
{
	/** Resolves the torch's visible and threat presentation from durable runtime facts. */
	DARKWELL_API EDarkwellTorchPresentationMode ResolveTorchPresentation(
		bool bEquipped,
		bool bHasFuel,
		bool bReloading,
		bool bSwinging,
		bool bDeterrentHeld);

	/** Only an intentionally held-forward torch creates a continuous deterrence field. */
	DARKWELL_API bool IsTorchDeterrentActive(EDarkwellTorchPresentationMode PresentationMode);

	/** A press starts as pending and becomes a hold only after this threshold is crossed. */
	DARKWELL_API EDarkwellRightToolGesture ResolveGesture(float HeldSeconds, float HoldThresholdSeconds);

	/** Applies either heat gain or cooling without leaving the normalized 0-100 range. */
	DARKWELL_API float UpdateTorchHeat(
		float CurrentHeat,
		float HeatGainPerSecond,
		float CoolPerSecond,
		float DeltaSeconds,
		bool bHeating);
}
