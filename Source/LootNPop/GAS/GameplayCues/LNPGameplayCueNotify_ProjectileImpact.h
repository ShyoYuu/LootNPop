// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "LNPGameplayCueNotify_ProjectileImpact.generated.h"

/**
 * GameplayCue.LNP.Projectile.Impact 발동 시 Ghost Projectile 재조정을 수행한다 (섹션 5.2).
 * 로컬 공격자면 Ghost를 찾아(브랜치 B) VFX+HitStop을 재생하거나, 이미 브랜치 A에서 처리됐으면 no-op.
 * 비공격자(시뮬레이티드 프록시)는 항상 서버 확정 위치에 VFX만 재생한다.
 * 블루프린트 그래프 없이 순수 C++로 동작 — 블루프린트 애셋은 부모 클래스·GameplayCueTag만 지정.
 */
UCLASS()
class LOOTNPOP_API ULNPGameplayCueNotify_ProjectileImpact : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

protected:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
