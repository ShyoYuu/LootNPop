# Phase 1-BR U4-10 채택·U4-11 새 세션 인수인계

> 상태: U4-0~10 완료·사용자 채택 · V010 동결 · U4-11 남동·북동 외곽은 새 세션에서 시작(미착수)
> 갱신: 2026-11-25
> 기준: [U4 계획 §4·§13~17](Phase01_U4LargeLayoutPlan.md) · [GX-034/035](../Decisions.md) · [Phase §2.1·U4](../phases/Phase01_RidgeReferencePatch.md)

## 1. 종료 지점

사용자가 U4Layout_V010을 채택하고 **U4-11부터 새 세션으로 넘기도록 지시했다. U4-11/V011은 시작하지 않았다.** 이번 커밋은 이전 `35657fe` 이후 U4-4~10 저작 JSON·보존/비교/제보 보정 기록·문서 정리까지 포함한다. 정확한 커밋은 `git log -1`로 확인한다. Content/Saved는 기존 저장소 정책에 따라 로컬에 보존하며 Git에는 강제 추가하지 않는다.

| 단위 | 채택 버전 | 구현 범위 | 추가 소유자 누계 |
|:---|:---|:---|---:|
| U4-0~3 | V001~V003 | Z1 보존 조사·Z2 하류/석교·Z4 메사 북면 | 5 |
| U4-4 | V004 | 메사 오름길/남쪽 외곽 | 7 |
| U4-5 | V005 | 북쪽 단 뒤/안부 | 9 |
| U4-6 | V006 보정본 | 서쪽 접근2개, 경사로 제보 뒤 자세 보정·24공간 PASS | 11 |
| U4-7 | V007 | 북 대지 동쪽 절벽/북쪽 암괴 | 13 |
| U4-8 | V008 | 골짜기 남쪽 측벽/북동쪽 어깨 | 15 |
| U4-9 | V009 | 보스 고원 남쪽 측벽/북쪽 어깨 | 17 |
| U4-10 | V010 | 남서 진입 바깥/북서 저지대 어깨 | 19 |
| U4-11 | V011 예정 | 남동·북동 외곽 | 미착수 |

U4 전체 큰 배치는 미완료다. 전체 초벌 검토/확정 → U5a Z1 몸체 시험 → U5b 구역별 몸체/충돌 → U6 전체 베이크/PIE → U7 세부 마감 순서를 유지한다.

## 2. 실제 에디터와 최종 근거

현재 맵은 `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/U4Layout_V010/LVI_Ridge_EDTerrain_U4Layout_V010`이다. **원래 `Z1Bake_V010`과 다른 U4 검토 맵**이다. 종료 직전 읽기 전용 `LiveHandoff.json`: PIE=false·추가19자세 일치·관련25에셋 dirty없음. 사용자 선택 `StaticMeshActor_2` 1개와 카메라를 유지했다. 현재 카메라 위치는 `(8452.889589,8452.889589,1047.528137)`, 회전 `(12.430340,45,0)`°다. 다음 세션은 다시 실제 상태를 읽는다.

- 실제 export: `Saved/Profiling/Graphics/Phase01/U4_10_V010/Export_20261125_021731_356481.json`. PASS·issues/dirty없음·exported=true.
- 기존 Z1 지문185,555인스턴스·데칼12·428키, SHA-256 `853681a3ea1bdb4c002491d0ef4dd484f1723dac18a1915a7d61e8d027527b88` 불변. 기존 메시8·Cull LI15와 채택17소유자·복구51파일도 불변이다.
- Pod10·훅6·검토점27·LI/로드34개. 신규2개는8,723+830인스턴스·데칼0·NoCollision이며 각46공간·15,286표본 겹침0이다. 메시 로컬 상자+50cm 표본 검사이며 충돌/PIE 통과가 아니다.
- 경고1건은 기존 Z1 북벽의 비Static `Actor_0.Scene`→Static `InstancedStaticMesh797` AttachTo다. 로드 시 기존 Shell459,537삼각형 Navigation export 경고도 과거에 있었다. 새 배치 경고나 수정 대상이라고 혼동하지 않는다.
- Unlit/FOV90°/1127×724 비교25시점: `Comparison.html`, `ComparisonCameras.json`, `CaptureMetadata_Draft.json`. 새 외곽의 이전 화면5개는 `Views_Before/`, 다른20개는 V009 `Views_Draft/`를 쓴다.
- 신규2개가 함께 보이는 저장 화면은 `Views_Draft/SelectedWestOuterOverview.png`와 `SelectionCamera.json`(H18000)이다. 비교용 WestOuterOverview(H13000)와 다르다. 현재 사용자 카메라를 임의로 덮어쓰지 않는다.
- `FinalValidation_V001.json`은 채택 전 검사 기록이다. 채택 뒤 메타데이터/파일 해시는 **`AcceptanceValidation_V001.json`·`AcceptedHandoffHashes_V001.json`**이 소유한다. `AcceptedHandoff/`에21파일 복구본이 있고 `BeforeAcceptance_V010/`에 채택 전 저작8파일을 보존했다.

## 3. 동결할 채택19소유자

저작 원본은 `Scripts/Profiling/Graphics/LargeLayout_V010/`의 manifest와 Z2/Z3/Z4/Z5/Z6/Z7 JSON이다. 모든 크기는1, tilt 약±1e-6°는 현지 축 오차다. 원본 조립체 내부 큰 바위/식생 층도 포함해 보존한다. H는 구면 현지 Up 방향 높이(cm)다.

| 구역 | 소유자 | X / Y / H(cm) | yaw | 인스턴스 | 결과 지문 |
|:---|:---|:---|:---|---:|:---|
| Z2 | ASM_Z2_LowerSouthBank | 1200 / -5800 / -6500 | 135° | 8,206 | `1ebc931549161469de67d3ebeb2f55cb6b100f2d7281d2fda6ff0e410400bff7` |
| Z2 | ASM_Z2_BridgeSouthButtress | -7850 / -9800 / -600 | 100° | 9,148 | `6ba107dde77a0c56b3c05e4b7617c4324af01a2bce464de55515578584ab0b2b` |
| Z2 | ASM_Z2_UpperSouthShoulder | -12900 / -9800 / -1600 | 80° | 830 | `a47ca98c89b19d193ea9860f79173622a0c6595ead1597ee7dd9466b34bd7140` |
| Z4 | ASM_Z4_MesaNorthEastWall | -1800 / -10000 / -700 | 80° | 8,723 | `629396ff51f02c29841f1e376cc8e79cde23dab3ff690b90c6922307b5dc01f4` |
| Z4 | ASM_Z4_MesaEastCrown | -2600 / -11800 / 1000 | 25° | 505 | `824504274c9a9fd8b4e7738a493f6fd11243f439b634b43e47749a6599bb9e40` |
| Z4 | ASM_Z4_MesaRampToe | -9900 / -15100 / -1400 | -90° | 830 | `1afb1d255002421a93e3ad57cd4c2df4a7658af97b2440929365b79fe56f362a` |
| Z4 | ASM_Z4_MesaSouthWall | -6500 / -14500 / -1200 | -90° | 8,206 | `910b0353d1a9386cfc105493c9d7c5be4947dc7a39dfe9c6bec85febd0d67c8a` |
| Z3 | ASM_Z3_NorthRearShoulder | -5600 / 6200 / 200 | -30° | 830 | `a3a0744155971e685bf21572a51946541ba9ef00c82e076253f6e9f54ea61ce5` |
| Z3 | ASM_Z3_SaddleSouthEast | -3100 / 8000 / 0 | 50° | 505 | `ae5e4c005a99b287f9c86c75bba8afbe7b5ea2d766ccec78a659c7a32ab079f7` |
| Z3 | ASM_Z3_WestFootShoulder | -13200 / 400 / -1600 | 160° | 830 | `b31e75386fc05966ddd915367631a5a5587238afe280e9dfd731746247543e87` |
| Z3 | ASM_Z3_WestUpperShoulder | -9000 / 1600 / -300 | 150° | 505 | `5a737c76c79ba857f9ba0c9175a27a32689788ba8f1ef79d56583ce164d3d3b5` |
| Z5 | ASM_Z5_TablelandEastRim | 7500 / 9500 / -1800 | 15° | 8,723 | `c1abf4a3f82e877b3d4a43de4ebf005b5e3b8075920f55114aad7fa889a9940a` |
| Z5 | ASM_Z5_TablelandNorthRim | 2500 / 13500 / -1500 | -45° | 830 | `6348c1f7f2c47281bc06e3f73b8c7dd45ecf1326a1d113171214c80b3a9779fe` |
| Z6 | ASM_Z6_ValleyLowerSouth | 5400 / -3000 / -5900 | 90° | 8,206 | `4ac37bb935d265e78b12f85bed3aa5d3e81aa76fcd31a6aba6dce75eafbc091b` |
| Z6 | ASM_Z6_ValleyUpperNorthEast | 11200 / 2800 / -3000 | -135° | 505 | `1c9ed058b5182f8572fe04affb495e5562da909e3dfc91b9ee4ac89b3a2acd5e` |
| Z6 | ASM_Z6_BossSouthWall | 13900 / -3200 / -3300 | 30° | 8,723 | `d92446503f6e07b4163ddec7bae2947d1e3ca7f2b6d6e4d3985654229d5c5eab` |
| Z6 | ASM_Z6_BossNorthShoulder | 14900 / 3200 / -1600 | -60° | 830 | `3860365a638385b73d076c43e07ddb9c21f21c43728aed5aa2737b3da11584fd` |
| Z7 | ASM_Z7_SouthwestEntryShoulder | -16000 / -13400 / -3500 | 30° | 8,723 | `1bb96c5fecd73677a1d84e69d94ac29c327468a498e92f3795fd1f08c06e531c` |
| Z7 | ASM_Z7_NorthwestEntryShoulder | -15700 / 11500 / -1700 | -60° | 830 | `62808de20b3c2de05243b3e833546442a1b2d861aa42f99b742037fa25d6b234` |

참조 패키지와 원본/사본 해시는 각 JSON의 `level`·`source_level`·`*_sha256`를 따른다. 기존 V001~V010·원본/공유 메시·재질·BPP는 수정하지 않는다. Z1 V037 JSON·Z1Bake_V010·Cull V001·렌더링 지각V012·충돌 지각V013·프록시V003·V010 SurfaceData·동굴/석교/부유섬도 보존한다. Cull 사본15개는 월드 자세가 이미 구워져 있으므로 LI identity를 유지하고 원본 로컬 자세를 다시 적용하지 않는다.

## 4. 새 V011의 이전 채택본 갱신

V010 manifest의 `previous_unit`은 **V009 제작 이력(17소유자·45저작파일)**이다. 그대로 복사하면 Z7 두 소유자를 중복 추가할 수 있다. 다음 세션에서 실제 상태를 대조하고 새 판을 만들 때 다음을 적용한다.

1. V010 맵을 새 `U4Layout_V011/LVI_Ridge_EDTerrain_U4Layout_V011`로 복제한다. 새 `LargeLayout_V011/`에서 기존6구역 JSON을 독립 사본으로 둔다.
2. `previous_unit.level`·맵 해시·`accepted_report`를 채택 V010으로 갱신한다. `locked_owners`는 최종 export의 **19개 전부(Z2 3·Z3 4·Z4 4·Z5 2·Z6 4·Z7 2)**와 실제 자세/참조/지문으로 채운다.
3. 누적 이전 저작45파일에 채택 V010의8파일을 더해 **53파일**로 고정한다. `AcceptedHandoffHashes_V001.json`의 현재 해시를 먼저 대조한다.
4. 새 U4-11 소유자만 Z7에 추가하고 새 전용 사본으로 작업한다. 기존 생성기/전체 살포/Cull/지각·충돌·베이크를 준비 목적으로 실행하지 않는다. `build`/저장/재생성을 기존 버전에 다시 실행하지 않는다.

## 5. U4-11 첫 범위와 보호 조건

다음은 **남동·북동 외곽**이다. 실제 bounds·곡률·원본 팔레트를 조사한 뒤 첫 추가 범위를 구체화한다. 아직 개수/자세를 확정하거나 승인된 배치로 표시하지 않는다.

- 남동: SoutheastEntry `(8400,-16000)`→기존 남동 하강로 `(8400,-14800)→(7600,-8500)→(7900,-2000)`와 메사/골짜기 사이 저지대 실루엣을 읽는다.
- 북동: NortheastEntry `(8400,16000)`→북 대지 진입과 채택 Z5/Z6·보스 고원의 외곽 높이 관계를 읽는다. 기존 길/Pod·동굴/잔도 인터페이스를 덮지 않는다.
- V010의46개 보호 공간을 유지하고 실제 신규 범위에 필요한 통로·Pod·동굴/석교·부유섬·그래플 사선을 더한다. 기존 입력을 큰 배치에 유리하게 소급 축소하거나 기존 Z1을 개별 삭제하지 않는다.
- GX-035: 비주얼 우선, 경사로 가장자리 일부 겹침 허용. 실질적으로 길을 덮으면 미채택 새 배치 보정 또는 배치를 보존한 별도 연결로 해결한다. 현재 경사로 확인을 생략하지 않는다.
- seam 시각5m 안 금지·후속 충돌20m 안 금지, 지각 native15m·전이55m를 유지한다. V010 남서 시각 상자의 최소17.12m는 시각 계약만 통과했으므로 후속 충돌에 그대로 쓰지 않는다. 북서는22.11m다. `Z7.body_handoff.seam_contract`를 따른다.
- U4-11 결과의 실제 export·기존 보존 검사·같은 카메라 비교를 정리하고 사용자 채택을 확인한다. 그 뒤 Ridge 전체 큰 배치 검토/확정을 별도로 거쳐 몸체 단계로 간다.

## 6. 도구·검증과 후속 보류

- 실제 에디터·PIE·미저장 변경·로컬 해시를 먼저 읽는다. dirty가 있으면 임의로 저장/폐기하지 않는다. 수동 GUI/PIE가 빠르고 정확하면 자동화 구현 전에 요청한다.
- 현재 MCP Programmatic은 등록 도구 조합용이며 임의 Unreal Python 실행이 불가하다. 실제 검사/export는 저장·재로드·비교 준비 뒤 사용자 UE Cmd에 요청한다: `py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/RidgeLargeLayout.py" export V011`.
- `verify V010`은 읽기 전용 상태 검사다. 채택19개의 정확한 자세/지문은 최종 채택 보고서와도 대조하고, 다음 manifest의19개 고정 목록을 누락하지 않는다. `FinalizeReview.py`·채택 스크립트는 당시 마감 도우미이며 채택본에 다시 실행하지 않는다.
- SceneTools.create_level_instance는 WorldAsset만 지정해 내부 로드를 놓칠 수 있다. 새 맵만 지정 저장·재로드한 뒤 LI/로드 수를 실제로 확인한다. `LoadRequested`면 생성부터 재실행하지 않는다.
- 읽기 전용 원본 덤프는 `Saved/Profiling/Graphics/Phase01/AssemblyTest_20261119/Cull/Export_V001.json`, 후보 조사 도우미는 U4_4/U4_10의 SurveyCandidates다. 존재하지 않는 ED 팔레트/PCG 반입을 가정하지 않는다. 새 반입이 필요하면 사용자 수동 작업부터 요청한다.
- R01~R06 제보 수집은 종료됐고 개별 삭제로 먼저 고치지 않는다. U4-6 경사로 제보는 신규2개 보정 후 확정했다. 남은 열린 내부·뒷면·받침/연속 외피는 몸체 단계다. 산괴 배경 A안·빠진 Pod2개·Floor50㎡·전체 Nav Layer256 예산·정식 풀/EnemyDensity1은 유지한다.
- 몸체·충돌·지각 재빌드·SurfaceData/전체 베이크·PIE·새 식생/잔돌 마감은 후속이다. 기존 조립체에 원래 있던 내부 식생만 이번 큰 배치의 일부로 유지했다.

## 7. 새 세션 시작 프롬프트

```text
@D:/UnrealProjects/LootNPop/.context/Graphics/README.md
@D:/UnrealProjects/LootNPop/.context/Graphics/Current.md
@D:/UnrealProjects/LootNPop/.context/Graphics/analysis/Phase01_U4LayoutHandoff.md

문서를 확인하고 Phase 1-BR U4-11 남동·북동 외곽의 큰 배치 초벌을 진행하자.
Decisions.md GX-034·GX-035, Phase01_RidgeReferencePatch.md §2.1·U4,
Phase01_U4LargeLayoutPlan.md §4·§13~17도 확인해줘.

U4-0~U4-10은 완료·사용자 채택 상태이고 U4Layout_V010은 실제 검사 PASS·export 후 동결했어.
U4-11/V011은 아직 시작하지 않았어. 원래 Z1Bake_V010과 새 U4Layout_V010을 구분해줘.
먼저 실제 에디터·PIE·미저장 변경·로컬 자료를 대조하고 U4-11 첫 추가 범위를 정한 뒤 새 V011에 배치해줘.
V010을 복제하고 채택19소유자의 위치·방향·크기·높이 관계·구도를 모두 고정해줘.
새 manifest의 이전 채택본은 V010 맵/저작본·19소유자·누적 이전 저작53파일로 갱신해줘.
V010 manifest의 V009 제작 이력을 그대로 복사해 두 소유자를 중복 추가하지 마.

Z1 V037/Z1Bake_V010과 U4 V001~V010·원본/공유 에셋은 보존하고 새 버전으로 작업해줘.
기존 생성물을 준비 목적으로 재실행하거나 U4-0부터 다시 시작하지 마.
R01~R06 수집은 종료됐고 개별 삭제로 먼저 고치지 마.
비주얼 우선·경사로 가장자리 겹침 허용을 유지하되 길을 실질적으로 막으면 새 배치 보정/별도 연결로 해결해줘.
통로·Pod·동굴·석교·부유섬·그래플과 seam 시각5m/후속 충돌20m 금지를 확인해줘.
전체 Ridge 큰 배치 초벌 확정 뒤 몸체·충돌·지각 재빌드·세부 식생/잔돌 마감으로 넘어가자.
실제 자세 export·기존 보존 검사·비교 화면까지 정리하고 구현 단위마다 다음 진행을 확인해줘.
수동 조작이 더 효율적이면 즉시 요청해줘.
```
