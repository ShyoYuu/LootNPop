# Phase 3c 작업 로그 — 완전 비행 NPC 기반

> 상태: 진행 중
> 실행 문서: `../phases/Phase03c_FlyingNpcFoundation.md`

## 2026-09-26 착수·실행 문서

- 실행 문서 작성. 사용자 결정: PureEntity 전용(ActorPromoted 비행 엘리트는 고도화된 StateTree·공격 패턴으로 후속 별도 구현, 이동·steering은 재사용 가능하게만), 1차 공격 원거리 사격 + 비행형 전용 LoS 게이트(모듈화해 지상 원거리형에 나중에 켤 수 있게), 고도는 교전 타겟 상대·비교전 Home 기준, 외형은 블렌더 저폴리 드론(품질 불요), 목표 개체 수·예산은 측정 후 결정.
- `Decisions.md` D-052(비행 NPC 구성·LoS), D-053(고도 기준) 추가.
- 넉백은 생산자(`ApplyEntityKnockback`)를 바꾸지 않고 비행 소비만 다르게 한다(중력 없는 감쇠, sweep, 조종 약화) — 실행 문서 §3.5.

## 2026-09-26 구현 단위 1 — 비행 archetype 골격과 메시

### 코드

- `ELNPNavigationDomain`(GroundSupport·FreeFlight), `FLNPEnemyFlightConfig`, `ULNPEnemyConfig::NavigationDomain`·`FlightConfig`·`IsFlying()`.
- `FLNPEnemyFlyingTag`를 Trait가 붙인다. `ValidateTemplate`이 FreeFlight + ActorPromoted 조합을 경고한다.
- 지상 이동·분리·격자 프로세서가 비행 태그를 None으로 거른다. 격자에서 뺀 이유: 유일한 소비처(지상 분리)가 접평면 거리만 재 머리 위 비행 개체가 지상 적을 밀어낸다.
- 피격 반응 시계를 `FLNPEnemyFragment::TickReactionTimers`로 뽑아 지상·비행 이동 프로세서가 공유한다.
- `LNPFlightSteering`(Mass 비의존 순수 함수): `ComputeArrivalVelocity`, `Step`(lookahead sphere sweep 1회, 막히면 여유 앞 정지, 시작 겹침은 풀기만, 호버는 query 0).
- `ULNPEnemyFlightMovementProcessor`: 비교전은 IdleTask 3D 배회점, 경계는 호버·주시, 교전은 타겟 위 교전 고도 가운데 값. 도착·배회 타임아웃은 3D 거리로 지상과 같은 신호 규약. 경직·공격 중 호버. 사망은 `StepAirborne` 낙하. 병렬 CVar를 지상과 공유.
- IdleTask: 비행형은 Home 위 `IdleAltitude` 대역 3D 점, 완료 판정 3D 거리.
- 스폰: `ULNPMassSpawnSubsystem::LiftFlyingSpawn` — 지면점에서 위로 sphere sweep 1회, 섬 밑면에 막히면 그 아래. 실행 문서의 "overlap + 재시도"를 이 방식으로 바꿨다(결정론적, 재시도 없음).
- query 분류 `FlightSteering`(필수, `PeriodicGroundValidation` 앞).
- 자동화 `LootNPop.SurfaceNavigation.FlightSteering.LookaheadStop`: 빈 공간 도착, 벽 앞 여유 정지·비관통, 여유 안 시작 시 밀려남, Unknown 벽 막힘, 호버 query 0.

### 에셋

- `Art/Meshes/SM_EnemyDrone01.fbx`(블렌더, 몸통 + 팔·로터 4 + 전방 발광 센서, 폭 ±64cm·높이 ±21cm, 도달 반경 79.5cm) → `/Game/Enemy/Drone/SM_EnemyDrone01`, 머티리얼 `M_EnemyDroneBody`·`M_EnemyDroneEye`(ISM 사용 플래그).
- `DA_Enemy_PureEntity_Flyer01`(Ranged01 복제): FreeFlight, 캡슐 80/80, AttackRange 2500, AimPitchMin -89, 총구 (100,0,-20), 시야 5000·240°, 초근접 인지 1500, 유지·세력권 6000, 배회 500~2500, ActionSequences 비움.
  - 시야를 넓힌 이유: 시야 판정이 3D 거리·접평면 전방 기준 3D 각도라 배회 고도(2000~3500)에서는 기존 값(2000·90°)으로 발견이 불가능하다.
- `DA_EnemyEntityConfig_PureEntity_Flyer01`: 드론 ISM, 오프셋 없음, LOD StaticMeshInstance×3 + None, 스켈레탈 제거. 복제 시 configGuid가 새로 발급됨을 확인.
- `DA_MassSpawnConfig`: Pod당 비행형 2(120 Pod, 240마리).

### 검증

- `LootNPopEditor Win64 Development` 성공(경고 없음).
- 자동화 `LootNPop.SurfaceNavigation` 22/22(기존 21 + 신규 1).
- PIE `TestMap03`(적 기본 편성 + 비행 240): 약 78초 동안 `FlightSteering` 325,162회(초당 약 4,100, 비행 1마리당 프레임 약 0.3회), 평균 3.71us, hit 2.4%, `UnknownHits=0`, `EnvelopeEscapes=0`, ensure 0.
- 미확인: 육안(드론 전방이 +X인지, 호버·배회 모습), 게스트 복제 위치 일치(2P 스모크).

### 사용자 PIE 피드백과 수정(같은 날)

- 센서가 전방이 아니라 드론 기준 오른쪽(+Y)에 있었다. 블렌더 -Y가 UE +Y로 들어온다(정적 메시 FBX, `axis_forward='-Z'`·`axis_up='Y'`). 블렌더에서 Z +90° 회전 적용 뒤 재익스포트, 센서 중심 블렌더 (0.39, 0, 0). 같은 경로 재임포트는 거부되므로 `_R`로 임포트 → EntityConfig 참조 교체 → 옛 메시 삭제 → 원래 이름으로 이동(리다이렉터 없음).
- 배회·교전 고도가 너무 높다 → 약 65%로: 배회 1,300~2,300, 교전 500~1,000.
- 교전 시 플레이어 정수리 방향(거의 90°)에 붙어 쐈다. 목표점이 타겟 바로 위였기 때문이다. `EngageElevationDeg`(기본 30°, 상한 45°)를 추가해 드론이 있는 쪽으로 `고도 / tan(각)`만큼 떨어진 점을 교전 지점으로 바꿨다. 목표: 플레이어 정면 기준 위로 50° 미만에서 대부분의 전투.
- 이어서 사용자 결정: 마네킹처럼 에셋 전방 = +Y, ISM `transformOffset` Yaw -90° 관행을 따른다(외부 캐릭터 에셋과 같은 규약). 블렌더 회전을 되돌려 센서를 다시 블렌더 -Y(UE +Y)에 두고 재익스포트했다. UE 재임포트와 오프셋 설정은 빌드 뒤 에디터에서 한다.
- 반영 완료: 센서 +Y 메시로 재교체(`_R` 경유, 리다이렉터 없음), `DA_EnemyEntityConfig_PureEntity_Flyer01` ISM `transformOffset` Yaw -90°. 비행 Config에 새 기본값(배회 1,300~2,300, 교전 500~1,000, `EngageElevationDeg` 30)이 들어간 것을 확인했다. `LootNPopEditor` 빌드 성공, 자동화 22/22.

### 사용자 PIE 2차 피드백(같은 날)

- 센서 방향·고도·교전 각도 모두 좋음. 다만 플레이어가 드론 밑으로 들어가면 드론이 도망친다 — 이미 사거리 안이면 각도에 맞춰 자리를 억지로 바꿀 필요가 없다. 발사 각도를 벗어날 때만 다시 잡는다(사용자 결정).
- 원인: 교전 중 목표점이 매 프레임 30° 교전 지점이었다. 정수리 위에서 명중하지 못하던 것은 발사 방향이 몸 전방 기준이라 총구(전방 100cm)보다 뒤에 있는 타겟을 앞으로 빗맞히기 때문이다.
- 수정: `FLNPEnemyFlightFragment::bHoldingFirePosition` 히스테리시스(교전 지점 100cm 안 도착 + 쏠 수 있음 → 유지, 사거리·조준 각도 이탈 → 재배치), 비행형 `AimPitchMinDeg` -70°.
- 검증: `LootNPopEditor` 빌드 성공, 자동화 22/22, PIE `TestMap03` 약 60초 `FlightSteering` 652,856회·평균 2.99us·hit 3.6%, `UnknownHits=0`, `EnvelopeEscapes=0`, ensure 0. 교전 위치 유지의 체감은 사용자 PIE 확인 대기.

### 구현 단위 1 마무리(같은 날)

- 사용자 PIE: 교전 위치 유지·센서 방향·고도·교전 각도 모두 자연스러움. 구현 단위 1 종료.
- 에디터 바이너리 `-game` 리슨 2P(기본 스폰, 비행 240 포함, 약 4분, 로그 `Saved/Logs/Smoke3c_U1`): 호스트·게스트 ensure·크래시·`LogLootNPop`/`LogMass` 오류 0(기존 `CharacterMovementComponent` 추출 오류 제외). 게스트는 접속 후 적 프록시를 받았다. Mover 시작 위치 경고는 이전 스모크와 같은 종류다.
- 한계: 게스트 쪽 드론 위치를 로그로 확인하는 수단이 없다(행동 상태 로그는 `-game`에서 꺼짐). 게스트 육안 확인은 Phase 종료 2P 스모크(D-031)에서 한다. 데스크톱 전체 캡처는 사용자 화면의 다른 창을 담으므로 쓰지 않는다.

## 2026-09-26 구현 단위 2 — 3D local planner

- `LNPFlightSteering::Steer`와 `FSteeringState`: 막히면 후보 heading 평가·0.25초 유지, 교착 1.5초 원뿔 90°+후퇴, 3초 위아래 선호 반전, 5초 `Stuck`.
- 비행 이동 프로세서가 `Step` 대신 `Steer`를 부른다. `Stuck`이면 배회 재추첨·교전 측면 90° 회전. 교전 측면을 재배치 동안 고정(`EngageSide`).
- 비행 전용 격자 `ULNPFlyingSpatialGridSubsystem`(격자 서브클래스, 같은 격자 프로세서가 짓는다)과 3D 분리(`FlightConfig.SeparationRadius` 250·`SeparationStrength` 400).
- 자동화 `FlightSteering.DetourAndStuck`: 섬 크기 블록 우회 도착·비접촉, 블록 속 목표는 `Stuck`·비관통.
- 검증: `LootNPopEditor` 빌드 성공, 자동화 23/23, PIE `TestMap03`(비행 240) `FlightSteering` 779,577회·평균 3.11us·hit 388(0.05%, 구현 단위 1 PIE는 3.6%. 원인은 확인하지 않았다 — 스폰 배치가 실행마다 달라 같은 조건 비교가 아니다), `UnknownHits=0`, `EnvelopeEscapes=0`, ensure 0. 우회·분리의 체감은 사용자 PIE 확인 대기.
- 사용자 PIE: Pod당 드론 10(120 Pod, 총 1,200)으로 늘려 섬 아래·경사로 아래 관통 동굴(유일한 동굴형 지형) 근처에서 확인 — 드론끼리 겹치지 않고 자연스럽게 우회해 온다. 스폰 편성 Pod당 10을 기본으로 유지(사용자 결정). 구현 단위 2 종료.

## 2026-09-26 구현 단위 3 — 교전 마무리

- LoS 게이트: `LNPEnemyLineOfSight::HasClearShot`, `FLNPEntityAttackConfig::bRequireLineOfSight`(기본 끔, 비행형만 켬), query 분류 `EnemyLineOfSight`, 공용 `LNPEntityAttack::ComputeAimPoint`(발사도 같은 함수로 교체). 막히면 공격 미시작 + `bLineOfSightBlocked` + 0.25초 뒤 재시도.
- 비행 반응: 교전 자리 유지 중 막히면 유지 해제·교전 측면 90° 회전. 접근 중 막힘은 무시.
- 넉백: `ApplyEntityKnockback`이 쓴 속도를 비행 프로세서가 반감기 0.25초로 감쇠, steering 추가 속도로 sweep, 조종 `1 - |넉백|/FlightSpeed`.
- 사망: `bDeathLanded`로 착지까지 낙하(속도 0이어도 중력 적분). 비행형 사망 팝 300cm/s.
- 자동화 `EnemyLineOfSight.Gate`: 섬 밑면 너머 막힘, 옆 트임, Unknown 막힘, 판정 1회 = query 1회.
- 검증: `LootNPopEditor` 빌드 성공, `DA_Enemy_PureEntity_Flyer01` `bRequireLineOfSight` 켬, 자동화 24/24. PIE `TestMap03`(드론 1,200) `FlightSteering` 평균 3.55us, `UnknownHits=0`, `EnvelopeEscapes=0`, ensure 0. 플레이어가 교전하지 않아 `EnemyLineOfSight` query는 0 — 게이트·넉백·사망 낙하의 실제 동작은 사용자 PIE 확인 대기.

### 사용자 PIE 피드백(구현 단위 3)

- 섬 밑·계단 밑 등 입체 지형 주변으로 도망쳐도 드론이 사격 가능한 위치를 찾아가 공격한다. 런처 유탄 넉백 뒤 조종 회복도 자연스럽다.
- 사망 팝(300cm/s, 높이 약 22cm)은 "살짝 움찔" 정도로 미미하다. 낙하 후 지면에 닿는 순간이 딱 멈춰 어색하다 — PureEntity라 물리 시뮬레이션이 없어 받아들이되, 착지 연출(짧은 바운스·기울기)은 후속 연출 과제로 둔다.
- 요청: 사망 시 빨간 센서 끄기 → `ULNPUpdateISMProcessor`(엔진 ISM 갱신 대체) + 인스턴스 커스텀 데이터 + 센서 머티리얼 `PerInstanceCustomData[0]`.
- 검증: `LootNPopEditor` 빌드 성공, 자동화 24/24, PIE `TestMap03`(드론 1,200) 약 40초 ensure 0·`LogMass`/`LogLootNPop` 오류 0(커스텀 데이터 개수 불일치 ensure 없음). 센서 소등과 ISM 전반 표시는 사용자 PIE 확인 대기.
- 사용자 PIE: 사망 시 센서 소등(어두운 회색)·생존 드론 발광 유지 확인. ISKM에서도 지상 적 매몰이 보여 ISM 프로세서 교체와 무관함을 확인했다(매몰은 3b 버그로 별도 수정, 아래). 구현 단위 3 종료.

## 2026-09-26 결함 수정 — 지상 적 스폰 매몰(3b부터)

- 증상(사용자 PIE): 지상 NPC의 절반가량이 허리~가슴까지 지면에 묻혀 있다. ISKM으로 바뀌어도 묻혀 있고, `LNP.SurfaceNav.EnemyExactGround 0`(legacy)으로 바꾸는 순간 전부 정상화된다 → 표현(ISM 프로세서 교체)이 아니라 위치 문제.
- 원인: Pod 스폰·부하 harness 배치가 적 Transform을 **발밑 점**으로 둔다(기준점은 캡슐 중심). 반높이만큼 묻혀 시작하고, exact 하향 probe는 시작부터 겹친 hit를 지지면으로 인정하지 않아 `Rejected`(제자리)로 둔다. 움직이는 적은 수평 sweep의 겹침 풀기로 빠져나오지만 서 있는 적은 계속 묻힌다. legacy는 매 프레임 표면에 스냅해 드러나지 않았다. exact 접지를 기본값으로 켠 3b 구현 단위 2(2026-09-25)부터의 결함이다.
- 수정: `SetupSpawnedEntities`가 비행형이 아닌 적을 발밑 점에서 Up으로 `CapsuleHalfHeight`만큼 올린다(Pod 스폰·harness 공통). `StepGrounded`는 probe가 시작부터 겹치면 겹침 법선으로 풀어 다음 프레임에 지지면을 다시 찾게 한다(Phase 6 exact 폴백의 안전망).
- 영향: 3b 한계치 측정(단일 500·병렬 750)도 같은 harness 배치라 서 있는 일부 적이 묻힌 상태였다. 묻힌 적의 probe는 지지면 없이 `Rejected`로 끝나 query 수·비용이 정상 접지와 조금 다를 수 있다. 구현 단위 4 측정에서 지상 N=750 병렬을 한 번 다시 재 기준선이 유지되는지 확인한다.
- 검증: Live Coding, 자동화 24/24(`ExactMovement.GroundAndCliff`에 묻힌 채 시작 케이스 추가).
- 사용자 PIE(`EnemyExactGround 1`): 묻힌 지상 NPC 없음, 정상.

## 2026-09-27 구현 단위 4 — 측정과 예산 결정

### harness 확장

- `-LNPLoadBaselineFlyers=F`: 비행 적을 지상 링과 다른 seed(`+7919`)의 두 번째 링 지면점에 두고 스폰 규약대로 띄운다. Home은 링 중심(섬 가장자리 아래 지각).
- 비행 전용 실행(지상 0)은 플레이어를 링 중심 쪽 큰 섬 가장자리에서 500cm 안쪽 윗면에 둔다. 혼합 실행은 지상 기준선 비교를 위해 링 중심을 유지한다.
  - 처음에는 섬 윗면 가운데에 뒀으나 Home에서 약 6,000cm라 시야(5,000cm) 밖이어서 교전이 거의 없었다(LoS 30초 140~406회).
  - 섬 윗면(반지름 25,800)과 링 중심 지각(약 30,980)의 높이 차가 약 5,000cm라 가장자리 자리도 Home에서 직선 약 5,300cm다. 그래도 교전 수는 슬롯 상한(아래)까지 찬다.
- 합성 넉백·분포·이동 이벤트는 지상 개념이라 비행 적을 뺀다(비행 적의 Velocity는 넉백 잔여).
- 보고 추가: steering 결과 누계(`LNPEnemyFlightStats`: Hover·Clear·Blocked·Stuck·교착 복구 진입), 비행 1마리당 프레임 `FlightSteering`·`EnemyLineOfSight` query 수, 비행 적 행동 상태(None·Alert·Confirmed) 1초 표본 평균, 관통 검출(CVar `LNP.SurfaceNav.LoadBaseline.FlightPenetrationCheck`, 비행 캡슐을 Up 1cm sweep해 시작 겹침. 켠 실행은 프레임 판정에 쓰지 않는다).
- `RunLoadBaselineMatrix.ps1`: `Flyers`·`Pen`·`Phase` 인자와 3c 시나리오(`F100/300/500_par`, `N500_F100_par`, `F500_par_pen`, 대조 `N500/700/750_exact_par_3c`). 로그는 `Saved/Profiling/Phase03c`.
- 유니티 빌드 묶음이 바뀌며 `LNPFlightSteeringTest.cpp`와 `LNPEnemyExactMovementTest.cpp`의 익명 네임스페이스 `using namespace`·`TestDeltaTime`이 충돌했다. 비행 테스트에서 using-directive를 없애고 `LNPFlightSteering::`로 한정했다.

### 측정 오염(1차, 폐기)

1차 매트릭스는 백그라운드 게임 클라이언트가 CPU 약 0.5코어와 GPU를 쓰는 중에 돌았다. 지상 N750이 query 수는 같은데(1,576) 호출당 비용이 모든 분류에서 올라(발사체 1.81→2.22us) P95 19.48ms가 나왔다. 게임 종료 뒤 대조 N500이 3b와 같아(12.94 vs 13.21ms) 1차 수치는 버렸다(`Saved/Profiling/Phase03c/run1_withBackgroundGame`). 측정 전에 다른 게임·무거운 프로세스가 없는지 확인한다.

### 결과(패키지 Development 호스트, `-nullrhi -corelimit=4`, 병렬 기본값, 조건당 1회)

| 시나리오 | 서버 프레임 P50/P95 | exact/프레임 P50 | 상태 평균 None/Alert/Confirmed | `FlightSteering` 1마리당 프레임 · 호출당 | 막힘(이동 프레임 중) | 교착 포기 / 복구 진입 |
|:---|:---|:---|:---|:---|:---|:---|
| 비행 100 | 4.16/4.75ms | 0.85ms | 0 / 60 / 40 | 0.20 · 3.89us | 0.0% | 0 / 0 |
| 비행 300 | 6.57/8.28ms | 1.52ms | 40 / 220 / 40 | 0.45 · 5.82us | 12.9% | 29 / 93 |
| 비행 500 | 8.20/11.39ms | 1.12ms | 9 / 451 / 40 | 0.20 · 4.03us | 3.0% | 9 / 24 |
| 지상 500 + 비행 100 | 13.06/14.44ms | 5.53ms | 3 / 92 / 5 | 0.14 · 6.06us | 18.9% | 4 / 12 |
| 지상 500(대조) | 11.75/12.94ms | 4.99ms | — | — | — | — |

- **동시 교전 비행 수는 슬롯 상한으로 고정된다.** 비행 적은 PureEntity 원거리라 원거리 슬롯 풀(`MaxRangedSlotsPerPlayer` 20)을 쓴다. 플레이어 2명이면 Confirmed는 40이고 나머지는 Alert(제자리 호버·주시)다. 혼합 실행은 지상 원거리 적이 같은 풀을 먼저 차지해 비행 Confirmed가 5뿐이었다. 비행 수를 늘리면 교전 비용이 아니라 대기·배회 비용이 는다.
- **비행 exact 비용은 작다.** 1마리당 프레임 0.2~0.45 query, 호출당 약 4~6us. LoS는 30초 468~589회(개체당 프레임 0.0003~0.0006)이고 거부는 0~9회다.
- **서버 프레임은 비행 100마리당 약 1.0~1.5ms 는다.** exact 몫은 0.1~0.3ms이고 나머지는 엔티티당 공통 비용(타게팅·StateTree·복제 등)이다. 지상 100마리당 약 1.7ms(그중 exact 약 0.85ms)와 비교하면 비행 1마리는 지상 1마리의 약 60~85%다. Phase 4 캐시로 줄어드는 부분이 아니다.
- 안전성: 모든 실행에서 `UnknownHits=0`, `EnvelopeEscapes=0`, 호스트·게스트 ensure·crash 0. 관통 검출(비행 500, 2,921프레임) 관통 0. 게스트 프레임 P95 2.3~3.9ms.
- 미해결: 2차 매트릭스의 비행 100 실행에서 호스트 게임 스레드가 비동기 스폰 배치 로그 직후 멈춰 게스트가 연결 타임아웃으로 끊겼다. 같은 조건 재실행과 다른 모든 실행에서는 재현되지 않았다. 덤프가 없어 원인 미상이다. 재발하면 `-LNPLoadBaseline` 스폰 배치(작업 스레드 exact probe)와 게임 스레드의 경합부터 본다.

### 지상 한계치 재확인(스폰 매몰 수정 뒤)

| N | 3b | 3c 1 | 3c 2 | 3c 3 |
|:---|:---|:---|:---|:---|
| 700 | 15.98ms | 16.81ms ✗ | 15.74ms ✓ | — |
| 750 | 16.40ms | 17.59ms ✗ | 17.60ms ✗ | 16.89ms ✗ |

- N750은 세 번 모두 16.6ms를 넘었다. 병렬 한계치는 **약 700(경계)**으로 내려왔다. N500은 3b와 같다.
- query 수(750: 1,574~1,584 vs 1,575)와 분류별 호출당 비용(`GroundRiskFallback` 5.89 vs 5.92us)이 3b와 같다. exact 비용이 아니라 3b 이후 늘어난 비 query 비용(비행 이동·비행 격자 프로세서의 빈 실행, ISM 갱신 프로세서 교체, 매몰 수정 뒤 접지 분포 변화 등)으로 보이나 나누어 재지 않았다.
- Phase 4 캐시 적중률 목표와 Phase 6 재측정의 비교 기준은 병렬 700으로 읽는다.

### 결정(사용자, 2026-09-27)

- 비행 적 총수는 **200**이다. Pod 편성을 타입으로 나눠 비행 적이 없는 Pod와 비행 적이 편성된 Pod를 둔다. 편성 Pod는 2~6기(2기만이면 허전해서 3·5·6 사용).
- `DA_MassSpawnConfig`: 세트 4개, Pod 설정·지상 편성(ActorPromoted 근접 2, PureEntity 근접 10·원거리 2)은 모두 같다.

| 타입 | Pod 수 | 드론/Pod | 드론 계 |
|:---|:---|:---|:---|
| 지상 전용 | 74 | 0 | 0 |
| 경 편성 | 20 | 3 | 60 |
| 표준 편성 | 16 | 5 | 80 |
| 중 편성 | 10 | 6 | 60 |
| 계 | 120 | | 200 |

- 예산 근거: 한 플레이어 주변에 지상 500이 몰린 harness에서 P95 여유는 약 3.7ms이고 비행 100마리당 약 1.5ms라 동시 비행 약 200이 한계다. 실제 월드는 Pod가 흩어져 있지만 지상·비행 이동 프로세서에 시뮬레이션 LOD·가변 틱이 없어 서버는 거리와 무관하게 모든 적을 매 프레임 처리한다. 그래서 동시 교전 수가 아니라 총수를 예산으로 삼는다.

## 2026-09-27 구현 단위 5 — 기능 점검과 Phase 종료

- 사용자 PIE: 새 Pod 편성(D-054) 체감 이상 없음 — 드론 3·5·6기 Pod와 드론 없는 Pod의 분포 자연스러움.
- D-031 2P 스모크(에디터 바이너리 `-game` 리슨, 기본 스폰 = 새 편성, 로그 `Saved/Logs/Smoke3c_U5`): 스폰 요청 526건(Pod 120 + 지상 360 + 드론 46)으로 편성과 일치. 사용자가 게스트 화면에서 드론과 교전 — 드론 위치·Ghost 발사체·HP 바 정상. 호스트·게스트 ensure·크래시 0, `LogLootNPop` 오류 0(기존 `CharacterMovementComponent` 추출 오류와 종료 시 연결 닫힘만).
- `LootNPopEditor Win64 Development` 성공, 자동화 `LootNPop.SurfaceNavigation` 24/24, 패키지 BuildCookRun(`LootNPop Win64 Development`) 성공.
- 문서: `../../TechDesign_EnemyNPC.md` 프로세서 표 18 → 21종(비행 이동, ISM 갱신 대체, 3c 이전부터 누락된 HitStop) — 인덱스 3곳(`CLAUDE.md`·`ProjectOverview.md`·Notion)과 `TechDesign_Networking.md`의 개수도 갱신. `design/RuntimeCollision.md` 쿼리 분류에 `FlightSteering`·`EnemyLineOfSight`, `design/MovementIntegration.md`에 3c 구현 규약, `Roadmap.md` 3c 완료·Phase 6 비교 기준 약 700.
- Phase 3c 완료. 완료 조건 전부 충족(`../phases/Phase03c_FlyingNpcFoundation.md` §5).
