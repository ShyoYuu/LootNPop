// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/GameplayCues/LNPGameplayCueNotify_VFXSound.h"
#include "LNPGameplayCueNotify_JustDodgeSuccess.generated.h"

/**
 * 저스트 회피 성공 큐(`GameplayCue.LNP.JustDodge.Success`) — 서버 FLNPJustDodgeCommand가 실행하고 ASC를 타고 모든 머신에 도착한다.
 *
 * 지금은 VFXSound 동작에 도착 로그만 더한다. 소유 화면의 연출 지연을 재기 위해서다(.context/JustDodge/phases/Phase03_TuningLatency.md 단위 1).
 * 비주얼 트랙 V1(순보 — 메시 숨김)이 이 클래스를 확장한다.
 */
UCLASS()
class LOOTNPOP_API ULNPGameplayCueNotify_JustDodgeSuccess : public ULNPGameplayCueNotify_VFXSound
{
	GENERATED_BODY()

protected:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
