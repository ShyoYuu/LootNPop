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
