// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/LNPEnemyWander.h"
#include "Enemy/LNPEnemyExactMovement.h"
#include "LNPWanderReplication.generated.h"

struct FLNPSurfaceDataSnapshot;

/** PositionYaw의 형제 필드. Iris 기본 정수 descriptor로 직렬화하며 native NetSerialize를 쓰지 않는다. */
USTRUCT()
struct LOOTNPOP_API FLNPReplicatedWanderData
{
	GENERATED_BODY()
	/** bit 0: 활성, bit 1: 비행, bit 2~4: slot. */
	UPROPERTY() uint8 Flags = 0;
	UPROPERTY() int16 StartX = 0;
	UPROPERTY() int16 StartY = 0;
	UPROPERTY() int16 StartZ = 0;
	UPROPERTY() int16 GoalX = 0;
	UPROPERTY() int16 GoalY = 0;
	UPROPERTY() int16 GoalZ = 0;
	UPROPERTY() uint32 StartMs = 0;
	UPROPERTY() uint16 TravelMs = 0;
	UPROPERTY() uint16 HoldMs = 0;
	UPROPERTY() uint16 Layer = 0;
	UPROPERTY() int16 Yaw = 0;

	bool IsEnabled() const { return (Flags & 1) != 0; }
	bool IsFlying() const { return (Flags & 2) != 0; }
	bool operator==(const FLNPReplicatedWanderData&) const = default;

	/** 좌표 범위·시간 한도를 넘으면 비활성 값을 돌려주며 조용히 clamp하지 않는다. */
	static FLNPReplicatedWanderData Encode(const FLNPEnemyWanderSegment& Segment, bool bFlying,
		const FLNPSurfaceHandle& Surface, float LocalYaw);
	/** uint32 ms의 가장 가까운 epoch. 수신 지연은 반주기(약 24.8일) 미만이어야 한다. */
	double DecodeStartTime(double ServerNow) const;
	void ApplyTo(FLNPEnemyWanderSegment& Segment, double ServerNow) const;
	bool IsMoving(double ServerNow) const;
	/** 지상은 현재 게시 snapshot의 같은 slot·Layer만 조회한다. 준비 전에는 false로 위치를 유지한다. */
	bool Evaluate(double ServerNow, const FLNPSurfaceDataSnapshot* Snapshot,
		const LNPEnemyExactMovement::FParams& Params, FTransform& OutTransform) const;
};
