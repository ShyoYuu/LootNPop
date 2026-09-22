# 지상 Nav Grid와 계층형 탐색 설계

> 상태: 초안
> 읽기 조건: Nav Grid, A*, cluster, portal, Traversal Link 또는 경로 공유를 구현할 때
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
(NavComponent, StartTile/Cluster, GoalTile/Cluster, RelevantRevisionSet)
```

초기에는 개별 A* + 결과 cache로 시작한다. 요청 수가 많으면 플레이어나 Pod를 goal로 하는 reverse flow field를 별도 최적화로 검토한다.

---

## 계층형 탐색 확장

### 포트폴리오 목표

계층형 탐색의 가치는 구현 자체가 아니라 LootNPop의 구면·다층·Mass 규모에 적용하고 단일 A* 대비 개선을 수치로 증명하는 데 있다.

비교 지표:

- 확장 node 수
- P50/P95 path latency
- scratch memory
- 동시 요청 처리량
- 경로 길이 오차
- 동적 변경 재계산 범위
- cache hit rate

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

