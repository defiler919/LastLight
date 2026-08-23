// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gameplay/DarkwellVisibilityComponent.h"

#include "Components/LocalLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/DarkwellFogSubject.h"
#include "Player/DarkwellCharacter.h"

float FDarkwellLightPresentationSource::EvaluateMargin(const FVector2D& WorldPoint) const
{
	const FVector2D Delta = WorldPoint - FVector2D(WorldOrigin.X, WorldOrigin.Y);
	const float Distance = Delta.Size();
	const float RadialMargin = Range - Distance;
	if (!bCone)
	{
		return RadialMargin;
	}

	const float ForwardDistance = FVector2D::DotProduct(Facing, Delta);
	const float SideDistance = FMath::Abs(Facing.X * Delta.Y - Facing.Y * Delta.X);
	const float AngularMargin = SinHalfAngle * ForwardDistance - CosHalfAngle * SideDistance;
	return FMath::Min(RadialMargin, AngularMargin);
}

void FDarkwellVisionPresentationState::SetSightCone(
	const FVector2D& Origin,
	const FVector2D& Facing,
	const float Range,
	const float HalfAngleDegrees)
{
	SightOrigin = Origin;
	SightFacing = Facing.GetSafeNormal();
	SightRange = FMath::Max(0.0f, Range);
	FMath::SinCos(
		&SightSinHalfAngle,
		&SightCosHalfAngle,
		FMath::DegreesToRadians(HalfAngleDegrees));
}

void FDarkwellVisionPresentationState::SetAwarenessRadius(const float Radius)
{
	AwarenessRadius = FMath::Max(0.0f, Radius);
}

void FDarkwellVisionPresentationState::AddLightCircle(
	const FVector& Origin,
	const float Range,
	const AActor* SourceOwner)
{
	if (Range <= 0.0f || LightCount >= MaximumLightCount)
	{
		return;
	}
	FDarkwellLightPresentationSource& Light = Lights[LightCount++];
	Light.WorldOrigin = Origin;
	Light.Range = Range;
	Light.bCone = false;
	Light.SourceOwner = SourceOwner;
}

void FDarkwellVisionPresentationState::AddLightCone(
	const FVector& Origin,
	const FVector2D& Facing,
	const float Range,
	const float HalfAngleDegrees,
	const AActor* SourceOwner)
{
	if (Range <= 0.0f || LightCount >= MaximumLightCount)
	{
		return;
	}
	FDarkwellLightPresentationSource& Light = Lights[LightCount++];
	Light.WorldOrigin = Origin;
	Light.Facing = Facing.GetSafeNormal();
	Light.Range = Range;
	Light.bCone = true;
	Light.SourceOwner = SourceOwner;
	FMath::SinCos(
		&Light.SinHalfAngle,
		&Light.CosHalfAngle,
		FMath::DegreesToRadians(HalfAngleDegrees));
}

float FDarkwellVisionPresentationState::EvaluateSightMargin(const FVector2D& WorldPoint) const
{
	const FVector2D Delta = WorldPoint - SightOrigin;
	return EvaluateSightMarginFromDelta(Delta, Delta.Size());
}

float FDarkwellVisionPresentationState::EvaluateSightMarginFromDelta(
	const FVector2D& Delta,
	const float Distance) const
{
	const float ForwardDistance = FVector2D::DotProduct(SightFacing, Delta);
	const float SideDistance = FMath::Abs(SightFacing.X * Delta.Y - SightFacing.Y * Delta.X);
	const float AngularMargin = SightSinHalfAngle * ForwardDistance
		- SightCosHalfAngle * SideDistance;
	return FMath::Min(SightRange - Distance, AngularMargin);
}

float FDarkwellVisionPresentationState::EvaluateAwarenessMarginFromDistance(
	const float Distance) const
{
	return AwarenessRadius - Distance;
}

float FDarkwellVisionPresentationState::EvaluateIlluminationMargin(
	const FVector2D& WorldPoint) const
{
	float BestMargin = -BIG_NUMBER;
	for (int32 Index = 0; Index < LightCount; ++Index)
	{
		BestMargin = FMath::Max(BestMargin, Lights[Index].EvaluateMargin(WorldPoint));
	}
	return BestMargin;
}

float FDarkwellVisionPresentationState::EvaluateMargin(const FVector2D& WorldPoint) const
{
	const FVector2D Delta = WorldPoint - SightOrigin;
	return EvaluateMarginFromDelta(Delta, Delta.Size());
}

float FDarkwellVisionPresentationState::EvaluateMarginFromDelta(
	const FVector2D& Delta,
	const float Distance) const
{
	const FVector2D WorldPoint = SightOrigin + Delta;
	const float LitSightMargin = FMath::Min(
		EvaluateSightMarginFromDelta(Delta, Distance),
		EvaluateIlluminationMargin(WorldPoint));
	return FMath::Max(EvaluateAwarenessMarginFromDistance(Distance), LitSightMargin);
}

UDarkwellVisibilityComponent::UDarkwellVisibilityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UDarkwellVisibilityComponent::BeginPlay()
{
	Super::BeginPlay();
	// Let the character's first movement/aim tick establish its facing before
	// committing explored memory. Sampling the constructor-facing direction here
	// produced a gray wedge at the start of a genuinely new game.
	RefreshTimeRemaining = 0.0f;
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
	FDarkwellVisionPresentationState State;
	BuildVisualPresentationState(State);
	return State.EvaluateMargin(FVector2D(WorldLocation.X, WorldLocation.Y));
}

float UDarkwellVisibilityComponent::GetCurrentMaximumVisionRange() const
{
	return SightRange;
}

void UDarkwellVisibilityComponent::BuildVisualPresentationState(
	FDarkwellVisionPresentationState& OutState) const
{
	OutState = FDarkwellVisionPresentationState();
	const ADarkwellCharacter* Character = Cast<ADarkwellCharacter>(GetOwner());
	if (!Character)
	{
		return;
	}
	OutState.SetSightCone(
		FVector2D(Character->GetActorLocation().X, Character->GetActorLocation().Y),
		FVector2D(Character->GetActorForwardVector().X, Character->GetActorForwardVector().Y),
		SightRange,
		FMath::Lerp(
			SightHalfAngleDegrees,
			AimedSightHalfAngleDegrees,
			Character->GetShotgunAimProgress()));
	OutState.SetAwarenessRadius(AwarenessRadius);
	AddActiveLocalLights(OutState);
}

void UDarkwellVisibilityComponent::AddActiveLocalLights(
	FDarkwellVisionPresentationState& OutState) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (!Actor || Actor->IsHidden()
			|| (Actor != GetOwner() && Actor->IsA<APawn>()))
		{
			continue;
		}

		TInlineComponentArray<ULocalLightComponent*, 4> LightComponents;
		Actor->GetComponents(LightComponents);
		for (const ULocalLightComponent* Light : LightComponents)
		{
			// Scene-component activation is not the rendering switch for Unreal
			// lights. Runtime-created point/spot lights can legitimately report
			// IsActive() == false while visibility still makes them illuminate the
			// world. Treat the same properties used by the renderer as authoritative.
			if (!Light || !Light->IsRegistered() || !Light->IsVisible()
				|| !Light->bAffectsWorld || Light->Intensity <= UE_KINDA_SMALL_NUMBER
				|| Light->AttenuationRadius <= UE_KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const FVector LightOrigin = Light->GetComponentLocation();
			if (FVector2D::Distance(
					FVector2D(LightOrigin.X, LightOrigin.Y),
					OutState.SightOrigin)
				> OutState.SightRange + Light->AttenuationRadius)
			{
				continue;
			}
			if (const USpotLightComponent* SpotLight = Cast<USpotLightComponent>(Light))
			{
				const FVector Forward = SpotLight->GetForwardVector();
				OutState.AddLightCone(
					LightOrigin,
					FVector2D(Forward.X, Forward.Y),
					SpotLight->AttenuationRadius,
					SpotLight->OuterConeAngle,
					Actor);
			}
			else
			{
				OutState.AddLightCircle(LightOrigin, Light->AttenuationRadius, Actor);
			}

			if (OutState.LightCount >= FDarkwellVisionPresentationState::MaximumLightCount)
			{
				return;
			}
		}
	}
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
	TArray<float, TInlineAllocator<1024>> RawRanges;
	RawRanges.SetNumUninitialized(SampleCount);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DarkwellFogVisualOcclusion), false, Owner);
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		QueryParams.AddIgnoredActor(*It);
	}

	const ADarkwellCharacter* Character = Cast<ADarkwellCharacter>(Owner);
	const FVector2D SightFacing = Character
		? FVector2D(Character->GetActorForwardVector().X, Character->GetActorForwardVector().Y).GetSafeNormal()
		: FVector2D(1.0f, 0.0f);
	const float ActiveSightHalfAngleDegrees = FMath::Lerp(
		SightHalfAngleDegrees,
		AimedSightHalfAngleDegrees,
		Character ? Character->GetShotgunAimProgress() : 0.0f);
	const float SightCosHalfAngle = FMath::Cos(FMath::DegreesToRadians(
		FMath::Clamp(ActiveSightHalfAngleDegrees + 0.5f, 0.0f, 180.0f)));
	const int32 AwarenessTraceStride = FMath::Max(1, SampleCount / 128);
	const FVector Start = Owner->GetActorLocation() + FVector::UpVector * VisibilityTraceHeight;
	auto TraceRange = [World, &QueryParams, &Start](
		const FVector& Direction,
		const float TraceRange)
	{
		FHitResult Hit;
		return World->LineTraceSingleByChannel(
			Hit,
			Start,
			Start + Direction * TraceRange,
			ECC_Visibility,
			QueryParams)
			? FMath::Clamp(Hit.Distance, 0.0f, TraceRange)
			: TraceRange;
	};
	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		const float Angle = UE_TWO_PI * static_cast<float>(Index) / SampleCount;
		const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		const bool bInsideSightCone = FVector2D::DotProduct(
			SightFacing,
			FVector2D(Direction.X, Direction.Y)) >= SightCosHalfAngle;
		if (bInsideSightCone)
		{
			RawRanges[Index] = TraceRange(Direction, MaximumRange);
		}
		else if (Index % AwarenessTraceStride == 0)
		{
			RawRanges[Index] = TraceRange(Direction, AwarenessRadius);
		}
		else
		{
			RawRanges[Index] = -1.0f;
		}
	}

	// Outside the sight cone only the small body-awareness disk can reveal the
	// world. Sample that disk at 128 directions and interpolate its short ranges;
	// the full 1024-ray density is retained across the actual sight cone where
	// long wall silhouettes need sub-degree precision.
	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		if (RawRanges[Index] >= 0.0f)
		{
			continue;
		}

		int32 PreviousIndex = Index;
		int32 PreviousSteps = 0;
		do
		{
			PreviousIndex = (PreviousIndex + SampleCount - 1) % SampleCount;
			++PreviousSteps;
		}
		while (RawRanges[PreviousIndex] < 0.0f && PreviousSteps < SampleCount);

		int32 NextIndex = Index;
		int32 NextSteps = 0;
		do
		{
			NextIndex = (NextIndex + 1) % SampleCount;
			++NextSteps;
		}
		while (RawRanges[NextIndex] < 0.0f && NextSteps < SampleCount);

		const float Alpha = static_cast<float>(PreviousSteps)
			/ FMath::Max(1, PreviousSteps + NextSteps);
		RawRanges[Index] = FMath::Min(
			AwarenessRadius,
			FMath::Lerp(RawRanges[PreviousIndex], RawRanges[NextIndex], Alpha));
	}

	// Thin and curved occluders can produce isolated range spikes as the player
	// moves between adjacent ray angles. A circular median-of-three rejects those
	// one-sample spikes while preserving the hard tangent at a shadow boundary.
	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		const float Previous = RawRanges[(Index + SampleCount - 1) % SampleCount];
		const float Center = RawRanges[Index];
		const float Next = RawRanges[(Index + 1) % SampleCount];
		const float Minimum = FMath::Min(FMath::Min(Previous, Center), Next);
		const float Maximum = FMath::Max(FMath::Max(Previous, Center), Next);
		OutRanges[Index] = Previous + Center + Next - Minimum - Maximum;
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

TArray<FIntPoint> UDarkwellVisibilityComponent::CaptureExploredPresentationCells() const
{
	TArray<FIntPoint> Result = ExploredPresentationCells.Array();
	Result.Sort([](const FIntPoint& A, const FIntPoint& B)
	{
		return A.X == B.X ? A.Y < B.Y : A.X < B.X;
	});
	return Result;
}

void UDarkwellVisibilityComponent::RestoreExploredCells(
	const TArray<FIntPoint>& SavedCells,
	const TArray<FIntPoint>& SavedPresentationCells,
	const float SavedPresentationCellSize)
{
	ExploredCells.Reset();
	ExploredPresentationCells.Reset();
	++PresentationMemoryRevision;
	const int32 SafeCount = FMath::Min(SavedCells.Num(), MaximumRememberedCells);
	for (int32 Index = 0; Index < SafeCount; ++Index)
	{
		ExploredCells.Add(SavedCells[Index]);
	}

	const float SourcePresentationCellSize = SavedPresentationCellSize > UE_KINDA_SMALL_NUMBER
		? SavedPresentationCellSize
		: 25.0f;
	if (FMath::IsNearlyEqual(SourcePresentationCellSize, PresentationCellSize))
	{
		const int32 SafePresentationCount = FMath::Min(
			SavedPresentationCells.Num(),
			MaximumRememberedPresentationCells);
		for (int32 Index = 0; Index < SafePresentationCount; ++Index)
		{
			RecordExploredPresentationCell(SavedPresentationCells[Index]);
		}
	}
	else
	{
		// Saved presentation cells are world-space raster samples. Re-rasterize
		// their covered area when the presentation resolution changes so older
		// saves keep the same explored footprint instead of shifting or shrinking.
		for (const FIntPoint& SavedCell : SavedPresentationCells)
		{
			const FVector2D SourceMinimum(
				SavedCell.X * SourcePresentationCellSize,
				SavedCell.Y * SourcePresentationCellSize);
			const FVector2D SourceMaximum = SourceMinimum + FVector2D(
				SourcePresentationCellSize - UE_KINDA_SMALL_NUMBER,
				SourcePresentationCellSize - UE_KINDA_SMALL_NUMBER);
			const FIntPoint TargetMinimum(
				FMath::FloorToInt(SourceMinimum.X / PresentationCellSize),
				FMath::FloorToInt(SourceMinimum.Y / PresentationCellSize));
			const FIntPoint TargetMaximum(
				FMath::FloorToInt(SourceMaximum.X / PresentationCellSize),
				FMath::FloorToInt(SourceMaximum.Y / PresentationCellSize));
			for (int32 CellY = TargetMinimum.Y; CellY <= TargetMaximum.Y; ++CellY)
			{
				for (int32 CellX = TargetMinimum.X; CellX <= TargetMaximum.X; ++CellX)
				{
					RecordExploredPresentationCell(FIntPoint(CellX, CellY));
				}
			}
			if (ExploredPresentationCells.Num() >= MaximumRememberedPresentationCells)
			{
				break;
			}
		}
	}

	// Version-4 saves only contain the authoritative 100 cm knowledge field.
	// Expand it once so old saves remain useful without reintroducing coarse blocks.
	if (ExploredPresentationCells.IsEmpty() && !ExploredCells.IsEmpty())
	{
		const int32 Scale = FMath::Max(1, FMath::RoundToInt(CellSize / PresentationCellSize));
		for (const FIntPoint& GameplayCell : ExploredCells)
		{
			const FIntPoint FirstPresentationCell = GameplayCell * Scale;
			for (int32 OffsetY = 0; OffsetY < Scale; ++OffsetY)
			{
				for (int32 OffsetX = 0; OffsetX < Scale; ++OffsetX)
				{
					RecordExploredPresentationCell(
						FirstPresentationCell + FIntPoint(OffsetX, OffsetY));
				}
			}
		}
	}
	RefreshVisibility();
}

bool UDarkwellVisibilityComponent::RecordExploredPresentationCell(const FIntPoint& Cell)
{
	if (ExploredPresentationCells.Contains(Cell))
	{
		return false;
	}
	if (ExploredPresentationCells.Num() >= MaximumRememberedPresentationCells)
	{
		return false;
	}

	ExploredPresentationCells.Add(Cell);
	++PresentationMemoryRevision;
	return true;
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

	FDarkwellVisionPresentationState PresentationState;
	BuildVisualPresentationState(PresentationState);
	const float MaximumVisionRange = PresentationState.SightRange;
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
			const FVector2D CellPoint(CellCenter.X, CellCenter.Y);
			const bool bInsideAwareness =
				PresentationState.EvaluateAwarenessMarginFromDistance(
					FVector2D::Distance(CellPoint, PresentationState.SightOrigin)) >= 0.0f;
			const bool bInsideLitSight = PresentationState.EvaluateSightMargin(CellPoint) >= 0.0f
				&& IsCellIlluminated(CellCenter, PresentationState, QueryParams);
			if ((bInsideAwareness || bInsideLitSight)
				&& HasLineOfSightToCell(CellCenter, QueryParams))
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

bool UDarkwellVisibilityComponent::IsCellIlluminated(
	const FVector& CellCenter,
	const FDarkwellVisionPresentationState& State,
	const FCollisionQueryParams& QueryParams) const
{
	const FVector2D CellPoint(CellCenter.X, CellCenter.Y);
	for (int32 Index = 0; Index < State.LightCount; ++Index)
	{
		const FDarkwellLightPresentationSource& Light = State.Lights[Index];
		if (Light.EvaluateMargin(CellPoint) >= 0.0f
			&& HasLightLineOfSightToCell(Light, CellCenter, QueryParams))
		{
			return true;
		}
	}
	return false;
}

bool UDarkwellVisibilityComponent::HasLightLineOfSightToCell(
	const FDarkwellLightPresentationSource& Light,
	const FVector& CellCenter,
	const FCollisionQueryParams& QueryParams) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector Start = Light.WorldOrigin;
	const FVector End = CellCenter + FVector::UpVector * VisibilityTraceHeight;
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams))
	{
		return true;
	}

	const float TraceLength = FVector::Distance(Start, End);
	return Hit.Distance >= TraceLength - CellSize * 0.55f;
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
