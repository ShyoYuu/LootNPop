// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "LNPGameplayCueNotify_Reload.generated.h"

/**
 * GameplayCue.LNP.Weapon.Reload — 재장전 동안 무기 메시에 재장전 애니(`ULNPWeaponVisualSet::WeaponReloadAnim`)를 재생한다.
 * ULNPAbility_Reload가 활성화 동안 큐를 걸어 두고 종료·취소 시 뗀다.
 *
 * 큐에 실은 이유는 관전자 가시성이다 — GA는 서버·소유 클라에서만 돌지만 큐는 ASC가 다른 클라이언트에도 복제한다.
 *
 * ⚠️ 재생 배속을 큐 파라미터로 받지 않는다. Mixed 복제 모드에서 비소유 클라이언트는 최소 복제 큐로 받아
 *    파라미터가 도착하지 않는다. 대신 대상 캐릭터의 복제된 무기 정의와 AttackSpeed 어트리뷰트로 직접 계산한다.
 */
UCLASS()
class LOOTNPOP_API ULNPGameplayCueNotify_Reload : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

protected:
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
