# 캐릭터 이동 시스템 기술 설계

## 1. 한눈에 보기

UE 5.8 **Mover 2.0** 기반. 구형 중력(위치마다 Up이 다른 Dyson Sphere 내벽)과 멀티플레이 예측·롤백을 함께 지원하려고 역할을 컴포넌트 단위로 나눴다.

```
[입력]  ULNPInputHandlerComponent (IMoverInputProducerInterface)
           │  Enhanced Input 캐싱 → OnProduceInput에서 InputCmd 구성
           │  (FCharacterDefaultInputs + FLNPModifierInputs)
           ▼
[시뮬레이션]  ULNPCharacterMoverComponent (UCharacterMoverComponent 확장)
           │  OnMoverPreSimulationTick: MoveSpeed 큐잉 → Guard → ADS → Sprint → Dash
           │  넉백·Launch는 InstantEffect·LayeredMove로 큐잉
           ▼
[중력]  ULNPPawnGravityComponent — Down/Up 계산, Mover에 Gravity·UpDirection Override
[시점]  ULNPControlRotationComponent — 곡률 보정 + 시선 입력 + 락온 보정을 합산해
           프레임당 SetControlRotation 1회 호출
```

### 컴포넌트 역할 분담

| 컴포넌트 | 역할 |
|:---|:---|
| `ALNPCharacterBase` | 컴포넌트 조합, AI 이동 의도 위임(`SetAIMoveInput`·`SetAIOrientationIntent`·`SetAIDesiredSpeed`), 랙돌(§9) |
| `ULNPInputHandlerComponent` | 입력 수집·버퍼링, InputCmd 생산 |
| `ULNPCharacterMoverComponent` | 이동 시뮬레이션, Sprint/Guard/ADS/MoveSpeed Modifier 관리, 대시·넉백·Launch·사망 정지, 행글라이더 전개·접기(§10) |
| `ULNPPawnGravityComponent` | 구형 중력 방향 결정, Mover Override |
| `ULNPControlRotationComponent` | 컨트롤 회전 전담 — 곡률 보정, 시선 입력(ADS 감도 포함), 락온 보정 소비 |
| `ULNPLockOnComponent` | 락온 대상 유지, 소프트 보정 델타·하드 클램프 방향을 ControlRotation에 적립 |

### 1.1 ⚠️ 이동 의도 벡터는 방향만 담는다

`SetAIMoveInput`에는 **단위 벡터 또는 영벡터만** 넣는다. 속도는 `SetAIDesiredSpeed`로 따로 준다.

- 원인: Mover `UMovementUtils::ComputeVelocity`의 방향 전환 항이 **정규화하지 않은** 의도 벡터를 속도에 곱한다(CMC는 `GetSafeNormal()`을 쓴다). 크기 s<1을 계속 넣으면 속도가 매 프레임 s배로 깎인다.
- 실측: s=0.3에서 기대 180cm/s가 약 27cm/s로 주저앉았다.
- `AIDesiredSpeed`는 `FLNPModifierInputs`(InputCmd)에 실리고, `FLNPMoveSpeedModifier`가 MaxSpeed로 반영한다(§4.1). 컴포넌트 멤버로만 두면 서버에만 값이 있어 적 폰을 재시뮬레이션하는 클라이언트가 CDO MaxSpeed(800)로 폴백한다 — 서버 180 대비 4배로 앞서다 매번 되감겼다(2026-08-28 실측).

---

## 2. 중력 및 방향 제어

### 2.1 중력 타입 (`ELNPGravityType`)

- 중력 크기의 단일 원본은 `Project Settings → Game → LNP Settings → Gravity → GravityStrength`(`ULNPSettings`, 기본 **980 cm/s²**)다. 캐릭터 Mover·랙돌, Mass NPC, 포물선 발사체, LootDice가 공유한다. 컴포넌트·NPC/무기 에셋·LootDice의 개별 중력 속성은 제거했다.
- 중력 크기 튜닝은 PIE 시작 전에 한다. 발사체는 생성 시 값을 공유 프래그먼트에 고정하고 Ghost에도 전달하므로, 비행 도중 설정을 바꿔도 이미 발사된 탄의 중력은 유지된다. 서버·클라이언트는 동일한 프로젝트 설정을 사용해야 한다.

| 모드 | 적용 환경 | Down 방향 |
|:---|:---|:---|
| `Fixed` | 표준 지형 | `FixedGravityDirection` |
| `RadialInward` | 행성 외부 표면 | 구 중심 방향 |
| `RadialOutward` | Dyson Sphere 내벽 | 구 중심 반대 방향 (원심) |

- BeginPlay에서 `ALNPGameState::bIsSphereWorld`를 읽어 `RadialOutward`(원점 0) 또는 `Fixed`(-Z)로 설정한다.
- 매 Tick Owner 위치로 Down/Up을 재계산해 `SetGravityOverride` + `SetUpDirectionOverride`로 넘긴다. Radial 모드는 항상, Fixed는 방향이 바뀔 때만.
- 캡슐 회전은 Mover가 UpDirection으로 처리한다. `SetActorRotation`을 직접 부르지 않는다.

### 2.2 곡률 보정 — `ULNPControlRotationComponent`

구면 위를 이동하면 지역 Up이 계속 변해 화면이 기운다. 이전 프레임 Up → 현재 Up 회전(`FQuat::FindBetweenNormals`)을 컨트롤 회전에 곱해 누적한다.

### 2.3 컨트롤 회전 파이프라인

`UpdateControllerOrientation()`이 매 프레임 아래 순서로 처리하고 `SetControlRotation`을 **한 번만** 호출한다.

```
1. 곡률 보정 (Up 벡터 변화량 누적)
2. 시선 입력 — Yaw: 로컬 Up 축, Pitch: 로컬 Right 축
   (LookYawSensitivity 8 · LookPitchSensitivity 4, ADS 중 ADSLookSensitivityScale 추가 — §2.6)
3. 락온 소프트 보정 (LockOnComponent가 적립한 Yaw/Pitch 델타)
4. Pitch 클램프 (Up과의 각도 약 ±85° — 짐벌락 방지)
5. 락온 하드 클램프 (타겟 방향과 최대 이탈각 초과분만큼 Slerp)
6. Roll-free 회전 재구성 → SetControlRotation
   (카메라 Roll 보정은 LNPGravityRollCorrection 노드 담당 — §2.4)
```

입력 소스(InputHandler·LockOn)는 델타를 **적립**만 하고 소비는 이 컴포넌트가 전담한다. 소스마다 SetControlRotation을 부르면 Tick 순서에 따라 서로 덮어쓴다(§7.3).

### 2.4 카메라 리그 — 구면 중력 보정 (Gameplay Cameras)

`ALNPPlayerCharacter`는 `UGameplayCameraComponent`를 `AnimSourceMesh`에 붙이고 `PossessedBy`에서 `ActivateCameraForPlayerController`로 켠다. 기본 리그는 `CR_ThirdPerson`.

**⚠️ 노드 순서에 의존한다.** 어기면 적도 부근에서 카메라가 하체로 내려가거나 고무줄 지연이 상하로 샌다.

| # | 노드 | 역할 | 기준 프레임 |
|:--|:---|:---|:---|
| 1 | `Offset` (붐암오프셋) | 피벗 오프셋. `BoomArmOffset` 파라미터 | 캐릭터 메시 — 이미 중력 정렬 |
| 2 | **`LNPRagdollPivotOffset`** | 랙돌일 때만 피벗을 중력 Up 방향으로 이동(§9.4). **BoomArm 앞** | 캐릭터 중력 Up |
| 3 | `BoomArm` | Yaw/Pitch 회전 + 피벗 조인트 발행. `BoomOffset = (0,0,0)` | **Roll=0 월드 Z-Up** |
| 4 | **`LNPGravityRollCorrection`** | 피벗 회전을 중력 정렬로 보정 (위치·회전 동시) | — |
| 5 | `LNPMovementBaseDamping` → `DampenPosition` | 이동패널 운반분을 제외한 고무줄 지연. `DampenSpace = CameraPose` | 패널 운반 좌표계에서 4번이 보정한 회전 |
| 6 | `Offset` (카메라오프셋) | 붐 거리. `CameraOffset` 파라미터. `OffsetSpace = CameraPose` | 4번이 보정한 회전 |
| 7 | `CollisionPush` | 카메라가 지형을 뚫지 않게 피벗(safe position) 쪽으로 당김. 엔진 기본값(`Pivot`·`ECC_Camera`·구 10cm·동기 sweep, 플레이어 폰 무시). **위치를 바꾸는 노드 중 맨 뒤** — 앞에 두면 뒤 노드가 붐을 다시 늘려 지형을 뚫는다 | 3번이 발행한 피벗 |
| 8 | `FieldOfView` / `PostProcess` | 포즈 미변경 | — |
| 9 | **`LNPLobbedADSPitch`** | 유탄 ADS일 때만 시선을 아래로 기울임(§2.7). `CR_ADS` **맨 끝** | 4번이 보정한 회전 |

**근본 원인 — Boom Arm이 Roll을 버린다.** `FBoomArmCameraNodeEvaluator::ComputeBoomRotation()`은 피벗 회전을 `FRotator3d(Pitch, Yaw, 0)`으로 만든다. 항상 월드 Z-Up 기준이라, 중력 Up이 월드 Z와 벌어질수록 이 회전을 프레임으로 쓰는 하위 노드가 전부 어긋난다. 어긋나는 각은 위치**와** 시선 방향에 함께 의존해 같은 지점에서도 0°~180°를 오간다.

**`LNPGravityRollCorrectionCameraNode`** (`Camera/LNPGravityRollCorrectionCameraNode.cpp`)
- BoomArm이 발행한 YawPitch 조인트에서 피벗 회전을 읽는다.
- 전방 축은 유지하고 Up만 중력 기준으로 재정렬한 회전을 만든다.
- 둘의 차이(피벗 전방 축 기준 순수 Roll 델타)를 카메라 포즈의 **회전과 피벗 기준 위치에 함께** 적용한다 — "피벗이 처음부터 중력 정렬이었다면 나왔을 결과"와 같다.

**배치 제약**
- `BoomArm` **뒤** — 피벗 조인트를 읽어야 한다. 앞에 두면 조인트가 없어 no-op.
- `DampenPosition`·`Offset` **앞** — 이들이 `CameraPose` 회전을 프레임으로 쓰므로 먼저 보정돼야 감쇠 Vertical 축과 오프셋이 중력 Up 기준이 된다.

**이동패널과 카메라 지연.** `LNPMovementBaseDamping`은 기존 `DampenPosition`을 자식으로 실행한다.
Mover의 movement base가 `ALNPMovingPanel`일 때 연속된 두 평가 사이 패널의 위치·회전 변화를 누적하고,
입력 포즈를 그 운반 좌표계로 되돌려 감쇠한 뒤 월드 좌표로 복원한다. 패널 운반은 즉시 따라가고
패널에 대한 캐릭터 이동은 기존 감쇠를 받는다. 메시의 비균일 스케일은 운반에서 제외한다.
승하차·패널 교체 시에는 누적 좌표계를 유지하되 서로 다른 base 사이의 변환은 적용하지 않아 위치 점프를 막는다.
자식은 `DampenSpace = CameraPose`를 사용하며 기존 3축 감쇠 파라미터 연결은 유지한다. ADS·활공 래퍼도 같은 리그를 공유한다.
입력 유무로 감쇠를 켜고 끄는 방식이 아니므로 점프·관성·넉백의 기존 지연은 유지한다.

### 2.5 ⚠️ 카메라 노드에서 "내 폰"을 얻는 법 (2026-08-21 실측)

**`Context->GetPlayerController()->GetPawnOrSpectator()`를 쓰면 안 된다.** 2인 PIE에서 카메라 컨텍스트가 플레이어당 2개씩 4개 생기는데, 이 방식은 각각 자기 폰 / `None` / **다른 플레이어의 폰**을 집어왔다. 컨트롤러↔폰 관계는 컨텍스트 생성 시점에 확정돼 있지 않다.

- **올바른 경로:** 컨텍스트 Owner가 `UGameplayCameraComponentBase` 자신(`Params.Owner = this`)이므로 그 컴포넌트의 소유 액터가 곧 폰이다.
- 공용 헬퍼 `LNPCamera::ResolveOwningCharacter()`(`Camera/LNPCameraNodeUtils.h`)를 두 노드가 함께 쓴다.
- **캐싱하지 않는다.** `OnInitialize`에서 캐싱하면 리스폰 후 파괴된 폰을 가리킨다. 매 프레임 캐스트 두 번이면 된다.
- BP 카메라 디렉터에서는 엔진 헬퍼 `FindEvaluationContextOwnerActor`가 같은 경로다.

### 2.6 ADS (정조준) — 총기류 전용

`TAG_AimMode_FreeAim`(`ULNPWeaponData::DefaultAimMode`가 부여)일 때만 동작한다. Guard와 **같은 키**를 쓰고 `ULNPInputHandlerComponent::IsFreeAimMode()`로 갈린다 — 총기면 ADS, 근접이면 Guard.

**상태 원본은 눌림 플래그 하나, 유효 상태는 파생값이다.**

```cpp
bool IsADSActive()   const { return bIsADSPressed   &&  IsFreeAimMode(); }
bool IsGuardActive() const { return bIsGuardPressed && !IsFreeAimMode(); }  // 정확히 반대
ModifierInputs.bWantsToGuard = IsGuardActive();   // 원시 bIsGuardPressed를 쓰면 안 된다
```

키를 누른 채 무기를 바꾸면 태그가 바뀌어 이동·카메라·감도가 스스로 풀린다. 대칭이 아니면 무기 교체가 상태를 남긴다.

> **⚠️ 파생값만으로는 부족하다 (2026-08-23).** 가드는 ASC 루즈 태그(`TAG_State_Guarding`·`TAG_State_ParryWindow`), 패링 창 타이머, Mass `FLNPParryStateFragment`를 **입력 순간에 명령형으로** 세팅한다. 폴링이 아니라 무기가 바뀌어도 풀리지 않는다 — 가드 중 총으로 바꾸면 총을 든 채 가드·패링이 유지되고(판정 프로세서가 프래그먼트를 그대로 본다), 속도가 `GuardWalkSpeed`에 묶이며, `CanADS()`까지 막힌다.
>
> - 해제 일체를 `ULNPInputHandlerComponent::ReleaseGuardState()`로 묶었다(키 뗌·가드 브레이크 `Client_ForceReleaseGuard`·무기 교체가 공유).
> - `ALNPCharacterBase::ApplyWeaponVisuals`는 **조준 모드가 실제로 바뀐 경우에만** `NotifyAimModeChanged()`를 부른다. 라이플→피스톨처럼 모드가 같으면 ADS를 유지한다.
> - 이때 가드·ADS 눌림을 **뗀 것으로 처리**하고 새 입력을 요구한다. 조용히 재개하면 이동 모디파이어만 폴링으로 되살아나고 패링 창은 안 열려 둘이 어긋난다.

**두 계층으로 갈린다 — 설계의 핵심.**

| | 소유 경로 | 이유 |
|:--|:---|:---|
| 카메라·조준 감도 | `ALNPCharacterBase::IsADSActive()` 직접 조회 (로컬) | 카메라는 각자 머신에서만 렌더된다 — 복제·예측 대상이 아니다 |
| 대시·질주 차단, 이동 속도 | `FLNPModifierInputs::bWantsToADS`(InputCmd) → `LNP.Mover.IsADS`(SyncState 태그)로 판정 | 서버·리시뮬레이션이 재현해야 한다 |

**카메라 — 리그 프리셋 전환.** `CDE_ThirdPerson`(BlueprintCameraDirector)이 매 프레임 `IsADSActive()`로 `CR_ADS` / `CR_Medium_FreeCam`을 고른다.
- `CR_ADS`는 `CR_ThirdPerson`을 감싸는 래퍼로 `CameraOffset`(붐 단축+우측 오버더숄더)·`FieldOfView`·`DampenPosition` 감쇠만 오버라이드한다. **노드 순서(§2.4)는 한 곳에만 둔다.**
- 블렌드: `EnterTransitions` 0.15s / `ExitTransitions` 0.22s.
- ⚠️ 카메라 컴포넌트를 직접 옮기는 방식은 불가 — `UMoverComponent::FinalizeFrame`이 매 프레임 주 비주얼 컴포넌트의 상대 트랜스폼을 복원한다(§9.4와 같은 원인).

**조준 감도.** FOV를 좁히면 같은 마우스 이동이 화면상 더 크게 돈다. `ADSLookSensitivityScale`(기본 0.65, 기준값 `tan(ADS_FOV/2) / tan(허리사격_FOV/2)`)을 Look 배율에 곱한다. **락온 보정 델타(§2.3의 3·5단계)에는 곱하지 않는다** — 플레이어 입력이 아니다.

**대시·질주 차단** — 판정은 `CanX()`가 소유하고 InputCmd는 의도만 나른다.

```cpp
bool CanSprint() const { return IsOnGround() && !IsGuarding() && !IsADS(); }
bool CanDash()   const { return IsOnGround() && !IsADS() && FindMovementModifierByType<FLNPDashCooldownModifier>() == nullptr; }
bool CanADS()    const { return !IsGuarding() && !IsGliding(); }   // 활공 중엔 지향 사격만 (§10)
```

- **역방향은 넣지 않는다.** `CanGuard()`에 `!IsADS()`를 넣으면 둘이 서로 취소·재시작하며 진동한다. Guard가 틱에서 먼저 처리되므로 **Guard > ADS 단방향**.
- `CanSprint()`는 시작·취소 분기 양쪽에서 평가돼 질주 중 ADS에 들어가면 질주가 풀린다. 이미 나간 대시는 중단하지 않는다 — 끊으면 튄다.
- **한 프레임 지연:** `CanX()`는 SyncState 태그를 보고, 태그는 `QueueMovementModifier` 다음 스텝에 선다. ADS를 누른 프레임에 질주·대시가 한 번 통과할 수 있다(60fps에서 16ms). Guard→Sprint와 같은 성질이라 수용했다. 틱에서 `bWantsToADS`를 직접 보면 즉시가 되지만 판정이 두 곳으로 흩어진다.

**이동 속도** — `FLNPADSModifier`는 `FLNPGuardModifier`와 같은 구조로, `OnStart`에서 `ADSAcceleration`만 적용한다. 실어 나르는 것은 사실상 `LNP.Mover.IsADS` 태그이고, 속도는 `FLNPMoveSpeedModifier`가 계산한다(§4.1). 태그가 SyncState에 있어 리시뮬레이션에서 함께 롤백된다.

**검증 완료:** 총기/근접 키 분기, `-game` 모드, 2인 상호 관찰, 적도 부근 ADS(2026-08-22). 가드↔ADS 무기 교체 즉시 해제, 같은 조준 모드 교체 시 ADS 유지(2026-08-23).

### 2.7 유탄 ADS — 카메라만 아래로 기울인다

유탄(`ELNPProjectileType::Lobbed`)은 멀리 쏠수록 총구를 위로 들어야 하는데, 착탄 가이드 장판은 지표면에
깔린다([TechDesign_HitDetection.md](TechDesign_HitDetection.md) §3.4). 카메라가 발사축과 같은 방향을 보면
정작 멀리 조준할 때 장판이 화면 아래로 밀려나 보이지 않는다. `ULNPLobbedADSPitchCameraNode`가 유탄 ADS
동안에만 카메라를 **제자리에서** `PitchDownDegrees`(기본 10°)만큼 내려 그 둘을 떼어놓는다.

**⚠️ 카메라만 내리면 효과가 정확히 상쇄된다.** 조준점 트레이스가 카메라 시선을 광선 방향으로 쓰면, 카메라를
θ 내린 만큼 조준선도 내려가고 플레이어는 같은 곳을 맞히려 θ만큼 더 올린다 — 카메라 방향은 원래대로 돌아온다.
그래서 조준 광선의 방향을 `ControlRotation`으로 바꾸는 것이 **이 노드가 성립하기 위한 전제**다
(§7.7 표의 마지막 행). 기울이지 않는 무기에서는 카메라 전방과 `ControlRotation`이 정확히 일치하므로
(BoomArm이 컨트롤 회전을 그대로 쓰고, GravityRollCorrection은 전방축 기준 Roll만, DampenPosition은 위치만
건드린다) 동작 차이가 없다.

| 판단 | 근거 |
|:---|:---|
| **발사각을 올리지 않고 카메라를 내린다** | 둘은 기하학적으로 같다(카메라 = 발사축 − θ). 하지만 발사각에 θ를 더하면 **화면 프레이밍용 상수가 서버 탄도 계약의 일부**가 된다 — 서버도 같은 θ를 적용해야 예측이 맞는다. 카메라 쪽에 두면 θ는 로컬 렌더에만 존재하고 재튜닝이 서버를 건드리지 않는다 (§2.6 소유권 표와 같은 원칙) |
| 리그를 나누지 않고 **노드가 스스로 게이팅** | `CDE_ThirdPerson`이 유탄용 리그를 따로 고르게 하면 ADS 오버라이드가 두 리그에 중복된다. `LNPRagdollPivotOffset`과 같은 자기 게이팅 패턴을 따르고, 대신 리그 전환 블렌드가 없으므로 노드 안에서 `BlendSpeedDegreesPerSecond`로 수렴시킨다 |
| 게이트는 `IsLobbedADSActive()` = **가이드 장판이 깔리는 조건과 같은 식** | 둘 다 `GetEffectiveProjectileGravity() > 0`을 본다. 장판이 없는데 카메라만 기울어지는 상태가 원천적으로 생기지 않는다 |
| **맨 끝**에 둔다 | 제자리 회전이라 위치는 안 바뀌지만, `DampenPosition`·`Offset`은 `CameraPose` 회전을 프레임으로 쓴다(§2.4). 앞에 두면 붐 거리와 감쇠 축까지 기울어진다 |

**θ 산정 — 매 프레임 착탄점에서 역산한다.**

```
θ = clamp(조준축_아래_벌어진각 − DesiredImpactBelowCenterDeg, 0, MaxPitchDownDegrees)
조준축_아래_벌어진각 = −atan2(ToImpact·AimUp, ToImpact·AimForward)
```

착탄점은 `ULNPTrajectoryGuideComponent::GetImpactPoint()`에서 **당겨 읽는다** — 카메라가 궤적을 다시
적분하면 카메라가 겨냥하는 곳과 장판이 놓인 곳이 갈린다. 장판을 모르는 프레임(베이킹 전 등)은 θ=0이다.
이 노드는 장판을 프레임 안에 붙잡으려 존재하므로, 붙잡을 것이 없으면 기울일 이유도 없다.

⚠️ **역산에 쓰는 축은 우리 Pitch가 적용되기 전의 `CameraPose`여야 한다.** 리그는 매 프레임 처음부터
다시 평가되므로 이 노드 진입 시점의 회전이 곧 조준축(`ControlRotation`)이고, 앞선 노드가 중력 정렬까지
끝내 둔 상태다(§2.4의 4번). 기울인 뒤의 축을 읽으면 되먹임이 생겨 각이 발산한다.

**고정 θ를 먼저 만들었다가 버렸다 (2026-09-18 플레이 테스트).** 고정값은 시선과 카메라를 일정한 각차로
단단히 묶는 대신, 화면상 장판 위치가 사거리에 따라 내려간다 — 현행 탄도(2800/1200)와 θ=10°로 계산하면
근거리 4.1° / 30m 7.1° / 50m 18.8°로, 50m가 곧 화면 하단 경계였다. 적응형으로 바꾸면 Look 입력과 카메라가
어긋나는 대가를 치르는데, **실측에서 그 대가가 오히려 이득이었다:**

- 상시 조작이면 결함이지만 **ADS 한정**이라 "조준"이라는 플레이 의도에 맞는다.
- 최대 사거리를 넘겨 계속 올리면 θ가 `MaxPitchDownDegrees`에 걸려 **카메라가 장판보다 위로 올라간다.**
  사거리 라벨(§11.13)이 한계를 알려주므로, 그 위를 보려는 의도로 자연스럽게 읽힌다.

⚠️ 위 계산의 발사각은 **구 내벽 곡률을 넣은 값**이다(10m 5.6° / 30m 17.3° / 50m 32.6°). 평면 공식
(`v²sin2α/g`)으로 풀면 30m가 13.7°로 나오지만 실제로는 17.3°다 — 곡률이 사거리를 11% 깎는 것과 같은 보정이다.

`BlendSpeedDegreesPerSecond`(기본 60)는 두 가지를 겸한다 — 조준을 옮기는 동안의 **추적 속도**와, ADS
진입·해제·무기 교체로 게이트가 뒤집힐 때의 수렴. 올리면 추적이 단단해지는 대신 교체 전환이 급해진다.

---

## 3. 이동 속도 설정

**`ULNPCharacterMovementSettings`** (Mover Shared Settings — `ULNPAsyncWalkingMode`가 등록)

| 항목 | 기본값 |
|:---|:---:|
| `SprintSpeed` / `SprintAcceleration` | 1200 cm/s / 6000 cm/s² |
| `SprintStartDirectionAngle` / `SprintCancelDirectionAngle` | 50° / 65° (§4.3) |
| `GuardWalkSpeed` / `GuardAcceleration` | 200 cm/s / 2000 cm/s² |
| `ADSWalkSpeed` / `ADSAcceleration` | 300 cm/s / 2500 cm/s² |

기본 걷기 속도(`MaxSpeed`)는 Mover 기본 `UCommonLegacyMovementSettings`가 관리한다.

**`ULNPCharacterMoverComponent`** (대시)

| 항목 | 기본값 |
|:---|:---:|
| `DashDuration` | 0.2초 |
| `DashCooldown` | 1.0초 |
| `DashImpulseMagnitude` | 2000 cm/s |

---

## 4. 질주 / 가드 / ADS — Modifier 패턴

Mover의 Crouch(Stance) 패턴을 따라 **의도(입력) → Modifier(적용) → Tag(조회)** 3단으로 나눴다.

```
FLNPModifierInputs { bWantsToSprint, bWantsToGuard, bWantsToADS, bWantsToDash, DashInputIntent, bIsLockOn, AIDesiredSpeed }
    │  InputHandler가 InputCmd에 기록 → OnMoverPreSimulationTick에서 읽음
    ▼
FLNPSprintModifier / FLNPGuardModifier / FLNPADSModifier
    │  OnStart: Acceleration만 LNP Settings 값으로 교체 (MaxSpeed는 §4.1 소유)
    │  OnEnd:   Acceleration을 CDO 값으로 복원
    ▼
LNP.Mover.IsSprinting / IsGuarding / IsADS (Gameplay Tag)
    └→ IsSprinting()/IsGuarding()/IsADS() 쿼리, ABP 전환 기준
```

- Modifier는 Mover `SyncState`에 포함돼 롤백 시 함께 복구된다.
- 의도를 **InputCmd로 전달하는 이유**는 §7.1 — 이 시스템의 핵심 규약.
- 처리 순서 Guard → ADS → Sprint. `CanSprint()`가 `!IsGuarding() && !IsADS()`를 본다. 질주는 여기에 방향 판정이 더해진다(§4.3).
- `FLNPModifierInputs::NetSerialize`는 bool 4개를 1비트씩, `DashInputIntent`와 `bIsLockOn`은 대시 프레임에만, `AIDesiredSpeed`는 값이 있을 때만 직렬화한다.
- ⚠️ 공격 입력(조준점·락온·근접 보정 대상)은 이 구조체에 넣지 않는다 — 60Hz로 중복 전송되고 서버는 입력 버퍼 깊이만큼 과거 커맨드를 읽는다. 발동 요청에 싣는다([TechDesign_Networking.md](TechDesign_Networking.md)).

### 4.1 MoveSpeed — `FLNPMoveSpeedModifier` (MaxSpeed 단일 소유자)

GAS `MoveSpeed` 어트리뷰트를 이동 속도에 반영하는 **상시 활성** Modifier. `OnMoverPreSimulationTick` 진입부에서 부재 시 큐잉되고(리시뮬레이션·재빙의로 유실될 수 있어 매 틱 타입 조회) 이후 매 틱 `OnPreMovement`가 실행된다.

```
BaseSpeed = AIDesiredSpeed (InputCmd, >0일 때) 또는 CDO.MaxSpeed
          | CDO.SprintSpeed     (IsSprinting)
          | CDO.GuardWalkSpeed  (IsGuarding)
          | CDO.ADSWalkSpeed    (IsADS)
CommonSettings->MaxSpeed = BaseSpeed × max(0.01, MoveSpeed)
```

**왜 매 틱 CDO에서 재계산하는가.** 버프를 적용 시점에 한 번만 써 두면, Sprint/Guard 종료가 CDO 값을 복원하는 순간 버프가 영구히 사라진다. 매 틱 CDO 기준으로 계산하면 ① 배율이 누적되지 않고 ② Modifier 실행 순서와 무관하다. 그래서 MaxSpeed는 이 Modifier만 쓰고, Sprint/Guard/ADS는 Acceleration만 만진다.

- ⚠️ 배율은 Mover 예측 상태가 아니라 **ASC 어트리뷰트에서 직접** 읽는다. 버프 적용·만료 순간 서버/클라 틱이 어긋나 짧은 보정이 생길 수 있다. 완전 예측이 필요하면 배율을 Modifier `NetSerialize` 페이로드로 옮긴다.
- ⚠️ 질주 진입 첫 프레임은 `OnStart`가 `OnPreMovement` 뒤에 오는 틱이라 기준 속도가 한 틱 늦을 수 있다.

**검증 (2026-07-27, 1인·2인 PIE):** 일반 이동·질주 중·**질주 종료 후** 모두 버프 속도 유지 — CDO 복원으로 인한 소실이 없음을 확인. `ShowSpeed` 실측 avg가 MaxSpeed의 약 97.5%로 수렴(비례 오차, 가속 점근 특성). 이동이 미세하게 어색하다는 리포트가 나오면 `mover.debug.ShowCorrections 1`부터 본다.

### 4.2 디버그 — `LNP.Debug.ShowSpeed [0|1]`

로컬 폰 속도를 화면에 2줄 표시한다(`LNPCharacterMoverComponent.cpp` 하단).

```
[Measured] now   612.3   avg   598.7   peak   780.0  cm/s          (초록)
[Settings] MaxSpeed   780.0  cm/s   state Sprint   MoveSpeed x1.30   (청록)
```

- **실측 줄**은 `FTSTicker`에서 액터 월드 위치의 프레임 간 변화량만으로 구한다 — 이동 로직을 바깥에서 교차 검증한다.
- **설정 줄**은 `기준값 × MoveSpeed = MaxSpeed` 대조용. `state`는 Walk/Sprint/Guard/ADS.
- 엔진 도구 병행: `mover.debug.ShowCorrections`·`ShowTrail`·`ShowTrajectory`, GameplayDebugger Mover 카테고리.

### 4.3 질주 방향 게이트 — 정면으로 갈 때만 질주한다 (2026-10-20)

질주 키를 누르고 있어도 **이동 방향이 바라보는 방향에서 크게 벗어나면 질주 상태 자체가 풀린다.** 키를 계속 누르고 있으면 정면으로 돌아오는 틱에 다시 붙는다(뗐다 누를 필요 없음).

- **동기:** 모션 매칭에 옆·뒷걸음 질주 클립이 없고, ABP는 질주면 이동 방향을 F로 고정한다(GASP 규칙). 스트레이프 중 옆으로 질주하면 정면 질주 포즈가 옆으로 미끄러졌다.
- **판정:** `IsSprintDirectionAllowed()`가 InputCmd의 `MoveInput`과 `OrientationIntent`를 캐릭터 Up(`GetUpDirection()`, 스텝 시작 위치 기준 — §7.9) 접평면에 투영한 사잇각을 본다. 질주 시작은 `SprintStartDirectionAngle`(50°) 이하, 질주 중에는 `SprintCancelDirectionAngle`(65°)을 넘으면 해제. 이동 입력이 없으면 통과한다.
- **50°/65°인 이유:** 키보드 대각(45°)은 질주하고 옆(90°)·뒤는 막는다. 둘을 띄운 것은 스틱 떨림과 `OrientationIntent` 양자화(16비트 고정소수, 서버만 양자화 값을 본다)로 경계에서 질주가 깜빡이거나 보정이 생기지 않게 하려는 것이다.
- **왜 현재 몸 방향이 아니라 `OrientationIntent`인가:**
  - 이동 방향을 바라보는 모드에서는 `OrientationIntent`가 곧 이동 방향이라 각도가 항상 0 — 기존과 같다. 몸 방향과 비교하면 급선회 중 몸이 도는 동안 질주가 끊겼다 붙어 Gait가 깜빡인다.
  - 스트레이프(락온·지향 사격 무기, `bFaceMoveDirection=false`)에서는 `OrientationIntent`가 카메라 수평 전방이라 옆·뒷걸음이 그대로 각도로 드러난다.
  - 락온·무기 모드는 로컬 상태지만 결과가 `OrientationIntent`에 담겨 InputCmd로 간다 — §7.1 규약을 새 필드·대역폭 없이 지킨다.
- **속도는 손대지 않는다:** 태그가 빠지면 §4.1이 달리기 기준 속도를 낸다. 최고속도 초과분은 엔진 `ComputeVelocity`가 감속값과 마찰로 매 틱 깎아 내므로 뚝 끊기지 않는다. 속도만 각도로 보간하는 안은 기각했다 — 태그가 남아 애니메이션이 계속 질주+F 고정이 되어 원래 문제가 그대로다.
- **영향 없음:** AI·ActorPromoted 적은 `bWantsToSprint`가 항상 false다. 스태미나 시스템은 없다.
- ⚠️ 코드가 회전을 덮어쓰는 동안(`OrientationOverride` — 근접 공격 보정)에는 그 방향이 기준이 된다. 질주 중 옆의 적을 치면 질주가 풀릴 수 있다.

**검증 (2026-10-20, 1인·리슨 2인 PIE):** 질주 키를 누른 채 정면 1200 → 좌우·뒤 전환 시 800으로 부드럽게 감속, 정면 복귀 시 1200으로 자연스럽게 재가속. 고속 옆·뒷걸음 포즈가 더는 보이지 않는다. 한계는 §8(이동 속도 버프).

---

## 5. 대시 시스템

반응성과 시각 동기화를 함께 얻는 **하이브리드 레이어드 무브**.

### 5.1 두 레이어드 무브의 역할

| 무브 | 역할 |
|:---|:---|
| `FLayeredMove_LinearVelocity` | 물리 이동. `OverrideAllExceptVerticalVelocity`로 수평만 즉각 추진, 종료 시 `MaintainLastRootMotionVelocity`로 걷기 전환 |
| `FLayeredMove_AnimRootMotion` | 몽타주 동기화. 루트 모션 추출은 쓰지 않아 대시 거리·속도를 코드로 제어 |

- `StartingMontagePosition`은 **0 고정**. 재생 중인 인스턴스에서 되읽으면 몽타주가 돌지 않는 서버·리시뮬레이션과 값이 갈린다.
- 몽타주 평가(`EvaluateMontage`) 실패는 **물리 대시를 취소하지 않는다**. Chooser 결과가 머신마다 갈리면 그것만으로 시뮬레이션이 분기한다.

**수직은 이동 모드에 맡긴다 — 대시 점프의 성립 조건 (2026-10-13).** 처음에는 `OverrideVelocity`였다. 이 모드는
수직까지 덮어써 대시 중 점프하면 `FJumpImpulseEffect`의 상승 속도가 매 틱 지워졌고, 남은 대시 동안 수평으로 떠 가다
곧바로 착지했다(점프가 "안 되는" 것처럼 보였다). `OverrideAllExceptVerticalVelocity`는 믹서가 이동 모드의 수직 성분
(점프 임펄스·중력)을 남긴다(`UMovementMixer::MixProposedMoves`). 엔진도 공중 모드에서 루트 모션 무브를 같은 모드로
바꾼다(`MovementModeStateMachine`, `Mover_SkipVerticalAnimRootMotion`).

- **대시 방향은 중력 평면에 투영한다.** `ControlRotation`에는 카메라 피치가 들어 있어, 투영하지 않으면 버려지는
  수직 성분만큼 수평 속도가 cos(피치)로 줄어 시선 각도에 따라 대시 거리가 달라진다. 투영이 0이면(수직 시선) 캐릭터 전방.
- 점프 쪽은 손대지 않았다. 엔진 `CanActorJump()`가 `IsOnGround()`만 보고 대시는 지상 모드에서 진행된다. 대시가
  `Super::OnMoverPreSimulationTick`(점프 처리)보다 먼저 실행되므로 **같은 프레임 동시 입력도** 대시 점프가 된다.
- MixMode는 uint8로 직렬화돼 InputCmd·넷코드 변경이 없다.
- ⚠️ 이 MixMode에서 `FinishVelocityMode`를 `SetVelocity`로 바꾸면 엔진 `GatherResidualVelocitySettings`가 `check(0)`에 걸린다.
- 부수 효과: 절벽 끝 대시는 남은 대시 동안 수평으로 떠 가지 않고 중력을 받는다. 대시 중 넉백은 수직 성분이 남는다.

### 5.2 방향성 대시 — Chooser 연동

| 구분 | 규칙 |
|:---|:---|
| 물리 방향 | 이동 입력 있음 → 컨트롤 회전 기준 입력 방향 / 없음 → 캐릭터 후방 (회피) |
| 몽타주 태그 (일반) | 캐릭터가 이동 방향을 바라보므로 Front/Back 2방향 |
| 몽타주 태그 (Strafe: FreeAim 무기 또는 락온 중) | 입력 각도로 Front/Right/Left/Back 4방향 |

⚠️ Strafe 판정에서 **락온은 ASC 태그가 아니라 `FLNPModifierInputs::bIsLockOn`으로 받는다.**
`LNP.AimMode.LockOn`은 락온을 건 머신에만 있는 루즈 태그라, 서버·게스트가 원격 폰을 시뮬레이션하면
방향 분류가 갈린다(2026-09-16 수정 — 그 전에는 태그만 보아 락온 중 좌/우 대시가 Front로 떨어졌다).

결정된 태그(`LNP.Montage.Value.Direction.*`)로 `EvaluateMontage(TAG_Montage_Situation_Dash, DirTag)`를 불러 Chooser Table에서 몽타주를 고른다.

### 5.3 제한 조건

- 공중 발동 불가 (`IsOnGround()`). **비대칭이다** — 대시 중 점프는 허용(§5.1, 후방 회피 대시 포함), 점프 중 대시는 금지.
  공중 대시를 열면 그래플과 역할이 겹치고 레벨의 도달 거리 설계가 무너진다. 활공(§10) 중에도 같은 이유로 막힌다.
- 쿨다운 중 불가 — `FLNPDashCooldownModifier` 존재로 판정 (§5.4).
- ADS 중 불가 — 단 이미 나간 대시는 중단하지 않는다 (§2.6).

### 5.4 네트워크 흐름 — InputCmd 경로

의도는 InputCmd로, 실행은 시뮬레이션 안에서.

1. **입력 콜백** (`OnDashStarted`): 버퍼 창(0.05초)만 연다. `ExecuteDash`를 직접 부르지 않는다.
2. **`OnProduceInput`:** 버퍼가 열린 동안 `bWantsToDash`와 `DashInputIntent`를 싣는다.
3. **`OnMoverPreSimulationTick`:** `bWantsToDash && CanDash()`면 `ExecuteDash`. 오토노머스(예측)·서버(권위)·시뮬레이티드(포워드 예측) 모두 같은 경로.

- **쿨다운은 SyncState에 싣는다.** 월드 시간 기준은 서버의 지연 시뮬레이션·롤백 재시뮬레이션에서 클라와 판정이 갈린다. 이동에 영향 없는 지속시간 Modifier `FLNPDashCooldownModifier`로 표현했다.
- **연출은 리시뮬레이션에서 제외한다.** 몽타주 재생과 `OnDashExecuted`(HUD 쿨다운 파이)는 `TimeStep.bIsResimulating`으로 게이팅 — 안 하면 롤백마다 재생·리셋된다.
- `ControlRotation`과 이동 인텐트는 폰이 아니라 **InputCmd에서 읽는다**. 서버가 원격 폰을 시뮬레이션하는 시점의 폰 값은 그 프레임 값이 아니다.

---

## 6. 넉백 / Launch

| API | 구현 | 용도 |
|:---|:---|:---|
| `ApplyKnockback()` | `FApplyVelocityEffect` (Instant Effect, 가산 속도) | 피격 넉백, 근접 패링 성공 시 공격자 넉백 |
| `LaunchWithVelocity()` | `FLayeredMove_Launch` (OverrideVelocity) | Actor 승격 시 로컬 Mover를 Mass 엔티티 속도로 시드 (`ALNPEnemyCharacter::SyncFromEntity`) — 유일한 호출처 |

- 두 API 모두 **Air 모드로 강제 전환**한다. Ground 모드는 매 틱 속도를 MaxSpeed로 클램프하고 지면에 스냅해 임펄스를 무력화한다.
- **네트워크:** 넉백은 권위(서버)에서만 트리거된다. 호출처는 `LNPHitDetectionShared.h`의 판정 적용 커맨드(데미지 적용 단계·패링 넉백)이고, 이를 쌓는 판정 프로세서가 비서버에서 조기 반환한다(네트워킹 문서 §3.8). 결과가 권위 SyncState로 복제되므로 §7.6의 함정에 해당하지 않는다.
- 사망 Pop은 Mover 넉백이 아니라 **랙돌 바디에 직접** 준다(§9). Mover 넉백은 캡슐만 날린다.
- **넉백은 예측되지 않는다** — 피격자 화면에서 RTT만큼 늦게 보정 스냅과 함께 나타난다(§8).

---

## 7. 어필 포인트 (트러블슈팅 & 엔진 분석)

### 7.1 "원격 클라이언트에서만 가드 속도가 안 걸린다" — Mover 입력 파이프라인 분석

- **증상:** Guard/Sprint 의도를 컴포넌트 bool 멤버로 두고 PreSimulationTick에서 읽었더니, 로컬은 완벽했지만 원격 클라이언트에서 간헐적으로 무시됐다.
- **원인:** Mover 시뮬레이션은 `InputCmd → SyncState` 파이프라인으로 예측·복제·리시뮬레이션된다. 평범한 멤버는 그 바깥이라 리시뮬레이션 시점 값과 어긋난다.
- **해결:** 엔진 Jump(`FCharacterDefaultInputs::bIsJumpJustPressed`)와 같은 방식으로 `FMoverDataStructBase` 파생 `FLNPModifierInputs`를 InputCmd 컬렉션에 실었다. `NetSerialize`·`ShouldReconcile`·`Interpolate` 구현.
- **⚠️ 부가 발견:** `FMoverDataStructBase::Interpolate` 기본 구현은 `check(false)`다. Smoothing 서비스가 보간하는 순간 크래시하며 **standalone `-game`에서만** 발현한다. 커스텀 InputCmd 데이터는 반드시 오버라이드한다.

같은 규약이 이후 대시(§7.6)와 AI 이동 속도(§1.1)에서 다시 적용됐다.

### 7.2 Ground 모드의 속도 클램프 vs 넉백 임펄스

Ground 모드에서 넉백을 주면 매 틱 클램프 + 지면 스냅으로 "밀려나는 느낌"이 사라진다. `UCommonLegacyMovementSettings::AirMovementModeName`으로 강제 모드 전환을 걸어 해결. 구형 중력에서는 이후 낙하가 곡면 착지로 자연스럽게 이어진다.

### 7.3 SetControlRotation 단일 진입점 설계

곡률 보정·시선 입력·락온이 각자 SetControlRotation을 부르면 실행 순서에 따라 서로를 덮어쓴다. 모든 소스가 델타를 적립하고 한 컴포넌트가 프레임당 한 번 합산하는 구조로 바꿔, Tick 순서 의존성을 명시적 파이프라인(§2.3)으로 대체했다.

### 7.4 쿼터니언 기반 구면 시점 제어

오일러 기반 시점 제어는 구면에서 짐벌락과 Roll 누적으로 무너진다. 지역 Up 축 Yaw, 지역 Right 축 Pitch를 쿼터니언으로 합성하고, Up·Forward 외적으로 Roll-free 기저를 재구성해 `FMatrix → Rotator`로 마무리한다.

### 7.5 "적도 부근에서만 카메라가 하체로 내려간다" — Gameplay Cameras 프레임 분석

증상이 **위치 의존적이면서 시선 방향 의존적**이라 진단이 어려웠다. 에셋 Offset 설정을 의심했지만 실제 원인은 **엔진 Boom Arm이 Roll을 0으로 강제**하는 것이었다(§2.4). 두 단계로 드러났다.

1. **카메라 오프셋 어긋남** — 보정 노드가 회전만 고치고 위치는 두어, 오프셋이 월드 Z-Up 평면에 놓였다. Roll 보정각이 180°에 가까우면 위쪽 오프셋이 아래로 뒤집힌다. → Roll 델타를 **위치에도** 적용.
2. **고무줄 지연이 상하로 샘** — `DampenPosition`은 지연을 Forward/Lateral/Vertical로 분해해 축마다 다르게 감쇠하는데, 그 축이 Roll=0 회전 기준이었다. 적도에서는 중력 Up과 월드 Z가 약 90° 어긋나 좌우 이동이 Vertical 축으로 처리됐다. → 보정 노드를 `DampenPosition` **앞**으로.

**교훈:** `ECameraNodeSpace::CameraPose`는 "그 시점까지 평가된 포즈 회전"이라 앞선 노드가 남긴 프레임에 전적으로 의존한다. 구면 중력에서는 **어느 노드가 언제 중력 정렬을 회복시키는지**가 리그 설계의 핵심 제약이다.

### 7.6 "클라이언트가 대시하면 서버 화면에 아무 일도 안 일어난다" — §7.1과 같은 함정, 다른 기능 (2026-08-19)

- **증상(완전한 단방향):** 서버 대시는 모든 화면에서 정상. 클라 대시는 클라 화면에서만 한 번 튀고 제자리로 돌아왔다. 같은 키의 질주(탭=대시, 홀드=질주)는 정상 — 이것이 결정적 단서였다.
- **원인:** 질주·가드는 InputCmd를 타는데, 대시만 입력 콜백에서 `ExecuteDash()` → `QueueLayeredMove`를 직접 불러 **누른 머신의 로컬 Mover에만** 들어갔다. 호스트는 곧 권위라 정상으로 보였을 뿐, 클라이언트가 둘이었다면 어느 쪽 대시도 동작하지 않았다.
- **해결:** §5.4의 InputCmd 경로로 이관하고, 함께 처리해야 했던 세 가지:

| 항목 | 그대로 두면 |
|:---|:---|
| 쿨다운을 월드 시간 → SyncState Modifier로 | 서버 지연 시뮬레이션·리시뮬레이션과 판정이 갈려 무한 리컨사일 |
| 연출을 `bIsResimulating`로 게이팅 | 롤백마다 몽타주 재생·HUD 쿨다운 리셋 반복 |
| 몽타주 실패 시 조기 return 제거, `StartingMontagePosition` 0 고정 | Chooser 평가·재생 인스턴스 값이 머신마다 달라 상태 분기 |

**교훈:** 증상이 **단방향**이면 클라→서버 경로와 서버→클라 복제 경로 중 어느 쪽이 끊겼는지부터 가르는 것이 가장 빠르다.

### 7.7 "대시할 때 가끔 보정 캡슐이 뜬다" — 서브스텝 사이에 갱신되지 않는 상태 캐시 (2026-10-13)

- **증상:** 2인 PIE 게스트가 대시하면 6번 중 3~4번 빨강·초록 보정 캡슐이 떴다. 점프만으로는 뜨지 않았다.
  대시 점프 작업(§5.1) 검증 중 발견했지만 일반 대시에서도 똑같이 났다.
- **진단 순서:** `np.PrintReconciles 1`로 서버·로컬 상태를 나란히 찍자, 처음 17건 중 16건이 적 Actor였다 —
  Fixed 틱은 한 인스턴스만 보정돼도 **모든 예측 인스턴스를 함께 롤백**하고, `Mover.LocalPlayer.ShowCorrections`는
  롤백될 때마다 캡슐을 그린다. `LNP.Spawn.EnemyDensity 0`으로 적을 지운 뒤 남은 보정은 전부 대시 전후였고,
  서버와 게스트의 **대시 무브 개수**(2 vs 4)와 **대시 실행 프레임**이 갈려 있었다.
- **원인:** `CanDash()`가 보는 `IsOnGround()`·`HasGameplayTag()`·`FindMovementModifierByType()`은 컴포넌트의
  캐시(`MoverSyncStateDoubleBuffer`)를 읽는데, 엔진은 이것을 **화면 프레임마다(`FinalizeFrame`)와 롤백 시점에만**
  갱신한다. 한 화면 프레임에 60Hz 고정 틱이 두 스텝 돌면 두 번째 스텝은 첫 스텝이 시작한 대시와 쿨다운을 못 보고
  **입력 버퍼 창 안에서 대시를 한 번 더 실행**했다. 스텝 수는 머신마다 달라 판정이 갈렸다.
  진단 로그에서 이중 실행은 예외 없이 **두 프레임이 같은 시각에 찍힌** 경우였다.
- **해결:** `ULNPCharacterMoverComponent::OnPreSimulate`(virtual, 이번 스텝의 `StartingData`를 받는다)에서
  `UpdateCachedFrameState`로 캐시를 시작 상태에 맞춘 뒤 `Super`를 부른다. 질주·가드·ADS 판정도 같은 캐시를 읽으므로
  함께 결정적이 된다. 기본 백엔드(NetworkPrediction)는 게임 스레드에서 스텝을 돌려 경합이 없다.
- **검증:** 같은 조건 대시 12회(일반·대시 점프 각 6) 전부 서버·게스트가 같은 프레임에 1회 실행.
  잔여 보정 1건은 성격이 다르다 — 무브·Modifier·속도는 일치하고 위치만 약 1프레임 이동분(33cm) 어긋났다(→ §7.8).

**교훈:** `OnPreSimulationTick` 안의 `Can*()` 판정은 "이번 스텝의 상태"를 본다고 가정하기 쉽지만, 엔진 쿼리 함수는
**마지막으로 확정된 화면 프레임**을 본다. 보정이 **가끔만** 나면 머신마다 다른 것(프레임당 서브스텝 수)을 의심하라.

### 7.8 "속도는 맞는데 위치만 한 틱 어긋난다" — 낡은 컴포넌트 위치를 쓰는 엔진 점프 이펙트 (2026-10-13)

- **증상:** §7.7 수정 뒤 남은 보정. 대시 점프 직후 공중에서 무브·Modifier·속도(약 2120cm/s)는 일치하고 위치만 약 32cm 갈렸다.
  같은 틱에 `Simulation start location ... disagrees with actual component location` 경고(LogMover)가 찍혔다.
- **원인:** 엔진 `FJumpImpulseEffect::ApplyMovementEffect`(BasicInstantMovementEffects.cpp)는 출력 **위치·회전을
  `UpdatedComponent`에서** 읽고 속도만 시작 상태에서 계산한다. Async 모드는 시뮬레이션 중 컴포넌트를 옮기지 않으므로
  컴포넌트는 `FinalizeFrame`(화면 프레임마다)과 롤백 때만 따라온다. 한 화면 프레임에 고정 틱이 두 번 돌면 두 번째 틱의
  컴포넌트는 한 틱 늦고, 재시뮬 중엔 롤백 지점에 멈춰 있다. 점프가 그 틱에 걸리면 **직전 틱 이동분이 사라진다.**
  로그 실측: 서버는 2371(대시)·2372(점프)를 한 화면 프레임에, 게스트는 따로 돌렸다. 서버 2372 결과 =
  2371 시작점 + 한 틱 이동이었다. 보정 허용 오차가 축별 5cm라 걷기 점프도 걸릴 수 있고, 대시는 크게 보였을 뿐이다.
  넉백에 쓰는 `FApplyVelocityEffect`도 같은 결함이 있다.
- **경고 자체는 원인이 아니다:** `CheckForExternalChanges`는 `bAcceptExternalMovement`(기본 0)가 꺼져 있으면 경고만 한다.
  Async 모드에서는 서브스텝마다 찍히는 정상 부산물이다. Fixed 틱 스무딩도 메시(시각 컴포넌트)만 옮긴다.
- **해결:** `FLNPJumpImpulseEffect`·`FLNPApplyVelocityEffect`(`Movement/LNPInstantMovementEffects`) — 부모를 실행한 뒤
  위치·회전만 서브스텝 시작 상태로 되돌린다. `ULNPCharacterMoverComponent::Jump()`를 오버라이드해 점프 이펙트를 바꾸고,
  `ApplyKnockback`도 파생형을 쓴다. 엔진이 고쳐지면 부모 타입으로 되돌린다.
- **검증:** 2인 PIE, `LNP.Spawn.EnemyDensity 0`·`t.MaxFPS 40`(프레임당 2틱을 자주 만든다), 대시 점프·질주 점프 다수 —
  점프 직후 "속도 일치·위치만 한 틱" 보정 0건. 남은 보정 4건은 성격이 다르다: 점프 입력·레이어드 무브가 없는
  착지 틱 불일치 3건(서버 Walking, 게스트 Falling)과 공중 수평 속도 차 1건(약 13cm/s) → §7.9.

**교훈:** 시뮬레이션 안에서 `UpdatedComponent`·액터 위치를 읽는 코드는 엔진 것이라도 의심하라. Async 모드에서
컴포넌트는 "마지막 화면 프레임"이고, 시뮬레이션의 현재 위치는 시작 sync state뿐이다(그래플 원점도 같은 이유, WorldDevice 문서).

### 7.9 "점프도 안 했는데 착지 틱이 갈린다" — 화면 프레임에 묶인 중력·Up 방향 (2026-10-13)

- **증상:** §7.8 뒤 남은 보정 4건. 점프 입력·레이어드 무브가 없다. 시작 위치의 반지름을 따라가 보면 굴곡 지형을
  질주 속도(1000~1900cm/s)로 달리며 약 0.6초마다 둔덕에서 떠올랐다(±130cm) 착지하고 있었다.
  3건은 착지 틱 자체가 비교됐다(모드가 다르면 위치와 무관하게 보정). 1건은 이전 착지에서 한 틱 갈려 생긴 속도 차(13cm/s)가
  위치로 쌓여 축별 5cm를 넘은 것이다 — **속도는 보정 비교 대상이 아니다**(`FMoverDefaultSyncState::ShouldReconcile`은 위치만 본다).
- **원인:** `ULNPPawnGravityComponent::TickComponent`가 화면 프레임마다 **액터 위치**로 중력·Up 방향 Override를 계산했다.
  §7.8과 같은 이유로 두 번째 서브스텝은 한 틱 늦고(반지름 3만cm에서 0.5~1e-3 rad), 리시뮬레이션 중에는 롤백 전 최신 위치에
  멈춰 있다. 프레임당 틱 묶음이 서버·게스트마다 달라 값이 갈린다. AsyncFalling/Walking은 이 값으로 수평·수직 분해,
  중력 벡터, 착지 시 수직 속도 제거, 경사·바닥 판정을 한다. 오차는 비행 한 번에 수 mm지만, 착지·이륙은 이산 결정이라
  경계 근처에서 한 틱이 뒤집힌다(틱당 수직 10cm 대비 확률 약 5%, 착지 약 55회에 보정 4건과 자릿수 일치).
- **배제:** 서버 입력 기아(해당 구간 SystemFault 없음), MoveInput 양자화(`SetMoveInput`이 로컬도 0.01 단위로 반올림),
  시뮬 블랙보드(롤백 시 무효화), MaxSpeed(매 틱 결정적 재계산).
- **해결:** `ULNPCharacterMoverComponent::OnPreSimulate`에서 스텝 시작 sync 위치로 `ULNPPawnGravityComponent::ApplyGravityAt`을
  부른다. TickComponent 쪽 Override는 시뮬레이션 밖 소비자(카메라·애니메이션)를 위해 남긴다 — 매 스텝 다시 덮어쓰므로 무해하다.

**교훈:** 시뮬레이션이 읽는 "설정"도 상태다. 위치에서 파생된 값을 게임 틱에서 써 넣으면 서브스텝·리시뮬레이션마다 낡는다.
보정이 이산 전이(착지·이륙)에서만 난다면 연속 입력의 미세한 차이가 경계에서 증폭된 것을 의심하라.

---

## 8. 미구현 / 한계

- **`CanGuard()` 상시 true:** 이동 쪽 가드 조건은 없다. 가드 브레이크(경직)는 입력 쪽 `Client_ForceReleaseGuard` → `ReleaseGuardState()`로 푼다([TechDesign_Poise.md](TechDesign_Poise.md)).
- **버프로 빨라진 옆·뒷걸음은 남는다:** 질주 방향 게이트(§4.3)는 질주 태그만 막는다. `MoveSpeed` 버프가 달리기 속도를 질주 수준까지 올리면 고속 옆·뒷걸음 모션이 다시 어색해질 수 있다. 고치기로 하면 그때 방향별 상한(예: 옆·뒤 기준 속도에 버프 상한)이나 애니메이션 쪽 대응을 따져 본다.
- **넉백 미예측:** 정합성은 맞지만 피격자 화면에서 RTT만큼 늦다(§6). 예측하려면 SyncState 트리거로 옮겨야 한다.
- **공중 대시 미지원:** 의도된 제한. 대시 점프는 지원한다(§5.1).
- **대시 점프 도달 거리 미실측:** 평지 근사 추정은 약 13~15m(질주 점프 약 8.4m). 그래플·런처 전용으로 의도한
  8~15m 틈이 넘어갈 수 있어 레벨 점검이 필요하다. 별도 수치로 조절하려면 점프 순간 대시 무브를 취소하고 수평 속도를
  따로 정하는 방식(태그 무브 + `CancelFeaturesWithTag`)이 필요하다.
- **대시 몽타주의 공중 잔류:** 대시 점프 시 FullBody 몽타주가 공중에서 최대 0.2초 남는다. 어색하면 공중 진입 시
  블렌드 아웃(로컬, `bIsResimulating` 게이팅).
- **활공 한계:** §10.6 참조.
- **대시 몽타주 표시:** `AM_MM_Dash_*` 4종은 Chooser에 배선돼 있고 슬롯도 맞췄다(`ABP_Lyra`에 `FullBody` 슬롯 추가,
  2026-09-16 — 그 전에는 슬롯이 없어 대시 포즈가 아예 보이지 않았다). 이동 자체는 슬롯과 무관하다(LayeredMove).

---

## 9. 사망 — 랙돌 · Pop 드랍 · 리스폰 (2026-08-21)

```
[플레이어 HP ≤ 0]                                   [Enemy HP ≤ 0 — ActorPromoted]
 └ PossessedBy에 건 Health 델리게이트 (서버 전용)     └ ULNPHealthProcessor (서버 전용 Mass)
   └ HandleDeathOnServer()  ※ bIsDead 1회 가드          └ ALNPEnemyCharacter::TriggerRagdoll()
     ├ ASC->CancelAllAbilities()                          └ Multicast_TriggerRagdoll(PopVelocity)
     ├ DropAllItemsOnDeath()  → LootDice N개                └ [각 머신] EnterRagdoll()
     ├ Multicast_OnDeath(PopVelocity)                     └ EnemyRagdollDuration(5초) 후 엔티티 파괴
     │  └ [각 머신] EnterRagdoll() + 입력/락온/상호작용 차단 + 사망 카메라
     ├ FLNPPlayerDeadTag 부여 (지연 커맨드 — 적 타게팅에서 제외)
     └ GameMode->ScheduleRespawn(PC, PlayerRespawnDelay=10초)
        └ UnPossess → 폰 Destroy → Health 복구 → RestartPlayer (랜덤 PlayerStart)
           └ PossessedBy → EnsureDefaultWeapon() (기본 무기 재지급)
```

순수 엔티티(`PureEntity`) 적의 사망 팝은 Actor 랙돌이 아니다 — [TechDesign_EnemyNPC_LowLOD.md](TechDesign_EnemyNPC_LowLOD.md) 참조.

**물리는 복제하지 않는다.** 시체는 판정 없는 코스메틱이라 각 머신이 로컬로 시뮬레이션하고, 방송하는 것은 "죽었다 + Pop 방향"뿐이다. 서버 권위 처리(드랍·타이머)는 `HandleDeathOnServer`에만 두고 `Multicast_OnDeath_Implementation`은 순수 연출(`bDeathFxPlayed` 멱등)이라 리슨 서버에서 로컬 실행돼도 중복이 없다.

### 9.1 랙돌이 6개월간 동작하지 않았던 이유 — 4중 결함

`TriggerRagdoll()`은 있었지만 보이는 건 Mover 넉백뿐이었다.

| # | 결함 | 엔진 근거 |
|:--|:--|:--|
| 1 | **물리 바디가 없다.** `VisualMesh`가 `NoCollision`이라 물리 상태가 생성되지 않아 모든 시뮬 호출이 no-op | `UPrimitiveComponent::ShouldCreatePhysicsState` — `NoCollision`이 아니어야 생성 |
| 2 | **호출 순서가 거꾸로.** 시뮬을 켠 **뒤** `SetCollisionProfileName("Ragdoll")` → 바디 재생성이 방금 세운 플래그를 날린다 | `SetCollisionProfileName` → `RecreatePhysicsState()` |
| 3 | **`SetAllBodiesSimulatePhysics`는 `bBlendPhysics`를 켜지 않는다.** 물리 포즈 반영 게이트를 통과 못 해 **애님 포즈가 물리를 100% 덮어쓴다** | `SetSimulatePhysics(true)`만 `bBlendPhysics`를 켠다 |
| 4 | **적 메시에 PhysicsAsset 미할당** | `PA_UEFN_Mannequin` 연결 누락 (2026-08-21 할당). 이제 없으면 경고 로그 후 건너뛴다 |

그래서 `ALNPCharacterBase::EnterRagdoll()`의 **순서에 의미가 있다**: 몽타주 정지 → 캡슐 콜리전 해제 → **프로필 `Ragdoll`**(바디 생성) → **`SetSimulatePhysics(true)`** → **`SetAllUseCCD(true)`**(§9.1.1) → `SetEnableGravity(false)` → `WakeAllRigidBodies` → Pop 속도·각속도 → `EnterDeadMode()`.
⚠️ `AnimSourceMesh->SetActive(false)`는 **금지** — 컴포넌트 틱이 죽으면 포즈 갱신이 끊긴다.

#### 9.1.1 지면 관통 — CCD 필수

Pop 속도 ±2000cm/s에 `bTickPhysicsAsync=True`(비동기 고정 스텝)라 1스텝에 수십 cm를 움직인다. 이산 판정은 손·발 같은 작은 바디가 지면을 건너뛴다(실측 10회 중 9회 관통). 속도를 낮추는 건 대증요법이고, 해법은 스윕 판정(CCD)이다.

- `SetAllUseCCD(true)`는 유효한 `FBodyInstance`에만 적용되므로 **바디 생성 뒤**(프로필·시뮬 설정 다음)에 부른다.
- `ExitRagdoll()`에서 **되돌리지 않는다.** `bUseCCD`는 바디마다 붙고, 프로필을 `NoCollision`으로 복원하면 바디가 전부 파괴돼 재진입 시 CCD 꺼진 기본값으로 다시 태어난다.
- ⚠️ `VisualMesh` 기본 프로필이 콜리전 있는 것으로 바뀌면 이 전제가 깨진다(바디가 살아남아 `bUseCCD=true` 잔류 → 불필요한 스윕 비용). 그때는 명시적 해제가 필요하다.
- 여전히 새면 `p.Chaos.CCD.EnableThresholdBoundsScale`(기본 0.4)·`AllowedDepthBoundsScale`(0.2)을 낮추거나 `TickRagdollGravity`에 종단 속도 상한을 넣는다.

### 9.2 랙돌 구형 중력

Chaos 바디는 커스텀 중력을 모른다. `LootDice`와 같은 전략 — `SetEnableGravity(false)`로 내장 -Z를 끄고 `TickRagdollGravity()`가 `AddForceToAllBodiesBelow(-Up × GravityStrength, bAccelChange=true)`를 준다. `IsAnyRigidBodyAwake()`일 때만 — 잠든 바디를 매 틱 깨우면 시체가 영원히 잠들지 못한다.

### 9.3 Mover 정지 — 왜 `UNullMovementMode`를 쓰면 안 되는가

**엔진 `UNullMovementMode`는 폰을 월드 원점으로 순간이동시킨다.** `SimulationTick`이 비어 있는데, NP 백엔드가 매 틱 `FMoverTickEndData`를 기본 생성하고 상태 머신이 기본값 `FMoverDefaultSyncState`를 만들어 둔다. 아무도 채우지 않으면 위치 `ZeroVector`가 `FinalizeFrame`에 실린다. 엔진에서 이 모드는 초기 플레이스홀더용이다.

대안도 막혀 있다 — `RequestStopMovement()`는 빈 TODO, `SetComponentTickEnabled(false)`는 시뮬을 NP가 구동해 무효, `SetUpdatedComponent(nullptr)`는 `FinalizeFrame`이 역참조해 크래시.

그래서 **`ULNPDeadMode`**(`Movement/LNPDeadMode.h/.cpp`)를 만들었다 — 시작 상태의 위치·회전을 되울리고 속도만 0. 입력에 의존하지 않아 결정론적이라 리컨사일을 유발하지 않는다. 진입점 `ULNPCharacterMoverComponent::EnterDeadMode()` / `ExitDeadMode()`(낙하 모드로 복귀).

### 9.4 사망 카메라 — 액터가 아니라 카메라를 옮긴다

`UGameplayCameraComponentBase`는 **자기 컴포넌트 트랜스폼**을 포즈 원점으로 쓴다. 그래서 `BeginDeathCameraFollow()`가 `GameplayCamera`를 폰 계층에서 떼고, `TickDeathCameraFollow()`가 매 틱 랙돌 앵커 본 위치로 `VInterpTo` 후 `SetWorldLocation`만 한다. **회전은 건드리지 않는다**(시체 회전을 따라가면 화면이 요동친다).

- ⚠️ 액터 루트를 옮기면 안 된다 — `UMoverComponent::FinalizeFrame`이 `UpdatedComponent`를 SyncState 위치로 되돌린다.
- ⚠️ 폰 계층에 붙인 채 옮겨도 안 된다 — 같은 함수가 `PrimaryVisualComponent`의 상대 트랜스폼을 되돌린다.

**궤도 피벗을 시체 쪽으로 내리는 일은 카메라 리그가 한다** — `ULNPRagdollPivotOffsetCameraNode`. `Offset`(붐암오프셋) 노드가 피벗을 선 캐릭터 높이로 올려, 누운 시체 위 허공을 중심으로 카메라가 돈다. 이 노드가 랙돌 동안(`IsRagdollActive()`) 중력 Up 방향으로 그 높이를 상쇄한다.

- 튜닝 값은 리그 에셋(`CR_ThirdPerson`)의 `PivotUpOffset`(`FDoubleCameraParameter`, 기본 -60cm). `BoomArmOffset` 높이만큼 빼는 값이 출발점이다.
- **회전이 아니라 위치다.** 초기 구현(`ULNPRagdollPitchCameraNode`, 삭제)은 BoomArm **뒤에서** 카메라를 굴려 시선 각도만 바뀌고 기준점은 그대로였다. 피벗을 옮기려면 BoomArm이 읽기 **전에** 위치를 바꿔야 한다. ControlRotation을 기울이던 더 이전 방식도 같은 이유로 삭제했다.

**사망 중 입력 — `SetGameplayInputEnabled(false)`를 쓰면 안 된다.** 매핑 컨텍스트를 통째로 떼어 **Look까지 죽인다**. `ULNPInputHandlerComponent::SetGameplayInputBlocked(true)`는 매핑을 유지한 채 Look을 제외한 입력 콜백이 조기 반환하고, 눌린 키 상태·공격·대시 버퍼도 턴다. 리스폰은 폰을 새로 만들므로 되돌릴 필요가 없다.

### 9.5 리스폰 — 엔진 함정 2건

| 함정 | 증상 | 대응 |
|:--|:--|:--|
| `AGameModeBase::FindPlayerStart_Implementation`이 `ShouldSpawnAtStartSpot`(= `StartSpot != nullptr`)이면 **이전 StartSpot을 그대로 반환** | 리스폰이 항상 같은 지점 — 랜덤 추첨에 도달 못 함 | `ALNPGameMode::ShouldSpawnAtStartSpot()` → `false` |
| `RestartPlayerAtPlayerStart`는 컨트롤러가 폰을 갖고 있으면 **새 폰을 만들지 않는다** | 리스폰이 조용히 실패 | `RestartPlayer` 전에 `UnPossess()` → 랙돌 폰 `Destroy()` |

- `UnPossess()`를 먼저 부르는 이유: 그냥 `Destroy()`하면 `APawn::Destroyed` 경로가 컨트롤러를 Inactive 상태로 민다.
- **빙의는 랙돌 동안 유지한다** — `ALNPPlayerController::OnUnPossess`가 `HudWidget->DeinitViewModel()`을 불러 일찍 풀면 HUD가 꺼진다. ASC·인벤토리·장비는 PlayerState 소유라 살아남고, Health만 `MaxHealth`로 되돌리면 HUD가 자동 갱신된다.

### 9.6 Enemy 풀 재사용

Enemy Actor는 Mass 표현 풀에서 재사용된다. `ALNPEnemyCharacter::SyncFromEntity` 선두에서 `ExitRagdoll()`을 부르지 않으면 재활용 액터가 랙돌 상태로 되살아난다. `EnterRagdoll`/`ExitRagdoll`은 멱등이다.

### 9.7 사망 오버레이

사망~리스폰 사이 반투명 카운트다운 UI — `ULNPDeathScreenWidget` / `WBP_LNPDeathScreen`. `Multicast_OnDeath`에서 로컬 제어일 때 `ALNPPlayerController::ShowDeathScreen()`을 부르고 리스폰 빙의가 걷는다. 상세는 [TechDesign_HUD.md](TechDesign_HUD.md) §12.

### 9.8 검증 도구

```
LNP.Debug.KillPlayer [PlayerIndex]
```

권위(호스트) 콘솔 전용. 대상 Health를 0으로 내려 **정상 사망 경로**(`PossessedBy`의 Health 델리게이트)를 그대로 태운다. 인수 없으면 0번(호스트). 리스폰 랜덤성 확인에 사실상 필수. 구현: `Character/LNPPlayerCharacter.cpp` 끝.

**PIE 2인 검증 완료 (2026-08-21):** 랙돌·구형 중력 낙하·시체 추적 카메라·전량 드랍·카운트다운·랜덤 리스폰·기본 장비 재지급·상호 관전, Enemy 랙돌과 풀 재사용, 구 정반대 적도 부근 카메라(§2.5 수정 확인).

### 9.9 알려진 한계

- **리스폰마다 `DefaultWeapon` 사본이 늘어난다.** 사망 시 가방을 비우므로 `EnsureDefaultWeapon()`이 매번 새 인스턴스를 만든다. 의도적 방치 — 밸런스 문제가 되면 대응.
- **relevancy 늦은 클라이언트.** `Multicast_OnDeath`는 호출 시점에 relevancy가 없던 클라이언트에 닿지 않아, 그 사이 진입한 관전자는 서 있는 캐릭터를 본다. 필요하면 `bIsDead`를 `ReplicatedUsing`으로 승격한다.
- **데디케이티드 서버는 적 랙돌을 만들지 않는다** (`Multicast_TriggerRagdoll` 구현부 조기 반환). 서버 시체 위치에 의존하는 로직이 없어야 한다. 플레이어 `Multicast_OnDeath`에는 이 조기 반환이 없다.

---

## 10. 행글라이더 활공 (2026-10-13)

공중에서 점프를 다시 누르면 등에 행글라이더가 나타나고, 천천히 일정 속도로 내려가면서 질주 수준 속도로 수평 이동한다.
활공 중에 점프를 다시 누르면 접혀 일반 낙하로 돌아간다. 펼치고 접는 연출은 없고 메시가 바로 나타나고 사라진다.
자세는 따로 만들지 않았다 — 공중 공용 Fall_Loop 고정 포즈(TechDesign_CombatAnimation.md §5.5)이고, 상반신은 평소처럼 무기 레이어가 덮는다.

```
[공중 점프 입력] FCharacterDefaultInputs::bIsJumpJustPressed (InputCmd, 엔진 점프와 같은 필드)
    │  OnMoverPreSimulationTick — Super(엔진 점프) 앞
    ├ IsGliding()  → QueueNextMode(AirMovementModeName)   접기
    └ CanGlide()   → QueueNextMode("LNPGlide")            펼치기
[이동] ULNPGlideMode : UAsyncFallingMode — GenerateMove만 교체, 착지·충돌은 부모 그대로
[표현] 각 머신이 IsGliding()을 폴링 — GliderMesh 표시(ALNPPlayerCharacter::Tick), CR_Glide 리그(CDE_ThirdPerson)
```

### 10.1 활공은 별도 이동 모드다 — 낙하 모드를 상속한다

**모드 이름이 SyncState에 실리므로 활공 상태의 예측·복제·롤백에 추가 상태가 0이다.** 새 InputCmd 필드도,
복제 프로퍼티도, Modifier도 없다. 원격 폰의 행글라이더 표시도 모드에서 파생하면 저절로 맞는다.

| 판단 | 근거 |
|:---|:---|
| **`UAsyncFallingMode`를 상속**하고 `GenerateMove`만 교체 | `SimulationTick`(충돌 슬라이드·`ProcessLanded`)을 그대로 쓴다. 착지하면 엔진이 지상 모드로 전환하므로 **접는 코드 없이 접힌다** |
| 낙하 태그(`Mover_IsFalling`)를 물려받는다 | ABP가 공중 포즈를 그대로 쓰고, `CanDash()`·`CanSprint()`(접지 요구)가 저절로 막힌다 |
| 엔진 `UAsyncFlyingMode`를 쓰지 않는다 | 착지 전이가 없고 상하 입력을 받는다 — "천천히 떨어지는 공중 이동"과 맞지 않는다 |
| 조회는 모드 태그 `LNP.Mover.IsGliding` | `IsADS()`와 같은 `HasGameplayTag` 패턴. **`IsFalling()`만으로는 활공 중을 거를 수 없다**(태그를 물려받으므로) |

`GenerateMove`가 부모와 다른 점은 둘이다.

- **수평:** 입력을 `AirControlPercentage`로 줄이지 않고 `GlideSpeed`(1200 = 질주)·`GlideAcceleration`·`GlideDeceleration`을 쓴다.
  `CommonSettings->MaxSpeed`를 읽지 않으므로 `FLNPMoveSpeedModifier`(버프)·가드 속도의 영향을 받지 않는다.
- **수직:** 중력 Up 기준 목표 `-GlideDescentSpeed`(300cm/s)로 수렴한다. 목표보다 위(상승·느린 하강)면 **중력으로**,
  아래(빠른 하강)면 `GlideCatchDeceleration`으로 다가간다. 런처로 솟구치다 펼치면 평소처럼 오르다 꺾이고,
  **목표보다 위로 올리는 경로가 없어** 접었다 펼치기를 반복해도 고도를 벌 수 없다.

수치는 전부 모드 서브오브젝트의 UPROPERTY다(BP Mover 컴포넌트 → Movement Modes → `LNPGlide`).

### 10.2 토글은 엔진 점프 입력을 그대로 쓴다

`bIsJumpJustPressed`는 이미 InputCmd에 실리고, `OnProduceInput`이 한 번 내보낸 뒤 지우므로 **한 스텝에만 true**다 —
토글이 두 번 튀지 않는다. 엔진 점프(`Super::OnMoverPreSimulationTick`)는 `CanActorJump()`이 접지를 요구해 공중 입력을
버리므로, `Super` 앞에서 가로채도 겹치지 않는다. 지상 점프 프레임은 아직 접지 상태라 `CanGlide()`가 거부한다.

`QueueNextMode`는 상태 머신이 이번 스텝 시작에 소비한다(`OnPreSimulationTick` 브로드캐스트가 모드 틱보다 앞).
같은 InputCmd로 재시뮬하면 같은 결과가 나온다.

### 10.3 고도 판정 — 일반 점프로는 펼 수 없다

`CanGlide()` = `IsFalling() && !IsGliding() && !IsGrappleFlying() && !IsInDeadMode() && HasGlideClearance()`

`HasGlideClearance()`는 **스텝 시작 SyncState 위치**에서 `-GetUpDirection()`으로 `캡슐 반높이 + MinGlideHeight`만큼
라인 트레이스해 아무것도 없으면 true다.

- **원점은 컴포넌트가 아니라 스텝 시작 상태다** — §7.7·§7.8과 같은 이유(서브스텝·재시뮬에서 컴포넌트는 낡았다).
- **오브젝트 타입은 WorldStatic·WorldDynamic만.** 폰 채널을 넣으면 적 캡슐 위를 지날 때 판정이 적의 복제 위치에 따라 머신마다 갈린다.
- `MinGlideHeight` 기본 400cm. 점프 속도는 750cm/s를 유지한다. 점프 정점은 `JumpUpwardsSpeed² / (2 × 중력)` = 750² / 1960 ≈ **287cm**라 평지 점프로는
  도달하지 못한다(대시 점프도 수직 속도가 같다). 절벽에서 뛰어내리거나 런처로 날아오르면 펼 수 있다.
- 그래플 비행 중에는 열지 않는다. 비행이 끝나 낙하로 돌아오면 다시 펼 수 있다.

### 10.4 다른 이동과의 관계 — 추가 코드 0줄

| 사건 | 결과 | 경로 |
|:---|:---|:---|
| 착지 | 접힘 | 부모 `ProcessLanded`가 지상 모드로 전환 |
| 피격 넉백 | 접힘(기획 결정) | `FLNPApplyVelocityEffect::ForceMovementMode = Air` |
| 그래플 | 접힘 | Launch 무브의 `ForceMovementMode = Air` |
| 사망 | 접힘 | `EnterDeadMode()`가 모드 교체 |
| 런처 사출 후 점프 | 펼침 | 사출이 낙하 모드로 보내므로 고도만 맞으면 된다 |

### 10.5 전투 — 총기는 지향 사격만, 근접은 가드만

| 행동 | 활공 중 | 구현 |
|:---|:---|:---|
| 총기 지향 사격 | 가능 | 손대지 않음 |
| ADS | **불가** | 로컬 `ULNPInputHandlerComponent::IsADSActive()`에 `!IsGliding()`, 시뮬 `CanADS()`에 `!IsGliding()` |
| 근접 가드 | 가능 | 손대지 않음. 활공 속도는 가드 속도의 영향을 받지 않는다 |
| 근접 공격 | **불가** | `ALNPCharacterBase::TryActivateAttack()` 선두(경직 검사 옆)에서 FreeAim이 아니면 거부 |

- ADS 차단은 §2.6의 두 계층(카메라는 로컬 파생값, 이동 모디파이어는 시뮬레이션 `CanX()`)을 그대로 따른다.
  `IsADSActive()`가 순수 파생값이라 **착지 후 키를 누르고 있으면 ADS가 저절로 재개**되고, 활공에 들어가면
  `CanADS()`의 취소 분기가 ADS 모디파이어를 푼다. 카메라·감도·궤도 가이드·`bWantsToADS`가 모두 이 함수를 거친다.
- 근접 공격 차단은 콤보 창 분기보다 **앞**이어야 한다 — 경직 검사와 같은 이유(콤보 창 분기는 차단 태그를 보지 않는다).
  공격 발동 요청이 소유 클라이언트에서 출발하므로 로컬 게이트로 충분하다.

### 10.6 표현 — 메시와 카메라 리그

- **메시:** `ALNPPlayerCharacter::GliderMesh`(`UStaticMeshComponent`, 콜리전·그림자 없음)가 `VisualMesh` **원점**에
  항등 트랜스폼으로 붙는다. `SM_HangGlider`(`/Game/Characters/Glider`, 소스 `Art/Meshes/SM_HangGlider.fbx/.blend`)는
  **마네킹 메시 공간(발 원점, 전방 UE +Y)**에 맞춰 모델링했다 — 날개폭 약 180cm, 세일 높이 186~202cm, 등 스트럿.
  본에 붙이지 않은 이유는 상대 트랜스폼을 본 바인드 포즈로 역산할 필요가 없어서고, 낙하 포즈가 거의 직립이라
  등뼈를 따라가지 않아도 어긋나지 않는다. 표시는 `Tick`이 `IsGlidingActive()`를 폴링해 바뀔 때만 갱신한다.
- **카메라:** `CDE_ThirdPerson`이 `IsGlidingActive()` → `CR_Glide`, `IsADSActive()` → `CR_ADS`, 그 외 → `CR_Medium_FreeCam`
  순으로 고른다. 활공 중엔 ADS가 꺼지므로 두 리그가 겹치지 않는다.
  `CR_Glide`는 `CR_ADS`의 복제본으로 `CR_ThirdPerson` 래퍼의 오버라이드만 바꿨다 — 피벗(`BoomArmOffset`)을 190cm로 올려
  세일 너머로 내려다보고, 붐(`CameraOffset` −320)을 늘리고, FOV 85. 전환 블렌드는 ADS와 같다(0.15/0.22초).
  `LNPLobbedADSPitch` 노드가 함께 복제됐지만 `IsLobbedADSActive()`가 ADS를 요구해 활공 중엔 동작하지 않는다.

**검증 (2026-10-14, 1인·2인 PIE):** 평지 점프로는 펼쳐지지 않고 높은 곳에서만 펼쳐진다. `ShowSpeed`가 활공 중 `state Glide`,
수평 속도는 질주와 비슷하다. 총기 지향 사격·근접 가드는 동작하고 ADS는 진입하지 않는다. 행글라이더와 `CR_Glide`가
시야를 과하게 가리지 않는다. 2인 PIE 게스트가 `np.PrintReconciles 1`로 접었다 펴며 이동하는 동안 보정 캡슐 0건.

### 10.7 한계

- **MoveSpeed 버프 미반영.** 활공 속도는 고정 `GlideSpeed`다. 필요하면 `GenerateMove`에서 어트리뷰트 배율을 곱한다.
- **고도 판정은 중심선 한 줄이다.** 캡슐 가장자리 아래의 턱은 보지 않는다 — 펼치자마자 착지할 뿐 문제는 없다.
- **활공 전용 자세가 없다.** 공중 공용 Fall_Loop를 쓴다(모션 매칭은 공중에서 다리가 떨려 우회 — TechDesign_CombatAnimation.md §5.5). 행글라이더를 잡은 자세가 필요하면 그 Bool 분기 앞에 활공 분기를 하나 더 둔다.
- **넉백 미예측(§8)과 같은 이유로**, 활공 중 피격 시 해제는 피격자 화면에서 RTT만큼 늦다.

### 10.8 윈드필드 — 즉시 전개와 상승 (2026-10-15)

윈드필드(`ALNPWindField`, [TechDesign_WorldDevice.md](TechDesign_WorldDevice.md) §10) 안에서는 점프 입력 즉시 행글라이더가 펼쳐지고,
떨어지지 않고 필드의 목표 고도까지 올라가 머문다. 상승 중에도, 도달한 뒤에도 수평 이동은 평소 활공과 같다.

**추가 예측 상태는 여전히 0이다.** 윈드필드는 모드가 아니라 `GenerateMove`·`CanGlide`가 **스텝 시작 SyncState 위치**로 묻는 월드 데이터다
(`ULNPWindFieldSubsystem::FindWindLift` — 물리 오버랩이 아닌 기하 판정이라 재시뮬레이션에서 결정적이다).

| 사건 | 필드 밖 | 필드 안 | 구현 |
|:---|:---|:---|:---|
| 지상 점프 | 일반 점프 | **점프 없이 즉시 전개** | `CanActorJump()` 오버라이드가 필드 안에서 false → 엔진 점프가 안 나가고 같은 `bIsJumpJustPressed`를 §10.2의 글라이드 분기가 소비 |
| 공중 점프 | 발밑 여유 400cm를 넘을 때만 전개 | 고도 무관 즉시 전개 | `CanGlide()`가 필드 안이면 `HasGlideClearance()`·`IsFalling()` 요구를 풀고 지상도 허용 |
| 활공 중 점프 | 접힘 | 접힘(기존 규칙 유지) | 변경 없음 |
| 활공 수직 | 하강 300cm/s 수렴 | 목표 고도까지 상승 후 호버 | `ULNPGlideMode::GenerateMove` 수직 분기 |
| 필드가 꺼짐·벗어남 | — | 행글라이더 유지, 일반 활공으로 하강 | 다음 스텝 조회가 실패해 기존 분기로 자연 복귀 |

**상승 곡선:** 목표 수직 속력 = `Clamp(남은 높이 / WindSettleTime, ±WindRiseSpeed)`, 현재 속도에서 `WindLiftAcceleration`으로 다가간다.
멀리서는 `WindRiseSpeed`(800cm/s)로 오르고 가까워질수록 느려져 오버슛 없이 멈춘다. 목표보다 아래로 내려가 있으면 다시 올라온다.
남은 높이는 `Dot(목표 셸 위 같은 반경선의 점 − 위치, Up)`이라 내벽/외벽 부호를 하드코딩하지 않는다. 수치는 활공 모드 서브오브젝트의
`LNP|Glide|WindField` 카테고리 UPROPERTY다 — 필드별로는 높이만 다르다.

**지상 이륙:** 낙하 모드는 이동 스윕이 바닥에 막힐 때만 착지하므로(`UAsyncFallingMode::ProcessLanded`) 첫 스텝부터 위로 움직이면 바로 재착지하지 않는다.

**한계:** 필드 on/off는 서버 권위 복제 상태라 토글 순간 필드 안 클라에 보정이 한 번 날 수 있다(수용).

**검증 (2026-10-15, 1인·2인 PIE, TestMap03 + `LNP.WindField.SpawnAtPlayer 1500 2000`):** 필드 안 지상 점프로 즉시 전개되고 20m 올라가 멈춘다.
상승 중과 정지 후 모두 WASD로 이동된다. 접으면 자유 낙하하고 다시 펴면 상승한다. 활공 중 필드 밖으로 나가면 하강하고 다시 들어오면 상승한다.
`SetEnabled 0/1`로 동작과 VFX가 꺼졌다 켜진다. 2인 PIE 게스트 `np.PrintReconciles 1`로 이륙·호버·이동을 반복하는 동안 보정 캡슐 0건.
같은 세션의 `LogNetworkPrediction: ... input stream starvation`은 윈드필드 이전 2인 PIE 로그에도 있던 PIE 환경 경고다. 보정과는 겹치지 않았다.
