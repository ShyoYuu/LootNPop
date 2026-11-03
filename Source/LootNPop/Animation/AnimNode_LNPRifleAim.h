// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_LNPRifleAim.generated.h"

class ULNPMassWorldCollisionSubsystem;

/** 게임 스레드에서 복사한 조준 캐시와 무기 부착 기하. 포즈는 노드가 이번 평가에서 읽는다(ULNPAnimInstance가 공급). */
USTRUCT(BlueprintType)
struct FLNPRifleAimInputs
{
	GENERATED_BODY()

	/** 0이면 끈다. 왼손 오프셋 그립과 같은 몽타주·커브 차단을 따른다(장전·대시 중 0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle Aim")
	float Alpha = 0.0f;

	/** 실탄과 같은 크로스헤어 조준점 캐시(ULNPInputHandlerComponent::GetCrosshairAimPoint). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle Aim")
	FVector AimTarget = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle Aim")
	FVector ViewDirection = FVector::ForwardVector;

	/** 무기가 붙은 표시 메시 본(부착 소켓의 부모 본). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle Aim")
	FName AnchorBone = NAME_None;

	/** Muzzle 소켓의 앵커 본 공간 변환. 무기가 앵커에 강체로 붙어 있어 지난 프레임 포즈로 구해도 정확하다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle Aim")
	FTransform MuzzleInAnchor = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle Aim")
	FVector MuzzleOffsetWorld = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle Aim")
	FVector ActorLocation = FVector::ZeroVector;

	/** 수명은 입력을 공급하는 애님 인스턴스가 보유한다. 워커는 모든 스레드용 동기 질의 API만 호출한다. */
	UPROPERTY(Transient)
	TObjectPtr<ULNPMassWorldCollisionSubsystem> WorldCollision;
};

/**
 * 라이플 총열 정밀 보정(AN-025) — 피벗 본(spine_03)만 돌려 총열을 실제 발사 방향과 나란히 맞춘다.
 * 표시 ABP에서 오른팔 견착 IK 뒤·왼손 IK 앞에 둔다. 무기가 붙은 메시의 포즈를 이번 평가에서 읽으므로
 * 해가 정확하고, 이전 프레임 보정을 누적하지 않는다. 해는 UAF 시험 trait와 같은 FLNPRifleAimSolver다.
 */
USTRUCT(BlueprintInternalUseOnly)
struct LOOTNPOP_API FAnimNode_LNPRifleAim : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	FAnimNode_LNPRifleAim();

	/** 회전시킬 상체 본. 무기 앵커 본의 조상이어야 한다. */
	UPROPERTY(EditAnywhere, Category = "Rifle Aim")
	FBoneReference PivotBone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rifle Aim", meta = (PinShownByDefault))
	FLNPRifleAimInputs Inputs;

	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

private:
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
};
