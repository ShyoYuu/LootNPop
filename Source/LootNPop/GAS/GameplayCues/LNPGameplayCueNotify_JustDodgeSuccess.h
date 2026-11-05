// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/GameplayCues/LNPGameplayCueNotify_VFXSound.h"
#include "LNPGameplayCueNotify_JustDodgeSuccess.generated.h"

/**
 * 저스트 회피 성공 큐(`GameplayCue.LNP.JustDodge.Success`) — 서버 FLNPJustDodgeCommand가 실행하고 ASC를 타고 모든 머신에 도착한다.
 *
 * 캐릭터의 순보(메시·무기·그림자 숨김과 복원)를 시작한다. 타이머와 실행별 상태는 캐릭터가 소유한다.
 * 도착 로그는 소유 화면의 연출 지연 측정에 사용한다(.context/JustDodge/phases/Phase03_TuningLatency.md 단위 1).
 */
UCLASS()
class LOOTNPOP_API ULNPGameplayCueNotify_JustDodgeSuccess : public ULNPGameplayCueNotify_VFXSound
{
	GENERATED_BODY()

protected:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

	/** 큐 수신 시의 표시 위치에서 재생할 로컬 스페이스 버스트. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|VFX")
	TObjectPtr<UNiagaraSystem> DisappearVFX;

	/** 0.12초 뒤 복원 위치에서 재생할 버스트. 대시 축 속도선과 흰색 중심 강조를 포함한다. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|VFX")
	TObjectPtr<UNiagaraSystem> ReappearVFX;
};
