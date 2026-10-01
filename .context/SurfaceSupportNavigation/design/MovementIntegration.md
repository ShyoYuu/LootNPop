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

재귀속은 도달성과 path cost가 필요하므로 Phase 7에서 구현한다. Phase 6의 Enemy는 착지 후 기존 Parent Pod를 유지한다.

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
가장 낮은 path cost의 Pod로 Parent 갱신
```

직선거리만으로 고르면 절벽 건너편 Pod를 선택할 수 있으므로 다음 순서를 사용한다.

1. 활성 Pod만 후보
2. 같은 ReachabilityGroup의 후보만 유지
3. 직선거리로 소수 후보 축소
4. 실제 또는 근사 path cost로 최종 선택

후보가 없으면 `Orphaned` 상태로 둔다.

- 근처 플레이어가 있으면 현재 위치에서 전투
- 비전투 시 현재 StaticNavComponent 안에서 제한 배회
- 나중에 link가 열리거나 Pod가 생기면 재귀속
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

Phase 7에서 도달성과 함께 구현한다.

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
- **교전 수 상한**: 비행 적은 원거리 슬롯 풀을 쓰므로 동시 교전 수는 플레이어당 원거리 슬롯(20)으로 묶이고 나머지는 Alert로 호버한다. 지상 원거리 적과 같은 풀을 나눈다.
- **넉백**: 생산자(`ApplyEntityKnockback`)는 지상과 같고, 비행 소비는 중력 없이 반감기 감쇠·steering 추가 속도로 sweep. "`Velocity != 0`이면 공중"이라는 지상 규약은 비행 개체에 적용하지 않는다.
- **사망**: 비행을 끊고 `StepAirborne` 낙하, 착지 여부는 `bDeathLanded`로 기억한다.
- **예산(D-054)**: 비행 적 총수 200. 서버 프레임은 비행 100마리당 약 1.0~1.5ms이고 그중 exact는 0.1~0.3ms라 대부분 엔티티당 공통 비용이다. 이동 프로세서에 시뮬레이션 LOD가 없어 동시 교전 수가 아니라 총수로 예산을 잡는다. 이 근거는 서버 CPU이며 대역폭이 아니다. 활동 대역(D-067)과 구간 배회 복제(D-068)를 넣은 뒤 총수를 재측정한다.
- **진형 기각(D-069)**: 리더만 복제하고 멤버 위치를 추론하는 진형은 채택하지 않는다. 멤버마다 락온·HP 바·넉백·경직·패링이 그대로 적용돼야 하는데, 맞는 순간 멤버가 결정론적 자리를 벗어나 전투 규칙 전반에 예외가 생긴다. 대역폭 비용은 "복제 엔티티 수"가 아니라 "위치가 바뀐 갱신 횟수"이므로(`LNPMassReplicator`는 바뀐 항목만 Dirty), 엔티티를 복제에서 빼지 않고 갱신을 줄이는 D-067·D-068로 같은 이득을 얻는다. 상세는 `../../DiscardedApproaches.md` [Case 06].

### Flight Corridor 도입 조건

다음 요구가 생길 때만 추가한다.

- 박쥐가 긴 동굴 안쪽까지 추격
- 특정 입구를 찾아야 함
- 큰 U자형 장애물을 반드시 우회
- 도달 가능성을 보장해야 함

이 경우 전역 3D grid 대신 동굴 중심선 waypoint/spline을 사용한다.

---

## 활동 대역(D-067, Phase 13 예정)

> 상태: 대역 경계 유도 규칙은 확정(D-067, 2026-10-01). 프로세서 구성과 히스테리시스 폭은 Phase 13 실행 문서에서 정한다.

지상·비행 이동 프로세서에는 시뮬레이션 LOD가 없어서 서버가 거리와 무관하게 모든 적을 매 프레임 처리한다. 그래서 지금은 동시 교전 수가 아니라 총수가 예산이다(D-054, Phase 6 지상 한계 약 800). 활동 대역은 예산을 "플레이어 근처에 있는 수"로 바꿔 총수를 늘린다.

**판정 단위는 Pod다.** Pod에 귀속된 PureEntity는 Pod와 함께 움직이므로 Pod와 가장 가까운 플레이어의 거리로 무리 전체의 대역을 정한다. Pod 120개 × 플레이어 2~4명이라 판정 비용이 거의 없다. 판정은 저빈도(초당 몇 회)로 하고 무리의 태그를 한꺼번에 바꾼다. 경계마다 히스테리시스를 둔다.

| 대역 | 거리(현재 설정 기준) | 동작 | 근거 |
|:---|:---|:---|:---|
| 휴면 | 가장 가까운 플레이어 > 약 130m | 제자리에 서 있거나 떠 있다. 이동·분리·배회·인지 프로세서가 태그로 제외한다 | 적 복제 컬 거리 120m 바깥이므로 어떤 클라이언트에도 복제되지 않는다. 멈춘 모습이 보일 위험이 구조적으로 없다 |
| 배회 | 약 60~130m | Pod 주변을 구간 배회한다(아래 "구간 배회 복제") | 보이지만 아직 교전 거리가 아니다 |
| 활성 | 약 60m 이내, 또는 깨움 | 지금처럼 서버가 직접 시뮬레이션한다 | 추격 반경 50m·Actor 스폰 거리 60m를 덮는다 |

- **깨움**: 플레이어 접근, 무리 중 한 개체의 피격(장거리 탄·스플래시 포함), Pod 루팅 시작. 피격은 개체가 아니라 Pod 무리 전체를 깨운다.
- 휴면 개체도 피격 판정과 적 탐색 질의의 공간 격자에는 남는다. 맞을 수 있어야 깨울 수 있다.
- **Pod에 귀속되지 않은 NPC**(향후 월드 배회 보스)는 대역 판정 대상이 아니며 항상 활성이다. 개체 수가 적어 서버 시뮬레이션 비용을 감당할 수 있다. Pod가 없으면 "항상 활성"으로 처리하는 규칙만 지키면 되므로 보스 작업 전에 따로 만들 것은 없다.
- 거리 값은 상수로 두지 않고 기존 설정에서 유도한다(D-067). 휴면 경계는 적 복제 컬 거리(`ReplicationCullDistance`, 현재 120m)에 여유를 더한 값, 활성 경계는 적 Actor 스폰 거리(현재 60m)다. 그래야 한쪽을 바꿨을 때 휴면 개체가 보이는 구간이 생기지 않는다.
- 측정: 도입 전에 비행 1기당 약 10~15us의 공통 비용을 Insights로 분해한다. 도입 뒤 Phase 6·3c와 같은 패키지 조건에서 총수를 다시 재고 D-054와 지상 한계를 새 결정으로 대체한다.

## 구간 배회 복제(D-068, Phase 13 예정)

> 상태: 초안. 페이로드 양자화와 시간 기준은 Phase 13 실행 문서에서 확정한다.

배회 대역의 이동은 서버가 경로를 한 구간씩 정한다. 구간 하나는 "다음 지점, 도착 시각, 도착 뒤 정지 시간"이며 구간이 시작될 때 한 번만 복제한다. 서버와 클라이언트는 같은 보간으로 위치를 계산하고, 구간 중에는 위치 갱신을 보내지 않는다.

- **무작위처럼 보이는 경로**: 구간마다 배회 지점을 새로 뽑으므로 경로가 반복되지 않는다. 오래 관찰해도 정해진 궤도로 보이지 않는다.
- **대역폭(추정)**: 구간 4초 기준 개체당 약 3~4B/s다. 지금 배회 개체는 약 30~40B/s(갱신 12.6B × 2.4~3.3회/s)이므로 약 1/10이 된다.
- **경로 검증은 서버가 구간을 만들 때 한 번만 한다.** 지상은 같은 ReachabilityGroup의 정적 Nav node와 7b 직선 보행 검사(필요하면 A* 경유점), 비행은 sphere sweep 1회, 벽 타기는 같은 표면 안 sweep과 하향 probe다. 움직이는 패널은 쓰지 않는다. 프레임마다 하던 충돌 검사가 사라지므로 서버 CPU도 줄어든다.
- **지면 보간**: 지상 개체는 구간 직선 위치를 Support snapshot에 투영해 지면에 붙인다. 클라이언트도 같은 snapshot을 게시하므로(Phase 5) 추가 데이터가 없다.
- **겹침 방지**: 배회 중에는 분리력이 돌지 않으므로 같은 Pod의 개체끼리 배회 영역을 부채꼴로 나눈다.
- **이탈**: 피격·넉백·인지가 일어나면 즉시 활성 모드로 넘어가 일반 복제를 쓴다. 현재 위치는 양쪽이 같은 보간으로 계산한 값이라 넘어가는 순간 위치가 튀지 않는다. 전투 규칙(락온·HP·넉백·경직·패링)은 바뀌지 않는다.
- **Lag Compensation**: 되감은 시각으로 구간을 보간하면 서버 판정 위치가 나온다.
- **시간 오차**: 서버 시간 동기화 오차는 화면에 보이는 위치에만 영향을 준다(배회 속도 270cm/s, 오차 50ms면 약 13cm, 추정). 판정의 정본은 서버다.
- **연출**: 클라이언트는 구간 위상(이동/정지)에서 걷기·대기 애니메이션을 고른다.
- **완전 결정론으로의 확장**: 클라이언트가 경로까지 생성하면 대역폭이 0이 되지만, 두 머신의 입력이 비트 단위로 같아야 한다(클라이언트 Pod 위치는 양자화돼 있고 PodID는 복제되지 않는다). 측정에서 대역폭 문제가 확인될 때만 도입하며, 같은 구간 구조를 그대로 이어 쓴다.

## 벽 타기 NPC(D-066, Phase 14 예정)

> 상태: 초안. 실행 계획은 착수 직전에 `../phases/`에 만든다.

거미형(다족 보행 드론) PureEntity는 벽·천장·섬 밑면을 기어 다닌다. Support Atlas는 구 중심에서 뻗는 방향마다 높이를 담는 구조라 벽을 표현할 수 없으므로, 비행 NPC처럼 Support·Nav를 쓰지 않고 exact query만으로 움직인다. 정교한 길찾기는 하지 않는다. 플레이어가 보기에 다소 멍청해도 된다.

- **도메인**: `ULNPEnemyTrait`의 `NavigationDomain = SurfaceCrawl`. 지상 이동·분리·지상 격자에서 제외하고 별도 이동 프로세서가 맡는다. 조향은 `LNPFlightSteering`처럼 Mass에 의존하지 않는 순수 함수로 둔다.
- **이동(wall-walker)**:
  1. 목표를 현재 표면 법선의 접평면에 투영해 그 방향으로 걷는다.
  2. 전방 sweep이 걸을 수 있는 각도의 벽에 막히면 그 hit 법선으로 갈아탄다(오목 모서리).
  3. 이동 뒤 -법선 방향 probe가 바닥을 잃으면 뒤쪽 아래로 감싸 돈다(볼록 모서리).
  4. 그래도 표면이 없으면 기존 `StepAirborne`으로 구 중심 중력 낙하를 하고, 착지한 표면에 다시 붙는다.
  - 비용은 프레임당 exact 약 2회로 비행과 같은 급이다(추정).
- **스스로 점프하지 않는다.** 넉백 낙하로는 다른 섬을 포함해 어디든 착지할 수 있다. 지상 NPC와 같은 규칙이다.
- **재귀속**: 착지한 뒤 Home 쪽으로 기어간다. 일정 시간 안에 가까워지지 않으면 직선거리로 가장 가까운 활성 Pod로 재귀속한다. Nav 도달성은 쓰지 않는다.
- **공격**: 원거리 투사체만 쓴다. 기존 PureEntity 원거리 경로(Windup → 발사, SalvoID, 게스트 Ghost)와 LoS 게이트(`bRequireLineOfSight`)를 재사용하고 사거리만 조정한다. 근접 공격과 덮치기는 없다.
- **인지**: 시야각 360°. 벽·천장에서는 몸 방향이 플레이어 쪽과 무관하므로 전방 FOV를 쓰지 않는다.
- **배회**: 활동 대역의 배회 대역에서도 서버 직접 시뮬레이션 대신 구간 배회 복제(D-068)를 쓴다. 구간은 같은 표면(법선 동일) 안의 짧은 직선이고 정지 시간을 길게 둔다(초안: 10초 중 약 1초만 이동). 구간 생성 때 sweep과 하향 probe로 한 번 검증한다. 클라이언트는 법선이 고정된 평면 위에서 보간한다.
- **"Up은 엔티티 회전의 Z축" 리팩터**: 지금 적 코드는 복제 회전 복원(`DecodeSphereRotation`), 공격 총구·조준, HP 바 위치, 애니메이션, 인지 FOV 등 약 80곳(13개 파일)이 "Up = 구 중심 방향"을 전제한다. 이를 엔티티 회전의 Z축에서 읽도록 통일한다. 지상·비행 적은 회전 Z가 원래 구 중심 방향이라 동작이 바뀌지 않는다. 이 리팩터가 Phase 14의 가장 넓은 변경이다.
- **복제**: 표면 법선을 octahedral 16bit 형제 필드로 추가하고, Yaw는 그 법선 기저의 로컬 Yaw로 인코딩한다. 법선은 표면을 갈아탈 때만 바뀌므로 평소에는 델타 압축으로 1비트다(`../../Guide_NetBandwidth.md` §2.3 형제 멤버 규약).
- **분리**: 접평면 거리 기반 지상 분리는 맞지 않으므로 비행처럼 3D 거리 격자를 쓴다.
- **개체 수**: 늘리고 싶지만 측정이 먼저다. Phase 3c §3.8과 같은 매트릭스(개체 수 단계, exact 호출 수·호출당 비용, 관통·Unknown hit 0)로 재고 예산을 결정 원장에 남긴다.
- **외형·애니메이션**: ISKM 걷기 시퀀스가 필요하다. 에셋 방식은 착수 시 정한다(`../../TechDesign_EnemyNPC_LowLOD.md`).

---
