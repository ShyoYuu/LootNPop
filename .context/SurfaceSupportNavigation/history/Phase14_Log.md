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
