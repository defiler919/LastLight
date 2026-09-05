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

FString ADarkwellMovingPropLabRoom::GetMovingLiveTelemetry(FName Id) const
{
	const FTrackedProp* P=Tracked.Find(Id);
	if(!P || P->History.GetCurrentIndex()==INDEX_NONE) return TEXT("{\"current\":0}");
	const auto& R=P->History.GetRecords()[P->History.GetCurrentIndex()];
	const auto& M=R.SpatialMemory; const auto* V=P->Visuals.Find(R.Epoch);
	double AMin=1, AMax=0, ASum=0, LMin=1, LMax=0, LSum=0; int32 D=0;
	TArray<FDarkwellSpatialPropMemory::FCell> DiagnosticCells;
    if(P->LocalEpoch==R.Epoch) for(const auto& Part:P->CurrentLive.Parts) DiagnosticCells.Append(Part.Local.GetCells());
    else DiagnosticCells.Append(M.GetCells());
	for(const auto& C:DiagnosticCells) { AMin=FMath::Min(AMin,double(C.AppearanceBlend)); AMax=FMath::Max(AMax,double(C.AppearanceBlend)); ASum+=C.AppearanceBlend;
		LMin=FMath::Min(LMin,double(C.LiveBlend)); LMax=FMath::Max(LMax,double(C.LiveBlend)); LSum+=C.LiveBlend; D+=C.DiscoveredPresent>0; }
	int32 Visible=0; if(P->Actual.IsValid()) for(const auto& Part:P->Actual->Parts) Visible+=Part && Part->IsVisible();
	const int32 N=FMath::Max(1,DiagnosticCells.Num()); const auto B=M.GetBounds(); const auto S=M.GetSize();
	FString Result = FString::Printf(TEXT("{\"current\":1,\"epoch\":%u,\"stale\":%d,\"pose_updates\":%llu,\"initialize\":%llu,\"begin_present\":%llu,\"transform_revision\":%llu,\"grid\":[%d,%d],\"bounds\":[%.6f,%.6f,%.6f,%.6f],\"discovered\":%d,\"appearance\":[%.6f,%.6f,%.6f],\"live\":[%.6f,%.6f,%.6f],\"coverage\":%.6f,\"valid\":%d,\"visible_primitives\":%d,\"texture_creations\":%d,\"texture_uploads\":%d,\"presentation_hash\":\"%llu\",\"yaw\":%.6f}"),
		R.Epoch,GetStaleEpochCountForTesting(Id),R.PoseUpdates,M.GetInitializeCount(),M.GetBeginPresentCount(),P->TransformRevision,S.X,S.Y,B.Min.X,B.Min.Y,B.Max.X,B.Max.Y,D,
		AMin,ASum/N,AMax,LMin,LSum/N,LMax,P->LastLegalCoverageRatio,P->bLastCoverageValid,Visible,V?V->TextureCreationCount+V->LiveTextureCreations:0,V?V->TextureUploadCount+V->LiveTextureUploads:0,V?V->TextureSignature:0,R.SnapshotTransform.Rotator().Yaw);
    Result.LeftChopInline(1);
    Result += FString::Printf(TEXT(",\"local_state_hash\":\"%llu\",\"local_samples_touched\":%llu,\"local_coverage_queries\":%llu,\"geometry_resets\":%llu,\"fully_observed_pose\":%d}"),
        P->CurrentLive.StateHash(), P->CurrentLive.SamplesTouched, P->CurrentLive.Queries, P->CurrentLive.GeometryResets, P->CurrentLive.bFullyObservedAtPose);
    return Result;

}

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

	FTrackedProp& Prop = Tracked.Add(StableId);
	Prop.ObjectPolicy = ObjectPolicy;
	Prop.StableId = StableId;
	Prop.Actual = Actor;
	Prop.InitialTransform = Transform;
	Prop.LastPhysicalTransform = Transform;
	Prop.LastGeometryTransform = Transform;
	Prop.Dimensions = Dimensions;
	Prop.Tint = Tint;
	Prop.Shape = Shape;
	Prop.bExists = true;
	Prop.History.Initialize(StableId);
	PendingHistoryDirtyRegions.Add(ActualBounds(*Actor));
	return Actor;
}

void ADarkwellMovingPropLabRoom::DestroyVisual(
	FRecordVisual& Visual, const bool bDiscardEvidence)
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
	for(auto& Texture:Visual.LiveTextures) OwnedTextures.Remove(Texture.Get());
	Visual.LiveTextures.Reset(); Visual.LivePixels.Reset(); Visual.LiveSignatures.Reset();
	Visual.Materials.Reset();
	Visual.CapTriangles = 0;
	Visual.CapSamplePoints.Reset();
	Visual.CapQuads.Reset();
	Visual.SubmittedPresentation.Reset();
	if (bDiscardEvidence)
	{
		Visual.SuppressedByCurrentEvidence.Reset();
		Visual.CachedCoarseCoverage.Reset();
		Visual.CachedCoarseEvidence.Reset();
		Visual.CachedFineCoverage.Reset();
		Visual.CachedFineOccupied.Reset(); Visual.CachedCoarseOccupied.Reset();
		Visual.CachedGeometryRegions.Reset(); Visual.CachedPhysicalGeometry.Reset(); Visual.CachedNewerGeometry.Reset();
	}
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
	if (ADarkwellPropLabFurniture* Actual = Prop->Actual.Get())
	{
		PendingHistoryDirtyRegions.Add(ActualBounds(*Actual));
		ActiveMotions.RemoveAll([Actual](const FActiveMotion& Motion)
		{
			return Motion.Prop.Get() == Actual;
		});
		Actual->Destroy();
	}
	Tracked.Remove(StableId);
	bHistoricalSpatialIndexDirty = true;
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
			Geometry.CachePlanarProjection();
		}
		++PrimitiveIndex;
	}
	return Result;
}

void ADarkwellMovingPropLabRoom::FPrimitiveGeometrySnapshot::CachePlanarProjection()
{
 bCachedPlanarProjection=false;
 const auto Rotation=WorldTransform.GetRotation();
 if(!LocalBounds.IsValid || WorldTransform.ContainsNaN() || Rotation.X!=0 || Rotation.Y!=0) return;
 const FVector Direction=WorldTransform.InverseTransformVector(FVector::UpVector);
 if(Direction.X!=0 || Direction.Y!=0 || FMath::Abs(Direction.Z)<=UE_DOUBLE_SMALL_NUMBER) return;
 const double OriginZ=WorldTransform.InverseTransformPosition(FVector::ZeroVector).Z;
 PlanarMinZ=(LocalBounds.Min.Z-OriginZ)/Direction.Z;
 PlanarMaxZ=(LocalBounds.Max.Z-OriginZ)/Direction.Z;
 if(PlanarMinZ>PlanarMaxZ) Swap(PlanarMinZ,PlanarMaxZ);
 const FVector Scale=WorldTransform.GetScale3D().GetAbs();
 ToleranceScale=FMath::Max(UE_DOUBLE_SMALL_NUMBER,FMath::Min(Scale.X,Scale.Y));
 bCachedPlanarProjection=true;
}

bool ADarkwellMovingPropLabRoom::QueryVerticalInterval(
	const FPrimitiveGeometrySnapshot& Geometry,
	const FVector2D Point,
	double& OutMinZ,
	double& OutMaxZ,
	const double ProjectionTolerance) const
{
	++RuntimeFrame.PrimitiveGeometryTests;
 bool UsePlanar=Geometry.bCachedPlanarProjection && FMath::IsFinite(Point.X) && FMath::IsFinite(Point.Y);
#if WITH_DEV_AUTOMATION_TESTS
 UsePlanar &= !bForceFullHistoryEvidenceForTesting;
#endif
 if(UsePlanar)
 {
  // Pure world-Z rotation makes inverse local Z independent of the query XY.
  // Preserve the original inverse transform, division, inclusive tolerance and
  // interval arithmetic exactly; tilted and singular geometry uses the full slab.
  const FVector Local=Geometry.WorldTransform.InverseTransformPosition(FVector(Point.X,Point.Y,0));
  const double Tolerance=ProjectionTolerance/Geometry.ToleranceScale;
  for(int32 Axis=0;Axis<2;++Axis)
  {
   const double Minimum=Geometry.LocalBounds.Min[Axis]-Tolerance;
   const double Maximum=Geometry.LocalBounds.Max[Axis]+Tolerance;
   if(Local[Axis]<Minimum-UE_KINDA_SMALL_NUMBER || Local[Axis]>Maximum+UE_KINDA_SMALL_NUMBER) return false;
  }
  OutMinZ=Geometry.PlanarMinZ; OutMaxZ=Geometry.PlanarMaxZ;
  return OutMaxZ-OutMinZ>UE_KINDA_SMALL_NUMBER;
 }

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
    const int32 CurrentIndex=Prop.History.GetCurrentIndex();
    const bool LocalCurrent=CurrentIndex!=INDEX_NONE && Prop.LocalEpoch==Prop.History.GetRecords()[CurrentIndex].Epoch;
    if(LocalCurrent && (!Prop.bLastCoverageValid || !Prop.History.GetRecords()[CurrentIndex].SnapshotTransform.Equals(Actual->GetActorTransform()))) return false;
 TArray<FPrimitiveGeometrySnapshot> FallbackGeometry;
 TConstArrayView<FPrimitiveGeometrySnapshot> CurrentGeometry;
 if(bUseFrameOccupancy) for(const auto& Snapshot:FrameOccupancy)
  if(Snapshot.StableId==Prop.StableId) { CurrentGeometry=Snapshot.Geometry; break; }
 if(CurrentGeometry.IsEmpty()) { FallbackGeometry=ActualPartGeometry(*Actual); CurrentGeometry=FallbackGeometry; }
	for (const FPrimitiveGeometrySnapshot& Geometry : CurrentGeometry)
	{
		if (ProjectionTolerance > 0.0)
		{
			FVector Local = Geometry.WorldTransform.InverseTransformPosition(FVector(Point, Geometry.WorldTransform.GetLocation().Z));
			Local.X = FMath::Clamp(Local.X, Geometry.LocalBounds.Min.X + 1.e-6, Geometry.LocalBounds.Max.X - 1.e-6);
			Local.Y = FMath::Clamp(Local.Y, Geometry.LocalBounds.Min.Y + 1.e-6, Geometry.LocalBounds.Max.Y - 1.e-6);
			const FVector2D EvidencePoint(Geometry.WorldTransform.TransformPosition(Local));
            if (LocalCurrent ? !Prop.CurrentLive.HasObservedContributionAt(EvidencePoint,Geometry.PrimitiveIndex)
                : !HasCurrentObservedContributionAt(Prop,EvidencePoint)) continue;
		}
        else if(LocalCurrent && !Prop.CurrentLive.HasObservedContributionAt(Point,Geometry.PrimitiveIndex)) continue;
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
 auto ProcessCandidate=[&](const FDarkwellSpatialObservationRecord& Candidate)
 {
		if (Candidate.Epoch <= OlderEpoch)
		{
			return;
		}
		if (Candidate.bCurrentObservedLocation)
		{
			TArray<FVector2D> CurrentIntervals;
			CollectCurrentOwnedVerticalIntervals(Prop, Point, CurrentIntervals, ProjectionTolerance);
			OutIntervals.Append(CurrentIntervals);
			return;
		}
		const FRecordVisual* Visual = Prop.Visuals.Find(Candidate.Epoch);
		const FBox2D& Bounds = Candidate.SpatialMemory.GetBounds();
		const FIntPoint Coarse = Candidate.SpatialMemory.GetSize();
		const FIntPoint Fine = Coarse * Darkwell::MovingPropLab::PresentationSamples;
		if (!Visual || Visual->bPresentationRetired || (ProjectionTolerance == 0.0 && !Bounds.IsInside(Point))
			|| Fine.X <= 0 || Fine.Y <= 0)
		{
			return;
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
			|| Darkwell::MovingPropLab::HistoricalOpacity(Candidate, FineIndex, CellIndex) <= 0.0f)
		{
			return;
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
 };
 if(bUseNewerCandidates && NewerCandidateId==Prop.StableId)
  { for(const auto* C:FrameNewerCandidates) ProcessCandidate(*C); }
 else { for(const auto& C:Prop.History.GetRecords()) ProcessCandidate(C); }
 return !OutIntervals.IsEmpty();
}

bool ADarkwellMovingPropLabRoom::HasNewerObservedGeometryOverlapAt(
	const FTrackedProp& Prop,
	const FRecordVisual& OlderVisual,
	const uint32 OlderEpoch,
	const FVector2D Point) const
{
 if(bUseNewerCandidates && NewerCandidateId==Prop.StableId && (FrameNewerCandidates.IsEmpty() || NewerCandidateMaximumEpoch<=OlderEpoch)) return false;
	if (bUseNewerCandidates && NewerCandidateId == Prop.StableId
		&& FrameNewerCandidates.Num() == 1
		&& FrameNewerCandidates[0]
		&& FrameNewerCandidates[0]->bCurrentObservedLocation
		&& FrameNewerCandidates[0]->Epoch > OlderEpoch)
	{
		// The common rotating-view path has one changing contributor: current.
		// Reject uncovered points before touching geometry, then compare vertical
		// intervals directly without constructing a per-sample interval array.
		if (!HasCurrentObservedContributionAt(Prop, Point)) return false;
		const ADarkwellPropLabFurniture* Actual = Prop.bExists ? Prop.Actual.Get() : nullptr;
		if (!Actual) return false;
		const int32 CurrentIndex = Prop.History.GetCurrentIndex();
		const bool bLocalCurrent = CurrentIndex != INDEX_NONE
			&& Prop.LocalEpoch == Prop.History.GetRecords()[CurrentIndex].Epoch;
		auto TestGeometry = [&](const FPrimitiveGeometrySnapshot& NewerGeometry)
		{
			if (bLocalCurrent && !Prop.CurrentLive.HasObservedContributionAt(
				Point, NewerGeometry.PrimitiveIndex)) return false;
			double NewerMinZ = 0.0;
			double NewerMaxZ = 0.0;
			if (!QueryVerticalInterval(NewerGeometry, Point, NewerMinZ, NewerMaxZ)) return false;
			for (const FPrimitiveGeometrySnapshot& OldGeometry : OlderVisual.PartGeometry)
			{
				double OldMinZ = 0.0;
				double OldMaxZ = 0.0;
				if (QueryVerticalInterval(OldGeometry, Point, OldMinZ, OldMaxZ)
					&& FMath::Min(OldMaxZ, NewerMaxZ)
						+ Darkwell::MovingPropLab::RenderOwnershipContactTolerance
						>= FMath::Max(OldMinZ, NewerMinZ))
				{
					return true;
				}
			}
			return false;
		};
		if (bUseFrameOccupancy)
		{
			for (const FActualOccupancySnapshot& Snapshot : FrameOccupancy)
			{
				if (Snapshot.StableId != Prop.StableId) continue;
				for (const FPrimitiveGeometrySnapshot& Geometry : Snapshot.Geometry)
					if (TestGeometry(Geometry)) return true;
				return false;
			}
		}
		for (const FPrimitiveGeometrySnapshot& Geometry : ActualPartGeometry(*Actual))
			if (TestGeometry(Geometry)) return true;
		return false;
	}
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
 auto ProcessCandidate=[&](const FDarkwellSpatialObservationRecord& Candidate)
 {
		if (Candidate.Epoch <= OlderEpoch)
		{
			return;
		}
		if (Candidate.bCurrentObservedLocation)
		{
			if (const ADarkwellPropLabFurniture* Actual = Prop.bExists ? Prop.Actual.Get() : nullptr)
			{
				bool Found=false;
    if(bUseFrameOccupancy) for(const auto& Snapshot:FrameOccupancy)
     if(Snapshot.StableId==Prop.StableId) { Result.Append(Snapshot.Geometry); Found=true; break; }
    if(!Found) Result.Append(ActualPartGeometry(*Actual));
			}
			return;
		}
		if (const FRecordVisual* Visual = Prop.Visuals.Find(Candidate.Epoch);
			Visual && !Visual->bPresentationRetired)
		{
			Result.Append(Visual->PartGeometry);
		}
 };
 if(bUseNewerCandidates && NewerCandidateId==Prop.StableId)
  { for(const auto* C:FrameNewerCandidates) ProcessCandidate(*C); }
 else { for(const auto& C:Prop.History.GetRecords()) ProcessCandidate(C); }
 return Result;
}

bool ADarkwellMovingPropLabRoom::HasNewerObservedGeometryOverlapWithinFootprint(
	const FTrackedProp& Prop,
	const FRecordVisual& OlderVisual,
	const uint32 OlderEpoch,
	const FBox2D& Footprint) const
{
 if(bUseNewerCandidates && NewerCandidateId==Prop.StableId && (FrameNewerCandidates.IsEmpty() || NewerCandidateMaximumEpoch<=OlderEpoch)) return false;
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
	TRACE_CPUPROFILER_EVENT_SCOPE(Darkwell_GrayHistory_Coverage);
	FScopedHistoryRuntimeTimer CoverageTimer(RuntimeFrame.CoverageUs);
	++RuntimeFrame.OwnershipTests;
	++RuntimeFrame.CoverageFullScans;
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
 const auto Query=Fog->QueryCanonicalCoverageRaster(Bounds,Size,Result.Values,RuntimeFrame.CoverageQueries);
 Result.bValid=Query.bValid; Result.AuthorityRevision=Query.AuthorityRevision; Result.CoverageRevision=Query.CoverageDrawRevision;
 const bool Any=Result.Values.ContainsByPredicate([](float V){return V>=FDarkwellSpatialPropMemory::LegalCoverage;});
 const bool Positive=Result.Values.ContainsByPredicate([](float V){return V>0;});
 Result.ZeroReason=Any?TEXT("NONE"):Positive?TEXT("BELOW_LEGAL_THRESHOLD"):Darkwell::MovingPropLab::CoverageZeroReasonName(Query.ZeroReason);
	return Result;
}

bool ADarkwellMovingPropLabRoom::AdvanceFineHistory(
	FTrackedProp& Prop, FDarkwellSpatialObservationRecord& Record,
	const float DeltaSeconds, const bool bCoverageDirty,
	const TConstArrayView<int32> GeometryDirtyIndices, const uint64 SweepPreviousDrawRevision)
{
	FScopedHistoryRuntimeTimer Timer(RuntimeFrame.AdvanceFineHistoryUs);
	FRecordVisual* Visual = Prop.Visuals.Find(Record.Epoch);
	if (!Visual || !Record.FineHistory.IsInitialized()) return false;
	const FIntPoint Size = Record.FineHistory.GetSize();
	const FBox2D& Bounds = Record.FineHistory.GetBounds();
	const int32 SampleCount = Size.X * Size.Y;
 TBitArray<> DirtyMask(false,SampleCount);
	if (bCoverageDirty)
	{
  const bool Existing=Visual->CachedFineCoverage.Num()==SampleCount;
  bool Reuse=bUseFrameOccupancy && Prop.History.GetRecords().Num()-(Prop.History.GetCurrentIndex()!=INDEX_NONE?1:0)>1;
#if WITH_DEV_AUTOMATION_TESTS
  Reuse &= !bForceFullHistoryEvidenceForTesting;
#endif
  const FHistoryCoverageReuse* Cached=nullptr;
  if(Reuse) for(const auto& Entry:FrameHistoryCoverage)
   if(Entry.Size==Size && Entry.Bounds.Min==Bounds.Min && Entry.Bounds.Max==Bounds.Max
    && Entry.bPreviousValid==Existing && (!Existing || (Entry.PreviousAuthority==Visual->CachedFineAuthorityRevision && Entry.PreviousDraw==Visual->CachedFineDrawRevision)))
   { Cached=&Entry; break; }
  if(Cached)
  {
   Visual->CachedFineCoverage=Cached->Values; DirtyMask=Cached->Crossings;
   Visual->CachedFineAuthorityRevision=Cached->Authority; Visual->CachedFineDrawRevision=Cached->Draw;
   ++RuntimeFrame.HistoryCoverageReuseHits;
  }
  else
  {
   const FCoverageSnapshot Coverage=SampleConservativeCoverage(Bounds,Record.Epoch,
    Record.SpatialMemory.GetGeneration(),FDarkwellHistoryGridV2::SamplesPerCell);
   if(!Coverage.bValid) return false;
   for(int32 I=0;I<SampleCount;++I)
    if(!Existing || (Coverage.Values[I]>=FDarkwellSpatialPropMemory::LegalCoverage)!=(Visual->CachedFineCoverage[I]>=FDarkwellSpatialPropMemory::LegalCoverage)) DirtyMask[I]=true;
   if(Reuse && FrameHistoryCoverage.Num()<64)
   {
    auto& Entry=FrameHistoryCoverage.AddDefaulted_GetRef(); Entry.Bounds=Bounds; Entry.Size=Size; Entry.bPreviousValid=Existing;
    Entry.PreviousAuthority=Visual->CachedFineAuthorityRevision; Entry.PreviousDraw=Visual->CachedFineDrawRevision;
    Entry.Authority=Coverage.AuthorityRevision; Entry.Draw=Coverage.CoverageRevision;
    Entry.Values=Coverage.Values; Entry.Crossings=DirtyMask;
   }
   Visual->CachedFineCoverage=Coverage.Values;
   Visual->CachedFineAuthorityRevision=Coverage.AuthorityRevision; Visual->CachedFineDrawRevision=Coverage.CoverageRevision;
  }
	}
	if (Visual->CachedFineCoverage.Num() != SampleCount) return false;
	if (Visual->CachedFineOccupied.Num() != SampleCount)
		Visual->CachedFineOccupied.Init(false, SampleCount);

#if WITH_DEV_AUTOMATION_TESTS
 if(bForceFullHistoryEvidenceForTesting && bCoverageDirty) DirtyMask.Init(true,SampleCount);
#endif
	for (const int32 Index : GeometryDirtyIndices)
	{
		if (DirtyMask.IsValidIndex(Index)) DirtyMask[Index] = true;
	}
 bool FilterTerminal=true;
#if WITH_DEV_AUTOMATION_TESTS
 FilterTerminal=!bForceFullHistoryEvidenceForTesting;
#endif
 if(FilterTerminal) Record.FineHistory.FilterMutableEvidence(DirtyMask);
	TArray<int32> DirtyIndices;
	DirtyIndices.Reserve(bCoverageDirty ? SampleCount : GeometryDirtyIndices.Num());
	for (TConstSetBitIterator<> It(DirtyMask); It; ++It) DirtyIndices.Add(It.GetIndex());

	TArray<float> SweepEvidence;
	const auto* Fog = GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>();
	FDarkwellFogVisualSourceSnapshot PreviousSource, CurrentSource;
	TConstArrayView<FDarkwellFogVisualSegment> Occluders;
	const bool bSupportedSweep = bCoverageDirty && Fog && Fog->GetHistoricalRotationSweep(
		SweepPreviousDrawRevision,PreviousSource,CurrentSource,Occluders);
	// Count conservative refusals (invalid/non-adjacent source, geometry motion,
	// translated origin or ambiguous turn); never retry with newer world data.
	if (bCoverageDirty && !bSupportedSweep) ++RuntimeFrame.SweepUnsupportedEvents;
 bool bSweepMayAdd=bSupportedSweep && FDarkwellHistoricalVisibilitySweep::MayAddIntermediateSamples(PreviousSource,CurrentSource,Bounds);
#if WITH_DEV_AUTOMATION_TESTS
 if(bForceFullHistoryEvidenceForTesting) bSweepMayAdd=bSupportedSweep && FDarkwellHistoricalVisibilitySweep::MayAffectBounds(PreviousSource,CurrentSource,Bounds);
#endif
	if (bSweepMayAdd)
	{
		FScopedHistoryRuntimeTimer SweepTimer(RuntimeFrame.SweepProofUs);
		const FVector2D Step = Bounds.GetSize() / FVector2D(Size.X,Size.Y);
        TBitArray<> SweepCandidates(true,SampleCount);
        if(FilterTerminal) Record.FineHistory.FilterMutableEvidence(SweepCandidates);
        for(TConstSetBitIterator<> Candidate(SweepCandidates);Candidate;++Candidate)
		{
            const int32 Index=Candidate.GetIndex();
			const auto& Sample = Record.FineHistory.GetSamples()[Index];
			if (Sample.bVerifiedEmpty || Sample.State == FDarkwellHistoryGridV2::Superseded()
				|| Visual->CachedFineOccupied[Index] || Visual->SuppressedByCurrentEvidence[Index]
				|| Visual->CachedFineCoverage[Index] >= FDarkwellSpatialPropMemory::LegalCoverage) continue;
			const FVector2D Min = Bounds.Min + Step * FVector2D(Index % Size.X,Index / Size.X);
			const FBox2D Footprint(Min,Min+Step);
			if (!FDarkwellHistoricalVisibilitySweep::MayAffectBounds(PreviousSource,CurrentSource,Footprint)) continue;
			// Per-room event budget. Extreme input fails conservatively, never queues
			// unbounded substep scans or replays old visibility with newer geometry.
			if (RuntimeFrame.SweepCandidateSamples >= 65536) { ++RuntimeFrame.SweepBudgetRejects; continue; }
			++RuntimeFrame.SweepCandidateSamples;
			uint64 Queries = 0;
			const bool bProven = FDarkwellHistoricalVisibilitySweep::ProveEmptyFootprintCoverage(
				PreviousSource,CurrentSource,Occluders,Footprint,Queries);
			RuntimeFrame.SweepCoverageQueries += Queries; RuntimeFrame.CoverageQueries += Queries;
			if (bProven)
			{
				if (SweepEvidence.IsEmpty()) SweepEvidence = Visual->CachedFineCoverage;
				SweepEvidence[Index] = 1; if(!DirtyMask[Index]) { DirtyMask[Index]=true; DirtyIndices.Add(Index); } ++RuntimeFrame.SweepAcceptedSamples;
			}
		}
	}
	RuntimeFrame.FineSamplesScanned += DirtyIndices.Num()+Record.FineHistory.GetActiveTransitionCount();
 bool bTopologyChanged = false;
	// Evidence owns historical output; the old coarse fields remain diagnostic.
	const bool bPresentationChanged = Record.FineHistory.AdvanceDirty(
		DeltaSeconds, SweepEvidence.IsEmpty() ? Visual->CachedFineCoverage : SweepEvidence, Visual->CachedFineOccupied,
		Visual->SuppressedByCurrentEvidence, DirtyIndices, bTopologyChanged);
	Visual->bPresentationDirty |= bPresentationChanged;
	Visual->bCapTopologyDirty |= bTopologyChanged;
	return bPresentationChanged || bTopologyChanged;
}

void ADarkwellMovingPropLabRoom::GetFineEvidenceDiagnosticsForTesting(
	const FName StableId, TArray<FFineEvidenceDiagnostic>& Out) const
{
	Out.Reset();
	const FTrackedProp* Prop = Tracked.Find(StableId);
	const auto* Fog = GetWorld() ? GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>() : nullptr;
	if (!Prop || !Fog) return;
	for (const auto& Record : Prop->History.GetRecords())
	{
		if (!Record.FineHistory.IsInitialized()) continue;
		const FRecordVisual* Visual = Prop->Visuals.Find(Record.Epoch);
		if (!Visual) continue;
		const auto Size = Record.FineHistory.GetSize();
		const auto Bounds = Record.FineHistory.GetBounds();
		const FVector2D Step = Bounds.GetSize() / FVector2D(Size.X, Size.Y);
		for (int32 I = 0; I < Record.FineHistory.GetSamples().Num(); ++I)
		{
			auto& D = Out.AddDefaulted_GetRef();
			D.Epoch = Record.Epoch; D.Index = I;
			D.Position = Bounds.Min + Step * FVector2D(I % Size.X + .5, I / Size.X + .5);
			D.Sample = Record.FineHistory.GetSamples()[I];
			D.bValid = Visual->CachedFineCoverage.IsValidIndex(I)
				&& Visual->CachedCoverageAuthorityRevision == Fog->GetDiagnostics().LastAuthorityRevision
				&& Visual->CachedCoverageDrawRevision == Fog->GetDiagnostics().CoverageDrawCount;
			D.Coverage = D.bValid ? Visual->CachedFineCoverage[I] : 0;
			D.bOccupied = Visual->CachedFineOccupied.IsValidIndex(I) && Visual->CachedFineOccupied[I];
			D.bOwned = Visual->SuppressedByCurrentEvidence.IsValidIndex(I) && Visual->SuppressedByCurrentEvidence[I];
			D.bSubmitted = !Visual->bPresentationRetired && Visual->SubmittedPresentation.IsValidIndex(I)
				&& Visual->SubmittedPresentation[I].A > 0 && Visual->SubmittedPresentation[I].B > 0;
		}
	}
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
		Result += FString::Printf(TEXT("epoch=%u fine=%dx%d never=%d unresolved=%d empty=%d superseded=%d mixed=%d oldBlockedNewEmpty=%d hash=%llu sample_bytes=%llu state_hash=%llu;"),
			Record.Epoch, Fine.X, Fine.Y, Grid.Count(Grid.NeverObserved()), Grid.Count(Grid.Unresolved()),
			Grid.Count(Grid.VerifiedEmpty()), Grid.Count(Grid.Superseded()), Grid.CountMixedCoarseCells(), OldBlockedNewEmpty,
			Grid.EvidenceHash(), uint64(Grid.GetSamples().Num()) * sizeof(FDarkwellHistoryGridV2::FSample), Grid.StateHash());
	}
	return Result;
}

FString ADarkwellMovingPropLabRoom::GetHistoryRuntimeTelemetry() const
{
	auto Format = [](const FHistoryRuntimeTelemetry& T)
	{
		return FString::Printf(
			TEXT("{\"frame\":%llu,\"frames\":%llu,\"epochs\":%d,\"candidates\":%d,\"sleeping\":%d,\"dirty_tiles\":%d,\"resident_samples\":%d,\"samples_scanned\":%llu,\"coverage_scans\":%llu,\"coverage_queries\":%llu,\"occlusion_only_queries\":%llu,\"occupancy_tests\":%llu,\"geometry_tests\":%llu,\"ownership_tests\":%llu,\"occupancy_hits\":%llu,\"history_geometry_reuse\":%llu,\"history_ownership_reuse\":%llu,\"history_coverage_reuse\":%llu,\"occupancy_samples_reused\":%llu,\"texture_creations\":%llu,\"mid_creations\":%llu,\"gpu_texture_uploads\":%llu,\"texture_calls\":%llu,\"texture_uploads\":%llu,\"cap_calls\":%llu,\"cap_rebuilds\":%llu,\"refresh_us\":%.3f,\"rotation_log_us\":%.3f,\"report_us\":%.3f,\"fine_advance_us\":%.3f,\"tracked_us\":%.3f,\"coverage_us\":%.3f,\"occupancy_us\":%.3f,\"ownership_us\":%.3f,\"texture_us\":%.3f,\"cap_us\":%.3f,\"current_reveal_us\":%.3f,\"candidate_us\":%.3f,\"historical_us\":%.3f,\"occupancy_snapshot_us\":%.3f,\"game_thread_us\":%.3f,\"proxies\":%d,\"caps\":%d,\"textures\":%d,\"mids\":%d,\"fine_bytes\":%llu,\"records\":%d,\"working_set\":%llu,\"uobjects\":%d,\"sweep_candidates\":%llu,\"sweep_queries\":%llu,\"sweep_accepted\":%llu,\"sweep_budget_rejects\":%llu,\"sweep_unsupported_events\":%llu,\"sweep_proof_us\":%.3f,\"sweep_uniform_substeps\":0}"),
			T.FrameNumber, T.FramesAccumulated, T.ActiveHistoricalEpochs,
			T.CandidateHistoricalEpochs, T.SleepingHistoricalEpochs, T.DirtyTileCount,
			T.FineSamplesResident, T.FineSamplesScanned, T.CoverageFullScans,
			T.CoverageQueries, T.OcclusionOnlyQueries,
			T.OccupancyTests, T.PrimitiveGeometryTests,
			T.OwnershipTests, T.OccupancyCacheHits, T.HistoryGeometryReuseHits,
            T.HistoryOwnershipReuseHits, T.HistoryCoverageReuseHits, T.HistoryOccupancySamplesReused,
			T.TextureCreations, T.MidCreations, T.GpuTextureUploads,
            T.UpdateRecordTextureCalls, T.TextureUploads,
			T.UpdateRecordCapCalls, T.CapMeshRebuilds,
			T.RefreshContributionDiagnosticsUs, T.LogRotationFrameUs,
			T.ReportHudUs, T.AdvanceFineHistoryUs, T.UpdateTrackedUs,
			T.CoverageUs, T.OccupancyUs, T.OwnershipUs,
			T.TextureSubmissionUs, T.CapPresentationUs, T.CurrentRevealUs,
			T.HistoricalCandidateUs, T.HistoricalEvidenceUs, T.OccupancySnapshotUs,
			T.MovingPropLabGameThreadUs, T.ProxyCount, T.CapComponentCount,
			T.TextureCount, T.MidCount, T.FineHistoryResidentBytes,
			T.SpatialRecordCount, T.ProcessWorkingSetBytes, T.UObjectCount,
			T.SweepCandidateSamples,T.SweepCoverageQueries,T.SweepAcceptedSamples,T.SweepBudgetRejects,
			T.SweepUnsupportedEvents,T.SweepProofUs);
	};
	return FString::Printf(TEXT("{\"frame_data\":%s,\"window_total\":%s}"),
		*Format(RuntimeFrame), *Format(RuntimeTotal));
}

void ADarkwellMovingPropLabRoom::ResetHistoryRuntimeTelemetryForTesting()
{
	RuntimeFrame = FHistoryRuntimeTelemetry();
	RuntimeTotal = FHistoryRuntimeTelemetry();
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
	ADarkwellPropLabFurniture* Actual = Prop ? Prop->Actual.Get() : nullptr;
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
	ADarkwellPropLabFurniture* Actual = Prop ? Prop->Actual.Get() : nullptr;
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
	Actual->Memory->ApplySourceGeometryVisibility(false);
	Actual->SetActorEnableCollision(false);
	Actual->SetActorHiddenInGame(true);
	Prop->bExists = false;
	bHistoricalSpatialIndexDirty = true;
	ResetHistoryRuntimeTelemetryForTesting();
	return Prop->History.GetRecords().Num() == HistoricalEpochs;
}

FString ADarkwellMovingPropLabRoom::GetMultiEpochCompositeDiagnosis(
	const FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	if (!Prop)
	{
		return TEXT("A=0 B=0 C=0 D=0 OTHER=0 MISSING=1");
	}
	int32 A = 0, B = 0, C = 0, D = 0, Other = 0;
	TArray<FString> Samples;
	uint32 NewestEpoch = 0;
	for (const FDarkwellSpatialObservationRecord& Candidate : Prop->History.GetRecords())
	{
		NewestEpoch = FMath::Max(NewestEpoch, Candidate.Epoch);
	}
	for (const FDarkwellSpatialObservationRecord& Record : Prop->History.GetRecords())
	{
		if (Record.bCurrentObservedLocation || !Record.FineHistory.IsInitialized())
		{
			continue;
		}
		const FRecordVisual* Visual = Prop->Visuals.Find(Record.Epoch);
		if (!Visual || Visual->bPresentationRetired)
		{
			continue;
		}
		const FCoverageSnapshot Coverage = SampleConservativeCoverage(
			Record.FineHistory.GetBounds(), Record.Epoch,
			Record.SpatialMemory.GetGeneration(), FDarkwellHistoryGridV2::SamplesPerCell);
		const FIntPoint Size = Record.FineHistory.GetSize();
		const FBox2D& Bounds = Record.FineHistory.GetBounds();
		const FVector2D Step = Bounds.GetSize() / FVector2D(Size.X, Size.Y);
		for (int32 Index = 0; Index < Record.FineHistory.GetSamples().Num(); ++Index)
		{
			const FDarkwellHistoryGridV2::FSample& State = Record.FineHistory.GetSamples()[Index];
			const bool bSubmitted = Visual->SubmittedPresentation.IsValidIndex(Index)
				&& Visual->SubmittedPresentation[Index].A > 0.0f
				&& Visual->SubmittedPresentation[Index].B > 0.0f;
			if (!bSubmitted)
			{
				continue;
			}
			const int32 X = Index % Size.X;
			const int32 Y = Index / Size.X;
			const FVector2D Minimum = Bounds.Min + Step * FVector2D(X, Y);
			const FVector2D Point = Minimum + Step * 0.5f;
			const bool bCoverageValid = Coverage.bValid && Coverage.Values.IsValidIndex(Index);
			const float LegalCoverage = bCoverageValid ? Coverage.Values[Index] : 0.0f;
			const bool bLegal = bCoverageValid
				&& LegalCoverage >= FDarkwellSpatialPropMemory::LegalCoverage;
			const bool bOccupied = IsOccupiedByActual(Point, NAME_None);
			const bool bOwnership = State.State == FDarkwellHistoryGridV2::Superseded()
				|| (Visual->SuppressedByCurrentEvidence.IsValidIndex(Index)
					&& Visual->SuppressedByCurrentEvidence[Index])
				|| HasNewerObservedGeometryOverlapWithinFootprint(
					*Prop, *Visual, Record.Epoch, FBox2D(Minimum, Minimum + Step));
			TCHAR Classification = TEXT('O');
			if (State.State == FDarkwellHistoryGridV2::VerifiedEmpty()
				|| State.State == FDarkwellHistoryGridV2::Superseded())
			{
				++D; Classification = TEXT('D');
			}
			else if (bOwnership)
			{
				++C; Classification = TEXT('C');
			}
			else if (State.State == FDarkwellHistoryGridV2::Unresolved() && !bOccupied && bLegal)
			{
				++A; Classification = TEXT('A');
			}
			else if (State.State == FDarkwellHistoryGridV2::Unresolved() && !bOccupied && !bLegal)
			{
				++B; Classification = TEXT('B');
			}
			else
			{
				++Other;
			}
			if (Samples.Num() < 64)
			{
				const TCHAR* StateName = State.State == FDarkwellHistoryGridV2::NeverObserved()
					? TEXT("NeverObserved") : State.State == FDarkwellHistoryGridV2::Unresolved()
						? TEXT("Unresolved") : State.State == FDarkwellHistoryGridV2::VerifiedEmpty()
							? TEXT("VerifiedEmpty") : TEXT("SupersededByNewerEvidence");
				Samples.Add(FString::Printf(
					TEXT("class=%c epoch=%u xy=(%.3f,%.3f) state=%s opacity=%.4f coverage=%.4f coverage_valid=%d occupied=%d ownership=%d newer_epoch=%u geometry_overlap=%d initial=%.4f verified_empty=%d"),
					Classification, Record.Epoch, Point.X, Point.Y, StateName,
					State.Opacity, LegalCoverage, bCoverageValid ? 1 : 0,
					bOccupied ? 1 : 0, bOwnership ? 1 : 0,
					bOwnership ? NewestEpoch : 0, bOwnership ? 1 : 0,
					State.InitialRemembered, State.bVerifiedEmpty ? 1 : 0));
			}
		}
	}
	return FString::Printf(TEXT("A=%d B=%d C=%d D=%d OTHER=%d surviving_visible=%d\n%s"),
		A, B, C, D, Other, A + B + C + D + Other, *FString::Join(Samples, TEXT("\n")));
}

bool ADarkwellMovingPropLabRoom::IsOccupiedByActual(
	const FVector2D Point,
	const FName IgnoredStableId) const
{
	++RuntimeFrame.OccupancyTests;
 if(bUseFrameOccupancy)
 {
  if(bFilterFrameOccupancy && FrameOccupancyCandidates.IsEmpty()) return false;
  // Every physical pose is fixed for this UpdateRoom. Historical ROI candidates
  // include every actor that can contain this point; cache exact coordinates only.
  bool UseCache=bCacheFrameOccupancyPoints && IgnoredStableId.IsNone();
#if WITH_DEV_AUTOMATION_TESTS
  UseCache &= !bForceFullHistoryEvidenceForTesting;
#endif
  if(UseCache) if(const bool* Cached=FrameOccupancyPoints.Find(Point)) { ++RuntimeFrame.OccupancyCacheHits; return *Cached; }
  auto Remember=[&](bool Value) { if(UseCache && FrameOccupancyPoints.Num()<131072) FrameOccupancyPoints.Add(Point,Value); return Value; };
  auto Occupies=[&](const FActualOccupancySnapshot& S)
  {
   if(S.StableId==IgnoredStableId || !S.Bounds.IsInside(Point)) return false;
   for(const auto& Geometry:S.Geometry) { double MinZ,MaxZ; if(QueryVerticalInterval(Geometry,Point,MinZ,MaxZ)) return true; }
   return false;
  };
  if(bFilterFrameOccupancy) { for(const auto* S:FrameOccupancyCandidates) if(Occupies(*S)) return Remember(true); }
  else for(const auto& S:FrameOccupancy) if(Occupies(S)) return Remember(true);
  return Remember(false);
 }
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
    if(Prop.LocalEpoch==Current.Epoch)
        return Prop.bLastCoverageValid && Current.SnapshotTransform.Equals(Actual->GetActorTransform())
            && Prop.CurrentLive.HasObservedContributionAt(Point);
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

void ADarkwellMovingPropLabRoom::BuildGeometryDirtyIndices(
	const FTrackedProp& Prop, FDarkwellSpatialObservationRecord& Record,
	FRecordVisual& Visual, TArray<int32>& OutDirtyIndices, TArray<int32>& OutPhysicalDirtyIndices)
{
	OutDirtyIndices.Reset();
	OutPhysicalDirtyIndices.Reset();
	if (!Record.FineHistory.IsInitialized()
		|| (Visual.ProcessedGeometryRevision == GeometryRevision
			&& Visual.ProcessedOwnershipRevision == Prop.ObservationOwnershipRevision))
	{
		return;
	}
 TArray<FPrimitiveGeometrySnapshot> PhysicalGeometry;
 if(bUseFrameOccupancy) for(const auto& S:FrameOccupancy) PhysicalGeometry.Append(S.Geometry);
 else for(const auto& Pair:Tracked) if(const auto* A=Pair.Value.bExists?Pair.Value.Actual.Get():nullptr; A && A->GetActorEnableCollision()) PhysicalGeometry.Append(ActualPartGeometry(*A));
 auto NewerGeometry=CollectNewerGeometrySnapshots(Prop,Record.Epoch);
	const FIntPoint Size = Record.FineHistory.GetSize();
	const FBox2D& Bounds = Record.FineHistory.GetBounds();
	const FVector2D Step = Bounds.GetSize() / FVector2D(Size.X, Size.Y);
 bool Reuse=bUseFrameOccupancy && Prop.History.GetRecords().Num()-(Prop.History.GetCurrentIndex()!=INDEX_NONE?1:0)>1;
#if WITH_DEV_AUTOMATION_TESTS
 Reuse &= !bForceFullHistoryEvidenceForTesting;
#endif
 auto SameGeometry=[](const TArray<FPrimitiveGeometrySnapshot>& A,const TArray<FPrimitiveGeometrySnapshot>& B)
 {
  if(A.Num()!=B.Num()) return false;
  for(int32 I=0;I<A.Num();++I)
   if(A[I].PrimitiveIndex!=B[I].PrimitiveIndex || A[I].LocalBounds.Min!=B[I].LocalBounds.Min
    || A[I].LocalBounds.Max!=B[I].LocalBounds.Max || !A[I].WorldTransform.Equals(B[I].WorldTransform,0)) return false;
  return true;
 };
 if(Reuse) for(const auto& Cached:FrameHistoryGeometry)
  if(Cached.StableId==Prop.StableId && Cached.Size==Size && Cached.Bounds.Min==Bounds.Min && Cached.Bounds.Max==Bounds.Max
   && Cached.PreviousGeometryRevision==Visual.ProcessedGeometryRevision && Cached.PreviousOwnershipRevision==Visual.ProcessedOwnershipRevision
   && SameGeometry(Cached.BeforePhysical,Visual.CachedPhysicalGeometry)
   && SameGeometry(Cached.BeforeNewer,Visual.CachedNewerGeometry) && SameGeometry(Cached.AfterNewer,NewerGeometry))
  {
   Visual.CachedFineOccupied=Cached.Occupied; OutDirtyIndices=Cached.DirtyIndices; OutPhysicalDirtyIndices=Cached.PhysicalDirtyIndices; RuntimeFrame.HistoryOccupancySamplesReused+=OutDirtyIndices.Num();
   Visual.CachedPhysicalGeometry=MoveTemp(PhysicalGeometry); Visual.CachedNewerGeometry=MoveTemp(NewerGeometry);
   Visual.ProcessedGeometryRevision=GeometryRevision; Visual.ProcessedOwnershipRevision=Prop.ObservationOwnershipRevision;
   ++RuntimeFrame.HistoryGeometryReuseHits;
   return;
  }
 // Cache size bounds scratch memory only. A full cache uses the original path.
 FHistoryGeometryReuse* Cached=nullptr;
 if(Reuse && FrameHistoryGeometry.Num()<64)
 {
  Cached=&FrameHistoryGeometry.AddDefaulted_GetRef(); Cached->StableId=Prop.StableId; Cached->Bounds=Bounds; Cached->Size=Size;
  Cached->PreviousGeometryRevision=Visual.ProcessedGeometryRevision; Cached->PreviousOwnershipRevision=Visual.ProcessedOwnershipRevision;
  Cached->BeforePhysical=Visual.CachedPhysicalGeometry; Cached->BeforeNewer=Visual.CachedNewerGeometry; Cached->AfterNewer=NewerGeometry;
 }

	TBitArray<> Dirty(false, Size.X * Size.Y);
	auto MarkRegion = [&](const FBox2D& Region)
	{
		if (!Region.bIsValid || !Bounds.Intersect(Region)) return;
		const FVector2D RelativeMin = (Region.Min - Bounds.Min) / Step;
		const FVector2D RelativeMax = (Region.Max - Bounds.Min) / Step;
		const int32 MinX = FMath::Clamp(FMath::FloorToInt(RelativeMin.X) - 1, 0, Size.X - 1);
		const int32 MinY = FMath::Clamp(FMath::FloorToInt(RelativeMin.Y) - 1, 0, Size.Y - 1);
		const int32 MaxX = FMath::Clamp(FMath::CeilToInt(RelativeMax.X) + 1, 0, Size.X - 1);
		const int32 MaxY = FMath::Clamp(FMath::CeilToInt(RelativeMax.Y) + 1, 0, Size.Y - 1);
		for (int32 Y = MinY; Y <= MaxY; ++Y)
			for (int32 X = MinX; X <= MaxX; ++X) Dirty[Y * Size.X + X] = true;
	};
 auto GeometryRegion=[](const FPrimitiveGeometrySnapshot& G) { const auto B=G.LocalBounds.TransformBy(G.WorldTransform); return FBox2D(FVector2D(B.Min),FVector2D(B.Max)); };
 auto MarkChanged=[&](const TArray<FPrimitiveGeometrySnapshot>& Before,const TArray<FPrimitiveGeometrySnapshot>& After,double Tolerance)
 {
  const int32 N=FMath::Max(Before.Num(),After.Num());
  for(int32 I=0;I<N;++I)
  {
   if(Before.IsValidIndex(I) && After.IsValidIndex(I) && Before[I].LocalBounds.Equals(After[I].LocalBounds,Tolerance)
    && Before[I].WorldTransform.Equals(After[I].WorldTransform,Tolerance)) continue;
   if(Before.IsValidIndex(I)) MarkRegion(GeometryRegion(Before[I]));
   if(After.IsValidIndex(I)) MarkRegion(GeometryRegion(After[I]));
  }
 };
 // Occupancy depends only on physical geometry. Ownership/view changes still
 // dirty evidence, but cannot invalidate a previously exact occupancy result.
 // Exact physical comparison also prevents sub-tolerance motion from leaving a
 // stale occupancy cache that a later ownership-only update would reuse.
 if(Visual.ProcessedGeometryRevision==0 || Visual.CachedFineOccupied.Num()!=Size.X*Size.Y) Dirty.Init(true,Size.X*Size.Y);
 else MarkChanged(Visual.CachedPhysicalGeometry,PhysicalGeometry,0);
 TBitArray<> PhysicalDirty=Dirty;
 if(Visual.ProcessedGeometryRevision!=0)
 {
  if(Visual.ProcessedOwnershipRevision!=Prop.ObservationOwnershipRevision)
  {
   if(!Prop.CurrentLive.OwnershipDirtyRegions.IsEmpty())
   {
    for(const FBox2D& Region : Prop.CurrentLive.OwnershipDirtyRegions) MarkRegion(Region);
   }
   else
   {
    MarkChanged(Visual.CachedNewerGeometry,NewerGeometry,1.e-6);
   }
  }
 }
#if WITH_DEV_AUTOMATION_TESTS
 if(bForceFullHistoryEvidenceForTesting) PhysicalDirty=Dirty;
#endif
 Visual.CachedPhysicalGeometry=MoveTemp(PhysicalGeometry); Visual.CachedNewerGeometry=MoveTemp(NewerGeometry);
 Visual.ProcessedGeometryRevision=GeometryRevision;
 Visual.ProcessedOwnershipRevision=Prop.ObservationOwnershipRevision;
	if (Visual.CachedFineOccupied.Num() != Size.X * Size.Y)
		Visual.CachedFineOccupied.Init(false, Size.X * Size.Y);
	for (TConstSetBitIterator<> It(Dirty); It; ++It)
	{
		const int32 Index = It.GetIndex();
        if(PhysicalDirty[Index])
        {
            const int32 X = Index % Size.X;
            const int32 Y = Index / Size.X;
            const FVector2D Point = Bounds.Min + Step * FVector2D(X + 0.5, Y + 0.5);
            Visual.CachedFineOccupied[Index] = IsOccupiedByActual(Point, NAME_None);
            OutPhysicalDirtyIndices.Add(Index);
        }
        else ++RuntimeFrame.HistoryOccupancySamplesReused;
		OutDirtyIndices.Add(Index);
	}
 if(Cached) { Cached->Occupied=Visual.CachedFineOccupied; Cached->DirtyIndices=OutDirtyIndices; Cached->PhysicalDirtyIndices=OutPhysicalDirtyIndices; }
}

bool ADarkwellMovingPropLabRoom::UpdateHistoricalContributionExclusion(
	FTrackedProp& Prop,
	FDarkwellSpatialObservationRecord& Record,
	const TConstArrayView<int32> DirtyIndices)
{
	if (Record.bCurrentObservedLocation)
	{
		return false;
	}
	FRecordVisual* Visual = Prop.Visuals.Find(Record.Epoch);
	if (!Visual)
	{
		return false;
	}
	const FIntPoint Size = Record.SpatialMemory.GetSize()
		* Darkwell::MovingPropLab::PresentationSamples;
	if (Size.X <= 0 || Size.Y <= 0)
	{
		return false;
	}
	if (Visual->SuppressedByCurrentEvidence.Num() != Size.X * Size.Y)
	{
		Visual->SuppressedByCurrentEvidence.Init(false, Size.X * Size.Y);
	}
	bool bChanged = false;
	const FBox2D& Bounds = Record.SpatialMemory.GetBounds();
	const FVector2D Step = Bounds.GetSize() / FVector2D(Size.X, Size.Y);
 FHistoryOwnershipReuse* Cached=nullptr;
 bool Reuse=bUseFrameOccupancy && Prop.History.GetRecords().Num()>2 && bUseNewerCandidates && NewerCandidateId==Prop.StableId;
#if WITH_DEV_AUTOMATION_TESTS
 Reuse &= !bForceFullHistoryEvidenceForTesting;
#endif
 if(Reuse)
 {
  TArray<uint32> NewerEpochs;
  for(const auto* C:FrameNewerCandidates)
   if(C->Epoch>Record.Epoch && (C->bCurrentObservedLocation || (Prop.Visuals.Find(C->Epoch) && !Prop.Visuals.FindChecked(C->Epoch).bPresentationRetired))) NewerEpochs.Add(C->Epoch);
  if(NewerEpochs.IsEmpty()) return false;
  auto SameOldGeometry=[&](const TArray<FPrimitiveGeometrySnapshot>& A)
  {
   const auto& B=Visual->PartGeometry;
   if(A.Num()!=B.Num()) return false;
   for(int32 I=0;I<A.Num();++I)
    if(A[I].PrimitiveIndex!=B[I].PrimitiveIndex || A[I].LocalBounds.Min!=B[I].LocalBounds.Min || A[I].LocalBounds.Max!=B[I].LocalBounds.Max
     || !A[I].WorldTransform.Equals(B[I].WorldTransform,0)) return false;
   return true;
  };
  // History updates run in strictly increasing epoch order. The matching set
  // of newer candidates has not yet been updated for any reader in this phase.
  // Current was already updated. This cache is never queried by later diagnostics.
  for(auto& Entry:FrameHistoryOwnership)
   if(Entry.StableId==Prop.StableId && Entry.Size==Size && Entry.Bounds.Min==Bounds.Min && Entry.Bounds.Max==Bounds.Max
    && Entry.NewerEpochs==NewerEpochs && SameOldGeometry(Entry.OldGeometry)) { Cached=&Entry; break; }
  if(!Cached && FrameHistoryOwnership.Num()<64)
  {
   Cached=&FrameHistoryOwnership.AddDefaulted_GetRef(); Cached->StableId=Prop.StableId; Cached->Bounds=Bounds; Cached->Size=Size;
   Cached->OldGeometry=Visual->PartGeometry; Cached->NewerEpochs=MoveTemp(NewerEpochs);
   Cached->Evaluated.Init(false,Size.X*Size.Y); Cached->Overlap.Init(false,Size.X*Size.Y);
  }
 }

	for (const int32 Index : DirtyIndices)
	{
		if (!Visual->SuppressedByCurrentEvidence.IsValidIndex(Index)
			|| Visual->SuppressedByCurrentEvidence[Index])
		{
			continue;
		}
		const int32 X = Index % Size.X;
		const int32 Y = Index / Size.X;
		const FVector2D Minimum = Bounds.Min + Step * FVector2D(X, Y);
  bool Overlap=false;
  if(Cached && Cached->Evaluated[Index]) { Overlap=Cached->Overlap[Index]; ++RuntimeFrame.HistoryOwnershipReuseHits; }
  else
  {
   Overlap=HasNewerObservedGeometryOverlapWithinFootprint(Prop,*Visual,Record.Epoch,FBox2D(Minimum,Minimum+Step));
   if(Cached) { Cached->Evaluated[Index]=true; Cached->Overlap[Index]=Overlap; }
  }
		if (Overlap)
		{
			// This is a monotonic presentation-ownership decision backed by new
			// legal present evidence. It does not mark the old cell verified empty.
			Visual->SuppressedByCurrentEvidence[Index] = true;
			bChanged = true;
		}
	}
	return bChanged;
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
	if (Record.FineHistory.IsInitialized()) return !Record.FineHistory.HasResidualSurface();
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
	// Retirement releases presentation resources only. The immutable terminal
	// evidence remains resident and keeps compact broad-phase membership so a
	// nearby observer still receives the same diagnostic authority updates.
	DestroyVisual(Visual, false);
	Visual.bPresentationRetired = true;
	bHistoricalSpatialIndexDirty = true;
}

void ADarkwellMovingPropLabRoom::RefreshContributionDiagnostics(
	FTrackedProp& Prop) const
{
	FScopedHistoryRuntimeTimer Timer(RuntimeFrame.RefreshContributionDiagnosticsUs);
 // Only rendered state and this identity's geometry affect these diagnostics.
 // Authority revisions alone cannot change any projected/3D contributor count.
 uint64 Signature=1469598103934665603ull;
 auto Mix=[&](uint64 V){Signature=(Signature^V)*1099511628211ull;};
 Mix(Prop.TransformRevision); Mix(Prop.GridRevision); Mix(Prop.bExists); Mix(Prop.bLastCoverageValid);
 Mix(Prop.History.GetCurrentIndex()+1); Mix(Prop.CurrentLive.StateHash());
 if(const auto* A=Prop.Actual.Get()) for(const UStaticMeshComponent* M:A->Memory->GetMemoryPrimitives()) Mix(M && M->IsVisible());
 for(const auto& R:Prop.History.GetRecords())
 {
  Mix(R.Epoch); Mix(R.bCurrentObservedLocation); Mix(R.SpatialMemory.GetGeneration());
  if(const auto* V=Prop.Visuals.Find(R.Epoch))
  {
   Mix(V->bPresentationRetired); Mix(V->TextureSignature);
   Mix(V->Proxy.IsValid() && !V->Proxy->IsHidden()); Mix(V->Cap.IsValid() && V->Cap->IsVisible());
   Mix(V->CapTriangles); if(V->CapTriangles>0) Mix(V->CapSignature);
   for(auto H:V->LiveSignatures) Mix(H);
  }
 }
 if(Prop.DiagnosticSignature==Signature) return;
 Prop.DiagnosticSignature=Signature;

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
 // One visible surface without cap geometry cannot form a contributor pair,
 // even when this identity retains retired evidence records for future proofs.
 int32 VisibleSurfaces=IsCurrentSourceVisibleForTesting(Prop.StableId)?1:0;
 bool AnyCap=false;
 for(const auto& Pair:Prop.Visuals)
 {
  const auto& V=Pair.Value;
  AnyCap |= V.CapTriangles>0 && V.Cap.IsValid() && V.Cap->IsVisible();
  VisibleSurfaces += V.Proxy.IsValid() && !V.Proxy->IsHidden()?1:0;
 }
 if(!AnyCap && VisibleSurfaces<=1)
 {
  Prop.MaxSurfaceContributors=VisibleSurfaces;
  Prop.MaxTotalContributors=VisibleSurfaces; Prop.MaxOverlapContributors=VisibleSurfaces;
  return;
 }
	TArray<FVector2D> SamplePoints;
	auto SurfaceContributorsAt = [&](const FVector2D Point)
	{
		int32 Contributors = HasCurrentObservedContributionAt(Prop, Point) ? 1 : 0;
		auto CountHistorical = [&](const FDarkwellSpatialObservationRecord& Historical)
		{
			if (Historical.bCurrentObservedLocation
				|| !Historical.SpatialMemory.GetBounds().IsInside(Point))
			{
				return;
			}
			const FRecordVisual* Visual = Prop.Visuals.Find(Historical.Epoch);
			const FIntPoint Coarse = Historical.SpatialMemory.GetSize();
			const FIntPoint Fine = Coarse * Darkwell::MovingPropLab::PresentationSamples;
			if (!Visual || Visual->bPresentationRetired || Fine.X <= 0 || Fine.Y <= 0)
			{
				return;
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
				&& Darkwell::MovingPropLab::HistoricalOpacity(Historical, FineIndex, CellY * Coarse.X + CellX) > 0.0f)
			{
				++Contributors;
			}
		};
  if(bUseNewerCandidates && NewerCandidateId==Prop.StableId)
   { for(const auto* C:FrameNewerCandidates) CountHistorical(*C); }
  else { for(const auto& C:Prop.History.GetRecords()) CountHistorical(C); }
		return Contributors;
	};
 // Preserve the original .25 cm Equals predicate. Buckets only narrow its
 // candidates; neighboring buckets retain edge and negative-coordinate cases.
 TMap<FIntPoint,TArray<TPair<uint32,FVector2D>>> CapBuckets;
 auto Bucket=[](FVector2D P){return FIntPoint(FMath::FloorToInt(P.X/.25),FMath::FloorToInt(P.Y/.25));};
 for(const auto& Pair:Prop.Visuals)
  if(Pair.Value.Cap.IsValid() && Pair.Value.Cap->IsVisible() && Pair.Value.CapTriangles>0)
   for(auto P:Pair.Value.CapSamplePoints) CapBuckets.FindOrAdd(Bucket(P)).Emplace(Pair.Key,P);
 auto CapContributorsAt=[&](FVector2D Point)
 {
  TArray<uint32,TInlineAllocator<8>> Epochs; const auto B=Bucket(Point);
  for(int32 Y=-1;Y<=1;++Y) for(int32 X=-1;X<=1;++X)
   if(const auto* Candidates=CapBuckets.Find(B+FIntPoint(X,Y)))
    for(const auto& C:*Candidates) if(C.Value.Equals(Point,.25f)) Epochs.AddUnique(C.Key);
  return Epochs.Num();
 };
 // Repeated epochs at an identical world raster contribute the same diagnostic
 // sample positions. Keep one such grid; all distinct positions and cap points
 // retain the original predicates. This does not alter any stored evidence.
 struct FDiagnosticRaster { FBox2D Bounds; FIntPoint Size; };
 TArray<FDiagnosticRaster,TInlineAllocator<8>> DiagnosticRasters;
 bool ReuseRaster=true;
#if WITH_DEV_AUTOMATION_TESTS
 ReuseRaster=!bForceFullHistoryEvidenceForTesting;
#endif
	for (const FDarkwellSpatialObservationRecord& SampleRecord : Prop.History.GetRecords())
	{
		const FIntPoint SampleSize = SampleRecord.SpatialMemory.GetSize()
			* Darkwell::MovingPropLab::PresentationSamples;
		const FBox2D& SampleBounds = SampleRecord.SpatialMemory.GetBounds();
		if (SampleSize.X <= 0 || SampleSize.Y <= 0 || !SampleBounds.bIsValid)
		{
			continue;
		}
  if(ReuseRaster)
  {
   if(DiagnosticRasters.ContainsByPredicate([&](const FDiagnosticRaster& R) {
    return R.Size==SampleSize && R.Bounds.Min==SampleBounds.Min && R.Bounds.Max==SampleBounds.Max;
   })) continue;
   DiagnosticRasters.Add({SampleBounds,SampleSize});
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
						|| Darkwell::MovingPropLab::HistoricalOpacity(Historical, FineIndex, CellIndex) <= 0.0f)
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
	TRACE_CPUPROFILER_EVENT_SCOPE(Darkwell_GrayHistory_UpdateTracked);
	FScopedHistoryRuntimeTimer Timer(RuntimeFrame.UpdateTrackedUs);
	bool bHistoryChangedThisFrame = false;
	USightWeaveObjectPolicyComponent* ObjectPolicy = Prop.ObjectPolicy.Get();
	check(ObjectPolicy);
	const ESightWeaveHistoryMode HistoryMode = ObjectPolicy->GetResolvedHistoryMode();
	const bool bWhole=ObjectPolicy->GetResolvedRevealMode()==ESightWeaveRevealMode::WholeObjectAfterSpan;
	if (Prop.ProcessedMovingRevision != static_cast<uint64>(ObjectPolicy->GetMovingRevision()))
	{
		Prop.ProcessedMovingRevision = ObjectPolicy->GetMovingRevision();
		Prop.CurrentPresentationActiveSeconds = 0.5f;
		Prop.bDiagnosticsDirty = true;
	}
	ADarkwellPropLabFurniture* Actual = Prop.bExists ? Prop.Actual.Get() : nullptr;
	const uint64 CurrentRevealStartCycles = FPlatformTime::Cycles64();
	if (Actual)
	{
		const FTransform Transform = Actual->GetActorTransform();
  bool bSweptLegalContact=false;
  const bool bPreviousCoverageValid=Prop.bLastCoverageValid;
  const uint64 PreviousCurrentDraw=Prop.CachedCurrentCoverageDrawRevision;
		const FBox2D Bounds = ActualBounds(*Actual);
		const bool bTransformChanged = !Darkwell::MovingPropLab::TransformsMatch(
			Prop.LastPhysicalTransform, Transform);
		const bool bGeometryTransformChanged = !Darkwell::MovingPropLab::TransformsMatch(
			Prop.LastGeometryTransform, Transform);
		if (bGeometryTransformChanged)
		{
			++Prop.TransformRevision;
			++GeometryRevision;
			Prop.bDiagnosticsDirty = true;
			Prop.LastGeometryTransform = Transform;
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
		const UDarkwellFogVisualSubsystem* Fog = GetWorld()
			? GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>() : nullptr;
		const uint64 AuthorityRevision = Fog ? Fog->GetDiagnostics().LastAuthorityRevision : 0;
		const uint64 CoverageDrawRevision = Fog ? Fog->GetDiagnostics().CoverageDrawCount : 0;
  const bool bRasterGeometryDirty=Prop.CachedCurrentTransformRevision!=Prop.TransformRevision || Prop.CachedCurrentGridRevision!=Prop.GridRevision;
		const bool bCoverageDirty = Prop.bInjectInvalidCoverageOnce
			|| Prop.CachedCurrentAuthorityRevision != AuthorityRevision
			|| Prop.CachedCurrentCoverageDrawRevision != CoverageDrawRevision
			|| Prop.CachedCurrentTransformRevision != Prop.TransformRevision
			|| Prop.CachedCurrentGridRevision != Prop.GridRevision;
		FCoverageSnapshot CoverageSnapshot;
		if (bCoverageDirty)
		{
			CoverageSnapshot = SampleConservativeCoverage(
				Bounds, Prop.TransformRevision, Prop.GridRevision);
			if (CoverageSnapshot.bValid)
			{
				Prop.CachedCurrentCoverage = CoverageSnapshot.Values;
				Prop.CachedCurrentAuthorityRevision = CoverageSnapshot.AuthorityRevision;
				Prop.CachedCurrentCoverageDrawRevision = CoverageSnapshot.CoverageRevision;
				Prop.CachedCurrentTransformRevision = Prop.TransformRevision;
				Prop.CachedCurrentGridRevision = Prop.GridRevision;
			}
			Prop.CurrentPresentationActiveSeconds = 0.5f;
			Prop.bDiagnosticsDirty = true;
		}
		else
		{
			CoverageSnapshot.Values = Prop.CachedCurrentCoverage;
			CoverageSnapshot.AuthorityRevision = Prop.CachedCurrentAuthorityRevision;
			CoverageSnapshot.CoverageRevision = Prop.CachedCurrentCoverageDrawRevision;
			CoverageSnapshot.TransformRevision = Prop.CachedCurrentTransformRevision;
			CoverageSnapshot.GridRevision = Prop.CachedCurrentGridRevision;
			CoverageSnapshot.ZeroReason = Prop.LastCoverageZeroReason;
			CoverageSnapshot.bValid = Prop.bLastCoverageValid;
		}
		if (Prop.bInjectInvalidCoverageOnce)
		{
			Prop.bInjectInvalidCoverageOnce = false;
			CoverageSnapshot.bValid = false;
			CoverageSnapshot.ZeroReason = TEXT("TEST_INJECTED_INVALID");
			Prop.CachedCurrentAuthorityRevision = MAX_uint64;
		}
		TArray<float>& Coverage = CoverageSnapshot.Values;
		Prop.bLastCoverageValid = CoverageSnapshot.bValid;
		Prop.CoverageAuthorityRevision = CoverageSnapshot.AuthorityRevision;
		Prop.CoverageRevision = CoverageSnapshot.CoverageRevision;
		Prop.CoverageTransformRevision = CoverageSnapshot.TransformRevision;
		Prop.CoverageGridRevision = CoverageSnapshot.GridRevision;
		Prop.LastCoverageZeroReason = CoverageSnapshot.ZeroReason;
		bool bAnyLegal = CoverageSnapshot.bValid
			&& Coverage.ContainsByPredicate([](const float Value)
			{
				return Value >= FDarkwellSpatialPropMemory::LegalCoverage;
			});
		if(bWhole && CoverageSnapshot.bValid)
		{
			TArray<FDarkwellCurrentLiveGrid::FDescriptor> Descriptors;
			for(const UStaticMeshComponent* Part:Actual->Memory->GetMemoryPrimitives())
				if(Part && Part->GetStaticMesh()) Descriptors.Add({Part->GetUniqueID(),Part->GetStaticMesh()->GetUniqueID(),Part->GetStaticMesh()->GetBoundingBox(),UDarkwellRememberablePropComponent::GetPrimitiveTransform(*Part)});
			if(!Prop.CurrentLive.MatchesGeometry(Descriptors,Transform))
			{
				if(!Prop.CurrentLive.Parts.IsEmpty())
				{
					if (IsCaptureEligible(Prop)) FreezeCurrentForHiddenMotion(Prop,TEXT("GEOMETRY_VERSION_CHANGED"));
					else AbandonCurrentObservationWithoutHistory(Prop);
				}
				Prop.CurrentLive.ResetGeometry(Prop.StableId,Descriptors,Transform);
				Prop.RevealObservation.Initialize(ObjectPolicy->GetResolvedPolicy(),Prop.CurrentLive.ObservationSize,Prop.CurrentLive.ObservationStepCm,Prop.CurrentLive.ObservationFootprint);
			}
   auto Query=[&](FVector2D Point) { const auto Q=Fog->QueryLiveCoverageAtWorldPoint(Point); return Q.bValid && Q.AuthorityRevision==CoverageSnapshot.AuthorityRevision && Q.CoverageDrawRevision==CoverageSnapshot.CoverageRevision?Q.Coverage:0.f; };
   auto Uniform=[&](const FBox2D& B,float& V) { return Fog->TryUniformCoverage(B,V); };
   if(Prop.RevealObservation.IsConfirmed())
   {
    Prop.CurrentLegalObservationMask.Empty();
    if(bCoverageDirty || bTransformChanged)
    {
     Prop.bCachedWholeLegalContact=Prop.CurrentLive.HasAnyLegalObservation(Transform,Query,Uniform);
     RuntimeFrame.CoverageQueries+=Prop.CurrentLive.Queries;
     RuntimeFrame.CurrentSamplesTouched+=Prop.CurrentLive.SamplesTouched;
    }
    bAnyLegal=Prop.bCachedWholeLegalContact;
   }
   else
   {
    if(bCoverageDirty || bTransformChanged || Prop.CurrentPresentationActiveSeconds>0)
    {
     float Value;
     if(Uniform(Bounds,Value) && Value==0) Prop.CurrentLegalObservationMask.Init(false,Prop.CurrentLive.ObservationFootprint.Num());
     else
     {
      Prop.CurrentLive.Advance(DeltaSeconds,Transform,Query,Uniform);
      RuntimeFrame.CoverageQueries+=Prop.CurrentLive.Queries;
      RuntimeFrame.CurrentSamplesTouched+=Prop.CurrentLive.SamplesTouched;
      Prop.CurrentLive.BuildCurrentLegalObservationMask(Prop.CurrentLegalObservationMask);
     }
    }
    bAnyLegal=Prop.CurrentLegalObservationMask.CountSetBits()>0;
   }
   TBitArray<> SweptMask;
   if((!Prop.RevealObservation.IsConfirmed() || !bAnyLegal) && bCoverageDirty && bPreviousCoverageValid
    && !ObjectPolicy->IsSightWeaveMoving() && Transform.Equals(Prop.LastPhysicalTransform,1.e-6))
   {
    FDarkwellFogVisualSourceSnapshot Previous,Current; TConstArrayView<FDarkwellFogVisualSegment> Occluders;
    if(Fog->GetHistoricalRotationSweep(PreviousCurrentDraw,Previous,Current,Occluders)
     && FDarkwellHistoricalVisibilitySweep::MayAddIntermediateSamples(Previous,Current,Bounds))
    {
     bSweptLegalContact=Prop.CurrentLive.BuildSweptObservationMask(Transform,Previous,Current,Occluders,SweptMask,Prop.RevealObservation.IsConfirmed());
     RuntimeFrame.CoverageQueries+=Prop.CurrentLive.Queries;
     RuntimeFrame.SweepCoverageQueries+=Prop.CurrentLive.Queries;
     RuntimeFrame.CurrentSamplesTouched+=Prop.CurrentLive.SamplesTouched;
    }
   }
   if(!Prop.RevealObservation.IsConfirmed())
   {
    if(bSweptLegalContact)
    {
     TBitArray<> SessionMask=Prop.CurrentLegalObservationMask;
     for(TConstSetBitIterator<> It(SweptMask);It;++It) SessionMask[It.GetIndex()]=true;
     Prop.RevealObservation.Observe(true,SessionMask);
     if(!bAnyLegal && !Prop.RevealObservation.IsConfirmed()) Prop.RevealObservation.Observe(true,Prop.CurrentLegalObservationMask);
    }
    else Prop.RevealObservation.Observe(true,Prop.CurrentLegalObservationMask);
   }
   Prop.bCachedWholeLegalContact=bAnyLegal;
		}
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
		if (bAnyLegal || bSweptLegalContact) ObjectPolicy->NotifyLegalObservation();
        if(bWhole && bSweptLegalContact && !bAnyLegal && Prop.RevealObservation.IsConfirmed() && IsCaptureEligible(Prop))
        {
         // A stationary object was legally seen inside this supported interval.
         // Seal that pose directly; never show an out-of-view current source.
         int32 SweptIndex=Prop.History.GetCurrentIndex();
         if(SweptIndex==INDEX_NONE)
         {
          SweptIndex=Prop.History.BeginCurrentObservation(Transform,Bounds,Darkwell::MovingPropLab::CellSize);
          if(SweptIndex!=INDEX_NONE) { ++Prop.ObservationEpisode; ++GeometryRevision; ++Prop.ObservationOwnershipRevision; }
         }
         if(SweptIndex!=INDEX_NONE)
         {
          auto& Current=Prop.History.GetMutableRecords()[SweptIndex]; Prop.LocalEpoch=Current.Epoch;
          if(Fog->IsObjectOcclusionFree(Bounds)) Prop.CurrentLive.AdvanceWholeUnoccluded(DeltaSeconds,Transform,Current.SpatialMemory,Bounds,Coverage);
          else
          {
           auto Occlusion=[&](FVector2D P)
           {
            ++RuntimeFrame.CoverageQueries;
            ++RuntimeFrame.OcclusionOnlyQueries;
            const auto Q=Fog->QueryObjectOcclusionAtWorldPoint(P);
            return Q.bValid && Q.AuthorityRevision==CoverageSnapshot.AuthorityRevision
             && Q.CoverageDrawRevision==CoverageSnapshot.CoverageRevision?Q.Coverage:0.f;
           };
           Prop.CurrentLive.AdvanceWholeWithOcclusion(DeltaSeconds,Transform,Current.SpatialMemory,Bounds,Coverage,Occlusion);
          }
          StampConfirmedWholeCapture(Prop,Current,CoverageSnapshot);
          Prop.ObservationState=EObservationState::ObservedArmed;
          FreezeCurrentForHiddenMotion(Prop,TEXT("SWEPT_LEGAL_OBSERVATION"));
          Prop.bDiagnosticsDirty=true; Prop.CurrentPresentationActiveSeconds=0;
         }
        }
		// Never and an unqualified moving observation have no gray fallback. Invalid
		// authority cannot arm capture; dropping a transient live record writes no V.
		if (!bAnyLegal && !IsCaptureEligible(Prop) && (!bWhole || CoverageSnapshot.bValid))
			AbandonCurrentObservationWithoutHistory(Prop);
		int32 CurrentIndex = Prop.History.GetCurrentIndex();
		if (CoverageSnapshot.bValid && bAnyLegal && CurrentIndex != INDEX_NONE)
		{
			const auto& Observed = Prop.History.GetRecords()[CurrentIndex];
			if (!Observed.Primitives.IsEmpty() && Observed.ContentRevision != Actual->Memory->ComputeMemoryContentRevision())
			{
				if (IsCaptureEligible(Prop)) FreezeCurrentForHiddenMotion(Prop,TEXT("OBSERVED_CONTENT_VERSION_CHANGED"));
				else AbandonCurrentObservationWithoutHistory(Prop);
				CurrentIndex = Prop.History.GetCurrentIndex();
			}
		}
		if (CoverageSnapshot.bValid && CurrentIndex != INDEX_NONE)
		{
			if (bAnyLegal)
			{
				if (bTransformChanged)
				{
					// Pose/evidence are committed together after descriptor validation below.
					FDarkwellSpatialObservationRecord& Current =
						Prop.History.GetMutableRecords()[Prop.History.GetCurrentIndex()];
					FRecordVisual& Visual = Prop.Visuals.FindOrAdd(Current.Epoch);
					Visual.PartBounds = ActualPartBounds(*Actual);
					Visual.PartGeometry = ActualPartGeometry(*Actual);
				}
				Prop.ObservationState = EObservationState::ObservedArmed;
			}
			else if ((bWhole || bTransformChanged || HistoryMode == ESightWeaveHistoryMode::StationaryOnly)
				&& Prop.ObservationState == EObservationState::ObservedArmed)
			{
				FreezeCurrentForHiddenMotion(Prop, TEXT("VALID_OBSERVED_TO_UNOBSERVED"));
			}
		}
		CurrentIndex = Prop.History.GetCurrentIndex();
		if (CoverageSnapshot.bValid && CurrentIndex == INDEX_NONE && bAnyLegal)
		{
			// Repeated sight contact is a new session, not a new object state.
			// Reuse only the still-resident local observation with identical content
			// and pose, and no intervening empty/replacement evidence.
			TArray<FDarkwellCurrentLiveGrid::FDescriptor> ResumeDescriptors;
			for (const UStaticMeshComponent* Part : Actual->Memory->GetMemoryPrimitives())
				if (Part && Part->GetStaticMesh()) ResumeDescriptors.Add({Part->GetUniqueID(),Part->GetStaticMesh()->GetUniqueID(),Part->GetStaticMesh()->GetBoundingBox(),UDarkwellRememberablePropComponent::GetPrimitiveTransform(*Part)});
			if (IsCaptureEligible(Prop) && !ObjectPolicy->IsSightWeaveMoving()
				&& Prop.LastCaptureAppearanceRevision == Actual->Memory->ComputeMemoryContentRevision()
				&& Prop.CurrentLive.LastLegalPose.Equals(Transform, 1.e-6)
				&& Prop.CurrentLive.MatchesGeometry(ResumeDescriptors, Transform)
				&& Prop.History.ResumeUncontradictedObservation(Prop.LocalEpoch))
			{
				Prop.CurrentLive.ResumeStationaryKnowledge();
				if (auto* V = Prop.Visuals.Find(Prop.LocalEpoch)) DestroyVisual(*V);
				Prop.Visuals.Remove(Prop.LocalEpoch);
				bHistoricalSpatialIndexDirty = true;
			}
			const int32 NewIndex = Prop.History.BeginCurrentObservation(
				Transform, Bounds, Darkwell::MovingPropLab::CellSize);
			if (NewIndex != INDEX_NONE)
			{
				++Prop.ObservationEpisode;
				Prop.ObservationState = EObservationState::ObservedArmed;
				++GeometryRevision;
				++Prop.ObservationOwnershipRevision;
				Prop.CurrentPresentationActiveSeconds = 0.5f;
				Prop.bDiagnosticsDirty = true;
			}
		}
		CurrentIndex = Prop.History.GetCurrentIndex();
		if (CurrentIndex != INDEX_NONE)
		{
			FDarkwellSpatialObservationRecord& Current =
				Prop.History.GetMutableRecords()[CurrentIndex];
			if (CoverageSnapshot.bValid && bAnyLegal && Current.Primitives.IsEmpty())
				CaptureObservedContent(Prop, Current);
			if (CoverageSnapshot.bValid
				&& CoverageSnapshot.TransformRevision == Prop.TransformRevision
				&& CoverageSnapshot.GridRevision == Prop.GridRevision
				&& Coverage.Num() == CoverageSize.X*CoverageSize.Y)
			{
				TArray<FDarkwellCurrentLiveGrid::FDescriptor> Descriptors;
                for(const UStaticMeshComponent* Part:Actual->Memory->GetMemoryPrimitives())
                    if(Part && Part->GetStaticMesh()) Descriptors.Add({Part->GetUniqueID(),Part->GetStaticMesh()->GetUniqueID(),Part->GetStaticMesh()->GetBoundingBox(),UDarkwellRememberablePropComponent::GetPrimitiveTransform(*Part)});
                if(Prop.LocalEpoch!=Current.Epoch)
                {
                    if(!bWhole) Prop.CurrentLive.ResetGeometry(Prop.StableId,Descriptors,Transform);
                    Prop.LocalEpoch=Current.Epoch;
                }
                if(!Prop.CurrentLive.MatchesGeometry(Descriptors,Transform))
                {
                    Actual->Memory->ApplySourceGeometryVisibility(false);
                    Prop.LastCoverageZeroReason=TEXT("GEOMETRY_RESET_REQUIRED");
                    return;
                }
                auto Query=[&](FVector2D Point) {
                    const auto Q=Fog->QueryLiveCoverageAtWorldPoint(Point);
                    return Q.bValid && Q.AuthorityRevision==CoverageSnapshot.AuthorityRevision
                        && Q.CoverageDrawRevision==CoverageSnapshot.CoverageRevision ? Q.Coverage : 0.f;
                };
                if(bTransformChanged) Prop.History.UpdateCurrentObservedPosePreservingEvidence(Transform);
                if(bCoverageDirty || bTransformChanged || Prop.CurrentPresentationActiveSeconds>0)
                {
                    auto Uniform=[&](const FBox2D& B,float& V) { return Fog->TryUniformCoverage(B,V); };
                    if(bWhole && Prop.RevealObservation.IsConfirmed() && bAnyLegal)
                    {
                     if(Fog->IsObjectOcclusionFree(Bounds))
                      Prop.CurrentLive.AdvanceWholeUnoccluded(DeltaSeconds,Transform,Current.SpatialMemory,Bounds,Coverage);
                     else
                     {
                      auto Occlusion=[&](FVector2D Point)
                      {
                       ++RuntimeFrame.CoverageQueries;
                       ++RuntimeFrame.OcclusionOnlyQueries;
                       const auto Q=Fog->QueryObjectOcclusionAtWorldPoint(Point);
                       return Q.bValid && Q.AuthorityRevision==CoverageSnapshot.AuthorityRevision
                        && Q.CoverageDrawRevision==CoverageSnapshot.CoverageRevision?Q.Coverage:0.f;
                      };
                      Prop.CurrentLive.AdvanceWholeWithOcclusion(DeltaSeconds,Transform,Current.SpatialMemory,Bounds,Coverage,Occlusion);
                     }
                     Prop.CurrentLegalObservationMask.Empty();
                     StampConfirmedWholeCapture(Prop,Current,CoverageSnapshot);
                    }
                    else
                    {
                     if(!bWhole) Prop.CurrentLive.Advance(DeltaSeconds,Transform,Query,Uniform);
                     else Prop.CurrentLive.Queries=0;
                     Prop.CurrentLive.WriteWorldSnapshot(Current.SpatialMemory,Bounds);
                     Prop.CurrentLive.WritePartRasters(Query,!IsCaptureEligible(Prop) || ObjectPolicy->IsSightWeaveMoving(),Uniform);
                     RuntimeFrame.CoverageQueries += Prop.CurrentLive.Queries;
                     if(!bWhole) RuntimeFrame.CurrentSamplesTouched += Prop.CurrentLive.SamplesTouched;
                    }
                }
				if (bCoverageDirty || Prop.CurrentPresentationActiveSeconds > 0.0f)
				{
					if(!Prop.CurrentLive.OwnershipDirtyRegions.IsEmpty()
						|| bTransformChanged || bRasterGeometryDirty) ++Prop.ObservationOwnershipRevision;
					Prop.bDiagnosticsDirty = true;
				}
			}
			EnsureRecordVisual(Prop, Current);
			if (bCoverageDirty || bTransformChanged
				|| Prop.CurrentPresentationActiveSeconds > 0.0f)
			{
                if(!Prop.CurrentLive.IsUniformWholePresentation()) UpdateRecordTexture(Prop, Current);
                UpdateCurrentPartTextures(Prop, Prop.Visuals.FindChecked(Current.Epoch));
				UpdateRecordCap(Prop, Current);
				Prop.CurrentPresentationActiveSeconds = FMath::Max(
					0.0f, Prop.CurrentPresentationActiveSeconds - DeltaSeconds);
			}
			if (FRecordVisual* CurrentVisual = Prop.Visuals.Find(Current.Epoch);
				CurrentVisual && CurrentVisual->Texture.IsValid())
			{
				// Original primitives use independent current raster bindings.
				Actual->Memory->ApplySourceGeometryVisibility(CoverageSnapshot.bValid);
			}
		}
		else
		{
			Actual->Memory->ApplySourceGeometryVisibility(false);
            Prop.CurrentPresentationActiveSeconds=FMath::Max(0.f,Prop.CurrentPresentationActiveSeconds-DeltaSeconds);
		}
		if (!bTransformChanged || CoverageSnapshot.bValid)
		{
			Prop.LastPhysicalTransform = Transform;
		}
	}
	RuntimeFrame.CurrentRevealUs += FPlatformTime::ToMilliseconds64(
		FPlatformTime::Cycles64() - CurrentRevealStartCycles) * 1000.0;

 // Current creation is complete. Record addresses remain stable through history
 // and diagnostics; erasure happens at the end, after the last candidate query.
	TRACE_CPUPROFILER_EVENT_SCOPE(Darkwell_GrayHistory_HistoricalPath);
	const uint64 CandidateStartCycles = FPlatformTime::Cycles64();
 TArray<const FDarkwellSpatialObservationRecord*,TInlineAllocator<8>> NewerCandidates;
 uint32 MaximumCandidateEpoch=0;
 for(const auto& C:Prop.History.GetRecords())
  if(C.bCurrentObservedLocation || (Prop.Visuals.Find(C.Epoch) && !Prop.Visuals.FindChecked(C.Epoch).bPresentationRetired))
  { NewerCandidates.Add(&C); MaximumCandidateEpoch=FMath::Max(MaximumCandidateEpoch,C.Epoch); }
 TGuardValue<bool> NewerScope(bUseNewerCandidates,true);
 TGuardValue<FName> IdScope(NewerCandidateId,Prop.StableId);
 TGuardValue<uint32> EpochScope(NewerCandidateMaximumEpoch,MaximumCandidateEpoch);
 TGuardValue<TConstArrayView<const FDarkwellSpatialObservationRecord*>> NewerViewScope(FrameNewerCandidates,MakeArrayView(NewerCandidates));
#if WITH_DEV_AUTOMATION_TESTS
 if(bForceFullHistoryEvidenceForTesting) bUseNewerCandidates=false;
#endif
	TArray<uint32> HistoricalEpochs;
	for (FDarkwellSpatialObservationRecord& Record : Prop.History.GetMutableRecords())
	{
		if (!Record.bCurrentObservedLocation)
		{
			const FRecordVisual* Visual = Prop.Visuals.Find(Record.Epoch);
			bool bCandidate = IsHistoricalCandidate(Prop, Record, Visual);
#if WITH_DEV_AUTOMATION_TESTS
			bCandidate |= bForceFullHistoryEvidenceForTesting;
#endif
			if (bCandidate) HistoricalEpochs.Add(Record.Epoch);
		}
	}
	RuntimeFrame.HistoricalCandidateUs += FPlatformTime::ToMilliseconds64(
		FPlatformTime::Cycles64() - CandidateStartCycles) * 1000.0;
	const uint64 HistoricalEvidenceStartCycles = FPlatformTime::Cycles64();
	for (const uint32 Epoch : HistoricalEpochs)
	{
		FDarkwellSpatialObservationRecord* Record = Prop.History.FindRecord(Epoch);
		if (!Record)
		{
			continue;
		}
        TArray<const FActualOccupancySnapshot*,TInlineAllocator<8>> OccupancyCandidates;
        if(bUseFrameOccupancy) for(const auto& S:FrameOccupancy)
         if(S.Bounds.Intersect(Record->SpatialMemory.GetBounds())) OccupancyCandidates.Add(&S);
        TGuardValue<bool> FilterScope(bFilterFrameOccupancy,bUseFrameOccupancy);
        TGuardValue<TConstArrayView<const FActualOccupancySnapshot*>> CandidateScope(FrameOccupancyCandidates,MakeArrayView(OccupancyCandidates));
		EnsureRecordVisual(Prop, *Record);
		FRecordVisual* Visual = Prop.Visuals.Find(Epoch);
		if (!Visual) continue;
		Visual->LastCandidateFrame = RuntimeFrameSequence;
		const bool bPresentationRetired = Visual->bPresentationRetired;
		const UDarkwellFogVisualSubsystem* Fog = GetWorld()
			? GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>() : nullptr;
		const uint64 AuthorityRevision = Fog ? Fog->GetDiagnostics().LastAuthorityRevision : 0;
		const uint64 CoverageDrawRevision = Fog ? Fog->GetDiagnostics().CoverageDrawCount : 0;
		const bool bCoverageDirty = Visual->CachedCoverageAuthorityRevision != AuthorityRevision
			|| Visual->CachedCoverageDrawRevision != CoverageDrawRevision;
		const uint64 SweepPreviousDraw = ActiveMotions.IsEmpty()
			&& Visual->ProcessedGeometryRevision == GeometryRevision
			? Visual->CachedCoverageDrawRevision : MAX_uint64;
		if (bCoverageDirty)
		{
			const FCoverageSnapshot HistoricalCoverage = SampleConservativeCoverage(
				Record->SpatialMemory.GetBounds(), Record->Epoch,
				Record->SpatialMemory.GetGeneration());
			if (HistoricalCoverage.bValid)
			{
				Visual->CachedCoverageAuthorityRevision = HistoricalCoverage.AuthorityRevision;
				Visual->CachedCoverageDrawRevision = HistoricalCoverage.CoverageRevision;
				Visual->CachedCoarseCoverage = HistoricalCoverage.Values;
				Visual->CoarseEvidenceActiveSeconds = 0.5f;
			}
		}
		TArray<int32> GeometryDirtyIndices, PhysicalDirtyIndices;
		const uint64 OccupancyStartCycles = FPlatformTime::Cycles64();
		BuildGeometryDirtyIndices(Prop, *Record, *Visual, GeometryDirtyIndices, PhysicalDirtyIndices);
		const FIntPoint CoarseSize=Record->SpatialMemory.GetSize();
  const FBox2D& CoarseBounds=Record->SpatialMemory.GetBounds();
  const FVector2D CoarseStep=CoarseBounds.GetSize()/FVector2D(CoarseSize.X,CoarseSize.Y);
  const int32 CoarseCount=CoarseSize.X*CoarseSize.Y;
  TBitArray<> CoarseDirty(false,CoarseCount);
  if(Visual->CachedCoarseOccupied.Num()!=CoarseCount)
  { Visual->CachedCoarseOccupied.Init(false,CoarseCount); CoarseDirty.Init(true,CoarseCount); }
  else if(!PhysicalDirtyIndices.IsEmpty() && PhysicalDirtyIndices.Num()==Record->FineHistory.GetSamples().Num()) CoarseDirty.Init(true,CoarseCount);
  else
  {
   const int32 FineX=Record->FineHistory.GetSize().X;
   for(const int32 I:PhysicalDirtyIndices)
    CoarseDirty[(I/FineX/FDarkwellHistoryGridV2::SamplesPerCell)*CoarseSize.X+(I%FineX/FDarkwellHistoryGridV2::SamplesPerCell)]=true;
  }
#if WITH_DEV_AUTOMATION_TESTS
  if(bForceFullHistoryEvidenceForTesting && bCoverageDirty) CoarseDirty.Init(true,CoarseCount);
#endif
  for(TConstSetBitIterator<> It(CoarseDirty);It;++It)
  { const int32 I=It.GetIndex(); Visual->CachedCoarseOccupied[I]=IsOccupiedByActual(CoarseBounds.Min+CoarseStep*FVector2D(I%CoarseSize.X+.5,I/CoarseSize.X+.5),NAME_None); }
		RuntimeFrame.OccupancyUs += FPlatformTime::ToMilliseconds64(
			FPlatformTime::Cycles64() - OccupancyStartCycles) * 1000.0;
  if(bCoverageDirty || !GeometryDirtyIndices.IsEmpty())
  {
   Visual->CachedCoarseEvidence=Visual->CachedCoarseCoverage;
   for(TConstSetBitIterator<> It(Visual->CachedCoarseOccupied);It;++It)
    if(Visual->CachedCoarseEvidence.IsValidIndex(It.GetIndex())) Visual->CachedCoarseEvidence[It.GetIndex()]=0;
   Visual->CoarseEvidenceActiveSeconds=.5f;
  }
		if (Visual->CoarseEvidenceActiveSeconds > 0.0f
			&& Visual->CachedCoarseEvidence.Num() == Record->SpatialMemory.GetCells().Num())
		{
			Prop.History.AdvanceHistorical(Epoch, DeltaSeconds, Visual->CachedCoarseEvidence);
			Visual->CoarseEvidenceActiveSeconds = FMath::Max(
				0.0f, Visual->CoarseEvidenceActiveSeconds - DeltaSeconds);
		}
		const uint64 OwnershipStartCycles = FPlatformTime::Cycles64();
		// Suppression is monotonic. Once this visual has processed a newer sealed
		// epoch, unchanged older contributors can never add another ownership bit.
		// The current epoch remains eligible because its legally observed mask can
		// grow without changing epoch while the player turns.
		TArray<const FDarkwellSpatialObservationRecord*, TInlineAllocator<8>>
			IncrementalOwnershipCandidates;
		bool bUseIncrementalOwnershipCandidates = true;
#if WITH_DEV_AUTOMATION_TESTS
		bUseIncrementalOwnershipCandidates = !bForceFullHistoryEvidenceForTesting;
#endif
		if (bUseIncrementalOwnershipCandidates)
		{
			for (const FDarkwellSpatialObservationRecord* Candidate : FrameNewerCandidates)
			{
				if (Candidate && (Candidate->bCurrentObservedLocation
					|| Candidate->Epoch > Visual->ProcessedOwnershipMaximumEpoch))
				{
					IncrementalOwnershipCandidates.Add(Candidate);
				}
			}
		}
		const TConstArrayView<const FDarkwellSpatialObservationRecord*> OwnershipView =
			bUseIncrementalOwnershipCandidates
			? MakeArrayView(IncrementalOwnershipCandidates)
			: FrameNewerCandidates;
		TGuardValue<TConstArrayView<const FDarkwellSpatialObservationRecord*>>
			OwnershipScope(FrameNewerCandidates, OwnershipView);
		const bool bOwnershipChanged = UpdateHistoricalContributionExclusion(
			Prop, *Record, GeometryDirtyIndices);
		Visual->ProcessedOwnershipMaximumEpoch = MaximumCandidateEpoch;
		RuntimeFrame.OwnershipUs += FPlatformTime::ToMilliseconds64(
			FPlatformTime::Cycles64() - OwnershipStartCycles) * 1000.0;
		const bool bFineChanged = AdvanceFineHistory(
			Prop, *Record, DeltaSeconds, bCoverageDirty, GeometryDirtyIndices, SweepPreviousDraw);
		Visual->bPresentationDirty |= bOwnershipChanged;
		Visual->bCapTopologyDirty |= bOwnershipChanged;
		if (!bPresentationRetired && Visual->bPresentationDirty)
		{
			UpdateRecordTexture(Prop, *Record);
			Visual->bPresentationDirty = false;
		}
		if (!bPresentationRetired && Visual->bCapTopologyDirty)
		{
			UpdateRecordCap(Prop, *Record);
			Visual->bCapTopologyDirty = false;
		}
		Prop.bDiagnosticsDirty |= !bPresentationRetired
			&& (bOwnershipChanged || bFineChanged);
		bHistoryChangedThisFrame |= bOwnershipChanged || bFineChanged;
		if (!bPresentationRetired && (bOwnershipChanged || bFineChanged)
			&& IsHistoricalPresentationResolved(*Record, *Visual))
		{
			RetireHistoricalPresentation(Prop, *Visual);
			Prop.bDiagnosticsDirty = true;
		}
	}
	RuntimeFrame.HistoricalEvidenceUs += FPlatformTime::ToMilliseconds64(
		FPlatformTime::Cycles64() - HistoricalEvidenceStartCycles) * 1000.0;
	const bool bDiagnosticsChanged = Prop.bDiagnosticsDirty;
	if (bDiagnosticsChanged)
	{
		RefreshContributionDiagnostics(Prop);
		Prop.bDiagnosticsDirty = false;
	}
	if (bDiagnosticsChanged && RuntimeFrameSequence % 6 == 0) LogRotationFrame(Prop);

	if (!bHistoryChangedThisFrame) return;
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
		if (Record.FineHistory.IsInitialized() ? Record.FineHistory.IsFullyVerifiedEmpty() : !bAny)
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
	if (!Erased.IsEmpty()) bHistoricalSpatialIndexDirty = true;
	Prop.History.ReleaseFullyErasedRecords();
}

void ADarkwellMovingPropLabRoom::StampConfirmedWholeCapture(
	FTrackedProp& Prop,
	FDarkwellSpatialObservationRecord& Record,
	const FCoverageSnapshot& CoverageSnapshot) const
{
	if (Record.Primitives.IsEmpty()) CaptureObservedContent(Prop, Record);
	Record.bConfirmedWholeCapture = true;
	Record.bCaptureRevisionValid = CoverageSnapshot.bValid
		&& CoverageSnapshot.TransformRevision == Prop.TransformRevision
		&& CoverageSnapshot.GridRevision == Prop.GridRevision;
	Record.CaptureAuthorityRevision = CoverageSnapshot.AuthorityRevision;
	Record.CaptureCoverageRevision = CoverageSnapshot.CoverageRevision;
	Record.CapturePoseRevision = Prop.TransformRevision;
	Record.CapturePolicyRevision = Prop.PolicyRevision;
	Record.CaptureGeometryRevision = Prop.CurrentLive.GeometryResets;
}

bool ADarkwellMovingPropLabRoom::FreezeCurrentForHiddenMotion(
	FTrackedProp& Prop,
	const TCHAR* Reason)
{
	if (!IsCaptureEligible(Prop))
	{
		AbandonCurrentObservationWithoutHistory(Prop);
		return false;
	}
	const int32 CurrentIndex = Prop.History.GetCurrentIndex();
	if (CurrentIndex == INDEX_NONE
		|| Prop.ObservationState != EObservationState::ObservedArmed)
	{
		return false;
	}
	if (!Prop.History.CanSealCurrentObservation())
	{
		// Reject only this capture. Existing history is untouched, and the next
		// legal observation can immediately acquire the independent live slot.
		Prop.History.FreezeCurrentForHiddenMovement(); // Records the capacity diagnostic.
		AbandonCurrentObservationWithoutHistory(Prop);
		return false;
	}
	FDarkwellSpatialObservationRecord& Current =
		Prop.History.GetMutableRecords()[CurrentIndex];
	const uint32 Epoch = Current.Epoch;
	Prop.LastCaptureAppearanceRevision = Current.ContentRevision;
	TBitArray<> WholeGeometryMask;
	if (Current.bConfirmedWholeCapture)
	{
		const bool bAtomicCapture = Current.bCaptureRevisionValid
			&& Current.CapturePolicyRevision == Prop.PolicyRevision
			&& Current.CaptureGeometryRevision == Prop.CurrentLive.GeometryResets
			&& Current.SnapshotTransform.Equals(Prop.CurrentLive.LastLegalPose, 1.e-6);
		const FIntPoint FineSize = Current.SpatialMemory.GetSize()
			* FDarkwellHistoryGridV2::SamplesPerCell;
		if (!bAtomicCapture || !Prop.CurrentLive.BuildFullGeometryMask(
			Current.SpatialMemory.GetBounds(), FineSize, WholeGeometryMask))
		{
			UE_LOG(LogDarkwellMovingPropLab, Warning,
				TEXT("WHOLE_FREEZE_REJECTED id=%s epoch=%u atomic=%d authority_rev=%llu coverage_rev=%llu pose_rev=%llu policy_rev=%llu geometry_rev=%llu current_policy_rev=%llu current_geometry_rev=%llu"),
				*Prop.StableId.ToString(), Epoch, bAtomicCapture ? 1 : 0,
				Current.CaptureAuthorityRevision, Current.CaptureCoverageRevision,
				Current.CapturePoseRevision, Current.CapturePolicyRevision,
				Current.CaptureGeometryRevision, Prop.PolicyRevision,
				Prop.CurrentLive.GeometryResets);
			return false;
		}
	}
	EnsureRecordVisual(Prop, Current);
	if (!Current.bConfirmedWholeCapture)
	{
		UpdateRecordTexture(Prop, Current);
		UpdateRecordCap(Prop, Current);
	}

	// Hide the original source before making the stale proxy renderable. Both
	// state changes happen on the game thread before StartMotion advances the
	// actor, so the overlapping start pose can never draw two coplanar layers.
	if (ADarkwellPropLabFurniture* Actual = Prop.Actual.Get())
	{
		Actual->Memory->ApplySourceGeometryVisibility(false);
        for(const UStaticMeshComponent* Part:Actual->Memory->GetMemoryPrimitives())
            if(auto* MID=Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0))) {
                MID->SetTextureParameterValue(TEXT("SpatialStateTexture"),nullptr);
                MID->SetScalarParameterValue(TEXT("SpatialReady"),0);
            }
	}
	if (FRecordVisual* V=Prop.Visuals.Find(Epoch)) {
        for(auto& T:V->LiveTextures) OwnedTextures.Remove(T.Get());
        V->LiveTextures.Reset(); V->LivePixels.Reset(); V->LiveSignatures.Reset();
    }
    const bool bFrozen = Current.bConfirmedWholeCapture
		? Prop.History.FreezeCurrentFromGeometryMask(WholeGeometryMask)
		: Prop.History.FreezeCurrentForHiddenMovement();
    if (!bFrozen)
	{
		return false;
	}
	Prop.ObservationState = EObservationState::UnobservedSealed;
	++Prop.HiddenFreezeCount;
	if (FDarkwellSpatialObservationRecord* Historical = Prop.History.FindRecord(Epoch))
	{
		Historical->FineHistory.Initialize(Historical->SpatialMemory, Historical->LastLegalCaptureMask);
		EnsureRecordVisual(Prop, *Historical);
		if (FRecordVisual* SealedVisual = Prop.Visuals.Find(Epoch))
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
			for(int32 I=0;I<Footprint.Num();++I) if(!Footprint[I]) Historical->LastLegalCaptureMask[I]=false;
		}
		UpdateRecordTexture(Prop, *Historical);
		UpdateRecordCap(Prop, *Historical);
	}
	const FRecordVisual* Visual = Prop.Visuals.Find(Epoch);
	bHistoricalSpatialIndexDirty = true;
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

void ADarkwellMovingPropLabRoom::AbandonCurrentObservationWithoutHistory(FTrackedProp& Prop)
{
	const int32 Index = Prop.History.GetCurrentIndex();
	if (Index == INDEX_NONE) return;
	const uint32 Epoch = Prop.History.GetRecords()[Index].Epoch;
	if (ADarkwellPropLabFurniture* Actual = Prop.Actual.Get())
    {
        Actual->Memory->ApplySourceGeometryVisibility(false);
        for(const UStaticMeshComponent* Part:Actual->Memory->GetMemoryPrimitives())
            if(auto* MID=Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0))) {
                MID->SetTextureParameterValue(TEXT("SpatialStateTexture"),nullptr);
                MID->SetScalarParameterValue(TEXT("SpatialReady"),0);
            }
    }
	if (FRecordVisual* Visual = Prop.Visuals.Find(Epoch)) DestroyVisual(*Visual);
	Prop.Visuals.Remove(Epoch);
	Prop.History.AbandonCurrentObservationWithoutHistory();
	Prop.ObservationState = EObservationState::NeverObserved;
	Prop.bDiagnosticsDirty = true;
	++GeometryRevision;
	++Prop.ObservationOwnershipRevision;
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
	PendingHistoryDirtyRegions.Add(ActualBounds(*Actual));
	++GeometryRevision;
	++Prop->ObservationOwnershipRevision;
	Prop->bDiagnosticsDirty = true;
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
		Prop->LastGeometryTransform = Prop->LastPhysicalTransform;
		PendingHistoryDirtyRegions.Add(ActualBounds(*Actual));
	}
	UE_LOG(LogDarkwellMovingPropLab, Display,
		TEXT("MOVING_RULES_ACTUAL id=%s state=%s records=%d"),
		*StableId.ToString(), bExists ? TEXT("PRESENT") : TEXT("ABSENT"),
		Prop->History.GetRecords().Num());
	return true;
}

void ADarkwellMovingPropLabRoom::CaptureObservedContent(
	const FTrackedProp& Prop, FDarkwellSpatialObservationRecord& Record) const
{
	const auto* Actual = Prop.Actual.Get();
	if (!Actual || !Record.bCurrentObservedLocation) return;
	Record.ContentRevision = Actual->Memory->ComputeMemoryContentRevision();
	Record.Tint = Actual->Memory->GetRememberedTint();
	Record.UVScale = Actual->Memory->GetRememberedUVScale();
	Record.Primitives.Reset();
	for (const UStaticMeshComponent* Part : Actual->Memory->GetMemoryPrimitives())
		if (Part && Part->GetStaticMesh()) Record.Primitives.Add({Part->GetStaticMesh(),
			Part->GetStaticMesh()->GetBoundingBox(), UDarkwellRememberablePropComponent::GetPrimitiveTransform(*Part), Part->GetUniqueID()});
}

void ADarkwellMovingPropLabRoom::EnsureRecordVisual(
	FTrackedProp& Prop,
	FDarkwellSpatialObservationRecord& Record)
{
	FRecordVisual& Visual = Prop.Visuals.FindOrAdd(Record.Epoch);
	Visual.Epoch = Record.Epoch;
	if (Record.bCurrentObservedLocation && Record.Primitives.IsEmpty()) CaptureObservedContent(Prop, Record);
	if (!Record.bCurrentObservedLocation && Visual.bPresentationRetired)
	{
		return;
	}
	const FIntPoint Size = (Record.bCurrentObservedLocation && Prop.LocalEpoch == Record.Epoch
        ? Prop.CurrentLive.AtlasCells : Record.SpatialMemory.GetSize())
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
		if(Visual.Texture.IsValid()) OwnedTextures.Remove(Visual.Texture.Get());
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
		++RuntimeFrame.TextureCreations;
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
		for (const auto& Part : Record.Primitives)
		{
			FPrimitiveGeometrySnapshot Geometry;
			Geometry.LocalBounds = Part.LocalBounds;
			Geometry.WorldTransform = Part.RelativeTransform * Record.SnapshotTransform;
			Geometry.PrimitiveIndex = Visual.PartGeometry.Num();
			Geometry.CachePlanarProjection();
			Visual.PartBounds.Add(Part.LocalBounds.TransformBy(Geometry.WorldTransform));
			Visual.PartGeometry.Add(Geometry);
		}
	}
	const bool bWholeWithoutCap = Record.bConfirmedWholeCapture;
	if ((bWholeWithoutCap || (Record.bCurrentObservedLocation && !IsCaptureEligible(Prop))) && Visual.Cap.IsValid())
	{
		OwnedCaps.Remove(Visual.Cap.Get());
		Visual.Cap->DestroyComponent();
		Visual.Cap.Reset();
		Visual.CapQuads.Reset();
		Visual.CapTriangles = 0;
		Visual.CapSignature = 0;
	}
	if (!bWholeWithoutCap && !Visual.Cap.IsValid() && (!Record.bCurrentObservedLocation || IsCaptureEligible(Prop)))
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

void ADarkwellMovingPropLabRoom::UpdateCurrentPartTextures(FTrackedProp& Prop,FRecordVisual& Visual)
{
 auto* Actual=Prop.Actual.Get(); if(!Actual) return;
 const auto Sources=Actual->Memory->GetMemoryPrimitives();
 const int32 Count=Prop.CurrentLive.Parts.Num();
 Visual.LiveTextures.SetNum(Count); Visual.LivePixels.SetNum(Count); Visual.LiveSignatures.SetNum(Count);
 for(int32 I=0;I<Count;++I)
 {
  const auto& Part=Prop.CurrentLive.Parts[I]; const auto Atlas=Part.AtlasCells*4;
  UTexture2D* Texture=Visual.LiveTextures[I].Get();
  if(!Texture)
  {
   Texture=UTexture2D::CreateTransient(Atlas.X,Atlas.Y,PF_FloatRGBA);
   Texture->SRGB=false; Texture->Filter=TF_Bilinear; Texture->AddressX=TA_Clamp; Texture->AddressY=TA_Clamp; Texture->NeverStream=true;
   auto& Bulk=Texture->GetPlatformData()->Mips[0].BulkData;
   FMemory::Memzero(Bulk.Lock(LOCK_READ_WRITE),Bulk.GetBulkDataSize()); Bulk.Unlock(); Texture->UpdateResource();
   Visual.LiveTextures[I]=Texture; OwnedTextures.Add(Texture); ++Visual.LiveTextureCreations;
   ++RuntimeFrame.TextureCreations;
  }
  auto& Pixels=Visual.LivePixels[I];
  FIntPoint Size;
  uint64 Hash=1469598103934665603ull;
  if(Part.bUniformWholePresentation)
  {
   // Same full-size atlas, filled with an exact constant only when RGB changes.
   // Rigid pose changes only update its bounds; they do not rebuild a local grid.
   Size=Atlas;
   for(float V:{Part.WholePixel.R,Part.WholePixel.G,Part.WholePixel.B,Part.WholePixel.A}) { uint32 Bits; FMemory::Memcpy(&Bits,&V,4); Hash=(Hash^Bits)*1099511628211ull; }
   Hash=(Hash^0x57484f4c45ull)*1099511628211ull;
   if(Hash!=Visual.LiveSignatures[I]) Pixels.Init(Part.WholePixel,Atlas.X*Atlas.Y);
  }
  else
  {
   Size=Part.Raster.BuildConservativePresentation(4,Pixels);
   for(const auto& Pixel:Pixels) for(float V : {Pixel.R,Pixel.G,Pixel.B,Pixel.A}) { uint32 Bits; FMemory::Memcpy(&Bits,&V,4); Hash=(Hash^Bits)*1099511628211ull; }
   Hash=(Hash^uint64(Size.X))*1099511628211ull; Hash=(Hash^uint64(Size.Y))*1099511628211ull;
  }
  if(Hash!=Visual.LiveSignatures[I])
  {
   if(Texture->GetResource())
   {
   ++RuntimeFrame.GpuTextureUploads;
   auto* Upload=new FFloat16Color[Atlas.X*Atlas.Y];
   if(Part.bUniformWholePresentation) for(int32 K=0;K<Atlas.X*Atlas.Y;++K) Upload[K]=FFloat16Color(Part.WholePixel);
   else FDarkwellCurrentLiveGrid::CopyAtlasWithClampBorder(Pixels,Size,Atlas,MakeArrayView(Upload,Atlas.X*Atlas.Y));
   auto* Region=new FUpdateTextureRegion2D(0,0,0,0,Atlas.X,Atlas.Y);
   Texture->UpdateTextureRegions(0,1,Region,Atlas.X*sizeof(FFloat16Color),sizeof(FFloat16Color),reinterpret_cast<uint8*>(Upload),
    [](uint8* Data,const FUpdateTextureRegion2D* R){delete[] reinterpret_cast<FFloat16Color*>(Data);delete R;});
   }
   ++Visual.LiveTextureUploads; ++RuntimeFrame.TextureUploads; Visual.LiveSignatures[I]=Hash;
  }
  if(Sources.IsValidIndex(I)) if(auto* Material=Cast<UMaterialInstanceDynamic>(Sources[I]->GetMaterial(0)))
  {
   const auto Bounds=Part.bUniformWholePresentation?Part.WholeBounds:Part.Raster.GetBounds(); const auto Extent=Bounds.GetSize()*FVector2D(Atlas)/FVector2D(Size);
   Material->SetTextureParameterValue(TEXT("SpatialStateTexture"),Texture);
   Material->SetVectorParameterValue(TEXT("SpatialMinInv"),FLinearColor(Bounds.Min.X,Bounds.Min.Y,1/Extent.X,1/Extent.Y));
   Material->SetScalarParameterValue(TEXT("SpatialReady"),1); Material->SetScalarParameterValue(TEXT("FixedRevealEnabled"),1);
  }
 }
}

void ADarkwellMovingPropLabRoom::UpdateRecordTexture(
	FTrackedProp& Prop,
	FDarkwellSpatialObservationRecord& Record)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Darkwell_GrayHistory_TextureSubmission);
	FScopedHistoryRuntimeTimer TextureTimer(RuntimeFrame.TextureSubmissionUs);
	++RuntimeFrame.UpdateRecordTextureCalls;
	FRecordVisual* Visual = Prop.Visuals.Find(Record.Epoch);
	if (!Visual || !Visual->Texture.IsValid())
	{
		return;
	}
	TArray<FLinearColor> Presentation;
	const bool FineHistory = !Record.bCurrentObservedLocation && Record.FineHistory.IsInitialized();
	FIntPoint Size;
	if (FineHistory)
	{
		Record.FineHistory.BuildPresentation(Presentation);
		Size = Record.FineHistory.GetSize();
	}
	else Size = Record.SpatialMemory.BuildConservativePresentation(
		Darkwell::MovingPropLab::PresentationSamples, Presentation,
		Record.bCurrentObservedLocation && !IsCaptureEligible(Prop));
	if (Size.X <= 0 || Presentation.Num() != Size.X * Size.Y)
	{
		return;
	}
	if (!Record.bCurrentObservedLocation && !FineHistory
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
	++RuntimeFrame.TextureUploads;
    // NullRHI has no texture resource; UTexture2D does not call DataCleanupFunc
    // in that case. Keep CPU submission diagnostics without leaking a buffer.
    if(!Visual->Texture->GetResource()) return;
	++RuntimeFrame.GpuTextureUploads;
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
	if (Record.Primitives.IsEmpty())
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
	for (const auto& Source : Record.Primitives)
	{
		UStaticMesh* SourceMesh = Source.Mesh.LoadSynchronous();
		if (!SourceMesh)
		{
			continue;
		}
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(
			Proxy, *FString::Printf(TEXT("SpatialMemoryMesh_%d"), Index++));
		Mesh->SetupAttachment(Root);
		Mesh->SetStaticMesh(SourceMesh);
		Mesh->SetWorldTransform(Source.RelativeTransform * Record.SnapshotTransform);
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
		++RuntimeFrame.MidCreations;
		Material->SetTextureParameterValue(TEXT("SpatialStateTexture"), Visual->Texture.Get());
		Material->SetVectorParameterValue(TEXT("SpatialMinInv"),
			FLinearColor(Bounds.Min.X, Bounds.Min.Y, Inv.X, Inv.Y));
		Material->SetVectorParameterValue(TEXT("OriginalBaseColorTint"), Record.Tint);
		Material->SetScalarParameterValue(TEXT("OriginalUVScale"), Record.UVScale);
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
	TRACE_CPUPROFILER_EVENT_SCOPE(Darkwell_GrayHistory_CapPresentation);
	FScopedHistoryRuntimeTimer CapTimer(RuntimeFrame.CapPresentationUs);
	++RuntimeFrame.UpdateRecordCapCalls;
	using namespace UE::Geometry;
	FRecordVisual* Visual = Prop.Visuals.Find(Record.Epoch);
	if (Record.bConfirmedWholeCapture)
	{
		if (Visual && Visual->Cap.IsValid())
		{
			OwnedCaps.Remove(Visual->Cap.Get());
			Visual->Cap->DestroyComponent();
			Visual->Cap.Reset();
			Visual->CapTriangles = 0;
			Visual->CapSignature = 0;
			Visual->CapQuads.Reset();
			Visual->CapSamplePoints.Reset();
		}
		return;
	}
	if (!Visual || !Visual->Cap.IsValid())
	{
		return;
	}
    // Full per-primitive discovery has no cut boundary. The empty area of the
    // world AABB outside rotated geometry is not an undiscovered cabinet part.
    if(Record.bCurrentObservedLocation && Prop.LocalEpoch==Record.Epoch && Prop.CurrentLive.bFullyObservedAtPose)
    {
        if(Visual->CapTriangles>0) { ++RuntimeFrame.CapMeshRebuilds; Visual->Cap->SetMesh(FDynamicMesh3()); }
        Visual->Cap->SetVisibility(false); Visual->CapTriangles=0; Visual->CapSignature=0;
        Visual->CapQuads.Reset(); Visual->CapSamplePoints.Reset();
        return;
    }
	const bool bPresent = Record.SpatialMemory.IsPresent();
	const bool bAbsent = Record.SpatialMemory.IsAbsent();
	const bool bFineHistory = !Record.bCurrentObservedLocation && Record.FineHistory.IsInitialized();
	TArray<FDarkwellSpatialPropMemory::FCell> FineCells;
	if (bFineHistory)
	{
		FineCells.SetNum(Record.FineHistory.GetSamples().Num());
		for (int32 I = 0; I < FineCells.Num(); ++I)
		{
			const auto& S = Record.FineHistory.GetSamples()[I];
			FineCells[I].InitialRemembered = S.State == FDarkwellHistoryGridV2::Unresolved() ? S.InitialRemembered : 0;
			FineCells[I].VerifiedEmpty = S.bVerifiedEmpty ? 1.f : 0.f;
		}
	}
	const TConstArrayView<FDarkwellSpatialPropMemory::FCell> Cells = bFineHistory
		? TConstArrayView<FDarkwellSpatialPropMemory::FCell>(FineCells) : Record.SpatialMemory.GetCells();
	const FIntPoint Size = bFineHistory ? Record.FineHistory.GetSize() : Record.SpatialMemory.GetSize();
	if ((!bPresent && !bAbsent) || Cells.IsEmpty() || Visual->PartBounds.IsEmpty())
	{
		++RuntimeFrame.CapMeshRebuilds;
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
		const int32 Samples = bFineHistory ? 1 : Darkwell::MovingPropLab::PresentationSamples;
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
	for (const auto& S : Record.FineHistory.GetSamples())
		Signature = (Signature ^ GetTypeHash(S.State)) * 1099511628211ull;
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
		for (const auto& S : Candidate.FineHistory.GetSamples())
			Signature = (Signature ^ GetTypeHash(S.State) ^ (S.Opacity > 0 ? 1ull : 0ull)) * 1099511628211ull;
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
	++RuntimeFrame.CapMeshRebuilds;
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
		if (bFineHistory)
		{
			const auto State = Record.FineHistory.GetSamples()[Y * Size.X + X].State;
			return State == FDarkwellHistoryGridV2::NeverObserved() || State == FDarkwellHistoryGridV2::VerifiedEmpty();
		}
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
		const int32 Segments = bFineHistory ? 1 : Darkwell::MovingPropLab::PresentationSamples;
		for (int32 Segment = 0; Segment < Segments;
			++Segment)
		{
			const double Alpha0 = static_cast<double>(Segment)
				/ Segments;
			const double Alpha1 = static_cast<double>(Segment + 1)
				/ Segments;
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
					const FIntPoint Fine = Size * (bFineHistory ? 1 : Darkwell::MovingPropLab::PresentationSamples);
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
					if (bFineHistory ? Record.FineHistory.CanEmitCap(Y * Size.X + X, NY * Size.X + NX)
						: (Neighbor.InitialRemembered == 0 || Neighbor.VerifiedEmpty > 0))
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
		if (ADarkwellPropLabFurniture* Actual = Motion.Prop.Get())
			if (FTrackedProp* Prop = Tracked.Find(Actual->StableId))
				Prop->ObjectPolicy->SetSightWeaveMoving(false);
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
	if (FTrackedProp* TrackedProp = Tracked.Find(Prop->StableId))
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
			if (FTrackedProp* Prop = Tracked.Find(Actual->StableId))
				Prop->ObjectPolicy->SetSightWeaveMoving(false);
			ActiveMotions.RemoveAtSwap(Index);
		}
	}
	bMotionActive = !ActiveMotions.IsEmpty();
}

void ADarkwellMovingPropLabRoom::RebuildHistoricalSpatialIndex()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Darkwell_GrayHistory_SpatialIndexRebuild);
	HistoricalSpatialIndex.Reset();
	for (const TPair<FName, FTrackedProp>& Pair : Tracked)
	{
		for (const FDarkwellSpatialObservationRecord& Record : Pair.Value.History.GetRecords())
		{
			if (Record.bCurrentObservedLocation) continue;
			const FHistorySpatialKey Key{Pair.Key, Record.Epoch};
			TSet<FIntPoint> RecordTiles;
			auto AddBounds = [&RecordTiles](const FBox2D& Bounds)
			{
				if (!Bounds.bIsValid) return;
				const FIntPoint Minimum(
					FMath::FloorToInt(Bounds.Min.X / Darkwell::MovingPropLab::HistorySpatialCellSize),
					FMath::FloorToInt(Bounds.Min.Y / Darkwell::MovingPropLab::HistorySpatialCellSize));
				const FIntPoint Maximum(
					FMath::FloorToInt(Bounds.Max.X / Darkwell::MovingPropLab::HistorySpatialCellSize),
					FMath::FloorToInt(Bounds.Max.Y / Darkwell::MovingPropLab::HistorySpatialCellSize));
				for (int32 Y = Minimum.Y; Y <= Maximum.Y; ++Y)
					for (int32 X = Minimum.X; X <= Maximum.X; ++X)
						RecordTiles.Add(FIntPoint(X, Y));
			};
			if (Record.FineHistory.IsInitialized())
			{
				const FIntPoint Size = Record.FineHistory.GetSize();
				const FBox2D& Bounds = Record.FineHistory.GetBounds();
				const FVector2D Step = Bounds.GetSize() / FVector2D(Size.X, Size.Y);
				const TConstArrayView<FDarkwellHistoryGridV2::FSample> Samples =
					Record.FineHistory.GetSamples();
				const FGameplayTag Superseded = FDarkwellHistoryGridV2::Superseded();
				for (int32 Index = 0; Index < Samples.Num(); ++Index)
				{
					const FDarkwellHistoryGridV2::FSample& Sample = Samples[Index];
					const bool bCanStillChange = Sample.InitialRemembered > 0.0f
						&& Sample.State != Superseded
						&& (!Sample.bVerifiedEmpty || Sample.Opacity > 0.0f);
					if (!bCanStillChange) continue;
					const int32 X = Index % Size.X;
					const int32 Y = Index / Size.X;
					const FVector2D Minimum = Bounds.Min + Step * FVector2D(X, Y);
					AddBounds(FBox2D(Minimum, Minimum + Step));
				}
			}
			else
			{
				const FIntPoint Size = Record.SpatialMemory.GetSize();
				const FBox2D& Bounds = Record.SpatialMemory.GetBounds();
				const FVector2D Step = Bounds.GetSize() / FVector2D(Size.X, Size.Y);
				const TConstArrayView<FDarkwellSpatialPropMemory::FCell> Cells =
					Record.SpatialMemory.GetCells();
				for (int32 Index = 0; Index < Cells.Num(); ++Index)
				{
					const FDarkwellSpatialPropMemory::FCell& Cell = Cells[Index];
					if (Cell.InitialRemembered <= 0.0f
						|| (Cell.VerifiedEmpty > 0.0f && Cell.StaleOpacity <= 0.0f)) continue;
					const int32 X = Index % Size.X;
					const int32 Y = Index / Size.X;
					const FVector2D Minimum = Bounds.Min + Step * FVector2D(X, Y);
					AddBounds(FBox2D(Minimum, Minimum + Step));
				}
			}
			// Terminal/retired records no longer have mutable samples, but keeping
			// their compact record footprint indexed preserves near-observer
			// diagnostic cache parity without making distant records active.
			if (RecordTiles.IsEmpty()) AddBounds(Record.SpatialMemory.GetBounds());
			for (const FIntPoint& Tile : RecordTiles)
				HistoricalSpatialIndex.FindOrAdd(Tile).Add(Key);
		}
	}
	bHistoricalSpatialIndexDirty = false;
}

void ADarkwellMovingPropLabRoom::QueryHistoricalSpatialIndex(
	const FBox2D& Bounds, const bool bDirtyRegion)
{
	if (!Bounds.bIsValid) return;
	const FIntPoint Minimum(
		FMath::FloorToInt(Bounds.Min.X / Darkwell::MovingPropLab::HistorySpatialCellSize),
		FMath::FloorToInt(Bounds.Min.Y / Darkwell::MovingPropLab::HistorySpatialCellSize));
	const FIntPoint Maximum(
		FMath::FloorToInt(Bounds.Max.X / Darkwell::MovingPropLab::HistorySpatialCellSize),
		FMath::FloorToInt(Bounds.Max.Y / Darkwell::MovingPropLab::HistorySpatialCellSize));
	for (int32 Y = Minimum.Y; Y <= Maximum.Y; ++Y)
	{
		for (int32 X = Minimum.X; X <= Maximum.X; ++X)
		{
			const FIntPoint Tile(X, Y);
			const TArray<FHistorySpatialKey>* Keys = HistoricalSpatialIndex.Find(Tile);
			if (!Keys) continue;
			if (bDirtyRegion) FrameHistoryDirtyTiles.Add(Tile);
			for (const FHistorySpatialKey& Key : *Keys) FrameHistoricalCandidates.Add(Key);
		}
	}
}

void ADarkwellMovingPropLabRoom::PrepareHistoricalCandidates(ADarkwellCharacter* Player)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Darkwell_GrayHistory_CandidateIndex);
	if (bHistoricalSpatialIndexDirty) RebuildHistoricalSpatialIndex();
	FrameHistoricalCandidates.Reset();
	FrameHistoryDirtyTiles.Reset();
	const FVector2D Observer(Player->GetActorLocation());
	const FVector2D Radius(Darkwell::MovingPropLab::HistoryMaximumInfluenceRange);
	QueryHistoricalSpatialIndex(FBox2D(Observer - Radius, Observer + Radius), false);
	if (bHasPreviousHistoryObserver && !PreviousHistoryObserverLocation.Equals(Observer, 0.01))
	{
		QueryHistoricalSpatialIndex(FBox2D(
			PreviousHistoryObserverLocation - Radius,
			PreviousHistoryObserverLocation + Radius), false);
	}
	for (const FBox2D& Dirty : PendingHistoryDirtyRegions)
		QueryHistoricalSpatialIndex(Dirty, true);
	PendingHistoryDirtyRegions.Reset();
	PreviousHistoryObserverLocation = Observer;
	bHasPreviousHistoryObserver = true;
	RuntimeFrame.DirtyTileCount = FrameHistoryDirtyTiles.Num();
}

bool ADarkwellMovingPropLabRoom::IsHistoricalCandidate(
	const FTrackedProp& Prop,
	const FDarkwellSpatialObservationRecord& Record,
	const FRecordVisual* Visual) const
{
	if (Record.bCurrentObservedLocation) return false;
	if (!Visual) return true;
	if (Visual->bPresentationDirty
		|| Visual->bCapTopologyDirty
		|| Visual->ProcessedGeometryRevision == 0) return true;
	return FrameHistoricalCandidates.Contains(FHistorySpatialKey{Prop.StableId, Record.Epoch});
}

void ADarkwellMovingPropLabRoom::UpdateRoom(
	const float DeltaSeconds,
	ADarkwellCharacter* Player)
{
	if (!bStarted || !Player || FindActive(GetWorld()) != this)
	{
		return;
	}
	RuntimeFrame = FHistoryRuntimeTelemetry();
	RuntimeFrame.FrameNumber = ++RuntimeFrameSequence;
	TRACE_CPUPROFILER_EVENT_SCOPE(Darkwell_GrayHistory_RoomUpdate);
	const uint64 UpdateRoomStartCycles = FPlatformTime::Cycles64();
 const auto* CoverageFog=GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>();
 const uint64 ComputationsBefore=CoverageFog?CoverageFog->GetCoverageComputationsForTesting():0;
 const uint64 HitsBefore=CoverageFog?CoverageFog->GetCoverageCacheHitsForTesting():0;
	UpdateInWorldAutomation(DeltaSeconds, Player);
	const bool bWasMoving = bMotionActive;
	UpdateDeterministicMotion(DeltaSeconds);
	if (bInWorldControls && bWasMoving && !bMotionActive)
	{
		CompleteInWorldMotionGroup();
	}
 // Motion has advanced every actor. Snapshot physical bounds/primitives once
 // for all history occupancy queries in this update, including other identities.
	const uint64 OccupancySnapshotStartCycles = FPlatformTime::Cycles64();
 FrameOccupancy.Reset(); FrameOccupancyPoints.Reset(); FrameHistoryGeometry.Reset(); FrameHistoryOwnership.Reset(); FrameHistoryCoverage.Reset();
 bCacheFrameOccupancyPoints=false;
 for(const auto& Pair:Tracked)
  bCacheFrameOccupancyPoints |= Pair.Value.History.GetRecords().Num()-(Pair.Value.History.GetCurrentIndex()!=INDEX_NONE?1:0)>1;
 bool PhysicalMotion=false;
 for(const auto& Pair:Tracked)
  if(const auto* A=Pair.Value.bExists?Pair.Value.Actual.Get():nullptr; A && A->GetActorEnableCollision())
	{
		auto& S=FrameOccupancy.AddDefaulted_GetRef();
		S.StableId=Pair.Key; S.Bounds=ActualBounds(*A); S.Geometry=ActualPartGeometry(*A);
		const bool bMoved = !Darkwell::MovingPropLab::TransformsMatch(
			Pair.Value.LastGeometryTransform, A->GetActorTransform());
		if (bMoved)
		{
			PendingHistoryDirtyRegions.Add(Pair.Value.LastCoverageBounds);
			PendingHistoryDirtyRegions.Add(S.Bounds);
		}
		PhysicalMotion |= bMoved;
	}
	RuntimeFrame.OccupancySnapshotUs = FPlatformTime::ToMilliseconds64(
		FPlatformTime::Cycles64() - OccupancySnapshotStartCycles) * 1000.0;
 if(PhysicalMotion) ++GeometryRevision;
	PrepareHistoricalCandidates(Player);
 bUseFrameOccupancy=true;
	for (TPair<FName, FTrackedProp>& Pair : Tracked)
	{
		UpdateTracked(Pair.Value, DeltaSeconds);
	}
 bUseFrameOccupancy=false;
	UpdateInWorldAutomation(0.0f, Player);
	Report();
 RuntimeFrame.CoverageComputations=CoverageFog?CoverageFog->GetCoverageComputationsForTesting()-ComputationsBefore:0;
 RuntimeFrame.CoverageCacheHits=CoverageFog?CoverageFog->GetCoverageCacheHitsForTesting()-HitsBefore:0;
	FinalizeHistoryRuntimeTelemetry(UpdateRoomStartCycles);
}

void ADarkwellMovingPropLabRoom::FinalizeHistoryRuntimeTelemetry(
	const uint64 UpdateRoomStartCycles)
{
	RuntimeFrame.MovingPropLabGameThreadUs = FPlatformTime::ToMilliseconds64(
		FPlatformTime::Cycles64() - UpdateRoomStartCycles) * 1000.0;
	for (const TPair<FName, FTrackedProp>& Pair : Tracked)
	{
		RuntimeFrame.SpatialRecordCount += Pair.Value.History.GetRecords().Num();
		if (const ADarkwellPropLabFurniture* Source = Pair.Value.Actual.Get())
			for (const UStaticMeshComponent* Part : Source->Memory->GetMemoryPrimitives())
				RuntimeFrame.SourceMidCount += Part && Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)) ? 1 : 0;
		for (const FDarkwellSpatialObservationRecord& Record : Pair.Value.History.GetRecords())
		{
			if (!Record.bCurrentObservedLocation)
			{
				const FRecordVisual* Visual = Pair.Value.Visuals.Find(Record.Epoch);
				if (Visual && Visual->LastCandidateFrame == RuntimeFrameSequence)
				{
					++RuntimeFrame.ActiveHistoricalEpochs;
					++RuntimeFrame.CandidateHistoricalEpochs;
				}
				else
				{
					++RuntimeFrame.SleepingHistoricalEpochs;
				}
			}
			RuntimeFrame.FineSamplesResident += Record.FineHistory.GetSamples().Num();
		}
	}
	RuntimeFrame.FineHistoryResidentBytes = static_cast<uint64>(
		RuntimeFrame.FineSamplesResident) * sizeof(FDarkwellHistoryGridV2::FSample);
	RuntimeFrame.ProxyCount = GetTotalProxyCount();
	RuntimeFrame.CapComponentCount = OwnedCaps.Num();
	RuntimeFrame.TextureCount = OwnedTextures.Num();
	RuntimeFrame.MidCount = OwnedMaterials.Num();
	RuntimeFrame.ProcessWorkingSetBytes = FPlatformMemory::GetStats().UsedPhysical;
	RuntimeFrame.UObjectCount = GUObjectArray.GetObjectArrayNum();
	RuntimeFrame.LiveUObjectCount = GUObjectArray.GetObjectArrayNumMinusAvailable();

	++RuntimeTotal.FramesAccumulated;
	RuntimeTotal.FrameNumber = RuntimeFrame.FrameNumber;
	RuntimeTotal.ActiveHistoricalEpochs = RuntimeFrame.ActiveHistoricalEpochs;
	RuntimeTotal.CandidateHistoricalEpochs = RuntimeFrame.CandidateHistoricalEpochs;
	RuntimeTotal.SleepingHistoricalEpochs = RuntimeFrame.SleepingHistoricalEpochs;
	RuntimeTotal.DirtyTileCount = RuntimeFrame.DirtyTileCount;
	RuntimeTotal.FineSamplesResident = RuntimeFrame.FineSamplesResident;
	RuntimeTotal.FineSamplesScanned += RuntimeFrame.FineSamplesScanned;
	RuntimeTotal.CoverageFullScans += RuntimeFrame.CoverageFullScans;
	RuntimeTotal.CoverageQueries += RuntimeFrame.CoverageQueries;
	RuntimeTotal.OcclusionOnlyQueries += RuntimeFrame.OcclusionOnlyQueries;
 RuntimeTotal.CoverageComputations+=RuntimeFrame.CoverageComputations; RuntimeTotal.CoverageCacheHits+=RuntimeFrame.CoverageCacheHits;
	RuntimeTotal.CurrentSamplesTouched += RuntimeFrame.CurrentSamplesTouched;
	RuntimeTotal.TextureCreations += RuntimeFrame.TextureCreations;
	RuntimeTotal.MidCreations += RuntimeFrame.MidCreations;
	RuntimeTotal.GpuTextureUploads += RuntimeFrame.GpuTextureUploads;
	RuntimeTotal.OccupancyTests += RuntimeFrame.OccupancyTests;
 RuntimeTotal.OccupancyCacheHits += RuntimeFrame.OccupancyCacheHits;
 RuntimeTotal.HistoryGeometryReuseHits += RuntimeFrame.HistoryGeometryReuseHits;
 RuntimeTotal.HistoryOwnershipReuseHits += RuntimeFrame.HistoryOwnershipReuseHits;
 RuntimeTotal.HistoryCoverageReuseHits += RuntimeFrame.HistoryCoverageReuseHits;
	RuntimeTotal.HistoryOccupancySamplesReused += RuntimeFrame.HistoryOccupancySamplesReused;
	RuntimeTotal.PrimitiveGeometryTests += RuntimeFrame.PrimitiveGeometryTests;
	RuntimeTotal.OwnershipTests += RuntimeFrame.OwnershipTests;
	RuntimeTotal.UpdateRecordTextureCalls += RuntimeFrame.UpdateRecordTextureCalls;
	RuntimeTotal.TextureUploads += RuntimeFrame.TextureUploads;
	RuntimeTotal.UpdateRecordCapCalls += RuntimeFrame.UpdateRecordCapCalls;
	RuntimeTotal.CapMeshRebuilds += RuntimeFrame.CapMeshRebuilds;
	RuntimeTotal.RefreshContributionDiagnosticsUs += RuntimeFrame.RefreshContributionDiagnosticsUs;
	RuntimeTotal.LogRotationFrameUs += RuntimeFrame.LogRotationFrameUs;
	RuntimeTotal.ReportHudUs += RuntimeFrame.ReportHudUs;
	RuntimeTotal.AdvanceFineHistoryUs += RuntimeFrame.AdvanceFineHistoryUs;
	RuntimeTotal.CoverageUs += RuntimeFrame.CoverageUs;
	RuntimeTotal.OccupancyUs += RuntimeFrame.OccupancyUs;
	RuntimeTotal.OwnershipUs += RuntimeFrame.OwnershipUs;
	RuntimeTotal.TextureSubmissionUs += RuntimeFrame.TextureSubmissionUs;
	RuntimeTotal.CapPresentationUs += RuntimeFrame.CapPresentationUs;
	RuntimeTotal.CurrentRevealUs += RuntimeFrame.CurrentRevealUs;
	RuntimeTotal.HistoricalCandidateUs += RuntimeFrame.HistoricalCandidateUs;
	RuntimeTotal.HistoricalEvidenceUs += RuntimeFrame.HistoricalEvidenceUs;
	RuntimeTotal.OccupancySnapshotUs += RuntimeFrame.OccupancySnapshotUs;
	RuntimeTotal.SweepCandidateSamples += RuntimeFrame.SweepCandidateSamples;
	RuntimeTotal.SweepCoverageQueries += RuntimeFrame.SweepCoverageQueries;
	RuntimeTotal.SweepAcceptedSamples += RuntimeFrame.SweepAcceptedSamples;
	RuntimeTotal.SweepBudgetRejects += RuntimeFrame.SweepBudgetRejects;
	RuntimeTotal.SweepUnsupportedEvents += RuntimeFrame.SweepUnsupportedEvents;
	RuntimeTotal.SweepProofUs += RuntimeFrame.SweepProofUs;
	RuntimeTotal.UpdateTrackedUs += RuntimeFrame.UpdateTrackedUs;
	RuntimeTotal.MovingPropLabGameThreadUs += RuntimeFrame.MovingPropLabGameThreadUs;
	RuntimeTotal.ProxyCount = RuntimeFrame.ProxyCount;
	RuntimeTotal.CapComponentCount = RuntimeFrame.CapComponentCount;
	RuntimeTotal.TextureCount = RuntimeFrame.TextureCount;
	RuntimeTotal.MidCount = RuntimeFrame.MidCount;
	RuntimeTotal.SourceMidCount = RuntimeFrame.SourceMidCount;
	RuntimeTotal.SpatialRecordCount = RuntimeFrame.SpatialRecordCount;
	RuntimeTotal.FineHistoryResidentBytes = RuntimeFrame.FineHistoryResidentBytes;
	RuntimeTotal.ProcessWorkingSetBytes = RuntimeFrame.ProcessWorkingSetBytes;
	RuntimeTotal.UObjectCount = RuntimeFrame.UObjectCount;
	RuntimeTotal.LiveUObjectCount = RuntimeFrame.LiveUObjectCount;
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
		const FIntPoint Expected = (Record.bCurrentObservedLocation && Prop->LocalEpoch==Record.Epoch
            ? Prop->CurrentLive.AtlasCells : Record.SpatialMemory.GetSize())
			* Darkwell::MovingPropLab::PresentationSamples;
		if (!Texture || Texture->GetSizeX() != Expected.X || Texture->GetSizeY() != Expected.Y)
		{
			return false;
		}
        if(Record.bCurrentObservedLocation && Prop->LocalEpoch==Record.Epoch)
        {
            if(!Prop->Actual.IsValid() || !Visual) return false;
            const auto Sources=Prop->Actual->Memory->GetMemoryPrimitives();
            if(Sources.Num()!=Prop->CurrentLive.Parts.Num() || Visual->LiveTextures.Num()!=Sources.Num()) return false;
            for(int32 I=0;I<Sources.Num();++I)
            {
                const auto& Part=Prop->CurrentLive.Parts[I]; const auto Atlas=Part.AtlasCells*4;
                const auto Logical=Part.Raster.GetSize()*4; const auto B=Part.Raster.GetBounds();
                auto* MID=Cast<UMaterialInstanceDynamic>(Sources[I]->GetMaterial(0));
                const auto* LiveTexture=Visual->LiveTextures[I].Get();
                if(!MID || !LiveTexture || LiveTexture->GetSizeX()!=Atlas.X || LiveTexture->GetSizeY()!=Atlas.Y
                    || Logical.X>Atlas.X || Logical.Y>Atlas.Y || Visual->LivePixels[I].Num()!=Logical.X*Logical.Y
                    || MID->K2_GetTextureParameterValue(TEXT("SpatialStateTexture"))!=LiveTexture) return false;
                const FVector2D Extent=B.GetSize()*FVector2D(Atlas)/FVector2D(Logical);
                if(!MID->K2_GetVectorParameterValue(TEXT("SpatialMinInv")).Equals(
                    FLinearColor(B.Min.X,B.Min.Y,1/Extent.X,1/Extent.Y),1.e-5f)) return false;
            }
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

bool ADarkwellMovingPropLabRoom::SetTrackedTransformForTesting(
	const FName StableId, const FTransform& Transform)
{
	FTrackedProp* Prop = Tracked.Find(StableId);
	ADarkwellPropLabFurniture* Actual = Prop ? Prop->Actual.Get() : nullptr;
	if (!Actual || Transform.ContainsNaN())
	{
		return false;
	}
	Actual->SetActorTransform(Transform);
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

USightWeaveObjectPolicyComponent* ADarkwellMovingPropLabRoom::GetObjectPolicyForTesting(FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	return Prop ? Prop->ObjectPolicy.Get() : nullptr;
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

bool ADarkwellMovingPropLabRoom::IsCaptureEligible(const FTrackedProp& Prop) const
{
	return Prop.ObjectPolicy.IsValid() && Prop.ObjectPolicy->IsHistoryEligible()
		&& (Prop.ObjectPolicy->GetResolvedRevealMode()==ESightWeaveRevealMode::SpatialPartial || Prop.RevealObservation.IsConfirmed());
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

bool ADarkwellMovingPropLabRoom::IsRevealConfirmedForTesting(FName StableId) const
{
	const auto* Prop=Tracked.Find(StableId); return Prop && Prop->RevealObservation.IsConfirmed();
}

float ADarkwellMovingPropLabRoom::GetCurrentPresentationMinimumForTesting(FName StableId) const
{
	const auto* Prop=Tracked.Find(StableId); if(!Prop || Prop->History.GetCurrentIndex()==INDEX_NONE) return 0;
	const auto& Record=Prop->History.GetRecords()[Prop->History.GetCurrentIndex()];
	const auto* Visual=Prop->Visuals.Find(Record.Epoch); if(!Visual || Visual->LivePixels.IsEmpty()) return 0;
	float Minimum=1;
	for(const auto& Pixels:Visual->LivePixels) for(const auto& P:Pixels) Minimum=FMath::Min(Minimum,P.R);
	return Minimum;
}

FString ADarkwellMovingPropLabRoom::GetRevealPolicyTelemetry(FName StableId) const
{
	const auto* Prop=Tracked.Find(StableId); if(!Prop) return TEXT("{}");
	const auto P=Prop->ObjectPolicy->GetResolvedPolicy();
	return FString::Printf(TEXT("{\"reveal_mode\":%d,\"history_mode\":%d,\"minimum_span_cm\":%.3f,\"effective_span_cm\":%.3f,\"observed_span_cm\":%.3f,\"confirmed\":%s,\"legal_samples\":%d,\"tentative_samples\":%d,\"history_eligible\":%s,\"capture_eligible\":%s}"),
		int32(P.RevealMode),int32(P.HistoryMode),P.MinimumObservedSpanCm,Prop->RevealObservation.GetEffectiveMinimumSpanCm(),Prop->RevealObservation.GetObservedSpanCm(),Prop->RevealObservation.IsConfirmed()?TEXT("true"):TEXT("false"),Prop->CurrentLegalObservationMask.CountSetBits(),Prop->RevealObservation.GetTentativeMask().CountSetBits(),Prop->ObjectPolicy->IsHistoryEligible()?TEXT("true"):TEXT("false"),IsCaptureEligible(*Prop)?TEXT("true"):TEXT("false"));
}

bool ADarkwellMovingPropLabRoom::IsCurrentSourceVisibleForTesting(FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	if (!Prop || !Prop->Actual.IsValid()) return false;
	for (const UStaticMeshComponent* Mesh : Prop->Actual->Memory->GetMemoryPrimitives())
		if (Mesh && Mesh->IsVisible()) return true;
	return false;
}

bool ADarkwellMovingPropLabRoom::CurrentHasOnlyLivePresentationForTesting(FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	if (!Prop) return false;
	const int32 Index = Prop->History.GetCurrentIndex();
	if (Index == INDEX_NONE) return true;
	const FRecordVisual* Visual = Prop->Visuals.Find(Prop->History.GetRecords()[Index].Epoch);
	if (!Visual) return false;
	for (const FLinearColor& Pixel : Visual->SubmittedPresentation)
		if (Pixel.B > 0 || (Pixel.A < FDarkwellSpatialPropMemory::LegalCoverage && Pixel.R > 0)) return false;
	return true;
}

FString ADarkwellMovingPropLabRoom::GetHistoryPolicyTelemetry(FName StableId) const
{
	const FTrackedProp* Prop = Tracked.Find(StableId);
	if (!Prop || !Prop->ObjectPolicy.IsValid()) return TEXT("HISTORY MODE: UNREGISTERED");
	const auto* Policy = Prop->ObjectPolicy.Get();
	const auto Mode = Policy->GetResolvedHistoryMode();
	const TCHAR* Name = Mode == ESightWeaveHistoryMode::Always ? TEXT("Always")
		: Mode == ESightWeaveHistoryMode::StationaryOnly ? TEXT("StationaryOnly") : TEXT("Never");
	return FString::Printf(TEXT("HISTORY MODE %s | MOVING %d | HISTORY ELIGIBLE %d | FRESH REQUIRED %d\nCURRENT RECORDS %d | STALE RECORDS %d | PROXIES %d | CAPS %d"),
		Name, Policy->IsSightWeaveMoving(), Policy->IsHistoryEligible(), Policy->RequiresFreshStationaryObservation(),
		GetCurrentEpochCountForTesting(StableId), GetStaleEpochCountForTesting(StableId),
		GetVisibleHistoricalProxyCountForTesting(StableId), GetVisibleHistoricalCapCountForTesting(StableId));
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

bool ADarkwellMovingPropLabRoom::GetNewestCaptureMasksForTesting(FName Id,TBitArray<>& Capture,TBitArray<>& Frozen) const
{
 Capture.Empty(); Frozen.Empty(); const auto* P=Tracked.Find(Id); if(!P) return false;
 const FDarkwellSpatialObservationRecord* Latest=nullptr;
 for(const auto& R:P->History.GetRecords()) if(!R.bCurrentObservedLocation && R.FineHistory.IsInitialized() && (!Latest || R.Epoch>Latest->Epoch)) Latest=&R;
 if(!Latest) return false;
 Capture=Latest->LastLegalCaptureMask; Frozen.Init(false,Latest->FineHistory.GetSamples().Num());
 for(int32 I=0;I<Frozen.Num();++I) Frozen[I]=Latest->FineHistory.GetSamples()[I].InitialRemembered>0;
 return true;
}

bool ADarkwellMovingPropLabRoom::GetDividerMaskDiagnosticsForTesting(
	const FName StableId,FDividerMaskDiagnostics& Out) const
{
	Out=FDividerMaskDiagnostics();
	const FTrackedProp* Prop=Tracked.Find(StableId);
	if(!Prop) return false;
	const FDarkwellSpatialObservationRecord* Focus=nullptr;
	const int32 CurrentIndex=Prop->History.GetCurrentIndex();
	if(Prop->History.GetRecords().IsValidIndex(CurrentIndex)) Focus=&Prop->History.GetRecords()[CurrentIndex];
	for(const FDarkwellSpatialObservationRecord& Record:Prop->History.GetRecords())
		if(!Focus || (!Record.bCurrentObservedLocation && Record.Epoch>Focus->Epoch)) Focus=&Record;
	if(!Focus) return false;
	const FBox2D Bounds=Focus->FineHistory.IsInitialized()?Focus->FineHistory.GetBounds():Focus->SpatialMemory.GetBounds();
	const FIntPoint Size=Focus->FineHistory.IsInitialized()?Focus->FineHistory.GetSize()
		:Focus->SpatialMemory.GetSize()*FDarkwellHistoryGridV2::SamplesPerCell;
	if(!Prop->CurrentLive.BuildFullGeometryMask(Bounds,Size,Out.FullGeometryMask)) return false;
	const int32 Count=Size.X*Size.Y;
	Out.RawLiveCoverage.Init(false,Count);
	Out.PhysicalOcclusionGate.Init(false,Count);
	Out.WholePresentationMask.Init(false,Count);
	Out.CapMask.Init(false,Count);
	Out.FinalCurrentContribution.Init(false,Count);
	Out.FinalHistoricalContribution.Init(false,Count);
	Out.CurrentLegalObservationMask=Prop->CurrentLegalObservationMask;
	Out.LastLegalCaptureMask=Focus->LastLegalCaptureMask;
	Out.FrozenHistoryMask.Init(false,Count);
	if(Focus->FineHistory.IsInitialized()) for(int32 Index=0;Index<Count;++Index)
		Out.FrozenHistoryMask[Index]=Focus->FineHistory.GetSamples()[Index].InitialRemembered>0;
	Out.bObjectHasLegalContact=Prop->bCachedWholeLegalContact;
	const FVector2D Step=Bounds.GetSize()/FVector2D(Size);
	const UDarkwellFogVisualSubsystem* Fog=GetWorld()?GetWorld()->GetSubsystem<UDarkwellFogVisualSubsystem>():nullptr;
	auto SampleHistorical=[&](const FVector2D World)
	{
		for(const FDarkwellSpatialObservationRecord& Record:Prop->History.GetRecords())
		{
			if(Record.bCurrentObservedLocation || !Record.FineHistory.IsInitialized()) continue;
			const FRecordVisual* Visual=Prop->Visuals.Find(Record.Epoch);
			if(!Visual || Visual->bPresentationRetired) continue;
			const FBox2D& HistoricalBounds=Record.FineHistory.GetBounds();
			if(!HistoricalBounds.IsInside(World)) continue;
			const FIntPoint HistoricalSize=Record.FineHistory.GetSize();
			const FVector2D UV=(World-HistoricalBounds.Min)/HistoricalBounds.GetSize();
			const int32 X=FMath::Clamp(FMath::FloorToInt(UV.X*HistoricalSize.X),0,HistoricalSize.X-1);
			const int32 Y=FMath::Clamp(FMath::FloorToInt(UV.Y*HistoricalSize.Y),0,HistoricalSize.Y-1);
			if(Record.FineHistory.GetSamples()[Y*HistoricalSize.X+X].Opacity>0) return true;
		}
		return false;
	};
	for(int32 Y=0;Y<Size.Y;++Y) for(int32 X=0;X<Size.X;++X)
	{
		const int32 Index=Y*Size.X+X;
		const FVector2D Min=Bounds.Min+Step*FVector2D(X,Y);
		const FVector2D Points[]{Min,Min+FVector2D(Step.X,0),Min+Step,Min+FVector2D(0,Step.Y),Min+Step*.5};
		if(Prop->LastCoverageBounds.bIsValid && Prop->LastCoverageSize.X>0 && Prop->LastCoverageSize.Y>0
			&& Prop->CachedCurrentCoverage.Num()==Prop->LastCoverageSize.X*Prop->LastCoverageSize.Y)
		{
			const FVector2D UV=(Points[4]-Prop->LastCoverageBounds.Min)/Prop->LastCoverageBounds.GetSize();
			const int32 RawX=FMath::Clamp(FMath::FloorToInt(UV.X*Prop->LastCoverageSize.X),0,Prop->LastCoverageSize.X-1);
			const int32 RawY=FMath::Clamp(FMath::FloorToInt(UV.Y*Prop->LastCoverageSize.Y),0,Prop->LastCoverageSize.Y-1);
			Out.RawLiveCoverage[Index]=Prop->CachedCurrentCoverage[RawY*Prop->LastCoverageSize.X+RawX]>=FDarkwellSpatialPropMemory::LegalCoverage;
		}
		bool bCurrent=false;
		for(const FVector2D Point:Points) bCurrent|=Prop->CurrentLive.HasObservedContributionAt(Point);
		Out.WholePresentationMask[Index]=bCurrent;
		Out.FinalCurrentContribution[Index]=bCurrent;
		Out.FinalHistoricalContribution[Index]=SampleHistorical(Points[4]);
		float Gate=1;
		if(Fog) for(const FVector2D Point:Points)
		{
			const auto Query=Fog->QueryObjectOcclusionAtWorldPoint(Point);
			Gate=FMath::Min(Gate,Query.bValid?Query.Coverage:0.f);
		}
		Out.PhysicalOcclusionGate[Index]=Gate>=FDarkwellSpatialPropMemory::LegalCoverage;
	}
	for(const TPair<uint32,FRecordVisual>& Pair:Prop->Visuals) if(Pair.Value.CapTriangles>0)
		for(const FVector2D Point:Pair.Value.CapSamplePoints) if(Bounds.IsInside(Point))
		{
			const FVector2D UV=(Point-Bounds.Min)/Bounds.GetSize();
			const int32 X=FMath::Clamp(FMath::FloorToInt(UV.X*Size.X),0,Size.X-1);
			const int32 Y=FMath::Clamp(FMath::FloorToInt(UV.Y*Size.Y),0,Size.Y-1);
			Out.CapMask[Y*Size.X+X]=true;
		}
	bool bOverlap=false;
	for(int32 Index=0;Index<Count;++Index) bOverlap|=Out.FinalCurrentContribution[Index] && Out.FinalHistoricalContribution[Index];
	if(bOverlap) Out.Source=FDarkwellCurrentLiveGrid::EDividerSource::MixedCurrentHistory;
	else if(Focus->bConfirmedWholeCapture && Out.CapMask.CountSetBits()>0) Out.Source=FDarkwellCurrentLiveGrid::EDividerSource::HistoryCap;
	else if(!Focus->bCurrentObservedLocation && Focus->bConfirmedWholeCapture && Out.FrozenHistoryMask!=Out.FullGeometryMask) Out.Source=FDarkwellCurrentLiveGrid::EDividerSource::HistorySurface;
	else if(Focus->bCurrentObservedLocation && Focus->bConfirmedWholeCapture)
	{
		TBitArray<> Expected=Out.FullGeometryMask;
		Expected.CombineWithBitwiseAND(Out.PhysicalOcclusionGate,EBitwiseOperatorFlags::MinSize);
		const bool bRawSplit=Out.RawLiveCoverage.CountSetBits()>0 && Out.RawLiveCoverage.CountSetBits()<Count;
		const bool bPhysicalSplit=Out.PhysicalOcclusionGate.CountSetBits()>0 && Out.PhysicalOcclusionGate.CountSetBits()<Count;
		if(bPhysicalSplit) Out.Source=FDarkwellCurrentLiveGrid::EDividerSource::WallOcclusion;
		else if(Out.WholePresentationMask!=Expected) Out.Source=bRawSplit
			?FDarkwellCurrentLiveGrid::EDividerSource::ViewEdge:FDarkwellCurrentLiveGrid::EDividerSource::WholeCurrentMask;
	}
	else if(Out.RawLiveCoverage.CountSetBits()>0 && Out.RawLiveCoverage.CountSetBits()<Count)
		Out.Source=FDarkwellCurrentLiveGrid::EDividerSource::PartialCurrentMask;
	return true;
}

FString ADarkwellMovingPropLabRoom::GetDividerMaskTelemetryForTesting(const FName StableId) const
{
	FDividerMaskDiagnostics Diagnostic;
	if(!GetDividerMaskDiagnosticsForTesting(StableId,Diagnostic)) return TEXT("{\"divider_source\":\"UNKNOWN\",\"valid\":false}");
	auto Hash=[](const TBitArray<>& Mask)
	{
		uint64 Value=1469598103934665603ull;
		for(TConstSetBitIterator<> It(Mask);It;++It) Value=(Value^uint64(It.GetIndex()+1))*1099511628211ull;
		return Value;
	};
	return FString::Printf(TEXT("{\"divider_source\":\"%s\",\"valid\":true,\"full_geometry\":{\"set\":%d,\"hash\":\"%llu\"},\"raw_live\":{\"set\":%d,\"hash\":\"%llu\"},\"object_contact\":%s,\"physical_occlusion\":{\"set\":%d,\"hash\":\"%llu\"},\"whole_presentation\":{\"set\":%d,\"hash\":\"%llu\"},\"current_legal\":{\"set\":%d,\"hash\":\"%llu\"},\"last_capture\":{\"set\":%d,\"hash\":\"%llu\"},\"frozen_history\":{\"set\":%d,\"hash\":\"%llu\"},\"cap\":{\"set\":%d,\"hash\":\"%llu\"},\"final_current\":{\"set\":%d,\"hash\":\"%llu\"},\"final_history\":{\"set\":%d,\"hash\":\"%llu\"}}"),
		FDarkwellCurrentLiveGrid::DividerSourceName(Diagnostic.Source),
		Diagnostic.FullGeometryMask.CountSetBits(),Hash(Diagnostic.FullGeometryMask),
		Diagnostic.RawLiveCoverage.CountSetBits(),Hash(Diagnostic.RawLiveCoverage),
		Diagnostic.bObjectHasLegalContact?TEXT("true"):TEXT("false"),
		Diagnostic.PhysicalOcclusionGate.CountSetBits(),Hash(Diagnostic.PhysicalOcclusionGate),
		Diagnostic.WholePresentationMask.CountSetBits(),Hash(Diagnostic.WholePresentationMask),
		Diagnostic.CurrentLegalObservationMask.CountSetBits(),Hash(Diagnostic.CurrentLegalObservationMask),
		Diagnostic.LastLegalCaptureMask.CountSetBits(),Hash(Diagnostic.LastLegalCaptureMask),
		Diagnostic.FrozenHistoryMask.CountSetBits(),Hash(Diagnostic.FrozenHistoryMask),
		Diagnostic.CapMask.CountSetBits(),Hash(Diagnostic.CapMask),
		Diagnostic.FinalCurrentContribution.CountSetBits(),Hash(Diagnostic.FinalCurrentContribution),
		Diagnostic.FinalHistoricalContribution.CountSetBits(),Hash(Diagnostic.FinalHistoricalContribution));
}

uint64 ADarkwellMovingPropLabRoom::GetRevealSpanEvaluationsForTesting(FName Id) const
{ const auto* P=Tracked.Find(Id); return P?P->RevealObservation.GetSpanEvaluations():0; }
FString ADarkwellMovingPropLabRoom::GetMemorySeamAuditForTesting(FName Id) const
{
	const FTrackedProp* Prop = Tracked.Find(Id);
	if (!Prop) return TEXT("{}");
	TArray<FString> Records;
	for (const auto& Record : Prop->History.GetRecords())
	{
		if (Record.bCurrentObservedLocation || !Record.FineHistory.IsInitialized()) continue;
		const auto* Visual = Prop->Visuals.Find(Record.Epoch);
		const auto Size = Record.FineHistory.GetSize();
		const auto Bounds = Record.FineHistory.GetBounds();
		const int32 Y = Size.Y / 2;
		TArray<FString> Row, Caps;
		for (int32 X = 0; X < Size.X; ++X)
		{
			const int32 I = Y * Size.X + X;
			const auto& S = Record.FineHistory.GetSamples()[I];
			const FLinearColor P = Visual && Visual->SubmittedPresentation.IsValidIndex(I)
				? Visual->SubmittedPresentation[I] : FLinearColor::Transparent;
			Row.Add(FString::Printf(TEXT("[%.4f,%d,%.4f,%.4f,%.4f]"),
				Bounds.Min.X + (X + .5) * Bounds.GetSize().X / Size.X,
				S.State == FDarkwellHistoryGridV2::Unresolved() ? 1 : S.State == FDarkwellHistoryGridV2::Superseded() ? 3 : S.State == FDarkwellHistoryGridV2::VerifiedEmpty() ? 2 : 0,
				S.FrozenAAEnvelope, P.B, P.A));
		}
		if (Visual) for (const auto& Q : Visual->CapQuads)
		{
			if (FMath::Abs(Q.A.X - Q.B.X) < .001)
				Caps.Add(FString::Printf(TEXT("[%.4f,%.4f,%.4f]"), Q.A.X, Q.A.Y, Q.B.Y));
		}
		Records.Add(FString::Printf(TEXT("{\"epoch\":%u,\"y\":%.4f,\"row\":[%s],\"vertical_caps\":[%s]}"),
			Record.Epoch, Bounds.Min.Y + (Y + .5) * Bounds.GetSize().Y / Size.Y,
			*FString::Join(Row, TEXT(",")), *FString::Join(Caps, TEXT(","))));
	}
	return FString::Printf(TEXT("{\"records\":[%s]}"), *FString::Join(Records, TEXT(",")));
}

bool ADarkwellMovingPropLabRoom::IsWholePresentationUniformForTesting(FName Id) const
{
 const auto* P=Tracked.Find(Id);
 return P && P->CurrentLive.IsUniformWholePresentation() && P->CurrentLegalObservationMask.Num()==0
  && P->RevealObservation.GetTentativeMask().Num()==0 && P->CurrentLive.SamplesTouched==0;
}

void ADarkwellMovingPropLabRoom::ForceContributionRefreshForTesting(FName Id)
{
#if WITH_DEV_AUTOMATION_TESTS
 TGuardValue<bool> ReferenceScope(bForceFullHistoryEvidenceForTesting,true);
#endif
 if(auto* P=Tracked.Find(Id)) { P->DiagnosticSignature=0; RefreshContributionDiagnostics(*P); }
}

bool ADarkwellMovingPropLabRoom::DoesFrameOccupancyMatchOracleForTesting(FName Id)
{
 const auto* P=Tracked.Find(Id); if(!P || FrameOccupancy.IsEmpty()) return false;
 bool Tested=false;
 for(const auto& R:P->History.GetRecords()) if(R.FineHistory.IsInitialized())
 {
  const auto Size=R.FineHistory.GetSize(); const auto B=R.FineHistory.GetBounds(); const auto Step=B.GetSize()/FVector2D(Size);
  for(int32 Y=0;Y<Size.Y;Y+=3) for(int32 X=0;X<Size.X;X+=3)
  {
   const auto Point=B.Min+Step*FVector2D(X+.5,Y+.5); bool Oracle,Cached;
   { TGuardValue<bool> Scope(bUseFrameOccupancy,false); Oracle=IsOccupiedByActual(Point,NAME_None); }
   { TGuardValue<bool> Scope(bUseFrameOccupancy,true); Cached=IsOccupiedByActual(Point,NAME_None); }
   if(Oracle!=Cached) return false; Tested=true;
  }
 }
 return Tested;
}

#if WITH_DEV_AUTOMATION_TESTS
TArray<TWeakObjectPtr<UObject>> ADarkwellMovingPropLabRoom::GetOwnedPresentationObjectsForTesting() const
{
 TArray<TWeakObjectPtr<UObject>> Objects;
 for(const auto& T:OwnedTextures) Objects.Add(T.Get());
 for(const auto& M:OwnedMaterials) Objects.Add(M.Get());
 for(const auto& C:OwnedCaps) Objects.Add(C.Get());
 for(const auto& Pair:Tracked)
 {
  const auto& Prop=Pair.Value;
  Objects.Add(Prop.Actual.Get()); Objects.Add(Prop.ObjectPolicy.Get());
  for(const auto& Visual:Prop.Visuals) if(Visual.Value.Proxy.IsValid()) Objects.Add(Visual.Value.Proxy.Get());
  if(const auto* Actual=Prop.Actual.Get())
   for(const UStaticMeshComponent* Part:Actual->Memory->GetMemoryPrimitives()) if(Part)
    for(int32 Index=0;Index<Part->GetNumMaterials();++Index)
     if(auto* Material=Cast<UMaterialInstanceDynamic>(Part->GetMaterial(Index))) Objects.Add(Material);
 }
 return Objects;
}
#endif
