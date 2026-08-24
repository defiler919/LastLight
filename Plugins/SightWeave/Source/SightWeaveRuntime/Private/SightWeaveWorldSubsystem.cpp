#include "SightWeaveWorldSubsystem.h"

void USightWeaveWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ResetState();
	bSightWeaveInitialized = true;
}

void USightWeaveWorldSubsystem::Deinitialize()
{
	ResetState();
	bSightWeaveInitialized = false;
	Super::Deinitialize();
}

FSightWeaveVisionSourceHandle USightWeaveWorldSubsystem::RegisterVisionSource(
	const FSightWeaveVisionSourceDescription& Description,
	UObject* Owner)
{
	if (!bSightWeaveInitialized || !Description.IsValid())
	{
		return FSightWeaveVisionSourceHandle();
	}

	FSightWeaveVisionSourceDescription NormalizedDescription = Description;
	NormalizedDescription.Compatibility.Normalize();
	const int64 NewId = NextVisionSourceId++;
	VisionSources.Add(NewId, MoveTemp(NormalizedDescription));
	if (Owner)
	{
		VisionOwners.Add(NewId, Owner);
	}
	AdvanceRevision();
	return FSightWeaveVisionSourceHandle(NewId);
}

bool USightWeaveWorldSubsystem::UpdateVisionSource(
	const FSightWeaveVisionSourceHandle Handle,
	const FSightWeaveVisionSourceDescription& Description)
{
	if (!bSightWeaveInitialized || !Description.IsValid() || !VisionSources.Contains(Handle.GetValue()))
	{
		return false;
	}

	FSightWeaveVisionSourceDescription NormalizedDescription = Description;
	NormalizedDescription.Compatibility.Normalize();
	VisionSources.FindChecked(Handle.GetValue()) = MoveTemp(NormalizedDescription);
	AdvanceRevision();
	return true;
}

bool USightWeaveWorldSubsystem::UnregisterVisionSource(const FSightWeaveVisionSourceHandle Handle)
{
	if (!bSightWeaveInitialized || VisionSources.Remove(Handle.GetValue()) == 0)
	{
		return false;
	}

	VisionOwners.Remove(Handle.GetValue());
	AdvanceRevision();
	return true;
}

bool USightWeaveWorldSubsystem::IsVisionSourceHandleValid(const FSightWeaveVisionSourceHandle Handle) const
{
	return bSightWeaveInitialized && Handle.IsValid() && VisionSources.Contains(Handle.GetValue());
}

FSightWeaveIlluminationSourceHandle USightWeaveWorldSubsystem::RegisterIlluminationSource(
	const FSightWeaveIlluminationSourceDescription& Description,
	UObject* Owner)
{
	if (!bSightWeaveInitialized || !Description.IsValid())
	{
		return FSightWeaveIlluminationSourceHandle();
	}

	FSightWeaveIlluminationSourceDescription NormalizedDescription = Description;
	NormalizedDescription.NormalizeCapabilities();
	const int64 NewId = NextIlluminationSourceId++;
	IlluminationSources.Add(NewId, MoveTemp(NormalizedDescription));
	if (Owner)
	{
		IlluminationOwners.Add(NewId, Owner);
	}
	AdvanceRevision();
	return FSightWeaveIlluminationSourceHandle(NewId);
}

bool USightWeaveWorldSubsystem::UpdateIlluminationSource(
	const FSightWeaveIlluminationSourceHandle Handle,
	const FSightWeaveIlluminationSourceDescription& Description)
{
	if (!bSightWeaveInitialized || !Description.IsValid() || !IlluminationSources.Contains(Handle.GetValue()))
	{
		return false;
	}

	FSightWeaveIlluminationSourceDescription NormalizedDescription = Description;
	NormalizedDescription.NormalizeCapabilities();
	IlluminationSources.FindChecked(Handle.GetValue()) = MoveTemp(NormalizedDescription);
	AdvanceRevision();
	return true;
}

bool USightWeaveWorldSubsystem::UnregisterIlluminationSource(const FSightWeaveIlluminationSourceHandle Handle)
{
	if (!bSightWeaveInitialized || IlluminationSources.Remove(Handle.GetValue()) == 0)
	{
		return false;
	}

	IlluminationOwners.Remove(Handle.GetValue());
	AdvanceRevision();
	return true;
}

bool USightWeaveWorldSubsystem::IsIlluminationSourceHandleValid(const FSightWeaveIlluminationSourceHandle Handle) const
{
	return bSightWeaveInitialized && Handle.IsValid() && IlluminationSources.Contains(Handle.GetValue());
}

FSightWeaveSubjectRevealHandle USightWeaveWorldSubsystem::ApplySubjectRevealOverride(
	const FSightWeaveSubjectRevealSpecification& Specification,
	UObject* Owner)
{
	if (!bSightWeaveInitialized || !Specification.IsValid())
	{
		return FSightWeaveSubjectRevealHandle();
	}

	const int64 NewId = NextSubjectRevealId++;
	SubjectReveals.Add(NewId, Specification);
	if (Owner)
	{
		SubjectRevealOwners.Add(NewId, Owner);
	}
	AdvanceRevision();
	return FSightWeaveSubjectRevealHandle(NewId);
}

bool USightWeaveWorldSubsystem::UpdateSubjectRevealOverride(
	const FSightWeaveSubjectRevealHandle Handle,
	const FSightWeaveSubjectRevealSpecification& Specification)
{
	if (!bSightWeaveInitialized || !Specification.IsValid() || !SubjectReveals.Contains(Handle.GetValue()))
	{
		return false;
	}

	SubjectReveals.FindChecked(Handle.GetValue()) = Specification;
	AdvanceRevision();
	return true;
}

bool USightWeaveWorldSubsystem::RemoveSubjectRevealOverride(const FSightWeaveSubjectRevealHandle Handle)
{
	if (!bSightWeaveInitialized || SubjectReveals.Remove(Handle.GetValue()) == 0)
	{
		return false;
	}

	SubjectRevealOwners.Remove(Handle.GetValue());
	AdvanceRevision();
	return true;
}

bool USightWeaveWorldSubsystem::IsSubjectRevealHandleValid(const FSightWeaveSubjectRevealHandle Handle) const
{
	return bSightWeaveInitialized && Handle.IsValid() && SubjectReveals.Contains(Handle.GetValue());
}

int32 USightWeaveWorldSubsystem::UnregisterAllForOwner(UObject* Owner)
{
	if (!bSightWeaveInitialized || !Owner)
	{
		return 0;
	}

	int32 RemovedCount = 0;
	auto RemoveOwned = [Owner, &RemovedCount](auto& Registrations, auto& Owners)
	{
		for (auto It = Owners.CreateIterator(); It; ++It)
		{
			if (It.Value().Get() == Owner)
			{
				Registrations.Remove(It.Key());
				It.RemoveCurrent();
				++RemovedCount;
			}
		}
	};

	RemoveOwned(VisionSources, VisionOwners);
	RemoveOwned(IlluminationSources, IlluminationOwners);
	RemoveOwned(SubjectReveals, SubjectRevealOwners);
	if (RemovedCount > 0)
	{
		AdvanceRevision();
	}
	return RemovedCount;
}

FSightWeaveVisibilityQueryResult USightWeaveWorldSubsystem::QueryVisibilityAtLocation(
	const FSightWeaveFloorId FloorId,
	const FVector WorldLocation) const
{
	if (!FloorId.IsValid())
	{
		return MakeQueryResult(ESightWeaveQueryStatus::InvalidFloor, FloorId);
	}
	if (WorldLocation.ContainsNaN())
	{
		return MakeQueryResult(ESightWeaveQueryStatus::InvalidInput, FloorId);
	}
	return MakeQueryResult(ESightWeaveQueryStatus::NotReady, FloorId);
}

FSightWeaveVisibilityQueryResult USightWeaveWorldSubsystem::QueryVisionSourceAtLocation(
	const FSightWeaveVisionSourceHandle Handle,
	const FSightWeaveFloorId FloorId,
	const FVector WorldLocation) const
{
	if (!IsVisionSourceHandleValid(Handle))
	{
		return MakeQueryResult(ESightWeaveQueryStatus::InvalidHandle, FloorId);
	}
	return QueryVisibilityAtLocation(FloorId, WorldLocation);
}

void USightWeaveWorldSubsystem::AdvanceRevision()
{
	Revision = FSightWeaveRevision(Revision.GetValue() + 1);
}

void USightWeaveWorldSubsystem::ResetState()
{
	VisionSources.Reset();
	IlluminationSources.Reset();
	SubjectReveals.Reset();
	VisionOwners.Reset();
	IlluminationOwners.Reset();
	SubjectRevealOwners.Reset();
	NextVisionSourceId = 1;
	NextIlluminationSourceId = 1;
	NextSubjectRevealId = 1;
	Revision = FSightWeaveRevision();
}

FSightWeaveVisibilityQueryResult USightWeaveWorldSubsystem::MakeQueryResult(
	const ESightWeaveQueryStatus Status,
	const FSightWeaveFloorId FloorId) const
{
	FSightWeaveVisibilityQueryResult Result;
	Result.Status = Status;
	Result.KnowledgeState = ESightWeaveKnowledgeState::Unknown;
	Result.bVisible = false;
	Result.Revision = Revision;
	Result.FloorId = FloorId;
	return Result;
}
