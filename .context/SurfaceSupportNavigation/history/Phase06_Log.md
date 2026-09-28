# Phase 6 작업 로그

## 2026-09-28 — 구현 단위 0 착수

- `phases/Phase06_EnemyMovementMigration.md`를 작성했다.
- PureEntity grounded에서는 수평 blocker sweep을 유지하고 하향 support probe만 cache-first로 바꾸기로 확정했다.
- `NoSupport`를 즉시 낙하로 간주하지 않고 동적 Support·낙하 전환 확인을 위해 exact 폴백하기로 했다.
- airborne은 계속 exact로 유지하고, cache hit와 exact landing 모두 `FLNPSurfaceHandle`을 갱신하도록 범위를 잡았다.
- ActorPromoted의 Actor→Entity 전환에는 Mover floor identity 역조회가 필요함을 확인했다.

## 2026-09-29 — 구현 단위 0 완료, 구현 단위 1 착수

- `LNPEnemySurfaceMovement`를 추가했다. `LNP.SurfaceNav.EnemySupportCache=0`은 Phase 3b exact-only, `1`은 cache-first이며 기본값은 1이다.
- `LNP.SurfaceNav.EnemyGround.Report`·`Reset`을 추가해 grounded step, cache hit, exact fallback, exact-only step을 센다.
- Phase 3b exact 접지를 수평 blocker sweep과 도달점 support probe로 분리했다. cache-first 실패 뒤 같은 도달점에서 probe하므로 수평 sweep을 중복하지 않는다.
- MovementProcessor는 Surface snapshot을 실행당 한 번만 잡고 병렬 worker에 immutable 포인터로 전달한다. 엔티티마다 thread-safe shared ref를 증감하지 않는다.
- cache-first는 유효한 현재 `SurfaceHandle`이 같은 snapshot generation을 가리킬 때만 허용한다. handle이 없는 DynamicSupport·미확정 접촉과 stale generation은 exact로 폴백한다.
- 공중 착지와 grounded exact probe가 face identity의 `(slot, LocalLayerId, SurfaceDataGeneration)`을 `FLNPSurfaceHandle`로 바꿀 수 있게 했다.
- 새 자동화 `EnemyMovement.CachedGroundDecision`과 `SurfaceHandleIdentity`가 2/2 통과했다. 기존 `ExactMovement` 4개도 4/4 통과했다.
- `LootNPopEditor Win64 Development` 전체 빌드가 성공했다.
- 새 소스 추가로 unity 묶음이 달라지며 Phase 5의 두 익명 `GuidLess`가 충돌했다. `LNPSpawnData.cpp` 쪽을 `SpawnGuidLess`로 구체화해 동작 변경 없이 빌드 안정성을 복구했다.
