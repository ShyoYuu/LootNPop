# Surface Support·Navigation 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 Phase: 10 대규모 추격 경로 비교 — 착수 대기(사용자 진행 확인 필요). 직전 Phase 14b 완료
> 마지막 갱신: 2026-10-12

## 현재 결정

**Phase 14b 완료(2026-10-12).** 벽보행의 사거리 밖 추격에 진행 기반 고정 방향 우회를 넣었다. ㄷ자 절벽 PIE에서 이전 0기였던 우회가 전원 성공했다. 설계는 `design/MovementIntegration.md` "벽 타기 NPC"의 진행 기반 우회 항목, 경위는 `history/Phase14b_Log.md`가 소유한다. 사거리를 늘린 뒤에는 ㄷ자 지형 회귀만 확인한다.

**Phase 9 완료(2026-10-12).** 실규모 블록아웃 옥탄트 `Bastion_00`·`Hollow_00`과 `Meadow_00`의 3종 혼합 풀이 다음을 통과했다.

- 베이크·전체 회귀(127검사 실패0)
- 게이트 통과 cook
- 패키지 1P, 리슨 2P/late join
- 성능·관통·메모리 Gate

완료 근거 표와 수용한 한계는 [Phase 9 실행 문서](phases/Phase09_VerticalSlice.md) §5, 경위는 `history/Phase09_Log.md`가 소유한다.

- 성능 기준선은 이제 [V011](phases/Phase09_PerfMemoryV011.md)이다. 혼합 풀 리슨 2P의 frame P95는 7.65~9.28ms다. 메모리는 decodedResident 60.11MiB, navResident 28.49MiB다. 측정은 `Scripts/Profiling/RunPhase13Gate.ps1 -Cases Production`(헤드리스·seed1)으로 하고 이전 Meadow 기준선은 Phase14 Final7이다.
- harness의 `exact ≤2ms` FAIL은 Phase 3 목표값이며 Gate가 아니다(Meadow 기준선도 FAIL).
- 관통 Gate는 활성 개체로 판정한다. 휴면 벽보행이 접안 패널에 덮이는 겹침은 수용한다(D-078). harness가 `dormantPenetratingEntityFrames`로 따로 보고한다.
- 벽보행 교전 거리(`DA_Enemy_PureEntity_Crawler01` `AttackRange`, 현재 800)는 사용자가 플레이 테스트로 정한다. 상한 5,000(=인지 거리)에서 성능 Gate를 통과했으므로 그 안의 값은 재측정하지 않는다.
- `Meadow_00`은 풀에 유지하고 재제작은 옥탄트 양산 단계에서 한다(사용자 결정).
- `OctantPoolData`의 세 definition은 전체 slot을 허용하고 풀 선택은 slot 순서 greedy다. definition별 slot mask를 다르게 주려면 D-043 제약 할당이 먼저다.
- 옥탄트 제작 규칙(반지름 300m 고정, 마커별 그래플 거리, 치수표·lint, 검토 중 `LNP.Spawn.EnemyDensity 0`)은 [설계 가이드](../Guide_OctantLevelDesign.md)·[제작 가이드](../Guide_OctantLevelInstance.md)를 따른다.
- 진단 CVar 기본값(ProfileStride/projectile validation/RewindCache/FlightPenetrationCheck/WorldCollision.LockProbe=0, sphere/grid/support cache/exact lateral sweep/parallel movement/density=1)은 바꾸지 않았다.

## 바로 다음 행동

- **Phase 10 착수 여부를 사용자에게 확인한다.** 범위는 `Roadmap.md` §Phase 10이다: 같은 benchmark harness(Phase 7 일반 A* 캡처 재생)에서 목표별 flow field와 계층형 A*의 최소 기능 프로토타입을 비교하고, 채택 기준을 통과한 방식만 production에 통합한다(D-044). 실행 문서 `phases/Phase10_*.md`는 착수 직전에 만든다.
- 착수하면 먼저 `design/GroundNavigation.md`와 Phase 7 benchmark 캡처의 현재 상태를 확인한다.

## 인계 근거

- 최신 패키지: `Saved/SurfaceNavigationPhase9V011BPackage`(Phase 14b 이전 소스, exe `732B90BD…56AA3`). 교전 거리 5,000 측정용 임시 패키지는 `Saved/SurfaceNavigationPhase9V011CPackage`다.
- 측정 원본: `Saved/Profiling/Phase09/GateV011`(V010 패키지)·`GateV011B`(최종)·`GateV011C`(교전 거리 상한)·`GateV011Fix`(기각한 휴면 운반 수정).
- 로컬 측정 산출물은 Git에 넣지 않는다.
