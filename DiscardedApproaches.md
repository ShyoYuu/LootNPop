# 프로젝트 반복 및 의사결정 이력 (Iteration & Decision History)

이 문서는 프로젝트 개발 과정에서 시도된 다양한 기술적 접근과 그에 따른 결과, 기각 사유 및 교훈을 기록합니다.

---

## [Case 01] 단일 볼륨 기반 전역 PCG 구체 생성 (Single-Volume PCG Sphere)
- **일자:** 2026-04-08
- **상태:** **기각 (Dismissed)**
- **구현 방식:**
    - 단일 `PCG Volume` 내에서 커스텀 C++ 노드(`UPCGSphereWorldSettings`)를 사용하여 피보나치 분포로 전체 구체 포인트를 생성.
    - `Radius` 값에 따라 동적으로 전체 타일을 배치하고 노이즈 및 경사면 정렬 수행.

### 1. 실패 현상 (Failure Symptoms)
- **스케일링 문제:** 반지름(Radius) 10,000cm까지는 에디터에서 원활했으나, 실질적인 월드 크기인 25,000cm 이상으로 확장 시 시스템이 프리징(Freezing)됨.
- **자원 고갈:** 단일 PCG 컴포넌트가 수백만 개의 포인트를 한 번에 계산(Fibonacci + Noise + Alignment)하려고 시도하면서 CPU 및 메모리 점유율이 임계치를 초과.

### 2. 기각 사유 (Reason for Dismissal)
1. **생성 병목:** HISM은 렌더링을 최적화할 뿐, 생성 단계의 CPU 계산 부하를 줄여주지 못함.
2. **파티셔닝 미흡:** 단일 거대 볼륨 방식은 Unreal의 World Partition 그리드와 효율적으로 연동되지 않아, 플레이어 근처가 아닌 구체 전체를 매번 계산하려 함.
3. **편집 편의성 저하:** 전체 월드를 한 번에 로드해야 하므로 에디터 성능이 극도로 저하됨.

### 3. 교훈 및 대안 (Lessons Learned)
- **분할 정복 (Divide & Conquer):** 거대 지형은 반드시 조각(Octant/Sector)으로 나누어 관리해야 함.
- **정적/동적 하이브리드:** 모든 것을 실시간 PCG로 처리하기보다, `Geometry Script`로 기본 지각을 형성하고 그 위에 상세 요소를 PCG로 얹는 방식이 유리함.
- **Baking:** 최종 확정된 지형 데이터는 `Level Instance`나 `HISM Baker`를 통해 정적 데이터로 변환하여 런타임 계산을 제거해야 함.

---
