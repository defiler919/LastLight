#include "SightWeavePersistence.h"

#include "Hash/Blake3.h"
#include "Misc/Compression.h"

namespace SightWeavePersistencePrivate
{
	constexpr uint8 Magic[8] = { 'S', 'W', 'P', 'E', 'R', 'S', 'V', '1' };
	constexpr uint32 CompressedFlag = 1u;
	constexpr uint32 PayloadVersion = 1u;
	constexpr uint32 MaximumNameBytes = 4096u;
	constexpr uint32 MaximumPathBytes = 1024u * 1024u;

	bool IsFinite(const FVector2D& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y);
	}

	bool IsFinite(const FVector& Value)
	{
		return FMath::IsFinite(Value.X)
			&& FMath::IsFinite(Value.Y)
			&& FMath::IsFinite(Value.Z);
	}

	FString CanonicalName(const FName Name)
	{
		return Name.ToString().ToLower();
	}

	int32 CompareName(const FName A, const FName B)
	{
		return CanonicalName(A).Compare(CanonicalName(B), ESearchCase::CaseSensitive);
	}

	bool NameLess(const FName A, const FName B)
	{
		return CompareName(A, B) < 0;
	}

	class FWriter
	{
	public:
		explicit FWriter(const int64 InMaximumBytes) : MaximumBytes(InMaximumBytes) {}

		bool HasFailed() const { return bFailed; }
		const TArray<uint8>& GetBytes() const { return Bytes; }
		TArray<uint8>& GetMutableBytes() { return Bytes; }

		bool WriteBytes(const void* Data, const int64 Size)
		{
			if (bFailed || Size < 0 || Size > MAX_int32
				|| Bytes.Num() > MaximumBytes - Size)
			{
				bFailed = true;
				return false;
			}
			if (Size > 0)
			{
				const int32 Offset = Bytes.AddUninitialized(static_cast<int32>(Size));
				FMemory::Memcpy(Bytes.GetData() + Offset, Data, static_cast<SIZE_T>(Size));
			}
			return true;
		}

		bool WriteU8(const uint8 Value) { return WriteBytes(&Value, sizeof(Value)); }
		bool WriteBool(const bool Value) { return WriteU8(Value ? 1u : 0u); }
		bool WriteU16(const uint16 Value)
		{
			const uint8 Data[2] = { static_cast<uint8>(Value), static_cast<uint8>(Value >> 8) };
			return WriteBytes(Data, UE_ARRAY_COUNT(Data));
		}
		bool WriteU32(const uint32 Value)
		{
			uint8 Data[4];
			for (int32 Index = 0; Index < 4; ++Index)
			{
				Data[Index] = static_cast<uint8>(Value >> (Index * 8));
			}
			return WriteBytes(Data, UE_ARRAY_COUNT(Data));
		}
		bool WriteI32(const int32 Value) { return WriteU32(static_cast<uint32>(Value)); }
		bool WriteU64(const uint64 Value)
		{
			uint8 Data[8];
			for (int32 Index = 0; Index < 8; ++Index)
			{
				Data[Index] = static_cast<uint8>(Value >> (Index * 8));
			}
			return WriteBytes(Data, UE_ARRAY_COUNT(Data));
		}
		bool WriteFloat(const float Value)
		{
			uint32 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
			return WriteU32(Bits);
		}
		bool WriteDouble(const double Value)
		{
			uint64 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
			return WriteU64(Bits);
		}
		bool WriteString(const FString& Value, const uint32 MaximumStringBytes)
		{
			FTCHARToUTF8 Utf8(*Value);
			const int64 Length = Utf8.Length();
			return Length >= 0 && Length <= MaximumStringBytes
				&& WriteU32(static_cast<uint32>(Length))
				&& WriteBytes(Utf8.Get(), Length);
		}
		bool WriteName(const FName Value)
		{
			return WriteString(CanonicalName(Value), MaximumNameBytes);
		}

	private:
		TArray<uint8> Bytes;
		int64 MaximumBytes = 0;
		bool bFailed = false;
	};

	class FReader
	{
	public:
		explicit FReader(const TConstArrayView<uint8> InBytes) : Bytes(InBytes) {}

		int64 GetOffset() const { return Offset; }
		int64 Remaining() const { return Bytes.Num() - Offset; }
		bool HasFailed() const { return bFailed; }

		bool ReadBytes(void* OutData, const int64 Size)
		{
			if (bFailed || Size < 0 || Size > Remaining())
			{
				bFailed = true;
				return false;
			}
			if (Size > 0)
			{
				FMemory::Memcpy(OutData, Bytes.GetData() + Offset, static_cast<SIZE_T>(Size));
				Offset += Size;
			}
			return true;
		}

		bool ReadU8(uint8& OutValue) { return ReadBytes(&OutValue, sizeof(OutValue)); }
		bool ReadBool(bool& OutValue)
		{
			uint8 Value = 0;
			if (!ReadU8(Value) || Value > 1)
			{
				bFailed = true;
				return false;
			}
			OutValue = Value != 0;
			return true;
		}
		bool ReadU16(uint16& OutValue)
		{
			uint8 Data[2];
			if (!ReadBytes(Data, UE_ARRAY_COUNT(Data)))
			{
				return false;
			}
			OutValue = static_cast<uint16>(Data[0])
				| (static_cast<uint16>(Data[1]) << 8);
			return true;
		}
		bool ReadU32(uint32& OutValue)
		{
			uint8 Data[4];
			if (!ReadBytes(Data, UE_ARRAY_COUNT(Data)))
			{
				return false;
			}
			OutValue = 0;
			for (int32 Index = 0; Index < 4; ++Index)
			{
				OutValue |= static_cast<uint32>(Data[Index]) << (Index * 8);
			}
			return true;
		}
		bool ReadI32(int32& OutValue)
		{
			uint32 Value = 0;
			if (!ReadU32(Value))
			{
				return false;
			}
			OutValue = static_cast<int32>(Value);
			return true;
		}
		bool ReadU64(uint64& OutValue)
		{
			uint8 Data[8];
			if (!ReadBytes(Data, UE_ARRAY_COUNT(Data)))
			{
				return false;
			}
			OutValue = 0;
			for (int32 Index = 0; Index < 8; ++Index)
			{
				OutValue |= static_cast<uint64>(Data[Index]) << (Index * 8);
			}
			return true;
		}
		bool ReadFloat(float& OutValue)
		{
			uint32 Bits = 0;
			if (!ReadU32(Bits))
			{
				return false;
			}
			FMemory::Memcpy(&OutValue, &Bits, sizeof(Bits));
			return true;
		}
		bool ReadDouble(double& OutValue)
		{
			uint64 Bits = 0;
			if (!ReadU64(Bits))
			{
				return false;
			}
			FMemory::Memcpy(&OutValue, &Bits, sizeof(Bits));
			return true;
		}
		bool ReadString(FString& OutValue, const uint32 MaximumStringBytes)
		{
			uint32 Length = 0;
			if (!ReadU32(Length) || Length > MaximumStringBytes || Length > Remaining())
			{
				bFailed = true;
				return false;
			}
			if (Length == 0)
			{
				OutValue.Reset();
				return true;
			}
			const FUTF8ToTCHAR Converted(
				reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + Offset),
				static_cast<int32>(Length));
			OutValue = FString(Converted.Length(), Converted.Get());
			Offset += Length;
			return true;
		}
		bool ReadName(FName& OutValue)
		{
			FString Value;
			if (!ReadString(Value, MaximumNameBytes))
			{
				return false;
			}
			OutValue = FName(*Value);
			return true;
		}
		bool ReadCount(uint32& OutCount, const int64 MinimumElementBytes = 1)
		{
			if (!ReadU32(OutCount)
				|| OutCount > SightWeave::Persistence::MaximumCollectionEntries
				|| (MinimumElementBytes > 0
					&& static_cast<uint64>(OutCount)
						> static_cast<uint64>(Remaining() / MinimumElementBytes)))
			{
				bFailed = true;
				return false;
			}
			return true;
		}

	private:
		TConstArrayView<uint8> Bytes;
		int64 Offset = 0;
		bool bFailed = false;
	};

	void NormalizeProfile(FSightWeaveRenderProfileIdentity& Profile)
	{
		FSightWeaveIlluminationCompatibilityProfile Source;
		Source.AcceptedCapabilities = Profile.CanonicalCapabilities;
		Profile = FSightWeaveRenderProfileIdentity::FromProfile(Source);
	}

	FString ProfileKey(const FSightWeaveRenderProfileIdentity& Profile)
	{
		FString Result;
		for (const FName Capability : Profile.CanonicalCapabilities)
		{
			Result += CanonicalName(Capability);
			Result.AppendChar(TEXT('\x1f'));
		}
		return Result;
	}

	FString ScopeKey(const FSightWeavePersistentScopeKey& Scope)
	{
		return CanonicalName(Scope.StableScopeId)
			+ TEXT("\x1f") + CanonicalName(Scope.KnowledgeOwnerId.GetValue())
			+ TEXT("\x1f") + CanonicalName(Scope.FloorId.GetValue());
	}

	bool WriteProfile(FWriter& Writer, const FSightWeaveRenderProfileIdentity& Profile)
	{
		if (!Writer.WriteU32(static_cast<uint32>(Profile.CanonicalCapabilities.Num())))
		{
			return false;
		}
		for (const FName Capability : Profile.CanonicalCapabilities)
		{
			if (!Writer.WriteName(Capability))
			{
				return false;
			}
		}
		return true;
	}

	bool ReadProfile(FReader& Reader, FSightWeaveRenderProfileIdentity& OutProfile)
	{
		uint32 Count = 0;
		if (!Reader.ReadCount(Count, 4))
		{
			return false;
		}
		FSightWeaveIlluminationCompatibilityProfile Source;
		Source.AcceptedCapabilities.Reserve(static_cast<int32>(Count));
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			FName Capability;
			if (!Reader.ReadName(Capability) || Capability.IsNone())
			{
				return false;
			}
			Source.AcceptedCapabilities.Add(Capability);
		}
		OutProfile = FSightWeaveRenderProfileIdentity::FromProfile(Source);
		return OutProfile.IsValid();
	}

	bool WriteRegion(FWriter& Writer, const FSightWeavePersistentRegion& Region)
	{
		return Writer.WriteFloat(Region.HeightRange.ZMin)
			&& Writer.WriteFloat(Region.HeightRange.ZMax)
			&& Writer.WriteU8(static_cast<uint8>(Region.Shape))
			&& Writer.WriteDouble(Region.Center.X)
			&& Writer.WriteDouble(Region.Center.Y)
			&& Writer.WriteDouble(Region.HalfExtents.X)
			&& Writer.WriteDouble(Region.HalfExtents.Y)
			&& Writer.WriteFloat(Region.Radius)
			&& Writer.WriteFloat(Region.RotationDegrees)
			&& Writer.WriteBool(Region.bEnabled)
			&& Writer.WriteU32(static_cast<uint32>(Region.PolygonVertices.Num()))
			&& [&Writer, &Region]()
			{
				for (const FVector2D Vertex : Region.PolygonVertices)
				{
					if (!Writer.WriteDouble(Vertex.X) || !Writer.WriteDouble(Vertex.Y))
					{
						return false;
					}
				}
				return true;
			}();
	}

	bool ReadRegion(FReader& Reader, FSightWeavePersistentRegion& OutRegion)
	{
		uint8 Shape = 0;
		uint32 PolygonCount = 0;
		if (!Reader.ReadFloat(OutRegion.HeightRange.ZMin)
			|| !Reader.ReadFloat(OutRegion.HeightRange.ZMax)
			|| !Reader.ReadU8(Shape)
			|| Shape > static_cast<uint8>(ESightWeaveMemoryRegionShape::Polygon)
			|| !Reader.ReadDouble(OutRegion.Center.X)
			|| !Reader.ReadDouble(OutRegion.Center.Y)
			|| !Reader.ReadDouble(OutRegion.HalfExtents.X)
			|| !Reader.ReadDouble(OutRegion.HalfExtents.Y)
			|| !Reader.ReadFloat(OutRegion.Radius)
			|| !Reader.ReadFloat(OutRegion.RotationDegrees)
			|| !Reader.ReadBool(OutRegion.bEnabled)
			|| !Reader.ReadCount(PolygonCount, 16))
		{
			return false;
		}
		OutRegion.Shape = static_cast<ESightWeaveMemoryRegionShape>(Shape);
		OutRegion.PolygonVertices.SetNumUninitialized(static_cast<int32>(PolygonCount));
		for (FVector2D& Vertex : OutRegion.PolygonVertices)
		{
			if (!Reader.ReadDouble(Vertex.X) || !Reader.ReadDouble(Vertex.Y))
			{
				return false;
			}
		}
		return OutRegion.IsValid();
	}

	bool WriteLastSeen(FWriter& Writer, const FSightWeavePersistentLastSeenRecord& Record)
	{
		const FVector Translation = Record.WorldTransform.GetTranslation();
		const FQuat Rotation = Record.WorldTransform.GetRotation();
		const FVector Scale = Record.WorldTransform.GetScale3D();
		if (!Writer.WriteU64(Record.SnapshotRevision)
			|| !Writer.WriteU64(Record.EligibilityRevision)
			|| !Writer.WriteU64(Record.SourceLiveRevision)
			|| !Writer.WriteDouble(Translation.X)
			|| !Writer.WriteDouble(Translation.Y)
			|| !Writer.WriteDouble(Translation.Z)
			|| !Writer.WriteDouble(Rotation.X)
			|| !Writer.WriteDouble(Rotation.Y)
			|| !Writer.WriteDouble(Rotation.Z)
			|| !Writer.WriteDouble(Rotation.W)
			|| !Writer.WriteDouble(Scale.X)
			|| !Writer.WriteDouble(Scale.Y)
			|| !Writer.WriteDouble(Scale.Z)
			|| !Writer.WriteDouble(Record.WorldBounds.Min.X)
			|| !Writer.WriteDouble(Record.WorldBounds.Min.Y)
			|| !Writer.WriteDouble(Record.WorldBounds.Min.Z)
			|| !Writer.WriteDouble(Record.WorldBounds.Max.X)
			|| !Writer.WriteDouble(Record.WorldBounds.Max.Y)
			|| !Writer.WriteDouble(Record.WorldBounds.Max.Z)
			|| !Writer.WriteString(Record.StaticMeshAsset.ToString(), MaximumPathBytes)
			|| !Writer.WriteU32(static_cast<uint32>(Record.MaterialOverrides.Num())))
		{
			return false;
		}
		for (const FSoftObjectPath& Path : Record.MaterialOverrides)
		{
			if (!Writer.WriteString(Path.ToString(), MaximumPathBytes))
			{
				return false;
			}
		}
		return Writer.WriteName(Record.VisualVariantId)
			&& Writer.WriteU8(static_cast<uint8>(Record.CaptureReason))
			&& Writer.WriteU64(Record.CaptureTransitionIdentity)
			&& Writer.WriteU32(static_cast<uint32>(Record.Validity));
	}

	bool ReadLastSeen(FReader& Reader, FSightWeavePersistentLastSeenRecord& OutRecord)
	{
		FVector Translation;
		FQuat Rotation;
		FVector Scale;
		uint32 MaterialCount = 0;
		FString Path;
		uint8 CaptureReason = 0;
		uint32 Validity = 0;
		if (!Reader.ReadU64(OutRecord.SnapshotRevision)
			|| !Reader.ReadU64(OutRecord.EligibilityRevision)
			|| !Reader.ReadU64(OutRecord.SourceLiveRevision)
			|| !Reader.ReadDouble(Translation.X)
			|| !Reader.ReadDouble(Translation.Y)
			|| !Reader.ReadDouble(Translation.Z)
			|| !Reader.ReadDouble(Rotation.X)
			|| !Reader.ReadDouble(Rotation.Y)
			|| !Reader.ReadDouble(Rotation.Z)
			|| !Reader.ReadDouble(Rotation.W)
			|| !Reader.ReadDouble(Scale.X)
			|| !Reader.ReadDouble(Scale.Y)
			|| !Reader.ReadDouble(Scale.Z)
			|| !Reader.ReadDouble(OutRecord.WorldBounds.Min.X)
			|| !Reader.ReadDouble(OutRecord.WorldBounds.Min.Y)
			|| !Reader.ReadDouble(OutRecord.WorldBounds.Min.Z)
			|| !Reader.ReadDouble(OutRecord.WorldBounds.Max.X)
			|| !Reader.ReadDouble(OutRecord.WorldBounds.Max.Y)
			|| !Reader.ReadDouble(OutRecord.WorldBounds.Max.Z)
			|| !Reader.ReadString(Path, MaximumPathBytes)
			|| !Reader.ReadCount(MaterialCount, 4))
		{
			return false;
		}
		OutRecord.WorldTransform = FTransform(Rotation, Translation, Scale);
		OutRecord.WorldBounds.IsValid = 1;
		OutRecord.StaticMeshAsset = FSoftObjectPath(Path);
		OutRecord.MaterialOverrides.Reserve(static_cast<int32>(MaterialCount));
		for (uint32 Index = 0; Index < MaterialCount; ++Index)
		{
			if (!Reader.ReadString(Path, MaximumPathBytes))
			{
				return false;
			}
			OutRecord.MaterialOverrides.Emplace(Path);
		}
		if (!Reader.ReadName(OutRecord.VisualVariantId)
			|| !Reader.ReadU8(CaptureReason)
			|| CaptureReason > static_cast<uint8>(ESightWeaveSubjectCaptureReason::LiveToNonLive)
			|| !Reader.ReadU64(OutRecord.CaptureTransitionIdentity)
			|| !Reader.ReadU32(Validity))
		{
			return false;
		}
		OutRecord.CaptureReason = static_cast<ESightWeaveSubjectCaptureReason>(CaptureReason);
		OutRecord.Validity = static_cast<ESightWeaveSubjectSnapshotValidity>(Validity);
		return OutRecord.IsValid();
	}

	bool NormalizeAndValidate(FSightWeaveCanonicalSnapshot& Snapshot, FSightWeaveSnapshotDiagnostic& Diagnostic)
	{
		for (FSightWeaveSnapshotScopeRecord& ScopeRecord : Snapshot.Scopes)
		{
			for (FSightWeaveRenderProfileIdentity& Profile : ScopeRecord.Scope.CanonicalProfiles)
			{
				NormalizeProfile(Profile);
			}
			ScopeRecord.Scope.CanonicalProfiles.Sort([](const auto& A, const auto& B)
			{
				return ProfileKey(A) < ProfileKey(B);
			});
			ScopeRecord.MemoryTiles.Sort([](const auto& A, const auto& B)
			{
				return A.Key.LogicalCoordinate.X != B.Key.LogicalCoordinate.X
					? A.Key.LogicalCoordinate.X < B.Key.LogicalCoordinate.X
					: A.Key.LogicalCoordinate.Y < B.Key.LogicalCoordinate.Y;
			});
			ScopeRecord.PersistentModifiers.Sort([](const auto& A, const auto& B)
			{
				return NameLess(A.StableId, B.StableId);
			});
			ScopeRecord.Subjects.Sort([](const auto& A, const auto& B)
			{
				const int32 NameComparison = CompareName(A.Identity.StableId, B.Identity.StableId);
				return NameComparison != 0
					? NameComparison < 0
					: A.Identity.InstanceGeneration < B.Identity.InstanceGeneration;
			});
		}
		Snapshot.Scopes.Sort([](const auto& A, const auto& B)
		{
			return ScopeKey(A.Scope) < ScopeKey(B.Scope);
		});
		Snapshot.ProviderPayloads.Sort([](const auto& A, const auto& B)
		{
			const int32 ProviderComparison = CompareName(A.ProviderId, B.ProviderId);
			if (ProviderComparison != 0) return ProviderComparison < 0;
			if (A.SchemaVersion != B.SchemaVersion) return A.SchemaVersion < B.SchemaVersion;
			if (A.Domain.Type != B.Domain.Type) return A.Domain.Type < B.Domain.Type;
			const int32 ScopeComparison = CompareName(A.Domain.StableScopeId, B.Domain.StableScopeId);
			if (ScopeComparison != 0) return ScopeComparison < 0;
			const int32 SubjectComparison = CompareName(
				A.Domain.SubjectIdentity.StableId,
				B.Domain.SubjectIdentity.StableId);
			if (SubjectComparison != 0) return SubjectComparison < 0;
			if (A.Domain.SubjectIdentity.InstanceGeneration
				!= B.Domain.SubjectIdentity.InstanceGeneration)
			{
				return A.Domain.SubjectIdentity.InstanceGeneration
					< B.Domain.SubjectIdentity.InstanceGeneration;
			}
			return NameLess(A.Domain.SemanticDomainId, B.Domain.SemanticDomainId);
		});

		for (int32 ScopeIndex = 0; ScopeIndex < Snapshot.Scopes.Num(); ++ScopeIndex)
		{
			const FSightWeaveSnapshotScopeRecord& ScopeRecord = Snapshot.Scopes[ScopeIndex];
			if (!ScopeRecord.IsValid())
			{
				Diagnostic.Result = ESightWeaveSnapshotResult::InvalidScope;
				Diagnostic.PrimaryId = ScopeRecord.Scope.StableScopeId;
				return false;
			}
			if (ScopeIndex > 0
				&& CompareName(Snapshot.Scopes[ScopeIndex - 1].Scope.StableScopeId,
					ScopeRecord.Scope.StableScopeId) == 0)
			{
				Diagnostic.Result = ESightWeaveSnapshotResult::DuplicateScope;
				Diagnostic.PrimaryId = ScopeRecord.Scope.StableScopeId;
				return false;
			}
			for (int32 Index = 1; Index < ScopeRecord.MemoryTiles.Num(); ++Index)
			{
				if (ScopeRecord.MemoryTiles[Index - 1].Key.LogicalCoordinate
					== ScopeRecord.MemoryTiles[Index].Key.LogicalCoordinate)
				{
					Diagnostic.Result = ESightWeaveSnapshotResult::DuplicateTile;
					Diagnostic.PrimaryId = ScopeRecord.Scope.StableScopeId;
					return false;
				}
			}
			for (int32 Index = 1; Index < ScopeRecord.PersistentModifiers.Num(); ++Index)
			{
				if (CompareName(ScopeRecord.PersistentModifiers[Index - 1].StableId,
					ScopeRecord.PersistentModifiers[Index].StableId) == 0)
				{
					Diagnostic.Result = ESightWeaveSnapshotResult::DuplicateModifierId;
					Diagnostic.PrimaryId = ScopeRecord.PersistentModifiers[Index].StableId;
					return false;
				}
			}
			for (int32 Index = 1; Index < ScopeRecord.Subjects.Num(); ++Index)
			{
				if (ScopeRecord.Subjects[Index - 1].Identity.IsEquivalentTo(
					ScopeRecord.Subjects[Index].Identity))
				{
					Diagnostic.Result = ESightWeaveSnapshotResult::DuplicateSubjectId;
					Diagnostic.PrimaryId = ScopeRecord.Subjects[Index].Identity.StableId;
					return false;
				}
			}
		}
		for (int32 Index = 0; Index < Snapshot.ProviderPayloads.Num(); ++Index)
		{
			const auto& Provider = Snapshot.ProviderPayloads[Index];
			if (!Provider.IsValid())
			{
				Diagnostic.Result = ESightWeaveSnapshotResult::InvalidProviderPayload;
				Diagnostic.PrimaryId = Provider.ProviderId;
				return false;
			}
			if (Index > 0)
			{
				const auto& Prior = Snapshot.ProviderPayloads[Index - 1];
				const bool bSameDomain = CompareName(Prior.ProviderId, Provider.ProviderId) == 0
					&& Prior.SchemaVersion == Provider.SchemaVersion
					&& Prior.Domain.Type == Provider.Domain.Type
					&& CompareName(Prior.Domain.StableScopeId, Provider.Domain.StableScopeId) == 0
					&& Prior.Domain.SubjectIdentity.IsEquivalentTo(Provider.Domain.SubjectIdentity)
					&& CompareName(Prior.Domain.SemanticDomainId,
						Provider.Domain.SemanticDomainId) == 0;
				if (bSameDomain)
				{
					Diagnostic.Result = ESightWeaveSnapshotResult::DuplicateProviderId;
					Diagnostic.PrimaryId = Provider.ProviderId;
					return false;
				}
			}
		}
		return true;
	}

	bool WritePayload(FWriter& Writer, const FSightWeaveCanonicalSnapshot& Snapshot)
	{
		if (!Writer.WriteU32(PayloadVersion)
			|| !Writer.WriteU32(static_cast<uint32>(Snapshot.Scopes.Num()))
			|| !Writer.WriteU32(static_cast<uint32>(Snapshot.ProviderPayloads.Num())))
		{
			return false;
		}
		for (const FSightWeaveSnapshotScopeRecord& ScopeRecord : Snapshot.Scopes)
		{
			const auto& Scope = ScopeRecord.Scope;
			if (!Writer.WriteName(Scope.StableScopeId)
				|| !Writer.WriteName(Scope.KnowledgeOwnerId.GetValue())
				|| !Writer.WriteName(Scope.FloorId.GetValue())
				|| !Writer.WriteDouble(Scope.FloorOrigin.X)
				|| !Writer.WriteDouble(Scope.FloorOrigin.Y)
				|| !Writer.WriteFloat(Scope.FloorPlaneZ)
				|| !Writer.WriteU8(static_cast<uint8>(Scope.PrecisionTier))
				|| !Writer.WriteU32(static_cast<uint32>(Scope.CanonicalProfiles.Num())))
			{
				return false;
			}
			for (const auto& Profile : Scope.CanonicalProfiles)
			{
				if (!WriteProfile(Writer, Profile)) return false;
			}
			if (!Writer.WriteU32(static_cast<uint32>(ScopeRecord.MemoryTiles.Num())))
			{
				return false;
			}
			for (const auto& Tile : ScopeRecord.MemoryTiles)
			{
				if (!Writer.WriteI32(Tile.Key.LogicalCoordinate.X)
					|| !Writer.WriteI32(Tile.Key.LogicalCoordinate.Y)
					|| !Writer.WriteBytes(Tile.PackedBits.GetData(), Tile.PackedBits.Num()))
				{
					return false;
				}
			}
			if (!Writer.WriteU32(static_cast<uint32>(ScopeRecord.PersistentModifiers.Num())))
			{
				return false;
			}
			for (const auto& Modifier : ScopeRecord.PersistentModifiers)
			{
				if (!Writer.WriteName(Modifier.StableId)
					|| !Writer.WriteU8(static_cast<uint8>(Modifier.Operation))
					|| !WriteRegion(Writer, Modifier.Region))
				{
					return false;
				}
			}
			if (!Writer.WriteU32(static_cast<uint32>(ScopeRecord.Subjects.Num())))
			{
				return false;
			}
			for (const auto& Subject : ScopeRecord.Subjects)
			{
				if (!Writer.WriteName(Subject.Identity.StableId)
					|| !Writer.WriteU64(static_cast<uint64>(Subject.Identity.InstanceGeneration))
					|| !Writer.WriteU8(static_cast<uint8>(Subject.Policy))
					|| !Writer.WriteName(Subject.CustomProviderId)
					|| !Writer.WriteU32(Subject.CustomProviderVersion)
					|| !Writer.WriteBool(Subject.LastSeen.IsSet()))
				{
					return false;
				}
				if (Subject.LastSeen.IsSet() && !WriteLastSeen(Writer, Subject.LastSeen.GetValue()))
				{
					return false;
				}
			}
		}
		for (const auto& Provider : Snapshot.ProviderPayloads)
		{
			if (!Writer.WriteName(Provider.ProviderId)
				|| !Writer.WriteU32(Provider.SchemaVersion)
				|| !Writer.WriteU8(static_cast<uint8>(Provider.Domain.Type))
				|| !Writer.WriteName(Provider.Domain.StableScopeId)
				|| !Writer.WriteName(Provider.Domain.SubjectIdentity.StableId)
				|| !Writer.WriteU64(static_cast<uint64>(
					Provider.Domain.SubjectIdentity.InstanceGeneration))
				|| !WriteRegion(Writer, Provider.Domain.Region)
				|| !Writer.WriteName(Provider.Domain.SemanticDomainId)
				|| !Writer.WriteU32(static_cast<uint32>(Provider.Payload.Num()))
				|| !Writer.WriteBytes(Provider.Payload.GetData(), Provider.Payload.Num()))
			{
				return false;
			}
		}
		return !Writer.HasFailed();
	}

	bool ReadPayload(FReader& Reader, FSightWeaveCanonicalSnapshot& OutSnapshot)
	{
		uint32 Version = 0;
		uint32 ScopeCount = 0;
		uint32 ProviderCount = 0;
		if (!Reader.ReadU32(Version) || Version != PayloadVersion
			|| !Reader.ReadCount(ScopeCount, 25)
			|| !Reader.ReadCount(ProviderCount, 25))
		{
			return false;
		}
		OutSnapshot.Scopes.Reserve(static_cast<int32>(ScopeCount));
		for (uint32 ScopeIndex = 0; ScopeIndex < ScopeCount; ++ScopeIndex)
		{
			auto& ScopeRecord = OutSnapshot.Scopes.AddDefaulted_GetRef();
			auto& Scope = ScopeRecord.Scope;
			FName Owner;
			FName Floor;
			uint8 Precision = 0;
			uint32 ProfileCount = 0;
			uint32 TileCount = 0;
			uint32 ModifierCount = 0;
			uint32 SubjectCount = 0;
			if (!Reader.ReadName(Scope.StableScopeId)
				|| !Reader.ReadName(Owner)
				|| !Reader.ReadName(Floor)
				|| !Reader.ReadDouble(Scope.FloorOrigin.X)
				|| !Reader.ReadDouble(Scope.FloorOrigin.Y)
				|| !Reader.ReadFloat(Scope.FloorPlaneZ)
				|| !Reader.ReadU8(Precision)
				|| Precision > static_cast<uint8>(ESightWeaveRenderPrecisionTier::Ultra)
				|| !Reader.ReadCount(ProfileCount, 4))
			{
				return false;
			}
			Scope.KnowledgeOwnerId = FSightWeaveKnowledgeOwnerId(Owner);
			Scope.FloorId = FSightWeaveFloorId(Floor);
			Scope.PrecisionTier = static_cast<ESightWeaveRenderPrecisionTier>(Precision);
			Scope.CanonicalProfiles.Reserve(static_cast<int32>(ProfileCount));
			for (uint32 Index = 0; Index < ProfileCount; ++Index)
			{
				if (!ReadProfile(Reader, Scope.CanonicalProfiles.AddDefaulted_GetRef())) return false;
			}
			if (!Reader.ReadCount(TileCount, 8 + SightWeave::Memory::PackedBytesPerTile))
			{
				return false;
			}
			ScopeRecord.MemoryTiles.Reserve(static_cast<int32>(TileCount));
			for (uint32 Index = 0; Index < TileCount; ++Index)
			{
				auto& Tile = ScopeRecord.MemoryTiles.AddDefaulted_GetRef();
				Tile.PackedBits.SetNumUninitialized(SightWeave::Memory::PackedBytesPerTile);
				if (!Reader.ReadI32(Tile.Key.LogicalCoordinate.X)
					|| !Reader.ReadI32(Tile.Key.LogicalCoordinate.Y)
					|| !Reader.ReadBytes(Tile.PackedBits.GetData(), Tile.PackedBits.Num()))
				{
					return false;
				}
			}
			if (!Reader.ReadCount(ModifierCount, 10)) return false;
			ScopeRecord.PersistentModifiers.Reserve(static_cast<int32>(ModifierCount));
			for (uint32 Index = 0; Index < ModifierCount; ++Index)
			{
				auto& Modifier = ScopeRecord.PersistentModifiers.AddDefaulted_GetRef();
				uint8 Operation = 0;
				if (!Reader.ReadName(Modifier.StableId)
					|| !Reader.ReadU8(Operation)
					|| Operation > static_cast<uint8>(
						ESightWeaveMemoryModifierOperation::SuppressMemoryPresentation)
					|| !ReadRegion(Reader, Modifier.Region))
				{
					return false;
				}
				Modifier.Operation = static_cast<ESightWeaveMemoryModifierOperation>(Operation);
			}
			if (!Reader.ReadCount(SubjectCount, 20)) return false;
			ScopeRecord.Subjects.Reserve(static_cast<int32>(SubjectCount));
			for (uint32 Index = 0; Index < SubjectCount; ++Index)
			{
				auto& Subject = ScopeRecord.Subjects.AddDefaulted_GetRef();
				uint64 InstanceGeneration = 0;
				uint8 Policy = 0;
				bool bHasLastSeen = false;
				if (!Reader.ReadName(Subject.Identity.StableId)
					|| !Reader.ReadU64(InstanceGeneration)
					|| InstanceGeneration > static_cast<uint64>(MAX_int64)
					|| !Reader.ReadU8(Policy)
					|| Policy > static_cast<uint8>(ESightWeaveSubjectMemoryPolicy::Custom)
					|| !Reader.ReadName(Subject.CustomProviderId)
					|| !Reader.ReadU32(Subject.CustomProviderVersion)
					|| !Reader.ReadBool(bHasLastSeen))
				{
					return false;
				}
				Subject.Identity.InstanceGeneration = static_cast<int64>(InstanceGeneration);
				Subject.Policy = static_cast<ESightWeaveSubjectMemoryPolicy>(Policy);
				if (bHasLastSeen)
				{
					FSightWeavePersistentLastSeenRecord Record;
					if (!ReadLastSeen(Reader, Record)) return false;
					Subject.LastSeen = MoveTemp(Record);
				}
			}
		}
		OutSnapshot.ProviderPayloads.Reserve(static_cast<int32>(ProviderCount));
		for (uint32 Index = 0; Index < ProviderCount; ++Index)
		{
			auto& Provider = OutSnapshot.ProviderPayloads.AddDefaulted_GetRef();
			uint8 DomainType = 0;
			uint64 InstanceGeneration = 0;
			uint32 PayloadBytes = 0;
			if (!Reader.ReadName(Provider.ProviderId)
				|| !Reader.ReadU32(Provider.SchemaVersion)
				|| !Reader.ReadU8(DomainType)
				|| DomainType > static_cast<uint8>(ESightWeaveProviderDomainType::Semantic)
				|| !Reader.ReadName(Provider.Domain.StableScopeId)
				|| !Reader.ReadName(Provider.Domain.SubjectIdentity.StableId)
				|| !Reader.ReadU64(InstanceGeneration)
				|| InstanceGeneration > static_cast<uint64>(MAX_int64)
				|| !ReadRegion(Reader, Provider.Domain.Region)
				|| !Reader.ReadName(Provider.Domain.SemanticDomainId)
				|| !Reader.ReadCount(PayloadBytes, 1))
			{
				return false;
			}
			Provider.Domain.Type = static_cast<ESightWeaveProviderDomainType>(DomainType);
			Provider.Domain.SubjectIdentity.InstanceGeneration = static_cast<int64>(InstanceGeneration);
			Provider.Payload.SetNumUninitialized(static_cast<int32>(PayloadBytes));
			if (!Reader.ReadBytes(Provider.Payload.GetData(), Provider.Payload.Num()))
			{
				return false;
			}
		}
		return !Reader.HasFailed();
	}

	void WriteHeaderU16(TArray<uint8>& Bytes, const int32 Offset, const uint16 Value)
	{
		Bytes[Offset] = static_cast<uint8>(Value);
		Bytes[Offset + 1] = static_cast<uint8>(Value >> 8);
	}

	void WriteHeaderU32(TArray<uint8>& Bytes, const int32 Offset, const uint32 Value)
	{
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Bytes[Offset + Index] = static_cast<uint8>(Value >> (Index * 8));
		}
	}

	void WriteHeaderU64(TArray<uint8>& Bytes, const int32 Offset, const uint64 Value)
	{
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Bytes[Offset + Index] = static_cast<uint8>(Value >> (Index * 8));
		}
	}

	uint16 ReadHeaderU16(const TArray<uint8>& Bytes, const int32 Offset)
	{
		return static_cast<uint16>(Bytes[Offset])
			| (static_cast<uint16>(Bytes[Offset + 1]) << 8);
	}

	uint32 ReadHeaderU32(const TArray<uint8>& Bytes, const int32 Offset)
	{
		uint32 Value = 0;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Value |= static_cast<uint32>(Bytes[Offset + Index]) << (Index * 8);
		}
		return Value;
	}

	uint64 ReadHeaderU64(const TArray<uint8>& Bytes, const int32 Offset)
	{
		uint64 Value = 0;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Value |= static_cast<uint64>(Bytes[Offset + Index]) << (Index * 8);
		}
		return Value;
	}
}

bool FSightWeaveSnapshotLimits::IsValid() const
{
	return MaximumCanonicalBytes > 0 && MaximumCanonicalBytes <= MAX_int32
		&& MaximumStoredBlobBytes >= SightWeave::Persistence::HeaderBytes
		&& MaximumStoredBlobBytes <= MAX_int32;
}

bool FSightWeavePersistentScopeKey::IsValid() const
{
	if (StableScopeId.IsNone() || !KnowledgeOwnerId.IsValid() || !FloorId.IsValid()
		|| !SightWeavePersistencePrivate::IsFinite(FloorOrigin)
		|| !FMath::IsFinite(FloorPlaneZ)
		|| SightWeaveCentimetersPerTexel(PrecisionTier) <= 0.0f)
	{
		return false;
	}
	for (const auto& Profile : CanonicalProfiles)
	{
		if (!Profile.IsValid()) return false;
	}
	return true;
}

bool FSightWeavePersistentRegion::IsValid() const
{
	if (!HeightRange.IsValid()
		|| !SightWeavePersistencePrivate::IsFinite(Center)
		|| !SightWeavePersistencePrivate::IsFinite(HalfExtents)
		|| !FMath::IsFinite(Radius)
		|| !FMath::IsFinite(RotationDegrees))
	{
		return false;
	}
	switch (Shape)
	{
	case ESightWeaveMemoryRegionShape::Circle:
		return Radius > 0.0f;
	case ESightWeaveMemoryRegionShape::AxisAlignedBox:
	case ESightWeaveMemoryRegionShape::RotatedBox:
		return HalfExtents.X > 0.0 && HalfExtents.Y > 0.0;
	case ESightWeaveMemoryRegionShape::Polygon:
		return PolygonVertices.Num() >= 3
			&& !PolygonVertices.ContainsByPredicate([](const FVector2D Vertex)
			{
				return !SightWeavePersistencePrivate::IsFinite(Vertex);
			});
	default:
		return false;
	}
}

bool FSightWeavePersistentModifierRecord::IsValid() const
{
	return !StableId.IsNone()
		&& Operation <= ESightWeaveMemoryModifierOperation::SuppressMemoryPresentation
		&& Region.IsValid();
}

bool FSightWeavePersistentLastSeenRecord::IsValid() const
{
	return SnapshotRevision != 0 && EligibilityRevision != 0 && SourceLiveRevision != 0
		&& !WorldTransform.ContainsNaN() && WorldBounds.IsValid
		&& StaticMeshAsset.IsValid()
		&& !MaterialOverrides.ContainsByPredicate([](const FSoftObjectPath& Path)
		{
			return !Path.IsValid();
		})
		&& CaptureReason == ESightWeaveSubjectCaptureReason::LiveToNonLive
		&& CaptureTransitionIdentity != 0
		&& EnumHasAllFlags(Validity,
			SightWeave::SubjectMemory::RequiredBasicSnapshotValidity);
}

bool FSightWeavePersistentSubjectRecord::IsValid() const
{
	if (!Identity.IsValid()
		|| Policy > ESightWeaveSubjectMemoryPolicy::Custom)
	{
		return false;
	}
	if (Policy == ESightWeaveSubjectMemoryPolicy::Custom)
	{
		if (CustomProviderId.IsNone() || CustomProviderVersion == 0) return false;
	}
	else if (!CustomProviderId.IsNone() || CustomProviderVersion != 0)
	{
		return false;
	}
	if (LastSeen.IsSet())
	{
		return (Policy == ESightWeaveSubjectMemoryPolicy::LastSeenSnapshot
				|| Policy == ESightWeaveSubjectMemoryPolicy::Custom)
			&& LastSeen->IsValid();
	}
	return true;
}

bool FSightWeaveSnapshotScopeRecord::IsValid() const
{
	if (!Scope.IsValid()
		|| MemoryTiles.ContainsByPredicate([](const FSightWeavePackedMemoryTile& Tile)
		{
			return Tile.PackedBits.Num() != SightWeave::Memory::PackedBytesPerTile
				|| Tile.IsEmpty();
		})
		|| PersistentModifiers.ContainsByPredicate([](const auto& Modifier)
		{
			return !Modifier.IsValid();
		})
		|| Subjects.ContainsByPredicate([](const auto& Subject)
		{
			return !Subject.IsValid();
		}))
	{
		return false;
	}
	return true;
}

bool FSightWeaveProviderDomain::IsValid() const
{
	if (StableScopeId.IsNone()) return false;
	switch (Type)
	{
	case ESightWeaveProviderDomainType::Subject:
		return SubjectIdentity.IsValid();
	case ESightWeaveProviderDomainType::Region:
		return Region.IsValid();
	case ESightWeaveProviderDomainType::Semantic:
		return !SemanticDomainId.IsNone();
	default:
		return false;
	}
}

bool FSightWeaveProviderPayloadRecord::IsValid() const
{
	return !ProviderId.IsNone() && SchemaVersion != 0 && Domain.IsValid();
}

FSightWeaveSnapshotDiagnostic FSightWeavePersistence::BuildBlob(
	const FSightWeaveCanonicalSnapshot& Snapshot,
	FSightWeaveSnapshotBlob& OutBlob,
	const FSightWeaveSnapshotLimits& Limits)
{
	using namespace SightWeavePersistencePrivate;
	OutBlob.Bytes.Reset();
	FSightWeaveSnapshotDiagnostic Diagnostic;
	if (!Limits.IsValid())
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::SizeLimitExceeded;
		return Diagnostic;
	}
	FSightWeaveCanonicalSnapshot Canonical = Snapshot;
	if (!NormalizeAndValidate(Canonical, Diagnostic)) return Diagnostic;
	FWriter PayloadWriter(Limits.MaximumCanonicalBytes);
	if (!WritePayload(PayloadWriter, Canonical) || PayloadWriter.HasFailed())
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::SizeLimitExceeded;
		return Diagnostic;
	}
	const TArray<uint8>& CanonicalBytes = PayloadWriter.GetBytes();
	if (CanonicalBytes.Num() > Limits.MaximumCanonicalBytes)
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::SizeLimitExceeded;
		return Diagnostic;
	}
	const FBlake3Hash Hash = FBlake3::HashBuffer(CanonicalBytes.GetData(), CanonicalBytes.Num());
	TArray<uint8> StoredBytes = CanonicalBytes;
	ESightWeaveSnapshotCompressionMethod Method = ESightWeaveSnapshotCompressionMethod::None;
	uint32 Flags = 0;
	if (CanonicalBytes.Num() >= SightWeave::Persistence::CompressionThresholdBytes)
	{
		int64 Bound = 0;
		if (!FCompression::CompressMemoryBound(NAME_Zlib, Bound, CanonicalBytes.Num())
			|| Bound <= 0 || Bound > MAX_int32)
		{
			Diagnostic.Result = ESightWeaveSnapshotResult::CompressionFailed;
			return Diagnostic;
		}
		TArray<uint8> Compressed;
		Compressed.SetNumUninitialized(static_cast<int32>(Bound));
		int64 CompressedBytes = Bound;
		if (!FCompression::CompressMemory(
			NAME_Zlib,
			Compressed.GetData(),
			CompressedBytes,
			CanonicalBytes.GetData(),
			CanonicalBytes.Num(),
			COMPRESS_NoFlags))
		{
			Diagnostic.Result = ESightWeaveSnapshotResult::CompressionFailed;
			return Diagnostic;
		}
		if (CompressedBytes < CanonicalBytes.Num())
		{
			Compressed.SetNum(static_cast<int32>(CompressedBytes), EAllowShrinking::No);
			StoredBytes = MoveTemp(Compressed);
			Method = ESightWeaveSnapshotCompressionMethod::Zlib;
			Flags = CompressedFlag;
		}
	}
	if (StoredBytes.Num() > Limits.MaximumStoredBlobBytes - SightWeave::Persistence::HeaderBytes)
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::SizeLimitExceeded;
		return Diagnostic;
	}
	OutBlob.Bytes.SetNumZeroed(SightWeave::Persistence::HeaderBytes + StoredBytes.Num());
	FMemory::Memcpy(OutBlob.Bytes.GetData(), Magic, UE_ARRAY_COUNT(Magic));
	WriteHeaderU16(OutBlob.Bytes, 8, SightWeave::Persistence::FormatVersion);
	WriteHeaderU16(OutBlob.Bytes, 10, SightWeave::Persistence::HeaderBytes);
	WriteHeaderU32(OutBlob.Bytes, 12, Flags);
	OutBlob.Bytes[16] = static_cast<uint8>(Method);
	WriteHeaderU64(OutBlob.Bytes, 24, static_cast<uint64>(CanonicalBytes.Num()));
	WriteHeaderU64(OutBlob.Bytes, 32, static_cast<uint64>(StoredBytes.Num()));
	WriteHeaderU32(OutBlob.Bytes, 40, static_cast<uint32>(Canonical.Scopes.Num()));
	WriteHeaderU32(OutBlob.Bytes, 44, static_cast<uint32>(Canonical.ProviderPayloads.Num()));
	FMemory::Memcpy(OutBlob.Bytes.GetData() + 48, Hash.GetBytes(), 32);
	FMemory::Memcpy(
		OutBlob.Bytes.GetData() + SightWeave::Persistence::HeaderBytes,
		StoredBytes.GetData(),
		StoredBytes.Num());
	Diagnostic.Result = ESightWeaveSnapshotResult::Succeeded;
	Diagnostic.CompressionMethod = Method;
	Diagnostic.FormatVersion = SightWeave::Persistence::FormatVersion;
	Diagnostic.CanonicalBytes = CanonicalBytes.Num();
	Diagnostic.StoredBytes = OutBlob.Bytes.Num();
	Diagnostic.ScopeCount = Canonical.Scopes.Num();
	Diagnostic.ProviderPayloadCount = Canonical.ProviderPayloads.Num();
	return Diagnostic;
}

FSightWeaveSnapshotDiagnostic FSightWeavePersistence::ParseBlob(
	const FSightWeaveSnapshotBlob& Blob,
	FSightWeaveCanonicalSnapshot& OutSnapshot,
	const FSightWeaveSnapshotLimits& Limits)
{
	using namespace SightWeavePersistencePrivate;
	OutSnapshot = FSightWeaveCanonicalSnapshot();
	FSightWeaveSnapshotDiagnostic Diagnostic;
	if (!Limits.IsValid())
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::SizeLimitExceeded;
		return Diagnostic;
	}
	if (Blob.Bytes.IsEmpty())
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::EmptyBlob;
		return Diagnostic;
	}
	if (Blob.Bytes.Num() < SightWeave::Persistence::HeaderBytes)
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::Truncated;
		return Diagnostic;
	}
	if (Blob.Bytes.Num() > Limits.MaximumStoredBlobBytes)
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::SizeLimitExceeded;
		return Diagnostic;
	}
	if (FMemory::Memcmp(Blob.Bytes.GetData(), Magic, UE_ARRAY_COUNT(Magic)) != 0)
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::InvalidMagic;
		return Diagnostic;
	}
	const uint16 Version = ReadHeaderU16(Blob.Bytes, 8);
	Diagnostic.FormatVersion = Version;
	if (Version < SightWeave::Persistence::FormatVersion)
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::UnsupportedLegacyVersion;
		return Diagnostic;
	}
	if (Version > SightWeave::Persistence::FormatVersion)
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::FutureVersion;
		return Diagnostic;
	}
	const uint16 HeaderSize = ReadHeaderU16(Blob.Bytes, 10);
	const uint32 Flags = ReadHeaderU32(Blob.Bytes, 12);
	const uint8 MethodValue = Blob.Bytes[16];
	if (HeaderSize != SightWeave::Persistence::HeaderBytes)
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::InvalidHeader;
		return Diagnostic;
	}
	if ((Flags & ~CompressedFlag) != 0)
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::InvalidFlags;
		return Diagnostic;
	}
	for (int32 Index = 17; Index < 24; ++Index)
	{
		if (Blob.Bytes[Index] != 0)
		{
			Diagnostic.Result = ESightWeaveSnapshotResult::InvalidHeader;
			return Diagnostic;
		}
	}
	if (MethodValue > static_cast<uint8>(ESightWeaveSnapshotCompressionMethod::Zlib))
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::InvalidCompressionMethod;
		return Diagnostic;
	}
	const auto Method = static_cast<ESightWeaveSnapshotCompressionMethod>(MethodValue);
	Diagnostic.CompressionMethod = Method;
	if ((Flags == 0 && Method != ESightWeaveSnapshotCompressionMethod::None)
		|| ((Flags & CompressedFlag) != 0
			&& Method != ESightWeaveSnapshotCompressionMethod::Zlib))
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::InvalidCompressionMethod;
		return Diagnostic;
	}
	const uint64 CanonicalSize = ReadHeaderU64(Blob.Bytes, 24);
	const uint64 StoredPayloadSize = ReadHeaderU64(Blob.Bytes, 32);
	Diagnostic.CanonicalBytes = static_cast<int64>(CanonicalSize);
	Diagnostic.StoredBytes = Blob.Bytes.Num();
	Diagnostic.ScopeCount = ReadHeaderU32(Blob.Bytes, 40);
	Diagnostic.ProviderPayloadCount = ReadHeaderU32(Blob.Bytes, 44);
	if (CanonicalSize > static_cast<uint64>(Limits.MaximumCanonicalBytes)
		|| StoredPayloadSize > static_cast<uint64>(
			Limits.MaximumStoredBlobBytes - SightWeave::Persistence::HeaderBytes))
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::SizeLimitExceeded;
		return Diagnostic;
	}
	if (StoredPayloadSize > MAX_uint64 - SightWeave::Persistence::HeaderBytes)
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::SizeOverflow;
		return Diagnostic;
	}
	if (static_cast<uint64>(Blob.Bytes.Num())
		!= static_cast<uint64>(SightWeave::Persistence::HeaderBytes) + StoredPayloadSize)
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::SizeMismatch;
		return Diagnostic;
	}
	if (Method == ESightWeaveSnapshotCompressionMethod::None
		&& CanonicalSize != StoredPayloadSize)
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::SizeMismatch;
		return Diagnostic;
	}
	if (Method == ESightWeaveSnapshotCompressionMethod::Zlib
		&& (CanonicalSize < SightWeave::Persistence::CompressionThresholdBytes
			|| StoredPayloadSize >= CanonicalSize))
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::SizeMismatch;
		return Diagnostic;
	}
	TArray<uint8> CanonicalBytes;
	CanonicalBytes.SetNumUninitialized(static_cast<int32>(CanonicalSize));
	const uint8* StoredPayload =
		Blob.Bytes.GetData() + SightWeave::Persistence::HeaderBytes;
	if (Method == ESightWeaveSnapshotCompressionMethod::Zlib)
	{
		if (!FCompression::UncompressMemory(
			NAME_Zlib,
			CanonicalBytes.GetData(),
			CanonicalBytes.Num(),
			StoredPayload,
			static_cast<int64>(StoredPayloadSize),
			COMPRESS_NoFlags))
		{
			Diagnostic.Result = ESightWeaveSnapshotResult::DecompressionFailed;
			return Diagnostic;
		}
	}
	else if (CanonicalSize > 0)
	{
		FMemory::Memcpy(CanonicalBytes.GetData(), StoredPayload, CanonicalBytes.Num());
	}
	const FBlake3Hash ActualHash =
		FBlake3::HashBuffer(CanonicalBytes.GetData(), CanonicalBytes.Num());
	if (FMemory::Memcmp(Blob.Bytes.GetData() + 48, ActualHash.GetBytes(), 32) != 0)
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::ChecksumMismatch;
		return Diagnostic;
	}
	FReader Reader(CanonicalBytes);
	FSightWeaveCanonicalSnapshot Parsed;
	if (!ReadPayload(Reader, Parsed))
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::PayloadMalformed;
		Diagnostic.FailureOffset = Reader.GetOffset();
		return Diagnostic;
	}
	if (Reader.Remaining() != 0)
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::TrailingPayload;
		Diagnostic.FailureOffset = Reader.GetOffset();
		return Diagnostic;
	}
	if (Parsed.Scopes.Num() != static_cast<int32>(Diagnostic.ScopeCount)
		|| Parsed.ProviderPayloads.Num()
			!= static_cast<int32>(Diagnostic.ProviderPayloadCount))
	{
		Diagnostic.Result = ESightWeaveSnapshotResult::InvalidHeader;
		return Diagnostic;
	}
	FSightWeaveSnapshotDiagnostic Validation;
	if (!NormalizeAndValidate(Parsed, Validation))
	{
		Validation.FormatVersion = Version;
		Validation.CompressionMethod = Method;
		Validation.CanonicalBytes = CanonicalSize;
		Validation.StoredBytes = Blob.Bytes.Num();
		return Validation;
	}
	OutSnapshot = MoveTemp(Parsed);
	Diagnostic.Result = ESightWeaveSnapshotResult::Succeeded;
	return Diagnostic;
}
