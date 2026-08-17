// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "LNPGameplayEffect_Stat.generated.h"

/**
 * 스탯 변경용 Infinite GE 한 쌍. 두 클래스 모두 LNPStat::GetStatMetaTable()의 모든 스탯에 대한
 * 모디파이어를 갖고, 크기는 스탯별 SetByCaller 태그로 주입받는다.
 * 그래서 스탯×연산 조합마다 GE 에셋을 만들 필요가 없다 — 아이템 DataAsset이 선언만 하면 된다.
 *
 * ⚠️ SetByCaller 태그를 지정하지 않으면 GAS는 에러 로그 후 0을 반환한다. Percent GE에서 0은
 *    스탯을 0으로 만들어버리므로, LNPStat::ApplyModifiers가 항상 모든 스탯 태그를
 *    no-op 값(Flat=0, Percent=1)으로 먼저 채운 뒤 필요한 항목만 덮어쓴다.
 *
 * 수명은 GE 자체가 아니라 적용한 컴포넌트(장비/인벤토리)가 핸들로 관리한다.
 */
UCLASS()
class LOOTNPOP_API ULNPGameplayEffect_StatFlat : public UGameplayEffect
{
	GENERATED_BODY()
public:
	ULNPGameplayEffect_StatFlat();
};

UCLASS()
class LOOTNPOP_API ULNPGameplayEffect_StatPercent : public UGameplayEffect
{
	GENERATED_BODY()
public:
	ULNPGameplayEffect_StatPercent();
};
