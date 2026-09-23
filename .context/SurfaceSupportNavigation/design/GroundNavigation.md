# 지상 Nav Grid·flow field·계층형 탐색 설계

> 상태: 초안
> 읽기 조건: Nav Grid, A*, flow field, cluster, portal, Traversal Link 또는 경로 공유를 구현할 때
> 최초 설계 원본: `../history/InitialPlan.md`

## 지상 Nav Grid

### Support Atlas와 분리하는 이유

Support 해상도는 접지 정확도를 위해 결정되고 Nav 해상도는 agent 크기와 통로 폭을 위해 결정된다.

25cm Support Atlas를 2m Nav Grid와 비교하면 같은 면적에서 node 수가 최대 64배 차이 난다. 직접 fine-grid A*는 저장 데이터 중복은 줄이지만 다음 비용이 커진다.

- A* 확장 node 수
- open/closed 작업 메모리
- 병렬 path request의 scratch memory
- 동적 overlay 셀 수
- 과도한 지형 세부에 따른 경로 흔들림

별도 Nav Grid는 위치·법선을 중복 저장하지 않고 Support 참조와 연결 정보만 저장하므로 메모리 증가가 제한적이다.

### 초기 해상도 후보

- 넓은 야외: 150~200cm
- 부유섬: 100~200cm
- 좁은 동굴: 100~150cm
- 기둥·다리: 명시적 corridor/waypoint

최종 값은 가장 좁게 허용할 통로, NPC 캡슐 지름, 프랍 간격을 기준으로 테스트 맵에서 결정한다.

### direct path 우선

모든 NPC가 항상 A*를 요청하지 않는다.

```text
목표까지 Nav/Support direct-path 검사
        │
        ├─ 통과 가능 → 기존 steering
        │
        └─ 막힘 또는 진행 교착
                     ↓
                 Grid A*
                     ↓
              Waypoint steering
                     ↓
          Fine Support + Chaos 검증
```

현재 Idle 배회의 미도달 timeout을 일반적인 stuck/path request 신호로 확장할 수 있다.

### 일반 A* 1차 구현

첫 버전은 다음 기능에 집중한다.

- Tiled Nav Grid
- stable `FNavNodeRef`
- 4방향 또는 8방향 암묵적 이웃
- slope·step·clearance 기반 edge cost
- bounded path request
- request scratch pool
- waypoint 단순화
- path cache
- runtime overlay 반영
- debug visualization
- path expansion 통계

A* 요청 전에 시작과 목표의 ReachabilityGroup을 비교한다. 다르면 탐색하지 않고 즉시 "경로 없음"을 반환한다. 끊긴 섬을 향한 요청이 전체 탐색 공간을 확장하는 최악 사례를 막는다.

휴리스틱은 두 위치의 각거리 × 관련 Layer 중 가장 작은 반지름이다. 내부형 구에서 부유섬은 지각보다 반지름이 작으므로, 지각 반지름을 쓰면 허용 가능성이 깨진다.

Tile은 저장·스트리밍 단위가 아니라 revision 국소성과 Cluster 구성 단위다. 월드 규모에서는 Nav 전체가 수 MB 이하라 스트리밍이 필요 없다(`DataModel.md`).

A* 구현이 raw grid 배열을 직접 참조하지 않도록 graph view API를 둔다.

```cpp
class FLNPNavGraphView
{
    bool IsWalkable(FNavNodeRef Node) const;
    void GetNeighbors(FNavNodeRef Node, FNeighborBuffer& Out) const;
    float GetTraversalCost(FNavNodeRef From, FNavNodeRef To) const;
};
```

### 경로 공유

다수 Enemy가 같은 플레이어나 Pod를 향하므로 다음 key의 path cache를 우선 검토한다.

```text
(ReachabilityGroup, StartTile/Cluster, GoalTile/Cluster, RelevantRevisionSet)
```

Phase 7은 개별 A* + 결과 cache로 시작한다. 플레이어나 Pod를 goal로 하는 flow field는 Phase 10에서 계층형 A*와 함께 구현해 비교한다(D-033).

### 실행 위치

- A*는 Mass worker에서 요청별 scratch를 사용해 실행한다. snapshot과 overlay는 읽기 전용이다.
- 프레임당 확장 node 예산을 두고, 초과한 요청은 다음 프레임으로 이어간다.
- 결과 경로는 순수 엔티티(적의 90% 이상)에게는 waypoint fragment로, Actor 승격 엘리트에게는 기존 AI 이동 입력(`SetAIMoveInput`) 경로로 전달한다.

---

## 대규모 추격 경로 비교 (Phase 10)

### 포트폴리오 목표

이 게임의 경로 수요는 수백~수천 마리가 2~4명의 플레이어와 소수의 Pod로 향하는 다대소 구조다. 계층형 A*는 출발·목표 쌍이 제각각일 때 이득이 크고, flow field는 목표가 적고 추격자가 많을 때 이득이 크다. 두 방식을 모두 구현하고, 구면·다층·동적 link 위에서 일반 A* 대비 개선을 같은 시나리오의 수치로 비교해 채택한다(D-033).

비교 지표:

- 프레임당 경로 CPU 시간과 P50/P95 지연
- 확장 node 수 또는 갱신 셀 수
- scratch·field 메모리
- 동시 추격자 수에 따른 확장성
- 경로 길이 오차
- 동적 변경 재계산 범위
- cache hit rate

## 목표별 flow field

- 목표는 플레이어와 Pod다. 목표 하나당 field 하나를 둔다.
- 목표 주변 반경 안에서만 Dijkstra로 거리장을 만든다. 반경 밖 개체는 반경 경계까지 일반 A* 또는 direct path로 접근한다.
- 갱신은 여러 프레임에 나눠 수행한다. 목표가 셀 몇 개 이상 움직였거나 영향 tile의 revision이 바뀌었을 때만 다시 계산한다.
- ReachabilityGroup이 다른 개체는 field를 조회하지 않는다.
- 추격자는 자기 셀에서 거리가 줄어드는 이웃 방향만 읽으므로 개체당 비용이 상수다.
- 배회, 재귀속, 목표가 드문 이동은 계속 일반 A*를 사용한다.

## 계층형 탐색

### Cluster 생성

- 여러 Nav cell/Tile을 Cluster로 묶음
- 경계의 연속 walkable span을 Portal로 압축
- Portal별 clearance 저장
- 같은 Cluster 내부 Portal 간 비용 사전 계산
- 동굴 입구·다리·seam은 고수준 edge로 등록

Cluster 크기와 Portal 압축 규칙은 1차 A* 프로파일을 보고 정한다.

### 계층형 query

```text
Start/Goal 저수준 node 찾기
        ↓
시작·목표 Cluster 연결
        ↓
고수준 Portal Graph A*
        ↓
Cluster corridor 확정
        ↓
해당 corridor 안에서만 저수준 A*
        ↓
Waypoint 단순화
```

### 동적 변경

- 변경된 Cluster만 dirty
- 해당 Cluster의 local portal connectivity/cost 재계산
- 재계산 중에는 저수준 A*로 폴백하거나 affected edge 비활성화
- 일반 moving obstacle은 hierarchy를 갱신하지 않고 local collision avoidance로 처리
- 기둥·다리 상태 전환은 명시적 high-level edge toggle로 처리

### 시각화

에디터와 PIE에서 최소 다음을 그릴 수 있어야 한다.

- Nav cell walkability
- NavComponent 색상
- Tile/Cluster 경계
- Portal span과 대표 node
- 고수준 경로
- 저수준 refinement corridor
- dirty Cluster
- runtime overlay
- 각 query의 expanded node

---

