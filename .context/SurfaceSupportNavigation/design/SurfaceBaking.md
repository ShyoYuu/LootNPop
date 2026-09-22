# Editor Surface Baker 설계

> 상태: 초안
> 읽기 조건: 에디터 베이커, Support Atlas, 동굴 layer 또는 베이크 검증을 구현할 때
> 최초 설계 원본: `../history/InitialPlan.md`

## Editor Surface Baker

### 입력 의미 체계

베이커는 모든 `WorldStatic`을 지면으로 취급하면 안 된다. 최소 다음 의미를 구분한다.

| 의미 | Support 베이크 | Nav blocker | Chaos exact collision |
|:---|:---:|:---:|:---:|
| Terrain Support | 예 | 형상에 따라 | 예 |
| Terrain Wall/Ceiling | 아니오 | 예 | 예 |
| Static Prop Blocker | 아니오 | 예 | 예 |
| Decoration | 아니오 | 아니오 | 보통 아니오 |
| Dynamic Terrain Source | 별도 local patch | runtime overlay | 예 |
| Destructible | 기본 상태만 | runtime invalidation | 예 |

전용 collision profile만으로 베이크 의미를 모두 표현하기 어렵다면 Actor/Component tag, material slot, vertex attribute, 전용 authoring component를 함께 사용한다.

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
seam·동굴 입구·정적 다리 portal 생성
    ↓
spawn candidate·clearance 생성
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

부유섬과 동굴은 전체 구체 격자를 점유하지 않고 자신의 angular footprint만 가진 sparse Atlas로 저장한다.

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

### 동굴 베이크

동굴은 중심에서 첫 hit만 수집하는 방식으로 만들 수 없다. 외부 내벽·동굴 천장·동굴 바닥이 같은 방향에 겹칠 수 있기 때문이다.

우선순위는 다음과 같다.

1. authoring 단계에서 동굴 바닥 Support Proxy 또는 surface attribute 제공
2. walkable triangle connectivity를 이용한 별도 Support Layer 생성
3. 방사 방향으로 여러 walkable 교차점을 저장

동굴 한 Layer는 다음 규약을 만족해야 한다.

- 해당 Layer 내부에서는 방향당 walkable floor가 최대 하나
- 바닥 법선이 지역 Up과 walkable slope 조건을 만족
- 수직 갱도 없음
- 복잡한 겹침 없음
- 입구는 Exterior Layer와 Walk Portal로 연결

### 베이크 검증 실패 조건

- source hash 불일치
- 한 Layer가 같은 방향에서 여러 바닥을 생성
- NaN/Inf 좌표 또는 법선
- 반지름 양자화 범위 초과
- 동굴 입구 portal의 capsule clearance 부족
- 옥탄트 seam 높이·법선 오차가 허용값 초과
- NavComponent에 portal 없는 고립 spawn candidate 존재
- SupportData가 참조하는 source actor 누락
- runtime collision과 Support 표면의 오차가 허용값 초과

개발 초기에는 경고로 시작하되 Shipping cook/CI 단계에서는 핵심 항목을 오류로 승격한다.

---

