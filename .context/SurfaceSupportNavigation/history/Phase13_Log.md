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
