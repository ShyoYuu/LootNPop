# 애니메이션 개선 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: **Phase 5 착수, 5-0 완료·5-1 진행 중.** 급전환 방향 추종 실패는 Offset Root 켬에서 재현·끔에서 미재현. Steering 누락 보완 단위 진행 승인 대기. Phase 0·1A·1C·2·3 완료, 1B는 대기
> 마지막 갱신: 2026-10-24

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

**Phase 5 선택, 5-0 완료(2026-10-23).** 실행서는 [Phase05_UAFSpike.md](phases/Phase05_UAFSpike.md), 수행 기록은 `history/Phase05_Log.md`다. GASP `/Game`의 UAF·AnimNext 클래스 조회 결과는 `[]`였으며, 현재는 LootNPop 테스트 사본으로 시험한다. **최종 일곱 게이트는 아직 통과하지 않았고 Go/No-Go는 보류다. 5-1은 미완료이며 5-2에 착수하지 않는다.**

**5-1 현재:** 숨김 `ABP_SourceUAFSpike` 공급층에서 기존 Chooser·Mover 궤적을 평가해 공유 Databases·Trajectory·InterruptMode·BlendArguments를 UAF에 전달한다. 네 Getter 배선·런타임 `inputs=ready`·UAF 표시 메시의 Idle·기본 이동 전환은 확인했다. S의 메시 locator는 `Current Actor → VisualMesh`, Pose History 수집은 본 4개와 Phase 커브다. Rewind 로컬 저장·빈 트랙 문제는 해결했다(1인 PIE Net Mode=`Play Standalone`). 기존 녹화의 `UAFLocomotion → Pose Search`에서 달리기·Idle의 실제 선택 DB/클립을 읽고 과거 달리기 구간 재조회도 확인했다. 입력 PSD 배열의 시점별 대조·전환/인터럽트·구면 루트·발 배치·공중 고정·움직이는 base는 미검증이다. GASP는 필요 없다.

### 새 세션의 첫 행동

1. CLAUDE.md와 이 문서, Phase05 실행서 및 수행 로그의 마지막 기록을 읽는다. 로그는 시간순이므로 중간의 대기 문구를 최신 상태로 해석하지 않는다.
2. **다음 단위는 Steering 보완·급전환 재검증이며 아직 승인받지 않았다.** 사용자에게 계속 진행할지 확인한 뒤 Phase05 §7의 조사 시작점에서 이어간다. 구면 보정 trait·RigVM 등록·포즈/입력 배선·AG/S 컴파일은 완료했지만 런타임 방향 문제가 남았다. 최신 Development 빌드 성공(5 actions, 48.13초), AG/S 최종 컴파일 성공·전체 구조체 핀 연결을 확인했다. 빌드를 반복할 필요는 없으며 새 코드 변경 후 필요한 빌드만 한다.
3. 첫 수동 시험에서 사용자는 이동→정지·180° 급반전·점프/착지를 수행했으며, 급전환 시 룩 회전량만큼 표시 캐릭터가 돌지 않아 전방 달리기 포즈로 옆/대각선 이동하는 문제를 쉽게 재현했다고 보고했다. 녹화 `RewindDebugger-LootNPop-Editor-20261024_145134378.utrace`(171,067,599바이트)는 Saved/RewindDebugger에 남았다. 후속 CVar 비교에서 사용자는 `a.UAF.OffsetRootBone.Enable 1`일 때 재현·`0`일 때 미재현을 확인했다. 로그의 14:57:15 KST 0·14:57:46 1 실행과 MCP의 최종 1 값을 확인했다. 현재 PIE는 꺼져 있다. 문제 범위는 Offset Root 적용 경로로 좁혔고, 기존 ABP의 Accumulate와 짝인 Steering이 최소 UAF에 없는 것이 유력하다. 단일 원인/동등성 해결 확정은 아니다. **다음 승인 단위:** UAF 자신의 누적 루트·궤적 목표·현재 애니 평가를 기준으로 Steering 연결 경로를 보완하고 빌드/그래프 컴파일·동일 급전환 재검증. 숨김 ABP 루트를 대입하거나 회전 모드만 바꿔 통과 처리하지 않는다. 시작 위치의 극/적도 구분·보정 수치·base는 미확인, 적도/base 시험은 방향 문제 수정 후 재개한다. Trace는 프로젝트 Saved/RewindDebugger만 사용하며 리슨 2인 회귀는 별도다.
4. TestMap03의 GameMode Override는 BP_GameModeUAFSpike 임시 설정 상태다. 이 맵은 기존 사용자 변경과 섞여 있어 이번 커밋에서 제외하고 로컬에 유지한다. 다른 체크아웃에서는 시험 전에 World Settings의 Override를 테스트 사본으로 지정해야 한다. 5-1 종료 시 원래 값으로 복원하되 맵 전체를 되돌리지 않는다. 원래 Override의 구체 값은 현재 기록에서 확인되지 않아 복원 전 사용자 메모·기준값을 확인한다. 다음 구현 단위마다 진행 확인을 받고, 수동 조작이 더 효율적이면 즉시 요청한다.

별도 미해결: 리슨 2인 최종 회귀에서 기존 적 Mover의 `UpDirection=0` 경고 410회가 재현됐다. 물리 켬/끔을 새로 통제한 성능 비교나 동일 Actor 풀 재사용의 직접 추적은 하지 않았다. 이 한계와 NPC 손의 큰 물리 추종 지연은 연구 §10·Phase 3 로그에 남겼다. 완료한 수동 회귀·실측 결과를 넘어 일반화하지 않는다.

| 갈래 | 첫 행동 | GASP 에디터(8001) | 건드리는 에셋 |
|:---|:---|:---|:---|
| ISKM 총기 적 재베이크 | [TechDesign_EnemyNPC_LowLOD.md](../TechDesign_EnemyNPC_LowLOD.md) "미해결 — ISKM 총기 적" 절대로 `Retargeted/Enemy/` 총기 클립을 다시 굽는다(정렬하지 않은 리타깃 포즈 + 견착 보정, 왼손 IK를 그립 오프셋으로 굽기). Phase 2와 독립 | 불필요 | `Retargeted/Enemy/`(git 제외), ISKM 베이크 스크립트 |
| 1B 마커리스 모캡 스파이크 | `phases/Phase01B_*.md`를 만든다. 사용자가 Fab 마커리스 플러그인(약 5GB)을 설치하고 고정 카메라 영상 1클립을 고른다(AN-010). 리타깃 대상은 UEFN 스켈레톤(93본) | 불필요 | 새 시퀀스·몽타주(새 폴더), PIE 확인 시 `CHT_Montage` 행 하나 |
| Phase 5 UAF 판정 스파이크(선택됨) | `phases/Phase05_UAFSpike.md` §7의 Steering 보완·급전환 재검증 승인부터. Offset Root 1에서 재현·0에서 미재현 | 불필요 | `/Game/Characters/UAFSpike/`와 스파이크 C++. 기존 ABP는 건드리지 않는다 |

**한 번에 한 세션만 돌린다(사용자 요청).** 같은 에셋을 건드리는 단위가 겹치지 않게 하는 제약:

- 1B·Phase 4는 `CHT_Montage`(몽타주)만, Phase 3-2는 표시 ABP 2개·히트리액트 경로·`CHT_Montage` HitReaction 행을 건드린다(1B·4와 같은 에셋이라 동시에 돌리지 않는다). 로코모션(`ABP_Main_UEFN`·`CHT_PoseSearchDatabases_UEFN`)은 Phase 2로 끝났고, Phase 5는 새 폴더에서 시험하므로 기존 ABP와 겹치지 않는다.
- `Retargeted/Lyra/`를 다시 만들 때는 `Scripts/AnimRetarget/RetargetLyraToUEFN.py`를 쓴다(엔진 문제로 Lyra 원본이 메모리에서 바뀌는 것을 스크립트가 되돌린다). 배치를 돌린 세션에서 Lyra 원본을 저장하지 않는다.
- `Retargeted/Lyra/`를 다시 만들면 `FixRetargetedMontages.py`도 이어서 돌린다(대시 블렌드 프로필 ensure 방지).
- GASP 소스(`UEFN_Mannequin/`)는 어느 단위도 저장하지 않는다(AN-018). 되돌리기 백업: `Saved/AssetRefresh/2026-10-18/backup_pre1C3/`(1C-3 직전), `backup/`(그 이전 원본).

### 5-1 현재 구성과 검증 한계

- 테스트 에셋 5개는 `Content/Characters/UAFSpike/`, 입력 공급층·구면 보정 코드는 `Source/LootNPop/Animation/UAFSpike/`다. 숨김 ABP 공급층과 UAF가 함께 돌아 성능 비교에 쓰지 않는다. 시스템 메시 locator는 `Current Actor → VisualMesh`, Pose History는 본 4개와 `Phase` 커브를 수집한다. `FoleyEvent: Walk/Run` 미처리 경고는 잔류한다.
- 현재 AG는 MM/Pose History → Offset Root Bone + LNP Spherical Offset Root → Output. 공유 OffsetRoot Getter는 Root Inputs 구조체 전체에 연결했다. 이동/회전 속도 클램프는 꺼짐. 같은 기본 스택에 프로젝트 보정 trait를 넣어야 하며 UAFWarping 내부 상태 의존성이 있다. RigVM 네이티브 구조체 등록도 게임 모듈 시작에 필요하다.
- Rewind 저장 보완·ListenServer 빈 시간축 원인·Standalone 비교·UAF 달리기/Idle DB/클립·과거 시점 재조회는 확인했다. 기록/백업은 로컬 Saved에만 있고 git에 포함하지 않는다. 세부 이력은 `history/Phase05_Log.md`, 구현과 다음 조사 시작점은 Phase05 §6·§7이다.
- 최신 CVar 비교로 급전환 문제가 남아 있다. Steering·OrientationWarping·ResetRoot/CopyBone·발 배치가 최소 UAF 그래프에 아직 없으며, 공중 Fall_Loop 고정·입력 PSD 배열 시점별 대조·전환/인터럽트·구면/base·적·2인 전체 회귀도 미완료다. Go/No-Go 보류, 5-2 미착수.
- 이번 커밋에서 제외하는 기존 로컬 변경: `Content/Maps/TestMap03.umap`, `Content/Gameplay/DT_PlayerBaseStats.uasset`, `Content/ItemData/Weapons/Levels/DT_LongSword_Levels.uasset`, `Content/SpawnData/DA_MassSpawnConfig_TestOnePerSpecies.uasset`. TestMap03의 임시 테스트 Override도 로컬에 남아 있으므로 다른 체크아웃에서 시험할 때 다시 지정한다.

## 인계 근거

- 스켈레톤 결정 근거·전환 구조·GASP 외부 의존: `design/GASPReference.md` §2.7, B0 계산식·임계값: §2.8
- 0-1~0-3 수행 기록: `history/Phase00_Log.md`, 1C 기록: `history/Phase01C_Log.md`, 2-0a~2-6 기록: `history/Phase02_Log.md`, 3-1 기록: `history/Phase03_Log.md`
- 소스 에셋 확보 절차: `README.md` §4, 재생성 절차: `phases/Phase01C_GASPAssetRefresh.md`(Enemy·ISKM 사본), `phases/Phase02_GASPMoverABP.md`(Lyra 98개 + 2-0b 몽타주 정리)
- ISKM 에셋 요건·사본 규약: [TechDesign_EnemyNPC_LowLOD.md](../TechDesign_EnemyNPC_LowLOD.md) §6.7
- 엔진·GASP 사실 조사: `research/UE58_AnimationFeatures.md`
