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
- Support와 같은 삼각 좌표계의 6방향 암묵적 이웃
- slope·step·clearance 기반 edge cost
- bounded path request
- request scratch pool
- waypoint 단순화
- path cache
- runtime overlay 반영
- debug visualization
- path expansion 통계

A* 요청 전에 시작과 목표의 ReachabilityGroup을 비교한다. 다르면 탐색하지 않고 즉시 "경로 없음"을 반환한다. 끊긴 섬을 향한 요청이 전체 탐색 공간을 확장하는 최악 사례를 막는다. ReachabilityGroup을 재계산할 때마다 `ConnectivityGraphVersion`을 증가시키며 group의 숫자 ID만 cache 유효성 근거로 사용하지 않는다.

기본 휴리스틱은 두 node의 월드 위치 사이 3D chord distance다(D-040). 구면 arc 기반 값은 자연석 다리·기둥·동굴 shortcut보다 커질 수 있어 허용적이지 않을 수 있다. edge cost가 거리 외 가중치를 포함하면 항상 1 이상인 최소 cost multiplier만 chord distance에 곱한다. 하한을 증명할 수 없는 특수 link가 생기면 해당 query는 휴리스틱 0으로 폴백한다.

Tile은 저장·스트리밍 단위가 아니라 revision 국소성과 Cluster 구성 단위다. 월드 규모에서는 Nav 전체가 수 MB 이하라 스트리밍이 필요 없다(`DataModel.md`).

Phase 7a 첫 codec은 16×16 Tile을 사용한다. 지각 목표 간격은 200cm, 비지각 Layer는 100cm이며, 삼각 격자 이웃은 `(±1,0)`, `(0,±1)`, `(+1,-1)`, `(-1,+1)` 여섯 방향이다. 같은 Layer의 edge는 양쪽 cell이 reciprocal bit를 가져야 하며 codec decode가 이를 검증한다. 세부 agent profile과 codec 규약은 `../phases/Phase07a_NavDataFoundation.md`를 따른다.

정적 베이크 agent는 반지름 50cm·반높이 88cm이고 캡슐 축은 바닥 법선이 아니라 지역 중력 Up이다. 경사면에서는 floor normal과 Up의 dot으로 접촉 높이를 보정한다. Layer 간 portal은 800cm 범위의 coarse node 후보를 거리만으로 연결하지 않는다. 50cm 간격 exact Support trace가 만든 floor point·normal polyline이 연속·walkable이어야 하며, 캡슐이 그 polyline을 양방향 sweep할 수 있을 때만 portal을 저장한다. endpoint 직선 sweep은 구면 지각과 20° 동굴 경사 사이에서 실제 바닥을 관통하므로 사용하지 않는다(회귀 fixture Layer 0↔3 사례, 2026-09-30).

7a 베이커는 local component 쌍마다 거리가 가장 짧은 portal 하나만 저장한다(`LNPNavBaking.cpp`의 `BestPortalByComponentPair`). 동굴 입구처럼 연결이 하나뿐인 콘텐츠에는 충분하지만, 입구가 여러 개인 건물이나 계단이 둘인 층에서는 A*가 한 연결로만 돌아간다. Phase 7c에서 쌍마다 최소 간격을 둔 여러 portal을 저장한다(D-065). `Traversal.Portals`는 이미 목록이므로 codec은 그대로이고 선택 정책과 재베이크만 바뀐다. 간격·상한 값과 portal 수 증가가 베이크 시간(후보마다 exact sweep)에 주는 영향은 Phase 7c에서 측정해 정한다.

같은 Layer의 6방향 edge도 portal과 같은 exact Support polyline 검사와 sweep을 쓴다. step과 경사는 구분해 판정한다. 인접한 두 점의 지역 Up 방향 높이 차가 `max(step 한도, 수평 거리 × tan(walkable 최대 경사))` 안이어야 한다(오름은 step-up 45cm, 내림은 step-down 60cm, 경사는 walkable dot 0.71의 약 44.8°). node 끝점에는 사전 필터로, 50cm 간격 exact Support 점 사이에는 본 판정으로 적용한다. 끝점 높이 차만 step 한도와 비교하면 200cm 지각 격자에서 약 12.7°를 넘는 경사가 모두 끊긴다. Meadow에서 local component가 5,151개로 부서졌던 원인이다(2026-10-01, 수정 후 91개).

도달성 조회는 게시된 snapshot 위에서만 한다. world 위치는 `FLNPSurfaceHandle`이 가리키는 같은 slot·Layer의 가장 가까운 walkable node로 제한 반경(기본 300cm) 안에서 투영한다. 다른 Layer나 slot으로 스냅하지 않고, D-060으로 막힌 이음매 node는 제외한다. node 위치·법선은 저장하지 않으므로 대응 Support Layer를 node 방향에서 보간해 복원한다. group 조회 결과는 `SnapshotGeneration`·`ConnectivityGraphVersion`을 함께 담고, 어느 하나라도 현재 snapshot과 다르면 도달성 판정은 `Stale`이다.

반경 검색은 slot local 위치의 각 성분에 ±반경을 적용한 상자를 양의 옥탄트로 제한하고, `i/N=x/(x+y+z)`, `j/N=y/(x+y+z)`의 단조성으로 격자 검색 범위를 구한다(`LNPNavData::GetGridSearchBounds`). float 지면점의 반올림을 덮는 한 cell 여유를 더하고 최종 world 거리로 후보를 거른다. 조밀 graph 검색과 `ProjectToNode`가 같은 범위를 쓴다. 각도 반경에 N만 곱한 창은 꼭짓점 근처 후보를 누락하므로 쓰지 않는다. Layer 기준 반지름과 실제 표면 높이에 의존하지 않는다.

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

Enemy 경로 소비의 목표 node 투영은 processor 한 번의 실행 안에서만 공유한다. 동일한 목표 월드 좌표와 `SurfaceHandle` 전체(slot·Layer·generation)가 key이며 스냅 반경은 기존 300cm다. 실행 시작에 잡은 immutable Nav snapshot과 Pod overlay를 모든 조회에 사용하고, `NoNode`도 같은 실행에서는 재사용한다. 실행이 끝나면 표를 버려 다음 프레임·generation·overlay 변경에는 다시 조회한다. 시작 투영·D-062 도달성 재선택·직선 보행 검사와 요청 정책은 그대로다. 내부 trace 범위와 측정 근거는 `../history/Phase07_Log.md`의 Enemy 내부 계측 기록을 따른다.

다수 Enemy가 같은 플레이어나 Pod를 향하므로 다음 key의 path cache를 우선 검토한다.

```text
(StartTile/Cluster, GoalNode/Tile/Cluster, AgentCostProfile,
 SnapshotGeneration, ConnectivityGraphVersion, TraversedRevisionFingerprint)
```

`TraversedRevisionFingerprint`는 계산된 경로가 실제 통과한 tile/edge revision을 결과와 함께 저장한 값이다. cache lookup에서는 동일 경로 후보의 저장된 revision 목록을 현재 snapshot과 비교한다. 탐색 전에는 알 수 없는 `RelevantRevisionSet`을 key 입력으로 요구하지 않는다. Phase 7은 개별 A* + 결과 cache로 시작한다. 플레이어나 Pod를 goal로 하는 flow field와 계층형 A*는 Phase 10에서 비교 가능한 최소 기능 프로토타입으로 평가한다(D-044).

### 실행 위치

- A*는 Mass worker에서 요청별 scratch를 사용해 실행한다. snapshot과 overlay는 읽기 전용이다.
- 프레임 예산은 `시작 요청 수 × ResolveCostInExpansions + 실제 확장 수`의 엄격한 상한이다. 시작 비용을 감당할 수 없으면 요청을 큐에서 꺼내지 않는다. 프레임 전체 예산이 시작 비용보다 작으면 새 요청은 설정을 올릴 때까지 대기하며, 예산을 다음 프레임으로 누적하지 않는다.
- 병렬 확장은 잔여 예산과 실행 중 요청 수 중 작은 수만큼 앞의 요청을 선택하고 균등 몫을 배정한다. 몫의 나머지와 조기 종료로 쓰지 않은 예산은 다음 라운드에서 재사용한다. 배정되지 않은 요청은 다음 라운드·프레임으로 이어간다. 이 차감 상한은 종료 처리·병렬 대기를 포함한 시간 상한을 보장하지 않는다.
- 동일 요청 비용의 통제 비교는 `Nav.RequestCostReplay` 자동화를 쓴다. CSV의 같은 시각 요청 묶음을 모두 완료한 뒤 다음 묶음을 넣고 독립 owner를 사용한다. Pod overlay·원래 owner 취소·실제 프레임 경계·warm-up cache는 복원하지 않으므로 실제 부하 Gate는 별도로 검사한다. 실행법·예산 검토 근거는 `../history/Phase07_Log.md`의 2026-10-02 기록을 따른다.
- 실제 요청 지연은 `BeginRequestCapture` 구간에 제출·재계획된 요청만 scheduler에서 기록한다. 별도 `NavRequests_*_Timings.csv`는 owner·serial·우선순위·최종 상태와 캡처 시작 기준 `FPlatformTime` 제출·시작·종료 시각(초)을 담는다. 시작·종료 전 시각은 -1이며, warm-up 요청은 제외하고 종료 시 남은 Queued·Running은 미완료로 보존한다. 큐 지연은 시작-제출, 실행 지연은 종료-시작, 전체 지연은 종료-제출이다. 취소와 미완료는 정상 완료 분포에서 분리하고 함께 보고한다. 캡처 외에는 시각 조회와 결과 배열 기록을 하지 않는다.
- 결과 경로는 순수 엔티티(적의 90% 이상)에게는 waypoint fragment로, Actor 승격 엘리트에게는 기존 AI 이동 입력(`SetAIMoveInput`) 경로로 전달한다.
- 다음 프레임으로 넘기는 request는 시작 당시 `SnapshotGeneration`, `ConnectivityGraphVersion`과 이미 읽은 tile revision을 보존한다. 어느 하나라도 바뀌면 영향 범위를 확인해 재시작하거나 실패시키며 서로 다른 snapshot의 node를 한 결과에 섞지 않는다.

---

## 대규모 추격 경로 비교 (Phase 10)

### 포트폴리오 목표

이 게임의 경로 수요는 수백~수천 마리가 2~4명의 플레이어와 소수의 Pod로 향하는 다대소 구조다. 계층형 A*는 출발·목표 쌍이 제각각일 때 이득이 크고, flow field는 목표가 적고 추격자가 많을 때 이득이 크다. 두 방식의 최소 기능 프로토타입을 동일 benchmark harness에서 일반 A*와 비교하고, 채택 기준을 통과한 방식만 production 수준으로 통합한다(D-044).

비교 지표:

- 프레임당 경로 CPU 시간과 P50/P95 지연
- 확장 node 수 또는 갱신 셀 수
- scratch·field 메모리
- 동시 추격자 수에 따른 확장성
- 경로 길이 오차
- 동적 변경 재계산 범위
- cache hit rate

## 목표별 flow field

- 목표는 플레이어와 Pod다. field key는 goal node, agent/cost profile, snapshot generation, connectivity version을 포함한다.
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
