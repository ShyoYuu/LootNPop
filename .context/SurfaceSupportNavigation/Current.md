# Surface Support·Navigation 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 Phase: Phase 12 완료 / Phase 13 최종 성능 Gate 재개 진행 확인 대기
> 마지막 갱신: 2026-10-05

## 현재 결정

Phase 12 단위 4와 전체 단계를 완료했다. 채택 기본 경로의 최종 패키지 지상 700/800 자연·추격 교차 반복은 frame/path를 통과했고, 사용자도 실제 리슨 2P 호스트·게스트 양쪽의 조준·피격·월드 차폐·패링 넉백/반사에서 이상을 발견하지 못했다고 확인했다. 실제 원격 사격 로그 분석·PIE 종료·진단 CVar 복구도 확인했다. **Phase 13은 핵심 구현·기능 검증 완료 및 최종 성능 Gate 보류 상태를 유지하며, 사용자 진행 확인 전에는 재개하지 않는다.** 구현 단위마다 검증 후 사용자에게 계속 진행할지 확인한다.

단위 1~3b의 정확한 캡슐 판정·sphere 선행 검사·서버 현재 위치 공간 필터는 채택 상태다. 원격 발사/되감기는 전수 후보 경로를 유지한다. 같은 팀 차단·기존 후보 순서·월드 차폐·viewer별 보간/배회 규칙을 보존한다. 구현과 측정 이력은 `history/Phase12_Log.md`, 실행 계획은 `phases/Phase12_ProjectileOptimization.md`가 소유한다.

단위 3c는 적별로 같은 processor 실행 안에서 정확히 같은 QueryTime의 히스토리 위치를 재사용하는 실험이다. 구현·검증은 완료했지만 **RewindCache 기본값 0을 유지한다**. 실제 사격 Off/On은 수동 발사량·NPC 편성/상태가 달라 순수 캐시 CPU 이득을 확정하지 못했다. 기본 활성화에는 통제된 되감기 부하 대조가 필요하다.

## 최신 검증과 재현 위치

- 단위 3c에서 빌드/cook 검증한 최종 패키지 `Saved/SurfaceNavigationPhase12RewindPackage`의 exe SHA256 일치를 확인했다. 이번 단위는 런타임 코드/에셋 변경이 없어 같은 패키지를 사용한다. 관련 자동화 12개 PASS, 결과는 `Saved/Tests/Phase12_Unit4`다.
- 기본 경로 리슨 2P 14회 host/guest exit 0·probe 112/112 PASS. trace 없는 자연·추격 각각 700→800→800→700 여덟 실행은 실제 전량 배치·frame/path 모두 PASS다. 자연 700 P95 15.10/13.63ms, 자연 800 15.51/13.89ms, 추격 700 12.95/13.24ms, 추격 800 13.66/14.51ms다.
- 월드 exact query 약 0.76~0.78ms/프레임, 상세 계측 wrapper 약 0.88~0.89ms/프레임을 관측했다. 추가 horizon/캐시는 도입하지 않는다. 기존 harness의 LockProbe=1 비용이 포함되며 중첩 범위는 합산하지 않는다. 실제 원격 사격이 없는 합성 부하라 원격 gameplay/되감기 이득으로 확대하지 않는다.
- 원본·실행기·Summary/ProjectileSummary·SHA256 목록은 `Saved/Profiling/Phase12/Final`, 분석기는 `Scripts/Profiling/AnalyzePhase12Final.ps1`다. 단위 3c의 실제 캐시 정확성·통제되지 않은 Off/On 관측과 기본 활성화 보류 근거는 `history/Phase12_Log.md`에 보존한다.
- 실제 통합 플레이 로그/분석은 `Saved/Profiling/Phase12/Final/Gameplay`다. 원격 투사체 프레임 표본 398개·히스토리 조회 499,238회, 투사체 패링 성공 49회·근접 패링 6회, 분석 불변식 통과·RuntimeErrors 0을 확인했다. 기능 진단 결과이며 고유 발사 수나 성능 Gate로 해석하지 않는다. 화면 동작 판정은 사용자의 양쪽 기본 플레이 확인 범위다.
- 완료 뒤 사용자 요청으로 근접 스윙의 RTT/2·원격 여부를 NotifyBegin에서 캐싱하도록 후속 수정했다. 근접 워커의 PlayerState/IsLocallyControlled 조회를 제거했다. 이 수정은 에디터 전체 빌드·관련 자동화 12개를 통과했으며, 위 최종 패키지/플레이 수치는 수정 전 코드 기준이다. Phase 13 production 측정 전에는 후속 수정을 포함한 새 패키지를 빌드/cook하고 식별자를 기록한다.

로컬 측정 산출물은 Git에 넣지 않는다. 빌드 로그·패키지/로그/trace SHA256·한계는 `history/Phase12_Log.md`를 따른다.

## 바로 다음 행동

1. 사용자에게 다음 Phase 13 최종 성능 Gate를 진행할지 확인한다. 기존 인계 순서는 별도 새 세션 재개다. 재개 승인을 받으면 Phase 13 실행서와 필요한 설계를 읽고 최신 기본 경로 패키지·자동화·700/800 PASS 근거를 인계받는다. 새 변경/실패가 없으면 완료한 지상 교차 반복을 다시 실행하지 않는다.
2. 첫 후속 단위는 최신 패키지의 Pod 귀속 일반/항상 활성 production·반경 확장 전량 배치 대조로 실제 편성/활동 대역 분포·frame/path·총수 한계를 확인한다. 비행 관통·혼합 부하·총수와 D-054 검토는 이후 단위로 나누고 각 단위 완료 뒤 진행 확인을 받는다. 구체적인 미해결 항목은 아래와 Phase 13 실행서를 따른다.

마지막 MCP 확인: 에디터 연결 정상·PIE 정지, ProfileStride=0 복구 완료·RewindCache/두 validation=0·sphere/grid=1이다. 실험용 설정 복구 미확인 항목은 없다.

방법은 에디터 실행 상태로 우선순위를 정하지 않는다. MCP가 더 효율적이면 즉시 연결을 요청하고 전체 빌드가 필요하면 즉시 에디터 저장·종료를 요청한다. 사용자 직접 PIE/GUI 검증이 빠르고 정확하면 수동 조작을 먼저 요청한다.

Phase 13의 전체 프레임·경로·총수 Gate는 별도로 남는다. 투사체 CPU를 프레임 P95에서 빼서 합격을 계산하지 않는다. Phase 12 완료만으로 Phase 13을 자동 완료하지 않으며 재검증 전 D-054의 비행 200과 총수 상한을 유지한다.

## Phase 13 보존한 완료 결과

- 단위 1~4: 비용 계측, Pod 단위 휴면/배회/활성·그룹 깨움, 서버 구간 배회, 구간 복제·클라이언트 시각 소비·위치 Dirty 억제·되감기 구현을 완료했다.
- 핵심 기능의 자동화·빌드/cook·패키지 1P·리슨 2P, 실제 late join·같은 게스트 NetID 48개의 bubble 제거/새 ReplicationID 재추가, 양쪽 지상/비행 배회·접지 화면 검증을 확보했다. 단일 테스트 실행이나 모든 고부하 기능 검증으로 확대하지 않는다.
- 배회 대조 1,320개·컬 안 지상/비행 배회 각각 평균 40개 이상에서 구간 복제의 연결 전체 평균 수신량이 PositionYaw보다 7.29% 낮았다. NPC 전용 wire 절감률이나 일반 전투 성능으로 해석하지 않는다.
- 기본 반경 800cm 일반 production은 실제 지상 709·비행 600개에서 frame P95 12.58/12.80ms·frame/path PASS였다. 요청 지상 720 중 11개 부족이 있었다. 반경 1,500cm 대조는 지상 720·비행 600 전량 배치지만 frame 17.27ms FAIL이라 기본 에셋 반경은 유지했다.

## Phase 12 착수 전 실패 기준선과 Phase 13 미해결 항목

후속 묶음 9에서 같은 ObserverEligible 패키지로 자연·추격 각각 700→800→800→700 trace 없는 교차 대조와 네 조건 trace를 실행했다. 아래는 Phase 12 착수 전 기록이며, 최신 기본 경로 결과는 위 단위 4 검증을 따른다.

| 조건 | frame P95(ms), 두 반복 | 경로 Gate |
|:---|:---|:---|
| 자연 700 | 18.60 / 18.70 | 두 번 PASS |
| 자연 800 | 20.71 / 20.50 | 두 번 PASS |
| 추격 700 | 18.18 / 17.81 | 두 번 PASS |
| 추격 800 | 21.69 / 20.62 | 첫 PASS / 둘째 FAIL(P95 1.765ms) |

여덟 번 모두 frame FAIL이다. 이전 700 합격 기록은 보존하지만 당시 동일 패키지 700도 실패해 과거 합격을 당시 기준선으로 쓰지 않는다. trace 호출당 투사체 판정 inclusive/exclusive는 자연 700→800 6.172/5.207→7.078/6.091ms, 추격 6.032/5.063→6.760/5.791ms다. 가장 큰 프로젝트 processor 범위와 증가분이며 내부 한 연산을 원인으로 확정한 것은 아니다. 경로 소비·이동 exact·인지/분리·Actor 애니 비용도 남는다. 활동+배회 관리는 약 0.09~0.10ms다. exact 하위 범위와 processor를 중복 합산하지 않으며 병렬 부모에는 대기가 포함된다.

12회 host/guest 24개 exit 0·probe 96/96 PASS·네 Insights export 정상 종료·30표본·전량 무귀속 Active·동일 패키지·Nav CSV 행/키/순서를 검증했다. assert/ensure/fatal·불변 위반 0이다. 기존 Mover/CMC와 headless 입력 fault는 남아 있다. 원본·재현 실행기·구조화 요약은 `Saved/Profiling/Phase13/GroundPopulationAttribution`, 분석기는 `Scripts/Profiling/AnalyzePhase13GroundPopulation.ps1`이다.

Phase 13 재개 시 다음을 확인한다.

- Phase 12 최종 기본 경로의 700/800 자연·추격 반복 PASS 근거를 인계받고, 후속 변경이나 통합 부하 실패가 있을 때만 경로 scheduler/consumer 비용을 다시 분해·수정
- Pod 귀속 일반/항상 활성·반경 확장 전량 배치 대조의 통합 성능과 활동 대역 총수 검증. 이전 1,801개 실패와 현재 기본 반경 1,309개 합격은 서로 다른 편성이다.
- 비행 300의 관통·혼합 부하·총수 검증. 앞선 frame P95 8.85/8.78ms는 PASS지만 exact 진단 2.035/2.365ms로 예산 초과이고 관통 검사는 꺼져 있었다.
- 최종 변경본의 관련 기능·네트워크 회귀·cook/stale 확인과 실제 필요에 맞는 사용자 플레이 검증, 총수/D-054 결정·단위 5 및 Phase 13 완료 인계

## 원본과 검증 주의

현재 계획은 `phases/Phase13_ActivityBandsAndWanderReplication.md`, 단계 순서는 `Roadmap.md`, 상세 구현·측정·과거 실패와 해석 한계는 `history/Phase13_Log.md`가 소유한다. Phase 8·7c 완료 근거는 각각의 history를 따른다.

- cook에는 필요한 회귀 게임 맵과 fixture LVI·fixture 폴더를 명시적으로 포함한다. soft path만으로 cook을 보장하지 않는다.
- 전체 자동화가 재저장하는 테스트 에셋은 실행 전 백업·실행 후 복구와 SHA256 검사를 한다.
- 기존 Mover 초기화/out-of-band 경고와 Mass CharacterMovementComponent 추출 오류를 assert/ensure/crash/identity 오류와 구분한다.
