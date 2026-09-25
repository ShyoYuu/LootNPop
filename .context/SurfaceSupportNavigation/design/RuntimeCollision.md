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
| 원거리 | 안전한 Nav cell 내부에서만 거친 지지면, 불확실한 경계는 차단 또는 exact | exact |

플레이어 근처라고 해서 무조건 exact는 아니다. 캐시가 확신하는 지면은 근처에서도 캐시를 쓴다. 수백 마리가 한 플레이어에게 몰리는 장면에서 근처 전부를 exact로 처리하면 그것이 가장 비싼 구성이 된다. 근처 반경과 원거리 판정 주기는 Phase 3b 실측으로 정한다.

원거리 coarse support는 정확성 필수 전환을 생략하는 수단이 아니다(D-038).

- 현재 cell과 다음 cell 사이의 Nav edge가 유효하고 두 cell 모두 coverage interior일 때만 coarse grounded 이동을 허용한다.
- `NeedsExact`, coverage edge, drop 후보, runtime overlay 경계는 원거리에서도 임의로 통과하지 않는다.
- 경계를 넘지 않아도 되는 LOD에서는 보수적으로 정지·steering 변경하고, 실제 경계 통과나 낙하 전환이 필요하면 그 프레임에 exact를 수행한다.
- 이 규칙으로 `GroundRiskFallback`의 "생략 불가"는 유지하면서 원거리 개체의 매 프레임 risk query를 피한다.

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
- Gate 0 패키지 측정(게임 락 `FRWLOCK`, 1024 q/frame)에서 게임 스레드 쓰기가 겹치지 않으면 프레임 락 합 P95는 0.1ms였고, kinematic body 62개를 같은 PrePhysics 구간에 움직이면 3.4ms로 30배가 됐다. query 자체도 20~30% 느려졌다. 락 대기는 읽기끼리의 경합이 아니라 **게임 스레드 물리 쓰기와 query phase의 겹침**이 만든다(`../history/Phase03_Log.md` 2026-09-24 Gate 0 패키지 절).
- 따라서 게임 스레드가 매 프레임 움직이는 body(동적 패널 transform)는 Mass exact query phase가 시작되기 전에 끝나도록 배치한다. 현재 패널은 TG_PrePhysics Actor 틱에서 움직이고 worker exact query는 StartPhysics 이후 페이즈에만 있다. PrePhysics 페이즈에 exact 소비자를 추가할 때는 `DynamicTerrain.md` §2의 순서를 다시 검토한다. ActorPromoted 적·플레이어 Mover의 이동 쓰기가 겹치는 조건(2P, 적 1000, 동시 Actor 4~6)에서도 발사체 500발 기준 프레임 락 합 P95는 0.128ms였다. Actor 수는 `MaxPromotedSlotsPerPlayer`가 묶으므로 적 총수가 늘어도 쓰기 겹침은 커지지 않는다(`../history/Phase03_Log.md` 2026-09-25).
- worker는 결과에서 UObject를 역참조하지 않는다. 위치·법선·거리·time·blocking 여부 같은 POD만 반환한다.
- 서브시스템은 `TMassExternalSubsystemTraits`에서 `GameThreadOnly = false`를 선언한다.
- 구현: `ULNPMassWorldCollisionSubsystem`(`SurfaceNavigation/LNPMassWorldCollision.*`). 입력 `FLNPWorldQueryParams`(분류·제외 Actor unique ID), 결과 `FLNPWorldHit`(POD + `FLNPExactHitIdentity`). 분류별 count·시간은 atomic counter, 락 probe는 CVar `LNP.SurfaceNav.WorldCollision.LockProbe`, debug draw는 MPSC 큐를 게임 스레드 Tick에서 그린다(`...DebugDraw`). 보고는 `LNP.SurfaceNav.WorldCollision.Report`.
- self/owner 제외: Pawn은 채널 기본 응답이 Ignore이고, PureEntity·Mass 투사체는 world body가 없어 제외할 대상이 없다. 그 밖의 제외는 게임 스레드에서 미리 구한 Actor unique ID로만 넘긴다.
- `ProbeSupport`는 호출자가 준 Up 기준으로 walkable 법선을 판정하고, registry 역할에 `Support`가 있어야 지지면으로 인정한다. 월드 Z를 쓰지 않는다.
- 기존 문서와 주석의 "라인트레이스는 게임 스레드 전용" 서술은 Phase 3 Gate 0 검증 뒤 이 방침으로 정정한다.

### hit identity registry

정확한 착지와 동적 지형 접촉은 위치·법선만으로 결정할 수 없다. 게임 스레드는 Level Instance 가시화와 동적 요소 스폰 뒤 다음 immutable registry를 게시한다(D-037).

```text
(Physics Shape 또는 Component identity, FaceIndex, InstanceIndex)
    → static/dynamic 의미 플래그
    → exact surface identity와 선택적 (slot, LocalLayerId) 후보
    → DynamicSupportId 또는 Stateful MarkerId
```

- component 하나가 여러 disconnected Support Layer를 만들 수 있으므로 component→Layer 단일 매핑은 금지한다.
- complex collision은 `FaceIndex`, ISM·HISM은 hit item/instance index를 함께 사용한다. Nanite/fallback collision에서 식별자가 안정적인지는 Phase 3 Gate 0과 Phase 4a에서 각각 검증한다.
- Phase 3은 identity 추출과 static/dynamic 의미 분류까지만 요구한다. 실제 `(slot, LocalLayerId)` binding은 Phase 4가 face/instance→Layer 표를 만들고 Phase 5 snapshot이 게시할 때 추가한다.
- query wrapper는 engine hit에서 registry key를 추출한 뒤 POD 의미 결과만 worker 호출자에게 돌려준다. UObject tag, Actor class, component property를 worker에서 읽지 않는다.
- registry가 hit를 해석하지 못하면 임의 Layer로 스냅하지 않고 `UnknownExactSurface`를 반환한다. 착지는 보수적으로 처리하고 진단 counter를 증가시킨다.
- registry는 Level Instance와 component lifetime보다 짧지 않아야 하며 match reset·stream unload 전에 Mass lifecycle gate로 worker 접근을 중단한다.

#### Phase 3 구현 계약 (`ULNPHitIdentitySubsystem`)

| 항목 | 규약 |
|:---|:---|
| slot source | 옥탄트 생성 완료 뒤 slot Level의 component 중 `LNPWorldExact`를 Block하는 것을 LNP collision profile로 분류해 일괄 등록한다. slot Level 목록이 바뀌면(생성 재시작·언로드) 전부 버리고 다시 모은다 |
| 런타임 source | slot Level 밖의 source는 소유자가 BeginPlay·EndPlay에서 등록·해제한다. 분류는 같은 profile 표를 쓴다. 현재 스프링 런처, 이후 동적 패널 |
| Mass proxy source | proxy 서브시스템의 index→엔티티 표를 게시마다 복사한다 |
| 분류 | profile → `Static`·`Dynamic`·`Destructible` 수명주기와 `Support`·`Blocker` 역할. 분류되지 않는 Block component는 등록하지 않으므로 hit 시 `Unknown`이 된다 |
| 키 | weak component의 index·serial. 해시와 비교 모두 UObject를 역참조하지 않는다 |
| 게시 | 게임 스레드가 dirty만 표시하고 tickable 구간에서 새 불변 snapshot으로 교체한다. tickable은 TG_PostPhysics 완료 대기 뒤·TG_PostUpdateWork 전에 돌고(`LevelTick.cpp`), 이 구간에는 Mass phase가 없다 |
| worker 조회 | `GetSnapshot`·`ResolveHit`. snapshot은 공유 참조로 잡고 락 없이 읽는다 |
| 결과 | `FLNPExactHitIdentity` POD: 수명주기·역할·slot·FaceIndex·InstanceIndex·proxy 엔티티·registry generation |
| 미해석 | 미등록 component와 proxy 표 범위 밖 Item은 `Unknown`을 반환하고 counter를 올린다 |

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
- hit identity registry 조회와 미해석 hit 통계
- 내부형 구·동굴·동적 지형의 walkable normal 판정

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

원거리 LOD에서 경계를 통과하지 않고 보수적으로 정지하는 것은 `GroundRiskFallback` 생략이 아니다. 실제 전환을 요청한 프레임에는 반드시 exact를 수행한다.

### Gate 0 계측 계약

- wrapper 전체 query 시간과 scene read-lock 대기를 별도 trace/counter로 기록한다. 엔진 기본 marker만으로 락 대기를 분리할 수 없으면 최소 범위의 엔진 trace instrumentation을 개발 빌드에 추가한다.
- warm-up, 적 수, PureEntity 비율, 동시 투사체 수, 동적 body 수, 측정 build와 CPU 구성을 보고서에 고정한다.
- P50/P95와 최악 프레임, query 종류별 count/time, 미해석 hit 수를 기록한다.
- Editor와 패키지의 락 구현이 다르므로 최종 Gate는 패키지 빌드 리슨 서버와 비동기 물리에서 판정한다. `UnrealEditor.exe -game`은 `WITH_EDITOR` 빌드라 에디터 락을 쓴다.
- 설치형 엔진에서는 쿼리 내부의 락 대기를 분리할 수 없다. 질의 직전에 같은 scene read lock을 한 번 잡았다 놓는 시간을 락 대기 추정치로 쓴다(Gate 0 스파이크 방식).

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

구현 규약(구현 단위 2):

| 항목 | 규약 |
|:---|:---|
| 단일 판정 함수 | `LNPProjectileMotion::TraceWorld` — `RaycastWorld`, 분류 `ProjectileMandatory`. 서버 투사체·클라이언트 Ghost·탄도 가이드 `PredictArc`가 같은 함수를 쓴다 |
| 형상 | sphere가 아니라 **line**이다. 투사체에는 월드 판정용 반지름이 없고, `HitRadius`는 캐릭터 캡슐을 부풀리는 값이라 지형에 쓰면 턱·모서리에 먼저 걸린다. 형상이 필요해지면 무기 DA에 월드 반지름을 따로 둔다 |
| earliest hit | 월드 hit을 먼저 구하고 캐릭터 판정 선분을 `PreviousPos → ImpactPoint`로 자른다. 잘린 선분 위의 캐릭터 hit은 항상 월드 hit보다 이르므로 hit 시각을 따로 비교하지 않는다 |
| 월드 착탄 | 폭발·스플래시 중심은 `ImpactPoint`, 임팩트 VFX는 `ImpactNormal` 방향으로 띄운다. 미해석(`Unknown`) hit도 LNPWorldExact를 Block한 것이므로 착탄으로 처리하고 counter만 오른다 |
| 수명 만료 | 그 자리에서 공중 폭발한다 |
| envelope 이탈 | 월드 hit 없이 `반지름 > envelope + 500cm`면 VFX·스플래시 없이 소멸하고 `EnvelopeEscapes` counter를 올린다. 가이드도 같은 스텝에서 끝난다(아래 "최외곽 반지름 안전망") |
| 관전 Ghost 외삽 | 발사 방송 도착 지연만큼 외삽해 스폰하는 구간(최대 200ms)도 `TraceWorld`로 검사하고, 월드에 맞으면 Ghost를 만들지 않는다. 검사하지 않으면 지면·프랍 너머에서 태어난다. 착탄 VFX는 서버 확정 큐가 서버 위치에 재생한다 |
| 기본 경로 | exact만 있다. audit(MISSING=0)와 production 8-slot oracle(`../design/ValidationAndMigration.md`) 통과 뒤 legacy `IsUnderSurface` 경로와 전환 CVar를 제거했다(D-036) |
| 탄도 가이드 준비 신호 | exact 경로도 SurfaceCache 베이크 완료를 옥탄트 로드 완료 신호로만 쓴다. SurfaceCache 제거(Phase 5) 때 옥탄트 생성 완료로 바꾼다 |

알려진 한계: 패링 반사탄은 `CurrentPos`에서 다시 스폰된다. 월드 hit으로 잘린 선분 위에서 패링이 일어나면 `CurrentPos`가 벽 너머일 수 있고, 반사탄은 다음 프레임 같은 벽에 착탄한다. 벽에 붙어 패링하는 경우만 해당하며 체감 문제가 확인되면 반사 위치를 패링 hit 지점으로 바꾼다.

전환 범위:

- 서버 판정 경로
- 클라이언트 ghost 투사체의 코스메틱 판정 경로
- 게임 스레드 탄도 가이드 `PredictArc`. 기존 코드가 "가이드와 실제 착탄은 같은 판정 함수"를 불변식으로 두므로 투사체와 같은 Phase에 바꾼다.

`IsUnderSurface`와 반지름 기반 착탄 판정은 제거했다.

### 최외곽 반지름 안전망

지각은 두께 없는 단면이다. exact segment 판정이 한 번 빗나가면 투사체나 엔티티가 지각 밖으로 빠져도 되돌릴 장치가 없다. 내부형 구에서 지면 아래는 바깥쪽이므로, 모든 옥탄트 geometry와 동적 요소의 authoring swept bounds보다 바깥은 항상 월드 밖이다. `반지름 > world collision envelope 최대 반지름 + 여유` 검사는 층과 무관하게 유효하며, 이를 종료·복구 안전망으로 유지한다. 동굴과 움직이는 요소는 지각보다 바깥쪽으로 갈 수 있으므로 기준은 지각 반지름이 아니라 베이크된 정적 bounds와 마커 경로 swept bounds를 합친 envelope여야 한다.

현재 구현(Phase 3):

| 항목 | 규약 |
|:---|:---|
| 원천 | hit identity registry에 등록된 source의 `MaxRadius` 최댓값을 snapshot `WorldEnvelopeRadius`로 게시한다. LootPod proxy는 지면 위라 넣지 않는다. 조회는 `ULNPMassWorldCollisionSubsystem::GetWorldEnvelopeRadius`이고 source가 없으면 0(안전망 꺼짐)이다 |
| `MaxRadius` 계산 | 등록 시점에 한 번(`ULNPHitIdentitySubsystem::ComputeSourceMaxRadius`). complex trimesh는 cooked 물리 정점, 단순 shape는 AABB 꼭짓점, ISM·HISM은 instance별 mesh bounds 꼭짓점. 지각 한 장의 world bounds 꼭짓점은 반지름의 약 √3배라 쓰지 않는다 |
| 에디터 베이크 전 | Phase 4 베이커가 정적 bounds를 SurfaceData에 넣으면 그 값으로 바꾼다. 런타임 계산은 production 8 slot에서 약 7ms, 1회다 |
| 동적 요소 | 런타임 source는 등록 시점 자세로 잰다. 스폰 뒤 움직이지 않는 장치(D-045)는 그대로 맞다. 움직이는 패널은 `RegisterRuntimeSource`의 `MaxRadiusOverride`로 경로 swept 반지름을 넣는다(Meadow_00 마커 25,435cm) |
| 여유 | 투사체 500cm(`LNPProjectileMotion::WorldEnvelopeMargin`). envelope 밖에서는 중력과 반지름 방향 속도가 모두 바깥을 향해 돌아올 수 없으므로 여유는 경계 여백일 뿐이다 |
| 기대 빈도 | 0이 정상이다. 이음매 좌표 평면을 정확히 따라가는 선분은 두 body의 공유 모서리 사이로 빠질 수 있다(8-slot oracle의 EdgeMiss). 측도 0이지만 이 안전망이 받는다 |

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
