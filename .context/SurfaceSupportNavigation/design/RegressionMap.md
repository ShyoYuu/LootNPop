# Surface Support 공통 회귀 맵

> 상태: 기준 설계
> 맵: `/Game/Maps/SurfaceNavigation/L_SurfaceRegression`
> 생성 스크립트: `Scripts/GenerateSurfaceRegressionMap.py`

## 1. 좌표 규약

- 월드 중심은 `(0, 0, 0)`이다.
- 기준 지각 반지름은 `25,000cm`다.
- fixture는 적도 `Z=0`의 +X에서 +Y 방향 호에 놓인다.
- 표의 방향은 중심에서 바깥쪽을 향하는 단위 벡터다.
- 캐릭터 Up은 방향의 반대인 중심 방향이다.
- 빨간색 구분용 별도 geometry 대신 `Probe_<Case>` 이름의 `LNPDecoration` 구체를 고정 debug 위치로 사용한다.
- `NavComponent` 이름은 Phase 7 전까지 테스트 oracle용 상징 이름이다. 직렬화 ID를 미리 고정하지 않는다.

## 2. 공통 기대 결과

- `LNPSurfaceSupport`는 `Support` source만 맞힌다.
- `LNPWorldExact`는 Support 여부와 무관하게 `Blocker`를 맞힌다.
- `Decoration`은 두 channel에서 모두 hit하지 않는다.
- 정적 layer 수에는 `Dynamic`, `StatefulTraversal`, `Destructible` source를 포함하지 않는다.
- 원거리 타게팅 가능은 fixture 사이의 실제 LoS가 아니라 해당 시나리오에서 blocker가 없다는 전제의 제품 기대값이다.

## 3. 테스트 지점과 oracle

| 사례 | 방향 | 고정 probe 위치(cm) | 정적 Support Layer | exact 기대 | NavComponent | 지상 추격 | 원거리 타게팅 |
|:---|:---|:---|:---:|:---|:---|:---|:---|
| 기본 지각 | `(0.996195, 0.087156, 0)` | `(24406.8, 2135.3, 0)` | 1 | 바깥 sweep이 지각에 hit | `NC_Crust` | 같은 지각에서 가능 | LoS 시 가능 |
| 부유섬 하나 | `(0.974370, 0.224951, 0)` | `(21923.3, 5061.4, 0)` | 2 | 안쪽 섬 뒤에 지각 hit가 존재 | `NC_Island1`, `NC_Crust` | layer 간 불가 | LoS 시 가능 |
| 부유섬 둘 | `(0.933580, 0.358368, 0)` | `(19418.5, 7454.1, 0)` | 3 | 두 섬과 지각을 거리순으로 구분 | `NC_Island2A`, `NC_Island2B`, `NC_Crust` | layer 간 불가 | LoS 시 가능 |
| 섬 가장자리 안쪽 | `(0.874620, 0.484810, 0)` | `(19412.3, 11389.2, 0)` | 1 | 윗면 hit | `NC_EdgeIsland` | 섬 내부 가능 | LoS 시 가능 |
| 섬 가장자리 바깥쪽 | `(0.874620, 0.484810, 0)` | `(19315.4, 11564.1, 0)` | 0 | 측벽 sweep은 hit, support는 miss | 없음 | 불가 | LoS 시 가능 |
| 단순 동굴 | `(0.798636, 0.601815, 0)` | `(19646.4, 14804.6, 0)` | 1 | 바깥쪽은 floor, 안쪽은 ceiling hit | `NC_Cave` | 입구를 통해 외부와 가능 | 천장·벽 LoS 반영 |
| 정적 프랍 | `(0.707107, 0.707107, 0)` | `(17253.4, 17253.4, 0)` | 1 | 나무·바위 hit, 장식 miss | `NC_PropGround` | blocker 우회 시 가능 | blocker LoS 반영 |
| 상태형 기둥 | `(0.601815, 0.798636, 0)` | `(14563.9, 19327.0, 0)` | gap에서 0 | 기둥 자체는 항상 hit | 좌·우 분리 후 안정 시 병합 가능 | 정지 전 불가, 안정 후 가능 | LoS 시 가능 |
| 움직이는 패널 | `(0.484810, 0.874620, 0)` | `(11538.5, 20815.9, 0)` | gap에서 0 | 패널 현재 transform에서 hit | 좌·우 정적 영역 분리 | 계획적 이용 불가 | LoS 시 가능 |
| 파괴 바닥 | `(0.358368, 0.933580, 0)` | `(8780.0, 22872.7, 0)` | gap에서 0 | 파괴 전 hit, 파괴 후 miss | 파괴 전 overlay 연결, 파괴 후 분리 | 파괴 전 가능, 후 불가 | LoS 시 가능 |
| 옥탄트 seam | `(0, 1, 0)` | `(0, 24500, 0)` | 1 | 경계 양쪽에서 연속 hit | `NC_Seam` 하나 | 경계 통과 가능 | LoS 시 가능 |

## 4. 동적 상태 oracle

| 사례 | 초기 상태 | 변경 | 기대 link | 기대 revision |
|:---|:---|:---|:---|:---|
| 상태형 기둥 | 이동 또는 직립 | 사전 정의된 안정 transform에서 완전 정지 | 안정 전 없음, 안정 후 좌·우 Walk Link 1개 | 영향 tile 지역 revision `+1` |
| 움직이는 패널 | 임의 transform | 매 프레임 transform 이동 | 계획용 Walk Link 없음 | 정적 Nav revision 변화 없음, Dynamic Support transform만 갱신 |
| 파괴 바닥 | 유효 | 파괴 이벤트 확정 | 기존 overlay link·support 제거 | 영향 tile 지역 revision `+1` |

동일 상태 이벤트의 중복 수신은 revision을 다시 증가시키지 않아야 한다. 상태형 기둥이 다시 움직이면 link를 닫고 지역 revision을 한 번 증가시킨다.

## 5. 맵 제작과 재생성

맵은 Engine 기본 Cube·Sphere·Cylinder만 참조하므로 프로젝트 전용 아트 에셋과 독립적이다. 생성 스크립트는 기존 맵을 기본적으로 덮어쓰지 않는다. 의도적으로 재생성할 때만 환경 변수 `LNP_REBUILD_SURFACE_REGRESSION_MAP=1`을 지정한다.

맵의 actor label은 `SSN_01_`부터 `SSN_10_`까지 사례 순서대로 정렬된다. Component Tag와 collision profile이 테스트 입력의 원본이며 label은 의미 판정에 사용하지 않는다.

## 6. 의도적으로 지원하지 않는 형상

- 복층·Y자·수직 동굴
- 서로 교차하는 Support sheet
- 움직이는 패널을 기다리고 탑승하는 계획 경로
- 임의 파괴 잔해가 만드는 새 보행면
- 측벽·밑면의 자동 보행면 승격
- 점프·훅·발사대·텔레포트를 사용하는 지상 NPC traversal
