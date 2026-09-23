# Chaos Scene Query 조사

> 상태: 엔진 소스 분석 기준선
> 읽기 조건: Chaos BVH, 동적 물체 포함 여부, trace semantics, 스레드별 query 또는 worker query를 재검토할 때
> 최초 설계 원본: `../history/InitialPlan.md`
> 엔진 경로 기준: `Engine/Source/Runtime/Engine/Private/Collision/` (UE 5.8)

## `LineTraceMulti`는 모든 blocking surface를 반환하지 않음

언리얼의 multi trace는 overlap hit들과 가장 가까운 blocking hit까지만 반환한다. 첫 blocking hit 뒤의 테스트는 수행하지 않는다.

따라서 기본 지각과 부유섬이 모두 blocking일 때 한 번의 `LineTraceMulti`로 모든 층을 베이크하는 설계는 사용할 수 없다.

에디터 베이커는 다음 중 하나를 사용해야 한다.

- Support source별 독립 트레이스
- 첫 hit 뒤 시작점을 전진시키는 반복 트레이스
- 삼각형의 방사형 아틀라스 직접 rasterization
- 명시적인 Support Proxy Mesh
- mesh 공간 다중 교차 쿼리: GeometryCore `TMeshAABBTree3::FindAllHitTriangles` (`Runtime/GeometryCore/Public/Spatial/MeshAABBTree3.h`)

동굴과 merged Mesh Terrain까지 고려하면 장기적으로 triangle rasterization 또는 mesh 공간 다중 교차 쿼리가 가장 안정적이다.

## Chaos scene query와 동적 물체

Chaos의 scene acceleration collection은 static, dynamic, query-only body를 포함한다. Movable Static Mesh, kinematic panel, rigid body, Geometry Collection 조각도 collision 설정이 맞으면 일반 `UWorld::LineTrace/Sweep`에 포함된다.

동적 물체가 늘면 별도 BVH가 필요한 것이 아니라 다음 비용이 증가한다.

- top-level broadphase entry 갱신
- 큰 swept bounds
- query candidate 증가
- Geometry Collection 파편 수
- 물리 collision pair
- scene read lock과 게임 스레드 물리 쓰기 사이의 경합

Mass 쪽은 우선 기존 scene query를 사용하고, Unreal Insights와 전용 통계를 통해 실제 병목을 확인한다.

## 스레드와 락

2026-09-23 엔진 소스 확인 결과.

### 동기 query는 임의 스레드에서 호출할 수 있다

- 트레이스·스윕 공통 경로 `TSceneCastCommonImpWithRetryRequest`는 `FScopedSceneReadLock`으로 씬 읽기 락을 잡는다(`SceneQuery.cpp`).
- overlap 경로는 `FPhysicsCommand::ExecuteRead(&PhysScene, ...)`로 같은 읽기 락을 잡는다(`SceneQuery.cpp` `GeomOverlapMultiHelper`).
- 게임 스레드의 body 쓰기는 `FPhysInterface_Chaos::ExecuteWrite`가 쓰기 락을 잡는다(`PhysicsEngine/Experimental/PhysInterface_Chaos.cpp`).
- 락 구현은 `CHAOS_SCENE_LOCK_TYPE`로 정해지는 읽기/쓰기 락이다(`Runtime/Experimental/Chaos/Public/Framework/Threading.h`). 기본값은 에디터 빌드가 `RWFIFO_CRITICALSECTION`(공정 RW, yield), 게임 빌드가 `FRWLOCK`(플랫폼 RW 락)이다. 읽기끼리는 동시에 진행하고, 쓰기 중에는 읽기가 대기한다. 에디터와 패키지의 락 특성이 다르므로 락 대기는 패키지 빌드에서도 측정한다.
- `GetThreadQueryContext`의 주석은 오디오·애니메이션 같은 게임 스레드 외 태스크의 query를 명시적으로 고려한다.

결론: Mass worker의 동기 query는 thread-safe지만 lock-free가 아니다. 게임 스레드가 컴포넌트를 움직이는 동안 대기가 생길 수 있으므로 락 대기를 측정해야 한다.

### 비동기 query는 게임 스레드 전용이다

- `StartNewTrace`가 `check(IsInGameThread())`와 `check(DataBuffer.bAsyncAllowed)`를 검사한다(`WorldCollisionAsync.cpp`).
- 발행 가능 구간은 월드 틱의 `ResetAsyncTrace`~`FinishAsyncTrace` 사이이고, 결과는 다음 프레임에 게임 스레드 콜백으로 온다.
- 기존 SurfaceCache 베이크가 `FTickableGameObject::Tick`에서 발행하는 이유가 이 구간 제약이다.

### 비동기 물리에서 query가 보는 데이터

프로젝트는 `bTickPhysicsAsync=True`다. `GetThreadQueryContext`는 fixed tick 콜백 중이 아닌 한 `EThreadQueryContext::GTData`, 즉 게임 스레드에서 보간된 자세를 사용한다(`SceneQuery.cpp`). 정적 지형은 영향이 없고, 물리로 시뮬레이션되는 물체는 query가 보는 자세와 물리 스레드 자세가 다를 수 있다. 동적 지형을 게임 스레드 kinematic으로 구동하는 근거다(D-027).

## custom acceleration structure 경로

엔진은 표준 scene query를 custom spatial acceleration에 대해 실행하는 경로를 이미 가진다.

- `FGenericRaycastPhysicsInterfaceUsingSpatialAcceleration<AccelType>`와 `FGenericGeomPhysicsInterfaceUsingSpatialAcceleration<AccelType, GeomType>`
- `Chaos::IDefaultChaosSpatialAcceleration`과 `IExternalSpatialAcceleration`에 대해 명시적으로 인스턴스화된다(`SceneQuery.cpp` 파일 끝).
- 내부적으로 `FOverrideAccelContainer`를 통해 같은 필터·narrowphase·hit 변환을 사용한다.

D-008의 별도 BVH가 필요해지더라도 narrowphase와 필터를 재구현할 필요 없이 acceleration structure만 교체할 수 있다.

---
