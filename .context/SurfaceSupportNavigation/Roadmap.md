# Surface Support·Navigation 로드맵

> 상태: 기준 로드맵
> 읽기 조건: Phase 전환, 전체 순서 변경, 작업 범위 재산정 시
> 마지막 갱신: 2026-09-21

## 1. 진행 원칙

- 각 Phase는 독립적으로 검증하고 커밋할 수 있어야 한다.
- 다음 Phase의 데이터 구조를 너무 일찍 고정하지 않는다.
- 위험한 엔진 기능과 프로젝트 계약을 먼저 검증한다.
- 정확성 기준선을 만든 뒤 캐시와 배치 최적화를 도입한다.
- 미래 Phase의 상세 실행서는 착수 직전에 작성한다.

## 2. 단계 의존성

`Phase 0 계약`
→ `Phase 1 Mesh Terrain 스파이크`
→ `Phase 2 데이터 스키마`
→ `Phase 3 정밀 충돌 기준선`
→ `Phase 4 에디터 베이커`
→ `Phase 5 런타임 전환`
→ `Phase 6 Enemy 이동 전환`
→ `Phase 7 일반 A*`
→ `Phase 8 동적 연결`
→ `Phase 9 Vertical Slice`
→ `Phase 10 계층형 탐색`

`Phase 11 비행 NPC`와 `Phase 12 투사체 최적화`는 핵심 지상 경로가 안정된 뒤 독립적으로 진행할 수 있다.

## 3. Phase 요약

| Phase | 목표 | 예상 | 상태 | 핵심 완료 조건 |
|:---|:---|:---:|:---|:---|
| 0 | 지형 의미 계약과 회귀 테스트 맵 | 1세션 | 완료 | 모든 후속 단계가 공유할 맵·좌표·지원 범위 확정 |
| 1 | Mesh Terrain 제작 스파이크 | 1~2세션 | 완료 | runtime static mesh/collision/metadata 산출물 계약 확정 |
| 2 | Octant Definition과 베이크 스키마 | 1~2세션 | 현재 | 기존 옥탄트를 새 정의로 결정론적으로 로드 |
| 3 | MassWorldCollision 정확성 기준선 | 2세션 | 대기 | 벽·섬·동굴·프랍·동적 패널 exact 충돌 및 측정 |
| 4 | Editor Support Baker | 3~5세션 | 대기 | 기본 지각·부유섬·동굴을 단일 옥탄트 데이터로 베이크 |
| 5 | 런타임 로더와 SurfaceCache 교체 | 2~3세션 | 대기 | 정상 실행에서 전체 runtime trace 제거 |
| 6 | Enemy 접지·공중·넉백 전환 | 3~4세션 | 대기 | 낙하·착지·동적 support·Pod 재귀속 동작 |
| 7 | Coarse Tiled Nav Grid와 일반 A* | 3~5세션 | 대기 | 프랍·절벽 우회와 연결된 섬·동굴 추격 |
| 8 | 상태형 Traversal Link와 파괴 Overlay | 2~3세션 | 대기 | 지역 길 열림·닫힘과 revision 기반 재탐색 |
| 9 | 부유섬·동굴 Vertical Slice | 2~3세션 | 대기 | 실제 품질 옥탄트와 멀티플레이에서 설계 검증 |
| 10 | 계층형 탐색 | 3~5세션 | 대기 | 긴 경로 P95·node expansion 개선 자료 확보 |
| 11 | 완전 비행 NPC 기반 | 2~3세션 | 대기 | 저비용 3D steering과 단순 장애물 회피 |
| 12 | 선택적 투사체 최적화 | 1~3세션 | 대기 | 정확성 유지와 측정 가능한 이득이 있을 때만 채택 |

## 4. 주요 게이트

### Gate A — 지형 계약

Phase 0 완료 전에는 최종 asset schema를 확정하지 않는다.

### Gate B — Mesh Terrain 산출물

- 비-WP 일반 mesh에서 Sphere Sculpt를 안정적으로 쓸 수 있으면 C안을 채택한다.
- C안이 실패하고 WP 제작 맵에서 독립 asset 추출이 안정적이면 B안을 채택한다.
- 둘 다 불안정하면 Mesh Terrain 사용 범위를 줄이고 Modeling Tools 또는 Geometry Script를 병행한다.

### Gate C — 정밀 쿼리 비용

MassWorldCollision을 정확성 기준선으로 먼저 구현한다. 별도 BVH, 쿼리 배치, Support 기반 horizon은 Unreal Insights 측정 뒤에만 판단한다.

### Gate D — 계층형 탐색

일반 A*의 실제 node expansion, P95 지연, 동시 요청량이 확보된 뒤 계층형 탐색의 개선 폭을 비교한다.

### Gate E — 선택 기능

전술 사격 위치, Flight Corridor, 투사체 horizon은 실제 플레이 또는 프로파일이 필요성을 보여줄 때만 추가한다.

## 5. 필수 범위 밖

- Recast NavMesh의 구면 개조
- 전 구체 sparse voxel navigation
- 전 세계 3D SDF를 CPU Mass collision에 사용
- 임의 파괴 잔해가 새 보행면 생성
- 지상 NPC의 점프·발사대·훅·텔레포트 traversal
- 움직이는 패널을 계획적으로 기다리고 탑승하는 AI
- 지상 NPC의 일시적 비행 모드
- 원거리 NPC의 전술 사격 위치 탐색
- 전면 runtime MeshPartition/MegaMesh 전환
- 근거 없는 custom Chaos BVH 복제

## 6. 전체 완료 정의

- 랜덤 옥탄트 조합에서 runtime surface bake가 없다.
- 기본 지각·부유섬·단순 동굴 SupportData가 에디터에서 베이크된다.
- Mass worker가 immutable snapshot을 안전하게 읽는다.
- 부유섬 가장자리와 동굴에서 유령 보간·반지름 매몰이 없다.
- 투사체가 world와 Mass target 중 실제 첫 충돌을 선택한다.
- Enemy가 부유섬·동굴에서 접지·낙하·착지한다.
- 다른 NavComponent로 날아간 Enemy가 가까운 reachable Pod로 재귀속한다.
- 별도 Nav Grid A*가 나무·바위·절벽을 제한적으로 우회한다.
- 실제 Walk 연결이 있는 부유섬만 지상 추격한다.
- 연결되지 않은 섬의 원거리 NPC도 LoS·사거리 조건에서 사격한다.
- 쓰러진 기둥이 정지한 뒤 새 길을 연다.
- 움직이는 패널이 우연히 착지한 NPC를 운반한다.
- 파괴가 기존 길을 열거나 닫을 수 있다.
- 일반 A*와 계층형 탐색의 성능 비교 자료가 있다.
- 서버와 클라이언트의 초기화·데이터 버전이 일치한다.
