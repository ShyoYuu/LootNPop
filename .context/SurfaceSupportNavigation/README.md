# Surface Support·월드 충돌·내비게이션 작업 안내

> 상태: 활성
> 역할: 이 작업의 진입점과 문서 라우팅
> 마지막 구조 갱신: 2026-09-21

## 1. 목적

이 폴더는 Mesh Terrain을 이용한 옥탄트 제작, 에디터 베이크 `Surface Support`, Chaos 기반 정밀 월드 충돌, 지상 NPC 내비게이션을 하나의 작업 단위로 관리한다.

핵심 목표는 다음과 같다.

- 런타임 `SurfaceCache` 베이크를 옥탄트별 에디터 베이크로 교체
- 같은 방사 방향에 여러 표면이 있는 부유섬 지원
- Mass NPC가 출입할 수 있는 단순 동굴 지원
- 캐시 기반 저비용 지면 지원과 Chaos 기반 정밀 충돌의 역할 분리
- 별도 저해상도 Nav Grid와 일반 A* 도입
- 이후 계층형 탐색으로 확장할 수 있는 식별자와 tile 구조 확보
- 움직이는 패널, 상태형 다리, 파괴로 인한 길 변경 지원

## 2. 세션 시작 시 필수 문서

이 작업을 수행하는 세션은 다음 세 문서를 먼저 읽는다.

1. `README.md`
2. `Current.md`
3. `Decisions.md`

이 세 문서를 읽은 뒤 현재 작업에 필요한 문서만 아래 라우팅 표에 따라 추가로 읽는다. `history/`와 현재 단계보다 먼 `phases/` 문서는 기본 컨텍스트에 올리지 않는다.

## 3. 문서 라우팅

| 작업 | 추가로 읽을 문서 |
|:---|:---|
| 전체 단계 조정·Phase 전환 | `Roadmap.md` |
| 현재 Phase 구현 | `Current.md`가 가리키는 `phases/PhaseNN_*.md` |
| 전체 데이터 흐름·런타임 초기화 | `design/Architecture.md` |
| 지형 의미 태그·collision profile | `design/TerrainContract.md` |
| 공통 회귀 맵·고정 좌표·기대 결과 | `design/RegressionMap.md` |
| Octant Definition·베이크 에셋·ID | `design/DataModel.md` |
| Support Atlas·동굴 layer·베이커 | `design/SurfaceBaking.md` |
| Surface query·MassWorldCollision·투사체 | `design/RuntimeCollision.md` |
| 이동 패널·기믹 다리·파괴 | `design/DynamicTerrain.md` |
| Nav Grid·A*·계층형 탐색 | `design/GroundNavigation.md` |
| 접지·낙하·넉백·Pod 재귀속·비행 NPC | `design/MovementIntegration.md` |
| 테스트·성능·소비자 전환 | `design/ValidationAndMigration.md` |
| Mesh Terrain 기능·제작 방식 검증 | `research/MeshTerrain.md` |
| Chaos BVH·scene query 재검토 | `research/ChaosSceneQueries.md` |
| 과거 경위·회귀 원인 조사 | 관련 `history/PhaseNN_Log.md` |
| 최초 통합 계획 확인 | `history/InitialPlan.md` |

## 4. 아키텍처 한 장 요약

### 에디터

`Mesh Terrain/Static Mesh/의미 태그`
→ 옥탄트별 `ULNPOctantSurfaceData`
→ Fine Support Atlas + Coarse Nav Layer + Traversal Graph

### 런타임

선택된 옥탄트 데이터 로드
→ slot transform을 적용한 immutable snapshot 게시
→ Mass worker가 lock-free read
→ 위험·정밀 구간만 Chaos scene query
→ 동적 지형은 지역 Runtime Overlay와 revision으로 반영

## 5. 핵심 용어

| 용어 | 의미 |
|:---|:---|
| Support Atlas | 특정 방향에서 캐릭터를 지지할 수 있는 표면을 샘플링한 고해상도 희소 데이터 |
| Support Layer | 같은 Atlas 좌표에 겹칠 수 있는 서로 다른 보행면 |
| Nav Layer | 저해상도 경로 탐색용 보행 셀 집합 |
| NavComponent | 정적 Walk 연결성으로 묶인 Nav 영역 |
| Nav Tile | 베이크·스트리밍·지역 무효화 단위 |
| Cluster | 계층형 탐색의 상위 노드 단위 |
| Traversal Link | 두 Nav 영역 사이의 실제 보행 가능 연결 |
| Dynamic Support | 이동 플랫폼처럼 transform이 시간에 따라 변하는 지지면 |
| Home Anchor | NPC가 배회하거나 재귀속할 Pod 기준점 |

## 6. 모든 구현이 지켜야 하는 불변 조건

- 런타임 정상 경로에서 전체 지표면 재베이크를 하지 않는다.
- Mass worker는 게시 완료된 immutable snapshot만 읽는다.
- 정밀 충돌은 우선 기존 Chaos cooked collision과 acceleration structure를 사용한다.
- 별도의 두 번째 BVH는 프로파일 근거 없이 도입하지 않는다.
- Support query는 지지면·근사·후보 축소 수단이며 임의 벽 충돌의 완전한 대체물이 아니다.
- 지상 NPC의 섬 간 이동은 실제로 이어진 정적 Walk 경로가 있을 때만 가능하다.
- 지상 NPC는 점프·발사대·훅·텔레포트로 섬을 건너지 않는다.
- 움직이는 패널은 우연히 올라탄 NPC를 운반하지만 AI가 계획적으로 이용하지 않는다.
- 임의 파괴 잔해는 새 보행면을 만들지 않는다.
- 쓰러진 기둥처럼 정해진 안정 상태는 새 Traversal Link를 열 수 있다.
- 다른 NavComponent에 착지한 NPC는 가까운 도달 가능한 활성 Pod로 재귀속한다.
- 타게팅 가능성과 지상 경로 도달 가능성은 별개의 판정이다.
- 완전 비행 NPC는 지상 Nav와 별도 이동 도메인을 사용한다.
- 투사체는 exact collision 기준선을 먼저 만들고 최적화는 측정 후 도입한다.

## 7. 문서별 소유 정보

- `README.md`: 작업 범위, 불변 조건, 문서 라우팅
- `Current.md`: 지금의 상태와 바로 다음 행동
- `Decisions.md`: 모든 세션이 알아야 하는 확정 결정
- `Roadmap.md`: 단계 순서와 Phase 게이트
- `design/`: 현재 유효한 상세 기술 설계
- `research/`: 엔진 조사, 실험 결과, 미확정 근거
- `phases/`: 해당 단계의 실행 계획과 완료 조건
- `history/`: 이미 끝난 작업, 측정 기록, 시행착오

정보의 원본은 한 곳에만 둔다. 다른 문서에서는 요약과 링크만 제공한다.

## 8. 문서 갱신 규칙

### 세션 시작

1. 필수 문서 세 개를 읽는다.
2. `Current.md`의 현재 Phase와 다음 행동을 확인한다.
3. 라우팅 표에서 필요한 설계·조사·Phase 문서만 읽는다.
4. 코드와 에셋의 실제 상태가 문서와 일치하는지 확인한다.

### 작업 중

- 새 영구 규약은 해당 `design/` 문서에 반영한다.
- 프로젝트 전체가 지켜야 하는 결정이면 `Decisions.md`에도 한 줄을 추가한다.
- 실험 수치와 조사 근거는 `research/` 또는 현재 Phase 로그에 기록한다.
- 단순 작업 일지는 설계 문서에 넣지 않는다.

### 세션 종료

1. `Current.md`를 실제 상태로 갱신한다.
2. 수행 내용과 검증 결과를 현재 `history/PhaseNN_Log.md`에 날짜별로 추가한다.
3. 완료 조건을 충족하면 Phase 문서 체크리스트를 갱신한다.
4. 설계가 바뀌었다면 관련 `design/`과 `Decisions.md`를 먼저 고친다.
5. 다음 세션이 바로 실행할 수 있는 구체적인 한두 작업을 남긴다.

### Phase 종료

1. 해당 Phase 완료 조건과 테스트 증거를 로그에 남긴다.
2. `Roadmap.md`의 상태를 갱신한다.
3. `Current.md`를 다음 Phase 기준으로 짧게 다시 작성한다.
4. 다음 Phase 실행 문서는 착수 직전에 생성하거나 구체화한다.

## 9. 문서 상태 표기

| 상태 | 의미 |
|:---|:---|
| 활성 | 매 세션의 현재 진입점 |
| 확정 | 변경 전 명시적 재논의가 필요한 제품 결정 |
| 기준 설계 | 구현이 따라야 하는 현재 기술 설계 |
| 초안 | 구현·스파이크 결과에 따라 바뀔 수 있음 |
| 검증 필요 | 엔진 또는 프로젝트 실험이 남아 있음 |
| 완료 | 해당 Phase의 완료 조건과 검증을 통과 |
| 보관 | 최신 설계가 아니며 역사 추적용 |
| 폐기 | 사용하지 않으며 기각 이유만 보존 |

## 10. 컨텍스트 예산 원칙

- 필수 세 문서는 합계 약 4,000~6,000토큰 이하를 목표로 한다.
- `Current.md`는 완료 이력을 누적하지 않고 200줄 이하를 유지한다.
- `Decisions.md`는 결론 중심의 표로 유지하고 상세 근거를 링크한다.
- 먼 미래 Phase는 `Roadmap.md`의 요약만 유지하며 착수 전에 상세화한다.
- 히스토리는 기본 로딩하지 않고 `rg` 검색으로 관련 날짜·키워드만 찾는다.
- 최초 통합 계획은 보관 문서이며 일상적인 구현 컨텍스트로 사용하지 않는다.

## 11. 현재 진입점

현재 작업 위치와 다음 행동은 `Current.md`를 따른다. 전체 순서를 다시 조정해야 할 때만 `Roadmap.md`를 읽는다.
