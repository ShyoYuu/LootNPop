// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "LNPGameplayEffect_Burn.generated.h"

/**
 * 플레이어 화상 DoT — 다른 팀 플레이어의 화염 원뿔에 맞은 플레이어에게만 건다 (Player/LNPTeam.h).
 * 적 화상은 이 GE가 아니라 FLNPBurnFragment가 원본이다 (적 ASC GE는 풀 반납 때 소실·누수).
 *
 * 규칙은 적 화상과 같다 (GameDesign_Ability.md 화상 절)
 *   - 적중마다 1중첩, 상한 MaxStacks. 지속시간은 모든 중첩이 공유하고 걸릴 때마다 처음으로 돌아간다.
 *   - 틱 피해 = 중첩 수 × 중첩당 피해. 주기 실행이 Additive 모디파이어에 중첩 수를 곱한다
 *     (bFactorInStackCount 기본 true, GameplayEffectTypes.cpp ComputeStackedModifierMagnitude).
 *   - 처음 걸릴 때만 주기 위상이 0이고 갱신은 위상을 건드리지 않는다 — StackPeriodResetPolicy가 기본값(적용마다 리셋)이면
 *     0.1초마다 갱신되는 동안 틱이 한 번도 터지지 않는다.
 *
 * 피해는 IncomingDamage 메타 어트리뷰트로 들어가 방어력이 적용된다 (PostGameplayEffectExecute).
 * 지속시간·주기는 적용 커맨드가 스펙에 어빌리티 값을 싣는다. 중첩 상한은 스펙에 실을 수 없어 CDO 값(MaxStacks)이다.
 */
UCLASS()
class LOOTNPOP_API ULNPGameplayEffect_Burn : public UGameplayEffect
{
	GENERATED_BODY()
public:
	ULNPGameplayEffect_Burn();

	/** 어빌리티 기본값(ULNPAbility_FlameAttack::MaxBurnStacks)과 맞춰 둔다. */
	static constexpr int32 MaxStacks = 5;
};
