# TechDesign - Enemy NPC StateTree

## 1. 개요

Enemy NPC의 행동 선택을 MassEntity + StateTree로 구현한다. 길찾기(Navigation)를 쓰지 않는 단순 Steering 추적과 데이터 기반 상태 전환이 목표다.

**역할 분담이 이 설계의 핵심이다.** StateTree는 *무엇을 할지*만 정하고 `FMassMoveTargetFragment`에 의도를 남긴다. 실제 이동·회전·표면 스냅은 전부 `ULNPEnemyMovementProcessor`가 한다 (→ [TechDesign_EnemyNPC.md](TechDesign_EnemyNPC.md) §5). Task 안에서 Transform을 건드리지 않는 이유는 구면 규약(캡슐 중심·접평면·경사 차단)이 그 프로세서 한 곳에만 있기 때문이다.

⚠️ **Task Tick은 신호 구동이다.** `StateTreeActivate` 신호가 없으면 돌지 않으므로, Task 안에 타임아웃을 넣어도 그 코드가 실행되지 않는다. 시간 계측과 깨우기는 매 프레임 도는 프로세서가 맡는다 (§3.B.4의 배회 교착 복구).

---

## 2. StateTree 구조 (`ST_EnemyEntity`)

상태는 위에서 아래로 조건을 검사해 선택한다. 유지 조건이 깨지면 **Transition to Root**로 재선택한다.

| 우선순위 | 상태 | Enter 조건 | Task |
|---:|:---|:---|:---|
| 1 | **Combat** | `TargetingState == Confirmed` | (자식 상태가 수행) |
| 1-1 | └ Attack | `DistanceToTarget <= AttackRange` | `LNPEnemyAttackTask` |
| 1-2 | └ Chase | 사거리 밖 | `LNPEnemySteeringTask` |
| 2 | **Alert** | `TargetingState == Alert` | `LNPEnemyLookAtTask` |
| 3 | **Idle** | (없음 — 상위 실패 시 기본) | `LNPEnemyIdleTask` |

⚠️ **`LNPEnemyLookAtTask`는 Alert 전용이다.** Tick이 `State != Alert`면 즉시 `Failed`를 돌려주므로 Combat 부모 상태의 공통 Task로 달 수 없다. 추격·공격 중의 타겟 주시는 Steering/Attack Task가 `MoveTarget.Center`를 매번 타겟 위치로 갱신하고 MovementProcessor가 그 방향으로 회전시키는 것으로 성립한다 — 주시 전용 Task가 따로 필요하지 않다.
(2026-09-16 에셋 실측 확인: **Combat 상태의 Task 목록은 비어 있고**, LookAtTask는 `TargetingState == Alert` 조건을 가진 상태에만 붙어 있다.)

전이는 모두 `On Tick` + 상태 조건 불일치 → **Transition to Root**. 슬롯 탈락에 대응하는 별도 Retreat 상태는 없다.

---

## 3. 핵심 C++ 구성 요소

파일: `Source/LootNPop/Enemy/LNPEnemyStateTreeProcessors.{h,cpp}`, 인스턴스 데이터는 `LNPEnemyStateTreeTypes.h`.

### A. Evaluator

**`FLNPEnemyStateEvaluator`** — Mass Fragment를 StateTree 변수로 옮기는 유일한 창구.

- 입력: `FLNPEnemySharedFragment`(Config) / `FLNPEnemyTargetingFragment` / `FLNPEnemyTargetingCandidateFragment`
- 출력(`FLNPEnemyStateEvaluatorInstanceData`): `TargetingState`(None/Alert/Confirmed) · `DistanceToTarget` · `AttackRange`
- 시야 감지 자체는 여기서 하지 않는다 — `ULNPEnemyScoringProcessor`가 판정해 CandidateFragment에 남긴 결과를 읽기만 한다.

### B. Tasks

모든 Task는 `GetDependencies()`로 Fragment 읽기/쓰기 권한을 명시 선언한다.

**1. `FLNPEnemyLookAtTask`** — Alert 전용. `MoveTarget.Center`를 타겟 위치로, `DesiredSpeed = 0`으로 둔다. 회전은 MovementProcessor가 `QInterpConstantTo`(Config `RotationRate`)로 수행한다.

**2. `FLNPEnemySteeringTask`** — `MoveTarget.Center`를 **타겟에서 정지 거리만큼 앞**으로 잡고 `DesiredSpeed = MoveSpeed`. 정지 거리는 `FLNPEnemyMovementConfig::ComputeStopDistance()` — TargetFollow·Movement 프로세서와 **같은 공식을 공유**해 "프로세서는 멈췄는데 StateTree는 도착을 모르는" 불일치를 막는다. 사거리 도달 시 `Succeeded` → Attack.

**3. `FLNPEnemyAttackTask`** — EnterState에서 이동을 멈추고(`DesiredSpeed = 0`) 타겟 방향을 유지. Tick은 `CombatMode`로 갈린다.

| 모드 | Tick 동작 |
|:---|:---|
| `PureEntity` | `FLNPEntityAttackFragment::bAttackRequested`만 세운다. 위상 진행은 `ULNPEntityAttackProcessor`가 맡는다 — 신호 구동인 Tick에 위상을 두면 신호가 끊긴 프레임에 스윙이 중간에 멈춘다 |
| `ActorPromoted` | `SetAimTargetLocation(TargetLocation)` → `TryActivateAttack()`. 조준을 **발사 직전에** 갱신해야 원거리 어빌리티가 같은 프레임에 그 값으로 발사 방향을 만든다 |

`Targeting.State != Confirmed`이거나 사거리를 벗어나면 `Failed` → Chase 복귀. ExitState에서 `ClearAimTarget()`으로 조준을 수평으로 되돌린다 — 빠뜨리면 배회 중에도 하늘을 겨눈 채 걷는다.

공격 루프 자체는 Task가 돌리지 않는다. `ULNPEnemyTargetFollowProcessor`가 정지 구역에 있는 동안 매 프레임 보내는 StateTree 신호가 Tick을 구동한다.

**4. `FLNPEnemyIdleTask`** — LootPod 주변 배회.

- 목표 방향: Pod 위치의 구면 접선 2축(`Tangent1`/`Tangent2`)을 랜덤 각도로 조합. 거리는 `WanderMin~MaxDistance`.
- 실제 목표점은 `ULNPSurfaceCacheSubsystem::GetSurfacePoint()`로 지형에 투영한 뒤 **캡슐 중심 높이로 끌어올린다** — 이 보정을 빼면 목적지와 현재 위치 사이에 `HalfHeight`만큼의 수직 성분이 상시로 남아 도착 임계값(`ArrivalTolerance` 30cm)을 영영 넘지 못하고 배회가 교착된다.
- 도착하면 **3초 대기 후** 새 목표를 뽑는다(간격이 아니라 도착 후 대기다).
- **시동 트릭:** EnterState에서 목적지를 현재 위치로 둬 다음 프레임의 "도착" 신호로 Tick이 즉시 재개되게 한다.
- **교착 복구:** MovementProcessor가 `bWanderTargetTimedOut`(`WanderTimeout` 10초)을 세우면 목표를 폐기하고 대기 간격을 건너뛰어 즉시 재추첨한다. 판단은 IdleTask 단독, 시간 계측은 프로세서 — 신호 구동 구조에서 목표 결정 주체를 하나로 유지하는 방식이다.

---

## 4. 액터 스폰 조건 (Representation)

`ULNPEnemyLODOverrideProcessor`는 **타게팅 상태만** 본다 — StateTree의 활성 상태는 보지 않는다.

- `TargetingState == Confirmed` → `FMassRepresentationLODFragment::LOD`를 `High`로 강제 (거리 무관).
- `ULNPEnemyConfig::CombatMode == PureEntity`면 이 강제를 통째로 건너뛴다 — 그 개체는 전투 중에도 순수 엔티티로 남는다.

`CurrentRepresentation`을 직접 바꾸지 않고 LOD 값만 올리는 이유, 게스트의 표현 소유권 분리는 → [TechDesign_EnemyNPC.md](TechDesign_EnemyNPC.md) §7.2·§7.10.
