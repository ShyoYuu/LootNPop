// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "RigidBodyControlData.h"
#include "LNPPhysicalHitReactAnimInstance.generated.h"

/**
 * ULNPPhysicalHitReactAnimInstance
 * VisualMesh용 물리 히트리액트 애님 인스턴스 (애니메이션 개선 Phase 1A 스파이크).
 * ABP_Retarget 사본의 RigidBodyWithControl 노드 핀에 피격 속도 변화와 노드 알파를 공급한다.
 * 코스메틱이며 복제하지 않는다 — 각 머신이 HitReact 큐를 받아 로컬로 호출한다(AN-004).
 */
UCLASS()
class LOOTNPOP_API ULNPPhysicalHitReactAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** 다음 애님 업데이트 한 번에 상체 바디로 줄 속도 변화(월드, cm/s)를 예약한다. 같은 프레임의 피격은 합산한다. */
	void QueueHitImpulse(const FVector& VelocityChangeWorld);

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** RBWC ForcesAndTorques 핀. 예약이 있던 업데이트 한 번만 채워지고 다음 업데이트에 비워진다. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "LNP|PhysicalHitReact")
	FRigidBodyForcesAndTorques HitForces;

	/**
	 * RBWC Alpha 핀. 평소엔 0(순수 애니)이고 피격 때만 1로 올렸다가 유지 후 페이드한다.
	 * 상시 켜 두면 이동·점프 가속마다 상체 체인이 출렁인다. 랙돌 중에도 0(물리 씬 바디가 포즈를 덮는다).
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "LNP|PhysicalHitReact")
	float PhysicsAlpha = 0.f;

private:
	FVector PendingVelocityChange = FVector::ZeroVector;
	bool bHasPendingImpulse = false;

	/** 물리 구간의 남은 시간(유지+페이드). 애님 DeltaSeconds로 줄어 HitStop 동안은 같이 늘어진다. */
	float HitBlendTimeRemaining = 0.f;
};
