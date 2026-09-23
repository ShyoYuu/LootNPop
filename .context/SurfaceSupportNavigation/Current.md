# Surface Support·Navigation 현재 작업 상태

> 상태: 활성
> 현재 Phase: Phase 3 — MassWorldCollision 정확성 기준선 · 착수 준비
> 마지막 갱신: 2026-09-23

## 현재 목표

Phase 2는 완료됐다. 다음 세션은 기존 Chaos cooked collision과 scene acceleration을 사용하는 `MassWorldCollision` 정확성 기준선을 정의하고, 회귀 fixture에서 벽·섬·동굴·프랍·동적 패널 충돌을 검증하는 Phase 3 실행 문서를 작성하는 것부터 시작한다.

Phase 3는 Phase 2가 제공한 runtime 공개 schema와 `FLNPSurfaceBakeHeader`를 참조할 수 있지만, Support Atlas payload를 생성하거나 소비하지 않는다. 실제 Support Atlas 생성은 Phase 4 범위다.

## 착수 시 필수 문서

- `Roadmap.md`
- `design/RuntimeCollision.md`
- `design/TerrainContract.md`
- `design/RegressionMap.md`
- `phases/Phase02_OctantDataSchema.md`

## Phase 2 인계 기준선

- `FLNPOctantDefinition`과 `ULNPOctantSurfaceData`의 runtime 공개 타입 사용 가능
- header에서 data version, source manifest/hash와 네 payload descriptor 확인 가능
- 기존 `OctantPool`과 새 `OctantDefinitions` 병존, legacy 필드 유지
- 같은 seed와 slot mask에서 definition 전체를 결정론적으로 선택·보존
- source LVI·직접 external package·역할 mesh와 semantic/settings 값을 제한적으로 hash
- 최소 SurfaceData의 editor package 저장·재로드와 Windows cooked runtime 로드 검증 완료
- 최소 fixture의 네 payload는 codec 계약이 아닌 직렬화 검증용 sentinel byte다

## 바로 다음 작업

1. `design/RuntimeCollision.md`, `design/TerrainContract.md`, `design/RegressionMap.md`를 기준으로 Phase 3 실행 문서와 완료 조건을 작성한다.
2. 기존 Chaos scene query를 사용하는 정확성 우선 `MassWorldCollision` API와 회귀 fixture 범위를 확정한다.
3. Support 기반 후보 축소나 별도 BVH 없이 exact query의 정확성과 비용 기준선을 만든다.

## Phase 1에서 확정된 입력 계약

- 기본 authoring: 비-WP 일반 Static Mesh + Sphere Height Sculpt
- 선택적 authoring: 별도 WP Mesh Terrain + 독립 Static Mesh 재구성
- runtime geometry: authoring graph에 의존하지 않는 독립 `UStaticMesh`
- exact collision: cooked `UBodySetup`과 `CTF_UseComplexAsSimple`
- 의미 metadata: source `UPrimitiveComponent`의 Terrain Contract tag와 collision profile
- slot transform: `(Pitch 0°/180°, Yaw 0°/90°/180°/270°)` 8개
- stale 검출 후보: source LVI·외부 actor/object·역할 mesh·semantic 값·schema version을 정렬한 manifest와 package saved hash

## Phase 2에서 이관한 후속 작업

- production Terrain Contract tag·collision profile 마이그레이션은 실제 베이커를 production source에 적용하는 Phase 4 이후에 수행한다.
- production definition의 SurfaceData 연결과 runtime 로드는 Phase 5 소비자 전환에서 수행한다.
- 실제 Support Atlas rasterization과 payload codec은 Phase 4 범위다.

## 알려진 불확실성

- Phase 3 exact query의 호출 빈도와 배치 단위는 정확성 기준선 측정 뒤 결정한다.
- `LNPSurfaceSupport`와 `LNPWorldExact` 신규 channel로 기존 소비자를 전환하는 시점은 Phase 3 회귀 결과와 함께 확정한다.
- Development package의 기존 Lyra Mannequin material은 누락 Material Function 때문에 default material로 대체된다. Surface Navigation 검증과는 분리된 콘텐츠 문제다.

## 블로커

현재 확인된 블로커는 없다.

## 마지막 검증

- `LootNPopEditor Win64 Development`: 성공, Editor 종료 상태의 전체 빌드
- `LootNPop Win64 Development`: 성공
- Phase 2 schema 자동화: 직렬화·선택·dependency·hash·package save/reload 5/5 통과, 오류·경고 없음
- 실제 `/Game/SurfaceNavigationTests/Schema/DA_MinimalOctantSurfaceData` 저장·package reload·object path 재로드 성공
- Windows 단일-package cook: 성공, cooked `.uasset`·`.uexp` 생성
- Win64 Development BuildCookRun: 성공, 953 package cook과 archive 생성
- packaged runtime `MinimalSurfaceDataCookedLoad`: 1/1 성공, 오류·경고 없음
- cooked header, manifest, Support·Navigation·Traversal·Spawn payload와 descriptor 보존 확인
- Editor 재시작 후 `OctantPoolData`: definition 1개와 legacy Level 1개 보존 확인
- PIE world generation: 8개 Level Instance spawn·visible load와 완료 이벤트 확인

## Phase 2 산출물

- 데이터 모델: `design/DataModel.md`
- 실행 기록: `history/Phase02_Log.md`
- 완료 조건: `phases/Phase02_OctantDataSchema.md`
- 최소 cooked output: `Saved/SurfaceNavigationPhase2Cook/`
- packaged 검증 archive: `Saved/SurfaceNavigationPhase2Package/Windows`
- packaged 자동화 보고서: `Saved/SurfaceNavigationPhase2Reports/index.json`
