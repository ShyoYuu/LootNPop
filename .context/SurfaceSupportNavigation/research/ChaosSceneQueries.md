# Chaos Scene Query 조사

> 상태: 엔진 소스 분석 기준선
> 읽기 조건: Chaos BVH, 동적 물체 포함 여부, trace semantics 또는 worker query를 재검토할 때
> 최초 설계 원본: `../history/InitialPlan.md`

## `LineTraceMulti`는 모든 blocking surface를 반환하지 않음

언리얼의 multi trace는 overlap hit들과 가장 가까운 blocking hit까지만 반환한다. 첫 blocking hit 뒤의 테스트는 수행하지 않는다.

따라서 기본 지각과 부유섬이 모두 blocking일 때 한 번의 `LineTraceMulti`로 모든 층을 베이크하는 설계는 사용할 수 없다.

에디터 베이커는 다음 중 하나를 사용해야 한다.

- Support source별 독립 트레이스
- 첫 hit 뒤 시작점을 전진시키는 반복 트레이스
- 삼각형의 방사형 아틀라스 직접 rasterization
- 명시적인 Support Proxy Mesh

동굴과 merged Mesh Terrain까지 고려하면 장기적으로 triangle rasterization 또는 명시적 Support Proxy가 가장 안정적이다.

## Chaos scene query와 동적 물체

Chaos의 scene acceleration collection은 static, dynamic, query-only body를 포함한다. Movable Static Mesh, kinematic panel, rigid body, Geometry Collection 조각도 collision 설정이 맞으면 일반 `UWorld::LineTrace/Sweep`에 포함된다.

동적 물체가 늘면 별도 BVH가 필요한 것이 아니라 다음 비용이 증가한다.

- top-level broadphase entry 갱신
- 큰 swept bounds
- query candidate 증가
- Geometry Collection 파편 수
- 물리 collision pair
- scene read lock과 physics update 사이의 경합

Mass 쪽은 우선 기존 scene query를 사용하고, Unreal Insights와 전용 통계를 통해 실제 병목을 확인한다.

---
