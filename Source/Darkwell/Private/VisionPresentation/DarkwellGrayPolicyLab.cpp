#include "VisionPresentation/DarkwellGrayPolicyLab.h"

#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Player/DarkwellCharacter.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "SlateOptMacros.h"
#include "Styling/SlateColor.h"
#include "VisionPresentation/DarkwellMovingPropLabRoom.h"
#include "VisionPresentation/DarkwellPropGameplayLab.h"
#include "VisionPresentation/DarkwellRememberablePropComponent.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DarkwellGrayPolicyLab"

namespace
{
	const FVector RoomCenters[] = {
		FVector(0, 0, 0),
		FVector(-6000, -3500, 0),
		FVector(0, -6500, 0),
		FVector(6000, -3500, 0),
		FVector(-6000, 3500, 0),
		FVector(0, 6500, 0),
		FVector(6000, 3500, 0)
	};

	FString FontPath()
	{
		return FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansFallback.ttf");
	}

	FSlateFontInfo ChineseFont(const int32 Size)
	{
		return FSlateFontInfo(FontPath(), Size);
	}

	FVector RoomControlPosition(const int32 Room, const float YOffset)
	{
		return RoomCenters[FMath::Clamp(Room, 0, 6)] + FVector(-850, YOffset, 55);
	}

	FText StressModeName(const int32 Mode)
	{
		switch (Mode)
		{
		case 1: return LOCTEXT("StressOne", "1 个对象");
		case 2: return LOCTEXT("StressEight", "8 个对象");
		case 3: return LOCTEXT("StressThirtyTwo", "32 个对象");
		case 4: return LOCTEXT("StressOverlap", "64 条同区域历史");
		case 5: return LOCTEXT("StressStable", "64 条同 StableID 多姿态历史");
		case 6: return LOCTEXT("StressDistributed", "184 条分布历史");
		case 7: return LOCTEXT("StressMixed", "六种策略混合");
		default: return LOCTEXT("StressOff", "空房间基线（压力关闭）");
		}
	}
}

bool Darkwell::GrayPolicyLab::IsWorld(const UWorld* World)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!World) return false;
	FString Name = World->GetOutermost()->GetName();
	if (!World->StreamingLevelsPrefix.IsEmpty())
	{
		Name.ReplaceInline(*World->StreamingLevelsPrefix, TEXT(""));
	}
	return Name == MapPath;
#else
	return false;
#endif
}

void UDarkwellGrayPolicyWorldLabelWidget::SetLabel(const FText& InLabel, const int32 InFontSize)
{
	Label = InLabel;
	FontSize = InFontSize;
	if (LabelText.IsValid())
	{
		LabelText->SetText(Label);
		LabelText->SetFont(ChineseFont(FontSize));
	}
}

TSharedRef<SWidget> UDarkwellGrayPolicyWorldLabelWidget::RebuildWidget()
{
	return SNew(SBorder)
		.Padding(FMargin(12.0f, 6.0f))
		.BorderBackgroundColor(FLinearColor(0.012f, 0.018f, 0.025f, 0.88f))
		[
			SAssignNew(LabelText, STextBlock)
			.Text(Label)
			.Font(ChineseFont(FontSize))
			.ColorAndOpacity(FLinearColor(0.92f, 0.96f, 1.0f, 1.0f))
			.Justification(ETextJustify::Center)
		];
}

ADarkwellGrayPolicyLabControl::ADarkwellGrayPolicyLabControl()
{
	ControlRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ControlRoot"));
	SetRootComponent(ControlRoot);
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(ControlRoot);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Body->SetStaticMesh(Cylinder.Object);
	Body->SetRelativeScale3D(FVector(1.15f, 1.15f, 0.10f));
	Body->SetCollisionProfileName(TEXT("BlockAll"));
	Body->SetCastShadow(false);

	LabelComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("ChineseLabel"));
	LabelComponent->SetupAttachment(ControlRoot);
	LabelComponent->SetRelativeLocation(FVector(0, 0, 155));
	LabelComponent->SetRelativeRotation(FRotator(90, 0, 0));
	LabelComponent->SetWidgetSpace(EWidgetSpace::World);
	LabelComponent->SetDrawSize(FVector2D(680, 128));
	LabelComponent->SetPivot(FVector2D(0.5f, 0.5f));
	LabelComponent->SetTwoSided(true);
	LabelComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LabelComponent->SetWidgetClass(UDarkwellGrayPolicyWorldLabelWidget::StaticClass());
}

void ADarkwellGrayPolicyLabControl::Configure(
	ADarkwellSightWeaveGrayPolicyLabDirector* InDirector,
	const EDarkwellGrayPolicyLabControlKind InKind,
	const FText& InLabel)
{
	Director = InDirector;
	Kind = InKind;
	Label = InLabel;
	LabelComponent->InitWidget();
	if (auto* Widget = Cast<UDarkwellGrayPolicyWorldLabelWidget>(LabelComponent->GetWidget()))
	{
		Widget->SetLabel(Label, 27);
	}
}

bool ADarkwellGrayPolicyLabControl::CanInteract(const ADarkwellCharacter& Character) const
{
	return Director.IsValid() && Director->CanActivateControl(Kind);
}

void ADarkwellGrayPolicyLabControl::Interact(ADarkwellCharacter& Character)
{
	if (Director.IsValid()) Director->ActivateControl(Kind, Character);
}

FText ADarkwellGrayPolicyLabControl::GetInteractionPrompt(const ADarkwellCharacter& Character) const
{
	return Director.IsValid() ? Director->GetControlPrompt(Kind) : FText::GetEmpty();
}

void ADarkwellGrayPolicyLabControl::OnInteractionFocusChanged(const bool bInFocused)
{
	bFocused = bInFocused;
	Body->SetCustomDepthStencilValue(bFocused ? 252 : 0);
	Body->SetRenderCustomDepth(bFocused);
}

ADarkwellSightWeaveGrayPolicyLabDirector::ADarkwellSightWeaveGrayPolicyLabDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	TInlineComponentArray<UStaticMeshComponent*> InheritedMeshes(this);
	for (UStaticMeshComponent* Mesh : InheritedMeshes)
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	GetRememberablePropComponent()->ConfigureStableId(NAME_None);
}

void ADarkwellSightWeaveGrayPolicyLabDirector::BeginPlay()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Darkwell_GrayPolicyLab_BeginPlay);
	Super::BeginPlay();
	if (!Darkwell::GrayPolicyLab::IsWorld(GetWorld()))
	{
		SetActorTickEnabled(false);
		return;
	}
	CheckoutSha = ResolveCheckoutSha();
	BuildEnvironment();
	BuildControls();
	AttachScreenGuidance();
	ADarkwellMovingPropLabRoom* Room = GetWorld()->SpawnActor<ADarkwellMovingPropLabRoom>();
	RuntimeRoom = Room;
	ADarkwellCharacter* Character = Cast<ADarkwellCharacter>(GetWorld()->GetFirstPlayerController()
		? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr);
	if (Room && Character && Room->ConfigureForGrayPolicyLab(Character))
	{
		bInitialized = true;
		TeleportPlayer(*Character, 0);
	}
}

void ADarkwellSightWeaveGrayPolicyLabDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	DetachScreenGuidance();
	if (ADarkwellMovingPropLabRoom* Room = RuntimeRoom.Get()) Room->Destroy();
	RuntimeRoom.Reset();
	Super::EndPlay(Reason);
}

void ADarkwellSightWeaveGrayPolicyLabDirector::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TRACE_CPUPROFILER_EVENT_SCOPE(Darkwell_GrayPolicyLab_Tick);
	if (!bInitialized) return;
	ADarkwellCharacter* Character = Cast<ADarkwellCharacter>(GetWorld()->GetFirstPlayerController()
		? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr);
	ADarkwellMovingPropLabRoom* Room = RuntimeRoom.Get();
	if (!Character || !Room) return;
	UpdateSweep(DeltaSeconds, *Character);
	RefreshSpatialPresentation();
	Room->UpdateRoom(DeltaSeconds, Character);
	UpdateFrameStatistics(DeltaSeconds);
	EmitHitchIfNeeded(DeltaSeconds);
}

FBox2D ADarkwellSightWeaveGrayPolicyLabDirector::GetSightWeaveFloorBounds() const
{
	return FBox2D(FVector2D(-7800, -7900), FVector2D(7800, 7900));
}

void ADarkwellSightWeaveGrayPolicyLabDirector::BuildSightWeaveOccluderSegments(
	TArray<FDarkwellVisionIntegrationSegment>& OutSegments) const
{
	OutSegments = Occluders;
}

void ADarkwellSightWeaveGrayPolicyLabDirector::BuildSightWeaveStaticSurfaces(
	TArray<FDarkwellVisionIntegrationSurface>& OutSurfaces) const
{
	OutSurfaces.Reset(1);
	auto& Floor = OutSurfaces.AddDefaulted_GetRef();
	const FBox2D Bounds = GetSightWeaveFloorBounds();
	Floor.WorldFootprint = {Bounds.Min, FVector2D(Bounds.Max.X, Bounds.Min.Y), Bounds.Max,
		FVector2D(Bounds.Min.X, Bounds.Max.Y)};
	Floor.NeutralIntensity = 90;
}

bool ADarkwellSightWeaveGrayPolicyLabDirector::EnableDarkwellProjectFogP4(
	UTexture* LiveCoverageTexture, const FVector2D WorldMin, const FVector2D InvWorldExtent)
{
	if (!LiveCoverageTexture || !Darkwell::GrayPolicyLab::IsWorld(GetWorld())) return false;
	RawCoverage = LiveCoverageTexture;
	FogMin = WorldMin;
	FogInv = InvWorldExtent;
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Darkwell/Vision/PropLab/M_PropLabSurface.M_PropLabSurface"));
	if (!Parent) return false;
	for (int32 Index = 0; Index < LabMeshes.Num(); ++Index)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, this);
		Material->SetTextureParameterValue(TEXT("DarkwellLiveCoverageTexture"), LiveCoverageTexture);
		Material->SetVectorParameterValue(TEXT("FogWorldMin"), FLinearColor(WorldMin.X, WorldMin.Y, 0, 0));
		Material->SetVectorParameterValue(TEXT("FogWorldInvExtent"), FLinearColor(InvWorldExtent.X, InvWorldExtent.Y, 0, 0));
		Material->SetVectorParameterValue(TEXT("OriginalBaseColorTint"),
			Index < 7 ? FLinearColor(.12f, .15f, .18f) : FLinearColor(.28f, .32f, .36f));
		Material->SetScalarParameterValue(TEXT("OriginalUVScale"), Index < 7 ? 18.0f : 3.0f);
		LabMeshes[Index]->SetMaterial(0, Material);
		LabMaterials.Add(Material);
	}
	RefreshSpatialPresentation();
	return true;
}

void ADarkwellSightWeaveGrayPolicyLabDirector::DisableDarkwellProjectFog()
{
	RawCoverage = nullptr;
	LabMaterials.Reset();
}

void ADarkwellSightWeaveGrayPolicyLabDirector::BuildEnvironment()
{
	AddAreaFloor(RoomCenters[0], FVector2D(2200, 1800), FLinearColor(.10f, .13f, .17f));
	for (int32 Room = 1; Room <= 6; ++Room)
	{
		AddAreaFloor(RoomCenters[Room], FVector2D(2600, 2000),
			FLinearColor(.11f + Room * .012f, .14f, .17f));
		AddWall(RoomCenters[Room] + FVector(0, 1000, 125), FVector(2600, 30, 250),
			FLinearColor(.31f, .35f, .39f), true);
		AddWall(RoomCenters[Room] + FVector(-1300, 0, 125), FVector(30, 2000, 250),
			FLinearColor(.28f, .32f, .36f), false);
		AddWall(RoomCenters[Room] + FVector(1300, 0, 125), FVector(30, 2000, 250),
			FLinearColor(.28f, .32f, .36f), false);
	}
	// Room 05 has one opaque wall split around a narrow observation gap.
	const FVector R5 = RoomCenters[5];
	AddWall(R5 + FVector(-780, 100, 125), FVector(1040, 30, 250), FLinearColor(.20f, .22f, .25f), true);
	AddWall(R5 + FVector(780, 100, 125), FVector(1040, 30, 250), FLinearColor(.20f, .22f, .25f), true);
}

void ADarkwellSightWeaveGrayPolicyLabDirector::AddAreaFloor(
	const FVector Center, const FVector2D Size, const FLinearColor& Tint)
{
	AddWall(Center + FVector(0, 0, -15), FVector(Size.X, Size.Y, 30), Tint, false);
}

void ADarkwellSightWeaveGrayPolicyLabDirector::AddWall(
	const FVector Center, const FVector Size, const FLinearColor& Tint, const bool bOccluder)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
	AddInstanceComponent(Mesh);
	Mesh->SetupAttachment(GetRootComponent());
	Mesh->SetStaticMesh(Cube);
	Mesh->SetWorldLocation(Center);
	Mesh->SetWorldScale3D(Size / 100.0f);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetRenderCustomDepth(false);
	Mesh->RegisterComponent();
	LabMeshes.Add(Mesh);
	if (bOccluder)
	{
		const FVector2D Extent = Size.X > Size.Y
			? FVector2D(Size.X * .5f, 0) : FVector2D(0, Size.Y * .5f);
		auto& Segment = Occluders.AddDefaulted_GetRef();
		Segment.A = FVector2D(Center) - Extent;
		Segment.B = FVector2D(Center) + Extent;
		Segment.ZMin = 0;
		Segment.ZMax = Size.Z;
	}
}

ADarkwellGrayPolicyLabControl* ADarkwellSightWeaveGrayPolicyLabDirector::AddControl(
	const EDarkwellGrayPolicyLabControlKind Kind, const FText& Label, const FVector Location)
{
	FTransform Transform(Location);
	auto* Control = GetWorld()->SpawnActorDeferred<ADarkwellGrayPolicyLabControl>(
		ADarkwellGrayPolicyLabControl::StaticClass(), Transform, this, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Control) return nullptr;
	Control->FinishSpawning(Transform);
	Control->Configure(this, Kind, Label);
	Controls.Add(Control);
	return Control;
}

void ADarkwellSightWeaveGrayPolicyLabDirector::BuildControls()
{
	const FText Labels[] = {
		LOCTEXT("EnterWhole", "01 整体显示"), LOCTEXT("EnterPartial", "02 局部切块"),
		LOCTEXT("EnterMoving", "03 移动物体"), LOCTEXT("EnterNever", "04 永不记忆"),
		LOCTEXT("EnterOcclusion", "05 遮挡与快速扫视"), LOCTEXT("EnterStress", "06 性能压力测试")
	};
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const float X = (Index % 3 - 1) * 620.0f;
		const float Y = (Index / 3 == 0 ? -360.0f : 360.0f);
		AddControl(static_cast<EDarkwellGrayPolicyLabControlKind>(Index), Labels[Index], FVector(X, Y, 55));
	}
	for (int32 Room = 1; Room <= 6; ++Room)
	{
		AddControl(EDarkwellGrayPolicyLabControlKind::ReturnToLobby, LOCTEXT("Return", "返回大厅"),
			RoomControlPosition(Room, -620));
		AddControl(EDarkwellGrayPolicyLabControlKind::ResetCurrentRoom, LOCTEXT("ResetRoom", "重置当前房间"),
			RoomControlPosition(Room, -260));
	}
	AddControl(EDarkwellGrayPolicyLabControlKind::ToggleMovingSubject, LOCTEXT("ToggleSubject", "切换整体 / 局部对象"), RoomControlPosition(3, 100));
	AddControl(EDarkwellGrayPolicyLabControlKind::StartMove, LOCTEXT("Move", "开始移动"), RoomControlPosition(3, 450));
	AddControl(EDarkwellGrayPolicyLabControlKind::StartRotate, LOCTEXT("Rotate", "开始旋转"), RoomControlPosition(3, 780));
	AddControl(EDarkwellGrayPolicyLabControlKind::AutoSweep90, LOCTEXT("Sweep90", "自动 90° 扫视"), RoomControlPosition(5, 100));
	AddControl(EDarkwellGrayPolicyLabControlKind::AutoSweep160, LOCTEXT("Sweep160", "自动 160° 扫视"), RoomControlPosition(5, 450));
	AddControl(EDarkwellGrayPolicyLabControlKind::RepeatSweep, LOCTEXT("RepeatSweep", "重复往返扫视"), RoomControlPosition(5, 780));
	AddControl(EDarkwellGrayPolicyLabControlKind::CycleStressMode, LOCTEXT("CycleStress", "切换压力档位"), RoomControlPosition(6, 100));
	AddControl(EDarkwellGrayPolicyLabControlKind::RepeatSweep, LOCTEXT("StressTurn", "连续快速转头"), RoomControlPosition(6, 450));
	AddControl(EDarkwellGrayPolicyLabControlKind::ResetStress, LOCTEXT("ResetStress", "完全重置性能房"), RoomControlPosition(6, 780));
}

bool ADarkwellSightWeaveGrayPolicyLabDirector::CanActivateControl(
	const EDarkwellGrayPolicyLabControlKind Kind) const
{
	return bInitialized && RuntimeRoom.IsValid();
}

FText ADarkwellSightWeaveGrayPolicyLabDirector::GetControlPrompt(
	const EDarkwellGrayPolicyLabControlKind Kind) const
{
	return FText::Format(LOCTEXT("ControlPrompt", "按 F：{0}"),
		Kind == EDarkwellGrayPolicyLabControlKind::ReturnToLobby
			? LOCTEXT("PromptReturn", "返回大厅") : LOCTEXT("PromptActivate", "执行此测试操作"));
}

bool ADarkwellSightWeaveGrayPolicyLabDirector::ActivateControl(
	const EDarkwellGrayPolicyLabControlKind Kind, ADarkwellCharacter& Character)
{
	if (!CanActivateControl(Kind)) return false;
	if (Kind <= EDarkwellGrayPolicyLabControlKind::EnterStress)
	{
		TeleportPlayer(Character, static_cast<int32>(Kind) + 1);
		return true;
	}
	switch (Kind)
	{
	case EDarkwellGrayPolicyLabControlKind::ReturnToLobby:
		TeleportPlayer(Character, 0); return true;
	case EDarkwellGrayPolicyLabControlKind::ResetCurrentRoom:
		ResetRoom(CurrentRoom); TeleportPlayer(Character, CurrentRoom); return true;
	case EDarkwellGrayPolicyLabControlKind::ToggleMovingSubject:
		return RuntimeRoom->ToggleGrayPolicyMovingSubject();
	case EDarkwellGrayPolicyLabControlKind::StartMove:
		return RuntimeRoom->StartGrayPolicyMotion(false);
	case EDarkwellGrayPolicyLabControlKind::StartRotate:
		return RuntimeRoom->StartGrayPolicyMotion(true);
	case EDarkwellGrayPolicyLabControlKind::AutoSweep90:
		StartSweep(90.0f, false); return true;
	case EDarkwellGrayPolicyLabControlKind::AutoSweep160:
		StartSweep(160.0f, false); return true;
	case EDarkwellGrayPolicyLabControlKind::RepeatSweep:
		StartSweep(160.0f, true); return true;
	case EDarkwellGrayPolicyLabControlKind::CycleStressMode:
		StressMode = (StressMode + 1) % 8;
		return RuntimeRoom->SetGrayPolicyStressMode(StressMode);
	case EDarkwellGrayPolicyLabControlKind::ResetStress:
		StressMode = 0; bRepeatSweep = false; SweepDuration = 0;
		return RuntimeRoom->SetGrayPolicyStressMode(0);
	default: return false;
	}
}

void ADarkwellSightWeaveGrayPolicyLabDirector::TeleportPlayer(
	ADarkwellCharacter& Character, const int32 Room)
{
	CurrentRoom = FMath::Clamp(Room, 0, 6);
	Character.SetActorLocation(RoomCenters[CurrentRoom] + FVector(0, -500, 92), false, nullptr,
		ETeleportType::TeleportPhysics);
	Character.SetActorRotation(FRotator(0, 90, 0));
	if (USpringArmComponent* Boom = Character.FindComponentByClass<USpringArmComponent>())
	{
		Boom->SetRelativeRotation(FRotator(-65, 90, 0));
		Boom->TargetArmLength = 1250;
		Boom->TargetOffset = FVector::ZeroVector;
	}
}

bool ADarkwellSightWeaveGrayPolicyLabDirector::TeleportToRoomForTesting(
	const int32 Room, ADarkwellCharacter* Character)
{
	if (!Character || Room < 0 || Room > 6) return false;
	TeleportPlayer(*Character, Room);
	return CurrentRoom == Room;
}

void ADarkwellSightWeaveGrayPolicyLabDirector::ResetRoom(const int32 Room)
{
	if (RuntimeRoom.IsValid()) RuntimeRoom->ResetGrayPolicyRoom(Room);
	if (Room == 6)
	{
		StressMode = 0;
		bRepeatSweep = false;
		SweepDuration = 0;
	}
}

bool ADarkwellSightWeaveGrayPolicyLabDirector::ResetCurrentRoomForTesting(ADarkwellCharacter* Character)
{
	if (!Character || CurrentRoom < 1 || CurrentRoom > 6) return false;
	ResetRoom(CurrentRoom);
	TeleportPlayer(*Character, CurrentRoom);
	return true;
}

void ADarkwellSightWeaveGrayPolicyLabDirector::StartSweep(const float Degrees, const bool bRepeat)
{
	ADarkwellCharacter* Character = Cast<ADarkwellCharacter>(GetWorld()->GetFirstPlayerController()
		? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr);
	if (!Character) return;
	SweepStartYaw = Character->GetActorRotation().Yaw;
	SweepTargetYaw = SweepStartYaw + Degrees;
	SweepElapsed = 0;
	SweepDuration = Degrees > 120.0f ? 0.22f : 0.18f;
	bRepeatSweep = bRepeat;
}

void ADarkwellSightWeaveGrayPolicyLabDirector::UpdateSweep(
	const float DeltaSeconds, ADarkwellCharacter& Character)
{
	if (SweepDuration <= 0) return;
	SweepElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(SweepElapsed / SweepDuration, 0.0f, 1.0f);
	Character.SetActorRotation(FRotator(0, FMath::Lerp(SweepStartYaw, SweepTargetYaw, Alpha), 0));
	if (Alpha >= 1.0f)
	{
		if (bRepeatSweep)
		{
			Swap(SweepStartYaw, SweepTargetYaw);
			SweepElapsed = 0;
		}
		else SweepDuration = 0;
	}
}

void ADarkwellSightWeaveGrayPolicyLabDirector::AttachScreenGuidance()
{
	if (!GEngine || !GEngine->GameViewport || ScreenGuidance.IsValid()) return;
	ScreenGuidance = SNew(SBox)
		.WidthOverride(650.0f)
		.MaxDesiredHeight(650.0f)
		.Padding(FMargin(20.0f))
		[
			SNew(SBorder)
			.Padding(FMargin(18.0f))
			.BorderBackgroundColor(FLinearColor(0.008f, 0.012f, 0.018f, 0.88f))
			[
				SNew(STextBlock)
				.Text_Lambda([WeakThis = TWeakObjectPtr<ADarkwellSightWeaveGrayPolicyLabDirector>(this)]
				{
					return WeakThis.IsValid() ? WeakThis->BuildScreenGuidance() : FText::GetEmpty();
				})
				.Font(ChineseFont(19))
				.ColorAndOpacity(FLinearColor(0.92f, 0.95f, 1.0f, 1.0f))
				.WrapTextAt(610.0f)
			]
		];
	GEngine->GameViewport->AddViewportWidgetContent(ScreenGuidance.ToSharedRef(), 50);
}

void ADarkwellSightWeaveGrayPolicyLabDirector::DetachScreenGuidance()
{
	if (GEngine && GEngine->GameViewport && ScreenGuidance.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ScreenGuidance.ToSharedRef());
	}
	ScreenGuidance.Reset();
}

FText ADarkwellSightWeaveGrayPolicyLabDirector::BuildScreenGuidance() const
{
	const FText RoomText = BuildRoomGuidance(CurrentRoom);
	return FText::Format(LOCTEXT("ScreenPanel",
		"SightWeave 灰色层测试实验室\n地图：L_SightWeaveGrayPolicyLab  |  运行 SHA：{0}\n当前房间：{1}\n\n{2}\n\n通用操作：WASD 移动，鼠标转向，面对圆形控制台按 F\n每个房间都有“返回大厅”和“重置当前房间”\n{3}"),
		FText::FromString(CheckoutSha), FText::AsNumber(CurrentRoom), RoomText,
		FText::FromString(CachedStatistics));
}

FText ADarkwellSightWeaveGrayPolicyLabDirector::BuildRoomGuidance(const int32 Room) const
{
	switch (Room)
	{
	case 1: return LOCTEXT("Room1Guidance", "01 整体显示\n测试目标：连续观察跨度达到 100 厘米后，整件物体显示。\n操作步骤：先在近侧短扫（低于 100 cm），移开视野；再沿地面标记完整扫过。\n正确结果：未达标不留灰影；确认后可留下完整灰影。\n异常结果：穿墙确认、未达标留影、确认后仍只有碎片。");
	case 2: return LOCTEXT("Room2Guidance", "02 局部切块\n测试目标：看到多少，灰色层只保留多少。\n操作步骤：分别从左侧、中间、右侧观察长条物体，再移开视野。\n正确结果：未观察部分不出现；局部切口有深灰封口。\n异常结果：整件物体提前出现或切口没有封口。");
	case 3: return LOCTEXT("Room3Guidance", "03 移动物体\n测试目标：移动中不记录新历史；视野外停止不自动产生终点记忆。\n操作步骤：切换整体/局部对象，执行移动或旋转，移开视野；停稳后重新观察。\n正确结果：移动中无新 stale/proxy/cap；重新合法观察后才恢复历史资格。\n异常结果：移动路径点阵、终点自动记忆或旧历史被身份清除。");
	case 4: return LOCTEXT("Room4Guidance", "04 永不记忆（负对照）\n测试目标：视野内正常显示，离开视野后直接消失。\n操作步骤：观察唯一对象，然后转开视野。\n正确结果：无论静止或移动，历史、灰色代理和 cap 始终为 0。\n异常结果：离开视野后留下任何灰影。");
	case 5: return LOCTEXT("Room5Guidance", "05 遮挡与快速扫视\n测试目标：墙后不能确认，快速扫视不能穿墙建立记忆。\n操作步骤：手动甩动视野，或使用 90°、160°、重复扫视控制台。\n正确结果：只有通过观察缝隙的合法部分被记录。\n异常结果：墙后整体确认或扫视路径穿墙留影。");
	case 6: return FText::Format(LOCTEXT("Room6Guidance", "06 性能压力测试\n警告：普通功能测试时保持关闭。\n当前档位：{0}\n可选：1 个对象 / 8 个对象 / 32 个对象 / 64 条同区域历史 / 64 条同 StableID 多姿态历史 / 184 条分布历史 / 六种策略混合。\n操作步骤：循环选择档位，再手动或自动连续转头。\n正确结果：关闭时接近 0 work；历史增加后候选只覆盖相关区域。\n异常结果：冷房间历史持续扫描、重复 rebuild/upload 或明显长停顿。"), StressModeName(StressMode));
	default: return LOCTEXT("LobbyGuidance", "每个房间只验证一种规则，请选择测试项目。\n六个中文入口分别对应：整体显示、局部切块、移动物体、永不记忆、遮挡与快速扫视、性能压力测试。\n大厅不显示底层长日志；进入房间后面板会显示目标、步骤、正确结果和异常结果。");
	}
}

FString ADarkwellSightWeaveGrayPolicyLabDirector::GetChineseGuidanceForTesting() const
{
	FString Result;
	for (int32 Room = 0; Room <= 6; ++Room) Result += BuildRoomGuidance(Room).ToString();
	return Result;
}

FString ADarkwellSightWeaveGrayPolicyLabDirector::GetRuntimeStatusForTesting() const
{
	return CachedStatistics;
}

int32 ADarkwellSightWeaveGrayPolicyLabDirector::GetRoomControlCountForTesting() const
{
	return Controls.Num();
}

FVector ADarkwellSightWeaveGrayPolicyLabDirector::GetRoomCenterForTesting(const int32 Room)
{
	return RoomCenters[FMath::Clamp(Room, 0, 6)];
}

TArray<FName> ADarkwellSightWeaveGrayPolicyLabDirector::GetStableIdsForRoomForTesting(const int32 Room)
{
	switch (Room)
	{
	case 1: return {TEXT("Lab.V2.Whole")};
	case 2: return {TEXT("Lab.V2.Partial")};
	case 3: return {TEXT("Lab.V2.MoveWhole"), TEXT("Lab.V2.MovePartial")};
	case 4: return {TEXT("Lab.V2.Never")};
	case 5: return {TEXT("Lab.V2.OcclusionWhole"), TEXT("Lab.V2.OcclusionPartial")};
	case 6: return {TEXT("Lab.V2.Stress")};
	default: return {};
	}
}

FString ADarkwellSightWeaveGrayPolicyLabDirector::ResolveCheckoutSha() const
{
	FString Head;
	const FString GitDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT(".git"));
	if (!FFileHelper::LoadFileToString(Head, *(GitDir / TEXT("HEAD")))) return TEXT("UNKNOWN");
	Head.TrimStartAndEndInline();
	if (!Head.StartsWith(TEXT("ref: "))) return Head.Left(12);
	FString Value;
	if (FFileHelper::LoadFileToString(Value, *(GitDir / Head.Mid(5))))
	{
		Value.TrimStartAndEndInline();
		return Value.Left(12);
	}
	return TEXT("UNKNOWN");
}

void ADarkwellSightWeaveGrayPolicyLabDirector::UpdateFrameStatistics(const float DeltaSeconds)
{
	FrameTimesMs.Add(DeltaSeconds * 1000.0f);
	if (FrameTimesMs.Num() > 600) FrameTimesMs.RemoveAt(0, FrameTimesMs.Num() - 600, EAllowShrinking::No);
	StatisticsAccumulator += DeltaSeconds;
	if (StatisticsAccumulator < .25f || FrameTimesMs.IsEmpty()) return;
	StatisticsAccumulator = 0;
	TArray<float, TInlineAllocator<600>> Sorted;
	Sorted.Append(FrameTimesMs);
	Sorted.Sort();
	auto Percentile = [&Sorted](const float P)
	{
		return Sorted[FMath::Clamp(FMath::RoundToInt((Sorted.Num() - 1) * P), 0, Sorted.Num() - 1)];
	};
	const auto T = RuntimeRoom->GetHistoryRuntimeFrameTelemetryForTesting();
	CachedStatistics = FString::Printf(TEXT("对象 %d | 记录 %d | 候选 %d | 唤醒 %d | 休眠 %d\nCoverage %llu | Occupancy %llu | Ownership %llu | Samples %llu\nCap rebuild %llu | Texture upload %llu | Backlog 0\n帧 %.2f ms | p50 %.2f | p95 %.2f | p99 %.2f | peak %.2f"),
		RuntimeRoom->GetTrackedIdentityCount(), T.SpatialRecordCount,
		T.CandidateHistoricalEpochs, T.ActiveHistoricalEpochs, T.SleepingHistoricalEpochs,
		T.CoverageQueries, T.OccupancyTests, T.OwnershipTests, T.FineSamplesScanned,
		T.CapMeshRebuilds, T.TextureUploads, DeltaSeconds * 1000.0f,
		Percentile(.50f), Percentile(.95f), Percentile(.99f), Sorted.Last());
}

void ADarkwellSightWeaveGrayPolicyLabDirector::EmitHitchIfNeeded(const float DeltaSeconds)
{
	const float Ms = DeltaSeconds * 1000.0f;
	if (Ms < 33.0f || !RuntimeRoom.IsValid()) return;
	const auto T = RuntimeRoom->GetHistoryRuntimeFrameTelemetryForTesting();
	UE_LOG(LogTemp, Warning, TEXT("GRAY_LAB_HITCH threshold=%s frame=%llu frame_ms=%.3f room=%d stress=%d histories=%d candidates=%d active=%d sleeping=%d dirty_tiles=%d coverage=%llu occupancy=%llu ownership=%llu samples=%llu cap_rebuild=%llu texture_upload=%llu room_us=%.3f history_us=%.3f diagnostics_us=%.3f"),
		Ms >= 100 ? TEXT("100") : Ms >= 50 ? TEXT("50") : TEXT("33"), T.FrameNumber, Ms,
		CurrentRoom, StressMode, T.ActiveHistoricalEpochs, T.CandidateHistoricalEpochs,
		T.ActiveHistoricalEpochs, T.SleepingHistoricalEpochs, T.DirtyTileCount,
		T.CoverageQueries, T.OccupancyTests, T.OwnershipTests, T.FineSamplesScanned,
		T.CapMeshRebuilds, T.TextureUploads, T.MovingPropLabGameThreadUs,
		T.AdvanceFineHistoryUs, T.RefreshContributionDiagnosticsUs);
}

void ADarkwellSightWeaveGrayPolicyLabDirector::RefreshSpatialPresentation()
{
	if (!RawCoverage) return;
	for (TActorIterator<ADarkwellPropLabFurniture> It(GetWorld()); It; ++It)
	{
		if (It->bSpatialHistoryManaged) It->BindPresentation(RawCoverage, RawCoverage, FogMin, FogInv, 0);
	}
}

#undef LOCTEXT_NAMESPACE
