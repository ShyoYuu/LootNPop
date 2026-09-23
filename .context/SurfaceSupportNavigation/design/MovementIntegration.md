# NPC 이동·추격 통합 설계

> 상태: 초안
> 읽기 조건: 접지, airborne, 넉백, Pod 재귀속, 동굴 이동 또는 완전 비행 NPC를 구현할 때
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
- 경로 없음: 가장자리로 무작정 돌진하지 않음
- Alert 유지 후 포기 또는 다른 target 선택

#### 원거리 NPC

- 경로 있음: 기존 사거리까지 접근
- 경로 없음 + 현재 위치에서 LoS와 사거리 충족: 사격
- 경로 없음 + 사격 불가: Alert 후 포기

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
    FlightCorridor
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

### Flight Corridor 도입 조건

다음 요구가 생길 때만 추가한다.

- 박쥐가 긴 동굴 안쪽까지 추격
- 특정 입구를 찾아야 함
- 큰 U자형 장애물을 반드시 우회
- 도달 가능성을 보장해야 함

이 경우 전역 3D grid 대신 동굴 중심선 waypoint/spline을 사용한다.

---
