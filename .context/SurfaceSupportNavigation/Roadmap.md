# Surface Support·Navigation 로드맵

> 상태: 기준 로드맵
> 읽기 조건: Phase 전환, 전체 순서 변경, 작업 범위 재산정 시
> 마지막 갱신: 2026-10-01

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
→ `Phase 7c 입체 지형 베이크 대응`
→ `Phase 8 동적 연결`
→ `Phase 9 Vertical Slice`
→ `Phase 10 대규모 추격 경로 비교`

`Phase 3c 완전 비행 NPC 기반`은 Phase 3 exact query에서 갈라지는 병렬 분기이며 Phase 4a의 선행 조건이 아니다(D-042). `Phase 12 투사체 최적화`는 핵심 지상 경로가 안정된 뒤 독립적으로 진행할 수 있다. 기존 `Phase 11 비행 NPC`는 `Phase 3c`로 앞당겼고, 11번은 결번으로 둔다.

2026-10-01 범위 확장 검토(건물·탑 입체 지형, 벽 타기 NPC, 비행 NPC 대량화)로 세 단계를 더했다. 기존 Phase는 버리지 않는다.

- `Phase 7c`는 7b 뒤, 8 앞의 본선이다. Phase 9의 입체 지형 콘텐츠가 이 단계에 의존한다.
- `Phase 13 활동 대역과 구간 배회 복제`는 7b의 직선 보행 검사에 의존하는 병렬 분기다.
- `Phase 14 벽 타기 NPC`는 Phase 3 exact query와 Phase 13 구간 복제에 의존하는 병렬 분기다.
- 권장 순서: 7b → 7c → 13 → 14. 8은 그 사이 필요에 따라 둔다. 비행 진형은 기각했다(D-069).

## 3. Phase 요약

| Phase | 목표 | 예상 | 상태 | 핵심 완료 조건 |
|:---|:---|:---:|:---|:---|
| 0 | 지형 의미 계약과 회귀 테스트 맵 | 1세션 | 완료 | 모든 후속 단계가 공유할 맵·좌표·지원 범위 확정 |
| 1 | Mesh Terrain 제작 스파이크 | 1~2세션 | 완료 | runtime static mesh/collision/metadata 산출물 계약 확정 |
| 2 | Octant Definition과 베이크 스키마 | 1~2세션 | 완료 | 기존 옥탄트를 새 정의로 결정론적으로 로드 |
| 3 | MassWorldCollision 정확성 기준선 | 3~5세션 | 완료 | production response Gate -1, worker 동기 query Gate 0, hit identity registry, 마커→서버 스폰 동적 패널, 투사체·탄도 가이드 exact 전환, 부하 기준선 |
| 3b | exact 전용 부유섬 프로토타입 | 2~3세션 | 완료 | greybox 섬 옥탄트에서 PureEntity가 exact만으로 접지·낙하·넉백, exact 한계치 실측 |
| 3c | 완전 비행 NPC 기반 | 2~3세션 | 완료 | 저비용 3D steering과 섬 회피, 섬 위 플레이어 교전 |
| 4a | 지각 Support Atlas와 옥탄트 이음매 | 2~3세션 | 완료 | greybox 옥탄트 지각의 Atlas 베이크와 8 slot 이음매 일치 |
| 4b | 부유섬·동굴 키트 다층 베이크 | 2~3세션 | 완료 | 같은 방향 다층 Support와 공동 모듈 floor 분리 |
| 5 | 런타임 로더와 SurfaceCache 교체 | 3~4세션 | 완료 | 정상 실행에서 전체 runtime trace 제거, 클라이언트 로드, cook 전 CI·게시 단계 stale 검출, LVI 지정점 우선 Spawn stream |
| 6 | Enemy 접지·공중·넉백 전환 | 3~4세션 | 완료 | PureEntity·Actor 경로의 낙하·착지·LOD 전환·패널 탑승, legacy 제거, 3b 시나리오 재측정 |
| 7 | Coarse Tiled Nav Grid, 일반 A*, 도달성 | 3~5세션 | 진행 중(7a 완료, 7b 계측·기능 검증·성능 개선 1 완료 / 700마리 반복 Gate 미통과) | 프랍·절벽 우회, 연결된 섬·동굴 추격, Pod 재귀속, 슬롯 도달성 |
| 7c | 입체 지형 베이크 대응 | 2~3세션 | 대기 | 접힌 sheet 자동 분할(D-064), component 쌍당 여러 portal(D-065), source별 Layer 해상도(D-072), 비행 headroom(D-073), 입체 지형·지하 대형 공동 회귀 사례와 계약 확정, cache 적중률·수용량 재측정 |
| 8 | Conditional Patch와 파괴 Overlay | 2~3세션 | 대기 | 지역 길 열림·닫힘, revision 기반 재탐색, 상태 복제 |
| 9 | 부유섬·동굴·입체 지형 Vertical Slice | 2~3세션 | 대기 | 건물·탑·계단·벽과 마커 앵커 동선, 입구 조합이 다른 지하 대형 공동을 갖춘 실제 품질 옥탄트와 멀티플레이에서 설계 검증 |
| 10 | 대규모 추격 경로: flow field와 계층형 A* | 4~6세션 | 대기 | 두 방식 구현, 같은 시나리오에서 일반 A* 대비 실측 비교 후 채택 |
| 12 | 선택적 투사체 최적화 | 1~3세션 | 대기 | 정확성 유지와 측정 가능한 이득이 있을 때만 채택 |
| 13 | PureEntity 활동 대역과 구간 배회 복제 | 3~4세션 | 대기 | Pod 단위 휴면·배회·활성(D-067), 구간 배회 복제(D-068), 공통 비용 분해 측정, 총수 재측정으로 D-054·지상 한계 대체 |
| 14 | 벽 타기 NPC(`SurfaceCrawl`) | 3~4세션 | 대기 | wall-walker 이동·Up 리팩터·법선 복제(D-066), 원거리 공격·360° 인지·재귀속, 개체 수 측정 후 예산 결정 |

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

지상 NPC는 섬을 건너지 않으므로(D-011) 섬은 근접 적에게서 안전한 지대가 된다. 그 공백을 메우는 것이 원거리 NPC와 비행 NPC다. 의존성은 Phase 3의 exact sweep뿐이며 베이커·Nav와 무관하므로 Phase 3b 이후 언제든 병렬로 진행할 수 있고 Phase 4a를 막지 않는다(D-042). 설계는 `design/MovementIntegration.md` "완전 비행 NPC"를 따른다. 실행 계획은 `phases/Phase03c_FlyingNpcFoundation.md`다.

- 2026-09-27 완료. PureEntity 비행 드론(3D local planner·LoS 게이트·교전 고도), 비행 적 총수 200·Pod 타입 분리(D-054).
- 동시 교전 비행 수는 원거리 슬롯 상한으로 묶인다. 비행 비용은 대부분 엔티티당 공통 비용이라 Phase 4 캐시의 절감 대상이 아니다.
- ActorPromoted 비행 엘리트는 후속 작업이다(`LNPFlightSteering` 순수 함수를 재사용).

### Phase 4 공통 전제

2026-09-27 앞의 세 항목 완료(`history/Phase04b_Log.md`). 회귀 공간은 정적 fixture LVI와 동적 사례 맵으로 나눴고(D-056), 동굴 키트 greybox 모듈과 에셋 규약(`design/TerrainContract.md` §6), `Meadow_00` 동굴(위도 15°·방위 60°), 마커 로컬 공간 규약(`design/DynamicTerrain.md` §5)이 들어갔다. 마지막 항목은 해당 definition을 도입할 때 처리한다.

- 회귀 fixture를 옥탄트 내부로 옮기고 fixture LVI로 만들어 8 slot 통합 경로를 탄다. 이때 fixture 좌표를 기준 반지름 30,000cm로 다시 계산한다(D-046).
- Phase 3b의 greybox 섬 옥탄트에 동굴 키트 공동 모듈 하나와 통로를 추가한다. 아트 품질은 요구하지 않는다. 동굴 geometry의 좌표 성분이 int16 복제 캡 안에 드는지 확인한다. 동굴은 옥탄트 꼭짓점(좌표축) 부근을 피한다(`design/TerrainContract.md` §7).
- Conditional Patch의 데이터 개념과 마커 로컬 공간 규약을 설계 문서에 확정한다. 구현은 Phase 8이다.
- 서로 다른 slot mask를 가진 production definition을 도입한다면 현재 greedy 선택을 최대 고유 제약 할당으로 교체하고 seam compatibility를 후보 제약에 포함한다(D-043).

### Phase 6 재측정

2026-09-29 완료. Phase 3b와 같은 Development package 리슨 2P·`-nullrhi`·`-corelimit=4`·투사체 500 조건에서 병렬 exact-only 한계는 약 700마리(P95 16.17ms, 750은 17.44ms), cache-first 한계는 약 800마리(P95 16.46ms, 850은 17.44ms)였다. 700마리 cache-first의 grounded cache hit는 90.60%이고 query/frame P50은 1,524→902, exact CPU P95는 7.354→3.082ms, 프레임 P95는 16.17→14.65ms로 줄었다. 상세는 `history/Phase06_Log.md` 구현 단위 5를 따른다.

### Phase 7 내부 게이트

Phase 7은 한 번에 완료하려 하지 않고 두 개의 독립 게이트로 나눈다.

- **7a — Nav 데이터 기반**: Nav codec, tile 경계, static connected component, seam/portal, ReachabilityGroup과 `ConnectivityGraphVersion`, debug visualization
- **7b — 경로 실행**: chord heuristic 일반 A*, 다중 프레임 request lifecycle, revision 검증, path cache, waypoint following, PureEntity·Actor 전달

7a의 cooked load와 8-slot 연결성 검증이 끝나기 전에 7b의 scheduler/cache 형식을 고정하지 않는다.
7a 실행 계획과 고정 입력은 `phases/Phase07a_NavDataFoundation.md`, 7b는 `phases/Phase07b_PathExecution.md`를 따른다.
2026-09-30 구현 단위 1까지 완료했다. `DataVersion=5` Navigation/Traversal stream, local component, 동굴 Layer portal과 ordered seam endpoint를 세 SurfaceData에 결정론적으로 저장했다. 다음은 runtime load와 8-slot 조립이다.

### Phase 7c — 입체 지형 베이크 대응

건물·탑·계단·벽을 늘리고 높이를 다양하게 한 그래플 입체기동 지형을 받기 위한 베이커·Nav 수정이다. Support·Nav 구조는 그대로 쓴다. 설계와 콘텐츠 규칙은 `design/SurfaceBaking.md` "접힌 sheet 자동 분할", `design/GroundNavigation.md` portal 절, `design/TerrainContract.md` §6-1이 소유한다.

- 접힌 sheet 자동 분할(D-064): 나선 경사로·경사로로 이어진 여러 층을 한 메시로 만들어도 베이크한다. `SupportAtlasFoldedSheet` 자동화를 분할 성공 기준으로 바꾼다.
- component 쌍당 여러 portal(D-065): 간격·상한을 정하고 베이크 시간 영향을 잰다.
- 입체 지형 회귀 사례: 여러 층 건물(입구 2개 이상), 나선 경사로 탑, 경사로 충돌 계단을 회귀 fixture에 더하고 §6-1 계약(계단 충돌·문 폭, D-070)을 검증한다.
- 수용량 재측정: 좁은 실내 때문에 cache 적중률이 떨어지는 만큼 Phase 6 cache-first 한계(약 800)가 exact 전용 한계(약 700) 쪽으로 내려가는지 확인한다.
- 앵커: 설계 동선은 마커 배치를 쓰고 시드 랜덤 수를 줄인다(코드 변경 최소).
- 지하 대형 공동(D-071): 옥탄트 중심 근방 대형 공동 안의 복층 구조물, 긴 경사로 입구와 수직 통로 입구를 회귀 fixture에 더한다. 긴 경사로로 지각 적이 공동까지 추격하고 수직 통로는 Nav로 이어지지 않는지, 긴 경로의 A* 확장 수가 7b scheduler 예산 안인지 확인한다. 지각 입구 평면 절단을 입구 여러 개·수직 통로로 넓힌다.
- source별 Layer 해상도(D-072): `LNP.Surface.CoarseSupport` 태그와 Layer별 m을 넣고, 대형 공동 바닥의 payload와 지각 기준 오차를 잰다.
- 비행 headroom(D-073): Spawn 후보에 위쪽 exact 거리를 더하고, 비행 편성 Pod 할당이 기준을 지키는지 본다. 높은 공동 안에서 비행 적의 배회·교전을 확인한다.
- 실제 품질 입체 지형 콘텐츠 제작은 Phase 9 입력이다.

### Phase 13 — PureEntity 활동 대역과 구간 배회 복제

비행 NPC 대량화 검토에서 나왔지만 모든 Pod 귀속 PureEntity에 적용한다. 설계는 `design/MovementIntegration.md` "활동 대역"·"구간 배회 복제"를 따른다.

1. 측정: 비행 1기당 약 10~15us 공통 비용과 지상 개체의 비 query 비용을 Insights로 분해한다.
2. Pod 단위 휴면·배회·활성 대역과 깨움 조건(D-067). Pod에 귀속되지 않은 NPC는 항상 활성이다.
3. 구간 배회 복제(D-068): 지상·비행부터 적용하고, 벽 타기는 Phase 14에서 같은 구간 구조를 쓴다.
4. 재측정: Phase 6·3c와 같은 패키지 조건으로 지상·비행 총수 한계를 다시 재고 D-054를 새 결정으로 대체한다. 게스트 대역폭은 `../Guide_NetBandwidth.md`의 절제·사유별 계수 방법으로 잰다.

### Phase 14 — 벽 타기 NPC

거미형(다족 보행 드론) PureEntity다. 설계는 `design/MovementIntegration.md` "벽 타기 NPC"(D-066)를 따른다. Support·Nav·베이커와 무관하므로 Phase 3c처럼 병렬로 진행할 수 있다.

- 적 코드의 "Up = 구 중심 방향" 가정을 "Up = 엔티티 회전 Z축"으로 통일하는 리팩터를 먼저 한다. 지상·비행 회귀가 없어야 한다.
- wall-walker 이동, 표면 법선 복제, 3D 분리, 원거리 공격·LoS 게이트, 360° 인지, 재귀속, 구간 배회(같은 표면 안 짧은 구간·긴 정지).
- 개체 수는 측정 뒤 정한다(Phase 3c §3.8 매트릭스 방식).

### Phase 10 — 대규모 추격 경로 비교

수백 마리가 소수의 플레이어·Pod로 향하는 다대소 수요에 맞춰 두 방식의 비교 가능한 최소 기능 프로토타입을 만든다(D-044).

- 목표별 flow field: 플레이어·Pod 주변 반경 한정, 여러 프레임에 분할 갱신
- 계층형 A*: Cluster·Portal 기반

Phase 7의 일반 A* 캡처를 재생하는 동일 benchmark harness에서 두 방식의 최소 기능 프로토타입을 먼저 비교한다. CPU 시간, 메모리, 동적 변경 재계산 범위, 경로 품질의 채택 기준을 통과한 방식만 production 통합한다. 비교를 위해 두 방식을 모두 제품 수준으로 완성하지 않는다.

### Phase 5와 6 사이

Phase 5가 임시로 남긴 기본 지각 Layer 전용 `GetSurfacePoint` adapter와 legacy SurfaceCache는 Phase 6에서 제거했다. production 이동 소비자는 다층 Support snapshot 또는 exact만 사용한다.

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
- 비행 진형(리더만 복제하고 멤버를 추론, D-069)
- 벽 타기 NPC의 자발 점프·근접 공격
- 접힌 sheet를 해결하려고 콘텐츠 메시를 수동으로 쪼개거나 분할 툴을 만드는 방식(D-064)
- 수직 통로를 지상 Nav로 연결하는 것(지상 적의 수직 이동, D-071)
- 옥탄트 경계·꼭짓점 근처의 지하 공간(D-071)

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
- 건물·탑·계단이 있는 옥탄트가 수동 메시 분할 없이 베이크되고, 입구가 여럿인 건물을 지상 적이 여러 경로로 추격한다.
- 옥탄트 중심 근방 지하 대형 공동(복층 구조물, 긴 경사로·수직 통로 입구)이 베이크되고, 지상 적은 경사로로만 들어오며, 비행 적은 headroom이 충분한 공동에서만 활동한다.
- 플레이어에게서 먼 Pod 무리는 휴면하고, 배회 대역은 구간 복제로 움직이며, 활동 대역 도입 전후의 총수 한계 비교 자료가 있다.
- 벽 타기 NPC가 벽·천장·섬 밑면을 기어 다니며 원거리로 교전한다.
- 서버와 클라이언트의 초기화·데이터 버전·동적 요소 상태가 일치한다.
