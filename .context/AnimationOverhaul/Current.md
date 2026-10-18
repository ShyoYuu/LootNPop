# 애니메이션 개선 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 작업: Phase 0 완료 — Phase 1A·1B·1C 착수 대기(병렬 가능, 사용자 승인 후)
> 마지막 갱신: 2026-10-18

## 현재 결정

코드·LootNPop 에셋 변경은 아직 없다. 확정 결정(상세 `Decisions.md`):

- GASP는 구성을 보며 직접 다시 만들고 소스 에셋만 들여온다(AN-001). GASP 에디터는 8001 읽기 전용(AN-009)
- 옛 GASP 유래 소스 에셋은 5.8 최신본으로 교체하고(AN-012), GASP 원본 경로로 옮긴 뒤 덮어쓴다(AN-013)
- 로코모션 선택 코어는 MM 노드 방식(AN-015)
- **스켈레톤은 UEFN 전환(AN-011).** `AnimSourceMesh` = `SKM_UEFN_Mannequin`, 로코모션은 GASP 클립 그대로, Lyra 무기·몽타주 애니(실사용 91개)는 UEFN으로 리타깃한 사본을 쓴다. `VisualMesh`는 캐릭터별 메시 + UEFN→그 메시 리타기터. 실행은 1C 뒤 2-0a·2-0b
- **폴더·git(AN-016):** GASP 소스는 GASP 패키지 경로 그대로 두고 경로 단위로 git 제외. LootNPop 구성 에셋은 GASP 경로 밖. 리타깃 산출물은 `Content/Characters/Retargeted/`(git 제외). AN-014도 이것으로 해소됐다
- **노티파이(AN-017):** 커브 압축 설정·`AM_*`는 반입, Foley·EarlyTransition은 반입하지 않고 `CoreRedirects` → no-op C++ 클래스
- 마커리스 소스 영상: 타사 영상은 프로토타입·레퍼런스만(AN-010)

⚠️ `GASP_Mannequin/Animations/Enemy/*`(리타깃 적 애니 14개)는 git에 없다. 1C-2에서 `Retargeted/`로 옮기기 전까지 지우거나 덮어쓰지 않는다.

⚠️ GASP 에디터의 `SandboxCharacter_Mover_ABP`·`SandboxCharacter_CMC_ABP`가 dirty다(`read_graph_dsl` 탓). 닫을 때 저장하지 않는다.

## 바로 다음 행동 (갈래별 첫 행동)

| 갈래 | 첫 행동 | 건드리는 에셋 |
|:---|:---|:---|
| 1C 소스 에셋 최신화 | 1C-3 완료(2026-10-18, [로그](history/Phase01C_Log.md)). `UEFN_Mannequin/`은 이제 GASP 5.8 본(369개 해시 일치)이고, `DefaultEngine.ini`에 `PackageRedirects Removed` 16줄이 들어갔다. 재시작 로드 경고 0, 깨진 참조 0(Removed 16과 기존 Lyra `AN_PlayWeaponMontage`만), UEFN PSD 4개 재빌드 성공. ⚠️ 사용자 PIE에서 **ISKM 적 몸통이 T 포즈**다(무기·Actor 적은 정상). 원인은 덮어쓰기로 사라진 소스 설정 2건이다(`SKM_UEFN_Mannequin` `bOptimizeForInstancing`, `M_UEFN_Mannequin` `bUsedWithInstancedStaticMeshes`, [로그](history/Phase01C_Log.md) 1C-3 §4). **다음 [1C-4](phases/Phase01C_GASPAssetRefresh.md) 회귀 검증:** ⓪ 두 설정의 복구 방식을 사용자와 확정한다(제안: 소스에 다시 적용 + 반입 후처리로 문서화). 그 뒤 ① TestMap03 PIE에서 ISKM 적(몸통·무기 메시 표시, 애니 재생)과 승격 `BP_LNPEnemy`(로코모션·공격·랙돌)를 1C-2 기준점과 비교한다. 새 메시는 정점 수가 바뀌었으니 `ASTP_Enemy_UEFN_*`·`SM_UEFN_Mannequin` 재베이크 필요 여부부터 본다 ② 새 `SKM_UEFN_Mannequin`의 포스트프로세스 ABP(머리 1.15·허벅지 1.12 스케일)를 받을지, `bDisablePostProcessBlueprint`로 끌지 사용자와 정한다 | 1C-4는 에디터 PIE만. 소스 에셋은 저장하지 않는다(해시 대조 유지). 되돌리기: 덮어쓰기 전 332개 `Saved/AssetRefresh/2026-10-18/backup_pre1C3/`, 그 전 원본 `backup/` |
| 1A 물리 히트리액트 스파이크 | **완료(2026-10-18).** RBWC 노드 방식, 피격 구간에만 알파. 결과·Phase 3 인계는 [research/PhysicsHitReactSpike.md](research/PhysicsHitReactSpike.md) §6. 다음은 Phase 3(1A + 2 완료 후). 2-0b는 `ABP_Retarget`에 RBWC 자리를 남기고, UEFN 메시 적 포즈 복사를 Leader Pose가 아닌 ABP 노드로 한다(같은 문서 §1.5) | 스파이크 사본 `Content/Characters/PhysHitReactSpike/`(존치, 사유는 `phases/Phase01A_PhysicsHitReactSpike.md`). `ABP_Retarget` 원본은 손대지 않음 |
| 1B 마커리스 모캡 스파이크 | 착수 직전에 `phases/Phase01B_*.md`를 만든다. 사용자가 Fab 마커리스 플러그인(약 5GB)을 설치하고 고정 카메라 영상 1클립을 고른다(AN-010). 리타깃 대상은 UEFN 스켈레톤. PIE 1타 확인 시점은 `Roadmap.md` Phase 1B 참고 | 새 시퀀스·몽타주(새 폴더). PIE 확인 시 `CHT_Montage` 행 하나 |

**동시 실행 제약 — 같은 에셋을 건드리는 단위끼리는 동시에 돌리지 않는다.**

- 1C-2(폴더 이동) 동안 다른 갈래는 `BP_LNPEnemy`·적 DA·ASTP·`Retargeters/*`·`GASP_Mannequin` 하위를 편집하지 않는다. 이동이 참조자를 다시 저장한다.
- 1C-3은 LootNPop 에디터를 끈다. 에디터(MCP 8000)는 갈래들이 함께 쓰므로 그동안 다른 갈래는 에디터 작업을 멈춘다. C++ 빌드도 겹치지 않게 한다.
- 1A가 `ABP_Retarget`에 노드를 넣으면 2-0b와 같은 에셋이다. 1A 결과를 2-0b 전에 커밋하거나 테스트 사본에서 한다.
- 1B가 `CHT_Montage`에 테스트 행을 넣으면 2-0b와 같은 에셋이다. 2-0b 착수 전에 정리한다.
- Phase 2(2-0a부터)는 1C가 끝난 뒤 시작한다.

## 인계 근거

- 스켈레톤 결정 근거·전환 구조·GASP 외부 의존: `design/GASPReference.md` §2.7
- 0-1~0-3 수행 기록: `history/Phase00_Log.md`
- 소스 에셋 확보 절차: `README.md` §4
- 엔진·GASP 사실 조사: `research/UE58_AnimationFeatures.md`
