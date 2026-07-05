// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "LNPGameplayCueNotify_VFXSound.generated.h"

class UNiagaraSystem;
class USoundBase;
class UCameraShakeBase;

/**
 * 파티클·사운드(선택적으로 카메라 쉐이크)만 재생하는 코스메틱 전용 GameplayCueNotify 공용 베이스.
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
};
