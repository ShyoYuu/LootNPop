# 엘리트 NPC 작업 안내

> 상태: 활성
> 역할: 이 작업의 진입점과 문서 라우팅
> 마지막 구조 갱신: 2026-11-05

## 1. 목적

이 폴더는 **LootPod에 귀속되지 않는 ActorPromoted 적(기획 용어로 "엘리트")**을 만드는 작업을 여러 세션에 걸쳐 관리한다.

- **Pod 적은 전부 PureEntity로 바꾼다.** Pod 편성(`AssociatedEnemies`)에 ActorPromoted가 들어가면 데이터 검증 오류가 난다(EL-003).
- **엘리트는 두 가지 귀속 방식으로 존재한다.**
  - **배회(Roam):** 월드 전체를 돌아다닌다. 고정 앵커가 없다.
  - **아레나(Arena):** 각 옥탄트에서 보스 무대로 설계한 랜드마크 안에서 대기한다. 아레나 밖으로 나가지 않는다.
- **전투 두뇌는 기존 적과 따로 둔다.** 매크로 판단은 엘리트 전용 Mass StateTree가, 전투 패턴은 Actor에 붙은 StateTreeComponent가 맡는다(EL-001).
- **엘리트가 죽으면 LootDice를 드랍한다.** 세션당 한 번만 등장한다(EL-006).
- **스폰 구성은 `DA_MassSpawnConfig` 안에서 Pod 구성과 따로 입력한다**(`UnboundEnemies`).

범위 밖:

- 비행·벽보행 엘리트. ActorPromoted Actor의 Mover는 지상 이동만 한다(`ValidateTemplate` 경고).
- 엘리트 재등장, 난이도 스케일링, 루팅 방해 어그로.
- 패링 불가 공격 같은 패턴 속성 확장(→ [Idea_Backlog.md](../Idea_Backlog.md)에 후보로 둔다).
- 엘리트 전용 HUD(보스 HP 바). 기존 적 HP 바로 시작한다.

## 2. 세션 시작 시 필수 문서

1. `README.md`
2. `Current.md`
3. `Decisions.md`

이 세 문서를 읽은 뒤에는 아래 라우팅 표에서 지금 작업에 필요한 문서만 더 읽는다.

## 3. 문서 라우팅

| 작업 | 추가로 읽을 문서 |
|:---|:---|
| 단계 조정·Phase 전환 | `Roadmap.md` |
| 현재 Phase 구현 | `Current.md`가 가리키는 `phases/PhaseNN_*.md` |
| 스폰 구성·배치 계획·귀속 프래그먼트·Pod 경로 제외·세력권·두뇌 구조·드랍 | `design/Architecture.md` |
| 아레나 이탈 리셋·재등장·드랍 등 기획 규칙 | `design/GameRules.md` |
| 검증 시나리오·PIE 절차·로그 판정 | `design/Validation.md` |
| 기존 적 스폰·CombatMode 분기·표현 소유권 | [TechDesign_EnemyNPC.md](../TechDesign_EnemyNPC.md) §1, §7.1, §7.2, §7.10 |
| 기존 Mass StateTree 구조·신호 구동 Tick | [TechDesign_EnemyNPC_StateTree.md](../TechDesign_EnemyNPC_StateTree.md) |
| 등급 어휘 규약·Promoted 슬롯 대역폭 예산 | [TechDesign_EnemyNPC_LowLOD.md](../TechDesign_EnemyNPC_LowLOD.md) §2, §7 |
| Spawn 스트림 베이크·마커 배치 | [Guide_OctantLevelInstance.md](../Guide_OctantLevelInstance.md), [Guide_OctantLevelDesign.md](../Guide_OctantLevelDesign.md) §4.2 |
| LootDice 스폰·보상 테이블 | [TechDesign_LootDice.md](../TechDesign_LootDice.md) §1, §3.1 |
| 경직·그로기, 패링 판정 | [TechDesign_Poise.md](../TechDesign_Poise.md), [TechDesign_ParrySystem.md](../TechDesign_ParrySystem.md) |
| Mass StateTree 엔진 동작 | [EngineAnalysis_MassStateTree.md](../EngineAnalysis_MassStateTree.md) |

## 4. 아키텍처 한 장 요약

`ULNPMassSpawnConfig::UnboundEnemies`
→ Pod 배치가 끝난 뒤 별도 시드 스트림으로 `BuildUnboundPlan`이 배치를 정한다(Arena는 베이크된 아레나 앵커, Roam은 절차 후보).
→ 스폰 직후 `FLNPEnemyHomeFragment` + `FLNPEnemyUnboundTag`를 붙인다. Pod 기반 프로세서는 이 태그를 쿼리에서 뺀다.
→ 엘리트 전용 Mass StateTree `ST_EnemyUnbound`가 Guard·Roam·Return·Alert·Combat를 고른다.
→ 귀환 중이면 Home 목표가 우선한다. Combat에서 Actor 초기화가 끝났으면 `Engage`가 `FLNPEnemyActorBrainTag`를 붙이고 Actor의 `CombatBrain`(StateTreeComponent)을 켠다. 이때부터 이동 의도는 Actor 두뇌만 쓴다.
→ Actor 두뇌는 패턴 어빌리티(GAS 몽타주)를 Utility Selection으로 고르고, 판정은 기존 칼날·발사체 파이프라인을 그대로 탄다.
→ HP가 0이 되면 `ULNPHealthProcessor`가 `DeathRewards`를 보고 배치 커맨드로 `ALNPLootDice::SpawnRewardSet`을 부른다.

상세는 `design/Architecture.md`가 소유한다.

## 5. 핵심 용어

| 용어 | 의미 |
|:---|:---|
| 엘리트 | 기획 용어. 코드에서는 "비귀속(Unbound) 적"이다. 코드·C++ 타입에는 쓰지 않는다(EL-002) |
| 귀속 방식(HomeMode) | `ELNPEnemyHomeMode { Pod, Arena, Roam }`. 적이 무엇을 집으로 삼는지 |
| HomeCenter / LeashRadius | Arena의 앵커 위치와 아레나 반경. Roam에는 없다 |
| 매크로 두뇌 | Mass StateTree `ST_EnemyUnbound`. 싸울지·돌아갈지·어디로 갈지를 정한다 |
| 전투 두뇌 | Actor의 `CombatBrain`(StateTreeComponent). 어떻게 싸울지(패턴·연계·페이즈)를 정한다 |
| 패턴 | 전투 두뇌가 고르는 공격 단위. GAS 어빌리티 1개 = 패턴 1개 |
| 해제 리셋 | 아레나 엘리트의 교전 해제 후 HP·경직·연출 장부를 초기화하는 것. 귀환 완료와 넉백 예외는 EL-016을 따른다 |

## 6. 모든 구현이 지켜야 하는 불변 조건

- **"싸울지"는 Mass가, "어떻게 싸울지"는 Actor가 정한다.** 타겟·슬롯·세력권·귀환의 원본은 엔티티다. 전투 두뇌는 엔티티의 Confirmed 타겟을 읽기만 하고 타겟을 고르지 않는다(EL-001).
- **이동 의도 작성자는 한 명이다.** `FLNPEnemyActorBrainTag`가 있으면 `ULNPEnemyMovementProcessor`는 Actor 의도를 쓰지 않는다. 태그가 없으면 전투 두뇌가 의도를 쓰지 않는다(EL-004).
- **Pod 적 경로를 바꾸지 않는다.** 엘리트 엔트리를 추가해도 Pod·Pod 적 배치 결과는 같은 시드에서 동일해야 한다(EL-005). Pod 기반 프로세서는 `FLNPEnemyUnboundTag`를 쿼리에서 제외하는 것으로만 대응한다.
- **코드에 등급 어휘를 넣지 않는다.** 축은 CombatMode와 HomeMode 둘뿐이다(EL-002, LowLOD §2).
- **상태는 엔티티가 보존한다.** Actor는 풀링되지 않고 강등 시 파괴된다. 페이즈는 HP 비율에서 파생하고, 쿨다운은 Actor 재생성 때 초기화되는 것을 허용한다(EL-007).
- **전투 두뇌와 드랍은 서버 전용이다.** 새 복제 필드를 만들지 않는다. 결과는 기존 GAS(몽타주·GameplayCue)와 LootDice 복제 경로로 나간다.
- **귀환·피격·패턴 종료를 함께 처리한다.** 귀환 중에는 타겟 추격이 Home 목표를 덮지 않는다. 바깥 공격과 귀환·리셋 중 피격은 EL-015로 차단하며, 교전 중단·사망 때 패턴을 정리한다(EL-012~016).
- **페이즈 전환 연출 소비는 엔티티가 보존한다.** 현재 페이즈는 계속 HP에서 파생한다. 연출은 시작 시 소비하고 Arena 완전 리셋 때 다시 허용한다(EL-017).

## 7. 문서별 소유 정보

- `README.md`: 작업 범위, 불변 조건, 문서 라우팅
- `Current.md`: 지금의 상태와 바로 다음 행동
- `Decisions.md`: 모든 세션이 알아야 하는 확정 결정
- `Roadmap.md`: 단계 순서, Phase 게이트
- `design/`: 현재 유효한 상세 설계와 검증 규약
- `phases/`: 해당 단계의 실행 계획과 완료 조건(착수 직전에 만든다)
- `history/`: 끝난 작업, 측정 기록, 시행착오(첫 작업 세션에서 만든다)

정보의 원본은 한 곳에만 둔다. 다른 문서에는 요약과 링크만 둔다.

## 8. 문서 갱신 규칙

### 세션 시작

1. 필수 문서 세 개를 읽는다.
2. `Current.md`에서 현재 Phase와 다음 행동을 확인한다.
3. 라우팅 표에서 필요한 설계·Phase 문서만 읽는다.
4. 코드와 에셋의 실제 상태가 문서와 맞는지 확인한다. 문서의 파일:라인 인용은 작성 시점(2026-11-05) 기준이다.

### 작업 중

- 새로 생긴 영구 규약은 해당 `design/` 문서에 반영한다.
- 작업 전체가 지켜야 하는 결정이면 `Decisions.md`에도 한 줄을 추가한다.
- 실험 수치와 측정 근거는 현재 Phase 로그에 기록한다.

### 세션 종료

1. `Current.md`를 실제 상태로 갱신한다.
2. 수행 내용과 검증 결과를 `history/PhaseNN_Log.md`에 날짜별로 추가한다(파일이 없으면 만든다).
3. 완료 조건을 충족했으면 Phase 문서의 체크리스트를 갱신한다.
4. 설계가 바뀌었으면 관련 `design/`과 `Decisions.md`를 먼저 고친다.
5. 다음 세션이 바로 실행할 수 있는 구체적인 작업 한두 개를 남긴다.

### Phase 종료

1. 완료 조건과 테스트 증거를 로그에 남긴다.
2. `Roadmap.md`의 상태를 갱신한다.
3. `Current.md`를 다음 Phase 기준으로 짧게 다시 쓴다.
4. 마지막 Phase(E6)에서 프로젝트 공통 설계 문서를 확정한다(`GameDesign_EliteNPC.md`·`TechDesign_EliteNPC.md` 신설 여부 판단, EnemyNPC·LootDice 교차 링크, 문서 인덱스 3곳).

## 9. 문서 상태 표기

| 상태 | 의미 |
|:---|:---|
| 활성 | 매 세션의 현재 진입점 |
| 확정 | 바꾸기 전에 명시적인 재논의가 필요한 결정 |
| 기준 설계 | 구현이 따라야 하는 현재 설계 |
| 초안 | 구현·측정 결과에 따라 바뀔 수 있음 |
| 검증 필요 | 엔진 또는 프로젝트 실험이 남아 있음 |
| 완료 | 해당 Phase의 완료 조건과 검증을 통과 |
| 보관 | 최신 설계가 아니며 역사 추적용 |
| 폐기 | 사용하지 않으며 기각 이유만 보존 |

## 10. 컨텍스트 예산 원칙

- 필수 세 문서는 합계 약 4,000~6,000토큰 이하를 목표로 한다.
- `Current.md`는 완료 이력을 쌓지 않고 200줄 이하를 유지한다.
- `Decisions.md`는 결론 중심의 표로 유지하고 상세 근거는 링크한다.
- 먼 Phase는 `Roadmap.md`에 요약만 두고 착수 전에 상세화한다.
