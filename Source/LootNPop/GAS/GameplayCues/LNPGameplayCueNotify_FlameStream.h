// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "LNPGameplayCueNotify_FlameStream.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

/**
 * 화염방사기 분사 VFX (`GameplayCue.LNP.Weapon.Flame`). 무기 메시 총구 소켓에 루프 Niagara를 붙인다.
 *
 * 이벤트가 두 경로로 들어오고, **한 머신에서는 그중 하나만 쓴다:**
 *   - GE 경로 (WhileActive / Removed) — `ULNPGameplayEffect_FlameFiring`이 서버에서 걸고 복제된다.
 *     호스트 자신과 모든 관전자 화면이 이것을 쓴다.
 *   - 로컬 펄스 (Executed) — 게스트 소유자 화면 전용. 서버 GE는 예측이 안 되어(스택 GE) 소유자에게 RTT만큼
 *     늦게 켜지고 늦게 꺼지므로, `ULNPAbility_FlameAttack`이 틱마다 이 머신에서만 펄스를 쏜다.
 *     펄스가 끊긴 뒤 `RawMagnitude`(= GE 지속시간)만큼 유지하고 끈다.
 * 대상이 "로컬 컨트롤 + 비권위"면 GE 이벤트를 무시한다 — 두 경로가 섞이지 않으니 중복 재생·조기 소거를 따질 필요가 없다.
 *
 * 수명은 직접 관리한다(`bAutoDestroyOnRemove = false`). 엔진은 그 플래그가 켜진 큐 액터를 활성 GE 없는
 * Executed 직후 바로 파괴하므로(`GameplayCueSet.cpp`), 펄스 경로가 성립하지 않는다.
 */
UCLASS(Abstract)
class LOOTNPOP_API ALNPGameplayCueNotify_FlameStream : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	ALNPGameplayCueNotify_FlameStream();

	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

	virtual bool Recycle() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** 루프 분사 시스템. 로컬 +X가 분사 방향이어야 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|VFX")
	TObjectPtr<UNiagaraSystem> StreamVFX;

	/** 무기 메시 소켓. 없으면 무기 메시 원점에 붙는다. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|VFX")
	FName AttachSocket = TEXT("Muzzle");

	/** 소켓 축이 총열 방향과 다를 때의 보정 회전. */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|VFX")
	FRotator RelativeRotation = FRotator::ZeroRotator;

private:
	/** 게스트 소유자 화면인가 — 로컬 펄스만 쓰고 GE 이벤트는 무시한다. */
	static bool IsPredictingOwner(const AActor* Target);

	void StartStream(AActor* Target);
	void StopStream();
	void OnLocalHoldExpired();

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> StreamComponent;

	FTimerHandle LocalHoldTimer;
};
