// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"

enum class ELNPSpawnCandidateFlags : uint8
{
	None = 0,
	Pod = 1 << 0,
	Enemy = 1 << 1,
};
ENUM_CLASS_FLAGS(ELNPSpawnCandidateFlags);

/** LVI에서 수동 배치하고 표면에 투영한 Pod 세트 앵커. 좌표는 옥탄트 로컬이다. */
struct LOOTNPOP_API FLNPSpawnAuthoredAnchor
{
	FGuid SpawnPointId;
	FName TargetSpawnSetId;
	FTransform3f LocalTransform = FTransform3f::Identity;
	uint16 LocalLayerId = MAX_uint16;
	float EdgeClearance = 0.0f;
	float CapsuleClearance = 0.0f;
};

/** Support Layer에서 결정론적으로 만든 절차 배치 후보. */
struct LOOTNPOP_API FLNPSpawnRandomCandidate
{
	uint32 CandidateIndex = 0;
	FVector3f LocalPosition = FVector3f::ZeroVector;
	FVector3f LocalNormal = FVector3f::ZeroVector;
	uint16 LocalLayerId = MAX_uint16;
	ELNPSpawnCandidateFlags Allowed = ELNPSpawnCandidateFlags::None;
	float SlopeDot = 0.0f;
	float EdgeClearance = 0.0f;
	float CapsuleClearance = 0.0f;
};

struct LOOTNPOP_API FLNPSpawnData
{
	TArray<FLNPSpawnAuthoredAnchor> AuthoredAnchors;
	TArray<FLNPSpawnRandomCandidate> RandomCandidates;
};

namespace LNPSpawnData
{
	constexpr uint16 CodecVersion = 1;

	/** 입력 순서와 무관한 canonical little-endian payload를 만든다. */
	LOOTNPOP_API bool Encode(const FLNPSpawnData& Data, TArray<uint8>& OutPayload, FString& OutError);

	/** 범위·중복 ID·정렬·유한값을 함께 검증한다. */
	LOOTNPOP_API bool Decode(TConstArrayView<uint8> Payload, FLNPSpawnData& OutData, FString& OutError);
}
