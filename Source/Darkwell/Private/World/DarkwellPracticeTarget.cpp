// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/DarkwellPracticeTarget.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ADarkwellPracticeTarget::ADarkwellPracticeTarget()
{
	PrimaryActorTick.bCanEverTick = false;

	TargetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetMesh"));
	SetRootComponent(TargetMesh);
	TargetMesh->SetRelativeScale3D(FVector(0.65f, 0.65f, 1.8f));
	TargetMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	TargetLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("TargetLight"));
	TargetLight->SetupAttachment(TargetMesh);
	TargetLight->SetRelativeLocation(FVector(0.0f, 0.0f, 65.0f));
	TargetLight->SetLightColor(FLinearColor(0.8f, 0.02f, 0.01f));
	TargetLight->SetIntensity(650.0f);
	TargetLight->SetAttenuationRadius(180.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		TargetMesh->SetStaticMesh(CylinderMesh.Object);
	}
}

float ADarkwellPracticeTarget::TakeDamage(
	const float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	const float AppliedDamage = FMath::Max(0.0f, DamageAmount);
	Health = FMath::Max(0.0f, Health - AppliedDamage);
	if (Health <= 0.0f)
	{
		Destroy();
	}
	else
	{
		const float HealthRatio = Health / MaxHealth;
		TargetLight->SetIntensity(FMath::Lerp(150.0f, 650.0f, HealthRatio));
	}
	return AppliedDamage;
}
