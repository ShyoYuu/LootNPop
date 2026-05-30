# Enemy NPC 시스템 기술 설계

## 1. 시스템 아키텍처

3단계 파이프라인으로 구성.

```
[MassEntity] ──── 데이터/시뮬레이션 (수백~수천 유닛, 병렬 처리)
     │
     ▼
[ULNPTargetingSubsystem] ──── 슬롯 기반 타겟 경쟁 로직 (게임스레드)
     │
     ▼
[ALNPEnemyCharacter + GAS] ──── 근접 전투 비주얼 / 어빌리티 실행 (High LOD)
```

---

## 2. 데이터 구조

### 2.1 Fragments & Tags

**`FLNPEnemyFragment`** — 적 개체 고유 상태
- `float Health` / `float MaxHealth` / `float Defense`
- `float DeathCountdown` — 사망 시 엔티티 소멸까지 남은 시간(초). 체력 0 시 설정됨
- `FGameplayTag EnemyTypeTag` — 타입 식별 및 슬롯 분류(Melee/Ranged) 기준
- `FMassEntityHandle ParentLootPod` — 연결된 LootPod 엔티티 핸들
- `FVector ParentPodLocation` — 배회 중심점 (리쉬 계산 기준)

**`FLNPEnemySharedFragment`** — 동일 타입 적들이 공유
- `TObjectPtr<ULNPEnemyConfig> Config` — 설정 데이터 에셋 포인터

**`FLNPEnemyTargetingCandidateFragment`** — 인식 단계 후보 목록
- `FMassEntityHandle PotentialTargets[4]` — 우선순위 정렬된 잠재 타겟 플레이어 배열
- `uint8 NumPotentialTargets` — 유효한 후보 수

**`FLNPEnemyTargetingFragment`** — 슬롯 경쟁 결과 및 최종 타겟 상태
- `FMassEntityHandle TargetPlayer` — 현재 추적 중인 플레이어 엔티티
- `ELNPTargetingState State` — `None` / `Alert` / `Confirmed`
- `FVector TargetLocation` — 마지막으로 확인한 타겟 위치 (시야 상실 시 사용)
- `float DistanceToTargetSq` — 이번 프레임 계산된 거리 제곱값 (슬롯 점수 입력)

**`FLNPEnemyIdleFragment`** — 배회 행동 지속 상태
- `double LastWanderTime` — 마지막 배회 목표 갱신 시각
- `uint8 bNeedNewWanderTarget` — 새 배회 목표 필요 여부 플래그

**Tags:**
- `FLNPEnemyTag` / `FLNPPlayerTag` — 쿼리 분류용 마커
- `FLNPEnemyActorInitializedTag` — Actor 초기화 완료 마커 (중복 초기화 방지)
- `FLNPEnemyDyingTag` — 체력 0 이후 소멸 대기 중인 엔티티 마커

### 2.2 Data Assets

**`ULNPEnemyConfig`** (`UPrimaryDataAsset`)
- `EnemyTypeTag` — 적 타입 Gameplay Tag (Melee/Ranged 분류 포함)
- `EnemyActorClass` — High LOD Actor 클래스
- `StateTree` — 이 적 타입이 사용할 StateTree 에셋
- `WeaponData` — 공격에 사용할 무기 데이터 (`ULNPWeaponData`)
- `DefaultAbilities` — 초기 부여 GAS 어빌리티 목록
- `InitialAttributeValues` — 초기 속성값 맵 (체력, 방어력 등 Gameplay Tag → float)
- `TargetingConfig` (`FLNPEnemyTargetingConfig`) — 인식 및 슬롯 점수 설정
- `MovementConfig` (`FLNPEnemyMovementConfig`) — 이동·회전·중력 설정
- `CapsuleHalfHeight` / `CapsuleRadius` — 히트 감지용 캡슐 크기

**`FLNPEnemyTargetingConfig`** — TargetingConfig 내부 서브 구조체
- `DistanceWeight` / `AngleWeight` — 슬롯 점수 가중치
- `MaxTargetingDistance` — 타겟 고려 최대 거리
- `MaxLeashDistance` — LootPod에서 이 거리 초과 시 점수 0 (이탈 제한)
- `VisionDistance` / `VisionAngle` — 시야 거리 및 FOV 각도
- `AwarenessDistance` — FOV 무관 근거리 무조건 감지 반경

**`FLNPEnemyMovementConfig`** — MovementConfig 내부 서브 구조체
- `MoveSpeed` / `RotationRate` — 이동 속도 및 회전 속도 (deg/s)
- `GravityStrength` / `GravityOrigin` — 구체 중력 크기 및 중심 좌표
- `WanderMinDistance` / `WanderMaxDistance` — 배회 반경 범위
- `AttackRange` / `AttackInterval` — 공격 사거리 및 쿨다운(초)

---

## 3. 핵심 서브시스템

### 3.1 ULNPTargetingSubsystem

플레이어별 슬롯 현황을 관리하는 게임스레드 전용 서브시스템.

```
슬롯 한도 (UPROPERTY, 에디터 설정 가능)
- MaxMeleeSlotsPerPlayer = 10
- MaxRangedSlotsPerPlayer = 20
```

**내부 데이터 구조**
- `FLNPPendingTargetEntry` — 이번 프레임 등록된 경쟁 항목 (EnemyHandle, PlayerHandle, Score, bIsMelee)
- `FLNPPlayerSlotData` — 플레이어별 점유 세트 (`OccupiedMelee` / `OccupiedRanged`, 각 `TSet<FMassEntityHandle>`)

**주요 함수**
- `RegisterEnemyInterest(EnemyHandle, PlayerHandle, Score, bIsMelee)` — 이번 프레임 점수 등록 (`bIsMelee`로 Melee/Ranged 슬롯 분류)
- `RebalanceSlots()` — 점수 기반 슬롯 재할당 (매 프레임 호출)
- `IsSlotConfirmed(EnemyHandle, PlayerHandle)` — 슬롯 점유 여부 조회

**스레드 안전성:** `mutable FCriticalSection DataLock`으로 Mass 워커 스레드와의 경합 방지.

---

## 4. Mass 프로세서

### 4.1 ULNPEnemyScoringProcessor

플레이어 목록을 순회하며 시야(`VisionDistance`, `VisionAngle`, `AwarenessDistance`) 및 거리 기반으로 `FLNPEnemyTargetingCandidateFragment`에 후보를 채우고, `RegisterEnemyInterest()`로 슬롯 경쟁 점수를 서브시스템에 등록.

### 4.2 ULNPEnemyTargetingProcessor

`IsSlotConfirmed()` 결과와 후보 목록을 바탕으로 `FLNPEnemyTargetingFragment::State`를 동기화.
- 슬롯 확보 → `Confirmed`
- 후보는 있으나 슬롯 미확보 → `Alert`
- 후보 없음 → `None`

### 4.3 ULNPEnemyTargetFollowProcessor

`State`에 따라 `FMassMoveTargetFragment`의 목적지를 갱신. StateTree에 거리 기반 시그널 전달도 담당.

### 4.4 ULNPEnemyMovementProcessor

`FMassMoveTargetFragment`의 이동 의도를 읽어 실제 이동/회전을 `FTransformFragment`에 적용.

**구형 내벽 UpDir 계산:**
```cpp
const FVector UpDir = (GravityOrigin - EntityLocation).GetSafeNormal();
// 내벽 세계에서 Up = 구 중심 방향 (RadialOutward 중력)
```
수학적 연산으로 처리하며 별도 GravityProcessor는 존재하지 않음.

### 4.5 ULNPHealthProcessor

체력 변화를 처리하고 체력이 0이 되면 `FLNPEnemyDyingTag`를 추가하여 사망 처리 흐름으로 전환.

### 4.6 ULNPEnemyLODOverrideProcessor

타겟팅 상태 및 StateTree 상태에 따라 MassRepresentation의 LOD를 강제 재정의. `Confirmed` 상태이거나 Combat 상태군이면 거리 무관 `HighResSpawnedActor` 강제.

### 4.7 ULNPEnemyActorInitializerProcessor

Actor 스폰 후 `FLNPEnemyActorInitializedTag`가 없는 엔티티를 감지하여 Config 기반 초기화 수행. 초기화 완료 시 태그 추가.

### 4.8 ULNPEnemyActorSyncProcessor

게임스레드에서 Representation 그룹 이후 실행.
- Actor 유효 → ASC 체력을 `FLNPEnemyFragment`로 역동기화 (`SyncToEntity`)
- Actor null → `FLNPEnemyActorInitializedTag` 제거 (다음 스폰 시 재초기화 유도)

### 4.9 ULNPEnemyDeathTimerProcessor

`FLNPEnemyDyingTag`가 붙은 엔티티의 `DeathCountdown`을 매 프레임 차감. 0 이하가 되면 엔티티를 소멸.

---

## 5. StateTree 행동 정의

Mass StateTree 프레임워크를 통해 각 엔티티가 독립적인 StateTree 인스턴스를 실행.

### Evaluator

**`FLNPEnemyStateEvaluator`** — 인식·타겟팅 데이터를 StateTree 컨텍스트에 통합 노출.
- `FLNPEnemySharedFragment` / `FLNPEnemyTargetingFragment` / `FLNPEnemyTargetingCandidateFragment` 바인딩
- **출력 변수:** `TargetingState` (ELNPTargetingState), `DistanceToTarget` (float), `AttackRange` (float)

### Tasks

| Task | 동작 |
|:---|:---|
| `FLNPEnemyLookAtTask` | 타겟 방향으로 회전 (구형 UpDir 보정 포함, Actor 동기화) |
| `FLNPEnemySteeringTask` | 타겟을 향한 조향 이동 (`FMassMoveTargetFragment` 설정) |
| `FLNPEnemyAttackTask` | Actor 상태에서 GAS 어빌리티 트리거. 사거리 이탈 시 `Failed` 반환 |
| `FLNPEnemyIdleTask` | `FLNPEnemyIdleFragment` 기반 시간제 배회 목표 갱신 |

**IdleTask 배회 방향 계산 (구면 표면 접선):**
```cpp
const FVector UpDir = (GravityOrigin - ParentPodLocation).GetSafeNormal();
const FVector Tangent1 = Cross(UpDir, ArbitraryAxis).GetSafeNormal();
const FVector Tangent2 = Cross(UpDir, Tangent1).GetSafeNormal();
// {±Tangent1, ±Tangent2} 중 무작위 선택 → 내벽 표면에 평행한 이동 보장
```

---

## 6. Actor 연동 (High LOD)

**`ALNPEnemyCharacter`** — MassRepresentation 전환 시 스폰되는 비주얼 Actor.
- `InitializeFromConfig(ULNPEnemyConfig*)` — 어빌리티 초기화
- `SyncFromEntity(Health, State)` — Mass → Actor 데이터 주입
- `SyncToEntity(out Health)` — Actor → Mass 역동기화

**LOD 전환 흐름:** `ULNPEnemyActorInitializerProcessor`가 스폰 감지 → 초기화 → `FLNPEnemyActorInitializedTag` 추가. `ULNPEnemyActorSyncProcessor`가 매 프레임 역동기화 및 디스폰 시 태그 제거.

**플레이어 등록:** `ALNPCharacterBase::BeginPlay()`에서 `MassAgentComponent`가 `FLNPPlayerTag`로 등록 → ScoringProcessor의 쿼리 대상이 됨.

### 월드 스페이스 HP Bar

Actor 상태일 때만 존재하는 `UWidgetComponent` 기반 HP Bar.

- **컴포넌트:** `HpBarComponent` (`UWidgetComponent`, World Space, Transparent 블렌드)
- **위젯:** `ULNPHpBarWidget` — `BindWidget UProgressBar* HpBar`를 직접 소유. `UpdateHpBar(float Current, float Max)`가 `SetPercent(Current / Max)` 호출.
- **가시 조건:** `HP > 0 && HP < MaxHP`. `RefreshHpBar()`가 조건 검사 후 컴포넌트 visibility 제어.
- **갱신 경로:**
  - 스폰 시: `SyncFromEntity()` → `RefreshHpBar()` (초기값)
  - 전투 중: ASC `GetGameplayAttributeValueChangeDelegate(Health)` → `OnHpAttributeChanged()` → `RefreshHpBar()`
- **설정:** `HpBarWidgetClass` (`EditDefaultsOnly`)를 적 캐릭터 BP CDO에서 `WBP_LNPHpBar`로 지정. Blueprint 내 ProgressBar 이름은 반드시 `HpBar`.

---

## 7. 미구현 항목 (구현 예정)

| 항목 | 설명 |
|:---|:---|
| 난이도 스케일링 | 잔여 LootPod 수 기반 NPC 강화 시스템 |

---

## 8. 성능 고려사항

- **SharedFragment:** 동일 타입 적들이 `ULNPEnemyConfig` 포인터를 공유하여 메모리 절약.
- **UpDir 실시간 계산:** 프래그먼트에 저장하지 않고 필요 시 계산 → 캐시 효율 유지.
- **SurfaceCache 연동 (예정):** 지형 표면 쿼리가 필요해지면 `ULNPSurfaceCacheSubsystem::GetSurfacePoint()`로 O(1) 조회. 자세한 내용은 [TechDesign_SurfaceCache.md](TechDesign_SurfaceCache.md) 참조.
