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

## 2. 데이터 구조 (구현 완료)

### 2.1 Fragments & Tags

**`FLNPEnemyFragment`** — 적 개체 고유 상태
- `float Health` / `float MaxHealth` / `float Defense`
- `FGameplayTag EnemyTypeTag` — 타입 식별 및 슬롯 분류(Melee/Ranged) 기준
- `FMassEntityHandle ParentLootPod` — 연결된 LootPod 엔티티 핸들
- `FVector ParentPodLocation` — 배회 중심점 (리쉬 계산 기준)
- `float MaxLeashDistance` — 부모 Pod에서 최대 이탈 허용 거리

**`FLNPEnemySharedFragment`** — 동일 타입 적들이 공유
- `TObjectPtr<ULNPEnemyConfig> Config` — 메쉬, 어빌리티, 이동 설정 포인터

**`FLNPEnemyTargetingFragment`** — 타겟팅 상태
- `FMassEntityHandle TargetPlayer` — 현재 추적 중인 플레이어 엔티티
- `ELNPTargetingState State` — `None` / `Requesting` / `Confirmed` / `Releasing`
- `FVector LastKnownTargetLocation` — 마지막으로 확인한 타겟 위치 (시야 상실 시 사용)
- `float DistanceToTargetSq` — 이번 프레임 계산된 거리 제곱값 (슬롯 점수 입력)

**Tags:** `FLNPEnemyTag` / `FLNPPlayerTag` — 쿼리 분류용 마커

### 2.2 Data Assets

**`ULNPEnemyConfig`** (`UPrimaryDataAsset`)
- `EnemyTypeTag` — 적 타입 Gameplay Tag (Melee/Ranged 분류 포함)
- `EnemyActorClass` — High LOD Actor 클래스
- `SkeletalMesh` / `AnimBlueprint` — 비주얼 설정
- `DefaultAbilities` — 초기 부여 GAS 어빌리티 목록
- `TargetingConfig` — 슬롯 점수 가중치 (거리, 각도, 상태)
- `MovementConfig` — 이동 속도, `GravityOrigin` (구체 중심 좌표)

---

## 3. 핵심 서브시스템

### 3.1 ULNPTargetingSubsystem (구현 완료)

플레이어별 슬롯 현황을 관리하는 게임스레드 전용 서브시스템.

```
슬롯 한도 (UPROPERTY, 에디터 설정 가능)
- MaxMeleeSlotsPerPlayer = 10
- MaxRangedSlotsPerPlayer = 20
```

**주요 함수**
- `RegisterEnemyInterest(EnemyHandle, PlayerHandle, Score, TypeTag)` — 적이 이번 프레임 점수를 등록
- `RebalanceSlots()` — 점수 기반 슬롯 재할당 (매 프레임 호출)
- `IsSlotConfirmed(EnemyHandle, PlayerHandle)` — 슬롯 점유 여부 조회

**스레드 안전성:** `FCriticalSection DataLock`으로 Mass 워커 스레드와의 경합 방지.

---

## 4. Mass 프로세서 (구현 완료)

### 4.1 ULNPEnemyScoringProcessor

`FLNPEnemyTargetingFragment`의 `DistanceToTargetSq`를 계산하고 `RegisterEnemyInterest()`를 호출하여 이번 프레임의 점수를 서브시스템에 등록.

### 4.2 ULNPEnemyTargetingProcessor

`IsSlotConfirmed()` 결과를 바탕으로 `FLNPEnemyTargetingFragment::State`를 동기화.
- Confirmed → 적극 추격
- None/Releasing → 배회 전환

### 4.3 ULNPEnemyMovementProcessor

`State`에 따라 이동 벡터를 계산하고 `FTransformFragment`를 갱신.

**구형 내벽 UpDir 계산:**
```cpp
const FVector UpDir = (GravityOrigin - EntityLocation).GetSafeNormal();
// 내벽 세계에서 Up = 구 중심 방향 (RadialOutward 중력)
```
수학적 연산으로 처리하며 별도 GravityProcessor는 존재하지 않음.

---

## 5. StateTree 행동 정의 (구현 완료)

`ULNPEnemyStateTreeProcessor`가 Mass와 StateTree를 연결.

### Evaluator

**`FLNPEnemyStateEvaluator`** — 매 프레임 Fragment 데이터를 StateTree 컨텍스트에 노출.
- `FLNPEnemyFragment` / `FLNPEnemyTargetingFragment` / `FTransformFragment` 바인딩

### Tasks

| Task | 동작 |
|:---|:---|
| `FLNPEnemyLookAtTask` | 타겟 방향으로 회전 (구형 UpDir 보정 포함) |
| `FLNPEnemySteeringTask` | 타겟을 향한 조향 이동 (MoveInput 설정) |
| `FLNPEnemyIdleTask` | 부모 Pod 위치 기반 배회. 표면 접선 벡터로 이동 방향 결정 |

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
- `InitializeFromConfig(ULNPEnemyConfig*)` — 메쉬, 어빌리티 초기화
- `SyncFromEntity(Health, State)` — Mass → Actor 데이터 주입
- `SyncToEntity(out Health)` — Actor → Mass 역동기화

**플레이어 등록:** `ALNPCharacterBase::BeginPlay()`에서 `MassAgentComponent`가 `FLNPPlayerTag`로 등록 → ScoringProcessor의 쿼리 대상이 됨.

---

## 7. 미구현 항목 (구현 예정)

| 항목 | 설명 |
|:---|:---|
| `ULNPHealthProcessor` | 체력 감소 처리 및 사망(Entity Destroy) — HitDetection 완성 후 구현 |
| GAS 어빌리티 실행 | `DefaultAbilities` 구조는 있으나 실제 발동 로직 미구현 |
| 난이도 스케일링 | 잔여 LootPod 수 기반 NPC 강화 시스템 |
| LOD 전환 역동기화 | Actor에서 Mass로 돌아올 때 피격 정보 보존 로직 |

---

## 8. 성능 고려사항

- **SharedFragment:** 동일 타입 적들이 `ULNPEnemyConfig` 포인터를 공유하여 메모리 절약.
- **UpDir 실시간 계산:** 프래그먼트에 저장하지 않고 필요 시 계산 → 캐시 효율 유지.
- **SurfaceCache 연동 (예정):** 지형 표면 쿼리가 필요해지면 `ULNPSurfaceCacheSubsystem::GetSurfacePoint()`로 O(1) 조회. 자세한 내용은 [TechDesign_SurfaceCache.md](TechDesign_SurfaceCache.md) 참조.
