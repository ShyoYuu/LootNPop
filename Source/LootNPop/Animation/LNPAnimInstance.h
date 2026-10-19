// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "LNPAnimInstance.generated.h"

class ALNPCharacterBase;
class ULNPCharacterMoverComponent;
class USkeletalMeshComponent;

/**
 * ULNPAnimInstance
 * LootNPop 프로젝트용 커스텀 AnimInstance 클래스.
 */
UCLASS()
class LOOTNPOP_API ULNPAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	/** 오프셋 그립의 왼손이 그립을 떠나야 하는 정도(0~1). LeftHandIKAlpha 주석 참조. */
	float ComputeLeftHandOffDemand() const;

	/** Owner 캐릭터 참조 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Character")
	TObjectPtr<ALNPCharacterBase> OwningMoverCharacter;

	/** 캐릭터의 MoverComponent 참조 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Character")
	TObjectPtr<ULNPCharacterMoverComponent> MoverComponent;

	/** 캐릭터의 현재 속도 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	FVector Velocity;

	/** 캐릭터의 수평 속력 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	float GroundSpeed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bIsOnGround;

	/** 캐릭터가 이동 애니메이션을 재생해야 하는지 여부 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bShouldMove;

	/** 캐릭터가 현재 낙하 상태인가? */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bIsFalling;

	/** 캐릭터가 현재 공중(지면 비접지 상태)에 있는가? */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bIsAirborne;

	/** 캐릭터 회전 기준 이동 방향 (-180 ~ 180) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	float Direction;

	/** 캐릭터가 스트레이핑 애니메이션을 사용해야 하는가? */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bShouldStrafe;

	/** 캐릭터가 현재 수영 중인가? */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bIsSwimming;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bIsCrouching;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Movement")
	bool bIsSprinting;

	/** 캐릭터가 현재 가드 중인가? Guard Modifier 활성 여부로 판단. ABP 분기에 사용. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Combat")
	bool bIsGuarding;

	/** Aim Offset: 카메라 Yaw - 캐릭터 Yaw 차이 (-180 ~ 180) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|AimOffset")
	float AimYaw = 0.0f;

	/** Aim Offset: 카메라 Pitch (-90 ~ 90) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|AimOffset")
	float AimPitch = 0.0f;

	/** 현재 장착 무기 메시 컴포넌트. 왼손 IK 소켓 위치 쿼리에 사용. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Combat")
	TObjectPtr<USkeletalMeshComponent> WeaponMeshComp;

	/** 무기의 LeftHandGrip 소켓 월드 위치. Two Bone IK 타겟으로 사용. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Combat")
	FVector LeftHandGripLocation = FVector::ZeroVector;

	/** 왼손 그립 목표가 있는가(LeftHandGrip 소켓 또는 표현 세트의 그립 오프셋). false면 왼손 IK 비활성. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Combat")
	bool bHasLeftHandGrip = false;

	/**
	 * 표시 메시(VisualMesh) ABP의 왼손 IK 알파(0~1).
	 * 소켓 그립(롱소드)은 원본 쪽 서브 ABP IK와 같이 항상 1이다.
	 * 오프셋 그립(총기)은 손이 그립을 떠날 수 있는 구간에 내린다 — `AnimSourceMesh`의
	 * `DisableLHandIK` 커브(Lyra 라이플 재장전)와 DefaultSlot·FullBody·UpperBody 몽타주 가중치 중 큰 값만큼.
	 * 사격(FullBodyAdditivePreAim)·피격(AdditiveHitReact)은 가산 슬롯이라 IK를 유지한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Combat")
	float LeftHandIKAlpha = 0.0f;

	/**
	 * 견착 보정(`ULNPWeaponVisualSet::RightHandCorrection`)을 무기 부착 본(hand_r) 공간으로 옮긴 값(cm).
	 * 플레이어 표시 ABP의 오른팔 Two Bone IK가 "현재 hand_r + 이 값"을 목표로 쓴다(Effector = BoneSpace hand_r).
	 * 무기가 손에 강체로 붙어 있어 프레임마다 같은 값이다(이전 프레임 IK 결과가 되먹임되지 않는다).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Combat")
	FVector RightHandIKOffset = FVector::ZeroVector;

	/** 오른팔 견착 보정 IK 알파(0~1). 보정이 있을 때만, 왼손 오프셋 그립과 같은 몽타주·커브 차단을 따른다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Combat")
	float RightHandIKAlpha = 0.0f;
};
