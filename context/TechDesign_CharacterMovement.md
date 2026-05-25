# 캐릭터 이동 시스템 기술 설계

## 1. 핵심 아키텍처

UE 5.7의 Mover 플러그인을 기반으로, 복잡한 중력 환경과 멀티플레이어 동기화를 모두 지원하기 위해 역할을 세 컴포넌트로 분리.

| 컴포넌트 | 역할 |
|:---|:---|
| `ALNPCharacterBase` | 입력 수집 및 이동 의도(Intent) 생성. `IMoverInputProducerInterface` 구현 |
| `ULNPPawnGravityComponent` | 구형 중력 방향 결정, 컨트롤러 회전 보정, 카메라 안정화 |
| `ULNPCharacterMoverComponent` | 이동 시뮬레이션 실행 및 네트워크 상태 동기화 |

---

## 2. 중력 및 방향 제어 (구현 완료)

### 2.1 중력 타입

`ELNPGravityType` 열거형으로 세 가지 모드를 지원.

| 모드 | 적용 환경 | Down 방향 |
|:---|:---|:---|
| `Fixed` | 표준 지형 | `FixedGravityDirection` |
| `RadialInward` | 행성 외부 표면 | 구 중심 방향 |
| `RadialOutward` | Dyson Sphere 내벽 | 구 중심 반대 방향 (원심) |

**LootNPop 메인 월드 설정:** `RadialOutward`, `GravityOrigin = (0, 0, 0)`
- Down = `(ActorLocation - Origin).GetSafeNormal()` (바깥쪽)
- Up = `-Down` = `(Origin - ActorLocation).GetSafeNormal()` (안쪽, 구 중심 방향)

### 2.2 곡률 보정 (Curvature Compensation)

구면 위를 이동하면 지역 Up 벡터가 연속적으로 변함. `UpdateControllerOrientation()`이 이를 보정.

```cpp
// 이전 프레임 Up → 현재 Up 사이의 회전 차이를 컨트롤 회전에 누적
const FQuat CurvatureDelta = FQuat::FindBetweenNormals(LastUpDir, TargetUpDir);
CurrentControlQuat = CurvatureDelta * CurrentControlQuat;
```

이를 통해 플레이어가 구면을 이동해도 화면이 기울어지지 않음.

### 2.3 룩 입력 처리

`InputLook(FRotator Delta)`를 통해 입력을 누적한 뒤 `UpdateControllerOrientation()`에서 일괄 적용.
- Yaw: 로컬 Up 축 기준 회전
- Pitch: 로컬 Right 축 기준 회전, 약 85도 클램프 (짐벌락 방지)

---

## 3. 이동 속도 설정 (구현 완료)

`ALNPCharacterBase`에 `UPROPERTY`로 선언된 값들이며 에디터에서 조정 가능.

| 항목 | 기본값 | 설명 |
|:---|:---:|:---|
| `WalkSpeed` | 400 | 기본 이동 속도 |
| `RunSpeed` | 800 | 질주 시 이동 속도 |
| `DashSpeed` | 1600 | 대시 초기 최대 속도 |
| `DashDuration` | 0.2초 | 대시 지속 시간 |
| `DashCooldown` | 1.0초 | 쿨타임 |
| `DashImpulseMagnitude` | 2000 | 대시 충격량 |

---

## 4. 질주 시스템 (구현 완료)

Mover 플러그인의 Crouch(Stance) 패턴을 벤치마킹하여 의도(Intent)와 수정자(Modifier)를 분리 관리.

```
bWantsToRun (의도 플래그)
    │  OnMoverPreSimulationTick()에서 매 프레임 검사
    ▼
LNPSprintModifier (속도 수정자)
    │  활성 시 SprintModifierHandle로 추적
    ▼
LNP.Mover.IsSprinting (Gameplay Tag)
    └→ 애니메이션 블루프린트에서 달리기 전환 기준
```

- `IsSprinting()` / `CanSprint()` — 외부 조회용 래퍼 함수
- 수정자 기반 설계로 네트워크 롤백 시 `SyncState`에 포함되어 안정적으로 복구됨

---

## 5. 대시 시스템 (설계 완료, 구현 예정)

반응성과 시각적 동기화를 동시에 달성하기 위해 **하이브리드 레이어드 무브** 방식 사용.

### 5.1 두 레이어드 무브의 역할

**물리 이동 (`FLayeredMove_LinearVelocity`)**
- 서버 권위적 위치/속도 시뮬레이션
- `OverrideVelocity` 믹스 모드로 즉각적인 추진력 제공
- 종료 시 `MaintainLastRootMotionVelocity`로 걷기 상태 부드럽게 전환

**시각 동기화 (`FLayeredMove_AnimRootMotion`)**
- 몽타주의 네트워크 동기화
- `FLayeredMove_MontageStateProvider` 구현으로 시뮬레이티드 프록시가 동일 프레임에서 몽타주 재생
- 실제 루트 모션 추출은 비활성화 → 애니메이션 에셋 수정 없이 코드만으로 대시 거리·속도 제어 가능

### 5.2 방향성 대시

이동 입력(WASD)에 따라 방향을 결정하고 방향별 몽타주를 재생.

| 조건 | 방향 | 몽타주 |
|:---|:---|:---|
| 이동 입력 있음 | 입력 방향 | `DashForward/Backward/Left/Right Montage` |
| 이동 입력 없음 | 캐릭터 후방 | `DashBackwardMontage` (회피) |

### 5.3 제한 조건

- 조준 중(`bIsAiming`) 또는 공중 상태에서 발동 불가
- `DashCooldown` 시간 내 재발동 불가

### 5.4 네트워크 흐름

1. **Autonomous Proxy:** 즉시 두 FLayeredMove를 큐에 삽입
2. **Server:** 무브 검증 후 `SyncState` 갱신
3. **Simulated Proxy:** `LinearVelocity` 수신 → 위치 업데이트, `AnimRootMotion` 수신 → 자동 몽타주 재생

---

## 6. 구현 현황 요약

| 기능 | 상태 |
|:---|:---|
| 중력 타입 (Fixed / RadialInward / RadialOutward) | ✅ 완료 |
| 곡률 보정 (Curvature Compensation) | ✅ 완료 |
| 룩 입력 / Pitch 클램프 | ✅ 완료 |
| 이동 속도 설정 | ✅ 완료 |
| 질주 시스템 (Stance + Modifier) | ✅ 완료 |
| 대시 시스템 (Layered Move) | 🔲 설계 완료, 구현 예정 |
| 방향성 대시 몽타주 | 🔲 설계 완료, 구현 예정 |
