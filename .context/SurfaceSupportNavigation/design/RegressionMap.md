# Surface Support 공통 회귀 공간

> 상태: 기준 설계
> 정적 사례: fixture LVI `/Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Regression`(생성 명령 `LNP.SurfaceNav.BuildRegressionFixture`)
> 동적 사례: 일반 레벨 `/Game/Maps/SurfaceNavigation/L_SurfaceRegression`(생성 스크립트 `Scripts/GenerateSurfaceRegressionMap.py`)

## 1. 구성과 좌표 규약

회귀 공간은 두 개로 나뉜다(D-056).

| 공간 | 담는 사례 | 이유 | 자동화 |
|:---|:---|:---|:---|
| 정적 fixture LVI | 지각·부유섬·섬 가장자리·동굴 키트·정적 프랍·이음매·꼭짓점 | 옥탄트 베이크와 8 slot 회전 경로를 그대로 탄다 | `WorldCollision.RegressionFixture`, `Bake.*`(결정론·이음매·Atlas 오차) |
| `L_SurfaceRegression` | 상태형 기둥·움직이는 패널·파괴 바닥 | LVI에는 동적 source를 둘 수 없다(D-026). Phase 8에서 마커 요소로 바꾼다 | `WorldCollision.RegressionMap` |

공통 규약:

- 월드 중심은 `(0, 0, 0)`, 기준 지각 반지름은 30,000cm다(D-046).
- 좌표는 source 옥탄트(+X,+Y,+Z) 로컬이다. 사례 위치는 (위도, 방위)로 적는다. 위도는 적도 `z=0`에서 +Z 쪽, 방위는 +X에서 +Y 쪽 각도다.
- 모든 사례는 위도 25~40° 안에 둔다. 이음매·꼭짓점과 int16 복제 캡에서 떨어뜨리기 위해서다. 이음매·꼭짓점 사례만 예외다.
- 사례 틀: Radial은 중심에서 바깥쪽 단위 벡터, Tangent는 방위 증가 방향, Bitangent는 위도 증가 방향이다. 캐릭터 Up은 `-Radial`이다.
- 구분용 geometry 대신 `Probe_<Case>` 이름의 `LNPDecoration` 구체를 debug 위치로 둔다.
- `NavComponent` 이름은 Phase 7 전까지 테스트 oracle용 상징 이름이다. 직렬화 ID를 미리 고정하지 않는다.
- 좌표 원본은 코드다. 기존 정적 사례는 `LootNPopEditor/SurfaceNavigation/LNPRegressionFixture.h`, Phase 7c 입체 사례는 `LNPVolumetricFixture.h`, 동적 사례는 생성 스크립트다. 이 문서의 표는 요약이다.

## 2. 공통 기대 결과

- `LNPSurfaceSupport`는 `Support` source만 맞힌다.
- `LNPWorldExact`는 Support 여부와 무관하게 `Blocker`를 맞힌다.
- `Decoration`은 두 channel에서 모두 hit하지 않는다. Pawn profile도 exact query에서 제외된다.
- 정적 layer 수에는 `Dynamic`, `StatefulTraversal`, `Destructible` source를 포함하지 않는다.
- 원거리 타게팅 가능은 fixture 사이의 실제 LoS가 아니라 해당 시나리오에서 blocker가 없다는 전제의 제품 기대값이다.
- Unknown hit는 0이다.

## 3. 정적 사례와 oracle

fixture LVI 한 개를 8 slot 회전으로 테스트 월드에 복제하고 **모든 slot에서** 같은 기대값을 검사한다. 지각은 노이즈 없는 완전 구면(격자 N=256, 평면 삼각형 반지름 오차 0.4cm 이하)이라 기대 반지름은 30,000cm다. 섬은 윗면(`Support+Blocker`)과 몸체(측벽·밑면, `Blocker`)가 별도 component다(`TerrainContract.md` §5). 섬 두께는 300cm다.

| 사례 | (위도, 방위) | 형상 | 정적 Support Layer | exact 기대 | NavComponent | 지상 추격 | 원거리 타게팅 |
|:---|:---|:---|:---:|:---|:---|:---|:---|
| 기본 지각 | (30, 8) | 지각 | 1 | 바깥 ray가 R에서 지각 hit, 법선은 중심 방향, Static Support | `NC_Crust` | 같은 지각에서 가능 | LoS 시 가능 |
| 부유섬 하나 | (32, 18) | 섬 1300×1300, 윗면 28,000 | 2 | 윗면 hit(Support), 섬 밑에서 출발하면 지각 hit | `NC_Island1`, `NC_Crust` | layer 간 불가 | LoS 시 가능 |
| 부유섬 둘 | (32, 30) | 안쪽 섬 1100, 윗면 26,400 / 바깥 섬 1400, 윗면 28,200 | 3 | 안쪽 섬 → 바깥 섬 → 지각 순서로 구분 | `NC_Island2A`, `NC_Island2B`, `NC_Crust` | layer 간 불가 | LoS 시 가능 |
| 섬 가장자리 안쪽 | (32, 42) | 섬 1600(위도)×1200(방위), 윗면 28,000 | 1 | 접선 550에서 support probe 성공 | `NC_EdgeIsland` | 섬 내부 가능 | LoS 시 가능 |
| 섬 가장자리 바깥쪽 | (32, 42) | 같은 섬 | 0 | 접선 750에서 support miss, 측벽 sweep은 접선 600에서 Blocker만 hit | 없음 | 불가 | LoS 시 가능 |
| 동굴 키트 | 공동 (28, 56) | §4 | 공동 바닥 1, 통로 바닥 1 | 바닥 Support, 천장·벽 Blocker만, 문 너머 통로 바닥, 입구에서 지각 없음 | `NC_Cave` | 입구 통로로 외부와 가능 | 천장·벽 LoS 반영 |
| 정적 프랍 | (32, 72) | 나무(원기둥 지름 180, 접선 -350), 바위(비균일 구, 접선 350), 장식(접선 700) | 1 | 나무·바위 Blocker만 hit, 장식·Pawn은 통과해 지각 hit | `NC_PropGround` | blocker 우회 시 가능 | blocker LoS 반영 |
| 이음매 변 중점 | 세 변의 중점 | 지각 | 1 | 경계 ±5cm에서 반지름 R, 경계를 가로지르는 sweep에 턱 없음 | `NC_Seam` 하나 | 경계 통과 가능 | LoS 시 가능 |
| 꼭짓점 | 세 좌표축 | 지각 | 1 | 축에서 수 cm 떨어진 ray가 반지름 R에서 hit | `NC_Seam` 하나 | 경계 통과 가능 | LoS 시 가능 |

지각 Atlas도 이 LVI로 굽는다(`DA_OctantSurface_Fixture_Regression`). 자동화 `Bake.OctantBakeDeterministic`·`CrustSeamMatch`·`CrustAtlasExactError`가 저장본 일치, 이음매 일치, Atlas–exact 오차를 검사한다. 2026-09-27 기준 유령 지면 0, 100cm 해상도 반지름 오차 P99 0.16cm, 이음매 반지름 차 0이다. 동굴 입구는 Atlas에서 coverage hole(Invalid 61샘플)이다.

다층 Atlas(Phase 4b)의 Layer 식별은 자동화 `WorldCollision.LayerIdentity`가 이 LVI의 8 slot 합성으로 검사한다. exact `LNPSurfaceSupport` trace hit의 (source key, `FaceIndex`)를 저장된 SurfaceData의 face 표로 해석한 Layer가 같은 발 위치의 `QueryLayers` Layer와 같아야 한다. Atlas가 `Supported`면 같은 Layer, `NoSupport`면 exact도 창 안에 지지면이 없어야 하고, `NeedsExact`는 exact가 판정한다.

| 사례 | 발 위치 | 기대 |
|:---|:---|:---|
| 부유섬 둘 | 안쪽 섬 윗면, 안쪽 섬 밑(바깥 섬 윗면), 바깥 섬 밑(지각) | 셋 다 `Supported`, 반지름 오차 1cm 이내, 서로 다른 Layer이고 마지막만 Layer 0 |
| 동굴 키트 | 공동 바닥 중심, 통로 경사 바닥 중간 | `Supported`, 지각보다 깊고 서로 다른 비지각 Layer. 지각·천장 뒤에서도 조회된다 |
| 섬 가장자리 | 윗면 높이, 접선 300~900을 5cm 간격, 창은 지각까지 | 모든 지점에서 Atlas와 exact 일치(유령 지면 없음). 접선 750은 지각 `Supported` |

`Meadow_00` 섬 B 계단(`SM_TerrainBox` 칸 Layer)도 같은 테스트가 slot 0에서 본다. 칸 위에서는 그 칸 Layer로 `Supported`이거나 `NeedsExact`이고, 칸 옆에서는 섬 윗면이 `Supported`여야 한다.

## 4. 동굴 키트 사례

키트 규약은 `TerrainContract.md` §6, 치수 원본은 `LootNPopEditor/SurfaceNavigation/LNPCaveKit.h`다.

- 공동: 직육면체 1600×1600, 높이 500. 바닥은 곡률 반지름 30,000의 구면 캡이다. +X 벽에 폭 400·높이 350 문이 있다.
- 통로: 문에서 위도 증가 쪽으로 수평 2,400cm, 경사 20°로 오른다. 단면 400×350이다. 입구 쪽 끝 바닥점이 지각 반지름에 오도록 공동 깊이를 정한다(`LNPCaveKit::PlaceUnderSphere`). 공동 바닥 반지름은 약 30,713cm, 천장은 약 30,213cm라 지각 아래 약 213cm 두께의 지붕이 남는다.
- 지각 입구: 지각에서 통로 내부 볼록 영역(두 벽·바닥·천장·양 끝 평면)과 겹치는 부분만 평면으로 잘라낸다. 통로 벽 평면이 곧 구멍 경계라 틈이 없다. 통로 천장·벽은 지각 위로 솟은 입구 구간에도 그대로 있어 지붕 덮인 입구가 된다. shell은 양면이라 바깥에서도 막는다.

| 검사 | 기대 |
|:---|:---|
| 공동 중심(바닥 위 250)에서 아래 | 공동 바닥(로컬 z=0), Support |
| 위 | 천장(로컬 z=500), Blocker만 |
| ±Y, -X | 벽(중심에서 800), Blocker만 |
| +X(문) 수평 | 통로 경사 바닥, Support |
| 통로 안(입구에서 300cm 안쪽, 지각보다 중심 쪽)에서 바닥으로 | 지각이 아니라 지각보다 50cm 이상 깊은 통로 바닥 |
| 입구 옆(벽 바깥 300cm)에서 바깥쪽 | 지각이 남아 있어 반지름 R |
| 지각 위로 솟은 통로 벽을 바깥에서 | 벽(통로 로컬 y=200), Blocker만 |

## 4-1. 입체·지하 공동 사례(Phase 7c)

기존 정적 LVI에 `LNP.SurfaceNav.ExtendRegressionFixture`로 추가한다. 기존 actor·source key와 수동 Spawn anchor 세 개는 유지하고 공동용 특정 세트 anchor 두 개를 추가한다. 입체 사례 중심은 **(위도 35.2643896828°, 방위 45°), 바닥 반지름 33,500cm**다. 아래 좌표는 이 중심의 바닥 피벗 기준이며 +X는 위도 증가, +Y는 방위 증가, +Z는 Up이다. `FloorZ(x,y)=33500-sqrt(33500²-x²-y²)`로 바닥 곡률을 맞춘다.

| 사례 | 고정 배치(cm) | 기대 결과 |
|:---|:---|:---|
| 높은 공동 | X·Y ±2200, 천장 Z=3200 | Floor는 `Support+Blocker+Static+CoarseSupport`(100cm), Shell은 `Blocker+Static`. 충분한 headroom 후보에는 비행 Pod 할당 가능 |
| 두 입구·긴 경사로 | +X 벽의 Y=±1200, 폭 600·높이 400, X=2200~10200 | 두 입구 모두 지각까지 Walk 연결. 수평 8000·상승 약 5218.91cm. 입구 가까운 시작·목표는 해당 경사로 선택. 두 단면으로 지각 절단 |
| 낮은 분기 | X ±600, Y=-3600~-2200, 끝 천장 높이 350 | 주 공동과 Walk 연결. 지상 편성 가능, 기본 비행 편성 필요 headroom 2638cm 미달 |
| 복층·두 계단 | 상층 X=1100~1900·Y ±1800, 바닥 위 1200. 계단 X=-1900~1100·Y=±1500·폭 400 | 상층까지 두 연속 경사면으로 Walk 연결. 칸 30개씩은 Decoration이고 Support·exact·Pawn 충돌 없음 |
| 나선 경사로 | 중심 (-700,0), 반지름 450~850, 1.3회전·104 segment·반경 100cm 네 띠, 상승 1200 | 하나의 Structure source 안 접힌 sheet가 여러 Layer로 분할. 끝에서 반경 방향으로 500cm 나와 Y≈903~1284의 연결판으로 발코니에 합류한다. 마지막 회전 위를 가로지르지 않으며 연결판은 길이 방향 24분할로 곡률을 맞춘다. 복층까지 portal로 Walk 연결, external face identity 유지 |
| 수직 통로 전용 공동 | X ±700·Y=-5400±700, 같은 구면 바닥. 천장 Z=3200 | 다른 공동과 분리. 지각과도 Walk 연결 없음. slot당 독립 ReachabilityGroup 하나 |
| 수직 입구 | X ±300·Y=-5400±300, Z=3200~4500 | 지각·천장 구멍에 유령 Support 없음. 플레이어 낙하 동선. 두 그래플 앵커 Placement Marker는 (0,-5400,3000)/(0,-5400,5100), 서버가 `BP_GrappleAnchor` 스폰. 두 번째 앵커는 외벽 위 600cm로, 150cm 전방 정지 뒤에도 캡슐이 외벽 밖으로 나갈 수 있다 |

위층·계단·나선·연결판의 Support 단면 30cm 아래에는 반대 방향 밑면을 Shell Blocker로 둬 아래쪽 관통을 막는다. 공동 Spawn anchor는 지상 `Bench01`=(1700,-600,FloorZ), 비행 포함 `Bench01_2`=(-1400,-900,FloorZ)이며 기존 세 개와 합쳐 authored 다섯 개다. 두 특정 세트는 실제 주변 지상/비행 적 후보를 제공한다.

구면 공동 바닥과 평면 입구 경사로가 만나는 높이 차는 `EntranceShell`의 양면 Blocker로 닫는다. 바닥의 100cm 경계 격자를 따라 두 입구에 여섯 조각씩 둔다. 보행 Support·Nav 연결은 기존 portal을 유지한다. `LNP.SurfaceNav.FixVolumetricEntranceShell`은 이 메시만 다시 만들며 이후 회귀 SurfaceData를 재베이크해야 한다. `Bake.VolumetricFixture`는 실제 2P에서 기록한 지형 이탈 선분 두 개의 exact hit도 8-slot에서 검사한다.

`Bake.VolumetricFixture`는 저장본으로 8-slot snapshot과 exact physics world를 구성한다. coarse/fine 해상도, exact source·external face→Layer, 다층 조회·반지름 오차 2cm 미만, 양 입구·복층·나선·분기 A*, 수직 통로 도달 불가, 지각 hole, 위층 밑면 Pawn 충돌·나선 끝→발코니 캡슐 sweep·특정 세트의 실제 주변 적 배치·headroom과 순수 Spawn planner를 검사한다. 비행 할당 검사의 높은 빈 영역은 X=-2050~-1550·Y ±700, 낮은 영역은 X ±300·Y=-3400~-2800이다. 천장 높이와 Pod capsule·edge clearance를 모두 만족한 실제 후보만 소비한다.

기존 `Nav.RegressionReachability`의 oracle은 지각·경사로 공동의 전역 group 하나, slot당 독립 섬 네 개와 수직 통로 공동 한 개로 **41 group**이다. 기존 exact fixture는 slot당 StaticMesh actor 96개(기존 29+추가 67), exact source 22개(기존 15+추가 7)를 검사한다. 두 Placement Marker는 StaticMesh actor 수에 포함하지 않는다. Layer ID는 source key·external face 순으로 계산하며 신규 층의 숫자 ID를 문서에 고정하지 않는다.

사용자 PIE 확인은 양쪽 경사로 추격, 계단·나선의 Layer 경계 이동, 수직 통로 낙하·그래플 복귀와 높은 공동의 비행 교전이다. 자동화 통과와 별도로 결과를 Phase 로그에 남긴다.

## 5. 동적 사례와 oracle

`L_SurfaceRegression`의 지면 판은 엔진 Cube(두께 100, 중심 R+50)라 안쪽 면이 R=30,000에 온다. 이 맵은 베이크 입력이 아니므로 box 단순 충돌을 그대로 쓴다.

| 사례 | (위도, 방위) | 고정 배치 | exact 기대 | NavComponent | 지상 추격 | 원거리 타게팅 |
|:---|:---|:---|:---|:---|:---|:---|
| 상태형 기둥 | (30, 20) | 지면 좌·우(접선 ±800), 기둥 길이 1000 중심 R-500 | 기둥 자체는 항상 hit(Dynamic, Blocker만) | 좌·우 분리 후 안정 시 병합 가능 | 정지 전 불가, 안정 후 가능 | LoS 시 가능 |
| 움직이는 패널 | (30, 45) | 지면 좌·우(접선 ±800), 패널 800×800 안쪽 면 R-800 | 패널 현재 transform에서 hit(Dynamic Support), 패널 아래 gap은 miss | 좌·우 정적 영역 분리 | 계획적 이용 불가 | LoS 시 가능 |
| 파괴 바닥 | (30, 70) | 지면 좌·우(접선 ±950), 가운데 파괴 바닥 | 파괴 전 hit(Destructible), 파괴 후 miss | 파괴 전 overlay 연결, 파괴 후 분리 | 파괴 전 가능, 후 불가 | LoS 시 가능 |

### 동적 상태 oracle

| 사례 | 초기 상태 | 변경 | 기대 link | 기대 revision |
|:---|:---|:---|:---|:---|
| 상태형 기둥 | 이동 또는 직립 | 사전 정의된 안정 transform에서 완전 정지 | 안정 전 없음, 안정 후 좌·우 Walk Link 1개 | 영향 tile 지역 revision `+1` |
| 움직이는 패널 | 임의 transform | 매 프레임 transform 이동 | 계획용 Walk Link 없음 | 정적 Nav revision 변화 없음, Dynamic Support transform만 갱신 |
| 파괴 바닥 | 유효 | 파괴 이벤트 확정 | 기존 overlay link·support 제거 | 영향 tile 지역 revision `+1` |

동일 상태 이벤트의 중복 수신은 revision을 다시 증가시키지 않아야 한다. 상태형 기둥이 다시 움직이면 link를 닫고 지역 revision을 한 번 증가시킨다.

## 6. 제작과 재생성

- **동굴 키트**: `LNP.SurfaceNav.BuildCaveKit`이 `/Game/Maps/CaveKit`의 키트 메시를 제자리 갱신한다.
- **정적 fixture LVI**: `LNP.SurfaceNav.BuildRegressionFixture`. 키트 메시가 먼저 있어야 하고, 기존 LVI가 있으면 거부한다. 다시 만들려면 LVI를 지운 뒤 실행하고 `LNP.SurfaceNav.BakeOctant`로 SurfaceData를 다시 굽는다.
- **입체 확장**: 기존 LVI·`SM_FixtureRegressionCrust_R30000`·SurfaceData를 먼저 백업한 뒤 `LNP.SurfaceNav.ExtendRegressionFixture`, `LNP.SurfaceNav.BakeOctant /Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Regression` 순으로 실행한다. 확장 actor가 이미 있으면 중복 추가를 거부한다. 재생성은 백업 LVI·지각 메시를 복구하고 같은 명령을 실행한다. 수동 anchor는 보존된다.
- **동적 맵**: 스크립트는 기존 맵을 기본적으로 덮어쓰지 않고 검증만 한다. 의도적으로 재생성할 때만 환경 변수 `LNP_REBUILD_SURFACE_REGRESSION_MAP=1`을 지정한다. 헤드리스 실행은 `UnrealEditor-Cmd -ExecutePythonScript=<스크립트>`다.
- actor label은 정적 LVI가 `RF_<순번>_`, 동적 맵이 `SSN_<순번>_`이다. Component Tag와 collision profile이 테스트 입력의 원본이며 label은 의미 판정에 쓰지 않는다.
- 좌표를 옮기면 이 문서의 표, 좌표 원본(헤더·스크립트), 자동화 기대값을 같은 커밋에서 갱신한다.

## 7. 예정된 fixture 추가

| 시점 | 변경 |
|:---|:---|
| Phase 4b(완료) | 한 component의 분리된 sheet, 내부형 double-sided normal의 Layer 분리 검증. 입력은 4a `LVI_Octant_Fixture_Crust`의 `FX_SplitSheet`·`FX_DoubleSidedPlate`다(`Bake.OctantSupportLayers`) |
| 콘텐츠가 필요할 때 | ISM/HISM instance와 Support proxy↔exact counterpart fixture(D-037·D-039). 그 전까지 둘 다 베이크 오류다(D-058) |
| Phase 8 착수 전 | 동적 사례 3종을 Placement Marker와 서버 스폰 요소로 전환 |
| Phase 8 착수 전 | 같은 tile에 겹치는 Conditional Patch 두 개와 중복 상태 이벤트 fixture |

non-uniform·negative scale과 Nanite complex collision 원본 검증은 4a `LVI_Octant_Fixture_Crust`(`FX_NegativeScaleSlab`)와 자동화 `Bake.NaniteComplexCollisionSource`가 맡는다.

Phase 3 네트워크 회귀에는 이동 중 패널 late join, server-time 보정, 상태 revision 불일치 진단을 추가한다. Phase 7에는 다중 프레임 path request 실행 중 tile revision과 connectivity version이 바뀌는 사례를 추가한다.

## 8. 의도적으로 지원하지 않는 형상

- 서로 물리적으로 관통하는 Support sheet(방사 방향에서 겹치는 복층·접힌 sheet는 지원)
- 움직이는 패널을 기다리고 탑승하는 계획 경로
- 임의 파괴 잔해가 만드는 새 보행면
- 측벽·밑면의 자동 보행면 승격
- 점프·훅·발사대·텔레포트를 사용하는 지상 NPC traversal
