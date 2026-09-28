# Phase 5 로그 — 런타임 로더와 SurfaceCache 교체

> 상태: 진행
> Phase 실행 문서: `../phases/Phase05_RuntimeLoader.md`

## 2026-09-28 — 착수 전 조사

### source key 런타임 안정성

- 비-Shipping 진단 명령 `LNP.SurfaceNav.ProbeSourceKeys`를 추가했다. runtime slot Level의 `LNP.Surface.Support` component를 `<Actor FName>.<Component FName>` 형식으로 집계한다.
- `Meadow_00` 8 slot에서 slot당 source 11개, 고유 key 11개, duplicate 0, slot 불일치 0이었다.
- 에디터 `-game` 리슨 호스트: PASS. 로그 `Saved/Logs/Phase05_SourceKeys_EditorGame.log`.
- Win64 Development package 리슨 호스트: PASS. 로그 `Saved/Logs/Phase05_SourceKeys_Package.log`.
- 런타임 11개 key를 `DA_OctantSurface_Meadow_00` 베이크 보고(`Saved/Logs/Auto4b_U4.log`)와 대조했고 전부 일치했다. OFPA UAID actor 이름은 Level Instance package 접미사와 무관하게 유지된다.
- 결론: Phase 5 face→Layer registry는 현재 source key를 계속 사용한다. snapshot 게시 전에 베이크 source의 missing·duplicate key를 오류로 차단한다.

### 게스트 SurfaceCache 소비자 분류

| 소비자 | 게스트 실행 | 의미 |
|:---|:---:|:---|
| `ALNPGameState::TryBeginClientBaking` | 예 | 옥탄트 로드와 서버 phase 투-게이트 후 로컬 runtime 베이크 시작 |
| `ALNPPlayerController` | 예 | 베이크 진행률·완료 delegate를 로딩 완료/Ready RPC 게이트로 사용 |
| `ULNPTrajectoryGuideComponent` | 예 | 반환된 점은 사용하지 않고 `GetSurfacePoint` 성공을 옥탄트 준비 신호로만 사용 |
| `ULNPEnemyMovementProcessor` | 아니오 | 클라이언트 world는 SurfaceCache 조회 전 즉시 return. 적 위치는 복제로 수신 |
| `FLNPEnemyIdleTask` | 아니오 | 엔진 Mass StateTree 실행 플래그가 `Server | Standalone` |
| `ULNPMassSpawnSubsystem` | 아니오 | GameMode의 서버 초기화 흐름에서만 `BeginSpawning` |
| `LNPExactOracle`·`LNPLoadBaseline` | 진단 | production gameplay 소비자가 아님 |

결론: 현재 게스트 gameplay는 Support point 값을 직접 필요로 하지 않는다. 그럼에도 Roadmap의 "클라이언트 로드"를 유지하면 서버·클라이언트 모두 같은 immutable Support snapshot을 게시하고 기존 로컬 베이크 완료 신호를 snapshot 게시 완료 신호로 바꾸는 구조가 가장 단순하다.

### 검증 및 부수 진단 보정

- `LootNPopEditor Win64 Development` 빌드 성공.
- Win64 Development BuildCookRun 성공(978 package cook, stage 성공). 기존 Lyra Mannequin Material Function 누락 경고만 재현.
- 부하 harness의 `ProbeFaceIndex`가 스프링 런처 `MeshComponent`를 한 번 맞혀 FaceIndex 누락으로 잘못 실패했다. 런처는 slot 밖의 서버 스폰 정적 장치이고 simple collision을 쓰므로 베이크 face 표 대상이 아니다. 검증 명령이 slot Level hit만 집계하도록 보정했다.
- 보정 후 에디터 `-game` 재검증은 `slotHits=1999`, `nonSlotHits=1`, `missingFaceIndex=0`으로 PASS했고 source key도 다시 PASS했다. 로그 `Saved/Logs/Phase05_SourceKeys_EditorGame_Filtered.log`.

## 2026-09-28 — Phase 5 범위 확정

- 사용자 결정으로 별도 Spawn stream을 Phase 5에 처음부터 포함한다(D-059). LVI에 레벨 디자이너가 Pod 세트 앵커를 배치하고 베이커가 authored anchor와 random candidate를 함께 저장한다.
- `DA_MassSpawnConfig`의 `PodSetCount`·`AssociatedEnemies.Count`가 총량의 단일 원본이다. marker는 총량을 늘리지 않는다.
- 할당 순서는 특정 `SpawnSetId` 지정점 → 일반 지정점 → 베이크된 random candidate다. 연관 적은 선택된 Pod와 같은 Layer의 반경 내 후보로 채운다.
- 소비자 경계는 지각 Layer 0 legacy adapter, Mass Spawn의 Spawn stream 직접 전환, 탄도 가이드의 명시적 준비 신호, 적 이동의 Phase 6 `QueryLayers` 전환으로 확정했다. production runtime bake 병행 경로는 두지 않는다.
- 실행 문서 `../phases/Phase05_RuntimeLoader.md`를 만들고 구현 단위 0~5와 완료 조건을 적었다. 다음은 production SurfaceData 연결과 공통 loader 골격이다.

## 2026-09-28 — 구현 단위 0 완료

- `ULNPSurfaceDataSubsystem`과 immutable `FLNPSurfaceDataSnapshot` 골격을 추가했다. 선택된 8개 SurfaceData를 unique path로 async load하고, 성공 generation만 release/acquire로 게시한다. 같은 asset의 Support decode는 한 번만 수행해 slot끼리 공유한다.
- 순수 `ValidateAndBuildSnapshot`은 definition/asset 수, `DataVersion`, Level↔SourceLevel manifest, descriptor size/content hash, Support decode·sample count·source key, 12개 world seam pair를 검증한다. 실패하면 부분 snapshot을 남기지 않는다.
- production 자동화의 첫 실행이 `/Game/Maps/OctantPoolData` definition 0의 빈 `SurfaceData`·`SeamSignature`를 잡았다. editor 명령 `LNP.SurfaceNav.LinkPoolSurfaceData`를 추가해 `DA_OctantSurface_Meadow_00`과 `DysonSphere_R300m_V1`을 연결·저장했다.
- 자동화 `Runtime.SurfaceDataLoaderValidation`은 정상 8 slot과 구버전·Level 불일치·payload hash 손상·seam 불일치를 검사한다. `Runtime.ProductionSurfaceDataDefinitions`은 실제 settings pool을 결정론적으로 8 slot 선택해 같은 validator를 통과시킨다.
- 최종 결과: 에디터 전체 빌드 성공, 두 자동화 2/2 성공. 로그 `Saved/Logs/Phase05_LoaderUnit0_Final.log`.
- 다음은 구현 단위 1: world Support query, runtime source binding, Layer 0 adapter와 준비 신호 전환.

## 2026-09-28 — 구현 단위 1 완료

- `FLNPSurfaceHandle`·`FLNPSurfaceQuery`·`FLNPSurfaceQueryResult`와 `ULNPSurfaceDataSubsystem::QuerySupport`를 추가했다. world 위치 방향을 slot의 inverse rotation으로 옮겨 `LNPSupportAtlas::QueryLayers`를 호출하고, 성공 결과에 `(slot, LocalLayerId, generation)`을 기록한다.
- Phase 6까지의 `GetSurfacePoint` adapter는 지각 Layer 0만 보간하며, 다층 snapshot에서 처음 호출될 때 비-Shipping 경고를 한 번 남긴다. 게시 전에는 query가 `NotReady`/false다.
- slot Level의 `LNP.Surface.Support` component key를 Atlas source 표와 비교한다. missing·duplicate·unexpected key면 전체 게시를 실패시키며, 정상 production은 11개/slot, 총 88개가 bind됐다.
- hit identity registry entry는 큰 face map을 복사하지 않고 immutable `FLNPSupportAtlas` shared pointer와 source index를 가진다. `ResolveHit`은 external `FaceIndex`로 `LocalLayerId`를 해석하고 `SurfaceDataGeneration`을 함께 반환한다. registry를 먼저 게시한 뒤 Support ready release store를 세운다.
- 탄도 가이드는 반환 point를 버리던 legacy 조회를 없애고 `SurfaceData::IsReady`를 직접 본다. PlayerController Ready delegate도 `OnSurfaceDataReady`로 옮겼다.
- 서버는 새 snapshot 게시 성공 뒤에만 기존 SurfaceCache runtime bake를 시작한다. 이 bake는 Mass spawn이 Spawn stream으로 전환되는 구현 단위 3까지의 임시 경로다.
- 중간 참여 게스트가 서버 phase `SurfaceBaking`을 건너뛰고 `Complete`를 먼저 받아도 로드를 놓치지 않도록 `ALNPGameState::OnRep_ServerPhase`를 `>= SurfaceBaking`으로 바꿨다. 실제 2P 게스트가 phase 3을 먼저 받은 뒤 snapshot을 게시하고 Ready를 완료했다.

### 검증

- `LootNPopEditor Win64 Development` 전체 빌드 성공.
- `LootNPop.SurfaceNavigation.Runtime` 자동화 2/2 성공. `SurfaceDataLoaderValidation`이 게시 전 `NotReady`, 8-slot inverse rotation query, source missing·duplicate, exact Layer/generation을 검사한다. 로그 `Saved/Logs/Phase05_LoaderUnit1_RuntimeTests.log`.
- `LootNPop.SurfaceNavigation.WorldCollision` 회귀 자동화 8/8 성공. API, 동적 마커 hit, earliest hit, Layer identity, Meadow 동굴 키트, projectile arc, fixture/map 회귀가 모두 통과했다. 로그 `Saved/Logs/Phase05_LoaderUnit1_WorldCollisionTests.log`.
- 에디터 `-game` 1P: `ProbeSurfaceData`가 `surfaceGeneration=1 registrySurfaceGeneration=1 slots=8 queries=8/8 bindings=88/88 -> PASS`. `ProbeFaceIndex`·`ProbeSourceKeys`와 부하 판정도 PASS. 로그 `Saved/Logs/Phase05_LoaderUnit1_EditorGame_Final.log`.
- 에디터 `-game` 리슨 2P: host와 guest 모두 같은 SurfaceData generation 1, query 8/8, binding 88/88로 PASS. 게스트 hit registry 자체 generation은 로컬 게시 이력 때문에 3, SurfaceData generation은 서버와 같은 1이다. 로그 `Saved/Logs/Phase05_LoaderUnit1_2P_Host.log`, `Phase05_LoaderUnit1_2P_Guest.log`.
- `-game`에서 experimental Toolsets Python bootstrap이 `ToolsetDefinition`·`AgentSkill` 미노출 오류를 남기고, 기존 Mass translator가 CharacterMovementComponent 추출 오류를 남겼다. `LogLootNPop` 오류·ensure·크래시는 없었고 Surface Navigation probe에는 영향이 없어 별도 환경/기존 문제로 분리했다.

다음은 구현 단위 2: spawn authoring actor, `SpawnSetId`, authored/random candidate baker와 Spawn codec이다.

## 2026-09-28 — 구현 단위 2 완료

### Authoring과 production 마이그레이션

- editor-only `ALNPMassSpawnPoint`를 추가했다. 최초 배치에는 안정 `SpawnPointId` GUID를 만들고 일반 복제·붙여넣기에는 새 GUID를 발급한다. `TargetSpawnSetId`는 선택 사항이며 GUID·세트 ID·transform 전체가 source semantic hash에 들어간다.
- `FLNPLootPodSpawnEntry.SpawnSetId`를 추가했다. `ULNPMassSpawnConfig::IsDataValid`이 빈 ID, 중복 ID, null Pod config, 음수 수량을 차단한다.
- `LNP.SurfaceNav.MigrateMassSpawnConfig`로 production `DA_MassSpawnConfig` 네 entry를 `Bench01`, `Bench01_2`, `Bench01_3`, `Bench01_4`로 마이그레이션했다.
- `LNP.SurfaceNav.PlaceRegressionSpawnAnchors`로 `LVI_Octant_Fixture_Regression`의 지각·부유섬·동굴에 authored anchor 3개를 배치했다.

### Spawn 베이크와 codec

- authored anchor는 가장 가까운 보간 가능 Support Layer로 투영하며 원래 forward를 접평면에 투영해 yaw를 보존한다. 공중 point는 투영 허용 거리를 넘으면, authored point끼리 Pod 최소 거리를 어기면 베이크를 실패시킨다.
- random candidate는 안전한 Support raster를 안정 stride로 샘플링해 안정 candidate index를 부여한다. slope·edge clearance와 Pod/Enemy 허용 비트를 저장한다.
- 베이크 전용 physics preview world에 exact blocker static mesh와 ISM instance를 복제했다. 후보 위치를 Support 법선 방향으로 capsule 반높이만큼 띄워 Pod(반지름 120cm·반높이 150cm)와 Enemy(반지름 50cm·반높이 100cm) overlap clearance를 검사한다. authored anchor는 Pod capsule clearance를 필수로 한다.
- `LNPSpawnData` codec v1은 authored anchor를 GUID, random candidate를 안정 index로 canonical 정렬해 little-endian payload를 만든다. decode 시 버전·reserved·범위·유한값·GUID/index 중복과 정렬·trailing bytes를 검사한다.
- loader가 Spawn descriptor와 content hash를 필수로 검증하고 payload를 decode한다. 같은 SurfaceData asset을 여러 slot이 사용하면 Support와 마찬가지로 immutable Spawn data를 한 번만 decode해 공유한다.
- `FLNPSurfaceBakeHeader::CurrentDataVersion=4`, `FLNPOctantSurfaceBaker::BakerSchemaVersion=3`으로 올렸다. 이전 Spawn 없는 SurfaceData는 runtime 호환 decode하지 않는다.

### 재베이크 결과

| SurfaceData | authored | random candidate | Spawn payload |
|:---|---:|---:|---:|
| `DA_OctantSurface_Fixture_Crust` | 0 | 10,573 | 380,640 B |
| `DA_OctantSurface_Fixture_Regression` | 3 | 10,616 | 382,368 B |
| `DA_OctantSurface_Meadow_00` | 0 | 10,538 | 379,380 B |

- capsule clearance 도입 전과 비교해 Regression의 blocker 인접 후보 1개, Meadow의 blocker 인접 후보 8개가 제거됐다.
- 최종 재베이크 로그는 `Saved/Logs/Phase05_SpawnBake_Clearance.log`다.

### 검증

- `LootNPopEditor Win64 Development` 전체 빌드 성공.
- `LootNPop.SurfaceNavigation.Bake.SpawnCodec`과 `Bake.SpawnAuthoringValidation` 2/2 성공. 후자는 unknown `SpawnSetId`, 공중 point, 중복 GUID, blocker capsule clearance 부족을 각각 실패시키는지 검사한다. 로그 `Saved/Logs/Phase05_SpawnTests.log`.
- `Bake.OctantBakeDeterministic` 1/1 성공. 저장본과 재베이크 Support/Spawn payload가 일치하고 회귀 LVI의 authored anchor 3개가 기대 Layer로 해석되는지 검사한다. 로그 `Saved/Logs/Phase05_DeterministicTest.log`.
- `Runtime.ProductionSurfaceDataDefinitions`와 `Runtime.SurfaceDataLoaderValidation` 2/2 성공. production `DataVersion=4` asset과 Spawn payload decode·공유가 유효하다. 로그 `Saved/Logs/Phase05_RuntimeTests.log`.
- `git diff --check`는 오류 없이 통과했다. 표시된 내용은 기존 checkout의 LF→CRLF 변환 경고뿐이다.

다음은 구현 단위 3: Mass spawn이 8-slot Spawn snapshot을 직접 소비하도록 바꾸고 authored 우선 Pod·enemy 후보 할당과 shortfall 통계를 구현한다.
