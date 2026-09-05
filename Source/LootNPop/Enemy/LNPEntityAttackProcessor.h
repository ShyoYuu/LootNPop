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

/**
 * 게스트 전용 — 복제된 행동 상태에서 **관전용 발사체 Ghost**를 만든다 (트랙 B의 §5.4).
 *
 * 새 RPC를 쓰지 않는 것이 요점이다. 발사마다 Multicast를 쏘면 `RPC 수 = 발사 수 x 개체 수`가 되어
 * "순수 엔티티는 다수"라는 전제와 충돌한다. 필요한 인자는 전부 이미 게스트에 있다 —
 * 무기 상수는 `FLNPEnemySharedFragment`의 Config에서, 발사 위치·방향은 복제된 위치·Yaw에서 나온다.
 *
 * ⚠️ **버블 핸들러 안에서 직접 스폰하지 않는다.** 수신 콜백은 Mass 실행 컨텍스트 밖이고,
 * "같은 프래그먼트를 읽는 단일 소비 경로"라는 이 채널의 규약과도 어긋난다.
 *
 * **감수하는 오차 (전부 코스메틱 — 임팩트 지점은 서버 큐가 확정한다):**
 * - 발사 지점이 미세하게 어긋난다(게스트는 보간값, 서버는 실측값).
 *   `ALNPCharacterBase::Multicast_SpawnGhostProjectiles`가 이미 감수하는 것과 같은 종류다.
 * - 수평 방향은 게스트가 보간한 몸 방향에서 나오므로 서버 실측값과 미세하게 다르다.
 *   **상하각은 서버가 발사 순간 확정한 값을 그대로 받는다** — 고저차 교전에서 "게스트에선 피했는데
 *   서버에선 맞는" 상태가 나와 2026-09-05에 넣었다.
 * - 발사 시각을 싣지 않으므로 Dead Reckoning의 업스트림 지연은 0이다(수신자 RTT/2만 적용된다).
 * - 선딜 전이와 발사 전이가 한 갱신에 뭉쳐 도착하면 그 발사의 Ghost를 건너뛴다(멀리 있는 개체).
 *   없는 탄이 생기는 것보다 안 보이는 편이 안전하다.
 */
UCLASS()
class LOOTNPOP_API ULNPEntityGhostProjectileProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	ULNPEntityGhostProjectileProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery GhostQuery;
};
