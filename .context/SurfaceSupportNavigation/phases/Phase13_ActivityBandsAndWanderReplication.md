# Phase 13 — PureEntity 활동 대역과 구간 배회 복제

> 상태: 진행 중
> 착수: 2026-10-03
> 순서: Phase 8 → Phase 13. 구현 단위 완료·검증 뒤 사용자 진행 확인을 받는다.

## 목표와 범위

D-067·D-068과 `design/MovementIntegration.md`의 활동 대역·구간 배회 복제를 지상·비행 Pod 귀속 PureEntity에 적용한다. 전투 규칙과 엔티티별 복제 수명은 유지한다. Phase 14의 SurfaceCrawl은 이번 범위 밖이다. 총수 상한은 측정 뒤 결정하며 D-054의 비행 200을 미리 올리지 않는다.

## 구현 단위와 검증 기준

1. **변경 전 공통 비용 분해**: 현재 Phase 8 Development 패키지와 기존 Insights processor 범위·`LNPLoadBaselineCapture`를 재사용한다. 지상 700·비행 100/300, 리슨 2P·seed 1·투사체 500·nullrhi·corelimit 4·10초 준비/30초 측정으로 이동·인지·분리·StateTree·경로·복제와 exact 비용을 구분한다. host/guest exit·probe·Unknown/이탈/층 도약·Mass 순서 위반과 export region을 확인한다. trace 실행은 CPU 분해 자료이며 trace 없는 프레임 Gate나 총수 상한을 대체하지 않는다.
2. **Pod 단위 활동 대역과 깨움**: 서버에서 Pod와 가장 가까운 플레이어 거리로 저빈도 판정하고 귀속 PureEntity에 휴면·배회·활성을 전달한다. 거리 경계는 실제 replication/representation 설정에서 유도한다. 히스테리시스·깨움 유지시간은 이 단위의 입력 검증 때 확정한다. 피격·넉백·Pod 루팅 시작이 무리 전체를 깨우며 Pod 없음/소멸/재귀속, 교전·공중·동적 기반·사망 중의 강등 안전성을 확인한다. 첫 연결에서는 배회 대역에 기존 서버 배회를 유지하고 구간 경로가 준비되기 전 이를 최적화 완료로 보고하지 않는다.
3. **서버 구간 배회**: 부채꼴 배회 영역에서 지상은 Support/Nav 직선 보행 검사, 비행은 sphere sweep으로 구간을 검증한다. 서버 시간의 시작·도착·정지 구간을 보간하며 동적 기반을 배회 경로로 사용하지 않는다. snapshot generation·지역 revision 변경이나 피격/전투 전환 때 구간을 취소한다. 도착·장애물·층 유지·길 파괴·겹침과 활성 복귀를 검증한다.
4. **구간 복제와 클라이언트 소비**: 시작점·다음 지점·서버 시각·정지 시간의 실제 페이로드, 양자화 및 wrap 규약은 구간 구조를 확인한 뒤 확정한다. PositionYaw와 형제 필드로 두고 구간 동안 위치 Dirty를 억제한다. 최초 bubble 진입·late join·패킷 중간 수신·활성 복귀·동일 시각 보간·애니 위상·Lag Compensation을 검증한다. HP·행동·발사·피격 이벤트의 Dirty는 계속 전달한다.
5. **최종 기능·성능 Gate**: 관련 자동화, cook/stale 검사, 패키지 1P·리슨 2P·late join·사용자 플레이 검증을 수행한다. 일반 Pod 스폰과 Pod 귀속 부하에서 휴면/배회/활성 수·전환·CPU·exact·위치 Dirty 사유·게스트 대역폭을 확인한다. Phase 6/3c 조건의 trace 없는 반복으로 지상·비행 총수를 재측정하고 D-054 대체 여부를 결정한다. 계측 실행과 trace 없는 합격 실행을 구분한다.

각 단위 착수 때 필요한 파일/API·자동화 oracle을 보완한다. 단위 2~4의 연결 검증은 수동 PIE가 더 빠르고 정확하면 먼저 사용자에게 조작을 요청한다. MCP가 효율적이면 즉시 연결 요청하고 전체 빌드가 필요하면 즉시 에디터 저장·종료를 요청한다. 에디터의 현재 상태로 방법 우선순위를 정하지 않는다.

## 현재 코드에서 확인한 연결점

- 귀속: `FLNPEnemyFragment::ParentLootPod`·`ParentPodLocation`, 경로 소비자의 Home 재확인/재귀속. 무효 귀속과 `bOrphaned`를 휴면으로 취급하지 않는다.
- 이동: `ULNPEnemyMovementProcessor`, `ULNPEnemyFlightMovementProcessor`. 현재 모든 대상에 프레임별 이동을 수행한다.
- 인지·배회: `ULNPEnemyScoringProcessor`·Targeting·TargetFollow와 `FLNPEnemyIdleTask`. StateTree Idle이 새 배회 목표를 만든다.
- 분리: `ULNPEnemySeparationProcessor`와 지상/비행 공간 격자. 분리 제외와 피격/적 탐색 질의 제외는 별개이며 휴면 적의 질의 가능성은 유지한다.
- 복제: `ULNPMassReplicator`의 LOD update interval·변경값 Dirty, `FLNPReplicatedAgent::PositionYaw`, bubble add/change와 `ULNPMassSmoothingProcessor`.
- 경계: `ULNPEnemyTrait::ReplicationCullDistance` 기본 12,000cm. Actor 스폰 경계는 소스 상수가 아니라 해당 EntityConfig의 visualization 설정을 확인해 유도해야 한다.
- 기존 부하 harness는 자체 스폰을 사용하며 `LNPLoadBaseline.cpp`에는 ParentLootPod 설정이 없다. 기존 부하만으로 Pod 대역을 검증하지 않는다. 후속 측정에서 무귀속 항상 활성 대조군과 실제 귀속 대역 시나리오를 구분한다.

## 진행 체크리스트

- [x] 단위 1: 변경 전 공통 비용 분해(기존 패키지 세 리슨 2P·Insights export 정상 종료, probe 24/24 PASS; 병렬 이동 내부 귀속 한계는 로그 참조)
- [x] 단위 2: Pod 단위 활동 대역과 깨움(전체 빌드·활동 4개/기존 회귀 실검증 29개·일반 스폰 게임 60초 확인; 시각화 Off 240m 안전 제한 유지)
- [x] 단위 3: 서버 구간 배회(전체 빌드·선별 실검증 35개/비용 재생 1개 SKIP·일반 스폰 게임 60초 확인)
- [x] 단위 4: 구간 복제와 클라이언트 소비(전체 빌드·실검증 39개 성공/비용 재생 1개 SKIP, 사용자 리슨 2P 배회·접근/이탈·저격 화면 확인)
- [ ] 단위 5: 최종 기능·성능 Gate와 문서 인계

측정 원본·분석·한계는 `../history/Phase13_Log.md`가 소유한다.

단위 2는 거리 판정 4Hz·이벤트 깨움 유지 10초, Active 60/66m·Dormant 132/126m 경계다. 실제 에셋의 시각화 Off 240m 때문에 전 멤버 가시거리+5% 밖 검사도 필요하다. 단위 3의 서버 구간 배회는 연결했으며 정적 구간 보간 중 exact 증가 0을 자동화로 확인했다. 동적 지형 slot은 안전 sweep을 유지하고 구간 후보가 막히면 3초 정지한다. 단위 4의 구간 복제·위치 Dirty 억제·클라이언트 시각 소비·되감기는 자동화 검증했고 사용자 리슨 2P 관찰에서 시각적 이상이 없었다. 실제 bubble 제거/재추가·late join과 성능/대역폭은 단위 5 Gate로 남긴다. 단위 5 진행 확인을 기다린다.
