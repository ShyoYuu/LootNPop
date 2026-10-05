# Phase 14 작업 로그

## 2026-10-05 — 착수: 실행 문서 구체화와 단위 1 범위 조사

사용자가 Phase 14 착수를 지시했다. README·Current·Decisions·Roadmap·`design/MovementIntegration.md` "벽 타기 NPC"를 읽고 실행 문서 `../phases/Phase14_SurfaceCrawlNpc.md`를 만들었다. 코드·에셋 변경은 없다.

### 단위 1 범위 조사

- 게임 모듈에서 위치로부터 구 중심 방향을 계산하는 지점(`(-Location).GetSafeNormal()`, `(GravityOrigin - Location).GetSafeNormal()`, `DecodeSphereRotation`)을 grep으로 전수 확인했다.
- 설계 문서의 "약 80곳·13개 파일"은 중력·이동 내부와 플레이어 쪽까지 포함한 추정이다. 적 자신의 몸 방향을 쓰는 소비처는 약 16곳·9개 파일이며 나머지는 지상·비행 도메인의 중력·투영이라 유지한다. 분류는 실행 문서 "단위 1 분류"가 소유한다.
- 인지 FOV와 피격 주시 조준 pitch 게이트는 이미 엔티티 회전(`EnemyForward`, `InverseTransformVectorNoScale`)으로 계산한다.
- 회전 쓰기 경로: 지상 이동 회전 보간·이동 후 `MakeFromXZ(…, NewUp)`, 비행 이동, 동적 지지면 delta, 배회 복원(`DecodeSphereRotation`), 게스트 Slerp, Actor 활성화 텔레포트. 모두 구 중심 방향 Z를 만들도록 작성돼 있으나 airborne·사망·휴면 상태의 실제 편차는 아직 측정하지 않았다.
- `FLNPPositionHistorySample`은 위치만 기록한다. 벽보행 되감기 캡슐 축은 단위 3에서 이력에 Up을 더해 해결한다.

## 2026-10-05 — 단위 1: 적 Up을 엔티티 회전 Z축에서 읽기

사용자 결정: 벽보행 적은 벽면 전용 스폰 후보 없이 Pod 주변 지면에서 스폰돼 스스로 벽으로 이동한다. 간단한 구현을 먼저 하고 나머지는 플레이에서 필요가 생길 때 계획한다.

### 구현

- `LNPMassUtils.h`에 `LNPMass::GetEntityUp(const FTransform&)`(회전 Z축)를 추가했다.
- 회전 Z로 바꾼 소비처: 공격 기저 `LNPEntityAttack::MakeBasis`, 투사체·근접 trace의 서버 적 수집과 게스트 ClientTargets(양쪽 같은 헬퍼), 근접 판정 디버그 드로, 적 탐색 질의, 락온·HP 마커, 패링 반격 넉백의 공격자 Up, 행동 디버그 마커.
- 위치에서 Up을 유도하던 표시 측 두 곳은 결과 구조에 Up을 실어 전달한다: `FLNPEnemyMarkerEntry::Up`(HUD HP 바), `FLNPTargetQueryResult::Up`(락온 조준 높이 클램프). 조사 때 놓쳤던 `LNPLockOnComponent`를 이 단계에서 추가로 찾았다.
- Poise 디버그 게이지는 플레이어와 적을 한 질의로 그리므로 이번에는 유지했다.
- 진단: `ULNPEnemyUpDeviationProcessor`(서버·클라이언트, PostPhysics). `LNP.Enemy.UpDeviationCheck 1`이면 `LNP.Enemy.UpDeviationWindowSeconds`(기본 5초)마다 상태(Dying/Flying/Airborne/Promoted/Dormant/Wander/Grounded)별 표본 수·최대 각도·해당 NetID를 `EnemyUpDeviation` 로그로 남긴다.

### 검증

- 에디터 전체 빌드(Build.bat) 성공.
- 헤드리스 자동화 30/30 PASS, 종료 코드 0: `LNP.HitDetection`(6), `LootNPop.SurfaceNavigation.Activity`(4)·`Wander`(2)·`WanderReplication`(5)·`EnemyMovement`(5)·`EnemyLineOfSight`(1)·`FlightSteering`(3)·`ExactMovement`(4).
- 남은 일: 리슨 2P에서 편차를 측정하고 판정·락온·HP 바 화면을 확인하는 것. 편차가 크면 소비처가 아니라 회전을 쓰는 쪽을 고친다.

### 리슨 2P 측정과 스폰 회전 수정

- 사용자 리슨 2P(호스트·게스트)에서 지상·비행 교전, HP 바, 락온 마커, 패링에 어색한 점이 없음을 확인했다(스폰 수정 전 코드).
- 같은 세션의 `EnemyUpDeviation` 로그(5초 창 99개)는 다음과 같다. 서버 비행 0.289°, 공중 0.103°, 사망 0.239°, 배회 0.000°. 게스트 비행 0.134°, 접지 0.034°. 반면 **서버 휴면은 29.16°**(NetID 899, 측정 내내 일정)였고, **서버 접지도 간헐적으로 최대 28.84°**였다.
- 원인: `LNPMassSpawnPlan`이 적 스폰 회전을 지면 법선 기준(`MakeFromZ(Candidate.Normal)`)으로 만든다. 경사에서 스폰된 적은 첫 이동 전까지, 휴면 중에는 계속 경사만큼 기운 회전을 유지한다. 게스트는 복제 회전을 구 중심 방향으로 복원하므로, 단위 1 이후에는 이런 정지 적의 판정 캡슐·공격 기저가 서버에서만 기운다.
- 수정: 계획(Pod와 공유)은 그대로 두고 `ULNPMassSpawnSubsystem::SetupSpawnedEntities`에서 적 회전 Z를 `GravityOrigin` 기준 구 중심 방향으로 맞췄다. 전방 벡터는 보존한다(`MakeFromZX`). Live Coding으로 반영했다.
- 재측정(MCP로 PIE 재시작, 5초 창 5개): 서버 휴면 0.000°, 접지 0.194°, 비행 0.242°, 공중 0.049°, 배회 0.000°. 게스트 비행 0.131°, 접지 0.040°. 남은 편차는 이동 중 회전 보간과 복제 보간에서 오는 0.3° 미만이다.
- 재측정 세션에서 비행 스폰 안전 거부 2건이 나왔다. 월드 seed가 18625→21749로 바뀌어 배치가 달라졌고, `TryLiftFlyingSpawn`은 위치만 읽으므로 회전 수정과 무관한 기존 안전 거부다.
- 측정 뒤 `LNP.Enemy.UpDeviationCheck 0`으로 되돌리고 PIE를 종료했다.
- 사용자가 에디터를 종료한 뒤 Build.bat 전체 빌드에 성공했다. 같은 관련 자동화 30개를 다시 돌려 30/30 PASS, 종료 코드 0이었다(스폰 경로를 지나는 `FlightSteering.SpawnPenetration` 포함). 단위 1을 완료한다.
