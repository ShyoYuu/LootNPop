# Surface Support·Navigation 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 Phase: 없음 — Phase 9 Vertical Slice 완료(2026-10-12). 다음 Phase는 사용자 확인 뒤 착수
> 마지막 갱신: 2026-10-12

## 현재 결정

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

- **다음 Phase를 사용자와 정한다.** 본선은 Phase 10(대규모 추격 경로: flow field와 계층형 A* 비교, `Roadmap.md` §4)이다. 병렬 분기 Phase 14b(벽보행 사거리 밖 추격 우회, [계획](phases/Phase14b_CrawlChaseDetour.md))는 교전 거리를 늘린 플레이 테스트 뒤 범위를 다시 판단한다. 사거리가 늘면 14b가 푸는 평형점 맴돌기 사례가 줄어든다.
- 선택한 Phase의 실행 문서는 착수 직전에 만들거나 구체화한다(README §8).

## 인계 근거

- 최신 패키지: `Saved/SurfaceNavigationPhase9V011BPackage`(현재 소스, exe `732B90BD…56AA3`). 교전 거리 5,000 측정용 임시 패키지는 `Saved/SurfaceNavigationPhase9V011CPackage`다.
- 측정 원본: `Saved/Profiling/Phase09/GateV011`(V010 패키지)·`GateV011B`(최종)·`GateV011C`(교전 거리 상한)·`GateV011Fix`(기각한 휴면 운반 수정).
- 로컬 측정 산출물은 Git에 넣지 않는다.
