#include "VisionPresentation/DarkwellMovingPropLabRoom.h"
#include "VisionPresentation/DarkwellGrayPolicyLab.h"
#include "VisionPresentation/DarkwellHistoricalVisibilitySweep.h"

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
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/Float16Color.h"
#include "Player/DarkwellCharacter.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectArray.h"
#include "VisionPresentation/DarkwellFogVisualSubsystem.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "VisionPresentation/DarkwellRememberablePropComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDarkwellMovingPropLab, Log, All);

namespace
{
	struct FScopedHistoryRuntimeTimer
	{
		explicit FScopedHistoryRuntimeTimer(double& InMicroseconds)
			: Microseconds(InMicroseconds), StartCycles(FPlatformTime::Cycles64()) {}
		~FScopedHistoryRuntimeTimer()
		{
			Microseconds += FPlatformTime::ToMilliseconds64(
				FPlatformTime::Cycles64() - StartCycles) * 1000.0;
		}
		double& Microseconds;
		uint64 StartCycles;
	};
}

namespace Darkwell::MovingPropLab
{
	constexpr float CellSize = 2.5f;
	constexpr int32 PresentationSamples = 4;
	// The broad phase is deliberately wider than the current 1,250 cm legal
	// cone. Keeping it above the adapter's 2,200 cm authored cone makes the
	// sleeping decision conservative if the active light range changes.
	constexpr double HistorySpatialCellSize = 1000.0;
	constexpr double HistoryMaximumInfluenceRange = 2250.0;
	float HistoricalOpacity(const FDarkwellSpatialObservationRecord& Record, int32 FineIndex, int32 CoarseIndex)
	{
		if (!Record.FineHistory.IsInitialized()) return Record.SpatialMemory.Presentation(CoarseIndex).B;
		const auto& S = Record.FineHistory.GetSamples()[FineIndex];
		return S.State == FDarkwellHistoryGridV2::Superseded() || S.State == FDarkwellHistoryGridV2::NeverObserved()
			? 0.f : S.Opacity * S.FrozenAAEnvelope;
	}
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
	bGrayPolicyLab = Darkwell::GrayPolicyLab::IsWorld(GetWorld());
	if (bGrayPolicyLab && GEngine)
	{
		GEngine->RemoveOnScreenDebugMessage(0xDA474);
	}
	bInWorldControls = Darkwell::MovingPropLab::IsInWorldControlRequest(GetWorld());
	const bool bActive = FindActive(GetWorld()) == this;
	SetActorHiddenInGame(!bActive || bGrayPolicyLab);
	SetActorEnableCollision(bActive && !bGrayPolicyLab);
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
		|| (!Darkwell::GrayPolicyLab::IsWorld(World)
			&& !World->URL.HasOption(TEXT("MoveRules"))
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
	if (bGrayPolicyLab)
	{
		return FBox2D(FVector2D(-7800, -7900), FVector2D(7800, 7900));
	}
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
		++RuntimeFrame.MidCreations;
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
	const FTransform& Transform,
	ESightWeaveObjectPolicySource PolicySource,
	ESightWeaveHistoryMode HistoryMode,
	const FResolvedSightWeaveObjectPolicy* PerFieldPolicy)
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
 Actor->Memory->bUseSpatialMemory=true;
	Actor->FinishSpawning(Transform);
	if (!Actor->HasActorBegunPlay())
	{
		Actor->DispatchBeginPlay();
	}
	for (UStaticMeshComponent* Part : Actor->Memory->GetMemoryPrimitives())
	{
		RuntimeFrame.MidCreations += Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)) ? 1 : 0;
		Part->SetCastShadow(true);
		Part->SetCastHiddenShadow(true);
	}
	Actor->Memory->ApplySourceGeometryVisibility(false);
	// Optional compatibility fixture explicitly preserves its original history matrix.
	if (PolicySource == ESightWeaveObjectPolicySource::UseProjectDefault
		&& (FParse::Param(FCommandLine::Get(), TEXT("PropLabHistoryPolicies"))
			|| GetWorld()->URL.HasOption(TEXT("HistoryPolicies"))))
	{
        PolicySource=ESightWeaveObjectPolicySource::Override;
        HistoryMode=ESightWeaveHistoryMode::Always;
		if (StableId == Darkwell::MovingPropLab::RotateId || StableId == Darkwell::MovingPropLab::MultiLowId)
		{
			PolicySource = ESightWeaveObjectPolicySource::Override;
			HistoryMode = ESightWeaveHistoryMode::StationaryOnly;
		}
		if (StableId == Darkwell::MovingPropLab::EdgeId || StableId == Darkwell::MovingPropLab::MultiBoxId)
		{
			PolicySource = ESightWeaveObjectPolicySource::Override;
			HistoryMode = ESightWeaveHistoryMode::Never;
		}
	}
 const bool GrayFixture=FParse::Param(FCommandLine::Get(),TEXT("PropLabGrayObjectPolicies")) || GetWorld()->URL.HasOption(TEXT("GrayObjectPolicies"));
 FResolvedSightWeaveObjectPolicy FixturePolicy;
 if(GrayFixture && !PerFieldPolicy && PolicySource==ESightWeaveObjectPolicySource::UseProjectDefault)
 {
  // These overrides are resolved once for this registration, including zone resets.
  FixturePolicy.MinimumObservedSpanCm=100;
  FixturePolicy.RevealMode=ESightWeaveRevealMode::WholeObjectAfterSpan;
  FixturePolicy.HistoryMode=ESightWeaveHistoryMode::StationaryOnly;
  if(StableId==Darkwell::MovingPropLab::MainId) FixturePolicy.HistoryMode=ESightWeaveHistoryMode::Always;
  if(StableId==Darkwell::MovingPropLab::EdgeId || StableId==Darkwell::MovingPropLab::MultiHighId || StableId==Darkwell::MovingPropLab::MultiLowId)
   FixturePolicy.RevealMode=ESightWeaveRevealMode::SpatialPartial;
  if(StableId==Darkwell::MovingPropLab::MultiHighId) FixturePolicy.HistoryMode=ESightWeaveHistoryMode::Always;
  if(StableId==Darkwell::MovingPropLab::EdgeId || StableId==Darkwell::MovingPropLab::MultiBoxId) FixturePolicy.HistoryMode=ESightWeaveHistoryMode::Never;
  PerFieldPolicy=&FixturePolicy;
 }
	USightWeaveObjectPolicyComponent* ObjectPolicy = NewObject<USightWeaveObjectPolicyComponent>(Actor);
	ObjectPolicy->PolicySource = PolicySource;
	ObjectPolicy->HistoryMode = HistoryMode;
	if(PerFieldPolicy)
	{
		ObjectPolicy->bOverrideRevealMode=true; ObjectPolicy->RevealMode=PerFieldPolicy->RevealMode;
		ObjectPolicy->bOverrideMinimumObservedSpan=true; ObjectPolicy->MinimumObservedSpanCm=PerFieldPolicy->MinimumObservedSpanCm;
		ObjectPolicy->bOverrideHistoryMode=true; ObjectPolicy->HistoryMode=PerFieldPolicy->HistoryMode;
	}
	Actor->AddInstanceComponent(ObjectPolicy);
	ObjectPolicy->RegisterComponent();

 if(!RegisterRememberable(Actor->Memory,ObjectPolicy)) { Actor->Destroy(); return nullptr; }
 auto& Prop=Tracked.FindChecked(StableId);
 Prop.Dimensions=Dimensions; Prop.Tint=Tint; Prop.Shape=Shape;
	return Actor;
}

void ADarkwellMovingPropLabRoom::DestroyTracked()
{
	for (TPair<FName, FTrackedProp>& Pair : Tracked)
	{
		for (TPair<uint32, FRecordVisual>& Visual : Pair.Value.Visuals)
		{
			DestroyVisual(Visual.Value);
		}
		ReleaseSourcePresentation(Pair.Value);
		if (AActor* Actual = Pair.Value.Actual.Get())
		{
			Actual->Destroy();
		}
	}
	Tracked.Reset();
	OwnedMaterials.Reset();
	OwnedTextures.Reset();
	OwnedCaps.Reset();
	ActiveMotions.Reset();
	HistoricalSpatialIndex.Reset();
	FrameHistoricalCandidates.Reset();
	FrameHistoryDirtyTiles.Reset();
	PendingHistoryDirtyRegions.Reset();
	bHistoricalSpatialIndexDirty = true;
	bHasPreviousHistoryObserver = false;
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
	ReleaseSourcePresentation(*Prop);
	if (AActor* Actual = Prop->Actual.Get())
	{
		PendingHistoryDirtyRegions.Add(ActualBounds(*Actual));
		ActiveMotions.RemoveAll([Actual](const FActiveMotion& Motion)
		{
			return Motion.Prop.Get() == Actual;
		});
		Actual->Destroy();
	}
	for(auto T:Prop->CurrentPresentation.LiveTextures) OwnedTextures.Remove(T.Get());
	Tracked.Remove(StableId);
	bHistoricalSpatialIndexDirty = true;
	bMotionActive = !ActiveMotions.IsEmpty();
}

void ADarkwellMovingPropLabRoom::ConfigureGrayPolicyLabProps()
{
	DestroyTracked();
	bGrayPartialMovingActive = false;
	GrayStressMode = 0;
	auto SpawnPolicy = [this](const TCHAR* Id, const FVector& Position, const FVector& Dimensions,
		const int32 Shape, const FLinearColor& Tint, const ESightWeaveRevealMode Reveal,
		const ESightWeaveHistoryMode History)
	{
		FResolvedSightWeaveObjectPolicy Policy;
		Policy.RevealMode = Reveal;
		Policy.MinimumObservedSpanCm = 100.0f;
		Policy.HistoryMode = History;
		return SpawnTracked(FName(Id), Shape, Dimensions, Tint, FTransform(Position),
			ESightWeaveObjectPolicySource::UseProjectDefault, History, &Policy);
	};
	SpawnPolicy(TEXT("Lab.V2.Whole"), FVector(-6000, -3150, 0), FVector(180, 80, 150), 0,
		FLinearColor(.20f, .48f, .68f), ESightWeaveRevealMode::WholeObjectAfterSpan,
		ESightWeaveHistoryMode::StationaryOnly);
	SpawnPolicy(TEXT("Lab.V2.Partial"), FVector(0, -6150, 0), FVector(620, 75, 115), 4,
		FLinearColor(.62f, .42f, .18f), ESightWeaveRevealMode::SpatialPartial,
		ESightWeaveHistoryMode::StationaryOnly);
	SpawnPolicy(TEXT("Lab.V2.MoveWhole"), FVector(5750, -3150, 0), FVector(170, 80, 145), 0,
		FLinearColor(.22f, .58f, .38f), ESightWeaveRevealMode::WholeObjectAfterSpan,
		ESightWeaveHistoryMode::StationaryOnly);
	auto* PartialMoving = SpawnPolicy(TEXT("Lab.V2.MovePartial"), FVector(6250, -3150, 0), FVector(230, 70, 120), 4,
		FLinearColor(.50f, .32f, .68f), ESightWeaveRevealMode::SpatialPartial,
		ESightWeaveHistoryMode::StationaryOnly);
	if (FTrackedProp* Prop = Tracked.Find(TEXT("Lab.V2.MovePartial")))
	{
		PartialMoving->SetActorHiddenInGame(true);
		PartialMoving->SetActorEnableCollision(false);
		Prop->bExists = false;
	}
	SpawnPolicy(TEXT("Lab.V2.Never"), FVector(-6000, 3850, 0), FVector(170, 80, 145), 0,
		FLinearColor(.72f, .25f, .22f), ESightWeaveRevealMode::SpatialPartial,
		ESightWeaveHistoryMode::Never);
	SpawnPolicy(TEXT("Lab.V2.OcclusionWhole"), FVector(-450, 7000, 0), FVector(180, 75, 145), 0,
		FLinearColor(.20f, .50f, .70f), ESightWeaveRevealMode::WholeObjectAfterSpan,
		ESightWeaveHistoryMode::StationaryOnly);
	SpawnPolicy(TEXT("Lab.V2.OcclusionPartial"), FVector(450, 7000, 0), FVector(260, 70, 115), 4,
		FLinearColor(.62f, .40f, .20f), ESightWeaveRevealMode::SpatialPartial,
		ESightWeaveHistoryMode::StationaryOnly);
}

bool ADarkwellMovingPropLabRoom::ConfigureForGrayPolicyLab(ADarkwellCharacter* Player)
{
	if (!Player || FindActive(GetWorld()) != this || !Darkwell::GrayPolicyLab::IsWorld(GetWorld()))
	{
		return false;
	}
	bGrayPolicyLab = true;
	bInWorldControls = false;
	ConfigureGrayPolicyLabProps();
	Player->RestorePersistentState(Player->GetActorTransform(), Player->GetMaxHealth(),
		DarkwellGameplayTags::State_Player_Alive, FGameplayTag());
	Player->GetLoadoutComponent()->RestorePersistentState(2, 100, 0, 100,
		DarkwellGameplayTags::Equipment_Left_Shotgun, DarkwellGameplayTags::Equipment_Right_Torch);
	bStarted = true;
	ResetHistoryRuntimeTelemetryForTesting();
	return Tracked.Num() == 7;
}

bool ADarkwellMovingPropLabRoom::ResetGrayPolicyRoom(const int32 RoomIndex)
{
	if (!bGrayPolicyLab || RoomIndex < 1 || RoomIndex > 6) return false;
	StopMotion();
	auto Respawn = [this](const TCHAR* Id, const FVector& Position, const FVector& Dimensions,
		const int32 Shape, const FLinearColor& Tint, const ESightWeaveRevealMode Reveal,
		const ESightWeaveHistoryMode History)
	{
		DestroyTracked(FName(Id));
		FResolvedSightWeaveObjectPolicy Policy;
		Policy.RevealMode = Reveal; Policy.MinimumObservedSpanCm = 100.0f; Policy.HistoryMode = History;
		return SpawnTracked(FName(Id), Shape, Dimensions, Tint, FTransform(Position),
			ESightWeaveObjectPolicySource::UseProjectDefault, History, &Policy);
	};
	if (RoomIndex == 1)
	{
		Respawn(TEXT("Lab.V2.Whole"), FVector(-6000, -3150, 0), FVector(180, 80, 150), 0,
			FLinearColor(.20f, .48f, .68f), ESightWeaveRevealMode::WholeObjectAfterSpan, ESightWeaveHistoryMode::StationaryOnly);
	}
	else if (RoomIndex == 2)
	{
		Respawn(TEXT("Lab.V2.Partial"), FVector(0, -6150, 0), FVector(620, 75, 115), 4,
			FLinearColor(.62f, .42f, .18f), ESightWeaveRevealMode::SpatialPartial, ESightWeaveHistoryMode::StationaryOnly);
	}
	else if (RoomIndex == 3)
	{
		Respawn(TEXT("Lab.V2.MoveWhole"), FVector(5750, -3150, 0), FVector(170, 80, 145), 0,
			FLinearColor(.22f, .58f, .38f), ESightWeaveRevealMode::WholeObjectAfterSpan, ESightWeaveHistoryMode::StationaryOnly);
		Respawn(TEXT("Lab.V2.MovePartial"), FVector(6250, -3150, 0), FVector(230, 70, 120), 4,
			FLinearColor(.50f, .32f, .68f), ESightWeaveRevealMode::SpatialPartial, ESightWeaveHistoryMode::StationaryOnly);
		const FName HiddenId = bGrayPartialMovingActive ? FName(TEXT("Lab.V2.MoveWhole")) : FName(TEXT("Lab.V2.MovePartial"));
		if (FTrackedProp* Hidden = Tracked.Find(HiddenId))
		{
			auto* Actor = Hidden->Actual.Get();
			Actor->SetActorHiddenInGame(true); Actor->SetActorEnableCollision(false); Hidden->bExists = false;
		}
	}
	else if (RoomIndex == 4)
	{
		Respawn(TEXT("Lab.V2.Never"), FVector(-6000, 3850, 0), FVector(170, 80, 145), 0,
			FLinearColor(.72f, .25f, .22f), ESightWeaveRevealMode::SpatialPartial, ESightWeaveHistoryMode::Never);
	}
	else if (RoomIndex == 5)
	{
		Respawn(TEXT("Lab.V2.OcclusionWhole"), FVector(-450, 7000, 0), FVector(180, 75, 145), 0,
			FLinearColor(.20f, .50f, .70f), ESightWeaveRevealMode::WholeObjectAfterSpan, ESightWeaveHistoryMode::StationaryOnly);
		Respawn(TEXT("Lab.V2.OcclusionPartial"), FVector(450, 7000, 0), FVector(260, 70, 115), 4,
			FLinearColor(.62f, .40f, .20f), ESightWeaveRevealMode::SpatialPartial, ESightWeaveHistoryMode::StationaryOnly);
	}
	else SetGrayPolicyStressMode(0);
	ResetHistoryRuntimeTelemetryForTesting();
	return true;
}

FName ADarkwellMovingPropLabRoom::GetGrayPolicyMovingSubject() const
{
	return bGrayPartialMovingActive ? FName(TEXT("Lab.V2.MovePartial")) : FName(TEXT("Lab.V2.MoveWhole"));
}

bool ADarkwellMovingPropLabRoom::ToggleGrayPolicyMovingSubject()
{
	if (!bGrayPolicyLab || bMotionActive) return false;
	bGrayPartialMovingActive = !bGrayPartialMovingActive;
	return ResetGrayPolicyRoom(3);
}

bool ADarkwellMovingPropLabRoom::StartGrayPolicyMotion(const bool bRotate)
{
	if (!bGrayPolicyLab || bMotionActive) return false;
	FTrackedProp* Prop = Tracked.Find(GetGrayPolicyMovingSubject());
	AActor* Actual = Prop ? Prop->Actual.Get() : nullptr;
	if (!Prop || !Actual || !Prop->bExists) return false;
	const FTransform Start = Actual->GetActorTransform();
	const FTransform Target(bRotate ? FRotator(0, Start.Rotator().Yaw + 180.0f, 0) : Start.Rotator(),
		Start.GetLocation() + (bRotate ? FVector::ZeroVector : FVector(0, 500, 0)));
	StartMotion(Actual, Target, bRotate ? 3.0f : 4.0f);
	return true;
}

void ADarkwellMovingPropLabRoom::DestroyGrayStressProps()
{
	TArray<FName> Remove;
	for (const auto& Pair : Tracked)
	{
		if (Pair.Key.ToString().StartsWith(TEXT("Lab.V2.Stress"))) Remove.Add(Pair.Key);
	}
	for (const FName Id : Remove) DestroyTracked(Id);
}

bool ADarkwellMovingPropLabRoom::SetGrayPolicyStressMode(const int32 Mode)
{
	if (!bGrayPolicyLab || Mode < 0 || Mode > 7 || bMotionActive)
	{
		UE_LOG(LogDarkwellMovingPropLab, Error,
			TEXT("GRAY_POLICY_STRESS_REJECT mode=%d gray_lab=%d motion=%d"),
			Mode, bGrayPolicyLab ? 1 : 0, bMotionActive ? 1 : 0);
		return false;
	}
	DestroyGrayStressProps();
	GrayStressMode = Mode;
	if (Mode == 0)
	{
		ResetHistoryRuntimeTelemetryForTesting();
		return true;
	}
	auto SpawnStress = [this](const int32 Index, const FVector& Position,
		const ESightWeaveRevealMode Reveal = ESightWeaveRevealMode::WholeObjectAfterSpan,
		const ESightWeaveHistoryMode History = ESightWeaveHistoryMode::StationaryOnly)
	{
		const FName Id(*FString::Printf(TEXT("Lab.V2.Stress.%03d"), Index));
		FResolvedSightWeaveObjectPolicy Policy;
		Policy.RevealMode = Reveal; Policy.MinimumObservedSpanCm = 100.0f; Policy.HistoryMode = History;
		return SpawnTracked(Id, 0, FVector(48, 48, 90), FLinearColor(.28f, .46f, .62f), FTransform(Position),
			ESightWeaveObjectPolicySource::UseProjectDefault, History, &Policy);
	};
	const FVector Center(6000, 3850, 0);
	if (Mode <= 3)
	{
		const int32 Count = Mode == 1 ? 1 : Mode == 2 ? 8 : 32;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			SpawnStress(Index, Center + FVector((Index % 8 - 3.5f) * 120.0f, (Index / 8) * 120.0f, 0));
		}
	}
	else if (Mode == 4)
	{
		for (int32 Index = 0; Index < 8; ++Index)
		{
			ADarkwellPropLabFurniture* Actor = SpawnStress(Index,
				Center + FVector((Index % 4) * 18.0f, (Index / 4) * 18.0f, 0),
				ESightWeaveRevealMode::SpatialPartial);
			if (!Actor || !ConfigureHistoricalEpochCountForTesting(Actor->StableId, 8))
			{
				UE_LOG(LogDarkwellMovingPropLab, Error,
					TEXT("GRAY_POLICY_STRESS_SETUP_FAIL mode=%d actor=%d spawned=%d"),
					Mode, Index, Actor ? 1 : 0);
				return false;
			}
		}
	}
	else if (Mode == 5)
	{
		ADarkwellPropLabFurniture* Actor = SpawnStress(
			0, Center, ESightWeaveRevealMode::SpatialPartial);
		if (!Actor || !ConfigureHistoricalEpochCountForTesting(Actor->StableId, 64))
		{
			UE_LOG(LogDarkwellMovingPropLab, Error,
				TEXT("GRAY_POLICY_STRESS_SETUP_FAIL mode=%d actor=0 spawned=%d"),
				Mode, Actor ? 1 : 0);
			return false;
		}
	}
	else if (Mode == 6)
	{
		const FVector Starts[] = {Center, FVector(-6000, -3150, 0), FVector(-6000, 3850, 0)};
		const int32 Counts[] = {64, 64, 56};
		for (int32 Index = 0; Index < 3; ++Index)
		{
			ADarkwellPropLabFurniture* Actor = SpawnStress(
				Index, Starts[Index], ESightWeaveRevealMode::SpatialPartial);
			if (!Actor || !ConfigureHistoricalEpochCountForTesting(Actor->StableId, Counts[Index]))
			{
				UE_LOG(LogDarkwellMovingPropLab, Error,
					TEXT("GRAY_POLICY_STRESS_SETUP_FAIL mode=%d actor=%d spawned=%d histories=%d"),
					Mode, Index, Actor ? 1 : 0, Counts[Index]);
				return false;
			}
		}
	}
	else
	{
		for (int32 Index = 0; Index < 6; ++Index)
		{
			const auto Reveal = Index < 3 ? ESightWeaveRevealMode::WholeObjectAfterSpan : ESightWeaveRevealMode::SpatialPartial;
			const auto History = Index % 3 == 0 ? ESightWeaveHistoryMode::Always
				: Index % 3 == 1 ? ESightWeaveHistoryMode::StationaryOnly : ESightWeaveHistoryMode::Never;
			SpawnStress(Index, Center + FVector((Index - 2.5f) * 160.0f, 0, 0), Reveal, History);
		}
	}
	ResetHistoryRuntimeTelemetryForTesting();
	return true;
}

bool ADarkwellMovingPropLabRoom::ConfigureHistoricalEpochCountForTesting(
	const FName StableId, const int32 HistoricalEpochs)
{
	if (HistoricalEpochs < 0
		|| HistoricalEpochs > FDarkwellSpatialObservationHistory::MaxResidentRecords)
	{
		return false;
	}
	FTrackedProp* Prop = Tracked.Find(StableId);
	AActor* Actual = Prop ? Prop->Actual.Get() : nullptr;
	if (!Prop || !Actual)
	{
		return false;
	}
	for (TPair<uint32, FRecordVisual>& Pair : Prop->Visuals)
	{
		DestroyVisual(Pair.Value);
	}
	Prop->Visuals.Reset();
	Prop->History.Initialize(StableId);
	Prop->ObservationState = EObservationState::NeverObserved;
	Prop->HiddenFreezeCount = 0;
	Prop->ObservationEpisode = 0;
	for (int32 Index = 0; Index < HistoricalEpochs; ++Index)
	{
		++GeometryRevision;
		const FTransform Pose(FRotator(0.0f, Index * 17.0f, 0.0f),
			Prop->InitialTransform.GetLocation() + FVector(Index * 7.0f, 0.0f, 0.0f));
		Actual->SetActorTransform(Pose);
		Actual->SetActorHiddenInGame(false);
		Actual->SetActorEnableCollision(true);
		Prop->bExists = true;
		Prop->LastPhysicalTransform = Pose;
		Prop->LastGeometryTransform = Pose;
		const FBox2D Bounds = ActualBounds(*Actual);
		const int32 CurrentIndex = Prop->History.BeginObservedLocation(
			Pose, Bounds, Darkwell::MovingPropLab::CellSize);
		if (CurrentIndex == INDEX_NONE)
		{
			UE_LOG(LogDarkwellMovingPropLab, Error,
				TEXT("GRAY_POLICY_SYNTHETIC_HISTORY_FAIL id=%s requested=%d index=%d stage=begin records=%d"),
				*StableId.ToString(), HistoricalEpochs, Index, Prop->History.GetRecords().Num());
			return false;
		}
		FDarkwellSpatialObservationRecord& Current =
			Prop->History.GetMutableRecords()[CurrentIndex];
		TArray<float> FullCoverage;
		FullCoverage.Init(1.0f, Current.SpatialMemory.GetCells().Num());
		Prop->History.AdvanceCurrent(0.20f, FullCoverage);
		// This helper seeds the exact state a real stationary legal observation
		// reaches before it is hidden. Do not bypass the policy lifecycle: arming
		// StationaryOnly here keeps the synthetic stress records semantically
		// identical to records captured by UpdateTracked.
		Prop->ObjectPolicy->NotifyLegalObservation();
		Prop->ObservationState = EObservationState::ObservedArmed;
		++Prop->ObservationEpisode;
		EnsureRecordVisual(*Prop, Current);
		if (!FreezeCurrentForHiddenMotion(*Prop, TEXT("TEST_EPOCH_SCALING")))
		{
			UE_LOG(LogDarkwellMovingPropLab, Error,
				TEXT("GRAY_POLICY_SYNTHETIC_HISTORY_FAIL id=%s requested=%d index=%d stage=freeze records=%d state=%d"),
				*StableId.ToString(), HistoricalEpochs, Index, Prop->History.GetRecords().Num(),
				static_cast<int32>(Prop->ObservationState));
			return false;
		}
	}
	Actual->FindComponentByClass<UDarkwellRememberablePropComponent>()->ApplySourceGeometryVisibility(false);
	Actual->SetActorEnableCollision(false);
	Actual->SetActorHiddenInGame(true);
	Prop->bExists = false;
	bHistoricalSpatialIndexDirty = true;
	ResetHistoryRuntimeTelemetryForTesting();
	return Prop->History.GetRecords().Num() == HistoricalEpochs;
}

void ADarkwellMovingPropLabRoom::LogRotationFrame(const FTrackedProp& Prop) const
{
	FScopedHistoryRuntimeTimer Timer(RuntimeFrame.LogRotationFrameUs);
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
	if (const AActor* Actual = Prop.Actual.Get())
	{
		ActualComponents = Actual->FindComponentByClass<UDarkwellRememberablePropComponent>()->GetMemoryPrimitives().Num();
		for (const UStaticMeshComponent* Part : Actual->FindComponentByClass<UDarkwellRememberablePropComponent>()->GetMemoryPrimitives())
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

bool ADarkwellMovingPropLabRoom::SetTrackedExists(
	const FName StableId,
	const bool bExists)
{
	FTrackedProp* Prop = Tracked.Find(StableId);
	AActor* Actual = Prop ? Prop->Actual.Get() : nullptr;
	if (!Prop || !Actual || Prop->bExists == bExists)
	{
		return false;
	}
	PendingHistoryDirtyRegions.Add(ActualBounds(*Actual));
	++GeometryRevision;
	++Prop->ObservationOwnershipRevision;
	Prop->bDiagnosticsDirty = true;
	if (!bExists)
	{
		FreezeCurrentForHiddenMotion(*Prop, TEXT("ACTUAL_BECAME_ABSENT"));
		Actual->FindComponentByClass<UDarkwellRememberablePropComponent>()->ApplySourceGeometryVisibility(false);
		Actual->SetActorEnableCollision(false);
		Actual->SetActorHiddenInGame(true);
		Prop->bExists = false;
	}
	else
	{
		Prop->bExists = true;
		Actual->SetActorHiddenInGame(false);
		Actual->SetActorEnableCollision(true);
		Actual->FindComponentByClass<UDarkwellRememberablePropComponent>()->ApplySourceGeometryVisibility(false);
		Prop->LastPhysicalTransform = Actual->GetActorTransform();
		Prop->LastGeometryTransform = Prop->LastPhysicalTransform;
		PendingHistoryDirtyRegions.Add(ActualBounds(*Actual));
	}
	UE_LOG(LogDarkwellMovingPropLab, Display,
		TEXT("MOVING_RULES_ACTUAL id=%s state=%s records=%d"),
		*StableId.ToString(), bExists ? TEXT("PRESENT") : TEXT("ABSENT"),
		Prop->History.GetRecords().Num());
	return true;
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
 if(FParse::Param(FCommandLine::Get(),TEXT("PropLabGrayObjectPolicies")) || GetWorld()->URL.HasOption(TEXT("GrayObjectPolicies")))
 {
  const TCHAR* Labels[]{TEXT("WHOLE / ALWAYS"),TEXT("STATIC WHOLE MEMORY"),TEXT("WHOLE / NEVER"),
   TEXT("PARTIAL / ALWAYS"),TEXT("STATIC PARTIAL MEMORY"),TEXT("STATIC NEVER CONTROL")};
  for(int32 I=0;I<6;++I)
  {
   FResolvedSightWeaveObjectPolicy Policy;
   Policy.RevealMode=I<3?ESightWeaveRevealMode::WholeObjectAfterSpan:ESightWeaveRevealMode::SpatialPartial;
   Policy.MinimumObservedSpanCm=100;
   Policy.HistoryMode=I%3==0?ESightWeaveHistoryMode::Always:I%3==1?ESightWeaveHistoryMode::StationaryOnly:ESightWeaveHistoryMode::Never;
   auto* Object=SpawnTracked(*FString::Printf(TEXT("Lab.Gray.Static.%d"),I),0,FVector(150,75,145),
    FLinearColor(.2f+.1f*I,.45f,.5f),FTransform(FVector(-1400+500*I,-900,0)),ESightWeaveObjectPolicySource::UseProjectDefault,Policy.HistoryMode,&Policy);
   if(Object)
   {
    auto* Label=NewObject<UTextRenderComponent>(Object); Object->AddInstanceComponent(Label);
    Label->SetupAttachment(Object->GetRootComponent()); Label->SetRelativeLocation(FVector(0,60,170));
    Label->SetRelativeRotation(FRotator(0,90,0)); Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetWorldSize(18); Label->SetText(FText::FromString(Labels[I])); Label->SetTextRenderColor(I%3==2?FColor::Orange:FColor::Green);
    Label->RegisterComponent();
   }
  }
 }
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
	const FName Id = GetInWorldPropId(Kind);
	const USightWeaveObjectPolicyComponent* Policy = GetObjectPolicyForTesting(Id);
	const TCHAR* PolicyName = !Policy ? TEXT("")
		: Policy->GetResolvedHistoryMode() == ESightWeaveHistoryMode::Always ? TEXT(" | Always")
		: Policy->GetResolvedHistoryMode() == ESightWeaveHistoryMode::StationaryOnly ? TEXT(" | StationaryOnly") : TEXT(" | Never");
	return FText::FromString(FString::Printf(TEXT("%s\n[%s]%s%s"), Name, *State, PolicyName, *Diagnostics));
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

bool ADarkwellMovingPropLabRoom::TriggerInWorldControlForTesting(
    const EDarkwellMovingPropLabControlKind Kind, ADarkwellCharacter* Character)
{
    ADarkwellMovingPropLabControl* Control=GetControlForTesting(Kind);
    return Control && Control->TriggerForLabEvidence(Character);
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
	AActor* Actual = Main ? Main->Actual.Get() : nullptr;
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
	if ((Count != 1 && Count != 2 && Count != 8 && Count != 32) || FindActive(GetWorld()) != this)
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
	AActor* Actual = Prop ? Prop->Actual.Get() : nullptr;
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
	Prop->ObjectPolicy->SetSightWeaveMoving(true);
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
	for (const FActiveMotion& Motion : ActiveMotions)
		if (AActor* Actual = Motion.Prop.Get())
			if (FTrackedProp* Prop = Tracked.Find(Actual->FindComponentByClass<UDarkwellRememberablePropComponent>()->GetStableId()))
				Prop->ObjectPolicy->SetSightWeaveMoving(false);
	bMotionActive = false;
	ActiveMotions.Reset();
}

void ADarkwellMovingPropLabRoom::StartMotion(
	AActor* Prop,
	const FTransform& Target,
	const float Duration)
{
	if (!Prop || Duration <= 0.0f)
	{
		return;
	}
	if (FTrackedProp* TrackedProp = Tracked.Find(Prop->FindComponentByClass<UDarkwellRememberablePropComponent>()->GetStableId()))
		TrackedProp->ObjectPolicy->SetSightWeaveMoving(true);
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
		AActor* Actual = Motion.Prop.Get();
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
			if (FTrackedProp* Prop = Tracked.Find(Actual->FindComponentByClass<UDarkwellRememberablePropComponent>()->GetStableId()))
				Prop->ObjectPolicy->SetSightWeaveMoving(false);
			ActiveMotions.RemoveAtSwap(Index);
		}
	}
	bMotionActive = !ActiveMotions.IsEmpty();
}

void ADarkwellMovingPropLabRoom::UpdateRoom(float DeltaSeconds, ADarkwellCharacter* Player)
{
 if (!bStarted || !Player || FindActive(GetWorld()) != this) return;
 const uint64 Start=FPlatformTime::Cycles64();
 UpdateInWorldAutomation(DeltaSeconds,Player);
 const bool WasMoving=bMotionActive;
 UpdateDeterministicMotion(DeltaSeconds);
 if(bInWorldControls && WasMoving && !bMotionActive) CompleteInWorldMotionGroup();
 UpdateMemory(DeltaSeconds,Player->GetActorLocation());
 UpdateInWorldAutomation(0,Player);
 Report();
 // Keep the legacy full fixture metric inclusive of its controls and HUD.
 const double FullUs=FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64()-Start)*1000;
 RuntimeTotal.MovingPropLabGameThreadUs+=FullUs-RuntimeFrame.MovingPropLabGameThreadUs;
 RuntimeFrame.MovingPropLabGameThreadUs=FullUs;
}

bool ADarkwellMovingPropLabRoom::TryDuplicateStableIdForTesting(const FName StableId)
{
	return SpawnTracked(StableId, 0, FVector(60, 60, 90), FLinearColor::Gray,
		FTransform(FVector::ZeroVector)) != nullptr;
}

bool ADarkwellMovingPropLabRoom::StartTrackedRotationForTesting(
	const FName StableId,
	const float TargetYaw,
	const float Duration)
{
	FTrackedProp* Prop = Tracked.Find(StableId);
	AActor* Actual = Prop ? Prop->Actual.Get() : nullptr;
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
	const AActor* Actual = Prop ? Prop->Actual.Get() : nullptr;
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

bool ADarkwellMovingPropLabRoom::ResetTrackedPolicyForLab(FName StableId, ESightWeaveHistoryMode Mode)
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	if (!Prop || bMotionActive) return false;
	const FTransform Transform = Prop->InitialTransform;
	const FVector Dimensions = Prop->Dimensions;
	const FLinearColor Tint = Prop->Tint;
	const int32 Shape = Prop->Shape;
	DestroyTracked(StableId);
	return SpawnTracked(StableId, Shape, Dimensions, Tint, Transform,
		ESightWeaveObjectPolicySource::Override, Mode) != nullptr;
}

bool ADarkwellMovingPropLabRoom::ResetTrackedRevealPolicyForLab(FName StableId,
	ESightWeaveRevealMode RevealMode,float MinimumSpanCm,ESightWeaveHistoryMode HistoryMode)
{
	const FTrackedProp* Prop=Tracked.Find(StableId);
	if(!Prop || bMotionActive || !FMath::IsFinite(MinimumSpanCm) || MinimumSpanCm<0) return false;
	const auto Transform=Prop->InitialTransform; const auto Dimensions=Prop->Dimensions;
	const auto Tint=Prop->Tint; const int32 Shape=Prop->Shape;
	FResolvedSightWeaveObjectPolicy Policy; Policy.RevealMode=RevealMode; Policy.MinimumObservedSpanCm=MinimumSpanCm; Policy.HistoryMode=HistoryMode;
	DestroyTracked(StableId);
	return SpawnTracked(StableId,Shape,Dimensions,Tint,Transform,ESightWeaveObjectPolicySource::UseProjectDefault,HistoryMode,&Policy)!=nullptr;
}

void ADarkwellMovingPropLabRoom::Report()
{
	FScopedHistoryRuntimeTimer Timer(RuntimeFrame.ReportHudUs);
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
		TEXT("MOVING + MULTI PROP LAB | MODE %d | RULE SpatialEvidenceOnly | HISTORY GRID V2 | ENEMY 0\nScenario %d | Phase %d | Motion %s | Object position %s\nCurrent interaction: %s%s\nCompleted %d/6 | NEXT TEST: %s\nIdentities %d | Spatial records %d | Multi %d\nWalk to a labeled mechanism and press F. Console is not required."),
		Darkwell::PropLab::PresentationMode(GetWorld()), Scenario, ScenarioPhase,
		*GetMotionState(), *GetObjectPositionLabel(), *CurrentInteraction, *RotationDiagnostics,
		CompletedInWorldControls.Num(), *GetNextInWorldControlLabel(),
		Tracked.Num(), GetTotalSpatialRecordCount(), MultiCount);
	const FName PolicyId = ActiveControl == EDarkwellMovingPropLabControlKind::MultiProp
		? Darkwell::MovingPropLab::MultiLowId : GetInWorldPropId(ActiveControl);
	if (!PolicyId.IsNone()) Status += TEXT("\n") + GetHistoryPolicyTelemetry(PolicyId);
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
	if (GEngine && !bGrayPolicyLab)
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
