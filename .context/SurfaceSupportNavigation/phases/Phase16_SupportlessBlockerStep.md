# Phase 16 — Support 없는 Blocker 단차 금지와 ISM Support

> 상태: 1단계 진행 중 — 단위1 Nav 커버리지(D-084)·단위2 보행 anchor(D-085) 완료, Production gate·리슨2P 마감 남음(§3.7) / ISM Support 보류
> 착수: 2026-11-16, 그래픽스 1-B5b(GX-014) 인계
> 로그: `history/Phase16_Log.md`

## 1. 배경

그래픽스 1-B5b에서 Ridge WIP의 바위(Moss 약48~62cm·Sandstone 약20cm·Cliff 약2.1m)를 Static+Blocker(`LNPStaticBlocker`)로 승격하고 재베이크했다. PCG 바위는 ISM이고 수동 바위5개는 일반 StaticMeshComponent다. ISM Support는 D-058로 막혀 있어 Support는 주지 않았다. DrawNav는 바위 주변 node·edge를 끊었고 벽보행 적은 바위 표면에 붙었지만, **지상형 적은 발이 바위를 관통했다**. 근거와 재현은 [그래픽스 실행서](../../Graphics/phases/Phase01_EnvironmentImport.md) §24다.

## 2. 원인

| 경로 | 낮은 Blocker(< MaxStepUp 45cm) 처리 | 비고 |
|:---|:---|:---|
| Nav 베이크 여유 검사 | 막음 | 지면+1cm 캡슐 overlap/sweep(`LNPWorldExact`). DrawNav 차단과 일치 |
| 배회 구간 검증 | 막음 | 지면+2cm 캡슐 sweep(`LNPEnemyWanderProcessor`) |
| 활성 접지 exact 경로 | 막음(제자리) | `ProbeGroundedAt` 하향 probe가 바위 윗면(Blocker, Support 아님)에 맞아 `Rejected` |
| **활성 접지 캐시 경로** | **관통** | `SweepLateral`이 캡슐을 45cm 올려 측면 sweep → 바위 위를 지나감 → `TryResolveCachedGround`가 Atlas(지각) 높이로 접지. Atlas는 Blocker를 모른다 |

Moss처럼 MaxStepUp보다 살짝 높은 둥근 바위도 같은 경로로 관통한다. 올린 sweep은 윗부분만 보고 둥근 윗면을 따라 미끄러진 뒤, 캐시 접지가 캡슐 아랫부분을 바위 안에 놓는다.

## 3. 1단계 — "Support 없는 Blocker는 턱으로 넘지 않는다"

### 3.1 사용자 결정(2026-11-16)

| 항목 | 결정 | 기각한 대안 |
|:---|:---|:---|
| 판정 기준 | hit identity registry 역할 조회. 캐시 접지 결과의 캡슐이 Support 없는 Blocker와 겹치는지 overlap-multi로 검사 | ① `SweepLateral` 낮은 sweep: sweep은 첫 hit만 받아 경사에서 지각이 먼저 맞고, 캐시 접지 캡슐은 경사에서 r(1−cosθ)(30°에서 약4.7cm) 지면에 묻혀 경사 위 바위에 구멍이 남는다. ② 베이크 `NeedsExact` 플래그: 런타임 비용은 0이지만 전 옥탄트·fixture·WIP 재베이크, 동굴 벽 주변 캐시율 변화로 Phase 15 조건이 바뀐다. ③ 새 trace channel: profile·D-022 확장이 필요하다 |
| 위반 시 동작 | 이번 프레임 수평 이동 취소(`Rejected`, 제자리). exact 경로의 Blocker 위 probe와 같은 동작. 바위 높이와 무관하게 적용한다 | 미끄러짐 재시도: PIE에서 끼임이 보이면 다시 검토 |
| 검증 | 자동화 픽스처 + SurfaceNavigation 전체 + Ridge WIP PIE(그래픽스 §22 방식) + Phase 13 Production gate 비용 비교 | — |

### 3.2 규약

- **술어 `IsSupportlessBlocker`:** registry source가 Blocker 역할이고, Support 역할·Support face 표·patch face 표가 모두 없다. 대상은 `LNPStaticBlocker`(바위·LootPod proxy·동굴 Shell·엄폐 Block), patch가 없는 `LNPStatefulTraversal`, `LNPDestructibleBlocker`다. patch face 표가 붙은 상태형 통로와 Support+Blocker 지형은 대상이 아니다. 미등록(Unknown) component는 위반으로 보지 않는다.
- **검사:** 캐시가 HighConfidence를 준 위치의 캡슐을 반지름·반높이 1cm 줄여(`BlockerOverlapSkin`, 벽에서 물러나는 `WallSkin`과 같음) `LNPWorldExact`로 overlap-multi하고 역할로 거른다. 겹친 component를 모두 받으므로 발밑 지각·경사에 가려지지 않는다.
- **예외:** 이동이 없는 프레임(`Target == Location`)은 검사하지 않는다. 출발 위치도 이미 겹쳐 있으면(스폰·상태형 지형이 덮음) 빠져나가도록 막지 않는다. 이 확인은 위반이 감지됐을 때만 한다.
- **거부 시:** `OutLocation = Location`, 속도 0, Surface handle을 캐시 질의 전으로 되돌린다.
- **exact 경로·배회 경로는 바꾸지 않는다.** 둘 다 이미 같은 규칙을 지킨다(§2).
- **계측:** query 분류 `GroundBlockerOverlap`(`LNP.SurfaceNav.WorldCollision.Report`), `EnemyGround` 보고 끝 필드 `blocker_rejects`. 기존 `AnalyzePhase13GroundPopulation.ps1` 정규식은 줄 끝을 고정하지 않아 그대로 동작한다.

### 3.3 기존 Blocker 전용 형상의 예상 동작

| 형상 | 예상 | 확인 수단 |
|:---|:---|:---|
| LootPod proxy(반지름128cm 구, 중심 +128cm) | 변화 없음. 올린 캡슐(80~186cm)과 접지 캡슐(35~141cm)의 축이 모두 구 중심 높이를 포함해 멈춤 거리(축 간 163cm+WallSkin)가 같다 | 픽스처 6번 |
| 바위(Sandstone·Moss) | 겹치기 직전에 정지 | 픽스처 1·2번, Ridge WIP PIE |
| 동굴 Shell 벽 아래쪽 퍼짐·Floor/Shell 접합 턱, Grove 미세 Shell 분류면 | 벽을 따라 미끄러지던 개체가 정지할 수 있다 | SurfaceNavigation 전체, 필요하면 PIE |
| Bastion·Hollow 외곽 Block(높이 3.2~5m) | 변화 없음(올린 sweep이 먼저 막는다) | — |

### 3.4 완료 조건

- [x] 게임 타깃 컴파일, Editor 빌드
- [x] 픽스처 `LootNPop.SurfaceNavigation.EnemyMovement.SupportlessBlockerStep` 통과
- [x] 당시 `LootNPop.SurfaceNavigation` 전체 회귀137/137 Success. **비채택 분리력 제한 실험을 포함한 결과이며 벽 정지 해결의 근거가 아니다.** 실험 제거·근본 수정 후 다시 검증한다. 경고/상세는 `history/Phase16_Log.md`
- [x] Ridge WIP PIE(EnemyDensity1): 지상형 적의 바위 관통 유지 없음(일시 겹침 후 바로 빠져나옴), 벽 근처 정지 재현 안 됨, 군집 겹침 수용 가능. 원래 풀 복원(2026-11-16 네 번째 PIE)
- [x] 단위1 Nav 커버리지(D-084): 정식7·fixture5 재베이크, 최종 전체 회귀·Editor/Game 빌드(수치는 `history/Phase16_Log.md`)
- [x] 단위2 플레이어 물리 지면/Nav 투영 분리·보행 anchor(D-085): `WalkAnchorRecovery`·전체 138/138·Editor/Game 빌드·수정본 WIP PIE(§3.6)
- [ ] Phase 13 Production gate(`RunPhase13Gate.ps1 -Cases Production`, 새 패키지 쿡 필요) 수정 전후 비교: `GroundBlockerOverlap` 횟수·평균 시간, frame P95, Nav node 증가의 경로 비용. 단위2 뒤 한 번에 잰다
- [ ] 리슨2P(D-031). 단위2 뒤 한 번에 확인한다
- [x] 사용자 요청으로 작업 체크포인트 커밋(66dba1c, 비채택 실험 포함 상태)

### 3.5 비채택 실험 — 분리력 제한으로 Nav 이탈 방지

**2026-11-16 사용자 기각(D-083).** 군집의 겹침 해소를 약화시켜 NPC끼리 겹치는 문제가 심해져서는 안 된다. Nav 시작점을 유지하기 위해 분리력을 빼거나 약화·취소하는 방식을 벽 정지 해결책으로 채택하지 않는다. Support 없는 실제 Blocker를 막는 D-082와는 별개의 결정이다.

`StepGroundedWithSeparation`은 분리력 포함 결과가 300cm 안의 Nav 시작점을 잃으면 분리력을 빼고 재시도하는 실험이다. 합성 재현·캐시/정밀 검사·전체137/137·Editor/Game 빌드·헤드리스 이동7/7은 통과했으나, 실제 세 번째 PIE에서 벽 근처 NoNode 정지가 남았다. 사용자의 "줄어든 느낌"은1회 체감일 뿐 검증 결과가 아니다. `nav_separation_retries=196`은 판정 실행의 근거만 된다.

**2026-11-16 제거 완료.** 함수·호출·`nav_separation_retries` 계측·`SeparationNavBoundary` 검사를 지웠고 이동 프로세서 호출은 D-082 이전과 같다. D-082 구현·`SupportlessBlockerStep` 픽스처는 보존했다. 상세 근거는 [기각 원장 Case09](../../DiscardedApproaches.md#case-09-nav-이탈을-막기-위한-npc-분리력-제한)·`history/Phase16_Log.md`를 따른다.

### 3.6 다음 세션 — 벽 근처 Nav 커버리지와 근접 목표의 근본 수정

사용자는 **아래 두 조치 모두 필요**하다고 결정했다(D-083). 분리력 제한 실험을 정리한 뒤 두 조치를 각각 검증 가능한 구현 단위로 수행하고, 단위마다 다음 진행을 확인한다.

**단위1: 실제 보행 가능 영역과 Nav 누락의 대조·수정**

- Ridge WIP의 실제 벽 정지 지점을 사용한다. 경사각뿐 아니라 실제 NPC 캡슐의 서기·연속 이동, 높이 여유, Support/Layer 정체성을 확인해 "물리적으로 걸을 수 있는 바닥"을 정의한다.
- Support 누락/불확실성, normal·경사 판정, node 캡슐 overlap, edge step/slope·연속 sweep, 격자 누락, Layer 투영을 나눠 배제 원인을 확인한다. 현재 지각 목표 간격200cm·베이크 agent 반지름50/반높이88cm·투영 반경300cm와 실제 개체 설정을 대조한다.
- 실제로 걸을 수 있고 연결되는 정적 바닥이 과하게 빠졌다면 그 원인을 바로잡는다. 어떤 판정이 잘못됐는지 확인하기 전에 전역 해상도·agent 크기·허용 경사·투영 반경을 일괄 변경하지 않는다. 베이크 변경이 필요하면 WIP에서 먼저 확인하고 재베이크 영향 범위를 기록한다.
- 검증: 재현 벽 구역의 실보행/캡슐 경로와 Nav 커버리지가 일치하고, 여러 지상 근접 NPC가 벽 근처로 몰린 뒤 플레이어를 따라 다시 나올 수 있어야 한다.
- **결과(2026-11-16 완료, D-084).** 두 PIE의 NoNode NPC를 옥탄트 local로 환산하면 WIP의 같은 지형이었다. `LNP.SurfaceNav.ExplainNavGap` 진단에서 반경400cm 후보 탈락의 대부분이 Atlas `NeedsExact` 꼭짓점이었고, 그중 다수는 평탄하고 NPC 캡슐이 서는 바닥이었다. node 위치를 Nav 전용 보간(`QueryNavLayer`)으로 만들도록 바꾸고 베이커 schema를10으로 올렸다. WIP 재베이크 후 다섯 정지 지점의 최근접 node가 110~280cm(발 기준)로 투영 반경 안에 들어왔고 주 정적 component에 연결됐다. 실제 WIP PIE에서 사용자가 NPC를 벽에 붙여 끌고 다녀도 정지가 재현되지 않았고, 군집 겹침은 수용 가능, 바위 관통 유지는 없었다. 남은 탈락 사유 `ClearanceFloorOnly`(node 캡슐이 지형 바닥에만 닿음, 지점당1~3개)는 다루지 않았다. 수치·회귀는 `history/Phase16_Log.md`.

**단위2: 물리적 바닥과 목표 Nav 투영 분리·근접 접근/공격 위치**

- `ULNPPlayerNavProcessor`는 실제 Floor identity가 있어도 가까운 Nav node가 없으면 `Surface`를 무효로 남기고 `GroundPoint`를 갱신하지 않는다. 물리적 접지/현재 지면 정체성과 Nav node·도달성 결과를 분리한다.
- 근접 목표는 플레이어 발밑 node 하나에만 의존하지 않는다. 같은 보행 연결 영역에서 걸어서 접근해 실제 사거리로 공격할 수 있는 위치를 사용하고, 마지막 접근의 캡슐 이동·필요한 공격 경로를 검증한다. 목표 투영 실패를 타겟 상실이나 영구 대기로 잘못 전파하지 않는다.
- 도달 불가능한 섬/다른 층·실제 막힌 통로는 계속 구분한다. 벽 너머 스냅·관통·순간 이동으로 연결을 만들어서는 안 된다. 이미 Nav 밖에 놓인 NPC의 복구도 실제 보행 연결이 검증되는 범위에서 검토한다.
- 검증: 플레이어가 벽에 붙어 있어도 공간상 접근·공격 가능한 근접 NPC는 추격·공격하며, 실제 도달 불가 사례는 기존 규칙을 지킨다.
- **결과(2026-11-16 완료, D-085).** 새 데이터의 재현에서 벽 옆 플레이어를 공격하지 못하는 경우는 없었다. 드물게(여러 번에 1회) 근접 적이 플레이어를 따라 벽 밑 경사 띠에 올라서 멈췄다. 띠는 걸을 수 있지만 칸 꼭짓점 하나가 벽면에 걸려 Nav가 없고, 최근접 node가 308cm라 시작점 NoNode → 슬롯 반납 → Alert 정지였다. 투영 반경 밖 위치를 **보행 anchor**로 잇도록 고쳤다: 600cm 안 node까지 실제 접지 이동(`StepGrounded`)으로 걸어서 이어지는지 검증해 적의 시작점·도달성·슬롯·경로 요청과 Nav 밖 플레이어의 목표에 쓰고, Nav 밖 적의 직접 접근은 현재 위치→MoveTarget 발 이동이 검증될 때만 허용한다. 플레이어 `Surface`·`GroundPoint`는 물리 바닥으로 분리했다. 고정 회귀 `EnemyMovement.WalkAnchorRecovery`(스텁 상태 실패 확인)·전체 138/138·Editor/Game 빌드 통과, 수정본 WIP PIE에서 정지·겹침 악화·바위 관통 유지 모두 재현되지 않았다. 원래 드문 현상이라 PIE는 재발 없음의 표본이다. 상세는 `history/Phase16_Log.md`.

**두 단위의 공통 수용 기준**

- 군집 분리력/겹침 해소를 약화시키지 않는다. 같은 군집 조건에서 NPC끼리의 겹침 빈도·정도·지속이 기존 기준보다 악화되지 않아야 한다. 경계에서 분리력을 빼는 방식으로 이 조건을 우회하지 않는다.
- 실제 Ridge WIP에서 벽 옆 플레이어·군집 추격·열린 곳으로 복귀·기존 바위 비관통을 수동 확인한다. 가능한 수동 재현을 새 자동화 장치보다 먼저 요청한다. 고정 회귀는 실제 실패 원인을 재현하도록 작성한다.
- 최종 코드의 Editor/Game 빌드·전체 SurfaceNavigation·리슨2P·Production 비용 비교를 확인한다. 과거 비채택 실험의137/137·7/7 결과를 최종 구현의 검증으로 재사용하지 않는다.
- 실제 WIP slot은 새 PIE 로그에서 다시 읽고, 검사용 풀은 메모리에서만 선택한다. 종료 뒤 원래 풀과 보호 파일 해시를 확인한다. 원거리 사선 재배치와 ISM Support는 이 근본 수정 뒤 별도 단위로 남긴다.

### 3.7 1단계 마감 인계(단위2 뒤)

단위1(D-084)·단위2(D-085)가 끝났다. 1단계에 남은 것은 다음 두 가지다.

- **Production gate:** 새 패키지 쿡(쿡 전 사용자 허가) 뒤 `Scripts/Profiling/RunPhase13Gate.ps1 -Cases Production`(헤드리스, seed 1). 기준선은 V011(`phases/Phase09_PerfMemoryV011.md`)이다. 볼 것: `GroundBlockerOverlap` 횟수·평균 시간(D-082), frame P95, D-084 Nav node 증가의 경로 비용, 보행 anchor 검증 비용(`LNPEnemyPath_WalkAnchor`·`LNPEnemyPath_FinalApproach` trace scope, 검증 걸음은 `EnemyGround` steps에 포함). harness의 `exact ≤2ms` FAIL은 Gate가 아니다.
- **리슨2P(D-031):** `-game` 리슨 서버 + 게스트 스모크. 경로·슬롯은 서버 전용이므로 게스트 쪽 적 표현·복제 이상 유무를 본다.

진단 메모: 정지 위치는 옥탄트 local로 바꿔 `LNP.SurfaceNav.ExplainNavGap`으로 본다. slot 회전은 PIE 로그에서 읽는다(예: `R(P=180,Y=270)`이면 local=(Wy,Wx,−Wz), `R(P=180,Y=90)`이면 (−Wy,−Wx,−Wz), `R(P=180)`이면 (−Wx,Wy,−Wz)). commandlet에서는 `load_map` 뒤 실행한다. `NavReport`의 `walkAnchored`(플레이어)·`walkAnchor`(적)는 보행 anchor 사용 여부다.

새 세션 시작 프롬프트:

> SurfaceSupportNavigation Phase16 1단계를 마감하자. README.md·Current.md·Decisions.md와 `phases/Phase16_SupportlessBlockerStep.md` §3.4·3.7, `history/Phase16_Log.md`의 단위2 기록을 읽어줘. 단위1(D-084 Nav 전용 보간)·단위2(D-085 보행 anchor)는 끝났고 전체 SurfaceNavigation 138/138이야. 남은 건 새 패키지 쿡 뒤 Production gate 비용 비교와 리슨2P 스모크야. 쿡 전에는 나한테 물어봐줘. 결과를 보고 1단계 마감 여부를 정하자.

## 4. 2단계 — ISM Support(D-058 해제) 조사·계획

1단계 마감 뒤 조사한다. 구현 착수는 사용자가 계획을 보고 정한다.

조사 항목:

- 인스턴스별 Support 소스 추출: 베이커 수집기가 ISM instance를 개별 source로 다루는 방법, 지각 위 작은 Layer의 해상도(D-072)
- 층 대응(D-037): exact hit의 `InstanceIndex` → Layer 해석 표, face map과의 관계
- 런타임 source binding 키: component 경로 + instance index의 안정성(PCG 재생성·RemoveSwap 순서)
- payload·베이크 시간: 바위 수에 비례하는 Layer·portal 증가량(1-B6 전체 밀도 기준 추정)
- Phase 15 경로 측정 조건 변화: Nav component·portal 증가가 A* 비교에 미치는 영향
- 역할 재판정: Sandstone은 Support+Blocker, Moss는 Blocker 유지가 맞는지
