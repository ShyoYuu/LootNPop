# TechDesign - Enemy NPC StateTree

## 1. 개요
Enemy NPC의 지능을 MassEntity와 StateTree를 결합하여 구현한다. 길찾기(Navigation)를 배제한 단순 Steering 기반 추적과 데이터 기반(Data-Driven) 상태 전환을 목표로 한다.

## 2. StateTree 구조 (ST_EnemyEntity)

### Root 및 상태 계층 (우선순위 순)
StateTree는 위에서 아래로 순차적으로 조건을 검사하여 상태를 선택한다. 각 상태는 유지 조건을 만족하지 못할 경우 **Transition to Root**를 통해 재선택을 유도한다.

1.  **Combat (State, Priority 1)**
    *   **Enter Condition**: `TargetingState == Confirmed`
    *   **Task**: `LNPEnemyLookAtTask` (부모 상태에서 공통 실행 - 플레이어 주시)
    *   **Child States**:
        *   **Attack (Priority 1)**: `DistanceToTarget <= AttackRange` 일 때 공격 어빌리티 실행.
        *   **Chase (Priority 2)**: 사거리 밖일 때 `LNPEnemySteeringTask`로 추적.
    *   **Transition**: `On Tick`, `TargetingState != Confirmed` -> **Transition to Root**

2.  **Alert (State, Priority 2)**
    *   **Enter Condition**: `TargetingState == Alert`
    *   **Task**: `LNPEnemyLookAtTask` (플레이어를 정면으로 바라봄)
    *   **Transition**: `On Tick`, `TargetingState == None` -> **Transition to Root**

3.  **Idle (State, Priority 3)**
    *   **Enter Condition**: (없음 - 상위 상태 조건 실패 시 기본 선택)
    *   **Transition**: `On Tick`, `TargetingState != None` -> **Transition to Root**

## 3. 핵심 C++ 구성 요소

### A. Evaluators (데이터 공급원)
Evaluator는 `FLNPEnemyStateEvaluatorInstanceData`를 `FInstanceDataType`으로 사용하며, Mass Fragment의 데이터를 StateTree 내부 변수로 전달한다.

**FLNPEnemyStateEvaluator** (통합 Evaluator)
*   **Source**: `FLNPEnemySharedFragment` (Config), `FLNPEnemyTargetingFragment`, `FLNPEnemyTargetingCandidateFragment`
*   **Logic**: 후보 목록 및 슬롯 상태(`ELNPTargetingState`)를 읽어 StateTree에 노출. 시야 감지(`VisionDistance`, `VisionAngle`, `AwarenessDistance`)는 ScoringProcessor에서 처리하여 CandidateFragment에 기록된 결과를 사용.
*   **Outputs**:
    *   `TargetingState` (ELNPTargetingState) — `None` / `Alert` / `Confirmed`
    *   `DistanceToTarget` (float)
    *   `AttackRange` (float) — Config에서 가져와 전달

### B. Tasks (동작 수행)
모든 Task는 `GetDependencies`를 통해 필요한 Fragment의 읽기/쓰기 권한을 명시적으로 선언한다.

1.  **LNPEnemyLookAtTask**
    *   **Logic**: `FMath::QInterpConstantTo`를 사용하여 `RotationRate`(Config)에 맞춰 타겟을 부드럽게 바라보도록 `FMassMoveTargetFragment`를 업데이트한다. Actor가 유효할 경우 Actor의 회전도 동기화한다.

2.  **LNPEnemySteeringTask**
    *   **Logic**: `MoveSpeed`(Config)를 적용하여 타겟 위치를 `FMassMoveTargetFragment`의 목적지로 설정한다.

3.  **LNPEnemyAttackTask**
    *   **Logic**: Actor 상태일 때 GAS 어빌리티를 트리거하고, 사거리 이탈 또는 타겟 상실 시 `Failed`를 반환하여 Chase 상태로의 전환을 유도한다.

4.  **LNPEnemyIdleTask**
    *   **Logic**: `FLNPEnemyIdleFragment`의 `LastWanderTime`과 `bNeedNewWanderTarget`을 기반으로 일정 시간마다 새 배회 목표를 설정한다. 목표 방향은 LootPod 위치의 구면 표면 접선 벡터로 계산.

## 4. 액터 스폰 조건 (Representation)
`ULNPEnemyLODOverrideProcessor`에서 다음 로직을 수행한다:
*   엔티티의 타겟팅 상태가 `Confirmed`이거나 StateTree가 `Combat` 상태군에 있을 경우, 거리에 상관없이 `HighResSpawnedActor` (Actor) 상태를 강제한다.
