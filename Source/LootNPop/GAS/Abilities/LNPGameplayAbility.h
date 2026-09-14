// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "LNPGameplayAbility.generated.h"

class ALNPCharacterBase;
class ALNPPlayerState;

UCLASS(Abstract, BlueprintType, Blueprintable)
class LOOTNPOP_API ULNPGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
protected:
	/** 아바타 Pawn이 LNP 캐릭터이면 반환한다. */
	UFUNCTION(BlueprintPure, Category = "LNP|Ability")
	ALNPCharacterBase* GetOwningCharacter() const;

	/** 이 Ability를 활성화하는 ASC를 소유한 LNP PlayerState를 반환한다. */
	UFUNCTION(BlueprintPure, Category = "LNP|Ability")
	ALNPPlayerState* GetOwningLNPPlayerState() const;

	/**
	 * AttackSpeed 어트리뷰트(버프 합산 후 최종값). ASC가 없으면 1.0.
	 * 몽타주 재생 속도·쿨다운·재장전 시간에 모두 쓰인다 — 같은 계수로 스케일해야
	 * 실제 공격 빈도가 계수만큼 빨라진다 (한쪽만 줄이면 다른 쪽이 병목이 된다).
	 */
	float GetAttackSpeed() const;
};
