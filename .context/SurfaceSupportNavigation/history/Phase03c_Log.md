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
