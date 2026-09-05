// Copyright (c) 2026 LootNPop. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "LNPEntityAttackProcessor.generated.h"

/**
 * 순수 엔티티(`ELNPEnemyCombatMode::PureEntity`) 기본 공격의 위상 구동.
 *
 * **판단은 StateTree Task가, 진행은 이 프로세서가 맡는다.** Task는 "지금 때리고 싶다"만
 * (`FLNPEntityAttackFragment::bAttackRequested`) 세우고, 쿨다운·위상 전이·중단은 전부 여기서 돈다.
 * Mass StateTree의 Task Tick은 `StateTreeActivate` 신호가 있어야만 돌기 때문에, 위상을 Task에 두면
 * 신호가 끊긴 프레임에 스윙이 중간에 멈춘 채 칼날 엔티티만 살아남는다 (배회 교착과 같은 함정).
 *
 * ⚠️ **경직·사망 시 공격을 끊어 줄 주체가 이 프로세서뿐이다.** Actor 경로에서는
 * `FLNPStaggerCommand::Run`이 `CancelCurrentAttackAbility()`로 끊지만, 그 함수는 Actor가 없으면
 * 도달하기 전에 조기 반환한다.
 *
 * 게임 스레드에서 돈다 — 발사체 스폰이 `FMassEntityManager::GetOrCreateConstSharedFragment`를 타고,
 * 그 경로가 워커 스레드에서 안전하다는 근거가 없다. 공격 위상을 도는 개체 수는 슬롯 한도로 묶여 있고
 * 개체당 작업이 사칙연산 수준이라 부담이 되지 않는다.
 */
UCLASS()
class LOOTNPOP_API ULNPEntityAttackProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEntityAttackProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	/** 적 엔티티 — 위상 진행과 칼날 4점 계산. */
	FMassEntityQuery AttackQuery;

	/**
	 * 살아 있는 가상 칼날 — 계산된 4점을 기록한다.
	 *
	 * 칼날은 적과 **다른 엔티티**라 워커/게임 스레드 어느 쪽에서도 임의 접근하지 않는다.
	 * `ULNPWeaponTraceHitDetectionProcessor`와 같은 2패스(수집 → 반영) 형태로 처리한다.
	 */
	FMassEntityQuery SwingQuery;
};
