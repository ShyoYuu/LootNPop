# Phase 2 — Octant Definition과 베이크 스키마 기록

> 상태: 완료
> 실행 문서: `../phases/Phase02_OctantDataSchema.md`

## 2026-09-23 — 최소 schema와 직렬화 기준선

### 구현

- `FLNPContentHash`를 추가해 `FIoHash`의 BLAKE3-160 20바이트를 reflected property로 손실 없이 저장했다.
- source package manifest를 source LVI, external actor, Terrain Contract mesh의 세 종류로 분리했다.
- `FLNPSurfaceBakeHeader`가 data version, source aggregate/semantic/settings hash, manifest, stream descriptor를 소유하도록 했다.
- `ULNPOctantSurfaceData`를 작은 header와 Support·Navigation·Traversal·Spawn 네 payload stream으로 나눴다.
- `FLNPOctantDefinition`에 Level asset, SurfaceData, 8-slot 허용 mask, seam signature를 추가했다.
- `ULNPOctantPoolData`에는 기존 `OctantPool`을 유지하면서 `OctantDefinitions`를 추가했다. 아직 runtime 선택 경로와 production asset은 전환하지 않았다.

### 검증

- Live Coding: 성공. 새 reflected 타입과 자동화 테스트가 Editor에 등록됐다.
- `LootNPop.SurfaceNavigation.Schema.SurfaceDataSerializationRoundTrip`: 성공, 1/1, 오류·경고 없음.
- 자동화는 20바이트 hash 변환, header, manifest, 네 payload, definition soft reference, slot mask와 seam signature의 UObject memory archive 왕복을 검증한다.

### 남은 검증

- reflected type 변경이므로 Editor를 닫은 상태의 `LootNPopEditor Win64 Development` 전체 빌드가 필요하다.
- 실제 package save/reload와 cook 검증은 최소 content asset을 만든 뒤 수행한다.

### 다음 작업

1. 기존 `OctantPoolData` content를 `OctantDefinitions`로 마이그레이션한다.
2. `ULNPOctantSpawnSubsystem`이 같은 seed에서 definition 전체를 선택·보존하도록 전환한다.
3. 허용 slot 제약을 만족하는 결정론적 선택 테스트를 추가한다.

## 2026-09-23 — 기존 Pool 최소 전환

### content 마이그레이션

- `/Game/Maps/OctantPoolData`의 legacy Level 1개를 같은 순서의 `OctantDefinitions` 항목으로 저장했다.
- 새 definition은 기존 Meadow LVI를 참조하고, 아직 SurfaceData asset이 없으므로 해당 참조와 seam signature는 비워 뒀으며 모든 8개 slot을 허용한다.
- legacy `OctantPool`은 같은 Level 1개를 그대로 유지했다. 에디터 재시작 뒤 두 목록이 모두 보존됨을 확인했다.

### runtime 전환

- `BuildEffectiveDefinitions`는 새 목록을 우선 사용하고, 새 목록이 비었을 때만 legacy Level 목록을 임시 definition으로 승격한다. 두 목록은 합치지 않는다.
- `SelectOctantDefinitions`는 slot 순서, pool index 순서와 `FRandomStream`만 사용해 결정론적으로 8개 definition을 선택한다.
- 각 slot에서 `AllowedSlotRotations`와 비어 있지 않은 Level reference를 검사한다. 현재 batch에서 가능한 중복을 피하고 후보를 소진하면 새 batch를 시작한다.
- 비어 있는 pool이나 특정 slot에 배치할 수 없는 pool은 부분 결과를 폐기하고 원인이 포함된 오류를 반환한다.
- `ULNPOctantSpawnSubsystem`은 Level만 추출하지 않고 선택된 definition 전체를 8개 slot 순서로 보존한다. Level Instance 로드 완료 시에도 이 배열은 유지한다.
- GameState가 없는 환경의 seed 0도 임의값으로 바꾸지 않고 유효한 결정론적 seed로 사용한다.
- 8개 actor를 모두 생성하지 못하면 이미 생성한 actor와 선택 결과를 정리하고 생성 상태를 종료한다.

### 자동화와 통합 검증

- `LootNPopEditor Win64 Development`: 에디터 종료 상태의 전체 빌드 성공.
- `LootNPop.SurfaceNavigation.Schema.SurfaceDataSerializationRoundTrip`: 성공, 오류·경고 없음.
- `LootNPop.SurfaceNavigation.Schema.DeterministicOctantSelection`: 성공, 오류·경고 없음.
- 결정론 테스트는 legacy 승격의 순서·기본 mask·빈 SurfaceData, 같은 seed의 서버·클라이언트 source index 일치, definition 전체 필드 보존, slot mask 준수, 빈 pool과 slot 7 배치 불가 오류를 검증한다.
- PIE에서 production `OctantPoolData`를 사용해 8개 Level Instance 생성과 visible load 완료, 완료 이벤트 발행을 확인했다. legacy fallback 경고는 발생하지 않았다.

### 다음 작업

1. source LVI, 직접 external package, Terrain Contract mesh의 dependency 수집기를 구현한다.
2. manifest 정렬·중복 제거와 semantic/settings/source aggregate hash의 입력 순서 독립성을 자동화한다.
3. 최소 SurfaceData asset의 실제 package save/reload와 cook 검증을 추가한다.

## 2026-09-23 — source manifest와 canonical hash 기준선

### dependency 수집

- `FLNPOctantSourceCollector`를 Editor 모듈에 추가했다.
- source LVI package와 그 package의 직접 dependency 중 `ULevel::GetExternalObjectsPaths()`가 반환한 소유 경로 아래 external actor/object package만 수집한다. dependency를 재귀 순회하지 않는다.
- 현재 3-kind schema를 유지해 `ExternalActor` kind가 `__ExternalActors__`와 `__ExternalObjects__` package를 함께 나타내도록 했다.
- `Support` 또는 `Blocker` 역할 태그와 정확히 하나의 수명주기 태그를 가진 `UStaticMeshComponent`만 Terrain source로 수집한다.
- 역할 component의 Static Mesh package만 `TerrainMesh`로 추가하고 decoration, 무태그 geometry, material, PCG dependency는 제외한다.
- 역할과 `Decoration` 혼용, 수명주기 태그 누락·중복, Static Mesh 없는 역할 component, package saved hash 누락은 명시적 오류다.

### canonical hash

- manifest는 package name, kind, saved hash 순으로 정렬하고 완전히 같은 행만 중복 제거한다. 같은 package name·kind의 saved hash 충돌은 오류다.
- `SourceSemanticHash`는 Terrain Contract tag, source-level component transform, collision profile, Static Mesh package 연결을 component별로 직렬화하고 row byte 순으로 정렬한다.
- UTF-8 문자열, little-endian 정수·실수 bit, `-0` 정규화, quaternion normalize·부호 정규화를 사용하며 유한하지 않은 transform은 거부한다.
- `BakeSettingsHash`는 baker schema version과 이름·typed value 설정을 정렬한다. 같은 설정의 중복은 제거하고 이름이 같은 충돌 값은 거부한다.
- `SourceContentHash`는 domain-separated canonical manifest와 `SourceSemanticHash`, `BakeSettingsHash`를 집계한다.

### 자동화와 실제 package 검증

- 사용자 실행 `LootNPopEditor Win64 Development` 전체 빌드 뒤 Editor 재시작에 성공했고 새 테스트가 등록됐다.
- `LootNPop.SurfaceNavigation.Schema.SourceDependencyCollection`: 성공. 실제 C-option LVI의 source·직접 external package와 transient Terrain Contract fixture의 역할 mesh만 수집하고 decoration·무태그 mesh를 제외했다.
- `LootNPop.SurfaceNavigation.Schema.SourceManifestAndHashDeterminism`: 성공. manifest/component/tag/settings 입력 순서와 동일 중복에 무관한 결과, quaternion 부호 동치, semantic·schema 변경 감지를 검증했다.
- `LootNPop.SurfaceNavigation.Schema.SurfaceDataPackageSaveReload`: 성공. `/Game/SurfaceNavigationTests/Schema/DA_MinimalOctantSurfaceData`를 실제 저장하고 package reload·object path 재로드 후 header, manifest와 네 payload를 검증했다.
- 기존 직렬화 왕복과 결정론 선택을 포함한 `LootNPop.SurfaceNavigation.Schema` 전체 5/5가 오류·경고 없이 통과했다.

### 다음 작업

1. production BP crust component의 Terrain Contract tag·collision profile을 마이그레이션한다.
2. production source에서 실제 manifest/hash를 생성해 SurfaceData header에 기록한다.
3. 최소 SurfaceData cook과 production definition 연결을 검증한다.

## 2026-09-23 — Windows cook 검증과 Phase 2 종료

### 범위 정리

- Phase 문서는 production Terrain Contract tag 마이그레이션을 제외 범위로 두지만, 당시 `Current.md`와 이전 로그의 다음 작업만 이를 Phase 2 잔여 항목으로 적고 있었다.
- 별도 확정 결정이나 Phase 게이트 근거가 없으므로 Phase 문서의 제외 범위를 유지했다.
- production tag·collision profile 마이그레이션은 실제 베이커를 production source에 적용하는 Phase 4 이후, production definition의 SurfaceData 연결과 runtime 로드는 Phase 5로 이관했다.
- 기존 `OctantPool`은 호환성 검증 기준선으로 계속 유지했다.

### cook 검증 보강

- 최소 asset의 Navigation·Traversal·Spawn descriptor에도 element count, uncompressed size와 content hash sentinel을 기록하도록 package save/reload fixture를 보강했다.
- 게임 모듈에 `LootNPop.SurfaceNavigation.PackagedRuntime.MinimalSurfaceDataCookedLoad`를 추가했다.
- 테스트는 cooked asset을 object path로 로드하고 data version, manifest, source content hash, 네 descriptor와 네 payload byte stream을 직접 비교한다.

### 최종 검증

- `LootNPopEditor Win64 Development`: 성공, 종료 코드 0.
- `LootNPop Win64 Development`: 성공, 종료 코드 0.
- `LootNPop.SurfaceNavigation.Schema`: 5/5 성공, 오류·경고 없음.
- Windows 단일-package cook: 성공, 종료 코드 0. cooked `DA_MinimalOctantSurfaceData.uasset` 1,721바이트와 `.uexp` 6,830바이트를 생성했고 `ReferencedSet.txt`에 package가 포함됐다.
- Win64 Development BuildCookRun: 성공, 종료 코드 0. 953 package를 cook하고 `Saved/SurfaceNavigationPhase2Package/Windows` archive를 생성했다.
- 기존 Lyra Mannequin Material Function 누락과 CVar 우선순위 경고만 재현됐으며 SurfaceData cook 오류는 없었다.
- D3D12 SM6 packaged runtime `MinimalSurfaceDataCookedLoad`: 1/1 성공, 오류·경고 없음. 보고서는 `Saved/SurfaceNavigationPhase2Reports/index.json`에 저장했다.
- cooked runtime에서 header, manifest, Support·Navigation·Traversal·Spawn descriptor와 네 non-empty sentinel payload의 보존을 확인했다.

### Phase 3 인계 판단

- Phase 3 완료 조건의 의미는 runtime 공개 schema/header를 컴파일 시점에 참조하고 최소 cooked asset을 로드할 수 있다는 것이다.
- 네 payload의 sentinel은 직렬화 경계 검증용이며 실제 Support Atlas 데이터가 아니다.
- 실제 Support Atlas rasterization을 Phase 2로 앞당기지 않고 Phase 4 범위로 유지한다.

### 결론

Phase 2 완료 조건을 모두 충족했다. 다음 세션은 `design/RuntimeCollision.md`, `design/TerrainContract.md`, `design/RegressionMap.md`를 기준으로 Phase 3 실행 문서와 정확성 우선 MassWorldCollision 기준선을 정의하는 것부터 시작한다.
