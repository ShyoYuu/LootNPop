# 캐릭터 이동 시스템 기술 설계

## 1. 한눈에 보기

UE 5.8 **Mover 2.0** 플러그인 기반. 구형 중력 환경(위치마다 Up이 다른 Dyson Sphere 내벽)과 멀티플레이어 예측·롤백을 동시에 지원하기 위해 역할을 컴포넌트 단위로 분리했다.

```
[입력]  ULNPInputHandlerComponent (IMoverInputProducerInterface)
           │  Enhanced Input 캐싱 → ProduceInput에서 InputCmd 구성
           │  (FCharacterDefaultInputs + FLNPModifierInputs)
           ▼
[시뮬레이션]  ULNPCharacterMoverComponent (UCharacterMoverComponent 확장)
           │  PreSimulationTick에서 Guard → Sprint Modifier 적용
           │  Dash/넉백/Launch는 LayeredMove·InstantEffect로 큐잉
           ▼
[중력]  ULNPPawnGravityComponent — Down/Up 방향 계산, Mover에 Gravity·UpDirection Override
[시점]  ULNPControlRotationComponent — 곡률 보정 + 시선 입력 + 락온 보정을 합산해
           프레임당 SetControlRotation 1회 호출
```

### 컴포넌트 역할 분담

| 컴포넌트 | 역할 |
|:---|:---|
| `ALNPCharacterBase` | 컴포넌트 조합 및 AI 이동 의도 위임 (`SetAIMoveInput`, `SetAIOrientationIntent`) |
| `ULNPInputHandlerComponent` | 입력 수집·버퍼링, `IMoverInputProducerInterface` 구현 (InputCmd 생산) |
| `ULNPCharacterMoverComponent` | 이동 시뮬레이션, Sprint/Guard Modifier 관리, 대시·넉백·Launch 실행 |
| `ULNPPawnGravityComponent` | 구형 중력 방향 결정, Mover Gravity/UpDirection Override |
| `ULNPControlRotationComponent` | 컨트롤 회전 전담 — 곡률 보정, 시선 입력, 락온 소프트 보정·하드 클램프 |

---

## 2. 중력 및 방향 제어

### 2.1 중력 타입 (`ELNPGravityType`)

| 모드 | 적용 환경 | Down 방향 |
|:---|:---|:---|
| `Fixed` | 표준 지형 | `FixedGravityDirection` |
| `RadialInward` | 행성 외부 표면 | 구 중심 방향 |
| `RadialOutward` | Dyson Sphere 내벽 | 구 중심 반대 방향 (원심) |

**LootNPop 메인 월드 설정:** `RadialOutward`, `GravityOrigin = (0, 0, 0)`
- BeginPlay에서 `ALNPGameState::bIsSphereWorld` 플래그를 읽어 자동 설정.
- 매 Tick Owner 위치 기준 Down/Up을 재계산하고, 방향이 바뀌면 `SetGravityOverride` + `SetUpDirectionOverride`로 Mover에 전달. 캡슐 회전은 Mover가 UpDirection 기반으로 부드럽게 처리 (직접 `SetActorRotation` 호출 없음).

### 2.2 곡률 보정 (Curvature Compensation) — `ULNPControlRotationComponent`

구면 위를 이동하면 지역 Up 벡터가 연속적으로 변한다. 보정하지 않으면 화면이 점점 기울어진다.

```cpp
// 이전 프레임 Up → 현재 Up 사이의 회전 차이를 컨트롤 회전에 누적
const FQuat CurvatureDelta = FQuat::FindBetweenNormals(LastUpDir, TargetUpDir);
CurrentControlQuat = CurvatureDelta * CurrentControlQuat;
```

### 2.3 컨트롤 회전 파이프라인

`UpdateControllerOrientation()`이 매 프레임 아래 순서로 처리하고 `SetControlRotation`을 **한 번만** 호출한다.

```
1. 곡률 보정 (Up 벡터 변화량 누적)
2. 시선 입력 적용 — Yaw: 로컬 Up 축, Pitch: 로컬 Right 축 회전
3. 락온 소프트 보정 (LockOnComponent가 적립한 Yaw/Pitch 델타)
4. Pitch 클램프 (Up과의 각도 약 ±85° — 짐벌락 방지)
5. 락온 하드 클램프 (타겟 방향과 최대 이탈각 초과 시 Slerp로 강제 보정)
6. Roll-free 회전 재구성 → SetControlRotation
   (카메라 Roll 잔여 보정은 LNPGravityRollCorrectionCameraNode 담당)
```

입력 소스(InputHandler·LockOn)는 델타를 **적립**만 하고, 소비·적용은 이 컴포넌트가 전담 — 프레임당 SetControlRotation 다중 호출로 인한 순서 충돌을 구조적으로 차단한다.

---

## 3. 이동 속도 설정

**`ULNPCharacterMovementSettings`** (Mover Shared Settings — `ULNPAsyncWalkingMode`가 등록)

| 항목 | 기본값 | 설명 |
|:---|:---:|:---|
| `SprintSpeed` | 1200 cm/s | 질주 시 최대 이동 속도 |
| `SprintAcceleration` | 6000 cm/s² | 질주 시 가속도 |
| `GuardWalkSpeed` | 200 cm/s | 가드 중 최대 이동 속도 |
| `GuardAcceleration` | 2000 cm/s² | 가드 중 가속도 |

> 기본 걷기 속도(`MaxSpeed`)는 Mover 기본 `UCommonLegacyMovementSettings`에서 관리.

**`ULNPCharacterMoverComponent`** (대시 속성)

| 항목 | 기본값 | 설명 |
|:---|:---:|:---|
| `DashDuration` | 0.2초 | 대시 지속 시간 |
| `DashCooldown` | 1.0초 | 쿨타임 |
| `DashImpulseMagnitude` | 2000 cm/s | 대시 충격량(속도) |

---

## 4. 질주 / 가드 — Modifier 패턴

Mover의 Crouch(Stance) 패턴을 벤치마킹하여 **의도(입력) → Modifier(적용) → Tag(조회)** 3단으로 분리. Guard가 Sprint보다 먼저 처리되며, `CanSprint()`가 `!IsGuarding()`을 포함해 가드 중 질주를 차단한다.

```
FLNPModifierInputs { bWantsToSprint, bWantsToGuard }   ← InputHandler가 InputCmd에 기록
    │  OnMoverPreSimulationTick()에서 InputCmd로부터 읽음
    ▼
FLNPSprintModifier / FLNPGuardModifier (속도 수정자)
    │  OnStart: MaxSpeed·Acceleration을 LNP Settings 값으로 교체
    │  OnEnd:   CDO 원본 값으로 복원
    ▼
LNP.Mover.IsSprinting / LNP.Mover.IsGuarding (Gameplay Tag)
    └→ IsSprinting()/IsGuarding() 쿼리, ABP 전환 기준
```

- Modifier는 Mover `SyncState`에 포함되어 네트워크 롤백 시 안정적으로 복구된다.
- 의도 플래그가 **InputCmd(FLNPModifierInputs)로 전달되는 이유**는 §7.1 참조 — 이 시스템의 핵심 설계 포인트.

### 4.1 MoveSpeed 버프 — `FLNPMoveSpeedModifier` ✅ 구현·PIE 검증 완료 (2026-07-27)

GAS `MoveSpeed` 어트리뷰트(버프 합산 후 최종값)를 이동 속도에 반영하는 **상시 활성** Modifier.
`OnMoverPreSimulationTick` 진입부에서 부재 시 1회 큐잉되며, 이후 매 틱 `OnPreMovement`가 실행된다.

```
BaseSpeed = CDO.MaxSpeed                    (기본)
          | CDO.SprintSpeed     (IsSprinting)
          | CDO.GuardWalkSpeed  (IsGuarding)
CommonSettings->MaxSpeed = BaseSpeed × MoveSpeed
```

**왜 매 틱 CDO에서 재계산하는가 (설계의 핵심):**
Sprint/Guard Modifier는 라이브 설정에 값을 쓰고 종료 시 **CDO 원본으로 되돌린다**(`FLNPSprintModifier::OnEnd`).
버프를 적용 시점에 한 번만 써 두면 **첫 질주가 끝나는 순간 CDO 값으로 복원되어 버프가 영구히 사라진다.**
`SprintSpeed`/`GuardWalkSpeed`까지 같이 덮어써도 진입만 해결될 뿐 복귀 경로에서 같은 문제가 남는다.
매 틱 CDO 기준으로 다시 계산하면 ① 배율이 누적되지 않고 ② Sprint/Guard 실행 순서와 무관하며
③ 그들이 복원해도 다음 틱에 다시 덮어쓴다. 그래서 **Sprint/Guard 코드는 수정하지 않았다.**

- `MaxSpeed`만 담당한다. `Acceleration`은 Sprint/Guard가 계속 소유한다.
- ⚠️ Sprint/Guard의 `MaxSpeed` 대입은 이제 매 틱 덮어써져 **사실상 무효**다 (동작엔 무해). 정리 여부는 미결.
- ⚠️ 배율은 Mover 예측 상태가 아니라 **ASC 어트리뷰트에서 직접** 읽는다. 버프 적용·만료 순간 서버/클라
  적용 틱이 어긋나 짧은 보정이 생길 수 있다(30초 버프당 2회). 완전 예측이 필요해지면 배율을
  Modifier의 `NetSerialize` 페이로드로 옮긴다.
- ⚠️ 질주 진입 첫 1프레임은 `OnStart`가 `OnPreMovement` 뒤에 오는 틱이라 버프 미적용 속도가 나올 수 있다.

**PIE 검증 결과 (2026-07-27, 1인):** 일반 이동·질주 중·**질주 종료 후** 세 구간 모두 버프 적용 속도가 유지된다
— CDO 복원으로 버프가 소실되는 문제가 실제로 발생하지 않음을 확인(이 설계의 핵심 검증점).
`LNP.Debug.ShowSpeed`(아래 §4.2) 실측 `avg`가 `MaxSpeed`의 약 97.5%에 수렴한다
(800 → 780, 1200 → 1170). 고정 오차가 아닌 **비례 오차**라 입력 크기·가속 점근 특성으로 설명되며 정상 범위.
**2인 PIE 검증 완료(2026-07-27):** 서버(1P)·클라이언트(2P) 양쪽에서 정상 동작.
모디파이어를 서버/클라 각자의 첫 틱에 큐잉하는 방식(Sprint/Guard의 `InputCmd` 기반과 다름)이
실사용상 문제를 일으키지 않음을 확인. 보정 튐 정량 측정은 하지 않았으므로, 이동이 미세하게
어색해지는 리포트가 나오면 `mover.debug.ShowCorrections 1`부터 확인할 것.

### 4.2 디버그 — `LNP.Debug.ShowSpeed [0|1]`

로컬 폰의 이동 속도를 화면에 2줄로 표시한다 (`LNPCharacterMoverComponent.cpp` 하단 익명 네임스페이스).

```
[Measured] now   612.3   avg   598.7   peak   780.0  cm/s          (초록)
[Settings] MaxSpeed   780.0  cm/s   state Sprint   MoveSpeed x1.30   (청록)
```

- **실측 줄**은 `FTSTicker`(코어 티커)에서 **액터 월드 위치의 프레임 간 변화량**만으로 구한다.
  `GetVelocity()`도 설정값도 읽지 않아 이동 로직을 **바깥에서 교차 검증**할 수 있다.
- **설정 줄**은 대조용 — `state`와 배율을 함께 찍어 `기준값 × MoveSpeed = MaxSpeed` 관계를 즉시 확인한다.
- 엔진 내장과 병행하면 좋다: `mover.debug.ShowCorrections`(서버 보정 시각화), `ShowTrail`, `ShowTrajectory`,
  GameplayDebugger Mover 카테고리(`'` 키 — 단 여기 `Speed`는 Mover 내부 `GetVelocity()` 값이다).

---

## 5. 대시 시스템

반응성과 시각 동기화를 동시에 달성하는 **하이브리드 레이어드 무브** 방식.

### 5.1 두 레이어드 무브의 역할

**물리 이동 (`FLayeredMove_LinearVelocity`)**
- 서버 권위적 위치/속도 시뮬레이션. `OverrideVelocity` 믹스 모드로 즉각적인 추진력.
- 종료 시 `MaintainLastRootMotionVelocity`로 걷기 상태로 부드럽게 전환.

**시각 동기화 (`FLayeredMove_AnimRootMotion`)**
- 몽타주의 네트워크 동기화. 시뮬레이티드 프록시가 동일 프레임에서 몽타주 재생.
- 실제 루트 모션 추출은 비활성 → 애니메이션 에셋 수정 없이 코드만으로 대시 거리·속도 제어.

### 5.2 방향성 대시 — Chooser 연동

물리 방향과 몽타주 방향 태그를 분리해서 결정한다.

| 구분 | 규칙 |
|:---|:---|
| 물리 방향 | 이동 입력 있음 → 컨트롤 회전 기준 입력 방향 / 없음 → 캐릭터 후방 (회피) |
| 몽타주 태그 (일반) | 캐릭터가 이동 방향을 바라보므로 Front/Back 2방향 |
| 몽타주 태그 (Strafe: FreeAim·LockOn) | 시선 고정 상태이므로 입력 각도로 Front/Right/Left/Back 4방향 분류 |

결정된 태그(`LNP.Montage.Value.Direction.*`)로 `EvaluateMontage(TAG_Montage_Situation_Dash, DirTag)`를 호출해 **Chooser Table에서 몽타주를 선택**한다. (하드코딩된 몽타주 프로퍼티 방식에서 마이그레이션 완료)

### 5.3 제한 조건

- 조준 중(`bIsAiming`) 또는 공중 상태에서 발동 불가.
- `DashCooldown` 시간 내 재발동 불가.

### 5.4 네트워크 흐름

1. **Autonomous Proxy:** 즉시 두 FLayeredMove를 큐에 삽입 (체감 지연 0)
2. **Server:** 무브 검증 후 `SyncState` 갱신
3. **Simulated Proxy:** `LinearVelocity` 수신 → 위치 갱신, `AnimRootMotion` 수신 → 자동 몽타주 재생

---

## 6. 넉백 / Launch

| API | 구현 | 용도 |
|:---|:---|:---|
| `ApplyKnockback()` | `FApplyVelocityEffect` (Instant Effect, 가산 속도) | 피격 넉백 |
| `LaunchWithVelocity()` | `FLayeredMove_Launch` (OverrideVelocity) | 패링 성공 시 공격자 발사 등 |

두 API 모두 **Air 모드로 강제 전환**해서 적용한다 — Ground 모드는 매 틱 속도를 MaxWalkSpeed로 클램프하고 위치를 지면에 스냅하므로 임펄스가 무력화되기 때문 (§7.2).

---

## 7. 어필 포인트 (트러블슈팅 & 엔진 분석)

### 7.1 "원격 클라이언트에서만 가드 속도가 안 걸린다" — Mover 입력 파이프라인 분석

Guard/Sprint 의도를 컴포넌트 멤버 변수(bool)로 두고 PreSimulationTick에서 읽는 초기 구현은 로컬에서는 완벽했지만 **원격 클라이언트(서버 기준 프록시)에서 간헐적으로 무시**됐다. 엔진 소스 분석 결과, Mover의 시뮬레이션은 `InputCmd → SyncState` 파이프라인으로 예측·복제·리시뮬레이션되는데, 평범한 컴포넌트 멤버는 이 파이프라인 바깥에 있어 리시뮬레이션 시점의 값과 어긋날 수 있었다.

**해결:** 엔진의 Jump 구현(`FCharacterDefaultInputs::bIsJumpJustPressed`)과 동일하게, `FMoverDataStructBase`를 상속한 `FLNPModifierInputs`를 정의해 InputCmd 컬렉션에 실었다. `NetSerialize`(1비트 직렬화), `ShouldReconcile`, `Interpolate`를 구현해 예측 파이프라인에 완전히 편입.

**부가 발견:** `FMoverDataStructBase::Interpolate`의 기본 구현은 `check(false)` — NetworkPrediction의 Smoothing 서비스가 보간을 시도하는 순간 크래시한다. PIE에서는 재현되지 않고 **standalone `-game` 모드에서만 발현**되는 크래시라서, 커스텀 InputCmd 데이터를 추가할 때 반드시 오버라이드해야 한다.

### 7.2 Ground 모드의 속도 클램프 vs 넉백 임펄스

넉백을 Ground 모드 상태에서 적용하면 매 틱 `MaxWalkSpeed` 클램프 + 지면 스냅 때문에 "밀려나는 느낌"이 사라진다. `UCommonLegacyMovementSettings::AirMovementModeName`으로 강제 모드 전환을 걸어 임펄스가 온전히 적용되도록 처리. 구형 중력 환경에서는 이후 낙하가 자연스럽게 곡면 착지로 이어진다.

### 7.3 SetControlRotation 단일 진입점 설계

곡률 보정·시선 입력·락온 보정이 각자 SetControlRotation을 호출하면 실행 순서에 따라 서로를 덮어쓴다. 모든 회전 소스가 델타를 **적립**하고 `ULNPControlRotationComponent`가 한 프레임에 한 번 합산 적용하는 구조로 재설계 — Tick 순서 의존성을 명시적 파이프라인(§2.3)으로 대체했다.

### 7.4 쿼터니언 기반 구면 시점 제어

구면 환경에서는 FRotator(오일러) 기반 시점 제어가 짐벌락과 Roll 누적으로 파탄난다. 지역 Up 축 기준 Yaw 회전, 지역 Right 축 기준 Pitch 회전을 쿼터니언으로 합성하고, 최종적으로 Up·Forward 외적으로 Roll-free 좌표계를 재구성해 `FMatrix → Rotator` 변환으로 마무리한다.

---

## 8. 미구현 / 한계

- **CanGuard() 상시 허용:** 현재 조건 없이 항상 true. 상태 이상(스태거 등) 연동 시 조건 추가 필요.
- **공중 대시 미지원:** 의도된 제한이지만, 공중 회피 등 확장 여지는 열려 있음.
- **Left/Right 대시 몽타주 에셋:** 4방향 태그 분류·Chooser 평가는 구현 완료 — 좌/우 전용 몽타주 에셋을 Chooser Table에 채우는 에디터 작업 잔여.
