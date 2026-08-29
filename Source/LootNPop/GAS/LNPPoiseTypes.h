// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassCommandBuffer.h"
#include "MassEntityManager.h"
#include "GameplayTagContainer.h"
#include "GAS/LNPDamageFormula.h"
#include "Config/LNPSettings.h"
#include "LNPPoiseTypes.generated.h"

/** 경직 단계. */
UENUM()
enum class ELNPStaggerTier : uint8
{
	/** 그로기 해제 — 경직도가 T1 아래로 자연회복했다. 실행 중인 경직 GA를 끝낸다. */
	None,
	/** 그로기 — 경직도가 T1 이상인 동안 계속된다. 고정 시간이 아니라 **게이지 값에 종속된 상태**다. */
	Groggy,
	/** 다운 — T2 도달(또는 그로기 지속 상한 초과). 고정 시간 + 게이지 리셋 + 면역. */
	Down,
};

/**
 * 경직도 상태. 경직 대상이 되는 모든 엔티티(Player·Enemy)에 붙인다.
 *
 * **설계 원칙: 경직도를 줄이는 수단은 매 틱 자연회복 하나뿐이다.**
 * 임계값을 넘었다고 리셋하거나 차감하지 않는다 — 자연회복 속도를 상회하는 화력을 몰아쳐야만
 * 게이지가 유지·상승하고, 손을 놓으면 저절로 풀린다. 이 불변식의 유일한 예외가 **다운**이다
 * (게이지 0 리셋 + 면역 — 스턴락을 끊는 단 하나의 탈출구).
 *
 * ```
 *   ~T1        평시. 감쇠만 진행
 *   T1 이상    그로기 — 공격·이동 불가. 누적은 계속된다 (리셋·면역·차감 없음)
 *              → 자연회복으로 T1 아래로 내려가면 해제
 *              → T2 도달 또는 그로기가 상한을 넘으면 다운
 *   다운       고정 시간 정지 + 게이지 0 + 면역 N초
 * ```
 *
 * T1~T2 구간이 곧 **딜 구간**이다. 적은 넓게(오래 두들길 수 있게), 플레이어는 좁게
 * (불쾌한 무력 구간을 짧게) 잡으므로 임계값은 전역 상수가 아니라 **폰별 값**이다.
 * ⚠️ 저항이 유입량을 나누기 때문에, 전역 상수로 두면 저항이 높은 플레이어 쪽 딜 구간이
 *    오히려 길어져 의도와 정반대가 된다.
 *
 * **서버 전용이며 복제하지 않는다.** 전파되는 것은 결과뿐 — 입력 차단은 GA 활성화 복제가,
 * 몽타주는 GameplayCue가 나른다.
 *
 * 부착 지점:
 *   - Enemy  : ULNPEnemyTrait::BuildTemplate
 *   - Player : DA_PlayerEntityConfig의 MassAssortedFragmentsTrait
 */
USTRUCT()
struct LOOTNPOP_API FLNPPoiseFragment : public FMassFragment
{
	GENERATED_BODY()

	/** 현재 경직도. 다운으로만 리셋된다. */
	float Current = 0.f;

	/**
	 * 경직저항력 미러. 판정 Pass는 워커 스레드라 ASC를 못 보므로 여기로 복사해 둔다
	 * (FLNPEnemyFragment::Defense가 이미 같은 이유로 존재한다).
	 */
	float Resistance = 0.f;

	/** T1 — 이 값 이상이면 그로기. 폰별로 시드된다. */
	float StaggerThreshold = 60.f;

	/** T2 — 이 값에 도달하면 다운. T1과의 간격이 곧 딜 구간의 폭이다. */
	float DownThreshold = 200.f;

	/** 마지막 피격 시각(World 초). 감쇠 유예 판정용. -1 = 아직 안 맞음. */
	double LastHitTime = -1.0;

	/** 다운 직후 경직 면역 잔여 시간. 0보다 크면 누적이 차단된다 — **유일한 스턴락 방지 장치**. */
	float ImmunityTimeRemaining = 0.f;

	/** 그로기가 이어진 시간. 상한을 넘으면 T2 미도달이어도 다운으로 승격한다. */
	float GroggyElapsed = 0.f;

	/** 그로기 상태 여부. 프로세서가 진입·이탈 에지를 잡아 GA를 켜고 끈다. */
	uint8 bIsGroggy : 1 = 0;

	/**
	 * 이번 그로기가 근접 패링으로 유발됐는가 — **연출만 갈라 쓰기 위한 1회성 플래그**다.
	 * 행동(지속 시간·차단·누적 규칙)은 일반 그로기와 완전히 동일하고, 몽타주 밸류 태그만 바뀐다.
	 * 그로기 진입·이탈 어느 쪽이든 소비된다.
	 */
	uint8 bParryBreakPending : 1 = 0;
};

/**
 * 경직 상태 전이를 Actor에 반영한다 (그로기 진입 / 그로기 해제 / 다운).
 *
 * 판정·누적은 워커 스레드에서 끝나고, ASC 접근이 필요한 부분만 게임 스레드인 Run()으로 미룬다
 * (LNPHitDetectionShared.h의 커맨드 4종과 같은 구조).
 */
struct FLNPStaggerCommand : public FMassBatchedCommand
{
	struct FEntry
	{
		FMassEntityHandle Entity;
		ELNPStaggerTier   Tier = ELNPStaggerTier::Groggy;
		/** 몽타주 밸류 태그 오버라이드. 비어 있으면 티어에서 유도한다 (패링 그로기만 따로 쓴다). */
		FGameplayTag      MontageValueTag;
	};

	FLNPStaggerCommand() : FMassBatchedCommand(EMassCommandOperationType::None) {}

	void Add(FMassEntityHandle InEntity, ELNPStaggerTier InTier, FGameplayTag InMontageValueTag = FGameplayTag())
	{
		Entries.Add({ InEntity, InTier, InMontageValueTag });
		bHasWork = true;
	}

	virtual void Run(FMassEntityManager& EntityManager) override;

	virtual void Reset() override { Entries.Reset(); FMassBatchedCommand::Reset(); }
	virtual SIZE_T GetAllocatedSize()     const override { return Entries.GetAllocatedSize(); }
	virtual int32  GetNumOperationsStat() const override { return Entries.Num(); }

private:
	TArray<FEntry> Entries;
};

namespace LNPPoise
{
	/** 경직 단계에 대응하는 GameplayEvent 태그 (GA_Stagger 트리거). None이면 빈 태그. */
	LOOTNPOP_API FGameplayTag GetStaggerEventTag(ELNPStaggerTier Tier);

	/** 경직 단계에 대응하는 몽타주 Chooser 밸류 태그. */
	LOOTNPOP_API FGameplayTag GetStaggerMontageValueTag(ELNPStaggerTier Tier);

	/**
	 * 경직력 한 방을 경직도에 누적한다. 판정 Processor의 서버 구역에서 호출한다.
	 *
	 * **그로기 중에도 계속 쌓인다** — 그게 딜 구간이 성립하는 이유다.
	 * 막는 것은 다운 직후 면역 구간 하나뿐이다.
	 *
	 * 그로기가 길어질수록 유입에 보너스가 붙는다(PoiseGroggyBonusPerSecond). 유입이 시간에 비례해
	 * 무한히 커지고 자연회복은 상수이므로, 계속 때리는 한 T2 도달이 보장된다 —
	 * "T1 바로 위 걸치기"로 무력 상태를 무한정 끄는 빈틈을 시계가 아니라 **타격으로** 막는다.
	 *
	 * @param Poise       피격자 프래그먼트. 아키타입에 없으면(Optional 요구) null이 들어올 수 있다.
	 * @param RawPoise    공격 어빌리티가 선언한 경직력.
	 * @param Now         World->GetTimeSeconds(). 감쇠 유예 기준 시각.
	 * @param Multiplier  가드로 막아냈을 때의 감쇠 비율 등. 피격은 1.0.
	 */
	inline void Accumulate(FLNPPoiseFragment* Poise, float RawPoise, double Now, float Multiplier = 1.f)
	{
		if (!Poise || RawPoise <= 0.f || Poise->ImmunityTimeRemaining > 0.f)
			return;

		float Gain = ApplyResistance(RawPoise, Poise->Resistance) * Multiplier;
		if (Poise->bIsGroggy)
			Gain *= 1.f + GetDefault<ULNPSettings>()->PoiseGroggyBonusPerSecond * Poise->GroggyElapsed;

		Poise->Current    += Gain;
		Poise->LastHitTime = Now;
	}

	/**
	 * 근접 패링 성공 시 공격자의 경직도를 대량으로 올린다.
	 *
	 * 경직저항력을 적용하지 않는다 — 패링은 스텟 대결이 아니라 타이밍 판정이므로 저항으로 희석되면 안 된다.
	 * 양은 **피격자 자신의 T1 배율**이라 폰별 임계값과 무관하게 의도한 결과가 나온다.
	 *
	 * ⚠️ 다운 직후 면역 구간에서는 다른 모든 유입과 마찬가지로 무시된다 — 면역은 예외를 두지 않는다.
	 */
	inline void ApplyParryBreak(FLNPPoiseFragment* Poise, double Now)
	{
		if (!Poise || Poise->ImmunityTimeRemaining > 0.f)
			return;

		Poise->Current           += Poise->StaggerThreshold * GetDefault<ULNPSettings>()->PoiseParryBreakRatio;
		Poise->LastHitTime        = Now;
		Poise->bParryBreakPending = 1;   // 이어질 그로기 진입에서 패링 전용 몽타주로 갈아탄다
	}
}
