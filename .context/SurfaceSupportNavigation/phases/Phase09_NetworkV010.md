# Phase 9 패키지 리슨2P·late join V010

> 상태: 패키지 리슨2P(Normal)·late join 기능 대조 통과. host exact P95 초과는 성능 단위에 남는다
> 날짜: 2026-10-12
> 범위: 단위5의 `-game` 리슨2P host/guest 동일 snapshot·동적 요소 상태·late join 검증(D-031)

## 1. 검증 수단 추가

기존 2P harness(`-LNPLoadBaselinePlayers=2`, host `TestMap03?Listen` + guest `127.0.0.1`)는 host/guest 각각의 probe PASS만 보고, 두 프로세스의 동적 요소 상태는 대조하지 않았다.

- `ALNPMovingPanel::EvaluatePose(ServerTime)`: `UpdatePose`의 자세 계산을 Actor를 옮기지 않는 const 함수로 분리했다(동작 동일, `DynamicTerrain.MovingPanel.GravityRotation` 통과).
- `LNP.SurfaceNav.ProbePanelPoses`: 패널마다 `(slot, MarkerId)`·경로 revision·활성 여부, 고정 server time 60/97.3/150.1초의 계산 자세, 마지막 갱신 시각 기준 Actor 자세 오차를 남긴다. 패널 자세는 복제 경로·시작 시각·server time의 결정론 함수(D-027)이므로 같은 시각의 host/guest 자세가 같아야 한다. PASS 조건은 전 패널 활성·live 오차 ≤0.1cm/0.01°다.
- `LNP.SurfaceNav.ProbeWorldDevices`: 훅 앵커·스프링 런처·상태형 지형의 위치(상태형은 ID·상태·revision)를 정렬해 남기고 로컬 폰 위치를 함께 기록한다.
- 두 probe는 LoadBaseline 종료 probe 순서에서 `ProbePanels` 뒤에 실행된다.
- 로컬 스크립트 `run-package-2p.ps1`이 두 로그를 대조한다: 종료 코드, crash/identity 표식, 5개 probe PASS, SurfaceData 요약(generation·bindings·Nav component/group·seam) 일치, slot별 definition·source 수 일치, 패널 identity·고정 시각 자세(≤0.5cm·quat 성분 ≤1e-4), 장치 대조.

### 장치 대조 규칙

훅·런처는 net cull distance 300m 안에서만 guest에 복제된다. 따라서 개수 일치를 요구하지 않는다. guest의 모든 장치가 host의 같은 종류·위치(≤1cm)에 대응하고, guest 폰에서 250m 안의 host 장치가 guest에 모두 있어야 한다. 첫 시도는 전체 개수 일치를 요구해 FAIL(207 vs 50)로 나왔으며, 이는 relevancy에 의한 것으로 확인하고 규칙을 바꿨다(`Attempt1`에 보존). LevelInstance 이름의 런타임 고유 접미사도 프로세스마다 달라 비교에서 뺐다.

## 2. 빌드·회귀·패키징

- unity 묶음 변경으로 드러난 기존 잠재 충돌(`LNPSurfaceCrawlTest.cpp`의 `FParams`가 `LNPEnemyExactMovementTest.cpp`의 익명 namespace `using`과 모호)을 `LNPSurfaceCrawl::FParams`로 한정해 고쳤다.
- 새 프로세스 전체127검사 통과: 경고 없는122·경고 포함5(HTTP timeout4·기존 B-option1)·실패/미실행0, 489.6초. 입력 에셋7개 해시 유지, 테스트 생성 에셋3개 HEAD 복구.
- 그 보고서를 게이트로 BuildCookRun exit0. 이후 `ProbeWorldDevices`에 로컬 폰 위치 로그 한 줄을 더해 에디터 빌드 성공 후 같은 보고서로 재패키징했다(exit0/75.9초). **최종 패키지는 전체 회귀 이후 진단 로그 한 줄이 추가된 소스다.** exe SHA256 `12CF8712B664B7C266EED67A9E3815B5BFACC98DCA65F769BBDFFAEF8ACE0667`.

## 3. 결과

V009와 같은 정식 편성·`-LNPLoadBaselineProduction -LNPLoadBaselineSeed=1`이며 2P라 옥탄트 배치가 1P와 다르다(Bastion0/3·Meadow1/5/6·Hollow2/4/7, bindings269).

| 항목 | Normal(guest +5초) | LateJoin(guest +60초) |
|:---|:---|:---|
| 종료 | host·guest exit0, 70.0초 | host·guest exit0, 122.9초 |
| snapshot | generation1·8slot·bindings269/269·Nav component/group589·seam4281, host=guest | 동일 |
| slot 배치 | 8slot definition·source 수 host=guest | 동일 |
| 패널 | 11/11 활성, 고정 시각 자세 차0cm/0, live 오차0 (양측) | 동일 |
| 장치 | host 앵커207·런처45, guest 49·11. guest 미대응0, guest 250m 내 host 장치33 전부 존재 | guest 65·10, 미대응0, 250m 내48 전부 존재 |
| 상태형 지형 | production 0개 | 0개 |
| probe | Panels·PanelPoses·FaceIndex·SourceKeys·SurfaceData 양측 PASS | 양측 PASS |
| 표식 | UnknownHits/EnvelopeEscapes0, assert/ensure/fatal0 | 동일 |
| Spawn(host) | Pod120/120, 적2434/2434 | 동일 |

- Error 로그는 각 프로세스에서 기존 ShaderMap6과 플레이어 폰마다의 CharacterMovementComponent 추출1개(2P라 2개)뿐이다.
- headless 수치: host frame P95 8.94/9.67ms PASS, **host exact P95 3.419/4.089ms FAIL(≤2ms)**, lock PASS. guest frame P95 2.08/1.76ms, exact PASS, 수신 28.3/17.4KB/s. 고정 해상도 성능 Gate를 대신하지 않는다.
- 상태형 지형(기둥·다리)은 production에 배치가 없어 복제 상태 대조는 0개로 통과한 것이다. 상태 전환 late join은 해당 콘텐츠가 들어올 때 같은 probe로 검사한다.

로컬 근거는 `Saved/SliceOctant/NetworkV010`(FullEditorReport·DynamicTerrainReport·Normal/LateJoin host·guest 로그와 `_Result.json`·Attempt1·스크립트)다.

## 4. 남은 범위

- 성능·메모리 Gate: 고정 해상도 패키지로 D-076 편성의 새 옥탄트 seed 성능을 Meadow 기준과 비교하고, exact P95 초과 원인을 다룬다. 관통 검사4회 이상과 메모리 기준선(payload·decoded resident·Nav node/portal·exact 삼각형·cache 적중률)을 기록한다.
- NPC LoS·교전 재검증(D-017)은 별도 세션 소유를 유지한다.
