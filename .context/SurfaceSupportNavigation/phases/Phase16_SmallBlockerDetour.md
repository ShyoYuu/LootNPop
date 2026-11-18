# 작은 Blocker 주변 지상 적 우회 — Phase16 후속

> 상태: 단위1 완료(2026-11-17, 사용자 PIE "자연스럽게 돈다"). 단위2 완료(2026-11-17, 사용자 PIE "자연스럽게 가로질러온다"). 단위3 완료(2026-11-17, 사용자 PIE "구멍이 메워지고 Nav가 지형에 맞게 자연스러워짐"). 단위4 완료(2026-11-17, D-090 실제 선분의 삼각형 통로·Nav 주머니·마지막 접근 검증, 사용자 최종 PIE에서 원래 정지 재현 안 됨). 단위5a 완료(2026-11-18, D-091 Nav 밖 플레이어 도달 불가·바위 위 층 빌리기·플레이어 주머니 판정·anchor 대상 조향, anchor 통과 주머니는 남은 한계). 단위5b 구현·커밋(2026-11-18, D-092 플레이어 발자취·anchor 궤적 점열·점열 추종 복구·적 자기 발자취·추격 자격 Nav 소속 기준, 헤드리스 142/142). 단위6 완료(2026-11-18, §8: 5b 마무리 검증, D-093 경로 단순화 삼각형 통로·D-094 포기 플레이어 기억·D-095 낀 적 회복, 헤드리스 142/142·패키지 Gate PASS)
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

## 5. 단위4 — 실제 선분의 삼각형 통로(D-090)

### 5.1 재현과 원인(코드로 재확인)

단위3 PIE에서 단위1·2 때 재현하지 못한 정지를 재현했다.

- 상황: WIP slot4 근접 PureEntity 525가 원래 문제 바위(`GraphicsRockProbe_Moss_01`, 칸 (128,101)) 옆에서 3분간 정지. 벽 지형 모서리+울퉁불퉁한 지면+바위, 바위에 직접 걸림.
- 로그: `[NavEnemy] state=2 pathStatus=0 waypoints=0 steering=0 walkAnchor=-1`, 3분 동안 경로 요청 수 불변(357), `EnemyGround` `blocker_rejects` 10,584/161,968(6.5%)·`blocker_detours` 415.
- WIP 로컬 좌표: NPC (20218.5, 15970.1, 21906.1), 플레이어 (20639.5, 16022.7, 21640.8). 진단 `Unit3/ExplainNavGap_Stuck525.log`.
- 원인: `LNPEnemyPathProcessor` 직선 목표 분기. 시작 node→목표 node 격자 직선(J=102 줄, 바위에서 약1m)은 통과하지만 실제 조향은 현재 위치(바위 칸에서 108cm, 시작 node에서 약2m)→플레이어 직선이라 바위를 지난다. 실제 보행 검증(`WalkReaches`)은 Nav 밖 시작(D-085)에만 있다. 옆걸음(D-087)은 절벽 모서리·요철로 대부분 실패한다.
- 단위3과의 관계: 이전에는 (127,102)·(128,102)가 `ClearanceFloorOnly`로 빠져 격자 직선이 실패하고 경로 waypoint로 돌았다. schema11이 node를 채워 기존 구멍(Nav 안 시작의 직선 분기 무검증)이 드러났다.

- 코드: `LNPEnemyPathProcessor`의 직선 목표 분기(`bDirectGoal`)와 waypoint 건너뛰기가 모두 node↔node 격자 직선(`IsDirectWalkable` → `WalkDirect`)만 봤다. `WalkDirect`는 샘플마다 가장 가까운 격자점이 node인지만 보므로 선에서 약1m까지만 보장한다. 실제 조향은 발 → MoveTarget(또는 waypoint) 선분이라 그 사이 어긋남이 수 m였다.
- 근거 확인: 베이커는 node 캡슐(`HasGravityCapsuleClearance`)과 edge 캡슐 sweep(`HasCapsulePolylineClearance`, 반지름50, 양방향)을 모두 본다. 세 변이 살아 있는 삼각형 안에서 검사 밖으로 남는 곳은 내접원 중심 반지름 약8cm(200cm 격자)뿐이다.
- 이전에 기록한 안(정지 감지 뒤 `WalkReaches`)은 사후 대응이라 구현하지 않았다. 아래 판정이 예상 밖으로 실패하면 쓸 예비안으로 남긴다.

### 5.2 결정(사용자 승인)

| 항목 | 결정 |
|:---|:---|
| 끝점(플레이어)이 불완전 삼각형 | 직선 거부, 경로로 목표 node까지. waypoint가 끝나면 이동이 MoveTarget으로 곧장 간다. PIE 4차 뒤 이 마지막 구간을 실제 보행으로 검증해 막히면 목표 node에서 대기(A). Nav 밖 플레이어는 D-085 anchor 검증으로 대신 |
| 시작점이 불완전 삼각형 | 직선·건너뛰기 거부, 현재 waypoint로 조향(새 경로는 최근접 node). PIE 1차 뒤 **Nav 주머니**만 보행 anchor로, PIE 4차 뒤 anchor 실패 시 마지막으로 트인 자리로 되돌아가기(B) 보강(5.4) |
| 현재 waypoint로 향하는 조향 선분 | 검사하지 않는다(waypoint 사이는 `SimplifyPath` 격자 직선) |
| slot·Layer | 같은 slot·Layer만(시작 node 기준), 옥탄트 면 밖이면 거부 |
| 격자 좌표 | Atlas `ComputeCell`과 같은 면 중심 투영을 `LNPNavData::ProjectToGrid`로 공개해 재사용 |

### 5.3 구현

- `LNPNavGraph::IsSegmentWalkable(Nav, FromNode, FromPosition, ToNode, ToPosition, Overlay)`: 선분을 시작 node의 slot·Layer 격자 (i,j)로 옮기고, i·j·i+j 정수선과의 교점을 t 순으로 병합해(정렬·할당 없음) 지나는 삼각형을 차례로 본다. 세 꼭짓점 node(BlockedNodes·overlay)와 세 변(이웃 edge·BlockedEdges 양방향)이 모두 있어야 통과다. 꼭짓점을 정확히 지나는 길이 0 구간은 건너뛴다.
- 적용: `bDirectGoal`(발 또는 Nav 밖이면 anchor node → `ApproachGoal`)과 waypoint 건너뛰기(발 → 다음 waypoint 지면점). A*·`SimplifyPath`·cache 합류(D-079)·D-088 지름길·D-085 마지막 접근 검증·D-087 옆걸음·분리력은 그대로다.
- Nav 주머니(`IsInNavPocket`, 외접원+25cm): 보행 anchor(D-085), 실패하면 `LastOpenPoint`(마지막으로 트인 삼각형의 발 위치)로 되돌아가기(B).
- 마지막 접근(A): waypoint를 다 쓰고 직선이 거부된 상태면 발 → MoveTarget을 `WalkReaches`(0.5초 재사용, `bEndApproachWalkable`)로 검증하고 막히면 제자리(목표 node 근처)에서 기다린다.
- 진단: `NavReport`의 `[NavEnemy]` 줄에 조향·waypoint·MoveTarget 거리, `startTri`·`pocket`·`directSeg`·`waypointSeg`(overlay 포함), `endWalk`·`lastOpenDist`, `playerToPod`·`chaseRadius`를 붙였다.

### 5.4 PIE(사용자, TestMap03 1인, 검사용 풀 메모리 지정)

| 차수 | WIP slot | 관찰 | 조치 |
|:---|:---|:---|:---|
| 1 | 5 | `blocker_rejects` **1/190,206**(이전 6.5%). 근접 325가 벽 모서리+바위 사이 **Nav 주머니**(최근접 node 255cm, 투영 반경 안)에서 정지. 직선은 거부됐고 첫 waypoint 조향이 sweep에 막힘(`Unit4/ExplainNavGap_Stuck325.log`) | 주머니면 보행 anchor(D-085) 재사용 |
| 2 | 4 | 원래 바위 자리 정지 재현 안 됨. 트인 평지에서 근접 496 **제자리걸음**. 프레임 추적상 주머니 판정(100cm 기준)이 Pod overlay 근처 경로에서 anchor↔waypoint를 오가며 진동(`Unit4/ExplainNavGap_March496.log`) | 주머니 기준을 삼각형 외접원+25cm로(`IsInNavPocket`), Live Coding 뒤 496이 플레이어에 도착 |
| 3 | 1 | 근접 4마리가 Pod 세력권 경계(플레이어→Pod 6,087cm > `ChaseRadius` 5,000)에서 Alert 8초 ↔ None 1초를 무한 반복하며 쳐다보기만 함. 기존 타게팅 동작이라 단위4와 무관 | 별도 세션에서 홈 반경 귀환으로 고침(24ea374) |
| 4 | 6 | 귀환 개선(24ea374) 뒤. 근접 179가 첫 재현과 같은 주머니에서 정지. `pocket=1`이지만 보행 anchor가 4개 모두 실패(`walkAnchor=-1`)해 최근접 node(바위 너머)로 조향. MoveTarget(근접 슬롯)이 주머니 안이라 마지막 접근 직진이 끌고 들어간 것으로 추정 | A(마지막 접근 걸음 검증)+B(마지막으로 트인 자리로 되돌아가기) |
| 5 | 5 | 원래 정지 재현 안 됨, `blocker_rejects` 41/390,416. 플레이어가 주머니 안에 서면 근접 적이 입구에서 섬: 플레이어가 Nav 밖이고 보행 anchor도 없어(`node=-1`) 경로 소비자가 건너뛰는 기존 규칙(5.6 한계) | 단위4 범위 밖, 사용자 판단으로 마무리 |

- 매 PIE 뒤 풀을 원래 값으로 되돌렸다(`Unit4/SelectionState.json`). 추적 Content/Config 변경은 없다.

### 5.5 검증

- 테스트 `EnemyMovement.DirectCorridor`: 재현 형태(격자 직선 통과·실제 선분 거부), 불완전 삼각형 시작·끝·정지 거부, 트인 삼각형·꼭짓점·격자선 통과 허용, slot 다름 거부, 살아 있는 꼭짓점 가까이는 주머니 아님, 주머니 픽스처(구멍 반지름220) 판정·보행 anchor.
- 빌드·회귀: Game·Editor 빌드, 헤드리스 전체 `LootNPop.SurfaceNavigation` **141/141**(외접원 기준 `Unit4/SurfaceNavFull3`, A+B 뒤 `Unit4/SurfaceNavFull4`), 재저장 시험편 3개 원복.
- 비용(`RunPhase13Gate.ps1 -Cases ProductionGround,Production`, 단위3 패키지와 교대 2회씩, 모두 종료0·Gate PASS). 2차는 주머니 100cm 기준 판, 3차가 최종 코드(외접원+A+B)다. 3차 "이후" 패키지에는 귀환 개선(24ea374)도 들어 있다.

| | ProductionGround frame P95(ms) | consumer P95(ms) | 경로 요청(캡처/전체) | 경로 추종 프레임 | Production P95(ms) |
|:---|:---|:---|:---|:---|:---|
| 1차 이전·이후(`Unit4/Cost_v1`) | 8.80·8.64 → 8.20·8.08 | 0.129·0.148 → 0.151·0.144 | 25/45·56/65 → 39/48·47/61 | 14,201·20,458 → 32,242·35,941 | 7.35·7.14 → 7.38·7.27 |
| 2차 이전·이후(`Unit4/Cost_v2`) | 8.18·8.39 → 8.50·8.47 | 0.119·0.120 → 0.145·0.138 | 40/50·34/38 → 21/38·32/67 | 10,632·9,146 → 21,406·26,620 | 7.32·7.33 → 7.37·7.47 |
| 3차 최종(`Unit4/Cost`, exe `8064376E…`) | 9.05·8.36 → 8.74·8.18 | 0.125·0.130 → 0.191·0.162 | 32/37·56/75 → 67/89·31/46 | 8,168·16,568 → 54,636·38,114 | 7.45·7.17 → 7.44·7.36 |

- 해석: 1·2차에서 경로 요청은 늘지 않았고, 직선 분기가 더 자주 거부되어 경로 추종 프레임이 약2배, consumer P95는 약 +0.02ms였다. 3차는 consumer P95 +0.03~0.06ms(마지막 접근 걸음 검증 몫과 귀환 개선 몫을 가를 수 없음)이고 1회차는 지상 활성 36(이전 27)으로 부하가 달랐다. 모두 Gate 1.5ms 대비 작다. frame P95 차이는 측정마다 방향이 달라 편차로 본다.

### 5.6 남은 것

- 한계(범위 밖): 플레이어가 주머니 안처럼 Nav 밖이고 보행 anchor도 없으면 근접 적이 입구에서 막혀 선다(경로 소비자가 건너뛰고 MoveTarget 직진, 추적 중 Confirmed 유지). → 단위5(§7)에서 다룬다: 5a 도달 불가 처리(D-091), 5b 플레이어 발자취(계획).
- 예비안: 판정이 예상 밖으로 실패하면 정지 감지 뒤 `WalkReaches` 검증·경로 재요청(이전 기록안).
- 범위 밖: 배회 목표의 직선 판정(`LNPEnemyStateTreeProcessors.cpp`)과 cache 합류(D-079)는 여전히 node↔node 격자 직선이다.

## 6. PIE 검증 절차(단위1)

1. `/Script/LootNPop.Default__LNPSettings`의 `OctantPool` 원래 값 `/Game/Maps/OctantPoolData.OctantPoolData`를 기록한다.
2. 메모리로만 `/Game/Maps/_WorkInProgress/Ridge_00/Graphics/V001/DA_GraphicsRidgeValidationPool`을 지정한다(SaveConfig 금지).
3. TestMap03 1인 PIE → 매 실행 로그에서 WIP slot 확인 → `LNP.SurfaceNav.EnemyGround.Reset` 후 교전 → `Report`의 `blocker_rejects` 비율을 2.63%와 비교한다.
4. PIE 종료 뒤 원래 풀로 복원한다. 그래픽스 세션과 WIP 저장·베이크를 동시에 하지 않는다.

## 7. 단위5 — Nav 밖 플레이어(5a D-091 도달 불가, 5b D-092 발자취)

단위4 PIE 5차에서 남긴 한계(§5.6)를 두 단계로 나눈다(사용자 승인). 5a는 "판정 불가"를 "도달 불가"로 바꾸는 최소 변경이고, 5b는 Nav 없는 걸을 수 있는 바닥까지 추격을 늘린다. 5b는 별도 세션에서 한다.

### 7.1 원인(코드로 재확인)

플레이어가 땅에 서 있고 물리 바닥은 식별되지만 투영 반경 300cm 안에 node가 없고 보행 anchor(D-085)도 실패하면 `FLNPPlayerNavFragment.Node == INDEX_NONE`, `GroundGroup` 무효다.

- `LNPEnemyNavigation::UpdateMeleeSlot`: `!GroundGroup.IsValid()`면 판정 없이 `bOccupied`를 돌려줬다. 이미 슬롯을 가진 근접 적은 기한 없이 슬롯을 유지하고 1.5초 도달 불가 유예도 시작되지 않았다. 새로 오는 적은 타게팅 후보 단계에서 도달 불가라 슬롯 경쟁에 들어오지 않았다.
- `ULNPEnemyPathProcessor`: `bUnreachableTarget`·`bAlertApproach`(D-063)를 `Node != INDEX_NONE`일 때만 계산했고, Node가 없으면 경로 소비를 건너뛰었다(`continue`). Confirmed 적은 조향점 없이 MoveTarget으로 곧장 걷다 바위에 막혀 섰고, Alert 적은 제자리에 섰다.

### 7.2 5a 구현(D-091)

- 판정 조건: `bGrounded` + `Surface` 유효(현재 snapshot generation) + Nav 소속 없음(`GroundGroup` 무효 = `Node == INDEX_NONE`).
- 슬롯(`UpdateMeleeSlot`에 플레이어 물리 바닥 `GroundSurface` 인자 추가): 위 조건이면 끝점 상태와 무관하게 도달 불가로 본다. 기존 슬롯은 1.5초 유예 뒤 반납하고 새 슬롯은 주지 않는다. 유예 시계는 기존과 같이 플레이어·group 변경(Nav 위 group → Nav 밖)에서 다시 시작한다. 끝점 Stale·바닥 handle이 이전 generation·바닥 식별 실패(`Surface` 무효)는 판정을 미루고(기존 슬롯 보존·새 슬롯 없음), 공중도 기존 슬롯 보존·새 슬롯 없음이다. 공중 유예 초기화(기존 규칙)는 Nav 소속이 있는 마지막 접지에만 적용된다. 마지막 접지가 Nav 밖이면 기존 코드 순서대로 공중 판정 전에 반환해 시계를 되돌리지 않는다. 주머니 안에서 1.5초보다 자주 뛰어 슬롯을 영구히 쥐는 길을 막기 위해 이 순서를 유지했다(1.5초가 지난 뒤 Nav 밖에 착지하면 바로 반납, Nav 위에 착지하면 Reachable로 유지).
- 슬롯을 놓은 적: 타게팅 후보 단계가 Nav 밖 플레이어를 이미 도달 불가로 보므로(직전 Confirmed만 예외) 슬롯을 잃으면 Alert로 내려가고 다시 슬롯을 다투지 않는다. 플레이어가 Nav로 돌아오면 끝점이 Reachable이 되어 바로 슬롯을 받는다.
- 경로 처리기: Node가 없어도 `Surface`가 유효하면 도달성을 계산하고, Stale이 아니면 도달 불가로 둔다. 목표는 `Target.TargetLocation`, 목표 Surface는 플레이어 물리 바닥이다. 투영 node가 없어도 `ResolveEndpoints`가 NoNode와 함께 접근점(자기 group의 6방향 내부 node 중 목표에 가장 가까운 것, 3,000cm)을 주므로 D-063 접근 경로가 그대로 나온다. 근접은 접근점까지 간 뒤 Alert로 서서 플레이어를 바라본다(절벽과 같은 모습). 1.5초 유예 동안의 Confirmed 적도 같은 접근 경로를 따른다(D-063과 같은 규칙).
- 공중: 경로 처리기는 Node가 있을 때처럼 마지막 접지 바닥으로 판정한다(점프마다 경로를 버리지 않는다). 슬롯 쪽은 기존대로 공중이면 판정하지 않는다.
- 바위 위(PIE 1차 뒤 추가, 사용자 승인): 플레이어가 Support 없는 정적 Blocker 위에 서면 바닥 층이 없어(`Surface` 무효) 위 조건에 들지 않았다. `ULNPPlayerNavProcessor`가 직전 접지가 같은 slot·현재 generation 지지면이면 그 층을 빌려(`LNPEnemyNavigation::BorrowBlockerFloorSurface`, `bBorrowedSurface`) 바위 위 발 위치로 Nav 투영·보행 anchor를 계속한다. 트인 평지의 바위는 둘레 node가 투영 반경 안이라 지금처럼 추격·공격하고, 주머니 속 바위는 node·anchor가 없어 5a 도달 불가가 된다. "바위 위면 무조건 도달 불가"는 평지의 작은 바위에만 올라서도 근접 적이 추격을 멈추는 악용 여지가 있어 택하지 않았다. Support 비보행 face·동적 지지면·미등록 Actor(Pod 등)·다른 slot·직전 지지면 없음은 빌리지 않는다(기존 규칙).
- 플레이어 Nav 주머니(PIE 2차 뒤 추가, 사용자 승인): 투영 반경 안에 node가 있어도 플레이어 발이 Nav 주머니(`IsInNavPocket`, 불완전 삼각형+최근접 node가 외접원+25cm 밖, overlay 없이)이면 투영 node를 쓰지 않고 Nav 밖과 같이 보행 anchor(D-085, node에 선 적이 플레이어 발 150cm까지 걸어옴)로 검증한다(`bInNavPocket`). 통과하면 anchor로 이어지고, 실패하면 Nav 소속 없음 → 5a 도달 불가다. 적 쪽에 이미 있던 D-090 주머니 규칙을 플레이어 판정에 맞춘 것이다. 이 규칙이 없으면 주머니 속 플레이어는 바위 너머 node로 Nav 위가 되어, 슬롯을 쥔 적이 마지막 접근 검증(D-090 A)에 막혀 목표 node에서 영구히 기다렸다. 주머니가 아닌 불완전 삼각형(벽·절벽 가장자리)은 기존대로 투영 node다.
- anchor 대상 마지막 구간(PIE 3차 뒤 추가, 사용자 승인): 보행 anchor로 이어진 플레이어(`bWalkAnchored`)를 쫓는 적은 경로를 다 쓰면 근접 슬롯 위치(MoveTarget)가 아니라 플레이어 발(`GroundPoint`)로 조향한다. anchor가 검증한 것은 anchor node → 플레이어 발 구간이고, 슬롯 위치는 그 길 밖(바위 너머)일 수 있었다. 정지는 기존 근접 정지 거리(실제 대상 거리)이고, 도착 신호·D-090 마지막 접근 검증(anchor 대상은 원래 건너뜀)은 그대로다.
- 진단: `NavReport`의 `[NavEnemy]` 줄에 `approachUnreachable`(도달 불가 접근 경로를 따르는 중), 플레이어 줄에 `borrowedSurface`·`pocket`을 붙였다. 플레이어 줄의 `grounded`·`slot`·`layer`·`node=-1`·`walkAnchored=0`이 판정 조건이다.

### 7.3 원거리 지상 적에 미치는 영향

- 슬롯: 원거리는 근접 슬롯 도달성 검사 대상이 아니라 바뀌지 않는다(테스트로 확인).
- 경로: 경로 처리기 변경은 공격 유형을 가리지 않으므로 D-063의 원거리 규칙("사거리에 들거나 접근점에 닿을 때까지 이동")이 Nav 밖 플레이어에게도 적용된다. Confirmed 원거리는 사거리 밖이면 MoveTarget 직진 대신 접근점까지 경로로 가고(사거리 안이면 지금처럼 선다), 슬롯 대기 Alert 원거리는 제자리 대신 접근점까지 걷는다. 절벽 위 플레이어에게 이미 하던 동작이다. 원거리 사선 확보(D-086)는 바꾸지 않았다.

### 7.4 5a 검증

- 테스트(`LootNPop.SurfaceNavigation.Nav.EnemyReachability`에 추가): 땅 위·Node 없음 → 새 슬롯 없음, 기존 슬롯 1.5초 유지 뒤 반납, 끝점 Reachable이어도 반납. 공중 → 기존 슬롯 보존·새 슬롯 없음·유예 시계 유지, 1.5초 뒤 Nav 밖 착지 → 반납. 끝점 Stale·이전 generation 바닥·바닥 식별 실패 → 기존 슬롯 무기한 보존·새 슬롯 없음. Nav 복귀 → 즉시 허용. 실제 Mass 슬롯 subsystem에서 근접·승격 근접 반납, 원거리 유지. Node 없는 목표(투영 반경 밖 1,000cm)로 접근 요청 → `NoNode` + 자기 group 접근점 경로. 바위 위 층 빌리기(`BorrowBlockerFloorSurface`): 정적 Blocker·같은 slot·현재 generation이면 빌림, Support 비보행·동적·미등록·다른 slot·직전 없음·이전 generation이면 빌리지 않음. 플레이어 주머니 판정과 anchor 대상 조향은 Mass 플레이어(Character) 없이 헤드리스로 만들 수 없어 PIE로 확인했다(`IsInNavPocket` 자체는 `EnemyMovement.DirectCorridor`가 덮는다).
- 빌드·회귀: Game·Editor 빌드(경고·오류 없음), 헤드리스 전체 `LootNPop.SurfaceNavigation` **141/141**(첫 실행 140/141은 공중 유예 테스트 기대값 오류, 수정 후 `Unit5/SurfaceNavFull2`·`SurfaceNavFull3`·최종 `SurfaceNavFull4`), 재저장 시험편3개 원복. 결과 `Saved/Profiling/Phase16/SmallBlockerDetour/Unit5/`.
- 패키지 비용: 측정 생략. 슬롯 판정은 플레이어 대상 적 항목마다 상수 비교 하나가 늘 뿐이다. 경로 처리기는 Nav 밖 플레이어를 노리는 적에게만 `ResolveEndpoints` 1회와 경로 요청(D-063 접근, 절벽 대상과 같은 비용)이 생긴다. 이전에는 이 적들이 경로 없이 MoveTarget으로 직진했다. Nav 밖 플레이어는 `ProductionGround`·`Production` 측정 시나리오(헤드리스 고정 위치)에 나오지 않아 Gate 수치에 실리지 않는다.

### 7.5 5a PIE

| 차수 | 관찰 | 조치 |
|:---|:---|:---|
| 1 | 단위4 주머니 재현(WIP slot7). 근접 830이 Confirmed로 MoveTarget 115cm 앞에서 정지(`endpoint=2 pathStatus=3 steering=0 approachUnreachable=0`). 플레이어 줄 `grounded=1 slot=65535 layer=65535 node=-1`, 바닥 `StaticMeshComponent0 face=-1 slot=7 layer=65535`, 발이 적 발보다 약0.5m 높음 → 주머니 속 낮은 바위 위라 바닥 층이 없어 5a 조건 밖. 주머니에서 나오면 정상 추격 | 바위 위면 직전 지지면 층을 빌림(사용자 승인) |
| 2 | 같은 주머니(WIP slot2, 바위 규칙 반영). 근접 382가 Confirmed로 목표 node 근처에서 정지(`endpoint=0 waypoints=1 waypoint=1 steering=1 steerDist=0 endWalk=0`, 시작 node (129,102)). 플레이어는 맨땅(`slot=2 layer=0`)인데 투영 반경 300cm 안의 바위 너머 node(`node=326543`)가 잡혀 Nav 위로 판정 → 슬롯 Reachable, 마지막 접근 걸음 검증(D-090 A)이 막혀 목표 node에서 무한 대기. 주머니에서 나오면 정상 추격 | 플레이어 판정에도 Nav 주머니 규칙(D-090) 적용(사용자 승인) |
| 3 | 같은 주머니(WIP slot4, 주머니 규칙 반영). 플레이어 `pocket=1 walkAnchored=1`(anchor 검증 통과). 근접 918이 경로를 다 쓴 뒤 슬롯 위치로 직진하다 정지(`steering=0 moveTargetDist=70`, 플레이어와 평면 243cm·높이 47cm). 주머니 밖·평지 작은 바위 위 추격·공격 정상 | anchor 대상은 경로 뒤 플레이어 발로 조향(사용자 승인) |
| 4 | 같은 주머니(WIP slot7, 사용자 풀빌드). 플레이어 투영 반경 밖 `walkAnchored=1 pocket=0`. 근접 6마리 모두 `steering=1`로 플레이어 발을 향하지만 가장 가까운 1061도 평면 252cm·높이 67cm 아래에서 막혀 Confirmed로 슬롯을 쥔 채 정지. 주머니 밖·평지 바위 위 추격·공격 정상 | 사용자 결정으로 마무리, 남은 한계로 5b에 넘김(7.6) |

- 정리: 5a의 Nav 밖 → 도달 불가 경로(`node=-1`)는 네 번의 재현 모두 다른 판정(바위 위 식별 실패 → 바위 너머 투영 node → anchor 통과)이 먼저 걸려 PIE에서 직접 보지 못했고 헤드리스 테스트로만 확인했다. 주머니 밖 추격과 평지 작은 바위 위 추격·공격은 2~4차에서 회귀 없음을 확인했다.
- 남은 한계: 보행 anchor 검증(기본 캡슐 반지름35, 평면 150cm·높이 150cm+단차 허용)은 통과하지만 실제 근접 적이 공격 거리에 들지 못하는 주머니가 있다. 검증이 넉넉한 탓인지 6마리가 입구에 몰린 탓인지 로그로 가르지 못했다. 이때 적은 Confirmed로 슬롯을 쥐고 선다.
- 풀은 매 PIE 뒤 MCP `ObjectTools.set_properties`(메모리, SaveConfig 없음)로 원래 값으로 되돌렸다(`Unit5/SelectionState.json=restored`). `LNPSettings`는 Python에 노출되지 않아 `py` 스크립트로는 바꿀 수 없다. 추적 Content/Config 변경은 없다.

### 7.6 5b 계획 — 플레이어 발자취(계획 당시 기록, 구현 결과는 7.7)

플레이어가 걸어 들어간 길은 이미 걸을 수 있다고 증명된 길이다. 그 길을 적 캡슐로 검증해 Nav 없는 바닥까지 추격을 잇는다.

- 5a에서 넘겨받는 것(7.5 4차): 보행 anchor는 통과하지만 실제 근접 적이 공격 거리에 들지 못하는 주머니. 5b를 설계할 때 먼저 확인한다.
  - 검증 캡슐을 실제 적 설정(`Shared.Config->CapsuleRadius`·`CapsuleHalfHeight`)으로 바꾸면 사라지는지, 플레이어 쪽 anchor 도달 판정(`PlayerWalkAnchorReach` 150cm·높이 허용)을 근접 공격 거리(3D, `GetChaseStopDistance`)로 좁히면 사라지는지 본다.
  - 발자취가 있으면 우선순위상 발자취가 anchor보다 뒤이므로, anchor가 잘못 통과하면 발자취도 쓰이지 않는다. 주머니(`bInNavPocket`)에서는 발자취를 anchor보다 앞에 둘지 정한다.
  - 예비안(D-090 정지 감지): Nav 밖·anchor 대상의 Confirmed 근접 적이 공격 거리 밖에서 일정 시간 거리를 줄이지 못하면 그 적의 슬롯을 도달 불가로 반납해 Alert로 보낸다. 5b로 해결되지 않을 때만 쓴다.

- 기록
  - 투영 반경 밖으로 나갈 때(Node가 투영 node에서 보행 anchor·없음으로 바뀔 때) 마지막 Nav node를 **입구 node**로 저장한다.
  - 땅에 붙어 이동하는 동안 약 100cm마다 발 위치를 점으로 남긴다. 플레이어당 20~30점 상한이다.
  - 다시 Nav 위(투영 node)로 오면 지운다.
- 검증
  - 새 점마다 직전 점→새 점(첫 점은 입구 node 지점→첫 점) 구간을 적 기본 지상 캡슐(보행 anchor와 같은 `AnchorParams`)의 `WalkReaches`로 걸어 본다. 실패하면 발자취를 끊는다.
  - 공중에서는 점을 기록하지 않는다. 착지점 구간(이륙 직전 점→착지점)을 검증하므로 절벽 낙하·그래플·런처·적이 못 오르는 단차가 자연히 끊긴다.
- 추격
  - 플레이어 Nav 소속 우선순위: 투영 → 보행 anchor(D-085) → 발자취.
  - 발자취가 있으면 플레이어 Nav 소속을 입구 node로 둔다(`Node`·`Group`·`GroundGroup`). 슬롯·도달성·A*가 정상으로 돌아가고 5a 도달 불가 규칙은 발동하지 않는다.
  - 적은 입구 node까지 경로로 간 뒤 발자취 점을 waypoint처럼 따라간다(D-088 지름길과 같이 Nav 밖 구간에서 anchor로 되돌리지 않는다). 마지막 접근은 D-090 규칙(직선 판정 또는 `WalkReaches` 검증, 막히면 대기)을 따른다.
  - 끊기거나 상한을 넘으면 5a 규칙(도달 불가 → 1.5초 뒤 슬롯 반납·접근점 Alert)으로 떨어진다.
- 위치·비용
  - 플레이어 바닥 판정 루프(`ULNPPlayerNavProcessor::Execute`, `LNPEnemyPathProcessor.cpp` 150행 근처의 보행 anchor 분기 옆)에 붙인다. 점 저장은 `FLNPPlayerNavFragment`(또는 서버 전용 별도 fragment)에 고정 크기 배열로 둔다.
  - 비용은 플레이어 수에 비례하고 서버 전용이다. 점 하나당 `WalkReaches` 1회(약 100cm, 캡슐 반지름 걸음 3~4번)다.
  - 적 쪽 소비(발자취 waypoint 추종)는 경로 처리기에서 개체마다 돈다. 적마다 비용이 늘어나므로 패키지 Gate를 측정한다.
- 검증 계획
  - 헤드리스 3종: 꺾인 주머니 추격(입구 node → 꺾인 발자취 → 플레이어 도달), 절벽 낙하 시 끊김(착지 구간 `WalkReaches` 실패 → 5a 도달 불가), 상한 초과 시 5a로 전환.
  - 단위4 주머니(WIP Ridge, 벽 모서리+바위 `GraphicsRockProbe_Moss_01` 주변)를 PIE로 재현해 근접 적이 주머니 안까지 따라오는지 본다.
  - 패키지 `RunPhase13Gate.ps1 -Cases ProductionGround,Production` 교대 측정.

### 7.7 5b 결과 — 플레이어 발자취(D-092, 2026-11-18)

7.6 계획을 0단계 진단 결과와 PIE 6회로 고쳐 구현했다. 마지막 두 묶음(발자취 모서리 보완·적 발자취 정체 건너뛰기, 추격 자격 판정)은 사용자 결정으로 컴파일만 확인하고 커밋했다. 그 PIE와 패키지 재측정은 다음 세션이 맡는다(7.7.6).

#### 7.7.1 0단계 — 5a PIE 4차의 anchor 오통과 원인

5a PIE 4차 원본 로그(`Saved/Logs/LootNPop.log`)로 다시 보니 맨 앞 근접 1061은 플레이어 anchor node 111cm 옆에서 플레이어 발로 조향 중이었다. 공격 정지 거리(170, 공격 거리 200−30)에 50cm 모자랐다. 플레이어는 적 발보다 67cm **위**였다(7.5의 "아래"는 반대). Melee01 실제 캡슐은 반지름 42·반높이 96으로, anchor 검증의 기본 캡슐(35·88)보다 크다.

후보는 다섯이었다: ① 캡슐 차이, ② 출발점·경로 차이(검증은 node 정확한 지점에서 직선 하나), ③ 높이 허용(평면 150cm + 높이 195cm), ④ 보폭 차이(검증 35cm, 실제 약 10cm), ⑤ 입구 혼잡(분리력 반경 90·세기 300). 로그로 가를 수 없어 사용자 결정으로 원인부터 확정했다. `WalkReaches`에 진단 인자(`FWalkProbe`: 보폭·끝난 사유·최근접 지점·궤적)를 더하고, `NavReport`가 `[NavProbe]` 줄로 anchor 검증을 변형해 다시 걷게 했다.

| 프로브(slot2, NavReport 2회) | 결과 | 판정 |
|:---|:---|:---|
| `anchorCheck`(node 출발·기본 캡슐) | 통과, 3D 131·162 | 재현 |
| `anchorMeleeCapsule`(node 출발·r42) | 통과, 3D 136·138(정지 170 안) | ① 아님 |
| `anchorClosest`·`anchorMeleeClosest`(30cm까지) | 3D 83~94까지 접근 | ③ 아님 |
| `enemy*`(실제 적 자리 출발, 캡슐·보폭 3종) | 첫 걸음부터 Rejected | ④ 아님 |

앞선 적들은 anchor node가 아니라 옆 node 근처에 있었고, 경로를 다 쓴 뒤 그 자리에서 발로 곧장 가다 막혔다. 분리력은 최대치(300)였고 대부분 옆 성분이었다. 원인은 ② 출발점·경로 이탈이고 ⑤ 혼잡이 이탈을 키웠다. anchor가 검증한 것은 node → 발 직선 하나인데, 적은 그 선 밖에서 출발해도 되돌릴 장치가 없었다. 로그는 `Saved/Profiling/Phase16/SmallBlockerDetour/Unit5b/Step0_ProbePIE.log`.

#### 7.7.2 구현

- **플레이어 발자취**(`LNPEnemyNavigation::UpdatePlayerTrail`, `FLNPPlayerTrail`, 서버 전용)
  - Nav 위(투영 node, 주머니 아님)에서 발 삼각형이 트였으면 그 node와 발을 시작점으로 기억한다. Nav 밖·주머니로 나가면 [입구 node 지면점, 트인 자리 발]로 시작한다(600cm 밖이거나 층이 다르면 끊김).
  - 직전 확인 자리에서 25cm 움직일 때마다 마지막 점 → 발 직선을 검증 캡슐로 걸어 보고(`WalkReaches`, 도달 50cm), 통과하면 100cm마다 점을 남긴다. 직선이 막히면 마지막으로 통과한 자리를 점으로 남기고 거기서 다시 걷는다. 그래도 막히면 끊는다(모서리를 곡선으로 돈 플레이어의 100cm 직선이 벽에 막혀 걷기만 해도 21번 끊긴 PIE 5차 뒤 추가).
  - 착지는 거리와 무관하게 검증한다(절벽·그래플·런처·벽 비비며 점프·적이 못 오르는 단차가 끊긴다). 바위(빌린 층, D-091) 위는 공중처럼 기록도 끊김도 멈추고, 내려와 지지면에 서면 착지처럼 검증한다(PIE 1차 뒤 추가).
  - 앞선 점 50cm 안으로 돌아오면 그 뒤를 지운다(고리 제거). 상한 24점. Nav로 돌아오면 지운다. 끊긴 발자취는 Nav로 돌아올 때까지 쓰지 않는다.
  - 검증 캡슐은 지상 적 설정 중 가장 큰 캡슐이다(`ULNPPlayerNavProcessor`가 적 청크의 공유 설정에서 매 프레임 고른다, 현재 r42·반높이 96).
- **Nav 소속 우선순위**: 투영 node → 발자취 입구 node(`bTrailAnchored`) → 보행 anchor → 없음(D-091 도달 불가). 발자취·anchor 모두 `bWalkAnchored`이고 `WalkAnchorPoint`가 머리 node 지면점이다.
- **보행 anchor 궤적 점열**: 플레이어 anchor 검증도 가장 큰 캡슐로 걷고, 걸은 궤적을 50cm마다 솎아 최대 12점(`AnchorPath`, [0]=anchor node)으로 남긴다(`BuildAnchorPath`). 궤적이 바뀔 때만 `AnchorRevision`이 는다. node → 발 한 점만 주던 5a 방식은 적이 선을 벗어나면 되돌릴 수 없었다(0단계·PIE 1·3차).
- **적 점열 추종**(`ULNPEnemyPathProcessor`, `SyncTail`·`FollowTail`)
  - 경로를 다 쓰면 머리 node부터 점열(발자취 또는 anchor 궤적) → 플레이어 발을 따라간다. 점 50cm 안이면 다음 점이다. 정지는 근접 정지 거리다.
  - 따라가는 동안은 Nav 밖이어도 자기 anchor·트인 자리로 되돌리지 않는다(D-088 지름길과 같은 규칙). 머리·개정·종류가 바뀌면 100cm 안의 가장 뒤쪽 점으로 다시 맞추고, 없으면 경로로 머리부터 다시 간다.
  - 현재 점까지 1초 동안 10cm도 가까워지지 못하면 직전 점으로 한 칸 물러난다. 머리 점에서도 막히면 점열을 놓고 3초 동안 걸어서 닿는 node(D-085)부터 경로를 다시 낸다(PIE 3차: 옆 자리에서 머리 node로 가는 직선도 막혀 영구 정지).
- **적 자기 발자취**(`Crumbs`, 50cm·최대 12점): 점열을 따라가는 동안 남기고, 주머니·Nav 밖에서 걸어서 닿는 node가 없으면 최근 점부터 되짚는다. 점까지 1초 정체하면 건너뛰고, 다 쓰면 D-090의 마지막 트인 자리 규칙으로 간다. 트인 Nav에 서면 지운다(PIE 2차: 플레이어가 먼저 나가자 주머니 속 적이 트인 자리 한 점으로 곧게 못 가 영구 정지).
- **추격 자격 판정**(`ULNPEnemyScoringProcessor`): 근접 도달성을 플레이어 물리 발이 아니라 Nav 소속으로 본다. 땅 위·바닥 식별·소속 없음이면 자격 없음, 소속이 있으면 `GetNavQueryPoint()`로 본다. 발 투영은 주머니 속 플레이어를 바위 너머 node로 "도달 가능"으로 보아, 슬롯(D-091)은 못 받는데 경계 인내 시계도 0으로 되돌아가 근접 적이 Alert로 영구히 섰다(PIE 6차).
- **진단**: `NavReport` 플레이어 줄에 `trailAnchored trail trailBroken trailRevision trailBreaks lastBreak lastBreakFeet anchorPath anchorRevision`(lastBreak 1=시작 자리 없음·2=층·3=상한·4=걸음), 적 줄에 `tail tailBest tailStallAge crumbs recover`, `[NavProbe]` 줄(anchor 검증 변형 4종, 근처 지상 적 3마리의 플레이어 발·조향점 걸음과 분리력).

#### 7.7.3 PIE 경과(WIP Ridge 검증 풀, 매번 메모리 지정·복원)

| 차수 | 관찰 | 조치(사용자 승인) |
|:---|:---|:---|
| 0단계 | 7.7.1 | 원인 ② 확정 |
| 1 | 플레이어가 주머니 속 바위 위라 발자취 끊김(`borrowedSurface=1`) → anchor. 옆으로 밀린 적이 머리 node로 돌아가지 못하고 `tail=0`에서 정지 | 바위 위 기록 멈춤 + 조향점 걸음·정체 시계 진단 |
| 2 | 다른 위치. 플레이어는 Nav 위, 발자취로 따라 들어간 적(`tailBest` 흔적)이 주머니에서 트인 자리 한 점으로 곧게 못 나와 정지 | — (3차와 묶음) |
| 3 | 또 다른 위치. 발자취 끊김 → anchor, 적 2마리가 머리 node로 가는 첫 걸음부터 막혀 `tail=0` 정지(정체 시계는 1초마다 돎) | A anchor 궤적 점열, B 머리 정체 시 경로 회복, C 끊김 사유 진단, D 적 자기 발자취 |
| 4 | 벽 비비며 점프로 들어간 자리: 착지 검증으로 끊김(`lastBreak=4`) → 도달 불가 → 근접 6기 1.5초 뒤 Alert, 8초 뒤 None, Pod 귀환. 의도대로 | 사용자 확인: 지금까지 재현을 점프로 했다 |
| 5 | 걷기만으로 적을 주머니에 넣음. 플레이어 발자취가 걷기만으로 21번 끊김, 주머니 속 적이 자기 발자취를 되짚다 첫 걸음부터 막혀 1분 넘게 정지 | 발자취 모서리 보완, 적 발자취 정체 건너뛰기·50cm 간격 |
| 6 | 바위를 밟고 넘어 들어간 자리: 착지 검증으로 끊김 → 도달 불가, 근접 4기 Alert로 접근점에 섬(의도). 그러나 10초 넘게 None으로 안 내려감 | 추격 자격 판정을 Nav 소속 기준으로(컴파일만) |

- 사용자: 이 지형에서 걸어서 갈 수 있는 곳은 모두 추격·공격이 잘 된다. 걸어서만 들어가는 Nav 없는 꺾인 주머니는 이 지형에서 재현하지 못해, 발자취를 따라 들어가는 장면은 헤드리스로만 확인했다.
- 도달 불가 뒤 대기 시간(Melee01): 슬롯 유예 1.5초 → Alert(접근점에서 대기) → `AlertPatienceTime` 8초 → None(`AlertRecoveryTime` 1초 재발견 금지) → `HomeRadius` 3,000cm 밖이면 Pod 귀환, 안이면 배회. 홈 안에서 시야에 플레이어가 남으면 Alert 8초 ↔ None 1초를 반복할 수 있다(entity 192, 7.7.6).

#### 7.7.4 검증

- 헤드리스 `LootNPop.SurfaceNavigation.EnemyMovement.PlayerTrail` 신설: 꺾인 주머니(곧장 313cm에서 막힘 → 발자취 143cm), 0단계 재현(옆으로 밀린 적이 곧장 291cm → anchor 궤적 점열 141cm, 끝점 조향에서 시작해 정체 뒤 물러남), 머리 점 정체 시 점열 놓기, 착지(평지 유지·높은 상자 끊김·5a 전환·Nav 복귀 시 해제), 상한·고리(되돌아오면 5점으로 줄고 상한에서 끊겨 5a).
- 헤드리스 전체 `LootNPop.SurfaceNavigation` **142/142**(발자취 모서리 보완·적 발자취 정체까지 반영한 `Unit5b/SurfaceNavFull3`). 마지막 추격 자격 판정 수정은 Game·Editor 빌드만 확인했다(헤드리스 미실행). 시험편 3개는 매번 원복했다.
- 패키지 Gate(초기 5b 구현, A~D·이후 수정 전): 단위4 최종 패키지(`SurfaceNavigationSegmentCorridorPackage`, fa7720c)와 5b 패키지(`SurfaceNavigationPlayerTrailPackage`)를 교대로 2회씩, 8회 모두 Gate PASS. ProductionGround frame P95 8.67·7.67 → 8.11·8.17ms, consumer P95 0.164·0.142 → 0.142·0.159ms, Production frame P95 6.97·7.18 → 7.00·7.17ms, 모두 편차 안. 측정 시나리오에 Nav 밖 플레이어가 없어 점열 추종 비용은 실리지 않는다. 결과 `Unit5b/Cost/`.

#### 7.7.5 비용

- 플레이어(서버, 플레이어 수 비례): 매 프레임 적 청크 공유 설정 순회(가장 큰 캡슐) 1회, Nav 밖에서 25cm마다 `WalkReaches` 1~2회(약 100cm), anchor 검증은 기존과 같은 주기(0.5초·50cm)에 궤적 기록만 더한다.
- 적: 점열 추종 중에는 점 하나 조향과 거리 비교뿐이고 걸음 검증이 없다. 경로 처리기 fragment는 점열 상태와 자기 발자취(FVector3f 12개)로 약 180바이트 늘었다.

#### 7.7.6 다음 세션 인계

- 마지막 두 묶음의 PIE: 걷기만 해도 발자취가 끊기지 않는지(`trailBreaks`), 주머니 속 적이 자기 발자취로 나오는지(`crumbs`), 도달 불가 플레이어가 주머니 안이어도 8초 뒤 None으로 내려가 Pod으로 돌아가는지.
- 최종 코드로 패키지 재쿡·Gate 교대 재측정(7.7.4 수치는 초기 5b 구현).
- 반복 정지 자리: Ridge의 (133,101) 근처에서 근접 적이 640cm 앞 waypoint(130,102)로 곧장 못 가 서는 일이 5a 코드부터 PIE마다 나왔다(entity 516·518·148·192). waypoint 조향 선분을 검사하지 않는 기존 경로 추종 한계(D-090 "검사하지 않음")로 보이며 5b와 무관하다.
- 원인 프로브(`[NavProbe]`, `FWalkProbe`)는 다음 진단에 쓰도록 남겼다. 필요 없어지면 정리한다.
- 5a 예비안(공격 거리 밖 정체 시 슬롯 반납)은 쓰지 않았다.

## 8. 단위6 — 5b 마무리 검증과 Nav 가장자리에 낀 적(D-093·D-094·D-095)

§7.7.6 인계를 받았다. 사용자 PIE(TestMap03 1인, WIP Ridge 검증 풀 메모리 지정)에서 피드백 한 건마다 원인을 로그로 밝히고 사용자 승인 뒤 고쳤다. 같은 Ridge 턱에서 갈래가 이어져, 범위는 매번 사용자와 정했다.

### 8.1 인계 항목 처리

| 항목 | 결과 |
|:---|:---|
| 1. 5b 마지막 두 묶음 | 헤드리스 전체 142/142(`Unit6/SurfaceNavFull1`). PIE: 걷기만으로는 주머니 재현이 매우 어려웠고(사용자), 재현 대부분은 점프라 발자취 끊김(`lastBreak=4`)은 의도대로. 도달 불가 뒤 None → Pod 귀환은 PIE 1차 entity 748에서 확인 |
| 2. Ridge (133,101) 반복 정지 | 원인 확정(8.2), 안1(D-093)·안2(D-095)로 수정 |
| 3. Alert ↔ None 반복 | 사용자 결정 B(포기 플레이어 기억, D-094) |
| 4. 패키지 재측정 | 8.6 |
| 5. 진단 프로브 | 사용자 결정으로 유지. `FWalkProbe`는 보행 anchor 궤적 점열(D-092)이 쓰므로 진단 전용이 아니다. `[NavProbe]`는 `NavReport` 때만 돈다 |

### 8.2 Ridge (133,101) 정지 원인

- 사례: 5a·5b의 516·518·148·192, 단위6 PIE의 748·672·673·410. 모두 같은 옥탄트 로컬 자리(slot만 다름).
- 지형(`Unit6/ExplainNavGap_Ridge133.log`): (129~131,101) 줄이 높이 약30cm의 가파른 턱(atlas 비보행, 경사 dot 0.69~0.87, 캡슐 불가). 102 줄 node는 이 턱에 붙은 불완전 삼각형이다. (128,101)은 바위 `GraphicsRockProbe_Moss_01`, (128,103)은 다른 바위.
- 경로: `SimplifyPath`가 node↔node 격자 직선(`IsDirectWalkable`, 약1m 허용)으로 (133,101)→(130,102)를 한 구간으로 합쳤다. 실제 선분은 턱 칸 (131,101)의 삼각형을 지난다(`waypointSeg=0`). D-090이 "조향 선분은 검사하지 않는다"고 한 근거(waypoint 사이 선은 안전)가 여기서 깨졌다.
- 정지: 적이 분리력·경사로 node 옆 불완전 삼각형(node에서 65~150cm)에 서면 조향점 쪽 첫 걸음이 비보행 hit로 `Rejected`다(`[NavProbe] enemyToSteer` 0걸음). D-087 옆걸음은 Blocker 겹침만 다루고, waypoint 추종에는 정체 감지가 없어 목표가 바뀔 때까지 섰다.

### 8.3 결정과 구현(모두 사용자 승인)

- **D-093 경로 단순화 삼각형 통로**: `SimplifyPath`의 합치기 판정을 `IsSegmentWalkable`로 바꿨다. 처음 헤드리스(`SurfaceNavFull4`)에서 단순화가 아무 구간도 합치지 못해 `Nav.RegressionPath`·새 단언이 실패했다. 원인은 판정의 수치 오차 두 가지였다: 끝점이 꼭짓점(node 지면점)이면 투영 오차로 꼭짓점 반대편(구멍 쪽) 삼각형을 보는 아주 짧은 구간이 생기고, 격자선을 그대로 따라가는 선분은 오차가 고른 한쪽 삼각형만 본다. 격자 길이 1e-3 미만 구간은 건너뛰고(제자리 판정은 그대로), 격자선 위 구간은 양쪽 중 하나라도 트였으면 통과로 고쳤다(허용 오차 1e-6은 node 좌표 정밀도 약1e-5보다 작아 실패, 1e-3으로). 비용 추정: 경로 생성 때만 늘고(초당 2~3건) 추종의 프레임당 호출 수는 그대로다.
- **D-094 포기 플레이어 기억**: 근접 지상 적이 인내를 소진하면 `FLNPEnemyTargetingCandidateFragment::GivenUpPlayer`에 남기고, 그 플레이어가 근접 도달 불가인 동안 시야 재발견을 막는다. 추격 자격과 같은 도달성 판정을 `IsMeleeReachable` 람다로 묶었다(판정 자체는 그대로). PIE 4차: 포기한 410이 312cm 앞 시야의 도달 불가 플레이어를 20초 동안 다시 잡지 않음(`candidates=0`).
- **D-095 낀 적의 회복**
  - ① waypoint 정체 회복(안2): 조향 waypoint로 1초 동안 10cm도 못 가까워지면 경로를 놓고 5b의 회복(3초, 걸어서 닿는 node부터)을 쓴다. 회복 도착 판정을 100cm에서 50cm(`TailArrival`)로 좁혔다(node 99cm 옆에 낀 410이 그대로 회복을 끝냈다). 닿는 node가 없으면 자기 발자취·트인 자리. 처음에는 "걸으려는 중"을 `MoveTarget.DesiredSpeed`로 봤는데 Confirmed 추격 중에도 0이라 감지가 매 프레임 초기화됐다(PIE 6차 826·830). 이동 프로세서가 정한 속도를 `FLNPEnemyFragment::LastMoveIntentSpeed`로 남겨 쓴다.
  - ② 목표 node에서 대기(C1): 마지막 접근이 막혀 기다리는 적(D-090 A)이 node에서 130cm 떨어진 턱 아래 경사에 선 채 30초 섰다(PIE 5차 830·829). 50cm 밖이면 목표 node로 돌아가 기다리고, 복귀도 ①의 감지 대상이다.
  - ③ 적 자기 발자취 항상 기록(D): 분리력에 밀려 꺾인 주머니에 들어간 근접 3기가 발자취 없이(`crumbs=0`, 점열 추종 중에만 기록) 트인 자리로 곧장 가다 섰다(PIE 7차). 발이 트인 삼각형 밖이면 늘 남긴다(되짚는 중 제외).
- 진단: `NavReport` paths 줄 `waypointStalls`, 적 줄 `waypointStallAge`.

### 8.4 PIE 경과(WIP Ridge 검증 풀, 매번 메모리 지정)

| 차수 | 관찰 | 조치 |
|:---|:---|:---|
| 1~3 | 5b 최종 코드. Ridge 턱 옆 748(Alert → None 뒤 귀환 경로로 빠져나감)·672·673 정지, 672(2차)는 경로 소진 뒤 대기 | 원인 확정(8.2), 안1 |
| 4 | 안1+B. B 동작 확인. 410이 (129,102) node 99cm 옆에서 정지(인접 edge 구간이라 안1 범위 밖) | 안2 |
| 5 | 안2. `waypointStalls=3`. 플레이어가 턱 위 node (132,101)에 서자 830·829가 경사에서 대기, 826은 슬롯 도착했으나 공격 거리 202>200 | C1 |
| 6 | 826·830이 waypoint 조향 중 정지인데 `waypointStallAge=0` | 감지 조건을 `LastMoveIntentSpeed`로 |
| 7 | 주머니로 밀려 들어간 3기가 트인 자리로 직진하다 정지 | D |
| 8 | 안2·C1·감지 수정 빌드로 여러 번 시도. 10초 이상 정지 재현 안 됨, 대부분 몇 초 안에 추격 재개 또는 Pod 귀환(사용자) | — |

- D 자체는 재현이 드물어 PIE로 보지 못했다(헤드리스·컴파일만).
- 환경 함정: 헤드리스(`UnrealEditor-Cmd`)가 먼저 뜨면 MCP 포트 8000을 차지해, 풀 지정이 사용자 에디터가 아니라 헤드리스에 들어갔다(그 헤드리스는 오염으로 중단). 이후 헤드리스는 `-ModelContextProtocolPort=8099`.

### 8.5 검증

- 헤드리스 전체 `LootNPop.SurfaceNavigation` **142/142**: 5b 최종(`SurfaceNavFull1`), 안1·B·안2·C1(`SurfaceNavFull6`), 최종 D 포함(`SurfaceNavFull7`). 경고는 기존 HTTP timeout·B-option 안내. 시험편 3개는 매번 원복.
- `EnemyMovement.DirectCorridor`에 단순화 꺾임(node 열 (2,0)→(1,1)→(0,1)→(-1,1)이 빠진 node의 삼각형을 가로지르지 않고 (0,1)에서 꺾임)·꼭짓점 끝 선분 허용·격자선 위 선분 허용 단언을 더했다.
- D-094·D-095는 Mass 플레이어·이동 프로세서가 필요해 PIE로 확인했다.

### 8.6 비용

`RunPhase13Gate.ps1 -Cases ProductionGround,Production`, 5b 패키지(`SurfaceNavigationPlayerTrailPackage`)와 단위6 최종 패키지(`SurfaceNavigationUnit6Package`, exe `6FF50028…`)를 교대로 2회씩. 8회 모두 host·guest 종료0·Gate PASS. 결과 `Unit6/Cost/`.

| | ProductionGround frame P95(ms) | consumer P95(ms) | 경로 요청(캡처) | 경로 추종 프레임·waypoint | Production frame P95(ms) |
|:---|:---|:---|:---|:---|:---|
| 5b 이전 R1·R2 | 8.83·8.76 | 0.195·0.169 | 40·43 | 28,633/80·35,745/84 | 7.59·7.41 |
| 단위6 이후 R1·R2 | 8.55·8.40 | 0.142·0.147 | 44·29 | 38,425/97·25,117/69 | 7.61·7.44 |

- 해석: 모든 수치가 이전 측정 편차(단위4·5b 표) 안이다. 단순화 변경(D-093)으로 waypoint 수가 늘 수 있지만 측정에서는 방향이 일정하지 않았다. 정체 감지(D-095)는 지상 27기 교전에서 `waypointStalls=0`(두 회 모두)이라 군중 속 오탐이 없었다.

### 8.7 남은 것

- 슬롯 위치까지는 왔지만 턱 때문에 공격 거리에 들지 못하는 근접 적(PIE 5차 826, 202cm)은 Confirmed로 선다. 플레이어가 Nav 위라 슬롯을 반납시키면 다른 적이 같은 슬롯을 다시 잡아 반복하므로 고치지 않았다.
- 밀려 들어온 길이 거꾸로도 막히면(한쪽으로만 내려갈 수 있는 턱) 적 발자취로도 못 나온다.
- 지형 쪽 근본 원인(높이 약30cm 가파른 턱 옆 node 줄, D-089 바닥 허용 node 포함)은 베이크·지형 작업 범위다.
