# Surface Support·Navigation 현재 작업 상태

> 상태: 활성 인계 문서
> 현재 Phase: Phase 9 Vertical Slice — 단위 2 완료(Bastion_00 1차 확정), 단위 3(Hollow_00) 대기(2026-10-07)
> 마지막 갱신: 2026-10-07

## 현재 결정

Phase 9를 착수했다. 실행 문서는 `phases/Phase09_VerticalSlice.md`, 경위는 `history/Phase09_Log.md`가 소유한다.

- 품질 기준: 실규모 레벨 디자인(동선·스케일 최종, 외형은 단순 머티리얼 블록아웃). 아트 패스는 범위 밖이다.
- 새 옥탄트 2개(가칭 A `Bastion_00` 입체 요새형, B `Hollow_00` 협곡·부유섬형). 지하 공동 입구 조합을 A는 긴 경사로+수직 통로, B는 수직 통로만으로 다르게 한다. `Meadow_00`은 풀에 남긴다.
- 메시 생성·배치는 Claude가 맡고(Blender MCP·에디터 명령), 사용자는 PIE로 동선을 검토한다.
- 지형을 그래플 거리에 맞추지 않는다(2026-10-07 사용자 지시). 먼 앵커는 마커별 `GrappleMaxDistance`로 거리를 준다.
- 스케일 기준(2026-10-07): 지면 위 200m·아래 100m, 거인 스케일 구조물(계단 칸만 플레이어 스케일), 폐허 위주, 옥탄트마다 메인 무대 하나(보스 자리). 다른 옥탄트에서 +240/−150, +180/−130도 시험한다.
- 검토 중 적은 에디터 세션 CVar `LNP.Spawn.EnemyDensity 0`으로 끈다(에셋 무변경).
- 레이아웃은 플레이어 치수표와 규칙(`LNPSliceOctant.cpp`의 `PlayerMetrics`·`DesignRules`, 표는 `../Guide_OctantLevelInstance.md` §6)으로 검사한다. 생성 로그 `lint=0`을 유지한다.
- 직전 Phase 14 결과(D-075~077, 편성 D-076)는 그대로 유효하다. 알려진 벽보행 한계는 `history/Phase14_Log.md`를 따른다.

## 바로 다음 행동

1. 단위 3(Hollow_00)에 착수한다. 검토는 spec에 검토 지점을 두고 `LNP.Debug.TeleportToReview [Name] [Slot]`(인자 없으면 목록)으로 이동하며, 사용자가 위치를 알려 줄 때는 `LNP.Debug.WhereAmI`로 slot·로컬 좌표·발밑 메시를 읽는다.
2. 제작 절차는 `../Guide_OctantLevelInstance.md`: `LNP.SurfaceNav.BuildSliceOctant <Name>`(로그 `lint=0` 확인, 생성 없이 검사만은 `LintSliceOctant`) → `BakeOctant` → PIE 게시 확인. 옥탄트 spec 수정은 Live Coding으로 반영된다(spec은 호출마다 생성). 새 콘솔 명령 추가만 전체 빌드가 필요하다.
3. 부하 측정은 `Scripts/Profiling/RunPhase13Gate.ps1`을 쓴다. Production 관통은 4회 이상 돈다. PIE 리슨 2P는 `LevelEditorPlaySettings.PlayNumberOfClients`를 2로 올리고 끝나면 1로 되돌린다.

## 인계받은 근거와 원본

- 성능 기준: Phase 14 최종 패키지 `Saved/SurfaceNavigationPhase14Final7Package`(Meadow, D-076, frame P95 10.71/10.67ms), 원본 `Saved/Profiling/Phase14/Final7`.
- 진단 CVar 기본값(ProfileStride/projectile validation/RewindCache/FlightPenetrationCheck/WorldCollision.LockProbe=0, sphere/grid/support cache/exact lateral sweep/parallel movement/density=1)은 바꾸지 않았다.
- `OctantPoolData`에 Meadow·Bastion_00·Hollow_00 세 definition이 있다(전체 slot 허용). 풀 선택은 아직 slot 순서 greedy다. 새 definition에 slot mask 제한을 두면 D-043 제약 할당이 먼저다.

규약은 `design/TerrainContract.md` §5·§6·§6-1·§7, 단계 순서는 `Roadmap.md`가 소유한다. 로컬 측정 산출물은 Git에 넣지 않는다.
