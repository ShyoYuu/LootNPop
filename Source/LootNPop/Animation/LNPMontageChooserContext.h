// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "LNPMontageChooserContext.generated.h"

/**
 * Chooser Table 평가 시 입력 파라미터를 담는 Context Object.
 *
 * Chooser의 GameplayTag 열은 FGameplayTagContainer의 "Has Tag" 조건으로 동작하므로
 * 각 파라미터를 별도 컨테이너로 분리하여 열 바인딩이 명확하게 보이도록 구성.
 *
 * 사용 예:
 *   Character->EvaluateMontage(TAG_Weapon_LongSword,
 *                               TAG_Montage_Situation_HitReaction,
 *                               TAG_Montage_Value_Direction_Front);
 */
UCLASS(Transient, BlueprintType)
class LOOTNPOP_API ULNPMontageChooserContext : public UObject
{
    GENERATED_BODY()

public:
    // 장착 중인 무기 종류 (LNP.Weapon.*)
    UPROPERTY(BlueprintReadWrite, Category = "Chooser", meta = (Categories = "LNP.Weapon"))
    FGameplayTagContainer WeaponType;

    // 몽타주 선택 상황 (LNP.Montage.Situation.*)
    UPROPERTY(BlueprintReadWrite, Category = "Chooser", meta = (Categories = "LNP.Montage.Situation"))
    FGameplayTagContainer SituationType;

    // 상황별 세부 값 (방향, 패링 역할 등, LNP.Montage.Value.*)
    UPROPERTY(BlueprintReadWrite, Category = "Chooser", meta = (Categories = "LNP.Montage.Value"))
    FGameplayTagContainer Value;
};
