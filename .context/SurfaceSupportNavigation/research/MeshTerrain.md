# Mesh Terrain 도입 조사

> 상태: 검증 필요
> 읽기 조건: Mesh Terrain 제작 스파이크나 옥탄트 authoring 파이프라인을 조사할 때
> 최초 설계 원본: `../history/InitialPlan.md`

## 현재 구현과 구조적 한계

### 현재 SurfaceCache

현재 `ULNPSurfaceCacheSubsystem`은 월드 생성 완료 후 다음 과정을 수행한다.

- 구면을 등장방형 위도·경도 격자로 분할
- 중심에서 바깥쪽으로 `AsyncLineTraceByChannel` 발사
- 방향마다 첫 번째 지표면 위치 하나 저장
- 완료 후 배열을 immutable로 게시
- Mass worker에서 4점 보간으로 `GetSurfacePoint(Direction)` 조회

현재 설정은 약 100cm 간격, 785×1571, 총 1,233,235개 샘플이다. `FVector`가 LWC double인 Win64에서 현재 `FPoint`는 대략 32바이트이므로 순수 샘플 배열만 약 37.6MiB다. 베이크는 머신마다 약 7초가 걸린다.

### 현재 표현이 깨지는 경우

`방향 → 표면점 하나`는 다음을 표현하지 못한다.

- 기본 지각 위의 부유섬
- 동굴 천장 뒤의 동굴 바닥
- 같은 방향에 겹친 여러 섬
- 절벽 위·아래의 불연속
- 섬의 측벽과 밑면
- 움직이는 패널
- 파괴로 사라진 바닥

해상도를 높여도 층과 불연속의 의미가 생기지 않으므로 근본 해결이 아니다.

### 깨지는 기존 소비자

전환 대상은 SurfaceCache 자체뿐 아니라 최소 다음 소비자들이다.

- Enemy grounded 이동과 경사 판정
- Enemy 공중 넉백·착지
- Idle 배회 지점 선택
- Loot Pod와 Enemy 초기 스폰
- 투사체 `IsUnderSurface`
- 투사체 탄도 예측과 지면 VFX
- PCG 표면 투영
- World Device 배치와 주변 지형 검사
- 추후 지상 경로 탐색

특히 `Pos.SizeSquared() >= SurfacePoint.SizeSquared()` 형태의 지하 판정은 다층 환경에서 의미가 없다. 최종적으로 제거해야 한다.

---

## UE 5.8 Mesh Terrain 제약

UE 5.8의 Mesh Partition/Mesh Terrain에는 구 기준 Height Sculpt가 존재하지만 Mesh Partition의 생성·변환 경로는 World Partition을 요구한다. 현재 옥탄트 LVI는 비-WP이므로 다음 두 경로를 스파이크한다.

- C안: 일반 Static Mesh에서 Sphere Height Sculpt 도구만 사용
- B안: 별도 WP 제작 맵에서 Mesh Terrain 편집 후 독립 Static Mesh asset으로 베이크

전면 MegaMesh 런타임 도입은 다음 이유로 우선 제외한다.

- 현재 옥탄트 랜덤 조합과 런타임 회전 모델에 맞지 않음
- compiled section이 월드 좌표 actor 중심으로 생성됨
- 월드 크기에 비해 인프라가 과함
- 실험 기능에 대한 런타임 의존도가 높아짐

Mesh Terrain 결과의 cooked collision은 complex-as-simple 및 double-sided 구성이 가능하므로 정적 지형 exact query에는 적합하다.

---

## C안 엔진 소스 조사 — 일반 Static Mesh 타깃 경로

2026-09-21 조사 결과, Mesh Terrain 모드 전체의 World Partition 경고와 Sphere Height Sculpt 도구 자체의 타깃 제약은 분리되어 있다.

### 확인된 경로

- `UHeightSculptToolBuilder`는 별도 Mesh Partition 전용 빌더가 아니라 `UMeshVertexSculptToolBuilder`를 그대로 상속한다.
  - `MeshPartitionModelingToolset/Public/MeshPartitionHeightSculptTool.h:61`
  - `MeshPartitionModelingToolset/Private/MeshPartitionHeightSculptTool.cpp:16`
- 부모 빌더의 타깃 요구사항은 material, dynamic mesh read, dynamic mesh commit, scene component backing 네 인터페이스다. Mesh Partition actor나 World Partition 자체를 요구하지 않는다.
  - `MeshModelingTools/Private/MeshVertexSculptTool.cpp:95-104`
- Mesh Terrain 모드는 일반 `UStaticMeshComponentToolTargetFactory`를 등록한 뒤 같은 Height Sculpt 빌더를 도구로 등록한다.
  - `MeshTerrainMode/Private/MeshTerrainMode.cpp:559`
  - `MeshTerrainMode/Private/MeshTerrainMode.cpp:1097-1099`
- 일반 Static Mesh component 팩토리는 편집 가능한 source LOD가 있고 `/Engine/` 읽기 전용 에셋이 아니면 dynamic mesh commit이 가능한 `UStaticMeshComponentToolTarget`을 만든다.
  - `ModelingComponentsEditorOnly/Private/ToolTargets/StaticMeshComponentToolTarget.cpp:304-337`
- Accept 시 공통 sculpt 기반 클래스가 편집된 dynamic mesh를 타깃에 다시 커밋한다.
  - `MeshModelingTools/Private/Sculpting/MeshSculptToolBase.cpp:241-305`

### World Partition 강제 지점

- 모드 Toolkit은 비-WP world에서 “Mesh Terrain Mode tools require World Partition” 경고를 표시하지만, 이 코드는 경고 표시이며 Height Sculpt 빌더의 `CanBuildTool` 조건이 아니다.
  - `MeshTerrainMode/Private/MeshTerrainModeToolkit.cpp:1624-1638`
- 반면 Mesh Partition Convert와 Create Mesh는 실행 경로에서 `World->GetWorldPartition()`을 명시적으로 검사해 비-WP world를 거부한다.
  - `MeshPartitionModelingToolset/Private/MeshPartitionConvertTool.cpp:71-76,117-121`
  - `MeshPartitionModelingToolset/Private/MeshPartitionCreateMeshTool.cpp:218-222,755-760`

따라서 C안의 최소 경로는 `Mesh Partition Convert 없이 일반 project Static Mesh component를 Height Sculpt 타깃으로 직접 사용`하는 것이다. 모드의 포괄 경고만으로 C안을 기각하면 안 된다.

### 자동화 최소 실험

테스트: `LootNPop.SurfaceNavigation.MeshTerrain.NonWPStaticMeshTarget`

- 입력: `/Game/Maps/Meshes/SM_Octant_00`을 읽기만 한 뒤 `/Game/SurfaceNavigationTests/Transient/SM_COptionTarget` 이름의 transient package에 메모리 복제
- world: transient `UWorld`, `GetWorldPartition() == nullptr`
- 디스크 산출물: 없음
- 검증:
  - 일반 Static Mesh 타깃 클래스가 Height Sculpt의 네 인터페이스 요구사항을 충족
  - `UStaticMeshComponentToolTargetFactory`가 복제본을 수정 가능한 타깃으로 수락
  - `UHeightSculptToolBuilder::CanBuildTool()` 성공
  - 빌더가 `UHeightSculptTool`과 commit 가능한 타깃을 생성
- 결과: 통과, commandlet 종료 코드 0

이 결과는 비-WP 일반 Static Mesh에 대한 **도구 진입과 commit 경로의 구조적 가능성**을 증명한다.

엔진 기본 `/Engine/BasicShapes/Sphere`는 읽기 전용이며 commit 타깃이 되지 않는다. 또한 Static Mesh가 비동기 컴파일 중이면 팩토리는 UI 응답성을 위해 일시적으로 false를 반환하므로 자동화에서는 asset compilation 완료를 기다려야 한다.

### 실제 Sphere Sculpt·독립 asset 왕복 실험

테스트: `LootNPop.SurfaceNavigation.MeshTerrain.NonWPSphereSculptRoundTrip`

- 입력: production `/Game/Maps/Meshes/SM_Octant_00` 읽기 전용 로드
- 산출물: `/Game/SurfaceNavigationTests/MeshTerrain/SM_COptionSphereSculpt`
- world: `EWorldType::EditorPreview`, World Partition 없음
- 도구: `UHeightSculptTool`, Sphere reference surface, Height Sculpt brush
- 왕복: 실제 stroke → preview 변위 확인 → Accept → package 저장 → `ReloadPackages` → object path 재로드
- production asset과 기존 Level Instance: 비변경

2026-09-22 회귀 실행에서 Mesh Terrain 테스트 3개가 모두 통과했다. 실제 stroke의 프리뷰 최대 정점 변위는 실행별로 1927.975546~2400.000000cm였으며 0이 아닌 변위가 Accept 후 Static Mesh에 반영되고 재로드 뒤에도 유지됐다.

#### 저장 전후 보존 결과

- vertex, triangle, polygon 수: 보존
- material slot 배열: 보존
- Nanite 설정: 보존
- collision trace flag: 보존
- double-sided collision: 보존
- physics triangle mesh data: Accept와 재로드 후 존재
- collision complexity: `CTF_UseComplexAsSimple` 유지
- Component Tag: 도구 Accept 중 source component에서 보존

Component Tag는 Static Mesh asset 자체의 직렬화 metadata가 아니라 배치된 `UStaticMeshComponent`의 속성이다. 따라서 독립 Static Mesh 저장 성공과 Component Tag 보존은 같은 문제가 아니다. runtime 산출물을 LVI에 다시 배치할 때 Terrain Contract tag를 actor/component authoring 단계에서 복원해야 한다.

### seam·8 slot·PCG 입력 실험

테스트: `LootNPop.SurfaceNavigation.MeshTerrain.COptionEightSlotSeamAndPCG`

- production 원본과 C안 산출물에서 로컬 좌표평면의 seam 정점 1194개를 대응 비교했다.
- 안정된 단일 스컬프트 기준 seam 최대 변위는 0cm였고 내부 최대 변위는 2400.001465cm였다. 즉 Sphere Sculpt가 내부를 바꾸면서 경계 고정은 유지했다.
- 런타임 규약 `(Pitch 0°/180°, Yaw 0°/90°/180°/270°)`으로 같은 산출물을 8개 변환했다.
- 양자화 0.1cm 기준 고유 seam 위치 4770개 모두가 최소 하나의 다른 slot과 일치했고 불일치는 0개였다.
- 각 slot의 중앙 방향에 PCG와 같은 `bTraceComplex=false` component trace를 수행해 8/8 hit를 확인했다.

이 실험은 Static Mesh 기하, runtime 회전값, complex-as-simple collision 입력의 조합을 검증한다.

### production PCG graph·Blueprint authoring 실험

테스트: `LootNPop.SurfaceNavigation.MeshTerrain.COptionLevelInstancePCGGraph`

- production LVI `/Game/Maps/Meadow_00/LVI_Octant_Meadow_00`의 저장 상태에는 production PCG graph, HISM 3개, 인스턴스 1964개가 존재했다.
- 임시 복제 LVI의 production BP actor에 C안 mesh를 대입하자 실제 Editor authoring world에서 같은 graph가 즉시 재생성됐다.
- 초기 C안 입력에서는 bounds 최대값 `(26996.842, 27075.174, 26983.242)`cm와 합계 2303개 HISM instance가 관측됐다. 이후 왕복 테스트가 기존 산출물에 스컬프트를 누적하던 결함이 확인되어 이 수치는 최종 기준선에서 제외했다.
- 동일 graph와 parameter를 transient Editor world에서 직접 실행하면 완료 상태지만 출력이 0개였다. PCG가 Editor authoring context에 의존하는 부분이 있어 이 headless 결과를 C안 실패로 해석하지 않는다.

상속된 Static Mesh component의 mesh를 LVI actor instance에서 바꾸는 방식은 저장·재로드 뒤 유지되지 않았다. Blueprint Construction Script가 class default를 다시 적용하기 때문이다. 이에 production BP를 복제한 `/Game/SurfaceNavigationTests/MeshTerrain/BP_Octant_COption`을 만들고 다음 기본값을 저장했다.

- crust mesh: `/Game/SurfaceNavigationTests/MeshTerrain/SM_COptionSphereSculpt`
- 당시 실험 Component Tags: `LNP.Terrain.Role.Support`, `LNP.Terrain.Role.PCGSurface`
- `bCanEverAffectNavigation=false`
- production PCG graph와 parameter 유지

자동화는 C안 BP를 transient non-WP world에 spawn해 위 mesh·tag·PCG graph와 실제 mesh에 일치하는 spatial input bounds를 검증한다. 실제 graph 출력은 전용 LVI의 안정된 1960개 결과로 별도 확인하고, production LVI의 저장 HISM baseline도 함께 검사한다.

왕복 테스트를 production 원본 geometry로 먼저 복원한 뒤 한 번만 스컬프트하도록 수정했다. 안정된 단일 스컬프트 산출물은 PCG spatial bounds 최대값이 25000cm이고 HISM 3개·1960개를 생성한다. 전체 자동화 5개를 연속 실행해 최대 정점 변위 2400.000000cm, seam 최대 변위 0cm, PCG bounds가 반복마다 동일함을 확인했다.

production BP의 crust component tag 배열은 현재 비어 있다. 따라서 이 실험은 C안 authoring 계약은 증명하지만 production content의 Terrain Contract tag 마이그레이션까지 완료한 것은 아니다. 저장되지 않는 임시 LVI 복제본은 제거했으며 production BP/LVI는 저장하지 않았다.

위 `LNP.Terrain.*` 이름은 Phase 1 실험 당시 값이며 현재 Terrain Contract가 아니다. Phase 4 입력으로 재사용할 C-option BP/LVI와 자동화는 `LNP.Surface.Support`, `LNP.Surface.Static` 및 필요 역할 조합으로 마이그레이션해야 한다.

#### 자동화 조건과 남은 위험

헤드리스 직접 입력에서는 기본 연결성 제한(`bRequireConnectivity=true`)으로 실제 brush ROI가 형성되지 않아 정점 변위가 0이었다. hover 갱신과 pressure 비활성화를 적용해도 같았고, `bRequireConnectivity=false`에서 실제 변위·Accept·저장 왕복이 성공했다. 이 차이는 headless 입력이 도구의 이전 hit triangle 상태를 대화형 viewport와 동일하게 구성하지 못하기 때문일 가능성이 있지만 아직 확정하지 않았다.

따라서 C안은 **비-WP 일반 Static Mesh에서 Sphere Sculpt와 독립 asset 저장이 가능함**을 증명했다. 다음 항목은 향후 authoring UX 보강 시 별도 검증한다.

- 대화형 Editor에서 연결성 제한을 유지한 실제 stroke

### 실제 8개 Level Instance 통합

실험 전용 산출물은 다음과 같다.

- `/Game/SurfaceNavigationTests/MeshTerrain/LVI_Octant_COption`
- `/Game/SurfaceNavigationTests/MeshTerrain/L_COptionEightSlotIntegration`

전용 LVI에는 production actor 대신 `BP_Octant_COption` 하나와 현재 단일 스컬프트 기준 HISM 3개·1960개를 저장했다. 통합 맵에는 runtime subsystem과 같은 `(Pitch 0°/180°, Yaw 0°/90°/180°/270°)` 회전으로 이 LVI 8개를 배치했다.

Simulate-In-Editor 결과는 다음과 같다.

- C안 BP actor 8개 모두 스트리밍 로드
- 각 actor의 crust mesh, `Support`·`PCGSurface` tag, `bCanEverAffectNavigation=false` 보존
- 각 actor PCG `bGenerated=true`, HISM 3개·1960개
- 총 HISM instance 15,680개
- 8개 octant 중앙 방향 world trace 8/8 hit
- trace 거리 25,168.210938~25,168.212891cm

`COptionEightLevelInstanceAsset` 자동화는 저장된 통합 맵의 LVI 8개, 정확한 slot 회전, C안 LVI 내부 actor·tag·PCG·HISM을 읽기 전용으로 검증한다. PIE 스트리밍 결과는 Unreal MCP로 별도 확인했다.

### Nanite 활성화와 Windows cook

2026-09-22에 C안 실험 mesh의 Nanite를 활성화하고 저장했다. 그 상태에서 `NonWPSphereSculptRoundTrip`을 포함한 Mesh Terrain 자동화 5개가 모두 통과했다. 따라서 Sphere Sculpt의 Accept·저장·재로드, seam, 8 slot 기하 조립, LVI asset, PCG 입력, complex-as-simple collision 계약이 Nanite 활성화 뒤에도 유지된다.

열린 Editor와의 MCP HTTP 포트 충돌을 피하기 위해 cook 프로세스에서는 `ModelContextProtocol` 플러그인을 비활성화했다. 다음 조건의 격리 Windows cook이 종료 코드 0과 `Success - 0 error(s), 1 warning(s)`으로 완료됐다.

- 대상: `/Game/SurfaceNavigationTests/MeshTerrain/SM_COptionSphereSculpt`
- 주요 옵션: `-TargetPlatform=Windows -CookSinglePackage -KeepSinglePackageRefs -SkipZenStore -DisablePlugins=ModelContextProtocol`
- 처리: 의존성 포함 277 package
- 현재 단일 스컬프트 commandlet 처리 시간: 3.51초
- 유일한 warning: 기존 `r.Shadow.Virtual.MaxPhysicalPages` 우선순위 경고

생성된 핵심 파일은 다음과 같다.

| 파일 | 크기 |
|:---|---:|
| `SM_COptionSphereSculpt.uasset` | 2,985바이트 |
| `SM_COptionSphereSculpt.uexp` | 417,278바이트 |
| `SM_COptionSphereSculpt.ubulk` | 5,093,896바이트 |

cook resource 통계에는 `StaticMesh` 4.10MiB, `BodySetup` 0.31MiB, `NavCollision` 0.05MiB, `DistanceField` 0.77MiB가 기록됐다. Editor asset의 `CTF_UseComplexAsSimple`과 physics triangle mesh data 보존 테스트를 함께 보면 C안 산출물이 Windows cooked collision을 직렬화할 수 있다는 근거가 된다.

### Development packaged runtime

테스트 `LootNPop.SurfaceNavigation.PackagedRuntime.COptionNaniteAndExactCollision`을 게임 모듈에 추가하고 `L_COptionEightSlotIntegration`을 포함한 Win64 Development 패키지를 생성했다. D3D12 `-RenderOffscreen` 실행에서 다음 계약을 직접 검증했다.

- cooked C안 Static Mesh object path 로드
- 유효한 render data와 Nanite data
- physics triangle mesh data와 `CTF_UseComplexAsSimple`
- 런타임 8 slot 회전으로 등록한 component에 대한 world scene query 8/8 hit

`NullRHI`에서는 Static Mesh render data를 역직렬화하지 않아 render·Nanite 검증에 사용할 수 없었다. 또한 cooked build의 `GetNumNaniteTriangles()`는 유효한 Nanite data가 있어도 0을 반환할 수 있어 성공 조건에서 제외했다. 최종 D3D12 오프스크린 실행은 `Test Completed. Result={Success}`와 `EXIT CODE: 0`을 기록했다.

패키징 과정에서 기존 Lyra Mannequin `M_Mannequin` 계열의 누락 Material Function 경고가 발생했고, 런타임에서는 해당 material이 default material로 대체됐다. 이는 C안 mesh의 render·Nanite·collision 검증과 무관한 기존 콘텐츠 문제로 분리한다.

Game 타깃에는 설치형 엔진의 `UnrealGame` 공유 바이너리와 호환되지 않는 `bWithPushModel=true` 강제값이 있었다. `bOverrideBuildEnvironment`는 Push Model 심볼 링크 실패를 만들고 `TargetBuildEnvironment.Unique`는 설치형 엔진에서 금지되므로 강제값을 제거해 Editor 타깃과 같은 엔진 기본 설정을 사용했다. Iris 모듈 구성은 유지되며 `LootNPop Win64 Development`와 BuildCookRun이 모두 성공했다.

## B안 WP authoring map 실험

2026-09-22에 `/Game/SurfaceNavigationTests/MeshTerrain/L_BOptionMeshTerrainAuthoring`을 별도 World Partition 제작 맵으로 만들었다. production 옥탄트 mesh를 Mesh Terrain Convert 도구로 3×3×1 `AMeshPartition`에 변환한 뒤 실제 Height Sculpt와 Boolean Subtract modifier를 적용했다.

Mesh Terrain의 최종 `UStaticMesh`는 독립 Content asset이 아니라 `PreviewSection` actor 아래의 `MeshPartitionStaticMesh_0` 중첩 object였다. 일반 UObject 복제는 저장 파일을 만들 수 있었지만 유효한 `StaticMeshDescriptionBulkData`를 만들지 못해 재로드 시 `Bad MeshDescription` assertion이 발생했다. 따라서 B안 추출기는 다음 데이터를 새 최상위 `UStaticMesh`에 명시적으로 다시 구성해야 했다.

- 중첩 결과의 LOD 0 `FMeshDescription`
- Static Material slot
- source model
- Nanite 설정
- `UBodySetup`, `CTF_UseComplexAsSimple`, double-sided collision

테스트 `LootNPop.SurfaceNavigation.MeshTerrain.BOptionIndependentAssetExtraction`은 `/Game/SurfaceNavigationTests/MeshTerrain/SM_BOptionExtracted`를 생성해 저장·package reload·object path 재로드를 두 번 연속 통과했다. 결과는 vertex 554개, triangle 755개, polygon 755개, material slot 1개이며 Nanite와 physics triangle mesh data를 보존했다. 직접 Asset Registry dependency는 `/Script/StaticMeshDescription`, `/Script/NavigationSystem`뿐이어서 authoring world와 Mesh Partition asset에 대한 runtime 의존성이 없다.

격리 Windows cook도 종료 코드 0과 `Success - 0 error(s), 1 warning(s)`으로 완료됐다.

| 파일 | 크기 |
|:---|---:|
| `SM_BOptionExtracted.uasset` | 2,802바이트 |
| `SM_BOptionExtracted.uexp` | 224,844바이트 |
| `SM_BOptionExtracted.ubulk` | 6,186,396바이트 |

cook resource 통계에는 `StaticMesh` 5.22MiB, `BodySetup` 0.31MiB, `NavCollision` 0.03MiB, `DistanceField` 0.68MiB가 기록됐다.

### C안과의 비교 결론

C안을 기본 제작 경로로 채택한다. C안은 기존 비-WP 옥탄트 구조에서 일반 Static Mesh를 직접 편집하고 그 asset을 곧바로 LVI·PCG·cook 입력으로 사용한다. 반면 B안은 별도 WP 맵, Mesh Partition 외부 actor/object 묶음, 중첩 결과를 새 Static Mesh로 재구성하는 커스텀 추출기, Component Tag 재부여가 추가로 필요하다.

B안은 실패한 경로가 아니라 보조 경로다. Boolean과 Mesh Partition modifier처럼 C안에 없는 기능이 제작상 반드시 필요할 때만 사용하고, runtime 산출물은 B안 authoring graph가 아니라 독립 `UStaticMesh`와 배치 component metadata로 제한한다.

### source hash 후보

stale 검출의 1차 후보는 Asset Registry의 `FAssetPackageData::GetPackageSavedHash()`가 제공하는 `FIoHash`다. UE 5.8은 package save 시 임의 GUID가 아니라 package byte hash를 기록하므로 별도 파일 timestamp보다 강한 변경 표식이다.

옥탄트 source hash는 다음 manifest를 package 이름으로 정렬한 뒤 hash를 다시 집계한다.

- source LVI package
- LVI가 직접 참조하는 external actor·external object package
- Terrain Contract 역할 component가 참조하는 Static Mesh package
- 역할 tag, component transform, collision profile처럼 베이크 의미를 바꾸는 authoring 값
- baker schema version과 bake settings

실험 LVI의 Asset Registry dependency에는 실제 `/Game/__ExternalActors__/.../LVI_Octant_COption/...` package가 포함됨을 확인했다. 따라서 OFPA actor 변경도 manifest에 포함할 수 있다. 전체 재귀 dependency를 무차별 포함하면 material thumbnail이나 PCG decoration 변경까지 불필요한 재베이크를 만들 수 있으므로, Phase 2에서는 역할 component를 기준으로 dependency를 필터링한다. `PackageSavedHash`는 20바이트이므로 현재 `FGuid SourceContentHash` 초안에 억지로 잘라 넣지 않고 `FIoHash` 또는 동등한 고정 크기 hash 저장 타입을 검토한다.
