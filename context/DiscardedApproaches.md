# 기각된 기술 접근법 (Discarded Approaches)

개발 과정에서 시도했으나 실패하거나 기각된 접근법을 기록. 같은 실수를 반복하지 않기 위한 의사결정 근거 보관소.

---

## [Case 01] 단일 볼륨 기반 전역 PCG 구체 생성 (Single-Volume PCG Sphere)

- **일자:** 2026-04-08
- **상태:** **기각 (Dismissed)**

### 구현 방식

단일 `PCG Volume` 내에서 커스텀 C++ 노드(`UPCGSphereWorldSettings`)를 사용하여 `SM_Plane` Static Mesh를 피보나치 분포로 전체 구체 포인트에 생성. `Radius` 값에 따라 전체 타일을 동적으로 배치하고, Perlin Noise 및 경사면 정렬을 포함한 모든 지형 계산을 PCG 노드로 처리.

### 실패 현상

- **스케일링 문제:** `PCG Volume` 크기를 1×1×1로 설정해 반지름 10,000cm 구체를 Low LOD(HISM)로 생성하면 에디터 플레이까지는 성공. 그러나 HLOD 베이킹을 위해 볼륨 크기를 키워 High LOD로 생성하면 메모리 부족으로 실패.
- **자원 고갈:** 단일 PCG 컴포넌트가 Fibonacci 분포, Noise, 경사면 정렬을 수백만 포인트에 한 번에 적용하면서 CPU·메모리 점유율이 임계치를 초과.

### 기각 사유

1. **생성 병목:** HISM은 렌더링을 최적화할 뿐, 생성 단계의 CPU 계산 부하는 절감 불가.
2. **파티셔닝 미흡:** 단일 거대 볼륨 방식은 World Partition HLOD 베이킹이 현실적인 시간 안에 불가능.
3. **편집 편의성 저하:** 전체 월드가 생성된 상태에서는 에디터 뷰포트 확인이 사실상 불가능.

### 채택한 대안

- **정적/동적 하이브리드:** Geometry Script로 기본 지형 Mesh를 생성하고, PCG는 배경 프랍에만 적용.
- **Baking:** 최종 확정된 지형 Actor(Static Mesh + PCG)를 Level Instance로 미리 생성해두고, 런타임에는 스폰만 수행.

---

## [Case 02] SphereWorld 상에서의 NPC 이동

- **일자:** 2026-05-01 (설계 검토 단계)
- **상태:** **기각 (Dismissed)**

### 구현 방식 (검토)

- **지형 충돌:** `ULNPEnemyMovementProcessor::Execute()` 내에서 매 틱 `UWorld::LineTraceSingleByChannel()`을 호출하여 NPC 발 아래 구형 표면 접점을 실시간 계산.
- **길찾기:** Unreal 표준 `NavMesh` 및 `UNavigationSystem` 활용.

### 기각 사유

1. **스레드 안전성 위반:** `UWorld::LineTraceSingleByChannel()`은 게임스레드 전용 API. `FMassProcessor::Execute()`는 워커 스레드에서 병렬 실행되므로 직접 호출 시 레이스 컨디션 및 크래시 발생. `bAllowConcurrentExecution = false`로 게임스레드 강제 실행이 가능하나, 이는 Mass의 핵심 이점인 병렬 처리를 포기하는 것.
2. **NavMesh 구형 환경 미지원:** NavMesh는 Z축이 Up인 평면 지형을 전제로 설계됨. 위치마다 Up 방향이 달라지는 구형 내벽 환경에서는 Reachable 영역 및 경로를 올바르게 계산할 수 없음.
3. **Navigation API 호환성:** `UNavigationSystem` API 또한 게임스레드 전용이어서 Mass 병렬 처리와 양립 불가.

### 채택한 대안

- 경로 탐색 대신 **직선 접근(Steering) + 장애물 회피(Avoidance)**로 대체.
- **`ULNPSurfaceCacheSubsystem`** — 게임 시작 전 베이킹 단계에서 구형 내벽 표면 좌표를 등장방형(Equirectangular) 그리드로 사전 계산.
  - `bBakingComplete = true` 이후 배열은 읽기 전용 → 멀티스레드 안전.
  - 방향 벡터 → 격자 인덱스 변환(atan2/asin)으로 O(1) 조회.
  - 라인 트레이스 대비 CPU 비용 무시할 수준.
  - 상세 구현은 [TechDesign_SurfaceCache.md](TechDesign_SurfaceCache.md) 참조.

---
