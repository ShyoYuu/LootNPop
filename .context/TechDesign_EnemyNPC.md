# Enemy NPC 시스템 기술 설계

## 1. 한눈에 보기

수백~수천 유닛을 전제로 한 **MassEntity ↔ Actor 하이브리드** 구조. 평상시에는 순수 엔티티(인스턴스 메시)로 시뮬레이션되고, 전투 진입(`Confirmed`) 시 **거리와 무관하게** High LOD Actor로 전환되어 GAS 전투·애니메이션이 활성화된다. 단 `ULNPEnemyConfig::CombatMode == PureEntity`인 적은 승격하지 않고 Mass 프로세서가 직접 공격한다 (→ [TechDesign_EnemyNPC_LowLOD.md](TechDesign_EnemyNPC_LowLOD.md)).

```
[MassEntity] ─── 데이터/시뮬레이션 (수백~수천 유닛, Worker Thread 병렬)
     │   Scoring → Targeting → TargetFollow → Movement  (프로세서 파이프라인)
     ▼
[ULNPTargetingSubsystem] ─── 슬롯 기반 타겟 경쟁 (전역 그리디 재배분)
     │
     ▼
[ALNPEnemyCharacter + GAS] ─── High LOD 전투 비주얼 / 어빌리티 (LODOverride가 강제 전환)
```

**서버 전용 시뮬레이션:** MassReplication 이후 클라이언트에도 동일 아키타입 엔티티가 존재하므로, 모든 AI·이동·HP 프로세서는 `LNPMass::IsClientWorld()` 가드로 클라이언트 실행을 차단한다 (결과는 복제 채널로 전달).

---

## 2. 데이터 구조

### 2.1 Fragments & Tags

`ULNPEnemyTrait::BuildTemplate`이 붙이는 것 전부(`Source/LootNPop/Enemy/LNPEnemyMassTypes.h`).

| 타입 | 내용 |
|:---|:---|
| `FLNPEnemyFragment` | Health/MaxHealth/Defense, DeathCountdown, EnemyTypeTag, ParentLootPod(+위치 — 세력권 기준), HitReactTimer/Direction(피격 주시 — §3.5), FlinchTimeRemaining(피격 플린치 연출) |
| `FLNPEnemySharedFragment` | `ULNPEnemyConfig` 포인터 (동일 타입 공유 — ConstShared) |
| `FLNPEnemyTargetingCandidateFragment` | 인식된 잠재 타겟 최대 4명 (거리 정렬) + `AlertDwellTime`(경계 인내) + `DisengageTimer`(재발견 금지 잔여). **뒤 두 float는 Reset 대상이 아니다** — §3.4 |
| `FLNPEnemyTargetingFragment` | 최종 타겟, `ELNPTargetingState`(None/Alert/Confirmed), 마지막 타겟 위치, 거리² |
| `FLNPEnemyIdleFragment` | 배회 타이머·플래그 + 미도달 타임아웃(`TimeSinceWanderIssued`/`bWanderTargetTimedOut` — §5.1 끝) |
| `FLNPEnemyVelocityFragment` | Entity 모드 물리 속도 (넉백·사망 팝의 포물선). 접지 시 0 |
| `FLNPEnemySeparationFragment` | 겹침을 푸는 접평면 밀어내기 속도. 분리 프로세서가 매 프레임 확정하고 이동 프로세서가 소비 — §5.0 |
| `FLNPEntityAttackFragment` · `FLNPEnemyActionFragment` | 순수 엔티티 공격 위상 / 게스트 연출의 단일 입력인 행동 상태 → [LowLOD](TechDesign_EnemyNPC_LowLOD.md) §4·§5 |
| `FMassRepresentationAnimationFragment` | ISKM 애니 데이터의 자리. ⚠️ **엔진 트레이트가 붙여 주지 않는다** — 없으면 소비 프로세서 쿼리가 아무것도 매칭하지 않아 경고 없이 안 움직인다 |
| `FLNPEnemyHealthDisplayFragment` | HP 바 표시 후보 선별용 장부. 서버·클라 각자 로컬로 채워 복제하지 않는다 (→ [TechDesign_HUD.md](TechDesign_HUD.md) §11) |
| `FLNPPoiseFragment` | 경직도. 적은 지속 버프를 안 받으므로 Trait이 Config 값으로 1회 시드하면 끝 (→ [TechDesign_Poise.md](TechDesign_Poise.md)) |
| `FLNPPositionHistoryFragment` | Lag Compensation용 위치 히스토리 (서버 기록) |
| `FLNPReplicatedMovementFragment` | **클라이언트 전용** — 복제 수신 사이를 메우는 보간 상태. `NM_Client`일 때만 아키타입에 들어간다 |
| Tags | `FLNPEnemyTag` / `FLNPPlayerTag`(쿼리 분류), `FLNPEnemyActorInitializedTag`(초기화 마커), `FLNPEnemyDyingTag`(소멸 대기), `FLNPPlayerDeadTag`(사망 플레이어 — 타게팅 제외) |

⚠️ **`FLNPEntityAttackFragment`·`FLNPEnemyActionFragment`·`FMassRepresentationAnimationFragment`는
`CombatMode`와 무관하게 전원에게 붙인다.** 모드로 아키타입을 가르면 같은 쿼리를 두 벌 유지해야 하고
StateTree 외부 데이터 핸들이 Optional이 되어 Task마다 null 분기가 생긴다. 행동 상태 쪽은 이유가 하나 더
있다 — **서버와 게스트의 아키타입이 같아야** 수신값을 쓸 자리가 생긴다.

### 2.2 ULNPEnemyConfig (Data Asset)

| 묶음 | 내용 |
|:---|:---|
| 식별·스폰 | EnemyTypeTag / EnemyActorClass / StateTree / WeaponData / DefaultAbilities / InitialAttributeValues / 캡슐 크기 |
| 전투 모드 | **`CombatMode`**(ActorPromoted·PureEntity) + **`AttackType`**(Melee·Ranged) → §7.2, [LowLOD](TechDesign_EnemyNPC_LowLOD.md) |
| 경직 | PoiseResistance / PoiseStaggerThreshold / PoiseDownThreshold — Trait이 프래그먼트에 시드 |
| 연출 | `ActionSequences`(행동 상태 → ISKM 시퀀스 인덱스) / AnimBlendTime / PureEntity 사망·플린치 3종 |
| 서브 구조체 3종 | 아래 |

- **`FLNPEnemyTargetingConfig`**: 인지 거리 3종 (AwarenessDistance / VisionDistance+Angle / AlertRetentionDistance) + 세력권 반경 ChaseRadius + 시간 3종 (AlertPatienceTime / AlertRecoveryTime / HitReactLookTime) — §3.3~3.5. `DistanceWeight`/`AngleWeight`는 선언만 있고 쓰이지 않는다(§8).
- **`FLNPEnemyMovementConfig`**: MoveSpeed, RotationRate, Gravity, Wander 반경, 분리 반경·강도, AttackRange/Interval, `AimPitchMin/MaxDeg`(§7.9) + 상수 `ArrivalTolerance`(30cm)·`WanderTimeout`(10초) + **`ComputeStopDistance()`** — 추격 정지 거리 공식의 단일 정의 (TargetFollow·Movement·SteeringTask가 공유)
- **`FLNPEntityAttackConfig`**: 순수 엔티티 기본 공격의 위상 시간·가상 칼날 치수·산탄 → [LowLOD](TechDesign_EnemyNPC_LowLOD.md) §4

**`ULNPEnemyTrait`가 엔티티 템플릿을 조립한다.** Config를 ConstShared로 묶고, MassReplication
Trait(BubbleInfo/Replicator 고정, `ReplicationCullDistance` 12,000)을 내부 위임한다. 두 가지가 더 있다.

- **무기 스텟을 스폰 시점에 녹여 넣는다.** `MaxHealth`·`Defense`를 `WeaponData`의 레벨 1 StatModifier로
  해석해 시드하고 `Health = MaxHealth`로 시작한다. 안 하면 ① 승격 순간 Actor ASC만 Max가 올라
  무손상 적에게 HP 바가 뜨고 ② `Defense`가 0으로 남아 **같은 공격이 순수 엔티티 적에게만 더 아프게** 들어간다.
- ⚠️ **`ValidateTemplate`이 `CombatMode`와 표현 매핑의 불일치를 경고한다.** 승격 여부의 단일 진실은
  enum이지만 실제 Actor 스폰 여부는 EntityConfig의 표현 매핑이 정한다. 둘은 서로를 모르므로 어긋나도
  컴파일도 실행도 실패하지 않고 조용히 틀린다 — `PureEntity`인데 매핑에 Actor가 남아 있으면 가까이 간
  것만으로 승격된다(실제로 밟았다).

---

## 3. 슬롯 기반 타겟팅

### 3.1 ULNPTargetingSubsystem

플레이어별 교전 밀도를 제한하는 전역 서브시스템. 슬롯 풀은 **`ELNPTargetSlotPool` 3종**이고
풀마다 독립된 한도를 갖는다 — `Melee` 10 / `Ranged` 20 / `Promoted` 2 (에디터 설정 가능).

풀 판별의 단일 원본은 `ULNPEnemyConfig::GetSlotPool()`이다: `ActorPromoted`면 무조건 `Promoted`,
`PureEntity`면 `AttackType`으로 Melee/Ranged. **승격 개체는 근접/원거리를 나누지 않는다** — 이 풀을
가르는 실제 비용 축이 교전 거리가 아니라 Actor 스폰 수이기 때문이다.
⚠️ `MaxPromotedSlotsPerPlayer`가 곧 **플레이어당 적 Actor 수 상한이자 대역폭 예산**이다(1기당 700~900 B/s).
근거와 튜닝 상한은 → [TechDesign_EnemyNPC_LowLOD.md](TechDesign_EnemyNPC_LowLOD.md) §7.

```
매 프레임:
  ScoringProcessor → RegisterEnemyInterest(Enemy, Player, Score, Pool)  ← 게임 스레드 커맨드로 지연 등록
  TargetingProcessor → RebalanceSlots()
      전체 등록 항목을 점수 내림차순 정렬 → 풀별로 그리디 할당
      (Enemy 1개는 1개 슬롯만, 풀 한도 초과 시 탈락)
  → IsSlotConfirmed()로 각 Enemy가 결과 조회
```

`FCriticalSection`으로 Mass Worker Thread 경합을 방지한다. ⚠️ `TMassExternalSubsystemTraits`의
`ThreadSafeWrite`는 **일부러 false다** — 쓰기 자체는 Lock으로 안전하지만 true로 두면 Mass가 이
서브시스템을 쓰는 프로세서들을 병렬로 돌려, 프레임당 한 번 도는 `RebalanceSlots()`가
`IsSlotConfirmed()` 조회와 뒤섞인다.

### 3.2 점수 공식 (ScoringProcessor)

```
Score = 1,000,000 / (거리 + 1)
```

거리 하나뿐이다. 세력권 감쇠는 **넣지 않는다** — 추격 자격이 이진값(§3.3)이라 자격 없는 개체는
애초에 등록되지 않고, 자격 있는 개체끼리 "집에서 먼 순"으로 순위를 매길 이유가 없다.

**타겟 고수(stickiness)는 없다.** `RebalanceSlots()`는 매 프레임 `PlayerSlots`를 통째로 비우고
점수순으로 다시 그리디 할당하며, `TargetingProcessor`는 후보를 거리 오름차순으로 훑어
먼저 확정되는 하나를 잡는다. 결과적으로 **"인지 중인 후보 가운데 가장 가까운 쪽"** 이 매 프레임
타겟이 된다 — 교전 중이라고 우선권이 붙지 않는다.

이는 의도된 성질이다. 2P 교차 사격 실측(2026-09-01)에서 근접·원거리 NPC의 거동이 갈렸는데,
원인은 근접/원거리 로직 차이가 아니라 **교전 거리 차이** 하나였다:

| | 타겟과의 유지 거리 | 뒤에서 쏜 다른 플레이어 | 결과 |
|:---|:---|:---|:---|
| 근접 NPC | `AttackRange` 200cm | 그보다 멀다 | 점수에서 밀려 **타겟 유지** |
| 원거리 NPC | 정지 거리 900cm | 더 가까울 때가 많다 | 점수가 높아 **타겟 전환** |

슬롯 풀은 **한도만** 가르며, 한도에 걸리지 않는 동안에는 거동에 관여하지 않는다. 즉 "근접은
안 돌아보고 원거리는 돌아본다"로 보이는 것은 같은 규칙의 서로 다른 입력값일 뿐이다.

⚠️ 두 플레이어가 **거의 같은 거리**에 있고 둘 다 인지되는 동안에는 미세한 거리 변화로 타겟이
프레임 단위로 뒤집힐 수 있다. 이 구조가 원래 갖는 성질이며(피격 인지와 무관하게 성립한다),
실측에서 문제로 드러난 적은 없다. 체감 문제가 생기면 고수 규칙이 아니라 **점수 히스테리시스**로
접근할 것 — 상태 전이에 래치를 다는 방식은 §7.8의 자기진동을 부른다.

### 3.3 인지·추격 규칙

```
None      ──[발견: VisionDistance+VisionAngle  또는  AwarenessDistance]──▶ Alert
Alert     ──[슬롯 획득]──────────────────────────────────────────────────▶ Confirmed
Confirmed ──[플레이어가 Pod 세력권 밖]───────────────────────────────────▶ Alert
Alert     ──[타겟이 AlertRetentionDistance 밖  또는  경계 인내 소진]─────▶ None
```

기본값: `AwarenessDistance` 200 < `VisionDistance` 2000 < `AlertRetentionDistance` 2500 ≤ `ChaseRadius` 5000.

**인식 조건** (하나라도 충족 시 후보 등록):
- `AwarenessDistance` 이내 → FOV·재발견 금지 무관, 무조건 감지
- **추적 중인 그 타겟**(`PreviousState != None && Player == TargetPlayer`)이고 `AlertRetentionDistance`
  이내 → 시야 재검사 면제. **이 면제는 그 한 명에게만 준다** — §7.7
- **피격 주시 중**(`HitReactTimer > 0`)이고 `VisionDistance` 이내 + **피격 방향** 기준
  `VisionAngle/2` 이내 + **조준 가용 각도**(`AimPitchMin/MaxDeg`) 이내 → 감지.
  재발견 금지 창은 보지 않는다 — §7.9
- `VisionDistance` 이내 + `VisionAngle/2` 이내 + 재발견 금지 창이 닫혀 있음 → 시야 감지

⚠️ **시야각은 3D 원뿔이다** — `VisionAngle`을 정면 벡터와 대상 방향의 3D 각도로 잰다.
따라서 고저차가 좌우 예산을 그대로 잡아먹어, 급경사에서는 평면상 정면인 상대도 시야 밖이 된다
(45° 경사면 ±45° 예산이 전부 소모된다). **이는 수용한 제약이다** — 접평면 부채꼴로 바꾸면
절벽 위 플레이어를 발밑에서 발견하게 되고, 상하 상한을 따로 두면 조준 클램프와 맞물려
튜닝 축이 하나 더 늘어난다. 실익 대비 복잡도가 크지 않다고 판단했다.
대신 **정면으로 못 보는 상대에게 맞았을 때** 반응하지 못하는 구멍만 §7.9로 메웠다.
**추격 자격**(슬롯 경쟁 참가 조건) = **플레이어**가 `ParentPodLocation`에서 `ChaseRadius` 이내
**또는** `AwarenessDistance` 이내(코앞 반격). 자격이 없는 후보는 목록에는 남지만
`RegisterEnemyInterest`를 타지 않는다 — 슬롯을 못 얻으니 `TargetingProcessor`가 자연히 `Alert`로 잡는다.
**이것이 강등 경로 전부다** (별도의 강등 코드나 상태 플래그가 없다).

⚠️ **거리를 재는 대상이 NPC가 아니라 플레이어인 것이 이 설계의 핵심이다.** §7.8 참조.

**경계 인내 — 시간도 강등 축이다.** 거리만으로는 사다리가 닫히지 않는다(기획 의도는
→ [GameDesign_EnemyNPC.md](GameDesign_EnemyNPC.md) §5.2). `FLNPEnemyTargetingCandidateFragment::AlertDwellTime`에
"추격도 못 하면서 경계만 하고 있는" 시간을 누적하고, `AlertPatienceTime`(8초)에 도달하면
**그 프레임에 유지 조건까지 끊어** `None`으로 내려보낸다.

- **발견만 막으면 안 된다.** 유지 조건을 남기면 추적 중인 타겟이 유지 거리 안에 계속 있어 경계가
  영원히 풀리지 않는다. 소진 프레임에는 초근접을 제외한 **모든 인식 경로를 끈다.**
- **누적 조건은 `PreviousState == Alert && !bAnyChaseEligible && HitReactTimer <= 0` 하나다.**
  그 밖에는 전부 0으로 초기화되므로, "슬롯 대기 중에는 안 흩어진다"·"교전에 성공하면 새로 시작"·
  "피격 반응 중에는 안 잰다"가 **예외 코드 없이** 한꺼번에 성립한다.

### 3.4 재발견 금지 창 — 등을 돌릴 시간을 벌어 준다

인내 소진만으로는 아무것도 해결되지 않는다. 포기한 그 프레임에도 플레이어는 정면 시야 안에 있어
**다음 프레임에 곧바로 재발견된다.** 8초마다 상태가 왕복할 뿐 NPC는 Pod 쪽으로 한 발짝도 못 걷는다.

`FLNPEnemyTargetingCandidateFragment::DisengageTimer`가 이 구멍을 막는다. `AlertDwellTime >= AlertPatienceTime`
인 프레임에 `AlertRecoveryTime`(1초)으로 세팅되고 매 프레임 감소하며, 0보다 큰 동안
**`VisionDistance` + FOV 경로만** 건너뛴다(유지·초근접 경로에는 관여하지 않는다).

- **이 창이 벌어 주는 것은 이동이 아니라 회전이다.** Idle이 된 NPC는 배회 목표를 향해 돌아서고,
  시야각 절반(45°)만 돌면 플레이어가 이미 FOV 밖이다. `RotationRate` 360°/s 기준 45°는 0.125초,
  180°도 0.5초 — 1초는 2배 버퍼다.
- **초근접(`AwarenessDistance`)은 이 금지를 무시한다.** 회복 중이라고 눈앞의 플레이어를 못 보면
  "때려도 반응 없는 적"이 되어 더 어색해진다.
- **인내와 별도의 float로 둔다.** 하나에 겹치면 값 하나만 보고 "차오르는 중인지 회복 중인지"를
  구분할 수 없어 계측이 그대로 함정이 된다.

⚠️ `AlertDwellTime`과 `DisengageTimer`는 `FLNPEnemyTargetingCandidateFragment::Reset()`이
**지우지 않는다.** Reset()은 매 프레임 후보 목록을 비우는 용도이고, 둘 다 프레임을 가로질러
유지되어야 하는 값이다.

### 3.5 피격 반응

피격 판정 두 곳(`ULNPWeaponTraceProcessor` 근접 / `ULNPProjectileProcessor` 원거리)이 Actor 승격
여부와 **무관하게** `FLNPEnemyFragment::HitReactTimer`(= `HitReactLookTime`)와 `HitReactDirection`
(피격자 → 공격자)을 기록한다. Actor 경로에서만 쓰면 LOD에 따라 반응이 갈린다.

`ULNPEnemyMovementProcessor`는 타이머를 **상태와 무관하게** 매 프레임 감소시키되(Alert 중에 맞은
타이머가 나중에 Idle에서 엉뚱하게 발동하는 것을 막는다), 연출은 `None` 분기에서만 재생한다 —
속도 0 + 피격 방향(접평면 투영)으로 회전. 돌아본 결과 시야에 플레이어가 있으면 평소의 발견 경로가
그대로 돌아 `Alert`가 되므로, StateTree에도 인식 코드에도 새 분기가 필요 없다.

**추격 자격은 주지 않는다** — 주면 세력권 밖에서 원거리로 찔러 무한정 끌고 다닐 수 있다.

**사망한 플레이어는 후보에서 빠진다.** 플레이어는 사망해도 폰이 파괴되지 않고 랙돌로
리스폰 지연시간만큼 월드에 남으며, 그동안 `FLNPPlayerTag` 엔티티도 Transform 동기화까지
그대로 살아 있다. `ALNPPlayerCharacter::HandleDeathOnServer`가 `FLNPPlayerDeadTag`를 부여하고
Scoring·Targeting의 `PlayerQuery`가 이를 배제한다 (적 쪽 `FLNPEnemyDyingTag` 배제와 대칭).
해제 경로는 없다 — 리스폰은 폰을 파괴하고 새로 스폰하므로 새 엔티티에는 태그가 없다.

**반대 방향(적이 죽는 쪽)은 태그 하나로 끝나지 않는다.** 플레이어의 락온·근접 보정이 쓰는
`ULNPTargetQueryProcessor`는 `FLNPEnemyFragment::Health <= 0`과 `FLNPEnemyActionFragment::Action == Dying`을
**둘 다** 보고 시체를 거른다 — 서버에서는 HP가 먼저 0이 되고 행동 상태 전이가 한 틱 늦을 수 있어서다.
사망 표현을 한쪽 값만으로 서술하면 이 소비처와 어긋난다.

슬롯 풀은 `ULNPEnemyConfig::GetSlotPool()`로 **청크당 1회만** 판정한다(§3.1). 예전에는 EnemyTypeTag에
"Melee"가 들어 있는지로 봤는데, 태그 이름과 거동이 조용히 어긋날 수 있어 Config의
`CombatMode`+`AttackType`으로 옮겼다.

---

## 4. Mass 프로세서 파이프라인 (`Enemy/` 18종)

"서버 전용"은 전부 `Execute` 첫 줄의 `LNPMass::IsClientWorld()` 가드다 — `ExecutionFlags`로 거르는
것은 표에 따로 적은 셋뿐이다.

| 프로세서 | 단계 | 역할 |
|:---|:---|:---|
| `ULNPEnemyScoringProcessor` | PostPhysics (UpdateWorldFromMass) | 인식 + 후보 4명 정렬 + 슬롯 점수 등록 |
| `ULNPEnemyTargetingProcessor` | Behavior | `RebalanceSlots()` 호출 → State 동기화(Confirmed/Alert/None) → 변경 시 StateTree 신호 |
| `ULNPEnemyTargetFollowProcessor` | Behavior (Targeting 이후) | MoveTarget 목적지 산출 (정지 거리 반영), 공격 루프용 StateTree 신호 |
| `ULNPEnemySpatialGridProcessor` | PrePhysics — Movement (Separation 이전) | 서버 전용: 살아 있는 적 전원의 브로드페이즈 격자를 매 프레임 재구축 — §5.0 |
| `ULNPEnemySeparationProcessor` | PrePhysics — Movement (Grid 이후, Movement 이전) | 서버 전용: 이웃 질의 → 겹침 분리력 산출. Transform은 건드리지 않는다 — §5.0 |
| `ULNPEnemyMovementProcessor` | Movement | 실제 이동/회전 적용 + 분리력·공중 물리 소비 — §5 상세 |
| `ULNPHealthProcessor` | PostPhysics | HP ≤ 0 → DyingTag + `DeathCountdown`. 모드로 갈린다: `ActorPromoted`는 `TriggerRagdoll()` 방송 + `ULNPSettings::EnemyRagdollDuration`, `PureEntity`는 **사망 팝**(속도 프래그먼트에 Up 방향 속도) + `Config::PureEntityDeathDuration` |
| `ULNPEnemyDeathTimerProcessor` | PostPhysics (Health 이후) | DeathCountdown 만료 엔티티 파괴 |
| `ULNPEnemyLODOverrideProcessor` | **PostPhysics** — LOD 그룹 | 서버 전용: Confirmed면 `RepresentationLOD.LOD = High` 강제 — §7.2 |
| `ULNPEnemyClientRepresentationProcessor` | **PrePhysics** — LOD 그룹 (Visualization 이전) | 게스트 전용(`ExecutionFlags = Client`): 복제 Actor만 표현으로 채택 — §7.10. 페이즈가 다른 이유도 §7.10 |
| `ULNPEnemyActorInitializerProcessor` | PostPhysics (Representation 이후) | 신규 스폰 Actor에 `InitializeOnce` + `SyncFromEntity` → InitializedTag 부여 |
| `ULNPEnemyActorSyncProcessor` | PostPhysics (LOD 이전, 게임 스레드) | Actor 유효: `SyncToEntity`(HP·속도 역동기화) / null: InitializedTag 제거 → 재초기화 유도 |
| `ULNPEnemyActionProcessor` | PrePhysics — Tasks (EntityAttack 이후) | 서버 전용: 행동 상태 산출 → `FLNPEnemyActionFragment` (게스트 연출의 단일 입력) |
| `ULNPEnemyAnimationProcessor` | **PrePhysics** — Representation 그룹, 게임 스레드 | 행동 상태 → ISKM 애니 데이터. `ExecutionFlags = Client \| Standalone`(데디 서버는 그리지 않는다). 페이즈 근거는 §7.10과 같다 |
| `ULNPEnemyActionDebugDrawProcessor` | 에디터 전용 (`LNP.Debug.DrawEnemyAction`, 기본 0) | 행동 상태별 색상 박스 + 전이 로그. 서버/클라 분기가 없는 것이 곧 채널 검증 수단이다 |
| `ULNPEnemyMarkerProcessor` | **PrePhysics** (HUD Tick과 같은 페이즈) | 적 HP 바 표시 후보 상위 N개 수집 → [TechDesign_HUD.md](TechDesign_HUD.md) §11 |
| `ULNPEntityAttackProcessor` | PrePhysics — Tasks | 서버 전용: 순수 엔티티 공격 위상 진행·가상 칼날·발사 → [LowLOD](TechDesign_EnemyNPC_LowLOD.md) §4 |
| `ULNPEntityGhostProjectileProcessor` | PrePhysics — Tasks, 게임 스레드 | 게스트 전용(`ExecutionFlags = Client`): 수신한 발사 전이로 Ghost 발사체 생성 → [LowLOD](TechDesign_EnemyNPC_LowLOD.md) §4 |

> 2026-09-06에 `ULNPEnemyDebugDrawProcessor`(타게팅 상태 박스)를 제거했다 — 행동 상태 드로우와 겹쳤고
> cvar 게이트도 없었다. **잃은 것은 "경계 중이지만 슬롯이 없어 구경만 하는 적"의 색 구분**이다.
> 슬롯 풀을 다시 눈으로 검증할 일이 생기면 되살릴 것.

---

## 5. Entity 이동 시뮬레이션 (MovementProcessor)

상태별 속도/방향 결정 후, **Actor 모드와 Entity 모드로 분기**한다.

```
Actor 모드 (High LOD):
  → SetAIOrientationIntent / SetAIMoveInput 위임 (게임 스레드 커맨드)
  → 실제 이동은 캐릭터의 Mover 컴포넌트가 처리 (플레이어와 동일 파이프라인)

Entity 모드 (Low LOD):
  ├─ PhysVelocity ≠ 0 (공중 — 넉백/사망 팝/포물선):
  │    중력 적분 → SurfaceCache로 착지 판정 → 착지 시 표면 스냅 + 속도 0
  └─ 접지:
       QInterpConstantTo 회전 (RotationRate) → 분리력 가산 → 경사 차단(§7.4) → SurfaceCache 표면 스냅 이동
```

- 구형 UpDir은 `(GravityOrigin - Location).GetSafeNormal()`로 실시간 계산 (Fragment 저장 없음 — 캐시 효율).
- 지표면 좌표는 전부 `ULNPSurfaceCacheSubsystem` O(1) 조회 (워커 스레드에서 직접 호출 —
  → [TechDesign_SurfaceCache.md](TechDesign_SurfaceCache.md)).
  ⚠️ **접지 스냅은 캐시 값을 검증 없이 그대로 위치로 쓴다** — 즉 **캐시 오차가 곧 매몰 깊이**다.
  승격 시 `TeleportActor`가 그 좌표를 그대로 옮기는데 Mover는 깊은 침투를 한 프레임에 풀지 못하고,
  경사 게이트(§7.4)까지 벽으로 판정해 속도를 0으로 만든다 — "꼼짝 못 하는데 공격은 하는" 상태가 된다
  (어빌리티·StateTree는 이 경로와 무관하다). 지형 한계는 `TechDesign_SurfaceCache.md` §7.
- **공중 물리는 람다 하나로 뽑아 두 소비처(넉백·사망 팝)가 공유한다.** 복제하면 죽는 순간에만
  다른 곡선을 그리는 어긋남이 생긴다.

**이 프로세서가 매 프레임 도는 유일한 경로라 시간 기반 상태의 감소도 여기 모여 있다** —
피격 주시 타이머(§3.5), 피격 플린치, 배회 타임아웃. 판단은 각자의 주인이 하고 시계만 여기서 돈다.

⚠️ **쿼리에서 `FLNPEnemyDyingTag`를 `None`으로 걸지 않는다.** 죽는 순간 태그가 붙어 쿼리에서 빠지면
**사망 팝을 아무도 적분하지 못한다**(→ [TechDesign_EnemyNPC_LowLOD.md](TechDesign_EnemyNPC_LowLOD.md) §8).
시체의 AI 이동·회전·StateTree 신호는 `Execute` 안에서 태그로 따로 차단하고,
랙돌이 붙은 시체는 물리의 주인이 Actor이므로 통째로 건너뛴다.

### 5.0 겹침 분리 — 격자는 서비스, 소비는 이동 프로세서

`ULNPEnemySeparationProcessor`가 이웃과의 **접평면** 거리로 밀어내기 속도를 만들어
`FLNPEnemySeparationFragment::Push`에 남기고, 이동 프로세서의 접지 분기가 그것을 속도에 더한다.

**분리 프로세서는 Transform을 건드리지 않는다.** 표면 스냅과 경사 차단이 이동 프로세서에 있으므로,
그 앞에서 위치를 옮기면 구면 규약이 반쪽만 적용된다 — §5.1의 쓰기 권한 규약이 그대로 적용되는 자리다.

- 이웃 탐색은 `ULNPEnemySpatialGridSubsystem`(등장방형 축소 행, 매 프레임 재구축)이 맡는다
  → [TechDesign_TargetQuery.md](TechDesign_TargetQuery.md) §6
- ⚠️ **Actor로 그려지는 개체는 밀지 않는다.** 캡슐 콜리전이 이미 겹침을 막고, `SetAIMoveInput`이
  **방향만** 받는 규약이라 분리력을 섞으면 크기가 1 미만이 되어 Mover의 의도 벡터 미정규화
  함정(속도 곱셈 붕괴)을 정면으로 밟는다.
- ⚠️ 넉백·사망 팝 중에는 건너뛴다 — 공중 분기가 위치의 주인이다.
- **생산자가 매 프레임 값을 확정한다.** 밀지 않기로 한 개체에도 0을 명시적으로 쓴다 —
  소비 쪽에서 지우게 하면 LOD가 바뀌는 순간 낡은 값이 한 번 새어 나간다.

⚠️ **순서는 그룹이 아니라 이름으로 건다.** `격자 → 분리 → 이동` 셋 다 `Movement` 그룹·`PrePhysics`에
두고 `ExecuteBefore/After`에 클래스 이름을 직접 적는다. 그룹 간 순서는 엔진의 고정 목록이 아니라
**프로세서들이 각자 선언한 간선에서 유도되고**, `Avoidance → Movement` 간선을 만들던 엔진의
`UMassApplyMovementProcessor`를 이 프로젝트는 쓰지 않는다 — 그 간선이 존재한다는 보장이 없다.

### 5.1 좌표 규약과 권한 경계 (불변식)

두 모드가 같은 `FTransformFragment`를 공유하므로 **기준점과 쓰기 권한을 하나로 못 박는다.**

- **기준점은 언제나 캡슐 중심이다.** 발밑이 아니다.
  피격 판정 프로세서(`|Axial| <= HalfHeight`), Actor 승격 시 엔진의 `TeleportActor`,
  플레이어 엔티티가 모두 중심을 가정한다. Entity 모드에서 표면점을 쓸 때는
  `표면 반지름 - CapsuleHalfHeight`로 중심 반지름을 만든다 (구 내벽이라 Up이 반지름 감소 방향).
- **Actor로 그려지는 동안 Transform의 권한은 Mover 단독이다.** Mass는 읽기만 한다.
  이를 보장하려고 EntityConfig의 `MassAgent*SyncTrait`를 **`ActorToMass`** 로 둔다
  (플레이어 설정과 동일). 엔진 기본값 `BothWays`로 두면 `AddTranslator`가 양방향 태그를 전부
  아키타입에 심어 `UMassTransformToActorCapsuleTranslator`가 매 프레임 Mover의 변위를
  되쓰기로 지운다 — 걷기 모션만 재생되고 제자리에 멈추는 증상이 된다.

  ⚠️ **판정 기준은 "Actor가 붙어 있는가"가 아니라 "Actor로 그려지는가"다.**
  `ALNPEnemyCharacter`는 `bReplicates = true`(ASC 복제용)라 서버가 승격시킨 적 Actor가
  **게스트에도 시뮬레이티드 프록시로 내려온다.** 게스트가 멀어서 인스턴스 메시로 그리는 동안에도
  `FMassActorFragment`는 채워져 있으므로, 존재 여부로 권한을 넘기면 ActorToMass 번역기가
  프록시 캡슐을 Transform에 되써서 **인스턴스가 프록시를 따라 지면에 파묻힌다.**
  판단은 `FMassRepresentationFragment::CurrentRepresentation`으로 하고, 클라 Transform을 쓰는
  프로세서는 `SyncWorldToMass` 그룹에서 `ExecuteAfter`로 번역기 뒤에 못 박는다.

- **클라이언트는 적 Actor의 이동을 직접 재시뮬레이션한다.** Mover가 Async 모드 +
  Chaos 물리 예측(`bEnablePhysicsPrediction=True`)이기 때문이다. 따라서 **이동 시뮬레이션이
  참조하는 값은 전부 Mover InputCmd(`FLNPModifierInputs`)를 타야 한다** — 폰의 평범한 컴포넌트
  멤버에 두면 서버에만 값이 있어 클라가 CDO 기본값으로 폴백하고, 위치 오차 임계값을 넘겨
  매번 되감긴다.

- **AI 이동 의도 벡터는 방향만 담는다 — 크기로 속도를 표현하지 않는다.**
  Mover는 의도 벡터를 정규화하지 않고 쓰므로 크기 1 미만을 지속 입력하면 속도가 곱셈 붕괴한다
  (→ [TechDesign_CharacterMovement.md](TechDesign_CharacterMovement.md) §1.1). 속도는 `SetAIDesiredSpeed()`로
  따로 넘긴다. 덕분에 Entity 경로와 Actor 경로가 같은 `ULNPEnemyConfig::MoveSpeed`를 쓰게 되어
  LOD 전환 시 속도가 튀지 않는다.

- **목적지까지의 거리는 접평면 성분으로만 잰다.** 반경 방향 차이(캡슐 중심 보정, 지형 높이차)는
  걸어서 좁힐 수 있는 거리가 아니므로 거리에 포함시키면 도착 판정이 영영 성립하지 않는다.
  임계값은 `FLNPEnemyMovementConfig::ArrivalTolerance` **하나뿐**이다 — MovementProcessor의
  도착 신호, IdleTask의 배회 완료 판정, `ComputeStopDistance()`의 버퍼 하한이 모두 이 값을 본다.
  (`FMassMoveTargetFragment::DistanceToGoal`은 임계값이 아니라 남은 거리다. 혼동 금물.)

- **신호 구동 상태 기계에는 반드시 신호 없이 도는 복구 경로가 있어야 한다.**
  Mass StateTree의 Task Tick은 `StateTreeActivate` 신호가 있어야만 돈다. "도착하면 신호"만 있으면
  **도달 불가능한 목표를 한 번 뽑은 개체는 영구 정지**한다 — Tick이 안 도니 스스로 목표를 바꿀 수 없고,
  타임아웃을 Task 안에 넣어도 그 코드가 실행되지 않는다. 그래서 **시간 측정과 깨우기는 매 프레임 도는
  MovementProcessor**가, **판단은 IdleTask가 단독으로** 한다
  (→ [TechDesign_EnemyNPC_StateTree.md](TechDesign_EnemyNPC_StateTree.md) §3.B.4).

---

## 6. Actor 연동 (High LOD)

**`ALNPEnemyCharacter`** — Config로 초기화되는 범용 셸.

| API | 역할 |
|:---|:---|
| `InitializeOnce(Config)` | ASC·어빌리티·무기 1회 초기화 (`bInitializedOnce` 가드) |
| `SyncFromEntity(Health, State, Velocity)` | 매 활성화: Mass → Actor 주입(HP·속도). **Actor는 표현 풀에서 재사용되므로** 직전 개체의 흔적을 함께 되돌린다 — `ExitRagdoll()`, AI 입력 3종, 조준 Pitch 0 |
| `SyncToEntity(out Health, out Velocity)` | 매 프레임: Actor → Mass 역동기화 |
| `TriggerRagdoll()` | **서버 전용** 사망 진입점 — `Multicast_TriggerRagdoll(PopVelocity)`로 방송한다. 사망 판정이 서버 전용 Mass 프로세서라 방송하지 않으면 클라이언트는 적이 그냥 사라지는 것만 보게 된다. 실제 랙돌은 베이스의 `EnterRagdoll()`/`ExitRagdoll()` (→ [TechDesign_CharacterMovement.md](TechDesign_CharacterMovement.md) §9). `SyncFromEntity`가 매 활성화마다 `ExitRagdoll()`을 불러 풀 재사용을 되돌린다 |
| `SetAimTargetLocation(WorldTarget)` / `ClearAimTarget()` | **서버 전용** 상하 조준 갱신·해제 (아래 §상하 조준) |
| `GetBaseAimRotation()` | 액터 전방에 복제된 로컬 Pitch를 얹은 조준선. Aim Offset과 발사 방향의 **공통 원본** |
| `EnemyConfig` (복제, `OnRep_EnemyConfig`) | 적 무기 상태의 단일 원본. **복제해야 하는 이유:** Actor 스폰·`InitializeOnce`가 서버 전용인데 게스트의 Actor는 일반 Relevancy로 온다 — 없으면 게스트에서 무기 메시·애님 레이어가 안 붙고 `GetActiveWeaponDef()`도 null이 된다. OnRep은 **비주얼만** 갱신한다 |

### HP 바 · 락온 표식

**Actor에 위젯을 달지 않는다.** 순수 엔티티에는 `UWidgetComponent`를 붙일 수 없어 LOD마다 표현이
갈리기 때문이다. 둘 다 HUD의 **스크린 스페이스 마커**로 그리고, 후보 수집은 `ULNPEnemyMarkerProcessor`
(상위 N개) 와 `ULNPTargetQuerySubsystem`(최선 1개)이 맡는다
→ [TechDesign_HUD.md](TechDesign_HUD.md) §11, [TechDesign_TargetQuery.md](TechDesign_TargetQuery.md).

### 상하 조준 (Aim Pitch)

적은 컨트롤러가 없어 `APawn::GetBaseAimRotation()`이 액터 회전(= 수평)을 그대로 돌려준다.
`MoveTarget`은 이동 평면상의 방향이라 Yaw만 만들 수 있으므로, 상하 성분은 별도로 공급해야 한다.

**전달하는 상태는 각도 하나 — `AimPitchDeg`(액터 로컬 좌표계, 복제).** 목표 지점(`FVector`)이 아니다.

```
FLNPEnemyAttackTask::Tick  (서버, TryActivateAttack 바로 앞)
  └─ SetAimTargetLocation(Targeting.TargetLocation)
       └─ 로컬 좌표 변환 → Pitch 추출 → AimPitchMin/MaxDeg 클램프 → TargetAimPitchDeg
ALNPEnemyCharacter::Tick   (서버) AimPitchDeg ←FInterpTo← TargetAimPitchDeg  →복제→ 게스트
GetBaseAimRotation()       (모든 머신) 로컬 Pitch를 액터 트랜스폼으로 월드 복원
  ├─ ULNPAnimInstance → AimPitch → 서브 ABP의 Aim Offset 노드   (외관)
  └─ ULNPAbility_RangedAttack::GetFireDirections                 (판정, 서버)
```

- **왜 로컬 각도인가.** 구면 중력 위에서 월드 Z 기준 Pitch는 의미가 없다. 로컬 값으로 주고받으면
  게스트가 자기 화면의 액터 회전으로 복원해도 같은 자세가 나온다 —
  [TechDesign_Networking.md](TechDesign_Networking.md)의 접평면 로컬 Yaw 인코딩과 같은 계열이다.
- **왜 복제하는가.** 타게팅은 서버 전용 Mass 로직이라 게스트는 이 적이 무엇을 겨누는지 알 방법이 없다.
  보간은 서버만 굴리고 결과를 복제한다 — 게스트가 한 번 더 보간하면 두 화면이 갈라진다.
- **판정과 외관이 같은 값을 쓴다.** 클램프도 한 곳에서만 걸린다. 발사 방향만 따로 계산하면
  겨눈 곳과 맞는 곳이 어긋난다.
- **가용 각도는 `FLNPEnemyMovementConfig::AimPitchMin/MaxDeg` 하나뿐이고 소비처가 셋이다** —
  조준 자세·발사 방향·**피격 인지의 상하 게이트**(§7.9). Actor가 아니라 Config에 두는 이유가
  세 번째 소비처다(Mass 프로세서는 Actor가 없는 Low LOD에서도 돌아야 한다).
- **조준선의 기준점은 캡슐 중심이다.** 총구는 조준 자세에 따라 움직여 자기참조가 된다.
  총구는 캡슐 중심과 거의 같은 높이라 실제 오차는 그립의 좌우 오프셋뿐이고, Yaw에서 이미 감수하던 값이다.
- **Attack 상태에서만 켠다.** `ExitState`의 `ClearAimTarget()`을 빠뜨리면 배회 중에도 하늘을 겨눈 채 걷는다.
  `SyncFromEntity`에서도 0으로 되돌린다 — Actor는 표현 풀에서 재사용되므로 직전 개체의 자세가 따라온다.

---

## 7. 어필 포인트 (트러블슈팅 & 설계 판단)

### 7.1 하나의 이동 프로세서, 두 개의 실행 모드

Actor 상태에서도 이동 결정은 Mass 프로세서가 내리고, 실행만 위임한다 — Actor면 Mover 컴포넌트에 AI Intent를 전달하고(플레이어와 동일한 이동 파이프라인·네트워크 예측 재사용), 엔티티면 Transform을 직접 적분한다. LOD 전환 시 "다른 AI"가 되는 문제가 없다.

### 7.2 LOD 강제는 LOD 값만 — Representation 전환 1회 감지 보장

전투 진입 시 `CurrentRepresentation`을 직접 바꾸지 않고 `FMassRepresentationLODFragment.LOD`(WantedRepresentation의 원천)만 High로 올린다. 엔진 RepresentationProcessor가 전환을 정확히 1회 감지해 스폰/디스폰 수명 주기가 깨지지 않는다.

⚠️ **승격은 더 이상 무조건이 아니다.** `ULNPEnemyConfig::CombatMode`가 `PureEntity`면 이 강제를 건너뛰고,
그 개체는 전투 중에도 순수 엔티티로 남아 Mass 프로세서가 직접 공격한다
(→ [TechDesign_EnemyNPC_LowLOD.md](TechDesign_EnemyNPC_LowLOD.md)). 기본값은 `ActorPromoted`라 기존 거동은 그대로다.

⚠️ **반대 방향으로 LOD를 눌러 승격을 막지는 않는다.** `LOD`는 표현뿐 아니라 유의도·틱 레이트까지 정하는
값이라, 표현 하나를 막으려고 나머지까지 끌어내리게 된다. 승격 차단은 EntityConfig의 `LODRepresentation`이
맡고 코드는 "끌어올리지 않는다"까지만 한다.

### 7.3 넉백 공중 상태를 태그가 아닌 속도로 판단

공중 여부를 `FLNPEnemyAirborneTag` 같은 태그로 분리하면 매 피격/착지마다 디퍼드 태그 추가·제거로 **아키타입 마이그레이션**이 반복된다. 넉백은 저빈도·단기 상태이므로 `PhysVelocity != 0` 분기로 처리 — 비행 Enemy처럼 고빈도·지속 상태가 생기면 그때 태그 분리를 재검토한다는 판단을 코드에 명시했다.

### 7.4 구면 지형의 경사 차단

Mover가 없는 엔티티도 45° 이상 오름 경사를 오르지 못하도록, 현재/목표 지점의 SurfaceCache 표면 좌표 차이에서 "Up 성분 대비 수평 성분 비율"을 검사한다 (`MaxWalkSlopeCosine = 0.71` — Mover CommonLegacySettings와 동일 기준).

### 7.5 Actor 동기화의 의도된 트레이드오프

Actor null 감지와 HP 역동기화를 한 프로세서(ActorSyncProcessor)에서 처리하면, "치명타 + LOD 경계 이탈"이 같은 프레임에 겹칠 때 그 프레임의 피해가 Fragment에 반영되지 않을 수 있다. 발생 확률이 극히 낮아 프로세서 분리 비용 대신 허용 — 트레이드오프를 주석으로 명문화.

### 7.6 잠복 컴파일 버그 — 에디터 전용 프로세서의 #else 분기

UCLASS 선언은 헤더에 무조건 남으므로, `#if WITH_EDITOR` 안에만 구현된 디버그 프로세서는
**비에디터(-game) 빌드에서 링크가 깨진다.** 에디터 타깃만 빌드하는 동안에는 드러나지 않는다.
두 번 밟았다 — 처음에는 `#else` 생성자가 존재하지 않는 멤버를 초기화하고 있었고,
2026-09-06 `ULNPEnemyActionDebugDrawProcessor`는 `#else` 스텁 자체가 없었다.

**규약:** `#if WITH_EDITOR`로 감싼 프로세서를 **추가할 때마다** `#else` 스텁(생성자에서
`bAutoRegisterWithProcessingPhases = false`, 빈 `ConfigureQueries`/`Execute`)을 같은 커밋에 넣는다.
지울 때는 **남은 클래스의 스텁이 그 블록에 있는지** 함께 확인한다 — 지우면서 `#else`가 통째로 비면
다음 사람은 스텁이 원래 없었다는 사실조차 모른다.

### 7.7 엔티티 단위 상태를 대상별 루프 안에서 읽지 말 것

"교전 중인 타겟은 시야 재검사를 면제한다"는 규칙을 `PreviousState == Confirmed`만으로 판정하면,
그 검사가 **플레이어별 루프 안**에 있는 순간 의미가 뒤집힌다. `PreviousState`는 엔티티 하나의
상태이지 "이 플레이어와의 관계"가 아니기 때문에, 한 명에게 Confirmed된 NPC가 **다른 모든
플레이어에 대해서도** 거리·시야각 검사를 건너뛰게 된다. 실제 증상은 "2P와 교전 중이던 NPC가
맵 반대편의 1P에게 달려감"이었고, 한 명이 죽어 그 핸들이 사라지는 순간 남은 전원이 일제히
후보가 되어 "우르르 몰려가는" 형태로 드러났다.

면제 조건은 반드시 **대상까지 함께** 물어야 한다 — `PreviousState == Confirmed && Player == TargetPlayer`.
같은 함정이 "이미 공격 중", "이미 락온됨" 같은 다른 엔티티 단위 플래그에도 그대로 적용된다.

### 7.8 자격 조건에 자기 위치를 넣으면 자기진동한다

리쉬(추격 이탈 제한)를 **NPC 자신의** Pod 거리로 판정하던 시기에 "적이 제자리에서 부들부들 떨며
안절부절"하는 증상이 나왔다. NPC가 리쉬 경계에서 멈추면 그 자리가 곧 판정 경계선이라,
Pod 쪽으로 한 발짝 움직이면 자격이 되살아나 다시 끌려나가고, 나가는 순간 또 자격을 잃는다.

```
DistToPod 5030 → 자격 없음 → Alert  → Pod 쪽으로 걷기
DistToPod 4999 → 자격 부활 → Chase  → 플레이어 쪽으로
DistToPod 5001 → 자격 없음 → Alert  → ...              (매 프레임 반복)
```

**자격 조건이 NPC 자신의 위치를 읽으면 NPC의 행동이 자기 조건을 바꾸는 피드백 루프가 된다.**
히스테리시스(경계에서 바로 풀지 않고 절반까지 돌아와야 풀기)를 걸어도 **진동 주기가 프레임에서
초 단위로 늘어날 뿐 사라지지 않는다** — 실제로 "이탈 → 복귀 → 절반에서 재교전 → 이탈"의 느린 요요가 됐다.

해법은 감쇠를 더 거는 것이 아니라 **기준점을 제어 주체 밖으로 옮기는 것**이다. 세력권을 플레이어의
Pod 거리로 재면 NPC가 무엇을 하든 조건이 변하지 않으므로 진동이 발생할 자리 자체가 없어지고,
복귀 래치·해제 비율 상수·리쉬 점수 감쇠가 전부 불필요해졌다. 같은 함정이 "자기 속도로 자기
가속 여부를 정한다", "자기 상태로 자기 상태 전이 조건을 정한다" 같은 모든 자기참조 판정에 적용된다.

### 7.9 회전축이 하나면 "돌아보면 보인다"가 성립하지 않는다

피격 주시(`HitReactLookTime`)의 설계 의도는 "그 자리에 서서 맞은 방향을 바라본다 → 돌아본 결과
시야에 플레이어가 있으면 평소의 발견 플로우를 그대로 탄다"였다. 별도 전이 규칙이 필요 없는
깔끔한 설계였지만, **고저차에서는 성립하지 않았다.**

몸통의 회전축은 로컬 Up 하나뿐이라 **좌우로만 돌아서는데** 발견 판정의 시야각은 3D 원뿔이다.
위·아래에서 날아온 공격은 아무리 돌아서도 원뿔 안에 들어오지 않아 "돌아본다"와 "본다"의 연결이
끊긴다 — 급경사에서 저격당한 NPC가 **경계 상태에조차 진입하지 못한 채** 계속 배회했다.

해법은 시야를 넓히는 것이 아니라 **시야의 축을 바꾸는 것**이다. 피격 주시 중에는 시야 중심을
정면 벡터가 아니라 `HitReactDirection`(3D)으로 둔다. 각도 예산(`VisionAngle`)은 그대로다.

⚠️ **각도 판정을 빼고 "피격 중이면 다 보인다"로 두면 안 된다.** `HitReactTimer`는 대상별이 아니라
**엔티티 단위** 값이라, 대상을 한정하지 않으면 교전 중 한 대 맞는 것만으로 사거리 안의 다른
플레이어가 전부 후보가 된다 — §7.7과 완전히 같은 함정이다.

**그리고 이 경로에는 상하 게이트가 반드시 붙어야 한다.** 이 경로는 정면 시야 원뿔을 우회하므로,
막지 않으면 조준 클램프 밖(정수리 위·발밑)에서 온 공격까지 인지한다. 그 결과는 "발견은 했는데
겨눌 수는 없어 클램프된 각도로 영원히 헛쏘는" 상태다 — **인지하지 못하는 편이 낫다.**
그래서 게이트는 조준·발사와 **같은 값**(`FLNPEnemyMovementConfig::AimPitchMin/MaxDeg`)을 읽는다.
값을 좁히면 "못 겨누는 각도"와 "못 알아채는 각도"가 함께 움직여 모순이 생기지 않는다.

가용 각도는 **로컬 수평면 기준** ∓75°(기본값)이라 남는 사각은 로컬 Up/Down에서 15° 이내의 좁은
원뿔뿐이다. 입체각이 작아 **의도적으로 남긴다** — 여기서 맞으면 NPC는 반응하지 않는 것이 정의된 동작이다.
상한의 근거는 Aim Offset 에셋의 한계가 아니라 게임플레이 판단이다(실측상 AO 자세는 거의 수직까지 나온다).

### 7.10 적 Actor의 표현 소유권은 넷 모드마다 하나뿐이다

`FMassActorFragment`는 **엔티티 하나당 Actor 하나**를 담는 자리이고, 엔진은 그 자리를 채우는 경로가
자기 하나뿐이라고 단정한다(`UMassAgentComponent::SetEntityHandleInternal`의 `checkf`).
그런데 게스트에는 그 자리를 노리는 경로가 **둘** 있었다.

1. **복제 퍼펫 링크** — 서버가 승격시킨 적 Actor가 릴러번트가 되어 도착하면
   `UMassAgentComponent::NetID`가 복제되고, `OnRep_NetID`가 NetID로 엔티티를 찾아 프래그먼트에 자기를 쓴다.
   (같은 NetID로 `OnRep_NetID`가 두 번 오면 엔진이 즉사하므로 `ULNPMassAgentComponent`가 두 번째 등록을
   건너뛴다. 플레이어 쪽 NetID 캐싱 타이밍 갭 보정도 같은 서브클래스에 있다.)
2. **게스트 자체의 표현 LOD 승격** — `UMassCrowdVisualizationProcessor`의 실행 플래그는
   `Client | Standalone`이라 **게스트에서 그대로 돈다.** 가까워진 적을 게스트가 스스로
   Actor로 스폰하고(= Mass 소유) 같은 프래그먼트를 채운다.

2가 먼저 서 있으면 1이 assert로 죽는다. 반대 순서는 엔진이 흡수한다(표현 프로세서는
Mass 소유가 아닌 Actor를 만나면 새로 스폰하지 않고 **그것을 재사용**한다).

> ⚠️ 게스트가 스스로 승격시킨 Actor는 애초에 **쓸모가 없었다.**
> `ULNPEnemyActorInitializerProcessor`가 클라이언트에서 조기 반환하므로 `InitializeOnce`·
> `SyncFromEntity`가 돌지 않는다 — 무기도 HP 바도 없는 빈 껍데기다. 적 Actor는 서버 권위이고
> 게스트는 복제본을 받으므로, 게스트 쪽 승격은 처음부터 중복이었다.

**그래서 게스트에서는 표현 소유권을 복제 Actor 하나로 못박는다.** 넷 모드마다 **프로세서를 나눈다:**

| 프로세서 | 페이즈 | 표현 LOD 처리 |
|:---|:---|:---|
| `ULNPEnemyLODOverrideProcessor` (서버·Standalone) | **PostPhysics** | 전투 진입(`Confirmed`)이면 `High` 강제 — §7.2 |
| `ULNPEnemyClientRepresentationProcessor` (게스트) | **PrePhysics** | 복제 Actor가 붙어 있으면 `High`(그 Actor를 표현으로 채택), 아니면 **Actor를 쓰지 않는 첫 LOD 단계까지 하향** |

하향 목표는 상수가 아니라 `FMassRepresentationParameters::LODRepresentation`를 훑어
`HighRes/LowResSpawnedActor`가 아닌 첫 단계를 찾는다 — EntityConfig에서 단계별 표현을 바꿔도
"게스트는 Actor를 스폰하지 않는다"는 불변식이 따라온다.

#### ⚠️ 둘을 한 프로세서에 담을 수 없다 — **페이즈가 다르기 때문이다**

이 분리는 취향이 아니라 필수다. 처음에는 한 클래스에 넷 모드로 분기해 넣었는데,
**그 클라이언트 분기는 한 번도 동작하지 않았다**(2026-09-05 실측). 세 사실이 겹친 결과다.

- 서버 분기는 판단 근거(타게팅 상태)를 `ULNPEnemyScoringProcessor`(**PostPhysics**)가 채우므로 그보다 뒤여야 한다.
- 표현을 실제로 정하는 `UMassCrowdVisualization*Processor`는 `ProcessingPhase`를 설정하지 않아
  **엔진 기본값 PrePhysics**로 돈다.
- **Mass는 프로세서를 페이즈별로 따로 버킷팅해 독립적으로 의존성을 해소한다**(`MassEntitySettings.cpp`) —
  따라서 **`ExecuteAfter`/`ExecuteBefore`는 페이즈를 건너지 못하고 조용히 무시된다.**

```
PrePhysics  LOD 계산      → LOD = Medium (거리 기반)
PrePhysics  표현 결정      → Medium = Actor → 게스트가 Actor 스폰
PostPhysics 우리 클램프    → LOD = Low   ← 아무도 안 읽고 다음 프레임에 덮인다
```

증상은 게스트만 가끔 죽는 것뿐이고 **경고도 크래시 로그의 단서도 없다.** 잡아낸 방법은
"게스트에 `IsOwnedByMass()`인 Actor가 하나라도 있는가"를 세는 상시 감시였다 —
수정 전 무입력 20초에 **1,819건**, 수정 후 고밀도 전투에서 **0건**(적 Actor 467기 교체).

> **규약:** LOD·표현처럼 **엔진이 이미 도는 체인에 끼어드는 프로세서는 페이즈부터 맞춘다.**
> 그룹과 `ExecuteBefore`만 맞으면 된다고 보면, 선언은 그럴듯한데 실행은 안 되는 상태가 된다.
> 하위 그룹도 함께 못 박을 것 — `UMassVisualizationProcessor`는 `Representation`이 아니라
> `Representation.VisualizationProcessing`에 들어간다.

이 처방은 엔진의 `FMassRepresentationParameters::bForceActorRepresentationForExternalActors`와
같은 의도이며, 그 플래그가 못 막는 **"게스트가 먼저 스폰한" 순서**까지 함께 닫는다.
데이터 에셋 체크박스가 아니라 LOD 쪽에 둔 이유가 이것이다.

---

## 8. 미구현 항목

| 항목 | 설명 |
|:---|:---|
| 시야각·상태 가중치 | `FLNPEnemyTargetingConfig::DistanceWeight`·`AngleWeight` **둘 다 읽는 곳이 없다** — 점수 공식은 `1,000,000 / (거리 + 1)` 고정. 시야각·공격 상태 가중치 보강 예정 |
| **루팅 방해** | 루팅 중인 플레이어를 최우선 타겟으로 끌어당긴다 (→ [GameDesign_EnemyNPC.md](GameDesign_EnemyNPC.md) §4.2). 점수 공식이 루팅 상태를 보지 않는다. 위 가중치 항목과 **같은 축**이므로 함께 설계할 것 |
| Pod 파괴 후처리 | `FLNPEnemyFragment::ParentLootPod` 핸들을 읽는 곳이 없다. Pod이 `Popped`돼도 `ParentPodLocation`이 남아 세력권이 유지된다 (→ [GameDesign_EnemyNPC.md](GameDesign_EnemyNPC.md) §7) |
| 난이도 스케일링 | 잔여 LootPod 수 기반 NPC 강화 (슬롯 한도 또는 능력치 단계 조정) |
| 원거리 적 반격 | 원거리 적도 슬롯을 얻어야 공격하므로, 세력권 밖에서 저격당하면 바라보기만 하고 반격하지 못한다 (→ [GameDesign_EnemyNPC.md](GameDesign_EnemyNPC.md) §5.3) |
| 적이 **패링하는** 쪽 | `ULNPEnemyTrait`가 `FLNPParryStateFragment`를 붙이지 않아 적은 패링할 수 없다(패링하는 쪽은 플레이어 전용). **패링당하는 쪽은 LOD와 무관하게 이미 동작한다** (→ [TechDesign_ParrySystem.md](TechDesign_ParrySystem.md)) |
