// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DarkwellWeaponWheelRules.generated.h"

UENUM()
enum class EDarkwellWeaponWheelSide : uint8
{
	None,
	Left,
	Right
};

namespace Darkwell::WeaponWheelRules
{
	/** Resolves held-key state without depending on a release callback reaching the pawn. */
	DARKWELL_API EDarkwellWeaponWheelSide ResolveActiveWheel(
		bool bLeftDown,
		bool bRightDown,
		bool bLeftWasDown,
		bool bRightWasDown,
		EDarkwellWeaponWheelSide CurrentWheel);

	/** Commits the current right-hand selection only when an open E wheel is released. */
	DARKWELL_API bool ShouldCycleRightHandItem(
		bool bRightDown,
		bool bRightWasDown,
		EDarkwellWeaponWheelSide CurrentWheel);
}
