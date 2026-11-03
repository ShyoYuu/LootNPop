// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class ULNPMassWorldCollisionSubsystem;

/**
 * 라이플 총열을 실제 발사 방향과 나란히 맞추는 상체 피벗 회전 해. 표시 ABP 노드(FAnimNode_LNPRifleAim)와
 * UAF 시험 trait가 공유한다. 발사 원점·방향은 실탄과 같은 헬퍼(ClipMuzzleToWorld·ResolveAimDirection)로 구한다.
 * 이번 평가 포즈만 읽고 이전 프레임 보정을 누적하지 않는다. 모든 스레드.
 */
struct LOOTNPOP_API FLNPRifleAimSolver
{
	/** Pivot·MuzzleWorld는 이번 평가 포즈의 월드 값이고, 나머지는 게임 스레드에서 복사한 발사 입력이다. */
	FLNPRifleAimSolver(const FVector& InPivot, const FTransform& MuzzleWorld, const FVector& InAimTarget, const FVector& InViewDirection,
		const FVector& InMuzzleOffsetWorld, const FVector& InActorLocation, const ULNPMassWorldCollisionSubsystem* InWorldCollision);

	/** 피벗을 월드 회전 Rotation만큼 돌렸을 때의 실제 발사 원점(월드 클리핑 포함). */
	FVector SpawnPosition(const FQuat& Rotation) const;

	/** 그 원점에서의 실제 발사 방향. */
	FVector AimDirection(const FQuat& Rotation) const;

	/** 총열이 실제 발사 방향과 나란해지는 피벗 월드 회전(완전 보정). */
	FQuat Solve() const;

	FVector Pivot;
	FVector Lever;
	FVector Barrel;
	FVector AimTarget;
	FVector ViewDirection;
	FVector MuzzleOffsetWorld;
	FVector ActorLocation;
	const ULNPMassWorldCollisionSubsystem* WorldCollision;
};
