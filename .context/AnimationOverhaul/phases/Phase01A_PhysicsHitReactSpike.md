# Phase 1A — 물리 히트리액트 스파이크

> 상태: 완료(2026-10-18)
> 선행: Phase 0 완료. Phase 1C·1B와 독립(병렬 가능)
> 후행: Phase 3(물리 히트리액트 본 구현)이 이 결과를 근거로 쓴다. 결과 원본은 `research/PhysicsHitReactSpike.md`
> 범위: 1세션. 기존 몽타주 경로(`LNPGameplayCueNotify_HitReact` → `PlayHitReact`)는 그대로 두고 그 위에 물리 반응을 **덧붙여** 본다(대체는 Phase 3)

## 목표

테스트 대상 캐릭터의 `VisualMesh` 상체에 피격 임펄스를 주고, 부모 공간 모터로 애니 포즈에 복귀시킨다. 다리·골반은 키네마틱이다. 구면 월드·2인 리슨·사망 랙돌·표현 풀 재사용에서 문제가 없는지 확인하고, Phase 3이 쓸 방식·파라미터·실패 지점을 남긴다.

## 방식 선택 — RigidBodyWithControl 노드 (결정 근거는 research 문서 §1)

| 기준 | PhysicsControl 컴포넌트 | RBWC 노드(`ABP_Retarget` 계열 마지막) |
|:---|:---|:---|
| 포즈 경로와의 순서 | 물리 씬 바디가 애님 결과를 목표로 따라가고, 결과는 `bBlendPhysics`로 덮는다. 목표 읽기·적용이 게임 스레드 틱·비동기 물리 틱에 걸쳐 갈린다 | `RetargetPoseFromMesh` 바로 뒤에서 같은 프레임의 리타깃 포즈를 목표로 쓰고, 결과가 곧 최종 포즈다 |
| 사망 랙돌과의 관계 | 랙돌과 **같은 물리 씬 바디**를 쓴다. `VisualMesh`가 `NoCollision`이라 바디가 없고, 쓰려면 프로필을 상시 바꿔야 한다 → `EnterRagdoll`/`ExitRagdoll`의 프로필·CCD 전제(TechDesign_CharacterMovement §9.1)가 깨진다 | 자체 Immediate Physics 시뮬레이션이라 물리 씬 바디와 분리된다. 랙돌 동안은 `bBlendPhysics`가 덮으므로 노드 출력이 무시된다 |
| 구면 중력 | 바디별 `SetEnableGravity(false)` + 별도 가속 | 모디파이어 `GravityMultiplier=0`(+`OverrideWorldGravity`)으로 노드 안에서 끝난다 |
| HitStop(`CustomTimeDilation`) | 물리 씬은 월드 시간으로 돈다 → 몽타주는 멈추고 상체만 계속 흔들린다 | 애님 DeltaSeconds를 쓰므로 몽타주와 같이 멈춘다 |
| 복제 | 플러그인에 복제 코드 없음 | 동일(애님은 머신별 로컬) |
| AN-011 이후(`AnimSourceMesh`=UEFN, `VisualMesh`=리타기터) | 바디가 `VisualMesh` 소유라 유지 | 리타깃 노드 뒤라는 위치가 그대로라 유지. **단 UEFN 메시 적을 Leader Pose로 바꾸면 ABP가 돌지 않아 노드가 사라진다** → 2-0b는 ABP 안의 포즈 복사를 써야 한다 |

## 실행 계획

1. **C++ 스파이크 훅** (신규 UCLASS → 에디터 재시작 필요, 착수 전 마스터 세션에 알림)
   - `ULNPPhysicalHitReactAnimInstance`(`Animation/`): RBWC 핀에 물릴 `HitForces`(`FRigidBodyForcesAndTorques`, 1프레임 유효)와 `PhysicsAlpha`(평소 0, 피격 시 1 → 유지 → 페이드, 랙돌 중 0)를 공급한다. 처음에는 알파를 상시 1로 두었다가 이동만으로 상체가 출렁여(research §5.2) 피격 구간 방식으로 바꿨다. `QueueHitImpulse(PushWorldDir)`가 접평면 투영 방향으로 `spine_03~05` 바디에 VelocityChange를 예약한다.
   - `ALNPCharacterBase::PlayHitReact`: 기존 몽타주 재생 뒤, `VisualMesh` 애님 인스턴스가 위 클래스일 때만 `QueueHitImpulse`. 아니면 아무 일도 없다(기존 경로 무변경).
   - 테스트 전환 CVar `LNP.Anim.PhysHitReactSpikeABP`(문자열, 빈 값=끔): `BeginPlay`에서 `VisualMesh`의 애님 클래스를 스파이크 사본으로 바꾼다. 플레이어·승격 적 모두 대상(적 풀 재사용 확인용). 임펄스 크기 CVar `LNP.Anim.PhysHitReactImpulse`.
   - `LootNPop.Build.cs`에 `PhysicsControl` 의존, `.uproject`에 플러그인 명시(현재는 `ControlRigModules` 경유로만 로드됨).
2. **스파이크 ABP** `/Game/Characters/PhysHitReactSpike/ABP_Retarget_PhysHitSpike` — `ABP_Retarget` 복제본(원본은 손대지 않음, 2-0b 충돌 회피), 부모를 위 클래스로. `RetargetPoseFromMesh → RigidBodyWithControl → Output`.
   - 림: Torso(pelvis)·Head(neck_01)·Arm_L/R(clavicle)·Leg_L/R(thigh), 부모 공간 컨트롤만 생성(월드 공간 없음).
   - 초기 갱신: 전 바디 Simulated·`GravityMultiplier=0`, `pelvis`·`thigh/calf/foot_l/r` Kinematic(림 이름은 풀리지 않아 본 이름으로 지정, research §5.1), 부모 공간 각강도 30Hz·감쇠비 1(GASP `physicalAnimation`의 15Hz에서 올림).
   - 시뮬레이션 공간 Component, 월드 지오메트리 충돌 끔, `bOverrideWorldGravity=true`·0.
3. **구면 중력** — 시뮬 바디 중력 0(모디파이어)으로 처리하고, 처짐이 보일 때만 `-Up×g` 바인딩(`TickRagdollGravity` 패턴)을 추가한다.
4. **검증(사용자 PIE, 묶음 요청)** — 아래 완료 조건 항목을 1~2회 측정으로 묶는다.
5. **기록** — `research/PhysicsHitReactSpike.md`(방식·파라미터·HitStop 관찰·실패 지점·Phase 3 인계), `history/Phase01A_Log.md`, `Roadmap.md`·`Current.md` 1A 행.

## 건드리지 않는 것 (병렬 세션 1C-1)

- `Content/Characters/GASP_Mannequin/` 하위, `BP_LNPEnemy`·적 DA·ASTP·`Retargeters/*`, `ABP_Retarget` 원본.
- `PlayHitReact`의 몽타주·`ApplyHitStop` 동작.

## 완료 조건

- [x] 방식 선택과 근거(AN-011 이후 유지 여부 포함)를 research 문서에 기록 — RBWC 노드(`research/PhysicsHitReactSpike.md` §1)
- [x] 테스트 캐릭터 `VisualMesh` 상체에 PhysicsControl(RBWC) 임펄스가 보이고 애니 포즈로 복귀한다. 다리·골반은 키네마틱
- [x] 적도 근처에서 상체 처짐이 없다
- [x] 2인 리슨 PIE에서 각 머신이 로컬로 재생한다(비복제, AN-004)
- [x] 캡슐·Mover·판정에 영향이 없다(튐·되감김 없음, 구조상 물리 씬 바디·Mover 미사용)
- [x] 사망 랙돌 진입·리스폰 뒤 정상 동작. 적 풀 재사용은 로그로만 확인(화면 확인은 Phase 3, research §3)
- [x] HitStop과의 상호작용 관찰 기록(research §4)
- [x] C++ 변경 커밋 뒤 깨끗한 트리에서 `Build.bat` 재확인
- [x] 임시 에셋·코드 정리 또는 존치 사유 기록(아래)

## 존치하는 스파이크 산출물과 사유

- `ULNPPhysicalHitReactAnimInstance`, `PlayHitReact`의 임펄스 훅, CVar 4종: 기본 상태(CVar 빈 값)에서는 `VisualMesh`가 기존 `ABP_Retarget`을 쓰므로 캐스트가 실패해 아무 일도 하지 않는다. Phase 3이 같은 클래스와 훅에서 시작한다. Phase 3에서 `ABP_Retarget`에 노드를 넣으면 `BeginPlay`의 애님 클래스 교체 CVar(`LNP.Anim.PhysHitReactSpikeABP`)는 지운다.
- `/Game/Characters/PhysHitReactSpike/ABP_Retarget_PhysHitSpike`: Phase 3이 `ABP_Retarget`(2-0b 이후 구조)에 같은 노드를 옮길 때의 기준 설정이다. 옮긴 뒤 폴더째 삭제한다.
- `.uproject`·`Build.cs`의 PhysicsControl 명시: 본 구현에서도 필요하다.
