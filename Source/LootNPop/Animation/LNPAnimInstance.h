// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "LNPAnimInstance.generated.h"

class ALNPCharacterBase;
class ULNPCharacterMoverComponent;

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
};
