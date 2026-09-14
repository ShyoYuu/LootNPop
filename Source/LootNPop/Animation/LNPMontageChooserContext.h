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
 *   Character->EvaluateMontage(TAG_VisualSet_LongSword,
 *                               TAG_Montage_Situation_HitReaction,
 *                               TAG_Montage_Value_Direction_Front);
 */
UCLASS(Transient, BlueprintType)
class LOOTNPOP_API ULNPMontageChooserContext : public UObject
{
    GENERATED_BODY()

public:
    // 장착 무기의 표현 세트 (LNP.VisualSet.*). 무기 종류가 아니다 — 표현을 공유하는 무기는 같은 값이다.
    // ⚠️ 프로퍼티 이름은 CHT_Montage 컬럼 바인딩이 참조하므로 바꾸지 않는다.
    UPROPERTY(BlueprintReadWrite, Category = "Chooser", meta = (Categories = "LNP.VisualSet"))
    FGameplayTagContainer WeaponType;

    // 몽타주 선택 상황 (LNP.Montage.Situation.*)
    UPROPERTY(BlueprintReadWrite, Category = "Chooser", meta = (Categories = "LNP.Montage.Situation"))
    FGameplayTagContainer SituationType;

    // 상황별 세부 값 (방향, 패링 역할 등, LNP.Montage.Value.*)
    UPROPERTY(BlueprintReadWrite, Category = "Chooser", meta = (Categories = "LNP.Montage.Value"))
    FGameplayTagContainer Value;
};
