// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "LNPGameplayEffect_JustDodgeBuff.generated.h"

/**
 * 저스트 회피 성공 버프 — AttackSpeed·MoveSpeed 곱연산 (.context/JustDodge/design/Architecture.md §5.2).
 *
 *   - 중첩되지 않고 시간만 갱신한다: 상한 1중첩 + 적용마다 지속 리셋.
 *   - 배율과 지속은 적용 커맨드(FLNPJustDodgeCommand)가 ULNPSettings 값을 스펙에 싣는다 — Phase 3 튜닝을 재컴파일 없이 하기 위해서다.
 *   - 버프 아이템 GE(Infinite + 인벤토리 타이머)와 달리 아이템이 아니므로 GE 기간으로 끝난다.
 */
UCLASS()
class LOOTNPOP_API ULNPGameplayEffect_JustDodgeBuff : public UGameplayEffect
{
	GENERATED_BODY()
public:
	ULNPGameplayEffect_JustDodgeBuff();
};
