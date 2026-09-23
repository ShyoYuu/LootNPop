# 런타임 Surface Query와 MassWorldCollision 설계

> 상태: 초안
> 읽기 조건: Surface query, Chaos scene query, NPC·투사체 월드 충돌을 구현하거나 측정할 때
> 최초 설계 원본: `../history/InitialPlan.md`

## 조회 방침

지면 조회는 Support snapshot이 기본이다(D-025). exact scene query는 다음 세 경우에만 사용한다.

| 조건 | 예 |
|:---|:---|
| 캐시가 표현하지 못하는 대상 | 벽·천장·섬 측벽·프랍·동적 지형, LoS, 투사체 segment |
| 캐시가 확신하지 못하는 결과 | `NeedsExact`, edge·risk·coverage 밖, Runtime Overlay 무효화 |
| 상태 전환 순간 | 공중→착지, Layer 전환, 동적 지형 접촉 시작 |

투사체·공중 착지·LoS가 처음부터 exact인 것은 이 방침의 예외가 아니라 첫 번째 조건에 해당한다. Support Atlas는 벽과 천장을 표현하지 않는다.

적의 90% 이상은 Actor 없는 PureEntity다. PureEntity도 Mass worker에서 동기 exact를 사용하므로 캐시에만 의존하지 않는다. 목표는 exact 0회가 아니라 대부분의 프레임이 캐시로 끝나고 exact는 필요한 순간에만 호출되는 것이다.

### 관찰 거리 축

부정확함이 문제가 되는 곳은 플레이어가 보는 곳이다. 위 조건에 관찰 거리를 겹친다.

| 거리 | risk·edge 구간 지면 | 정확성 필수 전환(낙하 시작·착지·Layer 전환) |
|:---|:---|:---|
| 플레이어 근처 | exact | exact |
| 원거리 | 거친 지지면(Nav 셀 대표 Support) | exact |

플레이어 근처라고 해서 무조건 exact는 아니다. 캐시가 확신하는 지면은 근처에서도 캐시를 쓴다. 수백 마리가 한 플레이어에게 몰리는 장면에서 근처 전부를 exact로 처리하면 그것이 가장 비싼 구성이 된다. 근처 반경과 원거리 판정 주기는 Phase 3b 실측으로 정한다.

## 스레드별 쿼리 API

| 호출 위치 | 허용 API | 결과 시점 | 근거 |
|:---|:---|:---|:---|
| Mass worker | 동기 `LineTrace`·`Sweep`·`Overlap` | 즉시 | 동기 경로가 Chaos 씬 읽기 락을 잡는다(`SceneQuery.cpp` `FScopedSceneReadLock`) |
| Mass worker | 비동기 `AsyncLineTrace`·`AsyncSweep` | 금지 | 발행 시 `check(IsInGameThread())`와 `bAsyncAllowed` 검사(`WorldCollisionAsync.cpp` `StartNewTrace`) |
| 게임 스레드 | 동기 API | 즉시 | 같은 프레임에 결과가 필요할 때 |
| 게임 스레드 | 비동기 API | 다음 프레임 콜백 | 한 프레임 지연을 견디는 대량·주기 쿼리. 월드 틱의 `ResetAsyncTrace`~`FinishAsyncTrace` 구간 안에서만 발행 |

- worker의 동기 query는 thread-safe지만 lock-free가 아니다. 게임 스레드의 물리 쓰기(컴포넌트 이동, Mover 이동)가 쓰기 락을 잡는 동안 대기한다.
- 읽기 락끼리는 서로 막지 않으므로 여러 worker의 동시 query는 줄을 서지 않는다. 에디터 빌드의 공정 FIFO 락에서는 대기 중인 쓰기 뒤에 새 읽기가 선다.
- 이 게임은 적의 90% 이상이 물리 body가 없는 PureEntity라, 매 프레임 게임 스레드가 움직이는 body는 플레이어·소수 엘리트·동적 지형뿐이다. 락 경합보다 query 자체의 CPU 비용(지각 trimesh broadphase·narrowphase)이 한계를 정할 가능성이 높다.
- Gate 0은 query 1회당 비용·처리량과 락 대기 시간을 따로 측정한다.
- worker는 결과에서 UObject를 역참조하지 않는다. 위치·법선·거리·time·blocking 여부 같은 POD만 반환한다.
- 서브시스템은 `TMassExternalSubsystemTraits`에서 `GameThreadOnly = false`를 선언한다.
- 기존 문서와 주석의 "라인트레이스는 게임 스레드 전용" 서술은 Phase 3 Gate 0 검증 뒤 이 방침으로 정정한다.

### 비동기 물리와 쿼리 데이터 시점

프로젝트는 `bTickPhysicsAsync=True`다. 게임 스레드 밖의 scene query는 GT data, 즉 게임 스레드에서 보간된 자세를 본다(`SceneQuery.cpp` `GetThreadQueryContext`).

- 정적 지형에는 차이가 없다.
- 물리로 시뮬레이션되는 물체는 Mass query와 물리 스레드 측 이동이 서로 다른 자세를 볼 수 있다.
- 따라서 동적 지형은 게임 스레드 kinematic으로 결정론적 함수에 따라 움직인다(D-027). 게임 스레드가 설정한 transform이 곧 query가 보는 자세다.

---

## Surface query API

기존 `GetSurfacePoint(Direction)`는 다층 환경에서 제거 대상이다.

개념 API:

```cpp
struct FSupportQuery
{
    FVector WorldPosition;
    FSurfaceHandle PreferredSurface;
    float MaxStepUp;
    float MaxDrop;
    float CapsuleRadius;
};

struct FSupportQueryResult
{
    ESupportQueryStatus Status;
    FVector Point;
    FVector Normal;
    FSurfaceHandle Surface;
    float RadialDelta;
    ESupportRiskFlags RiskFlags;
};
```

상태 후보:

- `HighConfidence`
- `NeedsExact`
- `NoSupport`
- `OutsideCoverage`
- `InvalidatedByRuntimeOverlay`
- `NotReady`

조회 순서:

1. `PreferredSurface`가 유효하면 같은 Layer 우선
2. 현재 방향과 Atlas angular bounds로 후보 축소
3. 현재 반지름·step/drop 규약으로 가능한 Layer 선택
4. 안전한 coverage 내부면 보간
5. edge/risk/invalidated면 exact 요청

SupportCache가 다른 Layer로 임의 스냅해서는 안 된다. Layer 전환은 exact landing 또는 유효한 Walk 연결을 통해서만 일어난다.

---

## MassWorldCollision

### 초기 구현 원칙

- 기존 `UWorld::LineTrace`·`Sweep`의 동기 경로 사용
- 별도 triangle/BVH 복제 없음
- channel은 `LNPWorldExact`(D-022). Support source 검증에는 `LNPSurfaceSupport`
- Pawn은 `LNPWorldExact`에 응답하지 않는다. channel 기본 응답이 Ignore다

### 서브시스템 API

```cpp
RaycastWorld(...)
SweepSphereWorld(...)
SweepCapsuleWorld(...)
ProbeSupport(...)
```

서브시스템은 다음을 캡슐화한다.

- trace channel과 query params
- scene query 통계와 락 대기 계측
- dynamic terrain 분류
- debug draw queue
- worker-safe POD result

### 제외 대상

- `LNPDecoration`: 두 channel 모두 무시
- 작은 파괴 파편: `LNPWorldExact`에 응답하지 않는 profile 사용

Support 베이크는 collision channel 하나에 의존하지 않고 Terrain Contract의 Component Tag를 사용한다.

### 쿼리 분류

| 분류 | 생략 가능 | 용도 |
|:---|:---:|:---|
| ProjectileMandatory | 아니오 | 현재 이동 segment 충돌 |
| AirborneMandatory | 아니오 | 넉백·낙하 착지 |
| GroundRiskFallback | 아니오 | edge·절벽·불연속·동적 overlay |
| DynamicSupportContact | 아니오 | 움직이는 패널 위 접촉 |
| PeriodicGroundValidation | 가능 | 고신뢰 지면의 주기 검증 |
| DebugValidation | 가능 | Support와 Chaos 오차 측정 |

정확성 필수 쿼리와 품질 향상용 쿼리의 예산을 섞지 않는다.

### 프로파일 결과에 따른 후속 선택

다음 조건이 확인될 때만 custom acceleration structure를 별도 과제로 검토한다.

- scene read lock 대기가 프레임 병목
- query filter로 줄일 수 없는 broadphase 비용
- Mass worker 동시 query가 게임 스레드 물리 쓰기를 반복적으로 막음
- 전용 channel과 query budget 최적화 후에도 목표 프레임을 넘음

그 경우에도 narrowphase와 필터를 다시 구현하지 않는다. 엔진의 `FGenericRaycastPhysicsInterfaceUsingSpatialAcceleration`이 custom `ISpatialAcceleration`을 받아 표준 query 경로를 그대로 사용한다(`../research/ChaosSceneQueries.md`).

---

## 투사체 월드 충돌

### 정확성 기준선

Phase 3에서 모든 투사체가 매 프레임 다음을 수행하도록 전환한다.

- `PreviousPos → CurrentPos` world segment/sphere sweep
- Mass target capsule 수학 판정
- world hit time과 entity hit time 비교
- 가장 이른 hit만 채택

이로써 섬 측벽 뒤 적, 동굴 벽, 나무·바위, 움직이는 패널을 올바르게 처리한다.

전환 범위:

- 서버 판정 경로
- 클라이언트 ghost 투사체의 코스메틱 판정 경로
- 게임 스레드 탄도 가이드 `PredictArc`. 기존 코드가 "가이드와 실제 착탄은 같은 판정 함수"를 불변식으로 두므로 투사체와 같은 Phase에 바꾼다.

`IsUnderSurface`와 반지름 기반 착탄 판정은 제거한다.

### 최외곽 반지름 안전망

지각은 두께 없는 단면이다. exact segment 판정이 한 번 빗나가면 투사체나 엔티티가 지각 밖으로 빠져도 되돌릴 장치가 없다. 내부형 구에서 지면 아래는 바깥쪽이므로, 모든 옥탄트 geometry의 최대 반지름보다 바깥은 항상 월드 밖이다. `반지름 > 옥탄트 geometry 최대 반지름 + 여유` 검사는 층과 무관하게 유효하며, 이를 종료·복구 안전망으로 유지한다. 동굴은 지각보다 바깥쪽으로 파고들므로 기준은 지각 반지름이 아니라 베이크된 옥탄트 bounds의 최대 반지름이어야 한다.

### Support 기반 충돌 horizon

Support 조회가 Chaos query보다 저렴한 것은 맞지만 SupportData는 벽·천장·동적 blocker를 모두 표현하지 않는다. 따라서 horizon은 exact query를 완전히 대체하는 판정으로 사용하지 않는다.

프로파일링 후 선택적 최적화(Phase 12):

1. 탄도 궤적을 저해상도로 Support에 샘플링
2. 정적 walkable surface까지 예상 충돌 시간 계산
3. 임계 시간 안에 들어오면 미래 탄도 구간을 Chaos로 exact pretrace
4. static hit의 시간·점·법선 캐시
5. dynamic blocker와 Mass target은 별도 검사

다음 경우 horizon과 무관하게 exact를 유지한다.

- homing 또는 궤적 변경
- bounce
- 고속탄
- cave/island edge/risk 영역
- dynamic terrain 인접
- Support coverage 밖

1초는 초기 실험값일 뿐이며 projectile speed, lookahead distance, query 비용으로 조정한다.

### 알려진 한계

서버 lag compensation은 `FLNPPositionHistoryFragment`를 가진 Enemy·플레이어 캡슐만 되감는다(`../../TechDesign_Networking.md` §3.1). 월드 query는 되감지 않으므로 움직이는 패널은 판정 시점의 현재 자세로 맞는다. 결정론적 움직임이라 필요하면 되감은 시각의 자세를 계산할 수 있지만, 체감 문제가 확인되기 전에는 도입하지 않는다.

---
