#include "VisionPresentation/DarkwellMovingPropLabRoom.h"

#include "Camera/CameraComponent.h"
#include "Combat/DarkwellLoadoutComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
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
	// Render ownership is a closed-set presentation rule. This tolerance is one
	// half millimetre in UE centimetres and never enters coverage or D/V/R state.
	constexpr double RenderOwnershipContactTolerance = 0.05;
	// Clipping must finish strictly outside the closed contact set. This 0.01 mm
	// numeric margin only absorbs transform/intersection roundoff; diagnostics
	// continue to measure contact against RenderOwnershipContactTolerance.
	constexpr double RenderOwnershipClipPrecisionMargin = 0.001;
	constexpr double RenderOwnershipClipClearance = RenderOwnershipContactTolerance
		+ RenderOwnershipClipPrecisionMargin;
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

	const TCHAR* CoverageZeroReasonName(const EDarkwellFogCoverageZeroReason Reason)
	{
		switch (Reason)
		{
		case EDarkwellFogCoverageZeroReason::None: return TEXT("NONE");
		case EDarkwellFogCoverageZeroReason::SubsystemInactive: return TEXT("SUBSYSTEM_INACTIVE");
		case EDarkwellFogCoverageZeroReason::SourceInvalid: return TEXT("SOURCE_INVALID");
		case EDarkwellFogCoverageZeroReason::PointInvalid: return TEXT("POINT_INVALID");
		case EDarkwellFogCoverageZeroReason::ConeNotLegallyLive: return TEXT("NO_LEGAL_CONE");
		case EDarkwellFogCoverageZeroReason::Occluded: return TEXT("OCCLUDED");
		case EDarkwellFogCoverageZeroReason::OutsideLegalSource: return TEXT("OUTSIDE_LEGAL_SOURCE");
		default: return TEXT("UNKNOWN");
		}
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
		OwnedCaps.Remove(Cap);
		Cap->DestroyComponent();
	}
	if (UTexture2D* Texture = Visual.Texture.Get())
	{
		OwnedTextures.Remove(Texture);
	}
	for (const TWeakObjectPtr<UMaterialInstanceDynamic>& Material : Visual.Materials)
	{
		if (UMaterialInstanceDynamic* MaterialObject = Material.Get())
		{
			OwnedMaterials.Remove(MaterialObject);
		}
	}
	Visual.Proxy.Reset();
	Visual.Cap.Reset();
	Visual.Texture.Reset();
	Visual.Materials.Reset();
	Visual.CapTriangles = 0;
	Visual.CapSamplePoints.Reset();
	Visual.CapQuads.Reset();
	Visual.SubmittedPresentation.Reset();
	Visual.SuppressedByCurrentEvidence.Reset();
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

TArray<ADarkwellMovingPropLabRoom::FPrimitiveGeometrySnapshot>
ADarkwellMovingPropLabRoom::ActualPartGeometry(
	const ADarkwellPropLabFurniture& Prop) const
{
	TArray<FPrimitiveGeometrySnapshot> Result;
	int32 PrimitiveIndex = 0;
	for (const UStaticMeshComponent* Part : Prop.Memory->GetMemoryPrimitives())
	{
		if (Part && Part->IsRegistered() && Part->GetStaticMesh())
		{
			FPrimitiveGeometrySnapshot& Geometry = Result.AddDefaulted_GetRef();
			Geometry.LocalBounds = Part->GetStaticMesh()->GetBounds().GetBox();
			Geometry.WorldTransform = Part->GetComponentTransform();
			Geometry.PrimitiveIndex = PrimitiveIndex;
		}
		++PrimitiveIndex;
	}
	return Result;
}

bool ADarkwellMovingPropLabRoom::QueryVerticalInterval(
	const FPrimitiveGeometrySnapshot& Geometry,
	const FVector2D Point,
	double& OutMinZ,
	double& OutMaxZ,
	const double ProjectionTolerance)
{
	if (!Geometry.LocalBounds.IsValid || Geometry.WorldTransform.ContainsNaN())
	{
		return false;
	}
	const FVector LocalOrigin = Geometry.WorldTransform.InverseTransformPosition(
		FVector(Point.X, Point.Y, 0.0));
	const FVector LocalDirection = Geometry.WorldTransform.InverseTransformVector(
		FVector::UpVector);
	double Near = -UE_DOUBLE_BIG_NUMBER;
	double Far = UE_DOUBLE_BIG_NUMBER;
	const FVector Scale = Geometry.WorldTransform.GetScale3D().GetAbs();
	// Same XY closed set as ClipSegmentToGeometryProjection; Z clearance is
	// applied exactly once by SubtractOwnedCapIntervals, never to authority.
	const double LocalTolerance = ProjectionTolerance / FMath::Max(UE_DOUBLE_SMALL_NUMBER, FMath::Min(Scale.X, Scale.Y));
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const double Origin = LocalOrigin[Axis];
		const double Direction = LocalDirection[Axis];
		const double Minimum = Geometry.LocalBounds.Min[Axis] - (Axis < 2 ? LocalTolerance : 0.0);
		const double Maximum = Geometry.LocalBounds.Max[Axis] + (Axis < 2 ? LocalTolerance : 0.0);
		if (FMath::Abs(Direction) <= UE_DOUBLE_SMALL_NUMBER)
		{
			if (Origin < Minimum - UE_KINDA_SMALL_NUMBER
				|| Origin > Maximum + UE_KINDA_SMALL_NUMBER)
			{
				return false;
			}
			continue;
		}
		double AxisNear = (Minimum - Origin) / Direction;
		double AxisFar = (Maximum - Origin) / Direction;
		if (AxisNear > AxisFar)
		{
			Swap(AxisNear, AxisFar);
		}
		Near = FMath::Max(Near, AxisNear);
		Far = FMath::Min(Far, AxisFar);
		if (Far < Near)
		{
			return false;
		}
	}
	OutMinZ = Near;
	OutMaxZ = Far;
	return OutMaxZ - OutMinZ > UE_KINDA_SMALL_NUMBER;
}

bool ADarkwellMovingPropLabRoom::ClipSegmentToGeometryProjection(
	const FPrimitiveGeometrySnapshot& Geometry,
	const FVector2D Start,
	const FVector2D End,
	const double WorldTolerance,
	double& OutStartAlpha,
	double& OutEndAlpha)
{
	if (!Geometry.LocalBounds.IsValid || Geometry.WorldTransform.ContainsNaN())
	{
		return false;
	}
	// Lab furniture transforms only around world Z. In that contract, inverse XY
	// is independent of the chosen world Z and a slab clip is the exact projected
	// segment/OBB intersection, including endpoints and tangency.
	const FVector WorldUp = Geometry.WorldTransform.TransformVectorNoScale(FVector::UpVector);
	if (FMath::Abs(FVector::DotProduct(WorldUp.GetSafeNormal(), FVector::UpVector)) < 0.9999)
	{
		return false;
	}
	const double ReferenceZ = Geometry.WorldTransform.GetLocation().Z;
	const FVector LocalStart3 = Geometry.WorldTransform.InverseTransformPosition(
		FVector(Start.X, Start.Y, ReferenceZ));
	const FVector LocalEnd3 = Geometry.WorldTransform.InverseTransformPosition(
		FVector(End.X, End.Y, ReferenceZ));
	const FVector2D LocalStart(LocalStart3.X, LocalStart3.Y);
	const FVector2D LocalEnd(LocalEnd3.X, LocalEnd3.Y);
	const FVector2D Delta = LocalEnd - LocalStart;
	const FVector Scale = Geometry.WorldTransform.GetScale3D().GetAbs();
	const double MinimumScale = FMath::Max(UE_DOUBLE_SMALL_NUMBER,
		FMath::Min(Scale.X, Scale.Y));
	const double LocalTolerance = WorldTolerance / MinimumScale;
	OutStartAlpha = 0.0;
	OutEndAlpha = 1.0;
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		const double Minimum = Geometry.LocalBounds.Min[Axis] - LocalTolerance;
		const double Maximum = Geometry.LocalBounds.Max[Axis] + LocalTolerance;
		if (FMath::Abs(Delta[Axis]) <= UE_DOUBLE_SMALL_NUMBER)
		{
			if (LocalStart[Axis] < Minimum || LocalStart[Axis] > Maximum)
			{
				return false;
			}
			continue;
		}
		double Near = (Minimum - LocalStart[Axis]) / Delta[Axis];
		double Far = (Maximum - LocalStart[Axis]) / Delta[Axis];
		if (Near > Far)
		{
			Swap(Near, Far);
		}
		OutStartAlpha = FMath::Max(OutStartAlpha, Near);
		OutEndAlpha = FMath::Min(OutEndAlpha, Far);
		if (OutEndAlpha + UE_DOUBLE_SMALL_NUMBER < OutStartAlpha)
		{
			return false;
		}
	}
	return OutEndAlpha >= 0.0 && OutStartAlpha <= 1.0;
}

bool ADarkwellMovingPropLabRoom::CollectCurrentOwnedVerticalIntervals(
	const FTrackedProp& Prop,
	const FVector2D Point,
	TArray<FVector2D>& OutIntervals, const double ProjectionTolerance) const
{
	OutIntervals.Reset();
	if (ProjectionTolerance == 0.0 && !HasCurrentObservedContributionAt(Prop, Point))
	{
		return false;
	}
	const ADarkwellPropLabFurniture* Actual = Prop.bExists ? Prop.Actual.Get() : nullptr;
	if (!Actual)
	{
		return false;
	}
	for (const FPrimitiveGeometrySnapshot& Geometry : ActualPartGeometry(*Actual))
	{
		if (ProjectionTolerance > 0.0)
		{
			FVector Local = Geometry.WorldTransform.InverseTransformPosition(FVector(Point, Geometry.WorldTransform.GetLocation().Z));
			Local.X = FMath::Clamp(Local.X, Geometry.LocalBounds.Min.X + 1.e-6, Geometry.LocalBounds.Max.X - 1.e-6);
			Local.Y = FMath::Clamp(Local.Y, Geometry.LocalBounds.Min.Y + 1.e-6, Geometry.LocalBounds.Max.Y - 1.e-6);
			if (!HasCurrentObservedContributionAt(Prop, FVector2D(Geometry.WorldTransform.TransformPosition(Local)))) continue;
		}
		double MinimumZ = 0.0;
		double MaximumZ = 0.0;
		if (QueryVerticalInterval(Geometry, Point, MinimumZ, MaximumZ, ProjectionTolerance))
		{
			OutIntervals.Add(FVector2D(MinimumZ, MaximumZ));
		}
	}
	return !OutIntervals.IsEmpty();
}

bool ADarkwellMovingPropLabRoom::CollectNewerOwnedVerticalIntervals(
	const FTrackedProp& Prop,
	const uint32 OlderEpoch,
	const FVector2D Point,
	TArray<FVector2D>& OutIntervals, const double ProjectionTolerance) const
{
	OutIntervals.Reset();
	for (const FDarkwellSpatialObservationRecord& Candidate : Prop.History.GetRecords())
	{
		if (Candidate.Epoch <= OlderEpoch)
		{
			continue;
		}
		if (Candidate.bCurrentObservedLocation)
		{
			TArray<FVector2D> CurrentIntervals;
			CollectCurrentOwnedVerticalIntervals(Prop, Point, CurrentIntervals, ProjectionTolerance);
			OutIntervals.Append(CurrentIntervals);
			continue;
		}
		const FRecordVisual* Visual = Prop.Visuals.Find(Candidate.Epoch);
		const FBox2D& Bounds = Candidate.SpatialMemory.GetBounds();
		const FIntPoint Coarse = Candidate.SpatialMemory.GetSize();
		const FIntPoint Fine = Coarse * Darkwell::MovingPropLab::PresentationSamples;
		if (!Visual || Visual->bPresentationRetired || (ProjectionTolerance == 0.0 && !Bounds.IsInside(Point))
			|| Fine.X <= 0 || Fine.Y <= 0)
		{
			continue;
		}
		const FVector2D Relative = (Point - Bounds.Min) / Bounds.GetSize();
		const int32 FineX = FMath::Clamp(FMath::FloorToInt(Relative.X * Fine.X), 0, Fine.X - 1);
		const int32 FineY = FMath::Clamp(FMath::FloorToInt(Relative.Y * Fine.Y), 0, Fine.Y - 1);
		const int32 FineIndex = FineY * Fine.X + FineX;
		const int32 CellIndex = (FineY / Darkwell::MovingPropLab::PresentationSamples)
			* Coarse.X + FineX / Darkwell::MovingPropLab::PresentationSamples;
		const bool bSuppressed = Visual->SuppressedByCurrentEvidence.IsValidIndex(FineIndex)
			&& Visual->SuppressedByCurrentEvidence[FineIndex];
		if (bSuppressed || !Candidate.SpatialMemory.GetCells().IsValidIndex(CellIndex)
			|| Candidate.SpatialMemory.Presentation(CellIndex).B <= 0.0f)
		{
			continue;
		}
		for (const FPrimitiveGeometrySnapshot& Geometry : Visual->PartGeometry)
		{
			double MinimumZ = 0.0;
			double MaximumZ = 0.0;
			if (QueryVerticalInterval(Geometry, Point, MinimumZ, MaximumZ, ProjectionTolerance))
			{
				OutIntervals.Add(FVector2D(MinimumZ, MaximumZ));
			}
		}
	}
	return !OutIntervals.IsEmpty();
}

bool ADarkwellMovingPropLabRoom::HasNewerObservedGeometryOverlapAt(
	const FTrackedProp& Prop,
	const FRecordVisual& OlderVisual,
	const uint32 OlderEpoch,
	const FVector2D Point) const
{
	TArray<FVector2D> NewerIntervals;
	if (!CollectNewerOwnedVerticalIntervals(Prop, OlderEpoch, Point, NewerIntervals))
	{
		return false;
	}
	for (const FPrimitiveGeometrySnapshot& OldGeometry : OlderVisual.PartGeometry)
	{
		double OldMinZ = 0.0;
		double OldMaxZ = 0.0;
		if (!QueryVerticalInterval(OldGeometry, Point, OldMinZ, OldMaxZ))
		{
			continue;
		}
		for (const FVector2D Newer : NewerIntervals)
		{
			if (FMath::Min(OldMaxZ, Newer.Y)
				+ Darkwell::MovingPropLab::RenderOwnershipContactTolerance
				>= FMath::Max(OldMinZ, Newer.X))
			{
				return true;
			}
		}
	}
	return false;
}

TArray<ADarkwellMovingPropLabRoom::FPrimitiveGeometrySnapshot>
ADarkwellMovingPropLabRoom::CollectNewerGeometrySnapshots(
	const FTrackedProp& Prop,
	const uint32 OlderEpoch) const
{
	TArray<FPrimitiveGeometrySnapshot> Result;
	for (const FDarkwellSpatialObservationRecord& Candidate : Prop.History.GetRecords())
	{
		if (Candidate.Epoch <= OlderEpoch)
		{
			continue;
		}
		if (Candidate.bCurrentObservedLocation)
		{
			if (const ADarkwellPropLabFurniture* Actual = Prop.bExists ? Prop.Actual.Get() : nullptr)
			{
				Result.Append(ActualPartGeometry(*Actual));
			}
			continue;
		}
		if (const FRecordVisual* Visual = Prop.Visuals.Find(Candidate.Epoch);
			Visual && !Visual->bPresentationRetired)
		{
			Result.Append(Visual->PartGeometry);
		}
	}
	return Result;
}

bool ADarkwellMovingPropLabRoom::HasNewerObservedGeometryOverlapWithinFootprint(
	const FTrackedProp& Prop,
	const FRecordVisual& OlderVisual,
	const uint32 OlderEpoch,
	const FBox2D& Footprint) const
{
	if (!Footprint.bIsValid)
	{
		return false;
	}
	auto TestPoint = [&](const FVector2D Point)
	{
		return HasNewerObservedGeometryOverlapAt(
			Prop, OlderVisual, OlderEpoch, Point);
	};
	const FVector2D Center = Footprint.GetCenter();
	const FVector2D Corners[] = {
		Footprint.Min,
		FVector2D(Footprint.Max.X, Footprint.Min.Y),
		Footprint.Max,
		FVector2D(Footprint.Min.X, Footprint.Max.Y)};
	if (TestPoint(Center))
	{
		return true;
	}
	for (const FVector2D Corner : Corners)
	{
		if (TestPoint(Corner))
		{
			return true;
		}
	}
	for (const FPrimitiveGeometrySnapshot& Geometry
		: CollectNewerGeometrySnapshots(Prop, OlderEpoch))
	{
		const FVector LocalCenter = Geometry.LocalBounds.GetCenter();
		const FVector WorldCenter = Geometry.WorldTransform.TransformPosition(LocalCenter);
		const FVector2D ProjectedCenter(WorldCenter.X, WorldCenter.Y);
		if (Footprint.IsInside(ProjectedCenter) && TestPoint(ProjectedCenter))
		{
			return true;
		}
		for (int32 Edge = 0; Edge < 4; ++Edge)
		{
			double Alpha0 = 0.0;
			double Alpha1 = 0.0;
			if (!ClipSegmentToGeometryProjection(
				Geometry, Corners[Edge], Corners[(Edge + 1) % 4],
				Darkwell::MovingPropLab::RenderOwnershipClipClearance, Alpha0, Alpha1))
			{
				continue;
			}
			const FVector2D Contact = FMath::Lerp(
				Corners[Edge], Corners[(Edge + 1) % 4],
				FMath::Clamp((Alpha0 + Alpha1) * 0.5, 0.0, 1.0));
			if (TestPoint(Contact))
			{
				return true;
			}
		}
	}
	return false;
}

TArray<float> ADarkwellMovingPropLabRoom::ConservativeCoverage(
	const FBox2D& Bounds) const
{
	return SampleConservativeCoverage(Bounds, 0, 0).Values;
}

ADarkwellMovingPropLabRoom::FCoverageSnapshot
ADarkwellMovingPropLabRoom::SampleConservativeCoverage(
	const FBox2D& Bounds,
	const uint64 TransformRevision,
	const uint64 GridRevision, const int32 Subdivision) const
{
	FCoverageSnapshot Result;
	Result.TransformRevision = TransformRevision;
	Result.GridRevision = GridRevision;
	const UDarkwellFogVisualSubsystem* Fog = GetWorld()
		? GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>() : nullptr;
	if (!Fog || !Bounds.bIsValid)
	{
		Result.ZeroReason = Fog ? TEXT("BOUNDS_INVALID") : TEXT("FOG_UNAVAILABLE");
		return Result;
	}
	const FIntPoint CoarseSize(
		FMath::CeilToInt(Bounds.GetSize().X / Darkwell::MovingPropLab::CellSize),
		FMath::CeilToInt(Bounds.GetSize().Y / Darkwell::MovingPropLab::CellSize));
	const FIntPoint Size = CoarseSize * Subdivision;
	if (Size.X <= 0 || Size.Y <= 0)
	{
		Result.ZeroReason = TEXT("GRID_INVALID");
		return Result;
	}
	const FVector2D Step = Bounds.GetSize() / FVector2D(Size.X, Size.Y);
	TArray<float> Corners;
	Corners.SetNumUninitialized((Size.X + 1) * (Size.Y + 1));
	Result.Values.SetNumUninitialized(Size.X * Size.Y);
	Result.bValid = true;
	bool bRevisionInitialized = false;
	EDarkwellFogCoverageZeroReason AggregateZeroReason = EDarkwellFogCoverageZeroReason::None;
	auto Sample = [&](const FVector2D Point)
	{
		const FDarkwellFogVisualCoverageQuery Query = Fog->QueryLiveCoverageAtWorldPoint(Point);
		if (!Query.bValid)
		{
			Result.bValid = false;
			AggregateZeroReason = Query.ZeroReason;
			return 0.0f;
		}
		if (!bRevisionInitialized)
		{
			Result.AuthorityRevision = Query.AuthorityRevision;
			Result.CoverageRevision = Query.CoverageDrawRevision;
			bRevisionInitialized = true;
		}
		else if (Result.AuthorityRevision != Query.AuthorityRevision
			|| Result.CoverageRevision != Query.CoverageDrawRevision)
		{
			Result.bValid = false;
			Result.ZeroReason = TEXT("REVISION_MISMATCH");
		}
		if (Query.Coverage <= 0.0f && AggregateZeroReason == EDarkwellFogCoverageZeroReason::None)
		{
			AggregateZeroReason = Query.ZeroReason;
		}
		return Query.Coverage;
	};
	for (int32 Y = 0; Y <= Size.Y; ++Y)
	{
		for (int32 X = 0; X <= Size.X; ++X)
		{
			Corners[Y * (Size.X + 1) + X] = Sample(
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
			Value = FMath::Min(Value, Sample(
				Bounds.Min + Step * FVector2D(X + .5f, Y + .5f)));
			Result.Values[Y * Size.X + X] = Value;
		}
	}
	if (!Result.bValid)
	{
		if (Result.ZeroReason == TEXT("NOT_SAMPLED"))
		{
			Result.ZeroReason = Darkwell::MovingPropLab::CoverageZeroReasonName(AggregateZeroReason);
		}
		return Result;
	}
	float Maximum = 0.0f;
	for (const float Value : Result.Values)
	{
		Maximum = FMath::Max(Maximum, Value);
	}
	const bool bAnyLegal = Result.Values.ContainsByPredicate([](const float Value)
	{
		return Value >= FDarkwellSpatialPropMemory::LegalCoverage;
	});
	Result.ZeroReason = bAnyLegal ? TEXT("NONE")
		: (Maximum > 0.0f ? TEXT("BELOW_LEGAL_THRESHOLD")
			: Darkwell::MovingPropLab::CoverageZeroReasonName(AggregateZeroReason));
	return Result;
}

void ADarkwellMovingPropLabRoom::AdvanceFineHistory(
	FTrackedProp& Prop, FDarkwellSpatialObservationRecord& Record, const float DeltaSeconds)
{
	FRecordVisual* Visual = Prop.Visuals.Find(Record.Epoch);
	if (!Visual || !Record.FineHistory.IsInitialized()) return;
	const FCoverageSnapshot Coverage = SampleConservativeCoverage(Record.FineHistory.GetBounds(),
		Record.Epoch, Record.SpatialMemory.GetGeneration(), FDarkwellHistoryGridV2::SamplesPerCell);
	if (!Coverage.bValid) return;
	const FIntPoint Size = Record.FineHistory.GetSize();
	const FBox2D& Bounds = Record.FineHistory.GetBounds();
	const FVector2D Step = Bounds.GetSize() / FVector2D(Size.X, Size.Y);
	TBitArray<> Occupied(false, Size.X * Size.Y);
	for (int32 Y = 0; Y < Size.Y; ++Y) for (int32 X = 0; X < Size.X; ++X)
	{
		Occupied[Y * Size.X + X] = IsOccupiedByActual(Bounds.Min + Step * FVector2D(X + .5, Y + .5), NAME_None);
	}
	// Checkpoint A: independent diagnostic evidence. Legacy fields still own all output.
	Record.FineHistory.Advance(DeltaSeconds, Coverage.Values, Occupied, Visual->SuppressedByCurrentEvidence);
}

FString ADarkwellMovingPropLabRoom::GetFineHistoryTelemetry(const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	FString Result;
	if (!Prop) return Result;
	for (const auto& Record : Prop->History.GetRecords())
	{
		const auto& Grid = Record.FineHistory;
		if (!Grid.IsInitialized()) continue;
		int32 OldBlockedNewEmpty = 0;
		const FIntPoint Coarse = Record.SpatialMemory.GetSize();
		const FIntPoint Fine = Grid.GetSize();
		for (int32 Y = 0; Y < Fine.Y; ++Y) for (int32 X = 0; X < Fine.X; ++X)
		{
			const auto& Cell = Record.SpatialMemory.GetCells()[(Y / 4) * Coarse.X + X / 4];
			OldBlockedNewEmpty += Cell.InitialRemembered > 0 && Cell.VerifiedEmpty == 0
				&& Grid.GetSamples()[Y * Fine.X + X].State == FDarkwellHistoryGridV2::VerifiedEmpty();
		}
		Result += FString::Printf(TEXT("epoch=%u fine=%dx%d never=%d unresolved=%d empty=%d superseded=%d mixed=%d oldBlockedNewEmpty=%d;"),
			Record.Epoch, Fine.X, Fine.Y, Grid.Count(Grid.NeverObserved()), Grid.Count(Grid.Unresolved()),
			Grid.Count(Grid.VerifiedEmpty()), Grid.Count(Grid.Superseded()), Grid.CountMixedCoarseCells(), OldBlockedNewEmpty);
	}
	return Result;
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
			// Actor bounds are only a broad phase. Empty space beside a rotated
			// door/handle must reach legal empty verification, not retain old skin.
			for (const FPrimitiveGeometrySnapshot& Geometry : ActualPartGeometry(*Actual))
			{
				double MinZ, MaxZ;
				if (QueryVerticalInterval(Geometry, Point, MinZ, MaxZ)) return true;
			}
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

bool ADarkwellMovingPropLabRoom::HasNewerObservedContributionAt(
	const FTrackedProp& Prop,
	const uint32 OlderEpoch,
	const FVector2D Point) const
{
	TArray<FVector2D> Intervals;
	return CollectNewerOwnedVerticalIntervals(Prop, OlderEpoch, Point, Intervals);
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
			const FVector2D Minimum = Bounds.Min + Step * FVector2D(X, Y);
			const bool bSuperseded = HasNewerObservedGeometryOverlapWithinFootprint(
				Prop, *Visual, Record.Epoch, FBox2D(Minimum, Minimum + Step));
			if (!Visual->SuppressedByCurrentEvidence[Index] && bSuperseded)
			{
				// This is a monotonic presentation-ownership decision backed by new
				// legal present evidence. It does not mark the old cell verified empty
				// and does not alter D/V/R authority.
				Visual->SuppressedByCurrentEvidence[Index] = true;
			}
		}
	}
}

bool ADarkwellMovingPropLabRoom::IsHistoricalPresentationResolved(
	const FDarkwellSpatialObservationRecord& Record,
	const FRecordVisual& Visual) const
{
	if (Record.bCurrentObservedLocation)
	{
		return false;
	}
	// A real remaining cut fragment can extend above/below newer owned space.
	// Surface-only XY retirement must not discard that independently clipped cap.
	if (Visual.CapTriangles > 0) return false;
	const FIntPoint Coarse = Record.SpatialMemory.GetSize();
	const int32 Samples = Darkwell::MovingPropLab::PresentationSamples;
	const FIntPoint Fine = Coarse * Samples;
	const FBox2D& Bounds = Record.SpatialMemory.GetBounds();
	if (Fine.X <= 0 || Fine.Y <= 0
		|| Visual.SuppressedByCurrentEvidence.Num() != Fine.X * Fine.Y)
	{
		return false;
	}
	const FVector2D Step = Bounds.GetSize() / FVector2D(Fine.X, Fine.Y);
	for (int32 Y = 0; Y < Fine.Y; ++Y)
	{
		for (int32 X = 0; X < Fine.X; ++X)
		{
			const int32 FineIndex = Y * Fine.X + X;
			const int32 CellIndex = (Y / Samples) * Coarse.X + X / Samples;
			const FVector2D Point = Bounds.Min + Step * FVector2D(X + 0.5f, Y + 0.5f);
			bool bInsideRecordedPart = Visual.PartGeometry.IsEmpty();
			for (const FPrimitiveGeometrySnapshot& Geometry : Visual.PartGeometry)
			{
				double MinimumZ = 0.0;
				double MaximumZ = 0.0;
				bInsideRecordedPart |= QueryVerticalInterval(
					Geometry, Point, MinimumZ, MaximumZ);
			}
			if (!bInsideRecordedPart)
			{
				continue;
			}
			if (!Visual.SuppressedByCurrentEvidence[FineIndex]
				&& Record.SpatialMemory.Presentation(CellIndex).B > 0.0f)
			{
				return false;
			}
		}
	}
	return true;
}

void ADarkwellMovingPropLabRoom::RetireHistoricalPresentation(
	FTrackedProp& Prop,
	FRecordVisual& Visual)
{
	if (Visual.bPresentationRetired)
	{
		return;
	}
	UE_LOG(LogDarkwellMovingPropLab, Display,
		TEXT("MOVING_RULES_PRESENTATION_RETIRED id=%s epoch=%u proxy=%d cap=%d texture=%d"),
		*Prop.StableId.ToString(), Visual.Epoch,
		Visual.Proxy.IsValid() ? Visual.Proxy->GetUniqueID() : 0,
		Visual.Cap.IsValid() ? Visual.Cap->GetUniqueID() : 0,
		Visual.Texture.IsValid() ? Visual.Texture->GetUniqueID() : 0);
	DestroyVisual(Visual);
	Visual.bPresentationRetired = true;
}

void ADarkwellMovingPropLabRoom::RefreshContributionDiagnostics(
	FTrackedProp& Prop) const
{
	Prop.MaxSurfaceContributors = 0;
	Prop.MaxCapContributors = 0;
	Prop.MaxTotalContributors = 0;
	Prop.VisibleHistoricalCaps = 0;
	Prop.Current3DOverlapStaleSurface = 0;
	Prop.Current3DOverlapStaleCap = 0;
	Prop.Max3DRenderOwnershipContributors = 1;
	Prop.CurrentRenderContactStaleSurface = 0;
	Prop.CurrentRenderContactStaleCap = 0;
	Prop.HardOwnershipFilterLeak = 0;
	Prop.ResidualFragmentDiagnostics.Reset();
	Prop.Offending3DEpoch = 0;
	Prop.Offending3DPrimitive = INDEX_NONE;
	Prop.Offending3DWorldPosition = FVector::ZeroVector;
	TArray<FVector2D> SamplePoints;
	auto SurfaceContributorsAt = [&](const FVector2D Point)
	{
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
			if (!Visual || Visual->bPresentationRetired || Fine.X <= 0 || Fine.Y <= 0)
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
		return Contributors;
	};
	auto CapContributorsAt = [&](const FVector2D Point)
	{
		int32 Contributors = 0;
		for (const TPair<uint32, FRecordVisual>& Pair : Prop.Visuals)
		{
			const FRecordVisual& Visual = Pair.Value;
			if (!Visual.Cap.IsValid() || !Visual.Cap->IsVisible()
				|| Visual.CapTriangles <= 0)
			{
				continue;
			}
			if (Visual.CapSamplePoints.ContainsByPredicate([&](const FVector2D Candidate)
			{
				return Candidate.Equals(Point, 0.25f);
			}))
			{
				++Contributors;
			}
		}
		return Contributors;
	};
	for (const FDarkwellSpatialObservationRecord& SampleRecord : Prop.History.GetRecords())
	{
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
				SamplePoints.Add(SampleBounds.Min
					+ Step * FVector2D(X + 0.5f, Y + 0.5f));
			}
		}
	}
	for (const FDarkwellSpatialObservationRecord& Record : Prop.History.GetRecords())
	{
		const FRecordVisual* Visual = Prop.Visuals.Find(Record.Epoch);
		if (!Visual || Visual->bPresentationRetired)
		{
			continue;
		}
		SamplePoints.Append(Visual->CapSamplePoints);
		if (!Record.bCurrentObservedLocation && Visual->Cap.IsValid()
			&& Visual->Cap->IsVisible() && Visual->CapTriangles > 0)
		{
			++Prop.VisibleHistoricalCaps;
		}
	}
	// The stale proxy samples B with bilinear filtering. A hard-zero ownership
	// texel adjacent to a positive stale texel can therefore submit a real stale
	// surface fragment inside current-owned space even though point diagnostics
	// report the zero texel. Record that render fragment explicitly.
	for (const FDarkwellSpatialObservationRecord& Historical : Prop.History.GetRecords())
	{
		if (Historical.bCurrentObservedLocation)
		{
			continue;
		}
		const FRecordVisual* Visual = Prop.Visuals.Find(Historical.Epoch);
		const FIntPoint Fine = Historical.SpatialMemory.GetSize()
			* Darkwell::MovingPropLab::PresentationSamples;
		if (!Visual || Visual->bPresentationRetired
			|| Visual->SubmittedPresentation.Num() != Fine.X * Fine.Y
			|| Visual->SuppressedByCurrentEvidence.Num() != Fine.X * Fine.Y)
		{
			continue;
		}
		for (int32 Y = 0; Y < Fine.Y; ++Y)
		{
			for (int32 X = 0; X < Fine.X; ++X)
			{
				const int32 Index = Y * Fine.X + X;
				if (!Visual->SuppressedByCurrentEvidence[Index])
				{
					continue;
				}
				bool bPositiveFilterNeighbour = false;
				for (int32 DY = -1; DY <= 1 && !bPositiveFilterNeighbour; ++DY)
				{
					for (int32 DX = -1; DX <= 1 && !bPositiveFilterNeighbour; ++DX)
					{
						const int32 NX = X + DX;
						const int32 NY = Y + DY;
						bPositiveFilterNeighbour = NX >= 0 && NY >= 0
							&& NX < Fine.X && NY < Fine.Y
							&& Visual->SubmittedPresentation[NY * Fine.X + NX].B > 0.0f;
					}
				}
				// The actual moving shader loads binary A after filtering B.
				if (!bPositiveFilterNeighbour || Visual->SubmittedPresentation[Index].A == 0.0f)
				{
					continue;
				}
				++Prop.HardOwnershipFilterLeak;
				++Prop.CurrentRenderContactStaleSurface;
				if (Prop.ResidualFragmentDiagnostics.Num() < 32)
				{
					Prop.ResidualFragmentDiagnostics.Add(FString::Printf(
						TEXT("epoch=%u primitive=ALL type=STALE_SURFACE texel=(%d,%d) material=M_ManualAccumulatedMemory ownership=SUPPRESSED clip=FILTER_LEAK overlap=BILINEAR_NEIGHBOUR"),
						Historical.Epoch, X, Y));
				}
			}
		}
	}
	for (const FVector2D Point : SamplePoints)
	{
		const int32 Surfaces = SurfaceContributorsAt(Point);
		const int32 Caps = CapContributorsAt(Point);
		Prop.MaxSurfaceContributors = FMath::Max(Prop.MaxSurfaceContributors, Surfaces);
		Prop.MaxCapContributors = FMath::Max(Prop.MaxCapContributors, Caps);
		// Surface samples represent horizontal/exterior mesh locations, while cap
		// samples represent the interior vertical cut plane. Their XY projection can
		// coincide without sharing a 3D render sample. Same-class overlaps are real;
		// current/newer ownership at a cap plane is rejected while building the cap.
		Prop.MaxTotalContributors = FMath::Max(Prop.MaxTotalContributors,
			FMath::Max(Surfaces, Caps));
	}
	auto IntervalsOverlap = [](const double AMin, const double AMax,
		const double BMin, const double BMax)
	{
		return FMath::Min(AMax, BMax) - FMath::Max(AMin, BMin)
			> UE_KINDA_SMALL_NUMBER;
	};
	auto IntervalsRenderContact = [](const double AMin, const double AMax,
		const double BMin, const double BMax)
	{
		return FMath::Min(AMax, BMax) + Darkwell::MovingPropLab::RenderOwnershipContactTolerance
			>= FMath::Max(AMin, BMin);
	};
	for (const FDarkwellSpatialObservationRecord& Historical : Prop.History.GetRecords())
	{
		if (Historical.bCurrentObservedLocation)
		{
			continue;
		}
		const FRecordVisual* Visual = Prop.Visuals.Find(Historical.Epoch);
		if (!Visual || Visual->bPresentationRetired)
		{
			continue;
		}
		const FIntPoint Coarse = Historical.SpatialMemory.GetSize();
		const FIntPoint Fine = Coarse * Darkwell::MovingPropLab::PresentationSamples;
		const FBox2D& Bounds = Historical.SpatialMemory.GetBounds();
		if (Fine.X > 0 && Fine.Y > 0 && Bounds.bIsValid)
		{
			const FVector2D Step = Bounds.GetSize() / FVector2D(Fine.X, Fine.Y);
			for (int32 Y = 0; Y < Fine.Y; ++Y)
			{
				for (int32 X = 0; X < Fine.X; ++X)
				{
					const int32 FineIndex = Y * Fine.X + X;
					const int32 CellIndex = (Y / Darkwell::MovingPropLab::PresentationSamples)
						* Coarse.X + X / Darkwell::MovingPropLab::PresentationSamples;
					const bool bSuppressed = Visual->SuppressedByCurrentEvidence.IsValidIndex(FineIndex)
						&& Visual->SuppressedByCurrentEvidence[FineIndex];
					if (bSuppressed || !Historical.SpatialMemory.GetCells().IsValidIndex(CellIndex)
						|| Historical.SpatialMemory.Presentation(CellIndex).B <= 0.0f)
					{
						continue;
					}
					const FVector2D Point = Bounds.Min
						+ Step * FVector2D(X + 0.5f, Y + 0.5f);
					TArray<FVector2D> CurrentIntervals;
					if (!CollectCurrentOwnedVerticalIntervals(Prop, Point, CurrentIntervals))
					{
						continue;
					}
					bool bOverlaps = false;
					for (const FPrimitiveGeometrySnapshot& OldGeometry : Visual->PartGeometry)
					{
						double OldMinZ = 0.0;
						double OldMaxZ = 0.0;
						if (!QueryVerticalInterval(OldGeometry, Point, OldMinZ, OldMaxZ))
						{
							continue;
						}
						for (const FVector2D CurrentInterval : CurrentIntervals)
						{
							if (IntervalsOverlap(OldMinZ, OldMaxZ,
								CurrentInterval.X, CurrentInterval.Y))
							{
								bOverlaps = true;
								if (Prop.Offending3DEpoch == 0)
								{
									Prop.Offending3DEpoch = Historical.Epoch;
									Prop.Offending3DPrimitive = OldGeometry.PrimitiveIndex;
									Prop.Offending3DWorldPosition = FVector(
										Point.X, Point.Y,
										FMath::Max(OldMinZ, CurrentInterval.X));
								}
								break;
							}
						}
						if (bOverlaps)
						{
							break;
						}
					}
					if (bOverlaps)
					{
						++Prop.Current3DOverlapStaleSurface;
						Prop.Max3DRenderOwnershipContributors = FMath::Max(
							Prop.Max3DRenderOwnershipContributors, 2);
					}
				}
			}
		}

		for (const FCapQuadSnapshot& Quad : Visual->CapQuads)
		{
			bool bQuadContactsCurrent = false;
			FVector ContactPoint = FVector::ZeroVector;
			if (const ADarkwellPropLabFurniture* CurrentActual = Prop.bExists
				? Prop.Actual.Get() : nullptr)
			{
				for (const FPrimitiveGeometrySnapshot& CurrentGeometry
					: ActualPartGeometry(*CurrentActual))
				{
					double Alpha0 = 0.0;
					double Alpha1 = 0.0;
					if (!ClipSegmentToGeometryProjection(CurrentGeometry,
						FVector2D(Quad.A), FVector2D(Quad.B),
						Darkwell::MovingPropLab::RenderOwnershipContactTolerance,
						Alpha0, Alpha1))
					{
						continue;
					}
					const double Alpha = FMath::Clamp((Alpha0 + Alpha1) * 0.5, 0.0, 1.0);
					const FVector Bottom = FMath::Lerp(Quad.A, Quad.B, Alpha);
					const FVector Top = FMath::Lerp(Quad.D, Quad.C, Alpha);
					const FVector2D Point(Bottom.X, Bottom.Y);
					TArray<FVector2D> CurrentIntervals;
					if (!CollectCurrentOwnedVerticalIntervals(Prop, Point, CurrentIntervals))
					{
						continue;
					}
					const double CapMinZ = FMath::Min(Bottom.Z, Top.Z);
					const double CapMaxZ = FMath::Max(Bottom.Z, Top.Z);
					for (const FVector2D CurrentInterval : CurrentIntervals)
					{
						if (IntervalsRenderContact(CapMinZ, CapMaxZ,
							CurrentInterval.X, CurrentInterval.Y))
						{
							bQuadContactsCurrent = true;
							ContactPoint = FVector(Point.X, Point.Y,
								FMath::Max(CapMinZ, CurrentInterval.X));
							break;
						}
					}
					if (bQuadContactsCurrent)
					{
						break;
					}
				}
			}
			for (const double Alpha : {0.0, 0.25, 0.5, 0.75, 1.0})
			{
				if (bQuadContactsCurrent)
				{
					break;
				}
				const FVector Bottom = FMath::Lerp(Quad.A, Quad.B, Alpha);
				const FVector Top = FMath::Lerp(Quad.D, Quad.C, Alpha);
				const FVector2D Point(Bottom.X, Bottom.Y);
				TArray<FVector2D> CurrentIntervals;
				if (!CollectCurrentOwnedVerticalIntervals(Prop, Point, CurrentIntervals))
				{
					continue;
				}
				const double CapMinZ = FMath::Min(Bottom.Z, Top.Z);
				const double CapMaxZ = FMath::Max(Bottom.Z, Top.Z);
				for (const FVector2D CurrentInterval : CurrentIntervals)
				{
					if (IntervalsRenderContact(CapMinZ, CapMaxZ,
						CurrentInterval.X, CurrentInterval.Y))
					{
						bQuadContactsCurrent = true;
						ContactPoint = FVector(Point.X, Point.Y,
							FMath::Max(CapMinZ, CurrentInterval.X));
						break;
					}
				}
				if (bQuadContactsCurrent)
				{
					break;
				}
			}
			if (bQuadContactsCurrent)
			{
				++Prop.CurrentRenderContactStaleCap;
				if (Prop.ResidualFragmentDiagnostics.Num() < 32)
				{
					const FVector Normal = FVector::CrossProduct(Quad.B - Quad.A, Quad.D - Quad.A).GetSafeNormal();
					Prop.ResidualFragmentDiagnostics.Add(FString::Printf(
						TEXT("epoch=%u primitive=%d type=STALE_CAP vertices=[%s|%s|%s|%s] normal=%s material=M_ManualStaleCutCap ownership=OLDER clip=KEPT overlap=CLOSED_CONTACT nearest=CURRENT separation<=%.3f world=%s"),
						Historical.Epoch, Quad.PrimitiveIndex, *Quad.A.ToCompactString(),
						*Quad.B.ToCompactString(), *Quad.C.ToCompactString(), *Quad.D.ToCompactString(),
						*Normal.ToCompactString(), Darkwell::MovingPropLab::RenderOwnershipContactTolerance,
						*ContactPoint.ToCompactString()));
				}
			}
			for (int32 Segment = 0; Segment < Darkwell::MovingPropLab::PresentationSamples;
				++Segment)
			{
				const double Alpha = (Segment + 0.5)
					/ Darkwell::MovingPropLab::PresentationSamples;
				const FVector Bottom = FMath::Lerp(Quad.A, Quad.B, Alpha);
				const FVector Top = FMath::Lerp(Quad.D, Quad.C, Alpha);
				const FVector2D Point(Bottom.X, Bottom.Y);
				TArray<FVector2D> CurrentIntervals;
				if (!CollectCurrentOwnedVerticalIntervals(Prop, Point, CurrentIntervals))
				{
					continue;
				}
				const double CapMinZ = FMath::Min(Bottom.Z, Top.Z);
				const double CapMaxZ = FMath::Max(Bottom.Z, Top.Z);
				const FVector2D* Overlap = CurrentIntervals.FindByPredicate(
					[&](const FVector2D Interval)
					{
						return IntervalsOverlap(CapMinZ, CapMaxZ, Interval.X, Interval.Y);
					});
				if (!Overlap)
				{
					continue;
				}
				++Prop.Current3DOverlapStaleCap;
				Prop.Max3DRenderOwnershipContributors = FMath::Max(
					Prop.Max3DRenderOwnershipContributors, 2);
				if (Prop.Offending3DEpoch == 0)
				{
					Prop.Offending3DEpoch = Historical.Epoch;
					Prop.Offending3DPrimitive = Quad.PrimitiveIndex;
					Prop.Offending3DWorldPosition = FVector(
						Point.X, Point.Y, FMath::Max(CapMinZ, Overlap->X));
				}
			}
		}
	}
	// SURFACE and CAP remain useful class-local projected diagnostics. TOTAL and
	// the compatibility overlap value now report the actual 3D ownership bound;
	// they must never be reconstructed as max(projected surface, projected cap).
	Prop.MaxTotalContributors = Prop.Max3DRenderOwnershipContributors;
	Prop.MaxOverlapContributors = Prop.Max3DRenderOwnershipContributors;
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
	int32 VisibleCaps = 0;
	int32 Discovered = 0;
	int32 Verified = 0;
	int32 Residual = 0;
	const TCHAR* ObservationState = Prop.ObservationState == EObservationState::ObservedArmed
		? TEXT("OBSERVED_ARMED")
		: (Prop.ObservationState == EObservationState::UnobservedSealed
			? TEXT("UNOBSERVED_SEALED") : TEXT("NEVER_OBSERVED"));
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
			const bool bCapVisible = Visual && Visual->Cap.IsValid()
				&& Visual->Cap->IsVisible() && Visual->CapTriangles > 0;
			int32 SupersededSamples = 0;
			if (Visual)
			{
				for (int32 Index = 0; Index < Visual->SuppressedByCurrentEvidence.Num(); ++Index)
				{
					SupersededSamples += Visual->SuppressedByCurrentEvidence[Index] ? 1 : 0;
				}
			}
			VisibleProxies += bVisible ? 1 : 0;
			VisibleCaps += bCapVisible ? 1 : 0;
			Historical.Add(FString::Printf(TEXT("%u@%.2f/proxy=%d/visible=%d/cap=%d/tri=%d/tex=%dx%d/gen=%u/superseded=%d"),
				Record.Epoch, Record.SnapshotTransform.Rotator().Yaw,
				Visual && Visual->Proxy.IsValid() ? Visual->Proxy->GetUniqueID() : 0,
				bVisible ? 1 : 0,
				Visual && Visual->Cap.IsValid() ? Visual->Cap->GetUniqueID() : 0,
				Visual ? Visual->CapTriangles : 0,
				Visual && Visual->Texture.IsValid() ? Visual->Texture->GetSizeX() : 0,
				Visual && Visual->Texture.IsValid() ? Visual->Texture->GetSizeY() : 0,
				Record.SpatialMemory.GetGeneration(), SupersededSamples));
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
		TEXT("ROTATION_FRAME live_epoch=%u stale_epochs=%d observation_episode=%d observation_state=%s actual=%d/%d actual_yaw=%.2f transform_rev=%llu coverage=%.4f coverage_valid=%d coverage_zero_reason=%s authority_rev=%llu coverage_rev=%llu coverage_transform_rev=%llu coverage_grid_rev=%llu grid_rev=%llu proxies=%d caps=%d stale=[%s] D=%d V=%d R=%d surface_contributors=%d cap_contributors=%d total_contributors=%d current_3d_overlap_stale_surface=%d current_3d_overlap_stale_cap=%d max_3d_render_ownership=%d offending_epoch=%u offending_primitive=%d offending_world=(%.3f,%.3f,%.3f) seal_count=%d"),
		CurrentEpoch, StaleEpochs, Prop.ObservationEpisode, ObservationState,
		VisibleActualComponents, ActualComponents,
		Prop.Actual.IsValid() ? Prop.Actual->GetActorRotation().Yaw : 0.0f,
		Prop.TransformRevision, Prop.LastLegalCoverageRatio, Prop.bLastCoverageValid ? 1 : 0,
		*Prop.LastCoverageZeroReason, Prop.CoverageAuthorityRevision, Prop.CoverageRevision,
		Prop.CoverageTransformRevision, Prop.CoverageGridRevision, Prop.GridRevision,
		VisibleProxies, VisibleCaps, *FString::Join(Historical, TEXT(";")),
		Discovered, Verified, Residual, Prop.MaxSurfaceContributors,
		Prop.MaxCapContributors, Prop.MaxTotalContributors,
		Prop.Current3DOverlapStaleSurface, Prop.Current3DOverlapStaleCap,
		Prop.Max3DRenderOwnershipContributors, Prop.Offending3DEpoch,
		Prop.Offending3DPrimitive, Prop.Offending3DWorldPosition.X,
		Prop.Offending3DWorldPosition.Y, Prop.Offending3DWorldPosition.Z,
		Prop.HiddenFreezeCount);
	if (Prop.CurrentRenderContactStaleSurface > 0
		|| Prop.CurrentRenderContactStaleCap > 0
		|| Prop.HardOwnershipFilterLeak > 0)
	{
		UE_LOG(LogDarkwellMovingPropLab, Warning,
			TEXT("ROTATION_RESIDUAL_FRAGMENTS surface=%d cap=%d filter=%d fragments=[%s]"),
			Prop.CurrentRenderContactStaleSurface,
			Prop.CurrentRenderContactStaleCap,
			Prop.HardOwnershipFilterLeak,
			*FString::Join(Prop.ResidualFragmentDiagnostics, TEXT("; ")));
	}
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
		const bool bTransformChanged = !Darkwell::MovingPropLab::TransformsMatch(
			Prop.LastPhysicalTransform, Transform);
		if (bTransformChanged)
		{
			++Prop.TransformRevision;
		}
		const FIntPoint CoverageSize = Bounds.bIsValid
			? FIntPoint(
				FMath::CeilToInt(Bounds.GetSize().X / Darkwell::MovingPropLab::CellSize),
				FMath::CeilToInt(Bounds.GetSize().Y / Darkwell::MovingPropLab::CellSize))
			: FIntPoint::ZeroValue;
		if (!Prop.LastCoverageBounds.bIsValid
			|| !Prop.LastCoverageBounds.Min.Equals(Bounds.Min, 0.01)
			|| !Prop.LastCoverageBounds.Max.Equals(Bounds.Max, 0.01)
			|| Prop.LastCoverageSize != CoverageSize)
		{
			++Prop.GridRevision;
			Prop.LastCoverageBounds = Bounds;
			Prop.LastCoverageSize = CoverageSize;
		}
		FCoverageSnapshot CoverageSnapshot = SampleConservativeCoverage(
			Bounds, Prop.TransformRevision, Prop.GridRevision);
		if (Prop.bInjectInvalidCoverageOnce)
		{
			Prop.bInjectInvalidCoverageOnce = false;
			CoverageSnapshot.bValid = false;
			CoverageSnapshot.ZeroReason = TEXT("TEST_INJECTED_INVALID");
		}
		TArray<float>& Coverage = CoverageSnapshot.Values;
		Prop.bLastCoverageValid = CoverageSnapshot.bValid;
		Prop.CoverageAuthorityRevision = CoverageSnapshot.AuthorityRevision;
		Prop.CoverageRevision = CoverageSnapshot.CoverageRevision;
		Prop.CoverageTransformRevision = CoverageSnapshot.TransformRevision;
		Prop.CoverageGridRevision = CoverageSnapshot.GridRevision;
		Prop.LastCoverageZeroReason = CoverageSnapshot.ZeroReason;
		const bool bAnyLegal = CoverageSnapshot.bValid
			&& Coverage.ContainsByPredicate([](const float Value)
			{
				return Value >= FDarkwellSpatialPropMemory::LegalCoverage;
			});
		int32 LegalCells = 0;
		for (const float Value : Coverage)
		{
			LegalCells += Value >= FDarkwellSpatialPropMemory::LegalCoverage ? 1 : 0;
		}
		Prop.LastLegalCoverageRatio = CoverageSnapshot.bValid && !Coverage.IsEmpty()
			? static_cast<float>(LegalCells) / Coverage.Num() : 0.0f;

		// Observation lifecycle is driven only by revision-matched authoritative
		// samples. Invalid/not-ready data may fail closed visually, but never writes
		// player knowledge, seals an epoch, or rearms a later seal.
		int32 CurrentIndex = Prop.History.GetCurrentIndex();
		if (CoverageSnapshot.bValid && CurrentIndex != INDEX_NONE)
		{
			if (bAnyLegal)
			{
				if (bTransformChanged)
				{
					Prop.History.RebaseCurrentObservedLocation(
						Transform, Bounds, Darkwell::MovingPropLab::CellSize);
					FDarkwellSpatialObservationRecord& Current =
						Prop.History.GetMutableRecords()[Prop.History.GetCurrentIndex()];
					FRecordVisual& Visual = Prop.Visuals.FindOrAdd(Current.Epoch);
					Visual.PartBounds = ActualPartBounds(*Actual);
					Visual.PartGeometry = ActualPartGeometry(*Actual);
				}
				Prop.ObservationState = EObservationState::ObservedArmed;
			}
			else if (bTransformChanged
				&& Prop.ObservationState == EObservationState::ObservedArmed)
			{
				FreezeCurrentForHiddenMotion(Prop, TEXT("VALID_OBSERVED_TO_UNOBSERVED"));
			}
		}
		CurrentIndex = Prop.History.GetCurrentIndex();
		if (CoverageSnapshot.bValid && CurrentIndex == INDEX_NONE && bAnyLegal)
		{
			const int32 NewIndex = Prop.History.BeginObservedLocation(
				Transform, Bounds, Darkwell::MovingPropLab::CellSize);
			if (NewIndex != INDEX_NONE)
			{
				++Prop.ObservationEpisode;
				Prop.ObservationState = EObservationState::ObservedArmed;
			}
		}
		CurrentIndex = Prop.History.GetCurrentIndex();
		if (CurrentIndex != INDEX_NONE)
		{
			FDarkwellSpatialObservationRecord& Current =
				Prop.History.GetMutableRecords()[CurrentIndex];
			if (CoverageSnapshot.bValid
				&& CoverageSnapshot.TransformRevision == Prop.TransformRevision
				&& CoverageSnapshot.GridRevision == Prop.GridRevision
				&& Coverage.Num() == Current.SpatialMemory.GetCells().Num())
			{
				Prop.History.AdvanceCurrent(DeltaSeconds, Coverage);
			}
			EnsureRecordVisual(Prop, Current);
			UpdateRecordTexture(Prop, Current);
			UpdateRecordCap(Prop, Current);
			if (FRecordVisual* CurrentVisual = Prop.Visuals.Find(Current.Epoch);
				CurrentVisual && CurrentVisual->Texture.IsValid())
			{
				Actual->BindSpatialState(CurrentVisual->Texture.Get(),
					Current.SpatialMemory.GetBounds());
				Actual->Memory->ApplySourceGeometryVisibility(true);
			}
		}
		else
		{
			Actual->Memory->ApplySourceGeometryVisibility(false);
		}
		if (!bTransformChanged || CoverageSnapshot.bValid)
		{
			Prop.LastPhysicalTransform = Transform;
		}
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
		EnsureRecordVisual(Prop, *Record);
		FRecordVisual* Visual = Prop.Visuals.Find(Epoch);
		if (!Visual) continue;
		if (Visual->bPresentationRetired)
		{
			UpdateHistoricalContributionExclusion(Prop, *Record);
			AdvanceFineHistory(Prop, *Record, DeltaSeconds);
			continue;
		}
		const FCoverageSnapshot HistoricalCoverage = SampleConservativeCoverage(
			Record->SpatialMemory.GetBounds(), Record->Epoch,
			Record->SpatialMemory.GetGeneration());
		if (HistoricalCoverage.bValid)
		{
			TArray<float> Coverage = HistoricalCoverage.Values;
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
		}
		UpdateHistoricalContributionExclusion(Prop, *Record);
		AdvanceFineHistory(Prop, *Record, DeltaSeconds);
		UpdateRecordTexture(Prop, *Record);
		UpdateRecordCap(Prop, *Record);
		if (IsHistoricalPresentationResolved(*Record, *Visual))
		{
			RetireHistoricalPresentation(Prop, *Visual);
		}
	}
	RefreshContributionDiagnostics(Prop);
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
	if (CurrentIndex == INDEX_NONE
		|| Prop.ObservationState != EObservationState::ObservedArmed)
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
	Prop.ObservationState = EObservationState::UnobservedSealed;
	++Prop.HiddenFreezeCount;
	if (FDarkwellSpatialObservationRecord* Historical = Prop.History.FindRecord(Epoch))
	{
		Historical->FineHistory.Initialize(Historical->SpatialMemory);
		EnsureRecordVisual(Prop, *Historical);
		if (const FRecordVisual* SealedVisual = Prop.Visuals.Find(Epoch))
		{
			const auto& Grid = Historical->FineHistory;
			const FIntPoint Size = Grid.GetSize();
			const FVector2D Step = Grid.GetBounds().GetSize() / FVector2D(Size.X, Size.Y);
			TBitArray<> Footprint(false, Size.X * Size.Y);
			for (int32 Y = 0; Y < Size.Y; ++Y) for (int32 X = 0; X < Size.X; ++X)
			{
				const FVector2D Min = Grid.GetBounds().Min + Step * FVector2D(X, Y);
				const FVector2D Corners[]{Min, Min + FVector2D(Step.X, 0), Min + Step, Min + FVector2D(0, Step.Y)};
				for (const auto& Geometry : SealedVisual->PartGeometry)
				{
					double A, B;
					bool Intersects = QueryVerticalInterval(Geometry, Min + Step * .5, A, B);
					for (int32 Edge = 0; Edge < 4 && !Intersects; ++Edge)
						Intersects |= ClipSegmentToGeometryProjection(Geometry, Corners[Edge], Corners[(Edge + 1) % 4], 0, A, B);
					Footprint[Y * Size.X + X] = Footprint[Y * Size.X + X] || Intersects;
				}
			}
			Historical->FineHistory.RestrictToRecordedGeometry(Footprint);
		}
		UpdateRecordTexture(Prop, *Historical);
		UpdateRecordCap(Prop, *Historical);
	}
	const FRecordVisual* Visual = Prop.Visuals.Find(Epoch);
	UE_LOG(LogDarkwellMovingPropLab, Display,
		TEXT("MOVING_RULES_STALE_SEALED id=%s epoch=%u episode=%d reason=%s freezes=%d transform_rev=%llu coverage_rev=%llu grid_rev=%llu proxy=%d texture=%dx%d uploads=%d"),
		*Prop.StableId.ToString(), Epoch, Prop.ObservationEpisode, Reason,
		Prop.HiddenFreezeCount, Prop.TransformRevision, Prop.CoverageRevision,
		Prop.GridRevision,
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
	if (!Record.bCurrentObservedLocation && Visual.bPresentationRetired)
	{
		return;
	}
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
			Visual.PartGeometry = ActualPartGeometry(*Actual);
		}
	}
	else if (Visual.PartGeometry.IsEmpty())
	{
		if (const ADarkwellPropLabFurniture* Actual = Prop.Actual.Get())
		{
			Visual.PartGeometry = ActualPartGeometry(*Actual);
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
			// Preserve frozen smooth RGB; load binary ownership A without filtering
			// at the final shader gate. No authoritative SpatialMemory cell changes.
			Presentation[Index].A = Visual->SuppressedByCurrentEvidence[Index] ? 0.0f : 1.0f;
		}
	}
	Visual->SubmittedPresentation = Presentation;
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
		nullptr, TEXT("/Game/Darkwell/Vision/PropLab/M_MovingAccumulatedMemory.M_MovingAccumulatedMemory"));
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
		Visual->Materials.Add(Material);
	}
}

TArray<FVector2D> ADarkwellMovingPropLabRoom::SubtractOwnedCapIntervals(
	FVector2D Candidate, TConstArrayView<FVector2D> Owned)
{
	TArray<FVector2D> Remaining{Candidate};
	for (const FVector2D Newer : Owned)
	{
		TArray<FVector2D> Next;
		const double ClipMin = Newer.X - Darkwell::MovingPropLab::RenderOwnershipClipClearance;
		const double ClipMax = Newer.Y + Darkwell::MovingPropLab::RenderOwnershipClipClearance;
		for (const FVector2D Interval : Remaining)
		{
			if (ClipMax < Interval.X || ClipMin > Interval.Y) { Next.Add(Interval); continue; }
			if (ClipMin > Interval.X + UE_KINDA_SMALL_NUMBER)
				Next.Add(FVector2D(Interval.X, FMath::Min(ClipMin, Interval.Y)));
			if (ClipMax < Interval.Y - UE_KINDA_SMALL_NUMBER)
				Next.Add(FVector2D(FMath::Max(ClipMax, Interval.X), Interval.Y));
		}
		Remaining = MoveTemp(Next);
	}
	return Remaining;
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
		Visual->CapSamplePoints.Reset();
		Visual->CapQuads.Reset();
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
	Signature = (Signature ^ Prop.TransformRevision) * 1099511628211ull;
	for (int32 Index = 0; Index < Visual->SuppressedByCurrentEvidence.Num(); ++Index)
	{
		Signature = (Signature ^ (Visual->SuppressedByCurrentEvidence[Index] ? 1ull : 0ull))
			* 1099511628211ull;
	}
	for (const FDarkwellSpatialObservationRecord& Candidate : Prop.History.GetRecords())
	{
		if (Candidate.Epoch <= Record.Epoch)
		{
			continue;
		}
		Signature = (Signature ^ Candidate.Epoch) * 1099511628211ull;
		const FRecordVisual* CandidateVisual = Prop.Visuals.Find(Candidate.Epoch);
		Signature = (Signature ^ (CandidateVisual && CandidateVisual->bPresentationRetired
			? 1ull : 0ull)) * 1099511628211ull;
		if (CandidateVisual)
		{
			for (int32 SuppressedIndex = 0;
				SuppressedIndex < CandidateVisual->SuppressedByCurrentEvidence.Num();
				++SuppressedIndex)
			{
				Signature = (Signature
					^ (CandidateVisual->SuppressedByCurrentEvidence[SuppressedIndex] ? 1ull : 0ull))
					* 1099511628211ull;
			}
		}
		for (const FDarkwellSpatialPropMemory::FCell& CandidateCell
			: Candidate.SpatialMemory.GetCells())
		{
			const uint64 Renderable = Candidate.bCurrentObservedLocation
				? (CandidateCell.DiscoveredPresent > 0.0f
					&& CandidateCell.AppearanceBlend > 0.0f ? 1ull : 0ull)
				: (CandidateCell.StaleOpacity > 0.0f ? 1ull : 0ull);
			Signature = (Signature ^ Renderable) * 1099511628211ull;
		}
	}
	if (Signature == Visual->CapSignature)
	{
		return;
	}
	Visual->CapSignature = Signature;
	FDynamicMesh3 Mesh;
	Visual->CapExpected = Visual->CapGenerated = Visual->CapClipped = 0;
	Visual->MissingHistoricalCuts = 0;
	Visual->CapSamplePoints.Reset();
	Visual->CapQuads.Reset();
	const FBox2D& Bounds = Record.SpatialMemory.GetBounds();
	const FVector2D Step = Bounds.GetSize() / FVector2D(Size.X, Size.Y);
	const FVector Origin = GetActorLocation();
	auto IsSubmitted = [&](const int32 X, const int32 Y)
	{
		if (X < 0 || Y < 0 || X >= Size.X || Y >= Size.Y) return false;
		const auto& Cell = Cells[Y * Size.X + X];
		return bPresent ? Cell.DiscoveredPresent > 0
			: Cell.InitialRemembered > 0 && Cell.VerifiedEmpty == 0;
	};
	auto IsCut = [&](const int32 X, const int32 Y)
	{
		if (X < 0 || Y < 0 || X >= Size.X || Y >= Size.Y) return false;
		const auto& Cell = Cells[Y * Size.X + X];
		return bPresent ? Cell.DiscoveredPresent == 0
			: Cell.InitialRemembered == 0 || Cell.VerifiedEmpty > 0;
	};
	auto AppendQuad = [&](const FVector& A, const FVector& B, const FVector& C, const FVector& D,
		const int32 PrimitiveIndex)
	{
		const FVector Center = (A + B + C + D) * 0.25f;
		const int32 IA = Mesh.AppendVertex(FVector3d(A - Origin));
		const int32 IB = Mesh.AppendVertex(FVector3d(B - Origin));
		const int32 IC = Mesh.AppendVertex(FVector3d(C - Origin));
		const int32 ID = Mesh.AppendVertex(FVector3d(D - Origin));
		Mesh.AppendTriangle(IA, IB, IC);
		Mesh.AppendTriangle(IA, IC, ID);
		Visual->CapSamplePoints.Add(FVector2D(Center.X, Center.Y));
		Visual->CapQuads.Add({A, B, C, D, PrimitiveIndex});
	};
	const TArray<FPrimitiveGeometrySnapshot> NewerGeometry = Record.bCurrentObservedLocation
		? TArray<FPrimitiveGeometrySnapshot>()
		: CollectNewerGeometrySnapshots(Prop, Record.Epoch);
	auto AddOwnershipGridBreakpoints = [&](const FVector2D SegmentStart,
		const FVector2D SegmentEnd, TArray<double>& Breakpoints)
	{
		const FVector2D Delta = SegmentEnd - SegmentStart;
		for (const FDarkwellSpatialObservationRecord& Candidate : Prop.History.GetRecords())
		{
			if (Candidate.Epoch < Record.Epoch)
			{
				continue;
			}
			const FBox2D& CandidateBounds = Candidate.SpatialMemory.GetBounds();
			FIntPoint CandidateSize = Candidate.SpatialMemory.GetSize();
			if (!Candidate.bCurrentObservedLocation)
			{
				CandidateSize *= Darkwell::MovingPropLab::PresentationSamples;
			}
			if (!CandidateBounds.bIsValid || CandidateSize.X <= 0 || CandidateSize.Y <= 0)
			{
				continue;
			}
			if (FMath::Abs(Delta.X) > UE_DOUBLE_SMALL_NUMBER)
			{
				for (int32 X = 0; X <= CandidateSize.X; ++X)
				{
					const double Coordinate = FMath::Lerp(
						CandidateBounds.Min.X, CandidateBounds.Max.X,
						static_cast<double>(X) / CandidateSize.X);
					const double Alpha = (Coordinate - SegmentStart.X) / Delta.X;
					if (Alpha > 0.0 && Alpha < 1.0)
					{
						Breakpoints.Add(Alpha);
						const double Guard = Darkwell::MovingPropLab::RenderOwnershipClipPrecisionMargin / FMath::Abs(Delta.X);
						Breakpoints.Add(FMath::Clamp(Alpha-Guard,0.0,1.0));
						Breakpoints.Add(FMath::Clamp(Alpha+Guard,0.0,1.0));
					}
				}
			}
			if (FMath::Abs(Delta.Y) > UE_DOUBLE_SMALL_NUMBER)
			{
				for (int32 Y = 0; Y <= CandidateSize.Y; ++Y)
				{
					const double Coordinate = FMath::Lerp(
						CandidateBounds.Min.Y, CandidateBounds.Max.Y,
						static_cast<double>(Y) / CandidateSize.Y);
					const double Alpha = (Coordinate - SegmentStart.Y) / Delta.Y;
					if (Alpha > 0.0 && Alpha < 1.0)
					{
						Breakpoints.Add(Alpha);
						const double Guard = Darkwell::MovingPropLab::RenderOwnershipClipPrecisionMargin / FMath::Abs(Delta.Y);
						Breakpoints.Add(FMath::Clamp(Alpha-Guard,0.0,1.0));
						Breakpoints.Add(FMath::Clamp(Alpha+Guard,0.0,1.0));
					}
				}
			}
		}
	};
	auto AddQuad = [&](const FVector& A, const FVector& B, const FVector& C, const FVector& D,
		const int32 PrimitiveIndex, const FVector2D RetainedSide)
	{
		++Visual->CapGenerated;
		for (int32 Segment = 0; Segment < Darkwell::MovingPropLab::PresentationSamples;
			++Segment)
		{
			const double Alpha0 = static_cast<double>(Segment)
				/ Darkwell::MovingPropLab::PresentationSamples;
			const double Alpha1 = static_cast<double>(Segment + 1)
				/ Darkwell::MovingPropLab::PresentationSamples;
			const FVector Bottom0 = FMath::Lerp(A, B, Alpha0);
			const FVector Bottom1 = FMath::Lerp(A, B, Alpha1);
			const FVector Top1 = FMath::Lerp(D, C, Alpha1);
			const FVector Top0 = FMath::Lerp(D, C, Alpha0);
			TArray<double> Breakpoints{0.0, 1.0};
			// Clip to the original transformed primitive too. A midpoint inside
			// its OBB does not imply both endpoints are inside (especially yaw).
			if (Visual->PartGeometry.IsValidIndex(PrimitiveIndex))
			{
				double Entry, Exit;
				if (!ClipSegmentToGeometryProjection(Visual->PartGeometry[PrimitiveIndex],
					FVector2D(Bottom0), FVector2D(Bottom1), 0.0, Entry, Exit)) continue;
				Breakpoints.Add(FMath::Clamp(Entry, 0.0, 1.0));
				Breakpoints.Add(FMath::Clamp(Exit, 0.0, 1.0));
			}
			if (!Record.bCurrentObservedLocation)
			{
				for (const FPrimitiveGeometrySnapshot& Geometry : NewerGeometry)
				{
					double Entry = 0.0;
					double Exit = 0.0;
					if (ClipSegmentToGeometryProjection(Geometry,
						FVector2D(Bottom0), FVector2D(Bottom1),
						Darkwell::MovingPropLab::RenderOwnershipClipClearance,
						Entry, Exit))
					{
						Breakpoints.Add(FMath::Clamp(Entry, 0.0, 1.0));
						Breakpoints.Add(FMath::Clamp(Exit, 0.0, 1.0));
					}
				}
				AddOwnershipGridBreakpoints(FVector2D(Bottom0), FVector2D(Bottom1), Breakpoints);
			}
			Breakpoints.Sort();
			for (int32 Index = Breakpoints.Num() - 1; Index > 0; --Index)
			{
				if (FMath::IsNearlyEqual(Breakpoints[Index], Breakpoints[Index - 1], 1.0e-7))
				{
					Breakpoints.RemoveAt(Index);
				}
			}
			for (int32 Span = 0; Span + 1 < Breakpoints.Num(); ++Span)
			{
				const double Span0 = Breakpoints[Span];
				const double Span1 = Breakpoints[Span + 1];
				if (Span1 - Span0 <= 1.0e-7)
				{
					continue;
				}
				const FVector SpanBottom0 = FMath::Lerp(Bottom0, Bottom1, Span0);
				const FVector SpanBottom1 = FMath::Lerp(Bottom0, Bottom1, Span1);
				const FVector SpanTop0 = FMath::Lerp(Top0, Top1, Span0);
				const FVector SpanTop1 = FMath::Lerp(Top0, Top1, Span1);
				const FVector2D Point(FMath::Lerp(
					FVector2D(SpanBottom0), FVector2D(SpanBottom1), 0.5));
				double OldMinZ = FMath::Min(SpanBottom0.Z, SpanTop0.Z);
				double OldMaxZ = FMath::Max(SpanBottom0.Z, SpanTop0.Z);
				if (Visual->PartGeometry.IsValidIndex(PrimitiveIndex))
				{
					double GeometryMinZ = 0.0;
					double GeometryMaxZ = 0.0;
					if (!QueryVerticalInterval(Visual->PartGeometry[PrimitiveIndex],
						Point, GeometryMinZ, GeometryMaxZ))
					{
						continue;
					}
					OldMinZ = FMath::Max(OldMinZ, GeometryMinZ);
					OldMaxZ = FMath::Min(OldMaxZ, GeometryMaxZ);
				}
				if (OldMaxZ - OldMinZ <= UE_KINDA_SMALL_NUMBER)
				{
					continue;
				}
				TArray<FVector2D> Remaining{FVector2D(OldMinZ, OldMaxZ)};
				if (!Record.bCurrentObservedLocation)
				{
					// Match the final surface ownership domain, not just exact OBBs.
					// A conservative fine texel can be wholly owned even when its
					// center lies just outside the newer mesh. Leaving a cap in that
					// texel creates a detached strip with no remaining historical skin.
					// This is post-candidate clipping only; never a new cut or V write.
					const FIntPoint Fine = Size * Darkwell::MovingPropLab::PresentationSamples;
					const FVector2D Support = Point + RetainedSide
						* Darkwell::MovingPropLab::RenderOwnershipClipPrecisionMargin;
					const FVector2D UV = (Support - Bounds.Min) / Bounds.GetSize();
					const int32 FX = FMath::Clamp(FMath::FloorToInt(UV.X * Fine.X), 0, Fine.X - 1);
					const int32 FY = FMath::Clamp(FMath::FloorToInt(UV.Y * Fine.Y), 0, Fine.Y - 1);
					if (Visual->SuppressedByCurrentEvidence.IsValidIndex(FY * Fine.X + FX)
						&& Visual->SuppressedByCurrentEvidence[FY * Fine.X + FX])
					{
						++Visual->CapClipped;
						continue;
					}
					TArray<FVector2D> NewerIntervals;
					CollectNewerOwnedVerticalIntervals(Prop, Record.Epoch, Point, NewerIntervals,
						Darkwell::MovingPropLab::RenderOwnershipClipClearance);
					// Resolve the closed ownership of an exact grid endpoint only in
					// the existing 0.001-cm precision strip, not the whole adjacent span.
					if (FVector2D::Distance(FVector2D(SpanBottom0), FVector2D(SpanBottom1)) <=
						2.01 * Darkwell::MovingPropLab::RenderOwnershipClipPrecisionMargin)
					{
						for (const FVector Endpoint : {SpanBottom0, SpanBottom1})
						{
							TArray<FVector2D> EndIntervals;
							CollectNewerOwnedVerticalIntervals(Prop, Record.Epoch, FVector2D(Endpoint), EndIntervals,
								Darkwell::MovingPropLab::RenderOwnershipClipClearance);
							NewerIntervals.Append(EndIntervals);
						}
					}
					Remaining = SubtractOwnedCapIntervals(FVector2D(OldMinZ, OldMaxZ), NewerIntervals);
					if (Remaining.Num() != 1 || Remaining[0] != FVector2D(OldMinZ, OldMaxZ))
					{
						++Visual->CapClipped;
					}
				}
				for (const FVector2D Interval : Remaining)
				{
					AppendQuad(
						FVector(SpanBottom0.X, SpanBottom0.Y, Interval.X),
						FVector(SpanBottom1.X, SpanBottom1.Y, Interval.X),
						FVector(SpanTop1.X, SpanTop1.Y, Interval.Y),
						FVector(SpanTop0.X, SpanTop0.Y, Interval.Y), PrimitiveIndex);
				}
			}
		}
	};
	auto Vertical = [&](const double X, const double Y0, const double Y1, const double RetainedX)
	{
		for (int32 PrimitiveIndex = 0; PrimitiveIndex < Visual->PartBounds.Num(); ++PrimitiveIndex)
		{
			const FBox& Part = Visual->PartBounds[PrimitiveIndex];
			if (X < Part.Min.X - UE_KINDA_SMALL_NUMBER || X > Part.Max.X + UE_KINDA_SMALL_NUMBER) continue;
			const double From = FMath::Max(Y0, Part.Min.Y);
			const double To = FMath::Min(Y1, Part.Max.Y);
			if (To - From > UE_KINDA_SMALL_NUMBER)
			{
				AddQuad(FVector(X, From, Part.Min.Z), FVector(X, To, Part.Min.Z),
					FVector(X, To, Part.Max.Z), FVector(X, From, Part.Max.Z), PrimitiveIndex, FVector2D(RetainedX, 0));
			}
		}
	};
	auto Horizontal = [&](const double Y, const double X0, const double X1, const double RetainedY)
	{
		for (int32 PrimitiveIndex = 0; PrimitiveIndex < Visual->PartBounds.Num(); ++PrimitiveIndex)
		{
			const FBox& Part = Visual->PartBounds[PrimitiveIndex];
			if (Y < Part.Min.Y - UE_KINDA_SMALL_NUMBER || Y > Part.Max.Y + UE_KINDA_SMALL_NUMBER) continue;
			const double From = FMath::Max(X0, Part.Min.X);
			const double To = FMath::Min(X1, Part.Max.X);
			if (To - From > UE_KINDA_SMALL_NUMBER)
			{
				AddQuad(FVector(From, Y, Part.Min.Z), FVector(To, Y, Part.Min.Z),
					FVector(To, Y, Part.Max.Z), FVector(From, Y, Part.Max.Z), PrimitiveIndex, FVector2D(0, RetainedY));
			}
		}
	};
	for (int32 Y = 0; Y < Size.Y; ++Y)
	{
		for (int32 X = 0; X < Size.X; ++X)
		{
			// Independent positive diagnostic: a sealed partial discovery is still
			// a real exposed history boundary, even without VerifiedEmpty evidence.
			const auto& Cell = Cells[Y * Size.X + X];
			if (bAbsent && Cell.InitialRemembered > 0 && Cell.VerifiedEmpty == 0)
			{
				for (const FIntPoint Offset : {FIntPoint(-1,0), FIntPoint(1,0), FIntPoint(0,-1), FIntPoint(0,1)})
				{
					const int32 NX = X + Offset.X, NY = Y + Offset.Y;
					if (NX < 0 || NY < 0 || NX >= Size.X || NY >= Size.Y) continue;
					const auto& Neighbor = Cells[NY * Size.X + NX];
					if (Neighbor.InitialRemembered == 0 || Neighbor.VerifiedEmpty > 0)
					{
						++Visual->CapExpected;
						Visual->MissingHistoricalCuts += !IsSubmitted(X, Y) || !IsCut(NX, NY);
					}
				}
			}
			if (!IsSubmitted(X, Y)) continue;
			const double X0 = Bounds.Min.X + X * Step.X;
			const double X1 = X0 + Step.X;
			const double Y0 = Bounds.Min.Y + Y * Step.Y;
			const double Y1 = Y0 + Step.Y;
			if (IsCut(X - 1, Y)) Vertical(X0, Y0, Y1, 1);
			if (IsCut(X + 1, Y)) Vertical(X1, Y0, Y1, -1);
			if (IsCut(X, Y - 1)) Horizontal(Y0, X0, X1, 1);
			if (IsCut(X, Y + 1)) Horizontal(Y1, X0, X1, -1);
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
		? FString::Printf(TEXT("\nLIVE %d | STALE %d | PROXIES %d | CAPS %d\nSURFACE %d | CAP %d | TOTAL %d\nCOVERAGE %.0f%% VALID %d %s | SEALS %d"),
			GetCurrentEpochCountForTesting(Darkwell::MovingPropLab::RotateId),
			GetStaleEpochCountForTesting(Darkwell::MovingPropLab::RotateId),
			GetVisibleHistoricalProxyCountForTesting(Darkwell::MovingPropLab::RotateId),
			GetVisibleHistoricalCapCountForTesting(Darkwell::MovingPropLab::RotateId),
			GetMaxSurfaceContributorsForTesting(Darkwell::MovingPropLab::RotateId),
			GetMaxCapContributorsForTesting(Darkwell::MovingPropLab::RotateId),
			GetMaxTotalContributorsForTesting(Darkwell::MovingPropLab::RotateId),
			GetLastLegalCoverageRatioForTesting(Darkwell::MovingPropLab::RotateId) * 100.0f,
			IsLastCoverageValidForTesting(Darkwell::MovingPropLab::RotateId) ? 1 : 0,
			*GetLastCoverageZeroReasonForTesting(Darkwell::MovingPropLab::RotateId),
			GetSealCountForTesting(Darkwell::MovingPropLab::RotateId))
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

int32 ADarkwellMovingPropLabRoom::GetVisibleHistoricalCapCountForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? Prop->VisibleHistoricalCaps : 0;
}

int32 ADarkwellMovingPropLabRoom::GetHistoricalPresentationResourceCountForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	int32 Count = 0;
	if (Prop)
	{
		for (const FDarkwellSpatialObservationRecord& Record : Prop->History.GetRecords())
		{
			if (Record.bCurrentObservedLocation) continue;
			const FRecordVisual* Visual = Prop->Visuals.Find(Record.Epoch);
			Count += Visual && (Visual->Proxy.IsValid() || Visual->Cap.IsValid()
				|| Visual->Texture.IsValid() || !Visual->Materials.IsEmpty()) ? 1 : 0;
		}
	}
	return Count;
}

int32 ADarkwellMovingPropLabRoom::GetMaxSurfaceContributorsForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? Prop->MaxSurfaceContributors : 0;
}

int32 ADarkwellMovingPropLabRoom::GetMaxCapContributorsForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? Prop->MaxCapContributors : 0;
}

int32 ADarkwellMovingPropLabRoom::GetMaxTotalContributorsForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? Prop->MaxTotalContributors : 0;
}

int32 ADarkwellMovingPropLabRoom::GetCurrent3DOverlapStaleSurfaceForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? Prop->Current3DOverlapStaleSurface : 0;
}

int32 ADarkwellMovingPropLabRoom::GetCurrent3DOverlapStaleCapForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? Prop->Current3DOverlapStaleCap : 0;
}

int32 ADarkwellMovingPropLabRoom::GetMax3DRenderOwnershipContributorsForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? Prop->Max3DRenderOwnershipContributors : 0;
}

int32 ADarkwellMovingPropLabRoom::GetCurrentRenderContactStaleSurfaceForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? Prop->CurrentRenderContactStaleSurface : 0;
}

int32 ADarkwellMovingPropLabRoom::GetCurrentRenderContactStaleCapForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? Prop->CurrentRenderContactStaleCap : 0;
}

int32 ADarkwellMovingPropLabRoom::GetHardOwnershipFilterLeakForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? Prop->HardOwnershipFilterLeak : 0;
}

FString ADarkwellMovingPropLabRoom::Get3DOwnershipTelemetryForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	if (!Prop)
	{
		return TEXT("CURRENT_3D_OVERLAP_STALE_SURFACE=0 CURRENT_3D_OVERLAP_STALE_CAP=0 MAX_3D_RENDER_OWNERSHIP=0");
	}
	return FString::Printf(
		TEXT("CURRENT_3D_OVERLAP_STALE_SURFACE=%d CURRENT_3D_OVERLAP_STALE_CAP=%d MAX_3D_RENDER_OWNERSHIP=%d CURRENT_RENDER_CONTACT_STALE_SURFACE=%d CURRENT_RENDER_CONTACT_STALE_CAP=%d HARD_OWNERSHIP_FILTER_LEAK=%d OFFENDING_EPOCH=%u OFFENDING_PRIMITIVE=%d OFFENDING_WORLD=(%.3f,%.3f,%.3f) CURRENT_OBSERVATION_EPOCH=%d CURRENT_TRANSFORM=%s"),
		Prop->Current3DOverlapStaleSurface,
		Prop->Current3DOverlapStaleCap,
		Prop->Max3DRenderOwnershipContributors,
		Prop->CurrentRenderContactStaleSurface,
		Prop->CurrentRenderContactStaleCap,
		Prop->HardOwnershipFilterLeak,
		Prop->Offending3DEpoch,
		Prop->Offending3DPrimitive,
		Prop->Offending3DWorldPosition.X,
		Prop->Offending3DWorldPosition.Y,
		Prop->Offending3DWorldPosition.Z,
		Prop->ObservationEpisode,
		Prop->Actual.IsValid() ? *Prop->Actual->GetActorTransform().ToHumanReadableString()
			: TEXT("NONE"));
}

FString ADarkwellMovingPropLabRoom::GetFalseOccupiedHistoryTelemetryForTesting(FName StableId) const
{
	FString Result;
	const FTrackedProp* Prop = Tracked.Find(StableId);
	if (!Prop) return Result;
	for (const auto& Record : Prop->History.GetRecords())
	{
		if (Record.bCurrentObservedLocation) continue;
		const FRecordVisual* Visual = Prop->Visuals.Find(Record.Epoch);
		if (!Visual || Visual->bPresentationRetired) continue;
		const auto Coverage = SampleConservativeCoverage(Record.SpatialMemory.GetBounds(), Record.Epoch, Record.SpatialMemory.GetGeneration());
		if (!Coverage.bValid) continue;
		const FIntPoint Size = Record.SpatialMemory.GetSize();
		const FBox2D& Bounds = Record.SpatialMemory.GetBounds();
		for (int32 I = 0; I < Coverage.Values.Num(); ++I)
		{
			const auto& Cell = Record.SpatialMemory.GetCells()[I];
			if (Coverage.Values[I] < FDarkwellSpatialPropMemory::LegalCoverage || Cell.StaleOpacity <= 0) continue;
			const FVector2D Point = Bounds.Min + Bounds.GetSize() * FVector2D((I % Size.X + .5) / Size.X, (I / Size.X + .5) / Size.Y);
			if (!IsOccupiedByActual(Point, NAME_None)) continue;
			bool bOccupied = false;
			for (const auto& Pair : Tracked)
				if (Pair.Value.bExists && Pair.Value.Actual.IsValid())
					for (const auto& Geometry : ActualPartGeometry(*Pair.Value.Actual.Get()))
					{
						double MinZ, MaxZ;
						bOccupied |= QueryVerticalInterval(Geometry, Point, MinZ, MaxZ);
					}
			if (bOccupied) continue;
			for (const auto& Geometry : Visual->PartGeometry)
			{
				double MinZ, MaxZ;
				if (!QueryVerticalInterval(Geometry, Point, MinZ, MaxZ)) continue;
				Result += FString::Printf(TEXT("epoch=%u primitive=%d type=STALE_SURFACE world=(%.4f,%.4f,%.4f..%.4f) legal=%.3f D=%.3f V=%.3f R=%.3f opacity=%.3f occupancy=AABB_FALSE_POSITIVE newer3D=0 material=M_ManualAccumulatedMemory component=%s retired=0;\n"),
					Record.Epoch, Geometry.PrimitiveIndex, Point.X, Point.Y, MinZ, MaxZ, Coverage.Values[I],
					Cell.DiscoveredPresent, Cell.VerifiedEmpty, Cell.RemainingStale, Cell.StaleOpacity, *GetNameSafe(Visual->Proxy.Get()));
			}
		}
	}
	return Result;
}

int32 ADarkwellMovingPropLabRoom::GetFalseOccupiedHistoryCountForTesting(FName StableId) const
{
	TArray<FString> Lines;
	GetFalseOccupiedHistoryTelemetryForTesting(StableId).ParseIntoArrayLines(Lines);
	return Lines.Num();
}

int32 ADarkwellMovingPropLabRoom::GetMissingHistoricalCutCountForTesting(FName StableId) const
{
	int32 Count = 0;
	if (const FTrackedProp* Prop = Tracked.Find(StableId))
		for (const auto& Pair : Prop->Visuals) Count += Pair.Value.MissingHistoricalCuts;
	return Count;
}

int32 ADarkwellMovingPropLabRoom::GetCapVerticesOutsideSourceForTesting(FName StableId) const
{
	int32 Count = 0;
	if (const FTrackedProp* Prop = Tracked.Find(StableId))
		for (const auto& Pair : Prop->Visuals)
			for (const FCapQuadSnapshot& Quad : Pair.Value.CapQuads)
			{
				if (!Pair.Value.PartGeometry.IsValidIndex(Quad.PrimitiveIndex)) continue;
				const auto& Part = Pair.Value.PartGeometry[Quad.PrimitiveIndex];
				for (const FVector Point : {Quad.A, Quad.B, Quad.C, Quad.D})
					Count += !Part.LocalBounds.ExpandBy(0.0001).IsInsideOrOn(
						Part.WorldTransform.InverseTransformPosition(Point));
			}
	return Count;
}

FString ADarkwellMovingPropLabRoom::GetCapLifecycleTelemetryForTesting(FName StableId) const
{
	FString Result;
	if (const FTrackedProp* Prop = Tracked.Find(StableId))
		for (const auto& Pair : Prop->Visuals)
		{
			const FRecordVisual& V = Pair.Value;
			Result += FString::Printf(TEXT("epoch=%u CAP_EXPECTED=%d CAP_GENERATED=%d CAP_CLIPPED=%d CAP_RENDERED=%d missing_candidate=%d retired=%d component=%s; "),
				Pair.Key, V.CapExpected, V.CapGenerated, V.CapClipped,
				V.Cap.IsValid() && V.Cap->IsVisible() ? V.CapTriangles : 0,
				V.MissingHistoricalCuts, V.bPresentationRetired, *GetNameSafe(V.Cap.Get()));
		}
	return Result;
}

FString ADarkwellMovingPropLabRoom::GetResidualFragmentTelemetryForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	if (!Prop) return TEXT("NO_TRACKED_PROP");
	FString Result = FString::Join(Prop->ResidualFragmentDiagnostics, TEXT("; "));
	for (const auto& Pair : Prop->Visuals)
	{
		const auto* Record = Prop->History.FindRecord(Pair.Key);
		if (!Record || Record->bCurrentObservedLocation || Pair.Value.bPresentationRetired) continue;
		const FRecordVisual& Visual = Pair.Value;
		const FIntPoint Coarse = Record->SpatialMemory.GetSize();
		const int32 Samples = Darkwell::MovingPropLab::PresentationSamples;
		const FIntPoint Fine = Coarse * Samples;
		const FBox2D& Bounds = Record->SpatialMemory.GetBounds();
		const FCoverageSnapshot Coverage = SampleConservativeCoverage(Bounds, Record->Epoch, Record->SpatialMemory.GetGeneration());
		int32 Reported = 0;
		for (int32 I = 0; I < Visual.SubmittedPresentation.Num() && Reported < 32; ++I)
		{
			const FLinearColor Submitted = Visual.SubmittedPresentation[I];
			if (Submitted.B <= 0.0f || Submitted.A == 0.0f) continue;
			const int32 CellIndex = (I / Fine.X / Samples) * Coarse.X + I % Fine.X / Samples;
			const FVector2D Point = Bounds.Min + Bounds.GetSize() * FVector2D((I % Fine.X + .5) / Fine.X, (I / Fine.X + .5) / Fine.Y);
			for (const auto& Geometry : Visual.PartGeometry)
			{
				double MinZ, MaxZ;
				if (!QueryVerticalInterval(Geometry, Point, MinZ, MaxZ)) continue;
				const auto& Cell = Record->SpatialMemory.GetCells()[CellIndex];
				Result += FString::Printf(TEXT("\nepoch=%u primitive=%d type=STALE_SURFACE world=(%.4f,%.4f,%.4f..%.4f) legal=%.3f D=%.3f V=%.3f R=%.3f smooth=%.3f hard=%.0f material=M_MovingAccumulatedMemory component=%s retired=0"),
					Pair.Key, Geometry.PrimitiveIndex, Point.X, Point.Y, MinZ, MaxZ,
					Coverage.Values.IsValidIndex(CellIndex) ? Coverage.Values[CellIndex] : 0.0f,
					Cell.DiscoveredPresent, Cell.VerifiedEmpty, Cell.RemainingStale, Submitted.B, Submitted.A, *GetNameSafe(Visual.Proxy.Get()));
				++Reported;
				break;
			}
		}
		if (!Visual.Cap.IsValid() || !Visual.Cap->IsVisible()) continue;
		for (const auto& Q : Pair.Value.CapQuads)
		{
			const FVector C = (Q.A+Q.B+Q.C+Q.D)*.25;
			Result += FString::Printf(TEXT("\nepoch=%u primitive=%d type=STALE_CAP A=(%.4f,%.4f,%.4f) B=(%.4f,%.4f,%.4f) top=%.4f actualOccupied=%d currentObserved=%d"),
				Pair.Key,Q.PrimitiveIndex,Q.A.X,Q.A.Y,Q.A.Z,Q.B.X,Q.B.Y,Q.B.Z,Q.C.Z,
				IsOccupiedByActual(FVector2D(C),NAME_None),HasCurrentObservedContributionAt(*Prop,FVector2D(C)));
		}
	}
	return Result;
}

int32 ADarkwellMovingPropLabRoom::GetNewestHistoricalDiscoveredCellCountForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	const FDarkwellSpatialObservationRecord* Newest = nullptr;
	if (Prop)
	{
		for (const FDarkwellSpatialObservationRecord& Record : Prop->History.GetRecords())
		{
			if (!Record.bCurrentObservedLocation && (!Newest || Record.Epoch > Newest->Epoch))
			{
				Newest = &Record;
			}
		}
	}
	int32 Discovered = 0;
	if (Newest)
	{
		for (const FDarkwellSpatialPropMemory::FCell& Cell : Newest->SpatialMemory.GetCells())
		{
			Discovered += Cell.DiscoveredPresent > 0.0f ? 1 : 0;
		}
	}
	return Discovered;
}

int32 ADarkwellMovingPropLabRoom::GetNewestHistoricalCellCountForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	const FDarkwellSpatialObservationRecord* Newest = nullptr;
	if (Prop)
	{
		for (const FDarkwellSpatialObservationRecord& Record : Prop->History.GetRecords())
		{
			if (!Record.bCurrentObservedLocation && (!Newest || Record.Epoch > Newest->Epoch))
			{
				Newest = &Record;
			}
		}
	}
	return Newest ? Newest->SpatialMemory.GetCells().Num() : 0;
}

float ADarkwellMovingPropLabRoom::GetLastLegalCoverageRatioForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? Prop->LastLegalCoverageRatio : 0.0f;
}

bool ADarkwellMovingPropLabRoom::IsLastCoverageValidForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop && Prop->bLastCoverageValid;
}

FString ADarkwellMovingPropLabRoom::GetLastCoverageZeroReasonForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? Prop->LastCoverageZeroReason : TEXT("MISSING");
}

int64 ADarkwellMovingPropLabRoom::GetTransformRevisionForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? static_cast<int64>(Prop->TransformRevision) : 0;
}

int64 ADarkwellMovingPropLabRoom::GetCoverageRevisionForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? static_cast<int64>(Prop->CoverageRevision) : 0;
}

int64 ADarkwellMovingPropLabRoom::GetCoverageTransformRevisionForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? static_cast<int64>(Prop->CoverageTransformRevision) : 0;
}

int64 ADarkwellMovingPropLabRoom::GetCoverageGridRevisionForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? static_cast<int64>(Prop->CoverageGridRevision) : 0;
}

int32 ADarkwellMovingPropLabRoom::GetSealCountForTesting(const FName StableId) const
{
	return GetHiddenFreezeCountForTesting(StableId);
}

int32 ADarkwellMovingPropLabRoom::GetObservationEpisodeForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? Prop->ObservationEpisode : 0;
}

FString ADarkwellMovingPropLabRoom::GetObservationStateForTesting(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	if (!Prop)
	{
		return TEXT("MISSING");
	}
	switch (Prop->ObservationState)
	{
	case EObservationState::ObservedArmed: return TEXT("OBSERVED_ARMED");
	case EObservationState::UnobservedSealed: return TEXT("UNOBSERVED_SEALED");
	default: return TEXT("NEVER_OBSERVED");
	}
}

bool ADarkwellMovingPropLabRoom::InjectInvalidCoverageOnceForTesting(
	const FName StableId)
{
	FTrackedProp* Prop = Tracked.Find(StableId);
	if (!Prop)
	{
		return false;
	}
	Prop->bInjectInvalidCoverageOnce = true;
	return true;
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
		? FString::Printf(TEXT("\nROTATE: LIVE %d | STALE %d | PROXIES %d | CAPS %d\nSURFACE %d | CAP %d | TOTAL %d | COVERAGE %.1f%% VALID %d %s\nOBS EPISODE %d %s | TRANSFORM REV %lld | COVERAGE REV %lld | GRID REV %lld | SEALS %d"),
			GetCurrentEpochCountForTesting(Darkwell::MovingPropLab::RotateId),
			GetStaleEpochCountForTesting(Darkwell::MovingPropLab::RotateId),
			GetVisibleHistoricalProxyCountForTesting(Darkwell::MovingPropLab::RotateId),
			GetVisibleHistoricalCapCountForTesting(Darkwell::MovingPropLab::RotateId),
			GetMaxSurfaceContributorsForTesting(Darkwell::MovingPropLab::RotateId),
			GetMaxCapContributorsForTesting(Darkwell::MovingPropLab::RotateId),
			GetMaxTotalContributorsForTesting(Darkwell::MovingPropLab::RotateId),
			GetLastLegalCoverageRatioForTesting(Darkwell::MovingPropLab::RotateId) * 100.0f,
			IsLastCoverageValidForTesting(Darkwell::MovingPropLab::RotateId) ? 1 : 0,
			*GetLastCoverageZeroReasonForTesting(Darkwell::MovingPropLab::RotateId),
			GetObservationEpisodeForTesting(Darkwell::MovingPropLab::RotateId),
			*GetObservationStateForTesting(Darkwell::MovingPropLab::RotateId),
			GetTransformRevisionForTesting(Darkwell::MovingPropLab::RotateId),
			GetCoverageRevisionForTesting(Darkwell::MovingPropLab::RotateId),
			GetCoverageGridRevisionForTesting(Darkwell::MovingPropLab::RotateId),
			GetSealCountForTesting(Darkwell::MovingPropLab::RotateId))
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
