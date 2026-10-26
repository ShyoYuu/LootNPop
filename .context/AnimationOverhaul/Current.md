# 애니메이션 개선 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: **Phase 5, 5-0 완료·5-1 진행 중.** 기본 UAF Strafe Warping 비교 구현 단위 완료(§14): 북극/적도 0/1·공중 해제/착지 복귀·실제 task 순서 확인. 다음 발 배치 Control Rig UAF 연결 단위 진행 확인 대기. Phase 0·1A·1C·2·3 완료, 1B는 대기
> 마지막 갱신: 2026-10-26

Phase 3은 기본 모드 **100 = 지점 임펄스 + 골반 오프셋 + 척추 강도 조절**로 마쳤다. 골반은 기준 세기에서 3cm·6°·7.5°이며 68로 골반만 끌 수 있다. 가벼운 HitReact 몽타주는 물리로 대체했고 경직 몽타주·HitStop은 유지한다. 플레이어 근접 공격 중에는 물리를 억제하고 NPC는 허용한다(AN-024). 사용자 수동 회귀·실측 근거는 `history/Phase03_Log.md`와 `research/PhysicsHitReactSpike.md` §10에 있다. 일회성 `PhysHitReactMeasure`와 평가 훅은 검증 후 제거했으며, 기본 꺼짐인 `PhysHitReactLog`만 유지한다. **다음 구현 단위 착수 전 사용자 확인을 받는다.**

## 현재 결정

확정 결정(상세 `Decisions.md`):

- GASP는 구성을 보며 직접 다시 만들고 소스 에셋만 들여온다(AN-001). GASP 에디터는 8001 읽기 전용(AN-009)
- 옛 GASP 유래 소스 에셋은 5.8 최신본으로 교체했다(AN-012, 1C 완료). 경로는 GASP 원본과 같은 `Content/Characters/UEFN_Mannequin/`이고, 다음 갱신도 덮어쓰기로 한다(AN-013)
- 로코모션 선택 코어는 MM 노드 방식(AN-015)
- **스켈레톤은 UEFN 전환(AN-011, 2-0b 완료).** `AnimSourceMesh` = `SKM_UEFN_Mannequin` + `ABP_Main_UEFN`, 로코모션은 GASP 클립 그대로(임시 Chooser `CHT_PSD_UEFN`), Lyra 무기·몽타주 애니는 UEFN 사본(`Retargeted/Lyra/` 98개, 서브 ABP `ABP_Sub_*_UEFN`). `VisualMesh`: 플레이어 `RTG_GASP_to_MS`(대상 리그 `IK_MS_FootGoal`, 다리 FK·팔 IK, 리타깃 포즈 정렬 안 함) + 표시 ABP의 견착 보정(오른팔)·왼손 그립 IK, 적 `ABP_VisualCopyPose`(포즈 복사 + 왼손 그립 IK). 무기별 값은 `VS_*`의 `LeftHandGripOffset`·`RightHandCorrection`. 구성 상세는 [TechDesign_CombatAnimation.md](../TechDesign_CombatAnimation.md) §3.3. 기준 스켈레톤은 GASP 5.8 `SK_UEFN_Mannequin` **93본**이다
- **폴더·git(AN-016):** GASP 소스는 GASP 패키지 경로 그대로 두고 경로 단위로 git 제외. LootNPop 구성 에셋은 GASP 경로 밖(UEFN PSD 등은 `Content/Characters/MotionMatchingData/`). 리타깃 산출물은 `Content/Characters/Retargeted/`(git 제외)
- **노티파이(AN-017):** GASP 노티파이·PSD 하드 참조는 `DefaultEngine.ini`의 `PackageRedirects ... Removed=true`로만 끊는다(C++ 없음)
- **소스에 설정을 넣지 않는다(AN-018):** 소스는 저장하지 않는다. 필요한 설정은 `Retargeted/` 사본에 건다. ISKM 적은 `SKM_UEFN_Mannequin_Instanced`·`M_UEFN_Mannequin_Instanced`를 쓴다
- **포스트프로세스 ABP 끔(AN-019):** `SKM_UEFN_Mannequin`을 쓰는 컴포넌트마다 `bDisablePostProcessBlueprint = true`(두 BP `AnimSourceMesh`, 적 `VisualMesh`)
- **몽타주 슬롯 그룹(AN-020):** `ULNPAnimInstance` 초기화가 Lyra 슬롯 → 그룹 표를 런타임 등록한다(2-0b). `SK_UEFN_Mannequin`을 에디터에서 열어 저장하지 않는다
- 마커리스 소스 영상: 타사 영상은 프로토타입·레퍼런스만(AN-010)
- **로코모션 공급층(2-1 완료):** 상태 enum 6종(`ELNP*`)·이력·궤적 파생값·가속·지면·술어는 `ULNPAnimInstance` 메인 인스턴스가 C++로 계산한다. 궤적(Mover 예측기, 0.033s×15 / 0.1s×15)도 C++ `Trajectory`가 소유하고 `PoseHistoryCollector`가 그 이름으로 바인딩돼 있다(이름 변경 금지). 계산식은 `design/GASPReference.md` §2.8, 확인은 CVar `LNP.Anim.DebugLocomotion 2` 로그(`[AnimB0]`)
- **선택 코어(2-2 완료):** 평면 멀티 결과 Chooser `CHT_PoseSearchDatabases_UEFN`이 `ELNP*`(이동 모드·상태·보행·방향)로 루프 DB 10개(`PSD_Stand_*_UEFN`, GASP 복제)를 고르고, `ULNPAnimInstance::UpdateMotionMatchingNode`가 DB·인터럽트·블렌드 시간을 적용한다(ABP는 호출만). 스트레이프는 방향별 DB(F·B·LL·RL). 인터럽트는 GASP 식 + 출발 프레임(Idle 스키마가 궤적을 안 봐서, 2-4에서 유지 확정). GASP 구성 에셋은 `LNP.Editor.GASPRef`로 덤프·복제한다(`design/GASPReference.md` §1.4·§2.9). 이동 방향 `Direction`은 엔진 `CalculateDirection` 대신 Up 둘레 부호각(적도 버그)
- **루트·워핑(2-3 완료):** MM 블렌드 스택 그래프 = GASP 체인(Steering×2·OrientationWarping·ResetRoot·CopyBone, 판정은 C++ 함수), AnimGraph `DeadBlending` 뒤 `OffsetRootBone`(태그 `OffsetRoot`). 모드·반감기·지면 법선은 C++ 함수, base 이동량과 가상 루트 Up 정렬은 `NativeThreadSafeUpdateAnimation`. 엔진 Offset Root는 구면에서 세 군데가 틀린다(월드 Z 지면 법선, 속도 클램프 `GetSafeNormal2D`, Accumulate가 캡슐 기울기를 안 따라감) — 속도 클램프는 켜지 말 것. AO·`FutureFacingDelta`는 가상 루트 기준. `MaxRotationError` 90°는 2-4 뒤에도 안전망으로 유지. 값 요약 `design/GASPReference.md` §2.10
- **전환 세트(2-4 완료):** Chooser 52행 = 루프 + 전환 DB 41개(Starts·Stops·Pivots·Turns·Spins·TurnInPlace·GaitTransitions·Lands, GASP 복제). 피벗·스핀은 `MovementDirection_Recent` 마스크, 착지는 지상 첫 프레임만(AN-007 유지). 인터럽트 예외는 출발 프레임 + **정지 중 제자리 회전 요청**(Idle 진행 포즈가 회전 후보를 이겨서). 행 정의 원본 `Scripts/AnimOverhaul/BuildLocomotionChooser_2-4.py`, 요약 `design/GASPReference.md` §2.11, 선택 확인은 `[AnimB6]` 로그
- **발 배치(2-5 완료):** GASP CR 복제 `CR_Biped_FootPlacement_UEFN`(함수 참조 재연결·구면 수정 3곳, 재현 `Scripts/AnimOverhaul/FixFootPlacementCR_2-5.py`)이 AnimGraph `OffsetRootBone` 뒤 노드 하나로 돈다(알파 = 지상, 공중 IK 없음, 착지 리셋, `UseIKBoneTargets` 끔). 움직이는 base 위에서는 핀 고정·루트 감쇠 끔(`IsOnStaticGround`). 경사는 CR slope warping만(AN-021). ⚠️ 엔진 `OffsetRootBone` `MaxRotationError` 클램프는 적도에서 루트를 기울인다 — 노드 값 −1, C++가 Up 둘레로 90° 제한(구면 함정 4번째). 요약 `design/GASPReference.md` §2.12
- **기울기(2-6 완료, AN-022):** GASP `AdditiveLeans`를 AnimGraph 노드 묶음으로, 위치는 무기 오버레이 뒤·슬롯 앞(무기 서브 ABP가 `spine_02` 위를 메시 공간 회전 블렌드로 덮어서). 분기는 C++ `ShouldApplyAdditiveLeans`(Moving, 공중 포함)·`ShouldApplyHeadLean`(FreeAim·락온이 아닐 때 머리도). 락온은 InputCmd `bIsLockOn`. CVar `LNP.Anim.AdditiveLeans`, 붙여넣기 생성 `Scripts/AnimOverhaul/BuildLeansPaste_2-6.py`. 요약 `design/GASPReference.md` §2.13
- **물리 히트리액트(Phase 3 완료):** 두 표시 ABP의 왼손 IK 뒤·출력 앞 RBWC가 물리 반응을 담당한다. 기본 모드 100, 골반 월드 오프셋(3cm·6°·7.5°) + 실제 존재하는 척추 컨트롤만 강도 조절. HitReact 몽타주 호출·Chooser 10행 제거, 경직 몽타주·HitStop 유지. 공통 설계 [TechDesign_CombatAnimation.md](../TechDesign_CombatAnimation.md) §6.4, 실측 결과 `research/PhysicsHitReactSpike.md` §10
- **플레이어 근접 판정 보호(AN-024):** 실제 무기 위치를 판정에 쓰므로 플레이어 근접 몽타주 전체·블렌드아웃 동안 물리 반응을 억제하고 잔여 반응·예약을 버린다. NPC 공격 중 반응은 허용한다. C++ 구현·Live Coding·사용자 PIE 4항목 통과. AN-004 대체
- **Phase 2 최종 포즈 파이프라인**은 `phases/Phase02_GASPMoverABP.md` "Phase 2 종료"와 [TechDesign_CombatAnimation.md](../TechDesign_CombatAnimation.md) §1이 소유한다. 엔진 노드 구면 함정 4건 + `CalculateDirection`은 같은 문서 §7.4
- **빌드:** GASP 에디터만 떠 있어도 LootNPop `Build.bat`이 Live Coding 검사에 막힌다(같은 엔진 실행 파일). LootNPop 에디터를 끈 뒤 `-NoHotReloadFromIDE`를 붙여 빌드한다. GASP 에디터는 끄지 않아도 된다

## 바로 다음 행동 (갈래별 첫 행동)

**현재 우선 — Strafe 비교 단위 완료(2026-10-26, Phase05 §14).** 기본 UAF Strafe Warping 입력·순서 어댑터 C++ 4파일과 AG를 구현했다. 전체 빌드(12 actions, 88.71초)·구면 수치 회귀 1/1·실제 기록의 준비/Steering → Strafe → Offset Root → 최종 구면 보정 순서를 확인했다. 북극/적도 0/1 비교에서 두 모드 모두 자연스럽고 차이는 작으며 북극은 1이 약간 낫다는 사용자 평가다. 적도 최종 기록은 `Saved/RewindDebugger/RewindDebugger-LootNPop-Editor-20261026_172123536.utrace`(189,181,212바이트), 고정 로그/집계는 `Saved/AnimationOverhaul/2026-10-26_StrafeEquator/`다. 적도 공중/착지 7쌍, 공중 알파/각도 0(커브 0.631 잔류 중에도 차단)과 착지 후 알파 1 복귀, 최종 루트 거리≤30cm·Yaw≤90°·Tilt=0.000°를 확인했다. 표본·진단 창·짧은 점프 한계는 §14를 따른다. 후보 기본값 1 유지, 기존 ABP의 예측/샘플별 보정·발/루트 처리와 완전 동등 판정은 아니다. 본 배열 전체 독립 재조회도 하지 않았다. **다음 제안은 발 배치 Control Rig의 UAF 연결 경로 조사·최소 구현·검증이며 사용자 진행 확인 전 착수하지 않는다.** ResetRoot/CopyBone 계약·큰 회전 base·추가 전환·적/네트워크·무기/몽타주 게이트는 남았다. 5-1 미완료·5-2 미착수·Go/No-Go 보류. PIE는 유지됐고 녹화는 종료됐다. 최종 로그 조회 시 Slate observer는 해제했다.

**Phase 5 선택, 5-0 완료(2026-10-23).** 실행서는 [Phase05_UAFSpike.md](phases/Phase05_UAFSpike.md), 수행 기록은 `history/Phase05_Log.md`다. GASP `/Game`의 UAF·AnimNext 클래스 조회 결과는 `[]`였으며, 현재는 LootNPop 테스트 사본으로 시험한다. **최종 일곱 게이트는 아직 통과하지 않았고 Go/No-Go는 보류다. 5-1은 미완료이며 5-2에 착수하지 않는다.**

**5-1 현재:** 숨김 `ABP_SourceUAFSpike`가 기존 Chooser·Mover 궤적과 상태를 공급하고 UAF가 표시 포즈를 평가한다. 공유 입력·UAF 실제 달리기/Idle 검색·Rewind 과거 시점 조회, Steering/구면 루트 보정과 공중 고정 단위를 완료했다. 입력 PSD 배열·대표 전환/인터럽트 대조도 완료했다(§13). 기본 Strafe 비교 단위도 완료했다(§14). 나머지 전환 종류·샘플별 워핑 동등성·발 배치·큰 각도 회전 base·적/네트워크 등은 남았다. 현재 구현/공중 검증은 Phase05 §12, 이전 구면 기준은 §10~§11이 소유하며 GASP는 필요 없다.

### 새 세션의 첫 행동

1. CLAUDE.md·README·이 문서·Decisions를 읽고 Phase05 §14의 현재 대기 상태에서 재개한다. §12 공중 고정과 §13 대표 입력/전환은 완료했다. §14의 기본 Strafe 비교는 완료했다. 다음 발 배치 Control Rig 단위는 새 세션의 진행 요청으로 승인받아 시작한다.
2. **완료한 기준을 유지한다.** 이전 구면 보정은 Development 빌드(5 actions, 37.65초)·수치 회귀 1/1·북극/적도/패널 시험을 통과했다(§10). 이번 공중 단위의 C++ 조회 변경은 Live Coding 성공과 AG/S 컴파일 및 실행으로 검증했다(§12). 공중 단위에서도 최종 Tilt 0.000°·Yaw≤90°·거리≤30cm를 확인했다. 코드 변경 없이 빌드를 반복하지 않는다.
3. **입력 PSD 배열·대표 전환/인터럽트 단위 완료.** 기존 `20261025_133940695.utrace`의 Idle·출발·착지·제자리 회전·정지·방향 전환을 대조했다. 마지막 방향 전환은 F Loops/해제 → RL Loops+Spins/인터럽트 → 같은 배열/해제, 블렌드 0.5와 실제 UAF RL Spin이다. 착지만 0.2→지상 0.5 복귀다. 상세 표·근거·Chooser 표시 시점 차이/빈 Native Properties 기록 한계는 Phase05 §13에 있다. 모든 전환 종류와 프레임을 통과 처리하지 않으며 Pivot·Sprint Turns·GaitTransitions 등 추가 표본은 남았다. 이번 단위는 문서 외 변경·빌드·새 녹화 없음. 이후 기본 Strafe 비교 단위도 완료했다(§14). 다음 발 배치 Control Rig 단위는 사용자 진행 확인 대기다. 5-1 전체·Go/No-Go는 보류, 5-2 미착수다.
4. TestMap03의 GameMode Override는 BP_GameModeUAFSpike 임시 설정 상태다. 이 맵은 기존 사용자 변경과 섞여 있어 이번 커밋에서 제외하고 로컬에 유지한다. 다른 체크아웃에서는 시험 전에 World Settings의 Override를 테스트 사본으로 지정해야 한다. 5-1 종료 시 원래 값으로 복원하되 맵 전체를 되돌리지 않는다. 원래 Override의 구체 값은 현재 기록에서 확인되지 않아 복원 전 사용자 메모·기준값을 확인한다. 다음 구현 단위마다 진행 확인을 받고, 수동 조작이 더 효율적이면 즉시 요청한다.

별도 미해결: 리슨 2인 최종 회귀에서 기존 적 Mover의 `UpDirection=0` 경고 410회가 재현됐다. 물리 켬/끔을 새로 통제한 성능 비교나 동일 Actor 풀 재사용의 직접 추적은 하지 않았다. 이 한계와 NPC 손의 큰 물리 추종 지연은 연구 §10·Phase 3 로그에 남겼다. 완료한 수동 회귀·실측 결과를 넘어 일반화하지 않는다.

| 갈래 | 첫 행동 | GASP 에디터(8001) | 건드리는 에셋 |
|:---|:---|:---|:---|
| ISKM 총기 적 재베이크 | [TechDesign_EnemyNPC_LowLOD.md](../TechDesign_EnemyNPC_LowLOD.md) "미해결 — ISKM 총기 적" 절대로 `Retargeted/Enemy/` 총기 클립을 다시 굽는다(정렬하지 않은 리타깃 포즈 + 견착 보정, 왼손 IK를 그립 오프셋으로 굽기). Phase 2와 독립 | 불필요 | `Retargeted/Enemy/`(git 제외), ISKM 베이크 스크립트 |
| 1B 마커리스 모캡 스파이크 | `phases/Phase01B_*.md`를 만든다. 사용자가 Fab 마커리스 플러그인(약 5GB)을 설치하고 고정 카메라 영상 1클립을 고른다(AN-010). 리타깃 대상은 UEFN 스켈레톤(93본) | 불필요 | 새 시퀀스·몽타주(새 폴더), PIE 확인 시 `CHT_Montage` 행 하나 |
| Phase 5 UAF 판정 스파이크(선택됨) | Phase05 §14 비교 단위 완료, 다음 발 배치 단위 진행 확인 대기 | 불필요 | `/Game/Characters/UAFSpike/`와 스파이크 C++. 기존 ABP는 건드리지 않는다 |

**한 번에 한 세션만 돌린다(사용자 요청).** 같은 에셋을 건드리는 단위가 겹치지 않게 하는 제약:

- 1B·Phase 4는 `CHT_Montage`(몽타주)만, Phase 3-2는 표시 ABP 2개·히트리액트 경로·`CHT_Montage` HitReaction 행을 건드린다(1B·4와 같은 에셋이라 동시에 돌리지 않는다). 로코모션(`ABP_Main_UEFN`·`CHT_PoseSearchDatabases_UEFN`)은 Phase 2로 끝났고, Phase 5는 새 폴더에서 시험하므로 기존 ABP와 겹치지 않는다.
- `Retargeted/Lyra/`를 다시 만들 때는 `Scripts/AnimRetarget/RetargetLyraToUEFN.py`를 쓴다(엔진 문제로 Lyra 원본이 메모리에서 바뀌는 것을 스크립트가 되돌린다). 배치를 돌린 세션에서 Lyra 원본을 저장하지 않는다.
- `Retargeted/Lyra/`를 다시 만들면 `FixRetargetedMontages.py`도 이어서 돌린다(대시 블렌드 프로필 ensure 방지).
- GASP 소스(`UEFN_Mannequin/`)는 어느 단위도 저장하지 않는다(AN-018). 되돌리기 백업: `Saved/AssetRefresh/2026-10-18/backup_pre1C3/`(1C-3 직전), `backup/`(그 이전 원본).

### 5-1 현재 구성과 검증 한계

- 테스트 에셋 5개는 `Content/Characters/UAFSpike/`, 입력 공급층·구면 보정 코드는 `Source/LootNPop/Animation/UAFSpike/`다. 숨김 ABP 공급층과 UAF가 함께 돌아 성능 비교에 쓰지 않는다. 시스템 메시 locator는 `Current Actor → VisualMesh`, Pose History는 본 4개와 `Phase` 커브를 수집한다. `FoleyEvent: Walk/Run` 미처리 경고는 잔류한다.
- 현재 AG는 MM(True)/Fall_Loop Sequence Player(False) → Bool/Pose History → Passthrough + Strafe Warping + LNP Strafe Warping Inputs → Offset Root Bone + LNP Spherical Offset Root → Output. Bool은 양방향 0.25초, Fall_Loop는 ForceLoop·속도 1·시작 0이다. 공유 OffsetRoot 전체 연결과 bOnGround 조건을 사용하며, 공유 Trajectory는 Bool의 Pose History에만 연결한다. 이동/회전 속도 클램프는 꺼짐. 루트 준비(base/Up/Yaw·Steering)는 Strafe 자식 앞에서 한 번 예약되고, Strafe 입력/기본 보정 뒤 Offset Root·최종 구면 보정·진단이 따른다. Steering 조회는 Bool 목적 자식의 MM을 따른다. UAFWarping·UAFPoseSearch 내부 상태 의존성과 RigVM 네이티브 타입 등록이 있다.
- Rewind 저장 보완·ListenServer 빈 시간축 원인·Standalone 비교·UAF 달리기/Idle DB/클립·과거 시점 재조회와 공중 평가 task를 확인했다. 현재 UI에 SequenceInfo는 없으므로 EvaluationProgram의 Tasks를 읽는다. 기록/백업은 로컬 Saved에만 있고 git에 포함하지 않는다. 세부 이력은 `history/Phase05_Log.md`, 현재 구현은 Phase05 §8~§14다.
- 최종 회전 보정 후 북극·적도·패널의 관찰/기록 수치에서 Up·Yaw 문제 개선을 확인했고 공중 고정도 이번 단위로 확인했다. 큰 각도 회전 base(이전 시험 base 델타 최대 0.001°)·샘플별 보정 동등성·OrientationWarping 완전 동등성·ResetRoot/CopyBone·발 배치·추가 전환 표본·적/2인 전체 회귀는 남았다. 입력 PSD 배열·대표 전환/인터럽트는 §13에서 확인했다. Go/No-Go 보류, 5-2 미착수.
- §12 당시 시험의 LogAnimation Warning 1,361건은 기존 미구현 `FoleyEvent: Run/Walk/Jump/RunStrafe` 계열이다. 엔진은 해당 이벤트만 건너뛴다. 현재 로코모션 검증 차단 사유는 아니나 Foley 연출/로그 정리가 필요하다. LogAnimation Error 0건이며 경고와 최종 루트 기울기의 인과관계는 확인되지 않았다. 카테고리 전체 억제나 새 이벤트 처리는 이번 단위에서 구현하지 않았다.
- 이번 커밋에서 제외하는 기존 로컬 변경: `Content/Maps/TestMap03.umap`, `Content/Gameplay/DT_PlayerBaseStats.uasset`, `Content/ItemData/Weapons/Levels/DT_LongSword_Levels.uasset`, `Content/SpawnData/DA_MassSpawnConfig_TestOnePerSpecies.uasset`. TestMap03의 임시 테스트 Override도 로컬에 남아 있으므로 다른 체크아웃에서 시험할 때 다시 지정한다.

## 세션 종료 인계(2026-10-26)

- **종료 지점·승인 경계:** Phase05 §14 기본 Strafe Warping 비교 단위를 완료하고 사용자 요청으로 인수인계·커밋한다. 다음 발 배치 Control Rig 단위는 제안만 했고 조사·구현에 착수하지 않았다. 새 세션에서 해당 단위를 진행하라는 요청을 받으면 그 승인으로 시작한다. 이후에도 구현 단위마다 진행 확인을 받고 수동 조작이 빠르면 즉시 요청한다.
- **다음 첫 행동:** 필수 문서와 Phase05 §14 종료 판정을 읽고 `design/GASPReference.md` §2.12·Phase02의 2-5 발 배치 계약을 확인한다. 승인 후 기존 `/Game/Characters/ControlRigs/CR_Biped_FootPlacement_UEFN`과 로컬 UE 5.8 UAF의 Control Rig 연결·입력 전달 경로를 좁혀 대조한다. 지상 알파·착지 리셋·UseIKBoneTargets 끔·움직이는 base의 핀/루트 감쇠 끔·구면 Up 계약이 기준이다. 사본이 필요하면 UAFSpike에 두고 기존 ABP/CR·Chooser·PSD·GASP 소스를 수정하지 않는다. 공개 동등 경로가 없으면 구현 가능성과 한계를 먼저 판정한다.
- **완료 근거 재사용:** 전체 Editor Development 빌드 12 actions·88.71초·Succeeded, 구면 수치 회귀 1/1, AG/S 컴파일·저장, 북극/적도 0/1과 공중 해제/착지 복귀, Rewind task 순서를 확인했다. 코드 변경 없이 인계 때문에 빌드·녹화를 반복하지 않는다. 기본 Strafe 후보의 결과이며 기존 ABP의 0.8초 예측·샘플별 보정·발/루트 수학 전체가 동등하다는 뜻은 아니다. 상세 수치·시각 평가·단서와 남은 게이트는 §14가 소유한다.
- **에디터·시험 설정:** 인계 조회에서 IsPIERunning=true, Steering=1·StrafeWarping=1·OffsetRootBone.Enable=1(Enabled=true)·DebugLocomotion=2를 확인했다. 1인 PIE Standalone 네트워크 모드와 TestMap03 테스트 GameMode가 시험 기준이다. 에디터/PIE 종료·새 플레이·녹화·설정 변경은 하지 않았다. 일시정지 여부와 재생 헤드, CVar는 새 세션에서 다시 확인한다. 편집에 PIE 종료가 필요하면 에디터는 유지한 채 사용자에게 수동 종료를 요청한다. Slate observer는 모두 해제했고 이전 ref를 재사용하지 않는다.
- **로컬 기록:** 북극 비교 `Saved/RewindDebugger/RewindDebugger-LootNPop-Editor-20261025_184053031.utrace`(233,145,933바이트), 적도 최종 비교 `...20261026_172123536.utrace`(189,181,212바이트). 후자의 171910512·172025681 선행 짧은 녹화와 혼동하지 않는다. 공급 로그/집계는 `Saved/AnimationOverhaul/2026-10-26_StrafeComparisonResume/`와 `2026-10-26_StrafeEquator/`, 편집 전 AG는 `2026-10-25_PreStrafeWarping/`, 크래시 근거는 `2026-10-25_StrafeLiveCodingCrash/`에 있다. 존재·파일 크기를 확인했고 Saved는 git 제외라 다른 체크아웃에 따라가지 않는다. 기존 공중/대표 전환 녹화 `20261025_133940695.utrace`는 §12~§13 근거다.
- **기록 조회:** `UAFLocomotion → S_LocomotionSpike → UAFAnimGraph_* → AG_LocomotionSpike → EvaluationProgram → Tasks`에서 실제 task와 클립을 읽는다. 북극 비교는 약 37.1초, 적도 비교는 약 24.7초부터 Strafe 켬이다. 적도 마지막 공중/착지는 대략 63.0~63.8초다. Native Properties는 AG 공유 입력, 실제 검색은 UAF Pose Search의 Active Pose다. 빈 Native Properties를 입력 없음으로 해석하지 않는다. 큰 스크럽·트랙 선택·배선은 수동이 빠르면 즉시 요청한다.
- **빌드 주의:** 이번 Live Coding은 컴파일·AG/S 재컴파일 성공 뒤 완료 알림 Slate/mimalloc 경로에서 크래시했다. 근본 원인은 미확정이고 Strafe task 실행 전이었다. 구조체/trait 레이아웃을 바꾸는 다음 C++ 변경은 에디터 종료 후 정상 전체 빌드를 우선 고려하고, Build.bat은 처음부터 권한 확장 실행한다. 에디터 실행 중 전체 빌드와 Live Coding을 동시에 돌리지 않는다. 관련 근거는 §14에 있다.
- **커밋·제외 범위:** 이번 대상은 C++ 4파일(LNPSphericalOffsetRootTrait.cpp/.h, LNPUAFSpikeAnimInstance.cpp, LNPUAFSpikeRootInputs.h), AG_LocomotionSpike, Current·Roadmap·Phase05 실행서·Phase05 로그의 **9파일**이다. 기존 로컬 변경 4개(TestMap03와 데이터 에셋 3개)는 보존·제외한다. TestMap03의 테스트 Override는 로컬에 남아 다른 체크아웃에서는 다시 지정해야 하며, 원래 Override 구체 값은 확인 후 복원한다. 5-1 전체 미완료·5-2 미착수·Go/No-Go 보류다.

## 인계 근거

- 스켈레톤 결정 근거·전환 구조·GASP 외부 의존: `design/GASPReference.md` §2.7, B0 계산식·임계값: §2.8
- 0-1~0-3 수행 기록: `history/Phase00_Log.md`, 1C 기록: `history/Phase01C_Log.md`, 2-0a~2-6 기록: `history/Phase02_Log.md`, 3-1 기록: `history/Phase03_Log.md`
- 소스 에셋 확보 절차: `README.md` §4, 재생성 절차: `phases/Phase01C_GASPAssetRefresh.md`(Enemy·ISKM 사본), `phases/Phase02_GASPMoverABP.md`(Lyra 98개 + 2-0b 몽타주 정리)
- ISKM 에셋 요건·사본 규약: [TechDesign_EnemyNPC_LowLOD.md](../TechDesign_EnemyNPC_LowLOD.md) §6.7
- 엔진·GASP 사실 조사: `research/UE58_AnimationFeatures.md`
