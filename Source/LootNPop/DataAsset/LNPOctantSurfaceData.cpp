// Copyright (c) 2026 LootNPop. All rights reserved.

#include "DataAsset/LNPOctantSurfaceData.h"

FLNPContentHash::FLNPContentHash()
{
	FMemory::Memzero(Bytes);
}

FLNPContentHash::FLNPContentHash(const FIoHash& InHash)
{
	FMemory::Memcpy(Bytes, InHash.GetBytes(), NumBytes);
}

FIoHash FLNPContentHash::ToIoHash() const
{
	return FIoHash(Bytes);
}

FString FLNPContentHash::ToString() const
{
	return LexToString(ToIoHash());
}

bool FLNPContentHash::IsZero() const
{
	return ToIoHash().IsZero();
}

bool FLNPContentHash::operator==(const FLNPContentHash& Other) const
{
	return FMemory::Memcmp(Bytes, Other.Bytes, NumBytes) == 0;
}

FLNPSurfaceBakeHeader::FLNPSurfaceBakeHeader()
	: DataVersion(CurrentDataVersion)
{
}
