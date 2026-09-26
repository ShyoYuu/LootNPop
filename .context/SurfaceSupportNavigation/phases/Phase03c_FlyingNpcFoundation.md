# Phase 3c — 완전 비행 NPC 기반

> 상태: 진행 중(2026-09-26 착수) — 구현 단위 1·2 완료, 구현 단위 3 코드 작성
> 예상 범위: 2~3세션
> 선행 조건: Phase 3 exact sweep(완료), Phase 3b 부유섬 옥탄트(완료). 베이커·Nav와 무관(D-042)

## 1. 목표

지상 NPC는 섬을 건너지 않으므로(D-011) 섬 위는 근접 적에게서 안전하다. 비행 NPC로 그 공백을 메운다.

1. 전역 3D voxel 없이 **저비용 3D local planner**로 섬·지각·측벽을 피해 날아다닌다
2. **섬 위 플레이어와 교전한다.** 섬 밑면에 막힌 사격을 하지 않는다
3. 비행 개체 1마리당 exact 비용을 실측해 목표 개체 수와 예산을 정한다(사용자 결정: 측정 후 결정)

## 2. 현재 코드의 출발점

- 적은 `ULNPEnemyTrait` 하나로 조립한다. `ULNPEnemyConfig::CombatMode`(ActorPromoted·PureEntity)와 `AttackType`(Melee·Ranged)이 전투를 가른다.
- `ULNPEnemyMovementProcessor`(PrePhysics, Movement 그룹, 병렬 청크)가 Actor 없는 적의 이동을 모두 맡는다. 공중 상태는 `FLNPEnemyVelocityFragment::Velocity != 0`이고, 중력을 적분해 착지할 때까지 날아간다(D-049). 지속 비행은 없다.
- 격자 → 분리 → 이동 순서는 클래스 이름으로 건다(`../../TechDesign_EnemyNPC.md` §5.0). 분리력은 접평면 성분이다.
- PureEntity 원거리 공격(`ULNPEntityAttackProcessor`)은 Windup 끝에 한 번 쏘고 **LoS를 검사하지 않는다.** 조준 Pitch는 `AimPitchMin/MaxDeg`로 클램프한다.
- Mass 복제 페이로드는 3D 위치(int16×3)와 접평면 로컬 Yaw다(`Replication/LNPMassReplication.h`). 비행 개체 위치도 그대로 실린다. 몸의 pitch·bank는 실리지 않는다.
- `ULNPEnemySpatialGridSubsystem::ForEachNeighbor`는 방향 격자이고 각반경을 `Radius / 중심거리`로 잡는다. 구 중심에 가까운 비행 개체는 더 넓게 찾으므로 보수적이다.
- 배회 목표는 SurfaceCache 방향 + 같은 반지름 하향 probe다(`LNPEnemyStateTreeProcessors.cpp`). 비행에는 맞지 않는다.
- 스폰은 `ULNPMassSpawnConfig`의 Pod별 `AssociatedEnemies`(EntityConfig + Count)이며 SurfaceCache 첫 hit 둘레에 둔다.
- 비행형에 쓸 메시·EntityConfig·Config 에셋은 없다.

## 3. 확정 결정

### 3.1 범위와 archetype(D-052)

- **PureEntity 전용.** 적 구성 목표(90% 이상 PureEntity)에 맞고, ActorPromoted 비행은 Mover 비행 모드가 필요해 범위 밖이다. 비행 Config의 EntityConfig에는 Actor 표현 매핑을 두지 않는다(`ValidateTemplate` 경고 규약).
- **같은 `ULNPEnemyTrait`를 쓰고 이동 도메인만 가른다.** Config에 `NavigationDomain`(`ELNPNavigationDomain`, `../design/MovementIntegration.md`)을 두고, `FreeFlight`면 Trait가 `FLNPEnemyFlyingTag`를 붙인다. 타게팅·공격·HP·경직·복제·HP 바는 그대로 재사용한다.
- 지상 `ULNPEnemyMovementProcessor`와 `ULNPEnemySeparationProcessor`는 이 태그를 `None`으로 거른다. 비행 이동은 새 `ULNPEnemyFlightMovementProcessor`가 같은 페이즈·그룹에서 맡는다(분리 뒤, 격자 이름 간선 유지). 도메인은 스폰 후 바뀌지 않으므로 태그 이주가 없다.
- 이동 시뮬레이션은 서버 전용이다(`LNPMass::IsClientWorld()` 가드). 복제 페이로드는 바꾸지 않는다.
- **ActorPromoted 비행 엘리트는 후속 작업이다**(사용자 결정, 2026-09-26). 고도화된 StateTree와 공격 패턴을 가진 별도 구현이다. 3c는 이동·steering 로직을 엘리트가 재사용할 수 있게 **Mass에 의존하지 않는 순수 함수 모듈**로 둔다(§3.3). 엘리트 전용 추상화는 미리 만들지 않는다.

### 3.2 1차 공격 패턴: 원거리 사격(사용자 결정)

- 기존 PureEntity Ranged 경로(Windup 끝 1회 발사, SalvoID, 게스트 Ghost)를 그대로 쓴다.
- **발사 전 LoS 게이트를 추가한다.** Windup 진입 시 총구 → 조준점 `RaycastWorld` 1회. 막히면 공격을 시작하지 않고 교전 위치를 다시 잡는다(§3.4). 섬 밑에서 섬 위를 향해 쏜 탄은 전부 섬 밑면에 막히기 때문이다.
- **LoS 게이트는 모듈로 분리하고 Config 플래그로 켠다.** 판정은 `LNPEnemyLineOfSight::HasClearShot`(총구 → 조준점 exact raycast 1회, query 분류 `EnemyLineOfSight`)이고, 총구·조준점은 발사와 같은 `LNPEntityAttack::ComputeMuzzle`·`ComputeAimPoint`다. `ULNPEntityAttackProcessor`는 `FLNPEntityAttackConfig::bRequireLineOfSight`가 켜진 원거리 개체만 공격 시작(`None → Windup`) 전에 부르고, 막히면 공격을 시작하지 않고 `FLNPEntityAttackFragment::bLineOfSightBlocked`를 세운 뒤 0.25초 뒤 다시 본다(요청이 매 프레임 오므로 매 프레임 raycast하지 않는다). 반응은 이동 도메인이 맡는다 — 비행형은 교전 자리를 지키던 중 막히면 유지를 풀고 교전 측면을 타겟 기준 90° 돌린다. 접근 중의 막힘에는 반응하지 않는다(매번 돌리면 도착 전에 맴돈다). 3c에서는 비행 Config만 켠다. 지상 원거리형도 필요한 기능이지만 적용은 이 세션 범위가 아니다(사용자 결정). 나중에 플래그를 켜고 지상 이동의 재배치 반응만 붙이면 된다.
- 공중에서는 아래를 향해 쏘는 경우가 많다. `AimPitchMin`을 비행 Config에서 -90 가까이로 연다. 짐벌 수렴 방지는 `LNPSpread` 공용 기저를 그대로 쓴다.
- 급강하 근접은 제외 범위다.

### 3.3 3D local planner(D-019, `../design/MovementIntegration.md` "1차 이동 방식")

한 프레임 흐름:

1. **desired velocity**: 목표점(§3.4)을 향하는 방향 × 비행 속도, 목표점 근처에서는 감속한다.
2. **전방 lookahead sphere sweep 1회**: 현재 위치 → `위치 + 방향 × 속도 × LookaheadTime`. 구 반지름은 몸 반지름 + clearance다. 이번 프레임 이동 거리가 lookahead 길이보다 짧으므로 clear면 그 구간은 안전하다.
3. **blocked일 때만 후보 heading 평가**: 좌/우 yaw, 중심 쪽/지각 쪽 pitch, 좌상·우상·좌하·우하, 이전 회피 방향. 후보마다 같은 sweep을 하고 점수(target progress, clearance, turn cost, 선호 고도 이탈, 이전 회피 방향 유지)로 고른다.
4. **회피 방향 유지**: 고른 회피 방향은 짧은 시간(초안 0.25초) 캐시한다. 그동안에는 그 방향으로 lookahead 1회만 한다. 매 프레임 9 sweep이 나오지 않게 하려는 장치다.
5. **실제 이동**: 선택 방향으로 속도를 회전 제한과 함께 보간하고, 위치를 옮긴다. 모든 후보가 막히면 이동하지 않고 교착 복구로 넘긴다.

교착 복구(`../design/MovementIntegration.md` "교착 복구"): progress 없음이 일정 시간 이어지면 후보 cone 확대 → 반대 radial band → 짧은 후퇴 → 실패하면 Home 복귀.

구현(구현 단위 2, `LNPFlightSteering::Steer`):

- 진행은 "목표까지 거리가 50cm 이상 줄었는가"로 잰다. 목표가 300cm 이상 옮겨 가면(움직이는 타겟) 기준을 새로 잡는다.
- 막힌 프레임: 원하는 방향 주위 원뿔 45°의 후보 8개 + 직전 회피 방향을 sweep하고 `트임(트인 후보 2, 막힌 후보 hit Time) + 진행 + 0.3·회피 연속성 + 0.25·위아래 선호`로 고른다. 고른 방향은 0.25초 유지한다.
- 교착 1.5초: 원뿔 90°, 후퇴 방향(-전방) 후보 추가. 3초: 위·아래 선호 반전. 5초: `Stuck`을 돌려준다.
- `Stuck` 처리: 배회는 배회 타임아웃 경로로 목표를 다시 뽑고, 교전은 교전 측면을 타겟 Up 기준 90° 돌린다. 교전 측면(`FLNPEnemyFlightFragment::EngageSide`)은 재배치를 시작할 때 한 번 정하고 도착할 때까지 유지한다 — 매 프레임 현재 위치에서 다시 뽑으면 우회 중 목표가 따라 돌아 교착 판정이 성립하지 않는다.
- 개체별 기억은 `FSteeringState` 순수 구조체이고 Mass fragment가 멤버로 든다. 엘리트 Actor도 같은 구조체를 들면 된다.

규약:

- planner는 `LNPFlightSteering` 모듈의 순수 함수다(`LNPEnemyExactMovement`와 같은 형태). 입력은 `ULNPMassWorldCollisionSubsystem`, 파라미터 구조체, 개체별 steering 상태(회피 방향·유지 시간·progress 타이머)이고, 출력은 이번 프레임 속도다. Mass fragment를 직접 받지 않으므로 Mass 이동 프로세서와 자동화, 나중의 엘리트 Actor 경로가 같은 함수를 부른다.
- 몸 기준점은 캡슐 중심이다(`../../TechDesign_EnemyNPC.md` §5.1). 몸의 Up은 지상과 같이 구 중심 방향이고, 전방은 비행 방향의 접평면 성분이다. 복제 Yaw 규약과 맞는다.
- query 분류에 `FlightSteering`(생략 불가)을 추가한다. 후보 평가 sweep도 같은 분류다. LoS 게이트는 `EnemyLineOfSight`(생략 불가)를 추가한다. 두 분류는 `../design/RuntimeCollision.md` 쿼리 분류 표에 올린다.
- `UnknownExactSurface` hit는 막힘으로 취급한다(D-037).
- 비행 개체끼리의 분리는 **3D 거리 기반**이다. 지상 분리 프로세서의 접평면 규약을 비행에 섞지 않는다. 비행 개체는 지상 격자에서 빼고(지상 분리가 접평면 거리만 재 머리 위 드론이 지상 적을 밀어낸다) 같은 구현의 별도 인스턴스 `ULNPFlyingSpatialGridSubsystem`에 짓는다(구현 단위 2). `FlightConfig.SeparationRadius` 250cm·`SeparationStrength` 400cm/s, 분리력은 방향 선택과 무관하게 더하고 이동은 sweep을 거친다.

### 3.4 고도: 교전은 타겟 상대, 비교전은 Home 기준(D-053, 사용자 결정)

- **교전(Confirmed)**: 목표점은 타겟 위(구 중심 쪽) `EngageAltitude` + 드론이 있는 쪽 접평면으로 `EngageAltitude / tan(EngageElevationDeg)`만큼 떨어진 점이다. 타겟이 교전 지점을 올려다보는 각도가 `EngageElevationDeg`(기본 30°, 상한 45°)다. 전투 대부분이 플레이어 정면 기준 위로 50° 안에서 일어나게 한다(사용자 결정, 2026-09-26 — 타겟 바로 위를 목표로 하면 정수리 방향에 붙어 쐈다). 대역 안의 선회는 구현 단위 3이다. 섬 위 플레이어와 지각 위 플레이어를 같은 규칙으로 다룬다.
- **교전 위치 유지(사용자 결정, 2026-09-26)**: 추격은 교전 지점까지 가되, 도착한 뒤에는 "지금 자리에서 쏠 수 있는 동안"(사거리 안, 조준점이 `AimPitchMinDeg`~`AimPitchMaxDeg` 안) 자리를 바꾸지 않는다. 벗어날 때만 새 교전 지점으로 옮긴다(`FLNPEnemyFlightFragment::bHoldingFirePosition`). 플레이어가 드론 밑으로 조금 파고들 때마다 교전 지점으로 도망치는 것이 부자연스러웠기 때문이다.
- 비행형 `AimPitchMinDeg`는 -70°다. 발사 방향이 "몸 전방 + 타겟에서 뽑은 Pitch"이고 총구가 전방 100cm 앞이라, 타겟이 거의 바로 아래면 총구보다 뒤에 있어 탄이 앞으로 빗나갔다(-89°에서 관찰). 이 값이 발사 클램프·피격 인지 상하 게이트·교전 위치 이탈 판정의 공용 원본이다.
- LoS 게이트에 막히면 선회 각을 바꾸거나 고도를 올려 교전 위치를 다시 잡는다.
- **비교전(Idle·Alert)**: Home(`ParentLootPod` 위치) 위 `IdleAltitude` 대역에서 배회한다. 배회 목표는 SurfaceCache·하향 probe를 쓰지 않는 3D 점이다. 도달 불가 목표는 기존 `WanderTimeout` 복구 경로를 그대로 탄다.
- 세력권(`ChaseRadius`)과 재발견 금지 창은 지상과 같다.
- 수치(초안, 구현 중 조정): 비행 속도 900cm/s, `LookaheadTime` 1.0초, clearance 50cm, `EngageAltitude` 500~1,000cm(가운데 750이면 수평 약 1,300cm·직선 약 1,500cm), `IdleAltitude` 지각 위 1,300~2,300cm. 첫 값(교전 800~1,500, 배회 2,000~3,500)이 너무 높아 사용자 PIE 뒤 약 65%로 낮췄다(2026-09-26). 섬 윗면(지각 위 1,500~4,000cm)과 겹치는 대역이라 회피가 실제로 일어난다.

### 3.5 피격·경직·사망

- 넉백: **생산자는 바꾸지 않고 소비만 다르게 한다.**
  - 생산자: 발사체·근접 판정·스플래시·패링이 `LNPHitDetection::ApplyEntityKnockback`으로 `FLNPEnemyVelocityFragment::Velocity`에 `(공격 반대 방향 0.7 + Up 0.3) × 세기`를 덮어쓴다. 비행 개체도 같은 fragment를 가지므로 판정 코드는 비행을 알 필요가 없다.
  - 지상 소비: `Velocity != 0`이 곧 공중 상태다. 중력으로 포물선을 그리고 착지하면 0이 된다.
  - 비행 소비: 중력을 적용하지 않고 지수 감쇠시킨다(`FlightConfig.KnockbackHalfLife` 0.25초, 속도 1,200cm/s면 밀리는 거리 약 430cm). 이동은 steering의 추가 속도로 더해져 같은 sweep에 막힌다. 벽·섬에 닿으면 여유 앞에 멈추고, 미끄러뜨리지는 않는다(0.5초 안에 감쇠해 끝나므로). `KnockbackStopSpeed`(50cm/s) 아래로 떨어지면 0으로 끊는다. 넉백 속도가 남아 있는 동안 원하는 속도에 `1 - |넉백|/FlightSpeed`를 곱한다. 맞는 순간 경로 제어를 잃고 밀려나는 느낌을 주기 위해서다.
  - 중력을 쓰지 않는 이유: 쓰면 날던 개체가 지면까지 떨어지고, 이륙 로직이 새로 필요해진다. 또 "`Velocity != 0`이면 공중"이라는 지상 규약이 비행 개체에서는 성립하지 않는다(비행 개체는 늘 공중이다). 그래서 이 fragment는 비행 개체에서 "외부에서 받은 속도" 의미로만 읽는다.
  - Up 0.3 성분은 지상에서 즉시 흡수되지 않게 하려고 넣은 값이다. 비행 개체에서는 살짝 떠오르는 효과만 낸다. 거슬리면 비행 소비 쪽에서 Up 성분을 줄인다(생산자는 그대로 둔다).
- 경직·다운: 제자리 호버, 공격 중단은 기존 `ULNPEntityAttackProcessor` 규약을 따른다.
- 사망: 비행을 끊고 중력 낙하(`LNPEnemyExactMovement::StepAirborne` 재사용), 지면이나 섬에 닿으면 멈추고 `PureEntityDeathDuration` 뒤 소멸한다. 착지 여부는 `FLNPEnemyFlightFragment::bDeathLanded`로 기억한다 — 속도 0을 "착지"로 읽으면 사망 팝이 0인 개체가 공중에 멈춘다. 비행형 `PureEntityDeathPopSpeed`는 300cm/s(기본 2,000이면 1,000cm 솟구친다). 배회 고도 상한에서 착지까지 약 1.7초로 소멸 시간 2.2초 안이다. 사망 중인 엔티티를 쿼리에서 빼지 않는 규약(`../../TechDesign_EnemyNPC.md` §5)을 비행 프로세서도 지킨다.

### 3.6 외형: 블렌더 새 메시(사용자 결정)

- 블렌더 MCP로 저폴리 **static mesh**를 만들어 ISM으로 그린다. 품질은 요구하지 않는다(사용자 결정). 드론이 연상되고 다른 오브젝트와 구분되면 된다(몸통 + 로터 4개 정도). 스켈레톤·ISKM 시퀀스는 1차에서 쓰지 않는다. 행동 상태 연출(공격 예고 등)이 필요하면 스케일 펄스·VFX로 처리하고 구현 단위 3에서 판단한다.
- 왕복 규약은 `blender-mcp-ue-roundtrip` 메모리를 따른다(좌표 규약, `transform_apply`, EMPTY 금지).
- 방향 규약은 마네킹과 같다(사용자 결정, 2026-09-26): 에셋 공간 전방 = +Y, EntityConfig ISM `transformOffset` Yaw -90°로 Actor 전방 +X에 맞춘다. 외부 캐릭터 에셋 관행과 같게 두어 헷갈리지 않게 한다.
- 충돌 크기는 Config 캡슐(`CapsuleRadius`·`CapsuleHalfHeight`)이 피격 판정의 단일 정의다. 메시는 그 캡슐 안에 들어가게 만든다.
- 사망 연출(사용자 요청, 2026-09-26): 죽으면 센서가 꺼진다. 센서 머티리얼(`M_EnemyDroneEye`)이 `PerInstanceCustomData[0]`(기본 1)을 발광에 곱하고 표면색을 어두운 회색과 보간한다. 값은 행동 상태가 `Dying`이면 0이라, 게스트도 이미 복제받는 값으로 추가 네트워크 비용 없이 같은 연출을 본다.
- 인스턴스별 커스텀 데이터는 한 ISM의 모든 인스턴스에 transform과 **같은 순서로** 넣어야 해서, 엔진 `UMassUpdateISMProcessor`를 `DefaultMass.ini`에서 끄고 같은 동작의 `ULNPUpdateISMProcessor`로 대체했다. 비행 태그 청크만 transform 바로 뒤에 커스텀 데이터를 붙인다. 그 밖의 ISM 개체 처리는 엔진과 같다.

### 3.7 스폰

- Pod `AssociatedEnemies`에 비행 EntityConfig를 추가한다. 스폰 위치는 기존 지면 점에서 Up으로 `IdleAltitude` 하한만큼 띄운다. 지면 점에서 위로 sphere sweep 1회를 하고, 섬 밑면에 막히면 그 아래(여유 구가 닿는 자리)에 둔다. 재시도 없이 한 번에 정해져 결정론적이다(`ULNPMassSpawnSubsystem::LiftFlyingSpawn`, 구현 단위 1).
- 부하 harness `-LNPLoadBaseline`에 비행 개체 수 인자를 추가한다(§3.8).

### 3.8 측정(사용자 결정: 측정 후 예산 결정)

측정 build 규약은 3b와 같다(패키지 Development, 호스트 `-nullrhi` 서버 CPU 프레임, `Scripts/Profiling/RunLoadBaselineMatrix.ps1`).

| 항목 | 값 |
|:---|:---|
| 배치 | slot 4 큰 섬 둘레. 비행 개체가 섬 측벽·밑면 사이를 지나도록 Home을 섬 가장자리 아래 지각에 두고, 플레이어는 큰 섬 위에 둔다 |
| 요인 | 비행 N ∈ {100, 300, 500}, 지상 0 · 병렬 기본값. 혼합 1회: 지상 500 + 비행 100 |
| 기록 | 서버 CPU 프레임 P50/P95, `FlightSteering`·`EnemyLineOfSight` 개체당 프레임 query 수와 호출당 비용, blocked 비율, 교착 복구 진입 수, `UnknownHits`, `EnvelopeEscapes`, 섬 관통(아래 참고) |
| 관통 검출 | 비행 개체 캡슐이 exact 지형과 겹친 프레임 수(검증용 overlap, `DebugValidation` 분류, 측정 CVar로만 켬). 0이어야 한다 |

결과를 보고 목표 동시 비행 수와 비용 예산을 사용자와 정해 `Decisions.md`에 한 줄로 남긴다.

## 4. 구현 단위

1. **비행 archetype 골격과 메시**: `ELNPNavigationDomain`·Config 필드, `FLNPEnemyFlyingTag`, 지상 이동·분리 쿼리에서 제외, `ULNPEnemyFlightMovementProcessor`(목표점 직선 비행 + lookahead sweep 정지까지), 블렌더 드론 메시·ISM 표현, `DA_Enemy_PureEntity_Flyer01`·EntityConfig, Pod 스폰. 검증: PIE에서 스폰·호버·배회, 게스트 복제 위치 일치, 지상 적 회귀 없음(자동화 21개)
2. **3D local planner**: 후보 heading 평가·회피 방향 유지·교착 복구, query 분류 `FlightSteering`, 비행 개체 3D 분리. 검증: 자동화(섬 측벽 우회, 섬 밑면 아래 통과, 오목 지형 교착 복구, Unknown hit 비관통)
3. **교전**: 타겟 상대 고도·선회, LoS 게이트(`EnemyLineOfSight`), 비행 Config Pitch 범위, 넉백·경직·사망 낙하. 검증: PIE로 큰 섬 위 플레이어가 공격받는 것, 섬 밑에서 헛사격하지 않는 것, 사망 낙하 착지 확인
4. **측정**: harness 비행 인자, §3.8 매트릭스, 결과를 `../history/Phase03c_Log.md`에 기록하고 예산 결정
5. **기능 점검과 Phase 종료**: 사용자 PIE, 2P 스모크, 문서 갱신

## 5. 완료 조건

- [ ] 비행 개체가 PureEntity로 스폰·배회·교전하고 지상 적 동작이 바뀌지 않음(자동화 통과)
- [ ] 비행 개체가 섬·지각·측벽을 관통하지 않음 — 자동화, 측정 관통 프레임 0, `UnknownHits=0`
- [ ] 막힌 비행 개체가 교착 복구로 빠져나오거나 Home으로 돌아감 — 자동화
- [ ] 큰 섬 위 플레이어가 비행 개체에게 사격받고, LoS가 막힌 발사가 없음 — PIE, LoS 거부 수 기록
- [ ] 넉백·경직·사망 낙하가 비행 개체에서 동작함
- [ ] §3.8 매트릭스 기록과 목표 개체 수·예산 결정
- [ ] `LootNPopEditor Win64 Development`와 `LootNPop Win64 Development` 성공, 자동화 통과
- [ ] `-game` 리슨 서버 2P 스모크(D-031): 게스트에서 비행 개체 위치·Ghost 발사체·HP 바 정상, ensure 0
- [ ] `../Current.md`, `../Roadmap.md`, `../history/Phase03c_Log.md`, `../design/MovementIntegration.md`, `../design/RuntimeCollision.md`(쿼리 분류), `../../TechDesign_EnemyNPC.md`(프로세서 표) 갱신

## 6. 제외 범위

- ActorPromoted 비행(Mover 비행 모드)
- 급강하 근접 공격
- Flight Corridor(동굴 중심선 waypoint) — `../design/MovementIntegration.md`의 도입 조건이 생길 때
- 몸 pitch·bank 복제, 스켈레탈 애니메이션
- 지상 원거리 적의 LoS 게이트
- 비행 개체용 캐시·배치 최적화(측정 결과가 예산을 넘을 때 별도 판단)
