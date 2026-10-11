// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "NativeGameplayTags.h"
#include "LNPGameplayEffect_MagazineRegen.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_GE_Data_MagazineRegen)

/**
 * 충전형 탄창(ULNPWeaponData::bRegeneratingMagazine) 회복 — 주기마다 MagazineAmmo 기초값을 더하는 Infinite GE.
 * 주기당 회복량은 TAG_GE_Data_MagazineRegen SetByCaller(= MagazineRegenPerSecond × Period)로 받는다.
 *
 * 분사 중(LNP.State.FlameFiring)에는 억제되어 주기 실행을 건너뛴다. 상한은 어트리뷰트 기초값 클램프가 자른다.
 * 서버가 장착 시 적용하고 해제 시 제거한다 (ULNPEquipmentComponent). 결과는 어트리뷰트 복제로 소유 클라에 간다.
 */
UCLASS()
class LOOTNPOP_API ULNPGameplayEffect_MagazineRegen : public UGameplayEffect
{
	GENERATED_BODY()
public:
	ULNPGameplayEffect_MagazineRegen();

	/** 주기 (초). 회복량 SetByCaller는 이 값을 곱해 넣어야 초당 회복량이 된다. */
	static constexpr float RegenPeriod = 0.1f;
};
