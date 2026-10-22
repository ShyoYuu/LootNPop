# 물리 히트리액트 스파이크 결과 (Phase 1A)

> 상태: 완료(2026-10-18, Phase 3 근거). §7 방향성 비교는 Phase 3-1(2026-10-21)
> 마지막 갱신: 2026-10-21
> 소유 정보: 방식 선택 근거, 스파이크 구성·파라미터, 검증 결과, HitStop 상호작용, 실패 지점, Phase 3 인계, 방향성 비교 결과(§7). 실행 계획·체크리스트는 `phases/Phase01A_PhysicsHitReactSpike.md`, 날짜별 경위는 `history/Phase01A_Log.md`.

## 1. 방식 선택 — RigidBodyWithControl(RBWC) 노드

**결론: `VisualMesh` ABP의 마지막(리타깃 노드 뒤)에 RBWC 노드를 둔다. PhysicsControl 컴포넌트는 쓰지 않는다.**

### 1.1 포즈 경로와의 순서 (판단 기준)

현재 `VisualMesh`는 `ABP_Retarget`(`RetargetPoseFromMesh` → Output)로 `AnimSourceMesh` 포즈를 받는다. 컴포넌트 태그(`RTG_Lyra_to_MS` 등)로 리타기터를 고르고, 플레이어·적이 같은 ABP를 공유한다.

- **RBWC 노드**는 `RetargetPoseFromMesh` 바로 뒤에서 같은 프레임의 리타깃 포즈를 목표로 시뮬레이션하고, 그 출력이 곧 `VisualMesh`의 최종 포즈다. 목표와 결과 사이에 프레임 지연이 없다.
- **PhysicsControl 컴포넌트**는 `VisualMesh`의 물리 씬 바디를 움직인다. 애님 결과를 게임 스레드에서 목표로 읽고, 비동기 물리 틱(`bTickPhysicsAsync`)이 시뮬레이션하고, 결과는 `bBlendPhysics`로 애님 포즈를 덮는다. 목표 읽기·시뮬·반영이 세 시점으로 갈린다.

### 1.2 사망 랙돌과의 충돌

- 랙돌(`EnterRagdoll`)은 `VisualMesh`의 물리 씬 바디를 쓴다. 평소 `VisualMesh`는 `NoCollision`이라 바디가 아예 없고, 진입 때 프로필 `Ragdoll`로 바디를 만들고 `ExitRagdoll`이 `NoCollision`으로 되돌려 바디를 파괴한다. CCD를 되돌리지 않는 근거도 이 파괴에 기대고 있다([TechDesign_CharacterMovement.md](../../TechDesign_CharacterMovement.md) §9.1).
- 컴포넌트 방식은 평소에도 바디가 있어야 하므로 이 전제를 깬다. 바디 모디파이어가 랙돌의 `SetSimulatePhysics(true)`와 같은 바디를 두고 다툰다.
- RBWC는 자체 Immediate Physics 시뮬레이션이라 물리 씬 바디와 분리된다. 랙돌 동안은 `bBlendPhysics`가 포즈를 덮으므로 노드 출력이 무시되고, 스파이크에서는 노드 알파를 0으로 내려 비용도 없앤다.

### 1.3 구면 중력

- RBWC 기본 중력은 `MovementComp->GetGravityZ()`(없으면 `World->GetGravityZ()`)로 만든 **월드 -Z**다(`AnimNode_RigidBodyWithControl.cpp` `PreUpdate`). Mover는 `UPawnMovementComponent`가 아니라 항상 월드 -Z가 된다. 적도에서는 옆으로 처진다.
- 해법: 모디파이어 `GravityMultiplier=0`(바디별 중력 끔) + `bOverrideWorldGravity=true`·`(0,0,0)`. GASP `PCA_SandboxCharacter`의 `physicalAnimation` 프로필도 `GravityMultiplier=0`이다. 상체는 부모 공간 모터가 애니 포즈로 끌어당기므로 짧은 히트리액트에 중력이 없어도 어색하지 않다.
- 중력감이 필요해지면 `OverrideWorldGravity`(월드 공간 벡터, 노드가 시뮬 공간으로 변환)를 핀으로 노출해 `-Up × g`를 매 프레임 공급한다(`TickRagdollGravity`와 같은 식).

### 1.4 HitStop

`ApplyHitStop`은 액터 `CustomTimeDilation`을 낮춘다. RBWC는 애님 `DeltaSeconds`(시간 팽창 적용)로 시뮬레이션하므로 몽타주와 함께 멈췄다가 함께 풀린다. 컴포넌트 방식은 월드 시간으로 도는 물리 씬이라 몽타주만 멈추고 상체는 계속 흔들린다. 관찰 결과는 §4.

### 1.5 AN-011(UEFN 전환) 이후에도 유지되는가

유지된다. 2-0b 뒤 `AnimSourceMesh`는 UEFN이 되고 `VisualMesh`는 UEFN → 캐릭터 메시 리타기터로 포즈를 받는다. 바뀌는 것은 리타깃 노드의 원본·리타기터뿐이고, RBWC는 여전히 그 뒤에서 `VisualMesh` 자신의 스켈레톤·PhysicsAsset으로 돈다.

⚠️ **2-0b 제약:** UEFN 메시 적은 "같은 스켈레톤이라 포즈 복사"로 계획돼 있다. 이를 `SetLeaderPoseComponent`(Leader Pose)로 구현하면 `VisualMesh`가 ABP를 돌리지 않아 RBWC를 둘 곳이 없다. **ABP 안의 `Copy Pose From Mesh` 노드**로 복사해야 Phase 3에서 적에게도 같은 노드를 쓸 수 있다. → 2-0b에서 `ABP_VisualCopyPose`(Copy Pose From Mesh → 출력)로 반영했다(2026-10-18). RBWC는 그 뒤에 이어지는 왼손 IK 다음, 출력 앞에 넣는다.

### 1.6 복제

플러그인 소스에 복제 코드가 없다(`research/UE58_AnimationFeatures.md` §2). 애님 노드는 머신마다 로컬로 돈다. 트리거도 머신별 로컬 GameplayCue(`GameplayCue.LNP.Character.HitReact`)라 AN-004와 일치한다.

## 2. 스파이크 구성

### 2.1 코드 (`Source/LootNPop/`)

| 위치 | 내용 |
|:---|:---|
| `Animation/LNPPhysicalHitReactAnimInstance` | `VisualMesh`용 애님 인스턴스. RBWC 핀 두 개를 공급한다. `HitForces`(`FRigidBodyForcesAndTorques`, 예약이 있던 업데이트 한 번만 채움)와 `PhysicsAlpha`(평소 0, 피격 시 1 → 유지 → 페이드, 랙돌 중 0)다. `QueueHitImpulse(속도 변화)`가 같은 프레임의 피격을 합산한다 |
| `Character/LNPCharacterBase.cpp` `PlayHitReact` | 기존 몽타주 재생 뒤, `VisualMesh` 애님 인스턴스가 위 클래스일 때만 `QueueHitImpulse`를 호출한다. 방향은 `-HitFromWorldDir`을 캐릭터 Up의 접평면에 투영한 것이다. 다른 ABP면 캐스트가 실패해 no-op이다 |
| 같은 파일 `BeginPlay` | CVar `LNP.Anim.PhysHitReactSpikeABP`(빈 값=끔)가 있으면 `VisualMesh` 애님 클래스를 그 경로로 바꾼다. 플레이어·승격 적 모두 대상이다 |
| `LootNPop.Build.cs`·`.uproject` | `PhysicsControl` 모듈 의존과 플러그인을 명시했다. 원래는 기본 활성 `ControlRigModules`를 경유해서만 로드되고 있었다 |

CVar(모두 스파이크용, 런타임 조정 가능):

| CVar | 기본값 | 의미 |
|:---|---:|:---|
| `LNP.Anim.PhysHitReactSpikeABP` | (빈 값) | PIE **시작 전에** 설정한다. 예: `/Game/Characters/PhysHitReactSpike/ABP_Retarget_PhysHitSpike.ABP_Retarget_PhysHitSpike_C` |
| `LNP.Anim.PhysHitReactImpulse` | 250 | 상체 바디 속도 변화(cm/s) |
| `LNP.Anim.PhysHitReactHoldSeconds` | 0.15 | 피격 후 알파 1 유지 시간 |
| `LNP.Anim.PhysHitReactFadeSeconds` | 0.35 | 알파 1 → 0 페이드 시간 |

### 2.2 에셋 `/Game/Characters/PhysHitReactSpike/ABP_Retarget_PhysHitSpike`

> 3-1(2026-10-21)에서 노드를 두 표시 ABP로 옮기고 이 사본 폴더와 스파이크 CVar `LNP.Anim.PhysHitReactSpikeABP`를 지웠다. 아래 노드 값은 지금 두 표시 ABP의 RBWC 값과 같다(필드 차이 0).

`ABP_Retarget` 복제본이고, 부모는 위 C++ 클래스다. 원본 `ABP_Retarget`은 건드리지 않았다(2-0b와 같은 에셋). AnimGraph는 `RetargetPoseFromMesh → LocalToComponent → RigidBodyWithControl → ComponentToLocal → Output`이다. RBWC는 컴포넌트 공간 포즈만 받으므로 변환 노드 두 개가 필요하다. `Alpha` 핀에는 `PhysicsAlpha`, `ForcesAndTorques` 핀에는 `HitForces`를 연결했다.

RBWC 노드 값(Phase 3 출발점):

| 항목 | 값 | 비고 |
|:---|:---|:---|
| `bEnableControls` | true | **기본값이 false다.** 켜지 않으면 컨트롤이 하나도 만들어지지 않는다 |
| 시뮬레이션 공간 | ComponentSpace | `ComponentLinearAccScale`·`SimSpaceSettings.WorldAlpha` 0 → 캡슐 이동·순간이동이 시뮬에 들어가지 않는다 |
| 월드 지오메트리 충돌 | 끔 | 코스메틱. 바디 간 충돌도 모디파이어 `NoCollision` |
| 중력 | `bOverrideWorldGravity`=true·(0,0,0), 모디파이어 `GravityMultiplier`=0 | §1.3 |
| 림 | Torso(pelvis)·Head(neck_01)·Arm_L/R(clavicle)·Leg_L/R(thigh) | 부모 공간 컨트롤만 만든다(월드 공간 없음) |
| 부모 공간 컨트롤 | 각강도 **30Hz**, 감쇠비 1, 선형 0, 애니 목표(`bUseSkeletalAnimation`) | 강도 단위는 Hz다(`ConvertStrengthToSpringParams`: ω = 강도×2π) |
| 키네마틱 | `pelvis`·`thigh/calf/foot_l/r` 7개 | 초기 모디파이어 갱신에 **본 이름**으로 지정(§5.1) |
| 임펄스 대상 | `spine_03`·`spine_04`·`spine_05` | VelocityChange, 월드 공간, 바디 질량 중심 |

## 3. 검증 결과 (2026-10-18)

| 항목 | 결과 | 근거 |
|:---|:---|:---|
| `VisualMesh` 상체 임펄스 → 애니 포즈 복귀, 다리·골반 키네마틱 | 통과 | 사용자 2인 PIE: 호스트·게스트 모두 피격 때 상체가 움찔하고 돌아온다. 다리는 서 있다. 기존 히트 몽타주와 겹쳐도 어색하지 않다 |
| 승격 적(UEFN 메시·`PA_UEFN_Mannequin`) | 통과(약함) | 같은 노드 설정으로 반응한다. 플레이어보다 약하게 느껴진다(§6) |
| 적도 근처 처짐 없음 | 통과 | 사용자 확인. 중력 0이라 위도와 무관하다 |
| 2인 리슨 PIE 각 머신 로컬 재생 | 통과 | 호스트·게스트 화면 모두 반응. 트리거는 머신별 GameplayCue이고 노드는 비복제 |
| 캡슐·Mover·판정 무영향 | 통과 | 사용자: 튐·되감김 없음. 구조상 `VisualMesh` 포즈만 바꾸고 물리 씬 바디·Mover를 건드리지 않는다. 1차 세션의 `np.PrintReconciles` 70건은 입력 스트림 불일치(받은 값·로컬 값의 `MoveInput`부터 다름, 89프레임 오프셋)였고 스파이크와 무관하다고 판단했다. 켬/끔 비교는 하지 않았다 |
| 사망 랙돌 진입·리스폰 | 통과 | 사용자 확인 + 로그(호스트 사망 1회, `LNP.Debug.KillPlayer 1`). 리스폰 폰에도 스파이크 ABP가 다시 붙는다 |
| `ExitRagdoll`(적 표현 풀 재사용) | 부분 확인 | 로그상 `BP_LNPEnemy_C_0`이 NetID 910 → 2775로 다른 엔티티에 재배정됐다. 경고는 없었다. 적이 랙돌로 죽은 직후 재사용되는 장면은 화면으로 따로 보지 않았다 → Phase 3 확인 항목 |
| 경고 | 없음 | 재검증 세션에서 `LogPhysicsControl` 경고·ensure 0건 |

## 4. HitStop과의 상호작용

- 공격자 HitStop(본인 0.2초)은 그대로 적용되는 것을 확인했다.
- RBWC는 애님 `DeltaSeconds`로 돈다. 그래서 피격자 HitStop(0.08초, `CustomTimeDilation` 0.1) 동안은 시뮬레이션도 몽타주와 함께 느려진다. 알파 유지·페이드 타이머도 같은 `DeltaSeconds`로 줄어 HitStop만큼 늘어난다. 튐이나 어긋남은 보고되지 않았다.
- 피격 순간 시간 팽창이 걸린 상태에서 VelocityChange가 들어가므로, 실제 움직임은 HitStop이 풀린 뒤에 보인다. 몽타주와 같은 박자라 자연스럽다는 판단이다. 대신 HitStop을 길게 바꾸면 반응이 늦게 보일 수 있다.

## 5. 실패 지점과 함정

### 5.1 초기 모디파이어 갱신은 림(세트) 이름을 풀지 않는다

`InitialControlAndBodyModifierUpdates.ModifierParameters`에 `Leg_L`·`Leg_R`를 넣으면 `ApplyControlAndBodyModifierDatas: Failed to find modifier with name Leg_L` 경고가 나고 무시된다. 본 이름(`pelvis`, `thigh_l` …)만 통한다. 다리가 시뮬레이션으로 남아 흔들렸다. 런타임 `ForcesAndTorques`는 세트 이름을 펼친다(`GetBodyModifierNamesInSet`). 두 경로의 동작이 다르다.

### 5.2 알파를 상시 1로 두면 "해파리"가 된다 (1차 PIE)

부모 공간 15Hz·중력 0·키네마틱 골반 구성을 상시 켜 두었더니, 피격과 무관하게 달리기·점프·그래플 가속마다 상체·팔이 출렁였다. 컨트롤은 정상 생성돼 있었다(Verbose 로그로 확인). GASP `physicalAnimation` 프로필도 같은 값이고, 그쪽은 랙돌 기상용 느슨한 물리다. 해법은 피격 구간에만 알파를 올리는 것이다. 알파 0 동안 노드는 평가되지 않는다. 다시 올라가는 첫 평가에서 시간 기반 리셋(`EvaluationResetTime`, 기본 0.01초)이 바디를 현재 입력 포즈로 순간이동시킨 뒤 같은 평가에서 임펄스를 준다. 그래서 튐 없이 시작한다.

### 5.3 그 밖의 함정

- RBWC 기본 중력은 월드 -Z다(§1.3). 끄지 않으면 적도에서 옆으로 처진다.
- `bEnableControls` 기본값은 false다. 림 셋업을 넣어도 컨트롤이 생기지 않는다.
- RBWC 컨트롤 생성 결과는 `LogPhysicsControl` **Verbose**에서만 나온다(`LogControlsModifiersAndSets`). 의심될 때는 Verbose를 켜고 PIE 한 번으로 확인한다.
- MCP로 노드 배열(`modifierParameters`) 크기와 값을 한 번에 바꾸면 "ArrayAdd ambiguous"로 거부된다. 같은 크기로 값을 바꾼 뒤 크기를 늘린다.
- 신규 UCLASS·멤버 추가는 Live Coding을 쓸 수 없어 에디터 풀빌드·재시작이 두 번 필요했다. Phase 3에서 이 클래스를 고칠 때도 같다.

## 6. Phase 3 인계

1. **방식은 RBWC 노드로 확정한다.** `VisualMesh` ABP의 리타깃 노드 뒤에 둔다. 2-0b가 `ABP_Retarget`을 정리할 때 이 위치(변환 노드 2개 포함)를 남길 자리로 설계하고, UEFN 메시 적도 Leader Pose가 아니라 ABP 안의 포즈 복사를 쓴다(§1.5).
2. **알파는 피격 구간에만 켠다.** 상시 물리는 금지다(§5.2). 유지·페이드 값은 GASP B11 패턴처럼 히트리액트 몽타주의 애니 곡선으로 줄 수 있다. 몽타주를 대체할 때 곡선 → `PhysicsAlpha`·컨트롤 강도로 옮기면 무기·방향별로 다르게 줄 수 있다.
3. **방향성이 약하다.** 사용자 평가는 "움찔은 자연스럽지만 공격 반대쪽으로 밀리는 느낌은 부족"이다. 지금은 가슴 세 바디에 같은 선속도만 준다. 골반이 키네마틱이고 부모 공간 컨트롤이 각도만 잡기 때문에 선속도는 대부분 관절 구속에 흡수된다. 후보는 다음과 같다.
   - `Up × PushDir` 축 각속도(Angular VelocityChange)로 상체를 젖힌다
   - 피격 위치에 가장 가까운 바디에 `LocationSpace=World`로 지점 임펄스를 준다(히트 판정의 ImpactPoint 사용)
   - 페이드 동안 척추 강도를 잠깐 낮춘다(`ControlAndModifierParameters` 핀)
4. **적 반응이 약하게 느껴진다.** `PA_UEFN_Mannequin`은 `spine_01` 바디가 더 있고 질량·관절 제한이 MoverSample PA와 다르다. 메시별로 임펄스·강도를 나누거나 PhysicsControl 프로필 에셋(PCA)으로 분리하는 것을 검토한다. 피격 HitStop 차이(적은 Actor 공격자 경로) 여부도 함께 본다.
5. **남은 확인:** 승격 적이 랙돌로 죽은 직후 재사용되는 장면(§3), `np.PrintReconciles` 켬/끔 비교, URO·LOD로 평가가 건너뛰어질 때 1프레임 임펄스가 사라지는지.
6. **몽타주 대체 범위:** 경직(GA_Stagger)·다운은 몽타주 유지(AN-004). 가벼운 히트리액트 몽타주만 물리로 바꿀지, 몽타주 + 물리 덧붙임으로 갈지는 3의 방향성 개선 결과를 보고 정한다. 이번 스파이크에서는 덧붙임만으로도 어색하지 않았다.

## 7. 방향성 비교 결과 (Phase 3-1, 2026-10-21)

§6-3의 세 후보를 같은 노드 설정(30Hz 부모 공간, 골반·다리 키네마틱)에서 CVar `LNP.Anim.PhysHitReactMode` 비트로 바꿔 가며 사용자 PIE로 비교했다. 실행 경위는 `history/Phase03_Log.md` §4~6.

| 조건 | 본 것 | 평가 |
|:---|:---|:---|
| 몽타주 위 | 1(가슴 선속도)·2(`Up × PushDir` 각속도)·4(맞은 지점에 가장 가까운 바디 지점 임펄스)·9·10·12(+ 강도 낮추기) | 모두 자연스럽고 모드끼리 체감 차이가 작다. 방향별 차이로 느낀 것은 4방향 몽타주였다 |
| 물리 단독(비트 16) | 17·26·28, 과장값(강도 배율 0.1·젖힘 20rad/s·지점 800cm/s) | 가벼운 피격 몽타주를 대체할 만큼 움찔은 확실하다. 맞은 부위·방향을 뚜렷이 체감할 정도는 아니다 |

**선택: 모드 12(지점 임펄스 + 강도 낮추기), 강도 배율 0.1, 지점 임펄스 800cm/s.** 체감 차이가 작아 맞은 부위 정보(머리·팔·가슴 중 어디에 줄지)를 쓰는 후보를 골랐다. 선속도·젖힘 비트는 남겨 두었다.

왜 방향성이 약한가(3-2 근거):

- 30Hz 부모 공간 컨트롤은 ω₀ ≈ 188rad/s라, 임계 감쇠에서 최대 변위 ≈ v₀/(ω₀·e)다. 가슴 250cm/s는 관절당 1° 안팎이다. 강도를 낮추면 변위는 커지지만 "어느 쪽으로 밀렸는지"를 만드는 것은 **몸 전체의 이동**(골반이 밀리거나 반 걸음 물러남)인데, 골반·다리가 키네마틱이라 이 성분이 없다.
- `ParentSpace_Torso` 세트는 척추만이 아니라 팔·목·다리 컨트롤까지 담는다(pelvis에서 시작하는 Torso 림이 하위 전부를 가진다, `LogPhysicsControl` Verbose로 확인). 강도 낮추기는 사실상 온몸이 물러지는 것이다. 척추만 무르게 하려면 세트를 따로 만들어야 한다(`AdditionalSets`, 메시마다 `pelvis_spine_01`/`pelvis_spine_02`가 다르다).
- 지점 임펄스(`LocationSpace=World`, VelocityChange)는 Chaos `AddForceAtLocation`이 각속도까지 만든다. 바디 선택은 정상(머리·팔·가슴으로 갈림)이지만 체감은 다른 모드와 비슷했다.

경로·확인 결과: 큐가 맞은 지점(`Location`)·피해(`RawMagnitude`)·근접(`NormalizedMagnitude`)을 싣는다. 리슨 2인에서 보정 횟수는 물리 끔 84/분 → 켬 60/분으로 늘지 않았다(1A §6-5 "켬/끔 비교" 해소). HitStop·랙돌 진입·표현 풀 재사용·적도·근접 콤보·가드·패링 중 피격 모두 이상 없음. URO·LOD 1프레임 소실은 아직 보지 않았다(3-2).

## 8. 골반·척추 보강 비교 (Phase 3-2 첫 실험, 2026-10-22)

사용자가 물리 단독 모드 28/60/84/116을 비교한 뒤 **"116이 제일 나은거 같아"**라고 선택했다. 116은 지점 임펄스(4) + HitReact 몽타주 생략(16) + 골반 월드 오프셋(32) + 척추만 강도 낮추기(64)다. 골반 값은 기준 세기에서 이동 4cm·젖힘 8°·좌우 피격에 따른 비틀림 10°·정점 0.1초, 척추 강도 배율 0.1, 지점 임펄스 800cm/s다. 전체 모드 표·수식은 `phases/Phase03_PhysicsHitReact.md` 단위 3-2가 소유한다.

사용자 요청으로 최종 비교 때 경직 몽타주(Light·Heavy·Parried)까지 임시로 생략했다. 비교 종료 후 해당 임시 코드를 제거하고 Live Coding으로 원래 재생을 복구했다. 최종 구간 로그는 116 피격 60회·경직 몽타주 생략 4회·컨트롤 경고 0건이다. 첫 비교에서 없는 척추 컨트롤 이름이 경고를 내므로, 실제 PhysicsAsset의 바디 존재와 부모 관계를 확인해 유효한 이름만 공급하도록 수정했다.

후속으로 사용자가 기본 적용·몽타주 제거를 승인했다. 기본값은 **100**(116에서 몽타주 생략 비트 16 제거)으로 바꾸고 HitReact 몽타주 호출과 Chooser HitReaction 10행을 제거했다. 일반 피격·경직 몽타주·공격/재장전 왼손 그립 사용자 PIE 확인도 통과했다. 2026-10-23에는 AN-024의 플레이어 근접 공격 중 억제를 추가하고, 적도·리슨 2인·골반/다리·랙돌/LOD 재접근·리스폰 최종 수동 회귀도 사용자 "모두 정상"으로 통과했다(증거·한계는 `history/Phase03_Log.md`). URO·RBWC LOD 설정 확인 결과는 §9.

## 9. URO·RBWC LOD 평가 생략 조건 확인 (2026-10-22)

LootNPop MCP로 실제 에셋 기본값과 전역 CVar를 읽었다. 에셋·C++은 수정하지 않았다.

| 항목 | 플레이어 | ActorPromoted 적 |
|:---|:---|:---|
| VisualMesh `bEnableUpdateRateOptimizations` | false | false |
| VisualMesh `VisibilityBasedAnimTickOption` | AlwaysTickPoseAndRefreshBones | AlwaysTickPoseAndRefreshBones |
| 표시 ABP의 RBWC `LODThreshold` | −1 | −1 |
| 전역 `p.RigidBodyWithControlLODThreshold` | −1(노드 값 사용) | 동일 |

엔진 근거(`Engine/` 상대 경로):

- `Source/Runtime/Engine/Private/Components/SkeletalMeshComponent.cpp`의 `TickPose`·`RefreshBoneTransforms`: URO를 사용할 때 업데이트·평가 생략 여부를 각각 판단한다. URO를 사용하지 않으면 이 평가 생략 분기를 쓰지 않는다.
- `Source/Runtime/Engine/Private/Components/SkinnedMeshComponent.cpp`의 `AnimUpdateRateSetParams`: 화면 밖의 인간 조작 캐릭터 등은 업데이트 빈도와 평가 빈도가 다를 수 있다. 일반적인 보이는 AI의 URO는 두 빈도를 함께 낮춘다.
- `Plugins/Animation/PhysicsControl/Source/PhysicsControl/Private/AnimNode_RigidBodyWithControl.cpp`: `UpdateInternal`은 시간을 누적하고, `EvaluateSkeletalControl_AnyThread`가 `ApplyForcesAndTorques`를 호출한다. `GetLODThreshold`는 노드 값과 전역 값으로 평가 제한을 정한다. 메시 LOD 변경 시에는 `InitializeBoneReferences`가 활성 바디를 갱신한다.

**현재 설정에서는 URO·RBWC LOD 제한에 의한 평가 생략이 활성화돼 있지 않다.** 따라서 `NativeUpdateAnimation` 한 번에만 힘을 채우는 기존 코드를 유지한다. 아직 발생 조건을 쓰지 않는 프로젝트에 임펄스 보류·소비 확인 로직을 추가하지 않는다.

이 확인은 **URO를 켠 상태의 런타임 통과 증거가 아니다.** 추후 URO·화면 밖 본 평가 생략·RBWC LOD 제한을 도입하면 업데이트 뒤 평가가 생략되는 피격을 재현하고, 평가까지 보류할지 화면 밖 코스메틱을 버릴지 정해야 한다. 메시 LOD별 바디 축소의 체감·표현 풀 재사용은 사용자 PIE 회귀로 따로 확인한다.

## 10. 실제 피격 시뮬레이션 바디·포즈 실측 결과 (2026-10-23)

사용자가 공격 중 피격 물리의 차이가 미묘하다며 수치 증명을 요청했고, 물리로 움직일 수 있는 부위도 물었다. MCP로 두 표시 메시의 PhysicsAsset 바디 목록과 RBWC 노드 설정을 읽었다. PhysicsAsset·ABP는 수정하지 않았다.

| 부위 | 플레이어 `PA_Mannequin` | NPC `PA_UEFN_Mannequin` |
|:---|:---|:---|
| 직접 시뮬레이션: 척추 | `spine_02`~`spine_05`(4개) | `spine_01`~`spine_05`(5개) |
| 직접 시뮬레이션: 목·머리 | `neck_01`, `neck_02`, `head` | 동일 |
| 직접 시뮬레이션: 양쪽 어깨·팔·손 | `clavicle_l/r`, `upperarm_l/r`, `lowerarm_l/r`, `hand_l/r`(8개) | 동일 |
| 키네마틱: 골반 | `pelvis`, 피격 타깃 오프셋으로만 이동·회전 | 동일 |
| 키네마틱: 양쪽 다리·발 | `thigh_l/r`, `calf_l/r`, `foot_l/r`(6개), 애니 포즈 유지 | 동일 |
| 전체 바디 / 직접 시뮬레이션 허용 바디 | 22 / 15 | 23 / 16 |

RBWC 기본 모디파이어는 `Simulated`, 초기 갱신은 골반·다리/발 7개를 `Kinematic`으로 바꾼다. 위 수는 그 설정과 현재 바디 목록의 교집합이며 일반 피격 경로 기준이다(LOD별 본 축소·사망 랙돌 제외). 임펄스를 직접 받는 후보는 `spine_03/04/05`, `head`, `upperarm_l/r` 중 피격 지점에 가장 가까운 하나다. 나머지 상체 바디는 관절을 통해 반응한다. 손가락 등 별도 바디가 없는 본은 부모 변화를 따라가며 독립 물리 바디는 없다.

**검증 당시 측정 구현(실측 후 제거).** 에디터 전용 CVar `LNP.Anim.PhysHitReactMeasure`로 `NativePostEvaluateAnimation`에서 골반·`spine_04`·머리·양손·양발의 같은 평가 입력/출력 컴포넌트 공간 변위(cm)·회전차(도)를 `[PhysHitMeasure]`로 남겼다. 월드·Actor·Player/NPC·프레임·메인 몽타주/시각·RBWC `ActualAlpha`도 기록해 머신·공격 상태를 구분했다. 이 일회성 CVar·훅은 최종 빌드에서 제거했으므로 현재 명령으로 사용할 수 없다. 기본 꺼짐인 `[PhysHitReact]` 공급/억제 로그만 유지하며 이를 실제 변형량의 증거로 혼동하지 않는다.

엔진 근거: `AnimNode_SkeletalControlBase.cpp`가 `ForwardedPose`를 알파 적용 전 복사한다(0일 때도). `SkeletalMeshComponent.cpp`의 `SwapEvaluationContextBuffers`는 평가 출력을 Editable 배열에 놓고, `PostAnimEvaluation`은 버퍼 반전 전에 `NativePostEvaluateAnimation`을 호출한다. 따라서 현재 그래프처럼 RBWC 뒤에 공간 변환·출력만 있으면 같은 프레임의 입력/출력을 비교할 수 있다. Read 배열은 이전 프레임일 수 있어 쓰지 않는다.

검증 요청은 0/68/100 비교였고 실제 로그에는 100→68→100만 기록됐다. 모드 0 검증을 했다고 쓰지 않는다. 대신 같은 측정 경로의 `ActualAlpha=0` 샘플 10,550개 모두 7개 측정 본 차이가 0임을 대조군으로 확인했다.

추가 조정: 사용자 "가끔 골반이 크게 흔들리면 춤추는 것 같다" 요청으로 골반 이동·젖힘·비틀림 기본값을 각각 4→3cm·8→6°·10→7.5°로 25% 낮췄다. 정점 시간·상체 임펄스·척추 강도는 유지했다. 전체 빌드 성공 후 사용자 **"측정 완료, 골반 켠 100이 나음"** 판정으로 100을 유지한다.

사용자가 골반 완전 끔/켬 비교도 요청했다. 기존 `LNP.Anim.PhysHitReactMode` 비트 32로 이미 분리할 수 있으므로 새 토글은 추가하지 않는다. **68 = 지점 임펄스(4)+척추 조절(64), 100 = 68+골반(32)**. `68 ↔ 100`은 골반 성분만 다르고, 0은 물리 전체 끔 대조군이다. CVar 도움말에도 비교 값을 명시했다. 사용자 에디터 종료 확인 후 전체 빌드 성공(35.32초), 아래 실측·선호 판정도 통과했다.

### 실측 수치

원본은 `Saved/Logs/LootNPop.log`(한국 2026-10-23 01:55:31 시작)이며 `Saved/AnimOverhaul/Phase3-2/evidence/physics_measure_2026-10-23.log`로 보존했다. 1인 PIE `TestMap03`, 수집 01:58:12~02:01:05, 총 13,016개 평가 샘플이다. 집계 산출물 `Saved/AnimOverhaul/Phase3-2/physics_measure_summary_2026-10-23.json`, 스크립트 `AnalyzePhysicsMeasure.py`(같은 폴더). 보존 원본을 인수로 다시 집계했고 JSON에 그 경로·SHA-256·모드 전환 행·조건별 전체 통계를 남겼다. 최초 모드는 코드 기본값 100이며 최초 힘 로그도 100이다.

| 조건 | 평가 샘플 | 가슴 위치 차이 평균 / 최대(cm) | 가슴 회전 차이 평균 / 최대(°) | 비고 |
|:---|---:|---:|---:|:---|
| 100 플레이어 근접 공격 | 1,043 | 0 / 0 | 0 / 0 | 실제 알파와 7개 측정 본 차이 모두 0 |
| 68 플레이어 근접 공격 | 536 | 0 / 0 | 0 / 0 | 동일 |
| 100 NPC 근접 공격·알파>0 | 313 | 2.7308 / 11.0548 | 19.4093 / 69.5609 | 262샘플에서 측정 본 중 하나 이상 차이>0.001cm/° |
| 68 NPC 근접 공격·알파>0 | 106 | 2.3567 / 8.9348 | 16.4546 / 90.794 | 골반은 0, 상체 반응 유지 |
| 100 플레이어 근접 공격 밖·알파>0 | 1,287 | 2.4965 / 9.9806 | 15.4812 / 40.6569 | 공격 밖 반응도 실제 출력에 남음 |

- 68의 전체 4,974샘플은 골반 위치·회전 차이 모두 0. 100의 전체 8,042샘플에서 골반 최대 2.475cm·7.9223°다. 해당 실행의 힘 기록 104회는 모두 세기 0.83이었다. 따라서 이 최대값을 세기 상한(근접 최대 3)의 결과라고 일반화하지 않는다.
- 100 NPC 공격·활성의 오른손 차이는 평균 21.176cm·최대 95.2793cm(P95 67.7977cm)로 클 수 있다. 이는 **같은 순간의 원본 공격 포즈 대비 RBWC 전체 기여분**이며, 임펄스뿐 아니라 모터가 빠른 공격 포즈를 따라가는 지연·골반 오프셋도 포함한다. 이 값을 순수 피격 힘만의 이동량으로 설명하지 않는다. NPC 공격 무기 궤적도 그 영향을 받을 수 있다는 AN-024 예외와 일치한다. 플레이어 근접 공격 중 오른손 차이는 전 샘플 0이다.
- 100 NPC 공격·활성의 키네마틱 발에는 최대 0.026cm 수준 위치 잔차가 있었고 회전 차이는 0이다. 68과 플레이어 근접 공격 구간은 발 차이도 0이었다.
- 모드 간 피격 지점·시각을 통제한 반복 실험은 아니므로 평균 차이를 골반 하나의 인과 효과로 정량 비교하지 않는다. 샘플은 프레임별 평가 기록이며 독립 피격 횟수가 아니다. NPC는 피격 반응 구간에만 알파가 올라가므로 공격 중 알파 0 샘플이 많은 것도 정상이다.
- PhysicsControl Warning/Error·컨트롤 누락·ensure 0건. 이번 1인 실행의 Mover 0-Up 경고도 0건이나 이전 2인 경고가 해결된 것으로 보지 않는다. 사용자 선호 100 유지, 일회성 진단 CVar는 최종 정리에서 제거했다.

### 실측 종료 후 정리

사용자 요청으로 Phase 3 종료 정리까지 마친 뒤 한 번에 커밋한다. 매 평가 로그·전용 CVar·평가 훅·전용 include는 제거하고 기존 기본 꺼짐 피격/억제 로그와 68/100 비교만 유지했다. 제거 전 소스·헤더는 `evidence/LNPPhysicalHitReactAnimInstance_measurement.cpp`·`.h`로 보존했다. 재측정이 필요하면 이 사본과 위 엔진 근거로 진단만 복원해 빌드한다. 최종 런타임에는 해당 계측이 없다.

리슨 2인 원본도 `evidence/listen_regression_2026-10-23.log`로 보존했다. `Saved` 증거·집계·진단 사본은 로컬 보관이며 git 커밋에는 넣지 않는다. 검증 결과·방법·한계는 이 문서와 Phase 3 로그에 남겨 저장소에서도 확인할 수 있다.

