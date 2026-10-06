# Phase 14 — 벽 타기 NPC(`SurfaceCrawl`)

> 상태: 진행 중 — 단위 1 완료, 단위 2 완료
> 착수: 2026-10-05
> 순서: 단위 1 → 2 → 3 → 4 → 5 → 6. 구현 단위 완료·검증 뒤 사용자 진행 확인을 받는다.

## 목표와 범위

D-066과 `design/MovementIntegration.md` "벽 타기 NPC"를 PureEntity 로봇청소기형 적으로 구현한다. Support·Nav·베이커는 바꾸지 않는다. 지상·비행 동작과 전투 규칙에 회귀가 없어야 한다. 마지막 단위에서 D-074가 넘긴 지상/비행/벽보행 최종 편성을 통합 측정으로 정한다.

범위 밖: 자발 점프·근접 공격·덮치기, ActorPromoted 벽보행 엘리트, 벽보행 전용 길찾기(Flight Corridor 류), 플레이어 Up 규약 변경.

## 구현 단위와 검증 기준

1. **적 Up을 엔티티 회전 Z축에서 읽기**: 아래 "단위 1 분류"의 소비처만 공통 헬퍼로 바꾼다. 바꾸기 전에 지상·비행 적의 회전 Z와 구 중심 방향의 최대 편차를 서버·게스트에서 상태별로 측정해, 이 리팩터가 동작을 바꾸지 않는다는 전제를 수치로 확인한다. 편차가 크면 소비처를 고치기 전에 회전을 쓰는 쪽을 먼저 고친다.
2. **SurfaceCrawl 도메인과 서버 wall-walker 이동**: `NavigationDomain = SurfaceCrawl` 태그, Mass 비의존 순수 함수 조향(접평면 투영·오목 모서리 갈아타기·볼록 모서리 감싸기), 표면 상실 시 `StepAirborne` 낙하와 재부착. 스폰은 지상 적과 같은 Pod 주변 지면 후보를 쓰고, 벽·천장으로는 배회·추격 중에 스스로 기어 올라간다(2026-10-05 사용자 결정). 벽면 전용 스폰 후보는 플레이에서 필요가 확인될 때만 계획한다. 지상 이동·분리·지상 격자에서 제외한다. 프레임당 exact 호출 수와 Unknown hit·관통 0을 자동화로 확인한다.
3. **법선 복제와 되감기**: octahedral 16bit 표면 법선 형제 필드, 법선 기저 로컬 Yaw, 클라이언트 smoothing, 위치 이력의 Up 기록(되감기 판정 캡슐 방향). 지상·비행 페이로드와 Dirty 비용이 바뀌지 않음을 확인한다.
4. **전투·인지·활동 대역**: 원거리 투사체·LoS 게이트 재사용, 360° 인지, 원거리 슬롯 풀, 3D 분리 격자, 재귀속(Home 접근 시한 초과 시 직선거리 최근접 활성 Pod), 넉백 낙하 착지, 활동 대역과 같은 표면 안 구간 배회(짧은 이동·긴 정지).
5. **외형**: ISKM 걷기 시퀀스 에셋 방식 결정·적용, HP 바·락온 마커·피격 연출이 벽·천장에서 맞는지 화면 확인.
6. **측정과 최종 Gate**: Phase 3c §3.8 매트릭스로 벽보행 개체 수 단계별 exact 호출 수·호출당 비용·관통 0을 재고 예산을 결정 원장에 남긴다. 이어 지상/비행/벽보행 통합 편성을 같은 활동·교전·viewer 조건에서 측정해 D-074를 대체한다. 관련 자동화, cook, 패키지 1P·리슨 2P, 사용자 플레이를 포함한다.

연결 검증은 수동 PIE가 더 빠르고 정확하면 먼저 사용자에게 조작을 요청한다. MCP가 효율적이면 즉시 연결을 요청하고, 전체 빌드가 필요하면 즉시 에디터 저장·종료를 요청한다.

## 단위 1 분류

규칙: **적 자신의 몸 방향**이 필요한 곳은 회전 Z로 바꾼다. **중력·지면 투영**이 필요한 곳은 그 도메인의 물리이므로 구 중심 방향을 유지한다. 벽보행 이동은 단위 2의 별도 프로세서가 소유하므로 지상·비행 이동 내부를 바꿀 이유가 없다.

### 회전 Z로 바꾼다

| 위치 | 용도 |
|:---|:---|
| `Enemy/LNPEntityAttackShared.h` `MakeBasis` | 공격 총구·조준 기저. 호출자는 이미 Transform을 넘긴다 |
| `HitDetection/LNPProjectileProcessors.cpp` 서버 적 수집·클라 ClientTargets·근접 디버그 드로 | 판정 캡슐 축 |
| `HitDetection/LNPWeaponTraceProcessors.cpp` 서버 적 수집·클라 ClientTargets | 판정 캡슐 축 |
| `HitDetection/LNPTargetQueryProcessor.cpp` | 적 탐색 질의 캡슐. 결과(`FLNPTargetQueryResult::Up`)에도 Up을 싣는다 |
| `Camera/LNPLockOnComponent.cpp` | 락온 대상 조준 높이 클램프. 질의 결과의 Up을 쓴다 |
| `Enemy/LNPEnemyMarkerProcessor.cpp` | 락온·HP 마커 기준점 |
| `UI/LNPHudWidget.cpp` HP 바 머리 위치 | 마커 엔트리(`FLNPEnemyMarkerEntry::Up`)로 전달한다 |
| `HitDetection/LNPHitDetectionShared.h` 패링 반격 넉백 | 공격자(적)의 Up 성분 |
| `Enemy/LNPEnemyProcessors.cpp` 행동 디버그 마커 | 디버그 표시 |

이미 회전 기반이라 바꿀 필요가 없는 곳: 인지 FOV(`EnemyForward`)·피격 주시 조준 pitch 게이트(`InverseTransformVectorNoScale`).

### 구 중심 방향을 유지한다

- 지상 이동·airborne·Support 투영·동적 지지면: `LNPEnemyProcessors.cpp` 이동부, `LNPEnemyExactMovement.cpp`, `LNPEnemySurfaceMovement.cpp`
- 지상 분리 접평면: `LNPEnemySpatialGrid.cpp`
- 비행 고도·조향: `LNPEnemyFlightMovementProcessor.cpp`(대상 플레이어의 Up 포함)
- 배회 구간 검증과 지상 거리 판정: `LNPEnemyWanderProcessor.cpp`, `LNPEnemyStateTreeProcessors.cpp`, `LNPWanderReplication.cpp`
- 스폰: `LNPMassSpawnSubsystem.cpp`, `LNPWorldDeviceSpawnSubsystem.cpp`
- 플레이어 판정 캡슐·플레이어 Up(`GetUpDirection`)
- 복제 회전 복원 `DecodeSphereRotation`: 단위 3에서 법선 필드와 함께 바꾼다
- 부하 harness `SurfaceNavigation/LNPLoadBaseline.cpp`
- Poise 디버그 게이지 `GAS/LNPPoiseDebugDrawProcessor.cpp`: 플레이어와 적을 같은 질의로 그린다. 벽보행 적 표시가 필요해질 때 적 쪽만 나눈다

### 단위 1에서 확인할 위험

- 회전 쓰기 경로: 지상 회전 보간(`QInterpConstantTo`)은 같은 Z축을 가진 두 회전 사이라 Z를 보존해야 한다. airborne·넉백·사망·휴면·Actor 활성화 텔레포트·게스트 Slerp 보간·배회 복원에서 회전 Z가 실제로 구 중심 방향인지 측정으로 확인한다.
  - 측정 결과: 스폰 계획이 적 회전을 지면 법선으로 만들어, 경사에서 스폰된 휴면·정지 적이 최대 29° 기울어 있었다. `SetupSpawnedEntities`에서 적 회전 Z를 구 중심 방향으로 맞췄다. 수정 뒤 모든 상태의 편차는 0.3° 미만이다.
- 되감기: 위치 이력은 위치만 기록하므로 되감은 캡슐도 현재 회전 Z를 쓴다. 지상·비행은 0.5초 사이 Up 변화가 무시할 만하지만 벽보행은 표면을 갈아타면 축이 바뀌므로 단위 3에서 이력에 Up을 기록한다.
- 판정 캡슐 축이 바뀌는 소비처는 서버·게스트 양쪽을 같은 헬퍼로 바꿔 투사체 validation 불일치를 만들지 않는다.

## 단위 2 세분

착수 조사(2026-10-05): 도메인 분기는 `FLNPEnemyFlyingTag` 하나다. 지상 이동·경로·지상 격자·분리 프로세서가 이 태그를 None으로 거르고, Config 수준 분기(`IsFlying()`)는 IdleTask 배회 목표, 배회 구간 프로세서, 활동 대역, 스폰 들어 올리기, 근접 도달성 슬롯 검사, 복제·위치 이력의 배회 인코딩에 있다. `StepAirborne`은 Up을 구 중심 방향으로 계산하고 walkable+Support hit에만 착지하므로 벽보행 낙하에 그대로 쓸 수 있다. 격자는 분리력 외 소비처가 없다.

규칙:

- 이동 몸체는 반지름 `CapsuleRadius`의 구다. 벽보행 Config는 `CapsuleHalfHeight = CapsuleRadius`이고 다르면 Trait 검증이 경고한다.
- 붙을 수 있는 표면은 hit identity가 Known이고 역할에 Support나 Blocker가 있는 면이다. Unknown에는 붙지 않는다(D-037).
- 벽보행 개체는 `SurfaceHandle`을 쓰지 않는다. 스폰 때 비운다. 그래서 단위 4 전까지 벽보행 개체가 든 Pod는 활동 대역에서 항상 활성이다. 단위 2 검증은 `DA_MassSpawnConfig` 편성 전체를 벽보행으로 바꿔 수행한다(사용자 결정, 편성은 테스트 환경마다 바꿔도 된다).
- 회전 Z는 매 프레임 현재 표면 법선과 정확히 같다. 전방만 회전 속도로 보간한다. 판정 캡슐·공격 기저가 모서리에서 늦게 따라오지 않게 한다.
- 단위 2 범위 밖: 게스트 회전·법선 복제(단위 3, 그 전까지 게스트는 구 중심 Up으로 보인다), 3D 분리·360° 인지·활동 대역·구간 배회(단위 4), 움직이는 패널 운반.

| 하위 단위 | 내용 | 검증 |
|:---|:---|:---|
| 2a 도메인 배선 | `ELNPNavigationDomain::SurfaceCrawl`, `FLNPEnemyCrawlTag`·`FLNPEnemyCrawlFragment`(법선·사망 착지), Trait 부착·검증, 지상 이동·경로·지상 격자·분리·배회 구간에서 제외, 스폰 시 `SurfaceHandle` 비우기, IdleTask 배회 목표(Home 지면점 주변, 현재 법선 접평면 거리 도착) | 빌드, 기존 관련 자동화 회귀 없음 |
| 2b 순수 이동 함수 | Mass 비의존 `LNPSurfaceCrawl::Step`: 접평면 구 sweep, 오목 모서리 hit 법선 채택, `-법선` 구 probe(볼록 모서리는 구 접촉 법선으로 감싸 돈다), probe 실패 시 뒤쪽 감싸기 sweep, 그래도 없으면 표면 상실. 신규 query 분류 `SurfaceCrawl` | 자동화: 평지·바닥→벽·벽→지붕·천장·절벽 낙하 후 재부착·Unknown 미부착, 관통 0, 프레임당 query 수 |
| 2c 이동 프로세서 | `ULNPEnemyCrawlMovementProcessor`(서버 전용): 배회·Alert·교전(원거리 정지 거리), 피격 반응·배회 타임아웃·경직·공격 중 정지, 넉백·표면 상실 시 `StepAirborne`과 착지 재부착, 사망 낙하 | 빌드·자동화, 벽보행 Config·EntityConfig 에셋, 편성 전체를 벽보행으로 바꾼 PIE 화면 확인 |
| 2d 수직 지형 검증 월드 | 벽·탑·돌출부·섬 밑면이 많은 검증 지형. 새 옥탄트 `LVI_Octant_Fixture_Crawl`(`LNP.SurfaceNav.BuildCrawlFixture`, 그래플 앵커 포함)로 정했다 | 오목·볼록 모서리·천장 이동, 추격 중 벽 오르기를 사용자 PIE로 확인 |
| 2e 단순 우회 시도 | 추격 중 목표 거리(`GoalDistanceSq`)가 일정 시간 줄지 않으면 막힘으로 보고, 현재 접평면에서 목표 방향에 수직인 옆 방향으로 잠시 기어간 뒤 일반 조향으로 돌아온다. 옆 방향은 개체마다 정하고 다시 막히면 반대로 바꾼다. 길찾기·추가 query 없이 같은 `Step`을 쓴다 | 자동화: 돌출부 밑면·판 밑면에서 윗면 목표로 우회 도착, 평지 직진 추격에 우회가 끼어들지 않음. 검증 옥탄트 PIE에서 돌출부 떨림·판 윗면 미도달이 줄었는지 사용자 확인 |

## 진행 체크리스트

- [x] 단위 1: 적 Up을 엔티티 회전 Z축에서 읽기(자동화 30/30·사용자 리슨 2P 화면 확인·편차 측정. 스폰 회전 수정 후 편차 0.3° 미만, 전체 빌드 후 30/30 재통과)
- [x] 단위 2: SurfaceCrawl 도메인과 서버 wall-walker 이동(2a~2e. 자동화 4종, 검증 옥탄트 PIE에서 벽 위·천장 추격과 일부 우회 확인)
- [ ] 단위 3: 법선 복제와 되감기
- [ ] 단위 4: 전투·인지·활동 대역
- [ ] 단위 5: 외형
- [ ] 단위 6: 측정과 최종 Gate

측정 원본·분석·한계는 `../history/Phase14_Log.md`가 소유한다.
