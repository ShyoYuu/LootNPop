# Enemy NPC 시스템 기술 설계

## 1. 한눈에 보기

수백~수천 유닛을 전제로 한 **MassEntity ↔ Actor 하이브리드** 구조. 평상시에는 순수 엔티티(ISM 비주얼)로 시뮬레이션되고, 전투 진입(Confirmed) 시 거리와 무관하게 High LOD Actor로 전환되어 GAS 전투·패링·애니메이션이 활성화된다.

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

| 타입 | 내용 |
|:---|:---|
| `FLNPEnemyFragment` | Health/MaxHealth/Defense, DeathCountdown, EnemyTypeTag, ParentLootPod(+위치 — 세력권 기준), HitReactTimer/Direction(피격 반응 — §3.5) |
| `FLNPEnemySharedFragment` | `ULNPEnemyConfig` 포인터 (동일 타입 공유 — ConstShared) |
| `FLNPEnemyTargetingCandidateFragment` | 인식된 잠재 타겟 최대 4명 (거리 정렬) + `AlertDwellTime`(경계 인내) + `DisengageTimer`(재발견 금지 잔여). **뒤 두 float는 Reset 대상이 아니다** — §3.4 |
| `FLNPEnemyTargetingFragment` | 최종 타겟, `ELNPTargetingState`(None/Alert/Confirmed), 마지막 타겟 위치, 거리² |
| `FLNPEnemyIdleFragment` | 배회 타이머·플래그 |
| `FLNPEnemyVelocityFragment` | Entity 모드 물리 속도 (넉백 포물선). 접지 시 0 |
| Tags | `FLNPEnemyTag` / `FLNPPlayerTag`(쿼리 분류), `FLNPEnemyActorInitializedTag`(초기화 마커), `FLNPEnemyDyingTag`(소멸 대기), `FLNPPlayerDeadTag`(사망 플레이어 — 타게팅 제외) |

### 2.2 ULNPEnemyConfig (Data Asset)

EnemyTypeTag / EnemyActorClass / StateTree / WeaponData / DefaultAbilities / InitialAttributeValues / 캡슐 크기 + 서브 구조체 2종:

- **`FLNPEnemyTargetingConfig`**: 인지 거리 3종 (AwarenessDistance / VisionDistance+Angle / AlertRetentionDistance) + 세력권 반경 ChaseRadius + 시간 3종 (AlertPatienceTime / AlertRecoveryTime / HitReactLookTime), Distance/AngleWeight — §3.3~3.5
- **`FLNPEnemyMovementConfig`**: MoveSpeed, RotationRate, Gravity, Wander 반경, AttackRange/Interval + **`ComputeStopDistance()`** — 추격 정지 거리 공식의 단일 정의 (TargetFollow·Movement·SteeringTask가 공유)

`ULNPEnemyTrait`가 이 Config를 SharedFragment로 묶어 엔티티 템플릿을 구성하고, MassReplication Trait(BubbleInfo/Replicator 고정)를 내부 위임한다.

---

## 3. 슬롯 기반 타겟팅

### 3.1 ULNPTargetingSubsystem

플레이어별 교전 밀도를 제한하는 전역 서브시스템. `MaxMeleeSlotsPerPlayer = 10`, `MaxRangedSlotsPerPlayer = 20` (에디터 설정 가능).

```
매 프레임:
  ScoringProcessor → RegisterEnemyInterest(Enemy, Player, Score, bIsMelee)  ← 게임 스레드 커맨드로 지연 등록
  TargetingProcessor → RebalanceSlots()
      전체 등록 항목을 점수 내림차순 정렬 → 그리디 할당
      (Enemy 1개는 1개 슬롯만, 슬롯 한도 초과 시 탈락)
  → IsSlotConfirmed()로 각 Enemy가 결과 조회
```

`FCriticalSection`으로 Mass Worker Thread 경합을 방지한다.

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

`bIsMelee`는 슬롯 한도(10/20)만 가르며, 플레이어 수가 적을 때는 한도에 걸릴 일이 없어
거동에 관여하지 않는다. 즉 "근접은 안 돌아보고 원거리는 돌아본다"로 보이는 것은
같은 규칙의 서로 다른 입력값일 뿐이다.

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

**경계 인내 — 시간도 강등 축이다.** 거리만으로는 사다리가 닫히지 않는다. 플레이어가 세력권 바로
바깥에 서 있으면 NPC는 싸우지도(자격 없음) 잊지도(시야 안) 못한 채 굳는다. 그래서
`FLNPEnemyTargetingCandidateFragment::AlertDwellTime`에 "추격도 못 하면서 경계만 하고 있는" 시간을
누적하고, `AlertPatienceTime`(8초)에 도달하면 **그 프레임에 유지 조건까지 끊어** `None`으로 내려보낸다.

- **발견만 막으면 안 된다.** 유지 조건을 남겨 두면 추적 중인 타겟이 유지 거리 안에 계속 있어
  경계가 영원히 풀리지 않는다. 소진 프레임에는 초근접을 제외한 **모든 인식 경로를 끈다.**
- **슬롯 대기 중인 개체에는 타이머가 돌지 않는다.** 추격 자격이 하나라도 있으면(`bAnyChaseEligible`)
  0으로 초기화한다 — 그 상태는 "전투 대기열"이지 "멀뚱히 보고 있음"이 아니다. 시간으로 흩어 버리면
  큰 무리와의 교전이 말라 버린다. `Confirmed`·`None`일 때도 초기화되므로
  **"교전에 성공하면 인내는 새로 시작"** 이 자동으로 성립한다(공격을 특수 처리할 필요가 없다).
- **피격 반응 중에는 재지 않는다** — 맞고 두리번거리는 시간은 대치가 아니다.

### 3.4 재발견 금지 창 — 등을 돌릴 시간을 벌어 준다

인내 소진만으로는 아무것도 해결되지 않는다. 포기한 그 프레임에도 플레이어는 여전히 정면 시야
안에 있으므로 **다음 프레임에 곧바로 재발견되어** `None` → `Alert`로 되돌아온다. 8초마다 상태가
왕복할 뿐이고, NPC는 Pod 쪽으로 한 발짝도 못 걷는다.

`FLNPEnemyTargetingCandidateFragment::DisengageTimer`가 이 구멍을 막는다. 소진한 프레임에
`AlertRecoveryTime`(1초)으로 세팅되고 매 프레임 감소하며, 0보다 큰 동안 **시야 발견만** 건너뛴다.

| | 내용 |
|:---|:---|
| **세팅** | `AlertDwellTime >= AlertPatienceTime`인 프레임 |
| **효과** | `VisionDistance` + FOV 경로를 건너뛴다. 유지·초근접 경로에는 관여하지 않는다 |
| **해제** | 매 프레임 `DeltaTime` 감소, 0에서 정지 |

- **이 창이 벌어 주는 것은 이동이 아니라 회전이다.** Idle이 된 NPC는 IdleTask가 뽑는 Pod 주변
  배회 목표를 향해 돌아서고, 시야각 절반(45°)만 돌면 플레이어가 이미 FOV 밖이다.
  등속 회전 `RotationRate` 360°/s 기준 45°는 0.125초, 완전한 180°도 0.5초 — 1초는 2배 버퍼다.
- **초근접(`AwarenessDistance`)은 이 금지를 무시한다.** 회복 중이라고 눈앞의 플레이어를 못 보는
  장님이 되면 "때려도 반응 없는 적"이 되어 더 어색해진다.
- **인내(`AlertDwellTime`)와 별도의 float로 둔다.** 하나에 겹치면 값 하나만 보고는 "차오르는 중인지
  회복 중인지" 구분할 수 없어 계측이 그대로 함정이 된다.

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

Melee/Ranged 분류는 EnemyTypeTag의 "Melee" 포함 여부로 청크당 1회만 판정 (루프 내 문자열 비교 방지).
---

## 4. Mass 프로세서 파이프라인 (10종)

| 프로세서 | 단계 | 역할 |
|:---|:---|:---|
| `ULNPEnemyScoringProcessor` | PostPhysics (UpdateWorldFromMass) | 인식 + 후보 4명 정렬 + 슬롯 점수 등록 |
| `ULNPEnemyTargetingProcessor` | Behavior | `RebalanceSlots()` 호출 → State 동기화(Confirmed/Alert/None) → 변경 시 StateTree 신호 |
| `ULNPEnemyTargetFollowProcessor` | Behavior (Targeting 이후) | MoveTarget 목적지 산출 (정지 거리 반영), 공격 루프용 StateTree 신호 |
| `ULNPEnemyMovementProcessor` | Movement | 실제 이동/회전 적용 — §5 상세 |
| `ULNPHealthProcessor` | PostPhysics | HP ≤ 0 → DyingTag + `TriggerRagdoll()` (전 클라이언트 방송) + `DeathCountdown = ULNPSettings::EnemyRagdollDuration` |
| `ULNPEnemyDeathTimerProcessor` | PostPhysics (Health 이후) | DeathCountdown 만료 엔티티 파괴 |
| `ULNPEnemyLODOverrideProcessor` | LOD (VisualizationLOD 이후, Representation 이전) | 서버: Confirmed면 `RepresentationLOD.LOD = High` 강제 — §7.2 / 클라이언트: 복제 Actor만 표현으로 채택 — §7.10 |
| `ULNPEnemyActorInitializerProcessor` | PostPhysics (Representation 이후) | 신규 스폰 Actor에 `InitializeOnce` + `SyncFromEntity` → InitializedTag 부여 |
| `ULNPEnemyActorSyncProcessor` | PostPhysics (LOD 이전, 게임 스레드) | Actor 유효: `SyncToEntity`(HP·속도 역동기화) / null: InitializedTag 제거 → 재초기화 유도 |
| `ULNPEnemyDebugDrawProcessor` | 에디터 전용 | 상태별 색상 박스(대기 초록/경계 노랑/추격 파랑/공격 빨강) + 전방 화살표 |

---

## 5. Entity 이동 시뮬레이션 (MovementProcessor)

상태별 속도/방향 결정 후, **Actor 모드와 Entity 모드로 분기**한다.

```
Actor 모드 (High LOD):
  → SetAIOrientationIntent / SetAIMoveInput 위임 (게임 스레드 커맨드)
  → 실제 이동은 캐릭터의 Mover 컴포넌트가 처리 (플레이어와 동일 파이프라인)

Entity 모드 (Low LOD):
  ├─ PhysVelocity ≠ 0 (공중 — 넉백/포물선):
  │    중력 적분 → SurfaceCache로 착지 판정 → 착지 시 표면 스냅 + 속도 0
  └─ 접지:
       QInterpConstantTo 회전 (RotationRate) → 경사 차단(§7.4) → SurfaceCache 표면 스냅 이동
```

- 구형 UpDir은 `(GravityOrigin - Location).GetSafeNormal()`로 실시간 계산 (Fragment 저장 없음 — 캐시 효율).
- 지표면 좌표는 전부 `ULNPSurfaceCacheSubsystem` O(1) 조회 (→ [TechDesign_SurfaceCache.md](TechDesign_SurfaceCache.md)).

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
  `ALNPEnemyCharacter`는 `bReplicates = true`(ASC 복제용)라 서버가 승격시킨 적 Actor는
  **게스트에도 시뮬레이티드 프록시로 내려온다.** 게스트가 멀어서 ISM으로 그리는 동안에도
  `FMassActorFragment`는 채워져 있으므로, 존재 여부로 권한을 넘기면 ActorToMass 번역기가
  프록시 캡슐을 Transform에 되써서 **ISM이 프록시를 따라 지면에 파묻힌다.**
  `FMassRepresentationFragment::CurrentRepresentation`이 `HighRes`/`LowResSpawnedActor`인지로
  판단하고, 클라 Transform을 쓰는 프로세서는 `SyncWorldToMass` 그룹에서
  `ExecuteAfter`로 번역기 뒤에 못 박는다.
  (§7.10 이후 게스트에서는 복제 Actor가 붙는 즉시 표현도 Actor로 올라가므로 두 상태가 갈리는
  구간 자체가 좁아졌지만, 판단 기준은 그대로 `CurrentRepresentation`이다.)

- **클라이언트는 적 Actor의 이동을 직접 재시뮬레이션한다.** Mover가 Async 모드 +
  Chaos 물리 예측(`bEnablePhysicsPrediction=True`)이기 때문이다. 따라서 **이동 시뮬레이션이
  참조하는 값은 전부 Mover InputCmd(`FLNPModifierInputs`)를 타야 한다** — 폰의 평범한 컴포넌트
  멤버에 두면 서버에만 값이 있어 클라가 CDO 기본값으로 폴백하고, 위치 오차 임계값을 넘겨
  매번 되감긴다.

- **AI 이동 의도 벡터는 방향만 담는다 — 크기로 속도를 표현하지 않는다.**
  Mover의 `UMovementUtils::ComputeVelocity`는 방향 전환 항에서 의도 벡터를 **정규화하지 않고** 쓴다
  (UE `CharacterMovementComponent`는 같은 자리에서 `GetSafeNormal()`을 쓴다).
  그래서 크기 `s`(<1)를 지속적으로 넣으면 매 프레임 속도가 `s`배로 깎여
  평형 속도가 `Acceleration × s × dt / (1 − s)`까지 주저앉는다 — 실측 180cm/s 기대치가 27cm/s로 나왔다.
  속도는 `SetAIDesiredSpeed()`로 넘겨 `FLNPMoveSpeedModifier`가 `MaxSpeed`에 반영한다.
  이 규약 덕분에 Entity 경로와 Actor 경로가 같은 `ULNPEnemyConfig::MoveSpeed`를 쓰게 되어
  LOD 전환 시 속도가 튀지 않는다.

- **목적지까지의 거리는 접평면 성분으로만 잰다.** 반경 방향 차이(캡슐 중심 보정, 지형 높이차)는
  걸어서 좁힐 수 있는 거리가 아니므로 거리에 포함시키면 도착 판정이 영영 성립하지 않는다.
  임계값은 `FLNPEnemyMovementConfig::ArrivalTolerance` **하나뿐**이다 — MovementProcessor의
  도착 신호, IdleTask의 배회 완료 판정, `ComputeStopDistance()`의 버퍼 하한이 모두 이 값을 본다.
  (`FMassMoveTargetFragment::DistanceToGoal`은 임계값이 아니라 남은 거리다. 혼동 금물.)

- **신호 구동 상태 기계에는 반드시 신호 없이 도는 복구 경로가 있어야 한다.**
  Mass StateTree의 Task Tick은 `StateTreeActivate` 신호가 있어야만 돈다. 그래서 "도착하면 신호"만
  있으면 **도달 불가능한 목표를 한 번 뽑은 개체는 영구 정지**한다 — Tick이 안 도니 스스로 목표를
  바꿀 수 없고, 타임아웃을 Task 안에 넣어도 그 코드가 실행되지 않는다.
  배회는 이 구조를 이렇게 푼다: 시간 측정과 깨우기는 **매 프레임 도는 MovementProcessor**가 맡고
  (`FLNPEnemyIdleFragment::TimeSinceWanderIssued` 누적 → `WanderTimeout` 초과 시
  `bWanderTargetTimedOut` + 신호), 목표를 폐기·재추첨하는 **판단은 IdleTask가 단독으로** 한다.

---

## 6. Actor 연동 (High LOD)

**`ALNPEnemyCharacter`** — Config로 초기화되는 범용 셸.

| API | 역할 |
|:---|:---|
| `InitializeOnce(Config)` | ASC·어빌리티·무기 1회 초기화 (`bInitializedOnce` 가드) |
| `SyncFromEntity(Health, State, Velocity)` | 매 활성화: Mass → Actor 주입 (HP Bar 초기값 포함) |
| `SyncToEntity(out Health, out Velocity)` | 매 프레임: Actor → Mass 역동기화 |
| `TriggerRagdoll()` | **서버 전용** 사망 진입점 — `Multicast_TriggerRagdoll(PopVelocity)`로 방송한다. 사망 판정이 서버 전용 Mass 프로세서라 방송하지 않으면 클라이언트는 적이 그냥 사라지는 것만 보게 된다. 실제 랙돌은 베이스의 `EnterRagdoll()`/`ExitRagdoll()` (→ [TechDesign_CharacterMovement.md](TechDesign_CharacterMovement.md) §9). `SyncFromEntity`가 매 활성화마다 `ExitRagdoll()`을 불러 풀 재사용을 되돌린다 |
| `SetLockOnMarkerVisible()` | 락온 표식 위젯 토글 (LockOnComponent가 호출) |
| `SetAimTargetLocation(WorldTarget)` / `ClearAimTarget()` | **서버 전용** 상하 조준 갱신·해제 (아래 §상하 조준) |
| `GetBaseAimRotation()` | 액터 전방에 복제된 로컬 Pitch를 얹은 조준선. Aim Offset과 발사 방향의 **공통 원본** |

### 월드 스페이스 HP Bar

- `UWidgetComponent`(World Space) + `ULNPHpBarWidget`(BindWidget `HpBar` ProgressBar).
- 가시 조건 `0 < HP < MaxHP`. 스폰 시 `SyncFromEntity`, 전투 중 ASC Health 변경 델리게이트로 갱신.
- BP CDO에서 `HpBarWidgetClass = WBP_LNPHpBar` 지정.

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

### 7.3 넉백 공중 상태를 태그가 아닌 속도로 판단

공중 여부를 `FLNPEnemyAirborneTag` 같은 태그로 분리하면 매 피격/착지마다 디퍼드 태그 추가·제거로 **아키타입 마이그레이션**이 반복된다. 넉백은 저빈도·단기 상태이므로 `PhysVelocity != 0` 분기로 처리 — 비행 Enemy처럼 고빈도·지속 상태가 생기면 그때 태그 분리를 재검토한다는 판단을 코드에 명시했다.

### 7.4 구면 지형의 경사 차단

Mover가 없는 엔티티도 45° 이상 오름 경사를 오르지 못하도록, 현재/목표 지점의 SurfaceCache 표면 좌표 차이에서 "Up 성분 대비 수평 성분 비율"을 검사한다 (`MaxWalkSlopeCosine = 0.71` — Mover CommonLegacySettings와 동일 기준).

### 7.5 Actor 동기화의 의도된 트레이드오프

Actor null 감지와 HP 역동기화를 한 프로세서(ActorSyncProcessor)에서 처리하면, "치명타 + LOD 경계 이탈"이 같은 프레임에 겹칠 때 그 프레임의 피해가 Fragment에 반영되지 않을 수 있다. 발생 확률이 극히 낮아 프로세서 분리 비용 대신 허용 — 트레이드오프를 주석으로 명문화.

### 7.6 잠복 컴파일 버그 — 에디터 전용 프로세서의 #else 분기

디버그 프로세서의 비에디터(`#else`) 생성자가 존재하지 않는 멤버를 초기화하고 있었다. `WITH_EDITOR` 빌드에서는 컴파일되지 않는 경로라 에디터 개발 중에는 무해하지만 패키징(-game) 빌드를 깨뜨리는 잠복 버그 — 조건부 컴파일 분기는 양쪽 모두 주기적 빌드 검증이 필요하다는 교훈.

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
안절부절"하는 증상이 나왔다. 원리는 단순하다 — NPC가 리쉬 경계에서 멈추면 그 자리가 곧 판정 경계선이고,
Pod 쪽으로 한 발짝 움직이는 순간 자격이 되살아나 다시 끌려나가고, 나가는 순간 또 자격을 잃는다.

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

몸통의 회전축은 로컬 Up 하나뿐이라 `OrientationIntent`는 접평면 벡터다 — 즉 **좌우로만 돌아선다.**
그런데 발견 판정의 시야각은 3D 원뿔이다. 위·아래에서 날아온 공격은 아무리 돌아서도 원뿔 안에
들어오지 않으므로, "돌아본다"와 "본다" 사이의 연결이 끊긴다. 급경사에서 저격당한 NPC가
**경계 상태에조차 진입하지 못한 채** 계속 배회하는 것이 실제 증상이었다.

해법은 시야를 넓히는 것이 아니라 **시야의 축을 바꾸는 것**이다. 피격 주시 중에는 시야 중심을
정면 벡터가 아니라 `HitReactDirection`(3D)으로 둔다. 각도 예산(`VisionAngle`)은 그대로라
넓어지지 않고, 축만 "맞은 쪽"으로 옮겨간다.

⚠️ **각도 판정을 빼고 "피격 중이면 다 보인다"로 두면 안 된다.** `HitReactTimer`는 대상별이 아니라
**엔티티 단위** 값이라, 대상을 한정하지 않으면 교전 중 한 대 맞는 것만으로 사거리 안의 다른
플레이어가 전부 후보가 된다 — §7.7과 완전히 같은 함정이다.

**그리고 이 경로에는 상하 게이트가 반드시 붙어야 한다.** 이 경로는 정면 시야 원뿔을 우회하므로,
막지 않으면 조준 클램프 밖(정수리 위·발밑)에서 온 공격까지 인지한다. 그 결과는 "발견은 했는데
겨눌 수는 없어 클램프된 각도로 영원히 헛쏘는" 상태다 — **인지하지 못하는 편이 낫다.**
그래서 게이트는 조준·발사와 **같은 값**(`FLNPEnemyMovementConfig::AimPitchMin/MaxDeg`)을 읽는다.
값을 좁히면 "못 겨누는 각도"와 "못 알아채는 각도"가 함께 움직여 모순이 생기지 않는다.

가용 각도는 **로컬 수평면 기준** ∓75°(기본값)이므로, 남는 사각은 로컬 Up/Down에서 15° 이내의
좁은 원뿔뿐이다. 입체각이 작아 실전에서 파고들 여지가 크지 않다고 보고 **의도적으로 남긴다** —
여기서 맞으면 NPC는 반응하지 않는 것이 정의된 동작이다.

상한을 정하는 근거는 Aim Offset 에셋의 한계가 아니라 **게임플레이 판단**이다. 플레이어 기준
실측상 AO 자세는 거의 수직까지 무리 없이 나오므로, 애니메이션이 병목이 되지는 않는다.

### 7.10 적 Actor의 표현 소유권은 넷 모드마다 하나뿐이다

`FMassActorFragment`는 **엔티티 하나당 Actor 하나**를 담는 자리이고, 엔진은 그 자리를 채우는
경로가 자기 하나뿐이라고 단정한다(`UMassAgentComponent::SetEntityHandleInternal`의
`checkf(!ActorInfo->IsValid())`). 그런데 게스트에는 그 자리를 노리는 경로가 **둘** 있었다.

1. **복제 퍼펫 링크** — 서버가 승격시킨 적 Actor가 릴러번트가 되어 도착하면
   `UMassAgentComponent::NetID`가 복제되고, `OnRep_NetID`가 NetID로 엔티티를 찾아 프래그먼트에 자기를 쓴다.
2. **게스트 자체의 표현 LOD 승격** — `UMassCrowdVisualizationProcessor`의 실행 플래그는
   `Client | Standalone`이라 **게스트에서 그대로 돈다.** 가까워진 적을 게스트가 스스로
   Actor로 스폰하고(= Mass 소유) 같은 프래그먼트를 채운다.

2가 먼저 서 있으면 1이 assert로 죽는다. 반대 순서는 엔진이 흡수한다(표현 프로세서는
Mass 소유가 아닌 Actor를 만나면 새로 스폰하지 않고 **그것을 재사용**한다).

> ⚠️ 게스트가 스스로 승격시킨 Actor는 애초에 **쓸모가 없었다.**
> `ULNPEnemyActorInitializerProcessor`가 클라이언트에서 조기 반환하므로 `InitializeOnce`·
> `SyncFromEntity`가 돌지 않는다 — 무기도 HP 바도 없는 빈 껍데기다. 적 Actor는 서버 권위이고
> 게스트는 복제본을 받으므로, 게스트 쪽 승격은 처음부터 중복이었다.

**그래서 게스트에서는 표현 소유권을 복제 Actor 하나로 못박는다.** `ULNPEnemyLODOverrideProcessor`가
넷 모드에 따라 갈라져(같은 프로세서가 `EProcessorExecutionFlags::All`로 클라이언트에서도 돈다):

| 넷 모드 | 표현 LOD 처리 |
|:---|:---|
| 서버·Standalone | 전투 진입(`Confirmed`)이면 `High` 강제 — §7.2 |
| 클라이언트 | 복제 Actor가 붙어 있으면 `High`(그 Actor를 표현으로 채택), 아니면 **Actor를 쓰지 않는 첫 LOD 단계까지 하향** |

하향 목표는 상수가 아니라 `FMassRepresentationParameters::LODRepresentation`를 훑어
`HighRes/LowResSpawnedActor`가 아닌 첫 단계를 찾는다 — EntityConfig에서 단계별 표현을 바꿔도
"게스트는 Actor를 스폰하지 않는다"는 불변식이 따라온다.

⚠️ **LOD 값을 덮어쓰는 프로세서는 그 값을 계산하는 프로세서 뒤에 못 박아야 한다.**
둘 다 `LOD` 그룹 안이라 명시하지 않으면 순서가 정해지지 않는다
(`ExecuteAfter("MassCrowdVisualizationLODProcessor")`).

이 처방은 엔진의 `FMassRepresentationParameters::bForceActorRepresentationForExternalActors`와
같은 의도이며, 그 플래그가 못 막는 **"게스트가 먼저 스폰한" 순서**까지 함께 닫는다.
데이터 에셋 체크박스가 아니라 LOD 쪽에 둔 이유가 이것이다.

---

## 8. 미구현 항목

| 항목 | 설명 |
|:---|:---|
| 시야각·상태 가중치 | `AngleWeight` 필드는 Config에 존재하나 점수 공식은 거리만 사용. 시야각·공격 상태 가중치 보강 예정 |
| 난이도 스케일링 | 잔여 LootPod 수 기반 NPC 강화 (슬롯 한도 또는 능력치 단계 조정) |
| 원거리 적 반격 | 원거리 적도 슬롯을 얻어야 공격하므로, 세력권 밖에서 저격당하면 바라보기만 하고 반격하지 못한다 (→ [GameDesign_EnemyNPC.md](GameDesign_EnemyNPC.md) §5.3) |
| Enemy 패링 | `FLNPParryStateFragment`를 Enemy 엔티티에 연결 + StateTree/GA 갱신 경로 (→ [TechDesign_ParrySystem.md](TechDesign_ParrySystem.md)) |
