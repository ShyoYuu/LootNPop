# Phase 6 — Enemy 접지·공중·넉백 전환

> 상태: 완료(2026-09-29) — 구현 단위 0~5와 종료 검증 완료
> 예상 범위: 3~4세션
> 선행 조건: Phase 5 런타임 로더와 SurfaceCache 교체(완료)

## 1. 목표

PureEntity와 ActorPromoted Enemy가 다층 Support 환경에서 같은 지면 정체성을 유지하도록 이동 소비자를 전환한다.

- PureEntity의 평상시 접지는 immutable Support snapshot을 우선 사용한다.
- 캐시가 확신하지 못하는 구간과 상태 전환은 Phase 3b exact 경로를 사용한다.
- 낙하·착지·넉백·사망 팝·LOD 전환에서 `FLNPSurfaceHandle`을 잃거나 다른 Layer로 임의 스냅하지 않는다.
- 움직이는 패널은 계획 경로가 아니라 우연한 착지와 운반만 지원한다(D-013).
- 배회 목표가 현재 Support Layer를 유지한다.
- Phase 3b와 같은 부하 시나리오에서 exact 호출 비율과 적 수 한계치를 다시 측정한다.
- 전환 완료 뒤 legacy `GetSurfacePoint` adapter와 `ULNPSurfaceCacheSubsystem`을 제거한다.

Phase 7 소유인 Nav Grid, path following, StaticNavComponent, Pod 재귀속과 슬롯 도달성은 구현하지 않는다. Phase 6에서 다른 Layer에 착지한 Enemy도 기존 Parent Pod를 유지한다.

## 2. 현재 코드 기준선

- `ULNPEnemyMovementProcessor`가 서버의 지상 PureEntity 이동과 넉백·사망 팝 공중 적분을 소유한다.
- `LNPEnemyExactMovement::StepGrounded`는 수평 capsule sweep과 하향 support probe를 모두 exact로 수행한다.
- `StepAirborne`은 이전 위치에서 제안 위치까지 exact capsule sweep을 수행하고 walkable Support에 착지한다.
- `FLNPEnemyFragment::SurfaceHandle`은 Phase 5 초기 스폰 때 채워지지만 이동 중에는 갱신되지 않는다.
- `ULNPSurfaceDataSubsystem::QuerySupport`는 다층 Atlas와 선호 Layer를 사용해 `HighConfidence`, `NeedsExact`, `NoSupport`, `OutsideCoverage`, `NotReady`를 반환한다.
- ActorPromoted 경로는 Mover가 exact floor를 소유하지만 Actor→Entity 전환 때 floor identity를 `SurfaceHandle`로 되돌리는 경로가 없다.
- Idle 배회는 exact `ProjectToSameLayer`를 매 목표 추첨마다 사용한다.

## 3. 확정 구현 규약

### 3.1 PureEntity grounded

한 프레임의 순서는 다음과 같다.

1. 이동량이 있으면 기존 수평 capsule sweep을 수행한다. Support Atlas는 벽·프랍·동적 blocker를 표현하지 않으므로 이 sweep은 캐시 적중과 무관하게 유지한다.
2. 도달한 캡슐 중심에서 발 접점(`Center - Up * CapsuleHalfHeight`)을 만들고 `QuerySupport`를 호출한다.
3. 현재 `SurfaceHandle`을 `PreferredSurface`로 전달한다.
4. `HighConfidence`면 Atlas 지면점 위에 캡슐 중심을 놓고 반환된 handle을 기록한다. 이 프레임에는 exact floor probe를 하지 않는다.
5. `NeedsExact`, `NoSupport`, `OutsideCoverage`, `NotReady`면 같은 도달점에서 Phase 3b 하향 probe를 수행한다. `NoSupport`도 곧장 낙하로 간주하지 않는다. 캐시에 없는 동적 Support와 정확성 필수 낙하 전환을 확인해야 하기 때문이다.
6. exact Support hit의 face 표가 유효하면 `(slot, LocalLayerId, SurfaceDataGeneration)`으로 handle을 기록한다.
7. `LostSupport`면 handle을 무효화하고 airborne으로 전환한다. `Rejected`면 직전 위치와 직전 handle을 유지한다.

지면 캐시가 대체하는 것은 하향 support probe다. 수평 blocker sweep까지 없애는 최적화는 Nav occupancy가 생기는 Phase 7 전에는 하지 않는다.

### 3.2 airborne·착지·넉백·사망 팝

- 공중 이동은 항상 `StepAirborne` exact sweep을 사용한다(D-049).
- walkable Support 착지 때 exact identity로 `SurfaceHandle`을 기록한다.
- 정적 Support인데 face→Layer identity가 없으면 착지 자체를 임의 Layer로 승격하지 않는다. 현재 registry 게시 계약상 production 정적 source에서는 오류이며 진단 대상으로 센다.
- DynamicSupport 착지는 static handle과 별도의 contact 상태를 사용한다. 패널 delta 적용·이탈 속도 상속은 구현 단위 3에서 묶는다.
- 넉백과 사망 팝은 같은 airborne 적분을 계속 공유한다.

### 3.3 ActorPromoted와 LOD 전환

- Entity→Actor: 기존 `SurfaceHandle`을 유지하고 Mover가 floor를 다시 찾는다.
- Actor가 활성인 동안 지면 판정은 Mover가 소유한다.
- Actor→Entity: Mover의 현재 floor hit identity를 registry로 해석해 `SurfaceHandle`을 기록한다. 공중이면 handle을 무효화하고 Mover 속도를 `FLNPEnemyVelocityFragment`에 넘긴다.
- 게스트의 복제 Actor는 Mover 재시뮬레이션만 수행하며 서버용 handle 전환을 실행하지 않는다.

### 3.4 Idle 배회

- 지상 Enemy의 목표 후보는 현재 위치와 같은 `SurfaceHandle`을 선호해 `QuerySupport`한다.
- `HighConfidence`가 아니면 기존 exact `ProjectToSameLayer`로 검증한다.
- 후보가 현재 Layer를 벗어나거나 지면이 없으면 제자리에 두고 다음 추첨을 기다린다.
- Nav 기반 reachable 후보와 edge clearance는 Phase 7 소유다.

### 3.5 측정 모드

Phase 3b exact-only 기준과 직접 비교할 수 있도록 한 빌드에서 다음 두 모드를 선택할 수 있어야 한다.

- exact-only: grounded 하향 probe를 매 프레임 exact로 수행
- cache-first: `HighConfidence`면 캐시, 나머지는 exact

모드별로 cache high-confidence 수, exact grounded fallback 수, airborne mandatory 수와 전체 Enemy movement 시간을 기록한다. legacy Layer 0 경로는 비교 모드로 유지하지 않는다.

## 4. 구현 단위

### 구현 단위 0 — 실행 경계와 회귀 입력

- [x] Phase 6 실행 문서 작성
- [x] PureEntity·ActorPromoted·Idle 소비자 목록 확정
- [x] exact-only/cache-first 전환 CVar와 계측 계약 추가
- [x] exact hit identity→`FLNPSurfaceHandle` 변환 단위 테스트
- [x] 다층 cache-first 의사결정 단위 테스트

완료 조건: 실제 이동 프로세서를 바꾸기 전에 캐시 적중·exact 폴백·Layer handle 갱신을 자동화로 고정한다.

### 구현 단위 1 — PureEntity grounded cache-first

- [x] 수평 sweep과 하향 support 판정을 분리해 중복 exact query 없이 조합한다.
- [x] `HighConfidence` 접지와 모든 비확신 exact 폴백을 구현한다.
- [x] 접지 유지·절벽 낙하·가파른 경사·정적 blocker·섬 가장자리·동굴 바닥을 회귀 검사한다.
- [x] 초기 스폰 handle이 cache hit와 exact fallback에서 유지·갱신되는 단위 경계를 검사한다.

완료 조건: smooth interior의 grounded 하향 exact probe가 사라지고 회귀 fixture의 결과가 Phase 3b와 같다.

### 구현 단위 2 — airborne·착지·넉백과 배회

- [x] exact 착지 결과에서 static Layer handle을 기록한다.
- [x] 넉백·사망 팝에서 handle 무효화와 재획득을 검사한다.
- [x] Idle 배회를 다층 query 우선·exact 폴백으로 전환한다.
- [x] 지각 아래·부유섬·동굴에서 다른 Layer로 목표가 튀지 않는지 검사한다.

완료 조건: 낙하와 착지가 반지름 비교 없이 exact이며 배회 목표가 현재 Layer를 유지한다.

### 구현 단위 3 — Actor LOD와 움직이는 패널

- [x] Actor→Entity Mover floor identity·공중 속도 인계를 구현한다.
- [x] Entity→Actor→Entity 왕복 시 위치·속도·handle 연속성을 검사한다.
- [x] DynamicSupport contact, 패널 transform delta, 이탈 속도 상속을 구현한다.
- [x] 패널 tick prerequisite와 2P late join 자세를 회귀 검사한다.

완료 조건: PureEntity·ActorPromoted 모두 패널 착지와 LOD 전환에서 순간이동하거나 Layer를 잃지 않는다.

### 구현 단위 4 — legacy 제거

- [x] `GetSurfacePoint` 직접 사용을 모두 제거한다.
- [x] `ULNPSurfaceCacheSubsystem`과 runtime bake 잔여 코드를 제거한다.
- [x] legacy CVar·경로·문서 주석을 정리한다.
- [x] 전체 자동화, 에디터 빌드, `-game` 리슨 2P 스모크를 수행한다.

완료 조건: 검색과 실행 로그에 legacy SurfaceCache 소비·베이크가 없고 모든 지상 소비자가 다층 API 또는 exact를 사용한다.

### 구현 단위 5 — 부하 재측정과 종료

- [x] Phase 3b와 같은 Development package `-nullrhi` 시나리오를 exact-only/cache-first로 실행한다.
- [x] 병렬 약 700마리 exact 기준과 비교해 한계치·query 비율·프레임 P50/P95를 기록한다.
- [x] 1P와 리슨 2P에서 지각·섬·동굴·패널·LOD 전환을 스모크한다.
- [x] `Current.md`, `Roadmap.md`, `history/Phase06_Log.md`를 종료 상태로 갱신한다.

## 5. 전체 완료 조건

- PureEntity가 지각·부유섬·동굴에서 캐시 우선으로 접지하고 비확신 구간만 exact를 사용한다.
- 절벽 낙하와 공중 착지는 exact이며 유령 보간이 없다.
- 넉백·사망 팝 뒤 착지 Layer handle이 정확하다.
- ActorPromoted LOD 왕복에서 위치·속도·Surface handle이 이어진다.
- 움직이는 패널에 우연히 착지한 Enemy가 함께 움직이고 이탈 속도를 상속한다.
- Idle 배회가 현재 Layer를 유지한다.
- legacy `GetSurfacePoint` adapter와 `ULNPSurfaceCacheSubsystem`이 제거된다.
- Phase 3b exact-only 대비 cache-first의 exact 호출 비율과 적 수 한계치 비교 자료가 있다.
- 전체 자동화, `LootNPopEditor Win64 Development`, Development package, `-game` 리슨 서버 2P 스모크가 통과한다.
