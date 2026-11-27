# Phase 1-BR — Ridge 지표 Electric Dreams 교체

> 상태: U4Layout_V011 동결·외곽 장식 종료 · U5a-7/V008 외관 사용자 채택 · GX-037 부유석 적극 활용 · [채택 결과](../analysis/Phase01_U5aBoundaryReview.md)
> 순서: 1-B5f → 1-BR(기존 1-B6 전체 교체를 흡수) → 1-C
> 상위: [그래픽스 로드맵](../Roadmap.md) §2·§4, [Phase 1 실행서](Phase01_BenchmarkContent.md) §3, [확정 결정](../Decisions.md) GX-019·GX-021
> 마지막 갱신: 2026-11-28

**U4-9 V009는 사용자가 채택했다(2026-11-25).** 보스 고원 측면 구현 근거는 [계획 §16](../analysis/Phase01_U4LargeLayoutPlan.md)에 보존한다.

**U4-0~11 큰 배치 채택·V011 동결(2026-11-25).** 실제 검사/export PASS·추가21개·51보호 공간·30시점 비교를 마쳤다. 사용자 지시로 외곽 추가 장식을 종료하고 내부 보라 구조물/부유섬은 후속 마감 대상으로 유지한다(GX-036). U5a-1 첫 생성 이후 U5a-6/7의 V007/V008까지 구현·실제 검사·시각 채택을 마쳤다. 현재 채택은 V008이며 다음 작업은 새 세션으로 넘긴다. 기존 경사로31표본 REVIEW는 남아 있다. [계획 §18~19](../analysis/Phase01_U4LargeLayoutPlan.md)·[인수인계](../analysis/Phase01_U4LayoutHandoff.md)를 따른다.

## 1. 목표

Ridge_00(V011)는 플레이 동선만 고려한 로우폴리 프로토타입 지형이다. 이 Phase에서는 옥탄트 경계 전이부를 뺀 **Ridge 지표 전체**를 Electric Dreams(ED) 지형 에셋과 ED 스타일 배경 프랍으로 교체한다.

- V011은 설계도로만 쓴다. 직행·우회, 전투장, 엄폐·사선, Pod·훅·착지를 읽는 기준이다.
- 현재 산·절벽·큰 바위의 배치와 구도는 사용자가 채택했다(GX-034). 그 위치·방향·크기·높이 관계와 전체 윤곽을 유지하고, 열린 외피 뒤와 아래의 빈 공간을 몸체로 채운다. 세부 형상·이음·잔돌·식생은 조정할 수 있다.
- 기존 10×10m 시험 구역 단계는 생략한다(GX-020 대체, GX-021). Z1에서 기존 프록시·베이크 파이프라인은 검증했다. 새 몸체 방식은 전체 큰 배치 초벌을 확정한 뒤 Z1에서 먼저 시험하고 구역별로 확장한다(GX-034, §2.1).

진행 순서는 다음과 같다.

1. 이번 V010 PIE의 R01~R06 제보 수집을 종료했다(2026-11-24 사용자). 정식 풀·적 밀도 복원과 해시 검증을 마쳤고 제보는 후속 몸체 검증 입력으로 보존한다.
2. 현재 큰 배치를 보존하면서 미배치 구역까지 Ridge 전체의 큰 지형 초벌을 채우고 구역별 배치 JSON을 확정한다(U4).
3. 확정 배치를 기준으로 Z1 대표 구간의 닫힌 몸체를 시험한다(U5a). 기존 ED 외피를 그 자리의 표면 디테일로 유지한다.
4. 시험을 통과한 방식으로 구역별 몸체·이음과 충돌을 만들고 기반면과 연결한다(U5b).
5. 전체 베이크·마커 보정·러프 PIE로 시각·충돌·동선을 확인한다(U6 게이트).
6. 잔돌·식생 등 세부 배경 프랍을 마감하고 재베이크·PIE를 확인한다(U7).

## 2. 확정 방식(GX-021, GX-034로 보강·일부 대체)

| 항목 | 결정 |
|:---|:---|
| 기반면·몸체·외피 | **고리+기반면, 큰 배치에 맞춘 닫힌 몸체, ED 외피.** 기존 경계·기반면을 유지하며 산·절벽·큰 바위는 현재 배치를 기준으로 지면에 이어지는 몸체를 만든다. ED 메시가 표면 디테일과 기존 구도를 맡는다. 몸체는 열린 뒤·아래와 조립 틈에서 빈 내부가 드러나지 않도록 시각 형상도 채운다. |
| 충돌 | **몸체와 외피에서 파생한 프록시.** ED 외피·시각 몸체는 `Decoration`/`LNPDecoration`·NoCollision이고, 기반면과 함께 파생한 충돌 지각·저폴리 `_Floor`(Support+Blocker)·`_Shell`(Blocker)이 충돌을 맡는다. 외피에 얇은 두께를 주는 것만으로 몸체 완성을 판정하지 않는다. |
| 범위 | **지표 먼저.** 동굴(대전당·산괴 동굴), 부유섬 2개, 하늘 석교는 V011 형상을 임시로 유지하고 다음 패스에서 교체한다. |
| 조립 | **초벌은 스크립트, 다듬기는 수동.** V011 고스트 설계도를 깔고 구면에 정렬한 초벌 배치를 Python으로 놓는다. 사용자가 에디터에서 구도와 이음을 다듬는다. 결과는 JSON으로 되읽어 재생성 원본으로 삼는다. |

이 방식을 고른 이유는 다음과 같다.

- 옥탄트 면적은 약 14만㎡이고 한 변이 약 470m다. ED 바닥 메시는 수 m짜리 패치이고 가장 큰 절벽도 30m 정도다. 그래서 넓은 평지를 ED 메시만으로 덮을 수 없다. 틈이 생기면 그 자리로 외우주에 추락한다.
- 베이크는 일반 StaticMesh의 단일 complex-as-simple trimesh만 Support로 받는다. ISM Support는 막혀 있다(SSN D-058). ED Nanite 메시는 대부분 단순 충돌이 없다.
- 바닥이 조각나면 Layer가 갈린다. 그러면 Pod 편성 후보(같은 Layer, 반경 15m 안 27명)가 모자란다. Ridge V007에서 실제로 그런 일이 있었다.

**C++ 변경은 계획에 없다.** 기존 베이커·수집기·PCG 샘플러 계약 안에서 해결한다. C++ 변경이 필요해지면 에디터 빌드 전에 사용자에게 확인한다.

### 2.1. 현재 큰 배치를 보존하는 몸체 계획(GX-034·037, 2026-11-27)

사용자는 현재 전반적인 비주얼과 산·절벽·큰 바위의 배치를 채택했다. 따라서 **현재 ED 배치를 목표 외형으로 삼아 그 안쪽에 몸체를 맞춘다.** 몸체를 만들기 위해 큰 덩어리를 재배치하거나 모든 열린 에셋을 일괄 삭제하는 계획으로 바꾸지 않는다. 제작 순서의 "몸체 먼저, 외피 마감"은 이 목표 외형 안에서 몸체와 플레이 경계를 먼저 성립시킨다는 뜻이다.

| 보존할 것 | 허용되는 조정 |
|:---|:---|
| 큰 산괴·절벽·바위의 위치·방향·크기·높이 관계, 스카이라인, 분지·테두리에서의 기존 구도 | 열린 뒤·아래를 메우는 몸체, 받침·지면 연결, 작은 이음 조각, 잔돌·식생·지표 패치의 국소 이동·제외 |
| 의도된 동굴·부유섬·석교·입구·실제 통로와 필요한 플레이 공간 | 의도하지 않은 빈 내부·낙하구·진입 틈의 폐쇄 |

- **전체 배치를 먼저 끝낸다는 범위는 큰 지형의 초벌이다.** 미배치 구역의 산·절벽·큰 바위와 주요 골짜기·통로 관계를 먼저 확정한다. 잔돌·식생·세밀한 바닥 덮개까지 완성한 뒤 몸체를 만드는 순서로 두지 않는다. 몸체·접지 조정 뒤 U7에서 마감한다.
- **Z1의 보존 기준은 이번 PIE 대상 V037/V010이다.** 채택된 큰 배치의 transform·입력 지문과 기존 시점을 작업 전 기록하고 사본 새 버전으로 진행한다. 과거 Cull 이전 배치를 자동 복원하지 않는다. 보존할 큰 덩어리 목록과 비교 시점은 구현 단위 시작 때 명시한다.
- **몸체는 큰 배치 전체를 하나의 볼록 껍질로 덮지 않는다.** 구역·덩어리별로 외피 안쪽의 빈 부피를 채운다. 기반면에 연결하는 지형과 독립된 부유 덩어리를 구별하고, 의도된 부유석에 지면 연결을 강제하지 않는다(GX-037). 의도된 동굴·교량 아래·이동 통로는 보존한다. 형상 생성법과 해상도는 U5a 시험에서 정한다.
- **시각 형상과 충돌이 같은 공간을 표현해야 한다.** 보이는 구멍 앞에 투명 벽만 추가하거나 뒷면 표시만 켜는 것으로 완료하지 않는다. 몸체를 시각 형상과 충돌 파생의 공통 입력으로 삼고, ED 외피 위의 필요한 보행면·선반은 프록시 맞춤을 확인한다.
- **지각·Nav를 몸체 내부에 남기지 않는다.** 의도된 빈 공간을 제외한 몸체 내부의 지각·Support·스폰 후보를 검사하고 기반면 절개를 다시 계산한다. 구면 현지 Up·이음매·Floor 50㎡·Nav Layer 예산 등 기존 계약은 유지한다.
- **플레이어 접근과 적 Nav는 별개다.** GX-033의 산괴 배경(A안) 결정은 몸체 추가만으로 바꾸지 않는다. CrownSummit·StairTerrace2 Pod를 자동 복원하지 않는다. 플레이어의 보행·점프·그래플 접근에서는 열린 내부로 진입하거나 바위 뒷면에 도달하지 않아야 한다.
- **정리 규칙은 몸체와 함께 재검토한다.** 기존 A~D는 V037 결과의 재현 근거로 보존한다. 새 판의 의도된 부유석에는 A의 지면 받침을 강제하지 않으며(GX-037), 큰 배치를 유지해야 하는 문제를 무차별 삭제로 해결하지 않는다. 원본 ED 에셋과 원본 조립체는 그대로 보존한다.

기존 동선의 완전 보존보다 큰 배치 비주얼을 우선한다(GX-035). 경사로 가장자리 겹침은 허용하고 통행을 덮는 곳은 미채택 배치를 보정하거나 별도 연결을 만든다.

새 실행 순서는 **제보 수집 종료 → U4 전체 큰 배치 초벌·구역별 확정 → U5a Z1 몸체 시험 → U5b 구역별 몸체·충돌 → U6 전체 검증 → U7 세부 마감**이다. 기존 "Z1 최종 러프 PIE를 통과한 뒤 다른 구역 배치" 순서는 이 순서로 대체한다. U5a에서는 최소한 열린 세로 외피(R01/R05 유형)와 지표 아래 공동·별도 입구(R03/R04 유형)를 포함하고, 통과 후 나머지 구역에 같은 방식을 적용한다.

각 구현 단위 뒤에는 큰 배치·구도 보존과 해당 범위의 시각·접근·충돌을 보고하고 다음 진행을 확인한다. 제한 Z1 몸체 시험은 U5a-7/V008까지 구현·시각 채택했으며 전체 몸체·충돌/접근 게이트는 후속이다. **이번 PIE 제보 수집은 종료됐고 U4-0~U4-11을 완료/채택했다.** V011 동결과 내부 몸체 시험 입력은 [U4 큰 배치 계획 §18~19](../analysis/Phase01_U4LargeLayoutPlan.md)·[현재 인수인계](../analysis/Phase01_U4LayoutHandoff.md)를 따른다. 이전 복구 상태는 [인수인계](../analysis/Phase01_LargeLayoutHandoff.md)에 보존한다.

**부유석 적극 활용(GX-037, 2026-11-27).** Ridge를 포함한 전체 LootNPop 맵에서 부유섬·공중 암석과 그 사이의 빈 공간을 적극적인 판타지 표현으로 사용한다. 시각적 접지가 없다는 이유로 메우거나 삭제하지 않는다. 내부 노출·뒷면·실질적인 막힘은 별도 검사하고, 기존 채택 배치와 충돌/Support·이음매 계약은 유지한다.

**외곽 종료·내부 우선(GX-036).** 사용자가 V011 큰 배치를 채택하고 외곽 보라색 전이 띠의 추가 장식을 현재 정도에서 종료했다. 삼각형 안쪽 보라색 보존 구조물과 두 부유섬은 마감 대상이다. 내부의 닫힌 몸체 시험을 먼저 하며, 의도된 동굴/교량 아래/통로·착지/그래플 공간을 보존한다.

## 3. 보존과 작업 위치

| 구분 | 경로·처리 |
|:---|:---|
| 새 WIP | `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/EDTerrain_V001/` (Git 제외) |
| 비교·복구 기준(수정 금지) | V006 WIP `Graphics/V001/LVI_Ridge_SurfaceProbe`, `ReferencePatch_V001/LVI_Ridge_ReferencePatch_V001`, 정식 Ridge(`/Game/Maps/Ridge_00/`)와 정식 풀 |
| 재생성 원본(Git 추적) | 이 Phase에서 만드는 스크립트와 JSON(§7) |

V006 장식 1,461개는 새 LVI로 옮기지 않는다. 그 규칙과 노드는 U7에서 재사용한다.

## 4. 단위별 실행 계획

단위마다 결과를 보고하고 다음 진행을 확인한다. 기존 미저장 사용자 변경은 임의로 버리거나 저장하지 않는다.

### U1. 설계도 추출(V011 → 조립 기준)

새 `Scripts/Profiling/Graphics/ExportRidgeBlueprint.py`는 `Scripts/RidgeOctantSpec.py`를 import해서 `RidgeBlueprint_V001.json`을 만든다. 담는 내용은 다음과 같다.

- 구역 다각형과 높이(PLATES 단 높이, BASINS)
- 단 가장자리 절벽선과 높이차. ED 절벽이 들어갈 자리다.
- PATHS 중심선·반폭, ROUTES
- PODS 12, ANCHORS 6, NODES 27
- VIEWS, SIGHT_EXPECTATIONS
- CAVES·ISLANDS·석교 보존 영역

조립 기준이 될 고스트도 만든다.

- V011 지각과 Rocks를 새 LVI 안에 반투명 재질로 둔다. 설정은 `Decoration`/`LNPDecoration`/HiddenInGame이다.
- 고스트는 조립 기준으로만 쓰고 최종 베이크 전에 제거한다.
- 동선 보호 형상은 기존 `DrawRidgeDecorationKeepOuts.py`로 그린다.

**확인 기준:** 새 LVI에서 고스트와 보호 형상이 보이고, JSON의 Pod·훅·검토점 좌표가 V011 마커와 일치한다.

**재현:** 셸에서 `ExportRidgeBlueprint.py`로 설계도 JSON을 생성한 뒤 UE 상태 표시줄 Cmd에서 `py "D:/UnrealProjects/LootNPop/Scripts/Profiling/Graphics/BuildRidgeBlueprintGhost.py"`를 실행한다. 빌더는 정식 V011의 마커 좌표를 대조한 뒤 새 LVI를 만든다. 보호선만 다시 보려면 `DrawRidgeDecorationKeepOuts.py draw-br`을 실행한다. 절벽선은 원래 단 다각형의 변이고, 높이차는 각 변 중점 양쪽의 V011 노면을 잰 부호 있는 값이다.

### U2. ED 지형 에셋 선정과 반입

역할별 후보는 다음과 같다(원본 디스크 조사 기준).

| 역할 | 후보 |
|:---|:---|
| 단 가장자리·큰 절벽면 | HugeSandstoneCliff_01~08, MassiveSandstoneCliff_01~06, SmartAssets `SM_Cliff_02~19` |
| 협곡 벽(최대 약 80m) | Massive와 Huge 계열을 쌓아서 만든다 |
| 능선·메사 윤곽 | CanyonSandstoneRidge_01~04, MassiveCanyonSandstoneMesa_01, GiganticSandstoneTerrain |
| 둑·완사면 | GrassyEmbankment, RiverbedEmbankment, SandstoneRockyGround |
| 첨탑·암괴 | ForestRockFormation, NordicBoulder |
| 하상 | 기존 01~04(§8)의 재질 혼합 결과를 재사용한다 |

- 원본 ED 에디터에서 읽기 전용으로 bounds, Nanite fallback 삼각형 수, 충돌 설정, 실효 MI를 조사한다. 결과는 `EDTerrainCatalog_V001.json`에 남긴다.
- VRAM 한계(7.6/8.2GiB) 때문에 원본 에디터와 LootNPop 에디터를 동시에 띄우지 않는다.
- **Migrate는 사용자가 직접 한다.** `/Game` 경로를 유지하고, 덮어쓰기 확인에서는 "모두 아니요"를 고른다.
- 반입 뒤에는 `ElectricDreamsPalette.json`에 목록과 해시를 추가하고, 새 폴더가 Git에서 제외되는지 확인한다.
- 원본 계열별 폴더 크기는 HugeSandstoneCliff 1.7G, MassiveSandstoneCliff 1.1G, SandstoneRockyGround 791M, ForestRockFormation 710M, CanyonSandstoneRidge 642M다. 반입량은 Migrate 의존성 목록으로 확정한다.

**확인 기준:** 역할마다 치수가 맞는 후보와 실효 재질이 있고, 반입 목록과 해시가 기록되어 있다.

실측한 후보 55개와 선정한 메시 17개, 원본 구도용 재질 덮어쓰기 및 Migrate 선택 목록은 [EDTerrainCatalog_V001.json](../../../Scripts/Profiling/Graphics/EDTerrainCatalog_V001.json)에 있다. 원본과 반입본의 패키지 해시는 [ElectricDreamsPalette.json](../../../Scripts/Profiling/Graphics/ElectricDreamsPalette.json)의 `ed_terrain_v001`에 기록했다. 재조사는 UE Cmd에서 `InspectRidgeEDTerrain.py source`, `dependencies`, `target` 인자로 실행한다.

### U3. 지각 V012: 경계 고리 + 기반면(Blender)

새 spec 모듈은 `Scripts/RidgeEDBaseSpec.py`다.

- RidgeOctantSpec을 import해서 쓴다. V011 재현성을 지키기 위해 원본 spec은 수정하지 않는다.
- `base_ground()`는 V011 높이장을 저역 통과한 값이다. 단·절벽·협곡 벽은 완만한 산괴와 골짜기로 바뀐다.
- 다음 높이는 고정한다. 그래서 기반면만으로도 동선이 대략 성립한다.
  - PATHS·ROUTES 노면
  - Pod 섬, 착지부
  - 동굴 입구 고리, 석교 교대

새 빌더는 `Scripts/Blender/BuildRidgeEDBase.py`다. `BuildRidgeOctant.py`의 `crust()`(301-427)와 `finish()`(207-285) 흐름을 재사용한다.

- Meadow 이음매의 native 띠 15m는 그대로 보존하고, 15→55m 구간은 블렌드한다.
- Rocks는 동굴·부유섬·석교 부분만 남긴다. 첨탑·엄폐 암반·외곽 바위는 ED로 대체하므로 뺀다.
- 걷는 지각의 최고점은 +78m 이하로 둔다(Layer 0 ±81.9m 한계).

검증과 반입:

- 검증은 `VerifyRidgeOctant.py`의 변형으로 한다. seam 평면 경계, 진입 광선, 경로·Pod 검사를 기반면 기준으로 돌린다.
- UE 반입은 `AssembleRidgeOctant.py` 흐름을 새 폴더로 돌린다.
  - `snap_seam`, 태그, 프로필, `CTF_UseComplexAsSimple`을 적용한다.
  - 마커(Pod 12, 훅 6, 검토점 27)는 기반면에 다시 투영해 복사한다.
  - 지각 재질은 RidgeSurface V005다.
- **조기 확인:** 기반면만으로 한 번 베이크해서 파이프라인과 seam delta 0을 확인한다. 짧은 확인이고 PIE는 선택이다.

**확인 기준:** Blender 검증 통과, UE seam 스냅 최대 보정 ≤0.05cm, 기반면 베이크 무오류.

재현하려면 `Saved/SliceOctant/Ridge_00/Baseline/MeadowSeam.fbx`를 준비하고 Blender 백그라운드에서 `BuildRidgeEDBase.py`와 `VerifyRidgeEDBase.py`를 차례로 실행한다. 기존 V011과 같은 Meadow 메시를 충돌·LOD 없이 FBX로 내보낸 원본을 쓴다. U1 레벨을 만든 뒤 UE Cmd에서 `AssembleRidgeEDBase.py assemble`, `AssembleRidgeEDBase.py bake`를 차례로 실행한다. Blender 검증·마커 JSON·반입·베이크 리포트는 `Saved/SliceOctant/Ridge_00/V012/`에 저장된다.

### U4. ED 지형 조립(구역별)

**U4-0 완료(2026-11-24).** V037/V010 실측 대조·복구51파일·Unlit 비교8시점과 Z1~Z7 작업 순서/첫 U4-1 범위를 [U4 큰 배치 계획](../analysis/Phase01_U4LargeLayoutPlan.md)에 기록했다. 기존 배치·생성물은 수정/재실행하지 않았다.

**U4-1 완료·사용자 채택.** 새 `U4Layout_V001` 사본과 Z2 조립체 사본에 큰 조립체1개를 추가했다. 내부 로드 누락을 복구한 사용자 재실행에서 기존 Z1 지문/51파일 불변·신규 NoCollision·협곡 상자 겹침0 PASS, 실제 자세 export 완료. 경로·초안 자세·오류 원인·최종 근거는 [계획 §7](../analysis/Phase01_U4LargeLayoutPlan.md#7-u4-1-초안-구현과-검사-대기2026-11-24)을 따른다. 사용자가 채택하고 U4-2 진행을 승인했다.

**U4-2 완료·사용자 채택.** V001을 동결하고 새 V002에 석교 남쪽 받침 절벽과 상류 남측 독립 암괴를 추가했다. 새 받침의 MesaAscent 겹침을 위치/방향 보정으로 해결했다. 실제 재실행 PASS: 기존 Z1·채택 하류·51파일 불변, 새9,148+830인스턴스 NoCollision, 여섯 보호 공간과 추가 Pod 위쪽 표본 겹침0. [계획 §8](../analysis/Phase01_U4LargeLayoutPlan.md)에 실제 경로·보정/보호 공간·최종 결과를 보존했다. 사용자가 채택하고 U4-3 진행을 승인했다.

**U4-1~10 구현 경과(완료/사용자 채택).** Z2 협곡/석교·Z4 메사·Z3 안부/서쪽 접근·Z5 북 대지·Z6 골짜기/보스 고원·Z7 남서/북서 외곽까지 추가19소유자를 채택했다. 구역별 검사/export·복구·비교 근거는 [계획 §7~17](../analysis/Phase01_U4LargeLayoutPlan.md)과 [Phase 로그](../history/Phase01_Log.md)에 보존한다. V006의 초기 경사로 REVIEW는 GX-035에 따른 신규2개 자세 보정과 중앙3m 띠 포함24공간 PASS로 해결·확정했다. 기존 지각/충돌/PIE의 검증을 새 큰 배치 검증으로 간주하지 않는다. **U4-11도 실제 검사/export PASS 뒤 채택했고 외곽 추가 장식을 종료했다(GX-036)**. 이전 단계의 pending/REVIEW 당시 기록은 이 최종 채택 상태로 구분한다.

새 `Scripts/Profiling/Graphics/PlaceRidgeEDTerrain.py`의 서브커맨드는 다음과 같다.

| 서브커맨드 | 하는 일 |
|:---|:---|
| `place` | 배치 JSON을 읽어 액터를 놓는다. 항목은 에셋, 로컬 X/Y, 높이 오프셋, 현지 Up 기준 yaw, 기울기, 스케일이다. 현지 Up은 `-normalize(pos)`이고 메시 +Z를 현지 Up에 맞춘다. 결과는 `EDTerrain/<Zone>` 폴더의 StaticMeshActor(`Decoration`/`LNPDecoration`)다. |
| `export` | 사용자가 손으로 다듬은 실제 transform을 되읽어 `EDTerrainPlacement_V001.json`에 쓴다. 이 파일은 Git에 추적하는 재생성 원본이다. |
| `clear` | 해당 구역을 지운다. |

구역 순서는 다음과 같다.

| 구역 | 범위 |
|:---|:---|
| **Z1(파일럿)** | 대분지·협곡 하류. 기존 프록시·베이크 기술 검증을 마쳤다. 큰 배치 보존 기준이며 전체 큰 배치 초벌 확정 뒤 U5a 몸체 시험을 먼저 한다(GX-034). |
| Z2 | 협곡(석교 아래) |
| Z3 | 산괴 계단·왕관 정상 |
| Z4 | 남쪽 메사 |
| Z5 | 북쪽 고개·대지·잔도 |
| Z6 | 골짜기 층계참·보스 고원 |
| Z7 | 외곽 보조 지형 |

배치 규칙:

- ED 메시는 seam 평면에서 5m 안에 두지 않는다. 충돌 프록시는 20m 안에 두지 않는다.
- PATHS 반폭은 GX-017 기준으로 비운다. Pod 중심 9m와 훅 사선(SIGHT_EXPECTATIONS)도 보호한다.
- 30m를 넘는 평면 메시는 곡률 처짐을 고려해 조각마다 기울기를 따로 준다.
- 플레이 시야에 드는 절벽 면은 ED 암벽을 연속으로 조립한다. 기반면은 암벽 단면과 상단 접점에 맞게 조형하고, 메우기 어려운 작은 이음에만 드러나도록 한다. 평탄한 분지와 하상에는 기존 보행면 방식이 적용된다.
- 뒤가 열린 곡면 조각은 열린 면을 다른 지형이 가리는 모서리에만 배치한다.

**확인 기준:** 구역마다 `export` JSON이 남아 있고, `clear` → `place`로 같은 배치가 재현된다.

`EDTerrainPlacement_V001.json`의 높이 오프셋은 V012 높이장을 기준으로 한다. 에디터에서 각 구역을 다듬은 뒤 `export <Zone>`을 실행한다. `clear`는 현재 액터의 메시·재질·transform이 JSON과 일치하는지 확인하고 해당 구역만 제거한다. `place`와 `export`는 메시의 보수적인 상자 범위를 써서 seam·동선·Pod·착지·훅과 유지할 사선을 검사한다. 에디터 명령 뒤 레벨 저장은 별도로 한다.

Z1에서는 사용자가 기존 동선을 바꿔도 된다고 하여 절벽 배치를 먼저 정하고 기반면을 맞춘 별도 조형 비교본을 만들었다. 새 지형은 `Scripts/Blender/BuildRidgeZ1Composition.py`, 별도 레벨 조립은 `Scripts/Profiling/Graphics/AssembleRidgeZ1Composition.py`로 실행한다. 구도를 채택한 뒤 동선과 마커를 다시 정하고 U4 배치 재현 기준을 확인한다.

Z1 비교본은 원본 조립체에서 확인한 메시 용도를 따른다. MassiveSandstoneCliff 05는 절벽 상부 선반, 03은 하천 둑과 지면, SmartAssets Cliff 06은 절벽 세로 외피로 재배치했다. 관측한 조립체와 각도는 [EDTerrainCatalog_V001.json](../../../Scripts/Profiling/Graphics/EDTerrainCatalog_V001.json)의 `source_placement`에서 확인한다. 현재 비교본의 배치는 `Scripts/RidgeZ1CompositionSpec.py`에 있다.

`Z1Composition_V002/LVI_Ridge_EDTerrain_Z1Composition_V002`가 현재 서쪽 절벽 검토본이다. 하단 벽 5개와 상단 왕관 3개를 이어 조립하고, Blender에서 뒤쪽 지각을 각 단면에 맞춰 조형했다. V001의 `PillarApron`·`NorthLedge`는 열린 뒷면이 보이던 자리를 비웠다. V002의 전체 ED 시각 메시는 16개이며 V001 비교본은 보존했다.

재현 시 Blender 명령은 `--python Scripts/Blender/BuildRidgeZ1Composition.py -- west-cover`, UE Cmd 명령은 `py "<프로젝트>/Scripts/Profiling/Graphics/AssembleRidgeZ1Composition.py" west-cover`다. 새 레벨 조립은 기존 V001 비교본의 하상 재질을 공유하므로 V001이 먼저 준비되어 있어야 한다.

정면 피복은 옥탄트 로컬 Y −3,100~2,900cm, H −3,650~1,300cm에서 150cm 격자 1,394개를 +X 쪽에서 −X로 쏘아 비교했다. 첫 가시 표면이 ED인 표본은 V001 527개, V002 1,350개였다. 이 검사는 서쪽 절벽의 선택 단면 기준이다. 양쪽 사선의 재질·형상 이음과 인접 사면 전이는 계속 검토한다.

`PlaceRidgeEDTerrain.py`·`EDTerrainPlacement_V001.json`은 개별 StaticMeshActor 방식이고 기본 WIP의 이전 13개 초벌만 재현한다. 조립체 방식으로 바뀐 Z1의 재현 원본은 아래 조립체 방식 끝의 `EDTerrainLayout_Z1.json`이다(GX-030). 충돌 프록시·비교본 SurfaceData·러프 PIE는 U5·U6에서 검증한다.

#### 조립체 방식(GX-022~GX-027, 2026-11-19~20)

위 개별 메시 비교본(V001·V002)은 계열 혼용과 원본보다 큰 스케일로 이질적이어서, 시각 단위를 ED 원본 조립체로 바꿨다(GX-022). 큰 산괴는 조립체를 벽돌처럼 이어서는 덩치가 모자라, ED CloseRange의 PCG 결과를 구워 쓴다(GX-023). Z1 서쪽은 다음 층으로 구성한다.

| 층 | 내용 | 도구 |
|:---|:---|:---|
| 앞면 벽 | 02c·02e·02d 조립체 Level Instance(벽 발치 x≈−2,000, 25m 간격). V023부터 yaw 남 +10°·가운데 −8°·북 +6°, 북쪽 벽만 4.5m 묻어 같은 계열 반복을 깬다(올려서 발치를 띄우지 않는다) | `BuildRidgeZ1AssemblyTest.py` `WALLS` |
| 왕관 | 앞면 벽 뒤 고원에 01b·02b·01c를 10~25m 묻어 높이를 달리 세워 벽 꼭대기 틈을 막는다(V023, GX-026) | `CROWN_V023` |
| 기반면 | 02 벽 안쪽 x −2,200부터 분지 기준 +46m 고원(Z1MassifCover V002). 렌더링 지각 V005는 ED 섬 지지면 높이장을 따라 둑 고리 +3m 단을 받치고 하상 띠를 50cm 판다(배치 높이는 V002). V006은 고원 남북 끝 V012 능선을 남 +6.5m·북 +20m로 깎고, V007은 분지 남 테두리를 약 12m·50° 둑 3단으로 바꾼다(GX-027). V008은 북 테두리 앞 분지 쪽으로 둑 2단을 덧대고, V009는 동굴 절개 모서리 옆 V012 바위 돌출을 깎는다(GX-028). V010은 평면 묶음 다각형(`FLAT_REGIONS`) 안만 50cm 낮춘다(배치 높이는 V009, GX-029) | `RidgeZ1CompositionSpec.massif_cover_ground`·`massif_terrace_ground`·`massif_spur_ground`·`massif_flat_ground`, Blender `-- massif-cover`·`-- massif-terrace`·`-- massif-spur`·`-- massif-flat`, UE `importcrust` |
| 긴 테두리 | 같은 계열 조립체를 줄지어 세우지 않는다(V025 기각). 둑 지각 위에 CR_Ditch 둑 경사면의 반경 3m 묶음을 표면 좌표계째 옮겨 심은 띠(320cm 간격, 큰 바위 40개). 판 조각은 지각 법선에 눕히고 법선 둘레로만 돌리며, 덩어리·식생은 지각 높이에 다시 앉힌다(V029, GX-027). 능선 자리에는 조립체를 단독 강조로만 둔다. 둑 여러 단이 들어갈 깊이가 없는 북 테두리(앞면 38m·높이 60m)는 분지 쪽 둑 2단 + 띠(큰 바위 덩어리형 1.4~2.0배)에 위 급사면 단독 02c 하나다(V033, GX-028) | `band <버전>`, `RIM_ACCENTS_V025`, `WALLS["V033"]` |
| 산괴 | CloseRange LargeAssembly를 원래 크기 강체로 고원 위 로컬 (−6,500, 300)에 | `BakeEDCloseRange.py`(ED), `WALLS` |
| 바닥·하천·잔재 | CloseRange Ground·SmallAssembly·Props·Ditch를 인스턴스마다 지각 높이와 현지 Up으로 드레이프. V003부터 납작한 지면 타일·데칼은 지각 법선으로 눕히고 급경사에서 뺀다 | `drape <버전>` |
| 맨 평지 | 띠·드레이프가 닿지 않는 북쪽 단(+20m)·남서 모서리 봉우리 마루(+6.5m)에 같은 4그룹을 섬 안 평탄한 10m 칸째 옮겨 심는다(칸 회전 90° 배수, 원본 지지면 위 높이 유지, Drape V004 기울기 규칙). V001 14,850개(북 11,611·남서 3,239, V034, GX-029) | `flat <버전>`, `spec.FLAT_REGIONS` |
| 지각 재질 | 하상 재질 사본의 구역 마스크를 넓힘. V003부터 구역은 이음매 띠를 뺀 옥탄트 전체(GX-025). CR 지면 느낌의 지면층 교체(V002, DriedGrassOnDampSoil)는 기각되어 지면층은 V001 그대로 | `surface V003` |
| 웅덩이 재질 | ED 진흙 웅덩이(Single Layer Water) 사본에 출력 노드를 더해 컴파일 실패를 고치고 Drape V006에서 바꿔 끼움 | `puddle`, `drape V006` |
| 시험 조명 | TestMap03 조명 사본 + 균일 회색 Sky Light 50(시험 레벨 전용, GX-025) | `WALL_SKY` |

현재 기준은 `Z1AssemblyWall_V034`이다(V023에 능선 깎기·남 테두리 둑 3단, 북 테두리 분지 쪽 둑 2단·돌출 깎기, 둑 묶음 띠 남 V005·북 V009, 능선 자리 단독 조립체 3개, 북 급사면 02c, 북쪽 단·남서 모서리 평면 묶음 Flat V001과 그 자리를 50cm 낮춘 지각 V010을 더함). 구도는 부감과 분지 눈높이 저시점(Pod 중심·협곡 진입·홀 게이트), 테두리 정면 2곳, 남 둑 근경, 북 테두리 3곳(둑 근경·부감·동쪽 동굴 입구), 북쪽 단·남서 부감에서 함께 판정한다(GX-026~GX-029). 석교 아래 협곡 바닥(Z2)과 남 테두리 위 남쪽 대지는 아직 맨 지각이다. 재생성 순서와 남은 문제는 [Current](../Current.md) §3이 소유한다. 이 층들은 모두 NoCollision 시각 시험이다.

**재현 원본(GX-030, 2026-11-22).** Z1의 Git 추적 원본은 `Scripts/Profiling/Graphics/EDTerrainLayout_Z1.json` 하나다.

- 손배치 층(개별 메시·조립체·CR 산괴 Level Instance·조명·Sky)은 결과 transform을 옥탄트 로컬로 담는다. 에디터에서 다듬은 배치는 `RidgeZ1Layout.py export`로 되읽는다.
- 인스턴스 층(드레이프·띠·평면 묶음, 약 14.2만 개)은 생성기 명령·설정·시드와 입력(`CR_*.umap` 해시)·결과(메시별 수·transform 해시) 지문만 담는다. 인스턴스 손수정은 받지 않고 생성 규칙으로 고친다.
- `build <이름>`은 JSON에서 시험 레벨을 만들고, `verify`는 열린 레벨을 JSON과 대조한다. `regen <drape|band|flat> <버전>`은 층을 `_Repro`에 다시 생성해 지문을 대조한다.
- V034는 층 4개 재생성·JSON 재구성·지각 Blender 재생성 모두 지문이 같았다.
- 동선·Pod 보호(GX-017)는 Z1 동선·마커를 새 지형에 다시 정한 뒤 규칙으로 일괄 적용한다(`protection.status = deferred`).

### U5. 충돌 프록시와 기반면 절개(Blender)

**현재는 몸체를 공통 입력으로 추가하는 U5a/U5b 계획이다(GX-034).** 아래 기존 내보내기·40cm 외피 두께·프록시 생성 절차는 V035~V037의 구현 근거이며, 그 절차만으로 큰 덩어리 내부가 채워졌다고 판정하지 않는다.

#### U5a. Z1 몸체 시험

**U5a-7 V008 국소 보강·실제 검사/27비교 완료·사용자 외관 채택(2026-11-27).** V007 유지 승인 뒤 기존 Cliff09 안에162정점/320삼각형 몸체1개만 추가했다. 기존4몸체/연결/26자세/ED는 고정했다. 실제73,273정점 PASS·자기 교차0·534파일 보존·다른50공간 새 겹침0·통로13,221표본 침범0·54화면/복구 완료다. StairRamp3의31표본/R01 전체 범위는 REVIEW다. [경계 검토](../analysis/Phase01_U5aBoundaryReview.md)를 따른다.

**U5a-6 V007 사용자 채택(2026-11-26).** 실제 전체 보존/73,111정점·자기 교차0·같은26카메라 비교 뒤 사용자가 유지를 승인했다. Acceptance_V007/FrozenInputs_V007이 현재 채택을 소유하고 Generation_V007의 이전 V006/accepted=false는 제작 이력으로 보존한다. [자기 교차 검토](../analysis/Phase01_U5aSelfIntersectionReview.md)를 따른다.

**U5a-5 몸체 캡/경계/재질·V006/26비교·실제 보존/73,111정점 PASS, 현재 외관 사용자 유지.** V005 연결 외형 유지 승인 후 새 V006에서 큰 캡2개25cm 수리/최대36.53cm 굴곡·하부 바깥816정점 최대44.09cm 보정·전용 Cliff MI/VT 텍스처2개를 적용했다. 실제4몸체145,806삼각형·NoCollision·26자세 오차0·이전333파일/연결 컴포넌트 불변·26비교·검토33파일 복구본이다. 새 몸체와 고정 통로13,221표본 침범0·다른50공간 새 겹침0, 기존길31표본/자기 교차 후보339/캡 재질 접합은 REVIEW다. 최종 FBX/VT 수정 뒤 촬영 구간 경고0, 사용자 실행 Verify/CapVerify_20261125_1937 보존/실제73,111정점 PASS·최대0.004367cm·전체 dirty없음이다. 최종36파일 복구본·검토 근거44파일을 남겼다. 사용자가 V006 현재 외관 유지를 확정하고 다음 U5a-6을 새 세션으로 넘겼다. Acceptance_V006/FrozenInputs_V006·채택43파일/다음417보존 목록을 고정했다. [새 세션 인계](../analysis/Phase01_U5aBodyHandoff.md)를 따른다. [최신 결과](../analysis/Phase01_U5aBodyCaps.md)를 따른다.

**U5a-4 연결 석재 외형·V005/26비교·실제 보존/858정점 PASS, 연결 외형 유지 사용자 승인(2026-11-25).** 새 연결6,288삼각형·받침4곳·실제26자세 오차0·이전267파일 불변이다. 사용자 실행 Verify/StoneVerify_20261125_181219는 보존/실제858정점 PASS·최대0.004367cm·전체 dirty없음이다. 최종22파일 복구본/검토 근거44파일을 남겼다. 동선/연결 외형은 V005 기준으로 유지하고 몸체와 물리 게이트는 구별한다. [기록](../analysis/Phase01_U5aStoneRefinement.md)을 따른다.

**U5a-3 별도 연결 초안·실제 보존/22비교 완료, 동선 유지 사용자 승인(2026-11-25).** V004에 새 연결1개·4,000삼각형·약129.9m·중앙3m·최대43.06°·NoCollision을 배치했다. 실제 전체 지문/자세/프로필 PASS·200파일 불변·dirty없음·26자세/22비교·16파일 복구본이다. 새 통로12,726표본 겹침0·다른50공간 새 겹침0이며 원래51입력/기존길29겹침은 별도 기록한다. 몸체4개/채택21개는 고정했다. [결과](../analysis/Phase01_U5aConnectionTrial.md)를 따른다.

**U5a-2 실제 반입·보존/16비교 완료, 외관/경사로 REVIEW(2026-11-25).** V003/생성V007의4메시96,344삼각형·NoCollision·identity를 반입/지정 저장했다. 실제 보존 PASS·Z1/채택21개/174파일 불변·dirty없음·25자세 export·새20파일 복구본을 확인했다. 하부468.33m³·경사로28표본 겹침·나머지50공간0·R03/R04 내부다. 새 경고0·기존 AttachTo1건이다. 캡 평면감/재질 차이와 북쪽 착지부는 REVIEW이며 새 연결은 미배치다. 이후 사용자 승인으로 위 U5a-3을 진행했다. [결과](../analysis/Phase01_U5aBodyRefinement.md)를 따른다.

**U5a-1 첫 생성·반입·비교 완료, 미채택 REVIEW(2026-11-25).** U5aBody_V002/생성V006에 닫힌 몸체4개·96,344삼각형을 NoCollision으로 시험했다. 경계/비다양체 변0·기존 Z1/21개/156파일 보존 PASS·16시점 비교를 마쳤다. R03/R04 기록 위치는 같은 몸체 부피에 들어가지만 StairRamp3 중앙32표본과 겹친다. 나머지50공간은 표본 겹침0이다. 새 UV 탄젠트 경고는 보정 뒤 사라졌고 기존 북벽 AttachTo1건만 남았다. 다음은 몸체 경계/캡 외관과 별도 경사로 연결 검토이며 충돌·내부 지각/Support·베이크·PIE는 후속이다. [시험 결과](../analysis/Phase01_U5aBodyTrial.md)를 따른다.

**U5a-0 입력 조사 완료(2026-11-25).** `ClosedBodyTrial_V001/BodyTrial.json`에 채택 V011/21소유자·저작8+이전53파일·51보호 공간을 고정했다. R01/R05 열린 세로 외피와 R03/R04 같은 하부 공동의 두 입구를 첫 시험으로 묶고 R02/R06도 회귀 감시한다. 기존 덤프의185,555개 잔존 대조와 내부 검토6시점을 남겼으며 이 입력 조사 당시에는 몸체/시험 맵이 없었고 현재 결과는 위 U5a-1을 따른다. [계획 §19](../analysis/Phase01_U4LargeLayoutPlan.md)를 따른다.

1. 전체 U4 큰 배치 초벌과 Z1 보존 기준을 확인한 뒤, 사본 새 버전에 대표 외피·하부 공동을 고른다. 지적 대장 R01~R06과 기존 구도 시점을 검증 입력으로 쓴다.
2. 기존 큰 배치 안쪽으로 닫힌 몸체·지면 받침을 만들고 외피의 위치·윤곽을 유지한다. 열린 틈을 메우는 세부 패치는 허용한다. 의도된 동굴·석교·통로를 보존한다.
3. 같은 몸체 입력에서 시각 형상과 충돌을 만들고 기반면 절개·내부 잔존 Support 검사를 갱신한다. 입력 배치 지문·몸체 설정·출력 지문을 재현 원본에 기록한다.
4. 전후 동일 시점에서 큰 덩어리·스카이라인·분지/테두리 구도를 대조하고 사용자 PIE로 보행·점프·그래플 접근, 지적 구멍·외피 뒤 진입을 확인한다.

**확인 기준:** 큰 배치 보존, 원치 않는 내부 노출·진입 차단, 의도된 통로 보존, 필요한 보행면의 시각/충돌 맞춤, 이음매·역할·내부 Support·Layer 검사 통과. 사용자가 시각·접근 결과를 확인한 뒤 U5b로 간다. 수치 매개변수와 생성 방식은 이 시험에서 정한다.

#### U5b. 구역별 몸체와 최종 충돌

- U5a 방식으로 나머지 구역의 확정 큰 배치를 몸체화한다. 구역 경계는 서로 연결하되 통로·동굴을 보존한다. 구역마다 작업 전후 구도와 배치 지문을 남긴다.
- 기존 세 충돌 역할(충돌 지각·Floor·Shell)을 유지하며 몸체와 외피에서 프록시를 다시 파생한다. Nav 참여는 별도 판정하고 산괴 배경 결정을 임의로 바꾸지 않는다.
- 몸체 아래 지각·스폰 후보·의도하지 않은 빈 공간, 새로 드러난 외피 뒤와 별도 입구를 확인한다. 전체 배치 재현·베이크는 U6에서 통합 확인한다.
- **Z1Cull 정리(사용자 결정, 2026-11-27):** `Z1Cull_V001` 15레벨은 Static으로 지정했는데도 Movable로 저장되어 있다. 북벽 레벨에는 Static ISM 1개가 남아 로드할 때마다 AttachTo 경고가 난다. U5b에서 Cull 사본을 새 판(`Z1Cull_V002`)으로 다시 만들어 함께 정리한다. 생성 스크립트는 모든 컴포넌트를 붙인 뒤 루트부터 Static을 지정하고 저장 뒤 디스크에서 다시 읽어 확인한다. ISM 합치기(북벽은 798개 중 386개가 인스턴스 1개짜리)도 같이 검토한다. V001은 V008 보존 대상이라 고치지 않는다. 근거는 [외피 시험 §13](../analysis/Phase01_IsleSkinTrial.md)이다.

#### 기존 프록시 절차와 Z1 기술 검증 기록(GX-031~GX-033)

UE에서 내보내는 것:

- 고유 메시는 Nanite fallback LOD로 한 번씩 FBX로 내보낸다. 4M 삼각형 원본은 쓰지 않는다.
- 여기에 배치를 함께 넘긴다. Z1에서 U5가 읽는 입력은 다음과 같다(GX-030).
  - 배치: `EDTerrainLayout_Z1.json`의 `actors`·`level_instances`. 인스턴스 층은 transform을 JSON에 담지 않으므로, JSON에서 만든 시험 레벨(`RidgeZ1Layout.py build`, `verify` PASS)의 Level Instance 하위 레벨에서 실제 world transform을 읽어 내보낸다.
  - 제외: `proxy.exclude_rule`. 메시 상자 꼭대기가 렌더링 지각보다 50cm 넘게 아래인 인스턴스이고, 동굴 절개 안은 판정하지 않는다. V034는 37,014개이며 대부분 02 계열 조립체 벽 뒤 바닥 덮개다. 목록은 Saved `Layout/ProxyExclude_V034.json`이다.
  - 지면: 렌더링 지각 V010(`crust_build.render`). 지면 타일 윗면은 배치 지각(`crust_build.placement`)을 따라 렌더링 지각보다 약 50cm 위다(GX-024).
  - 프록시 후보에서 식생·데칼·납작한 지면 타일을 뺄지, 크기 하한을 얼마로 할지, `_Floor` 높이 기준을 어느 지각에 둘지는 U5 첫 단계에서 정한다.

새 `Scripts/Blender/BuildRidgeEDCollision.py`는 구역별로 다음을 한다.

1. 인스턴스를 만들고 합친 뒤 voxel remesh(약 30cm)와 decimate를 건다.
2. 기반면 아래로 묻힌 면은 지운다.
3. 법선·Up 내적 0.71을 기준으로 `_Floor`와 `_Shell`로 나눈다(`finish()`와 같은 규칙).
4. 작은 walkable 조각(약 4㎡ 미만)은 Shell로 넘긴다. Nav Layer 상한 256과 Pod 같은-Layer 후보를 지키기 위해서다.

**기반면 절개:**

- 큰 ED 조각이 기반면과 만나는 발자국을 지각 cut 다각형으로 넘겨 V012 지각을 다시 굽는다. 기존 동굴 cut과 같은 방식이다.
- 목적은 바위 속에 기반면이 남아 Nav 노드나 Spawn 후보가 바위 안에 생기는 것을 막는 것이다. 베이커는 닫힌 메시 안의 캡슐을 잡지 못한다.
- Blender에서 "닫힌 프록시 내부에 남은 기반면 0"을 assert한다.

UE 반입과 맞춤 검사:

- 이름은 `SM_Ridge_EDTerrain_V001_<Zone>_Floor`와 `_Shell`이다.
- 태그는 [옥탄트 제작 가이드](../../Guide_OctantLevelInstance.md) §4 역할 표를 따른다.
- 충돌은 complex-as-simple 단일 trimesh다. 정점은 옥탄트 안에 있어야 하고 각 성분의 절댓값이 32767 이하여야 한다.
- ED 시각 메시와 프록시의 거리를 표본으로 재서 p95를 보고한다. 목표는 Floor ≤25cm다.

**확인 기준:** 프록시가 단일 trimesh이고 태그 검사를 통과한다. 내부에 남은 기반면이 0이다. p95 맞춤 수치가 기록되어 있다.

**Z1 결과(2026-11-22, GX-031).** 위 계획 중 바뀐 점과 결과는 다음과 같다. 상세는 [Phase 1 로그](../history/Phase01_Log.md)의 "U5 Z1 충돌 프록시"다.

- 형식: 내보내기는 FBX가 아니라 GeometryScript RenderData LOD0 이진 파일이다(`ExportRidgeEDCollision.py export`).
- 규모: fallback 합계가 약 13억 삼각형이라 전체 병합·remesh는 하지 않았다. 지면층(납작한 타일)은 윗면을 50cm 래스터해 충돌 전용 지각 V011의 특징점으로 굽고, 덩어리만 40m 칸 voxel remesh(30cm)한다.
- 기반면: V012 기반면을 직접 고치지 않고, 렌더링 지각 V010과 별도로 충돌 전용 지각 V011(숨김)을 굽는다. 덩어리 발자국 절개는 구운 지각 표본으로 7회 반복해 수렴했다(절개 5,142㎡).
- 결과: V011 133,671·Floor 68,776·Shell 404,399삼각형. 덩어리 안 50cm보다 깊은 지각 표본 0. Floor p95 21.5cm, 보이는 걷는 면 전체 p95 13.0cm. 반입 검사(단일 trimesh·complex-as-simple·옥탄트 안·32767 이하)와 역할 태그 검사 통과.
- ED 시각 메시 충돌: 조립체·CR 레벨·BPP 블루프린트·Drape에 충돌이 켜져 있었다. U5에서 모두 껐다(`visualoff save`). 시험 레벨은 `Z1AssemblyWall_V035`(JSON V035)다.

### U6. 베이크·마커·러프 PIE(게이트)

베이크:

- 고스트를 제거하고 저장한다.
- TestMap03에서 `LNP.SurfaceNav.BakeOctant <WIP LVI>`(schema11)를 실행한다.
- Support/Nav/Spawn 수치와 오류를 기록한다.

마커 보정:

- Pod: strict Support에서 100cm 안, 가장자리 250cm, 캡슐 r120/hh150, 편성 반경 안의 같은-Layer 후보
- 훅, 검토점
- 사선은 `VerifyRidgeReady.py` 변형으로 확인한다.

PIE:

- 검사용 풀은 메모리에서만 임시로 선택한다([환경 실행서](Phase01_EnvironmentImport.md) §22).
- 1인 PIE에서 사용자가 아래 체크리스트로 플레이한다.

| 확인할 것 |
|:---|
| 서쪽 진입 → 계단 상승로 → 왕관 정상 |
| 협곡 통과 |
| 대분지 전투 |
| 잔도 → 북쪽 대지 |
| 골짜기 → 보스 고원 |
| Pod 전투·추격, 훅 |
| 동굴 입구·석교·부유섬 연결 |
| 추락 구멍이 없는지 |

- 함께 기록할 것: `LNP.SurfaceNav.EnemyGround.Report`의 `blocker_rejects`, PIE 로그 `maxSeamRadiusDelta=0.000cm`.
- PIE가 끝나면 원래 풀을 복원하고 해시로 확인한다.

**현재 게이트(GX-034):** U5a에서는 대표 구간의 큰 배치 보존·몸체·접근을 먼저 확인한다. 전체 U4/U5b 뒤에는 이번 R01~R06 및 후속 제보 지점, 일반 동선·점프·그래플·NPC 전투를 다시 확인한다. 사용자가 전체 시각·충돌·동선이 성립한다고 판정하면 U7로 간다. 개구부 한 곳을 막은 것만으로 같은 내부의 다른 입구까지 해결됐다고 판정하지 않는다.

**기존 Z1 기술 검증 경과(2026-11-22~23, GX-032·GX-033).** 상세는 [Phase 1 로그](../history/Phase01_Log.md)의 "U6 Z1 베이크·러프 PIE"다. 이후 2026-11-24 V010 제보 수집은 종료했고, 남은 검증은 GX-034 후속 몸체 단계로 넘겼다.

- LVI: 시험 레벨 복사 대신 JSON에서 `BuildRidgeZ1BakeLVI.py build`로 만든다(조명 제외, 보존 바위 충돌 복원, 시각 Level Instance는 하위 레벨이라 베이크 입력이 아님). Pod는 Pod 없이 구운 판의 스폰 후보(가장자리 여유 260cm 이상)로 옮긴다.
- 베이크: Floor 성분마다 Layer가 생겨 Nav Layer 400개(런타임 상한 256)가 나왔다. Floor 하한 50㎡로 Layer 79~82(GX-032).
- 동선: 산괴(고원·+38m 받침)는 적 Nav가 주 섬과 끊긴 고립 섬이라 배경으로 두고(A안) CrownSummit·StairTerrace2 Pod를 뺐다. 계단 상승로·왕관 정상·석교 북단은 알려진 단절이다.
- 러프 PIE(V007) 지적 7곳을 정리 규칙 A~D(시각 인스턴스 29,150개, GX-033), 동굴 입구 바위 제거, BossValley 경사로(렌더링 지각 V012)로 고치고 U5를 다시 구웠다(충돌 지각 V013·프록시 V003). 재점검 PIE 대상은 `Z1Bake_V010`이다.

### U7. 배경 프랍 채우기(ED 스타일)

- `LNPDecorationSampler`, 규칙 DA, `BuildRidgeDecorationPCG.py`를 재사용하거나 오프라인 배치 도구로 V007 규칙과 그래프를 만든다. 대상은 새 LVI 내부 전체 중 **이미 고밀도로 꾸며진 구역(Z1 CloseRange 층·섬 윗면 V018 등)을 제외한 나머지 바닥**이며 구역별 팔레트를 쓴다.
  - U7은 몸체·충돌·동선이 성립한 뒤(U5b → U6 이후) 한 번에 마감한다. 기존 식생·잔돌은 구도 참고로 유지하되 접지·이음 보완에 필요한 국소 조정을 허용한다(GX-034).
  - 샘플러는 충돌 표면(프록시+기반면)에 접지한다.
  - 작은 바위는 Blocker ISM이다. GX-018 여유를 유지한다. 다만 SSN 수정 뒤 `blocker_rejects`가 거의 0이므로 밀도를 다시 보는 것도 선택지다.
  - 식생과 작은 잔돌(<100cm)은 NoCollision Decoration이다.
- ED 중형 바위와 잔해 같은 주요 배치는 U4 도구로 수동 배치한다.
- 새 식생의 바람 WPO는 구면 접선 프레임으로 바꾼다(`MF_Ridge_FernWind` 방식, 환경 실행서 §15~16).
- Blocker ISM이 바뀌면 다시 베이크하고 PIE로 다시 확인한다.
- **U7 착수 시 고려할 추가 작업(2026-11-28 정리 — 에디터 타임 생성 + 셀 분할 ISM / 거리 컬링 방식):**
  - 런타임 PCG 스트리밍(`GenerateAtRuntime`/3D HiGen)은 기존 `LNPDecorationSampler`의 원점 항등 전제(회전 8슬롯 `ALevelInstance` 미지원)·메인 스레드 일괄 트레이스 히치·구면 3D 그리드 전환 및 `APCGWorldActor` 검증 비용·충돌/SurfaceData 베이크 불일치 때문에 기본 경로로 채택하지 않고, **에디터 타임 생성 → Static ISM 베이크 + 거리 컬링/WPO 제어**를 기본 방향으로 둔다.
  - 위 방식을 옥탄트 전체에 적용하려면 U7에서 아래 3가지 보완이 추가로 필요하다.
    1. **고밀도 구역 제외 마스크와 AABB 과잉 거부 해소:** 현재 `LNPDecorationSampler`는 `LNP.Surface.Decoration` 컴포넌트의 월드 AABB로 겹침을 피하므로 Z1 대형 ISM 덩어리가 인접 빈 바닥까지 거부할 수 있다. Z1·섬 윗면 등 기존 고밀도 구역을 제외 다각형/마스크로 분리하고 대형 ISM의 AABB 과잉 거부를 피하도록 정리한다.
    2. **다중 충돌 표면 접지:** 현재 `LNPDecorationSampler`는 단일 지각 메시(`CrustMesh`) 1개만 트레이스한다. U5b 이후 기반면 + 구역별 `_Floor` 프록시 위에도 접지할 수 있게 샘플러(또는 오프라인 투영 스크립트)를 확장한다. (C++ 수정 시 에디터 빌드 전 사용자 확인)
    3. **공간 셀 분할 Static ISM + 거리 컬링/WPO 비활성화 거리:** 옥탄트 전체(약 14만㎡)를 거대한 단일 ISM으로 두면 프러스텀/거리 컬링 효율이 떨어진다. NoCollision 잔돌·식생 ISM을 구역(셀) 단위로 분할해 굽고, 메시 크기별 `InstanceStartCullDistance`/`InstanceEndCullDistance`·`WorldPositionOffsetDisableDistance`·Nanite를 설정해 런타임 CPU 히치 없이 거리별 LOD·컬링 효과를 확보한다.

**확인 기준:** `InspectRidgeDecorationPCG.py` 계약 검사, 재베이크, PIE 재확인을 통과한다.

### U8. 마감

- 재생성 절차를 남긴다. 에셋과 WIP는 Git에서 제외되므로 원본은 스크립트와 JSON이다.
  - 순서: 반입 목록 → U3 기반면 → U4 전체 큰 배치 초벌·구역별 JSON → U5a 시험 → U5b 몸체·외피 마감·프록시/절개 → U6 베이크·PIE → U7 세부 마감
- Phase 로그(`history/Phase01_Log.md`), [Current](../Current.md), 이 문서의 체크리스트를 갱신하고 화면 비교 캡처를 남긴다.
- 패키지 성능 측정은 1-D에서 GX-001 규약으로 한다.
- 다음 패스로 동굴·부유섬·석교 교체 계획을 남긴다.

## 5. 우려 사항

1. **작업량.** 옥탄트 면적이 약 14만㎡라 여러 세션이 걸린다. 10×10m 시험을 생략한 대신 Z1 파일럿에서 U5·U6을 먼저 통과시켜 파이프라인 위험을 일찍 드러낸다.
2. **디스크와 VRAM.** 추가 반입은 몇 GB 규모로 예상한다. 노트북 GPU는 8GB다. 큰 Nanite 절벽이 많아지면 에디터 응답과 VSM·Lumen 비용이 오른다. 현재 Frame은 19.687ms로 이미 예산 16.6ms를 넘는다. 이 증가는 벤치마크 콘텐츠의 목적에 맞는 것으로 보고 1-D에서 측정한다.
3. **보이는 면과 충돌 면의 차이.** 프록시를 쓰므로 발이 뜨거나 묻히고, 탄환이 보이지 않는 면에 맞을 수 있다. voxel 크기와 decimate로 조정하고 p95로 보고한다.
4. **바위 속 Nav와 Spawn.** 기반면 절개와 Blender assert로 막는다. 베이커에는 이 경우를 잡는 검사가 없다.
5. **Pod 편성.** Pod는 기반면(Layer 0)이나 큰 Floor 한 장 위에 둔다.
6. **곡률(R=300m, 안쪽 오목).** 30m 메시는 가운데가 약 0.4m 처진다. 큰 벽을 쌓을 때는 조각마다 기울인다.
7. **경계 고리의 외형.** 15~55m 고리는 기반면 재질로 통일한다. ED Decoration은 seam에서 5m 바깥까지만 덮어서 전이부를 가린다.
8. **동굴·부유섬·석교.** 1차에서는 V011 형상이 남아 화면에서 이질적으로 보인다. 다음 패스에서 교체한다.

## 6. 체크리스트

- [x] U1 설계도 JSON·고스트
- [x] U2 에셋 카탈로그·반입·해시
- [x] U3 지각 V012 기반면·조기 베이크
- [x] 기존 U5 Z1 프록시·충돌 지각·시각 충돌 끄기의 기술 검증(GX-031, 몸체 완성 아님)
- [x] 이번 U6 V010 PIE 제보 R01~R06 수집 종료·풀/밀도 복원·INI/정식 풀/배치 JSON 해시 검증(전체 U6 통과 아님)
- [x] GX-034 현재 큰 배치 보존·몸체 방식 계획 반영(구현 전)
- [x] U4-0 실제 상태 대조·Z1 큰 배치 보존 목록·복구 사본·비교 시점 고정·전체 구역 작업 순서 제안
- [x] U4-1~10 검사/export·사용자 채택, V010 동결·19소유자 보존·새 세션 인계
- [x] U4-11 남동·북동 외곽2개 배치·실제 검사/export·보존 검사·30시점 비교
- [x] U4-11 사용자 채택·V011 큰 배치 기준 동결·외곽 추가 장식 종료·내부 검토(GX-036)
- [x] U5a-0 채택21소유자/입력 해시·51보호 공간·대표 두 유형 입력 조사
- [x] U5a-1 제한된 닫힌 몸체 생성·기존 보존 검사·16시점 비교(경사로 REVIEW·미채택)
- [x] U5a-2 경계 보정·실제 보존/16비교(외관/경사로 REVIEW)
- [x] U5a-3 별도 연결 초안·실제 보존/22비교·동선 유지 사용자 승인
- [x] U5a-4 연결 석재 외형/받침·26비교·실제 전체 보존/858정점 검사·사용자 유지
- [x] U5a-4 V005 연결 외형 유지 사용자 승인
- [x] U5a-5 몸체 캡/직선 경계/재질·26비교·실제 보존/73,111정점 검사·V006 외관 사용자 유지
- [x] U5a-6 자기 교차 분류·V007 최소 보정·실제 검사/26비교·사용자 채택
- [x] U5a-7 R01 국소 보강 V008 실제 검사·27비교/복구·사용자 외관 채택(GX-037)
- [x] U5a 몸체/별도 연결 V008 현재 외관 채택·다음 단위 새 세션 인계(전체 몸체·물리 게이트와 구별)
- [ ] U5a 공통 몸체에서 충돌/지각·내부 Support 검사와 실제 접근 검증
- [ ] U4 Z2~Z7 포함 전체 큰 지형 초벌·구역별 배치 JSON 확정
- [ ] U5a Z1 대표 구간 몸체·시각/충돌·접근 시험
- [ ] U5b 구역별 몸체·외피 이음·전 구역 프록시·기반면 절개·Z1Cull_V002 Static 정리(북벽 AttachTo 경고 제거)
- [ ] U6 전체 베이크·러프 PIE 게이트(사용자 판정)
- [ ] U7 배경 프랍·재베이크·PIE
- [ ] U8 재생성 절차·로그·캡처

## 7. 핵심 파일

| 구분 | 파일 |
|:---|:---|
| 재사용(지형) | `Scripts/RidgeOctantSpec.py`, `Scripts/Blender/BuildRidgeOctant.py`, `Scripts/Blender/VerifyRidgeOctant.py`, `Scripts/AssembleRidgeOctant.py`(`snap_seam` 53-85, 태그 163-202), `Scripts/BakeRidgeOctant.py`, `Scripts/RunRidgeStage.ps1`, `Scripts/VerifyRidgeReady.py` |
| 재사용(장식) | `Scripts/Profiling/Graphics/BuildRidgeDecorationPCG.py`, `InspectRidgeDecorationPCG.py`, `DrawRidgeDecorationKeepOuts.py` |
| U1 재생성 원본 | `Scripts/Profiling/Graphics/ExportRidgeBlueprint.py`, `RidgeBlueprint_V001.json`, `BuildRidgeBlueprintGhost.py` |
| U2 카탈로그·반입 검증 | `Scripts/Profiling/Graphics/InspectRidgeEDTerrain.py`, `EDTerrainCatalog_V001.json`, `ElectricDreamsPalette.json` |
| U3 기반면·검증 | `Scripts/RidgeEDBaseSpec.py`, `Scripts/Blender/BuildRidgeEDBase.py`, `Scripts/Blender/VerifyRidgeEDBase.py`, `Scripts/Profiling/Graphics/AssembleRidgeEDBase.py` |
| U4 배치 도구·비교본 | `Scripts/Profiling/Graphics/PlaceRidgeEDTerrain.py`, `EDTerrainPlacement_V001.json`, `AssembleRidgeZ1Composition.py`, `Scripts/RidgeZ1CompositionSpec.py`, `Scripts/Blender/BuildRidgeZ1Composition.py` |
| U4 조립체·산괴 시험 | `Scripts/Profiling/Graphics/BuildRidgeZ1AssemblyTest.py`, `Scripts/Profiling/Graphics/BakeEDCloseRange.py`(ElectricDreamsEnv 에디터 전용) |
| U4 Z1 재현 원본(GX-030) | `Scripts/Profiling/Graphics/EDTerrainLayout_Z1.json`, `Scripts/Profiling/Graphics/RidgeZ1Layout.py`, `Scripts/Blender/HashRidgeCrust.py` |
| U5 Z1 충돌(GX-031) | `Scripts/Profiling/Graphics/ExportRidgeEDCollision.py`(export·import·visualoff·layout·check), `Scripts/Blender/BuildRidgeEDCollision.py`, `Scripts/Blender/BuildRidgeOctant.py`(`crust`의 `cut_test`) |
| U6 Z1 베이크·정리(GX-032·GX-033) | `Scripts/Profiling/Graphics/BuildRidgeZ1BakeLVI.py`(build·check·bake·nav·survey·pool), `Scripts/Profiling/Graphics/CullRidgeZ1Instances.py`(export·build), `Scripts/Blender/DecideRidgeZ1Cull.py`, `Scripts/Blender/BuildRidgeEDCollision.py`(`-- split`, `-- build V003`), `Scripts/RidgeZ1CompositionSpec.py`(`RAMPS`·`massif_ramp_ground`) |
| 후속 몸체 저작·검증(계획) | U5a 착수 때 빌더·설정·보존 목록·비교 시점의 파일 경로를 정하고 구역 배치 JSON과 연결한다. 기존 V037·충돌 V003은 복구 기준으로 보존한다. |
| 계약 참조 | [옥탄트 제작 가이드](../../Guide_OctantLevelInstance.md) §3~7, `SurfaceSupportNavigation/design/TerrainContract.md`, `SurfaceSupportNavigation/design/SurfaceBaking.md`, [Ridge 설계 원본](../../../Art/Octants/Ridge_00/README.md) |

## 8. 이전 시험 기록(01~04, 2026-11-18)

- 원본 구도는 `/Game/Levels/PCG/ElectricDreams_PCG`에 있다. 기준 카메라는 위치 `(89993.03, -7753.45, 1915.32)`cm, 회전 `(Pitch 6.00°, Yaw 82.40°, Roll 0.00°)`, FOV 90°다.
- 화면·카메라 값·원본 메시와 재질 경로는 `Saved/Profiling/Graphics/Phase01/RidgeReferencePatch_20261118/`에 있다(`ReferenceTerrainAssets.json`, `01_RidgeSectionPlan.md`~`04_DecorationComparison.md`).
- 01~04에서 한 시험:
  - V011 바닥에 하상 재질을 섞었다(`MI_Ridge_ReferenceRiverbed_V001`).
  - 벽 일부에 `SM_Cliff_01`로 만든 층리 외피를 얹었다(`SM_Ridge_ReferenceEastCliff_V001`, 약 400만 삼각형, NoCollision).
  - 이렇게 접합과 플레이 검증 절차를 확인했다.
- 사용자 판정: 밝은 하상 재질과 절벽 조각은 조립 재료로 쓸 만하다.
- 남은 문제: 하상 타일 반복, 외피 둘레의 직선 재질 경계, 400만 삼각형 형상의 경량화.
