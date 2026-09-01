#include "VisionPresentation/DarkwellMovingPropLabRoom.h"

#include "Camera/CameraComponent.h"
#include "Combat/DarkwellLoadoutComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Gameplay/DarkwellGameplayTags.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/Float16Color.h"
#include "Player/DarkwellCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "VisionPresentation/DarkwellRememberablePropComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDarkwellMovingPropLab, Log, All);

namespace Darkwell::MovingPropLab
{
	constexpr float CellSize = 2.5f;
	constexpr int32 PresentationSamples = 4;
	const FName MainId(TEXT("Lab.Moving.Cabinet"));
	const FVector A(-1100.0f, 650.0f, 0.0f);
	const FVector B(0.0f, 650.0f, 0.0f);
	const FVector C(1100.0f, 650.0f, 0.0f);
	const FName RotateId(TEXT("Lab.InWorld.Rotate.Cabinet"));
	const FName HiddenId(TEXT("Lab.InWorld.Hidden.Cabinet"));
	const FName EdgeId(TEXT("Lab.InWorld.Edge.Cabinet"));
	const FName AbcId(TEXT("Lab.InWorld.ABC.Cabinet"));
	const FName MultiHighId(TEXT("Lab.InWorld.Multi.HighCabinet"));
	const FName MultiLowId(TEXT("Lab.InWorld.Multi.LowCabinet"));
	const FName MultiTableId(TEXT("Lab.InWorld.Multi.LongTable"));
	const FName MultiBoxId(TEXT("Lab.InWorld.Multi.SmallBox"));
	const FVector TranslateA(-1100.0f, 650.0f, 0.0f);
	const FVector TranslateB(-700.0f, 650.0f, 0.0f);
	const FVector RotateA(-300.0f, 650.0f, 0.0f);
	const FVector HiddenA(500.0f, 650.0f, 0.0f);
	const FVector HiddenB(900.0f, 650.0f, 0.0f);
	const FVector EdgeA(1400.0f, 650.0f, 0.0f);
	const FVector EdgeB(1400.0f, -650.0f, 0.0f);
	const FVector AbcA(-1500.0f, 1000.0f, 0.0f);
	const FVector AbcB(-1050.0f, 1000.0f, 0.0f);
	const FVector AbcC(-600.0f, 1000.0f, 0.0f);

	bool IsInWorldControlRequest(const UWorld* World)
	{
		return World && (World->URL.HasOption(TEXT("InWorldControls"))
			|| FParse::Param(FCommandLine::Get(), TEXT("PropLabMovingControls")));
	}

	bool TransformsMatch(const FTransform& Left, const FTransform& Right)
	{
		return Left.GetLocation().Equals(Right.GetLocation(), 0.25f)
			&& Left.GetRotation().Equals(Right.GetRotation(), 1.0e-5f)
			&& Left.GetScale3D().Equals(Right.GetScale3D(), 1.0e-5f);
	}

	void SetMode2()
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (IConsoleVariable* Mode = IConsoleManager::Get().FindConsoleVariable(
			TEXT("r.Darkwell.ProjectFogVisual.PropPresentationMode")))
		{
			Mode->Set(2, ECVF_SetByConsole);
		}
#endif
	}
}

ADarkwellMovingPropLabControl::ADarkwellMovingPropLabControl()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	ControlRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ControlRoot"));
	SetRootComponent(ControlRoot);
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ControlBody"));
	Body->SetupAttachment(ControlRoot);
	Body->SetStaticMesh(Cube.Object);
	Body->SetRelativeScale3D(FVector(1.8f, 1.35f, 0.40f));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionObjectType(ECC_WorldDynamic);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Body->SetGenerateOverlapEvents(false);
	Body->SetCanEverAffectNavigation(false);
	Body->SetCastShadow(false);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ControlLabel"));
	Label->SetupAttachment(ControlRoot);
	Label->SetRelativeLocation(FVector(0, -72, 92));
	Label->SetRelativeRotation(FRotator(0, -90, 0));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(42.0f);
	Label->SetTextRenderColor(FColor(90, 255, 150));
	Label->SetCastShadow(false);

	StatusIndicator = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ControlStatusIndicator"));
	StatusIndicator->SetupAttachment(ControlRoot);
	StatusIndicator->SetRelativeLocation(FVector(0, -74, 155));
	StatusIndicator->SetRelativeRotation(FRotator(0, -90, 0));
	StatusIndicator->SetHorizontalAlignment(EHTA_Center);
	StatusIndicator->SetVerticalAlignment(EVRTA_TextCenter);
	StatusIndicator->SetWorldSize(58.0f);
	StatusIndicator->SetText(FText::FromString(TEXT("●")));
	StatusIndicator->SetTextRenderColor(FColor(90, 255, 150));
	StatusIndicator->SetCastShadow(false);
}

void ADarkwellMovingPropLabControl::Configure(
	ADarkwellMovingPropLabRoom* InRoom,
	const EDarkwellMovingPropLabControlKind InKind,
	const FText& InLabel)
{
	Room = InRoom;
	Kind = InKind;
	BaseLabel = InLabel;
	RefreshDisplay();
}

void ADarkwellMovingPropLabControl::RefreshDisplay()
{
	Label->SetText(Room.IsValid() ? Room->GetInWorldControlDisplay(Kind) : BaseLabel);
	const FColor StateColor = Room.IsValid()
		? Room->GetInWorldControlColor(Kind) : FColor(90, 255, 150);
	Label->SetTextRenderColor(bFocused ? FColor::Yellow : StateColor);
	StatusIndicator->SetTextRenderColor(bFocused ? FColor::White : StateColor);
}

bool ADarkwellMovingPropLabControl::CanInteract(const ADarkwellCharacter&) const
{
	// A focused but blocked control must remain a valid interactable so its prompt
	// can explain BUSY / COMPLETED / DISTANCE state. Activation remains authoritative.
	return Room.IsValid() && Room->CanFocusInWorldControl();
}

void ADarkwellMovingPropLabControl::Interact(ADarkwellCharacter& Character)
{
	if (Room.IsValid())
	{
		Room->ActivateInWorldControl(Kind, Character);
		RefreshDisplay();
	}
}

FText ADarkwellMovingPropLabControl::GetInteractionPrompt(const ADarkwellCharacter&) const
{
	return Room.IsValid() ? Room->GetInWorldControlPrompt(Kind) : BaseLabel;
}

void ADarkwellMovingPropLabControl::OnInteractionFocusChanged(const bool bInFocused)
{
	bFocused = bInFocused;
	RefreshDisplay();
}

bool ADarkwellMovingPropLabControl::TriggerForLabEvidence(ADarkwellCharacter* Character)
{
	if (!Character || !Room.IsValid() || !CanInteract(*Character))
	{
		return false;
	}
	return Room->ActivateInWorldControl(Kind, *Character);
}

ADarkwellMovingPropLabRoom::ADarkwellMovingPropLabRoom()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("MovingRoomRoot")));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	const FVector Centers[] = {
		{0, 0, -15}, {-1800, 0, 125}, {1800, 0, 125},
		{0, -1200, 125}, {0, 1200, 125}, {-300, 0, 125},
		{700, 450, 70}, {700, 790, 70}, {1450, 0, 125}};
	const FVector Sizes[] = {
		{3600, 2400, 30}, {30, 2400, 250}, {30, 2400, 250},
		{3600, 30, 250}, {3600, 30, 250}, {3000, 30, 250},
		{30, 300, 140}, {30, 300, 140}, {30, 300, 250}};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Centers); ++Index)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("MovingRoomStructure%d"), Index));
		Part->SetupAttachment(GetRootComponent());
		Part->SetStaticMesh(Cube.Object);
		Part->SetRelativeLocation(Centers[Index]);
		Part->SetRelativeScale3D(Sizes[Index] / 100.0f);
		Part->SetCollisionProfileName(TEXT("BlockAll"));
		Part->SetRenderCustomDepth(false);
		Structure.Add(Part);
	}
	PressurePlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HiddenMovePressurePlate"));
	PressurePlate->SetupAttachment(GetRootComponent());
	PressurePlate->SetStaticMesh(Cylinder.Object);
	PressurePlate->SetRelativeLocation(FVector(900, -850, 4));
	PressurePlate->SetRelativeScale3D(FVector(1.7f, 1.7f, 0.08f));
	PressurePlate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PressurePlate->SetCastShadow(false);
	PressurePlate->SetVisibility(false);

	PressureLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HiddenMovePressureLabel"));
	PressureLabel->SetupAttachment(PressurePlate);
	PressureLabel->SetRelativeLocation(FVector(0, 0, 900));
	PressureLabel->SetRelativeRotation(FRotator(0, 90, 0));
	PressureLabel->SetHorizontalAlignment(EHTA_Center);
	PressureLabel->SetWorldSize(24.0f);
	PressureLabel->SetText(FText::FromString(TEXT("OFFSCREEN MOVE PRESSURE PLATE\nARM A->B / A->B->C ABOVE, THEN WALK HERE")));
	PressureLabel->SetTextRenderColor(FColor(255, 180, 60));
	PressureLabel->SetVisibility(false);
}

void ADarkwellMovingPropLabRoom::BeginPlay()
{
	Super::BeginPlay();
	bInWorldControls = Darkwell::MovingPropLab::IsInWorldControlRequest(GetWorld());
	const bool bActive = FindActive(GetWorld()) == this;
	SetActorHiddenInGame(!bActive);
	SetActorEnableCollision(bActive);
	PressurePlate->SetVisibility(bActive && bInWorldControls);
	PressureLabel->SetVisibility(bActive && bInWorldControls);
	if (bActive && bInWorldControls)
	{
		SpawnInWorldControls();
	}
}

void ADarkwellMovingPropLabRoom::EndPlay(const EEndPlayReason::Type Reason)
{
	DestroyInWorldControls();
	DestroyTracked();
	Super::EndPlay(Reason);
}

ADarkwellMovingPropLabRoom* ADarkwellMovingPropLabRoom::FindActive(const UWorld* World)
{
	if (!Darkwell::PropLab::IsLabWorld(World)
		|| (!World->URL.HasOption(TEXT("MoveRules"))
			&& !Darkwell::MovingPropLab::IsInWorldControlRequest(World)))
	{
		return nullptr;
	}
	for (TActorIterator<ADarkwellMovingPropLabRoom> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

FBox2D ADarkwellMovingPropLabRoom::FloorBounds() const
{
	return FBox2D(
		FVector2D(GetActorLocation()) + FVector2D(-1800, -1200),
		FVector2D(GetActorLocation()) + FVector2D(1800, 1200));
}

void ADarkwellMovingPropLabRoom::BuildOccluders(
	TArray<FDarkwellVisionIntegrationSegment>& Out) const
{
	Out.Reset();
	for (int32 Index = 1; Index < Structure.Num(); ++Index)
	{
		const FVector Position = Structure[Index]->GetComponentLocation();
		const FVector Size = Structure[Index]->GetComponentScale() * 100.0f;
		const FVector2D Extent = Size.X > Size.Y
			? FVector2D(Size.X * 0.5f, 0.0f)
			: FVector2D(0.0f, Size.Y * 0.5f);
		FDarkwellVisionIntegrationSegment& Segment = Out.AddDefaulted_GetRef();
		Segment.A = FVector2D(Position) - Extent;
		Segment.B = FVector2D(Position) + Extent;
		Segment.ZMin = 0.0f;
		Segment.ZMax = Size.Z;
	}
}

void ADarkwellMovingPropLabRoom::BindRoomPresentation(
	UTexture* Raw,
	const FVector2D Min,
	const FVector2D Inv)
{
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Darkwell/Vision/PropLab/M_PropLabSurface.M_PropLabSurface"));
	for (int32 Index = 0; Index < Structure.Num(); ++Index)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, this);
		Material->SetTextureParameterValue(TEXT("DarkwellLiveCoverageTexture"), Raw);
		Material->SetVectorParameterValue(TEXT("FogWorldMin"), FLinearColor(Min.X, Min.Y, 0, 0));
		Material->SetVectorParameterValue(TEXT("FogWorldInvExtent"), FLinearColor(Inv.X, Inv.Y, 0, 0));
		Material->SetVectorParameterValue(TEXT("OriginalBaseColorTint"),
			Index == 0 ? FLinearColor(.13f, .15f, .17f) : FLinearColor(.32f, .35f, .38f));
		Material->SetScalarParameterValue(TEXT("OriginalUVScale"), Index == 0 ? 20.0f : 3.0f);
		Structure[Index]->SetMaterial(0, Material);
		OwnedMaterials.Add(Material);
	}
}

ADarkwellPropLabFurniture* ADarkwellMovingPropLabRoom::SpawnTracked(
	const FName StableId,
	const int32 Shape,
	const FVector Dimensions,
	const FLinearColor Tint,
	const FTransform& Transform)
{
	if (StableId.IsNone() || Tracked.Contains(StableId))
	{
		UE_LOG(LogDarkwellMovingPropLab, Warning,
			TEXT("MOVING_RULES duplicate StableID rejected: %s"), *StableId.ToString());
		return nullptr;
	}
	ADarkwellPropLabFurniture* Actor = GetWorld()->SpawnActorDeferred<ADarkwellPropLabFurniture>(
		ADarkwellPropLabFurniture::StaticClass(), Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Actor)
	{
		return nullptr;
	}
	Actor->StableId = StableId;
	Actor->Shape = Shape;
	Actor->Dimensions = Dimensions;
	Actor->Tint = Tint;
	Actor->bIndividualWorktop = false;
	Actor->bSpatialHistoryManaged = true;
	Actor->Memory->bRememberFromStart = false;
	Actor->FinishSpawning(Transform);
	if (!Actor->HasActorBegunPlay())
	{
		Actor->DispatchBeginPlay();
	}
	for (UStaticMeshComponent* Part : Actor->Memory->GetMemoryPrimitives())
	{
		Part->SetCastShadow(true);
		Part->SetCastHiddenShadow(true);
	}
	Actor->Memory->ApplySourceGeometryVisibility(false);

	FTrackedProp& Prop = Tracked.Add(StableId);
	Prop.StableId = StableId;
	Prop.Actual = Actor;
	Prop.InitialTransform = Transform;
	Prop.LastPhysicalTransform = Transform;
	Prop.Dimensions = Dimensions;
	Prop.Tint = Tint;
	Prop.Shape = Shape;
	Prop.bExists = true;
	Prop.History.Initialize(StableId);
	return Actor;
}

void ADarkwellMovingPropLabRoom::DestroyVisual(FRecordVisual& Visual)
{
	if (AActor* Proxy = Visual.Proxy.Get())
	{
		Proxy->Destroy();
	}
	if (UDynamicMeshComponent* Cap = Visual.Cap.Get())
	{
		Cap->DestroyComponent();
	}
	Visual.Proxy.Reset();
	Visual.Cap.Reset();
}

void ADarkwellMovingPropLabRoom::DestroyTracked()
{
	for (TPair<FName, FTrackedProp>& Pair : Tracked)
	{
		for (TPair<uint32, FRecordVisual>& Visual : Pair.Value.Visuals)
		{
			DestroyVisual(Visual.Value);
		}
		if (ADarkwellPropLabFurniture* Actual = Pair.Value.Actual.Get())
		{
			Actual->Destroy();
		}
	}
	Tracked.Reset();
	OwnedMaterials.Reset();
	OwnedTextures.Reset();
	OwnedCaps.Reset();
	ActiveMotions.Reset();
	bMotionActive = false;
}

void ADarkwellMovingPropLabRoom::DestroyTracked(const FName StableId)
{
	FTrackedProp* Prop = Tracked.Find(StableId);
	if (!Prop)
	{
		return;
	}
	for (TPair<uint32, FRecordVisual>& Visual : Prop->Visuals)
	{
		DestroyVisual(Visual.Value);
	}
	if (ADarkwellPropLabFurniture* Actual = Prop->Actual.Get())
	{
		ActiveMotions.RemoveAll([Actual](const FActiveMotion& Motion)
		{
			return Motion.Prop.Get() == Actual;
		});
		Actual->Destroy();
	}
	Tracked.Remove(StableId);
	bMotionActive = !ActiveMotions.IsEmpty();
}

FBox2D ADarkwellMovingPropLabRoom::ActualBounds(
	const ADarkwellPropLabFurniture& Prop) const
{
	FBox Bounds(ForceInit);
	for (const UStaticMeshComponent* Part : Prop.Memory->GetMemoryPrimitives())
	{
		if (Part && Part->IsRegistered())
		{
			Bounds += Part->Bounds.GetBox();
		}
	}
	return Bounds.IsValid
		? FBox2D(FVector2D(Bounds.Min), FVector2D(Bounds.Max))
		: FBox2D(ForceInit);
}

TArray<FBox> ADarkwellMovingPropLabRoom::ActualPartBounds(
	const ADarkwellPropLabFurniture& Prop) const
{
	TArray<FBox> Result;
	for (const UStaticMeshComponent* Part : Prop.Memory->GetMemoryPrimitives())
	{
		if (Part && Part->IsRegistered())
		{
			Result.Add(Part->Bounds.GetBox());
		}
	}
	return Result;
}

TArray<float> ADarkwellMovingPropLabRoom::ConservativeCoverage(
	const FBox2D& Bounds) const
{
	TArray<float> Coverage;
	const UDarkwellFogVisualSubsystem* Fog = GetWorld()
		? GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>() : nullptr;
	if (!Fog || !Bounds.bIsValid)
	{
		return Coverage;
	}
	const FIntPoint Size(
		FMath::CeilToInt(Bounds.GetSize().X / Darkwell::MovingPropLab::CellSize),
		FMath::CeilToInt(Bounds.GetSize().Y / Darkwell::MovingPropLab::CellSize));
	if (Size.X <= 0 || Size.Y <= 0)
	{
		return Coverage;
	}
	const FVector2D Step = Bounds.GetSize() / FVector2D(Size.X, Size.Y);
	TArray<float> Corners;
	Corners.SetNumUninitialized((Size.X + 1) * (Size.Y + 1));
	Coverage.SetNumUninitialized(Size.X * Size.Y);
	for (int32 Y = 0; Y <= Size.Y; ++Y)
	{
		for (int32 X = 0; X <= Size.X; ++X)
		{
			Corners[Y * (Size.X + 1) + X] = Fog->EvaluateLiveCoverageAtWorldPoint(
				Bounds.Min + Step * FVector2D(X, Y));
		}
	}
	for (int32 Y = 0; Y < Size.Y; ++Y)
	{
		for (int32 X = 0; X < Size.X; ++X)
		{
			const int32 Corner = Y * (Size.X + 1) + X;
			float Value = FMath::Min(
				FMath::Min(Corners[Corner], Corners[Corner + 1]),
				FMath::Min(Corners[Corner + Size.X + 1], Corners[Corner + Size.X + 2]));
			Value = FMath::Min(Value, Fog->EvaluateLiveCoverageAtWorldPoint(
				Bounds.Min + Step * FVector2D(X + .5f, Y + .5f)));
			Coverage[Y * Size.X + X] = Value;
		}
	}
	return Coverage;
}

bool ADarkwellMovingPropLabRoom::IsOccupiedByActual(
	const FVector2D Point,
	const FName IgnoredStableId) const
{
	for (const TPair<FName, FTrackedProp>& Pair : Tracked)
	{
		const FTrackedProp& Prop = Pair.Value;
		if (!Prop.bExists || Pair.Key == IgnoredStableId)
		{
			continue;
		}
		const ADarkwellPropLabFurniture* Actual = Prop.Actual.Get();
		if (Actual && Actual->GetActorEnableCollision() && ActualBounds(*Actual).IsInside(Point))
		{
			return true;
		}
	}
	return false;
}

bool ADarkwellMovingPropLabRoom::HasCurrentObservedContributionAt(
	const FTrackedProp& Prop,
	const FVector2D Point) const
{
	const int32 CurrentIndex = Prop.History.GetCurrentIndex();
	const ADarkwellPropLabFurniture* Actual = Prop.bExists ? Prop.Actual.Get() : nullptr;
	if (!Actual || CurrentIndex == INDEX_NONE
		|| !Prop.History.GetRecords().IsValidIndex(CurrentIndex))
	{
		return false;
	}
	const FDarkwellSpatialObservationRecord& Current = Prop.History.GetRecords()[CurrentIndex];
	const FBox2D& Bounds = Current.SpatialMemory.GetBounds();
	const FIntPoint Size = Current.SpatialMemory.GetSize();
	if (!Bounds.bIsValid || Size.X <= 0 || Size.Y <= 0 || !Bounds.IsInside(Point))
	{
		return false;
	}
	bool bInsideSourceFootprint = false;
	for (const UStaticMeshComponent* Part : Actual->Memory->GetMemoryPrimitives())
	{
		if (!Part || !Part->IsRegistered())
		{
			continue;
		}
		const FBox PartBounds = Part->Bounds.GetBox();
		bInsideSourceFootprint |= Point.X >= PartBounds.Min.X && Point.X <= PartBounds.Max.X
			&& Point.Y >= PartBounds.Min.Y && Point.Y <= PartBounds.Max.Y;
	}
	if (!bInsideSourceFootprint)
	{
		return false;
	}
	const FVector2D Relative = (Point - Bounds.Min) / Bounds.GetSize();
	const int32 X = FMath::Clamp(FMath::FloorToInt(Relative.X * Size.X), 0, Size.X - 1);
	const int32 Y = FMath::Clamp(FMath::FloorToInt(Relative.Y * Size.Y), 0, Size.Y - 1);
	const FDarkwellSpatialPropMemory::FCell& Cell =
		Current.SpatialMemory.GetCells()[Y * Size.X + X];
	// DiscoveredPresent is latched only by legal SightWeave evidence. Once the
	// current pose owns this sample, an older pose cannot become a second visible
	// contributor when the player turns away.
	return Cell.DiscoveredPresent > 0.0f && Cell.AppearanceBlend > 0.0f;
}

void ADarkwellMovingPropLabRoom::UpdateHistoricalContributionExclusion(
	FTrackedProp& Prop,
	FDarkwellSpatialObservationRecord& Record)
{
	if (Record.bCurrentObservedLocation)
	{
		return;
	}
	FRecordVisual* Visual = Prop.Visuals.Find(Record.Epoch);
	if (!Visual)
	{
		return;
	}
	const FIntPoint Size = Record.SpatialMemory.GetSize()
		* Darkwell::MovingPropLab::PresentationSamples;
	if (Size.X <= 0 || Size.Y <= 0)
	{
		return;
	}
	if (Visual->SuppressedByCurrentEvidence.Num() != Size.X * Size.Y)
	{
		Visual->SuppressedByCurrentEvidence.Init(false, Size.X * Size.Y);
	}
	const FBox2D& Bounds = Record.SpatialMemory.GetBounds();
	const FVector2D Step = Bounds.GetSize() / FVector2D(Size.X, Size.Y);
	for (int32 Y = 0; Y < Size.Y; ++Y)
	{
		for (int32 X = 0; X < Size.X; ++X)
		{
			const int32 Index = Y * Size.X + X;
			if (!Visual->SuppressedByCurrentEvidence[Index]
				&& HasCurrentObservedContributionAt(
					Prop, Bounds.Min + Step * FVector2D(X + 0.5f, Y + 0.5f)))
			{
				// This is a monotonic presentation-ownership decision backed by new
				// legal present evidence. It does not mark the old cell verified empty
				// and does not alter D/V/R authority.
				Visual->SuppressedByCurrentEvidence[Index] = true;
			}
		}
	}
}

int32 ADarkwellMovingPropLabRoom::ComputeMaxOverlapContributors(
	const FTrackedProp& Prop) const
{
	int32 Maximum = 0;
	const int32 CurrentIndex = Prop.History.GetCurrentIndex();
	if (CurrentIndex != INDEX_NONE && Prop.History.GetRecords().IsValidIndex(CurrentIndex))
	{
		const FDarkwellSpatialObservationRecord& Current = Prop.History.GetRecords()[CurrentIndex];
		Maximum = Current.SpatialMemory.GetCells().ContainsByPredicate(
			[](const FDarkwellSpatialPropMemory::FCell& Cell)
			{
				return Cell.DiscoveredPresent > 0.0f && Cell.AppearanceBlend > 0.0f;
			}) ? 1 : 0;
	}
	for (const FDarkwellSpatialObservationRecord& SampleRecord : Prop.History.GetRecords())
	{
		if (SampleRecord.bCurrentObservedLocation)
		{
			continue;
		}
		const FIntPoint SampleSize = SampleRecord.SpatialMemory.GetSize()
			* Darkwell::MovingPropLab::PresentationSamples;
		const FBox2D& SampleBounds = SampleRecord.SpatialMemory.GetBounds();
		if (SampleSize.X <= 0 || SampleSize.Y <= 0 || !SampleBounds.bIsValid)
		{
			continue;
		}
		const FVector2D Step = SampleBounds.GetSize() / FVector2D(SampleSize.X, SampleSize.Y);
		for (int32 Y = 0; Y < SampleSize.Y; ++Y)
		{
			for (int32 X = 0; X < SampleSize.X; ++X)
			{
				const FVector2D Point = SampleBounds.Min
					+ Step * FVector2D(X + 0.5f, Y + 0.5f);
				int32 Contributors = HasCurrentObservedContributionAt(Prop, Point) ? 1 : 0;
				for (const FDarkwellSpatialObservationRecord& Historical : Prop.History.GetRecords())
				{
					if (Historical.bCurrentObservedLocation
						|| !Historical.SpatialMemory.GetBounds().IsInside(Point))
					{
						continue;
					}
					const FRecordVisual* Visual = Prop.Visuals.Find(Historical.Epoch);
					const FIntPoint Coarse = Historical.SpatialMemory.GetSize();
					const FIntPoint Fine = Coarse * Darkwell::MovingPropLab::PresentationSamples;
					if (!Visual || Fine.X <= 0 || Fine.Y <= 0)
					{
						continue;
					}
					const FVector2D Relative = (Point - Historical.SpatialMemory.GetBounds().Min)
						/ Historical.SpatialMemory.GetBounds().GetSize();
					const int32 FineX = FMath::Clamp(FMath::FloorToInt(Relative.X * Fine.X), 0, Fine.X - 1);
					const int32 FineY = FMath::Clamp(FMath::FloorToInt(Relative.Y * Fine.Y), 0, Fine.Y - 1);
					const int32 FineIndex = FineY * Fine.X + FineX;
					const int32 CellX = FineX / Darkwell::MovingPropLab::PresentationSamples;
					const int32 CellY = FineY / Darkwell::MovingPropLab::PresentationSamples;
					const bool bSuppressed = Visual->SuppressedByCurrentEvidence.IsValidIndex(FineIndex)
						&& Visual->SuppressedByCurrentEvidence[FineIndex];
					if (!bSuppressed
						&& Historical.SpatialMemory.Presentation(CellY * Coarse.X + CellX).B > 0.0f)
					{
						++Contributors;
					}
				}
				Maximum = FMath::Max(Maximum, Contributors);
			}
		}
	}
	return Maximum;
}

void ADarkwellMovingPropLabRoom::LogRotationFrame(const FTrackedProp& Prop) const
{
	if (!bInWorldControls || Scenario != 2 || Prop.StableId != Darkwell::MovingPropLab::RotateId)
	{
		return;
	}
	uint32 CurrentEpoch = 0;
	int32 StaleEpochs = 0;
	int32 VisibleProxies = 0;
	int32 Discovered = 0;
	int32 Verified = 0;
	int32 Residual = 0;
	TArray<FString> Historical;
	for (const FDarkwellSpatialObservationRecord& Record : Prop.History.GetRecords())
	{
		if (Record.bCurrentObservedLocation)
		{
			CurrentEpoch = Record.Epoch;
		}
		else
		{
			++StaleEpochs;
			const FRecordVisual* Visual = Prop.Visuals.Find(Record.Epoch);
			const bool bVisible = Visual && Visual->Proxy.IsValid() && !Visual->Proxy->IsHidden();
			VisibleProxies += bVisible ? 1 : 0;
			Historical.Add(FString::Printf(TEXT("%u@%.2f/proxy=%d/visible=%d"),
				Record.Epoch, Record.SnapshotTransform.Rotator().Yaw,
				Visual && Visual->Proxy.IsValid() ? Visual->Proxy->GetUniqueID() : 0,
				bVisible ? 1 : 0));
		}
		for (const FDarkwellSpatialPropMemory::FCell& Cell : Record.SpatialMemory.GetCells())
		{
			Discovered += Cell.DiscoveredPresent > 0.0f ? 1 : 0;
			Verified += Cell.VerifiedEmpty > 0.0f ? 1 : 0;
			Residual += Cell.RemainingStale > 0.0f ? 1 : 0;
		}
	}
	int32 ActualComponents = 0;
	int32 VisibleActualComponents = 0;
	if (const ADarkwellPropLabFurniture* Actual = Prop.Actual.Get())
	{
		ActualComponents = Actual->Memory->GetMemoryPrimitives().Num();
		for (const UStaticMeshComponent* Part : Actual->Memory->GetMemoryPrimitives())
		{
			VisibleActualComponents += Part && Part->IsVisible() ? 1 : 0;
		}
	}
	UE_LOG(LogDarkwellMovingPropLab, Display,
		TEXT("ROTATION_FRAME live_epoch=%u stale_epochs=%d actual=%d/%d actual_yaw=%.2f coverage=%.4f proxies=%d stale=[%s] D=%d V=%d R=%d overlap_contributors=%d"),
		CurrentEpoch, StaleEpochs, VisibleActualComponents, ActualComponents,
		Prop.Actual.IsValid() ? Prop.Actual->GetActorRotation().Yaw : 0.0f,
		Prop.LastLegalCoverageRatio, VisibleProxies, *FString::Join(Historical, TEXT(";")),
		Discovered, Verified, Residual, Prop.MaxOverlapContributors);
}

void ADarkwellMovingPropLabRoom::UpdateTracked(
	FTrackedProp& Prop,
	const float DeltaSeconds)
{
	ADarkwellPropLabFurniture* Actual = Prop.bExists ? Prop.Actual.Get() : nullptr;
	if (Actual)
	{
		const FTransform Transform = Actual->GetActorTransform();
		const FBox2D Bounds = ActualBounds(*Actual);
		TArray<float> Coverage = ConservativeCoverage(Bounds);
		const bool bAnyLegal = Coverage.ContainsByPredicate([](const float Value)
		{
			return Value >= FDarkwellSpatialPropMemory::LegalCoverage;
		});
		int32 LegalCells = 0;
		for (const float Value : Coverage)
		{
			LegalCells += Value >= FDarkwellSpatialPropMemory::LegalCoverage ? 1 : 0;
		}
		Prop.LastLegalCoverageRatio = Coverage.IsEmpty()
			? 0.0f : static_cast<float>(LegalCells) / Coverage.Num();
		const int32 CurrentIndex = Prop.History.GetCurrentIndex();
		if (CurrentIndex != INDEX_NONE
			&& !Darkwell::MovingPropLab::TransformsMatch(Prop.LastPhysicalTransform, Transform))
		{
			if (bAnyLegal)
			{
				Prop.History.RebaseCurrentObservedLocation(
					Transform, Bounds, Darkwell::MovingPropLab::CellSize);
				FDarkwellSpatialObservationRecord& Current =
					Prop.History.GetMutableRecords()[Prop.History.GetCurrentIndex()];
				FRecordVisual& Visual = Prop.Visuals.FindOrAdd(Current.Epoch);
				Visual.PartBounds = ActualPartBounds(*Actual);
			}
			else
			{
				// Fallback for externally driven hidden motion. In-world scenarios seal
				// the epoch before StartMotion, so they never enter through this late path.
				FreezeCurrentForHiddenMotion(Prop, TEXT("UNANNOUNCED_HIDDEN_TRANSFORM"));
			}
		}
		if (Prop.History.GetCurrentIndex() == INDEX_NONE && bAnyLegal)
		{
			Prop.History.BeginObservedLocation(
				Transform, Bounds, Darkwell::MovingPropLab::CellSize);
		}
		if (Prop.History.GetCurrentIndex() != INDEX_NONE)
		{
			FDarkwellSpatialObservationRecord& Current =
				Prop.History.GetMutableRecords()[Prop.History.GetCurrentIndex()];
			if (Coverage.Num() == Current.SpatialMemory.GetCells().Num())
			{
				Prop.History.AdvanceCurrent(DeltaSeconds, Coverage);
			}
			EnsureRecordVisual(Prop, Current);
			UpdateRecordTexture(Prop, Current);
			UpdateRecordCap(Prop, Current);
			Actual->BindSpatialState(
				Prop.Visuals.FindChecked(Current.Epoch).Texture.Get(),
				Current.SpatialMemory.GetBounds());
			Actual->Memory->ApplySourceGeometryVisibility(true);
		}
		else
		{
			Actual->Memory->ApplySourceGeometryVisibility(false);
		}
		Prop.LastPhysicalTransform = Transform;
	}

	TArray<uint32> HistoricalEpochs;
	for (FDarkwellSpatialObservationRecord& Record : Prop.History.GetMutableRecords())
	{
		if (!Record.bCurrentObservedLocation)
		{
			HistoricalEpochs.Add(Record.Epoch);
		}
	}
	for (const uint32 Epoch : HistoricalEpochs)
	{
		FDarkwellSpatialObservationRecord* Record = Prop.History.FindRecord(Epoch);
		if (!Record)
		{
			continue;
		}
		TArray<float> Coverage = ConservativeCoverage(Record->SpatialMemory.GetBounds());
		const FIntPoint Size = Record->SpatialMemory.GetSize();
		const FBox2D& Bounds = Record->SpatialMemory.GetBounds();
		const FVector2D Step = Bounds.GetSize() / FVector2D(Size.X, Size.Y);
		for (int32 Y = 0; Y < Size.Y; ++Y)
		{
			for (int32 X = 0; X < Size.X; ++X)
			{
				if (IsOccupiedByActual(
					Bounds.Min + Step * FVector2D(X + .5f, Y + .5f), NAME_None))
				{
					Coverage[Y * Size.X + X] = 0.0f;
				}
			}
		}
		Prop.History.AdvanceHistorical(Epoch, DeltaSeconds, Coverage);
		EnsureRecordVisual(Prop, *Record);
		UpdateHistoricalContributionExclusion(Prop, *Record);
		UpdateRecordTexture(Prop, *Record);
		UpdateRecordCap(Prop, *Record);
	}
	Prop.MaxOverlapContributors = ComputeMaxOverlapContributors(Prop);
	LogRotationFrame(Prop);

	TArray<uint32> Erased;
	for (const FDarkwellSpatialObservationRecord& Record : Prop.History.GetRecords())
	{
		if (Record.bCurrentObservedLocation || !Record.SpatialMemory.IsAbsent())
		{
			continue;
		}
		const bool bAny = Record.SpatialMemory.GetCells().ContainsByPredicate(
			[](const FDarkwellSpatialPropMemory::FCell& Cell)
			{
				return Cell.RemainingStale > 0.0f || Cell.StaleOpacity > 0.0f;
			});
		if (!bAny)
		{
			Erased.Add(Record.Epoch);
		}
	}
	for (const uint32 Epoch : Erased)
	{
		if (FRecordVisual* Visual = Prop.Visuals.Find(Epoch))
		{
			DestroyVisual(*Visual);
			Prop.Visuals.Remove(Epoch);
		}
	}
	Prop.History.ReleaseFullyErasedRecords();
}

bool ADarkwellMovingPropLabRoom::FreezeCurrentForHiddenMotion(
	FTrackedProp& Prop,
	const TCHAR* Reason)
{
	const int32 CurrentIndex = Prop.History.GetCurrentIndex();
	if (CurrentIndex == INDEX_NONE)
	{
		return false;
	}
	FDarkwellSpatialObservationRecord& Current =
		Prop.History.GetMutableRecords()[CurrentIndex];
	const uint32 Epoch = Current.Epoch;
	EnsureRecordVisual(Prop, Current);
	UpdateRecordTexture(Prop, Current);
	UpdateRecordCap(Prop, Current);

	// Hide the original source before making the stale proxy renderable. Both
	// state changes happen on the game thread before StartMotion advances the
	// actor, so the overlapping start pose can never draw two coplanar layers.
	if (ADarkwellPropLabFurniture* Actual = Prop.Actual.Get())
	{
		Actual->Memory->ApplySourceGeometryVisibility(false);
	}
	if (!Prop.History.FreezeCurrentForHiddenMovement())
	{
		return false;
	}
	++Prop.HiddenFreezeCount;
	if (FDarkwellSpatialObservationRecord* Historical = Prop.History.FindRecord(Epoch))
	{
		EnsureRecordVisual(Prop, *Historical);
		UpdateRecordTexture(Prop, *Historical);
		UpdateRecordCap(Prop, *Historical);
	}
	const FRecordVisual* Visual = Prop.Visuals.Find(Epoch);
	UE_LOG(LogDarkwellMovingPropLab, Display,
		TEXT("MOVING_RULES_STALE_SEALED id=%s epoch=%u reason=%s freezes=%d proxy=%d texture=%dx%d uploads=%d"),
		*Prop.StableId.ToString(), Epoch, Reason, Prop.HiddenFreezeCount,
		Visual && Visual->Proxy.IsValid() ? Visual->Proxy->GetUniqueID() : 0,
		Visual && Visual->Texture.IsValid() ? Visual->Texture->GetSizeX() : 0,
		Visual && Visual->Texture.IsValid() ? Visual->Texture->GetSizeY() : 0,
		Visual ? Visual->TextureUploadCount : 0);
	return true;
}

bool ADarkwellMovingPropLabRoom::SetTrackedExists(
	const FName StableId,
	const bool bExists)
{
	FTrackedProp* Prop = Tracked.Find(StableId);
	ADarkwellPropLabFurniture* Actual = Prop ? Prop->Actual.Get() : nullptr;
	if (!Prop || !Actual || Prop->bExists == bExists)
	{
		return false;
	}
	if (!bExists)
	{
		FreezeCurrentForHiddenMotion(*Prop, TEXT("ACTUAL_BECAME_ABSENT"));
		Actual->Memory->ApplySourceGeometryVisibility(false);
		Actual->SetActorEnableCollision(false);
		Actual->SetActorHiddenInGame(true);
		Prop->bExists = false;
	}
	else
	{
		Prop->bExists = true;
		Actual->SetActorHiddenInGame(false);
		Actual->SetActorEnableCollision(true);
		Actual->Memory->ApplySourceGeometryVisibility(false);
		Prop->LastPhysicalTransform = Actual->GetActorTransform();
	}
	UE_LOG(LogDarkwellMovingPropLab, Display,
		TEXT("MOVING_RULES_ACTUAL id=%s state=%s records=%d"),
		*StableId.ToString(), bExists ? TEXT("PRESENT") : TEXT("ABSENT"),
		Prop->History.GetRecords().Num());
	return true;
}

void ADarkwellMovingPropLabRoom::EnsureRecordVisual(
	FTrackedProp& Prop,
	FDarkwellSpatialObservationRecord& Record)
{
	FRecordVisual& Visual = Prop.Visuals.FindOrAdd(Record.Epoch);
	Visual.Epoch = Record.Epoch;
	const FIntPoint Size = Record.SpatialMemory.GetSize()
		* Darkwell::MovingPropLab::PresentationSamples;
	if (!Record.bCurrentObservedLocation)
	{
		if (Visual.HistoricalTextureSize == FIntPoint::ZeroValue)
		{
			Visual.HistoricalTextureSize = Size;
		}
		else if (Visual.HistoricalTextureSize != Size)
		{
			UE_LOG(LogDarkwellMovingPropLab, Error,
				TEXT("MOVING_RULES_STALE_SIZE_CHANGED id=%s epoch=%u locked=%dx%d requested=%dx%d"),
				*Prop.StableId.ToString(), Record.Epoch,
				Visual.HistoricalTextureSize.X, Visual.HistoricalTextureSize.Y, Size.X, Size.Y);
		}
		if (Visual.SuppressedByCurrentEvidence.Num() != Size.X * Size.Y)
		{
			Visual.SuppressedByCurrentEvidence.Init(false, Size.X * Size.Y);
		}
	}
	const bool bTextureSizeChanged = Visual.Texture.IsValid()
		&& (Visual.Texture->GetSizeX() != Size.X || Visual.Texture->GetSizeY() != Size.Y);
	if (!Visual.Texture.IsValid() || bTextureSizeChanged)
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(Size.X, Size.Y, PF_FloatRGBA);
		Texture->SRGB = false;
		Texture->Filter = TF_Bilinear;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->NeverStream = true;
		auto& Bulk = Texture->GetPlatformData()->Mips[0].BulkData;
		FMemory::Memzero(Bulk.Lock(LOCK_READ_WRITE), Bulk.GetBulkDataSize());
		Bulk.Unlock();
		Texture->UpdateResource();
		Visual.Texture = Texture;
		++Visual.TextureCreationCount;
		Visual.TextureSignature = 0;
		OwnedTextures.Add(Texture);
		if (bTextureSizeChanged)
		{
			UE_LOG(LogDarkwellMovingPropLab, Verbose,
				TEXT("MOVING_RULES_TEXTURE_RESIZED id=%s epoch=%u size=%dx%d"),
				*Prop.StableId.ToString(), Record.Epoch, Size.X, Size.Y);
		}
	}
	if (Visual.PartBounds.IsEmpty())
	{
		if (const ADarkwellPropLabFurniture* Actual = Prop.Actual.Get())
		{
			Visual.PartBounds = ActualPartBounds(*Actual);
		}
	}
	if (!Visual.Cap.IsValid())
	{
		UDynamicMeshComponent* Cap = NewObject<UDynamicMeshComponent>(
			this, *FString::Printf(TEXT("MovingCap_%s_%u"), *Prop.StableId.ToString(), Record.Epoch));
		Cap->SetupAttachment(GetRootComponent());
		Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cap->SetGenerateOverlapEvents(false);
		Cap->SetCastShadow(false);
		Cap->SetReceivesDecals(false);
		Cap->SetVisibility(false);
		Cap->RegisterComponent();
		Cap->SetMaterial(0, LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/Darkwell/Vision/PropLab/M_ManualStaleCutCap.M_ManualStaleCutCap")));
		Visual.Cap = Cap;
		OwnedCaps.Add(Cap);
	}
	if (!Record.bCurrentObservedLocation && !Visual.Proxy.IsValid())
	{
		if (AActor* Proxy = SpawnMemoryProxy(Prop, Record))
		{
			Visual.Proxy = Proxy;
			++Visual.ProxyCreationCount;
			BindProxyMaterial(Prop, Record, Proxy);
		}
	}
	if (!Record.bCurrentObservedLocation && Visual.Proxy.IsValid())
	{
		const bool bVisible = !Visual.Proxy->IsHidden();
		if (Visual.bHasProxyVisibilitySample && Visual.bLastProxyVisible != bVisible)
		{
			++Visual.ProxyVisibilityTransitions;
			UE_LOG(LogDarkwellMovingPropLab, Warning,
				TEXT("MOVING_RULES_STALE_VISIBILITY_CHANGED id=%s epoch=%u visible=%d transitions=%d"),
				*Prop.StableId.ToString(), Record.Epoch, bVisible,
				Visual.ProxyVisibilityTransitions);
		}
		Visual.bHasProxyVisibilitySample = true;
		Visual.bLastProxyVisible = bVisible;
	}
}

void ADarkwellMovingPropLabRoom::UpdateRecordTexture(
	FTrackedProp& Prop,
	FDarkwellSpatialObservationRecord& Record)
{
	FRecordVisual* Visual = Prop.Visuals.Find(Record.Epoch);
	if (!Visual || !Visual->Texture.IsValid())
	{
		return;
	}
	TArray<FLinearColor> Presentation;
	const FIntPoint Size = Record.SpatialMemory.BuildConservativePresentation(
		Darkwell::MovingPropLab::PresentationSamples, Presentation);
	if (Size.X <= 0 || Presentation.Num() != Size.X * Size.Y)
	{
		return;
	}
	if (!Record.bCurrentObservedLocation
		&& Visual->SuppressedByCurrentEvidence.Num() == Presentation.Num())
	{
		for (int32 Index = 0; Index < Presentation.Num(); ++Index)
		{
			if (Visual->SuppressedByCurrentEvidence[Index])
			{
				// Current legally discovered geometry owns this world sample. The
				// historical proxy remains a valid spatial record but contributes no
				// second surface at the same sample.
				Presentation[Index].R = 0.0f;
				Presentation[Index].G = 0.0f;
				Presentation[Index].B = 0.0f;
			}
		}
	}
	uint64 Signature = 1469598103934665603ull;
	auto MixFloat = [&Signature](const float Value)
	{
		uint32 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		Signature = (Signature ^ Bits) * 1099511628211ull;
	};
	for (const FLinearColor& Pixel : Presentation)
	{
		MixFloat(Pixel.R);
		MixFloat(Pixel.G);
		MixFloat(Pixel.B);
		MixFloat(Pixel.A);
	}
	if (Visual->TextureSignature == Signature)
	{
		return;
	}
	Visual->TextureSignature = Signature;
	++Visual->TextureUploadCount;
	FFloat16Color* Pixels = new FFloat16Color[Presentation.Num()];
	for (int32 Index = 0; Index < Presentation.Num(); ++Index)
	{
		Pixels[Index] = FFloat16Color(Presentation[Index]);
	}
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Size.X, Size.Y);
	Visual->Texture->UpdateTextureRegions(
		0, 1, Region, Size.X * sizeof(FFloat16Color), sizeof(FFloat16Color),
		reinterpret_cast<uint8*>(Pixels),
		[](uint8* Data, const FUpdateTextureRegion2D* UpdatedRegion)
		{
			delete[] reinterpret_cast<FFloat16Color*>(Data);
			delete UpdatedRegion;
		});
}

AActor* ADarkwellMovingPropLabRoom::SpawnMemoryProxy(
	const FTrackedProp& Prop,
	const FDarkwellSpatialObservationRecord& Record)
{
	const ADarkwellPropLabFurniture* Actual = Prop.Actual.Get();
	if (!Actual)
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.Name = MakeUniqueObjectName(
		GetWorld(), AActor::StaticClass(),
		*FString::Printf(TEXT("SpatialMemory_%s_Epoch%u"), *Prop.StableId.ToString(), Record.Epoch));
	Parameters.ObjectFlags |= RF_Transient;
	AActor* Proxy = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Parameters);
	if (!Proxy)
	{
		return nullptr;
	}
	Proxy->SetActorEnableCollision(false);
	USceneComponent* Root = NewObject<USceneComponent>(Proxy, TEXT("SpatialMemoryRoot"));
	Proxy->SetRootComponent(Root);
	Root->RegisterComponent();
	int32 Index = 0;
	for (const UStaticMeshComponent* Source : Actual->Memory->GetMemoryPrimitives())
	{
		if (!Source || !Source->GetStaticMesh())
		{
			continue;
		}
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(
			Proxy, *FString::Printf(TEXT("SpatialMemoryMesh_%d"), Index++));
		Mesh->SetupAttachment(Root);
		Mesh->SetStaticMesh(Source->GetStaticMesh());
		Mesh->SetWorldTransform(Source->GetRelativeTransform() * Record.SnapshotTransform);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetCastShadow(false);
		Mesh->SetAffectDynamicIndirectLighting(false);
		Mesh->SetAffectDistanceFieldLighting(false);
		Mesh->SetVisibleInRayTracing(false);
		Mesh->SetRenderCustomDepth(false);
		Mesh->SetReceivesDecals(false);
		Mesh->RegisterComponent();
	}
	return Proxy;
}

void ADarkwellMovingPropLabRoom::BindProxyMaterial(
	FTrackedProp& Prop,
	FDarkwellSpatialObservationRecord& Record,
	AActor* Proxy)
{
	FRecordVisual* Visual = Prop.Visuals.Find(Record.Epoch);
	if (!Visual || !Visual->Texture.IsValid() || !Proxy)
	{
		return;
	}
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Darkwell/Vision/PropLab/M_ManualAccumulatedMemory.M_ManualAccumulatedMemory"));
	const FBox2D& Bounds = Record.SpatialMemory.GetBounds();
	const FVector2D Inv = FVector2D(1, 1) / Bounds.GetSize();
	TInlineComponentArray<UStaticMeshComponent*> Meshes(Proxy);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, this);
		Material->SetTextureParameterValue(TEXT("SpatialStateTexture"), Visual->Texture.Get());
		Material->SetVectorParameterValue(TEXT("SpatialMinInv"),
			FLinearColor(Bounds.Min.X, Bounds.Min.Y, Inv.X, Inv.Y));
		Material->SetVectorParameterValue(TEXT("OriginalBaseColorTint"), Prop.Tint);
		Material->SetScalarParameterValue(TEXT("SpatialReady"), 1.0f);
		Mesh->SetMaterial(0, Material);
		OwnedMaterials.Add(Material);
	}
}

void ADarkwellMovingPropLabRoom::UpdateRecordCap(
	FTrackedProp& Prop,
	FDarkwellSpatialObservationRecord& Record)
{
	using namespace UE::Geometry;
	FRecordVisual* Visual = Prop.Visuals.Find(Record.Epoch);
	if (!Visual || !Visual->Cap.IsValid())
	{
		return;
	}
	const bool bPresent = Record.SpatialMemory.IsPresent();
	const bool bAbsent = Record.SpatialMemory.IsAbsent();
	const TConstArrayView<FDarkwellSpatialPropMemory::FCell> Cells = Record.SpatialMemory.GetCells();
	const FIntPoint Size = Record.SpatialMemory.GetSize();
	if ((!bPresent && !bAbsent) || Cells.IsEmpty() || Visual->PartBounds.IsEmpty())
	{
		Visual->Cap->SetMesh(FDynamicMesh3());
		Visual->Cap->SetVisibility(false);
		Visual->CapTriangles = 0;
		return;
	}
	auto IsSuppressedByCurrent = [&](const int32 X, const int32 Y)
	{
		if (Record.bCurrentObservedLocation || X < 0 || Y < 0 || X >= Size.X || Y >= Size.Y)
		{
			return false;
		}
		const int32 Samples = Darkwell::MovingPropLab::PresentationSamples;
		const FIntPoint FineSize = Size * Samples;
		if (Visual->SuppressedByCurrentEvidence.Num() != FineSize.X * FineSize.Y)
		{
			return false;
		}
		for (int32 SampleY = 0; SampleY < Samples; ++SampleY)
		{
			for (int32 SampleX = 0; SampleX < Samples; ++SampleX)
			{
				const int32 FineIndex = (Y * Samples + SampleY) * FineSize.X
					+ X * Samples + SampleX;
				if (!Visual->SuppressedByCurrentEvidence[FineIndex])
				{
					return false;
				}
			}
		}
		return true;
	};
	uint64 Signature = (uint64(Record.SpatialMemory.GetGeneration()) << 1 | uint64(bPresent))
		* 1099511628211ull;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FDarkwellSpatialPropMemory::FCell& Cell = Cells[Index];
		const uint64 Bits = bPresent
			? (Cell.DiscoveredPresent > 0 ? 1ull : 0ull)
			: ((Cell.InitialRemembered > 0 ? 1ull : 0ull)
				| (Cell.VerifiedEmpty > 0 ? 2ull : 0ull)
				| (IsSuppressedByCurrent(Index % Size.X, Index / Size.X) ? 4ull : 0ull));
		Signature = (Signature ^ Bits) * 1099511628211ull;
	}
	if (Signature == Visual->CapSignature)
	{
		return;
	}
	Visual->CapSignature = Signature;
	FDynamicMesh3 Mesh;
	const FBox2D& Bounds = Record.SpatialMemory.GetBounds();
	const FVector2D Step = Bounds.GetSize() / FVector2D(Size.X, Size.Y);
	const FVector Origin = GetActorLocation();
	auto IsSubmitted = [&](const int32 X, const int32 Y)
	{
		if (X < 0 || Y < 0 || X >= Size.X || Y >= Size.Y) return false;
		const auto& Cell = Cells[Y * Size.X + X];
		return bPresent ? Cell.DiscoveredPresent > 0
			: Cell.InitialRemembered > 0 && Cell.VerifiedEmpty == 0
				&& !IsSuppressedByCurrent(X, Y);
	};
	auto IsCut = [&](const int32 X, const int32 Y)
	{
		if (X < 0 || Y < 0 || X >= Size.X || Y >= Size.Y) return false;
		const auto& Cell = Cells[Y * Size.X + X];
		return bPresent ? Cell.DiscoveredPresent == 0
			: Cell.InitialRemembered > 0
				&& (Cell.VerifiedEmpty > 0 || IsSuppressedByCurrent(X, Y));
	};
	auto AddQuad = [&](const FVector& A, const FVector& B, const FVector& C, const FVector& D)
	{
		const int32 IA = Mesh.AppendVertex(FVector3d(A - Origin));
		const int32 IB = Mesh.AppendVertex(FVector3d(B - Origin));
		const int32 IC = Mesh.AppendVertex(FVector3d(C - Origin));
		const int32 ID = Mesh.AppendVertex(FVector3d(D - Origin));
		Mesh.AppendTriangle(IA, IB, IC);
		Mesh.AppendTriangle(IA, IC, ID);
	};
	auto Vertical = [&](const double X, const double Y0, const double Y1)
	{
		for (const FBox& Part : Visual->PartBounds)
		{
			if (X < Part.Min.X - UE_KINDA_SMALL_NUMBER || X > Part.Max.X + UE_KINDA_SMALL_NUMBER) continue;
			const double From = FMath::Max(Y0, Part.Min.Y);
			const double To = FMath::Min(Y1, Part.Max.Y);
			if (To - From > UE_KINDA_SMALL_NUMBER)
			{
				AddQuad(FVector(X, From, Part.Min.Z), FVector(X, To, Part.Min.Z),
					FVector(X, To, Part.Max.Z), FVector(X, From, Part.Max.Z));
			}
		}
	};
	auto Horizontal = [&](const double Y, const double X0, const double X1)
	{
		for (const FBox& Part : Visual->PartBounds)
		{
			if (Y < Part.Min.Y - UE_KINDA_SMALL_NUMBER || Y > Part.Max.Y + UE_KINDA_SMALL_NUMBER) continue;
			const double From = FMath::Max(X0, Part.Min.X);
			const double To = FMath::Min(X1, Part.Max.X);
			if (To - From > UE_KINDA_SMALL_NUMBER)
			{
				AddQuad(FVector(From, Y, Part.Min.Z), FVector(To, Y, Part.Min.Z),
					FVector(To, Y, Part.Max.Z), FVector(From, Y, Part.Max.Z));
			}
		}
	};
	for (int32 Y = 0; Y < Size.Y; ++Y)
	{
		for (int32 X = 0; X < Size.X; ++X)
		{
			if (!IsSubmitted(X, Y)) continue;
			const double X0 = Bounds.Min.X + X * Step.X;
			const double X1 = X0 + Step.X;
			const double Y0 = Bounds.Min.Y + Y * Step.Y;
			const double Y1 = Y0 + Step.Y;
			if (IsCut(X - 1, Y)) Vertical(X0, Y0, Y1);
			if (IsCut(X + 1, Y)) Vertical(X1, Y0, Y1);
			if (IsCut(X, Y - 1)) Horizontal(Y0, X0, X1);
			if (IsCut(X, Y + 1)) Horizontal(Y1, X0, X1);
		}
	}
	Visual->CapTriangles = Mesh.TriangleCount();
	Visual->Cap->SetMesh(MoveTemp(Mesh));
	Visual->Cap->SetVisibility(Visual->CapTriangles > 0);
}

void ADarkwellMovingPropLabRoom::TeleportPlayer(
	ADarkwellCharacter* Player,
	const FVector Location,
	const float Yaw) const
{
	if (!Player) return;
	Player->GetCharacterMovement()->StopMovementImmediately();
	Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Player->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	Player->SetActorRotation(FRotator(0, Yaw, 0));
	if (USpringArmComponent* Boom = Player->FindComponentByClass<USpringArmComponent>())
	{
		Boom->SetRelativeRotation(FRotator(-65, 90, 0));
		Boom->TargetArmLength = 1250;
		Boom->TargetOffset = FVector::ZeroVector;
	}
}

void ADarkwellMovingPropLabRoom::ConfigureInWorldProps()
{
	DestroyTracked();
	CompletedInWorldControls.Reset();
	SpawnTracked(Darkwell::MovingPropLab::MainId, 0, FVector(160, 80, 150),
		FLinearColor(.18f, .43f, .62f), FTransform(Darkwell::MovingPropLab::TranslateA));
	SpawnTracked(Darkwell::MovingPropLab::RotateId, 0, FVector(150, 75, 145),
		FLinearColor(.52f, .26f, .20f), FTransform(Darkwell::MovingPropLab::RotateA));
	SpawnTracked(Darkwell::MovingPropLab::HiddenId, 0, FVector(160, 80, 150),
		FLinearColor(.24f, .52f, .30f), FTransform(Darkwell::MovingPropLab::HiddenA));
	SpawnTracked(Darkwell::MovingPropLab::EdgeId, 0, FVector(150, 75, 145),
		FLinearColor(.52f, .44f, .18f), FTransform(Darkwell::MovingPropLab::EdgeA));
	SpawnTracked(Darkwell::MovingPropLab::AbcId, 0, FVector(145, 70, 140),
		FLinearColor(.46f, .24f, .56f), FTransform(Darkwell::MovingPropLab::AbcA));
	SpawnTracked(Darkwell::MovingPropLab::MultiHighId, 0, FVector(100, 70, 180),
		FLinearColor(.16f, .40f, .68f), FTransform(FVector(-1200, -650, 0)));
	SpawnTracked(Darkwell::MovingPropLab::MultiLowId, 0, FVector(130, 85, 80),
		FLinearColor(.66f, .34f, .16f), FTransform(FVector(-650, -650, 0)));
	SpawnTracked(Darkwell::MovingPropLab::MultiTableId, 6, FVector(240, 90, 90),
		FLinearColor(.26f, .58f, .34f), FTransform(FVector(0, -650, 0)));
	SpawnTracked(Darkwell::MovingPropLab::MultiBoxId, 8, FVector(65, 65, 65),
		FLinearColor(.62f, .22f, .32f), FTransform(FVector(700, -650, 0)));
	Scenario = 0;
	ScenarioPhase = 0;
	MultiCount = 4;
	ResetInWorldControlState();
}

void ADarkwellMovingPropLabRoom::SpawnInWorldControls()
{
	DestroyInWorldControls();
	struct FControlDefinition
	{
		EDarkwellMovingPropLabControlKind Kind;
		FVector Location;
		const TCHAR* Label;
	};
	const FControlDefinition Definitions[] = {
		{EDarkwellMovingPropLabControlKind::VisibleTranslate, FVector(-1100, 260, 35), TEXT("F  VISIBLE TRANSLATE\n1s WAIT / 4s A -> B")},
		{EDarkwellMovingPropLabControlKind::VisibleRotate, FVector(-300, 260, 35), TEXT("F  VISIBLE ROTATE\n1s WAIT / 4s 180 DEG")},
		{EDarkwellMovingPropLabControlKind::HiddenAtoB, FVector(500, 260, 35), TEXT("F  ARM OFFSCREEN A -> B\nTHEN WALK ON ORANGE PLATE")},
		{EDarkwellMovingPropLabControlKind::CoverageBoundary, FVector(1400, 260, 35), TEXT("F  COVERAGE EDGE\n8s CONTINUOUS CROSSING")},
		{EDarkwellMovingPropLabControlKind::AtoBtoC, FVector(-1500, 700, 35), TEXT("F  ARM A -> B -> C\nUSE ORANGE PLATE TWICE")},
		{EDarkwellMovingPropLabControlKind::MultiProp, FVector(-1200, -1050, 35), TEXT("F  MULTI PROP\n2 MOVE / 1 ABSENT / 1 STATIC")},
		{EDarkwellMovingPropLabControlKind::ResetCurrent, FVector(1500, -1050, 35), TEXT("F  RESET CURRENT EXPERIMENT\nONLY EXPLICIT RESET CLEARS IT")}};
	for (const FControlDefinition& Definition : Definitions)
	{
		ADarkwellMovingPropLabControl* Control = GetWorld()->SpawnActor<ADarkwellMovingPropLabControl>(
			Definition.Location, FRotator::ZeroRotator);
		if (Control)
		{
			Control->Configure(this, Definition.Kind, FText::FromString(Definition.Label));
			InWorldControls.Add(Control);
		}
	}
}

void ADarkwellMovingPropLabRoom::DestroyInWorldControls()
{
	for (ADarkwellMovingPropLabControl* Control : InWorldControls)
	{
		if (IsValid(Control))
		{
			Control->Destroy();
		}
	}
	InWorldControls.Reset();
}

void ADarkwellMovingPropLabRoom::ResetInWorldControlState()
{
	StopMotion();
	CurrentInteraction = TEXT("NONE - CHOOSE A GREEN F CONTROL");
	AutoDelaySeconds = 0.0f;
	Scenario = 0;
	ScenarioPhase = 0;
	HiddenMoveIndex = 0;
	bInWorldScenarioSelected = false;
	bInWorldFinished = false;
	bPressureWaitingForExit = false;
}

FName ADarkwellMovingPropLabRoom::GetInWorldPropId(
	const EDarkwellMovingPropLabControlKind Kind) const
{
	switch (Kind)
	{
	case EDarkwellMovingPropLabControlKind::VisibleTranslate: return Darkwell::MovingPropLab::MainId;
	case EDarkwellMovingPropLabControlKind::VisibleRotate: return Darkwell::MovingPropLab::RotateId;
	case EDarkwellMovingPropLabControlKind::HiddenAtoB: return Darkwell::MovingPropLab::HiddenId;
	case EDarkwellMovingPropLabControlKind::CoverageBoundary: return Darkwell::MovingPropLab::EdgeId;
	case EDarkwellMovingPropLabControlKind::AtoBtoC: return Darkwell::MovingPropLab::AbcId;
	default: return NAME_None;
	}
}

bool ADarkwellMovingPropLabRoom::ResetCurrentInWorldZone()
{
	if (!bInWorldScenarioSelected)
	{
		return false;
	}
	if (ActiveControl == EDarkwellMovingPropLabControlKind::MultiProp)
	{
		for (const FName Id : {Darkwell::MovingPropLab::MultiHighId,
			Darkwell::MovingPropLab::MultiLowId, Darkwell::MovingPropLab::MultiTableId,
			Darkwell::MovingPropLab::MultiBoxId})
		{
			DestroyTracked(Id);
		}
		SpawnTracked(Darkwell::MovingPropLab::MultiHighId, 0, FVector(100, 70, 180),
			FLinearColor(.16f, .40f, .68f), FTransform(FVector(-1200, -650, 0)));
		SpawnTracked(Darkwell::MovingPropLab::MultiLowId, 0, FVector(130, 85, 80),
			FLinearColor(.66f, .34f, .16f), FTransform(FVector(-650, -650, 0)));
		SpawnTracked(Darkwell::MovingPropLab::MultiTableId, 6, FVector(240, 90, 90),
			FLinearColor(.26f, .58f, .34f), FTransform(FVector(0, -650, 0)));
		SpawnTracked(Darkwell::MovingPropLab::MultiBoxId, 8, FVector(65, 65, 65),
			FLinearColor(.62f, .22f, .32f), FTransform(FVector(700, -650, 0)));
	}
	else
	{
		const FName Id = GetInWorldPropId(ActiveControl);
		DestroyTracked(Id);
		switch (ActiveControl)
		{
		case EDarkwellMovingPropLabControlKind::VisibleTranslate:
			SpawnTracked(Id, 0, FVector(160, 80, 150), FLinearColor(.18f, .43f, .62f), FTransform(Darkwell::MovingPropLab::TranslateA)); break;
		case EDarkwellMovingPropLabControlKind::VisibleRotate:
			SpawnTracked(Id, 0, FVector(150, 75, 145), FLinearColor(.52f, .26f, .20f), FTransform(Darkwell::MovingPropLab::RotateA)); break;
		case EDarkwellMovingPropLabControlKind::HiddenAtoB:
			SpawnTracked(Id, 0, FVector(160, 80, 150), FLinearColor(.24f, .52f, .30f), FTransform(Darkwell::MovingPropLab::HiddenA)); break;
		case EDarkwellMovingPropLabControlKind::CoverageBoundary:
			SpawnTracked(Id, 0, FVector(150, 75, 145), FLinearColor(.52f, .44f, .18f), FTransform(Darkwell::MovingPropLab::EdgeA)); break;
		case EDarkwellMovingPropLabControlKind::AtoBtoC:
			SpawnTracked(Id, 0, FVector(145, 70, 140), FLinearColor(.46f, .24f, .56f), FTransform(Darkwell::MovingPropLab::AbcA)); break;
		default: break;
		}
	}
	CompletedInWorldControls.Remove(ActiveControl);
	ResetInWorldControlState();
	UE_LOG(LogDarkwellMovingPropLab, Display, TEXT("IN_WORLD_CONTROL reset current zone only"));
	return true;
}

bool ADarkwellMovingPropLabRoom::CanActivateInWorldControl(
	const EDarkwellMovingPropLabControlKind Kind) const
{
	if (!bInWorldControls || !bStarted)
	{
		return false;
	}
	if (Kind == EDarkwellMovingPropLabControlKind::ResetCurrent)
	{
		return bInWorldScenarioSelected;
	}
	return !IsCurrentInWorldControlBusy() && !CompletedInWorldControls.Contains(Kind);
}

bool ADarkwellMovingPropLabRoom::IsInWorldControlCompleted(
	const EDarkwellMovingPropLabControlKind Kind) const
{
	return CompletedInWorldControls.Contains(Kind);
}

bool ADarkwellMovingPropLabRoom::IsCurrentInWorldControlBusy() const
{
	if (!bInWorldScenarioSelected || CompletedInWorldControls.Contains(ActiveControl))
	{
		return false;
	}
	if (bMotionActive || AutoDelaySeconds > 0.0f)
	{
		return true;
	}
	// Hidden scenarios remain busy while armed, while awaiting the player at B/C,
	// and between the two pressure-plate legs. Their completion is explicit.
	if (ActiveControl == EDarkwellMovingPropLabControlKind::HiddenAtoB
		|| ActiveControl == EDarkwellMovingPropLabControlKind::AtoBtoC)
	{
		return !bInWorldFinished;
	}
	return !bInWorldFinished;
}

FString ADarkwellMovingPropLabRoom::GetNextInWorldControlLabel() const
{
	const TPair<EDarkwellMovingPropLabControlKind, const TCHAR*> Ordered[] = {
		{EDarkwellMovingPropLabControlKind::VisibleTranslate, TEXT("VISIBLE TRANSLATE")},
		{EDarkwellMovingPropLabControlKind::VisibleRotate, TEXT("VISIBLE ROTATE")},
		{EDarkwellMovingPropLabControlKind::HiddenAtoB, TEXT("OFFSCREEN A -> B")},
		{EDarkwellMovingPropLabControlKind::CoverageBoundary, TEXT("COVERAGE EDGE")},
		{EDarkwellMovingPropLabControlKind::AtoBtoC, TEXT("A -> B -> C")},
		{EDarkwellMovingPropLabControlKind::MultiProp, TEXT("MULTI PROP")}};
	for (const TPair<EDarkwellMovingPropLabControlKind, const TCHAR*>& Entry : Ordered)
	{
		if (!CompletedInWorldControls.Contains(Entry.Key))
		{
			return Entry.Value;
		}
	}
	return TEXT("RESET CURRENT OR RESTART PIE");
}

void ADarkwellMovingPropLabRoom::MarkActiveInWorldControlCompleted()
{
	if (!bInWorldScenarioSelected || ActiveControl == EDarkwellMovingPropLabControlKind::ResetCurrent)
	{
		return;
	}
	bInWorldFinished = true;
	CompletedInWorldControls.Add(ActiveControl);
	CurrentInteraction += FString::Printf(TEXT(" | NEXT TEST: %s"), *GetNextInWorldControlLabel());
	UE_LOG(LogDarkwellMovingPropLab, Display,
		TEXT("IN_WORLD_CONTROL completed scenario=%d control=%d next=%s"),
		Scenario, static_cast<int32>(ActiveControl), *GetNextInWorldControlLabel());
}

bool ADarkwellMovingPropLabRoom::ActivateInWorldControl(
	const EDarkwellMovingPropLabControlKind Kind,
	ADarkwellCharacter&)
{
	if (!CanActivateInWorldControl(Kind))
	{
		if (Kind == EDarkwellMovingPropLabControlKind::ResetCurrent)
		{
			CurrentInteraction = TEXT("RESET UNAVAILABLE - START AN EXPERIMENT FIRST");
		}
		else if (CompletedInWorldControls.Contains(Kind))
		{
			CurrentInteraction = TEXT("THIS EXPERIMENT IS COMPLETE - FOLLOW NEXT TEST OR RESET IT WHILE CURRENT");
		}
		else
		{
			CurrentInteraction = FString::Printf(TEXT("CONTROL BLOCKED - CURRENT SCENARIO %d IS STILL BUSY"), Scenario);
		}
		Report();
		return false;
	}
	if (Kind == EDarkwellMovingPropLabControlKind::ResetCurrent)
	{
		return ResetCurrentInWorldZone();
	}
	ActiveControl = Kind;
	bInWorldScenarioSelected = true;
	bInWorldFinished = false;
	ScenarioPhase = 0;
	HiddenMoveIndex = 0;
	bPressureWaitingForExit = false;
	switch (Kind)
	{
	case EDarkwellMovingPropLabControlKind::VisibleTranslate:
		Scenario = 1; CurrentInteraction = TEXT("VISIBLE TRANSLATE - HOLD VIEW; STARTS IN 1s"); AutoDelaySeconds = 1.0f; break;
	case EDarkwellMovingPropLabControlKind::VisibleRotate:
		Scenario = 2; CurrentInteraction = TEXT("VISIBLE ROTATE - HOLD VIEW; STARTS IN 1s"); AutoDelaySeconds = 1.0f; break;
	case EDarkwellMovingPropLabControlKind::HiddenAtoB:
		Scenario = 3; CurrentInteraction = TEXT("A->B ARMED - WALK BEHIND WALL ONTO ORANGE PLATE"); break;
	case EDarkwellMovingPropLabControlKind::CoverageBoundary:
		Scenario = 6; CurrentInteraction = TEXT("COVERAGE EDGE - HOLD VIEW; STARTS IN 1s"); AutoDelaySeconds = 1.0f; break;
	case EDarkwellMovingPropLabControlKind::AtoBtoC:
		Scenario = 7; CurrentInteraction = TEXT("A->B->C ARMED - WALK BEHIND WALL ONTO ORANGE PLATE"); break;
	case EDarkwellMovingPropLabControlKind::MultiProp:
		Scenario = 100; CurrentInteraction = TEXT("MULTI PROP - WATCH FOUR ITEMS; STARTS IN 1s"); AutoDelaySeconds = 1.0f; break;
	default: return false;
	}
	UE_LOG(LogDarkwellMovingPropLab, Display,
		TEXT("IN_WORLD_CONTROL activated scenario=%d interaction=%s"), Scenario, *CurrentInteraction);
	return true;
}

FText ADarkwellMovingPropLabRoom::GetInWorldControlPrompt(
	const EDarkwellMovingPropLabControlKind Kind) const
{
	if (Kind == EDarkwellMovingPropLabControlKind::ResetCurrent)
	{
		return FText::FromString(bInWorldScenarioSelected
			? TEXT("F — RESET CURRENT EXPERIMENT ZONE") : TEXT("RESET UNAVAILABLE — START AN EXPERIMENT FIRST"));
	}
	if (CompletedInWorldControls.Contains(Kind))
	{
		return FText::FromString(TEXT("COMPLETE — FOLLOW NEXT TEST; RESET ONLY CLEARS CURRENT ZONE"));
	}
	if (IsCurrentInWorldControlBusy())
	{
		return FText::FromString(FString::Printf(
			TEXT("WAIT — SCENARIO %d IS RUNNING OR AWAITING ITS NEXT STEP"), Scenario));
	}
	return FText::FromString(TEXT("F — START THIS EXPERIMENT"));
}

FText ADarkwellMovingPropLabRoom::GetInWorldControlDisplay(
	const EDarkwellMovingPropLabControlKind Kind) const
{
	const TCHAR* Name = TEXT("UNKNOWN");
	switch (Kind)
	{
	case EDarkwellMovingPropLabControlKind::VisibleTranslate: Name = TEXT("F  VISIBLE TRANSLATE\n1s WAIT / 4s A -> B"); break;
	case EDarkwellMovingPropLabControlKind::VisibleRotate: Name = TEXT("F  VISIBLE ROTATE\n1s WAIT / 4s 180 DEG"); break;
	case EDarkwellMovingPropLabControlKind::HiddenAtoB: Name = TEXT("F  ARM OFFSCREEN A -> B\nTHEN ORANGE PLATE"); break;
	case EDarkwellMovingPropLabControlKind::CoverageBoundary: Name = TEXT("F  COVERAGE EDGE\n8s CONTINUOUS CROSSING"); break;
	case EDarkwellMovingPropLabControlKind::AtoBtoC: Name = TEXT("F  ARM A -> B -> C\nORANGE PLATE TWICE"); break;
	case EDarkwellMovingPropLabControlKind::MultiProp: Name = TEXT("F  MULTI PROP\n2 MOVE / 1 ABSENT / 1 STATIC"); break;
	case EDarkwellMovingPropLabControlKind::ResetCurrent: Name = TEXT("F  RESET CURRENT EXPERIMENT\nONLY THIS CLEARS ITS RECORDS"); break;
	}
	FString State = TEXT("READY");
	if (Kind == EDarkwellMovingPropLabControlKind::ResetCurrent)
	{
		State = bInWorldScenarioSelected ? TEXT("ARMED") : TEXT("NO CURRENT TEST");
	}
	else if (CompletedInWorldControls.Contains(Kind))
	{
		State = TEXT("COMPLETE");
	}
	else if (bInWorldScenarioSelected && Kind == ActiveControl && IsCurrentInWorldControlBusy())
	{
		State = bMotionActive ? TEXT("RUNNING") : TEXT("ARMED / WAITING FOR NEXT STEP");
	}
	else if (IsCurrentInWorldControlBusy())
	{
		State = FString::Printf(TEXT("WAIT - SCENARIO %d BUSY"), Scenario);
	}
	else if (FString(Name).Contains(GetNextInWorldControlLabel()))
	{
		State = TEXT("NEXT TEST");
	}
	const FString Diagnostics = Kind == EDarkwellMovingPropLabControlKind::VisibleRotate
		? FString::Printf(TEXT("\nLIVE EPOCHS %d | STALE EPOCHS %d\nVISIBLE PROXIES %d | OVERLAP CONTRIBUTORS %d"),
			GetCurrentEpochCountForTesting(Darkwell::MovingPropLab::RotateId),
			GetStaleEpochCountForTesting(Darkwell::MovingPropLab::RotateId),
			GetVisibleHistoricalProxyCountForTesting(Darkwell::MovingPropLab::RotateId),
			GetMaxOverlapContributorsForTesting(Darkwell::MovingPropLab::RotateId))
		: FString();
	return FText::FromString(FString::Printf(TEXT("%s\n[%s]%s"), Name, *State, *Diagnostics));
}

FColor ADarkwellMovingPropLabRoom::GetInWorldControlColor(
	const EDarkwellMovingPropLabControlKind Kind) const
{
	if (Kind == EDarkwellMovingPropLabControlKind::ResetCurrent)
	{
		return bInWorldScenarioSelected ? FColor(255, 90, 230) : FColor(150, 70, 140);
	}
	if (CompletedInWorldControls.Contains(Kind))
	{
		return FColor(70, 220, 255);
	}
	if (bInWorldScenarioSelected && Kind == ActiveControl && IsCurrentInWorldControlBusy())
	{
		return FColor(255, 190, 45);
	}
	if (IsCurrentInWorldControlBusy())
	{
		return FColor(255, 105, 55);
	}
	return FColor(90, 255, 150);
}

ADarkwellMovingPropLabControl* ADarkwellMovingPropLabRoom::GetControlForTesting(
	const EDarkwellMovingPropLabControlKind Kind) const
{
	for (ADarkwellMovingPropLabControl* Control : InWorldControls)
	{
		if (IsValid(Control) && Control->GetKind() == Kind)
		{
			return Control;
		}
	}
	return nullptr;
}

void ADarkwellMovingPropLabRoom::ConfigureScenarioProps(const int32 InScenario)
{
	DestroyTracked();
	SpawnTracked(Darkwell::MovingPropLab::MainId, 0, FVector(180, 80, 150),
		FLinearColor(.18f, .43f, .62f), FTransform(Darkwell::MovingPropLab::A));
	SpawnTracked(TEXT("Lab.Moving.Bed"), 5, FVector(210, 100, 55),
		FLinearColor(.48f, .32f, .20f), FTransform(FVector(-1300, -650, 0)));
	SpawnTracked(TEXT("Lab.Moving.Table"), 6, FVector(150, 90, 85),
		FLinearColor(.36f, .24f, .16f), FTransform(FVector(-650, -650, 0)));
	SpawnTracked(TEXT("Lab.Moving.Lamp"), 7, FVector(45, 45, 120),
		FLinearColor(.72f, .58f, .22f), FTransform(FVector(-650, -650, 0)));
	SpawnTracked(TEXT("Lab.Moving.ThinPanel"), 8, FVector(180, 8, 130),
		FLinearColor(.30f, .52f, .46f), FTransform(FRotator(0, 20, 0), FVector(350, -650, 0)));
	SpawnTracked(TEXT("Lab.Moving.TwinA"), 0, FVector(90, 60, 110),
		FLinearColor(.45f, .30f, .18f), FTransform(FVector(900, -700, 0)));
	SpawnTracked(TEXT("Lab.Moving.TwinB"), 0, FVector(90, 60, 110),
		FLinearColor(.45f, .30f, .18f), FTransform(FVector(1100, -700, 0)));
	Scenario = InScenario;
	ScenarioPhase = 0;
	MultiCount = 0;
}

bool ADarkwellMovingPropLabRoom::ResetRoom(ADarkwellCharacter* Player)
{
	if (FindActive(GetWorld()) != this || !Player)
	{
		return false;
	}
	bInWorldControls = Darkwell::MovingPropLab::IsInWorldControlRequest(GetWorld());
	if (bInWorldControls && InWorldControls.IsEmpty())
	{
		PressurePlate->SetVisibility(true);
		PressureLabel->SetVisibility(true);
		SpawnInWorldControls();
	}
	Darkwell::MovingPropLab::SetMode2();
	if (bInWorldControls)
	{
		ConfigureInWorldProps();
	}
	else
	{
		ConfigureScenarioProps(0);
	}
	Player->RestorePersistentState(Player->GetActorTransform(), Player->GetMaxHealth(),
		DarkwellGameplayTags::State_Player_Alive, FGameplayTag());
	Player->GetLoadoutComponent()->RestorePersistentState(
		2, 100, 0, 100, DarkwellGameplayTags::Equipment_Left_Shotgun,
		DarkwellGameplayTags::Equipment_Right_Torch);
	TeleportPlayer(Player, bInWorldControls
		? FVector(-1100, 80, 92) : FVector(-1100, 300, 92), 90);
	bStarted = true;
	UE_LOG(LogDarkwellMovingPropLab, Display,
		TEXT("MOVING_RULES_RESET rule=SpatialEvidenceOnly enemy=0 timer=0 identities=%d inWorld=%d"),
		Tracked.Num(), bInWorldControls);
	return true;
}

bool ADarkwellMovingPropLabRoom::SelectScenario(
	const int32 InScenario,
	ADarkwellCharacter* Player)
{
	if (InScenario < 1 || InScenario > 7 || !ResetRoom(Player))
	{
		return false;
	}
	Scenario = InScenario;
	ScenarioPhase = 0;
	UE_LOG(LogDarkwellMovingPropLab, Display,
		TEXT("MOVING_RULES_SCENARIO scenario=%d phase=0 use advance; no deadline"), Scenario);
	return true;
}

bool ADarkwellMovingPropLabRoom::AdvanceScenario(ADarkwellCharacter* Player)
{
	if (Scenario == 100 && MultiCount >= 2 && Player)
	{
		FTrackedProp* Moving = Tracked.Find(TEXT("Lab.Multi.00"));
		if (ScenarioPhase == 0 && Moving && Moving->Actual.IsValid())
		{
			TeleportPlayer(Player, FVector(0, -850, 92), -90);
			Moving->Actual->AddActorWorldOffset(FVector(0, -500, 0));
			SetTrackedExists(TEXT("Lab.Multi.01"), false);
		}
		else if (ScenarioPhase == 1)
		{
			TeleportPlayer(Player, FVector(0, 100, 92), 90);
		}
		else
		{
			return false;
		}
		++ScenarioPhase;
		return true;
	}
	FTrackedProp* Main = Tracked.Find(Darkwell::MovingPropLab::MainId);
	ADarkwellPropLabFurniture* Actual = Main ? Main->Actual.Get() : nullptr;
	if (!Player || !Actual || Scenario < 1 || Scenario > 7)
	{
		return false;
	}
	auto HideAndMove = [&](const FVector Target, const FRotator Rotation = FRotator::ZeroRotator)
	{
		TeleportPlayer(Player, FVector(0, -850, 92), -90);
		Actual->SetActorLocationAndRotation(Target, Rotation);
	};
	auto StartMainMotion = [&](const FTransform& Target, const float Duration)
	{
		StartMotion(Actual, Target, Duration);
	};

	if (Scenario == 1 && ScenarioPhase == 0)
	{
		// Keep the complete translation inside the legal view so it rebases the
		// current epoch instead of becoming an offscreen move.
		StartMainMotion(FTransform(Darkwell::MovingPropLab::A + FVector(200, 0, 0)), 4.0f);
	}
	else if (Scenario == 2 && ScenarioPhase < 2)
	{
		const float Yaw = ScenarioPhase == 0 ? 90.0f : 180.0f;
		StartMainMotion(FTransform(FRotator(0, Yaw, 0), Darkwell::MovingPropLab::A), 3.0f);
	}
	else if (Scenario >= 3 && Scenario <= 5)
	{
		if (ScenarioPhase == 0) HideAndMove(Darkwell::MovingPropLab::B);
		else if (Scenario == 4 && ScenarioPhase == 1) TeleportPlayer(Player, FVector(-1100, 300, 92), 90);
		else if (Scenario == 4 && ScenarioPhase == 2) TeleportPlayer(Player, FVector(0, 300, 92), 90);
		else if ((Scenario == 3 || Scenario == 5) && ScenarioPhase == 1) TeleportPlayer(Player, FVector(0, 300, 92), 90);
		else if ((Scenario == 3 || Scenario == 5) && ScenarioPhase == 2) TeleportPlayer(Player, FVector(-1100, 300, 92), 90);
		else return false;
	}
	else if (Scenario == 6 && ScenarioPhase == 0)
	{
		// Leave legal view before the continuous move. The opaque divider blocks
		// A, so its last observation freezes without any false empty evidence.
		TeleportPlayer(Player, FVector(-1100, -1000, 92), -90);
		StartMainMotion(FTransform(FVector(-1100, -650, 0)), 8.0f);
	}
	else if (Scenario == 6 && ScenarioPhase == 1)
	{
		TeleportPlayer(Player, FVector(-1100, -1000, 92), 90);
	}
	else if (Scenario == 7)
	{
		if (ScenarioPhase == 0) HideAndMove(Darkwell::MovingPropLab::B);
		else if (ScenarioPhase == 1) TeleportPlayer(Player, FVector(0, 300, 92), 90);
		else if (ScenarioPhase == 2) HideAndMove(Darkwell::MovingPropLab::C);
		else if (ScenarioPhase == 3) TeleportPlayer(Player, FVector(1100, 300, 92), 90);
		else if (ScenarioPhase == 4) TeleportPlayer(Player, FVector(-1100, 300, 92), 90);
		else if (ScenarioPhase == 5) TeleportPlayer(Player, FVector(0, 300, 92), 90);
		else return false;
	}
	else
	{
		return false;
	}
	++ScenarioPhase;
	UE_LOG(LogDarkwellMovingPropLab, Display,
		TEXT("MOVING_RULES_ADVANCE scenario=%d phase=%d"), Scenario, ScenarioPhase);
	return true;
}

bool ADarkwellMovingPropLabRoom::SetMultiCount(
	const int32 Count,
	ADarkwellCharacter* Player)
{
	if ((Count != 2 && Count != 8 && Count != 32) || FindActive(GetWorld()) != this)
	{
		return false;
	}
	DestroyTracked();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 Column = Index % 8;
		const int32 Row = Index / 8;
		SpawnTracked(*FString::Printf(TEXT("Lab.Multi.%02d"), Index), 0,
			FVector(70, 55, 100), FLinearColor(.42f, .29f, .17f),
			FTransform(FVector(-1400 + Column * 370, 850 - Row * 260, 0)));
	}
	Scenario = 100;
	ScenarioPhase = 0;
	MultiCount = Count;
	TeleportPlayer(Player, FVector(0, 100, 92), 90);
	bStarted = true;
	UE_LOG(LogDarkwellMovingPropLab, Display,
		TEXT("MOVING_RULES_MULTI count=%d uniqueStableIds=%d"), Count, Tracked.Num());
	return Tracked.Num() == Count;
}

void ADarkwellMovingPropLabRoom::UpdatePressurePlate(ADarkwellCharacter* Player)
{
	if (!bInWorldScenarioSelected || !Player || bMotionActive
		|| (ActiveControl != EDarkwellMovingPropLabControlKind::HiddenAtoB
			&& ActiveControl != EDarkwellMovingPropLabControlKind::AtoBtoC))
	{
		return;
	}
	const FVector Offset = Player->GetActorLocation() - GetPressurePlatePosition();
	const bool bInside = FVector2D(Offset).SizeSquared() <= FMath::Square(170.0f)
		&& FMath::Abs(Offset.Z) < 160.0f;
	if (!bInside)
	{
		bPressureWaitingForExit = false;
		return;
	}
	if (bPressureWaitingForExit)
	{
		return;
	}
	const bool bFirstMove = ScenarioPhase == 0;
	const bool bSecondAbcMove = ActiveControl == EDarkwellMovingPropLabControlKind::AtoBtoC
		&& ScenarioPhase == 3;
	if (!bFirstMove && !bSecondAbcMove)
	{
		return;
	}
	const FName Id = GetInWorldPropId(ActiveControl);
	FTrackedProp* Prop = Tracked.Find(Id);
	ADarkwellPropLabFurniture* Actual = Prop ? Prop->Actual.Get() : nullptr;
	if (!Actual)
	{
		return;
	}
	const TArray<float> Coverage = ConservativeCoverage(ActualBounds(*Actual));
	const bool bAnyLegal = Coverage.ContainsByPredicate([](const float Value)
	{
		return Value >= FDarkwellSpatialPropMemory::LegalCoverage;
	});
	if (bAnyLegal)
	{
		CurrentInteraction = TEXT("PRESSURE BLOCKED - CABINET STILL HAS LEGAL COVERAGE");
		return;
	}
	const FVector Target = ActiveControl == EDarkwellMovingPropLabControlKind::HiddenAtoB
		? Darkwell::MovingPropLab::HiddenB
		: (bFirstMove ? Darkwell::MovingPropLab::AbcB : Darkwell::MovingPropLab::AbcC);
	// Seal exactly one fixed stale epoch while the source is still at A/B. This
	// precedes the first interpolated transform and prevents a one-frame overlap.
	FreezeCurrentForHiddenMotion(*Prop, bFirstMove ? TEXT("PRESSURE_A_TO_B") : TEXT("PRESSURE_B_TO_C"));
	StartMotion(Actual, FTransform(Target), 4.0f);
	bPressureWaitingForExit = true;
	ScenarioPhase = bFirstMove ? 1 : 4;
	CurrentInteraction = bFirstMove
		? TEXT("OFFSCREEN MOTION A -> B RUNNING (4s)")
		: TEXT("OFFSCREEN MOTION B -> C RUNNING (4s)");
}

void ADarkwellMovingPropLabRoom::UpdateInWorldAutomation(
	const float DeltaSeconds,
	ADarkwellCharacter* Player)
{
	if (!bInWorldControls || !bInWorldScenarioSelected)
	{
		return;
	}
	UpdatePressurePlate(Player);
	if (AutoDelaySeconds > 0.0f)
	{
		AutoDelaySeconds = FMath::Max(0.0f, AutoDelaySeconds - DeltaSeconds);
		if (AutoDelaySeconds > 0.0f)
		{
			return;
		}
		ScenarioPhase = 1;
		switch (ActiveControl)
		{
		case EDarkwellMovingPropLabControlKind::VisibleTranslate:
			StartMotion(Tracked.FindChecked(Darkwell::MovingPropLab::MainId).Actual.Get(),
				FTransform(Darkwell::MovingPropLab::TranslateB), 4.0f);
			CurrentInteraction = TEXT("VISIBLE TRANSLATE A -> B RUNNING (4s)");
			break;
		case EDarkwellMovingPropLabControlKind::VisibleRotate:
			StartMotion(Tracked.FindChecked(Darkwell::MovingPropLab::RotateId).Actual.Get(),
				FTransform(FRotator(0, 180, 0), Darkwell::MovingPropLab::RotateA), 4.0f);
			CurrentInteraction = TEXT("VISIBLE ROTATION 0 -> 180 RUNNING (4s)");
			break;
		case EDarkwellMovingPropLabControlKind::CoverageBoundary:
			StartMotion(Tracked.FindChecked(Darkwell::MovingPropLab::EdgeId).Actual.Get(),
				FTransform(Darkwell::MovingPropLab::EdgeB), 8.0f);
			CurrentInteraction = TEXT("COVERAGE BOUNDARY CROSSING RUNNING (8s)");
			break;
		case EDarkwellMovingPropLabControlKind::MultiProp:
			StartMotion(Tracked.FindChecked(Darkwell::MovingPropLab::MultiHighId).Actual.Get(),
				FTransform(FVector(-950, -650, 0)), 4.0f);
			StartMotion(Tracked.FindChecked(Darkwell::MovingPropLab::MultiLowId).Actual.Get(),
				FTransform(FVector(-650, -350, 0)), 4.0f);
			SetTrackedExists(Darkwell::MovingPropLab::MultiBoxId, false);
			CurrentInteraction = TEXT("MULTI: HIGH+LOW MOVING / BOX ABSENT / TABLE STATIC (4s)");
			break;
		default:
			break;
		}
	}
	if (!bMotionActive)
	{
		const FName Id = GetInWorldPropId(ActiveControl);
		if (ActiveControl == EDarkwellMovingPropLabControlKind::HiddenAtoB
			&& ScenarioPhase == 2 && GetSpatialRecordCount(Id) >= 2)
		{
			ScenarioPhase = 3;
			CurrentInteraction = TEXT("B SEEN / A MEMORY RETAINED - SCAN A TO ERASE IT");
			MarkActiveInWorldControlCompleted();
		}
		else if (ActiveControl == EDarkwellMovingPropLabControlKind::AtoBtoC
			&& ScenarioPhase == 2 && GetSpatialRecordCount(Id) >= 2)
		{
			ScenarioPhase = 3;
			HiddenMoveIndex = 1;
			CurrentInteraction = TEXT("B SEEN / A RETAINED - RETURN TO ORANGE PLATE FOR B -> C");
		}
		else if (ActiveControl == EDarkwellMovingPropLabControlKind::AtoBtoC
			&& ScenarioPhase == 5 && GetSpatialRecordCount(Id) >= 3)
		{
			ScenarioPhase = 6;
			CurrentInteraction = TEXT("C SEEN / A+B RETAINED - VERIFY OLD LOCATIONS INDEPENDENTLY");
			MarkActiveInWorldControlCompleted();
		}
	}
}

void ADarkwellMovingPropLabRoom::CompleteInWorldMotionGroup()
{
	if (!bInWorldScenarioSelected)
	{
		return;
	}
	if (ActiveControl == EDarkwellMovingPropLabControlKind::HiddenAtoB
		&& ScenarioPhase == 1)
	{
		ScenarioPhase = 2;
		CurrentInteraction = TEXT("A -> B FINISHED OFFSCREEN - RETURN TOP AND FIND B");
		return;
	}
	if (ActiveControl == EDarkwellMovingPropLabControlKind::AtoBtoC)
	{
		if (ScenarioPhase == 1)
		{
			ScenarioPhase = 2;
			CurrentInteraction = TEXT("A -> B FINISHED OFFSCREEN - RETURN TOP AND FIND B");
			return;
		}
		if (ScenarioPhase == 4)
		{
			ScenarioPhase = 5;
			CurrentInteraction = TEXT("B -> C FINISHED OFFSCREEN - RETURN TOP AND FIND C");
			return;
		}
	}
	ScenarioPhase = 2;
	CurrentInteraction = ActiveControl == EDarkwellMovingPropLabControlKind::MultiProp
		? TEXT("MULTI FINISHED - TWO MOVED / BOX ABSENT / TABLE UNCHANGED")
		: TEXT("MOTION FINISHED - STATE HELD UNTIL EXPLICIT RESET");
	MarkActiveInWorldControlCompleted();
}

void ADarkwellMovingPropLabRoom::StopMotion()
{
	bMotionActive = false;
	ActiveMotions.Reset();
}

void ADarkwellMovingPropLabRoom::StartMotion(
	ADarkwellPropLabFurniture* Prop,
	const FTransform& Target,
	const float Duration)
{
	if (!Prop || Duration <= 0.0f)
	{
		return;
	}
	FActiveMotion& Motion = ActiveMotions.AddDefaulted_GetRef();
	Motion.Prop = Prop;
	Motion.Start = Prop->GetActorTransform();
	Motion.End = Target;
	Motion.Duration = Duration;
	Motion.Seconds = 0.0f;
	bMotionActive = true;
}

void ADarkwellMovingPropLabRoom::UpdateDeterministicMotion(const float DeltaSeconds)
{
	if (!bMotionActive || ActiveMotions.IsEmpty())
	{
		return;
	}
	for (int32 Index = ActiveMotions.Num() - 1; Index >= 0; --Index)
	{
		FActiveMotion& Motion = ActiveMotions[Index];
		ADarkwellPropLabFurniture* Actual = Motion.Prop.Get();
		if (!Actual || Motion.Duration <= 0.0f)
		{
			ActiveMotions.RemoveAtSwap(Index);
			continue;
		}
		Motion.Seconds = FMath::Min(Motion.Seconds + DeltaSeconds, Motion.Duration);
		const float Alpha = Motion.Seconds / Motion.Duration;
		FTransform Transform;
		Transform.Blend(Motion.Start, Motion.End, Alpha);
		Actual->SetActorTransform(Transform);
		if (Alpha >= 1.0f)
		{
			ActiveMotions.RemoveAtSwap(Index);
		}
	}
	bMotionActive = !ActiveMotions.IsEmpty();
}

void ADarkwellMovingPropLabRoom::UpdateRoom(
	const float DeltaSeconds,
	ADarkwellCharacter* Player)
{
	if (!bStarted || !Player || FindActive(GetWorld()) != this)
	{
		return;
	}
	UpdateInWorldAutomation(DeltaSeconds, Player);
	const bool bWasMoving = bMotionActive;
	UpdateDeterministicMotion(DeltaSeconds);
	if (bInWorldControls && bWasMoving && !bMotionActive)
	{
		CompleteInWorldMotionGroup();
	}
	for (TPair<FName, FTrackedProp>& Pair : Tracked)
	{
		UpdateTracked(Pair.Value, DeltaSeconds);
	}
	UpdateInWorldAutomation(0.0f, Player);
	Report();
}

int32 ADarkwellMovingPropLabRoom::GetTotalSpatialRecordCount() const
{
	int32 Total = 0;
	for (const TPair<FName, FTrackedProp>& Pair : Tracked)
	{
		Total += Pair.Value.History.GetRecords().Num();
	}
	return Total;
}

int32 ADarkwellMovingPropLabRoom::GetSpatialRecordCount(const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? Prop->History.GetRecords().Num() : 0;
}

bool ADarkwellMovingPropLabRoom::IsActualPresent(const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop && Prop->bExists && Prop->Actual.IsValid();
}

int32 ADarkwellMovingPropLabRoom::GetTotalProxyCount() const
{
	int32 Total = 0;
	for (const TPair<FName, FTrackedProp>& Pair : Tracked)
	{
		for (const TPair<uint32, FRecordVisual>& Visual : Pair.Value.Visuals)
		{
			Total += Visual.Value.Proxy.IsValid() ? 1 : 0;
		}
	}
	return Total;
}

int32 ADarkwellMovingPropLabRoom::GetTotalCapTriangles() const
{
	int32 Total = 0;
	for (const TPair<FName, FTrackedProp>& Pair : Tracked)
	{
		for (const TPair<uint32, FRecordVisual>& Visual : Pair.Value.Visuals)
		{
			Total += Visual.Value.CapTriangles;
		}
	}
	return Total;
}

bool ADarkwellMovingPropLabRoom::TryDuplicateStableIdForTesting(const FName StableId)
{
	return SpawnTracked(StableId, 0, FVector(60, 60, 90), FLinearColor::Gray,
		FTransform(FVector::ZeroVector)) != nullptr;
}

bool ADarkwellMovingPropLabRoom::DoSpatialRecordTexturesMatchForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	if (!Prop)
	{
		return false;
	}
	for (const FDarkwellSpatialObservationRecord& Record : Prop->History.GetRecords())
	{
		const FRecordVisual* Visual = Prop->Visuals.Find(Record.Epoch);
		const UTexture2D* Texture = Visual ? Visual->Texture.Get() : nullptr;
		const FIntPoint Expected = Record.SpatialMemory.GetSize()
			* Darkwell::MovingPropLab::PresentationSamples;
		if (!Texture || Texture->GetSizeX() != Expected.X || Texture->GetSizeY() != Expected.Y)
		{
			return false;
		}
	}
	return true;
}

int32 ADarkwellMovingPropLabRoom::GetHiddenFreezeCountForTesting(const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? Prop->HiddenFreezeCount : 0;
}

int32 ADarkwellMovingPropLabRoom::GetHistoricalProxyVisibilityTransitionsForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	int32 Total = 0;
	if (Prop)
	{
		for (const FDarkwellSpatialObservationRecord& Record : Prop->History.GetRecords())
		{
			if (Record.bCurrentObservedLocation) continue;
			if (const FRecordVisual* Visual = Prop->Visuals.Find(Record.Epoch))
			{
				Total += Visual->ProxyVisibilityTransitions;
			}
		}
	}
	return Total;
}

int32 ADarkwellMovingPropLabRoom::GetHistoricalProxyCreationCountForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	int32 Total = 0;
	if (Prop)
	{
		for (const FDarkwellSpatialObservationRecord& Record : Prop->History.GetRecords())
		{
			if (Record.bCurrentObservedLocation) continue;
			if (const FRecordVisual* Visual = Prop->Visuals.Find(Record.Epoch))
			{
				Total += Visual->ProxyCreationCount;
			}
		}
	}
	return Total;
}

int32 ADarkwellMovingPropLabRoom::GetHistoricalTextureUploadCountForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	int32 Total = 0;
	if (Prop)
	{
		for (const FDarkwellSpatialObservationRecord& Record : Prop->History.GetRecords())
		{
			if (Record.bCurrentObservedLocation) continue;
			if (const FRecordVisual* Visual = Prop->Visuals.Find(Record.Epoch))
			{
				Total += Visual->TextureUploadCount;
			}
		}
	}
	return Total;
}

uint64 ADarkwellMovingPropLabRoom::GetHistoricalVisualSignatureForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	if (!Prop) return 0;
	uint64 Signature = 1469598103934665603ull;
	auto Mix = [&Signature](const uint64 Value)
	{
		Signature = (Signature ^ Value) * 1099511628211ull;
	};
	auto MixFloat = [&Mix](const float Value)
	{
		uint32 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		Mix(Bits);
	};
	for (const FDarkwellSpatialObservationRecord& Record : Prop->History.GetRecords())
	{
		if (Record.bCurrentObservedLocation) continue;
		Mix(Record.Epoch);
		const FRecordVisual* Visual = Prop->Visuals.Find(Record.Epoch);
		Mix(Visual && Visual->Proxy.IsValid() ? Visual->Proxy->GetUniqueID() : 0);
		Mix(Visual && Visual->Texture.IsValid() ? Visual->Texture->GetUniqueID() : 0);
		Mix(Visual && Visual->Texture.IsValid() ? Visual->Texture->GetSizeX() : 0);
		Mix(Visual && Visual->Texture.IsValid() ? Visual->Texture->GetSizeY() : 0);
		Mix(Visual && Visual->bLastProxyVisible ? 1 : 0);
		for (const FDarkwellSpatialPropMemory::FCell& Cell : Record.SpatialMemory.GetCells())
		{
			MixFloat(Cell.DiscoveredPresent);
			MixFloat(Cell.VerifiedEmpty);
			MixFloat(Cell.RemainingStale);
			MixFloat(Cell.StaleOpacity);
			MixFloat(Cell.CurrentLegalCoverage);
		}
	}
	return Signature;
}

FString ADarkwellMovingPropLabRoom::GetHistoricalVisualTelemetryForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	if (!Prop) return FString::Printf(TEXT("id=%s missing"), *StableId.ToString());
	TArray<FString> Records;
	for (const FDarkwellSpatialObservationRecord& Record : Prop->History.GetRecords())
	{
		if (Record.bCurrentObservedLocation) continue;
		const FRecordVisual* Visual = Prop->Visuals.Find(Record.Epoch);
		Records.Add(FString::Printf(
			TEXT("epoch=%u proxy=%d visible=%d transitions=%d texture=%d size=%dx%d creates=%d uploads=%d"),
			Record.Epoch,
			Visual && Visual->Proxy.IsValid() ? Visual->Proxy->GetUniqueID() : 0,
			Visual && Visual->bLastProxyVisible ? 1 : 0,
			Visual ? Visual->ProxyVisibilityTransitions : 0,
			Visual && Visual->Texture.IsValid() ? Visual->Texture->GetUniqueID() : 0,
			Visual && Visual->Texture.IsValid() ? Visual->Texture->GetSizeX() : 0,
			Visual && Visual->Texture.IsValid() ? Visual->Texture->GetSizeY() : 0,
			Visual ? Visual->ProxyCreationCount : 0,
			Visual ? Visual->TextureUploadCount : 0));
	}
	return FString::Printf(TEXT("id=%s freezes=%d records=[%s]"),
		*StableId.ToString(), Prop->HiddenFreezeCount, *FString::Join(Records, TEXT("; ")));
}

int32 ADarkwellMovingPropLabRoom::GetCurrentEpochCountForTesting(const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop && Prop->History.GetCurrentIndex() != INDEX_NONE ? 1 : 0;
}

int32 ADarkwellMovingPropLabRoom::GetStaleEpochCountForTesting(const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? Prop->History.GetRecords().Num()
		- (Prop->History.GetCurrentIndex() != INDEX_NONE ? 1 : 0) : 0;
}

int32 ADarkwellMovingPropLabRoom::GetVisibleHistoricalProxyCountForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	int32 Count = 0;
	if (Prop)
	{
		for (const FDarkwellSpatialObservationRecord& Record : Prop->History.GetRecords())
		{
			if (Record.bCurrentObservedLocation)
			{
				continue;
			}
			const FRecordVisual* Visual = Prop->Visuals.Find(Record.Epoch);
			Count += Visual && Visual->Proxy.IsValid() && !Visual->Proxy->IsHidden() ? 1 : 0;
		}
	}
	return Count;
}

int32 ADarkwellMovingPropLabRoom::GetMaxOverlapContributorsForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? Prop->MaxOverlapContributors : 0;
}

float ADarkwellMovingPropLabRoom::GetLastLegalCoverageRatioForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? Prop->LastLegalCoverageRatio : 0.0f;
}

float ADarkwellMovingPropLabRoom::GetNewestHistoricalYawForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	uint32 NewestEpoch = 0;
	float Yaw = 0.0f;
	if (Prop)
	{
		for (const FDarkwellSpatialObservationRecord& Record : Prop->History.GetRecords())
		{
			if (!Record.bCurrentObservedLocation && Record.Epoch >= NewestEpoch)
			{
				NewestEpoch = Record.Epoch;
				Yaw = Record.SnapshotTransform.Rotator().Yaw;
			}
		}
	}
	return Yaw;
}

bool ADarkwellMovingPropLabRoom::StartTrackedRotationForTesting(
	const FName StableId,
	const float TargetYaw,
	const float Duration)
{
	FTrackedProp* Prop = Tracked.Find(StableId);
	ADarkwellPropLabFurniture* Actual = Prop ? Prop->Actual.Get() : nullptr;
	if (!Actual || !FMath::IsFinite(TargetYaw) || Duration <= 0.0f || bMotionActive)
	{
		return false;
	}
	StartMotion(Actual, FTransform(FRotator(0.0f, TargetYaw, 0.0f),
		Actual->GetActorLocation()), Duration);
	return true;
}

FString ADarkwellMovingPropLabRoom::GetMotionState() const
{
	if (bMotionActive)
	{
		return TEXT("RUNNING");
	}
	return bInWorldFinished ? TEXT("FINISHED") : TEXT("STOPPED");
}

FString ADarkwellMovingPropLabRoom::GetCurrentInteraction() const
{
	return CurrentInteraction;
}

FString ADarkwellMovingPropLabRoom::GetObjectPositionLabel() const
{
	if (!bInWorldScenarioSelected)
	{
		return TEXT("-");
	}
	if (ActiveControl == EDarkwellMovingPropLabControlKind::MultiProp)
	{
		return TEXT("MULTI");
	}
	const FTrackedProp* Prop = Tracked.Find(GetInWorldPropId(ActiveControl));
	const ADarkwellPropLabFurniture* Actual = Prop ? Prop->Actual.Get() : nullptr;
	if (!Actual)
	{
		return TEXT("ABSENT");
	}
	if (bMotionActive)
	{
		return TEXT("TRANSIT");
	}
	const FVector Location = Actual->GetActorLocation();
	if (ActiveControl == EDarkwellMovingPropLabControlKind::VisibleTranslate)
	{
		return Location.Equals(Darkwell::MovingPropLab::TranslateB, 1.0f) ? TEXT("B") : TEXT("A");
	}
	if (ActiveControl == EDarkwellMovingPropLabControlKind::HiddenAtoB)
	{
		return Location.Equals(Darkwell::MovingPropLab::HiddenB, 1.0f) ? TEXT("B") : TEXT("A");
	}
	if (ActiveControl == EDarkwellMovingPropLabControlKind::AtoBtoC)
	{
		if (Location.Equals(Darkwell::MovingPropLab::AbcC, 1.0f)) return TEXT("C");
		if (Location.Equals(Darkwell::MovingPropLab::AbcB, 1.0f)) return TEXT("B");
		return TEXT("A");
	}
	if (ActiveControl == EDarkwellMovingPropLabControlKind::CoverageBoundary)
	{
		return Location.Equals(Darkwell::MovingPropLab::EdgeB, 1.0f) ? TEXT("B") : TEXT("A");
	}
	return TEXT("A");
}

FVector ADarkwellMovingPropLabRoom::GetPressurePlatePosition() const
{
	return PressurePlate ? PressurePlate->GetComponentLocation() : FVector::ZeroVector;
}

FTransform ADarkwellMovingPropLabRoom::GetTrackedTransform(const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop && Prop->Actual.IsValid()
		? Prop->Actual->GetActorTransform() : FTransform::Identity;
}

FString ADarkwellMovingPropLabRoom::GetTelemetry() const
{
	int32 Current = 0;
	int32 Historical = 0;
	int32 Caps = 0;
	for (const TPair<FName, FTrackedProp>& Pair : Tracked)
	{
		for (const FDarkwellSpatialObservationRecord& Record : Pair.Value.History.GetRecords())
		{
			Current += Record.bCurrentObservedLocation ? 1 : 0;
			Historical += Record.bCurrentObservedLocation ? 0 : 1;
		}
		for (const TPair<uint32, FRecordVisual>& Visual : Pair.Value.Visuals)
		{
			Caps += Visual.Value.CapTriangles;
		}
	}
	return FString::Printf(
		TEXT("{\"rule\":\"SpatialEvidenceOnly\",\"scenario\":%d,\"phase\":%d,\"motion\":\"%s\",\"position\":\"%s\",\"interaction\":\"%s\",\"identities\":%d,\"records\":%d,\"current\":%d,\"historical\":%d,\"capTriangles\":%d,\"multi\":%d,\"enemy\":0}"),
		Scenario, ScenarioPhase, *GetMotionState(), *GetObjectPositionLabel(), *CurrentInteraction.ReplaceCharWithEscapedChar(), Tracked.Num(), Current + Historical,
		Current, Historical, Caps, MultiCount);
}

void ADarkwellMovingPropLabRoom::Report()
{
	const FString RotationDiagnostics = Scenario == 2
		? FString::Printf(TEXT("\nROTATE: LIVE EPOCHS %d | STALE EPOCHS %d | VISIBLE PROXIES %d | OVERLAP CONTRIBUTORS %d | LEGAL COVERAGE %.1f%%"),
			GetCurrentEpochCountForTesting(Darkwell::MovingPropLab::RotateId),
			GetStaleEpochCountForTesting(Darkwell::MovingPropLab::RotateId),
			GetVisibleHistoricalProxyCountForTesting(Darkwell::MovingPropLab::RotateId),
			GetMaxOverlapContributorsForTesting(Darkwell::MovingPropLab::RotateId),
			GetLastLegalCoverageRatioForTesting(Darkwell::MovingPropLab::RotateId) * 100.0f)
		: FString();
	Status = FString::Printf(
		TEXT("MOVING + MULTI PROP LAB | MODE %d | RULE SpatialEvidenceOnly | ENEMY 0\nScenario %d | Phase %d | Motion %s | Object position %s\nCurrent interaction: %s%s\nCompleted %d/6 | NEXT TEST: %s\nIdentities %d | Spatial records %d | Multi %d\nWalk to a labeled mechanism and press F. Console is not required."),
		Darkwell::PropLab::PresentationMode(GetWorld()), Scenario, ScenarioPhase,
		*GetMotionState(), *GetObjectPositionLabel(), *CurrentInteraction, *RotationDiagnostics,
		CompletedInWorldControls.Num(), *GetNextInWorldControlLabel(),
		Tracked.Num(), GetTotalSpatialRecordCount(), MultiCount);
	for (ADarkwellMovingPropLabControl* Control : InWorldControls)
	{
		if (IsValid(Control)) Control->RefreshDisplay();
	}
	if (PressureLabel && bInWorldControls)
	{
		PressureLabel->SetText(FText::FromString(FString::Printf(
			TEXT("OFFSCREEN PRESSURE PLATE\n%s\nCabinet coverage must be ZERO"),
			bPressureWaitingForExit ? TEXT("WAITING FOR EXIT") : TEXT("ARMED"))));
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(0xDA474, 0.0f, FColor::Cyan, Status);
	}
}

void ADarkwellMovingPropLabRoom::Command(const TArray<FString>& Args)
{
	ADarkwellCharacter* Player = Cast<ADarkwellCharacter>(
		UGameplayStatics::GetPlayerPawn(this, 0));
	bool bHandled = false;
	if (Args.Num() == 2 && Args[1] == TEXT("reset"))
	{
		bHandled = ResetRoom(Player);
	}
	else if (Args.Num() == 2 && Args[1] == TEXT("advance"))
	{
		bHandled = AdvanceScenario(Player);
	}
	else if (Args.Num() == 2 && Args[1] == TEXT("stop"))
	{
		StopMotion();
		bHandled = true;
	}
	else if (Args.Num() == 3 && Args[1] == TEXT("scenario") && Args[2].IsNumeric())
	{
		bHandled = SelectScenario(FCString::Atoi(*Args[2]), Player);
	}
	else if (Args.Num() == 3 && Args[1] == TEXT("multi") && Args[2].IsNumeric())
	{
		bHandled = SetMultiCount(FCString::Atoi(*Args[2]), Player);
	}
	if (!bHandled || (Args.Num() == 2 && Args[1] == TEXT("help")))
	{
		UE_LOG(LogDarkwellMovingPropLab, Display,
			TEXT("Darkwell.PropLab moverules reset | scenario 1..7 | advance | multi 2/8/32 | stop | help. Scenarios: 1 visible translate; 2 visible rotate; 3 hidden A-B then B/A; 4 hidden A-B then A/B; 5 B-first then A; 6 cross coverage boundary; 7 A-B-C then verify A/B. No timer, policy switch or enemy."));
	}
	UE_LOG(LogDarkwellMovingPropLab, Display,
		TEXT("MOVING_RULES_COMMAND %s handled=%d telemetry=%s"),
		*FString::Join(Args, TEXT(" ")), bHandled, *GetTelemetry());
}
