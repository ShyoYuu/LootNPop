# Phase 1 작업 로그

## 2026-09-21 — C안 엔진 조사와 비-WP 타깃 최소 실험

### 수행

- Mesh Terrain Mode의 Height Sculpt 등록, 일반 Static Mesh target factory, 부모 Vertex Sculpt 빌더 요구사항, Accept commit 경로를 조사했다.
- Mesh Partition Convert·Create Mesh의 World Partition 강제 검사와 모드 Toolkit의 포괄 경고를 분리했다.
- `LootNPop.SurfaceNavigation.MeshTerrain.NonWPStaticMeshTarget` 자동화 테스트를 추가했다.
- production `SM_Octant_00`은 읽기만 하고 transient package의 메모리 복제본으로 테스트했다. production asset과 Level Instance는 수정하지 않았다.

### 결과

- 비-WP transient world에서 일반 project Static Mesh component가 Height Sculpt의 수정 가능한 dynamic mesh 타깃으로 구성됐다.
- `UHeightSculptToolBuilder::CanBuildTool()`과 `UHeightSculptTool` 생성이 통과했다.
- 실제 Sphere 브러시 스트로크, Accept 후 asset 저장·재로드는 아직 수행하지 않았다.

### 검증

- `LootNPopEditor Win64 Development`: 성공
- `LootNPop.SurfaceNavigation.MeshTerrain.NonWPStaticMeshTarget`: 통과, commandlet 종료 코드 0
- 테스트 디스크 산출물: 없음

### 시행착오

- 최초 UBT 실행의 `dotnet.exe` 팝업은 코드 문제가 아니라 `%LOCALAPPDATA%/UnrealBuildTool` trace 백업 삭제가 샌드박스에서 거부된 `UnauthorizedAccessException`이었다. 권한 승인 후 정상 빌드했다.
- `/Engine/BasicShapes/Sphere`는 읽기 전용 엔진 에셋이라 commit 타깃이 되지 않았다.
- Static Mesh 비동기 컴파일 중에는 target factory가 의도적으로 false를 반환하므로 자동화에서 compilation 완료를 기다렸다.
- 초기 테스트의 null ToolManager는 도구 생성 시 `Object is not packaged` fatal을 일으켰다. transient ToolManager를 제공해 실제 에디터 조건과 맞췄다.

### 다음 작업

1. 별도 실험 Static Mesh asset에서 Sphere reference surface 브러시를 실제 적용하고 Accept·저장·재로드한다.
2. topology, material, Nanite, collision, Component Tag 보존 여부를 저장 전후 비교한다.

## 2026-09-22 — C안 실제 Sphere Sculpt와 독립 asset 왕복

### 수행

- `LootNPop.SurfaceNavigation.MeshTerrain.NonWPSphereSculptRoundTrip` 자동화 테스트를 추가했다.
- production `SM_Octant_00`을 읽기 전용 입력으로 사용하고 `/Game/SurfaceNavigationTests/MeshTerrain/SM_COptionSphereSculpt` 독립 실험 asset을 생성했다.
- 비-WP EditorPreview world에서 실제 `UHeightSculptTool`을 활성화해 Sphere reference surface stroke를 실행했다.
- 프리뷰 정점 변위를 확인한 뒤 Accept, package 저장, package 재로드, object path 재로딩을 수행했다.
- 저장 전후 topology와 material, Nanite, collision 속성을 비교했다.

### 결과

- 실제 Sphere Height Sculpt가 일반 Static Mesh에서 작동했다.
- 회귀 실행의 프리뷰 최대 정점 변위는 2226.534156cm였다. 단독 실행에서는 2400.000000cm였다.
- Accept 후 vertex, triangle, polygon 수를 유지하면서 정점 위치가 변경됐다.
- material slot, Nanite 설정, collision trace flag, double-sided collision이 보존됐다.
- Accept와 재로드 후 physics triangle mesh data가 존재했다.
- source component의 Terrain Contract Component Tag는 Accept 동안 보존됐다.
- 독립 package 저장과 재로드가 성공했다.
- production `SM_Octant_00`과 기존 `LVI_Octant_Meadow_00`은 수정하지 않았다.

### 검증

- `LootNPopEditor Win64 Development`: 성공
- `LootNPop.SurfaceNavigation.MeshTerrain.NonWPSphereSculptRoundTrip`: 통과
- `LootNPop.SurfaceNavigation.MeshTerrain.NonWPStaticMeshTarget`: 통과
- `LootNPop.SurfaceNavigation.MeshTerrain`: 2/2 통과, commandlet 종료 코드 0

### 시행착오와 제약

- 기본 brush pressure가 헤드리스 입력에서 0으로 평가될 수 있어 pressure sensitivity를 명시적으로 껐다.
- stroke 중에는 tool mesh update가 허용되지 않으므로 변위 캡처를 `OnEndDrag` 뒤로 옮겼다.
- 연결성 제한이 켜진 상태에서는 헤드리스 직접 입력의 ROI가 비어 정점 변위가 0이었다. `bRequireConnectivity=false`에서 실제 변위와 왕복이 성공했다.
- Component Tag는 Static Mesh asset metadata가 아니라 배치 component 소유다. 산출물을 LVI에 배치할 때 별도 복원이 필요하다.

### 다음 작업

1. C안 독립 산출물을 비-WP LVI에 배치해 8 slot 회전, seam, PCG 입력을 검증한다.
2. cook/package 후 Nanite와 cooked collision을 검증한다.
3. B안 WP authoring map의 Convert·Sculpt·Boolean→독립 asset 경로와 비교한다.

## 2026-09-22 — C안 seam·8 slot·PCG 방식 trace 검증

### 수행

- `LootNPop.SurfaceNavigation.MeshTerrain.COptionEightSlotSeamAndPCG` 자동화 테스트를 추가했다.
- production 원본과 스컬프 산출물의 seam 정점을 좌표평면 기준으로 대응 비교했다.
- 런타임과 같은 Pitch/Yaw 8개 회전으로 seam 좌표를 조립해 slot 간 일치를 검사했다.
- 각 회전 component에 기존 PCG 입력과 같은 비복합 trace를 실행했다.
- 저장 왕복 테스트에 `CTF_UseComplexAsSimple` 명시 검증을 추가했다.

### 결과

- seam 정점: 1194개, 최대 변위 0cm
- 스컬프 내부 최대 변위: 4599.937988cm
- 8 slot 고유 seam 위치: 4770개, 다른 slot과 불일치 0개
- PCG 방식 simple component trace: 8/8 hit
- Mesh Terrain 자동화 회귀: 3/3 통과, commandlet 종료 코드 0

### 범위 제한

- 이 결과는 직접 회전한 Static Mesh component 기준이다.
- 실제 `ALevelInstance` 로드, 기존 PCG graph 실행, HISM bake는 아직 검증하지 않았다.

### 다음 작업

1. 실제 비-WP LVI/PCG graph에 C안 산출물을 대입한 회귀 fixture를 만든다.
2. cook/package 후 cooked collision과 Nanite runtime 결과를 검증한다.
3. B안 WP authoring map 산출물과 비교한다.

## 2026-09-22 — production PCG graph와 C안 Blueprint 계약 검증

### 수행

- Unreal MCP로 열린 Editor의 production LVI, BP component, PCG component와 저장 HISM을 읽기 전용 검사했다.
- 임시 복제 LVI에서 production BP actor의 crust mesh를 C안 산출물로 교체해 실제 Editor PCG 재생성을 실행했다.
- production BP를 `/Game/SurfaceNavigationTests/MeshTerrain/BP_Octant_COption`으로 복제하고 crust component template에 C안 mesh와 `Support`, `PCGSurface` tag를 저장했다.
- `COptionLevelInstancePCGGraph` 자동화 테스트를 영속 BP 계약, production LVI 저장 baseline, C안 PCG spatial input 검증으로 구성했다.
- 저장되지 않는 임시 LVI 복제본과 외부 actor package는 제거했다.

### 결과

- production 저장 baseline: PCG graph 존재, HISM 3개, 인스턴스 1964개
- C안 mesh 실제 Editor 재생성: Cone 1301개, Cube 689개, Cylinder 313개, 합계 2303개
- C안 PCG bounds 최대값: `(26996.842, 27075.174, 26983.242)`cm
- C안 BP spawn 결과: sculpt mesh, 두 Terrain Contract tag, production PCG graph, 확장 spatial bounds 유지
- production BP crust에는 아직 Component Tag가 없어 content migration 항목으로 남겼다.

### 시행착오와 제약

- LVI actor의 상속 component에 적용한 mesh instance override는 즉시 PCG를 재생성하지만 저장·재로드 시 Blueprint Construction Script 기본값으로 복귀했다.
- 외부 actor를 사용하는 LVI 복제본은 MCP의 일반 asset save로 actor 교체가 영속 저장되지 않았다. 잘못된 fixture를 남기지 않고 산출물별 Blueprint 기본값을 영속 authoring 단위로 채택했다.
- transient Editor world의 PCG subsystem에서는 graph가 완료돼도 ISM 출력이 0개였다. 실제 Editor authoring world의 2303개 출력과 달라 자동화에서 headless output count를 성공 조건으로 사용하지 않는다.

### 검증

- Live Coding: 성공
- `LootNPop.SurfaceNavigation.MeshTerrain.COptionLevelInstancePCGGraph`: 통과
- 전체 Mesh Terrain 자동화: 4개 테스트 구성
- production BP/LVI: 이번 실험에서 저장하지 않았고 Editor `dirty=false`

### 다음 작업

1. C안 산출물을 cook/package해 cooked collision과 Nanite runtime 결과를 검증한다.
2. 실제 게임의 8개 Level Instance slot에서 회전·seam·PCG 결과를 통합 검증한다.
3. B안 WP authoring map 산출물과 비교한다.

## 2026-09-22 — C안 Nanite 활성화와 Windows cook

### 수행

- C안 실험 mesh에서 Nanite를 활성화하고 저장했다.
- Nanite 활성화 상태에서 전체 Mesh Terrain 자동화 4개를 다시 실행했다.
- 실험 mesh 한 package를 대상으로 Windows 격리 cook을 실행했다.
- cook 프로세스와 열린 Editor의 MCP HTTP 포트 충돌을 피하도록 commandlet에서 `ModelContextProtocol` 플러그인을 비활성화했다.

### 결과

- Nanite 활성화 후 전체 Mesh Terrain 자동화: 4/4 통과
- Windows cook: 종료 코드 0, `Success - 0 error(s), 1 warning(s)`
- 생성 파일: `.uasset` 2,985바이트, `.uexp` 1,076,968바이트, `.ubulk` 5,306,321바이트
- cook resource: StaticMesh 4.14MiB, BodySetup 0.46MiB, NavCollision 0.11MiB, DistanceField 0.89MiB
- 기존 `CTF_UseComplexAsSimple`과 physics triangle mesh data 보존 회귀도 계속 통과했다.

### 시행착오와 범위 제한

- 최초 cook은 샌드박스 밖 사용자 DDC·shader working directory 쓰기가 필요해 권한 있는 실행으로 전환했다.
- BP까지 포함한 전체 의존성 cook은 917 package를 처리했지만 열린 Editor와 commandlet의 MCP HTTP 포트 8000 충돌 때문에 종료 코드 1이었다. cook 격리 조건을 명시한 mesh 단독 실행으로 포트 충돌을 제거했다.
- 이번 결과는 cooked StaticMesh·BodySetup·NavCollision 직렬화 검증이다. packaged executable에서 Nanite 렌더링과 실제 scene query를 실행한 것은 아니다.

### 다음 작업

1. 실제 게임의 8개 Level Instance slot에서 C안 BP의 회전·seam·PCG 결과를 통합 검증한다.
2. B안 WP authoring map의 Convert·Sculpt·Boolean→독립 asset 경로와 비교한다.
3. 최종 후보는 packaged runtime에서 Nanite와 exact collision을 실행 검증한다.

## 2026-09-22 — C안 8-LVI 통합과 반복 결정성 수정

### 수행

- C안 전용 `LVI_Octant_COption`과 `L_COptionEightSlotIntegration`을 생성했다.
- 전용 LVI는 Level Instance edit·commit 경로로 production actor를 `BP_Octant_COption`으로 교체했다.
- 통합 맵에 runtime subsystem과 동일한 8개 회전을 저장하고 Simulate-In-Editor로 스트리밍했다.
- 왕복 테스트가 기존 실험 mesh를 다시 입력으로 사용해 스컬프트를 누적하던 결함을 수정했다.
- 매 테스트 시작 시 production 원본 geometry·material·collision을 복원하고 Nanite를 활성화한 뒤 한 번만 스컬프트하도록 변경했다.
- 저장된 8-slot map과 C안 LVI를 읽기 전용 검증하는 `COptionEightLevelInstanceAsset` 테스트를 추가했다.

### 결과

- PIE C안 actor: 8개 모두 로드
- 각 actor: HISM 3개·1960개, 전체 15,680개
- 각 actor: C안 mesh, `Support`·`PCGSurface` tag, legacy nav 비활성 유지
- 8방향 world trace: 8/8 hit, 거리 약 25,168.21cm
- 전체 Mesh Terrain 자동화: 5/5 연속 통과
- 반복 수치: Sphere Sculpt 최대 변위 2400.000000cm, seam 최대 변위 0cm, PCG bounds 최대 25000cm로 동일
- current mesh 재-cook: 종료 코드 0, `Success - 0 error(s), 1 warning(s)`
- current cooked 파일: `.uasset` 2,985바이트, `.uexp` 417,278바이트, `.ubulk` 5,093,896바이트

### 수정된 해석

- 기존 2303개와 이후 2777개 PCG instance 수는 반복 테스트가 스컬프트를 누적한 결과였다. 최종 단일 스컬프트 기준은 1960개다.
- axis-aligned bounds가 25000cm를 넘는다는 검사는 누적 변형에 의존했다. PCG spatial bounds가 실제 sculpted mesh bounds와 일치하는지 비교하도록 바꿨다.
- LVI에 남아 있던 2777개 stale HISM도 전용 actor를 재생성·commit해 1960개로 동기화했다.

### source hash 후보

- `FAssetPackageData::GetPackageSavedHash()`의 `FIoHash`를 기본 변경 표식으로 채택 후보에 올렸다.
- LVI, 외부 actor/object, 역할 component가 참조하는 mesh, semantic authoring 값, baker version/settings를 정렬 manifest로 집계한다.
- 최종 저장 타입과 dependency 필터는 Phase 2 데이터 스키마에서 확정한다.

### 다음 작업

1. B안 WP authoring map의 Convert·Sculpt·Boolean→독립 asset 경로를 실험한다.
2. C안과 B안을 반복 제작 비용과 산출물 계약으로 비교한다.
3. 최종 후보를 packaged runtime에서 실행 검증한다.

## 2026-09-22 — B안 WP authoring과 독립 asset 추출 시도

### 수행

- 별도 World Partition 맵 `L_BOptionMeshTerrainAuthoring`에서 production 원본 mesh를 Mesh Terrain의 Convert 도구로 3×3×1 `AMeshPartition`으로 변환했다.
- 실제 Height Sculpt 스트로크와 Boolean Subtract modifier를 적용해 Mesh Partition 재빌드를 확인했다.
- PreviewSection actor 내부에 생성되는 `MeshPartitionStaticMesh_0`을 독립 package로 추출하는 자동화 테스트를 추가했다.

### 중간 결과와 시행착오

- Mesh Terrain 결과 mesh는 독립 Content asset이 아니라 PreviewSection actor의 중첩 `UStaticMesh`로 생성된다.
- 일반 UObject 복제는 저장까지는 진행됐지만 유효한 `StaticMeshDescriptionBulkData`를 만들지 못했다. 재로드 시 `Bad MeshDescription`과 assertion으로 에디터가 종료되어 해당 손상 실험 asset을 즉시 삭제했다.
- 추출 경로를 중첩 mesh의 `FMeshDescription`을 복제한 뒤 새 최상위 `UStaticMesh`의 source model, material, BodySetup, Nanite를 명시적으로 재구성하는 방식으로 교체했다. 에디터 재실행 뒤 저장·재로드 검증이 남아 있다.

### 빌드 환경 재발 방지

- 같은 전체 빌드 명령이 관리형 샌드박스 안에서는 bundled .NET/UBT 시작 직후 컴파일 진단 없이 종료 코드 1을 반환했고, 권한 확장 실행에서는 컴파일·링크까지 정상 완료했다.
- 원인은 이전과 동일하게 UBT/.NET이 `%LOCALAPPDATA%\UnrealBuildTool`, 임시 폴더, Unreal Build Accelerator 등 프로젝트 밖 경로에 기록해야 하는 조건이다.
- 이후 전체 빌드와 cook/package는 샌드박스에서 먼저 실패시켜 보지 않고 처음부터 권한 확장 실행한다. 이 공통 규칙은 프로젝트 단일 협업 기준인 `CLAUDE.md`에 기록했다.

### 검증

- 손상 실험 asset 삭제 확인: `SM_BOptionExtracted.uasset` 없음
- B안 authoring map과 외부 actor/object package 유지 확인
- 수정 후 `LootNPopEditor Win64 Development`: 권한 확장 전체 빌드 성공, 경고 없음

## 2026-09-22 — B안 추출 완료와 C/B 선택

### 결과

- 수정한 `BOptionIndependentAssetExtraction`이 B안 중첩 mesh를 새 최상위 Static Mesh로 재구성해 저장·재로드를 두 번 연속 통과했다.
- B안 asset: vertex 554개, triangle 755개, polygon 755개, material slot 1개
- Nanite, `CTF_UseComplexAsSimple`, double-sided collision, physics triangle mesh data 유지
- 직접 dependency에 B안 authoring world와 Mesh Partition asset이 없고 저장 후 `dirty=false`
- Windows 격리 cook: 종료 코드 0, `Success - 0 error(s), 1 warning(s)`
- cooked 파일: `.uasset` 2,802바이트, `.uexp` 224,844바이트, `.ubulk` 6,186,396바이트
- cook resource: StaticMesh 5.22MiB, BodySetup 0.31MiB, NavCollision 0.03MiB, DistanceField 0.68MiB

### 회귀 수정

- B안 WP 맵에서 전체 6개 테스트를 실행하자 `COptionEightLevelInstanceAsset`이 아직 로드되지 않은 C안 통합 맵의 World Partition 외부 actor를 `PersistentLevel->Actors`에서 0개로 읽었다.
- 테스트가 C안 통합 맵을 직접 열고, Level Instance의 저장 상태는 로드 Actor 배열 대신 `FWorldPartitionActorDescInstance`와 `FLevelInstanceActorDesc`로 검사하도록 수정했다.
- 전체 빌드 성공 후 headless Mesh Terrain 자동화 6/6 통과, 종료 코드 0을 확인했다.
- 종료 직전 저장된 `BP_Octant_COption`과 `LVI_Octant_COption` 외부 actor도 동일 회귀에서 mesh·tag·PCG·HISM 계약을 통과했다.

### 선택

- 기본 경로: C안 일반 Static Mesh + Sphere Height Sculpt
- 제한적 보조 경로: B안 WP Mesh Terrain. Boolean 등 Mesh Partition 고유 modifier가 반드시 필요한 제작에만 사용
- runtime 계약은 경로와 무관하게 독립 `UStaticMesh`, cooked `BodySetup`/`NavCollision`, 배치 component의 Terrain Contract tag로 통일

### 다음 작업

1. C안 8-slot 통합 맵을 Development packaged runtime으로 실행한다.
2. Nanite resource와 8방향 exact collision scene query를 확인한다.
3. 통과하면 Phase 1을 종료하고 Phase 2 데이터 스키마로 전환한다.

## 2026-09-22 — Development packaged runtime 검증과 Phase 1 종료

### 수행

- 게임 모듈에 `LootNPop.SurfaceNavigation.PackagedRuntime.COptionNaniteAndExactCollision` 자동화를 추가했다.
- `L_COptionEightSlotIntegration`을 포함한 Win64 Development 패키지를 생성했다.
- 패키지 실행 파일을 D3D12 오프스크린으로 기동해 cooked C안 mesh의 render·Nanite·physics collision data와 8방향 world scene query를 검증했다.

### Game 타깃 빌드 수정

- 기존 `bWithPushModel=true`는 설치형 엔진의 공유 `UnrealGame` 바이너리와 빌드 환경이 달라 UBT가 빌드를 거부했다.
- `bOverrideBuildEnvironment=true`는 `UEPushModelPrivate::MarkPropertyDirty` 링크 실패를 만들었다.
- `TargetBuildEnvironment.Unique`는 설치형 엔진에서 사용할 수 없었다.
- Game 타깃의 강제값을 제거해 엔진 기본 설정을 사용하도록 했다. Iris module 구성은 유지했고 `LootNPop Win64 Development` 링크와 BuildCookRun이 성공했다.

### 테스트 보정

- 첫 `NullRHI` 실행은 Static Mesh render data를 역직렬화하지 않아 Nanite 검증에 부적합했다.
- D3D12 실행에서는 render data·Nanite data·physics triangle data가 모두 유효했다.
- cooked build의 Nanite triangle 통계는 0일 수 있으므로 `HasValidNaniteData()`를 계약으로 사용했다.
- C안은 source의 단면 collision 설정을 보존하므로 Editor 기준과 같은 안쪽→바깥쪽 trace 방향을 사용했다.

### 최종 결과

- `LootNPopEditor Win64 Development`: 성공
- `LootNPop Win64 Development`: 성공
- BuildCookRun: 종료 코드 0, 967 package cook, archive 생성 성공
- packaged runtime 자동화: `Result={Success}`, `EXIT CODE: 0`
- cooked render data: 유효
- cooked Nanite data: 유효
- cooked physics triangle data와 `CTF_UseComplexAsSimple`: 유효
- 런타임 8 slot world scene query: 8/8 hit

기존 Lyra Mannequin material의 누락 Material Function 경고와 default material 대체는 별도 콘텐츠 문제이며 Surface Navigation 검증 결과에는 영향을 주지 않았다.

### 결론

Phase 1 완료 조건을 모두 충족했다. C안을 기본 제작 경로, B안을 Mesh Partition 고유 modifier용 제한적 보조 경로로 확정하고 Phase 2 Octant Definition·베이크 스키마 설계로 전환한다.
