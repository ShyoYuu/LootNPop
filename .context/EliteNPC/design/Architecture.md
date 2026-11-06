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
   │ Combat && Actor 초기화 완료        │ Combat && Actor 없음 → 기존 Chase 폴백     │
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
    FLNPSurfaceHandle HomeSurface;             // Arena: 앵커의 slot·Layer·generation
    double DisengagedSince = -1.0;             // Confirmed 해제 시각. -1이면 계측 안 함
    bool bResetPending = false;                // 교전 또는 허용된 피격 이후 리셋 필요
    bool bReturning = false;                   // 귀환 이동이 목표를 소유함
};

USTRUCT() struct FLNPEnemyUnboundTag : public FMassTag {};     // Pod 기반 프로세서 쿼리 제외용
USTRUCT() struct FLNPEnemyActorBrainTag : public FMassTag {};  // 전투 두뇌가 이동 의도를 소유하는 동안
```

- Pod 적은 이 프래그먼트를 갖지 않는다. HomeMode `Pod`는 스폰 엔트리 검증에서 "금지 값"으로만 쓴다. 귀속 축 전체를 한 enum으로 표현하기 위해 둔다.
- 기존 `FLNPEnemyFragment::ParentPodLocation`을 아레나 중심으로 재사용하지 않는다. 이름이 거짓말을 하게 되고, Pod 기반 프로세서가 읽기 때문이다.
- 위 필드는 설계용 스케치다. `bReturning`은 앵커 도착까지 유지한다(EL-016). 넉백 이탈과 교전 해제의 리셋 규칙이 다르므로 귀환 시작 사유도 엔티티에 보존한다.
- Roam·Return·전투 이동의 경로 목표는 **위치 + 목적지 `FLNPSurfaceHandle`**로 구성한다. 목적지 식별 정보는 현재 위치의 Surface와 별개이며, 기존 이동/경로 프래그먼트의 확장 위치는 E4에서 정한다. 페이즈 연출 소비 상태는 §7.5가 소유한다.

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
  - 출력: HomeCenter = 앵커 월드 위치, HomeSurface = 앵커의 slot·Layer·현재 generation, LeashRadius = `Radius`.
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
- Roam에서 기존 EnemyNPC 문서 §7.8의 자기진동(세력권을 NPC 위치로 측정)이 문제되지 않는 이유: 비교할 앵커가 없다.
- Promoted 슬롯 풀(플레이어당 2, `Enemy/LNPTargetingSubsystem.h:66-83`)은 이제 엘리트 전용이다. 한도는 바꾸지 않는다(LowLOD §7 대역폭 근거).

### 5.4 아레나 피격 자격 (EL-015)

- **추격 자격과 피격 자격은 별개다.** §5.3에서 슬롯을 막는 것만으로 아레나 밖 피해 누적·지연 시간 안의 처치를 막을 수 없다.
- 피해 허용 정책의 원본은 [GameRules.md](GameRules.md) §6.1이다. 서버 명중 판정 시 공격자가 아레나 안이고 피격자가 귀환·리셋 중이 아닐 때만 피해·경직·넉백을 허용한다.
- E4에서 근접·투사체 직격·폭발 범위의 서버 판정에 같은 자격 검사를 연결한다. Actor 유무와 무관하게 엔티티의 Home 정보로 판단하며, 거부된 피격이 HP·경직·넉백·피격 인지를 일부만 적용하지 않게 한다. 지연 커맨드는 적용 직전에도 피격자 활성·생존·현재 귀환/리셋 상태를 검사한다.
- 기준은 서버가 해당 명중을 판정하는 시점의 공격자 위치 스냅샷이다. 발사 시점 위치나 Lag Compensation으로 되감은 충돌 시점의 위치를 대신 쓰지 않는다. 판정 후 게임 스레드 적용까지 공격자가 이동했다는 이유로 다른 시각의 위치를 다시 혼합하지 않는다.
- 스플래시도 폭발 중심이 아니라 공격자의 위치로 자격을 판단한다. 반사탄은 기존 반사 파이프라인이 갱신한 공격자 귀속을 사용한다. 유효한 공격자 핸들을 확인할 수 없으면 위치 자격을 입증할 수 없으므로 아레나 보스에 대한 해당 피해를 거부한다. 공격 종류별 이 귀속 정보 전달을 E4에서 확인한다.

## 6. 매크로 두뇌 `ST_EnemyUnbound` (Mass StateTree)

### 6.1 상태 구조

| 우선순위 | 상태 | Enter 조건 | Task |
|---:|:---|:---|:---|
| 1 | **Return** | Arena && `bReturning` | `FLNPEnemyReturnTask` |
| 2 | **Combat** | `TargetingState == Confirmed` && 귀환 아님 | (자식) |
| 2-1 | └ Engage | Actor 초기화 완료 && `ActorCombatStateTree` 있음 | `FLNPEnemyEngageTask` |
| 2-2 | └ Attack | 사거리 안 | 기존 `FLNPEnemyAttackTask`(폴백) |
| 2-3 | └ Chase | 그 외 | 기존 `FLNPEnemySteeringTask`(폴백) |
| 3 | **Alert** | `TargetingState == Alert` | 기존 `FLNPEnemyLookAtTask` |
| 4 | **Idle** | (기본) | (자식) |
| 4-1 | └ Guard | `HomeMode == Arena` | `FLNPEnemyGuardTask` |
| 4-2 | └ Roam | `HomeMode == Roam` | `FLNPEnemyRoamTask` |

- 전이는 기존 트리와 같이 On Tick + 조건 불일치 → Root.
- Engage가 Attack·Chase보다 위다. Actor가 강등돼 사라지면 Engage 조건이 깨져 폴백으로 내려간다.
- 사망 확정은 모든 상태보다 우선한다. 사망 시 §7.7의 종료 경로를 실행하고 매크로 트리도 정지시킨다. DyingTag로 프로세서 쿼리에서 빠지는 것만으로 Task 종료를 대신하지 않는다.
- 귀환 시작은 매크로 두뇌가 결정한다. 아레나 밖으로 밀려났거나 교전 후 모든 플레이어가 추격 자격을 잃으면 시작한다. 안쪽에 추격 가능한 후보가 있는데 슬롯만 잠깐 끊긴 경우에는 즉시 귀환을 고정하지 않고 Alert와 기존 리셋 지연을 사용한다. 귀환 중에는 슬롯 경쟁·새 패턴 발동을 보류하되 추격 가능한 후보 관측은 유지한다.
- `GuardRadius`는 사용하지 않는다. 아레나 이탈은 `LeashRadius`, 앵커 도착은 기존 `FLNPEnemyMovementConfig::ArrivalTolerance`를 쓴다. 귀환 중에는 도착 전까지 `bReturning`을 유지하여 도착 허용 범위의 경계에서 상태가 반복되지 않게 한다. 플레이어 재진입으로 조기 해제하지 않는다(EL-016).

### 6.2 노드 (`Enemy/LNPEnemyUnboundStateTree.{h,cpp}`)

| 노드 | 역할 |
|:---|:---|
| `FLNPEnemyHomeEvaluator` | 출력: `HomeMode`, `DistanceToHome`, `bOutsideArena`(`LeashRadius` 초과), `bReturning`, 앵커 도착 여부 |
| `FLNPEnemyEngageTask` | Enter: §6.3의 소유권 인계 후 `StartCombatBrain()`. Exit: §7.7 종료 계약. Tick: 생존·Confirmed·Actor·트리 유효성 확인 |
| `FLNPEnemyReturnTask` | 목표를 HomeCenter + HomeSurface로 설정. 경로 조향점으로 이동하고 앵커 도착을 보고. 재교전/리셋 조건까지 충족해야 귀환 상태 해제 |
| `FLNPEnemyGuardTask` | 앵커 근처 정지. 아레나 중심 또는 마지막 위협 방향을 본다 |
| `FLNPEnemyRoamTask` | §6.5 경유점 선택. 도착하면 다음 경유점 |

- Task는 Transform을 건드리지 않는다. `MoveTarget`에 의도만 남기고 이동은 `ULNPEnemyMovementProcessor`가 한다(StateTree 문서 §1 규약 유지).
- 이 표는 최종 구조다. E4는 Guard·Roam·Return과 기존 Attack/Chase 폴백까지 검증하고, Engage의 실제 CombatBrain 시작·종료 연결은 E5에서 완성한다. E4 리셋은 기존 무기 공격·경직을 정리하고, E5에서 §7.7의 패턴 정리를 연결해 같은 리셋 검증을 반복한다. 아직 없는 CombatBrain 구현을 E4 빌드의 선행 조건으로 두지 않는다.
- Tick은 신호 구동이다. 귀환 도착은 Movement 프로세서의 도착 신호로 깨운다. Actor 준비·소실 신호는 E5의 두뇌 인계에서 연결한다. 기존 TargetFollow 신호가 모든 Confirmed 프레임에 온다고 가정하지 않는다.

### 6.3 이동 의도 소유권 (EL-004)

- `ULNPEnemyMovementProcessor`의 Actor 위임 분기(`Enemy/LNPEnemyProcessors.cpp:868-889`)는 엔티티에 `FLNPEnemyActorBrainTag`가 있으면 `SetAIMoveInput`·`SetAIOrientationIntent`·`SetAIDesiredSpeed`를 쓰지 않는다.
- 전투 두뇌는 `StartCombatBrain()`과 `StopCombatBrain()` 사이에서만 의도를 쓴다. Stop 때 의도를 0으로 비운다(남은 입력이 다음 소유자에게 새지 않도록).
- Actor가 사라지면 `ULNPEnemyActorSyncProcessor`(`:1299-1348`)가 초기화 태그를 뗄 때 `FLNPEnemyActorBrainTag`도 같이 뗀다. 그래야 Actor 없는 Chase 폴백에서 엔티티 적분이 다시 돈다.
- **귀환 목표도 단일 작성자다.** `bReturning` 동안 TargetFollow는 타겟 위치를 MoveTarget에 쓰지 않는다. PathProcessor는 Alert/Confirmed의 플레이어 목표 대신 HomeCenter + HomeSurface를 소비하고, MovementProcessor는 Alert 정지 분기 대신 귀환 속도·조향을 적용한다. 타겟 장부는 유지하되 이동을 덮지 않는다. Pod 적의 기존 분기는 그대로 둔다.
- 전투 두뇌의 이동 목표도 §7.8에 따라 경로에 전달한다. ActorBrain이 켜진 동안 TargetFollow의 기본 추격 목표가 전투 이동 목표를 덮지 않도록 한다. 이동 의도 작성자와 경로 계산자는 구분한다.
- 소유권 전환은 게임 스레드에서 순서가 보장되는 인계로 처리한다. 이전 두뇌 정지·입력 초기화 후 새 두뇌만 입력을 쓴다. 지연된 Mass 이동 커맨드는 적용 시점에도 엔티티 활성 여부·현재 Actor 연결·현재 소유권을 확인하여 이전 소유자의 입력이 뒤늦게 덮지 못하게 한다.

### 6.4 해제 리셋 (Arena 전용, EL-006)

- 새 프로세서 `ULNPEnemyHomeProcessor`(서버, Arena인 `FLNPEnemyUnboundTag`만). 매크로 상태 갱신과 실제 리셋 적용의 순서를 구분한다.
- 일반 교전의 Confirmed 동안 `bResetPending = true`, `DisengagedSince = -1`이다. Confirmed를 잃으면 해제 시각을 한 번 기록한다. 귀환 고정 전이고 지연 안에 다시 Confirmed가 되면 타이머를 취소한다. 미교전 상태에서 **허용된 피격**으로 HP·경직 등이 바뀌어도 pending을 세우고 해제 타이머를 시작한다. 반복 피격이 타이머를 계속 연장하지 않는다. §5.4에서 피격이 거부되면 pending도 만들지 않는다.
- pending이고 해제 시간이 `DisengageResetDelay` 이상이면 리셋 가능 상태다. 교전 해제로 귀환하는 경우 앵커 도착과 지연을 모두 충족한 뒤 리셋하고 `bReturning`을 내린다. 먼저 도착하면 남은 지연을 대기하고, 먼저 지연이 지나면 귀환을 계속한다. 도중 재진입은 이 귀환·리셋을 취소하지 않는다(EL-016).
- 넉백 귀환은 별도 사유로 남긴다. 안에 추격 가능한 후보가 계속 있으면 귀환이 억제한 Confirmed 상실로 타이머를 시작하지 않는다. 앵커 도착 후 HP·경직·연출 장부를 리셋하지 않고 재교전한다. 도중 후보가 모두 사라지면 교전 해제 귀환으로 전환하고 그 시점에 해제 타이머를 시작한다.
- 리셋 성공 때만 pending을 내리며, 이후 새 교전·허용된 피격은 다시 pending을 세운다. 한 번 리셋한 뒤 영구히 재무장되지 않는 `bResetApplied` 방식은 쓰지 않는다.
- 리셋은 게임 스레드에서 엔티티와 현재 Actor에 **한 번의 정리 작업**으로 적용한다.
  1. 엔티티·Actor 연결과 실제 HP를 다시 읽는다. `Health <= 0` 또는 Dying이면 리셋을 취소한다. 이미 확정된 사망과 보상을 되돌리지 않는다.
  2. CombatBrain 정지, 진행 패턴 취소, 칼날·워프·AI 입력 정리를 §7.7에 따라 완료한다.
  3. 활성 그로기·다운 GA를 명시적으로 종료한다. `FLNPStaggerCommand`의 해제 경로를 재사용하거나 같은 정리를 실행한 뒤 경직 누적·LastHitTime·GroggyElapsed·면역 잔여·상태 플래그를 초기화한다. 프래그먼트만 0으로 만들어 PoiseProcessor의 해제 에지를 없애지 않는다. 저항·임계값 등 Config 유래 값은 보존한다.
  4. 템플릿이 계산한 최대 HP로 엔티티와 현재 Actor ASC를 함께 맞추고, §7.5의 전환 연출 장부를 초기화한다. HP 비율에서 파생하는 페이즈는 1로 돌아간다.
  5. pending·타이머를 정리한 뒤 매크로 트리를 깨운다. Actor가 없을 때도 엔티티에 완료 상태가 남아야 한다.
- **실행 순서는 E4의 완료 조건이다.** 기존 ActorSync·피해 적용·Poise·Health와 같은 PostPhysics 안의 선후 및 커맨드 flush 지점을 확인한다. 이번 프레임의 피해 적용과 Actor→엔티티 HP 반영, 사망 확정 후 살아 있는 개체만 리셋하며, 다음 ActorSync가 이전 HP를 덮지 않도록 한다. 다른 ProcessingPhase에 `ExecuteAfter`만 선언하거나, 프래그먼트 쓰기와 ASC 쓰기를 서로 다른 시점에 예약하는 것으로 보장했다고 보지 않는다. E4 계획에 실제 순서와 치명타/리셋 동시 프레임의 증거를 남긴다.
- 기존 Poise의 자연회복 원칙에서 **아레나 완전 리셋을 별도 예외**로 기록한다. 공통 Poise 문서는 E4 구현과 함께 갱신한다.
- 로그: `[UnboundReset] entity=... hp=...`.

### 6.5 배회 경유점 (Roam)

- `FLNPEnemyRoamTask`가 자기 위치에서 `RoamStepMin`~`RoamStepMax` 거리 대역의 Enemy 플래그 후보를 고른다. 후보 원본은 Surface 데이터의 Spawn 스트림이다.
- 후보의 slot·Layer에서 현재 generation을 붙인 **목적지 Surface**를 만들고 위치와 함께 보존한다. 출발 Surface는 적의 현재 지지면이다. 두 식별 정보로 `ResolveEndpoints`와 경로 요청을 수행한다.
- 기존 `ProjectWanderTarget`은 다른 slot·Layer를 거부하는 같은 표면 전용 헬퍼다. Pod IdleTask에는 이 제약을 유지하고, Roam에는 후보 자체의 Surface를 검증하는 경로를 쓴다. 도달성·안전성의 공통 부분만 공유하며 기존 헬퍼를 그대로 호출해 옥탄트 간 배회를 구현했다고 보지 않는다.
- `ULNPEnemyPathProcessor`는 Roam·Return의 명시적 목적지 Surface를 사용한다. `GoalSurface = Enemy.SurfaceHandle`로 대체하지 않는다. 오래된 generation이면 목표·경로를 폐기하고 새 snapshot에서 재선정한다. Return의 HomeSurface는 베이크 앵커의 slot·Layer로 갱신·재검증하고, 귀환 불가 시 반복 요청하지 않도록 실패 원인을 기록한다. 강제 순간이동은 별도 합의 없이 추가하지 않는다.
- 도착하면 다음 경유점을 고르고, 도달 불가·타임아웃이면 목표를 폐기하고 재추첨한다. 장거리 경로는 기존 스케줄러의 예산·우선순위를 사용한다. 옥탄트를 넘는 경로의 비용·예산은 E4 착수 시 먼저 조사한다(Roadmap §4).
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
| Task | `FLNPActivatePatternTask` | 태그로 해당 패턴의 AbilitySpecHandle을 찾아 발동하고 그 핸들의 종료만 처리한다. 정상 종료는 Succeeded, 취소·실패는 Failed. 동기 종료·강제 Exit의 안전 규칙은 §7.7 |
| Task | `FLNPCombatMoveTask` | 접근·측면 이동·후퇴 목표를 §7.8에 따라 검증하고 기존 경로 조향점을 소비한다. 모드, 목표 거리 대역, 최대 지속 시간을 파라미터로 받는다. 의도는 방향 단위벡터 + 속도로 분리한다 |
| Task | `FLNPFaceTargetTask` | 제자리 회전. 정면각이 임계값 안이면 Succeeded |
| Condition | `FLNPPatternReadyCondition` | 패턴 어빌리티의 쿨다운 태그가 없고 `CanActivateAbility`가 참 |

- 거리·각도·페이즈 조건은 엔진 float·enum 비교 조건을 쓴다. C++ 조건은 위 하나만 만든다.
- 패턴 선택: 패턴 상태들을 형제로 두고 부모의 선택 방식을 Utility Selection으로 한다. Consideration은 거리 커브, 정면각 커브, 페이즈 가중치. 같은 점수면 무작위.
- 페이즈: 현재 페이즈는 HP 비율에서 파생한다. Phase1·Phase2의 조건을 상호 배타적으로 두고, 임계값 통과 시의 명시적 전이와 Staggered 해제 후 재선택을 구성한다. Phase1의 무조건 진입이나 형제 순서만으로 페이즈 전환을 구현하지 않는다. Phase2 전환 연출(어빌리티 1개)의 소비 여부는 §7.5에서 따로 관리한다. 리셋으로 HP가 돌아오면 Phase1로 돌아간다.
- 경직 대기: 최상위 첫 상태로 "Staggered"를 두고, `GA_Stagger`의 입력 차단 태그가 있는 동안 머문다.

### 7.3 패턴 어빌리티 `ULNPAbility_MontagePattern` (`GAS/Abilities/`)

- 기존 근접 어빌리티(`ULNPAbility_MeleeAttack`)는 Chooser로 몽타주를 고르고 콤보 인덱스를 쓴다. 패턴은 "이 몽타주를 한 번"이므로 몽타주를 명시적으로 지정하는 별도 클래스를 둔다.
- 판정은 몽타주의 `ANS_LNPMeleeHitWindow`가 만드는 칼날 엔티티(`FLNPWeaponTraceFragment`)를 그대로 탄다. 그래서 패링·저스트 회피·가드·무적이 `LNPGuardParry::Judge` 한 곳에서 자동으로 성립한다.
- 필드: `Montage`, `DamageMultiplier`, `PoiseDamageMultiplier`, `bUseMotionWarping` + 워프 타겟 이름, 쿨다운(기존 `LNPAbility_BasicAttack`의 Cooldown GE 방식, `GAS/Abilities/LNPAbility_BasicAttack.cpp:66-95`).
- 태그: `Ability.Pattern.<이름>`(발동용), `Ability.Type.Attack`(경직 취소용).
- 원거리 패턴은 기존 `ULNPAbility_RangedAttack`의 발사·조준(`GetBaseAimRotation`, 복제 Pitch)을 재사용한다. 단, 기존 GA는 발사·몽타주 시작 직후 종료하므로 그대로 패턴 수명으로 쓰지 않는다. 원거리 패턴 GA는 선딜·발사·후딜 전체를 소유하고 완료 때 종료한다. 기존 플레이어/일반 원거리 공격의 수명은 바꾸지 않는다. §7.7의 종료·취소 계약을 따른다.

### 7.4 경직·패링 연동

- 경직 명령 `FLNPStaggerCommand::Run`(`GAS/LNPPoiseTypes.cpp:57-140`)은 `CancelCurrentAttackAbility()`(`:92`)를 부른다. 적 구현(`Enemy/LNPEnemyCharacter.cpp:207`)은 지금 무기 핸들만 취소한다. 이를 **`Ability.Type.Attack` 태그를 가진 활성 어빌리티 전체 취소**로 넓힌다. 플레이어 구현은 바꾸지 않는다.
- 패링 브레이크·넉백은 기존 `FLNPMeleeParryCommand`가 Actor 경로로 처리한다. 추가 작업 없음.
- 경직 임계값은 적 Config 데이터(`PoiseStaggerThreshold`·`PoiseDownThreshold`)로 높게 둔다. 코드 변경 없음.

### 7.5 상태 보존 (EL-007)

- Actor는 풀링되지 않고 강등 때 파괴된다(TechDesign_EnemyNPC "Actor 수명"). Confirmed 동안에는 LODOverride가 High를 강제하므로 교전 중 파괴는 드물다.
- 전투 두뇌가 가진 상태(현재 패턴, 쿨다운)는 Actor와 함께 사라진다. 다시 승격되면 쿨다운이 비어 있는 상태로 시작한다. 이를 허용한다.
- 페이즈는 저장하지 않는다. 엔티티 HP 비율에서 매번 파생한다.
- **현재 페이즈와 전환 연출 소비 여부는 다르다.** 전환 연출을 이미 사용했는지는 엔티티의 서버 전용 장부로 보존한다. 경직 후 Phase2 재진입, 타겟 교체, CombatBrain 재시작, Actor 재생성만으로 장부를 지우지 않는다. Arena 완전 리셋 때 초기화하고 Roam은 완전 리셋이 없으므로 Actor 강등만으로 재허용하지 않는다.
- 장부는 연출 GA 발동 성공으로 연출이 시작될 때 소비한다(EL-017, GameRules §6.3). 발동 전에 실패하면 미소비, 시작 후 중단되면 소비를 유지하고 페이즈 패턴으로 진행한다. 이 장부는 EL-007의 HP 기반 페이즈를 대체하지 않으며 새 복제 필드도 아니다.

### 7.6 복제

- 전투 두뇌·패턴 선택은 서버에서만 돈다. 결과는 기존 경로로 복제된다: 몽타주(ASC), GameplayCue, Mover 이동, 복제 Pitch.
- 적 ASC는 Minimal 복제 모드다. 패턴 몽타주가 게스트에서 재생되는지 E5에서 2P로 확인한다(기존 근접 적이 같은 경로를 쓰므로 위험은 낮다).
- 새 복제 필드를 만들지 않는다. 대역폭 예산은 Promoted Actor 1기당 기존 값(700~900 B/s)을 넘지 않아야 한다(E6 측정).

### 7.7 패턴 수명과 종료 계약

- 패턴 GA 하나가 예고·유효 공격·후딜 전체를 소유한다. 발사 1회나 몽타주 시작을 패턴 완료로 해석하지 않는다. PatternTask는 조준·워프를 설정하고 **발동 전에** 종료 구독을 준비한다. 발동 실패와 발동 호출 안의 동기 종료를 모두 처리하며, 성공 핸들을 나중에 저장하는 사이 종료를 놓치지 않는다.
- 구독은 해당 AbilitySpecHandle로 필터링하고 StateTree의 약한 실행 컨텍스트를 사용한다. 종료된 상태의 인스턴스 데이터나 스택의 ExecutionContext를 콜백에서 직접 참조하지 않는다. Exit는 구독 해제와 종료 처리를 멱등으로 수행해 취소 콜백이 이미 종료된 Task를 다시 완료시키지 않게 한다.
- 정상 완료는 해당 GA의 EndAbility가 칼날·워프·패턴 소유 입력을 정리한 뒤 Succeeded로 보고한다. 이미 완료한 GA를 Exit에서 다시 취소하지 않는다.
- 경직, Confirmed 상실, Return 진입, 리셋, 사망, Actor 연결 해제/EndPlay, 트리 교체·중지는 **강제 종료**다. 새 패턴 발동을 먼저 막고, Task가 소유한 활성 GA를 취소하고, 몽타주·칼날·워프·입력을 정리한 뒤 두뇌의 이동 소유권을 반환한다. 경직 GA와 그로기 태그까지 무조건 지우는 것은 리셋 경로에서만 한다.
- 칼날 정리는 NotifyEnd만 가정하지 않는다. 몽타주 취소·Actor 파괴 경로에서도 패턴이 만든 칼날이 다음 유효 판정을 하지 않도록 소유 핸들과 수명 정리를 확인한다. 수명이 남은 기존 비행 발사체의 소거는 별도 정책이며 이 종료 계약만으로 소거한다고 가정하지 않는다. 피격 허용은 §5.4를 계속 따른다.
- `StopCombatBrain()`은 반복 호출 가능해야 한다. 사망 처리에서 명시적으로 호출하며 랙돌·DyingTag 쿼리 제외가 대신 호출해 준다고 가정하지 않는다. E5에서 EndPlay와 리셋까지 같은 종료 함수를 사용하고, 무기 폴백 공격에서 Engage로 넘길 때도 남은 이전 공격을 정리한다.

### 7.8 전투 이동·아레나 경계

- **접근:** 전투 두뇌가 위치 + 목적지 Surface를 정하고 기존 PathProcessor/스케줄러로 우회를 계산한다. CombatMoveTask는 `FLNPEnemyPathFragment::SteeringPoint`를 소비해 Actor 의도를 작성한다. 벽·계단이 있는 상황에서 타겟까지 직선 입력만 넣는 것을 접근 구현으로 보지 않는다.
- **측면 이동·후퇴:** 후보 지면과 목적지 Surface, 경로 도달성, 캡슐 이동 안전성을 검증한다. 도달 불가 또는 지속 시간 초과 시 Task를 종료하고 매크로/패턴 선택에 실패를 보고한다. 제자리에서 같은 실패 목표를 매 프레임 재요청하지 않는다.
- **Arena:** 일반 이동 목표·중간 조향점·워프 목표에 같은 LeashRadius 제약을 적용하고 실제 이동도 검증한다. 경계 밖 성분을 접선으로 바꾸는 것만으로 반경 내부가 보장된다고 보지 않는다. 루트 모션은 시작·종점만이 아니라 이동 구간이 경계를 넘는지 검사한다. E5에서는 필요한 경우 해당 패턴/워프 이동을 제한하거나 취소하여 경계를 지킨다. 넉백 이탈은 §6의 귀환 경로로 처리한다.
- 경계 위치는 서버의 월드 좌표와 HomeCenter 거리로 통일한다. NPC 몸체 기준과 플레이어 추격 자격 기준의 차이(캡슐 중심/지면점·몸체 여유)를 E4에서 명시하고 같은 헬퍼를 사용한다. Return 경로는 바깥에서 안으로 복귀해야 하므로 일반 전투 이동의 내부 후보 제한으로 막지 않는다.
- 복합 근접·원거리 패턴은 기존 Config의 `AttackType`이 인지·슬롯 도달성에도 쓰인다는 제약을 따른다. 첫 콘텐츠에서 접근 가능한 지상 타겟을 기준으로 검증하며, 원거리 패턴이 있다는 이유만으로 기존 근접 슬롯의 도달 불가 판정을 자동 우회하지 않는다. 높은/분리된 지면의 타겟을 원거리로 계속 상대해야 한다면 E5 콘텐츠 요구로 분리해 자격 정책을 추가 결정한다.

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
