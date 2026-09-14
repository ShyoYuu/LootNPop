// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Abilities/LNPGameplayAbility.h"
#include "LNPAbility_Reload.generated.h"

/**
 * 장착 무기 탄창을 가득 채우는 재장전 GA. 무기와 무관하므로 폰 `DefaultAbilities`로 부여한다(Stagger와 같은 방식).
 *
 * - 시간: `ULNPWeaponData::GetReloadDuration(AttackSpeed)` — 공격속도 버프가 재장전도 빠르게 한다.
 * - 연출: 캐릭터 몽타주(Chooser `LNP.Montage.Situation.Reload`)는 이 GA가 재생하고,
 *   무기 메시 애니는 `GameplayCue.LNP.Weapon.Reload`에 싣는다 — GA는 서버·소유 클라에서만 돌아
 *   큐가 아니면 다른 게스트 화면에 무기 애니가 보이지 않는다.
 * - 취소: 경직(`ULNPAbility_Stagger::CancelAbilitiesWithTag`), 무기 교체(`ULNPEquipmentComponent`), 대시(여기서 구독).
 *   취소되면 탄은 채워지지 않는다.
 *
 * **탄을 채우는 곳은 EndAbility다** (취소가 아닌 종료일 때만). 서버는 자기 타이머와 소유 클라의 종료 통지 중
 * 먼저 온 쪽으로 끝나는데, 클라 통지가 먼저 오면 서버 타이머 콜백은 영영 불리지 않는다 — 타이머 콜백에서
 * 채우면 그 경로에서 탄이 사라진다. 경과 시간 하한(`MinCompletionRatio`)을 두어 조기 종료 통지로 재장전을
 * 건너뛰지 못하게 한다.
 */
UCLASS()
class LOOTNPOP_API ULNPAbility_Reload : public ULNPGameplayAbility
{
	GENERATED_BODY()

public:
	ULNPAbility_Reload();

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	/**
	 * 종료 시 탄을 채우는 데 필요한 최소 경과 비율. 서버의 활성화와 소유 클라의 종료 통지는 같은 편도 지연을
	 * 겪으므로 서버가 보는 경과는 정상적으로 거의 1.0이다 — 지터 여유만 남긴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Reload", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinCompletionRatio = 0.8f;

private:
	UFUNCTION()
	void OnReloadFinished();

	void OnDashExecuted();

	/** 이번 활성화의 재장전 시간과 시작 시각. EndAbility가 정상 완료 여부를 판정한다. */
	float ReloadDuration = 0.f;
	double ActivationTime = 0.0;

	bool bCueAdded = false;

	TWeakObjectPtr<class ULNPCharacterMoverComponent> BoundMover;
	FDelegateHandle DashExecutedHandle;
};
