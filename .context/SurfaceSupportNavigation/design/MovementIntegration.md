# NPC 이동·추격 통합 설계

> 상태: 초안
> 읽기 조건: 접지, airborne, 넉백, Pod 재귀속, 동굴 이동, 완전 비행 NPC, 활동 대역·구간 배회, 벽 타기 NPC를 구현할 때
> 최초 설계 원본: `../history/InitialPlan.md`

## 지상 NPC 행동 통합

### 두 이동 경로

아래 grounded·airborne 규칙은 Actor가 없는 순수 엔티티 경로의 규칙이며, 적의 90% 이상이 이 경로다. Actor가 붙는 소수 엘리트는 Mass 프로세서가 이동 의도만 넘기고, 접지·충돌·착지는 Mover가 exact collision으로 처리한다. 게스트는 복제된 적 Actor의 이동을 Mover로 재시뮬레이션한다.

순수 엔티티 경로의 exact 판정(3·4단계, 공중·착지)은 Phase 3b 프로토타입에서 먼저 만들고, Phase 6은 그 위에 Support 캐시를 얹는다. 캐시와 exact 선택은 `RuntimeCollision.md`의 관찰 거리 축을 따른다.

두 경로가 공유해야 하는 것은 판정이 아니라 결과다.

- 착지 이벤트: 순수 엔티티는 capsule sweep 결과, Actor는 Mover 착지 결과를 같은 이벤트로 발행한다.
- `FSurfaceHandle`: Actor 경로는 Mover floor hit의 component/shape, face index, ISM instance index를 hit identity registry에서 역조회해 기록한다. component 단독 매핑은 사용하지 않는다. LOD 전환 규칙은 `Architecture.md` "실행 경로별 소유권"이 소유한다.

Mover floor query는 face index를 요청하지 않는다. uniform source는 기존 component 표로 즉시 해석하고, 복수 Layer source의 face가 빠졌을 때만 기존 접점의 법선 방향 ±10cm를 `MassWorldCollision`의 `GroundRiskFallback` exact 광선으로 재검사한다. 같은 component·같은 instance에 맞은 face만 현재 snapshot으로 해석한다. 플레이어 Nav와 Actor NPC 인계가 이 경로를 공유한다. floor hit 자체가 아직 없는 Mover 활성화 첫 프레임에는 이전 정체성을 보존하지만, 유효한 현재 hit의 층을 끝내 해석하지 못하면 이전 정적 handle·동적 contact를 지운다. 공중 프레임은 기존 규칙대로 처리한다.

정적 접지 상태의 Actor 활성화는 Mass의 기존 raw Actor transform 이동에 더해 Mover 시뮬레이션 자세도 초기화한다. 현재 generation의 명시적 Surface handle이 있고 공중 속도가 0인 지상 적만 기존 같은 Layer 투영 경로로 캡슐 중심을 검증한다. 탐색 범위는 캡슐 반높이이며 같은 층을 찾았을 때만 Up 방향 2cm 여유를 더한 위치와 Entity 회전을 `FTeleportEffect`로 전달한다. 비행·공중·동적 접촉의 속도 인계와 현재 층을 검증하지 못한 경우의 기존 활성화 흐름은 유지한다.
- 경로 추종: A* 결과를 순수 엔티티는 waypoint로, Actor는 AI 이동 입력으로 받는다.

### grounded 이동

1. 현재 `FSurfaceHandle`의 Support를 우선 조회
2. high-confidence interior면 cache로 접지
3. edge/risk/discontinuity면 exact floor probe
4. 정적 obstacle risk 또는 runtime blocker가 있으면 capsule sweep
5. 같은 Layer에서 유효 support가 없으면 유령 지면으로 보간하지 않음
6. 실제 낙차가 발생하면 airborne로 전환

### airborne와 착지

- `PreviousPosition → ProposedPosition` capsule sweep
- earliest blocking hit 사용
- normal이 walkable이면 착지
- 내부형 구에서는 지역 Up이 중심 방향이다. exact hit normal의 방향·double-sided collision 동작을 회귀 테스트하고, 임의로 normal을 뒤집어 벽을 바닥으로 승격하지 않는다
- hit 위치에서 가장 가까운 Support Layer를 resolve
- 새 `SurfaceHandle`과 StaticNavComponent 기록. Phase 6에서는 `SurfaceHandle`만 기록하고, StaticNavComponent는 Phase 7부터 기록한다
- DynamicSupport면 dynamic contact 상태로 진입
- non-walkable hit면 반사/슬라이드/정지 정책을 별도 설정

현재 반지름 비교 착지는 제거한다.

원거리 grounded LOD는 안전한 Nav cell 내부에서만 대표 Support를 쓴다. coverage/risk 경계를 넘어야 하면 exact 전환을 수행하거나 보수적으로 이동을 멈춘다(D-038).

### 넉백 후 Pod 재귀속

Phase 7b는 정적 지면 착지 뒤 `ResolveEndpoints`의 D-062 도달성으로 Parent Pod를 검사한다. PureEntity 공중 이동과 Actor Mover 인계가 재검사 플래그를 세우고, 공통 경로 프로세서가 착지 뒤 처리한다. DynamicSupport 접촉 중에는 판정을 미룬다.

```text
Airborne
   ↓
착지
   ├─ Parent Pod와 같은 ReachabilityGroup → 기존 Pod 유지
   ├─ 다른 ReachabilityGroup → 재귀속 대기
   └─ DynamicSupport → Displaced 유지

재귀속 대기 후
   ↓
도달 가능한 활성 Pod 후보 조회
   ↓
도달 가능한 활성 Pod 중 가장 가까운 Pod로 Parent 갱신
```

절벽 건너편 Pod를 거리만으로 고르지 않도록 다음 순서를 사용한다.

1. 활성 Pod만 후보
2. D-062의 공통 ReachabilityGroup 짝이 있는 후보만 유지
3. 기존 Parent가 여전히 도달 가능한 활성 Pod이면 유지
4. 나머지는 chord 거리 최소 후보로 선택(동률은 엔티티 index 순)

7b 1차 구현의 후보 축소 거리와 최종 근사 비용은 모두 chord이므로, 가까운 4개를 별도 목록으로 만들지 않고 같은 최소 후보를 바로 고른다. 실제 A* 비용 비교는 구현 단위 4 측정에서 필요성이 확인될 때만 추가한다(`phases/Phase07b_PathExecution.md` §3.8).

후보가 없으면 `Orphaned` 상태로 둔다.

- 근처 플레이어가 있으면 현재 위치에서 전투
- 비전투 시 착지점을 배회 중심으로 삼고 현재 StaticNavComponent 밖 후보를 거부한다
- 1초 간격으로 활성 Pod를 다시 확인하므로 나중에 link가 열리거나 Pod가 생기면 재귀속한다
- 장시간 고립되고 비가시 상태면 despawn/reinsert를 선택적으로 적용

### 배회

배회 목표는 Parent Pod 주변의 임의 방향을 즉시 Support에 투영하지 않는다.

- Parent Pod와 같은 reachable Nav 영역의 spawn/wander candidate 사용
- edge clearance 보장
- path 없이 direct 이동 가능한 후보 우선
- 도달 불가능하면 path request 또는 재추첨

### 타게팅과 도달 가능성 분리

타게팅은 다음을 기준으로 하며 Nav 연결 여부로 후보를 삭제하지 않는다.

- 시야
- 거리
- FOV
- 위협도
- target slot
- Pod 세력권

이후 engagement 단계에서 reachability를 평가한다.

#### 슬롯과 도달성

타게팅 후보는 도달성으로 지우지 않지만, 근접 교전 슬롯 배정에는 도달성이 들어간다. 슬롯은 플레이어당 교전 밀도이자 Actor 승격 상한·대역폭 예산이다(`../../GameDesign_EnemyNPC.md` §4). 도달할 수 없는 근접 적이 슬롯을 쥐면 예산을 낭비하고 도달 가능한 적을 막는다.

- 근접 슬롯 배정 조건에 "대상과 같은 ReachabilityGroup"을 추가한다.
- 이미 슬롯을 가진 근접 적이 link 변화로 도달 불가가 되면 슬롯을 반납한다.
- 원거리 슬롯은 LoS·사거리 기준을 유지한다.

Phase 7b는 신규 지상 근접 슬롯에 D-062의 공통 group 짝을 요구한다(승격 근접 포함). 기존 슬롯은 플레이어 접지 group이 바뀌어 도달 불가가 되면 1.5초 유예 뒤 반납한다. 점프 중에는 마지막 접지 정보를 유지하고 기존 슬롯만 보존하며, `Stale`도 신규 슬롯 없이 기존 판정을 미룬다. 원거리·비행 슬롯은 이 필터를 사용하지 않는다.

#### 근접 NPC

- 지상 경로 있음: 추격
- 경로 없음: 슬롯 없이 Alert로 남되, 제자리 대신 **접근점**까지 걸어가 선다(D-063). 접근점은 자기 ReachabilityGroup의 내부 node(6방향 전부 열림) 중 목표에 가장 가까운 node다. 가장자리 바로 위에는 서지 않는다
- Alert 인내 시간 뒤 포기 또는 다른 target 선택은 그대로다

#### 원거리 NPC

- 경로 있음: 기존 사거리까지 접근
- 경로 없음 + 현재 위치에서 LoS와 사거리 충족: 사격
- 경로 없음 + 사격 불가: 사거리에 들거나 접근점에 닿을 때까지 접근점으로 이동하고, 그래도 사격할 수 없으면 Alert 후 포기(D-063)

접근점은 목표 쪽 가장자리일 수도 있고 아닐 수도 있다. 적이 섬 위, 플레이어가 지각에 있으면 섬 가장자리 안쪽에 모인다. 플레이어가 섬 위, 적이 지각에 있으면 지각에서 가장 가까운 점은 섬 바로 아래이므로 섬 밑에 모인다. 가장자리에서 밀려 떨어진 적은 착지 뒤 재귀속 규칙을 따른다.

전술 사격 위치 탐색은 구현하지 않는다. 플레이테스트에서 반복적으로 부자연스러운 상황이 확인될 때만 optional backlog로 올린다.

LoS는 SupportCache가 아니라 Chaos exact trace를 사용한다.

---

## 동굴 내 NPC 이동

동굴은 키트(공동 모듈 + 통로 1~2개)로 구성되며(`TerrainContract.md` §6), 통로와 공동 바닥은 지각과 Walk Portal로 연결된 별도 Nav Layer다.

```text
지각 Nav Layer
        │ Walk Portal (통로 입구 1~2개)
        ▼
통로 Nav Layer
        │
        ▼
공동 바닥 Nav Layer ── 기둥 등 Blocker 프랍
```

규칙:

- 입구 바닥이 실제로 연속되면 베이커가 Walk Portal 자동 생성
- 자동 검출이 불안정하면 명시적 Portal Actor로 보정
- 천장·벽은 exact collision 및 Nav blocker
- 공동·통로 바닥은 별도 Support Layer
- 수직 통로·복층·분기 제외
- 공동 내부 정적 프랍은 Nav bake에 포함
- 통로가 2개면 공동을 통과하는 순환 경로가 생기며 A*와 flow field 모두 그대로 다룬다
- 동굴로 들어간 플레이어는 지상 경로가 있으면 NPC가 추격

공동은 넓은 홀이라 통로만 좁다. 좁은 통로에서 Grid 해상도가 부족하면 해당 통로 조각에만 centerline corridor를 보조 데이터로 붙일 수 있으나 첫 구현은 같은 Tiled Nav 구조를 유지한다.

---

## 완전 비행 NPC

비행 NPC는 지상 NPC가 일시적으로 Fly 모드로 전환하는 형태가 아니다. 새·박쥐·드론처럼 처음부터 3D 이동을 전제로 하는 별도 archetype이다.

지상 NPC는 섬을 건너지 않으므로(D-011) 부유섬은 근접 적에게서 안전한 지대가 된다. 이 공백을 메우는 것이 원거리 NPC와 비행 NPC다. 그래서 비행 NPC는 섬 프로토타입 직후인 Phase 3c에서 구현한다. 의존성은 Phase 3의 exact sweep뿐이다.

```cpp
enum class ELNPNavigationDomain : uint8
{
    GroundSupport,
    FreeFlight,
    FlightCorridor,
    SurfaceCrawl    // 벽 타기 NPC(D-066, Phase 14)
};
```

### 1차 이동 방식

전역 3D voxel navigation을 만들지 않고 3D local planner를 사용한다.

```text
목표 방향 desired velocity
        ↓
전방 lookahead sphere sweep
        │
        ├─ clear → 그대로 이동
        └─ blocked → 후보 heading 평가
```

후보 방향:

- 좌/우 yaw
- 중심 쪽/지각 쪽 pitch
- 좌상/우상
- 좌하/우하
- 이전 프레임 회피 방향

점수 요소:

- target progress
- obstacle clearance
- turn cost
- preferred radial altitude
- 이전 회피 방향 유지

대부분의 NPC는 전방 sweep 하나만 수행하고 막힌 NPC만 추가 후보를 검사한다.

### 교착 복구

- 일정 시간 target progress가 없으면 후보 cone 확대
- 반대 radial band 시도
- 짧은 orbit/후퇴
- 그래도 실패하면 home 또는 다른 target 선택

### 구현 규약(Phase 3c, 상세는 `../phases/Phase03c_FlyingNpcFoundation.md`)

- **archetype**: PureEntity 전용(D-052). 같은 `ULNPEnemyTrait`에 `NavigationDomain = FreeFlight`면 `FLNPEnemyFlyingTag`가 붙고, 지상 이동·분리·격자는 이 태그를 거른다. 이동은 `ULNPEnemyFlightMovementProcessor`, 조향은 Mass 비의존 순수 함수 `LNPFlightSteering`(엘리트 Actor 재사용 대비)이다. 비행끼리 분리는 별도 격자의 3D 거리다.
- **고도(D-053)**: 비교전은 Home 위 `IdleAltitude` 대역 3D 배회, 교전은 타겟 위 `EngageAltitude`·올려다보는 각 `EngageElevationDeg`의 교전 지점. 쏠 수 있는 동안은 자리를 지키고, 사거리·조준 각도를 벗어나거나 LoS가 막힐 때만 재배치한다.
- **headroom(D-073)**: Phase 7c 단위 3은 비행 편성 Pod와 주변 비행 적 시작점의 headroom을 필터링한다. `ULNPEnemyConfig::GetSpawnRequiredHeadroom`은 `max(IdleAltitudeMin, IdleAltitudeMax) + max(CapsuleRadius, CapsuleHalfHeight) + Flight.Clearance + 200cm`를 요구한다. 몸 구는 실제 `TryLiftFlyingSpawn`과 같은 규칙이고 기본값은 2,638cm다. 지상 Config는 0이다. 게임 스레드에서 EntityConfig의 가까운 EnemyTrait부터 부모 체인을 읽고 실제 생성 수가 양수인 편성의 최댓값을 세트 입력에 담는다. worker는 float 입력만 읽는다.

  베이크는 Config와 무관하게 모든 authored anchor·random candidate에 `Headroom`을 기록한다. 반지름 50cm 구를 바닥점의 지역 Up(구 중심 방향)으로 `LNPWorldExact` sweep한다. 시작 중심 높이는 `50 / dot(Up, floorNormal) + 1cm`, 종료 중심 높이는 `min(32767 - 50, 바닥점 반지름 - 50 - 1)`이다. 바닥 관통·중심 통과와 복제 좌표 캡 초과를 피하며 시작 겹침·측정 공간 부족은 0이다. hit는 `시작 높이 + Hit.Distance + 50cm`, 무충돌은 `종료 높이 + 50cm`를 저장한다. 이는 구가 닿는 천장·옆벽의 보수적 수직 높이이며 반지름 50cm 기둥 바깥의 자유 공간을 보장하지 않는다. 실제 비행의 몸 구 sweep은 계속 장애물을 막는다. 값은 `[0,32767]`의 유한 float이고 무충돌을 infinity로 표시하지 않는다. 측정 반지름·상한은 bake settings hash에 포함하지만 비행 고도 Config·요구 여유는 포함하지 않는다.

  authored-for-set → generic → random 우선순위와 총량·shortfall은 유지한다. `Headroom >= RequiredHeadroom`이면 선택하며 낮은 후보를 소비하지 않아 이후 지상 편성에 남는다. 비행 없는 세트는 기존 배치와 같다. 주변 적은 필요 headroom 내림차순으로 배정해 지상 적이 비행 후보를 먼저 소비하지 않게 한다. 같은 필요값끼리는 Config 순서를 유지한다. 적 종류별 headroom과 기존 slot·Layer·반경·간격 제한은 계속 검사한다. Config 변경은 같은 snapshot 위의 할당만 바꾼다. 지하 공동·건물 안·섬 밑은 같은 규칙이다. 천장 낮은 공간의 추격은 기존 LoS·교착 복구로 입구 밖에서 호버하거나 물러나는 동작을 허용한다. 높은 공동의 천장 근처 교전 고도 clamp는 단위 4의 플레이 검증에서 필요할 때만 추가한다.
- **런타임 비행 스폰**: headroom은 후보 필터이며 실제 몸·옆벽 여유를 보장하지 않는다. `TryLiftFlyingSpawn`은 `max(CapsuleRadius, CapsuleHalfHeight) + Clearance` 구로 시작점부터 배회 고도 하한까지 검사한다. 시작 겹침은 hit 법선으로 깊이+0.5cm만큼 최대 4회 해소하고 다시 검사한다. 천장/벽 hit 앞에서는 0.5cm를 남긴다. 안전한 여유 구를 확보할 수 없으면 해당 엔티티를 제거하고 거부 로그를 남기며 겹친 시작점을 반환하지 않는다. 후보 계획의 배치 수와 실제 런타임 엔티티 수는 별도로 검증한다. 휴면 이동 생략 규칙을 유지하므로 스폰 시 안전성에 이동 processor의 사후 복구를 기대하지 않는다.
- **교전 수 상한**: 비행 적은 원거리 슬롯 풀을 쓰므로 동시 교전 수는 플레이어당 원거리 슬롯(20)으로 묶이고 나머지는 Alert로 호버한다. 지상 원거리 적과 같은 풀을 나눈다.
- **넉백**: 생산자(`ApplyEntityKnockback`)는 지상과 같고, 비행 소비는 중력 없이 반감기 감쇠·steering 추가 속도로 sweep. "`Velocity != 0`이면 공중"이라는 지상 규약은 비행 개체에 적용하지 않는다.
- **사망**: 비행을 끊고 `StepAirborne` 낙하, 착지 여부는 `bDeathLanded`로 기억한다.
- **예산(D-074, D-054 대체)**: Pod 귀속 PureEntity 비행 적의 임시 월드 편성 목표는 600이다. 활동 대역(D-067)·구간 배회 복제(D-068)를 전제로 한다. 현재 편성은 120 Pod 중 100 Pod에 비행 6기씩이며 지상 요청은 PureEntity 720개다. 월드 총수와 동시 활성/사격 수를 구분하고, 이 편성을 무귀속 전량 활성 혼합의 합격 상한으로 쓰지 않는다. 지상/비행/벽보행의 최종 총수·비율·활성 예산은 Phase 14 뒤 통합 측정으로 결정한다. 단독 900~1,200은 확대 후보이며 최대 총수는 미확정이다. 과거 비행 100기당 1.0~1.5ms는 활동 대역 도입 전 측정이라 현재 총수에 선형 적용하지 않는다. 실제 근거와 실패 조건은 `../history/Phase13_Log.md`가 소유한다.
- **진형 기각(D-069)**: 리더만 복제하고 멤버 위치를 추론하는 진형은 채택하지 않는다. 멤버마다 락온·HP 바·넉백·경직·패링이 그대로 적용돼야 하는데, 맞는 순간 멤버가 결정론적 자리를 벗어나 전투 규칙 전반에 예외가 생긴다. 대역폭 비용은 "복제 엔티티 수"가 아니라 "위치가 바뀐 갱신 횟수"이므로(`LNPMassReplicator`는 바뀐 항목만 Dirty), 엔티티를 복제에서 빼지 않고 갱신을 줄이는 D-067·D-068로 같은 이득을 얻는다. 상세는 `../../DiscardedApproaches.md` [Case 06].

### Flight Corridor 도입 조건

다음 요구가 생길 때만 추가한다.

- 박쥐가 긴 동굴 안쪽까지 추격
- 특정 입구를 찾아야 함
- 큰 U자형 장애물을 반드시 우회
- 도달 가능성을 보장해야 함

이 경우 전역 3D grid 대신 동굴 중심선 waypoint/spline을 사용한다.

---

## 활동 대역(D-067, Phase 13 단위 2)

> 상태: Phase 13 단위 1~5 완료. 최종 기능·성능 검증과 사용자 리슨 2P 확인을 통과했다. 임시 월드 편성은 D-074, 검증 근거와 한계는 `../history/Phase13_Log.md`를 따른다.

도입 전 지상·비행 이동은 거리와 무관하게 모든 적을 매 프레임 처리했다. 활동 대역으로 휴면 개체의 이동·인지·분리·경로·배회 처리를 제외한다. Phase 13 재측정과 사용자 승인으로 비행 임시 월드 편성은 D-074를 따른다. 기존 지상 단독 검증과 월드 혼합 편성은 별개이며 최대 총수·벽보행을 포함한 최종 배분은 Phase 14 뒤 측정한다.

**판정 단위는 Pod다.** 서버의 `ULNPEnemyActivityProcessor`가 PostPhysics에서 무리 상태를 확정하고 다음 PrePhysics 전에 휴면 태그를 반영한다. Pod와 가장 가까운 플레이어의 거리를 4Hz로 판정한다. 즉시 깨움과 강등 안전성은 매 프레임 읽으며 현재 구현은 개체 전체를 두 번 순회한다. 거리 판정이 저빈도라는 이유만으로 전체 관리 비용도 Pod 수에 비례한다고 보고하지 않는다.

| 대역 | 거리(현재 설정 기준) | 동작 | 근거 |
|:---|:---|:---|:---|
| 휴면 | Pod 거리 >132m 진입, ≤126m 복귀; 전 개체 가시거리 안전 검사도 통과 | 이동·분리·배회·인지·경로에서 제외한다 | 복제 컬 120m에 10%/5% 여유. 현재 시각화 Off 240m 때문에 실제 개체 거리도 >252m여야 휴면 가능 |
| 배회 | 활성·휴면 조건 사이 | 서버가 검증한 구간을 복제하고 양쪽이 시각 기반 보간 | 현재 가시거리를 보존하므로 복제 컬 밖에서도 배회할 수 있다 |
| 활성 | Pod 거리 ≤60m 진입, >66m 이탈, 또는 깨움 | 기존 서버 시뮬레이션 | 추격 반경 50m·시각화 Low 경계 60m를 덮는다 |

- **깨움**: 플레이어 접근, 무리 중 한 개체의 피격(장거리 탄·스플래시 포함), Pod 루팅 시작. 피격은 개체가 아니라 Pod 무리 전체를 깨운다.
- 휴면 개체도 피격 판정과 적 탐색 질의의 공간 격자에는 남는다. 맞을 수 있어야 깨울 수 있다.
- **Pod에 귀속되지 않은 NPC**(향후 월드 배회 보스)는 대역 판정 대상이 아니며 항상 활성이다. 개체 수가 적어 서버 시뮬레이션 비용을 감당할 수 있다. Pod가 없으면 "항상 활성"으로 처리하는 규칙만 지키면 되므로 보스 작업 전에 따로 만들 것은 없다.
- 거리 값은 상수로 두지 않고 기존 설정에서 유도한다(D-067). 휴면 경계는 적 복제 컬 거리(`ReplicationCullDistance`, 현재 120m)에 여유를 더한 값, 활성 경계는 적 Actor 스폰 거리(현재 60m)다. 그래야 한쪽을 바꿨을 때 휴면 개체가 보이는 구간이 생기지 않는다.
- 실제 EntityConfig의 자식 우선 visualization trait에서 Base/Visible Low 거리의 최댓값과 Off 거리의 최댓값을 읽는다. 호스트·싱글플레이 시각화 Off는 현재 240m로 복제 컬 120m보다 넓다. 에셋을 바꾸지 않고 각 개체가 `max(복제 컬, 시각화 Off)×1.05` 밖에 있을 때만 휴면을 허용한다. 유효 설정이나 지상 Support handle이 없으면 활성이다.
- 깨움은 10초 유지한다. 피격·스플래시·근접 판정의 깨움 플래그, 속도·공격·타겟·경직·Poise·Home 재확인·동적 기반·사망·실제 Actor 존재 중에는 무리 전체를 활성으로 둔다. Looting 중에도 활성이다. 새 Surface snapshot 게시 시 무리 전체를 한 번 깨우는 보수적 정책이며 지역별 축소는 아직 없다.
- Pod 무효/소멸/Popped 또는 귀속 해제 시 항상 활성으로 복귀한다. 휴면 진입은 진행 중 경로를 취소하고 이동·분리를 멈춘다. 복귀는 새 배회 목표와 StateTree 활성 신호를 요청한다. 피격·탐색 공간 격자와 기존 복제 수명은 유지한다.
- 측정: 도입 전에 비행 1기당 약 10~15us의 공통 비용을 Insights로 분해한다. 도입 뒤 Phase 6·3c와 같은 패키지 조건에서 총수를 다시 재고 D-054와 지상 한계를 새 결정으로 대체한다.

## 구간 배회 복제(D-068, Phase 13)

> 상태: 서버 구간 배회는 단위 3 구현. 복제 페이로드·양자화·wrap은 단위 4에서 확정한다.

### 서버 구간 배회(단위 3)

- `FLNPEnemyIdleFragment::WanderSegment`에 캡슐 중심 시작점·목표·서버 world time의 시작/도착/정지 종료 시각을 둔다. 짧은 구간의 Tile fingerprint는 최대 16개 고정 목록이며 한도를 넘으면 후보를 거부한다. 정지 시간은 3초, 속도는 기존 배회와 같은 이동 속도의 30%다. 위치는 시각 기반 선형 보간으로 계산하고 지상은 매 프레임 같은 Layer의 안전한 Support에 투영한다.
- `ULNPEnemyWanderProcessor`가 서버 PrePhysics에서 공격·경로 소비 뒤, 기존 지상·비행 이동 전에 실행된다. 처리한 개체의 기존 이동은 건너뛰고 Wander에서는 StateTree의 기존 목표 생성·지상 분리·일반 경로 요청을 제외한다. 인지·피격 질의·위치 복제는 유지한다.
- Pod 멤버의 handle index 순서로 부채꼴을 나눈다. 구간마다 부채꼴 내부 후보를 최대 네 번 추첨한다. 지상은 같은 Layer의 HighConfidence Support, overlay 포함 Nav 직선 보행 검사, 실제 보간 곡선의 Support 샘플과 capsule sweep을 구간 생성 때 확인한다. 비행은 기존 body radius와 clearance로 전체 구간을 sphere sweep한다. A* 우회 후보는 이번 배회 경로에 쓰지 않으며 후보가 모두 막히면 제자리에서 3초 뒤 다시 시도한다.
- 같은 Pod의 현재 위치·예약된 남은 선분과 구간이 교차하면 후보를 거부한다. 이 검사는 캡슐을 감싸는 구로 보수적으로 계산해 밀집 구간에서 정지가 늘 수 있다. 이미 겹친 출발점은 Active·그룹 깨움을 요청하여 기존 분리 경로로 복귀한다. 다른 Pod 사이의 예약을 전역으로 조율하는 기능은 없다.
- snapshot generation이나 경유 Tile revision 변경, 피격·교전·넉백·경직·사망·Actor 승격·Home 재확인·동적 기반·활동 대역 변경 때 구간을 취소한다. 취소는 현재 위치를 유지하고 기존 배회 목표를 다시 요청한다. 단위 2의 지형 게시 전체 그룹 깨움도 유지한다.
- 정적 구간은 보간 중 exact query를 하지 않는다. 동적 지지면이 있는 slot은 시작 때의 검증으로 미래 자세를 보장할 수 없으므로 실제 프레임 이동에 exact sweep을 추가한다. persistent level 동적 지지면(slot 없음)은 모든 slot을 위험으로 본다. 막히면 이동 전 취소하며 움직이는 패널을 계획적으로 이용하지 않는다. 동적 영향 범위를 slot보다 좁히는 최적화는 아직 없다.

### 복제와 클라이언트 소비(단위 4)

배회 대역의 이동은 서버가 경로를 한 구간씩 정한다. 구간 하나는 "다음 지점, 도착 시각, 도착 뒤 정지 시간"이며 구간이 시작될 때 한 번만 복제한다. 서버와 클라이언트는 같은 보간으로 위치를 계산하고, 구간 중에는 위치 갱신을 보내지 않는다.

- **무작위처럼 보이는 경로**: 구간마다 배회 지점을 새로 뽑으므로 경로가 반복되지 않는다. 오래 관찰해도 정해진 궤도로 보이지 않는다.
- **페이로드**: `FLNPReplicatedAgent::Wander`는 PositionYaw의 형제 필드다. 활성/비행/slot flags(uint8), 시작·목표 XYZ(int16 6개), 시작 서버 시각(uint32 ms), 이동/정지 길이(uint16 ms 각 하나), Layer(uint16), 고정 로컬 Yaw(int16)다. 좌표는 1cm, 시각은 1ms이며 각 길이의 한도는 65.535초다. 시작 시각은 약 49.7일에 wrap하며 수신 서버 시각에서 가장 가까운 epoch를 고른다(반주기 약 24.8일 미만 지연 전제). 범위를 넘으면 clamp하지 않고 활성 이동으로 복귀한다.
- **서버 정본**: 구간 생성 직후 양자화된 좌표·시각·Yaw를 서버 구간에 되쓴다. 생성 시의 고정 Yaw를 다시 인코딩하므로 구면 이동 중 현재 회전 변화가 구간 Dirty를 만들지 않는다. 좌표 반올림의 3D 오차는 1cm 미만이다. 구간 검증의 바닥 여유 2cm·예약 여유 10cm를 유지한다.
- **Dirty와 비용**: 구간 시작·변경·취소는 기존 갱신 주기를 우회한다. 같은 구간에서는 PositionYaw를 유지하고 HP·행동·공격·피격 변경은 기존 규약으로 전달한다. 일반 정수 Iris descriptor를 사용하며 native NetSerialize를 추가하지 않는다. 필드 폭 합계는 25바이트지만 change mask·FastArray·패킷 비용을 포함한 실측 전송량이 아니다. 대역폭 절감률과 초당 비용은 단위 5에서 측정한다.
- **경로 검증**: 지상·비행 후보 검증은 앞의 서버 구간 배회 규약을 따른다. 정적 구간 보간에서는 exact를 생략하지만 동적 지지면 slot의 프레임별 안전 sweep은 유지한다. 벽 타기는 Phase 14 범위다.
- **지면 보간**: 지상은 직선 위치를 같은 slot·Layer의 Support에 투영한다. snapshot generation은 머신별 게시 번호이므로 패킷에 싣지 않고 클라이언트 현재 generation으로 조회한다. snapshot 준비 전·조회 실패 때 현재 자세를 유지한다. 비행은 구간 직선을 그대로 소비한다.
- **겹침 방지**: 배회 중에는 분리력이 돌지 않으므로 같은 Pod의 개체끼리 배회 영역을 부채꼴로 나눈다.
- **수신과 이탈**: bubble 추가에는 현재 PositionYaw와 구간을 함께 시드한다. 중간 수신·late join은 GameState의 동기화된 서버 시각으로 현재 위상을 평가한다. 구간 중 HP 패킷이 와도 오래된 PositionYaw로 보간 목표를 덮지 않는다. 구간 취소는 현재 서버 자세를 전달하고 수신 현재 자세에서 기존 최소 50ms 보간으로 활성 이동에 복귀한다. 네트워크 지연으로 인한 보정 자체가 없다는 뜻은 아니다.
- **Lag Compensation**: 50ms 간격·11개 위치 이력에 구간·게시 generation·접지 높이·중력 원점을 함께 기록한다. 두 이력이 같은 구간이고 지상 snapshot generation이 일치하면 되감은 시각에 구간을 평가하며, 전환·지형 게시 경계에서는 기존 기록 위치 보간을 쓴다. 배회는 현재 서버 시각을 직접 소비하므로 투사체/근접 판정과 A/B 계측 모두 기존 LOD 보간 지연을 더하지 않는다. 핑 되감기 상한과 기존 근접 보정 적용 여부는 유지한다.
- **시간 오차**: 서버 시간 동기화 오차는 화면에 보이는 위치에만 영향을 준다(배회 속도 270cm/s, 오차 50ms면 약 13cm, 추정). 판정의 정본은 서버다.
- **연출**: 클라이언트는 구간 위상(이동/정지)으로 Idle/Move만 바꾼다. 공격·피격 등 일회성 행동을 덮지 않는다. 자동화로 행동 fragment 위상을 확인했고 사용자가 리슨 2P 배회 이동/정지·게스트 접근/이탈·원거리 저격에서 시각적 이상이 없음을 확인했다. 지상·비행 각각의 화면과 실제 bubble 재생성·late join은 최종 Gate에서 확인한다.
- **완전 결정론으로의 확장**: 클라이언트가 경로까지 생성하면 대역폭이 0이 되지만, 두 머신의 입력이 비트 단위로 같아야 한다(클라이언트 Pod 위치는 양자화돼 있고 PodID는 복제되지 않는다). 측정에서 대역폭 문제가 확인될 때만 도입하며, 같은 구간 구조를 그대로 이어 쓴다.

## 벽 타기 NPC(D-066, Phase 14)

> 상태: 초안. 실행 계획은 `../phases/Phase14_SurfaceCrawlNpc.md`다.

로봇청소기형 PureEntity는 벽·천장·섬 밑면을 기어 다닌다. 크기는 비행 드론과 비슷하다. 다리 애니메이션이 필요 없는 바퀴·흡착 구동 외형이라 거미형 대신 택했다(2026-10-05 사용자 결정). Support Atlas는 구 중심에서 뻗는 방향마다 높이를 담는 구조라 벽을 표현할 수 없으므로, 비행 NPC처럼 Support·Nav를 쓰지 않고 exact query만으로 움직인다. 정교한 길찾기는 하지 않는다. 플레이어가 보기에 다소 멍청해도 된다.

- **도메인**: `ULNPEnemyTrait`의 `NavigationDomain = SurfaceCrawl`. 지상 이동·분리·지상 격자에서 제외하고 별도 이동 프로세서가 맡는다. 조향은 `LNPFlightSteering`처럼 Mass에 의존하지 않는 순수 함수로 둔다.
- **이동 몸체는 구다**(2026-10-05 사용자 결정). 반지름은 `CapsuleRadius`이고 벽보행 Config는 `CapsuleHalfHeight = CapsuleRadius`로 둔다. 표면을 갈아탈 때 몸이 회전해도 구는 지형에 박히지 않아 모서리 처리에 관통 해소가 필요 없다. 피격 판정 캡슐은 다른 적처럼 Config 값을 쓴다.
- **이동(wall-walker)**:
  1. 목표를 현재 표면 법선의 접평면에 투영해 그 방향으로 걷는다.
  2. 전방 sweep이 걸을 수 있는 각도의 벽에 막히면 그 hit 법선으로 갈아탄다(오목 모서리).
  3. 이동 뒤 -법선 방향 probe가 바닥을 잃으면 뒤쪽 아래로 감싸 돈다(볼록 모서리).
  4. 그래도 표면이 없으면 기존 `StepAirborne`으로 구 중심 중력 낙하를 하고, 착지한 표면에 다시 붙는다.
  - 구현(Phase 14 단위 2, `LNPSurfaceCrawl`): 표면 법선은 구의 **접촉 법선**(접점 → 구 중심)이다. 평면에서는 면 법선이고 볼록 모서리에서는 모서리를 축으로 연속으로 돌아, 3의 감싸기가 대부분 -법선 probe 하나로 일어난다. 접평면 sweep은 법선 쪽으로 `StepUp`(15cm)만큼 띄워 턱을 넘고, 오목 모서리를 갈아탈 때 그만큼 내려앉는다. 한 프레임에 모서리를 지나친 경우만 probe 끝에서 이동 반대쪽 아래로 감싸기 sweep을 한 번 더 한다. 붙을 수 있는 면은 Known이면서 Support나 Blocker 역할이 있는 면이다.
  - 조향: 길찾기 없이 목표 방향을 접평면에 투영한다. 목표가 표면 앞쪽(법선 방향)으로 몸 반지름보다 멀면 그 거리만큼 내리막(중력 아래의 접평면 성분)을 더한다. 벽에 붙은 개체가 앞 바닥 목표와 같은 높이에서 투영 0으로 멈추지 않고 벽을 타고 내려가게 한다.
  - 도착 거리: 현재 표면의 접평면 거리에 법선 방향 거리 중 몸 반지름을 넘는 부분을 더한다. 접평면 거리만 쓰면 벽 위 개체가 바닥 목표와 같은 높이에서 도착으로 오판한다.
  - 회전 Z는 매 프레임 표면 법선과 같고 전방만 회전 속도로 보간한다. 표면을 갈아탄 프레임에는 법선이 돈 만큼 몸 전체를 같이 기울인다.
  - 비용: 정지 0회, 이동 2회(접평면 sweep + probe), 모서리 갈아타기·감싸기 프레임에 +1회다(자동화 실측, 평지 이동 60프레임 = 120회).
- **스스로 점프하지 않는다.** 넉백 낙하로는 다른 섬을 포함해 어디든 착지할 수 있다. 지상 NPC와 같은 규칙이다.
- **재귀속**: 착지한 뒤 Home 쪽으로 기어간다. 일정 시간 안에 가까워지지 않으면 직선거리로 가장 가까운 활성 Pod로 재귀속한다. Nav 도달성은 쓰지 않는다.
- **공격**: 원거리 투사체만 쓴다. 기존 PureEntity 원거리 경로(Windup → 발사, SalvoID, 게스트 Ghost)와 LoS 게이트(`bRequireLineOfSight`)를 재사용하고 사거리만 조정한다. 근접 공격과 덮치기는 없다.
- **인지**: 시야각 360°. 벽·천장에서는 몸 방향이 플레이어 쪽과 무관하므로 전방 FOV를 쓰지 않는다.
- **배회**: 활동 대역의 배회 대역에서도 서버 직접 시뮬레이션 대신 구간 배회 복제(D-068)를 쓴다. 구간은 같은 표면(법선 동일) 안의 짧은 직선이고 정지 시간을 길게 둔다(초안: 10초 중 약 1초만 이동). 구간 생성 때 sweep과 하향 probe로 한 번 검증한다. 클라이언트는 법선이 고정된 평면 위에서 보간한다.
- **"Up은 엔티티 회전의 Z축" 리팩터**: 지금 적 코드는 복제 회전 복원(`DecodeSphereRotation`), 공격 총구·조준, HP 바 위치, 애니메이션, 인지 FOV 등 약 80곳(13개 파일)이 "Up = 구 중심 방향"을 전제한다. 이를 엔티티 회전의 Z축에서 읽도록 통일한다. 지상·비행 적은 회전 Z가 원래 구 중심 방향이라 동작이 바뀌지 않는다. 착수 조사 결과 실제로 바꿀 곳은 적 자신의 몸 방향을 쓰는 약 16곳·9개 파일이다. 지상·비행 이동 내부의 중력·투영은 그 도메인의 물리이므로 구 중심 방향을 유지한다. 분류는 실행 문서가 소유한다.
- **복제**: 표면 법선을 octahedral 16bit 형제 필드로 추가하고, Yaw는 그 법선 기저의 로컬 Yaw로 인코딩한다. 법선은 표면을 갈아탈 때만 바뀌므로 평소에는 델타 압축으로 1비트다(`../../Guide_NetBandwidth.md` §2.3 형제 멤버 규약).
- **분리**: 접평면 거리 기반 지상 분리는 맞지 않으므로 비행처럼 3D 거리 격자를 쓴다.
- **개체 수**: 늘리고 싶지만 측정이 먼저다. Phase 3c §3.8과 같은 매트릭스(개체 수 단계, exact 호출 수·호출당 비용, 관통·Unknown hit 0)로 재고 예산을 결정 원장에 남긴다.
- **외형·애니메이션**: 로봇청소기형이라 다리 걷기 시퀀스 없이 정적 메시와 회전 부품으로 표현할 수 있는지 단위 5에서 먼저 확인한다(`../../TechDesign_EnemyNPC_LowLOD.md`).

---
