#include "VisionPresentation/DarkwellMovingPropLabRoom.h"

#include "Camera/CameraComponent.h"
#include "Combat/DarkwellLoadoutComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
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

ADarkwellMovingPropLabRoom::ADarkwellMovingPropLabRoom()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("MovingRoomRoot")));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
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
}

void ADarkwellMovingPropLabRoom::BeginPlay()
{
	Super::BeginPlay();
	const bool bActive = FindActive(GetWorld()) == this;
	SetActorHiddenInGame(!bActive);
	SetActorEnableCollision(bActive);
}

void ADarkwellMovingPropLabRoom::EndPlay(const EEndPlayReason::Type Reason)
{
	DestroyTracked();
	Super::EndPlay(Reason);
}

ADarkwellMovingPropLabRoom* ADarkwellMovingPropLabRoom::FindActive(const UWorld* World)
{
	if (!Darkwell::PropLab::IsLabWorld(World) || !World->URL.HasOption(TEXT("MoveRules")))
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
	MotionProp.Reset();
	bMotionActive = false;
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
				Prop.History.FreezeCurrentForHiddenMovement();
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
		UpdateRecordTexture(Prop, *Record);
		UpdateRecordCap(Prop, *Record);
	}

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
		if (Prop->History.GetCurrentIndex() != INDEX_NONE)
		{
			FDarkwellSpatialObservationRecord& Current =
				Prop->History.GetMutableRecords()[Prop->History.GetCurrentIndex()];
			EnsureRecordVisual(*Prop, Current);
			Prop->History.FreezeCurrentForHiddenMovement();
		}
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
	if (!Visual.Texture.IsValid())
	{
		const FIntPoint Size = Record.SpatialMemory.GetSize()
			* Darkwell::MovingPropLab::PresentationSamples;
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
		OwnedTextures.Add(Texture);
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
			BindProxyMaterial(Prop, Record, Proxy);
		}
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
	uint64 Signature = (uint64(Record.SpatialMemory.GetGeneration()) << 1 | uint64(bPresent))
		* 1099511628211ull;
	for (const FDarkwellSpatialPropMemory::FCell& Cell : Cells)
	{
		const uint64 Bits = bPresent
			? (Cell.DiscoveredPresent > 0 ? 1ull : 0ull)
			: ((Cell.InitialRemembered > 0 ? 1ull : 0ull)
				| (Cell.VerifiedEmpty > 0 ? 2ull : 0ull));
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
			: Cell.InitialRemembered > 0 && Cell.VerifiedEmpty == 0;
	};
	auto IsCut = [&](const int32 X, const int32 Y)
	{
		if (X < 0 || Y < 0 || X >= Size.X || Y >= Size.Y) return false;
		const auto& Cell = Cells[Y * Size.X + X];
		return bPresent ? Cell.DiscoveredPresent == 0
			: Cell.InitialRemembered > 0 && Cell.VerifiedEmpty > 0;
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
	Darkwell::MovingPropLab::SetMode2();
	ConfigureScenarioProps(0);
	Player->RestorePersistentState(Player->GetActorTransform(), Player->GetMaxHealth(),
		DarkwellGameplayTags::State_Player_Alive, FGameplayTag());
	Player->GetLoadoutComponent()->RestorePersistentState(
		2, 100, 0, 100, DarkwellGameplayTags::Equipment_Left_Shotgun,
		DarkwellGameplayTags::Equipment_Right_Torch);
	TeleportPlayer(Player, FVector(-1100, 300, 92), 90);
	bStarted = true;
	UE_LOG(LogDarkwellMovingPropLab, Display,
		TEXT("MOVING_RULES_RESET rule=SpatialEvidenceOnly enemy=0 timer=0 identities=%d"),
		Tracked.Num());
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
	auto StartMotion = [&](const FTransform& Target, const float Duration)
	{
		MotionProp = Actual;
		MotionStart = Actual->GetActorTransform();
		MotionEnd = Target;
		MotionSeconds = 0.0f;
		MotionDuration = Duration;
		bMotionActive = true;
	};

	if (Scenario == 1 && ScenarioPhase == 0)
	{
		// Keep the complete translation inside the legal view so it rebases the
		// current epoch instead of becoming an offscreen move.
		StartMotion(FTransform(Darkwell::MovingPropLab::A + FVector(200, 0, 0)), 4.0f);
	}
	else if (Scenario == 2 && ScenarioPhase < 2)
	{
		const float Yaw = ScenarioPhase == 0 ? 90.0f : 180.0f;
		StartMotion(FTransform(FRotator(0, Yaw, 0), Darkwell::MovingPropLab::A), 3.0f);
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
		StartMotion(FTransform(FVector(-1100, -650, 0)), 8.0f);
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

void ADarkwellMovingPropLabRoom::StopMotion()
{
	bMotionActive = false;
	MotionProp.Reset();
	MotionSeconds = 0.0f;
	MotionDuration = 0.0f;
}

void ADarkwellMovingPropLabRoom::UpdateDeterministicMotion(const float DeltaSeconds)
{
	ADarkwellPropLabFurniture* Actual = MotionProp.Get();
	if (!bMotionActive || !Actual || MotionDuration <= 0.0f)
	{
		return;
	}
	MotionSeconds = FMath::Min(MotionSeconds + DeltaSeconds, MotionDuration);
	const float Alpha = MotionSeconds / MotionDuration;
	FTransform Transform;
	Transform.Blend(MotionStart, MotionEnd, Alpha);
	Actual->SetActorTransform(Transform);
	if (Alpha >= 1.0f)
	{
		StopMotion();
	}
}

void ADarkwellMovingPropLabRoom::UpdateRoom(
	const float DeltaSeconds,
	ADarkwellCharacter* Player)
{
	if (!bStarted || !Player || FindActive(GetWorld()) != this)
	{
		return;
	}
	UpdateDeterministicMotion(DeltaSeconds);
	for (TPair<FName, FTrackedProp>& Pair : Tracked)
	{
		UpdateTracked(Pair.Value, DeltaSeconds);
	}
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
		TEXT("{\"rule\":\"SpatialEvidenceOnly\",\"scenario\":%d,\"phase\":%d,\"identities\":%d,\"records\":%d,\"current\":%d,\"historical\":%d,\"capTriangles\":%d,\"multi\":%d,\"enemy\":0}"),
		Scenario, ScenarioPhase, Tracked.Num(), Current + Historical,
		Current, Historical, Caps, MultiCount);
}

void ADarkwellMovingPropLabRoom::Report()
{
	Status = FString::Printf(
		TEXT("MOVING + MULTI PROP LAB | MODE %d | RULE SpatialEvidenceOnly | ENEMY 0\nScenario %d phase %d | identities %d | spatial records %d | multi %d | motion %s\nStableID is internal identity; only legal evidence at each old location can erase its record.\nDarkwell.PropLab moverules help"),
		Darkwell::PropLab::PresentationMode(GetWorld()), Scenario, ScenarioPhase,
		Tracked.Num(), GetTotalSpatialRecordCount(), MultiCount,
		bMotionActive ? TEXT("RUNNING") : TEXT("STOPPED"));
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
