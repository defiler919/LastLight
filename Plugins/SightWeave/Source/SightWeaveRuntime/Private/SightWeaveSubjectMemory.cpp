#include "SightWeaveSubjectMemory.h"

#include "Algo/Sort.h"

namespace SightWeaveSubjectMemoryPrivate
{
	bool AreStablePathsValid(TConstArrayView<FSoftObjectPath> Paths)
	{
		return !Paths.ContainsByPredicate(
			[](const FSoftObjectPath& Path) { return !Path.IsValid(); });
	}

	bool HasAllBasicValidity(const ESightWeaveSubjectSnapshotValidity Value)
	{
		const uint32 Required = static_cast<uint32>(
			SightWeave::SubjectMemory::RequiredBasicSnapshotValidity);
		return (static_cast<uint32>(Value) & Required) == Required;
	}

	bool BoundsIntersectRegion(const FBox& Bounds, const FSightWeaveMemoryRegion& Region)
	{
		if (!Bounds.IsValid || !Region.IsValid()
			|| Bounds.Max.Z < Region.HeightRange.ZMin
			|| Bounds.Min.Z > Region.HeightRange.ZMax)
		{
			return false;
		}
		const FVector2D Minimum(Bounds.Min.X, Bounds.Min.Y);
		const FVector2D Maximum(Bounds.Max.X, Bounds.Max.Y);
		const FVector2D Center = (Minimum + Maximum) * 0.5;
		const FVector Samples[] =
		{
			FVector(Minimum.X, Minimum.Y, Bounds.GetCenter().Z),
			FVector(Maximum.X, Minimum.Y, Bounds.GetCenter().Z),
			FVector(Maximum.X, Maximum.Y, Bounds.GetCenter().Z),
			FVector(Minimum.X, Maximum.Y, Bounds.GetCenter().Z),
			FVector(Center, Bounds.GetCenter().Z)
		};
		for (const FVector Sample : Samples)
		{
			if (Region.ContainsWorldLocation(Sample))
			{
				return true;
			}
		}
		if (Region.Shape == ESightWeaveMemoryRegionShape::Circle)
		{
			const double ClosestX = FMath::Clamp(Region.Center.X, Minimum.X, Maximum.X);
			const double ClosestY = FMath::Clamp(Region.Center.Y, Minimum.Y, Maximum.Y);
			return FVector2D::DistSquared(
				Region.Center,
				FVector2D(ClosestX, ClosestY))
				<= FMath::Square(static_cast<double>(Region.Radius));
		}
		return false;
	}

	ESightWeaveSubjectSnapshotValidity BuildValidity(
		const FSightWeaveSubjectRegistration& Registration,
		const FSightWeaveBasicStaticMeshSnapshotCandidate& Candidate,
		const uint64 TransitionIdentity)
	{
		ESightWeaveSubjectSnapshotValidity Result = ESightWeaveSubjectSnapshotValidity::None;
		if (Registration.Identity.IsValid())
		{
			Result |= ESightWeaveSubjectSnapshotValidity::Identity;
		}
		if (Registration.Scope.IsValid())
		{
			Result |= ESightWeaveSubjectSnapshotValidity::Scope;
		}
		if (!Candidate.WorldTransform.ContainsNaN())
		{
			Result |= ESightWeaveSubjectSnapshotValidity::Transform;
		}
		if (Candidate.WorldBounds.IsValid)
		{
			Result |= ESightWeaveSubjectSnapshotValidity::Bounds;
		}
		if (Candidate.bOpaqueStaticMesh
			&& Candidate.StaticMeshAsset.IsValid()
			&& !Candidate.bHasUnsupportedComponents)
		{
			Result |= ESightWeaveSubjectSnapshotValidity::OpaqueStaticMesh;
		}
		if (!Candidate.bHasDynamicMaterial
			&& AreStablePathsValid(Candidate.MaterialOverrides))
		{
			Result |= ESightWeaveSubjectSnapshotValidity::StableMaterials;
		}
		if (TransitionIdentity != 0)
		{
			Result |= ESightWeaveSubjectSnapshotValidity::Transition;
		}
		return Result;
	}
}

bool FSightWeaveBasicStaticMeshSnapshotCandidate::IsValid() const
{
	return !WorldTransform.ContainsNaN()
		&& WorldBounds.IsValid
		&& StaticMeshAsset.IsValid()
		&& SightWeaveSubjectMemoryPrivate::AreStablePathsValid(MaterialOverrides)
		&& bOpaqueStaticMesh
		&& !bHasDynamicMaterial
		&& !bHasUnsupportedComponents;
}

bool FSightWeaveBasicStaticMeshSnapshotCandidate::IsEquivalentTo(
	const FSightWeaveBasicStaticMeshSnapshotCandidate& Other) const
{
	return WorldTransform.Equals(Other.WorldTransform)
		&& WorldBounds.Min == Other.WorldBounds.Min
		&& WorldBounds.Max == Other.WorldBounds.Max
		&& WorldBounds.IsValid == Other.WorldBounds.IsValid
		&& StaticMeshAsset == Other.StaticMeshAsset
		&& MaterialOverrides == Other.MaterialOverrides
		&& VisualVariantId == Other.VisualVariantId
		&& bOpaqueStaticMesh == Other.bOpaqueStaticMesh
		&& bHasDynamicMaterial == Other.bHasDynamicMaterial
		&& bHasUnsupportedComponents == Other.bHasUnsupportedComponents;
}

bool FSightWeaveSubjectRegistration::IsValid() const
{
	if (!Identity.IsValid() || !Scope.IsValid())
	{
		return false;
	}
	return Policy != ESightWeaveSubjectMemoryPolicy::Custom
		|| (!CustomProviderName.IsNone() && CustomProviderVersion != 0);
}

bool FSightWeaveSubjectRegistration::IsEquivalentTo(
	const FSightWeaveSubjectRegistration& Other) const
{
	return Identity.IsEquivalentTo(Other.Identity)
		&& Scope.IsEquivalentTo(Other.Scope)
		&& Policy == Other.Policy
		&& CustomProviderName == Other.CustomProviderName
		&& CustomProviderVersion == Other.CustomProviderVersion;
}

bool FSightWeaveLastSeenSnapshotDescriptor::IsValid() const
{
	return Identity.IsValid()
		&& Scope.IsValid()
		&& (Policy == ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot
			|| Policy == ESightWeaveSubjectMemoryPolicy::Custom)
		&& SnapshotRevision != 0
		&& EligibilityRevision != 0
		&& SourceLiveRevision != 0
		&& !WorldTransform.ContainsNaN()
		&& WorldBounds.IsValid
		&& StaticMeshAsset.IsValid()
		&& SightWeaveSubjectMemoryPrivate::AreStablePathsValid(MaterialOverrides)
		&& CaptureReason == ESightWeaveSubjectCaptureReason::LiveToNonLive
		&& CaptureTransitionIdentity != 0
		&& SightWeaveSubjectMemoryPrivate::HasAllBasicValidity(Validity);
}

bool FSightWeaveLastSeenSnapshotDescriptor::IsEquivalentTo(
	const FSightWeaveLastSeenSnapshotDescriptor& Other) const
{
	return Identity.IsEquivalentTo(Other.Identity)
		&& Scope.IsEquivalentTo(Other.Scope)
		&& Policy == Other.Policy
		&& SnapshotRevision == Other.SnapshotRevision
		&& EligibilityRevision == Other.EligibilityRevision
		&& SourceLiveRevision == Other.SourceLiveRevision
		&& WorldTransform.Equals(Other.WorldTransform)
		&& WorldBounds.Min == Other.WorldBounds.Min
		&& WorldBounds.Max == Other.WorldBounds.Max
		&& WorldBounds.IsValid == Other.WorldBounds.IsValid
		&& StaticMeshAsset == Other.StaticMeshAsset
		&& MaterialOverrides == Other.MaterialOverrides
		&& VisualVariantId == Other.VisualVariantId
		&& CaptureReason == Other.CaptureReason
		&& CaptureTransitionIdentity == Other.CaptureTransitionIdentity
		&& Validity == Other.Validity;
}

bool FSightWeaveSubjectObservation::HasValidHeader() const
{
	return Identity.IsValid()
		&& Scope.IsValid()
		&& ObservationRevision != 0
		&& EligibilityRevision != 0
		&& SourceLiveRevision != 0;
}

FSightWeaveSubjectHandle FSightWeaveSubjectMemoryAuthority::Register(
	const FSightWeaveSubjectRegistration& Registration)
{
	check(IsInGameThread());
	if (!Registration.IsValid()
		|| Records.ContainsByPredicate(
			[&Registration](const FRecord& Record)
			{
				return Record.Registration.Identity.IsEquivalentTo(Registration.Identity);
			}))
	{
		return FSightWeaveSubjectHandle();
	}
	FRecord& Added = Records.AddDefaulted_GetRef();
	Added.Handle = FSightWeaveSubjectHandle(NextHandle++);
	Added.Registration = Registration;
	Records.Sort([](const FRecord& A, const FRecord& B)
	{
		return A.Handle.GetValue() < B.Handle.GetValue();
	});
	return Added.Handle;
}

bool FSightWeaveSubjectMemoryAuthority::Update(
	const FSightWeaveSubjectHandle Handle,
	const FSightWeaveSubjectRegistration& Registration)
{
	check(IsInGameThread());
	FRecord* Record = FindRecord(Handle);
	if (!Record || !Registration.IsValid())
	{
		return false;
	}
	if (Record->Registration.IsEquivalentTo(Registration))
	{
		return true;
	}
	if (Records.ContainsByPredicate(
		[Handle, &Registration](const FRecord& Other)
		{
			return Other.Handle != Handle
				&& Other.Registration.Identity.IsEquivalentTo(Registration.Identity);
		}))
	{
		return false;
	}
	const FSightWeaveSubjectHandle StableHandle = Record->Handle;
	*Record = FRecord();
	Record->Handle = StableHandle;
	Record->Registration = Registration;
	return true;
}

bool FSightWeaveSubjectMemoryAuthority::Unregister(const FSightWeaveSubjectHandle Handle)
{
	check(IsInGameThread());
	return Records.RemoveAll(
		[Handle](const FRecord& Record) { return Record.Handle == Handle; }) == 1;
}

bool FSightWeaveSubjectMemoryAuthority::IsHandleValid(
	const FSightWeaveSubjectHandle Handle) const
{
	return FindRecord(Handle) != nullptr;
}

void FSightWeaveSubjectMemoryAuthority::Reset()
{
	check(IsInGameThread());
	Records.Reset();
	NextHandle = 1;
}

FSightWeaveSubjectTransitionResult FSightWeaveSubjectMemoryAuthority::SubmitObservation(
	const FSightWeaveSubjectHandle Handle,
	const FSightWeaveSubjectObservation& Observation,
	const ISightWeaveSubjectSnapshotProvider* CustomProvider)
{
	check(IsInGameThread());
	FSightWeaveSubjectTransitionResult Result;
	FRecord* Record = FindRecord(Handle);
	if (!Record)
	{
		Result.Failure = ESightWeaveSubjectTransitionFailure::InvalidHandle;
		return Result;
	}
	Result.PriorSnapshotRevision = Record->Snapshot.IsSet()
		? Record->Snapshot->SnapshotRevision
		: 0;
	Result.SnapshotRevision = Result.PriorSnapshotRevision;
	if (!Record->Registration.IsValid())
	{
		Result.Failure = ESightWeaveSubjectTransitionFailure::InvalidRegistration;
		return Result;
	}
	if (!Observation.HasValidHeader())
	{
		Result.Failure = ESightWeaveSubjectTransitionFailure::InvalidObservation;
		return Result;
	}
	if (!Observation.Identity.IsEquivalentTo(Record->Registration.Identity))
	{
		Result.Failure = ESightWeaveSubjectTransitionFailure::IdentityMismatch;
		return Result;
	}
	if (!Observation.Scope.IsEquivalentTo(Record->Registration.Scope))
	{
		Result.Failure = ESightWeaveSubjectTransitionFailure::ScopeMismatch;
		return Result;
	}
	if (Observation.ObservationRevision <= Record->LastObservationRevision)
	{
		Result.Failure = ESightWeaveSubjectTransitionFailure::StaleObservation;
		return Result;
	}
	if (Observation.SourceLiveRevision < Record->LastLiveSourceRevision)
	{
		Result.Failure = ESightWeaveSubjectTransitionFailure::StaleSourceLiveRevision;
		return Result;
	}

	const bool bFallingEdge = Record->bHasObservation
		&& Record->bWasHardLive
		&& !Observation.bHardLive;
	Record->LastObservationRevision = Observation.ObservationRevision;
	Record->bHasObservation = true;
	if (Observation.bHardLive)
	{
		Record->bWasHardLive = true;
		Record->LastLiveEligibilityRevision = Observation.EligibilityRevision;
		Record->LastLiveSourceRevision = Observation.SourceLiveRevision;
		Record->bLastLiveEligibleForMemoryWrite = Observation.bEligibleForMemoryWrite;
		Record->LastLiveCandidate = Observation.BasicSnapshot;
		Record->bHasValidLastLiveCandidate = Observation.BasicSnapshot.IsValid();
		Result.Disposition = ESightWeaveSubjectTransitionDisposition::LiveAccepted;
		Result.Failure = ESightWeaveSubjectTransitionFailure::None;
		Result.bProxyMustHide = true;
		return Result;
	}

	Record->bWasHardLive = false;
	Result.Disposition = ESightWeaveSubjectTransitionDisposition::NonLiveAccepted;
	Result.Failure = ESightWeaveSubjectTransitionFailure::None;
	if (!bFallingEdge)
	{
		return Result;
	}

	switch (Record->Registration.Policy)
	{
	case ESightWeaveSubjectMemoryPolicy::NeverRemember:
	case ESightWeaveSubjectMemoryPolicy::VisibleOnly:
	case ESightWeaveSubjectMemoryPolicy::StaticEnvironment:
		return Result;
	case ESightWeaveSubjectMemoryPolicy::Custom:
		break;
	case ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot:
		break;
	default:
		Result.Failure = ESightWeaveSubjectTransitionFailure::InvalidRegistration;
		Result.Disposition = ESightWeaveSubjectTransitionDisposition::Rejected;
		return Result;
	}

	if (Observation.TransitionIdentity == 0)
	{
		Result.Failure = ESightWeaveSubjectTransitionFailure::InvalidTransition;
		Result.Disposition = ESightWeaveSubjectTransitionDisposition::Rejected;
		return Result;
	}
	if (Observation.TransitionIdentity <= Record->LastConsumedTransitionIdentity)
	{
		Result.Failure = ESightWeaveSubjectTransitionFailure::DuplicateTransition;
		Result.Disposition = ESightWeaveSubjectTransitionDisposition::Rejected;
		return Result;
	}
	if (!Record->bLastLiveEligibleForMemoryWrite)
	{
		Result.Failure = ESightWeaveSubjectTransitionFailure::NotMemoryEligible;
		Result.Disposition = ESightWeaveSubjectTransitionDisposition::Rejected;
		return Result;
	}
	FSightWeaveBasicStaticMeshSnapshotCandidate SnapshotCandidate = Record->LastLiveCandidate;
	if (Record->Registration.Policy == ESightWeaveSubjectMemoryPolicy::Custom)
	{
		if (!CustomProvider)
		{
			Result.Failure = ESightWeaveSubjectTransitionFailure::MissingCustomProvider;
			Result.Disposition = ESightWeaveSubjectTransitionDisposition::Rejected;
			return Result;
		}
		if (CustomProvider->GetSightWeaveProviderName() != Record->Registration.CustomProviderName
			|| CustomProvider->GetSightWeaveProviderVersion() != Record->Registration.CustomProviderVersion)
		{
			Result.Failure = ESightWeaveSubjectTransitionFailure::CustomProviderMismatch;
			Result.Disposition = ESightWeaveSubjectTransitionDisposition::Rejected;
			return Result;
		}
		SnapshotCandidate = FSightWeaveBasicStaticMeshSnapshotCandidate();
		if (!CustomProvider->BuildSightWeaveSnapshotCandidate(
			Record->Registration,
			Observation,
			SnapshotCandidate))
		{
			Result.Failure = ESightWeaveSubjectTransitionFailure::CustomProviderRejected;
			Result.Disposition = ESightWeaveSubjectTransitionDisposition::Rejected;
			return Result;
		}
		if (!SnapshotCandidate.IsValid())
		{
			Result.Failure = ESightWeaveSubjectTransitionFailure::InvalidCustomProviderResult;
			Result.Disposition = ESightWeaveSubjectTransitionDisposition::Rejected;
			return Result;
		}
	}
	else if (!Record->bHasValidLastLiveCandidate)
	{
		Result.Failure = ESightWeaveSubjectTransitionFailure::UnsupportedSubject;
		Result.Disposition = ESightWeaveSubjectTransitionDisposition::Rejected;
		return Result;
	}

	FSightWeaveLastSeenSnapshotDescriptor Snapshot;
	Snapshot.Identity = Record->Registration.Identity;
	Snapshot.Scope = Record->Registration.Scope;
	Snapshot.Policy = Record->Registration.Policy;
	Snapshot.SnapshotRevision = Record->NextSnapshotRevision++;
	Snapshot.EligibilityRevision = Record->LastLiveEligibilityRevision;
	Snapshot.SourceLiveRevision = Record->LastLiveSourceRevision;
	Snapshot.WorldTransform = SnapshotCandidate.WorldTransform;
	Snapshot.WorldBounds = SnapshotCandidate.WorldBounds;
	Snapshot.StaticMeshAsset = SnapshotCandidate.StaticMeshAsset;
	Snapshot.MaterialOverrides = SnapshotCandidate.MaterialOverrides;
	Snapshot.VisualVariantId = SnapshotCandidate.VisualVariantId;
	Snapshot.CaptureReason = ESightWeaveSubjectCaptureReason::LiveToNonLive;
	Snapshot.CaptureTransitionIdentity = Observation.TransitionIdentity;
	Snapshot.Validity = SightWeaveSubjectMemoryPrivate::BuildValidity(
		Record->Registration,
		SnapshotCandidate,
		Observation.TransitionIdentity);
	if (!Snapshot.IsValid())
	{
		Result.Failure = ESightWeaveSubjectTransitionFailure::UnsupportedSubject;
		Result.Disposition = ESightWeaveSubjectTransitionDisposition::Rejected;
		return Result;
	}
	Record->LastConsumedTransitionIdentity = Observation.TransitionIdentity;
	Record->Snapshot = MoveTemp(Snapshot);
	Result.Failure = ESightWeaveSubjectTransitionFailure::None;
	Result.Disposition = ESightWeaveSubjectTransitionDisposition::SnapshotCaptured;
	Result.SnapshotRevision = Record->Snapshot->SnapshotRevision;
	return Result;
}

FSightWeaveSubjectPresentationResult FSightWeaveSubjectMemoryAuthority::EvaluatePresentation(
	const FSightWeaveSubjectHandle Handle,
	const FSightWeaveSubjectPresentationContext& Context) const
{
	FSightWeaveSubjectPresentationResult Result;
	const FRecord* Record = FindRecord(Handle);
	if (!Record)
	{
		Result.Failure = ESightWeaveSubjectPresentationFailure::InvalidHandle;
		return Result;
	}
	if (!Context.Identity.IsEquivalentTo(Record->Registration.Identity))
	{
		Result.Failure = ESightWeaveSubjectPresentationFailure::IdentityMismatch;
		return Result;
	}
	if (!Context.Scope.IsEquivalentTo(Record->Registration.Scope))
	{
		Result.Failure = ESightWeaveSubjectPresentationFailure::ScopeMismatch;
		return Result;
	}
	if (Record->Registration.Policy == ESightWeaveSubjectMemoryPolicy::StaticEnvironment)
	{
		Result.State = ESightWeaveSubjectPresentationState::StaticEnvironmentDelegated;
		return Result;
	}
	if (Context.bHardLive)
	{
		Result.State = ESightWeaveSubjectPresentationState::Live;
		return Result;
	}
	if (Record->Registration.Policy == ESightWeaveSubjectMemoryPolicy::NeverRemember
		|| Record->Registration.Policy == ESightWeaveSubjectMemoryPolicy::VisibleOnly)
	{
		return Result;
	}
	if (Record->Registration.Policy != ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot
		&& Record->Registration.Policy != ESightWeaveSubjectMemoryPolicy::Custom)
	{
		Result.Failure = ESightWeaveSubjectPresentationFailure::UnsupportedPolicy;
		return Result;
	}
	if (!Record->Snapshot.IsSet())
	{
		Result.Failure = ESightWeaveSubjectPresentationFailure::MissingSnapshot;
		return Result;
	}
	const FSightWeaveLastSeenSnapshotDescriptor& Snapshot = Record->Snapshot.GetValue();
	Result.SnapshotRevision = Snapshot.SnapshotRevision;
	if (!Snapshot.IsValid()
		|| !DoesSnapshotMatchRegistration(Snapshot, Record->Registration))
	{
		Result.Failure = ESightWeaveSubjectPresentationFailure::InvalidSnapshot;
		return Result;
	}
	if (Context.SnapshotRevision != Snapshot.SnapshotRevision)
	{
		Result.Failure = ESightWeaveSubjectPresentationFailure::SnapshotRevisionMismatch;
		return Result;
	}
	if (Context.EligibilityRevision != Snapshot.EligibilityRevision)
	{
		Result.Failure = ESightWeaveSubjectPresentationFailure::EligibilityRevisionMismatch;
		return Result;
	}
	if (Context.SourceLiveRevision != Snapshot.SourceLiveRevision)
	{
		Result.Failure = ESightWeaveSubjectPresentationFailure::SourceLiveRevisionMismatch;
		return Result;
	}
	if (!Context.bHardMemoryAtSnapshot)
	{
		Result.Failure = ESightWeaveSubjectPresentationFailure::UnknownMemory;
		return Result;
	}
	if (Context.bBlockMemoryWrites || Context.bSuppressMemoryPresentation)
	{
		Result.Failure = ESightWeaveSubjectPresentationFailure::PresentationSuppressed;
		return Result;
	}
	Result.State = ESightWeaveSubjectPresentationState::LastSeenProxy;
	return Result;
}

int32 FSightWeaveSubjectMemoryAuthority::ClearSnapshots(const FSightWeaveMemoryRegion& Region)
{
	check(IsInGameThread());
	if (!Region.IsValid() || !Region.bEnabled)
	{
		return 0;
	}
	int32 RemovedCount = 0;
	for (FRecord& Record : Records)
	{
		if (Record.Snapshot.IsSet()
			&& Record.Snapshot->Scope.IsEquivalentTo(Region.Scope)
			&& SightWeaveSubjectMemoryPrivate::BoundsIntersectRegion(
				Record.Snapshot->WorldBounds,
				Region))
		{
			Record.Snapshot.Reset();
			++RemovedCount;
		}
	}
	return RemovedCount;
}

const FSightWeaveLastSeenSnapshotDescriptor* FSightWeaveSubjectMemoryAuthority::FindSnapshot(
	const FSightWeaveSubjectHandle Handle) const
{
	const FRecord* Record = FindRecord(Handle);
	return Record && Record->Snapshot.IsSet() ? &Record->Snapshot.GetValue() : nullptr;
}

int32 FSightWeaveSubjectMemoryAuthority::GetSnapshotCount() const
{
	int32 Count = 0;
	for (const FRecord& Record : Records)
	{
		Count += Record.Snapshot.IsSet() ? 1 : 0;
	}
	return Count;
}

bool FSightWeaveSubjectMemoryAuthority::DoesSnapshotMatchRegistration(
	const FSightWeaveLastSeenSnapshotDescriptor& Snapshot,
	const FSightWeaveSubjectRegistration& Registration)
{
	return Snapshot.IsValid()
		&& Registration.IsValid()
		&& Snapshot.Identity.IsEquivalentTo(Registration.Identity)
		&& Snapshot.Scope.IsEquivalentTo(Registration.Scope)
		&& Snapshot.Policy == Registration.Policy;
}

FSightWeaveSubjectMemoryAuthority::FRecord* FSightWeaveSubjectMemoryAuthority::FindRecord(
	const FSightWeaveSubjectHandle Handle)
{
	return Handle.IsValid()
		? Records.FindByPredicate(
			[Handle](const FRecord& Record) { return Record.Handle == Handle; })
		: nullptr;
}

const FSightWeaveSubjectMemoryAuthority::FRecord*
FSightWeaveSubjectMemoryAuthority::FindRecord(const FSightWeaveSubjectHandle Handle) const
{
	return Handle.IsValid()
		? Records.FindByPredicate(
			[Handle](const FRecord& Record) { return Record.Handle == Handle; })
		: nullptr;
}
