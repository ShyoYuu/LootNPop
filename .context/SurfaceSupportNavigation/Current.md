# Surface Support·Navigation 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 Phase: Phase 14 진행 중 — 단위 1 완료, 단위 2 착수 확인 대기
> 마지막 갱신: 2026-10-05

## 현재 결정

Phase 13은 완료됐다(D-074: Pod 귀속 PureEntity 비행 600 임시 월드 편성). 사용자 지시로 Phase 14 벽 타기 NPC(D-066)에 착수했다. 실행 문서는 `phases/Phase14_SurfaceCrawlNpc.md`다. 벽보행 적은 벽면 전용 스폰 후보 없이 Pod 주변 지면에서 스폰돼 스스로 벽으로 간다. 간단한 구현을 먼저 하고 나머지는 플레이에서 필요가 생길 때 계획한다(사용자 결정).

단위 1의 코드 전환, 전체 빌드, 관련 자동화 30/30 PASS, 사용자 리슨 2P 화면 확인을 마쳤다. 편차 측정에서 경사에 스폰된 휴면·정지 적의 회전이 최대 29° 기울어 있는 것을 찾았다. 원인은 스폰 계획의 지면 법선 회전이다. `SetupSpawnedEntities`에서 적 회전 Z를 구 중심 방향으로 맞췄고, 수정 뒤 편차는 0.3° 미만이다. 전체 빌드 후 관련 자동화 30/30을 다시 통과해 단위 1을 완료했다. 에셋 변경은 없다.

단위 1의 규칙은 다음과 같다. 적 자신의 몸 방향이 필요한 곳(공격 기저·판정 캡슐·적 탐색 질의·락온·마커/HP 바·패링 반격 넉백·디버그 표시)만 엔티티 회전 Z(`LNPMass::GetEntityUp`)로 바꾼다. 지상·비행 이동 내부의 중력·투영, 플레이어 쪽, 복제 회전 복원은 유지한다. 복제 회전 복원은 단위 3에서 법선 필드와 함께 바꾼다.

현재 `DA_MassSpawnConfig`는 120 Pod 중 100 Pod에 비행 6기씩, 지상 PureEntity 요청 720개·Actor 승격형 요청 0개, 기본 반경 1,000cm·density 1이다.

## 바로 다음 행동

1. 사용자 확인 뒤 단위 2(SurfaceCrawl 도메인과 서버 wall-walker 이동)에 착수한다. 스폰은 Pod 주변 지면 후보를 그대로 쓴다(사용자 결정). 먼저 적 Config·Trait의 도메인 분기 지점(`NavigationDomain`, 비행 태그 부착·지상 프로세서 제외 방식)과 `StepAirborne` 재사용 경계를 조사해 단위 2를 세분한다.
2. 벽보행 개체의 외형은 단위 5 전까지 기존 적 메시를 임시로 쓴다. 화면에서 법선 정렬을 확인하려면 행동 디버그 마커(`LNP.Debug.DrawEnemyAction`)가 이미 회전 Z를 따른다.

## 인계받은 근거와 원본

- Phase 13 최신 패키지: `Saved/SurfaceNavigationPhase13PenetrationFixedPackage`, exe SHA256 `D9A5D1C8CBD4938166484FC0C6C9B47E245656FBC010A477A1E198E437291723`. 관련 자동화 25/25 PASS.
- Pod 귀속 성능: 기본 실제 지상 709/비행 600 P95 6.61ms, 반경 1,500cm 전량 720/600 두 반복 6.63/6.93ms·frame/path PASS. `Saved/Profiling/Phase13/PenetrationFix`.
- 증량 검토: 비행 900 단독 5.85ms·1,200 9.01/9.01ms. 지상 800+비행 900은 안정적 합격이 아니다. `Saved/Profiling/Phase13/FlightPopulationReview`.
- 보존한 한계: 기본 반경 seed 1 지상 11개 부족, 전량 활성/증량의 exact 2ms 진단 초과와 혼합 frame 실패, 기존 Mover/CMC·cook 재질 fallback 경고. 최대 총수와 최종 지상/비행/벽보행 편성은 Phase 14 단위 6에서 정한다.
- 마지막 MCP 확인(Phase 14 단위 1): PIE 종료, `LNP.Enemy.UpDeviationCheck 0`. Phase 13 마무리 때의 진단 CVar 기본값(ProfileStride/projectile validation/RewindCache/FlightPenetrationCheck/WorldCollision.LockProbe=0, sphere/grid/support cache/exact lateral sweep/parallel movement/density=1)은 이번에 바꾸지 않았다.

상세는 `history/Phase14_Log.md`(이번 Phase), `history/Phase13_Log.md`(인계 근거), 단계 순서는 `Roadmap.md`, 규약은 `design/MovementIntegration.md`와 `Decisions.md`가 소유한다. 로컬 측정 산출물은 Git에 넣지 않는다.
