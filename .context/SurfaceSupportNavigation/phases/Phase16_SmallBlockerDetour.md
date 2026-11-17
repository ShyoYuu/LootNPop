# 작은 Blocker 주변 지상 적 우회 — Phase16 후속

> 상태: 단위1 완료(2026-11-17, 사용자 PIE "자연스럽게 돈다"). 단위2 완료(2026-11-17, 사용자 PIE "자연스럽게 가로질러온다"). 단위3 완료(2026-11-17, 사용자 PIE "구멍이 메워지고 Nav가 지형에 맞게 자연스러워짐"). 단위4(직선 목표 분기 정지) 대기
> 작성: 2026-11-17
> 관계: Phase16 1단계(D-082 Blocker 비관통)의 후속. 그래픽스 1-B5d·1-B5e 인계를 받는다.
> 진행: 단위는 한 세션에 하나씩 순서대로 진행한다(1→2→3). 마스터 세션이 결과를 확인한 뒤 다음 단위를 띄운다.

## 1. 문제

그래픽스 세션이 Ridge WIP 옥탄트에 PCG 바위(Static+Blocker, Support 없음, D-058)를 깔자 지상 PureEntity 적에서 두 증상이 나왔다.

| 인계 | 증상 | 근거 |
|:---|:---|:---|
| 1-B5e | 작은 바위를 가끔 돌아가지 못하고 붙어 선다. `LNP.SurfaceNav.EnemyGround.Report`의 `blocker_rejects` 12,122/460,164단계(2.63%) | `Saved/Profiling/Graphics/Phase01/RidgeDecorationVariants_20261117/SelectionState.json` |
| 1-B5d | 바위 하나 주변의 Nav 구멍을 평지에서 크게 돌아간다 | `Saved/Profiling/Graphics/Phase01/RidgeDecorationExpand_20261116/` |

마스터 세션이 원인을 셋으로 나눴다. 각각 아래 단위가 맡는다.

## 2. 단위1 — Blocker 거부 시 옆걸음(D-087)

### 2.1 원인(코드로 재확인)

- 측면 sweep(`LNPEnemyExactMovement.cpp` `SweepLateral`)은 캡슐을 `MaxStepUp`(45cm) 올려서 한다. 45cm보다 낮은 바위나 바위의 넓은 아랫부분은 sweep에 걸리지 않는다. 55cm 바위도 올린 캡슐의 아래 반구가 윗모서리에만 닿아, 아래쪽이 캡슐 안으로 들어온다.
- 그 결과 캐시 접지 캡슐이 바위와 겹치고, D-082 규칙으로 거부되어 그 프레임은 제자리에 선다(`LNPEnemySurfaceMovement.cpp` `StepGrounded` 캐시 분기).
- 높은 벽은 sweep의 slide가 옆으로 미끄러뜨리지만 이 경로에는 미끄러짐이 없다. 조향 방향(경로 waypoint + 분리력)이 바뀌지 않으면 매 프레임 같은 거부가 반복되어 붙어 선다.

### 2.2 구현

- 캐시 접지가 Support 없는 Blocker 겹침으로 거부된 프레임에만 옆걸음 후보를 시험한다. 후보는 이번 이동(분리력 포함 속도)을 접평면에서 ±45°, ±90° 돌린 것이다. 작은 각도부터 본다.
- 각 후보는 기존과 같은 `MoveGroundedLaterally`(올린 sweep) → `TryResolveCachedGround` → `OverlapsSupportlessBlocker`를 통과해야 한다. 이번 걸음의 1/4보다 덜 움직인 후보(벽에 막힘)는 통과로 치지 않는다.
- 같은 각도에서 좌우가 모두 통과하면 원래 방향 진척이 큰 쪽(0.5cm 넘게 차이)을 고른다. 좌우 후보는 대칭이라 진척은 대개 같다. 이 경우 원래 목표 지점에서 각 쪽으로 캡슐 반지름만큼 비킨 자리가 Blocker와 겹치지 않는 쪽, 즉 Blocker가 먼저 끝나는 쪽을 고른다(겹침 질의 2회). 그래도 가릴 수 없으면 좌(+)다. 처음에는 "같으면 좌"만 두었는데, 그러면 이 규칙이 대부분을 정해 바위가 치우친 쪽(긴 쪽)으로 도는 것을 설계 단계에서 확인했다.
- 상태를 저장하지 않는다. 다음 프레임은 원래 방향부터 다시 본다.
- 모든 후보가 실패하면 지금처럼 `Rejected`로 제자리다.
- 바꾸지 않은 것: Blocker 비관통·이미 겹친 채 출발한 개체의 탈출 규칙, exact 폴백 경로(캐시가 아닌 프레임은 옆걸음 없음), 분리력(D-083). 비용은 거부 프레임에만 생긴다(후보당 sweep1·캐시 조회1·겹침1, 동점이면 겹침2 추가).
- 계측: `EnemyGround.Report`에 `blocker_detours`(옆걸음 성공 프레임)를 추가했다. `blocker_rejects`는 이제 옆걸음까지 모두 실패해 제자리에 선 프레임만 센다.

### 2.3 `WalkReaches`·`FindWalkAnchor`에 미치는 영향

둘 다 `StepGrounded`를 반복해 "실제로 걸어서 닿는가"를 본다(D-085). 이제 같은 옆걸음으로 작은 Blocker를 돌아 검증한다. 실제 이동이 옆걸음을 하므로 검증과 실제 이동이 일치하는 것이 의도와 맞다. 걸음 수 상한(직선 거리의 1.5배)과 같은 slot·Layer 유지, 제자리걸음 실패 규칙은 그대로라 우회는 짧은 것만 성공한다. 높은 벽은 기존처럼 sweep에서 멈추므로 거부·옆걸음이 생기지 않는다(`WalkAnchorRecovery`의 벽 케이스 통과).

### 2.4 검증

`EnemyMovement.SupportlessBlockerStep`(픽스처는 정면 조향을 매 프레임 고정). 사용자 확인을 받아 "제자리 정지" 단언을 불변식(비관통·돌아 지나감)으로 바꿨다.

| 케이스 | 이전 기대 | 지금 기대 | 결과(FinalX·옆 범위·Rejected) |
|:---|:---|:---|:---|
| 1 LowBlocker 40×200×20 | 앞에서 정지 | 옆면을 따라 돌아 지나감, 비관통 | 631·[0,131]·0 |
| 2 BoundaryBlocker 55cm | 앞에서 정지 | 같은 이유로 거부된 뒤 돌아 지나감, 비관통 | 626·[0,136]·0 |
| 4 EmbeddedSupport | 앞에서 정지 | 돌아 지나감, Support 겹침이 판정을 가리지 않음 | 631·[0,131]·0 |
| 7 OffsetSmallBlocker 40×40×20, y=0~40(새) | — | Blocker가 먼저 끝나는 −y로 돌아 지나감, 정지0 | 731·[−31,0]·0 |
| 8 BlockerPocket U자(새) | — | 후보 모두 실패 → 제자리 | 145·[0,0]·121 |
| 3·5·6 | 그대로 | 그대로 | 통과 |

- 케이스2는 확인 요청 때 "그대로"라고 설명했으나 실제로는 돌아 지나갔다. 같은 승인 방식(불변식 교체)을 적용했다.
- 낮은 상자 높이(20cm)에서 캡슐 아래 반구의 수평 반지름은 약30cm다. 그래서 옆 여유는 판정 반지름(34)보다 작다. 주머니 케이스의 옆 벽은 반구 위까지 닿는 40cm로 두어 여유를 2cm로 만들었다.
- 빌드: Game·Editor 타깃 Build.bat 성공. 에디터 Live Coding으로 `EnemyMovement` 7/7을 확인했다.
- 전체 `LootNPop.SurfaceNavigation`: 처음에는 에디터 안에서 돌렸는데 76개째(`FlightSteering.SpawnPenetration`)에서 에디터가 GPU 크래시(DXGI_ERROR_DEVICE_REMOVED, VRAM 5.75/6.64GB)로 종료됐다. 그때까지의 실패 2건은 환경 때문이다. 하나는 MCP 대기 timeout이 남긴 HTTP socket Error 1줄(`ProductionPoolCurrentAndDeterministic`)이고, 다른 하나는 크래시 순간의 테스트다. Editor 빌드 뒤 `UnrealEditor-Cmd -NullRHI` 헤드리스로 다시 돌려 **139/139 통과**(실패0, 경고4는 HTTP timeout3·B-option 안내1의 기존 노이즈)했다. 재저장 시험편3개는 git으로 원복했다. 보고서는 `Saved/Profiling/Phase16/SmallBlockerDetour/SurfaceNavFull/`다.

### 2.5 PIE(2026-11-17)

| 실행 | 편성 | 관찰 | `EnemyGround.Report` |
|:---|:---|:---|:---|
| 1 | 기본(Pod당 벽보행9·근접6·원거리3·비행9) | "이전보다 훨씬 잘 추격해온다" | 미수집(Report 전에 종료) |
| 2 | 지상 전용 메모리 편성(근접18·원거리9, 지상 2,220기 배치) | "자연스럽게 돈다" | steps 106,716·cache 94.31%·`blocker_rejects` **0**·`blocker_detours` 716(0.67%) |

- 두 실행 모두 WIP Ridge는 slot5(`LVI_Ridge_SurfaceProbe`), 1인이다.
- 이전 2.63%(그래픽스 1-B5e)는 거부 프레임 전체였다. 지금 같은 의미의 값은 rejects+detours = 0.67%이고, 그중 실제 정지는 0이다. 편성과 플레이 경로가 달라 정밀 비교는 아니다.
- 정리: `LNPSettings.OctantPool`과 `DA_MassSpawnConfig`는 메모리에서만 바꿨고(SaveConfig·에셋 저장 없음) PIE 뒤 원래 값으로 되돌렸다. 기록은 `Saved/Profiling/Phase16/SmallBlockerDetour/SelectionState.json`·`SpawnConfig_*.json`이다. 추적 Content/Config 변경은 0이다.
- 남은 것: 변경 비용의 패키지 전후 비교(`ProductionGround`)는 재지 않았다. 비용은 거부 프레임(이번 PIE 0.67%)에만 생긴다.

## 3. 단위2 — 경로 추종 지름길(D-088)

### 3.1 원인(코드로 재확인)

- `LNPEnemyPathProcessor`의 기존 추종은 (1) 발이 현재 waypoint 100cm 안이면 다음으로, (2) 시작 node에서 다음 waypoint node까지 `LNPNavGraph::IsDirectWalkable`이 통과하는 동안 개수 제한 없이 건너뛴다.
- 이 검사는 격자 선분이 지나는 모든 칸이 Nav node여야 통과한다. 경로도 `SimplifyPath`가 같은 검사로 줄이므로 Nav 구멍 둘레의 모서리 waypoint를 하나씩 밟는다.
- 지각 Nav는 200cm 삼각 격자·반지름50 캡슐이라 1m 바위도 node 1~3개와 간선을 지워 지름 4~6m 구멍이 된다. 그래서 평지에서 크게 돈다.

### 3.2 구현(사용자 승인 값)

- 격자 검사가 멈춘 직후, 다음 waypoint(i+1)가 있으면 `LNPEnemyNavigation::CheckPathShortcut`으로 본다. 통과하면 i+1로 건너뛰고 i+2까지 이어서 본다(최대 2개, 가까운 것부터).
- `CheckPathShortcut`: 후보까지 발 거리 800cm 초과면 `TooFar`, (현재→i→i+1) − (현재→i+1) < 50cm면 `LowGain`으로 걷지 않는다. 나머지는 현재 캡슐 중심에서 후보 지면점까지 `WalkReaches`(도달 50cm)로 걸어 `Walkable`/`Blocked`다.
- 비용 상한: 걸음 검증 뒤 같은 개체는 0.5초(`WalkAnchorRetrySeconds`) 동안 다시 걷지 않는다. `Blocked`는 `RequestSerial`·waypoint index로 기억해 같은 경로에서 다시 걷지 않는다. 거리·절약 조건에 걸린 후보는 걷지도, 기억하지도 않는다.
- Nav 밖 충돌: 지름길 도중 시작점이 투영 반경 밖이 되면 기존 코드는 조향을 보행 anchor로 되돌린다(D-085). 검증으로 건너뛴 waypoint(`ShortcutSerial`·`ShortcutWaypoint`)를 따라가는 동안은 되돌리지 않는다. 경로 재요청의 시작점은 그대로 anchor다.
- overlay(Pod·마커 차단 node)는 걸음 검증이 보지 않는다. Pod는 collision proxy, 마커는 실제 충돌이 있어 걸음이 막는다.
- 계측: `LNP.SurfaceNav.NavReport` 경로 줄에 `shortcutWalks`·`shortcutPasses`, trace scope `LNPEnemyPath_Shortcut`.
- 바꾸지 않은 것: A*·`SimplifyPath`·scheduler cache 합류(D-079), Alert·도달 불가 접근(D-063), 보행 anchor(D-085), 분리력(D-083).

### 3.3 검증

- 자동화 `EnemyMovement.PathShortcut`(새). 픽스처는 Nav |x|<=1200·|y|<=600에서 반지름250 원판을 뺀 구멍이고, 적 S(x=−380) → 구멍 둘레 W(0, 350) → 다음 waypoint C(x=380)다. 모든 케이스에서 S·C node 사이 격자 직선 검사가 실패함을 먼저 확인한다.

| 케이스 | 기대 | 결과 |
|:---|:---|:---|
| ShortcutHoleRock(바위 100×100×50 Blocker) | Walkable(옆걸음으로 돎) | Walkable |
| ShortcutHoleOpen(장애물 없이 Nav만 빠짐) | Walkable | Walkable |
| ShortcutWall(40×1400×300) | Blocked | Blocked |
| ShortcutLargeBlocker(200×1000×20 Support 없음) | Blocked(걸음 상한 안에 못 돎) | Blocked |
| ShortcutSupportGap(\|x\|<=120 Support 빈 띠, 캐시 해상도512) | Blocked | Blocked |
| 800cm 초과·거의 일직선 모서리 | TooFar·LowGain(걷지 않음) | 일치 |

- 빌드: Game·Editor 타깃 Build.bat 성공. 전체 `LootNPop.SurfaceNavigation` 헤드리스(`UnrealEditor-Cmd -NullRHI`) **140/140 통과**(경고5: HTTP timeout4·B-option 안내1 기존 노이즈). 재저장 시험편3개 원복. 보고서 `Saved/Profiling/Phase16/SmallBlockerDetour/Unit2_SurfaceNavFull/`.
- 비용: 현재 소스를 `Saved/SurfaceNavigationShortcutPackage`로 cook(약88초, 오류0, exe SHA256 `91FD4EEF…`)하고, 기준선 패키지(42e485a, 단위1 이전)와 같은 시간대에 `RunPhase13Gate.ps1 -Cases ProductionGround,Production`을 번갈아 돌렸다. 모든 실행 host/guest 종료0·Gate frame·path PASS. 결과 `Saved/Profiling/Phase16/SmallBlockerDetour/Unit2_Cost/`.

| | ProductionGround frame P95(ms) | consumer P95(ms) | Production frame P95(ms) |
|:---|:---|:---|:---|
| 이전(4회) | 7.84·7.52·7.84·7.71(평균7.73) | 0.116·0.100·0.126·0.118(평균0.115) | 6.87·7.16 |
| 이후(4회) | 7.74·8.71·7.92·7.97(평균8.09) | 0.110·0.120·0.121·0.116(평균0.117) | 7.11·6.98 |

- 해석: 지름길 코드는 모두 경로 consumer 안에서 도는데 consumer P95는 차이가 없다. frame P95 평균 차(+0.36ms)는 P50도 함께 올라(7.06→7.32) 프레임 전체가 밀린 것이고, 8.71ms 1회를 확인하려 2회씩 더 돌린 값은 기준(8.14/7.96) 범위다. 이전 패키지는 단위1 이전이라 그 몫도 섞인다. 측정 로그에는 지름길 횟수가 없어 이 부하에서 몇 번 돌았는지는 모른다.
- PIE(사용자, 2026-11-17): 검사용 풀을 메모리로만 지정, TestMap03 1인, WIP Ridge는 slot2. `LNP.Debug.TeleportToOctant 2 2697 -2626 -3824`로 X2697·Y-2626 구멍(단위3 전이라 그대로)에 서서 `DrawNav component 3000`과 함께 추격을 보았다. 사용자 관찰 **"자연스럽게 가로질러온다"**. 같은 구간 `NavReport` 차이: followerFrames +37,431·경로 추종 +6,214·waypoint 진행 +260·`shortcutWalks` +88·`shortcutPasses` +87. PIE 뒤 원래 풀로 복원했다(`Saved/Profiling/Phase16/SmallBlockerDetour/Unit2_SelectionState.json`). 추적 Content/Config 변경은 0이다.

## 4. 단위3 — 베이커 `ClearanceFloorOnly` 오판(D-089)

### 4.1 원인(코드로 재확인)

- 근거: `Saved/Profiling/Phase16/GraphicsHandoff_20261117/ExplainNavGap_Slot6_X2697_Y-2626.log`. 반경 500cm 후보17 중 Node7·NodeNavInterp2·Clearance2(바위)·ClearanceFloorOnly2·NonWalkable4였다. ClearanceFloorOnly2는 중심 67cm 지점을 포함하며 npcFits=1, dot0.96~0.98(평지)이다.
- node clearance(`HasGravityCapsuleClearance`)는 캡슐 바닥을 node 위치에서 `SlopeLift`+1cm 위에 둔다. node 위치는 Atlas 보간이라 두 node는 실제 지면보다 1.6·0.3cm 낮았고(`groundDR`), 반지름50 안의 작은 요철에 바닥만 닿았다.
- 같은 구조: edge·portal sweep(`HasCapsulePolylineClearance`, 끝점이 같은 보간 위치이고 50cm 표본 사이 요철 위를 지남), Conditional Patch base node 차단·Patch Layer node(같은 함수). 지각 이음매 node는 같은 함수라 양쪽 사본에 대칭으로 적용되고 D-060 대조·엄격 보간은 그대로다. 런타임은 clearance를 다시 계산하지 않는다.

### 4.2 구현(사용자 승인)

- 캡슐 **아래쪽만** 10cm 줄인다(`NavClearanceFloorTolerance`, 중심 +5cm·반높이 88→83). 캡슐 전체를 올리는 방식(진단의 `lifted10`)은 머리 끝도 10cm 올라가 낮은 천장·동굴 통로 node가 새로 빠질 수 있어 고르지 않았다.
- 적용 범위: 정적 node·같은 Layer edge·Layer 간 portal·Conditional Patch base node 차단과 Patch Layer node. node만 바꾸면 새 node가 edge를 못 얻어 고립 조각 제거(D-084)로 다시 빠질 수 있다.
- 10cm 넘게 솟은 Blocker·벽·천장은 계속 막는다. NonWalkable·경사 판정과 고립 조각 제거는 그대로다. 10cm보다 낮은 Support 없는 Blocker는 Nav에 남고 실제 이동은 옆걸음(D-087)이 처리한다.
- 베이크 보고 `Nav clearance floorToleranceNodes=<n> npcFits=<m>`(고립 조각 제거 전): 이 허용으로만 통과한 node와 그중 실제 바닥에서 실제 NPC 캡슐(반지름35, 줄이지 않음)이 서는 수.
- `ExplainNavGap`: `ClearanceFloorOnly`·`lifted10`을 없애고 같은 캡슐의 `Clearance`+blocker로 표기, 통과 node에 `floorTolerance=1`(줄이지 않았다면 빠졌을 node)을 붙인다. 베이커 schema 10→11.

### 4.3 재베이크(정식7·fixture5·WIP, `Unit3/BakeAll_Schema11.log`)

| 옥탄트 | cells | components | portals | 새 node(r35 섬) |
|:---|:---|:---|:---|:---|
| Meadow | 68,000→68,824 (+1.2%) | 84→12 | 24→41 | 816 (757, 93%) |
| Bastion | 162,745→164,783 (+1.3%) | 248→205 | 2,753→2,872 | 2,031 (1,534, 76%) |
| Hollow | 192,058→192,409 (+0.2%) | 24→24 | 1,347→1,483 | 349 (199, 57%) |
| Acropolis | 139,452→141,964 (+1.8%) | 237→170 | 1,528→1,737 | 2,478 (1,980, 80%) |
| Pyramid | 778,225→778,916 (+0.1%) | 222→220 | 5,679→6,642 | 692 (385, 56%) |
| Grove | 130,817→131,301 (+0.4%) | 76→69 | 509→1,157 | 482 (364, 76%) |
| Ridge | 75,888→76,487 (+0.8%) | 70→48 | 424→528 | 577 (499, 86%) |
| fixture5 | +0~8 | 불변 | Regression·Conditional 79→95, Crawl 4→31, Island 12→19 | 0~8 |
| WIP Ridge | 75,870→76,466 | 70→48 | 424→528 | 574 (496) |

- 정식7 새 node 7,425개 중 r35가 서는 것은 5,718개(77%)다. r35 검사는 실제 바닥에서 허용 없이 하므로, r35만 실패한 node는 캡슐 맨 아래(r50 검사가 보지 않는 10cm 띠)만 닿은 것이다(지형 요철·10cm 미만 물체·작은 턱). Hollow·Pyramid는 실내 바닥 요철이 많다.
- 고립 조각 `ListNavInterpComponents` navInterpOnly는 13개 모두 0이다. 이음매 clearance 경고는 49→44(기존).
- 문제 지점 ExplainNavGap(`Unit3/BakeAll_Schema11.log` 끝): 이전 ClearanceFloorOnly 두 node가 저장 node(edge 3·5, 주 component, `floorTolerance=1`)가 됐고 Clearance 2개는 바위만 남았다(Node7→9).

### 4.4 검증

- 빌드: Editor 타깃 Build.bat 성공(베이커는 에디터 모듈이라 게임 타깃으로는 확인 불가).
- 전체 `LootNPop.SurfaceNavigation` 헤드리스: 137 통과·3 실패(Nav 데이터 변화). 사용자 결정으로 기준을 갱신·검사를 보강하고 3개를 다시 돌려 통과, 합계 **140/140**(경고는 HTTP timeout·B-option 안내의 기존 노이즈). 재저장 시험편3개 원복. 보고서 `Unit3/SurfaceNavFull`·`Unit3/Rerun3`.
  - `Nav.Hierarchy`: Cluster당 682→728.8B(intraEntries 1.69M→1.82M, K 최대 59→67). 끊겼던 조각이 이어진 콘텐츠 변화라 상한을 715→802B(schema11 값 +10%)로 갱신.
  - `Nav.ProductionScheduler`: 요청 상한(30,000) 초과 쌍 1→7/300(직선20~77m·비용362~555m, 대부분 이음매 근처, 상한 없이 모두 성공). 허용을 1%→3%로 갱신. 런타임에서는 상한 뒤 NoPath로 끝나고 프레임 예산 안에서 나뉜다.
  - `Nav.FlowField`: 분할 빌드 검사가 "앞 3쌍은 120m 안"을 가정했는데 새 긴 우회 쌍이 섞였다(settled 40→36). 반경 안에 드는 앞쪽 3쌍을 고르도록 고쳤다.
  - `Nav.ProductionSpawnProjection`: groups 786→549, 고립 node 투영 18→6, 작은 component 투영 21→7, 투영 최대 213.5→173.8cm. `Nav.RegressionReachability` 불변.
- 비용: schema11 데이터로 `Saved/SurfaceNavigationFloorTolerancePackage`를 cook(83초, 오류0, exe는 단위2와 같은 SHA256 `91FD4EEF…`)하고 단위2 패키지와 번갈아 `RunPhase13Gate.ps1 -Cases ProductionGround,Production`을 2회씩 돌렸다. 8회 모두 종료0·Gate PASS. 결과 `Unit3/Cost/`.

| | ProductionGround frame P95(ms) | consumer P95(ms) | Production frame P95(ms) | 경로 요청 |
|:---|:---|:---|:---|:---|
| 이전(schema10) | 7.66·7.44 | 0.121·0.101 | 6.71·6.57 | 55·43 |
| 이후(schema11) | 7.78·7.55 | 0.114·0.117 | 6.57·7.12 | 37·23 |

- 차이는 단위2 측정 편차(7.52~8.71) 안이다. 지상 활성은 4회 모두 27이고, 경로 요청·waypoint(102·70→46·37)는 줄었다.
- PIE(사용자, 2026-11-17): 검사용 풀을 메모리로만 지정, TestMap03 1인, WIP slot4. 사용자 관찰 "원래 문제 지형의 구멍이 메워졌고 Nav node와 연결선이 훨씬 지형에 맞게 자연스러워졌다. 추격도 자연스럽다(단위1·2 때 이미 자연스러웠으므로 이번 영향인지는 모름)". PIE 뒤 원래 풀로 복원(`Unit3/SelectionState.json`), 추적 Content/Config 변경은 재베이크한 SurfaceData12개뿐이다.

## 5. 단위4 — 직선 목표 분기의 바위 앞 정지(대기)

단위3 PIE에서 단위1·2 때 재현하지 못한 정지를 재현했다.

- 상황: WIP slot4 근접 PureEntity 525가 원래 문제 바위(`GraphicsRockProbe_Moss_01`, 칸 (128,101)) 옆에서 3분간 정지. 벽 지형 모서리+울퉁불퉁한 지면+바위, 바위에 직접 걸림.
- 로그: `[NavEnemy] state=2 pathStatus=0 waypoints=0 steering=0 walkAnchor=-1`, 3분 동안 경로 요청 수 불변(357), `EnemyGround` `blocker_rejects` 10,584/161,968(6.5%)·`blocker_detours` 415.
- WIP 로컬 좌표: NPC (20218.5, 15970.1, 21906.1), 플레이어 (20639.5, 16022.7, 21640.8). 진단 `Unit3/ExplainNavGap_Stuck525.log`.
- 원인: `LNPEnemyPathProcessor` 직선 목표 분기. 시작 node→목표 node 격자 직선(J=102 줄, 바위에서 약1m)은 통과하지만 실제 조향은 현재 위치(바위 칸에서 108cm, 시작 node에서 약2m)→플레이어 직선이라 바위를 지난다. 실제 보행 검증(`WalkReaches`)은 Nav 밖 시작(D-085)에만 있다. 옆걸음(D-087)은 절벽 모서리·요철로 대부분 실패한다.
- 단위3과의 관계: 이전에는 (127,102)·(128,102)가 `ClearanceFloorOnly`로 빠져 격자 직선이 실패하고 경로 waypoint로 돌았다. schema11이 node를 채워 기존 구멍(Nav 안 시작의 직선 분기 무검증)이 드러났다.
- 수정 방향(안, 사용자 확인 전): 직선 분기 중 진척 없음·Blocker 거부가 이어질 때만 0.5초 간격 `WalkReaches`로 검증하고, 실패하면 경로를 요청한다. 같은 목표에서 경로 추종의 격자 건너뛰기도 실제 보행 검증을 거쳐야 다시 직선으로 끌려가지 않는다.

## 6. PIE 검증 절차(단위1)

1. `/Script/LootNPop.Default__LNPSettings`의 `OctantPool` 원래 값 `/Game/Maps/OctantPoolData.OctantPoolData`를 기록한다.
2. 메모리로만 `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/V001/DA_GraphicsRidgeValidationPool`을 지정한다(SaveConfig 금지).
3. TestMap03 1인 PIE → 매 실행 로그에서 WIP slot 확인 → `LNP.SurfaceNav.EnemyGround.Reset` 후 교전 → `Report`의 `blocker_rejects` 비율을 2.63%와 비교한다.
4. PIE 종료 뒤 원래 풀로 복원한다. 그래픽스 세션과 WIP 저장·베이크를 동시에 하지 않는다.
