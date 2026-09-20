// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayTagContainer.h"
#include "LNPGameplayCueNotify_VFXSound.generated.h"

class UNiagaraSystem;
class USoundBase;
class UCameraShakeBase;

/**
 * 파티클·사운드(선택적으로 카메라 쉐이크·HitStop)만 재생하는 코스메틱 전용 GameplayCueNotify 공용 베이스.
 * Guard.Block, Parry.Success, Melee.Impact 등 캐릭터 로직 호출이 필요 없는 큐가 상속해서
 * 블루프린트 그래프 없이 Class Defaults에서 에셋만 지정하면 된다.
 * CameraShake는 MyTarget이 로컬 컨트롤 중인 Pawn일 때만(자기 자신의 화면에서만) 재생된다.
 */
UCLASS()
class LOOTNPOP_API ULNPGameplayCueNotify_VFXSound : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

protected:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

	UPROPERTY(EditDefaultsOnly, Category = "LNP|VFX")
	TObjectPtr<UNiagaraSystem> VFX;

	UPROPERTY(EditDefaultsOnly, Category = "LNP|VFX")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditDefaultsOnly, Category = "LNP|VFX")
	TSubclassOf<UCameraShakeBase> CameraShake;

	/**
	 * 0보다 크면 큐 대상이 `ALNPCharacterBase`일 때 이 시간 동안 HitStop을 건다.
	 *
	 * 가드 성공처럼 **연출 외에는 아무 반응도 남지 않는 큐**에 타격감을 주기 위한 것이다 —
	 * 피격 경로는 `ULNPGameplayCueNotify_HitReact`가 몽타주와 함께 이미 걸고 있고,
	 * 가드는 임팩트 큐·데미지 GE·넉백을 전부 건너뛰므로 여기서 걸지 않으면 남는 것이 없다.
	 *
	 * ⚠️ 큐 자체가 전 머신에 전파되므로 **여기서 걸면 모두의 화면에서 걸린다** — 서버 커맨드에서
	 * 직접 부르면 호스트에서만 보인다(`FLNPGuardBlockCommand::Run`은 서버 전용이다).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Feel", meta = (ClampMin = "0.0"))
	float HitStopDuration = 0.f;

	/**
	 * 유효하면 큐 대상이 `ALNPCharacterBase`일 때 이 상황 태그로 Chooser 몽타주를 재생한다.
	 *
	 * ⚠️ **리액션 몽타주를 서버 커맨드에서 직접 재생하면 게스트에 가지 않는다.** 판정 커맨드의
	 * `Run`은 서버에서만 돌고 `Montage_Play`는 로컬 호출이라, 호스트 화면에서만 보인다 —
	 * 2026-09-20에 "게스트에서 방어자 패링 모션이 안 나온다"로 확인된 경로다.
	 * 큐는 피격자 ASC를 타고 전 머신에 전파되므로 여기가 유일하게 맞는 자리다
	 * (`GameplayCue.LNP.Character.Stagger`가 같은 이유로 만들어졌다).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Feel")
	FGameplayTag MontageSituation;

	/** Chooser의 세부 값 태그. 비워 두면 상황 태그만으로 평가한다. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Feel")
	FGameplayTag MontageValue;
};
