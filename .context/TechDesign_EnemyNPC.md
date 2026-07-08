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
| `FLNPEnemyFragment` | Health/MaxHealth/Defense, DeathCountdown, EnemyTypeTag, ParentLootPod(+위치 — 리쉬 기준) |
| `FLNPEnemySharedFragment` | `ULNPEnemyConfig` 포인터 (동일 타입 공유 — ConstShared) |
| `FLNPEnemyTargetingCandidateFragment` | 인식된 잠재 타겟 최대 4명 (거리 정렬) |
| `FLNPEnemyTargetingFragment` | 최종 타겟, `ELNPTargetingState`(None/Alert/Confirmed), 마지막 타겟 위치, 거리² |
| `FLNPEnemyIdleFragment` | 배회 타이머·플래그 |
| `FLNPEnemyVelocityFragment` | Entity 모드 물리 속도 (넉백 포물선). 접지 시 0 |
| Tags | `FLNPEnemyTag` / `FLNPPlayerTag`(쿼리 분류), `FLNPEnemyActorInitializedTag`(초기화 마커), `FLNPEnemyDyingTag`(소멸 대기) |

### 2.2 ULNPEnemyConfig (Data Asset)

EnemyTypeTag / EnemyActorClass / StateTree / WeaponData / DefaultAbilities / InitialAttributeValues / 캡슐 크기 + 서브 구조체 2종:

- **`FLNPEnemyTargetingConfig`**: MaxTargetingDistance, MaxLeashDistance, VisionDistance/Angle, AwarenessDistance, Distance/AngleWeight
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
Score = 1,000,000 / (거리 + 1) × LeashFactor
LeashFactor = clamp(1 - DistToPod / MaxLeashDistance, 0, 1)²   ← Pod에서 멀어질수록 제곱 감쇠
```

**인식 조건** (하나라도 충족 시 후보 등록):
- 이미 `Confirmed` 상태 → 무조건 유지 (시야 재검사 없음 — 등 뒤로 돌아도 어그로 유지)
- `AwarenessDistance` 이내 → FOV 무관 감지
- `VisionDistance` 이내 + `VisionAngle/2` 이내 → 시야 감지

Melee/Ranged 분류는 EnemyTypeTag의 "Melee" 포함 여부로 청크당 1회만 판정 (루프 내 문자열 비교 방지).

---

## 4. Mass 프로세서 파이프라인 (10종)

| 프로세서 | 단계 | 역할 |
|:---|:---|:---|
| `ULNPEnemyScoringProcessor` | PostPhysics (UpdateWorldFromMass) | 인식 + 후보 4명 정렬 + 슬롯 점수 등록 |
| `ULNPEnemyTargetingProcessor` | Behavior | `RebalanceSlots()` 호출 → State 동기화(Confirmed/Alert/None) → 변경 시 StateTree 신호 |
| `ULNPEnemyTargetFollowProcessor` | Behavior (Targeting 이후) | MoveTarget 목적지 산출 (정지 거리 반영), 공격 루프용 StateTree 신호 |
| `ULNPEnemyMovementProcessor` | Movement | 실제 이동/회전 적용 — §5 상세 |
| `ULNPHealthProcessor` | PostPhysics | HP ≤ 0 → DyingTag + Ragdoll 트리거 + DeathCountdown 설정 |
| `ULNPEnemyDeathTimerProcessor` | PostPhysics (Health 이후) | DeathCountdown 만료 엔티티 파괴 |
| `ULNPEnemyLODOverrideProcessor` | LOD (DistanceLOD 이후, Representation 이전) | Confirmed면 `RepresentationLOD.LOD = High` 강제 — §7.2 |
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

---

## 6. Actor 연동 (High LOD)

**`ALNPEnemyCharacter`** — Config로 초기화되는 범용 셸.

| API | 역할 |
|:---|:---|
| `InitializeOnce(Config)` | ASC·어빌리티·무기 1회 초기화 (`bInitializedOnce` 가드) |
| `SyncFromEntity(Health, State, Velocity)` | 매 활성화: Mass → Actor 주입 (HP Bar 초기값 포함) |
| `SyncToEntity(out Health, out Velocity)` | 매 프레임: Actor → Mass 역동기화 |
| `TriggerRagdoll()` | 사망 연출 — 물리 래그돌 + 이동 비활성 (중복 호출 안전) |
| `SetLockOnMarkerVisible()` | 락온 표식 위젯 토글 (LockOnComponent가 호출) |

### 월드 스페이스 HP Bar

- `UWidgetComponent`(World Space) + `ULNPHpBarWidget`(BindWidget `HpBar` ProgressBar).
- 가시 조건 `0 < HP < MaxHP`. 스폰 시 `SyncFromEntity`, 전투 중 ASC Health 변경 델리게이트로 갱신.
- BP CDO에서 `HpBarWidgetClass = WBP_LNPHpBar` 지정.

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

---

## 8. 미구현 항목

| 항목 | 설명 |
|:---|:---|
| 시야각·상태 가중치 | `AngleWeight` 필드는 Config에 존재하나 점수 공식은 거리+Leash만 사용. 시야각·공격 상태 가중치 보강 예정 |
| 난이도 스케일링 | 잔여 LootPod 수 기반 NPC 강화 (슬롯 한도 또는 능력치 단계 조정) |
| Enemy 패링 | `FLNPParryStateFragment`를 Enemy 엔티티에 연결 + StateTree/GA 갱신 경로 (→ [TechDesign_ParrySystem.md](TechDesign_ParrySystem.md)) |
