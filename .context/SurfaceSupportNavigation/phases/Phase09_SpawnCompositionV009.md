# Phase 9 혼합 풀 SourceKeys·정식 편성 배치 부족 수정 V009

> 상태: 패키지1P 기능 검사 전체 통과. exact P95 초과는 성능 단위에 남는다. 2P·late join은 미실행
> 날짜: 2026-10-12
> 범위: V008 패키지1P의 잔여 실패 2건(혼합 풀 `ProbeSourceKeys`, 정식 편성 적105마리 부족)의 재현·수정·재패키징·동일1P

## 1. 혼합 풀 SourceKeys probe

기존 probe는 모든 key가 8 slot에 한 번씩 있어야 하고 slot마다 source 수가 같아야 한다고 요구했다. 단일 Meadow 8slot 시절의 가정이다. 혼합 풀(Meadow11·Bastion100·Hollow12)에서는 123 unique key가 모두 inconsistent로 집계되어 FAIL이었다.

- `LNPHitIdentityProbe.cpp`: slot마다 runtime Level Instance의 Support key를 다시 모아, 그 slot에 게시된 SurfaceData snapshot의 저장 source 표(`FLNPSupportAtlas::Sources`)와 누락·예상 밖·중복을 대조한다. slot 사이 비교는 하지 않는다.
- 런타임 로더의 `BuildSourceBindings`와 같은 규칙이지만 key를 독립적으로 다시 수집하므로 교차 확인 역할은 유지한다. 실패 시 slot·key를 경고로 남긴다.

## 2. 정식 편성 적 배치 부족

### 재현

새 Editor 검사 `Runtime.ProductionSpawnComposition`이 실제 풀 선택→snapshot→정식 `DA_MassSpawnConfig`로 `BuildPlan`을 실행한다. 패키지 배치(Meadow0/3·Bastion1/5/7·Hollow2/4/6, WorldSeed1)와 production 검사와 같은 32 seed를 검사하며, Pod 부족0과 적 전량 배치를 요구한다. 수정 전 패키지 배치에서 **2130/2235(105부족)**로 패키지 로그와 같은 값이 재현됐고 32 seed 대부분도 부족했다.

### 원인

랜덤 Pod 후보는 Pod 자리의 headroom과 Pod 간 거리만 보고 채택했다. 적은 Pod와 **같은 slot·Layer**의 Enemy 후보 중 반경1,500cm 안·headroom 충족·200cm 간격인 자리만 받는다. 부족 Pod 대부분은 반경 안 후보가 2·3·7·13·16개처럼 편성 27마리보다 적은 좁은 Layer(Bastion layer12·13·195, Hollow layer7 등)에 놓인 랜덤 Pod였다. 일부는 후보는 충분했지만 다른 Pod와의 경합·간격 때문에 원거리형 1~3마리가 모자랐다. 수동 앵커 Pod(Bench01_3 generic30)는 패키지 배치에서 부족이 없었다.

### 수정

`LNPMassSpawnPlan.cpp`의 할당 순서를 바꿨다. 영구 규약은 `design/SurfaceBaking.md` Mass Spawn stream 절이 소유한다.

1. 세트 순서대로 수동 앵커(세트 지정→generic)를 먼저 고른다. 이 선택은 랜덤 후보와 무관하므로 기존 결과와 같다.
2. 수동 앵커 Pod의 적을 배치한다(편성 지정 Pod 우선). 수동 전투가 출현 후보를 랜덤 Pod보다 먼저 갖는 기존 규칙을 유지한다.
3. 랜덤 Pod는 그 자리에서 편성 전량을 실제로 배치할 수 있을 때만 채택하고 적 후보를 즉시 확정한다. 실패한 후보는 되돌리고 다음 후보로 넘어간다.

적 후보는 섞인 순서를 유지한 채 (slot, Layer)별로 묶어 조회한다. 거리·headroom·간격 규칙과 편성 수량·반경은 바꾸지 않았다. 테스트 편성으로 바꾸거나 부족을 허용하지 않았다.

### 결과

| 검사 | 수정 전 | 수정 후 |
|:---|:---|:---|
| 패키지 배치 Editor 재현 | 2130/2235 | 2235/2235, Pod 부족0 |
| 33개 배치(패키지+32seed) | 대부분 부족 | 77,629/77,629, 부족 배치0 |
| `BastionEncounters`·`MassSpawnPlanning` | — | 통과 |

## 3. 전체 회귀·패키징·1P

- 전체 빌드: Editor 증분 성공(37.29초·21.95초). 새 프로세스 전체 `LootNPop.SurfaceNavigation` **127검사**(기존126+신규1) 통과: 경고 없는123·경고 포함4(HTTP timeout3·기존 B-option1)·실패/미실행/진행 중0, 513.5초, exit0.
- 입력 에셋: production SurfaceData3·fixture3·`DefaultGame.ini` 실행 전후 SHA256 동일(`test-asset-restoration.json`). 테스트가 다시 저장한 `SurfaceNavigationTests/` 생성 에셋3개는 세션 시작 상태인 HEAD 바이트로 복구했다.
- 선행 게이트(전체 실패0 + production 최신성·SupportPrecision·ProductionSpawnComposition 성공)를 통과한 Development BuildCookRun exit0/104.6초, 1,229 packages. 출력 `Saved/SurfaceNavigationPhase9V009Package/Windows`, 게임 exe SHA256 `D79B9229B0173FC2EDA0AD920CA6788DA151ACCD7DDA0804064971FD179AA0ED`.
- V008과 같은 인자의 패키지1P: exit0/42.7초, timeout 없음.
  - Spawn: Bench01 540/540, Bench01_2 577/577, Bench01_3 308/308, Bench01_4 810/810, Pod120/120.
  - `ProbeSourceKeys` loadedSlots8·runtime358/saved358·duplicates/missing/unexpected0 **PASS**. `ProbePanels`11/실패0, `ProbeFaceIndex`2000방향·누락0, `ProbeSurfaceData` 8/8·358/358 모두 PASS.
  - generation1/8slot/358bindings 게시, UnknownHits0·EnvelopeEscapes0·LayerJumps0, assert/ensure/fatal/unhandled exception 표식0.
  - Error 로그는 기존 ShaderMap6·CharacterMovementComponent 추출1개뿐이다(V008과 동일).
  - headless frame P95 7.15ms·lock P95 0.075ms PASS, **exact P95 4.536ms는 harness 2ms 기준 FAIL**이다. 적 전량이 배치된 조건의 값이며 고정 해상도 성능 Gate를 대신하지 않는다.

패키징 뒤 `PlaceEnemies`의 후보 배열 임시 복사만 참조로 바꿨다(동작 동일). 재빌드 후 Spawn 관련5검사(`ProductionSpawnComposition` 77,629/77,629·`MassSpawnPlanning`·`MassSpawnHeadroom`·`BastionEncounters`·`HollowContent`)가 실패0(HTTP 경고1)으로 통과했다. 위 패키지 exe는 이 정리 직전 소스로 만든 것이다.

로컬 근거는 `Saved/SliceOctant/SpawnCompositionV009`(ReproReport·FixReport·FullEditorReport·package/1P 로그·스크립트)다.

## 4. 남은 범위

- 리슨2P·late join(host/guest 동일 snapshot·동적 요소 상태) 미실행. 사용자 진행 확인 뒤 다음 단위로 진행한다.
- exact P95 초과는 성능·메모리 단위에서 고정 해상도 패키지로 다룬다. 추가 관통 진단은 꺼진 상태라 관통 Gate 통과를 주장하지 않는다.
- NPC LoS·교전 재검증은 별도 세션 소유를 유지한다.
