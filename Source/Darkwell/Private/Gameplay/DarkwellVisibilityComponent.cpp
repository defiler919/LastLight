// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gameplay/DarkwellVisibilityComponent.h"

#include "Combat/DarkwellLoadoutComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/DarkwellFogSubject.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "Player/DarkwellCharacter.h"

UDarkwellVisibilityComponent::UDarkwellVisibilityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UDarkwellVisibilityComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshVisibility();
}

void UDarkwellVisibilityComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	RefreshTimeRemaining -= FMath::Max(0.0f, DeltaTime);
	if (RefreshTimeRemaining <= 0.0f)
	{
		RefreshVisibility();
	}
}

EDarkwellFogCellState UDarkwellVisibilityComponent::GetCellState(const FIntPoint& Cell) const
{
	return Darkwell::VisibilityMath::ResolveFogCellState(
		VisibleCells.Contains(Cell),
		ExploredCells.Contains(Cell));
}

EDarkwellFogCellState UDarkwellVisibilityComponent::GetWorldLocationState(const FVector& WorldLocation) const
{
	return GetCellState(Darkwell::VisibilityMath::WorldToCell(WorldLocation, CellSize));
}

bool UDarkwellVisibilityComponent::IsWorldLocationCurrentlyVisible(const FVector& WorldLocation) const
{
	return GetWorldLocationState(WorldLocation) == EDarkwellFogCellState::Visible;
}

bool UDarkwellVisibilityComponent::IsWorldLocationExplored(const FVector& WorldLocation) const
{
	return GetWorldLocationState(WorldLocation) != EDarkwellFogCellState::Unexplored;
}

float UDarkwellVisibilityComponent::GetVisionSourceMargin(const FVector& WorldLocation) const
{
	const ADarkwellCharacter* Character = Cast<ADarkwellCharacter>(GetOwner());
	if (!Character)
	{
		return -BIG_NUMBER;
	}

	const FVector Origin = Character->GetActorLocation();
	const FVector Facing = Character->GetActorForwardVector();
	const float Distance = FVector::Dist2D(Origin, WorldLocation);
	float BestMargin = AwarenessRadius - Distance;
	BestMargin = FMath::Max(
		BestMargin,
		GetConeMargin(
			Origin,
			Facing,
			WorldLocation,
			UnlitVisionRange,
			UnlitVisionHalfAngleDegrees));

	const UDarkwellLoadoutComponent* Loadout = Character->GetLoadoutComponent();
	if (!Loadout)
	{
		return BestMargin;
	}

	if (Loadout->GetEquippedRightHandItem() == DarkwellGameplayTags::Equipment_Right_Torch
		&& Loadout->IsTorchOn())
	{
		if (Loadout->IsReloading())
		{
			BestMargin = FMath::Max(BestMargin, ReloadLightRadius - Distance);
		}
		else
		{
			const float Range = Loadout->IsTorchDeterrentActive()
				? TorchDeterrentVisionRange
				: TorchVisionRange;
			BestMargin = FMath::Max(
				BestMargin,
				GetConeMargin(
					Origin,
					Facing,
					WorldLocation,
					Range,
					TorchVisionHalfAngleDegrees));
		}
	}

	if (Loadout->GetEquippedRightHandItem() == DarkwellGameplayTags::Equipment_Right_Lantern
		&& Loadout->IsLanternOn())
	{
		BestMargin = FMath::Max(BestMargin, LanternBaseVisionRadius - Distance);
		if (Loadout->IsLanternFocused())
		{
			BestMargin = FMath::Max(
				BestMargin,
				GetConeMargin(
					Origin,
					Facing,
					WorldLocation,
					LanternFocusVisionRange,
					LanternFocusHalfAngleDegrees));
		}
		if (Loadout->IsLanternFlashActive())
		{
			BestMargin = FMath::Max(
				BestMargin,
				GetConeMargin(
					Origin,
					Facing,
					WorldLocation,
					LanternFlashVisionRange,
					LanternFlashHalfAngleDegrees));
		}
	}

	return BestMargin;
}

float UDarkwellVisibilityComponent::GetCurrentMaximumVisionRange() const
{
	const ADarkwellCharacter* Character = Cast<ADarkwellCharacter>(GetOwner());
	float MaximumRange = FMath::Max(AwarenessRadius, UnlitVisionRange);
	if (!Character)
	{
		return MaximumRange;
	}

	const UDarkwellLoadoutComponent* Loadout = Character->GetLoadoutComponent();
	if (!Loadout)
	{
		return MaximumRange;
	}

	if (Loadout->GetEquippedRightHandItem() == DarkwellGameplayTags::Equipment_Right_Torch
		&& Loadout->IsTorchOn())
	{
		MaximumRange = FMath::Max(
			MaximumRange,
			Loadout->IsReloading()
				? ReloadLightRadius
				: (Loadout->IsTorchDeterrentActive() ? TorchDeterrentVisionRange : TorchVisionRange));
	}
	else if (Loadout->GetEquippedRightHandItem() == DarkwellGameplayTags::Equipment_Right_Lantern
		&& Loadout->IsLanternOn())
	{
		MaximumRange = FMath::Max(MaximumRange, LanternBaseVisionRadius);
		if (Loadout->IsLanternFocused())
		{
			MaximumRange = FMath::Max(MaximumRange, LanternFocusVisionRange);
		}
		if (Loadout->IsLanternFlashActive())
		{
			MaximumRange = FMath::Max(MaximumRange, LanternFlashVisionRange);
		}
	}
	return MaximumRange;
}

void UDarkwellVisibilityComponent::BuildVisualOcclusionRanges(
	const int32 SampleCount,
	TArray<float>& OutRanges) const
{
	OutRanges.Reset();
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || SampleCount < 3)
	{
		return;
	}

	const float MaximumRange = GetCurrentMaximumVisionRange();
	OutRanges.SetNumUninitialized(SampleCount);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DarkwellFogVisualOcclusion), false, Owner);
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		QueryParams.AddIgnoredActor(*It);
	}

	const FVector Start = Owner->GetActorLocation() + FVector::UpVector * VisibilityTraceHeight;
	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		const float Angle = UE_TWO_PI * static_cast<float>(Index) / SampleCount;
		const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		FHitResult Hit;
		OutRanges[Index] = World->LineTraceSingleByChannel(
			Hit,
			Start,
			Start + Direction * MaximumRange,
			ECC_Visibility,
			QueryParams)
			? FMath::Clamp(Hit.Distance, 0.0f, MaximumRange)
			: MaximumRange;
	}
}

TArray<FIntPoint> UDarkwellVisibilityComponent::CaptureExploredCells() const
{
	TArray<FIntPoint> Result = ExploredCells.Array();
	Result.Sort([](const FIntPoint& A, const FIntPoint& B)
	{
		return A.X == B.X ? A.Y < B.Y : A.X < B.X;
	});
	return Result;
}

void UDarkwellVisibilityComponent::RestoreExploredCells(const TArray<FIntPoint>& SavedCells)
{
	ExploredCells.Reset();
	const int32 SafeCount = FMath::Min(SavedCells.Num(), MaximumRememberedCells);
	for (int32 Index = 0; Index < SafeCount; ++Index)
	{
		ExploredCells.Add(SavedCells[Index]);
	}
	RefreshVisibility();
}

void UDarkwellVisibilityComponent::RefreshVisibility()
{
	RefreshTimeRemaining = RefreshIntervalSeconds;
	VisibleCells.Reset();

	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}

	const float MaximumVisionRange = FMath::Max3(
		TorchDeterrentVisionRange,
		LanternFocusVisionRange,
		FMath::Max(UnlitVisionRange, LanternFlashVisionRange));
	const FVector Origin = Owner->GetActorLocation();
	const FIntPoint OriginCell = Darkwell::VisibilityMath::WorldToCell(Origin, CellSize);
	const int32 CellRadius = FMath::CeilToInt(MaximumVisionRange / CellSize) + 1;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DarkwellFogVisibility), false, Owner);
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		QueryParams.AddIgnoredActor(*It);
	}

	for (int32 Y = OriginCell.Y - CellRadius; Y <= OriginCell.Y + CellRadius; ++Y)
	{
		for (int32 X = OriginCell.X - CellRadius; X <= OriginCell.X + CellRadius; ++X)
		{
			const FIntPoint Cell(X, Y);
			const FVector CellCenter = Darkwell::VisibilityMath::CellToWorldCenter(Cell, CellSize, Origin.Z);
			if (IsCellInsideAnyVisionSource(CellCenter) && HasLineOfSightToCell(CellCenter, QueryParams))
			{
				VisibleCells.Add(Cell);
				if (ExploredCells.Num() < MaximumRememberedCells || ExploredCells.Contains(Cell))
				{
					ExploredCells.Add(Cell);
				}
			}
		}
	}

	VisibleCells.Add(OriginCell);
	ExploredCells.Add(OriginCell);
	UpdateFogSubjects();
}

bool UDarkwellVisibilityComponent::IsCellInsideAnyVisionSource(const FVector& CellCenter) const
{
	return GetVisionSourceMargin(CellCenter) >= 0.0f;
}

float UDarkwellVisibilityComponent::GetConeMargin(
	const FVector& Origin,
	const FVector& Facing,
	const FVector& Target,
	const float Range,
	const float HalfAngleDegrees)
{
	const FVector ToTarget = FVector(Target.X - Origin.X, Target.Y - Origin.Y, 0.0f);
	const float Distance = ToTarget.Size();
	const float RadialMargin = Range - Distance;
	if (Distance <= UE_KINDA_SMALL_NUMBER)
	{
		return RadialMargin;
	}

	const FVector PlanarFacing = FVector(Facing.X, Facing.Y, 0.0f).GetSafeNormal();
	const float Dot = FVector::DotProduct(PlanarFacing, ToTarget / Distance);
	const float Angle = FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f));
	const float AngularMargin =
		(FMath::DegreesToRadians(HalfAngleDegrees) - Angle) * Distance;
	return FMath::Min(RadialMargin, AngularMargin);
}

bool UDarkwellVisibilityComponent::HasLineOfSightToCell(
	const FVector& CellCenter,
	const FCollisionQueryParams& QueryParams) const
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return false;
	}

	const FVector Start = Owner->GetActorLocation() + FVector::UpVector * VisibilityTraceHeight;
	const FVector End = CellCenter + FVector::UpVector * VisibilityTraceHeight;
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams))
	{
		return true;
	}

	const float TraceLength = FVector::Distance(Start, End);
	return Hit.Distance >= TraceLength - CellSize * 0.55f;
}

void UDarkwellVisibilityComponent::UpdateFogSubjects()
{
	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (IDarkwellFogSubject* FogSubject = Cast<IDarkwellFogSubject>(*It))
		{
			FogSubject->SetPlayerFogState(GetWorldLocationState(It->GetActorLocation()));
		}
	}
}
