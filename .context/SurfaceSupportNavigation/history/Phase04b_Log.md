# Phase 4b 로그 — 부유섬·동굴 키트 다층 베이크

## 2026-09-27

### Phase 4 공통 전제 — 결정

- 사용자 결정 두 가지:
  - 회귀 공간을 나눈다. 정적 사례는 새 fixture LVI를 8 slot 회전으로 합성해 검사하고, 동적 사례 3종만 `L_SurfaceRegression`에 30,000cm로 남긴다(D-056, D-023 대체).
  - 동굴 키트 메시는 지금 C++ greybox 빌더로 만든다. 프로덕션 품질은 아트 패스에서 Blender로 만든다. 교체 비용을 낮추려고 에셋 규약(이름·피벗·소켓·단면/양면·clearance)과 규약 검사는 지금 고정한다(`design/TerrainContract.md` §6 "키트 에셋 규약"). 규약이 4b 베이크와 Phase 7 Nav를 거치며 바뀔 수 있어서, 제작 파이프라인을 먼저 다듬으면 두 번 만들게 된다는 판단이다.

### 공유 메시 도구와 동굴 키트

- 4a fixture 빌더의 메시 헬퍼를 `LootNPopEditor/SurfaceNavigation/LNPFixtureMesh.*`로 옮겼다. 삼각형 목록, 구면 옥탄트 격자, 면 선택 상자, complex-as-simple 저장(소켓 포함), level world 생성·저장이 들어 있다. 4a 빌더는 이 도구를 쓰도록 바꿨고 삼각형 생성 순서는 그대로다(4a 에셋은 다시 만들지 않았다).
- `FLNPFixtureMesh::RemoveInsideConvex`: 볼록 영역 안쪽을 평면으로 잘라낸다. 경계를 가로지르는 삼각형만 쪼개고, 새 정점은 모서리 양 끝을 정렬해 계산·공유해서 이웃 삼각형과 같은 점이 된다.
- `LNPCaveKit.*`: 직육면체 공동(`RoomBox`)과 경사 통로(`RampCorridor`), 각각 Floor/Shell 두 메시. `LNP.SurfaceNav.BuildCaveKit`이 `/Game/Maps/CaveKit`에 쓴다.
  - 공동 1600×1600×500, 바닥은 곡률 30,000 구면 캡, +X 벽에 400×350 문.
  - 통로 수평 2,400cm·20°, 단면 400×350. 소켓 `Door_0`·`Lower`·`Mouth`.
  - Shell은 양면, 벽은 바닥 아래 30cm까지 내려 접합부 틈을 막는다.
  - `PlaceUnderSphere`: 통로 `Mouth` 바닥점이 지정 반지름 구면에 오도록 공동 깊이를 닫힌 식으로 정한다.
  - 지각 입구 설계: 통로 내부 볼록 영역(두 벽·바닥·천장·양 끝 평면)과 겹치는 지각만 자른다. 벽 평면이 곧 구멍 경계라 틈이 없다. 통로 천장·벽은 지각 위로 솟은 입구 구간에도 그대로 둬서 지붕 덮인 입구가 된다. 지각 곡률·요철과 무관하게 입구가 닫힌다. Chaos 단순 query는 trimesh를 단면으로 보므로, shell을 단면으로 두면 지각 위로 솟은 벽이 바깥에서 뚫린다. 그래서 shell은 양면이어야 한다.

### 정적 회귀 fixture LVI

- `LNP.SurfaceNav.BuildRegressionFixture` → `LVI_Octant_Fixture_Regression`. 배치는 `LNPRegressionFixture.h`가 빌더와 자동화에 공유한다.
- 완전 구면 지각 N=256(삼각형 65,556개), 통로 입구 절단 34개 삼각형. 공동 바닥 반지름 30,713.0cm.
- 섬은 윗면(`SM_FixtureBoxTop`, Support+Blocker)과 몸체(`SM_FixtureBoxBody`, Blocker)로 나눴다. 옛 맵은 엔진 Cube 하나를 Support로 썼다.
- 액터 29개: exact source 15개, Decoration 14개.
- `BakeOctant` 결과: Support source 7개 중 지각 식별 성공. Invalid 61샘플이 입구 coverage hole이다. `DA_OctantSurface_Fixture_Regression`을 저장했다.

### 동적 사례 맵

- `Scripts/GenerateSurfaceRegressionMap.py`를 동적 3사례·(위도, 방위) 좌표·30,000cm로 바꿨다. 기둥 (30,20), 패널 (30,45), 파괴 바닥 (30,70).
- 헤드리스 재생성은 `LNP_REBUILD_SURFACE_REGRESSION_MAP=1 UnrealEditor-Cmd -ExecutePythonScript=<스크립트>`로 했다. 결과는 fixture 12개로 스크립트 검증을 통과했다.

### 자동화

- 신규 `WorldCollision.RegressionFixture`: 8 slot 전부에서 지각·섬 하나·섬 둘·가장자리·동굴 키트(바닥·천장·벽·문·입구·입구 옆 지각·외벽)·프랍·이음매 변 중점·꼭짓점을 검사한다.
  - 첫 실행에서 장식·Pawn 검사가 8.5cm 어긋났다. 접선 700cm 지점의 구면 지각은 방사 투영이 `R - 8.2cm`라서다. 평면 cube 시절 oracle을 그대로 쓴 테스트 오류였고, hit 점의 실제 반지름으로 비교하게 고쳤다.
- 신규 `Bake.CaveKitContract`: 저장된 키트 에셋의 cooked 충돌 삼각형으로 규약을 검사한다. 바닥 walkable·위 방향, shell 양면, 소켓이 바닥 위에 있음, clearance ≥ 250cm.
- `WorldCollision.RegressionMap`은 동적 3사례·12 fixture로 줄였다.
- `Bake.OctantBakeDeterministic`·`CrustAtlasExactError`·`CrustSeamMatch` 대상에 새 LVI를 추가했다. 결과는 유령 지면 0, 100cm 반지름 오차 P99 0.16cm, 이음매 반지름 차 0, 세 변 seam hash 동일.

### `Meadow_00` 동굴

- `LNP.SurfaceNav.PlaceCaveKit <Lat> <Az> <Heading> [Apply]`(`LNPCaveKitPlacer.cpp`), 열린 editor world 대상이다.
  - 지각은 D-055로 식별한다. 입구 방향의 실제 지각 반지름을 광선으로 재서 `PlaceUnderSphere`를 반복해 맞춘다.
  - 공동 천장 위 지각 두께(최소 100cm), 통로 안 지각 범위, 입구 주변 PCG 프랍 수를 보고한다.
  - Apply는 지각 메시를 `FMeshPlaneCut::SplitEdgesOnly`로 쪼개고 통로 내부 삼각형을 지운 뒤 키트 액터 4개(`CaveKit_00_*`)를 배치한다. 분할은 `EdgeFilterFunc`로 통로 영역 400cm 안 모서리에 한정했다. 이음매 정점은 바뀌지 않고 UV·법선은 보간된다. 퇴화 모서리 collapse는 껐다.
- 후보 10곳 미리보기. `Meadow_00` 지각 기복이 커서 대부분이 지붕 두께에서 탈락했다(최소 -1,100cm~82cm).
  - 채택: (15°, 60°, 방향 위도 증가). 지붕 666cm, 입구 절단은 통로 x ≥ 1,499cm(입구 2,400), 입구 주변 프랍 4개, 가장 가까운 꼭짓점과 약 33°.
  - 탈락 사례: (18°, 72°)는 37cm, (18°, 30°)는 82cm, (16°, 80°)는 193cm로 통과했지만 이음매에 더 가깝다.
- 적용 결과: 지각 삼각형 91개 제거(237,606 → 237,841, 분할로 늘어남). 재베이크 결과 Support source 11개, Invalid 42샘플이 입구다. 조회 NeedsExact 2.35%, 반지름 오차 P99 1.06cm로 이전과 같고, 이음매 반지름 차는 0이다.
- PCG는 `GENERATE_ON_LOAD`이고 지각 컴포넌트에만 광선을 쏜다. 그래서 입구 구멍에는 프랍이 생기지 않는다. 지붕 덮인 입구 옆 프랍은 막지 못한다. PCG 결과(BP 외부 액터)는 저장하지 않았다.
- 신규 `WorldCollision.MeadowCaveKit`: `Meadow_00`의 지각과 키트 액터만 복제해 공동 바닥, 입구 끝 지각 높이, 입구 통과, 입구 옆 지각 잔존을 검사한다. 입구 바닥점에서 중심 쪽 ray는 지각보다 입구 천장에 먼저 맞으므로, 입구 끝 100cm 바깥 지각과 비교한다.

### Conditional Patch 마커 로컬 공간 규약

- `design/DynamicTerrain.md` §5에 좌표 3단계를 확정했다. authoring은 마커 로컬, bake는 옥탄트 로컬, runtime은 `(slot, MarkerId)` 활성화다.
- 확정한 내용:
  - patch는 base 인덱스로 저장한다.
  - patch Support는 4b sparse Atlas를 재사용하는 추가 Layer이고, ID는 MarkerId 순으로 정한다.
  - 파괴 바닥의 온전한 Support도 patch다.
  - 활성화는 상태 값만 본다.
  - 정적 Layer와 캡슐 높이 안쪽에서 겹치면 베이크 오류다.

### 검증

- `LootNPopEditor Win64 Development` 빌드 성공.
- 자동화 `LootNPop.SurfaceNavigation` 46개. 최종 실행에서 45개 통과, `WorldCollision.Api` 1개 실패.
  - 실패 내용은 "Some raycasts ran off the game thread"다. `ParallelFor`가 워커를 받지 못해 전부 게임 스레드에서 돈 경우다. 단독 재실행 2회 모두 통과했다. 이번 변경과 무관한 기존 간헐 실패다.
  - 이 실행 전 같은 날 전체 실행 2회에서는 모두 통과했다(45/45, `MeadowCaveKit` 추가 전).
- 자동화가 다시 저장한 `SurfaceNavigationTests/MeshTerrain` 두 에셋은 매번 git으로 원복했다.
- D-031 2P 스모크(에디터 바이너리 `-game` 리슨 `TestMap03?Listen`, 게스트 접속 후 약 2분 30초, 로그 `Saved/Logs/Smoke4b_Host.log`·`Smoke4b_Guest.log`): 8 옥탄트 로드와 게스트 접속 성공. 호스트·게스트 ensure·크래시·`LogLootNPop` 오류 0. 오류는 기존 종류(엔진 Experimental 툴셋 Python 초기화, `CharacterMovementComponent` 추출 실패)뿐이다. 키트 메시·지각 메시 로드 경고 없음.
- 사용자 PIE: `Meadow_00` 동굴에 걸어 들어가 내부 바닥·벽면 사격을 확인했다. 입구 통행을 막는 프랍은 없었다.
- 사용자 결정: 입구 주변 프랍 간섭은 옥탄트 양산 때 충분히 생길 수 있으므로 PCG 제외 구역을 만든다(시점 미정, `Current.md` 이관 항목).
