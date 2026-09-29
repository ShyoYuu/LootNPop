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

## 2026-09-29 — 구현 단위 1 완료

- 저장된 `DA_OctantSurface_Fixture_Regression`과 같은 fixture exact source를 쓰는 `EnemyMovement.GroundedFixture` 자동화를 추가했다. 지각·동굴 공동의 direct cache hit, 섬 가장자리의 exact `LostSupport`, 정적 나무 blocker의 공통 수평 capsule sweep을 Phase 3b exact-only 결과와 비교한다.
- 가파른 경사·절벽·섬 몸체·동굴·Unknown은 기존 `ExactMovement` 4개 회귀와 함께 검사한다. 최종 결과는 EnemyMovement 3/3, ExactMovement 4/4 통과다(`Saved/Logs/Phase06_Unit1_EnemyMovementTests.log`, `Phase06_Unit1_ExactMovementTests.log`).
- 부하 harness가 capture 시작에 `EnemyGround` counter를 초기화하고 종료 보고에 cache/exact 수를 자동 기록하게 했다. 콘솔 `EnemyGround.Reset`·`Report`도 같은 공개 계측 API를 쓴다.
- 에디터 `-game` `TestMap03`, 지상 적 100·투사체 0·병렬 cache-first, warm-up 10초/capture 30초 실행에서 grounded 194,138 step, cache hit 167,090(86.07%), exact fallback 27,048, exact-only 0이었다. 합성 넉백 265회와 착지 260회가 있었고 Unknown hit·Layer jump·Envelope escape는 0이었다.
- 같은 실행에서 exact query P95 0.630ms, scene lock P95 0.025ms, `ProbeFaceIndex`·`ProbeSourceKeys`·`ProbeSurfaceData`가 PASS했다. 프레임 P95 20.35ms는 에디터 빌드라 성능 Gate로 쓰지 않고 Phase 6 구현 단위 5의 Development package 재측정에 남긴다(`Saved/Logs/Phase06_Unit1_EditorGame.log`).
- `LootNPopEditor Win64 Development` 전체 빌드가 성공했다. 구현 단위 1 완료 조건인 smooth interior exact probe 제거와 회귀 fixture의 Phase 3b 결과 일치를 충족했다.

## 2026-09-29 — 구현 단위 2 완료

- 공중 이동의 handle 전이를 `UpdateSurfaceHandleAfterAirborne` 경계로 분리했다. 공중 프레임은 이전 handle을 무효화하고, 착지한 경우에만 exact static Support identity의 `(slot, LocalLayerId, SurfaceDataGeneration)`으로 재획득한다. 넉백과 사망 팝은 기존처럼 같은 `StepAirborne` wrapper를 공유한다.
- exact `ProjectToSameLayer`가 선택적으로 착지 identity를 반환하게 했다. 새 `ProjectWanderTarget`은 현재 handle을 선호한 immutable snapshot 조회를 먼저 수행하고, 비확신 결과만 exact로 폴백한다. cache 또는 exact가 현재 `(slot, LocalLayerId, generation)`과 다르면 목표를 거부한다.
- `FLNPEnemyIdleTask`를 새 배회 경계로 전환했다. 비행 NPC 경로와 `EnemyExactGround=0` legacy 비교 경로는 유지했다.
- `SurfaceHandleIdentity`가 공중 무효화·정적 착지 재획득·Blocker 착지 거부를 검사한다. `GroundedFixture`가 동굴 비지각 Layer 유지, 섬 가장자리 밖 목표 거부, 섬 아래 지각 Layer 0 유지까지 검사한다.
- 최종 자동화는 EnemyMovement 3/3, ExactMovement 4/4 통과했고 `LootNPopEditor Win64 Development` 전체 빌드가 성공했다(`Saved/Logs/Phase06_Unit2_EnemyMovementTests.log`, `Phase06_Unit2_ExactMovementTests.log`).
- 에디터 `-game` `TestMap03`, 지상 적 100·투사체 0·병렬 cache-first capture에서 grounded 247,012 step, cache hit 226,992(91.90%), exact fallback 20,020이었다. 합성 넉백 270회·착지 266회, Layer jump·Unknown hit·Envelope escape 0이고 frame P95 13.45ms, exact P95 0.628ms, lock P95 0.024ms와 face/source/snapshot probe가 모두 PASS했다(`Saved/Logs/Phase06_Unit2_EditorGame.log`).

## 2026-09-29 — 구현 단위 3 완료

- `ALNPEnemyCharacter::SyncToEntity`가 Mover의 공중 여부·속도와 `TryGetFloorCheckHitResult` 결과를 내보내게 했다. `ULNPEnemyActorSyncProcessor`는 floor hit을 immutable registry snapshot으로 해석하고 `UpdateActorHandoff`를 통해 정적 Support면 정확한 `FLNPSurfaceHandle`, 움직이는 패널이면 별도 contact를 기록한다. 공중이면 기존 지면 상태를 모두 무효화하고 Mover 속도를 그대로 `FLNPEnemyVelocityFragment`에 남긴다.
- Entity→Actor에서는 기존 handle/contact를 건드리지 않고 Mover가 floor를 다시 찾는다. Mover 활성화 직후 floor blackboard가 아직 비어 있는 프레임은 Entity의 마지막 handle을 보존한다. `EnemyMovement.ActorHandoff`가 정적 floor 교체, 동적 floor 전환, floor 미게시 보존, 공중 속도 인계를 검사한다.
- `FLNPEnemyDynamicSupportContact`에 `(slot, MarkerId)`, 패널 로컬 캡슐 중심, 마지막 선속도를 분리 저장한다. PureEntity grounded 프레임은 게시된 `FLNPDynamicSupportFrame`의 현재 transform으로 위치·회전을 운반한 뒤 exact 접지를 재검증한다. contact 상실·가장자리 이탈 시 마지막 패널 선속도를 공중 속도에 더한다.
- `EnemyMovement.DynamicSupportContact`가 transform delta·회전·속도 보존을 검사한다. `GroundedFixture`에는 실제 `LNPDynamicTerrain` collision과 marker identity를 가진 합성 패널을 추가해 공중 capsule exact 착지→contact 생성→패널 collision 이동→같은 패널 exact 재접지를 검사한다.
- 첫 late-join 2P 스모크에서 호스트 worker가 `ULNPEnemyMovementProcessor::Execute` 끝의 `DynamicFrame` shared ref 해제 중 접근 위반으로 종료됐다. 패널 Actor tick은 Mass와 게시 tick 각각의 선행 조건이었지만, 게시 tick 자체가 Mass의 선행 조건은 아니어서 두 작업이 같은 패널 뒤 동시에 시작할 수 있었다.
- `ULNPDynamicTerrainSubsystem`이 `패널 Actor tick -> DynamicSupport PublishTick -> Mass PrePhysics`의 완전한 prerequisite 체인을 만들도록 수정했다. worker가 `Frame`을 복사하기 전에 게임 스레드 게시가 끝나므로 shared ref 교체 경쟁이 사라진다.
- 수정 뒤 late-join 리슨 2P 무인 재실행에서 호스트·게스트 모두 패널 8개가 같은 marker/path start로 활성화되고 `ProbePanels` 8/8, `ProbeSurfaceData` query 8/8·binding 88/88로 PASS했다. `MassPrePhysicsOrder`는 호스트 75,088회·게스트 72,544회 검사에서 위반 0, Unknown hit·Envelope escape·ensure·crash 0이며 양쪽 정상 종료했다(`Saved/Logs/Phase06_Unit3_2P_Host.log`, `Phase06_Unit3_2P_Guest.log`).
- 최종 `LootNPopEditor Win64 Development` 전체 빌드 성공. EnemyMovement 5/5, ExactMovement 4/4 통과(`Saved/Logs/Phase06_Unit3_EnemyMovementTests.log`, `Phase06_Unit3_ExactMovementTests.log`). 구현 단위 4의 legacy 제거로 진행한다.

## 2026-09-29 — 구현 단위 4 완료

- Enemy 지상 이동과 공중 적분의 `EnemyExactGround=0` 분기를 삭제했다. 접지는 `LNPEnemySurfaceMovement::StepGrounded`의 cache-first/exact 폴백, 공중은 `StepAirborne` exact만 사용한다. Idle 배회도 다층 `ProjectWanderTarget` 외의 Layer 0 fallback을 제거했다.
- `ULNPSurfaceDataSubsystem::GetSurfacePoint` adapter와 다층 경고 상태를 삭제했다. exact oracle과 부하 harness의 의도적인 지각 비교·배치만 immutable snapshot의 `LNPSurfaceDataLoading::QueryLayerZero`를 직접 사용하며 일반 이동 소비자는 이 API를 쓰지 않는다.
- `ULNPSurfaceCacheSubsystem`의 헤더·구현과 `SurfaceCacheCellSpacing`·`SurfaceCacheSamplesPerFrame` 설정을 삭제했다. `LNP.SurfaceNav.EnemyExactGround` CVar와 관련 런타임 주석도 제거했다. 소스·설정 검색에서 이 심볼들과 `GetSurfacePoint` 호출은 0건이다.
- `LootNPopEditor Win64 Development` 전체 빌드가 성공했다. EnemyMovement 5/5, ExactMovement 4/4, 전체 `LootNPop.SurfaceNavigation` 64/64가 통과했다(`Saved/Logs/Phase06_Unit4_EnemyMovementTests.log`, `Phase06_Unit4_ExactMovementTests.log`, `Phase06_Unit4_SurfaceNavigationTests.log`).
- 리슨 2P `-game` 스모크를 지상 적 100·투사체 0·cache-first로 실행했다. 서버는 grounded 125,700 step 중 cache hit 112,950(89.86%), exact fallback 12,750, exact-only 0이며 넉백 268회·착지 276회·Layer jump 0이었다. 호스트·게스트 모두 `ProbePanels`와 `ProbeSurfaceData`가 PASS했고 DynamicSupport→Mass 순서 위반, Unknown hit, Envelope escape, ensure, crash가 없었다(`Saved/Logs/Phase06_Unit4_2P_Host.log`, `Phase06_Unit4_2P_Guest.log`).
- 호스트 프레임 P95 29.35ms는 같은 머신에서 에디터 프로세스 2개를 동시에 실행한 기능 스모크라 성능 Gate로 쓰지 않는다. 구현 단위 5에서 Development package 단독 실행으로 exact-only/cache-first 부하를 다시 측정한다.

## 2026-09-29 — 구현 단위 5 완료, Phase 6 종료

- Win64 Development BuildCookRun이 build·full cook 972 packages·stage·pak·archive까지 성공했다. 기존 Lyra Mannequin Material Function 누락 경고만 재현됐고 오류는 0이었다.
- `Scripts/Profiling/RunLoadBaselineMatrix.ps1`를 삭제된 `EnemyExactGround` 대신 `EnemySupportCache=0/1`을 쓰는 Phase 6 매트릭스로 갱신했다. 모든 실행은 같은 Development package의 리슨 2P, `-nullrhi -corelimit=4`, 투사체 500, 병렬 이동 조건이다.

| 조건 | 서버 프레임 P50/P95 | exact CPU P50/P95 | query/frame P50 | grounded cache hit | 결과 |
|:---|:---|:---|:---|:---|:---|
| 700 exact-only | 15.12/16.17ms | 6.755/7.354ms | 1,524 | 0% | PASS |
| 700 cache-first | 13.60/14.65ms | 2.745/3.082ms | 902 | 90.60% | PASS |
| 750 exact-only | 15.82/17.44ms | 7.044/7.633ms | 1,588 | 0% | FAIL |
| 800 cache-first | 14.68/16.46ms | 3.124/3.573ms | 985 | 90.32% | PASS |
| 850 cache-first | 15.66/17.44ms | 3.377/3.776ms | 1,022 | 90.42% | FAIL |
| 1,000 cache-first | 17.93/20.20ms | 3.714/4.128ms | 1,093 | 90.93% | FAIL |
| 1,500 cache-first | 25.52/27.65ms | 5.593/6.103ms | 1,438 | 91.10% | FAIL |

- 프레임 P95 16.6ms 기준 병렬 exact-only 한계는 약 700마리로 Phase 3c 재확인값과 같고, cache-first 한계는 약 800마리다. 적 수 한계가 100마리(약 14%) 늘었다. 700마리에서 query/frame P50은 40.8%, exact CPU P95는 58.1%, 프레임 P95는 9.4% 줄었다.
- high-load에서 exact CPU 2ms 진단 예산은 넘지만, 투사체·공중·risk 구간의 correctness-mandatory query를 생략하지 않는 설계 결과다. 적 수 한계 판정은 Phase 3b와 동일하게 서버 CPU 프레임 P95 16.6ms를 쓴다.
- 모든 매트릭스 실행에서 Layer jump·Unknown hit·Envelope escape·DynamicSupport→Mass 순서 위반·ensure·crash가 0이고 `ProbePanels`·`ProbeSurfaceData`가 호스트와 게스트에서 PASS했다. 로그는 `Saved/Profiling/Phase06`이다.
- 한계 밖의 1,500마리 cache-first 게스트에서 기존 네트워크 복제 계층의 `Duplicate Mass NetID` 오류가 1회 기록됐다. 해당 실행은 정상 종료했고 Surface Navigation probe·identity·순서 판정에는 이상이 없었으므로 Phase 6 Gate에는 포함하지 않되 후속 네트워크 부하 진단 대상으로 남긴다.
- Development package 1P 100마리 스모크도 frame P95 2.93ms, exact P95 0.598ms, lock P95 0.013ms로 PASS했다. cache hit 78.41%, 넉백 293회·착지 296회·Layer jump 0이고 패널·SurfaceData probe와 순서 검사가 통과했다(`Saved/Logs/Phase06_Unit5_1P.log`).
- 전체 자동화 64/64, 에디터 전체 빌드, Development package, 1P와 리슨 2P를 모두 통과했다. Phase 6 완료 조건을 충족했고 다음은 Phase 7a Nav 데이터 기반 실행 계획 작성이다.
