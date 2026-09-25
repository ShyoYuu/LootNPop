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

## 2026-09-25 — 구현 단위 2: exact 접지 경로

### 패널 → Mass PrePhysics 선행 조건(D-050, C안)

- `ULNPDynamicTerrainSubsystem::RegisterPanel`·`UnregisterPanel`이 게시 틱과 같은 자리에서 Mass PrePhysics 페이즈 tick function(`UMassSimulationSubsystem::GetMutablePhaseManager().GetProcessingPhaseTickFunction`)에 패널 Actor 틱 선행 조건을 걸고 푼다. `MassSimulation` 모듈 의존성 추가, `InitializeDependency<UMassSimulationSubsystem>`으로 정리 순서를 건다.
- 엔진 확인: 페이즈 tick 재등록(`EnableTickFunctions`)은 `RegisterTickFunction`만 다시 부르고 선행 조건 배열은 멤버라 남는다. `Stop`은 enable만 끈다. 페이즈 `ExecuteTick`은 게임 스레드에서 `OnPhaseStart`를 먼저 방송한다.
- 검증 장치: `OnProcessingPhaseStarted(PrePhysics)`에서 등록 패널 중 이번 프레임(`GFrameCounter`)에 아직 틱하지 않은 수를 센다(`ALNPMovingPanel::GetLastTickFrame`). 월드 종료 시 `[DynamicTerrain] MassPrePhysicsOrder checks=… violations=…`.
- 2P 스모크 결과: 호스트 checks 42,096·게스트 40,976, **위반 0**. 클라이언트도 같은 순서다. B안(PostPhysics 이전)은 필요 없다.

### exact 이동(D-049)

- `Enemy/LNPEnemyExactMovement.*`: `StepGrounded`(수평 capsule sweep + 하향 support probe), `StepAirborne`(이전→제안 capsule sweep), `ProjectToSameLayer`(배회 재투영). 이동 프로세서와 자동화가 같은 함수를 부른다. 세부 규약은 Phase 문서 §3.3 "구현 규약".
- 이동 프로세서: `bExactGround`면 접지 분기의 캐시 경사 검사·표면 스냅을 건너뛰고 `StepGrounded`, `IntegrateAirborne`(넉백·사망 팝 공용)은 `StepAirborne`. Actor가 있는 적은 그대로 Mover다.
- 배회 목표(`FLNPEnemyIdleTask`): exact 모드에서 엔티티와 같은 반지름에서 위아래 300cm를 찍는다. 없으면(섬 가장자리 밖 등) 목표를 현재 위치로 두어 다음 추첨을 기다린다.
- CVar `LNP.SurfaceNav.EnemyExactGround`(기본 1, 스모크 뒤 변경)·`LNP.SurfaceNav.EnemyExactLateralSweep`(기본 1).

### 자동화에서 드러난 문제 두 가지

- 벽 사선 보행이 벽에 닿은 뒤 멈췄다. 처음에는 시작 겹침 sweep이 이동 전체를 막았고, 고친 뒤에는 벽에 딱 붙은 캡슐과 같은 반지름의 하향 probe가 벽을 먼저 맞혀 Rejected가 됐다(프레임별 위치 진단으로 확인). 겹침을 풀고 법선 성분만 지워 다시 sweep하고, 막힌 자리에서 1cm 물러나 멈추게 했다.
- 설계 중 발견: probe 구를 캡슐보다 작게 하면 절벽 끝에서 착지·지지면 상실이 반복될 수 있어 캡슐 바닥 구와 같게 했다.

### 검증

- `LootNPopEditor`·`LootNPop Win64 Development` 빌드 성공, 경고 없음.
- 자동화 `LootNPop.SurfaceNavigation` 21/21. 신규 `ExactMovement.*` 4개:
  - `GroundAndCliff`: 30cm 떠 있어도 접지, 캡슐 전체가 가장자리를 벗어난 뒤에만 낙하, 아랫판 1회 착지
  - `WallAndSlope`: 벽 정면 정지, 사선 보행이 벽을 따라 424cm/s로 미끄러짐, 25도 경사로 오르기, 60도 경사 못 오름
  - `IslandBody`: 섬 밑면에 막히고 착지하지 않은 채 바닥으로 복귀, 측벽을 따라 미끄러져 바닥 착지, 섬 윗면에서 밀려나 아래 착지, 배회 재투영(섬 아래→바닥, 섬 위→섬, 가장자리 밖→실패)
  - `CaveAndUnknown`: 천장(바닥 +400cm) 아래 보행 높이 불변, 수직 구멍으로 떨어져 아래층 착지, 미등록 판에 착지하지 않고 뚫지도 않음
- 에디터 바이너리 `-game` 리슨 2P 무인(양쪽 `-nullrhi`, `-corelimit=4`, `-LNPLoadBaseline=300`, 발사체 500, 호스트 `EnemyExactGround 1`):
  - 호스트: `UnknownHits=0`, `EnvelopeEscapes=0`, 락 합 P95 0.153ms PASS, 크래시·ensure 0. exact query 프레임당 약 1,000회(GroundRiskFallback 평균 3.86us). 프레임·exact 합계 FAIL은 에디터 바이너리 측정이라 판정하지 않는다(측정은 구현 단위 3에서 패키지로).
  - 게스트: 캡처 구간 `UnknownHits=0`, 전체 누적 1건(워밍업 구간, 게스트 query는 투사체 Ghost뿐). 게스트는 적 이동을 돌리지 않아 이번 변경과 무관하게 보이나 원인은 확인하지 않았다.
  - 경고는 이전 스모크와 같은 종류(Mover 시작 위치 불일치, `AttackTask` cast 실패)다.
- 스모크는 harness 합성 넉백이 아직 없어 공중 query가 15회뿐이다. 넉백·섬 배치 시나리오는 구현 단위 3에서 돌린다.

## 2026-09-26 — 구현 단위 3: 측정 harness 확장과 §3.6 매트릭스

### harness 확장(`SurfaceNavigation/LNPLoadBaseline.*`)

사용자 결정: 플레이어는 링 중심으로 옮긴다(추격·공격·Actor 승격 부하 유지), 합성 넉백은 고정 세기.

- 링 중심: slot 4 큰 섬(slot 로컬 (1,1,1), 월드 (−1,1,−1)/√3) 가장자리 아래 지각. 섬 중심에서 −Z 극 쪽으로 섬 각반지름(3,000/25,800rad)만큼 옮긴 방향이다. −Z 극 PlayerStart에서 약 25,000cm 떨어진다.
- 배치는 exact `ProbeSupport`로 층을 골라 찍는다. 지각은 반지름 27,700(섬 밑면 가장 깊은 27,300과 지각 최소 28,235 사이)에서 32,000까지, 섬 윗면은 25,000에서 27,000까지 찍는다. 20마리 중 2마리(인덱스 4 근접·15 원거리)가 섬 윗면이다. SurfaceCache는 섬 방향에서 섬 윗면만 알아 섬 아래 지각에 둘 수 없다.
- 플레이어: 준비가 끝나면 서버가 모든 플레이어를 UnPossess·Destroy한 뒤 `RestartPlayerAtTransform`으로 링 중심 옆에 다시 스폰한다(`ALNPGameMode::DoRespawn`과 같은 방식). 자리마다 exact로 지각을 찾는다.
- 합성 넉백: 서버가 접지한 PureEntity(Actor 없는 적)마다 포아송 과정으로 평균 10초에 한 번, 1,200cm/s(`ApplyEntityKnockback`, 방향 0.7·Up 0.3). 섬 윗면 적은 섬 중심 반대쪽, 나머지는 임의 접선 방향이다.
- 이벤트 counter(서버, capture 구간): Knockbacks, Falls(harness 넉백 없는 접지→공중, 즉 지지면 상실), Landings, IslandLeaves(섬 윗면에서 공중으로), IslandDrops(섬 윗면을 떠나 반지름 27,000 밖에 착지), LayerJumps(접지 상태로 한 프레임에 반지름이 500cm 넘게 변함). 분포는 IslandTop·UnderIsland·OpenCrust·Airborne을 capture 시작과 끝에 남긴다.
- capture 시작에 `ULNPMassWorldCollisionSubsystem::ResetStats`를 불러 분류별 count·평균이 capture 구간만 담게 했다. 보고 머리줄에 `ExactGround`·`LateralSweep` CVar 값을 넣었다.
- 발사체 주입 위치는 계속 SurfaceCache로 찍는다. exact로 찍으면 harness가 측정 대상 counter에 query를 더한다.

### 스모크에서 고친 것

- 링 중심 정방향에 Blocker 전용 Static 소스(slot 4, 반지름 30,756, 평평한 윗면, 프랍으로 추정)가 있어 probe가 실패했다. 스폰 쪽도 실패해 기본 반지름 30,000을 쓰고 있었다. 링 중심·플레이어 자리는 나선으로 찍어 첫 지지면을 쓴다(`FindCrustNear`). 적 배치는 원래 재시도가 있어 영향이 없었다.
- **Mover `FTeleportEffect`는 게스트 폰을 옮기지 못한다.** instant effect는 네트워크로 전달되지 않는다(엔진 주석: Chaos Mover만 복제). 서버만 넣은 경우, 게스트만 넣은 경우, 양쪽 모두 넣은 경우 세 가지 모두 서버에서 본 게스트 위치가 그대로였다. 호스트 폰만 옮겨졌다. 폰을 다시 스폰하는 방식으로 바꿨다.
- 두 번째 자리를 접선으로 300cm 옮긴 곳에 그냥 스폰하자 `SpawnActor failed because of collision`이 났고, 게스트가 폰을 잃었다. 자리마다 exact로 지각을 찾아 해결했다.
- unity 빌드 묶음이 바뀌며 `LNPEnemyExactMovementTest.cpp`의 익명 네임스페이스 `HalfHeight`·`Radius`가 다른 테스트와 엔진 헤더의 같은 이름을 가렸다(C4459). `TestHalfHeight`·`TestRadius`로 바꿨다.

### 측정 규약

- 패키지 Development(`Saved/SurfaceNavigationPhase3Package`), 같은 머신(AMD Ryzen 7 8845HS), 호스트·게스트 모두 `-nullrhi -corelimit=4`, 리슨 2P, 발사체 500, seed 1. 호스트 프레임이 서버 CPU 프레임이다.
- CVar는 `-dpcvars=LNP.SurfaceNav.EnemyExactGround=…,LNP.SurfaceNav.EnemyExactLateralSweep=…`로 넣는다. 로그는 `Saved/Profiling/Phase03b/`(gitignore).

### 결과

| 조건 | 서버 CPU 프레임 P50/P95 | exact/frame P50/P95 | query/frame P50 | 락 P95 | 게스트 P95 |
|:---|:---|:---|:---|:---|:---|
| 300 legacy | 7.67/8.40ms | 0.93/1.03ms | 500 | 0.041ms | 3.55ms |
| 300 exact | 10.06/**11.51ms** | 3.17/3.57ms | 938 | 0.075ms | 3.16ms |
| 450 exact | 12.90/**14.09ms** | 4.30/4.80ms | 1,144 | 0.089ms | 3.04ms |
| 500 exact | 14.45/**15.85ms** | 4.84/5.36ms | 1,224 | 0.095ms | 3.34ms |
| 550 exact | 15.60/**17.01ms** | 5.07/5.61ms | 1,284 | 0.102ms | 3.56ms |
| 1000 legacy | 13.97/15.61ms | 1.01/1.14ms | 500 | 0.047ms | 3.48ms |
| 1000 exact | 24.68/**26.49ms** | 8.81/9.62ms | 1,942 | 0.157ms | 3.20ms |
| 1000 exact, lateral 0 | 22.96/24.63ms | 7.25/7.89ms | 1,526 | 0.125ms | 3.47ms |
| 2000 legacy | 23.42/25.63ms | 1.22/1.36ms | 500 | 0.063ms | 3.76ms |
| 2000 exact | 42.96/**46.80ms** | 17.48/18.72ms | 3,429 | 0.271ms | 2.80ms |

- **exact 한계치(서버 CPU 프레임 P95 ≤ 16.6ms): 500마리.** 500은 15.85ms로 통과, 550은 17.01ms로 실패. 이분 탐색 3회(550·450·500).
- legacy 한계치는 1000과 2000 사이다(1000 P95 15.61ms). 이 시나리오의 legacy는 적이 섬 윗면으로 순간이동하므로 비교 기준일 뿐 정답 경로가 아니다.
- 모든 실행에서 `UnknownHits=0`, `EnvelopeEscapes=0`, ensure·crash 0. 적 300/300~2000/2000 배치.

분류별 비용(N=1000 exact, capture 구간 합을 프레임 수로 나눔):

| 분류 | query/frame | 평균 | 프레임당 합 |
|:---|:---|:---|:---|
| GroundRiskFallback(접지 수평·슬라이드·probe, 배회 재투영) | 1,272 | 5.64us | 7.18ms |
| AirborneMandatory(공중 sweep) | 170 | 3.71us | 0.63ms |
| ProjectileMandatory | 503 | 2.08us | 1.05ms |

- 접지 개체 1마리당 query 약 1.46회, 프레임당 약 8us다. 300·550·2000에서도 GroundRisk 평균은 5.6~5.7us로 일정하다. 비용이 적 수에 선형이다.
- 수평 sweep 기여(N=1000): query/frame −416, exact P50 −1.56ms, 프레임 P50 −1.7ms. exact 증가분의 약 20%다.
- exact와 legacy의 프레임 차(N=1000 P50 +10.7ms) 가운데 exact 합 증가는 약 7.8ms다. 나머지 약 3ms는 공중 개체가 늘어난 몫(분포 Airborne 약 12~15%, legacy 약 3~5%)과 다른 처리로 보이며 Insights로 나누지는 않았다.
- **관찰: 이동 프로세서는 `ForEachEntityChunk`(단일 스레드)라 exact query 합이 게임 스레드 임계 경로에 그대로 얹힌다.** 락 대기는 N=2000에서도 P95 0.27ms라 병렬화 여지가 있다. 3b는 "캐시 없는 최악 조건" 측정이라 최적화를 넣지 않았다(§3.3). 결정은 사용자에게 넘긴다.

이벤트(capture 30초):

| 조건 | Knockbacks | Falls | Landings | IslandLeaves | IslandDrops | LayerJumps | 시작 분포 IslandTop/UnderIsland/OpenCrust/Airborne |
|:---|:---|:---|:---|:---|:---|:---|:---|
| 300 legacy | 912 | 0 | 578 | 76 | 44 | **12** | 70/10/201/13 |
| 300 exact | 817 | 9 | 766 | 63 | 14 | **0** | 24/85/151/35 |
| 1000 legacy | 2,942 | 0 | 2,072 | 283 | 141 | **178** | 194/18/732/52 |
| 1000 exact | 2,570 | 6 | 2,368 | 198 | 32 | **0** | 90/167/620/119 |
| 2000 legacy | 5,807 | 0 | 4,096 | 423 | 190 | **368** | 278/18/1,604/95 |
| 2000 exact | 5,079 | 1 | 4,555 | 455 | 63 | **0** | 178/168/1,405/244 |

- **섬 아래 순간이동: exact 0, legacy 12~368.** legacy는 capture 시작 때 이미 섬 아래 적이 10~18마리뿐이다(exact 85~168). 배치 직후 첫 이동 프레임에서 섬 윗면으로 올라갔고, capture 중에도 섬 아래로 들어간 적이 계속 올라간다. 완료 조건 "섬 아래 지각의 적이 섬 윗면으로 순간이동하지 않음"을 충족한다.
- exact에서 섬 윗면을 떠난 적 가운데 16~23%가 아래 지각에 착지했다(IslandDrops/IslandLeaves). 나머지는 넉백 거리(약 4~5m)가 짧아 섬 위에 다시 착지했다. 착지 hit가 Unknown인 경우는 없다.
- Falls(넉백 없이 가장자리에서 걸어 나감)는 exact에서 1~11회다. 섬 위 적은 대부분 가장자리에 닿기 전에 공격 거리에서 멈춘다.

### 에디터 바이너리 스모크(참고)

- `-game` 리슨 2P 무인, 적 300: 두 플레이어 링 중심 560·627cm, 배치 300/300, `UnknownHits=0`. legacy(`-dpcvars`)는 LayerJumps 146, exact 0.

### 검증

- `LootNPopEditor Win64 Development`, 패키지 BuildCookRun(`LootNPop Win64 Development` 포함) 성공, 경고 없음.
- 자동화 `LootNPop.SurfaceNavigation` 21/21(자동화가 다시 저장한 `SM_COptionSphereSculpt`·`SM_BOptionExtracted`는 git으로 되돌림).

### 결정(사용자, 2026-09-26): 이동 병렬화를 3b에서 측정

- 단일 스레드 한계치 500은 최악 조건 기준선으로 남기고, 병렬 exact 한계치를 추가로 잰다(Phase 문서 §3.6.1). Phase 6에서 캐시와 병렬화 효과를 나눠 읽고, Phase 4 적중률 목표를 병렬 exact 기준으로 잡기 위해서다.
- 작업량이 약 1세션이라 새 세션에서 진행한다. 측정 스크립트는 `Scripts/Profiling/RunLoadBaselineMatrix.ps1`로 저장소에 옮겼다(`$args`는 `Where-Object` 블록 안에서 가려지므로 먼저 변수에 담는다).

## 2026-09-26 — 구현 단위 3b: 이동 병렬화 측정(§3.6.1)

### 구현

- CVar `LNP.SurfaceNav.EnemyParallelMovement`(기본 0, 서버 전용)와 `LNPEnemyExactMovement::IsParallelMovementEnabled()`를 추가했다.
- `ULNPEnemyMovementProcessor::Execute`: 청크 처리 람다를 하나로 두고, CVar가 1이면 `ParallelForEachEntityChunk(AutoBalance)`, 0이면 기존 `ForEachEntityChunk`로 부른다. 청크마다 exact query 수가 달라서(접지·공중·Actor 비율) 잡을 미리 나눠 주지 않고 빈 스레드가 가져가게 했다.
  - 청크 사이의 공유 쓰기는 `EntitiesToSignal`뿐이다. 청크 로컬 배열에 모았다가 청크 끝(`ON_SCOPE_EXIT`)에서 락을 잡고 합친다. 신호는 엔티티 단위로 쌓일 뿐 순서에 의미가 없다.
  - Actor 대상 deferred 명령은 엔진이 병렬 잡마다 command buffer를 따로 만들고 끝에 합친다(`bAllowParallelCommands` 기본 true). exact 통계는 원래 atomic이다.
- harness 보고 머리줄에 `ParallelMovement`, 둘째 줄에 `EnemyChunks`·`LargestChunk`를 넣었다. 병렬 잡 단위가 청크라서 청크 수가 병렬 이득의 상한이다(harness의 적 query 기준, 죽어가는 개체 청크는 빠진다).
- `RunLoadBaselineMatrix.ps1`에 `Parallel` 인자와 `N{500,650,700,750,800,850,1000,2000}_exact_par` 시나리오를 추가했다.

### 결과(패키지 Development, 구현 단위 3과 같은 측정 규약)

| N | 단일 스레드 P50/P95 | 병렬 P50/P95 | exact/frame P50(CPU 합) | 락 P95 | 청크 수 | 게스트 P95 |
|:---|:---|:---|:---|:---|:---|:---|
| 500 | 14.45/15.85ms | 11.99/**13.21ms** | 4.83ms | 0.111ms | 10 | 3.35ms |
| 650 | — | 13.80/**15.34ms** | 6.09ms | 0.133ms | 14 | 3.63ms |
| 700 | — | 13.95/**15.98ms** | 6.25ms | 0.136ms | 13 | 3.32ms |
| 750 | — | 15.15/**16.40ms** | 6.86ms | 0.148ms | 13 | 3.64ms |
| 800 | — | 16.75/**18.32ms** | 7.53ms | 0.158ms | 14 | 3.66ms |
| 850 | — | 17.30/**18.99ms** | 7.91ms | 0.161ms | 15 | 3.73ms |
| 1000 | 24.68/26.49ms | 19.48/**21.13ms** | 9.04ms | 0.185ms | 16 | 3.56ms |
| 2000 | 42.96/46.80ms | 33.45/**35.72ms** | 17.32ms | 0.326ms | 26 | 3.76ms |

- **병렬 exact 한계치(서버 CPU 프레임 P95 ≤ 16.6ms): 750마리.** 750은 16.40ms로 통과(여유 0.2ms), 800은 18.32ms로 실패. 조건마다 1회 실행이라 경계 부근은 ±50마리 정도의 흔들림을 감안한다. 단일 스레드 한계치 500은 최악 조건 기준선으로 그대로 둔다.
- 청크당 최대 103마리라 500마리에서도 잡이 10개다. 청크 수는 병목이 아니다.
- 병렬화로 줄어든 프레임 P50은 N=500 2.5ms, 1000 5.2ms, 2000 9.5ms로 exact CPU 합의 약 51~58%다. `-corelimit=4`에서 이상적인 몫은 75%다. 나머지는 잡 불균형과 병렬 진입 비용으로 보이며 Insights로 나누지는 않았다.
- exact CPU 합과 query 수는 단일 스레드와 같다(N=1000 9.04ms·1,952회 vs 8.81ms·1,942회). 병렬화는 총량을 줄이지 않는다.
- 락 대기는 worker 동시 query에서도 N=1000까지 P95 0.2ms 예산 안이고, N=2000에서 0.326ms로 처음 넘는다. 락 경합은 이 규모에서 병목이 아니다.
- 모든 실행에서 `UnknownHits=0`, `EnvelopeEscapes=0`, `LayerJumps=0`, ensure·crash 0. 이벤트 비율(IslandDrops/IslandLeaves 약 17~21%)은 단일 스레드와 같은 범위다.

### 검증

- `LootNPopEditor Win64 Development`, 패키지 BuildCookRun 성공(새 경고 없음, 기존 Mannequin material 경고만).
- 자동화 `LootNPop.SurfaceNavigation` 21/21(자동화가 다시 저장한 테스트 mesh 2개는 git으로 되돌림).
- 에디터 바이너리 `-game` 리슨 2P 무인(적 300·발사체 500, 병렬 1): 호스트·게스트 `UnknownHits=0`, `MassPrePhysicsOrder` 위반 0, LayerJumps 0, ensure·crash 0. 프레임은 에디터 바이너리라 판정하지 않는다.

### 결정(사용자, 2026-09-26): 병렬 이동을 기본값으로

- `LNP.SurfaceNav.EnemyParallelMovement` 기본값을 1로 바꿨다. 측정 스크립트는 모든 시나리오에 값을 명시하므로(`Parallel` 없으면 0) 단일 스레드 기준선은 그대로 재현된다.
