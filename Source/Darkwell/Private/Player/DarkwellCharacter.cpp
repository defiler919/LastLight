// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/DarkwellCharacter.h"

#include "Camera/CameraComponent.h"
#include "Combat/DarkwellLoadoutComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "Gameplay/DarkwellSurvivalRules.h"
#include "Gameplay/DarkwellVisibilityComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "Interaction/DarkwellInteractionComponent.h"
#include "Inventory/DarkwellInventoryComponent.h"
#include "Player/DarkwellPlayerController.h"
#include "Player/DarkwellPlayerMath.h"
#include "UObject/ConstructorHelpers.h"

ADarkwellCharacter::ADarkwellCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 88.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->MaxAcceleration = 1800.0f;
	Movement->BrakingDecelerationWalking = 1600.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->TargetArmLength = 1200.0f;
	CameraBoom->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	CameraBoom->bDoCollisionTest = false;

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCamera->bUsePawnControlRotation = false;

	GreyboxBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GreyboxBody"));
	GreyboxBody->SetupAttachment(GetCapsuleComponent());
	GreyboxBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GreyboxBody->SetCanEverAffectNavigation(false);
	GreyboxBody->SetRelativeScale3D(FVector(0.72f, 0.72f, 1.4f));

	AimMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AimMarker"));
	AimMarker->SetupAttachment(GetCapsuleComponent());
	AimMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AimMarker->SetCanEverAffectNavigation(false);
	AimMarker->SetRelativeLocation(FVector(70.0f, 0.0f, 0.0f));
	AimMarker->SetRelativeScale3D(FVector(0.45f, 0.08f, 0.08f));

	ShotgunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShotgunMesh"));
	ShotgunMesh->SetupAttachment(GetCapsuleComponent());
	ShotgunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShotgunMesh->SetCanEverAffectNavigation(false);
	ShotgunMesh->SetRelativeLocation(FVector(42.0f, -25.0f, 45.0f));
	ShotgunMesh->SetRelativeScale3D(FVector(0.72f, 0.09f, 0.09f));

	TorchMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TorchMesh"));
	TorchMesh->SetupAttachment(GetCapsuleComponent());
	TorchMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TorchMesh->SetCanEverAffectNavigation(false);
	TorchMesh->SetRelativeLocation(FVector(28.0f, 27.0f, 42.0f));
	TorchMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	TorchMesh->SetRelativeScale3D(FVector(0.08f, 0.08f, 0.32f));

	TorchLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("TorchLight"));
	TorchLight->SetupAttachment(GetCapsuleComponent());
	TorchLight->SetRelativeLocation(FVector(48.0f, 27.0f, 48.0f));
	TorchLight->SetLightColor(FLinearColor(1.0f, 0.72f, 0.38f));
	TorchLight->SetIntensity(5200.0f);
	TorchLight->SetAttenuationRadius(1250.0f);
	TorchLight->SetSourceRadius(6.0f);
	TorchLight->SetSoftSourceRadius(12.0f);
	TorchLight->ShadowResolutionScale = 4.0f;
	TorchLight->SetVisibility(false);

	LanternMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LanternMesh"));
	LanternMesh->SetupAttachment(GetCapsuleComponent());
	LanternMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LanternMesh->SetCanEverAffectNavigation(false);
	LanternMesh->SetRelativeLocation(FVector(30.0f, 27.0f, 39.0f));
	LanternMesh->SetRelativeScale3D(FVector(0.15f, 0.12f, 0.22f));
	LanternMesh->SetVisibility(false);

	LanternBaseLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("LanternBaseLight"));
	LanternBaseLight->SetupAttachment(GetCapsuleComponent());
	LanternBaseLight->SetRelativeLocation(FVector(38.0f, 27.0f, 50.0f));
	LanternBaseLight->SetLightColor(FLinearColor(0.78f, 0.9f, 1.0f));
	LanternBaseLight->SetIntensity(3000.0f);
	LanternBaseLight->SetAttenuationRadius(900.0f);
	LanternBaseLight->SetSourceRadius(10.0f);
	LanternBaseLight->SetSoftSourceRadius(20.0f);
	LanternBaseLight->ShadowResolutionScale = 3.0f;
	LanternBaseLight->SetVisibility(false);

	LanternFocusLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("LanternFocusLight"));
	LanternFocusLight->SetupAttachment(GetCapsuleComponent());
	LanternFocusLight->SetRelativeLocation(FVector(52.0f, 27.0f, 52.0f));
	LanternFocusLight->SetRelativeRotation(FRotator(-3.0f, 0.0f, 0.0f));
	LanternFocusLight->SetLightColor(FLinearColor(0.72f, 0.88f, 1.0f));
	LanternFocusLight->SetIntensity(13500.0f);
	LanternFocusLight->SetAttenuationRadius(2200.0f);
	LanternFocusLight->SetInnerConeAngle(8.0f);
	LanternFocusLight->SetOuterConeAngle(18.0f);
	LanternFocusLight->SetSourceRadius(8.0f);
	LanternFocusLight->SetSoftSourceRadius(16.0f);
	LanternFocusLight->ShadowResolutionScale = 4.0f;
	LanternFocusLight->SetVisibility(false);

	InventoryComponent = CreateDefaultSubobject<UDarkwellInventoryComponent>(TEXT("InventoryComponent"));
	InventoryComponent->InitializeInventory(16);
	LoadoutComponent = CreateDefaultSubobject<UDarkwellLoadoutComponent>(TEXT("LoadoutComponent"));
	InteractionComponent = CreateDefaultSubobject<UDarkwellInteractionComponent>(TEXT("InteractionComponent"));
	VisibilityComponent = CreateDefaultSubobject<UDarkwellVisibilityComponent>(TEXT("VisibilityComponent"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		GreyboxBody->SetStaticMesh(CylinderMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		AimMarker->SetStaticMesh(CubeMesh.Object);
		ShotgunMesh->SetStaticMesh(CubeMesh.Object);
		LanternMesh->SetStaticMesh(CubeMesh.Object);
	}

	if (CylinderMesh.Succeeded())
	{
		TorchMesh->SetStaticMesh(CylinderMesh.Object);
	}

	DefaultMappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("DefaultMappingContext"));
	MoveForwardAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveForwardAction"));
	MoveBackwardAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveBackwardAction"));
	MoveLeftAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveLeftAction"));
	MoveRightAction = CreateDefaultSubobject<UInputAction>(TEXT("MoveRightAction"));
	InteractAction = CreateDefaultSubobject<UInputAction>(TEXT("InteractAction"));
	UseRightHandAction = CreateDefaultSubobject<UInputAction>(TEXT("UseRightHandAction"));
	LeftWeaponWheelAction = CreateDefaultSubobject<UInputAction>(TEXT("LeftWeaponWheelAction"));
	RightWeaponWheelAction = CreateDefaultSubobject<UInputAction>(TEXT("RightWeaponWheelAction"));
	FireAction = CreateDefaultSubobject<UInputAction>(TEXT("FireAction"));
	ReloadAction = CreateDefaultSubobject<UInputAction>(TEXT("ReloadAction"));
	BackpackAction = CreateDefaultSubobject<UInputAction>(TEXT("BackpackAction"));
	TakeAllAction = CreateDefaultSubobject<UInputAction>(TEXT("TakeAllAction"));
	SprintAction = CreateDefaultSubobject<UInputAction>(TEXT("SprintAction"));

	MoveForwardAction->ValueType = EInputActionValueType::Boolean;
	MoveBackwardAction->ValueType = EInputActionValueType::Boolean;
	MoveLeftAction->ValueType = EInputActionValueType::Boolean;
	MoveRightAction->ValueType = EInputActionValueType::Boolean;
	InteractAction->ValueType = EInputActionValueType::Boolean;
	UseRightHandAction->ValueType = EInputActionValueType::Boolean;
	LeftWeaponWheelAction->ValueType = EInputActionValueType::Boolean;
	RightWeaponWheelAction->ValueType = EInputActionValueType::Boolean;
	FireAction->ValueType = EInputActionValueType::Boolean;
	ReloadAction->ValueType = EInputActionValueType::Boolean;
	BackpackAction->ValueType = EInputActionValueType::Boolean;
	TakeAllAction->ValueType = EInputActionValueType::Boolean;
	SprintAction->ValueType = EInputActionValueType::Boolean;

	DefaultMappingContext->MapKey(MoveForwardAction, EKeys::W);
	DefaultMappingContext->MapKey(MoveBackwardAction, EKeys::S);
	DefaultMappingContext->MapKey(MoveLeftAction, EKeys::A);
	DefaultMappingContext->MapKey(MoveRightAction, EKeys::D);
	DefaultMappingContext->MapKey(InteractAction, EKeys::F);
	DefaultMappingContext->MapKey(UseRightHandAction, EKeys::RightMouseButton);
	DefaultMappingContext->MapKey(LeftWeaponWheelAction, EKeys::Q);
	DefaultMappingContext->MapKey(RightWeaponWheelAction, EKeys::E);
	DefaultMappingContext->MapKey(FireAction, EKeys::LeftMouseButton);
	DefaultMappingContext->MapKey(ReloadAction, EKeys::R);
	DefaultMappingContext->MapKey(BackpackAction, EKeys::Tab);
	DefaultMappingContext->MapKey(TakeAllAction, EKeys::T);
	DefaultMappingContext->MapKey(SprintAction, EKeys::LeftShift);

	LoadoutComponent->SetRightHandPresentation(
		TorchMesh,
		TorchLight,
		LanternMesh,
		LanternBaseLight,
		LanternFocusLight);
}

void ADarkwellCharacter::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	LifeState = DarkwellGameplayTags::State_Player_Alive;
	CompletionState = FGameplayTag::EmptyTag;
	MovementState = DarkwellGameplayTags::State_Player_Movement_Walking;
	bSprintRequested = false;
	bMoveForwardRequested = false;
	bMoveBackwardRequested = false;
	bMoveLeftRequested = false;
	bMoveRightRequested = false;
	ResetPrimaryFireGesture();
	InvulnerableUntilTimeSeconds = 0.0;
	LastDamageTimeSeconds = -1.0;
}

void ADarkwellCharacter::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdatePrimaryFireGesture(DeltaTime);
	UpdateFacingAndMovement(DeltaTime);
}

float ADarkwellCharacter::TakeDamage(
	const float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	if (!CanAcceptGameplayInput())
	{
		return 0.0f;
	}

	const double CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (!Darkwell::SurvivalRules::CanApplyDamage(
		Health,
		CurrentTimeSeconds,
		InvulnerableUntilTimeSeconds))
	{
		return 0.0f;
	}

	const float AppliedDamage = Darkwell::SurvivalRules::ComputeAppliedDamage(Health, DamageAmount);
	if (AppliedDamage <= 0.0f)
	{
		return 0.0f;
	}

	Health -= AppliedDamage;
	LastDamageTimeSeconds = CurrentTimeSeconds;
	InvulnerableUntilTimeSeconds = CurrentTimeSeconds + DamageInvulnerabilitySeconds;
	if (Health <= 0.0f)
	{
		Health = 0.0f;
		HandleDeath();
	}
	return AppliedDamage;
}

float ADarkwellCharacter::GetDamageFeedbackAlpha() const
{
	const double CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	return Darkwell::SurvivalRules::ComputeDamageFeedbackAlpha(
		CurrentTimeSeconds,
		LastDamageTimeSeconds,
		DamageFeedbackDurationSeconds);
}

bool ADarkwellCharacter::IsAlive() const
{
	return Health > 0.0f && LifeState != DarkwellGameplayTags::State_Player_Dead;
}

bool ADarkwellCharacter::HasEscaped() const
{
	return CompletionState == DarkwellGameplayTags::State_Player_Escaped;
}

bool ADarkwellCharacter::CanAcceptGameplayInput() const
{
	const ADarkwellPlayerController* PlayerController = Cast<ADarkwellPlayerController>(GetController());
	return IsAlive() && !HasEscaped() && (!PlayerController || !PlayerController->IsMenuOpen());
}

bool ADarkwellCharacter::IsInventoryOpen() const
{
	const ADarkwellPlayerController* PlayerController = Cast<ADarkwellPlayerController>(GetController());
	return PlayerController && PlayerController->IsInventoryOpen();
}

bool ADarkwellCharacter::IsSprinting() const
{
	return MovementState == DarkwellGameplayTags::State_Player_Movement_Sprinting;
}

float ADarkwellCharacter::GetShotgunAimProgress() const
{
	return bPrimaryFireHeld
		? Darkwell::PlayerMath::ComputePrimaryFireAimProgress(
			PrimaryFireHeldSeconds,
			PrimaryFireAimTightenDuration)
		: 0.0f;
}

void ADarkwellCharacter::GrantLoadProtection(const float DurationSeconds)
{
	const double CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	InvulnerableUntilTimeSeconds = FMath::Max(
		InvulnerableUntilTimeSeconds,
		CurrentTimeSeconds + FMath::Max(0.0f, DurationSeconds));
}

void ADarkwellCharacter::CompleteEscape()
{
	if (!IsAlive() || HasEscaped())
	{
		return;
	}

	CompletionState = DarkwellGameplayTags::State_Player_Escaped;
	if (ADarkwellPlayerController* PlayerController = Cast<ADarkwellPlayerController>(GetController()))
	{
		PlayerController->CloseInventory();
	}
	ActiveWeaponWheel = EDarkwellWeaponWheelSide::None;
	bSprintRequested = false;
	bMoveForwardRequested = false;
	bMoveBackwardRequested = false;
	bMoveLeftRequested = false;
	bMoveRightRequested = false;
	ResetPrimaryFireGesture();
	MovementState = DarkwellGameplayTags::State_Player_Movement_Walking;
	UpdateInteractionFocus(nullptr);
	GetCharacterMovement()->DisableMovement();
	LoadoutComponent->DeactivateForOwnerIncapacitated();
}

void ADarkwellCharacter::RestorePersistentState(
	const FTransform& SavedTransform,
	const float SavedHealth,
	const FGameplayTag SavedLifeState,
	const FGameplayTag SavedCompletionState)
{
	Health = FMath::Clamp(SavedHealth, 0.0f, MaxHealth);
	LifeState = SavedLifeState == DarkwellGameplayTags::State_Player_Dead || Health <= 0.0f
		? DarkwellGameplayTags::State_Player_Dead
		: DarkwellGameplayTags::State_Player_Alive;
	if (LifeState == DarkwellGameplayTags::State_Player_Dead)
	{
		Health = 0.0f;
	}
	CompletionState = SavedCompletionState == DarkwellGameplayTags::State_Player_Escaped
		? DarkwellGameplayTags::State_Player_Escaped
		: FGameplayTag::EmptyTag;
	InvulnerableUntilTimeSeconds = 0.0;
	LastDamageTimeSeconds = -1.0;
	ActiveWeaponWheel = EDarkwellWeaponWheelSide::None;
	bSprintRequested = false;
	bMoveForwardRequested = false;
	bMoveBackwardRequested = false;
	bMoveLeftRequested = false;
	bMoveRightRequested = false;
	ResetPrimaryFireGesture();
	MovementState = DarkwellGameplayTags::State_Player_Movement_Walking;
	UpdateInteractionFocus(nullptr);
	SetActorTransform(SavedTransform, false, nullptr, ETeleportType::TeleportPhysics);

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		if (CanAcceptGameplayInput())
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
		else
		{
			Movement->DisableMovement();
			LoadoutComponent->DeactivateForOwnerIncapacitated();
		}
	}
}

void ADarkwellCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	AddDefaultInputMapping();

	UEnhancedInputComponent* EnhancedInput = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	EnhancedInput->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &ThisClass::MoveForward);
	EnhancedInput->BindAction(MoveForwardAction, ETriggerEvent::Completed, this, &ThisClass::MoveForward);
	EnhancedInput->BindAction(MoveForwardAction, ETriggerEvent::Canceled, this, &ThisClass::MoveForward);
	EnhancedInput->BindAction(MoveBackwardAction, ETriggerEvent::Triggered, this, &ThisClass::MoveBackward);
	EnhancedInput->BindAction(MoveBackwardAction, ETriggerEvent::Completed, this, &ThisClass::MoveBackward);
	EnhancedInput->BindAction(MoveBackwardAction, ETriggerEvent::Canceled, this, &ThisClass::MoveBackward);
	EnhancedInput->BindAction(MoveLeftAction, ETriggerEvent::Triggered, this, &ThisClass::MoveLeft);
	EnhancedInput->BindAction(MoveLeftAction, ETriggerEvent::Completed, this, &ThisClass::MoveLeft);
	EnhancedInput->BindAction(MoveLeftAction, ETriggerEvent::Canceled, this, &ThisClass::MoveLeft);
	EnhancedInput->BindAction(MoveRightAction, ETriggerEvent::Triggered, this, &ThisClass::MoveRight);
	EnhancedInput->BindAction(MoveRightAction, ETriggerEvent::Completed, this, &ThisClass::MoveRight);
	EnhancedInput->BindAction(MoveRightAction, ETriggerEvent::Canceled, this, &ThisClass::MoveRight);
	EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &ThisClass::Interact);
	EnhancedInput->BindAction(UseRightHandAction, ETriggerEvent::Started, this, &ThisClass::BeginUseRightHand);
	EnhancedInput->BindAction(UseRightHandAction, ETriggerEvent::Completed, this, &ThisClass::EndUseRightHand);
	EnhancedInput->BindAction(UseRightHandAction, ETriggerEvent::Canceled, this, &ThisClass::EndUseRightHand);
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &ThisClass::BeginPrimaryFire);
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &ThisClass::EndPrimaryFire);
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Canceled, this, &ThisClass::CancelPrimaryFire);
	EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &ThisClass::ReloadShotgun);
	EnhancedInput->BindAction(BackpackAction, ETriggerEvent::Started, this, &ThisClass::ToggleBackpack);
	EnhancedInput->BindAction(TakeAllAction, ETriggerEvent::Started, this, &ThisClass::TakeAllInventory);
	EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &ThisClass::BeginSprint);
	EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed, this, &ThisClass::EndSprint);
	EnhancedInput->BindAction(SprintAction, ETriggerEvent::Canceled, this, &ThisClass::EndSprint);
}

void ADarkwellCharacter::AimAtWorldPoint(const FVector& WorldPoint)
{
	if (!CanAcceptGameplayInput() || IsInventoryOpen())
	{
		return;
	}

	FVector AimDirection;
	if (Darkwell::PlayerMath::TryGetPlanarDirection(GetActorLocation(), WorldPoint, AimDirection))
	{
		CurrentAimPoint = WorldPoint;
		bHasAimPoint = true;
	}
}

void ADarkwellCharacter::UpdateFacingAndMovement(const float DeltaTime)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	const bool bCanMove = CanAcceptGameplayInput() && !IsInventoryOpen();
	const FVector MovementDirection = GetRequestedMovementDirection();

	const bool bShouldSprint = Darkwell::PlayerMath::ShouldSprint(
		bSprintRequested,
		bCanMove,
		MovementDirection);
	MovementState = bShouldSprint
		? DarkwellGameplayTags::State_Player_Movement_Sprinting.GetTag()
		: DarkwellGameplayTags::State_Player_Movement_Walking.GetTag();
	const float BaseSpeed = bShouldSprint ? SprintSpeed : WalkSpeed;
	const float DirectionalSpeedScale = MovementDirection.IsNearlyZero()
		? 1.0f
		: Darkwell::PlayerMath::ComputeDirectionalSpeedScale(
			GetActorForwardVector(),
			MovementDirection,
			StrafeSpeedScale,
			BackpedalSpeedScale);
	Movement->MaxWalkSpeed = BaseSpeed * DirectionalSpeedScale;

	if (!bCanMove)
	{
		return;
	}

	FVector DesiredFacing = FVector::ZeroVector;
	if (bShouldSprint)
	{
		DesiredFacing = MovementDirection;
	}
	else if (bHasAimPoint)
	{
		Darkwell::PlayerMath::TryGetPlanarDirection(GetActorLocation(), CurrentAimPoint, DesiredFacing);
	}

	if (!DesiredFacing.IsNearlyZero())
	{
		const float TurnRate = bShouldSprint
			? SprintTurnRateDegreesPerSecond
			: WalkTurnRateDegreesPerSecond;
		const float NewYaw = Darkwell::PlayerMath::TurnYawToward(
			GetActorRotation().Yaw,
			DesiredFacing.Rotation().Yaw,
			TurnRate,
			DeltaTime);
		SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
	}
}

void ADarkwellCharacter::UpdateInteractionFocus(AActor* Candidate)
{
	InteractionComponent->UpdateFocusedActor(CanAcceptGameplayInput() ? Candidate : nullptr);
}

void ADarkwellCharacter::RefreshFacingInteractionFocus()
{
	if (CanAcceptGameplayInput())
	{
		InteractionComponent->UpdateFocusedActorFromWorld();
	}
	else
	{
		InteractionComponent->UpdateFocusedActor(nullptr);
	}
}

void ADarkwellCharacter::UpdateWeaponWheelInput(const bool bLeftDown, const bool bRightDown)
{
	if (!CanAcceptGameplayInput() || IsInventoryOpen())
	{
		ResetPrimaryFireGesture();
		LoadoutComponent->CancelRightHandUse();
		ActiveWeaponWheel = EDarkwellWeaponWheelSide::None;
		bLeftWeaponWheelWasDown = bLeftDown;
		bRightWeaponWheelWasDown = bRightDown;
		return;
	}

	const bool bRightJustPressed = bRightDown && !bRightWeaponWheelWasDown;
	const bool bCommitRightSelection = Darkwell::WeaponWheelRules::ShouldCycleRightHandItem(
		bRightDown,
		bRightWeaponWheelWasDown,
		ActiveWeaponWheel);
	if (bRightJustPressed)
	{
		ResetPrimaryFireGesture();
		LoadoutComponent->CancelRightHandUse();
	}

	ActiveWeaponWheel = Darkwell::WeaponWheelRules::ResolveActiveWheel(
		bLeftDown,
		bRightDown,
		bLeftWeaponWheelWasDown,
		bRightWeaponWheelWasDown,
		ActiveWeaponWheel);
	bLeftWeaponWheelWasDown = bLeftDown;
	bRightWeaponWheelWasDown = bRightDown;
	if (bCommitRightSelection)
	{
		LoadoutComponent->CycleRightHandItem();
	}
}

void ADarkwellCharacter::AddDefaultInputMapping()
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
	{
		InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
	}
}

void ADarkwellCharacter::MoveForward(const FInputActionValue& Value)
{
	bMoveForwardRequested = Value.Get<bool>();
	if (bMoveForwardRequested)
	{
		MoveAlongCameraAxes(1.0f, 0.0f);
	}
}

void ADarkwellCharacter::MoveBackward(const FInputActionValue& Value)
{
	bMoveBackwardRequested = Value.Get<bool>();
	if (bMoveBackwardRequested)
	{
		MoveAlongCameraAxes(-1.0f, 0.0f);
	}
}

void ADarkwellCharacter::MoveLeft(const FInputActionValue& Value)
{
	bMoveLeftRequested = Value.Get<bool>();
	if (bMoveLeftRequested)
	{
		MoveAlongCameraAxes(0.0f, -1.0f);
	}
}

void ADarkwellCharacter::MoveRight(const FInputActionValue& Value)
{
	bMoveRightRequested = Value.Get<bool>();
	if (bMoveRightRequested)
	{
		MoveAlongCameraAxes(0.0f, 1.0f);
	}
}

void ADarkwellCharacter::MoveAlongCameraAxes(const float ForwardAmount, const float RightAmount)
{
	if (!CanAcceptGameplayInput() || IsInventoryOpen())
	{
		return;
	}

	const FRotator CameraYaw(0.0f, TopDownCamera->GetComponentRotation().Yaw, 0.0f);
	const FVector ForwardDirection = FRotationMatrix(CameraYaw).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(CameraYaw).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, ForwardAmount);
	AddMovementInput(RightDirection, RightAmount);
}

FVector ADarkwellCharacter::GetRequestedMovementDirection() const
{
	const float ForwardAmount = static_cast<float>(bMoveForwardRequested)
		- static_cast<float>(bMoveBackwardRequested);
	const float RightAmount = static_cast<float>(bMoveRightRequested)
		- static_cast<float>(bMoveLeftRequested);
	if (FMath::IsNearlyZero(ForwardAmount) && FMath::IsNearlyZero(RightAmount))
	{
		return FVector::ZeroVector;
	}

	const FRotator CameraYaw(0.0f, TopDownCamera->GetComponentRotation().Yaw, 0.0f);
	const FVector ForwardDirection = FRotationMatrix(CameraYaw).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(CameraYaw).GetUnitAxis(EAxis::Y);
	return (ForwardDirection * ForwardAmount + RightDirection * RightAmount).GetSafeNormal2D();
}

void ADarkwellCharacter::BeginSprint(const FInputActionValue& Value)
{
	bSprintRequested = Value.Get<bool>();
}

void ADarkwellCharacter::EndSprint(const FInputActionValue& Value)
{
	bSprintRequested = false;
}

void ADarkwellCharacter::Interact(const FInputActionValue& Value)
{
	if (IsInventoryOpen())
	{
		if (ADarkwellPlayerController* PlayerController = Cast<ADarkwellPlayerController>(GetController()))
		{
			PlayerController->CloseInventory();
		}
		return;
	}

	if (CanAcceptGameplayInput() && !IsWeaponWheelOpen())
	{
		InteractionComponent->TryInteract();
	}
}

void ADarkwellCharacter::BeginUseRightHand(const FInputActionValue& Value)
{
	if (!CanAcceptGameplayInput())
	{
		return;
	}

	if (IsInventoryOpen())
	{
		if (ADarkwellPlayerController* PlayerController = Cast<ADarkwellPlayerController>(GetController()))
		{
			PlayerController->HandleInventoryClick(true);
		}
		return;
	}

	if (!IsWeaponWheelOpen())
	{
		LoadoutComponent->BeginRightHandUse();
	}
}

void ADarkwellCharacter::EndUseRightHand(const FInputActionValue& Value)
{
	if (!CanAcceptGameplayInput() || IsInventoryOpen() || IsWeaponWheelOpen())
	{
		LoadoutComponent->CancelRightHandUse();
		return;
	}
	LoadoutComponent->EndRightHandUse();
}

void ADarkwellCharacter::BeginPrimaryFire(const FInputActionValue& Value)
{
	if (!CanAcceptGameplayInput())
	{
		return;
	}

	if (IsInventoryOpen())
	{
		if (ADarkwellPlayerController* PlayerController = Cast<ADarkwellPlayerController>(GetController()))
		{
			PlayerController->HandleInventoryClick(false);
		}
		return;
	}

	if (IsWeaponWheelOpen())
	{
		return;
	}

	bPrimaryFireHeld = true;
	bShotgunAiming = false;
	PrimaryFireHeldSeconds = 0.0f;
}

void ADarkwellCharacter::EndPrimaryFire(const FInputActionValue& Value)
{
	if (!bPrimaryFireHeld)
	{
		return;
	}

	const bool bAimedShot = bShotgunAiming;
	const float AimProgress = bAimedShot ? GetShotgunAimProgress() : 0.0f;
	ResetPrimaryFireGesture();
	if (CanAcceptGameplayInput() && !IsInventoryOpen() && !IsWeaponWheelOpen())
	{
		FireShotgun(AimProgress);
	}
}

void ADarkwellCharacter::CancelPrimaryFire(const FInputActionValue& Value)
{
	ResetPrimaryFireGesture();
}

void ADarkwellCharacter::ResetPrimaryFireGesture()
{
	bPrimaryFireHeld = false;
	bShotgunAiming = false;
	PrimaryFireHeldSeconds = 0.0f;
}

void ADarkwellCharacter::UpdatePrimaryFireGesture(const float DeltaTime)
{
	if (!bPrimaryFireHeld)
	{
		return;
	}
	if (!CanAcceptGameplayInput() || IsInventoryOpen() || IsWeaponWheelOpen()
		|| LoadoutComponent->IsReloading())
	{
		ResetPrimaryFireGesture();
		return;
	}

	PrimaryFireHeldSeconds += FMath::Max(0.0f, DeltaTime);
	if (Darkwell::PlayerMath::IsPrimaryFireAimActive(
		PrimaryFireHeldSeconds,
		PrimaryFireHoldThreshold))
	{
		bShotgunAiming = true;
	}
}

void ADarkwellCharacter::FireShotgun(const float AimProgress)
{
	const FVector AimPoint = GetActorLocation() + GetActorForwardVector() * 2000.0f;
	LoadoutComponent->TryFire(AimPoint, AimProgress);
}

void ADarkwellCharacter::ReloadShotgun(const FInputActionValue& Value)
{
	if (!CanAcceptGameplayInput())
	{
		if (ADarkwellPlayerController* PlayerController = Cast<ADarkwellPlayerController>(GetController()))
		{
			PlayerController->RequestRestartCurrentLevel();
		}
		return;
	}

	if (!IsInventoryOpen() && !IsWeaponWheelOpen())
	{
		ResetPrimaryFireGesture();
		LoadoutComponent->BeginReload();
	}
}

void ADarkwellCharacter::ToggleBackpack(const FInputActionValue& Value)
{
	if (!CanAcceptGameplayInput())
	{
		return;
	}

	if (ADarkwellPlayerController* PlayerController = Cast<ADarkwellPlayerController>(GetController()))
	{
		if (PlayerController->IsInventoryOpen())
		{
			PlayerController->CloseInventory();
		}
		else
		{
			ResetPrimaryFireGesture();
			LoadoutComponent->CancelRightHandUse();
			PlayerController->OpenBackpack();
		}
	}
}

void ADarkwellCharacter::TakeAllInventory(const FInputActionValue& Value)
{
	if (CanAcceptGameplayInput())
	{
		if (ADarkwellPlayerController* PlayerController = Cast<ADarkwellPlayerController>(GetController()))
		{
			PlayerController->TakeAllFromContainer();
		}
	}
}

void ADarkwellCharacter::HandleDeath()
{
	LifeState = DarkwellGameplayTags::State_Player_Dead;
	if (ADarkwellPlayerController* PlayerController = Cast<ADarkwellPlayerController>(GetController()))
	{
		PlayerController->CloseInventory();
	}
	ActiveWeaponWheel = EDarkwellWeaponWheelSide::None;
	bSprintRequested = false;
	bMoveForwardRequested = false;
	bMoveBackwardRequested = false;
	bMoveLeftRequested = false;
	bMoveRightRequested = false;
	ResetPrimaryFireGesture();
	MovementState = DarkwellGameplayTags::State_Player_Movement_Walking;
	UpdateInteractionFocus(nullptr);
	GetCharacterMovement()->DisableMovement();
	LoadoutComponent->DeactivateForOwnerIncapacitated();
}
