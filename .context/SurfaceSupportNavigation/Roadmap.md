# Surface Support·Navigation 로드맵

> 상태: 기준 로드맵
> 읽기 조건: Phase 전환, 전체 순서 변경, 작업 범위 재산정 시
> 마지막 갱신: 2026-09-25

## 1. 진행 원칙

- 각 Phase는 독립적으로 검증하고 커밋할 수 있어야 한다.
- 다음 Phase의 데이터 구조를 너무 일찍 고정하지 않는다.
- 위험한 엔진 기능과 프로젝트 계약을 먼저 검증한다.
- 정확성 기준선을 만든 뒤 캐시와 배치 최적화를 도입한다.
- 미래 Phase의 상세 실행서는 착수 직전에 작성한다.
- 모든 Phase 완료 조건에 `-game` 리슨 서버 2P 스모크를 포함한다. 네트워크 영향은 Phase마다 다르므로 Vertical Slice로 미루지 않는다.

## 2. 단계 의존성

`Phase 0 계약`
→ `Phase 1 Mesh Terrain 스파이크`
→ `Phase 2 데이터 스키마`
→ `Phase 3 정밀 충돌 기준선·동적 요소 스폰·투사체 전환`
→ `Phase 3b exact 전용 부유섬 프로토타입`
→ `Phase 4a 지각 Atlas와 옥탄트 이음매`
→ `Phase 4b 부유섬·동굴 키트 다층`
→ `Phase 5 런타임 전환`
→ `Phase 6 Enemy 이동 전환`
→ `Phase 7 일반 A*와 도달성`
→ `Phase 8 동적 연결`
→ `Phase 9 Vertical Slice`
→ `Phase 10 대규모 추격 경로 비교`

`Phase 3c 완전 비행 NPC 기반`은 Phase 3 exact query에서 갈라지는 병렬 분기이며 Phase 4a의 선행 조건이 아니다(D-042). `Phase 12 투사체 최적화`는 핵심 지상 경로가 안정된 뒤 독립적으로 진행할 수 있다. 기존 `Phase 11 비행 NPC`는 `Phase 3c`로 앞당겼고, 11번은 결번으로 둔다.

## 3. Phase 요약

| Phase | 목표 | 예상 | 상태 | 핵심 완료 조건 |
|:---|:---|:---:|:---|:---|
| 0 | 지형 의미 계약과 회귀 테스트 맵 | 1세션 | 완료 | 모든 후속 단계가 공유할 맵·좌표·지원 범위 확정 |
| 1 | Mesh Terrain 제작 스파이크 | 1~2세션 | 완료 | runtime static mesh/collision/metadata 산출물 계약 확정 |
| 2 | Octant Definition과 베이크 스키마 | 1~2세션 | 완료 | 기존 옥탄트를 새 정의로 결정론적으로 로드 |
| 3 | MassWorldCollision 정확성 기준선 | 3~5세션 | 완료 | production response Gate -1, worker 동기 query Gate 0, hit identity registry, 마커→서버 스폰 동적 패널, 투사체·탄도 가이드 exact 전환, 부하 기준선 |
| 3b | exact 전용 부유섬 프로토타입 | 2~3세션 | 다음 | greybox 섬 옥탄트에서 PureEntity가 exact만으로 접지·낙하·넉백, exact 한계치 실측 |
| 3c | 완전 비행 NPC 기반 | 2~3세션 | 대기 | 저비용 3D steering과 섬 회피, 섬 위 플레이어 교전 |
| 4a | 지각 Support Atlas와 옥탄트 이음매 | 2~3세션 | 대기 | greybox 옥탄트 지각의 Atlas 베이크와 8 slot 이음매 일치 |
| 4b | 부유섬·동굴 키트 다층 베이크 | 2~3세션 | 대기 | 같은 방향 다층 Support와 공동 모듈 floor 분리 |
| 5 | 런타임 로더와 SurfaceCache 교체 | 2~3세션 | 대기 | 정상 실행에서 전체 runtime trace 제거, 클라이언트 로드, cook 단계 stale 검출 |
| 6 | Enemy 접지·공중·넉백 전환 | 3~4세션 | 대기 | PureEntity·Actor 경로의 낙하·착지·LOD 전환·패널 탑승, legacy 제거, 3b 시나리오 재측정 |
| 7 | Coarse Tiled Nav Grid, 일반 A*, 도달성 | 3~5세션 | 대기 | 프랍·절벽 우회, 연결된 섬·동굴 추격, Pod 재귀속, 슬롯 도달성 |
| 8 | Conditional Patch와 파괴 Overlay | 2~3세션 | 대기 | 지역 길 열림·닫힘, revision 기반 재탐색, 상태 복제 |
| 9 | 부유섬·동굴 Vertical Slice | 2~3세션 | 대기 | 실제 품질 옥탄트와 멀티플레이에서 설계 검증 |
| 10 | 대규모 추격 경로: flow field와 계층형 A* | 4~6세션 | 대기 | 두 방식 구현, 같은 시나리오에서 일반 A* 대비 실측 비교 후 채택 |
| 12 | 선택적 투사체 최적화 | 1~3세션 | 대기 | 정확성 유지와 측정 가능한 이득이 있을 때만 채택 |

## 4. Phase별 범위 보충

### Phase 3

1. Gate -1: production 지형의 `LNPWorldExact` response를 audit·마이그레이션한다. 베이크용 Component Tag 전환은 Phase 4까지 미룰 수 있지만 exact 소비자를 먼저 전환하지 않는다(D-036).
2. Gate -1: worker-safe hit identity registry의 키·lifetime·face/instance mapping과 slot→Level Instance 참조 보존 방식을 확정한다(D-037).
3. D-025 스레드·쿼리 방침과 Gate 0: Mass worker 동기 scene query를 `-game` 리슨 서버와 비동기 물리 조건에서 실행해 ensure 부재, query 총비용과 씬 읽기 락 대기를 분리 측정한다.
4. `LNPWorldExact` 기반 MassWorldCollision API와 POD 결과 변환을 구현한다.
5. 투사체 `IsUnderSurface` 제거와 exact segment 전환. 서버 판정, 클라이언트 ghost, 게임 스레드 탄도 가이드를 함께 바꾼다. 층과 무관한 최외곽 반지름 안전망을 유지한다.
6. Placement Marker → 서버 스폰 복제 Actor 경로를 만들고, 결정론적 움직임의 동적 패널 위에서 2P Mover가 서는지 확인한다.
7. 적·투사체 수, CombatMode 비율, warm-up과 측정 구간을 고정한 부하 시나리오로 비용 기준선을 기록한다.

### Phase 3b — exact 전용 부유섬 프로토타입

Support 캐시를 만들기 전에 두 가지를 확인한다(D-032).

- 300m 내부형 구에서 부유섬이 플레이 공간으로 재미있는가
- PureEntity 접지·낙하·넉백을 worker 동기 exact만으로 처리할 때 몇 마리에서 프레임 예산을 넘는가

범위:

- greybox 부유섬 옥탄트 LVI를 기준 반지름 30,000cm로 만든다(D-046). Phase 4a·4b의 입력으로 그대로 재사용한다. 이 시점에 `SphereRadius`를 30,000으로 올리고, 8 slot을 모두 30,000cm 옥탄트로 채운다.
- 소수의 PureEntity를 섬과 지각에 스폰하고, exact 하향 probe와 capsule sweep으로 접지·낙하·넉백·착지를 처리한다. 기존 SurfaceCache 경로와 CVar로 전환한다.
- 적 수를 단계적으로 늘리며 프레임 시간, exact 호출 수, 호출당 비용, 락 대기를 기록한다. 이 수치가 Phase 4의 목표 캐시 적중률과 Phase 6 재측정의 기준선이다.
- 여기서 만든 exact 접지 경로는 버리지 않고 Phase 6의 exact 폴백으로 재사용한다.

### Phase 3c — 완전 비행 NPC 기반

지상 NPC는 섬을 건너지 않으므로(D-011) 섬은 근접 적에게서 안전한 지대가 된다. 그 공백을 메우는 것이 원거리 NPC와 비행 NPC다. 의존성은 Phase 3의 exact sweep뿐이며 베이커·Nav와 무관하므로 Phase 3b 이후 언제든 병렬로 진행할 수 있고 Phase 4a를 막지 않는다(D-042). 설계는 `design/MovementIntegration.md` "완전 비행 NPC"를 따른다.

### Phase 4 공통 전제

- 회귀 fixture를 옥탄트 내부로 옮기고 fixture LVI로 만들어 8 slot 통합 경로를 탄다. 이때 fixture 좌표를 기준 반지름 30,000cm로 다시 계산한다(D-046).
- Phase 3b의 greybox 섬 옥탄트에 동굴 키트 공동 모듈 하나와 통로를 추가한다. 아트 품질은 요구하지 않는다. 동굴 geometry의 좌표 성분이 int16 복제 캡 안에 드는지 확인한다. 동굴은 옥탄트 꼭짓점(좌표축) 부근을 피한다(`design/TerrainContract.md` §7).
- Conditional Patch의 데이터 개념과 마커 로컬 공간 규약을 설계 문서에 확정한다. 구현은 Phase 8이다.
- 서로 다른 slot mask를 가진 production definition을 도입한다면 현재 greedy 선택을 최대 고유 제약 할당으로 교체하고 seam compatibility를 후보 제약에 포함한다(D-043).

### Phase 6 재측정

Phase 3b와 같은 시나리오를 캐시 경로로 다시 측정해, 캐시 도입 전후의 적 수 한계치와 exact 호출 비율을 비교한다.

### Phase 7 내부 게이트

Phase 7은 한 번에 완료하려 하지 않고 두 개의 독립 게이트로 나눈다.

- **7a — Nav 데이터 기반**: Nav codec, tile 경계, static connected component, seam/portal, ReachabilityGroup과 `ConnectivityGraphVersion`, debug visualization
- **7b — 경로 실행**: chord heuristic 일반 A*, 다중 프레임 request lifecycle, revision 검증, path cache, waypoint following, PureEntity·Actor 전달

7a의 cooked load와 8-slot 연결성 검증이 끝나기 전에 7b의 scheduler/cache 형식을 고정하지 않는다.

### Phase 10 — 대규모 추격 경로 비교

수백 마리가 소수의 플레이어·Pod로 향하는 다대소 수요에 맞춰 두 방식의 비교 가능한 최소 기능 프로토타입을 만든다(D-044).

- 목표별 flow field: 플레이어·Pod 주변 반경 한정, 여러 프레임에 분할 갱신
- 계층형 A*: Cluster·Portal 기반

Phase 7의 일반 A* 캡처를 재생하는 동일 benchmark harness에서 두 방식의 최소 기능 프로토타입을 먼저 비교한다. CPU 시간, 메모리, 동적 변경 재계산 범위, 경로 품질의 채택 기준을 통과한 방식만 production 통합한다. 비교를 위해 두 방식을 모두 제품 수준으로 완성하지 않는다.

### Phase 5와 6 사이

Phase 5는 기본 지각 Layer만 반환하는 legacy `GetSurfacePoint` adapter를 산출물로 포함한다. Phase 6 완료 전까지 부유섬이 있는 콘텐츠를 production pool에 넣지 않는다. legacy SurfaceCache와 adapter는 Phase 6 완료 조건에서 제거한다.

### Pod 재귀속

Pod 재귀속은 NavComponent와 path cost가 필요하므로 Phase 7에서 구현한다. Phase 6의 Enemy는 착지 후 기존 Parent Pod를 유지하고, 착지 지점의 Support Layer만 기록한다.

## 5. 주요 게이트

### Gate A — 지형 계약

Phase 0 완료 전에는 최종 asset schema를 확정하지 않는다.

### Gate B — Mesh Terrain 산출물

- 비-WP 일반 mesh에서 Sphere Sculpt를 안정적으로 쓸 수 있으면 C안을 채택한다.
- C안이 실패하고 WP 제작 맵에서 독립 asset 추출이 안정적이면 B안을 채택한다.
- 둘 다 불안정하면 Mesh Terrain 사용 범위를 줄이고 Modeling Tools 또는 Geometry Script를 병행한다.

### Gate C — 정밀 쿼리 비용

production exact response audit와 hit identity 계약을 Gate -1로 통과한 뒤 MassWorldCollision을 정확성 기준선으로 구현한다. 별도 BVH, 쿼리 배치, Support 기반 horizon은 Unreal Insights 측정 뒤에만 판단한다.

### Gate D — 대규모 추격 경로

일반 A*의 실제 node expansion, P95 지연, 동시 요청량이 확보된 뒤 flow field와 계층형 탐색의 개선 폭을 비교한다.

### Gate E — 선택 기능

전술 사격 위치, Flight Corridor, 투사체 horizon은 실제 플레이 또는 프로파일이 필요성을 보여줄 때만 추가한다.

## 6. 필수 범위 밖

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
- 동적 요소를 LVI 내부 복제 Actor나 복제 Level Instance로 제공하는 방식

## 7. 전체 완료 정의

- 랜덤 옥탄트 조합에서 runtime surface bake가 없다.
- 기본 지각·부유섬·동굴 키트 SupportData가 에디터에서 베이크된다.
- exact 전용 대비 캐시 도입 후의 적 수 한계치 비교 자료가 있다.
- Mass worker가 immutable snapshot을 안전하게 읽는다.
- 부유섬 가장자리와 동굴에서 유령 보간·반지름 매몰이 없다.
- 투사체가 world와 Mass target 중 실제 첫 충돌을 선택한다.
- Enemy가 부유섬·동굴에서 접지·낙하·착지한다.
- 다른 NavComponent로 날아간 Enemy가 가까운 reachable Pod로 재귀속한다.
- 별도 Nav Grid A*가 나무·바위·절벽을 제한적으로 우회한다.
- 실제 Walk 연결이 있는 부유섬만 지상 추격한다.
- 연결되지 않은 섬의 원거리 NPC도 LoS·사거리 조건에서 사격한다.
- 쓰러진 기둥이 정지한 뒤 새 길을 연다.
- 움직이는 패널이 우연히 착지한 NPC와 플레이어를 운반한다.
- 파괴가 기존 길을 열거나 닫을 수 있다.
- 일반 A*, flow field, 계층형 탐색의 성능 비교 자료가 있다.
- 서버와 클라이언트의 초기화·데이터 버전·동적 요소 상태가 일치한다.
