# Phase 4a 로그 — 지각 Support Atlas와 옥탄트 이음매

## 2026-09-27

### 착수와 범위 결정

- 실행 문서 `phases/Phase04a_CrustAtlasAndSeams.md`를 작성했다.
- 사용자 결정 세 가지:
  - 4a에는 Phase 4 공통 전제 중 지각 관련만 넣는다. 섬·동굴 fixture 재배치, 동굴 키트, Conditional Patch 개념 설계는 4b 착수 전으로 미룬다.
  - 지각 Layer는 세 이음매 변 모두에 닿는 유일한 Support 컴포넌트로 식별한다(D-055).
  - 4a는 지각만 굽는다.

### 구현 단위 0 — 수집기 보강

- `LNPOctantSourceCollector`에 다음 검증을 추가했다.
  - 무태그 충돌 컴포넌트를 오류로 보고한다. 해당 컴포넌트를 모두 모아 한 번에 보고한다.
  - LVI 안의 비-Static 수명주기를 차단한다.
  - tag와 profile의 `LNPSurfaceSupport`·`LNPWorldExact` 응답 일치를 검증한다.
  - Decoration 컴포넌트의 profile과 태그 배타성을 검증한다.
- 예외 두 가지:
  - 모든 레벨의 기본 builder brush(`Brush_0`)는 충돌이 켜져 있어 제외했다. BSP·볼륨 brush는 그대로 오류다.
  - 에디터로 레벨을 열면 transient `SmartObjectSubsystemRenderingActor`가 붙는다. transient 액터는 저장되지 않으므로 source에서 제외했다.
- 기존 `SourceDependencyCollection` 테스트는 `BlockAll` profile을 쓰고 있어서 LNP profile로 바꿨다.
- 음성 사례 11건 `Schema.SourceContractValidation`과 실제 LVI 검증 `Schema.ProductionOctantSourceValidation`을 추가했다.

### `Meadow_00` 태그 마이그레이션

- 새 검증에서 `BP_Octant_Meadow_00`의 무태그 충돌 컴포넌트 4개가 드러났다. 지각 `StaticMesh`와 PCG HISM 3개다. 섬 컴포넌트는 이미 태그가 있었다.
- 지각 SCS 템플릿에 Support+Blocker+Static을 넣었다(profile `LNPStaticTerrain`).
- `PCG_Octant_BaseProps` Static Mesh Spawner의 `TemplateDescriptor.componentTags`에 Blocker+Static을 넣었다(profile `LNPStaticBlocker`). PCG Seed를 42→43→42로 바꿔 재생성하고 LVI 외부 액터를 저장했다.
- 런타임 코드는 이 태그를 읽지 않는다.

### 지각 fixture LVI

- 에디터 명령 `LNP.SurfaceNav.BuildCrustFixture`를 추가했다. 이 명령이 `/Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Crust`를 생성한다.
  - 지각은 완전 구면 옥탄트 패치다(N=96, 삼각형 9,209개). 구멍 1.5°로 삼각형 7개가 빠졌다.
  - 분리 sheet, 양면 판, 음수·비균일 scale 슬래브, `Probe_*` 7개를 둔다.
- 새 FAutoConsoleCommand를 새 파일에 넣으면 Live Coding은 링크만 하고 컴파일하지 않는다. 에디터를 닫고 UBT 빌드가 필요했다.
- 헤드리스 `-ExecCmds`는 쉼표로 명령을 나눈다. 세미콜론을 쓰면 전체가 한 명령으로 처리돼 아무것도 실행되지 않는다. 또 에디터 바이너리는 `Quit`로 종료되지 않아 프로세스를 직접 끝냈다. `Automation RunTests ...; Quit`는 자동화 명령이 세미콜론을 스스로 해석하고 종료한다.

### 검증

- `LootNPopEditor Win64 Development` 빌드 성공
- 자동화 `LootNPop.SurfaceNavigation` 26/26 통과. 에디터 MCP와 헤드리스 둘 다에서 돌렸다.

### 구현 단위 1 — 삼각형 추출과 지각 식별

- Editor `FLNPOctantTriangleExtractor`: Support component의 cooked Chaos trimesh(`UBodySetup::TriMeshGeometries`)를 component transform으로 옥탄트 로컬 삼각형으로 옮긴다. trimesh가 없으면 `CreatePhysicsMeshes()`로 만든다.
  - 엔진 쿠킹은 winding을 뒤집어 저장하고(`bFlipNormals`) Chaos 면 법선은 `(B-A)×(C-A)`라서, 저장된 인덱스를 그대로 쓰면 엔진 기준 앞면 법선이 나온다. transform 행렬식이 음수면 두 인덱스를 바꾼다.
  - 로드만 한 source World의 component는 등록되지 않는다. 그래도 `GetComponentTransform()`이 부착 체인의 relative transform 합성과 같음을 확인했다(수집기 semantic hash가 transform을 제대로 반영한다).
  - instanced Support component는 아직 오류다(4b).
- exact query가 `bTraceComplex=false`라서 Support mesh가 `CTF_UseComplexAsSimple`이 아니면 추출기가 오류를 낸다. `../../TechDesign_WorldGeneration.md` §4.1의 기존 규약을 베이커가 강제하게 됐다. `TerrainContract.md` §8에 추가했다.
- runtime `LNPSurfaceBake`: 지각 식별(D-055), 옥탄트 경계 초과·int16 캡·비유한 좌표 검사. slot 회전은 축을 ±축으로 보내므로 성분 최대 절댓값 검사는 로컬 좌표 하나로 8 slot을 덮는다.
- fixture 슬래브가 엔진 Cube(단순 box 충돌)여서 exact가 trimesh를 맞히지 않았다. `SM_FixtureSlab`(100cm 정육면체, complex-as-simple)을 만들도록 `BuildCrustFixture`를 고치고 LVI를 지운 뒤 다시 생성했다.
- 측정(`Bake.FixtureTriangleExtractionMatchesExact`): 삼각형 앞면에서 5cm 떨어진 곳부터 되돌아오는 trace(컴포넌트당 최대 약 256개).

| 컴포넌트 | 삼각형 | 샘플 | miss | 위치 오차 최대 | 법선 불일치 | 행렬식 |
|:---|---:|---:|---:|---:|---:|---:|
| 지각 | 9,209 | 264 | 0 | 0cm | 0 | 1.0 |
| 분리 sheet | 528 | 264 | 0 | 0cm | 0 | 1.0 |
| 양면 판 | 528 | 264 | 0 | 0cm | 0 | 1.0 |
| 음수 scale 슬래브 | 12 | 12 | 0 | 0cm | 0 | -2.4 |

- 같은 테스트에서 지각 뒷면으로 들어오는 trace는 맞지 않았다. **Chaos 단순 query는 trimesh를 단면으로 본다.** Atlas 광선도 앞면 교차만 센다(구현 단위 2). Phase 3 Gate -1 B의 "내부형 double-sided shell hit normal" 항목은 4b 섬·동굴 fixture에서 이 사실을 전제로 다시 본다.
- Nanite: 원본 2,304 / fallback LOD0 230 / Chaos trimesh 230. complex collision은 Nanite fallback에서 만들어진다(`../research/MeshTerrain.md`).
- **`Meadow_00` 지각 식별 실패(콘텐츠):** 섬 B의 엔진 Cube Support 액터 4개(계단 3단·플랫폼, 3b에서 배치)가 complex-as-simple이 아니다. Blocker 전용 Cube 2개(L자 벽)는 추출 대상이 아니라 괜찮다. unreal-mcp가 연결되지 않아 이번 세션에서는 고치지 못했다.

### 구현 단위 2 — 격자와 rasterization

- runtime `LNPCrustAtlas`: 격자 인덱스·방향, `ComputeSubdivisionsForSpacing`(100cm → N=735), `Rasterize`.
  - `TMeshAABBTree3` + `FWatertightRay3d`로 모든 교차를 모으고 앞면만 센다. 0.1cm 안의 앞면 교차는 모서리·정점 중복으로 합친다. 앞면 교차 2개 이상은 overhang 오류다.
  - 광선은 `ParallelFor`로 행 단위 병렬, 플래그는 직렬이다. 샘플마다 독립이라 결정론적이다.
  - `NeedsExact`: invalid·non-walkable, invalid 이웃, 이웃 선분 경사 > walkable 각도, 이웃 법선 차 > 25°(초안).
- 합성 자동화 5종 통과.
  - 정점 정렬 구면(N=40): 861 샘플, 반지름 오차 0, NeedsExact 0
  - 비정렬 구면(mesh 40, Atlas 37): 최대 sag 14.56cm(한계 56.23cm), 법선 최대 1.85°, NeedsExact 0
  - 구멍(6°): invalid 12, 구멍 안 유령 지면 0, 구멍 둘레 미표시 0, 먼 곳 오탐 0
  - overhang 오류, 뒷면 판은 무시
  - 절벽: 처음 스케일(Atlas 간격 약 1,470cm에 단차 400cm)은 경사 약 15°의 비탈이라 절벽이 아니었다. mesh 셀 574cm·단차 1,000cm(벽 약 60°)로 바꾸자 벽 hit 49개 non-walkable, 절벽 양쪽 전부 NeedsExact, 평지 오탐 0

### 검증

- `LootNPopEditor Win64 Development` 빌드 성공
- 자동화 `LootNPop.SurfaceNavigation`: `Bake.OctantCrustIdentification` 1건 실패(위 `Meadow_00` 콘텐츠), 나머지 통과. 구현 단위 2 테스트는 `Bake.CrustAtlas*`만 따로 다시 돌렸다.

### `Meadow_00` 엔진 Cube Support 교체

- 엔진 Cube를 `/Game/Maps/Meadow_00/Islands/SM_TerrainBox`로 복제하고 BodySetup `CollisionTraceFlag`를 `CTF_UseComplexAsSimple`로 바꿨다(MCP).
- 섬 B의 Support+Blocker+Static Cube 4개(계단 3단·플랫폼)의 mesh만 교체했다. 컴포넌트의 `BasicShapeMaterial` 오버라이드가 남아 외형은 같다. Blocker 전용 Cube 2개(L자 벽)는 그대로 둔다.
- OFPA라서 dirty 전체 저장으로 외부 액터 4개를 저장했다. 다른 패키지는 바뀌지 않았다.
- 에디터 MCP 자동화 `LootNPop.SurfaceNavigation` 36/36 통과. `Meadow_00`: Support 9개, 지각 `SM_Octant_Meadow_00_R30000`(삼각형 237,606개), 성분 최대 절댓값 30,000cm.

### 구현 단위 3 — codec·저장·오차 측정

- runtime `LNPCrustAtlas`에 codec v1(`Encode`·`Decode`), octahedral 법선 인코딩, 변 샘플 좌표, 조회 `QuerySupport`를 더했다. 레이아웃과 합격 기준은 `../phases/Phase04a_CrustAtlasAndSeams.md` §3.6에 있다.
  - seam hash 3개도 payload header에 넣었다. 구현 단위 4에서 codec을 다시 올리지 않으려는 것이다.
- Editor `FLNPOctantSurfaceBaker`와 명령 `LNP.SurfaceNav.BakeOctant <LevelPath>`: 수집(hash) → 추출 → 검증 → 지각 식별 → rasterize → encode → LVI 옆 `DA_OctantSurface_<이름>` 저장. 계산이 모두 끝난 뒤에만 에셋에 쓴다.
  - 명령은 Asset Registry 스캔 완료를 기다린다. `-ExecCmds`로 시작 직후 부르면 스캔이 끝나지 않았을 수 있다.
- **`DataVersion` 기본값 함정:** 헤더 생성자가 `DataVersion`을 `CurrentDataVersion`으로 초기화하고 있었다. tagged property 직렬화는 기본값과 같은 값을 저장하지 않는다. 그래서 1→2로 올려도 `DA_MinimalOctantSurfaceData`가 재저장 후 바이트가 같았고, 옛 v1 에셋을 로드하면 2로 보였다. 기본값을 0으로 바꾸고 베이커·테스트가 명시적으로 쓰게 했다. 수정 전에 저장한 `DA_OctantSurface_*`가 새 검사에서 `DataVersion` 0으로 잡히는 것을 확인한 뒤 다시 구웠다.
- 유니티 빌드 재그룹으로 `LNPSurfaceBakeGeometryTest.cpp`의 인자 `IslandRadius`가 `LNPLoadBaseline.cpp`의 익명 네임스페이스 상수를 가렸다(C4459). 테스트 인자 이름을 `IslandDistance`로 바꿨다.

측정(`Bake.CrustAtlasExactError`): 옥탄트 무작위 방향 20,000개(seed 20260927). Atlas 조회와 지각 component 하나만 등록한 physics world의 `LNPSurfaceSupport` trace(구 중심 → 바깥)를 비교했다. "샘플 NE"는 NeedsExact 샘플 비율, "조회 NE"는 조회가 exact로 넘긴 비율이다.

| 옥탄트 | 간격 | N | payload | 샘플 NE | 조회 NE | 유령 | 반지름 P50/P99/최대(cm) | 법선 P50/P99/최대(°) |
|:---|---:|---:|---:|---:|---:|---:|:---|:---|
| fixture | 200cm | 368 | 0.48MB | 0.13% | 0.21% | 0 | 0.105 / 0.498 / 1.096 | 0.08 / 0.74 / 1.32 |
| fixture | 100cm | 735 | 1.90MB | 0.09% | 0.14% | 0 | 0.049 / 0.232 / 0.546 | 0.00 / 0.64 / 1.12 |
| fixture | 50cm | 1470 | 7.58MB | 0.08% | 0.14% | 0 | 0.040 / 0.127 / 0.319 | 0.00 / 0.56 / 0.99 |
| `Meadow_00` | 200cm | 368 | 0.48MB | 2.01% | 3.20% | 0 | 0.234 / 3.549 / 10.254 | 0.33 / 2.22 / 9.15 |
| `Meadow_00` | 100cm | 735 | 1.90MB | 1.53% | 2.40% | 0 | 0.084 / 1.055 / 5.009 | 0.24 / 2.18 / 10.81 |
| `Meadow_00` | 50cm | 1470 | 7.58MB | 1.32% | 2.01% | 0 | 0.051 / 0.427 / 2.947 | 0.10 / 1.88 / 15.65 |

- fixture 입구 구멍 원뿔(각반지름 2.25°, 2,000개): exact miss 761개, Atlas 유령 지면 0(세 해상도 모두). 조회 NE는 77.5%·53.9%·45.8%다.
- 100cm를 택했다. 50cm는 반지름 P99를 1.06 → 0.43cm로 줄이지만 에셋이 4배(옥탄트당 7.6MB)다. 200cm는 `Meadow_00` 최대 오차가 10cm로 커진다.
- 법선 최대 오차가 해상도를 올려도 줄지 않는다(15.65°). 조회는 샘플 법선을 보간하고 exact는 mesh 면 법선이라, 모서리(주름) 근처에서는 해상도와 무관하게 벌어진다. P99는 해상도에 따라 줄어든다.
- codec 왕복: 반지름 오차 ≤ step/2, 법선 최대 0.034°(합성 자동화).
- 베이크 시간(헤드리스, `BakeOctant`): `Meadow_00` 수집 0.09s·추출 0.05s·rasterize 0.14s·encode 0.00s, fixture는 합계 0.1s 미만이다. 레벨 로드 시간은 제외한 값이다. 저장 에셋 크기는 `Meadow_00` 1,934,653바이트, fixture 1,907,716바이트다.
- 지각 반지름 범위: fixture 29,996.76~30,000.00cm, `Meadow_00` 28,263.46~31,779.03cm.
- seam hash: 두 옥탄트 모두 세 변의 hash가 같다(fixture `0f671c69…`, `Meadow_00` `2c5f706a…`). 단일 대칭 프로필(D-030)과 맞는다.

### 검증

- `LootNPopEditor Win64 Development` 빌드 성공
- 헤드리스 `BakeOctant` 두 옥탄트 저장 → 자동화 `LootNPop.SurfaceNavigation` 40/40 통과
- 새 프로세스에서 `Bake.OctantBakeDeterministic` + `Schema.*` 재실행 통과(디스크 저장본이 현재 source와 일치)
- 자동화가 다시 저장한 `SurfaceNavigationTests/MeshTerrain` 두 에셋은 git으로 되돌렸다. `DA_MinimalOctantSurfaceData`는 이제 `DataVersion` 2를 실제로 저장하므로 변경을 유지한다(패키지 cooked 로드 테스트가 현재 버전을 기대한다)

### 구현 단위 4 — 이음매 검증과 Phase 종료

- runtime `LNPCrustAtlas::ComputeSeamPairs`: slot 회전으로 변 양 끝 꼭짓점(로컬 축)을 월드 축에 놓고, 무방향 월드 변 키로 24개 변 인스턴스를 12쌍으로 묶는다. 인스턴스가 둘이 아닌 월드 변은 오류다. Phase 5의 seam hash 호환성 검사(D-043)에서 재사용한다.
- 대응표(파이썬으로 `FRotationMatrix` 규약을 따로 계산해 고정 기대값으로 넣었다): `x=0`·`y=0` 변끼리 정순 8쌍(0x-1y, 0y-3x, 1x-2y, 2x-3y, 4x-7y, 4y-5x, 5y-6x, 6y-7x), `z=0` 변끼리 역순 4쌍(0-7, 1-4, 2-5, 3-6).
  - 짝 관계만으로는 `x=0`·`y=0` hash 일치와 `z=0` 회문이면 충분하다. "세 변 hash가 같다"는 단일 대칭 프로필(D-030)에서 오는 더 강한 조건이라 자동화가 따로 검사한다.
- 방향 허용값 1e-6은 실제로 필요하다. `FRotator::RotateVector`가 `FMath::SinCos` 다항 근사를 써서 Pitch 180° slot에서 약 1e-8 오차가 난다(처음 1e-9로 걸었다가 실패).
- **이음매 경계 정점 잡음:** 첫 8 slot 합성에서 `Meadow_00` 변 샘플 528개(변당 44개, 3~7개씩 군집, 세 변·양 끝 대칭)가 Invalid였다. 지각 삼각형을 직접 조사하니 이음매 평면 위 정점 399개 중 28개가 `|d| < 5e-7cm` 잡음을 가졌고, 그중 옥탄트 안쪽(양수)으로 밀린 5개(Y≈301·6476·7856·9184·10456cm)가 군집 위치와 정확히 맞았다. 변 위 광선은 평면 위를 지나므로 안쪽으로 밀린 경계 변을 스쳐 빗나간다. fixture는 정점이 정확히 0이라 문제가 없었다.
  - 해결: `Rasterize`가 광선 전에 이음매 평면 1e-3cm 안의 정점 성분을 0으로 맞춘다(`FLNPCrustRasterSettings::SeamSnapDistance`, bake setting hash 포함). 합성 회귀 `Bake.CrustAtlasSeamSnap`: 경계 정점을 5e-7cm 안쪽으로 민 구면에서 스냅 전 변 샘플 114/114 Invalid, 스냅 후 0.
  - 잡음의 출처(mesh 생성기·빌드)는 추적하지 않았다. exact 충돌에서는 1e-7cm 틈이라 capsule sweep에 영향이 없다.
- 두 옥탄트 재베이크. `Meadow_00`: Valid 271,216/271,216(이전에는 이음매 Invalid 포함), 샘플 NE 1.53% → 1.40%, 조회 NE 2.40% → 2.32%, 반지름·법선 오차는 같다(P99 1.055cm·최대 5.009cm, 법선 P99 2.18°). seam hash `2c5f706a…` → `eb7e9631…`(세 변 동일). fixture는 payload가 바뀌지 않았다(seam hash `0f671c69…`).

8 slot 합성(`Bake.CrustSeamMatch`, 저장된 SurfaceData, 월드 변 12개 × 736 = 샘플 쌍 8,832개):

| 옥탄트 | 방향 불일치 | Invalid | 반지름 차 최대 | 꼭짓점 4-slot 차 | 법선 차 P50/P99/최대 |
|:---|---:|---:|---:|---:|:---|
| fixture | 0 | 0 | 0.000cm | 0.000cm | 0.54 / 0.60 / 1.13° |
| `Meadow_00` | 0 | 0 | 0.000cm | 0.000cm | 2.51 / 16.88 / 17.66° |

- 반지름 차가 정확히 0인 이유: 같은 definition이 양쪽이고 양쪽 샘플이 같은 이음매 선분을 맞힌다. definition이 섞이는 production pool에서는 이 값이 실제 허용값(1cm)의 대상이 된다.
- `Meadow_00` 법선 차 17.7°는 이음매 주름이다. 변위 마스크 `(X·Y·Z)/R³`는 이음매에서 0이지만 기울기가 0이 아니라 양쪽 면이 꺾여 만난다. exact도 같은 법선을 보므로 Atlas 오차가 아니다. 허용값을 옥탄트 안 `NeedsExact` 기준(`MaxNeighborNormalAngleDeg` 25°)과 같게 정했다. 이보다 큰 주름은 옥탄트 안이었다면 보간하지 않았을 각도다.

### Phase 종료 검증

- `LootNPopEditor Win64 Development` 빌드 성공
- 헤드리스 `BakeOctant` 두 옥탄트 재저장 → 자동화 `LootNPop.SurfaceNavigation` 43/43 통과(Bake.* 17개 포함). 법선 주름 상한을 추가한 뒤 `CrustSeamMatch` 재통과
- D-031 2P 스모크(에디터 바이너리 `-game` 리슨 `TestMap03?Listen`, 기본 스폰, 게스트 접속 후 약 3분, 로그 `Saved/Logs/Smoke4a_Host.log`·`Smoke4a_Guest.log`): 게스트 접속 성공, 호스트·게스트 ensure·크래시·`LogLootNPop` 오류 0. 오류는 기존 종류(`CharacterMovementComponent` 추출 실패, 엔진 Experimental 툴셋 Python 초기화)뿐이다. 4a는 런타임 경로를 바꾸지 않았다(SurfaceData는 아직 런타임이 로드하지 않는다)
- 참고: 게스트의 Mover SimulatedProxy 시작 위치 경고 중 반지름 40,000cm 밖 위치가 많다(최대 약 1,512m). 3c 스모크(U1·U5)에도 같은 양상이라 4a 회귀는 아니다
- 자동화가 다시 저장한 `SurfaceNavigationTests/MeshTerrain` 두 에셋은 git으로 되돌렸다
