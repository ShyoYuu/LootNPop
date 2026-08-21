// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GAS/LNPStatModifier.h"
#include "LNPWeaponLevelRow.generated.h"

/**
 * 무기 레벨 한 단계의 데이터. ULNPWeaponData::LevelTable의 행 구조다.
 *
 * **행 이름이 곧 레벨**이다 ("1", "2", ... "10"). UDataTable::GetRowMap()은 TMap이라
 * 저작 순서가 보존되지 않으므로 인덱스가 아니라 이름으로 조회한다.
 * "1"부터 연속으로 존재하는 마지막 행이 그 무기의 최대 레벨이 된다.
 *
 * 값은 배율이 아니라 **절대값**이다 — 레벨마다 손으로 미세 조정할 수 있어야 하고,
 * 높은 레벨에서만 새 스탯(예: 방어력)이 붙는 설계도 가능해야 하기 때문이다.
 */
USTRUCT(BlueprintType)
struct LOOTNPOP_API FLNPWeaponLevelRow : public FTableRowBase
{
	GENERATED_BODY()

	/**
	 * 이 레벨에서 무기가 주는 스탯 전체(절대값). 장착 중 GE로 어트리뷰트에 적용된다.
	 * 테이블을 지정한 무기는 ULNPItemDefinitionBase::StatModifiers 대신 이 목록을 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LNP|Weapon Level")
	TArray<FLNPStatModifier> StatModifiers;

	/**
	 * 무기 내장 어빌리티의 피해 계수 배율.
	 * 최종 피해 = AttackPower × (어빌리티 CDO의 BaseDamageCoefficient × 이 값).
	 *
	 * ⚠️ 위 StatModifiers의 AttackPower도 레벨과 함께 오르므로 최종 피해는 두 축이 곱해진다.
	 *    이 값은 완만하게 올릴 것 (기획 의도 — GameDesign_Ability.md 참조).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LNP|Weapon Level", meta = (ClampMin = "0"))
	float AbilityCoefScale = 1.0f;
};
