// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/LNPEnemyExactMovement.h"
#include "SurfaceNavigation/LNPSurfaceTypes.h"

class ULNPMassWorldCollisionSubsystem;
struct FLNPExactHitIdentity;
struct FLNPEnemyDynamicSupportContact;
struct FLNPDynamicSupportFrame;
struct FLNPSurfaceDataSnapshot;

/** Phase 6의 Support snapshot 우선·exact 폴백 PureEntity 이동 조합. */
namespace LNPEnemySurfaceMovement
{
	/** `LNP.SurfaceNav.EnemySupportCache`. false면 Phase 3b exact-only 기준선이다. */
	LOOTNPOP_API bool IsSupportCacheEnabled();

	/** grounded cache/exact 계측을 비우고 보고한다. 부하 harness도 capture 경계에서 같은 API를 쓴다. */
	LOOTNPOP_API void ResetGroundStats();
	LOOTNPOP_API void ReportGroundStats();

	/** exact face identity가 현재 SurfaceData generation의 정적 Layer를 가리키면 handle로 바꾼다. */
	LOOTNPOP_API bool MakeSurfaceHandle(const FLNPExactHitIdentity& Identity, FLNPSurfaceHandle& OutHandle);

	/** Dynamic exact identity와 게시 snapshot으로 패널 로컬 접촉을 만든다. */
	LOOTNPOP_API bool MakeDynamicSupportContact(
		const FLNPExactHitIdentity& Identity,
		const FLNPDynamicSupportFrame& DynamicFrame,
		const FVector& CapsuleCenter,
		FLNPEnemyDynamicSupportContact& OutContact);

	/** 저장된 로컬 접촉을 현재 패널 자세로 운반하고 회전 delta도 적용한다. */
	LOOTNPOP_API bool ApplyDynamicSupportDelta(
		const FLNPDynamicSupportFrame& DynamicFrame,
		FLNPEnemyDynamicSupportContact& InOutContact,
		FTransform& InOutTransform);

	/** Mover가 소유한 Actor 상태를 PureEntity 재개용 속도·정적 handle·동적 contact로 변환한다. */
	LOOTNPOP_API void UpdateActorHandoff(
		bool bAirborne,
		const FVector& ActorVelocity,
		const FLNPExactHitIdentity* FloorIdentity,
		const FLNPDynamicSupportFrame& DynamicFrame,
		const FVector& CapsuleCenter,
		FVector& OutVelocity,
		FLNPSurfaceHandle& InOutSurface,
		FLNPEnemyDynamicSupportContact& InOutDynamicContact);

	/** scene query 없이 snapshot의 HighConfidence 지면을 캡슐 중심과 handle로 바꾼다. */
	LOOTNPOP_API bool TryResolveCachedGround(
		const FLNPSurfaceDataSnapshot& Snapshot,
		const LNPEnemyExactMovement::FParams& Params,
		const FVector& TargetLocation,
		FLNPSurfaceHandle& InOutSurface,
		FVector& OutLocation);

	/**
	 * Idle 배회 목표를 현재 정적 Layer에 투영한다. HighConfidence snapshot을 우선하고,
	 * 비확신 결과는 exact identity가 현재 handle과 같을 때만 채택한다.
	 */
	LOOTNPOP_API bool ProjectWanderTarget(
		const FLNPSurfaceDataSnapshot* SurfaceSnapshot,
		const ULNPMassWorldCollisionSubsystem& Collision,
		const LNPEnemyExactMovement::FParams& Params,
		const FVector& ReferenceLocation,
		const FVector& Direction,
		float Reach,
		const FLNPSurfaceHandle& CurrentSurface,
		FVector& OutCapsuleCenter);

	/** 공중 프레임 결과에 따라 이전 handle을 지우고, 정적 Support 착지 identity만 새 handle로 기록한다. */
	LOOTNPOP_API void UpdateSurfaceHandleAfterAirborne(
		bool bLanded,
		const FLNPExactHitIdentity& Identity,
		FLNPSurfaceHandle& InOutSurface);

	/**
	 * grounded 한 프레임. 수평 blocker sweep 뒤 cache high-confidence를 우선하고,
	 * 그 밖의 상태는 같은 도달점에서 exact support probe로 폴백한다.
	 */
	LOOTNPOP_API LNPEnemyExactMovement::EGroundResult StepGrounded(
		const FLNPSurfaceDataSnapshot* SurfaceSnapshot,
		const ULNPMassWorldCollisionSubsystem& Collision,
		const LNPEnemyExactMovement::FParams& Params,
		const FVector& Location,
		const FVector& Velocity,
		float DeltaTime,
		FVector& OutLocation,
		FVector& OutVelocity,
		FLNPSurfaceHandle& InOutSurface,
		FLNPExactHitIdentity* OutSurfaceIdentity = nullptr);

	/** 공중은 항상 exact다. 착지한 정적 Support의 handle을 기록하고 그 밖에는 무효화한다. */
	LOOTNPOP_API bool StepAirborne(
		const ULNPMassWorldCollisionSubsystem& Collision,
		const LNPEnemyExactMovement::FParams& Params,
		const FVector& Location,
		FVector& InOutVelocity,
		float DeltaTime,
		FVector& OutLocation,
		FLNPSurfaceHandle& InOutSurface,
		FLNPExactHitIdentity* OutSurfaceIdentity = nullptr);
}
