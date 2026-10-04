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

2026-10-04 후속 묶음 5에서 관찰·반경·PositionYaw 대조 계측을 구현했다. 빌드·관련 자동화 13개·반경 1,500cm 전량 배치·수정 패키지 네 교차 실행의 기능/프레임/경로 검사는 통과했다. 컬 안 배회 표본은 부족해 대역폭 개선은 미입증이다. 인지 제외 소스를 추가했으나 마지막 BuildCookRun 권한 요청이 거절되어 그 변경은 빌드/패키지 미검증이다. 묶음 5와 단위 5 전체를 완료 처리하지 않는다. 정확한 후속 실행 경로는 `Current.md`를 따른다.

2026-10-04 후속 묶음 4에서 사용자 승인으로 임시 `DA_MassSpawnConfig`를 배회 검증용 PureEntity 편성으로 바꿨다. Pod 120개 중 비행 편성 100개, 지상/비행 요청 720/600개다. 일반 PIE 실제 적 배치 1,306/1,320개와 사용자 양쪽 종류 배회 자연스러움을 확인했다. 14개 배치 부족·실제 종류별 수·정량 활동 표본은 남아 있어 성능/대역폭 Gate 합격으로 처리하지 않는다. PIE 종료·에디터/MCP 유지 상태에서 다음 계측 구현 진행 확인을 기다린다. 이전 패키지는 새 편성을 포함하지 않는다.

2026-10-04 후속 묶음 3에서 같은 패키지의 production/항상 활성 두 반복씩을 재측정했다. production 프레임 P95 24.80/18.23ms·항상 활성 24.18/24.13ms로 모두 프레임 FAIL·경로 PASS다. host/guest 8개 exit 0·probe 32/32·Nav CSV 정합성 통과, 불변 조건 위반 0이다. 충분한 배회 대역과 대역폭 감소는 미입증이다. 분석기 입력 폴더 선택을 보완했고 원본은 `Saved/Profiling/Phase13/ProductionRecheck`다. 묶음 3 완료 뒤 다음 시나리오 구현 진행 확인을 기다린다. 단위 5 전체·Phase 13·총수 상향은 미완료다.

2026-10-04 후속 묶음 2에서 자연 700 trace 없는 두 반복은 프레임 P95 14.70/14.95ms·경로 tick P95 0.753/0.611ms로 모두 Gate PASS다. host/guest exit 0·probe 16/16 PASS·Nav CSV 7,433/8,249행 검사를 통과했다. 실제 PIE late join, 양쪽 지상/비행 배회·접지 화면, 동일 게스트 NetID 48개(비행 6개 포함)의 bubble 제거/새 ReplicationID 재추가와 재등장 중 튐 없음도 확인했다. 후속 묶음 2는 완료했으나 production 성능·충분한 배회 대역의 대역폭/총수 Gate는 남아 있다. 다음 묶음은 사용자 진행 확인 뒤 착수하며 단위 5 전체·Phase 13·D-054는 완료/상향 처리하지 않는다. 아래는 이전 시점의 경과다.

2026-10-04 후속 비용 대조 묶음에서 실행별 Nav CSV 보존과 패키지 교차 측정·분석을 보완했다. 지상 700 추격의 trace 없는 이전/현재 두 쌍은 모두 프레임·경로 Gate PASS(이전 P95 12.50/14.40ms, 현재 14.10/14.13ms)다. 앞선 실패는 보존하며 자연/production Gate까지 통과한 것으로 확대하지 않는다. trace 대조는 큰 이동/경로/인지 비용 증가를 확인하지 못했고 승격 Actor 수가 달라 청크 원인도 확정하지 않았다. gameplay 수정 없이 이 묶음을 종료하고 다음 진행 확인을 기다린다. 자연 700 재측정·실제 late join/bubble 재진입·종류별 화면 검증과 배회 대역폭/총수 Gate는 남아 있다.

단위 2는 거리 판정 4Hz·이벤트 깨움 유지 10초, Active 60/66m·Dormant 132/126m 경계다. 실제 에셋의 시각화 Off 240m 때문에 전 멤버 가시거리+5% 밖 검사도 필요하다. 단위 3의 서버 구간 배회는 연결했으며 정적 구간 보간 중 exact 증가 0을 자동화로 확인했다. 동적 지형 slot은 안전 sweep을 유지하고 구간 후보가 막히면 3초 정지한다. 단위 4의 구간 복제·위치 Dirty 억제·클라이언트 시각 소비·되감기는 자동화 검증했고 사용자 리슨 2P 관찰에서 시각적 이상이 없었다.

2026-10-04 단위 5의 production 귀속 대조·활동 표본·복제 사유·게스트 연결 수신량 계측과 실행기를 구현했다. 빌드/cook·패키지 1P/리슨 2P·실검증 42종은 확인했지만 지상 700의 trace 없는 프레임 Gate는 미통과다. 비행 200은 두 반복 프레임·경로 Gate 통과이며 총수 상향은 보류한다. 실제 bubble 추가/제거 횟수는 기록했지만 같은 개체의 동일 게스트 재진입과 실제 late join·종류별 배회 화면 검증은 남아 있다. 계측 구현 묶음 완료 뒤, 지상 성능 분석·수정과 남은 최종 검증의 진행 확인을 기다린다. 단위 5 전체와 Phase 13은 완료 처리하지 않는다.
