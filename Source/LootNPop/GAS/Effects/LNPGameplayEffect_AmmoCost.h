// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "LNPGameplayEffect_AmmoCost.generated.h"

/**
 * 원거리 기본 공격 1회 발사의 탄약 비용 — MagazineAmmo를 1 깎는 Instant GE.
 * 산탄도 발사 1회 = 1발이다.
 *
 * 어빌리티 Cost 경로(ApplyCost)로 적용되므로 GAS가 예측 키로 소유 클라에 선반영하고
 * 서버 확정 시 정산한다 — 별도 복제 변수로 짜면 늦게 도착한 서버 값이 연사 중 로컬 차감을 덮어쓴다.
 */
UCLASS()
class LOOTNPOP_API ULNPGameplayEffect_AmmoCost : public UGameplayEffect
{
	GENERATED_BODY()
public:
	ULNPGameplayEffect_AmmoCost();
};
