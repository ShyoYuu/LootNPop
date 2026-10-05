# Surface Support·Navigation 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 Phase: Phase 14 진행 중 — 단위 1 완료, 단위 2 진행 중(2a~2d 완료, 2e 단순 우회 남음)
> 마지막 갱신: 2026-10-06

## 현재 결정

Phase 13은 완료됐다(D-074: Pod 귀속 PureEntity 비행 600 임시 월드 편성). 사용자 지시로 Phase 14 벽 타기 NPC(D-066)에 착수했다. 실행 문서는 `phases/Phase14_SurfaceCrawlNpc.md`다. 벽보행 적은 벽면 전용 스폰 후보 없이 Pod 주변 지면에서 스폰돼 스스로 벽으로 간다. 간단한 구현을 먼저 하고 나머지는 플레이에서 필요가 생길 때 계획한다(사용자 결정).

단위 1은 완료됐다(적 Up을 엔티티 회전 Z에서 읽기, 스폰 회전 수정 후 편차 0.3° 미만).

단위 2는 실행 문서 "단위 2 세분"을 따른다. 사용자 결정: 이동 몸체는 구(`CapsuleHalfHeight = CapsuleRadius`), 외형 컨셉은 로봇청소기형(비행 드론과 비슷한 크기, 모델링은 단위 5), `DA_MassSpawnConfig`는 임시 테스트 편성이라 벽보행 검증 때 편성 전체를 벽보행으로 바꾼다, 수직 지형이 많은 검증 월드가 필요하다. 2a 도메인 배선·2b 순수 함수 `LNPSurfaceCrawl`·2c `ULNPEnemyCrawlMovementProcessor` 코드를 마쳤고 전체 빌드, 신규 자동화 3/3, 단위 1 관련 자동화 30/30을 통과했다. 벽보행 에셋(`DA_Enemy_PureEntity_Crawler01`·`DA_EnemyEntityConfig_PureEntity_Crawler01`, 반지름 80, 공격 거리 1m)을 만들고 TestMap03 PIE에서 경사 추종·벽 따라 추격·넉백 착지 재부착·총구 방향을 사용자가 확인했다. 2d 벽보행 검증 옥탄트(`LVI_Octant_Fixture_Crawl`)에서 벽 위·천장까지 추격을 확인했다. 사용자 결정으로 단순 우회 시도(2e)를 단위 2에 더했다.

현재 `DA_MassSpawnConfig`는 4개 세트 모두 벽보행 3기씩(벽보행 360기, 나머지 0)이다. 이전 편성은 `history/Phase14_Log.md` 2026-10-06 항목에 있다.

## 바로 다음 행동

1. 2e 단순 우회 시도를 실행 문서와 `design/MovementIntegration.md` "벽 타기 NPC" 우회 규칙대로 구현한다. 끝나면 단위 3(법선 복제·되감기)에 착수한다. 벽보행 PIE는 TestMap03의 pool을 메모리에서만 `LVI_Octant_Fixture_Crawl`로 바꿔 띄운다(`history/Phase14_Log.md` 2026-10-06 단위 2d). TestMap03에서 본 옆으로 누운 개체는 검증 옥탄트에서 재현되지 않아 원인 미확정으로 남긴다.
2. 단순 조향 한계(돌출부 밑면에서 떨림, 판 윗면으로 돌아오지 못함): 길찾기 없는 한계 자체는 받아들이되 간단한 우회 시도는 한다(사용자 결정, 2e).
3. 알려진 단위 2 한계: 벽보행 개체가 든 Pod는 활동 대역에서 항상 활성이고(Support handle 없음), 게스트에는 구 중심 Up 회전으로 보인다(단위 3). 3D 분리·360° 인지·재귀속은 단위 4다.

## 인계받은 근거와 원본

- Phase 13 최신 패키지: `Saved/SurfaceNavigationPhase13PenetrationFixedPackage`, exe SHA256 `D9A5D1C8CBD4938166484FC0C6C9B47E245656FBC010A477A1E198E437291723`. 관련 자동화 25/25 PASS.
- Pod 귀속 성능: 기본 실제 지상 709/비행 600 P95 6.61ms, 반경 1,500cm 전량 720/600 두 반복 6.63/6.93ms·frame/path PASS. `Saved/Profiling/Phase13/PenetrationFix`.
- 증량 검토: 비행 900 단독 5.85ms·1,200 9.01/9.01ms. 지상 800+비행 900은 안정적 합격이 아니다. `Saved/Profiling/Phase13/FlightPopulationReview`.
- 보존한 한계: 기본 반경 seed 1 지상 11개 부족, 전량 활성/증량의 exact 2ms 진단 초과와 혼합 frame 실패, 기존 Mover/CMC·cook 재질 fallback 경고. 최대 총수와 최종 지상/비행/벽보행 편성은 Phase 14 단위 6에서 정한다.
- 마지막 MCP 확인(Phase 14 단위 1): PIE 종료, `LNP.Enemy.UpDeviationCheck 0`. Phase 13 마무리 때의 진단 CVar 기본값(ProfileStride/projectile validation/RewindCache/FlightPenetrationCheck/WorldCollision.LockProbe=0, sphere/grid/support cache/exact lateral sweep/parallel movement/density=1)은 이번에 바꾸지 않았다.

상세는 `history/Phase14_Log.md`(이번 Phase), `history/Phase13_Log.md`(인계 근거), 단계 순서는 `Roadmap.md`, 규약은 `design/MovementIntegration.md`와 `Decisions.md`가 소유한다. 로컬 측정 산출물은 Git에 넣지 않는다.
