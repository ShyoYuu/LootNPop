# 검증·마이그레이션 계획

> 상태: 초안
> 읽기 조건: 소비자 전환, 테스트 맵, 성능 기준, 위험 또는 완료 조건을 다룰 때
> 최초 설계 원본: `../history/InitialPlan.md`

## Spawn·PCG·World Device 전환

### Mass Spawn

현재의 임의 방향→단일 표면 투영을 region-aware candidate 방식으로 바꾼다.

- Support Layer별 spawn weight
- slope·edge·capsule clearance
- StaticNavComponent ID
- Pod 허용 여부
- Enemy 허용 여부
- 동굴/부유섬 인구 예산

Pod와 Enemy에 초기 `SurfaceHandle`을 부여한다(Phase 5). `NavNodeRef`와 StaticNavComponent는 Nav 데이터가 생기는 Phase 7에서 추가한다.

### PCG

- 지각과 부유섬을 별도 support source로 처리
- 정적 PCG 프랍은 Nav bake 전에 확정
- 장식 프랍은 Support와 Mass collision에서 제외
- blocker 프랍은 agent radius만큼 Nav occupancy에 반영
- runtime PCG가 walkable terrain 자체를 생성하는 기능은 제외

### World Device

World Device는 두 source를 가진다(`DynamicTerrain.md` §1). 수동 Placement Marker는 레벨 디자인 의도대로 배치하고, seed 기반 절차 배치는 지금처럼 유지하되 두 source가 같은 서버 스폰 함수를 거친다.

절차 배치에서는 중심→외부 첫 hit에 의존하는 방식을 제거한다.

- 허용 Support Layer 명시
- edge·slope·clearance 사용
- 동굴/섬별 배치 정책
- exact collision으로 주변 여유 공간 확인

---

## 테스트 계획

모든 기능 테스트는 `/Game/Maps/SurfaceNavigation/L_SurfaceRegression`과 `RegressionMap.md`의 고정 probe·oracle을 공통 기준으로 사용한다. Phase별 구현은 새 전용 맵을 복제하지 않고 같은 사례에 측정 항목을 추가한다.

### 단위 테스트

- 방향→Atlas coordinate 변환
- slot rotation round trip
- radius quantization 오차
- normal packing 오차
- coverage boundary
- 동일 방향 여러 Layer 선택
- PreferredSurface 우선 규칙
- step/drop 제한
- Nav node encoding/decoding
- tile 경계 neighbor
- local revision 검증
- earliest hit time 비교
- exact hit identity의 face/instance→Layer 변환
- A* chord heuristic의 admissibility
- multi-frame path request의 snapshot/connectivity/revision 취소
- 겹친 Conditional Patch의 occupancy reference count

### 에디터 베이크 테스트

- source hash 안정성
- 변경 후 stale 검출
- 같은 asset 반복 베이크 결과 결정론
- seam 오차
- cave floor 분리
- island footprint coverage
- static prop dilation
- connected component
- portal 자동 생성
- Support proxy와 exact counterpart coverage·오차
- collision profile 설정·marker authoring 변경 후 stale 검출

### 기능 테스트 맵

| 시나리오 | 기대 결과 |
|:---|:---|
| 같은 방향의 기본 지각+섬 | 현재 Layer를 유지하며 올바른 지면 조회 |
| 섬 가장자리 | 유령 보간 없이 exact/airborne 전환 |
| 겹친 섬 둘 | 위치와 PreferredSurface에 맞는 Layer 선택 |
| 동굴 입구 | 외부↔동굴 Walk 경로 생성 |
| 동굴 천장 | NPC는 바닥 유지, 투사체는 천장 충돌 |
| 벽 뒤 적 | 투사체가 벽을 먼저 맞음 |
| 나무·바위 | direct path 실패 후 우회 |
| 끊긴 부유섬 | 근접 추격 불가, 원거리 사격 가능 |
| 쓰러지는 기둥 | 정지 전 link 없음, 정지 후 경로 생성 |
| 움직이는 패널 | 우연히 착지한 NPC가 함께 이동 |
| 다른 섬으로 넉백 | 착지 후 가까운 reachable Pod로 재귀속 |
| 파괴된 바닥 | Conditional Patch 비활성화 후 NPC가 진입하지 않음 |
| 옥탄트 회전 | 모든 slot에서 Support/Nav가 mesh와 일치 |
| 멀티플레이 | server/client 초기화, asset 조합, 동적 요소 상태 일치 |
| 패널 위 플레이어 | 2P에서 Mover가 복제된 패널을 base로 인식하고 예측이 어긋나지 않음 |
| 패널 late join | 이동 중 접속한 클라이언트가 path revision·server epoch로 같은 자세를 복원 |
| 한 component의 분리된 sheet | face/instance identity로 서로 다른 Layer를 정확히 resolve |
| 내부형 double-sided shell | exact normal이 지역 Up 기준 walkable 판정과 일치 |
| 요청 중 revision 변경 | 이전 snapshot의 다중 프레임 경로를 폐기·재시작 |
| 겹친 Conditional Patch | 한 patch 비활성화가 다른 활성 blocker/support를 지우지 않음 |

모든 Phase 완료 조건에 `-game` 리슨 서버 2P 스모크를 포함한다(D-031). PIE는 월드 서브시스템 초기화 시점의 net mode를 재현하지 못하므로 대체할 수 없다.

### 성능 측정

현재 기준:

- runtime bake sample: 1,233,235
- runtime bake 시간: 약 7초. 계산 비용이 아니라 트레이스 발사를 프레임당 3000개로 제한해 약 412프레임에 나눈 시간이다
- 현재 sample 메모리: 약 37.6MiB

신규 측정:

- SurfaceData cooked asset 크기, decode peak와 runtime resident memory
- initial data load/validation 시간
- cache high-confidence hit 비율
- exact fallback 비율
- query 종류별 count/time
- physics scene read-lock 대기
- Enemy movement P50/P95
- 일반 A* expanded node/P50/P95
- path cache hit rate
- 동적 변경 dirty cell/Tile/Cluster 수
- exact 전용(Phase 3b)과 캐시 도입 후(Phase 6)의 적 수 한계치·exact 호출 비율
- 관찰 거리 구간별 exact 호출 수
- 일반 A*·flow field·계층형 탐색의 동일 시나리오 비교

초기 목표:

- 정상 실행의 runtime surface bake 제거
- 초기화 시간: 현재 runtime bake 약 7초를 SurfaceData 로드·게시 시간으로 대체하고 그 값을 기록
- 정적 smooth interior의 대다수가 exact query 없이 처리
- correctness-mandatory query는 예산 초과를 이유로 생략하지 않음

초기 메모리 추정은 하한값으로만 사용한다. sparse index, coverage, Atlas metadata, decoded buffer, 다층과 overlay를 포함한 peak/resident memory를 Phase 4·5에서 측정하며, 그 전에는 Support·Nav 전체가 10MB 안쪽이라고 가정하지 않는다(`DataModel.md`).

CPU의 절대 합격값은 Phase 3의 부하 시나리오(적 수와 CombatMode 비율, 동시 투사체 수 고정) 기준 캡처를 만든 뒤 확정한다.

---

## 마이그레이션 전략

### 한 번에 모든 소비자를 바꾸지 않음

전환 중에는 새 snapshot 위에 기존 API 호환 adapter를 둘 수 있다.

```text
새 QuerySupport
    ↓
Legacy GetSurfacePoint adapter
```

단, adapter는 기본 지각 Layer만 반환하고 다층 환경에서 사용하면 경고하도록 한다. 새 기능이 legacy API에 의존해 출시되지 않게 한다.

### 소비자 전환 순서

| 순서 | 소비자 | Phase |
|:---:|:---|:---:|
| 0 | production `LNPWorldExact` response audit·마이그레이션과 hit identity registry | 3 Gate -1 |
| 1 | 투사체 서버 판정·클라이언트 ghost의 exact world collision | 3 |
| 1 | 탄도 가이드 `PredictArc` (투사체와 같은 판정 함수 불변식) | 3 |
| 2 | World Device 절차 배치 → 서버 스폰 경로 통합, Placement Marker 스폰 | 3 |
| 2 | PureEntity exact 접지·낙하·넉백 (프로토타입, CVar 전환, Phase 6 exact 폴백으로 재사용) | 3b |
| 2 | 완전 비행 NPC (exact sweep 기반, 신규 소비자) | 3c |
| 3 | Editor SupportData와 runtime query, legacy adapter | 4a·4b·5 |
| 4 | Mass Spawn (Spawn stream) | 5 |
| 5 | Enemy grounded 이동·airborne/landing, Actor 경로 LOD 전환 | 6 |
| 6 | Idle 배회 | 6 |
| 7 | legacy SurfaceCache·adapter 제거 | 6 |
| 8 | Nav Grid, path following, Pod 재귀속, 슬롯 도달성 | 7 |
| 9 | Conditional Patch와 파괴 overlay | 8 |

PCG는 에디터 시점에 실행되고 결과가 LVI에 저장되므로 런타임 전환 대상이 아니다. 베이크 전에 PCG 결과가 확정돼 있으면 된다.

production Terrain Contract Component Tag 전환은 Phase 4까지 나눠 진행할 수 있지만 exact response 전환보다 늦어서는 안 되는 항목과 분리한다. Phase 3 동안 신규 exact 경로를 기본값으로 만들기 전 production 옥탄트 8-slot trace/sweep 회귀를 통과해야 한다(D-036).

### production 8-slot exact oracle

콘솔 `LNP.SurfaceNav.ExactOracle`(`SurfaceNavigation/LNPExactOracle.cpp`). 옥탄트 생성·registry 게시·SurfaceCache 베이크를 기다렸다 실행하므로 `-ExecCmds`로 시작 시 걸 수 있다. `-game` 클라이언트는 travel 뒤 월드로 옮겨 실행한다.

| 항목 | 내용 |
|:---|:---|
| 방향 | 로컬 Fibonacci 4,096개 × slot 회전 8개, 그리고 좌표 평면 3개 위 0.25° 간격에 평면 위·±0.0001°·±0.001°·±0.01°(기준 반지름 25,000cm에서 0.04·0.4·4.4cm) |
| query | 반지름 `SphereRadius×0.5`에서 `envelope+500cm`까지 line, sphere(30), 방사 축 capsule(34/88). `DebugValidation` 분류, ParallelFor worker 실행 |
| 실패 | Miss, Unknown, StartPenetrating, ShapeOrder(sweep이 line보다 늦음), ExactDeeper(line hit가 legacy 표면보다 200cm 넘게 바깥), SlotMismatch(모든 slot이 같은 Level일 때 같은 로컬 방향 8개의 hit 거리 차 > 1cm) |
| 비교 제외 | SlotMismatch는 persistent level 런타임 source(런처·앵커·LootPod proxy)를 맞힌 방향을 건너뛴다. seed 배치라 회전 대칭이 아니다 |
| 정보 | EdgeMiss — 좌표 평면 위에 **정확히** 놓인 line이 빠지고 같은 방향 sphere는 맞는 경우. 평면에서 0.04cm만 떨어져도 맞으면 틈이 아니라 두 body 공유 모서리의 측도 0 경우다. 실제 투사체에서는 envelope 안전망이 받는다 |

통과 기록은 `../history/Phase03_Log.md` 2026-09-24 "8-slot oracle" 절.

### 제거 대상

- 머신별 123만 async trace 베이크
- 등장방형 전역 단일 배열
- `GetSurfacePoint(Direction)` 직접 사용
- `IsUnderSurface` — Phase 3에서 제거 완료
- 반지름 비교 기반 공중 착지
- `ECC_WorldStatic`을 곧바로 지면 의미로 사용하는 코드

---

## 주요 위험과 완화

### Mesh Terrain 실험 기능 의존

- 완화: authoring-only 사용, 독립 Static Mesh 산출물 commit
- 폴백: Modeling Tools/Geometry Script

### stale SurfaceData

- 완화: cook·CI에서 source manifest와 header hash를 비교해 차단(D-029). collision profile 정의는 Terrain Contract/baker schema version에 반영하고, Conditional Patch 도입 시 marker authoring 전체를 hash한다(D-041). 런타임은 `DataVersion`만 확인

### 옥탄트 seam 불일치

- 완화: seam signature, 자동 비교, seam risk band exact fallback

### 동굴 바닥 분리 실패

- 완화: 공동 모듈·통로 바닥을 별도 `Support` 컴포넌트로 분리(D-035), 모듈 제작 시 1회 검증

### Support와 Chaos 불일치

- 완화: 베이크 후 샘플 검증 trace, 오차 통계, high-risk exact fallback

### scene query 경합

- 완화: 전용 channel, 쿼리 분류, interior cache, 관찰 거리 축(D-025), Insights 계측. query CPU 비용과 락 대기를 따로 측정

### Nav Grid가 좁은 통로 삭제

- 완화: 동굴별 해상도, clearance-aware bake, 명시적 corridor

### 계층형 사전 비용의 동적 무효화

- 완화: cluster-local revision, dirty cluster 재계산, 저수준 폴백

### 움직이는 패널에서 NPC 이탈

- 완화: local contact, transform delta, exact contact 검증, 이탈 시 velocity 상속, tick prerequisite, late-join과 server-time 보정 테스트

### 동적 요소의 네트워크 불일치

- 원인 후보: LVI 내부 Actor의 경로 불일치, 비동기 물리의 보간 자세, Mover base 참조 누락
- 완화: 서버 스폰 복제 Actor(D-026), 결정론적 kinematic 움직임(D-027), Phase 3 2P 패널 탑승 스파이크

### 넉백 후 Pod 부재

- 완화: Orphaned 상태와 비가시 재삽입 정책

### 경로 요청 폭증

- 완화: direct-path 우선, stuck-triggered request, cache, request budget, 추후 hierarchy/flow field

---

## 전체 완료 정의

다음 조건을 모두 만족하면 본 계획의 핵심 목표가 완료된 것으로 본다.

- 랜덤 옥탄트 조합에서 runtime surface bake가 없음
- 기본 지각·부유섬·동굴 키트의 SupportData가 에디터에서 베이크됨
- exact 전용 대비 캐시 도입 후의 적 수 한계치 비교 자료 확보
- Mass worker가 immutable snapshot을 안전하게 조회
- 부유섬 가장자리와 동굴에서 유령 보간·반지름 매몰이 없음
- 투사체가 world와 Mass target 중 실제 첫 충돌을 선택
- Enemy가 부유섬·동굴에서 접지·낙하·착지
- 다른 NavComponent로 날아간 Enemy가 가까운 reachable Pod로 재귀속
- 나무·바위·절벽을 제한적으로 우회하는 별도 Nav Grid A* 동작
- 실제 Walk 연결이 있는 부유섬만 추격
- 연결되지 않은 섬의 원거리 NPC가 LoS/사거리 조건에서 사격
- 쓰러진 기둥이 정지한 뒤 새 길이 활성화
- 움직이는 패널이 우연히 착지한 NPC를 운반
- 파괴가 기존 길을 열거나 닫을 수 있음
- 일반 A*·flow field·계층형 탐색의 성능 비교 자료 확보
- server/client 초기화와 데이터 버전이 일치

---
