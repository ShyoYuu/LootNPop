# Surface Support·Navigation 현재 작업 상태

> 상태: 활성
> 현재 Phase: Phase 2 — Octant Definition과 베이크 스키마 · 착수 준비
> 마지막 갱신: 2026-09-22

## 현재 목표

Phase 1에서 확정한 독립 Static Mesh·cooked collision·component metadata 입력 계약을 바탕으로, 기존 옥탄트를 결정론적으로 로드할 `FLNPOctantDefinition`과 `ULNPOctantSurfaceData`의 최소 직렬화 스키마를 확정한다.

Phase 1은 완료됐다. 기본 제작 경로는 비-WP 일반 Static Mesh + Sphere Height Sculpt(C안)이며, WP Mesh Terrain(B안)은 Boolean 등 고유 modifier가 필요한 특수 제작의 보조 경로다. 두 경로 모두 runtime에는 독립 `UStaticMesh`와 배치 component metadata만 전달한다.

## 착수 시 필수 문서

- `Roadmap.md`
- `design/Architecture.md`
- `design/DataModel.md`
- `design/TerrainContract.md`
- `research/MeshTerrain.md`
- `phases/Phase01_MeshTerrainSpike.md`

## 바로 다음 작업

1. Phase 2 실행 문서와 완료 조건을 작성한다.
2. `FLNPOctantDefinition`의 최소 필드, source manifest와 content hash 저장 타입을 확정한다.
3. `ULNPOctantSurfaceData`의 header와 payload 경계를 정하고 직렬화 왕복 테스트를 먼저 만든다.

## Phase 1에서 확정된 입력 계약

- 기본 authoring: 비-WP 일반 Static Mesh + Sphere Height Sculpt
- 선택적 authoring: 별도 WP Mesh Terrain + 독립 Static Mesh 재구성
- runtime geometry: authoring graph에 의존하지 않는 독립 `UStaticMesh`
- exact collision: cooked `UBodySetup`과 `CTF_UseComplexAsSimple`
- 의미 metadata: source `UPrimitiveComponent`의 Terrain Contract tag와 collision profile
- slot transform: `(Pitch 0°/180°, Yaw 0°/90°/180°/270°)` 8개
- stale 검출 후보: source LVI·외부 actor/object·역할 mesh·semantic 값·schema version을 정렬한 manifest와 package saved hash

## Phase 2에서 확정할 항목

- `FLNPOctantDefinition`의 실제 필드와 소유 asset
- `FIoHash` 또는 동등한 고정 크기 직렬화 타입
- source dependency 필터와 manifest 정렬 규칙
- `DataVersion`, `AllowedSlotRotations`, `SeamSignature` 표현
- `ULNPOctantSurfaceData`의 header와 향후 BulkData stream 경계
- 기존 Octant Pool과 world generation이 definition 전체를 보존하는 최소 전환 경로

## 알려진 불확실성

- production BP crust component에는 Terrain Contract tag가 아직 없어 별도 마이그레이션이 필요하다.
- Component Tag는 Static Mesh asset이 아니라 배치 component가 소유하므로 asset 저장과 metadata 저장을 분리해야 한다.
- 전체 재귀 dependency를 hash에 포함하면 장식 PCG나 material 변경까지 불필요한 재베이크를 만들 수 있다.
- 대화형 Editor에서 연결성 제한을 유지한 Sphere Sculpt stroke는 향후 authoring UX 보강 때 별도 확인한다.
- Development package의 기존 Lyra Mannequin material은 누락 Material Function 때문에 default material로 대체된다. Surface Navigation 검증과는 분리된 콘텐츠 문제다.

## 블로커

현재 확인된 블로커는 없다.

## 마지막 검증

- Editor 자동화 `LootNPop.SurfaceNavigation.MeshTerrain`: headless 6/6 통과, 종료 코드 0
- `LootNPopEditor Win64 Development`: 성공
- `LootNPop Win64 Development`: 성공
- Win64 Development BuildCookRun: 성공, archive 생성
- packaged runtime `COptionNaniteAndExactCollision`: D3D12 오프스크린 성공, 종료 코드 0
- cooked C안 mesh: render·Nanite·physics triangle data 유효, `CTF_UseComplexAsSimple` 유지
- runtime 8 slot world scene query: 8/8 hit
- C안: seam 불일치 0, 전용 LVI actor 8개와 HISM 총 15,680개 검증
- B안: 독립 asset 저장·재로드·cook과 authoring world 비의존성 검증

## Phase 1 산출물

- 상세 결과: `research/MeshTerrain.md`
- 실행 기록: `history/Phase01_Log.md`
- 완료 조건: `phases/Phase01_MeshTerrainSpike.md`
- 확정 결정: `Decisions.md`의 D-024
- 패키지 archive: `Saved/SurfaceNavigationPhase1Package/Windows`
