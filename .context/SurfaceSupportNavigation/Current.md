# Surface Support·Navigation 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 Phase: Phase 14 진행 중 — 단위 1~4 완료, 다음은 단위 5(외형)
> 마지막 갱신: 2026-10-06

## 현재 결정

Phase 13은 완료됐다(D-074: Pod 귀속 PureEntity 비행 600 임시 월드 편성). 사용자 지시로 Phase 14 벽 타기 NPC(D-066)에 착수했다. 실행 문서는 `phases/Phase14_SurfaceCrawlNpc.md`다. 벽보행 적은 벽면 전용 스폰 후보 없이 Pod 주변 지면에서 스폰돼 스스로 벽으로 간다. 간단한 구현을 먼저 하고 나머지는 플레이에서 필요가 생길 때 계획한다(사용자 결정).

- 단위 1: 적 Up을 엔티티 회전 Z에서 읽는다.
- 단위 2: 구 몸체 wall-walker(`LNPSurfaceCrawl`, `ULNPEnemyCrawlMovementProcessor`)와 단순 우회(2e). 외형 컨셉은 로봇청소기형이다.
- 단위 3: 표면 법선 복제(`FLNPReplicatedAgent::SurfaceNormal`).
- 단위 4: 360° 인지·조준 ±89·사거리 800(4a), LoS 막힘 시 계속 접근(4b), 벽보행 전용 3D 분리 격자(4c), 재귀속(4d — 10초 정체 시 직선거리 최근접 활성 Pod, 착지 Home 판정, 실패 Pod 8개 기억), 휴면·같은 면 구간 배회(4e — 최대 180cm·정지 9초), 배회 복제(4f — Flags bit 5, 법선 고정 Up). 자동화 39/39와 섬 검증 옥탄트 1P·리슨 2P PIE로 확인했다.
- 적 NPC 전반의 시야 상실 후 추적 제한은 `../Idea_Backlog.md` 후보다(사용자 결정).

현재 `DA_MassSpawnConfig`는 4개 세트 모두 벽보행 3기씩(벽보행 360기, 나머지 0)이다. 이전 편성은 `history/Phase14_Log.md` 2026-10-06 항목에 있다. 벽보행 Config는 `DA_Enemy_PureEntity_Crawler01`(반지름 80, 사거리 800, 분리 반경 220)이다.

## 바로 다음 행동

1. 사용자 진행 확인 뒤 단위 5(외형)를 착수한다: 로봇청소기형 메시와 ISKM 걷기 시퀀스 방식 결정·적용, HP 바·락온 마커·피격 연출이 벽·천장에서 맞는지 화면 확인(`../TechDesign_EnemyNPC_LowLOD.md`, 메모리 "ISKM 애니메이션 함정"·"블렌더 MCP ↔ UE 왕복"). 착수 때 하위 단위로 나눠 실행 문서에 적는다.
2. 벽보행 PIE: TestMap03의 `OctantPoolData` 정의를 MCP `ObjectTools.set_properties`로 메모리에서만 검증 옥탄트로 바꾸고 `StartPIE`한 뒤 즉시 Meadow 정의로 되돌린다. 에디터 종료 때 `OctantPoolData`는 저장하지 않는다.
   - 벽·천장 지형: `LVI_Octant_Fixture_Crawl`·`DA_OctantSurface_Fixture_Crawl`, 시작 위치 17176.17×3.
   - 부유섬·재귀속·배회: `LVI_Octant_Fixture_Island`·`DA_OctantSurface_Fixture_Island`, 시작 위치 14722.4×3(섬 위). 섬 -X 가장자리 경사로로 돌아온다.
   - 회전은 둘 다 pitch 0·yaw 135·roll 125.2644. 리슨 2P는 `LevelEditorPlaySettings.PlayNumberOfClients`를 2로 올리고 끝나면 1로 되돌린다(평소 1 — 게스트가 어그로를 가져간다). 콘솔 명령은 MCP SlateInspector로 Output Log 입력창에 입력한다.
3. 알려진 한계: 길찾기 없이 단순 우회만 한다(2e, 사용자 수용). 개체가 많으면 뒤쪽 개체의 분리력이 앞쪽을 사거리보다 가까이 밀어낸다(사용자 수용). 재귀속 최종 Pod가 지각 Pod인지는 로그로 확정하지 못했다. 부하 harness 활동 통계는 벽보행을 지상으로 센다(단위 6). TestMap03에서 본 옆으로 누운 개체는 원인 미확정이다.

## 인계받은 근거와 원본

- Phase 13 최신 패키지: `Saved/SurfaceNavigationPhase13PenetrationFixedPackage`, exe SHA256 `D9A5D1C8CBD4938166484FC0C6C9B47E245656FBC010A477A1E198E437291723`. 관련 자동화 25/25 PASS.
- Pod 귀속 성능: 기본 실제 지상 709/비행 600 P95 6.61ms, 반경 1,500cm 전량 720/600 두 반복 6.63/6.93ms·frame/path PASS. `Saved/Profiling/Phase13/PenetrationFix`.
- 증량 검토: 비행 900 단독 5.85ms·1,200 9.01/9.01ms. 지상 800+비행 900은 안정적 합격이 아니다. `Saved/Profiling/Phase13/FlightPopulationReview`.
- 보존한 한계: 기본 반경 seed 1 지상 11개 부족, 전량 활성/증량의 exact 2ms 진단 초과와 혼합 frame 실패, 기존 Mover/CMC·cook 재질 fallback 경고. 최대 총수와 최종 지상/비행/벽보행 편성은 Phase 14 단위 6에서 정한다.
- 마지막 MCP 확인(Phase 14 단위 1): PIE 종료, `LNP.Enemy.UpDeviationCheck 0`. Phase 13 마무리 때의 진단 CVar 기본값(ProfileStride/projectile validation/RewindCache/FlightPenetrationCheck/WorldCollision.LockProbe=0, sphere/grid/support cache/exact lateral sweep/parallel movement/density=1)은 이번에 바꾸지 않았다.

상세는 `history/Phase14_Log.md`(이번 Phase), `history/Phase13_Log.md`(인계 근거), 단계 순서는 `Roadmap.md`, 규약은 `design/MovementIntegration.md`와 `Decisions.md`가 소유한다. 로컬 측정 산출물은 Git에 넣지 않는다.
