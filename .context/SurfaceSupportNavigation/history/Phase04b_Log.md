# Phase 4b 로그 — 부유섬·동굴 키트 다층 베이크

## 2026-09-27

### Phase 4 공통 전제 — 결정

- 사용자 결정 두 가지:
  - 회귀 공간을 나눈다. 정적 사례는 새 fixture LVI를 8 slot 회전으로 합성해 검사하고, 동적 사례 3종만 `L_SurfaceRegression`에 30,000cm로 남긴다(D-056, D-023 대체).
  - 동굴 키트 메시는 지금 C++ greybox 빌더로 만든다. 프로덕션 품질은 아트 패스에서 Blender로 만든다. 교체 비용을 낮추려고 에셋 규약(이름·피벗·소켓·단면/양면·clearance)과 규약 검사는 지금 고정한다(`design/TerrainContract.md` §6 "키트 에셋 규약"). 규약이 4b 베이크와 Phase 7 Nav를 거치며 바뀔 수 있어서, 제작 파이프라인을 먼저 다듬으면 두 번 만들게 된다는 판단이다.

### 공유 메시 도구와 동굴 키트

- 4a fixture 빌더의 메시 헬퍼를 `LootNPopEditor/SurfaceNavigation/LNPFixtureMesh.*`로 옮겼다. 삼각형 목록, 구면 옥탄트 격자, 면 선택 상자, complex-as-simple 저장(소켓 포함), level world 생성·저장이 들어 있다. 4a 빌더는 이 도구를 쓰도록 바꿨고 삼각형 생성 순서는 그대로다(4a 에셋은 다시 만들지 않았다).
- `FLNPFixtureMesh::RemoveInsideConvex`: 볼록 영역 안쪽을 평면으로 잘라낸다. 경계를 가로지르는 삼각형만 쪼개고, 새 정점은 모서리 양 끝을 정렬해 계산·공유해서 이웃 삼각형과 같은 점이 된다.
- `LNPCaveKit.*`: 직육면체 공동(`RoomBox`)과 경사 통로(`RampCorridor`), 각각 Floor/Shell 두 메시. `LNP.SurfaceNav.BuildCaveKit`이 `/Game/Maps/CaveKit`에 쓴다.
  - 공동 1600×1600×500, 바닥은 곡률 30,000 구면 캡, +X 벽에 400×350 문.
  - 통로 수평 2,400cm·20°, 단면 400×350. 소켓 `Door_0`·`Lower`·`Mouth`.
  - Shell은 양면, 벽은 바닥 아래 30cm까지 내려 접합부 틈을 막는다.
  - `PlaceUnderSphere`: 통로 `Mouth` 바닥점이 지정 반지름 구면에 오도록 공동 깊이를 닫힌 식으로 정한다.
  - 지각 입구 설계: 통로 내부 볼록 영역(두 벽·바닥·천장·양 끝 평면)과 겹치는 지각만 자른다. 벽 평면이 곧 구멍 경계라 틈이 없다. 통로 천장·벽은 지각 위로 솟은 입구 구간에도 그대로 둬서 지붕 덮인 입구가 된다. 지각 곡률·요철과 무관하게 입구가 닫힌다. Chaos 단순 query는 trimesh를 단면으로 보므로, shell을 단면으로 두면 지각 위로 솟은 벽이 바깥에서 뚫린다. 그래서 shell은 양면이어야 한다.

### 정적 회귀 fixture LVI

- `LNP.SurfaceNav.BuildRegressionFixture` → `LVI_Octant_Fixture_Regression`. 배치는 `LNPRegressionFixture.h`가 빌더와 자동화에 공유한다.
- 완전 구면 지각 N=256(삼각형 65,556개), 통로 입구 절단 34개 삼각형. 공동 바닥 반지름 30,713.0cm.
- 섬은 윗면(`SM_FixtureBoxTop`, Support+Blocker)과 몸체(`SM_FixtureBoxBody`, Blocker)로 나눴다. 옛 맵은 엔진 Cube 하나를 Support로 썼다.
- 액터 29개: exact source 15개, Decoration 14개.
- `BakeOctant` 결과: Support source 7개 중 지각 식별 성공. Invalid 61샘플이 입구 coverage hole이다. `DA_OctantSurface_Fixture_Regression`을 저장했다.

### 동적 사례 맵

- `Scripts/GenerateSurfaceRegressionMap.py`를 동적 3사례·(위도, 방위) 좌표·30,000cm로 바꿨다. 기둥 (30,20), 패널 (30,45), 파괴 바닥 (30,70).
- 헤드리스 재생성은 `LNP_REBUILD_SURFACE_REGRESSION_MAP=1 UnrealEditor-Cmd -ExecutePythonScript=<스크립트>`로 했다. 결과는 fixture 12개로 스크립트 검증을 통과했다.

### 자동화

- 신규 `WorldCollision.RegressionFixture`: 8 slot 전부에서 지각·섬 하나·섬 둘·가장자리·동굴 키트(바닥·천장·벽·문·입구·입구 옆 지각·외벽)·프랍·이음매 변 중점·꼭짓점을 검사한다.
  - 첫 실행에서 장식·Pawn 검사가 8.5cm 어긋났다. 접선 700cm 지점의 구면 지각은 방사 투영이 `R - 8.2cm`라서다. 평면 cube 시절 oracle을 그대로 쓴 테스트 오류였고, hit 점의 실제 반지름으로 비교하게 고쳤다.
- 신규 `Bake.CaveKitContract`: 저장된 키트 에셋의 cooked 충돌 삼각형으로 규약을 검사한다. 바닥 walkable·위 방향, shell 양면, 소켓이 바닥 위에 있음, clearance ≥ 250cm.
- `WorldCollision.RegressionMap`은 동적 3사례·12 fixture로 줄였다.
- `Bake.OctantBakeDeterministic`·`CrustAtlasExactError`·`CrustSeamMatch` 대상에 새 LVI를 추가했다. 결과는 유령 지면 0, 100cm 반지름 오차 P99 0.16cm, 이음매 반지름 차 0, 세 변 seam hash 동일.

### `Meadow_00` 동굴

- `LNP.SurfaceNav.PlaceCaveKit <Lat> <Az> <Heading> [Apply]`(`LNPCaveKitPlacer.cpp`), 열린 editor world 대상이다.
  - 지각은 D-055로 식별한다. 입구 방향의 실제 지각 반지름을 광선으로 재서 `PlaceUnderSphere`를 반복해 맞춘다.
  - 공동 천장 위 지각 두께(최소 100cm), 통로 안 지각 범위, 입구 주변 PCG 프랍 수를 보고한다.
  - Apply는 지각 메시를 `FMeshPlaneCut::SplitEdgesOnly`로 쪼개고 통로 내부 삼각형을 지운 뒤 키트 액터 4개(`CaveKit_00_*`)를 배치한다. 분할은 `EdgeFilterFunc`로 통로 영역 400cm 안 모서리에 한정했다. 이음매 정점은 바뀌지 않고 UV·법선은 보간된다. 퇴화 모서리 collapse는 껐다.
- 후보 10곳 미리보기. `Meadow_00` 지각 기복이 커서 대부분이 지붕 두께에서 탈락했다(최소 -1,100cm~82cm).
  - 채택: (15°, 60°, 방향 위도 증가). 지붕 666cm, 입구 절단은 통로 x ≥ 1,499cm(입구 2,400), 입구 주변 프랍 4개, 가장 가까운 꼭짓점과 약 33°.
  - 탈락 사례: (18°, 72°)는 37cm, (18°, 30°)는 82cm, (16°, 80°)는 193cm로 통과했지만 이음매에 더 가깝다.
- 적용 결과: 지각 삼각형 91개 제거(237,606 → 237,841, 분할로 늘어남). 재베이크 결과 Support source 11개, Invalid 42샘플이 입구다. 조회 NeedsExact 2.35%, 반지름 오차 P99 1.06cm로 이전과 같고, 이음매 반지름 차는 0이다.
- PCG는 `GENERATE_ON_LOAD`이고 지각 컴포넌트에만 광선을 쏜다. 그래서 입구 구멍에는 프랍이 생기지 않는다. 지붕 덮인 입구 옆 프랍은 막지 못한다. PCG 결과(BP 외부 액터)는 저장하지 않았다.
- 신규 `WorldCollision.MeadowCaveKit`: `Meadow_00`의 지각과 키트 액터만 복제해 공동 바닥, 입구 끝 지각 높이, 입구 통과, 입구 옆 지각 잔존을 검사한다. 입구 바닥점에서 중심 쪽 ray는 지각보다 입구 천장에 먼저 맞으므로, 입구 끝 100cm 바깥 지각과 비교한다.

### Conditional Patch 마커 로컬 공간 규약

- `design/DynamicTerrain.md` §5에 좌표 3단계를 확정했다. authoring은 마커 로컬, bake는 옥탄트 로컬, runtime은 `(slot, MarkerId)` 활성화다.
- 확정한 내용:
  - patch는 base 인덱스로 저장한다.
  - patch Support는 4b sparse Atlas를 재사용하는 추가 Layer이고, ID는 MarkerId 순으로 정한다.
  - 파괴 바닥의 온전한 Support도 patch다.
  - 활성화는 상태 값만 본다.
  - 정적 Layer와 캡슐 높이 안쪽에서 겹치면 베이크 오류다.

### 검증

- `LootNPopEditor Win64 Development` 빌드 성공.
- 자동화 `LootNPop.SurfaceNavigation` 46개. 최종 실행에서 45개 통과, `WorldCollision.Api` 1개 실패.
  - 실패 내용은 "Some raycasts ran off the game thread"다. `ParallelFor`가 워커를 받지 못해 전부 게임 스레드에서 돈 경우다. 단독 재실행 2회 모두 통과했다. 이번 변경과 무관한 기존 간헐 실패다.
  - 이 실행 전 같은 날 전체 실행 2회에서는 모두 통과했다(45/45, `MeadowCaveKit` 추가 전).
- 자동화가 다시 저장한 `SurfaceNavigationTests/MeshTerrain` 두 에셋은 매번 git으로 원복했다.
- D-031 2P 스모크(에디터 바이너리 `-game` 리슨 `TestMap03?Listen`, 게스트 접속 후 약 2분 30초, 로그 `Saved/Logs/Smoke4b_Host.log`·`Smoke4b_Guest.log`): 8 옥탄트 로드와 게스트 접속 성공. 호스트·게스트 ensure·크래시·`LogLootNPop` 오류 0. 오류는 기존 종류(엔진 Experimental 툴셋 Python 초기화, `CharacterMovementComponent` 추출 실패)뿐이다. 키트 메시·지각 메시 로드 경고 없음.
- 사용자 PIE: `Meadow_00` 동굴에 걸어 들어가 내부 바닥·벽면 사격을 확인했다. 입구 통행을 막는 프랍은 없었다.
- 사용자 결정: 입구 주변 프랍 간섭은 옥탄트 양산 때 충분히 생길 수 있으므로 PCG 제외 구역을 만든다(시점 미정, `Current.md` 이관 항목).

### Phase 4b 실행 문서

- `phases/Phase04b_MultiLayerSupport.md` 작성. 구현 단위 0~4: 전제 검증·proxy 차단 → Layer 분리·face 표 → sparse rasterization·codec v2·다층 조회 → 오차 측정·해상도 → face→Layer 종단 검증.
- 사용자 결정 두 가지:
  - sparse Atlas는 지각과 같은 octahedral 격자 계열(분할 수 = 지각 N의 정수배)에 row span으로 저장한다(D-057). 조회·보간 코드를 지각과 공유하고, Phase 8 patch의 base 격자 인덱스 규약과 바로 맞는다.
  - ISM·HISM Support와 Support-only proxy는 현재 콘텐츠에 없으므로 미루고 베이크 오류로 막는다(D-058). proxy는 지금 검증 없이 일반 Support처럼 구워지는 빈틈이 있어 구현 단위 0에서 차단한다. `design/RegressionMap.md` §7의 해당 행을 "콘텐츠가 필요할 때"로 옮겼다.
- 엔진 확인: trimesh hit의 `FaceIndex`는 Chaos 내부 face 번호가 아니라 `GetExternalFaceIndexFromInternal`을 거친 원본 mesh 삼각형 번호다(`CollisionConversions.cpp` `ConvertQueryImpactHitImp`). 현재 추출기는 내부 순서로 삼각형을 읽으므로 face→Layer 표는 external 번호로 만든다. cooked trimesh의 external 표는 `UBodySetup::bSupportFaceRemapOnMeshBVH`(기본 true) 경로로 유지되는 것으로 보이지만 패키지에서 실측해 확인한다.

### 구현 단위 0 — 전제 검증과 proxy 차단

- 추출기(`LNPOctantTriangleExtractor.cpp`)는 삼각형마다 `GetExternalFaceIndexFromInternal`로 얻은 external 번호를 기록한다. 번호가 없거나(-1) 유효한 trimesh가 2개 이상이면 오류다. source key `<Actor>.<Component>`를 만들고, 옥탄트 안에서 key가 겹치면 오류다.
- 수집기는 `Support`는 있고 `Blocker`는 없는 컴포넌트를 오류로 막는다(D-058). 기존 manifest 테스트는 proxy 액터를 Blocker 액터로 바꿨다. 이 테스트는 role 행 2개를 보는 목적이라 의미가 같다.
- editor 결과(`Bake.FixtureTriangleExtractionMatchesExact`): 지각·분리 sheet·양면 판·음수 scale 슬래브 모두 hit `FaceIndex` → 추출 삼각형의 위치·법선 불일치가 0이다. 내부 순서와 external 번호가 다른 삼각형은 지각 9,208/9,209, sheet 528/528, 판 528/528, 슬래브 0/12다. Chaos 쿠킹이 BVH 순서로 삼각형을 재배열하기 때문이다.
- 패키지 결과: 비-Shipping 명령 `LNP.SurfaceNav.ProbeFaceIndex [Count]`를 추가했다. 월드 중심에서 Fibonacci 2,000방향으로 `LNPSurfaceSupport` trace를 쏜다. 부하 harness 보고 끝에서 `ProbePanels` 뒤에 호출한다. Development 패키지 리슨 2P(`-LNPLoadBaseline=50`)에서 호스트(NetMode 2)·게스트(NetMode 3) 모두 hit 2,000, 누락 0으로 PASS다. cooked trimesh에 external 표가 남는다(`bSupportFaceRemapOnMeshBVH` 기본 true). 같은 실행에서 `ProbePanels`도 PASS했다.
- 자동화 `LootNPop.SurfaceNavigation` 46/46 통과. 다시 저장된 `SurfaceNavigationTests/MeshTerrain` 두 에셋은 git으로 원복했다.

### 구현 단위 1 — Layer 분리와 face 표

- runtime `LNPSupportLayers::BuildLayers`(`LNPSupportLayers.*`):
  - 지각은 Layer 0 하나이고, 나머지 source는 walkable 삼각형 연결 성분마다 Layer 하나다.
  - 연결은 0.1cm 격자 해시로 정점을 용접한 뒤 모서리 공유로 판정한다. union-find root는 작은 인덱스라서 입력 순서만으로 결과가 정해진다.
  - Layer ID는 source Key 순, 같은 source 안에서는 최소 external face 순이다.
  - face 표는 모든 face 값이 같으면 컴포넌트 단위로 두고, 아니면 external 번호 배열로 둔다.
- 베이커는 Layer 분리를 실행하고 보고서에 Layer 수를 적는다. payload는 아직 지각만 담아서 `OctantBakeDeterministic`은 저장본과 그대로 일치한다.
- 결과: `Fixture_Crust`는 Layer 5개(split 2, 양면 1, 슬래브 1)다. `Fixture_Regression`은 source 7개가 Layer 7개가 됐다. `Meadow_00`은 source 11개가 Layer 11개이고, 섬 B `SM_TerrainBox` 4개만 옆면 때문에 per-face 표를 쓴다.
- source key는 에디터 label이 아니라 actor FName이다(OFPA는 `StaticMeshActor_UAID_...`). 처음 쓴 테스트가 label(`FX_*`)로 찾다 실패해서, mesh 이름으로 찾도록 고쳤다.
- 접힌 sheet 오류는 광선 교차로만 드러나므로 구현 단위 2 검사로 옮겼다.
- 자동화 `LootNPop.SurfaceNavigation` 48/48(`Bake.SupportLayersSplit`·`OctantSupportLayers` 추가).

### 구현 단위 2 — sparse rasterization·codec v2·다층 조회

- runtime `LNPSupportAtlas`(`LNPSupportAtlas.*`)를 새로 두고 4a `LNPCrustAtlas`의 격자·법선 codec·rasterize·보간을 옮겼다. `LNPCrustAtlas`에는 이음매 스냅 rasterize, seam 좌표·짝·hash만 남았다. 지각은 `FLNPSupportLayout::MakeFull`(전체 배치)이라 샘플 인덱스가 4a와 같다.
- row span(`ComputeFootprint`): Layer 삼각형을 옥탄트 면에 중심 투영한 삼각형을 행마다 잘라 `i` 구간을 모은다. 여유 1e-4 격자 단위. 광선은 투영 영역 밖을 맞힐 수 없으므로 구간 밖은 모두 coverage hole이다. `Bake.SupportAtlasFootprint`가 전체 배치로 구운 결과와 Valid 샘플·플래그가 모두 같음을 확인했다(구간 밖 이웃을 invalid로 보기 때문에 NeedsExact도 같다).
- codec v2는 Phase 문서 §3.7 초안에서 두 가지를 바꿨다. header에 지각 N·기준 반지름을 두지 않고 Layer 표가 Layer마다 분할 수·기준 반지름을 가진다. 샘플 offset은 행 `Count` 누적이라 저장하지 않는다. 형식 원본은 `design/SurfaceBaking.md` "다층 Atlas 규약"으로 옮겼다.
- `QueryLayers`: 창에 걸친 보간 실패 Layer가 있으면 즉시 NeedsExact, 아니면 선호 Layer 우선·가장 위 Layer. 걸침 판정은 Valid 꼭짓점 반지름 범위와 창의 겹침이다(꼭짓점 하나만 창 안인지 보는 것보다 보수적).
- 베이커: source를 Key 순으로 정렬한 뒤 Layer 분리·rasterize·encode하고, 저장 전에 payload를 decode해 Layer 쌍 겹침 수를 보고서에 적는다. `BakerSchemaVersion` 2, `CurrentDataVersion` 3. bake settings hash에 `Layer.SubdivisionMultiplier`·`OverlapReportHeight`(200cm, 보고 전용)·Layer 분리 walkable·용접 거리를 넣었다.
- 합성 자동화(지각 N=200, m=2):
  - `SupportAtlasLayerQuery`: 3층 캡은 발 위치마다 각 층, 넓은 창은 가장 위, 선호 Layer 우선. 30cm 계단은 StepUp 50이면 위 칸, 20이면 섬 윗면. 위 캡 가장자리 스캔 501방향에서 NeedsExact 181, 아래 Layer로 떨어짐 0, 유령 지면 0, 가장자리를 벗어나면 가운데 캡 237.
  - `SupportAtlasFoldedSheet`: 1.3바퀴 나선 경사로는 한 Layer로 분리되고 rasterize에서 오류다.
  - `SupportAtlasCodec`: 8 Layer 23,611샘플 왕복(반지름 오차 ≤ step/2, 플래그·법선·배치·source 표 일치), 잘린·남는 바이트·v1·Key 정렬 위반·Layer 0 비전체·정수배 아닌 분할 수·int16 범위 초과를 거부.
- 세 옥탄트 재베이크(m=2):

| 옥탄트 | Layer | 비지각 샘플 | payload | 비고 |
|:---|---:|---:|---:|:---|
| `Fixture_Crust` | 5 | 1,502 | 1,918,176 | split 2 Layer 각 약 460샘플, 슬래브 윗면 62샘플 중 29 NeedsExact |
| `Fixture_Regression` | 7 | 5,717 | 1,947,109 | 통로 바닥 Layer 3이 지각·공동 바닥과 겹침 보고(39·7샘플) |
| `Meadow_00` | 11 | 25,623 | 2,088,939 | 큰 섬 17,785샘플, 섬 B 계단 3칸 각 14~15샘플 100% NeedsExact |

- 지각 지표는 4b 공통 전제 재베이크 때와 같다(`Meadow_00` 조회 NeedsExact 2.35%, 반지름 P99 1.055cm·최대 5.009cm).
- 발견: `Meadow_00` 섬 B 계단 칸은 50cm 격자에서 내부 샘플이 없다. 모든 칸 샘플이 구간 밖 이웃을 가져 NeedsExact다. 칸 위에서는 Atlas가 Layer를 고르지 못하고 exact로 간다. 구현 단위 3에서 m=4를 볼 때 이 Layer들을 따로 기록한다.
- 기존 테스트는 API 이름만 바꿨다(`FLNPCrustSample`→`FLNPSupportSample` 등, 지각 조회는 `QueryLayer(Atlas.Layers[0], …)`).
- 검증: 에디터 빌드 성공, 자동화 `LootNPop.SurfaceNavigation` 52/52(`Saved/Logs/Auto4b_U2.log`). `SurfaceNavigationTests/MeshTerrain` 두 에셋은 git으로 원복했다. `Schema/DA_MinimalOctantSurfaceData`는 저장·재로드 테스트가 `CurrentDataVersion` 3을 기록한 것이라 4a 때처럼 커밋 대상이다.

### 구현 단위 3 — Layer Atlas 오차 측정과 해상도 확정

- 자동화 `Bake.LayerAtlasExactError`를 추가했다. 세 옥탄트를 m=2·4로 굽는다. Layer마다 footprint 행·열 범위에 격자 2칸 여유를 둔 영역에서 무작위 방향 4,000개(seed 20260927+LayerId)를 뽑고, Atlas `QueryLayer`와 source 컴포넌트 하나만 복제한 physics world의 `LineTraceComponent`를 비교한다.
  - exact가 같은 source의 다른 Layer나 non-walkable face를 먼저 맞히면 face 표로 확인하고 hit 뒤 0.01cm에서 다시 쏜다(최대 8회). Atlas가 Layer 삼각형만으로 샘플링하는 기준과 맞추기 위해서다. face 표 해석 실패는 모든 경우 0이었다.
  - NeedsExact 비율의 분모는 exact가 Layer를 맞힌 방향이다(둘 다 Layer가 없는 방향은 뺐다).
  - 베이커 보고서 Layer 행에 `SourceName`(컴포넌트 전체 경로)을 추가했다. 테스트가 컴포넌트를 찾는 데 쓴다. `FLNPSupportFaceMap`은 테스트 모듈에서 `Resolve`를 부르려고 `LOOTNPOP_API`로 export했다. Phase 5 registry도 쓸 함수다.
- 결과(`Saved/Logs/Auto4b_U3_Layer.log`): 유령 지면은 모든 옥탄트·해상도·Layer에서 0이다. 반지름 오차는 두 해상도 모두 P99 약 0.12cm이고, 최대는 m=2 0.72cm(`Meadow_00` 구멍 난 경사로), m=4 0.34cm다. 법선 P99는 0.25°/0.20°다. 해상도 차이는 정확도가 아니라 NeedsExact 비율에서 나온다. `Meadow_00` 기준 27.2% → 20.6%이고, 큰 섬 3.8 → 1.9%, 섬 A·B 10~11 → 5~6%, 경사로 31 → 16%다. payload는 2,088,939 → 2,630,943바이트(Layer body 179KB → 718KB)이고 Layer 굽기 시간은 둘 다 0.1초 미만이다.
- **m=4로 확정(사용자 결정).** `FLNPOctantBakeOptions::LayerSubdivisionMultiplier` 기본값을 4로 바꾸고 세 옥탄트를 다시 구웠다(`Saved/Logs/Bake4b_U3_m4.log`, payload `Fixture_Crust` 1,950,311, `Fixture_Regression` 2,069,402, `Meadow_00` 2,630,943바이트). 합격 기준은 지각과 같은 값(P99 ≤ 2cm, 최대 ≤ 10cm, 법선 P99 ≤ 5°)을 기본 m에 단언한다.
- **발견·수정: footprint 가장자리 띠.** exact가 Layer를 맞히는데 방향을 담은 격자 삼각형 꼭짓점이 모두 footprint 밖이면, `QueryLayers`는 그 Layer를 후보로 보지 않고 아래 Layer를 고른다. m=2 `Meadow_00`에서 14,982개 중 5개였다(계단 Layer 5에서 3개, Layer 7에서 2개). `Fixture_Crust` 슬래브는 3개, `Fixture_Regression` 섬은 1개, m=4 `Meadow_00` Layer 8은 1개였다. 격자 한 칸 미만 폭의 띠지만 §3.5가 막으려던 "가장자리에서 먼 Layer로 떨어짐"에 해당한다.
  - 수정(사용자 결정): `QueryLayer`는 꼭짓점이 모두 invalid여도 꼭짓점의 6-이웃에 Valid 샘플이 있으면 `bNearFootprintEdge`로 표시하고, 반지름 범위를 그 이웃 샘플에서 가져온다. `QueryLayers`는 `IsCandidate()`로 후보를 판정한다. 꼭짓점 행이 Layer 행 구간에서 한 행 넘게 떨어져 있으면 이웃 조회를 건너뛴다.
  - 결과: missedFloor는 모든 경우 0이 됐고 자동화가 이를 단언한다. 합성 `SupportAtlasLayerQuery` 위 캡 가장자리 스캔은 NeedsExact 181 → 260, 가장자리 밖 가운데 캡 선택 237 → 158이 됐다. 떨어짐·유령 지면은 여전히 0이다. Layer 가장자리 약 한 칸 폭에서 exact 호출이 늘어난다.
  - 한계: 격자 한 칸보다 가는 형상이 가장 가까운 Valid 샘플에서 두 칸 넘게 뻗으면 이 판정이 덮지 못한다. 현재 콘텐츠에는 없으며, 새 콘텐츠는 `Bake.LayerAtlasExactError`의 missedFloor 단언이 잡는다.
- 섬 B 계단은 m=4에서도 칸당 55샘플 중 약 70%가 NeedsExact이고 조회는 98~99%가 NeedsExact다. 보간 가능한 내부는 가장자리에서 격자 약 2칸 안쪽뿐이라 칸이 좁으면 사라진다. 구현 단위 4 계단 기준은 "칸 위에서 그 칸 Layer로 Supported이거나 NeedsExact이고, 다른 Layer는 고르지 않음"으로 바꿨다(사용자 결정). 넓은 칸과 좁은 칸을 같은 기준으로 검사한다.
- 검증: 에디터 빌드 성공, 자동화 `LootNPop.SurfaceNavigation` 53/53(`Saved/Logs/Auto4b_U3.log`). `OctantBakeDeterministic`은 m=4 저장본과 일치하고, 지각 `CrustAtlasExactError`·`CrustSeamMatch`는 무회귀다. `SurfaceNavigationTests/MeshTerrain` 두 에셋은 git으로 원복했다.
