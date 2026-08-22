// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/DarkwellStalkerController.h"

#include "AI/DarkwellEnemyMath.h"
#include "AI/DarkwellStalkerCharacter.h"
#include "Combat/DarkwellLoadoutComponent.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Player/DarkwellCharacter.h"

ADarkwellStalkerController::ADarkwellStalkerController()
{
	PrimaryActorTick.bCanEverTick = true;

	EnemyPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("EnemyPerception"));
	SetPerceptionComponent(*EnemyPerception);

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 1750.0f;
	SightConfig->LoseSightRadius = 2100.0f;
	SightConfig->PeripheralVisionAngleDegrees = 120.0f;
	SightConfig->SetMaxAge(2.0f);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	EnemyPerception->ConfigureSense(*SightConfig);
	EnemyPerception->SetDominantSense(UAISense_Sight::StaticClass());

	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = 3000.0f;
	HearingConfig->SetMaxAge(5.0f);
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	EnemyPerception->ConfigureSense(*HearingConfig);

	EnemyPerception->OnTargetPerceptionUpdated.AddDynamic(this, &ThisClass::HandleTargetPerceptionUpdated);
}

void ADarkwellStalkerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SetEnemyState(DarkwellGameplayTags::State_Enemy_Idle);
}

void ADarkwellStalkerController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	LoadSafetyGraceRemaining = FMath::Max(0.0f, LoadSafetyGraceRemaining - DeltaSeconds);
	AlertTimeRemaining = FMath::Max(0.0f, AlertTimeRemaining - DeltaSeconds);
	AttackCooldownRemaining = FMath::Max(0.0f, AttackCooldownRemaining - DeltaSeconds);
	LightControlRemaining = FMath::Max(0.0f, LightControlRemaining - DeltaSeconds);
	UpdateLanternFocus(DeltaSeconds);
	BehaviorUpdateRemaining -= DeltaSeconds;
	if (BehaviorUpdateRemaining <= 0.0f)
	{
		BehaviorUpdateRemaining = 0.2f;
		UpdateBehavior();
	}
}

void ADarkwellStalkerController::ApplyLightControl(const float DurationSeconds)
{
	LightControlRemaining = FMath::Max(LightControlRemaining, FMath::Max(0.0f, DurationSeconds));
	AttackCooldownRemaining = FMath::Max(AttackCooldownRemaining, LightControlRemaining);
	StopMovement();
	SetEnemyState(DarkwellGameplayTags::State_Enemy_LightStunned);
}

void ADarkwellStalkerController::ApplyLoadSafetyGrace(const float DurationSeconds)
{
	LoadSafetyGraceRemaining = FMath::Max(0.0f, DurationSeconds);
	TrackedPlayer = nullptr;
	bHasVisualContact = false;
	AlertTimeRemaining = 0.0f;
	AttackCooldownRemaining = LoadSafetyGraceRemaining;
	LightControlRemaining = 0.0f;
	LanternStunBuildup = 0.0f;
	LastKnownPlayerLocation = FVector::ZeroVector;
	if (EnemyPerception)
	{
		EnemyPerception->ForgetAll();
	}
	StopMovement();
	SetEnemyState(DarkwellGameplayTags::State_Enemy_Idle);
}

void ADarkwellStalkerController::UpdateLanternFocus(const float DeltaSeconds)
{
	ADarkwellStalkerCharacter* Enemy = Cast<ADarkwellStalkerCharacter>(GetPawn());
	if (!Enemy || !Enemy->IsAlive())
	{
		LanternStunBuildup = 0.0f;
		return;
	}

	if (!IsValid(TrackedPlayer))
	{
		TrackedPlayer = Cast<ADarkwellCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	}

	bool bFocusHitsEnemy = false;
	if (LoadSafetyGraceRemaining <= 0.0f && LightControlRemaining <= 0.0f
		&& TrackedPlayer && TrackedPlayer->CanAcceptGameplayInput())
	{
		const UDarkwellLoadoutComponent* Loadout = TrackedPlayer->GetLoadoutComponent();
		float FocusRange = 0.0f;
		float FacingThreshold = 1.0f;
		const EDarkwellLightPressureKind LightPressure = Loadout
			? Loadout->GetActiveLightPressure(FocusRange, FacingThreshold)
			: EDarkwellLightPressureKind::None;
		if (LightPressure == EDarkwellLightPressureKind::LanternFocus)
		{
			const FVector ToEnemy = Enemy->GetActorLocation() - TrackedPlayer->GetActorLocation();
			const float DistanceToEnemy = ToEnemy.Size2D();
			const float FacingDot = FVector::DotProduct(
				TrackedPlayer->GetActorForwardVector().GetSafeNormal2D(),
				ToEnemy.GetSafeNormal2D());
			bFocusHitsEnemy = DistanceToEnemy <= FocusRange
				&& FacingDot >= FacingThreshold
				&& HasClearLightPathTo(*TrackedPlayer);
		}
	}

	if (LightControlRemaining > 0.0f)
	{
		LanternStunBuildup = 0.0f;
		return;
	}

	LanternStunBuildup = Darkwell::EnemyMath::UpdateLanternStunBuildup(
		LanternStunBuildup,
		bFocusHitsEnemy,
		DeltaSeconds,
		Enemy->GetLanternFocusSecondsToStun(),
		Enemy->GetLanternFocusDecayPerSecond());
	if (LanternStunBuildup >= 1.0f - UE_KINDA_SMALL_NUMBER)
	{
		LanternStunBuildup = 0.0f;
		ApplyLightControl(Enemy->GetLanternFocusStunDuration());
	}
}

bool ADarkwellStalkerController::HasClearLightPathTo(const ADarkwellCharacter& Player) const
{
	const ADarkwellStalkerCharacter* Enemy = Cast<ADarkwellStalkerCharacter>(GetPawn());
	if (!Enemy || !GetWorld())
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DarkwellLanternFocus), false, &Player);
	FHitResult Hit;
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
		Hit,
		Player.GetActorLocation() + FVector::UpVector * 45.0f,
		Enemy->GetActorLocation() + FVector::UpVector * 45.0f,
		ECC_Visibility,
		QueryParams);
	return !bBlocked || Hit.GetActor() == Enemy;
}

void ADarkwellStalkerController::HandleTargetPerceptionUpdated(AActor* Actor, const FAIStimulus Stimulus)
{
	ADarkwellCharacter* Player = Cast<ADarkwellCharacter>(Actor);
	if (!Player)
	{
		return;
	}

	const FAISenseID SightSense = UAISense::GetSenseID<UAISense_Sight>();
	if (Stimulus.Type == SightSense)
	{
		bHasVisualContact = Stimulus.WasSuccessfullySensed();
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		TrackedPlayer = Player;
		LastKnownPlayerLocation = Stimulus.StimulusLocation;
		if (LastKnownPlayerLocation.ContainsNaN())
		{
			LastKnownPlayerLocation = Player->GetActorLocation();
		}
		AlertTimeRemaining = InvestigationDuration;
	}
}

void ADarkwellStalkerController::UpdateBehavior()
{
	ADarkwellStalkerCharacter* Enemy = Cast<ADarkwellStalkerCharacter>(GetPawn());
	if (!Enemy || !Enemy->IsAlive())
	{
		StopMovement();
		return;
	}
	if (LoadSafetyGraceRemaining > 0.0f)
	{
		SetEnemyState(DarkwellGameplayTags::State_Enemy_Idle);
		StopMovement();
		return;
	}

	if (!IsValid(TrackedPlayer))
	{
		TrackedPlayer = Cast<ADarkwellCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	}

	if (!TrackedPlayer || !TrackedPlayer->CanAcceptGameplayInput())
	{
		SetEnemyState(DarkwellGameplayTags::State_Enemy_Idle);
		StopMovement();
		return;
	}
	if (LightControlRemaining > 0.0f)
	{
		SetEnemyState(DarkwellGameplayTags::State_Enemy_LightStunned);
		StopMovement();
		return;
	}

	const bool bHasClearAwareness = bHasVisualContact
		|| (AlertTimeRemaining > 0.0f && LineOfSightTo(TrackedPlayer));
	if (bHasClearAwareness)
	{
		LastKnownPlayerLocation = TrackedPlayer->GetActorLocation();
		AlertTimeRemaining = InvestigationDuration;
	}

	const FVector PlayerToEnemy = (Enemy->GetActorLocation() - TrackedPlayer->GetActorLocation()).GetSafeNormal2D();
	const FVector PlayerFacing = TrackedPlayer->GetActorForwardVector().GetSafeNormal2D();
	const float PlayerFacingDot = FVector::DotProduct(PlayerFacing, PlayerToEnemy);
	const float DistanceToPlayer = FVector::Dist2D(Enemy->GetActorLocation(), TrackedPlayer->GetActorLocation());
	const UDarkwellLoadoutComponent* Loadout = TrackedPlayer->GetLoadoutComponent();
	float LightPressureRange = 0.0f;
	float FacingThreshold = 1.0f;
	const EDarkwellLightPressureKind LightPressure = Loadout
		? Loadout->GetActiveLightPressure(LightPressureRange, FacingThreshold)
		: EDarkwellLightPressureKind::None;
	const float EffectiveTorchRange = LightPressureRange * Enemy->GetTorchDeterrenceRangeScale();

	const EDarkwellEnemyIntent Intent = Darkwell::EnemyMath::ChooseIntent(
		bHasClearAwareness,
		AlertTimeRemaining > 0.0f,
		LightPressure == EDarkwellLightPressureKind::TorchDeterrent,
		DistanceToPlayer,
		PlayerFacingDot,
		EffectiveTorchRange,
		FacingThreshold,
		Enemy->GetTorchBoundaryBuffer());

	switch (Intent)
	{
	case EDarkwellEnemyIntent::Repel:
		SetEnemyState(DarkwellGameplayTags::State_Enemy_Repelled);
		MoveToLocation(
			Darkwell::EnemyMath::MakeBoundaryDestination(
				Enemy->GetActorLocation(),
				TrackedPlayer->GetActorLocation(),
				EffectiveTorchRange + Enemy->GetTorchBoundarySafetyMargin()),
			35.0f,
			true,
			true,
			false,
			true);
		break;

	case EDarkwellEnemyIntent::HoldAtBay:
		SetEnemyState(DarkwellGameplayTags::State_Enemy_Repelled);
		StopMovement();
		break;

	case EDarkwellEnemyIntent::Hunt:
		SetEnemyState(DarkwellGameplayTags::State_Enemy_Hunting);
		MoveToActor(TrackedPlayer, 10.0f, false, true, true, nullptr, true);
		if (DistanceToPlayer <= Enemy->GetAttackRange() && AttackCooldownRemaining <= 0.0f)
		{
			UGameplayStatics::ApplyDamage(
				TrackedPlayer,
				Enemy->GetAttackDamage(),
				this,
				Enemy,
				UDamageType::StaticClass());
			AttackCooldownRemaining = Enemy->GetAttackInterval();
		}
		break;

	case EDarkwellEnemyIntent::Investigate:
		SetEnemyState(DarkwellGameplayTags::State_Enemy_Investigating);
		MoveToLocation(LastKnownPlayerLocation, 90.0f, true, true, false, true);
		break;

	case EDarkwellEnemyIntent::Idle:
	default:
		SetEnemyState(DarkwellGameplayTags::State_Enemy_Idle);
		StopMovement();
		break;
	}
}

void ADarkwellStalkerController::SetEnemyState(const FGameplayTag State) const
{
	if (ADarkwellStalkerCharacter* Enemy = Cast<ADarkwellStalkerCharacter>(GetPawn()))
	{
		Enemy->SetBehaviorState(State);
	}
}
