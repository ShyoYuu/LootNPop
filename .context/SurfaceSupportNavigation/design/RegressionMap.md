# Surface Support 공통 회귀 공간

> 상태: 기준 설계
> 정적 사례: fixture LVI `/Game/Maps/SurfaceNavigation/Fixtures/LVI_Octant_Fixture_Regression`(생성 명령 `LNP.SurfaceNav.BuildRegressionFixture`)
> 동적 사례: 일반 레벨 `/Game/Maps/SurfaceNavigation/L_SurfaceRegression`(생성 스크립트 `Scripts/GenerateSurfaceRegressionMap.py`)

## 1. 구성과 좌표 규약

회귀 공간은 두 개로 나뉜다(D-056).

| 공간 | 담는 사례 | 이유 | 자동화 |
|:---|:---|:---|:---|
| 정적 fixture LVI | 지각·부유섬·섬 가장자리·동굴 키트·정적 프랍·이음매·꼭짓점 | 옥탄트 베이크와 8 slot 회전 경로를 그대로 탄다 | `WorldCollision.RegressionFixture`, `Bake.*`(결정론·이음매·Atlas 오차) |
| `L_SurfaceRegression` | 상태형 기둥·움직이는 패널·파괴 바닥 | LVI에는 동적 source를 둘 수 없다(D-026). Phase 8에서 마커 요소로 바꾼다 | `WorldCollision.RegressionMap` |

공통 규약:

- 월드 중심은 `(0, 0, 0)`, 기준 지각 반지름은 30,000cm다(D-046).
- 좌표는 source 옥탄트(+X,+Y,+Z) 로컬이다. 사례 위치는 (위도, 방위)로 적는다. 위도는 적도 `z=0`에서 +Z 쪽, 방위는 +X에서 +Y 쪽 각도다.
- 모든 사례는 위도 25~40° 안에 둔다. 이음매·꼭짓점과 int16 복제 캡에서 떨어뜨리기 위해서다. 이음매·꼭짓점 사례만 예외다.
- 사례 틀: Radial은 중심에서 바깥쪽 단위 벡터, Tangent는 방위 증가 방향, Bitangent는 위도 증가 방향이다. 캐릭터 Up은 `-Radial`이다.
- 구분용 geometry 대신 `Probe_<Case>` 이름의 `LNPDecoration` 구체를 debug 위치로 둔다.
- `NavComponent` 이름은 Phase 7 전까지 테스트 oracle용 상징 이름이다. 직렬화 ID를 미리 고정하지 않는다.
- 좌표 원본은 코드다. 정적 사례는 `LootNPopEditor/SurfaceNavigation/LNPRegressionFixture.h`, 동적 사례는 생성 스크립트다. 이 문서의 표는 요약이다.

## 2. 공통 기대 결과

- `LNPSurfaceSupport`는 `Support` source만 맞힌다.
- `LNPWorldExact`는 Support 여부와 무관하게 `Blocker`를 맞힌다.
- `Decoration`은 두 channel에서 모두 hit하지 않는다. Pawn profile도 exact query에서 제외된다.
- 정적 layer 수에는 `Dynamic`, `StatefulTraversal`, `Destructible` source를 포함하지 않는다.
- 원거리 타게팅 가능은 fixture 사이의 실제 LoS가 아니라 해당 시나리오에서 blocker가 없다는 전제의 제품 기대값이다.
- Unknown hit는 0이다.

## 3. 정적 사례와 oracle

fixture LVI 한 개를 8 slot 회전으로 테스트 월드에 복제하고 **모든 slot에서** 같은 기대값을 검사한다. 지각은 노이즈 없는 완전 구면(격자 N=256, 평면 삼각형 반지름 오차 0.4cm 이하)이라 기대 반지름은 30,000cm다. 섬은 윗면(`Support+Blocker`)과 몸체(측벽·밑면, `Blocker`)가 별도 component다(`TerrainContract.md` §5). 섬 두께는 300cm다.

| 사례 | (위도, 방위) | 형상 | 정적 Support Layer | exact 기대 | NavComponent | 지상 추격 | 원거리 타게팅 |
|:---|:---|:---|:---:|:---|:---|:---|:---|
| 기본 지각 | (30, 8) | 지각 | 1 | 바깥 ray가 R에서 지각 hit, 법선은 중심 방향, Static Support | `NC_Crust` | 같은 지각에서 가능 | LoS 시 가능 |
| 부유섬 하나 | (32, 18) | 섬 1300×1300, 윗면 28,000 | 2 | 윗면 hit(Support), 섬 밑에서 출발하면 지각 hit | `NC_Island1`, `NC_Crust` | layer 간 불가 | LoS 시 가능 |
| 부유섬 둘 | (32, 30) | 안쪽 섬 1100, 윗면 26,400 / 바깥 섬 1400, 윗면 28,200 | 3 | 안쪽 섬 → 바깥 섬 → 지각 순서로 구분 | `NC_Island2A`, `NC_Island2B`, `NC_Crust` | layer 간 불가 | LoS 시 가능 |
| 섬 가장자리 안쪽 | (32, 42) | 섬 1600(위도)×1200(방위), 윗면 28,000 | 1 | 접선 550에서 support probe 성공 | `NC_EdgeIsland` | 섬 내부 가능 | LoS 시 가능 |
| 섬 가장자리 바깥쪽 | (32, 42) | 같은 섬 | 0 | 접선 750에서 support miss, 측벽 sweep은 접선 600에서 Blocker만 hit | 없음 | 불가 | LoS 시 가능 |
| 동굴 키트 | 공동 (28, 56) | §4 | 공동 바닥 1, 통로 바닥 1 | 바닥 Support, 천장·벽 Blocker만, 문 너머 통로 바닥, 입구에서 지각 없음 | `NC_Cave` | 입구 통로로 외부와 가능 | 천장·벽 LoS 반영 |
| 정적 프랍 | (32, 72) | 나무(원기둥 지름 180, 접선 -350), 바위(비균일 구, 접선 350), 장식(접선 700) | 1 | 나무·바위 Blocker만 hit, 장식·Pawn은 통과해 지각 hit | `NC_PropGround` | blocker 우회 시 가능 | blocker LoS 반영 |
| 이음매 변 중점 | 세 변의 중점 | 지각 | 1 | 경계 ±5cm에서 반지름 R, 경계를 가로지르는 sweep에 턱 없음 | `NC_Seam` 하나 | 경계 통과 가능 | LoS 시 가능 |
| 꼭짓점 | 세 좌표축 | 지각 | 1 | 축에서 수 cm 떨어진 ray가 반지름 R에서 hit | `NC_Seam` 하나 | 경계 통과 가능 | LoS 시 가능 |

지각 Atlas도 이 LVI로 굽는다(`DA_OctantSurface_Fixture_Regression`). 자동화 `Bake.OctantBakeDeterministic`·`CrustSeamMatch`·`CrustAtlasExactError`가 저장본 일치, 이음매 일치, Atlas–exact 오차를 검사한다. 2026-09-27 기준 유령 지면 0, 100cm 해상도 반지름 오차 P99 0.16cm, 이음매 반지름 차 0이다. 동굴 입구는 Atlas에서 coverage hole(Invalid 61샘플)이다.

## 4. 동굴 키트 사례

키트 규약은 `TerrainContract.md` §6, 치수 원본은 `LootNPopEditor/SurfaceNavigation/LNPCaveKit.h`다.

- 공동: 직육면체 1600×1600, 높이 500. 바닥은 곡률 반지름 30,000의 구면 캡이다. +X 벽에 폭 400·높이 350 문이 있다.
- 통로: 문에서 위도 증가 쪽으로 수평 2,400cm, 경사 20°로 오른다. 단면 400×350이다. 입구 쪽 끝 바닥점이 지각 반지름에 오도록 공동 깊이를 정한다(`LNPCaveKit::PlaceUnderSphere`). 공동 바닥 반지름은 약 30,713cm, 천장은 약 30,213cm라 지각 아래 약 213cm 두께의 지붕이 남는다.
- 지각 입구: 지각에서 통로 내부 볼록 영역(두 벽·바닥·천장·양 끝 평면)과 겹치는 부분만 평면으로 잘라낸다. 통로 벽 평면이 곧 구멍 경계라 틈이 없다. 통로 천장·벽은 지각 위로 솟은 입구 구간에도 그대로 있어 지붕 덮인 입구가 된다. shell은 양면이라 바깥에서도 막는다.

| 검사 | 기대 |
|:---|:---|
| 공동 중심(바닥 위 250)에서 아래 | 공동 바닥(로컬 z=0), Support |
| 위 | 천장(로컬 z=500), Blocker만 |
| ±Y, -X | 벽(중심에서 800), Blocker만 |
| +X(문) 수평 | 통로 경사 바닥, Support |
| 통로 안(입구에서 300cm 안쪽, 지각보다 중심 쪽)에서 바닥으로 | 지각이 아니라 지각보다 50cm 이상 깊은 통로 바닥 |
| 입구 옆(벽 바깥 300cm)에서 바깥쪽 | 지각이 남아 있어 반지름 R |
| 지각 위로 솟은 통로 벽을 바깥에서 | 벽(통로 로컬 y=200), Blocker만 |

## 5. 동적 사례와 oracle

`L_SurfaceRegression`의 지면 판은 엔진 Cube(두께 100, 중심 R+50)라 안쪽 면이 R=30,000에 온다. 이 맵은 베이크 입력이 아니므로 box 단순 충돌을 그대로 쓴다.

| 사례 | (위도, 방위) | 고정 배치 | exact 기대 | NavComponent | 지상 추격 | 원거리 타게팅 |
|:---|:---|:---|:---|:---|:---|:---|
| 상태형 기둥 | (30, 20) | 지면 좌·우(접선 ±800), 기둥 길이 1000 중심 R-500 | 기둥 자체는 항상 hit(Dynamic, Blocker만) | 좌·우 분리 후 안정 시 병합 가능 | 정지 전 불가, 안정 후 가능 | LoS 시 가능 |
| 움직이는 패널 | (30, 45) | 지면 좌·우(접선 ±800), 패널 800×800 안쪽 면 R-800 | 패널 현재 transform에서 hit(Dynamic Support), 패널 아래 gap은 miss | 좌·우 정적 영역 분리 | 계획적 이용 불가 | LoS 시 가능 |
| 파괴 바닥 | (30, 70) | 지면 좌·우(접선 ±950), 가운데 파괴 바닥 | 파괴 전 hit(Destructible), 파괴 후 miss | 파괴 전 overlay 연결, 파괴 후 분리 | 파괴 전 가능, 후 불가 | LoS 시 가능 |

### 동적 상태 oracle

| 사례 | 초기 상태 | 변경 | 기대 link | 기대 revision |
|:---|:---|:---|:---|:---|
| 상태형 기둥 | 이동 또는 직립 | 사전 정의된 안정 transform에서 완전 정지 | 안정 전 없음, 안정 후 좌·우 Walk Link 1개 | 영향 tile 지역 revision `+1` |
| 움직이는 패널 | 임의 transform | 매 프레임 transform 이동 | 계획용 Walk Link 없음 | 정적 Nav revision 변화 없음, Dynamic Support transform만 갱신 |
| 파괴 바닥 | 유효 | 파괴 이벤트 확정 | 기존 overlay link·support 제거 | 영향 tile 지역 revision `+1` |

동일 상태 이벤트의 중복 수신은 revision을 다시 증가시키지 않아야 한다. 상태형 기둥이 다시 움직이면 link를 닫고 지역 revision을 한 번 증가시킨다.

## 6. 제작과 재생성

- **동굴 키트**: `LNP.SurfaceNav.BuildCaveKit`이 `/Game/Maps/CaveKit`의 키트 메시를 제자리 갱신한다.
- **정적 fixture LVI**: `LNP.SurfaceNav.BuildRegressionFixture`. 키트 메시가 먼저 있어야 하고, 기존 LVI가 있으면 거부한다. 다시 만들려면 LVI를 지운 뒤 실행하고 `LNP.SurfaceNav.BakeOctant`로 SurfaceData를 다시 굽는다.
- **동적 맵**: 스크립트는 기존 맵을 기본적으로 덮어쓰지 않고 검증만 한다. 의도적으로 재생성할 때만 환경 변수 `LNP_REBUILD_SURFACE_REGRESSION_MAP=1`을 지정한다. 헤드리스 실행은 `UnrealEditor-Cmd -ExecutePythonScript=<스크립트>`다.
- actor label은 정적 LVI가 `RF_<순번>_`, 동적 맵이 `SSN_<순번>_`이다. Component Tag와 collision profile이 테스트 입력의 원본이며 label은 의미 판정에 쓰지 않는다.
- 좌표를 옮기면 이 문서의 표, 좌표 원본(헤더·스크립트), 자동화 기대값을 같은 커밋에서 갱신한다.

## 7. 예정된 fixture 추가

| 시점 | 변경 |
|:---|:---|
| Phase 4b | 한 component의 분리된 sheet, 내부형 double-sided normal의 Layer 분리 검증. 입력은 4a `LVI_Octant_Fixture_Crust`의 `FX_SplitSheet`·`FX_DoubleSidedPlate`다 |
| Phase 4b | ISM/HISM instance와 Support proxy↔exact counterpart fixture(D-037·D-039) |
| Phase 8 착수 전 | 동적 사례 3종을 Placement Marker와 서버 스폰 요소로 전환 |
| Phase 8 착수 전 | 같은 tile에 겹치는 Conditional Patch 두 개와 중복 상태 이벤트 fixture |

non-uniform·negative scale과 Nanite complex collision 원본 검증은 4a `LVI_Octant_Fixture_Crust`(`FX_NegativeScaleSlab`)와 자동화 `Bake.NaniteComplexCollisionSource`가 맡는다.

Phase 3 네트워크 회귀에는 이동 중 패널 late join, server-time 보정, 상태 revision 불일치 진단을 추가한다. Phase 7에는 다중 프레임 path request 실행 중 tile revision과 connectivity version이 바뀌는 사례를 추가한다.

## 8. 의도적으로 지원하지 않는 형상

- 복층·Y자·수직 동굴
- 서로 교차하는 Support sheet
- 움직이는 패널을 기다리고 탑승하는 계획 경로
- 임의 파괴 잔해가 만드는 새 보행면
- 측벽·밑면의 자동 보행면 승격
- 점프·훅·발사대·텔레포트를 사용하는 지상 NPC traversal
