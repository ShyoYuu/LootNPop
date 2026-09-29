// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class ULNPMassWorldCollisionSubsystem;
struct FLNPExactHitIdentity;

/**
 * Actor 없는 적(PureEntity)의 exact 이동 한 프레임(D-049, Phase03b §3.3).
 *
 * - 위치는 캡슐 중심이고 Up은 GravityOrigin 방향이다(내부형 구).
 * - 모든 스레드에서 호출할 수 있다. Mass worker의 동기 exact query만 쓴다(D-025).
 * - 공중 여부는 호출자의 속도 규약(0 = 접지)을 그대로 따른다. 이 함수들은 공중으로 넘길 때 0이 아닌 속도를 돌려준다.
 * - Phase 6에서 Support 캐시가 확신하지 못하는 구간의 exact 폴백으로 재사용한다.
 */
namespace LNPEnemyExactMovement
{
	/** LNP.SurfaceNav.EnemyExactLateralSweep. */
	LOOTNPOP_API bool IsLateralSweepEnabled();

	/** LNP.SurfaceNav.EnemyParallelMovement. 이동 프로세서의 청크 병렬 실행 여부(측정 요인, Phase03b §3.6.1). */
	LOOTNPOP_API bool IsParallelMovementEnabled();

	struct FParams
	{
		FVector GravityOrigin = FVector::ZeroVector;
		float GravityStrength = 2000.f;
		float CapsuleRadius = 35.f;
		float CapsuleHalfHeight = 88.f;

		/** 수평 sweep을 올리는 높이이자 오를 수 있는 단차(cm). walkable 경사에서 캡슐이 지면에 박히지 않도록 반지름 이상으로 둔다. */
		float MaxStepUp = 45.f;

		/** 접지를 유지한 채 내려갈 수 있는 낙차(cm). 이보다 깊으면 공중으로 넘어간다. */
		float MaxStepDown = 60.f;

		/** walkable 판정의 최소 dot(ImpactNormal, Up). 약 45도. */
		float WalkableMinDot = 0.71f;

		/** false면 수평 sweep을 생략하고 목표로 바로 옮긴다(측정 요인 분리용). */
		bool bLateralSweep = true;
	};

	enum class EGroundResult : uint8
	{
		/** 지지면 위에 섰다. */
		Grounded,
		/** 이동 끝에 지지면이 없다. OutVelocity가 0이 아닌 공중 상태로 넘어간다. */
		LostSupport,
		/** 이동 끝의 hit가 walkable 지지면이 아니다(가파른 경사·Blocker·Unknown). 수평 이동을 취소하고 제자리에 선다. */
		Rejected,
	};

	/**
	 * 접지 이동의 수평 blocker 판정만 수행한다. Support Atlas는 벽·프랍을 표현하지 않으므로
	 * Phase 6 cache-first 경로도 이동량이 있을 때 이 sweep을 유지한다.
	 */
	LOOTNPOP_API FVector MoveGroundedLaterally(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, const FVector& Velocity, float DeltaTime);

	/**
	 * 이미 수평 이동을 마친 캡슐 중심에서 exact support probe만 수행한다.
	 * OutSurfaceIdentity는 Grounded일 때만 채운다.
	 */
	LOOTNPOP_API EGroundResult ProbeGroundedAt(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& OriginalLocation, const FVector& TargetLocation, float DeltaTime,
		FVector& OutLocation, FVector& OutVelocity, FLNPExactHitIdentity* OutSurfaceIdentity = nullptr);

	/**
	 * 접지 상태 한 프레임. Velocity는 접평면 이동 속도다(법선 성분은 버린다).
	 * query: 수평 sweep 1회(막히면 미끄러짐 sweep 1회 추가) + 하향 probe 1회.
	 */
	LOOTNPOP_API EGroundResult StepGrounded(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, const FVector& Velocity, float DeltaTime, FVector& OutLocation, FVector& OutVelocity,
		FLNPExactHitIdentity* OutSurfaceIdentity = nullptr);

	/**
	 * 공중 상태 한 프레임. 중력을 적분하고 이전→제안 위치 capsule sweep의 earliest hit를 쓴다.
	 * walkable Support hit면 착지해 InOutVelocity를 0으로 만들고 true를 돌려준다.
	 * 그 밖의 hit(측벽·밑면·Blocker·Unknown)는 속도의 법선 성분을 지우고 미끄러진다. 속도는 0이 되지 않는다.
	 * query: capsule sweep 1회(시작부터 겹쳐 있으면 겹침을 풀고 1회 추가).
	 */
	LOOTNPOP_API bool StepAirborne(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& Location, FVector& InOutVelocity, float DeltaTime, FVector& OutLocation,
		FLNPExactHitIdentity* OutSurfaceIdentity = nullptr);

	/**
	 * 배회 목표 재투영. Direction 방향, 기준 위치와 같은 반지름의 점에서 위아래 Reach만큼 지지면을 찾는다.
	 * 찾으면 그 위의 캡슐 중심을 돌려준다. 섬 위 적이 가장자리 밖을, 섬 아래 적이 섬 윗면을 목표로 잡지 않게 한다.
	 */
	LOOTNPOP_API bool ProjectToSameLayer(const ULNPMassWorldCollisionSubsystem& Collision, const FParams& Params,
		const FVector& ReferenceLocation, const FVector& Direction, float Reach, FVector& OutCapsuleCenter,
		FLNPExactHitIdentity* OutSurfaceIdentity = nullptr);
}
