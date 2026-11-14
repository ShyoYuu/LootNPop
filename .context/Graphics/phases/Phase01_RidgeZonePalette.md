# Phase 1 단위 1-B0 — Ridge 대표 구역·Electric Dreams 팔레트 선정

> 상태: 완료 · 구역/톤/재질 방침 및 6종 조건부 팔레트 선정, 반입 시험 전
> 작성: 2026-11-13
> 마지막 보강: 2026-11-14
> 상위 문서: [Phase 1 실행서](Phase01_BenchmarkContent.md) 1-B 환경
> 결정 근거: GX-013(기존 Ridge + Electric Dreams, 새 옥탄트 없음, 1차 장식은 보행 충돌 불변)

## 1. 목적과 범위

Ridge V011에서 먼저 꾸밀 대표 구역 하나를 정하고, Electric Dreams 메시·재질의 **조건부 팔레트**와 장식 원칙을 고른다. 대표 구역을 먼저 꾸민 뒤 Ridge 전체로 확장한다(2026-11-13, 사용자 확인). 대표 구역 경계는 최종 장식을 그 안으로만 제한하는 경계가 아니다. 최종 팔레트와 세부 허용치는 다음 단위의 작은 반입 시험 뒤 확정한다. 이 단위는 **선정과 후속 검토 항목 정리까지**다.

권장 접근은 **기존 지각의 재질로 큰 면을 완성하고, 필요한 부분에 메시를 보강**하는 것이다. 큰 절벽 메시로 지각 전체를 덮을 수 있다고 전제하지 않는다. 톤·구역·역할별 채택 여부는 작업 중 사용자 확인과 실제 적합성 검토로 정한다.

이 문서는 여러 세션의 인계 기준이다. 각 단계에서는 다음 선택에 필요한 만큼만 조사하고, 세부 배치량·여유 폭·투영 방식은 해당 작업 세션에서 분석한다. 결과에 확정 사항·조건부 선택·미확인 사항을 구분해 남기며, 모든 기술적 의문을 1-B0에서 해소할 필요는 없다.

| 포함 | 제외(다음 단위) |
|:---|:---|
| 구역 경계 초안, 면 분류·면적 개요 | Electric Dreams 에셋을 LootNPop으로 반입 |
| 장식 금지·제한 원칙, 접촉 오차 검토 항목 | PCG 그래프 제작, 배치·돌출 수치 확정 |
| 역할별 조건부 팔레트, 반입 의존성 규모 | Support/Nav 베이크, 패키지·측정 |
| 지면 재질 방침(교체 여부와 확인 항목) | git 제외 경로 설정, 배치 방식 결정 |
| 카메라 A/B/C 후보 위치 메모 | 하네스 카메라 코드 수정 |

## 2. 입력

| 자료 | 경로 |
|:---|:---|
| Ridge 구성·연결표·검토점 | [Ridge README](../../../Art/Octants/Ridge_00/README.md), [정식 통합](../../../Art/Octants/Ridge_00/Production.md) |
| Ridge 제작 경위 | [Phase09_RidgePrototype](../../SurfaceSupportNavigation/phases/Phase09_RidgePrototype.md) §9~§16 |
| 형상 원본 | `Scripts/RidgeOctantSpec.py`(`PLATES`·`BASINS`·`PATHS`·`ROCKS`·`PODS`·`ANCHORS`·`NODES`·`VIEWS`), `Art/Octants/Ridge_00/Ridge_00_V011.blend` |
| 평면·단면·렌더 | `Art/Octants/Ridge_00/Previews/V011_Plan.png`, `Sections.svg`, `Previews/V011_*Eye.png` |
| 보조 스크립트 | `Scripts/PreviewRidgeOctant.py`, `Scripts/WriteRidgeSections.py`, `Scripts/Blender/RenderRidgeViews.py`, `Scripts/Blender/VerifyRidgeOctant.py` |
| Electric Dreams 검토 | [검토 보고서](../analysis/ElectricDreamsReview_20261112.md), 원자료 `Saved/Profiling/Graphics/Phase01/ElectricDreamsReview_20261112/`(`AssetInventory.json` 메시 600개, `ActiveComponents.json` CloseRange 액터·메시·재질 오버라이드, `AssetSamples.json`, `DependencyClosure.json`, `MaterialAndGraphs.json`) |
| 원본 프로젝트 | `D:/UnrealProjects/ElectricDreamsEnv`, UE 5.8.4, MCP `unreal-mcp-second`(8001). 읽기 전용 |

## 3. 대표 구역 후보

좌표는 Ridge spec의 로컬 X/Y/H(cm)이며 H는 현지 Up 쪽으로 증가한다. 개략 구성은 Ridge README를 참고하되 경계 계산에는 V011 spec·실제 메시를 기준으로 삼는다.

| 후보 | 범위 | 장점 | 단점 |
|:---|:---|:---|:---|
| **A. 대분지 허브(권장)** | spec 분지 중심 (3000,-300), 평탄 반경 23m·falloff 15m·H=-40m. Pod·검토점은 (3300,-300). 산괴 동쪽 대절벽 x≈-2200→+700(76→-40m), 협곡 하류 출구, 잔도 하단 | 세 경로 합류부라 군중과 환경을 함께 보는 후보로 적합하다. 정상 가장자리의 하향 시점과 분지 바닥 시점을 얻고 하상 패치를 활용할 수 있다 | 실제 적 노출과 절벽 화면 점유율은 구도로 확인해야 한다. 넓은 면의 외장 수량과 분지 조명을 검토해야 한다 |
| B. 협곡·석교 | 협곡 (-14500,-7800)→(1200,-2600), 협곡 방 Pod, 석교 +38m | CloseRange 협곡 구도와 가장 닮았다 | 폭 13~26m라 카메라와 군중 시야가 짧다. 외장이 통로를 침범하기 쉽다 |
| C. 서쪽 진입·계단 상승로 | 진입 (-17600,0), 경사로 4개 | 첫인상 장면 | 7m 경사로가 절벽면에 붙어 있어 돌출 여유가 거의 없다. Pod 1개 |

A를 권장하고, 협곡 하류의 짧은 구간을 연결 구간으로 붙이는 안이다. 사용자가 Z2에서 확정한다.

## 4. 구역 확정 절차

### Z1. 경계 초안 (원본만 읽는다)

1. 기존 평면도와 spec 좌표로 구역 경계 초안을 정한다. 이음매 보존 띠 1,500cm와 전이 띠 4,000cm는 기존 구면 규약에 따라 제외한다. 사용자 구역 확인 전부터 정밀 면적 계산을 확대하지 않는다.
2. 구역 안의 완만한 면·중간 경사·급경사, 암반·거석, 경로, Pod 편성 반경, 훅과 지하 입구를 구분한다. 경사는 월드 Z 대신 구면 현지 Up과 표면 법선으로 계산한다(`PreviewRidgeOctant.py` 참고). 30°·70°를 쓰면 장식용 초안 분류임을 표시하고 실제 Support/보행 판정과 혼동하지 않는다.
3. 구역 선택에 필요한 오버레이와 면적 개요를 남긴다. spec 높이장 추정과 실제 메시 표면 면적을 구분하고, 지하·암반처럼 겹치는 표면은 필요한 경우 레이어를 구분한다. 전체 면적뿐 아니라 금지·제한 영역을 제외하고 어떤 장식을 놓을 수 있는지 확인한다. 분지 Pod의 기본 편성 반경 25m도 반영한다.
4. Blender 원본·spec·LootNPop 에셋은 수정하지 않는다. 계산용 스크립트는 저장소의 작업 경로 밖(세션 스크래치 또는 §6의 Saved 폴더)에 둔다.

### Z2. 사용자 PIE 확인

LootNPop 에디터(8000)에서 사용자가 서버 콘솔 `LNP.Debug.TeleportToReview <Name> <Slot>`으로 다음을 본다. Ridge가 배정된 slot은 시작 로그에서 확인한다.

- `RidgeCrownEastRim`, `RidgeDeepBasin`, `RidgeValleyRise`, `RidgeTablelandRim`

협곡 하류 출구는 spec `RiftCanyon` 끝점 (1200,-2600) 부근을 분지에서 이동해 확인한다. `RidgeCanyonMouth`는 서쪽 입구 (-14500,-7800)이며 하류 확인점이 아니다. 서쪽 진입 구도 비교가 필요할 때만 추가한다.

확인할 것은 세 가지다.

- 구역 범위가 화면에서 하나의 장면으로 읽히는지
- 벤치마크 카메라 A/B/C 후보가 바닥 근접·절벽 측면·허브 전경을 보여 주는지. 면적보다 화면에서 환경과 적이 함께 드러나는지를 본다
- 분지 조명과 적 식별성. 이상이 보이면 기존 지각과 장식의 영향을 구분할 후속 확인 항목으로 남긴다

적 밀도는 사용자가 정한다. 구도는 0으로 시작할 수 있고, 구역 확정 전에는 실제 적 노출을 밀도 1에서 짧게 확인하는 안을 권한다. 상세 전투 검증은 후속 배치 단계가 맡는다.

여기서는 카메라 후보만 남긴다. 1-A에서 실제 시드·slot과 A→B→C→A 연결 구간까지 확인해 경로를 고정하고, 콘텐츠 반입 전에 출발 기준선을 잰다. 이후 경로가 바뀌면 장식 전 기준선도 같은 경로로 다시 확보한다. 같은 Ridge가 여러 slot에 배정되면 장식·재질의 적용 범위도 확인한다.

### Z3. 장식 금지·제한 규칙

| 영역 | 규칙 |
|:---|:---|
| 경로 통로, 경사로, 4m 착지부 | 통행·착지를 침범하는 장식 금지. 경로 여유는 중심선이 아닌 기존 통로 가장자리에서 추가한다(좌우 2m는 초안) |
| Pod 편성 반경 | 지피식물·바닥 패치만. 엄폐로 오인될 바위·나무 금지 |
| 훅 시야선·접근·착지 공간 | 조준과 이동을 가리는 장식 금지. 출발점→앵커 직선만으로 충분하다고 보지 않는다 |
| 검토점·카메라 주변 | 시야와 카메라 공간을 침범하는 장식 금지. 낮은 표면 디테일까지 일괄 제외하지 않는다 |
| 이음매 보존·전이 띠 | 금지 |
| 쌍둥이 첨탑, 하늘 석교, 부유섬 | 1차 제외 |
| 지하 입구 테두리 | 입구 윤곽을 가리지 않는 범위만 |

이번에는 보호할 영역과 이유를 정한다. 정확한 여유 폭과 배치 마스크는 후속 배치 세션에서 정하며, 배치점뿐 아니라 메시의 회전·스케일·수관 등 전체 범위로 침범 여부를 본다.

### Z4. 시각 표면과 충돌의 정합

1차 장식은 충돌이 없다. 외장이 충돌면 앞으로 튀어나오면 다음 문제가 생긴다.

- 카메라가 메시 속으로 파고든다(카메라는 충돌만 본다).
- 벽보행 NPC가 시각 메시 안을 걷는 것처럼 보인다(정확한 월드 충돌을 따른다).
- 착지와 발 위치가 실제 판정과 어긋나 보인다.

뒤로 묻은 부분은 기존 불투명 지각에 가려지므로, 이 방법만으로 큰 절벽 전체가 자연스럽게 덮인다고 가정하지 않는다. 바닥·착지부·벽보행 접근면의 접촉 오차와 큰 메시의 구면 접합을 작은 반입 시험에서 확인한다. 일률적인 돌출 허용치를 지금 확정하지 않는다.

시험에서 충분한 노출과 접촉 정합을 함께 얻기 어렵다면 외장 범위를 줄이고 지각 재질로 보완한다. 기존 지형을 숨기거나 충돌을 추가해 해결하는 변경은 별도 범위다. 이번 결과에는 위험한 역할과 시험할 후보만 남긴다.

## 5. 팔레트 선정 절차

### P0. 톤 결정 (사용자)

원본은 두 계열로 나뉜다. CloseRange는 실제로 둘을 섞어 쓴다(액터 단위 사용: SandstoneBoulder 12, MossyForestBoulder 14).

- **사암 협곡:** HugeSandstoneCliff, MassiveSandstoneCliff, SandstoneCliff, CanyonSandstoneRidge, SandstoneBoulder, SandstoneRockyGround
- **이끼 숲:** MossyForestBoulder, MossyForestRock, MossyRockFace, ForestRockFormation, ForestRockShelf, Embankment, GrassyEmbankment

반대편 옥탄트가 머리 위로 보이는 구조라 월드 전체 톤도 함께 고려한다. 사용자는 사암, 이끼, 혼합 중 하나를 고른다. 혼합이면 역할별로 어느 계열을 쓸지 정한다.

### P1. 지각·지면 재질 방침

메시로 덮어야 할 범위를 판단할 수 있도록 큰 면의 재질 방침을 먼저 검토한다. 지각(`SM_Octant_Ridge_00_V011_R30000`)과 암반 외피의 UV·현재 재질을 읽기 전용으로 확인하고, Electric Dreams 재질 재사용과 텍스처를 활용한 간단한 재질 중 후보를 고른다.

이번에는 교체 범위(대표 구역 또는 Ridge 전체), 후보 재질·텍스처, 반입 시험 질문까지만 남긴다. UV가 부적합하면 트라이플래너가 유력한 대안이지만 필수 방식으로 확정하지 않는다. 투영 좌표·텍스처 스케일·노멀 처리 등 구현은 해당 세션에서 정한다. 월드 정렬 자체와 월드 Z 기반 상하 마스크를 구분하고, 구면 현지 Up·slot 회전·이음매, WPO에 의한 접촉면 변화를 후속 확인 단서로 남긴다.

### P2. 원자료로 역할별 후보 추리기

필요한 역할부터 소수의 주후보와 예비 후보를 남긴다. 아래 역할을 전부 채우거나 역할마다 같은 수의 후보를 조사할 필요는 없다.

| 역할 | Ridge 형상 | 후보 계열 |
|:---|:---|:---|
| 대절벽 부분 보강 | 116m 대절벽, 80m 협곡 벽 | SM_Cliff(SmartAssets 19종), MassiveSandstoneCliff, HugeSandstoneCliff. Z4 반입 시험 전제 |
| 단 가장자리 6m 띠 | 19m 간격 지층 단 가장자리 | ForestRockShelf, CanyonSandstoneRidge, MossyRockFace |
| 거석 주변 바위 | 분지 거석, 외곽 바위 | SandstoneBoulder, MossyForestBoulder, ForestRockFormation |
| 바닥 패치 | 분지 바닥, 하상 | DryRiverBed, ForestTerrain, SandstoneRockyGround, SmallStonesPack |
| 지피·저층 식생 | 보행면, Pod 반경 | CloverVarieties, BeechFern, Fern, KikuyuGrass, DeadLeaves |
| 나무 | 대지, 단 윗면 | EuropeanHornbeam(약 16m) |
| 덩굴·뿌리(선택) | 절벽면 | CliffVine, HangingVine, ErosionRoots |

선별 기준:

- 로컬 크기가 Ridge 치수(6m 띠, 19m 단, 80~116m 벽, 7m 경사로)에 맞는가
- Nanite 삼각형과 fallback 삼각형
- 재질 슬롯 수와 CloseRange 배치 시 덮어쓰는 재질(`overrideMaterials`)
- 실효 재질의 Opaque/Masked·양면·WPO 여부. Nanite 삼각형 수만으로 비용을 판정하지 않는다
- 충돌 정책(1차는 끄지만 원본 상태를 기록한다)
- CloseRange 사용 빈도(검증된 조합 우선)
- 헬퍼·시험 메시를 걸러 낸다(`/Game/Meshes/_GENERATED/`, `Custom/IvyTest`, `Custom/RootsTest` 주의). 슬롯이 `None`이면 실제 배치 오버라이드로 완성되는 후보인지 먼저 확인한다

원자료에서 확인되는 정보부터 쓰고, 선택에 필요한 누락 정보만 8001 에디터에서 읽기 전용으로 조회한다. 예상 스케일·배치량·중첩은 선정에 영향을 주는 경우만 개략 메모하고 정확한 밀도는 배치 단계로 넘긴다.

### P3. 8001 에디터 시각 확인

- 후보 메시를 원본 맵에서 볼 수 있는 위치로 뷰포트를 옮겨 MCP로 캡처한다. 데스크톱 전체 캡처는 쓰지 않는다.
- 원본 에셋과 레벨은 편집·저장하지 않는다. 시작·종료 시 미저장 변경 상태를 비교하고, 이미 있던 변경도 저장하거나 되돌리지 않는다.
- 캡처와 추천 이유를 사용자에게 보여 조건부 채택안을 고른다. 구면에서의 크기·접합·바람·재질 적합성은 아직 미검증임을 구분한다.

### P4. 조건부 팔레트 의존성 재계산

- 채택안의 메시뿐 아니라 실제 오버라이드 재질과 P1 지면 재질·텍스처를 시작점에 포함해 `/Game` 전이 의존성을 중복 없이 추적한다. 패키지 수, 원본 크기, 텍스처 수와 VT 여부를 기록한다. 예비안은 구분한다.
- 메시 8~10개·원본 약 2GB는 초기 작업 규모의 가이드다. 넘으면 역할 필요성·공유 의존성을 보고 범위 축소를 검토하며, 이 수치로 렌더 성능을 판정하지 않는다.
- 원본 디스크 크기를 VRAM이나 패키지 크기로 해석하지 않는다.

## 6. 산출물과 후속 세션 인계

결과 요약은 §8에 쓴다. 오버레이·면적 개요·의존성 목록 등 원자료는 `Saved/Profiling/Graphics/Phase01/RidgeZonePalette_<날짜>/`에 두고 요약과 경로를 남긴다. 면적의 정밀도와 미확인 정보도 표시한다.

1. 구역 경계·면적 개요와 장식 보호 영역
2. 역할별 조건부 팔레트: 경로, 확인한 크기·Nanite/fallback·실효 재질·원본 충돌 정책, 적용 면과 미확인 사항
3. 지각·지면 재질 방침과 채택안 전체 의존성 규모
4. 카메라 A/B/C 후보 메모와 사용자 확인 범위
5. 후속 세션의 우선 확인 항목과 다음 행동

후속 단위의 실행서는 착수할 때 구체화한다. 아래는 그때 분석할 단서이며 1-B0의 추가 완료 조건이 아니다.

| 시점 | 검토 단서 |
|:---|:---|
| 1-A 경로·출발 기준선 | 실제 slot·연결 구간 무관통·적과 환경의 노출. 장식 전후 같은 경로 유지 |
| 작은 반입 시험 | 절벽·바닥·식생 등 주후보 소수로 시작. 노출과 접촉 정합·구면 접합·VT·바람·회전 확인 후 팔레트 최종 확정. 실패한 역할은 축소·대체 |
| PCG 배치 | 현지 Up·표면 법선, 역할별 보호 영역, 메시 전체 범위, 고정 시드·수량·중첩. 원본의 평면 PCG 그래프를 그대로 쓸 수 있다고 가정하지 않음 |
| 충돌·기존 데이터 검증 | 장식 컴포넌트는 `LNP.Surface.Decoration` Component Tag와 `LNPDecoration` profile만 사용([Terrain Contract](../../SurfaceSupportNavigation/design/TerrainContract.md)). 기존 지형·Support source 보존과 SurfaceData 현재성·런타임 로드 확인 |
| 표시·성능 검증 | 근접 접지·카메라·벽보행·탄환·적 식별성. 조명 이상 시 기존 지각과 장식을 구분해 Lumen Scene/Surface Cache 확인. 식생의 Masked·WPO·그림자·중첩과 VT/메모리 압박은 패키지에서 확인 |

세션마다 수행한 단계·미확인 사항·다음 행동을 §8에 남기고, [Current](../Current.md)에는 상태와 다음 행동을 요약한다. 근거와 경과는 [Phase 1 로그](../history/Phase01_Log.md)에 기록한다. 단위가 끝나면 상위 실행서 1-B 행도 갱신한다. 미확인 세부 사항은 후속 담당 단계와 함께 넘기면 된다.

## 7. 완료 체크리스트

- [x] Z1 경계 초안·오버레이·면적 개요(추정/실제 구분)
- [x] Z2 사용자 PIE 확인(구역 범위·카메라 후보)
- [x] Z3 보호 영역 원칙, Z4 접촉 정합 위험·반입 시험 항목
- [x] P0 톤 사용자 결정
- [x] P1 지각·지면 재질 방침
- [x] P2 역할별 후보, P3 사용자 조건부 선택
- [x] P4 메시·오버라이드·지면 재질을 포함한 의존성 규모
- [x] 후속 검토 항목·다음 행동과 Phase 1 남은 범위 확인
- [x] Current·실행서·로그 갱신

## 8. 결과

### 8.1 완료 단계·사용자 확인(2026-11-13)

**1-B0 완료.** Z1 경계/면적 개요, Z2 사용자 PIE, Z3 보호 원칙, Z4 접촉 위험, P0 톤, P1 재질 방침, P2·P3 조건부 메시 선정, P4 의존성 정리를 마쳤다. 반입 시험을 통과한 최종 팔레트나 장식 구현 완료를 뜻하지 않는다.

- **구역·순서:** 사용자는 아래 대분지 대표 구역을 먼저 꾸민 뒤 Ridge 전체로 확장하는 안을 채택했다. 이 경계가 최종 장식을 그 안으로만 제한하는 경계는 아니다.
- **P0:** 사암 주조, 이끼·낮은 식생은 분지 바닥·하상에 제한한다.
- **Z2:** 일반 PIE의 Ridge **slot 5**에서 DeepBasin·CrownEastRim·ValleyRise·TablelandRim 명령을 제공했고, 사용자는 대표 장면·구도·분지 밝기·적 식별성/이동에 대해 **"모두 문제없어"**라고 확인했다. 벤치마크 시드 1 배정·정식 A/B/C 좌표/연결 경로·밀도별 검증은 별도다.
- **P1:** 사용자는 **원본 지형 재질 파생본 + 보강 메시 원본 재질 유지**를 채택했다. 아래 여섯 메시도 **반입 시험 전까지 조건부 채택**했다.
- **P3:** 8001 CloseRange에서 사용자가 바닥과 높은 절벽이 함께 보이는 구도를 잡았다. MCP 캡처와 여섯 메시 썸네일을 확인해 추천했고 사용자가 6종을 채택했다. 구면 접합·회전·최종 접촉 품질은 확인하지 않았다.

### 8.2 채택한 경계안·면적 개요

- 범위: 산괴 동쪽 대절벽·대분지·협곡 하류 출구·골짜기 초입·잔도 하부/북쪽 지하 문 주변.
- 로컬 X/Y(cm) 경계: `(-3200,-3400) → (1200,-4300) → (6500,-3600) → (9000,-1500) → (9000,1800) → (5500,3800) → (-2400,3800) → (-3200,1800)`.
- 원자료 폴더: **`Saved/Profiling/Graphics/Phase01/RidgeZonePalette_20261113/`**. 이하 파일명은 이 폴더 기준이다. `ZoneDraft.png`·`ZoneDraft.json`·`HeightfieldSamples.csv`, 재계산은 `AnalyzeZone.py`·`DrawZone.ps1`.

| 높이장 구간 | 로컬 평면 표본 면적(m²) | spec 표면 적분 추정(m²) |
|:---|---:|---:|
| 전체 | 8,630 | 19,933 |
| 0~30° | 4,983 | 5,578 |
| 30~70° | 1,329 | 2,526 |
| 70° 이상 | 2,318 | 11,830 |

구면 현지 Up과 법선으로 구한 **장식 검토용 분류**다. Support/보행 판정이 아니다. 1m·2m 표본의 전체 표면 추정은 19,933·19,930m²였지만 실제 메시 정확도 근거로 사용하지 않는다. 암반/지하 몸체 대체 footprint(로컬 약 1,238m²·가상 높이장 표면 약 3,204m²)를 포함한 대체 전 높이장 표이므로 실제 지각·암반 면적 합계가 아니다. 지하 벽/바닥·개구부·부유섬의 겹친 면은 적분하지 않았다.

대분지 Pod 25m 외 인접 정상 35m·골짜기 32m도 고려했다. 수치는 표면 점과 Pod의 3D 거리 추정이며 그림의 원은 로컬 평면상 참고다. 경로 반폭+2m 초안·지상 Pod·대체 footprint를 제외한 약 2,610m²는 훅·입구·카메라와 전체 메시 범위 검토가 빠져 있어 **배치 가능 면적/수량 예산이 아니다**. 제외 영역은 겹치므로 합산하지 않는다. 경계 내 표본은 이음매 보존/전이 띠와 겹치지 않았다.

### 8.3 원본 읽기·현재 재질 확인

- 8000은 LootNPop/TestMap03, 8001은 Electric Dreams/Startup으로 시작했고 로그의 MCP 포트도 확인했다. 8001에서 기존 CloseRange를 열고 카메라/캡처만 사용했다. 종료 시 CloseRange와 조사한 의존 패키지 **104개 모두 dirty=false**다(`SourceFinalState.json`). 원본 편집·저장·PCG 재생성 명령은 호출하지 않았다.
- `BlendUVRead.json`·`InspectBlend.py`: 원본 blend를 별도 background에서 저장 없이 읽었다. 지각에는 LightMapUV 1개, 암반 Floor/Shell에는 UV가 없다. UE 정식 3메시는 Registry상 UVChannels=2·재질 슬롯 1개·Nanite=false였다. 이 채널 수로 표면 UV 적합성을 확인한 것은 아니다.
- 정식 메시의 기본 슬롯은 WorldGridMaterial이고, LVI 의존성에는 M_Ridge_Ground/Rock이 있다. Assembly/Production은 컴포넌트 오버라이드로 이 두 재질을 쓰는 구조다. 라이브 M_Ridge_Ground는 Constant3Vector 노드 1개였다. 실제 컴포넌트 교체/마스크 적용은 후속 작업에서 확인한다.
- **dirty 조회 정정:** `is_dirty`에 오브젝트 경로(`패키지.오브젝트`)를 넘긴 초기 true는 경로 형식에 따른 오탐이었다. 동일 MI의 패키지/오브젝트/패키지 경로 조회는 false/true/false였다. 올바른 패키지 경로로 조회한 재질과 맵은 false다. `P1_SourceMaterials.json`의 초기 dirty 값은 미저장 편집 증거가 아니다. 원본을 저장하거나 변경을 되돌리지 않았다. 상세 근거는 로그와 `P1_ReuseReadEndState.json`에 있다.
- spec·blend의 Z1 해시와 주요 원본 파일의 디스크 읽기 기록을 보존했다. 계산·오버레이·캡처·의존성 자료는 Saved에만 썼다. 에셋 반입/제작·베이크·빌드·성능 측정은 하지 않았다.

### 8.4 바로 다음 행동·카메라 후보·남은 범위

> 아래는 1-B0 선정 직후의 기록이다. 이후 확정 경로·출발 기준선·1-A 마감은 §9와 상위 실행서 §13을 따른다.

1. **다음 단위는 1-A 경로 확정/출발 기준선 준비다.** 벤치마크의 실제 시드/slot을 확인하고 A=DeepBasin에서 대절벽, B=TablelandRim에서 절벽 측면/분지, C=ValleyRise에서 허브 전경을 보는 안을 사용자와 고정한다. CrownEastRim은 높은 하향 비교의 예비 후보다. 위치 이름은 구도 후보이며 좌표/회전/FOV를 확정한 것이 아니다. A→B→C→A 연결 구간도 확인한다. 이번 일반 PIE slot 5를 재사용하지 않는다.
2. **콘텐츠 반입 전에 1-A를 마친다.** 이후 git 제외 경로와 반입/배치 방식의 실행 범위를 정하고, 아래 소수 역할로 작은 반입 시험을 진행한다. 이번 단위에서 그 작업을 시작하지 않았다.
3. Phase 1에는 1-A 경로/출발 기준선, 작은 반입 시험·환경 장식/전체 확장, 파라곤 2~3종, 동일 조건 전후 측정이 남아 있다. 필요한 작업과 검증량에 맞춰 세션을 사용한다.

### 8.5 Z3 보호 원칙·Z4 접촉 정합 위험

| 보호 대상 | 채택한 원칙 | 후속 담당 |
|:---|:---|:---|
| RiftCanyon 하류·BossValley·NorthLedge·4m 착지부 | 기존 통로/착지 폭 보존. 가장자리 바깥 여유 2m는 초안. 낮은 패치도 발/착지 접촉 확인 | PCG 배치에서 어깨/끝 캡과 회전·스케일·메시 전체 bounds로 마스크 결정 |
| 대분지 및 인접 Pod의 3D 편성 공간 | 낮은 지피·표면 패치만. 새 바위/나무 엄폐 금지, 기존 BasinMonolith 실루엣/판정 보존 | 반입/배치 단계에서 표면·레이어별 편성 공간과 적 시야 확인 |
| TablelandRimHub | 조준·출발·이동·착지·실패 시 회복 공간 보호. 출발→앵커 직선만으로 완료하지 않음 | 배치 후 플레이어/카메라 수동 확인 |
| HallBasinGate·대전당 몸체 | 문 윤곽/진입 공간 보존. 같은 X/Y의 지하·지붕·외벽을 구분 | 재질/PCG의 실제 표면 소유·외부/내부 마스크 확인 |
| 검토점·카메라 | 시야/카메라 공간 보존. 낮은 디테일을 일괄 제외하지 않음 | 1-A 경로 고정 및 배치 뒤 같은 구도 재확인 |
| 이음매·범위 밖 구조 | 이음매 보존/전이 띠 금지. 첨탑·석교·섬은 첫 대표 구역에서 제외 | Ridge 전체 확장 시 별도 검토 |

- 116m 벽 전체를 외장으로 덮지 않는다. 70° 이상 면도 벽보행 접근면일 수 있다. 큰 외장은 **부분 보강 시험용**이며 카메라 관통·NPC 매몰·발/착지 부유·탄환 시각 불일치·구면 접합을 확인한다.
- 뒤로 묻으면 노출이 사라질 수 있으므로 노출과 접촉 정합을 함께 판단한다. 실패하면 외장 범위를 줄이거나 제외하고 지각 재질로 보완한다. 지각 숨김/충돌 추가로 해결하는 변경은 포함하지 않는다.
- **돌출 허용치·밀도·스케일·노출 비율은 지금 확정하지 않는다.** 작은 반입 시험과 후속 배치가 분석한다.
- 장식은 `LNP.Surface.Decoration` 태그 단독+`LNPDecoration` profile이다. Support/Blocker/Static 태그를 장식에 섞지 않는다. 기존 지각·Floor/Shell·Support source를 보존하고 SurfaceData 현재성·런타임 로드를 후속 단계에서 확인한다.

### 8.6 P1 채택한 재질 방침

**원본 `/Game/Landscape/M_BGLandscape_Auto`와 `MI_BGLandscape_Auto_01`의 표현/그래프를 재사용하는 파생본을 우선 시험하고, 보강 메시에는 아래 원본 MI를 유지한다.** 단순 재질 재구성안은 시험 결과가 부족할 때의 대안이다. 원본 자체를 수정하는 방침이 아니다.

- 원본 지형 재질은 BeachCliff·MossyRockyGround를 이미 쓰며 JungleGround·MossyGrass·노이즈도 참조한다. 절벽/지면 블렌드·색/거칠기 조절을 재사용한다. 전체 참조를 P4에 포함했으며 레이어 가중치 축소를 비용/의존성 제거로 해석하지 않는다.
- 보완할 부분은 **구면 현지 Up 경사 마스크·표면 좌표/노멀·대표 구역/표면 마스크**다. 원본 경사 마스크는 월드 normal B(Z), 지면은 UV0, 절벽 WorldAlignedTexture/Normal의 현재 소비 출력은 XY Texture다. 기존 함수 호출을 그대로 구면 트라이플래너 완성품으로 취급하지 않는다. 투영 방식/스케일/노멀 처리는 반입 시험 담당이다.
- 원본의 WPO 출력은 비어 있고 테셀레이션은 꺼져 있다. 지각 재질의 변위/WPO는 사용하지 않는다. Fern 등 식생의 원본 WPO는 별도 조건부 시험 항목이다. 일부 출력 enum은 도구에서 읽지 못했으므로 픽셀 깊이 등은 후속 표시 시험에서 확인한다.
- 기존 지각은 1슬롯이고 암반 외피는 합쳐져 있다. 지역/표면 마스크로 대표 구역부터 적용하고 지하 내부·범위 밖 표면을 자동 변경하지 않는다. 같은 Ridge가 여러 slot에 배정될 때 적용 범위/회전도 확인한다.
- StaticMesh 반입·근경 품질·텍스처 반복·색 일치·VT/샘플러·slot 회전·이음매는 미검증이다. 마스크 채널은 이름만 보고 연결하지 않고 원본 소비 배선에서 확인한다.
- 근거: `P1_OriginalParentInventory.json`·`P1_OriginalCoordinateRead.json`·`P1_OriginalWiring.json`·`P1_OriginalLandscapeMI.json`·`P1_SurfaceTextures.json`.

### 8.7 P2·P3 조건부 팔레트

아래 6종은 **사용자 조건부 채택**이다. 전부 Nanite=true, 슬롯 1개다. 크기는 원본 로컬 AABB(m)이며 회전/스케일 뒤 크기가 아니다. Nanite 수는 Registry 태그, LOD0은 이번 로드 뒤 `get_num_triangles(0)` 값이다. 일부 초기 Registry Triangles와 달라 `P2_LoadedLOD0.json`을 기준으로 기록했다.

| 역할·메시 경로 | 로컬 크기(m) | Nanite / 로드 LOD0 삼각형 | CloseRange 액터/컴포넌트 사용례 |
|:---|:---|---:|:---:|
| 절벽 부분 보강 `/Game/SmartAssets/AssetMeshes/SM_Cliff_01` | 13.59×9.33×14.19 | 3,996,469 / 9,424 | 1 / 2 |
| 낮은 사암 `/Game/Megascans/3D_Assets/SandstoneBoulder/SM_SandstoneBoulder_01` | 1.30×0.50×0.20 | 1,000,000 / 7,166 | 1 / 2 |
| 분지 가장자리 이끼 바위 `/Game/Megascans/3D_Assets/MossyForestBoulder/SM_MossyForestBoulder_06` | 1.25×1.23×0.69 | 1,000,000 / 7,414 | 3 / 3 |
| 얇은 하상 `/Game/Megascans/3D_Assets/DryRiverBed/SM_DryRiverBed_02` | 1.00×1.66×0.08 | 999,999 / 27,846 | 1 / 1 |
| 지피 `/Game/Megascans/3D_Plants/CloverVarieties/SM_CloverVarieties_01` | 1.36×1.28×0.15 | 3,668 / 1,716 | 1 / 1 |
| 제한적 고사리 `/Game/Megascans/3D_Plants/Fern/SM_Fern_01` | 1.56×1.88×1.03 | 1,714 / 306 | 1 / 1 |

사용례는 기존 CloseRange 원자료의 액터/컴포넌트 수로, 인스턴스 수·밀도·드로우콜이 아니다. 현행 메시 기본 슬롯이 아래 실효 재질과 같고, 원자료의 명시 오버라이드도 함께 확인했다.

| 메시 | 유지할 MI | 실효 부모의 Blend / 양면 / WPO 출력 | 원본 충돌 정책(1차 장식에서는 끔) |
|:---|:---|:---|:---|
| Cliff_01 | `/Game/SmartAssets/AssetMaterials/MI_Cliff_01` | M_BlendMossFuzz · Opaque / 단면 / 미연결 | SimpleAndComplex, 프리미티브 64 |
| SandstoneBoulder_01 | 같은 폴더 `MI_SandstoneBoulder_01` | DynamicLayering · Opaque / 단면 / 미연결 | ComplexAsSimple, 1 |
| MossyForestBoulder_06 | 같은 폴더 **`MI_MossyForestBoulder_09`** | DynamicLayering · Opaque / 단면 / 미연결 | SimpleAndComplex, 3 |
| DryRiverBed_02 | 같은 폴더 `MI_DryRiverBed_02` | DynamicLayering · Opaque / 단면 / 미연결 | ComplexAsSimple, 0. 별도 complexCollisionMesh 참조 |
| CloverVarieties_01 | 같은 폴더 `MI_CloverVarieties_01` | CustomFoliageNoWPO · Masked / 양면 / 미연결 | SimpleAndComplex, 0 |
| Fern_01 | 같은 폴더 `MI_Fern_01` | CustomFoliage · Masked / 양면 / 연결 | SimpleAndComplex, 0 |

- Cliff는 깊이 약 9m로 노출/접촉 위험이 가장 크다. 벽 전체 외장이 아니라 작은 보강 시험만 한다. 노출과 접촉을 함께 얻지 못하면 제외한다.
- 사암 조각·이끼 바위는 Pod/경로를 침범하지 않는 낮은 장식 후보다. 이끼 바위는 분지 가장자리로 제한하며 기존 거석 엄폐를 대신하지 않는다. 하상도 뜸/관통/반복과 발 접촉을 확인한다.
- Clover는 낮은 지피의 우선 후보다. Fern은 약 1m 높이로 Pod·통로·훅·카메라 밖에 제한하고 원본 바람의 구면 방향/변위를 시험한다. Masked·양면·WPO·그림자 비용은 패키지 검증 담당이다.
- 나무(Hornbeam)·ForestRockShelf·HugeSandstoneCliff 등은 **현재 팔레트/의존성 시작점에서 보류**했다. 나무의 충돌 없는 줄기와 시야 가림, 선반의 접촉/실루엣 위험을 전체 확장 때 다시 판단한다. 예비 후보의 의존성을 현재 총계에 섞지 않았다.
- 근거: `P2_CandidateUsage.json`·`P2_CloseRangeCandidateTags.json`·`P2_ActualMaterials.json`·`P2_LoadedLOD0.json`. 사용자 구도는 **`p3_userscene.png`**, 개별 썸네일은 `p3_cliff01.png`·`p3_sandstone01.png`·`p3_moss06.png`·`p3_riverbed02.png`·`p3_clover01.png`·`p3_fern01.png`다. MCP 원본 PNG 바이트를 저장했으며 이미지 편집은 하지 않았다. 원본의 에디터 경고/볼륨 표시는 남겨 두었고 해당 설정을 바꾸지 않았다.

### 8.8 P4 의존성·원본 규모

6메시+실효 MI 6개+지형 부모/MI 2개의 **14개 명시 시작점**에서 `/Game` 전이 의존성을 중복 없이 추적했다. `P4_DependencyClosure.json`에 시작점 그룹/전체 그래프, `P4_SourceFiles.json`에 파일/크기/주요 해시, `P4_FootprintSummary.json`에 집계가 있다. 질의/계산은 `P4_QueryClosure.py`·`MeasurePaletteFootprint.py`다.

- **104패키지·104개 uasset, 1,785,971,111 bytes = 1,703.23MiB ≈ 1.663GiB.** 누락 파일 0개. 원본 디스크 크기이며 cook 크기·VRAM·런타임 메모리가 아니다.
- 텍스처 **66개, VT 65/비VT 1**. 4K 31개·2K 24개·1K 4개·256² 5개·32² 2개다. 단일 텍스처의 색/마스크 채널 해석과 샘플러는 반입 시 확인한다.
- 클래스: StaticMesh 7, MI 7, Material 5, MaterialFunction 13, Texture2D 66, MPC 1, PhysicalMaterial 1, MaterialLayer/Blend/Instance 4.
- 선택은 6메시지만 `DryRiverBed_02`의 **complexCollisionMesh**인 `/Game/Meshes/_GENERATED/RockFormation/SM_DryRiverBed_02`가 추가 의존성이다. 장식 후보로 채택한 헬퍼가 아니다. 현재 전이 범위에는 보존했고 NoCollision 반입 시 불필요 참조를 제거할지는 별도 판단한다.
- 지형 부모/MI 자체의 전이 범위는 15패키지·426.87MiB, 가장 큰 파일은 SM_Cliff_01 약 365.58MiB다. 부분 집계는 서로 공유하므로 합산하지 않는다. 초기 약 2GB 가이드 안이며 원본 크기로 렌더 성능을 판정하지 않는다.
- 외부 참조는 엔진 함수/텍스처와 Interchange·NavigationSystem·PhysicsCore 계열이다. 조사한 전이 범위에는 샘플 전용 게임플레이 모듈 참조가 없었다. 실제 Migrate·리다이렉터·누락 의존·cook 성공을 검증한 것은 아니다. 새 타깃 파생본의 생성 후 의존성은 아직 존재하지 않는다.

### 8.9 미확인 사항·후속 담당

| 후속 단계 | 우선 확인할 항목 |
|:---|:---|
| 1-A 경로/출발 기준선 | 실제 시드/slot, 정식 A/B/C와 연결 구간, 적/환경 노출, 장식 전후 같은 경로 |
| 반입 준비/방식 결정 | 정확한 git 제외 폴더, P4 전체 참조와 실제 원본 MI, 타깃 파생본/원본 소유권·재생성 절차 |
| 작은 반입 시험 | 지형 파생본의 구면 마스크·좌표/노멀·VT·근경/반복/색 일치. Cliff 노출+접촉 정합, 하상/바위 접지, Clover/Fern 원본 재질과 바람/회전. 실패 역할 축소/대체 후 팔레트 최종 확정 |
| PCG 배치 | 역할별 표면/현지 Up, 보호 영역과 메시 전체 범위, 고정 시드·밀도·중첩·스케일·투영 방식. 지하/지붕/섬의 다른 레이어 보존 |
| 데이터/표시·성능 | Decoration 계약, 기존 Support source·SurfaceData 현재성/런타임 로드, 카메라·벽보행·발/착지·탄환·적 식별성. Lumen/식생/VT/메모리 비용은 패키지에서 확인 |

## 9. 다음 세션

1-B0 선정과 **1-A 출발 기준선은 완료됐다(2026-11-14 사용자 마감).** Ridge slot 1·확정 A/B/C·B 현지 Up +50cm 경로와 정확 1440p 패키지 검증, Medium 3회 재현성·적 0 센서 보완을 통과했다. [출발 기준선 보고서](../analysis/Phase01_StartBaselineReport.md)와 [상위 실행서](Phase01_BenchmarkContent.md) §13을 현재 인계로 사용한다.

2026-11-14에 반입/좌표 진단과 1-B3c 반복 완화 구현/기술 검증을 마감했다. 재질은 잔여 반복과 높은 샘플 비용으로 조건부 후보다. 2026-11-15에는 1-B4a 사암/하상/클로버의 소수 근경 시험을 마감했고 역할별 현재 판정은 §10을 따른다. 다음은 진행 확인 뒤 이끼 바위/절벽 부분 보강 소수 시험 제안이다. 결과와 다음 범위는 [환경 반입 실행서](Phase01_EnvironmentImport.md) §13이 소유한다. §8은 최초 선정 기록이며 1-B0을 재선정하지 않는다. 정식 Ridge 적용·PCG·전체 밀도/패키지 검증은 아직 하지 않았다.

## 10. 소수 시험에 따른 현재 팔레트(2026-11-15)

| 메시 | 현재 판정 |
|:---|:---|
| SandstoneBoulder_01 | **근경 통과·조건부 유지.** 사용자에게 매우 자연스럽다는 확인. WIP2개 유지 |
| CloverVarieties_01 | **근경 통과·조건부 유지.** 부유/이질감 없음 확인. WIP6곳 유지 |
| DryRiverBed_02 | **제외.** 표면 자체는 자연스럽지만 네모 외곽이 주변과 어울리지 않는다는 사용자 판정. 시험2액터 제거·원본 보존 |
| Cliff_01 | 미시험·조건부 |
| MossyForestBoulder_06(MI09) | 미시험·조건부 |
| Fern_01 | 미시험·조건부. 원본 WPO의 바람/구면 방향 확인 필요 |

위 판정은 WIP 검사용 조명의 수동 근경 시험 범위다. 전체 밀도·원거리 반복·PIE 발/카메라/탄환·SurfaceData/런타임 로드·패키지 성능까지 통과한 최종 팔레트가 아니다. 반입 원본/의존성104개와 §8의 초기14시작점 식별 기록은 실패 역할도 포함해 보존한다. 재생성 코드에서는 실패 하상2개를 뺐으며 다른 하상 후보로 자동 대체하지 않았다. 상세 근거는 [환경 반입 실행서](Phase01_EnvironmentImport.md) §13을 따른다.
