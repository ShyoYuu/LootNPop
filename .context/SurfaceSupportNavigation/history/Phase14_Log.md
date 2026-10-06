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

## 2026-10-05 — 단위 2 착수: 세분과 2a·2b·2c 코드

사용자가 단위 2 진행을 지시했다. 착수 조사와 세분은 실행 문서 "단위 2 세분"이 소유한다.

### 사용자 결정

- 이동 충돌 몸체는 구다(비행 드론과 같은 방식). 벽보행 Config는 `CapsuleHalfHeight = CapsuleRadius`로 둔다.
- 외형 컨셉을 거미형에서 **로봇청소기형**으로 바꾼다. 다리 애니메이션을 구하기 어렵기 때문이다. 크기는 비행 드론과 비슷하게 한다. 모델링은 단위 5다.
- `DA_MassSpawnConfig`는 임시 테스트 편성이다. 벽보행 검증 때는 편성 전체를 벽보행으로 바꾼다. 성능 기준선은 지상·벽보행·비행 각각의 상한만 가늠한다.
- 벽보행을 검증할 수직 지형이 많은 월드가 필요하다(하위 단위 2d).

### 구현

- 2a 도메인 배선: `ELNPNavigationDomain::SurfaceCrawl`·`IsCrawling()`, `FLNPEnemyCrawlTag`·`FLNPEnemyCrawlFragment`(표면 법선·사망 착지). Trait가 태그·fragment를 붙이고, PureEntity·원거리가 아니거나 반높이 ≠ 반지름이면 경고한다. 지상 이동·경로·지상 격자·분리·배회 구간·Up 편차 진단 쿼리에서 제외했다. 스폰은 지상과 같은 지면점 + 반높이이며 `SurfaceHandle`을 비운다. IdleTask 배회 목표는 Home 주변 방향을 `ProjectToSameLayer`로 exact 투영한 점(실패하면 Pod 접평면 점)이다.
- 2b 순수 함수 `LNPSurfaceCrawl`(`Step`·`Attach`·`SteerDirection`·`GoalDistanceSq`·`IsCrawlable`)과 query 분류 `SurfaceCrawl`(필수 분류, FlightSteering 다음). 규약은 `design/MovementIntegration.md` "벽 타기 NPC"에 반영했다.
- 2c `ULNPEnemyCrawlMovementProcessor`(서버 전용, Movement 그룹, 공격 프로세서 뒤): 배회·Alert·교전(원거리 정지 거리), 피격 반응·배회 타임아웃·경직·공격 중 정지, 넉백·표면 상실 시 `StepAirborne` 후 착지 자리 재부착, 사망 낙하. 스폰 직후 법선을 모르면 발밑을 한 번 probe한다.

### 시행착오

- 첫 내려가기 자동화가 실패했다. 도착 판정을 현재 표면의 접평면 거리로만 재서, 뒷벽에 붙은 개체가 바닥 목표와 같은 높이(벽 수직 거리 359cm)에서 도착으로 오판했다. IdleTask에 넣은 규칙도 같은 결함이었다. 법선 방향 거리 중 반지름 초과분을 더하는 `GoalDistanceSq`로 바꿨다.
- 이어서 같은 시나리오가 벽 아래 구석에서 멈췄다. 목표 방향이 벽면에 거의 수직이라 접평면 투영이 0이었다. 목표가 표면 앞쪽으로 멀면 내리막 성분을 더하는 `SteerDirection`으로 해결했다.

### 검증

- Build.bat 전체 빌드 성공.
- 신규 자동화 `LootNPop.SurfaceNavigation.SurfaceCrawl` 3/3 PASS, 종료 코드 0(`Saved/Logs/Phase14_Unit2_Crawl3.log`): 평지 이동 높이·법선 유지와 이동 프레임당 query 2회(60프레임 120회), 정지 query 0회, 천장 이동, 허공 Lost, 바닥 → 앞벽(오목) → 지붕(볼록) 오르기와 지붕 → 뒷벽(볼록) → 바닥(오목) 내려가기, 천장에서 넉백 낙하 후 바닥 재부착 1회, Unknown 벽 미부착, 모든 시나리오 관통 0.
- 단위 1의 관련 자동화 30개를 같은 실행에서 다시 돌려 모두 PASS했다(`Saved/Logs/Phase14_Unit2_Tests2.log`, 그 실행의 유일한 실패는 수정 전 내려가기 테스트).
- 자동화 뒤 `git status`에 에셋 변경이 없다.
- 남은 일: 벽보행 EnemyConfig·EntityConfig 에셋, 편성 전체를 벽보행으로 바꾼 PIE 확인(2c), 수직 지형 검증 월드(2d).

## 2026-10-06 — 단위 2c: 벽보행 에셋과 PIE 확인

### 에셋(MCP)

- `/Game/Enemy/DA_Enemy_PureEntity_Crawler01`: 비행 드론 Config 복제. 반지름·반높이 80, PureEntity·원거리, `NavigationDomain = SurfaceCrawl`. 외형은 단위 5 전까지 드론 메시다.
- `/Game/Enemy/DA_EnemyEntityConfig_PureEntity_Crawler01`: EntityConfig 복제(새 GUID), Trait의 EnemyConfig를 Crawler01로 바꿨다.
- `/Game/SpawnData/DA_MassSpawnConfig`: 4개 세트(Pod 20·40·30·30) 모두 Crawler01 3기, 나머지 항목 0 — 벽보행 360기. 바꾸기 전 편성은 로컬 `Saved/Phase14_Unit2/DA_MassSpawnConfig_before.txt`에 남겼다(Bench01 근접 4·원거리 2, 나머지 세트 근접 4·원거리 2·비행 6, Pod 주변 반경 1,000).
- 사용자 요청으로 벽보행 공격 거리를 2500 → **100(1m)**으로 줄였다. TestMap03의 벽이 낮아 사거리 25m로는 벽 아래에서 사격만 하고 따라 오르지 않았다. 정지 거리(70cm)가 몸 반지름(80cm)보다 작아 플레이어와 겹칠 수 있다 — 벽보행 3D 분리는 단위 4다.

### PIE 확인(TestMap03, 리슨 2P 호스트 화면)

- 배치: 4개 세트 모두 부족분 0(enemy_placed 60·120·90·90). Trait 검증 경고 없음. 오류는 기존 CMC 추출 경고뿐이다.
- 사용자 확인: 경사 지형에서 몸이 지면 경사를 따라 기운다. 벽면에 붙은 채 벽을 따라 추격한다. 넉백으로 날려도 착지 뒤 지면에 다시 붙는다. 총구 방향과 몸 방향이 맞는다.
- 관찰된 이상: 배회 중 가끔 옆으로(수직 각도로) 누운 채 멈춘 개체가 있다. 플레이어를 인식해 움직이기 시작하면 바로 지면에 붙는다. 정지 중에는 표면 query를 하지 않으므로 멈추기 직전에 잡힌 법선이 남는 것으로 본다. 유력한 추정은 몸(반지름 80)보다 낮은 턱 옆면에 붙은 채 배회 도착 판정에 걸린 경우다 — 확인하지 못했다. 다음 PIE에서 누운 개체 옆에 낮은 턱이 있는지 본다.

## 2026-10-06 — 단위 2d: 벽보행 검증 옥탄트

사용자 결정: 회귀 fixture를 확장하지 않고 벽보행 전용 검증 옥탄트를 새로 만든다. 수직 추격을 쉽게 시험하도록 그래플 앵커를 곳곳에 많이 둔다(사용자 요청).

### 구현

- 에디터 명령 `LNP.SurfaceNav.BuildCrawlFixture`(`LootNPopEditor/SurfaceNavigation/LNPCrawlFixture.cpp`) → `/Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Crawl`, 지각 `SM_FixtureCrawlCrust_R30000`(완전 구면 N=256). 상자는 회귀 fixture의 `SM_FixtureBoxTop`(Support)·`SM_FixtureBoxBody`(Blocker)를 재사용한다. oracle이 없는 PIE 확인용이다.
- 옥탄트 중앙(위도 35.26°, 방위 45°) 기준: 절벽(50×8m, 높이 12m)과 광장 쪽 옆면 8m 높이의 돌출부(밑면 = 천장), 탑(10×10m, 높이 30m), 네 기둥 위 판(24m, 천장 8m), 턱 줄(10·30·50·70·100·150·250cm), 가는 기둥 2개(폭 2·3m, 높이 15m).
- 그래플 앵커 마커 12개(높이 12~36m, 15~20m 간격 — 사거리 4~25m), 편성 세트당 Pod anchor 2개(8개).
- `LNP.SurfaceNav.BakeOctant`로 `DA_OctantSurface_Fixture_Crawl`을 구웠다: Support source 18개·Layer 18개, NeedsExact 0, spawn authored 8·후보 10,624.

### 시행착오

- 첫 굽기가 Pod anchor 간격 검사(최소 20m)에 걸렸다(18.4m). 빌더 좌표를 고치고, 새 콘솔 명령 등록 때문에 에디터를 재시작하지 않도록 만든 LVI의 anchor 4개만 MCP로 옮겼다. 계산식은 옮기지 않은 anchor 2개의 저장 transform과 일치함을 확인했다.

### PIE 확인

- 방법: TestMap03의 `OctantPoolData` 정의를 메모리에서만 이 LVI·SurfaceData로 바꾸고, 광장 위에서 PIE를 시작한 뒤 바로 Meadow 원본으로 되돌렸다. 디스크 pool SHA256은 원본과 같다(`Saved/Phase14_Unit2/OctantPool_sha256.txt`).
- 8 slot 모두 이 옥탄트. 4개 세트 모두 authored 16 사용, 부족분 0, 벽보행 360기 배치. 마커 요소 96개(12 × 8) 생성.
- 사용자 확인: 그래플로 절벽 위에 오르면 벽면에 붙어 벽 위까지 추격한다. 판 위로 오르면 기둥을 타고 판 밑면(천장)까지는 따라온다.
- 길찾기 없는 조향의 한계(사용자가 정상으로 판정): 절벽 돌출부 밑면에서는 목표가 위쪽 너머라 천장 접평면 방향이 다시 벽 쪽을 가리켜 벽 ↔ 천장을 오가며 떤다. 판 밑면에서 판 옆면을 돌아 윗면으로 오지 못한다. 
- 누운 개체: 검증 옥탄트의 턱 줄 주변에서 사용자가 관찰했을 때 옆으로 선 개체가 없었다. TestMap03에서 본 현상의 원인은 확정하지 못했다. 다시 보이면 그 자리 지형과 함께 조사한다.

## 2026-10-06 — 단위 2e 추가: 단순 우회 시도

사용자 결정: 길찾기 없는 조향 한계 자체는 받아들이지만, 구현이 복잡하지 않은 선에서 간단한 우회 시도는 한다. 우회 시도가 아예 없는 것과 단순하게라도 시도하는 것은 플레이어 체감 차이가 크기 때문이다. 하위 단위 2e로 실행 문서와 `design/MovementIntegration.md`에 반영했고, 단위 3보다 먼저 진행한다.

## 2026-10-06 — 단위 2e: 단순 우회 구현과 확인

### 구현

- `LNPSurfaceCrawl::UpdateDetour`·`ResetDetour`와 `FDetourState`(`FLNPEnemyCrawlFragment::Detour`). 벽보행 이동 프로세서가 교전 추격(Confirmed, 속도 > 0) 중에만 조향 방향을 이 함수로 바꾸고, 그 밖의 프레임·공중에서는 비운다. 규칙은 `design/MovementIntegration.md` "벽 타기 NPC" 우회 항목이 소유한다.
- 자동화 `LootNPop.SurfaceNavigation.SurfaceCrawl.Detour`: 돌출부(벽 앞면에 붙은 판)와 테이블(기둥 위 판). 조향만으로는 900프레임 안에 윗면 목표에 닿지 못하고, 우회하면 각각 우회 2회로 닿는다. 평지 직진 추격에서는 우회 0회, 모든 경우 관통 0.

### 시행착오

- 첫 구현은 테이블에서 우회 55회에도 실패했다. 벽 ↔ 천장 떨림 중이라 법선이 매 프레임 바뀌어, "다른 면에 올라서면 끝"이 시작 다음 프레임에 성립했다. 시작 위치를 벗어나기 전에는 면 전환을 무시하게 했다.
- 이어서 천장에서 판 가장자리까지는 갔지만, 모서리를 반쯤 감싼 법선(내적 0.70)에서 우회가 끝나 일반 조향이 밑면으로 끌어내렸다. 다른 면 판정을 내적 < 0.1로 바꾸고, 천장 여부를 현재 법선이 아니라 기준 면(시작 위치를 떠날 때의 면)으로 정했다.

### 검증

- Build.bat 전체 빌드 성공. 관련 자동화 34/34 PASS, 종료 코드 0(`Saved/Logs/Phase14_Unit2e_Tests.log`): SurfaceCrawl 4종과 단위 1의 30개.
- 검증 옥탄트 PIE(같은 메모리 pool 교체 방식, 디스크 pool 해시 원본 일치): 이전에는 돌출부·판을 한 마리도 넘어오지 못했지만, 이번에는 전부는 아니어도 일부가 우회해 넘어오는 것을 사용자가 확인했다.
- 단위 2를 완료한다.

## 2026-10-06 — 단위 3: 법선 복제

### 구현

- `FLNPReplicatedAgent::SurfaceNormal`(uint16 형제 필드, octahedral 8+8bit, 0 = 법선 없음 예약). 벽보행(`FLNPEnemyCrawlTag`) 개체만 회전 Z를 싣고, Yaw는 양자화 법선의 `MakeFromZ` 기저로 인코딩한다(`LNP::Replication::EncodeReplicatedLocalYaw`·`DecodeReplicatedRotation`). 지상·비행·Player·LootPod은 0을 유지해 기존 구 접평면 디코드와 같다.
- 클라이언트 `ApplyReplicatedTransform`·`PushSmoothingTarget`이 agent 전체를 받아 `DecodeRotation()`으로 복원한다. 기존 Slerp가 법선 변화를 보간한다.
- `UpdateEnemyAgent`에 `bCrawl` 인자: 법선 코드 변경은 자세 변경이며 Yaw를 다시 쓴다. 주기 게이트는 유지한다.
- 위치 이력 기록 프로세서가 벽보행 이동 프로세서 뒤에 돈다.
- 계획 변경: 위치 이력의 Up 기록은 하지 않았다. 벽보행 판정 캡슐이 구(HalfHeight = Radius)라 `SegmentHitsCapsule`의 축 길이가 0이고 Up이 판정에 들어가지 않는다.

### 검증

- Build.bat 전체 빌드 성공. 관련 자동화 36/36 PASS, 종료 코드 0(`Saved/Logs/Phase14_Unit3_Tests.log`): 신규 `SurfaceCrawl.Replication.NormalCodec`(구면 격자 최대 오차 0.902°, 0 코드 미사용, 벽·지면·천장·극점 자세 Up·전방 복원 1.5° 미만, 지상은 기존 구 디코드와 동일)·`PoseDirty`(지상 정지 Dirty 없음·법선 0 유지, 같은 벽 회전은 법선 코드 유지, 표면 전환은 주기 게이트 뒤 갱신)와 기존 34개.
- 리슨 2P PIE(검증 옥탄트, 같은 메모리 pool 교체 방식, 디스크 pool 해시 원본 일치, 벽보행 360기 부족분 0): 게스트 화면에서 벽면·판 밑면 이동과 모서리 통과 중 개체가 표면에 자연스럽게 붙어 있고 호스트와 자세가 일치함을 사용자가 확인했다. PIE 클라이언트 수는 확인 뒤 1로 되돌렸다.
- 단위 3을 완료한다.

## 2026-10-06 — 단위 4 착수: 세분과 4a~4c(인지·조준, LoS 반응, 3D 분리)

### 세분

착수 조사와 하위 단위 4a~4f는 실행 문서 "단위 4 세분"에 적었다. 원거리 슬롯 풀은 `GetSlotPool`이 AttackType에서 유도하므로 이미 원거리라 코드 변경이 없었다. 넉백 낙하 착지 재부착은 단위 2에서 끝났고 남은 것은 착지 뒤 재귀속(4d)이다. 사용자 결정: 사거리는 800(1m는 단위 2 벽 오르기 확인용이었다), 4a~4c를 묶어 구현한 뒤 PIE로 확인한다.

### 구현

- 4a: `DA_Enemy_PureEntity_Crawler01` — `VisionAngle` 240 → 360, `AimPitchMin/MaxDeg` -70/75 → ±89(천장 개체 바로 아래 공격자의 피격 인지·조준), `AttackRange` 100 → 800. Trait 검증이 벽보행 `VisionAngle < 360`을 경고한다.
- 4b: 벽보행 이동이 교전 중 사거리 안이어도 `bLineOfSightBlocked`이면 서지 않고 타겟 쪽으로 계속 기어간다. 플래그는 소비하지 않는다(공격 게이트가 0.25초마다 다시 보고 트이면 0으로 되돌린다). 계속 막히면 2e 우회가 끼어든다.
- 4c: `ULNPCrawlSpatialGridSubsystem`(비행 격자와 같은 구현, 인스턴스만 따로)을 격자 프로세서가 벽보행(사망 제외)으로 짓는다. 벽보행 이동이 3D 거리로 이웃을 찾고 미는 방향을 현재 접평면에 눕혀 속도에 더한다. 접평면에서 겹친 쌍은 핸들 순서로 좌우를 가른다. 이웃이 없으면 0이라 정지 개체의 query 0회가 유지된다. 에셋 `SeparationRadius` 90 → 220(몸 지름 160).

### 검증

- 새 UCLASS라 사용자가 에디터를 종료한 뒤 Build.bat 전체 빌드 성공. 관련 자동화 36/36 PASS, 종료 코드 0(`Saved/Logs/Phase14_Unit4abc_Tests.log`): 단위 3과 같은 묶음.
- 검증 옥탄트 1P PIE(같은 메모리 pool 교체 방식, 디스크 pool 미저장 — 2P는 게스트가 어그로를 가져가 사용자가 1로 줄였다): 개체끼리 겹치지 않고, 천장·벽에서도 사선이 트이면 즉시 공격하며, 사선이 막히면 공격하지 않고 접근하려고 이리저리 움직이는 것을 사용자가 확인했다. 8m보다 가까이서 쏘는 듯한 체감은 뒤쪽 개체가 사거리에 들어오려고 앞쪽 개체를 밀어내서 생겼고, 개체 수를 줄이자 사거리가 맞았다(사용자 확인).
- 관찰: 판 밑으로 유도한 개체가 판 위의 플레이어를 정확히 따라간다. 추적 중 타겟 유지가 거리만 보고 실제 위치를 싣기 때문이며 현재 설계상 정상이다. 적 NPC 전반의 시야 상실 후 추적 제한(마지막 목격 위치·유예 시간·부정확한 소리 감지)은 사용자 결정으로 `../../Idea_Backlog.md`에 후보로 올렸다.

## 2026-10-06 — 단위 4d: 재귀속

### 구현

- 순수 함수 `LNPSurfaceCrawl::UpdateHomeApproach`·`ResetHomeApproach`·`SelectNearestPod`와 `FHomeApproachState`(`FLNPEnemyCrawlFragment::HomeApproach`). 규칙은 `design/MovementIntegration.md` "벽 타기 NPC" 재귀속 항목이 소유한다.
- 벽보행 이동 프로세서가 터지지 않은 Pod 위치를 모으고(`PodQuery`), 비교전 프레임마다 계측한다. 재귀속하면 배회 목표를 다시 요청하고 `[SurfaceCrawlHome] enemy oldPod newPod orphaned oldDist newDist` 로그를 남긴다(거리 필드는 첫 PIE 뒤 Live Coding으로 추가).

### 검증

- `FLNPEnemyCrawlFragment` 레이아웃 변경이라 에디터 종료 뒤 Build.bat 전체 빌드 성공. 관련 자동화 37/37 PASS, 종료 코드 0(`Saved/Logs/Phase14_Unit4d_Tests.log`): 신규 `SurfaceCrawl.HomeApproach`(다가오면 30초 무재귀속, 배회 거리 안 무계측, 정체 10초 뒤 재귀속, 반지름 미만 떨림은 진전 아님, 진전 뒤 정체는 마지막 진전부터 10초, 최근접·후보 없음)와 기존 36개.
- 검증 옥탄트 1P PIE 두 번: 첫 번째에서 넉백·도주 뒤 재귀속 1건(`enemy=178 oldPod=177 newPod=457 orphaned=0`, 거리 로그 추가 전), 두 번째 약 3분에서는 0건. 검증 옥탄트에는 분리된 섬이 없고 넉백(500)으로 멀리 날지 않아, 대부분 기어서 Home 쪽으로 돌아가 재귀속 조건이 성립하지 않은 것으로 본다.
- 사용자 결정: 자동화와 PIE 1건으로 4d를 완료한다. 정확한 재현은 섬 위에서 플레이어·NPC를 스폰하고, 섬 밑면에 모인 NPC를 런처로 흩어 다른 지형에 떨어뜨리는 방식이 필요하다. 4f까지 마친 뒤 섬 스폰이 포함된 테스트 레벨을 준비해 4d~4f를 한 번에 확인한다.

## 2026-10-06 — 단위 4e·4f: 휴면·벽보행 구간 배회와 복제

배회 구간은 생성 때 복제 인코딩까지 하므로 서버(4e)만 넣으면 게스트 표시가 깨진다. 둘을 함께 구현했다.

### 구현

- 활동 대역: 벽보행 Pod를 강제 활성 조건(`!IsFlying() && !SurfaceHandle.IsValid()`)에서 뺐다. 휴면 개체는 벽보행 이동 쿼리에서 빠져 붙은 자리에 멈춘다.
- 서버 구간 배회: `ULNPEnemyWanderProcessor`가 벽보행도 처리한다. 지금 붙은 면(엔티티 회전 Z)의 접평면에서 기존 부채꼴 배회점 쪽으로 최대 `0.3 × MoveSpeed × 1초`(현재 180cm, 0.5~1배 무작위) 직선, 정지 9초. 생성 검증 `LNPSurfaceCrawl::ValidateWanderSegment`(같은 높이 구 sweep 1 + 몸 반지름 간격 하향 probe, 법선 내적 ≥ 0.99·붙을 자리 오차 ≤ 2cm). 보간 중 query 없음, 동적 지지면 slot은 기존처럼 프레임 sweep(벽보행은 구 sweep). 벽보행 이동 프로세서는 `bWanderHandled` 개체를 건너뛴다.
- 복제: `FLNPEnemyWanderSegment::bCrawl`, `FLNPReplicatedWanderData` Flags bit 5(slot·Layer 미사용). 서버 Yaw는 법선 기저(`EncodeReplicatedLocalYaw`)로 인코딩한다. 구간 시작 Dirty가 자세(위치·법선·Yaw)를 함께 쓰므로 클라이언트는 그때의 `SurfaceNormal`을 `FLNPReplicatedMovementFragment::WanderNormal`에 두고 구간 Up으로 쓴다. 클라이언트·되감기 평가는 비행처럼 Support 투영과 snapshot generation 조건 없이 직선을 쓴다. 페이로드 크기 변화 없음.

### 검증

- 이동 프래그먼트 레이아웃 변경이라 에디터 종료 뒤 Build.bat 전체 빌드 성공. 관련 자동화 39/39 PASS, 종료 코드 0(`Saved/Logs/Phase14_Unit4ef_Tests.log`): 신규 `SurfaceCrawl.WanderSegment`(평지·벽면 수락, 벽 진입·절벽 밖·벽 → 바닥 거부, 평지 query 6회)·`WanderReplication.Crawl`(Flags 1|32·Layer 0, 지상 비트 미혼합, 재인코딩 무변경, snapshot 없는 평가, 벽 법선 Up·구간 전방, 제자리 Yaw 복원, 되감기)와 기존 37개.
- PIE는 사용자 결정대로 섬 스폰 테스트 레벨에서 4d와 함께 확인한다.
- 남은 일: 부하 harness(`LNPLoadBaseline`) 활동 통계가 벽보행을 지상으로 센다 — 단위 6 측정 때 나눈다.

## 2026-10-06 — 단위 4 통합 확인: 섬 검증 옥탄트, 착지 Home 판정, 실패 Pod 기억

### 섬 검증 옥탄트

- `LNP.SurfaceNav.BuildIslandFixture`(`LootNPopEditor/SurfaceNavigation/LNPIslandFixture.cpp`) → `LVI_Octant_Fixture_Island`·`DA_OctantSurface_Fixture_Island`·`SM_FixtureIslandCrust_R30000`. 완전 구면 지각, 부유섬 60m × 60m(밑면 40m, 윗면 43m), 지각에서 섬 가장자리로 오르는 30° 경사로(수평 74.5m, 폭 6m), 탑(지각 Pod 곁, 25m), Pod anchor 8곳(섬 위 4, 섬 중심에서 70~90m 지각 4), 탑 앵커 1, 스프링 런처 2. 베이크 Layer 4(지각·섬·탑·경사로), authored anchor 8 모두 투영.
- PIE 시작 transform: 위치 14722.4×3(섬 윗면 위 2m), 회전 pitch 0·yaw 135·roll 125.2644. pool 교체는 같은 메모리 방식.
- 시행착오: 처음 둔 3단 그래플 사다리는 앵커가 거의 머리 위라(사거리 50m, 조준 반각 8°) 오르기 어려웠다. 사용자 지적대로 섬 밑으로는 그냥 떨어지면 되므로(낙하 피해 없음) 사다리를 빼고 경사로로 바꿨다.

### 발견과 수정

- 첫 섬 PIE에서 재귀속 0건: 도망치면 섬 위 Pod가 배회·휴면 대역으로 내려가 벽보행 이동 프로세서(재귀속 계측)가 개체를 건너뛰었다. 지상처럼 착지 때 `bNeedsHomeCheck`를 세워(활동 대역 강제 활성, 구간 배회 건너뜀) Home 배회 거리 안 도착·재귀속·Orphaned 때 내리게 했다.
- 두 번째 섬 PIE에서 재귀속 12건, 그러나 섬 밑 지각에서는 섬 위 Pod(직선 약 43m)가 지각 Pod(70~90m)보다 가까워 도달할 수 없는 섬 Pod 사이를 오갔다(`29→321→29`, `177→321→177→53`). 사용자 결정으로 개체마다 재귀속으로 떠난 Pod 최근 8개를 기억해 후보에서 뺀다(`FLNPEnemyCrawlFragment::FailedPods`). 실제 Pod Home 배회 거리 안 도착·교전 때 비운다.

### 검증

- Build.bat 전체 빌드, 관련 자동화 39/39 PASS, 종료 코드 0(`Saved/Logs/Phase14_Unit4_FailedPods_Tests.log`, `HomeApproach`에 실패 Pod 제외 케이스 추가).
- 세 번째 섬 1P PIE: 재귀속 8건, 모두 oldDist 42~48m에서 10초 정체 뒤, 같은 Pod로 되돌아간 경우 없음(예: `enemy=308 305→409→197`). 최종 Pod가 지각 Pod인지·도착했는지는 로그로 확정하지 못했지만 이후 추가 재귀속은 없었다. 탑 주변 지각 Pod 개체의 배회(짧은 이동·긴 정지)를 사용자가 확인했다.
- 리슨 2P PIE(같은 섬 옥탄트): 게스트 화면에서 탑 주변 개체의 배회, 배회 중 벽면 이동, 배회 → 교전 진입이 자연스러움을 사용자가 확인했다. PIE 클라이언트 수는 1로 되돌렸다.
- 섬 위에서 떨어진 개체를 관찰하기 어려웠던 이유: 섬 밑 개체와 섬 위 플레이어 거리가 43~50m라 경계 유지 거리(60m) 안이고 추적 유지는 LoS를 보지 않는다(`../../Idea_Backlog.md` "시야 상실 후 추적 정보 제한"). 배회 확인은 교전한 적 없는 지각 Pod 개체로 했다.
- 단위 4를 완료한다.

## 2026-10-06 — 단위 5: 외형

### 방식 결정

- 로봇청소기형은 다리가 없어 ISKM 걷기 시퀀스가 필요 없다. 드론(Phase 3c)과 같이 블렌더 MCP 저폴리 정적 메시 + ISM으로 표현한다. 회전 부품(사이드 브러시)은 ISM에서 따로 돌릴 수 없어 고정이다.

### 메시

- 블렌더(미터 단위, 드론과 같은 익스포트 옵션) → `Art/Meshes/SM_EnemyCrawler01.fbx` → `/Game/Enemy/Crawler/SM_EnemyCrawler01`(1,824 삼각형, 충돌 제거). bounds X ±78, Y -78~+100(전방 = 포신 끝), Z -78~-2.
- 형태: 원반 몸체 반지름 76(범퍼 78) + 납작한 돔 + 라이다 포탑(발광 링) + 전방 포신 + 전면 센서 띠 + 바닥 사이드 브러시 2. 머티리얼은 `M_EnemyDroneBody`·`M_EnemyDroneEye` 재사용(ISM 플래그 있음).
- 엔티티 위치가 몸 구(반지름 80) 중심이라 메시 바닥을 -78에 뒀다. 포신 끝을 전방 100·높이 -20에 맞춰 총구 오프셋 (100, 0, -20)을 바꾸지 않았다.
- `DA_EnemyEntityConfig_PureEntity_Crawler01` ISM 메시 교체(Yaw -90 오프셋 유지).
- 사용자 PIE: 바닥에 붙어 있고 포신이 이동·공격 방향과 일치, 피격·넉백 자연스러움, 외형 만족.

### 피드백과 수정

- 사망 시 발광: 드론은 `ULNPUpdateISMProcessor`가 인스턴스 커스텀 데이터[0]에 발광(생존 1·Dying 0)을 넣는다. 벽보행 태그도 같은 조건에 넣었다. ISM이 벽보행 메시 전용이라 커스텀 데이터 전부/전무 규칙을 지킨다.
- HP 바·락온 마커가 몸 위로 떠 있었다. 원인은 두 마커가 판정 구 중심 기준인데 몸이 구 아래쪽 절반에 있기 때문이다. 처음에는 표시 전용 오프셋 두 개(HP 바·락온 마커)로 고치려 했다.
- 사용자 질문으로 같은 원인이 판정에도 있음을 확인했다: 몸 위 허공 약 80cm가 맞고 원반 아래 테두리는 판정 밖이었다. 드론도 메시 높이 ±21에 판정 구 80이라 위아래 약 60cm가 비어 있었다.
- 사용자 결정(1안): 이동 구는 그대로 두고 판정 몸체를 분리한다. Config `HitCenterUpOffset`·`HitCapsuleRadius`·`HitCapsuleHalfHeight`(0이면 이동 캡슐 값). `CapsuleRadius`/`CapsuleHalfHeight`는 비행 sweep·벽보행 몸 구·지상 이동이 쓰므로 바꾸지 않는다. `ResolveEnemyCapsuleCenter`의 쓰지 않던 세 번째 인자를 판정 중심 오프셋으로 바꿨고, 판정 소비처(투사체 서버·게스트 수집과 근접 디버그, 근접 서버·게스트 수집, 적 탐색 질의, HP 바 마커)가 판정 크기를 읽는다. 되감기·투사체 격자·스플래시는 이 중심에서 위치 차이만 더하므로 오프셋이 유지된다.
  - 값: 벽보행 -40·78/78(원반 반지름), 드론 0·64/64(메시 폭). 판정은 구보다 납작할 수 없어 벽보행 윗면 위 약 40cm는 남는다(반지름을 줄이면 옆 테두리가 빠진다).
  - 락온 마커 전용 오프셋은 되돌렸다 — 락온 대상 위치가 판정 중심(몸 가운데)이 됐다. HP 바만 Config `HpBarUpOffset`(벽보행 -45, 윗면 위 약 25cm)을 쓴다.
- 시체 넉백(사용자 요청, 드론 공통): 투사체는 이미 시체를 맞히고 넉백 속도를 넣었지만 사망 이동은 착지(`bDeathLanded`) 뒤 다시 적분하지 않았다. 착지 때 속도가 0이 되므로 착지 뒤 속도가 생기면 착지 상태를 풀게 했다(비행·벽보행 이동 프로세서). 근접은 사망 개체를 판정에서 빼므로 원거리만 해당하고, 지상 적 시체는 바꾸지 않았다.

### 검증

- 새 UPROPERTY라 에디터 종료 뒤 Build.bat 전체 빌드 성공(경고 없음). 관련 자동화 39/39 PASS(MCP 실행, 단위 4와 같은 묶음).
- 시체 넉백은 Live Coding으로 확인한 뒤 에디터 종료 후 Build.bat으로 다시 빌드했다.
- 검증 옥탄트 PIE 사용자 확인: 판정(몸 위 허공 축소), HP 바·락온 마커 위치, 사망 시 발광 꺼짐, 시체 넉백 모두 정상. 드론 판정 축소는 이 편성(전부 벽보행)에서 보지 않았다.
- 단위 5를 완료한다.

## 2026-10-06 — 단위 6 착수: 세분과 6a(harness 벽보행 분리)

사용자가 Phase 14 진행을 지시했다. 착수 조사와 세분(6a harness → 6b 벽보행 매트릭스 → 6c 통합 편성 → 6d 최종 Gate)은 실행 문서 "단위 6 세분"이 소유한다.

### 구현

- 실행 인자 `-LNPLoadBaselineCrawlers=C`: 세 번째 링(seed 오프셋 15485)의 지면점에 `DA_EnemyEntityConfig_PureEntity_Crawler01`을 지상 적처럼 둔다. 벽보행이 있으면 플레이어는 링 중심(지각)에 선다.
- 지상 분포·이동 이벤트·합성 넉백 쿼리에서 벽보행을 뺐다. 벽보행 합성 넉백(`DriveCrawlers`)은 지상과 같은 빈도·세기이며 방향은 표면 접평면, Up 성분은 표면 법선이다. 표면 방향 분포(법선과 구 중심 Up 내적 ±0.7071로 바닥·벽·천장, 공중)를 capture 시작·끝에 남긴다.
- 활동 표본·CullWander에 `Crawl*` 필드를 줄 끝에 붙였다(기존 Phase 13 분석기 정규식 호환). 보고 머리줄과 `Active:` 줄에 `crawlers=`를 더했다.
- 관통 검사(`LNP.SurfaceNav.LoadBaseline.FlightPenetrationCheck`)를 벽보행에도 적용하고 따로 집계한다. 벽보행은 표면 법선(회전 Z) 쪽으로 1cm sweep한다.
- 벽보행 이동 누계 `LNPSurfaceCrawlStats`(우회 진입·표면 상실·착지·재귀속)를 이동 프로세서 청크 끝에서 더한다. 보고는 개체·프레임당 `SurfaceCrawl`·LoS query 수와 상태 평균을 낸다. 호출당 비용은 기존 `[WorldCollision] SurfaceCrawl` 줄이다.
- 실행기는 `RunPhase13Gate.ps1`에 `C100/C300/C500/N500C100` 조건만 추가했다.

### 시행착오

- 첫 스모크(벽보행 100, 관통 On)에서 벽보행 관통이 205틱·최대 2개체로 나왔다. 상세 16건 모두 깊이 1.5e-5cm 이하, Static Blocker ISM(Face −1, 프랍)이다. 기어가며 옆 프랍에 몸이 닿는 접촉이지 파고듦이 아니다(Phase 13 비행 관통은 5~22cm). 벽보행은 깊이 0.1cm 초과만 관통으로 세고 이하는 `contactEntityFrames`로 따로 남긴다. 비행 판정은 바꾸지 않았다.

### 검증

- 에디터 전체 빌드(Build.bat) 성공. 관련 자동화 39/39 PASS, 종료 코드 0(`Saved/Logs/Phase14_Unit6a_Tests.log`, 단위 4·5와 같은 묶음).
- 에디터 `-game` 스모크(1P, nullrhi, seed 1, 관통 On — 측정값이 아니라 보고 줄 확인용):
  - 벽보행 100(`Phase14_Unit6a_C100Smoke.log`): 100/100 배치·전량 Active. `SurfaceCrawl` query 1.60회/개체·프레임, 호출당 5.80us. 합성 넉백 285·착지 286·표면 상실 0·우회 0·재귀속 8. 상태 평균 None 63/Alert 17/Confirmed 20. 표면 끝 분포 바닥 89·벽 8·천장 1·공중 2.
  - 지상 100 + 비행 50 + 벽보행 100(`Phase14_Unit6a_MixedSmoke.log`): 세 링 전량 배치. 활동 표본이 지상 100·비행 50·벽보행 100으로 나뉘고, 지상 분포 합(99, 나머지는 승격 Actor)에 벽보행이 섞이지 않았다. 비행 관통 0. **벽보행 관통 1틱·1개체(깊이 0.115cm, Static Blocker ISM)**, 접촉 254 개체·틱. `UnknownHits=0`.
- 발견: 링 벽보행은 Home(링 중심)에서 배회 거리 밖이라 비교전 상태로 10초 정체하면 가까운 실제 Pod로 재귀속한다(30초에 2~8기, `Bound`가 0에서 늘어난다). 링 지상 적에는 없는 harness 차이로 기록만 하고 고치지 않았다.
- 남은 일: 6b 패키지 측정에서 벽보행 관통 표본을 늘려 0.115cm 관통의 재현·원인(착지 재부착·분리 밀기·프랍 모서리)을 가른다.

## 2026-10-06 — 단위 6b: 벽보행 매트릭스 측정

사용자가 6b 진행을 지시했다.

### 패키지

- 성능 패키지 `Saved/SurfaceNavigationPhase14CrawlPackage`(BuildCookRun exit 0, 154초, Phase 13과 같은 맵·CookDir), exe SHA256 `AA5D77D47803297A33FAA0DFD78420715A3D0A71FD71B37F58418DDF73AFB6DF`.
- 관통 진단 패키지 `Saved/SurfaceNavigationPhase14CrawlDiagPackage`: 성능 패키지에 벽보행 관통 상태 로그(`CrawlerPenetrationState`: 공중 속도·붙은 면 법선·겹친 면과의 내적)만 더했다. 관통 On 실행 전용이다.

### 성능(관통 Off, 리슨 2P·nullrhi·corelimit 4·seed 1·10초 준비/30초 capture·투사체 목표 500)

원본 `Saved/Profiling/Phase14/CrawlMatrix/Perf`. 8회 host/guest exit 0, 기능 검사(Probe 3종·Unknown/이탈/층 도약 0) 통과. 값은 반복 1/2.

| 조건 | frame P95(ms) | exact 진단 P95(ms) | `SurfaceCrawl` query/개체·프레임 | 호출당 | 게스트 수신(KB/s) | 상태 평균 None/Alert/Confirmed |
|:---|:---|:---|:---|:---|:---|:---|
| 벽보행 100 | 4.69/4.63 | 2.054/1.854 | 1.071/1.070 | 5.91/5.92us | 21.2/25.7 | 19/45/36, 14/49/37 |
| 벽보행 300 | 5.50/5.59 | 2.847/3.585 | 0.876/1.060 | 5.69/5.55us | 54.6/52.4 | 11/250/40, 76/185/39 |
| 벽보행 500 | 6.49/6.49 | 4.408/4.097 | 1.115/0.983 | 5.41/5.47us | 99.7/99.0 | 57/403/40, 41/419/40 |
| 지상 500 + 벽보행 100 | 8.85/9.22 | 2.982/3.018 | 0.669/0.624 | 5.76/5.68us | 86.8/99.2 | 4/76/21, 4/82/15 |

- 전 실행 frame/path PASS. exact 진단은 벽보행 100 첫 반복부터 2ms 근처이고 300 이상은 초과다(투사체 500발 포함).
- 벽보행 100기당 서버 frame P95 약 +0.45ms(100→500). Phase 3c 비행 100기당 1.0~1.5ms보다 작다. Confirmed는 플레이어 2명 × 원거리 슬롯 20 = 40에서 포화한다.
- 게스트 수신량은 벽보행 1기당 약 200B/s다(비행 단독 300기 32~37KB/s의 약 1.7배). 전량 Active 링이라 배회 구간 복제 절감이 없는 조건이다.
- 한계: Meadow 링은 탁 트인 지각이라 표면 분포가 바닥 약 85%·벽 10%·천장 0~1%다. 우회 진입·표면 상실 0이다. 벽·천장을 많이 타는 비용은 이 매트릭스에 거의 없다. 합성 넉백 1기당 30초 약 3회와 착지 재부착은 들어 있다. 링 벽보행 재귀속은 실행당 0~12기다(6a 발견).

### 관통(진단 패키지, 관통 On, 같은 조건 1회씩)

원본 `Saved/Profiling/Phase14/CrawlMatrix/Penetration`. On 실행의 frame·exact는 성능에 쓰지 않는다. 비행이 없는 조건이라 비행 관통 줄은 해당 없다.

| 조건 | 검사 틱 | 관통 틱/개체·틱 | 접촉 개체·틱(깊이 ≤ 0.1cm) | 기록 개체 |
|:---|:---|:---|:---|:---|
| 벽보행 100 | 6,661 | 62/62 | 3,254 | 1기 — 깊이 0.114cm, 공중 속도 56, 붙은 면과 내적 0.443 |
| 벽보행 300 | 4,994 | 28/28 | 891 | 2기 — 0.281cm 붙음(속도 0)·내적 −0.493 / 0.185cm 공중 속도 236·내적 0.444 |
| 벽보행 500 | 3,825 | 0/0 | 1,761 | — |
| 지상 500 + 벽보행 100 | 4,399 | 0/0 | 375 | — |

- 모든 관통 hit는 Static Blocker 전용 프랍 ISM(Roles 2, Face −1)의 **붙은 면이 아닌 옆면**이고 깊이는 0.11~0.28cm다. 비행 관통(5~22cm, 휴면 내내 유지)과 성격이 다르다. 개체·틱 합이 관통 틱과 같아 한 틱에 둘 이상 겹친 적은 없다.
- 코드 확인으로 본 후보(확정 아님): (1) 공중 — `StepAirborne`은 hit 자리(`Hit.Location`, 틈 0)에 멈추고 다음 틱 시작 겹침을 깊이+0.5cm로 풀었다가 다시 hit 자리로 돌아오므로, Blocker 옆면을 미끄러지는 동안 얕은 겹침이 이어질 수 있다. 지상 적도 같은 함수지만 지상은 관통 검사를 하지 않는다. (2) 붙음·정지 — `LNPSurfaceCrawl::Step`은 이동량이 0이면 query 없이 돌아가 겹침을 풀지 않는다. 오목한 틈에서 `SettleOn`이 붙은 면 법선으로 1cm 띄울 때 맞은편 프랍 면에 얕게 들어가면 정지한 동안 남는다.

### 사용자 결정과 관통 원인 확정

- 사용자 결정: 얕은 관통도 Gate 기준(관통 0)대로 원인을 확정하고 고친다.
- 겹친 source를 게임 스레드 overlap으로 찍어(진단 패키지 전용) 엔진 기본 도형 HISM 프랍 `SM_Cone`·`SM_Cylinder`(`LNPStaticBlocker`)로 특정했다. seed 1에서 같은 두 지점이 반복 재현된다. 임시 궤적 로그(직전 틱·이후 60틱)로 경로를 확인한 뒤 궤적 코드는 지웠다.
  - 원뿔 둘 사이(공중): 경사면(Up 내적 0.443) 둘이 만드는 골이 몸 지름보다 좁아 바닥에 닿지 못한다. 둘 다 걸을 수 없는 Blocker라 `StepAirborne`이 착지하지 않고, 매 틱 한 면에서 겹침을 풀면 다른 면에 들어가며 수십 틱 공중에 남는다.
  - 원기둥 둘과 바닥 사이(붙음, Alert 접근 중): 붙는 면이 매 틱 세 면을 돈다. `SettleOn`이 고른 면의 법선으로 1cm 띄우면서 옆 면에 0.1~0.15cm 들어간다.
- 평면 상자 V자 틈·단일 원뿔·단일 원기둥 자동화는 재현하지 못했다. 같은 모양의 **주머니**(맞닿은 원뿔 둘, 60cm 간격 원기둥 둘 + 바닥)로 재현했다: 원뿔은 240틱 착지 실패, 원기둥은 96틱·최대 0.76cm 관통.

### 수정

- 착지 면(D-066 해석): 벽보행은 걸을 수 있는 각도·Support 역할과 무관하게 붙을 수 있는 면(Known Support·Blocker)에 착지한다. `LNPEnemyExactMovement::FParams::bLandOnAnyKnownSolid`(기본 false — 지상 적 불변)와 `StepAirborne`의 착지 법선 출력(기본 nullptr)을 더했다. 착지 뒤 착지 면 법선 반대쪽으로 붙을 표면을 찾는다.
- 표면 틈: `SettleOn`이 hit를 낸 sweep의 시작점을 받아, 지나온 빈 길로 물러나 틈을 만든다(추가 query 없음, 법선 거리 기준 틈만큼·최대 틈의 4배·지나온 거리까지).
- 1차 수정본 재측정(`Saved/Profiling/Phase14/CrawlMatrixFix`, exe `3AB97291…E7D7`): 관통 틱 C100 0·C300 3·C500 1·혼합 0. 남은 4틱은 모두 붙은 채 정지한 개체였다. (1) 착지 hit 자리가 볼록 hull 안으로 0.13cm 들어가(붙은 면과 내적 1.000) 이어지는 `Attach`가 시작 겹침으로 실패하고 그대로 남았다. (2) `Step`의 겹침 해소가 한 면 법선으로만 밀어 주머니의 다른 면에 들어갔다.
- 2차 수정: `LNPSurfaceCrawl::Depenetrate` — 밀어낸 뒤 다시 검사하며 최대 8회 반복하고, 직전에 민 방향과 마주 보는 면에 걸리면 두 방향의 이등분(주머니 출구)으로 민다. `Step`의 두 겹침 해소 분기와 착지 직후 `Attach` 실패에 쓴다. 겹침이 없을 때 평상시 이동 query는 늘지 않는다. 면 법선만 쓰는 첫 구현은 마주 보는 원기둥 사이에서 4회 안에 빠져나오지 못했다(자동화 실측).
- 자동화: `SurfaceCrawl.CreviceContact` 신규(V자 틈·급경사 미끄럼·원뿔·원기둥·원뿔 주머니·원기둥 주머니·주머니 겹침 해소). 기존 테스트 fixture도 같은 착지 규칙을 쓴다. 관련 자동화 **40/40 PASS**, 종료 코드 0(`Saved/Logs/Phase14_Unit6b_Fix3_Tests.log`).

### 2차 수정본 재측정

패키지 `Saved/SurfaceNavigationPhase14CrawlFix2Package`, exe SHA256 `665F8091C5FE276BDC377223BEC6223BF4CB2E2473712CD18BB8056EE512AE5A`(관통 진단 로그 포함 — 관통이 없으면 출력 없음). 원본 `Saved/Profiling/Phase14/CrawlMatrixFix2`. 12회 host/guest exit 0, `UnknownHits=0`.

| 조건 | frame P95(ms) | exact 진단 P95(ms) | `SurfaceCrawl` query/개체·프레임 | 호출당 | 게스트 수신(KB/s) | 관통 틱/검사 틱 | 접촉 개체·틱 |
|:---|:---|:---|:---|:---|:---|:---|:---|
| 벽보행 100 | 4.74/4.73 | 1.846/1.737 | 1.09/0.98 | 6.4/6.3us | 12.2/19.9 | 0/6,577 | 6,900 |
| 벽보행 300 | 5.30/5.39 | 3.311/2.691 | 0.89/0.79 | 5.1/6.0us | 40.8/49.9 | 0/4,878 | 4,165 |
| 벽보행 500 | 6.32/6.55 | 4.255/5.430 | 0.89/1.23 | 5.7/5.5us | 92.6/63.7 | 0/3,979 | 7,927 |
| 지상 500 + 벽보행 100 | 8.72/9.12 | 2.880/2.845 | 0.63/0.49 | 6.0/6.4us | 84.4/78.0 | 0/3,452 | 6,783 |

- **관통 0**(총 18,886 검사 틱). 성능은 수정 전과 같은 범위다(벽보행 100기당 frame P95 약 +0.45ms, 개체·프레임당 약 1회·호출당 5~6us). 수정 전보다 착지 면이 늘어 표면 끝 분포의 벽 비율이 15~20%로 올랐다.
- 접촉(깊이 ≤ 0.1cm) 개체·틱은 조건마다 4~8천으로 1차 수정본(최대 4.9만)보다 줄었다.
- 게스트 수신량은 같은 조건에서도 반복 간 차이가 크다(100기 12~24KB/s, 500기 64~93KB/s). 개체당 비용으로 단정하지 않는다.

### 6b 예산 결정

- 사용자 결정: 6b 예산을 D-075로 기록했다.

## 2026-10-06 — 단위 6c: 1:1:1 통합 편성의 상한 측정

사용자 결정: 지상·비행·벽보행을 1:1:1로 두고 현실적인 최대 상한을 잰다.

### 편성과 실행

- `DA_MassSpawnConfig`(MCP 수정·저장): 4개 세트(Pod 20·40·30·30, 총 120) 모두 벽보행 3·근접 2·원거리 1·비행 3. Bench01에도 비행을 넣었다. 반경 1,000cm는 그대로다.
- 총수는 `-LNPEnemyDensity`로 올린다. 실행기 `RunPhase13Gate.ps1`에 `-EnemyDensity`를 더했다. 밀도 1·1.5·2·2.5·3·4·5는 반올림 뒤에도 Pod당 1:1:1이다.
- 패키지 `Saved/SurfaceNavigationPhase14MixPackage`(코드는 6b 2차 수정본과 같다, exe SHA256 `665F8091…AE5A`). 리슨 2P·nullrhi·corelimit 4·seed 1·10초 준비/30초 capture·투사체 목표 500·관통 Off. 원본 `Saved/Profiling/Phase14/Mix`.

### 결과

| 조건 | 실제 지상/비행/벽보행(총) | 활성 지상/비행/벽보행 | frame P50/P95(ms) | 16.6ms 초과 | exact P95(ms) | 게스트 수신(KB/s) |
|:---|:---|:---|:---|:---|:---|:---|
| Production 반경 1,000 밀도 2 | 611/720/707 (2,038) | 86/96/96 | 7.45/8.14 | 0 | 2.96 | 23.9 |
| Production 반경 1,000 밀도 3 | 443/1,077/961 (2,481) | 62/108/104 | 8.19/9.03 | 0 | 3.29 | 31.1 |
| Production 반경 1,500 밀도 3 | 1,055/1,079/1,077 (3,211) | 135/135/135 | 9.94/10.79 | 4 | 4.10 | 31.2 |
| Production 반경 1,500 밀도 4 반복 1 | 1,296/1,439/1,425 (4,160) | 240/276/276 | 11.95/12.84 | 6 | 5.40 | 36.8 |
| Production 반경 1,500 밀도 4 반복 2 | 같음 | 233/252/252 | **17.50/22.52** | 1,095 | 4.85 | 34.2 |
| Production 반경 1,500 밀도 4 반복 3 | 같음 | — | 11.83/12.83 | 7 | 5.27 | — |
| Production 반경 1,500 밀도 5 | 1,338/1,797/1,751 (4,886) | 226/284/285 | 12.83/13.77 | 7 | 5.76 | 44.1 |
| 전원 활성 반경 1,000 밀도 2 | 2,038 전부 | 611/720/707 | 15.14/16.09 | 31 | 11.99 | 19.4 |
| 전원 활성 반경 1,500 밀도 3 | 3,211 전부 | 1,055/1,079/1,077 | **21.60/22.98** | 1,383 | 17.62 | 24.2 |

- 모든 실행 host/guest exit 0, path PASS, `UnknownHits=0`.
- **배치가 먼저 막힌다.** 반경 1,000cm에서는 Pod 주변 지면 자리가 모자라 밀도 3에서 지상이 1,080 중 443만 들어간다. 에셋 목록에서 벽보행이 지상보다 앞이라 지면 자리를 먼저 차지한다. 반경 1,500cm면 밀도 3까지 거의 정확히 1:1:1이고, 밀도 5에서는 다시 지상 1,338/1,800·벽보행 1,751·비행 1,797로 부족하다.
- 활동 대역이 켜진 Production은 활성 수(이번 조건에서 270~790)가 비용을 정한다. 밀도 5(4,886기)까지 frame P95 PASS다. 전원 활성은 약 2,000기(종류별 약 700)가 경계이고 3,211기는 FAIL이다.
- 밀도 4 반복 2는 같은 배치에서 frame P95 22.52ms였다. capture 프레임 수가 1,670으로 다른 반복(2,489·2,514)보다 적은데 exact·이동 query 총량과 호출당 비용은 오히려 작아, 적 시뮬레이션 밖의 지연으로 보인다. 반복 3에서 재현되지 않았다. 원인은 확정하지 않았고 결과는 보존한다.
- exact 진단 예산 2ms는 모든 실행이 넘는다(투사체 500발 포함, Phase 13과 같다).
- 범위 밖: 관통 On 진단, 실제 GUI 화면·렌더링 비용, 플레이어가 여러 Pod를 동시에 깨우는 이동 패턴.

### 사용자 결정

- 최종 총수: 종류별 1,080(총 약 3,200), 스폰 반경 1,500cm. D-076으로 D-074를 대체했다.
- `DA_MassSpawnConfig`(MCP 수정·저장): 4개 세트 모두 벽보행 9·근접 6·원거리 3·비행 9, `EnemySpawnRadiusAroundPod` 1,500.

## 2026-10-06 — 단위 6d: 최종 편성 검증과 스폰 겹침 수정

### 최종 편성 첫 측정

패키지 `Saved/SurfaceNavigationPhase14FinalPackage`(코드는 6b 2차 수정본, 에셋만 최종 편성). 원본 `Saved/Profiling/Phase14/Final`.

- 성능(관통 Off) 2회: 실제 1,055/1,079/1,077(3,211), frame P95 10.66/12.16ms, exact 4.16/4.44ms, path PASS, `UnknownHits=0`.
- **관통 On: 벽보행 관통 557틱**(비행 0). 지각 지형(trimesh)과의 겹침이며 1건은 4.25cm였다. 링 측정에는 없던 경우다.

### 원인

- 같은 조건의 진단 실행 5회(3·537·0·3·3틱)는 편차가 크다. 궤적 진단에서 개체 하나가 법선이 고정된 채 매 틱 약 3.6cm씩 지각 안으로 들어갔다(4.9 → 12.1cm).
- 겹침 해소 분기에 제한 로그를 넣자, 2~17cm 깊이의 해소 64건이 **모두 스폰 시각**(capture 15초 전)에 몰려 있었다. 배치 후보는 지상 적(반지름 35 캡슐) 기준인데 벽보행 몸 구(반지름 80)를 지면점 + 반높이에 두므로 옆 프랍·지형 굴곡과 깊게 겹친 채 생성된다. 활성 개체는 첫 이동에서 풀지만 휴면·정지 개체는 query를 하지 않아 그대로 남는다. 반경 1,500·Pod당 9기에서 경사·프랍 근처 배치가 늘어 드러났다. 이등분 해소는 이 64건 중 1회도 쓰이지 않았다(모두 1회 반복으로 법선 방향 해소).
- 수정: `ULNPMassSpawnSubsystem::TrySettleCrawlSpawn` — 스폰 때 `LNPSurfaceCrawl::Depenetrate`로 겹침을 풀고 발밑 표면에 `Attach`해 위치·회전·`FLNPEnemyCrawlFragment::SurfaceNormal`을 정한다. 풀 수 없으면 비행과 같이 그 개체를 스폰하지 않고 경고를 남긴다. 게임 스레드 초기 스폰에서만 query가 는다.
- 수정 뒤 진단 3회: 관통 2·0·2틱, 스폰 거부 0. 남은 건은 아래로 기운 면·벽을 따라 기어가다 바로 옆 지각 바닥에 0.13~0.2cm 겹쳤다가 2~3틱 안에 스스로 풀리는 일시 겹침이다. 이동 sweep은 그 면에서 멀어지는 방향이고 검사 sweep은 그 면 쪽 1cm라, Chaos trimesh sweep이 멀어지는 방향의 얕은 시작 겹침을 보고하지 않는 것으로 추정한다(확정 아님).
- 임시 궤적·해소 로그는 지웠다. 관련 자동화 40/40 PASS(`Saved/Logs/Phase14_Unit6d_Tests.log`).

### 최종본 재측정

패키지 `Saved/SurfaceNavigationPhase14Final2Package`, exe SHA256 `BDFB3B194EE545860F3776A81BAA67462A129B60BF6CB15038A76E879E2CDD7C`. 원본 `Saved/Profiling/Phase14/Final2`. 4회 host/guest exit 0.

| 실행 | 실제 지상/비행/벽보행 | frame P50/P95(ms) | exact P95(ms) | 관통 틱/검사 틱(벽보행·비행) | 게스트 수신(KB/s) |
|:---|:---|:---|:---|:---|:---|
| 성능 1 | 1,055/1,079/1,077 | 10.00/10.81 | 4.47 | — | 28.8 |
| 성능 2 | 같음 | 9.78/10.54 | 4.20 | — | 32.4 |
| 관통 On 1 | 같음 | (검사 비용 포함) | — | 벽보행 1/1,766, 비행 0 | 32.1 |
| 관통 On 2 | 같음 | (검사 비용 포함) | — | 0/1,731, 비행 0 | 26.5 |

- path PASS, `UnknownHits=0`. 비행 스폰 거부는 실행마다 같은 지점 1건(기존 안전 거부), 벽보행 스폰 거부 0.
- 남은 1틱은 배회 대역(Band 1) 개체의 0.17cm 겹침이다. 배회 구간은 생성 때만 검증하고 보간 중 query가 없으며(D-068), 검증 허용 오차는 법선 거리 2cm다. 180cm 직선이 반지름 30,000cm 지각에서 생기는 처짐만 약 0.14cm라 구조상 생길 수 있는 크기다.

### 사용자 PIE(최종 편성, TestMap03 리슨 2P)

- 사용자 결정: 남은 일시 겹침(0.5cm 이하, 수 틱 안 해소)을 한계로 둔다.
- 호스트·게스트에서 원거리 관찰과 근접 교전을 했다. 비행 드론(판정 반지름 64)을 포함해 어색한 점이 없었다. 프레임은 대체로 괜찮았고, 에디터 등 다른 프로그램이 떠 있어 가끔 드랍이 있었다.
- 호스트가 게스트보다 훨씬 먼 NPC까지 보인다. 리슨 서버 호스트는 모든 적을 시뮬레이션하고 자기 시각화 거리로 그리기 때문이며 현재 구조상 정상이다. 출시 전에 게스트 복제 거리로 맞춘다(사용자 결정, `../../Idea_Backlog.md` "리슨 서버 호스트의 적 표시 거리 제한").
- 결함: 이동 패널 아래 깔린 벽보행 개체가 패널과 겹쳐 보이다가 지면 밑으로 박혔다. 원인은 `Step`이 `Depenetrate` 실패를 무시하고 마지막으로 민 자리(지면 안)를 쓴 것이다. 벽보행은 패널을 지지면으로 다루지 않았다(단위 2 범위 밖).
- 사용자 결정: 압사(겹침 해소 실패 + Dynamic source면 체력 0)는 모든 NPC 공통 규칙으로 백로그에 두고(`../../Idea_Backlog.md` "움직이는 지형에 끼인 NPC 압사"), 이번에는 벽보행 패널 운반을 한다.

## 2026-10-06 — 단위 6e: 벽보행 패널 운반

### 구현

- `LNPSurfaceCrawl::Step`·`Attach`에 새로 붙은 면의 hit identity 출력(선택, 기본 nullptr)을 더했다. 정지·겹침 해소·막힘·상실 프레임에는 바꾸지 않는다.
- `LNPSurfaceCrawl::CarryOnDynamicSupport`: 지상과 같은 `ApplyDynamicSupportDelta`로 위치·회전을 옮기고 표면 법선도 회전 delta만큼 돌린다.
- 이동 프로세서: 붙어 있고 접촉이 있으면 이동 전에 운반한다. 패널이 사라지면 마지막 패널 속도로 공중. `Step`·착지 `Attach`가 새 면을 찾으면 `MakeDynamicSupportContact`로 접촉을 갱신한다(정적 면이면 비움). 공중·표면 상실 때 비운다. 접촉 필드는 지상과 같은 `FLNPEnemyFragment::DynamicSupportContact`라 활동 대역 강제 활성·구간 배회 제외가 그대로 적용된다.
- `Depenetrate` 실패 시 `Step`과 착지 붙기는 제자리에 둔다.

### 검증

- 에디터 전체 빌드 성공. 관련 자동화 **41/41 PASS**(`Saved/Logs/Phase14_Unit6e_Tests.log`): 신규 `SurfaceCrawl.DynamicSupportCarry`(이동 Step이 붙은 면 identity를 돌려줌·정지 Step은 바꾸지 않음·정적 면은 접촉 없음, 패널 이동 +X 30·롤 90°에서 위치·법선·몸 Up 운반, 패널 소멸 시 운반 거부·마지막 패널 속도).
- 사용자 PIE(리슨 2P): 패널 윗면에 올라탄 벽보행이 패널과 함께 이동하고, 패널이 공중으로 올라간 뒤에도 옆면·밑면을 오가며 따라간다. 회전 패널은 맵에 없어 화면 확인은 못 했다(자동화만).
- 패널에 깔렸던 벽보행들이 지면에 박히지 않고 패널 밑면에 붙어 함께 올라갔다. 붙을 수 있는 면이면 어디든 붙는 규칙의 결과다. 사용자가 재미있다고 보아 의도된 동작으로 둔다. 영향: 패널에 붙은 개체는 휴면하지 않는다(지상과 같은 강제 활성).

### 운반 포함 최종본 회귀와 원인 확정

- 운반 포함 패키지(`Saved/SurfaceNavigationPhase14Final3Package`) 관통 On 2회: 벽보행 1,657·548틱, 깊이 2~4cm. 성능은 P95 10.79/10.69ms로 문제없다.
- 1차 추정(운반이 지면 안으로 끌고 감)에 따라 운반 경로 sweep 검사(패널 Actor 제외, `FLNPDynamicSupportSnapshot::ActorId` 추가)를 넣었다. 2차 추정(좁은 쐐기에서 Chaos trimesh가 멀어지는 sweep의 겹침을 놓침)에 따라 낮은 틈 프레임의 결과 검사를 넣었다. 3차로 `Depenetrate`의 뒤집힌 법선 대응(같은 면에서 겹침이 줄지 않으면 반대로 밂, 같은 면일 때만 비교)을 넣었다. 셋 다 Production 관통을 줄이지 못했다(530·4·0·550틱, 1,025·296·1,410·1,415틱).
- 진단(이동 직후 겹침 프레임 기록, 겹친 면 identity·snapshot 조회): 큰 건은 정지 또는 저속 개체가 같은 자리에서 틱당 약 5.4cm씩 지면 쪽으로 밀리며 깊어진다. 겹친 면은 지각(Static)이다.
- 에디터에서 핫스팟을 slot 5 회전(pitch 180·yaw 90)으로 옥탄트 로컬 (5908, 5869, 28936)으로 환산해 Meadow 레벨을 보자, 그 자리에 **`Marker_MovingPanel_00`(움직이는 패널 마커, 로컬 (5800, 5800, 29000))**이 있었다. 원인 확정: 내려오는 패널에 깔린 벽보행 개체를 `Depenetrate`가 패널 법선(아래)으로 밀어 지면 안에 넣는다. 위쪽 겹침 검사는 지면 겹침을 놓친다. 패널 속도만큼 매 틱 깊어진다. 사용자 PIE에서 본 "패널에 깔려 지면 밑으로 박힘"과 같은 현상이다.
- 밀어낸 자리를 민 방향으로 검사하도록 바꾸고, 재현 자동화(`SurfaceCrawl.CreviceContact` 8번: 바닥 위 개체에 Blocker 판이 틱당 5cm씩 내려옴)를 넣었다. 자동화는 재현에 성공했지만 수정으로는 막지 못했다(개체가 z = −410까지 빠짐). 판이 바닥까지 내려오면 빈 공간이 없어 어떤 해소도 성공할 수 없다.
- 사용자 결정: 벽보행만 압사를 지금 구현한다(겹침 해소 실패 + 겹친 면에 Dynamic source가 있으면 체력 0, 기존 사망 경로). 지상·비행 적용은 백로그에 남긴다. 운반 경로 검사와 낮은 틈 검사·뒤집힌 법선 대응은 유지하되 효과는 압사 구현 뒤 다시 잰다.

### 벽보행 압사 구현

- `EStepResult::Crushed`: `Step`의 겹침 해소가 실패하고 그 과정에서 Dynamic source와 겹쳤으면 낸다. 이동 프로세서가 체력을 0으로 만들고(`ULNPHealthProcessor` 사망 경로) `LNPSurfaceCrawlStats::Crushes`를 센다. harness 보고 줄에 `Crushes=`를 더했다.
- `LNPSurfaceCrawl::ResolveStationaryOverlap`: 정지 개체의 제자리 해소. 이동 프로세서가 접촉 없는 정지 개체 중 패널 중심 1,500cm 안의 개체에만 부른다.
- `Depenetrate` 보강: Dynamic 겹침 여부 출력, 지지면 안쪽 해소 금지(`SupportNormal`), 첫 검사 방향을 붙은 면 법선 쪽으로(`Step` 호출).
- `Step` 3단계 감싸기 sweep의 시작 겹침은 막힘으로 바꿨다.
- 자동화 `SurfaceCrawl.CreviceContact` 8번을 Dynamic 판(`LNPDynamicTerrain`)으로 바꾸고 이동 중·정지 중 두 경우를 본다. 시행착오: (1) 지지면 보호 전에는 정지 개체가 매 틱 바닥으로 밀리다 바닥을 넘어 z = −229로 빠졌다. (2) 이동 중 경우가 계속 실패해 프레임별로 찍어 보니 테스트 판을 바닥이 없는 y = 2000에 둔 배치 실수였다(y = 600으로 수정). 수정 뒤 두 경우 모두 4틱 만에 압사, 바닥 침투 0. 관련 자동화 **41/41 PASS**(`Saved/Logs/Phase14_Unit6e_Tests11.log`).

### 압사 패키지 측정과 마지막 보정

- 압사 포함 패키지(`Saved/SurfaceNavigationPhase14Final6Package`) 관통 On 4회: 9·1,153·0·5틱, 압사 3·1·2·1기. 남은 1,153틱은 패널 옆면에 붙어 있던 개체가 패널이 지나간 뒤 지면에 2.3cm 박힌 채 남은 경우다. 오래된 법선(패널 옆면, 중력 Up과 내적 −0.32) 기준의 지지면 보호가 지면에서 빠져나오는 해소를 막았다.
- 보정: Dynamic 겹침이 없으면 지지면 보호 없이 한 번 더 해소한다(`LNPSurfaceCrawl.cpp` 내부 `ResolveOverlap`). 관련 자동화 41/41 PASS(`Saved/Logs/Phase14_Unit6e_Tests12.log`).
- 최종 패키지 `Saved/SurfaceNavigationPhase14Final7Package`, exe SHA256 `588B07DD8F9923C44289216625DBAB85A26D25F1AD3C3CF7387F666839C8F57A`. 원본 `Saved/Profiling/Phase14/Final7`. 6회 host/guest exit 0.

| 실행 | 실제 지상/비행/벽보행 | frame P50/P95(ms) | exact P95(ms) | 관통 틱/검사 틱(벽보행·비행) | 압사 | 게스트 수신(KB/s) |
|:---|:---|:---|:---|:---|:---|:---|
| 성능 1 | 1,055/1,079/1,076 | 9.96/10.71 | 4.13 | — | 1 | 29.3 |
| 성능 2 | 1,055/1,079/1,075 | 9.95/10.67 | 4.30 | — | 2 | 32.8 |
| 관통 On 1 | 1,055/1,079/1,076 | (검사 비용 포함) | — | 1/1,778, 비행 0 | 1 | 30.8 |
| 관통 On 2 | 1,055/1,079/1,075 | (검사 비용 포함) | — | 2/1,728, 비행 0 | 2 | 30.8 |
| 관통 On 3 | 1,055/1,079/1,077 | (검사 비용 포함) | — | 0/1,759, 비행 0 | 0 | 29.7 |
| 관통 On 4 | 1,055/1,079/1,076 | (검사 비용 포함) | — | 0/1,762, 비행 0 | 1 | 31.5 |

- 남은 3틱은 0.11~0.16cm, 정적 지형 옆면의 일시 겹침이다(사용자 수용 한계). path PASS, `UnknownHits=0`.
- 압사는 실행당 0~2기, 모두 slot 5 패널 마커 자리다. 그 자리의 벽보행 수가 압사만큼 준다(1,077 → 1,075~1,076).

## 2026-10-06 — Phase 14 완료

- 사용자 PIE: 패널 아래 깔린 벽보행이 압사하는 것을 확인했다.
- 완료 근거: 자동화 41/41(`Saved/Logs/Phase14_Unit6e_Tests12.log`), 최종 패키지(`Saved/SurfaceNavigationPhase14Final7Package`) 리슨 2P 성능 2회 frame P95 10.71/10.67ms·path PASS, 관통 On 4회 벽보행 1·2·0·0틱(0.16cm 이하 일시 겹침, 사용자 수용 한계)·비행 0, `UnknownHits=0`, 사용자 리슨 2P 플레이(원거리 관찰·교전·드론 판정·패널 운반·압사).
- 결정: D-075(벽보행 비용 예산), D-076(1:1:1 종류별 1,080·스폰 반경 1,500, D-074 대체), D-077(벽보행 패널 운반·압사).
- 넘긴 일: 모든 NPC 공통 압사, 리슨 호스트 적 표시 거리 제한(`../../Idea_Backlog.md`). 회전 패널 운반은 자동화만 확인했다(회전 패널 콘텐츠 없음).
