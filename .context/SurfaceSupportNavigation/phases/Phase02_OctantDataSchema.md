# Phase 2 — Octant Definition과 베이크 스키마

> 상태: 완료
> 예상 범위: 1~2세션
> 선행 조건: Phase 1 Mesh Terrain 제작 스파이크

## 목표

기존 Level 전용 Octant Pool을 Level Instance와 사전 베이크 데이터를 함께 보존하는 정의 목록으로 확장하고, `ULNPOctantSurfaceData`의 최소 직렬화 계약을 확정한다. 서버와 클라이언트는 같은 시드에서 같은 정의 전체를 선택해야 한다.

## 필수 문서

- `../Current.md`
- `../design/Architecture.md`
- `../design/DataModel.md`
- `../design/TerrainContract.md`
- `../research/MeshTerrain.md`
- `Phase01_MeshTerrainSpike.md`

## 실행 단위

### A. 최소 schema와 직렬화 기준선

- [x] `FIoHash` 20바이트를 손실 없이 저장하는 reflected 고정 크기 타입 추가
- [x] 정렬된 source package manifest와 semantic/settings hash 경계 정의
- [x] `FLNPSurfaceBakeHeader`와 Support·Navigation·Traversal·Spawn payload 경계 정의
- [x] `FLNPOctantDefinition`에 Level, SurfaceData, 허용 slot, seam signature 추가
- [x] 기존 `OctantPool`을 유지한 채 `OctantDefinitions`를 추가
- [x] header, manifest, 네 payload, 정의 배열의 직렬화 왕복 자동화 통과

### B. 기존 Pool의 최소 전환

- [x] 기존 Level 목록을 definition 목록으로 마이그레이션
- [x] 선택 결과가 Level만이 아니라 definition 전체를 8개 slot 순서로 보존
- [x] 허용 slot bitmask를 만족하는 결정론적 선택 알고리즘 구현
- [x] 같은 seed와 pool에서 서버·클라이언트 선택 결과가 같은 자동화 추가
- [x] 비어 있거나 배치 불가능한 pool의 명시적 오류 처리

### C. source manifest와 stale 검출

- [x] source LVI package 포함
- [x] 직접 참조 external actor·external object package 포함
- [x] Terrain Contract 역할 component가 참조하는 Static Mesh package 포함
- [x] Component Tag, transform, collision profile canonical hash 포함
- [x] baker schema version과 bake settings hash 포함
- [x] package name, package kind 순 정렬과 중복 제거 검증
- [x] 전체 재귀 dependency와 decoration/material dependency가 섞이지 않는 필터 테스트

### D. Windows cook과 다음 Phase 인계

- [x] 최소 SurfaceData를 Windows 단일 package로 cook
- [x] cooked asset을 packaged runtime에서 object path로 로드
- [x] cooked header, manifest와 Support·Navigation·Traversal·Spawn stream 보존 검증
- [x] Phase 3가 runtime 공개 schema/header를 참조할 수 있는 빌드 경계 확인

## 확정한 최소 계약

- `FIoHash`는 `FGuid`로 자르지 않고 20바이트 전부 저장한다.
- package hash는 Asset Registry의 `GetPackageSavedHash()` 값을 그대로 보존한다.
- `SourceContentHash`는 정렬된 package manifest, `SourceSemanticHash`, `BakeSettingsHash`의 집계 결과다.
- `DataVersion`은 SurfaceData header가 단일 원본으로 소유한다. definition에 중복 저장하지 않는다.
- `AllowedSlotRotations`는 고정 8개 runtime 회전에 대응하는 `uint8` bitmask다.
- `SeamSignature`는 조립 호환성 식별자이며 source freshness hash와 분리한다.
- SurfaceData는 작은 header와 Support·Navigation·Traversal·Spawn 네 stream을 분리한다.
- Phase 2에서는 stream payload를 byte array로 직렬화한다. 실제 codec과 BulkData 저장 정책은 각 소비 Phase의 측정 뒤 확정한다.
- `ThemeData`는 현재 runtime Level Instance가 직접 소비하지 않으므로 최소 definition에 넣지 않는다.

## 완료 조건

- [x] 새 schema의 save/load 또는 동등한 UObject 직렬화 왕복 테스트 통과
- [x] 기존 OctantPool content가 definition 형식으로 마이그레이션됨
- [x] 같은 seed가 같은 8개 definition과 slot 회전을 선택함
- [x] 선택된 definition 전체가 Level Instance 로드 이후에도 보존됨
- [x] source manifest 정렬과 aggregate hash가 입력 순서에 무관하게 결정론적임
- [x] `LootNPopEditor Win64 Development` 빌드 성공
- [x] Phase 3가 runtime 공개 schema/header를 참조할 수 있고 non-empty 최소 cooked fixture를 로드할 수 있음

마지막 조건은 Phase 3가 실제 Support Atlas를 소비한다는 뜻이 아니다. runtime 모듈에서 `FLNPSurfaceBakeHeader`와 `ULNPOctantSurfaceData`를 참조할 수 있고, sentinel byte가 든 최소 fixture가 cook 이후에도 header·manifest·네 stream을 보존한다는 직렬화·모듈 경계 게이트다. 실제 Support Atlas 생성과 codec은 Phase 4에 남긴다.

## 제외 범위

- 실제 Support Atlas rasterization
- Nav cell codec과 A*
- runtime immutable snapshot 게시
- 기존 SurfaceCache 소비자 전환
- production Terrain Contract tag 마이그레이션
- production definition의 SurfaceData 연결
