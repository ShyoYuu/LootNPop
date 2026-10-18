# 애니메이션 개선 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: Phase 0·1A·1C 완료 — 다음 Phase 2 단위 2-0a(사용자 승인 후). 1B는 대기
> 마지막 갱신: 2026-10-18

## 현재 결정

확정 결정(상세 `Decisions.md`):

- GASP는 구성을 보며 직접 다시 만들고 소스 에셋만 들여온다(AN-001). GASP 에디터는 8001 읽기 전용(AN-009)
- 옛 GASP 유래 소스 에셋은 5.8 최신본으로 교체했다(AN-012, 1C 완료). 경로는 GASP 원본과 같은 `Content/Characters/UEFN_Mannequin/`이고, 다음 갱신도 덮어쓰기로 한다(AN-013)
- 로코모션 선택 코어는 MM 노드 방식(AN-015)
- **스켈레톤은 UEFN 전환(AN-011).** `AnimSourceMesh` = `SKM_UEFN_Mannequin`, 로코모션은 GASP 클립 그대로, Lyra 무기·몽타주 애니(실사용 91개)는 UEFN으로 리타깃한 사본을 쓴다. `VisualMesh`는 캐릭터별 메시 + UEFN→그 메시 리타기터. 실행은 2-0a·2-0b. 기준 스켈레톤은 GASP 5.8 `SK_UEFN_Mannequin` **93본**이다(88본 + 끝에 5본, 바인드 포즈 동일)
- **폴더·git(AN-016):** GASP 소스는 GASP 패키지 경로 그대로 두고 경로 단위로 git 제외. LootNPop 구성 에셋은 GASP 경로 밖(UEFN PSD 등은 `Content/Characters/MotionMatchingData/`). 리타깃 산출물은 `Content/Characters/Retargeted/`(git 제외)
- **노티파이(AN-017):** GASP 노티파이·PSD 하드 참조는 `DefaultEngine.ini`의 `PackageRedirects ... Removed=true`로만 끊는다(C++ 없음)
- **소스에 설정을 넣지 않는다(AN-018):** 소스는 저장하지 않는다. 필요한 설정은 `Retargeted/` 사본에 건다. ISKM 적은 `SKM_UEFN_Mannequin_Instanced`·`M_UEFN_Mannequin_Instanced`를 쓴다
- **포스트프로세스 ABP 끔(AN-019):** `SKM_UEFN_Mannequin`을 쓰는 컴포넌트마다 `bDisablePostProcessBlueprint = true`. 지금은 `BP_LNPEnemy` `VisualMesh`, 2-0b에서 새 `AnimSourceMesh`에도 넣는다
- 마커리스 소스 영상: 타사 영상은 프로토타입·레퍼런스만(AN-010)

## 바로 다음 행동 (갈래별 첫 행동)

| 갈래 | 첫 행동 | 건드리는 에셋 |
|:---|:---|:---|
| **2-0a Lyra 세트 UEFN 리타깃(다음 단위)** | 1C 완료(2026-10-18, [로그](history/Phase01C_Log.md)). ① 착수 직전에 `phases/Phase02_*.md`를 만들고 2-0a 절을 구체화한다(`Roadmap.md` Phase 2 표, `design/GASPReference.md` §2.7). ② 실사용 91개(서브 ABP 경로 37·몽타주 경로 55, 몽타주 21·AO 2 포함) 목록을 파일 단위로 다시 뽑아 확정한다. 0-3 집계 방법(`history/Phase00_Log.md` 0-3 항목 1, 추적 에셋에서 `get_dependencies` 추적)을 쓴다. ③ 리타기터를 고른다. 후보는 루트모션을 살리는 `RTG_Lyra_to_GASP`다. `_noRootMotion`은 ISKM 제자리 클립 전용이다. 그다음, MCP로 IK Retargeter 배치 리타깃이 되는지 시험한 뒤 `Retargeted/`에 산출한다. 대상 스켈레톤은 93본 `SK_UEFN_Mannequin`. **GASP 에디터(8001)는 필요 없다**(원본은 LootNPop의 Lyra 애니, 대상은 이미 반입된 UEFN 스켈레톤). LootNPop 에디터(8000)만 쓴다 | 새 리타깃 사본(`Retargeted/`, git 제외), UEFN 서브 ABP 5개(새 구성 에셋). 런타임 참조는 바꾸지 않는다(전환은 2-0b) |
| 1A 물리 히트리액트 스파이크 | **완료(2026-10-18).** 결과·Phase 3 인계는 [research/PhysicsHitReactSpike.md](research/PhysicsHitReactSpike.md) §6. 2-0b는 `ABP_Retarget`에 RBWC 자리를 남기고, UEFN 메시 적 포즈 복사를 Leader Pose가 아닌 ABP 노드로 한다(같은 문서 §1.5) | 스파이크 사본 `Content/Characters/PhysHitReactSpike/`(존치) |
| 1B 마커리스 모캡 스파이크 | 착수 직전에 `phases/Phase01B_*.md`를 만든다. 사용자가 Fab 마커리스 플러그인(약 5GB)을 설치하고 고정 카메라 영상 1클립을 고른다(AN-010). 리타깃 대상은 UEFN 스켈레톤(93본) | 새 시퀀스·몽타주(새 폴더). PIE 확인 시 `CHT_Montage` 행 하나 |

**한 번에 한 세션만 돌린다(사용자 요청).** 같은 에셋을 건드리는 단위가 겹치지 않게 하는 제약:

- 1B가 `CHT_Montage`에 테스트 행을 넣으면 2-0b와 같은 에셋이다. 2-0b 착수 전에 정리한다.
- 2-0b는 1A 스파이크 결과(`ABP_Retarget` RBWC 자리)를 반영한다.
- GASP 소스(`UEFN_Mannequin/`)는 어느 단위도 저장하지 않는다(AN-018). 되돌리기 백업: `Saved/AssetRefresh/2026-10-18/backup_pre1C3/`(1C-3 직전), `backup/`(그 이전 원본).

## 인계 근거

- 스켈레톤 결정 근거·전환 구조·GASP 외부 의존: `design/GASPReference.md` §2.7
- 0-1~0-3 수행 기록: `history/Phase00_Log.md`, 1C 기록: `history/Phase01C_Log.md`
- 소스 에셋 확보 절차: `README.md` §4, 재생성 절차: `phases/Phase01C_GASPAssetRefresh.md`
- ISKM 에셋 요건·사본 규약: [TechDesign_EnemyNPC_LowLOD.md](../TechDesign_EnemyNPC_LowLOD.md) §6.7
- 엔진·GASP 사실 조사: `research/UE58_AnimationFeatures.md`
