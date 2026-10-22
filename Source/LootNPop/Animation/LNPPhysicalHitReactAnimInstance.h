// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/LNPAnimInstance.h"
#include "RigidBodyControlData.h"
#include "PhysicsControlData.h"
#include "LNPPhysicalHitReactAnimInstance.generated.h"

/**
 * ULNPPhysicalHitReactAnimInstance
 * VisualMesh 표시 ABP(ABP_Retarget·ABP_VisualCopyPose)의 부모. 왼손 IK 등 표시 메시 변수는 ULNPAnimInstance가 그대로 공급하고,
 * 이 클래스는 출력 앞 RigidBodyWithControl 노드 핀(힘·알파·컨트롤 배율)을 공급한다(애니메이션 개선 Phase 3).
 * 코스메틱이며 복제하지 않는다 — 각 머신이 HitReact 큐를 받아 로컬로 호출한다(AN-004).
 */
UCLASS()
class LOOTNPOP_API ULNPPhysicalHitReactAnimInstance : public ULNPAnimInstance
{
	GENERATED_BODY()

public:
	/**
	 * 다음 애님 업데이트 한 번에 줄 피격 반응을 예약한다. 같은 프레임에 여럿이면 가장 센 것만 남긴다.
	 * @param PushDirWorld      밀려날 방향(월드, 캐릭터 Up 접평면의 단위 벡터)
	 * @param ImpactPointWorld  맞은 지점(월드). 모르면 ZeroVector — 지점 임펄스는 가슴 중심으로 떨어진다
	 * @param Damage            피해량. 0 이하면 기준 피해로 본다
	 * @param bMelee            근접 피격인가(무기 종류 배율)
	 */
	void QueueHitReact(const FVector& PushDirWorld, const FVector& ImpactPointWorld, float Damage, bool bMelee);

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** RBWC ForcesAndTorques 핀. 예약이 있던 업데이트 한 번만 채워지고 다음 업데이트에 비워진다. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "LNP|PhysicalHitReact")
	FRigidBodyForcesAndTorques HitForces;

	/**
	 * RBWC ControlAndModifierParameters 핀. 피격 구간 동안 컨트롤 강도를 낮춘다(방향성 후보 3, 세트 ParentSpace_Torso = 사실상 온몸).
	 * 이 핀은 매 프레임 적용되고, 비우면 노드가 원래 강도로 돌아간다.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "LNP|PhysicalHitReact")
	FPhysicsControlControlAndModifierParameters HitControlParameters;

	/** RBWC KinematicTargets 핀. 골반에 캐릭터 Up 기준의 월드 공간 오프셋을 준다. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "LNP|PhysicalHitReact")
	FRigidBodyKinematicTargets HitKinematicTargets;

	/**
	 * RBWC Alpha 핀. 평소엔 0(순수 애니)이고 피격 때만 1로 올렸다가 유지 후 페이드한다.
	 * 상시 켜 두면 이동·점프 가속마다 상체 체인이 출렁인다. 랙돌 중에도 0(물리 씬 바디가 포즈를 덮는다).
	 */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "LNP|PhysicalHitReact")
	float PhysicsAlpha = 0.f;

private:
	/** 이번 피격의 힘·토크를 방향성 모드(LNP.Anim.PhysHitReactMode)대로 HitForces에 채운다. */
	void BuildHitForces();

	struct FPendingHit
	{
		FVector PushDir = FVector::ZeroVector;
		FVector ImpactPoint = FVector::ZeroVector;
		FVector Up = FVector::UpVector;
		float Strength = 0.f;
	};
	FPendingHit PendingHit;
	bool bHasPendingHit = false;

	/** 골반 오프셋은 임펄스와 달리 피격 구간 동안 매 업데이트 공급한다. */
	FPendingHit ActivePelvisHit;
	float PelvisHitElapsed = 0.f;
	float PelvisHitSide = 0.f;

	/** 물리 구간의 남은 시간(유지+페이드). 애님 DeltaSeconds로 줄어 HitStop 동안은 같이 늘어진다. */
	float HitBlendTimeRemaining = 0.f;
};
