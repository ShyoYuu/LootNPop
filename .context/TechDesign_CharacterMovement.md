# 캐릭터 이동 시스템 기술 설계

## 1. 한눈에 보기

UE 5.8 **Mover 2.0** 플러그인 기반. 구형 중력 환경(위치마다 Up이 다른 Dyson Sphere 내벽)과 멀티플레이어 예측·롤백을 동시에 지원하기 위해 역할을 컴포넌트 단위로 분리했다.

```
[입력]  ULNPInputHandlerComponent (IMoverInputProducerInterface)
           │  Enhanced Input 캐싱 → ProduceInput에서 InputCmd 구성
           │  (FCharacterDefaultInputs + FLNPModifierInputs)
           ▼
[시뮬레이션]  ULNPCharacterMoverComponent (UCharacterMoverComponent 확장)
           │  PreSimulationTick에서 Guard → ADS → Sprint Modifier 적용
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
| `ULNPCharacterMoverComponent` | 이동 시뮬레이션, Sprint/Guard/ADS Modifier 관리, 대시·넉백·Launch 실행 |
| `ULNPPawnGravityComponent` | 구형 중력 방향 결정, Mover Gravity/UpDirection Override |
| `ULNPControlRotationComponent` | 컨트롤 회전 전담 — 곡률 보정, 시선 입력(ADS 감도 배율 포함), 락온 소프트 보정·하드 클램프 |

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
   (배율은 `LookYawSensitivity`·`LookPitchSensitivity`, ADS 중에는 `ADSLookSensitivityScale`을 추가로 곱한다 — §2.6)
3. 락온 소프트 보정 (LockOnComponent가 적립한 Yaw/Pitch 델타)
4. Pitch 클램프 (Up과의 각도 약 ±85° — 짐벌락 방지)
5. 락온 하드 클램프 (타겟 방향과 최대 이탈각 초과 시 Slerp로 강제 보정)
6. Roll-free 회전 재구성 → SetControlRotation
   (카메라 자세 보정은 LNPGravityRollCorrectionCameraNode 담당 — §2.4)
```

입력 소스(InputHandler·LockOn)는 델타를 **적립**만 하고, 소비·적용은 이 컴포넌트가 전담 — 프레임당 SetControlRotation 다중 호출로 인한 순서 충돌을 구조적으로 차단한다.

### 2.4 카메라 리그 — 구면 중력 보정 (Gameplay Cameras)

`ALNPPlayerCharacter`는 `UGameplayCameraComponent`를 `AnimSourceMesh`에 부착하고, `PossessedBy`에서 `ActivateCameraForPlayerController`로 활성화한다. 활성 리그는 `CR_ThirdPerson`.

**⚠️ 노드 순서에 의존하는 구조다.** 아래 순서를 지키지 않으면 적도 부근에서 카메라가 캐릭터 하체로 내려가거나, 이동 시 고무줄 지연이 상하로 새는 증상이 난다.

| # | 노드 | 역할 | 기준 프레임 |
|:--|:---|:---|:---|
| 1 | `Offset` (붐암오프셋) | 피벗 오프셋. `BoomArmOffset` 파라미터 구동 | 컨텍스트(캐릭터 메시) — 이미 중력 정렬 |
| 2 | **`LNPRagdollPivotOffset`** | 랙돌일 때만 피벗을 중력 Up 방향으로 이동 (§9.4). **BoomArm 앞이어야 한다** | 캐릭터 중력 Up |
| 3 | `BoomArm` | Yaw/Pitch 회전 + 피벗 조인트 발행. `BoomOffset = (0,0,0)` | **Roll=0 월드 Z-Up** |
| 4 | **`LNPGravityRollCorrection`** | 피벗 회전을 중력 정렬로 보정 (위치·회전 동시) | — |
| 5 | `DampenPosition` | 고무줄 지연. `DampenSpace = CameraPose` | 4번이 보정한 회전 |
| 6 | `Offset` (카메라오프셋) | 실제 붐 거리. `CameraOffset` 파라미터 구동. `OffsetSpace = CameraPose` | 4번이 보정한 회전 |
| 7 | `FieldOfView` / `PostProcess` | 포즈 미변경 | — |

**근본 원인 — Boom Arm이 Roll을 버린다.** `FBoomArmCameraNodeEvaluator::ComputeBoomRotation()`은 붐 피벗 회전을 `FRotator3d(Pitch, Yaw, 0)`으로 재구성한다. 즉 **항상 월드 Z-Up 평면 기준**이라, 구면 중력에서 중력 Up이 월드 Z와 벌어질수록 이 회전을 프레임으로 삼는 모든 하위 노드가 어긋난다. 어긋나는 각도(= Roll 보정각)는 구면상 위치 **와** 시선 방향에 함께 의존하므로, 같은 지점에서도 바라보는 방향에 따라 0°~180°를 오간다.

**`LNPGravityRollCorrectionCameraNode`의 처리**

```cpp
// BoomArm이 발행한 Yaw/Pitch 조인트에서 피벗을 얻는다
const FQuat PivotRot = PivotJoint->Transform.GetRotation();
// 전방 축은 유지, Up만 중력 기준으로 재정렬
const FQuat CorrectedPivotRot = /* Forward, Up×Forward, Forward×Right 기저 */;
// 피벗 전방 축 기준 순수 Roll 델타를 회전과 위치에 함께 적용
const FQuat RollDelta = CorrectedPivotRot * PivotRot.Inverse();
CameraPose.SetLocation(PivotLoc + RollDelta.RotateVector(CamLoc - PivotLoc));
CameraPose.SetRotation((RollDelta * CamRot).Rotator());
```

"붐 피벗이 처음부터 중력 정렬되어 있었다면 나왔을 결과"와 수학적으로 동일하다.

**배치 제약**

- `BoomArm`보다 **뒤** — 피벗 조인트(`CameraRigJoints`의 YawPitch 조인트)를 읽어야 한다. 앞에 두면 조인트가 없어 노드 전체가 no-op이 된다.
- `DampenPosition`·`Offset`보다 **앞** — 이 노드들이 `CameraPose` 회전을 기준 프레임으로 쓰므로, 그 전에 보정돼 있어야 감쇠 Vertical 축과 카메라 오프셋이 중력 Up 기준이 된다.

현재 구성에서는 `BoomOffset = (0,0,0)`이라 3번 노드의 위치 보정분이 0이 되지만, 코드는 그대로 유지한다 — 붐 오프셋을 쓰거나 노드 순서를 바꿔도 여전히 올바르게 동작한다.

### 2.5 ⚠️ 카메라 노드에서 "내 폰"을 얻는 법 (2026-08-21 실측)

**`Context->GetPlayerController()->GetPawnOrSpectator()`로 폰을 얻으면 안 된다.**
`LNPRagdollPitch` 노드가 아무 효과도 내지 않아 로그를 붙여 확인한 결과, **2인 PIE에서 카메라 컨텍스트가
플레이어당 2개씩 총 4개 만들어지고** 이 방식이 다음처럼 어긋났다:

```
OnInitialize — PC=..._C_0  Pawn=BP_LNPPlayer_C_0   ← 맞음
OnInitialize — PC=..._C_0  Pawn=None               ← 폰이 아직 없음
OnInitialize — PC=..._C_1  Pawn=BP_LNPPlayer_C_1   ← 맞음
OnInitialize — PC=..._C_0  Pawn=BP_LNPPlayer_C_1   ← 다른 플레이어의 폰!
```

컨트롤러↔폰 관계는 평가 컨텍스트가 만들어지는 시점에 확정돼 있지 않다.

**올바른 경로:** 컨텍스트의 Owner가 `UGameplayCameraComponentBase` 자신이므로
(`GameplayCameraComponentBase.cpp` — `Params.Owner = this`), 그 컴포넌트의 소유 액터가 곧
이 카메라가 따라다니는 폰이다. 컨텍스트당 정확하다.

공용 헬퍼 `LNPCamera::ResolveOwningCharacter()` (`Camera/LNPCameraNodeUtils.h`)로 뽑아 두 노드가 함께 쓴다.

```cpp
UObject* ContextOwner = Context ? Context->GetOwner() : nullptr;
if (const UActorComponent* OwnerComponent = Cast<UActorComponent>(ContextOwner))
    return Cast<ALNPCharacterBase>(OwnerComponent->GetOwner());
```

**캐싱하지 않는다.** `OnRun`에서 매 프레임 캐스트 두 번이면 되고, 리스폰으로 폰이 바뀌어도 스스로 따라간다
(`OnInitialize`에서 캐싱하면 리스폰 후 죽은 폰을 가리킨 채로 남는다).

`LNPGravityRollCorrection`도 같은 함정을 갖고 있어 **함께 고쳤다** — 2인 이상에서 남의 중력 Up을 읽거나
캐시가 null이라 롤 보정이 통째로 빠지는 브랜치가 있었다. 겸사겸사 `ULNPPawnGravityComponent` 직접 참조도
`ALNPCharacterBase::GetUpDirection()` 위임으로 정리했다.

### 2.6 ADS (정조준) — 총기류 전용

`TAG_AimMode_FreeAim`(= `ULNPWeaponData::DefaultAimMode`가 부여)일 때만 동작한다.
Guard와 **같은 키를 공유**하며, `ULNPInputHandlerComponent::IsFreeAimMode()`로 정확히 반대 조건으로 갈린다
— 총기면 ADS, 근접이면 Guard.

**상태 원본은 하나다.** 별도 플래그를 두지 않고 기존 `bIsADSPressed`를 그대로 쓴다:

```cpp
bool ULNPInputHandlerComponent::IsADSActive() const
{
    return bIsADSPressed && IsFreeAimMode();
}
```

키를 누른 채 근접 무기로 바꾸면 태그가 바뀌어 자동으로 false가 되므로, 이동·카메라·감도는 스스로 풀린다.

**Guard도 같은 형태여야 한다 — 대칭이 아니면 무기 교체가 상태를 남긴다.**

```cpp
bool IsGuardActive() const { return bIsGuardPressed && !IsFreeAimMode(); }   // IsADSActive()와 정확히 반대
...
ModifierInputs.bWantsToGuard = IsGuardActive();   // 원시 bIsGuardPressed를 그대로 쓰면 안 된다
```

> **⚠️ 파생값만으로는 부족하다 (2026-08-23).** 가드는 ADS와 달리 ASC 루즈 태그(`TAG_State_Guarding`,
> `TAG_State_ParryWindow`), 패링 창 타이머, Mass `FLNPParryStateFragment`를 **입력 순간에 명령형으로**
> 세팅한다. 폴링이 아니므로 무기가 바뀌어도 스스로 풀리지 않는다 — 가드 중 총으로 교체하면
> **총을 든 채 가드·패링이 유지되고**(`LNPProjectileProcessors.cpp:600,665`,
> `LNPWeaponTraceProcessors.cpp:516,533`이 프래그먼트를 그대로 본다), 이동 속도가 `GuardWalkSpeed`에 묶이며,
> `CanADS() = !IsGuarding()` 때문에 ADS까지 막힌다.
>
> 그래서 해제 일체를 `ULNPInputHandlerComponent::ReleaseGuardState()`로 묶고,
> `ALNPCharacterBase::ApplyWeaponVisuals`가 **조준 모드가 실제로 바뀐 경우에만**
> `NotifyAimModeChanged()`를 부른다(라이플→피스톨처럼 모드가 같으면 부르지 않는다 — ADS가 끊기면 안 된다).
> 이때 눌린 상태를 **뗀 것으로 처리**하고 새 입력을 요구한다. 조용히 재개시키면 이동 모디파이어만
> 폴링으로 되살아나고 패링 창은 안 열려 둘이 어긋난다.

**두 계층으로 갈라진다 — 이게 설계의 핵심이다.**

| | 소유 경로 | 이유 |
|:--|:---|:---|
| 카메라·조준 감도 | `ALNPCharacterBase::IsADSActive()` 직접 조회 (로컬) | 카메라는 각자 머신에서만 렌더된다 — 복제·예측 대상이 아니다 |
| 대시·질주 차단, 이동 속도 | `FLNPModifierInputs::bWantsToADS`(InputCmd)로 의도 전달 → `LNP.Mover.IsADS`(SyncState 태그)로 판정 | 시뮬레이션 판정이라 서버·리시뮬레이션이 재현해야 한다 |

로컬 상태를 시뮬레이션 판정에 쓰면 원격 클라이언트에서 어긋난다 (`LNPModifierInputs.h` 헤더 주석 참조).

**카메라 — 리그 프리셋 전환**

`CDE_ThirdPerson`(BlueprintCameraDirector)이 매 프레임 분기해 리그를 고른다:

```
FindEvaluationContextOwnerActor(ALNPCharacterBase)   ← 엔진 헬퍼. §2.5의 함정을 피하는 유일한 경로
 ├ IsADSActive() → CR_ADS
 └ else          → CR_Medium_FreeCam
```

`CR_ADS`는 `CR_ThirdPerson`을 감싸는 얇은 래퍼(`CameraRigCameraNode`)로, 노출된 인터페이스 파라미터
`CameraOffset`(붐 단축 + 우측 오버더숄더) / `FieldOfView`(축소) / `DampenPosition` 감쇠 계수(완화)만
오버라이드한다. **`CR_ThirdPerson`의 노드 순서(§2.4)는 건드리지 않는다** — 프레임 제약이 한 곳에만 살아 있게 유지하려는 것이다.
블렌드는 엔진 블렌드 스택이 처리한다 (`EnterTransitions` 0.15s / `ExitTransitions` 0.22s — 진입은 즉각적으로, 해제는 부드럽게).

> **⚠️ 카메라 컴포넌트를 직접 옮기는 방식은 불가.** `UMoverComponent::FinalizeFrame`이 매 프레임 주 비주얼
> 컴포넌트의 상대 트랜스폼을 복원한다. 사망 카메라(§9.4)가 `DetachFromComponent`부터 하는 이유와 같다.
> ADS 카메라 이동은 반드시 리그 안에서 처리한다.

**조준 감도** — FOV를 좁히면 같은 마우스 이동이 화면상 더 크게 돌아 과민해진다.
`ULNPControlRotationComponent`가 `ADSLookSensitivityScale`(기본 0.65)을 Look 배율에 곱한다.
기준값은 `tan(ADS_FOV/2) / tan(허리사격_FOV/2)`이므로 리그의 FOV를 바꾸면 이 값도 함께 본다.
**락온 보정 델타(§2.3의 3·5단계)에는 곱하지 않는다** — 그건 시스템이 만든 값이지 플레이어 입력이 아니다.

**대시·질주 차단** — 판정은 기존 `CanX()` 인터페이스가 소유한다. InputCmd는 의도만 나르고, "지금 그 행동이 가능한가"는 전부 여기 모인다:

```cpp
bool CanSprint() const { return IsOnGround() && !IsGuarding() && !IsADS(); }
bool CanDash()   const { return IsOnGround() && !IsADS() && FindMovementModifierByType<FLNPDashCooldownModifier>() == nullptr; }
bool CanADS()    const { return !IsGuarding(); }
```

> **역방향은 넣지 않는다.** `CanGuard()`에 `!IsADS()`를 추가하면 둘 다 켜졌을 때 서로를 취소하다 재시작해 진동한다. Guard가 틱에서 먼저 처리되므로 **Guard > ADS 단방향 우선순위**로 고정한다.

`CanSprint()`는 시작 분기와 취소 분기 **양쪽**에서 평가되므로, 질주 도중 ADS에 들어가면 질주가 풀린다. 반면 이미 나간 대시는 `FLayeredMove_LinearVelocity`가 공중에서 진행 중이라 중단하지 않는다 — 끊으면 오히려 튄다.

> **한 프레임 지연이 있다.** `CanX()`가 보는 것은 InputCmd가 아니라 SyncState의 태그이고, 태그는 `QueueMovementModifier` 다음 스텝에 선다. 그래서 ADS를 누른 그 프레임에는 질주·대시가 한 번 통과할 수 있다. Guard→Sprint가 원래 갖고 있던 것과 같은 성질이라 그대로 수용했다(60fps에서 16ms).
> 즉시성이 필요해지면 `bWantsToADS`를 틱에서 직접 보는 방식으로 되돌리면 되지만, 그러면 판정이 `CanX()`와 틱 두 곳으로 흩어진다.

**이동 속도** — `FLNPADSModifier`(`Movement/LNPADSModifier.h/.cpp`). 구조는 `FLNPGuardModifier`와 같다:
`OnStart`에서 `ADSAcceleration`만 적용하고 **MaxSpeed는 건드리지 않는다.** 이 Modifier가 실어 나르는 것은
사실상 `LNP.Mover.IsADS` 태그이고, 실제 속도는 `FLNPMoveSpeedModifier`가 매 틱 CDO 기준으로 계산한다:

```
Sprinting → SprintSpeed
Guarding  → GuardWalkSpeed
IsADS     → ADSWalkSpeed        ← 추가
else      → CommonSettings.MaxSpeed
            × MoveSpeed 어트리뷰트 배율
```

태그가 Mover SyncState에 실리므로 리시뮬레이션에서 함께 롤백된다 (§4.1의 미세 어긋남 문제를 피하는 이유).
튜닝 값은 `ULNPCharacterMovementSettings`의 `ADSWalkSpeed` / `ADSAcceleration`.

**검증 완료 (2026-08-22):** 총기/근접 키 분기, `-game` 모드, 2인 상호 관찰, 적도 부근 ADS,
`ShowSpeed`로 질주 속도 미발현 확인.

**무기 교체 검증 완료 (2026-08-23):** 가드 유지 중 총기로 교체 → 스탠스 즉시 해제,
ADS 유지 중 양손검으로 교체 → ADS 즉시 해제. 양쪽 모두 키를 뗐다 다시 눌러야 반대 행동이 걸린다(의도).
라이플→피스톨처럼 조준 모드가 같은 교체는 ADS가 유지된다.


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

## 4. 질주 / 가드 / ADS — Modifier 패턴

Mover의 Crouch(Stance) 패턴을 벤치마킹하여 **의도(입력) → Modifier(적용) → Tag(조회)** 3단으로 분리. Guard가 Sprint보다 먼저 처리되며, `CanSprint()`가 `!IsGuarding()`을 포함해 가드 중 질주를 차단한다.

```
FLNPModifierInputs { bWantsToSprint, bWantsToGuard, bWantsToADS }   ← InputHandler가 InputCmd에 기록
    │  OnMoverPreSimulationTick()에서 InputCmd로부터 읽음
    ▼
FLNPSprintModifier / FLNPGuardModifier / FLNPADSModifier (속도 수정자)
    │  OnStart: MaxSpeed·Acceleration을 LNP Settings 값으로 교체
    │  OnEnd:   CDO 원본 값으로 복원
    ▼
LNP.Mover.IsSprinting / LNP.Mover.IsGuarding / LNP.Mover.IsADS (Gameplay Tag)
    └→ IsSprinting()/IsGuarding()/IsADS() 쿼리, ABP 전환 기준
```

- Modifier는 Mover `SyncState`에 포함되어 네트워크 롤백 시 안정적으로 복구된다.
- 의도 플래그가 **InputCmd(FLNPModifierInputs)로 전달되는 이유**는 §7.1 참조 — 이 시스템의 핵심 설계 포인트.
- ADS는 Guard와 같은 구조지만 **키가 겹친다** — `IsFreeAimMode()`로 총기/근접이 갈리므로 둘이 동시에 서지 않는다. ADS 중에는 질주·대시가 InputCmd 단계에서 차단된다. 상세는 §2.6.

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
- `StartingMontagePosition`은 **0으로 고정**한다. 재생 중인 몽타주 인스턴스에서 되읽으면 몽타주가 돌지 않는 서버·리시뮬레이션과 값이 갈린다.

몽타주 평가(`EvaluateMontage`) 실패는 **물리 대시를 취소하지 않는다**. 몽타주는 연출이고, 서버와 클라이언트의 Chooser 평가 결과가 갈릴 경우 그것만으로 시뮬레이션 상태가 분기하기 때문이다.

### 5.2 방향성 대시 — Chooser 연동

물리 방향과 몽타주 방향 태그를 분리해서 결정한다.

| 구분 | 규칙 |
|:---|:---|
| 물리 방향 | 이동 입력 있음 → 컨트롤 회전 기준 입력 방향 / 없음 → 캐릭터 후방 (회피) |
| 몽타주 태그 (일반) | 캐릭터가 이동 방향을 바라보므로 Front/Back 2방향 |
| 몽타주 태그 (Strafe: FreeAim·LockOn) | 시선 고정 상태이므로 입력 각도로 Front/Right/Left/Back 4방향 분류 |

결정된 태그(`LNP.Montage.Value.Direction.*`)로 `EvaluateMontage(TAG_Montage_Situation_Dash, DirTag)`를 호출해 **Chooser Table에서 몽타주를 선택**한다. (하드코딩된 몽타주 프로퍼티 방식에서 마이그레이션 완료)

### 5.3 제한 조건

- 공중 상태에서 발동 불가 (`IsOnGround()`).
- 쿨다운 중 재발동 불가 — `FLNPDashCooldownModifier`의 존재로 판정한다 (§5.4).
- ADS(정조준) 중 발동 불가 — `CanDash()`가 `!IsADS()`를 본다 (§2.6). 단 **이미 나간 대시는 중단하지 않는다.**

### 5.4 네트워크 흐름 — InputCmd 경로 (2026-08-19 이관 완료)

대시는 질주·가드와 동일하게 **의도는 `InputCmd`로 전달하고 실행은 시뮬레이션 안에서** 한다.

1. **입력 콜백** (`ULNPInputHandlerComponent::OnDashStarted`): 버퍼 창(0.05초)만 연다. `ExecuteDash`를 직접 호출하지 않는다.
2. **`OnProduceInput`:** 버퍼가 열려 있는 동안 `FLNPModifierInputs::bWantsToDash`와 `DashInputIntent`를 InputCmd에 싣는다. 방향은 대시 프레임에만 조건부 직렬화해 평상시 대역폭을 쓰지 않는다.
3. **`OnMoverPreSimulationTick`:** `bWantsToDash && CanDash()`이면 `ExecuteDash`를 호출한다. 이 경로는 오토노머스 프록시(예측)·서버(권위)·시뮬레이티드 프록시(포워드 예측) 모두에서 동일하게 실행된다.

**쿨다운은 SyncState에 실린다.** 기존의 `LastDashTime` + `GetWorld()->GetTimeSeconds()` 방식은 서버가 원격 폰을 버퍼된 입력으로 늦게 시뮬레이션하거나 롤백 후 과거 프레임을 재시뮬레이션할 때 클라이언트와 판정이 어긋난다. `FLNPDashCooldownModifier`(이동에 영향 없는 지속시간 Modifier)로 표현해 롤백과 함께 복원되게 했다.

**연출은 리시뮬레이션에서 제외한다.** 몽타주 재생과 `OnDashExecuted`(HUD 쿨다운 파이) 브로드캐스트는 `TimeStep.bIsResimulating`으로 게이팅한다 — 게이팅하지 않으면 롤백마다 몽타주가 다시 재생되고 쿨다운 표시가 계속 리셋된다.

`ControlRotation`과 이동 인텐트는 폰이 아니라 **InputCmd에서 읽는다**. 서버가 원격 폰을 시뮬레이션하는 시점의 폰 현재값은 해당 프레임의 값이 아니라 방향이 갈린다.

---

## 6. 넉백 / Launch

| API | 구현 | 용도 |
|:---|:---|:---|
| `ApplyKnockback()` | `FApplyVelocityEffect` (Instant Effect, 가산 속도) | 피격 넉백 |
| `LaunchWithVelocity()` | `FLayeredMove_Launch` (OverrideVelocity) | 패링 성공 시 공격자 발사 등 |

두 API 모두 **Air 모드로 강제 전환**해서 적용한다 — Ground 모드는 매 틱 속도를 MaxWalkSpeed로 클램프하고 위치를 지면에 스냅하므로 임펄스가 무력화되기 때문 (§7.2).

**네트워크 특성 — 대시와 트리거 방향이 반대라 §7.6의 함정에 해당하지 않는다.** 넉백·Launch는 전부 권위(서버)에서만 트리거된다. 히트 판정 프로세서가 비서버에서 조기 반환하므로(`LNPWeaponTraceProcessors.cpp`, `LNPProjectileProcessors.cpp` — 네트워킹 문서 §3.8의 3-구역 패턴) 결과가 권위 SyncState에 들어가 그대로 복제된다. `SyncFromEntity()`의 `LaunchWithVelocity()`는 액터 승격 시 각 머신이 동일한 Mass 엔티티 속도로 로컬 Mover를 시드하는 의도된 초기화다.

> **정정 (2026-08-21)** — 이전 판에는 "`TriggerRagdoll()`의 넉백은 캡슐 콜리전이 꺼지고 물리 메시로 넘어간 뒤라
> Mover 상태가 무의미하다"고 적혀 있었다. **틀렸다.** 물리 메시로 넘어간 적이 없었고(§9의 4중 결함),
> Mover 넉백은 캡슐만 10000uu/s로 날려 보내고 있었다. 사망 Pop은 이제 Mover가 아니라
> **랙돌 바디에 직접** 준다 — §9 참조.

다만 **넉백은 예측되지 않는다** — 피격자 본인 화면에서 넉백이 RTT만큼 늦게 보정 스냅과 함께 나타난다. 서버 권위 설계의 정상 비용이며, 예측하려면 InputCmd가 아닌 SyncState 트리거로 옮기는 별도 작업이 필요하다 (§8).

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

### 7.5 "적도 부근에서만 카메라가 하체로 내려간다" — Gameplay Cameras 프레임 분석

증상이 **위치 의존적이면서 동시에 시선 방향 의존적**이고 간헐적으로 보인 탓에 초기 진단이 어려웠다. `CR_ThirdPerson`의 Offset 값이 월드 좌표계로 계산되는 것 아니냐는 가설에서 출발했지만, 실제 범인은 에셋 설정이 아니라 **엔진의 Boom Arm 노드가 Roll을 0으로 강제**하는 것이었다(§2.4).

두 단계로 드러났다.

1. **카메라 오프셋이 어긋남** — 보정 노드가 Rotation만 고치고 Location은 두어서, 붐/카메라 오프셋이 월드 Z-Up 평면에 놓였다. Roll 보정각이 180°에 가까워지면 위쪽 오프셋이 아래로 뒤집혀 카메라가 하체를 본다. → 보정 노드가 Roll 델타를 **위치에도** 적용하도록 수정.
2. **고무줄 지연이 상하로 샘** — 1번을 고치자 드러났다. `DampenPosition`은 지연 벡터를 Forward/Lateral/Vertical로 분해해 **축마다 다른 감쇠**를 거는데, 그 축의 기준이 Boom Arm의 Roll=0 회전이었다. 적도에서는 중력 Up과 월드 Z가 약 90° 어긋나 감쇠 축이 사실상 뒤바뀌고, 좌우 이동이 Vertical 축으로 처리되어 카메라가 캐릭터 몸을 따라 미끄러진다. → 보정 노드를 `DampenPosition` **앞**으로 이동.

**교훈:** Gameplay Cameras의 `ECameraNodeSpace::CameraPose`는 "그 시점까지 평가된 `CameraPose.GetRotation()`"이라, 앞선 노드가 어떤 프레임을 남겼는지에 전적으로 의존한다. 구면 중력에서는 **어느 노드가 언제 중력 정렬을 회복시키는지**가 리그 설계의 핵심 제약이 된다.

### 7.6 "클라이언트가 대시하면 서버 화면에 아무 일도 안 일어난다" — §7.1과 같은 함정, 다른 기능 (2026-08-19)

증상이 **완전한 단방향**이었다. 서버가 대시하면 모든 화면에서 정상, 클라이언트가 대시하면 서버 화면에는 아무 변화가 없고 클라이언트 화면에서만 한 번 튀었다가 제자리로 돌아왔다. 같은 키를 공유하는 질주(탭=대시, 홀드=질주)는 정상 동기화되고 있었다는 점이 결정적 단서였다.

원인은 §7.1에서 이미 정리해 둔 함정에 **대시만 빠져 있었던 것**이다. 질주·가드는 `FLNPModifierInputs`를 통해 InputCmd를 타는데, 대시는 입력 콜백에서 `MoverComponent->ExecuteDash()`를 직접 호출해 `QueueLayeredMove`까지 가고 있었다. 그 레이어드 무브는 키를 누른 그 머신의 로컬 Mover에만 들어간다.

- 서버가 누름 → 권위 시뮬레이션에 들어가고 SyncState로 복제 → 정상으로 보인다.
- 클라이언트가 누름 → 예측 시뮬레이션에만 들어간다. 서버는 InputCmd에 대시 비트가 없으니 아무것도 하지 않고, 다음 권위 상태 도착 시 클라이언트가 롤백된다.

즉 **클라이언트가 2명이었다면 어느 쪽 대시도 동작하지 않았다.** 호스트가 곧 권위라 "서버는 되니까 절반은 맞다"고 보인 것뿐이다.

**해결:** §5.4의 InputCmd 경로로 이관. 이관 자체는 비트 하나지만, 함께 처리해야 할 것이 세 가지 있었다.

| 항목 | 그대로 두면 |
|:---|:---|
| 쿨다운을 월드 시간 → SyncState Modifier로 | 서버의 지연 시뮬레이션·리시뮬레이션과 판정이 갈려 무한 리컨사일 |
| 연출을 `bIsResimulating`로 게이팅 | 롤백마다 몽타주 재생·HUD 쿨다운 리셋이 반복 |
| 몽타주 실패 시 조기 return 제거, `StartingMontagePosition` 0 고정 | Chooser 평가·재생 인스턴스 값이 머신마다 달라 시뮬레이션 상태가 분기 |

**교훈:** "예측 파이프라인 바깥에서 상태를 바꾸지 말 것"은 §7.1에서 이미 배운 규칙인데, 새 기능(대시)을 추가할 때 같은 함정을 다시 밟았다. 증상이 **단방향**이면 클라→서버 RPC 경로와 서버→클라 프로퍼티 복제 경로 중 어느 쪽이 끊겼는지부터 가르는 것이 가장 빠른 진단이다.

---

## 8. 미구현 / 한계

- **CanGuard() 상시 허용:** 현재 조건 없이 항상 true. 상태 이상(스태거 등) 연동 시 조건 추가 필요.
- **~~`bIsAiming` 사문화~~ (2026-08-22 해소):** 해당 훅은 삭제하고 `CanDash()`가 `!IsADS()`를 보도록 바꿨다 — 원래 `!bIsAiming`이 노리던 자리 그대로이며, 저장소만 평범한 멤버에서 Mover SyncState 태그로 옮겼다. 질주 차단도 같은 방식으로 `CanSprint()`에 들어갔다. 상세는 §2.6.
- **넉백 미예측:** 권위에서만 트리거되므로 정합성은 맞지만 피격자 본인 화면에서 RTT만큼 늦게 나타난다 (§6). 예측하려면 넉백을 SyncState 트리거로 옮겨야 한다.
- **공중 대시 미지원:** 의도된 제한이지만, 공중 회피 등 확장 여지는 열려 있음.
- **Left/Right 대시 몽타주 에셋:** 4방향 태그 분류·Chooser 평가는 구현 완료 — 좌/우 전용 몽타주 에셋을 Chooser Table에 채우는 에디터 작업 잔여.

---

## 9. 사망 — 랙돌 · Pop 드랍 · 리스폰 (2026-08-21)

```
[플레이어 HP ≤ 0]                                   [Enemy HP ≤ 0]
 └ PossessedBy에 건 Health 델리게이트 (서버 전용)     └ ULNPHealthProcessor (서버 전용 Mass)
   └ HandleDeathOnServer()  ※ bIsDead 1회 가드          └ ALNPEnemyCharacter::TriggerRagdoll()
     ├ ASC->CancelAllAbilities()                          └ Multicast_TriggerRagdoll(PopVelocity)
     ├ DropAllItemsOnDeath()  → LootDice N개                └ [각 머신] EnterRagdoll()
     ├ Multicast_OnDeath(PopVelocity)                     └ DeathCountdown = EnemyRagdollDuration → 엔티티 파괴
     │  └ [각 머신] EnterRagdoll() + 입력/락온/상호작용 차단 + 사망 카메라
     └ GameMode->ScheduleRespawn(PC, PlayerRespawnDelay)
        └ UnPossess → 폰 Destroy → Health 복구 → RestartPlayer (랜덤 PlayerStart)
           └ PossessedBy → EnsureDefaultWeapon() (기본 무기 재지급)
```

**물리는 복제하지 않는다.** 시체는 게임플레이 판정이 없는 코스메틱이라 각 머신이 로컬로 시뮬레이션하고,
방송하는 것은 "죽었다 + Pop 방향" 두 값뿐이다. 서버 권위 처리(드랍·타이머)는 `HandleDeathOnServer`에만 두고
`Multicast_OnDeath_Implementation`은 순수 연출만 담아, 리슨 서버에서 구현부가 로컬로도 실행돼도 중복이 없다.

### 9.1 랙돌이 6개월간 동작하지 않았던 이유 — 4중 결함

`ALNPEnemyCharacter::TriggerRagdoll()`은 존재했지만 실제로 보이는 건 Mover 넉백뿐이었다. 원인이 넷 겹쳐 있었다.

| # | 결함 | 엔진 근거 |
|:--|:--|:--|
| 1 | **물리 바디가 애초에 없다.** BP의 `VisualMesh` 콜리전이 `NoCollision`이라 물리 상태가 생성되지 않아 `Bodies`가 비어 있다 → 모든 시뮬 호출이 no-op | `UPrimitiveComponent::ShouldCreatePhysicsState`(`PrimitiveComponent.cpp:2131`) — `GetCollisionEnabled() != NoCollision`이어야 생성 |
| 2 | **호출 순서가 거꾸로.** 시뮬을 켠 **뒤에** `SetCollisionProfileName("Ragdoll")`을 불렀다. 프로필 변경이 `EnsurePhysicsStateCreated()` → `RecreatePhysicsState()`로 바디를 새로 만들며 방금 세운 플래그를 날린다 | `UPrimitiveComponent::SetCollisionProfileName`(`PrimitiveComponentPhysics.cpp:1408-1417`) |
| 3 | **`SetAllBodiesSimulatePhysics`는 `bBlendPhysics`를 켜지 않는다.** 물리 결과가 포즈에 반영되는 게이트는 `Bodies.Num()>0 && CollisionEnabledHasPhysics(...) && (bBlendPhysics \|\| DoAnyPhysicsBodiesHaveWeight())`인데 셋 다 실패 → **애님 포즈가 물리를 100% 덮어쓴다** | `SkeletalMeshComponentPhysics.cpp:1439-1450` vs `PhysAnim.cpp:398-403`. `SetSimulatePhysics(true)`(`:362-392`)는 372행에서 `bBlendPhysics`를 켠다 |
| 4 | **적 메시에 PhysicsAsset 미할당.** `SKM_UEFN_Mannequin.PhysicsAsset == None` | `PA_UEFN_Mannequin` 에셋은 있었다 — 연결만 빠져 있었다 (2026-08-21 할당·저장) |

따라서 `ALNPCharacterBase::EnterRagdoll()`의 **호출 순서에 의미가 있다**: 몽타주 정지 → 캡슐 콜리전 해제 →
**콜리전 프로필 `Ragdoll`** (바디 생성) → **`SetSimulatePhysics(true)`** (`bBlendPhysics` 포함) →
**`SetAllUseCCD(true)`** (§9.1.1) → `SetEnableGravity(false)` → `WakeAllRigidBodies` → Pop 속도·각속도.
`AnimSourceMesh->SetActive(false)`는 **금지** — 컴포넌트 틱이 죽으면 포즈 갱신이 끊긴다.

#### 9.1.1 지면 관통 — CCD 필수 (2026-08-21)

Pop 속도가 ±2000cm/s이고 프로젝트는 `bTickPhysicsAsync=True`(비동기 고정 스텝)라 물리 1스텝에
수십 cm를 이동한다. 이산 충돌 판정은 스텝 시작·끝 두 지점만 보므로 손·발처럼 바운드가 작은 바디가
지면을 통째로 건너뛴다(터널링) — 실측상 10회 중 9회 관통. **낙하 속도를 낮추는 건 대증요법이고,
해법은 스윕 판정(CCD)이다.**

`VisualMesh->SetAllUseCCD(true)`는 `Bodies`를 순회하며 유효한 `FBodyInstance`에만 적용하므로
(`SkeletalMeshComponentPhysics.cpp:3984`) **바디가 생성된 뒤**, 즉 프로필·시뮬 설정 다음에 불러야 한다.
`ExitRagdoll()`에서는 **되돌리지 않는다.** `bUseCCD`는 컴포넌트가 아니라 바디마다 붙어 있고
(`BodyInstance.cpp:4055`), 프로필을 `NoCollision`으로 복원하면 `TermArticulated()`가 모든 `FBodyInstance`를
파괴하며(`SkeletalMeshComponentPhysics.cpp:1213-1229`) 재진입 시 `CopyBodyInstancePropertiesFrom(DefaultInstance)`
(`:1029`)로 CCD가 꺼진 상태에서 다시 태어나기 때문이다. 시뮬 여부와 무관한 플래그라는 점에 주의 —
Chaos는 키네마틱 파티클도 CCD 경로를 탄다(`CCDUtilities.cpp:84`). **`VisualMesh` 프로필이 콜리전 있는 것으로
바뀌면 이 전제가 깨진다**(바디가 살아남아 `bUseCCD=true` 잔류 → 불필요한 스윕 미드페이즈 비용).

Chaos 쪽 기본값은 그대로 쓴다 — `p.Chaos.CCD.EnableThresholdBoundsScale=0.4`(바디 최소 바운드의 40% 이상
움직이면 CCD 발동), `p.Chaos.CCD.AllowedDepthBoundsScale=0.2`(TOI 롤백 시 허용 관통 깊이).
여전히 새면 이 두 값을 낮추거나(0이면 항상 CCD), 종단 속도 상한을 `TickRagdollGravity`에 넣는 것이 다음 카드다.

### 9.2 랙돌 구형 중력

Chaos 리지드바디는 프로젝트의 커스텀 중력을 모른다. `LootDice`와 같은 전략을 쓴다 —
`SetEnableGravity(false)`로 내장 -Z를 끄고, `ALNPCharacterBase::Tick`에서
`AddForceToAllBodiesBelow(-GetUpDirection() * GravityStrength, NAME_None, bAccelChange=true)`.
`IsAnyRigidBodyAwake()` 체크 후에만 주입한다 — 잠든 바디를 매 틱 깨우면 시체가 영원히 잠들지 못한다.

### 9.3 Mover 정지 — 왜 `UNullMovementMode`를 쓰면 안 되는가

**엔진 `UNullMovementMode`는 이 용도에 쓸 수 없다.** `SimulationTick_Implementation`이 완전히 비어 있는데
(`MovementMode.cpp:155-157`), NP 백엔드는 매 틱 `FMoverTickEndData`를 **기본 생성**해 넘기고
(`MoverNetworkPredictionLiaison.cpp:166`), 상태 머신이 `FindOrAddMutableDataByType<FMoverDefaultSyncState>()`로
기본값 구조체를 만들어 둔다(`MovementModeStateMachine.cpp:253`). 아무도 채우지 않으면 위치가 `ZeroVector`인 채
`FinalizeFrame` → `SetFrameStateFromContext`에 실려 **폰이 월드 원점으로 순간이동한다.**
엔진에서 이 모드는 "초기 플레이스홀더"로만 쓰인다.

대안도 전부 막혀 있다 — `RequestStopMovement()`는 본문이 빈 TODO(`MoverComponent.cpp:831`),
`SetComponentTickEnabled(false)`는 시뮬을 NP 서브시스템이 구동하므로 무효,
`SetUpdatedComponent(nullptr)`은 `FinalizeFrame`이 무조건 역참조해 크래시.

그래서 **`ULNPDeadMode`(`Movement/LNPDeadMode.h/.cpp`)를 직접 만든다** — 시작 상태의 위치·회전을 그대로
되울리고 속도만 0으로 만든다. 입력에 의존하지 않아 결정론적이라 NP 리컨사일도 유발하지 않는다.
`ULNPCharacterMoverComponent::EnterDeadMode()` / `ExitDeadMode()`가 진입점이다.

### 9.4 사망 카메라 — 액터가 아니라 카메라를 옮긴다

`UGameplayCameraComponentBase`는 **자기 컴포넌트 트랜스폼**을 카메라 포즈의 원점으로 쓴다
(`GameplayCameraComponentBase.cpp:668-669`). 따라서 `GameplayCamera`를 폰 계층에서 떼고
매 틱 `SetWorldLocation(랙돌 pelvis)`만 하면 시체 추적이 성립한다 — **회전은 건드리지 않는다**
(시체 회전을 따라가면 화면이 요동친다).

액터 루트를 옮기는 방식은 **쓰면 안 된다**: `UMoverComponent::FinalizeFrame`(`MoverComponent.cpp:356-367`)이
`UpdatedComponent` 위치가 SyncState와 다르면 매 프레임 되돌린다. 폰 계층에 붙인 채 옮기는 것도 안 된다 —
같은 함수가 `PrimaryVisualComponent`의 상대 트랜스폼도 되돌린다(`:387-393`).

**궤도 회전의 기준점(피벗)을 시체 쪽으로 내리는 일은 카메라 리그가 한다** —
`ULNPRagdollPivotOffsetCameraNode`(`Camera/LNPRagdollPivotOffsetCameraNode.h/.cpp`).

카메라 컴포넌트는 이미 랙돌 골반을 따라간다. 그런데 `Offset`(붐암오프셋) 노드가 피벗을
**선 캐릭터 기준 높이**로 들어올리므로, 바닥에 누운 시체 위로 피벗이 붕 뜬다 —
Look 입력으로 카메라를 돌리면 시체가 아니라 그 허공을 중심으로 돈다.
이 노드가 랙돌 동안(`ALNPCharacterBase::IsRagdollActive()`) 그 높이를 상쇄한다.

**튜닝 값은 리그 에셋(`CR_ThirdPerson`)에 있다** — 노드의 `PivotUpOffset`
(`FDoubleCameraParameter`, 기본 -60cm, 음수 = 아래). 리그의 `BoomArmOffset` 높이만큼 빼주는 값이 출발점이다.
카메라 연출을 조정하려고 폰 BP를 열 이유가 없고, 필요하면 리그 인터페이스 파라미터로 노출하거나
카메라 변수로 구동할 수도 있다.

> **회전이 아니라 위치다** (2026-08-21 실측 후 수정). 초기 구현(`ULNPRagdollPitchCameraNode`)은
> BoomArm **뒤에서** 카메라를 피벗 중심으로 25° 굴렸다. 그러면 시선 각도만 바뀌고
> **회전 기준점은 1cm도 움직이지 않는다** — 값을 아무리 키워도 "기준점이 내려간다"는 체감이 없었다.
> 피벗을 옮기려면 BoomArm이 그것을 읽기 **전에** 위치를 바꿔야 한다.
>
> 폰이 ControlRotation을 기울이던 더 이전 방식(`ULNPControlRotationComponent::AddPitchOffsetDeg`,
> `ALNPPlayerCharacter::DeathCameraPitchDownDeg`)도 같은 이유로 폐기·**삭제**했다.

**사망 중 입력 — `SetGameplayInputEnabled(false)`를 쓰면 안 된다.** 그쪽은 `DefaultMappingContext`를 통째로
떼어내 **Look까지 죽인다**. 그래서 `ULNPInputHandlerComponent::SetGameplayInputBlocked(bool)`를 따로 뒀다 —
매핑은 유지한 채 Look을 제외한 입력 콜백 9종(이동·점프·대시·상호작용·공격·가드·ADS·락온·액티브 스킬)이
조기 반환한다. 차단 시점에 눌려 있던 키 상태와 공격·대시 버퍼도 함께 턴다.
리스폰은 폰을 새로 만들므로 플래그를 되돌릴 필요가 없다.

### 9.5 리스폰 — 엔진 함정 2건

| 함정 | 증상 | 대응 |
|:--|:--|:--|
| `AGameModeBase::FindPlayerStart_Implementation`(`GameModeBase.cpp:1168`)이 `ShouldSpawnAtStartSpot`(= `Player->StartSpot != nullptr`)이면 **이전 StartSpot을 그대로 반환** | 리스폰이 항상 같은 지점 — `ChoosePlayerStart`의 랜덤 추첨(`:1130`)에 도달조차 못 한다 | `ALNPGameMode::ShouldSpawnAtStartSpot()`을 `false` 반환으로 오버라이드 |
| `RestartPlayerAtPlayerStart`(`:1287`)는 `NewPlayer->GetPawn() != nullptr`이면 **새 폰을 만들지 않는다** | 리스폰이 조용히 실패 | `RestartPlayer` 전에 `UnPossess()` → 랙돌 폰 `Destroy()` |

`UnPossess()`를 명시적으로 먼저 부르는 이유: 그냥 `Destroy()`하면 `APawn::Destroyed` 경로가
컨트롤러를 Inactive 상태로 밀어 넣는다.

**빙의는 랙돌 10초 동안 유지한다** — `ALNPPlayerController::OnUnPossess`가 `HudWidget->DeinitViewModel()`을
부르므로 일찍 풀면 HUD가 꺼진다. ASC·인벤토리·장비는 PlayerState 소유라 폰을 넘어 살아남으므로,
리스폰 시 Health만 `MaxHealth`로 되돌리면 HUD가 자동 갱신된다.

### 9.6 Enemy 풀 재사용

Enemy Actor는 Mass 표현 풀에서 **재사용**된다. `ALNPEnemyCharacter::SyncFromEntity` 선두에서
`ExitRagdoll()`을 부르지 않으면 재활용된 액터가 랙돌·콜리전 해제 상태로 되살아난다.
`EnterRagdoll`/`ExitRagdoll` 양쪽 모두 멱등이라 매 활성화마다 불러도 무해하다.

### 9.7 사망 오버레이

사망~리스폰 사이에만 화면을 덮는 반투명 카운트다운 UI — `ULNPDeathScreenWidget` / `WBP_LNPDeathScreen`.
폰이 `Multicast_OnDeath`에서 로컬 제어일 때만 `ALNPPlayerController::ShowDeathScreen()`을 부르고,
리스폰 빙의가 걷는다. 상세는 [TechDesign_HUD.md](TechDesign_HUD.md) §12.

### 9.8 검증 도구

```
LNP.Debug.KillPlayer [PlayerIndex]
```
권위(호스트) 콘솔 전용. 대상 플레이어의 Health를 0으로 내려 **정상 사망 경로**를 그대로 태운다
(치트 분기가 아니라 `PossessedBy`의 Health 델리게이트를 탄다). 인수 없으면 0번(호스트), `1`이면 게스트.
리스폰 지점 랜덤성은 여러 번 죽어봐야 확인되므로 이 커맨드가 사실상 필수다.
구현: `Character/LNPPlayerCharacter.cpp` 파일 끝.

**PIE 2인 검증 완료 (2026-08-21):** 랙돌·구형 중력 낙하·시체 추적 카메라·전량 드랍(인벤토리 UI 즉시 갱신)·
10초 카운트다운 오버레이·랜덤 PlayerStart 리스폰·기본 장비 재지급·호스트↔게스트 상호 관전까지 전 구간 정상.
Enemy 랙돌과 Mass 표현 풀 재사용(여러 마리 사살 후 타 지점 재스폰) 정상.
호스트·게스트가 구의 정반대 적도 부근까지 떨어져도 카메라 이상 없음(§2.5 수정 확인).

### 9.9 알려진 한계

- **리스폰마다 `DefaultWeapon` 사본이 늘어난다.** 사망 시 가방을 전부 비우므로 `EnsureDefaultWeapon()`의
  "가방 조회 후 없으면 생성" 멱등성이 매번 새 인스턴스를 만든다. **의도적 방치** — 밸런스 문제가 되면 대응.
- **relevancy 늦은 클라이언트.** `Multicast_OnDeath`는 호출 시점에 relevancy가 없던 클라이언트에는 닿지 않아,
  그 사이 진입한 관전자는 서 있는 캐릭터를 본다. 필요해지면 `bIsDead`를 `ReplicatedUsing`으로 승격해
  OnRep에서 같은 연출을 태운다.
- **데디케이티드 서버는 랙돌을 만들지 않는다** (`Multicast` 구현부에서 조기 반환). 볼 사람이 없으므로
  물리 바디 생성 비용을 아끼는 의도된 최적화이며, 서버 시체 위치에 의존하는 로직이 없어야 한다.
