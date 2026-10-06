# Surface Support·Navigation 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 Phase: Phase 14 진행 중 — 단위 1·2·3 완료, 단위 4는 4a~4f 구현·다음은 섬 스폰 테스트 레벨과 통합 PIE
> 마지막 갱신: 2026-10-06

## 현재 결정

Phase 13은 완료됐다(D-074: Pod 귀속 PureEntity 비행 600 임시 월드 편성). 사용자 지시로 Phase 14 벽 타기 NPC(D-066)에 착수했다. 실행 문서는 `phases/Phase14_SurfaceCrawlNpc.md`다. 벽보행 적은 벽면 전용 스폰 후보 없이 Pod 주변 지면에서 스폰돼 스스로 벽으로 간다. 간단한 구현을 먼저 하고 나머지는 플레이에서 필요가 생길 때 계획한다(사용자 결정).

단위 1은 완료됐다(적 Up을 엔티티 회전 Z에서 읽기, 스폰 회전 수정 후 편차 0.3° 미만).

단위 2는 실행 문서 "단위 2 세분"을 따른다. 사용자 결정: 이동 몸체는 구(`CapsuleHalfHeight = CapsuleRadius`), 외형 컨셉은 로봇청소기형(비행 드론과 비슷한 크기, 모델링은 단위 5), `DA_MassSpawnConfig`는 임시 테스트 편성이라 벽보행 검증 때 편성 전체를 벽보행으로 바꾼다, 수직 지형이 많은 검증 월드가 필요하다. 2a 도메인 배선·2b 순수 함수 `LNPSurfaceCrawl`·2c `ULNPEnemyCrawlMovementProcessor` 코드를 마쳤고 전체 빌드, 신규 자동화 3/3, 단위 1 관련 자동화 30/30을 통과했다. 벽보행 에셋(`DA_Enemy_PureEntity_Crawler01`·`DA_EnemyEntityConfig_PureEntity_Crawler01`, 반지름 80, 공격 거리 1m)을 만들고 TestMap03 PIE에서 경사 추종·벽 따라 추격·넉백 착지 재부착·총구 방향을 사용자가 확인했다. 2d 벽보행 검증 옥탄트(`LVI_Octant_Fixture_Crawl`)에서 벽 위·천장까지 추격을 확인했다. 사용자 결정으로 단순 우회 시도(2e, `LNPSurfaceCrawl::UpdateDetour`)를 더했고, 돌출부·판 밑면에서 일부 개체가 우회해 넘어오는 것을 사용자가 확인해 단위 2를 완료했다.

단위 3은 완료됐다(법선 복제). `FLNPReplicatedAgent::SurfaceNormal`(octahedral 16bit 형제 필드, 0 = 구 접평면 기저)에 벽보행 개체의 회전 Z를 싣고 Yaw를 양자화 법선 기저로 인코딩한다. 자동화 36/36, 리슨 2P 게스트에서 벽·판 밑면·모서리 자세가 호스트와 일치함을 사용자가 확인했다. 벽보행 판정 캡슐이 구라 위치 이력에 Up을 기록하지 않았다(`design/MovementIntegration.md` "되감기").

단위 4는 실행 문서 "단위 4 세분"(4a~4f)을 따른다. 4a 인지·조준(벽보행 Config `VisionAngle` 360·`AimPitch` ±89·사거리 800), 4b LoS 반응(사선이 막히면 사거리 안에서도 계속 접근), 4c 3D 분리(`ULNPCrawlSpatialGridSubsystem`, 분리 반경 220)를 마쳤다. 자동화 36/36, 1P PIE에서 겹침 없음·사선 트이면 즉시 사격·막히면 접근을 사용자가 확인했다. 적 NPC 전반의 시야 상실 후 추적 제한은 `../Idea_Backlog.md` 후보로 올렸다(사용자 결정). 4d 재귀속(Home에 10초간 가까워지지 않으면 직선거리 최근접 활성 Pod, `[SurfaceCrawlHome]` 로그)은 자동화 37/37과 PIE 재귀속 1건으로 완료했다(사용자 결정). 4e 휴면·벽보행 구간 배회(같은 면 위 최대 180cm·정지 9초)와 4f 배회 복제(Flags bit 5, 법선 고정 Up)를 구현하고 자동화 39/39를 통과했다. PIE는 아직 하지 않았다.

현재 `DA_MassSpawnConfig`는 4개 세트 모두 벽보행 3기씩(벽보행 360기, 나머지 0)이다. 이전 편성은 `history/Phase14_Log.md` 2026-10-06 항목에 있다.

## 바로 다음 행동

1. 섬 위 스폰(플레이어·NPC) 테스트 레벨을 준비한다.
2. 그 레벨에서 4d 재귀속(섬 밑면의 NPC를 런처로 흩기)·4e 휴면/배회·4f 리슨 2P 배회 자세를 한 번에 확인한다(사용자 결정). 통과하면 단위 4를 완료한다.
3. 벽보행 PIE: TestMap03의 `OctantPoolData` 정의를 MCP `ObjectTools.set_properties`로 메모리에서만 `LVI_Octant_Fixture_Crawl`·`DA_OctantSurface_Fixture_Crawl`로 바꾸고, 광장 위 시작 transform(위치 17176.17×3, 회전 pitch 0·yaw 135·roll 125.2644)으로 `StartPIE`한 뒤 즉시 Meadow 정의로 되돌린다. 리슨 2P는 `LevelEditorPlaySettings.PlayNumberOfClients`를 2로 올리고 끝나면 1로 되돌린다(사용자가 현재 1로 둔다 — 게스트가 어그로를 가져가 1P 확인이 편하다). 에디터 종료 때 `OctantPoolData`는 저장하지 않는다. 콘솔 명령은 MCP SlateInspector로 Output Log 입력창에 입력한다.
4. 알려진 한계: 길찾기 없이 단순 우회만 한다(2e, 사용자 수용). 부하 harness 활동 통계는 벽보행을 지상으로 센다(단위 6). 개체가 많으면 뒤쪽 개체의 분리력이 앞쪽을 사거리보다 가까이 밀어낸다(사용자 관찰, 수용). TestMap03에서 본 옆으로 누운 개체는 검증 옥탄트에서 재현되지 않아 원인 미확정이다.

## 인계받은 근거와 원본

- Phase 13 최신 패키지: `Saved/SurfaceNavigationPhase13PenetrationFixedPackage`, exe SHA256 `D9A5D1C8CBD4938166484FC0C6C9B47E245656FBC010A477A1E198E437291723`. 관련 자동화 25/25 PASS.
- Pod 귀속 성능: 기본 실제 지상 709/비행 600 P95 6.61ms, 반경 1,500cm 전량 720/600 두 반복 6.63/6.93ms·frame/path PASS. `Saved/Profiling/Phase13/PenetrationFix`.
- 증량 검토: 비행 900 단독 5.85ms·1,200 9.01/9.01ms. 지상 800+비행 900은 안정적 합격이 아니다. `Saved/Profiling/Phase13/FlightPopulationReview`.
- 보존한 한계: 기본 반경 seed 1 지상 11개 부족, 전량 활성/증량의 exact 2ms 진단 초과와 혼합 frame 실패, 기존 Mover/CMC·cook 재질 fallback 경고. 최대 총수와 최종 지상/비행/벽보행 편성은 Phase 14 단위 6에서 정한다.
- 마지막 MCP 확인(Phase 14 단위 1): PIE 종료, `LNP.Enemy.UpDeviationCheck 0`. Phase 13 마무리 때의 진단 CVar 기본값(ProfileStride/projectile validation/RewindCache/FlightPenetrationCheck/WorldCollision.LockProbe=0, sphere/grid/support cache/exact lateral sweep/parallel movement/density=1)은 이번에 바꾸지 않았다.

상세는 `history/Phase14_Log.md`(이번 Phase), `history/Phase13_Log.md`(인계 근거), 단계 순서는 `Roadmap.md`, 규약은 `design/MovementIntegration.md`와 `Decisions.md`가 소유한다. 로컬 측정 산출물은 Git에 넣지 않는다.
