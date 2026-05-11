# Engine Analysis - Unreal Engine 5.7 Mass StateTree Architecture

## 1. 개요 (Overview)
StateTree는 언리얼 엔진의 데이터 지향적(Data-oriented) 계층형 상태 머신입니다. Mass Entity 프레임워크와의 결합을 통해 대규모 엔티티의 지능을 효율적으로 처리하며, 전이(Transition) 중심의 설계를 통해 전통적인 AI 아키텍처보다 가볍고 예측 가능한 실행 흐름을 제공합니다.

---

## 2. 노드 베이스 API (Core Node APIs)
StateTree의 모든 노드(Evaluator, Task, Condition)는 공통적인 초기화 및 의존성 선언 과정을 거칩니다.

### 2.1 `Link(FStateTreeLinker& Linker)`
*   **역할**: 노드가 실행 중에 참조할 외부 데이터(Fragment, Subsystem)에 대한 **핸들을 확보(Binding)**하는 단계입니다.
*   **동작 원리**: 런타임에 직접 데이터를 검색하는 비용을 줄이기 위해, 초기화 시점에 `TStateTreeExternalDataHandle`을 데이터 소스에 연결합니다.
*   **필수 구현**: `Linker.LinkExternalData(Handle)`를 통해 모든 핸들을 바인딩해야 합니다. 바인딩되지 않은 핸들을 사용하면 런타임 크래시가 발생합니다.

### 2.2 `GetDependencies(FStateTreeDependencyBuilder& Builder)`
*   **역할**: 노드가 사용하는 외부 데이터의 **접근 권한(ReadOnly/ReadWrite)**을 Mass 프레임워크에 알립니다.
*   **중요성**: Mass의 병렬 실행 시스템(Parallel Execution)은 이 정보를 바탕으로 각 프로세서 간의 충돌을 방지하고 작업 순서를 결정합니다.
*   **필수 구현**: `Link()`에서 바인딩한 모든 핸들에 대해 `AddReadOnly` 또는 `AddReadWrite`를 선언해야 합니다.

### 2.3 `GetInstanceDataType()`
*   **역할**: 각 노드 인스턴스가 독립적으로 가질 데이터 구조체(Instance Data)를 정의합니다.
*   **특징**: StateTree는 노드 본체는 하나만 존재(Flyweight 패턴)하고, 상태별/엔티티별 가변 데이터는 이 함수가 반환하는 구조체에 저장합니다.

---

## 3. 구성 요소별 상세 분석 (Component Deep Dive)

### 3.1 Evaluators (The Perception Layer)
*   **실행 시점**: 상태 선택(State Selection) 직전, 그리고 활성화된 상태의 Tick 중에 실행됩니다.
*   **데이터 흐름**: 외부 환경 데이터(Fragments, Subsystems)를 읽어와서 StateTree 내부 파라미터로 "가공"합니다.
*   **설계 원칙**: 
    *   부작용(Side Effect)이 없어야 합니다. 오직 읽기 전용으로 데이터를 분석하여 결과값을 출력(Output)하는 역할에 집중합니다.
    *   복잡한 로직은 Evaluator에서 처리하고, Task는 그 결과값을 단순 활용하는 구조가 이상적입니다.

### 3.2 Tasks (The Action Layer)
*   **실행 흐름**: `EnterState` → `Tick` → `ExitState`.
*   **Status 반환의 의미**:
    *   `Running`: 동작이 지속 중임. 전이가 발생하거나 Task가 완료될 때까지 상태를 유지합니다.
    *   `Succeeded / Failed`: 동작이 즉시 종료됨을 의미하며, `On Completion` 트랜지션을 유발합니다.
*   **State Latency**: Mass 환경에서 Task는 기본적으로 "Reactive"합니다. 즉, 외부 자극(Signal)이나 강제 Tick이 없으면 `Running` 상태여도 다음 프레임에 로직이 수행되지 않을 수 있습니다.

#### ⚠️ 중요: EnterState 이후 첫 Tick의 지연
상태 전이가 일어날 때 `EnterState`는 해당 전이 처리 사이클 안에서 즉시 호출됩니다. 그러나 `Tick`은 **그 사이클 안에서 호출되지 않습니다.** 첫 번째 `Tick` 호출은 다음 시그널이 도착할 때까지 대기합니다.

```
[시그널 수신 → 사이클 시작]
  OldTask.Tick()  → Failed/Succeeded
  OldTask.ExitState()
  NewTask.EnterState()  → Running 반환
  [← 여기서 사이클 종료. NewTask.Tick()은 아직 안 불림]

[다음 시그널 수신 → 다음 사이클]
  NewTask.Tick()  ← 비로소 여기서 첫 호출
```

이 "한 사이클 지연"을 설계 시 반드시 고려해야 합니다. 특히 `EnterState`에서 Fragment 데이터를 초기화하지 않으면, 첫 `Tick`이 예상치 못한 기존 상태값을 보게 됩니다.

#### ✅ 권장 패턴: EnterState에서 Fragment 초기화
상태에 진입할 때 해당 Task의 동작에 필요한 Fragment 상태를 `EnterState`에서 명시적으로 초기화합니다. 이전 상태가 남긴 값이 첫 `Tick`에 영향을 미치지 않도록 방어합니다.

```cpp
EStateTreeRunStatus FMyTask::EnterState(FStateTreeExecutionContext& Context, ...) const
{
    FMyFragment& Data = Context.GetExternalData(MyHandle);
    Data.bSomeFlag = true;
    Data.LastTime = 0.0;
    // 이전 Task가 남긴 MoveTarget 등도 여기서 리셋
    return EStateTreeRunStatus::Running;
}
```

### 3.3 Transitions & States (The Logic Layer)
*   **계층 구조**: 부모 상태가 활성화되면 자식 상태의 전이 조건도 함께 평가됩니다. (우선순위 기반)
*   **트리거 메커니즘**:
    *   `On Tick`: 매 Tick마다 전이 조건 평가.
    *   `On Event`: 특정 Gameplay Tag 수신 시 즉시 평가.
    *   `On Completion`: 하위 Task 종료 시 평가.
*   **Global States**: 모든 상태에서 공통적으로 체크해야 하는 전이(예: 사망, 기절)는 루트 레벨의 전역 상태로 관리합니다.

---

## 4. Mass Integration: 실행 및 신호 시스템

StateTree가 Mass Entity 환경에서 구동될 때 가장 핵심이 되는 개념입니다.

### 4.1 FMassStateTreeExecutionContext
*   StateTree와 Mass EntityManager 사이의 가교 역할을 합니다.
*   엔티티의 Fragment 데이터 뷰(DataView)를 StateTree 노드들이 접근할 수 있도록 포매팅합니다.

### 4.2 핵심 엔진 프로세서 2종

Mass + StateTree 시스템은 두 개의 엔진 내장 프로세서로 구동됩니다.

#### `UMassStateTreeActivationProcessor` (초기화 전담)
*   **실행 순서**: `ExecuteAfter LOD`, `ExecuteBefore Behavior`
*   **역할**: `FMassStateTreeActivatedTag`가 **없는** 엔티티(= 새로 스폰된 엔티티)를 대상으로, StateTree 인스턴스를 할당하고 `Start()`를 호출합니다. 완료 후 `FMassStateTreeActivatedTag`를 추가하여 이후 실행에서 제외됩니다. 즉, **엔티티당 딱 한 번만 실행**됩니다.
*   **최초 시그널**: 초기화 완료 후 `StateTreeActivate` 시그널을 발송하여, 같은 프레임의 `UMassStateTreeProcessor`가 첫 상태 선택을 수행하도록 유도합니다.

#### `UMassStateTreeProcessor` (실행 전담)
*   **실행 순서**: `ExecuteInGroup Behavior`, `ExecuteAfter SyncWorldToMass`, `ExecuteBefore Tasks`
*   **역할**: 시그널을 받은 엔티티에 대해 `FStateTreeExecutionContext::Tick()`을 호출합니다.
*   **핵심 특성**: `UMassSignalProcessorBase`를 상속합니다. **수신된 시그널이 없으면 즉시 리턴합니다.** 매 프레임 모든 엔티티를 순회하지 않습니다.

```cpp
// MassSignalProcessorBase.cpp
void UMassSignalProcessorBase::Execute(...)
{
    if (ReceivedSignalRanges.IsEmpty())
        return;  // 시그널 없으면 아무것도 안 함
    // ...
}
```

### 4.3 이중 버퍼(Double Buffer) 시그널 메커니즘

`UMassSignalProcessorBase`는 시그널을 두 개의 버퍼(`FrameReceivedSignals[2]`)로 관리합니다.

```
[UMyProcessor::Execute() 실행 중]
  SignalSubsystem.SignalEntities(...)
    → OnSignalReceived() 호출
      → buffer[CurrentIndex]에 시그널 저장

[UMassStateTreeProcessor::Execute() 실행 시작]
  CurrentFrameBufferIndex를 (0→1 또는 1→0)으로 swap
    → 이후 새 시그널은 buffer[1]에 저장됨
  buffer[0] 처리 (← 이번 프레임에 쌓인 시그널 포함!)
```

**동일 프레임 처리의 조건**: 커스텀 프로세서(`UMyProcessor`)가 `UMassStateTreeProcessor`보다 **먼저** 실행된다면, 해당 프레임에 발송한 시그널은 **같은 프레임** 안에서 처리됩니다. 반대라면 다음 프레임에 처리됩니다.

두 프로세서가 모두 `Behavior` 그룹에 속하고 명시적 순서 제약이 없다면, 실행 순서는 엔진 내부 의존성 그래프에 의해 결정됩니다. 동작을 예측 가능하게 만들려면 `ExecuteAfter` / `ExecuteBefore`로 명시적 순서를 지정하는 것을 권장합니다.

### 4.4 이중 Tick 최적화 (Double Tick on Non-Running)

`UMassStateTreeProcessor`는 시그널 처리 사이클 안에서 최대 두 번의 `Tick`을 수행합니다.

```cpp
// MassStateTreeProcessors.cpp (SignalEntities 내부)
StateTreeExecutionContext.Tick(AdjustedDeltaTime);  // 1차 Tick

if (StateTreeExecutionContext.GetLastTickStatus() != EStateTreeRunStatus::Running)
{
    // Task가 Failed/Succeeded → 즉시 전이 시도
    StateTreeExecutionContext.Tick(0.0f);  // 2차 Tick (DeltaTime=0)

    if (StateTreeExecutionContext.GetLastTickStatus() != EStateTreeRunStatus::Running)
    {
        // 그래도 Running 못 찾음 → 다음 프레임 재시도 예약
        SignalSubsystem.SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, entity);
    }
}
```

*   **1차 Tick**: 현재 Task를 실행. `Running`이면 사이클 종료.
*   **2차 Tick(DeltaTime=0)**: Task가 `Failed`/`Succeeded`를 반환한 경우, 즉시 전이를 시도하고 새 상태의 `EnterState`를 호출. 새 상태가 `Running`을 반환하면 사이클 종료.
*   **`NewStateTreeTaskRequired` 시그널**: 두 번의 Tick 후에도 `Running`을 찾지 못하면(적합한 전이 없음), 다음 프레임에 다시 시도하도록 스스로 재시그널합니다.

**결론**: Task가 `Failed`를 반환할 때, `ExitState`와 다음 상태의 `EnterState`는 **같은 시그널 처리 사이클**에서 호출됩니다. 그러나 새 상태의 첫 `Tick`은 **다음 시그널이 올 때까지 호출되지 않습니다.**

### 4.5 주요 시그널 종류

| 시그널 이름 | 발송 주체 | 의미 |
|---|---|---|
| `UE::Mass::Signals::StateTreeActivate` | 커스텀 프로세서, ActivationProcessor | 트리를 깨우고 상태 선택 재수행 |
| `UE::Mass::Signals::NewStateTreeTaskRequired` | UMassStateTreeProcessor 자신 | Running 상태를 찾지 못했으니 다음 프레임 재시도 |
| `UE::Mass::Signals::LookAtFinished` | 엔진 내부 | LookAt 동작 완료 |
| `UE::Mass::Signals::StandTaskFinished` | 엔진 내부 | Stand 동작 완료 |

### 4.6 "도착 시그널(Arrival Signal)" 패턴

StateTree가 시그널 기반이므로, **Running 상태의 Task가 매 프레임 Tick을 받으려면** 외부에서 지속적으로 시그널을 보내야 합니다. 가장 흔한 패턴은 이동 프로세서가 엔티티의 목적지 도착을 감지하여 시그널을 보내는 것입니다.

```cpp
// 커스텀 MovementProcessor::Execute() 내부
if (EffectiveSpeed > 0.0f && DistSq < ArrivalThresholdSq)
{
    EntitiesToSignal.Add(entity);
}
// ...
SignalSubsystem.SignalEntities(UE::Mass::Signals::StateTreeActivate, EntitiesToSignal);
```

이 패턴에서 StateTree의 Tick 주기는 "엔티티가 목적지에 도착하는 빈도"와 같습니다. Task의 `EnterState`에서 `MoveTarget.Center`를 엔티티의 **현재 위치**로 설정하면, 다음 프레임에 MovementProcessor가 즉시 도착을 감지하여 시그널을 보내고, 1~2 프레임 내에 첫 `Tick`을 받을 수 있습니다.

---

## 5. 프레임별 실행 순서 전체 흐름

한 프레임에서 Mass 관련 프로세서들의 실행 순서는 대략 다음과 같습니다.

```
[LOD 그룹]
  MassDistanceLODProcessor (LOD 계산)

[LOD 이후 / Behavior 이전]
  UMassStateTreeActivationProcessor  ← 신규 엔티티만, 1회 초기화 + 첫 시그널

[Behavior 그룹]
  UMassStateTreeProcessor            ← 시그널받은 엔티티만 Tick
  ... (커스텀 Behavior 프로세서들) ...
  * ExecuteAfter/Before 로 상대 순서 제어 가능

[Tasks 그룹]                         ← UMassStateTreeProcessor는 Before:Tasks

[Movement 그룹]
  커스텀 MovementProcessor           ← 이동 처리, 도착 감지, StateTree 재시그널

[UpdateWorldFromMass 그룹]
  Representation 동기화 등
```

> **주의**: `UMassStateTreeProcessor`는 `Behavior` 그룹 안에서 `ExecuteAfter SyncWorldToMass` 만 선언하고 커스텀 프로세서와의 순서는 지정하지 않습니다. 커스텀 프로세서에서 `ExecuteAfter UMassStateTreeProcessor`를 명시해야 같은 프레임 내 시그널 처리가 보장됩니다.

---

## 6. 하이브리드 제어 전략 (Hybrid Control Strategy)

대규모 엔티티의 성능 최적화와 개별 캐릭터의 정교한 시뮬레이션을 동시에 달성하기 위한 설계 패턴입니다.

### 6.1 LOD에 따른 제어권 이양 (Control Handover)
*   **High LOD (Actor 존재)**: 
    *   StateTree Task는 `FMassActorFragment`를 통해 유효한 Actor 인스턴스가 있는지 확인합니다.
    *   직접적인 위치 수정 대신 Actor의 이동 컴포넌트(`MovementComponent` 등)에 **의도(Intent)**나 **입력(Input)**을 전달합니다.
    *   지형 충돌, 장애물 회피, 부드러운 물리 보간은 엔진의 검증된 시스템에 위임합니다.
*   **Low LOD (Actor 부재)**: 
    *   Task가 직접 **Mass Fragment**(`FTransformFragment`, `FMassVelocityFragment`)를 수정합니다.
    *   성능을 위해 Trace 기반 물리 대신 단순화된 수학적 이동(예: 구체 투영, 단순 높이값 유지)을 수행합니다.

### 6.2 의도 기반 제어 (Intent-based Control) Pattern
*   Actor를 직접 제어(`SetActorRotation` 등)하는 것보다, AI의 이동/회전 목표 벡터를 Actor 내부 필드에 기록하는 방식이 권장됩니다.
*   **이점**: Actor 내부의 기존 물리 로직이나 애니메이션 시스템(`AnimInstance`)이 이 의도를 읽어 부드러운 시각적 효과를 독립적으로 생성할 수 있습니다.

### 6.3 Sync Direction의 전략적 선택
Mass Representation 설정의 Sync Direction은 데이터의 주권(Authority)을 결정합니다.
*   **Mass to Actor**: Mass의 로직이 절대적인 주권을 가집니다. 단순 군중 시뮬레이션에 적합합니다.
*   **Actor to Mass**: 엔진 물리 시뮬레이션 결과가 주권이 됩니다. 정교한 개별 AI에 적합합니다.
*   **Both Ways**: 가장 유연한 방식으로, Actor가 있는 동안에는 물리 엔진이 주도하고 Actor가 사라지면 마지막 상태를 Mass가 이어받아 시뮬레이션을 지속합니다.

---

## 7. 실전 문제 해결 가이드 (Troubleshooting)

### 7.1 StateTree가 실행되지 않는 경우 (Silent Skip)
*   **원인**: 커스텀 프로세서의 `ConfigureQueries`에서 Task/Evaluator가 요구하는 Fragment 데이터가 누락됨.
*   **확인**: `AreContextDataViewsValid()` 함수가 `false`를 반환하는지 로그로 체크합니다.
*   **해결**: 프로세서 쿼리에 모든 노드가 사용하는 외부 데이터를 명시적으로 포함해야 합니다.

### 7.2 상태 깜빡임 (Flickering) 및 보간 끊김
*   **원인**: Task가 목표값에 근접했을 때 즉시 `Succeeded`를 반환하여 상태를 종료하고, 다음 프레임에 다시 동일 상태로 진입하는 과정을 반복함.
*   **해결**: 지속적인 업데이트가 필요한 동작(추적, 바라보기 등)은 동작이 완전히 취소되어야 할 때만 `Succeeded/Failed`를 반환하고, 그 외에는 `Running`을 유지하며 매 프레임 보간을 수행합니다.

### 7.3 인스턴스 데이터 할당 실패
*   **증상**: `InstanceData` 핸들이 유효하지 않거나 데이터가 NULL임.
*   **원인**: 엔티티 생성 시 엔진의 `ActivationProcessor`가 작동하지 않았거나, 템플릿에 `FMassStateTreeInstanceFragment`가 누락됨.
*   **해결**: 엔티티 생성 트레잇(Trait)에서 필수 조각들을 정확히 추가하는지 확인합니다.

### 7.4 상태 재진입 후 동작이 이상한 경우 (Stale Fragment Data)
*   **증상**: 전투 → 이동 → 복귀처럼 상태를 순환한 후 이전 상태로 돌아왔을 때, 엔티티가 엉뚱한 방향으로 움직이거나 예상치 못한 행동을 함.
*   **원인**: 이전 Task가 Fragment(예: `FMassMoveTargetFragment.Center`)에 남긴 값이 새 Task의 `EnterState`에서 초기화되지 않은 채 그대로 사용됨. `Tick`은 아직 호출되지 않은 상태이므로 `Tick` 내부의 초기화 코드도 아직 실행되지 않음.
*   **해결**: `EnterState`에서 이 Task가 소유하는 Fragment 데이터를 반드시 초기화합니다.

```cpp
// 잘못된 예: Tick이 호출될 때까지 이전 Task의 MoveTarget.Center가 그대로 남음
EStateTreeRunStatus FMyTask::EnterState(...) const
{
    UE_LOG(LogTemp, Log, TEXT("Entering MyTask"));
    return EStateTreeRunStatus::Running;
}

// 올바른 예: 진입 즉시 Fragment 초기화
EStateTreeRunStatus FMyTask::EnterState(...) const
{
    UE_LOG(LogTemp, Log, TEXT("Entering MyTask"));
    FMyFragment& Data = Context.GetExternalData(MyHandle);
    FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);
    const FTransformFragment& Transform = Context.GetExternalData(TransformHandle);

    Data.bNeedsNewTarget = true;
    Data.LastActionTime = 0.0;
    MoveTarget.Center = Transform.GetTransform().GetLocation(); // "현 위치 도착" 즉시 트리거
    return EStateTreeRunStatus::Running;
}
```

### 7.5 첫 Tick이 오랫동안 호출되지 않는 경우 (Delayed First Tick)
*   **증상**: `"Entering XxxTask"` 로그는 보이지만 `Tick` 내부 로그가 수 초 동안 나타나지 않음.
*   **원인**: `EnterState` 이후 첫 `Tick`은 다음 시그널이 도착해야 호출됩니다. 시그널 발신 주체(예: MovementProcessor의 도착 감지)가 이전 Task가 남긴 잘못된 `MoveTarget.Center`를 기준으로 동작하면, 엔티티가 그 위치에 도달할 때까지 시그널이 오지 않습니다.
*   **해결**: 7.4의 방법으로 `EnterState`에서 `MoveTarget.Center`를 현재 위치로 초기화하면, 다음 프레임 MovementProcessor가 즉시 도착을 감지하여 시그널을 보냅니다.

### 7.6 시그널이 다음 프레임에야 처리되는 경우 (Signal Processing Delay)
*   **증상**: 프로세서에서 `SignalEntities()`를 호출했는데, 상태 전이 로그가 다음 프레임에 나타남.
*   **원인**: `UMassStateTreeProcessor`가 커스텀 프로세서보다 **먼저** 실행되어, 해당 프레임의 시그널 버퍼를 이미 처리한 뒤임.
*   **해결**: 커스텀 프로세서에 `ExecutionOrder.ExecuteAfter.Add(UMassStateTreeProcessor::StaticClass()->GetFName())` 또는 더 안전하게 `ExecuteBefore`로 StateTree가 나중에 오도록 명시합니다.

---

## 8. 설계 시 고려해야 할 정신 모델 (Mental Model)

1.  **"누가 트리를 깨우는가?"**: 트리는 가만히 있으면 아무것도 하지 않습니다. 신호(Signal)가 보내지고 있는지, 아니면 프로세서가 매 프레임 돌고(Continuous) 있는지 명확히 인지해야 합니다.
2.  **"EnterState와 첫 Tick 사이에는 한 사이클 간격이 있다"**: `EnterState`가 호출된 뒤 첫 `Tick`은 다음 시그널이 올 때 실행됩니다. `EnterState`를 Fragment 초기화의 장소로 적극 활용해야 합니다.
3.  **"데이터는 어디에 있는가?"**: StateTree 내부 파라미터는 휘발성입니다. 영구적인 상태나 데이터는 반드시 Fragment에 저장하고 핸들을 통해 접근해야 합니다.
4.  **"전이는 명확한가?"**: `Running` 상태에서 빠져나올 수 있는 전이 경로가 항상 존재해야 합니다. 그렇지 않으면 엔티티는 특정 상태에 영원히 갇힐 수 있습니다.
5.  **"시그널 체인이 끊어지지 않는가?"**: `Running` Task는 스스로 다음 시그널을 유발하는 메커니즘(도착 감지, 타이머, 외부 프로세서)이 있어야 합니다. 체인이 끊어지면 엔티티는 살아있지만 아무것도 하지 않는 상태가 됩니다.
6.  **"LOD와 연동되는가?"**: Mass의 LOD 시스템과 결합하여, 먼 거리에 있는 엔티티는 신호 기반으로 동작하게 하고 가까운 엔티티는 강제 Tick하게 설계하는 것이 대규모 최적화의 정석입니다.
