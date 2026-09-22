# 런타임 Surface Query와 MassWorldCollision 설계

> 상태: 초안
> 읽기 조건: Surface query, Chaos scene query, NPC·투사체 월드 충돌을 구현하거나 측정할 때
> 최초 설계 원본: `../history/InitialPlan.md`

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

- 기존 `UWorld::LineTrace`·`Sweep` 사용
- worker에서 동기 scene query 사용
- 별도 triangle/BVH 복제 없음
- `AsyncLineTrace`를 Mass worker 경로에서 사용하지 않음
- 결과에서 UObject를 worker가 역참조하지 않음
- 위치·법선·거리·time·blocking 여부 같은 POD 결과만 반환

### 서브시스템 API

```cpp
RaycastWorld(...)
SweepSphereWorld(...)
SweepCapsuleWorld(...)
ProbeSupport(...)
```

서브시스템은 다음을 캡슐화한다.

- trace/object channel
- query params
- scene query 통계
- dynamic terrain 분류
- debug draw queue
- worker-safe POD result

### 충돌 프로파일

최소 다음 의미를 분리한다.

- `LNP_MassWorld`: NPC·투사체를 막는 지형과 선택된 프랍
- `LNP_DynamicSupport`: 움직이는 패널·기믹
- Decoration: Mass world query에서 제외
- 작은 파괴 파편: 기본적으로 Mass world query에서 제외

Support 베이크는 collision channel 하나에 의존하지 않고 authoring metadata를 사용한다.

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

다음 조건이 확인될 때만 custom immutable Chaos snapshot/TLAS를 별도 과제로 검토한다.

- scene read lock 대기가 프레임 병목
- query filter로 줄일 수 없는 broadphase 비용
- Mass worker 동시 query가 physics thread 진행을 반복적으로 방해
- 전용 channel과 query budget 최적화 후에도 목표 프레임을 넘음

메모리와 lifetime 복잡성 때문에 추측만으로 도입하지 않는다.

---

## 투사체 월드 충돌

### 정확성 기준선

첫 구현은 모든 투사체가 매 프레임 다음을 수행한다.

- `PreviousPos → CurrentPos` world segment/sphere sweep
- Mass target capsule 수학 판정
- world hit time과 entity hit time 비교
- 가장 이른 hit만 채택

이로써 섬 측벽 뒤 적, 동굴 벽, 나무·바위, 움직이는 패널을 올바르게 처리한다.

`IsUnderSurface`와 반지름 기반 착탄 판정은 제거한다.

### Support 기반 충돌 horizon

Support 조회가 Chaos query보다 저렴한 것은 맞지만 SupportData는 벽·천장·동적 blocker를 모두 표현하지 않는다. 따라서 horizon은 exact query를 완전히 대체하는 판정으로 사용하지 않는다.

프로파일링 후 선택적 최적화:

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

---

