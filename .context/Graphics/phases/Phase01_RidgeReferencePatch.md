# Phase 1-BR — Ridge 지표 Electric Dreams 교체

> 상태: U1~U3 완료 · U4 Z1 서쪽 산괴 기준 시험본 V034(GX-023~GX-029, 남 둑 3단·북 둑 2단 + ED 둑 묶음 띠 + 북쪽 단·남서 모서리 평면 묶음) · 재현 원본 `EDTerrainLayout_Z1.json` 확정(GX-030) · 다음은 U5 Z1 충돌 프록시
> 순서: 1-B5f → 1-BR(기존 1-B6 전체 교체를 흡수) → 1-C
> 상위: [그래픽스 로드맵](../Roadmap.md) §2·§4, [Phase 1 실행서](Phase01_BenchmarkContent.md) §3, [확정 결정](../Decisions.md) GX-019·GX-021
> 마지막 갱신: 2026-11-22

## 1. 목표

Ridge_00(V011)는 플레이 동선만 고려한 로우폴리 프로토타입 지형이다. 이 Phase에서는 옥탄트 경계 전이부를 뺀 **Ridge 지표 전체**를 Electric Dreams(ED) 지형 에셋과 ED 스타일 배경 프랍으로 교체한다.

- V011은 설계도로만 쓴다. 직행·우회, 전투장, 엄폐·사선, Pod·훅·착지를 읽는 기준이다.
- 세부 형태는 바뀌어도 된다. 플레이 동선이 대략 성립하면 된다.
- 기존 10×10m 시험 구역 단계는 생략한다(GX-020 대체, GX-021). 대신 첫 구역 Z1을 파일럿으로 삼아 충돌·베이크 파이프라인을 먼저 통과시킨다(§4 U4).

진행 순서는 다음과 같다.

1. 지각을 새로 굽는다.
2. ED 지형을 조립한다.
3. 러프 PIE로 동선을 확인한다(게이트).
4. 배경 프랍을 채운다.

## 2. 확정 방식(GX-021, 2026-11-19 사용자)

| 항목 | 결정 |
|:---|:---|
| 보행면 | **고리+기반면.** 지각은 경계 고리와 매끈한 기반면 한 장으로 만든다. 기반면에는 ED 지형 재질을 입히고, ED 원본의 Landscape 역할을 맡긴다. 절벽·능선·메사·첨탑·둑은 ED 메시로 세운다. |
| 충돌 | **배치에서 파생한 프록시.** ED 메시는 보이기만 한다(`Decoration`/`LNPDecoration`). 구역별 배치를 Blender에서 합쳐 저폴리 `_Floor`(Support+Blocker)와 `_Shell`(Blocker)을 따로 굽는다. |
| 범위 | **지표 먼저.** 동굴(대전당·산괴 동굴), 부유섬 2개, 하늘 석교는 V011 형상을 임시로 유지하고 다음 패스에서 교체한다. |
| 조립 | **초벌은 스크립트, 다듬기는 수동.** V011 고스트 설계도를 깔고 구면에 정렬한 초벌 배치를 Python으로 놓는다. 사용자가 에디터에서 구도와 이음을 다듬는다. 결과는 JSON으로 되읽어 재생성 원본으로 삼는다. |

이 방식을 고른 이유는 다음과 같다.

- 옥탄트 면적은 약 14만㎡이고 한 변이 약 470m다. ED 바닥 메시는 수 m짜리 패치이고 가장 큰 절벽도 30m 정도다. 그래서 넓은 평지를 ED 메시만으로 덮을 수 없다. 틈이 생기면 그 자리로 외우주에 추락한다.
- 베이크는 일반 StaticMesh의 단일 complex-as-simple trimesh만 Support로 받는다. ISM Support는 막혀 있다(SSN D-058). ED Nanite 메시는 대부분 단순 충돌이 없다.
- 바닥이 조각나면 Layer가 갈린다. 그러면 Pod 편성 후보(같은 Layer, 반경 15m 안 27명)가 모자란다. Ridge V007에서 실제로 그런 일이 있었다.

**C++ 변경은 계획에 없다.** 기존 베이커·수집기·PCG 샘플러 계약 안에서 해결한다. C++ 변경이 필요해지면 에디터 빌드 전에 사용자에게 확인한다.

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

새 `Scripts/Profiling/Graphics/PlaceRidgeEDTerrain.py`의 서브커맨드는 다음과 같다.

| 서브커맨드 | 하는 일 |
|:---|:---|
| `place` | 배치 JSON을 읽어 액터를 놓는다. 항목은 에셋, 로컬 X/Y, 높이 오프셋, 현지 Up 기준 yaw, 기울기, 스케일이다. 현지 Up은 `-normalize(pos)`이고 메시 +Z를 현지 Up에 맞춘다. 결과는 `EDTerrain/<Zone>` 폴더의 StaticMeshActor(`Decoration`/`LNPDecoration`)다. |
| `export` | 사용자가 손으로 다듬은 실제 transform을 되읽어 `EDTerrainPlacement_V001.json`에 쓴다. 이 파일은 Git에 추적하는 재생성 원본이다. |
| `clear` | 해당 구역을 지운다. |

구역 순서는 다음과 같다.

| 구역 | 범위 |
|:---|:---|
| **Z1(파일럿)** | 대분지·협곡 하류. Z1만 끝내면 U5·U6을 Z1에 한 번 돌려 프록시 파이프라인을 검증한 뒤 나머지로 간다. |
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

**게이트:** 사용자가 동선이 성립한다고 판정하면 U7로 간다.

### U7. 배경 프랍 채우기(ED 스타일)

- `LNPDecorationSampler`, 규칙 DA, `BuildRidgeDecorationPCG.py`를 재사용해 V007 규칙과 그래프를 만든다. 대상은 새 LVI 내부 전체이고 구역별 팔레트를 쓴다.
  - 샘플러는 충돌 표면(프록시+기반면)에 접지한다.
  - 작은 바위는 Blocker ISM이다. GX-018 여유를 유지한다. 다만 SSN 수정 뒤 `blocker_rejects`가 거의 0이므로 밀도를 다시 보는 것도 선택지다.
  - 식생은 Decoration이다.
- ED 중형 바위와 잔해 같은 주요 배치는 U4 도구로 수동 배치한다.
- 새 식생의 바람 WPO는 구면 접선 프레임으로 바꾼다(`MF_Ridge_FernWind` 방식, 환경 실행서 §15~16).
- Blocker ISM이 바뀌면 다시 베이크하고 PIE로 다시 확인한다.

**확인 기준:** `InspectRidgeDecorationPCG.py` 계약 검사, 재베이크, PIE 재확인을 통과한다.

### U8. 마감

- 재생성 절차를 남긴다. 에셋과 WIP는 Git에서 제외되므로 원본은 스크립트와 JSON이다.
  - 순서: 반입 목록 → U3 빌더 → U4 `place` → U5 프록시 → 베이크 → U7
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
- [ ] U4 Z1 파일럿 조립
- [ ] U5·U6 Z1 파일럿 프록시·베이크·PIE
- [ ] U4 Z2~Z7 조립
- [ ] U5 전 구역 프록시·기반면 절개
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
| 후속 신규 | `Scripts/Blender/BuildRidgeEDCollision.py` |
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
