# Phase 1-BR U4-3 확정·U4-4 새 세션 인수인계

> 상태: 현재 인수인계 · 사용자 U4-3 V003 확정 · 다음 U4-4는 새 세션
> 갱신: 2026-11-24
> 원본: [U4 큰 배치 계획](Phase01_U4LargeLayoutPlan.md) §2~4·§7~9, [GX-034](../Decisions.md), [Phase 1-BR](../phases/Phase01_RidgeReferencePatch.md) §2.1·U4

## 1. 종료 지점과 다음 범위

사용자가 U4-3 구도를 확정하고 U4-4부터 새 세션으로 넘기도록 지시했다. **이번 세션에서 U4-4 배치를 시작하지 않았다.** U4 전체 큰 배치는 미완료다.

- U4-0: V037/V010 실제 상태 대조·보존 목록·복구51파일·동일 비교8시점과 Z1~Z7 작업 순서 확보.
- U4-1 V001: Z2 하류 큰 절벽1개, 검사/export PASS·사용자 채택.
- U4-2 V002: 석교 남쪽 받침 절벽·협곡 상류 남측 암괴2개, 오름길 겹침 보정 후 PASS/export·사용자 채택.
- U4-3 V003: 메사 북동쪽 절벽·동쪽 상단 암괴2개, 여덟 보호 공간 PASS/export·사용자 채택.
- 다음 **U4-4: Z4 MesaRamp1/2와 남쪽 외곽의 큰 배치 초벌**. 이후 Z3 북쪽 단 뒤/서쪽 접근(U4-5·6) → Z5 북 대지(U4-7) → Z6 골짜기/보스 고원(U4-8·9) → Z7 외곽(U4-10·11). 각 단위 결과를 보고하고 다음 진행을 확인한다.

전체 큰 배치 확정 → U5a Z1 몸체 시험 → U5b 구역별 몸체/충돌 → U6 전체 베이크/PIE → U7 식생·잔돌 마감 순서를 유지한다. 기존 생성물을 준비 목적으로 재실행하거나 R01~R06을 개별 삭제로 먼저 고치지 않는다.

## 2. 실제 에디터와 채택본

종료 직전 MCP 읽기 확인: 현재 맵은 아래 V003, PIE=false, V003 dirty=false다. 카메라는 MesaNorthFace이며 새 Z4 두 소유자를 선택한 상태다. Unlit·FOV90°·1305×724 비교 기준을 유지한다.

```text
/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/U4Layout_V003/LVI_Ridge_EDTerrain_U4Layout_V003
```

저작 원본은 `Scripts/Profiling/Graphics/LargeLayout_V003/`의 `RidgeLayout.json`·`EDTerrainLayout_Z2.json`·`EDTerrainLayout_Z4.json`이다. 사용자 확정은 manifest의 `acceptance`와 두 구역 `status=accepted_large_layout_rough`에 기록했다. export 뒤 채택 메타데이터만 추가했으며 배치 자세는 최종 보고서와 같다. V001/V002 맵·저작 파일은 동결했다. V003도 이후 수정하지 않고 V004 사본으로 이어간다.

현재 추가 소유자5개의 기준은 다음과 같다. 좌표는 옥탄트 로컬 X/Y/H(cm), 스케일은 전부1, tilt 약±1e-6°는 현지 축 변환 오차다. 지문은 메시별 수·transform 결과 지문이다.

| 소유자 | X / Y / H | yaw | 인스턴스 | SHA-256 |
|:---|:---|:---|---:|:---|
| ASM_Z2_LowerSouthBank | 1200 / -5800 / -6500 | 135° | 8,206 | `1ebc931549161469de67d3ebeb2f55cb6b100f2d7281d2fda6ff0e410400bff7` |
| ASM_Z2_BridgeSouthButtress | -7850 / -9800 / -600 | 100° | 9,148 | `6ba107dde77a0c56b3c05e4b7617c4324af01a2bce464de55515578584ab0b2b` |
| ASM_Z2_UpperSouthShoulder | -12900 / -9800 / -1600 | 80° | 830 | `a47ca98c89b19d193ea9860f79173622a0c6595ead1597ee7dd9466b34bd7140` |
| ASM_Z4_MesaNorthEastWall | -1800 / -10000 / -700 | 80° | 8,723 | `629396ff51f02c29841f1e376cc8e79cde23dab3ff690b90c6922307b5dc01f4` |
| ASM_Z4_MesaEastCrown | -2600 / -11800 / 1000 | 25° | 505 | `824504274c9a9fd8b4e7738a493f6fd11243f439b634b43e47749a6599bb9e40` |

실제 참조 패키지와 원본/사본 파일 해시는 위 구역 JSON의 `level`·`source_level`을 따른다. 새 Z4 사본2개는 `U4Layout_V003/Z4Assembly_V003/`에 있다. 원래 내부 배치·크기·식생 층을 유지했으며 공유 메시/재질/BPP는 수정하지 않았다.

채택 V003의 파일 기준은 다음과 같다. 후속 manifest의 `previous_unit`을 만들 때 **V003 맵과 저작 파일을 기준으로 갱신하고, 고정 소유자는 Z2 세 개와 Z4 두 개를 모두 포함한다.** V003 manifest 안의 `previous_unit`은 V002를 가리키는 제작 이력이므로 그대로 복사해 두면 Z4 두 개를 중복 생성할 수 있다.

| 채택 파일 | SHA-256 |
|:---|:---|
| `Content/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/U4Layout_V003/LVI_Ridge_EDTerrain_U4Layout_V003.umap` | `5138ac37bc8e5ddc6102d6d24971edbce1483ea623d6a7aa8cb97b29f1171609` |
| `Scripts/Profiling/Graphics/LargeLayout_V003/EDTerrainLayout_Z2.json` | `8b22ea58e1bb0cc314480d057f4c86589cdafdaf5b13034620c0c2bcf44f1158` |
| `Scripts/Profiling/Graphics/LargeLayout_V003/EDTerrainLayout_Z4.json` | `a310172b6c3906a9ca58272d3cc868048d4595caf05bea16b7a8622cf9a70447` |
| `Scripts/Profiling/Graphics/LargeLayout_V003/RidgeLayout.json` | `dde3ffcc18aa6d7a53fee447288bf334a76658f5fbecbf58f8d2e872f5f00890` |

## 3. Z1·이전 단계의 불변 기준

- Z1 기준 JSON은 `Scripts/Profiling/Graphics/EDTerrainLayout_Z1.json` V037, SHA-256 `bc0a5ac948bd2c69057f41294a1efc29ce9789c17eb89e7ac0756eff2f2ec5a0`이다. 실제 기준 맵은 `EDTerrain_V001/Z1Bake_V010/LVI_Ridge_EDTerrain_Z1Bake_V010`다. JSON의 `level`이 가리키는 Z1AssemblyWall_V037 시험 맵은 실제로 만들지 않았다.
- 최상위 기존 메시8개·Cull LI15개를 보존한다. 기존 시각185,555인스턴스·데칼12·428키 지문은 `853681a3ea1bdb4c002491d0ef4dd484f1723dac18a1915a7d61e8d027527b88`다. Cull 사본은 **이미 월드 좌표로 구워져 있어 LI 자세가 identity**다. 원본 조립체의 로컬 자세를 다시 적용하지 않는다.
- 기존 CR 산괴·정면3벽·왕관·큰 바위·남북 테두리의 크기/윤곽/주요 가림을 유지한다. U4 추가물은 소유자를 구분해 대조한다. Z1의 자세·원본 보존 목록은 [계획 §2](Phase01_U4LargeLayoutPlan.md)에 있다.
- 렌더링 지각V012·충돌 지각V013·프록시V003·V010 SurfaceData·보존 동굴/석교/부유섬은 그대로다. 새 시각 배치의 몸체/충돌은 아직 없다. Pod10·훅6·검토점27, CrownSummit·StairTerrace2 제외를 유지한다.
- 정식 풀 `/Game/Maps/OctantPoolData.OctantPoolData`·EnemyDensity=1은 PIE 제보 종료 때 복원했다. 이번 U4에서 풀/밀도/마커를 바꾸지 않았다. 새 세션에서 메모리 상태를 다시 확인한다.

## 4. 최종 검사와 화면·복구 자료

모든 경로는 `Saved/Profiling/Graphics/Phase01/` 아래다. Saved·Content 에셋은 Git 제외 로컬 자료이므로 다른 체크아웃에는 없을 수 있다. 없으면 필요한 원본/사본을 먼저 확인하고 사용자 수동 복구/반입을 요청한다. 기존 생성기를 준비 목적으로 재실행하지 않는다.

| 단위 | 최종 보고서 | 현재 유효한 비교 화면 |
|:---|:---|:---|
| U4-0 | `U4_0_V001/LiveSnapshot_20261124_164625_996859.json` | `ComparisonViews_V001.json`·`Views/`8장, 복구 `RecoveryManifest.json`·`Recovery/`51파일 |
| U4-1 | `U4_1_V001/Export_20261124_174437_554656.json` | `Views_Loaded_V001/`·`ReviewCamera_Loaded_V001.json`.최초 `Views/`는 미로드 상태이므로 새 배치 비교에 쓰지 않음 |
| U4-2 | `U4_2_V002/Export_20261124_194856_426015.json` | `Views_ClearanceCorrected/`12장·`ReviewCameras.json`.이전 Initial/FinalDraft는 보정 전 |
| U4-3 | `U4_3_V003/Export_20261124_221922_034419.json` | `Views_Draft/`12장·`ReviewCameras.json`·`CaptureMetadata_Draft.json`.이 화면을 사용자 채택 |

U4-3 보고서는 PASS, issues없음, exported=true, dirty없음, 보존51파일 변경0이다. 기존 Z1과 채택 Z2 세 배치의 자세/참조/결과 지문이 동일하다. LI/로드 레벨20개, 마커 수도 동일하다. 신규8,723+505인스턴스는 데칼0·충돌 활성 시각 컴포넌트0이다.

새2개 각각의 보호 표본은 하류63·중상류432·석교132·협곡 Pod637·동굴 접근120·MesaAscent810·SouthMesa Pod637·석교 남단 접근66이다. 큰 암석107/69개와 **모든 공간에서 상자 겹침0**이다. 실제 충돌·PIE·Nav 통과가 아니라 메시 로컬 상자+50cm 여유의 보수적 배치 표본 검사다. Pod는 반경9m, 지면+1~61m를5m 간격13층으로 본다.

- U4-2 첫 받침 `(-7000,-10200,H−400), yaw90°`는 MesaAscent49겹침으로 기각됐다. 채택 자세 `(-7850,-9800,H−600), yaw100°`로 유지한다. 보호 폭/상자 여유를 줄여 통과시키지 않았다.
- 경고1건은 기존 `ASM_Z1_WallNorth`의 비 Static `Actor_0.Scene`에 Static `InstancedStaticMesh797`을 붙이지 못한다는 AttachTo 경고다. 기존 Shell459,537삼각형 Navigation export 경고도 과거 로드에서 관측했다. 이번 보존 대조 불일치는 없으며 원본은 수정하지 않았다. 새 경고는 따로 확인한다.
- `U4_3_V003/PreviousAccepted/`·`PreviousAcceptedHashes.json`은 V0024파일, `InitialDraft/`·`InitialDraftHashes.json`은 V003 초안6파일이다. 세션 종료 채택 사본은 `AcceptedHandoff_V001/`·`AcceptedHandoffHashes_V001.json`을 따른다.

카메라 MesaNorthFace: eye 로컬 `(2000,-7000,H10500)`, target `(-4500,-10200,H2500)`. 월드 위치 `(13551.6306979497,7298.446828953416,11972.300502047514)`, 회전 pitch−1.4754622961/yaw18.0364963217/roll−167.4817361781°. 다른 새3시점은 MesaEast·MesaTop·BridgeSouth다. 기존8시점과 같은 FOV/비율/Unlit로 큰 가림을 비교하고 픽셀 해시 일치를 판정 기준으로 쓰지 않는다.

## 5. 새 세션 첫 단위 U4-4

1. 필수 문서와 이 인계·계획 §4/§9를 읽고 MCP 대상·현재 맵·PIE·미저장 사용자 변경·로컬 파일을 대조한다. U4-0부터 다시 조사/생성하지 않는다. 미저장 변경은 임의 저장/폐기하지 않는다.
2. 채택 V003 맵/저작 파일·새 조립체 사본·지문을 보존하고 **새 V004**를 마련한다. `LargeLayout_V004/`에 Z2·Z4 JSON 자체 사본을 두고 이전 맵/파일 해시와 고정5소유자를 설정한다. 현재 V003을 `build V004`의 준비 수단으로 재빌드하지 않는다. 이미 V004가 있으면 먼저 상태를 확인한다.
3. **MesaRamp1/2·남쪽 외곽의 실제 기반면과 큰 덩어리 공백을 먼저 읽고 첫 추가 소유자의 범위를 정한다.** 좌표 봉투와 최종 소유자 수/자세는 아직 미확정이다. 1단H약1700~1900, 상부H약3861 관계와 아래 보호 경로를 유지하며 필요한 현지 Up/yaw·묻힘을 조사한다. 같은 조립체를 일렬 반복해 긴 벽으로 만들지 않는다(GX-027). 넓은 산괴·둑 입력이 새로 필요하면 ED 수동 작업/반입을 즉시 요청한다.
4. 새 큰 강조부만 배치하고 원본 크기/내부 구성을 유지한다. 기존 남 테두리 띠를 위쪽 대지까지 늘리거나 기존 조립체를 움직이지 않는다. 닫힌 몸체로 채울 내부와 비워 둘 통로를 구역 JSON에 인계한다. 몸체/충돌/지각/층 재빌드·전용 식생/잔돌 마감은 하지 않는다.
5. 새 맵만 지정 저장·모든 LI 로드 확인 → 보존/보호 검사 → 새 버전 JSON에 실제 자세 export → 기존8+해당 구역 시점 비교 → 사용자에게 이번 단위 채택/다음 U4-5 진행 확인. 수동 Cmd/뷰포트 조작이 더 빠르면 도구 우회 구현 전에 요청한다.

### U4-4가 지킬 주요 빈 공간

- MesaAscent 중심선 로컬XY: `(-14500,-11000)→(-12900,-15200)→(-12100,-15200)→(-10800,-9600)→(-10100,-10000)→(-10000,-12600)→(-9000,-13200)→(-3300,-11000)→(-3500,-10200)→(-6000,-10300)`.기존 보호 반폭350cm·지면+100/250cm를 유지한다.
- 실제 SouthMesa Pod 중심 `(-6060.9,-10497.8,H3862.6)` 반경9m, 석교 남단 `(-4610.512,-9499.724,H3855.053)`에서 Pod로 이어지는 접근 반폭450cm를 유지한다.
- 협곡 통로·CanyonChamber Pod·동굴 남쪽 접근·석교 갑판/아래를 유지한다. 교량 북단의 갑판H약3835와 기존 고원H약600 단절은 후속 연결/마커 검토에 남긴다. 교량을 임의로 옮기지 않는다.
- 이음매 native15m·전이55m 계약, 시각 메시 seam5m 안 금지·후속 충돌20m 안 금지를 유지한다. 현재 U4-4는 최종 범위를 아직 정하지 않았으므로 새 소유자의 실제 bounds로 대조한다.

## 6. 도구·재사용 자료와 함정

- `InspectRidgeLargeLayout.py`: 열린 원본 V010의 읽기 전용 기준 조사용이다. 현재 V003/후속판의 기본 검사는 `RidgeLargeLayout.py verify <판>`이다. 기존 보고서는 덮어쓰지 않는다.
- `RidgeLargeLayout.py`: 버전별 manifest를 읽는다. `build <새 판>`은 채택 이전 맵 복제+고정되지 않은 소유자만 추가, 이미 결과가 있으면 중단한다. `verify`는 읽기 전용 대조, `export`는 통과할 때 해당 버전의 구역 JSON에 실제 자세를 기록한다. 기존 층/지각/Cull/충돌/베이크 생성기를 호출하지 않는다.
- SceneTools.create_level_instance는 WorldAsset 지정만 하고 내부 로드를 요청하지 않는 함정이 있다. 새 맵 지정 저장 후 다시 열거나 명시적 load_level_instance를 요청하고, 다음 Cmd에서 실제 로드를 검사한다. 스크립트가 `LoadRequested`를 남기면 로드 후 같은 검사 Cmd만 재실행한다. 기다리기만 하거나 생성부터 다시 하지 않는다.
- 현재 MCP ProgrammaticToolset은 등록 도구 조합용이며 임의 Unreal Python 실행이 불가하다. 일반 Python 스크립트 실행은 에디터 Cmd에 사용자 수동 입력을 요청했다. 검사 요청 전에 실제 로드/저장과 비교 준비를 마친다.
- 팔레트는 현재 반입된 PL_LargeCliff 계열을 사용했다. Ditch/ASM_RiverEmbankment_02B·LargeAssembly/Embankment_00·Ditch/ASM_Perimeter_Cliff_01은 미반입이므로 존재한다고 가정하지 않는다.
- 읽기 전용 원본 기하 덤프는 `Saved/.../AssemblyTest_20261119/Cull/Export_V001.json`이다. meshes=경로/로컬bounds, components=소유라벨/액터/컴포넌트/메시인덱스, instances=컴포넌트인덱스/인스턴스인덱스/위치3/쿼터니언4/스케일3이다. 옛 Cull 전 자세의 역변환→새 자세로 후보 상자를 대조했다. `U4_3_V003/SurveyCandidates*.py`·`CandidateSurvey*.json`은 이 자료를 읽는 사전 대조이며 생성기가 아니다.
- 뷰포트 이미지는 native CaptureViewport로 촬영했다. base64를 Saved 텍스트에 기록하고 PNG로 풀었다. 대용량 base64를 작은 REPL 입력에 직접 넣지 않는다. Unlit 화면이 밝기/그늘 판정을 대신하지 않는다.
- 구면 현지 Up을 사용하며 월드Z를 높이로 가정하지 않는다. 원본 `RidgeOctantSpec`/`RidgeZ1CompositionSpec`의 좌표/지각 함수와 `RidgeZ1Layout`의 pose/지문 함수를 재사용한다.

R01~R06 제보는 수집 종료·미수정이며 몸체 단계의 재검증 입력이다. R06 내부 정면은 실제 충돌 벽이다. 산괴의 적 Nav 밖 배경(A안), 제외 Pod2개, GX-030 동선 보호 보류, Floor50㎡·전체 Nav Layer256 예산은 유지한다. Crown/WestStair 동선·마커 보류와 벽 발치 잔돌·빈 분지 마감은 [이전 인계 §5](Phase01_LargeLayoutHandoff.md)를 따른다.

## 7. 새 세션 시작 프롬프트

```text
@.context/Graphics/README.md
@.context/Graphics/Current.md
@.context/Graphics/analysis/Phase01_U4LayoutHandoff.md

문서를 확인하고 Phase 1-BR U4-4 메사 오름길·남쪽 외곽의 큰 배치 초벌을 진행하자.
Decisions.md GX-034와 Phase01_RidgeReferencePatch.md §2.1·U4,
Phase01_U4LargeLayoutPlan.md §4·§9도 확인해줘.

U4-3 V003은 검사 PASS·export 후 사용자 확정했어. U4-4는 아직 시작하지 않았어.
Z1 V037/V010과 채택된 Z2 세 배치·Z4 두 배치의 위치·크기·높이·구도를 유지하고,
V003과 원본은 보존한 채 새 V004에서 이어가줘.
먼저 실제 상태와 로컬 자료를 대조하고 U4-4 첫 추가 범위를 정한 뒤 배치해줘.
이번 세션에서 다시 U4-0 조사나 기존 생성기를 준비 목적으로 실행하지 마.
R01~R06 제보는 종료됐고 개별 삭제로 먼저 고치지 마.
몸체·충돌·지각 재빌드와 세부 식생·잔돌 마감은 전체 큰 배치 확정 뒤 단계야.
단위 완료마다 다음 진행을 확인하고 수동 조작이 더 효율적이면 즉시 요청해줘.
```
