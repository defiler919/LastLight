#include "SightWeaveActors.h"

ASightWeaveFloorActor::ASightWeaveFloorActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	FloorComponent = CreateDefaultSubobject<USightWeaveFloorComponent>(TEXT("SightWeaveFloor"));
	FloorComponent->SetupAttachment(SceneRoot);
}

ASightWeaveVisionSourceActor::ASightWeaveVisionSourceActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	VisionSourceComponent = CreateDefaultSubobject<USightWeaveVisionSourceComponent>(TEXT("SightWeaveVisionSource"));
	VisionSourceComponent->SetupAttachment(SceneRoot);
}

ASightWeaveIlluminationSourceActor::ASightWeaveIlluminationSourceActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	IlluminationSourceComponent = CreateDefaultSubobject<USightWeaveIlluminationSourceComponent>(TEXT("SightWeaveIlluminationSource"));
	IlluminationSourceComponent->SetupAttachment(SceneRoot);
}

ASightWeaveOccluderActor::ASightWeaveOccluderActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	OccluderComponent = CreateDefaultSubobject<USightWeaveOccluderComponent>(TEXT("SightWeaveOccluder"));
	OccluderComponent->SetupAttachment(SceneRoot);
}

ASightWeaveHardSuppressionActor::ASightWeaveHardSuppressionActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	HardSuppressionComponent = CreateDefaultSubobject<USightWeaveHardSuppressionComponent>(TEXT("SightWeaveHardSuppression"));
	HardSuppressionComponent->SetupAttachment(SceneRoot);
}

ASightWeaveDebugQueryActor::ASightWeaveDebugQueryActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	DebugQueryComponent = CreateDefaultSubobject<USightWeaveDebugQueryComponent>(TEXT("SightWeaveDebugQuery"));
	DebugQueryComponent->SetupAttachment(SceneRoot);
}
