// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "AttributeSet.h"                // FGameplayAttribute (값 타입으로 사용)
#include "GameplayTagContainer.h"
#include "ActiveGameplayEffectHandle.h"
#include "LNPStatModifier.generated.h"

class UAbilitySystemComponent;

/**
 * 스탯 모디파이어의 연산 종류.
 *
 * GAS 어그리게이터 평가식이
 *   ((Base + AddBase) * MultiplyAdditive / DivideAdditive * MultiplyCompound) + AddFinal
 * 이고, MultiplyAdditive는 Bias 1.0으로 합산(`SumMods`)되므로 배율들이 서로 더해진 뒤 한 번만 곱해진다.
 * 따라서 이 두 채널만 쓰면 기획 공식이 그대로 성립한다:
 *   최종 = (캐릭터 기초 + 무기 스텟 + Flat) × (1 + Σ Percent)
 *
 * ⚠️ DivideAdditive / MultiplyCompound / AddFinal / Override는 사용 금지 —
 *    쓰는 순간 스탯 UI의 `C = A × B` 분해가 깨진다.
 */
UENUM(BlueprintType)
enum class ELNPStatModOp : uint8
{
	/** 기초 스탯에 더한다 (곱연산 버프에 증폭되므로 고효율·레어). → EGameplayModOp::AddBase */
	Flat    UMETA(DisplayName = "Flat (Add Base)"),

	/** 배율끼리 합산된 뒤 한 번 곱해진다 (중복 획득 시 체감 효율 저하). → EGameplayModOp::MultiplyAdditive */
	Percent UMETA(DisplayName = "Percent (Multiply Additive)")
};

/** 아이템(무기·버프·스킬)이 선언하는 스탯 변경 한 줄. */
USTRUCT(BlueprintType)
struct LOOTNPOP_API FLNPStatModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LNP|Stats")
	FGameplayAttribute Attribute;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LNP|Stats")
	ELNPStatModOp Op = ELNPStatModOp::Percent;

	/**
	 * Flat: 기초값에 더할 절대량. 배율 스탯(이동·공격·루팅 속도)은 기초가 1.0이므로 0.1 = "기초 +10%".
	 * Percent: 0.4 = "+40%".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LNP|Stats")
	float Magnitude = 0.f;
};

/** 스탯 값의 표기 방식. */
enum class ELNPStatDisplay : uint8
{
	Integer,  // 소수점 없음 (MaxHealth)
	Scalar,   // 소수점 1자리 (AttackPower, DefensePower)
	Ratio,    // 퍼센트 (AttackSpeed, MoveSpeed, LootSpeed — 기초 1.0)
};

/** 스탯 하나의 메타데이터. 어트리뷰트 목록·SetByCaller 태그·UI 표기를 한 곳에서 관리한다. */
struct FLNPStatMeta
{
	FGameplayAttribute Attribute;
	FGameplayTag       SetByCallerTag;
	FText              DisplayName;
	ELNPStatDisplay    Display = ELNPStatDisplay::Scalar;
};

namespace LNPStat
{
	/**
	 * 버프·무기가 다룰 수 있는 스탯 전체 목록. 스탯을 추가하려면 여기 한 곳만 고치면
	 * GE 모디파이어·스탯 탭 행·구독 목록이 모두 따라온다.
	 */
	LOOTNPOP_API TConstArrayView<FLNPStatMeta> GetStatMetaTable();

	LOOTNPOP_API const FLNPStatMeta* FindStatMeta(const FGameplayAttribute& Attribute);

	/** 스탯 값 한 개를 표기 방식에 맞춰 문자열로 만든다 (Ratio는 "154%"). */
	LOOTNPOP_API FString FormatStatValue(float Value, ELNPStatDisplay Display);

	/** 아이템 설명용 한 줄: "Base Attack +10" / "Attack +40%" / "Base Move Speed +10%". */
	LOOTNPOP_API FText MakeModifierText(const FLNPStatModifier& Modifier);

	/**
	 * 선언된 모디파이어들을 GE로 적용하고 핸들을 OutHandles에 덧붙인다.
	 * Flat·Percent 각각 최대 1개의 Infinite GE만 만들며, 해당 Op의 항목이 없으면 그 GE는 만들지 않는다.
	 * 수명 관리(해제)는 호출자가 핸들로 한다.
	 */
	LOOTNPOP_API void ApplyModifiers(UAbilitySystemComponent& ASC,
		TConstArrayView<FLNPStatModifier> Modifiers,
		TArray<FActiveGameplayEffectHandle>& OutHandles);
}
