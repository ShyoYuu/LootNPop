# 프로젝트 반복 및 의사결정 이력 (Iteration & Decision History)

이 문서는 프로젝트 개발 과정에서 시도된 다양한 기술적 접근과 그에 따른 결과, 기각 사유 및 교훈을 기록.

---

## [Case 01] 단일 볼륨 기반 전역 PCG 구체 생성 (Single-Volume PCG Sphere)

- **일자:** 2026-04-08
- **상태:** **기각 (Dismissed)**

### 구현 방식

단일 `PCG Volume` 내에서 커스텀 C++ 노드(`UPCGSphereWorldSettings`)를 사용하여 피보나치 분포로 전체 구체 포인트를 생성. `Radius` 값에 따라 동적으로 전체 타일을 배치하고 노이즈 및 경사면 정렬 수행.

### 실패 현상

- **스케일링 문제:** 반지름 10,000cm까지는 에디터에서 원활했으나, 실질적인 월드 크기인 25,000cm 이상으로 확장 시 시스템이 프리징.
- **자원 고갈:** 단일 PCG 컴포넌트가 수백만 개의 포인트를 한 번에 계산(Fibonacci + Noise + Alignment)하면서 CPU 및 메모리 점유율이 임계치를 초과.

### 기각 사유

1. **생성 병목:** HISM은 렌더링을 최적화할 뿐, 생성 단계의 CPU 계산 부하는 절감 불가.
2. **파티셔닝 미흡:** 단일 거대 볼륨 방식은 World Partition 그리드와 효율적으로 연동되지 않아, 플레이어 근처가 아닌 구체 전체를 매번 계산 시도.
3. **편집 편의성 저하:** 전체 월드를 한 번에 로드해야 하므로 에디터 성능이 극도로 저하.

### 교훈 및 채택한 대안

- **분할 정복:** 거대 지형은 반드시 조각(Octant)으로 나누어 관리. → `ULNPOctantSpawnSubsystem`
- **정적/동적 하이브리드:** Geometry Script로 기본 지각 형성 후, PCG를 상세 요소에만 적용.
- **Baking:** 최종 확정된 지형 데이터를 HISM으로 정적 변환하여 런타임 계산 제거.

---

## [Case 02] NavMesh 기반 구형 지형 탐색

- **일자:** 2026-05-01 (설계 검토 단계)
- **상태:** **기각 (Dismissed)**

### 구현 방식 (검토)

Unreal의 표준 `NavMesh` 및 `UNavigationSystem`을 이용하여 적 NPC의 경로 탐색과 목적지 계산을 처리하는 방식.

### 기각 사유

1. **Z축 고정 설계:** NavMesh는 전적으로 Z축이 Up인 평면 지형을 전제로 설계됨. Dyson Sphere 내벽처럼 Up 방향이 위치마다 달라지는 구형 환경에서는 올바른 Reachable 영역과 경로 계산 불가.
2. **Mass 비호환:** `FMassProcessor::Execute()`는 워커 스레드에서 병렬 실행됨. `UNavigationSystem` API는 게임스레드 전용이어서 Mass 프로세서에서 직접 호출 불가.
3. **불필요한 복잡성:** 이 프로젝트의 적 NPC는 복잡한 경로 탐색보다 플레이어를 향한 직선 접근이 주 행동. NavMesh의 계산 비용이 실질적인 이득 대비 과도.

### 채택한 대안

- 경로 탐색 대신 **직선 접근 + 장애물 회피(Avoidance)**로 대체.
- 표면 접점 계산은 `ULNPSurfaceCacheSubsystem`의 O(1) 조회로 처리. 스레드 안전하며 게임 시작 전 한 번만 계산.

---

## [Case 03] Mass 프로세서 내 인라인 라인 트레이스

- **일자:** 2026-05-01 (설계 검토 단계)
- **상태:** **기각 (Dismissed)**

### 구현 방식 (검토)

`ULNPEnemyMovementProcessor::Execute()` 내에서 매 틱 `UWorld::LineTraceSingleByChannel()`을 호출하여 적 NPC의 발 아래 구형 표면 접점을 실시간으로 계산하는 방식.

### 기각 사유

1. **스레드 안전성 위반:** `UWorld::LineTraceSingleByChannel()`은 게임스레드 전용 API. `FMassProcessor::Execute()`는 워커 스레드에서 병렬 실행되므로, 여기서 직접 호출하면 레이스 컨디션 및 크래시 발생. `bAllowConcurrentExecution = false`로 게임스레드 강제 실행할 수 있으나, 이는 Mass 사용의 핵심 이점(병렬 처리) 포기.
2. **성능 비용:** 수백 개의 엔티티가 매 틱 라인 트레이스를 각각 발사하면 Chaos 물리 엔진에 과도한 부하 발생. 구형 지형에서 결과는 수학적으로 예측 가능하므로, 라인 트레이스는 낭비.

### 채택한 대안

**`ULNPSurfaceCacheSubsystem`** — 게임 시작 전 베이킹 단계에서 구형 내벽 표면 좌표를 등장방형(Equirectangular) 그리드로 사전 계산.

- `bBakingComplete = true` 이후 배열은 읽기 전용 → 멀티스레드 안전.
- 방향 벡터 → 격자 인덱스 변환(atan2/asin)으로 O(1) 조회.
- 라인 트레이스 대비 CPU 비용이 무시할 수준.

상세 구현은 `TechDesign_SurfaceCache.md` 참조.
