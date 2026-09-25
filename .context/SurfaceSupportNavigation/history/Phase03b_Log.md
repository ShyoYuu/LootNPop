# Phase 3b 작업 기록

> 상태: 진행 중
> 실행 문서: `../phases/Phase03b_ExactFloatingIslandPrototype.md`

## 2026-09-25 — 착수 전 결정(사용자)

- 부유섬은 옥탄트 LVI 안에 배치하는 옥탄트의 일부다(D-048). 옥탄트와 섬을 따로 만들어 조합하면 산 높이와 섬이 겹치지 않도록 대역을 나누는 등 제약이 늘고 레벨 디자인 의도가 흐려진다. 섬은 옥탄트 경계면에 걸치지 않는다. 섬 모양은 계단·벽 등 자유롭고, 기본 보행면은 평면이 아니라 구면 곡면이다. 오르는 수단은 스프링 런처와 훅 앵커다.
- 테스트 옥탄트는 큰 섬 1개와 작은 섬 2개. 섬 하나는 지각과 경사로로 잇는다.
- 기존 25,000cm Meadow_00은 최종 삭제한다. 백업하면 이름을 바꾼다. 30,000cm 새 Meadow_00을 만든다. PlayerStart는 남극 극지방 주변.
- PureEntity exact 이동은 하향 probe + 수평 sweep(선택지 B, D-049).
- CVar 기본값·적용 범위와 측정 매트릭스는 Claude 판단에 맡김 → Phase 문서 §3.4·§3.6 권장안으로 확정.
- 패널과 Mass PrePhysics 순서는 지금 확정한다. 엔진 tick 선행 조건(C안)을 먼저 검토하고, 보장이 안 되면 PostPhysics 이전(B안)을 별도 세션으로 한다(D-050).
- 재미 평가는 사용자가 한다. Claude는 카메라 충돌·탄도 가이드 끊김 같은 기능 점검만 한다.

### C안 가능성 검토

- `FMassProcessingPhaseManager::GetProcessingPhaseTickFunction`은 public이고 `FTickFunction&`를 돌려준다(`MassProcessingPhaseManager.h`). `UMassSimulationSubsystem::GetMutablePhaseManager()`로 접근한다.
- `ULNPDynamicTerrainSubsystem::RegisterPanel`·`UnregisterPanel`이 이미 게시 틱에 패널 틱 선행 조건을 걸고 푼다. 같은 자리에 Mass PrePhysics 페이즈 tick function을 추가하면 된다.
- 패널 틱은 선언된 선행 조건이 없어 사이클이 생기지 않는다. 재등록 시 선행 조건 보존과 실제 실행 순서는 구현 단위 2에서 확인한다.

## 2026-09-25 — 구현 단위 1: 30,000cm Meadow_00과 부유섬

### 레거시 처리

- `Meadow_00` 이름 변경은 소스 문자열 참조(`LNPMeshTerrainTargetTest.cpp`, `LNPOctantSurfaceDataTest.cpp`) 때문에 엔진 확인 대화상자가 뜨고 MCP에서 자동 취소된다. 백업 에셋 없이 `Meadow_00`을 제자리에서 30,000cm로 갱신했다. 25,000cm 원본은 git 이력(`6345b2b`)에 있다.
- 두 Mesh Terrain 테스트는 production 옥탄트의 구조(BP 액터 1개·PCG 그래프·ISM 인스턴스)만 보므로 새 옥탄트에서도 유효하다(자동화 17/17).

### 지각

- `BP_OctantGenerator` Radius 30,000, 나머지 파라미터 유지(Subdivisions 200, Magnitude 18,000, Frequency 0.0002) → `/Game/Maps/Meshes/SM_Octant_Meadow_00_R30000`. 정점 120,000, bounds (0~30,000)³, `CTF_UseComplexAsSimple`, Nanite 끔(기존과 같음).
- `AssetPathAndName`은 `/Game/` 접두사가 있어야 저장된다. 가이드 예시(`Maps/Meshes/...`)로는 아무것도 만들어지지 않는다.
- 지각 기복: 반지름 28,235~31,766cm(옥탄트 중심이 가장 거칠고 이음매 쪽은 30,000 근처). 정점 간격 약 100~150cm.
- `BP_Octant_Meadow_00` 지각 템플릿 교체, LVI PCG 재생성(Seed 42→43→42).
- `DefaultGame.ini` `SphereRadius=30000`. `TestMap03` PlayerStart 4개 z −24,000 → −29,000(지각에서 1,000cm 여유 유지). WP 맵이라 `save_actor`·external 경로 `save_assets`가 모두 실패해 dirty 전체 저장을 썼다.
- 움직이는 패널 마커: 같은 방향 새 지각(r 30,199)에서 20cm 안쪽으로 이동. `set_actor_transform`은 location만 넘기면 회전을 0으로 초기화하므로 회전을 함께 넘겨야 한다.

### 부유섬(블렌더 MCP, `Scripts/Blender/GenerateFloatingIslands.py`)

- 모든 정점을 월드 중심 기준 방향+반지름으로 정의해 윗면이 구와 동심인 구면 곡면이다. 피벗은 윗면 중심, 로컬 +Z=Up, +X=위도 증가(적도) 방향. UE (x,y,z)cm → Blender (x,−y,z)/100 m, FBX 임포트 뒤 bounds로 방향과 배율을 확인했다.
- 섬 하나를 닫힌 메시로 만들어 법선을 맞춘 뒤 `Top`(`Support+Blocker+Static`, `LNPStaticTerrain`)과 `Body`(측벽·밑면, `Blocker+Static`, `LNPStaticBlocker`)로 나눴다(TerrainContract §5).

| 섬 | 방향 θ/φ | 윗면 반지름 | 섬 반지름 | 밑면 깊이 | 지각 대비 |
|:---|:---|:---|:---|:---|:---|
| Big | 54.74°/45° | 25,800 | 3,000 | 1,500 | 평균 위 약 4,200, 최고 지각과 여유 935cm |
| A | 35°/20° | 28,580 | 1,000 | 700 | 약 1,500, 여유 약 470cm |
| B | 35°/70° | 27,430 | 1,000 | 1,500 | 약 2,500, 여유 약 980cm |

- 섬 A 경사로: 25°, 폭 500cm. 적도 쪽은 지각이 경사로와 거의 나란히 내려가 4,500cm를 가야 묻혀서 **극 쪽(−X)**에 붙였다. 약 3,400cm에서 지각과 만나고 300cm 더 묻힌다(상승 약 1,050cm). 적도 쪽(+X)은 Sculpt 언덕 비교 자리다.
- 섬 B: 엔진 Cube 계단 3단(30cm 단차·60cm 단)과 300×300×120cm 플랫폼, L자 벽(800·470cm, 높이 300cm).
- 배치 검증 트레이스 13개가 설계값과 5cm 이내로 일치.

### 월드 장치 마커

- 런처·앵커가 `ILNPPlacedElement`를 구현하지 않아 마커 `ElementClass`로 쓸 수 없었다. 두 클래스에 구현을 추가하고, `AnchorID`는 시드 배치와 마커 배치가 `ULNPWorldDeviceSpawnSubsystem::AllocateAnchorID` 카운터를 공유한다.
- 현재 설정(속도 4,500, 45°, 중력 2,000)의 런처 정점 높이는 약 2,530cm다. 런처는 섬 A만, 앵커는 섬 B·큰 섬을 맡는다(사용자 결정). 런처별 속도·각도 입력은 후속 작업이다.
- 앵커 4개(큰 섬 가장자리 양쪽, 섬 B 두 곳): 가장자리 50cm 안쪽·윗면 600cm 위. 처음 200cm 안쪽·300cm 위에서는 큰 섬 앵커의 지상 시선이 섬 가장자리에 막혔다. 가장자리 밖 1,000~2,000cm 지상 눈높이에서 거리 3,091~4,829cm, 시선 막힘 없음(큰 섬 극 쪽 1,000cm만 5,049cm).
- 런처 1개: 섬 A에서 −Y(φ 증가) 쪽 8,300cm 지각(r 29,038), 섬 A를 향함.

### 검증

- `LootNPopEditor Win64 Development` 빌드 성공.
- 8 slot 생성, `[DynamicTerrain] Spawned 48 marker elements from 9 levels (0 skipped)`.
- `ExactOracle`(에디터 바이너리 `-game` 단독): Miss·Unknown·StartPenetrating·ShapeOrder·SlotMismatch 모두 0, EdgeMiss 822(정보). **ExactDeeper 20으로 FAIL**. 20개 모두 큰 섬·섬 B 가장자리 바로 바깥 방향이며, exact는 8 slot에서 같은 지각 반지름을 맞히고 legacy SurfaceCache는 이웃 셀의 섬 윗면을 보간해 slot마다 1,000~2,000cm 얕다. legacy 쪽 결함(첫 hit 단층 캐시)이다.
- audit(PIE): `Ok=205 MISSING=0 ExactOnly=0 NonBlocking=0 NoBody=149`. `EnvelopeRadius=32,636cm`(지각 깊은 골). int16 캡은 성분별이고 지각 메시 성분 최댓값은 30,000cm라 여유가 있다.
- 자동화 `LootNPop.SurfaceNavigation` 17/17.

## 2026-09-25 — 섬 A 경사로 비교: Mesh Terrain Sculpt 조사와 지각 일체형 언덕

### 판정 기준 변경

- `ExactOracle`의 ExactDeeper를 실패에서 정보 항목으로 강등(사용자 결정). 부유섬이 있으면 첫 hit 단층 SurfaceCache가 섬 가장자리 바깥 방향에 이웃 셀의 섬 윗면을 보간해 exact가 맞아도 생긴다. legacy 캐시를 제거하는 Phase 6까지 개수만 출력한다.

### Sculpt 도구 자동 구동

- `LNP.MeshTerrain.SphereSculpt`(`Source/LootNPopEditor/Tools/LNPSphereSculptCommand.cpp`): Phase 1 자동화와 같은 `UHeightSculptTool`을 경로 스트로크로 구동한다. 구 중심에서 경로 점으로 쏘는 ray를 쓰고, 도구는 자기 대상 메시에만 hit test하므로 사이의 섬에 가리지 않는다. `Invert=1`이 내부형 구에서 지면을 올린다(중심 쪽). `Reference=Sphere|Plane`, `Commit=0`은 미리보기다.
- **Sphere 기준면 엔진 제약**: 브러시 영역이 구 중심에서 `max(메시 bounds 최대 변, 1000)` 높이의 원기둥이다(`MeshVertexSculptTool.cpp` CylinderOnSphere). 멤버 `InitialBoundsMaxDim`은 private이라 바꿀 수 없다.
  - 섬 메시(bounds 약 2,000cm, 중심에서 28,580cm): 정점 0개 이동. 섬 피벗을 월드 중심에 둔 시험 메시도 같았다. 에디터 UI에서도 같다.
  - 지각(bounds 30,000cm): 반지름 30,000cm를 넘는 정점은 편집되지 않는다. 섬 A 가장자리 주변 지각(약 30,000~30,080)이 고정돼 언덕이 가장자리까지 이어지지 않았다.
  - 구 중심을 섬 피벗에 두면 움직이지만 피벗에서 뻗는 방향(수평)으로 당겨져 틀리다.
- **Plane 기준면**: 영역이 무한 원기둥이라 제약이 없다. 섬 윗면에서 방사 정렬 1.000으로 올라갔다. 기즈모 위치는 월드 좌표, 회전은 대상 컴포넌트 로컬 기준으로 쓰인다(회전된 섬에서 실측). 따라서 에디터 UI에서 섬은 Plane 모드 기본 설정("Initialize From Target")이면 섬 Up 축으로 올바르게 편집된다.
- 브러시 자동화로 언덕을 만드는 것은 중단했다. Plane Sculpt로 중심선은 목표 경사 ±40cm까지 맞췄지만 단면이 칼날 능선이었고, Plane Flatten은 중심선을 300~530cm 솟게 했다. 보면서 다듬는 도구를 트레이스·스트로크 반복으로 조종하기 어렵다.

### 지각 일체형 언덕(`LNP.MeshTerrain.RadialRamp`)

- `Source/LootNPopEditor/Tools/LNPRadialRampCommand.cpp`: 월드 중심 구면 좌표에서 시작 방향과 진행 방향이 이루는 대원을 경로로, 진행 호 길이·옆 호 길이로 목표 반지름을 계산해 지각 정점을 옮긴다. 지면은 올리기만 해서 발끝과 옆면이 원래 지형과 만난다. 옮긴 뒤 법선·탄젠트 재계산, `CommitMeshDescription`·`Build`.
- 섬 A 적도 쪽: 시작 = 가장자리 윗면 1cm 아래(반지름 28,581), 경사 25°, 윗면 폭 500cm, 옆면 35°, `Behind=0`. 정점 1,238개, 최대 1,401cm 상승.
- 트레이스 검증: 중심선 1,050~4,100cm에서 목표와 0~3cm, 폭 ±200cm 0~8cm, ±500cm는 설계대로 약 180cm 낮음.
- 지각 메시 저장, LVI PCG 재생성.

### 부수 기록

- `LootNPop.SurfaceNavigation` 자동화를 돌리면 Mesh Terrain 실험 테스트가 `SM_COptionSphereSculpt`·`SM_BOptionExtracted`를 다시 저장한다. 자동화 뒤 두 파일을 git으로 되돌린다.
- 지각 메시를 다시 빌드하면 `BP_Octant_Meadow_00`이 dirty로 표시된다. 저장할 변경은 없다.

### 오라클 재실행

- 언덕 반영 뒤 `ExactOracle`(에디터 바이너리 `-game`): Miss·Unknown·StartPenetrating·ShapeOrder·SlotMismatch 0, 정보 EdgeMiss 824·ExactDeeper 20. **Result=PASS**.

## 2026-09-25 — 플레이 확인(사용자)과 간이 동굴

### 사용자 PIE 확인

- PIE 시작 위치는 `EditorAppToolset.StartPIE`의 `startTransform`으로 지정했다. slot 0은 회전이 없어 LVI 좌표가 곧 월드 좌표다. 적은 `LNP.Spawn.EnemyDensity 0`(에디터 콘솔, PIE 전)으로 없앴다(LootPod 118개 유지, 0.1이면 적 약 120).
- 섬 A: 두 경사로 모두 오르내림에 불편이 없었다. 언덕 위 PCG 프랍이 통행을 막지 않았으나 우연일 수 있다. **두 방법을 모두 남긴다**(D-051). `Meadow_00`은 현재 상태를 유지한다.
- 섬 B: 장거리 훅이 어색함 없이 동작했다.
- 큰 섬: 앵커로 오르내림 확인. 앵커가 가장자리 바로 위라 지상에서 올려다보는 각이 약 77°로 가파르다. 처음에는 앵커를 찾지 못했다.

### 간이 동굴(블렌더 경사로, 극 쪽)

- `GenerateFloatingIslands.py`에 `RAMP_A["cutouts"]` 추가. 경사로를 닫힌 메시 하나로 만들고 Boolean(Exact)으로 파낸 뒤, 월드 중심을 향한 면(dot > 0.5)만 윗면으로 다시 분류한다. 파낸 면은 모두 몸체(Blocker)다.
- A 관통 터널: 섬 중심에서 1,900~2,300cm, 경사로를 옆으로 관통, 천장 29,359(지면 약 +400cm). B 방: 1,300~1,600cm, 한쪽 옆면(런처 반대편)으로 열림, 천장 28,929. B 수직 구멍: 1,400~1,600cm × 폭 200cm, 윗면에서 방 바닥(지각)까지 약 11m 낙하.
- 트레이스: 구멍은 윗면에서 바닥까지 열림, 터널은 바닥 +150cm에서 옆으로 관통, 방은 열린 쪽만 통과, 천장은 아래에서 막힘(29,359·28,929).
- `import_file`은 같은 이름 덮어쓰기를 거부한다. 새 이름 `SM_Ramp_A_Cave_Top`·`SM_Ramp_A_Cave_Body`로 임포트하고 액터 메시를 교체한 뒤 옛 메시를 삭제했다.
