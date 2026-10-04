# Surface Support·Navigation 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 Phase: Phase 12 새 세션 착수 대기 / Phase 13 최종 성능 Gate 보류
> 마지막 갱신: 2026-10-04

## 현재 결정과 다음 행동

사용자는 **Phase 13을 핵심 구현·기능 검증 완료, 최종 성능 Gate 보류** 상태로 두고 **새 세션에서 Phase 12 투사체 최적화를 먼저 진행**하기로 결정했다. Phase 12 완료 뒤 별도 새 세션에서 Phase 13을 마무리한다. 이번 세션에서는 문서 갱신·커밋까지만 수행하며 Phase 12 구현에 착수하지 않는다.

투사체 후보 제외·피격 판정·월드 query 최적화는 Phase 12에서 집중적으로 검토한다. 앞선 후보 제외 최적화 제안은 Phase 13의 다음 구현 지시로 쓰지 않는다. 특정 방법은 아직 채택하지 않았으며 내부 비용을 분해한 뒤 정확성과 실측 이득을 기준으로 선택한다.

Phase 13의 기존 완료 조건은 유지한다. 이동·Nav 격리 측정은 보조 진단이며 전체 프레임 Gate를 대체하지 않는다. 투사체 CPU를 프레임 P95에서 빼서 합격을 계산하지 않는다. 경로 800 실패와 비행 관통·혼합/활동 대역 총수 검증도 남아 있으므로 Phase 12 완료만으로 Phase 13을 자동 완료하지 않는다. 재검증 전 D-054의 비행 200과 총수 상한은 유지한다.

## Phase 12 새 세션 진입점

1. 필수 `README.md`·이 문서·`Decisions.md`와 `Roadmap.md`의 Phase 12 범위·순서 결정을 읽는다. 투사체 관련 설계는 `design/RuntimeCollision.md`, `.context/TechDesign_HitDetection.md`를 따른다. 착수 시 Phase 12 실행서를 구체화한다.
2. `history/Phase13_Log.md`의 후속 묶음 9와 `Saved/Profiling/Phase13/GroundPopulationAttribution/Summary.json`을 최신 비용 기준선으로 읽는다. `LNPProjectileProcessors.cpp` 서버/클라이언트 판정의 대상 수집·팀/발사자 제외·되감기·캡슐 판정·월드 query 내부 비용부터 분해한다. 이전/현재 패키지 대조는 `Saved/Profiling/Phase13/GroundAttribution/Summary.json`에 따로 보존돼 있다.
3. 정확성·명중/피격 순서·월드 충돌과 되감기를 유지하고 측정 가능한 이득이 있는 변경만 채택한다. 구현 단위마다 검증을 마친 뒤 사용자에게 계속 진행할지 확인한다.

방법은 에디터 실행 상태로 우선순위를 정하지 않는다. MCP가 더 효율적이면 즉시 연결 요청하고 전체 빌드가 필요하면 즉시 에디터 저장·종료를 요청한다. 현재 에디터·게임·Insights는 종료 상태다.

## Phase 13 보존한 완료 결과

- 단위 1~4: 비용 계측, Pod 단위 휴면/배회/활성·그룹 깨움, 서버 구간 배회, 구간 복제·클라이언트 시각 소비·위치 Dirty 억제·되감기 구현을 완료했다.
- 핵심 기능의 자동화·빌드/cook·패키지 1P·리슨 2P, 실제 late join·같은 게스트 NetID 48개의 bubble 제거/새 ReplicationID 재추가, 양쪽 지상/비행 배회·접지 화면 검증을 확보했다. 단일 테스트 실행이나 모든 고부하 기능 검증으로 확대하지 않는다.
- 배회 대조 1,320개·컬 안 지상/비행 배회 각각 평균 40개 이상에서 구간 복제의 연결 전체 평균 수신량이 PositionYaw보다 7.29% 낮았다. NPC 전용 wire 절감률이나 일반 전투 성능으로 해석하지 않는다.
- 기본 반경 800cm 일반 production은 실제 지상 709·비행 600개에서 frame P95 12.58/12.80ms·frame/path PASS였다. 요청 지상 720 중 11개 부족이 있었다. 반경 1,500cm 대조는 지상 720·비행 600 전량 배치지만 frame 17.27ms FAIL이라 기본 에셋 반경은 유지했다.

## 최신 성능 기준선과 미해결 항목

후속 묶음 9에서 같은 ObserverEligible 패키지로 자연·추격 각각 700→800→800→700 trace 없는 교차 대조와 네 조건 trace를 실행했다.

| 조건 | frame P95(ms), 두 반복 | 경로 Gate |
|:---|:---|:---|
| 자연 700 | 18.60 / 18.70 | 두 번 PASS |
| 자연 800 | 20.71 / 20.50 | 두 번 PASS |
| 추격 700 | 18.18 / 17.81 | 두 번 PASS |
| 추격 800 | 21.69 / 20.62 | 첫 PASS / 둘째 FAIL(P95 1.765ms) |

여덟 번 모두 frame FAIL이다. 이전 700 합격 기록은 보존하지만 현재 동일 패키지 700도 실패해 과거 합격을 현재 기준선으로 쓰지 않는다. trace 호출당 투사체 판정 inclusive/exclusive는 자연 700→800 6.172/5.207→7.078/6.091ms, 추격 6.032/5.063→6.760/5.791ms다. 가장 큰 프로젝트 processor 범위와 증가분이며 내부 한 연산을 원인으로 확정한 것은 아니다. 경로 소비·이동 exact·인지/분리·Actor 애니 비용도 남는다. 활동+배회 관리는 약 0.09~0.10ms다. exact 하위 범위와 processor를 중복 합산하지 않으며 병렬 부모에는 대기가 포함된다.

12회 host/guest 24개 exit 0·probe 96/96 PASS·네 Insights export 정상 종료·30표본·전량 무귀속 Active·동일 패키지·Nav CSV 행/키/순서를 검증했다. assert/ensure/fatal·불변 위반 0이다. 기존 Mover/CMC와 headless 입력 fault는 남아 있다. 원본·재현 실행기·구조화 요약은 `Saved/Profiling/Phase13/GroundPopulationAttribution`, 분석기는 `Scripts/Profiling/AnalyzePhase13GroundPopulation.ps1`이다.

Phase 13 재개 시 다음을 확인한다.

- Phase 12 최종 패키지의 trace 없는 지상 700/800 자연·추격 반복과 실패 시 경로 scheduler/consumer 비용 분해·필요한 수정
- Pod 귀속 일반/항상 활성·반경 확장 전량 배치 대조의 통합 성능과 활동 대역 총수 검증. 이전 1,801개 실패와 현재 기본 반경 1,309개 합격은 서로 다른 편성이다.
- 비행 300의 관통·혼합 부하·총수 검증. 앞선 frame P95 8.85/8.78ms는 PASS지만 exact 진단 2.035/2.365ms로 예산 초과이고 관통 검사는 꺼져 있었다.
- 최종 변경본의 관련 기능·네트워크 회귀·cook/stale 확인과 실제 필요에 맞는 사용자 플레이 검증, 총수/D-054 결정·단위 5 및 Phase 13 완료 인계

## 원본과 검증 주의

현재 계획은 `phases/Phase13_ActivityBandsAndWanderReplication.md`, 단계 순서는 `Roadmap.md`, 상세 구현·측정·과거 실패와 해석 한계는 `history/Phase13_Log.md`가 소유한다. Phase 8·7c 완료 근거는 각각의 history를 따른다.

- cook에는 필요한 회귀 게임 맵과 fixture LVI·fixture 폴더를 명시적으로 포함한다. soft path만으로 cook을 보장하지 않는다.
- 전체 자동화가 재저장하는 테스트 에셋은 실행 전 백업·실행 후 복구와 SHA256 검사를 한다.
- 기존 Mover 초기화/out-of-band 경고와 Mass CharacterMovementComponent 추출 오류를 assert/ensure/crash/identity 오류와 구분한다.
