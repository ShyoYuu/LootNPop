# 엘리트 NPC 아키텍처

> 상태: 기준 설계(E0 작성, 구현 전)
> 소유: 데이터 모델, 스폰·배치, 귀속과 Pod 경로 제외, 세력권, 두 두뇌의 구조와 소유권, 사망 드랍
> 마지막 갱신: 2026-11-05 — 파일:라인 인용은 이 날짜 기준이다. 착수 세션에서 다시 확인한다.

## 1. 구조 개요

```
                 ┌──────────────── 서버 ────────────────────────────────────────────┐
DA_MassSpawnConfig.UnboundEnemies                                                   │
   │  BuildUnboundPlan (Pod 배치 뒤, 별도 시드)                                      │
   ▼                                                                                │
엔티티 + FLNPEnemyHomeFragment + FLNPEnemyUnboundTag                                 │
   │                                                                                │
   │  Scoring/Targeting (기존, HomeMode별 추격 자격만 분기) → TargetingState        │
   ▼                                                                                │
ST_EnemyUnbound (Mass, 매크로) ── Guard / Roam / Return / Alert / Combat            │
   │                                   │                                            │
   │ Combat && Actor 있음               │ Combat && Actor 없음 → 기존 Chase 폴백     │
   ▼                                                                                │
Engage: FLNPEnemyActorBrainTag 부착 → ALNPEnemyCharacter::StartCombatBrain()        │
   │                                                                                │
   ▼                                                                                │
CombatBrain (UStateTreeComponent, Actor) ── 패턴 선택 → GAS 어빌리티 → 몽타주·ANS  │
   │                                        └ 이동 의도 → InputHandler → Mover      │
   ▼                                                                                │
HP 0 → ULNPHealthProcessor → FLNPEnemyDeathRewardCommand → SpawnRewardSet → Dice    │
                 └──────────────────────────────────────────────────────────────────┘
게스트: 복제 Actor(몽타주·큐)와 LootDice만 받는다. 새 복제 필드 없음.
```

두 두뇌의 경계(EL-001):

| 질문 | 담당 | 원본 데이터 |
|:---|:---|:---|
| 누구를 노리는가, 싸우는가 | Mass 타겟팅 + 매크로 두뇌 | `FLNPEnemyTargetingFragment` |
| 어디에 있어야 하는가(대기·배회·귀환) | 매크로 두뇌 | `FLNPEnemyHomeFragment` |
| 어떤 패턴을 언제 쓰는가, 페이즈 | 전투 두뇌 | Actor ASC 태그·쿨다운, 엔티티 HP 비율 |
| 전투 중 어디로 움직이는가 | 전투 두뇌 (EL-004) | InputHandler AI Intent |

## 2. 데이터 모델

### 2.1 귀속 방식과 프래그먼트 (`Enemy/LNPEnemyMassTypes.h`)

```cpp
UENUM(BlueprintType)
enum class ELNPEnemyHomeMode : uint8 { Pod, Arena, Roam };

USTRUCT() struct FLNPEnemyHomeFragment : public FMassFragment
{
    ELNPEnemyHomeMode HomeMode = ELNPEnemyHomeMode::Roam;
    FVector HomeCenter = FVector::ZeroVector;  // Arena: 앵커 월드 위치. Roam: 스폰 위치(기록용)
    float LeashRadius = 0.f;                   // Arena: ArenaRadius. Roam: 0(무제한)
    double LastConfirmedTime = 0.0;            // 해제 리셋 판정용
    bool bResetApplied = true;                 // 리셋을 한 번만 적용
};

USTRUCT() struct FLNPEnemyUnboundTag : public FMassTag {};     // Pod 기반 프로세서 쿼리 제외용
USTRUCT() struct FLNPEnemyActorBrainTag : public FMassTag {};  // 전투 두뇌가 이동 의도를 소유하는 동안
```

- Pod 적은 이 프래그먼트를 갖지 않는다. HomeMode `Pod`는 스폰 엔트리 검증에서 "금지 값"으로만 쓴다. 귀속 축 전체를 한 enum으로 표현하기 위해 둔다.
- 기존 `FLNPEnemyFragment::ParentPodLocation`을 아레나 중심으로 재사용하지 않는다. 이름이 거짓말을 하게 되고, Pod 기반 프로세서가 읽기 때문이다.

### 2.2 스폰 구성 (`DataAsset/LNPMassSpawnConfig.{h,cpp}`)

```cpp
USTRUCT(BlueprintType)
struct FLNPUnboundEnemySpawnEntry
{
    FName SpawnId;                                    // 안정 ID. 비거나 중복 불가
    TObjectPtr<UMassEntityConfigAsset> EnemyEntityConfig;
    ELNPEnemyHomeMode HomeMode = ELNPEnemyHomeMode::Roam;  // Arena | Roam
    FName ArenaTag;                                   // Arena: 같은 태그의 아레나 앵커마다 Count기
    int32 Count = 1;                                  // Roam: 월드 전체 수. Arena: 앵커당 수
    float MinDistanceFromPods = 3000.f;               // Roam 전용. 서로 간 최소 거리에도 쓴다
};

// ULNPMassSpawnConfig
UPROPERTY(EditAnywhere, Category = "LNP|Setup")
TArray<FLNPUnboundEnemySpawnEntry> UnboundEnemies;
```

`IsDataValid`(`LNPMassSpawnConfig.cpp:10-46`에 추가):

| 대상 | 조건 | 판정 |
|:---|:---|:---|
| Pod `AssociatedEnemies` | 적 Config의 `CombatMode == ActorPromoted` | 오류(EL-003) |
| `UnboundEnemies` | `SpawnId`가 비었거나 중복 | 오류 |
| `UnboundEnemies` | `HomeMode == Pod` | 오류 |
| `UnboundEnemies` | `Arena`인데 `ArenaTag`가 None | 오류 |
| `UnboundEnemies` | `EnemyEntityConfig` 누락, `Count < 0` | 오류 |
| `UnboundEnemies` | 적 Config가 `PureEntity` | 경고. 전투 두뇌 없이 매크로 트리의 Chase/Attack 폴백으로만 싸운다 |
| `UnboundEnemies` | `ActorCombatStateTree`가 비어 있음 | 경고. 같은 폴백 |

- 적 Config를 읽으려면 EntityConfig에서 `ULNPEnemyTrait`를 찾아야 한다. `EnqueueSpawnProject`의 기존 조회(`GameLogic/LNPMassSpawnSubsystem.cpp:393-401`)와 같은 방식을 헬퍼로 묶어 둘이 공유한다.
- 같은 단위에서 `Enemy/LNPEnemyConfig.h:565`의 CombatMode 주석("LODOverride가 표현 LOD를 눌러")을 실제 동작(조기 반환, LowLOD §6.2)에 맞게 고친다.

### 2.3 `ULNPEnemyConfig` 추가 필드 (`Enemy/LNPEnemyConfig.h`)

| 필드 | 타입 | 용도 |
|:---|:---|:---|
| `ActorCombatStateTree` | `TObjectPtr<UStateTree>` | 전투 두뇌 트리. 비어 있으면 전투 두뇌를 켜지 않는다 |
| `PatternAbilities` | `TArray<TSubclassOf<UGameplayAbility>>` | `InitializeOnce`에서 무기 어빌리티와 함께 부여 |
| `DeathRewards` | `FLNPLootDiceRewardSet` | 사망 드랍. `Entries`가 비어 있으면 드랍 없음(EL-009) |
| `DisengageResetDelay` | `float`(3.0) | 아레나 해제 리셋 지연 |
| `RoamStepMin` / `RoamStepMax` | `float`(3000 / 8000) | 배회 경유점 거리 대역 |

- 기존 `ULNPEnemyConfig::StateTree`(`LNPEnemyConfig.h:640-642`)는 C++ 어디서도 읽지 않는다. 실제 트리는 EntityConfig의 `MassStateTreeTrait`가 정한다. 이번 작업에서 지우거나 재활용하지 않는다(혼동 방지를 위해 주석만 보강할지 E1에서 판단).

## 3. 아레나 마커와 베이크 (EL-010)

- 새 에디터 전용 Actor `ALNPArenaSpawnPoint`(`SurfaceNavigation/LNPArenaSpawnPoint.{h,cpp}`). `ALNPMassSpawnPoint`(`SurfaceNavigation/LNPMassSpawnPoint.h:17-48`)와 같은 성격이다.
  - `ArenaId`(FGuid, 생성 시 발급), `ArenaTag`(FName), `ArenaRadius`(cm, 초안 3000)
  - 에디터에서 반경을 보여 주는 구 컴포넌트를 붙인다.
- `FLNPSpawnData`(`SurfaceNavigation/LNPSpawnData.h:44-48`)에 `TArray<FLNPSpawnArenaAnchor> ArenaAnchors`를 추가한다.
  - `FLNPSpawnArenaAnchor { FGuid ArenaId; FName ArenaTag; FTransform3f LocalTransform; uint16 LocalLayerId; float Radius; }`
  - `LNPSpawnData::CodecVersion` 3→4. Encode/Decode에 정렬(Guid 기준)·중복 ID·유한값 검증을 같은 규칙으로 넣는다.
  - FName은 문자열로 직렬화한다(FName 인덱스는 프로세스마다 다르다).
- 베이커(`LootNPopEditor/SurfaceNavigation/LNPOctantSurfaceBaker.cpp:692-800`의 SpawnPoint 수집부)에 아레나 마커 수집과 표면 투영을 추가한다. 투영 실패는 베이크 오류로 보고한다(기존 앵커 처리와 같은 형식).
- 모든 옥탄트 Spawn 스트림을 재베이크해야 한다. 옛 스트림은 codec 불일치로 Decode가 실패해야 한다(E3 착수 전 확인).
- 콘텐츠: 보스 랜드마크가 있는 옥탄트(Acropolis 기단 반경 약 68m, Hollow Arena 반경 33m 등)에 마커를 둔다. 반경은 지형마다 다르므로 마커가 소유한다.

## 4. 배치 계획 (`SurfaceNavigation/LNPMassSpawnPlan.{h,cpp}`) (EL-005)

- 새 함수 `LNPMassSpawnPlanning::BuildUnboundPlan(Snapshot, Entries, WorldSeed, PodPlacements, OutPlan, OutError)`. `BuildPlan`이 끝난 뒤 같은 스냅샷과 Pod 배치 결과를 받아 돈다.
- 시드: `FRandomStream(WorldSeed ^ <새 salt>)`. 기존 `WorldSeed ^ 0x51A7F00D` 스트림(`LNPMassSpawnPlan.cpp:266`)을 건드리지 않는다. 그래서 엔트리를 추가해도 Pod 결과가 바뀌지 않는다.
- 기준 순서: 엔트리는 `SpawnId`, 아레나 앵커는 (slot, ArenaId), 후보는 (slot, CandidateIndex)로 정렬한 뒤 셔플한다(기존 `:238-249` 방식).
- **Arena:** 게시된 8칸의 `ArenaAnchors` 중 `ArenaTag`가 같은 앵커마다 `Count`기.
  - 위치: 앵커 반경 안, 앵커와 같은 (slot, Layer)의 Enemy 플래그 후보. 앵커에 가까운 것부터. 적끼리 `MinEnemySpacing`(200) 유지.
  - 후보가 없으면 앵커 위치 자체를 쓴다(아레나는 수동 배치라 앵커가 표면 위에 있음이 베이크로 보장된다).
  - 출력: HomeCenter = 앵커 월드 위치, LeashRadius = `Radius`.
- **Roam:** `RandomCandidates` 중 Enemy 플래그 후보. 모든 Pod에서 `MinDistanceFromPods` 이상, 이미 놓인 Roam 엘리트와도 같은 거리 이상.
- 놓지 못한 수량은 계획 오류가 아니라 경고 로그(`[UnboundPlan] entry=... placed=N/M`)로 남긴다. 옥탄트 조합에 따라 아레나 태그가 없을 수 있기 때문이다.
- 비행 적 천장 높이 처리(`GetSpawnRequiredHeadroom`)는 엘리트가 지상 전용이라 쓰지 않는다.

## 5. 스폰, 귀속, Pod 경로 제외, 세력권

### 5.1 스폰 (`GameLogic/LNPMassSpawnSubsystem.cpp`)

- `EnqueueSpawnProject`(`:299-594`): Unbound 엔트리의 에셋도 `CapturedAssets`로 미리 바꾸고, 계획 결과를 Pod 요청 뒤 큐 끝에 넣는다. `ProcessQueue`의 프레임 분산(`MaxSpawnsPerFrame`)을 그대로 탄다.
- 템플릿 사전 등록(`OnWorldBeginPlay`, `:148`)에 Unbound 엔트리의 EntityConfig를 포함한다. 게스트도 같은 템플릿이 필요하다.
- `SetupSpawnedEntities`(`:771-886`): Unbound 요청이면
  - `ParentLootPod`는 무효, `ParentPodLocation`은 스폰 위치로 둔다(기존 코드가 0 벡터를 읽지 않게).
  - `FLNPEnemyHomeFragment`와 `FLNPEnemyUnboundTag`를 붙인다. 소수라 아키타입 이동 비용은 문제가 아니다. 방식(지연 커맨드 vs 템플릿 트레잇 옵션)은 E1 착수 시 정한다(Roadmap §4).
  - 지상 보정(캡슐 반높이)은 기존 경로를 그대로 쓴다.
- 결과 로그: `[UnboundSpawn] id=... mode=... entity=... home=...`.

### 5.2 Pod 기반 프로세서에서 빼기

| 경로 | 위치 | 처리 | 빼지 않으면 |
|:---|:---|:---|:---|
| Home 재귀속·고아 처리 | `Enemy/LNPEnemyPathProcessor.cpp:284-322` | 해당 블록을 `FLNPEnemyUnboundTag`가 있으면 건너뛴다(경로 요청 자체는 유지) | 1초마다 `SelectHomePod`가 엘리트를 가까운 Pod에 붙인다 |
| 활동 대역 | `Enemy/LNPEnemyActivityProcessor.cpp` | PureEntity 전용이라 ActorPromoted 엘리트는 이미 빠진다. Unbound PureEntity를 위해 쿼리에서도 제외 → 항상 Active(D-067) | Unbound PureEntity가 Dormant가 된다 |
| Pod 배회 대역 | `Enemy/LNPEnemyWanderProcessor.cpp:133-134` | `ParentLootPod.IsValid()` 조건으로 이미 빠진다. 확인만 한다 | — |
| 벽보행 재귀속 | `Enemy/LNPEnemyCrawlMovementProcessor.cpp:455-500` | 엘리트는 벽보행이 없다. 확인만 한다 | — |
| IdleTask | `ST_EnemyEntity` | 엘리트는 `ST_EnemyUnbound`를 쓰므로 해당 없음(E1~E3 임시 기간 제외, Roadmap §2) | — |

### 5.3 추격 자격 (`Enemy/LNPEnemyProcessors.cpp:244-248`, EL-008)

```cpp
// 기존: PlayerToPod <= ChaseRadius || 후보 거리 <= AwarenessDistance
switch (Home ? Home->HomeMode : Pod)
  Pod:   기존 그대로
  Arena: PlayerToHomeCenter <= LeashRadius        // 코앞 반격(AwarenessDistance)도 아레나 밖이면 막는다
  Roam:  true                                     // 앵커 없음. 시야·인지·인내 규칙만
```

- Arena에서 `AwarenessDistance` 예외를 막는 이유: 아레나 경계 바로 밖에 서서 근접 반격을 유도하는 공략을 막는다. 근접 거리가 아레나 반경보다 훨씬 작으므로 경계 안쪽 플레이에는 영향이 없다.
- Roam에서 §7.8(세력권을 NPC 위치로 재면 자기진동)이 문제되지 않는 이유: 비교할 앵커가 없다.
- Promoted 슬롯 풀(플레이어당 2, `Enemy/LNPTargetingSubsystem.h:66-83`)은 이제 엘리트 전용이다. 한도는 바꾸지 않는다(LowLOD §7 대역폭 근거).

## 6. 매크로 두뇌 `ST_EnemyUnbound` (Mass StateTree)

### 6.1 상태 구조

| 우선순위 | 상태 | Enter 조건 | Task |
|---:|:---|:---|:---|
| 1 | **Combat** | `TargetingState == Confirmed` | (자식) |
| 1-1 | └ Engage | Actor 있음 && `ActorCombatStateTree` 있음 | `FLNPEnemyEngageTask` |
| 1-2 | └ Attack | 사거리 안 | 기존 `FLNPEnemyAttackTask`(폴백) |
| 1-3 | └ Chase | 그 외 | 기존 `FLNPEnemySteeringTask`(폴백) |
| 2 | **Return** | Arena && `bOutsideHome` | `FLNPEnemyReturnTask` |
| 3 | **Alert** | `TargetingState == Alert` | 기존 `FLNPEnemyLookAtTask` |
| 4 | **Idle** | (기본) | (자식) |
| 4-1 | └ Guard | `HomeMode == Arena` | `FLNPEnemyGuardTask` |
| 4-2 | └ Roam | `HomeMode == Roam` | `FLNPEnemyRoamTask` |

- 전이는 기존 트리와 같이 On Tick + 조건 불일치 → Root.
- Engage가 Attack·Chase보다 위다. Actor가 강등돼 사라지면 Engage 조건이 깨져 폴백으로 내려간다.

### 6.2 노드 (`Enemy/LNPEnemyUnboundStateTree.{h,cpp}`)

| 노드 | 역할 |
|:---|:---|
| `FLNPEnemyHomeEvaluator` | 출력: `HomeMode`, `DistanceToHome`, `bOutsideHome`(Arena에서 HomeCenter에서 `GuardRadius` 초과) |
| `FLNPEnemyEngageTask` | Enter: `FLNPEnemyActorBrainTag` 부착, `StartCombatBrain()`. Exit: 태그 제거, `StopCombatBrain()`. Tick: Actor·트리 유효성만 확인 |
| `FLNPEnemyReturnTask` | `MoveTarget`을 HomeCenter로. 도착하면 Succeeded |
| `FLNPEnemyGuardTask` | 앵커 근처 정지. 아레나 중심 또는 마지막 위협 방향을 본다 |
| `FLNPEnemyRoamTask` | §6.5 경유점 선택. 도착하면 다음 경유점 |

- Task는 Transform을 건드리지 않는다. `MoveTarget`에 의도만 남기고 이동은 `ULNPEnemyMovementProcessor`가 한다(StateTree 문서 §1 규약 유지).
- Tick은 신호 구동이다. 귀환 도착은 Movement 프로세서의 도착 신호가, 전투 중 진행은 `ULNPEnemyTargetFollowProcessor`(`Enemy/LNPEnemyProcessors.cpp:467-523`) 신호가 깨운다. 이 신호가 Engage 상태에도 오는지 E4에서 확인한다.

### 6.3 이동 의도 소유권 (EL-004)

- `ULNPEnemyMovementProcessor`의 Actor 위임 분기(`Enemy/LNPEnemyProcessors.cpp:868-889`)는 엔티티에 `FLNPEnemyActorBrainTag`가 있으면 `SetAIMoveInput`·`SetAIOrientationIntent`·`SetAIDesiredSpeed`를 쓰지 않는다.
- 전투 두뇌는 `StartCombatBrain()`과 `StopCombatBrain()` 사이에서만 의도를 쓴다. Stop 때 의도를 0으로 비운다(남은 입력이 다음 소유자에게 새지 않도록).
- Actor가 사라지면 `ULNPEnemyActorSyncProcessor`(`:1299-1348`)가 초기화 태그를 뗄 때 `FLNPEnemyActorBrainTag`도 같이 뗀다. 그래야 Actor 없는 Chase 폴백에서 엔티티 적분이 다시 돈다.

### 6.4 해제 리셋 (Arena 전용, EL-006)

- 새 프로세서 `ULNPEnemyHomeProcessor`(서버, Behavior 그룹 뒤, `FLNPEnemyUnboundTag` 쿼리 — 소수라 매 프레임 돌아도 된다).
  - Confirmed면 `LastConfirmedTime = Now`, `bResetApplied = false`.
  - `!bResetApplied && Now - LastConfirmedTime >= DisengageResetDelay`면 리셋:
    - `FLNPEnemyFragment::Health`를 최대치로(최대치는 템플릿이 계산한 값, `Enemy/LNPEnemyMassTypes.cpp:43-59`)
    - `FLNPPoiseFragment` 누적·그로기·다운 상태를 초기화
    - Actor가 있으면 게임 스레드 커맨드로 ASC Health도 최대로(다음 프레임 `SyncToEntity`가 덮어쓰지 않게 같은 커맨드에서 처리)
  - 페이즈는 HP 비율에서 파생하므로 자동으로 1페이즈로 돌아간다(EL-007).
- 로그: `[UnboundReset] entity=... hp=...`.

### 6.5 배회 경유점 (Roam)

- `FLNPEnemyRoamTask`가 자기 위치에서 `RoamStepMin`~`RoamStepMax` 거리 대역의 Enemy 플래그 후보를 고른다. 후보 원본은 Surface 데이터의 Spawn 스트림이다.
- 지상 도달성 검사는 `FLNPEnemyIdleTask`의 지상 후보 검증(`ProjectWanderTarget` + 도달성, `Enemy/LNPEnemyStateTreeProcessors.cpp:440-532`)을 함수로 빼서 공유한다.
- 장거리 경로 추종은 `ULNPEnemyPathProcessor`의 경로 요청을 Idle(Roam) 상태에도 연다. 옥탄트를 넘는 경로의 비용·예산은 E4 착수 시 먼저 조사한다(Roadmap §4).
- 경유점 추첨은 서버 전용이라 결정론이 필요 없다(기존 배회도 `FMath::FRandRange`).

## 7. 전투 두뇌 `CombatBrain` (Actor StateTreeComponent, 서버 전용)

### 7.1 컴포넌트

- `ALNPEnemyCharacter`(`Enemy/LNPEnemyCharacter.{h,cpp}`)에 `UStateTreeComponent* CombatBrain`을 만든다.
  - `bStartLogicAutomatically = false`. 트리는 `EnemyConfig->ActorCombatStateTree`에서 `InitializeOnce`(`:77-126`) 때 지정한다. 비어 있으면 아무것도 하지 않는다.
  - `StartCombatBrain()` / `StopCombatBrain()`: Authority에서만 동작한다. 게스트의 복제 Actor에서는 컴포넌트가 돌지 않는다.
  - AIController는 쓰지 않는다. 스키마는 Pawn을 컨텍스트로 받는 컴포넌트 스키마다(E5 착수 시 5.8 API로 확인).
- `PatternAbilities`를 `InitializeOnce`에서 부여한다. 무기 어빌리티(`WeaponAbilityHandle`)는 그대로 둔다(폴백 공격·기본 콤보용).

### 7.2 노드 (`Enemy/CombatBrain/`)

| 종류 | 이름 | 역할 |
|:---|:---|:---|
| 전역 Evaluator | `FLNPCombatContextEvaluator` | 엔티티의 Confirmed 타겟 Pawn(타겟 엔티티의 `FMassActorFragment`), 거리, 정면각, 타겟 속도, HP 비율, 그로기·다운 여부, 패턴 어빌리티 활성 여부, HomeMode·HomeCenter·LeashRadius |
| Task | `FLNPActivatePatternTask` | 어빌리티 태그로 발동한다. 발동 직전 조준점(`SetAimTargetLocation`)과 MotionWarping 타겟을 설정한다. ASC의 어빌리티 종료 이벤트로 Succeeded(정상 종료)/Failed(취소·발동 실패)를 돌려준다. Exit에서 구독을 반드시 해제한다 |
| Task | `FLNPCombatMoveTask` | 접근·측면 이동·후퇴. 모드, 목표 거리 대역, 최대 지속 시간을 파라미터로 받는다. 의도는 방향 단위벡터 + 속도로 분리해서 넣는다(Mover 의도 크기<1 함정). Arena면 다음 위치가 LeashRadius 밖이면 접선 성분만 남긴다 |
| Task | `FLNPFaceTargetTask` | 제자리 회전. 정면각이 임계값 안이면 Succeeded |
| Condition | `FLNPPatternReadyCondition` | 패턴 어빌리티의 쿨다운 태그가 없고 `CanActivateAbility`가 참 |

- 거리·각도·페이즈 조건은 엔진 float·enum 비교 조건을 쓴다. C++ 조건은 위 하나만 만든다.
- 패턴 선택: 패턴 상태들을 형제로 두고 부모의 선택 방식을 Utility Selection으로 한다. Consideration은 거리 커브, 정면각 커브, 페이즈 가중치. 같은 점수면 무작위.
- 페이즈: 최상위에 Phase1·Phase2 상태를 두고 Phase2 진입 조건을 `HP 비율 <= 임계값`으로 한다. Phase2 진입 직후 전환 연출 상태(어빌리티 1개)를 한 번 거친다. 리셋으로 HP가 돌아오면 Phase1로 돌아간다.
- 경직 대기: 최상위 첫 상태로 "Staggered"를 두고, `GA_Stagger`의 입력 차단 태그가 있는 동안 머문다.

### 7.3 패턴 어빌리티 `ULNPAbility_MontagePattern` (`GAS/Abilities/`)

- 기존 근접 어빌리티(`ULNPAbility_MeleeAttack`)는 Chooser로 몽타주를 고르고 콤보 인덱스를 쓴다. 패턴은 "이 몽타주를 한 번"이므로 몽타주를 명시적으로 지정하는 별도 클래스를 둔다.
- 판정은 몽타주의 `ANS_LNPMeleeHitWindow`가 만드는 칼날 엔티티(`FLNPWeaponTraceFragment`)를 그대로 탄다. 그래서 패링·저스트 회피·가드·무적이 `LNPGuardParry::Judge` 한 곳에서 자동으로 성립한다.
- 필드: `Montage`, `DamageMultiplier`, `PoiseDamageMultiplier`, `bUseMotionWarping` + 워프 타겟 이름, 쿨다운(기존 `LNPAbility_BasicAttack`의 Cooldown GE 방식, `GAS/Abilities/LNPAbility_BasicAttack.cpp:66-95`).
- 태그: `Ability.Pattern.<이름>`(발동용), `Ability.Type.Attack`(경직 취소용).
- 원거리 패턴은 `ULNPAbility_RangedAttack` 계열을 쓴다. 조준은 `GetBaseAimRotation()`(복제 Pitch)이라 추가 작업이 없다.

### 7.4 경직·패링 연동

- 경직 명령 `FLNPStaggerCommand::Run`(`GAS/LNPPoiseTypes.cpp:57-140`)은 `CancelCurrentAttackAbility()`(`:92`)를 부른다. 적 구현(`Enemy/LNPEnemyCharacter.cpp:207`)은 지금 무기 핸들만 취소한다. 이를 **`Ability.Type.Attack` 태그를 가진 활성 어빌리티 전체 취소**로 넓힌다. 플레이어 구현은 바꾸지 않는다.
- 패링 브레이크·넉백은 기존 `FLNPMeleeParryCommand`가 Actor 경로로 처리한다. 추가 작업 없음.
- 경직 임계값은 적 Config 데이터(`PoiseStaggerThreshold`·`PoiseDownThreshold`)로 높게 둔다. 코드 변경 없음.

### 7.5 상태 보존 (EL-007)

- Actor는 풀링되지 않고 강등 때 파괴된다(TechDesign_EnemyNPC "Actor 수명"). Confirmed 동안에는 LODOverride가 High를 강제하므로 교전 중 파괴는 드물다.
- 전투 두뇌가 가진 상태(현재 패턴, 쿨다운)는 Actor와 함께 사라진다. 다시 승격되면 쿨다운이 비어 있는 상태로 시작한다. 이를 허용한다.
- 페이즈는 저장하지 않는다. 엔티티 HP 비율에서 매번 파생한다.

### 7.6 복제

- 전투 두뇌·패턴 선택은 서버에서만 돈다. 결과는 기존 경로로 복제된다: 몽타주(ASC), GameplayCue, Mover 이동, 복제 Pitch.
- 적 ASC는 Minimal 복제 모드다. 패턴 몽타주가 게스트에서 재생되는지 E5에서 2P로 확인한다(기존 근접 적이 같은 경로를 쓰므로 위험은 낮다).
- 새 복제 필드를 만들지 않는다. 대역폭 예산은 Promoted Actor 1기당 기존 값(700~900 B/s)을 넘지 않아야 한다(E6 측정).

## 8. 사망 LootDice 드랍 (EL-009)

- `ALNPLootDice::SpawnPodRewards`(`LootDice/LNPLootDice.cpp:328-377`)의 본문(가중치 합, MinDrops~MaxDrops 추첨, `SpawnDice` 반복)을 `static void SpawnRewardSet(UWorld&, const FLNPLootDiceRewardSet&, const FVector& Location)`으로 추출한다. `SpawnPodRewards`는 테이블 조회 후 이것을 부르기만 한다. Pod 보상 동작은 바뀌지 않는다.
- `ULNPHealthProcessor`(`Enemy/LNPEnemyProcessors.cpp:970-1054`)에서 `Health <= 0`을 확정하고 `DyingEntities.Add` 하는 지점(`:1020` 부근)에서:
  - 공유 프래그먼트의 Config에 `DeathRewards.Entries`가 있으면 (위치 + Up×50, Config 포인터)를 모은다.
  - 루프 뒤 배치 커맨드 `FLNPEnemyDeathRewardCommand`(`FLNPPodStateTransitionCommand` 패턴, `LootPod/LNPLootPodProcessor.cpp:24-106`)로 게임 스레드에 넘긴다.
- 1회성 보장: 이 프로세서 쿼리는 `FLNPEnemyDyingTag`를 제외한다(`:984-985`). 같은 엔티티가 두 번 들어오지 않는다.
- 서버 전용 프로세서라 게스트에서 중복 드랍이 없다. Dice는 기존 Actor 복제로 게스트에 보인다.
- 가해자 정보는 쓰지 않는다(프래그먼트에 없다). 드랍은 모두에게 열려 있다.

## 9. 콘텐츠 전환

- `DA_MassSpawnConfig`: Pod `AssociatedEnemies`의 `DA_EnemyEntityConfig_ActorPromoted_Melee01`을 `DA_EnemyEntityConfig_PureEntity_Melee01`로 바꾼다. 기본 편성 보존 규칙의 예외이며 사용자가 요청했다.
- `DA_MassSpawnConfig_TestOnePerSpecies`: 같은 검증 규칙을 통과하도록 ActorPromoted 종을 `UnboundEnemies`(Roam 1기)로 옮긴다.
- 첫 엘리트(E6): `DA_Enemy_<이름>` + `DA_EnemyEntityConfig_<이름>`(트레잇의 StateTree를 `ST_EnemyUnbound`로) + `ST_Combat_<이름>` + 패턴 GA 4~6개 + `DeathRewards`. 에셋 이름에 "Elite"를 넣지 않는다(EL-002). 개체 이름을 쓴다.

## 10. 위험·미결

| 항목 | 내용 | 대응 |
|:---|:---|:---|
| Promoted 슬롯 2 | 아레나 근처에 배회 엘리트가 겹치면 아레나 엘리트가 슬롯을 놓칠 수 있다 | E6 측정 뒤 한도 조정 또는 Arena 점수 가산 |
| 장거리 경로 비용 | Roam이 옥탄트를 넘는 경로를 자주 요청한다 | E4 착수 전 PathProcessor 예산 조사, 경유점 거리 상한으로 제어 |
| codec v4 재베이크 | 전 옥탄트 대상, 누락 시 런타임 Decode 실패 | 옛 스트림이 확실히 실패하는지 먼저 확인, 재베이크 목록을 Phase 문서에 둔다 |
| 지상 전용 | Mover는 지상만. 아레나는 지상 접근이 가능해야 한다 | 마커 배치 시 확인 |
| 임시 기간의 IdleTask | E1~E3에는 엘리트가 `ST_EnemyEntity`를 쓴다 | 무해한지만 확인, E4에서 해소 |
