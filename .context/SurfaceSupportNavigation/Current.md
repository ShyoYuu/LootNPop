# Surface Support·Navigation 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 Phase: Phase 13 완료 / Phase 14 착수 진행 확인 대기
> 마지막 갱신: 2026-10-05

## 현재 결정

Phase 13 단위 1~5를 완료했다. Pod 활동 대역·서버 구간 배회·복제/되감기·초기 비행 스폰 관통 수정과 성능 검증을 완료했고, 사용자가 최신 수정본의 호스트·게스트 배회·관통·접근·피격·게스트 이탈/재접근·패링에서 이상 없음을 확인했다. D-074로 D-054를 대체해 **Pod 귀속 PureEntity 비행 600개를 임시 월드 편성 목표로 채택**한다. 최대 총수나 지상 800+비행 600 전량 활성의 합격 보장은 아니다. 최종 지상/비행/벽보행 총수·비율·활성 예산은 Phase 14 뒤 통합 측정으로 정한다.

현재 `DA_MassSpawnConfig`는 120 Pod 중 100 Pod에 비행 6기씩, 지상 PureEntity 요청 720개·Actor 승격형 요청 0개, 기본 반경 1,000cm·density 1이다. 이번 마무리에서 런타임/에셋을 추가 변경하지 않았다. 모든 구현 단위 완료·검증 뒤 사용자에게 계속 진행할지 확인한다.

## 바로 다음 행동

1. 사용자에게 Phase 14 착수를 진행할지 확인한다. 승인 뒤 `Roadmap.md`·`design/MovementIntegration.md`의 벽 타기 NPC(D-066)를 기준으로 실행 문서를 구체화하고, 첫 구현 단위는 적의 Up을 엔티티 회전 Z축에서 읽도록 전환하는 범위를 조사·분할한다. Phase 14 구현은 아직 시작하지 않았다.
2. 벽보행 자체와 기존 지상/비행 회귀를 검증한 뒤 D-074의 최종 혼합 편성을 측정한다. 비행 900~1,200은 확대 후보다. 월드 총수·대역별 수·동시 교전·투사체·viewer/복제 부하와 렌더링을 함께 기록한다. MCP가 효율적이면 즉시 연결 요청, 전체 빌드가 필요하면 즉시 에디터 저장·종료 요청, 직접 PIE가 빠르면 사용자 조작을 먼저 요청한다.

## 완료 근거와 원본

- 최신 패키지: `Saved/SurfaceNavigationPhase13PenetrationFixedPackage`, exe SHA256 `D9A5D1C8CBD4938166484FC0C6C9B47E245656FBC010A477A1E198E437291723`. 전체 빌드/cook/archive·관련 자동화 25/25 PASS, 최신 소스/에셋/cook 입력 16개와 exe 해시 일치를 마무리 때 확인했다.
- 최신 Pod 귀속 성능: 기본 실제 지상 709/비행 600 P95 6.61ms, 반경 1,500cm 전량 720/600 두 반복 6.63/6.93ms·frame/path PASS. exact 진단 1.743~1.756ms. 별도 관통 On 총 11,788 검사 틱 관통 0·비행 600 유지·거부 0. `Saved/Profiling/Phase13/PenetrationFix`.
- 증량: 비행 900 단독 5.85ms·1,200 두 반복 9.01/9.01ms. 지상 800+비행 900은 17.33ms FAIL/14.07ms PASS이며 후자는 bubble 제거·입력 fault·수신량 변화가 있어 안정적 합격으로 쓰지 않는다. 비행 600 링 요청은 실제 599라 전량 표본에서 제외한다. 별도 비행 1,200/혼합 800+900 관통 진단 4,746틱 관통 0. `Saved/Profiling/Phase13/FlightPopulationReview`.
- 기존 기능: 실제 late join·같은 게스트 NetID 48개(비행 6개 포함)의 bubble 제거/새 ReplicationID 재추가, 양쪽 배회·접지·재등장 화면 확인. `Saved/Tests/Phase13_Unit5_Network`. 배회 대조의 연결 전체 평균 수신량은 PositionYaw보다 7.29% 낮았으며 NPC 전용 wire 절감률이 아니다. `Saved/Profiling/Phase13/WanderComparisonEligible`.
- 최신 실제 플레이: 사용자의 양쪽 배회·관통·접근·피격·게스트 이탈/재접근·패링 이상 없음. 로그 assert/ensure/fatal/투사체 validation 불일치/비행 스폰 거부 0. `Saved/Profiling/Phase13/Closure`의 GameplaySession/GameplayAudit/패키지 식별자·해시. 이번 GUI 플레이의 프레임 성능이나 NetID 재진입 수를 새로 측정한 것은 아니다.
- Phase 12의 지상 700/800 자연·추격 교차 frame/path PASS와 사용자 기본 플레이 결과를 인계받는다. sphere/grid=1, RewindCache=0을 유지한다. `history/Phase12_Log.md`, `Saved/Profiling/Phase12/Final`.

## 보존한 한계와 현재 에디터 상태

- 기본 반경 seed 1 지상 11개 부족, 최신 PIE 전체 계획 배치 1,317/1,320개를 기록했다. PIE 종류별 실수는 계측하지 않았으며 비행 600 전량 근거는 별도 패키지 활동 표본이다. 후보 계획 수와 실제 스폰 수를 혼동하지 않는다.
- 전량 활성/증량의 exact 2ms 진단 초과와 혼합 frame 실패는 남는다. 과거 1,801개 실패·최대 총수 해결로 확대하지 않는다. nullrhi 성능과 실제 화면 기능 검증을 구분한다.
- 기존 Mover/CMC 경고·cook 재질 fallback 경고는 보존하며 기능 실패로 숨기거나 이번 변경으로 해결됐다고 보고하지 않는다. 기본 반경을 800cm로 적었던 기록은 에셋과 원본 로그에 맞춰 1,000cm로 정정했다.
- 마지막 MCP 확인: 에디터 연결 정상·PIE 종료. ProfileStride/두 projectile validation/RewindCache/FlightPenetrationCheck/WorldCollision.LockProbe=0, sphere/grid/support cache/exact lateral sweep/parallel movement/density=1. 추가 설정 복구 항목 없음.

상세 구현·측정·한계는 `history/Phase13_Log.md`, 완료 기준은 `phases/Phase13_ActivityBandsAndWanderReplication.md`, 단계 순서는 `Roadmap.md`, 규약은 `design/MovementIntegration.md`와 `Decisions.md`가 소유한다. 로컬 측정 산출물은 Git에 넣지 않는다.
