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

	/** LeftHandGrip 소켓 존재 여부. false면 왼손 IK 비활성. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LNP|Combat")
	bool bHasLeftHandGrip = false;
};
