# Phase 13 작업 로그

## 2026-10-03 — 단위 1: 변경 전 비용 분해

사용자가 Phase 13 착수와 구현 단위마다 진행 확인을 지시했다. README·Current·Decisions·Roadmap·MovementIntegration 및 실제 Mass 이동/배회/복제 소비자를 읽고 실행 계획을 작성했다. 단위 1은 변경 전 계측이며 gameplay 소스·에셋·설정은 수정하지 않았다. 기존 패키지의 processor 범위와 Insights CLI가 충분하므로 MCP 연결이나 빌드는 필요하지 않았다. 방법은 에디터 실행 여부가 아니라 기존 계측 수단의 충족 여부로 선택했다.

### 산출물과 실행 조건

- 실행 계획: `../phases/Phase13_ActivityBandsAndWanderReplication.md`.
- 재현 스크립트: `Scripts/Profiling/RunPhase13Baseline.ps1`, `AnalyzePhase13Baseline.ps1`. 후자는 invariant culture로 CPU 초를 파싱하고 region 길이·호출 수·inclusive/exclusive를 검사한다.
- 패키지: `Saved/SurfaceNavigationPhase8Package/Windows/LootNPop/Binaries/Win64/LootNPop.exe`, SHA256 `141337AEB87785413DABFD93AB8DC97A53ACE74360BEB8DDD39BDF222C0A2997`.
- `TestMap03?Listen`, 리슨 2P·seed 1·투사체 목표 500·`-nullrhi -nosound -corelimit=4`·cache/lateral/parallel=1. 준비 10초/측정 30초. 합성 추격 인자는 사용하지 않았다. 비행 전용 실행은 기존 harness대로 플레이어를 섬 위에 둔다.
- 지상 700·비행 100·비행 300을 각각 한 번 실행했다. 호스트만 `cpu,frame,bookmark,region,task` trace를 켰다. 게임 실행과 Insights export를 겹치지 않았다.
- 원본: `Saved/Profiling/Phase13/Before/{N700,F100,F300}_{Host.log,Guest.log,Host.utrace,Exit.json,Insights.log,Export.rsp,AllStats_LNPLoadBaselineCapture.csv}`. 분석은 같은 폴더 `Summary.json`, 패키지 hash는 `Package.json`.

### 호스트 측정 결과

| 실행 | 프레임 P50/P95(ms) | exact P50/P95(ms) | query/frame P50/P95 | 락 P95(ms) |
|:---|---:|---:|---:|---:|
| N700 | 16.97 / 19.60 | 3.907 / 4.534 | 1,389 / 1,539 | 0.152 |
| F100 | 4.88 / 5.58 | 0.848 / 1.051 | 530 / 576 | 0.044 |
| F300 | 7.57 / 9.54 | 1.744 / 2.871 | 674 / 1,010 | 0.082 |

다음은 region 안 processor 호출당 **평균 inclusive ms**다. 범위에 자식 작업과 대기가 포함되므로 서로 더해 전체 CPU라고 보고하지 않는다. exclusive와 전체 exact leaf CPU/sec도 Summary에 따로 보존했다.

| processor | N700 | F100 | F300 |
|:---|---:|---:|---:|
| 지상 이동 | 1.322 | 0.006 | 0.006 |
| 비행 이동 | 0.007 | 0.143 | 0.444 |
| 경로 소비 | 2.160 | 0.009 | 0.010 |
| StateTree | 1.690 | 0.178 | 0.236 |
| 인지 scoring | 0.425 | 0.048 | 0.123 |
| targeting | 0.440 | 0.045 | 0.109 |
| 지상 분리 | 0.496 | 0.003 | 0.003 |
| 지상·비행 공간 격자 | 0.143 | 0.108 | 0.122 |
| Mass 복제 | 0.537 | 0.175 | 0.255 |

지상은 경로 소비·StateTree·인지·분리가 주요 대상이다. 비행은 이동·StateTree·복제·공간 격자가 반복 비용을 가진다. F100→F300 프레임 P50 차를 200으로 나누면 약 13.45us/기지만, 교전/hover/장애물 상태가 달라 **순수 공통 CPU 비용으로 확정하지 않는다**. F100은 hover 466,734·clear 144,164·blocked 2, F300은 hover 907,347·clear 230,530·blocked 21,608·stuck 15·recovery 71이다. Alert/Confirmed 평균은 각각 64.5/35.5와 235/40이며 F300은 None 평균 25다. 이동 내부 비-query CPU까지 정확히 귀속하려면 worker chunk 안에 범위를 추가하는 후속 계측이 필요하다. 현재 processor 부모 exclusive만으로 이를 대신하지 않는다.

### 기능 검증과 해석 제한

- 세 host/guest와 세 Insights export 모두 exit 0. 측정 region은 각각 30.008563/30.000389/30.000874초이며 분석기의 29~31초 검사 통과다.
- panels·FaceIndex·SourceKeys·SurfaceData probe는 여섯 로그에서 총 24/24 PASS. assert·ensure·crash·UnknownHits·EnvelopeEscapes·LayerJumps와 MassPrePhysicsOrder violations는 0이다. 지상 섬 이탈/낙하 이벤트는 harness의 합성 넉백 결과이며 월드 envelope 이탈과 다르다.
- 비행 관통 검사는 꺼져 있어 관통 0을 실검증했다고 보고하지 않는다. 이번 단위에서는 Unknown·envelope·층 도약·probe만 확인했다.
- harness 자체 결과는 N700 frame/exact FAIL, F300 exact FAIL다. trace 한 번의 CPU 분해 실행으로 정규 Gate 실패/통과나 새 총수 상한을 판정하지 않는다. D-054와 Phase 8의 trace 없는 반복 Gate는 유지한다.
- 전체 exact leaf에는 투사체·다른 소비자가 함께 있다. 이를 이동 processor inclusive에서 빼서 NPC 비-query CPU로 계산하지 않는다. 병렬 부모 범위에는 작업 대기도 있다.
- Insights에는 memory channel 없이 실행한 기존 MemAlloc tag 분석 오류·Slate 리소스 경고가 있다. 메모리 결과는 사용하지 않았으며 CPU region과 CSV 완성·정상 종료를 확인했다. 기존 게임 Mover/CharacterMovementComponent 로그도 남아 있으므로 전체 로그 오류 0이라고 보고하지 않는다.
- 코드 조사에서 기존 harness는 SpawnEntity 호출에 ParentLootPod를 전달하지 않는다. 이 결과는 변경 전 항상 활성 부하 기준선이며, 후속 활동 대역은 실제 Pod 귀속 production 스폰과 전용 귀속 시나리오에서 검증해야 한다.

단위 1을 완료하고 사용자 진행 확인을 기다린다. 다음은 단위 2 Pod 단위 활동 대역과 깨움이다. 실제 visualization 거리 에셋 확인에 MCP가 효율적이면 연결을 요청하고, 전체 빌드가 필요하면 즉시 에디터 저장·종료를 요청한다.

## 2026-10-04 — 단위 2: Pod 활동 대역과 깨움

사용자가 단위 2 진행을 승인했다. 실제 EntityConfig 읽기에 Unreal MCP를 요청했고 에디터 실행·Codex 재시작 후 연결을 확인했다. 새 reflected enum·fragment/tag·processor는 전체 빌드가 필요하므로 에디터 저장·종료를 요청했고 사용자가 종료를 확인했다. 한도 중단 뒤 빌드 세션은 소멸했으며, 잔여 14 action을 다시 빌드해 `LootNPopEditor Win64 Development` 성공을 확인했다.

### 실제 에셋 거리와 구현

- MCP로 PureEntity Melee/Ranged/Flyer와 ActorPromoted Melee의 trait를 읽었다. 모두 replication 컬 12,000cm, visualization Base/Visible 거리 `[0,2000,6000,24000]`, 거리 히스테리시스 20%다. 읽기 증거는 `Saved/Tests/Phase13_Unit2_AssetDistances.json`. 에셋은 수정하지 않았다.
- 현재 시각화 Off 240m가 복제 컬 120m보다 넓어 호스트·싱글플레이의 가시성은 복제 컬만으로 보장되지 않는다. Active는 Low 60m에서 유도하고 이탈은 66m, Dormant는 컬+10% 132m 진입/컬+5% 126m 복귀로 둔다. 여기에 각 멤버가 `max(컬, Off)×1.05` 밖에 있어야 휴면을 허용한다. 따라서 현재 실제 휴면은 전 멤버가 플레이어에서 252m 밖에 있을 때 가능하다. 두 거리의 불일치를 숨기거나 가시거리를 줄이지 않았다.
- 서버 PostPhysics 활동 processor는 Pod/플레이어 거리를 4Hz로 읽고, 매 프레임 두 개체 순회로 무리 깨움·강등 조건을 검사/적용한다. 지상·비행 PureEntity가 같은 Pod 대역을 소비한다. 무귀속·무효/Popped Pod·ActorPromoted는 항상 Active다.
- 투사체 직접/스플래시·근접 생산자가 깨움 요청을 기록한다. 속도·공격·타겟·경직·Poise·Home 재확인·동적 접촉·사망·실제 Actor 존재와 Pod Looting도 그룹을 깨우며 10초 유지한다. 새 immutable Surface 게시 시 전체 Pod를 한 번 깨우는 보수적 정책이다.
- Dormant 태그는 지상/비행 이동·분리·인지·경로에서 제외한다. Idle StateTree Tick도 새 배회/질의를 만들지 않는다. 진입은 경로를 취소하고 이동·분리를 초기화하며 복귀는 새 배회와 StateTree 활성 신호를 요청한다. 피격/탐색 격자·Lag Compensation·기존 복제는 유지한다. Wander는 아직 기존 서버 배회다.

### 검증 중 발견과 수정

- 첫 빌드의 deprecated shared API 경고를 현행 `Add`로 수정했다. 첫 자동화는 126m float 경계 반올림과 테스트 실행 ContextType 누락을 발견했다. 대역 경계 계산은 double로 바꾸고 fixture 실행을 Processor 컨텍스트로 수정했다.
- 다음 선별 자동화는 기존 회귀가 통과했지만 새 Mass fixture의 세 테스트가 실패했다. 엔진의 인스턴스 목록 `CreateEntity` 오버로드가 shared composition을 추가하지 않아 활동 쿼리에 들어가지 않았다. fixture는 archetype 오버로드로 변경했다. 런타임 production 템플릿 생성은 이 fixture 경로를 사용하지 않는다.
- 초기 실패 증거: `Saved/Logs/Phase13_Unit2_Activity.log`(assert 포함), `Saved/Logs/Phase13_Unit2_Verified.log`·`Saved/Tests/Phase13_Unit2_Verified/index.json`(34 결과 중 새 테스트 3 실패; 비용 재생은 입력 CSV 없이 SKIP). 이 결과를 최종 통과로 합산하지 않는다.
- fixture를 실제 쿼리에 연결한 뒤 휴면 진입의 경로 취소에서 subsystem 접근 범위 오류를 발견했다(`Phase13_Unit2_ActivityFinal.log`). 쿼리 순회 전에 processor subsystem 참조를 확보하도록 수정했다. 다음 실행의 기능 assertion은 통과했지만 소비자 fixture 종료에 미반영 deferred command ensure가 남았다(`Phase13_Unit2_ActivityPass.log`). 소비자 실행 뒤 명시적으로 flush하도록 수정했으며 실패 로그도 보존했다.
- ensure의 정확한 원인은 fixture 컨텍스트의 기본 쿼리별 자동 flush였다. 실제 Mass executor처럼 쿼리 중 자동 flush를 끄고 processor 실행 뒤 명시적으로 반영했다. `Phase13_Unit2_Final.log`·동명 report에는 이 수정 전 DormantConsumers 1 실패가 남아 있으며 이를 합격 결과로 보고하지 않는다.

### 최종 자동화

- 마지막 `LootNPopEditor Win64 Development` 빌드 성공(10.17초, 증분 4 action). 새 reflected 구성 전체 빌드는 앞서 성공했으며 마지막 수정은 fixture 실행 규약이다.
- `Saved/Tests/Phase13_Unit2_ActivityValidated/index.json`·`Saved/Logs/Phase13_Unit2_ActivityValidated.log`: 네 활동 테스트 성공, 실패/테스트 경고/오류/assert/ensure 0, 프로세스 exit 0. startup의 기존 scalability/MCP/레이아웃 경고는 테스트 경고와 구분한다.
- Boundaries는 실제 값과 변경된 설정·정확한 경계·히스테리시스를, PodWake는 혼합 지상/비행 무리·피격 플래그·10초 유지·루팅·접근·귀속 해제를 검증한다. Safety는 넉백·Support 없음·실제 멤버 가시거리·Off 240m/컬 120m 불일치·사망·Popped를 검증한다. DormantConsumers는 실제 지상/비행 이동과 격자 processor에서 위치 유지·exact 질의 증가 0·두 격자의 질의 가능성을 검증한다.
- 수정본의 선별 전체 실행 `Saved/Tests/Phase13_Unit2_Final/index.json`에서 기존 EnemyMovement 5·ExactMovement 4·FlightSteering 2·LoS 1·Nav 실검증 15·Conditional Runtime/Publication 2 = **29개 성공**이다. RequestCostReplay 1개는 입력 CSV 미지정 SKIP다. 그 실행에는 fixture auto-flush 실패가 있으므로 단일 전체 34/34 성공으로 보고하지 않는다. 이후 활동 4개 별도 성공을 합쳐 실검증 종류는 33개다.
- 선별 실행은 테스트 에셋을 재저장하지 않는 범위로 제한했다. `Content`·`Config`의 git 변경은 없다.

### 일반 스폰 실행과 인계

`UnrealEditor-Cmd LootNPop.uproject TestMap03 -game -nullrhi -nosound -unattended -corelimit=4 -seconds=60`으로 production Pod 귀속 스폰을 실행했다. `Saved/Logs/Phase13_Unit2_ProductionSmoke.log`에 All entities spawned·서버 초기화 완료가 있고, 60초 정상 종료 exit 0·assert/ensure/크래시/스케줄 순환 0이다. 기존 CharacterMovementComponent 추출 오류 10개와 Mover 경고는 남아 있으므로 게임 전체 오류 0으로 보고하지 않는다. headless 시작/실행 확인이며 대역별 수·원거리 가시성·루팅/피격 수동 플레이나 성능 Gate를 대신하지 않는다.

요약은 `Saved/Tests/Phase13_Unit2_Summary.json`, 최종 빌드 로그는 `Saved/Tests/Phase13_Unit2_Build.log`다. `git diff --check` 통과. 에디터·검증 게임은 종료됐고 에셋 변경은 없다. 단위 2를 완료하고 단위 3 서버 구간 배회 진행 확인을 기다린다.

### 남은 범위와 해석 제한

구간 배회·구간 복제·위치 Dirty 억제는 단위 3~4다. 최종 cook/패키지/리슨 2P/late join/수동 플레이·귀속 부하와 총수/대역폭 Gate는 단위 5다. 현재 관리 비용은 매 프레임 O(N) 두 순회이고 지형 게시 깨움은 전역이다. 이번 단위로 CPU 절감량이나 총수 상한을 확정하지 않는다. 원래 무귀속 부하 harness는 항상 활성 대조군이며 Pod 최적화 검증으로 대체하지 않는다.

## 2026-10-04 — 단위 3: 서버 구간 배회

사용자의 이어서 진행 요청을 단위 3 착수 승인으로 받아 진행했다. 에디터 프로세스가 없는 것을 확인한 뒤 `LootNPopEditor Win64 Development -WaitMutex -NoHotReloadFromIDE` 전체 빌드를 실행했다. 이번 범위의 소스/API·자동화 검증은 파일과 빌드 도구로 처리했으며 live-editor MCP 작업은 없었다. 구현 단위 종료 후 다음 단위 진행 확인을 받는다.

### 구현과 설계

- 새 `ULNPEnemyWanderProcessor`가 서버 PrePhysics에서 공격/경로 소비 뒤, 기존 지상/비행 이동 전에 실행된다. `FLNPEnemyIdleFragment::WanderSegment`의 시작·도착·정지 종료 서버 시각을 보간한다. 기존 속도의 30%, 도착 뒤 3초 정지다. 다른 이동의 중복 소비를 막고 Wander의 일반 StateTree 배회 목표·지상 분리·일반 경로 요청을 제외했다. 인지·질의·위치 복제는 유지한다.
- 멤버별 부채꼴 후보를 최대 네 번 추첨한다. 지상은 현재 Layer의 HighConfidence Support, overlay 포함 Nav 직선 보행, 실제 보간 곡선의 Support/캡슐 sweep으로 구간을 확인한다. A* 우회를 만들지 않고 불가 후보는 정지 후 재추첨한다. 비행은 body를 감싸는 sphere+clearance로 전체 구간을 sweep한다.
- 같은 Pod의 현재 위치와 예약된 남은 선분의 교차를 보수적으로 거부한다. 이미 겹친 출발점은 그룹 깨움·활성 이동을 요청해 기존 분리로 복귀한다. 다른 Pod 사이 예약 조율은 없다. 밀집/복잡 지형에서 정지가 늘 수 있으며 플레이 감각과 총수/CPU는 최종 Gate에서 확인한다.
- generation·경유 Tile revision, 활동 대역·피격·교전·넉백·경직·사망·Actor·Home 재확인·동적 기반 변화가 구간을 취소한다. 현재 위치를 유지하고 기존 목표를 다시 요청한다. 최대 16개 Tile fingerprint를 고정 배열로 저장하고 넘는 후보를 거부한다.
- 정적 구간 보간에는 exact query가 없다. 동적 지지면이 있는 slot은 미래 패널 자세를 시작 sweep만으로 보장할 수 없어 실제 이동 구간의 exact sweep을 유지한다. persistent 동적 지지면은 모든 slot을 위험으로 처리한다. 해당 영향 범위를 slot보다 좁히는 최적화는 없다. 막히면 이동 전에 취소한다.
- 유효 설계는 `design/MovementIntegration.md`의 서버 구간 배회 절에 반영했다. 구간 페이로드·클라이언트 소비·위치 Dirty 억제는 단위 4이며 아직 구현하지 않았다.

### 빌드 중 수정

첫 빌드에서 Idle fragment 내부 `TArray`가 Mass의 trivially-copyable 규약을 위반했다. 동적 목록을 고정 Tile 배열로 수정했다. 후속 빌드의 동적 요소 ID 필드명 `OctantSlot` 오류를 실제 `Slot`로 고쳤다. 실패 빌드를 합격으로 보고하지 않는다. 전체 소스 컴파일 후 수정분 6 action 빌드는 9.00초 성공, 최종 테스트 추가 뒤 4 action 빌드는 8.41초 성공이다. 최종 빌드 로그는 `Saved/Tests/Phase13_Unit3_Build.log`다.

### 자동화 검증

- 최초 구간 검증은 `Saved/Tests/Phase13_Unit3_Wander/index.json`·`Saved/Logs/Phase13_Unit3_Wander.log`에 두 테스트 성공·exit 0이다. 이후 교전·넉백·Home·Groggy·겹친 출발점 검증을 같은 Runtime 테스트에 추가했다.
- 최종 `Saved/Tests/Phase13_Unit3_Regression/index.json`·`Saved/Logs/Phase13_Unit3_Regression.log`에는 보고서 36 Success·실패/테스트 경고/오류 0·exit 0이다. `RequestCostReplay`는 CSV가 없어 Success 표기지만 SKIP다. 실검증은 **35개 성공·1개 SKIP**이며 36/36 실검증으로 보고하지 않는다.
- 실검증 구성은 Activity 4·Wander 2·EnemyMovement 5·ExactMovement 4·FlightSteering 2·LoS 1·Nav 15·Conditional Runtime/Publication 2다. assert/ensure/치명 오류는 0이다.
- Wander.Runtime은 저장된 회귀 SurfaceData의 8-slot snapshot과 실제 Mass 쿼리를 사용한다. 지상/비행의 움직이는 구간 생성·도착, 정적 보간 60프레임의 ground/flight exact 증가 0, 같은 Layer 유지, 피격/활성/교전/넉백/Groggy/Home 취소·현재 위치 유지, 새 snapshot 취소, 겹친 출발점의 활성 복귀를 확인했다. 실제 등록된 Chaos blocker로 후보 전체를 막아 정지·이동 없음·Unknown hit 0도 확인했다. 테스트 world의 Nav는 저장 fixture이며 그 전체 지형 Actor를 로드한 게임 플레이 검증은 아니다.
- Wander.TimeAndRevision은 시작 전 clamp·중간 서버 시각·정지·0길이 구간, 경유 Tile만의 revision 무효화와 generation 변경을 확인했다. 동적 slot 안전 sweep의 실제 움직이는 패널 교차 연출은 이번 자동화가 직접 검증하지 않았으며 최종 기능 Gate에 남긴다.

### 일반 스폰 스모크와 한계

`UnrealEditor-Cmd LootNPop.uproject TestMap03 -game -nullrhi -nosound -unattended -corelimit=4 -seconds=60` 실행은 `Saved/Logs/Phase13_Unit3_ProductionSmoke.log`에서 All entities spawned·60초 정상 종료 exit 0·assert/ensure/스케줄 순환 0이다. 기존 PythonTestRunner startup 오류, Mover 초기화 경고, CharacterMovementComponent 추출 오류, 일부 스폰 후보 부족 로그는 남아 있다. 후보 부족 로그는 단위 2 원본에도 있다. 이 스모크를 게임 전체 오류 0이나 모든 요청 적의 스폰 성공으로 보고하지 않는다.

이번 실행은 headless 단일 게임의 production 스폰/스케줄 확인이다. 실제 대역별 수·시각적 겹침·움직이는 패널 교차·경로 파괴 플레이, 패키지 리슨 2P·late join·성능/대역폭 Gate를 대체하지 않는다. 활동/예약 관리의 매 프레임 순회·멤버 정렬, 보수적 후보 거부, 구간 생성 때 집중되는 sweep 비용은 최종 측정 대상이다. 총수 상한과 CPU 개선 수치는 변경하지 않았다.

요약은 `Saved/Tests/Phase13_Unit3_Summary.json`. Content·Config 에셋 변경 없이 `git diff --check`를 확인했다. 에디터와 검증 게임은 종료된 상태다. 단위 3 완료 후 **단위 4 구간 복제와 클라이언트 소비** 진행 확인을 기다린다.

## 단위 4 — 구간 복제와 클라이언트 소비(2026-10-04)

사용자가 구현 단위 4까지 진행을 승인했다. 구현·자동화 검증을 마쳤으며 실제 리슨 2P 확인은 대기 중이다. 에디터를 열고 Unreal MCP를 연결하겠다는 사용자 응답을 받았다. 단위 5에는 착수하지 않았다.

### 구현과 복제 규약

- `FLNPReplicatedWanderData`를 PositionYaw의 형제로 추가했다. 시작/목표는 int16 XYZ 1cm, 시작 시각은 uint32 1ms wrap, 이동/정지는 uint16 1ms, flags에 활성/비행/slot, uint16 Layer와 int16 고정 로컬 Yaw를 둔다. 시작 시각 약 49.7일 wrap의 가장 가까운 epoch를 사용하며 반주기 미만 수신 지연을 전제한다. 좌표·시간 길이 한도를 넘는 후보는 활성 이동으로 복귀한다.
- 서버는 생성한 구간에 양자화 값을 되써 클라이언트와 같은 위치 함수를 소비한다. 구간의 고정 Yaw를 저장해 현재 구면 자세 변화가 매 프레임 구간 변경으로 해석되지 않게 했다. 필드 폭 합계 25바이트는 Iris change mask·FastArray·패킷 포함 실측 대역폭이 아니다. 일반 정수 descriptor를 사용하며 native NetSerialize는 추가하지 않았다.
- 서버 복제 갱신 helper를 기존 조건에서 분리해 실제 운영 Dirty 판단을 테스트한다. 같은 구간의 PositionYaw는 유지하고 구간 시작/변경/취소·일회성 공격·피격은 주기 게이트를 우회한다. HP·Idle/Move의 기존 게이트는 유지한다. bubble 추가 시 현재 자세와 구간을 함께 시드한다.
- 클라이언트 수신은 구간 중 HP/행동 패킷으로 오래된 PositionYaw를 보간 목표에 다시 넣지 않는다. GameState의 서버 동기화 시각으로 위치와 Idle/Move 위상을 평가하며 일회성 행동은 덮지 않는다. 지상은 현재 게시 snapshot generation과 패킷 slot/Layer로 조회하고 snapshot 미준비·안전 조회 실패 때 자세를 유지한다. 구간 취소는 현재 클라이언트 자세에서 기존 최소 50ms 보간으로 활성 이동에 복귀한다.
- 위치 이력은 이동 뒤 기존 50ms·11개 링버퍼에 구간/게시 generation/접지 높이/중력 원점을 기록한다. 같은 구간·안전한 snapshot에서는 요청 시각의 구간 위치로 되감고 전환/지형 게시 경계는 기존 기록 위치 보간을 쓴다. 투사체·근접 판정과 보정 A/B 계측에서 배회 구간에는 LOD 보간 지연을 추가하지 않는다. 핑 상한·기존 근접 보정 기본 적용 여부는 유지한다.

### 빌드와 자동화

- `LootNPopEditor Win64 Development -WaitMutex -NoHotReloadFromIDE` 전체 빌드 성공(78.45초), 동일 시각 비교 테스트 보강 빌드 성공(8.71초), A/B 계측 규칙을 맞춘 최종 빌드 성공(9.29초)이다. 최종 로그는 `Saved/Tests/Phase13_Unit4_FinalBuild.log`다.
- 최초 `WanderReplication` 자동화 4/4 성공. 보강 후 선별 실행 `Saved/Tests/Phase13_Unit4_Regression/index.json`·`Saved/Logs/Phase13_Unit4_Regression.log`는 40 Success·실패 0·exit 0이다. `RequestCostReplay`는 입력 CSV 미지정으로 실제 실행되지 않으므로 **실검증 39개 성공·1개 SKIP**다. 보고서 테스트 Warning/Error 0이며 시작 시 기존 scalability/MCP/layout 경고는 별도다.
- Codec은 시작/중간/도착/정지/late join 시각, wrap 경계·여러 epoch, 재인코딩 안정성, 범위 초과 거부, 지상 snapshot 미준비 자세 보존을 확인한다. Dirty는 실제 복제 helper에서 구간 시작 즉시 전달·120프레임 위치 Dirty 0·HP/공격/피격 전달·취소 즉시 전달을 확인한다.
- Client는 실제 수신 helper와 SmoothingProcessor를 실행해 중간 bubble 시각 위치·HP 패킷 이후 후퇴 없음·Idle/Move 위상·활성 복귀의 수신 순간 무스냅과 50ms 기존 보간을 확인한다. Rewind는 구간 시각 평가·활성 기록의 지연 규칙 전환·전환 구간 기존 위치 보간을 확인한다.
- 기존 Wander.Runtime에 실제 서버 processor와 복제 위치 함수를 60개 동일 시각에서 지상/비행 모두 비교하는 oracle을 추가했다. 클라이언트 snapshot generation을 다르게 둬도 같은 slot/Layer의 위치가 일치한다. 기존 정적 exact 증가 0 검증도 통과했다. 정지 좌표 반올림의 1cm 미만 오차는 허용한다.

### 남은 실제 검증과 한계

자동화 Client는 네트워크를 통과한 실제 PostReplicatedAdd/Change 호출이 아니라 그 수신 helper와 실제 smoothing processor를 소비한다. 실제 Iris 전송·리슨 2P 화면·bubble 재진입·피격 활성 복귀·움직이는 패널 교차의 플레이 확인을 완료한 것으로 보고하지 않는다. 구현 후 전체 production 스케줄의 실제 게임 확인도 남아 있다. 수동 PIE가 더 빠르므로 검증 장치를 추가하기 전에 에디터/MCP 연결을 요청했다. 패키지/cook/stale·대역폭/CPU·trace 없는 총수 Gate는 단위 5다.

Content·Config 변경 없이 문서와 소스만 변경했다. 구간 이력을 추가해 고정 링버퍼의 개체당 메모리는 증가하며 전체 경로를 무제한 저장하지 않는다. 실제 확인 후 단위 4 완료 체크와 단위 5 진행 확인을 갱신한다. 요약은 `Saved/Tests/Phase13_Unit4_Summary.json`이다.

### 실제 리슨 2P PIE 착수(2026-10-04)

사용자가 에디터를 열고 MCP를 연결했다. 읽기 도구로 TestMap03·PIE 미실행·리슨 서버/단일 프로세스/플레이어 1명 설정을 확인하고 플레이어 수만 2명으로 임시 변경했다. MCP StartPIE의 floating mode·8초 준비 뒤 Server 0/Client 1 창, Iris 두 시스템·게스트 Join succeeded·일반 Pod 스폰 완료를 확인했다. 게스트 화면의 실제 지형·플레이어·적 렌더링도 확인했다. 시작 로그 기준 assert/ensure/fatal·스케줄 순환 0이며 기존 Mover/CharacterMovementComponent 로그와 PIE 중 모듈 로드의 Iris 경고는 남아 있다.

사용자에게 양쪽 플레이어의 Pod 거리 70~110m 배회·이동/정지 애니메이션, 게스트 원거리 이탈/재진입, 원거리 피격 후 그룹 깨움을 요청했다. 직접 플레이 결과는 아직 대기 중이므로 단위 4 완료 체크는 유지하지 않았다. 플레이어 수 설정은 이미 1명으로 복원했으며 실행 중인 두 PIE 창은 사용자의 확인을 위해 유지했다. 에셋/Config 변경은 없다.

### 사용자 리슨 2P 확인과 단위 4 완료(2026-10-04)

사용자는 호스트·게스트를 같은 Pod에서 멀리 떨어뜨리고 관찰하여 적들이 이동/정지를 반복하며 배회하는 데 어색함이 없음을 확인했다. 게스트가 가까이 접근했다 다시 멀어지는 관찰에서도 이상을 느끼지 못했다. 인식 거리 밖 저격은 적이 공격자 쪽을 바라보다 잠시 뒤 배회하는 기존 동작을 유지했다. 관찰 목적은 위치의 후퇴/튐·전환의 끊김·피격 뒤 배회 복귀 회귀였으며, 눈에 띄는 새로운 행동을 요구한 테스트가 아니었다.

이 사용자 관찰을 내부 그룹 활동 대역·bubble 제거/재추가·구간 Dirty 사유를 직접 계측한 증거로 확대하지 않는다. 지상/비행 종류별 화면 확인·실제 late join도 구분하지 않았으므로 해당 항목은 단위 5의 최종 기능 Gate에 남긴다. 단위 4는 구간 정본·실제 수신 helper·위치/애니 위상·되감기 자동화와 production 리슨 2P 연결/플레이 관찰을 합쳐 완료한다. 실제 대역폭 감소량·총수 상한 변경은 아직 주장하지 않는다.

검증 PIE를 MCP로 종료하고 IsPIERunning=false를 확인했다. 에디터/MCP는 유지하며 플레이어 수 설정은 원래 1명으로 복원됐다. 세션 로그는 `Saved/Logs/Phase13_Unit4_Listen2P_PIE.log`, 검사 요약은 `Saved/Tests/Phase13_Unit4_Listen2P_PIE_Checks.json`이다. Iris 게스트 접속 1회·일반 스폰 완료 1회, assert/ensure/fatal·스케줄 순환 0, 양쪽 DynamicTerrain MassPrePhysicsOrder 검사 각각 208529/208544회·위반 0이다. 기존 Mover/CharacterMovementComponent 로그와 Iris 경고가 있으므로 로그 전체 오류/경고 0으로 보고하지 않는다.

Current·MovementIntegration·Phase 13 체크리스트와 `Saved/Tests/Phase13_Unit4_Summary.json`을 갱신했다. Content·Config 변경 없음·git diff --check 정상이다. **단위 4 완료, 단위 5 최종 기능·성능 Gate 진행 확인 대기**다.

## 2026-10-04 — 단위 5: 귀속 계측 구현과 첫 최종 Gate

사용자의 이어서 진행 요청으로 단위 5에 착수했다. Unreal MCP 읽기 호출로 연결·PIE 미실행을 확인했고 등록된 Agent Skill에는 이번 검증에 적용할 항목이 없었다. 기존 링 harness는 ParentLootPod를 설정하지 않으므로 귀속 계측을 보완하기로 했다. 전체 빌드/패키징을 위해 에디터 저장·종료를 요청했고 사용자가 종료를 확인했다. 이번 세션의 구현 묶음은 계측·대조 실행기이며, Gate 실패에 대한 추가 수정 전에 사용자 진행 확인을 받는다.

### 구현

- `-LNPLoadBaselineProduction`: 일반 Pod 편성·귀속 스폰을 유지하고 링 스폰·강제 리스폰·합성 넉백을 제외한다. 총수는 실제 에셋과 EnemyDensity에 따르며 링의 요청 N/F로 대체하지 않는다.
- `-LNPLoadBaselineAlwaysActive`: production 측정에서만 모든 Pod 무리를 항상 활성으로 유지한다. 일반 플레이의 활동 규칙은 유지한다.
- 서버 1Hz 활동 표본은 살아 있는 지상/비행의 Dormant/Wander/Active·귀속·구간 소비 수다. 모든 전환을 세는 계측이 아니다. 서버 복제 통계는 클라이언트별 Dirty 및 중복 가능한 PositionYaw/행동/피격/HP/구간 사유와 bubble 추가/제거 횟수다. 개체별 동일 게스트 재진입의 증거가 아니다.
- 게스트 수신량은 capture 동안 `UNetConnection::InTotalBytes`의 uint32 차분이다. Actor·이벤트·패킷을 포함한 연결 전체 수신량이며 NPC 전용 wire 페이로드와 구분한다. 복제 processor가 게임 스레드를 요구하는 엔진 소스를 확인했고 서버 계측은 같은 스레드에서 갱신한다.
- `RunPhase13Gate.ps1`은 trace 없는 실행과 `-Trace`의 CPU 분해 실행을 `After`/`AfterTrace`에 분리하고 기존 원본 덮어쓰기를 거부한다. `-FixedSeed`와 Pod/링 seed 1을 함께 사용한다. `AnalyzePhase13Gate.ps1`은 실제 로그와 활동 표본을 JSON으로 요약한다.

### 빌드·자동화·기능

- 첫 에디터 전체 빌드 성공 72.27초(`Saved/Tests/Phase13_Unit5_Build.log`). 대조 조건 추가 후 BuildCookRun의 에디터·게임 빌드 성공 97.27초, 전체 BuildCookRun exit 0·198.52초(`Saved/Logs/Phase13_Unit5_BuildCookRun.log`). 새 패키지는 `Saved/SurfaceNavigationPhase13Package`, cook은 오류 0·기존 경고 13이다.
- `Phase13_Unit5_Regression` 보고서: 40 Success 중 실검증 **39개 성공·비용 재생 1개 SKIP**. `Nav.PodOverlay`에 외부 HTTP 확인 타임아웃 경고 1개가 들어갔다. 초기 대화의 경고 0 집계를 정정했다. 오류/실패/assert/ensure 0·exit 0이다.
- 별도 `Phase13_Unit5_Freshness`의 OctantBakeDeterministic·ConditionalPatch.Bake·RegressionFixture **3개 성공**, 오류/실패/assert/ensure 0·exit 0이다. RegressionFixture에도 같은 외부 HTTP 타임아웃 경고 1개가 있다. 실검증 종류 합계 42이며 단일 전체 실행 43/43로 보고하지 않는다. freshness는 기존 규약의 별도 자동화이며 plain cook에 자동 validator가 있다고 보고하지 않는다. 이번에는 cook 후 저장된 source/payload 일치를 검증했고 콘텐츠 변경은 없었다.
- 일반 귀속 에디터 `-game` 1P는 1,801개 귀속 적과 휴면/지상 구간 소비를 기록하고 정상 종료했다(`Phase13_Unit5_ProductionSmoke.log`, exit 0). 기능 스모크이며 정식 성능 Gate가 아니다.
- 패키지 1P 최초 실행은 GUI 실행 파일을 PowerShell에서 직접 실행하여 셸만 먼저 반환했다. 정상 종료 로그는 있지만 종료 코드 수집을 대신하지 않았다. 별도 `Start-Process -PassThru -Wait` 실행 `Phase13_Unit5_Package1P_Verified.log`에서 실제 exit 0·네 probe PASS·assert/ensure/Unknown/이탈/층 도약/순서 위반 0을 확인했다. `Saved/Tests/Phase13_Unit5_Package1P_Exit.json`이 종료 코드 증거다.
- 신규 패키지 17회와 이전 패키지 대조 1회, 합계 리슨 2P 18회의 host/guest exit 0이다. 로그 36개에서 probe **144/144 PASS**, assert/ensure/fatal·Unknown·Envelope·LayerJump·Mass 순서 위반 0이다(`Saved/Tests/Phase13_Unit5_PackageChecks.json`). 이는 headless 기능 검증이며 화면·late join 검증이 아니다. 기존 Mover/CharacterMovementComponent 로그는 남아 있으므로 전체 게임 로그 오류 0으로 보고하지 않는다.

### trace 없는 측정

리슨 2P·nullrhi·corelimit 4·투사체 500·10초 준비/30초 측정이다. 사용자는 다른 무거운 작업 없음과 같은 전원/성능 조건을 확인했다. 모든 원본은 보존했다.

| 실행 | 프레임 P95 ms | exact P95 ms | 게스트 연결 B/s | 판정 |
|:---|:---|:---|:---|:---|
| production 1,801개 귀속, 두 반복 | 24.71 / 24.25 | 2.838 / 2.910 | 21,003.5 / 16,542.8 | 프레임 FAIL·경로 PASS |
| 같은 스폰 항상 활성, 두 반복 | 32.35 / 31.95 | 6.133 / 6.141 | 16,505.6 / 15,676.8 | 프레임 FAIL·경로 PASS |
| 지상 700 자연, 초기 두 반복 | 18.21 / 18.27 | 4.105 / 4.066 | 77,001.6 / 78,829.7 | 프레임 FAIL·경로 PASS |
| 지상 700 추격, 초기 두 반복 | 17.92 / 17.43 | 3.637 / 3.646 | 78,671.6 / 80,059.8 | 프레임 FAIL·경로 PASS |
| 비행 200, 두 반복 | 5.57 / 4.95 | 1.125 / 1.207 | 26,951.4 / 27,421.9 | 프레임·경로 PASS |
| 지상 700 자연, 재검증 두 반복 | 19.84 / 18.60 | 3.679 / 4.139 | 80,170.9 / 76,020.0 | 프레임 FAIL·경로 PASS |
| 지상 700 추격, 재검증 두 반복 | 20.81 / 18.72 | 3.860 / 3.712 | 70,403.7 / 78,695.2 | 프레임 FAIL·마지막 경로 FAIL |
| 이전 Phase 8 패키지, 지상 700 추격 대조 1회 | 16.39 | 3.570 | 미계측 | 프레임·경로 PASS |

소수 셋째 자리 exact 원본은 해당 로그/JSON을 따른다. `After/Summary.json`·`Recheck/Summary.json`, `Control/N700Chase_Repeat1_*`가 원본이다. 비행 200 통과를 새 비행 최대 총수로 해석하거나 기존 exact P95 2ms 기준을 지상 Nav Gate에 혼합하지 않는다.

production의 실제 수는 지상 1,601·비행 200·귀속 1,801이다. 첫 반복의 휴면은 지상 1,133·비행 176, 두 번째는 지상 1,122·비행 173이다. 지상 배회/구간은 일부 12개 표본이고 비행 배회는 0이다. 대조군은 모두 Active·구간 0을 확인했다. 활동 대역 적용으로 이 workload의 프레임 P95는 대략 24% 낮아졌으나 합격은 아니다. 대부분 원거리 개체가 복제 컬 밖이라 연결 전체 대역폭 감소는 입증하지 못했다. 배회 대역폭 Gate에는 별도 관찰 거리 시나리오가 필요하다.

### CPU 분해와 미해결 사항

`AfterTrace`의 지상 700 추격·production·항상 활성 세 실행과 세 Insights export는 정상 종료했고 각 capture는 29~31초다. `AfterTrace/Summary.json`이 timer 원본 요약이다. 지상 700 추격의 활동 관리/구간 관리 호출당 inclusive는 약 0.037/0.034ms이며 위치 이력은 약 0.021ms다. 이 trace 실행 프레임 P95는 14.71ms로 trace 없는 실패 실행보다 낮았다. trace 합격으로 정식 Gate 실패를 덮지 않으며 실행 간 변동·추가 비용을 계속 조사한다.

production/항상 활성의 경로 processor 호출당 inclusive는 약 1.25/4.36ms, StateTree 0.57/1.67ms, 비행 이동 0.08/0.22ms다. 복제 0.73/0.71ms·활동 관리 0.11/0.11ms이며 구간 관리는 0.39/0.22ms다. 병렬 부모 inclusive의 대기를 순수 CPU로 합산하거나 전체 exact를 NPC 이동에서 빼서 개체당 CPU를 계산하지 않는다.

최대 청크 개체 수는 이전 패키지 86, 새 패키지 51로 줄었고 700 추격의 청크는 대조 13·신규 초기 16·신규 trace 17이다. 구간/위치 이력의 개체당 메모리 증가가 원인 후보지만 프레임 차이의 귀속을 확정하지 않았다. 근거 없이 로직을 최적화하거나 총수 상한을 바꾸지 않았다.

**계측 구현 묶음 완료, 단위 5 전체·Phase 13 미완료**다. 다음은 지상 700 Gate 초과의 대조/비용 분해·필요한 수정이며 진행 확인을 기다린다. 실제 late join·동일 게스트/개체의 bubble 재진입, 지상·비행 각각의 배회 화면, 충분한 배회 대역의 대역폭 및 총수 재검증도 남아 있다. D-054·Roadmap 완료 상태는 바꾸지 않았다. 검증 에디터/게임은 종료했고 Content·Config 변경은 없다.

## 2026-10-04 — 단위 5 후속 묶음 1: 지상 700 교차 대조와 비용 귀속

사용자의 이어서 진행 요청으로 착수했다. 사용자는 다른 무거운 작업 없음·이전과 같은 전원/성능 조건에서 측정 가능함을 확인했다. 기존 패키지와 Insights로 원인 후보를 구분할 수 있어 MCP 연결이나 새 빌드는 필요하지 않았다. gameplay·Content·Config는 이번 묶음에서 수정하지 않았다.

### 구현과 검증

- `RunPhase13Gate.ps1`에 패키지/출력 폴더/반복 시작 번호 입력을 추가했다. 기본 실행 조건과 기존 증거 덮어쓰기 거부를 유지한다. 같은 패키지의 다음 실행이 덮어쓰던 Nav 요청/timing CSV를 실행별로 보존한다.
- `AnalyzePhase13GroundAttribution.ps1`을 추가했다. trace 없는 Gate와 trace 비용을 분리하고, 보존된 Nav CSV의 제출 수·요청/timing 키 일치·중복·누락·시간 역전을 검사한다. 병렬 부모 inclusive를 순수 CPU로 합산하지 않는다.
- PowerShell 구문 검사와 실제 실행/분석 exit 0이다. 최초 임시 실행기의 프로젝트 경로 깊이가 한 단계 틀려 실행 전 실패했으며 수정 후 측정했다. 게임이 시작된 실패 실행은 없다.
- 임시 실행 순서는 이전→현재→현재→이전(trace 없음), 이전→현재(trace)다. 이전 패키지 SHA256 `141337AEB87785413DABFD93AB8DC97A53ACE74360BEB8DDD39BDF222C0A2997`, 현재 `AF435B31F283DC2D7D59B208053A7281565C7974E51D45A57EA9E48F4D2667E7`로 이전 계측과 같다.
- 여섯 리슨 2P host/guest 모두 exit 0, 로그 12개에서 probe **48/48 PASS**, assert/ensure/fatal·Unknown/Envelope/LayerJump·Mass 순서 위반 0이다. 기존 Mover/CharacterMovementComponent 로그는 남아 있다. 두 Insights export 정상 종료·capture 30.007220/30.004111초다.

### 결과

같은 지상 700 추격·seed 1·투사체 500·2P·nullrhi·corelimit 4·10초 준비/30초 측정이다.

| 실행 | frame P95(ms) | frame/path Gate | 청크 수 | 승격 Actor 평균 |
|:---|---:|:---|---:|---:|
| 이전 trace 없음 1 | 12.50 | PASS/PASS | 12 | 2.2 |
| 현재 trace 없음 1 | 14.10 | PASS/PASS | 16 | 5.2 |
| 현재 trace 없음 2 | 14.13 | PASS/PASS | 17 | 1.3 |
| 이전 trace 없음 2 | 14.40 | PASS/PASS | 13 | 1.4 |
| 이전 trace | 13.68 | 귀속 자료, 정규 반복에서 제외 | 13 | 1.8 |
| 현재 trace | 15.70 | 귀속 자료, 정규 반복에서 제외 | 17 | 9.3 |

trace 호출당 inclusive(ms)의 이전→현재는 투사체 충돌 4.467→4.521, 경로 소비 1.332→1.331, 병렬 이동 1.071→1.042, 타게팅 0.603→0.600, 복제 0.414→0.464, 위치 이력 0.013→0.022다. 신규 활동 관리 0.039·배회 관리 0.033ms다. 큰 이동/경로/인지 비용 증가는 확인되지 않았다. Actor 승격 수가 다르고 스케줄 대기가 포함되므로 이 차이를 개체당 순수 CPU나 청크 증가 효과로 해석하지 않는다. 같은 조건/seed도 프레임 수와 상태 전환까지 같다는 뜻은 아니다.

앞선 지상 추격 실패는 이번 교차 실행에서 재현되지 않았다. 이전 두 실행 사이 P95도 1.90ms 차이가 난다. gameplay 수정을 해야 할 특정 병목을 확정하지 못했으므로 근거 없는 메모리/청크 최적화를 추가하지 않았다. 이번 합격으로 앞선 실패를 삭제하거나 지상 자연/production Gate·Phase 13 전체를 완료 처리하지 않는다.

### 증거와 한계·다음 행동

- 원본은 `Saved/Profiling/Phase13/GroundAttribution/{Control,Current,ControlTrace,CurrentTrace}`의 host/guest 로그·exit·CSV·trace·Insights export다. 요약 `Summary.json`, 기능 검사 `Validation.json`, 임시 교차 실행기 `Saved/Profiling/Phase13/RunGroundAttribution.ps1`이다.
- Nav CSV 보존 보완 전에 시작된 **현재 trace 없는 반복 1**의 원본 CSV는 다음 실행에서 덮어써져 미보존이다. 뒤늦은 복사본이 반복 2의 제출 수 10,291임을 확인해 반복 1 증거에서 제거했다. 반복 1은 로그/exit/frame/path 요약만 사용하고 CSV 행 수를 null로 남겼다. 다른 다섯 실행의 요청/timing은 각각 9,569/9,767/10,291/9,738/9,248행이며 제출 수·키 일치·중복/누락/역전 검사를 통과했다.
- 자동화 테스트·cook/stale·새 빌드는 이 묶음에서 재실행하지 않았다. 기존 검증은 앞선 단위 5 기록을 따른다. D-054·총수·Roadmap 상태는 유지한다.
- **후속 대조/분석 구현 묶음 완료, 사용자 진행 확인 대기**다. 다음 묶음은 자연 700의 trace 없는 재검증과 실제 late join·동일 게스트 bubble 재진입·지상/비행 배회 화면이다. production·충분한 배회 대역의 대역폭/총수 Gate도 남아 있다. 게임/Insights 종료와 에디터 프로세스 없음 확인 후 종료했다.

## 2026-10-04 — 단위 5 후속 묶음 2 진행 기록: 실제 late join·화면 관찰

사용자의 다음 단위 승인으로 착수했다. 투사체 충돌 processor의 앞선 대조 수치와 해석 한계는 Phase 12 검토 참고용으로 `Roadmap.md`에 연결했으며 Phase 12 구현을 시작하지 않았다. 자연 700 실행은 에디터 실행을 감지해 게임 시작 전에 중단됐다. 사용자 요청에 따라 작업을 멈춘 뒤 Codex 재시작과 Unreal MCP 연결 복구 후 이어갔다.

TestMap03 floating PIE의 기존 리슨 서버/단일 프로세스/플레이어 1명 설정에서 서버를 시작하고 약 50초 뒤 내장 `Add Client`로 게스트를 추가했다. Iris 서버/클라이언트와 `Join succeeded`를 확인했다. 설정·에셋·gameplay 변경은 없다. 읽기 전용 `obj dump`로 두 world의 bubble 배열과 NetID를 확인했고 원본을 `Saved/Tests/Phase13_Unit5_Network`에 보존했다. 초기 snapshot은 같은 개체의 제거/재추가를 증명하는 자료가 아니다.

사용자는 **호스트·게스트 두 화면 모두 비행 드론 배회가 자연스럽고 이동–정지–이동 중 어색함·튐·후퇴가 없다**고 확인했다. 비행 화면 관찰은 통과다. 지상 화면·동일 게스트/NetID bubble 제거·재진입은 아직 미확인이며, 게스트만 관찰 Pod에서 약 260m 이상 이탈하고 복귀 전 멈추도록 요청했다. 자연 700 두 반복과 나머지 Gate도 남아 있으므로 이 묶음·단위 5·Phase 13을 완료 처리하지 않는다.

이후 사용자가 호스트는 유지하고 게스트만 NPC가 보이지 않을 만큼 이탈한 뒤, snapshot 확인 후 호스트 곁으로 복귀했다. 같은 `/Game/Maps/UEDPIE_1_TestMap03.TestMap03:PersistentLevel.LNPMassClientBubbleInfo_0`의 이탈 전/원거리/복귀 배열은 172/151/172개다. 기존 NetID 48개가 원거리 snapshot에서 제거됐고 복귀 snapshot에서 모두 재등장했으며 **48개 모두 ReplicationID가 변경**됐다. 템플릿별 재추가는 `0B9205...` 30·`39335F...` 6·`C28EF5...` 6·비행 config `DA9AB58F4275CA98C46515989CE22DE7` 6개다. 사용자도 이동하며 NPC가 등장하는 동안 튐이 없었다고 확인했다. 실제 동일 게스트/개체 bubble 제거·재추가 검증은 통과다. 원본은 `ClientBeforeExit.txt`(04:54:33 UTC), `ClientFar.txt`(05:03:29 UTC), `ClientReturned.txt`(05:05:34 UTC), 항목별 비교는 `BubbleReentrySummary.json`이다. 호스트 원거리 시점 bubble에는 해당 48개가 없어 그 snapshot만으로 서버 유지 여부를 증명하지 않았으며, 복귀 후 같은 NetID와 새 ReplicationID로 재추가를 판정했다. 지상 배회·접지 사용자 관찰은 아직 대기 중이다.

사용자는 이어서 **양쪽 모두 지상 배회·접지가 자연스럽다**고 확인했다. 지상/비행 화면·실제 late join·동일 개체 bubble 재진입 검증을 통과했다. PIE를 종료하고 세션 로그를 `PIESession.log`로 보존했다. `PIEAudit.json`의 assert/ensure/fatal·스케줄 순환은 각각 0이며 기존 Mover/CMC 로그는 남아 있다. 자연 700 측정을 위해 에디터 종료와 동일 전원/성능·부하 조건 확인을 요청한다. 자연 측정·후속 묶음 2는 아직 미완료다.

### 후속 묶음 2 완료 — 자연 700 재검증

사용자가 에디터 포함 무거운 작업 종료를 확인한 뒤 자연 700 리슨 2P를 trace 없이 두 반복 실행했다. 패키지 SHA256은 `AF435B31F283DC2D7D59B208053A7281565C7974E51D45A57EA9E48F4D2667E7`로 앞선 측정과 같다. seed 1·투사체 500·nullrhi·corelimit 4·10초 준비/30초 측정 조건을 유지했다. 실행기·임시 분석/검사 exit 0이며 원본·exit·실행별 Nav CSV·Summary/Validation은 `Saved/Profiling/Phase13/NaturalRecheck`에 보존했다.

| 반복 | 프레임 P95(ms) | 경로 tick P95(ms) | frame/path Gate | Nav 요청/timing 행 |
|:---|---:|---:|:---|---:|
| 1 | 14.70 | 0.753 | PASS/PASS | 7,433/7,433 |
| 2 | 14.95 | 0.611 | PASS/PASS | 8,249/8,249 |

실제 EnemyEntities는 두 실행 모두 700이다. host/guest 네 exit 0·probe 16/16 PASS·assert/ensure/fatal·Unknown/Envelope/LayerJump·Mass 순서 위반 0이다. Nav 제출 수·요청/timing 키 일치·중복·누락·시간 역전 검사를 통과했다. 기존 Mover/CMC 로그는 남아 있다. guest 연결 전체 수신량은 78,800.2/93,957.7B/s이나 무귀속 링 시나리오이며 Wander Dirty 0이므로 구간 배회 대역폭 개선 근거로 사용하지 않는다.

실제 late join·양쪽 지상/비행 화면·동일 게스트/개체 bubble 재진입과 자연 700 재검증을 끝내 **후속 묶음 2 완료**로 기록한다. 앞선 성능 실패는 삭제하지 않는다. 이번 묶음에 gameplay·Content·Config 변경이나 새 빌드/cook은 없고 에디터/게임/Insights는 종료 상태다. Phase 13 전체·단위 5·D-054·총수 상향은 미완료/유지다. **다음은 production 성능과 충분한 배회 대역의 대역폭/총수 Gate 검증**이며 사용자 진행 확인 전 착수하지 않는다.

## 2026-10-04 — 단위 5 후속 묶음 3: production 재측정과 분석 입력 보완

사용자의 이어서 진행 요청으로 착수했다. 에디터·게임·Insights와 다른 무거운 작업 종료 및 이전과 같은 전원/성능 조건을 사용자에게 확인받았다. 이번 묶음은 기존 패키지의 production 성능 재검증·활동/복제 분석으로 한정하고, 다음 부하 시나리오 구현 전에 진행 확인을 받는다. 기존 패키지로 가능한 검증이므로 MCP 연결·새 빌드/cook은 필요하지 않았다.

### 구현·실행과 기능 검사

- `AnalyzePhase13Gate.ps1`에 `InputDirectory`만 추가했다. 기본 `After` 입력은 유지하고 이번 결과는 별도 폴더에서 분석한다. 구문 검사·실제 분석·검사 exit 0, `git diff --check` 정상이다.
- 같은 Phase 13 패키지 SHA256 `AF435B31F283DC2D7D59B208053A7281565C7974E51D45A57EA9E48F4D2667E7`, seed 1·투사체 목표 500·리슨 2P·nullrhi·corelimit 4·10초 준비/30초 측정·trace 없음이다.
- 실행 순서는 **production 반복 1→production 반복 2→항상 활성 반복 1→항상 활성 반복 2**다. 최초 사용자 업데이트에서 교차 순서라고 잘못 설명했으나 실제 실행기 순서를 확인해 진행 중 정정했다. 교차 대조로 보고하지 않는다.
- 네 리슨 2P의 host/guest 8개 exit 0·probe **32/32 PASS**, assert/ensure/fatal·Unknown/Envelope/LayerJump·Mass 순서 위반 0이다. 기존 Mover/CMC 로그는 남아 있어 전체 오류 0으로 보고하지 않는다.
- Nav 요청/timing은 실행 순서대로 **1,377/1,381/4,213/4,462행**이다. 각 실행의 제출 수·행 수·키 일치·중복/누락·시간 역전 검사를 통과했다.
- 원본 로그·exit·Nav CSV·`Summary.json`·`Validation.json`·재사용한 임시 `Validate.ps1`은 `Saved/Profiling/Phase13/ProductionRecheck`에 보존했다. 임시 검사는 기존 NaturalRecheck 검사에서 예상 완료 실행 수만 4개로 변경했다.

### 결과와 제한

| 실행 | frame P95(ms) | exact P95(ms) | 게스트 연결 B/s | frame/path Gate |
|:---|---:|---:|---:|:---|
| production 1 | 24.80 | 2.898 | 23,847.8 | FAIL/PASS |
| production 2 | 18.23 | 2.219 | 22,043.5 | FAIL/PASS |
| 항상 활성 1 | 24.18 | 4.954 | 19,110.3 | FAIL/PASS |
| 항상 활성 2 | 24.13 | 4.959 | 19,943.9 | FAIL/PASS |

실제 개체는 모두 지상 1,601·비행 200·귀속 1,801이다. production 지상 휴면은 1,133/1,122·비행 휴면은 176/173으로 capture 내 일정하다. 지상 배회는 두 반복 모두 0~24개(평균 16.8/13.2), 비행 배회는 0~3개(평균 1.2)/0개다. 항상 활성은 지상 1,601·비행 200 모두 Active·휴면/배회/구간 0이다. 첫 production의 마지막 표본만 보고 비행 배회 0이라고 업데이트했으나 전체 표본 집계 후 정정했다.

production 구간 Dirty는 134/239이며 Pose Dirty는 7,319/8,081이다. 항상 활성 Pose Dirty 7,322/7,133·구간 Dirty 0이다. 사유는 중복될 수 있고 클라이언트별 합계다. 연결 전체 수신량은 production 쪽이 낮지 않으며 충분한 배회 대역도 확보되지 않았다. NPC 전용 wire 비용이나 배회 절감률을 입증하지 못했다.

production 반복 간 frame P95 차이는 6.57ms이고 첫 반복은 항상 활성보다도 높다. same seed가 동일 프레임 수·Actor 승격·행동 분포를 보장하지 않으므로 이 대조로 고정 절감률이나 새 병목을 확정하지 않는다. 휴면 적용과 exact 비용 감소는 관찰했지만 **production 프레임 Gate 실패가 재확인**됐다. 앞선 자연/추격 700 합격과 production 실패는 각각 유지한다. 이번에는 CPU trace를 재측정하지 않아 원인 귀속이나 gameplay 수정을 추가하지 않았다.

### 인계

**후속 묶음 3 완료, 다음 진행 확인 대기**다. 다음 묶음은 충분한 지상/비행 배회를 복제 컬 안에서 관찰하는 부하 시나리오와 위치 갱신 대조 조건 구현이다. 플레이어·Pod 거리와 실제 배회·구간 소비 수를 기록해 먼저 대역폭 측정 가능한 조건을 확인한다. production 실패 비용 분해·필요한 수정과 총수 Gate도 남아 있다. 단위 5 전체·Phase 13 완료와 D-054 변경은 하지 않는다. gameplay·Content·Config 변경이나 새 빌드/cook은 없고 에디터/게임/Insights 프로세스 없음 확인 후 종료했다.

## 2026-10-04 — 단위 5 후속 묶음 4: 배회 검증용 테스트 편성

사용자가 계속 진행을 승인하고 `DA_MassSpawnConfig`는 기획 확정값이 아닌 임시값이며 테스트 필요에 따라 크게 변경해도 된다고 명시했다. 에셋 구조/일반 스폰 확인은 MCP가 효율적이므로 즉시 에디터/MCP 연결을 요청했다. 사용자 에디터 실행·Codex 재시작 후 읽기 호출로 연결과 PIE 미실행을 확인했다. 등록 Agent Skill에 이번 편성 변경에 해당하는 항목은 없었다.

### 변경

- Pod 총수 120개 유지. `Bench01/Bench01_2/Bench01_3/Bench01_4` 세트 수 74/20/16/10→20/40/30/30이다. 안정 SpawnSetId와 Pod EntityConfig는 유지한다.
- 세트별 지상 PureEntity 근접 10→4·원거리 2 유지, ActorPromoted 근접 2→0이다. ActorPromoted 항목은 Count 0으로 남겼다.
- 비행 편성 Pod 46→100개, 비행 3/5/6→모두 6기다. 지상 요청 1,680→720·비행 요청 200→600, 전체 요청 1,880→1,320개다. 이전 실제 1,801개와 요청 수를 혼합하지 않는다.
- 배치 간격 2,000cm·적 스폰 반경 1,000cm·프레임별 최대 스폰 50은 유지한다. 비행 headroom·접지·활동 경계 규칙을 바꾸지 않았다. 테스트 편성이며 D-054나 합격 총수 상한 변경이 아니다.
- 최초 도구 호출은 배열 축소와 원소 값 변경을 같이 요청해 ArrayRemove 모호성으로 실패했다. PodSetCount만 부분 적용된 것을 읽기로 확인했고, 배열 크기를 유지하며 ActorPromoted Count를 0으로 바꿔 최종 상태를 적용했다. 저장/최종 읽기 일치·is_dirty=false를 확인했다. 초기 orchestration의 `structuredClone` 미지원 오류는 변경 호출 전에 발생했고 JSON 복사로 수정했다.

### 일반 PIE 확인과 제한

TestMap03 floating PIE 기존 설정(플레이어 1명)으로 스폰을 확인했다. 세트별 Pod 요청/배치 20/20·40/40·30/30·30/30, 적 요청/배치 **120/120·480/471·360/360·360/355**다. 총 Pod 120개·적 **1,306/1,320개**이고 후보 부족 14개가 있다. `All entities spawned`는 만들어진 큐 소비 완료이지 요청 수 전부 성공이라는 뜻이 아니다. 로그가 종류별 수를 기록하지 않아 실제 지상 720·비행 600으로 보고하지 않는다. 초기화 합계 1,426개는 Pod 120개를 포함한다.

사용자는 비행 드론이 있는 Pod에서 약 80~100m 관찰 요청 뒤 **드론이 늘었고 지상·비행 양쪽 배회도 자연스럽다**고 확인했다. 이번은 단일 PIE 화면 확인이며 신규 편성의 리슨 2P/게스트·패키지 성능·정량 배회 수/대역폭 검증은 아니다. 후보 부족이 있으므로 정확한 총수 성능 표본으로 사용하지 않는다.

PIE를 MCP로 종료하고 IsPIERunning=false를 확인했다. 세션 assert/ensure/fatal·Mass 순서 위반 0이다. 후보 부족 오류와 기존 Mover/CMC 로그는 남아 있어 전체 오류 0으로 보고하지 않는다. 에셋 SHA256은 `298D2CDF76C6648FDCC4BDDE7E2AD8DCED4AF1EA296AC0A09559893BE7448F6F`다. 변경 전/후 JSON·`SpawnSession.txt`·`PIESession.log`·세트별 `SpawnAudit.json`은 `Saved/Tests/Phase13_WanderComposition`에 보존했다.

**편성 변경 구현 단위(후속 묶음 4) 완료, 다음 진행 확인 대기**다. 다음은 배치 부족/실제 종류별 수와 관찰 거리·충분한 배회 표본·위치 갱신 대조 조건의 계측 구현이다. 이번 C++/Config 변경·새 빌드/cook은 없으며 이전 패키지는 이전 에셋을 담고 있다. 새 편성 패키지 검증 전에 재cook한다. 에디터/MCP는 유지한다. production 실패와 Phase 13·단위 5 전체·대역폭/총수 Gate는 미완료다.

## 2026-10-04 — 단위 5 후속 묶음 5: 배회 관찰·위치 갱신 대조 계측 (검증 미완료)

사용자의 진행 승인, 에디터 저장·종료 및 같은 전원/성능 조건 확인 후 착수했다. production 전용 스폰 반경 1,500cm override, 지상/비행 배회 관찰 위치 선택, 복제 컬 거리 안의 배회/구간 수 계측, 같은 서버 배회를 PositionYaw로 보내는 대조 조건을 추가했다. 에셋의 반경 1,000cm는 유지한다. 컬 계측은 가장 가까운 플레이어 거리 기준이며 실제 bubble membership이 아니다.

### 검증된 구현과 시행착오

- Editor 전체 빌드 성공, Activity/Wander/MassSpawn 자동화 **13개 성공**이다. PositionYaw 대조 테스트는 구간 전송 억제·위치/HP 전달·서버 입력 구간 보존을 확인한다. 이 테스트 결과는 뒤에 추가한 관찰자 고정/인지 제외 변경의 검증으로 확대하지 않는다.
- 1P smoke에서 Pod 120·적 요청/배치 **1,320/1,320**, 실제 지상 720·비행 600을 확인했다. production override로 이전 편성의 후보 부족을 해소했다.
- 최초 패키지 2P 측정은 무제한 FPS 게스트의 reliable RPC queue ensure와 관찰자 위치 이탈로 무효다. 이를 성공 표본으로 사용하지 않는다. 관찰자를 기존 Mover EnterDeadMode로 고정하고 이 두 측정 case에만 게스트 60FPS 제한, 관찰 위치/인원 검사를 추가했다.
- 수정 패키지는 build/cook 성공 후 **구간 1→PositionYaw 1→PositionYaw 2→구간 2** 순서로 실행했다. host/guest 8개 exit 0, probe **32/32 PASS**, assert/ensure/fatal·활동/지원 불변식 위반 0이다. 매 표본 지상 720·비행 600·귀속 1,320, 네 스폰 세트 전부 요청 수를 채웠고 같은 패키지·관찰 위치를 확인했다.
- Nav CSV는 실행 순서대로 206/205/220/165행이며 제출 수·키·중복/누락·시간 역전 검사를 통과했다. `git diff --check` 정상이다.

| 실행 | frame P95(ms) | exact P95(ms) | 게스트 연결 B/s | 컬 안 지상/비행 배회 평균 |
|:---|---:|---:|---:|:---|
| 구간 1 | 12.44 | 1.57 | 16,843.7 | 0.47/1.07 |
| PositionYaw 1 | 12.59 | 1.57 | 16,990.3 | 1.93/0.87 |
| PositionYaw 2 | 12.34 | 1.56 | 16,846.7 | 1.93/1.50 |
| 구간 2 | 12.40 | 1.55 | 16,943.8 | 1.13/1.43 |

네 실행 frame/path Gate는 PASS다. 구간/PositionYaw 연결 전체 평균은 16,893.75/16,918.5 B/s로 차이는 약 0.15%지만 컬 안 배회 표본이 부족하므로 **배회 대역폭 절감 입증으로 사용하지 않는다**. NPC 전용 wire 비용·production 전투 성능·최대 총수 Gate도 입증하지 않았다.

### 마지막 변경과 인계

근처 인지/활동 유지 때문에 컬 안 배회가 충분하지 않은 조건을 보완하려고 `IsWanderObservation()`을 추가하고 관찰자만 Enemy Scoring의 인지 후보에서 제외했다. 활동 대역 거리 질의는 유지하고 정상 gameplay에서는 적용하지 않는다. 이는 격리된 배회 복제 대조 조건이며 일반 production 전투 조건이 아니다.

이 마지막 변경을 담는 `SurfaceNavigationPhase13ObserverIsolatedPackage`의 BuildCookRun 권한 요청이 **사용자 거절로 실행 전에 중단**됐다. 재시도하지 않았다. 따라서 마지막 인지 제외 소스는 **컴파일·cook·자동화·리슨 측정 미검증**, 후속 묶음 5는 미완료다. 사용자가 최종 build/cook 실행을 허용하면 새 패키지로 ABBA를 재측정하고 28개 이상 표본 및 지상/비행 각각 평균 12개 이상의 컬 안 배회를 먼저 확인한다. 에디터·게임·Insights 프로세스는 종료 상태다.

## 2026-10-04 — 후속 묶음 5 재개: 관찰자 인지 제외 패키지·교차 검증 종료, 표본 Gate 미충족

사용자가 Phase 13 이어서 진행과 구현 단위별 진행 확인을 요청했다. 에디터·게임·Insights 종료를 확인하고 마지막 인지 제외 소스를 새 패키지로 빌드/cook했다. 사용자가 권한 확장 실행을 승인했고 같은 전원/성능 조건·무거운 작업 없음도 확인했다. gameplay·Content·Config 추가 수정 없이 기존 수정의 검증과 문서 인계를 수행했다.

- `Saved/SurfaceNavigationPhase13ObserverIsolatedPackage`: BuildCookRun exit 0·144.66초, 게임/에디터 빌드 성공·83.18초, cook 오류 0·기존 경고 13개. 로그는 `Saved/Logs/Phase13_ObserverIsolated_BuildCookRun.log`다. 앞선 권한 거절 기록은 과거 경과이며 이번 패키지에는 마지막 소스가 포함됐다.
- Activity/Wander/WanderReplication/MassSpawn 자동화 13/13 Success·테스트 경고/오류 0·프로세스 exit 0. 보고서는 `Saved/Tests/Phase13_ObserverIsolated/index.json`, 로그는 `Saved/Logs/Phase13_ObserverIsolated_Tests.log`다. 저장 에셋을 재베이크하는 테스트는 이번에 실행하지 않았다.
- trace 없이 구간 1→PositionYaw 1→PositionYaw 2→구간 2 순서로 리슨 2P 네 번을 실행했다. host/guest 8개 exit 0·probe 32/32 PASS·assert/ensure/fatal 0·Unknown/Envelope/LayerJump 및 Mass 순서 위반 0이다. 기존 Mover/CMC 로그는 남아 있어 게임 로그 전체 오류 0을 뜻하지 않는다.
- 매 실행 지상 720·비행 600·귀속 1,320개, 네 spawn set 전량 배치와 30개 활동/컬 표본을 확인했다. 같은 패키지 SHA256·관찰 Foot `X=-20540.406 Y=18339.610 Z=-14258.678`, `NoPerception=1`과 실제 PoseControl 모드를 확인했다. PositionYaw 대조의 구간 Dirty는 0이다.
- Nav 요청/timing 행은 실행 순서대로 221/169/138/195개다. 각각 행 수 일치·키 중복/누락·시간 역전 0이다.

| 실행 순서 | frame P95(ms) | 게스트 연결 전체 B/s | 컬 안 지상 배회/구간 평균 | 컬 안 비행 배회/구간 평균 | Pose/Wander Dirty |
|:---|---:|---:|---:|---:|:---|
| 구간 1 | 16.73 | 16,134.9 | 2.57 | 0.90 | 3,668 / 213 |
| PositionYaw 1 | 16.38 | 16,132.5 | 2.90 | 1.30 | 3,762 / 0 |
| PositionYaw 2 | 15.37 | 16,291.2 | 5.73 | 4.73 | 3,438 / 0 |
| 구간 2 | 14.48 | 16,575.9 | 2.00 | 1.50 | 3,844 / 224 |

첫 실행만 frame FAIL(기준 P95 ≤16.67ms), 나머지 frame PASS·모든 path PASS다. 표본 수 28개 기준은 충족했지만 양쪽 종류의 컬 안 배회/구간 평균 각각 12개 기준은 모두 미충족이다. 관찰자 인지 제외만으로 충분한 배회 표본을 확보하지 못했다. 구간/PositionYaw 연결 전체 평균은 16,355.4/16,211.85 B/s이며 구간 쪽 약 0.89% 증가다. 배회 분포도 실행마다 달라 이 차이를 구간 복제의 개선/회귀 효과로 해석하지 않는다. NPC 전용 wire 비용과 production 전투 성능·최대 총수 Gate도 입증하지 않았다.

원본·실행/분석 콘솔·`Summary.json`·`Validation.json`·`Comparison.json`은 `Saved/Profiling/Phase13/WanderComparisonIsolated`다. 분석기는 정상 종료했으며 `BothDomainsSampledAll=false`를 남겼다. 이 false는 합격으로 덮지 않는다.

**이번 재패키징·교차 검증 묶음 종료, 다음 진행 확인 대기**다. 후속 묶음 5의 표본 Gate와 단위 5 전체·Phase 13·D-054 상향은 미완료/유지다. 다음 구현 후보는 컬 안 NPC의 활성 유지/깨움 사유·실제 활동 거리 입력 계측과 원인에 맞는 관찰 조건 보정이다. 기존 CMC 추출 오류만으로 Mass 위치 불일치를 원인으로 확정하지 않았다. 에디터·게임·Insights 종료 상태를 확인했으며 사용자 확인 전 추가 구현에 착수하지 않는다.

## 2026-10-04 — 단위 5 후속 묶음 6 완료: 깨움 사유 진단·관찰 조건 보정·충분한 배회 대조

사용자가 다음 단위 진행을 승인했다. 활동 판정의 실제 입력과 컬 안 Pod의 활성 사유를 조사하고, 스폰 수·전투 규칙·활동/배회 안전 판정을 유지한 채 관찰 조건을 보정했다.

### 구현과 원인 확인

- `-LNPLoadBaselineActivityDiagnostics`는 ObserveWander에서만 동작한다. 활동 프로세서의 플레이어 Mass/Actor 위치, 컬 안 멤버가 있는 Pod의 양쪽 거리·활성/컬 경계·대역·강제 활성·깨움 유지시간을 1Hz로 출력한다. 사유 비트는 프레임마다 누적해 순간 이벤트도 보존하고 표본 출력 후 비운다. 배회 프로세서는 실제 깨움 요청의 UnsafeOrRevision/Overlap/Codec/Support/DynamicBlock 사유를 남긴다. 정상 플레이와 성능 대조에는 계측을 켜지 않는다.
- 첫 순간 표본은 `Force=0/Reasons=0`인데 깨움 유지시간이 반복되어 이벤트를 놓쳤다. 누적 진단으로 원인을 확인했다. 기존 관찰 위치의 플레이어 Mass/Actor 차이는 39표본 모두 0cm이며, 컬 표본에 등장한 Pod들의 실행 전체 배회 취소 이벤트는 DynamicBlock 55·Overlap 9개다. 전체 Pod에서는 83/17개다. 이는 warm-up도 포함한 진단 실행 이벤트이며 프레임별 전환 횟수나 capture 전용 통계가 아니다.
- `DynamicBlock`은 동적 위험 구간의 안전 sweep 차단이며 hit 자체가 동적 geometry였다는 뜻은 아니다. 같은 Pod의 멤버 겹침도 구간을 취소한다. 이 요청으로 10초 Pod 전체 깨움이 반복되어 가까운 배회 후보가 실제로는 Active에 머물렀다. 기존 CMC 추출 오류를 원인으로 확정하지 않는다.
- 관찰 자리 선택은 먼저 각 멤버의 시작 자세 exact capsule/sphere sweep과 같은 Pod 멤버 간 기존 보수적 구 겹침 검사를 수행한다. 막히거나 겹친 Pod를 후보 수에서 제외하고 기존 지각 32후보에 안전한 Pod 주변 고정 8방향을 더했다. Pod handle 순서로 후보를 생성해 TMap 순회 순서에 의존하지 않는다. startup probe는 capture 초기화 전이며 정상 이동·구간 안전 검사는 유지했다. 시작 자세 통과만으로 이후 배회 안전을 보장하지 않는다.
- 원인·보정 진단은 `Saved/Logs/Phase13_ActivityDiagnosticsAccumulated_Game.log`·`Phase13_ObserverEligibility_Game.log`, 구조화 근거는 `Saved/Tests/Phase13_ActivityDiagnosis.json`이다. 보정 1P 진단은 120 Pod 중 UnsafePods 44·후보 640개에서 자리를 골랐고, 30개 capture 표본의 컬 안 배회 평균은 지상 25.0·비행 26.43개다. 프로세스 exit 0·assert/ensure/fatal/불변 위반 0이다. 진단 계측을 켠 `-game` 프레임 P95 18.61ms는 성능 합격 표본으로 사용하지 않는다.

### 빌드·회귀·패키지

- 진단/누적 진단/관찰 보정 Editor 전체 빌드 성공: 46.65/46.23/12.28초. 로그는 `Saved/Logs/Phase13_ActivityDiagnostics_Build.log`·`Phase13_ActivityDiagnosticsAccumulated_Build.log`·`Phase13_ObserverEligibility_Build.log`다.
- 최종 Activity/Wander/WanderReplication/MassSpawn 자동화 **13/13 Success·테스트 경고/오류 0·exit 0**. 보고서는 `Saved/Tests/Phase13_ObserverEligibility/index.json`이다. 앞선 초기 진단 테스트를 합산해 단일 26종 검증으로 보고하지 않는다. 저장 에셋을 재베이크하는 자동화는 실행하지 않았다.
- `Saved/SurfaceNavigationPhase13ObserverEligiblePackage` BuildCookRun exit 0·115.27초, 게임/에디터 빌드 성공·55.00초, cook 오류 0·기존 경고 13개. 로그는 `Saved/Logs/Phase13_ObserverEligible_BuildCookRun.log`다. Content·Config 변경은 없고 기존 테스트 편성을 포함해 cook했다.

### 진단 없는 교차 리슨 2P 대조

앞서 확인한 같은 전원/성능 조건에서 구간 1→PositionYaw 1→PositionYaw 2→구간 2를 실행했다. 진단 인자/로그는 0이고 trace도 없다. 네 실행에서 같은 패키지 SHA256·관찰 Foot `X=-1773.873 Y=-7138.794 Z=-29048.523`·`NoPerception=1`과 실제 PoseControl 모드를 확인했다. 120 Pod 중 UnsafePods 41·고정 후보 664개, 선택 당시 양쪽 후보 42개다. 매 실행 지상 720·비행 600·귀속 1,320개, 네 spawn set 전량 배치와 30개 활동/컬 표본을 확인했다.

| 실행 순서 | frame P95(ms) | 게스트 연결 전체 B/s | 컬 안 지상 배회/구간 평균 | 컬 안 비행 배회/구간 평균 | Pose/Wander Dirty |
|:---|---:|---:|---:|---:|:---|
| 구간 1 | 12.65 | 17,475.0 | 43.37 | 41.83 | 2,798 / 569 |
| PositionYaw 1 | 12.73 | 18,980.9 | 41.27 | 40.87 | 6,151 / 0 |
| PositionYaw 2 | 14.08 | 18,572.4 | 40.80 | 40.70 | 6,159 / 0 |
| 구간 2 | 14.03 | 17,339.4 | 44.37 | 42.13 | 2,511 / 559 |

- **양쪽 종류의 컬 안 배회/구간 평균 각각 12개 이상·표본 28개 이상 기준 PASS**다. 최소 표본 개체 수는 양쪽 모두 36개다. 기존 묶음 5의 표본 부족을 이번 관찰 조건으로 해소했다.
- frame/path 네 실행 모두 PASS·host/guest 8개 exit 0·probe **32/32 PASS**·assert/ensure/fatal 및 Unknown/Envelope/LayerJump·Mass 순서 위반 0이다. Nav 요청/timing은 실행 순서대로 159/169/219/199행이며 행 수·키·순서 검사 통과다. 기존 Mover/CMC 로그는 남아 있어 게임 로그 전체 오류 0을 뜻하지 않는다.
- 연결 전체 평균 수신량은 구간 **17,407.2 B/s**, PositionYaw **18,776.65 B/s**로 구간 쪽 **7.29% 낮다**. 두 쌍 각각 구간 쪽 수신량이 낮으며 위치 Dirty도 줄었다. 실제 행동·배회 분포는 실행마다 다르므로 동일 개체의 동일 경로를 replay한 실험이나 NPC 전용 wire 절감률로 해석하지 않는다. 인지 제외·관찰 폰 정지의 합성 대조 조건에서 확인한 연결 전체 차이다.

실행 원본·콘솔·Nav CSV·`Summary.json`·`Validation.json`·`Comparison.json`은 `Saved/Profiling/Phase13/WanderComparisonEligible`다. 분석기 정상 종료, SamePackage/SameObserverFoot/BothDomainsSampledAll=true이며 NPCOnlyWireBytesVerified/MaximumPopulationGateVerified=false다.

**후속 묶음 6 완료, 다음 진행 확인 대기**다. 일반 production 전투 성능·지상/비행 총수 Gate와 D-054 재검토는 남아 있어 단위 5 전체·Phase 13은 미완료다. 다음 후보는 현재 편성의 인지·전투를 포함한 일반 production 프레임 재측정과 실패 시 비용 분해·수정이다. 이전 1,801개 production 실패를 격리된 1,320개 배회 대조로 해결했다고 보고하지 않는다. 에디터·게임·Insights 종료 상태를 확인했고 사용자 진행 확인 전 다음 구현에 착수하지 않는다.

빌드/자동화/smoke 증거는 `Saved/Tests/Phase13_WanderComposition` 및 `Saved/Tests/Phase13_ObserverRegression`, 최초 실패는 `Saved/Profiling/Phase13/WanderComparison`, 검증된 네 대조는 `Saved/Profiling/Phase13/WanderComparisonFixed`에 보존했다. `WanderComparisonIsolated`에는 다음 실행/분석 스크립트만 준비돼 있다. Current·Phase13·ValidationAndMigration에 같은 미검증 경계를 기록했다. 이전 production 프레임 Gate 실패·D-054·Phase13 전체 미완료 상태는 유지한다.
