# 엔진 분석: UE 5.7 Mover 플러그인 아키텍처

## 개요
Mover 플러그인은 모듈식이며 네트워크 동기화가 보장되는 이동 시스템을 도입했습니다. 이 시스템의 확장성은 크게 **이동 모드(Movement Modes)**, **이동 모디파이어(Movement Modifiers)**, 그리고 **레이어드 무브(Layered Moves)**라는 세 가지 기둥을 기반으로 설계되었습니다.

---

## 1. 이동 모드 (Movement Modes - `UBaseMovementMode`)
**"근본적인 물리 법칙 (The Laws of Physics)"**

이동 모드는 입력을 속도로 변환하는 핵심 수학적 공식을 정의합니다. 이는 캐릭터가 현재 처한 환경에 따라 결정되는 가장 기본적이고 상시적인 물리 상태입니다.

- **배타적(Exclusivity)**: 한 번에 오직 하나의 모드만 활성화됩니다. (예: `Walking` vs `Swimming`)
- **수학적 기반**: 마찰력, 중력, 충돌 반응 로직(`GenerateMove`)을 직접 소유합니다.
- **예시**: 걷기, 비행, 낙하, 수영, 집라인.

---

## 2. 이동 모디파이어 (Movement Modifiers - `FMovementModifierBase`)
**"상태 주도적 파라미터 변형 (Parameter Transformation)"**

이동 모디파이어는 활성화된 이동 모드의 설정값(Settings)을 수정하는 가볍고 중첩 가능한 구조체입니다. 물리 법칙 자체는 바꾸지 않되, 그 안에서 캐릭터가 어떻게 행동하는지(상태)를 정의합니다.

- **중첩 가능(Stackable)**: 여러 개가 동시에 존재할 수 있습니다. (예: `질주` + `앉기` + `슬로우 디버프`)
- **데이터 중심**: `MaxSpeed`나 `Acceleration` 같은 공유 설정값을 일시적으로 교체합니다.
- **네트워크 효율성**: `SyncState`에 핸들로 기록되어 롤백 시 매우 안전합니다.
- **예시**: 질주(Sprint), 앉기(Crouch), 조준(ADS) 시 속도 저하, 각종 버프/디버프.

---

## 3. 인스턴트 이동 효과 (Instant Movement Effects - `FInstantMovementEffect`)
**"시간을 소비하지 않는 즉각적인 상태 변화 (Instantaneous Change)"**

인스턴트 이동 효과는 특정 시뮬레이션 틱에서 즉시 실행되고 사라지는 1회성 명령입니다. 지속 시간(Duration) 개념이 없으며, 액터의 위치, 속도, 혹은 이동 모드를 찰나의 순간에 강제로 변경할 때 사용합니다.

- **즉각성(Atemporality)**: 시간을 소모하지 않고 즉시 반영됩니다.
- **창(Window) 기반 실행**: 시뮬레이션 틱의 시작, 끝, 혹은 서브스텝 사이의 안전한 시점에 실행되어 네트워크 롤백 시 정밀한 복구를 보장합니다.
- **강제 전환**: 현재 진행 중인 물리 연산을 무시하고 상태를 "덮어쓰는" 데 탁월합니다.
- **예시**: 점프(Jump Impulse), 순간이동(Teleport), 런치(Launch), 특정 이동 모드로의 강제 전환.

---

## 4. 레이어드 무브 (Layered Moves - `FLayeredMoveBase`)
**"일시적이고 강력한 이동 영향력 (Temporary Influence)"**

이전 모드나 설정에 관계없이 특정 기간 동안 기존의 이동 로직 위에 추가적인 속도나 위치 변화를 '층(Layer)'처럼 쌓는 기능입니다.

- **일시적(Transient)**: 수명이 존재하며(Duration), 목적을 달성하면 자동으로 제거됩니다.
- **조합/블렌딩(Blending)**: 기존 속도에 더하거나(Add), 무시하고 덮어쓰는(Override) 등 블렌딩 모드를 결정할 수 있습니다.
- **예시**: 대시(Dash), 넉백(Knockback), 애니메이션 루트 모션.

---

## 5. 결정 매트릭스: 무엇을 사용해야 하는가?

새로운 이동 기능을 설계할 때, 다음 표를 기준으로 적절한 아키텍처를 선택하십시오.

| 질문 | 이동 모드 | 모디파이어 | 인스턴트 효과 | 레이어드 무브 |
| :--- | :--- | :--- | :--- | :--- |
| **물리 공식 자체가 바뀌는가?** | **예** | 아니오 | 아니오 | 아니오 |
| **상태 변화가 즉각적인가?** | 아니오 | 아니오 | **예** | 아니오 (Duration) |
| **다른 상태와 중첩 가능한가?** | 아니오 | **예** | 아니오 (즉시 소멸) | **예** |
| **사용 수명(Lifetime)** | 상시 | 의도 유지 시 | 1회성 | 단기 (기간제) |

---

## 6. 시뮬레이션 틱(Simulation Tick) 실행 순서
Mover의 한 프레임(Simulation Step)은 내부적으로 다음 순서로 연산됩니다:

1.  **모드 식별**: 현재 활성화된 `Movement Mode`를 결정합니다.
2.  **인스턴트 효과 적용**: 큐에 쌓인 `Instant Effects`를 실행하여 위치/속도/모드를 즉시 수정합니다.
3.  **모디파이어 적용**: 활성 `Modifiers`들이 공유 설정값을 수정합니다. (예: `MaxSpeed` 증가)
4.  **레이어드 무브 적용**: `Layered Moves`들이 최종 속도에 간섭합니다.
5.  **모드 실행**: 모든 영향력이 반영된 후 `GenerateMove`가 최종 이동량을 계산합니다.
6.  **동기화**: 결과 데이터가 `SyncState`에 기록되어 네트워크로 전송됩니다.


---

## 7. 트리거(Trigger)와 결과 상태(State)의 분리 — Mover 전 영역 공통 원칙

Movement Modes, Movement Modifiers, Layered Moves, Instant Movement Effects 네 기둥 전부에 공통으로 적용되는 규칙이 있습니다: **"지금 이걸 시작할지" 판단(트리거)과 "이미 시작된 것의 상태"(결과)는 동기화되는 방식이 다릅니다.** 이를 혼동하면 특정 기둥에서만 골라 문제가 생기는 게 아니라, 네 기둥 전부에서 동일한 패턴의 버그가 재현됩니다.

### 결과 상태 — 이미 자동으로 동기화됨

`FMoverSyncState`(`MoverSimulationTypes.h`)는 다음을 필드로 갖고, 각각 `NetSerialize`/`ShouldReconcile`을 구현합니다:

- `MovementMode` (FName) — 현재 활성 이동 모드
- `LayeredMoves` / `LayeredMoveInstances` — 레이어드 무브 큐/활성 목록
- `MovementModifiers` — 모디파이어 큐/활성 목록

즉 Movement Mode, Movement Modifiers, Layered Moves 셋은 서버가 만들어낸 "현재 상태"가 서버→클라로 전송되고, 클라의 로컬 예측 결과와 비교(`ShouldReconcile`)해서 어긋나면 자동으로 교정됩니다. 여기에 별도 리플리케이션 코드를 추가할 필요는 없습니다.

Instant Movement Effect는 1회성이라 SyncState에 자기 몫의 필드가 없습니다 — 그 효과의 흔적은 오직 `SyncStateCollection`(Position/Velocity 등 기본 상태)의 변화로만 간접적으로 남습니다.

### 트리거 — 자동으로 동기화되지 않음

리컨실은 어디까지나 "서버가 이미 만들어낸 결과"와 "클라가 예측한 결과"를 비교하는 것입니다. 서버가 애초에 그 Mode 전환 / Modifier 큐잉 / LayeredMove 시작 / Instant Effect 적용을 **시작조차 하지 않았다면, 비교할 결과 자체가 없으므로 리컨실도 일어나지 않습니다.**

이 넷 모두 "시작 여부"를 판단하는 코드(대개 `OnMoverPreSimulationTick` 등 시뮬레이션 콜백)는 네트워크 프리딕션 백엔드(`UMoverNetworkPredictionLiaisonComponent::SimulationTick`)가 버퍼링된 과거 move 단위로 호출합니다. 이때 판단 로직이 참조할 수 있는, 클라→서버로 실제 전달되는 유일한 채널은 `FMoverInputCmdContext`(`ProduceInput → SimulationTick`)뿐이며, 수동 Server RPC를 쓰지 않는 한 그 외의 통로는 없습니다.

### 왜 일반 멤버 변수로는 안 되는가

UE의 액터 리플리케이션은 **서버 → 클라 단방향**입니다. 로컬 클라에서만 세팅한 일반 멤버 변수는 `Replicated` 지정 여부와 무관하게 서버가 그 폰(리모트 클라의 폰)을 시뮬레이트하는 인스턴스로 전달될 방법이 없습니다.

이 때문에 판단 조건(Mode 강제 전환 여부, Modifier 큐잉 여부, LayeredMove 시작 여부, Instant Effect 적용 여부 무엇이든)을 InputCmd가 아닌 멤버 변수로 두면 **리슨서버 호스트(로컬 예측=서버 authoritative가 같은 프로세스) 자신은 항상 정상 작동하고, 리모트 클라는 서버 시뮬레이션에서 그 조건이 계속 다르게(대개 거짓으로) 읽혀 아예 반영되지 않는** 비대칭 버그가 발생합니다.

### 엔진 자체의 대조 사례 (Jump vs Crouch)

- **Jump (`FCharacterDefaultInputs::bIsJumpJustPressed`)**: Instant Movement Effect(`FJumpImpulseEffect`)를 시작하는 트리거이며, InputCmd에 태워 전달되는 네트워크까지 완결된 예제입니다.
- **Crouch (`UCharacterMoverComponent::bWantsToCrouch` + `FStanceModifier`)**: Movement Modifier를 시작하는 트리거인데, `Transient` 일반 멤버 변수를 `OnMoverPreSimulationTick`에서 직접 읽어 판단합니다. InputCmd를 타지 않으며, 엔진 전체를 검색해도 `Crouch()`/`UnCrouch()`를 실제로 호출하는 코드가 어디에도 없습니다 — 게임 쪽에서 입력 배선을 완성해야 하는 미완성 레퍼런스입니다. 그대로 멀티플레이에 사용하면 위에서 설명한 것과 동일한 비대칭 버그가 재현됩니다.

### 올바른 패턴 (네 기둥 공통)

1. 트리거가 되는 입력 의도를 담는 `FMoverDataStructBase` 파생 구조체를 정의한다 (예: `FLNPModifierInputs`의 `bWantsToGuard`, `bWantsToSprint`).
2. `ProduceInput` 단계에서 이 구조체를 `InputCmd.InputCollection`에 실어 보낸다.
3. `OnMoverPreSimulationTick(TimeStep, InputCmd)`에서 `InputCmd.InputCollection.FindDataByType<T>()`로 값을 읽어, Mode 전환이든 `QueueMovementModifier` / `CancelModifierFromHandle`(Modifier)이든 `QueueLayeredMove`(Layered Move)이든 `QueueInstantMovementEffect`(Instant Effect)이든 동일한 방식으로 트리거한다.

LootNPop의 `LNPModifierInputs.h` + `LNPCharacterMoverComponent.cpp`(Guard/Sprint 판단부)가 이 패턴을 따릅니다.

---
*참조: LootNPop 프로젝트 - Mover 시스템 설계 가이드 (2026년 4월)*
