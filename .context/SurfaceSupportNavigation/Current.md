# Surface Support·Navigation 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 Phase: Phase 14 완료(2026-10-06) — 다음 Phase는 사용자 결정 대기
> 마지막 갱신: 2026-10-06

## 현재 결정

Phase 14(벽 타기 NPC)를 완료했다. 실행 문서는 `phases/Phase14_SurfaceCrawlNpc.md`, 경위와 측정은 `history/Phase14_Log.md`가 소유한다.

- 월드 편성(D-076): 지상·비행·벽보행 1:1:1, 종류별 1,080(120 Pod × Pod당 9, 지상은 근접 6·원거리 3), 스폰 반경 1,500cm. `DA_MassSpawnConfig`에 저장돼 있다. 동시 활성 상한은 약 2,000(전원 활성 경계)이다.
- 벽보행 비용(D-075): 전량 활성 100기당 서버 frame P95 약 +0.45ms, exact 약 1회/개체·프레임·호출당 5~6us.
- 벽보행 패널 운반·압사(D-077). 패널에 깔린 개체가 패널 밑면에 붙어 올라가는 동작은 의도된 결과다.
- 알려진 한계(사용자 수용): 0.5cm 이하·수 틱 안 해소되는 일시 겹침, 단순 우회만 함(길찾기 없음), 판정은 윗면 위 약 40cm 남음, 시체 넉백은 원거리만, 회전 패널은 자동화로만 확인.

## 바로 다음 행동

1. 사용자와 다음 Phase를 정한다(`Roadmap.md`). 후보 백로그: 모든 NPC 공통 압사, 리슨 호스트 적 표시 거리 제한(`../Idea_Backlog.md`).
2. 부하 측정은 `Scripts/Profiling/RunPhase13Gate.ps1`(벽보행 조건 `C100/C300/C500/N500C100`, `-EnemyDensity`, `-FlightPenetrationCheck`)을 쓴다. Production 관통은 실행마다 편차가 커서 4회 이상 돈다.
3. PIE: TestMap03 리슨 2P는 `LevelEditorPlaySettings.PlayNumberOfClients`를 2로 올리고 끝나면 1로 되돌린다.

## 인계받은 근거와 원본

- Phase 14 최종 패키지: `Saved/SurfaceNavigationPhase14Final7Package`, exe SHA256 `588B07DD8F9923C44289216625DBAB85A26D25F1AD3C3CF7387F666839C8F57A`. 원본 `Saved/Profiling/Phase14/Final7`(성능 2회 frame P95 10.71/10.67ms, 관통 On 4회 벽보행 1·2·0·0틱·비행 0).
- 벽보행 매트릭스 `Saved/Profiling/Phase14/CrawlMatrixFix2`, 1:1:1 상한 측정 `Saved/Profiling/Phase14/Mix`.
- 진단 CVar 기본값(ProfileStride/projectile validation/RewindCache/FlightPenetrationCheck/WorldCollision.LockProbe=0, sphere/grid/support cache/exact lateral sweep/parallel movement/density=1)은 바꾸지 않았다.

상세는 `history/Phase14_Log.md`, 단계 순서는 `Roadmap.md`, 규약은 `design/MovementIntegration.md`와 `Decisions.md`가 소유한다. 로컬 측정 산출물은 Git에 넣지 않는다.
