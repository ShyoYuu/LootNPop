# Phase 5 — 런타임 로더와 SurfaceCache 교체

> 상태: 완료(2026-09-28)
> 예상 범위: 3~4세션
> 선행 조건: Phase 4b 완료, source key·게스트 소비자 사전 조사 완료(`../history/Phase05_Log.md`)

## 1. 목표

1. 선택된 8개 `ULNPOctantSurfaceData`를 서버와 클라이언트가 로드·검증·decode하고, slot transform과 함께 immutable world snapshot으로 게시한다.
2. 정상 실행의 머신별 SurfaceCache trace bake를 제거하고, 기존 소비자는 지각 Layer 0 legacy adapter로 이어 Phase 6까지 동작시킨다.
3. Support source key와 face 표를 runtime component에 연결해 exact hit가 `(slot, LocalLayerId)`로 해석되게 한다.
4. LVI에 레벨 디자이너가 지정한 Mass spawn point와 절차 후보를 별도 Spawn stream으로 베이크한다.
5. `DA_MassSpawnConfig`의 총량을 유지하면서 지정점을 먼저 채우고, 남은 수량만 베이크된 절차 후보로 채운다.
6. stale SurfaceData를 cook·CI에서 차단하고, 에디터 `-game`·Development package·2P에서 같은 초기화 결과를 확인한다.

## 2. 현재 코드의 출발점

- `FLNPOctantDefinition`에는 `LevelAsset`과 `SurfaceData`가 있으나 production definition의 연결과 runtime loader가 아직 없다.
- `ULNPOctantSurfaceData`는 Support·Navigation·Traversal·Spawn 네 payload와 descriptor를 이미 가진다. 현재 Support만 채워지고 `DataVersion`은 3이다.
- `ULNPSurfaceCacheSubsystem`은 약 123만 runtime trace 뒤 snapshot을 게시한다. `ULNPMassSpawnSubsystem`은 그 snapshot의 첫 hit로 Pod와 연관 적 위치를 만든다.
- `ULNPMassSpawnConfig`는 `LootPodSpawnSets[]`의 `PodSetCount`와 `AssociatedEnemies[].Count`를 총량의 단일 원본으로 쓴다.
- source key `<Actor FName>.<Component FName>`은 runtime Level Instance 8 slot에서 베이크 key와 같음이 에디터 `-game`과 Development package에서 확인됐다.
- 게스트 production gameplay는 현재 Support point를 직접 소비하지 않지만, 로컬 베이크 완료 delegate를 로딩 완료·Ready RPC 게이트로 쓴다.

## 3. 확정 결정

### 3.1 서버·클라이언트가 같은 immutable snapshot을 게시한다

- 양쪽 모두 선택된 definition의 SurfaceData를 로드한다. 게스트에서 현재 point 값 소비가 없다는 이유로 payload를 생략하지 않는다.
- 기존 `SurfaceBaking` 초기화 phase 이름은 첫 전환에서 유지하되, 내부 의미를 `SurfaceData load → validation → snapshot publish`로 바꾼다.
- 로컬 bake 진행률·완료 delegate는 snapshot 로드/게시 진행률·완료 신호로 교체한다.
- payload UObject와 decoded buffer는 snapshot보다 오래 strong reference로 유지한다. 같은 SurfaceData가 여러 slot에 쓰이면 decoded payload는 공유하고 slot transform만 따로 둔다.
- loader 오류는 빈 지면으로 계속 진행하지 않는다. 준비 상태를 실패로 끝내고 match 진입을 차단한다.

### 3.2 Phase 5 소비자 전환 경계

- `GetSurfacePoint(Direction)` 호환 adapter는 새 snapshot의 **지각 Layer 0만** 조회한다. 다층 위치에서 새 코드가 이 API를 쓰면 비-Shipping 경고를 남긴다.
- 적 grounded 이동·Idle의 `QueryLayers` 전환과 exact 폴백 결합은 Phase 6에 남긴다.
- 탄도 가이드는 반환 point를 사용하지 않으므로 Surface query 대신 옥탄트/snapshot 준비 상태를 직접 본다.
- Mass spawn은 Phase 5에서 Spawn stream으로 직접 전환한다. legacy adapter를 거치지 않는다.
- 정상 초기화에서는 기존 runtime SurfaceCache bake를 병행하지 않는다. 비교가 필요하면 테스트·진단 명령에서만 새 snapshot과 exact 결과를 비교한다.

### 3.3 LVI 수동 spawn point는 Pod 세트 앵커다(사용자 결정, 2026-09-28, D-059)

- 새 editor authoring actor `ALNPMassSpawnPoint`를 LVI에 배치한다. cooked runtime은 이 actor를 스캔하지 않고 베이크된 Spawn stream만 읽는다.
- point는 다음 최소 필드를 가진다.
  - `SpawnPointId: FGuid`: 배치 시 발급, 복제·붙여넣기는 새 ID. 결정론 정렬과 진단에 사용한다.
  - `TargetSpawnSetId: FName`: 특정 Pod 세트 지정. `None`이면 어느 세트나 사용할 수 있는 일반 앵커다.
  - actor transform: 위치는 Pod의 발밑 접점, 회전은 표면 Up에 투영한 authored yaw를 뜻한다.
- `FLNPLootPodSpawnEntry`에는 배열 인덱스 대신 쓸 필수 `SpawnSetId: FName`을 추가한다. 한 config 안에서 비어 있거나 중복이면 validation 오류다.
- marker는 **개수를 늘리지 않는다**. 총 Pod 수는 계속 `PodSetCount`, 연관 적 수는 계속 `AssociatedEnemies[].Count`가 정한다.
- Phase 5의 수동 point는 개별 연관 적 socket이 아니다. 연관 적은 선택된 Pod와 같은 Support Layer에서 `EnemySpawnRadiusAroundPod` 안의 베이크 후보를 고른다.

### 3.4 지정점 우선, 절차 후보 보충

각 `SpawnSetId`의 요청 수는 density 보정과 무관한 `PodSetCount`다. 8 slot의 Spawn stream을 월드 후보로 조립한 뒤 다음 순서로 할당한다.

1. 해당 `SpawnSetId`를 명시한 authored anchor를 먼저 사용한다.
2. 남은 수량에 `TargetSpawnSetId=None`인 일반 authored anchor를 사용한다.
3. 그래도 남은 수량만 random candidate pool에서 seed 기반 결정론적 순서로 고른다.

세트와 point는 각각 `SpawnSetId`, `(slot, SpawnPointId)`로 안정 정렬한 뒤 world seed로 shuffle한다. config 배열 순서, async task 완료 순서, UObject 경로에 결과를 의존시키지 않는다.

- 지정점이 요청 수보다 많으면 결정론적으로 일부만 사용하고 unused 수를 보고한다.
- 특정 세트 지정점이 config에 없는 ID를 가리키면 cook validation 오류다.
- 지정점은 random 후보보다 우선하므로 런타임 거리 검사로 조용히 버리지 않는다. authored point끼리 `MinDistanceBetweenPods`를 위반하거나 clearance가 부족하면 bake/cook 오류로 제작자에게 돌려보낸다.
- random 후보는 이미 사용한 모든 authored anchor와 Pod 최소 거리를 지킨다. 후보가 부족하면 trace로 임의 위치를 만들지 않고 요청·배치·부족 수를 오류 로그와 검증 결과에 남긴다.
- `AssociatedEnemies`는 선택된 Pod와 같은 `LocalLayerId`, 반경 안, enemy 허용·clearance 조건을 만족하는 후보를 고른다. 부족하면 해당 entry만 적게 생성하고 shortfall을 보고한다.
- 비행 적도 ground candidate를 먼저 고른 뒤 기존 `LiftFlyingSpawn` 규약으로 고도를 적용한다.

### 3.5 Spawn stream 형식과 stale 경계

Spawn stream은 두 레코드 집합을 저장한다.

| 레코드 | 필드 |
|:---|:---|
| Authored anchor | `SpawnPointId`, 선택적 `TargetSpawnSetId`, 옥탄트 로컬 transform, `LocalLayerId`, clearance |
| Random candidate | 안정 candidate index, 옥탄트 로컬 위치·법선, `LocalLayerId`, slope·edge·capsule clearance, Pod/Enemy 허용 비트 |

- random candidate는 Support Layer의 walkable·보간 가능 영역에서 결정론적으로 만든다. runtime scene trace로 보충하지 않는다.
- 위치는 Support payload와 독립 decode할 수 있게 Spawn stream에 저장하되, 베이크 검증에서 같은 Layer의 Support/exact 표면과 허용 오차 안인지 확인한다.
- `SpawnPointId`, marker transform, `TargetSpawnSetId`, 후보 생성 설정을 semantic/settings hash 입력에 포함한다.
- Spawn codec 도입으로 `FLNPSurfaceBakeHeader::CurrentDataVersion`을 4, `BakerSchemaVersion`을 3으로 올리고 production SurfaceData를 전부 다시 굽는다. 이전 codec의 runtime 호환 decode는 두지 않는다.

### 3.6 runtime 검증과 게시 원자성

snapshot 게시 전에 한 번에 다음을 검사한다.

- 8 slot 모두 definition·Level·SurfaceData가 있고 `DataVersion`이 현재 값임
- 지각 seam hash가 D-043 조합 규약을 만족함
- Support/Spawn descriptor와 content hash, decode 범위가 유효함
- 각 slot의 runtime Support component key가 source 표와 missing·duplicate 없이 정확히 대응함
- SpawnSetId 참조, authored anchor 간 거리, world 후보 shortfall 진단이 유효함

하나라도 실패하면 Support snapshot과 hit identity registry를 부분 게시하지 않는다. 완성된 한 generation을 release store로 동시에 공개하고 worker는 acquire로 읽는다.

## 4. 구현 단위

### 구현 단위 0 — schema·production definition·loader 골격

- production `FLNPOctantDefinition.SurfaceData` 연결.
- SurfaceData async load, `DataVersion`·Level 짝·descriptor·seam hash 검증.
- 서버·클라이언트 공통 준비 상태와 실패 상태, 로딩 진행률.
- 같은 asset decode 공유와 8 slot transform snapshot 골격.
- 자동화: 누락 asset, 구버전, 잘못된 Level, seam 불일치, 중복 asset 사용.
- 상태(2026-09-28): 완료.
  - runtime `ULNPSurfaceDataSubsystem`: 선택된 SurfaceData 비동기 load, 명시적 `NotStarted/Loading/Ready/Failed`, 실패 원인, release/acquire snapshot 게시 골격을 추가했다.
  - 순수 validator가 8 slot 수, non-null Level/SurfaceData, `DataVersion`, SourceLevel manifest 짝, 네 descriptor 크기·content hash, Support codec v2 decode·element count·source key, 12개 지각 seam pair를 게시 전에 검사한다.
  - 같은 SurfaceData asset을 여러 slot이 쓰면 Support decode 결과를 shared pointer 하나로 재사용하고 slot rotation만 분리한다.
  - production `/Game/Maps/OctantPoolData` definition 0을 `/Game/Maps/Meadow_00/DA_OctantSurface_Meadow_00`에 연결하고 seam signature를 `DysonSphere_R300m_V1`로 저장했다.
  - editor 명령 `LNP.SurfaceNav.LinkPoolSurfaceData [SeamSignature]`는 각 definition Level 옆의 표준 이름 SurfaceData를 연결·저장한다.
  - 자동화 `Runtime.SurfaceDataLoaderValidation`·`Runtime.ProductionSurfaceDataDefinitions` 2/2 통과(`Saved/Logs/Phase05_LoaderUnit0_Final.log`). 에디터 전체 빌드 성공.

### 구현 단위 1 — Support snapshot·query·hit registry

- Support codec v2 decode 결과를 immutable snapshot에 연결하고 slot inverse rotation query 구현.
- source key로 runtime component를 face 표에 bind해 `(slot, LocalLayerId)`를 registry에 게시.
- Layer 0 legacy `GetSurfacePoint` adapter와 다층 사용 경고.
- 탄도 가이드와 클라이언트 Ready 게이트를 snapshot 준비 신호로 전환.
- 자동화·에디터 `-game`에서 8 slot query, source missing/duplicate, 게시 전 `NotReady`, 게시 뒤 generation 일치.
- 상태(2026-09-28): 완료.
  - `FLNPSurfaceHandle`·`FLNPSurfaceQuery`·`FLNPSurfaceQueryResult`와 `QuerySupport`를 추가했다. world 방향은 slot의 inverse rotation으로 옥탄트 로컬 방향에 옮기며 결과 handle에 `(slot, LocalLayerId, generation)`을 기록한다.
  - runtime `LNP.Surface.Support` key를 slot별 Atlas source 표와 정확히 대조하고 missing·duplicate·unexpected key를 게시 전에 막는다. hit registry entry는 큰 face 표를 복사하지 않고 immutable Atlas shared pointer와 source index를 보유한다.
  - exact hit 결과가 `LocalLayerId`와 `SurfaceDataGeneration`을 반환한다. Support ready release store보다 registry 게시를 먼저 수행해 ready reader가 부분 generation을 보지 않게 했다.
  - Layer 0 `GetSurfacePoint` adapter, 탄도 가이드 준비 신호, PlayerController Ready 신호를 새 subsystem으로 옮겼다. 서버는 구현 단위 3까지 Mass spawn용 legacy runtime bake를 snapshot 게시 뒤에만 시작한다.
  - 중간 참여 게스트가 `SurfaceBaking` 복제 단계를 건너뛰고 `Complete`를 먼저 받아도 로드를 시작하도록 `OnRep_ServerPhase` 조건을 `>= SurfaceBaking`으로 고쳤다.
  - 자동화 2/2, 전체 에디터 빌드, 에디터 `-game` 1P·2P에서 host/guest 모두 `queries=8/8`, `bindings=88/88`, SurfaceData/registry generation 일치로 통과했다. 로그는 `../history/Phase05_Log.md`에 기록했다.

### 구현 단위 2 — Spawn authoring·baker·codec

- `ALNPMassSpawnPoint`, GUID 수명주기, `TargetSpawnSetId` editor validation.
- `FLNPLootPodSpawnEntry.SpawnSetId` 추가와 `DA_MassSpawnConfig` 마이그레이션.
- authored anchor 투영·Layer 해석·yaw 보존, random candidate·clearance 생성.
- Spawn codec 순수 encode/decode와 결정론 테스트. `DataVersion=4`, `BakerSchemaVersion=3`, 세 SurfaceData 재베이크.
- fixture: 지각·섬·동굴 authored anchor, 잘못된 공중 point, 중복 GUID, unknown SpawnSetId, clearance 부족.
- 상태(2026-09-28): 완료.
  - editor-only `ALNPMassSpawnPoint`를 추가했다. 생성 시 GUID를 발급하고 일반 복제·붙여넣기에는 새 GUID를 주며, source semantic hash에 GUID·세트 ID·transform을 포함한다.
  - production `DA_MassSpawnConfig`의 네 entry를 안정 `SpawnSetId`로 마이그레이션하고 config validation에서 빈 ID·중복 ID·잘못된 수량을 차단한다.
  - 베이커가 authored anchor를 같은 방향의 가장 가까운 보간 가능 Support Layer로 투영하고 접평면 yaw를 보존한다. random candidate는 안전한 Support raster에서 안정 index 순서로 만들며 Pod·Enemy capsule clearance를 bake-only physics preview world에서 검사한다.
  - Spawn codec v1은 authored/random 레코드를 canonical 정렬해 little-endian payload로 저장한다. 로더는 descriptor·content hash·decode·element count를 검사하고 같은 SurfaceData를 쓰는 slot끼리 decode 결과를 공유한다.
  - 지각·섬·동굴 anchor 3개를 회귀 LVI에 배치했다. unknown `SpawnSetId`, 공중 point, 중복 GUID, blocker clearance 부족을 자동화에서 모두 차단했다.
  - `DataVersion=4`, `BakerSchemaVersion=3`으로 올리고 production/fixture SurfaceData 3개를 다시 구웠다. 마지막 Spawn 레코드는 Crust 10,573개, Regression 10,619개(3 authored + 10,616 random), Meadow 10,538개다.
  - 전체 에디터 빌드, Spawn 자동화 2/2, 결정론 베이크 1/1, runtime loader 2/2가 통과했다. 세부 로그와 payload 크기는 `../history/Phase05_Log.md`에 기록했다.

### 구현 단위 3 — Mass spawn 전환

- `ULNPMassSpawnSubsystem`이 SurfaceCache snapshot 대신 world Spawn snapshot을 받는다.
- 세트 지정 anchor → 일반 anchor → random candidate 순으로 Pod 요청을 만든다.
- 같은 Layer·반경의 enemy 후보를 연관 적에 배정하고 `SurfaceHandle`을 초기화한다.
- 기존 `MaxSpawnsPerFrame`, Pod link, PodID, density CVar, 부하 harness 경로는 유지한다.
- 할당 통계: 세트별 requested/authored/generic/random/shortfall, enemy requested/placed.
- 상태(2026-09-28): 완료.
  - `FLNPMassSpawnPlan` 순수 계획 경계를 추가했다. 8-slot Spawn snapshot을 안정 정렬한 뒤 world seed로 shuffle하고, 특정 세트 authored → 일반 authored → random 순서로 Pod를 배정한다.
  - marker 수는 `PodSetCount`를 늘리지 않는다. random Pod는 기존 Pod 최소 거리를 지키며, 연관 적은 같은 slot·`LocalLayerId`·반경 안의 단일 사용 후보만 고른다.
  - Pod·Enemy fragment가 초기 `FLNPSurfaceHandle`을 보유한다. 기존 per-frame queue, Pod link, PodID, density와 부하 harness는 유지했다.
  - `Runtime.MassSpawnPlanning` 자동화와 에디터 `-game`에서 production 네 세트 74+20+16+10=120개가 모두 배치되고 shortfall 0임을 확인했다.

### 구현 단위 4 — runtime bake 제거와 초기화 종단 전환

- GameState·PlayerController의 bake 시작/진행/완료 의미를 load/publish로 교체.
- production `BeginBaking` 호출과 123만 runtime trace 경로 제거. Phase 6까지 필요한 adapter facade만 유지.
- 서버 초기화 순서: snapshot 게시 → 동적 요소 스폰 → Mass spawn → Complete.
- 클라이언트 초기화 순서: snapshot 게시 → 복제 동적 요소 준비 → Ready RPC.
- 상태(2026-09-28): 완료.
  - `ALNPGameMode::OnSurfaceDataReady()`가 legacy bake 없이 바로 `BeginEntitySpawning()`을 호출한다. production 초기화에서 `ULNPSurfaceCacheSubsystem::BeginBaking()`과 `OnBakingComplete`를 제거했다.
  - 적 이동·Idle StateTree·부하 baseline projectile·exact oracle의 Layer 0 호환 조회를 `ULNPSurfaceDataSubsystem::GetSurfacePoint()`로 옮겼다.
  - `ULNPSurfaceCacheSubsystem` 구현은 과거 진단 코드로 남아 있지만 production 호출자는 없다. 패키지 로그에도 legacy bake 시작·완료가 없다.

### 구현 단위 5 — cook validation·패키지·2P 종료 검증

- cook/CI에서 source manifest stale, marker hash, SpawnSetId, codec version을 차단 오류로 검사.
- 자동화 전체, 에디터 빌드, Development BuildCookRun.
- 에디터 `-game`과 Development package에서 source key·Support query·Spawn allocation 결과 비교.
- 2P에서 서버·게스트 snapshot generation과 Ready 순서, Mass 복제 무회귀 확인.
- 기존 약 7초 bake 구간이 load·decode·publish로 대체된 시간, payload 크기, decode peak/resident memory 기록.
- 상태(2026-09-28): 완료.
  - `Bake.OctantBakeDeterministic`를 CI의 cook 전 freshness gate로 사용한다. source manifest·semantic/settings hash, marker, Spawn payload가 저장본과 다르면 실패한다. 런타임은 게시 전에 codec/DataVersion·descriptor/hash·seam·source key·SpawnSetId를 다시 검증한다. plain BuildCookRun 자체에 별도 validator를 추가한 것은 아니다.
  - 전체 자동화 59/59, `LootNPopEditor Win64 Development`, Win64 Development BuildCookRun(972 packages), 패키지 1P와 리슨 2P host/guest가 통과했다.
  - 베이크 clearance preview가 비동기 Static Mesh 컴파일 완료 여부에 따라 Meadow PCG blocker를 다르게 보던 순서 의존성을 제거했다. transient/editor-only 입력을 제외하고 mesh compile을 완료한 뒤 physics probe를 만들며, Meadow Spawn은 9,990 candidates·359,652 B로 다시 구웠다.
  - 패키지 1P: load→validation→decode→binding→publish 17.85ms, 고유 serialized payload 2.86MiB, 고유 decoded snapshot resident 2.97MiB. 프로세스 물리 메모리는 488.35→501.97MiB, 실행 전역 peak 501.97MiB였다.
  - 리슨 2P: host 15.89ms, guest 14.44ms. 양쪽 모두 SurfaceData generation 1, query 8/8, bindings 88/88였다. `processPhysicalPeak`은 decode 구간 전용 peak가 아니라 해당 프로세스 실행 전역 상한이다.

## 5. 완료 조건

- [x] 서버와 클라이언트가 같은 8-slot immutable Support/Spawn snapshot을 게시한다.
- [x] 정상 실행에 SurfaceCache runtime bake trace가 없고, publish 전 query는 `NotReady`다.
- [x] runtime source key 11개가 저장된 source 표와 정확히 bind되고 exact face가 올바른 Layer를 반환한다.
- [x] Layer 0 legacy adapter가 기존 소비자를 유지하며 다층 신규 사용을 경고한다.
- [x] LVI authored spawn point가 같은 seed에서 random 후보보다 먼저 채워진다.
- [x] `PodSetCount`·`AssociatedEnemies.Count`가 총량의 단일 원본이고 marker 수가 총량을 늘리지 않는다.
- [x] 지정점 부족분만 random candidate로 채워지며, Pod와 연관 적 shortfall이 측정·보고된다.
- [x] 지각·섬·동굴 marker와 random 후보가 올바른 `LocalLayerId`·clearance를 가진다.
- [x] stale·구버전·seam 불일치·unknown SpawnSetId·중복 key/ID가 cook 또는 게시 전에 차단된다.
- [x] 자동화, 에디터 빌드, Development package, `-game` 리슨 2P가 통과한다.
- [x] load/decode/publish 시간과 메모리 수치가 기록된다.
