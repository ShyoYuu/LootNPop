# Phase 1 — Mesh Terrain 제작 스파이크

> 상태: 완료
> 예상 범위: 1~2세션
> 선행 조건: Phase 0 지형 계약과 회귀 테스트 맵

## 목표

UE 5.8 Mesh Terrain을 현재 비-WP 옥탄트 Level Instance 구조의 제작 도구로 사용할 수 있는지 검증하고, 런타임이 소비할 static mesh·collision·metadata 산출물 계약을 확정한다.

## 필수 문서

- `../research/MeshTerrain.md`
- `../design/Architecture.md`
- `../design/DataModel.md`
- `../../TechDesign_WorldGeneration.md`
- `../../Guide_OctantLevelInstance.md`

## 실험 후보

### C안

비-WP 일반 Static Mesh 제작 흐름에서 Sphere Height Sculpt를 차용한다.

### B안

별도 World Partition authoring map에서 Convert·Sculpt·Boolean을 수행하고 독립 runtime asset으로 베이크한다.

### 폴백

두 방식 모두 불안정하면 Mesh Terrain의 사용 범위를 제한하고 Modeling Tools 또는 Geometry Script를 병행한다.

## 검증 항목

- [x] 비-WP Static Mesh에서 Sphere Height Sculpt 사용 가능 여부
- [x] WP 제작 맵의 Convert·Sculpt·Boolean
- [x] 독립 Static Mesh 산출물 추출
- [x] 기본 지각의 source 의미 보존과 부유섬·동굴 입력 계약 확정
- [x] 기존 비-WP LVI에서 8 slot 회전
- [x] 옥탄트 seam 반지름 보존
- [x] Nanite 호환
- [x] Windows cook과 cooked collision 생성
- [x] packaged runtime 실행
- [x] 기존 PCG 입력 회귀
- [x] 산출물 stale 검출에 사용할 source hash 후보

## 결과 기록

각 후보마다 다음을 `../research/MeshTerrain.md`와 `../history/Phase01_Log.md`에 기록한다.

- 사용한 엔진 기능과 관련 소스 위치
- 입력 asset과 생성 산출물
- 수동 편집이 필요한 단계
- 재생성의 결정론과 반복 시간
- collision·Nanite·metadata 보존 여부
- LVI slot 회전과 seam 결과
- 패키지 빌드 결과
- 채택 또는 기각 이유

## 결정 게이트

- [x] C안 성공: 일반 mesh 기반 Sphere Sculpt 흐름 채택
- [ ] C안 실패·B안 성공: WP authoring map→독립 asset bake 채택
- [ ] 둘 다 불안정: 제한적 Mesh Terrain + Modeling Tools/Geometry Script 병행

현재 C안은 실제 Sphere Sculpt, Accept, 독립 asset 저장·재로드, production PCG graph의 Editor 재생성, Nanite 활성화, Windows cook, 실제 8개 LVI의 PIE 스트리밍과 Development packaged runtime까지 통과했다. B안도 WP 제작 맵의 Convert·Sculpt·Boolean, 독립 asset 재구성·재로드와 Windows cook을 통과했지만 별도 WP graph와 커스텀 추출기가 필요하다. 따라서 C안을 기본 제작 경로로 채택하고 B안은 Mesh Partition 고유 modifier가 필요한 특수 제작에만 사용한다.

직접 component 조립 실험에서는 런타임과 같은 8개 회전의 seam 일치와 PCG 방식 단순 trace를 통과했다. C안 전용 LVI와 8-slot 통합 맵의 PIE에서는 actor 8개가 모두 로드되고 각 HISM 3개·1960개, 8방향 world trace 8/8 hit를 확인했다. Nanite를 활성화한 동일 mesh는 Windows cook에서 BodySetup과 NavCollision을 포함해 오류 없이 직렬화됐다. Development packaged runtime의 D3D12 오프스크린 자동화에서도 유효한 render·Nanite·physics triangle data와 `CTF_UseComplexAsSimple`, 8방향 world trace 8/8 hit를 확인했다.

## 완료 조건

- [x] runtime이 직접 Mesh Terrain 시스템에 의존하는지 여부 확정
- [x] runtime static mesh 산출물 형식 확정
- [x] cooked collision 산출물 형식 확정
- [x] 베이커가 읽을 authoring metadata 형식 확정
- [x] 옥탄트 8 slot 회전과 seam 검증 통과
- [x] 패키지 빌드 또는 cook 검증 통과
- [x] Phase 2 스키마 설계에 필요한 입력 계약 확정

## 제외 범위

- 최종 `ULNPOctantSurfaceData` 구현
- 런타임 Support query
- MassWorldCollision
- Nav Grid
- 실제 품질의 전체 옥탄트 제작
