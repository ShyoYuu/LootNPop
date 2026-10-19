# 애니메이션 개선 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: Phase 0·1A·1C 완료, Phase 2 단위 2-0a·2-0b·2-1 완료 — 다음 2-2(사용자 승인 후 착수). 1B는 대기
> 마지막 갱신: 2026-10-19

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
- **빌드:** GASP 에디터만 떠 있어도 LootNPop `Build.bat`이 Live Coding 검사에 막힌다(같은 엔진 실행 파일). LootNPop 에디터를 끈 뒤 `-NoHotReloadFromIDE`를 붙여 빌드한다. GASP 에디터는 끄지 않아도 된다

## 바로 다음 행동 (갈래별 첫 행동)

| 갈래 | 첫 행동 | 건드리는 에셋 |
|:---|:---|:---|
| **2-2 B1(다음 단위)** | 2-1 완료(2026-10-19, [로그](history/Phase02_Log.md)). 착수 직전에 `phases/Phase02_GASPMoverABP.md`에 2-2 절을 만든다. ① GASP `CHT_PoseSearchDatabases_Relaxed` 8행의 결과 PSD 묶음을 확인한다(행 결과는 MCP로 안 읽혀 사용자 스크린샷 1장, §2.6). ② GASP 5.8 방향별 Walk·Run 루프(스트레이프, 2-0b 알려진 차이)를 GASP 원본 경로 그대로 반입한다(LootNPop 에디터 끈 상태에서 파일 복사, AN-013·AN-016). ③ `CHT_PSD_UEFN`을 상태별 PSD 묶음 Chooser로 교체한다. 행 입력은 2-1의 `ELNP*` enum(이동 모드·이동 상태·보행·회전 모드, 스트레이프는 `RotationMode`). DB 인터럽트 모드·블렌드 시간은 GASP `Get_MMInterruptMode`·`Get_MMBlendTime`(§2.8)을 C++ 함수로. 승격 적의 `MovementState` 깜빡임(§2.8 끝)을 고려해 `_Recent` 사용 여부를 정한다. **GASP 에디터(8001)가 필요하다**(읽기 전용, Chooser·PSD 구성 확인). AN-007 공중 고정 유지 | `CHT_PSD_UEFN`(또는 새 Chooser), `Characters/MotionMatchingData/` PSD, `ABP_Main_UEFN` `OnUpdate_MotionMatching`, `ULNPAnimInstance`(함수 추가), 반입 시퀀스(`UEFN_Mannequin/`, git 제외) |
| 1A 물리 히트리액트 스파이크 | **완료(2026-10-18).** 결과·Phase 3 인계는 [research/PhysicsHitReactSpike.md](research/PhysicsHitReactSpike.md) §6. 2-0b가 RBWC 자리를 남겼다: 플레이어 `ABP_Retarget`, UEFN 메시 적은 Leader Pose가 아닌 ABP 노드 `ABP_VisualCopyPose`(같은 문서 §1.5) | 스파이크 사본 `Content/Characters/PhysHitReactSpike/`(존치) |
| 1B 마커리스 모캡 스파이크 | 착수 직전에 `phases/Phase01B_*.md`를 만든다. 사용자가 Fab 마커리스 플러그인(약 5GB)을 설치하고 고정 카메라 영상 1클립을 고른다(AN-010). 리타깃 대상은 UEFN 스켈레톤(93본) | 새 시퀀스·몽타주(새 폴더). PIE 확인 시 `CHT_Montage` 행 하나 |

**한 번에 한 세션만 돌린다(사용자 요청).** 같은 에셋을 건드리는 단위가 겹치지 않게 하는 제약:

- 1B가 `CHT_Montage`에 테스트 행을 넣으면 2-2(Chooser 재구성)와 겹칠 수 있다. 1B 착수 시 정한다.
- `Retargeted/Lyra/`를 다시 만들 때는 `Scripts/AnimRetarget/RetargetLyraToUEFN.py`를 쓴다(엔진 문제로 Lyra 원본이 메모리에서 바뀌는 것을 스크립트가 되돌린다). 배치를 돌린 세션에서 Lyra 원본을 저장하지 않는다.
- Phase 3의 RBWC는 두 표시 ABP(`ABP_Retarget`·`ABP_VisualCopyPose`)의 왼손 IK 뒤, 출력 앞에 넣는다(2-0b에서 자리 확보).
- **ISKM 총기 적 견착·왼손 불일치(별도 세션, 2-0b 이전부터 있던 문제):** 구운 `Retargeted/Enemy/` 클립을 정렬한 리타깃 포즈로 다시 굽고 왼손 IK를 그립 오프셋으로 굽는다 → [TechDesign_EnemyNPC_LowLOD.md](../TechDesign_EnemyNPC_LowLOD.md) "미해결 — ISKM 총기 적" 절
- **알려진 차이:** 스트레이프 방향 클립이 없어 총기·락온 이동도 정면 달리기로 보인다 → 2-2에서 GASP 방향별 루프 반입과 함께 복구(사용자 결정 2026-10-19). Orientation Warping(허리 돌리기)은 2-3.
- `Retargeted/Lyra/`를 다시 만들면 `FixRetargetedMontages.py`도 이어서 돌린다(대시 블렌드 프로필 ensure 방지).
- GASP 소스(`UEFN_Mannequin/`)는 어느 단위도 저장하지 않는다(AN-018). 되돌리기 백업: `Saved/AssetRefresh/2026-10-18/backup_pre1C3/`(1C-3 직전), `backup/`(그 이전 원본).

## 인계 근거

- 스켈레톤 결정 근거·전환 구조·GASP 외부 의존: `design/GASPReference.md` §2.7, B0 계산식·임계값: §2.8
- 0-1~0-3 수행 기록: `history/Phase00_Log.md`, 1C 기록: `history/Phase01C_Log.md`, 2-0a·2-0b·2-1 기록: `history/Phase02_Log.md`
- 소스 에셋 확보 절차: `README.md` §4, 재생성 절차: `phases/Phase01C_GASPAssetRefresh.md`(Enemy·ISKM 사본), `phases/Phase02_GASPMoverABP.md`(Lyra 98개 + 2-0b 몽타주 정리)
- ISKM 에셋 요건·사본 규약: [TechDesign_EnemyNPC_LowLOD.md](../TechDesign_EnemyNPC_LowLOD.md) §6.7
- 엔진·GASP 사실 조사: `research/UE58_AnimationFeatures.md`
