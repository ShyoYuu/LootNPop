// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "GAS/Abilities/LNPGameplayAbility.h"
#include "LNPAbility_SpringLaunch.generated.h"

class ALNPSpringLauncher;
class UAnimMontage;

/**
 * 스프링 런처 사용의 발동 스냅샷 — "어느 런처인가".
 *
 * 규약과 함정은 `LNPAttackInputTargetData.h` 주석이 전부 설명한다. 특히
 * ⚠️ **필드는 반드시 UPROPERTY여야 한다** — Iris는 FGameplayAbilityTargetData 파생 타입을
 * 리플렉션으로 직렬화하고 커스텀 NetSerialize를 무시한다.
 *
 * 액터 포인터를 그대로 싣는 것이 성립하는 이유는 런처가 **서버 스폰 복제 액터**라 NetGUID가 있기 때문이다.
 * 비복제 동적 스폰 액터였다면 서버에 null로 도착해 ID 규약을 따로 만들어야 했다.
 */
USTRUCT()
struct LOOTNPOP_API FLNPSpringLaunchTargetData : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ALNPSpringLauncher> Launcher = nullptr;

	virtual UScriptStruct* GetScriptStruct() const override { return FLNPSpringLaunchTargetData::StaticStruct(); }
	virtual FString ToString() const override { return TEXT("FLNPSpringLaunchTargetData"); }
};

/**
 * 스프링 런처 사용 GA — 슬롯 정렬 후 사출. 런처와 무관하게 폰 `DefaultAbilities`로 부여한다(Reload와 같은 방식).
 *
 * **왜 순수 Server RPC가 아니라 GA인가.** 사용자가 지정한 "RPC 경로"와 모순되지 않는다 —
 * 어빌리티 발동은 `ServerTryActivateAbilityWithEventData`라는 RPC이고, 런처 참조는
 * TechDesign_Networking.md §4.8이 "발동 요청에 싣는다"고 규정한 종류의 값(소유 클라만 아는 상호작용 대상)이다.
 * GA를 쓰면 몽타주 복제·중복 발동 차단(태그)·예측 창이 전부 따라온다.
 *
 * **두 단계.** ① 정렬: 슬롯으로 끌어당기는 LayeredMove + 회전 덮어쓰기 → ② 발사: `LaunchWithVelocity`.
 * 두 단계 모두 서버와 소유 클라가 **같은 액터 트랜스폼에서 파생**하므로 값이 갈리지 않는다.
 * 그래서 발사를 서버 전용으로 두지 않는다 — 예측하면 소유 클라의 반응이 즉시가 되고,
 * 권위는 어차피 Mover SyncState 복제가 덮는다 (대시와 같은 구조).
 *
 * ⚠️ 정렬 **회전**은 모션 워핑(`bWarpRotation`)으로 하지 않는다 — 레이어드 무브의 각속도는 이동 모드가
 * 매 프레임 OrientationIntent로 되감아 상쇄한다 (`ULNPInputHandlerComponent::SetOrientationOverride` 주석).
 */
UCLASS()
class LOOTNPOP_API ULNPAbility_SpringLaunch : public ULNPGameplayAbility
{
	GENERATED_BODY()

public:
	ULNPAbility_SpringLaunch();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	/**
	 * 정렬 연출 몽타주. **없어도 동작한다** — 정렬 이동은 코드가 만드는 LayeredMove이고 몽타주는 연출이다.
	 * 지정하면 길이가 AlignDuration을 대체한다 (애니메이터가 타이밍을 갖는다).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Launcher")
	TObjectPtr<UAnimMontage> AlignMontage;

	// 정렬 시간은 ULNPWorldDeviceConfig::AlignDuration이 정한다 (AlignMontage가 있으면 그 길이가 우선).

	/**
	 * 사출에 필요한 최소 경과 비율. 서버의 활성화와 소유 클라의 종료 통지는 같은 편도 지연을 겪으므로
	 * 서버가 보는 경과는 정상적으로 거의 1.0이다 — 지터 여유만 남긴다 (ULNPAbility_Reload와 같은 규약).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "LNP|Launcher", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinCompletionRatio = 0.8f;

private:
	UFUNCTION()
	void OnAlignFinished();

	/** 실제 사출. 서버·소유 클라가 각자 같은 액터 트랜스폼에서 같은 값을 뽑아 부른다. */
	void ExecuteLaunch();

	TWeakObjectPtr<ALNPSpringLauncher> PendingLauncher;

	/** 이번 활성화의 정렬 시간과 시작 시각. EndAbility가 정상 완료 여부를 판정한다. */
	float AlignSeconds = 0.f;
	double ActivationTime = 0.0;
};
