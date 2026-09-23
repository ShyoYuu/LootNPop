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

- 1순위는 cooked collision trimesh(`UBodySetup`의 Chaos triangle mesh)다.
- Nanite mesh의 complex collision이 원본 mesh에서 만들어지는지 fallback mesh에서 만들어지는지는 Phase 4a 착수 시 확인한다.
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

### Support Atlas 파라미터화

기본 지각은 등장방형보다 옥탄트 단위 octahedral/삼각 파라미터화를 우선 검토한다.

이유:

- 극점 과밀 제거
- Octant LVI 소유권과 자연스럽게 대응
- slot rotation 적용이 명확함
- 옥탄트 단위 asset과 seam 검증이 쉬움

부유섬과 동굴은 전체 구체 격자를 점유하지 않고 자신의 angular footprint만 가진 sparse Atlas로 저장한다. footprint는 옥탄트 경계를 넘지 않는다(D-030).

단순 octahedral 사상(`p / |p|`, `x+y+z=1`)은 옥탄트 중심의 셀이 꼭짓점 근처보다 면적 약 5.2배(`3√3`), 선 길이 약 2.3배 크다. 변 중점 대비로는 선 길이 약 1.35배다. 목표 해상도는 가장 큰 셀인 중심 기준으로 잡고, 왜곡을 줄이는 사상은 메모리나 품질 문제가 측정될 때만 검토한다.

### 단계 분할

- Phase 4a: 지각 Atlas, 옥탄트 세 변의 샘플링 규약, 8 slot 이음매 일치 검증
- Phase 4b: 부유섬·동굴 키트 sparse Atlas, 같은 방향 다층 선택, 공동 바닥 분리

두 단계 모두 Phase 3b의 greybox 옥탄트 LVI와 fixture LVI를 입력으로 사용한다(`../Roadmap.md` §4).

Phase 1 C-option fixture의 구형 `LNP.Terrain.*` Component Tag는 입력으로 재사용하기 전에 `TerrainContract.md`의 `LNP.Surface.*` 계약으로 마이그레이션한다.

### 해상도 정책

Support 해상도는 지형별로 다르게 둘 수 있다.

- 기본 지각: 100cm 전후에서 시작
- 부유섬: 25~50cm 후보
- 동굴 바닥: 25~50cm 또는 콘텐츠 폭에 맞춤
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
