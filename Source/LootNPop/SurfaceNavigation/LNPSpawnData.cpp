// Copyright (c) 2026 LootNPop. All rights reserved.

#include "SurfaceNavigation/LNPSpawnData.h"

#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "SurfaceNavigation/LNPSupportAtlas.h"

namespace
{
	constexpr int32 MaxRecordCount = 4 * 1024 * 1024;
	constexpr int32 MaxNameBytes = 1024;

	bool SpawnGuidLess(const FGuid& A, const FGuid& B)
	{
		return A.A != B.A ? A.A < B.A : A.B != B.B ? A.B < B.B : A.C != B.C ? A.C < B.C : A.D < B.D;
	}

	bool IsFinite(const FVector3f& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	bool IsFinite(const FQuat4f& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z) && FMath::IsFinite(Value.W);
	}

	bool ValidateAnchor(const FLNPSpawnAuthoredAnchor& Anchor, FString& OutError)
	{
		const FQuat4f Rotation = Anchor.LocalTransform.GetRotation();
		if (!Anchor.SpawnPointId.IsValid() || !IsFinite(Anchor.LocalTransform.GetLocation()) || !IsFinite(Rotation)
			|| !FMath::IsFinite(Anchor.EdgeClearance) || !FMath::IsFinite(Anchor.CapsuleClearance)
			|| !FMath::IsFinite(Anchor.Headroom) || Anchor.Headroom < 0.0f || Anchor.Headroom > LNPSpawnData::MaxHeadroom
			|| Anchor.LocalLayerId == MAX_uint16 || !Rotation.IsNormalized())
		{
			OutError = FString::Printf(TEXT("Invalid authored spawn anchor %s."), *Anchor.SpawnPointId.ToString(EGuidFormats::Short));
			return false;
		}
		return true;
	}

	bool ValidateCandidate(const FLNPSpawnRandomCandidate& Candidate, FString& OutError)
	{
		const uint8 Allowed = static_cast<uint8>(Candidate.Allowed);
		if (!IsFinite(Candidate.LocalPosition) || !IsFinite(Candidate.LocalNormal) || Candidate.LocalNormal.IsNearlyZero()
			|| Candidate.LocalLayerId == MAX_uint16 || (Allowed & ~static_cast<uint8>(ELNPSpawnCandidateFlags::Pod | ELNPSpawnCandidateFlags::Enemy)) != 0
			|| Allowed == 0 || !FMath::IsFinite(Candidate.SlopeDot) || !FMath::IsFinite(Candidate.EdgeClearance)
			|| !FMath::IsFinite(Candidate.CapsuleClearance) || !FMath::IsFinite(Candidate.Headroom)
			|| Candidate.Headroom < 0.0f || Candidate.Headroom > LNPSpawnData::MaxHeadroom)
		{
			OutError = FString::Printf(TEXT("Invalid random spawn candidate %u."), Candidate.CandidateIndex);
			return false;
		}
		return true;
	}

	void SerializeName(FArchive& Archive, FName& Name)
	{
		if (Archive.IsSaving())
		{
			FTCHARToUTF8 Utf8(*Name.ToString());
			int32 Length = Name.IsNone() ? 0 : Utf8.Length();
			Archive << Length;
			if (Length > 0)
			{
				Archive.Serialize(const_cast<ANSICHAR*>(Utf8.Get()), Length);
			}
		}
		else
		{
			int32 Length = 0;
			Archive << Length;
			if (Length < 0 || Length > MaxNameBytes || Length > Archive.TotalSize() - Archive.Tell())
			{
				Archive.SetError();
				return;
			}
			TArray<ANSICHAR> Bytes;
			Bytes.SetNumUninitialized(Length + 1);
			if (Length > 0)
			{
				Archive.Serialize(Bytes.GetData(), Length);
			}
			Bytes[Length] = '\0';
			Name = Length == 0 ? NAME_None : FName(UTF8_TO_TCHAR(Bytes.GetData()));
		}
	}

	void SerializeVector(FArchive& Archive, FVector3f& Value)
	{
		Archive << Value.X << Value.Y << Value.Z;
	}

	void SerializeQuat(FArchive& Archive, FQuat4f& Value)
	{
		Archive << Value.X << Value.Y << Value.Z << Value.W;
	}
}

bool LNPSpawnData::Encode(const FLNPSpawnData& Data, TArray<uint8>& OutPayload, FString& OutError)
{
	OutPayload.Reset();
	OutError.Reset();
	FLNPSpawnData Canonical = Data;
	Canonical.AuthoredAnchors.Sort([](const FLNPSpawnAuthoredAnchor& A, const FLNPSpawnAuthoredAnchor& B)
	{
		return SpawnGuidLess(A.SpawnPointId, B.SpawnPointId);
	});
	Canonical.RandomCandidates.Sort([](const FLNPSpawnRandomCandidate& A, const FLNPSpawnRandomCandidate& B)
	{
		return A.CandidateIndex < B.CandidateIndex;
	});

	for (int32 Index = 0; Index < Canonical.AuthoredAnchors.Num(); ++Index)
	{
		if (!ValidateAnchor(Canonical.AuthoredAnchors[Index], OutError)
			|| (Index > 0 && Canonical.AuthoredAnchors[Index - 1].SpawnPointId == Canonical.AuthoredAnchors[Index].SpawnPointId))
		{
			if (OutError.IsEmpty()) OutError = TEXT("Duplicate authored SpawnPointId.");
			return false;
		}
	}
	for (int32 Index = 0; Index < Canonical.RandomCandidates.Num(); ++Index)
	{
		if (!ValidateCandidate(Canonical.RandomCandidates[Index], OutError)
			|| (Index > 0 && Canonical.RandomCandidates[Index - 1].CandidateIndex == Canonical.RandomCandidates[Index].CandidateIndex))
		{
			if (OutError.IsEmpty()) OutError = TEXT("Duplicate random candidate index.");
			return false;
		}
	}

	FMemoryWriter Writer(OutPayload, true);
	Writer.SetByteSwapping(!PLATFORM_LITTLE_ENDIAN);
	uint16 Version = CodecVersion;
	uint16 Reserved = 0;
	uint32 AuthoredCount = Canonical.AuthoredAnchors.Num();
	uint32 RandomCount = Canonical.RandomCandidates.Num();
	Writer << Version << Reserved << AuthoredCount << RandomCount;
	for (FLNPSpawnAuthoredAnchor& Anchor : Canonical.AuthoredAnchors)
	{
		Writer << Anchor.SpawnPointId.A << Anchor.SpawnPointId.B << Anchor.SpawnPointId.C << Anchor.SpawnPointId.D;
		SerializeName(Writer, Anchor.TargetSpawnSetId);
		FVector3f Location = Anchor.LocalTransform.GetLocation();
		FQuat4f Rotation = Anchor.LocalTransform.GetRotation();
		SerializeVector(Writer, Location);
		SerializeQuat(Writer, Rotation);
		Writer << Anchor.LocalLayerId << Reserved << Anchor.EdgeClearance << Anchor.CapsuleClearance << Anchor.Headroom;
	}
	for (FLNPSpawnRandomCandidate& Candidate : Canonical.RandomCandidates)
	{
		int16 NormalX = 0;
		int16 NormalY = 0;
		LNPSupportAtlas::EncodeNormal(Candidate.LocalNormal.GetSafeNormal(), NormalX, NormalY);
		uint8 Allowed = static_cast<uint8>(Candidate.Allowed);
		uint8 ByteReserved = 0;
		Writer << Candidate.CandidateIndex;
		SerializeVector(Writer, Candidate.LocalPosition);
		Writer << NormalX << NormalY << Candidate.LocalLayerId << Allowed << ByteReserved;
		Writer << Candidate.SlopeDot << Candidate.EdgeClearance << Candidate.CapsuleClearance << Candidate.Headroom;
	}
	if (Writer.IsError())
	{
		OutPayload.Reset();
		OutError = TEXT("Failed to encode Spawn payload.");
		return false;
	}
	return true;
}

bool LNPSpawnData::Decode(TConstArrayView<uint8> Payload, FLNPSpawnData& OutData, FString& OutError)
{
	OutData = FLNPSpawnData();
	OutError.Reset();
	if (Payload.Num() < 12)
	{
		OutError = TEXT("Spawn payload is truncated before its header.");
		return false;
	}
	FMemoryReaderView Reader(Payload, true);
	Reader.SetByteSwapping(!PLATFORM_LITTLE_ENDIAN);
	uint16 Version = 0;
	uint16 Reserved = 0;
	uint32 AuthoredCount = 0;
	uint32 RandomCount = 0;
	Reader << Version << Reserved << AuthoredCount << RandomCount;
	if (Version != CodecVersion || Reserved != 0 || AuthoredCount > MaxRecordCount || RandomCount > MaxRecordCount)
	{
		OutError = FString::Printf(TEXT("Invalid Spawn payload header (version=%u authored=%u random=%u)."),
			Version, AuthoredCount, RandomCount);
		return false;
	}
	OutData.AuthoredAnchors.SetNum(AuthoredCount);
	for (FLNPSpawnAuthoredAnchor& Anchor : OutData.AuthoredAnchors)
	{
		Reader << Anchor.SpawnPointId.A << Anchor.SpawnPointId.B << Anchor.SpawnPointId.C << Anchor.SpawnPointId.D;
		SerializeName(Reader, Anchor.TargetSpawnSetId);
		FVector3f Location;
		FQuat4f Rotation;
		SerializeVector(Reader, Location);
		SerializeQuat(Reader, Rotation);
		uint16 RecordReserved = 0;
		Reader << Anchor.LocalLayerId << RecordReserved << Anchor.EdgeClearance << Anchor.CapsuleClearance << Anchor.Headroom;
		if (RecordReserved != 0)
		{
			Reader.SetError();
		}
		Anchor.LocalTransform = FTransform3f(Rotation, Location);
	}
	OutData.RandomCandidates.SetNum(RandomCount);
	for (FLNPSpawnRandomCandidate& Candidate : OutData.RandomCandidates)
	{
		int16 NormalX = 0;
		int16 NormalY = 0;
		uint8 Allowed = 0;
		uint8 RecordReserved = 0;
		Reader << Candidate.CandidateIndex;
		SerializeVector(Reader, Candidate.LocalPosition);
		Reader << NormalX << NormalY << Candidate.LocalLayerId << Allowed << RecordReserved;
		Reader << Candidate.SlopeDot << Candidate.EdgeClearance << Candidate.CapsuleClearance << Candidate.Headroom;
		Candidate.LocalNormal = LNPSupportAtlas::DecodeNormal(NormalX, NormalY);
		Candidate.Allowed = static_cast<ELNPSpawnCandidateFlags>(Allowed);
		if (RecordReserved != 0)
		{
			Reader.SetError();
		}
	}
	if (Reader.IsError() || Reader.Tell() != Payload.Num())
	{
		OutData = FLNPSpawnData();
		OutError = TEXT("Spawn payload is truncated, malformed, or has trailing bytes.");
		return false;
	}

	for (int32 Index = 0; Index < OutData.AuthoredAnchors.Num(); ++Index)
	{
		if (!ValidateAnchor(OutData.AuthoredAnchors[Index], OutError)
			|| (Index > 0 && !SpawnGuidLess(OutData.AuthoredAnchors[Index - 1].SpawnPointId, OutData.AuthoredAnchors[Index].SpawnPointId)))
		{
			if (OutError.IsEmpty()) OutError = TEXT("Authored anchors are not uniquely sorted.");
			OutData = FLNPSpawnData();
			return false;
		}
	}
	for (int32 Index = 0; Index < OutData.RandomCandidates.Num(); ++Index)
	{
		if (!ValidateCandidate(OutData.RandomCandidates[Index], OutError)
			|| (Index > 0 && OutData.RandomCandidates[Index - 1].CandidateIndex >= OutData.RandomCandidates[Index].CandidateIndex))
		{
			if (OutError.IsEmpty()) OutError = TEXT("Random candidates are not uniquely sorted.");
			OutData = FLNPSpawnData();
			return false;
		}
	}
	return true;
}
