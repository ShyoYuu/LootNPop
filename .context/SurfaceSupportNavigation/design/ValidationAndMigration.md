# 검증·마이그레이션 계획

> 상태: 초안
> 읽기 조건: 소비자 전환, 테스트 맵, 성능 기준, 위험 또는 완료 조건을 다룰 때
> 최초 설계 원본: `../history/InitialPlan.md`

## Spawn·PCG·World Device 전환

### Mass Spawn

현재의 임의 방향→단일 표면 투영을 region-aware candidate 방식으로 바꾼다.

- Support Layer별 spawn weight
- slope·edge·capsule clearance
- NavComponent ID
- Pod 허용 여부
- Enemy 허용 여부
- 동굴/부유섬 인구 예산

Pod와 Enemy에 초기 `SurfaceHandle`, `NavNodeRef`, `NavComponent`를 부여한다.

### PCG

- 지각과 부유섬을 별도 support source로 처리
- 정적 PCG 프랍은 Nav bake 전에 확정
- 장식 프랍은 Support와 Mass collision에서 제외
- blocker 프랍은 agent radius만큼 Nav occupancy에 반영
- runtime PCG가 walkable terrain 자체를 생성하는 기능은 제외

### World Device

중심→외부 첫 hit에 의존하는 배치를 제거한다.

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
| 파괴된 바닥 | cell 무효화 후 NPC가 진입하지 않음 |
| 옥탄트 회전 | 모든 slot에서 Support/Nav가 mesh와 일치 |
| 멀티플레이 | server/client 초기화와 asset 조합 일치 |

### 성능 측정

현재 기준:

- runtime bake sample: 1,233,235
- runtime bake 시간: 약 7초
- 현재 sample 메모리: 약 37.6MiB

신규 측정:

- SurfaceData asset 크기와 runtime resident memory
- initial data load/validation 시간
- cache high-confidence hit 비율
- exact fallback 비율
- query 종류별 count/time
- physics scene read-lock 대기
- Enemy movement P50/P95
- 일반 A* expanded node/P50/P95
- path cache hit rate
- 동적 변경 dirty cell/Tile/Cluster 수
- 계층형 도입 전후 동일 경로 비교

초기 목표:

- 정상 실행의 runtime surface bake 제거
- 실제 부유섬 콘텐츠를 포함한 SurfaceData resident memory가 현재 약 38MiB 범위 안에 들어오도록 시도
- 정적 smooth interior의 대다수가 exact query 없이 처리
- correctness-mandatory query는 예산 초과를 이유로 생략하지 않음

CPU의 절대 합격값은 목표 플랫폼과 최대 Enemy/Projectile 수의 기준 캡처를 만든 뒤 확정한다.

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

1. 투사체 exact world collision
2. Editor SupportData와 runtime query
3. Mass Spawn
4. Enemy grounded 이동
5. Enemy airborne/landing
6. Idle 배회
7. PCG/World Device
8. Nav Grid와 path following
9. legacy SurfaceCache 제거

### 제거 대상

- 머신별 123만 async trace 베이크
- 등장방형 전역 단일 배열
- `GetSurfacePoint(Direction)` 직접 사용
- `IsUnderSurface`
- 반지름 비교 기반 공중 착지
- `ECC_WorldStatic`을 곧바로 지면 의미로 사용하는 코드

---

## 주요 위험과 완화

### Mesh Terrain 실험 기능 의존

- 완화: authoring-only 사용, 독립 Static Mesh 산출물 commit
- 폴백: Modeling Tools/Geometry Script

### stale SurfaceData

- 완화: source hash, version, cook validation

### 옥탄트 seam 불일치

- 완화: seam signature, 자동 비교, seam risk band exact fallback

### 동굴 floor 분리 실패

- 완화: Support Proxy/attribute authoring, triangle connectivity 검사

### Support와 Chaos 불일치

- 완화: 베이크 후 샘플 검증 trace, 오차 통계, high-risk exact fallback

### scene query 경합

- 완화: 전용 channel, 쿼리 분류, interior cache, Insights 계측

### Nav Grid가 좁은 통로 삭제

- 완화: 동굴별 해상도, clearance-aware bake, 명시적 corridor

### 계층형 사전 비용의 동적 무효화

- 완화: cluster-local revision, dirty cluster 재계산, 저수준 폴백

### 움직이는 패널에서 NPC 이탈

- 완화: local contact, transform delta, exact contact 검증, 이탈 시 velocity 상속

### 넉백 후 Pod 부재

- 완화: Orphaned 상태와 비가시 재삽입 정책

### 경로 요청 폭증

- 완화: direct-path 우선, stuck-triggered request, cache, request budget, 추후 hierarchy/flow field

---

## 전체 완료 정의

다음 조건을 모두 만족하면 본 계획의 핵심 목표가 완료된 것으로 본다.

- 랜덤 옥탄트 조합에서 runtime surface bake가 없음
- 기본 지각·부유섬·단순 동굴의 SupportData가 에디터에서 베이크됨
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
- 일반 A*와 계층형 탐색의 성능 비교 자료 확보
- server/client 초기화와 데이터 버전이 일치

---
