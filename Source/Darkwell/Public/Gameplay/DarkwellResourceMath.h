// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Darkwell::ResourceMath
{
	/** Moves as many shells as possible from reserve into the weapon and returns the amount moved. */
	DARKWELL_API int32 TransferShells(int32 Capacity, int32& LoadedShells, int32& ReserveShells);

	/** Adds a pickup to a bounded reserve and returns the amount accepted. */
	DARKWELL_API int32 AddToReserve(int32 Amount, int32 Capacity, int32& Reserve);

	/** Drains a non-negative resource without allowing it to cross zero. */
	DARKWELL_API float DrainResource(float Current, float RatePerSecond, float DeltaSeconds);
}
