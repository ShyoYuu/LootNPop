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
*   **실행 흐름**: `EnterState` -> `Tick` -> `ExitState`.
*   **Status 반환의 의미**:
    *   `Running`: 동작이 지속 중임. 전이가 발생하거나 Task가 완료될 때까지 상태를 유지합니다.
    *   `Succeeded / Failed`: 동작이 즉시 종료됨을 의미하며, `On Completion` 트랜지션을 유발합니다.
*   **State Latency**: Mass 환경에서 Task는 기본적으로 "Reactive"합니다. 즉, 외부 자극(Signal)이나 강제 Tick이 없으면 `Running` 상태여도 다음 프레임에 로직이 수행되지 않을 수 있습니다.

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

### 4.2 Signal-driven 모델 vs Continuous 모델
1.  **신호 기반 (Signal-driven)**:
    *   기본 엔진 프로세서(`UMassStateTreeProcessor`)가 사용하는 방식입니다.
    *   `MassSignalSubsystem`을 통해 "너의 상태를 업데이트해라"라는 신호를 받은 엔티티만 Tick합니다.
    *   **장점**: CPU 비용이 극도로 낮음.
    *   **단점**: 매 프레임 부드러운 업데이트(Smooth Rotation 등)가 필요한 경우 적합하지 않음.

2.  **지속적 실행 (Continuous/Forced Tick)**:
    *   커스텀 프로세서를 구현하여 특정 태그를 가진 엔티티를 매 프레임 Tick합니다.
    *   **장점**: 반응성이 높고 부드러운 로직 수행 가능.
    *   **단점**: 엔티티 수에 따라 성능 부하 증가.

### 4.3 신호 전파 (`MassSignalSubsystem`)
*   `StateTreeActivate`: 트리를 깨우고 상태 선택을 다시 수행하게 하는 가장 일반적인 신호입니다.
*   데이터 프래그먼트의 값이 바뀌는 시점(예: 타겟 감지)에 신호를 보내는 것이 효율적인 AI 설계의 핵심입니다.

---

## 5. 하이브리드 제어 전략 (Hybrid Control Strategy)

대규모 엔티티의 성능 최적화와 개별 캐릭터의 정교한 시뮬레이션을 동시에 달성하기 위한 설계 패턴입니다.

### 5.1 LOD에 따른 제어권 이양 (Control Handover)
*   **High LOD (Actor 존재)**: 
    *   StateTree Task는 `FMassActorFragment`를 통해 유효한 Actor 인스턴스가 있는지 확인합니다.
    *   직접적인 위치 수정 대신 Actor의 이동 컴포넌트(`MovementComponent` 등)에 **의도(Intent)**나 **입력(Input)**을 전달합니다.
    *   지형 충돌, 장애물 회피, 부드러운 물리 보간은 엔진의 검증된 시스템에 위임합니다.
*   **Low LOD (Actor 부재)**: 
    *   Task가 직접 **Mass Fragment**(`FTransformFragment`, `FMassVelocityFragment`)를 수정합니다.
    *   성능을 위해 Trace 기반 물리 대신 단순화된 수학적 이동(예: 구체 투영, 단순 높이값 유지)을 수행합니다.

### 5.2 의도 기반 제어 (Intent-based Control) Pattern
*   Actor를 직접 제어(`SetActorRotation` 등)하는 것보다, AI의 이동/회전 목표 벡터를 Actor 내부 필드에 기록하는 방식이 권장됩니다.
*   **이점**: Actor 내부의 기존 물리 로직이나 애니메이션 시스템(`AnimInstance`)이 이 의도를 읽어 부드러운 시각적 효과를 독립적으로 생성할 수 있습니다.

### 5.3 Sync Direction의 전략적 선택
Mass Representation 설정의 Sync Direction은 데이터의 주권(Authority)을 결정합니다.
*   **Mass to Actor**: Mass의 로직이 절대적인 주권을 가집니다. 단순 군중 시뮬레이션에 적합합니다.
*   **Actor to Mass**: 엔진 물리 시뮬레이션 결과가 주권이 됩니다. 정교한 개별 AI에 적합합니다.
*   **Both Ways**: 가장 유연한 방식으로, Actor가 있는 동안에는 물리 엔진이 주도하고 Actor가 사라지면 마지막 상태를 Mass가 이어받아 시뮬레이션을 지속합니다.

---

## 6. 실전 문제 해결 가이드 (Troubleshooting)

### 6.1 StateTree가 실행되지 않는 경우 (Silent Skip)
*   **원인**: 커스텀 프로세서의 `ConfigureQueries`에서 Task/Evaluator가 요구하는 Fragment 데이터가 누락됨.
*   **확인**: `AreContextDataViewsValid()` 함수가 `false`를 반환하는지 로그로 체크합니다.
*   **해결**: 프로세서 쿼리에 모든 노드가 사용하는 외부 데이터를 명시적으로 포함해야 합니다.

### 6.2 상태 깜빡임 (Flickering) 및 보간 끊김
*   **원인**: Task가 목표값에 근접했을 때 즉시 `Succeeded`를 반환하여 상태를 종료하고, 다음 프레임에 다시 동일 상태로 진입하는 과정을 반복함.
*   **해결**: 지속적인 업데이트가 필요한 동작(추적, 바라보기 등)은 동작이 완전히 취소되어야 할 때만 `Succeeded/Failed`를 반환하고, 그 외에는 `Running`을 유지하며 매 프레임 보간을 수행합니다.

### 6.3 인스턴스 데이터 할당 실패
*   **증상**: `InstanceData` 핸들이 유효하지 않거나 데이터가 NULL임.
*   **원인**: 엔티티 생성 시 엔진의 `ActivationProcessor`가 작동하지 않았거나, 템플릿에 `FMassStateTreeInstanceFragment`가 누락됨.
*   **해결**: 엔티티 생성 트레잇(Trait)에서 필수 조각들을 정확히 추가하는지 확인합니다.

---

## 7. 설계 시 고려해야 할 정신 모델 (Mental Model)

1.  **"누가 트리를 깨우는가?"**: 트리는 가만히 있으면 아무것도 하지 않습니다. 신호(Signal)가 보내지고 있는지, 아니면 프로세서가 매 프레임 돌고(Continuous) 있는지 명확히 인지해야 합니다.
2.  **"데이터는 어디에 있는가?"**: StateTree 내부 파라미터는 휘발성입니다. 영구적인 상태나 데이터는 반드시 Fragment에 저장하고 핸들을 통해 접근해야 합니다.
3.  **"전이는 명확한가?"**: `Running` 상태에서 빠져나올 수 있는 전이 경로가 항상 존재해야 합니다. 그렇지 않으면 엔티티는 특정 상태에 영원히 갇힐 수 있습니다.
4.  **"LOD와 연동되는가?"**: Mass의 LOD 시스템과 결합하여, 먼 거리에 있는 엔티티는 신호 기반으로 동작하게 하고 가까운 엔티티는 강제 Tick하게 설계하는 것이 대규모 최적화의 정석입니다.
