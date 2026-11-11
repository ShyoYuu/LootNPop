// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimData/BoneMaskFilter.h"
#include "Animation/AnimTypes.h"
#include "BoneContainer.h"
#include "AnimNode_LNPLocomotionDeltaBlend.generated.h"

class UAnimSequenceBase;

/**
 * 로코모션 보존 상체 블렌드. Layered Blend per Bone(메시 공간 회전)을 대체한다.
 *
 * 무기 오버레이를 메시 공간으로 덮으면 상체 방향이 무기 자세에 고정돼, 착지 숙임처럼 로코모션이 상체에 주는
 * 움직임이 사라지고 골반만 움직여 허리가 꺾여 보인다. 이 노드는 덮기 전에 척추 체인 각 본의
 * "로코모션 포즈 − 기준 포즈(중립 아이들 한 프레임)" 회전 차이를 **메시 공간**에서 구해 오버레이에 얹는다.
 *
 * - 메시 공간인 이유: 무기 자세는 몸통이 옆으로 돌아 있어, 로컬 공간 델타를 얹으면 "앞으로 숙임"이 옆 꺾임이 된다.
 * - 수직축(메시 Z) 비틀기는 버린다: 달리기의 몸통 좌우 비틀기가 가슴·팔을 흔들고 조준 방향을 돌리기 때문이다.
 *   숙임·기울임(swing)만 옮긴다.
 * - 체인 끝 본의 자손(목·머리·팔·무기)은 끝 본의 델타를 그대로 받아 상체와 한 덩어리로 움직인다.
 *
 * - 델타는 임계 감쇠 스프링으로 거른다: 걸음마다 생기는 잔떨림을 줄이고, 착지 숙임이 무게감 있게 따라오게 한다.
 * - 체인 끝 본에 실제로 얹은 델타를 커브 3개(메시 공간 회전 벡터, 라디안)로 내보낸다. 표시 ABP의 총열 보정은
 *   이 기울기를 되돌리지 않는다 — 총도 몸과 함께 숙여졌다가 같은 속도로 돌아온다.
 *
 * 골반 안정화: GASP 로코모션은 비무장 몸이라 착지·달리기에서 골반이 크게 출렁인다. 무기를 든 전투 자세에서는
 * 이 출렁임(오래 유지되는 기울기를 뺀 순간 성분의 숙임·기울임)을 줄인다. 수직축 회전과 골반 높이는 그대로이고,
 * 다리는 메시 공간 회전을 유지해 발·무릎은 로코모션 그대로다. 상체를 풀어 골반에 맞추는 대신 골반을 잡아 상체에 맞춘다.
 *
 * 그 뒤의 합성은 Layered Blend per Bone과 같은 엔진 경로(브랜치 필터 마스크·메시 공간 회전·커브 Override)다.
 */
USTRUCT(BlueprintInternalUseOnly)
struct LOOTNPOP_API FAnimNode_LNPLocomotionDeltaBlend : public FAnimNode_Base
{
	GENERATED_BODY()

	/** 로코모션 포즈. 마스크 밖 본은 그대로 나가고, 델타의 원본이 된다. */
	UPROPERTY(EditAnywhere, Category = Links)
	FPoseLink BasePose;

	/** 무기 자세(조준 AO 포함). 마스크 안 본이 이 포즈 + 로코모션 델타로 덮인다. */
	UPROPERTY(EditAnywhere, Category = Links)
	FPoseLink OverlayPose;

	/** 덮을 범위. Layered Blend per Bone의 브랜치 필터와 같은 의미다(깊이로 가중치 경사). */
	UPROPERTY(EditAnywhere, Category = Config)
	FInputBlendPose LayerSetup;

	/** 델타 기준 포즈 — 로코모션의 중립 아이들. 이 포즈일 때 델타가 0이다. */
	UPROPERTY(EditAnywhere, Category = Config)
	TObjectPtr<UAnimSequenceBase> ReferencePose;

	UPROPERTY(EditAnywhere, Category = Config, meta = (ClampMin = "0"))
	float ReferenceTime = 0.0f;

	/** 각자 자기 델타를 받는 척추 체인의 시작·끝 본. 끝 본의 자손은 끝 본의 델타를 받는다. */
	UPROPERTY(EditAnywhere, Category = Config)
	FBoneReference ChainStartBone;

	UPROPERTY(EditAnywhere, Category = Config)
	FBoneReference ChainEndBone;

	/** 델타 비율. 1이면 로코모션의 상체 숙임·기울임을 그대로 옮긴다. 전투 자세는 무게감만 남기는 낮은 값이다. */
	UPROPERTY(EditAnywhere, Category = Config, meta = (PinShownByDefault, ClampMin = "0"))
	float DeltaAlpha = 0.3f;

	/** 델타가 로코모션을 따라가는 스프링 시간(초). 0이면 거르지 않는다. CVar LNP.Anim.UpperBodyDeltaResponse(0 이상)가 덮어쓴다. */
	UPROPERTY(EditAnywhere, Category = Config, meta = (ClampMin = "0", Units = "s"))
	float ResponseTime = 0.08f;

	/** 안정화할 골반 본. 이 본의 자식 중 척추 체인이 아닌 본(다리)은 메시 공간 회전을 유지한다. */
	UPROPERTY(EditAnywhere, Category = Config)
	FBoneReference PelvisBone;

	/** 골반 순간 출렁임(숙임·기울임)을 남기는 비율. 1이면 로코모션 그대로, 0이면 완전히 잡는다.
	 *  CVar LNP.Anim.ArmedPelvisSwingKeep(0 이상)이 덮어쓴다. */
	UPROPERTY(EditAnywhere, Category = Config, meta = (ClampMin = "0", ClampMax = "1"))
	float PelvisSwingKeep = 0.3f;

	/**
	 * 오래 유지되는 델타를 빼는 시간(초). 기준 포즈와 실제 아이들·달리기 자세의 고정 차이가 상체를 기울이고
	 * 총열 이탈로 남지 않게, 거른 델타의 느린 평균을 뺀다 — 착지·피격 같은 순간 움직임만 남는다.
	 * 0이면 빼지 않는다. CVar LNP.Anim.UpperBodyDeltaSettle(0 이상)이 덮어쓴다.
	 */
	UPROPERTY(EditAnywhere, Category = Config, meta = (ClampMin = "0", Units = "s"))
	float SettleTime = 1.5f;

	UPROPERTY(EditAnywhere, Category = Config)
	bool bMeshSpaceRotationBlend = true;

	UPROPERTY(EditAnywhere, Category = Config)
	TEnumAsByte<ECurveBlendOption::Type> CurveBlendOption = ECurveBlendOption::Override;

	FAnimNode_LNPLocomotionDeltaBlend();

	// FAnimNode_Base
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;

	/** 체인 끝 본에 얹은 델타(메시 공간 회전 벡터 X·Y·Z, 라디안)를 내보내는 커브 이름. */
	static const FName ChestDeltaCurveNames[3];

private:
	enum class EDeltaGroup : uint8 { None, Chain, Follow };

	void RebuildCachedBoneData(const FBoneContainer& RequiredBones, const USkeleton* Skeleton);
	/** 오버레이에 델타를 얹고, 체인 끝 본에 얹은 델타(회전 벡터)를 돌려준다. */
	FVector ApplyLocomotionDelta(const FCompactPose& Base, FCompactPose& Overlay, float Alpha, float DeltaTime);
	/** 로코모션 포즈의 골반 순간 출렁임을 줄이고 다리 메시 공간 회전을 되돌린다. */
	void StabilizePelvis(FCompactPose& Base, float DeltaTime);
	void ResetFilter();
	float GetSettleTime() const;

	/** 컴팩트 포즈 인덱스별 덮기 가중치(LayeredBoneBlend와 같은 형식). */
	TArray<FPerBoneBlendWeight> BoneBlendWeights;
	/** 컴팩트 포즈 인덱스별 기준 포즈 컴포넌트 공간 회전. */
	TArray<FQuat> ReferenceComponentRotations;
	/** 컴팩트 포즈 인덱스별 델타 그룹. */
	TArray<EDeltaGroup> DeltaGroups;
	FCompactPoseBoneIndex ChainEndIndex = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex PelvisIndex = FCompactPoseBoneIndex(INDEX_NONE);
	/** 골반의 자식 중 척추 체인이 아닌 본(다리 등). */
	TArray<FCompactPoseBoneIndex> PelvisLegChildren;
	uint16 CachedRequiredBonesSerial = 0;

	/** 골반 숙임·기울임의 느린 평균(오래 유지되는 성분). */
	FVector PelvisSettled = FVector::ZeroVector;
	bool bPelvisSettledValid = false;

	/** 컴팩트 포즈 인덱스별 거른 델타(회전 벡터)와 그 속도. 체인 본만 쓴다. */
	TArray<FVector> FilteredDeltas;
	TArray<FVector> FilteredVelocities;
	/** 컴팩트 포즈 인덱스별 거른 델타의 느린 평균(오래 유지되는 성분). */
	TArray<FVector> SettledDeltas;
	bool bFilterValid = false;
	/** 마지막 평가에서 체인 끝 본에 얹은 델타(디버그 표시용). */
	FVector LastChestDelta = FVector::ZeroVector;
	/** 마지막 평가 뒤 쌓인 업데이트 시간. */
	float PendingDeltaTime = 0.0f;
};
