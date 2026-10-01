# Editor Surface Baker 설계

> 상태: 초안
> 읽기 조건: 에디터 베이커, Support Atlas, 동굴 layer 또는 베이크 검증을 구현할 때
> 최초 설계 원본: `../history/InitialPlan.md`

## Editor Surface Baker

### 입력 의미 체계

입력 의미는 `TerrainContract.md`의 Component Tag 조합만 사용한다(D-021). 베이커는 모든 `WorldStatic`을 지면으로 취급하지 않으며, material slot이나 vertex attribute로 의미를 추론하지 않는다.

베이커 입력은 두 종류다.

| 입력 | 출처 | 산출 |
|:---|:---|:---|
| 정적 source | LVI의 `Static` 역할 컴포넌트 | 정적 Support·Nav·Traversal·Spawn stream |
| Placement Marker | LVI의 마커와 그 요소 클래스의 patch source mesh | 마커 로컬 공간 Conditional Patch(Phase 8) |

### 모듈 배치

베이커 핵심 계산은 collision 삼각형과 설정을 입력으로 받아 payload를 돌려주는 순수 함수로 runtime 모듈에 둔다(D-034). Editor 모듈은 source 수집, 태그 검증, asset 저장, 오류 보고 UI만 담당한다.

- 정상 경로에서 런타임 호출은 하지 않는다. 목적은 코드 경계 유지와 테스트 용이성이다.
- 부수 효과로 asset별 로드 시 베이크 시간을 실측할 수 있다. 현재 런타임 베이크 약 7초는 계산 비용이 아니라 트레이스 발사를 프레임당 3000개로 제한해 412프레임에 나눈 시간이다(`../../TechDesign_SurfaceCache.md`). asset 로컬 공간에서 한 번만 구우면 비용이 1/8 이하로 줄어 Nav 파생을 포함해도 asset당 1초 안팎으로 추정한다. 에디터 베이크를 택한 근거는 로드 시간보다 제작 시점 오류 피드백과 로드 예산 밖의 무거운 분석이다.

### stale 입력 범위

- collision profile/channel 정의 변경은 project config 파일 자체를 무차별 hash하지 않고 Terrain Contract version 또는 baker schema version 상승으로 반영한다.
- Phase 2 schema는 correctness를 우선해 source LVI가 직접 참조하는 owned external actor/object package를 모두 freshness manifest에 넣는다. decoration actor 저장도 false stale을 만들 수 있음을 보고서에 명시하고, 실제 제작에서 문제가 측정될 때만 contributor package 필터를 도입한다.
- Conditional Patch가 도입되면 MarkerId, transform, Actor class, 경로·상태 파라미터, patch mesh와 안정 transform을 canonical hash 입력으로 추가한다(D-041).

### 삼각형 원본

베이커가 읽는 삼각형은 런타임 exact query가 맞히는 삼각형과 같아야 한다. 그래야 "Support와 Chaos 오차"가 구조적으로 0에 가까워진다.

- 1순위는 cooked collision trimesh(`UBodySetup`의 Chaos triangle mesh)다. Editor `FLNPOctantTriangleExtractor`가 component transform을 적용해 옥탄트 로컬 삼각형으로 옮기고, 음수 scale이면 winding을 뒤집는다. 앞면 법선은 `(B-A)×(C-A)`로 Chaos 면 법선 규약과 같다.
- exact query는 `bTraceComplex=false`다. 그래서 Support component의 mesh는 `CTF_UseComplexAsSimple`이어야 한다. 아니면 exact는 단순 shape(box·convex)를 맞히고 베이커는 trimesh를 읽어 두 표면이 달라지므로 베이크 오류로 막는다(`TerrainContract.md` §8).
- Chaos 단순 query는 trimesh를 **단면**으로 본다. 뒷면에서 들어오는 trace는 맞지 않는다(Phase 4a 실측). Atlas 광선도 앞면 교차만 센다.
- Nanite mesh의 complex collision은 원본이 아니라 Nanite fallback mesh에서 만들어진다(`../research/MeshTerrain.md` "Nanite mesh의 complex collision 원본"). fallback 설정을 바꾸면 다시 굽는다.
- 다른 삼각형 원본을 쓰면 bake 후 exact trace 샘플 검증으로 오차를 측정해야 한다.
- Support-only proxy를 사용하면 대응 exact component association을 입력으로 받아 coverage와 오차를 검증한다. exact counterpart가 없는 proxy는 playable Support를 생성하지 않는다(D-039).

### 다층 교차 수집

`LineTraceMulti`는 첫 blocking hit에서 멈추므로 다층 베이크에 쓸 수 없다(`../research/ChaosSceneQueries.md`). 베이커는 Chaos scene query 대신 source별 mesh 공간 쿼리를 사용한다.

- GeometryCore `TMeshAABBTree3::FindAllHitTriangles`(`Spatial/MeshAABBTree3.h`)는 한 광선의 모든 교차를 반환한다.
- source 컴포넌트별로 트리를 만들면 Layer 후보가 컴포넌트 단위로 1차 분리된다. 한 컴포넌트 안의 분리는 walkable triangle 연결성으로 나눈다.
- 한 컴포넌트가 여러 Layer로 나뉠 수 있으므로 collision face/triangle과 local Layer ID의 대응표를 산출한다. ISM·HISM은 instance index를 추가 키로 사용하며 runtime hit identity registry가 이 표를 소비한다(D-037).

### 베이크 파이프라인

```text
옥탄트 맵 로딩
    ↓
source geometry·transform·tag 수집
    ↓
walkable triangle 분류
    ↓
연결된 surface sheet/명시적 region 분리
    ↓
Support Atlas rasterization
    ↓
coverage·normal·risk mask 계산
    ↓
성긴 Nav Grid 파생
    ↓
agent radius 기준 obstacle dilation
    ↓
local connected component 분석
    ↓
동굴 입구·정적 다리 portal 생성, 옥탄트 세 변의 이음매 샘플 기록
    ↓
spawn candidate·clearance 생성
    ↓
마커별 Conditional Patch 베이크 (Phase 8)
    ↓
source hash·통계·오류 기록
    ↓
ULNPOctantSurfaceData 저장
```

### Mass Spawn stream(Phase 5)

Mass Spawn stream은 LVI에 수동 배치한 Pod 세트 앵커와 베이커가 만든 절차 후보를 분리해 저장한다(D-059). 수동 앵커는 `SpawnPointId`, 선택적 `SpawnSetId`, 옥탄트 로컬 transform, Support `LocalLayerId`, clearance를 가진다. 절차 후보는 안정 index, 위치·법선, Layer, slope·edge·capsule clearance와 Pod/Enemy 허용 비트를 가진다.

- 수동 앵커와 절차 후보 모두 같은 정적 Support geometry에서 검증하지만, runtime 선택을 위해 Support 샘플을 다시 훑거나 scene trace를 하지 않는다.
- LVI의 point 수는 spawn 총량이 아니다. `DA_MassSpawnConfig`가 정한 총량 안에서 수동 앵커가 먼저 소비되고 부족분만 절차 후보가 채운다.
- marker transform과 ID·세트 지정, 후보 생성 설정은 stale hash 입력이다.
- codec과 할당 규약의 원본은 `../phases/Phase05_RuntimeLoader.md` §3.3~§3.5다.

### Support Atlas 파라미터화

기본 지각은 등장방형 대신 옥탄트 단위 octahedral 삼각 파라미터화를 사용한다(Phase 4a 확정, 아래 "지각 Atlas 규약").

이유:

- 극점 과밀 제거
- Octant LVI 소유권과 자연스럽게 대응
- slot rotation 적용이 명확함
- 옥탄트 단위 asset과 seam 검증이 쉬움

부유섬과 동굴은 전체 구체 격자를 점유하지 않고 자신의 angular footprint만 가진 sparse Atlas로 저장한다. footprint는 옥탄트 경계를 넘지 않는다(D-030).

단순 octahedral 사상(`p / |p|`, `x+y+z=1`)은 옥탄트 중심의 셀이 꼭짓점 근처보다 면적 약 5.2배(`3√3`), 선 길이 약 2.3배 크다. 변 중점 대비로는 선 길이 약 1.35배다. 목표 해상도는 가장 큰 셀인 중심 기준으로 잡고, 왜곡을 줄이는 사상은 메모리나 품질 문제가 측정될 때만 검토한다.

### 지각 Atlas 규약(Phase 4a 확정)

지각 Atlas의 격자·샘플·codec·조회·이음매 규약의 원본은 이 절이다. 결정 경위와 측정값은 `../phases/Phase04a_CrustAtlasAndSeams.md`와 `../history/Phase04a_Log.md`에 있다. 지각 식별 규칙은 `TerrainContract.md` §7(D-055)이 소유한다.

**격자**

- 옥탄트 면 `x+y+z=1`(x,y,z≥0) 위의 꼭짓점 중심 삼각 격자다. 분할 수 N에 대해 격자점은 `(i, j, k=N-i-j)`, 방향은 `normalize(i, j, k)`다. 변 위 격자점이 이웃 옥탄트와 정확히 같은 방향이라 이음매 비교가 보간 없는 샘플 대 샘플 비교가 된다.
- 저장 순서는 `j` 행 우선, 행 안에서 `i` 증가다. 인덱스 `j·(N+1) - j·(j-1)/2 + i`, 샘플 수 `(N+1)(N+2)/2`.
- N은 옥탄트 중심 간격 `R·√6 / N`이 bake setting `CrustSpacing` 이하가 되는 최소값이다. 기본 100cm, R=30,000cm에서 N=735(샘플 271,216개)다.

**샘플 계산**(`LNPCrustAtlas::Rasterize` → 공용 `LNPSupportAtlas::Rasterize`, runtime 순수 함수, D-034)

- 격자 방향마다 구 중심에서 바깥쪽 광선을 쏴 지각 삼각형의 앞면 교차만 센다(`TMeshAABBTree3::FindAllHitTriangles`, watertight ray). 0개면 coverage hole(`Valid=0`), 1개면 반지름·면 법선 기록, 2개 이상이면 overhang 베이크 오류다. `HitMergeDistance`(0.1cm) 안의 교차는 삼각형 모서리 중복으로 합친다.
- 광선 전에 이음매 평면에서 `SeamSnapDistance`(1e-3cm) 안의 정점 성분을 0으로 맞춘다. 변 위 광선은 이음매 평면 위를 지나므로, 경계 정점이 부동소수점 잡음만큼 옥탄트 안쪽에 있으면 경계 변을 스쳐 빗나간다(`Meadow_00` 실측 `|d| < 5e-7cm`, 스냅 전 이음매 샘플 528개 Invalid).
- `Walkable`은 법선과 지역 Up(`-방향`)의 dot ≥ 0.71(exact 이동과 같은 약 45°)이다.
- `NeedsExact`: 자신이 invalid·non-walkable이거나, 6-이웃 중 invalid가 있거나, 이웃과의 선분 경사가 walkable 각도보다 가파르거나, 이웃과 법선 각도 차가 25°를 넘을 때다. 높이 차 허용값은 따로 두지 않는다.

**샘플 인코딩**: `int16` 반지름 offset(step 0.25cm, Layer 기준 반지름 ±81.9m), `int16` octahedral 법선 2개, `uint8` 플래그. 샘플당 7바이트이고 지각 기본 해상도 body는 1,898,512바이트다. invalid 샘플의 반지름·법선은 0이다. 범위 초과·비유한 값은 인코딩 오류다. payload 배치는 아래 "다층 Atlas 규약"의 codec v2다(Phase 4a의 지각 전용 codec v1은 폐기했다).

**조회**(`LNPSupportAtlas::QueryLayer`, 지각은 Layer 0)는 위 "보간 규칙"을 따른다.

**이음매**

- 변 샘플 순서: 각 변은 로컬 축 번호가 작은 꼭짓점에서 큰 쪽으로 N+1개다(`z=0`은 +X→+Y, `x=0`은 +Y→+Z, `y=0`은 +X→+Z). `LNPCrustAtlas::GetSeamSampleCoord`.
- 대응표: `LNPCrustAtlas::ComputeSeamPairs`가 slot 회전으로 변 양 끝 꼭짓점을 월드 축에 놓아 24개 변 인스턴스를 12개 월드 변으로 짝짓는다. 현재 회전 집합(`ULNPOctantSpawnSubsystem::OctantRotations`)에서는 `x=0`·`y=0` 변끼리 정순으로, `z=0` 변끼리 역순으로 만난다. 고정 기대값은 자동화 `Bake.CrustSeamPairs`가 가진다.
- 일치 기준: 같은 월드 변의 양쪽 샘플이 같은 방향(1e-6), 반지름 차 1cm 이하, 둘 다 Valid. 옥탄트 꼭짓점(좌표축)은 네 slot 샘플이 모두 이 기준을 만족해야 한다.
- 법선은 옥탄트마다 한쪽 삼각형만 보므로 이음매에서 꺾인다. 변위 마스크 `(X·Y·Z)/R³`는 이음매에서 값은 0이지만 기울기는 0이 아니기 때문이다. 이음매 법선 차는 옥탄트 안 `NeedsExact` 기준과 같은 25° 이하여야 한다(`Meadow_00` 최대 17.7°). 이 꺾임은 exact도 똑같이 보므로 Atlas 오차가 아니다.
- seam hash는 변 샘플을 규약 순서로 읽은 양자화 반지름(int16)과 Valid 비트의 hash다. 법선은 넣지 않는다. 짝 관계만 보면 `x=0`·`y=0` hash 일치와 `z=0` 회문이면 충분하지만, 단일 대칭 프로필(D-030)이므로 세 변의 hash가 모두 같아야 한다. `SeamSignature` 문자열 대신 이 hash로 호환성을 판정한다(D-043, 런타임 검사는 Phase 5).

### 다층 Atlas 규약(Phase 4b)

Layer 분리·face 표·조회 규칙의 결정 경위는 `../phases/Phase04b_MultiLayerSupport.md` §3이다. 이 절은 구현이 따르는 형식의 원본이다.

**Layer**(`LNPSupportLayers::BuildLayers`)

- Layer 0은 지각(D-055)이고 non-walkable face까지 전부 담는다. 나머지 source는 walkable 삼각형(법선과 삼각형 중심 지역 Up의 dot ≥ 0.71)의 연결 성분(0.1cm 위치 용접 후 모서리 공유)마다 Layer 하나다.
- `LocalLayerId`는 지각 뒤에 source Key(`<Actor FName>.<Component FName>`) 오름차순, 같은 source 안에서는 최소 external face 번호 순이다.
- face 표는 external face 번호(exact hit `FaceIndex`)를 키로 한다. 모든 face 값이 같으면 컴포넌트 단위 값 하나, 아니면 face 단위 `uint16` 배열(None=0xFFFF)이다.

**격자와 row span**(`LNPSupportAtlas::ComputeFootprint`·`Rasterize`)

- 비지각 Layer 격자는 지각과 같은 octahedral 격자이고 분할 수가 `m·N`이다(D-057). m은 bake setting `LayerSubdivisionMultiplier`이고 옥탄트 안 모든 비지각 Layer가 같다. **기본 m=4(25cm, N_L=2,940)**. 구현 단위 3에서 m=2·4를 exact와 비교해 정했다. 오차는 둘 다 합격이었고, m=4가 섬·경사로의 NeedsExact를 절반으로 줄이는 대가로 `Meadow_00` payload가 2.09 → 2.63MB가 된다(`../phases/Phase04b_MultiLayerSupport.md` 구현 단위 3).
- 배치는 `j` 범위 `[J0, J0+RowCount)`와 행별 `i` 구간 `[IStart, IStart+Count)`다. Layer 삼각형을 옥탄트 면에 중심 투영한 영역 안의 격자점을 모두 담는 최소 구간이다. 광선은 투영 영역 밖에서 삼각형을 맞힐 수 없으므로 구간 밖은 모두 coverage hole과 같다. 지각은 전체 배치(`J0=0`, 행 `j`는 `[0, N-j]`)라 인덱스가 지각 격자 인덱스와 같다.
- 광선은 그 Layer 삼각형만으로 만든 트리에 쏜다. 앞면 교차가 둘 이상이면 베이크 오류다(지각 overhang, Layer의 접힌 sheet). Phase 7c부터 비지각 Layer의 접힌 sheet는 Layer를 만들기 전에 자동 분할하므로(아래 "접힌 sheet 자동 분할") 이 오류는 지각 overhang과 분할 결함의 안전망으로만 남는다.
- 플래그 규칙은 지각과 같다. 구간 밖 이웃은 invalid로 보므로 Layer 경계 샘플은 `NeedsExact`다. 이음매 스냅은 지각에만 한다.

**codec v2**(`LNPSupportAtlas::Encode`·`Decode`, little-endian)

- header 76바이트: `uint16` codec version(2), `uint16` Layer 수, `uint16` source 수, `uint16` 예약, `double` 반지름 step, 지각 seam hash 3개(`FIoHash`, 변 `x=0`·`y=0`·`z=0` 순).
- Layer 표(Layer 순): `uint16` source 번호, `uint16` 예약, `double` Layer 기준 반지름, `int32` 분할 수, `int32` `J0`, `int32` 행 수, 행마다 `int32` `IStart`·`int32` `Count`. 샘플 offset은 행 `Count` 누적으로 정해지므로 저장하지 않는다.
- body(Layer 순, Layer마다 SoA): 위 "샘플 인코딩"의 반지름·법선·플래그 배열.
- source 표(Key 오름차순): `int32` Key 바이트 수, UTF-8 Key, `uint8` 표 종류(0 컴포넌트 단위, 1 face 단위), 종류 0이면 `uint16` Layer, 1이면 `int32` 길이와 `uint16` Layer 배열.
- 기준 반지름: Layer 0은 옥탄트 기준 반지름이다. 비지각 Layer는 Valid 샘플 반지름 범위의 중간값을 step 단위로 반올림한 값이다(Valid가 없으면 옥탄트 기준 반지름).
- decode는 Layer 0 전체 배치, 비지각 분할 수가 지각 N의 정수배, 행 구간이 옥탄트 안, source Key 오름차순, face 표 값이 있는 Layer, 남는 바이트 없음을 검사한다.
- `Header.Support.ElementCount`는 모든 Layer 샘플 수 합이다. `FLNPSurfaceBakeHeader::CurrentDataVersion` 3, `FLNPOctantSurfaceBaker::BakerSchemaVersion` 2. 레이아웃을 바꾸면 codec version과 `CurrentDataVersion`을 함께 올린다.

**같은 방향 다층 조회**(`LNPSupportAtlas::QueryLayers`)

- 입력은 옥탄트 로컬 방향, 발 반지름, `MaxStepUp`, `MaxDrop`, 선호 Layer다. 탐색 창은 `[FeetRadius - MaxStepUp, FeetRadius + MaxDrop]`이고 반지름이 작을수록 위다.
- 후보 Layer는 방향을 담은 격자 삼각형 꼭짓점 중 하나라도 Valid인 Layer다. 꼭짓점이 모두 invalid여도 꼭짓점의 6-이웃에 Valid 샘플이 있으면 **footprint 가장자리 띠**로 보고 후보에 넣으며, 이때 반지름 범위는 그 이웃 샘플의 범위다. Layer 형상은 격자점 없이 격자 한 칸 미만만큼 삼각형 안으로 걸칠 수 있기 때문이다(구현 단위 3 실측: 이 띠를 빼면 m=2 `Meadow_00`에서 exact가 맞힌 방향 14,982개 중 5개가 아래 Layer로 떨어졌다).
- 후보 중 반지름 범위가 창에 걸치는데 보간되지 않는 Layer가 하나라도 있으면 `NeedsExact`다. 가장자리에서 가까운 Layer를 건너뛰고 먼 Layer로 떨어지지 않게 하기 위해서다.
- 아니면 보간 반지름이 창 안인 Layer 중 선호 Layer, 없으면 가장 위 Layer를 고른다. 창 안에 없으면 `NoSupport`, 방향이 이 옥탄트가 아니면 `NeedsExact`다.
- Layer 사이 겹침(같은 방향 반지름 차 `OverlapReportHeight` 200cm 이내)은 오류가 아니며 베이크 보고서에 Layer 쌍별 샘플 수로만 적는다.

### 접힌 sheet 자동 분할(D-064, Phase 7c 예정)

> 상태: 초안. 세부 규칙은 Phase 7c 실행 문서에서 확정한다.

건물·탑을 도입하면 나선 경사로, 한 메시 안에서 경사로로 이어진 여러 층처럼 **walkable 연결 성분 하나가 같은 방향에서 자기 위를 덮는** 콘텐츠가 흔해진다. 벽으로 끊긴 층 바닥들은 이미 서로 다른 sheet이므로 대상이 아니다. 콘텐츠 메시를 수동으로 쪼개면 반복 비용이 쌓이고, 분할 툴은 같은 판정 알고리즘에 에셋 생성·컴포넌트 교체·태그 부여가 더해진다. 그래서 베이커가 메모리 안의 삼각형 집합만 나눈다.

- 위치: `LNPSupportLayers::BuildLayers`가 비지각 walkable sheet를 만든 직후, Layer로 확정하기 전이다. 지각(Layer 0)은 분할하지 않으며 overhang은 계속 오류다.
- 접힘 판정: sheet 삼각형을 비지각 Layer 격자(`m·N`)에 중심 투영해 덮는 격자점마다 반지름을 기록한다. 같은 격자점을 `HitMergeDistance`보다 큰 반지름 차로 덮는 두 삼각형이 있으면 접힌 sheet다.
- 분할: 외부 face 번호가 가장 작은 삼각형을 시드로 모서리 인접 삼각형을 넓혀 가되, 현재 sub-sheet가 이미 다른 반지름으로 덮은 격자점을 덮는 삼각형은 받지 않고 다음 sub-sheet로 미룬다. 결과는 입력 순서만으로 정해진다(결정론, `Bake.OctantBakeDeterministic` 대상).
- 식별: sub-sheet는 기존 `LocalLayerId` 규칙(source Key 오름차순 → 최소 external face 순)을 그대로 따른다. 한 컴포넌트가 여러 Layer가 되므로 face 표는 face 단위 배열이다.
- 연결: 잘린 경계는 서로 다른 컴포넌트 Layer가 맞닿은 경계와 같게 다룬다. runtime Layer 전환은 지금의 경사로·섬 윗면 경계와 같은 exact 재획득 경로를 타고, Nav는 portal 탐색의 exact polyline sweep으로 잇는다. 잘린 선이 길면 D-065에 따라 portal을 여러 개 둔다.
- 대가: 잘린 경계 양쪽 격자 약 2칸 띠가 `NeedsExact`가 된다. 분할 수·잘린 경계 길이를 베이크 보고서에 적는다.
- 검증: 기존 자동화 `SupportAtlasFoldedSheet`(1.3바퀴 나선 경사로)를 "오류"에서 "분할 성공, sub-sheet마다 방향당 앞면 교차 1개, 인접 sub-sheet 사이 portal 존재"로 바꾼다.

### 단계 분할

- Phase 4a: 지각 Atlas, 옥탄트 세 변의 샘플링 규약, 8 slot 이음매 일치 검증
- Phase 4b: 부유섬·동굴 키트 sparse Atlas, 같은 방향 다층 선택, 공동 바닥 분리
- Phase 7c: 건물·탑 같은 입체 지형을 위한 접힌 sheet 자동 분할(D-064)

두 단계 모두 Phase 3b의 greybox 옥탄트 LVI와 fixture LVI를 입력으로 사용한다(`../Roadmap.md` §4).

Phase 1 C-option fixture의 구형 `LNP.Terrain.*` Component Tag는 입력으로 재사용하기 전에 `TerrainContract.md`의 `LNP.Surface.*` 계약으로 마이그레이션한다.

### 해상도 정책

Support 해상도는 지형별로 다르게 둘 수 있다.

- 기본 지각: 100cm(옥탄트 중심 간격, N=735). Phase 4a에서 200·100·50cm를 exact와 비교해 확정했다(`../phases/Phase04a_CrustAtlasAndSeams.md` §3.6)
- 부유섬·경사로·계단·동굴 바닥(비지각 Layer): 25cm(m=4). Phase 4b 구현 단위 3에서 50cm와 비교해 확정했다. Layer마다 다른 해상도는 두지 않는다(D-057)
- 가장자리에서 격자 약 2칸(m=4면 약 50cm) 안쪽은 보간되지 않고 exact로 간다. 그보다 좁은 칸(예: `Meadow_00` 섬 B 계단)은 사실상 exact 전용이며 이는 의도한 동작이다
- 경계와 급격한 곡률 구간: risk 표시 후 exact 폴백

전 구체를 25cm로 만드는 대신 정밀도가 필요한 Atlas에만 고해상도를 사용한다.

### 보간 규칙

다음 조건을 모두 만족할 때만 일반 보간을 허용한다.

- 동일 Support Atlas
- 필요한 corner sample 전부 valid
- coverage hole 없음
- 높이 차가 연속 지형 허용 범위 안
- normal 변화가 허용 범위 안
- risk/edge 플래그 없음

조건을 만족하지 않으면 nearest sample로 지면을 연장하지 않고 `NeedsExact`를 반환한다. 섬 가장자리 밖에 유령 지면이 생기는 것을 방지한다.

지각 Atlas는 베이크 때 위 조건을 샘플 플래그 `NeedsExact`로 미리 계산한다. 조회(`LNPSupportAtlas::QueryLayer`)는 방향을 담은 격자 삼각형 세 꼭짓점이 모두 Valid이면서 `NeedsExact`가 아닐 때만 barycentric 보간한다.

### 동굴 키트 베이크

동굴은 중심에서 첫 hit만 수집하는 방식으로 만들 수 없다. 지각·공동 천장·공동 바닥이 같은 방향에 겹치기 때문이다. 키트 계약은 `TerrainContract.md` §6이 소유한다(D-035).

- 공동 모듈과 통로 조각은 바닥을 별도 `Support` 컴포넌트로 가진다. 베이커는 컴포넌트 단위로 Layer를 나누므로 추가 추론이 필요 없다.
- 모듈 종류가 적으므로 바닥 walkable 조건, 천장 높이, 통로 capsule clearance는 모듈 제작 시 한 번 검증한다. 옥탄트 베이크는 배치 transform과 지각 입구 연결만 검증한다.

공동·통로 한 Layer는 다음 규약을 만족해야 한다.

- 해당 Layer 내부에서는 방향당 walkable floor가 최대 하나
- 바닥 법선이 지역 Up과 walkable slope 조건을 만족
- 수직 통로 없음
- 통로 입구는 지각 Layer와 Walk Portal로 연결

### 베이크 검증 실패 조건

- source hash 불일치(cook·CI에서 검사, D-029)
- 한 Layer가 같은 방향에서 여러 바닥을 생성
- 옥탄트 경계를 넘는 Support footprint 또는 마커 영향 범위
- NaN/Inf 좌표 또는 법선
- 반지름 양자화 범위 초과
- 동굴 입구 portal의 capsule clearance 부족
- 옥탄트 seam 높이·법선 오차가 허용값 초과
- StaticNavComponent에 portal 없는 고립 spawn candidate 존재
- SupportData가 참조하는 source actor 누락
- runtime collision과 Support 표면의 오차가 허용값 초과
- collision이 켜진 무태그 primitive 또는 LVI 안의 `Dynamic`·`StatefulTraversal`·`Destructible` 역할 component
- tag/profile/channel 조합 불일치
- 한 component의 collision face/instance를 Layer ID로 유일하게 해석할 수 없음
- Support proxy와 대응 exact geometry association 누락

개발 초기에는 경고로 시작하되 Shipping cook/CI 단계에서는 핵심 항목을 오류로 승격한다.

---
