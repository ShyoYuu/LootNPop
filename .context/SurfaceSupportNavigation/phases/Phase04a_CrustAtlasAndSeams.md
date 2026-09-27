# Phase 4a — 지각 Support Atlas와 옥탄트 이음매

> 상태: 진행 중(2026-09-27 착수)
> 예상 범위: 2~3세션
> 선행 조건: Phase 3b greybox 옥탄트 `Meadow_00`(완료). Phase 3c와 무관(D-042)

## 1. 목표

1. 옥탄트 **지각 한 장**의 Support Atlas를 에디터에서 결정론적으로 베이크해 `ULNPOctantSurfaceData`의 `SupportPayload`에 저장한다
2. 8 slot 회전 배치에서 **12개 이음매 변의 양쪽 샘플이 일치**함을 자동화로 증명한다
3. Atlas와 exact(`LNPSurfaceSupport`) 표면의 오차를 측정해 양자화·해상도·허용 오차를 확정한다

런타임 로더와 snapshot 게시는 Phase 5다. 4a의 "이음매 일치"는 에디터 자동화에서 8 slot 회전을 합성해 검증한다.

## 2. 현재 코드의 출발점

- `ULNPOctantSurfaceData`는 Phase 2 스키마다. `Header`(DataVersion 1, source hash 3종·manifest, stream descriptor 4개)와 비어 있는 `TArray<uint8>` payload 4개를 가진다.
- `FLNPOctantSourceCollector`(Editor 모듈)는 manifest·semantic hash·settings hash만 만든다. 삼각형을 읽지 않는다.
  - 역할 태그가 없는 컴포넌트는 충돌 여부와 무관하게 건너뛴다.
  - `Dynamic`·`StatefulTraversal`·`Destructible` 수명주기를 LVI 안에서도 통과시킨다.
  - tag와 collision profile의 조합을 검증하지 않는다.
- source 옥탄트는 (+X,+Y,+Z)다. slot 회전은 `ULNPOctantSpawnSubsystem::OctantRotations`의 Pitch {0,180} × Yaw {0,90,180,270}이다(`ELNPOctantSlotRotation` 비트 순서와 같다).
- 지각 mesh는 `LNPOctantMeshGenerator`가 만들고 변위 마스크 `(X·Y·Z)/R³`로 이음매를 기준 반지름 R에 고정한다(`../../TechDesign_WorldGeneration.md` §6.5).
- `Meadow_00`(30,000cm)에는 지각 컴포넌트, 섬 3개(윗면 `Support+Blocker+Static`, 몸체 `Blocker+Static`), 섬 A 지각 일체형 언덕, 간이 동굴, 월드 장치 마커가 있다.
- 회귀 맵 `L_SurfaceRegression`은 25,000cm 일반 레벨이다. Phase 3 exact 회귀용으로 그대로 둔다.

## 3. 확정 결정

### 3.1 범위: 지각 관련만(사용자 결정, 2026-09-27)

- **4a에 포함:** 수집기 검증 보강, 지각 관련 fixture, 지각 입구 구멍(coverage hole), Nanite complex collision 원본 확인, 지각 Atlas·codec·이음매.
- **4b로 미룸:** 섬·동굴 fixture 재배치, 동굴 키트 공동 모듈과 통로, sparse Atlas, 다층 선택, Conditional Patch 개념 설계.
- 기존 `L_SurfaceRegression`의 좌표와 `WorldCollision.RegressionMap` 기대값은 4a에서 바꾸지 않는다. 새 fixture는 별도 fixture LVI에 **추가**한다. 기존 맵 재배치는 섬·동굴 fixture를 옮기는 4b에서 한다.

### 3.2 지각 Layer 식별: 이음매 3변에 닿는 유일한 Support 컴포넌트(사용자 결정, D-055)

- 섬·동굴은 옥탄트 경계를 넘지 않으므로(D-030) 세 이음매 변 모두에 닿는 `Support+Static` 컴포넌트는 지각뿐이다. 새 태그를 추가하지 않는다.
- 판정: 컴포넌트 삼각형 중 이음매 평면(`x=0`, `y=0`, `z=0`)에서 거리 허용값(초안 1cm) 안의 정점을 가진 평면이 세 개 모두인 컴포넌트다. 0개나 2개 이상이면 베이크 오류다.
- 지각이 여러 컴포넌트로 쪼개진 옥탄트는 지원하지 않는다. 필요해지면 그때 규칙을 확장한다.

### 3.3 비지각 Support는 수집·검증만(사용자 결정)

- 섬 윗면처럼 지각이 아닌 Support 컴포넌트는 수집·태그 검증·int16 캡 검사만 하고 Atlas를 만들지 않는다.
- 4a 결과물은 지각 한 장의 dense Atlas다. Layer ID·face→Layer 대응표(D-037)는 4b에서 만든다.

### 3.4 Atlas 파라미터화: octahedral 꼭짓점 격자

- 옥탄트 면 `x+y+z=1`(x,y,z≥0) 위의 **꼭짓점 중심 삼각 격자**다. 분할 수 N에 대해 격자점은 `(i, j, k=N-i-j)`, 방향은 `normalize(i, j, k)`다.
- 꼭짓점 중심을 쓰는 이유: 변 위 격자점(`i=0`, `j=0`, `k=0`)이 두 옥탄트에서 정확히 같은 방향이 된다. 이음매 대응이 보간 없는 샘플 대 샘플 비교가 되고, 대응표가 상수다.
- 저장 순서: `j` 행 우선, 행 안에서 `i` 증가. 인덱스는 `j·(N+1) - j·(j-1)/2 + i`다. 샘플 수는 `(N+1)(N+2)/2`다.
- 해상도는 가장 큰 셀인 옥탄트 중심 간격으로 잡는다(`../design/SurfaceBaking.md`). 중심 간격은 `R·√6 / N`이다. 100cm면 R=30,000에서 N≈735, 샘플 약 27.1만 개다. 꼭짓점 부근 간격은 약 33~58cm다. N은 bake setting이며 구현 단위 3의 오차 측정 뒤 확정한다.
- 이음매 변 샘플 순서 규약: 각 변은 옥탄트 **로컬 축 번호가 작은 꼭짓점에서 큰 꼭짓점 쪽으로** N+1개를 나열한다(변 `z=0`은 +X→+Y, `x=0`은 +Y→+Z, `y=0`은 +X→+Z).

### 3.5 샘플 계산

- 각 격자 방향으로 원점에서 바깥쪽 광선을 쏘고 지각 컴포넌트의 `TMeshAABBTree3::FindAllHitTriangles`(`FWatertightRay3d`, 인접 삼각형 사이 틈 없음)로 모든 교차를 모은다.
  - **앞면 교차만 센다.** Chaos 단순 query는 trimesh 뒷면을 맞히지 않으므로(구현 단위 1 실측) 뒷면까지 세면 exact와 달라진다.
  - 앞면 교차 0: coverage hole(지각 입구). `Valid=0`.
  - 앞면 교차 1: 반지름·법선 기록.
  - 앞면 교차 2 이상: 지각이 한 방향에서 여러 면을 가진다(overhang). 설계상 한 Layer의 방향당 바닥은 하나이므로(`../design/SurfaceBaking.md` "베이크 검증 실패 조건") 베이크 오류로 보고한다. 이음매 정점처럼 삼각형 모서리에서 생기는 중복 교차는 거리 허용값으로 하나로 합친다.
- 법선은 hit 삼각형의 면 법선이다. 내부형 구이므로 walkable 판정은 법선과 지역 Up(`-방향`)의 각도로 한다. 기준은 exact 이동의 `WalkableMinDot` 0.71(약 45°)과 같다.
- 플래그(초안): `Valid`, `Walkable`, `NeedsExact`. 자신이 invalid·non-walkable이거나, 6-이웃 중 invalid가 있거나, 이웃과의 선분 경사가 walkable 각도보다 가파르거나(`|Δr| > 호 길이 × tan(walkable)`), 이웃과 법선 각도 차가 허용값(초안 25°)을 넘으면 `NeedsExact`를 세운다. 높이 차는 별도 허용값을 두지 않는다. 격자 간격보다 완만한 단차는 Atlas 입장에서 걸을 수 있는 비탈이기 때문이다. 세부 원인 비트(`NearCoverageEdge`·`HeightDiscontinuity`·`SteepSlope`)는 codec에 자리만 두고 소비자가 생길 때 채운다.
- 삼각형 원본은 cooked Chaos triangle mesh다. exact query가 맞히는 삼각형과 같아야 Support와 exact 오차가 구조적으로 0에 가깝기 때문이다(`../design/SurfaceBaking.md` "삼각형 원본"). 컴포넌트 transform을 적용하고, 음수 scale이면 winding을 뒤집는다.
- 핵심 계산(격자·광선·플래그·codec)은 runtime 모듈의 순수 함수다(D-034). Editor 모듈은 수집·삼각형 추출·저장만 한다.
- 코드: 추출 `LootNPopEditor/SurfaceNavigation/LNPOctantTriangleExtractor.*`, geometry 검증·지각 식별 `LootNPop/SurfaceNavigation/LNPSurfaceBakeGeometry.*`, 격자·rasterization `LootNPop/SurfaceNavigation/LNPCrustAtlas.*`.

### 3.6 codec v1(초안, 구현 단위 3에서 확정)

- payload header: codec version, N, 기준 반지름, 반지름 양자화 step, Layer 수(4a는 1).
- 샘플: 반지름은 기준 반지름 대비 부호 있는 offset의 int16 양자화, 법선은 octahedral 2×int16(또는 2×int8) 인코딩, 플래그 uint8. 샘플당 약 5~7바이트, 옥탄트당 약 1.5~2MB다.
- 양자화 step과 법선 정밀도는 exact 오차 측정으로 정한다. 지각 높이 범위가 step × 32,767을 넘으면 베이크 오류다.
- `FLNPSurfaceBakeHeader::CurrentDataVersion`을 2로 올린다. `Header.Support` descriptor에 샘플 수·크기·content hash를 채운다.

### 3.7 이음매 검증

- **대응표**: 8 slot × 3변 = 24개 변 인스턴스가 12개 월드 변으로 짝지어진다. 짝과 역순 여부는 회전 집합이 고정이라 상수다. 손으로 쓰지 않고 slot 회전을 적용한 변 양 끝 꼭짓점으로 계산하고, 결과를 자동화가 고정 기대값과 비교한다.
- **일치 기준**: 같은 월드 변의 양쪽 샘플이 같은 방향(허용 1e-6)이고, 반지름 차가 허용값(초안 1cm) 이하이며, 둘 다 Valid다. 법선은 각 옥탄트 한쪽 삼각형만 보므로 이음매에서 꺾일 수 있다. 법선 차는 측정만 하고, 허용값은 측정 뒤 정한다.
- **seam hash**: 변 샘플을 규약 순서로 양자화한 값의 hash를 payload header에 기록한다. `SeamSignature` 문자열은 신뢰하지 않고 이 hash와 오차를 함께 본다(D-043). 같은 definition이 8 slot을 채우는 현재 구성에서는 세 변의 hash가 서로 같아야 한다(단일 대칭 프로필, D-030).
- 옥탄트 꼭짓점(좌표축)은 네 옥탄트가 만난다. 변 중점 사례와 꼭짓점 사례를 따로 검증한다.

### 3.8 Nanite complex collision 원본 확인

- 구현 단위 1에서 `Meadow_00` 지각의 cooked Chaos trimesh를 원본 mesh LOD0·Nanite fallback mesh와 정점 수·위치로 비교해 어느 쪽에서 만들어지는지 기록한다(`../research/MeshTerrain.md`).
- 베이커는 결과와 무관하게 cooked Chaos trimesh를 읽으므로 Atlas 정확성에는 영향이 없다. 확인 목적은 fallback 설정 변경이 exact 충돌을 바꾸는지 제작 규약에 남기는 것이다.
- **결과(2026-09-27): Nanite fallback에서 만들어진다.** 원본 2,304 / fallback 230 / Chaos trimesh 230. `Meadow_00` 지각은 Nanite가 꺼져 있다. 제작 규약은 `../research/MeshTerrain.md`에 남겼다.

## 4. 구현 단위

### 구현 단위 0 — 수집기 보강과 지각 fixture

- `LNPOctantSourceCollector`:
  - 충돌이 켜졌는데 Terrain Contract 태그가 없는 primitive 컴포넌트를 오류로 보고한다(`../design/TerrainContract.md` §3). 마커의 editor-only 시각화처럼 `NoCollision`인 컴포넌트는 제외한다.
  - LVI 안의 `Dynamic`·`StatefulTraversal`·`Destructible` 수명주기를 오류로 차단한다.
  - tag와 profile의 `LNPSurfaceSupport`·`LNPWorldExact` 응답 일치를 검증한다(`../design/TerrainContract.md` §8).
  - 기존 콘텐츠(`Meadow_00`)가 새 규칙에 걸리면 콘텐츠를 고친다. 규칙을 느슨하게 하지 않는다.
- 지각 fixture LVI `/Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Crust`(30,000cm). 에디터 명령 `LNP.SurfaceNav.BuildCrustFixture`(`LNPSurfaceFixtureBuilder.cpp`)가 결정론적으로 만든다. 기존 LVI가 있으면 거부하므로 다시 만들 때는 LVI를 지운 뒤 실행한다.
  - 지각 `SM_FixtureCrust_R30000`: 노이즈 없는 완전 구면 옥탄트 패치(octahedral 격자 N=96, 삼각형 9,209개)다. 기대값이 "반지름 R, 법선은 중심 방향"으로 해석적으로 정해진다. N은 Atlas 분할 수와 배수 관계가 아니어서 샘플이 정점에만 떨어지지 않는다. 입구 구멍은 위도 30°·방위 45° 방향 각반지름 1.5°(약 785cm)로 뚫었다.
  - 이음매 변 중점 3개·꼭짓점 3개·구멍 위치에 `Probe_*` decoration을 둔다.
  - 한 컴포넌트 안의 분리된 sheet(`SM_FixtureSplitSheet`, 캡 2개), 양면 판(`SM_FixtureDoubleSidedPlate`, 삼각형을 양 winding으로 중복), 음수·비균일 scale 슬래브(`SM_FixtureSlab` 100cm 정육면체, scale (4,-3,0.2). 엔진 Cube는 단순 box 충돌이라 exact가 trimesh를 맞히지 않아 구현 단위 1에서 교체)를 지각 800cm 안쪽에 둔다. 4a는 이들의 삼각형 추출·transform·법선을 검증하고 Layer 분리는 4b가 검증한다.
  - fixture는 옥탄트 내부(위도 25~40°)에 두어 꼭짓점 부근을 피한다(int16 캡, `../design/TerrainContract.md` §7).
  - Nanite complex/fallback 확인은 LVI fixture 대신 구현 단위 1의 자동화에서 transient mesh로 한다(§3.8).
- 8 slot 통합은 테스트 맵 대신 구현 단위 4 자동화가 slot 회전을 수학적으로 합성해 검증한다. 런타임 로더가 없는 4a에서 맵은 검증 이득이 없다.
- 검증: 수집기 음성 사례 자동화(무태그 충돌, LVI 내 Dynamic, profile 불일치), `Meadow_00`·fixture LVI 수집 통과(`Schema.SourceContractValidation`·`Schema.ProductionOctantSourceValidation`). **완료(2026-09-27).**

### 구현 단위 1 — 삼각형 추출과 지각 식별

- Editor: 컴포넌트의 cooked Chaos trimesh → 옥탄트 로컬 삼각형 배열(transform·음수 scale winding 반영).
- runtime 순수 함수: 지각 식별(§3.2), int16 캡 검사(`../design/TerrainContract.md` §7).
- Nanite 원본 확인(§3.8).
- 검증: fixture의 음수·비균일 scale 삼각형이 exact trace hit 위치·법선과 일치, 지각 식별 성공·0개·2개 사례.
- 상태(2026-09-27): 완료. fixture 추출 삼각형과 exact trace는 4개 컴포넌트 모두 위치 오차 0·법선 불일치 0이다. `Meadow_00` 섬 B의 엔진 Cube Support 액터 4개(계단 3단·플랫폼)가 complex-as-simple 규칙에 걸려 `SM_TerrainBox`(엔진 Cube 복제, complex-as-simple)로 교체했다. 지각 식별은 두 옥탄트 모두 정확히 하나를 고른다(`Meadow_00` Support 9개 중 `SM_Octant_Meadow_00_R30000`, 삼각형 237,606개).

### 구현 단위 2 — 격자와 rasterization

- runtime 순수 함수: 격자 인덱스·방향, 광선 교차, 샘플 반지름·법선·플래그(§3.4·§3.5).
- 합성 입력 자동화: 완전한 구면 지각(반지름 R 정확 일치), 구멍 뚫린 지각(구멍 둘레 `NeedsExact`), overhang 지각(오류).
- 상태(2026-09-27): 완료. `Bake.CrustAtlasGrid`·`Sphere`·`Hole`·`Overhang`(뒷면 판 무시 포함)·`Cliff` 통과. 실제 fixture·`Meadow_00` 지각 rasterization은 구현 단위 3에서 저장과 함께 한다.

### 구현 단위 3 — codec·저장·오차 측정

- codec v1과 `DataVersion` 2, payload header, `Header.Support` descriptor.
- Editor 콘솔 명령 `LNP.SurfaceNav.BakeOctant <LevelPath>`로 SurfaceData를 굽고 저장한다. 같은 입력을 두 번 구우면 payload hash가 같아야 한다.
- runtime 순수 조회 함수: 방향 → 보간 반지름·법선 또는 `NeedsExact`(`../design/SurfaceBaking.md` "보간 규칙"). Phase 5 소비자가 이 함수를 쓴다.
- 오차 측정 자동화: 무작위 방향 K개(고정 seed)에서 Atlas 조회와 `LNPSurfaceSupport` exact trace를 비교해 반지름 오차 P50/P99/최대, 법선 각 오차, `NeedsExact` 비율을 기록한다. 이 결과로 N·양자화 step·허용값을 정한다.
- 기록: asset 크기, 베이크 시간.

### 구현 단위 4 — 이음매 검증과 Phase 종료

- 이음매 대응표 계산과 고정 기대값 자동화(§3.7).
- 8 slot 합성 자동화: `Meadow_00`과 fixture LVI 각각으로 12개 변 일치, seam hash 기록.
- 에디터 빌드, 자동화 전체, `-game` 리슨 2P 스모크(D-031. 4a는 런타임 경로를 바꾸지 않으므로 기존 동작 무회귀 확인).
- 문서: `../design/SurfaceBaking.md`(격자·codec·이음매 규약 확정), `../design/DataModel.md`(DataVersion 2), `../design/TerrainContract.md`(지각 식별 규칙), Roadmap·Current.

## 5. 완료 조건

- [x] 수집기가 무태그 충돌·LVI 내 동적 수명주기·tag/profile 불일치를 오류로 보고하고, `Meadow_00`·fixture LVI는 통과함
- [ ] `Meadow_00`과 fixture LVI의 지각 Atlas가 베이크·저장되고, 두 번 구운 결과가 같음
- [x] 지각 식별이 두 옥탄트에서 정확히 하나를 고르고, 합성 0개·2개 사례가 오류를 냄
- [ ] Atlas와 exact 오차가 측정됐고, 그 결과로 N·양자화 step·허용값이 문서에 확정됨
- [ ] coverage hole 둘레가 `NeedsExact`이고 구멍 안에 유령 지면이 없음
- [ ] 8 slot 합성에서 12개 변의 양쪽 샘플이 허용값 안에서 일치하고, 이음매 대응표가 고정 기대값과 같음
- [x] Nanite complex collision 원본이 기록됨
- [ ] 에디터 빌드, 자동화 전체 통과, `-game` 리슨 2P 스모크 통과
