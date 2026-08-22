// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/DarkwellWeaponWheelRules.h"

EDarkwellWeaponWheelSide Darkwell::WeaponWheelRules::ResolveActiveWheel(
	const bool bLeftDown,
	const bool bRightDown,
	const bool bLeftWasDown,
	const bool bRightWasDown,
	const EDarkwellWeaponWheelSide CurrentWheel)
{
	if (!bLeftDown && !bRightDown)
	{
		return EDarkwellWeaponWheelSide::None;
	}

	if (bLeftDown && !bRightDown)
	{
		return EDarkwellWeaponWheelSide::Left;
	}

	if (bRightDown && !bLeftDown)
	{
		return EDarkwellWeaponWheelSide::Right;
	}

	const bool bLeftJustPressed = bLeftDown && !bLeftWasDown;
	const bool bRightJustPressed = bRightDown && !bRightWasDown;
	if (bRightJustPressed && !bLeftJustPressed)
	{
		return EDarkwellWeaponWheelSide::Right;
	}

	if (bLeftJustPressed && !bRightJustPressed)
	{
		return EDarkwellWeaponWheelSide::Left;
	}

	return CurrentWheel != EDarkwellWeaponWheelSide::None
		? CurrentWheel
		: EDarkwellWeaponWheelSide::Right;
}

bool Darkwell::WeaponWheelRules::ShouldCycleRightHandItem(
	const bool bRightDown,
	const bool bRightWasDown,
	const EDarkwellWeaponWheelSide CurrentWheel)
{
	return !bRightDown
		&& bRightWasDown
		&& CurrentWheel == EDarkwellWeaponWheelSide::Right;
}
