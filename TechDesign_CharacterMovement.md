# 기술 설계: 캐릭터 이동 시스템 (Character Movement System)

이 문서는 UE 5.7의 실험적 기능인 **Mover** 플러그인을 활용하여 구현된 LootNPop(LNP)의 캐릭터 이동 시스템 기술 사양을 설명합니다.

## 1. 핵심 아키텍처
이동 시스템은 복잡한 중력 환경 지원과 강력한 네트워크 동기화를 위해 각 기능이 분리된 구조로 설계되었습니다.

### 1.1 주요 구성 요소
- **LNPCharacterBase**: `IMoverInputProducerInterface`를 구현하는 기본 Pawn 클래스입니다. 입력 수집 및 상위 수준의 이동 로직을 담당합니다.
- **LNPPawnGravityComponent**: 구체 월드(Sphere World)와 같은 커스텀 중력원에 맞춰 Pawn의 방향(Orientation)을 설정하고 카메라를 안정화하는 전용 컴포넌트입니다.
- **CharacterMoverComponent**: Mover 플러그인에서 제공하는 핵심 시뮬레이션 엔진으로, 이동 모드 실행 및 네트워크 상태 동기화를 담당합니다.

---

## 2. 중력 및 방향 제어 (Gravity & Orientation)
구체 월드 및 동적 중력 환경을 지원하기 위해 방향 제어 로직을 표준 이동 방식과 분리했습니다.

- **중력 타입 지원**: `Fixed`(표준), `RadialInward`(행성형), `RadialOutward`(반전 구체 월드) 모드를 지원합니다.
- **방향 안정화**: `LNPPawnGravityComponent`는 캐릭터의 Up 벡터를 항상 로컬 중력 법선 방향과 일치시킵니다.
- **곡률 보상 (Curvature Compensation)**: 플레이어가 곡면을 이동할 때, 컨트롤러의 회전 값(`ControlRotation`)을 자동으로 보정하여 화면이 기울어지거나 방향 감각을 잃는 것을 방지합니다.

---

## 3. 이동 속도 및 입력 처리
이동 의도(Intent)는 `OnProduceInput`에서 계산되어 Mover 시뮬레이션으로 전달됩니다.

- **WalkSpeed** (기본 400): 표준 탐험 속도.
- **RunSpeed** (기본 800): 대시 버튼을 유지하며 달릴 때의 속도.
- **DashSpeed** (기본 1600): 대시 초기 폭발적 추진 시 도달하는 최고 속도.

---

## 4. 대시 및 회피 시스템 (네트워크 동기화)
대시 시스템은 물리적 반응성과 시각적 동기화를 완벽하게 일치시키기 위해 **하이브리드 레이어드 무브(Hybrid Layered Move)** 방식을 사용합니다.

### 4.1 하이브리드 레이어드 무브 전략
단일 무브 타입에 의존하지 않고, 두 가지 `FLayeredMove`를 동시에 실행합니다.

1.  **물리 이동 (`FLayeredMove_LinearVelocity`)**:
    *   **목적**: 권한 있는(Authoritative) 위치 및 속도 시뮬레이션.
    *   **믹스 모드**: `OverrideVelocity`를 사용하여 즉각적이고 예측 가능한 추진력을 제공합니다.
    *   **관성**: 종료 시 `MaintainLastRootMotionVelocity` 설정을 통해 걷기 상태로 부드럽게 전환됩니다.

2.  **시각적 동기화 (`FLayeredMove_AnimRootMotion`)**:
    *   **목적**: 애니메이션 몽타주의 네트워크 동기화.
    *   **메커니즘**: `FLayeredMove_MontageStateProvider`를 구현하여, 시뮬레이티드 프록시가 정확히 동기화된 프레임에서 올바른 몽타주를 재생할 수 있게 합니다.
    *   **분리의 장점**: 애니메이션의 실제 루트 모션 추출을 비활성화함으로써, 애니메이션 에셋 수정 없이 코드만으로 대시 거리와 속도를 자유롭게 조절할 수 있습니다.

### 4.2 방향성 로직
- **방향성 대시**: 이동 입력(WASD)에 따라 상대적 방향을 계산하고 그에 맞는 몽타주(`Forward`, `Backward`, `Left`, `Right`)를 재생합니다.
- **회피 (뒤로 물러나기)**: 이동 입력 없이 대시 버튼을 누르면 캐릭터의 후방 방향으로 추진력을 얻으며 `Backward` 몽타주를 재생합니다.

### 4.3 제한 사항 및 쿨타임
- **쿨타임**: `DashCooldown` (기본 1.0초)을 통해 무분별한 사용을 방지합니다.
- **상태 제한**: 조준 중(`bIsAiming`)이거나 캐릭터가 공중에 떠 있는 경우에는 대시가 발동되지 않습니다.

---

## 5. 네트워크 동기화 흐름
1.  **자율 프록시 (Autonomous Proxy)**: 로컬에서 대시를 즉시 시작하며, 두 개의 `FLayeredMove` 구조체를 큐에 넣습니다.
2.  **서버 (Server)**: 무브를 검증하고 `SyncState`를 업데이트합니다.
3.  **시뮬레이티드 프록시 (Simulated Proxy)**:
    *   `FLayeredMove_LinearVelocity` 수신 -> 월드 위치 업데이트.
    *   `FLayeredMove_AnimRootMotion` 수신 -> `CharacterMoverComponent` 내부의 동기화 로직에 의해 자동으로 `Montage_Play` 실행.

---

## 6. 질주 시스템 (Sprint System)
LNP의 질주 시스템은 단순한 속도 변경을 넘어, Mover 2.0의 상태 관리 철학을 엄격히 따릅니다.

- **설계 참조**: 기본 Mover 플러그인의 **Crouch(Stance) 로직을 벤치마킹**하여 구현되었습니다. 의도(Intent)와 실제 수정자(Modifier)를 분리하여 관리하는 엔진의 표준 패턴을 사용합니다.
- **상태 머신 구조**:
    *   `bWantsToRun`: 플레이어의 입력을 통해 설정되는 "의도" 플래그입니다. `SetWantsToRun()`을 통해 제어됩니다.
    *   `SprintModifierHandle`: 현재 활성화된 `LNPSprintModifier`를 추적하는 핸들입니다. 엔진의 `StanceModifierHandle`과 동일한 방식으로 롤백 시 안정성을 보장합니다.
    *   `OnMoverPreSimulationTick`: 매 시뮬레이션 프레임마다 의도와 조건을 검사하여 수정자를 큐에 넣거나 취소합니다.
- **게임플레이 태그 및 외부 연동**:
    *   **전용 태그**: `LNP.Mover.IsSprinting` 태그를 사용하여 현재 질주 상태를 시스템 전반에 노출합니다.
    *   **태그 전파**: `LNPSprintModifier`가 활성화되면 `HasGameplayTag`를 통해 해당 태그가 액터에 부여됩니다. 이는 애니메이션 블루프린트(ABP)에서 복잡한 로직 없이 태그 체크만으로 달리기 애니메이션을 재생할 수 있게 합니다.
    *   **래퍼 함수**: `IsSprinting()` 함수를 제공하여 블루프린트나 C++ 코드에서 직관적으로 상태를 쿼리할 수 있습니다.
- **네트워크 동기화**: 수정자 기반 설계를 통해 네트워크 롤백(Rollback) 발생 시에도 스프린트 상태가 `SyncState`에 포함되어 정확하게 복구됩니다.
- **확장성**: `CanSprint()` 가상 함수를 통해 지면 상태, 스테미너, 혹은 특정 상태(예: 조준 중)에 따른 질주 제한 로직을 중앙 집중식으로 관리합니다.


