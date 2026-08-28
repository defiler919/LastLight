#include "SightWeaveComponents.h"

#include "Engine/World.h"
#include "SightWeaveSettings.h"
#include "SightWeaveWorldSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogSightWeaveDebugQuery, Log, All);

namespace
{
	USightWeaveWorldSubsystem* GetSightWeaveSubsystem(const UActorComponent* Component)
	{
		const UWorld* World = Component ? Component->GetWorld() : nullptr;
		return World ? World->GetSubsystem<USightWeaveWorldSubsystem>() : nullptr;
	}
}

USightWeaveFloorComponent::USightWeaveFloorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsOnUpdateTransform = true;
	Definition.FloorId = FSightWeaveFloorId(FName(TEXT("Default")));
}

void USightWeaveFloorComponent::OnRegister()
{
	Super::OnRegister();
	RefreshFloorRegistration();
}

void USightWeaveFloorComponent::OnUnregister()
{
	if (USightWeaveWorldSubsystem* Subsystem = GetSightWeaveSubsystem(this); Subsystem && bRegistered)
	{
		Subsystem->UnregisterFloor(RegisteredFloorId);
	}
	bRegistered = false;
	RegisteredFloorId = FSightWeaveFloorId();
	Super::OnUnregister();
}

void USightWeaveFloorComponent::OnUpdateTransform(
	const EUpdateTransformFlags UpdateTransformFlags,
	const ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
	if (IsRegistered())
	{
		RefreshFloorRegistration();
	}
}

#if WITH_EDITOR
void USightWeaveFloorComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshFloorRegistration();
}
#endif

bool USightWeaveFloorComponent::RefreshFloorRegistration()
{
	USightWeaveWorldSubsystem* Subsystem = GetSightWeaveSubsystem(this);
	if (!Subsystem)
	{
		return false;
	}
	const FSightWeaveFloorDefinition WorldDefinition = BuildWorldDefinition();
	if (!WorldDefinition.IsValid())
	{
		return false;
	}
	if (bRegistered && RegisteredFloorId != WorldDefinition.FloorId)
	{
		Subsystem->UnregisterFloor(RegisteredFloorId);
		bRegistered = false;
		RegisteredFloorId = FSightWeaveFloorId();
	}
	if (bRegistered)
	{
		return Subsystem->UpdateFloor(RegisteredFloorId, WorldDefinition);
	}
	bRegistered = Subsystem->RegisterFloor(WorldDefinition, this);
	if (bRegistered)
	{
		RegisteredFloorId = WorldDefinition.FloorId;
	}
	return bRegistered;
}

FSightWeaveFloorDefinition USightWeaveFloorComponent::BuildWorldDefinition() const
{
	FSightWeaveFloorDefinition Result = Definition;
	const FTransform Transform = GetComponentTransform();
	FBox2D WorldBounds(ForceInit);
	for (const FVector2D Corner : {
		Definition.BoundsMin,
		FVector2D(Definition.BoundsMin.X, Definition.BoundsMax.Y),
		Definition.BoundsMax,
		FVector2D(Definition.BoundsMax.X, Definition.BoundsMin.Y) })
	{
		const FVector World = Transform.TransformPosition(FVector(Corner.X, Corner.Y, 0.0));
		WorldBounds += FVector2D(World.X, World.Y);
	}
	Result.BoundsMin = WorldBounds.Min;
	Result.BoundsMax = WorldBounds.Max;
	const FVector Scale = Transform.GetScale3D();
	if (Scale.Z <= 0.0 || !FMath::IsFinite(Scale.Z))
	{
		Result.HeightRange.ZMin = 1.0f;
		Result.HeightRange.ZMax = 0.0f;
		return Result;
	}
	Result.HeightRange.ZMin = Transform.GetLocation().Z + Definition.HeightRange.ZMin * Scale.Z;
	Result.HeightRange.ZMax = Transform.GetLocation().Z + Definition.HeightRange.ZMax * Scale.Z;
	return Result;
}

USightWeaveVisionSourceComponent::USightWeaveVisionSourceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsOnUpdateTransform = true;
}

void USightWeaveVisionSourceComponent::OnRegister()
{
	Super::OnRegister();
	RefreshVisionSourceRegistration();
}

void USightWeaveVisionSourceComponent::OnUnregister()
{
	if (USightWeaveWorldSubsystem* Subsystem = GetSightWeaveSubsystem(this); Subsystem && Handle.IsValid())
	{
		Subsystem->UnregisterVisionSource(Handle);
	}
	Handle = FSightWeaveVisionSourceHandle();
	Super::OnUnregister();
}

void USightWeaveVisionSourceComponent::OnUpdateTransform(
	const EUpdateTransformFlags UpdateTransformFlags,
	const ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
	if (IsRegistered())
	{
		USightWeaveWorldSubsystem* Subsystem = GetSightWeaveSubsystem(this);
		if (!Subsystem
			|| !Handle.IsValid()
			|| !Subsystem->UpdateVisionSourceTransform(Handle, GetComponentTransform()))
		{
			RefreshVisionSourceRegistration();
		}
	}
}

#if WITH_EDITOR
void USightWeaveVisionSourceComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshVisionSourceRegistration();
}
#endif

void USightWeaveVisionSourceComponent::SetVisionSourceEnabled(const bool bEnabled)
{
	Description.bActive = bEnabled;
	RefreshVisionSourceRegistration();
}

bool USightWeaveVisionSourceComponent::RefreshVisionSourceRegistration()
{
	USightWeaveWorldSubsystem* Subsystem = GetSightWeaveSubsystem(this);
	if (!Subsystem)
	{
		return false;
	}
	const FSightWeaveVisionSourceDescription WorldDescription = BuildWorldDescription();
	if (Handle.IsValid())
	{
		return Subsystem->UpdateVisionSource(Handle, WorldDescription);
	}
	Handle = Subsystem->RegisterVisionSource(WorldDescription, this);
	return Handle.IsValid();
}

FSightWeaveVisionSourceDescription USightWeaveVisionSourceComponent::BuildWorldDescription() const
{
	FSightWeaveVisionSourceDescription Result = Description;
	Result.Transform = GetComponentTransform();
	return Result;
}

USightWeaveIlluminationSourceComponent::USightWeaveIlluminationSourceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsOnUpdateTransform = true;
}

void USightWeaveIlluminationSourceComponent::OnRegister()
{
	Super::OnRegister();
	RefreshIlluminationSourceRegistration();
}

void USightWeaveIlluminationSourceComponent::OnUnregister()
{
	if (USightWeaveWorldSubsystem* Subsystem = GetSightWeaveSubsystem(this); Subsystem && Handle.IsValid())
	{
		Subsystem->UnregisterIlluminationSource(Handle);
	}
	Handle = FSightWeaveIlluminationSourceHandle();
	Super::OnUnregister();
}

void USightWeaveIlluminationSourceComponent::OnUpdateTransform(
	const EUpdateTransformFlags UpdateTransformFlags,
	const ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
	if (IsRegistered())
	{
		USightWeaveWorldSubsystem* Subsystem = GetSightWeaveSubsystem(this);
		if (!Subsystem
			|| !Handle.IsValid()
			|| !Subsystem->UpdateIlluminationSourceTransform(Handle, GetComponentTransform()))
		{
			RefreshIlluminationSourceRegistration();
		}
	}
}

#if WITH_EDITOR
void USightWeaveIlluminationSourceComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshIlluminationSourceRegistration();
}
#endif

void USightWeaveIlluminationSourceComponent::SetIlluminationSourceEnabled(const bool bEnabled)
{
	Description.bActive = bEnabled;
	RefreshIlluminationSourceRegistration();
}

bool USightWeaveIlluminationSourceComponent::RefreshIlluminationSourceRegistration()
{
	USightWeaveWorldSubsystem* Subsystem = GetSightWeaveSubsystem(this);
	if (!Subsystem)
	{
		return false;
	}
	const FSightWeaveIlluminationSourceDescription WorldDescription = BuildWorldDescription();
	if (Handle.IsValid())
	{
		return Subsystem->UpdateIlluminationSource(Handle, WorldDescription);
	}
	Handle = Subsystem->RegisterIlluminationSource(WorldDescription, this);
	return Handle.IsValid();
}

FSightWeaveIlluminationSourceDescription USightWeaveIlluminationSourceComponent::BuildWorldDescription() const
{
	FSightWeaveIlluminationSourceDescription Result = Description;
	Result.Transform = GetComponentTransform();
	return Result;
}

USightWeaveOccluderComponent::USightWeaveOccluderComponent()
	: FloorId(FName(TEXT("Default")))
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsOnUpdateTransform = true;
	LocalPoints = { FVector2D(-50.0, 0.0), FVector2D(50.0, 0.0) };
}

void USightWeaveOccluderComponent::OnRegister()
{
	Super::OnRegister();
	RefreshOccluderRegistration();
}

void USightWeaveOccluderComponent::OnUnregister()
{
	if (USightWeaveWorldSubsystem* Subsystem = GetSightWeaveSubsystem(this); Subsystem && Handle.IsValid())
	{
		Subsystem->UnregisterOccluder(Handle);
	}
	Handle = FSightWeaveOccluderHandle();
	Super::OnUnregister();
}

void USightWeaveOccluderComponent::OnUpdateTransform(
	const EUpdateTransformFlags UpdateTransformFlags,
	const ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
	if (IsRegistered())
	{
		RefreshOccluderRegistration();
	}
}

#if WITH_EDITOR
void USightWeaveOccluderComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshOccluderRegistration();
}
#endif

void USightWeaveOccluderComponent::SetOccluderEnabled(const bool bInEnabled)
{
	bEnabled = bInEnabled;
	RefreshOccluderRegistration();
}

bool USightWeaveOccluderComponent::RefreshOccluderRegistration()
{
	USightWeaveWorldSubsystem* Subsystem = GetSightWeaveSubsystem(this);
	if (!Subsystem)
	{
		return false;
	}
	TArray<FSightWeaveSegment2D> WorldSegments;
	if (!BuildWorldSegments(WorldSegments))
	{
		return false;
	}
	if (Handle.IsValid())
	{
		return Subsystem->UpdateOccluder(Handle, WorldSegments, bDynamic, bEnabled);
	}
	Handle = Subsystem->RegisterOccluder(WorldSegments, bDynamic, bEnabled, this);
	return Handle.IsValid();
}

bool USightWeaveOccluderComponent::BuildWorldSegments(TArray<FSightWeaveSegment2D>& OutSegments)
{
	OutSegments.Reset();
	LastValidationError.Reset();
	if (!FloorId.IsValid() || !LocalHeightRange.IsValid() || LocalPoints.Num() < 2)
	{
		LastValidationError = TEXT("Occluder requires a valid floor, height range, and at least two points");
		return false;
	}
	const FTransform Transform = GetComponentTransform();
	const FVector Scale = Transform.GetScale3D();
	const FRotator Rotation = Transform.Rotator();
	if (!FMath::IsFinite(Scale.X)
		|| !FMath::IsFinite(Scale.Y)
		|| !FMath::IsFinite(Scale.Z)
		|| Scale.X <= 0.0
		|| Scale.Y <= 0.0
		|| Scale.Z <= 0.0
		|| !FMath::IsNearlyEqual(Scale.X, Scale.Y, 1.0e-4)
		|| !FMath::IsNearlyZero(Rotation.Pitch, 1.0e-4)
		|| !FMath::IsNearlyZero(Rotation.Roll, 1.0e-4))
	{
		LastValidationError = TEXT("SightWeave 2.5D occluders reject negative, non-uniform XY, pitch, and roll transforms");
		return false;
	}

	TArray<FVector> WorldVertices;
	WorldVertices.Reserve(LocalPoints.Num());
	for (const FVector2D Point : LocalPoints)
	{
		if (Point.ContainsNaN())
		{
			LastValidationError = TEXT("Occluder contains a non-finite point");
			return false;
		}
		WorldVertices.Add(Transform.TransformPosition(FVector(Point.X, Point.Y, 0.0)));
	}
	FSightWeaveHeightRange WorldHeight;
	WorldHeight.ZMin = Transform.GetLocation().Z + LocalHeightRange.ZMin * Scale.Z;
	WorldHeight.ZMax = Transform.GetLocation().Z + LocalHeightRange.ZMax * Scale.Z;
	if (!WorldHeight.IsValid())
	{
		LastValidationError = TEXT("Occluder world height range is invalid");
		return false;
	}

	const int32 EdgeCount = bClosedContour ? WorldVertices.Num() : WorldVertices.Num() - 1;
	TArray<FSightWeaveSegment2D> RawSegments;
	RawSegments.Reserve(EdgeCount);
	for (int32 Index = 0; Index < EdgeCount; ++Index)
	{
		const FVector& A = WorldVertices[Index];
		const FVector& B = WorldVertices[(Index + 1) % WorldVertices.Num()];
		FSightWeaveSegment2D Segment;
		Segment.A = FVector2D(A.X, A.Y);
		Segment.B = FVector2D(B.X, B.Y);
		Segment.FloorId = FloorId;
		Segment.HeightRange = WorldHeight;
		Segment.bDynamic = bDynamic;
		Segment.SourceEdgeIndices.Add(Index);
		RawSegments.Add(MoveTemp(Segment));
	}

	FSightWeaveGeometryTolerances Tolerances = GetDefault<USightWeaveSettings>()->GeometryTolerances;
	Tolerances.Normalize();
	if (bClosedContour && WorldVertices.Num() >= 3
		&& !SightWeave::Geometry::IsSimplePolygon(WorldVertices, Tolerances))
	{
		LastValidationError = TEXT("Closed occluder contour self-intersects or degenerates");
		return false;
	}
	FSightWeaveNormalizationResult Normalized = SightWeave::Geometry::NormalizeSegments(
		RawSegments,
		Tolerances,
		bMergeSafeCollinearSegments);
	if (Normalized.RemovedInvalid > 0 || Normalized.Segments.IsEmpty())
	{
		LastValidationError = TEXT("Occluder normalization rejected all or part of invalid geometry");
		return false;
	}
	OutSegments = MoveTemp(Normalized.Segments);
	return true;
}

USightWeaveHardSuppressionComponent::USightWeaveHardSuppressionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsOnUpdateTransform = true;
	Description.FloorId = FSightWeaveFloorId(FName(TEXT("Default")));
}

void USightWeaveHardSuppressionComponent::OnRegister()
{
	Super::OnRegister();
	RefreshHardSuppressionRegistration();
}

void USightWeaveHardSuppressionComponent::OnUnregister()
{
	if (USightWeaveWorldSubsystem* Subsystem = GetSightWeaveSubsystem(this); Subsystem && Handle.IsValid())
	{
		Subsystem->UnregisterHardLiveSuppression(Handle);
	}
	Handle = FSightWeaveHardSuppressionHandle();
	Super::OnUnregister();
}

void USightWeaveHardSuppressionComponent::OnUpdateTransform(
	const EUpdateTransformFlags UpdateTransformFlags,
	const ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
	if (IsRegistered())
	{
		RefreshHardSuppressionRegistration();
	}
}

#if WITH_EDITOR
void USightWeaveHardSuppressionComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshHardSuppressionRegistration();
}
#endif

void USightWeaveHardSuppressionComponent::SetHardSuppressionEnabled(const bool bEnabled)
{
	Description.bEnabled = bEnabled;
	RefreshHardSuppressionRegistration();
}

bool USightWeaveHardSuppressionComponent::RefreshHardSuppressionRegistration()
{
	USightWeaveWorldSubsystem* Subsystem = GetSightWeaveSubsystem(this);
	FSightWeaveHardSuppressionDescription WorldDescription;
	if (!Subsystem || !BuildWorldDescription(WorldDescription))
	{
		return false;
	}
	if (Handle.IsValid())
	{
		return Subsystem->UpdateHardLiveSuppression(Handle, WorldDescription);
	}
	Handle = Subsystem->RegisterHardLiveSuppression(WorldDescription, this);
	return Handle.IsValid();
}

bool USightWeaveHardSuppressionComponent::BuildWorldDescription(
	FSightWeaveHardSuppressionDescription& OutDescription) const
{
	const FTransform Transform = GetComponentTransform();
	const FVector Scale = Transform.GetScale3D();
	if (!FMath::IsFinite(Scale.X)
		|| !FMath::IsFinite(Scale.Y)
		|| !FMath::IsFinite(Scale.Z)
		|| Scale.X <= 0.0
		|| Scale.Y <= 0.0
		|| Scale.Z <= 0.0
		|| !FMath::IsNearlyEqual(Scale.X, Scale.Y, 1.0e-4))
	{
		return false;
	}
	OutDescription = Description;
	const FVector WorldCenter = Transform.TransformPosition(FVector(Description.Center.X, Description.Center.Y, 0.0));
	OutDescription.Center = FVector2D(WorldCenter.X, WorldCenter.Y);
	OutDescription.Radius = Description.Radius * Scale.X;
	OutDescription.HeightRange.ZMin = Transform.GetLocation().Z + Description.HeightRange.ZMin * Scale.Z;
	OutDescription.HeightRange.ZMax = Transform.GetLocation().Z + Description.HeightRange.ZMax * Scale.Z;
	return OutDescription.IsValid();
}

USightWeaveStaticEnvironmentComponent::USightWeaveStaticEnvironmentComponent()
	: KnowledgeOwnerId(FName(TEXT("Local")))
	, FloorId(FName(TEXT("Default")))
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsOnUpdateTransform = true;
	LocalHeightRange = { 0.0f, 300.0f };
	LocalFootprint = {
		FVector2D(-100.0, -100.0),
		FVector2D(100.0, -100.0),
		FVector2D(100.0, 100.0),
		FVector2D(-100.0, 100.0)
	};
}

void USightWeaveStaticEnvironmentComponent::OnRegister()
{
	Super::OnRegister();
	RefreshStaticEnvironmentRegistration();
}

void USightWeaveStaticEnvironmentComponent::OnUnregister()
{
	if (USightWeaveWorldSubsystem* Subsystem = GetSightWeaveSubsystem(this);
		Subsystem && Handle.IsValid())
	{
		Subsystem->UnregisterStaticEnvironment(Handle);
	}
	Handle = FSightWeaveStaticEnvironmentHandle();
	Super::OnUnregister();
}

void USightWeaveStaticEnvironmentComponent::OnUpdateTransform(
	const EUpdateTransformFlags UpdateTransformFlags,
	const ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
	if (IsRegistered())
	{
		RefreshStaticEnvironmentRegistration();
	}
}

#if WITH_EDITOR
void USightWeaveStaticEnvironmentComponent::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshStaticEnvironmentRegistration();
}
#endif

bool USightWeaveStaticEnvironmentComponent::RefreshStaticEnvironmentRegistration()
{
	USightWeaveWorldSubsystem* Subsystem = GetSightWeaveSubsystem(this);
	FSightWeaveStaticEnvironmentDescription Description;
	if (!Subsystem || !BuildWorldDescription(Description))
	{
		return false;
	}
	if (Handle.IsValid())
	{
		return Subsystem->UpdateStaticEnvironment(Handle, Description);
	}
	Handle = Subsystem->RegisterStaticEnvironment(Description, this);
	return Handle.IsValid();
}

void USightWeaveStaticEnvironmentComponent::SetStaticEnvironmentEnabled(const bool bInEnabled)
{
	if (bEnabled == bInEnabled)
	{
		return;
	}
	bEnabled = bInEnabled;
	RefreshStaticEnvironmentRegistration();
}

bool USightWeaveStaticEnvironmentComponent::BuildWorldDescription(
	FSightWeaveStaticEnvironmentDescription& OutDescription) const
{
	OutDescription = FSightWeaveStaticEnvironmentDescription();
	const FTransform Transform = GetComponentTransform();
	const FVector Scale = Transform.GetScale3D();
	const FRotator Rotation = Transform.Rotator();
	if (Scale.X <= 0.0
		|| Scale.Y <= 0.0
		|| Scale.Z <= 0.0
		|| !FMath::IsNearlyEqual(Scale.X, Scale.Y, 1.0e-4)
		|| !FMath::IsNearlyZero(Rotation.Pitch, 1.0e-4)
		|| !FMath::IsNearlyZero(Rotation.Roll, 1.0e-4))
	{
		return false;
	}
	OutDescription.KnowledgeOwnerId = KnowledgeOwnerId;
	OutDescription.FloorId = FloorId;
	OutDescription.HeightRange.ZMin =
		Transform.GetLocation().Z + LocalHeightRange.ZMin * Scale.Z;
	OutDescription.HeightRange.ZMax =
		Transform.GetLocation().Z + LocalHeightRange.ZMax * Scale.Z;
	OutDescription.WorldFootprint.Reserve(LocalFootprint.Num());
	for (const FVector2D Local : LocalFootprint)
	{
		const FVector World = Transform.TransformPosition(FVector(Local, 0.0));
		OutDescription.WorldFootprint.Emplace(World.X, World.Y);
	}
	OutDescription.NeutralIntensity = NeutralIntensity;
	OutDescription.bExplicitlyImmutable = bExplicitlyImmutable;
	OutDescription.bEnabled = bEnabled;
	return OutDescription.IsValid();
}

USightWeaveMemoryModifierComponent::USightWeaveMemoryModifierComponent()
	: KnowledgeOwnerId(FName(TEXT("Local")))
	, FloorId(FName(TEXT("Default")))
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsOnUpdateTransform = true;
	LocalHeightRange = { 0.0f, 300.0f };
	LocalPolygonVertices = {
		FVector2D(-100.0, -100.0),
		FVector2D(100.0, -100.0),
		FVector2D(100.0, 100.0),
		FVector2D(-100.0, 100.0)
	};
}

void USightWeaveMemoryModifierComponent::OnRegister()
{
	Super::OnRegister();
	RefreshMemoryModifierRegistration();
}

void USightWeaveMemoryModifierComponent::OnUnregister()
{
	if (USightWeaveWorldSubsystem* Subsystem = GetSightWeaveSubsystem(this);
		Subsystem && Handle.IsValid())
	{
		Subsystem->UnregisterMemoryModifier(Handle);
	}
	Handle = FSightWeaveMemoryModifierHandle();
	Super::OnUnregister();
}

void USightWeaveMemoryModifierComponent::OnUpdateTransform(
	const EUpdateTransformFlags UpdateTransformFlags,
	const ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
	if (IsRegistered())
	{
		RefreshMemoryModifierRegistration();
	}
}

#if WITH_EDITOR
void USightWeaveMemoryModifierComponent::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshMemoryModifierRegistration();
}
#endif

bool USightWeaveMemoryModifierComponent::RefreshMemoryModifierRegistration()
{
	USightWeaveWorldSubsystem* Subsystem = GetSightWeaveSubsystem(this);
	FSightWeaveMemoryModifierDescription Description;
	if (!Subsystem || !BuildWorldDescription(Description))
	{
		return false;
	}
	if (Handle.IsValid())
	{
		return Subsystem->UpdateMemoryModifier(Handle, Description);
	}
	Handle = Subsystem->RegisterMemoryModifier(Description);
	return Handle.IsValid();
}

void USightWeaveMemoryModifierComponent::SetMemoryModifierEnabled(const bool bInEnabled)
{
	if (bEnabled == bInEnabled)
	{
		return;
	}
	bEnabled = bInEnabled;
	RefreshMemoryModifierRegistration();
}

bool USightWeaveMemoryModifierComponent::BuildWorldDescription(
	FSightWeaveMemoryModifierDescription& OutDescription) const
{
	OutDescription = FSightWeaveMemoryModifierDescription();
	USightWeaveWorldSubsystem* Subsystem = GetSightWeaveSubsystem(this);
	FSightWeaveMemoryScopeKey Scope;
	const FTransform Transform = GetComponentTransform();
	const FVector Scale = Transform.GetScale3D();
	const FRotator Rotation = Transform.Rotator();
	if (!Subsystem
		|| !Subsystem->GetExplorationMemoryScope(Scope)
		|| Scope.KnowledgeOwnerId != KnowledgeOwnerId
		|| Scope.FloorId != FloorId
		|| Scale.X <= 0.0 || Scale.Y <= 0.0 || Scale.Z <= 0.0
		|| !FMath::IsNearlyEqual(Scale.X, Scale.Y, 1.0e-4)
		|| !FMath::IsNearlyZero(Rotation.Pitch, 1.0e-4)
		|| !FMath::IsNearlyZero(Rotation.Roll, 1.0e-4))
	{
		return false;
	}
	OutDescription.Operation = Operation;
	OutDescription.Persistence = Persistence;
	OutDescription.StablePersistenceId = StablePersistenceId;
	FSightWeaveMemoryRegion& Region = OutDescription.Region;
	Region.Scope = MoveTemp(Scope);
	Region.HeightRange.ZMin = Transform.GetLocation().Z + LocalHeightRange.ZMin * Scale.Z;
	Region.HeightRange.ZMax = Transform.GetLocation().Z + LocalHeightRange.ZMax * Scale.Z;
	Region.Shape = Shape;
	const FVector WorldCenter = Transform.TransformPosition(FVector(LocalCenter, 0.0));
	Region.Center = FVector2D(WorldCenter.X, WorldCenter.Y);
	Region.HalfExtents = HalfExtents * Scale.X;
	Region.Radius = Radius * Scale.X;
	Region.RotationDegrees = RotationDegrees + Rotation.Yaw;
	Region.PolygonVertices.Reserve(LocalPolygonVertices.Num());
	for (const FVector2D Local : LocalPolygonVertices)
	{
		const FVector World = Transform.TransformPosition(FVector(Local, 0.0));
		Region.PolygonVertices.Emplace(World.X, World.Y);
	}
	Region.bEnabled = bEnabled;
	return OutDescription.IsValid();
}

USightWeaveDebugQueryComponent::USightWeaveDebugQueryComponent()
	: KnowledgeOwnerId(FName(TEXT("Local")))
	, FloorId(FName(TEXT("Default")))
{
	PrimaryComponentTick.bCanEverTick = false;
	DrawOptions.DurationSeconds = 10.0f;
}

void USightWeaveDebugQueryComponent::BeginPlay()
{
	Super::BeginPlay();
	LastResult = RefreshDebugQuery();
	UE_LOG(LogSightWeaveDebugQuery, Display,
		TEXT("SightWeave debug query actor=%s status=%d authoritative=%d live=%d vision=%d light=%d bypass=%d snapshot=%lld visionSources=%d illuminationSources=%d"),
		*GetNameSafe(GetOwner()),
		static_cast<int32>(LastResult.Status),
		LastResult.bAuthoritative ? 1 : 0,
		LastResult.bVisible ? 1 : 0,
		LastResult.bInVisionPolygon ? 1 : 0,
		LastResult.bHasLegalIllumination ? 1 : 0,
		LastResult.bUsedBypass ? 1 : 0,
		LastResult.SnapshotRevision.GetValue(),
		LastResult.ContributingVisionSources.Num(),
		LastResult.ContributingIlluminationSources.Num());
}

FSightWeaveVisibilityQueryResult USightWeaveDebugQueryComponent::RefreshDebugQuery()
{
	USightWeaveWorldSubsystem* Subsystem = GetSightWeaveSubsystem(this);
	if (!Subsystem)
	{
		return FSightWeaveVisibilityQueryResult();
	}
	LastResult = Subsystem->QueryEffectiveLiveAtLocation(KnowledgeOwnerId, FloorId, GetComponentLocation());
	if (bDrawAtBeginPlay)
	{
		FSightWeaveDebugQueryMarker Marker;
		Marker.WorldLocation = GetComponentLocation();
		Marker.Result = LastResult;
		Subsystem->DrawDebugSnapshot(DrawOptions, { Marker });
	}
	return LastResult;
}
