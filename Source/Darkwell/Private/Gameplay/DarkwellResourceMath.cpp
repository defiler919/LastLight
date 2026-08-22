// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gameplay/DarkwellResourceMath.h"

int32 Darkwell::ResourceMath::TransferShells(
	const int32 Capacity,
	int32& LoadedShells,
	int32& ReserveShells)
{
	const int32 SafeCapacity = FMath::Max(0, Capacity);
	LoadedShells = FMath::Clamp(LoadedShells, 0, SafeCapacity);
	ReserveShells = FMath::Max(0, ReserveShells);

	const int32 TransferCount = FMath::Min(SafeCapacity - LoadedShells, ReserveShells);
	LoadedShells += TransferCount;
	ReserveShells -= TransferCount;
	return TransferCount;
}

int32 Darkwell::ResourceMath::AddToReserve(const int32 Amount, const int32 Capacity, int32& Reserve)
{
	const int32 SafeCapacity = FMath::Max(0, Capacity);
	Reserve = FMath::Clamp(Reserve, 0, SafeCapacity);
	const int32 Added = FMath::Min(FMath::Max(0, Amount), SafeCapacity - Reserve);
	Reserve += Added;
	return Added;
}

float Darkwell::ResourceMath::DrainResource(
	const float Current,
	const float RatePerSecond,
	const float DeltaSeconds)
{
	return FMath::Max(0.0f, Current - FMath::Max(0.0f, RatePerSecond) * FMath::Max(0.0f, DeltaSeconds));
}
