# 애니메이션 개선 로드맵

> 상태: 기준 로드맵
> 읽기 조건: Phase 전환, 전체 순서 변경, 작업 범위 재산정 시
> 마지막 갱신: 2026-10-26

## 1. 진행 원칙

- **구현 단위 하나 = 세션 하나.** 각 단위는 그 자체로 검증하고 커밋할 수 있는 상태로 끝난다. 단위가 끝나면 사용자 진행 확인을 받는다.
- **실현 위험이 큰 것부터.** 노력은 크지만 위험이 낮은 ABP 재구성(Phase 2)보다, 될지 안 될지 모르는 스파이크(Phase 1)를 먼저 끝낸다.
- **GASP는 따라 만든다(AN-001).** GASP 에디터에서 블록을 읽고 LootNPop에서 직접 만든다. 소스 에셋만 마이그레이트한다.
- **미래 Phase의 상세 실행서는 착수 직전에 쓴다.** 이 문서는 요약만 소유한다.
- 모든 단위의 완료 조건에 다음을 포함한다.
  - 적도 근처(중력 Up이 월드 Z와 크게 다른 지점) PIE 확인
  - 해당 변경이 `ActorPromoted` 적에 미치는 영향 확인(적은 같은 `ABP_Lyra`를 쓴다)
  - 네트워크가 걸린 변경은 PIE 2인(리슨) 확인
  - 커밋 뒤 깨끗한 트리에서 `Build.bat` 재확인(C++ 변경 시)

## 2. 트랙과 의존성

```
[본 트랙]
Phase 0 참조 환경과 기준 결정
  ├─→ Phase 1A 물리 히트리액트 스파이크 ──────────────┐
  ├─→ Phase 1B 마커리스 모캡 스파이크 ──────────────┼─→ Phase 4 모캡 근접 세트 (상시 병행)
  └─→ Phase 1C GASP 소스 에셋 최신화 ─→ Phase 2 GASP Mover ABP 재구성 ─┤
                                                     └─→ Phase 3 물리 히트리액트 본 구현 (1A + 2 필요)
Phase 2 완료 ─→ Phase 5 UAF 판정 스파이크 ─→ (Go) Phase 6 UAF 마이그레이션

[트랙 L — 최후순위, 독립 (AN-008)]
L1 Locomotor 구면 검증 → L2 드론 전투 설계 → L3 이식
```

- Phase 1A·1B·1C는 Phase 0 뒤에 서로 독립이라 **병렬 세션**으로 돌릴 수 있다. 같은 에셋을 건드리는 단위끼리는 동시에 돌리지 않는다(1C는 `GASP_Mannequin` 폴더와 그 참조자 — `BP_LNPEnemy`·적 DA·ASTP·`Retargeters/*` — 를, Phase 2는 `ABP_Lyra`·서브 ABP·`CHT_Montage`·`ABP_Retarget`을 바꾼다). LootNPop 에디터(MCP 8000)는 하나를 함께 쓰므로, 1C-3처럼 에디터를 꺼야 하는 단위 동안 다른 갈래는 에디터 작업을 멈춘다. 구체 목록은 `Current.md`.
- Phase 2는 1C가 끝난 뒤 최신 시퀀스로 시작한다. 낡은 시퀀스로 블록을 짠 뒤 교체하면 PSD·Chooser를 두 번 맞추게 된다. 스켈레톤 전환(2-0a·2-0b, AN-011)도 1C가 확정한 UEFN 스켈레톤으로 리타깃해야 다시 만들 일이 없다.
- Phase 3은 Phase 2가 끝나 최종 포즈 파이프라인이 정해진 뒤에 들어간다(물리 노드 위치가 거기에 달려 있다).
- Phase 4는 1B를 통과하면 Phase 2·3과 병행한다. 근접 몽타주·ANS만 다루고 ABP는 건드리지 않는다.

## 3. Phase 요약

| Phase | 목표 | 세션 단위 | 상태 | 핵심 완료 조건 |
|:---|:---|:---|:---|:---|
| 0 | 참조 환경과 기준 결정 | 0-1 MCP 읽기 범위 확인 · 0-2 GASP 블록 목록 · 0-3 스켈레톤·소스 에셋 규칙 결정 (1~2세션) | 완료(2026-10-18) | `design/GASPReference.md` 생성, AN-011 UEFN 전환, 마이그레이트 규칙 AN-016·AN-017 확정 (`phases/Phase00_ReferenceSetup.md`) |
| 1A | 물리 히트리액트 스파이크 | 1세션 | 완료(2026-10-18, RBWC 노드 방식 — `research/PhysicsHitReactSpike.md`) | 테스트 폰의 `VisualMesh` 상체에 PhysicsControl 임펄스, 적도에서 처짐 없음, 2인 PIE 각 머신 로컬 재생, 캡슐·Mover 무영향 |
| 1B | 마커리스 모캡 스파이크 | 1~2세션 | 계획 | 영상 1클립 → MetaHuman 바디 → UEFN 리타깃 → 루트모션·검 방향 정리 → 공격 몽타주로 PIE 1타 |
| 1C | GASP 소스 에셋 최신화 | 1C-1 대조표·백업 · 1C-2 경로 정렬 · 1C-3 덮어쓰기 · 1C-4 회귀 (3~4세션) | 완료(2026-10-18, 4세션. ISKM 인스턴싱 설정은 `Retargeted/` 사본에(AN-018), 포스트프로세스 ABP 끔(AN-019)) | 옛 GASP 유래 411개를 5.8 최신본으로 교체, 원본 경로 정렬·`.gitignore`(AN-016), 노티파이 리다이렉트(AN-017), LootNPop 산출물 보존, 깨진 참조 0, ISKM·승격 적 회귀 없음 (`phases/Phase01C_GASPAssetRefresh.md`) |
| 2 | 스켈레톤 전환 + GASP Mover ABP 재구성 | 2-0a·2-0b 전환 2세션 + 블록당 1세션 6세션 | 완료(2026-10-21, 8세션 2-0a~2-6, `phases/Phase02_GASPMoverABP.md`) | 아래 §4. 블록마다 적도·NPC·2인 확인, 무기 레이어·슬롯·ANS 회귀 없음 |
| 3 | 물리 히트리액트 본 구현 | 2세션(3-1 경로·방향성, 3-2 몽타주 대체·적 분리) | 완료(2026-10-23, 계측 정리·최종 에디터 빌드·종료 기록 포함. `phases/Phase03_PhysicsHitReact.md`) | 히트리액트 몽타주 대체, 방향·강도 반영, 플레이어·`ActorPromoted` 적, 2인 PIE |
| 4 | 모캡 근접 세트 확충 | 무기 세트당 1~2세션 | 계획 | 루트모션 공격이 근접 보정 Motion Warping 경로를 탐, ANS 5종 배치, 칼날 판정 일치 |
| 5 | UAF 판정 스파이크 | 준비 포함 3단위(5-0 참조·실행서, 5-1 최소 구면 로코모션, 5-2 계약·최종 판정). 조기 No-Go 시 종료 | 진행 중(2026-10-26, 5-0 완료·5-1 구면 루트/공중 고정/대표 입력·전환/기본 Strafe 비교/발 배치 최소 연결 확인·잔여 계약 미완료, `phases/Phase05_UAFSpike.md`) | 체크리스트(§4 Phase 5) 전 항목 판정, Go/No-Go 기록 |
| 6 | UAF 마이그레이션 | 착수 시 산정 | 보류(Phase 5 Go 시) | Phase 2 결과를 UAF로 옮기고 회귀 없음 |
| L1~L3 | Locomotor 사족 드론 | 착수 시 산정 | 보류(최후순위) | 아래 §4 트랙 L |

## 4. Phase별 범위 보충

### Phase 1A — 물리 히트리액트 스파이크

- 랙돌과 같은 `VisualMesh`가 대상이다(`EnterRagdoll`이 `VisualMesh`의 PhysicsAsset을 쓴다). `AnimSourceMesh`는 포즈 원본이라 건드리지 않는다.
- PhysicsControl **컴포넌트** 방식과 ABP의 **RigidBodyWithControl 노드** 방식 중 하나를 고르고 근거를 남긴다. `VisualMesh`가 포즈를 받는 경로와의 순서가 판단 기준이다.
- 구면 중력: 시뮬레이션되는 상체 바디의 Chaos 중력을 끄고, 필요하면 기존 `TickRagdollGravity` 패턴(중력 Up 방향 가속)을 재사용한다.
- 다리·골반은 키네마틱, 상체는 부모 공간 모터로 애니 포즈에 복귀한다.
- 확인: 사망 랙돌 진입·`ExitRagdoll`(표현 풀 재사용)과 충돌하지 않는다.
- 결과(2026-10-18): RBWC 노드 방식을 택했다. 알파를 상시 켜면 이동만으로 상체가 출렁이므로 피격 구간에만 켠다. 방향성 보강과 적 반응 세기가 Phase 3 과제다. 상세는 `research/PhysicsHitReactSpike.md`.

### Phase 1B — 마커리스 모캡 스파이크

- 준비: `MetaHuman Animator`·`Capture Manager`·Live Link Hub와 Fab의 마커리스 플러그인(약 5GB). Windows 전용, Experimental.
- 파이프라인: 영상 → MetaHuman 바디 애니 → IK 리타깃(UEFN 스켈레톤, AN-011) → 루트모션 추출·발 접지 정리 → 검 방향 보정(무기는 추적되지 않는다) → 몽타주.
- 2-0b(스켈레톤 전환) 전에 1B를 하면 `AnimSourceMesh`가 아직 Lyra라 UEFN 몽타주를 바로 PIE에 올릴 수 없다. PIE 1타는 2-0b 뒤로 미루거나 임시로 Lyra 사본을 한 번 더 리타깃해 확인한다(1B 착수 시 정한다).
- 소스 영상은 AN-010(확정)을 따른다: 다른 게임 영상은 프로토타입·레퍼런스만.
- 영상 선택 기준: 고정 카메라, 전신 노출, 컷·히트스톱·VFX 가림 없는 구간.
- 산출: 파이프라인 단계별 소요·실패 지점·수작업 비율을 `research/`에 기록해 Phase 4 산정 근거로 쓴다.

### Phase 1C — GASP 소스 에셋 최신화

- 옛 GASP에서 가져온 `Content/Characters/GASP_Mannequin/`(MotionMatchingData 제외)을 중복이어도 5.8 최신본으로 바꾼다(AN-012).
- 방식은 폴더를 GASP 원본 경로(`/Game/Characters/UEFN_Mannequin/`)로 옮긴 뒤 파일을 덮어쓰는 것이다(AN-013 확정). 411쌍의 참조 교체를 피하고, 다음 GASP 업데이트도 덮어쓰기로 끝낸다. 폴더·git 규칙은 AN-016, 노티파이·모디파이어는 AN-017.
- 적이 이 스켈레톤·메시를 쓰므로 회귀 검증의 중심은 ISKM 순수 엔티티 적과 `BP_LNPEnemy`다.
- 상세는 `phases/Phase01C_GASPAssetRefresh.md`.

### Phase 2 — 스켈레톤 전환과 GASP Mover ABP 재구성 (0-2 확정, 0-3에서 2-0a·2-0b 추가, 2026-10-18)

MM 노드 방식(AN-015). 블록 정의는 `design/GASPReference.md` §2.2(B0~B12). 블록 하나가 세션 하나다. 앞 단위가 뒤 단위의 입력을 만든다(전환 → B0 → 선택 → 루트·워핑 → 전환 세트 → 발 → 기울기). 2-0a·2-0b는 AN-011(UEFN 전환)의 실행 단위다(구조는 `design/GASPReference.md` §2.7).

| 단위 | 블록 | 내용 | 비고 |
|:---|:---|:---|:---|
| 2-0a | 리타깃 세트 | **완료(2026-10-18).** Lyra 실사용 91개 + 0-3이 놓친 7개(`Weapons/` 캐릭터 몽타주 6, `Fall_Loop` 1) = 98개를 오프라인 리타기터 `RTG_Lyra_to_UEFN_Offline`(루트 본 지정·발가락 매핑 해제·Copy Base Pose)으로 `Content/Characters/Retargeted/Lyra/`(git 제외)에 만들었다. 서브 ABP 5개 `ABP_Sub_*_UEFN`. 재생성 스크립트 `Scripts/AnimRetarget/` | 런타임 변화 없음. 대조 98쌍 결함 0(ANS 5종·섹션·루트모션·노티파이·슬롯). 가상 본 미사용. 몽타주 그룹 14건 차이는 스켈레톤 슬롯 그룹 문제 → 2-0b 결정 B |
| 2-0b | 전환 | **완료(2026-10-19).** `AnimSourceMesh` = `SKM_UEFN_Mannequin` + `ABP_Main_UEFN`(`ABP_Lyra` 리타깃 복제, `bDisablePostProcessBlueprint`), 임시 로코모션 `CHT_PSD_UEFN`(결정 A), 슬롯 그룹 C++ 런타임 등록(결정 B, AN-020), `CHT_Montage`·GA 2개·`VS_*`를 사본으로, 플레이어 `VisualMesh`는 `RTG_GASP_to_MS`(대상 리그 `IK_MS_FootGoal`: 다리 FK·팔 IK), 적 `VisualMesh`는 `ABP_VisualCopyPose`(포즈 복사, RBWC 자리). 표시 ABP 왼손 그립 IK·견착 보정(무기별 `VS_*` 값), Chooser 행 교체용 에디터 명령 `LNP.Editor.ReplaceAssetRefs` | 사용자 2인 PIE 통과(근접 콤보·ANS·근접 보정·재장전·가드/패링·대시·랙돌·피격, 적도). PIE에서 찾아 고친 것: 대시 블렌드 프로필, 리타기터 다리 IK·체인 매핑, UEFN PSD 정규화 세트 누락, 무기 왼손·견착·총구. **알려진 차이: 스트레이프 방향 클립 없음 → 2-2** |
| 2-1 | B0 + B10 | **완료(2026-10-19).** `ULNPAnimInstance`에 상태 enum 6종·이력, 궤적 파생값, 가속, 지면 법선·경사, `BasedMovementDelta`, 상태 술어 8개(계산식 `design/GASPReference.md` §2.8). 궤적(Mover 예측기)도 C++가 소유하고 `ABP_Main_UEFN`의 BP 궤적 로직을 지웠다. Pose History GASP 설정, `AdditiveHitReact` 슬롯 뒤 DeadBlending. 디버그 CVar `LNP.Anim.DebugLocomotion` | 사용자 2인 PIE(적도·승격 적) 로그로 값 확인, 회귀 없음. PIE에서 고친 것: `bJustTeleported` 착지·시뮬 프록시 오탐 |
| 2-2 | B1 | **완료(2026-10-19).** 평면 멀티 결과 Chooser `CHT_PoseSearchDatabases_UEFN`(행 입력 `ELNP*` 이동 모드·상태·보행·방향) + 루프 DB 10개(Idle·Walk/Run × F·B·LL·RL·Sprint, GASP PSD·PSS·PSN을 `LNP.Editor.GASPRef`로 복제). 방향별 시퀀스 26개 반입으로 **스트레이프 복구**. 인터럽트·블렌드 시간·DB 설정은 `ULNPAnimInstance::UpdateMotionMatchingNode`(ABP는 호출만), MM 노드 GASP 설정 | 사용자 1·2인 PIE(적도·승격 적) 통과. PIE에서 고친 것: 적도 이동 방향(`CalculateDirection` 월드 Z 제거), 출발 시 Idle 고착(출발 프레임 인터럽트). AN-007 유지 |
| 2-3 | B3 + B2 | **완료(2026-10-20).** MM 블렌드 스택 그래프에 GASP 체인(CopyBone×2 → Steering → 제자리 회전 Steering → OrientationWarping → ResetRoot, 함수는 C++), AnimGraph `DeadBlending` 뒤 `OffsetRootBone`(모드·반감기·지면 법선 C++, 전신 몽타주 Release, `MaxRotationError` 90°). `NativeThreadSafeUpdateAnimation`이 base 이동량·Up 정렬을 넣는다. AO·`FutureFacingDelta`는 가상 루트 기준 | 엔진 구면 함정 3건(지면 법선 월드 Z, 속도 클램프 `GetSafeNormal2D`, Accumulate가 캡슐 기울기를 안 따라감) 대응. 사용자 1·2인 PIE(적도·승격 적)·움직이는 발판 통과. 정지 회전은 미끄러짐(2-4 TurnInPlace). Stride Warping은 넣지 않음 |
| 2-4 | B6 (+B7 착지) | **완료(2026-10-20).** 전환 DB 41개(Starts·Stops·Pivots·Turns·Spins·TurnInPlace·GaitTransitions·Lands, GASP 복제)와 스키마 7개, 시퀀스 239개 반입, `CHT_PoseSearchDatabases_UEFN` 52행(최근 방향·직전 보행 마스크, 속력 구간, 술어 6개). 인터럽트 예외 = 출발 프레임(유지 확정) + 정지 중 제자리 회전 요청. 착지 DB는 지상 첫 프레임만(AN-007 유지) | 사용자 1·2인 PIE(적도·승격 적) 회귀 없음, 제자리 회전 발 디딤 확인(소량 미끄러짐). `MaxRotationError` 90° 안전망 유지. 피벗·스핀은 LootNPop 고속 큰 호 탓에 거의 안 뽑힘 |
| 2-5 | B4 + B5 | **완료(2026-10-20).** GASP CR 복제 `CR_Biped_FootPlacement_UEFN`(함수 참조 재연결, 구면 수정 3곳 — 루트 감쇠 Up 방향 지연량·핀 리그 공간 섞기), AnimGraph `OffsetRootBone` 뒤 CR 노드(알파 = 지상, 공중 IK 없음, `UseIKBoneTargets` 끔, 착지 리셋). 움직이는 base 위 핀 고정·루트 감쇠 끔. B5는 CR slope warping만(AN-021, 반입 없음) | PIE에서 고친 것: 발판 위 텔레포트 오탐, 느린 정지 걷기 유지(정지 프레임 인터럽트), 발판 위 다리 끊김·전신 움찔, **엔진 `OffsetRootBone` `MaxRotationError` 클램프의 적도 루트 기울어짐(구면 함정 4번째)**. 사용자 1·2인 PIE(적도) 회귀 없음 |
| 2-6 | B8 + 적 검증 | **완료(2026-10-21).** GASP `AdditiveLeans`를 AnimGraph 노드 묶음으로(분기는 C++ `ShouldApplyAdditiveLeans`·`ShouldApplyHeadLean`, 머리 기울기는 FreeAim·락온이 아닐 때만, 락온은 InputCmd에서 읽음). 위치는 **무기 오버레이 뒤·슬롯 앞** — 무기 서브 ABP가 `spine_02` 위를 메시 공간 회전 블렌드로 덮어 앞이면 상체 기울기가 지워진다. Leans 소스 16개는 1C에 이미 있었고 BS 3개만 GASP 원본으로 덮어써 해시를 맞췄다. `ActorPromoted` 적 전체 회귀 | 사용자 1·2인 PIE(적도·승격 적·게스트) 어색함 없음(육안). 최종 파이프라인은 `phases/Phase02_GASPMoverABP.md` "Phase 2 종료" |

제외: B9 GASP Aim Offset(무기별 AO 유지), B11 래그돌 연동(AN-005, 곡선으로 PhysicsControl 강도를 주는 패턴만 Phase 3 참고), B12 상호작용 손 IK, Slide·Traversal·Debug Flying. 앉기(Crouch)는 입력이 생길 때까지 보류.

유지 조건: `ALI_WeaponStyles` 레이어, 슬롯 3종, AO 경로, 공중 `Fall_Loop` 고정(AN-007), 근접 보정 Motion Warping 어댑터.

### Phase 3 — 물리 히트리액트 본 구현

- `LNPGameplayCueNotify_HitReact` → `PlayHitReact` 경로에서 몽타주 대신 임펄스(방향·강도)를 준다. `CHT_Montage`의 HitReaction 행 정리.
- HitStop(`CustomTimeDilation`)과 물리 시뮬레이션의 상호작용을 확인한다.
- 경직 몽타주(Light/Heavy/Parried)는 그대로 둔다(AN-004).
- 3-1 결과(2026-10-21): 두 표시 ABP에 RBWC를 넣고 큐가 맞은 지점·피해·근접 여부를 싣게 했다. 방향성 3안은 몽타주 위에서는 체감 차이가 작았고 당시 기본값은 모드 12였다. 물리 단독은 "가벼운 피격은 대체 가능, 다만 맞은 부위·방향 체감은 약하다"는 평가로 3-2의 보강으로 이어졌다.
- 3-2 완료(2026-10-23): 골반 오프셋·척추 강도 조절을 보강한 모드 100으로 HitReact 몽타주·Chooser 10행을 대체했다. 골반은 3cm·6°·7.5°로 완화했고 사용자 100 선호로 유지한다. 적 세기는 유지하고 플레이어 근접 공격 중 물리만 억제한다(AN-024, NPC 공격 중 허용). 리슨 2인·적도·골반/다리·랙돌/LOD 재접근·리스폰 수동 회귀, 포즈 실측, 일회성 계측 제거 후 최종 에디터 빌드를 통과했다. 기존 Mover `UpDirection=0` 경고 등 증거 한계는 `history/Phase03_Log.md`에 남긴다.

### Phase 4 — 모캡 근접 세트 확충

- 루트모션이 실린 공격은 근접 보정이 자동으로 Motion Warping 경로를 탄다(코드 존재, [TechDesign_CombatAnimation.md](../TechDesign_CombatAnimation.md) §6.6).
- 섹션 기반 콤보·ANS 5종 배치, `sword_tip`/`sword_root` 궤적과 판정 일치 확인.

### Phase 5 — UAF 판정 스파이크

실행서: `phases/Phase05_UAFSpike.md`. 5-0 조사 후 5-1에서 기존 입력 공유·UAF 달리기/Idle 검색·Rewind 과거 조회를 확인했다. Steering/구면 루트 보정은 C++ 수치 회귀 1/1·북극/적도/패널 병진의 관찰 및 수치로 확인했다(§8~§10). 공중 Fall_Loop 고정(AN-007)도 Bool/Sequence Player·Steering 조회 보완, 긴 체공/적도 관찰과 수치 및 Rewind 실제 평가 task 두 공중 시점/착지 복귀를 확인해 단위 완료했다(§12). 입력 PSD 배열·대표 전환/인터럽트도 기존 녹화의 Idle·출발·정지·착지·제자리 회전·방향 전환으로 확인해 단위 완료했다(§13). 모든 전환 종류/프레임 동등성으로 확대하지 않는다. 기본 UAF Strafe Warping 비교도 북극/적도 0/1·실제 task 순서·공중 해제/착지 복귀·구면 루트 제한과 사용자 관찰로 단위 완료했다(§14). 이어 발 배치 CR 최소 연결·검증도 완료했다(§15, 2026-10-26). 신규 v2 유지·기존 구면 CR 참조·Bool 알파/입력 연결, 북극 경사0/1·적도 관찰, 실제 공중 task 해제/지상 CR 실행과 발판 입력을 확인했다. 루트 수치는 CR 앞 진단이고 발판 녹화에는 공중 표본이 없다. 다음 제안은 ResetRoot/CopyBone 잔여 계약 대조이며 사용자 확인 대기다. 샘플별 보정 동등성·큰 회전 base·나머지 전체 게이트·Go/No-Go는 보류, 5-1 미완료·5-2 미착수다.

무기 없는 플레이어 1명을 `MoverAnimNext`(Mover UAF)와 UAF 모션매칭으로 구동하고 아래를 판정한다.

- [ ] Phase 2의 PSD·Chooser를 그대로 재사용
- [ ] 몽타주 재생과 노티파이·노티파이 스테이트(특히 `Montage_Stop` 시 `NotifyEnd` 보장)
- [ ] 무기별 레이어 교체에 해당하는 기능(`UAFLayering`)
- [ ] Motion Warping·루트모션의 Mover 연동
- [ ] 시뮬레이티드 프록시·게스트 재시뮬레이션
- [ ] Rewind Debugger 등 디버깅 도구
- [ ] 적도 근처 동작

하나라도 동등하지 않으면 No-Go(AN-006).

### 트랙 L — Locomotor 사족 드론 (최후순위, AN-008)

- **L1 구면 검증:** 엔진 `Locomotor`는 월드 Z를 Up으로 하드코딩한다(`research/UE58_AnimationFeatures.md` §3). 프로젝트 `Plugins/`로 포크해 Up을 파라미터화하고 적도 보행을 판정한다.
- **L2 전투 설계:** GASP에는 이동만 있다. 공격 수단(Control Rig 절차 공격, `SK_Mech` 전용 클립 제작)과 물리 피격(PhysicsControl, Physics Asset 신규 제작 필요)을 정한다. 드론은 Control Rig가 Actor를 요구하므로 `ActorPromoted` 전용이다.
- **L3 이식:** 루트 소유권(Mover 캡슐 vs Locomotor 몸통 이동), 승격 전 ISKM 구간용 베이크 클립, 사족 체형과 캡슐 판정 규약.
- 착수 조건: 본 트랙 Phase 3 이후 또는 사용자 호출.

## 5. 범위 밖과 후속 작업

- 파워드 래그돌 넉다운·기상(AN-005). [TechDesign_CombatAnimation.md](../TechDesign_CombatAnimation.md) §8의 넉다운 몽타주 확보는 그와 별개로 몽타주 교체만으로 할 수 있다.
- `PureEntity`(ISKM) 적의 애니메이션 — ISKM은 물리·절차 애니메이션을 쓸 수 없다. UAFMass 플러그인은 관찰만 한다.
- 경사 전용 애니(GASP `BS_Relaxed_*_Loop_F_Slopes`) — 2-5에서 넣지 않았다(AN-021). 필요해지면 경사각 가산 레이어나 PSD 경사 채널을 별도로 설계한다.
- 원거리 무기 왼손 IK, 이동 중 상체 공격 같은 기존 잔여 작업([TechDesign_CombatAnimation.md](../TechDesign_CombatAnimation.md) §8)은 Phase 2 중 자연스럽게 걸리면 같이 처리하되 이 로드맵의 완료 조건에는 넣지 않는다.
